/* Standalone MintRIVA IPTV ReAction browser.
 *
 * Keeping this in a second process gives directory parsing its own event loop;
 * selected URLs are still played by the normal mrplay executable.
 */
#include "../core/mr_source.h"
#include "../iptv/mr_iptv.h"

#include <classes/window.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <exec/libraries.h>
#include <exec/types.h>
#include <gadgets/button.h>
#include <gadgets/chooser.h>
#include <gadgets/layout.h>
#include <gadgets/listbrowser.h>
#include <gadgets/string.h>
#include <images/label.h>
#include <intuition/intuition.h>
#include <proto/button.h>
#include <proto/chooser.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/label.h>
#include <proto/layout.h>
#include <proto/listbrowser.h>
#include <proto/string.h>
#include <proto/utility.h>
#include <proto/window.h>
#include <reaction/reaction.h>
#include <reaction/reaction_macros.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define IPTV_CLASS_VERSION 44
#define IPTV_FILE_MAX (32UL * 1024UL * 1024UL)
#define MRPLAY_STACK_SIZE 320000UL
#define IPTV_CACHE_SECONDS (24UL * 60UL * 60UL)
#define IPTV_CHANNELS_URL "https://iptv-org.github.io/api/channels.json"
#define IPTV_STREAMS_URL "https://iptv-org.github.io/api/streams.json"

struct IntuitionBase *IntuitionBase;
struct Library *UtilityBase, *WindowBase, *LayoutBase, *ButtonBase;
struct Library *ChooserBase, *ListBrowserBase, *StringBase, *LabelBase;

enum {
  G_SEARCH = 1,
  G_COUNTRY,
  G_CATEGORY,
  G_CHANNELS,
  G_REFRESH,
  G_PLAY,
  G_OPEN_URL,
  G_CLOSE
};

static int open_classes(void) {
  IntuitionBase = (struct IntuitionBase *)OpenLibrary(
      (CONST_STRPTR) "intuition.library", 39);
  UtilityBase = OpenLibrary((CONST_STRPTR) "utility.library", 39);
  WindowBase = OpenLibrary((CONST_STRPTR) "window.class", IPTV_CLASS_VERSION);
  LayoutBase =
      OpenLibrary((CONST_STRPTR) "gadgets/layout.gadget", IPTV_CLASS_VERSION);
  ButtonBase =
      OpenLibrary((CONST_STRPTR) "gadgets/button.gadget", IPTV_CLASS_VERSION);
  ChooserBase =
      OpenLibrary((CONST_STRPTR) "gadgets/chooser.gadget", IPTV_CLASS_VERSION);
  ListBrowserBase = OpenLibrary((CONST_STRPTR) "gadgets/listbrowser.gadget",
                                IPTV_CLASS_VERSION);
  StringBase =
      OpenLibrary((CONST_STRPTR) "gadgets/string.gadget", IPTV_CLASS_VERSION);
  LabelBase =
      OpenLibrary((CONST_STRPTR) "images/label.image", IPTV_CLASS_VERSION);
  return IntuitionBase && UtilityBase && WindowBase && LayoutBase &&
         ButtonBase && ChooserBase && ListBrowserBase && StringBase &&
         LabelBase;
}

static void close_classes(void) {
#define CLOSE_BASE(base)                                                       \
  do {                                                                         \
    if (base) {                                                                \
      CloseLibrary((struct Library *)base);                                    \
      base = NULL;                                                             \
    }                                                                          \
  } while (0)
  CLOSE_BASE(LabelBase);
  CLOSE_BASE(StringBase);
  CLOSE_BASE(ListBrowserBase);
  CLOSE_BASE(ChooserBase);
  CLOSE_BASE(ButtonBase);
  CLOSE_BASE(LayoutBase);
  CLOSE_BASE(WindowBase);
  CLOSE_BASE(UtilityBase);
  CLOSE_BASE(IntuitionBase);
#undef CLOSE_BASE
}

static int add_chooser(struct List *list, const char *text) {
  struct Node *node = AllocChooserNode(CNA_Text, (ULONG)text, TAG_END);
  if (!node)
    return 0;
  AddTail(list, node);
  return 1;
}

static void free_chooser(struct List *list) {
  struct Node *node;
  while ((node = RemHead(list)) != NULL)
    FreeChooserNode(node);
}

