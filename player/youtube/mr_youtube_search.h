#ifndef MR_YOUTUBE_SEARCH_H
#define MR_YOUTUBE_SEARCH_H

#include <stddef.h>

#define MR_YOUTUBE_SEARCH_MAX_RESULTS 100
#define MR_YOUTUBE_SEARCH_TITLE_MAX 192
#define MR_YOUTUBE_SEARCH_CHANNEL_MAX 128
#define MR_YOUTUBE_SEARCH_ROW_MAX 352

typedef struct mr_youtube_search_result {
    char video_id[12];
    char title[MR_YOUTUBE_SEARCH_TITLE_MAX];
    char channel[MR_YOUTUBE_SEARCH_CHANNEL_MAX];
    char row[MR_YOUTUBE_SEARCH_ROW_MAX];
    int live;
} mr_youtube_search_result;

typedef struct mr_youtube_search_results {
    mr_youtube_search_result *items;
    size_t count;
} mr_youtube_search_results;

void mr_youtube_search_results_init(mr_youtube_search_results *results);
void mr_youtube_search_results_free(mr_youtube_search_results *results);

/* Build the public YouTube search-page URL. No API key or account is needed. */
int mr_youtube_search_build_url(char *output, size_t output_size,
                                const char *query, int live_only);

/* Parse videoRenderer objects from YouTube's embedded search-page JSON. */
int mr_youtube_search_parse(mr_youtube_search_results *results,
                            const char *document, size_t document_size,
                            int live_only);

int mr_youtube_search_watch_url(char *output, size_t output_size,
                                const mr_youtube_search_result *result);

const char *mr_youtube_search_last_error(void);

#endif
