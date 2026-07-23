/*
 * MintRIVA - streaming HTTP/HTTPS random-access media source.
 *
 * One response remains open while a demuxer reads sequentially. A seek closes
 * it and starts a Range request at the new byte offset. This keeps TS playback
 * to a handful of connections while still allowing AVI/MOV metadata seeks.
 */
#include "mr_http.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(AMIGA_M68K) || defined(__amigaos__) || defined(__AMIGA__)
#define MR_HTTP_AMIGA 1
#include <exec/types.h>
#include <exec/libraries.h>
#include <proto/exec.h>
#include <proto/bsdsocket.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#else
#define MR_HTTP_AMIGA 0
#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#endif

#if MR_HTTP_AMIGA && defined(HAVE_AMISSL)
#define MR_HTTP_HAVE_TLS 1
#include <proto/amisslmaster.h>
#include <proto/amissl.h>
#include <libraries/amisslmaster.h>
#include <libraries/amissl.h>
#include <amissl/amissl.h>
#elif !MR_HTTP_AMIGA && defined(MR_HTTP_HAVE_OPENSSL)
#define MR_HTTP_HAVE_TLS 1
#include <openssl/ssl.h>
#include <openssl/err.h>
#else
#define MR_HTTP_HAVE_TLS 0
#endif

#define HTTP_URL_MAX       1024
#define HTTP_HOST_MAX       256
#define HTTP_PATH_MAX       768
#define HTTP_HEADER_MAX   16384
#define HTTP_REQUEST_MAX   1536
#define HTTP_CHUNK_LINE_MAX 128
#ifndef HTTP_CACHE_SIZE
#define HTTP_CACHE_SIZE  (256UL * 1024)
#endif
#define HTTP_REDIRECT_MAX     5
#define HTTP_IO_RETRIES        2

#if MR_HTTP_AMIGA
struct Library *SocketBase = NULL;
#if defined(HAVE_AMISSL)
struct Library *AmiSSLMasterBase = NULL;
struct Library *AmiSSLBase = NULL;
struct Library *AmiSSLExtBase = NULL;
#endif
#endif

typedef struct {
    char host[HTTP_HOST_MAX];
    char path[HTTP_PATH_MAX];
    unsigned short port;
    int tls;
} http_url;

typedef struct {
    char url[HTTP_URL_MAX];
    int sock;
    int socket_ready;
    int using_tls;
    int platform_ready;
    int tls_ready;
    int tls_quarantined;
#if MR_HTTP_HAVE_TLS
    SSL_CTX *ssl_ctx;
    SSL     *ssl;
#endif
    size_t total_len;
    size_t body_pos;
    size_t response_left;
    int response_left_known;
    int streaming;              /* forward-only: no length, no range seeking  */
    int chunked;
    size_t chunk_left;
    int chunk_need_crlf;
    int chunk_done;
    unsigned char header[HTTP_HEADER_MAX];
    size_t prefetch_pos;
    size_t prefetch_len;
    unsigned char *cache;
    size_t cache_start;
    size_t cache_len;
} http_source;

static int ascii_tolower(int c)
{
    return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
}

static int ascii_ncasecmp(const char *a, const char *b, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++) {
        int ca = ascii_tolower((unsigned char)a[i]);
        int cb = ascii_tolower((unsigned char)b[i]);
        if (ca != cb || !ca || !cb) return ca - cb;
    }
    return 0;
}

static int starts_nocase(const char *s, const char *prefix)
{
    return ascii_ncasecmp(s, prefix, strlen(prefix)) == 0;
}

static void source_error_status(const char *prefix, int status)
{
    char text[192];
    snprintf(text, sizeof text, "%s (HTTP status %d)", prefix, status);
    mr_source_set_error(text);
}

static int parse_decimal(const char *p, const char **end, size_t *value)
{
    size_t v = 0;
    int have = 0;
    while (*p >= '0' && *p <= '9') {
        unsigned digit = (unsigned)(*p - '0');
        if (v > (SIZE_MAX - digit) / 10u) return 0;
        v = v * 10u + digit;
        p++;
        have = 1;
    }
    if (!have) return 0;
    if (end) *end = p;
    *value = v;
    return 1;
}

static int parse_port(const char *p, const char *end, unsigned short *port)
{
    unsigned long v = 0;
    if (p == end) return 0;
    while (p < end) {
        if (*p < '0' || *p > '9') return 0;
        v = v * 10u + (unsigned)(*p++ - '0');
        if (v > 65535u) return 0;
    }
    if (!v) return 0;
    *port = (unsigned short)v;
    return 1;
}

