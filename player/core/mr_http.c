/*
 * MintVID - HTTP implementation wrapper for a process-local YouTube session.
 *
 * The HTTP engine itself lives unchanged in mr_http_impl.c.  This small layer
 * observes only final HTTP request strings and decrypted response bytes.  It
 * keeps anonymous youtube.com Set-Cookie values in RAM and adds them to later
 * youtube.com requests, giving WEB_SAFARI the same session continuity a browser
 * has.  Cookies are never sent to googlevideo.com (or any non-youtube host),
 * never written to disk and never logged.
 */
#include "mr_http.h"
#include "mr_alloc.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if (defined(AMIGA_M68K) && !defined(MR_HOST_BUILD)) || \
    defined(__amigaos__) || defined(__AMIGA__)
#define MR_YT_SESSION_AMIGA 1
#include <exec/types.h>
#include <exec/libraries.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <proto/exec.h>
#include <proto/bsdsocket.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#else
#define MR_YT_SESSION_AMIGA 0
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>
#endif

#if MR_YT_SESSION_AMIGA && defined(HAVE_AMISSL)
#define MR_YT_SESSION_TLS 1
#include <proto/amisslmaster.h>
#include <proto/amissl.h>
#include <libraries/amisslmaster.h>
#include <libraries/amissl.h>
#include <amissl/amissl.h>
#elif !MR_YT_SESSION_AMIGA && defined(MR_HTTP_HAVE_OPENSSL)
#define MR_YT_SESSION_TLS 1
#include <openssl/ssl.h>
#else
#define MR_YT_SESSION_TLS 0
#endif

#define MR_YT_COOKIE_SLOTS       12
#define MR_YT_COOKIE_NAME_MAX    64
#define MR_YT_COOKIE_VALUE_MAX   384
#define MR_YT_COOKIE_HEADER_MAX  2048
#define MR_YT_RESPONSE_HEADER_MAX 16384

typedef struct mr_yt_cookie {
    char name[MR_YT_COOKIE_NAME_MAX];
    char value[MR_YT_COOKIE_VALUE_MAX];
} mr_yt_cookie;

static mr_yt_cookie g_mr_yt_cookies[MR_YT_COOKIE_SLOTS];
static char g_mr_yt_cookie_header[MR_YT_COOKIE_HEADER_MAX];
static unsigned char g_mr_yt_response_header[MR_YT_RESPONSE_HEADER_MAX];
static size_t g_mr_yt_response_header_len;
static int g_mr_yt_current_request;
static int g_mr_yt_response_header_done;
static int g_mr_yt_logged_capture;
static int g_mr_yt_logged_reuse;

static int mr_yt_ascii_tolower(int c)
{
    return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
}

static int mr_yt_ascii_equal_n(const char *a, const char *b, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++) {
        if (mr_yt_ascii_tolower((unsigned char)a[i]) !=
            mr_yt_ascii_tolower((unsigned char)b[i]))
            return 0;
    }
    return 1;
}

static int mr_yt_host_is_youtube(const char *host, size_t len)
{
    static const char suffix[] = "youtube.com";
    const size_t suffix_len = sizeof suffix - 1;

    while (len && (*host == ' ' || *host == '\t')) {
        host++;
        len--;
    }
    while (len && (host[len - 1] == ' ' || host[len - 1] == '\t'))
        len--;

    /* Host headers here are DNS names, not IPv6 literals. Strip :port. */
    {
        size_t i;
        for (i = 0; i < len; i++) {
            if (host[i] == ':') {
                len = i;
                break;
            }
        }
    }

    if (len < suffix_len ||
        !mr_yt_ascii_equal_n(host + len - suffix_len, suffix, suffix_len))
        return 0;
    return len == suffix_len || host[len - suffix_len - 1] == '.';
}

static void mr_yt_rebuild_cookie_header(void)
{
    size_t used = 0;
    int i;

    g_mr_yt_cookie_header[0] = '\0';
    for (i = 0; i < MR_YT_COOKIE_SLOTS; i++) {
        const mr_yt_cookie *cookie = &g_mr_yt_cookies[i];
        size_t name_len, value_len, need;
        if (!cookie->name[0]) continue;
        name_len = strlen(cookie->name);
        value_len = strlen(cookie->value);
        need = name_len + 1 + value_len + (used ? 2 : 0);
        if (used + need >= sizeof g_mr_yt_cookie_header) continue;
        if (used) {
            g_mr_yt_cookie_header[used++] = ';';
            g_mr_yt_cookie_header[used++] = ' ';
        }
        memcpy(g_mr_yt_cookie_header + used, cookie->name, name_len);
        used += name_len;
        g_mr_yt_cookie_header[used++] = '=';
        memcpy(g_mr_yt_cookie_header + used, cookie->value, value_len);
        used += value_len;
        g_mr_yt_cookie_header[used] = '\0';
    }
}