static char *read_file(const char *path, size_t *length) {
  FILE *file;
  long size;
  char *data;
  *length = 0;
  file = fopen(path, "rb");
  if (!file)
    return NULL;
  if (fseek(file, 0, SEEK_END) || (size = ftell(file)) < 0 ||
      (unsigned long)size > IPTV_FILE_MAX || fseek(file, 0, SEEK_SET)) {
    fclose(file);
    return NULL;
  }
  data = malloc((size_t)size + 1);
  if (!data || fread(data, 1, (size_t)size, file) != (size_t)size) {
    free(data);
    fclose(file);
    return NULL;
  }
  fclose(file);
  data[size] = 0;
  *length = (size_t)size;
  return data;
}

static void ensure_cache_drawers(void) {
  BPTR lock;
  lock = CreateDir((CONST_STRPTR) "PROGDIR:Cache");
  if (lock)
    UnLock(lock);
  lock = CreateDir((CONST_STRPTR) "PROGDIR:Cache/IPTV");
  if (lock)
    UnLock(lock);
}

static int cache_is_fresh(void) {
  FILE *meta = fopen("PROGDIR:Cache/IPTV/cache.meta", "r");
  unsigned long saved;
  time_t now;
  if (!meta)
    return 0;
  if (fscanf(meta, "%lu", &saved) != 1) {
    fclose(meta);
    return 0;
  }
  fclose(meta);
  now = time(NULL);
  return now != (time_t)-1 && (unsigned long)now >= saved &&
         (unsigned long)now - saved < IPTV_CACHE_SECONDS;
}

static int download_file(const char *url, const char *path) {
  mr_source *source;
  FILE *file;
  unsigned char buffer[16384];
  size_t length, offset, count;
  source = mr_http_source_open(url);
  if (!source)
    return 0;
  length = mr_source_length(source);
  if (!length || length == MR_SOURCE_LEN_UNKNOWN || length > IPTV_FILE_MAX) {
    mr_source_close(source);
    return 0;
  }
  file = fopen(path, "wb");
  if (!file) {
    mr_source_close(source);
    return 0;
  }
  for (offset = 0; offset < length; offset += count) {
    count = length - offset;
    if (count > sizeof(buffer))
      count = sizeof(buffer);
    if (!mr_source_read_at(source, offset, buffer, count) ||
        fwrite(buffer, 1, count, file) != count) {
      fclose(file);
      remove(path);
      mr_source_close(source);
      return 0;
    }
  }
  if (fclose(file) != 0) {
    remove(path);
    mr_source_close(source);
    return 0;
  }
  mr_source_close(source);
  return 1;
}

static int refresh_cache(void) {
  const char *channels_tmp = "PROGDIR:Cache/IPTV/channels.json.tmp";
  const char *streams_tmp = "PROGDIR:Cache/IPTV/streams.json.tmp";
  mr_iptv_directory check;
  char *channels = NULL, *streams = NULL;
  size_t channels_size, streams_size;
  FILE *meta;
  time_t now;
  int valid = 0;

  ensure_cache_drawers();
  remove(channels_tmp);
  remove(streams_tmp);
  if (!download_file(IPTV_CHANNELS_URL, channels_tmp) ||
      !download_file(IPTV_STREAMS_URL, streams_tmp))
    goto done;
  channels = read_file(channels_tmp, &channels_size);
  streams = read_file(streams_tmp, &streams_size);
  mr_iptv_init(&check);
  if (!channels || !streams ||
      !mr_iptv_parse_channels(&check, channels, channels_size) ||
      !mr_iptv_join_streams(&check, streams, streams_size) ||
      !check.channel_count)
    goto parsed;

  /* Temporary files are fully downloaded and parsed before the old valid
   * cache is touched. */
  remove("PROGDIR:Cache/IPTV/channels.json.old");
  remove("PROGDIR:Cache/IPTV/streams.json.old");
  rename("PROGDIR:Cache/IPTV/channels.json",
         "PROGDIR:Cache/IPTV/channels.json.old");
  rename("PROGDIR:Cache/IPTV/streams.json",
         "PROGDIR:Cache/IPTV/streams.json.old");
  if (rename(channels_tmp, "PROGDIR:Cache/IPTV/channels.json") != 0 ||
      rename(streams_tmp, "PROGDIR:Cache/IPTV/streams.json") != 0) {
    remove("PROGDIR:Cache/IPTV/channels.json");
    remove("PROGDIR:Cache/IPTV/streams.json");
    rename("PROGDIR:Cache/IPTV/channels.json.old",
           "PROGDIR:Cache/IPTV/channels.json");
    rename("PROGDIR:Cache/IPTV/streams.json.old",
           "PROGDIR:Cache/IPTV/streams.json");
    goto parsed;
  }
  remove("PROGDIR:Cache/IPTV/channels.json.old");
  remove("PROGDIR:Cache/IPTV/streams.json.old");
  now = time(NULL);
  meta = fopen("PROGDIR:Cache/IPTV/cache.meta", "w");
  if (meta) {
    fprintf(meta, "%lu\n", (unsigned long)now);
    fclose(meta);
  }
  valid = 1;
parsed:
  mr_iptv_free(&check);
done:
  free(channels);
  free(streams);
  remove(channels_tmp);
  remove(streams_tmp);
  return valid;
}

