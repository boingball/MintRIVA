/* MintRIVA - bounded LOAS/LATM AAC framing parser. */
#ifndef MR_LATM_H
#define MR_LATM_H

#include "mr_types.h"

typedef struct {
    unsigned object_type;
    unsigned sample_rate;
    unsigned channels;
    uint8_t asc[4];
    uint8_t asc_len;
    int valid;
} mr_latm_config;

/* Parse one AudioMuxElement (the bytes after a three-byte LOAS header).
 * Returns the raw AAC payload location in bits because LATM need not align it
 * to a byte boundary.  The common broadcast layout (one program/layer and
 * frameLengthType 0) is deliberately supported; exotic multiplexes reject. */
int mr_latm_payload(const uint8_t *data, size_t len, mr_latm_config *config,
                    size_t *payload_bit, size_t *payload_len);

/* Copy an arbitrary bit-aligned LATM payload into ordinary byte-aligned AAC. */
int mr_latm_copy_payload(const uint8_t *data, size_t len, size_t payload_bit,
                         uint8_t *out, size_t payload_len);

#endif