static int mr_yt_cookie_component_valid(const char *text, size_t len,
                                        int is_name)
{
    size_t i;
    if (!len) return 0;
    for (i = 0; i < len; i++) {
        unsigned char c = (unsigned char)text[i];
        if (c < 0x21 || c > 0x7e || c == ';' || c == '\\' ||
            (is_name && c == '='))
            return 0;
    }
    return 1;
}

static int mr_yt_store_cookie(const char *name, size_t name_len,
                              const char *value, size_t value_len)
{
    int i, free_slot = -1;

    if (name_len >= MR_YT_COOKIE_NAME_MAX ||
        value_len >= MR_YT_COOKIE_VALUE_MAX ||
        !mr_yt_cookie_component_valid(name, name_len, 1) ||
        (value_len && !mr_yt_cookie_component_valid(value, value_len, 0)))
        return 0;

    for (i = 0; i < MR_YT_COOKIE_SLOTS; i++) {
        if (!g_mr_yt_cookies[i].name[0]) {
            if (free_slot < 0) free_slot = i;
            continue;
        }
        if (strlen(g_mr_yt_cookies[i].name) == name_len &&
            !memcmp(g_mr_yt_cookies[i].name, name, name_len))
            break;
    }

    if (i == MR_YT_COOKIE_SLOTS) {
        if (free_slot < 0) return 0;
        i = free_slot;
    }

    if (!value_len) {
        if (!g_mr_yt_cookies[i].name[0]) return 0;
        g_mr_yt_cookies[i].name[0] = '\0';
        g_mr_yt_cookies[i].value[0] = '\0';
        mr_yt_rebuild_cookie_header();
        return 1;
    }

    if (g_mr_yt_cookies[i].name[0] &&
        strlen(g_mr_yt_cookies[i].value) == value_len &&
        !memcmp(g_mr_yt_cookies[i].value, value, value_len))
        return 0;

    memcpy(g_mr_yt_cookies[i].name, name, name_len);
    g_mr_yt_cookies[i].name[name_len] = '\0';
    memcpy(g_mr_yt_cookies[i].value, value, value_len);
    g_mr_yt_cookies[i].value[value_len] = '\0';
    mr_yt_rebuild_cookie_header();
    return 1;
}

static void mr_yt_parse_set_cookies(const unsigned char *headers,
                                    size_t header_len)
{
    size_t pos = 0;
    int changed = 0;

    while (pos < header_len) {
        size_t end = pos;
        const char *line;
        size_t line_len;
        while (end + 1 < header_len &&
               !(headers[end] == '\r' && headers[end + 1] == '\n'))
            end++;
        if (end + 1 >= header_len) break;
        line = (const char *)headers + pos;
        line_len = end - pos;

        if (line_len >= 11 && mr_yt_ascii_equal_n(line, "Set-Cookie:", 11)) {
            const char *p = line + 11;
            const char *line_end = line + line_len;
            const char *pair_end;
            const char *eq;
            while (p < line_end && (*p == ' ' || *p == '\t')) p++;
            pair_end = p;
            while (pair_end < line_end && *pair_end != ';') pair_end++;
            while (pair_end > p &&
                   (pair_end[-1] == ' ' || pair_end[-1] == '\t'))
                pair_end--;
            eq = (const char *)memchr(p, '=', (size_t)(pair_end - p));
            if (eq && eq > p) {
                const char *name_end = eq;
                const char *value = eq + 1;
                const char *value_end = pair_end;
                while (name_end > p &&
                       (name_end[-1] == ' ' || name_end[-1] == '\t'))
                    name_end--;
                while (value < value_end && (*value == ' ' || *value == '\t'))
                    value++;
                if (mr_yt_store_cookie(p, (size_t)(name_end - p),
                                       value, (size_t)(value_end - value)))
                    changed = 1;
            }
        }
        pos = end + 2;
    }

    if (changed && !g_mr_yt_logged_capture) {
        g_mr_yt_logged_capture = 1;
        printf("YouTube HTTP session: captured anonymous cookies\n");
    }
}