static int parse_url(const char *url, http_url *out)
{
    const char *authority, *slash, *host_end, *colon = NULL, *p;
    size_t n;
    memset(out, 0, sizeof *out);
    if (starts_nocase(url, "http://")) {
        out->tls = 0;
        out->port = 80;
        authority = url + 7;
    } else if (starts_nocase(url, "https://")) {
        out->tls = 1;
        out->port = 443;
        authority = url + 8;
    } else {
        return 0;
    }
    slash = strchr(authority, '/');
    host_end = slash ? slash : authority + strlen(authority);
    if (authority == host_end || memchr(authority, '@',
                                         (size_t)(host_end - authority)))
        return 0;
    /* IPv6 literals are deliberately deferred; Amiga TCP/IP stacks in the
     * target range are predominantly IPv4 and gethostbyname-based. */
    if (*authority == '[') return 0;
    for (p = authority; p < host_end; p++)
        if (*p == ':') colon = p;
    if (colon) {
        if (!parse_port(colon + 1, host_end, &out->port)) return 0;
        host_end = colon;
    }
    n = (size_t)(host_end - authority);
    if (!n || n >= sizeof out->host) return 0;
    memcpy(out->host, authority, n);
    out->host[n] = '\0';
    if (slash) {
        n = strlen(slash);
        if (n >= sizeof out->path) return 0;
        memcpy(out->path, slash, n + 1);
    } else {
        strcpy(out->path, "/");
    }
    return 1;
}

static int resolve_redirect(const char *base_url, const char *location,
                            char *out, size_t out_size)
{
    http_url base;
    const char *scheme;
    int n;
    if (starts_nocase(location, "http://") ||
        starts_nocase(location, "https://")) {
        if (strlen(location) >= out_size) return 0;
        strcpy(out, location);
        return 1;
    }
    if (!parse_url(base_url, &base)) return 0;
    scheme = base.tls ? "https" : "http";
    if (location[0] == '/' && location[1] == '/') {
        n = snprintf(out, out_size, "%s:%s", scheme, location);
    } else if (location[0] == '/') {
        int default_port = (!base.tls && base.port == 80) ||
                           (base.tls && base.port == 443);
        n = default_port
          ? snprintf(out, out_size, "%s://%s%s",
                     scheme, base.host, location)
          : snprintf(out, out_size, "%s://%s:%u%s",
                     scheme, base.host, (unsigned)base.port, location);
    } else {
        char directory[HTTP_PATH_MAX];
        char *last;
        int default_port;
        size_t path_len = strlen(base.path);
        if (path_len >= sizeof directory) return 0;
        memcpy(directory, base.path, path_len + 1);
        last = strrchr(directory, '/');
        if (!last) strcpy(directory, "/");
        else last[1] = '\0';
        default_port = (!base.tls && base.port == 80) ||
                       (base.tls && base.port == 443);
        n = default_port
          ? snprintf(out, out_size, "%s://%s%s%s",
                     scheme, base.host, directory, location)
          : snprintf(out, out_size, "%s://%s:%u%s%s",
                     scheme, base.host, (unsigned)base.port,
                     directory, location);
    }
    return n > 0 && (size_t)n < out_size;
}

static int platform_open(http_source *h)
{
#if MR_HTTP_AMIGA
    if (!SocketBase)
        SocketBase = OpenLibrary("bsdsocket.library", 4);
    if (!SocketBase) {
        mr_source_set_error("bsdsocket.library v4 is required for HTTP");
        return 0;
    }
#else
    (void)h;
#endif
    h->platform_ready = 1;
    return 1;
}

#if MR_HTTP_HAVE_TLS
static int tls_open(http_source *h)
{
    const SSL_METHOD *method;
    if (h->tls_ready) return 1;
#if MR_HTTP_AMIGA
    AmiSSLMasterBase =
        OpenLibrary("amisslmaster.library", AMISSLMASTER_MIN_VERSION);
    if (!AmiSSLMasterBase) {
        mr_source_set_error("AmiSSL v5 is required for HTTPS");
        return 0;
    }
    if (OpenAmiSSLTags(AMISSL_CURRENT_VERSION,
                       AmiSSL_UsesOpenSSLStructs, TRUE,
                       AmiSSL_GetAmiSSLBase, (ULONG)&AmiSSLBase,
                       AmiSSL_GetAmiSSLExtBase, (ULONG)&AmiSSLExtBase,
                       AmiSSL_SocketBase, (ULONG)SocketBase,
                       AmiSSL_ErrNoPtr, (ULONG)&errno,
                       TAG_DONE) != 0) {
        mr_source_set_error("cannot initialise AmiSSL");
        CloseLibrary(AmiSSLMasterBase);
        AmiSSLMasterBase = NULL;
        return 0;
    }
    if (InitAmiSSL(AmiSSL_SocketBase, (ULONG)SocketBase,
                   AmiSSL_ErrNoPtr, (ULONG)&errno,
                   TAG_DONE) != 0) {
        mr_source_set_error("cannot initialise AmiSSL");
        CloseAmiSSL();
        AmiSSLBase = NULL;
        AmiSSLExtBase = NULL;
        CloseLibrary(AmiSSLMasterBase);
        AmiSSLMasterBase = NULL;
        return 0;
    }
#else
    SSL_library_init();
    SSL_load_error_strings();
#endif
    h->tls_ready = 1;
    method = SSLv23_client_method();
    if (!method || !(h->ssl_ctx = SSL_CTX_new(method))) {
        mr_source_set_error("cannot create TLS context");
        return 0;
    }
#ifdef MR_HTTP_SSL_VERIFY_PEER
    SSL_CTX_set_verify(h->ssl_ctx, SSL_VERIFY_PEER, NULL);
    if (SSL_CTX_set_default_verify_paths(h->ssl_ctx) != 1) {
        mr_source_set_error("cannot load TLS root certificates");
        return 0;
    }
#else
    SSL_CTX_set_verify(h->ssl_ctx, SSL_VERIFY_NONE, NULL);
#endif
#ifdef SSL_OP_IGNORE_UNEXPECTED_EOF
    SSL_CTX_set_options(h->ssl_ctx, SSL_OP_IGNORE_UNEXPECTED_EOF);
#endif
    return 1;
}
#endif

