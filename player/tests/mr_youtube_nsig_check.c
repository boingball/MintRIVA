#include "../core/mr_youtube_nsig.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

static void expect(int condition, const char *name)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s (%s)\n", name,
                mr_youtube_nsig_last_error());
        failures++;
    }
}

/* This deliberately has the outer shape and marker used by current YouTube
 * players, while keeping the transformation deterministic for an offline
 * test. EJS must parse, extract, regenerate and execute the function; this is
 * not merely a test that QuickJS can evaluate 1+1. */
static const char synthetic_player[] =
    "(function(){"
    "function U(){this.v={};}"
    "U.prototype.set=function(k,v){this.v[k]=v};"
    "U.prototype.get=function(k){return this.v[k]};"
    "U.prototype.clone=function(){};"
    "U.prototype.flip=function(){"
      "this.v.n=this.v.n.split('').reverse().join('')};"
    "function solve(url,key,sig){"
      "var u=new U();u.set(key,sig);return u;Q.alr('alr','yes');}"
    "}).call(this);";

static char *read_player(const char *path, size_t *length)
{
    FILE *file = fopen(path, "rb");
    char *data;
    long size;
    if (!file || fseek(file, 0, SEEK_END) ||
        (size = ftell(file)) <= 0 || fseek(file, 0, SEEK_SET)) {
        if (file) fclose(file);
        return NULL;
    }
    data = (char *)malloc((size_t)size + 1);
    if (!data || fread(data, 1, (size_t)size, file) != (size_t)size) {
        free(data);
        fclose(file);
        return NULL;
    }
    fclose(file);
    data[size] = '\0';
    *length = (size_t)size;
    return data;
}

int main(int argc, char **argv)
{
    char out[1024];
    if (argc == 4) {
        size_t player_len = 0;
        char *player = read_player(argv[1], &player_len);
        if (!player) {
            fprintf(stderr, "cannot read player: %s\n", argv[1]);
            return 2;
        }
        if (!mr_youtube_nsig_solve(player, player_len, argv[2],
                                   out, sizeof out)) {
            fprintf(stderr, "real player solve failed: %s\n",
                    mr_youtube_nsig_last_error());
            free(player);
            return 1;
        }
        free(player);
        if (strcmp(out, argv[3])) {
            fprintf(stderr, "real player mismatch: got %s, expected %s\n",
                    out, argv[3]);
            return 1;
        }
        puts("YouTube real-player n solver check passed");
        return 0;
    }
    if (argc != 1) {
        fprintf(stderr, "usage: %s [player.js challenge expected]\n", argv[0]);
        return 2;
    }

    expect(mr_youtube_nsig_solve(synthetic_player,
                                 sizeof synthetic_player - 1,
                                 "abcDEF_123", out, sizeof out) &&
           !strcmp(out, "321_FEDcba"),
           "EJS extracts and executes synthetic n solver");

    expect(mr_youtube_nsig_transform_url(
               synthetic_player, sizeof synthetic_player - 1,
               "https://manifest.googlevideo.com/api/manifest/"
               "hls_variant/n/abcDEF_123/file/index.m3u8",
               out, sizeof out, NULL) &&
           !strcmp(out,
                   "https://manifest.googlevideo.com/api/manifest/"
                   "hls_variant/n/321_FEDcba/file/index.m3u8"),
           "HLS path n component transformed");

    expect(mr_youtube_nsig_transform_url(
               synthetic_player, sizeof synthetic_player - 1,
               "https://r1.googlevideo.com/videoplayback?x=1&n=abcDEF_123&y=2",
               out, sizeof out, NULL) &&
           !strcmp(out,
                   "https://r1.googlevideo.com/videoplayback?x=1&n=321_FEDcba&y=2"),
           "media query n parameter transformed");

    expect(!mr_youtube_nsig_transform_url(
               synthetic_player, sizeof synthetic_player - 1,
               "https://r1.googlevideo.com/videoplayback?x=1",
               out, sizeof out, NULL),
           "URL without n challenge rejected");

    if (failures) return 1;
    puts("YouTube native QuickJS n solver checks passed");
    return 0;
}
