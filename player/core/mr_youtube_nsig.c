/*
 * MintVID - native YouTube n-challenge solver using embedded QuickJS.
 *
 * Only QuickJS's language runtime is linked.  quickjs-libc is intentionally
 * absent, leaving evaluated code without filesystem, networking, process or
 * native AmigaOS APIs.  A memory ceiling and bytecode interrupt budget bound
 * hostile or unexpectedly expensive player scripts.
 */
#include "mr_youtube_nsig.h"

#include "quickjs.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef MR_QUICKJS_MEMORY_LIMIT_MB
#define MR_QUICKJS_MEMORY_LIMIT_MB 192UL
#endif
#define NSIG_MEMORY_LIMIT (MR_QUICKJS_MEMORY_LIMIT_MB * 1024UL * 1024UL)
#define NSIG_STACK_LIMIT  (192UL * 1024UL)
#define NSIG_INTERRUPT_BUDGET 20000UL
#define NSIG_ERROR_MAX 256

/* qjsc bytecode is byte-order specific.  The Amiga build explicitly selects
 * the big-endian objects; normal host checks use the little-endian set. */
#ifdef MR_QUICKJS_BYTECODE_BE
extern const uint32_t mint_ejs_lib_be_size;
extern const uint8_t mint_ejs_lib_be[];
extern const uint32_t mint_ejs_core_be_size;
extern const uint8_t mint_ejs_core_be[];
#define NSIG_LIB_BYTECODE mint_ejs_lib_be
#define NSIG_LIB_BYTECODE_SIZE mint_ejs_lib_be_size
#define NSIG_CORE_BYTECODE mint_ejs_core_be
#define NSIG_CORE_BYTECODE_SIZE mint_ejs_core_be_size
#else
extern const uint32_t mint_ejs_lib_le_size;
extern const uint8_t mint_ejs_lib_le[];
extern const uint32_t mint_ejs_core_le_size;
extern const uint8_t mint_ejs_core_le[];
#define NSIG_LIB_BYTECODE mint_ejs_lib_le
#define NSIG_LIB_BYTECODE_SIZE mint_ejs_lib_le_size
#define NSIG_CORE_BYTECODE mint_ejs_core_le
#define NSIG_CORE_BYTECODE_SIZE mint_ejs_core_le_size
#endif

static char g_nsig_error[NSIG_ERROR_MAX];

typedef struct nsig_budget {
    unsigned long remaining;
} nsig_budget;

static void nsig_set_error(const char *message)
{
    if (!message) message = "unknown n challenge error";
    snprintf(g_nsig_error, sizeof g_nsig_error, "%s", message);
}

static void nsig_set_prefixed_error(const char *prefix, const char *detail)
{
    if (!prefix) prefix = "n challenge";
    if (!detail) detail = "unknown error";
    snprintf(g_nsig_error, sizeof g_nsig_error, "%s: %.190s",
             prefix, detail);
}

const char *mr_youtube_nsig_last_error(void)
{
    return g_nsig_error;
}

static int nsig_interrupt(JSRuntime *runtime, void *opaque)
{
    nsig_budget *budget = (nsig_budget *)opaque;
    (void)runtime;
    if (!budget || !budget->remaining) return 1;
    budget->remaining--;
    return 0;
}

static void nsig_capture_exception(JSContext *context, const char *stage)
{
    JSValue exception = JS_GetException(context);
    const char *text = JS_ToCString(context, exception);
    if (!text && (JS_IsNull(exception) || JS_IsUndefined(exception)))
        nsig_set_prefixed_error(stage, "out of memory");
    else
        nsig_set_prefixed_error(stage, text ? text : "JavaScript exception");
    if (text) JS_FreeCString(context, text);
    JS_FreeValue(context, exception);
}

static int nsig_eval(JSContext *context, const char *source, size_t length,
                     const char *filename)
{
    JSValue value = JS_Eval(context, source, length, filename,
                            JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(value)) {
        nsig_capture_exception(context, filename);
        return 0;
    }
    JS_FreeValue(context, value);
    return 1;
}