static int load_cache(mr_iptv_directory *directory) {
  char *channels, *streams;
  size_t channels_size, streams_size;
  channels = read_file("PROGDIR:Cache/IPTV/channels.json", &channels_size);
  streams = read_file("PROGDIR:Cache/IPTV/streams.json", &streams_size);
  if (!channels || !streams ||
      !mr_iptv_parse_channels(directory, channels, channels_size) ||
      !mr_iptv_join_streams(directory, streams, streams_size)) {
    free(channels);
    free(streams);
    return 0;
  }
  free(channels);
  free(streams);
  return 1;
}

static void free_nodes(struct List *list) {
  struct Node *node;
  while ((node = RemHead(list)) != NULL)
    FreeListBrowserNode(node);
}

static size_t rebuild_nodes(struct List *list, mr_iptv_directory *directory,
                            const char *search, ULONG country, ULONG category) {
  mr_iptv_filter filter;
  size_t i, shown = 0;
  filter.country = country == 0 ? "GB" : "All";
  filter.category = category == 1   ? "news"
                    : category == 2 ? "entertainment"
                                    : "All";
  filter.search = search;
  free_nodes(list);
  for (i = 0; i < directory->channel_count; i++) {
    struct Node *node;
    if (!mr_iptv_channel_visible(&directory->channels[i], &filter))
      continue;
    node = AllocListBrowserNode(
        1, LBNA_UserData, (ULONG)&directory->channels[i], LBNA_Column, 0,
        LBNCA_Text, (ULONG)directory->channels[i].name, TAG_END);
    if (!node)
      break;
    AddTail(list, node);
    shown++;
  }
  return shown;
}

static void set_status(Object *status, struct Window *window,
                       const char *text) {
  SetGadgetAttrs((struct Gadget *)status, window, NULL, STRINGA_TextVal,
                 (ULONG)text, TAG_DONE);
}

static void quote_arg(char *dst, size_t cap, const char *src) {
  size_t n = 0;
  if (cap)
    dst[n++] = '"';
  while (*src && n + 3 < cap) {
    if (*src == '"' || *src == '*')
      dst[n++] = '*';
    dst[n++] = *src++;
  }
  if (n + 1 < cap)
    dst[n++] = '"';
  dst[n] = 0;
}

static int start_url(const char *url) {
  char quoted[MR_IPTV_URL_MAX * 2 + 4], args[MR_IPTV_URL_MAX * 2 + 8];
  BPTR seglist;
  if (!mr_iptv_valid_url(url))
    return 0;
  quote_arg(quoted, sizeof(quoted), url);
  snprintf(args, sizeof(args), "%s\n", quoted);
  seglist = LoadSeg((CONST_STRPTR) "PROGDIR:mrplay");
  if (!seglist)
    seglist = LoadSeg((CONST_STRPTR) "mrplay");
  if (!seglist)
    return 0;
  if (!CreateNewProcTags(
          NP_Seglist, seglist, NP_FreeSeglist, TRUE, NP_Arguments, (ULONG)args,
          NP_StackSize, MRPLAY_STACK_SIZE, NP_Cli, TRUE, NP_CommandName,
          (ULONG) "mrplay", NP_Name, (ULONG) "MintRIVA player", TAG_END)) {
    UnLoadSeg(seglist);
    return 0;
  }
  return 1;
}