static void close_socket_only(http_source *h)
{
    if (!h->socket_ready) return;
#if MR_HTTP_AMIGA
    CloseSocket(h->sock);
#else
    close(h->sock);
#endif
    h->sock = -1;
    h->socket_ready = 0;
}

static void close_connection(http_source *h, int healthy)
{
#if MR_HTTP_HAVE_TLS
    if (h->ssl) {
#if MR_HTTP_AMIGA
        if (!healthy) {
            /* MintAMP's real-hardware soak tests found SSL_free unsafe after
             * some peer-drop/SYSCALL paths. Quarantine that one object and
             * let process exit reclaim it instead of risking a hard lock. */
            h->ssl = NULL;
            h->tls_quarantined = 1;
        } else
#endif
        {
            BIO *rbio, *wbio;
            SSL_set_shutdown(h->ssl, SSL_SENT_SHUTDOWN);
            rbio = SSL_get_rbio(h->ssl);
            wbio = SSL_get_wbio(h->ssl);
            if (rbio) BIO_set_close(rbio, BIO_NOCLOSE);
            if (wbio && wbio != rbio) BIO_set_close(wbio, BIO_NOCLOSE);
            SSL_free(h->ssl);
            h->ssl = NULL;
        }
    }
#else
    (void)healthy;
#endif
    close_socket_only(h);
    h->using_tls = 0;
    h->response_left_known = 0;
    h->chunked = 0;
    h->chunk_left = 0;
    h->chunk_need_crlf = 0;
    h->chunk_done = 0;
    h->prefetch_pos = h->prefetch_len = 0;
}

static int connect_socket(http_source *h, const http_url *url)
{
    struct hostent *he;
    struct sockaddr_in sa;
    struct timeval timeout;
    he = gethostbyname(url->host);
    if (!he || !he->h_addr_list || !he->h_addr_list[0]) {
        mr_source_set_error("HTTP DNS lookup failed");
        return 0;
    }
    h->sock = (int)socket(AF_INET, SOCK_STREAM, 0);
    if (h->sock < 0) {
        mr_source_set_error("cannot create HTTP socket");
        return 0;
    }
    h->socket_ready = 1;
    timeout.tv_sec = 20;
    timeout.tv_usec = 0;
    setsockopt(h->sock, SOL_SOCKET, SO_RCVTIMEO,
               (const char *)&timeout, sizeof timeout);
    setsockopt(h->sock, SOL_SOCKET, SO_SNDTIMEO,
               (const char *)&timeout, sizeof timeout);
    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_port = htons(url->port);
    memcpy(&sa.sin_addr, he->h_addr_list[0], (size_t)he->h_length);
    if (connect(h->sock, (struct sockaddr *)&sa, sizeof sa) != 0) {
        mr_source_set_error("HTTP connection failed");
        close_socket_only(h);
        return 0;
    }
    if (url->tls) {
#if MR_HTTP_HAVE_TLS
#if MR_HTTP_AMIGA
        if (h->tls_quarantined) {
            mr_source_set_error(
                "HTTPS connection failed; restart player before retrying");
            close_socket_only(h);
            return 0;
        }
#endif
        if (!tls_open(h)) {
            close_socket_only(h);
            return 0;
        }
        h->ssl = SSL_new(h->ssl_ctx);
        if (!h->ssl) {
            mr_source_set_error("cannot create TLS session");
            close_socket_only(h);
            return 0;
        }
#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
        SSL_set_tlsext_host_name(h->ssl, url->host);
#endif
#ifdef MR_HTTP_SSL_VERIFY_PEER
        {
            X509_VERIFY_PARAM *param = SSL_get0_param(h->ssl);
            if (param) X509_VERIFY_PARAM_set1_host(param, url->host, 0);
        }
#endif
        if (SSL_set_fd(h->ssl, h->sock) != 1 ||
            SSL_connect(h->ssl) != 1) {
            mr_source_set_error("HTTPS TLS handshake failed");
            close_connection(h, 0);
            return 0;
        }
        h->using_tls = 1;
#else
        mr_source_set_error(
            "HTTPS support was not compiled in; rebuild with SSL=1");
        close_socket_only(h);
        return 0;
#endif
    }
    return 1;
}

