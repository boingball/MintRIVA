#ifndef MINTVID_VERSION_H
#define MINTVID_VERSION_H

/* Human-facing semantic version and AmigaOS Version-command identity. Amiga
 * $VER strings conventionally use version.revision rather than three-part
 * semantic versions, so 1.1.0 is represented there as 1.1. */
#define MINTVID_VERSION       "1.1.0"
#define MINTVID_AMIGA_VERSION "1.1"
#define MINTVID_VERSION_DATE  "2.9.2026"

#if defined(__GNUC__)
#define MINTVID_VERSION_USED __attribute__((used))
#else
#define MINTVID_VERSION_USED
#endif

#define MINTVID_DECLARE_VERSION(symbol, program) \
    const char symbol[] MINTVID_VERSION_USED = \
        "\0$VER: " program " " MINTVID_AMIGA_VERSION \
        " (" MINTVID_VERSION_DATE ")"

#endif
