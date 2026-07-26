#include "mr_iptv.h"
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void mr_iptv_init(mr_iptv_directory *d) { memset(d, 0, sizeof(*d)); }
void mr_iptv_free(mr_iptv_directory *d) {
  size_t i;
  for (i = 0; i < d->channel_count; i++) {
    free(d->channels[i].alt_names);
    free(d->channels[i].categories);
    free(d->channels[i].streams);
  }
  free(d->channels);
  mr_iptv_init(d);
}

static int contains_ci(const char *s, const char *needle) {
  size_t n = strlen(needle), i;
  if (!n)
    return 1;
  for (; *s; s++) {
    for (i = 0;
         i < n && s[i] &&
         tolower((unsigned char)s[i]) == tolower((unsigned char)needle[i]);
         i++)
      ;
    if (i == n)
      return 1;
  }
  return 0;
}

int mr_iptv_channel_visible(const mr_iptv_channel *c, const mr_iptv_filter *f) {
  unsigned i;
  int category = 0, search = 0;
  if (!c || c->is_nsfw || c->closed || !c->stream_count)
    return 0;
  if (f && f->country && *f->country && strcmp(f->country, "All") &&
      strcmp(c->country, f->country))
    return 0;
  if (f && f->category && *f->category && strcmp(f->category, "All")) {
    for (i = 0; i < c->category_count; i++)
      if (!strcmp(c->categories[i], f->category))
        category = 1;
    if (!category)
      return 0;
  }
  if (!f || !f->search || !*f->search)
    return 1;
  search = contains_ci(c->name, f->search) || contains_ci(c->id, f->search) ||
           contains_ci(c->network, f->search);
  for (i = 0; !search && i < c->alt_count; i++)
    search = contains_ci(c->alt_names[i], f->search);
  return search;
}

size_t mr_iptv_filter_channels(const mr_iptv_directory *d,
                               const mr_iptv_filter *f, size_t *indices,
                               size_t capacity) {
  size_t i, n = 0;
  for (i = 0; i < d->channel_count; i++)
    if (mr_iptv_channel_visible(&d->channels[i], f)) {
      if (n < capacity)
        indices[n] = i;
      n++;
    }
  return n;
}

int mr_iptv_valid_url(const char *u) {
  const char *p;
  if (!u || (strncmp(u, "http://", 7) && strncmp(u, "https://", 8)))
    return 0;
  p = strstr(u, "://") + 3;
  return *p && !strchr(p, ' ') && !strchr(p, '\r') && !strchr(p, '\n');
}

static int append_quoted(char *out, size_t cap, const char *option,
                         const char *value) {
  size_t n = strlen(out), i;
  int written;
  written = snprintf(out + n, cap > n ? cap - n : 0, "%s%s%s\"",
                     n ? " " : "", option ? option : "",
                     option ? " " : "");
  if (written < 0 || (size_t)written >= (cap > n ? cap - n : 0))
    return 0;
  n += (size_t)written;
  for (i = 0; value[i]; i++) {
    if (value[i] == '\r' || value[i] == '\n')
      return 0;
    if ((value[i] == '"' || value[i] == '*') && n + 1 >= cap)
      return 0;
    if (value[i] == '"' || value[i] == '*')
      out[n++] = '*';
    if (n + 1 >= cap)
      return 0;
    out[n++] = value[i];
  }
  if (n + 2 >= cap)
    return 0;
  out[n++] = '"';
  out[n] = 0;
  return 1;
}

int mr_iptv_build_mrplay_args(const mr_iptv_stream *stream, char *out,
                              size_t out_size) {
  if (!stream || !out || !out_size ||
      !memchr(stream->url, 0, sizeof(stream->url)) ||
      !memchr(stream->user_agent, 0, sizeof(stream->user_agent)) ||
      !memchr(stream->http_referrer, 0, sizeof(stream->http_referrer)) ||
      !mr_iptv_valid_url(stream->url))
    return 0;
  out[0] = 0;
  if ((stream->user_agent[0] &&
       !append_quoted(out, out_size, "--user-agent", stream->user_agent)) ||
      (stream->http_referrer[0] &&
       !append_quoted(out, out_size, "--referer", stream->http_referrer)) ||
      !append_quoted(out, out_size, NULL, stream->url))
    return 0;
  if (strlen(out) + 2 > out_size)
    return 0;
  strcat(out, "\n");
  return 1;
}
