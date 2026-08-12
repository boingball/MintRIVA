#include "mr_iptv.h"
#include <stdlib.h>
#include <string.h>

static void copy(char *d, size_t n, const char *s, size_t z) {
  if (z >= n)
    z = n - 1;
  memcpy(d, s, z);
  d[z] = 0;
}
static void attr(const char *line, const char *key, char *out, size_t cap) {
  const char *p = strstr(line, key);
  const char *e;
  if (!p)
    return;
  p += strlen(key);
  if (*p != '=')
    return;
  p++;
  if (*p == '"') {
    p++;
    e = strchr(p, '"');
  } else {
    e = strchr(p, ' ');
  }
  if (!e)
    e = p + strlen(p);
  copy(out, cap, p, (size_t)(e - p));
}
static int append(mr_iptv_directory *d, const mr_iptv_channel *c) {
  void *p;
  size_t cap;
  if (d->channel_count == d->channel_capacity) {
    cap = d->channel_capacity ? d->channel_capacity * 2 : 32;
    p = realloc(d->channels, cap * sizeof(*d->channels));
    if (!p)
      return 0;
    d->channels = p;
    d->channel_capacity = cap;
  }
  d->channels[d->channel_count++] = *c;
  return 1;
}

int mr_iptv_parse_m3u(mr_iptv_directory *out, const char *data, size_t len) {
  const char *p = data, *end = data + len, *e;
  mr_iptv_directory d;
  mr_iptv_channel pending;
  int have = 0, header = 0;
  mr_iptv_init(&d);
  memset(&pending, 0, sizeof(pending));
  while (p < end) {
    e = memchr(p, '\n', (size_t)(end - p));
    if (!e)
      e = end;
    while (e > p && (e[-1] == '\r' || e[-1] == ' ' || e[-1] == '\t'))
      e--;
    if (!header) {
      if ((size_t)(e - p) != 7 || memcmp(p, "#EXTM3U", 7))
        goto fail;
      header = 1;
    } else if ((size_t)(e - p) > 8 && !memcmp(p, "#EXTINF:", 8)) {
      const char *comma = memchr(p, ',', (size_t)(e - p));
      char group[MR_IPTV_NAME_MAX];
      free(pending.categories);
      free(pending.streams);
      memset(&pending, 0, sizeof(pending));
      group[0] = 0;
      attr(p, "tvg-id", pending.id, sizeof(pending.id));
      attr(p, "tvg-name", pending.name, sizeof(pending.name));
      attr(p, "group-title", group, sizeof(group));
      if (group[0]) {
        pending.categories = calloc(1, sizeof(*pending.categories));
        if (!pending.categories)
          goto fail;
        copy(pending.categories[0], sizeof(pending.categories[0]), group,
             strlen(group));
        pending.category_count = 1;
      }
      if (comma && comma + 1 < e && !pending.name[0])
        copy(pending.name, sizeof(pending.name), comma + 1,
             (size_t)(e - comma - 1));
      have = 1;
    } else if (have && e > p && *p != '#') {
      char url[MR_IPTV_URL_MAX];
      copy(url, sizeof(url), p, (size_t)(e - p));
      if (mr_iptv_supported_url(url)) {
        if (!pending.id[0])
          copy(pending.id, sizeof(pending.id), pending.name,
               strlen(pending.name));
        pending.streams = (mr_iptv_stream *)calloc(1, sizeof(*pending.streams));
        if (!pending.streams)
          goto fail;
        copy(pending.streams[0].url, sizeof(pending.streams[0].url), url,
             strlen(url));
        pending.stream_count = 1;
        if (!append(&d, &pending))
          goto fail;
        pending.streams = NULL;
        pending.categories = NULL;
      }
      have = 0;
    }
    p = e < end ? e + 1 : end;
  }
  if (!header)
    goto fail;
  free(pending.categories);
  free(pending.streams);
  mr_iptv_free(out);
  *out = d;
  return 1;
fail:
  free(pending.categories);
  free(pending.streams);
  mr_iptv_free(&d);
  return 0;
}