static int net_write_all(http_source *h, const void *buf, size_t len)
{
    const unsigned char *p = (const unsigned char *)buf;
    while (len) {
        int n;
#if MR_HTTP_HAVE_TLS
        if (h->using_tls)
            n = SSL_write(h->ssl, p, len > 0x7fffffffUL
                                      ? 0x7fffffff : (int)len);
        else
#endif
            n = (int)send(h->sock, (const char *)p,
                          len > 0x7fffffffUL ? 0x7fffffff : (int)len, 0);
        if (n <= 0) return 0;
        p += n;
        len -= (size_t)n;
    }
    return 1;
}

static int net_read_some(http_source *h, void *buf, size_t len)
{
    int n;
    int amount = len > 0x7fffffffUL ? 0x7fffffff : (int)len;
#if MR_HTTP_HAVE_TLS
    if (h->using_tls)
        n = SSL_read(h->ssl, buf, amount);
    else
#endif
        n = (int)recv(h->sock, (char *)buf, amount, 0);
    return n;
}

static int raw_read_some(http_source *h, void *buf, size_t len)
{
    if (h->prefetch_pos < h->prefetch_len) {
        size_t n = h->prefetch_len - h->prefetch_pos;
        if (n > len) n = len;
        memcpy(buf, h->header + h->prefetch_pos, n);
        h->prefetch_pos += n;
        return (int)n;
    }
    return net_read_some(h, buf, len);
}

static int raw_read_exact(http_source *h, void *buf, size_t len)
{
    unsigned char *out = (unsigned char *)buf;
    size_t done = 0;
    while (done < len) {
        int n = raw_read_some(h, out + done, len - done);
        if (n <= 0) return 0;
        done += (size_t)n;
    }
    return 1;
}

static int raw_read_line(http_source *h, char *line, size_t line_size)
{
    size_t used = 0;
    if (!line_size) return 0;
    while (used + 1 < line_size) {
        unsigned char c;
        if (!raw_read_exact(h, &c, 1)) return 0;
        if (c == '\n') {
            if (used && line[used - 1] == '\r') used--;
            line[used] = '\0';
            return 1;
        }
        line[used++] = (char)c;
    }
    line[0] = '\0';
    mr_source_set_error("HTTP chunk header is too large");
    return 0;
}

