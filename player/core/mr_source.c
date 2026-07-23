/*
 * MintRIVA - local-file and generic source ownership.
 */
#include "mr_source.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MR_SOURCE_NAME_MAX 1024
#define MR_SOURCE_ERROR_MAX 192

struct mr_source {
    void   *ctx;
    size_t  len;
    int   (*read_at)(void *, size_t, void *, size_t);
    void  (*close)(void *);
    char    final_name[MR_SOURCE_NAME_MAX];
};

typedef struct {
    FILE   *file;
    size_t  pos;
    int     pos_valid;
} file_source;

static char g_source_error[MR_SOURCE_ERROR_MAX];

void mr_source_set_error(const char *message)
{
    size_t n;
    if (!message) message = "source open failed";
    n = strlen(message);
    if (n >= sizeof g_source_error) n = sizeof g_source_error - 1;
    memcpy(g_source_error, message, n);
    g_source_error[n] = '\0';
}

const char *mr_source_last_error(void)
{
    return g_source_error[0] ? g_source_error : "source open failed";
}

static int starts_nocase(const char *s, const char *prefix)
{
    while (*prefix) {
        int a = (unsigned char)*s++;
        int b = (unsigned char)*prefix++;
        if (a >= 'A' && a <= 'Z') a += 'a' - 'A';
        if (b >= 'A' && b <= 'Z') b += 'a' - 'A';
        if (a != b) return 0;
    }
    return 1;
}

int mr_source_is_url(const char *path)
{
    return path && (starts_nocase(path, "http://") ||
                    starts_nocase(path, "https://"));
}

mr_source *mr_source_create(void *ctx, size_t len,
                            int (*read_at)(void *, size_t, void *, size_t),
                            void (*close)(void *),
                            const char *final_name)
{
    mr_source *s;
    size_t n;
    if (!ctx || !read_at || !close || !len) return NULL;
    s = (mr_source *)calloc(1, sizeof *s);
    if (!s) {
        close(ctx);
        mr_source_set_error("not enough memory for media source");
        return NULL;
    }
    s->ctx = ctx;
    s->len = len;
    s->read_at = read_at;
    s->close = close;
    if (!final_name) final_name = "";
    n = strlen(final_name);
    if (n >= sizeof s->final_name) n = sizeof s->final_name - 1;
    memcpy(s->final_name, final_name, n);
    s->final_name[n] = '\0';
    return s;
}

static int file_read_at(void *opaque, size_t off, void *dst, size_t len)
{
    file_source *f = (file_source *)opaque;
    if (!f || !f->file || (!dst && len)) return 0;
    if (!f->pos_valid || f->pos != off) {
        if (off > 0x7fffffffUL || fseek(f->file, (long)off, SEEK_SET) != 0) {
            f->pos_valid = 0;
            return 0;
        }
    }
    if (len && fread(dst, 1, len, f->file) != len) {
        f->pos_valid = 0;
        return 0;
    }
    f->pos = off + len;
    f->pos_valid = 1;
    return 1;
}

static void file_close(void *opaque)
{
    file_source *f = (file_source *)opaque;
    if (!f) return;
    if (f->file) fclose(f->file);
    free(f);
}

static mr_source *open_local_file(const char *path)
{
    file_source *ctx;
    mr_source *source;
    long end;
    FILE *file = fopen(path, "rb");
    if (!file) {
        mr_source_set_error("cannot open local file");
        return NULL;
    }
    if (fseek(file, 0, SEEK_END) != 0 || (end = ftell(file)) <= 0 ||
        fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        mr_source_set_error("cannot determine local file size");
        return NULL;
    }
    ctx = (file_source *)calloc(1, sizeof *ctx);
    if (!ctx) {
        fclose(file);
        mr_source_set_error("not enough memory for local file");
        return NULL;
    }
    ctx->file = file;
    ctx->pos = 0;
    ctx->pos_valid = 1;
    source = mr_source_create(ctx, (size_t)end, file_read_at, file_close, path);
    return source;
}

mr_source *mr_source_open(const char *path)
{
    g_source_error[0] = '\0';
    if (!path || !*path) {
        mr_source_set_error("empty media path");
        return NULL;
    }
    if (mr_source_is_url(path))
        return mr_http_source_open(path);
    return open_local_file(path);
}

int mr_source_read_at(mr_source *s, size_t off, void *dst, size_t len)
{
    if (!s || (!dst && len) || off > s->len || len > s->len - off)
        return 0;
    return s->read_at(s->ctx, off, dst, len);
}

size_t mr_source_length(const mr_source *s)
{
    return s ? s->len : 0;
}

const char *mr_source_final_name(const mr_source *s)
{
    return s ? s->final_name : "";
}

void mr_source_close(mr_source *s)
{
    if (!s) return;
    s->close(s->ctx);
    free(s);
}