static void mr_yt_capture_response_bytes(const void *buffer, size_t len)
{
    const unsigned char *src = (const unsigned char *)buffer;
    size_t old_len, copy_len, scan;

    if (!g_mr_yt_current_request || g_mr_yt_response_header_done || !len)
        return;

    old_len = g_mr_yt_response_header_len;
    copy_len = sizeof g_mr_yt_response_header - old_len;
    if (copy_len > len) copy_len = len;
    if (copy_len) {
        memcpy(g_mr_yt_response_header + old_len, src, copy_len);
        g_mr_yt_response_header_len += copy_len;
    }

    scan = old_len > 3 ? old_len - 3 : 0;
    while (scan + 3 < g_mr_yt_response_header_len) {
        if (g_mr_yt_response_header[scan] == '\r' &&
            g_mr_yt_response_header[scan + 1] == '\n' &&
            g_mr_yt_response_header[scan + 2] == '\r' &&
            g_mr_yt_response_header[scan + 3] == '\n') {
            g_mr_yt_response_header_done = 1;
            mr_yt_parse_set_cookies(g_mr_yt_response_header, scan + 4);
            return;
        }
        scan++;
    }

    if (g_mr_yt_response_header_len == sizeof g_mr_yt_response_header)
        g_mr_yt_response_header_done = 1;
}

static int mr_yt_request_host(const char *request,
                              const char **host_out, size_t *host_len_out)
{
    const char *host, *end;
    if (strncmp(request, "GET ", 4) && strncmp(request, "POST ", 5))
        return 0;
    host = strstr(request, "\r\nHost:");
    if (!host) return 0;
    host += 7;
    while (*host == ' ' || *host == '\t') host++;
    end = strstr(host, "\r\n");
    if (!end || end == host) return 0;
    *host_out = host;
    *host_len_out = (size_t)(end - host);
    return 1;
}

static int mr_yt_prepare_request(char *request, size_t cap, int length)
{
    const char *host;
    size_t host_len;
    int youtube = 0;

    if (length <= 0 || (size_t)length >= cap ||
        !mr_yt_request_host(request, &host, &host_len))
        return length;

    youtube = mr_yt_host_is_youtube(host, host_len);
    g_mr_yt_current_request = youtube;
    g_mr_yt_response_header_len = 0;
    g_mr_yt_response_header_done = youtube ? 0 : 1;

    if (youtube && g_mr_yt_cookie_header[0] &&
        !strstr(request, "\r\nCookie:")) {
        char *header_end = strstr(request, "\r\n\r\n");
        size_t cookie_len = strlen(g_mr_yt_cookie_header);
        size_t add = 8 + cookie_len + 2; /* "Cookie: " + value + CRLF */
        if (header_end && (size_t)length + add < cap) {
            size_t insert = (size_t)(header_end - request) + 2;
            memmove(request + insert + add, request + insert,
                    (size_t)length - insert + 1);
            memcpy(request + insert, "Cookie: ", 8);
            memcpy(request + insert + 8, g_mr_yt_cookie_header, cookie_len);
            request[insert + 8 + cookie_len] = '\r';
            request[insert + 8 + cookie_len + 1] = '\n';
            length += (int)add;
            if (!g_mr_yt_logged_reuse) {
                g_mr_yt_logged_reuse = 1;
                printf("YouTube HTTP session: reusing anonymous cookies\n");
            }
        }
    }
    return length;
}

static int mr_yt_session_snprintf(char *out, size_t cap, const char *fmt, ...)
{
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = vsnprintf(out, cap, fmt, ap);
    va_end(ap);
    return mr_yt_prepare_request(out, cap, n);
}

#if MR_YT_SESSION_AMIGA
static int mr_yt_session_recv(int sock, char *buf, int len, int flags)
{
    int n = (int)recv(sock, buf, len, flags);
    if (n > 0) mr_yt_capture_response_bytes(buf, (size_t)n);
    return n;
}
#else
static int mr_yt_session_recv(int sock, void *buf, size_t len, int flags)
{
    int n = (int)recv(sock, buf, len, flags);
    if (n > 0) mr_yt_capture_response_bytes(buf, (size_t)n);
    return n;
}
#endif

#if MR_YT_SESSION_TLS
static int mr_yt_session_ssl_read(SSL *ssl, void *buf, int len)
{
    int n = SSL_read(ssl, buf, len);
    if (n > 0) mr_yt_capture_response_bytes(buf, (size_t)n);
    return n;
}
#endif

/* The real implementation includes the same system headers again, but their
 * include guards keep these replacement macros out of the declarations. */
#ifdef snprintf
#undef snprintf
#endif
#define snprintf mr_yt_session_snprintf

#ifdef recv
#undef recv
#endif
#define recv mr_yt_session_recv

#if MR_YT_SESSION_TLS
#ifdef SSL_read
#undef SSL_read
#endif
#define SSL_read mr_yt_session_ssl_read
#endif

#include "mr_http_impl.c"

#undef snprintf
#undef recv
#if MR_YT_SESSION_TLS
#undef SSL_read
#endif
