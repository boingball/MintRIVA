#include "mr_iptv.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define JSON_MAX_DEPTH 24
#define IPTV_MAX_CHANNELS 20000
#define IPTV_MAX_STREAMS 30000
typedef struct {
  const char *p, *end;
  int error;
} parser;

static void ws(parser *j) {
  while (j->p < j->end && isspace((unsigned char)*j->p))
    j->p++;
}
static int hex(char c) {
  return c >= '0' && c <= '9'   ? c - '0'
         : c >= 'a' && c <= 'f' ? c - 'a' + 10
         : c >= 'A' && c <= 'F' ? c - 'A' + 10
                                : -1;
}
static int string(parser *j, char *out, size_t cap) {
  size_t n = 0;
  int a, b, c, d, cp;
  ws(j);
  if (j->p == j->end || *j->p++ != '"')
    return j->error = 1, 0;
  while (j->p < j->end && *j->p != '"') {
    unsigned char ch = (unsigned char)*j->p++;
    if (ch < 0x20)
      return j->error = 1, 0;
    if (ch == '\\') {
      if (j->p == j->end)
        return j->error = 1, 0;
      ch = (unsigned char)*j->p++;
      if (ch == 'u') {
        if (j->end - j->p < 4 || (a = hex(j->p[0])) < 0 ||
            (b = hex(j->p[1])) < 0 || (c = hex(j->p[2])) < 0 ||
            (d = hex(j->p[3])) < 0)
          return j->error = 1, 0;
        cp = (a << 12) | (b << 8) | (c << 4) | d;
        j->p += 4;
        ch = cp < 128 ? (unsigned char)cp : '?';
      } else if (ch == 'n')
        ch = '\n';
      else if (ch == 'r')
        ch = '\r';
      else if (ch == 't')
        ch = '\t';
      else if (ch == 'b')
        ch = '\b';
      else if (ch == 'f')
        ch = '\f';
      else if (ch != '"' && ch != '\\' && ch != '/')
        return j->error = 1, 0;
    }
    if (cap && n + 1 < cap)
      out[n++] = (char)ch;
  }
  if (j->p == j->end)
    return j->error = 1, 0;
  j->p++;
  if (cap)
    out[n] = 0;
  return 1;
}
static int skip(parser *, int);
static int skip(parser *j, int depth) {
  char key[2];
  char open, close;
  if (depth > JSON_MAX_DEPTH)
    return j->error = 1, 0;
  ws(j);
  if (j->p == j->end)
    return j->error = 1, 0;
  if (*j->p == '"')
    return string(j, key, sizeof(key));
  if (*j->p == '{' || *j->p == '[') {
    open = *j->p++;
    close = open == '{' ? '}' : ']';
    ws(j);
    if (j->p < j->end && *j->p == close)
      return j->p++, 1;
    for (;;) {
      if (open == '{' && (!string(j, key, sizeof(key)) ||
                          (ws(j), j->p == j->end || *j->p++ != ':')))
        return j->error = 1, 0;
      if (!skip(j, depth + 1))
        return 0;
      ws(j);
      if (j->p == j->end)
        return j->error = 1, 0;
      if (*j->p == close)
        return j->p++, 1;
      if (*j->p++ != ',')
        return j->error = 1, 0;
    }
  }
  while (j->p < j->end && !strchr(",]} \t\r\n", *j->p))
    j->p++;
  return 1;
}
static int boolean(parser *j, unsigned *v) {
  ws(j);
  if (j->end - j->p >= 4 && !memcmp(j->p, "true", 4))
    return j->p += 4, *v = 1, 1;
  if (j->end - j->p >= 5 && !memcmp(j->p, "false", 5))
    return j->p += 5, *v = 0, 1;
  return j->error = 1, 0;
}
static int str_array(parser *j, char a[][MR_IPTV_NAME_MAX], unsigned *count,
                     unsigned max) {
  ws(j);
  if (j->p == j->end || *j->p++ != '[')
    return j->error = 1, 0;
  ws(j);
  if (j->p < j->end && *j->p == ']')
    return j->p++, 1;
  for (;;) {
    char tmp[MR_IPTV_NAME_MAX];
    if (!string(j, tmp, sizeof(tmp)))
      return 0;
    if (*count < max)
      strcpy(a[(*count)++], tmp);
    ws(j);
    if (j->p == j->end)
      return j->error = 1, 0;
    if (*j->p == ']')
      return j->p++, 1;
    if (*j->p++ != ',')
      return j->error = 1, 0;
  }
}
static int grow(mr_iptv_directory *d) {
  size_t cap = d->channel_capacity ? d->channel_capacity * 2 : 64;
  void *p;
  if (cap > IPTV_MAX_CHANNELS)
    cap = IPTV_MAX_CHANNELS;
  if (d->channel_count == cap)
    return 0;
  p = realloc(d->channels, cap * sizeof(*d->channels));
  if (!p)
    return 0;
  d->channels = p;
  d->channel_capacity = cap;
  return 1;
}