static int nsig_eval_bytecode(JSContext *context,
                              const uint8_t *bytecode, uint32_t length,
                              const char *name)
{
    JSValue object = JS_ReadObject(context, bytecode, (size_t)length,
                                   JS_READ_OBJ_BYTECODE);
    JSValue value;
    if (JS_IsException(object)) {
        nsig_capture_exception(context, name);
        return 0;
    }
    /* JS_EvalFunction consumes the bytecode function object. */
    value = JS_EvalFunction(context, object);
    if (JS_IsException(value)) {
        nsig_capture_exception(context, name);
        return 0;
    }
    JS_FreeValue(context, value);
    return 1;
}

static int nsig_set_global_string(JSContext *context, JSValue global,
                                  const char *name,
                                  const char *value, size_t value_len)
{
    JSValue string = JS_NewStringLen(context, value, value_len);
    if (JS_IsException(string)) {
        nsig_capture_exception(context, "QuickJS string allocation");
        return 0;
    }
    if (JS_SetPropertyStr(context, global, name, string) < 0) {
        nsig_capture_exception(context, "QuickJS global assignment");
        return 0;
    }
    return 1;
}

int mr_youtube_nsig_solve(const char *player_js, size_t player_js_len,
                          const char *challenge,
                          char *out, size_t out_size)
{
    static const char expose_libraries[] =
        "globalThis.meriyah=lib.meriyah;"
        "globalThis.astring=lib.astring;";
    static const char run_solver[] =
        "globalThis.__mint_output=(function(){"
        "var r=jsc({type:'player',player:globalThis.__mint_player,"
        "output_preprocessed:false,requests:[{type:'n',"
        "challenges:[globalThis.__mint_n]}]});"
        "if(!r||r.type!=='result'||!r.responses||!r.responses[0])"
        "throw new Error('invalid EJS response');"
        "var x=r.responses[0];"
        "if(x.type!=='result')throw new Error(x.error||'EJS failed');"
        "var v=x.data[globalThis.__mint_n];"
        "if(typeof v!=='string'||!v.length)"
        "throw new Error('empty EJS n result');"
        "return v;})();";
    JSRuntime *runtime = NULL;
    JSContext *context = NULL;
    JSValue global = JS_UNDEFINED;
    JSValue result = JS_UNDEFINED;
    nsig_budget budget;
    size_t result_len = 0;
    const char *result_text = NULL;
    int ok = 0;

    g_nsig_error[0] = '\0';
    if (!player_js || !player_js_len || !challenge || !*challenge ||
        !out || out_size < 2) {
        nsig_set_error("invalid n challenge input");
        return 0;
    }
    out[0] = '\0';
    runtime = JS_NewRuntime();
    if (!runtime) {
        nsig_set_error("cannot create QuickJS runtime");
        goto done;
    }
    JS_SetMemoryLimit(runtime, NSIG_MEMORY_LIMIT);
    JS_SetMaxStackSize(runtime, NSIG_STACK_LIMIT);
    budget.remaining = NSIG_INTERRUPT_BUDGET;
    JS_SetInterruptHandler(runtime, nsig_interrupt, &budget);
    context = JS_NewContext(runtime);
    if (!context) {
        nsig_set_error("cannot create QuickJS context");
        goto done;
    }
    global = JS_GetGlobalObject(context);
    if (JS_IsException(global)) {
        nsig_capture_exception(context, "QuickJS global object");
        goto done;
    }
    if (!nsig_eval_bytecode(context,
                            NSIG_LIB_BYTECODE, NSIG_LIB_BYTECODE_SIZE,
                            "embedded yt.solver.lib bytecode") ||
        !nsig_eval(context, expose_libraries,
                   sizeof expose_libraries - 1, "MintVID EJS setup") ||
        !nsig_eval_bytecode(context,
                            NSIG_CORE_BYTECODE, NSIG_CORE_BYTECODE_SIZE,
                            "embedded yt.solver.core bytecode") ||
        !nsig_set_global_string(context, global, "__mint_player",
                                player_js, player_js_len) ||
        !nsig_set_global_string(context, global, "__mint_n",
                                challenge, strlen(challenge)) ||
        !nsig_eval(context, run_solver, sizeof run_solver - 1,
                   "MintVID n solver"))
        goto done;

    result = JS_GetPropertyStr(context, global, "__mint_output");
    if (JS_IsException(result)) {
        nsig_capture_exception(context, "QuickJS result");
        goto done;
    }
    result_text = JS_ToCStringLen(context, &result_len, result);
    if (!result_text || !result_len || result_len + 1 > out_size) {
        nsig_set_error(result_text ? "transformed n value is too long"
                                   : "cannot read transformed n value");
        goto done;
    }
    memcpy(out, result_text, result_len);
    out[result_len] = '\0';
    ok = 1;

done:
    if (result_text && context) JS_FreeCString(context, result_text);
    if (context && !JS_IsUndefined(result)) JS_FreeValue(context, result);
    if (context && !JS_IsUndefined(global)) JS_FreeValue(context, global);
    if (context) JS_FreeContext(context);
    if (runtime) JS_FreeRuntime(runtime);
    return ok;
}