int main(void) {
  Object *winobj = NULL, *layout = NULL, *search = NULL, *country = NULL;
  Object *category = NULL, *channels = NULL, *status = NULL, *url = NULL;
  Object *buttons = NULL;
  Object *refresh_button = NULL, *play_button = NULL, *open_button = NULL;
  Object *close_button = NULL;
  Object *search_label = NULL, *country_label = NULL, *category_label = NULL;
  Object *url_label = NULL;
  struct Window *window = NULL;
  struct List channel_nodes, countries, categories;
  mr_iptv_directory directory;
  ULONG sigmask, signals, result, selected, country_index, category_index;
  UWORD code;
  char status_text[128];
  int rc = RETURN_FAIL, loaded, refresh_attempted, refreshed;

  channel_nodes.lh_Head = (struct Node *)&channel_nodes.lh_Tail;
  channel_nodes.lh_Tail = NULL;
  channel_nodes.lh_TailPred = (struct Node *)&channel_nodes.lh_Head;
  countries.lh_Head = (struct Node *)&countries.lh_Tail;
  countries.lh_Tail = NULL;
  countries.lh_TailPred = (struct Node *)&countries.lh_Head;
  categories.lh_Head = (struct Node *)&categories.lh_Tail;
  categories.lh_Tail = NULL;
  categories.lh_TailPred = (struct Node *)&categories.lh_Head;
  mr_iptv_init(&directory);
  if (!open_classes())
    goto cleanup;
  if (!add_chooser(&countries, "United Kingdom") ||
      !add_chooser(&countries, "All") || !add_chooser(&categories, "All") ||
      !add_chooser(&categories, "News") ||
      !add_chooser(&categories, "Entertainment"))
    goto cleanup;

  refresh_attempted = !cache_is_fresh();
  refreshed = refresh_attempted ? refresh_cache() : 0;
  loaded = load_cache(&directory);
  if (loaded)
    rebuild_nodes(&channel_nodes, &directory, "", 0, 0);

  search = (Object *)NewObject(STRING_GetClass(), NULL, GA_ID, G_SEARCH,
                               GA_RelVerify, TRUE, STRINGA_MaxChars,
                               MR_IPTV_NAME_MAX, TAG_DONE);
  country = (Object *)NewObject(
      CHOOSER_GetClass(), NULL, GA_ID, G_COUNTRY, GA_RelVerify, TRUE,
      CHOOSER_Labels, (ULONG)&countries, CHOOSER_Selected, 0, TAG_DONE);
  category = (Object *)NewObject(
      CHOOSER_GetClass(), NULL, GA_ID, G_CATEGORY, GA_RelVerify, TRUE,
      CHOOSER_Labels, (ULONG)&categories, CHOOSER_Selected, 0, TAG_DONE);
  channels = (Object *)NewObject(
      LISTBROWSER_GetClass(), NULL, GA_ID, G_CHANNELS, GA_RelVerify, TRUE,
      LISTBROWSER_Labels, (ULONG)&channel_nodes, LISTBROWSER_AutoFit, TRUE,
      LISTBROWSER_ShowSelected, TRUE, TAG_DONE);
  url = (Object *)NewObject(STRING_GetClass(), NULL, STRINGA_MaxChars,
                            MR_IPTV_URL_MAX, TAG_DONE);
  if (loaded)
    snprintf(status_text, sizeof(status_text),
             refresh_attempted && !refreshed
                 ? "%lu channels loaded; refresh failed, retained cache"
                 : "%lu channels loaded from cache",
             (unsigned long)directory.channel_count);
  else
    strcpy(status_text, "No IPTV cache. Copy iptv-org JSON to Cache/IPTV.");
  status = (Object *)NewObject(STRING_GetClass(), NULL, GA_ReadOnly, TRUE,
                               STRINGA_TextVal, (ULONG)status_text,
                               STRINGA_MaxChars, sizeof(status_text), TAG_DONE);
  if (!search || !country || !category || !channels || !url || !status)
    goto cleanup;

  refresh_button = (Object *)NewObject(
      BUTTON_GetClass(), NULL, GA_ID, G_REFRESH, GA_Text,
      (ULONG) "Refresh cache", GA_RelVerify, TRUE, TAG_DONE);
  play_button = (Object *)NewObject(
      BUTTON_GetClass(), NULL, GA_ID, G_PLAY, GA_Text, (ULONG) "Play",
      GA_RelVerify, TRUE, GA_Disabled, loaded ? FALSE : TRUE, TAG_DONE);
  open_button =
      (Object *)NewObject(BUTTON_GetClass(), NULL, GA_ID, G_OPEN_URL, GA_Text,
                          (ULONG) "Open URL...", GA_RelVerify, TRUE, TAG_DONE);
  close_button =
      (Object *)NewObject(BUTTON_GetClass(), NULL, GA_ID, G_CLOSE, GA_Text,
                          (ULONG) "Close", GA_RelVerify, TRUE, TAG_DONE);
  search_label = (Object *)NewObject(LABEL_GetClass(), NULL, LABEL_Text,
                                     (ULONG) "Search", TAG_DONE);
  country_label = (Object *)NewObject(LABEL_GetClass(), NULL, LABEL_Text,
                                      (ULONG) "Country", TAG_DONE);
  category_label = (Object *)NewObject(LABEL_GetClass(), NULL, LABEL_Text,
                                       (ULONG) "Category", TAG_DONE);
  url_label = (Object *)NewObject(LABEL_GetClass(), NULL, LABEL_Text,
                                  (ULONG) "Manual URL", TAG_DONE);
  if (!refresh_button || !play_button || !open_button || !close_button ||
      !search_label || !country_label || !category_label || !url_label)
    goto cleanup;

  buttons = (Object *)NewObject(
      LAYOUT_GetClass(), NULL, LAYOUT_Orientation, LAYOUT_ORIENT_HORIZ,
      LAYOUT_EvenSize, TRUE, LAYOUT_AddChild, (ULONG)refresh_button,
      LAYOUT_AddChild, (ULONG)play_button, LAYOUT_AddChild, (ULONG)open_button,
      LAYOUT_AddChild, (ULONG)close_button, TAG_DONE);
  if (!buttons)
    goto cleanup;
  layout = (Object *)NewObject(
      LAYOUT_GetClass(), NULL, LAYOUT_Orientation, LAYOUT_ORIENT_VERT,
      LAYOUT_SpaceOuter, TRUE, LAYOUT_SpaceInner, TRUE, LAYOUT_AddChild,
      (ULONG)search, CHILD_Label, (ULONG)search_label, LAYOUT_AddChild,
      (ULONG)country, CHILD_Label, (ULONG)country_label, LAYOUT_AddChild,
      (ULONG)category, CHILD_Label, (ULONG)category_label, LAYOUT_AddChild,
      (ULONG)channels, LAYOUT_AddChild, (ULONG)url, CHILD_Label,
      (ULONG)url_label, LAYOUT_AddChild, (ULONG)buttons, CHILD_WeightedHeight,
      0, LAYOUT_AddChild, (ULONG)status, CHILD_WeightedHeight, 0, TAG_DONE);
  if (!layout)
    goto cleanup;
  winobj = (Object *)NewObject(
      WINDOW_GetClass(), NULL, WA_Title, (ULONG) "MintRIVA IPTV", WA_Activate,
      TRUE, WA_DepthGadget, TRUE, WA_DragBar, TRUE, WA_CloseGadget, TRUE,
      WA_SizeGadget, TRUE, WA_IDCMP,
      IDCMP_GADGETUP | IDCMP_CLOSEWINDOW | IDCMP_IDCMPUPDATE, WINDOW_Position,
      WPOS_CENTERSCREEN, WINDOW_ParentGroup, (ULONG)layout, TAG_DONE);
  if (!winobj)
    goto cleanup;
  window = (struct Window *)RA_OpenWindow(winobj);
  if (!window)
    goto cleanup;
  GetAttr(WINDOW_SigMask, winobj, &sigmask);
  for (;;) {
    signals = Wait(sigmask | SIGBREAKF_CTRL_C);
    if (signals & SIGBREAKF_CTRL_C)
      break;
    while ((result = RA_HandleInput(winobj, &code)) != WMHI_LASTMSG) {
      STRPTR text = NULL;
      struct Node *node = NULL;
      mr_iptv_channel *channel = NULL;
      if ((result & WMHI_CLASSMASK) == WMHI_CLOSEWINDOW)
        goto done;
      if ((result & WMHI_CLASSMASK) != WMHI_GADGETUP)
        continue;
      if ((result & WMHI_GADGETMASK) == G_CLOSE)
        goto done;
      if ((result & WMHI_GADGETMASK) == G_SEARCH ||
          (result & WMHI_GADGETMASK) == G_COUNTRY ||
          (result & WMHI_GADGETMASK) == G_CATEGORY) {
        GetAttr(STRINGA_TextVal, search, (ULONG *)&text);
        GetAttr(CHOOSER_Selected, country, &country_index);
        GetAttr(CHOOSER_Selected, category, &category_index);
        SetGadgetAttrs((struct Gadget *)channels, window, NULL,
                       LISTBROWSER_Labels, ~0UL, TAG_DONE);
        selected =
            rebuild_nodes(&channel_nodes, &directory, text ? (char *)text : "",
                          country_index, category_index);
        SetGadgetAttrs((struct Gadget *)channels, window, NULL,
                       LISTBROWSER_Labels, (ULONG)&channel_nodes, TAG_DONE);
        snprintf(status_text, sizeof(status_text), "%lu channels shown",
                 (unsigned long)selected);
        set_status(status, window, status_text);
      } else if ((result & WMHI_GADGETMASK) == G_REFRESH) {
        STRPTR search_text = NULL;
        set_status(status, window, "Downloading channel directory...");
        SetGadgetAttrs((struct Gadget *)play_button, window, NULL, GA_Disabled,
                       TRUE, TAG_DONE);
        if (!refresh_cache()) {
          set_status(status, window,
                     "IPTV refresh failed; retaining cached directory");
          SetGadgetAttrs((struct Gadget *)play_button, window, NULL,
                         GA_Disabled, loaded ? FALSE : TRUE, TAG_DONE);
        } else {
          GetAttr(STRINGA_TextVal, search, (ULONG *)&search_text);
          GetAttr(CHOOSER_Selected, country, &country_index);
          GetAttr(CHOOSER_Selected, category, &category_index);
          SetGadgetAttrs((struct Gadget *)channels, window, NULL,
                         LISTBROWSER_Labels, ~0UL, TAG_DONE);
          free_nodes(&channel_nodes);
          mr_iptv_free(&directory);
          loaded = load_cache(&directory);
          selected = rebuild_nodes(&channel_nodes, &directory,
                                   search_text ? (char *)search_text : "",
                                   country_index, category_index);
          SetGadgetAttrs((struct Gadget *)channels, window, NULL,
                         LISTBROWSER_Labels, (ULONG)&channel_nodes, TAG_DONE);
          SetGadgetAttrs((struct Gadget *)play_button, window, NULL,
                         GA_Disabled, loaded ? FALSE : TRUE, TAG_DONE);
          snprintf(status_text, sizeof(status_text), "%lu channels shown",
                   (unsigned long)selected);
          set_status(status, window, status_text);
        }
      } else if ((result & WMHI_GADGETMASK) == G_OPEN_URL) {
        GetAttr(STRINGA_TextVal, url, (ULONG *)&text);
        if (!start_url(text ? (char *)text : ""))
          set_status(status, window, "Invalid URL or mrplay could not start.");
      } else if ((result & WMHI_GADGETMASK) == G_PLAY) {
        GetAttr(LISTBROWSER_SelectedNode, channels, (ULONG *)&node);
        if (node)
          GetListBrowserNodeAttrs(node, LBNA_UserData, (ULONG)&channel,
                                  TAG_DONE);
        if (!channel)
          set_status(status, window, "Select a channel first.");
        else if (channel->streams[0].http_referrer[0] ||
                 channel->streams[0].user_agent[0])
          set_status(status, window,
                     "This channel requires HTTP headers not yet supported");
        else if (!start_url(channel->streams[0].url))
          set_status(status, window, "mrplay could not start this stream.");
      }
    }
  }
done:
  rc = RETURN_OK;
cleanup:
  if (winobj) {
    if (window)
      RA_CloseWindow(winobj);
    DisposeObject(winobj);
  } else if (layout) {
    DisposeObject(layout);
  } else {
    if (buttons) {
      DisposeObject(buttons);
    } else {
      if (refresh_button)
        DisposeObject(refresh_button);
      if (play_button)
        DisposeObject(play_button);
      if (open_button)
        DisposeObject(open_button);
      if (close_button)
        DisposeObject(close_button);
    }
    if (search)
      DisposeObject(search);
    if (country)
      DisposeObject(country);
    if (category)
      DisposeObject(category);
    if (channels)
      DisposeObject(channels);
    if (url)
      DisposeObject(url);
    if (status)
      DisposeObject(status);
    if (search_label)
      DisposeObject(search_label);
    if (country_label)
      DisposeObject(country_label);
    if (category_label)
      DisposeObject(category_label);
    if (url_label)
      DisposeObject(url_label);
  }
  free_nodes(&channel_nodes);
  free_chooser(&countries);
  free_chooser(&categories);
  mr_iptv_free(&directory);
  close_classes();
  return rc;
}