int mr_iptv_parse_channels(mr_iptv_directory *out, const char *data,
                           size_t len) {
  parser j = {data, data + len, 0};
  mr_iptv_directory d;
  mr_iptv_init(&d);
  ws(&j);
  if (j.p == j.end || *j.p++ != '[')
    goto fail;
  ws(&j);
  if (j.p < j.end && *j.p == ']') {
    j.p++;
    goto done;
  }
  for (;;) {
    mr_iptv_channel c;
    memset(&c, 0, sizeof(c));
    ws(&j);
    if (j.p == j.end || *j.p++ != '{')
      goto fail;
    ws(&j);
    while (j.p < j.end && *j.p != '}') {
      char key[40];
      unsigned b;
      if (!string(&j, key, sizeof(key)) ||
          (ws(&j), j.p == j.end || *j.p++ != ':'))
        goto fail;
      if (!strcmp(key, "id")) {
        if (!string(&j, c.id, sizeof(c.id)))
          goto fail;
      } else if (!strcmp(key, "name")) {
        if (!string(&j, c.name, sizeof(c.name)))
          goto fail;
      } else if (!strcmp(key, "network")) {
        if (!string(&j, c.network, sizeof(c.network)))
          goto fail;
      } else if (!strcmp(key, "country")) {
        if (!string(&j, c.country, sizeof(c.country)))
          goto fail;
      } else if (!strcmp(key, "alt_names")) {
        if (!str_array(&j, c.alt_names, &c.alt_count, MR_IPTV_ALT_MAX))
          goto fail;
      } else if (!strcmp(key, "categories")) {
        if (!str_array(&j, c.categories, &c.category_count,
                       MR_IPTV_CATEGORY_MAX))
          goto fail;
      } else if (!strcmp(key, "is_nsfw")) {
        if (!boolean(&j, &b))
          goto fail;
        c.is_nsfw = b;
      } else if (!strcmp(key, "closed")) {
        ws(&j);
        if (j.p < j.end && *j.p == '"') {
          char x[8];
          if (!string(&j, x, sizeof(x)))
            goto fail;
          c.closed = x[0] != 0;
        } else if (!skip(&j, 1))
          goto fail;
      } else if (!strcmp(key, "replaced_by")) {
        ws(&j);
        if (j.p < j.end && *j.p == '"') {
          char x[8];
          if (!string(&j, x, sizeof(x)))
            goto fail;
          c.replaced = x[0] != 0;
        } else if (!skip(&j, 1))
          goto fail;
      } else if (!skip(&j, 1))
        goto fail;
      ws(&j);
      if (j.p < j.end && *j.p == ',') {
        j.p++;
        ws(&j);
      } else
        break;
    }
    if (j.p == j.end || *j.p++ != '}')
      goto fail;
    if (c.id[0] && c.name[0]) {
      if (d.channel_count == d.channel_capacity && !grow(&d))
        goto fail;
      d.channels[d.channel_count++] = c;
    }
    ws(&j);
    if (j.p == j.end)
      goto fail;
    if (*j.p == ']') {
      j.p++;
      break;
    }
    if (*j.p++ != ',')
      goto fail;
  }
done:
  ws(&j);
  if (j.p != j.end)
    goto fail;
  mr_iptv_free(out);
  *out = d;
  return 1;
fail:
  mr_iptv_free(&d);
  return 0;
}

typedef struct {
  char channel[MR_IPTV_ID_MAX];
  mr_iptv_stream stream;
} pending;
int mr_iptv_join_streams(mr_iptv_directory *d, const char *data, size_t len) {
  parser j = {data, data + len, 0};
  pending *all = NULL;
  size_t n = 0, cap = 0, i, k;
  ws(&j);
  if (j.p == j.end || *j.p++ != '[')
    goto fail;
  ws(&j);
  while (j.p < j.end && *j.p != ']') {
    pending x;
    memset(&x, 0, sizeof(x));
    if (*j.p++ != '{')
      goto fail;
    ws(&j);
    while (j.p < j.end && *j.p != '}') {
      char key[40];
      if (!string(&j, key, sizeof(key)) ||
          (ws(&j), j.p == j.end || *j.p++ != ':'))
        goto fail;
      if (!strcmp(key, "channel")) {
        if (!string(&j, x.channel, sizeof(x.channel)))
          goto fail;
      } else if (!strcmp(key, "url")) {
        if (!string(&j, x.stream.url, sizeof(x.stream.url)))
          goto fail;
      } else if (!strcmp(key, "http_referrer")) {
        if (!string(&j, x.stream.http_referrer, sizeof(x.stream.http_referrer)))
          goto fail;
      } else if (!strcmp(key, "user_agent")) {
        if (!string(&j, x.stream.user_agent, sizeof(x.stream.user_agent)))
          goto fail;
      } else if (!skip(&j, 1))
        goto fail;
      ws(&j);
      if (j.p < j.end && *j.p == ',') {
        j.p++;
        ws(&j);
      } else
        break;
    }
    if (j.p == j.end || *j.p++ != '}')
      goto fail;
    if (x.channel[0] && mr_iptv_valid_url(x.stream.url)) {
      if (n == IPTV_MAX_STREAMS)
        goto fail;
      if (n == cap) {
        size_t nc = cap ? cap * 2 : 128;
        void *p = realloc(all, nc * sizeof(*all));
        if (!p)
          goto fail;
        all = p;
        cap = nc;
      }
      all[n++] = x;
    }
    ws(&j);
    if (j.p < j.end && *j.p == ',') {
      j.p++;
      ws(&j);
    } else
      break;
  }
  if (j.p == j.end || *j.p++ != ']')
    goto fail;
  ws(&j);
  if (j.p != j.end)
    goto fail;
  for (i = 0; i < d->channel_count; i++) {
    mr_iptv_channel *c = &d->channels[i];
    c->stream_count = 0;
    for (k = 0; k < n; k++)
      if (!strcmp(all[k].channel, c->id) &&
          c->stream_count < MR_IPTV_STREAM_MAX)
        c->streams[c->stream_count++] = all[k].stream;
    for (k = 0; k < n && c->stream_count < MR_IPTV_STREAM_MAX; k++) {
      size_t z = strlen(c->id);
      if (!strncmp(all[k].channel, c->id, z) && all[k].channel[z] == '@')
        c->streams[c->stream_count++] = all[k].stream;
    }
  }
  free(all);
  return 1;
fail:
  free(all);
  return 0;
}
