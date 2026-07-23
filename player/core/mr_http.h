/*
 * MintRIVA - HTTP/HTTPS media source internals.
 */
#ifndef MR_HTTP_H
#define MR_HTTP_H

#if (defined(AMIGA_M68K) || defined(__amigaos__) || defined(__AMIGA__)) && \
    defined(__GNUC__)
/*
 * The classic Amiga SDK inline stubs use unsigned-char CONST_STRPTR names and
 * non-const STRPTR/APTR parameters for several read-only socket arguments.
 * GCC consequently reports pointer-sign and discarded-qualifier warnings at
 * otherwise valid OpenLibrary(), gethostbyname(), setsockopt() and send()
 * calls. Keep those SDK-only diagnostics scoped to this HTTP translation unit
 * rather than weakening the warning policy for the complete Amiga build.
 */
#pragma GCC diagnostic ignored "-Wpointer-sign"
#pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"
#endif

#include "mr_source.h"

mr_source *mr_http_source_open(const char *url);

#endif /* MR_HTTP_H */
