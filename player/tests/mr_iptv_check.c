#include "../iptv/mr_iptv.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
  mr_iptv_directory d;
  char *many;
  size_t many_used;
  int many_index;
  size_t ids[8];
  mr_iptv_filter f;
  const char *c =
      "[{\"id\":\"BBCNews.uk\",\"name\":\"BBC News\",\"alt_names\":[\"BBC "
      "World\"],\"network\":\"BBC\",\"country\":\"GB\",\"categories\":["
      "\"news\"],\"is_nsfw\":false,\"unknown\":{\"x\":[1]}},{\"id\":\"Fun.uk\","
      "\"name\":\"Fun "
      "\\u2603\",\"alt_names\":[],\"network\":\"Net\",\"country\":\"GB\","
      "\"categories\":[\"entertainment\"],\"is_nsfw\":false},{\"id\":\"US.us\","
      "\"name\":\"US "
      "One\",\"country\":\"US\",\"categories\":[\"news\"],\"alt_names\":[],"
      "\"is_nsfw\":false},{\"id\":\"Adult.uk\",\"name\":\"Adult\",\"country\":"
      "\"GB\",\"categories\":[],\"alt_names\":[],\"is_nsfw\":true},{\"id\":"
      "\"Closed.uk\",\"name\":\"Closed\",\"country\":\"GB\",\"categories\":[],"
      "\"alt_names\":[],\"is_nsfw\":false,\"closed\":\"2020\"},{\"id\":\"Empty."
      "uk\",\"name\":\"Empty\",\"country\":\"GB\",\"categories\":[],\"alt_"
      "names\":[],\"is_nsfw\":false}]";
  const char *s =
      "[{\"channel\":\"BBCNews.uk\",\"url\":\"https://one.test/"
      "live.m3u8\",\"status\":\"online\"},{\"channel\":\"BBCNews.uk\",\"url\":"
      "\"http://two.test/live\",\"http_referrer\":\"https://ref.test/"
      "\"},{\"channel\":\"Fun.uk@East\",\"url\":\"https://fun.test/"
      "a.ts\"},{\"channel\":\"US.us\",\"url\":\"not a "
      "url\"},{\"channel\":\"Adult.uk\",\"url\":\"http://adult.test/"
      "a\"},{\"channel\":\"Closed.uk\",\"url\":\"http://closed.test/a\"}]";
  const char *m = "#EXTM3U\n#EXTINF:-1 tvg-id=\"nasa.us\" tvg-name=\"NASA TV\" "
                  "group-title=\"Science\",NASA\nhttps://nasa.test/live.m3u8\n";
  const char *nullable_channels =
      "[{\"id\":\"Example.uk\",\"name\":\"Caf\xc3\xa9 TV\",\"alt_names\":[],"
      "\"network\":null,\"country\":\"GB\",\"categories\":[\"news\"],"
      "\"is_nsfw\":false,\"website\":null,\"closed\":null,"
      "\"replaced_by\":null,"
      "\"owner\":null,\"extra\":{\"nothing\":null,\"values\":[true,false,"
      "null,12.5]}}]  \n";
  const char *nullable_streams =
      "[{\"channel\":null,\"url\":\"https://unused.test/live.m3u8\","
      "\"http_referrer\":null,\"user_agent\":null,\"extra\":null},"
      "{\"channel\":\"Example.uk\",\"url\":\"https://example.com/live.m3u8\","
      "\"http_referrer\":null,\"user_agent\":null}]\t";
  mr_iptv_init(&d);
  assert(mr_iptv_parse_channels(&d, c, strlen(c)));
  assert(d.channel_count == 6);
  assert(mr_iptv_join_streams(&d, s, strlen(s)));
  assert(d.channels[0].stream_count == 2);
  assert(d.channels[1].stream_count == 1);
  assert(!strcmp(d.channels[1].name, "Fun ?"));
  f.country = "GB";
  f.category = "news";
  f.search = "world";
  assert(mr_iptv_filter_channels(&d, &f, ids, 8) == 1 && ids[0] == 0);
  f.category = "All";
  f.search = "net";
  assert(mr_iptv_filter_channels(&d, &f, ids, 8) == 1);
  assert(!mr_iptv_parse_channels(&d, "[{", 2));
  assert(d.channel_count == 6);
  assert(!mr_iptv_join_streams(&d, "[", 1));
  mr_iptv_free(&d);
  mr_iptv_init(&d);
  assert(mr_iptv_parse_m3u(&d, m, strlen(m)));
  assert(d.channel_count == 1);
  assert(!strcmp(d.channels[0].id, "nasa.us"));
  assert(!strcmp(d.channels[0].categories[0], "Science"));
  assert(!mr_iptv_parse_m3u(&d, "bad", 3));
  assert(d.channel_count == 1);
  mr_iptv_free(&d);
  mr_iptv_init(&d);
  assert(
      mr_iptv_parse_channels(&d, nullable_channels, strlen(nullable_channels)));
  assert(d.channel_count == 1 && !d.channels[0].network[0]);
  assert(!d.channels[0].closed && !d.channels[0].replaced);
  assert(!strcmp(d.channels[0].name, "Caf\xc3\xa9 TV"));
  assert(mr_iptv_join_streams(&d, nullable_streams, strlen(nullable_streams)));
  assert(d.channels[0].stream_count == 1);
  assert(!d.channels[0].streams[0].http_referrer[0]);
  assert(!d.channels[0].streams[0].user_agent[0]);
  assert(mr_iptv_parse_channels(&d, "[{\"website\":null}]",
                                sizeof("[{\"website\":null}]") - 1));
  assert(mr_iptv_parse_channels(
      &d, "[{\"website\":\"https://example.com\"}]",
      sizeof("[{\"website\":\"https://example.com\"}]") - 1));
  assert(!mr_iptv_parse_channels(&d, "[{\"website\":123}]",
                                 sizeof("[{\"website\":123}]") - 1));
  assert(strstr(mr_iptv_last_error(), "field website"));
  assert(strstr(mr_iptv_last_error(), "byte 12"));
  assert(
      !mr_iptv_parse_channels(&d, "[{\"id\":\"x\",\"network\":nul}]",
                              sizeof("[{\"id\":\"x\",\"network\":nul}]") - 1));
  assert(strstr(mr_iptv_last_error(), "field network"));
  assert(strstr(mr_iptv_last_error(), "byte"));
  assert(
      !mr_iptv_parse_channels(&d, "[{\"id\":\"x\",\"network\":NULL}]",
                              sizeof("[{\"id\":\"x\",\"network\":NULL}]") - 1));
  assert(!mr_iptv_parse_channels(
      &d, "[{\"id\":\"x\",\"network\":null \"country\":\"GB\"}]",
      sizeof("[{\"id\":\"x\",\"network\":null \"country\":\"GB\"}]") - 1));
  assert(strstr(mr_iptv_last_error(), "',' or '}'"));
  assert(!mr_iptv_join_streams(
      &d, "[{\"channel\":\"Example.uk\",\"url\":none}]",
      sizeof("[{\"channel\":\"Example.uk\",\"url\":none}]") - 1));
  assert(strstr(mr_iptv_last_error(), "field url"));
  many = (char *)malloc(220000);
  assert(many);
  many[0] = '[';
  many_used = 1;
  for (many_index = 0; many_index < 1100; many_index++)
    many_used += (size_t)snprintf(
        many + many_used, 220000 - many_used,
        "%s{\"id\":\"Test%d.uk\",\"name\":\"Test\",\"network\":null,"
        "\"website\":null,\"country\":\"GB\",\"categories\":[],"
        "\"alt_names\":[],\"is_nsfw\":false,\"closed\":null,"
        "\"replaced_by\":null}",
        many_index ? "," : "", many_index);
  many[many_used++] = ']';
  many[many_used] = 0;
  assert(mr_iptv_parse_channels(&d, many, many_used));
  assert(d.channel_count == 1100);
  free(many);
  mr_iptv_free(&d);
  puts("IPTV parser/filter checks passed");
  return 0;
}
