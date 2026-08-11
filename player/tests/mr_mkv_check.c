#include "../core/mr_mkv.h"

#include <stdio.h>
#include <stdlib.h>

/* Memory-backed tests do not call this, but mr_mkv.c retains one portable
 * source-backed code path and therefore references the symbol at link time. */
int mr_source_read_at(mr_source *source, size_t offset, void *dst, size_t len)
{
    (void)source; (void)offset; (void)dst; (void)len; return 0;
}

int main(int argc, char **argv)
{
    FILE *f;
    long n;
    uint8_t *data;
    mr_mkv m;
    mr_status st;
    unsigned video = 0, audio = 0;
    mr_packet pkt;
    if (argc != 2) return 2;
    f = fopen(argv[1], "rb");
    if (!f) return 2;
    fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
    data = (uint8_t *)malloc((size_t)n);
    if (!data || fread(data, 1, (size_t)n, f) != (size_t)n) return 2;
    fclose(f);
    st = mr_mkv_open(&m, data, (size_t)n);
    if (st != MR_OK) {
        fprintf(stderr, "MKV open failed: %d (video=%d, %dx%d, track=%lu, "
                        "config=%lu, cluster=%lu)\n",
                st, m.video.valid, m.video.width, m.video.height,
                (unsigned long)m.video_track,
                (unsigned long)m.video_config_len,
                (unsigned long)m.first_cluster);
        mr_mkv_close(&m); free(data); return 1;
    }
    while (mr_mkv_next_packet(&m, &pkt) == MR_OK)
        if (pkt.is_video) video++; else audio++;
    printf("MKV: %dx%d, video packets=%u, audio packets=%u\n",
           m.video.width, m.video.height, video, audio);
    mr_mkv_close(&m); free(data);
    return video ? 0 : 1;
}
