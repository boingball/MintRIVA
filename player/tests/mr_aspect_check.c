#include "../amiga/mr_aspect.h"

#include <stdio.h>

static int expect(int sw, int sh, int bw, int bh,
                  int x, int y, int w, int h)
{
    mr_aspect_rect r = mr_aspect_fit(sw, sh, bw, bh);
    if (r.x != x || r.y != y || r.w != w || r.h != h) {
        fprintf(stderr, "%dx%d in %dx%d: got %d,%d %dx%d; "
                        "expected %d,%d %dx%d\n",
                sw, sh, bw, bh, r.x, r.y, r.w, r.h, x, y, w, h);
        return 0;
    }
    return 1;
}

int main(void)
{
    if (!expect(640, 360, 800, 600, 0, 75, 800, 450)) return 1;
    if (!expect(1920, 1080, 640, 480, 0, 60, 640, 360)) return 1;
    if (!expect(640, 480, 1280, 720, 160, 0, 960, 720)) return 1;
    if (!expect(640, 360, 640, 360, 0, 0, 640, 360)) return 1;
    puts("RTG aspect-fit geometry checks passed");
    return 0;
}
