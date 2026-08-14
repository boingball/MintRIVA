#include "mr_iptv.h"
#include "../core/mr_play_options.h"
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

static int url_has_extension(const char *url, const char *extension) {
  const char *end, *dot = NULL, *p;
  size_t length, i;
  if (!url || !extension)
    return 0;
  end = url + strlen(url);
  for (p = url; p < end && *p != '?' && *p != '#'; p++) {
    if (*p == '/')
      dot = NULL;
    else if (*p == '.')
      dot = p;
  }
  end = p;
  if (!dot)
    return 0;
  length = strlen(extension);
  if ((size_t)(end - dot) != length)
    return 0;
  for (i = 0; i < length; i++)
    if (tolower((unsigned char)dot[i]) !=
        tolower((unsigned char)extension[i]))
      return 0;
  return 1;
}

int mr_iptv_supported_url(const char *url) {
  /* MintVID supports HLS and direct HTTP media, but not MPEG-DASH. Keeping a
   * known .mpd entry makes the browser advertise a channel which can only fail
   * later as an unsupported container. Unknown extensions remain eligible so
   * server-generated and extensionless HLS URLs continue to work. */
  return mr_iptv_valid_url(url) && !url_has_extension(url, ".mpd");
}

int mr_iptv_build_mrplay_args(const mr_iptv_stream *stream, char *out,
                              size_t out_size) {
  mr_play_options options;
  if (!stream || !out || !out_size ||
      !memchr(stream->url, 0, sizeof(stream->url)) ||
      !memchr(stream->user_agent, 0, sizeof(stream->user_agent)) ||
      !memchr(stream->http_referrer, 0, sizeof(stream->http_referrer)) ||
      !mr_iptv_supported_url(stream->url))
    return 0;
  mr_play_options_default(&options);
  return mr_build_player_arguments(out, out_size, &options, stream->url,
                                   stream->user_agent,
                                   stream->http_referrer);
}
