#include "../youtube/mr_youtube_search.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition, message)                                             \
    do {                                                                      \
        if (!(condition)) {                                                   \
            fprintf(stderr, "FAIL: %s\n", message);                         \
            failures++;                                                       \
        }                                                                     \
    } while (0)

int main(void)
{
    static const char fixture[] =
        "<html><script>var ytInitialData={\"contents\":["
        "{\"videoRenderer\":{\"videoId\":\"LIVE1234567\","
        "\"title\":{\"runs\":[{\"text\":\"News \\u0026 weather\"}]},"
        "\"ownerText\":{\"runs\":[{\"text\":\"Test Channel\"}]},"
        "\"badges\":[{\"metadataBadgeRenderer\":{"
        "\"style\":\"BADGE_STYLE_TYPE_LIVE_NOW\"}}]}},"
        "{\"videoRenderer\":{\"videoId\":\"VIDEO123456\","
        "\"title\":{\"simpleText\":\"Recorded video\"},"
        "\"ownerText\":{\"runs\":[{\"text\":\"Archive\"}]}}},"
        "{\"videoRenderer\":{\"videoId\":\"LIVE1234567\","
        "\"title\":{\"runs\":[{\"text\":\"Duplicate\"}]},"
        "\"badges\":[{\"metadataBadgeRenderer\":{"
        "\"style\":\"BADGE_STYLE_TYPE_LIVE_NOW\"}}]}}]};</script></html>";
    mr_youtube_search_results results;
    char url[512];

    mr_youtube_search_results_init(&results);
    CHECK(mr_youtube_search_build_url(url, sizeof(url), "Amiga live!", 1),
          "build live search URL");
    CHECK(strstr(url, "Amiga%20live%21") != NULL, "URL encodes query");
    CHECK(strstr(url, "sp=EgJAAQ%253D%253D") != NULL,
          "live search includes YouTube live filter");

    CHECK(mr_youtube_search_parse(&results, fixture, strlen(fixture), 1),
          "parse live-only fixture");
    CHECK(results.count == 1, "live-only parse filters and deduplicates");
    if (results.count) {
        CHECK(!strcmp(results.items[0].video_id, "LIVE1234567"),
              "video id extracted");
        CHECK(!strcmp(results.items[0].title, "News & weather"),
              "JSON title decoded");
        CHECK(!strcmp(results.items[0].channel, "Test Channel"),
              "channel extracted");
        CHECK(results.items[0].live, "live badge detected");
        CHECK(mr_youtube_search_watch_url(url, sizeof(url), &results.items[0]),
              "watch URL built");
        CHECK(!strcmp(url,
                      "https://www.youtube.com/watch?v=LIVE1234567"),
              "watch URL correct");
    }
    mr_youtube_search_results_free(&results);

    CHECK(mr_youtube_search_parse(&results, fixture, strlen(fixture), 0),
          "parse all-result fixture");
    CHECK(results.count == 2, "all-result parse includes recorded video");
    if (results.count == 2)
        CHECK(!results.items[1].live, "recorded video is not marked live");
    mr_youtube_search_results_free(&results);

    CHECK(!mr_youtube_search_build_url(url, sizeof(url), "", 1),
          "empty query rejected");
    CHECK(strstr(mr_youtube_search_last_error(), "Enter") != NULL,
          "empty-query error is useful");

    if (failures)
        return 1;
    puts("YouTube search checks passed");
    return 0;
}