static int hex_value(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int parse_chunk_size(const char *line, size_t *size)
{
    size_t value = 0;
    int have = 0;
    while (*line == ' ' || *line == '\t') line++;
    while (*line) {
        int digit = hex_value((unsigned char)*line);
        if (digit < 0) break;
        if (value > (SIZE_MAX - (unsigned)digit) / 16u) return 0;
        value = value * 16u + (unsigned)digit;
        line++;
        have = 1;
    }
    if (!have) return 0;
    while (*line == ' ' || *line == '\t') line++;
    if (*line && *line != ';') return 0;
    *size = value;
    return 1;
}

static int begin_chunk(http_source *h)
{
    char line[HTTP_CHUNK_LINE_MAX];
    if (h->chunk_done) return 0;
    if (h->chunk_need_crlf) {
        unsigned char crlf[2];
        if (!raw_read_exact(h, crlf, sizeof crlf) ||
            crlf[0] != '\r' || crlf[1] != '\n') {
            mr_source_set_error("invalid HTTP chunk delimiter");
            return 0;
        }
        h->chunk_need_crlf = 0;
    }
    if (!raw_read_line(h, line, sizeof line) ||
        !parse_chunk_size(line, &h->chunk_left)) {
        mr_source_set_error("invalid HTTP chunk size");
        return 0;
    }
    if (!h->chunk_left) {
        size_t trailer_bytes = 0;
        do {
            if (!raw_read_line(h, line, sizeof line)) return 0;
            trailer_bytes += strlen(line) + 2;
            if (trailer_bytes > HTTP_HEADER_MAX) {
                mr_source_set_error("HTTP chunk trailers are too large");
                return 0;
            }
        } while (line[0]);
        h->chunk_done = 1;
        return 0;
    }
    if (h->response_left_known && h->chunk_left > h->response_left) {
        mr_source_set_error("HTTP chunk exceeds response length");
        return 0;
    }
    return 1;
}

static int find_header_end(const unsigned char *buf, size_t len)
{
    size_t i;
    for (i = 0; i + 3 < len; i++)
        if (buf[i] == '\r' && buf[i + 1] == '\n' &&
            buf[i + 2] == '\r' && buf[i + 3] == '\n')
            return (int)(i + 4);
    return -1;
}

static int header_value(const unsigned char *headers, size_t header_len,
                        const char *name, char *out, size_t out_size)
{
    size_t name_len = strlen(name);
    size_t pos = 0;
    while (pos < header_len) {
        size_t start = pos, end;
        while (pos < header_len && headers[pos] != '\n') pos++;
        end = pos;
        if (end > start && headers[end - 1] == '\r') end--;
        if (end - start > name_len &&
            ascii_ncasecmp((const char *)headers + start, name, name_len) == 0 &&
            headers[start + name_len] == ':') {
            size_t value = start + name_len + 1;
            size_t n;
            while (value < end &&
                   (headers[value] == ' ' || headers[value] == '\t'))
                value++;
            n = end - value;
            if (n >= out_size) n = out_size - 1;
            memcpy(out, headers + value, n);
            out[n] = '\0';
            return 1;
        }
        if (pos < header_len) pos++;
    }
    if (out_size) out[0] = '\0';
    return 0;
}

static int contains_nocase(const char *text, const char *needle)
{
    size_t n = strlen(needle);
    while (*text) {
        if (ascii_ncasecmp(text, needle, n) == 0) return 1;
        text++;
    }
    return 0;
}

static int response_status(const unsigned char *headers, size_t len)
{
    const char *p = (const char *)headers;
    const char *end = p + len;
    if (len < 12 || ascii_ncasecmp(p, "HTTP/", 5) != 0) return 0;
    while (p < end && *p != ' ') p++;
    while (p < end && *p == ' ') p++;
    if (p + 3 > end || p[0] < '0' || p[0] > '9' ||
        p[1] < '0' || p[1] > '9' || p[2] < '0' || p[2] > '9')
        return 0;
    return (p[0] - '0') * 100 + (p[1] - '0') * 10 + (p[2] - '0');
}

static int parse_content_range(const char *value, size_t *start,
                               size_t *last, size_t *total)
{
    const char *p = value, *end;
    while (*p == ' ' || *p == '\t') p++;
    if (ascii_ncasecmp(p, "bytes", 5) != 0) return 0;
    p += 5;
    while (*p == ' ' || *p == '\t') p++;
    if (!parse_decimal(p, &end, start) || *end != '-') return 0;
    p = end + 1;
    if (!parse_decimal(p, &end, last) || *end != '/') return 0;
    p = end + 1;
    if (*p == '*' || !parse_decimal(p, &end, total)) return 0;
    while (*end == ' ' || *end == '\t') end++;
    return !*end && *last >= *start && *last < *total;
}

static int read_headers(http_source *h, size_t *header_len)
{
    size_t used = 0;
    int end = -1;
    while (used < sizeof h->header) {
        int n = net_read_some(h, h->header + used, sizeof h->header - used);
        if (n <= 0) {
            mr_source_set_error("HTTP server closed before response headers");
            return 0;
        }
        used += (size_t)n;
        end = find_header_end(h->header, used);
        if (end >= 0) break;
    }
    if (end < 0) {
        mr_source_set_error("HTTP response headers are too large");
        return 0;
    }
    *header_len = (size_t)end;
    h->prefetch_pos = (size_t)end;
    h->prefetch_len = used;
    return 1;
}

static int probe_request_length(http_source *h, const char *method,
                                int one_byte_range, size_t *total_out)
{
    int redirects;
    for (redirects = 0; redirects <= HTTP_REDIRECT_MAX; redirects++) {
        http_url url;
        char request[HTTP_REQUEST_MAX];
        char host_header[HTTP_HOST_MAX + 16];
        char content_length[64], content_range[128];
        char location[HTTP_URL_MAX], next_url[HTTP_URL_MAX];
        size_t header_len, length = 0;
        size_t range_start = 0, range_last = 0, total = 0;
        int status, n, default_port;

        if (!parse_url(h->url, &url)) return 0;
        close_connection(h, 1);
        if (!connect_socket(h, &url)) return 0;
        default_port = (!url.tls && url.port == 80) ||
                       (url.tls && url.port == 443);
        if (default_port)
            snprintf(host_header, sizeof host_header, "%s", url.host);
        else
            snprintf(host_header, sizeof host_header, "%s:%u",
                     url.host, (unsigned)url.port);
        n = snprintf(request, sizeof request,
                     "%s %s HTTP/1.1\r\n"
                     "Host: %s\r\n"
                     "User-Agent: MintRIVA/0.1 AmigaOS\r\n"
                     "Accept: */*\r\n"
                     "Accept-Encoding: identity\r\n"
                     "%s"
                     "Connection: close\r\n"
                     "\r\n",
                     method, url.path, host_header,
                     one_byte_range ? "Range: bytes=0-0\r\n" : "");
        if (n <= 0 || (size_t)n >= sizeof request ||
            !net_write_all(h, request, (size_t)n) ||
            !read_headers(h, &header_len)) {
            close_connection(h, 0);
            return 0;
        }
        status = response_status(h->header, header_len);
        if (status == 301 || status == 302 || status == 303 ||
            status == 307 || status == 308) {
            if (redirects == HTTP_REDIRECT_MAX ||
                !header_value(h->header, header_len, "Location",
                              location, sizeof location) ||
                !resolve_redirect(h->url, location,
                                  next_url, sizeof next_url)) {
                close_connection(h, 1);
                return 0;
            }
            strcpy(h->url, next_url);
            close_connection(h, 1);
            continue;
        }
        if (status == 206 &&
            header_value(h->header, header_len, "Content-Range",
                         content_range, sizeof content_range) &&
            parse_content_range(content_range, &range_start,
                                &range_last, &total) &&
            range_start == 0 && total) {
            close_connection(h, 1);
            *total_out = total;
            return 1;
        }
        if (status == 200 &&
            header_value(h->header, header_len, "Content-Length",
                         content_length, sizeof content_length)) {
            const char *end;
            if (parse_decimal(content_length, &end, &length)) {
                while (*end == ' ' || *end == '\t') end++;
                if (!*end && length) {
                    close_connection(h, 1);
                    *total_out = length;
                    return 1;
                }
            }
        }
        close_connection(h, 1);
        return 0;
    }
    return 0;
}

static int probe_total_length(http_source *h)
{
    size_t total = 0;
    if (probe_request_length(h, "HEAD", 0, &total) ||
        probe_request_length(h, "GET", 1, &total)) {
        h->total_len = total;
        return 1;
    }
    mr_source_set_error("chunked HTTP media omitted a seekable file length");
    return 0;
}

static int begin_response(http_source *h, size_t offset)
{
    int redirects;
    for (redirects = 0; redirects <= HTTP_REDIRECT_MAX; redirects++) {
        http_url url;
        char request[HTTP_REQUEST_MAX];
        char host_header[HTTP_HOST_MAX + 16];
        char content_length[64], content_range[128], transfer[64];
        char location[HTTP_URL_MAX], next_url[HTTP_URL_MAX];
        size_t header_len, response_len = 0, content_length_value = 0;
        size_t range_start = 0, range_last = 0, total = 0;
        int status, n, default_port, have_content_length, chunked;

        if (!parse_url(h->url, &url)) {
            mr_source_set_error("invalid HTTP/HTTPS URL");
            return 0;
        }
        close_connection(h, 1);
        if (!connect_socket(h, &url)) return 0;

        default_port = (!url.tls && url.port == 80) ||
                       (url.tls && url.port == 443);
        if (default_port)
            snprintf(host_header, sizeof host_header, "%s", url.host);
        else
            snprintf(host_header, sizeof host_header, "%s:%u",
                     url.host, (unsigned)url.port);
        n = snprintf(request, sizeof request,
                     "GET %s HTTP/1.1\r\n"
                     "Host: %s\r\n"
                     "User-Agent: MintRIVA/0.1 AmigaOS\r\n"
                     "Accept: */*\r\n"
                     "Accept-Encoding: identity\r\n"
                     "Range: bytes=%lu-\r\n"
                     "Connection: close\r\n"
                     "\r\n",
                     url.path, host_header, (unsigned long)offset);
        if (n <= 0 || (size_t)n >= sizeof request) {
            mr_source_set_error("HTTP request is too large");
            close_connection(h, 1);
            return 0;
        }
        if (!net_write_all(h, request, (size_t)n) ||
            !read_headers(h, &header_len)) {
            close_connection(h, 0);
            return 0;
        }
        status = response_status(h->header, header_len);
        if (status == 301 || status == 302 || status == 303 ||
            status == 307 || status == 308) {
            if (redirects == HTTP_REDIRECT_MAX) {
                mr_source_set_error("too many HTTP redirects");
                close_connection(h, 1);
                return 0;
            }
            if (!header_value(h->header, header_len, "Location",
                              location, sizeof location) ||
                !resolve_redirect(h->url, location,
                                  next_url, sizeof next_url)) {
                mr_source_set_error("invalid HTTP redirect");
                close_connection(h, 1);
                return 0;
            }
            strcpy(h->url, next_url);
            close_connection(h, 1);
            continue;
        }
        if (status != 200 && status != 206) {
            source_error_status("HTTP media request failed", status);
            close_connection(h, 1);
            return 0;
        }
        if (offset && status != 206) {
            mr_source_set_error(
                "HTTP server does not support byte-range seeking");
            close_connection(h, 1);
            return 0;
        }

        chunked = header_value(h->header, header_len, "Transfer-Encoding",
                               transfer, sizeof transfer) &&
                  contains_nocase(transfer, "chunked");
        have_content_length = header_value(h->header, header_len,
                                           "Content-Length",
                                           content_length,
                                           sizeof content_length);
        if (have_content_length) {
            const char *end;
            if (!parse_decimal(content_length, &end, &response_len)) {
                mr_source_set_error("invalid HTTP Content-Length");
                close_connection(h, 1);
                return 0;
            }
            while (*end == ' ' || *end == '\t') end++;
            if (*end) {
                mr_source_set_error("invalid HTTP Content-Length");
                close_connection(h, 1);
                return 0;
            }
            content_length_value = response_len;
        }

        if (status == 206) {
            if (!header_value(h->header, header_len, "Content-Range",
                              content_range, sizeof content_range) ||
                !parse_content_range(content_range, &range_start,
                                     &range_last, &total) ||
                range_start != offset || !total) {
                mr_source_set_error("invalid HTTP Content-Range");
                close_connection(h, 1);
                return 0;
            }
            response_len = range_last - range_start + 1;
            if (!chunked && have_content_length &&
                response_len != content_length_value) {
                mr_source_set_error("HTTP range length mismatch");
                close_connection(h, 1);
                return 0;
            }
        } else {
            if (!have_content_length) {
                if (chunked && h->total_len) {
                    total = h->total_len;
                    response_len = total;
                } else if (chunked && !h->streaming) {
                    /* No length in the response. Try to discover one (HEAD or a
                     * range probe) so the media stays seekable; if the server
                     * offers neither, fall back to forward-only streaming. */
                    close_connection(h, 1);
                    if (!probe_total_length(h))
                        h->streaming = 1;
                    return begin_response(h, offset);
                } else if (chunked) {
                    /* Streaming re-entry: length stays unknown and EOF is
                     * signalled by the terminating zero-size chunk. */
                    total = 0;
                    response_len = 0;
                } else {
                    mr_source_set_error(
                        "HTTP media requires Content-Length or Content-Range");
                    close_connection(h, 1);
                    return 0;
                }
            } else {
                total = response_len;
            }
        }
        if (h->streaming) {
            /* A length-less stream can only be read from the start; a nonzero
             * offset would mean a seek the server cannot honour. */
            if (offset != 0) {
                mr_source_set_error("cannot seek a length-less HTTP stream");
                close_connection(h, 1);
                return 0;
            }
            h->total_len = 0;
            h->body_pos = 0;
            h->response_left_known = 0;
            h->response_left = 0;
            h->chunked = 1;
            h->chunk_left = 0;
            h->chunk_need_crlf = 0;
            h->chunk_done = 0;
            return 1;
        }
        if (!total || !response_len) {
            mr_source_set_error(
                "HTTP media requires Content-Length or Content-Range");
            close_connection(h, 1);
            return 0;
        }
        if (h->total_len && h->total_len != total) {
            mr_source_set_error("HTTP media length changed during playback");
            close_connection(h, 1);
            return 0;
        }
        h->total_len = total;
        h->body_pos = offset;
        h->response_left_known = 1;
        h->response_left = response_len;
        h->chunked = chunked;
        h->chunk_left = 0;
        h->chunk_need_crlf = 0;
        h->chunk_done = 0;
        return 1;
    }
    return 0;
}

static int copy_response_bytes(http_source *h, unsigned char *dst, size_t len)
{
    size_t done = 0;
    while (done < len) {
        size_t want = len - done;
        int n;
        if (h->response_left_known) {
            if (!h->response_left) break;
            if (want > h->response_left) want = h->response_left;
        }
        if (h->chunked) {
            if (!h->chunk_left && !begin_chunk(h)) break;
            if (want > h->chunk_left) want = h->chunk_left;
        }
        n = raw_read_some(h, dst + done, want);
        if (n <= 0) break;
        done += (size_t)n;
        h->body_pos += (size_t)n;
        if (h->response_left_known) h->response_left -= (size_t)n;
        if (h->chunked) {
            h->chunk_left -= (size_t)n;
            if (!h->chunk_left) h->chunk_need_crlf = 1;
        }
    }
    return (int)done;
}

static int cache_copy(const http_source *h, size_t off,
                      unsigned char *dst, size_t len)
{
    if (!h->cache || off < h->cache_start ||
        off - h->cache_start > h->cache_len ||
        len > h->cache_len - (off - h->cache_start))
        return 0;
    memcpy(dst, h->cache + (off - h->cache_start), len);
    return 1;
}

static void cache_store(http_source *h, size_t off,
                        const unsigned char *data, size_t len)
{
    size_t end, cache_end, overlap, keep;
    if (!h->cache || !len) return;
    if (len >= HTTP_CACHE_SIZE) {
        memcpy(h->cache, data + len - HTTP_CACHE_SIZE, HTTP_CACHE_SIZE);
        h->cache_start = off + len - HTTP_CACHE_SIZE;
        h->cache_len = HTTP_CACHE_SIZE;
        return;
    }
    end = off + len;
    cache_end = h->cache_start + h->cache_len;
    if (h->cache_len && off <= cache_end && end >= h->cache_start) {
        if (off < h->cache_start) {
            /* This uncommon backwards extension is simpler and bounded when
             * rebuilt as a fresh range. Sequential playback takes the append
             * branch below. */
            memcpy(h->cache, data, len);
            h->cache_start = off;
            h->cache_len = len;
            return;
        }
        overlap = cache_end > off ? cache_end - off : 0;
        if (overlap > len) overlap = len;
        if (len > overlap) {
            size_t add = len - overlap;
            if (h->cache_len + add > HTTP_CACHE_SIZE) {
                size_t drop = h->cache_len + add - HTTP_CACHE_SIZE;
                memmove(h->cache, h->cache + drop, h->cache_len - drop);
                h->cache_start += drop;
                h->cache_len -= drop;
            }
            memcpy(h->cache + h->cache_len, data + overlap, add);
            h->cache_len += add;
        }
        return;
    }
    keep = len;
    memcpy(h->cache, data, keep);
    h->cache_start = off;
    h->cache_len = keep;
}

static int http_read_at(void *opaque, size_t off, void *dst, size_t len)
{
    http_source *h = (http_source *)opaque;
    unsigned char *out = (unsigned char *)dst;
    size_t done = 0;
    int retries = 0;
    if (!len) return 1;
    if (cache_copy(h, off, out, len)) return 1;

    /* A demuxer may reread a header that overlaps the current network
     * position (TS's 512-byte sniff versus 188-byte packets). Reuse the cached
     * prefix and continue from the already-open response without reconnecting. */
    if (h->socket_ready && off < h->body_pos && off >= h->cache_start &&
        h->body_pos - off < len &&
        cache_copy(h, off, out, h->body_pos - off)) {
        done = h->body_pos - off;
    }
    if (!h->socket_ready || h->body_pos != off + done) {
        done = 0;
        if (!begin_response(h, off)) return 0;
    }
    while (done < len) {
        int n = copy_response_bytes(h, out + done, len - done);
        if (n > 0) {
            done += (size_t)n;
            continue;
        }
        close_connection(h, 0);
        if (done == len) break;
        if (retries++ >= HTTP_IO_RETRIES ||
            !begin_response(h, off + done))
            return 0;
    }
    cache_store(h, off, out, len);
    return 1;
}

static void platform_close(http_source *h)
{
    close_connection(h, !h->tls_quarantined);
#if MR_HTTP_HAVE_TLS
    if (!h->tls_quarantined && h->ssl_ctx) {
        SSL_CTX_free(h->ssl_ctx);
        h->ssl_ctx = NULL;
    }
#if MR_HTTP_AMIGA
    if (!h->tls_quarantined && h->tls_ready) {
        CleanupAmiSSL(TAG_DONE);
        h->tls_ready = 0;
    }
    if (!h->tls_quarantined && AmiSSLBase) {
        CloseAmiSSL();
        AmiSSLBase = NULL;
        AmiSSLExtBase = NULL;
    }
    if (!h->tls_quarantined && AmiSSLMasterBase) {
        CloseLibrary(AmiSSLMasterBase);
        AmiSSLMasterBase = NULL;
    }
#endif
#endif
#if MR_HTTP_AMIGA
    if (!h->tls_quarantined && SocketBase) {
        CloseLibrary(SocketBase);
        SocketBase = NULL;
    }
#endif
    h->platform_ready = 0;
}

static void http_close(void *opaque)
{
    http_source *h = (http_source *)opaque;
    if (!h) return;
    platform_close(h);
    free(h->cache);
    free(h);
}

mr_source *mr_http_source_open(const char *url)
{
    http_source *h;
    mr_source *source;
    size_t n;
    if (!url || !*url || strlen(url) >= HTTP_URL_MAX) {
        mr_source_set_error("HTTP URL is empty or too long");
        return NULL;
    }
    h = (http_source *)calloc(1, sizeof *h);
    if (!h) {
        mr_source_set_error("not enough memory for HTTP source");
        return NULL;
    }
    h->sock = -1;
    h->cache = (unsigned char *)malloc(HTTP_CACHE_SIZE);
    if (!h->cache) {
        mr_source_set_error("not enough memory for HTTP rewind cache");
        free(h);
        return NULL;
    }
    n = strlen(url);
    memcpy(h->url, url, n + 1);
    if (!platform_open(h) || !begin_response(h, 0)) {
        http_close(h);
        return NULL;
    }
    source = mr_source_create(h,
                              h->streaming ? MR_SOURCE_LEN_UNKNOWN : h->total_len,
                              http_read_at, http_close, h->url);
    return source;
}
