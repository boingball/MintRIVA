#include "mr_iptv.h"
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define READER_BUFFER 16384
#define OBJECT_MAX 65536
#define COUNTRY_CHANNEL_MAX 5000

typedef struct {
  FILE *file;
  unsigned char io[READER_BUFFER];
  size_t offset;
  int started, done;
} object_reader;

static int next_nonspace(object_reader *r) {
  int c;
  do {
    c = fgetc(r->file);
    if (c != EOF)
      r->offset++;
  } while (c != EOF && isspace((unsigned char)c));
  return c;
}

/* Return one complete object. stdio's fixed buffer deliberately makes
 * strings/escapes cross refill boundaries. */
static int next_object(object_reader *r, char *out, size_t cap, size_t *len) {
  size_t n = 0;
  int c, depth = 0, quoted = 0, escaped = 0;
  if (!r->started) {
    setvbuf(r->file, (char *)r->io, _IOFBF, sizeof(r->io));
    c = next_nonspace(r);
    if (c != '[')
      return -1;
    r->started = 1;
  }
  if (r->done)
    return 0;
  c = next_nonspace(r);
  if (c == ']') {
    if (next_nonspace(r) != EOF)
      return -1;
    r->done = 1;
    return 0;
  }
  if (c != '{')
    return -1;
  for (;;) {
    if (n + 1 >= cap)
      return -1;
    out[n++] = (char)c;
    if (quoted) {
      if (escaped)
        escaped = 0;
      else if (c == '\\')
        escaped = 1;
      else if (c == '"')
        quoted = 0;
    } else if (c == '"') {
      quoted = 1;
    } else if (c == '{' || c == '[') {
      depth++;
    } else if (c == '}' || c == ']') {
      if (--depth == 0)
        break;
    }
    c = fgetc(r->file);
    if (c == EOF)
      return -1;
    r->offset++;
  }
  out[n] = 0;
  c = next_nonspace(r);
  if (c == ']') {
    if (next_nonspace(r) != EOF)
      return -1;
    r->done = 1;
  } else if (c != ',')
    return -1;
  *len = n;
  return 1;
}

static uint32_t hash_id(const char *s) {
  uint32_t h = 2166136261u;
  while (*s) {
    h ^= (unsigned char)*s++;
    h *= 16777619u;
  }
  return h;
}

static int stream_score(const mr_iptv_stream *s) {
  int score = 0;
  if (!s->http_referrer[0] && !s->user_agent[0])
    score += 8;
  if (strstr(s->url, ".m3u8"))
    score += 4;
  else if (strstr(s->url, ".ts"))
    score += 2;
  if (!strncmp(s->url, "https://", 8))
    score++;
  return score;
}

static int keep_stream(mr_iptv_directory *d, mr_iptv_channel *c,
                       const mr_iptv_stream *s) {
  unsigned i, worst = 0;
  if (c->stream_count < 4) {
    if (!c->streams) {
      c->streams = calloc(4, sizeof(*c->streams));
      if (!c->streams)
        return 0;
      d->stream_storage_allocation_count++;
    }
    c->streams[c->stream_count++] = *s;
    return 1;
  }
  for (i = 1; i < c->stream_count; i++)
    if (stream_score(&c->streams[i]) < stream_score(&c->streams[worst]))
      worst = i;
  if (stream_score(s) > stream_score(&c->streams[worst]))
    c->streams[worst] = *s;
  return 1;
}

static int reserve_country_channels(mr_iptv_directory *d, size_t required) {
  size_t capacity;
  void *items;
  if (required <= d->channel_capacity)
    return 1;
  capacity = d->channel_capacity ? d->channel_capacity : 64;
  while (capacity < required && capacity < COUNTRY_CHANNEL_MAX)
    capacity *= 2;
  if (capacity > COUNTRY_CHANNEL_MAX)
    capacity = COUNTRY_CHANNEL_MAX;
  if (capacity < required || capacity > (size_t)-1 / sizeof(*d->channels))
    return 0;
  items = realloc(d->channels, capacity * sizeof(*d->channels));
  if (!items)
    return 0;
  d->channels = items;
  d->channel_capacity = capacity;
  d->channel_table_realloc_count++;
  return 1;
}

#if defined(AMIGA_M68K) || defined(__amigaos__) || defined(__AMIGA__)
#include <exec/memory.h>
#include <proto/exec.h>
static void report_fast_ram(const char *phase) {
  printf("IPTV: Fast RAM %s: %lu KB; largest %lu KB\n", phase,
         (unsigned long)(AvailMem(MEMF_FAST) / 1024),
         (unsigned long)(AvailMem(MEMF_FAST | MEMF_LARGEST) / 1024));
}
#else
static void report_fast_ram(const char *phase) { (void)phase; }
#endif