static int nsig_is_unreserved(unsigned char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '-' || c == '_' ||
           c == '.' || c == '~';
}

static int nsig_hex_value(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int nsig_percent_decode(const char *start, size_t length,
                               char *out, size_t out_size)
{
    size_t i = 0, used = 0;
    while (i < length) {
        unsigned char c = (unsigned char)start[i++];
        if (c == '%' && i + 1 < length) {
            int hi = nsig_hex_value((unsigned char)start[i]);
            int lo = nsig_hex_value((unsigned char)start[i + 1]);
            if (hi < 0 || lo < 0) return 0;
            c = (unsigned char)((hi << 4) | lo);
            i += 2;
        }
        if (!c || used + 1 >= out_size) return 0;
        out[used++] = (char)c;
    }
    out[used] = '\0';
    return used != 0;
}

static int nsig_percent_encode(const char *value,
                               char *out, size_t out_size,
                               size_t *out_len)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t used = 0;
    while (*value) {
        unsigned char c = (unsigned char)*value++;
        if (nsig_is_unreserved(c)) {
            if (used + 1 >= out_size) return 0;
            out[used++] = (char)c;
        } else {
            if (used + 3 >= out_size) return 0;
            out[used++] = '%';
            out[used++] = hex[c >> 4];
            out[used++] = hex[c & 15];
        }
    }
    out[used] = '\0';
    if (out_len) *out_len = used;
    return used != 0;
}

int mr_youtube_nsig_transform_url(const char *player_js,
                                  size_t player_js_len,
                                  const char *url,
                                  char *out, size_t out_size,
                                  void *opaque)
{
    const char *start = NULL, *end = NULL, *query, *p;
    char challenge[256], solved[256], encoded[768];
    size_t prefix_len, encoded_len, suffix_len;
    (void)opaque;
    if (!url || !out || out_size < 2) {
        nsig_set_error("invalid n challenge URL");
        return 0;
    }
    p = strstr(url, "/n/");
    if (p) {
        start = p + 3;
        end = start + strcspn(start, "/?#");
    } else if ((query = strchr(url, '?')) != NULL) {
        p = query + 1;
        while (*p) {
            if (p[0] == 'n' && p[1] == '=') {
                start = p + 2;
                end = start + strcspn(start, "&#");
                break;
            }
            p = strchr(p, '&');
            if (!p) break;
            p++;
        }
    }
    if (!start || !end || end <= start ||
        !nsig_percent_decode(start, (size_t)(end - start),
                             challenge, sizeof challenge)) {
        nsig_set_error("URL has no valid n challenge");
        return 0;
    }
    if (!mr_youtube_nsig_solve(player_js, player_js_len, challenge,
                               solved, sizeof solved) ||
        !nsig_percent_encode(solved, encoded, sizeof encoded, &encoded_len))
        return 0;
    prefix_len = (size_t)(start - url);
    suffix_len = strlen(end);
    if (prefix_len + encoded_len + suffix_len + 1 > out_size) {
        nsig_set_error("transformed media URL is too long");
        return 0;
    }
    memcpy(out, url, prefix_len);
    memcpy(out + prefix_len, encoded, encoded_len);
    memcpy(out + prefix_len + encoded_len, end, suffix_len + 1);
    return 1;
}
