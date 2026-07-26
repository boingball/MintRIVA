#ifndef MR_IPTV_H
#define MR_IPTV_H

#include <stddef.h>

#define MR_IPTV_ID_MAX 128
#define MR_IPTV_NAME_MAX 128
#define MR_IPTV_URL_MAX 1024
#define MR_IPTV_ALT_MAX 8
#define MR_IPTV_STREAM_MAX 8
#define MR_IPTV_CATEGORY_MAX 8

typedef struct {
  char url[MR_IPTV_URL_MAX];
  char http_referrer[256];
  char user_agent[256];
} mr_iptv_stream;

typedef struct {
  char id[MR_IPTV_ID_MAX], name[MR_IPTV_NAME_MAX];
  char network[MR_IPTV_NAME_MAX], country[8];
  char alt_names[MR_IPTV_ALT_MAX][MR_IPTV_NAME_MAX];
  char categories[MR_IPTV_CATEGORY_MAX][MR_IPTV_NAME_MAX];
  unsigned alt_count, category_count, stream_count;
  unsigned is_nsfw : 1, closed : 1, replaced : 1;
  mr_iptv_stream streams[MR_IPTV_STREAM_MAX];
} mr_iptv_channel;

typedef struct {
  mr_iptv_channel *channels;
  size_t channel_count, channel_capacity;
} mr_iptv_directory;

typedef struct {
  const char *country, *category, *search;
} mr_iptv_filter;

void mr_iptv_init(mr_iptv_directory *directory);
void mr_iptv_free(mr_iptv_directory *directory);
int mr_iptv_parse_channels(mr_iptv_directory *, const char *, size_t);
int mr_iptv_join_streams(mr_iptv_directory *, const char *, size_t);
int mr_iptv_parse_m3u(mr_iptv_directory *, const char *, size_t);
int mr_iptv_channel_visible(const mr_iptv_channel *, const mr_iptv_filter *);
size_t mr_iptv_filter_channels(const mr_iptv_directory *,
                               const mr_iptv_filter *, size_t *, size_t);
int mr_iptv_valid_url(const char *url);
const char *mr_iptv_last_error(void);

#endif