static int find_channel(const mr_iptv_directory *d, const uint32_t *table,
                        size_t size, const char *id) {
  char base[MR_IPTV_ID_MAX];
  const char *at = strchr(id, '@');
  size_t n = at ? (size_t)(at - id) : strlen(id), slot, guard;
  if (n >= sizeof(base))
    return -1;
  memcpy(base, id, n);
  base[n] = 0;
  slot = hash_id(base) & (size - 1);
  for (guard = 0; guard < size; guard++, slot = (slot + 1) & (size - 1)) {
    if (!table[slot])
      return -1;
    if (!strcmp(d->channels[table[slot] - 1].id, base))
      return (int)table[slot] - 1;
  }
  return -1;
}

int mr_iptv_load_country_files(mr_iptv_directory *out,
                               const char *channels_path,
                               const char *streams_path, const char *country) {
  object_reader cr = {0}, sr = {0};
  mr_iptv_directory d;
  mr_iptv_channel parsed_channel;
  mr_iptv_stream parsed_stream;
  char *object = NULL, channel[MR_IPTV_ID_MAX];
  char parse_error[256];
  uint32_t *hash = NULL;
  size_t len, hash_size = 1, i, slot, retained = 0;
  int rc, match, preserve_error = 0;
  mr_iptv_init(&d);
  report_fast_ram("before load");
  cr.file = fopen(channels_path, "rb");
  if (!cr.file)
    goto fail;
  object = malloc(OBJECT_MAX);
  if (!object)
    goto fail;
  while ((rc = next_object(&cr, object, OBJECT_MAX, &len)) > 0) {
    if (!mr_iptv_parse_channel_object_for_country(
            object, len, country, &parsed_channel, parse_error,
            sizeof(parse_error))) {
      preserve_error = 1;
      mr_iptv_set_error(parse_error);
      goto fail;
    }
    d.parsed_channel_count++;
    if (parsed_channel.id[0] && parsed_channel.name[0] &&
        !parsed_channel.is_nsfw && !parsed_channel.closed &&
        !parsed_channel.replaced &&
        (!country || !*country || !strcmp(parsed_channel.country, country))) {
      d.country_match_count++;
      d.eligible_channel_count++;
      if (d.channel_count == COUNTRY_CHANNEL_MAX) {
        d.skipped_channel_count++;
        free(parsed_channel.alt_names);
        free(parsed_channel.categories);
        continue;
      }
      if (!reserve_country_channels(&d, d.channel_count + 1)) {
        free(parsed_channel.alt_names);
        free(parsed_channel.categories);
        goto fail;
      }
      d.channels[d.channel_count++] = parsed_channel;
    } else {
      d.skipped_channel_count++;
    }
  }
  fclose(cr.file);
  cr.file = NULL;
  if (rc < 0)
    goto fail;
  printf("IPTV: parsed %lu channels\n", (unsigned long)d.parsed_channel_count);
  printf("IPTV: retained %lu channels for %s\n", (unsigned long)d.channel_count,
         country ? country : "All");
  report_fast_ram("after country channels");
  if (!d.country_match_count) {
    char message[128];
    snprintf(message, sizeof(message), "No channel records matched country %s",
             country ? country : "All");
    mr_iptv_set_error(message);
    preserve_error = 1;
    goto fail;
  }
  while (hash_size < d.channel_count * 2 + 1)
    hash_size <<= 1;
  hash = calloc(hash_size, sizeof(*hash));
  if (!hash)
    goto fail;
  for (i = 0; i < d.channel_count; i++) {
    slot = hash_id(d.channels[i].id) & (hash_size - 1);
    while (hash[slot])
      slot = (slot + 1) & (hash_size - 1);
    hash[slot] = (uint32_t)i + 1;
  }
  sr.file = fopen(streams_path, "rb");
  if (!sr.file)
    goto fail;
  while ((rc = next_object(&sr, object, OBJECT_MAX, &len)) > 0) {
    d.parsed_stream_count++;
    if (!mr_iptv_parse_stream_channel_object(
            object, len, channel, sizeof(channel), parse_error,
            sizeof(parse_error))) {
      preserve_error = 1;
      mr_iptv_set_error(parse_error);
      goto fail;
    }
    match = channel[0] ? find_channel(&d, hash, hash_size, channel) : -1;
    if (match < 0)
      continue;
    if (!mr_iptv_parse_stream_object(object, len, channel, sizeof(channel),
                                     &parsed_stream, parse_error,
                                     sizeof(parse_error))) {
      preserve_error = 1;
      mr_iptv_set_error(parse_error);
      goto fail;
    }
    if (match >= 0 && mr_iptv_valid_url(parsed_stream.url) &&
        !keep_stream(&d, &d.channels[match], &parsed_stream))
      goto fail;
    if (match >= 0 && mr_iptv_valid_url(parsed_stream.url))
      d.matched_stream_count++;
  }
  fclose(sr.file);
  sr.file = NULL;
  if (rc < 0)
    goto fail;
  for (i = 0; i < d.channel_count; i++) {
    if (!d.channels[i].stream_count) {
      free(d.channels[i].alt_names);
      free(d.channels[i].categories);
      free(d.channels[i].streams);
      d.skipped_channel_count++;
    } else {
      if (retained != i)
        d.channels[retained] = d.channels[i];
      retained++;
    }
  }
  d.channel_count = retained;
  report_fast_ram("after stream join");
  if (!d.channel_count) {
    char message[160];
    snprintf(message, sizeof(message),
             "%lu %s channels found, but none had matching streams",
             (unsigned long)d.eligible_channel_count,
             country ? country : "All");
    mr_iptv_set_error(message);
    preserve_error = 1;
    goto fail;
  }
  {
    size_t streams_kept = 0, stream_slots = 0, pool_bytes = 0;
    size_t metadata_bytes = 0;
    size_t channel_bytes, stream_bytes, hash_bytes, working_bytes;
    for (i = 0; i < d.channel_count; i++) {
      unsigned k;
      streams_kept += d.channels[i].stream_count;
      if (d.channels[i].streams)
        stream_slots += 4;
      pool_bytes += strlen(d.channels[i].id) + strlen(d.channels[i].name) +
                    strlen(d.channels[i].network) + 3;
      metadata_bytes +=
          (d.channels[i].alt_count + d.channels[i].category_count) *
          MR_IPTV_NAME_MAX;
      for (k = 0; k < d.channels[i].stream_count; k++)
        pool_bytes += strlen(d.channels[i].streams[k].url) +
                      strlen(d.channels[i].streams[k].http_referrer) +
                      strlen(d.channels[i].streams[k].user_agent) + 3;
    }
    printf("IPTV: retained %lu channels, %lu streams\n",
           (unsigned long)d.channel_count, (unsigned long)streams_kept);
    channel_bytes = d.channel_capacity * sizeof(*d.channels);
    stream_bytes = stream_slots * sizeof(mr_iptv_stream);
    hash_bytes = hash_size * sizeof(*hash);
    working_bytes = channel_bytes + stream_bytes + metadata_bytes + hash_bytes +
                    OBJECT_MAX + 2 * READER_BUFFER;
    printf("IPTV: string content %lu KB; channel table %lu KB; stream table "
           "%lu KB; hash table %lu KB; JSON buffers %u KB\n",
           (unsigned long)((pool_bytes + 1023) / 1024),
           (unsigned long)((channel_bytes + 1023) / 1024),
           (unsigned long)((stream_bytes + 1023) / 1024),
           (unsigned long)((hash_bytes + 1023) / 1024),
           (unsigned)((OBJECT_MAX + 2 * READER_BUFFER) / 1024));
    printf("IPTV: estimated directory working memory %lu KB\n",
           (unsigned long)((working_bytes + 1023) / 1024));
    printf("IPTV: channel table reallocations %lu; stream allocations %lu; "
           "global stream reallocations 0\n",
           (unsigned long)d.channel_table_realloc_count,
           (unsigned long)d.stream_storage_allocation_count);
  }
  printf("IPTV: parsed %lu streams; matched %lu streams\n",
         (unsigned long)d.parsed_stream_count,
         (unsigned long)d.matched_stream_count);
  printf("IPTV: retained %lu playable %s channels\n",
         (unsigned long)d.channel_count, country ? country : "All");
  free(hash);
  free(object);
  mr_iptv_free(out);
  *out = d;
  return 1;
fail:
  if (!preserve_error)
    mr_iptv_set_error("IPTV streaming directory parse failed");
  if (cr.file)
    fclose(cr.file);
  if (sr.file)
    fclose(sr.file);
  free(hash);
  free(object);
  mr_iptv_free(&d);
  return 0;
}
