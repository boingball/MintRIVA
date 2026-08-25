/*
 * QuickJS Javascript Engine
 *
 * Copyright (c) 2017-2025 Fabrice Bellard
 * Copyright (c) 2017-2025 Charlie Gordon
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#ifndef MR_QUICKJS_PORTABLE_TIME
#include <sys/time.h>
#endif
#include <math.h>
#if defined(__APPLE__)
#include <malloc/malloc.h>
#elif defined(__linux__) || defined(__GLIBC__)
#include <malloc.h>
#elif defined(__FreeBSD__)
#include <malloc_np.h>
#endif

#include "cutils.h"
#include "list.h"
#include "quickjs.h"
#include "libregexp.h"
#include "libunicode.h"
#include "dtoa.h"

#ifndef CONFIG_VERSION
#define CONFIG_VERSION "2026-06-04"
#endif

#ifdef MR_QUICKJS_FMOD_COMPAT
double mr_quickjs_fmod(double x, double y);
#define fmod mr_quickjs_fmod
#endif

#ifdef MR_QUICKJS_PORTABLE_LIBC
#ifndef alloca
#define alloca(size) __builtin_alloca(size)
#endif
#endif

#define OPTIMIZE         1
#define SHORT_OPCODES    1
#if defined(__EMSCRIPTEN__)
#define DIRECT_DISPATCH  0
#else
#define DIRECT_DISPATCH  1
#endif

#if defined(MR_QUICKJS_PORTABLE_LIBC)
#define MALLOC_OVERHEAD  ((int)sizeof(size_t))
#elif defined(__APPLE__)
#define MALLOC_OVERHEAD  0
#else
#define MALLOC_OVERHEAD  8
#endif

#if !defined(_WIN32)
/* define it if printf uses the RNDN rounding mode instead of RNDNA */
#define CONFIG_PRINTF_RNDN
#endif

/* define to include Atomics.* operations which depend on the OS
   threads */
#if !defined(__EMSCRIPTEN__) && !defined(MR_QUICKJS_NO_THREADS)
#define CONFIG_ATOMICS
#endif

#if !defined(__EMSCRIPTEN__)
/* enable stack limitation */
#define CONFIG_STACK_CHECK
#endif


/* dump object free */
//#define DUMP_FREE
//#define DUMP_CLOSURE
/* dump the bytecode of the compiled functions: combination of bits
   1: dump pass 3 final byte code
   2: dump pass 2 code
   4: dump pass 1 code
   8: dump stdlib functions
  16: dump bytecode in hex
  32: dump line number table
  64: dump compute_stack_size
 */
//#define DUMP_BYTECODE  (1)
/* dump the occurence of the automatic GC */
//#define DUMP_GC
/* dump objects freed by the garbage collector */
//#define DUMP_GC_FREE
/* dump objects leaking when freeing the runtime */
//#define DUMP_LEAKS  1
/* dump memory usage before running the garbage collector */
//#define DUMP_MEM
//#define DUMP_OBJECTS    /* dump objects in JS_FreeContext */
//#define DUMP_ATOMS      /* dump atoms in JS_FreeContext */
//#define DUMP_SHAPES     /* dump shapes in JS_FreeContext */
//#define DUMP_MODULE_RESOLVE
//#define DUMP_MODULE_EXEC
//#define DUMP_PROMISE
//#define DUMP_READ_OBJECT
//#define DUMP_ROPE_REBALANCE
/* add asm labels to each opcode so that it is easier to see the generated code */
//#define OPCODE_ASM_LABEL

/* test the GC by forcing it before each object allocation */
//#define FORCE_GC_AT_MALLOC

#ifdef CONFIG_ATOMICS
#include <pthread.h>
#include <stdatomic.h>
#include <errno.h>
#endif

enum {
    /* classid tag        */    /* union usage   | properties */
    JS_CLASS_OBJECT = 1,        /* must be first */
    JS_CLASS_ARRAY,             /* u.array       | length */
    JS_CLASS_ERROR,
    JS_CLASS_NUMBER,            /* u.object_data */
    JS_CLASS_STRING,            /* u.object_data */
    JS_CLASS_BOOLEAN,           /* u.object_data */
    JS_CLASS_SYMBOL,            /* u.object_data */
    JS_CLASS_ARGUMENTS,         /* u.array       | length */
    JS_CLASS_MAPPED_ARGUMENTS,  /* u.array       | length */
    JS_CLASS_DATE,              /* u.object_data */
    JS_CLASS_MODULE_NS,
    JS_CLASS_C_FUNCTION,        /* u.cfunc */
    JS_CLASS_BYTECODE_FUNCTION, /* u.func */
    JS_CLASS_BOUND_FUNCTION,    /* u.bound_function */
    JS_CLASS_C_FUNCTION_DATA,   /* u.c_function_data_record */
    JS_CLASS_GENERATOR_FUNCTION, /* u.func */
    JS_CLASS_FOR_IN_ITERATOR,   /* u.for_in_iterator */
    JS_CLASS_REGEXP,            /* u.regexp */
    JS_CLASS_ARRAY_BUFFER,      /* u.array_buffer */
    JS_CLASS_SHARED_ARRAY_BUFFER, /* u.array_buffer */
    JS_CLASS_UINT8C_ARRAY,      /* u.array (typed_array) */
    JS_CLASS_INT8_ARRAY,        /* u.array (typed_array) */
    JS_CLASS_UINT8_ARRAY,       /* u.array (typed_array) */
    JS_CLASS_INT16_ARRAY,       /* u.array (typed_array) */
    JS_CLASS_UINT16_ARRAY,      /* u.array (typed_array) */
    JS_CLASS_INT32_ARRAY,       /* u.array (typed_array) */
    JS_CLASS_UINT32_ARRAY,      /* u.array (typed_array) */
    JS_CLASS_BIG_INT64_ARRAY,   /* u.array (typed_array) */
    JS_CLASS_BIG_UINT64_ARRAY,  /* u.array (typed_array) */
    JS_CLASS_FLOAT16_ARRAY,     /* u.array (typed_array) */
    JS_CLASS_FLOAT32_ARRAY,     /* u.array (typed_array) */
    JS_CLASS_FLOAT64_ARRAY,     /* u.array (typed_array) */
    JS_CLASS_DATAVIEW,          /* u.typed_array */
    JS_CLASS_BIG_INT,           /* u.object_data */
    JS_CLASS_MAP,               /* u.map_state */
    JS_CLASS_SET,               /* u.map_state */
    JS_CLASS_WEAKMAP,           /* u.map_state */
    JS_CLASS_WEAKSET,           /* u.map_state */
    JS_CLASS_ITERATOR,          /* u.map_iterator_data */
    JS_CLASS_ITERATOR_CONCAT,   /* u.iterator_concat_data */
    JS_CLASS_ITERATOR_HELPER,   /* u.iterator_helper_data */
    JS_CLASS_ITERATOR_WRAP,     /* u.iterator_wrap_data */
    JS_CLASS_MAP_ITERATOR,      /* u.map_iterator_data */
    JS_CLASS_SET_ITERATOR,      /* u.map_iterator_data */
    JS_CLASS_ARRAY_ITERATOR,    /* u.array_iterator_data */
    JS_CLASS_STRING_ITERATOR,   /* u.array_iterator_data */
    JS_CLASS_REGEXP_STRING_ITERATOR,   /* u.regexp_string_iterator_data */
    JS_CLASS_GENERATOR,         /* u.generator_data */
    JS_CLASS_GLOBAL_OBJECT,     /* u.global_object */
    JS_CLASS_RAWJSON,
    JS_CLASS_PROXY,             /* u.proxy_data */
    JS_CLASS_PROMISE,           /* u.promise_data */
    JS_CLASS_PROMISE_RESOLVE_FUNCTION,  /* u.promise_function_data */
    JS_CLASS_PROMISE_REJECT_FUNCTION,   /* u.promise_function_data */
    JS_CLASS_ASYNC_FUNCTION,            /* u.func */
    JS_CLASS_ASYNC_FUNCTION_RESOLVE,    /* u.async_function_data */
    JS_CLASS_ASYNC_FUNCTION_REJECT,     /* u.async_function_data */
    JS_CLASS_ASYNC_FROM_SYNC_ITERATOR,  /* u.async_from_sync_iterator_data */
    JS_CLASS_ASYNC_GENERATOR_FUNCTION,  /* u.func */
    JS_CLASS_ASYNC_GENERATOR,   /* u.async_generator_data */
    JS_CLASS_WEAK_REF,
    JS_CLASS_FINALIZATION_REGISTRY,

    JS_CLASS_INIT_COUNT, /* last entry for predefined classes */
};

/* number of typed array types */
#define JS_TYPED_ARRAY_COUNT  (JS_CLASS_FLOAT64_ARRAY - JS_CLASS_UINT8C_ARRAY + 1)
static uint8_t const typed_array_size_log2[JS_TYPED_ARRAY_COUNT];
#define typed_array_size_log2(classid)  (typed_array_size_log2[(classid)- JS_CLASS_UINT8C_ARRAY])

typedef enum JSErrorEnum {
    JS_EVAL_ERROR,
    JS_RANGE_ERROR,
    JS_REFERENCE_ERROR,
    JS_SYNTAX_ERROR,
    JS_TYPE_ERROR,
    JS_URI_ERROR,
    JS_INTERNAL_ERROR,
    JS_AGGREGATE_ERROR,

    JS_NATIVE_ERROR_COUNT, /* number of different NativeError objects */
} JSErrorEnum;

/* the variable and scope indexes must fit on 16 bits. The (-1) and
   ARG_SCOPE_END values are reserved. */
#define JS_MAX_LOCAL_VARS 65534
#define JS_STACK_SIZE_MAX 65534
#define JS_STRING_LEN_MAX ((1 << 30) - 1)

/* strings <= this length are not concatenated using ropes. if too
   small, the rope memory overhead becomes high. */
#define JS_STRING_ROPE_SHORT_LEN  512
/* specific threshold for initial rope use */
#define JS_STRING_ROPE_SHORT2_LEN 8192
/* rope depth at which we rebalance */
#define JS_STRING_ROPE_MAX_DEPTH 60

#define __exception __attribute__((warn_unused_result))

typedef struct JSShape JSShape;
typedef struct JSString JSString;
typedef struct JSString JSAtomStruct;
typedef struct JSObject JSObject;

#define JS_VALUE_GET_OBJ(v) ((JSObject *)JS_VALUE_GET_PTR(v))
#define JS_VALUE_GET_STRING(v) ((JSString *)JS_VALUE_GET_PTR(v))
#define JS_VALUE_GET_STRING_ROPE(v) ((JSStringRope *)JS_VALUE_GET_PTR(v))

typedef enum {
    JS_GC_PHASE_NONE,
    JS_GC_PHASE_DECREF,
    JS_GC_PHASE_REMOVE_CYCLES,
} JSGCPhaseEnum;

typedef enum OPCodeEnum OPCodeEnum;

/* JS malloc */

#define JS_MALLOC_ALIGN 8
#define JS_MALLOC_ARENA_SIZE 4096
#define JS_MALLOC_BLOCK_SIZE_COUNT 31
#define JS_MALLOC_MIN_SMALL_SIZE 16
#define JS_MALLOC_MAX_SMALL_SIZE 512
#if defined(__SANITIZE_ADDRESS__)
/* use the host malloc() for all allocations */
#define JS_MALLOC_LARGE_BLOCKS_ONLY 1
#else
#define JS_MALLOC_LARGE_BLOCKS_ONLY 0
#endif

/* allow iteration among the allocated blocks. Currently not used. May
   be used to suppress the memory overhead of JSGCObjectHeader */
//#define JS_MALLOC_USE_ITER

#define FREE_NIL 0xffff

/* 8 byte header */
/* Notes:
   - the header is necessary at least to recover a pointer to
     JSMallocArena because we don't want to enforce a page
     alignment on the system malloc().
   - could store the block offset instead of (block_idx,
   block_size_idx), but it would require a division to recover the block
   index.
*/
typedef struct JSMallocBlockHeader {
    union {
        uint16_t block_idx; /* FREE_NIL if large block */
        uint16_t free_next; /* FREE_NIL if none */
    } u;
    uint8_t block_size_idx;
    uint8_t gc_obj_type : 7;
    uint8_t mark : 1;
    int ref_count;
    __attribute__((aligned(JS_MALLOC_ALIGN))) uint8_t user_data[];
} JSMallocBlockHeader;

typedef struct JSMallocLargeBlockHeader {
#ifdef JS_MALLOC_USE_ITER
    struct list_head link;
#endif
    JSMallocBlockHeader header;
} JSMallocLargeBlockHeader;

typedef struct {
    struct list_head free_link;
    struct list_head link;
    uint8_t block_size_idx;
    uint16_t n_used_blocks; /* number of allocated blocks */
    uint16_t n_blocks; /* total number of blocks */
    uint16_t first_free_block; /* FREE_NIL if none */
#ifdef JS_MALLOC_USE_ITER
    /* bit set to 1 for allocated block */
    uint32_t bitmap[((JS_MALLOC_ARENA_SIZE / JS_MALLOC_MIN_SMALL_SIZE) + 31) / 32];
#endif
    /* n_blocks memory blocks of identical size */
    __attribute__((aligned(JS_MALLOC_ALIGN))) uint8_t blocks[];
} JSMallocArena;

typedef struct {
    struct list_head arena_list[JS_MALLOC_BLOCK_SIZE_COUNT]; /* list of JSMallocArena.link (all arenas) */
    struct list_head free_arena_list[JS_MALLOC_BLOCK_SIZE_COUNT]; /* list of JSMallocArena.free_link (arenas where n_used_blocks < n_blocks) */
#ifdef JS_MALLOC_USE_ITER
    struct list_head large_block_list; /* list of JSMallocLargeBlockHeader.link */
#endif
    __attribute__((aligned(JS_MALLOC_ALIGN))) uint8_t zero_size_block[sizeof(JSMallocBlockHeader)];

    /* callbacks to the host malloc */
    JSMallocFunctions mf;
    JSMallocState malloc_state;
} JSMallocContext;

/* end JS Malloc */

struct JSRuntime {
    JSMallocContext malloc_ctx;
    const char *rt_info;

    int atom_hash_size; /* power of two */
    int atom_count;
    int atom_size;
    int atom_count_resize; /* resize hash table at this count */
    uint32_t *atom_hash;
    JSAtomStruct **atom_array;
    int atom_free_index; /* 0 = none */

    int class_count;    /* size of class_array */
    JSClass *class_array;

    struct list_head context_list; /* list of JSContext.link */
    /* list of JSGCObjectHeader.link. List of allocated GC objects (used
       by the garbage collector) */
    struct list_head gc_obj_list;
    /* list of JSGCObjectHeader.link. Used during JS_FreeValueRT() */
    struct list_head gc_zero_ref_count_list;
    struct list_head tmp_obj_list; /* used during GC */
    JSGCPhaseEnum gc_phase : 8;
    size_t malloc_gc_threshold;
    struct list_head weakref_list; /* list of JSWeakRefHeader.link */
#ifdef DUMP_LEAKS
    struct list_head string_list; /* list of JSString.link */
#endif
    /* stack limitation */
    uintptr_t stack_size; /* in bytes, 0 if no limit */
    uintptr_t stack_top;
    uintptr_t stack_limit; /* lower stack limit */

    JSValue current_exception;
    /* true if the current exception cannot be catched */
    BOOL current_exception_is_uncatchable : 8;
    /* true if inside an out of memory error, to avoid recursing */
    BOOL in_out_of_memory : 8;

    struct JSStackFrame *current_stack_frame;

    JSInterruptHandler *interrupt_handler;
    void *interrupt_opaque;

    JSHostPromiseRejectionTracker *host_promise_rejection_tracker;
    void *host_promise_rejection_tracker_opaque;

    struct list_head job_list; /* list of JSJobEntry.link */

    JSModuleNormalizeFunc *module_normalize_func;
    BOOL module_loader_has_attr;
    union {
        JSModuleLoaderFunc *module_loader_func;
        JSModuleLoaderFunc2 *module_loader_func2;
    } u;
    JSModuleCheckSupportedImportAttributes *module_check_attrs;
    void *module_loader_opaque;
    /* timestamp for internal use in module evaluation */
    int64_t module_async_evaluation_next_timestamp;

    BOOL can_block : 8; /* TRUE if Atomics.wait can block */
    /* used to allocate, free and clone SharedArrayBuffers */
    JSSharedArrayBufferFunctions sab_funcs;
    /* see JS_SetStripInfo() */
    uint8_t strip_flags;

    /* Shape hash table */
    int shape_hash_bits;
    int shape_hash_size;
    int shape_hash_count; /* number of hashed shapes */
    JSShape **shape_hash;
    void *user_opaque;
};

struct JSClass {
    uint32_t class_id; /* 0 means free entry */
    JSAtom class_name;
    JSClassFinalizer *finalizer;
    JSClassGCMark *gc_mark;
    JSClassCall *call;
    /* pointers for exotic behavior, can be NULL if none are present */
    const JSClassExoticMethods *exotic;
};

#define JS_MODE_STRICT (1 << 0)
#define JS_MODE_ASYNC  (1 << 2) /* async function */
#define JS_MODE_BACKTRACE_BARRIER (1 << 3) /* stop backtrace before this frame */

typedef struct JSStackFrame {
    struct JSStackFrame *prev_frame; /* NULL if first stack frame */
    JSValue cur_func; /* current function, JS_UNDEFINED if the frame is detached */
    JSValue *arg_buf; /* arguments */
    JSValue *var_buf; /* variables */
    struct JSVarRef **var_refs; /* references to arguments or local variables */
    const uint8_t *cur_pc; /* only used in bytecode functions : PC of the
                        instruction after the call */
    int arg_count;
    int js_mode; /* not supported for C functions */
    /* only used in generators. Current stack pointer value. NULL if
       the function is running. */
    JSValue *cur_sp;
} JSStackFrame;

typedef enum {
    JS_GC_OBJ_TYPE_JS_OBJECT,
    JS_GC_OBJ_TYPE_FUNCTION_BYTECODE,
    JS_GC_OBJ_TYPE_SHAPE,
    JS_GC_OBJ_TYPE_VAR_REF,
    JS_GC_OBJ_TYPE_ASYNC_FUNCTION,
    JS_GC_OBJ_TYPE_JS_CONTEXT,
    JS_GC_OBJ_TYPE_MODULE,
} JSGCObjectTypeEnum;

/* header for GC objects. GC objects are C data structures with a
   reference count that can reference other GC objects. JS Objects are
   a particular type of GC object. */
struct JSGCObjectHeader {
    struct list_head link;
};

typedef enum {
    JS_WEAKREF_TYPE_MAP,
    JS_WEAKREF_TYPE_WEAKREF,
    JS_WEAKREF_TYPE_FINREC,
} JSWeakRefHeaderTypeEnum;

typedef struct {
    struct list_head link;
    JSWeakRefHeaderTypeEnum weakref_type;
} JSWeakRefHeader;

typedef struct JSVarRef {
    JSGCObjectHeader header; /* must come first */
    uint8_t is_detached;
    uint8_t is_lexical; /* only used with global variables */
    uint8_t is_const; /* only used with global variables */
    JSValue *pvalue; /* pointer to the value, either on the stack or
                        to 'value' */
    union {
        JSValue value; /* used when is_detached = TRUE */
        struct {
            uint16_t var_ref_idx; /* index in JSStackFrame.var_refs[] */
            JSStackFrame *stack_frame;
        }; /* used when is_detached = FALSE */
    };
} JSVarRef;

/* bigint */

#if JS_LIMB_BITS == 32

typedef int32_t js_slimb_t;
typedef uint32_t js_limb_t;
typedef int64_t js_sdlimb_t;
typedef uint64_t js_dlimb_t;

#define JS_LIMB_DIGITS 9

#else

typedef __int128 int128_t;
typedef unsigned __int128 uint128_t;
typedef int64_t js_slimb_t;
typedef uint64_t js_limb_t;
typedef int128_t js_sdlimb_t;
typedef uint128_t js_dlimb_t;

#define JS_LIMB_DIGITS 19

#endif

typedef struct JSBigInt {
    uint32_t len; /* number of limbs, >= 1 */
    js_limb_t tab[]; /* two's complement representation, always
                        normalized so that 'len' is the minimum
                        possible length >= 1 */
} JSBigInt;

/* this bigint structure can hold a 64 bit integer */
typedef struct {
    js_limb_t big_int_buf[sizeof(JSBigInt) / sizeof(js_limb_t)]; /* for JSBigInt */
    /* must come just after */
    js_limb_t tab[(64 + JS_LIMB_BITS - 1) / JS_LIMB_BITS];
} JSBigIntBuf;

typedef enum {
    JS_AUTOINIT_ID_PROTOTYPE,
    JS_AUTOINIT_ID_MODULE_NS,
    JS_AUTOINIT_ID_PROP,
} JSAutoInitIDEnum;

/* must be large enough to have a negligible runtime cost and small
   enough to call the interrupt callback often. */
#define JS_INTERRUPT_COUNTER_INIT 10000

struct JSContext {
    JSGCObjectHeader header; /* must come first */
    JSRuntime *rt;
    struct list_head link;

    uint16_t binary_object_count;
    int binary_object_size;

    JSShape *array_shape;   /* initial shape for Array objects */
    JSShape *arguments_shape;  /* shape for arguments objects */
    JSShape *mapped_arguments_shape;  /* shape for mapped arguments objects */
    JSShape *regexp_shape;  /* shape for regexp objects */
    JSShape *regexp_result_shape;  /* shape for regexp result objects */

    JSValue *class_proto;
    JSValue function_proto;
    JSValue function_ctor;
    JSValue array_ctor;
    JSValue regexp_ctor;
    JSValue promise_ctor;
    JSValue native_error_proto[JS_NATIVE_ERROR_COUNT];
    JSValue iterator_ctor;
    JSValue async_iterator_proto;
    JSValue array_proto_values;
    JSValue throw_type_error;
    JSValue eval_obj;

    JSValue global_obj; /* global object */
    JSValue global_var_obj; /* contains the global let/const definitions */

    uint64_t random_state;

    /* when the counter reaches zero, JSRutime.interrupt_handler is called */
    int interrupt_counter;

    struct list_head loaded_modules; /* list of JSModuleDef.link */

    /* if NULL, RegExp compilation is not supported */
    JSValue (*compile_regexp)(JSContext *ctx, JSValueConst pattern,
                              JSValueConst flags);
    /* if NULL, eval is not supported */
    JSValue (*eval_internal)(JSContext *ctx, JSValueConst this_obj,
                             const char *input, size_t input_len,
                             const char *filename, int flags, int scope_idx);
    void *user_opaque;
};

typedef union JSFloat64Union {
    double d;
    uint64_t u64;
    uint32_t u32[2];
} JSFloat64Union;

enum {
    JS_ATOM_TYPE_STRING = 1,
    JS_ATOM_TYPE_GLOBAL_SYMBOL,
    JS_ATOM_TYPE_SYMBOL,
    JS_ATOM_TYPE_PRIVATE,
};

typedef enum {
    JS_ATOM_KIND_STRING,
    JS_ATOM_KIND_SYMBOL,
    JS_ATOM_KIND_PRIVATE,
} JSAtomKindEnum;

#define JS_ATOM_HASH_MASK  ((1 << 30) - 1)
#define JS_ATOM_HASH_PRIVATE JS_ATOM_HASH_MASK

struct JSString {
    uint32_t len : 31;
    uint8_t is_wide_char : 1; /* 0 = 8 bits, 1 = 16 bits characters */
    /* for JS_ATOM_TYPE_SYMBOL: hash = weakref_count, atom_type = 3,
       for JS_ATOM_TYPE_PRIVATE: hash = JS_ATOM_HASH_PRIVATE, atom_type = 3
       XXX: could change encoding to have one more bit in hash */
    uint32_t hash : 30;
    uint8_t atom_type : 2; /* != 0 if atom, JS_ATOM_TYPE_x */
    uint32_t hash_next; /* atom_index for JS_ATOM_TYPE_SYMBOL */
#ifdef DUMP_LEAKS
    struct list_head link; /* string list */
#endif
    union {
        uint8_t str8[0]; /* 8 bit strings will get an extra null terminator */
        uint16_t str16[0];
    } u;
};

typedef struct JSStringRope {
    uint32_t len;
    uint8_t is_wide_char; /* 0 = 8 bits, 1 = 16 bits characters */
    uint8_t depth; /* max depth of the rope tree */
    /* XXX: could reduce memory usage by using a direct pointer with
       bit 0 to select rope or string */
    JSValue left;
    JSValue right; /* might be the empty string */
} JSStringRope;

typedef enum {
    JS_CLOSURE_LOCAL, /* 'var_idx' is the index of a local variable in the parent function */
    JS_CLOSURE_ARG, /* 'var_idx' is the index of a argument variable in the parent function */
    JS_CLOSURE_REF, /* 'var_idx' is the index of a closure variable in the parent function */
    JS_CLOSURE_GLOBAL_REF, /* 'var_idx' in the index of a closure
                              variable in the parent function
                              referencing a global variable */
    JS_CLOSURE_GLOBAL_DECL, /* global variable declaration (eval code only) */
    JS_CLOSURE_GLOBAL, /* global variable (eval code only) */
    JS_CLOSURE_MODULE_DECL, /* definition of a module variable (eval code only) */
    JS_CLOSURE_MODULE_IMPORT, /* definition of a module import (eval code only) */
} JSClosureTypeEnum;

typedef struct JSClosureVar {
    JSClosureTypeEnum closure_type : 3;
    uint8_t is_lexical : 1; /* lexical variable */
    uint8_t is_const : 1; /* const variable (is_lexical = 1 if is_const = 1 */
    uint8_t var_kind : 4; /* see JSVarKindEnum */
    uint16_t var_idx; /* is_local = TRUE: index to a normal variable of the
                    parent function. otherwise: index to a closure
                    variable of the parent function */
    JSAtom var_name;
} JSClosureVar;

#define ARG_SCOPE_INDEX 1
#define ARG_SCOPE_END (-2)

typedef enum {
    /* XXX: add more variable kinds here instead of using bit fields */
    JS_VAR_NORMAL,
    JS_VAR_FUNCTION_DECL, /* lexical var with function declaration */
    JS_VAR_NEW_FUNCTION_DECL, /* lexical var with async/generator
                                 function declaration */
    JS_VAR_CATCH,
    JS_VAR_FUNCTION_NAME, /* function expression name */
    JS_VAR_PRIVATE_FIELD,
    JS_VAR_PRIVATE_METHOD,
    JS_VAR_PRIVATE_GETTER,
    JS_VAR_PRIVATE_SETTER, /* must come after JS_VAR_PRIVATE_GETTER */
    JS_VAR_PRIVATE_GETTER_SETTER, /* must come after JS_VAR_PRIVATE_SETTER */
    JS_VAR_GLOBAL_FUNCTION_DECL, /* global function definition, only in JSVarDef */
} JSVarKindEnum;

typedef struct JSBytecodeVarDef {
    JSAtom var_name;
    /* index into JSFunctionBytecode.vars of the next variable in the same or
       enclosing lexical scope
    */
    int scope_next; /* XXX: store on 16 bits */
    uint8_t is_const : 1;
    uint8_t is_lexical : 1;
    uint8_t is_captured : 1; /* XXX: could remove and use a var_ref_idx value */
    uint8_t has_scope: 1; /* true if JSVarDef.scope_level != 0 */
    uint8_t var_kind : 4; /* see JSVarKindEnum */
    /* If is_captured = TRUE, provides, the index of the corresponding
       JSVarRef on stack. It would be more compact to have a separate
       table with the corresponding inverted table but it requires
       more modifications in the code. */
    uint16_t var_ref_idx;
} JSBytecodeVarDef;

/* for the encoding of the pc2line table */
#define PC2LINE_BASE     (-1)
#define PC2LINE_RANGE    5
#define PC2LINE_OP_FIRST 1
#define PC2LINE_DIFF_PC_MAX ((255 - PC2LINE_OP_FIRST) / PC2LINE_RANGE)

typedef enum JSFunctionKindEnum {
    JS_FUNC_NORMAL = 0,
    JS_FUNC_GENERATOR = (1 << 0),
    JS_FUNC_ASYNC = (1 << 1),
    JS_FUNC_ASYNC_GENERATOR = (JS_FUNC_GENERATOR | JS_FUNC_ASYNC),
} JSFunctionKindEnum;

typedef struct JSFunctionBytecode {
    JSGCObjectHeader header; /* must come first */
    uint8_t js_mode;
    uint8_t has_prototype : 1; /* true if a prototype field is necessary */
    uint8_t has_simple_parameter_list : 1;
    uint8_t is_derived_class_constructor : 1;
    /* true if home_object needs to be initialized */
    uint8_t need_home_object : 1;
    uint8_t func_kind : 2;
    uint8_t new_target_allowed : 1;
    uint8_t super_call_allowed : 1;
    uint8_t super_allowed : 1;
    uint8_t arguments_allowed : 1;
    uint8_t has_debug : 1;
    uint8_t read_only_bytecode : 1;
    uint8_t is_direct_or_indirect_eval : 1; /* used by JS_GetScriptOrModuleName() */
    /* XXX: 10 bits available */
    uint8_t *byte_code_buf; /* (self pointer) */
    int byte_code_len;
    JSAtom func_name;
    JSBytecodeVarDef *vardefs; /* arguments + local variables (arg_count + var_count) (self pointer) */
    JSClosureVar *closure_var; /* list of variables in the closure (self pointer) */
    uint16_t arg_count;
    uint16_t var_count;
    uint16_t defined_arg_count; /* for length function property */
    uint16_t stack_size; /* maximum stack size */
    uint16_t var_ref_count; /* number of local variable references */
    JSContext *realm; /* function realm */
    JSValue *cpool; /* constant pool (self pointer) */
    int cpool_count;
    int closure_var_count;
    struct {
        /* debug info, move to separate structure to save memory? */
        JSAtom filename;
        int source_len;
        int pc2line_len;
        uint8_t *pc2line_buf;
        char *source;
    } debug;
} JSFunctionBytecode;

typedef struct JSBoundFunction {
    JSValue func_obj;
    JSValue this_val;
    int argc;
    JSValue argv[0];
} JSBoundFunction;

typedef enum JSIteratorKindEnum {
    JS_ITERATOR_KIND_KEY,
    JS_ITERATOR_KIND_VALUE,
    JS_ITERATOR_KIND_KEY_AND_VALUE,
} JSIteratorKindEnum;

typedef struct JSForInIterator {
    JSValue obj;
    uint32_t idx;
    uint32_t atom_count;
    uint8_t in_prototype_chain;
    uint8_t is_array;
    JSPropertyEnum *tab_atom; /* is_array = FALSE */
} JSForInIterator;

typedef struct JSRegExp {
    JSString *pattern;
    JSString *bytecode; /* also contains the flags */
} JSRegExp;

typedef struct JSProxyData {
    JSValue target;
    JSValue handler;
    uint8_t is_func;
    uint8_t is_revoked;
} JSProxyData;

typedef struct JSArrayBuffer {
    int byte_length; /* 0 if detached */
    int max_byte_length; /* -1 if not resizable; >= byte_length otherwise */
    uint8_t detached;
    uint8_t shared; /* if shared, the array buffer cannot be detached */
    uint8_t *data; /* NULL if detached */
    struct list_head array_list;
    void *opaque;
    JSFreeArrayBufferDataFunc *free_func;
} JSArrayBuffer;

typedef struct JSTypedArray {
    struct list_head link; /* link to arraybuffer */
    JSObject *obj; /* back pointer to the TypedArray/DataView object */
    JSObject *buffer; /* based array buffer */
    uint32_t offset; /* byte offset in the array buffer */
    uint32_t length; /* byte length in the array buffer */
    BOOL track_rab; /* auto-track length of backing array buffer */
} JSTypedArray;

typedef struct JSGlobalObject {
    JSValue uninitialized_vars; /* hidden object containing the list of uninitialized variables */
} JSGlobalObject;

typedef struct JSAsyncFunctionState {
    JSGCObjectHeader header;
    JSValue this_val; /* 'this' argument */
    int argc; /* number of function arguments */
    BOOL throw_flag; /* used to throw an exception in JS_CallInternal() */
    BOOL is_completed; /* TRUE if the function has returned. The stack
                          frame is no longer valid */
    JSValue resolving_funcs[2]; /* only used in JS async functions */
    JSStackFrame frame;
    /* arg_buf, var_buf, stack_buf and var_refs follow */
} JSAsyncFunctionState;

typedef enum {
   /* binary operators */
   JS_OVOP_ADD,
   JS_OVOP_SUB,
   JS_OVOP_MUL,
   JS_OVOP_DIV,
   JS_OVOP_MOD,
   JS_OVOP_POW,
   JS_OVOP_OR,
   JS_OVOP_AND,
   JS_OVOP_XOR,
   JS_OVOP_SHL,
   JS_OVOP_SAR,
   JS_OVOP_SHR,
   JS_OVOP_EQ,
   JS_OVOP_LESS,

   JS_OVOP_BINARY_COUNT,
   /* unary operators */
   JS_OVOP_POS = JS_OVOP_BINARY_COUNT,
   JS_OVOP_NEG,
   JS_OVOP_INC,
   JS_OVOP_DEC,
   JS_OVOP_NOT,

   JS_OVOP_COUNT,
} JSOverloadableOperatorEnum;

typedef struct {
    uint32_t operator_index;
    JSObject *ops[JS_OVOP_BINARY_COUNT]; /* self operators */
} JSBinaryOperatorDefEntry;

typedef struct {
    int count;
    JSBinaryOperatorDefEntry *tab;
} JSBinaryOperatorDef;

typedef struct {
    uint32_t operator_counter;
    BOOL is_primitive; /* OperatorSet for a primitive type */
    /* NULL if no operator is defined */
    JSObject *self_ops[JS_OVOP_COUNT]; /* self operators */
    JSBinaryOperatorDef left;
    JSBinaryOperatorDef right;
} JSOperatorSetData;

typedef struct JSReqModuleEntry {
    JSAtom module_name;
    JSModuleDef *module; /* used using resolution */
    JSValue attributes; /* JS_UNDEFINED or an object contains the attributes as key/value */
} JSReqModuleEntry;

typedef enum JSExportTypeEnum {
    JS_EXPORT_TYPE_LOCAL,
    JS_EXPORT_TYPE_INDIRECT,
} JSExportTypeEnum;

typedef struct JSExportEntry {
    union {
        struct {
            int var_idx; /* closure variable index */
            JSVarRef *var_ref; /* if != NULL, reference to the variable */
        } local; /* for local export */
        int req_module_idx; /* module for indirect export */
    } u;
    JSExportTypeEnum export_type;
    JSAtom local_name; /* '*' if export ns from. not used for local
                          export after compilation */
    JSAtom export_name; /* exported variable name */
} JSExportEntry;

typedef struct JSStarExportEntry {
    int req_module_idx; /* in req_module_entries */
} JSStarExportEntry;

typedef struct JSImportEntry {
    int var_idx; /* closure variable index */
    BOOL is_star; /* import_name = '*' is a valid import name, so need a flag */
    JSAtom import_name;
    int req_module_idx; /* in req_module_entries */
} JSImportEntry;

typedef enum {
    JS_MODULE_STATUS_UNLINKED,
    JS_MODULE_STATUS_LINKING,
    JS_MODULE_STATUS_LINKED,
    JS_MODULE_STATUS_EVALUATING,
    JS_MODULE_STATUS_EVALUATING_ASYNC,
    JS_MODULE_STATUS_EVALUATED,
} JSModuleStatus;

struct JSModuleDef {
    JSGCObjectHeader header; /* must come first */
    JSAtom module_name;
    struct list_head link;

    JSReqModuleEntry *req_module_entries;
    int req_module_entries_count;
    int req_module_entries_size;

    JSExportEntry *export_entries;
    int export_entries_count;
    int export_entries_size;

    JSStarExportEntry *star_export_entries;
    int star_export_entries_count;
    int star_export_entries_size;

    JSImportEntry *import_entries;
    int import_entries_count;
    int import_entries_size;

    JSValue module_ns;
    JSValue func_obj; /* only used for JS modules */
    JSModuleInitFunc *init_func; /* only used for C modules */
    BOOL has_tla : 8; /* true if func_obj contains await */
    BOOL resolved : 8;
    BOOL func_created : 8;
    JSModuleStatus status : 8;
    /* temp use during js_module_link() & js_module_evaluate() */
    int dfs_index, dfs_ancestor_index;
    JSModuleDef *stack_prev;
    /* temp use during js_module_evaluate() */
    JSModuleDef **async_parent_modules;
    int async_parent_modules_count;
    int async_parent_modules_size;
    int pending_async_dependencies;
    BOOL async_evaluation; /* true: async_evaluation_timestamp corresponds to [[AsyncEvaluationOrder]]
                              false: [[AsyncEvaluationOrder]] is UNSET or DONE */
    int64_t async_evaluation_timestamp;
    JSModuleDef *cycle_root;
    JSValue promise; /* corresponds to spec field: capability */
    JSValue resolving_funcs[2]; /* corresponds to spec field: capability */

    /* true if evaluation yielded an exception. It is saved in
       eval_exception */
    BOOL eval_has_exception : 8;
    JSValue eval_exception;
    JSValue meta_obj; /* for import.meta */
    JSValue private_value; /* private value for C modules */
};

typedef struct JSJobEntry {
    struct list_head link;
    JSContext *realm;
    JSJobFunc *job_func;
    int argc;
    JSValue argv[0];
} JSJobEntry;

typedef struct JSProperty {
    union {
        JSValue value;      /* JS_PROP_NORMAL */
        struct {            /* JS_PROP_GETSET */
            JSObject *getter; /* NULL if undefined */
            JSObject *setter; /* NULL if undefined */
        } getset;
        JSVarRef *var_ref;  /* JS_PROP_VARREF */
        struct {            /* JS_PROP_AUTOINIT */
            /* in order to use only 2 pointers, we compress the realm
               and the init function pointer */
            uintptr_t realm_and_id; /* realm and init_id (JS_AUTOINIT_ID_x)
                                       in the 2 low bits */
            void *opaque;
        } init;
    } u;
} JSProperty;

#define JS_PROP_INITIAL_SIZE 2
#define JS_PROP_INITIAL_HASH_SIZE 4 /* must be a power of two */

typedef struct JSShapeProperty {
    uint32_t hash_next : 26; /* 0 if last in list */
    uint32_t flags : 6;   /* JS_PROP_XXX */
    JSAtom atom; /* JS_ATOM_NULL = free property entry */
} JSShapeProperty;

struct JSShape {
    JSGCObjectHeader header;
    /* true if the shape is inserted in the shape hash table. If not,
       JSShape.hash is not valid */
    uint8_t is_hashed;
    uint32_t hash; /* current hash value */
    uint32_t prop_hash_mask; /* >= 2 */
    int prop_size; /* allocated properties */
    int prop_count; /* include deleted properties */
    int deleted_prop_count;
    JSShape *shape_hash_next; /* in JSRuntime.shape_hash[h] list */
    JSObject *proto;
    uint32_t hash_table[]; /* prop_hash_mask + 1 elements */
    /* followed by JSShapeProperty prop[prop_size]; */
};

struct JSObject {
    JSGCObjectHeader header;
    /* TRUE if the array prototype is "normal":
       - no small index properties which are get/set or non writable
       - its prototype is Object.prototype
       - Object.prototype has no small index properties which are get/set or non writable
       - the prototype of Object.prototype is null (always true as it is immutable)
    */
    uint8_t is_std_array_prototype : 1;

    uint8_t extensible : 1;
    uint8_t free_mark : 1; /* only used when freeing objects with cycles */
    uint8_t is_exotic : 1; /* TRUE if object has exotic property handlers */
    uint8_t fast_array : 1; /* TRUE if u.array is used for get/put (for JS_CLASS_ARRAY, JS_CLASS_ARGUMENTS, JS_CLASS_MAPPED_ARGUMENTS and typed arrays) */
    uint8_t is_constructor : 1; /* TRUE if object is a constructor function */
    uint8_t has_immutable_prototype : 1; /* cannot modify the prototype */
    uint8_t tmp_mark : 1; /* used in JS_WriteObjectRec() */
    uint8_t is_HTMLDDA : 1; /* specific annex B IsHtmlDDA behavior */
    uint16_t class_id; /* see JS_CLASS_x */
    /* count the number of weak references to this object. The object
       structure is freed only if header.ref_count = 0 and
       weakref_count = 0 */
    uint32_t weakref_count;
    JSShape *shape; /* prototype and property names + flag */
    JSProperty *prop; /* array of properties */
    union {
        void *opaque;
        struct JSBoundFunction *bound_function; /* JS_CLASS_BOUND_FUNCTION */
        struct JSCFunctionDataRecord *c_function_data_record; /* JS_CLASS_C_FUNCTION_DATA */
        struct JSForInIterator *for_in_iterator; /* JS_CLASS_FOR_IN_ITERATOR */
        struct JSArrayBuffer *array_buffer; /* JS_CLASS_ARRAY_BUFFER, JS_CLASS_SHARED_ARRAY_BUFFER */
        struct JSTypedArray *typed_array; /* JS_CLASS_UINT8C_ARRAY..JS_CLASS_DATAVIEW */
        struct JSMapState *map_state;   /* JS_CLASS_MAP..JS_CLASS_WEAKSET */
        struct JSMapIteratorData *map_iterator_data; /* JS_CLASS_MAP_ITERATOR, JS_CLASS_SET_ITERATOR */
        struct JSArrayIteratorData *array_iterator_data; /* JS_CLASS_ARRAY_ITERATOR, JS_CLASS_STRING_ITERATOR */
        struct JSRegExpStringIteratorData *regexp_string_iterator_data; /* JS_CLASS_REGEXP_STRING_ITERATOR */
        struct JSGeneratorData *generator_data; /* JS_CLASS_GENERATOR */
        struct JSIteratorConcatData *iterator_concat_data; /* JS_CLASS_ITERATOR_CONCAT */
        struct JSIteratorHelperData *iterator_helper_data; /* JS_CLASS_ITERATOR_HELPER */
        struct JSIteratorWrapData *iterator_wrap_data; /* JS_CLASS_ITERATOR_WRAP */
        struct JSProxyData *proxy_data; /* JS_CLASS_PROXY */
        struct JSPromiseData *promise_data; /* JS_CLASS_PROMISE */
        struct JSPromiseFunctionData *promise_function_data; /* JS_CLASS_PROMISE_RESOLVE_FUNCTION, JS_CLASS_PROMISE_REJECT_FUNCTION */
        struct JSAsyncFunctionState *async_function_data; /* JS_CLASS_ASYNC_FUNCTION_RESOLVE, JS_CLASS_ASYNC_FUNCTION_REJECT */
        struct JSAsyncFromSyncIteratorData *async_from_sync_iterator_data; /* JS_CLASS_ASYNC_FROM_SYNC_ITERATOR */
        struct JSAsyncGeneratorData *async_generator_data; /* JS_CLASS_ASYNC_GENERATOR */
        struct { /* JS_CLASS_BYTECODE_FUNCTION: 12/24 bytes */
            /* also used by JS_CLASS_GENERATOR_FUNCTION, JS_CLASS_ASYNC_FUNCTION and JS_CLASS_ASYNC_GENERATOR_FUNCTION */
            struct JSFunctionBytecode *function_bytecode;
            JSVarRef **var_refs;
            JSObject *home_object; /* for 'super' access */
        } func;
        struct { /* JS_CLASS_C_FUNCTION: 12/20 bytes */
            JSContext *realm;
            JSCFunctionType c_function;
            uint8_t length;
            uint8_t cproto;
            int16_t magic;
        } cfunc;
        /* array part for fast arrays and typed arrays */
        struct { /* JS_CLASS_ARRAY, JS_CLASS_ARGUMENTS, JS_CLASS_MAPPED_ARGUMENTS, JS_CLASS_UINT8C_ARRAY..JS_CLASS_FLOAT64_ARRAY */
            union {
                uint32_t size;          /* JS_CLASS_ARRAY */
                struct JSTypedArray *typed_array; /* JS_CLASS_UINT8C_ARRAY..JS_CLASS_FLOAT64_ARRAY */
            } u1;
            union {
                JSValue *values;        /* JS_CLASS_ARRAY, JS_CLASS_ARGUMENTS */
                JSVarRef **var_refs;     /* JS_CLASS_MAPPED_ARGUMENTS */
                void *ptr;              /* JS_CLASS_UINT8C_ARRAY..JS_CLASS_FLOAT64_ARRAY */
                int8_t *int8_ptr;       /* JS_CLASS_INT8_ARRAY */
                uint8_t *uint8_ptr;     /* JS_CLASS_UINT8_ARRAY, JS_CLASS_UINT8C_ARRAY */
                int16_t *int16_ptr;     /* JS_CLASS_INT16_ARRAY */
                uint16_t *uint16_ptr;   /* JS_CLASS_UINT16_ARRAY */
                int32_t *int32_ptr;     /* JS_CLASS_INT32_ARRAY */
                uint32_t *uint32_ptr;   /* JS_CLASS_UINT32_ARRAY */
                int64_t *int64_ptr;     /* JS_CLASS_INT64_ARRAY */
                uint64_t *uint64_ptr;   /* JS_CLASS_UINT64_ARRAY */
                uint16_t *fp16_ptr;     /* JS_CLASS_FLOAT16_ARRAY */
                float *float_ptr;       /* JS_CLASS_FLOAT32_ARRAY */
                double *double_ptr;     /* JS_CLASS_FLOAT64_ARRAY */
            } u;
            uint32_t count; /* <= 2^31-1. 0 for a detached typed array */
        } array;    /* 12/20 bytes */
        JSRegExp regexp;    /* JS_CLASS_REGEXP: 8/16 bytes */
        JSValue object_data;    /* for JS_SetObjectData(): 8/16/16 bytes */
        JSGlobalObject global_object;
    } u;
};

typedef struct JSMapRecord {
    int ref_count; /* used during enumeration to avoid freeing the record */
    BOOL empty : 8; /* TRUE if the record is deleted */
    struct list_head link;
    struct JSMapRecord *hash_next;
    JSValue key;
    JSValue value;
} JSMapRecord;

typedef struct JSMapState {
    BOOL is_weak; /* TRUE if WeakSet/WeakMap */
    struct list_head records; /* list of JSMapRecord.link */
    uint32_t record_count;
    JSMapRecord **hash_table;
    int hash_bits;
    uint32_t hash_size; /* = 2 ^ hash_bits */
    uint32_t record_count_threshold; /* count at which a hash table
                                        resize is needed */
    JSWeakRefHeader weakref_header; /* only used if is_weak = TRUE */
} JSMapState;

enum {
    __JS_ATOM_NULL = JS_ATOM_NULL,
#define DEF(name, str) JS_ATOM_ ## name,
#include "quickjs-atom.h"
#undef DEF
    JS_ATOM_END,
};
#define JS_ATOM_LAST_KEYWORD JS_ATOM_super
#define JS_ATOM_LAST_STRICT_KEYWORD JS_ATOM_yield

static const char js_atom_init[] =
#define DEF(name, str) str "\0"
#include "quickjs-atom.h"
#undef DEF
;

typedef enum OPCodeFormat {
#define FMT(f) OP_FMT_ ## f,
#define DEF(id, size, n_pop, n_push, f)
#include "quickjs-opcode.h"
#undef DEF
#undef FMT
} OPCodeFormat;

enum OPCodeEnum {
#define FMT(f)
#define DEF(id, size, n_pop, n_push, f) OP_ ## id,
#define def(id, size, n_pop, n_push, f)
#include "quickjs-opcode.h"
#undef def
#undef DEF
#undef FMT
    OP_COUNT, /* excluding temporary opcodes */
    /* temporary opcodes : overlap with the short opcodes */
    OP_TEMP_START = OP_nop + 1,
    OP___dummy = OP_TEMP_START - 1,
#define FMT(f)
#define DEF(id, size, n_pop, n_push, f)
#define def(id, size, n_pop, n_push, f) OP_ ## id,
#include "quickjs-opcode.h"
#undef def
#undef DEF
#undef FMT
    OP_TEMP_END,
};

static int JS_InitAtoms(JSRuntime *rt);
static JSAtom __JS_NewAtomInit(JSRuntime *rt, const char *str, int len,
                               int atom_type);
static void JS_FreeAtomStruct(JSRuntime *rt, JSAtomStruct *p);
static void free_function_bytecode(JSRuntime *rt, JSFunctionBytecode *b);
static JSValue js_call_c_function(JSContext *ctx, JSValueConst func_obj,
                                  JSValueConst this_obj,
                                  int argc, JSValueConst *argv, int flags);
static JSValue js_call_bound_function(JSContext *ctx, JSValueConst func_obj,
                                      JSValueConst this_obj,
                                      int argc, JSValueConst *argv, int flags);
static JSValue JS_CallInternal(JSContext *ctx, JSValueConst func_obj,
                               JSValueConst this_obj, JSValueConst new_target,
                               int argc, JSValue *argv, int flags);
static JSValue JS_CallConstructorInternal(JSContext *ctx,
                                          JSValueConst func_obj,
                                          JSValueConst new_target,
                                          int argc, JSValue *argv, int flags);
static JSValue JS_CallFree(JSContext *ctx, JSValue func_obj, JSValueConst this_obj,
                           int argc, JSValueConst *argv);
static JSValue JS_InvokeFree(JSContext *ctx, JSValue this_val, JSAtom atom,
                             int argc, JSValueConst *argv);
static __exception int JS_ToArrayLengthFree(JSContext *ctx, uint32_t *plen,
                                            JSValue val, BOOL is_array_ctor);
static JSValue JS_EvalObject(JSContext *ctx, JSValueConst this_obj,
                             JSValueConst val, int flags, int scope_idx);
JSValue __attribute__((format(printf, 2, 3))) JS_ThrowInternalError(JSContext *ctx, const char *fmt, ...);
static __maybe_unused void JS_DumpAtoms(JSRuntime *rt);
static __maybe_unused void JS_DumpString(JSRuntime *rt, const JSString *p);
static __maybe_unused void JS_DumpObjectHeader(JSRuntime *rt);
static __maybe_unused void JS_DumpObject(JSRuntime *rt, JSObject *p);
static __maybe_unused void JS_DumpGCObject(JSRuntime *rt, JSGCObjectHeader *p);
static __maybe_unused void JS_DumpAtom(JSContext *ctx, const char *str, JSAtom atom);
static __maybe_unused void JS_DumpValueRT(JSRuntime *rt, const char *str, JSValueConst val);
static __maybe_unused void JS_DumpValue(JSContext *ctx, const char *str, JSValueConst val);
static __maybe_unused void JS_DumpShapes(JSRuntime *rt);
static void js_dump_value_write(void *opaque, const char *buf, size_t len);
static JSValue js_function_apply(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv, int magic);
static void js_array_finalizer(JSRuntime *rt, JSValue val);
static void js_array_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
static void js_mapped_arguments_finalizer(JSRuntime *rt, JSValue val);
static void js_mapped_arguments_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
static void js_object_data_finalizer(JSRuntime *rt, JSValue val);
static void js_object_data_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
static void js_c_function_finalizer(JSRuntime *rt, JSValue val);
static void js_c_function_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
static void js_bytecode_function_finalizer(JSRuntime *rt, JSValue val);
static void js_bytecode_function_mark(JSRuntime *rt, JSValueConst val,
                                JS_MarkFunc *mark_func);
static void js_bound_function_finalizer(JSRuntime *rt, JSValue val);
static void js_bound_function_mark(JSRuntime *rt, JSValueConst val,
                                JS_MarkFunc *mark_func);
static void js_for_in_iterator_finalizer(JSRuntime *rt, JSValue val);
static void js_for_in_iterator_mark(JSRuntime *rt, JSValueConst val,
                                JS_MarkFunc *mark_func);
static void js_regexp_finalizer(JSRuntime *rt, JSValue val);
static void js_array_buffer_finalizer(JSRuntime *rt, JSValue val);
static void js_typed_array_finalizer(JSRuntime *rt, JSValue val);
static void js_typed_array_mark(JSRuntime *rt, JSValueConst val,
                                JS_MarkFunc *mark_func);
static void js_proxy_finalizer(JSRuntime *rt, JSValue val);
static void js_proxy_mark(JSRuntime *rt, JSValueConst val,
                                JS_MarkFunc *mark_func);
static void js_map_finalizer(JSRuntime *rt, JSValue val);
static void js_map_mark(JSRuntime *rt, JSValueConst val,
                                JS_MarkFunc *mark_func);
static void js_map_iterator_finalizer(JSRuntime *rt, JSValue val);
static void js_map_iterator_mark(JSRuntime *rt, JSValueConst val,
                                JS_MarkFunc *mark_func);
static void js_array_iterator_finalizer(JSRuntime *rt, JSValue val);
static void js_array_iterator_mark(JSRuntime *rt, JSValueConst val,
                                JS_MarkFunc *mark_func);
static void js_iterator_concat_finalizer(JSRuntime *rt, JSValue val);
static void js_iterator_concat_mark(JSRuntime *rt, JSValueConst val,
                                    JS_MarkFunc *mark_func);
static void js_iterator_helper_finalizer(JSRuntime *rt, JSValue val);
static void js_iterator_helper_mark(JSRuntime *rt, JSValueConst val,
                                    JS_MarkFunc *mark_func);
static void js_iterator_wrap_finalizer(JSRuntime *rt, JSValue val);
static void js_iterator_wrap_mark(JSRuntime *rt, JSValueConst val,
                                  JS_MarkFunc *mark_func);
static void js_regexp_string_iterator_finalizer(JSRuntime *rt, JSValue val);
static void js_regexp_string_iterator_mark(JSRuntime *rt, JSValueConst val,
                                JS_MarkFunc *mark_func);
static void js_generator_finalizer(JSRuntime *rt, JSValue obj);
static void js_generator_mark(JSRuntime *rt, JSValueConst val,
                                JS_MarkFunc *mark_func);
static void js_global_object_finalizer(JSRuntime *rt, JSValue obj);
static void js_global_object_mark(JSRuntime *rt, JSValueConst val,
                                  JS_MarkFunc *mark_func);
static void js_promise_finalizer(JSRuntime *rt, JSValue val);
static void js_promise_mark(JSRuntime *rt, JSValueConst val,
                                JS_MarkFunc *mark_func);
static void js_promise_resolve_function_finalizer(JSRuntime *rt, JSValue val);
static void js_promise_resolve_function_mark(JSRuntime *rt, JSValueConst val,
                                JS_MarkFunc *mark_func);

#define HINT_STRING  0
#define HINT_NUMBER  1
#define HINT_NONE    2
#define HINT_FORCE_ORDINARY (1 << 4) // don't try Symbol.toPrimitive
static JSValue JS_ToPrimitiveFree(JSContext *ctx, JSValue val, int hint);
static JSValue JS_ToStringFree(JSContext *ctx, JSValue val);
static int JS_ToBoolFree(JSContext *ctx, JSValue val);
static int JS_ToInt32Free(JSContext *ctx, int32_t *pres, JSValue val);
static int JS_ToFloat64Free(JSContext *ctx, double *pres, JSValue val);
static int JS_ToUint8ClampFree(JSContext *ctx, int32_t *pres, JSValue val);
static JSValue js_new_string8_len(JSContext *ctx, const char *buf, int len);
static JSValue js_compile_regexp(JSContext *ctx, JSValueConst pattern,
                                 JSValueConst flags);
static JSValue JS_NewRegexp(JSContext *ctx, JSValue pattern, JSValue bc);
static void gc_decref(JSRuntime *rt);
static int JS_NewClass1(JSRuntime *rt, JSClassID class_id,
                        const JSClassDef *class_def, JSAtom name);

typedef enum JSStrictEqModeEnum {
    JS_EQ_STRICT,
    JS_EQ_SAME_VALUE,
    JS_EQ_SAME_VALUE_ZERO,
} JSStrictEqModeEnum;

static BOOL js_strict_eq2(JSContext *ctx, JSValueConst op1, JSValueConst op2,
                          JSStrictEqModeEnum eq_mode);
static BOOL js_strict_eq(JSContext *ctx, JSValueConst op1, JSValueConst op2);
static BOOL js_same_value(JSContext *ctx, JSValueConst op1, JSValueConst op2);
static BOOL js_same_value_zero(JSContext *ctx, JSValueConst op1, JSValueConst op2);
static JSValue JS_ToObject(JSContext *ctx, JSValueConst val);
static JSValue JS_ToObjectFree(JSContext *ctx, JSValue val);
static JSProperty *add_property(JSContext *ctx,
                                JSObject *p, JSAtom prop, int prop_flags);
static void free_property(JSRuntime *rt, JSProperty *pr, int prop_flags);
static int JS_ToBigInt64Free(JSContext *ctx, int64_t *pres, JSValue val);
JSValue JS_ThrowOutOfMemory(JSContext *ctx);
static JSValue JS_ThrowTypeErrorRevokedProxy(JSContext *ctx);

static int js_resolve_proxy(JSContext *ctx, JSValueConst *pval, int throw_exception);
static int JS_CreateProperty(JSContext *ctx, JSObject *p,
                             JSAtom prop, JSValueConst val,
                             JSValueConst getter, JSValueConst setter,
                             int flags);
static int js_string_memcmp(const JSString *p1, int pos1, const JSString *p2,
                            int pos2, int len);
static JSValue js_array_buffer_constructor3(JSContext *ctx,
                                            JSValueConst new_target,
                                            uint64_t len, uint64_t *max_len,
                                            JSClassID class_id,
                                            uint8_t *buf,
                                            JSFreeArrayBufferDataFunc *free_func,
                                            void *opaque, BOOL alloc_flag);
static void js_array_buffer_free(JSRuntime *rt, void *opaque, void *ptr);
static JSArrayBuffer *js_get_array_buffer(JSContext *ctx, JSValueConst obj);
static BOOL array_buffer_is_resizable(const JSArrayBuffer *abuf);
static JSValue js_typed_array_constructor(JSContext *ctx,
                                          JSValueConst this_val,
                                          int argc, JSValueConst *argv,
                                          int classid);
static JSValue js_typed_array_constructor_ta(JSContext *ctx,
                                             JSValueConst new_target,
                                             JSValueConst src_obj,
                                             int classid, uint32_t len);
static BOOL typed_array_is_oob(JSObject *p);
static int js_typed_array_get_length_unsafe(JSContext *ctx, JSValueConst obj);
static JSValue JS_ThrowTypeErrorDetachedArrayBuffer(JSContext *ctx);
static JSValue JS_ThrowTypeErrorArrayBufferOOB(JSContext *ctx);
static JSVarRef *js_create_var_ref(JSContext *ctx, BOOL is_lexical);
static JSVarRef *get_var_ref(JSContext *ctx, JSStackFrame *sf, int var_idx,
                             BOOL is_arg);
static void __async_func_free(JSRuntime *rt, JSAsyncFunctionState *s);
static void async_func_free(JSRuntime *rt, JSAsyncFunctionState *s);
static JSValue js_generator_function_call(JSContext *ctx, JSValueConst func_obj,
                                          JSValueConst this_obj,
                                          int argc, JSValueConst *argv,
                                          int flags);
static void js_async_function_resolve_finalizer(JSRuntime *rt, JSValue val);
static void js_async_function_resolve_mark(JSRuntime *rt, JSValueConst val,
                                           JS_MarkFunc *mark_func);
static JSValue JS_EvalInternal(JSContext *ctx, JSValueConst this_obj,
                               const char *input, size_t input_len,
                               const char *filename, int flags, int scope_idx);
static void js_free_module_def(JSRuntime *rt, JSModuleDef *m);
static void js_mark_module_def(JSRuntime *rt, JSModuleDef *m,
                               JS_MarkFunc *mark_func);
static JSValue js_import_meta(JSContext *ctx);
static JSValue js_dynamic_import(JSContext *ctx, JSValueConst specifier, JSValueConst options);
static void free_var_ref(JSRuntime *rt, JSVarRef *var_ref);
static JSValue js_new_promise_capability(JSContext *ctx,
                                         JSValue *resolving_funcs,
                                         JSValueConst ctor);
static __exception int perform_promise_then(JSContext *ctx,
                                            JSValueConst promise,
                                            JSValueConst *resolve_reject,
                                            JSValueConst *cap_resolving_funcs);
static JSValue js_promise_resolve(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv, int magic);
static JSValue js_promise_then(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv);
static BOOL js_string_eq(JSContext *ctx,
                         const JSString *p1, const JSString *p2);
static int js_string_compare(JSContext *ctx,
                             const JSString *p1, const JSString *p2);
static JSValue JS_ToNumber(JSContext *ctx, JSValueConst val);
static int JS_SetPropertyValue(JSContext *ctx, JSValueConst this_obj,
                               JSValue prop, JSValue val, int flags);
static int JS_NumberIsInteger(JSContext *ctx, JSValueConst val);
static BOOL JS_NumberIsNegativeOrMinusZero(JSContext *ctx, JSValueConst val);
static JSValue JS_ToNumberFree(JSContext *ctx, JSValue val);
static int JS_GetOwnPropertyInternal(JSContext *ctx, JSPropertyDescriptor *desc,
                                     JSObject *p, JSAtom prop);
static void js_free_desc(JSContext *ctx, JSPropertyDescriptor *desc);
static int JS_AddIntrinsicBasicObjects(JSContext *ctx);
static void js_free_shape(JSRuntime *rt, JSShape *sh);
static void js_free_shape_null(JSRuntime *rt, JSShape *sh);
static int js_shape_prepare_update(JSContext *ctx, JSObject *p,
                                   JSShapeProperty **pprs);
static int init_shape_hash(JSRuntime *rt);
static __exception int js_get_length32(JSContext *ctx, uint32_t *pres,
                                       JSValueConst obj);
static __exception int js_get_length64(JSContext *ctx, int64_t *pres,
                                       JSValueConst obj);
static void free_arg_list(JSContext *ctx, JSValue *tab, uint32_t len);
static JSValue *build_arg_list(JSContext *ctx, uint32_t *plen,
                               JSValueConst array_arg);
static BOOL js_get_fast_array(JSContext *ctx, JSValueConst obj,
                              JSValue **arrpp, uint32_t *countp);
static JSValue JS_CreateAsyncFromSyncIterator(JSContext *ctx,
                                              JSValueConst sync_iter);
static void js_c_function_data_finalizer(JSRuntime *rt, JSValue val);
static void js_c_function_data_mark(JSRuntime *rt, JSValueConst val,
                                    JS_MarkFunc *mark_func);
static JSValue js_c_function_data_call(JSContext *ctx, JSValueConst func_obj,
                                       JSValueConst this_val,
                                       int argc, JSValueConst *argv, int flags);
static JSAtom js_symbol_to_atom(JSContext *ctx, JSValue val);
static void add_gc_object(JSRuntime *rt, JSGCObjectHeader *h,
                          JSGCObjectTypeEnum type);
static void remove_gc_object(JSGCObjectHeader *h);
static JSValue js_instantiate_prototype(JSContext *ctx, JSObject *p, JSAtom atom, void *opaque);
static JSValue js_module_ns_autoinit(JSContext *ctx, JSObject *p, JSAtom atom,
                                 void *opaque);
static JSValue JS_InstantiateFunctionListItem2(JSContext *ctx, JSObject *p,
                                               JSAtom atom, void *opaque);
static JSValue js_object_groupBy(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv, int is_map);
static void map_delete_weakrefs(JSRuntime *rt, JSWeakRefHeader *wh);
static void weakref_delete_weakref(JSRuntime *rt, JSWeakRefHeader *wh);
static void finrec_delete_weakref(JSRuntime *rt, JSWeakRefHeader *wh);
static void JS_RunGCInternal(JSRuntime *rt, BOOL remove_weak_objects);
static JSValue js_array_from_iterator(JSContext *ctx, uint32_t *plen,
                                      JSValueConst obj, JSValueConst method);
static int js_string_find_invalid_codepoint(JSString *p);
static JSValue js_regexp_toString(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv);
static JSValue get_date_string(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv, int magic);
static JSValue js_error_toString(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv);
static JSVarRef *js_global_object_find_uninitialized_var(JSContext *ctx, JSObject *p,
                                                         JSAtom atom, BOOL is_lexical);
static int typed_array_init(JSContext *ctx, JSValueConst obj,
                            JSValue buffer, uint64_t offset, uint64_t len,
                            BOOL track_rab);


static const JSClassExoticMethods js_arguments_exotic_methods;
static const JSClassExoticMethods js_string_exotic_methods;
static const JSClassExoticMethods js_proxy_exotic_methods;
static const JSClassExoticMethods js_module_ns_exotic_methods;
static JSClassID js_class_id_alloc = JS_CLASS_INIT_COUNT;

/* JS malloc */

/* max overhead for size >= 64: 12.5% */
static const uint16_t js_malloc_block_sizes[JS_MALLOC_BLOCK_SIZE_COUNT] = {
    16,
    24,
    32,
    40,
    48,
    56,
    64,
    72,
    80,
    88,
    96,
    104,
    112,
    120,
    128,
    144,
    160,
    176,
    192,
    208,
    224,
    240,
    256,
    288,
    320,
    352,
    384,
    416,
    448,
    480,
    512,
};

static int get_block_size_index(size_t size)
{
    if (size <= 16) {
        return 0;
    } else if (size <= 128) {
        return (size + 7) / 8 - 2;
    } else if (size <= 256) {
        return (size + 15) / 16 + 6;
    } else if (size <= 512) {
        return (size + 31) / 32 + 14;
    } else {
        return JS_MALLOC_BLOCK_SIZE_COUNT;
    }
}

static JSMallocBlockHeader *get_zero_size_block(JSMallocContext *s)
{
    return (JSMallocBlockHeader *)s->zero_size_block;
}

static void js_malloc_init(JSMallocContext *s)
{
    int i;
    memset(s, 0, sizeof(*s));
    get_zero_size_block(s)->u.block_idx = FREE_NIL;
    for(i = 0; i < JS_MALLOC_BLOCK_SIZE_COUNT; i++) {
        init_list_head(&s->arena_list[i]);
        init_list_head(&s->free_arena_list[i]);
    }
#ifdef JS_MALLOC_USE_ITER
    init_list_head(&s->large_block_list);
#endif
}

static void *get_arena_block(JSMallocArena *ar, unsigned int idx, unsigned int block_size)
{
    return ar->blocks + idx * block_size;
}

static inline JSMallocBlockHeader *js_rc(void *ptr)
{
    return container_of(ptr, JSMallocBlockHeader, user_data);
}

static no_inline JSMallocArena *js_malloc_new_arena(JSMallocContext *s, int block_size_idx)
{
    JSMallocBlockHeader *b;
    JSMallocArena *ar;
    int n_blocks, block_size, i;

    block_size = js_malloc_block_sizes[block_size_idx];
    n_blocks = (JS_MALLOC_ARENA_SIZE - sizeof(JSMallocArena)) / block_size;
    ar = s->mf.js_malloc(&s->malloc_state, sizeof(JSMallocArena) + n_blocks * block_size);
    if (!ar)
        return NULL;

    ar->block_size_idx = block_size_idx;
    ar->n_blocks = n_blocks;
    ar->n_used_blocks = 0;
    ar->first_free_block = 0;
#ifdef JS_MALLOC_USE_ITER
    {
        int n_bitmap_words = (n_blocks + 31) / 32;
        for(i = 0; i < n_bitmap_words; i++)
            ar->bitmap[i] = 0;
    }
#endif
    for(i = 0; i < n_blocks - 1; i++) {
        b = get_arena_block(ar, i, block_size);
        b->u.free_next = i + 1;
        b->block_size_idx = block_size_idx;
    }
    b = get_arena_block(ar, n_blocks - 1, block_size);
    b->u.free_next = FREE_NIL;
    b->block_size_idx = block_size_idx;

    /* add to the head */
    list_add(&ar->link, &s->arena_list[block_size_idx]);
    list_add(&ar->free_link, &s->free_arena_list[block_size_idx]);
    return ar;
}

static no_inline void *js_malloc_large(JSMallocContext *s, size_t size)
{
    JSMallocLargeBlockHeader *b;
    b = s->mf.js_malloc(&s->malloc_state, sizeof(JSMallocLargeBlockHeader) + size);
    if (!b)
        return NULL;
    b->header.u.block_idx = FREE_NIL;
    b->header.block_size_idx = 0xff; /* fail safe */
#ifdef JS_MALLOC_USE_ITER
    list_add_tail(&b->link, &s->large_block_list);
#endif
    return b->header.user_data;
}

static void *__js_malloc(JSMallocContext *s, size_t size)
{
    size_t total_size;
    if (unlikely(size == 0)) {
        JSMallocBlockHeader *b = get_zero_size_block(s);
        return b->user_data;
    } else {
        total_size = ((size + JS_MALLOC_ALIGN - 1) & ~(JS_MALLOC_ALIGN - 1)) +
            sizeof(JSMallocBlockHeader);
        if (!JS_MALLOC_LARGE_BLOCKS_ONLY &&
            total_size <= JS_MALLOC_MAX_SMALL_SIZE) {
            int block_size_idx;
            unsigned int block_idx, block_size;
            JSMallocBlockHeader *b;
            JSMallocArena *ar;
            struct list_head *el, *head;

            block_size_idx = get_block_size_index(total_size);
            block_size = js_malloc_block_sizes[block_size_idx];
            head = &s->free_arena_list[block_size_idx];
            el = head->next;
            if (unlikely(el == head)) {
                ar = js_malloc_new_arena(s, block_size_idx);
                if (!ar)
                    return NULL;
            } else {
                ar = list_entry(el, JSMallocArena, free_link);
            }
            block_idx = ar->first_free_block;
            b = get_arena_block(ar, ar->first_free_block, block_size);
            ar->first_free_block = b->u.free_next;
            b->u.block_idx = block_idx;
            ar->n_used_blocks++;
            if (unlikely(ar->n_used_blocks == ar->n_blocks)) {
                list_del(&ar->free_link);
            }
#ifdef JS_MALLOC_USE_ITER
            ar->bitmap[block_idx / 32] |= 1 << (block_idx % 32);
#endif
            return b->user_data;
        } else {
            return js_malloc_large(s, size);
        }
    }
}

static void __js_free(JSMallocContext *s, void *ptr)
{
    JSMallocBlockHeader *b;

    if (!ptr)
        return;
    b = container_of(ptr, JSMallocBlockHeader, user_data);
    if (unlikely(b->u.block_idx == FREE_NIL)) {
        /* large or zero size block */
        if (b == get_zero_size_block(s)) {
            /* nothing to do */
        } else {
            JSMallocLargeBlockHeader *lb = container_of(ptr, JSMallocLargeBlockHeader, header.user_data);
#ifdef JS_MALLOC_USE_ITER
            list_del(&lb->link);
#endif
            s->mf.js_free(&s->malloc_state, lb);
        }
    } else {
        unsigned int block_idx = b->u.block_idx;
        unsigned int block_size_idx = b->block_size_idx;
        unsigned int block_size = js_malloc_block_sizes[block_size_idx];
        JSMallocArena *ar = (JSMallocArena *)((uint8_t *)b - block_size * block_idx - sizeof(JSMallocArena));
        b->u.free_next = ar->first_free_block;
        ar->first_free_block = block_idx;
#ifdef JS_MALLOC_USE_ITER
        ar->bitmap[block_idx / 32] &= ~(1 << (block_idx % 32));
#endif
        /* add back to the free list if needed */
        if (unlikely(ar->n_used_blocks == ar->n_blocks)) {
            list_add(&ar->free_link, &s->free_arena_list[block_size_idx]);
        }
        ar->n_used_blocks--;
        if (unlikely(ar->n_used_blocks == 0)) {
            list_del(&ar->link);
            list_del(&ar->free_link);
            s->mf.js_free(&s->malloc_state, ar);
        }
    }
}

static void *__js_realloc(JSMallocContext *s, void *ptr, size_t size)
{
    JSMallocBlockHeader *b;
    if (ptr == NULL) {
        return __js_malloc(s, size);
    } else if (size == 0) {
        __js_free(s, ptr);
        return NULL;
    }
    b = container_of(ptr, JSMallocBlockHeader, user_data);
    if (b->u.block_idx == FREE_NIL) {
        if (b == get_zero_size_block(s)) {
            return __js_malloc(s, size);
        } else {
            JSMallocLargeBlockHeader *lb, *new_lb;
            lb = container_of(ptr, JSMallocLargeBlockHeader, header.user_data);
#ifdef JS_MALLOC_USE_ITER
            list_del(&lb->link);
#endif
            new_lb = s->mf.js_realloc(&s->malloc_state, lb, sizeof(JSMallocLargeBlockHeader) + size);
            if (!new_lb) {
#ifdef JS_MALLOC_USE_ITER
                /* add again in the list */
                list_add_tail(&lb->link, &s->large_block_list);
#endif
                return NULL;
            }
            new_lb->header.u.block_idx = FREE_NIL;
            new_lb->header.block_size_idx = 0xff; /* fail safe */
#ifdef JS_MALLOC_USE_ITER
            list_add_tail(&new_lb->link, &s->large_block_list);
#endif
            return new_lb->header.user_data;
        }
    } else {
        unsigned int block_size_idx = b->block_size_idx;
        size_t block_size = js_malloc_block_sizes[block_size_idx];
        size_t total_size, old_size;
        void *new_ptr;
        JSMallocBlockHeader *new_b;

        total_size = ((size + JS_MALLOC_ALIGN - 1) & ~(JS_MALLOC_ALIGN - 1)) +
            sizeof(JSMallocBlockHeader);
        if (total_size <= block_size)
            return ptr;
        new_ptr = __js_malloc(s, size);
        if (!new_ptr)
            return NULL;
        new_b = container_of(new_ptr, JSMallocBlockHeader, user_data);
        /* copy the GC data */
        new_b->gc_obj_type = b->gc_obj_type;
        new_b->mark = b->mark;
        new_b->ref_count = b->ref_count;
        /* copy the data */
        old_size = block_size - sizeof(JSMallocBlockHeader);
        if (size > old_size)
            size = old_size;
        memcpy(new_ptr, ptr, size);
        __js_free(s, ptr);
        return new_ptr;
    }
}

static size_t __js_malloc_usable_size(JSMallocContext *s, const char *ptr)
{
    JSMallocBlockHeader *b;
    if (!ptr)
        return 0;
    b = container_of(ptr, JSMallocBlockHeader, user_data);
    if (b->u.block_idx == FREE_NIL) {
        if (b == get_zero_size_block(s)) {
            return 0;
        } else {
            JSMallocLargeBlockHeader *lb;
            size_t size;
            lb = container_of(ptr, JSMallocLargeBlockHeader, header.user_data);
            if (s->mf.js_malloc_usable_size) {
                size = s->mf.js_malloc_usable_size(lb);
                if (size != 0)
                    size -= sizeof(JSMallocLargeBlockHeader);
                return size;
            } else {
                return 0;
            }
        }
    } else {
        size_t block_size = js_malloc_block_sizes[b->block_size_idx];
        return block_size - sizeof(*b);
    }
}

static __maybe_unused void js_malloc_dump_arenas(JSMallocContext *s)
{
    struct list_head *el;
    int block_size_idx;

    printf("%20s %10s %10s\n", "PTR", "BLK_SIZE", "ALLOC");
    for(block_size_idx = 0; block_size_idx < JS_MALLOC_BLOCK_SIZE_COUNT; block_size_idx++) {
        int block_size = js_malloc_block_sizes[block_size_idx];
        list_for_each(el, &s->arena_list[block_size_idx]) {
            JSMallocArena *ar = list_entry(el, JSMallocArena, link);
            printf("%20p %10u %9.1f%%\n",
                   ar, block_size,
                   (double)ar->n_used_blocks / ar->n_blocks * 100);
        }
    }
}

#ifdef JS_MALLOC_USE_ITER
typedef void JSMallocIterFunc(void *opaque, void *ptr);

/* iterate thru allocated blocks. The allocated block list should not
   be modified while iterating. */
static __maybe_unused void js_malloc_iter(JSMallocContext *s, JSMallocIterFunc *iter_func, void *iter_opaque)
{
    struct list_head *el;
    int block_size_idx;
    int i, j, n_words;
    uint32_t bmp;

    for(block_size_idx = 0; block_size_idx < JS_MALLOC_BLOCK_SIZE_COUNT; block_size_idx++) {
        unsigned int block_size = js_malloc_block_sizes[block_size_idx];
        list_for_each(el, &s->arena_list[block_size_idx]) {
            JSMallocArena *ar = list_entry(el, JSMallocArena, link);
            n_words = (ar->n_blocks + 31) / 32;
            for(i = 0; i < n_words; i++) {
                bmp = ar->bitmap[i];
                while (bmp != 0) {
                    j = ctz32(bmp);
                    bmp &= ~(1 << j);
                    iter_func(iter_opaque, get_arena_block(ar, i * 32+ j, block_size));
                }
            }
        }
    }
    list_for_each(el, &s->large_block_list) {
        JSMallocLargeBlockHeader *lb = list_entry(el, JSMallocLargeBlockHeader, link);
        iter_func(iter_opaque, lb->header.user_data);
    }
}
#endif

/* end JS malloc */

static void js_trigger_gc(JSRuntime *rt, size_t size)
{
    BOOL force_gc;
#ifdef FORCE_GC_AT_MALLOC
    force_gc = TRUE;
#else
    force_gc = ((rt->malloc_ctx.malloc_state.malloc_size + size) >
                rt->malloc_gc_threshold);
#endif
    if (force_gc) {
#ifdef DUMP_GC
        printf("GC: size=%" PRIu64 "\n",
               (uint64_t)rt->malloc_ctx.malloc_state.malloc_size);
#endif
        JS_RunGC(rt);
        rt->malloc_gc_threshold = rt->malloc_ctx.malloc_state.malloc_size +
            (rt->malloc_ctx.malloc_state.malloc_size >> 1);
    }
}

void *js_malloc_rt(JSRuntime *rt, size_t size)
{
    return __js_malloc(&rt->malloc_ctx, size);
}

void js_free_rt(JSRuntime *rt, void *ptr)
{
    __js_free(&rt->malloc_ctx, ptr);
}

void *js_realloc_rt(JSRuntime *rt, void *ptr, size_t size)
{
    return __js_realloc(&rt->malloc_ctx, ptr, size);
}

size_t js_malloc_usable_size_rt(JSRuntime *rt, const void *ptr)
{
    return __js_malloc_usable_size(&rt->malloc_ctx, ptr);
}

void *js_mallocz_rt(JSRuntime *rt, size_t size)
{
    void *ptr;
    ptr = js_malloc_rt(rt, size);
    if (unlikely(!ptr))
        return NULL;
    return memset(ptr, 0, size);
}

/* Throw out of memory in case of error */
void *js_malloc(JSContext *ctx, size_t size)
{
    void *ptr;
    ptr = js_malloc_rt(ctx->rt, size);
    if (unlikely(!ptr)) {
        JS_ThrowOutOfMemory(ctx);
        return NULL;
    }
    return ptr;
}

/* Throw out of memory in case of error */
void *js_mallocz(JSContext *ctx, size_t size)
{
    void *ptr;
    ptr = js_mallocz_rt(ctx->rt, size);
    if (unlikely(!ptr)) {
        JS_ThrowOutOfMemory(ctx);
        return NULL;
    }
    return ptr;
}

void js_free(JSContext *ctx, void *ptr)
{
    js_free_rt(ctx->rt, ptr);
}

/* Throw out of memory in case of error */
void *js_realloc(JSContext *ctx, void *ptr, size_t size)
{
    void *ret;
    ret = js_realloc_rt(ctx->rt, ptr, size);
    if (unlikely(!ret && size != 0)) {
        JS_ThrowOutOfMemory(ctx);
        return NULL;
    }
    return ret;
}

/* store extra allocated size in *pslack if successful */
void *js_realloc2(JSContext *ctx, void *ptr, size_t size, size_t *pslack)
{
    void *ret;
    ret = js_realloc_rt(ctx->rt, ptr, size);
    if (unlikely(!ret && size != 0)) {
        JS_ThrowOutOfMemory(ctx);
        return NULL;
    }
    if (pslack) {
        size_t new_size = js_malloc_usable_size_rt(ctx->rt, ret);
        *pslack = (new_size > size) ? new_size - size : 0;
    }
    return ret;
}

size_t js_malloc_usable_size(JSContext *ctx, const void *ptr)
{
    return js_malloc_usable_size_rt(ctx->rt, ptr);
}

/* Throw out of memory exception in case of error */
char *js_strndup(JSContext *ctx, const char *s, size_t n)
{
    char *ptr;
    ptr = js_malloc(ctx, n + 1);
    if (ptr) {
        memcpy(ptr, s, n);
        ptr[n] = '\0';
    }
    return ptr;
}

char *js_strdup(JSContext *ctx, const char *str)
{
    return js_strndup(ctx, str, strlen(str));
}

static no_inline int js_realloc_array(JSContext *ctx, void **parray,
                                      int elem_size, int *psize, int req_size)
{
    int new_size;
    size_t slack;
    void *new_array;
    /* XXX: potential arithmetic overflow */
    new_size = max_int(req_size, *psize * 3 / 2);
    new_array = js_realloc2(ctx, *parray, new_size * elem_size, &slack);
    if (!new_array)
        return -1;
    new_size += slack / elem_size;
    *psize = new_size;
    *parray = new_array;
    return 0;
}

/* resize the array and update its size if req_size > *psize */
static inline int js_resize_array(JSContext *ctx, void **parray, int elem_size,
                                  int *psize, int req_size)
{
    if (unlikely(req_size > *psize))
        return js_realloc_array(ctx, parray, elem_size, psize, req_size);
    else
        return 0;
}

static inline void js_dbuf_init(JSContext *ctx, DynBuf *s)
{
    dbuf_init2(s, ctx->rt, (DynBufReallocFunc *)js_realloc_rt);
}

static void *js_realloc_bytecode_rt(void *opaque, void *ptr, size_t size)
{
    JSRuntime *rt = opaque;
    if (size > (INT32_MAX / 2)) {
        /* the bytecode cannot be larger than 2G. Leave some slack to
           avoid some overflows. */
        return NULL;
    } else {
        return js_realloc_rt(rt, ptr, size);
    }
}

static inline void js_dbuf_bytecode_init(JSContext *ctx, DynBuf *s)
{
    dbuf_init2(s, ctx->rt, js_realloc_bytecode_rt);
}

static inline int is_digit(int c) {
    return c >= '0' && c <= '9';
}

static inline int string_get(const JSString *p, int idx) {
    return p->is_wide_char ? p->u.str16[idx] : p->u.str8[idx];
}

typedef struct JSClassShortDef {
    JSAtom class_name;
    JSClassFinalizer *finalizer;
    JSClassGCMark *gc_mark;
} JSClassShortDef;

static JSClassShortDef const js_std_class_def[] = {
    { JS_ATOM_Object, NULL, NULL },                             /* JS_CLASS_OBJECT */
    { JS_ATOM_Array, js_array_finalizer, js_array_mark },       /* JS_CLASS_ARRAY */
    { JS_ATOM_Error, NULL, NULL }, /* JS_CLASS_ERROR */
    { JS_ATOM_Number, js_object_data_finalizer, js_object_data_mark }, /* JS_CLASS_NUMBER */
    { JS_ATOM_String, js_object_data_finalizer, js_object_data_mark }, /* JS_CLASS_STRING */
    { JS_ATOM_Boolean, js_object_data_finalizer, js_object_data_mark }, /* JS_CLASS_BOOLEAN */
    { JS_ATOM_Symbol, js_object_data_finalizer, js_object_data_mark }, /* JS_CLASS_SYMBOL */
    { JS_ATOM_Arguments, js_array_finalizer, js_array_mark },   /* JS_CLASS_ARGUMENTS */
    { JS_ATOM_Arguments, js_mapped_arguments_finalizer, js_mapped_arguments_mark }, /* JS_CLASS_MAPPED_ARGUMENTS */
    { JS_ATOM_Date, js_object_data_finalizer, js_object_data_mark }, /* JS_CLASS_DATE */
    { JS_ATOM_Object, NULL, NULL },                             /* JS_CLASS_MODULE_NS */
    { JS_ATOM_Function, js_c_function_finalizer, js_c_function_mark }, /* JS_CLASS_C_FUNCTION */
    { JS_ATOM_Function, js_bytecode_function_finalizer, js_bytecode_function_mark }, /* JS_CLASS_BYTECODE_FUNCTION */
    { JS_ATOM_Function, js_bound_function_finalizer, js_bound_function_mark }, /* JS_CLASS_BOUND_FUNCTION */
    { JS_ATOM_Function, js_c_function_data_finalizer, js_c_function_data_mark }, /* JS_CLASS_C_FUNCTION_DATA */
    { JS_ATOM_GeneratorFunction, js_bytecode_function_finalizer, js_bytecode_function_mark },  /* JS_CLASS_GENERATOR_FUNCTION */
    { JS_ATOM_ForInIterator, js_for_in_iterator_finalizer, js_for_in_iterator_mark },      /* JS_CLASS_FOR_IN_ITERATOR */
    { JS_ATOM_RegExp, js_regexp_finalizer, NULL },                              /* JS_CLASS_REGEXP */
    { JS_ATOM_ArrayBuffer, js_array_buffer_finalizer, NULL },                   /* JS_CLASS_ARRAY_BUFFER */
    { JS_ATOM_SharedArrayBuffer, js_array_buffer_finalizer, NULL },             /* JS_CLASS_SHARED_ARRAY_BUFFER */
    { JS_ATOM_Uint8ClampedArray, js_typed_array_finalizer, js_typed_array_mark }, /* JS_CLASS_UINT8C_ARRAY */
    { JS_ATOM_Int8Array, js_typed_array_finalizer, js_typed_array_mark },       /* JS_CLASS_INT8_ARRAY */
    { JS_ATOM_Uint8Array, js_typed_array_finalizer, js_typed_array_mark },      /* JS_CLASS_UINT8_ARRAY */
    { JS_ATOM_Int16Array, js_typed_array_finalizer, js_typed_array_mark },      /* JS_CLASS_INT16_ARRAY */
    { JS_ATOM_Uint16Array, js_typed_array_finalizer, js_typed_array_mark },     /* JS_CLASS_UINT16_ARRAY */
    { JS_ATOM_Int32Array, js_typed_array_finalizer, js_typed_array_mark },      /* JS_CLASS_INT32_ARRAY */
    { JS_ATOM_Uint32Array, js_typed_array_finalizer, js_typed_array_mark },     /* JS_CLASS_UINT32_ARRAY */
    { JS_ATOM_BigInt64Array, js_typed_array_finalizer, js_typed_array_mark },   /* JS_CLASS_BIG_INT64_ARRAY */
    { JS_ATOM_BigUint64Array, js_typed_array_finalizer, js_typed_array_mark },  /* JS_CLASS_BIG_UINT64_ARRAY */
    { JS_ATOM_Float16Array, js_typed_array_finalizer, js_typed_array_mark },    /* JS_CLASS_FLOAT16_ARRAY */
    { JS_ATOM_Float32Array, js_typed_array_finalizer, js_typed_array_mark },    /* JS_CLASS_FLOAT32_ARRAY */
    { JS_ATOM_Float64Array, js_typed_array_finalizer, js_typed_array_mark },    /* JS_CLASS_FLOAT64_ARRAY */
    { JS_ATOM_DataView, js_typed_array_finalizer, js_typed_array_mark },        /* JS_CLASS_DATAVIEW */
    { JS_ATOM_BigInt, js_object_data_finalizer, js_object_data_mark },      /* JS_CLASS_BIG_INT */
    { JS_ATOM_Map, js_map_finalizer, js_map_mark },             /* JS_CLASS_MAP */
    { JS_ATOM_Set, js_map_finalizer, js_map_mark },             /* JS_CLASS_SET */
    { JS_ATOM_WeakMap, js_map_finalizer, js_map_mark },         /* JS_CLASS_WEAKMAP */
    { JS_ATOM_WeakSet, js_map_finalizer, js_map_mark },         /* JS_CLASS_WEAKSET */
    { JS_ATOM_Iterator, NULL, NULL },                           /* JS_CLASS_ITERATOR */
    { JS_ATOM_IteratorConcat, js_iterator_concat_finalizer, js_iterator_concat_mark }, /* JS_CLASS_ITERATOR_CONCAT */
    { JS_ATOM_IteratorHelper, js_iterator_helper_finalizer, js_iterator_helper_mark }, /* JS_CLASS_ITERATOR_HELPER */
    { JS_ATOM_IteratorWrap, js_iterator_wrap_finalizer, js_iterator_wrap_mark }, /* JS_CLASS_ITERATOR_WRAP */
    { JS_ATOM_Map_Iterator, js_map_iterator_finalizer, js_map_iterator_mark }, /* JS_CLASS_MAP_ITERATOR */
    { JS_ATOM_Set_Iterator, js_map_iterator_finalizer, js_map_iterator_mark }, /* JS_CLASS_SET_ITERATOR */
    { JS_ATOM_Array_Iterator, js_array_iterator_finalizer, js_array_iterator_mark }, /* JS_CLASS_ARRAY_ITERATOR */
    { JS_ATOM_String_Iterator, js_array_iterator_finalizer, js_array_iterator_mark }, /* JS_CLASS_STRING_ITERATOR */
    { JS_ATOM_RegExp_String_Iterator, js_regexp_string_iterator_finalizer, js_regexp_string_iterator_mark }, /* JS_CLASS_REGEXP_STRING_ITERATOR */
    { JS_ATOM_Generator, js_generator_finalizer, js_generator_mark }, /* JS_CLASS_GENERATOR */
    { JS_ATOM_Object, js_global_object_finalizer, js_global_object_mark }, /* JS_CLASS_GLOBAL_OBJECT */
    { JS_ATOM_Object, NULL, NULL }, /* JS_CLASS_RAWJSON */
};

static int init_class_range(JSRuntime *rt, JSClassShortDef const *tab,
                            int start, int count)
{
    JSClassDef cm_s, *cm = &cm_s;
    int i, class_id;

    for(i = 0; i < count; i++) {
        class_id = i + start;
        memset(cm, 0, sizeof(*cm));
        cm->finalizer = tab[i].finalizer;
        cm->gc_mark = tab[i].gc_mark;
        if (JS_NewClass1(rt, class_id, cm, tab[i].class_name) < 0)
            return -1;
    }
    return 0;
}

#if !defined(CONFIG_STACK_CHECK)
/* no stack limitation */
static inline uintptr_t js_get_stack_pointer(void)
{
    return 0;
}

static inline BOOL js_check_stack_overflow(JSRuntime *rt, size_t alloca_size)
{
    return FALSE;
}
#else
/* Note: OS and CPU dependent */
static inline uintptr_t js_get_stack_pointer(void)
{
    return (uintptr_t)__builtin_frame_address(0);
}

static inline BOOL js_check_stack_overflow(JSRuntime *rt, size_t alloca_size)
{
    uintptr_t sp;
    sp = js_get_stack_pointer() - alloca_size;
    return unlikely(sp < rt->stack_limit);
}
#endif

JSRuntime *JS_NewRuntime2(const JSMallocFunctions *mf, void *opaque)
{
    JSRuntime *rt;
    JSMallocState ms;

    memset(&ms, 0, sizeof(ms));
    ms.opaque = opaque;
    ms.malloc_limit = -1;

    rt = mf->js_malloc(&ms, sizeof(JSRuntime));
    if (!rt)
        return NULL;
    memset(rt, 0, sizeof(*rt));
    js_malloc_init(&rt->malloc_ctx);
    rt->malloc_ctx.mf = *mf;
    rt->malloc_ctx.malloc_state = ms;
    rt->malloc_gc_threshold = 256 * 1024;

    init_list_head(&rt->context_list);
    init_list_head(&rt->gc_obj_list);
    init_list_head(&rt->gc_zero_ref_count_list);
    rt->gc_phase = JS_GC_PHASE_NONE;
    init_list_head(&rt->weakref_list);

#ifdef DUMP_LEAKS
    init_list_head(&rt->string_list);
#endif
    init_list_head(&rt->job_list);

    if (JS_InitAtoms(rt))
        goto fail;

    /* create the object, array and function classes */
    if (init_class_range(rt, js_std_class_def, JS_CLASS_OBJECT,
                         countof(js_std_class_def)) < 0)
        goto fail;
    rt->class_array[JS_CLASS_ARGUMENTS].exotic = &js_arguments_exotic_methods;
    rt->class_array[JS_CLASS_MAPPED_ARGUMENTS].exotic = &js_arguments_exotic_methods;
    rt->class_array[JS_CLASS_STRING].exotic = &js_string_exotic_methods;
    rt->class_array[JS_CLASS_MODULE_NS].exotic = &js_module_ns_exotic_methods;

    rt->class_array[JS_CLASS_C_FUNCTION].call = js_call_c_function;
    rt->class_array[JS_CLASS_C_FUNCTION_DATA].call = js_c_function_data_call;
    rt->class_array[JS_CLASS_BOUND_FUNCTION].call = js_call_bound_function;
    rt->class_array[JS_CLASS_GENERATOR_FUNCTION].call = js_generator_function_call;
    if (init_shape_hash(rt))
        goto fail;

    rt->stack_size = JS_DEFAULT_STACK_SIZE;
    JS_UpdateStackTop(rt);

    rt->current_exception = JS_UNINITIALIZED;

    return rt;
 fail:
    JS_FreeRuntime(rt);
    return NULL;
}

void *JS_GetRuntimeOpaque(JSRuntime *rt)
{
    return rt->user_opaque;
}

void JS_SetRuntimeOpaque(JSRuntime *rt, void *opaque)
{
    rt->user_opaque = opaque;
}

/* default memory allocation functions with memory limitation */
static size_t js_def_malloc_usable_size(const void *ptr)
{
#if defined(MR_QUICKJS_PORTABLE_LIBC)
    return ((const size_t *)ptr)[-1];
#elif defined(__APPLE__)
    return malloc_size(ptr);
#elif defined(_WIN32)
    return _msize((void *)ptr);
#elif defined(__EMSCRIPTEN__)
    return 0;
#elif defined(__linux__) || defined(__GLIBC__)
    return malloc_usable_size((void *)ptr);
#else
    /* change this to `return 0;` if compilation fails */
    return malloc_usable_size((void *)ptr);
#endif
}

static void *js_def_malloc(JSMallocState *s, size_t size)
{
    void *ptr;

    /* Do not allocate zero bytes: behavior is platform dependent */
    assert(size != 0);

    if (unlikely(s->malloc_size + size > s->malloc_limit))
        return NULL;

#ifdef MR_QUICKJS_PORTABLE_LIBC
    {
        size_t *block;
        if (size > SIZE_MAX - sizeof(*block))
            return NULL;
        block = malloc(size + sizeof(*block));
        if (!block)
            return NULL;
        *block = size;
        ptr = block + 1;
    }
#else
    ptr = malloc(size);
#endif
    if (!ptr)
        return NULL;

    s->malloc_count++;
    s->malloc_size += js_def_malloc_usable_size(ptr) + MALLOC_OVERHEAD;
    return ptr;
}

static void js_def_free(JSMallocState *s, void *ptr)
{
    if (!ptr)
        return;

    s->malloc_count--;
    s->malloc_size -= js_def_malloc_usable_size(ptr) + MALLOC_OVERHEAD;
#ifdef MR_QUICKJS_PORTABLE_LIBC
    free((size_t *)ptr - 1);
#else
    free(ptr);
#endif
}

static void *js_def_realloc(JSMallocState *s, void *ptr, size_t size)
{
    size_t old_size;

    if (!ptr) {
        if (size == 0)
            return NULL;
        return js_def_malloc(s, size);
    }
    old_size = js_def_malloc_usable_size(ptr);
    if (size == 0) {
        s->malloc_count--;
        s->malloc_size -= old_size + MALLOC_OVERHEAD;
        free(ptr);
        return NULL;
    }
    if (s->malloc_size + size - old_size > s->malloc_limit)
        return NULL;

#ifdef MR_QUICKJS_PORTABLE_LIBC
    {
        size_t *block;
        if (size > SIZE_MAX - sizeof(*block))
            return NULL;
        block = realloc((size_t *)ptr - 1, size + sizeof(*block));
        if (!block)
            return NULL;
        *block = size;
        ptr = block + 1;
    }
#else
    ptr = realloc(ptr, size);
    if (!ptr)
        return NULL;
#endif

    s->malloc_size += js_def_malloc_usable_size(ptr) - old_size;
    return ptr;
}

static const JSMallocFunctions def_malloc_funcs = {
    js_def_malloc,
    js_def_free,
    js_def_realloc,
    js_def_malloc_usable_size,
};

JSRuntime *JS_NewRuntime(void)
{
    return JS_NewRuntime2(&def_malloc_funcs, NULL);
}

void JS_SetMemoryLimit(JSRuntime *rt, size_t limit)
{
    rt->malloc_ctx.malloc_state.malloc_limit = limit;
}

/* use -1 to disable automatic GC */
void JS_SetGCThreshold(JSRuntime *rt, size_t gc_threshold)
{
    rt->malloc_gc_threshold = gc_threshold;
}

#define malloc(s) malloc_is_forbidden(s)
#define free(p) free_is_forbidden(p)
#define realloc(p,s) realloc_is_forbidden(p,s)

void JS_SetInterruptHandler(JSRuntime *rt, JSInterruptHandler *cb, void *opaque)
{
    rt->interrupt_handler = cb;
    rt->interrupt_opaque = opaque;
}

void JS_SetCanBlock(JSRuntime *rt, BOOL can_block)
{
    rt->can_block = can_block;
}

void JS_SetSharedArrayBufferFunctions(JSRuntime *rt,
                                      const JSSharedArrayBufferFunctions *sf)
{
    rt->sab_funcs = *sf;
}

void JS_SetStripInfo(JSRuntime *rt, int flags)
{
    rt->strip_flags = flags;
}

int JS_GetStripInfo(JSRuntime *rt)
{
    return rt->strip_flags;
}

static int JS_EnqueueJob2(JSContext *ctx, JSJobFunc *job_func,
                          int argc, JSValueConst *argv, BOOL no_exception)
{
    JSRuntime *rt = ctx->rt;
    JSJobEntry *e;
    int i;

    if (no_exception)
        e = js_malloc_rt(ctx->rt, sizeof(*e) + argc * sizeof(JSValue));
    else
        e = js_malloc(ctx, sizeof(*e) + argc * sizeof(JSValue));
    if (!e)
        return -1;
    e->realm = JS_DupContext(ctx);
    e->job_func = job_func;
    e->argc = argc;
    for(i = 0; i < argc; i++) {
        e->argv[i] = JS_DupValue(ctx, argv[i]);
    }
    list_add_tail(&e->link, &rt->job_list);
    return 0;
}

/* return 0 if OK, < 0 if exception */
int JS_EnqueueJob(JSContext *ctx, JSJobFunc *job_func,
                  int argc, JSValueConst *argv)
{
    return JS_EnqueueJob2(ctx, job_func, argc, argv, FALSE);
}

BOOL JS_IsJobPending(JSRuntime *rt)
{
    return !list_empty(&rt->job_list);
}

/* return < 0 if exception, 0 if no job pending, 1 if a job was
   executed successfully. The context of the job is stored in '*pctx'
   if pctx != NULL. It may be NULL if the context was already
   destroyed or if no job was pending. The 'pctx' parameter is now
   absolete. */
int JS_ExecutePendingJob(JSRuntime *rt, JSContext **pctx)
{
    JSContext *ctx;
    JSJobEntry *e;
    JSValue res;
    int i, ret;

    if (list_empty(&rt->job_list)) {
        if (pctx)
            *pctx = NULL;
        return 0;
    }

    /* get the first pending job and execute it */
    e = list_entry(rt->job_list.next, JSJobEntry, link);
    list_del(&e->link);
    ctx = e->realm;
    res = e->job_func(ctx, e->argc, (JSValueConst *)e->argv);
    for(i = 0; i < e->argc; i++)
        JS_FreeValue(ctx, e->argv[i]);
    if (JS_IsException(res))
        ret = -1;
    else
        ret = 1;
    JS_FreeValue(ctx, res);
    js_free(ctx, e);
    if (pctx) {
        if (js_rc(ctx)->ref_count > 1)
            *pctx = ctx;
        else
            *pctx = NULL;
    }
    JS_FreeContext(ctx);
    return ret;
}

static inline uint32_t atom_get_free(const JSAtomStruct *p)
{
    return (uintptr_t)p >> 1;
}

static inline BOOL atom_is_free(const JSAtomStruct *p)
{
    return (uintptr_t)p & 1;
}

static inline JSAtomStruct *atom_set_free(uint32_t v)
{
    return (JSAtomStruct *)(((uintptr_t)v << 1) | 1);
}

/* Note: the string contents are uninitialized */
static JSString *js_alloc_string_rt(JSRuntime *rt, int max_len, int is_wide_char)
{
    JSString *str;
    str = js_malloc_rt(rt, sizeof(JSString) + (max_len << is_wide_char) + 1 - is_wide_char);
    if (unlikely(!str))
        return NULL;
    js_rc(str)->ref_count = 1;
    str->is_wide_char = is_wide_char;
    str->len = max_len;
    str->atom_type = 0;
    str->hash = 0;          /* optional but costless */
    str->hash_next = 0;     /* optional */
#ifdef DUMP_LEAKS
    list_add_tail(&str->link, &rt->string_list);
#endif
    return str;
}

static JSString *js_alloc_string(JSContext *ctx, int max_len, int is_wide_char)
{
    JSString *p;
    p = js_alloc_string_rt(ctx->rt, max_len, is_wide_char);
    if (unlikely(!p)) {
        JS_ThrowOutOfMemory(ctx);
        return NULL;
    }
    return p;
}

/* same as JS_FreeValueRT() but faster */
static inline void js_free_string(JSRuntime *rt, JSString *str)
{
    if (--js_rc(str)->ref_count <= 0) {
        if (str->atom_type) {
            JS_FreeAtomStruct(rt, str);
        } else {
#ifdef DUMP_LEAKS
            list_del(&str->link);
#endif
            js_free_rt(rt, str);
        }
    }
}

void JS_SetRuntimeInfo(JSRuntime *rt, const char *s)
{
    if (rt)
        rt->rt_info = s;
}

void JS_FreeRuntime(JSRuntime *rt)
{
    struct list_head *el, *el1;
    int i;

    JS_FreeValueRT(rt, rt->current_exception);

    list_for_each_safe(el, el1, &rt->job_list) {
        JSJobEntry *e = list_entry(el, JSJobEntry, link);
        for(i = 0; i < e->argc; i++)
            JS_FreeValueRT(rt, e->argv[i]);
        JS_FreeContext(e->realm);
        js_free_rt(rt, e);
    }
    init_list_head(&rt->job_list);

    /* don't remove the weak objects to avoid create new jobs with
       FinalizationRegistry */
    JS_RunGCInternal(rt, FALSE);

#ifdef DUMP_LEAKS
    /* leaking objects */
    {
        BOOL header_done;
        JSGCObjectHeader *p;
        int count;

        /* remove the internal refcounts to display only the object
           referenced externally */
        list_for_each(el, &rt->gc_obj_list) {
            p = list_entry(el, JSGCObjectHeader, link);
            js_rc(p)->mark = 0;
        }
        gc_decref(rt);

        header_done = FALSE;
        list_for_each(el, &rt->gc_obj_list) {
            p = list_entry(el, JSGCObjectHeader, link);
            if (js_rc(p)->ref_count != 0) {
                if (!header_done) {
                    printf("Object leaks:\n");
                    JS_DumpObjectHeader(rt);
                    header_done = TRUE;
                }
                JS_DumpGCObject(rt, p);
            }
        }

        count = 0;
        list_for_each(el, &rt->gc_obj_list) {
            p = list_entry(el, JSGCObjectHeader, link);
            if (js_rc(p)->ref_count == 0) {
                count++;
            }
        }
        if (count != 0)
            printf("Secondary object leaks: %d\n", count);
    }
#endif
    assert(list_empty(&rt->gc_obj_list));
    assert(list_empty(&rt->weakref_list));

    /* free the classes */
    for(i = 0; i < rt->class_count; i++) {
        JSClass *cl = &rt->class_array[i];
        if (cl->class_id != 0) {
            JS_FreeAtomRT(rt, cl->class_name);
        }
    }
    js_free_rt(rt, rt->class_array);

#ifdef DUMP_LEAKS
    /* only the atoms defined in JS_InitAtoms() should be left */
    {
        BOOL header_done = FALSE;

        for(i = 0; i < rt->atom_size; i++) {
            JSAtomStruct *p = rt->atom_array[i];
            if (!atom_is_free(p) /* && p->str*/) {
                if (i >= JS_ATOM_END || js_rc(p)->ref_count != 1) {
                    if (!header_done) {
                        header_done = TRUE;
                        if (rt->rt_info) {
                            printf("%s:1: atom leakage:", rt->rt_info);
                        } else {
                            printf("Atom leaks:\n"
                                   "    %6s %6s %s\n",
                                   "ID", "REFCNT", "NAME");
                        }
                    }
                    if (rt->rt_info) {
                        printf(" ");
                    } else {
                        printf("    %6u %6u ", i, js_rc(p)->ref_count);
                    }
                    switch (p->atom_type) {
                    case JS_ATOM_TYPE_STRING:
                        JS_DumpString(rt, p);
                        break;
                    case JS_ATOM_TYPE_GLOBAL_SYMBOL:
                        printf("Symbol.for(");
                        JS_DumpString(rt, p);
                        printf(")");
                        break;
                    case JS_ATOM_TYPE_SYMBOL:
                        if (p->hash != JS_ATOM_HASH_PRIVATE) {
                            printf("Symbol(");
                            JS_DumpString(rt, p);
                            printf(")");
                        } else {
                            printf("Private(");
                            JS_DumpString(rt, p);
                            printf(")");
                        }
                        break;
                    }
                    if (rt->rt_info) {
                        printf(":%u", js_rc(p)->ref_count);
                    } else {
                        printf("\n");
                    }
                }
            }
        }
        if (rt->rt_info && header_done)
            printf("\n");
    }
#endif

    /* free the atoms */
    for(i = 0; i < rt->atom_size; i++) {
        JSAtomStruct *p = rt->atom_array[i];
        if (!atom_is_free(p)) {
#ifdef DUMP_LEAKS
            list_del(&p->link);
#endif
            js_free_rt(rt, p);
        }
    }
    js_free_rt(rt, rt->atom_array);
    js_free_rt(rt, rt->atom_hash);
    js_free_rt(rt, rt->shape_hash);
#ifdef DUMP_LEAKS
    if (!list_empty(&rt->string_list)) {
        if (rt->rt_info) {
            printf("%s:1: string leakage:", rt->rt_info);
        } else {
            printf("String leaks:\n"
                   "    %6s %s\n",
                   "REFCNT", "VALUE");
        }
        list_for_each_safe(el, el1, &rt->string_list) {
            JSString *str = list_entry(el, JSString, link);
            if (rt->rt_info) {
                printf(" ");
            } else {
                printf("    %6u ", js_rc(str)->ref_count);
            }
            JS_DumpString(rt, str);
            if (rt->rt_info) {
                printf(":%u", js_rc(str)->ref_count);
            } else {
                printf("\n");
            }
            list_del(&str->link);
            js_free_rt(rt, str);
        }
        if (rt->rt_info)
            printf("\n");
    }
    {
        JSMallocState *s = &rt->malloc_ctx.malloc_state;
        if (s->malloc_count > 1) {
            if (rt->rt_info)
                printf("%s:1: ", rt->rt_info);
            printf("Memory leak: %"PRIu64" bytes lost in %"PRIu64" block%s\n",
                   (uint64_t)(s->malloc_size - sizeof(JSRuntime)),
                   (uint64_t)(s->malloc_count - 1), &"s"[s->malloc_count == 2]);
        }
    }
#endif

    {
        JSMallocState ms = rt->malloc_ctx.malloc_state;
        rt->malloc_ctx.mf.js_free(&ms, rt);
    }
}

JSContext *JS_NewContextRaw(JSRuntime *rt)
{
    JSContext *ctx;
    int i;

    ctx = js_mallocz_rt(rt, sizeof(JSContext));
    if (!ctx)
        return NULL;
    js_rc(ctx)->ref_count = 1;
    add_gc_object(rt, &ctx->header, JS_GC_OBJ_TYPE_JS_CONTEXT);

    ctx->class_proto = js_malloc_rt(rt, sizeof(ctx->class_proto[0]) *
                                    rt->class_count);
    if (!ctx->class_proto) {
        js_free_rt(rt, ctx);
        return NULL;
    }
    ctx->rt = rt;
    list_add_tail(&ctx->link, &rt->context_list);
    for(i = 0; i < rt->class_count; i++)
        ctx->class_proto[i] = JS_NULL;
    ctx->array_ctor = JS_NULL;
    ctx->iterator_ctor = JS_NULL;
    ctx->regexp_ctor = JS_NULL;
    ctx->promise_ctor = JS_NULL;
    init_list_head(&ctx->loaded_modules);

    if (JS_AddIntrinsicBasicObjects(ctx)) {
        JS_FreeContext(ctx);
        return NULL;
    }
    return ctx;
}

JSContext *JS_NewContext(JSRuntime *rt)
{
    JSContext *ctx;

    ctx = JS_NewContextRaw(rt);
    if (!ctx)
        return NULL;

    if (JS_AddIntrinsicBaseObjects(ctx) ||
        JS_AddIntrinsicDate(ctx) ||
        JS_AddIntrinsicEval(ctx) ||
        JS_AddIntrinsicStringNormalize(ctx) ||
        JS_AddIntrinsicRegExp(ctx) ||
        JS_AddIntrinsicJSON(ctx) ||
        JS_AddIntrinsicProxy(ctx) ||
        JS_AddIntrinsicMapSet(ctx) ||
        JS_AddIntrinsicTypedArrays(ctx) ||
        JS_AddIntrinsicPromise(ctx) ||
        JS_AddIntrinsicWeakRef(ctx)) {
        JS_FreeContext(ctx);
        return NULL;
    }
    return ctx;
}

void *JS_GetContextOpaque(JSContext *ctx)
{
    return ctx->user_opaque;
}

void JS_SetContextOpaque(JSContext *ctx, void *opaque)
{
    ctx->user_opaque = opaque;
}

/* set the new value and free the old value after (freeing the value
   can reallocate the object data) */
static inline void set_value(JSContext *ctx, JSValue *pval, JSValue new_val)
{
    JSValue old_val;
    old_val = *pval;
    *pval = new_val;
    JS_FreeValue(ctx, old_val);
}

void JS_SetClassProto(JSContext *ctx, JSClassID class_id, JSValue obj)
{
    JSRuntime *rt = ctx->rt;
    assert(class_id < rt->class_count);
    set_value(ctx, &ctx->class_proto[class_id], obj);
}

JSValue JS_GetClassProto(JSContext *ctx, JSClassID class_id)
{
    JSRuntime *rt = ctx->rt;
    assert(class_id < rt->class_count);
    return JS_DupValue(ctx, ctx->class_proto[class_id]);
}

typedef enum JSFreeModuleEnum {
    JS_FREE_MODULE_ALL,
    JS_FREE_MODULE_NOT_RESOLVED,
} JSFreeModuleEnum;

/* XXX: would be more efficient with separate module lists */
static void js_free_modules(JSContext *ctx, JSFreeModuleEnum flag)
{
    struct list_head *el, *el1;
    list_for_each_safe(el, el1, &ctx->loaded_modules) {
        JSModuleDef *m = list_entry(el, JSModuleDef, link);
        if (flag == JS_FREE_MODULE_ALL ||
            (flag == JS_FREE_MODULE_NOT_RESOLVED && !m->resolved)) {
            /* warning: the module may be referenced elsewhere. It
               could be simpler to use an array instead of a list for
               'ctx->loaded_modules' */
            list_del(&m->link);
            m->link.prev = NULL;
            m->link.next = NULL;
            JS_FreeValue(ctx, JS_MKPTR(JS_TAG_MODULE, m));
        }
    }
}

JSContext *JS_DupContext(JSContext *ctx)
{
    js_rc(ctx)->ref_count++;
    return ctx;
}

/* used by the GC */
static void JS_MarkContext(JSRuntime *rt, JSContext *ctx,
                           JS_MarkFunc *mark_func)
{
    int i;
    struct list_head *el;

    list_for_each(el, &ctx->loaded_modules) {
        JSModuleDef *m = list_entry(el, JSModuleDef, link);
        JS_MarkValue(rt, JS_MKPTR(JS_TAG_MODULE, m), mark_func);
    }

    JS_MarkValue(rt, ctx->global_obj, mark_func);
    JS_MarkValue(rt, ctx->global_var_obj, mark_func);

    JS_MarkValue(rt, ctx->throw_type_error, mark_func);
    JS_MarkValue(rt, ctx->eval_obj, mark_func);

    JS_MarkValue(rt, ctx->array_proto_values, mark_func);
    for(i = 0; i < JS_NATIVE_ERROR_COUNT; i++) {
        JS_MarkValue(rt, ctx->native_error_proto[i], mark_func);
    }
    for(i = 0; i < rt->class_count; i++) {
        JS_MarkValue(rt, ctx->class_proto[i], mark_func);
    }
    JS_MarkValue(rt, ctx->iterator_ctor, mark_func);
    JS_MarkValue(rt, ctx->async_iterator_proto, mark_func);
    JS_MarkValue(rt, ctx->promise_ctor, mark_func);
    JS_MarkValue(rt, ctx->array_ctor, mark_func);
    JS_MarkValue(rt, ctx->regexp_ctor, mark_func);
    JS_MarkValue(rt, ctx->function_ctor, mark_func);
    JS_MarkValue(rt, ctx->function_proto, mark_func);

    if (ctx->array_shape)
        mark_func(rt, &ctx->array_shape->header);

    if (ctx->arguments_shape)
        mark_func(rt, &ctx->arguments_shape->header);

    if (ctx->mapped_arguments_shape)
        mark_func(rt, &ctx->mapped_arguments_shape->header);

    if (ctx->regexp_shape)
        mark_func(rt, &ctx->regexp_shape->header);

    if (ctx->regexp_result_shape)
        mark_func(rt, &ctx->regexp_result_shape->header);
}

void JS_FreeContext(JSContext *ctx)
{
    JSRuntime *rt = ctx->rt;
    int i;

    if (--js_rc(ctx)->ref_count > 0)
        return;
    assert(js_rc(ctx)->ref_count == 0);

#ifdef DUMP_ATOMS
    JS_DumpAtoms(ctx->rt);
#endif
#ifdef DUMP_SHAPES
    JS_DumpShapes(ctx->rt);
#endif
#ifdef DUMP_OBJECTS
    {
        struct list_head *el;
        JSGCObjectHeader *p;
        printf("JSObjects: {\n");
        JS_DumpObjectHeader(ctx->rt);
        list_for_each(el, &rt->gc_obj_list) {
            p = list_entry(el, JSGCObjectHeader, link);
            JS_DumpGCObject(rt, p);
        }
        printf("}\n");
    }
#endif
#ifdef DUMP_MEM
    {
        JSMemoryUsage stats;
        JS_ComputeMemoryUsage(rt, &stats);
        JS_DumpMemoryUsage(stdout, &stats, rt);
    }
#endif

    js_free_modules(ctx, JS_FREE_MODULE_ALL);

    JS_FreeValue(ctx, ctx->global_obj);
    JS_FreeValue(ctx, ctx->global_var_obj);

    JS_FreeValue(ctx, ctx->throw_type_error);
    JS_FreeValue(ctx, ctx->eval_obj);

    JS_FreeValue(ctx, ctx->array_proto_values);
    for(i = 0; i < JS_NATIVE_ERROR_COUNT; i++) {
        JS_FreeValue(ctx, ctx->native_error_proto[i]);
    }
    for(i = 0; i < rt->class_count; i++) {
        JS_FreeValue(ctx, ctx->class_proto[i]);
    }
    js_free_rt(rt, ctx->class_proto);
    JS_FreeValue(ctx, ctx->iterator_ctor);
    JS_FreeValue(ctx, ctx->async_iterator_proto);
    JS_FreeValue(ctx, ctx->promise_ctor);
    JS_FreeValue(ctx, ctx->array_ctor);
    JS_FreeValue(ctx, ctx->regexp_ctor);
    JS_FreeValue(ctx, ctx->function_ctor);
    JS_FreeValue(ctx, ctx->function_proto);

    js_free_shape_null(ctx->rt, ctx->array_shape);
    js_free_shape_null(ctx->rt, ctx->arguments_shape);
    js_free_shape_null(ctx->rt, ctx->mapped_arguments_shape);
    js_free_shape_null(ctx->rt, ctx->regexp_shape);
    js_free_shape_null(ctx->rt, ctx->regexp_result_shape);

    list_del(&ctx->link);
    remove_gc_object(&ctx->header);
    js_free_rt(ctx->rt, ctx);
}

JSRuntime *JS_GetRuntime(JSContext *ctx)
{
    return ctx->rt;
}

static void update_stack_limit(JSRuntime *rt)
{
    if (rt->stack_size == 0) {
        rt->stack_limit = 0; /* no limit */
    } else {
        rt->stack_limit = rt->stack_top - rt->stack_size;
    }
}

void JS_SetMaxStackSize(JSRuntime *rt, size_t stack_size)
{
    rt->stack_size = stack_size;
    update_stack_limit(rt);
}

void JS_UpdateStackTop(JSRuntime *rt)
{
    rt->stack_top = js_get_stack_pointer();
    update_stack_limit(rt);
}

static inline BOOL is_strict_mode(JSContext *ctx)
{
    JSStackFrame *sf = ctx->rt->current_stack_frame;
    return (sf && (sf->js_mode & JS_MODE_STRICT));
}

/* JSAtom support */

#define JS_ATOM_TAG_INT (1U << 31)
#define JS_ATOM_MAX_INT (JS_ATOM_TAG_INT - 1)
#define JS_ATOM_MAX     ((1U << 30) - 1)

/* return the max count from the hash size */
#define JS_ATOM_COUNT_RESIZE(n) ((n) * 2)

static inline BOOL __JS_AtomIsConst(JSAtom v)
{
#if defined(DUMP_LEAKS) && DUMP_LEAKS > 1
        return (int32_t)v <= 0;
#else
        return (int32_t)v < JS_ATOM_END;
#endif
}

static inline BOOL __JS_AtomIsTaggedInt(JSAtom v)
{
    return (v & JS_ATOM_TAG_INT) != 0;
}

static inline JSAtom __JS_AtomFromUInt32(uint32_t v)
{
    return v | JS_ATOM_TAG_INT;
}

static inline uint32_t __JS_AtomToUInt32(JSAtom atom)
{
    return atom & ~JS_ATOM_TAG_INT;
}

static inline int is_num(int c)
{
    return c >= '0' && c <= '9';
}

/* return TRUE if the string is a number n with 0 <= n <= 2^32-1 */
static inline BOOL is_num_string(uint32_t *pval, const JSString *p)
{
    uint32_t n;
    uint64_t n64;
    int c, i, len;

    len = p->len;
    if (len == 0 || len > 10)
        return FALSE;
    c = string_get(p, 0);
    if (is_num(c)) {
        if (c == '0') {
            if (len != 1)
                return FALSE;
            n = 0;
        } else {
            n = c - '0';
            for(i = 1; i < len; i++) {
                c = string_get(p, i);
                if (!is_num(c))
                    return FALSE;
                n64 = (uint64_t)n * 10 + (c - '0');
                if ((n64 >> 32) != 0)
                    return FALSE;
                n = n64;
            }
        }
        *pval = n;
        return TRUE;
    } else {
        return FALSE;
    }
}

/* XXX: could use faster version ? */
static inline uint32_t hash_string8(const uint8_t *str, size_t len, uint32_t h)
{
    size_t i;

    for(i = 0; i < len; i++)
        h = h * 263 + str[i];
    return h;
}

static inline uint32_t hash_string16(const uint16_t *str,
                                     size_t len, uint32_t h)
{
    size_t i;

    for(i = 0; i < len; i++)
        h = h * 263 + str[i];
    return h;
}

static uint32_t hash_string(const JSString *str, uint32_t h)
{
    if (str->is_wide_char)
        h = hash_string16(str->u.str16, str->len, h);
    else
        h = hash_string8(str->u.str8, str->len, h);
    return h;
}

static uint32_t hash_string_rope(JSValueConst val, uint32_t h)
{
    if (JS_VALUE_GET_TAG(val) == JS_TAG_STRING) {
        return hash_string(JS_VALUE_GET_STRING(val), h);
    } else {
        JSStringRope *r = JS_VALUE_GET_STRING_ROPE(val);
        h = hash_string_rope(r->left, h);
        return hash_string_rope(r->right, h);
    }
}

static __maybe_unused void JS_DumpChar(FILE *fo, int c, int sep)
{
    if (c == sep || c == '\\') {
        fputc('\\', fo);
        fputc(c, fo);
    } else if (c >= ' ' && c <= 126) {
        fputc(c, fo);
    } else if (c == '\n') {
        fputc('\\', fo);
        fputc('n', fo);
    } else {
        fprintf(fo, "\\u%04x", c);
    }
}

static __maybe_unused void JS_DumpString(JSRuntime *rt, const JSString *p)
{
    int i, sep;

    if (p == NULL) {
        printf("<null>");
        return;
    }
    printf("%d", js_rc((void *)p)->ref_count);
    sep = (js_rc((void *)p)->ref_count == 1) ? '\"' : '\'';
    putchar(sep);
    for(i = 0; i < p->len; i++) {
        JS_DumpChar(stdout, string_get(p, i), sep);
    }
    putchar(sep);
}

static __maybe_unused void JS_DumpAtoms(JSRuntime *rt)
{
    JSAtomStruct *p;
    int h, i;
    /* This only dumps hashed atoms, not JS_ATOM_TYPE_SYMBOL atoms */
    printf("JSAtom count=%d size=%d hash_size=%d:\n",
           rt->atom_count, rt->atom_size, rt->atom_hash_size);
    printf("JSAtom hash table: {\n");
    for(i = 0; i < rt->atom_hash_size; i++) {
        h = rt->atom_hash[i];
        if (h) {
            printf("  %d:", i);
            while (h) {
                p = rt->atom_array[h];
                printf(" ");
                JS_DumpString(rt, p);
                h = p->hash_next;
            }
            printf("\n");
        }
    }
    printf("}\n");
    printf("JSAtom table: {\n");
    for(i = 0; i < rt->atom_size; i++) {
        p = rt->atom_array[i];
        if (!atom_is_free(p)) {
            printf("  %d: { %d %08x ", i, p->atom_type, p->hash);
            if (!(p->len == 0 && p->is_wide_char != 0))
                JS_DumpString(rt, p);
            printf(" %d }\n", p->hash_next);
        }
    }
    printf("}\n");
}

static int JS_ResizeAtomHash(JSRuntime *rt, int new_hash_size)
{
    JSAtomStruct *p;
    uint32_t new_hash_mask, h, i, hash_next1, j, *new_hash;

    assert((new_hash_size & (new_hash_size - 1)) == 0); /* power of two */
    new_hash_mask = new_hash_size - 1;
    new_hash = js_mallocz_rt(rt, sizeof(rt->atom_hash[0]) * new_hash_size);
    if (!new_hash)
        return -1;
    for(i = 0; i < rt->atom_hash_size; i++) {
        h = rt->atom_hash[i];
        while (h != 0) {
            p = rt->atom_array[h];
            hash_next1 = p->hash_next;
            /* add in new hash table */
            j = p->hash & new_hash_mask;
            p->hash_next = new_hash[j];
            new_hash[j] = h;
            h = hash_next1;
        }
    }
    js_free_rt(rt, rt->atom_hash);
    rt->atom_hash = new_hash;
    rt->atom_hash_size = new_hash_size;
    rt->atom_count_resize = JS_ATOM_COUNT_RESIZE(new_hash_size);
    //    JS_DumpAtoms(rt);
    return 0;
}

static int JS_InitAtoms(JSRuntime *rt)
{
    int i, len, atom_type;
    const char *p;

    rt->atom_hash_size = 0;
    rt->atom_hash = NULL;
    rt->atom_count = 0;
    rt->atom_size = 0;
    rt->atom_free_index = 0;
    if (JS_ResizeAtomHash(rt, 512))     /* there are at least 504 predefined atoms */
        return -1;

    p = js_atom_init;
    for(i = 1; i < JS_ATOM_END; i++) {
        if (i == JS_ATOM_Private_brand)
            atom_type = JS_ATOM_TYPE_PRIVATE;
        else if (i >= JS_ATOM_Symbol_toPrimitive)
            atom_type = JS_ATOM_TYPE_SYMBOL;
        else
            atom_type = JS_ATOM_TYPE_STRING;
        len = strlen(p);
        if (__JS_NewAtomInit(rt, p, len, atom_type) == JS_ATOM_NULL)
            return -1;
        p = p + len + 1;
    }
    return 0;
}

static JSAtom JS_DupAtomRT(JSRuntime *rt, JSAtom v)
{
    JSAtomStruct *p;

    if (!__JS_AtomIsConst(v)) {
        p = rt->atom_array[v];
        js_rc(p)->ref_count++;
    }
    return v;
}

JSAtom JS_DupAtom(JSContext *ctx, JSAtom v)
{
    JSRuntime *rt;
    JSAtomStruct *p;

    if (!__JS_AtomIsConst(v)) {
        rt = ctx->rt;
        p = rt->atom_array[v];
        js_rc(p)->ref_count++;
    }
    return v;
}

static JSAtomKindEnum JS_AtomGetKind(JSContext *ctx, JSAtom v)
{
    JSRuntime *rt;
    JSAtomStruct *p;

    rt = ctx->rt;
    if (__JS_AtomIsTaggedInt(v))
        return JS_ATOM_KIND_STRING;
    p = rt->atom_array[v];
    switch(p->atom_type) {
    case JS_ATOM_TYPE_STRING:
        return JS_ATOM_KIND_STRING;
    case JS_ATOM_TYPE_GLOBAL_SYMBOL:
        return JS_ATOM_KIND_SYMBOL;
    case JS_ATOM_TYPE_SYMBOL:
        if (p->hash == JS_ATOM_HASH_PRIVATE)
            return JS_ATOM_KIND_PRIVATE;
        else
            return JS_ATOM_KIND_SYMBOL;
    default:
        abort();
    }
}

static BOOL JS_AtomIsString(JSContext *ctx, JSAtom v)
{
    return JS_AtomGetKind(ctx, v) == JS_ATOM_KIND_STRING;
}

static JSAtom js_get_atom_index(JSRuntime *rt, JSAtomStruct *p)
{
    uint32_t i = p->hash_next;  /* atom_index */
    if (p->atom_type != JS_ATOM_TYPE_SYMBOL) {
        JSAtomStruct *p1;

        i = rt->atom_hash[p->hash & (rt->atom_hash_size - 1)];
        p1 = rt->atom_array[i];
        while (p1 != p) {
            assert(i != 0);
            i = p1->hash_next;
            p1 = rt->atom_array[i];
        }
    }
    return i;
}

/* string case (internal). Return JS_ATOM_NULL if error. 'str' is
   freed. */
static JSAtom __JS_NewAtom(JSRuntime *rt, JSString *str, int atom_type)
{
    uint32_t h, h1, i;
    JSAtomStruct *p;
    int len;

#if 0
    printf("__JS_NewAtom: ");  JS_DumpString(rt, str); printf("\n");
#endif
    if (atom_type < JS_ATOM_TYPE_SYMBOL) {
        /* str is not NULL */
        if (str->atom_type == atom_type) {
            /* str is the atom, return its index */
            i = js_get_atom_index(rt, str);
            /* reduce string refcount and increase atom's unless constant */
            if (__JS_AtomIsConst(i))
                js_rc(str)->ref_count--;
            return i;
        }
        /* try and locate an already registered atom */
        len = str->len;
        h = hash_string(str, atom_type);
        h &= JS_ATOM_HASH_MASK;
        h1 = h & (rt->atom_hash_size - 1);
        i = rt->atom_hash[h1];
        while (i != 0) {
            p = rt->atom_array[i];
            if (p->hash == h &&
                p->atom_type == atom_type &&
                p->len == len &&
                js_string_memcmp(p, 0, str, 0, len) == 0) {
                if (!__JS_AtomIsConst(i))
                    js_rc(p)->ref_count++;
                goto done;
            }
            i = p->hash_next;
        }
    } else {
        h1 = 0; /* avoid warning */
        if (atom_type == JS_ATOM_TYPE_SYMBOL) {
            h = 0;
        } else {
            h = JS_ATOM_HASH_PRIVATE;
            atom_type = JS_ATOM_TYPE_SYMBOL;
        }
    }

    if (rt->atom_free_index == 0) {
        /* allow new atom entries */
        uint32_t new_size, start;
        JSAtomStruct **new_array;

        /* alloc new with size progression 3/2:
           4 6 9 13 19 28 42 63 94 141 211 316 474 711 1066 1599 2398 3597 5395 8092
           preallocating space for predefined atoms (at least 504).
         */
        new_size = max_int(711, rt->atom_size * 3 / 2);
        if (new_size > JS_ATOM_MAX)
            goto fail;
        /* XXX: should use realloc2 to use slack space */
        new_array = js_realloc_rt(rt, rt->atom_array, sizeof(*new_array) * new_size);
        if (!new_array)
            goto fail;
        /* Note: the atom 0 is not used */
        start = rt->atom_size;
        if (start == 0) {
            /* JS_ATOM_NULL entry */
            p = js_mallocz_rt(rt, sizeof(JSAtomStruct));
            if (!p) {
                js_free_rt(rt, new_array);
                goto fail;
            }
            js_rc(p)->ref_count = 1;  /* not refcounted */
            p->atom_type = JS_ATOM_TYPE_SYMBOL;
#ifdef DUMP_LEAKS
            list_add_tail(&p->link, &rt->string_list);
#endif
            new_array[0] = p;
            rt->atom_count++;
            start = 1;
        }
        rt->atom_size = new_size;
        rt->atom_array = new_array;
        rt->atom_free_index = start;
        for(i = start; i < new_size; i++) {
            uint32_t next;
            if (i == (new_size - 1))
                next = 0;
            else
                next = i + 1;
            rt->atom_array[i] = atom_set_free(next);
        }
    }

    if (str) {
        if (str->atom_type == 0) {
            p = str;
            p->atom_type = atom_type;
        } else {
            p = js_malloc_rt(rt, sizeof(JSString) +
                             (str->len << str->is_wide_char) +
                             1 - str->is_wide_char);
            if (unlikely(!p))
                goto fail;
            js_rc(p)->ref_count = 1;
            p->is_wide_char = str->is_wide_char;
            p->len = str->len;
#ifdef DUMP_LEAKS
            list_add_tail(&p->link, &rt->string_list);
#endif
            memcpy(p->u.str8, str->u.str8, (str->len << str->is_wide_char) +
                   1 - str->is_wide_char);
            js_free_string(rt, str);
        }
    } else {
        p = js_malloc_rt(rt, sizeof(JSAtomStruct)); /* empty wide string */
        if (!p)
            return JS_ATOM_NULL;
        js_rc(p)->ref_count = 1;
        p->is_wide_char = 1;    /* Hack to represent NULL as a JSString */
        p->len = 0;
#ifdef DUMP_LEAKS
        list_add_tail(&p->link, &rt->string_list);
#endif
    }

    /* use an already free entry */
    i = rt->atom_free_index;
    rt->atom_free_index = atom_get_free(rt->atom_array[i]);
    rt->atom_array[i] = p;

    p->hash = h;
    p->hash_next = i;   /* atom_index */
    p->atom_type = atom_type;

    rt->atom_count++;

    if (atom_type != JS_ATOM_TYPE_SYMBOL) {
        p->hash_next = rt->atom_hash[h1];
        rt->atom_hash[h1] = i;
        if (unlikely(rt->atom_count >= rt->atom_count_resize))
            JS_ResizeAtomHash(rt, rt->atom_hash_size * 2);
    }

    //    JS_DumpAtoms(rt);
    return i;

 fail:
    i = JS_ATOM_NULL;
 done:
    if (str)
        js_free_string(rt, str);
    return i;
}

/* only works with zero terminated 8 bit strings */
static JSAtom __JS_NewAtomInit(JSRuntime *rt, const char *str, int len,
                               int atom_type)
{
    JSString *p;
    p = js_alloc_string_rt(rt, len, 0);
    if (!p)
        return JS_ATOM_NULL;
    memcpy(p->u.str8, str, len);
    p->u.str8[len] = '\0';
    return __JS_NewAtom(rt, p, atom_type);
}

/* Warning: str must be ASCII only */
static JSAtom __JS_FindAtom(JSRuntime *rt, const char *str, size_t len,
                            int atom_type)
{
    uint32_t h, h1, i;
    JSAtomStruct *p;

    h = hash_string8((const uint8_t *)str, len, JS_ATOM_TYPE_STRING);
    h &= JS_ATOM_HASH_MASK;
    h1 = h & (rt->atom_hash_size - 1);
    i = rt->atom_hash[h1];
    while (i != 0) {
        p = rt->atom_array[i];
        if (p->hash == h &&
            p->atom_type == JS_ATOM_TYPE_STRING &&
            p->len == len &&
            p->is_wide_char == 0 &&
            memcmp(p->u.str8, str, len) == 0) {
            if (!__JS_AtomIsConst(i))
                js_rc(p)->ref_count++;
            return i;
        }
        i = p->hash_next;
    }
    return JS_ATOM_NULL;
}

static void JS_FreeAtomStruct(JSRuntime *rt, JSAtomStruct *p)
{
#if 0   /* JS_ATOM_NULL is not refcounted: __JS_AtomIsConst() includes 0 */
    if (unlikely(i == JS_ATOM_NULL)) {
        js_rc(p)->ref_count = INT32_MAX / 2;
        return;
    }
#endif
    uint32_t i = p->hash_next;  /* atom_index */
    if (p->atom_type != JS_ATOM_TYPE_SYMBOL) {
        JSAtomStruct *p0, *p1;
        uint32_t h0;

        h0 = p->hash & (rt->atom_hash_size - 1);
        i = rt->atom_hash[h0];
        p1 = rt->atom_array[i];
        if (p1 == p) {
            rt->atom_hash[h0] = p1->hash_next;
        } else {
            for(;;) {
                assert(i != 0);
                p0 = p1;
                i = p1->hash_next;
                p1 = rt->atom_array[i];
                if (p1 == p) {
                    p0->hash_next = p1->hash_next;
                    break;
                }
            }
        }
    }
    /* insert in free atom list */
    rt->atom_array[i] = atom_set_free(rt->atom_free_index);
    rt->atom_free_index = i;
    /* free the string structure */
#ifdef DUMP_LEAKS
    list_del(&p->link);
#endif
    if (p->atom_type == JS_ATOM_TYPE_SYMBOL &&
        p->hash != JS_ATOM_HASH_PRIVATE && p->hash != 0) {
        /* live weak references are still present on this object: keep
           it */
    } else {
        js_free_rt(rt, p);
    }
    rt->atom_count--;
    assert(rt->atom_count >= 0);
}

static void __JS_FreeAtom(JSRuntime *rt, uint32_t i)
{
    JSAtomStruct *p;

    p = rt->atom_array[i];
    if (--js_rc(p)->ref_count > 0)
        return;
    JS_FreeAtomStruct(rt, p);
}

/* Warning: 'p' is freed */
static JSAtom JS_NewAtomStr(JSContext *ctx, JSString *p)
{
    JSRuntime *rt = ctx->rt;
    uint32_t n;
    if (is_num_string(&n, p)) {
        if (n <= JS_ATOM_MAX_INT) {
            js_free_string(rt, p);
            return __JS_AtomFromUInt32(n);
        }
    }
    /* XXX: should generate an exception */
    return __JS_NewAtom(rt, p, JS_ATOM_TYPE_STRING);
}

/* XXX: optimize */
static size_t count_ascii(const uint8_t *buf, size_t len)
{
    const uint8_t *p, *p_end;
    p = buf;
    p_end = buf + len;
    while (p < p_end && *p < 128)
        p++;
    return p - buf;
}

/* str is UTF-8 encoded */
JSAtom JS_NewAtomLen(JSContext *ctx, const char *str, size_t len)
{
    JSValue val;

    if (len == 0 ||
        (!is_digit(*str) &&
         count_ascii((const uint8_t *)str, len) == len)) {
        JSAtom atom = __JS_FindAtom(ctx->rt, str, len, JS_ATOM_TYPE_STRING);
        if (atom)
            return atom;
    }
    val = JS_NewStringLen(ctx, str, len);
    if (JS_IsException(val))
        return JS_ATOM_NULL;
    return JS_NewAtomStr(ctx, JS_VALUE_GET_STRING(val));
}

JSAtom JS_NewAtom(JSContext *ctx, const char *str)
{
    return JS_NewAtomLen(ctx, str, strlen(str));
}

JSAtom JS_NewAtomUInt32(JSContext *ctx, uint32_t n)
{
    if (n <= JS_ATOM_MAX_INT) {
        return __JS_AtomFromUInt32(n);
    } else {
        char buf[11];
        JSValue val;
        size_t len;
        len = u32toa(buf, n);
        val = js_new_string8_len(ctx, buf, len);
        if (JS_IsException(val))
            return JS_ATOM_NULL;
        return __JS_NewAtom(ctx->rt, JS_VALUE_GET_STRING(val),
                            JS_ATOM_TYPE_STRING);
    }
}

static JSAtom JS_NewAtomInt64(JSContext *ctx, int64_t n)
{
    if ((uint64_t)n <= JS_ATOM_MAX_INT) {
        return __JS_AtomFromUInt32((uint32_t)n);
    } else {
        char buf[24];
        JSValue val;
        size_t len;
        len = i64toa(buf, n);
        val = js_new_string8_len(ctx, buf, len);
        if (JS_IsException(val))
            return JS_ATOM_NULL;
        return __JS_NewAtom(ctx->rt, JS_VALUE_GET_STRING(val),
                            JS_ATOM_TYPE_STRING);
    }
}

/* 'p' is freed */
static JSValue JS_NewSymbol(JSContext *ctx, JSString *p, int atom_type)
{
    JSRuntime *rt = ctx->rt;
    JSAtom atom;
    atom = __JS_NewAtom(rt, p, atom_type);
    if (atom == JS_ATOM_NULL)
        return JS_ThrowOutOfMemory(ctx);
    return JS_MKPTR(JS_TAG_SYMBOL, rt->atom_array[atom]);
}

/* descr must be a non-numeric string atom */
static JSValue JS_NewSymbolFromAtom(JSContext *ctx, JSAtom descr,
                                    int atom_type)
{
    JSRuntime *rt = ctx->rt;
    JSString *p;

    assert(!__JS_AtomIsTaggedInt(descr));
    assert(descr < rt->atom_size);
    p = rt->atom_array[descr];
    JS_DupValue(ctx, JS_MKPTR(JS_TAG_STRING, p));
    return JS_NewSymbol(ctx, p, atom_type);
}

#define ATOM_GET_STR_BUF_SIZE 64

/* Should only be used for debug. */
static const char *JS_AtomGetStrRT(JSRuntime *rt, char *buf, int buf_size,
                                   JSAtom atom)
{
    if (__JS_AtomIsTaggedInt(atom)) {
        snprintf(buf, buf_size, "%u", __JS_AtomToUInt32(atom));
    } else {
        JSAtomStruct *p;
        assert(atom < rt->atom_size);
        if (atom == JS_ATOM_NULL) {
            snprintf(buf, buf_size, "<null>");
        } else {
            int i, c;
            char *q;
            JSString *str;

            q = buf;
            p = rt->atom_array[atom];
            assert(!atom_is_free(p));
            str = p;
            if (str) {
                if (!str->is_wide_char) {
                    /* special case ASCII strings */
                    c = 0;
                    for(i = 0; i < str->len; i++) {
                        c |= str->u.str8[i];
                    }
                    if (c < 0x80)
                        return (const char *)str->u.str8;
                }
                for(i = 0; i < str->len; i++) {
                    c = string_get(str, i);
                    if ((q - buf) >= buf_size - UTF8_CHAR_LEN_MAX)
                        break;
                    if (c < 128) {
                        *q++ = c;
                    } else {
                        q += unicode_to_utf8((uint8_t *)q, c);
                    }
                }
            }
            *q = '\0';
        }
    }
    return buf;
}

static const char *JS_AtomGetStr(JSContext *ctx, char *buf, int buf_size, JSAtom atom)
{
    return JS_AtomGetStrRT(ctx->rt, buf, buf_size, atom);
}

static JSValue __JS_AtomToValue(JSContext *ctx, JSAtom atom, BOOL force_string)
{
    char buf[ATOM_GET_STR_BUF_SIZE];

    if (__JS_AtomIsTaggedInt(atom)) {
        size_t len = u32toa(buf, __JS_AtomToUInt32(atom));
        return js_new_string8_len(ctx, buf, len);
    } else {
        JSRuntime *rt = ctx->rt;
        JSAtomStruct *p;
        assert(atom < rt->atom_size);
        p = rt->atom_array[atom];
        if (p->atom_type == JS_ATOM_TYPE_STRING) {
            goto ret_string;
        } else if (force_string) {
            if (p->len == 0 && p->is_wide_char != 0) {
                /* no description string */
                p = rt->atom_array[JS_ATOM_empty_string];
            }
        ret_string:
            return JS_DupValue(ctx, JS_MKPTR(JS_TAG_STRING, p));
        } else {
            return JS_DupValue(ctx, JS_MKPTR(JS_TAG_SYMBOL, p));
        }
    }
}

JSValue JS_AtomToValue(JSContext *ctx, JSAtom atom)
{
    return __JS_AtomToValue(ctx, atom, FALSE);
}

JSValue JS_AtomToString(JSContext *ctx, JSAtom atom)
{
    return __JS_AtomToValue(ctx, atom, TRUE);
}

/* return TRUE if the atom is an array index (i.e. 0 <= index <=
   2^32-2 and return its value */
static BOOL JS_AtomIsArrayIndex(JSContext *ctx, uint32_t *pval, JSAtom atom)
{
    if (__JS_AtomIsTaggedInt(atom)) {
        *pval = __JS_AtomToUInt32(atom);
        return TRUE;
    } else {
        JSRuntime *rt = ctx->rt;
        JSAtomStruct *p;
        uint32_t val;

        assert(atom < rt->atom_size);
        p = rt->atom_array[atom];
        if (p->atom_type == JS_ATOM_TYPE_STRING &&
            is_num_string(&val, p) && val != -1) {
            *pval = val;
            return TRUE;
        } else {
            *pval = 0;
            return FALSE;
        }
    }
}

/* This test must be fast if atom is not a numeric index (e.g. a
   method name). Return JS_UNDEFINED if not a numeric
   index. JS_EXCEPTION can also be returned. */
static JSValue JS_AtomIsNumericIndex1(JSContext *ctx, JSAtom atom)
{
    JSRuntime *rt = ctx->rt;
    JSAtomStruct *p1;
    JSString *p;
    int c, ret;
    JSValue num, str;

    if (__JS_AtomIsTaggedInt(atom))
        return JS_NewInt32(ctx, __JS_AtomToUInt32(atom));
    assert(atom < rt->atom_size);
    p1 = rt->atom_array[atom];
    if (p1->atom_type != JS_ATOM_TYPE_STRING)
        return JS_UNDEFINED;
    switch(atom) {
    case JS_ATOM_minus_zero:
        return __JS_NewFloat64(ctx, -0.0);
    case JS_ATOM_Infinity:
        return __JS_NewFloat64(ctx, INFINITY);
    case JS_ATOM_minus_Infinity:
        return __JS_NewFloat64(ctx, -INFINITY);
    case JS_ATOM_NaN:
        return __JS_NewFloat64(ctx, NAN);
    default:
        break;
    }
    p = p1;
    if (p->len == 0)
        return JS_UNDEFINED;
    c = string_get(p, 0);
    if (!is_num(c) && c != '-')
        return JS_UNDEFINED;
    /* this is ECMA CanonicalNumericIndexString primitive */
    num = JS_ToNumber(ctx, JS_MKPTR(JS_TAG_STRING, p));
    if (JS_IsException(num))
        return num;
    str = JS_ToString(ctx, num);
    if (JS_IsException(str)) {
        JS_FreeValue(ctx, num);
        return str;
    }
    ret = js_string_eq(ctx, p, JS_VALUE_GET_STRING(str));
    JS_FreeValue(ctx, str);
    if (ret) {
        return num;
    } else {
        JS_FreeValue(ctx, num);
        return JS_UNDEFINED;
    }
}

/* return -1 if exception or TRUE/FALSE */
static int JS_AtomIsNumericIndex(JSContext *ctx, JSAtom atom)
{
    JSValue num;
    num = JS_AtomIsNumericIndex1(ctx, atom);
    if (likely(JS_IsUndefined(num)))
        return FALSE;
    if (JS_IsException(num))
        return -1;
    JS_FreeValue(ctx, num);
    return TRUE;
}

void JS_FreeAtom(JSContext *ctx, JSAtom v)
{
    if (!__JS_AtomIsConst(v))
        __JS_FreeAtom(ctx->rt, v);
}

void JS_FreeAtomRT(JSRuntime *rt, JSAtom v)
{
    if (!__JS_AtomIsConst(v))
        __JS_FreeAtom(rt, v);
}

/* return TRUE if 'v' is a symbol with a string description */
static BOOL JS_AtomSymbolHasDescription(JSContext *ctx, JSAtom v)
{
    JSRuntime *rt;
    JSAtomStruct *p;

    rt = ctx->rt;
    if (__JS_AtomIsTaggedInt(v))
        return FALSE;
    p = rt->atom_array[v];
    return (((p->atom_type == JS_ATOM_TYPE_SYMBOL &&
              p->hash != JS_ATOM_HASH_PRIVATE) ||
             p->atom_type == JS_ATOM_TYPE_GLOBAL_SYMBOL) &&
            !(p->len == 0 && p->is_wide_char != 0));
}

/* free with JS_FreeCString() */
const char *JS_AtomToCStringLen(JSContext *ctx, size_t *plen, JSAtom atom)
{
    JSValue str;
    const char *cstr;

    str = JS_AtomToString(ctx, atom);
    if (JS_IsException(str)) {
        if (plen)
            *plen = 0;
        return NULL;
    }
    cstr = JS_ToCStringLen(ctx, plen, str);
    JS_FreeValue(ctx, str);
    return cstr;
}

/* return a string atom containing name concatenated with str1 */
static JSAtom js_atom_concat_str(JSContext *ctx, JSAtom name, const char *str1)
{
    JSValue str;
    JSAtom atom;
    const char *cstr;
    char *cstr2;
    size_t len, len1;

    str = JS_AtomToString(ctx, name);
    if (JS_IsException(str))
        return JS_ATOM_NULL;
    cstr = JS_ToCStringLen(ctx, &len, str);
    if (!cstr)
        goto fail;
    len1 = strlen(str1);
    cstr2 = js_malloc(ctx, len + len1 + 1);
    if (!cstr2)
        goto fail;
    memcpy(cstr2, cstr, len);
    memcpy(cstr2 + len, str1, len1);
    cstr2[len + len1] = '\0';
    atom = JS_NewAtomLen(ctx, cstr2, len + len1);
    js_free(ctx, cstr2);
    JS_FreeCString(ctx, cstr);
    JS_FreeValue(ctx, str);
    return atom;
 fail:
    JS_FreeCString(ctx, cstr);
    JS_FreeValue(ctx, str);
    return JS_ATOM_NULL;
}

static JSAtom js_atom_concat_num(JSContext *ctx, JSAtom name, uint32_t n)
{
    char buf[16];
    size_t len;
    len = u32toa(buf, n);
    buf[len] = '\0';
    return js_atom_concat_str(ctx, name, buf);
}

static inline BOOL JS_IsEmptyString(JSValueConst v)
{
    return JS_VALUE_GET_TAG(v) == JS_TAG_STRING && JS_VALUE_GET_STRING(v)->len == 0;
}

/* JSClass support */

#ifdef CONFIG_ATOMICS
static pthread_mutex_t js_class_id_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

/* a new class ID is allocated if *pclass_id != 0 */
JSClassID JS_NewClassID(JSClassID *pclass_id)
{
    JSClassID class_id;
#ifdef CONFIG_ATOMICS
    pthread_mutex_lock(&js_class_id_mutex);
#endif
    class_id = *pclass_id;
    if (class_id == 0) {
        class_id = js_class_id_alloc++;
        *pclass_id = class_id;
    }
#ifdef CONFIG_ATOMICS
    pthread_mutex_unlock(&js_class_id_mutex);
#endif
    return class_id;
}

JSClassID JS_GetClassID(JSValue v)
{
    JSObject *p;
    if (JS_VALUE_GET_TAG(v) != JS_TAG_OBJECT)
        return JS_INVALID_CLASS_ID;
    p = JS_VALUE_GET_OBJ(v);
    return p->class_id;
}

BOOL JS_IsRegisteredClass(JSRuntime *rt, JSClassID class_id)
{
    return (class_id < rt->class_count &&
            rt->class_array[class_id].class_id != 0);
}

/* create a new object internal class. Return -1 if error, 0 if
   OK. The finalizer can be NULL if none is needed. */
static int JS_NewClass1(JSRuntime *rt, JSClassID class_id,
                        const JSClassDef *class_def, JSAtom name)
{
    int new_size, i;
    JSClass *cl, *new_class_array;
    struct list_head *el;

    if (class_id >= (1 << 16))
        return -1;
    if (class_id < rt->class_count &&
        rt->class_array[class_id].class_id != 0)
        return -1;

    if (class_id >= rt->class_count) {
        new_size = max_int(JS_CLASS_INIT_COUNT,
                           max_int(class_id + 1, rt->class_count * 3 / 2));

        /* reallocate the context class prototype array, if any */
        list_for_each(el, &rt->context_list) {
            JSContext *ctx = list_entry(el, JSContext, link);
            JSValue *new_tab;
            new_tab = js_realloc_rt(rt, ctx->class_proto,
                                    sizeof(ctx->class_proto[0]) * new_size);
            if (!new_tab)
                return -1;
            for(i = rt->class_count; i < new_size; i++)
                new_tab[i] = JS_NULL;
            ctx->class_proto = new_tab;
        }
        /* reallocate the class array */
        new_class_array = js_realloc_rt(rt, rt->class_array,
                                        sizeof(JSClass) * new_size);
        if (!new_class_array)
            return -1;
        memset(new_class_array + rt->class_count, 0,
               (new_size - rt->class_count) * sizeof(JSClass));
        rt->class_array = new_class_array;
        rt->class_count = new_size;
    }
    cl = &rt->class_array[class_id];
    cl->class_id = class_id;
    cl->class_name = JS_DupAtomRT(rt, name);
    cl->finalizer = class_def->finalizer;
    cl->gc_mark = class_def->gc_mark;
    cl->call = class_def->call;
    cl->exotic = class_def->exotic;
    return 0;
}

int JS_NewClass(JSRuntime *rt, JSClassID class_id, const JSClassDef *class_def)
{
    int ret, len;
    JSAtom name;

    len = strlen(class_def->class_name);
    name = __JS_FindAtom(rt, class_def->class_name, len, JS_ATOM_TYPE_STRING);
    if (name == JS_ATOM_NULL) {
        name = __JS_NewAtomInit(rt, class_def->class_name, len, JS_ATOM_TYPE_STRING);
        if (name == JS_ATOM_NULL)
            return -1;
    }
    ret = JS_NewClass1(rt, class_id, class_def, name);
    JS_FreeAtomRT(rt, name);
    return ret;
}

static JSValue js_new_string8_len(JSContext *ctx, const char *buf, int len)
{
    JSString *str;

    if (len <= 0) {
        return JS_AtomToString(ctx, JS_ATOM_empty_string);
    }
    str = js_alloc_string(ctx, len, 0);
    if (!str)
        return JS_EXCEPTION;
    memcpy(str->u.str8, buf, len);
    str->u.str8[len] = '\0';
    return JS_MKPTR(JS_TAG_STRING, str);
}

static JSValue js_new_string8(JSContext *ctx, const char *buf)
{
    return js_new_string8_len(ctx, buf, strlen(buf));
}

static JSValue js_new_string16_len(JSContext *ctx, const uint16_t *buf, int len)
{
    JSString *str;
    str = js_alloc_string(ctx, len, 1);
    if (!str)
        return JS_EXCEPTION;
    memcpy(str->u.str16, buf, len * 2);
    return JS_MKPTR(JS_TAG_STRING, str);
}

static JSValue js_new_string_char(JSContext *ctx, uint16_t c)
{
    if (c < 0x100) {
        uint8_t ch8 = c;
        return js_new_string8_len(ctx, (const char *)&ch8, 1);
    } else {
        uint16_t ch16 = c;
        return js_new_string16_len(ctx, &ch16, 1);
    }
}

static JSValue js_sub_string(JSContext *ctx, JSString *p, int start, int end)
{
    int len = end - start;
    if (start == 0 && end == p->len) {
        return JS_DupValue(ctx, JS_MKPTR(JS_TAG_STRING, p));
    }
    if (p->is_wide_char && len > 0) {
        JSString *str;
        int i;
        uint16_t c = 0;
        for (i = start; i < end; i++) {
            c |= p->u.str16[i];
        }
        if (c > 0xFF)
            return js_new_string16_len(ctx, p->u.str16 + start, len);

        str = js_alloc_string(ctx, len, 0);
        if (!str)
            return JS_EXCEPTION;
        for (i = 0; i < len; i++) {
            str->u.str8[i] = p->u.str16[start + i];
        }
        str->u.str8[len] = '\0';
        return JS_MKPTR(JS_TAG_STRING, str);
    } else {
        return js_new_string8_len(ctx, (const char *)(p->u.str8 + start), len);
    }
}

typedef struct StringBuffer {
    JSContext *ctx;
    JSString *str;
    int len;
    int size;
    int is_wide_char;
    int error_status;
} StringBuffer;

/* It is valid to call string_buffer_end() and all string_buffer functions even
   if string_buffer_init() or another string_buffer function returns an error.
   If the error_status is set, string_buffer_end() returns JS_EXCEPTION.
 */
static int string_buffer_init2(JSContext *ctx, StringBuffer *s, int size,
                               int is_wide)
{
    s->ctx = ctx;
    s->size = size;
    s->len = 0;
    s->is_wide_char = is_wide;
    s->error_status = 0;
    s->str = js_alloc_string(ctx, size, is_wide);
    if (unlikely(!s->str)) {
        s->size = 0;
        return s->error_status = -1;
    }
#ifdef DUMP_LEAKS
    /* the StringBuffer may reallocate the JSString, only link it at the end */
    list_del(&s->str->link);
#endif
    return 0;
}

static inline int string_buffer_init(JSContext *ctx, StringBuffer *s, int size)
{
    return string_buffer_init2(ctx, s, size, 0);
}

static void string_buffer_free(StringBuffer *s)
{
    js_free(s->ctx, s->str);
    s->str = NULL;
}

static int string_buffer_set_error(StringBuffer *s)
{
    js_free(s->ctx, s->str);
    s->str = NULL;
    s->size = 0;
    s->len = 0;
    return s->error_status = -1;
}

static no_inline int string_buffer_widen(StringBuffer *s, int size)
{
    JSString *str;
    size_t slack;
    int i;

    if (s->error_status)
        return -1;

    str = js_realloc2(s->ctx, s->str, sizeof(JSString) + (size << 1), &slack);
    if (!str)
        return string_buffer_set_error(s);
    size += slack >> 1;
    for(i = s->len; i-- > 0;) {
        str->u.str16[i] = str->u.str8[i];
    }
    s->is_wide_char = 1;
    s->size = size;
    s->str = str;
    return 0;
}

static no_inline int string_buffer_realloc(StringBuffer *s, int new_len, int c)
{
    JSString *new_str;
    int new_size;
    size_t new_size_bytes, slack;

    if (s->error_status)
        return -1;

    if (new_len > JS_STRING_LEN_MAX) {
        JS_ThrowInternalError(s->ctx, "string too long");
        return string_buffer_set_error(s);
    }
    new_size = min_int(max_int(new_len, s->size * 3 / 2), JS_STRING_LEN_MAX);
    if (!s->is_wide_char && c >= 0x100) {
        return string_buffer_widen(s, new_size);
    }
    new_size_bytes = sizeof(JSString) + (new_size << s->is_wide_char) + 1 - s->is_wide_char;
    new_str = js_realloc2(s->ctx, s->str, new_size_bytes, &slack);
    if (!new_str)
        return string_buffer_set_error(s);
    new_size = min_int(new_size + (slack >> s->is_wide_char), JS_STRING_LEN_MAX);
    s->size = new_size;
    s->str = new_str;
    return 0;
}

static no_inline int string_buffer_putc16_slow(StringBuffer *s, uint32_t c)
{
    if (unlikely(s->len >= s->size)) {
        if (string_buffer_realloc(s, s->len + 1, c))
            return -1;
    }
    if (s->is_wide_char) {
        s->str->u.str16[s->len++] = c;
    } else if (c < 0x100) {
        s->str->u.str8[s->len++] = c;
    } else {
        if (string_buffer_widen(s, s->size))
            return -1;
        s->str->u.str16[s->len++] = c;
    }
    return 0;
}

/* 0 <= c <= 0xff */
static int string_buffer_putc8(StringBuffer *s, uint32_t c)
{
    if (unlikely(s->len >= s->size)) {
        if (string_buffer_realloc(s, s->len + 1, c))
            return -1;
    }
    if (s->is_wide_char) {
        s->str->u.str16[s->len++] = c;
    } else {
        s->str->u.str8[s->len++] = c;
    }
    return 0;
}

/* 0 <= c <= 0xffff */
static int string_buffer_putc16(StringBuffer *s, uint32_t c)
{
    if (likely(s->len < s->size)) {
        if (s->is_wide_char) {
            s->str->u.str16[s->len++] = c;
            return 0;
        } else if (c < 0x100) {
            s->str->u.str8[s->len++] = c;
            return 0;
        }
    }
    return string_buffer_putc16_slow(s, c);
}

static int string_buffer_putc_slow(StringBuffer *s, uint32_t c)
{
    if (unlikely(c >= 0x10000)) {
        /* surrogate pair */
        if (string_buffer_putc16(s, get_hi_surrogate(c)))
            return -1;
        c = get_lo_surrogate(c);
    }
    return string_buffer_putc16(s, c);
}

/* 0 <= c <= 0x10ffff */
static inline int string_buffer_putc(StringBuffer *s, uint32_t c)
{
    if (likely(s->len < s->size)) {
        if (s->is_wide_char) {
            if (c < 0x10000) {
                s->str->u.str16[s->len++] = c;
                return 0;
            } else if (likely((s->len + 1) < s->size)) {
                s->str->u.str16[s->len++] = get_hi_surrogate(c);
                s->str->u.str16[s->len++] = get_lo_surrogate(c);
                return 0;
            }
        } else if (c < 0x100) {
            s->str->u.str8[s->len++] = c;
            return 0;
        }
    }
    return string_buffer_putc_slow(s, c);
}

static int string_getc(const JSString *p, int *pidx)
{
    int idx, c, c1;
    idx = *pidx;
    if (p->is_wide_char) {
        c = p->u.str16[idx++];
        if (is_hi_surrogate(c) && idx < p->len) {
            c1 = p->u.str16[idx];
            if (is_lo_surrogate(c1)) {
                c = from_surrogate(c, c1);
                idx++;
            }
        }
    } else {
        c = p->u.str8[idx++];
    }
    *pidx = idx;
    return c;
}

static int string_buffer_write8(StringBuffer *s, const uint8_t *p, int len)
{
    int i;

    if (s->len + len > s->size) {
        if (string_buffer_realloc(s, s->len + len, 0))
            return -1;
    }
    if (s->is_wide_char) {
        for (i = 0; i < len; i++) {
            s->str->u.str16[s->len + i] = p[i];
        }
        s->len += len;
    } else {
        memcpy(&s->str->u.str8[s->len], p, len);
        s->len += len;
    }
    return 0;
}

static int string_buffer_write16(StringBuffer *s, const uint16_t *p, int len)
{
    int c = 0, i;

    for (i = 0; i < len; i++) {
        c |= p[i];
    }
    if (s->len + len > s->size) {
        if (string_buffer_realloc(s, s->len + len, c))
            return -1;
    } else if (!s->is_wide_char && c >= 0x100) {
        if (string_buffer_widen(s, s->size))
            return -1;
    }
    if (s->is_wide_char) {
        memcpy(&s->str->u.str16[s->len], p, len << 1);
        s->len += len;
    } else {
        for (i = 0; i < len; i++) {
            s->str->u.str8[s->len + i] = p[i];
        }
        s->len += len;
    }
    return 0;
}

/* appending an ASCII string */
static int string_buffer_puts8(StringBuffer *s, const char *str)
{
    return string_buffer_write8(s, (const uint8_t *)str, strlen(str));
}

static int string_buffer_concat(StringBuffer *s, const JSString *p,
                                uint32_t from, uint32_t to)
{
    if (to <= from)
        return 0;
    if (p->is_wide_char)
        return string_buffer_write16(s, p->u.str16 + from, to - from);
    else
        return string_buffer_write8(s, p->u.str8 + from, to - from);
}

static int string_buffer_concat_value(StringBuffer *s, JSValueConst v)
{
    JSString *p;
    JSValue v1;
    int res;

    if (s->error_status) {
        /* prevent exception overload */
        return -1;
    }
    if (unlikely(JS_VALUE_GET_TAG(v) != JS_TAG_STRING)) {
        if (JS_VALUE_GET_TAG(v) == JS_TAG_STRING_ROPE) {
            JSStringRope *r = JS_VALUE_GET_STRING_ROPE(v);
            /* recursion is acceptable because the rope depth is bounded */
            if (string_buffer_concat_value(s, r->left))
                return -1;
            return string_buffer_concat_value(s, r->right);
        } else {
            v1 = JS_ToString(s->ctx, v);
            if (JS_IsException(v1))
                return string_buffer_set_error(s);
            p = JS_VALUE_GET_STRING(v1);
            res = string_buffer_concat(s, p, 0, p->len);
            JS_FreeValue(s->ctx, v1);
            return res;
        }
    }
    p = JS_VALUE_GET_STRING(v);
    return string_buffer_concat(s, p, 0, p->len);
}

static int string_buffer_concat_value_free(StringBuffer *s, JSValue v)
{
    JSString *p;
    int res;

    if (s->error_status) {
        /* prevent exception overload */
        JS_FreeValue(s->ctx, v);
        return -1;
    }
    if (unlikely(JS_VALUE_GET_TAG(v) != JS_TAG_STRING)) {
        v = JS_ToStringFree(s->ctx, v);
        if (JS_IsException(v))
            return string_buffer_set_error(s);
    }
    p = JS_VALUE_GET_STRING(v);
    res = string_buffer_concat(s, p, 0, p->len);
    JS_FreeValue(s->ctx, v);
    return res;
}

static int string_buffer_fill(StringBuffer *s, int c, int count)
{
    /* XXX: optimize */
    if (s->len + count > s->size) {
        if (string_buffer_realloc(s, s->len + count, c))
            return -1;
    }
    while (count-- > 0) {
        if (string_buffer_putc16(s, c))
            return -1;
    }
    return 0;
}

static JSValue string_buffer_end(StringBuffer *s)
{
    JSString *str;
    str = s->str;
    if (s->error_status)
        return JS_EXCEPTION;
    if (s->len == 0) {
        js_free(s->ctx, str);
        s->str = NULL;
        return JS_AtomToString(s->ctx, JS_ATOM_empty_string);
    }
    if (s->len < s->size) {
        /* smaller size so js_realloc should not fail, but OK if it does */
        /* XXX: should add some slack to avoid unnecessary calls */
        /* XXX: might need to use malloc+free to ensure smaller size */
        str = js_realloc_rt(s->ctx->rt, str, sizeof(JSString) +
                            (s->len << s->is_wide_char) + 1 - s->is_wide_char);
        if (str == NULL)
            str = s->str;
        s->str = str;
    }
    if (!s->is_wide_char)
        str->u.str8[s->len] = 0;
#ifdef DUMP_LEAKS
    list_add_tail(&str->link, &s->ctx->rt->string_list);
#endif
    str->is_wide_char = s->is_wide_char;
    str->len = s->len;
    s->str = NULL;
    return JS_MKPTR(JS_TAG_STRING, str);
}

/* create a string from a UTF-8 buffer */
JSValue JS_NewStringLen(JSContext *ctx, const char *buf, size_t buf_len)
{
    const uint8_t *p, *p_end, *p_start, *p_next;
    uint32_t c;
    StringBuffer b_s, *b = &b_s;
    size_t len1;

    p_start = (const uint8_t *)buf;
    p_end = p_start + buf_len;
    len1 = count_ascii(p_start, buf_len);
    p = p_start + len1;
    if (len1 > JS_STRING_LEN_MAX)
        return JS_ThrowInternalError(ctx, "string too long");
    if (p == p_end) {
        /* ASCII string */
        return js_new_string8_len(ctx, buf, buf_len);
    } else {
        if (string_buffer_init(ctx, b, buf_len))
            goto fail;
        string_buffer_write8(b, p_start, len1);
        while (p < p_end) {
            if (*p < 128) {
                string_buffer_putc8(b, *p++);
            } else {
                /* parse utf-8 sequence, return 0xFFFFFFFF for error */
                c = unicode_from_utf8(p, p_end - p, &p_next);
                if (c < 0x10000) {
                    p = p_next;
                } else if (c <= 0x10FFFF) {
                    p = p_next;
                    /* surrogate pair */
                    string_buffer_putc16(b, get_hi_surrogate(c));
                    c = get_lo_surrogate(c);
                } else {
                    /* invalid char */
                    c = 0xfffd;
                    /* skip the invalid chars */
                    /* XXX: seems incorrect. Why not just use c = *p++; ? */
                    while (p < p_end && (*p >= 0x80 && *p < 0xc0))
                        p++;
                    if (p < p_end) {
                        p++;
                        while (p < p_end && (*p >= 0x80 && *p < 0xc0))
                            p++;
                    }
                }
                string_buffer_putc16(b, c);
            }
        }
    }
    return string_buffer_end(b);

 fail:
    string_buffer_free(b);
    return JS_EXCEPTION;
}

static JSValue JS_ConcatString3(JSContext *ctx, const char *str1,
                                JSValue str2, const char *str3)
{
    StringBuffer b_s, *b = &b_s;
    int len1, len3;
    JSString *p;

    if (unlikely(JS_VALUE_GET_TAG(str2) != JS_TAG_STRING)) {
        str2 = JS_ToStringFree(ctx, str2);
        if (JS_IsException(str2))
            goto fail;
    }
    p = JS_VALUE_GET_STRING(str2);
    len1 = strlen(str1);
    len3 = strlen(str3);

    if (string_buffer_init2(ctx, b, len1 + p->len + len3, p->is_wide_char))
        goto fail;

    string_buffer_write8(b, (const uint8_t *)str1, len1);
    string_buffer_concat(b, p, 0, p->len);
    string_buffer_write8(b, (const uint8_t *)str3, len3);

    JS_FreeValue(ctx, str2);
    return string_buffer_end(b);

 fail:
    JS_FreeValue(ctx, str2);
    return JS_EXCEPTION;
}

JSValue JS_NewAtomString(JSContext *ctx, const char *str)
{
    JSAtom atom = JS_NewAtom(ctx, str);
    if (atom == JS_ATOM_NULL)
        return JS_EXCEPTION;
    JSValue val = JS_AtomToString(ctx, atom);
    JS_FreeAtom(ctx, atom);
    return val;
}

/* return (NULL, 0) if exception. */
/* return pointer into a JSString with a live ref_count */
/* cesu8 determines if non-BMP1 codepoints are encoded as 1 or 2 utf-8 sequences */
const char *JS_ToCStringLen2(JSContext *ctx, size_t *plen, JSValueConst val1, BOOL cesu8)
{
    JSValue val;
    JSString *str, *str_new;
    int pos, len, c, c1;
    uint8_t *q;

    if (JS_VALUE_GET_TAG(val1) != JS_TAG_STRING) {
        val = JS_ToString(ctx, val1);
        if (JS_IsException(val))
            goto fail;
    } else {
        val = JS_DupValue(ctx, val1);
    }

    str = JS_VALUE_GET_STRING(val);
    len = str->len;
    if (!str->is_wide_char) {
        const uint8_t *src = str->u.str8;
        int count;

        /* count the number of non-ASCII characters */
        /* Scanning the whole string is required for ASCII strings,
           and computing the number of non-ASCII bytes is less expensive
           than testing each byte, hence this method is faster for ASCII
           strings, which is the most common case.
         */
        count = 0;
        for (pos = 0; pos < len; pos++) {
            count += src[pos] >> 7;
        }
        if (count == 0) {
            if (plen)
                *plen = len;
            return (const char *)src;
        }
        str_new = js_alloc_string(ctx, len + count, 0);
        if (!str_new)
            goto fail;
        q = str_new->u.str8;
        for (pos = 0; pos < len; pos++) {
            c = src[pos];
            if (c < 0x80) {
                *q++ = c;
            } else {
                *q++ = (c >> 6) | 0xc0;
                *q++ = (c & 0x3f) | 0x80;
            }
        }
    } else {
        const uint16_t *src = str->u.str16;
        /* Allocate 3 bytes per 16 bit code point. Surrogate pairs may
           produce 4 bytes but use 2 code points.
         */
        str_new = js_alloc_string(ctx, len * 3, 0);
        if (!str_new)
            goto fail;
        q = str_new->u.str8;
        pos = 0;
        while (pos < len) {
            c = src[pos++];
            if (c < 0x80) {
                *q++ = c;
            } else {
                if (is_hi_surrogate(c)) {
                    if (pos < len && !cesu8) {
                        c1 = src[pos];
                        if (is_lo_surrogate(c1)) {
                            pos++;
                            c = from_surrogate(c, c1);
                        } else {
                            /* Keep unmatched surrogate code points */
                            /* c = 0xfffd; */ /* error */
                        }
                    } else {
                        /* Keep unmatched surrogate code points */
                        /* c = 0xfffd; */ /* error */
                    }
                }
                q += unicode_to_utf8(q, c);
            }
        }
    }

    *q = '\0';
    str_new->len = q - str_new->u.str8;
    JS_FreeValue(ctx, val);
    if (plen)
        *plen = str_new->len;
    return (const char *)str_new->u.str8;
 fail:
    if (plen)
        *plen = 0;
    return NULL;
}

void JS_FreeCString(JSContext *ctx, const char *ptr)
{
    JSString *p;
    if (!ptr)
        return;
    /* purposely removing constness */
    p = container_of(ptr, JSString, u);
    JS_FreeValue(ctx, JS_MKPTR(JS_TAG_STRING, p));
}

static int memcmp16_8(const uint16_t *src1, const uint8_t *src2, int len)
{
    int c, i;
    for(i = 0; i < len; i++) {
        c = src1[i] - src2[i];
        if (c != 0)
            return c;
    }
    return 0;
}

static int memcmp16(const uint16_t *src1, const uint16_t *src2, int len)
{
    int c, i;
    for(i = 0; i < len; i++) {
        c = src1[i] - src2[i];
        if (c != 0)
            return c;
    }
    return 0;
}

static int js_string_memcmp(const JSString *p1, int pos1, const JSString *p2,
                            int pos2, int len)
{
    int res;

    if (likely(!p1->is_wide_char)) {
        if (likely(!p2->is_wide_char))
            res = memcmp(p1->u.str8 + pos1, p2->u.str8 + pos2, len);
        else
            res = -memcmp16_8(p2->u.str16 + pos2, p1->u.str8 + pos1, len);
    } else {
        if (!p2->is_wide_char)
            res = memcmp16_8(p1->u.str16 + pos1, p2->u.str8 + pos2, len);
        else
            res = memcmp16(p1->u.str16 + pos1, p2->u.str16 + pos2, len);
    }
    return res;
}

static BOOL js_string_eq(JSContext *ctx,
                         const JSString *p1, const JSString *p2)
{
    if (p1->len != p2->len)
        return FALSE;
    if (p1 == p2)
        return TRUE;
    return js_string_memcmp(p1, 0, p2, 0, p1->len) == 0;
}

/* return < 0, 0 or > 0 */
static int js_string_compare(JSContext *ctx,
                             const JSString *p1, const JSString *p2)
{
    int res, len;
    len = min_int(p1->len, p2->len);
    res = js_string_memcmp(p1, 0, p2, 0, len);
    if (res == 0) {
        if (p1->len == p2->len)
            res = 0;
        else if (p1->len < p2->len)
            res = -1;
        else
            res = 1;
    }
    return res;
}

static void copy_str16(uint16_t *dst, const JSString *p, int offset, int len)
{
    if (p->is_wide_char) {
        memcpy(dst, p->u.str16 + offset, len * 2);
    } else {
        const uint8_t *src1 = p->u.str8 + offset;
        int i;

        for(i = 0; i < len; i++)
            dst[i] = src1[i];
    }
}

static JSValue JS_ConcatString1(JSContext *ctx,
                                const JSString *p1, const JSString *p2)
{
    JSString *p;
    uint32_t len;
    int is_wide_char;

    len = p1->len + p2->len;
    if (len > JS_STRING_LEN_MAX)
        return JS_ThrowInternalError(ctx, "string too long");
    is_wide_char = p1->is_wide_char | p2->is_wide_char;
    p = js_alloc_string(ctx, len, is_wide_char);
    if (!p)
        return JS_EXCEPTION;
    if (!is_wide_char) {
        memcpy(p->u.str8, p1->u.str8, p1->len);
        memcpy(p->u.str8 + p1->len, p2->u.str8, p2->len);
        p->u.str8[len] = '\0';
    } else {
        copy_str16(p->u.str16, p1, 0, p1->len);
        copy_str16(p->u.str16 + p1->len, p2, 0, p2->len);
    }
    return JS_MKPTR(JS_TAG_STRING, p);
}

static BOOL JS_ConcatStringInPlace(JSContext *ctx, JSString *p1, JSValueConst op2) {
    if (JS_VALUE_GET_TAG(op2) == JS_TAG_STRING) {
        JSString *p2 = JS_VALUE_GET_STRING(op2);
        size_t size1;

        if (p2->len == 0)
            return TRUE;
        if (js_rc(p1)->ref_count != 1)
            return FALSE;
        size1 = js_malloc_usable_size(ctx, p1);
        if (p1->is_wide_char) {
            if (size1 >= sizeof(*p1) + ((p1->len + p2->len) << 1)) {
                if (p2->is_wide_char) {
                    memcpy(p1->u.str16 + p1->len, p2->u.str16, p2->len << 1);
                    p1->len += p2->len;
                    return TRUE;
                } else {
                    size_t i;
                    for (i = 0; i < p2->len; i++) {
                        p1->u.str16[p1->len++] = p2->u.str8[i];
                    }
                    return TRUE;
                }
            }
        } else if (!p2->is_wide_char) {
            if (size1 >= sizeof(*p1) + p1->len + p2->len + 1) {
                memcpy(p1->u.str8 + p1->len, p2->u.str8, p2->len);
                p1->len += p2->len;
                p1->u.str8[p1->len] = '\0';
                return TRUE;
            }
        }
    }
    return FALSE;
}

static JSValue JS_ConcatString2(JSContext *ctx, JSValue op1, JSValue op2)
{
    JSValue ret;
    JSString *p1, *p2;
    p1 = JS_VALUE_GET_STRING(op1);
    if (JS_ConcatStringInPlace(ctx, p1, op2)) {
        JS_FreeValue(ctx, op2);
        return op1;
    }
    p2 = JS_VALUE_GET_STRING(op2);
    ret = JS_ConcatString1(ctx, p1, p2);
    JS_FreeValue(ctx, op1);
    JS_FreeValue(ctx, op2);
    return ret;
}

/* Return the character at position 'idx'. 'val' must be a string or rope */
static int string_rope_get(JSValueConst val, uint32_t idx)
{
    if (JS_VALUE_GET_TAG(val) == JS_TAG_STRING) {
        return string_get(JS_VALUE_GET_STRING(val), idx);
    } else {
        JSStringRope *r = JS_VALUE_GET_STRING_ROPE(val);
        uint32_t len;
        if (JS_VALUE_GET_TAG(r->left) == JS_TAG_STRING)
            len = JS_VALUE_GET_STRING(r->left)->len;
        else
            len = JS_VALUE_GET_STRING_ROPE(r->left)->len;
        if (idx < len)
            return string_rope_get(r->left, idx);
        else
            return string_rope_get(r->right, idx - len);
    }
}

typedef struct {
    JSValueConst stack[JS_STRING_ROPE_MAX_DEPTH];
    int stack_len;
} JSStringRopeIter;

static void string_rope_iter_init(JSStringRopeIter *s, JSValueConst val)
{
    s->stack_len = 0;
    s->stack[s->stack_len++] = val;
}

/* iterate thru a rope and return the strings in order */
static JSString *string_rope_iter_next(JSStringRopeIter *s)
{
    JSValueConst val;
    JSStringRope *r;

    if (s->stack_len == 0)
        return NULL;
    val = s->stack[--s->stack_len];
    for(;;) {
        if (JS_VALUE_GET_TAG(val) == JS_TAG_STRING)
            return JS_VALUE_GET_STRING(val);
        r = JS_VALUE_GET_STRING_ROPE(val);
        assert(s->stack_len < JS_STRING_ROPE_MAX_DEPTH);
        s->stack[s->stack_len++] = r->right;
        val = r->left;
    }
}

static uint32_t string_rope_get_len(JSValueConst val)
{
    if (JS_VALUE_GET_TAG(val) == JS_TAG_STRING)
        return JS_VALUE_GET_STRING(val)->len;
    else
        return JS_VALUE_GET_STRING_ROPE(val)->len;
}

static int js_string_rope_compare(JSContext *ctx, JSValueConst op1,
                                  JSValueConst op2, BOOL eq_only)
{
    uint32_t len1, len2, len, pos1, pos2, l;
    int res;
    JSStringRopeIter it1, it2;
    JSString *p1, *p2;

    len1 = string_rope_get_len(op1);
    len2 = string_rope_get_len(op2);
    /* no need to go further for equality test if
       different length */
    if (eq_only && len1 != len2)
        return 1;
    len = min_uint32(len1, len2);
    string_rope_iter_init(&it1, op1);
    string_rope_iter_init(&it2, op2);
    p1 = string_rope_iter_next(&it1);
    p2 = string_rope_iter_next(&it2);
    pos1 = 0;
    pos2 = 0;
    while (len != 0) {
        l = min_uint32(p1->len - pos1, p2->len - pos2);
        l = min_uint32(l, len);
        res = js_string_memcmp(p1, pos1, p2, pos2, l);
        if (res != 0)
            return res;
        len -= l;
        pos1 += l;
        if (pos1 >= p1->len) {
            p1 = string_rope_iter_next(&it1);
            pos1 = 0;
        }
        pos2 += l;
        if (pos2 >= p2->len) {
            p2 = string_rope_iter_next(&it2);
            pos2 = 0;
        }
    }

    if (len1 == len2)
        res = 0;
    else if (len1 < len2)
        res = -1;
    else
        res = 1;
    return res;
}

/* 'rope' must be a rope. return a string and modify the rope so that
   it won't need to be linearized again. */
static JSValue js_linearize_string_rope(JSContext *ctx, JSValue rope)
{
    StringBuffer b_s, *b = &b_s;
    JSStringRope *r;
    JSValue ret;

    r = JS_VALUE_GET_STRING_ROPE(rope);

    /* check whether it is already linearized */
    if (JS_VALUE_GET_TAG(r->right) == JS_TAG_STRING &&
        JS_VALUE_GET_STRING(r->right)->len == 0) {
        ret = JS_DupValue(ctx, r->left);
        JS_FreeValue(ctx, rope);
        return ret;
    }
    if (string_buffer_init2(ctx, b, r->len, r->is_wide_char))
        goto fail;
    if (string_buffer_concat_value(b, rope))
        goto fail;
    ret = string_buffer_end(b);
    if (js_rc(r)->ref_count > 1) {
        /* update the rope so that it won't need to be linearized again */
        JS_FreeValue(ctx, r->left);
        JS_FreeValue(ctx, r->right);
        r->left = JS_DupValue(ctx, ret);
        r->right = JS_AtomToString(ctx, JS_ATOM_empty_string);
    }
    JS_FreeValue(ctx, rope);
    return ret;
 fail:
    JS_FreeValue(ctx, rope);
    return JS_EXCEPTION;
}

static JSValue js_rebalancee_string_rope(JSContext *ctx, JSValueConst rope);

/* op1 and op2 must be strings or string ropes */
static JSValue js_new_string_rope(JSContext *ctx, JSValue op1, JSValue op2)
{
    uint32_t len;
    int is_wide_char, depth;
    JSStringRope *r;
    JSValue res;

    if (JS_VALUE_GET_TAG(op1) == JS_TAG_STRING) {
        JSString *p1 = JS_VALUE_GET_STRING(op1);
        len = p1->len;
        is_wide_char = p1->is_wide_char;
        depth = 0;
    } else {
        JSStringRope *r1 = JS_VALUE_GET_STRING_ROPE(op1);
        len = r1->len;
        is_wide_char = r1->is_wide_char;
        depth = r1->depth;
    }

    if (JS_VALUE_GET_TAG(op2) == JS_TAG_STRING) {
        JSString *p2 = JS_VALUE_GET_STRING(op2);
        len += p2->len;
        is_wide_char |= p2->is_wide_char;
    } else {
        JSStringRope *r2 = JS_VALUE_GET_STRING_ROPE(op2);
        len += r2->len;
        is_wide_char |= r2->is_wide_char;
        depth = max_int(depth, r2->depth);
    }
    if (len > JS_STRING_LEN_MAX) {
        JS_ThrowInternalError(ctx, "string too long");
        goto fail;
    }
    r = js_malloc(ctx, sizeof(*r));
    if (!r)
        goto fail;
    js_rc(r)->ref_count = 1;
    r->len = len;
    r->is_wide_char = is_wide_char;
    r->depth = depth + 1;
    r->left = op1;
    r->right = op2;
    res = JS_MKPTR(JS_TAG_STRING_ROPE, r);
    if (r->depth > JS_STRING_ROPE_MAX_DEPTH) {
        JSValue res2;
#ifdef DUMP_ROPE_REBALANCE
        printf("rebalance: initial depth=%d\n", r->depth);
#endif
        res2 = js_rebalancee_string_rope(ctx, res);
#ifdef DUMP_ROPE_REBALANCE
        if (JS_VALUE_GET_TAG(res2) == JS_TAG_STRING_ROPE)
            printf("rebalance: final depth=%d\n", JS_VALUE_GET_STRING_ROPE(res2)->depth);
#endif
        JS_FreeValue(ctx, res);
        return res2;
    } else {
        return res;
    }
 fail:
    JS_FreeValue(ctx, op1);
    JS_FreeValue(ctx, op2);
    return JS_EXCEPTION;
}

#define ROPE_N_BUCKETS 44

/* Fibonacii numbers starting from F_2 */
static const uint32_t rope_bucket_len[ROPE_N_BUCKETS] = {
          1,          2,          3,          5,
          8,         13,         21,         34,
         55,         89,        144,        233,
        377,        610,        987,       1597,
       2584,       4181,       6765,      10946,
      17711,      28657,      46368,      75025,
     121393,     196418,     317811,     514229,
     832040,    1346269,    2178309,    3524578,
    5702887,    9227465,   14930352,   24157817,
   39088169,   63245986,  102334155,  165580141,
  267914296,  433494437,  701408733, 1134903170, /* > JS_STRING_LEN_MAX */
};

static int js_rebalancee_string_rope_rec(JSContext *ctx, JSValue *buckets,
                                          JSValueConst val)
{
    if (JS_VALUE_GET_TAG(val) == JS_TAG_STRING) {
        JSString *p = JS_VALUE_GET_STRING(val);
        uint32_t len, i;
        JSValue a, b;

        len = p->len;
        if (len == 0)
            return 0; /* nothing to do */
        /* find the bucket i so that rope_bucket_len[i] <= len <
           rope_bucket_len[i + 1] and concatenate the ropes in the
           buckets before */
        a = JS_NULL;
        i = 0;
        while (len >= rope_bucket_len[i + 1]) {
            b = buckets[i];
            if (!JS_IsNull(b)) {
                buckets[i] = JS_NULL;
                if (JS_IsNull(a)) {
                    a = b;
                } else {
                    a = js_new_string_rope(ctx, b, a);
                    if (JS_IsException(a))
                        return -1;
                }
            }
            i++;
        }
        if (!JS_IsNull(a)) {
            a = js_new_string_rope(ctx, a, JS_DupValue(ctx, val));
            if (JS_IsException(a))
                return -1;
        } else {
            a = JS_DupValue(ctx, val);
        }
        while (!JS_IsNull(buckets[i])) {
            a = js_new_string_rope(ctx, buckets[i], a);
            buckets[i] = JS_NULL;
            if (JS_IsException(a))
                return -1;
            i++;
        }
        buckets[i] = a;
    } else {
        JSStringRope *r = JS_VALUE_GET_STRING_ROPE(val);
        js_rebalancee_string_rope_rec(ctx, buckets, r->left);
        js_rebalancee_string_rope_rec(ctx, buckets, r->right);
    }
    return 0;
}

/* Return a new rope which is balanced. Algorithm from "Ropes: an
   Alternative to Strings", Hans-J. Boehm, Russ Atkinson and Michael
   Plass. */
static JSValue js_rebalancee_string_rope(JSContext *ctx, JSValueConst rope)
{
    JSValue buckets[ROPE_N_BUCKETS], a, b;
    int i;

    for(i = 0; i < ROPE_N_BUCKETS; i++)
        buckets[i] = JS_NULL;
    if (js_rebalancee_string_rope_rec(ctx, buckets, rope))
        goto fail;
    a = JS_NULL;
    for(i = 0; i < ROPE_N_BUCKETS; i++) {
        b = buckets[i];
        if (!JS_IsNull(b)) {
            buckets[i] = JS_NULL;
            if (JS_IsNull(a)) {
                a = b;
            } else {
                a = js_new_string_rope(ctx, b, a);
                if (JS_IsException(a))
                    goto fail;
            }
        }
    }
    /* fail safe */
    if (JS_IsNull(a))
        return JS_AtomToString(ctx, JS_ATOM_empty_string);
    else
        return a;
 fail:
    for(i = 0; i < ROPE_N_BUCKETS; i++) {
        JS_FreeValue(ctx, buckets[i]);
    }
    return JS_EXCEPTION;
}

/* op1 and op2 are converted to strings. For convenience, op1 or op2 =
   JS_EXCEPTION are accepted and return JS_EXCEPTION.  */
static JSValue JS_ConcatString(JSContext *ctx, JSValue op1, JSValue op2)
{
    JSString *p1, *p2;

    if (unlikely(JS_VALUE_GET_TAG(op1) != JS_TAG_STRING &&
                 JS_VALUE_GET_TAG(op1) != JS_TAG_STRING_ROPE)) {
        op1 = JS_ToStringFree(ctx, op1);
        if (JS_IsException(op1)) {
            JS_FreeValue(ctx, op2);
            return JS_EXCEPTION;
        }
    }
    if (unlikely(JS_VALUE_GET_TAG(op2) != JS_TAG_STRING &&
                 JS_VALUE_GET_TAG(op2) != JS_TAG_STRING_ROPE)) {
        op2 = JS_ToStringFree(ctx, op2);
        if (JS_IsException(op2)) {
            JS_FreeValue(ctx, op1);
            return JS_EXCEPTION;
        }
    }

    /* normal concatenation for short strings */
    if (JS_VALUE_GET_TAG(op2) == JS_TAG_STRING) {
        p2 = JS_VALUE_GET_STRING(op2);
        if (p2->len == 0) {
            JS_FreeValue(ctx, op2);
            return op1;
        }
        if (p2->len <= JS_STRING_ROPE_SHORT_LEN) {
            if (JS_VALUE_GET_TAG(op1) == JS_TAG_STRING) {
                p1 = JS_VALUE_GET_STRING(op1);
                if (p1->len <= JS_STRING_ROPE_SHORT2_LEN) {
                    return JS_ConcatString2(ctx, op1, op2);
                } else {
                    return js_new_string_rope(ctx, op1, op2);
                }
            } else {
                JSStringRope *r1;
                r1 = JS_VALUE_GET_STRING_ROPE(op1);
                if (JS_VALUE_GET_TAG(r1->right) == JS_TAG_STRING &&
                    JS_VALUE_GET_STRING(r1->right)->len <= JS_STRING_ROPE_SHORT_LEN) {
                    JSValue val, ret;
                    val = JS_ConcatString2(ctx, JS_DupValue(ctx, r1->right), op2);
                    if (JS_IsException(val)) {
                        JS_FreeValue(ctx, op1);
                        return JS_EXCEPTION;
                    }
                    ret = js_new_string_rope(ctx, JS_DupValue(ctx, r1->left), val);
                    JS_FreeValue(ctx, op1);
                    return ret;
                }
            }
        }
    } else if (JS_VALUE_GET_TAG(op1) == JS_TAG_STRING) {
        JSStringRope *r2;
        p1 = JS_VALUE_GET_STRING(op1);
        if (p1->len == 0) {
            JS_FreeValue(ctx, op1);
            return op2;
        }
        r2 = JS_VALUE_GET_STRING_ROPE(op2);
        if (JS_VALUE_GET_TAG(r2->left) == JS_TAG_STRING &&
            JS_VALUE_GET_STRING(r2->left)->len <= JS_STRING_ROPE_SHORT_LEN) {
            JSValue val, ret;
            val = JS_ConcatString2(ctx, op1, JS_DupValue(ctx, r2->left));
            if (JS_IsException(val)) {
                JS_FreeValue(ctx, op2);
                return JS_EXCEPTION;
            }
            ret = js_new_string_rope(ctx, val, JS_DupValue(ctx, r2->right));
            JS_FreeValue(ctx, op2);
            return ret;
        }
    }
    return js_new_string_rope(ctx, op1, op2);
}

/* Shape support */

static inline size_t get_shape_size(size_t hash_size, size_t prop_size)
{
    return sizeof(JSShape) + hash_size * sizeof(uint32_t) +
        prop_size * sizeof(JSShapeProperty);
}

static inline JSShapeProperty *get_shape_prop(JSShape *sh)
{
    return (JSShapeProperty *)((uint32_t *)(sh + 1) + sh->prop_hash_mask + 1);
}

static int init_shape_hash(JSRuntime *rt)
{
    rt->shape_hash_bits = 4;   /* 16 shapes */
    rt->shape_hash_size = 1 << rt->shape_hash_bits;
    rt->shape_hash_count = 0;
    rt->shape_hash = js_mallocz_rt(rt, sizeof(rt->shape_hash[0]) *
                                   rt->shape_hash_size);
    if (!rt->shape_hash)
        return -1;
    return 0;
}

/* same magic hash multiplier as the Linux kernel */
static uint32_t shape_hash(uint32_t h, uint32_t val)
{
    return (h + val) * 0x9e370001;
}

/* truncate the shape hash to 'hash_bits' bits */
static uint32_t get_shape_hash(uint32_t h, int hash_bits)
{
    return h >> (32 - hash_bits);
}

static uint32_t shape_initial_hash(JSObject *proto)
{
    uint32_t h;
    h = shape_hash(1, (uintptr_t)proto);
    if (sizeof(proto) > 4)
        h = shape_hash(h, (uint64_t)(uintptr_t)proto >> 32);
    return h;
}

static int resize_shape_hash(JSRuntime *rt, int new_shape_hash_bits)
{
    int new_shape_hash_size, i;
    uint32_t h;
    JSShape **new_shape_hash, *sh, *sh_next;

    new_shape_hash_size = 1 << new_shape_hash_bits;
    new_shape_hash = js_mallocz_rt(rt, sizeof(rt->shape_hash[0]) *
                                   new_shape_hash_size);
    if (!new_shape_hash)
        return -1;
    for(i = 0; i < rt->shape_hash_size; i++) {
        for(sh = rt->shape_hash[i]; sh != NULL; sh = sh_next) {
            sh_next = sh->shape_hash_next;
            h = get_shape_hash(sh->hash, new_shape_hash_bits);
            sh->shape_hash_next = new_shape_hash[h];
            new_shape_hash[h] = sh;
        }
    }
    js_free_rt(rt, rt->shape_hash);
    rt->shape_hash_bits = new_shape_hash_bits;
    rt->shape_hash_size = new_shape_hash_size;
    rt->shape_hash = new_shape_hash;
    return 0;
}

static void js_shape_hash_link(JSRuntime *rt, JSShape *sh)
{
    uint32_t h;
    h = get_shape_hash(sh->hash, rt->shape_hash_bits);
    sh->shape_hash_next = rt->shape_hash[h];
    rt->shape_hash[h] = sh;
    rt->shape_hash_count++;
}

static void js_shape_hash_unlink(JSRuntime *rt, JSShape *sh)
{
    uint32_t h;
    JSShape **psh;

    h = get_shape_hash(sh->hash, rt->shape_hash_bits);
    psh = &rt->shape_hash[h];
    while (*psh != sh)
        psh = &(*psh)->shape_hash_next;
    *psh = sh->shape_hash_next;
    rt->shape_hash_count--;
}

/* create a new empty shape with prototype 'proto'. It is not hashed */
static inline JSShape *js_new_shape_nohash(JSContext *ctx, JSObject *proto,
                                           int hash_size, int prop_size)
{
    JSRuntime *rt = ctx->rt;
    JSShape *sh;

    sh = js_malloc(ctx, get_shape_size(hash_size, prop_size));
    if (!sh)
        return NULL;
    js_rc(sh)->ref_count = 1;
    add_gc_object(rt, &sh->header, JS_GC_OBJ_TYPE_SHAPE);
    if (proto)
        JS_DupValue(ctx, JS_MKPTR(JS_TAG_OBJECT, proto));
    sh->proto = proto;
    memset(sh->hash_table, 0, sizeof(sh->hash_table[0]) * hash_size);
    sh->prop_hash_mask = hash_size - 1;
    sh->prop_size = prop_size;
    sh->prop_count = 0;
    sh->deleted_prop_count = 0;
    sh->is_hashed = FALSE;
    return sh;
}

/* create a new empty shape with prototype 'proto' */
static no_inline JSShape *js_new_shape2(JSContext *ctx, JSObject *proto,
                                        int hash_size, int prop_size)
{
    JSRuntime *rt = ctx->rt;
    JSShape *sh;

    /* resize the shape hash table if necessary */
    if (2 * (rt->shape_hash_count + 1) > rt->shape_hash_size) {
        resize_shape_hash(rt, rt->shape_hash_bits + 1);
    }

    sh = js_new_shape_nohash(ctx, proto, hash_size, prop_size);
    if (!sh)
        return NULL;

    /* insert in the hash table */
    sh->hash = shape_initial_hash(proto);
    sh->is_hashed = TRUE;
    js_shape_hash_link(ctx->rt, sh);
    return sh;
}

static JSShape *js_new_shape(JSContext *ctx, JSObject *proto)
{
    return js_new_shape2(ctx, proto, JS_PROP_INITIAL_HASH_SIZE,
                         JS_PROP_INITIAL_SIZE);
}

/* The shape is cloned. The new shape is not inserted in the shape
   hash table */
static JSShape *js_clone_shape(JSContext *ctx, JSShape *sh1)
{
    JSShape *sh;
    size_t size;
    JSShapeProperty *pr;
    uint32_t i, hash_size;

    hash_size = sh1->prop_hash_mask + 1;
    size = get_shape_size(hash_size, sh1->prop_size);
    sh = js_malloc(ctx, size);
    if (!sh)
        return NULL;
    memcpy(&sh->header + 1, &sh1->header + 1,
           size - sizeof(JSGCObjectHeader));
    js_rc(sh)->ref_count = 1;
    add_gc_object(ctx->rt, &sh->header, JS_GC_OBJ_TYPE_SHAPE);
    sh->is_hashed = FALSE;
    if (sh->proto) {
        JS_DupValue(ctx, JS_MKPTR(JS_TAG_OBJECT, sh->proto));
    }
    for(i = 0, pr = get_shape_prop(sh); i < sh->prop_count; i++, pr++) {
        JS_DupAtom(ctx, pr->atom);
    }
    return sh;
}

static JSShape *js_dup_shape(JSShape *sh)
{
    js_rc(sh)->ref_count++;
    return sh;
}

static void js_free_shape0(JSRuntime *rt, JSShape *sh)
{
    uint32_t i;
    JSShapeProperty *pr;

    assert(js_rc(sh)->ref_count == 0);
    if (sh->is_hashed)
        js_shape_hash_unlink(rt, sh);
    if (sh->proto != NULL) {
        JS_FreeValueRT(rt, JS_MKPTR(JS_TAG_OBJECT, sh->proto));
    }
    pr = get_shape_prop(sh);
    for(i = 0; i < sh->prop_count; i++) {
        JS_FreeAtomRT(rt, pr->atom);
        pr++;
    }
    remove_gc_object(&sh->header);
    js_free_rt(rt, sh);
}

static void js_free_shape(JSRuntime *rt, JSShape *sh)
{
    if (unlikely(--js_rc(sh)->ref_count <= 0)) {
        js_free_shape0(rt, sh);
    }
}

static void js_free_shape_null(JSRuntime *rt, JSShape *sh)
{
    if (sh)
        js_free_shape(rt, sh);
}

/* make space to hold at least 'count' properties */
static no_inline int resize_properties(JSContext *ctx, JSShape **psh,
                                       JSObject *p, uint32_t count)
{
    JSShape *sh;
    uint32_t new_size, new_hash_size, new_hash_mask, i;
    JSShapeProperty *pr;
    intptr_t h;
    JSShape *old_sh;

    sh = *psh;
    new_size = max_int(count, sh->prop_size * 3 / 2);
    /* Reallocate prop array first to avoid crash or size inconsistency
       in case of memory allocation failure */
    if (p) {
        JSProperty *new_prop;
        new_prop = js_realloc(ctx, p->prop, sizeof(new_prop[0]) * new_size);
        if (unlikely(!new_prop))
            return -1;
        p->prop = new_prop;
    }
    new_hash_size = sh->prop_hash_mask + 1;
    while (new_hash_size < new_size)
        new_hash_size = 2 * new_hash_size;
    /* resize the property shapes. Using js_realloc() is not possible in
       case the GC runs during the allocation */
    old_sh = sh;
    sh = js_malloc(ctx, get_shape_size(new_hash_size, new_size));
    if (!sh)
        return -1;
    remove_gc_object(&old_sh->header);

    js_rc(sh)->ref_count = 1;
    add_gc_object(ctx->rt, &sh->header, JS_GC_OBJ_TYPE_SHAPE);

    memcpy(&sh->header + 1, &old_sh->header + 1,
           sizeof(JSShape) - sizeof(JSGCObjectHeader));

    if (new_hash_size != (sh->prop_hash_mask + 1)) {
        /* resize the hash table and the properties */
        new_hash_mask = new_hash_size - 1;
        sh->prop_hash_mask = new_hash_mask;
        memset(sh->hash_table, 0,
               sizeof(sh->hash_table[0]) * new_hash_size);
        memcpy(get_shape_prop(sh), get_shape_prop(old_sh),
               sizeof(JSShapeProperty) * old_sh->prop_count);
        for(i = 0, pr = get_shape_prop(sh); i < sh->prop_count; i++, pr++) {
            if (pr->atom != JS_ATOM_NULL) {
                h = ((uintptr_t)pr->atom & new_hash_mask);
                pr->hash_next = sh->hash_table[h];
                sh->hash_table[h] = i + 1;
            }
        }
    } else {
        /* just copy the previous hash table and the properties */
        memcpy(sh->hash_table, old_sh->hash_table,
               sizeof(sh->hash_table[0]) * new_hash_size);

        memcpy(get_shape_prop(sh), get_shape_prop(old_sh),
               sizeof(JSShapeProperty) * old_sh->prop_count);
    }
    js_free(ctx, old_sh);
    *psh = sh;
    sh->prop_size = new_size;
    return 0;
}

/* remove the deleted properties. */
static int compact_properties(JSContext *ctx, JSObject *p)
{
    JSShape *sh, *old_sh;
    intptr_t h;
    uint32_t new_hash_size, i, j, new_hash_mask, new_size;
    JSShapeProperty *old_pr, *pr;
    JSProperty *prop, *new_prop;

    sh = p->shape;
    assert(!sh->is_hashed);

    new_size = max_int(JS_PROP_INITIAL_SIZE,
                       sh->prop_count - sh->deleted_prop_count);
    assert(new_size <= sh->prop_size);

    new_hash_size = sh->prop_hash_mask + 1;
    while ((new_hash_size / 2) >= new_size)
        new_hash_size = new_hash_size / 2;
    new_hash_mask = new_hash_size - 1;

    /* resize the hash table and the properties */
    old_sh = sh;
    sh = js_malloc(ctx, get_shape_size(new_hash_size, new_size));
    if (!sh)
        return -1;
    remove_gc_object(&old_sh->header);

    js_rc(sh)->ref_count = 1;
    add_gc_object(ctx->rt, &sh->header, JS_GC_OBJ_TYPE_SHAPE);

    memcpy(&sh->header + 1, &old_sh->header + 1,
           sizeof(JSShape) - sizeof(JSGCObjectHeader));

    memset(sh->hash_table, 0, sizeof(sh->hash_table[0]) * new_hash_size);
    sh->prop_hash_mask = new_hash_mask;

    j = 0;
    old_pr = get_shape_prop(old_sh);
    pr = get_shape_prop(sh);
    prop = p->prop;
    for(i = 0; i < sh->prop_count; i++) {
        if (old_pr->atom != JS_ATOM_NULL) {
            pr->atom = old_pr->atom;
            pr->flags = old_pr->flags;
            h = ((uintptr_t)old_pr->atom & new_hash_mask);
            pr->hash_next = sh->hash_table[h];
            sh->hash_table[h] = j + 1;
            prop[j] = prop[i];
            j++;
            pr++;
        }
        old_pr++;
    }
    assert(j == (sh->prop_count - sh->deleted_prop_count));
    sh->prop_size = new_size;
    sh->deleted_prop_count = 0;
    sh->prop_count = j;

    p->shape = sh;
    js_free(ctx, old_sh);

    /* reduce the size of the object properties */
    new_prop = js_realloc(ctx, p->prop, sizeof(new_prop[0]) * new_size);
    if (new_prop)
        p->prop = new_prop;
    return 0;
}

static int add_shape_property(JSContext *ctx, JSShape **psh,
                              JSObject *p, JSAtom atom, int prop_flags)
{
    JSRuntime *rt = ctx->rt;
    JSShape *sh = *psh;
    JSShapeProperty *pr, *prop;
    uint32_t hash_mask, new_shape_hash = 0;
    intptr_t h;

    /* update the shape hash */
    if (sh->is_hashed) {
        js_shape_hash_unlink(rt, sh);
        new_shape_hash = shape_hash(shape_hash(sh->hash, atom), prop_flags);
    }

    if (unlikely(sh->prop_count >= sh->prop_size)) {
        if (resize_properties(ctx, psh, p, sh->prop_count + 1)) {
            /* in case of error, reinsert in the hash table.
               sh is still valid if resize_properties() failed */
            if (sh->is_hashed)
                js_shape_hash_link(rt, sh);
            return -1;
        }
        sh = *psh;
    }
    if (sh->is_hashed) {
        sh->hash = new_shape_hash;
        js_shape_hash_link(rt, sh);
    }
    /* Initialize the new shape property.
       The object property at p->prop[sh->prop_count] is uninitialized */
    prop = get_shape_prop(sh);
    pr = &prop[sh->prop_count++];
    pr->atom = JS_DupAtom(ctx, atom);
    pr->flags = prop_flags;
    /* add in hash table */
    hash_mask = sh->prop_hash_mask;
    h = atom & hash_mask;
    pr->hash_next = sh->hash_table[h];
    sh->hash_table[h] = sh->prop_count;
    return 0;
}

/* find a hashed empty shape matching the prototype. Return NULL if
   not found */
static JSShape *find_hashed_shape_proto(JSRuntime *rt, JSObject *proto)
{
    JSShape *sh1;
    uint32_t h, h1;

    h = shape_initial_hash(proto);
    h1 = get_shape_hash(h, rt->shape_hash_bits);
    for(sh1 = rt->shape_hash[h1]; sh1 != NULL; sh1 = sh1->shape_hash_next) {
        if (sh1->hash == h &&
            sh1->proto == proto &&
            sh1->prop_count == 0) {
            return sh1;
        }
    }
    return NULL;
}

/* find a hashed shape matching sh + (prop, prop_flags). Return NULL if
   not found */
static JSShape *find_hashed_shape_prop(JSRuntime *rt, JSShape *sh,
                                       JSAtom atom, int prop_flags)
{
    JSShape *sh1;
    uint32_t h, h1, i, n;

    h = sh->hash;
    h = shape_hash(h, atom);
    h = shape_hash(h, prop_flags);
    h1 = get_shape_hash(h, rt->shape_hash_bits);
    for(sh1 = rt->shape_hash[h1]; sh1 != NULL; sh1 = sh1->shape_hash_next) {
        /* we test the hash first so that the rest is done only if the
           shapes really match */
        if (sh1->hash == h &&
            sh1->proto == sh->proto &&
            sh1->prop_count == ((n = sh->prop_count) + 1)) {
            JSShapeProperty *prop = get_shape_prop(sh);
            JSShapeProperty *prop1 = get_shape_prop(sh1);
            for(i = 0; i < n; i++) {
                if (unlikely(prop1[i].atom != prop[i].atom) ||
                    unlikely(prop1[i].flags != prop[i].flags))
                    goto next;
            }
            if (unlikely(prop1[n].atom != atom) ||
                unlikely(prop1[n].flags != prop_flags))
                goto next;
            return sh1;
        }
    next: ;
    }
    return NULL;
}

static __maybe_unused void JS_DumpShape(JSRuntime *rt, int i, JSShape *sh)
{
    char atom_buf[ATOM_GET_STR_BUF_SIZE];
    int j;

    /* XXX: should output readable class prototype */
    printf("%5d %3d%c %14p %5d %5d", i,
           js_rc(sh)->ref_count, " *"[sh->is_hashed],
           (void *)sh->proto, sh->prop_size, sh->prop_count);
    for(j = 0; j < sh->prop_count; j++) {
        printf(" %s", JS_AtomGetStrRT(rt, atom_buf, sizeof(atom_buf),
                                      get_shape_prop(sh)[j].atom));
    }
    printf("\n");
}

static __maybe_unused void JS_DumpShapes(JSRuntime *rt)
{
    int i;
    JSShape *sh;
    struct list_head *el;
    JSObject *p;
    JSGCObjectHeader *gp;

    printf("JSShapes: {\n");
    printf("%5s %4s %14s %5s %5s %s\n", "SLOT", "REFS", "PROTO", "SIZE", "COUNT", "PROPS");
    for(i = 0; i < rt->shape_hash_size; i++) {
        for(sh = rt->shape_hash[i]; sh != NULL; sh = sh->shape_hash_next) {
            JS_DumpShape(rt, i, sh);
            assert(sh->is_hashed);
        }
    }
    /* dump non-hashed shapes */
    list_for_each(el, &rt->gc_obj_list) {
        gp = list_entry(el, JSGCObjectHeader, link);
        if (js_rc(gp)->gc_obj_type == JS_GC_OBJ_TYPE_JS_OBJECT) {
            p = (JSObject *)gp;
            if (!p->shape->is_hashed) {
                JS_DumpShape(rt, -1, p->shape);
            }
        }
    }
    printf("}\n");
}

/* 'props[]' is used to initialized the object properties. The number
   of elements depends on the shape. */
static JSValue JS_NewObjectFromShape(JSContext *ctx, JSShape *sh, JSClassID class_id,
                                     JSProperty *props)
{
    JSObject *p;
    int i;

    js_trigger_gc(ctx->rt, sizeof(JSObject));
    p = js_malloc(ctx, sizeof(JSObject));
    if (unlikely(!p))
        goto fail;
    p->class_id = class_id;
    p->is_std_array_prototype = 0;
    p->extensible = TRUE;
    p->free_mark = 0;
    p->is_exotic = 0;
    p->fast_array = 0;
    p->is_constructor = 0;
    p->has_immutable_prototype = 0;
    p->tmp_mark = 0;
    p->is_HTMLDDA = 0;
    p->weakref_count = 0;
    p->u.opaque = NULL;
    p->shape = sh;
    p->prop = js_malloc(ctx, sizeof(JSProperty) * sh->prop_size);
    if (unlikely(!p->prop)) {
        js_free(ctx, p);
    fail:
        if (props) {
            JSShapeProperty *prs = get_shape_prop(sh);
            for(i = 0; i < sh->prop_count; i++) {
                free_property(ctx->rt, &props[i], prs->flags);
                prs++;
            }
        }
        js_free_shape(ctx->rt, sh);
        return JS_EXCEPTION;
    }

    switch(class_id) {
    case JS_CLASS_OBJECT:
        break;
    case JS_CLASS_ARRAY:
        {
            JSProperty *pr;
            p->is_exotic = 1;
            p->fast_array = 1;
            p->u.array.u.values = NULL;
            p->u.array.count = 0;
            p->u.array.u1.size = 0;
            if (!props) {
                /* XXX: remove */
                /* the length property is always the first one */
                if (likely(sh == ctx->array_shape)) {
                    pr = &p->prop[0];
                } else {
                    /* only used for the first array */
                    /* cannot fail */
                    pr = add_property(ctx, p, JS_ATOM_length,
                                      JS_PROP_WRITABLE | JS_PROP_LENGTH);
                }
                pr->u.value = JS_NewInt32(ctx, 0);
            }
        }
        break;
    case JS_CLASS_C_FUNCTION:
        p->prop[0].u.value = JS_UNDEFINED;
        break;
    case JS_CLASS_ARGUMENTS:
    case JS_CLASS_MAPPED_ARGUMENTS:
    case JS_CLASS_UINT8C_ARRAY:
    case JS_CLASS_INT8_ARRAY:
    case JS_CLASS_UINT8_ARRAY:
    case JS_CLASS_INT16_ARRAY:
    case JS_CLASS_UINT16_ARRAY:
    case JS_CLASS_INT32_ARRAY:
    case JS_CLASS_UINT32_ARRAY:
    case JS_CLASS_BIG_INT64_ARRAY:
    case JS_CLASS_BIG_UINT64_ARRAY:
    case JS_CLASS_FLOAT16_ARRAY:
    case JS_CLASS_FLOAT32_ARRAY:
    case JS_CLASS_FLOAT64_ARRAY:
        p->is_exotic = 1;
        p->fast_array = 1;
        p->u.array.u.ptr = NULL;
        p->u.array.count = 0;
        break;
    case JS_CLASS_DATAVIEW:
        p->u.array.u.ptr = NULL;
        p->u.array.count = 0;
        break;
    case JS_CLASS_NUMBER:
    case JS_CLASS_STRING:
    case JS_CLASS_BOOLEAN:
    case JS_CLASS_SYMBOL:
    case JS_CLASS_DATE:
    case JS_CLASS_BIG_INT:
        p->u.object_data = JS_UNDEFINED;
        goto set_exotic;
    case JS_CLASS_REGEXP:
        p->u.regexp.pattern = NULL;
        p->u.regexp.bytecode = NULL;
        break;
    case JS_CLASS_GLOBAL_OBJECT:
        p->u.global_object.uninitialized_vars = JS_UNDEFINED;
        break;
    default:
    set_exotic:
        if (ctx->rt->class_array[class_id].exotic) {
            p->is_exotic = 1;
        }
        break;
    }
    js_rc(p)->ref_count = 1;
    add_gc_object(ctx->rt, &p->header, JS_GC_OBJ_TYPE_JS_OBJECT);
    if (props) {
        for(i = 0; i < sh->prop_count; i++)
            p->prop[i] = props[i];
    }
    return JS_MKPTR(JS_TAG_OBJECT, p);
}

static JSObject *get_proto_obj(JSValueConst proto_val)
{
    if (JS_VALUE_GET_TAG(proto_val) != JS_TAG_OBJECT)
        return NULL;
    else
        return JS_VALUE_GET_OBJ(proto_val);
}

/* WARNING: proto must be an object or JS_NULL */
JSValue JS_NewObjectProtoClass(JSContext *ctx, JSValueConst proto_val,
                               JSClassID class_id)
{
    JSShape *sh;
    JSObject *proto;

    proto = get_proto_obj(proto_val);
    sh = find_hashed_shape_proto(ctx->rt, proto);
    if (likely(sh)) {
        sh = js_dup_shape(sh);
    } else {
        sh = js_new_shape(ctx, proto);
        if (!sh)
            return JS_EXCEPTION;
    }
    return JS_NewObjectFromShape(ctx, sh, class_id, NULL);
}

/* WARNING: the shape is not hashed. It is used for objects where
   factorizing the shape is not relevant (prototypes, constructors) */
static JSValue JS_NewObjectProtoClassAlloc(JSContext *ctx, JSValueConst proto_val,
                                           JSClassID class_id, int n_alloc_props)
{
    JSShape *sh;
    JSObject *proto;
    int hash_size, hash_bits;

    if (n_alloc_props <= JS_PROP_INITIAL_SIZE) {
        n_alloc_props = JS_PROP_INITIAL_SIZE;
        hash_size = JS_PROP_INITIAL_HASH_SIZE;
    } else {
        hash_bits = 32 - clz32(n_alloc_props - 1); /* ceil(log2(radix)) */
        hash_size = 1 << hash_bits;
    }
    proto = get_proto_obj(proto_val);
    sh = js_new_shape_nohash(ctx, proto, hash_size, n_alloc_props);
    if (!sh)
        return JS_EXCEPTION;
    return JS_NewObjectFromShape(ctx, sh, class_id, NULL);
}

#if 0
static JSValue JS_GetObjectData(JSContext *ctx, JSValueConst obj)
{
    JSObject *p;

    if (JS_VALUE_GET_TAG(obj) == JS_TAG_OBJECT) {
        p = JS_VALUE_GET_OBJ(obj);
        switch(p->class_id) {
        case JS_CLASS_NUMBER:
        case JS_CLASS_STRING:
        case JS_CLASS_BOOLEAN:
        case JS_CLASS_SYMBOL:
        case JS_CLASS_DATE:
        case JS_CLASS_BIG_INT:
            return JS_DupValue(ctx, p->u.object_data);
        }
    }
    return JS_UNDEFINED;
}
#endif

static int JS_SetObjectData(JSContext *ctx, JSValueConst obj, JSValue val)
{
    JSObject *p;

    if (JS_VALUE_GET_TAG(obj) == JS_TAG_OBJECT) {
        p = JS_VALUE_GET_OBJ(obj);
        switch(p->class_id) {
        case JS_CLASS_NUMBER:
        case JS_CLASS_STRING:
        case JS_CLASS_BOOLEAN:
        case JS_CLASS_SYMBOL:
        case JS_CLASS_DATE:
        case JS_CLASS_BIG_INT:
            JS_FreeValue(ctx, p->u.object_data);
            p->u.object_data = val; /* for JS_CLASS_STRING, 'val' must
                                       be JS_TAG_STRING (and not a
                                       rope) */
            return 0;
        }
    }
    JS_FreeValue(ctx, val);
    if (!JS_IsException(obj))
        JS_ThrowTypeError(ctx, "invalid object type");
    return -1;
}

JSValue JS_NewObjectClass(JSContext *ctx, int class_id)
{
    return JS_NewObjectProtoClass(ctx, ctx->class_proto[class_id], class_id);
}

JSValue JS_NewObjectProto(JSContext *ctx, JSValueConst proto)
{
    return JS_NewObjectProtoClass(ctx, proto, JS_CLASS_OBJECT);
}

JSValue JS_NewArray(JSContext *ctx)
{
    return JS_NewObjectFromShape(ctx, js_dup_shape(ctx->array_shape),
                                 JS_CLASS_ARRAY, NULL);
}

JSValue JS_NewObject(JSContext *ctx)
{
    /* inline JS_NewObjectClass(ctx, JS_CLASS_OBJECT); */
    return JS_NewObjectProtoClass(ctx, ctx->class_proto[JS_CLASS_OBJECT], JS_CLASS_OBJECT);
}

static void js_function_set_properties(JSContext *ctx, JSValueConst func_obj,
                                       JSAtom name, int len)
{
    /* ES6 feature non compatible with ES5.1: length is configurable */
    JS_DefinePropertyValue(ctx, func_obj, JS_ATOM_length, JS_NewInt32(ctx, len),
                           JS_PROP_CONFIGURABLE);
    JS_DefinePropertyValue(ctx, func_obj, JS_ATOM_name,
                           JS_AtomToString(ctx, name), JS_PROP_CONFIGURABLE);
}

static BOOL js_class_has_bytecode(JSClassID class_id)
{
    return (class_id == JS_CLASS_BYTECODE_FUNCTION ||
            class_id == JS_CLASS_GENERATOR_FUNCTION ||
            class_id == JS_CLASS_ASYNC_FUNCTION ||
            class_id == JS_CLASS_ASYNC_GENERATOR_FUNCTION);
}

/* return NULL without exception if not a function or no bytecode */
static JSFunctionBytecode *JS_GetFunctionBytecode(JSValueConst val)
{
    JSObject *p;
    if (JS_VALUE_GET_TAG(val) != JS_TAG_OBJECT)
        return NULL;
    p = JS_VALUE_GET_OBJ(val);
    if (!js_class_has_bytecode(p->class_id))
        return NULL;
    return p->u.func.function_bytecode;
}

static void js_method_set_home_object(JSContext *ctx, JSValueConst func_obj,
                                      JSValueConst home_obj)
{
    JSObject *p, *p1;
    JSFunctionBytecode *b;

    if (JS_VALUE_GET_TAG(func_obj) != JS_TAG_OBJECT)
        return;
    p = JS_VALUE_GET_OBJ(func_obj);
    if (!js_class_has_bytecode(p->class_id))
        return;
    b = p->u.func.function_bytecode;
    if (b->need_home_object) {
        p1 = p->u.func.home_object;
        if (p1) {
            JS_FreeValue(ctx, JS_MKPTR(JS_TAG_OBJECT, p1));
        }
        if (JS_VALUE_GET_TAG(home_obj) == JS_TAG_OBJECT)
            p1 = JS_VALUE_GET_OBJ(JS_DupValue(ctx, home_obj));
        else
            p1 = NULL;
        p->u.func.home_object = p1;
    }
}

static JSValue js_get_function_name(JSContext *ctx, JSAtom name)
{
    JSValue name_str;

    name_str = JS_AtomToString(ctx, name);
    if (JS_AtomSymbolHasDescription(ctx, name)) {
        name_str = JS_ConcatString3(ctx, "[", name_str, "]");
    }
    return name_str;
}

/* Modify the name of a method according to the atom and
   'flags'. 'flags' is a bitmask of JS_PROP_HAS_GET and
   JS_PROP_HAS_SET. Also set the home object of the method.
   Return < 0 if exception. */
static int js_method_set_properties(JSContext *ctx, JSValueConst func_obj,
                                    JSAtom name, int flags, JSValueConst home_obj)
{
    JSValue name_str;

    name_str = js_get_function_name(ctx, name);
    if (flags & JS_PROP_HAS_GET) {
        name_str = JS_ConcatString3(ctx, "get ", name_str, "");
    } else if (flags & JS_PROP_HAS_SET) {
        name_str = JS_ConcatString3(ctx, "set ", name_str, "");
    }
    if (JS_IsException(name_str))
        return -1;
    if (JS_DefinePropertyValue(ctx, func_obj, JS_ATOM_name, name_str,
                               JS_PROP_CONFIGURABLE) < 0)
        return -1;
    js_method_set_home_object(ctx, func_obj, home_obj);
    return 0;
}

/* Note: at least 'length' arguments will be readable in 'argv' */
static JSValue JS_NewCFunction3(JSContext *ctx, JSCFunction *func,
                                const char *name,
                                int length, JSCFunctionEnum cproto, int magic,
                                JSValueConst proto_val, int n_fields)
{
    JSValue func_obj;
    JSObject *p;
    JSAtom name_atom;

    if (n_fields > 0) {
        func_obj = JS_NewObjectProtoClassAlloc(ctx, proto_val, JS_CLASS_C_FUNCTION, n_fields);
    } else {
        func_obj = JS_NewObjectProtoClass(ctx, proto_val, JS_CLASS_C_FUNCTION);
    }
    if (JS_IsException(func_obj))
        return func_obj;
    p = JS_VALUE_GET_OBJ(func_obj);
    p->u.cfunc.realm = JS_DupContext(ctx);
    p->u.cfunc.c_function.generic = func;
    p->u.cfunc.length = length;
    p->u.cfunc.cproto = cproto;
    p->u.cfunc.magic = magic;
    p->is_constructor = (cproto == JS_CFUNC_constructor ||
                         cproto == JS_CFUNC_constructor_magic ||
                         cproto == JS_CFUNC_constructor_or_func ||
                         cproto == JS_CFUNC_constructor_or_func_magic);
    if (!name)
        name = "";
    name_atom = JS_NewAtom(ctx, name);
    if (name_atom == JS_ATOM_NULL) {
        JS_FreeValue(ctx, func_obj);
        return JS_EXCEPTION;
    }
    js_function_set_properties(ctx, func_obj, name_atom, length);
    JS_FreeAtom(ctx, name_atom);
    return func_obj;
}

/* Note: at least 'length' arguments will be readable in 'argv' */
JSValue JS_NewCFunction2(JSContext *ctx, JSCFunction *func,
                         const char *name,
                         int length, JSCFunctionEnum cproto, int magic)
{
    return JS_NewCFunction3(ctx, func, name, length, cproto, magic,
                            ctx->function_proto, 0);
}

typedef struct JSCFunctionDataRecord {
    JSCFunctionData *func;
    uint8_t length;
    uint8_t data_len;
    uint16_t magic;
    JSValue data[0];
} JSCFunctionDataRecord;

static void js_c_function_data_finalizer(JSRuntime *rt, JSValue val)
{
    JSCFunctionDataRecord *s = JS_GetOpaque(val, JS_CLASS_C_FUNCTION_DATA);
    int i;

    if (s) {
        for(i = 0; i < s->data_len; i++) {
            JS_FreeValueRT(rt, s->data[i]);
        }
        js_free_rt(rt, s);
    }
}

static void js_c_function_data_mark(JSRuntime *rt, JSValueConst val,
                                    JS_MarkFunc *mark_func)
{
    JSCFunctionDataRecord *s = JS_GetOpaque(val, JS_CLASS_C_FUNCTION_DATA);
    int i;

    if (s) {
        for(i = 0; i < s->data_len; i++) {
            JS_MarkValue(rt, s->data[i], mark_func);
        }
    }
}

static JSValue js_c_function_data_call(JSContext *ctx, JSValueConst func_obj,
                                       JSValueConst this_val,
                                       int argc, JSValueConst *argv, int flags)
{
    JSCFunctionDataRecord *s = JS_GetOpaque(func_obj, JS_CLASS_C_FUNCTION_DATA);
    JSValueConst *arg_buf;
    int i;

    /* XXX: could add the function on the stack for debug */
    if (unlikely(argc < s->length)) {
        arg_buf = alloca(sizeof(arg_buf[0]) * s->length);
        for(i = 0; i < argc; i++)
            arg_buf[i] = argv[i];
        for(i = argc; i < s->length; i++)
            arg_buf[i] = JS_UNDEFINED;
    } else {
        arg_buf = argv;
    }

    return s->func(ctx, this_val, argc, arg_buf, s->magic, s->data);
}

JSValue JS_NewCFunctionData(JSContext *ctx, JSCFunctionData *func,
                            int length, int magic, int data_len,
                            JSValueConst *data)
{
    JSCFunctionDataRecord *s;
    JSValue func_obj;
    int i;

    func_obj = JS_NewObjectProtoClass(ctx, ctx->function_proto,
                                      JS_CLASS_C_FUNCTION_DATA);
    if (JS_IsException(func_obj))
        return func_obj;
    s = js_malloc(ctx, sizeof(*s) + data_len * sizeof(JSValue));
    if (!s) {
        JS_FreeValue(ctx, func_obj);
        return JS_EXCEPTION;
    }
    s->func = func;
    s->length = length;
    s->data_len = data_len;
    s->magic = magic;
    for(i = 0; i < data_len; i++)
        s->data[i] = JS_DupValue(ctx, data[i]);
    JS_SetOpaque(func_obj, s);
    js_function_set_properties(ctx, func_obj,
                               JS_ATOM_empty_string, length);
    return func_obj;
}

static JSContext *js_autoinit_get_realm(JSProperty *pr)
{
    return (JSContext *)(pr->u.init.realm_and_id & ~3);
}

static JSAutoInitIDEnum js_autoinit_get_id(JSProperty *pr)
{
    return pr->u.init.realm_and_id & 3;
}

static void js_autoinit_free(JSRuntime *rt, JSProperty *pr)
{
    JS_FreeContext(js_autoinit_get_realm(pr));
}

static void js_autoinit_mark(JSRuntime *rt, JSProperty *pr,
                             JS_MarkFunc *mark_func)
{
    mark_func(rt, &js_autoinit_get_realm(pr)->header);
}

static void free_property(JSRuntime *rt, JSProperty *pr, int prop_flags)
{
    if (unlikely(prop_flags & JS_PROP_TMASK)) {
        if ((prop_flags & JS_PROP_TMASK) == JS_PROP_GETSET) {
            if (pr->u.getset.getter)
                JS_FreeValueRT(rt, JS_MKPTR(JS_TAG_OBJECT, pr->u.getset.getter));
            if (pr->u.getset.setter)
                JS_FreeValueRT(rt, JS_MKPTR(JS_TAG_OBJECT, pr->u.getset.setter));
        } else if ((prop_flags & JS_PROP_TMASK) == JS_PROP_VARREF) {
            free_var_ref(rt, pr->u.var_ref);
        } else if ((prop_flags & JS_PROP_TMASK) == JS_PROP_AUTOINIT) {
            js_autoinit_free(rt, pr);
        }
    } else {
        JS_FreeValueRT(rt, pr->u.value);
    }
}

static force_inline JSShapeProperty *find_own_property1(JSObject *p,
                                                        JSAtom atom)
{
    JSShape *sh;
    JSShapeProperty *pr, *prop;
    intptr_t h;
    sh = p->shape;
    h = (uintptr_t)atom & sh->prop_hash_mask;
    h = sh->hash_table[h];
    prop = get_shape_prop(sh);
    while (h) {
        pr = &prop[h - 1];
        if (likely(pr->atom == atom)) {
            return pr;
        }
        h = pr->hash_next;
    }
    return NULL;
}

static force_inline JSShapeProperty *find_own_property(JSProperty **ppr,
                                                       JSObject *p,
                                                       JSAtom atom)
{
    JSShape *sh;
    JSShapeProperty *pr, *prop;
    intptr_t h;
    sh = p->shape;
    h = (uintptr_t)atom & sh->prop_hash_mask;
    h = sh->hash_table[h];
    prop = get_shape_prop(sh);
    while (h) {
        pr = &prop[h - 1];
        if (likely(pr->atom == atom)) {
            *ppr = &p->prop[h - 1];
            /* the compiler should be able to assume that pr != NULL here */
            return pr;
        }
        h = pr->hash_next;
    }
    *ppr = NULL;
    return NULL;
}

/* indicate that the object may be part of a function prototype cycle */
static void set_cycle_flag(JSContext *ctx, JSValueConst obj)
{
}

static void free_var_ref(JSRuntime *rt, JSVarRef *var_ref)
{
    if (var_ref) {
        assert(js_rc(var_ref)->ref_count > 0);
        if (--js_rc(var_ref)->ref_count == 0) {
            if (var_ref->is_detached) {
                JS_FreeValueRT(rt, var_ref->value);
            } else {
                JSStackFrame *sf = var_ref->stack_frame;
                assert(sf->var_refs[var_ref->var_ref_idx] == var_ref);
                sf->var_refs[var_ref->var_ref_idx] = NULL;
                if (sf->js_mode & JS_MODE_ASYNC) {
                    JSAsyncFunctionState *async_func = container_of(sf, JSAsyncFunctionState, frame);
                    async_func_free(rt, async_func);
                }
            }
            remove_gc_object(&var_ref->header);
            js_free_rt(rt, var_ref);
        }
    }
}

static void js_array_finalizer(JSRuntime *rt, JSValue val)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    int i;

    for(i = 0; i < p->u.array.count; i++) {
        JS_FreeValueRT(rt, p->u.array.u.values[i]);
    }
    js_free_rt(rt, p->u.array.u.values);
}

static void js_array_mark(JSRuntime *rt, JSValueConst val,
                          JS_MarkFunc *mark_func)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    int i;

    for(i = 0; i < p->u.array.count; i++) {
        JS_MarkValue(rt, p->u.array.u.values[i], mark_func);
    }
}

static void js_object_data_finalizer(JSRuntime *rt, JSValue val)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JS_FreeValueRT(rt, p->u.object_data);
    p->u.object_data = JS_UNDEFINED;
}

static void js_object_data_mark(JSRuntime *rt, JSValueConst val,
                                JS_MarkFunc *mark_func)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JS_MarkValue(rt, p->u.object_data, mark_func);
}

static void js_c_function_finalizer(JSRuntime *rt, JSValue val)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);

    if (p->u.cfunc.realm)
        JS_FreeContext(p->u.cfunc.realm);
}

static void js_c_function_mark(JSRuntime *rt, JSValueConst val,
                               JS_MarkFunc *mark_func)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);

    if (p->u.cfunc.realm)
        mark_func(rt, &p->u.cfunc.realm->header);
}

static void js_bytecode_function_finalizer(JSRuntime *rt, JSValue val)
{
    JSObject *p1, *p = JS_VALUE_GET_OBJ(val);
    JSFunctionBytecode *b;
    JSVarRef **var_refs;
    int i;

    p1 = p->u.func.home_object;
    if (p1) {
        JS_FreeValueRT(rt, JS_MKPTR(JS_TAG_OBJECT, p1));
    }
    b = p->u.func.function_bytecode;
    if (b) {
        var_refs = p->u.func.var_refs;
        if (var_refs) {
            for(i = 0; i < b->closure_var_count; i++)
                free_var_ref(rt, var_refs[i]);
            js_free_rt(rt, var_refs);
        }
        JS_FreeValueRT(rt, JS_MKPTR(JS_TAG_FUNCTION_BYTECODE, b));
    }
}

static void js_bytecode_function_mark(JSRuntime *rt, JSValueConst val,
                                      JS_MarkFunc *mark_func)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JSVarRef **var_refs = p->u.func.var_refs;
    JSFunctionBytecode *b = p->u.func.function_bytecode;
    int i;

    if (p->u.func.home_object) {
        JS_MarkValue(rt, JS_MKPTR(JS_TAG_OBJECT, p->u.func.home_object),
                     mark_func);
    }
    if (b) {
        if (var_refs) {
            for(i = 0; i < b->closure_var_count; i++) {
                JSVarRef *var_ref = var_refs[i];
                if (var_ref) {
                    mark_func(rt, &var_ref->header);
                }
            }
        }
        /* must mark the function bytecode because template objects may be
           part of a cycle */
        JS_MarkValue(rt, JS_MKPTR(JS_TAG_FUNCTION_BYTECODE, b), mark_func);
    }
}

static void js_bound_function_finalizer(JSRuntime *rt, JSValue val)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JSBoundFunction *bf = p->u.bound_function;
    int i;

    JS_FreeValueRT(rt, bf->func_obj);
    JS_FreeValueRT(rt, bf->this_val);
    for(i = 0; i < bf->argc; i++) {
        JS_FreeValueRT(rt, bf->argv[i]);
    }
    js_free_rt(rt, bf);
}

static void js_bound_function_mark(JSRuntime *rt, JSValueConst val,
                                JS_MarkFunc *mark_func)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JSBoundFunction *bf = p->u.bound_function;
    int i;

    JS_MarkValue(rt, bf->func_obj, mark_func);
    JS_MarkValue(rt, bf->this_val, mark_func);
    for(i = 0; i < bf->argc; i++)
        JS_MarkValue(rt, bf->argv[i], mark_func);
}

static void js_for_in_iterator_finalizer(JSRuntime *rt, JSValue val)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JSForInIterator *it = p->u.for_in_iterator;
    int i;

    JS_FreeValueRT(rt, it->obj);
    if (!it->is_array) {
        for(i = 0; i < it->atom_count; i++) {
            JS_FreeAtomRT(rt, it->tab_atom[i].atom);
        }
        js_free_rt(rt, it->tab_atom);
    }
    js_free_rt(rt, it);
}

static void js_for_in_iterator_mark(JSRuntime *rt, JSValueConst val,
                                JS_MarkFunc *mark_func)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JSForInIterator *it = p->u.for_in_iterator;
    JS_MarkValue(rt, it->obj, mark_func);
}

static void free_object(JSRuntime *rt, JSObject *p)
{
    int i;
    JSClassFinalizer *finalizer;
    JSShape *sh;
    JSShapeProperty *pr;

    p->free_mark = 1; /* used to tell the object is invalid when
                         freeing cycles */
    /* free all the fields */
    sh = p->shape;
    pr = get_shape_prop(sh);
    for(i = 0; i < sh->prop_count; i++) {
        free_property(rt, &p->prop[i], pr->flags);
        pr++;
    }
    js_free_rt(rt, p->prop);
    /* as an optimization we destroy the shape immediately without
       putting it in gc_zero_ref_count_list */
    js_free_shape(rt, sh);

    /* fail safe */
    p->shape = NULL;
    p->prop = NULL;

    finalizer = rt->class_array[p->class_id].finalizer;
    if (finalizer)
        (*finalizer)(rt, JS_MKPTR(JS_TAG_OBJECT, p));

    /* fail safe */
    p->class_id = 0;
    p->u.opaque = NULL;
    p->u.func.var_refs = NULL;
    p->u.func.home_object = NULL;

    remove_gc_object(&p->header);
    if (rt->gc_phase == JS_GC_PHASE_REMOVE_CYCLES) {
        if (js_rc(p)->ref_count == 0 && p->weakref_count == 0) {
            js_free_rt(rt, p);
        } else {
            /* keep the object structure because there are may be
               references to it */
            list_add_tail(&p->header.link, &rt->gc_zero_ref_count_list);
        }
    } else {
        /* keep the object structure in case there are weak references to it */
        if (p->weakref_count == 0) {
            js_free_rt(rt, p);
        } else {
            js_rc(p)->mark = 0; /* reset the mark so that the weakref can be freed */
        }
    }
}

static void free_gc_object(JSRuntime *rt, JSGCObjectHeader *gp)
{
    switch(js_rc(gp)->gc_obj_type) {
    case JS_GC_OBJ_TYPE_JS_OBJECT:
        free_object(rt, (JSObject *)gp);
        break;
    case JS_GC_OBJ_TYPE_FUNCTION_BYTECODE:
        free_function_bytecode(rt, (JSFunctionBytecode *)gp);
        break;
    case JS_GC_OBJ_TYPE_ASYNC_FUNCTION:
        __async_func_free(rt, (JSAsyncFunctionState *)gp);
        break;
    case JS_GC_OBJ_TYPE_MODULE:
        js_free_module_def(rt, (JSModuleDef *)gp);
        break;
    default:
        abort();
    }
}

static void free_zero_refcount(JSRuntime *rt)
{
    struct list_head *el;
    JSGCObjectHeader *p;

    rt->gc_phase = JS_GC_PHASE_DECREF;
    for(;;) {
        el = rt->gc_zero_ref_count_list.next;
        if (el == &rt->gc_zero_ref_count_list)
            break;
        p = list_entry(el, JSGCObjectHeader, link);
        assert(js_rc(p)->ref_count == 0);
        free_gc_object(rt, p);
    }
    rt->gc_phase = JS_GC_PHASE_NONE;
}

/* called with the ref_count of 'v' reaches zero. */
void __JS_FreeValueRT(JSRuntime *rt, JSValue v)
{
    uint32_t tag = JS_VALUE_GET_TAG(v);

#ifdef DUMP_FREE
    {
        printf("Freeing ");
        if (tag == JS_TAG_OBJECT) {
            JS_DumpObject(rt, JS_VALUE_GET_OBJ(v));
        } else {
            JS_DumpValueShort(rt, v);
            printf("\n");
        }
    }
#endif

    switch(tag) {
    case JS_TAG_STRING:
        {
            JSString *p = JS_VALUE_GET_STRING(v);
            if (p->atom_type) {
                JS_FreeAtomStruct(rt, p);
            } else {
#ifdef DUMP_LEAKS
                list_del(&p->link);
#endif
                js_free_rt(rt, p);
            }
        }
        break;
    case JS_TAG_STRING_ROPE:
        /* Note: recursion is acceptable because the rope depth is bounded */
        {
            JSStringRope *p = JS_VALUE_GET_STRING_ROPE(v);
            JS_FreeValueRT(rt, p->left);
            JS_FreeValueRT(rt, p->right);
            js_free_rt(rt, p);
        }
        break;
    case JS_TAG_OBJECT:
    case JS_TAG_FUNCTION_BYTECODE:
    case JS_TAG_MODULE:
        {
            JSGCObjectHeader *p = JS_VALUE_GET_PTR(v);
            if (rt->gc_phase != JS_GC_PHASE_REMOVE_CYCLES) {
                list_del(&p->link);
                list_add(&p->link, &rt->gc_zero_ref_count_list);
                js_rc(p)->mark = 1; /* indicate that the object is about to be freed */
                if (rt->gc_phase == JS_GC_PHASE_NONE) {
                    free_zero_refcount(rt);
                }
            }
        }
        break;
    case JS_TAG_BIG_INT:
        {
            JSBigInt *p = JS_VALUE_GET_PTR(v);
            js_free_rt(rt, p);
        }
        break;
    case JS_TAG_SYMBOL:
        {
            JSAtomStruct *p = JS_VALUE_GET_PTR(v);
            JS_FreeAtomStruct(rt, p);
        }
        break;
    default:
        abort();
    }
}

void __JS_FreeValue(JSContext *ctx, JSValue v)
{
    __JS_FreeValueRT(ctx->rt, v);
}

/* garbage collection */

static void gc_remove_weak_objects(JSRuntime *rt)
{
    struct list_head *el;

    /* add the freed objects to rt->gc_zero_ref_count_list so that
       rt->weakref_list is not modified while we traverse it */
    rt->gc_phase = JS_GC_PHASE_DECREF;

    list_for_each(el, &rt->weakref_list) {
        JSWeakRefHeader *wh = list_entry(el, JSWeakRefHeader, link);
        switch(wh->weakref_type) {
        case JS_WEAKREF_TYPE_MAP:
            map_delete_weakrefs(rt, wh);
            break;
        case JS_WEAKREF_TYPE_WEAKREF:
            weakref_delete_weakref(rt, wh);
            break;
        case JS_WEAKREF_TYPE_FINREC:
            finrec_delete_weakref(rt, wh);
            break;
        default:
            abort();
        }
    }

    rt->gc_phase = JS_GC_PHASE_NONE;
    /* free the freed objects here. */
    free_zero_refcount(rt);
}

static void add_gc_object(JSRuntime *rt, JSGCObjectHeader *h,
                          JSGCObjectTypeEnum type)
{
    js_rc(h)->mark = 0;
    js_rc(h)->gc_obj_type = type;
    list_add_tail(&h->link, &rt->gc_obj_list);
}

static void remove_gc_object(JSGCObjectHeader *h)
{
    list_del(&h->link);
}

void JS_MarkValue(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func)
{
    if (JS_VALUE_HAS_REF_COUNT(val)) {
        switch(JS_VALUE_GET_TAG(val)) {
        case JS_TAG_OBJECT:
        case JS_TAG_FUNCTION_BYTECODE:
        case JS_TAG_MODULE:
            mark_func(rt, JS_VALUE_GET_PTR(val));
            break;
        default:
            break;
        }
    }
}

static void mark_children(JSRuntime *rt, JSGCObjectHeader *gp,
                          JS_MarkFunc *mark_func)
{
    switch(js_rc(gp)->gc_obj_type) {
    case JS_GC_OBJ_TYPE_JS_OBJECT:
        {
            JSObject *p = (JSObject *)gp;
            JSShapeProperty *prs;
            JSShape *sh;
            int i;
            sh = p->shape;
            mark_func(rt, &sh->header);
            /* mark all the fields */
            prs = get_shape_prop(sh);
            for(i = 0; i < sh->prop_count; i++) {
                JSProperty *pr = &p->prop[i];
                if (prs->atom != JS_ATOM_NULL) {
                    if (prs->flags & JS_PROP_TMASK) {
                        if ((prs->flags & JS_PROP_TMASK) == JS_PROP_GETSET) {
                            if (pr->u.getset.getter)
                                mark_func(rt, &pr->u.getset.getter->header);
                            if (pr->u.getset.setter)
                                mark_func(rt, &pr->u.getset.setter->header);
                        } else if ((prs->flags & JS_PROP_TMASK) == JS_PROP_VARREF) {
                            /* Note: the tag does not matter
                               provided it is a GC object */
                            mark_func(rt, &pr->u.var_ref->header);
                        } else if ((prs->flags & JS_PROP_TMASK) == JS_PROP_AUTOINIT) {
                            js_autoinit_mark(rt, pr, mark_func);
                        }
                    } else {
                        JS_MarkValue(rt, pr->u.value, mark_func);
                    }
                }
                prs++;
            }

            if (p->class_id != JS_CLASS_OBJECT) {
                JSClassGCMark *gc_mark;
                gc_mark = rt->class_array[p->class_id].gc_mark;
                if (gc_mark)
                    gc_mark(rt, JS_MKPTR(JS_TAG_OBJECT, p), mark_func);
            }
        }
        break;
    case JS_GC_OBJ_TYPE_FUNCTION_BYTECODE:
        /* the template objects can be part of a cycle */
        {
            JSFunctionBytecode *b = (JSFunctionBytecode *)gp;
            int i;
            for(i = 0; i < b->cpool_count; i++) {
                JS_MarkValue(rt, b->cpool[i], mark_func);
            }
            if (b->realm)
                mark_func(rt, &b->realm->header);
        }
        break;
    case JS_GC_OBJ_TYPE_VAR_REF:
        {
            JSVarRef *var_ref = (JSVarRef *)gp;
            if (var_ref->is_detached) {
                JS_MarkValue(rt, *var_ref->pvalue, mark_func);
            } else {
                JSStackFrame *sf = var_ref->stack_frame;
                if (sf->js_mode & JS_MODE_ASYNC) {
                    JSAsyncFunctionState *async_func = container_of(sf, JSAsyncFunctionState, frame);
                    mark_func(rt, &async_func->header);
                }
            }
        }
        break;
    case JS_GC_OBJ_TYPE_ASYNC_FUNCTION:
        {
            JSAsyncFunctionState *s = (JSAsyncFunctionState *)gp;
            JSStackFrame *sf = &s->frame;
            JSValue *sp;

            if (!s->is_completed) {
                JS_MarkValue(rt, sf->cur_func, mark_func);
                JS_MarkValue(rt, s->this_val, mark_func);
                /* sf->cur_sp = NULL if the function is running */
                if (sf->cur_sp) {
                    /* if the function is running, cur_sp is not known so we
                       cannot mark the stack. Marking the variables is not needed
                       because a running function cannot be part of a removable
                       cycle */
                    for(sp = sf->arg_buf; sp < sf->cur_sp; sp++)
                        JS_MarkValue(rt, *sp, mark_func);
                }
            }
            JS_MarkValue(rt, s->resolving_funcs[0], mark_func);
            JS_MarkValue(rt, s->resolving_funcs[1], mark_func);
        }
        break;
    case JS_GC_OBJ_TYPE_SHAPE:
        {
            JSShape *sh = (JSShape *)gp;
            if (sh->proto != NULL) {
                mark_func(rt, &sh->proto->header);
            }
        }
        break;
    case JS_GC_OBJ_TYPE_JS_CONTEXT:
        {
            JSContext *ctx = (JSContext *)gp;
            JS_MarkContext(rt, ctx, mark_func);
        }
        break;
    case JS_GC_OBJ_TYPE_MODULE:
        {
            JSModuleDef *m = (JSModuleDef *)gp;
            js_mark_module_def(rt, m, mark_func);
        }
        break;
    default:
        abort();
    }
}

static void gc_decref_child(JSRuntime *rt, JSGCObjectHeader *p)
{
    assert(js_rc(p)->ref_count > 0);
    js_rc(p)->ref_count--;
    if (js_rc(p)->ref_count == 0 && js_rc(p)->mark == 1) {
        list_del(&p->link);
        list_add_tail(&p->link, &rt->tmp_obj_list);
    }
}

static void gc_decref(JSRuntime *rt)
{
    struct list_head *el, *el1;
    JSGCObjectHeader *p;

    init_list_head(&rt->tmp_obj_list);

    /* decrement the refcount of all the children of all the GC
       objects and move the GC objects with zero refcount to
       tmp_obj_list */
    list_for_each_safe(el, el1, &rt->gc_obj_list) {
        p = list_entry(el, JSGCObjectHeader, link);
        assert(js_rc(p)->mark == 0);
        mark_children(rt, p, gc_decref_child);
        js_rc(p)->mark = 1;
        if (js_rc(p)->ref_count == 0) {
            list_del(&p->link);
            list_add_tail(&p->link, &rt->tmp_obj_list);
        }
    }
}

static void gc_scan_incref_child(JSRuntime *rt, JSGCObjectHeader *p)
{
    js_rc(p)->ref_count++;
    if (js_rc(p)->ref_count == 1) {
        /* ref_count was 0: remove from tmp_obj_list and add at the
           end of gc_obj_list */
        list_del(&p->link);
        list_add_tail(&p->link, &rt->gc_obj_list);
        js_rc(p)->mark = 0; /* reset the mark for the next GC call */
    }
}

static void gc_scan_incref_child2(JSRuntime *rt, JSGCObjectHeader *p)
{
    js_rc(p)->ref_count++;
}

static void gc_scan(JSRuntime *rt)
{
    struct list_head *el;
    JSGCObjectHeader *p;

    /* keep the objects with a refcount > 0 and their children. */
    list_for_each(el, &rt->gc_obj_list) {
        p = list_entry(el, JSGCObjectHeader, link);
        assert(js_rc(p)->ref_count > 0);
        js_rc(p)->mark = 0; /* reset the mark for the next GC call */
        mark_children(rt, p, gc_scan_incref_child);
    }

    /* restore the refcount of the objects to be deleted. */
    list_for_each(el, &rt->tmp_obj_list) {
        p = list_entry(el, JSGCObjectHeader, link);
        mark_children(rt, p, gc_scan_incref_child2);
    }
}

static void gc_free_cycles(JSRuntime *rt)
{
    struct list_head *el, *el1;
    JSGCObjectHeader *p;
#ifdef DUMP_GC_FREE
    BOOL header_done = FALSE;
#endif

    rt->gc_phase = JS_GC_PHASE_REMOVE_CYCLES;

    for(;;) {
        el = rt->tmp_obj_list.next;
        if (el == &rt->tmp_obj_list)
            break;
        p = list_entry(el, JSGCObjectHeader, link);
        /* Only need to free the GC object associated with JS values
           or async functions. The rest will be automatically removed
           because they must be referenced by them. */
        switch(js_rc(p)->gc_obj_type) {
        case JS_GC_OBJ_TYPE_JS_OBJECT:
        case JS_GC_OBJ_TYPE_FUNCTION_BYTECODE:
        case JS_GC_OBJ_TYPE_ASYNC_FUNCTION:
        case JS_GC_OBJ_TYPE_MODULE:
#ifdef DUMP_GC_FREE
            if (!header_done) {
                printf("Freeing cycles:\n");
                JS_DumpObjectHeader(rt);
                header_done = TRUE;
            }
            JS_DumpGCObject(rt, p);
#endif
            free_gc_object(rt, p);
            break;
        default:
            list_del(&p->link);
            list_add_tail(&p->link, &rt->gc_zero_ref_count_list);
            break;
        }
    }
    rt->gc_phase = JS_GC_PHASE_NONE;

    list_for_each_safe(el, el1, &rt->gc_zero_ref_count_list) {
        p = list_entry(el, JSGCObjectHeader, link);
        assert(js_rc(p)->gc_obj_type == JS_GC_OBJ_TYPE_JS_OBJECT ||
               js_rc(p)->gc_obj_type == JS_GC_OBJ_TYPE_FUNCTION_BYTECODE ||
               js_rc(p)->gc_obj_type == JS_GC_OBJ_TYPE_ASYNC_FUNCTION ||
               js_rc(p)->gc_obj_type == JS_GC_OBJ_TYPE_MODULE);
        if (js_rc(p)->gc_obj_type == JS_GC_OBJ_TYPE_JS_OBJECT &&
            ((JSObject *)p)->weakref_count != 0) {
            /* keep the object because there are weak references to it */
            js_rc(p)->mark = 0;
        } else {
            js_free_rt(rt, p);
        }
    }

    init_list_head(&rt->gc_zero_ref_count_list);
}

static void JS_RunGCInternal(JSRuntime *rt, BOOL remove_weak_objects)
{
    if (remove_weak_objects) {
        /* free the weakly referenced object or symbol structures, delete
           the associated Map/Set entries and queue the finalization
           registry callbacks. */
        gc_remove_weak_objects(rt);
    }

    /* decrement the reference of the children of each object. mark =
       1 after this pass. */
    gc_decref(rt);

    /* keep the GC objects with a non zero refcount and their childs */
    gc_scan(rt);

    /* free the GC objects in a cycle */
    gc_free_cycles(rt);
}

void JS_RunGC(JSRuntime *rt)
{
    JS_RunGCInternal(rt, TRUE);
}

/* Return false if not an object or if the object has already been
   freed (zombie objects are visible in finalizers when freeing
   cycles). */
BOOL JS_IsLiveObject(JSRuntime *rt, JSValueConst obj)
{
    JSObject *p;
    if (!JS_IsObject(obj))
        return FALSE;
    p = JS_VALUE_GET_OBJ(obj);
    return !p->free_mark;
}

/* Compute memory used by various object types */
/* XXX: poor man's approach to handling multiply referenced objects */
typedef struct JSMemoryUsage_helper {
    double memory_used_count;
    double str_count;
    double str_size;
    int64_t js_func_count;
    double js_func_size;
    int64_t js_func_code_size;
    int64_t js_func_pc2line_count;
    int64_t js_func_pc2line_size;
} JSMemoryUsage_helper;

static void compute_value_size(JSValueConst val, JSMemoryUsage_helper *hp);

static void compute_jsstring_size(JSString *str, JSMemoryUsage_helper *hp)
{
    if (!str->atom_type) {  /* atoms are handled separately */
        double s_ref_count = js_rc(str)->ref_count;
        hp->str_count += 1 / s_ref_count;
        hp->str_size += ((sizeof(*str) + (str->len << str->is_wide_char) +
                          1 - str->is_wide_char) / s_ref_count);
    }
}

static void compute_bytecode_size(JSFunctionBytecode *b, JSMemoryUsage_helper *hp)
{
    int memory_used_count, js_func_size, i;

    memory_used_count = 0;
    js_func_size = offsetof(JSFunctionBytecode, debug);
    if (b->vardefs) {
        js_func_size += (b->arg_count + b->var_count) * sizeof(*b->vardefs);
    }
    if (b->cpool) {
        js_func_size += b->cpool_count * sizeof(*b->cpool);
        for (i = 0; i < b->cpool_count; i++) {
            JSValueConst val = b->cpool[i];
            compute_value_size(val, hp);
        }
    }
    if (b->closure_var) {
        js_func_size += b->closure_var_count * sizeof(*b->closure_var);
    }
    if (!b->read_only_bytecode && b->byte_code_buf) {
        hp->js_func_code_size += b->byte_code_len;
    }
    if (b->has_debug) {
        js_func_size += sizeof(*b) - offsetof(JSFunctionBytecode, debug);
        if (b->debug.source) {
            memory_used_count++;
            js_func_size += b->debug.source_len + 1;
        }
        if (b->debug.pc2line_len) {
            memory_used_count++;
            hp->js_func_pc2line_count += 1;
            hp->js_func_pc2line_size += b->debug.pc2line_len;
        }
    }
    hp->js_func_size += js_func_size;
    hp->js_func_count += 1;
    hp->memory_used_count += memory_used_count;
}

static void compute_value_size(JSValueConst val, JSMemoryUsage_helper *hp)
{
    switch(JS_VALUE_GET_TAG(val)) {
    case JS_TAG_STRING:
        compute_jsstring_size(JS_VALUE_GET_STRING(val), hp);
        break;
    case JS_TAG_BIG_INT:
        /* should track JSBigInt usage */
        break;
    }
}

void JS_ComputeMemoryUsage(JSRuntime *rt, JSMemoryUsage *s)
{
    struct list_head *el, *el1;
    int i;
    JSMemoryUsage_helper mem = { 0 }, *hp = &mem;

    memset(s, 0, sizeof(*s));
    s->malloc_count = rt->malloc_ctx.malloc_state.malloc_count;
    s->malloc_size = rt->malloc_ctx.malloc_state.malloc_size;
    s->malloc_limit = rt->malloc_ctx.malloc_state.malloc_limit;

    s->memory_used_count = 2; /* rt + rt->class_array */
    s->memory_used_size = sizeof(JSRuntime) + sizeof(JSValue) * rt->class_count;

    list_for_each(el, &rt->context_list) {
        JSContext *ctx = list_entry(el, JSContext, link);
        JSShape *sh = ctx->array_shape;
        s->memory_used_count += 2; /* ctx + ctx->class_proto */
        s->memory_used_size += sizeof(JSContext) +
            sizeof(JSValue) * rt->class_count;
        s->binary_object_count += ctx->binary_object_count;
        s->binary_object_size += ctx->binary_object_size;

        /* the hashed shapes are counted separately */
        if (sh && !sh->is_hashed) {
            int hash_size = sh->prop_hash_mask + 1;
            s->shape_count++;
            s->shape_size += get_shape_size(hash_size, sh->prop_size);
        }
        list_for_each(el1, &ctx->loaded_modules) {
            JSModuleDef *m = list_entry(el1, JSModuleDef, link);
            s->memory_used_count += 1;
            s->memory_used_size += sizeof(*m);
            if (m->req_module_entries) {
                s->memory_used_count += 1;
                s->memory_used_size += m->req_module_entries_count * sizeof(*m->req_module_entries);
            }
            if (m->export_entries) {
                s->memory_used_count += 1;
                s->memory_used_size += m->export_entries_count * sizeof(*m->export_entries);
                for (i = 0; i < m->export_entries_count; i++) {
                    JSExportEntry *me = &m->export_entries[i];
                    if (me->export_type == JS_EXPORT_TYPE_LOCAL && me->u.local.var_ref) {
                        /* potential multiple count */
                        s->memory_used_count += 1;
                        compute_value_size(me->u.local.var_ref->value, hp);
                    }
                }
            }
            if (m->star_export_entries) {
                s->memory_used_count += 1;
                s->memory_used_size += m->star_export_entries_count * sizeof(*m->star_export_entries);
            }
            if (m->import_entries) {
                s->memory_used_count += 1;
                s->memory_used_size += m->import_entries_count * sizeof(*m->import_entries);
            }
            compute_value_size(m->module_ns, hp);
            compute_value_size(m->func_obj, hp);
        }
    }

    list_for_each(el, &rt->gc_obj_list) {
        JSGCObjectHeader *gp = list_entry(el, JSGCObjectHeader, link);
        JSObject *p;
        JSShape *sh;
        JSShapeProperty *prs;

        /* XXX: could count the other GC object types too */
        if (js_rc(gp)->gc_obj_type == JS_GC_OBJ_TYPE_FUNCTION_BYTECODE) {
            compute_bytecode_size((JSFunctionBytecode *)gp, hp);
            continue;
        } else if (js_rc(gp)->gc_obj_type != JS_GC_OBJ_TYPE_JS_OBJECT) {
            continue;
        }
        p = (JSObject *)gp;
        sh = p->shape;
        s->obj_count++;
        if (p->prop) {
            s->memory_used_count++;
            s->prop_size += sh->prop_size * sizeof(*p->prop);
            s->prop_count += sh->prop_count;
            prs = get_shape_prop(sh);
            for(i = 0; i < sh->prop_count; i++) {
                JSProperty *pr = &p->prop[i];
                if (prs->atom != JS_ATOM_NULL && !(prs->flags & JS_PROP_TMASK)) {
                    compute_value_size(pr->u.value, hp);
                }
                prs++;
            }
        }
        /* the hashed shapes are counted separately */
        if (!sh->is_hashed) {
            int hash_size = sh->prop_hash_mask + 1;
            s->shape_count++;
            s->shape_size += get_shape_size(hash_size, sh->prop_size);
        }

        switch(p->class_id) {
        case JS_CLASS_ARRAY:             /* u.array | length */
        case JS_CLASS_ARGUMENTS:         /* u.array | length */
            s->array_count++;
            if (p->fast_array) {
                s->fast_array_count++;
                if (p->u.array.u.values) {
                    s->memory_used_count++;
                    s->memory_used_size += p->u.array.count *
                        sizeof(*p->u.array.u.values);
                    s->fast_array_elements += p->u.array.count;
                    for (i = 0; i < p->u.array.count; i++) {
                        compute_value_size(p->u.array.u.values[i], hp);
                    }
                }
            }
            break;
        case JS_CLASS_MAPPED_ARGUMENTS:         /* u.array | length */
            if (p->fast_array) {
                s->fast_array_count++;
                if (p->u.array.u.values) {
                    s->memory_used_count++;
                    s->memory_used_size += p->u.array.count *
                        sizeof(*p->u.array.u.var_refs);
                    s->fast_array_elements += p->u.array.count;
                    for (i = 0; i < p->u.array.count; i++) {
                        compute_value_size(*p->u.array.u.var_refs[i]->pvalue, hp);
                    }
                }
            }
            break;
        case JS_CLASS_NUMBER:            /* u.object_data */
        case JS_CLASS_STRING:            /* u.object_data */
        case JS_CLASS_BOOLEAN:           /* u.object_data */
        case JS_CLASS_SYMBOL:            /* u.object_data */
        case JS_CLASS_DATE:              /* u.object_data */
        case JS_CLASS_BIG_INT:           /* u.object_data */
            compute_value_size(p->u.object_data, hp);
            break;
        case JS_CLASS_C_FUNCTION:        /* u.cfunc */
            s->c_func_count++;
            break;
        case JS_CLASS_BYTECODE_FUNCTION: /* u.func */
            {
                JSFunctionBytecode *b = p->u.func.function_bytecode;
                JSVarRef **var_refs = p->u.func.var_refs;
                /* home_object: object will be accounted for in list scan */
                if (var_refs) {
                    s->memory_used_count++;
                    s->js_func_size += b->closure_var_count * sizeof(*var_refs);
                    for (i = 0; i < b->closure_var_count; i++) {
                        if (var_refs[i]) {
                            double ref_count = js_rc(var_refs[i])->ref_count;
                            s->memory_used_count += 1 / ref_count;
                            s->js_func_size += sizeof(*var_refs[i]) / ref_count;
                            /* handle non object closed values */
                            if (var_refs[i]->pvalue == &var_refs[i]->value) {
                                /* potential multiple count */
                                compute_value_size(var_refs[i]->value, hp);
                            }
                        }
                    }
                }
            }
            break;
        case JS_CLASS_BOUND_FUNCTION:    /* u.bound_function */
            {
                JSBoundFunction *bf = p->u.bound_function;
                /* func_obj and this_val are objects */
                for (i = 0; i < bf->argc; i++) {
                    compute_value_size(bf->argv[i], hp);
                }
                s->memory_used_count += 1;
                s->memory_used_size += sizeof(*bf) + bf->argc * sizeof(*bf->argv);
            }
            break;
        case JS_CLASS_C_FUNCTION_DATA:   /* u.c_function_data_record */
            {
                JSCFunctionDataRecord *fd = p->u.c_function_data_record;
                if (fd) {
                    for (i = 0; i < fd->data_len; i++) {
                        compute_value_size(fd->data[i], hp);
                    }
                    s->memory_used_count += 1;
                    s->memory_used_size += sizeof(*fd) + fd->data_len * sizeof(*fd->data);
                }
            }
            break;
        case JS_CLASS_REGEXP:            /* u.regexp */
            compute_jsstring_size(p->u.regexp.pattern, hp);
            compute_jsstring_size(p->u.regexp.bytecode, hp);
            break;

        case JS_CLASS_FOR_IN_ITERATOR:   /* u.for_in_iterator */
            {
                JSForInIterator *it = p->u.for_in_iterator;
                if (it) {
                    compute_value_size(it->obj, hp);
                    s->memory_used_count += 1;
                    s->memory_used_size += sizeof(*it);
                }
            }
            break;
        case JS_CLASS_ARRAY_BUFFER:      /* u.array_buffer */
        case JS_CLASS_SHARED_ARRAY_BUFFER: /* u.array_buffer */
            {
                JSArrayBuffer *abuf = p->u.array_buffer;
                if (abuf) {
                    s->memory_used_count += 1;
                    s->memory_used_size += sizeof(*abuf);
                    if (abuf->data) {
                        s->memory_used_count += 1;
                        s->memory_used_size += abuf->byte_length;
                    }
                }
            }
            break;
        case JS_CLASS_GENERATOR:         /* u.generator_data */
        case JS_CLASS_UINT8C_ARRAY:      /* u.typed_array / u.array */
        case JS_CLASS_INT8_ARRAY:        /* u.typed_array / u.array */
        case JS_CLASS_UINT8_ARRAY:       /* u.typed_array / u.array */
        case JS_CLASS_INT16_ARRAY:       /* u.typed_array / u.array */
        case JS_CLASS_UINT16_ARRAY:      /* u.typed_array / u.array */
        case JS_CLASS_INT32_ARRAY:       /* u.typed_array / u.array */
        case JS_CLASS_UINT32_ARRAY:      /* u.typed_array / u.array */
        case JS_CLASS_BIG_INT64_ARRAY:   /* u.typed_array / u.array */
        case JS_CLASS_BIG_UINT64_ARRAY:  /* u.typed_array / u.array */
        case JS_CLASS_FLOAT16_ARRAY:     /* u.typed_array / u.array */
        case JS_CLASS_FLOAT32_ARRAY:     /* u.typed_array / u.array */
        case JS_CLASS_FLOAT64_ARRAY:     /* u.typed_array / u.array */
        case JS_CLASS_DATAVIEW:          /* u.typed_array */
        case JS_CLASS_MAP:               /* u.map_state */
        case JS_CLASS_SET:               /* u.map_state */
        case JS_CLASS_WEAKMAP:           /* u.map_state */
        case JS_CLASS_WEAKSET:           /* u.map_state */
        case JS_CLASS_MAP_ITERATOR:      /* u.map_iterator_data */
        case JS_CLASS_SET_ITERATOR:      /* u.map_iterator_data */
        case JS_CLASS_ARRAY_ITERATOR:    /* u.array_iterator_data */
        case JS_CLASS_STRING_ITERATOR:   /* u.array_iterator_data */
        case JS_CLASS_PROXY:             /* u.proxy_data */
        case JS_CLASS_PROMISE:           /* u.promise_data */
        case JS_CLASS_PROMISE_RESOLVE_FUNCTION:  /* u.promise_function_data */
        case JS_CLASS_PROMISE_REJECT_FUNCTION:   /* u.promise_function_data */
        case JS_CLASS_ASYNC_FUNCTION_RESOLVE:    /* u.async_function_data */
        case JS_CLASS_ASYNC_FUNCTION_REJECT:     /* u.async_function_data */
        case JS_CLASS_ASYNC_FROM_SYNC_ITERATOR:  /* u.async_from_sync_iterator_data */
        case JS_CLASS_ASYNC_GENERATOR:   /* u.async_generator_data */
            /* TODO */
        default:
            /* XXX: class definition should have an opaque block size */
            if (p->u.opaque) {
                s->memory_used_count += 1;
            }
            break;
        }
    }
    s->obj_size += s->obj_count * sizeof(JSObject);

    /* hashed shapes */
    s->memory_used_count++; /* rt->shape_hash */
    s->memory_used_size += sizeof(rt->shape_hash[0]) * rt->shape_hash_size;
    for(i = 0; i < rt->shape_hash_size; i++) {
        JSShape *sh;
        for(sh = rt->shape_hash[i]; sh != NULL; sh = sh->shape_hash_next) {
            int hash_size = sh->prop_hash_mask + 1;
            s->shape_count++;
            s->shape_size += get_shape_size(hash_size, sh->prop_size);
        }
    }

    /* atoms */
    s->memory_used_count += 2; /* rt->atom_array, rt->atom_hash */
    s->atom_count = rt->atom_count;
    s->atom_size = sizeof(rt->atom_array[0]) * rt->atom_size +
        sizeof(rt->atom_hash[0]) * rt->atom_hash_size;
    for(i = 0; i < rt->atom_size; i++) {
        JSAtomStruct *p = rt->atom_array[i];
        if (!atom_is_free(p)) {
            s->atom_size += (sizeof(*p) + (p->len << p->is_wide_char) +
                             1 - p->is_wide_char);
        }
    }
    s->str_count = round(mem.str_count);
    s->str_size = round(mem.str_size);
    s->js_func_count = mem.js_func_count;
    s->js_func_size = round(mem.js_func_size);
    s->js_func_code_size = mem.js_func_code_size;
    s->js_func_pc2line_count = mem.js_func_pc2line_count;
    s->js_func_pc2line_size = mem.js_func_pc2line_size;
    s->memory_used_count += round(mem.memory_used_count) +
        s->atom_count + s->str_count +
        s->obj_count + s->shape_count +
        s->js_func_count + s->js_func_pc2line_count;
    s->memory_used_size += s->atom_size + s->str_size +
        s->obj_size + s->prop_size + s->shape_size +
        s->js_func_size + s->js_func_code_size + s->js_func_pc2line_size;
}

void JS_DumpMemoryUsage(FILE *fp, const JSMemoryUsage *s, JSRuntime *rt)
{
    fprintf(fp, "QuickJS memory usage -- " CONFIG_VERSION " version, %d-bit, malloc limit: %"PRId64"\n\n",
            (int)sizeof(void *) * 8, s->malloc_limit);
#if 1
    if (rt) {
        static const struct {
            const char *name;
            size_t size;
        } object_types[] = {
            { "JSRuntime", sizeof(JSRuntime) },
            { "JSContext", sizeof(JSContext) },
            { "JSObject", sizeof(JSObject) },
            { "JSString", sizeof(JSString) },
            { "JSFunctionBytecode", sizeof(JSFunctionBytecode) },
        };
        int i, usage_size_ok = 0;
        for(i = 0; i < countof(object_types); i++) {
            unsigned int size = object_types[i].size;
            void *p = js_malloc_rt(rt, size);
            if (p) {
                unsigned int size1 = js_malloc_usable_size_rt(rt, p);
                if (size1 >= size) {
                    usage_size_ok = 1;
                    fprintf(fp, "  %3u + %-2u  %s\n",
                            size, size1 - size, object_types[i].name);
                }
                js_free_rt(rt, p);
            }
        }
        if (!usage_size_ok) {
            fprintf(fp, "  malloc_usable_size unavailable\n");
        }
        {
            int obj_classes[JS_CLASS_INIT_COUNT + 1] = { 0 };
            int class_id;
            struct list_head *el;
            list_for_each(el, &rt->gc_obj_list) {
                JSGCObjectHeader *gp = list_entry(el, JSGCObjectHeader, link);
                JSObject *p;
                if (js_rc(gp)->gc_obj_type == JS_GC_OBJ_TYPE_JS_OBJECT) {
                    p = (JSObject *)gp;
                    obj_classes[min_uint32(p->class_id, JS_CLASS_INIT_COUNT)]++;
                }
            }
            fprintf(fp, "\n" "JSObject classes\n");
            if (obj_classes[0])
                fprintf(fp, "  %5d  %2.0d %s\n", obj_classes[0], 0, "none");
            for (class_id = 1; class_id < JS_CLASS_INIT_COUNT; class_id++) {
                if (obj_classes[class_id] && class_id < rt->class_count) {
                    char buf[ATOM_GET_STR_BUF_SIZE];
                    fprintf(fp, "  %5d  %2.0d %s\n", obj_classes[class_id], class_id,
                            JS_AtomGetStrRT(rt, buf, sizeof(buf), rt->class_array[class_id].class_name));
                }
            }
            if (obj_classes[JS_CLASS_INIT_COUNT])
                fprintf(fp, "  %5d  %2.0d %s\n", obj_classes[JS_CLASS_INIT_COUNT], 0, "other");
        }
        fprintf(fp, "\n");
    }
#endif
    fprintf(fp, "%-20s %8s %8s\n", "NAME", "COUNT", "SIZE");

    if (s->malloc_count) {
        fprintf(fp, "%-20s %8"PRId64" %8"PRId64"  (%0.1f per block)\n",
                "memory allocated", s->malloc_count, s->malloc_size,
                (double)s->malloc_size / s->malloc_count);
        fprintf(fp, "%-20s %8"PRId64" %8"PRId64"  (%d overhead, %0.1f average slack)\n",
                "memory used", s->memory_used_count, s->memory_used_size,
                MALLOC_OVERHEAD, ((double)(s->malloc_size - s->memory_used_size) /
                                  s->memory_used_count));
    }
    if (s->atom_count) {
        fprintf(fp, "%-20s %8"PRId64" %8"PRId64"  (%0.1f per atom)\n",
                "atoms", s->atom_count, s->atom_size,
                (double)s->atom_size / s->atom_count);
    }
    if (s->str_count) {
        fprintf(fp, "%-20s %8"PRId64" %8"PRId64"  (%0.1f per string)\n",
                "strings", s->str_count, s->str_size,
                (double)s->str_size / s->str_count);
    }
    if (s->obj_count) {
        fprintf(fp, "%-20s %8"PRId64" %8"PRId64"  (%0.1f per object)\n",
                "objects", s->obj_count, s->obj_size,
                (double)s->obj_size / s->obj_count);
        fprintf(fp, "%-20s %8"PRId64" %8"PRId64"  (%0.1f per object)\n",
                "  properties", s->prop_count, s->prop_size,
                (double)s->prop_count / s->obj_count);
        fprintf(fp, "%-20s %8"PRId64" %8"PRId64"  (%0.1f per shape)\n",
                "  shapes", s->shape_count, s->shape_size,
                (double)s->shape_size / s->shape_count);
    }
    if (s->js_func_count) {
        fprintf(fp, "%-20s %8"PRId64" %8"PRId64"\n",
                "bytecode functions", s->js_func_count, s->js_func_size);
        fprintf(fp, "%-20s %8"PRId64" %8"PRId64"  (%0.1f per function)\n",
                "  bytecode", s->js_func_count, s->js_func_code_size,
                (double)s->js_func_code_size / s->js_func_count);
        if (s->js_func_pc2line_count) {
            fprintf(fp, "%-20s %8"PRId64" %8"PRId64"  (%0.1f per function)\n",
                    "  pc2line", s->js_func_pc2line_count,
                    s->js_func_pc2line_size,
                    (double)s->js_func_pc2line_size / s->js_func_pc2line_count);
        }
    }
    if (s->c_func_count) {
        fprintf(fp, "%-20s %8"PRId64"\n", "C functions", s->c_func_count);
    }
    if (s->array_count) {
        fprintf(fp, "%-20s %8"PRId64"\n", "arrays", s->array_count);
        if (s->fast_array_count) {
            fprintf(fp, "%-20s %8"PRId64"\n", "  fast arrays", s->fast_array_count);
            fprintf(fp, "%-20s %8"PRId64" %8"PRId64"  (%0.1f per fast array)\n",
                    "  elements", s->fast_array_elements,
                    s->fast_array_elements * (int)sizeof(JSValue),
                    (double)s->fast_array_elements / s->fast_array_count);
        }
    }
    if (s->binary_object_count) {
        fprintf(fp, "%-20s %8"PRId64" %8"PRId64"\n",
                "binary objects", s->binary_object_count, s->binary_object_size);
    }
}

JSValue JS_GetGlobalObject(JSContext *ctx)
{
    return JS_DupValue(ctx, ctx->global_obj);
}

/* WARNING: obj is freed */
JSValue JS_Throw(JSContext *ctx, JSValue obj)
{
    JSRuntime *rt = ctx->rt;
    JS_FreeValue(ctx, rt->current_exception);
    rt->current_exception = obj;
    rt->current_exception_is_uncatchable = FALSE;
    return JS_EXCEPTION;
}

/* return the pending exception (cannot be called twice). */
JSValue JS_GetException(JSContext *ctx)
{
    JSValue val;
    JSRuntime *rt = ctx->rt;
    val = rt->current_exception;
    rt->current_exception = JS_UNINITIALIZED;
    return val;
}

JS_BOOL JS_HasException(JSContext *ctx)
{
    return !JS_IsUninitialized(ctx->rt->current_exception);
}

static void dbuf_put_leb128(DynBuf *s, uint32_t v)
{
    uint32_t a;
    for(;;) {
        a = v & 0x7f;
        v >>= 7;
        if (v != 0) {
            dbuf_putc(s, a | 0x80);
        } else {
            dbuf_putc(s, a);
            break;
        }
    }
}

static void dbuf_put_sleb128(DynBuf *s, int32_t v1)
{
    uint32_t v = v1;
    dbuf_put_leb128(s, (2 * v) ^ -(v >> 31));
}

static int get_leb128(uint32_t *pval, const uint8_t *buf,
                      const uint8_t *buf_end)
{
    const uint8_t *ptr = buf;
    uint32_t v, a, i;
    v = 0;
    for(i = 0; i < 5; i++) {
        if (unlikely(ptr >= buf_end))
            break;
        a = *ptr++;
        v |= (a & 0x7f) << (i * 7);
        if (!(a & 0x80)) {
            *pval = v;
            return ptr - buf;
        }
    }
    *pval = 0;
    return -1;
}

static int get_sleb128(int32_t *pval, const uint8_t *buf,
                       const uint8_t *buf_end)
{
    int ret;
    uint32_t val;
    ret = get_leb128(&val, buf, buf_end);
    if (ret < 0) {
        *pval = 0;
        return -1;
    }
    *pval = (val >> 1) ^ -(val & 1);
    return ret;
}

/* use pc_value = -1 to get the position of the function definition */
static int find_line_num(JSContext *ctx, JSFunctionBytecode *b,
                         uint32_t pc_value, int *pcol_num)
{
    const uint8_t *p_end, *p;
    int new_line_num, line_num, pc, v, ret, new_col_num, col_num;
    uint32_t val;
    unsigned int op;

    if (!b->has_debug || !b->debug.pc2line_buf)
        goto fail; /* function was stripped */

    p = b->debug.pc2line_buf;
    p_end = p + b->debug.pc2line_len;

    /* get the function line and column numbers */
    ret = get_leb128(&val, p, p_end);
    if (ret < 0)
        goto fail;
    p += ret;
    line_num = val + 1;

    ret = get_leb128(&val, p, p_end);
    if (ret < 0)
        goto fail;
    p += ret;
    col_num = val + 1;

    if (pc_value != -1) {
        pc = 0;
        while (p < p_end) {
            op = *p++;
            if (op == 0) {
                ret = get_leb128(&val, p, p_end);
                if (ret < 0)
                    goto fail;
                pc += val;
                p += ret;
                ret = get_sleb128(&v, p, p_end);
                if (ret < 0)
                    goto fail;
                p += ret;
                new_line_num = line_num + v;
            } else {
                op -= PC2LINE_OP_FIRST;
                pc += (op / PC2LINE_RANGE);
                new_line_num = line_num + (op % PC2LINE_RANGE) + PC2LINE_BASE;
            }
            ret = get_sleb128(&v, p, p_end);
            if (ret < 0)
                goto fail;
            p += ret;
            new_col_num = col_num + v;

            if (pc_value < pc)
                goto done;
            line_num = new_line_num;
            col_num = new_col_num;
        }
    }
 done:
    *pcol_num = col_num;
    return line_num;
 fail:
    *pcol_num = 0;
    return 0;
}

/* return a string property without executing arbitrary JS code (used
   when dumping the stack trace or in debug print). */
static const char *get_prop_string(JSContext *ctx, JSValueConst obj, JSAtom prop)
{
    JSObject *p;
    JSProperty *pr;
    JSShapeProperty *prs;
    JSValueConst val;

    if (JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT)
        return NULL;
    p = JS_VALUE_GET_OBJ(obj);
    prs = find_own_property(&pr, p, prop);
    if (!prs) {
        /* we look at one level in the prototype to handle the 'name'
           field of the Error objects */
        p = p->shape->proto;
        if (!p)
            return NULL;
        prs = find_own_property(&pr, p, prop);
        if (!prs)
            return NULL;
    }

    if ((prs->flags & JS_PROP_TMASK) != JS_PROP_NORMAL)
        return NULL;
    val = pr->u.value;
    if (JS_VALUE_GET_TAG(val) != JS_TAG_STRING)
        return NULL;
    return JS_ToCString(ctx, val);
}

#define JS_BACKTRACE_FLAG_SKIP_FIRST_LEVEL (1 << 0)

/* if filename != NULL, an additional level is added with the filename
   and line number information (used for parse error). */
static void build_backtrace(JSContext *ctx, JSValueConst error_obj,
                            const char *filename, int line_num, int col_num,
                            int backtrace_flags)
{
    JSStackFrame *sf;
    JSValue str;
    DynBuf dbuf;
    const char *func_name_str;
    const char *str1;
    JSObject *p;

    if (!JS_IsObject(error_obj))
        return; /* protection in the out of memory case */

    js_dbuf_init(ctx, &dbuf);
    if (filename) {
        dbuf_printf(&dbuf, "    at %s", filename);
        if (line_num != -1)
            dbuf_printf(&dbuf, ":%d:%d", line_num, col_num);
        dbuf_putc(&dbuf, '\n');
        str = JS_NewString(ctx, filename);
        if (JS_IsException(str))
            return;
        /* Note: SpiderMonkey does that, could update once there is a standard */
        if (JS_DefinePropertyValue(ctx, error_obj, JS_ATOM_fileName, str,
                                   JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE) < 0 ||
            JS_DefinePropertyValue(ctx, error_obj, JS_ATOM_lineNumber, JS_NewInt32(ctx, line_num),
                                   JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE) < 0 ||
            JS_DefinePropertyValue(ctx, error_obj, JS_ATOM_columnNumber, JS_NewInt32(ctx, col_num),
                                   JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE) < 0) {
            return;
        }
    }
    for(sf = ctx->rt->current_stack_frame; sf != NULL; sf = sf->prev_frame) {
        if (sf->js_mode & JS_MODE_BACKTRACE_BARRIER)
            break;
        if (backtrace_flags & JS_BACKTRACE_FLAG_SKIP_FIRST_LEVEL) {
            backtrace_flags &= ~JS_BACKTRACE_FLAG_SKIP_FIRST_LEVEL;
            continue;
        }
        func_name_str = get_prop_string(ctx, sf->cur_func, JS_ATOM_name);
        if (!func_name_str || func_name_str[0] == '\0')
            str1 = "<anonymous>";
        else
            str1 = func_name_str;
        dbuf_printf(&dbuf, "    at %s", str1);
        JS_FreeCString(ctx, func_name_str);

        p = JS_VALUE_GET_OBJ(sf->cur_func);
        if (js_class_has_bytecode(p->class_id)) {
            JSFunctionBytecode *b;
            const char *atom_str;
            int line_num1, col_num1;

            b = p->u.func.function_bytecode;
            if (b->has_debug) {
                line_num1 = find_line_num(ctx, b,
                                          sf->cur_pc - b->byte_code_buf - 1, &col_num1);
                atom_str = JS_AtomToCString(ctx, b->debug.filename);
                dbuf_printf(&dbuf, " (%s",
                            atom_str ? atom_str : "<null>");
                JS_FreeCString(ctx, atom_str);
                if (line_num1 != 0)
                    dbuf_printf(&dbuf, ":%d:%d", line_num1, col_num1);
                dbuf_putc(&dbuf, ')');
            }
        } else {
            dbuf_printf(&dbuf, " (native)");
        }
        dbuf_putc(&dbuf, '\n');
    }
    dbuf_putc(&dbuf, '\0');
    if (dbuf_error(&dbuf))
        str = JS_NULL;
    else
        str = JS_NewString(ctx, (char *)dbuf.buf);
    dbuf_free(&dbuf);
    JS_DefinePropertyValue(ctx, error_obj, JS_ATOM_stack, str,
                           JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
}

/* Note: it is important that no exception is returned by this function */
static BOOL is_backtrace_needed(JSContext *ctx, JSValueConst obj)
{
    JSObject *p;
    if (JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT)
        return FALSE;
    p = JS_VALUE_GET_OBJ(obj);
    if (p->class_id != JS_CLASS_ERROR)
        return FALSE;
    if (find_own_property1(p, JS_ATOM_stack))
        return FALSE;
    return TRUE;
}

JSValue JS_NewError(JSContext *ctx)
{
    return JS_NewObjectClass(ctx, JS_CLASS_ERROR);
}

static JSValue JS_ThrowError2(JSContext *ctx, JSErrorEnum error_num,
                              const char *fmt, va_list ap, BOOL add_backtrace)
{
    char buf[256];
    JSValue obj, ret;

    vsnprintf(buf, sizeof(buf), fmt, ap);
    obj = JS_NewObjectProtoClass(ctx, ctx->native_error_proto[error_num],
                                 JS_CLASS_ERROR);
    if (unlikely(JS_IsException(obj))) {
        /* out of memory: throw JS_NULL to avoid recursing */
        obj = JS_NULL;
    } else {
        JS_DefinePropertyValue(ctx, obj, JS_ATOM_message,
                               JS_NewString(ctx, buf),
                               JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
        if (add_backtrace) {
            build_backtrace(ctx, obj, NULL, 0, 0, 0);
        }
    }
    ret = JS_Throw(ctx, obj);
    return ret;
}

static JSValue JS_ThrowError(JSContext *ctx, JSErrorEnum error_num,
                             const char *fmt, va_list ap)
{
    JSRuntime *rt = ctx->rt;
    JSStackFrame *sf;
    BOOL add_backtrace;

    /* the backtrace is added later if called from a bytecode function */
    sf = rt->current_stack_frame;
    add_backtrace = !rt->in_out_of_memory &&
        (!sf || (JS_GetFunctionBytecode(sf->cur_func) == NULL));
    return JS_ThrowError2(ctx, error_num, fmt, ap, add_backtrace);
}

JSValue __attribute__((format(printf, 2, 3))) JS_ThrowSyntaxError(JSContext *ctx, const char *fmt, ...)
{
    JSValue val;
    va_list ap;

    va_start(ap, fmt);
    val = JS_ThrowError(ctx, JS_SYNTAX_ERROR, fmt, ap);
    va_end(ap);
    return val;
}

JSValue __attribute__((format(printf, 2, 3))) JS_ThrowTypeError(JSContext *ctx, const char *fmt, ...)
{
    JSValue val;
    va_list ap;

    va_start(ap, fmt);
    val = JS_ThrowError(ctx, JS_TYPE_ERROR, fmt, ap);
    va_end(ap);
    return val;
}

static int __attribute__((format(printf, 3, 4))) JS_ThrowTypeErrorOrFalse(JSContext *ctx, int flags, const char *fmt, ...)
{
    va_list ap;

    if ((flags & JS_PROP_THROW) ||
        ((flags & JS_PROP_THROW_STRICT) && is_strict_mode(ctx))) {
        va_start(ap, fmt);
        JS_ThrowError(ctx, JS_TYPE_ERROR, fmt, ap);
        va_end(ap);
        return -1;
    } else {
        return FALSE;
    }
}

/* never use it directly */
static JSValue __attribute__((format(printf, 3, 4))) __JS_ThrowTypeErrorAtom(JSContext *ctx, JSAtom atom, const char *fmt, ...)
{
    char buf[ATOM_GET_STR_BUF_SIZE];
    return JS_ThrowTypeError(ctx, fmt,
                             JS_AtomGetStr(ctx, buf, sizeof(buf), atom));
}

/* never use it directly */
static JSValue __attribute__((format(printf, 3, 4))) __JS_ThrowSyntaxErrorAtom(JSContext *ctx, JSAtom atom, const char *fmt, ...)
{
    char buf[ATOM_GET_STR_BUF_SIZE];
    return JS_ThrowSyntaxError(ctx, fmt,
                             JS_AtomGetStr(ctx, buf, sizeof(buf), atom));
}

/* %s is replaced by 'atom'. The macro is used so that gcc can check
    the format string. */
#define JS_ThrowTypeErrorAtom(ctx, fmt, atom) __JS_ThrowTypeErrorAtom(ctx, atom, fmt, "")
#define JS_ThrowSyntaxErrorAtom(ctx, fmt, atom) __JS_ThrowSyntaxErrorAtom(ctx, atom, fmt, "")

static int JS_ThrowTypeErrorReadOnly(JSContext *ctx, int flags, JSAtom atom)
{
    if ((flags & JS_PROP_THROW) ||
        ((flags & JS_PROP_THROW_STRICT) && is_strict_mode(ctx))) {
        JS_ThrowTypeErrorAtom(ctx, "'%s' is read-only", atom);
        return -1;
    } else {
        return FALSE;
    }
}

JSValue __attribute__((format(printf, 2, 3))) JS_ThrowReferenceError(JSContext *ctx, const char *fmt, ...)
{
    JSValue val;
    va_list ap;

    va_start(ap, fmt);
    val = JS_ThrowError(ctx, JS_REFERENCE_ERROR, fmt, ap);
    va_end(ap);
    return val;
}

JSValue __attribute__((format(printf, 2, 3))) JS_ThrowRangeError(JSContext *ctx, const char *fmt, ...)
{
    JSValue val;
    va_list ap;

    va_start(ap, fmt);
    val = JS_ThrowError(ctx, JS_RANGE_ERROR, fmt, ap);
    va_end(ap);
    return val;
}

JSValue __attribute__((format(printf, 2, 3))) JS_ThrowInternalError(JSContext *ctx, const char *fmt, ...)
{
    JSValue val;
    va_list ap;

    va_start(ap, fmt);
    val = JS_ThrowError(ctx, JS_INTERNAL_ERROR, fmt, ap);
    va_end(ap);
    return val;
}

JSValue JS_ThrowOutOfMemory(JSContext *ctx)
{
    JSRuntime *rt = ctx->rt;
    if (!rt->in_out_of_memory) {
        rt->in_out_of_memory = TRUE;
        JS_ThrowInternalError(ctx, "out of memory");
        rt->in_out_of_memory = FALSE;
    }
    return JS_EXCEPTION;
}

static JSValue JS_ThrowStackOverflow(JSContext *ctx)
{
    return JS_ThrowInternalError(ctx, "stack overflow");
}

static JSValue JS_ThrowTypeErrorNotAnObject(JSContext *ctx)
{
    return JS_ThrowTypeError(ctx, "not an object");
}

static JSValue JS_ThrowTypeErrorNotAConstructor(JSContext *ctx,
                                                JSValueConst func_obj)
{
    const char *name;
    if (!JS_IsFunction(ctx, func_obj))
        goto fail;
    name = get_prop_string(ctx, func_obj, JS_ATOM_name);
    if (!name) {
    fail:
        return JS_ThrowTypeError(ctx, "not a constructor");
    }
    JS_ThrowTypeError(ctx, "%s is not a constructor", name);
    JS_FreeCString(ctx, name);
    return JS_EXCEPTION;
}

static JSValue JS_ThrowTypeErrorNotASymbol(JSContext *ctx)
{
    return JS_ThrowTypeError(ctx, "not a symbol");
}

static JSValue JS_ThrowReferenceErrorNotDefined(JSContext *ctx, JSAtom name)
{
    char buf[ATOM_GET_STR_BUF_SIZE];
    return JS_ThrowReferenceError(ctx, "'%s' is not defined",
                                  JS_AtomGetStr(ctx, buf, sizeof(buf), name));
}

static JSValue JS_ThrowReferenceErrorUninitialized(JSContext *ctx, JSAtom name)
{
    char buf[ATOM_GET_STR_BUF_SIZE];
    return JS_ThrowReferenceError(ctx, "%s is not initialized",
                                  name == JS_ATOM_NULL ? "lexical variable" :
                                  JS_AtomGetStr(ctx, buf, sizeof(buf), name));
}

static JSValue JS_ThrowReferenceErrorUninitialized2(JSContext *ctx,
                                                    JSFunctionBytecode *b,
                                                    int idx, BOOL is_ref)
{
    JSAtom atom = JS_ATOM_NULL;
    if (is_ref) {
        atom = b->closure_var[idx].var_name;
    } else {
        /* not present if the function is stripped and contains no eval() */
        if (b->vardefs)
            atom = b->vardefs[b->arg_count + idx].var_name;
    }
    return JS_ThrowReferenceErrorUninitialized(ctx, atom);
}

static JSValue JS_ThrowTypeErrorInvalidClass(JSContext *ctx, int class_id)
{
    JSRuntime *rt = ctx->rt;
    JSAtom name;
    name = rt->class_array[class_id].class_name;
    return JS_ThrowTypeErrorAtom(ctx, "%s object expected", name);
}

static void JS_ThrowInterrupted(JSContext *ctx)
{
    JS_ThrowInternalError(ctx, "interrupted");
    JS_SetUncatchableException(ctx, TRUE);
}

static no_inline __exception int __js_poll_interrupts(JSContext *ctx)
{
    JSRuntime *rt = ctx->rt;
    ctx->interrupt_counter = JS_INTERRUPT_COUNTER_INIT;
    if (rt->interrupt_handler) {
        if (rt->interrupt_handler(rt, rt->interrupt_opaque)) {
            JS_ThrowInterrupted(ctx);
            return -1;
        }
    }
    return 0;
}

static inline __exception int js_poll_interrupts(JSContext *ctx)
{
    if (unlikely(--ctx->interrupt_counter <= 0)) {
        return __js_poll_interrupts(ctx);
    } else {
        return 0;
    }
}

static void JS_SetImmutablePrototype(JSContext *ctx, JSValueConst obj)
{
    JSObject *p;
    if (JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT)
        return;
    p = JS_VALUE_GET_OBJ(obj);
    p->has_immutable_prototype = TRUE;
}

/* Return -1 (exception) or TRUE/FALSE. 'throw_flag' = FALSE indicates
   that it is called from Reflect.setPrototypeOf(). */
static int JS_SetPrototypeInternal(JSContext *ctx, JSValueConst obj,
                                   JSValueConst proto_val,
                                   BOOL throw_flag)
{
    JSObject *proto, *p, *p1;
    JSShape *sh;

    if (throw_flag) {
        if (JS_VALUE_GET_TAG(obj) == JS_TAG_NULL ||
            JS_VALUE_GET_TAG(obj) == JS_TAG_UNDEFINED)
            goto not_obj;
    } else {
        if (JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT)
            goto not_obj;
    }
    p = JS_VALUE_GET_OBJ(obj);
    if (JS_VALUE_GET_TAG(proto_val) != JS_TAG_OBJECT) {
        if (JS_VALUE_GET_TAG(proto_val) != JS_TAG_NULL) {
        not_obj:
            JS_ThrowTypeErrorNotAnObject(ctx);
            return -1;
        }
        proto = NULL;
    } else {
        proto = JS_VALUE_GET_OBJ(proto_val);
    }

    if (throw_flag && JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT)
        return TRUE;

    if (unlikely(p->is_exotic)) {
        const JSClassExoticMethods *em = ctx->rt->class_array[p->class_id].exotic;
        int ret;
        if (em && em->set_prototype) {
            ret = em->set_prototype(ctx, obj, proto_val);
            if (ret == 0 && throw_flag) {
                JS_ThrowTypeError(ctx, "proxy: bad prototype");
                return -1;
            } else {
                return ret;
            }
        }
    }

    sh = p->shape;
    if (sh->proto == proto)
        return TRUE;
    if (unlikely(p->has_immutable_prototype)) {
        if (throw_flag) {
            JS_ThrowTypeError(ctx, "prototype is immutable");
            return -1;
        } else {
            return FALSE;
        }
    }
    if (unlikely(!p->extensible)) {
        if (throw_flag) {
            JS_ThrowTypeError(ctx, "object is not extensible");
            return -1;
        } else {
            return FALSE;
        }
    }
    if (proto) {
        /* check if there is a cycle */
        p1 = proto;
        do {
            if (p1 == p) {
                if (throw_flag) {
                    JS_ThrowTypeError(ctx, "circular prototype chain");
                    return -1;
                } else {
                    return FALSE;
                }
            }
            /* Note: for Proxy objects, proto is NULL */
            p1 = p1->shape->proto;
        } while (p1 != NULL);
        JS_DupValue(ctx, proto_val);
    }

    if (js_shape_prepare_update(ctx, p, NULL))
        return -1;
    sh = p->shape;
    if (sh->proto)
        JS_FreeValue(ctx, JS_MKPTR(JS_TAG_OBJECT, sh->proto));
    sh->proto = proto;
    p->is_std_array_prototype = FALSE;
    return TRUE;
}

/* return -1 (exception) or TRUE/FALSE */
int JS_SetPrototype(JSContext *ctx, JSValueConst obj, JSValueConst proto_val)
{
    return JS_SetPrototypeInternal(ctx, obj, proto_val, TRUE);
}

/* Only works for primitive types, otherwise return JS_NULL. */
static JSValueConst JS_GetPrototypePrimitive(JSContext *ctx, JSValueConst val)
{
    switch(JS_VALUE_GET_NORM_TAG(val)) {
    case JS_TAG_SHORT_BIG_INT:
    case JS_TAG_BIG_INT:
        val = ctx->class_proto[JS_CLASS_BIG_INT];
        break;
    case JS_TAG_INT:
    case JS_TAG_FLOAT64:
        val = ctx->class_proto[JS_CLASS_NUMBER];
        break;
    case JS_TAG_BOOL:
        val = ctx->class_proto[JS_CLASS_BOOLEAN];
        break;
    case JS_TAG_STRING:
    case JS_TAG_STRING_ROPE:
        val = ctx->class_proto[JS_CLASS_STRING];
        break;
    case JS_TAG_SYMBOL:
        val = ctx->class_proto[JS_CLASS_SYMBOL];
        break;
    case JS_TAG_OBJECT:
    case JS_TAG_NULL:
    case JS_TAG_UNDEFINED:
    default:
        val = JS_NULL;
        break;
    }
    return val;
}

/* Return an Object, JS_NULL or JS_EXCEPTION in case of exotic object. */
JSValue JS_GetPrototype(JSContext *ctx, JSValueConst obj)
{
    JSValue val;
    if (JS_VALUE_GET_TAG(obj) == JS_TAG_OBJECT) {
        JSObject *p;
        p = JS_VALUE_GET_OBJ(obj);
        if (unlikely(p->is_exotic)) {
            const JSClassExoticMethods *em = ctx->rt->class_array[p->class_id].exotic;
            if (em && em->get_prototype) {
                return em->get_prototype(ctx, obj);
            }
        }
        p = p->shape->proto;
        if (!p)
            val = JS_NULL;
        else
            val = JS_DupValue(ctx, JS_MKPTR(JS_TAG_OBJECT, p));
    } else {
        val = JS_DupValue(ctx, JS_GetPrototypePrimitive(ctx, obj));
    }
    return val;
}

static JSValue JS_GetPrototypeFree(JSContext *ctx, JSValue obj)
{
    JSValue obj1;
    obj1 = JS_GetPrototype(ctx, obj);
    JS_FreeValue(ctx, obj);
    return obj1;
}

/* return TRUE, FALSE or (-1) in case of exception */
static int JS_OrdinaryIsInstanceOf(JSContext *ctx, JSValueConst val,
                                   JSValueConst obj)
{
    JSValue obj_proto;
    JSObject *proto;
    const JSObject *p, *proto1;
    BOOL ret;

    if (!JS_IsFunction(ctx, obj))
        return FALSE;
    p = JS_VALUE_GET_OBJ(obj);
    if (p->class_id == JS_CLASS_BOUND_FUNCTION) {
        JSBoundFunction *s = p->u.bound_function;
        return JS_IsInstanceOf(ctx, val, s->func_obj);
    }

    /* Only explicitly boxed values are instances of constructors */
    if (JS_VALUE_GET_TAG(val) != JS_TAG_OBJECT)
        return FALSE;
    obj_proto = JS_GetProperty(ctx, obj, JS_ATOM_prototype);
    if (JS_VALUE_GET_TAG(obj_proto) != JS_TAG_OBJECT) {
        if (!JS_IsException(obj_proto))
            JS_ThrowTypeError(ctx, "operand 'prototype' property is not an object");
        ret = -1;
        goto done;
    }
    proto = JS_VALUE_GET_OBJ(obj_proto);
    p = JS_VALUE_GET_OBJ(val);
    for(;;) {
        proto1 = p->shape->proto;
        if (!proto1) {
            /* slow case if exotic object in the prototype chain */
            if (unlikely(p->is_exotic && !p->fast_array)) {
                JSValue obj1;
                obj1 = JS_DupValue(ctx, JS_MKPTR(JS_TAG_OBJECT, (JSObject *)p));
                for(;;) {
                    obj1 = JS_GetPrototypeFree(ctx, obj1);
                    if (JS_IsException(obj1)) {
                        ret = -1;
                        break;
                    }
                    if (JS_IsNull(obj1)) {
                        ret = FALSE;
                        break;
                    }
                    if (proto == JS_VALUE_GET_OBJ(obj1)) {
                        JS_FreeValue(ctx, obj1);
                        ret = TRUE;
                        break;
                    }
                    /* must check for timeout to avoid infinite loop */
                    if (js_poll_interrupts(ctx)) {
                        JS_FreeValue(ctx, obj1);
                        ret = -1;
                        break;
                    }
                }
            } else {
                ret = FALSE;
            }
            break;
        }
        p = proto1;
        if (proto == p) {
            ret = TRUE;
            break;
        }
    }
done:
    JS_FreeValue(ctx, obj_proto);
    return ret;
}

/* return TRUE, FALSE or (-1) in case of exception */
int JS_IsInstanceOf(JSContext *ctx, JSValueConst val, JSValueConst obj)
{
    JSValue method;

    if (!JS_IsObject(obj))
        goto fail;
    method = JS_GetProperty(ctx, obj, JS_ATOM_Symbol_hasInstance);
    if (JS_IsException(method))
        return -1;
    if (!JS_IsNull(method) && !JS_IsUndefined(method)) {
        JSValue ret;
        ret = JS_CallFree(ctx, method, obj, 1, &val);
        return JS_ToBoolFree(ctx, ret);
    }

    /* legacy case */
    if (!JS_IsFunction(ctx, obj)) {
    fail:
        JS_ThrowTypeError(ctx, "invalid 'instanceof' right operand");
        return -1;
    }
    return JS_OrdinaryIsInstanceOf(ctx, val, obj);
}

/* return the value associated to the autoinit property or an exception */
typedef JSValue JSAutoInitFunc(JSContext *ctx, JSObject *p, JSAtom atom, void *opaque);

static JSAutoInitFunc *js_autoinit_func_table[] = {
    js_instantiate_prototype, /* JS_AUTOINIT_ID_PROTOTYPE */
    js_module_ns_autoinit, /* JS_AUTOINIT_ID_MODULE_NS */
    JS_InstantiateFunctionListItem2, /* JS_AUTOINIT_ID_PROP */
};

/* warning: 'prs' is reallocated after it */
static int JS_AutoInitProperty(JSContext *ctx, JSObject *p, JSAtom prop,
                               JSProperty *pr, JSShapeProperty *prs)
{
    JSValue val;
    JSContext *realm;
    JSAutoInitFunc *func;
    JSAutoInitIDEnum id;

    if (js_shape_prepare_update(ctx, p, &prs))
        return -1;

    realm = js_autoinit_get_realm(pr);
    id = js_autoinit_get_id(pr);
    func = js_autoinit_func_table[id];
    /* 'func' shall not modify the object properties 'pr' */
    val = func(realm, p, prop, pr->u.init.opaque);
    js_autoinit_free(ctx->rt, pr);
    prs->flags &= ~JS_PROP_TMASK;
    pr->u.value = JS_UNDEFINED;
    if (JS_IsException(val))
        return -1;
    if (id == JS_AUTOINIT_ID_MODULE_NS &&
        JS_VALUE_GET_TAG(val) == JS_TAG_STRING) {
        /* WARNING: a varref is returned as a string  ! */
        prs->flags |= JS_PROP_VARREF;
        pr->u.var_ref = JS_VALUE_GET_PTR(val);
        js_rc(pr->u.var_ref)->ref_count++;
    } else if (p->class_id == JS_CLASS_GLOBAL_OBJECT) {
        JSVarRef *var_ref;
        /* in the global object we use references */
        var_ref = js_create_var_ref(ctx, FALSE);
        if (!var_ref)
            return -1;
        prs->flags |= JS_PROP_VARREF;
        pr->u.var_ref = var_ref;
        var_ref->value = val;
        var_ref->is_const = !(prs->flags & JS_PROP_WRITABLE);
    } else {
        pr->u.value = val;
    }
    return 0;
}

JSValue JS_GetPropertyInternal(JSContext *ctx, JSValueConst obj,
                               JSAtom prop, JSValueConst this_obj,
                               BOOL throw_ref_error)
{
    JSObject *p;
    JSProperty *pr;
    JSShapeProperty *prs;
    uint32_t tag;

    tag = JS_VALUE_GET_TAG(obj);
    if (unlikely(tag != JS_TAG_OBJECT)) {
        switch(tag) {
        case JS_TAG_NULL:
            return JS_ThrowTypeErrorAtom(ctx, "cannot read property '%s' of null", prop);
        case JS_TAG_UNDEFINED:
            return JS_ThrowTypeErrorAtom(ctx, "cannot read property '%s' of undefined", prop);
        case JS_TAG_EXCEPTION:
            return JS_EXCEPTION;
        case JS_TAG_STRING:
            {
                JSString *p1 = JS_VALUE_GET_STRING(obj);
                if (__JS_AtomIsTaggedInt(prop)) {
                    uint32_t idx;
                    idx = __JS_AtomToUInt32(prop);
                    if (idx < p1->len) {
                        return js_new_string_char(ctx, string_get(p1, idx));
                    }
                } else if (prop == JS_ATOM_length) {
                    return JS_NewInt32(ctx, p1->len);
                }
            }
            break;
        case JS_TAG_STRING_ROPE:
            {
                JSStringRope *p1 = JS_VALUE_GET_STRING_ROPE(obj);
                if (__JS_AtomIsTaggedInt(prop)) {
                    uint32_t idx;
                    idx = __JS_AtomToUInt32(prop);
                    if (idx < p1->len) {
                        return js_new_string_char(ctx, string_rope_get(obj, idx));
                    }
                } else if (prop == JS_ATOM_length) {
                    return JS_NewInt32(ctx, p1->len);
                }
            }
            break;
        default:
            break;
        }
        /* cannot raise an exception */
        p = JS_VALUE_GET_OBJ(JS_GetPrototypePrimitive(ctx, obj));
        if (!p)
            return JS_UNDEFINED;
    } else {
        p = JS_VALUE_GET_OBJ(obj);
    }

    for(;;) {
        prs = find_own_property(&pr, p, prop);
        if (prs) {
            /* found */
            if (unlikely(prs->flags & JS_PROP_TMASK)) {
                if ((prs->flags & JS_PROP_TMASK) == JS_PROP_GETSET) {
                    if (unlikely(!pr->u.getset.getter)) {
                        return JS_UNDEFINED;
                    } else {
                        JSValue func = JS_MKPTR(JS_TAG_OBJECT, pr->u.getset.getter);
                        /* Note: the field could be removed in the getter */
                        func = JS_DupValue(ctx, func);
                        return JS_CallFree(ctx, func, this_obj, 0, NULL);
                    }
                } else if ((prs->flags & JS_PROP_TMASK) == JS_PROP_VARREF) {
                    JSValue val = *pr->u.var_ref->pvalue;
                    if (unlikely(JS_IsUninitialized(val)))
                        return JS_ThrowReferenceErrorUninitialized(ctx, prs->atom);
                    return JS_DupValue(ctx, val);
                } else if ((prs->flags & JS_PROP_TMASK) == JS_PROP_AUTOINIT) {
                    /* Instantiate property and retry */
                    if (JS_AutoInitProperty(ctx, p, prop, pr, prs))
                        return JS_EXCEPTION;
                    continue;
                }
            } else {
                return JS_DupValue(ctx, pr->u.value);
            }
        }
        if (unlikely(p->is_exotic)) {
            /* exotic behaviors */
            if (p->fast_array) {
                if (__JS_AtomIsTaggedInt(prop)) {
                    uint32_t idx = __JS_AtomToUInt32(prop);
                    if (idx < p->u.array.count) {
                        /* we avoid duplicating the code */
                        return JS_GetPropertyUint32(ctx, JS_MKPTR(JS_TAG_OBJECT, p), idx);
                    } else if (p->class_id >= JS_CLASS_UINT8C_ARRAY &&
                               p->class_id <= JS_CLASS_FLOAT64_ARRAY) {
                        return JS_UNDEFINED;
                    }
                } else if (p->class_id >= JS_CLASS_UINT8C_ARRAY &&
                           p->class_id <= JS_CLASS_FLOAT64_ARRAY) {
                    int ret;
                    ret = JS_AtomIsNumericIndex(ctx, prop);
                    if (ret != 0) {
                        if (ret < 0)
                            return JS_EXCEPTION;
                        return JS_UNDEFINED;
                    }
                }
            } else {
                const JSClassExoticMethods *em = ctx->rt->class_array[p->class_id].exotic;
                if (em) {
                    if (em->get_property) {
                        JSValue obj1, retval;
                        /* XXX: should pass throw_ref_error */
                        /* Note: if 'p' is a prototype, it can be
                           freed in the called function */
                        obj1 = JS_DupValue(ctx, JS_MKPTR(JS_TAG_OBJECT, p));
                        retval = em->get_property(ctx, obj1, prop, this_obj);
                        JS_FreeValue(ctx, obj1);
                        return retval;
                    }
                    if (em->get_own_property) {
                        JSPropertyDescriptor desc;
                        int ret;
                        JSValue obj1;

                        /* Note: if 'p' is a prototype, it can be
                           freed in the called function */
                        obj1 = JS_DupValue(ctx, JS_MKPTR(JS_TAG_OBJECT, p));
                        ret = em->get_own_property(ctx, &desc, obj1, prop);
                        JS_FreeValue(ctx, obj1);
                        if (ret < 0)
                            return JS_EXCEPTION;
                        if (ret) {
                            if (desc.flags & JS_PROP_GETSET) {
                                JS_FreeValue(ctx, desc.setter);
                                return JS_CallFree(ctx, desc.getter, this_obj, 0, NULL);
                            } else {
                                return desc.value;
                            }
                        }
                    }
                }
            }
        }
        p = p->shape->proto;
        if (!p)
            break;
    }
    if (unlikely(throw_ref_error)) {
        return JS_ThrowReferenceErrorNotDefined(ctx, prop);
    } else {
        return JS_UNDEFINED;
    }
}

static JSValue JS_ThrowTypeErrorPrivateNotFound(JSContext *ctx, JSAtom atom)
{
    return JS_ThrowTypeErrorAtom(ctx, "private class field '%s' does not exist",
                                 atom);
}

/* Private fields can be added even on non extensible objects or
   Proxies */
static int JS_DefinePrivateField(JSContext *ctx, JSValueConst obj,
                                 JSValueConst name, JSValue val)
{
    JSObject *p;
    JSShapeProperty *prs;
    JSProperty *pr;
    JSAtom prop;

    if (unlikely(JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT)) {
        JS_ThrowTypeErrorNotAnObject(ctx);
        goto fail;
    }
    /* safety check */
    if (unlikely(JS_VALUE_GET_TAG(name) != JS_TAG_SYMBOL)) {
        JS_ThrowTypeErrorNotASymbol(ctx);
        goto fail;
    }
    prop = js_symbol_to_atom(ctx, (JSValue)name);
    p = JS_VALUE_GET_OBJ(obj);
    prs = find_own_property(&pr, p, prop);
    if (prs) {
        JS_ThrowTypeErrorAtom(ctx, "private class field '%s' already exists",
                              prop);
        goto fail;
    }
    pr = add_property(ctx, p, prop, JS_PROP_C_W_E);
    if (unlikely(!pr)) {
    fail:
        JS_FreeValue(ctx, val);
        return -1;
    }
    pr->u.value = val;
    return 0;
}

static JSValue JS_GetPrivateField(JSContext *ctx, JSValueConst obj,
                                  JSValueConst name)
{
    JSObject *p;
    JSShapeProperty *prs;
    JSProperty *pr;
    JSAtom prop;

    if (unlikely(JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT))
        return JS_ThrowTypeErrorNotAnObject(ctx);
    /* safety check */
    if (unlikely(JS_VALUE_GET_TAG(name) != JS_TAG_SYMBOL))
        return JS_ThrowTypeErrorNotASymbol(ctx);
    prop = js_symbol_to_atom(ctx, (JSValue)name);
    p = JS_VALUE_GET_OBJ(obj);
    prs = find_own_property(&pr, p, prop);
    if (!prs) {
        JS_ThrowTypeErrorPrivateNotFound(ctx, prop);
        return JS_EXCEPTION;
    }
    return JS_DupValue(ctx, pr->u.value);
}

static int JS_SetPrivateField(JSContext *ctx, JSValueConst obj,
                              JSValueConst name, JSValue val)
{
    JSObject *p;
    JSShapeProperty *prs;
    JSProperty *pr;
    JSAtom prop;

    if (unlikely(JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT)) {
        JS_ThrowTypeErrorNotAnObject(ctx);
        goto fail;
    }
    /* safety check */
    if (unlikely(JS_VALUE_GET_TAG(name) != JS_TAG_SYMBOL)) {
        JS_ThrowTypeErrorNotASymbol(ctx);
        goto fail;
    }
    prop = js_symbol_to_atom(ctx, (JSValue)name);
    p = JS_VALUE_GET_OBJ(obj);
    prs = find_own_property(&pr, p, prop);
    if (!prs) {
        JS_ThrowTypeErrorPrivateNotFound(ctx, prop);
    fail:
        JS_FreeValue(ctx, val);
        return -1;
    }
    set_value(ctx, &pr->u.value, val);
    return 0;
}

/* add a private brand field to 'home_obj' if not already present and
   if obj is != null add a private brand to it */
static int JS_AddBrand(JSContext *ctx, JSValueConst obj, JSValueConst home_obj)
{
    JSObject *p, *p1;
    JSShapeProperty *prs;
    JSProperty *pr;
    JSValue brand;
    JSAtom brand_atom;

    if (unlikely(JS_VALUE_GET_TAG(home_obj) != JS_TAG_OBJECT)) {
        JS_ThrowTypeErrorNotAnObject(ctx);
        return -1;
    }
    p = JS_VALUE_GET_OBJ(home_obj);
    prs = find_own_property(&pr, p, JS_ATOM_Private_brand);
    if (!prs) {
        /* if the brand is not present, add it */
        brand = JS_NewSymbolFromAtom(ctx, JS_ATOM_brand, JS_ATOM_TYPE_PRIVATE);
        if (JS_IsException(brand))
            return -1;
        pr = add_property(ctx, p, JS_ATOM_Private_brand, JS_PROP_C_W_E);
        if (!pr) {
            JS_FreeValue(ctx, brand);
            return -1;
        }
        pr->u.value = JS_DupValue(ctx, brand);
    } else {
        brand = JS_DupValue(ctx, pr->u.value);
    }
    brand_atom = js_symbol_to_atom(ctx, brand);

    if (JS_IsObject(obj)) {
        p1 = JS_VALUE_GET_OBJ(obj);
        prs = find_own_property(&pr, p1, brand_atom);
        if (unlikely(prs)) {
            JS_FreeAtom(ctx, brand_atom);
            JS_ThrowTypeError(ctx, "private method is already present");
            return -1;
        }
        pr = add_property(ctx, p1, brand_atom, JS_PROP_C_W_E);
        JS_FreeAtom(ctx, brand_atom);
        if (!pr)
            return -1;
        pr->u.value = JS_UNDEFINED;
    } else {
        JS_FreeAtom(ctx, brand_atom);
    }
    return 0;
}

/* return a boolean telling if the brand of the home object of 'func'
   is present on 'obj' or -1 in case of exception */
static int JS_CheckBrand(JSContext *ctx, JSValueConst obj, JSValueConst func)
{
    JSObject *p, *p1, *home_obj;
    JSShapeProperty *prs;
    JSProperty *pr;
    JSValueConst brand;

    /* get the home object of 'func' */
    if (unlikely(JS_VALUE_GET_TAG(func) != JS_TAG_OBJECT))
        goto not_obj;
    p1 = JS_VALUE_GET_OBJ(func);
    if (!js_class_has_bytecode(p1->class_id))
        goto not_obj;
    home_obj = p1->u.func.home_object;
    if (!home_obj)
        goto not_obj;
    prs = find_own_property(&pr, home_obj, JS_ATOM_Private_brand);
    if (!prs) {
        JS_ThrowTypeError(ctx, "expecting <brand> private field");
        return -1;
    }
    brand = pr->u.value;
    /* safety check */
    if (unlikely(JS_VALUE_GET_TAG(brand) != JS_TAG_SYMBOL))
        goto not_obj;

    /* get the brand array of 'obj' */
    if (unlikely(JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT)) {
    not_obj:
        JS_ThrowTypeErrorNotAnObject(ctx);
        return -1;
    }
    p = JS_VALUE_GET_OBJ(obj);
    prs = find_own_property(&pr, p, js_symbol_to_atom(ctx, (JSValue)brand));
    return (prs != NULL);
}

static uint32_t js_string_obj_get_length(JSContext *ctx,
                                         JSValueConst obj)
{
    JSObject *p;
    uint32_t len = 0;

    /* This is a class exotic method: obj class_id is JS_CLASS_STRING */
    p = JS_VALUE_GET_OBJ(obj);
    if (JS_VALUE_GET_TAG(p->u.object_data) == JS_TAG_STRING) {
        JSString *p1 = JS_VALUE_GET_STRING(p->u.object_data);
        len = p1->len;
    }
    return len;
}

static int num_keys_cmp(const void *p1, const void *p2, void *opaque)
{
    JSContext *ctx = opaque;
    JSAtom atom1 = ((const JSPropertyEnum *)p1)->atom;
    JSAtom atom2 = ((const JSPropertyEnum *)p2)->atom;
    uint32_t v1, v2;
    BOOL atom1_is_integer, atom2_is_integer;

    atom1_is_integer = JS_AtomIsArrayIndex(ctx, &v1, atom1);
    atom2_is_integer = JS_AtomIsArrayIndex(ctx, &v2, atom2);
    assert(atom1_is_integer && atom2_is_integer);
    if (v1 < v2)
        return -1;
    else if (v1 == v2)
        return 0;
    else
        return 1;
}

void JS_FreePropertyEnum(JSContext *ctx, JSPropertyEnum *tab, uint32_t len)
{
    uint32_t i;
    if (tab) {
        for(i = 0; i < len; i++)
            JS_FreeAtom(ctx, tab[i].atom);
        js_free(ctx, tab);
    }
}

/* return < 0 in case if exception, 0 if OK. ptab and its atoms must
   be freed by the user. */
static int __exception JS_GetOwnPropertyNamesInternal(JSContext *ctx,
                                                      JSPropertyEnum **ptab,
                                                      uint32_t *plen,
                                                      JSObject *p, int flags)
{
    int i, j;
    JSShape *sh;
    JSShapeProperty *prs;
    JSPropertyEnum *tab_atom, *tab_exotic;
    JSAtom atom;
    uint32_t num_keys_count, str_keys_count, sym_keys_count, atom_count;
    uint32_t num_index, str_index, sym_index, exotic_count, exotic_keys_count;
    BOOL is_enumerable, num_sorted;
    uint32_t num_key;
    JSAtomKindEnum kind;

    /* clear pointer for consistency in case of failure */
    *ptab = NULL;
    *plen = 0;

    /* compute the number of returned properties */
    num_keys_count = 0;
    str_keys_count = 0;
    sym_keys_count = 0;
    exotic_keys_count = 0;
    exotic_count = 0;
    tab_exotic = NULL;
    sh = p->shape;
    for(i = 0, prs = get_shape_prop(sh); i < sh->prop_count; i++, prs++) {
        atom = prs->atom;
        if (atom != JS_ATOM_NULL) {
            is_enumerable = ((prs->flags & JS_PROP_ENUMERABLE) != 0);
            kind = JS_AtomGetKind(ctx, atom);
            if ((!(flags & JS_GPN_ENUM_ONLY) || is_enumerable) &&
                ((flags >> kind) & 1) != 0) {
                /* need to raise an exception in case of the module
                   name space (implicit GetOwnProperty) */
                if (unlikely((prs->flags & JS_PROP_TMASK) == JS_PROP_VARREF) &&
                    (flags & (JS_GPN_SET_ENUM | JS_GPN_ENUM_ONLY))) {
                    JSVarRef *var_ref = p->prop[i].u.var_ref;
                    if (unlikely(JS_IsUninitialized(*var_ref->pvalue))) {
                        JS_ThrowReferenceErrorUninitialized(ctx, prs->atom);
                        return -1;
                    }
                }
                if (JS_AtomIsArrayIndex(ctx, &num_key, atom)) {
                    num_keys_count++;
                } else if (kind == JS_ATOM_KIND_STRING) {
                    str_keys_count++;
                } else {
                    sym_keys_count++;
                }
            }
        }
    }

    if (p->is_exotic) {
        if (p->fast_array) {
            if (flags & JS_GPN_STRING_MASK) {
                num_keys_count += p->u.array.count;
            }
        } else if (p->class_id == JS_CLASS_STRING) {
            if (flags & JS_GPN_STRING_MASK) {
                num_keys_count += js_string_obj_get_length(ctx, JS_MKPTR(JS_TAG_OBJECT, p));
            }
        } else {
            const JSClassExoticMethods *em = ctx->rt->class_array[p->class_id].exotic;
            if (em && em->get_own_property_names) {
                if (em->get_own_property_names(ctx, &tab_exotic, &exotic_count,
                                               JS_MKPTR(JS_TAG_OBJECT, p)))
                    return -1;
                for(i = 0; i < exotic_count; i++) {
                    atom = tab_exotic[i].atom;
                    kind = JS_AtomGetKind(ctx, atom);
                    if (((flags >> kind) & 1) != 0) {
                        is_enumerable = FALSE;
                        if (flags & (JS_GPN_SET_ENUM | JS_GPN_ENUM_ONLY)) {
                            JSPropertyDescriptor desc;
                            int res;
                            /* set the "is_enumerable" field if necessary */
                            res = JS_GetOwnPropertyInternal(ctx, &desc, p, atom);
                            if (res < 0) {
                                JS_FreePropertyEnum(ctx, tab_exotic, exotic_count);
                                return -1;
                            }
                            if (res) {
                                is_enumerable =
                                    ((desc.flags & JS_PROP_ENUMERABLE) != 0);
                                js_free_desc(ctx, &desc);
                            }
                            tab_exotic[i].is_enumerable = is_enumerable;
                        }
                        if (!(flags & JS_GPN_ENUM_ONLY) || is_enumerable) {
                            exotic_keys_count++;
                        }
                    }
                }
            }
        }
    }

    /* fill them */

    atom_count = num_keys_count + str_keys_count;
    if (atom_count < str_keys_count)
        goto add_overflow;
    atom_count += sym_keys_count;
    if (atom_count < sym_keys_count)
        goto add_overflow;
    atom_count += exotic_keys_count;
    if (atom_count < exotic_keys_count || atom_count > INT32_MAX) {
    add_overflow:
        JS_ThrowOutOfMemory(ctx);
        JS_FreePropertyEnum(ctx, tab_exotic, exotic_count);
        return -1;
    }
    /* XXX: need generic way to test for js_malloc(ctx, a * b) overflow */

    /* avoid allocating 0 bytes */
    tab_atom = js_malloc(ctx, sizeof(tab_atom[0]) * max_int(atom_count, 1));
    if (!tab_atom) {
        JS_FreePropertyEnum(ctx, tab_exotic, exotic_count);
        return -1;
    }

    num_index = 0;
    str_index = num_keys_count;
    sym_index = str_index + str_keys_count;

    num_sorted = TRUE;
    sh = p->shape;
    for(i = 0, prs = get_shape_prop(sh); i < sh->prop_count; i++, prs++) {
        atom = prs->atom;
        if (atom != JS_ATOM_NULL) {
            is_enumerable = ((prs->flags & JS_PROP_ENUMERABLE) != 0);
            kind = JS_AtomGetKind(ctx, atom);
            if ((!(flags & JS_GPN_ENUM_ONLY) || is_enumerable) &&
                ((flags >> kind) & 1) != 0) {
                if (JS_AtomIsArrayIndex(ctx, &num_key, atom)) {
                    j = num_index++;
                    num_sorted = FALSE;
                } else if (kind == JS_ATOM_KIND_STRING) {
                    j = str_index++;
                } else {
                    j = sym_index++;
                }
                tab_atom[j].atom = JS_DupAtom(ctx, atom);
                tab_atom[j].is_enumerable = is_enumerable;
            }
        }
    }

    if (p->is_exotic) {
        int len;
        if (p->fast_array) {
            if (flags & JS_GPN_STRING_MASK) {
                len = p->u.array.count;
                goto add_array_keys;
            }
        } else if (p->class_id == JS_CLASS_STRING) {
            if (flags & JS_GPN_STRING_MASK) {
                len = js_string_obj_get_length(ctx, JS_MKPTR(JS_TAG_OBJECT, p));
            add_array_keys:
                for(i = 0; i < len; i++) {
                    tab_atom[num_index].atom = __JS_AtomFromUInt32(i);
                    if (tab_atom[num_index].atom == JS_ATOM_NULL) {
                        JS_FreePropertyEnum(ctx, tab_atom, num_index);
                        return -1;
                    }
                    tab_atom[num_index].is_enumerable = TRUE;
                    num_index++;
                }
            }
        } else {
            /* Note: exotic keys are not reordered and comes after the object own properties. */
            for(i = 0; i < exotic_count; i++) {
                atom = tab_exotic[i].atom;
                is_enumerable = tab_exotic[i].is_enumerable;
                kind = JS_AtomGetKind(ctx, atom);
                if ((!(flags & JS_GPN_ENUM_ONLY) || is_enumerable) &&
                    ((flags >> kind) & 1) != 0) {
                    tab_atom[sym_index].atom = atom;
                    tab_atom[sym_index].is_enumerable = is_enumerable;
                    sym_index++;
                } else {
                    JS_FreeAtom(ctx, atom);
                }
            }
            js_free(ctx, tab_exotic);
        }
    }

    assert(num_index == num_keys_count);
    assert(str_index == num_keys_count + str_keys_count);
    assert(sym_index == atom_count);

    if (num_keys_count != 0 && !num_sorted) {
        rqsort(tab_atom, num_keys_count, sizeof(tab_atom[0]), num_keys_cmp,
               ctx);
    }
    *ptab = tab_atom;
    *plen = atom_count;
    return 0;
}

int JS_GetOwnPropertyNames(JSContext *ctx, JSPropertyEnum **ptab,
                           uint32_t *plen, JSValueConst obj, int flags)
{
    if (JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT) {
        JS_ThrowTypeErrorNotAnObject(ctx);
        return -1;
    }
    return JS_GetOwnPropertyNamesInternal(ctx, ptab, plen,
                                          JS_VALUE_GET_OBJ(obj), flags);
}

/* Return -1 if exception,
   FALSE if the property does not exist, TRUE if it exists. If TRUE is
   returned, the property descriptor 'desc' is filled present. */
static int JS_GetOwnPropertyInternal(JSContext *ctx, JSPropertyDescriptor *desc,
                                     JSObject *p, JSAtom prop)
{
    JSShapeProperty *prs;
    JSProperty *pr;

retry:
    prs = find_own_property(&pr, p, prop);
    if (prs) {
        if (desc) {
            desc->flags = prs->flags & JS_PROP_C_W_E;
            desc->getter = JS_UNDEFINED;
            desc->setter = JS_UNDEFINED;
            desc->value = JS_UNDEFINED;
            if (unlikely(prs->flags & JS_PROP_TMASK)) {
                if ((prs->flags & JS_PROP_TMASK) == JS_PROP_GETSET) {
                    desc->flags |= JS_PROP_GETSET;
                    if (pr->u.getset.getter)
                        desc->getter = JS_DupValue(ctx, JS_MKPTR(JS_TAG_OBJECT, pr->u.getset.getter));
                    if (pr->u.getset.setter)
                        desc->setter = JS_DupValue(ctx, JS_MKPTR(JS_TAG_OBJECT, pr->u.getset.setter));
                } else if ((prs->flags & JS_PROP_TMASK) == JS_PROP_VARREF) {
                    JSValue val = *pr->u.var_ref->pvalue;
                    if (unlikely(JS_IsUninitialized(val))) {
                        JS_ThrowReferenceErrorUninitialized(ctx, prs->atom);
                        return -1;
                    }
                    desc->value = JS_DupValue(ctx, val);
                } else if ((prs->flags & JS_PROP_TMASK) == JS_PROP_AUTOINIT) {
                    /* Instantiate property and retry */
                    if (JS_AutoInitProperty(ctx, p, prop, pr, prs))
                        return -1;
                    goto retry;
                }
            } else {
                desc->value = JS_DupValue(ctx, pr->u.value);
            }
        } else {
            /* for consistency, send the exception even if desc is NULL */
            if (unlikely((prs->flags & JS_PROP_TMASK) == JS_PROP_VARREF)) {
                if (unlikely(JS_IsUninitialized(*pr->u.var_ref->pvalue))) {
                    JS_ThrowReferenceErrorUninitialized(ctx, prs->atom);
                    return -1;
                }
            } else if ((prs->flags & JS_PROP_TMASK) == JS_PROP_AUTOINIT) {
                /* nothing to do: delay instantiation until actual value and/or attributes are read */
            }
        }
        return TRUE;
    }
    if (p->is_exotic) {
        if (p->fast_array) {
            /* specific case for fast arrays */
            if (__JS_AtomIsTaggedInt(prop)) {
                uint32_t idx;
                idx = __JS_AtomToUInt32(prop);
                if (idx < p->u.array.count) {
                    if (desc) {
                        desc->flags = JS_PROP_WRITABLE | JS_PROP_ENUMERABLE |
                            JS_PROP_CONFIGURABLE;
                        desc->getter = JS_UNDEFINED;
                        desc->setter = JS_UNDEFINED;
                        desc->value = JS_GetPropertyUint32(ctx, JS_MKPTR(JS_TAG_OBJECT, p), idx);
                    }
                    return TRUE;
                }
            }
        } else {
            const JSClassExoticMethods *em = ctx->rt->class_array[p->class_id].exotic;
            if (em && em->get_own_property) {
                return em->get_own_property(ctx, desc,
                                            JS_MKPTR(JS_TAG_OBJECT, p), prop);
            }
        }
    }
    return FALSE;
}

int JS_GetOwnProperty(JSContext *ctx, JSPropertyDescriptor *desc,
                      JSValueConst obj, JSAtom prop)
{
    if (JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT) {
        JS_ThrowTypeErrorNotAnObject(ctx);
        return -1;
    }
    return JS_GetOwnPropertyInternal(ctx, desc, JS_VALUE_GET_OBJ(obj), prop);
}

/* return -1 if exception (exotic object only) or TRUE/FALSE */
int JS_IsExtensible(JSContext *ctx, JSValueConst obj)
{
    JSObject *p;

    if (unlikely(JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT))
        return FALSE;
    p = JS_VALUE_GET_OBJ(obj);
    if (unlikely(p->is_exotic)) {
        const JSClassExoticMethods *em = ctx->rt->class_array[p->class_id].exotic;
        if (em && em->is_extensible) {
            return em->is_extensible(ctx, obj);
        }
    }
    return p->extensible;
}

/* return -1 if exception (exotic object only) or TRUE/FALSE */
int JS_PreventExtensions(JSContext *ctx, JSValueConst obj)
{
    JSObject *p;

    if (unlikely(JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT))
        return FALSE;
    p = JS_VALUE_GET_OBJ(obj);
    if (unlikely(p->is_exotic)) {
        if (p->class_id >= JS_CLASS_UINT8C_ARRAY &&
            p->class_id <= JS_CLASS_FLOAT64_ARRAY) {
            JSTypedArray *ta;
            JSArrayBuffer *abuf;
            /* resizable type arrays return FALSE */
            ta = p->u.typed_array;
            abuf = ta->buffer->u.array_buffer;
            if (ta->track_rab ||
                (array_buffer_is_resizable(abuf) && !abuf->shared))
                return FALSE;
        } else {
            const JSClassExoticMethods *em = ctx->rt->class_array[p->class_id].exotic;
            if (em && em->prevent_extensions) {
                return em->prevent_extensions(ctx, obj);
            }
        }
    }
    p->extensible = FALSE;
    return TRUE;
}

/* return -1 if exception otherwise TRUE or FALSE */
int JS_HasProperty(JSContext *ctx, JSValueConst obj, JSAtom prop)
{
    JSObject *p;
    int ret;
    JSValue obj1;

    if (unlikely(JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT))
        return FALSE;
    p = JS_VALUE_GET_OBJ(obj);
    for(;;) {
        if (p->is_exotic) {
            const JSClassExoticMethods *em = ctx->rt->class_array[p->class_id].exotic;
            if (em && em->has_property) {
                /* has_property can free the prototype */
                obj1 = JS_DupValue(ctx, JS_MKPTR(JS_TAG_OBJECT, p));
                ret = em->has_property(ctx, obj1, prop);
                JS_FreeValue(ctx, obj1);
                return ret;
            }
        }
        /* JS_GetOwnPropertyInternal can free the prototype */
        JS_DupValue(ctx, JS_MKPTR(JS_TAG_OBJECT, p));
        ret = JS_GetOwnPropertyInternal(ctx, NULL, p, prop);
        JS_FreeValue(ctx, JS_MKPTR(JS_TAG_OBJECT, p));
        if (ret != 0)
            return ret;
        if (p->class_id >= JS_CLASS_UINT8C_ARRAY &&
            p->class_id <= JS_CLASS_FLOAT64_ARRAY) {
            ret = JS_AtomIsNumericIndex(ctx, prop);
            if (ret != 0) {
                if (ret < 0)
                    return -1;
                return FALSE;
            }
        }
        p = p->shape->proto;
        if (!p)
            break;
    }
    return FALSE;
}

/* val must be a symbol */
static JSAtom js_symbol_to_atom(JSContext *ctx, JSValue val)
{
    JSAtomStruct *p = JS_VALUE_GET_PTR(val);
    return js_get_atom_index(ctx->rt, p);
}

/* return JS_ATOM_NULL in case of exception */
JSAtom JS_ValueToAtom(JSContext *ctx, JSValueConst val)
{
    JSAtom atom;
    uint32_t tag;
    tag = JS_VALUE_GET_TAG(val);
    if (tag == JS_TAG_INT &&
        (uint32_t)JS_VALUE_GET_INT(val) <= JS_ATOM_MAX_INT) {
        /* fast path for integer values */
        atom = __JS_AtomFromUInt32(JS_VALUE_GET_INT(val));
    } else if (tag == JS_TAG_SYMBOL) {
        JSAtomStruct *p = JS_VALUE_GET_PTR(val);
        atom = JS_DupAtom(ctx, js_get_atom_index(ctx->rt, p));
    } else {
        JSValue str;
        str = JS_ToPropertyKey(ctx, val);
        if (JS_IsException(str))
            return JS_ATOM_NULL;
        if (JS_VALUE_GET_TAG(str) == JS_TAG_SYMBOL) {
            atom = js_symbol_to_atom(ctx, str);
        } else {
            atom = JS_NewAtomStr(ctx, JS_VALUE_GET_STRING(str));
        }
    }
    return atom;
}

static JSValue JS_GetPropertyValue(JSContext *ctx, JSValueConst this_obj,
                                   JSValue prop)
{
    JSAtom atom;
    JSValue ret;

    if (likely(JS_VALUE_GET_TAG(this_obj) == JS_TAG_OBJECT &&
               JS_VALUE_GET_TAG(prop) == JS_TAG_INT)) {
        JSObject *p;
        uint32_t idx;
        /* fast path for array access */
        p = JS_VALUE_GET_OBJ(this_obj);
        idx = JS_VALUE_GET_INT(prop);
        switch(p->class_id) {
        case JS_CLASS_ARRAY:
        case JS_CLASS_ARGUMENTS:
            if (unlikely(idx >= p->u.array.count)) goto slow_path;
            return JS_DupValue(ctx, p->u.array.u.values[idx]);
        case JS_CLASS_MAPPED_ARGUMENTS:
            if (unlikely(idx >= p->u.array.count)) goto slow_path;
            return JS_DupValue(ctx, *p->u.array.u.var_refs[idx]->pvalue);
        case JS_CLASS_INT8_ARRAY:
            if (unlikely(idx >= p->u.array.count)) goto slow_path;
            return JS_NewInt32(ctx, p->u.array.u.int8_ptr[idx]);
        case JS_CLASS_UINT8C_ARRAY:
        case JS_CLASS_UINT8_ARRAY:
            if (unlikely(idx >= p->u.array.count)) goto slow_path;
            return JS_NewInt32(ctx, p->u.array.u.uint8_ptr[idx]);
        case JS_CLASS_INT16_ARRAY:
            if (unlikely(idx >= p->u.array.count)) goto slow_path;
            return JS_NewInt32(ctx, p->u.array.u.int16_ptr[idx]);
        case JS_CLASS_UINT16_ARRAY:
            if (unlikely(idx >= p->u.array.count)) goto slow_path;
            return JS_NewInt32(ctx, p->u.array.u.uint16_ptr[idx]);
        case JS_CLASS_INT32_ARRAY:
            if (unlikely(idx >= p->u.array.count)) goto slow_path;
            return JS_NewInt32(ctx, p->u.array.u.int32_ptr[idx]);
        case JS_CLASS_UINT32_ARRAY:
            if (unlikely(idx >= p->u.array.count)) goto slow_path;
            return JS_NewUint32(ctx, p->u.array.u.uint32_ptr[idx]);
        case JS_CLASS_BIG_INT64_ARRAY:
            if (unlikely(idx >= p->u.array.count)) goto slow_path;
            return JS_NewBigInt64(ctx, p->u.array.u.int64_ptr[idx]);
        case JS_CLASS_BIG_UINT64_ARRAY:
            if (unlikely(idx >= p->u.array.count)) goto slow_path;
            return JS_NewBigUint64(ctx, p->u.array.u.uint64_ptr[idx]);
        case JS_CLASS_FLOAT16_ARRAY:
            if (unlikely(idx >= p->u.array.count)) goto slow_path;
            return __JS_NewFloat64(ctx, fromfp16(p->u.array.u.fp16_ptr[idx]));
        case JS_CLASS_FLOAT32_ARRAY:
            if (unlikely(idx >= p->u.array.count)) goto slow_path;
            return __JS_NewFloat64(ctx, p->u.array.u.float_ptr[idx]);
        case JS_CLASS_FLOAT64_ARRAY:
            if (unlikely(idx >= p->u.array.count)) goto slow_path;
            return __JS_NewFloat64(ctx, p->u.array.u.double_ptr[idx]);
        default:
            goto slow_path;
        }
    } else {
    slow_path:
        /* ToObject() must be done before ToPropertyKey() */
        if (JS_IsNull(this_obj) || JS_IsUndefined(this_obj)) {
            JS_FreeValue(ctx, prop);
            return JS_ThrowTypeError(ctx, "cannot read property of %s", JS_IsNull(this_obj) ? "null" : "undefined");
        }
        atom = JS_ValueToAtom(ctx, prop);
        JS_FreeValue(ctx, prop);
        if (unlikely(atom == JS_ATOM_NULL))
            return JS_EXCEPTION;
        ret = JS_GetProperty(ctx, this_obj, atom);
        JS_FreeAtom(ctx, atom);
        return ret;
    }
}

JSValue JS_GetPropertyUint32(JSContext *ctx, JSValueConst this_obj,
                             uint32_t idx)
{
    return JS_GetPropertyValue(ctx, this_obj, JS_NewUint32(ctx, idx));
}

/* Check if an object has a generalized numeric property. Return value:
   -1 for exception,
   TRUE if property exists, stored into *pval,
   FALSE if proprty does not exist.
 */
static int JS_TryGetPropertyInt64(JSContext *ctx, JSValueConst obj, int64_t idx, JSValue *pval)
{
    JSValue val = JS_UNDEFINED;
    JSAtom prop;
    int present;

    if (likely((uint64_t)idx <= JS_ATOM_MAX_INT)) {
        /* fast path */
        present = JS_HasProperty(ctx, obj, __JS_AtomFromUInt32(idx));
        if (present > 0) {
            val = JS_GetPropertyValue(ctx, obj, JS_NewInt32(ctx, idx));
            if (unlikely(JS_IsException(val)))
                present = -1;
        }
    } else {
        prop = JS_NewAtomInt64(ctx, idx);
        present = -1;
        if (likely(prop != JS_ATOM_NULL)) {
            present = JS_HasProperty(ctx, obj, prop);
            if (present > 0) {
                val = JS_GetProperty(ctx, obj, prop);
                if (unlikely(JS_IsException(val)))
                    present = -1;
            }
            JS_FreeAtom(ctx, prop);
        }
    }
    *pval = val;
    return present;
}

static JSValue JS_GetPropertyInt64(JSContext *ctx, JSValueConst obj, int64_t idx)
{
    JSAtom prop;
    JSValue val;

    if ((uint64_t)idx <= INT32_MAX) {
        /* fast path for fast arrays */
        return JS_GetPropertyValue(ctx, obj, JS_NewInt32(ctx, idx));
    }
    prop = JS_NewAtomInt64(ctx, idx);
    if (prop == JS_ATOM_NULL)
        return JS_EXCEPTION;

    val = JS_GetProperty(ctx, obj, prop);
    JS_FreeAtom(ctx, prop);
    return val;
}

JSValue JS_GetPropertyStr(JSContext *ctx, JSValueConst this_obj,
                          const char *prop)
{
    JSAtom atom;
    JSValue ret;
    atom = JS_NewAtom(ctx, prop);
    if (atom == JS_ATOM_NULL)
        return JS_EXCEPTION;
    ret = JS_GetProperty(ctx, this_obj, atom);
    JS_FreeAtom(ctx, atom);
    return ret;
}

/* Note: the property value is not initialized. Return NULL if memory
   error. */
static JSProperty *add_property(JSContext *ctx,
                                JSObject *p, JSAtom prop, int prop_flags)
{
    JSShape *sh, *new_sh;

    if (unlikely(__JS_AtomIsTaggedInt(prop))) {
        /* update is_std_array_prototype */
        if (unlikely(p->is_std_array_prototype)) {
            p->is_std_array_prototype = FALSE;
        } else if (unlikely(p->has_immutable_prototype)) {
            struct list_head *el;

            /* modifying Object.prototype : reset the corresponding is_std_array_prototype */
            list_for_each(el, &ctx->rt->context_list) {
                JSContext *ctx1 = list_entry(el, JSContext, link);
                if (JS_IsObject(ctx1->class_proto[JS_CLASS_OBJECT]) &&
                    JS_VALUE_GET_OBJ(ctx1->class_proto[JS_CLASS_OBJECT]) == p) {
                    if (JS_IsObject(ctx1->class_proto[JS_CLASS_ARRAY])) {
                        JSObject *p1 = JS_VALUE_GET_OBJ(ctx1->class_proto[JS_CLASS_ARRAY]);
                        p1->is_std_array_prototype = FALSE;
                    }
                    break;
                }
            }
        }
    }
    sh = p->shape;
    if (sh->is_hashed) {
        /* try to find an existing shape */
        new_sh = find_hashed_shape_prop(ctx->rt, sh, prop, prop_flags);
        if (new_sh) {
            /* matching shape found: use it */
            /*  the property array may need to be resized */
            if (new_sh->prop_size != sh->prop_size) {
                JSProperty *new_prop;
                new_prop = js_realloc(ctx, p->prop, sizeof(p->prop[0]) *
                                      new_sh->prop_size);
                if (!new_prop)
                    return NULL;
                p->prop = new_prop;
            }
            p->shape = js_dup_shape(new_sh);
            js_free_shape(ctx->rt, sh);
            return &p->prop[new_sh->prop_count - 1];
        } else if (js_rc(sh)->ref_count != 1) {
            /* if the shape is shared, clone it */
            new_sh = js_clone_shape(ctx, sh);
            if (!new_sh)
                return NULL;
            /* hash the cloned shape */
            new_sh->is_hashed = TRUE;
            js_shape_hash_link(ctx->rt, new_sh);
            js_free_shape(ctx->rt, p->shape);
            p->shape = new_sh;
        }
    }
    assert(js_rc(p->shape)->ref_count == 1);
    if (add_shape_property(ctx, &p->shape, p, prop, prop_flags))
        return NULL;
    return &p->prop[p->shape->prop_count - 1];
}

/* can be called on JS_CLASS_ARRAY, JS_CLASS_ARGUMENTS or
   JS_CLASS_MAPPED_ARGUMENTS objects. return < 0 if memory alloc
   error. */
static no_inline __exception int convert_fast_array_to_array(JSContext *ctx,
                                                             JSObject *p)
{
    JSProperty *pr;
    JSShape *sh;
    uint32_t i, len, new_count;

    if (js_shape_prepare_update(ctx, p, NULL))
        return -1;
    len = p->u.array.count;
    /* resize the properties once to simplify the error handling */
    sh = p->shape;
    new_count = sh->prop_count + len;
    if (new_count > sh->prop_size) {
        if (resize_properties(ctx, &p->shape, p, new_count))
            return -1;
    }

    if (p->class_id == JS_CLASS_MAPPED_ARGUMENTS) {
        JSVarRef **tab = p->u.array.u.var_refs;
        for(i = 0; i < len; i++) {
            /* add_property cannot fail here but
               __JS_AtomFromUInt32(i) fails for i > INT32_MAX */
            pr = add_property(ctx, p, __JS_AtomFromUInt32(i), JS_PROP_C_W_E | JS_PROP_VARREF);
            pr->u.var_ref = *tab++;
        }
    } else {
        JSValue *tab = p->u.array.u.values;
        for(i = 0; i < len; i++) {
            /* add_property cannot fail here but
               __JS_AtomFromUInt32(i) fails for i > INT32_MAX */
            pr = add_property(ctx, p, __JS_AtomFromUInt32(i), JS_PROP_C_W_E);
            pr->u.value = *tab++;
        }
    }
    js_free(ctx, p->u.array.u.values);
    p->u.array.count = 0;
    p->u.array.u.values = NULL; /* fail safe */
    p->u.array.u1.size = 0;
    p->fast_array = 0;
    p->is_std_array_prototype = FALSE;
    return 0;
}

static int remove_global_object_property(JSContext *ctx, JSObject *p,
                                         JSShapeProperty *prs, JSProperty *pr)
{
    JSVarRef *var_ref;
    JSObject *p1;
    JSProperty *pr1;

    var_ref = pr->u.var_ref;
    if (js_rc(var_ref)->ref_count == 1)
        return 0;
    p1 = JS_VALUE_GET_OBJ(p->u.global_object.uninitialized_vars);
    pr1 = add_property(ctx, p1, prs->atom, JS_PROP_C_W_E | JS_PROP_VARREF);
    if (!pr1)
        return -1;
    pr1->u.var_ref = var_ref;
    js_rc(var_ref)->ref_count++;
    JS_FreeValue(ctx, var_ref->value);
    var_ref->is_lexical = FALSE;
    var_ref->is_const = FALSE;
    var_ref->value = JS_UNINITIALIZED;
    return 0;
}

static int delete_property(JSContext *ctx, JSObject *p, JSAtom atom)
{
    JSShape *sh;
    JSShapeProperty *pr, *lpr, *prop;
    JSProperty *pr1;
    uint32_t lpr_idx;
    intptr_t h, h1;

 redo:
    sh = p->shape;
    h1 = atom & sh->prop_hash_mask;
    h = sh->hash_table[h1];
    prop = get_shape_prop(sh);
    lpr = NULL;
    lpr_idx = 0;   /* prevent warning */
    while (h != 0) {
        pr = &prop[h - 1];
        if (likely(pr->atom == atom)) {
            /* found ! */
            if (!(pr->flags & JS_PROP_CONFIGURABLE))
                return FALSE;
            /* realloc the shape if needed */
            if (lpr)
                lpr_idx = lpr - get_shape_prop(sh);
            if (js_shape_prepare_update(ctx, p, &pr))
                return -1;
            sh = p->shape;
            /* remove property */
            if (lpr) {
                lpr = get_shape_prop(sh) + lpr_idx;
                lpr->hash_next = pr->hash_next;
            } else {
                sh->hash_table[h1] = pr->hash_next;
            }
            sh->deleted_prop_count++;
            /* free the entry */
            pr1 = &p->prop[h - 1];
            if (unlikely(p->class_id == JS_CLASS_GLOBAL_OBJECT)) {
                if ((pr->flags & JS_PROP_TMASK) == JS_PROP_VARREF)
                    if (remove_global_object_property(ctx, p, pr, pr1))
                        return -1;
            }
            free_property(ctx->rt, pr1, pr->flags);
            JS_FreeAtom(ctx, pr->atom);
            /* put default values */
            pr->flags = 0;
            pr->atom = JS_ATOM_NULL;
            pr1->u.value = JS_UNDEFINED;

            /* compact the properties if too many deleted properties */
            if (sh->deleted_prop_count >= 8 &&
                sh->deleted_prop_count >= ((unsigned)sh->prop_count / 2)) {
                compact_properties(ctx, p);
            }
            return TRUE;
        }
        lpr = pr;
        h = pr->hash_next;
    }

    if (p->is_exotic) {
        if (p->fast_array) {
            uint32_t idx;
            if (JS_AtomIsArrayIndex(ctx, &idx, atom) &&
                idx < p->u.array.count) {
                if (p->class_id == JS_CLASS_ARRAY ||
                    p->class_id == JS_CLASS_ARGUMENTS ||
                    p->class_id == JS_CLASS_MAPPED_ARGUMENTS) {
                    /* Special case deleting the last element of a fast Array */
                    if (idx == p->u.array.count - 1) {
                        if (p->class_id == JS_CLASS_MAPPED_ARGUMENTS) {
                            free_var_ref(ctx->rt, p->u.array.u.var_refs[idx]);
                        } else {
                            JS_FreeValue(ctx, p->u.array.u.values[idx]);
                        }
                        p->u.array.count = idx;
                        return TRUE;
                    }
                    if (convert_fast_array_to_array(ctx, p))
                        return -1;
                    goto redo;
                } else {
                    return FALSE;
                }
            }
        } else {
            const JSClassExoticMethods *em = ctx->rt->class_array[p->class_id].exotic;
            if (em && em->delete_property) {
                return em->delete_property(ctx, JS_MKPTR(JS_TAG_OBJECT, p), atom);
            }
        }
    }
    /* not found */
    return TRUE;
}

static int call_setter(JSContext *ctx, JSObject *setter,
                       JSValueConst this_obj, JSValue val, int flags)
{
    JSValue ret, func;
    if (likely(setter)) {
        func = JS_MKPTR(JS_TAG_OBJECT, setter);
        /* Note: the field could be removed in the setter */
        func = JS_DupValue(ctx, func);
        ret = JS_CallFree(ctx, func, this_obj, 1, (JSValueConst *)&val);
        JS_FreeValue(ctx, val);
        if (JS_IsException(ret))
            return -1;
        JS_FreeValue(ctx, ret);
        return TRUE;
    } else {
        JS_FreeValue(ctx, val);
        if ((flags & JS_PROP_THROW) ||
            ((flags & JS_PROP_THROW_STRICT) && is_strict_mode(ctx))) {
            JS_ThrowTypeError(ctx, "no setter for property");
            return -1;
        }
        return FALSE;
    }
}

/* set the array length and remove the array elements if necessary. */
static int set_array_length(JSContext *ctx, JSObject *p, JSValue val,
                            int flags)
{
    uint32_t len, idx, cur_len;
    int i, ret;

    /* Note: this call can reallocate the properties of 'p' */
    ret = JS_ToArrayLengthFree(ctx, &len, val, FALSE);
    if (ret)
        return -1;
    /* JS_ToArrayLengthFree() must be done before the read-only test */
    if (unlikely(!(get_shape_prop(p->shape)[0].flags & JS_PROP_WRITABLE)))
        return JS_ThrowTypeErrorReadOnly(ctx, flags, JS_ATOM_length);

    if (likely(p->fast_array)) {
        uint32_t old_len = p->u.array.count;
        if (len < old_len) {
            for(i = len; i < old_len; i++) {
                JS_FreeValue(ctx, p->u.array.u.values[i]);
            }
            p->u.array.count = len;
        }
        p->prop[0].u.value = JS_NewUint32(ctx, len);
    } else {
        /* Note: length is always a uint32 because the object is an
           array */
        JS_ToUint32(ctx, &cur_len, p->prop[0].u.value);
        if (len < cur_len) {
            uint32_t d;
            JSShape *sh;
            JSShapeProperty *pr;

            d = cur_len - len;
            sh = p->shape;
            if (d <= sh->prop_count) {
                JSAtom atom;

                /* faster to iterate */
                while (cur_len > len) {
                    atom = JS_NewAtomUInt32(ctx, cur_len - 1);
                    ret = delete_property(ctx, p, atom);
                    JS_FreeAtom(ctx, atom);
                    if (unlikely(!ret)) {
                        /* unlikely case: property is not
                           configurable */
                        break;
                    }
                    cur_len--;
                }
            } else {
                /* faster to iterate thru all the properties. Need two
                   passes in case one of the property is not
                   configurable */
                cur_len = len;
                for(i = 0, pr = get_shape_prop(sh); i < sh->prop_count;
                    i++, pr++) {
                    if (pr->atom != JS_ATOM_NULL &&
                        JS_AtomIsArrayIndex(ctx, &idx, pr->atom)) {
                        if (idx >= cur_len &&
                            !(pr->flags & JS_PROP_CONFIGURABLE)) {
                            cur_len = idx + 1;
                        }
                    }
                }

                for(i = 0, pr = get_shape_prop(sh); i < sh->prop_count;
                    i++, pr++) {
                    if (pr->atom != JS_ATOM_NULL &&
                        JS_AtomIsArrayIndex(ctx, &idx, pr->atom)) {
                        if (idx >= cur_len) {
                            /* remove the property */
                            delete_property(ctx, p, pr->atom);
                            /* WARNING: the shape may have been modified */
                            sh = p->shape;
                            pr = get_shape_prop(sh) + i;
                        }
                    }
                }
            }
        } else {
            cur_len = len;
        }
        set_value(ctx, &p->prop[0].u.value, JS_NewUint32(ctx, cur_len));
        if (unlikely(cur_len > len)) {
            return JS_ThrowTypeErrorOrFalse(ctx, flags, "not configurable");
        }
    }
    return TRUE;
}

/* return -1 if exception */
static int expand_fast_array(JSContext *ctx, JSObject *p, uint32_t new_len)
{
    uint32_t new_size;
    size_t slack;
    JSValue *new_array_prop;
    /* XXX: potential arithmetic overflow */
    new_size = max_int(new_len, p->u.array.u1.size * 3 / 2);
    new_array_prop = js_realloc2(ctx, p->u.array.u.values, sizeof(JSValue) * new_size, &slack);
    if (!new_array_prop)
        return -1;
    new_size += slack / sizeof(*new_array_prop);
    p->u.array.u.values = new_array_prop;
    p->u.array.u1.size = new_size;
    return 0;
}

/* Preconditions: 'p' must be of class JS_CLASS_ARRAY, p->fast_array =
   TRUE and p->extensible = TRUE */
static inline int add_fast_array_element(JSContext *ctx, JSObject *p,
                                         JSValue val, int flags)
{
    uint32_t new_len, array_len;
    /* extend the array by one */
    /* XXX: convert to slow array if new_len > 2^31-1 elements */
    new_len = p->u.array.count + 1;
    /* update the length if necessary. We assume that if the length is
       not an integer, then if it >= 2^31.  */
    if (likely(JS_VALUE_GET_TAG(p->prop[0].u.value) == JS_TAG_INT)) {
        array_len = JS_VALUE_GET_INT(p->prop[0].u.value);
        if (new_len > array_len) {
            if (unlikely(!(get_shape_prop(p->shape)->flags & JS_PROP_WRITABLE))) {
                JS_FreeValue(ctx, val);
                return JS_ThrowTypeErrorReadOnly(ctx, flags, JS_ATOM_length);
            }
            p->prop[0].u.value = JS_NewInt32(ctx, new_len);
        }
    }
    if (unlikely(new_len > p->u.array.u1.size)) {
        if (expand_fast_array(ctx, p, new_len)) {
            JS_FreeValue(ctx, val);
            return -1;
        }
    }
    p->u.array.u.values[new_len - 1] = val;
    p->u.array.count = new_len;
    return TRUE;
}

/* Allocate a new fast array initialized to JS_UNDEFINED. Its maximum
   size is 2^31-1 elements. For convenience, 'len' is a 64 bit
   integer. */
static JSValue js_allocate_fast_array(JSContext *ctx, int64_t len)
{
    JSValue arr;
    JSObject *p;
    int i;

    if (len > INT32_MAX)
        return JS_ThrowRangeError(ctx, "invalid array length");
    arr = JS_NewArray(ctx);
    if (JS_IsException(arr))
        return arr;
    if (len > 0) {
        p = JS_VALUE_GET_OBJ(arr);
        if (expand_fast_array(ctx, p, len) < 0) {
            JS_FreeValue(ctx, arr);
            return JS_EXCEPTION;
        }
        p->u.array.count = len;
        for(i = 0; i < len; i++)
            p->u.array.u.values[i] = JS_UNDEFINED;
        /* update the 'length' field */
        set_value(ctx, &p->prop[0].u.value, JS_NewInt32(ctx, len));
    }
    return arr;
}

static JSValue js_create_array(JSContext *ctx, int len, JSValueConst *tab)
{
    JSValue obj;
    JSObject *p;
    int i;

    obj = JS_NewArray(ctx);
    if (JS_IsException(obj))
        return JS_EXCEPTION;
    if (len > 0) {
        p = JS_VALUE_GET_OBJ(obj);
        if (expand_fast_array(ctx, p, len) < 0) {
            JS_FreeValue(ctx, obj);
            return JS_EXCEPTION;
        }
        p->u.array.count = len;
        for(i = 0; i < len; i++)
            p->u.array.u.values[i] = JS_DupValue(ctx, tab[i]);
        /* update the 'length' field */
        set_value(ctx, &p->prop[0].u.value, JS_NewInt32(ctx, len));
    }
    return obj;
}

static JSValue js_create_array_free(JSContext *ctx, int len, JSValue *tab)
{
    JSValue obj;
    JSObject *p;
    int i;

    obj = JS_NewArray(ctx);
    if (JS_IsException(obj))
        goto fail;
    if (len > 0) {
        p = JS_VALUE_GET_OBJ(obj);
        if (expand_fast_array(ctx, p, len) < 0) {
            JS_FreeValue(ctx, obj);
        fail:
            for(i = 0; i < len; i++)
                JS_FreeValue(ctx, tab[i]);
            return JS_EXCEPTION;
        }
        p->u.array.count = len;
        for(i = 0; i < len; i++)
            p->u.array.u.values[i] = tab[i];
        /* update the 'length' field */
        set_value(ctx, &p->prop[0].u.value, JS_NewInt32(ctx, len));
    }
    return obj;
}

static void js_free_desc(JSContext *ctx, JSPropertyDescriptor *desc)
{
    JS_FreeValue(ctx, desc->getter);
    JS_FreeValue(ctx, desc->setter);
    JS_FreeValue(ctx, desc->value);
}

/* return -1 in case of exception or TRUE or FALSE. Warning: 'val' is
   freed by the function. 'flags' is a bitmask of JS_PROP_THROW and
   JS_PROP_THROW_STRICT. 'this_obj' is the receiver. If obj !=
   this_obj, then obj must be an object (Reflect.set case). */
int JS_SetPropertyInternal(JSContext *ctx, JSValueConst obj,
                           JSAtom prop, JSValue val, JSValueConst this_obj, int flags)
{
    JSObject *p, *p1;
    JSShapeProperty *prs;
    JSProperty *pr;
    uint32_t tag;
    JSPropertyDescriptor desc;
    int ret;
#if 0
    printf("JS_SetPropertyInternal: "); print_atom(ctx, prop); printf("\n");
#endif
    tag = JS_VALUE_GET_TAG(this_obj);
    if (unlikely(tag != JS_TAG_OBJECT)) {
        if (JS_VALUE_GET_TAG(obj) == JS_TAG_OBJECT) {
            p = NULL;
            p1 = JS_VALUE_GET_OBJ(obj);
            goto prototype_lookup;
        } else {
            switch(tag) {
            case JS_TAG_NULL:
                JS_FreeValue(ctx, val);
                JS_ThrowTypeErrorAtom(ctx, "cannot set property '%s' of null", prop);
                return -1;
            case JS_TAG_UNDEFINED:
                JS_FreeValue(ctx, val);
                JS_ThrowTypeErrorAtom(ctx, "cannot set property '%s' of undefined", prop);
                return -1;
            default:
                /* even on a primitive type we can have setters on the prototype */
                p = NULL;
                p1 = JS_VALUE_GET_OBJ(JS_GetPrototypePrimitive(ctx, obj));
                goto prototype_lookup;
            }
        }
    } else {
        p = JS_VALUE_GET_OBJ(this_obj);
        p1 = JS_VALUE_GET_OBJ(obj);
        if (unlikely(p != p1))
            goto retry2;
    }

    /* fast path if obj == this_obj */
 retry:
    prs = find_own_property(&pr, p1, prop);
    if (prs) {
        if (likely((prs->flags & (JS_PROP_TMASK | JS_PROP_WRITABLE |
                                  JS_PROP_LENGTH)) == JS_PROP_WRITABLE)) {
            /* fast case */
            set_value(ctx, &pr->u.value, val);
            return TRUE;
        } else if (prs->flags & JS_PROP_LENGTH) {
            assert(p->class_id == JS_CLASS_ARRAY);
            assert(prop == JS_ATOM_length);
            return set_array_length(ctx, p, val, flags);
        } else if ((prs->flags & JS_PROP_TMASK) == JS_PROP_GETSET) {
            return call_setter(ctx, pr->u.getset.setter, this_obj, val, flags);
        } else if ((prs->flags & JS_PROP_TMASK) == JS_PROP_VARREF) {
            /* XXX: already use var_ref->is_const. Cannot simplify use the
               writable flag for JS_CLASS_MODULE_NS. */
            if (p->class_id == JS_CLASS_MODULE_NS || pr->u.var_ref->is_const)
                goto read_only_prop;
            set_value(ctx, pr->u.var_ref->pvalue, val);
            return TRUE;
        } else if ((prs->flags & JS_PROP_TMASK) == JS_PROP_AUTOINIT) {
            /* Instantiate property and retry (potentially useless) */
            if (JS_AutoInitProperty(ctx, p, prop, pr, prs)) {
                JS_FreeValue(ctx, val);
                return -1;
            }
            goto retry;
        } else {
            goto read_only_prop;
        }
    }

    for(;;) {
        if (p1->is_exotic) {
            if (p1->fast_array) {
                if (__JS_AtomIsTaggedInt(prop)) {
                    uint32_t idx = __JS_AtomToUInt32(prop);
                    if (idx < p1->u.array.count) {
                        if (unlikely(p == p1))
                            return JS_SetPropertyValue(ctx, this_obj, JS_NewInt32(ctx, idx), val, flags);
                        else
                            break;
                    } else if (p1->class_id >= JS_CLASS_UINT8C_ARRAY &&
                               p1->class_id <= JS_CLASS_FLOAT64_ARRAY) {
                        goto typed_array_oob;
                    }
                } else if (p1->class_id >= JS_CLASS_UINT8C_ARRAY &&
                           p1->class_id <= JS_CLASS_FLOAT64_ARRAY) {
                    ret = JS_AtomIsNumericIndex(ctx, prop);
                    if (ret != 0) {
                        if (ret < 0) {
                            JS_FreeValue(ctx, val);
                            return -1;
                        }
                    typed_array_oob:
                        if (p == p1) {
                            /* must convert the argument even if out of bound access */
                            if (p1->class_id == JS_CLASS_BIG_INT64_ARRAY ||
                                p1->class_id == JS_CLASS_BIG_UINT64_ARRAY) {
                                int64_t v;
                                if (JS_ToBigInt64Free(ctx, &v, val))
                                    return -1;
                            } else {
                                val = JS_ToNumberFree(ctx, val);
                                JS_FreeValue(ctx, val);
                                if (JS_IsException(val))
                                    return -1;
                            }
                        } else {
                            JS_FreeValue(ctx, val);
                        }
                        return TRUE;
                    }
                }
            } else {
                const JSClassExoticMethods *em = ctx->rt->class_array[p1->class_id].exotic;
                if (em) {
                    JSValue obj1;
                    if (em->set_property) {
                        /* set_property can free the prototype */
                        obj1 = JS_DupValue(ctx, JS_MKPTR(JS_TAG_OBJECT, p1));
                        ret = em->set_property(ctx, obj1, prop,
                                               val, this_obj, flags);
                        JS_FreeValue(ctx, obj1);
                        JS_FreeValue(ctx, val);
                        return ret;
                    }
                    if (em->get_own_property) {
                        /* get_own_property can free the prototype */
                        obj1 = JS_DupValue(ctx, JS_MKPTR(JS_TAG_OBJECT, p1));
                        ret = em->get_own_property(ctx, &desc,
                                                   obj1, prop);
                        JS_FreeValue(ctx, obj1);
                        if (ret < 0) {
                            JS_FreeValue(ctx, val);
                            return ret;
                        }
                        if (ret) {
                            if (desc.flags & JS_PROP_GETSET) {
                                JSObject *setter;
                                if (JS_IsUndefined(desc.setter))
                                    setter = NULL;
                                else
                                    setter = JS_VALUE_GET_OBJ(desc.setter);
                                ret = call_setter(ctx, setter, this_obj, val, flags);
                                JS_FreeValue(ctx, desc.getter);
                                JS_FreeValue(ctx, desc.setter);
                                return ret;
                            } else {
                                JS_FreeValue(ctx, desc.value);
                                if (!(desc.flags & JS_PROP_WRITABLE))
                                    goto read_only_prop;
                                if (likely(p == p1)) {
                                    ret = JS_DefineProperty(ctx, this_obj, prop, val,
                                                            JS_UNDEFINED, JS_UNDEFINED,
                                                            JS_PROP_HAS_VALUE);
                                    JS_FreeValue(ctx, val);
                                    return ret;
                                } else {
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
        p1 = p1->shape->proto;
    prototype_lookup:
        if (!p1)
            break;

    retry2:
        prs = find_own_property(&pr, p1, prop);
        if (prs) {
            if ((prs->flags & JS_PROP_TMASK) == JS_PROP_GETSET) {
                return call_setter(ctx, pr->u.getset.setter, this_obj, val, flags);
            } else if ((prs->flags & JS_PROP_TMASK) == JS_PROP_AUTOINIT) {
                /* Instantiate property and retry (potentially useless) */
                if (JS_AutoInitProperty(ctx, p1, prop, pr, prs))
                    return -1;
                goto retry2;
            } else if (!(prs->flags & JS_PROP_WRITABLE)) {
                goto read_only_prop;
            } else {
                break;
            }
        }
    }

    if (unlikely(!p)) {
        JS_FreeValue(ctx, val);
        return JS_ThrowTypeErrorOrFalse(ctx, flags, "not an object");
    }

    if (unlikely(!p->extensible)) {
        JS_FreeValue(ctx, val);
        return JS_ThrowTypeErrorOrFalse(ctx, flags, "object is not extensible");
    }

    if (likely(p == JS_VALUE_GET_OBJ(obj))) {
        if (p->is_exotic) {
            if (p->class_id == JS_CLASS_ARRAY && p->fast_array &&
                __JS_AtomIsTaggedInt(prop)) {
                uint32_t idx = __JS_AtomToUInt32(prop);
                if (idx == p->u.array.count) {
                    /* fast case */
                    return add_fast_array_element(ctx, p, val, flags);
                } else {
                    goto generic_create_prop;
                }
            } else {
                goto generic_create_prop;
            }
        } else {
            if (unlikely(p->class_id == JS_CLASS_GLOBAL_OBJECT))
                goto generic_create_prop;
            pr = add_property(ctx, p, prop, JS_PROP_C_W_E);
            if (unlikely(!pr)) {
                JS_FreeValue(ctx, val);
                return -1;
            }
            pr->u.value = val;
            return TRUE;
        }
    } else {
        /* generic case: modify the property in this_obj if it already exists */
        ret = JS_GetOwnPropertyInternal(ctx, &desc, p, prop);
        if (ret < 0) {
            JS_FreeValue(ctx, val);
            return ret;
        }
        if (ret) {
            if (desc.flags & JS_PROP_GETSET) {
                JS_FreeValue(ctx, desc.getter);
                JS_FreeValue(ctx, desc.setter);
                JS_FreeValue(ctx, val);
                return JS_ThrowTypeErrorOrFalse(ctx, flags, "setter is forbidden");
            } else {
                JS_FreeValue(ctx, desc.value);
                if (!(desc.flags & JS_PROP_WRITABLE) ||
                    p->class_id == JS_CLASS_MODULE_NS) {
                read_only_prop:
                    JS_FreeValue(ctx, val);
                    return JS_ThrowTypeErrorReadOnly(ctx, flags, prop);
                }
            }
            ret = JS_DefineProperty(ctx, this_obj, prop, val,
                                    JS_UNDEFINED, JS_UNDEFINED,
                                    JS_PROP_HAS_VALUE);
            JS_FreeValue(ctx, val);
            return ret;
        } else {
        generic_create_prop:
            ret = JS_CreateProperty(ctx, p, prop, val, JS_UNDEFINED, JS_UNDEFINED,
                                    flags |
                                    JS_PROP_HAS_VALUE |
                                    JS_PROP_HAS_ENUMERABLE |
                                    JS_PROP_HAS_WRITABLE |
                                    JS_PROP_HAS_CONFIGURABLE |
                                    JS_PROP_C_W_E);
            JS_FreeValue(ctx, val);
            return ret;
        }
    }
}

/* return true if an element can be added to a fast array without further tests */
static force_inline BOOL can_extend_fast_array(JSObject *p)
{
    JSObject *proto;
    if (!p->extensible)
        return FALSE;
    proto = p->shape->proto;
    if (!proto)
        return TRUE;
    return proto->is_std_array_prototype;
}

/* flags can be JS_PROP_THROW or JS_PROP_THROW_STRICT */
static int JS_SetPropertyValue(JSContext *ctx, JSValueConst this_obj,
                               JSValue prop, JSValue val, int flags)
{
    if (likely(JS_VALUE_GET_TAG(this_obj) == JS_TAG_OBJECT &&
               JS_VALUE_GET_TAG(prop) == JS_TAG_INT)) {
        JSObject *p;
        uint32_t idx;
        double d;
        int32_t v;

        /* fast path for array access */
        p = JS_VALUE_GET_OBJ(this_obj);
        idx = JS_VALUE_GET_INT(prop);
        switch(p->class_id) {
        case JS_CLASS_ARRAY:
            if (unlikely(idx >= (uint32_t)p->u.array.count)) {
                /* fast path to add an element to the array */
                if (unlikely(idx != (uint32_t)p->u.array.count ||
                             !p->fast_array ||
                             !can_extend_fast_array(p))) {
                    goto slow_path;
                }
                /* add element */
                return add_fast_array_element(ctx, p, val, flags);
            }
            set_value(ctx, &p->u.array.u.values[idx], val);
            break;
        case JS_CLASS_ARGUMENTS:
            if (unlikely(idx >= (uint32_t)p->u.array.count))
                goto slow_path;
            set_value(ctx, &p->u.array.u.values[idx], val);
            break;
        case JS_CLASS_MAPPED_ARGUMENTS:
            if (unlikely(idx >= (uint32_t)p->u.array.count))
                goto slow_path;
            set_value(ctx, p->u.array.u.var_refs[idx]->pvalue, val);
            break;
        case JS_CLASS_UINT8C_ARRAY:
            if (JS_ToUint8ClampFree(ctx, &v, val))
                return -1;
            /* Note: the conversion can detach the typed array, so the
               array bound check must be done after */
            if (unlikely(idx >= (uint32_t)p->u.array.count))
                goto ta_out_of_bound;
            p->u.array.u.uint8_ptr[idx] = v;
            break;
        case JS_CLASS_INT8_ARRAY:
        case JS_CLASS_UINT8_ARRAY:
            if (JS_ToInt32Free(ctx, &v, val))
                return -1;
            if (unlikely(idx >= (uint32_t)p->u.array.count))
                goto ta_out_of_bound;
            p->u.array.u.uint8_ptr[idx] = v;
            break;
        case JS_CLASS_INT16_ARRAY:
        case JS_CLASS_UINT16_ARRAY:
            if (JS_ToInt32Free(ctx, &v, val))
                return -1;
            if (unlikely(idx >= (uint32_t)p->u.array.count))
                goto ta_out_of_bound;
            p->u.array.u.uint16_ptr[idx] = v;
            break;
        case JS_CLASS_INT32_ARRAY:
        case JS_CLASS_UINT32_ARRAY:
            if (JS_ToInt32Free(ctx, &v, val))
                return -1;
            if (unlikely(idx >= (uint32_t)p->u.array.count))
                goto ta_out_of_bound;
            p->u.array.u.uint32_ptr[idx] = v;
            break;
        case JS_CLASS_BIG_INT64_ARRAY:
        case JS_CLASS_BIG_UINT64_ARRAY:
            /* XXX: need specific conversion function */
            {
                int64_t v;
                if (JS_ToBigInt64Free(ctx, &v, val))
                    return -1;
                if (unlikely(idx >= (uint32_t)p->u.array.count))
                    goto ta_out_of_bound;
                p->u.array.u.uint64_ptr[idx] = v;
            }
            break;
        case JS_CLASS_FLOAT16_ARRAY:
            if (JS_ToFloat64Free(ctx, &d, val))
                return -1;
            if (unlikely(idx >= (uint32_t)p->u.array.count))
                goto ta_out_of_bound;
            p->u.array.u.fp16_ptr[idx] = tofp16(d);
            break;
        case JS_CLASS_FLOAT32_ARRAY:
            if (JS_ToFloat64Free(ctx, &d, val))
                return -1;
            if (unlikely(idx >= (uint32_t)p->u.array.count))
                goto ta_out_of_bound;
            p->u.array.u.float_ptr[idx] = d;
            break;
        case JS_CLASS_FLOAT64_ARRAY:
            if (JS_ToFloat64Free(ctx, &d, val))
                return -1;
            if (unlikely(idx >= (uint32_t)p->u.array.count)) {
            ta_out_of_bound:
                return TRUE;
            }
            p->u.array.u.double_ptr[idx] = d;
            break;
        default:
            goto slow_path;
        }
        return TRUE;
    } else {
        JSAtom atom;
        int ret;
    slow_path:
        atom = JS_ValueToAtom(ctx, prop);
        JS_FreeValue(ctx, prop);
        if (unlikely(atom == JS_ATOM_NULL)) {
            JS_FreeValue(ctx, val);
            return -1;
        }
        ret = JS_SetPropertyInternal(ctx, this_obj, atom, val, this_obj, flags);
        JS_FreeAtom(ctx, atom);
        return ret;
    }
}

int JS_SetPropertyUint32(JSContext *ctx, JSValueConst this_obj,
                         uint32_t idx, JSValue val)
{
    return JS_SetPropertyValue(ctx, this_obj, JS_NewUint32(ctx, idx), val,
                               JS_PROP_THROW);
}

int JS_SetPropertyInt64(JSContext *ctx, JSValueConst this_obj,
                        int64_t idx, JSValue val)
{
    JSAtom prop;
    int res;

    if ((uint64_t)idx <= INT32_MAX) {
        /* fast path for fast arrays */
        return JS_SetPropertyValue(ctx, this_obj, JS_NewInt32(ctx, idx), val,
                                   JS_PROP_THROW);
    }
    prop = JS_NewAtomInt64(ctx, idx);
    if (prop == JS_ATOM_NULL) {
        JS_FreeValue(ctx, val);
        return -1;
    }
    res = JS_SetProperty(ctx, this_obj, prop, val);
    JS_FreeAtom(ctx, prop);
    return res;
}

int JS_SetPropertyStr(JSContext *ctx, JSValueConst this_obj,
                      const char *prop, JSValue val)
{
    JSAtom atom;
    int ret;
    atom = JS_NewAtom(ctx, prop);
    if (atom == JS_ATOM_NULL) {
        JS_FreeValue(ctx, val);
        return -1;
    }
    ret = JS_SetPropertyInternal(ctx, this_obj, atom, val, this_obj, JS_PROP_THROW);
    JS_FreeAtom(ctx, atom);
    return ret;
}

/* compute the property flags. For each flag: (JS_PROP_HAS_x forces
   it, otherwise def_flags is used)
   Note: makes assumption about the bit pattern of the flags
*/
static int get_prop_flags(int flags, int def_flags)
{
    int mask;
    mask = (flags >> JS_PROP_HAS_SHIFT) & JS_PROP_C_W_E;
    return (flags & mask) | (def_flags & ~mask);
}

static int JS_CreateProperty(JSContext *ctx, JSObject *p,
                             JSAtom prop, JSValueConst val,
                             JSValueConst getter, JSValueConst setter,
                             int flags)
{
    JSProperty *pr;
    int ret, prop_flags;
    JSVarRef *var_ref;
    JSObject *delete_obj;

    /* add a new property or modify an existing exotic one */
    if (p->is_exotic) {
        if (p->class_id == JS_CLASS_ARRAY) {
            uint32_t idx, len;

            if (p->fast_array) {
                if (__JS_AtomIsTaggedInt(prop)) {
                    idx = __JS_AtomToUInt32(prop);
                    if (idx == p->u.array.count) {
                        if (!p->extensible)
                            goto not_extensible;
                        if (flags & (JS_PROP_HAS_GET | JS_PROP_HAS_SET))
                            goto convert_to_array;
                        prop_flags = get_prop_flags(flags, 0);
                        if (prop_flags != JS_PROP_C_W_E)
                            goto convert_to_array;
                        return add_fast_array_element(ctx, p,
                                                      JS_DupValue(ctx, val), flags);
                    } else {
                        goto convert_to_array;
                    }
                } else if (JS_AtomIsArrayIndex(ctx, &idx, prop)) {
                    /* convert the fast array to normal array */
                convert_to_array:
                    if (convert_fast_array_to_array(ctx, p))
                        return -1;
                    goto generic_array;
                }
            } else if (JS_AtomIsArrayIndex(ctx, &idx, prop)) {
                JSProperty *plen;
                JSShapeProperty *pslen;
            generic_array:
                /* update the length field */
                plen = &p->prop[0];
                JS_ToUint32(ctx, &len, plen->u.value);
                if ((idx + 1) > len) {
                    pslen = get_shape_prop(p->shape);
                    if (unlikely(!(pslen->flags & JS_PROP_WRITABLE)))
                        return JS_ThrowTypeErrorReadOnly(ctx, flags, JS_ATOM_length);
                    /* XXX: should update the length after defining
                       the property */
                    len = idx + 1;
                    set_value(ctx, &plen->u.value, JS_NewUint32(ctx, len));
                }
            }
        } else if (p->class_id >= JS_CLASS_UINT8C_ARRAY &&
                   p->class_id <= JS_CLASS_FLOAT64_ARRAY) {
            ret = JS_AtomIsNumericIndex(ctx, prop);
            if (ret != 0) {
                if (ret < 0)
                    return -1;
                return JS_ThrowTypeErrorOrFalse(ctx, flags, "cannot create numeric index in typed array");
            }
        } else if (!(flags & JS_PROP_NO_EXOTIC)) {
            const JSClassExoticMethods *em = ctx->rt->class_array[p->class_id].exotic;
            if (em) {
                if (em->define_own_property) {
                    return em->define_own_property(ctx, JS_MKPTR(JS_TAG_OBJECT, p),
                                                   prop, val, getter, setter, flags);
                }
                ret = JS_IsExtensible(ctx, JS_MKPTR(JS_TAG_OBJECT, p));
                if (ret < 0)
                    return -1;
                if (!ret)
                    goto not_extensible;
            }
        }
    }

    if (!p->extensible) {
    not_extensible:
        return JS_ThrowTypeErrorOrFalse(ctx, flags, "object is not extensible");
    }

    var_ref = NULL;
    delete_obj = NULL;
    if (flags & (JS_PROP_HAS_GET | JS_PROP_HAS_SET)) {
        prop_flags = (flags & (JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE)) |
            JS_PROP_GETSET;
    } else {
        prop_flags = flags & JS_PROP_C_W_E;
        if (p->class_id == JS_CLASS_GLOBAL_OBJECT) {
            JSObject *p1 = JS_VALUE_GET_OBJ(p->u.global_object.uninitialized_vars);
            JSShapeProperty *prs1;
            JSProperty *pr1;
            prs1 = find_own_property(&pr1, p1, prop);
            if (prs1) {
                delete_obj = p1;
                var_ref = pr1->u.var_ref;
                js_rc(var_ref)->ref_count++;
            } else {
                var_ref = js_create_var_ref(ctx, FALSE);
                if (!var_ref)
                    return -1;
            }
            var_ref->is_const = !(prop_flags & JS_PROP_WRITABLE);
            prop_flags |= JS_PROP_VARREF;
        }
    }
    pr = add_property(ctx, p, prop, prop_flags);
    if (unlikely(!pr)) {
        if (var_ref)
            free_var_ref(ctx->rt, var_ref);
        return -1;
    }
    if (flags & (JS_PROP_HAS_GET | JS_PROP_HAS_SET)) {
        pr->u.getset.getter = NULL;
        if ((flags & JS_PROP_HAS_GET) && JS_IsFunction(ctx, getter)) {
            pr->u.getset.getter =
                JS_VALUE_GET_OBJ(JS_DupValue(ctx, getter));
        }
        pr->u.getset.setter = NULL;
        if ((flags & JS_PROP_HAS_SET) && JS_IsFunction(ctx, setter)) {
            pr->u.getset.setter =
                JS_VALUE_GET_OBJ(JS_DupValue(ctx, setter));
        }
    } else if (p->class_id == JS_CLASS_GLOBAL_OBJECT) {
        if (delete_obj)
            delete_property(ctx, delete_obj, prop);
        pr->u.var_ref = var_ref;
        if (flags & JS_PROP_HAS_VALUE) {
            *var_ref->pvalue = JS_DupValue(ctx, val);
        } else {
            *var_ref->pvalue = JS_UNDEFINED;
        }
    } else {
        if (flags & JS_PROP_HAS_VALUE) {
            pr->u.value = JS_DupValue(ctx, val);
        } else {
            pr->u.value = JS_UNDEFINED;
        }
    }
    return TRUE;
}

/* return FALSE if not OK */
static BOOL check_define_prop_flags(int prop_flags, int flags)
{
    BOOL has_accessor, is_getset;

    if (!(prop_flags & JS_PROP_CONFIGURABLE)) {
        if ((flags & (JS_PROP_HAS_CONFIGURABLE | JS_PROP_CONFIGURABLE)) ==
            (JS_PROP_HAS_CONFIGURABLE | JS_PROP_CONFIGURABLE)) {
            return FALSE;
        }
        if ((flags & JS_PROP_HAS_ENUMERABLE) &&
            (flags & JS_PROP_ENUMERABLE) != (prop_flags & JS_PROP_ENUMERABLE))
            return FALSE;
        if (flags & (JS_PROP_HAS_VALUE | JS_PROP_HAS_WRITABLE |
                     JS_PROP_HAS_GET | JS_PROP_HAS_SET)) {
            has_accessor = ((flags & (JS_PROP_HAS_GET | JS_PROP_HAS_SET)) != 0);
            is_getset = ((prop_flags & JS_PROP_TMASK) == JS_PROP_GETSET);
            if (has_accessor != is_getset)
                return FALSE;
            if (!is_getset && !(prop_flags & JS_PROP_WRITABLE)) {
                /* not writable: cannot set the writable bit */
                if ((flags & (JS_PROP_HAS_WRITABLE | JS_PROP_WRITABLE)) ==
                    (JS_PROP_HAS_WRITABLE | JS_PROP_WRITABLE))
                    return FALSE;
            }
        }
    }
    return TRUE;
}

/* ensure that the shape can be safely modified */
static int js_shape_prepare_update(JSContext *ctx, JSObject *p,
                                   JSShapeProperty **pprs)
{
    JSShape *sh;
    uint32_t idx = 0;    /* prevent warning */

    sh = p->shape;
    if (sh->is_hashed) {
        if (js_rc(sh)->ref_count != 1) {
            if (pprs)
                idx = *pprs - get_shape_prop(sh);
            /* clone the shape (the resulting one is no longer hashed) */
            sh = js_clone_shape(ctx, sh);
            if (!sh)
                return -1;
            js_free_shape(ctx->rt, p->shape);
            p->shape = sh;
            if (pprs)
                *pprs = get_shape_prop(sh) + idx;
        } else {
            js_shape_hash_unlink(ctx->rt, sh);
            sh->is_hashed = FALSE;
        }
    }
    return 0;
}

static int js_update_property_flags(JSContext *ctx, JSObject *p,
                                    JSShapeProperty **pprs, int flags)
{
    if (flags != (*pprs)->flags) {
        if (js_shape_prepare_update(ctx, p, pprs))
            return -1;
        (*pprs)->flags = flags;
    }
    return 0;
}

/* allowed flags:
   JS_PROP_CONFIGURABLE, JS_PROP_WRITABLE, JS_PROP_ENUMERABLE
   JS_PROP_HAS_GET, JS_PROP_HAS_SET, JS_PROP_HAS_VALUE,
   JS_PROP_HAS_CONFIGURABLE, JS_PROP_HAS_WRITABLE, JS_PROP_HAS_ENUMERABLE,
   JS_PROP_THROW, JS_PROP_NO_EXOTIC.
   If JS_PROP_THROW is set, return an exception instead of FALSE.
   if JS_PROP_NO_EXOTIC is set, do not call the exotic
   define_own_property callback.
   return -1 (exception), FALSE or TRUE.
*/
int JS_DefineProperty(JSContext *ctx, JSValueConst this_obj,
                      JSAtom prop, JSValueConst val,
                      JSValueConst getter, JSValueConst setter, int flags)
{
    JSObject *p;
    JSShapeProperty *prs;
    JSProperty *pr;
    int mask, res;

    if (JS_VALUE_GET_TAG(this_obj) != JS_TAG_OBJECT) {
        JS_ThrowTypeErrorNotAnObject(ctx);
        return -1;
    }
    p = JS_VALUE_GET_OBJ(this_obj);

 redo_prop_update:
    prs = find_own_property(&pr, p, prop);
    if (prs) {
        /* the range of the Array length property is always tested before */
        if ((prs->flags & JS_PROP_LENGTH) && (flags & JS_PROP_HAS_VALUE)) {
            uint32_t array_length;
            if (JS_ToArrayLengthFree(ctx, &array_length,
                                     JS_DupValue(ctx, val), FALSE)) {
                return -1;
            }
            /* this code relies on the fact that Uint32 are never allocated */
            val = (JSValueConst)JS_NewUint32(ctx, array_length);
            /* prs may have been modified */
            prs = find_own_property(&pr, p, prop);
            assert(prs != NULL);
        }
        /* property already exists */
        if (!check_define_prop_flags(prs->flags, flags)) {
        not_configurable:
            return JS_ThrowTypeErrorOrFalse(ctx, flags, "property is not configurable");
        }

        if ((prs->flags & JS_PROP_TMASK) == JS_PROP_AUTOINIT) {
            /* Instantiate property and retry */
            if (JS_AutoInitProperty(ctx, p, prop, pr, prs))
                return -1;
            goto redo_prop_update;
        }

        if (flags & (JS_PROP_HAS_VALUE | JS_PROP_HAS_WRITABLE |
                     JS_PROP_HAS_GET | JS_PROP_HAS_SET)) {
            if (flags & (JS_PROP_HAS_GET | JS_PROP_HAS_SET)) {
                JSObject *new_getter, *new_setter;

                if (JS_IsFunction(ctx, getter)) {
                    new_getter = JS_VALUE_GET_OBJ(getter);
                } else {
                    new_getter = NULL;
                }
                if (JS_IsFunction(ctx, setter)) {
                    new_setter = JS_VALUE_GET_OBJ(setter);
                } else {
                    new_setter = NULL;
                }

                if ((prs->flags & JS_PROP_TMASK) != JS_PROP_GETSET) {
                    if (js_shape_prepare_update(ctx, p, &prs))
                        return -1;
                    /* convert to getset */
                    if ((prs->flags & JS_PROP_TMASK) == JS_PROP_VARREF) {
                        if (unlikely(p->class_id == JS_CLASS_GLOBAL_OBJECT)) {
                            if (remove_global_object_property(ctx, p, prs, pr))
                                return -1;
                        }
                        free_var_ref(ctx->rt, pr->u.var_ref);
                    } else {
                        JS_FreeValue(ctx, pr->u.value);
                    }
                    prs->flags = (prs->flags &
                                  (JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE)) |
                        JS_PROP_GETSET;
                    pr->u.getset.getter = NULL;
                    pr->u.getset.setter = NULL;
                } else {
                    if (!(prs->flags & JS_PROP_CONFIGURABLE)) {
                        if ((flags & JS_PROP_HAS_GET) &&
                            new_getter != pr->u.getset.getter) {
                            goto not_configurable;
                        }
                        if ((flags & JS_PROP_HAS_SET) &&
                            new_setter != pr->u.getset.setter) {
                            goto not_configurable;
                        }
                    }
                }
                if (flags & JS_PROP_HAS_GET) {
                    if (pr->u.getset.getter)
                        JS_FreeValue(ctx, JS_MKPTR(JS_TAG_OBJECT, pr->u.getset.getter));
                    if (new_getter)
                        JS_DupValue(ctx, getter);
                    pr->u.getset.getter = new_getter;
                }
                if (flags & JS_PROP_HAS_SET) {
                    if (pr->u.getset.setter)
                        JS_FreeValue(ctx, JS_MKPTR(JS_TAG_OBJECT, pr->u.getset.setter));
                    if (new_setter)
                        JS_DupValue(ctx, setter);
                    pr->u.getset.setter = new_setter;
                }
            } else {
                if ((prs->flags & JS_PROP_TMASK) == JS_PROP_GETSET) {
                    /* convert to data descriptor */
                    JSVarRef *var_ref;
                    if (unlikely(p->class_id == JS_CLASS_GLOBAL_OBJECT)) {
                        var_ref = js_global_object_find_uninitialized_var(ctx, p, prop, FALSE);
                        if (!var_ref)
                            return -1;
                    } else {
                        var_ref = NULL;
                    }
                    if (js_shape_prepare_update(ctx, p, &prs)) {
                        if (var_ref)
                            free_var_ref(ctx->rt, var_ref);
                        return -1;
                    }
                    if (pr->u.getset.getter)
                        JS_FreeValue(ctx, JS_MKPTR(JS_TAG_OBJECT, pr->u.getset.getter));
                    if (pr->u.getset.setter)
                        JS_FreeValue(ctx, JS_MKPTR(JS_TAG_OBJECT, pr->u.getset.setter));
                    if (var_ref) {
                        prs->flags = (prs->flags & ~JS_PROP_TMASK) |
                            JS_PROP_VARREF | JS_PROP_WRITABLE;
                        pr->u.var_ref = var_ref;
                    } else {
                        prs->flags &= ~(JS_PROP_TMASK | JS_PROP_WRITABLE);
                        pr->u.value = JS_UNDEFINED;
                    }
                } else if ((prs->flags & JS_PROP_TMASK) == JS_PROP_VARREF) {
                    /* Note: JS_PROP_VARREF is always writable */
                } else {
                    if ((prs->flags & (JS_PROP_CONFIGURABLE | JS_PROP_WRITABLE)) == 0 &&
                        (flags & JS_PROP_HAS_VALUE)) {
                        if (!js_same_value(ctx, val, pr->u.value)) {
                            goto not_configurable;
                        } else {
                            return TRUE;
                        }
                    }
                }
                if ((prs->flags & JS_PROP_TMASK) == JS_PROP_VARREF) {
                    if (flags & JS_PROP_HAS_VALUE) {
                        if (p->class_id == JS_CLASS_MODULE_NS) {
                            /* JS_PROP_WRITABLE is always true for variable
                               references, but they are write protected in module name
                               spaces. */
                            if (!js_same_value(ctx, val, *pr->u.var_ref->pvalue))
                                goto not_configurable;
                        } else {
                            /* update the reference */
                            set_value(ctx, pr->u.var_ref->pvalue,
                                      JS_DupValue(ctx, val));
                        }
                    }
                    if ((flags & (JS_PROP_HAS_WRITABLE | JS_PROP_WRITABLE)) == JS_PROP_HAS_WRITABLE) {
                        JSValue val1;
                        if (p->class_id == JS_CLASS_MODULE_NS) {
                            return JS_ThrowTypeErrorOrFalse(ctx, flags, "module namespace properties have writable = false");
                        }
                        if (js_shape_prepare_update(ctx, p, &prs))
                            return -1;
                        if (p->class_id == JS_CLASS_GLOBAL_OBJECT) {
                            pr->u.var_ref->is_const = TRUE; /* mark as read-only */
                            prs->flags &= ~JS_PROP_WRITABLE;
                        } else {
                            /* if writable is set to false, no longer a
                               reference (for mapped arguments) */
                            val1 = JS_DupValue(ctx, *pr->u.var_ref->pvalue);
                            free_var_ref(ctx->rt, pr->u.var_ref);
                            pr->u.value = val1;
                            prs->flags &= ~(JS_PROP_TMASK | JS_PROP_WRITABLE);
                        }
                    }
                } else if (prs->flags & JS_PROP_LENGTH) {
                    if (flags & JS_PROP_HAS_VALUE) {
                        /* Note: no JS code is executable because
                           'val' is guaranted to be a Uint32 */
                        res = set_array_length(ctx, p, JS_DupValue(ctx, val),
                                               flags);
                    } else {
                        res = TRUE;
                    }
                    /* still need to reset the writable flag if
                       needed.  The JS_PROP_LENGTH is kept because the
                       Uint32 test is still done if the length
                       property is read-only. */
                    if ((flags & (JS_PROP_HAS_WRITABLE | JS_PROP_WRITABLE)) ==
                        JS_PROP_HAS_WRITABLE) {
                        prs = get_shape_prop(p->shape);
                        if (js_update_property_flags(ctx, p, &prs,
                                                     prs->flags & ~JS_PROP_WRITABLE))
                            return -1;
                    }
                    return res;
                } else {
                    if (flags & JS_PROP_HAS_VALUE) {
                        JS_FreeValue(ctx, pr->u.value);
                        pr->u.value = JS_DupValue(ctx, val);
                    }
                    if (flags & JS_PROP_HAS_WRITABLE) {
                        if (js_update_property_flags(ctx, p, &prs,
                                                     (prs->flags & ~JS_PROP_WRITABLE) |
                                                     (flags & JS_PROP_WRITABLE)))
                            return -1;
                    }
                }
            }
        }
        mask = 0;
        if (flags & JS_PROP_HAS_CONFIGURABLE)
            mask |= JS_PROP_CONFIGURABLE;
        if (flags & JS_PROP_HAS_ENUMERABLE)
            mask |= JS_PROP_ENUMERABLE;
        if (js_update_property_flags(ctx, p, &prs,
                                     (prs->flags & ~mask) | (flags & mask)))
            return -1;
        return TRUE;
    }

    /* handle modification of fast array elements */
    if (p->fast_array) {
        uint32_t idx;
        uint32_t prop_flags;
        if (p->class_id == JS_CLASS_ARRAY) {
            if (__JS_AtomIsTaggedInt(prop)) {
                idx = __JS_AtomToUInt32(prop);
                if (idx < p->u.array.count) {
                    prop_flags = get_prop_flags(flags, JS_PROP_C_W_E);
                    if (prop_flags != JS_PROP_C_W_E)
                        goto convert_to_slow_array;
                    if (flags & (JS_PROP_HAS_GET | JS_PROP_HAS_SET)) {
                    convert_to_slow_array:
                        if (convert_fast_array_to_array(ctx, p))
                            return -1;
                        else
                            goto redo_prop_update;
                    }
                    if (flags & JS_PROP_HAS_VALUE) {
                        set_value(ctx, &p->u.array.u.values[idx], JS_DupValue(ctx, val));
                    }
                    return TRUE;
                }
            }
        } else if (p->class_id >= JS_CLASS_UINT8C_ARRAY &&
                   p->class_id <= JS_CLASS_FLOAT64_ARRAY) {
            JSValue num;
            int ret;

            if (!__JS_AtomIsTaggedInt(prop)) {
                /* slow path with to handle all numeric indexes */
                num = JS_AtomIsNumericIndex1(ctx, prop);
                if (JS_IsUndefined(num))
                    goto typed_array_done;
                if (JS_IsException(num))
                    return -1;
                ret = JS_NumberIsInteger(ctx, num);
                if (ret < 0) {
                    JS_FreeValue(ctx, num);
                    return -1;
                }
                if (!ret) {
                    JS_FreeValue(ctx, num);
                    return JS_ThrowTypeErrorOrFalse(ctx, flags, "non integer index in typed array");
                }
                ret = JS_NumberIsNegativeOrMinusZero(ctx, num);
                JS_FreeValue(ctx, num);
                if (ret) {
                    return JS_ThrowTypeErrorOrFalse(ctx, flags, "negative index in typed array");
                }
                if (!__JS_AtomIsTaggedInt(prop))
                    goto typed_array_oob;
            }
            idx = __JS_AtomToUInt32(prop);
            /* if the typed array is detached, p->u.array.count = 0 */
            if (idx >= p->u.array.count) {
            typed_array_oob:
                return JS_ThrowTypeErrorOrFalse(ctx, flags, "out-of-bound index in typed array");
            }
            prop_flags = get_prop_flags(flags, JS_PROP_ENUMERABLE | JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
            if (flags & (JS_PROP_HAS_GET | JS_PROP_HAS_SET) ||
                prop_flags != (JS_PROP_ENUMERABLE | JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE)) {
                return JS_ThrowTypeErrorOrFalse(ctx, flags, "invalid descriptor flags");
            }
            if (flags & JS_PROP_HAS_VALUE) {
                return JS_SetPropertyValue(ctx, this_obj, JS_NewInt32(ctx, idx), JS_DupValue(ctx, val), flags);
            }
            return TRUE;
        typed_array_done: ;
        }
    }

    return JS_CreateProperty(ctx, p, prop, val, getter, setter, flags);
}

static int JS_DefineAutoInitProperty(JSContext *ctx, JSValueConst this_obj,
                                     JSAtom prop, JSAutoInitIDEnum id,
                                     void *opaque, int flags)
{
    JSObject *p;
    JSProperty *pr;

    if (JS_VALUE_GET_TAG(this_obj) != JS_TAG_OBJECT)
        return FALSE;

    p = JS_VALUE_GET_OBJ(this_obj);

    if (find_own_property(&pr, p, prop)) {
        /* property already exists */
        abort();
        return FALSE;
    }

    /* Specialized CreateProperty */
    pr = add_property(ctx, p, prop, (flags & JS_PROP_C_W_E) | JS_PROP_AUTOINIT);
    if (unlikely(!pr))
        return -1;
    pr->u.init.realm_and_id = (uintptr_t)JS_DupContext(ctx);
    assert((pr->u.init.realm_and_id & 3) == 0);
    assert(id <= 3);
    pr->u.init.realm_and_id |= id;
    pr->u.init.opaque = opaque;
    return TRUE;
}

/* shortcut to add or redefine a new property value */
int JS_DefinePropertyValue(JSContext *ctx, JSValueConst this_obj,
                           JSAtom prop, JSValue val, int flags)
{
    int ret;
    ret = JS_DefineProperty(ctx, this_obj, prop, val, JS_UNDEFINED, JS_UNDEFINED,
                            flags | JS_PROP_HAS_VALUE | JS_PROP_HAS_CONFIGURABLE | JS_PROP_HAS_WRITABLE | JS_PROP_HAS_ENUMERABLE);
    JS_FreeValue(ctx, val);
    return ret;
}

int JS_DefinePropertyValueValue(JSContext *ctx, JSValueConst this_obj,
                                JSValue prop, JSValue val, int flags)
{
    JSAtom atom;
    int ret;
    atom = JS_ValueToAtom(ctx, prop);
    JS_FreeValue(ctx, prop);
    if (unlikely(atom == JS_ATOM_NULL)) {
        JS_FreeValue(ctx, val);
        return -1;
    }
    ret = JS_DefinePropertyValue(ctx, this_obj, atom, val, flags);
    JS_FreeAtom(ctx, atom);
    return ret;
}

int JS_DefinePropertyValueUint32(JSContext *ctx, JSValueConst this_obj,
                                 uint32_t idx, JSValue val, int flags)
{
    return JS_DefinePropertyValueValue(ctx, this_obj, JS_NewUint32(ctx, idx),
                                       val, flags);
}

int JS_DefinePropertyValueInt64(JSContext *ctx, JSValueConst this_obj,
                                int64_t idx, JSValue val, int flags)
{
    return JS_DefinePropertyValueValue(ctx, this_obj, JS_NewInt64(ctx, idx),
                                       val, flags);
}

int JS_DefinePropertyValueStr(JSContext *ctx, JSValueConst this_obj,
                              const char *prop, JSValue val, int flags)
{
    JSAtom atom;
    int ret;
    atom = JS_NewAtom(ctx, prop);
    if (atom == JS_ATOM_NULL) {
        JS_FreeValue(ctx, val);
        return -1;
    }
    ret = JS_DefinePropertyValue(ctx, this_obj, atom, val, flags);
    JS_FreeAtom(ctx, atom);
    return ret;
}

/* shortcut to add getter & setter */
int JS_DefinePropertyGetSet(JSContext *ctx, JSValueConst this_obj,
                            JSAtom prop, JSValue getter, JSValue setter,
                            int flags)
{
    int ret;
    ret = JS_DefineProperty(ctx, this_obj, prop, JS_UNDEFINED, getter, setter,
                            flags | JS_PROP_HAS_GET | JS_PROP_HAS_SET |
                            JS_PROP_HAS_CONFIGURABLE | JS_PROP_HAS_ENUMERABLE);
    JS_FreeValue(ctx, getter);
    JS_FreeValue(ctx, setter);
    return ret;
}

static int JS_CreateDataPropertyUint32(JSContext *ctx, JSValueConst this_obj,
                                       int64_t idx, JSValue val, int flags)
{
    return JS_DefinePropertyValueValue(ctx, this_obj, JS_NewInt64(ctx, idx),
                                       val, flags | JS_PROP_CONFIGURABLE |
                                       JS_PROP_ENUMERABLE | JS_PROP_WRITABLE);
}


/* return TRUE if 'obj' has a non empty 'name' string */
static BOOL js_object_has_name(JSContext *ctx, JSValueConst obj)
{
    JSProperty *pr;
    JSShapeProperty *prs;
    JSValueConst val;
    JSString *p;

    prs = find_own_property(&pr, JS_VALUE_GET_OBJ(obj), JS_ATOM_name);
    if (!prs)
        return FALSE;
    if ((prs->flags & JS_PROP_TMASK) != JS_PROP_NORMAL)
        return TRUE;
    val = pr->u.value;
    if (JS_VALUE_GET_TAG(val) != JS_TAG_STRING)
        return TRUE;
    p = JS_VALUE_GET_STRING(val);
    return (p->len != 0);
}

static int JS_DefineObjectName(JSContext *ctx, JSValueConst obj,
                               JSAtom name, int flags)
{
    if (name != JS_ATOM_NULL
    &&  JS_IsObject(obj)
    &&  !js_object_has_name(ctx, obj)
    &&  JS_DefinePropertyValue(ctx, obj, JS_ATOM_name, JS_AtomToString(ctx, name), flags) < 0) {
        return -1;
    }
    return 0;
}

static int JS_DefineObjectNameComputed(JSContext *ctx, JSValueConst obj,
                                       JSValueConst str, int flags)
{
    if (JS_IsObject(obj) &&
        !js_object_has_name(ctx, obj)) {
        JSAtom prop;
        JSValue name_str;
        prop = JS_ValueToAtom(ctx, str);
        if (prop == JS_ATOM_NULL)
            return -1;
        name_str = js_get_function_name(ctx, prop);
        JS_FreeAtom(ctx, prop);
        if (JS_IsException(name_str))
            return -1;
        if (JS_DefinePropertyValue(ctx, obj, JS_ATOM_name, name_str, flags) < 0)
            return -1;
    }
    return 0;
}

#define DEFINE_GLOBAL_LEX_VAR (1 << 7)
#define DEFINE_GLOBAL_FUNC_VAR (1 << 6)

static JSValue JS_ThrowSyntaxErrorVarRedeclaration(JSContext *ctx, JSAtom prop)
{
    return JS_ThrowSyntaxErrorAtom(ctx, "redeclaration of '%s'", prop);
}

/* flags is 0, DEFINE_GLOBAL_LEX_VAR or DEFINE_GLOBAL_FUNC_VAR */
/* XXX: could support exotic global object. */
static int JS_CheckDefineGlobalVar(JSContext *ctx, JSAtom prop, int flags)
{
    JSObject *p;
    JSShapeProperty *prs;

    p = JS_VALUE_GET_OBJ(ctx->global_obj);
    prs = find_own_property1(p, prop);
    /* XXX: should handle JS_PROP_AUTOINIT */
    if (flags & DEFINE_GLOBAL_LEX_VAR) {
        if (prs && !(prs->flags & JS_PROP_CONFIGURABLE))
            goto fail_redeclaration;
    } else {
        if (!prs && !p->extensible)
            goto define_error;
        if (flags & DEFINE_GLOBAL_FUNC_VAR) {
            if (prs) {
                if (!(prs->flags & JS_PROP_CONFIGURABLE) &&
                    ((prs->flags & JS_PROP_TMASK) == JS_PROP_GETSET ||
                     ((prs->flags & (JS_PROP_WRITABLE | JS_PROP_ENUMERABLE)) !=
                      (JS_PROP_WRITABLE | JS_PROP_ENUMERABLE)))) {
                define_error:
                    JS_ThrowTypeErrorAtom(ctx, "cannot define variable '%s'",
                                          prop);
                    return -1;
                }
            }
        }
    }
    /* check if there already is a lexical declaration */
    p = JS_VALUE_GET_OBJ(ctx->global_var_obj);
    prs = find_own_property1(p, prop);
    if (prs) {
    fail_redeclaration:
        JS_ThrowSyntaxErrorVarRedeclaration(ctx, prop);
        return -1;
    }
    return 0;
}

/* construct a reference to a global variable */
static int JS_GetGlobalVarRef(JSContext *ctx, JSAtom prop, JSValue *sp)
{
    JSObject *p;
    JSShapeProperty *prs;
    JSProperty *pr;

    /* no exotic behavior is possible in global_var_obj */
    p = JS_VALUE_GET_OBJ(ctx->global_var_obj);
    prs = find_own_property(&pr, p, prop);
    if (prs) {
        /* XXX: conformance: do these tests in
           OP_put_var_ref/OP_get_var_ref ? */
        if (unlikely(JS_IsUninitialized(*pr->u.var_ref->pvalue))) {
            JS_ThrowReferenceErrorUninitialized(ctx, prs->atom);
            return -1;
        }
        if (unlikely(!(prs->flags & JS_PROP_WRITABLE))) {
            return JS_ThrowTypeErrorReadOnly(ctx, JS_PROP_THROW, prop);
        }
        sp[0] = JS_DupValue(ctx, ctx->global_var_obj);
    } else {
        int ret;
        ret = JS_HasProperty(ctx, ctx->global_obj, prop);
        if (ret < 0)
            return -1;
        if (ret) {
            sp[0] = JS_DupValue(ctx, ctx->global_obj);
        } else {
            sp[0] = JS_UNDEFINED;
        }
    }
    sp[1] = JS_AtomToValue(ctx, prop);
    return 0;
}

/* return -1, FALSE or TRUE */
static int JS_DeleteGlobalVar(JSContext *ctx, JSAtom prop)
{
    JSObject *p;
    JSShapeProperty *prs;
    JSProperty *pr;
    int ret;

    /* 9.1.1.4.7 DeleteBinding ( N ) */
    p = JS_VALUE_GET_OBJ(ctx->global_var_obj);
    prs = find_own_property(&pr, p, prop);
    if (prs)
        return FALSE; /* lexical variables cannot be deleted */
    ret = JS_HasProperty(ctx, ctx->global_obj, prop);
    if (ret < 0)
        return -1;
    if (ret) {
        return JS_DeleteProperty(ctx, ctx->global_obj, prop, 0);
    } else {
        return TRUE;
    }
}

/* return -1, FALSE or TRUE. return FALSE if not configurable or
   invalid object. return -1 in case of exception.
   flags can be 0, JS_PROP_THROW or JS_PROP_THROW_STRICT */
int JS_DeleteProperty(JSContext *ctx, JSValueConst obj, JSAtom prop, int flags)
{
    JSValue obj1;
    JSObject *p;
    int res;

    obj1 = JS_ToObject(ctx, obj);
    if (JS_IsException(obj1))
        return -1;
    p = JS_VALUE_GET_OBJ(obj1);
    res = delete_property(ctx, p, prop);
    JS_FreeValue(ctx, obj1);
    if (res != FALSE)
        return res;
    if ((flags & JS_PROP_THROW) ||
        ((flags & JS_PROP_THROW_STRICT) && is_strict_mode(ctx))) {
        JS_ThrowTypeError(ctx, "could not delete property");
        return -1;
    }
    return FALSE;
}

int JS_DeletePropertyInt64(JSContext *ctx, JSValueConst obj, int64_t idx, int flags)
{
    JSAtom prop;
    int res;

    if ((uint64_t)idx <= JS_ATOM_MAX_INT) {
        /* fast path for fast arrays */
        return JS_DeleteProperty(ctx, obj, __JS_AtomFromUInt32(idx), flags);
    }
    prop = JS_NewAtomInt64(ctx, idx);
    if (prop == JS_ATOM_NULL)
        return -1;
    res = JS_DeleteProperty(ctx, obj, prop, flags);
    JS_FreeAtom(ctx, prop);
    return res;
}

BOOL JS_IsFunction(JSContext *ctx, JSValueConst val)
{
    JSObject *p;
    if (JS_VALUE_GET_TAG(val) != JS_TAG_OBJECT)
        return FALSE;
    p = JS_VALUE_GET_OBJ(val);
    switch(p->class_id) {
    case JS_CLASS_BYTECODE_FUNCTION:
        return TRUE;
    case JS_CLASS_PROXY:
        return p->u.proxy_data->is_func;
    default:
        return (ctx->rt->class_array[p->class_id].call != NULL);
    }
}

BOOL JS_IsCFunction(JSContext *ctx, JSValueConst val, JSCFunction *func, int magic)
{
    JSObject *p;
    if (JS_VALUE_GET_TAG(val) != JS_TAG_OBJECT)
        return FALSE;
    p = JS_VALUE_GET_OBJ(val);
    if (p->class_id == JS_CLASS_C_FUNCTION)
        return (p->u.cfunc.c_function.generic == func && p->u.cfunc.magic == magic);
    else
        return FALSE;
}

BOOL JS_IsConstructor(JSContext *ctx, JSValueConst val)
{
    JSObject *p;
    if (JS_VALUE_GET_TAG(val) != JS_TAG_OBJECT)
        return FALSE;
    p = JS_VALUE_GET_OBJ(val);
    return p->is_constructor;
}

BOOL JS_SetConstructorBit(JSContext *ctx, JSValueConst func_obj, BOOL val)
{
    JSObject *p;
    if (JS_VALUE_GET_TAG(func_obj) != JS_TAG_OBJECT)
        return FALSE;
    p = JS_VALUE_GET_OBJ(func_obj);
    p->is_constructor = val;
    return TRUE;
}

BOOL JS_IsError(JSContext *ctx, JSValueConst val)
{
    JSObject *p;
    if (JS_VALUE_GET_TAG(val) != JS_TAG_OBJECT)
        return FALSE;
    p = JS_VALUE_GET_OBJ(val);
    return (p->class_id == JS_CLASS_ERROR);
}

/* must be called after JS_Throw() */
void JS_SetUncatchableException(JSContext *ctx, BOOL flag)
{
    ctx->rt->current_exception_is_uncatchable = flag;
}

void JS_SetOpaque(JSValue obj, void *opaque)
{
   JSObject *p;
    if (JS_VALUE_GET_TAG(obj) == JS_TAG_OBJECT) {
        p = JS_VALUE_GET_OBJ(obj);
        p->u.opaque = opaque;
    }
}

/* return NULL if not an object of class class_id */
void *JS_GetOpaque(JSValueConst obj, JSClassID class_id)
{
    JSObject *p;
    if (JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT)
        return NULL;
    p = JS_VALUE_GET_OBJ(obj);
    if (p->class_id != class_id)
        return NULL;
    return p->u.opaque;
}

void *JS_GetOpaque2(JSContext *ctx, JSValueConst obj, JSClassID class_id)
{
    void *p = JS_GetOpaque(obj, class_id);
    if (unlikely(!p)) {
        JS_ThrowTypeErrorInvalidClass(ctx, class_id);
    }
    return p;
}

void *JS_GetAnyOpaque(JSValueConst obj, JSClassID *class_id)
{
    JSObject *p;
    if (JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT) {
        *class_id = 0;
        return NULL;
    }
    p = JS_VALUE_GET_OBJ(obj);
    *class_id = p->class_id;
    return p->u.opaque;
}

static JSValue JS_ToPrimitiveFree(JSContext *ctx, JSValue val, int hint)
{
    int i;
    BOOL force_ordinary;

    JSAtom method_name;
    JSValue method, ret;
    if (JS_VALUE_GET_TAG(val) != JS_TAG_OBJECT)
        return val;
    force_ordinary = hint & HINT_FORCE_ORDINARY;
    hint &= ~HINT_FORCE_ORDINARY;
    if (!force_ordinary) {
        method = JS_GetProperty(ctx, val, JS_ATOM_Symbol_toPrimitive);
        if (JS_IsException(method))
            goto exception;
        /* ECMA says *If exoticToPrim is not undefined* but tests in
           test262 use null as a non callable converter */
        if (!JS_IsUndefined(method) && !JS_IsNull(method)) {
            JSAtom atom;
            JSValue arg;
            switch(hint) {
            case HINT_STRING:
                atom = JS_ATOM_string;
                break;
            case HINT_NUMBER:
                atom = JS_ATOM_number;
                break;
            default:
            case HINT_NONE:
                atom = JS_ATOM_default;
                break;
            }
            arg = JS_AtomToString(ctx, atom);
            ret = JS_CallFree(ctx, method, val, 1, (JSValueConst *)&arg);
            JS_FreeValue(ctx, arg);
            if (JS_IsException(ret))
                goto exception;
            JS_FreeValue(ctx, val);
            if (JS_VALUE_GET_TAG(ret) != JS_TAG_OBJECT)
                return ret;
            JS_FreeValue(ctx, ret);
            return JS_ThrowTypeError(ctx, "toPrimitive");
        }
    }
    if (hint != HINT_STRING)
        hint = HINT_NUMBER;
    for(i = 0; i < 2; i++) {
        if ((i ^ hint) == 0) {
            method_name = JS_ATOM_toString;
        } else {
            method_name = JS_ATOM_valueOf;
        }
        method = JS_GetProperty(ctx, val, method_name);
        if (JS_IsException(method))
            goto exception;
        if (JS_IsFunction(ctx, method)) {
            ret = JS_CallFree(ctx, method, val, 0, NULL);
            if (JS_IsException(ret))
                goto exception;
            if (JS_VALUE_GET_TAG(ret) != JS_TAG_OBJECT) {
                JS_FreeValue(ctx, val);
                return ret;
            }
            JS_FreeValue(ctx, ret);
        } else {
            JS_FreeValue(ctx, method);
        }
    }
    JS_ThrowTypeError(ctx, "toPrimitive");
exception:
    JS_FreeValue(ctx, val);
    return JS_EXCEPTION;
}

static JSValue JS_ToPrimitive(JSContext *ctx, JSValueConst val, int hint)
{
    return JS_ToPrimitiveFree(ctx, JS_DupValue(ctx, val), hint);
}

void JS_SetIsHTMLDDA(JSContext *ctx, JSValueConst obj)
{
    JSObject *p;
    if (JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT)
        return;
    p = JS_VALUE_GET_OBJ(obj);
    p->is_HTMLDDA = TRUE;
}

static inline BOOL JS_IsHTMLDDA(JSContext *ctx, JSValueConst obj)
{
    JSObject *p;
    if (JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT)
        return FALSE;
    p = JS_VALUE_GET_OBJ(obj);
    return p->is_HTMLDDA;
}

static int JS_ToBoolFree(JSContext *ctx, JSValue val)
{
    uint32_t tag = JS_VALUE_GET_TAG(val);
    switch(tag) {
    case JS_TAG_INT:
        return JS_VALUE_GET_INT(val) != 0;
    case JS_TAG_BOOL:
    case JS_TAG_NULL:
    case JS_TAG_UNDEFINED:
        return JS_VALUE_GET_INT(val);
    case JS_TAG_EXCEPTION:
        return -1;
    case JS_TAG_STRING:
        {
            BOOL ret = JS_VALUE_GET_STRING(val)->len != 0;
            JS_FreeValue(ctx, val);
            return ret;
        }
    case JS_TAG_STRING_ROPE:
        {
            BOOL ret = JS_VALUE_GET_STRING_ROPE(val)->len != 0;
            JS_FreeValue(ctx, val);
            return ret;
        }
    case JS_TAG_SHORT_BIG_INT:
        return JS_VALUE_GET_SHORT_BIG_INT(val) != 0;
    case JS_TAG_BIG_INT:
        {
            JSBigInt *p = JS_VALUE_GET_PTR(val);
            BOOL ret;
            int i;

            /* fail safe: we assume it is not necessarily
               normalized. Beginning from the MSB ensures that the
               test is fast. */
            ret = FALSE;
            for(i = p->len - 1; i >= 0; i--) {
                if (p->tab[i] != 0) {
                    ret = TRUE;
                    break;
                }
            }
            JS_FreeValue(ctx, val);
            return ret;
        }
    case JS_TAG_OBJECT:
        {
            JSObject *p = JS_VALUE_GET_OBJ(val);
            BOOL ret;
            ret = !p->is_HTMLDDA;
            JS_FreeValue(ctx, val);
            return ret;
        }
        break;
    default:
        if (JS_TAG_IS_FLOAT64(tag)) {
            double d = JS_VALUE_GET_FLOAT64(val);
            return !isnan(d) && d != 0;
        } else {
            JS_FreeValue(ctx, val);
            return TRUE;
        }
    }
}

int JS_ToBool(JSContext *ctx, JSValueConst val)
{
    return JS_ToBoolFree(ctx, JS_DupValue(ctx, val));
}

static int skip_spaces(const char *pc)
{
    const uint8_t *p, *p_next, *p_start;
    uint32_t c;

    p = p_start = (const uint8_t *)pc;
    for (;;) {
        c = *p;
        if (c < 128) {
            if (!((c >= 0x09 && c <= 0x0d) || (c == 0x20)))
                break;
            p++;
        } else {
            c = unicode_from_utf8(p, UTF8_CHAR_LEN_MAX, &p_next);
            if (!lre_is_space(c))
                break;
            p = p_next;
        }
    }
    return p - p_start;
}

static inline int to_digit(int c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    else if (c >= 'A' && c <= 'Z')
        return c - 'A' + 10;
    else if (c >= 'a' && c <= 'z')
        return c - 'a' + 10;
    else
        return 36;
}

/* bigint support */

#define JS_BIGINT_MAX_SIZE ((1024 * 1024) / JS_LIMB_BITS) /* in limbs */

/* it is currently assumed that JS_SHORT_BIG_INT_BITS = JS_LIMB_BITS */
#if JS_SHORT_BIG_INT_BITS == 32
#define JS_SHORT_BIG_INT_MIN INT32_MIN
#define JS_SHORT_BIG_INT_MAX INT32_MAX
#elif JS_SHORT_BIG_INT_BITS == 64
#define JS_SHORT_BIG_INT_MIN INT64_MIN
#define JS_SHORT_BIG_INT_MAX INT64_MAX
#else
#error unsupported
#endif

#define ADDC(res, carry_out, op1, op2, carry_in)        \
do {                                                    \
    js_limb_t __v, __a, __k, __k1;                      \
    __v = (op1);                                        \
    __a = __v + (op2);                                  \
    __k1 = __a < __v;                                   \
    __k = (carry_in);                                   \
    __a = __a + __k;                                    \
    carry_out = (__a < __k) | __k1;                     \
    res = __a;                                          \
} while (0)

#if JS_LIMB_BITS == 32
/* a != 0 */
static inline js_limb_t js_limb_clz(js_limb_t a)
{
    return clz32(a);
}
#else
static inline js_limb_t js_limb_clz(js_limb_t a)
{
    return clz64(a);
}
#endif

/* handle a = 0 too */
static inline js_limb_t js_limb_safe_clz(js_limb_t a)
{
    if (a == 0)
        return JS_LIMB_BITS;
    else
        return js_limb_clz(a);
}

static js_limb_t mp_add(js_limb_t *res, const js_limb_t *op1, const js_limb_t *op2,
                     js_limb_t n, js_limb_t carry)
{
    int i;
    for(i = 0;i < n; i++) {
        ADDC(res[i], carry, op1[i], op2[i], carry);
    }
    return carry;
}

static js_limb_t mp_sub(js_limb_t *res, const js_limb_t *op1, const js_limb_t *op2,
                        int n, js_limb_t carry)
{
    int i;
    js_limb_t k, a, v, k1;

    k = carry;
    for(i=0;i<n;i++) {
        v = op1[i];
        a = v - op2[i];
        k1 = a > v;
        v = a - k;
        k = (v > a) | k1;
        res[i] = v;
    }
    return k;
}

/* compute 0 - op2. carry = 0 or 1. */
static js_limb_t mp_neg(js_limb_t *res, const js_limb_t *op2, int n)
{
    int i;
    js_limb_t v, carry;

    carry = 1;
    for(i=0;i<n;i++) {
        v = ~op2[i] + carry;
        carry = v < carry;
        res[i] = v;
    }
    return carry;
}

/* tabr[] = taba[] * b + l. Return the high carry */
static js_limb_t mp_mul1(js_limb_t *tabr, const js_limb_t *taba, js_limb_t n,
                      js_limb_t b, js_limb_t l)
{
    js_limb_t i;
    js_dlimb_t t;

    for(i = 0; i < n; i++) {
        t = (js_dlimb_t)taba[i] * (js_dlimb_t)b + l;
        tabr[i] = t;
        l = t >> JS_LIMB_BITS;
    }
    return l;
}

static js_limb_t mp_div1(js_limb_t *tabr, const js_limb_t *taba, js_limb_t n,
                      js_limb_t b, js_limb_t r)
{
    js_slimb_t i;
    js_dlimb_t a1;
    for(i = n - 1; i >= 0; i--) {
        a1 = ((js_dlimb_t)r << JS_LIMB_BITS) | taba[i];
        tabr[i] = a1 / b;
        r = a1 % b;
    }
    return r;
}

/* tabr[] += taba[] * b, return the high word. */
static js_limb_t mp_add_mul1(js_limb_t *tabr, const js_limb_t *taba, js_limb_t n,
                          js_limb_t b)
{
    js_limb_t i, l;
    js_dlimb_t t;

    l = 0;
    for(i = 0; i < n; i++) {
        t = (js_dlimb_t)taba[i] * (js_dlimb_t)b + l + tabr[i];
        tabr[i] = t;
        l = t >> JS_LIMB_BITS;
    }
    return l;
}

/* size of the result : op1_size + op2_size. */
static void mp_mul_basecase(js_limb_t *result,
                            const js_limb_t *op1, js_limb_t op1_size,
                            const js_limb_t *op2, js_limb_t op2_size)
{
    int i;
    js_limb_t r;

    result[op1_size] = mp_mul1(result, op1, op1_size, op2[0], 0);
    for(i=1;i<op2_size;i++) {
        r = mp_add_mul1(result + i, op1, op1_size, op2[i]);
        result[i + op1_size] = r;
    }
}

/* tabr[] -= taba[] * b. Return the value to substract to the high
   word. */
static js_limb_t mp_sub_mul1(js_limb_t *tabr, const js_limb_t *taba, js_limb_t n,
                          js_limb_t b)
{
    js_limb_t i, l;
    js_dlimb_t t;

    l = 0;
    for(i = 0; i < n; i++) {
        t = tabr[i] - (js_dlimb_t)taba[i] * (js_dlimb_t)b - l;
        tabr[i] = t;
        l = -(t >> JS_LIMB_BITS);
    }
    return l;
}

/* WARNING: d must be >= 2^(JS_LIMB_BITS-1) */
static inline js_limb_t udiv1norm_init(js_limb_t d)
{
    js_limb_t a0, a1;
    a1 = -d - 1;
    a0 = -1;
    return (((js_dlimb_t)a1 << JS_LIMB_BITS) | a0) / d;
}

/* return the quotient and the remainder in '*pr'of 'a1*2^JS_LIMB_BITS+a0
   / d' with 0 <= a1 < d. */
static inline js_limb_t udiv1norm(js_limb_t *pr, js_limb_t a1, js_limb_t a0,
                                js_limb_t d, js_limb_t d_inv)
{
    js_limb_t n1m, n_adj, q, r, ah;
    js_dlimb_t a;
    n1m = ((js_slimb_t)a0 >> (JS_LIMB_BITS - 1));
    n_adj = a0 + (n1m & d);
    a = (js_dlimb_t)d_inv * (a1 - n1m) + n_adj;
    q = (a >> JS_LIMB_BITS) + a1;
    /* compute a - q * r and update q so that the remainder is\
       between 0 and d - 1 */
    a = ((js_dlimb_t)a1 << JS_LIMB_BITS) | a0;
    a = a - (js_dlimb_t)q * d - d;
    ah = a >> JS_LIMB_BITS;
    q += 1 + ah;
    r = (js_limb_t)a + (ah & d);
    *pr = r;
    return q;
}

#define UDIV1NORM_THRESHOLD 3

/* b must be >= 1 << (JS_LIMB_BITS - 1) */
static js_limb_t mp_div1norm(js_limb_t *tabr, const js_limb_t *taba, js_limb_t n,
                          js_limb_t b, js_limb_t r)
{
    js_slimb_t i;

    if (n >= UDIV1NORM_THRESHOLD) {
        js_limb_t b_inv;
        b_inv = udiv1norm_init(b);
        for(i = n - 1; i >= 0; i--) {
            tabr[i] = udiv1norm(&r, r, taba[i], b, b_inv);
        }
    } else {
        js_dlimb_t a1;
        for(i = n - 1; i >= 0; i--) {
            a1 = ((js_dlimb_t)r << JS_LIMB_BITS) | taba[i];
            tabr[i] = a1 / b;
            r = a1 % b;
        }
    }
    return r;
}

/* base case division: divides taba[0..na-1] by tabb[0..nb-1]. tabb[nb
   - 1] must be >= 1 << (JS_LIMB_BITS - 1). na - nb must be >= 0. 'taba'
   is modified and contains the remainder (nb limbs). tabq[0..na-nb]
   contains the quotient with tabq[na - nb] <= 1. */
static void mp_divnorm(js_limb_t *tabq, js_limb_t *taba, js_limb_t na,
                       const js_limb_t *tabb, js_limb_t nb)
{
    js_limb_t r, a, c, q, v, b1, b1_inv, n, dummy_r;
    int i, j;

    b1 = tabb[nb - 1];
    if (nb == 1) {
        taba[0] = mp_div1norm(tabq, taba, na, b1, 0);
        return;
    }
    n = na - nb;

    if (n >= UDIV1NORM_THRESHOLD)
        b1_inv = udiv1norm_init(b1);
    else
        b1_inv = 0;

    /* first iteration: the quotient is only 0 or 1 */
    q = 1;
    for(j = nb - 1; j >= 0; j--) {
        if (taba[n + j] != tabb[j]) {
            if (taba[n + j] < tabb[j])
                q = 0;
            break;
        }
    }
    tabq[n] = q;
    if (q) {
        mp_sub(taba + n, taba + n, tabb, nb, 0);
    }

    for(i = n - 1; i >= 0; i--) {
        if (unlikely(taba[i + nb] >= b1)) {
            q = -1;
        } else if (b1_inv) {
            q = udiv1norm(&dummy_r, taba[i + nb], taba[i + nb - 1], b1, b1_inv);
        } else {
            js_dlimb_t al;
            al = ((js_dlimb_t)taba[i + nb] << JS_LIMB_BITS) | taba[i + nb - 1];
            q = al / b1;
            r = al % b1;
        }
        r = mp_sub_mul1(taba + i, tabb, nb, q);

        v = taba[i + nb];
        a = v - r;
        c = (a > v);
        taba[i + nb] = a;

        if (c != 0) {
            /* negative result */
            for(;;) {
                q--;
                c = mp_add(taba + i, taba + i, tabb, nb, 0);
                /* propagate carry and test if positive result */
                if (c != 0) {
                    if (++taba[i + nb] == 0) {
                        break;
                    }
                }
            }
        }
        tabq[i] = q;
    }
}

/* 1 <= shift <= JS_LIMB_BITS - 1 */
static js_limb_t mp_shl(js_limb_t *tabr, const js_limb_t *taba, int n,
                        int shift)
{
    int i;
    js_limb_t l, v;
    l = 0;
    for(i = 0; i < n; i++) {
        v = taba[i];
        tabr[i] = (v << shift) | l;
        l = v >> (JS_LIMB_BITS - shift);
    }
    return l;
}

/* r = (a + high*B^n) >> shift. Return the remainder r (0 <= r < 2^shift).
   1 <= shift <= LIMB_BITS - 1 */
static js_limb_t mp_shr(js_limb_t *tab_r, const js_limb_t *tab, int n,
                        int shift, js_limb_t high)
{
    int i;
    js_limb_t l, a;

    l = high;
    for(i = n - 1; i >= 0; i--) {
        a = tab[i];
        tab_r[i] = (a >> shift) | (l << (JS_LIMB_BITS - shift));
        l = a;
    }
    return l & (((js_limb_t)1 << shift) - 1);
}

static JSBigInt *js_bigint_new(JSContext *ctx, int len)
{
    JSBigInt *r;
    if (len > JS_BIGINT_MAX_SIZE) {
        JS_ThrowRangeError(ctx, "BigInt is too large to allocate");
        return NULL;
    }
    r = js_malloc(ctx, sizeof(JSBigInt) + len * sizeof(js_limb_t));
    if (!r)
        return NULL;
    js_rc(r)->ref_count = 1;
    r->len = len;
    return r;
}

static JSBigInt *js_bigint_set_si(JSBigIntBuf *buf, js_slimb_t a)
{
    JSBigInt *r = (JSBigInt *)buf->big_int_buf;
    r->len = 1;
    r->tab[0] = a;
    return r;
}

static JSBigInt *js_bigint_set_si64(JSBigIntBuf *buf, int64_t a)
{
#if JS_LIMB_BITS == 64
    return js_bigint_set_si(buf, a);
#else
    JSBigInt *r = (JSBigInt *)buf->big_int_buf;
    if (a >= INT32_MIN && a <= INT32_MAX) {
        r->len = 1;
        r->tab[0] = a;
    } else {
        r->len = 2;
        r->tab[0] = a;
        r->tab[1] = a >> JS_LIMB_BITS;
    }
    return r;
#endif
}

/* val must be a short big int */
static JSBigInt *js_bigint_set_short(JSBigIntBuf *buf, JSValueConst val)
{
    return js_bigint_set_si(buf, JS_VALUE_GET_SHORT_BIG_INT(val));
}

static __maybe_unused void js_bigint_dump1(JSContext *ctx, const char *str,
                                           const js_limb_t *tab, int len)
{
    int i;
    printf("%s: ", str);
    for(i = len - 1; i >= 0; i--) {
#if JS_LIMB_BITS == 32
        printf(" %08x", tab[i]);
#else
        printf(" %016" PRIx64, tab[i]);
#endif
    }
    printf("\n");
}

static __maybe_unused void js_bigint_dump(JSContext *ctx, const char *str,
                                          const JSBigInt *p)
{
    js_bigint_dump1(ctx, str, p->tab, p->len);
}

static JSBigInt *js_bigint_new_si(JSContext *ctx, js_slimb_t a)
{
    JSBigInt *r;
    r = js_bigint_new(ctx, 1);
    if (!r)
        return NULL;
    r->tab[0] = a;
    return r;
}

static JSBigInt *js_bigint_new_si64(JSContext *ctx, int64_t a)
{
#if JS_LIMB_BITS == 64
    return js_bigint_new_si(ctx, a);
#else
    if (a >= INT32_MIN && a <= INT32_MAX) {
        return js_bigint_new_si(ctx, a);
    } else {
        JSBigInt *r;
        r = js_bigint_new(ctx, 2);
        if (!r)
            return NULL;
        r->tab[0] = a;
        r->tab[1] = a >> 32;
        return r;
    }
#endif
}

static JSBigInt *js_bigint_new_ui64(JSContext *ctx, uint64_t a)
{
    if (a <= INT64_MAX) {
        return js_bigint_new_si64(ctx, a);
    } else {
        JSBigInt *r;
        r = js_bigint_new(ctx, (65 + JS_LIMB_BITS - 1) / JS_LIMB_BITS);
        if (!r)
            return NULL;
#if JS_LIMB_BITS == 64
        r->tab[0] = a;
        r->tab[1] = 0;
#else
        r->tab[0] = a;
        r->tab[1] = a >> 32;
        r->tab[2] = 0;
#endif
        return r;
    }
}

static JSBigInt *js_bigint_new_di(JSContext *ctx, js_sdlimb_t a)
{
    JSBigInt *r;
    if (a == (js_slimb_t)a) {
        r = js_bigint_new(ctx, 1);
        if (!r)
            return NULL;
        r->tab[0] = a;
    } else {
        r = js_bigint_new(ctx, 2);
        if (!r)
            return NULL;
        r->tab[0] = a;
        r->tab[1] = a >> JS_LIMB_BITS;
    }
    return r;
}

/* Remove redundant high order limbs. Warning: 'a' may be
   reallocated. Can never fail.
*/
static JSBigInt *js_bigint_normalize1(JSContext *ctx, JSBigInt *a, int l)
{
    js_limb_t v;

    assert(js_rc(a)->ref_count == 1);
    while (l > 1) {
        v = a->tab[l - 1];
        if ((v != 0 && v != -1) ||
            (v & 1) != (a->tab[l - 2] >> (JS_LIMB_BITS - 1))) {
            break;
        }
        l--;
    }
    if (l != a->len) {
        JSBigInt *a1;
        /* realloc to reduce the size */
        a->len = l;
        a1 = js_realloc(ctx, a, sizeof(JSBigInt) + l * sizeof(js_limb_t));
        if (a1)
            a = a1;
    }
    return a;
}

static JSBigInt *js_bigint_normalize(JSContext *ctx, JSBigInt *a)
{
    return js_bigint_normalize1(ctx, a, a->len);
}

/* return 0 or 1 depending on the sign */
static inline int js_bigint_sign(const JSBigInt *a)
{
    return a->tab[a->len - 1] >> (JS_LIMB_BITS - 1);
}

static js_slimb_t js_bigint_get_si_sat(const JSBigInt *a)
{
    if (a->len == 1) {
        return a->tab[0];
    } else {
#if JS_LIMB_BITS == 32
        if (js_bigint_sign(a))
            return INT32_MIN;
        else
            return INT32_MAX;
#else
        if (js_bigint_sign(a))
            return INT64_MIN;
        else
            return INT64_MAX;
#endif
    }
}

/* add the op1 limb */
static JSBigInt *js_bigint_extend(JSContext *ctx, JSBigInt *r,
                                  js_limb_t op1)
{
    int n2 = r->len;
    if ((op1 != 0 && op1 != -1) ||
        (op1 & 1) != r->tab[n2 - 1] >> (JS_LIMB_BITS - 1)) {
        JSBigInt *r1;
        r1 = js_realloc(ctx, r,
                        sizeof(JSBigInt) + (n2 + 1) * sizeof(js_limb_t));
        if (!r1) {
            js_free(ctx, r);
            return NULL;
        }
        r = r1;
        r->len = n2 + 1;
        r->tab[n2] = op1;
    } else {
        /* otherwise still need to normalize the result */
        r = js_bigint_normalize(ctx, r);
    }
    return r;
}

/* return NULL in case of error. Compute a + b (b_neg = 0) or a - b
   (b_neg = 1) */
/* XXX: optimize */
static JSBigInt *js_bigint_add(JSContext *ctx, const JSBigInt *a,
                               const JSBigInt *b, int b_neg)
{
    JSBigInt *r;
    int n1, n2, i;
    js_limb_t carry, op1, op2, a_sign, b_sign;

    n2 = max_int(a->len, b->len);
    n1 = min_int(a->len, b->len);
    r = js_bigint_new(ctx, n2);
    if (!r)
        return NULL;
    /* XXX: optimize */
    /* common part */
    carry = b_neg;
    for(i = 0; i < n1; i++) {
        op1 = a->tab[i];
        op2 = b->tab[i] ^ (-b_neg);
        ADDC(r->tab[i], carry, op1, op2, carry);
    }
    a_sign = -js_bigint_sign(a);
    b_sign = (-js_bigint_sign(b)) ^ (-b_neg);
    /* part with sign extension of one operand  */
    if (a->len > b->len) {
        for(i = n1; i < n2; i++) {
            op1 = a->tab[i];
            ADDC(r->tab[i], carry, op1, b_sign, carry);
        }
    } else if (a->len < b->len) {
        for(i = n1; i < n2; i++) {
            op2 = b->tab[i] ^ (-b_neg);
            ADDC(r->tab[i], carry, a_sign, op2, carry);
        }
    }

    /* part with sign extension for both operands. Extend the result
       if necessary */
    return js_bigint_extend(ctx, r, a_sign + b_sign + carry);
}

/* XXX: optimize */
static JSBigInt *js_bigint_neg(JSContext *ctx, const JSBigInt *a)
{
    JSBigIntBuf buf;
    JSBigInt *b;
    b = js_bigint_set_si(&buf, 0);
    return js_bigint_add(ctx, b, a, 1);
}

static JSBigInt *js_bigint_mul(JSContext *ctx, const JSBigInt *a,
                               const JSBigInt *b)
{
    JSBigInt *r;

    r = js_bigint_new(ctx, a->len + b->len);
    if (!r)
        return NULL;
    mp_mul_basecase(r->tab, a->tab, a->len, b->tab, b->len);
    /* correct the result if negative operands (no overflow is
       possible) */
    if (js_bigint_sign(a))
        mp_sub(r->tab + a->len, r->tab + a->len, b->tab, b->len, 0);
    if (js_bigint_sign(b))
        mp_sub(r->tab + b->len, r->tab + b->len, a->tab, a->len, 0);
    return js_bigint_normalize(ctx, r);
}

/* return the division or the remainder. 'b' must be != 0. return NULL
   in case of exception (division by zero or memory error) */
static JSBigInt *js_bigint_divrem(JSContext *ctx, const JSBigInt *a,
                                  const JSBigInt *b, BOOL is_rem)
{
    JSBigInt *r, *q;
    js_limb_t *tabb, h;
    int na, nb, a_sign, b_sign, shift;

    if (b->len == 1 && b->tab[0] == 0) {
        JS_ThrowRangeError(ctx, "BigInt division by zero");
        return NULL;
    }

    a_sign = js_bigint_sign(a);
    b_sign = js_bigint_sign(b);
    na = a->len;
    nb = b->len;

    r = js_bigint_new(ctx, na + 2);
    if (!r)
        return NULL;
    if (a_sign) {
        mp_neg(r->tab, a->tab, na);
    } else {
        memcpy(r->tab, a->tab, na * sizeof(a->tab[0]));
    }
    /* normalize */
    while (na > 1 && r->tab[na - 1] == 0)
        na--;

    tabb = js_malloc(ctx, nb * sizeof(tabb[0]));
    if (!tabb) {
        js_free(ctx, r);
        return NULL;
    }
    if (b_sign) {
        mp_neg(tabb, b->tab, nb);
    } else {
        memcpy(tabb, b->tab, nb * sizeof(tabb[0]));
    }
    /* normalize */
    while (nb > 1 && tabb[nb - 1] == 0)
        nb--;

    /* trivial case if 'a' is small */
    if (na < nb) {
        js_free(ctx, r);
        js_free(ctx, tabb);
        if (is_rem) {
            /* r = a */
            r = js_bigint_new(ctx, a->len);
            if (!r)
                return NULL;
            memcpy(r->tab, a->tab, a->len * sizeof(a->tab[0]));
            return r;
        } else {
            /* q = 0 */
            return js_bigint_new_si(ctx, 0);
        }
    }

    /* normalize 'b' */
    shift = js_limb_clz(tabb[nb - 1]);
    if (shift != 0) {
        mp_shl(tabb, tabb, nb, shift);
        h = mp_shl(r->tab, r->tab, na, shift);
        if (h != 0)
            r->tab[na++] = h;
    }

    q = js_bigint_new(ctx, na - nb + 2); /* one more limb for the sign */
    if (!q) {
        js_free(ctx, r);
        js_free(ctx, tabb);
        return NULL;
    }

    //    js_bigint_dump1(ctx, "a", r->tab, na);
    //    js_bigint_dump1(ctx, "b", tabb, nb);
    mp_divnorm(q->tab, r->tab, na, tabb, nb);
    js_free(ctx, tabb);

    if (is_rem) {
        js_free(ctx, q);
        if (shift != 0)
            mp_shr(r->tab, r->tab, nb, shift, 0);
        r->tab[nb++] = 0;
        if (a_sign)
            mp_neg(r->tab, r->tab, nb);
        r = js_bigint_normalize1(ctx, r, nb);
        return r;
    } else {
        js_free(ctx, r);
        q->tab[na - nb + 1] = 0;
        if (a_sign ^ b_sign) {
            mp_neg(q->tab, q->tab, q->len);
        }
        q = js_bigint_normalize(ctx, q);
        return q;
    }
}

/* and, or, xor */
static JSBigInt *js_bigint_logic(JSContext *ctx, const JSBigInt *a,
                                 const JSBigInt *b, OPCodeEnum op)
{
    JSBigInt *r;
    js_limb_t b_sign;
    int a_len, b_len, i;

    if (a->len < b->len) {
        const JSBigInt *tmp;
        tmp = a;
        a = b;
        b = tmp;
    }
    /* a_len >= b_len */
    a_len = a->len;
    b_len = b->len;
    b_sign = -js_bigint_sign(b);

    r = js_bigint_new(ctx, a_len);
    if (!r)
        return NULL;
    switch(op) {
    case OP_or:
        for(i = 0; i < b_len; i++) {
            r->tab[i] = a->tab[i] | b->tab[i];
        }
        for(i = b_len; i < a_len; i++) {
            r->tab[i] = a->tab[i] | b_sign;
        }
        break;
    case OP_and:
        for(i = 0; i < b_len; i++) {
            r->tab[i] = a->tab[i] & b->tab[i];
        }
        for(i = b_len; i < a_len; i++) {
            r->tab[i] = a->tab[i] & b_sign;
        }
        break;
    case OP_xor:
        for(i = 0; i < b_len; i++) {
            r->tab[i] = a->tab[i] ^ b->tab[i];
        }
        for(i = b_len; i < a_len; i++) {
            r->tab[i] = a->tab[i] ^ b_sign;
        }
        break;
    default:
        abort();
    }
    return js_bigint_normalize(ctx, r);
}

static JSBigInt *js_bigint_not(JSContext *ctx, const JSBigInt *a)
{
    JSBigInt *r;
    int i;

    r = js_bigint_new(ctx, a->len);
    if (!r)
        return NULL;
    for(i = 0; i < a->len; i++) {
        r->tab[i] = ~a->tab[i];
    }
    /* no normalization is needed */
    return r;
}

static JSBigInt *js_bigint_shl(JSContext *ctx, const JSBigInt *a,
                               unsigned int shift1)
{
    int d, i, shift;
    JSBigInt *r;
    js_limb_t l;

    if (a->len == 1 && a->tab[0] == 0)
        return js_bigint_new_si(ctx, 0); /* zero case */
    d = shift1 / JS_LIMB_BITS;
    shift = shift1 % JS_LIMB_BITS;
    r = js_bigint_new(ctx, a->len + d);
    if (!r)
        return NULL;
    for(i = 0; i < d; i++)
        r->tab[i] = 0;
    if (shift == 0) {
        for(i = 0; i < a->len; i++) {
            r->tab[i + d] = a->tab[i];
        }
    } else {
        l = mp_shl(r->tab + d, a->tab, a->len, shift);
        if (js_bigint_sign(a))
            l |= (js_limb_t)(-1) << shift;
        r = js_bigint_extend(ctx, r, l);
    }
    return r;
}

static JSBigInt *js_bigint_shr(JSContext *ctx, const JSBigInt *a,
                               unsigned int shift1)
{
    int d, i, shift, a_sign, n1;
    JSBigInt *r;

    d = shift1 / JS_LIMB_BITS;
    shift = shift1 % JS_LIMB_BITS;
    a_sign = js_bigint_sign(a);
    if (d >= a->len)
        return js_bigint_new_si(ctx, -a_sign);
    n1 = a->len - d;
    r = js_bigint_new(ctx, n1);
    if (!r)
        return NULL;
    if (shift == 0) {
        for(i = 0; i < n1; i++) {
            r->tab[i] = a->tab[i + d];
        }
        /* no normalization is needed */
    } else {
        mp_shr(r->tab, a->tab + d, n1, shift, -a_sign);
        r = js_bigint_normalize(ctx, r);
    }
    return r;
}

static JSBigInt *js_bigint_pow(JSContext *ctx, const JSBigInt *a, JSBigInt *b)
{
    uint32_t e;
    int n_bits, i;
    JSBigInt *r, *r1;

    /* b must be >= 0 */
    if (js_bigint_sign(b)) {
        JS_ThrowRangeError(ctx, "BigInt negative exponent");
        return NULL;
    }
    if (b->len == 1 && b->tab[0] == 0) {
        /* a^0 = 1 */
        return js_bigint_new_si(ctx, 1);
    } else if (a->len == 1) {
        js_limb_t v;
        BOOL is_neg;

        v = a->tab[0];
        if (v <= 1)
            return js_bigint_new_si(ctx, v);
        else if (v == -1)
            return js_bigint_new_si(ctx, 1 - 2 * (b->tab[0] & 1));
        is_neg = (js_slimb_t)v < 0;
        if (is_neg)
            v = -v;
        if ((v & (v - 1)) == 0) {
            uint64_t e1;
            int n;
            /* v = 2^n */
            n = JS_LIMB_BITS - 1 - js_limb_clz(v);
            if (b->len > 1)
                goto overflow;
            if (b->tab[0] > INT32_MAX)
                goto overflow;
            e = b->tab[0];
            e1 = (uint64_t)e * n;
            if (e1 > JS_BIGINT_MAX_SIZE * JS_LIMB_BITS)
                goto overflow;
            e = e1;
            if (is_neg)
                is_neg = b->tab[0] & 1;
            r = js_bigint_new(ctx,
                              (e + JS_LIMB_BITS + 1 - is_neg) / JS_LIMB_BITS);
            if (!r)
                return NULL;
            memset(r->tab, 0, sizeof(r->tab[0]) * r->len);
            r->tab[e / JS_LIMB_BITS] =
                (js_limb_t)(1 - 2 * is_neg) << (e % JS_LIMB_BITS);
            return r;
        }
    }
    if (b->len > 1)
        goto overflow;
    if (b->tab[0] > INT32_MAX)
        goto overflow;
    e = b->tab[0];
    n_bits = 32 - clz32(e);

    r = js_bigint_new(ctx, a->len);
    if (!r)
        return NULL;
    memcpy(r->tab, a->tab, a->len * sizeof(a->tab[0]));
    for(i = n_bits - 2; i >= 0; i--) {
        r1 = js_bigint_mul(ctx, r, r);
        if (!r1)
            return NULL;
        js_free(ctx, r);
        r = r1;
        if ((e >> i) & 1) {
            r1 = js_bigint_mul(ctx, r, a);
            if (!r1)
                return NULL;
            js_free(ctx, r);
            r = r1;
        }
    }
    return r;
 overflow:
    JS_ThrowRangeError(ctx, "BigInt is too large");
    return NULL;
}

/* return (mant, exp) so that abs(a) ~ mant*2^(exp - (limb_bits -
   1). a must be != 0. */
static uint64_t js_bigint_get_mant_exp(JSContext *ctx,
                                       int *pexp, const JSBigInt *a)
{
    js_limb_t t[4 - JS_LIMB_BITS / 32], carry, v, low_bits;
    int n1, n2, sgn, shift, i, j, e;
    uint64_t a1, a0;

    n2 = 4 - JS_LIMB_BITS / 32;
    n1 = a->len - n2;
    sgn = js_bigint_sign(a);

    /* low_bits != 0 if there are a non zero low bit in abs(a) */
    low_bits = 0;
    carry = sgn;
    for(i = 0; i < n1; i++) {
        v = (a->tab[i] ^ (-sgn)) + carry;
        carry = v < carry;
        low_bits |= v;
    }
    /* get the n2 high limbs of abs(a) */
    for(j = 0; j < n2; j++) {
        i = j + n1;
        if (i < 0) {
            v = 0;
        } else {
            v = (a->tab[i] ^ (-sgn)) + carry;
            carry = v < carry;
        }
        t[j] = v;
    }

#if JS_LIMB_BITS == 32
    a1 = ((uint64_t)t[2] << 32) | t[1];
    a0 = (uint64_t)t[0] << 32;
#else
    a1 = t[1];
    a0 = t[0];
#endif
    a0 |= (low_bits != 0);
    /* normalize */
    if (a1 == 0) {
        /* JS_LIMB_BITS = 64 bit only */
        shift = 64;
        a1 = a0;
        a0 = 0;
    } else {
        shift = clz64(a1);
        if (shift != 0) {
            a1 = (a1 << shift) | (a0 >> (64 - shift));
            a0 <<= shift;
        }
    }
    a1 |= (a0 != 0); /* keep the bits for the final rounding */
    /* compute the exponent */
    e = a->len * JS_LIMB_BITS - shift - 1;
    *pexp = e;
    return a1;
}

/* shift left with round to nearest, ties to even. n >= 1 */
static uint64_t shr_rndn(uint64_t a, int n)
{
    uint64_t addend = ((a >> n) & 1) + ((1 << (n - 1)) - 1);
    return (a + addend) >> n;
}

/* convert to float64 with round to nearest, ties to even. Return
   +/-infinity if too large. */
static double js_bigint_to_float64(JSContext *ctx, const JSBigInt *a)
{
    int sgn, e;
    uint64_t mant;

    if (a->len == 1) {
        /* fast case, including zero */
        return (double)(js_slimb_t)a->tab[0];
    }

    sgn = js_bigint_sign(a);
    mant = js_bigint_get_mant_exp(ctx, &e, a);
    if (e > 1023) {
        /* overflow: return infinity */
        mant = 0;
        e = 1024;
    } else {
        mant = (mant >> 1) | (mant & 1); /* avoid overflow in rounding */
        mant = shr_rndn(mant, 10);
        /* rounding can cause an overflow */
        if (mant >= ((uint64_t)1 << 53)) {
            mant >>= 1;
            e++;
        }
        mant &= (((uint64_t)1 << 52) - 1);
    }
    return uint64_as_float64(((uint64_t)sgn << 63) |
                             ((uint64_t)(e + 1023) << 52) |
                             mant);
}

/* return (1, NULL) if not an integer, (2, NULL) if NaN or Infinity,
   (0, n) if an integer, (0, NULL) in case of memory error */
static JSBigInt *js_bigint_from_float64(JSContext *ctx, int *pres, double a1)
{
    uint64_t a = float64_as_uint64(a1);
    int sgn, e, shift;
    uint64_t mant;
    JSBigIntBuf buf;
    JSBigInt *r;

    sgn = a >> 63;
    e = (a >> 52) & ((1 << 11) - 1);
    mant = a & (((uint64_t)1 << 52) - 1);
    if (e == 2047) {
        /* NaN, Infinity */
        *pres = 2;
        return NULL;
    }
    if (e == 0 && mant == 0) {
        /* zero */
        *pres = 0;
        return js_bigint_new_si(ctx, 0);
    }
    e -= 1023;
    /* 0 < a < 1 : not an integer */
    if (e < 0)
        goto not_an_integer;
    mant |= (uint64_t)1 << 52;
    if (e < 52) {
        shift = 52 - e;
        /* check that there is no fractional part */
        if (mant & (((uint64_t)1 << shift) - 1)) {
        not_an_integer:
            *pres = 1;
            return NULL;
        }
        mant >>= shift;
        e = 0;
    } else {
        e -= 52;
    }
    if (sgn)
        mant = -mant;
    /* the integer is mant*2^e */
    r = js_bigint_set_si64(&buf, (int64_t)mant);
    *pres = 0;
    return js_bigint_shl(ctx, r, e);
}

/* return -1, 0, 1 or (2) (unordered) */
static int js_bigint_float64_cmp(JSContext *ctx, const JSBigInt *a,
                                 double b)
{
    int b_sign, a_sign, e, f;
    uint64_t mant, b1, a_mant;

    b1 = float64_as_uint64(b);
    b_sign = b1 >> 63;
    e = (b1 >> 52) & ((1 << 11) - 1);
    mant = b1 & (((uint64_t)1 << 52) - 1);
    a_sign = js_bigint_sign(a);
    if (e == 2047) {
        if (mant != 0) {
            /* NaN */
            return 2;
        } else {
            /* +/- infinity */
            return 2 * b_sign - 1;
        }
    } else if (e == 0 && mant == 0) {
        /* b = +/-0 */
        if (a->len == 1 && a->tab[0] == 0)
            return 0;
        else
            return 1 - 2 * a_sign;
    } else if (a->len == 1 && a->tab[0] == 0) {
        /* a = 0, b != 0 */
        return 2 * b_sign - 1;
    } else if (a_sign != b_sign) {
        return 1 - 2 * a_sign;
    } else {
        e -= 1023;
        /* Note: handling denormals is not necessary because we
           compare to integers hence f >= 0 */
        /* compute f so that 2^f <= abs(a) < 2^(f+1) */
        a_mant = js_bigint_get_mant_exp(ctx, &f, a);
        if (f != e) {
            if (f < e)
                return -1;
            else
                return 1;
        } else {
            mant = (mant | ((uint64_t)1 << 52)) << 11; /* align to a_mant */
            if (a_mant < mant)
                return 2 * a_sign - 1;
            else if (a_mant > mant)
                return 1 - 2 * a_sign;
            else
                return 0;
        }
    }
}

/* return -1, 0 or 1 */
static int js_bigint_cmp(JSContext *ctx, const JSBigInt *a,
                         const JSBigInt *b)
{
    int a_sign, b_sign, res, i;
    a_sign = js_bigint_sign(a);
    b_sign = js_bigint_sign(b);
    if (a_sign != b_sign) {
        res = 1 - 2 * a_sign;
    } else {
        /* we assume the numbers are normalized */
        if (a->len != b->len) {
            if (a->len < b->len)
                res = 2 * a_sign - 1;
            else
                res = 1 - 2 * a_sign;
        } else {
            res = 0;
            for(i = a->len -1; i >= 0; i--) {
                if (a->tab[i] != b->tab[i]) {
                    if (a->tab[i] < b->tab[i])
                        res = -1;
                    else
                        res = 1;
                    break;
                }
            }
        }
    }
    return res;
}

/* contains 10^i */
static const js_limb_t js_pow_dec[JS_LIMB_DIGITS + 1] = {
    1U,
    10U,
    100U,
    1000U,
    10000U,
    100000U,
    1000000U,
    10000000U,
    100000000U,
    1000000000U,
#if JS_LIMB_BITS == 64
    10000000000U,
    100000000000U,
    1000000000000U,
    10000000000000U,
    100000000000000U,
    1000000000000000U,
    10000000000000000U,
    100000000000000000U,
    1000000000000000000U,
    10000000000000000000U,
#endif
};

/* syntax: [-]digits in base radix. Return NULL if memory error. radix
   = 10, 2, 8 or 16. */
static JSBigInt *js_bigint_from_string(JSContext *ctx,
                                       const char *str, int radix)
{
    const char *p = str;
    size_t n_digits1;
    int is_neg, n_digits, n_limbs, len, log2_radix, n_bits, i;
    JSBigInt *r;
    js_limb_t v, c, h;

    is_neg = 0;
    if (*p == '-') {
        is_neg = 1;
        p++;
    }
    while (*p == '0')
        p++;
    n_digits1 = strlen(p);
    /* the real check for overflox is done js_bigint_new(). Here
       we just avoid integer overflow */
    if (n_digits1 > JS_BIGINT_MAX_SIZE * JS_LIMB_BITS) {
        JS_ThrowRangeError(ctx, "BigInt is too large to allocate");
        return NULL;
    }
    n_digits = n_digits1;
    log2_radix = 32 - clz32(radix - 1); /* ceil(log2(radix)) */
    /* compute the maximum number of limbs */
    if (radix == 10) {
        n_bits = (n_digits * 27 + 7) / 8; /* >= ceil(n_digits * log2(10)) */
    } else {
        n_bits = n_digits * log2_radix;
    }
    /* we add one extra bit for the sign */
    n_limbs = max_int(1, n_bits / JS_LIMB_BITS + 1);
    r = js_bigint_new(ctx, n_limbs);
    if (!r)
        return NULL;
    if (radix == 10) {
        int digits_per_limb = JS_LIMB_DIGITS;
        len = 1;
        r->tab[0] = 0;
        for(;;) {
            /* XXX: slow */
            v = 0;
            for(i = 0; i < digits_per_limb; i++) {
                c = to_digit(*p);
                if (c >= radix)
                    break;
                p++;
                v = v * 10 + c;
            }
            if (i == 0)
                break;
            if (len == 1 && r->tab[0] == 0) {
                r->tab[0] = v;
            } else {
                h = mp_mul1(r->tab, r->tab, len, js_pow_dec[i], v);
                if (h != 0) {
                    r->tab[len++] = h;
                }
            }
        }
        /* add one extra limb to have the correct sign*/
        if ((r->tab[len - 1] >> (JS_LIMB_BITS - 1)) != 0)
            r->tab[len++] = 0;
        r->len = len;
    } else {
        unsigned int bit_pos, shift, pos;

        /* power of two base: no multiplication is needed */
        r->len = n_limbs;
        memset(r->tab, 0, sizeof(r->tab[0]) * n_limbs);
        for(i = 0; i < n_digits; i++) {
            c = to_digit(p[n_digits - 1 - i]);
            assert(c < radix);
            bit_pos = i * log2_radix;
            shift = bit_pos & (JS_LIMB_BITS - 1);
            pos = bit_pos / JS_LIMB_BITS;
            r->tab[pos] |= c << shift;
            /* if log2_radix does not divide JS_LIMB_BITS, needed an
               additional op */
            if (shift + log2_radix > JS_LIMB_BITS) {
                r->tab[pos + 1] |= c >> (JS_LIMB_BITS - shift);
            }
        }
    }
    r = js_bigint_normalize(ctx, r);
    /* XXX: could do it in place */
    if (is_neg) {
        JSBigInt *r1;
        r1 = js_bigint_neg(ctx, r);
        js_free(ctx, r);
        r = r1;
    }
    return r;
}

/* 2 <= base <= 36 */
static char const digits[36] = {
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b',
  'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
  'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z'
};

/* special version going backwards */
/* XXX: use dtoa.c */
static char *js_u64toa(char *q, int64_t n, unsigned int base)
{
    int digit;
    if (base == 10) {
        /* division by known base uses multiplication */
        do {
            digit = (uint64_t)n % 10;
            n = (uint64_t)n / 10;
            *--q = '0' + digit;
        } while (n != 0);
    } else {
        do {
            digit = (uint64_t)n % base;
            n = (uint64_t)n / base;
            *--q = digits[digit];
        } while (n != 0);
    }
    return q;
}

/* len >= 1. 2 <= radix <= 36 */
static char *limb_to_a(char *q, js_limb_t n, unsigned int radix, int len)
{
    int digit, i;

    if (radix == 10) {
        /* specific case with constant divisor */
        /* XXX: optimize */
        for(i = 0; i < len; i++) {
            digit = (js_limb_t)n % 10;
            n = (js_limb_t)n / 10;
            *--q = digit + '0';
        }
    } else {
        for(i = 0; i < len; i++) {
            digit = (js_limb_t)n % radix;
            n = (js_limb_t)n / radix;
            *--q = digits[digit];
        }
    }
    return q;
}

#define JS_RADIX_MAX 36

static const uint8_t digits_per_limb_table[JS_RADIX_MAX - 1] = {
#if JS_LIMB_BITS == 32
32,20,16,13,12,11,10,10, 9, 9, 8, 8, 8, 8, 8, 7, 7, 7, 7, 7, 7, 7, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
#else
64,40,32,27,24,22,21,20,19,18,17,17,16,16,16,15,15,15,14,14,14,14,13,13,13,13,13,13,13,12,12,12,12,12,12,
#endif
};

static const js_limb_t radix_base_table[JS_RADIX_MAX - 1] = {
#if JS_LIMB_BITS == 32
 0x00000000, 0xcfd41b91, 0x00000000, 0x48c27395,
 0x81bf1000, 0x75db9c97, 0x40000000, 0xcfd41b91,
 0x3b9aca00, 0x8c8b6d2b, 0x19a10000, 0x309f1021,
 0x57f6c100, 0x98c29b81, 0x00000000, 0x18754571,
 0x247dbc80, 0x3547667b, 0x4c4b4000, 0x6b5a6e1d,
 0x94ace180, 0xcaf18367, 0x0b640000, 0x0e8d4a51,
 0x1269ae40, 0x17179149, 0x1cb91000, 0x23744899,
 0x2b73a840, 0x34e63b41, 0x40000000, 0x4cfa3cc1,
 0x5c13d840, 0x6d91b519, 0x81bf1000,
#else
 0x0000000000000000, 0xa8b8b452291fe821, 0x0000000000000000, 0x6765c793fa10079d,
 0x41c21cb8e1000000, 0x3642798750226111, 0x8000000000000000, 0xa8b8b452291fe821,
 0x8ac7230489e80000, 0x4d28cb56c33fa539, 0x1eca170c00000000, 0x780c7372621bd74d,
 0x1e39a5057d810000, 0x5b27ac993df97701, 0x0000000000000000, 0x27b95e997e21d9f1,
 0x5da0e1e53c5c8000, 0xd2ae3299c1c4aedb, 0x16bcc41e90000000, 0x2d04b7fdd9c0ef49,
 0x5658597bcaa24000, 0xa0e2073737609371, 0x0c29e98000000000, 0x14adf4b7320334b9,
 0x226ed36478bfa000, 0x383d9170b85ff80b, 0x5a3c23e39c000000, 0x8e65137388122bcd,
 0xdd41bb36d259e000, 0x0aee5720ee830681, 0x1000000000000000, 0x172588ad4f5f0981,
 0x211e44f7d02c1000, 0x2ee56725f06e5c71, 0x41c21cb8e1000000,
#endif
};

static JSValue js_bigint_to_string1(JSContext *ctx, JSValueConst val, int radix)
{
    if (JS_VALUE_GET_TAG(val) == JS_TAG_SHORT_BIG_INT) {
        char buf[66];
        int len;
        len = i64toa_radix(buf, JS_VALUE_GET_SHORT_BIG_INT(val), radix);
        return js_new_string8_len(ctx, buf, len);
    } else {
        JSBigInt *r, *tmp = NULL;
        char *buf, *q, *buf_end;
        int is_neg, n_bits, log2_radix, n_digits;
        BOOL is_binary_radix;
        JSValue res;

        assert(JS_VALUE_GET_TAG(val) == JS_TAG_BIG_INT);
        r = JS_VALUE_GET_PTR(val);
        if (r->len == 1 && r->tab[0] == 0) {
            /* '0' case */
            return js_new_string8_len(ctx, "0", 1);
        }
        is_binary_radix = ((radix & (radix - 1)) == 0);
        is_neg = js_bigint_sign(r);
        if (is_neg) {
            tmp = js_bigint_neg(ctx, r);
            if (!tmp)
                return JS_EXCEPTION;
            r = tmp;
        } else if (!is_binary_radix) {
            /* need to modify 'r' */
            tmp = js_bigint_new(ctx, r->len);
            if (!tmp)
                return JS_EXCEPTION;
            memcpy(tmp->tab, r->tab, r->len * sizeof(r->tab[0]));
            r = tmp;
        }
        log2_radix = 31 - clz32(radix); /* floor(log2(radix)) */
        n_bits = r->len * JS_LIMB_BITS - js_limb_safe_clz(r->tab[r->len - 1]);
        /* n_digits is exact only if radix is a power of
           two. Otherwise it is >= the exact number of digits */
        n_digits = (n_bits + log2_radix - 1) / log2_radix;
        /* XXX: could directly build the JSString */
        buf = js_malloc(ctx, n_digits + is_neg + 1);
        if (!buf) {
            js_free(ctx, tmp);
            return JS_EXCEPTION;
        }
        q = buf + n_digits + is_neg + 1;
        *--q = '\0';
        buf_end = q;
        if (!is_binary_radix) {
            int len;
            js_limb_t radix_base, v;
            radix_base = radix_base_table[radix - 2];
            len = r->len;
            for(;;) {
                /* remove leading zero limbs */
                while (len > 1 && r->tab[len - 1] == 0)
                    len--;
                if (len == 1 && r->tab[0] < radix_base) {
                    v = r->tab[0];
                    if (v != 0) {
                        q = js_u64toa(q, v, radix);
                    }
                    break;
                } else {
                    v = mp_div1(r->tab, r->tab, len, radix_base, 0);
                    q = limb_to_a(q, v, radix, digits_per_limb_table[radix - 2]);
                }
            }
        } else {
            int i, shift;
            unsigned int bit_pos, pos, c;

            /* radix is a power of two */
            for(i = 0; i < n_digits; i++) {
                bit_pos = i * log2_radix;
                pos = bit_pos / JS_LIMB_BITS;
                shift = bit_pos % JS_LIMB_BITS;
                c = r->tab[pos] >> shift;
                if ((shift + log2_radix) > JS_LIMB_BITS &&
                    (pos + 1) < r->len) {
                    c |= r->tab[pos + 1] << (JS_LIMB_BITS - shift);
                }
                c &= (radix - 1);
                *--q = digits[c];
            }
        }
        if (is_neg)
            *--q = '-';
        js_free(ctx, tmp);
        res = js_new_string8_len(ctx, q, buf_end - q);
        js_free(ctx, buf);
        return res;
    }
}

/* if possible transform a BigInt to short big and free it, otherwise
   return a normal bigint */
static JSValue JS_CompactBigInt(JSContext *ctx, JSBigInt *p)
{
    JSValue res;
    if (p->len == 1) {
        res = __JS_NewShortBigInt(ctx, (js_slimb_t)p->tab[0]);
        js_free(ctx, p);
        return res;
    } else {
        return JS_MKPTR(JS_TAG_BIG_INT, p);
    }
}

#define ATOD_INT_ONLY        (1 << 0)
/* accept Oo and Ob prefixes in addition to 0x prefix if radix = 0 */
#define ATOD_ACCEPT_BIN_OCT  (1 << 2)
/* accept O prefix as octal if radix == 0 and properly formed (Annex B) */
#define ATOD_ACCEPT_LEGACY_OCTAL  (1 << 4)
/* accept _ between digits as a digit separator */
#define ATOD_ACCEPT_UNDERSCORES  (1 << 5)
/* allow a suffix to override the type */
#define ATOD_ACCEPT_SUFFIX    (1 << 6)
/* default type */
#define ATOD_TYPE_MASK        (3 << 7)
#define ATOD_TYPE_FLOAT64     (0 << 7)
#define ATOD_TYPE_BIG_INT     (1 << 7)
/* accept -0x1 */
#define ATOD_ACCEPT_PREFIX_AFTER_SIGN (1 << 10)

/* return an exception in case of memory error. Return JS_NAN if
   invalid syntax */
/* XXX: directly use js_atod() */
static JSValue js_atof(JSContext *ctx, const char *str, const char **pp,
                       int radix, int flags)
{
    const char *p, *p_start;
    int sep, is_neg;
    BOOL is_float, has_legacy_octal;
    int atod_type = flags & ATOD_TYPE_MASK;
    char buf1[64], *buf;
    int i, j, len;
    BOOL buf_allocated = FALSE;
    JSValue val;
    JSATODTempMem atod_mem;

    /* optional separator between digits */
    sep = (flags & ATOD_ACCEPT_UNDERSCORES) ? '_' : 256;
    has_legacy_octal = FALSE;

    p = str;
    p_start = p;
    is_neg = 0;
    if (p[0] == '+') {
        p++;
        p_start++;
        if (!(flags & ATOD_ACCEPT_PREFIX_AFTER_SIGN))
            goto no_radix_prefix;
    } else if (p[0] == '-') {
        p++;
        p_start++;
        is_neg = 1;
        if (!(flags & ATOD_ACCEPT_PREFIX_AFTER_SIGN))
            goto no_radix_prefix;
    }
    if (p[0] == '0') {
        if ((p[1] == 'x' || p[1] == 'X') &&
            (radix == 0 || radix == 16)) {
            p += 2;
            radix = 16;
        } else if ((p[1] == 'o' || p[1] == 'O') &&
                   radix == 0 && (flags & ATOD_ACCEPT_BIN_OCT)) {
            p += 2;
            radix = 8;
        } else if ((p[1] == 'b' || p[1] == 'B') &&
                   radix == 0 && (flags & ATOD_ACCEPT_BIN_OCT)) {
            p += 2;
            radix = 2;
        } else if ((p[1] >= '0' && p[1] <= '9') &&
                   radix == 0 && (flags & ATOD_ACCEPT_LEGACY_OCTAL)) {
            int i;
            has_legacy_octal = TRUE;
            sep = 256;
            for (i = 1; (p[i] >= '0' && p[i] <= '7'); i++)
                continue;
            if (p[i] == '8' || p[i] == '9')
                goto no_prefix;
            p += 1;
            radix = 8;
        } else {
            goto no_prefix;
        }
        /* there must be a digit after the prefix */
        if (to_digit((uint8_t)*p) >= radix)
            goto fail;
    no_prefix: ;
    } else {
 no_radix_prefix:
        if (!(flags & ATOD_INT_ONLY) &&
            (atod_type == ATOD_TYPE_FLOAT64) &&
            strstart(p, "Infinity", &p)) {
            double d = 1.0 / 0.0;
            if (is_neg)
                d = -d;
            val = JS_NewFloat64(ctx, d);
            goto done;
        }
    }
    if (radix == 0)
        radix = 10;
    is_float = FALSE;
    p_start = p;
    while (to_digit((uint8_t)*p) < radix
           ||  (*p == sep && (radix != 10 ||
                              p != p_start + 1 || p[-1] != '0') &&
                to_digit((uint8_t)p[1]) < radix)) {
        p++;
    }
    if (!(flags & ATOD_INT_ONLY) && radix == 10) {
        if (*p == '.' && (p > p_start || to_digit((uint8_t)p[1]) < radix)) {
            is_float = TRUE;
            p++;
            if (*p == sep)
                goto fail;
            while (to_digit((uint8_t)*p) < radix ||
                   (*p == sep && to_digit((uint8_t)p[1]) < radix))
                p++;
        }
        if (p > p_start && (*p == 'e' || *p == 'E')) {
            const char *p1 = p + 1;
            is_float = TRUE;
            if (*p1 == '+') {
                p1++;
            } else if (*p1 == '-') {
                p1++;
            }
            if (is_digit((uint8_t)*p1)) {
                p = p1 + 1;
                while (is_digit((uint8_t)*p) || (*p == sep && is_digit((uint8_t)p[1])))
                    p++;
            }
        }
    }
    if (p == p_start)
        goto fail;

    buf = buf1;
    buf_allocated = FALSE;
    len = p - p_start;
    if (unlikely((len + 2) > sizeof(buf1))) {
        buf = js_malloc_rt(ctx->rt, len + 2); /* no exception raised */
        if (!buf)
            goto mem_error;
        buf_allocated = TRUE;
    }
    /* remove the separators and the radix prefixes */
    j = 0;
    if (is_neg)
        buf[j++] = '-';
    for (i = 0; i < len; i++) {
        if (p_start[i] != '_')
            buf[j++] = p_start[i];
    }
    buf[j] = '\0';

    if ((flags & ATOD_ACCEPT_SUFFIX) && *p == 'n') {
        p++;
        atod_type = ATOD_TYPE_BIG_INT;
    }

    switch(atod_type) {
    case ATOD_TYPE_FLOAT64:
        {
            double d;
            d = js_atod(buf, NULL, radix, is_float ? 0 : JS_ATOD_INT_ONLY,
                        &atod_mem);
            /* return int or float64 */
            val = JS_NewFloat64(ctx, d);
        }
        break;
    case ATOD_TYPE_BIG_INT:
        {
            JSBigInt *r;
            if (has_legacy_octal || is_float)
                goto fail;
            r = js_bigint_from_string(ctx, buf, radix);
            if (!r) {
                val = JS_EXCEPTION;
                goto done;
            }
            val = JS_CompactBigInt(ctx, r);
        }
        break;
    default:
        abort();
    }

done:
    if (buf_allocated)
        js_free_rt(ctx->rt, buf);
    if (pp)
        *pp = p;
    return val;
 fail:
    val = JS_NAN;
    goto done;
 mem_error:
    val = JS_ThrowOutOfMemory(ctx);
    goto done;
}

typedef enum JSToNumberHintEnum {
    TON_FLAG_NUMBER,
    TON_FLAG_NUMERIC,
} JSToNumberHintEnum;

static JSValue JS_ToNumberHintFree(JSContext *ctx, JSValue val,
                                   JSToNumberHintEnum flag)
{
    uint32_t tag;
    JSValue ret;

 redo:
    tag = JS_VALUE_GET_NORM_TAG(val);
    switch(tag) {
    case JS_TAG_BIG_INT:
    case JS_TAG_SHORT_BIG_INT:
        if (flag != TON_FLAG_NUMERIC) {
            JS_FreeValue(ctx, val);
            return JS_ThrowTypeError(ctx, "cannot convert bigint to number");
        }
        ret = val;
        break;
    case JS_TAG_FLOAT64:
    case JS_TAG_INT:
    case JS_TAG_EXCEPTION:
        ret = val;
        break;
    case JS_TAG_BOOL:
    case JS_TAG_NULL:
        ret = JS_NewInt32(ctx, JS_VALUE_GET_INT(val));
        break;
    case JS_TAG_UNDEFINED:
        ret = JS_NAN;
        break;
    case JS_TAG_OBJECT:
        val = JS_ToPrimitiveFree(ctx, val, HINT_NUMBER);
        if (JS_IsException(val))
            return JS_EXCEPTION;
        goto redo;
    case JS_TAG_STRING:
    case JS_TAG_STRING_ROPE:
        {
            const char *str;
            const char *p;
            size_t len;

            str = JS_ToCStringLen(ctx, &len, val);
            JS_FreeValue(ctx, val);
            if (!str)
                return JS_EXCEPTION;
            p = str;
            p += skip_spaces(p);
            if ((p - str) == len) {
                ret = JS_NewInt32(ctx, 0);
            } else {
                int flags = ATOD_ACCEPT_BIN_OCT;
                ret = js_atof(ctx, p, &p, 0, flags);
                if (!JS_IsException(ret)) {
                    p += skip_spaces(p);
                    if ((p - str) != len) {
                        JS_FreeValue(ctx, ret);
                        ret = JS_NAN;
                    }
                }
            }
            JS_FreeCString(ctx, str);
        }
        break;
    case JS_TAG_SYMBOL:
        JS_FreeValue(ctx, val);
        return JS_ThrowTypeError(ctx, "cannot convert symbol to number");
    default:
        JS_FreeValue(ctx, val);
        ret = JS_NAN;
        break;
    }
    return ret;
}

static JSValue JS_ToNumberFree(JSContext *ctx, JSValue val)
{
    return JS_ToNumberHintFree(ctx, val, TON_FLAG_NUMBER);
}

static JSValue JS_ToNumericFree(JSContext *ctx, JSValue val)
{
    return JS_ToNumberHintFree(ctx, val, TON_FLAG_NUMERIC);
}

static JSValue JS_ToNumeric(JSContext *ctx, JSValueConst val)
{
    return JS_ToNumericFree(ctx, JS_DupValue(ctx, val));
}

static __exception int __JS_ToFloat64Free(JSContext *ctx, double *pres,
                                          JSValue val)
{
    double d;
    uint32_t tag;

    val = JS_ToNumberFree(ctx, val);
    if (JS_IsException(val))
        goto fail;
    tag = JS_VALUE_GET_NORM_TAG(val);
    switch(tag) {
    case JS_TAG_INT:
        d = JS_VALUE_GET_INT(val);
        break;
    case JS_TAG_FLOAT64:
        d = JS_VALUE_GET_FLOAT64(val);
        break;
    default:
        abort();
    }
    *pres = d;
    return 0;
 fail:
    *pres = JS_FLOAT64_NAN;
    return -1;
}

static inline int JS_ToFloat64Free(JSContext *ctx, double *pres, JSValue val)
{
    uint32_t tag;

    tag = JS_VALUE_GET_TAG(val);
    if (tag <= JS_TAG_NULL) {
        *pres = JS_VALUE_GET_INT(val);
        return 0;
    } else if (JS_TAG_IS_FLOAT64(tag)) {
        *pres = JS_VALUE_GET_FLOAT64(val);
        return 0;
    } else {
        return __JS_ToFloat64Free(ctx, pres, val);
    }
}

int JS_ToFloat64(JSContext *ctx, double *pres, JSValueConst val)
{
    return JS_ToFloat64Free(ctx, pres, JS_DupValue(ctx, val));
}

static JSValue JS_ToNumber(JSContext *ctx, JSValueConst val)
{
    return JS_ToNumberFree(ctx, JS_DupValue(ctx, val));
}

/* same as JS_ToNumber() but return 0 in case of NaN/Undefined */
static __maybe_unused JSValue JS_ToIntegerFree(JSContext *ctx, JSValue val)
{
    uint32_t tag;
    JSValue ret;

 redo:
    tag = JS_VALUE_GET_NORM_TAG(val);
    switch(tag) {
    case JS_TAG_INT:
    case JS_TAG_BOOL:
    case JS_TAG_NULL:
    case JS_TAG_UNDEFINED:
        ret = JS_NewInt32(ctx, JS_VALUE_GET_INT(val));
        break;
    case JS_TAG_FLOAT64:
        {
            double d = JS_VALUE_GET_FLOAT64(val);
            if (isnan(d)) {
                ret = JS_NewInt32(ctx, 0);
            } else {
                /* convert -0 to +0 */
                d = trunc(d) + 0.0;
                ret = JS_NewFloat64(ctx, d);
            }
        }
        break;
    default:
        val = JS_ToNumberFree(ctx, val);
        if (JS_IsException(val))
            return val;
        goto redo;
    }
    return ret;
}

/* Note: the integer value is satured to 32 bits */
static int JS_ToInt32SatFree(JSContext *ctx, int *pres, JSValue val)
{
    uint32_t tag;
    int ret;

 redo:
    tag = JS_VALUE_GET_NORM_TAG(val);
    switch(tag) {
    case JS_TAG_INT:
    case JS_TAG_BOOL:
    case JS_TAG_NULL:
    case JS_TAG_UNDEFINED:
        ret = JS_VALUE_GET_INT(val);
        break;
    case JS_TAG_EXCEPTION:
        *pres = 0;
        return -1;
    case JS_TAG_FLOAT64:
        {
            double d = JS_VALUE_GET_FLOAT64(val);
            if (isnan(d)) {
                ret = 0;
            } else {
                if (d < INT32_MIN)
                    ret = INT32_MIN;
                else if (d > INT32_MAX)
                    ret = INT32_MAX;
                else
                    ret = (int)d;
            }
        }
        break;
    default:
        val = JS_ToNumberFree(ctx, val);
        if (JS_IsException(val)) {
            *pres = 0;
            return -1;
        }
        goto redo;
    }
    *pres = ret;
    return 0;
}

int JS_ToInt32Sat(JSContext *ctx, int *pres, JSValueConst val)
{
    return JS_ToInt32SatFree(ctx, pres, JS_DupValue(ctx, val));
}

int JS_ToInt32Clamp(JSContext *ctx, int *pres, JSValueConst val,
                    int min, int max, int min_offset)
{
    int res = JS_ToInt32SatFree(ctx, pres, JS_DupValue(ctx, val));
    if (res == 0) {
        if (*pres < min) {
            *pres += min_offset;
            if (*pres < min)
                *pres = min;
        } else {
            if (*pres > max)
                *pres = max;
        }
    }
    return res;
}

static int JS_ToInt64SatFree(JSContext *ctx, int64_t *pres, JSValue val)
{
    uint32_t tag;

 redo:
    tag = JS_VALUE_GET_NORM_TAG(val);
    switch(tag) {
    case JS_TAG_INT:
    case JS_TAG_BOOL:
    case JS_TAG_NULL:
    case JS_TAG_UNDEFINED:
        *pres = JS_VALUE_GET_INT(val);
        return 0;
    case JS_TAG_EXCEPTION:
        *pres = 0;
        return -1;
    case JS_TAG_FLOAT64:
        {
            double d = JS_VALUE_GET_FLOAT64(val);
            if (isnan(d)) {
                *pres = 0;
            } else {
                if (d < INT64_MIN)
                    *pres = INT64_MIN;
                else if (d >= 0x1p63) /* must use INT64_MAX + 1 because INT64_MAX cannot be exactly represented as a double */
                    *pres = INT64_MAX;
                else
                    *pres = (int64_t)d;
            }
        }
        return 0;
    default:
        val = JS_ToNumberFree(ctx, val);
        if (JS_IsException(val)) {
            *pres = 0;
            return -1;
        }
        goto redo;
    }
}

int JS_ToInt64Sat(JSContext *ctx, int64_t *pres, JSValueConst val)
{
    return JS_ToInt64SatFree(ctx, pres, JS_DupValue(ctx, val));
}

int JS_ToInt64Clamp(JSContext *ctx, int64_t *pres, JSValueConst val,
                    int64_t min, int64_t max, int64_t neg_offset)
{
    int res = JS_ToInt64SatFree(ctx, pres, JS_DupValue(ctx, val));
    if (res == 0) {
        if (*pres < 0)
            *pres += neg_offset;
        if (*pres < min)
            *pres = min;
        else if (*pres > max)
            *pres = max;
    }
    return res;
}

/* Same as JS_ToInt32Free() but with a 64 bit result. Return (<0, 0)
   in case of exception */
static int JS_ToInt64Free(JSContext *ctx, int64_t *pres, JSValue val)
{
    uint32_t tag;
    int64_t ret;

 redo:
    tag = JS_VALUE_GET_NORM_TAG(val);
    switch(tag) {
    case JS_TAG_INT:
    case JS_TAG_BOOL:
    case JS_TAG_NULL:
    case JS_TAG_UNDEFINED:
        ret = JS_VALUE_GET_INT(val);
        break;
    case JS_TAG_FLOAT64:
        {
            JSFloat64Union u;
            double d;
            int e;
            d = JS_VALUE_GET_FLOAT64(val);
            u.d = d;
            /* we avoid doing fmod(x, 2^64) */
            e = (u.u64 >> 52) & 0x7ff;
            if (likely(e <= (1023 + 62))) {
                /* fast case */
                ret = (int64_t)d;
            } else if (e <= (1023 + 62 + 53)) {
                uint64_t v;
                /* remainder modulo 2^64 */
                v = (u.u64 & (((uint64_t)1 << 52) - 1)) | ((uint64_t)1 << 52);
                ret = v << ((e - 1023) - 52);
                /* take the sign into account */
                if (u.u64 >> 63)
                    ret = -ret;
            } else {
                ret = 0; /* also handles NaN and +inf */
            }
        }
        break;
    default:
        val = JS_ToNumberFree(ctx, val);
        if (JS_IsException(val)) {
            *pres = 0;
            return -1;
        }
        goto redo;
    }
    *pres = ret;
    return 0;
}

int JS_ToInt64(JSContext *ctx, int64_t *pres, JSValueConst val)
{
    return JS_ToInt64Free(ctx, pres, JS_DupValue(ctx, val));
}

int JS_ToInt64Ext(JSContext *ctx, int64_t *pres, JSValueConst val)
{
    if (JS_IsBigInt(ctx, val))
        return JS_ToBigInt64(ctx, pres, val);
    else
        return JS_ToInt64(ctx, pres, val);
}

/* return (<0, 0) in case of exception */
static int JS_ToInt32Free(JSContext *ctx, int32_t *pres, JSValue val)
{
    uint32_t tag;
    int32_t ret;

 redo:
    tag = JS_VALUE_GET_NORM_TAG(val);
    switch(tag) {
    case JS_TAG_INT:
    case JS_TAG_BOOL:
    case JS_TAG_NULL:
    case JS_TAG_UNDEFINED:
        ret = JS_VALUE_GET_INT(val);
        break;
    case JS_TAG_FLOAT64:
        {
            JSFloat64Union u;
            double d;
            int e;
            d = JS_VALUE_GET_FLOAT64(val);
            u.d = d;
            /* we avoid doing fmod(x, 2^32) */
            e = (u.u64 >> 52) & 0x7ff;
            if (likely(e <= (1023 + 30))) {
                /* fast case */
                ret = (int32_t)d;
            } else if (e <= (1023 + 30 + 53)) {
                uint64_t v;
                /* remainder modulo 2^32 */
                v = (u.u64 & (((uint64_t)1 << 52) - 1)) | ((uint64_t)1 << 52);
                v = v << ((e - 1023) - 52 + 32);
                ret = v >> 32;
                /* take the sign into account */
                if (u.u64 >> 63)
                    ret = -ret;
            } else {
                ret = 0; /* also handles NaN and +inf */
            }
        }
        break;
    default:
        val = JS_ToNumberFree(ctx, val);
        if (JS_IsException(val)) {
            *pres = 0;
            return -1;
        }
        goto redo;
    }
    *pres = ret;
    return 0;
}

int JS_ToInt32(JSContext *ctx, int32_t *pres, JSValueConst val)
{
    return JS_ToInt32Free(ctx, pres, JS_DupValue(ctx, val));
}

static inline int JS_ToUint32Free(JSContext *ctx, uint32_t *pres, JSValue val)
{
    return JS_ToInt32Free(ctx, (int32_t *)pres, val);
}

static int JS_ToUint8ClampFree(JSContext *ctx, int32_t *pres, JSValue val)
{
    uint32_t tag;
    int res;

 redo:
    tag = JS_VALUE_GET_NORM_TAG(val);
    switch(tag) {
    case JS_TAG_INT:
    case JS_TAG_BOOL:
    case JS_TAG_NULL:
    case JS_TAG_UNDEFINED:
        res = JS_VALUE_GET_INT(val);
        res = max_int(0, min_int(255, res));
        break;
    case JS_TAG_FLOAT64:
        {
            double d = JS_VALUE_GET_FLOAT64(val);
            if (isnan(d)) {
                res = 0;
            } else {
                if (d < 0)
                    res = 0;
                else if (d > 255)
                    res = 255;
                else
                    res = lrint(d);
            }
        }
        break;
    default:
        val = JS_ToNumberFree(ctx, val);
        if (JS_IsException(val)) {
            *pres = 0;
            return -1;
        }
        goto redo;
    }
    *pres = res;
    return 0;
}

static __exception int JS_ToArrayLengthFree(JSContext *ctx, uint32_t *plen,
                                            JSValue val, BOOL is_array_ctor)
{
    uint32_t tag, len;

    tag = JS_VALUE_GET_TAG(val);
    switch(tag) {
    case JS_TAG_INT:
    case JS_TAG_BOOL:
    case JS_TAG_NULL:
        {
            int v;
            v = JS_VALUE_GET_INT(val);
            if (v < 0)
                goto fail;
            len = v;
        }
        break;
    default:
        if (JS_TAG_IS_FLOAT64(tag)) {
            double d;
            d = JS_VALUE_GET_FLOAT64(val);
            if (!(d >= 0 && d <= UINT32_MAX))
                goto fail;
            len = (uint32_t)d;
            if (len != d)
                goto fail;
        } else {
            uint32_t len1;

            if (is_array_ctor) {
                val = JS_ToNumberFree(ctx, val);
                if (JS_IsException(val))
                    return -1;
                /* cannot recurse because val is a number */
                if (JS_ToArrayLengthFree(ctx, &len, val, TRUE))
                    return -1;
            } else {
                /* legacy behavior: must do the conversion twice and compare */
                if (JS_ToUint32(ctx, &len, val)) {
                    JS_FreeValue(ctx, val);
                    return -1;
                }
                val = JS_ToNumberFree(ctx, val);
                if (JS_IsException(val))
                    return -1;
                /* cannot recurse because val is a number */
                if (JS_ToArrayLengthFree(ctx, &len1, val, FALSE))
                    return -1;
                if (len1 != len) {
                fail:
                    JS_ThrowRangeError(ctx, "invalid array length");
                    return -1;
                }
            }
        }
        break;
    }
    *plen = len;
    return 0;
}

#define MAX_SAFE_INTEGER (((int64_t)1 << 53) - 1)

static BOOL is_safe_integer(double d)
{
    return isfinite(d) && floor(d) == d &&
        fabs(d) <= (double)MAX_SAFE_INTEGER;
}

int JS_ToIndex(JSContext *ctx, uint64_t *plen, JSValueConst val)
{
    int64_t v;
    if (JS_ToInt64Sat(ctx, &v, val))
        return -1;
    if (v < 0 || v > MAX_SAFE_INTEGER) {
        JS_ThrowRangeError(ctx, "invalid array index");
        *plen = 0;
        return -1;
    }
    *plen = v;
    return 0;
}

/* convert a value to a length between 0 and MAX_SAFE_INTEGER.
   return -1 for exception */
static __exception int JS_ToLengthFree(JSContext *ctx, int64_t *plen,
                                       JSValue val)
{
    int res = JS_ToInt64Clamp(ctx, plen, val, 0, MAX_SAFE_INTEGER, 0);
    JS_FreeValue(ctx, val);
    return res;
}

/* Note: can return an exception */
static int JS_NumberIsInteger(JSContext *ctx, JSValueConst val)
{
    double d;
    if (!JS_IsNumber(val))
        return FALSE;
    if (unlikely(JS_ToFloat64(ctx, &d, val)))
        return -1;
    return isfinite(d) && floor(d) == d;
}

static BOOL JS_NumberIsNegativeOrMinusZero(JSContext *ctx, JSValueConst val)
{
    uint32_t tag;

    tag = JS_VALUE_GET_NORM_TAG(val);
    switch(tag) {
    case JS_TAG_INT:
        {
            int v;
            v = JS_VALUE_GET_INT(val);
            return (v < 0);
        }
    case JS_TAG_FLOAT64:
        {
            JSFloat64Union u;
            u.d = JS_VALUE_GET_FLOAT64(val);
            return (u.u64 >> 63);
        }
    case JS_TAG_SHORT_BIG_INT:
        return (JS_VALUE_GET_SHORT_BIG_INT(val) < 0);
    case JS_TAG_BIG_INT:
        {
            JSBigInt *p = JS_VALUE_GET_PTR(val);
            return js_bigint_sign(p);
        }
    default:
        return FALSE;
    }
}

static JSValue js_bigint_to_string(JSContext *ctx, JSValueConst val)
{
    return js_bigint_to_string1(ctx, val, 10);
}

static JSValue js_dtoa2(JSContext *ctx,
                        double d, int radix, int n_digits, int flags)
{
    char static_buf[128], *buf, *tmp_buf;
    int len, len_max;
    JSValue res;
    JSDTOATempMem dtoa_mem;
    len_max = js_dtoa_max_len(d, radix, n_digits, flags);

    /* longer buffer may be used if radix != 10 */
    if (len_max > sizeof(static_buf) - 1) {
        tmp_buf = js_malloc(ctx, len_max + 1);
        if (!tmp_buf)
            return JS_EXCEPTION;
        buf = tmp_buf;
    } else {
        tmp_buf = NULL;
        buf = static_buf;
    }
    len = js_dtoa(buf, d, radix, n_digits, flags, &dtoa_mem);
    res = js_new_string8_len(ctx, buf, len);
    js_free(ctx, tmp_buf);
    return res;
}

static JSValue JS_ToStringInternal(JSContext *ctx, JSValueConst val, BOOL is_ToPropertyKey)
{
    uint32_t tag;
    char buf[32];

    tag = JS_VALUE_GET_NORM_TAG(val);
    switch(tag) {
    case JS_TAG_STRING:
        return JS_DupValue(ctx, val);
    case JS_TAG_STRING_ROPE:
        return js_linearize_string_rope(ctx, JS_DupValue(ctx, val));
    case JS_TAG_INT:
        {
            size_t len;
            len = i32toa(buf, JS_VALUE_GET_INT(val));
            return js_new_string8_len(ctx, buf, len);
        }
        break;
    case JS_TAG_BOOL:
        return JS_AtomToString(ctx, JS_VALUE_GET_BOOL(val) ?
                          JS_ATOM_true : JS_ATOM_false);
    case JS_TAG_NULL:
        return JS_AtomToString(ctx, JS_ATOM_null);
    case JS_TAG_UNDEFINED:
        return JS_AtomToString(ctx, JS_ATOM_undefined);
    case JS_TAG_EXCEPTION:
        return JS_EXCEPTION;
    case JS_TAG_OBJECT:
        {
            JSValue val1, ret;
            val1 = JS_ToPrimitive(ctx, val, HINT_STRING);
            if (JS_IsException(val1))
                return val1;
            ret = JS_ToStringInternal(ctx, val1, is_ToPropertyKey);
            JS_FreeValue(ctx, val1);
            return ret;
        }
        break;
    case JS_TAG_FUNCTION_BYTECODE:
        return js_new_string8(ctx, "[function bytecode]");
    case JS_TAG_SYMBOL:
        if (is_ToPropertyKey) {
            return JS_DupValue(ctx, val);
        } else {
            return JS_ThrowTypeError(ctx, "cannot convert symbol to string");
        }
    case JS_TAG_FLOAT64:
        return js_dtoa2(ctx, JS_VALUE_GET_FLOAT64(val), 10, 0,
                        JS_DTOA_FORMAT_FREE);
    case JS_TAG_SHORT_BIG_INT:
    case JS_TAG_BIG_INT:
        return js_bigint_to_string(ctx, val);
    default:
        return js_new_string8(ctx, "[unsupported type]");
    }
}

JSValue JS_ToString(JSContext *ctx, JSValueConst val)
{
    return JS_ToStringInternal(ctx, val, FALSE);
}

static JSValue JS_ToStringFree(JSContext *ctx, JSValue val)
{
    JSValue ret;
    ret = JS_ToString(ctx, val);
    JS_FreeValue(ctx, val);
    return ret;
}

static JSValue JS_ToLocaleStringFree(JSContext *ctx, JSValue val)
{
    if (JS_IsUndefined(val) || JS_IsNull(val))
        return JS_ToStringFree(ctx, val);
    return JS_InvokeFree(ctx, val, JS_ATOM_toLocaleString, 0, NULL);
}

JSValue JS_ToPropertyKey(JSContext *ctx, JSValueConst val)
{
    return JS_ToStringInternal(ctx, val, TRUE);
}

static JSValue JS_ToStringCheckObject(JSContext *ctx, JSValueConst val)
{
    uint32_t tag = JS_VALUE_GET_TAG(val);
    if (tag == JS_TAG_NULL || tag == JS_TAG_UNDEFINED)
        return JS_ThrowTypeError(ctx, "null or undefined are forbidden");
    return JS_ToString(ctx, val);
}

#define JS_PRINT_MAX_DEPTH 8

typedef struct {
    JSRuntime *rt;
    JSContext *ctx; /* may be NULL */
    JSPrintValueOptions options;
    JSPrintValueWrite *write_func;
    void *write_opaque;
    int level;
    JSObject *print_stack[JS_PRINT_MAX_DEPTH]; /* level values */
} JSPrintValueState;

static void js_print_value(JSPrintValueState *s, JSValueConst val);

static void js_putc(JSPrintValueState *s, char c)
{
    s->write_func(s->write_opaque, &c, 1);
}

static void js_puts(JSPrintValueState *s, const char *str)
{
    s->write_func(s->write_opaque, str, strlen(str));
}

static void __attribute__((format(printf, 2, 3))) js_printf(JSPrintValueState *s, const char *fmt, ...)
{
    va_list ap;
    char buf[256];

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    s->write_func(s->write_opaque, buf, strlen(buf));
}

static void js_print_float64(JSPrintValueState *s, double d)
{
    JSDTOATempMem dtoa_mem;
    char buf[32];
    int len;
    len = js_dtoa(buf, d, 10, 0, JS_DTOA_FORMAT_FREE | JS_DTOA_MINUS_ZERO, &dtoa_mem);
    s->write_func(s->write_opaque, buf, len);
}

static uint32_t js_string_get_length(JSValueConst val)
{
    if (JS_VALUE_GET_TAG(val) == JS_TAG_STRING) {
        JSString *p = JS_VALUE_GET_STRING(val);
        return p->len;
    } else if (JS_VALUE_GET_TAG(val) == JS_TAG_STRING_ROPE) {
        JSStringRope *r = JS_VALUE_GET_PTR(val);
        return r->len;
    } else {
        return 0;
    }
}

/* pretty print the first 'len' characters of 'p' */
static void js_print_string1(JSPrintValueState *s, JSString *p, int len, int sep)
{
    uint8_t buf[UTF8_CHAR_LEN_MAX];
    int l, i, c, c1;

    for(i = 0; i < len; i++) {
        c = string_get(p, i);
        switch(c) {
        case '\t':
            c = 't';
            goto quote;
        case '\r':
            c = 'r';
            goto quote;
        case '\n':
            c = 'n';
            goto quote;
        case '\b':
            c = 'b';
            goto quote;
        case '\f':
            c = 'f';
            goto quote;
        case '\\':
        quote:
            js_putc(s, '\\');
            js_putc(s, c);
            break;
        default:
            if (c == sep)
                goto quote;
            if (c >= 32 && c <= 126) {
                js_putc(s, c);
            } else if (c < 32 ||
                       (c >= 0x7f && c <= 0x9f)) {
            escape:
                js_printf(s, "\\u%04x", c);
            } else {
                if (is_hi_surrogate(c)) {
                    if ((i + 1) >= len)
                        goto escape;
                    c1 = string_get(p, i + 1);
                    if (!is_lo_surrogate(c1))
                        goto escape;
                    i++;
                    c = from_surrogate(c, c1);
                } else if (is_lo_surrogate(c)) {
                    goto escape;
                }
                l = unicode_to_utf8(buf, c);
                s->write_func(s->write_opaque, (char *)buf, l);
            }
            break;
        }
    }
}

static void js_print_string_rec(JSPrintValueState *s, JSValueConst val,
                                int sep, uint32_t pos)
{
    if (JS_VALUE_GET_TAG(val) == JS_TAG_STRING) {
        JSString *p = JS_VALUE_GET_STRING(val);
        uint32_t len;
        if (pos < s->options.max_string_length) {
            len = min_uint32(p->len, s->options.max_string_length - pos);
            js_print_string1(s, p, len, sep);
        }
    } else if (JS_VALUE_GET_TAG(val) == JS_TAG_STRING_ROPE) {
        JSStringRope *r = JS_VALUE_GET_PTR(val);
        js_print_string_rec(s, r->left, sep, pos);
        js_print_string_rec(s, r->right, sep, pos + js_string_get_length(r->left));
    } else {
        js_printf(s, "<invalid string tag %d>", (int)JS_VALUE_GET_TAG(val));
    }
}

static void js_print_string(JSPrintValueState *s, JSValueConst val)
{
    int sep;
    if (s->options.raw_dump && JS_VALUE_GET_TAG(val) == JS_TAG_STRING) {
        JSString *p = JS_VALUE_GET_STRING(val);
        js_printf(s, "%d", js_rc(p)->ref_count);
        sep = (js_rc(p)->ref_count == 1) ? '\"' : '\'';
    } else {
        sep = '\"';
    }
    js_putc(s, sep);
    js_print_string_rec(s, val, sep, 0);
    js_putc(s, sep);
    if (js_string_get_length(val) > s->options.max_string_length) {
        uint32_t n = js_string_get_length(val) - s->options.max_string_length;
        js_printf(s, "... %u more character%s", n, n > 1 ? "s" : "");
    }
}

static void js_print_raw_string(JSPrintValueState *s, JSValueConst val)
{
    const char *cstr;
    size_t len;
    cstr = JS_ToCStringLen(s->ctx, &len, val);
    if (cstr) {
        s->write_func(s->write_opaque, cstr, len);
        JS_FreeCString(s->ctx, cstr);
    }
}

static BOOL is_ascii_ident(const JSString *p)
{
    int i, c;

    if (p->len == 0)
        return FALSE;
    for(i = 0; i < p->len; i++) {
        c = string_get(p, i);
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c == '_' || c == '$') || (c >= '0' && c <= '9' && i > 0)))
            return FALSE;
    }
    return TRUE;
}

static void js_print_atom(JSPrintValueState *s, JSAtom atom)
{
    int i;
    if (__JS_AtomIsTaggedInt(atom)) {
        js_printf(s, "%u", __JS_AtomToUInt32(atom));
    } else if (atom == JS_ATOM_NULL) {
        js_puts(s, "<null>");
    } else {
        assert(atom < s->rt->atom_size);
        JSString *p;
        p = s->rt->atom_array[atom];
        if (is_ascii_ident(p)) {
            for(i = 0; i < p->len; i++) {
                js_putc(s, string_get(p, i));
            }
        } else {
            js_putc(s, '"');
            js_print_string1(s, p, p->len, '\"');
            js_putc(s, '"');
        }
    }
}

/* return 0 if invalid length */
static uint32_t js_print_array_get_length(JSObject *p)
{
    JSProperty *pr;
    JSShapeProperty *prs;
    JSValueConst val;

    prs = find_own_property(&pr, p, JS_ATOM_length);
    if (!prs)
        return 0;
    if ((prs->flags & JS_PROP_TMASK) != JS_PROP_NORMAL)
        return 0;
    val = pr->u.value;
    switch(JS_VALUE_GET_NORM_TAG(val)) {
    case JS_TAG_INT:
        return JS_VALUE_GET_INT(val);
    case JS_TAG_FLOAT64:
        return (uint32_t)JS_VALUE_GET_FLOAT64(val);
    default:
        return 0;
    }
}

static void js_print_comma(JSPrintValueState *s, int *pcomma_state)
{
    switch(*pcomma_state) {
    case 0:
        break;
    case 1:
        js_printf(s, ", ");
        break;
    case 2:
        js_printf(s, " { ");
        break;
    }
    *pcomma_state = 1;
}

static void js_print_more_items(JSPrintValueState *s, int *pcomma_state,
                                uint32_t n)
{
    js_print_comma(s, pcomma_state);
    js_printf(s, "... %u more item%s", n, n > 1 ? "s" : "");
}

/* similar to js_regexp_toString() but without side effect */
static void js_print_regexp(JSPrintValueState *s, JSObject *p1)
{
    JSRegExp *re = &p1->u.regexp;
    JSString *p;
    int i, n, c, c2, bra, flags;
    static const char regexp_flags[] = { 'g', 'i', 'm', 's', 'u', 'y', 'd', 'v' };

    if (!re->pattern || !re->bytecode) {
        /* the regexp fields are zeroed at init */
        js_puts(s, "[uninitialized_regexp]");
        return;
    }
    p = re->pattern;
    js_putc(s, '/');
    if (p->len == 0) {
        js_puts(s, "(?:)");
    } else {
        bra = 0;
        for (i = 0, n = p->len; i < n;) {
            c2 = -1;
            switch (c = string_get(p, i++)) {
            case '\\':
                if (i < n)
                    c2 = string_get(p, i++);
                break;
            case ']':
                bra = 0;
                break;
            case '[':
                if (!bra) {
                    if (i < n && string_get(p, i) == ']')
                        c2 = string_get(p, i++);
                    bra = 1;
                }
                break;
            case '\n':
                c = '\\';
                c2 = 'n';
                break;
            case '\r':
                c = '\\';
                c2 = 'r';
                break;
            case '/':
                if (!bra) {
                    c = '\\';
                    c2 = '/';
                }
                break;
            }
            js_putc(s, c);
            if (c2 >= 0)
                js_putc(s, c2);
        }
    }
    js_putc(s, '/');

    flags = lre_get_flags(re->bytecode->u.str8);
    for(i = 0; i < countof(regexp_flags); i++) {
        if ((flags >> i) & 1) {
            js_putc(s, regexp_flags[i]);
        }
    }
}

/* similar to js_error_toString() but without side effect */
static void js_print_error(JSPrintValueState *s, JSObject *p)
{
    const char *str;
    size_t len;

    str = get_prop_string(s->ctx, JS_MKPTR(JS_TAG_OBJECT, p), JS_ATOM_name);
    if (!str) {
        js_puts(s, "Error");
    } else {
        js_puts(s, str);
        JS_FreeCString(s->ctx, str);
    }

    str = get_prop_string(s->ctx, JS_MKPTR(JS_TAG_OBJECT, p), JS_ATOM_message);
    if (str && str[0] != '\0') {
        js_puts(s, ": ");
        js_puts(s, str);
    }
    JS_FreeCString(s->ctx, str);

    /* dump the stack if present */
    str = get_prop_string(s->ctx, JS_MKPTR(JS_TAG_OBJECT, p), JS_ATOM_stack);
    if (str) {
        js_putc(s, '\n');

        /* XXX: should remove the last '\n' in stack as
           v8. SpiderMonkey does not do it */
        len = strlen(str);
        if (len > 0 && str[len - 1] == '\n')
            len--;
        s->write_func(s->write_opaque, str, len);

        JS_FreeCString(s->ctx, str);
    }
}

static void js_print_object(JSPrintValueState *s, JSObject *p)
{
    JSRuntime *rt = s->rt;
    JSShape *sh;
    JSShapeProperty *prs;
    JSProperty *pr;
    int comma_state;
    BOOL is_array;
    uint32_t i;

    comma_state = 0;
    is_array = FALSE;
    if (p->class_id == JS_CLASS_ARRAY) {
        is_array = TRUE;
        js_printf(s, "[ ");
        /* XXX: print array like properties even if not fast array */
        if (p->fast_array) {
            uint32_t len, n, len1;
            len = js_print_array_get_length(p);

            len1 = min_uint32(p->u.array.count, s->options.max_item_count);
            for(i = 0; i < len1; i++) {
                js_print_comma(s, &comma_state);
                js_print_value(s, p->u.array.u.values[i]);
            }
            if (len1 < p->u.array.count)
                js_print_more_items(s, &comma_state, p->u.array.count - len1);
            if (p->u.array.count < len) {
                n = len - p->u.array.count;
                js_print_comma(s, &comma_state);
                js_printf(s, "<%u empty item%s>", n, n > 1 ? "s" : "");
            }
        }
    } else if (p->class_id >= JS_CLASS_UINT8C_ARRAY && p->class_id <= JS_CLASS_FLOAT64_ARRAY) {
        uint32_t size = 1 << typed_array_size_log2(p->class_id);
        uint32_t len1;
        int64_t v;

        js_print_atom(s, rt->class_array[p->class_id].class_name);
        js_printf(s, "(%u) [ ", p->u.array.count);

        is_array = TRUE;
        len1 = min_uint32(p->u.array.count, s->options.max_item_count);
        for(i = 0; i < len1; i++) {
            const uint8_t *ptr = p->u.array.u.uint8_ptr + i * size;
            js_print_comma(s, &comma_state);
            switch(p->class_id) {
            case JS_CLASS_UINT8C_ARRAY:
            case JS_CLASS_UINT8_ARRAY:
                v = *ptr;
                goto ta_int64;
            case JS_CLASS_INT8_ARRAY:
                v = *(int8_t *)ptr;
                goto ta_int64;
            case JS_CLASS_INT16_ARRAY:
                v = *(int16_t *)ptr;
                goto ta_int64;
            case JS_CLASS_UINT16_ARRAY:
                v = *(uint16_t *)ptr;
                goto ta_int64;
            case JS_CLASS_INT32_ARRAY:
                v = *(int32_t *)ptr;
                goto ta_int64;
            case JS_CLASS_UINT32_ARRAY:
                v = *(uint32_t *)ptr;
                goto ta_int64;
            case JS_CLASS_BIG_INT64_ARRAY:
                v = *(int64_t *)ptr;
            ta_int64:
                js_printf(s, "%" PRId64, v);
                break;
            case JS_CLASS_BIG_UINT64_ARRAY:
                js_printf(s, "%" PRIu64, *(uint64_t *)ptr);
                break;
            case JS_CLASS_FLOAT16_ARRAY:
                js_print_float64(s, fromfp16(*(uint16_t *)ptr));
                break;
            case JS_CLASS_FLOAT32_ARRAY:
                js_print_float64(s, *(float *)ptr);
                break;
            case JS_CLASS_FLOAT64_ARRAY:
                js_print_float64(s, *(double *)ptr);
                break;
            }
        }
        if (len1 < p->u.array.count)
            js_print_more_items(s, &comma_state, p->u.array.count - len1);
    } else if (p->class_id == JS_CLASS_BYTECODE_FUNCTION ||
               (rt->class_array[p->class_id].call != NULL &&
                p->class_id != JS_CLASS_PROXY)) {
        js_printf(s, "[Function");
        /* XXX: allow dump without ctx */
        if (!s->options.raw_dump && s->ctx) {
            const char *func_name_str;
            js_putc(s, ' ');
            func_name_str = get_prop_string(s->ctx, JS_MKPTR(JS_TAG_OBJECT, p), JS_ATOM_name);
            if (!func_name_str || func_name_str[0] == '\0')
                js_puts(s, "(anonymous)");
            else
                js_puts(s, func_name_str);
            JS_FreeCString(s->ctx, func_name_str);
        }
        js_printf(s, "]");
        comma_state = 2;
    } else if (p->class_id == JS_CLASS_MAP || p->class_id == JS_CLASS_SET) {
        JSMapState *ms = p->u.opaque;
        struct list_head *el;

        if (!ms)
            goto default_obj;
        js_print_atom(s, rt->class_array[p->class_id].class_name);
        js_printf(s, "(%u) { ", ms->record_count);
        i = 0;
        list_for_each(el, &ms->records) {
            JSMapRecord *mr = list_entry(el, JSMapRecord, link);
            js_print_comma(s, &comma_state);
            if (mr->empty)
                continue;
            js_print_value(s, mr->key);
            if (p->class_id == JS_CLASS_MAP) {
                js_printf(s, " => ");
                js_print_value(s, mr->value);
            }
            i++;
            if (i >= s->options.max_item_count)
                break;
        }
        if (i < ms->record_count)
            js_print_more_items(s, &comma_state, ms->record_count - i);
    } else if (p->class_id == JS_CLASS_REGEXP && s->ctx) {
        js_print_regexp(s, p);
        comma_state = 2;
    } else if (p->class_id == JS_CLASS_DATE && s->ctx) {
        /* get_date_string() has no side effect */
        JSValue str = get_date_string(s->ctx, JS_MKPTR(JS_TAG_OBJECT, p), 0, NULL, 0x23); /* toISOString() */
        if (JS_IsException(str))
            goto default_obj;
        js_print_raw_string(s, str);
        JS_FreeValueRT(s->rt, str);
        comma_state = 2;
    } else if (p->class_id == JS_CLASS_ERROR && s->ctx) {
        js_print_error(s, p);
        comma_state = 2;
    } else {
        default_obj:
        if (p->class_id != JS_CLASS_OBJECT) {
            js_print_atom(s, rt->class_array[p->class_id].class_name);
            js_printf(s, " ");
        }
        js_printf(s, "{ ");
    }

    sh = p->shape; /* the shape can be NULL while freeing an object */
    if (sh) {
        uint32_t j;

        j = 0;
        for(i = 0, prs = get_shape_prop(sh); i < sh->prop_count; i++, prs++) {
            if (prs->atom != JS_ATOM_NULL) {
                if (!(prs->flags & JS_PROP_ENUMERABLE) &&
                    !s->options.show_hidden) {
                    continue;
                }
                if (j < s->options.max_item_count) {
                    pr = &p->prop[i];
                    js_print_comma(s, &comma_state);
                    js_print_atom(s, prs->atom);
                    js_printf(s, ": ");

                    /* XXX: autoinit property */
                    if ((prs->flags & JS_PROP_TMASK) == JS_PROP_GETSET) {
                        if (s->options.raw_dump) {
                            js_printf(s, "[Getter %p Setter %p]",
                                    pr->u.getset.getter, pr->u.getset.setter);
                        } else {
                            if (pr->u.getset.getter && pr->u.getset.setter) {
                                js_printf(s, "[Getter/Setter]");
                            } else if (pr->u.getset.setter) {
                                js_printf(s, "[Setter]");
                            } else {
                                js_printf(s, "[Getter]");
                            }
                        }
                    } else if ((prs->flags & JS_PROP_TMASK) == JS_PROP_VARREF) {
                        if (s->options.raw_dump) {
                            js_printf(s, "[varref %p]", (void *)pr->u.var_ref);
                        } else {
                            js_print_value(s, *pr->u.var_ref->pvalue);
                        }
                    } else if ((prs->flags & JS_PROP_TMASK) == JS_PROP_AUTOINIT) {
                        if (s->options.raw_dump) {
                            js_printf(s, "[autoinit %p %d %p]",
                                    (void *)js_autoinit_get_realm(pr),
                                    js_autoinit_get_id(pr),
                                    (void *)pr->u.init.opaque);
                        } else {
                            /* XXX: could autoinit but need to restart
                               the iteration */
                            js_printf(s, "[autoinit]");
                        }
                    } else {
                        js_print_value(s, pr->u.value);
                    }
                }
                j++;
            }
        }
        if (j > s->options.max_item_count)
            js_print_more_items(s, &comma_state, j - s->options.max_item_count);
    }
    if (s->options.raw_dump && js_class_has_bytecode(p->class_id)) {
        JSFunctionBytecode *b = p->u.func.function_bytecode;
        if (b->closure_var_count) {
            JSVarRef **var_refs;
            var_refs = p->u.func.var_refs;

            js_print_comma(s, &comma_state);
            js_printf(s, "[[Closure]]: [");
            for(i = 0; i < b->closure_var_count; i++) {
                if (i != 0)
                    js_printf(s, ", ");
                js_print_value(s, var_refs[i]->value);
            }
            js_printf(s, " ]");
        }
        if (p->u.func.home_object) {
            js_print_comma(s, &comma_state);
            js_printf(s, "[[HomeObject]]: ");
            js_print_value(s, JS_MKPTR(JS_TAG_OBJECT, p->u.func.home_object));
        }
    }

    if (!is_array) {
        if (comma_state != 2) {
            js_printf(s, " }");
        }
    } else {
        js_printf(s, " ]");
    }
}

static int js_print_stack_index(JSPrintValueState *s, JSObject *p)
{
    int i;
    for(i = 0; i < s->level; i++)
        if (s->print_stack[i] == p)
            return i;
    return -1;
}

static void js_print_value(JSPrintValueState *s, JSValueConst val)
{
    uint32_t tag = JS_VALUE_GET_NORM_TAG(val);
    const char *str;

    switch(tag) {
    case JS_TAG_INT:
        js_printf(s, "%d", JS_VALUE_GET_INT(val));
        break;
    case JS_TAG_BOOL:
        if (JS_VALUE_GET_BOOL(val))
            str = "true";
        else
            str = "false";
        goto print_str;
    case JS_TAG_NULL:
        str = "null";
        goto print_str;
    case JS_TAG_EXCEPTION:
        str = "exception";
        goto print_str;
    case JS_TAG_UNINITIALIZED:
        str = "uninitialized";
        goto print_str;
    case JS_TAG_UNDEFINED:
        str = "undefined";
    print_str:
        js_puts(s, str);
        break;
    case JS_TAG_FLOAT64:
        js_print_float64(s, JS_VALUE_GET_FLOAT64(val));
        break;
    case JS_TAG_SHORT_BIG_INT:
        js_printf(s, "%" PRId64 "n", (int64_t)JS_VALUE_GET_SHORT_BIG_INT(val));
        break;
    case JS_TAG_BIG_INT:
        if (!s->options.raw_dump && s->ctx) {
            JSValue str = js_bigint_to_string(s->ctx, val);
            if (JS_IsException(str))
                goto raw_bigint;
            js_print_raw_string(s, str);
            js_putc(s, 'n');
            JS_FreeValueRT(s->rt, str);
        } else {
            JSBigInt *p;
            int sgn, i;
        raw_bigint:
            p = JS_VALUE_GET_PTR(val);
            /* In order to avoid allocations we just dump the limbs */
            sgn = js_bigint_sign(p);
            if (sgn)
                js_printf(s, "BigInt.asIntN(%d,", p->len * JS_LIMB_BITS);
            js_printf(s, "0x");
            for(i = p->len - 1; i >= 0; i--) {
                if (i != p->len - 1)
                    js_putc(s, '_');
#if JS_LIMB_BITS == 32
                js_printf(s, "%08x", p->tab[i]);
#else
                js_printf(s, "%016" PRIx64, p->tab[i]);
#endif
            }
            js_putc(s, 'n');
            if (sgn)
                js_putc(s, ')');
        }
        break;
    case JS_TAG_STRING:
    case JS_TAG_STRING_ROPE:
        if (s->options.raw_dump && tag == JS_TAG_STRING_ROPE) {
            JSStringRope *r = JS_VALUE_GET_STRING_ROPE(val);
            js_printf(s, "[rope len=%d depth=%d]", r->len, r->depth);
        } else {
            js_print_string(s, val);
        }
        break;
    case JS_TAG_FUNCTION_BYTECODE:
        {
            JSFunctionBytecode *b = JS_VALUE_GET_PTR(val);
            js_puts(s, "[bytecode ");
            js_print_atom(s, b->func_name);
            js_putc(s, ']');
        }
        break;
    case JS_TAG_OBJECT:
        {
            JSObject *p = JS_VALUE_GET_OBJ(val);
            int idx;
            idx = js_print_stack_index(s, p);
            if (idx >= 0) {
                js_printf(s, "[circular %d]", idx);
            } else if (s->level < s->options.max_depth) {
                s->print_stack[s->level++] = p;
                js_print_object(s, JS_VALUE_GET_OBJ(val));
                s->level--;
            } else {
                JSAtom atom = s->rt->class_array[p->class_id].class_name;
                js_putc(s, '[');
                js_print_atom(s, atom);
                if (s->options.raw_dump) {
                    js_printf(s, " %p", (void *)p);
                }
                js_putc(s, ']');
            }
        }
        break;
    case JS_TAG_SYMBOL:
        {
            JSAtomStruct *p = JS_VALUE_GET_PTR(val);
            js_puts(s, "Symbol(");
            js_print_atom(s, js_get_atom_index(s->rt, p));
            js_putc(s, ')');
        }
        break;
    case JS_TAG_MODULE:
        js_puts(s, "[module]");
        break;
    default:
        js_printf(s, "[unknown tag %d]", tag);
        break;
    }
}

void JS_PrintValueSetDefaultOptions(JSPrintValueOptions *options)
{
    memset(options, 0, sizeof(*options));
    options->max_depth = 2;
    options->max_string_length = 1000;
    options->max_item_count = 100;
}

static void JS_PrintValueInternal(JSRuntime *rt, JSContext *ctx,
                                  JSPrintValueWrite *write_func, void *write_opaque,
                                  JSValueConst val, const JSPrintValueOptions *options)
{
    JSPrintValueState ss, *s = &ss;
    if (options)
        s->options = *options;
    else
        JS_PrintValueSetDefaultOptions(&s->options);
    if (s->options.max_depth <= 0)
        s->options.max_depth = JS_PRINT_MAX_DEPTH;
    else
        s->options.max_depth = min_int(s->options.max_depth, JS_PRINT_MAX_DEPTH);
    if (s->options.max_string_length == 0)
        s->options.max_string_length = UINT32_MAX;
    if (s->options.max_item_count == 0)
        s->options.max_item_count = UINT32_MAX;
    s->rt = rt;
    s->ctx = ctx;
    s->write_func = write_func;
    s->write_opaque = write_opaque;
    s->level = 0;
    js_print_value(s, val);
}

void JS_PrintValueRT(JSRuntime *rt, JSPrintValueWrite *write_func, void *write_opaque,
                     JSValueConst val, const JSPrintValueOptions *options)
{
    JS_PrintValueInternal(rt, NULL, write_func, write_opaque, val, options);
}

void JS_PrintValue(JSContext *ctx, JSPrintValueWrite *write_func, void *write_opaque,
                   JSValueConst val, const JSPrintValueOptions *options)
{
    JS_PrintValueInternal(ctx->rt, ctx, write_func, write_opaque, val, options);
}

static void js_dump_value_write(void *opaque, const char *buf, size_t len)
{
    FILE *fo = opaque;
    fwrite(buf, 1, len, fo);
}

static __maybe_unused void print_atom(JSContext *ctx, JSAtom atom)
{
    JSPrintValueState ss, *s = &ss;
    memset(s, 0, sizeof(*s));
    s->rt = ctx->rt;
    s->ctx = ctx;
    s->write_func = js_dump_value_write;
    s->write_opaque = stdout;
    js_print_atom(s, atom);
}

static __maybe_unused void JS_DumpAtom(JSContext *ctx, const char *str, JSAtom atom)
{
    printf("%s=", str);
    print_atom(ctx, atom);
    printf("\n");
}

static __maybe_unused void JS_DumpValue(JSContext *ctx, const char *str, JSValueConst val)
{
    printf("%s=", str);
    JS_PrintValue(ctx, js_dump_value_write, stdout, val, NULL);
    printf("\n");
}

static __maybe_unused void JS_DumpValueRT(JSRuntime *rt, const char *str, JSValueConst val)
{
    printf("%s=", str);
    JS_PrintValueRT(rt, js_dump_value_write, stdout, val, NULL);
    printf("\n");
}

static __maybe_unused void JS_DumpObjectHeader(JSRuntime *rt)
{
    printf("%14s %4s %4s %14s %s\n",
           "ADDRESS", "REFS", "SHRF", "PROTO", "CONTENT");
}

/* for debug only: dump an object without side effect */
static __maybe_unused void JS_DumpObject(JSRuntime *rt, JSObject *p)
{
    JSShape *sh;
    JSPrintValueOptions options;

    /* XXX: should encode atoms with special characters */
    sh = p->shape; /* the shape can be NULL while freeing an object */
    printf("%14p %4d ",
           (void *)p,
           js_rc(p)->ref_count);
    if (sh) {
        printf("%3d%c %14p ",
               js_rc(sh)->ref_count,
               " *"[sh->is_hashed],
               (void *)sh->proto);
    } else {
        printf("%3s  %14s ", "-", "-");
    }

    JS_PrintValueSetDefaultOptions(&options);
    options.max_depth = 1;
    options.show_hidden = TRUE;
    options.raw_dump = TRUE;
    JS_PrintValueRT(rt, js_dump_value_write, stdout, JS_MKPTR(JS_TAG_OBJECT, p), &options);

    printf("\n");
}

static __maybe_unused void JS_DumpGCObject(JSRuntime *rt, JSGCObjectHeader *p)
{
    if (js_rc(p)->gc_obj_type == JS_GC_OBJ_TYPE_JS_OBJECT) {
        JS_DumpObject(rt, (JSObject *)p);
    } else {
        printf("%14p %4d ",
               (void *)p,
               js_rc(p)->ref_count);
        switch(js_rc(p)->gc_obj_type) {
        case JS_GC_OBJ_TYPE_FUNCTION_BYTECODE:
            printf("[function bytecode]");
            break;
        case JS_GC_OBJ_TYPE_SHAPE:
            printf("[shape]");
            break;
        case JS_GC_OBJ_TYPE_VAR_REF:
            printf("[var_ref]");
            break;
        case JS_GC_OBJ_TYPE_ASYNC_FUNCTION:
            printf("[async_function]");
            break;
        case JS_GC_OBJ_TYPE_JS_CONTEXT:
            printf("[js_context]");
            break;
        case JS_GC_OBJ_TYPE_MODULE:
            printf("[module]");
            break;
        default:
            printf("[unknown %d]", js_rc(p)->gc_obj_type);
            break;
        }
        printf("\n");
    }
}

/* return -1 if exception (proxy case) or TRUE/FALSE */
// TODO: should take flags to make proxy resolution and exceptions optional
int JS_IsArray(JSContext *ctx, JSValueConst val)
{
    if (js_resolve_proxy(ctx, &val, TRUE))
        return -1;
    if (JS_VALUE_GET_TAG(val) == JS_TAG_OBJECT) {
        JSObject *p = JS_VALUE_GET_OBJ(val);
        return p->class_id == JS_CLASS_ARRAY;
    } else {
        return FALSE;
    }
}

static double js_pow(double a, double b)
{
    if (unlikely(!isfinite(b)) && fabs(a) == 1) {
        /* not compatible with IEEE 754 */
        return JS_FLOAT64_NAN;
    } else {
        return pow(a, b);
    }
}

JSValue JS_NewBigInt64(JSContext *ctx, int64_t v)
{
#if JS_SHORT_BIG_INT_BITS == 64
    return __JS_NewShortBigInt(ctx, v);
#else
    if (v >= JS_SHORT_BIG_INT_MIN && v <= JS_SHORT_BIG_INT_MAX) {
        return __JS_NewShortBigInt(ctx, v);
    } else {
        JSBigInt *p;
        p = js_bigint_new_si64(ctx, v);
        if (!p)
            return JS_EXCEPTION;
        return JS_MKPTR(JS_TAG_BIG_INT, p);
    }
#endif
}

JSValue JS_NewBigUint64(JSContext *ctx, uint64_t v)
{
    if (v <= JS_SHORT_BIG_INT_MAX) {
        return __JS_NewShortBigInt(ctx, v);
    } else {
        JSBigInt *p;
        p = js_bigint_new_ui64(ctx, v);
        if (!p)
            return JS_EXCEPTION;
        return JS_MKPTR(JS_TAG_BIG_INT, p);
    }
}

/* return NaN if bad bigint literal */
static JSValue JS_StringToBigInt(JSContext *ctx, JSValue val)
{
    const char *str, *p;
    size_t len;
    int flags;

    str = JS_ToCStringLen(ctx, &len, val);
    JS_FreeValue(ctx, val);
    if (!str)
        return JS_EXCEPTION;
    p = str;
    p += skip_spaces(p);
    if ((p - str) == len) {
        val = JS_NewBigInt64(ctx, 0);
    } else {
        flags = ATOD_INT_ONLY | ATOD_ACCEPT_BIN_OCT | ATOD_TYPE_BIG_INT;
        val = js_atof(ctx, p, &p, 0, flags);
        p += skip_spaces(p);
        if (!JS_IsException(val)) {
            if ((p - str) != len) {
                JS_FreeValue(ctx, val);
                val = JS_NAN;
            }
        }
    }
    JS_FreeCString(ctx, str);
    return val;
}

static JSValue JS_StringToBigIntErr(JSContext *ctx, JSValue val)
{
    val = JS_StringToBigInt(ctx, val);
    if (JS_VALUE_IS_NAN(val))
        return JS_ThrowSyntaxError(ctx, "invalid bigint literal");
    return val;
}

/* JS Numbers are not allowed */
static JSValue JS_ToBigIntFree(JSContext *ctx, JSValue val)
{
    uint32_t tag;

 redo:
    tag = JS_VALUE_GET_NORM_TAG(val);
    switch(tag) {
    case JS_TAG_SHORT_BIG_INT:
    case JS_TAG_BIG_INT:
        break;
    case JS_TAG_INT:
    case JS_TAG_NULL:
    case JS_TAG_UNDEFINED:
    case JS_TAG_FLOAT64:
        goto fail;
    case JS_TAG_BOOL:
        val = __JS_NewShortBigInt(ctx, JS_VALUE_GET_INT(val));
        break;
    case JS_TAG_STRING:
    case JS_TAG_STRING_ROPE:
        val = JS_StringToBigIntErr(ctx, val);
        if (JS_IsException(val))
            return val;
        goto redo;
    case JS_TAG_OBJECT:
        val = JS_ToPrimitiveFree(ctx, val, HINT_NUMBER);
        if (JS_IsException(val))
            return val;
        goto redo;
    default:
    fail:
        JS_FreeValue(ctx, val);
        return JS_ThrowTypeError(ctx, "cannot convert to bigint");
    }
    return val;
}

static JSValue JS_ToBigInt(JSContext *ctx, JSValueConst val)
{
    return JS_ToBigIntFree(ctx, JS_DupValue(ctx, val));
}

/* XXX: merge with JS_ToInt64Free with a specific flag ? */
static int JS_ToBigInt64Free(JSContext *ctx, int64_t *pres, JSValue val)
{
    uint64_t res;

    val = JS_ToBigIntFree(ctx, val);
    if (JS_IsException(val)) {
        *pres = 0;
        return -1;
    }
    if (JS_VALUE_GET_TAG(val) == JS_TAG_SHORT_BIG_INT) {
        res = JS_VALUE_GET_SHORT_BIG_INT(val);
    } else {
        JSBigInt *p = JS_VALUE_GET_PTR(val);
        /* return the value mod 2^64 */
        res = p->tab[0];
#if JS_LIMB_BITS == 32
        if (p->len >= 2)
            res |= (uint64_t)p->tab[1] << 32;
#endif
        JS_FreeValue(ctx, val);
    }
    *pres = res;
    return 0;
}

int JS_ToBigInt64(JSContext *ctx, int64_t *pres, JSValueConst val)
{
    return JS_ToBigInt64Free(ctx, pres, JS_DupValue(ctx, val));
}

static no_inline __exception int js_unary_arith_slow(JSContext *ctx,
                                                     JSValue *sp,
                                                     OPCodeEnum op)
{
    JSValue op1;
    int v;
    uint32_t tag;
    JSBigIntBuf buf1;
    JSBigInt *p1;

    op1 = sp[-1];
    /* fast path for float64 */
    if (JS_TAG_IS_FLOAT64(JS_VALUE_GET_TAG(op1)))
        goto handle_float64;
    op1 = JS_ToNumericFree(ctx, op1);
    if (JS_IsException(op1))
        goto exception;
    tag = JS_VALUE_GET_TAG(op1);
    switch(tag) {
    case JS_TAG_INT:
        {
            int64_t v64;
            v64 = JS_VALUE_GET_INT(op1);
            switch(op) {
            case OP_inc:
            case OP_dec:
                v = 2 * (op - OP_dec) - 1;
                v64 += v;
                break;
            case OP_plus:
                break;
            case OP_neg:
                if (v64 == 0) {
                    sp[-1] = __JS_NewFloat64(ctx, -0.0);
                    return 0;
                } else {
                    v64 = -v64;
                }
                break;
            default:
                abort();
            }
            sp[-1] = JS_NewInt64(ctx, v64);
        }
        break;
    case JS_TAG_SHORT_BIG_INT:
        {
            int64_t v;
            v = JS_VALUE_GET_SHORT_BIG_INT(op1);
            switch(op) {
            case OP_plus:
                JS_ThrowTypeError(ctx, "bigint argument with unary +");
                goto exception;
            case OP_inc:
                if (v == JS_SHORT_BIG_INT_MAX)
                    goto bigint_slow_case;
                sp[-1] = __JS_NewShortBigInt(ctx, v + 1);
                break;
            case OP_dec:
                if (v == JS_SHORT_BIG_INT_MIN)
                    goto bigint_slow_case;
                sp[-1] = __JS_NewShortBigInt(ctx, v - 1);
                break;
            case OP_neg:
                v = JS_VALUE_GET_SHORT_BIG_INT(op1);
                if (v == JS_SHORT_BIG_INT_MIN) {
                bigint_slow_case:
                    p1 = js_bigint_set_short(&buf1, op1);
                    goto bigint_slow_case1;
                }
                sp[-1] = __JS_NewShortBigInt(ctx, -v);
                break;
            default:
                abort();
            }
        }
        break;
    case JS_TAG_BIG_INT:
        {
            JSBigInt *r;
            p1 = JS_VALUE_GET_PTR(op1);
        bigint_slow_case1:
            switch(op) {
            case OP_plus:
                JS_ThrowTypeError(ctx, "bigint argument with unary +");
                JS_FreeValue(ctx, op1);
                goto exception;
            case OP_inc:
            case OP_dec:
                {
                    JSBigIntBuf buf2;
                    JSBigInt *p2;
                    p2 = js_bigint_set_si(&buf2, 2 * (op - OP_dec) - 1);
                    r = js_bigint_add(ctx, p1, p2, 0);
                }
                break;
            case OP_neg:
                r = js_bigint_neg(ctx, p1);
                break;
            case OP_not:
                r = js_bigint_not(ctx, p1);
                break;
            default:
                abort();
            }
            JS_FreeValue(ctx, op1);
            if (!r)
                goto exception;
            sp[-1] = JS_CompactBigInt(ctx, r);
        }
        break;
    default:
    handle_float64:
        {
            double d;
            d = JS_VALUE_GET_FLOAT64(op1);
            switch(op) {
            case OP_inc:
            case OP_dec:
                v = 2 * (op - OP_dec) - 1;
                d += v;
                break;
            case OP_plus:
                break;
            case OP_neg:
                d = -d;
                break;
            default:
                abort();
            }
            sp[-1] = __JS_NewFloat64(ctx, d);
        }
        break;
    }
    return 0;
 exception:
    sp[-1] = JS_UNDEFINED;
    return -1;
}

static __exception int js_post_inc_slow(JSContext *ctx,
                                        JSValue *sp, OPCodeEnum op)
{
    JSValue op1;

    /* XXX: allow custom operators */
    op1 = sp[-1];
    op1 = JS_ToNumericFree(ctx, op1);
    if (JS_IsException(op1)) {
        sp[-1] = JS_UNDEFINED;
        return -1;
    }
    sp[-1] = op1;
    sp[0] = JS_DupValue(ctx, op1);
    return js_unary_arith_slow(ctx, sp + 1, op - OP_post_dec + OP_dec);
}

static no_inline int js_not_slow(JSContext *ctx, JSValue *sp)
{
    JSValue op1;

    op1 = sp[-1];
    op1 = JS_ToNumericFree(ctx, op1);
    if (JS_IsException(op1))
        goto exception;
    if (JS_VALUE_GET_TAG(op1) == JS_TAG_SHORT_BIG_INT) {
        sp[-1] = __JS_NewShortBigInt(ctx, ~JS_VALUE_GET_SHORT_BIG_INT(op1));
    } else if (JS_VALUE_GET_TAG(op1) == JS_TAG_BIG_INT) {
        JSBigInt *r;
        r = js_bigint_not(ctx, JS_VALUE_GET_PTR(op1));
        JS_FreeValue(ctx, op1);
        if (!r)
            goto exception;
        sp[-1] = JS_CompactBigInt(ctx, r);
    } else {
        int32_t v1;
        if (unlikely(JS_ToInt32Free(ctx, &v1, op1)))
            goto exception;
        sp[-1] = JS_NewInt32(ctx, ~v1);
    }
    return 0;
 exception:
    sp[-1] = JS_UNDEFINED;
    return -1;
}

static no_inline __exception int js_binary_arith_slow(JSContext *ctx, JSValue *sp,
                                                      OPCodeEnum op)
{
    JSValue op1, op2;
    uint32_t tag1, tag2;
    double d1, d2;

    op1 = sp[-2];
    op2 = sp[-1];
    tag1 = JS_VALUE_GET_NORM_TAG(op1);
    tag2 = JS_VALUE_GET_NORM_TAG(op2);
    /* fast path for float operations */
    if (tag1 == JS_TAG_FLOAT64 && tag2 == JS_TAG_FLOAT64) {
        d1 = JS_VALUE_GET_FLOAT64(op1);
        d2 = JS_VALUE_GET_FLOAT64(op2);
        goto handle_float64;
    }
    /* fast path for short big int operations */
    if (tag1 == JS_TAG_SHORT_BIG_INT && tag2 == JS_TAG_SHORT_BIG_INT) {
        js_slimb_t v1, v2;
        js_sdlimb_t v;
        v1 = JS_VALUE_GET_SHORT_BIG_INT(op1);
        v2 = JS_VALUE_GET_SHORT_BIG_INT(op2);
        switch(op) {
        case OP_sub:
            v = (js_sdlimb_t)v1 - (js_sdlimb_t)v2;
            break;
        case OP_mul:
            v = (js_sdlimb_t)v1 * (js_sdlimb_t)v2;
            break;
        case OP_div:
            if (v2 == 0 ||
                ((js_limb_t)v1 == (js_limb_t)1 << (JS_LIMB_BITS - 1) &&
                 v2 == -1)) {
                goto slow_big_int;
            }
            sp[-2] = __JS_NewShortBigInt(ctx, v1 / v2);
            return 0;
        case OP_mod:
            if (v2 == 0 ||
                ((js_limb_t)v1 == (js_limb_t)1 << (JS_LIMB_BITS - 1) &&
                 v2 == -1)) {
                goto slow_big_int;
            }
            sp[-2] = __JS_NewShortBigInt(ctx, v1 % v2);
            return 0;
        case OP_pow:
            goto slow_big_int;
        default:
            abort();
        }
        if (likely(v >= JS_SHORT_BIG_INT_MIN && v <= JS_SHORT_BIG_INT_MAX)) {
            sp[-2] = __JS_NewShortBigInt(ctx, v);
        } else {
            JSBigInt *r = js_bigint_new_di(ctx, v);
            if (!r)
                goto exception;
            sp[-2] = JS_MKPTR(JS_TAG_BIG_INT, r);
        }
        return 0;
    }
    op1 = JS_ToNumericFree(ctx, op1);
    if (JS_IsException(op1)) {
        JS_FreeValue(ctx, op2);
        goto exception;
    }
    op2 = JS_ToNumericFree(ctx, op2);
    if (JS_IsException(op2)) {
        JS_FreeValue(ctx, op1);
        goto exception;
    }
    tag1 = JS_VALUE_GET_NORM_TAG(op1);
    tag2 = JS_VALUE_GET_NORM_TAG(op2);

    if (tag1 == JS_TAG_INT && tag2 == JS_TAG_INT) {
        int32_t v1, v2;
        int64_t v;
        v1 = JS_VALUE_GET_INT(op1);
        v2 = JS_VALUE_GET_INT(op2);
        switch(op) {
        case OP_sub:
            v = (int64_t)v1 - (int64_t)v2;
            break;
        case OP_mul:
            v = (int64_t)v1 * (int64_t)v2;
            if (v == 0 && (v1 | v2) < 0) {
                sp[-2] = __JS_NewFloat64(ctx, -0.0);
                return 0;
            }
            break;
        case OP_div:
            sp[-2] = JS_NewFloat64(ctx, (double)v1 / (double)v2);
            return 0;
        case OP_mod:
            if (v1 < 0 || v2 <= 0) {
                sp[-2] = JS_NewFloat64(ctx, fmod(v1, v2));
                return 0;
            } else {
                v = (int64_t)v1 % (int64_t)v2;
            }
            break;
        case OP_pow:
            sp[-2] = JS_NewFloat64(ctx, js_pow(v1, v2));
            return 0;
        default:
            abort();
        }
        sp[-2] = JS_NewInt64(ctx, v);
    } else if ((tag1 == JS_TAG_SHORT_BIG_INT || tag1 == JS_TAG_BIG_INT) &&
               (tag2 == JS_TAG_SHORT_BIG_INT || tag2 == JS_TAG_BIG_INT)) {
        JSBigInt *p1, *p2, *r;
        JSBigIntBuf buf1, buf2;
    slow_big_int:
        /* bigint result */
        if (JS_VALUE_GET_TAG(op1) == JS_TAG_SHORT_BIG_INT)
            p1 = js_bigint_set_short(&buf1, op1);
        else
            p1 = JS_VALUE_GET_PTR(op1);
        if (JS_VALUE_GET_TAG(op2) == JS_TAG_SHORT_BIG_INT)
            p2 = js_bigint_set_short(&buf2, op2);
        else
            p2 = JS_VALUE_GET_PTR(op2);
        switch(op) {
        case OP_add:
            r = js_bigint_add(ctx, p1, p2, 0);
            break;
        case OP_sub:
            r = js_bigint_add(ctx, p1, p2, 1);
            break;
        case OP_mul:
            r = js_bigint_mul(ctx, p1, p2);
            break;
        case OP_div:
            r = js_bigint_divrem(ctx, p1, p2, FALSE);
            break;
        case OP_mod:
            r = js_bigint_divrem(ctx, p1, p2, TRUE);
            break;
        case OP_pow:
            r = js_bigint_pow(ctx, p1, p2);
            break;
        default:
            abort();
        }
        JS_FreeValue(ctx, op1);
        JS_FreeValue(ctx, op2);
        if (!r)
            goto exception;
        sp[-2] = JS_CompactBigInt(ctx, r);
    } else {
        double dr;
        /* float64 result */
        if (JS_ToFloat64Free(ctx, &d1, op1)) {
            JS_FreeValue(ctx, op2);
            goto exception;
        }
        if (JS_ToFloat64Free(ctx, &d2, op2))
            goto exception;
    handle_float64:
        switch(op) {
        case OP_sub:
            dr = d1 - d2;
            break;
        case OP_mul:
            dr = d1 * d2;
            break;
        case OP_div:
            dr = d1 / d2;
            break;
        case OP_mod:
            dr = fmod(d1, d2);
            break;
        case OP_pow:
            dr = js_pow(d1, d2);
            break;
        default:
            abort();
        }
        sp[-2] = __JS_NewFloat64(ctx, dr);
    }
    return 0;
 exception:
    sp[-2] = JS_UNDEFINED;
    sp[-1] = JS_UNDEFINED;
    return -1;
}

static inline BOOL tag_is_string(uint32_t tag)
{
    return tag == JS_TAG_STRING || tag == JS_TAG_STRING_ROPE;
}

static no_inline __exception int js_add_slow(JSContext *ctx, JSValue *sp)
{
    JSValue op1, op2;
    uint32_t tag1, tag2;

    op1 = sp[-2];
    op2 = sp[-1];

    tag1 = JS_VALUE_GET_NORM_TAG(op1);
    tag2 = JS_VALUE_GET_NORM_TAG(op2);
    /* fast path for float64 */
    if (tag1 == JS_TAG_FLOAT64 && tag2 == JS_TAG_FLOAT64) {
        double d1, d2;
        d1 = JS_VALUE_GET_FLOAT64(op1);
        d2 = JS_VALUE_GET_FLOAT64(op2);
        sp[-2] = __JS_NewFloat64(ctx, d1 + d2);
        return 0;
    }
    /* fast path for short bigint */
    if (tag1 == JS_TAG_SHORT_BIG_INT && tag2 == JS_TAG_SHORT_BIG_INT) {
        js_slimb_t v1, v2;
        js_sdlimb_t v;
        v1 = JS_VALUE_GET_SHORT_BIG_INT(op1);
        v2 = JS_VALUE_GET_SHORT_BIG_INT(op2);
        v = (js_sdlimb_t)v1 + (js_sdlimb_t)v2;
        if (likely(v >= JS_SHORT_BIG_INT_MIN && v <= JS_SHORT_BIG_INT_MAX)) {
            sp[-2] = __JS_NewShortBigInt(ctx, v);
        } else {
            JSBigInt *r = js_bigint_new_di(ctx, v);
            if (!r)
                goto exception;
            sp[-2] = JS_MKPTR(JS_TAG_BIG_INT, r);
        }
        return 0;
    }

    if (tag1 == JS_TAG_OBJECT || tag2 == JS_TAG_OBJECT) {
        op1 = JS_ToPrimitiveFree(ctx, op1, HINT_NONE);
        if (JS_IsException(op1)) {
            JS_FreeValue(ctx, op2);
            goto exception;
        }

        op2 = JS_ToPrimitiveFree(ctx, op2, HINT_NONE);
        if (JS_IsException(op2)) {
            JS_FreeValue(ctx, op1);
            goto exception;
        }
        tag1 = JS_VALUE_GET_NORM_TAG(op1);
        tag2 = JS_VALUE_GET_NORM_TAG(op2);
    }

    if (tag_is_string(tag1) || tag_is_string(tag2)) {
        sp[-2] = JS_ConcatString(ctx, op1, op2);
        if (JS_IsException(sp[-2]))
            goto exception;
        return 0;
    }

    op1 = JS_ToNumericFree(ctx, op1);
    if (JS_IsException(op1)) {
        JS_FreeValue(ctx, op2);
        goto exception;
    }
    op2 = JS_ToNumericFree(ctx, op2);
    if (JS_IsException(op2)) {
        JS_FreeValue(ctx, op1);
        goto exception;
    }
    tag1 = JS_VALUE_GET_NORM_TAG(op1);
    tag2 = JS_VALUE_GET_NORM_TAG(op2);

    if (tag1 == JS_TAG_INT && tag2 == JS_TAG_INT) {
        int32_t v1, v2;
        int64_t v;
        v1 = JS_VALUE_GET_INT(op1);
        v2 = JS_VALUE_GET_INT(op2);
        v = (int64_t)v1 + (int64_t)v2;
        sp[-2] = JS_NewInt64(ctx, v);
    } else if ((tag1 == JS_TAG_BIG_INT || tag1 == JS_TAG_SHORT_BIG_INT) &&
               (tag2 == JS_TAG_BIG_INT || tag2 == JS_TAG_SHORT_BIG_INT)) {
        JSBigInt *p1, *p2, *r;
        JSBigIntBuf buf1, buf2;
        /* bigint result */
        if (JS_VALUE_GET_TAG(op1) == JS_TAG_SHORT_BIG_INT)
            p1 = js_bigint_set_short(&buf1, op1);
        else
            p1 = JS_VALUE_GET_PTR(op1);
        if (JS_VALUE_GET_TAG(op2) == JS_TAG_SHORT_BIG_INT)
            p2 = js_bigint_set_short(&buf2, op2);
        else
            p2 = JS_VALUE_GET_PTR(op2);
        r = js_bigint_add(ctx, p1, p2, 0);
        JS_FreeValue(ctx, op1);
        JS_FreeValue(ctx, op2);
        if (!r)
            goto exception;
        sp[-2] = JS_CompactBigInt(ctx, r);
    } else {
        double d1, d2;
        /* float64 result */
        if (JS_ToFloat64Free(ctx, &d1, op1)) {
            JS_FreeValue(ctx, op2);
            goto exception;
        }
        if (JS_ToFloat64Free(ctx, &d2, op2))
            goto exception;
        sp[-2] = __JS_NewFloat64(ctx, d1 + d2);
    }
    return 0;
 exception:
    sp[-2] = JS_UNDEFINED;
    sp[-1] = JS_UNDEFINED;
    return -1;
}

static no_inline __exception int js_binary_logic_slow(JSContext *ctx,
                                                      JSValue *sp,
                                                      OPCodeEnum op)
{
    JSValue op1, op2;
    uint32_t tag1, tag2;
    uint32_t v1, v2, r;

    op1 = sp[-2];
    op2 = sp[-1];
    tag1 = JS_VALUE_GET_NORM_TAG(op1);
    tag2 = JS_VALUE_GET_NORM_TAG(op2);

    if (tag1 == JS_TAG_SHORT_BIG_INT && tag2 == JS_TAG_SHORT_BIG_INT) {
        js_slimb_t v1, v2, v;
        js_sdlimb_t vd;
        v1 = JS_VALUE_GET_SHORT_BIG_INT(op1);
        v2 = JS_VALUE_GET_SHORT_BIG_INT(op2);
        /* bigint fast path */
        switch(op) {
        case OP_and:
            v = v1 & v2;
            break;
        case OP_or:
            v = v1 | v2;
            break;
        case OP_xor:
            v = v1 ^ v2;
            break;
        case OP_sar:
            if (v2 > (JS_LIMB_BITS - 1)) {
                goto slow_big_int;
            } else if (v2 < 0) {
                if (v2 < -(JS_LIMB_BITS - 1))
                    goto slow_big_int;
                v2 = -v2;
                goto bigint_shl;
            }
        bigint_sar:
            v = v1 >> v2;
            break;
        case OP_shl:
            if (v2 > (JS_LIMB_BITS - 1)) {
                goto slow_big_int;
            } else if (v2 < 0) {
                if (v2 < -(JS_LIMB_BITS - 1))
                    goto slow_big_int;
                v2 = -v2;
                goto bigint_sar;
            }
        bigint_shl:
            vd = (js_dlimb_t)v1 << v2;
            if (likely(vd >= JS_SHORT_BIG_INT_MIN &&
                       vd <= JS_SHORT_BIG_INT_MAX)) {
                v = vd;
            } else {
                JSBigInt *r = js_bigint_new_di(ctx, vd);
                if (!r)
                    goto exception;
                sp[-2] = JS_MKPTR(JS_TAG_BIG_INT, r);
                return 0;
            }
            break;
        default:
            abort();
        }
        sp[-2] = __JS_NewShortBigInt(ctx, v);
        return 0;
    }
    op1 = JS_ToNumericFree(ctx, op1);
    if (JS_IsException(op1)) {
        JS_FreeValue(ctx, op2);
        goto exception;
    }
    op2 = JS_ToNumericFree(ctx, op2);
    if (JS_IsException(op2)) {
        JS_FreeValue(ctx, op1);
        goto exception;
    }

    tag1 = JS_VALUE_GET_TAG(op1);
    tag2 = JS_VALUE_GET_TAG(op2);
    if ((tag1 == JS_TAG_BIG_INT || tag1 == JS_TAG_SHORT_BIG_INT) &&
        (tag2 == JS_TAG_BIG_INT || tag2 == JS_TAG_SHORT_BIG_INT)) {
        JSBigInt *p1, *p2, *r;
        JSBigIntBuf buf1, buf2;
    slow_big_int:
        if (JS_VALUE_GET_TAG(op1) == JS_TAG_SHORT_BIG_INT)
            p1 = js_bigint_set_short(&buf1, op1);
        else
            p1 = JS_VALUE_GET_PTR(op1);
        if (JS_VALUE_GET_TAG(op2) == JS_TAG_SHORT_BIG_INT)
            p2 = js_bigint_set_short(&buf2, op2);
        else
            p2 = JS_VALUE_GET_PTR(op2);
        switch(op) {
        case OP_and:
        case OP_or:
        case OP_xor:
            r = js_bigint_logic(ctx, p1, p2, op);
            break;
        case OP_shl:
        case OP_sar:
            {
                js_slimb_t shift;
                shift = js_bigint_get_si_sat(p2);
                if (shift > INT32_MAX)
                    shift = INT32_MAX;
                else if (shift < -INT32_MAX)
                    shift = -INT32_MAX;
                if (op == OP_sar)
                    shift = -shift;
                if (shift >= 0)
                    r = js_bigint_shl(ctx, p1, shift);
                else
                    r = js_bigint_shr(ctx, p1, -shift);
            }
            break;
        default:
            abort();
        }
        JS_FreeValue(ctx, op1);
        JS_FreeValue(ctx, op2);
        if (!r)
            goto exception;
        sp[-2] = JS_CompactBigInt(ctx, r);
    } else {
        if (unlikely(JS_ToInt32Free(ctx, (int32_t *)&v1, op1))) {
            JS_FreeValue(ctx, op2);
            goto exception;
        }
        if (unlikely(JS_ToInt32Free(ctx, (int32_t *)&v2, op2)))
            goto exception;
        switch(op) {
        case OP_shl:
            r = v1 << (v2 & 0x1f);
            break;
        case OP_sar:
            r = (int)v1 >> (v2 & 0x1f);
            break;
        case OP_and:
            r = v1 & v2;
            break;
        case OP_or:
            r = v1 | v2;
            break;
        case OP_xor:
            r = v1 ^ v2;
            break;
        default:
            abort();
        }
        sp[-2] = JS_NewInt32(ctx, r);
    }
    return 0;
 exception:
    sp[-2] = JS_UNDEFINED;
    sp[-1] = JS_UNDEFINED;
    return -1;
}

/* op1 must be a bigint or int. */
static JSBigInt *JS_ToBigIntBuf(JSContext *ctx, JSBigIntBuf *buf1,
                                JSValue op1)
{
    JSBigInt *p1;

    switch(JS_VALUE_GET_TAG(op1)) {
    case JS_TAG_INT:
        p1 = js_bigint_set_si(buf1, JS_VALUE_GET_INT(op1));
        break;
    case JS_TAG_SHORT_BIG_INT:
        p1 = js_bigint_set_short(buf1, op1);
        break;
    case JS_TAG_BIG_INT:
        p1 = JS_VALUE_GET_PTR(op1);
        break;
    default:
        abort();
    }
    return p1;
}

/* op1 and op2 must be numeric types and at least one must be a
   bigint. No exception is generated. */
static int js_compare_bigint(JSContext *ctx, OPCodeEnum op,
                             JSValue op1, JSValue op2)
{
    int res, val, tag1, tag2;
    JSBigIntBuf buf1, buf2;
    JSBigInt *p1, *p2;

    tag1 = JS_VALUE_GET_NORM_TAG(op1);
    tag2 = JS_VALUE_GET_NORM_TAG(op2);
    if ((tag1 == JS_TAG_SHORT_BIG_INT || tag1 == JS_TAG_INT) &&
        (tag2 == JS_TAG_SHORT_BIG_INT || tag2 == JS_TAG_INT)) {
        /* fast path */
        js_slimb_t v1, v2;
        if (tag1 == JS_TAG_INT)
            v1 = JS_VALUE_GET_INT(op1);
        else
            v1 = JS_VALUE_GET_SHORT_BIG_INT(op1);
        if (tag2 == JS_TAG_INT)
            v2 = JS_VALUE_GET_INT(op2);
        else
            v2 = JS_VALUE_GET_SHORT_BIG_INT(op2);
        val = (v1 > v2) - (v1 < v2);
    } else {
        if (tag1 == JS_TAG_FLOAT64) {
            p2 = JS_ToBigIntBuf(ctx, &buf2, op2);
            val = js_bigint_float64_cmp(ctx, p2, JS_VALUE_GET_FLOAT64(op1));
            if (val == 2)
                goto unordered;
            val = -val;
        } else if (tag2 == JS_TAG_FLOAT64) {
            p1 = JS_ToBigIntBuf(ctx, &buf1, op1);
            val = js_bigint_float64_cmp(ctx, p1, JS_VALUE_GET_FLOAT64(op2));
            if (val == 2) {
            unordered:
                JS_FreeValue(ctx, op1);
                JS_FreeValue(ctx, op2);
                return FALSE;
            }
        } else {
            p1 = JS_ToBigIntBuf(ctx, &buf1, op1);
            p2 = JS_ToBigIntBuf(ctx, &buf2, op2);
            val = js_bigint_cmp(ctx, p1, p2);
        }
        JS_FreeValue(ctx, op1);
        JS_FreeValue(ctx, op2);
    }

    switch(op) {
    case OP_lt:
        res = val < 0;
        break;
    case OP_lte:
        res = val <= 0;
        break;
    case OP_gt:
        res = val > 0;
        break;
    case OP_gte:
        res = val >= 0;
        break;
    case OP_eq:
        res = val == 0;
        break;
    default:
        abort();
    }
    return res;
}

static no_inline int js_relational_slow(JSContext *ctx, JSValue *sp,
                                        OPCodeEnum op)
{
    JSValue op1, op2;
    int res;
    uint32_t tag1, tag2;

    op1 = sp[-2];
    op2 = sp[-1];
    tag1 = JS_VALUE_GET_NORM_TAG(op1);
    tag2 = JS_VALUE_GET_NORM_TAG(op2);
    op1 = JS_ToPrimitiveFree(ctx, op1, HINT_NUMBER);
    if (JS_IsException(op1)) {
        JS_FreeValue(ctx, op2);
        goto exception;
    }
    op2 = JS_ToPrimitiveFree(ctx, op2, HINT_NUMBER);
    if (JS_IsException(op2)) {
        JS_FreeValue(ctx, op1);
        goto exception;
    }
    tag1 = JS_VALUE_GET_NORM_TAG(op1);
    tag2 = JS_VALUE_GET_NORM_TAG(op2);

    if (tag_is_string(tag1) && tag_is_string(tag2)) {
        if (tag1 == JS_TAG_STRING && tag2 == JS_TAG_STRING) {
            res = js_string_compare(ctx, JS_VALUE_GET_STRING(op1),
                                    JS_VALUE_GET_STRING(op2));
        } else {
            res = js_string_rope_compare(ctx, op1, op2, FALSE);
        }
        switch(op) {
        case OP_lt:
            res = (res < 0);
            break;
        case OP_lte:
            res = (res <= 0);
            break;
        case OP_gt:
            res = (res > 0);
            break;
        default:
        case OP_gte:
            res = (res >= 0);
            break;
        }
        JS_FreeValue(ctx, op1);
        JS_FreeValue(ctx, op2);
    } else if ((tag1 <= JS_TAG_NULL || tag1 == JS_TAG_FLOAT64) &&
               (tag2 <= JS_TAG_NULL || tag2 == JS_TAG_FLOAT64)) {
        /* fast path for float64/int */
        goto float64_compare;
    } else {
        if ((((tag1 == JS_TAG_BIG_INT || tag1 == JS_TAG_SHORT_BIG_INT) &&
              tag_is_string(tag2)) ||
             ((tag2 == JS_TAG_BIG_INT || tag2 == JS_TAG_SHORT_BIG_INT) &&
              tag_is_string(tag1)))) {
            if (tag_is_string(tag1)) {
                op1 = JS_StringToBigInt(ctx, op1);
                if (JS_VALUE_GET_TAG(op1) != JS_TAG_BIG_INT &&
                    JS_VALUE_GET_TAG(op1) != JS_TAG_SHORT_BIG_INT)
                    goto invalid_bigint_string;
            }
            if (tag_is_string(tag2)) {
                op2 = JS_StringToBigInt(ctx, op2);
                if (JS_VALUE_GET_TAG(op2) != JS_TAG_BIG_INT &&
                    JS_VALUE_GET_TAG(op2) != JS_TAG_SHORT_BIG_INT) {
                invalid_bigint_string:
                    JS_FreeValue(ctx, op1);
                    JS_FreeValue(ctx, op2);
                    res = FALSE;
                    goto done;
                }
            }
        } else {
            op1 = JS_ToNumericFree(ctx, op1);
            if (JS_IsException(op1)) {
                JS_FreeValue(ctx, op2);
                goto exception;
            }
            op2 = JS_ToNumericFree(ctx, op2);
            if (JS_IsException(op2)) {
                JS_FreeValue(ctx, op1);
                goto exception;
            }
        }

        tag1 = JS_VALUE_GET_NORM_TAG(op1);
        tag2 = JS_VALUE_GET_NORM_TAG(op2);

        if (tag1 == JS_TAG_BIG_INT || tag1 == JS_TAG_SHORT_BIG_INT ||
            tag2 == JS_TAG_BIG_INT || tag2 == JS_TAG_SHORT_BIG_INT) {
            res = js_compare_bigint(ctx, op, op1, op2);
        } else {
            double d1, d2;

        float64_compare:
            /* can use floating point comparison */
            if (tag1 == JS_TAG_FLOAT64) {
                d1 = JS_VALUE_GET_FLOAT64(op1);
            } else {
                d1 = JS_VALUE_GET_INT(op1);
            }
            if (tag2 == JS_TAG_FLOAT64) {
                d2 = JS_VALUE_GET_FLOAT64(op2);
            } else {
                d2 = JS_VALUE_GET_INT(op2);
            }
            switch(op) {
            case OP_lt:
                res = (d1 < d2); /* if NaN return false */
                break;
            case OP_lte:
                res = (d1 <= d2); /* if NaN return false */
                break;
            case OP_gt:
                res = (d1 > d2); /* if NaN return false */
                break;
            default:
            case OP_gte:
                res = (d1 >= d2); /* if NaN return false */
                break;
            }
        }
    }
 done:
    sp[-2] = JS_NewBool(ctx, res);
    return 0;
 exception:
    sp[-2] = JS_UNDEFINED;
    sp[-1] = JS_UNDEFINED;
    return -1;
}

static BOOL tag_is_number(uint32_t tag)
{
    return (tag == JS_TAG_INT ||
            tag == JS_TAG_FLOAT64 ||
            tag == JS_TAG_BIG_INT || tag == JS_TAG_SHORT_BIG_INT);
}

static no_inline __exception int js_eq_slow(JSContext *ctx, JSValue *sp,
                                            BOOL is_neq)
{
    JSValue op1, op2;
    int res;
    uint32_t tag1, tag2;

    op1 = sp[-2];
    op2 = sp[-1];
 redo:
    tag1 = JS_VALUE_GET_NORM_TAG(op1);
    tag2 = JS_VALUE_GET_NORM_TAG(op2);
    if (tag_is_number(tag1) && tag_is_number(tag2)) {
        if (tag1 == JS_TAG_INT && tag2 == JS_TAG_INT) {
            res = JS_VALUE_GET_INT(op1) == JS_VALUE_GET_INT(op2);
        } else if ((tag1 == JS_TAG_FLOAT64 &&
                    (tag2 == JS_TAG_INT || tag2 == JS_TAG_FLOAT64)) ||
                   (tag2 == JS_TAG_FLOAT64 &&
                    (tag1 == JS_TAG_INT || tag1 == JS_TAG_FLOAT64))) {
            double d1, d2;
            if (tag1 == JS_TAG_FLOAT64) {
                d1 = JS_VALUE_GET_FLOAT64(op1);
            } else {
                d1 = JS_VALUE_GET_INT(op1);
            }
            if (tag2 == JS_TAG_FLOAT64) {
                d2 = JS_VALUE_GET_FLOAT64(op2);
            } else {
                d2 = JS_VALUE_GET_INT(op2);
            }
            res = (d1 == d2);
        } else {
            res = js_compare_bigint(ctx, OP_eq, op1, op2);
        }
    } else if (tag1 == tag2) {
        res = js_strict_eq2(ctx, op1, op2, JS_EQ_STRICT);
        JS_FreeValue(ctx, op1);
        JS_FreeValue(ctx, op2);
    } else if ((tag1 == JS_TAG_NULL && tag2 == JS_TAG_UNDEFINED) ||
               (tag2 == JS_TAG_NULL && tag1 == JS_TAG_UNDEFINED)) {
        res = TRUE;
    } else if (tag_is_string(tag1) && tag_is_string(tag2)) {
        /* needed when comparing strings and ropes */
        res = js_strict_eq2(ctx, op1, op2, JS_EQ_STRICT);
        JS_FreeValue(ctx, op1);
        JS_FreeValue(ctx, op2);
    } else if ((tag_is_string(tag1) && tag_is_number(tag2)) ||
               (tag_is_string(tag2) && tag_is_number(tag1))) {

        if (tag1 == JS_TAG_BIG_INT || tag1 == JS_TAG_SHORT_BIG_INT ||
            tag2 == JS_TAG_BIG_INT || tag2 == JS_TAG_SHORT_BIG_INT) {
            if (tag_is_string(tag1)) {
                op1 = JS_StringToBigInt(ctx, op1);
                if (JS_VALUE_GET_TAG(op1) != JS_TAG_BIG_INT &&
                    JS_VALUE_GET_TAG(op1) != JS_TAG_SHORT_BIG_INT)
                    goto invalid_bigint_string;
            }
            if (tag_is_string(tag2)) {
                op2 = JS_StringToBigInt(ctx, op2);
                if (JS_VALUE_GET_TAG(op2) != JS_TAG_BIG_INT &&
                    JS_VALUE_GET_TAG(op2) != JS_TAG_SHORT_BIG_INT ) {
                invalid_bigint_string:
                    JS_FreeValue(ctx, op1);
                    JS_FreeValue(ctx, op2);
                    res = FALSE;
                    goto done;
                }
            }
        } else {
            op1 = JS_ToNumericFree(ctx, op1);
            if (JS_IsException(op1)) {
                JS_FreeValue(ctx, op2);
                goto exception;
            }
            op2 = JS_ToNumericFree(ctx, op2);
            if (JS_IsException(op2)) {
                JS_FreeValue(ctx, op1);
                goto exception;
            }
        }
        res = js_strict_eq2(ctx, op1, op2, JS_EQ_STRICT);
        JS_FreeValue(ctx, op1);
        JS_FreeValue(ctx, op2);
    } else if (tag1 == JS_TAG_BOOL) {
        op1 = JS_NewInt32(ctx, JS_VALUE_GET_INT(op1));
        goto redo;
    } else if (tag2 == JS_TAG_BOOL) {
        op2 = JS_NewInt32(ctx, JS_VALUE_GET_INT(op2));
        goto redo;
    } else if ((tag1 == JS_TAG_OBJECT &&
                (tag_is_number(tag2) || tag_is_string(tag2) || tag2 == JS_TAG_SYMBOL)) ||
               (tag2 == JS_TAG_OBJECT &&
                (tag_is_number(tag1) || tag_is_string(tag1) || tag1 == JS_TAG_SYMBOL))) {
        op1 = JS_ToPrimitiveFree(ctx, op1, HINT_NONE);
        if (JS_IsException(op1)) {
            JS_FreeValue(ctx, op2);
            goto exception;
        }
        op2 = JS_ToPrimitiveFree(ctx, op2, HINT_NONE);
        if (JS_IsException(op2)) {
            JS_FreeValue(ctx, op1);
            goto exception;
        }
        goto redo;
    } else {
        /* IsHTMLDDA object is equivalent to undefined for '==' and '!=' */
        if ((JS_IsHTMLDDA(ctx, op1) &&
             (tag2 == JS_TAG_NULL || tag2 == JS_TAG_UNDEFINED)) ||
            (JS_IsHTMLDDA(ctx, op2) &&
             (tag1 == JS_TAG_NULL || tag1 == JS_TAG_UNDEFINED))) {
            res = TRUE;
        } else {
            res = FALSE;
        }
        JS_FreeValue(ctx, op1);
        JS_FreeValue(ctx, op2);
    }
 done:
    sp[-2] = JS_NewBool(ctx, res ^ is_neq);
    return 0;
 exception:
    sp[-2] = JS_UNDEFINED;
    sp[-1] = JS_UNDEFINED;
    return -1;
}

static no_inline int js_shr_slow(JSContext *ctx, JSValue *sp)
{
    JSValue op1, op2;
    uint32_t v1, v2, r;

    op1 = sp[-2];
    op2 = sp[-1];
    op1 = JS_ToNumericFree(ctx, op1);
    if (JS_IsException(op1)) {
        JS_FreeValue(ctx, op2);
        goto exception;
    }
    op2 = JS_ToNumericFree(ctx, op2);
    if (JS_IsException(op2)) {
        JS_FreeValue(ctx, op1);
        goto exception;
    }
    if (JS_VALUE_GET_TAG(op1) == JS_TAG_BIG_INT ||
        JS_VALUE_GET_TAG(op1) == JS_TAG_SHORT_BIG_INT ||
        JS_VALUE_GET_TAG(op2) == JS_TAG_BIG_INT ||
        JS_VALUE_GET_TAG(op2) == JS_TAG_SHORT_BIG_INT) {
        JS_ThrowTypeError(ctx, "bigint operands are forbidden for >>>");
        JS_FreeValue(ctx, op1);
        JS_FreeValue(ctx, op2);
        goto exception;
    }
    /* cannot give an exception */
    JS_ToUint32Free(ctx, &v1, op1);
    JS_ToUint32Free(ctx, &v2, op2);
    r = v1 >> (v2 & 0x1f);
    sp[-2] = JS_NewUint32(ctx, r);
    return 0;
 exception:
    sp[-2] = JS_UNDEFINED;
    sp[-1] = JS_UNDEFINED;
    return -1;
}

static BOOL js_strict_eq2(JSContext *ctx, JSValueConst op1, JSValueConst op2,
                          JSStrictEqModeEnum eq_mode)
{
    BOOL res;
    int tag1, tag2;
    double d1, d2;

    tag1 = JS_VALUE_GET_NORM_TAG(op1);
    tag2 = JS_VALUE_GET_NORM_TAG(op2);
    switch(tag1) {
    case JS_TAG_BOOL:
        if (tag1 != tag2) {
            res = FALSE;
        } else {
            res = JS_VALUE_GET_INT(op1) == JS_VALUE_GET_INT(op2);
        }
        break;
    case JS_TAG_NULL:
    case JS_TAG_UNDEFINED:
        res = (tag1 == tag2);
        break;
    case JS_TAG_STRING:
    case JS_TAG_STRING_ROPE:
        {
            if (!tag_is_string(tag2)) {
                res = FALSE;
            } else if (tag1 == JS_TAG_STRING && tag2 == JS_TAG_STRING) {
                res = js_string_eq(ctx, JS_VALUE_GET_STRING(op1),
                                   JS_VALUE_GET_STRING(op2));
            } else {
                res = (js_string_rope_compare(ctx, op1, op2, TRUE) == 0);
            }
        }
        break;
    case JS_TAG_SYMBOL:
        {
            JSAtomStruct *p1, *p2;
            if (tag1 != tag2) {
                res = FALSE;
            } else {
                p1 = JS_VALUE_GET_PTR(op1);
                p2 = JS_VALUE_GET_PTR(op2);
                res = (p1 == p2);
            }
        }
        break;
    case JS_TAG_OBJECT:
        if (tag1 != tag2)
            res = FALSE;
        else
            res = JS_VALUE_GET_OBJ(op1) == JS_VALUE_GET_OBJ(op2);
        break;
    case JS_TAG_INT:
        d1 = JS_VALUE_GET_INT(op1);
        if (tag2 == JS_TAG_INT) {
            d2 = JS_VALUE_GET_INT(op2);
            goto number_test;
        } else if (tag2 == JS_TAG_FLOAT64) {
            d2 = JS_VALUE_GET_FLOAT64(op2);
            goto number_test;
        } else {
            res = FALSE;
        }
        break;
    case JS_TAG_FLOAT64:
        d1 = JS_VALUE_GET_FLOAT64(op1);
        if (tag2 == JS_TAG_FLOAT64) {
            d2 = JS_VALUE_GET_FLOAT64(op2);
        } else if (tag2 == JS_TAG_INT) {
            d2 = JS_VALUE_GET_INT(op2);
        } else {
            res = FALSE;
            break;
        }
    number_test:
        if (unlikely(eq_mode >= JS_EQ_SAME_VALUE)) {
            JSFloat64Union u1, u2;
            /* NaN is not always normalized, so this test is necessary */
            if (isnan(d1) || isnan(d2)) {
                res = isnan(d1) == isnan(d2);
            } else if (eq_mode == JS_EQ_SAME_VALUE_ZERO) {
                res = (d1 == d2); /* +0 == -0 */
            } else {
                u1.d = d1;
                u2.d = d2;
                res = (u1.u64 == u2.u64); /* +0 != -0 */
            }
        } else {
            res = (d1 == d2); /* if NaN return false and +0 == -0 */
        }
        break;
    case JS_TAG_SHORT_BIG_INT:
    case JS_TAG_BIG_INT:
        {
            JSBigIntBuf buf1, buf2;
            JSBigInt *p1, *p2;

            if (tag2 != JS_TAG_SHORT_BIG_INT &&
                tag2 != JS_TAG_BIG_INT) {
                res = FALSE;
                break;
            }

            if (JS_VALUE_GET_TAG(op1) == JS_TAG_SHORT_BIG_INT)
                p1 = js_bigint_set_short(&buf1, op1);
            else
                p1 = JS_VALUE_GET_PTR(op1);
            if (JS_VALUE_GET_TAG(op2) == JS_TAG_SHORT_BIG_INT)
                p2 = js_bigint_set_short(&buf2, op2);
            else
                p2 = JS_VALUE_GET_PTR(op2);
            res = (js_bigint_cmp(ctx, p1, p2) == 0);
        }
        break;
    default:
        res = FALSE;
        break;
    }
    return res;
}

static BOOL js_strict_eq(JSContext *ctx, JSValueConst op1, JSValueConst op2)
{
    return js_strict_eq2(ctx, op1, op2, JS_EQ_STRICT);
}

BOOL JS_StrictEq(JSContext *ctx, JSValueConst op1, JSValueConst op2)
{
    return js_strict_eq(ctx, op1, op2);
}

static BOOL js_same_value(JSContext *ctx, JSValueConst op1, JSValueConst op2)
{
    return js_strict_eq2(ctx, op1, op2, JS_EQ_SAME_VALUE);
}

BOOL JS_SameValue(JSContext *ctx, JSValueConst op1, JSValueConst op2)
{
    return js_same_value(ctx, op1, op2);
}

static BOOL js_same_value_zero(JSContext *ctx, JSValueConst op1, JSValueConst op2)
{
    return js_strict_eq2(ctx, op1, op2, JS_EQ_SAME_VALUE_ZERO);
}

BOOL JS_SameValueZero(JSContext *ctx, JSValueConst op1, JSValueConst op2)
{
    return js_same_value_zero(ctx, op1, op2);
}

static __exception int js_operator_in(JSContext *ctx, JSValue *sp)
{
    JSValue op1, op2;
    JSAtom atom;
    int ret;

    op1 = sp[-2];
    op2 = sp[-1];

    if (JS_VALUE_GET_TAG(op2) != JS_TAG_OBJECT) {
        JS_ThrowTypeError(ctx, "invalid 'in' operand");
        return -1;
    }
    atom = JS_ValueToAtom(ctx, op1);
    if (unlikely(atom == JS_ATOM_NULL))
        return -1;
    ret = JS_HasProperty(ctx, op2, atom);
    JS_FreeAtom(ctx, atom);
    if (ret < 0)
        return -1;
    JS_FreeValue(ctx, op1);
    JS_FreeValue(ctx, op2);
    sp[-2] = JS_NewBool(ctx, ret);
    return 0;
}

static __exception int js_operator_private_in(JSContext *ctx, JSValue *sp)
{
    JSValue op1, op2;
    int ret;

    op1 = sp[-2]; /* object */
    op2 = sp[-1]; /* field name or method function */

    if (JS_VALUE_GET_TAG(op1) != JS_TAG_OBJECT) {
        JS_ThrowTypeError(ctx, "invalid 'in' operand");
        return -1;
    }
    if (JS_IsObject(op2)) {
        /* method: use the brand */
        ret = JS_CheckBrand(ctx, op1, op2);
        if (ret < 0)
            return -1;
    } else {
        JSAtom atom;
        JSObject *p;
        JSShapeProperty *prs;
        JSProperty *pr;
        /* field */
        atom = JS_ValueToAtom(ctx, op2);
        if (unlikely(atom == JS_ATOM_NULL))
            return -1;
        p = JS_VALUE_GET_OBJ(op1);
        prs = find_own_property(&pr, p, atom);
        JS_FreeAtom(ctx, atom);
        ret = (prs != NULL);
    }
    JS_FreeValue(ctx, op1);
    JS_FreeValue(ctx, op2);
    sp[-2] = JS_NewBool(ctx, ret);
    return 0;
}

static __exception int js_has_unscopable(JSContext *ctx, JSValueConst obj,
                                         JSAtom atom)
{
    JSValue arr, val;
    int ret;

    arr = JS_GetProperty(ctx, obj, JS_ATOM_Symbol_unscopables);
    if (JS_IsException(arr))
        return -1;
    ret = 0;
    if (JS_IsObject(arr)) {
        val = JS_GetProperty(ctx, arr, atom);
        ret = JS_ToBoolFree(ctx, val);
    }
    JS_FreeValue(ctx, arr);
    return ret;
}

static __exception int js_operator_instanceof(JSContext *ctx, JSValue *sp)
{
    JSValue op1, op2;
    BOOL ret;

    op1 = sp[-2];
    op2 = sp[-1];
    ret = JS_IsInstanceOf(ctx, op1, op2);
    if (ret < 0)
        return ret;
    JS_FreeValue(ctx, op1);
    JS_FreeValue(ctx, op2);
    sp[-2] = JS_NewBool(ctx, ret);
    return 0;
}

static __exception int js_operator_typeof(JSContext *ctx, JSValueConst op1)
{
    JSAtom atom;
    uint32_t tag;

    tag = JS_VALUE_GET_NORM_TAG(op1);
    switch(tag) {
    case JS_TAG_SHORT_BIG_INT:
    case JS_TAG_BIG_INT:
        atom = JS_ATOM_bigint;
        break;
    case JS_TAG_INT:
    case JS_TAG_FLOAT64:
        atom = JS_ATOM_number;
        break;
    case JS_TAG_UNDEFINED:
        atom = JS_ATOM_undefined;
        break;
    case JS_TAG_BOOL:
        atom = JS_ATOM_boolean;
        break;
    case JS_TAG_STRING:
    case JS_TAG_STRING_ROPE:
        atom = JS_ATOM_string;
        break;
    case JS_TAG_OBJECT:
        {
            JSObject *p;
            p = JS_VALUE_GET_OBJ(op1);
            if (unlikely(p->is_HTMLDDA))
                atom = JS_ATOM_undefined;
            else if (JS_IsFunction(ctx, op1))
                atom = JS_ATOM_function;
            else
                goto obj_type;
        }
        break;
    case JS_TAG_NULL:
    obj_type:
        atom = JS_ATOM_object;
        break;
    case JS_TAG_SYMBOL:
        atom = JS_ATOM_symbol;
        break;
    default:
        atom = JS_ATOM_unknown;
        break;
    }
    return atom;
}

static __exception int js_operator_delete(JSContext *ctx, JSValue *sp)
{
    JSValue op1, op2;
    JSAtom atom;
    int ret;

    op1 = sp[-2];
    op2 = sp[-1];
    atom = JS_ValueToAtom(ctx, op2);
    if (unlikely(atom == JS_ATOM_NULL))
        return -1;
    ret = JS_DeleteProperty(ctx, op1, atom, JS_PROP_THROW_STRICT);
    JS_FreeAtom(ctx, atom);
    if (unlikely(ret < 0))
        return -1;
    JS_FreeValue(ctx, op1);
    JS_FreeValue(ctx, op2);
    sp[-2] = JS_NewBool(ctx, ret);
    return 0;
}

/* XXX: not 100% compatible, but mozilla seems to use a similar
   implementation to ensure that caller in non strict mode does not
   throw (ES5 compatibility) */
static JSValue js_throw_type_error(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv)
{
    JSFunctionBytecode *b = JS_GetFunctionBytecode(this_val);
    if (!b || (b->js_mode & JS_MODE_STRICT) || !b->has_prototype || argc >= 1) {
        return JS_ThrowTypeError(ctx, "invalid property access");
    }
    return JS_UNDEFINED;
}

static JSValue js_function_proto_fileName(JSContext *ctx,
                                          JSValueConst this_val)
{
    JSFunctionBytecode *b = JS_GetFunctionBytecode(this_val);
    if (b && b->has_debug) {
        return JS_AtomToString(ctx, b->debug.filename);
    }
    return JS_UNDEFINED;
}

static JSValue js_function_proto_lineNumber(JSContext *ctx,
                                            JSValueConst this_val, int is_col)
{
    JSFunctionBytecode *b = JS_GetFunctionBytecode(this_val);
    if (b && b->has_debug) {
        int line_num, col_num;
        line_num = find_line_num(ctx, b, -1, &col_num);
        if (is_col)
            return JS_NewInt32(ctx, col_num);
        else
            return JS_NewInt32(ctx, line_num);
    }
    return JS_UNDEFINED;
}

static int js_arguments_define_own_property(JSContext *ctx,
                                            JSValueConst this_obj,
                                            JSAtom prop, JSValueConst val,
                                            JSValueConst getter, JSValueConst setter, int flags)
{
    JSObject *p;
    uint32_t idx;
    p = JS_VALUE_GET_OBJ(this_obj);
    /* convert to normal array when redefining an existing numeric field */
    if (p->fast_array && JS_AtomIsArrayIndex(ctx, &idx, prop) &&
        idx < p->u.array.count) {
        if (convert_fast_array_to_array(ctx, p))
            return -1;
    }
    /* run the default define own property */
    return JS_DefineProperty(ctx, this_obj, prop, val, getter, setter,
                             flags | JS_PROP_NO_EXOTIC);
}

static const JSClassExoticMethods js_arguments_exotic_methods = {
    .define_own_property = js_arguments_define_own_property,
};

static JSValue js_build_arguments(JSContext *ctx, int argc, JSValueConst *argv)
{
    JSValue val, *tab;
    JSProperty props[3];
    JSObject *p;
    int i;

    props[0].u.value = JS_NewInt32(ctx, argc); /* length */
    props[1].u.value = JS_DupValue(ctx, ctx->array_proto_values); /* Symbol.iterator */
    props[2].u.getset.getter = JS_VALUE_GET_OBJ(JS_DupValue(ctx, ctx->throw_type_error)); /* callee */
    props[2].u.getset.setter = JS_VALUE_GET_OBJ(JS_DupValue(ctx, ctx->throw_type_error)); /* callee */

    val = JS_NewObjectFromShape(ctx, js_dup_shape(ctx->arguments_shape),
                                JS_CLASS_ARGUMENTS, props);
    if (JS_IsException(val))
        return val;
    p = JS_VALUE_GET_OBJ(val);

    /* initialize the fast array part */
    tab = NULL;
    if (argc > 0) {
        tab = js_malloc(ctx, sizeof(tab[0]) * argc);
        if (!tab)
            goto fail;
        for(i = 0; i < argc; i++) {
            tab[i] = JS_DupValue(ctx, argv[i]);
        }
    }
    p->u.array.u.values = tab;
    p->u.array.count = argc;
    return val;
 fail:
    JS_FreeValue(ctx, val);
    return JS_EXCEPTION;
}

#define GLOBAL_VAR_OFFSET 0x40000000
#define ARGUMENT_VAR_OFFSET 0x20000000

static void js_mapped_arguments_finalizer(JSRuntime *rt, JSValue val)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JSVarRef **var_refs = p->u.array.u.var_refs;
    int i;
    for(i = 0; i < p->u.array.count; i++)
        free_var_ref(rt, var_refs[i]);
    js_free_rt(rt, var_refs);
}

static void js_mapped_arguments_mark(JSRuntime *rt, JSValueConst val,
                                     JS_MarkFunc *mark_func)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JSVarRef **var_refs = p->u.array.u.var_refs;
    int i;

    for(i = 0; i < p->u.array.count; i++)
        mark_func(rt, &var_refs[i]->header);
}

/* legacy arguments object: add references to the function arguments */
static JSValue js_build_mapped_arguments(JSContext *ctx, int argc,
                                         JSValueConst *argv,
                                         JSStackFrame *sf, int arg_count)
{
    JSValue val;
    JSProperty props[3];
    JSVarRef **tab, *var_ref;
    JSObject *p;
    int i, j;

    props[0].u.value = JS_NewInt32(ctx, argc); /* length */
    props[1].u.value = JS_DupValue(ctx, ctx->array_proto_values); /* Symbol.iterator */
    props[2].u.value = JS_DupValue(ctx, ctx->rt->current_stack_frame->cur_func); /* callee */

    val = JS_NewObjectFromShape(ctx, js_dup_shape(ctx->mapped_arguments_shape),
                                JS_CLASS_MAPPED_ARGUMENTS, props);
    if (JS_IsException(val))
        return val;
    p = JS_VALUE_GET_OBJ(val);

    /* initialize the fast array part */
    tab = NULL;
    if (argc > 0) {
        tab = js_malloc(ctx, sizeof(tab[0]) * argc);
        if (!tab)
            goto fail;
        for(i = 0; i < arg_count; i++) {
            var_ref = get_var_ref(ctx, sf, i, TRUE);
            if (!var_ref)
                goto fail1;
            tab[i] = var_ref;
        }
        for(i = arg_count; i < argc; i++) {
            var_ref = js_create_var_ref(ctx, FALSE);
            if (!var_ref) {
            fail1:
                for(j = 0; j < i; j++)
                    free_var_ref(ctx->rt, tab[j]);
                js_free(ctx, tab);
                goto fail;
            }
            var_ref->value = JS_DupValue(ctx, argv[i]);
            tab[i] = var_ref;
        }
    }
    p->u.array.u.var_refs = tab;
    p->u.array.count = argc;
    return val;
 fail:
    JS_FreeValue(ctx, val);
    return JS_EXCEPTION;
}

static JSValue build_for_in_iterator(JSContext *ctx, JSValue obj)
{
    JSObject *p, *p1;
    JSPropertyEnum *tab_atom;
    int i;
    JSValue enum_obj;
    JSForInIterator *it;
    uint32_t tag, tab_atom_count;

    tag = JS_VALUE_GET_TAG(obj);
    if (tag != JS_TAG_OBJECT && tag != JS_TAG_NULL && tag != JS_TAG_UNDEFINED) {
        obj = JS_ToObjectFree(ctx, obj);
    }

    it = js_malloc(ctx, sizeof(*it));
    if (!it) {
        JS_FreeValue(ctx, obj);
        return JS_EXCEPTION;
    }
    enum_obj = JS_NewObjectProtoClass(ctx, JS_NULL, JS_CLASS_FOR_IN_ITERATOR);
    if (JS_IsException(enum_obj)) {
        js_free(ctx, it);
        JS_FreeValue(ctx, obj);
        return JS_EXCEPTION;
    }
    it->is_array = FALSE;
    it->obj = obj;
    it->idx = 0;
    it->tab_atom = NULL;
    it->atom_count = 0;
    it->in_prototype_chain = FALSE;
    p1 = JS_VALUE_GET_OBJ(enum_obj);
    p1->u.for_in_iterator = it;

    if (tag == JS_TAG_NULL || tag == JS_TAG_UNDEFINED)
        return enum_obj;

    p = JS_VALUE_GET_OBJ(obj);
    if (p->fast_array) {
        JSShape *sh;
        JSShapeProperty *prs;
        /* check that there are no enumerable normal fields */
        sh = p->shape;
        for(i = 0, prs = get_shape_prop(sh); i < sh->prop_count; i++, prs++) {
            if (prs->flags & JS_PROP_ENUMERABLE)
                goto normal_case;
        }
        /* for fast arrays, we only store the number of elements */
        it->is_array = TRUE;
        it->atom_count = p->u.array.count;
    } else {
    normal_case:
        if (JS_GetOwnPropertyNamesInternal(ctx, &tab_atom, &tab_atom_count, p,
                                           JS_GPN_STRING_MASK | JS_GPN_SET_ENUM)) {
            JS_FreeValue(ctx, enum_obj);
            return JS_EXCEPTION;
        }
        it->tab_atom = tab_atom;
        it->atom_count = tab_atom_count;
    }
    return enum_obj;
}

/* obj -> enum_obj */
static __exception int js_for_in_start(JSContext *ctx, JSValue *sp)
{
    sp[-1] = build_for_in_iterator(ctx, sp[-1]);
    if (JS_IsException(sp[-1]))
        return -1;
    return 0;
}

/* return -1 if exception, 0 if slow case, 1 if the enumeration is finished */
static __exception int js_for_in_prepare_prototype_chain_enum(JSContext *ctx,
                                                              JSValueConst enum_obj)
{
    JSObject *p;
    JSForInIterator *it;
    JSPropertyEnum *tab_atom;
    uint32_t tab_atom_count, i;
    JSValue obj1;

    p = JS_VALUE_GET_OBJ(enum_obj);
    it = p->u.for_in_iterator;

    /* check if there are enumerable properties in the prototype chain (fast path) */
    obj1 = JS_DupValue(ctx, it->obj);
    for(;;) {
        obj1 = JS_GetPrototypeFree(ctx, obj1);
        if (JS_IsNull(obj1))
            break;
        if (JS_IsException(obj1))
            goto fail;
        if (JS_GetOwnPropertyNamesInternal(ctx, &tab_atom, &tab_atom_count,
                                           JS_VALUE_GET_OBJ(obj1),
                                           JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY)) {
            JS_FreeValue(ctx, obj1);
            goto fail;
        }
        JS_FreePropertyEnum(ctx, tab_atom, tab_atom_count);
        if (tab_atom_count != 0) {
            JS_FreeValue(ctx, obj1);
            goto slow_path;
        }
        /* must check for timeout to avoid infinite loop */
        if (js_poll_interrupts(ctx)) {
            JS_FreeValue(ctx, obj1);
            goto fail;
        }
    }
    JS_FreeValue(ctx, obj1);
    return 1;

 slow_path:
    /* add the visited properties, even if they are not enumerable */
    if (it->is_array) {
        if (JS_GetOwnPropertyNamesInternal(ctx, &tab_atom, &tab_atom_count,
                                           JS_VALUE_GET_OBJ(it->obj),
                                           JS_GPN_STRING_MASK | JS_GPN_SET_ENUM)) {
            goto fail;
        }
        it->is_array = FALSE;
        it->tab_atom = tab_atom;
        it->atom_count = tab_atom_count;
    }

    for(i = 0; i < it->atom_count; i++) {
        if (JS_DefinePropertyValue(ctx, enum_obj, it->tab_atom[i].atom, JS_NULL, JS_PROP_ENUMERABLE) < 0)
            goto fail;
    }
    return 0;
 fail:
    return -1;
}

/* enum_obj -> enum_obj value done */
static __exception int js_for_in_next(JSContext *ctx, JSValue *sp)
{
    JSValueConst enum_obj;
    JSObject *p;
    JSAtom prop;
    JSForInIterator *it;
    JSPropertyEnum *tab_atom;
    uint32_t tab_atom_count;
    int ret;

    enum_obj = sp[-1];
    /* fail safe */
    if (JS_VALUE_GET_TAG(enum_obj) != JS_TAG_OBJECT)
        goto done;
    p = JS_VALUE_GET_OBJ(enum_obj);
    if (p->class_id != JS_CLASS_FOR_IN_ITERATOR)
        goto done;
    it = p->u.for_in_iterator;

    for(;;) {
        if (it->idx >= it->atom_count) {
            if (JS_IsNull(it->obj) || JS_IsUndefined(it->obj))
                goto done; /* not an object */
            /* no more property in the current object: look in the prototype */
            if (!it->in_prototype_chain) {
                ret = js_for_in_prepare_prototype_chain_enum(ctx, enum_obj);
                if (ret < 0)
                    return -1;
                if (ret)
                    goto done;
                it->in_prototype_chain = TRUE;
            }
            it->obj = JS_GetPrototypeFree(ctx, it->obj);
            if (JS_IsException(it->obj))
                return -1;
            if (JS_IsNull(it->obj))
                goto done; /* no more prototype */

            /* must check for timeout to avoid infinite loop */
            if (js_poll_interrupts(ctx))
                return -1;

            if (JS_GetOwnPropertyNamesInternal(ctx, &tab_atom, &tab_atom_count,
                                               JS_VALUE_GET_OBJ(it->obj),
                                               JS_GPN_STRING_MASK | JS_GPN_SET_ENUM)) {
                return -1;
            }
            JS_FreePropertyEnum(ctx, it->tab_atom, it->atom_count);
            it->tab_atom = tab_atom;
            it->atom_count = tab_atom_count;
            it->idx = 0;
        } else {
            if (it->is_array) {
                prop = __JS_AtomFromUInt32(it->idx);
                it->idx++;
            } else {
                BOOL is_enumerable;
                prop = it->tab_atom[it->idx].atom;
                is_enumerable = it->tab_atom[it->idx].is_enumerable;
                it->idx++;
                if (it->in_prototype_chain) {
                    /* slow case: we are in the prototype chain */
                    ret = JS_GetOwnPropertyInternal(ctx, NULL, JS_VALUE_GET_OBJ(enum_obj), prop);
                    if (ret < 0)
                        return ret;
                    if (ret)
                        continue; /* already visited */
                    /* add to the visited property list */
                    if (JS_DefinePropertyValue(ctx, enum_obj, prop, JS_NULL,
                                               JS_PROP_ENUMERABLE) < 0)
                        return -1;
                }
                if (!is_enumerable)
                    continue;
            }
            /* check if the property was deleted */
            ret = JS_GetOwnPropertyInternal(ctx, NULL, JS_VALUE_GET_OBJ(it->obj), prop);
            if (ret < 0)
                return ret;
            if (ret)
                break;
        }
    }
    /* return the property */
    sp[0] = JS_AtomToValue(ctx, prop);
    sp[1] = JS_FALSE;
    return 0;
 done:
    /* return the end */
    sp[0] = JS_UNDEFINED;
    sp[1] = JS_TRUE;
    return 0;
}

static JSValue JS_GetIterator2(JSContext *ctx, JSValueConst obj,
                               JSValueConst method)
{
    JSValue enum_obj;

    enum_obj = JS_Call(ctx, method, obj, 0, NULL);
    if (JS_IsException(enum_obj))
        return enum_obj;
    if (!JS_IsObject(enum_obj)) {
        JS_FreeValue(ctx, enum_obj);
        return JS_ThrowTypeErrorNotAnObject(ctx);
    }
    return enum_obj;
}

static JSValue JS_GetIterator(JSContext *ctx, JSValueConst obj, BOOL is_async)
{
    JSValue method, ret, sync_iter;

    if (is_async) {
        method = JS_GetProperty(ctx, obj, JS_ATOM_Symbol_asyncIterator);
        if (JS_IsException(method))
            return method;
        if (JS_IsUndefined(method) || JS_IsNull(method)) {
            method = JS_GetProperty(ctx, obj, JS_ATOM_Symbol_iterator);
            if (JS_IsException(method))
                return method;
            sync_iter = JS_GetIterator2(ctx, obj, method);
            JS_FreeValue(ctx, method);
            if (JS_IsException(sync_iter))
                return sync_iter;
            ret = JS_CreateAsyncFromSyncIterator(ctx, sync_iter);
            JS_FreeValue(ctx, sync_iter);
            return ret;
        }
    } else {
        method = JS_GetProperty(ctx, obj, JS_ATOM_Symbol_iterator);
        if (JS_IsException(method))
            return method;
    }
    if (!JS_IsFunction(ctx, method)) {
        JS_FreeValue(ctx, method);
        return JS_ThrowTypeError(ctx, "value is not iterable");
    }
    ret = JS_GetIterator2(ctx, obj, method);
    JS_FreeValue(ctx, method);
    return ret;
}

/* return *pdone = 2 if the iterator object is not parsed */
static JSValue JS_IteratorNext2(JSContext *ctx, JSValueConst enum_obj,
                                JSValueConst method,
                                int argc, JSValueConst *argv, int *pdone)
{
    JSValue obj;

    /* fast path for the built-in iterators (avoid creating the
       intermediate result object) */
    if (JS_IsObject(method)) {
        JSObject *p = JS_VALUE_GET_OBJ(method);
        if (p->class_id == JS_CLASS_C_FUNCTION &&
            p->u.cfunc.cproto == JS_CFUNC_iterator_next) {
            JSCFunctionType func;
            JSValueConst args[1];

            /* in case the function expects one argument */
            if (argc == 0) {
                args[0] = JS_UNDEFINED;
                argv = args;
            }
            func = p->u.cfunc.c_function;
            return func.iterator_next(ctx, enum_obj, argc, argv,
                                      pdone, p->u.cfunc.magic);
        }
    }
    obj = JS_Call(ctx, method, enum_obj, argc, argv);
    if (JS_IsException(obj))
        goto fail;
    if (!JS_IsObject(obj)) {
        JS_FreeValue(ctx, obj);
        JS_ThrowTypeError(ctx, "iterator must return an object");
        goto fail;
    }
    *pdone = 2;
    return obj;
 fail:
    *pdone = FALSE;
    return JS_EXCEPTION;
}

/* Note: always return JS_UNDEFINED when *pdone = TRUE. */
static JSValue JS_IteratorNext(JSContext *ctx, JSValueConst enum_obj,
                               JSValueConst method,
                               int argc, JSValueConst *argv, BOOL *pdone)
{
    JSValue obj, value, done_val;
    int done;

    obj = JS_IteratorNext2(ctx, enum_obj, method, argc, argv, &done);
    if (JS_IsException(obj))
        goto fail;
    if (likely(done == 0)) {
        *pdone = FALSE;
        return obj;
    } else if (done != 2) {
        JS_FreeValue(ctx, obj);
        *pdone = TRUE;
        return JS_UNDEFINED;
    } else {
        done_val = JS_GetProperty(ctx, obj, JS_ATOM_done);
        if (JS_IsException(done_val))
            goto fail;
        *pdone = JS_ToBoolFree(ctx, done_val);
        value = JS_UNDEFINED;
        if (!*pdone) {
            value = JS_GetProperty(ctx, obj, JS_ATOM_value);
        }
        JS_FreeValue(ctx, obj);
        return value;
    }
 fail:
    JS_FreeValue(ctx, obj);
    *pdone = FALSE;
    return JS_EXCEPTION;
}

/* return < 0 in case of exception */
static int JS_IteratorClose(JSContext *ctx, JSValueConst enum_obj,
                            BOOL is_exception_pending)
{
    JSValue method, ret, ex_obj;
    int res;

    if (is_exception_pending) {
        ex_obj = ctx->rt->current_exception;
        ctx->rt->current_exception = JS_UNINITIALIZED;
        res = -1;
    } else {
        ex_obj = JS_UNDEFINED;
        res = 0;
    }
    method = JS_GetProperty(ctx, enum_obj, JS_ATOM_return);
    if (JS_IsException(method)) {
        res = -1;
        goto done;
    }
    if (JS_IsUndefined(method) || JS_IsNull(method)) {
        goto done;
    }
    ret = JS_CallFree(ctx, method, enum_obj, 0, NULL);
    if (!is_exception_pending) {
        if (JS_IsException(ret)) {
            res = -1;
        } else if (!JS_IsObject(ret)) {
            JS_ThrowTypeErrorNotAnObject(ctx);
            res = -1;
        }
    }
    JS_FreeValue(ctx, ret);
 done:
    if (is_exception_pending) {
        JS_Throw(ctx, ex_obj);
    }
    return res;
}

/* obj -> enum_rec (3 slots) */
static __exception int js_for_of_start(JSContext *ctx, JSValue *sp,
                                       BOOL is_async)
{
    JSValue op1, obj, method;
    op1 = sp[-1];
    obj = JS_GetIterator(ctx, op1, is_async);
    if (JS_IsException(obj))
        return -1;
    JS_FreeValue(ctx, op1);
    sp[-1] = obj;
    method = JS_GetProperty(ctx, obj, JS_ATOM_next);
    if (JS_IsException(method))
        return -1;
    sp[0] = method;
    return 0;
}

/* enum_rec [objs] -> enum_rec [objs] value done. There are 'offset'
   objs. If 'done' is true or in case of exception, 'enum_rec' is set
   to undefined. If 'done' is true, 'value' is always set to
   undefined. */
static __exception int js_for_of_next(JSContext *ctx, JSValue *sp, int offset)
{
    JSValue value = JS_UNDEFINED;
    int done = 1;

    if (likely(!JS_IsUndefined(sp[offset]))) {
        value = JS_IteratorNext(ctx, sp[offset], sp[offset + 1], 0, NULL, &done);
        if (JS_IsException(value))
            done = -1;
        if (done) {
            /* value is JS_UNDEFINED or JS_EXCEPTION */
            /* replace the iteration object with undefined */
            JS_FreeValue(ctx, sp[offset]);
            sp[offset] = JS_UNDEFINED;
            if (done < 0) {
                return -1;
            } else {
                JS_FreeValue(ctx, value);
                value = JS_UNDEFINED;
            }
        }
    }
    sp[0] = value;
    sp[1] = JS_NewBool(ctx, done);
    return 0;
}

static __exception int js_for_await_of_next(JSContext *ctx, JSValue *sp)
{
    JSValue obj, iter, next;

    sp[-1] = JS_UNDEFINED; /* disable the catch offset so that
                              exceptions do not close the iterator */
    iter = sp[-3];
    next = sp[-2];
    obj = JS_Call(ctx, next, iter, 0, NULL);
    if (JS_IsException(obj))
        return -1;
    sp[0] = obj;
    return 0;
}

static JSValue JS_IteratorGetCompleteValue(JSContext *ctx, JSValueConst obj,
                                           BOOL *pdone)
{
    JSValue done_val, value;
    BOOL done;
    done_val = JS_GetProperty(ctx, obj, JS_ATOM_done);
    if (JS_IsException(done_val))
        goto fail;
    done = JS_ToBoolFree(ctx, done_val);
    value = JS_GetProperty(ctx, obj, JS_ATOM_value);
    if (JS_IsException(value))
        goto fail;
    *pdone = done;
    return value;
 fail:
    *pdone = FALSE;
    return JS_EXCEPTION;
}

static __exception int js_iterator_get_value_done(JSContext *ctx, JSValue *sp)
{
    JSValue obj, value;
    BOOL done;
    obj = sp[-1];
    if (!JS_IsObject(obj)) {
        JS_ThrowTypeError(ctx, "iterator must return an object");
        return -1;
    }
    value = JS_IteratorGetCompleteValue(ctx, obj, &done);
    if (JS_IsException(value))
        return -1;
    JS_FreeValue(ctx, obj);
    /* put again the catch offset so that exceptions close the
       iterator */
    sp[-2] = JS_NewCatchOffset(ctx, 0);
    sp[-1] = value;
    sp[0] = JS_NewBool(ctx, done);
    return 0;
}

static JSValue js_create_iterator_result(JSContext *ctx,
                                         JSValue val,
                                         BOOL done)
{
    JSValue obj;
    obj = JS_NewObject(ctx);
    if (JS_IsException(obj)) {
        JS_FreeValue(ctx, val);
        return obj;
    }
    if (JS_DefinePropertyValue(ctx, obj, JS_ATOM_value,
                               val, JS_PROP_C_W_E) < 0) {
        goto fail;
    }
    if (JS_DefinePropertyValue(ctx, obj, JS_ATOM_done,
                               JS_NewBool(ctx, done), JS_PROP_C_W_E) < 0) {
    fail:
        JS_FreeValue(ctx, obj);
        return JS_EXCEPTION;
    }
    return obj;
}

static JSValue js_array_iterator_next(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv,
                                      BOOL *pdone, int magic);

static JSValue js_create_array_iterator(JSContext *ctx, JSValueConst this_val,
                                        int argc, JSValueConst *argv, int magic);

/* Access an Array's internal JSValue array if available */
static BOOL js_get_fast_array(JSContext *ctx, JSValueConst obj,
                              JSValue **arrpp, uint32_t *countp)
{
    /* Try and handle fast arrays explicitly */
    if (JS_VALUE_GET_TAG(obj) == JS_TAG_OBJECT) {
        JSObject *p = JS_VALUE_GET_OBJ(obj);
        if (p->class_id == JS_CLASS_ARRAY && p->fast_array) {
            *countp = p->u.array.count;
            *arrpp = p->u.array.u.values;
            return TRUE;
        }
    }
    return FALSE;
}

static __exception int js_append_enumerate(JSContext *ctx, JSValue *sp)
{
    JSValue iterator, enumobj, method, value;
    int is_array_iterator;
    JSValue *arrp;
    uint32_t i, count32, pos;
    JSCFunctionType ft;

    if (JS_VALUE_GET_TAG(sp[-2]) != JS_TAG_INT) {
        JS_ThrowInternalError(ctx, "invalid index for append");
        return -1;
    }

    pos = JS_VALUE_GET_INT(sp[-2]);

    /* XXX: further optimisations:
       - use ctx->array_proto_values?
       - check if array_iterator_prototype next method is built-in and
         avoid constructing actual iterator object?
       - build this into js_for_of_start and use in all `for (x of o)` loops
     */
    iterator = JS_GetProperty(ctx, sp[-1], JS_ATOM_Symbol_iterator);
    if (JS_IsException(iterator))
        return -1;
    ft.generic_magic = js_create_array_iterator;
    is_array_iterator = JS_IsCFunction(ctx, iterator, ft.generic,
                                       JS_ITERATOR_KIND_VALUE);
    JS_FreeValue(ctx, iterator);

    enumobj = JS_GetIterator(ctx, sp[-1], FALSE);
    if (JS_IsException(enumobj))
        return -1;
    method = JS_GetProperty(ctx, enumobj, JS_ATOM_next);
    if (JS_IsException(method)) {
        JS_FreeValue(ctx, enumobj);
        return -1;
    }

    ft.iterator_next = js_array_iterator_next;
    if (is_array_iterator
    &&  JS_IsCFunction(ctx, method, ft.generic, 0)
    &&  js_get_fast_array(ctx, sp[-1], &arrp, &count32)) {
        uint32_t len;
        if (js_get_length32(ctx, &len, sp[-1]))
            goto exception;
        /* if len > count32, the elements >= count32 might be read in
           the prototypes and might have side effects */
        if (len != count32)
            goto general_case;
        /* Handle fast arrays explicitly */
        for (i = 0; i < count32; i++) {
            if (JS_DefinePropertyValueUint32(ctx, sp[-3], pos++,
                                             JS_DupValue(ctx, arrp[i]), JS_PROP_C_W_E) < 0)
                goto exception;
        }
    } else {
    general_case:
        for (;;) {
            BOOL done;
            value = JS_IteratorNext(ctx, enumobj, method, 0, NULL, &done);
            if (JS_IsException(value))
                goto exception;
            if (done) {
                /* value is JS_UNDEFINED */
                break;
            }
            if (JS_DefinePropertyValueUint32(ctx, sp[-3], pos++, value, JS_PROP_C_W_E) < 0)
                goto exception;
        }
    }
    /* Note: could raise an error if too many elements */
    sp[-2] = JS_NewInt32(ctx, pos);
    JS_FreeValue(ctx, enumobj);
    JS_FreeValue(ctx, method);
    return 0;

exception:
    JS_IteratorClose(ctx, enumobj, TRUE);
    JS_FreeValue(ctx, enumobj);
    JS_FreeValue(ctx, method);
    return -1;
}

static __exception int JS_CopyDataProperties(JSContext *ctx,
                                             JSValueConst target,
                                             JSValueConst source,
                                             JSValueConst excluded,
                                             BOOL setprop)
{
    JSPropertyEnum *tab_atom;
    JSValue val;
    uint32_t i, tab_atom_count;
    JSObject *p;
    JSObject *pexcl = NULL;
    int ret, gpn_flags;
    JSPropertyDescriptor desc;
    BOOL is_enumerable;

    if (JS_VALUE_GET_TAG(source) != JS_TAG_OBJECT)
        return 0;

    if (JS_VALUE_GET_TAG(excluded) == JS_TAG_OBJECT)
        pexcl = JS_VALUE_GET_OBJ(excluded);

    p = JS_VALUE_GET_OBJ(source);

    gpn_flags = JS_GPN_STRING_MASK | JS_GPN_SYMBOL_MASK | JS_GPN_ENUM_ONLY;
    if (p->is_exotic) {
        const JSClassExoticMethods *em = ctx->rt->class_array[p->class_id].exotic;
        /* cannot use JS_GPN_ENUM_ONLY with e.g. proxies because it
           introduces a visible change */
        if (em && em->get_own_property_names) {
            gpn_flags &= ~JS_GPN_ENUM_ONLY;
        }
    }
    if (JS_GetOwnPropertyNamesInternal(ctx, &tab_atom, &tab_atom_count, p,
                                       gpn_flags))
        return -1;

    for (i = 0; i < tab_atom_count; i++) {
        if (pexcl) {
            ret = JS_GetOwnPropertyInternal(ctx, NULL, pexcl, tab_atom[i].atom);
            if (ret) {
                if (ret < 0)
                    goto exception;
                continue;
            }
        }
        if (!(gpn_flags & JS_GPN_ENUM_ONLY)) {
            /* test if the property is enumerable */
            ret = JS_GetOwnPropertyInternal(ctx, &desc, p, tab_atom[i].atom);
            if (ret < 0)
                goto exception;
            if (!ret)
                continue;
            is_enumerable = (desc.flags & JS_PROP_ENUMERABLE) != 0;
            js_free_desc(ctx, &desc);
            if (!is_enumerable)
                continue;
        }
        val = JS_GetProperty(ctx, source, tab_atom[i].atom);
        if (JS_IsException(val))
            goto exception;
        if (setprop)
            ret = JS_SetProperty(ctx, target, tab_atom[i].atom, val);
        else
            ret = JS_DefinePropertyValue(ctx, target, tab_atom[i].atom, val,
                                         JS_PROP_C_W_E);
        if (ret < 0)
            goto exception;
    }
    JS_FreePropertyEnum(ctx, tab_atom, tab_atom_count);
    return 0;
 exception:
    JS_FreePropertyEnum(ctx, tab_atom, tab_atom_count);
    return -1;
}

/* only valid inside C functions */
static JSValueConst JS_GetActiveFunction(JSContext *ctx)
{
    return ctx->rt->current_stack_frame->cur_func;
}

static JSVarRef *js_create_var_ref(JSContext *ctx, BOOL is_lexical)
{
    JSVarRef *var_ref;
    var_ref = js_malloc(ctx, sizeof(JSVarRef));
    if (!var_ref)
        return NULL;
    js_rc(var_ref)->ref_count = 1;
    if (is_lexical)
        var_ref->value = JS_UNINITIALIZED;
    else
        var_ref->value = JS_UNDEFINED;
    var_ref->pvalue = &var_ref->value;
    var_ref->is_detached = TRUE;
    var_ref->is_lexical = FALSE;
    var_ref->is_const = FALSE;
    add_gc_object(ctx->rt, &var_ref->header, JS_GC_OBJ_TYPE_VAR_REF);
    return var_ref;
}

static JSVarRef *get_var_ref(JSContext *ctx, JSStackFrame *sf, int var_idx,
                             BOOL is_arg)
{
    JSObject *p;
    JSFunctionBytecode *b;
    JSVarRef *var_ref;
    JSValue *pvalue;
    int var_ref_idx;
    JSBytecodeVarDef *vd;

    p = JS_VALUE_GET_OBJ(sf->cur_func);
    b = p->u.func.function_bytecode;

    if (is_arg) {
        vd = &b->vardefs[var_idx];
        pvalue = &sf->arg_buf[var_idx];
    } else {
        vd = &b->vardefs[b->arg_count + var_idx];
        pvalue = &sf->var_buf[var_idx];
    }
    assert(vd->is_captured);
    var_ref_idx = vd->var_ref_idx;
    assert(var_ref_idx < b->var_ref_count);
    var_ref = sf->var_refs[var_ref_idx];
    if (var_ref) {
        /* reference to the already created local variable */
        assert(var_ref->pvalue == pvalue);
        js_rc(var_ref)->ref_count++;
        return var_ref;
    }

    /* create a new one */
    var_ref = js_malloc(ctx, sizeof(JSVarRef));
    if (!var_ref)
        return NULL;
    js_rc(var_ref)->ref_count = 1;
    add_gc_object(ctx->rt, &var_ref->header, JS_GC_OBJ_TYPE_VAR_REF);
    var_ref->is_detached = FALSE;
    var_ref->is_lexical = FALSE;
    var_ref->is_const = FALSE;
    var_ref->var_ref_idx = var_ref_idx;
    var_ref->stack_frame = sf;
    sf->var_refs[var_ref_idx] = var_ref;
    if (sf->js_mode & JS_MODE_ASYNC) {
        JSAsyncFunctionState *async_func = container_of(sf, JSAsyncFunctionState, frame);
        /* The stack frame is detached and may be destroyed at any
           time so its reference count must be increased. Calling
           close_var_refs() when destroying the stack frame is not
           possible because it would change the graph between the GC
           objects. Another solution could be to temporarily detach
           the JSVarRef of async functions during the GC. It would
           have the advantage of allowing the release of unused stack
           frames in a cycle. */
        js_rc(async_func)->ref_count++;
    }
    var_ref->pvalue = pvalue;
    return var_ref;
}

static void js_global_object_finalizer(JSRuntime *rt, JSValue obj)
{
    JSObject *p = JS_VALUE_GET_OBJ(obj);
    JS_FreeValueRT(rt, p->u.global_object.uninitialized_vars);
}

static void js_global_object_mark(JSRuntime *rt, JSValueConst val,
                                  JS_MarkFunc *mark_func)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JS_MarkValue(rt, p->u.global_object.uninitialized_vars, mark_func);
}

static JSVarRef *js_global_object_get_uninitialized_var(JSContext *ctx, JSObject *p1,
                                                        JSAtom atom)
{
    JSObject *p = JS_VALUE_GET_OBJ(p1->u.global_object.uninitialized_vars);
    JSShapeProperty *prs;
    JSProperty *pr;
    JSVarRef *var_ref;

    prs = find_own_property(&pr, p, atom);
    if (prs) {
        assert((prs->flags & JS_PROP_TMASK) == JS_PROP_VARREF);
        var_ref = pr->u.var_ref;
        js_rc(var_ref)->ref_count++;
        return var_ref;
    }

    var_ref = js_create_var_ref(ctx, TRUE);
    if (!var_ref)
        return NULL;
    pr = add_property(ctx, p, atom, JS_PROP_C_W_E | JS_PROP_VARREF);
    if (unlikely(!pr)) {
        free_var_ref(ctx->rt, var_ref);
        return NULL;
    }
    pr->u.var_ref = var_ref;
    js_rc(var_ref)->ref_count++;
    return var_ref;
}

/* return a new variable reference. Get it from the uninitialized
   variables if it is present. Return NULL in case of memory error. */
static JSVarRef *js_global_object_find_uninitialized_var(JSContext *ctx, JSObject *p,
                                                         JSAtom atom, BOOL is_lexical)
{
    JSObject *p1;
    JSShapeProperty *prs;
    JSProperty *pr;
    JSVarRef *var_ref;

    p1 = JS_VALUE_GET_OBJ(p->u.global_object.uninitialized_vars);
    prs = find_own_property(&pr, p1, atom);
    if (prs) {
        assert((prs->flags & JS_PROP_TMASK) == JS_PROP_VARREF);
        var_ref = pr->u.var_ref;
        js_rc(var_ref)->ref_count++;
        delete_property(ctx, p1, atom);
        if (!is_lexical)
            var_ref->value = JS_UNDEFINED;
    } else {
        var_ref = js_create_var_ref(ctx, is_lexical);
        if (!var_ref)
            return NULL;
    }
    return var_ref;
}

static JSVarRef *js_closure_define_global_var(JSContext *ctx, JSClosureVar *cv,
                                              BOOL is_direct_or_indirect_eval)
{
    JSObject *p, *p1;
    JSShapeProperty *prs;
    int flags;
    JSProperty *pr;
    JSVarRef *var_ref;

    if (cv->is_lexical) {
        p = JS_VALUE_GET_OBJ(ctx->global_var_obj);
        flags = JS_PROP_ENUMERABLE | JS_PROP_CONFIGURABLE;
        if (!cv->is_const)
            flags |= JS_PROP_WRITABLE;

        prs = find_own_property(&pr, p, cv->var_name);
        if (prs) {
            assert((prs->flags & JS_PROP_TMASK) == JS_PROP_VARREF);
            var_ref = pr->u.var_ref;
            js_rc(var_ref)->ref_count++;
            return var_ref;
        }

        /* if there is a corresponding global variable, reuse its
           reference and create a new one for the global variable */
        p1 = JS_VALUE_GET_OBJ(ctx->global_obj);
        prs = find_own_property(&pr, p1, cv->var_name);
        if (prs && (prs->flags & JS_PROP_TMASK) == JS_PROP_VARREF) {
            JSVarRef *var_ref1;
            var_ref1 = js_create_var_ref(ctx, FALSE);
            if (!var_ref1)
                return NULL;
            var_ref = pr->u.var_ref;
            var_ref1->value = var_ref->value;
            var_ref->value = JS_UNINITIALIZED;
            pr->u.var_ref = var_ref1;
            goto add_var_ref;
        }
    } else {
        p = JS_VALUE_GET_OBJ(ctx->global_obj);
        flags = JS_PROP_ENUMERABLE | JS_PROP_WRITABLE;
        if (is_direct_or_indirect_eval)
            flags |= JS_PROP_CONFIGURABLE;

    retry:
        prs = find_own_property(&pr, p, cv->var_name);
        if (prs) {
            if (unlikely((prs->flags & JS_PROP_TMASK) == JS_PROP_AUTOINIT)) {
                if (JS_AutoInitProperty(ctx, p, cv->var_name, pr, prs))
                    return NULL;
                goto retry;
            } else if ((prs->flags & JS_PROP_TMASK) != JS_PROP_VARREF) {
                var_ref = js_global_object_get_uninitialized_var(ctx, p, cv->var_name);
                if (!var_ref)
                    return NULL;
            } else {
                var_ref = pr->u.var_ref;
                js_rc(var_ref)->ref_count++;
            }
            if (cv->var_kind == JS_VAR_GLOBAL_FUNCTION_DECL &&
                (prs->flags & JS_PROP_CONFIGURABLE)) {
                /* update the property flags if possible when
                   declaring a global function */
                if ((prs->flags & JS_PROP_TMASK) == JS_PROP_GETSET) {
                    free_property(ctx->rt, pr, prs->flags);
                    prs->flags = flags | JS_PROP_VARREF;
                    pr->u.var_ref = var_ref;
                    js_rc(var_ref)->ref_count++;
                } else {
                    assert((prs->flags & JS_PROP_TMASK) == JS_PROP_VARREF);
                    prs->flags = (prs->flags & ~JS_PROP_C_W_E) | flags;
                }
                var_ref->is_const = FALSE;
            }
            return var_ref;
        }

        if (!p->extensible) {
            return js_global_object_get_uninitialized_var(ctx, p, cv->var_name);
        }
    }

    /* if there is a corresponding uninitialized variable, use it */
    p1 = JS_VALUE_GET_OBJ(ctx->global_obj);
    var_ref = js_global_object_find_uninitialized_var(ctx, p1, cv->var_name, cv->is_lexical);
    if (!var_ref)
        return NULL;
 add_var_ref:
    if (cv->is_lexical) {
        var_ref->is_lexical = TRUE;
        var_ref->is_const = cv->is_const;
    }

    pr = add_property(ctx, p, cv->var_name, flags | JS_PROP_VARREF);
    if (unlikely(!pr)) {
        free_var_ref(ctx->rt, var_ref);
        return NULL;
    }
    pr->u.var_ref = var_ref;
    js_rc(var_ref)->ref_count++;
    return var_ref;
}

static JSVarRef *js_closure_global_var(JSContext *ctx, JSClosureVar *cv)
{
    JSObject *p;
    JSShapeProperty *prs;
    JSProperty *pr;
    JSVarRef *var_ref;

    p = JS_VALUE_GET_OBJ(ctx->global_var_obj);
    prs = find_own_property(&pr, p, cv->var_name);
    if (prs) {
        assert((prs->flags & JS_PROP_TMASK) == JS_PROP_VARREF);
        var_ref = pr->u.var_ref;
        js_rc(var_ref)->ref_count++;
        return var_ref;
    }
    p = JS_VALUE_GET_OBJ(ctx->global_obj);
 redo:
    prs = find_own_property(&pr, p, cv->var_name);
    if (prs) {
        if (unlikely((prs->flags & JS_PROP_TMASK) == JS_PROP_AUTOINIT)) {
            /* Instantiate property and retry */
            if (JS_AutoInitProperty(ctx, p, cv->var_name, pr, prs))
                return NULL;
            goto redo;
        }
        if ((prs->flags & JS_PROP_TMASK) == JS_PROP_VARREF) {
            var_ref = pr->u.var_ref;
            js_rc(var_ref)->ref_count++;
            return var_ref;
        }
    }
    return js_global_object_get_uninitialized_var(ctx, p, cv->var_name);
}

static JSValue js_closure2(JSContext *ctx, JSValue func_obj,
                           JSFunctionBytecode *b,
                           JSVarRef **cur_var_refs,
                           JSStackFrame *sf,
                           BOOL is_eval, JSModuleDef *m)
{
    JSObject *p;
    JSVarRef **var_refs;
    int i;

    p = JS_VALUE_GET_OBJ(func_obj);
    p->u.func.function_bytecode = b;
    p->u.func.home_object = NULL;
    p->u.func.var_refs = NULL;
    if (b->closure_var_count) {
        var_refs = js_mallocz(ctx, sizeof(var_refs[0]) * b->closure_var_count);
        if (!var_refs)
            goto fail;
        p->u.func.var_refs = var_refs;
        if (is_eval) {
            /* first pass to check the global variable definitions */
            for(i = 0; i < b->closure_var_count; i++) {
                JSClosureVar *cv = &b->closure_var[i];
                if (cv->closure_type == JS_CLOSURE_GLOBAL_DECL) {
                    int flags;
                    flags = 0;
                    if (cv->is_lexical)
                        flags |= DEFINE_GLOBAL_LEX_VAR;
                    if (cv->var_kind == JS_VAR_GLOBAL_FUNCTION_DECL)
                        flags |= DEFINE_GLOBAL_FUNC_VAR;
                    if (JS_CheckDefineGlobalVar(ctx, cv->var_name, flags))
                        goto fail;
                }
            }
        }
        for(i = 0; i < b->closure_var_count; i++) {
            JSClosureVar *cv = &b->closure_var[i];
            JSVarRef *var_ref;
            switch(cv->closure_type) {
            case JS_CLOSURE_MODULE_IMPORT:
                /* imported from other modules */
                continue;
            case JS_CLOSURE_MODULE_DECL:
                var_ref = js_create_var_ref(ctx, cv->is_lexical);
                break;
            case JS_CLOSURE_GLOBAL_DECL:
                var_ref = js_closure_define_global_var(ctx, cv, b->is_direct_or_indirect_eval);
                break;
            case JS_CLOSURE_GLOBAL:
                var_ref = js_closure_global_var(ctx, cv);
                break;
            case JS_CLOSURE_LOCAL:
                /* reuse the existing variable reference if it already exists */
                var_ref = get_var_ref(ctx, sf, cv->var_idx, FALSE);
                break;
            case JS_CLOSURE_ARG:
                /* reuse the existing variable reference if it already exists */
                var_ref = get_var_ref(ctx, sf, cv->var_idx, TRUE);
                break;
            case JS_CLOSURE_REF:
            case JS_CLOSURE_GLOBAL_REF:
                var_ref = cur_var_refs[cv->var_idx];
                js_rc(var_ref)->ref_count++;
                break;
            default:
                abort();
            }
            if (!var_ref)
                goto fail;
            var_refs[i] = var_ref;
        }
    }
    return func_obj;
 fail:
    /* bfunc is freed when func_obj is freed */
    JS_FreeValue(ctx, func_obj);
    return JS_EXCEPTION;
}

static JSValue js_instantiate_prototype(JSContext *ctx, JSObject *p, JSAtom atom, void *opaque)
{
    JSValue obj, this_val;
    int ret;

    this_val = JS_MKPTR(JS_TAG_OBJECT, p);
    obj = JS_NewObject(ctx);
    if (JS_IsException(obj))
        return JS_EXCEPTION;
    set_cycle_flag(ctx, obj);
    set_cycle_flag(ctx, this_val);
    ret = JS_DefinePropertyValue(ctx, obj, JS_ATOM_constructor,
                                 JS_DupValue(ctx, this_val),
                                 JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
    if (ret < 0) {
        JS_FreeValue(ctx, obj);
        return JS_EXCEPTION;
    }
    return obj;
}

static const uint16_t func_kind_to_class_id[] = {
    [JS_FUNC_NORMAL] = JS_CLASS_BYTECODE_FUNCTION,
    [JS_FUNC_GENERATOR] = JS_CLASS_GENERATOR_FUNCTION,
    [JS_FUNC_ASYNC] = JS_CLASS_ASYNC_FUNCTION,
    [JS_FUNC_ASYNC_GENERATOR] = JS_CLASS_ASYNC_GENERATOR_FUNCTION,
};

static JSValue js_closure(JSContext *ctx, JSValue bfunc,
                          JSVarRef **cur_var_refs,
                          JSStackFrame *sf, BOOL is_eval)
{
    JSFunctionBytecode *b;
    JSValue func_obj;
    JSAtom name_atom;

    b = JS_VALUE_GET_PTR(bfunc);
    func_obj = JS_NewObjectClass(ctx, func_kind_to_class_id[b->func_kind]);
    if (JS_IsException(func_obj)) {
        JS_FreeValue(ctx, bfunc);
        return JS_EXCEPTION;
    }
    func_obj = js_closure2(ctx, func_obj, b, cur_var_refs, sf, is_eval, NULL);
    if (JS_IsException(func_obj)) {
        /* bfunc has been freed */
        goto fail;
    }
    name_atom = b->func_name;
    if (name_atom == JS_ATOM_NULL)
        name_atom = JS_ATOM_empty_string;
    js_function_set_properties(ctx, func_obj, name_atom,
                               b->defined_arg_count);

    if (b->func_kind & JS_FUNC_GENERATOR) {
        JSValue proto;
        int proto_class_id;
        /* generators have a prototype field which is used as
           prototype for the generator object */
        if (b->func_kind == JS_FUNC_ASYNC_GENERATOR)
            proto_class_id = JS_CLASS_ASYNC_GENERATOR;
        else
            proto_class_id = JS_CLASS_GENERATOR;
        proto = JS_NewObjectProto(ctx, ctx->class_proto[proto_class_id]);
        if (JS_IsException(proto))
            goto fail;
        JS_DefinePropertyValue(ctx, func_obj, JS_ATOM_prototype, proto,
                               JS_PROP_WRITABLE);
    } else if (b->has_prototype) {
        /* add the 'prototype' property: delay instantiation to avoid
           creating cycles for every javascript function. The prototype
           object is created on the fly when first accessed */
        JS_SetConstructorBit(ctx, func_obj, TRUE);
        JS_DefineAutoInitProperty(ctx, func_obj, JS_ATOM_prototype,
                                  JS_AUTOINIT_ID_PROTOTYPE, NULL,
                                  JS_PROP_WRITABLE);
    }
    return func_obj;
 fail:
    /* bfunc is freed when func_obj is freed */
    JS_FreeValue(ctx, func_obj);
    return JS_EXCEPTION;
}

#define JS_DEFINE_CLASS_HAS_HERITAGE     (1 << 0)

static int js_op_define_class(JSContext *ctx, JSValue *sp,
                              JSAtom class_name, int class_flags,
                              JSVarRef **cur_var_refs,
                              JSStackFrame *sf, BOOL is_computed_name)
{
    JSValue bfunc, parent_class, proto = JS_UNDEFINED;
    JSValue ctor = JS_UNDEFINED, parent_proto = JS_UNDEFINED;
    JSFunctionBytecode *b;

    parent_class = sp[-2];
    bfunc = sp[-1];

    if (class_flags & JS_DEFINE_CLASS_HAS_HERITAGE) {
        if (JS_IsNull(parent_class)) {
            parent_proto = JS_NULL;
            parent_class = JS_DupValue(ctx, ctx->function_proto);
        } else {
            if (!JS_IsConstructor(ctx, parent_class)) {
                JS_ThrowTypeError(ctx, "parent class must be constructor");
                goto fail;
            }
            parent_proto = JS_GetProperty(ctx, parent_class, JS_ATOM_prototype);
            if (JS_IsException(parent_proto))
                goto fail;
            if (!JS_IsNull(parent_proto) && !JS_IsObject(parent_proto)) {
                JS_ThrowTypeError(ctx, "parent prototype must be an object or null");
                goto fail;
            }
        }
    } else {
        /* parent_class is JS_UNDEFINED in this case */
        parent_proto = JS_DupValue(ctx, ctx->class_proto[JS_CLASS_OBJECT]);
        parent_class = JS_DupValue(ctx, ctx->function_proto);
    }
    proto = JS_NewObjectProto(ctx, parent_proto);
    if (JS_IsException(proto))
        goto fail;

    b = JS_VALUE_GET_PTR(bfunc);
    assert(b->func_kind == JS_FUNC_NORMAL);
    ctor = JS_NewObjectProtoClass(ctx, parent_class,
                                  JS_CLASS_BYTECODE_FUNCTION);
    if (JS_IsException(ctor))
        goto fail;
    ctor = js_closure2(ctx, ctor, b, cur_var_refs, sf, FALSE, NULL);
    bfunc = JS_UNDEFINED;
    if (JS_IsException(ctor))
        goto fail;
    js_method_set_home_object(ctx, ctor, proto);
    JS_SetConstructorBit(ctx, ctor, TRUE);

    JS_DefinePropertyValue(ctx, ctor, JS_ATOM_length,
                           JS_NewInt32(ctx, b->defined_arg_count),
                           JS_PROP_CONFIGURABLE);

    if (is_computed_name) {
        if (JS_DefineObjectNameComputed(ctx, ctor, sp[-3],
                                        JS_PROP_CONFIGURABLE) < 0)
            goto fail;
    } else {
        if (JS_DefineObjectName(ctx, ctor, class_name, JS_PROP_CONFIGURABLE) < 0)
            goto fail;
    }

    /* the constructor property must be first. It can be overriden by
       computed property names */
    if (JS_DefinePropertyValue(ctx, proto, JS_ATOM_constructor,
                               JS_DupValue(ctx, ctor),
                               JS_PROP_CONFIGURABLE |
                               JS_PROP_WRITABLE | JS_PROP_THROW) < 0)
        goto fail;
    /* set the prototype property */
    if (JS_DefinePropertyValue(ctx, ctor, JS_ATOM_prototype,
                               JS_DupValue(ctx, proto), JS_PROP_THROW) < 0)
        goto fail;
    set_cycle_flag(ctx, ctor);
    set_cycle_flag(ctx, proto);

    JS_FreeValue(ctx, parent_proto);
    JS_FreeValue(ctx, parent_class);

    sp[-2] = ctor;
    sp[-1] = proto;
    return 0;
 fail:
    JS_FreeValue(ctx, parent_class);
    JS_FreeValue(ctx, parent_proto);
    JS_FreeValue(ctx, bfunc);
    JS_FreeValue(ctx, proto);
    JS_FreeValue(ctx, ctor);
    sp[-2] = JS_UNDEFINED;
    sp[-1] = JS_UNDEFINED;
    return -1;
}

static void close_var_ref(JSRuntime *rt, JSStackFrame *sf, JSVarRef *var_ref)
{
    if (sf->js_mode & JS_MODE_ASYNC) {
        JSAsyncFunctionState *async_func = container_of(sf, JSAsyncFunctionState, frame);
        async_func_free(rt, async_func);
    }
    var_ref->value = JS_DupValueRT(rt, *var_ref->pvalue);
    var_ref->pvalue = &var_ref->value;
    /* the reference is no longer to a local variable */
    var_ref->is_detached = TRUE;
}

static void close_var_refs(JSRuntime *rt, JSFunctionBytecode *b, JSStackFrame *sf)
{
    JSVarRef *var_ref;
    int i;

    for(i = 0; i < b->var_ref_count; i++) {
        var_ref = sf->var_refs[i];
        if (var_ref)
            close_var_ref(rt, sf, var_ref);
    }
}

static void close_lexical_var(JSContext *ctx, JSFunctionBytecode *b,
                              JSStackFrame *sf, int var_idx)
{
    JSVarRef *var_ref;
    int var_ref_idx;

    var_ref_idx = b->vardefs[b->arg_count + var_idx].var_ref_idx;
    var_ref = sf->var_refs[var_ref_idx];
    if (var_ref) {
        close_var_ref(ctx->rt, sf, var_ref);
        sf->var_refs[var_ref_idx] = NULL;
    }
}

#define JS_CALL_FLAG_COPY_ARGV   (1 << 1)
#define JS_CALL_FLAG_GENERATOR   (1 << 2)

static JSValue js_call_c_function(JSContext *ctx, JSValueConst func_obj,
                                  JSValueConst this_obj,
                                  int argc, JSValueConst *argv, int flags)
{
    JSRuntime *rt = ctx->rt;
    JSCFunctionType func;
    JSObject *p;
    JSStackFrame sf_s, *sf = &sf_s, *prev_sf;
    JSValue ret_val;
    JSValueConst *arg_buf;
    int arg_count, i;
    JSCFunctionEnum cproto;

    p = JS_VALUE_GET_OBJ(func_obj);
    cproto = p->u.cfunc.cproto;
    arg_count = p->u.cfunc.length;

    /* better to always check stack overflow */
    if (js_check_stack_overflow(rt, sizeof(arg_buf[0]) * arg_count))
        return JS_ThrowStackOverflow(ctx);

    prev_sf = rt->current_stack_frame;
    sf->prev_frame = prev_sf;
    rt->current_stack_frame = sf;
    ctx = p->u.cfunc.realm; /* change the current realm */
    sf->js_mode = 0;
    sf->cur_func = (JSValue)func_obj;
    sf->arg_count = argc;
    arg_buf = argv;

    if (unlikely(argc < arg_count)) {
        /* ensure that at least argc_count arguments are readable */
        arg_buf = alloca(sizeof(arg_buf[0]) * arg_count);
        for(i = 0; i < argc; i++)
            arg_buf[i] = argv[i];
        for(i = argc; i < arg_count; i++)
            arg_buf[i] = JS_UNDEFINED;
        sf->arg_count = arg_count;
    }
    sf->arg_buf = (JSValue*)arg_buf;

    func = p->u.cfunc.c_function;
    switch(cproto) {
    case JS_CFUNC_constructor:
    case JS_CFUNC_constructor_or_func:
        if (!(flags & JS_CALL_FLAG_CONSTRUCTOR)) {
            if (cproto == JS_CFUNC_constructor) {
            not_a_constructor:
                ret_val = JS_ThrowTypeError(ctx, "must be called with new");
                break;
            } else {
                this_obj = JS_UNDEFINED;
            }
        }
        /* here this_obj is new_target */
        /* fall thru */
    case JS_CFUNC_generic:
        ret_val = func.generic(ctx, this_obj, argc, arg_buf);
        break;
    case JS_CFUNC_constructor_magic:
    case JS_CFUNC_constructor_or_func_magic:
        if (!(flags & JS_CALL_FLAG_CONSTRUCTOR)) {
            if (cproto == JS_CFUNC_constructor_magic) {
                goto not_a_constructor;
            } else {
                this_obj = JS_UNDEFINED;
            }
        }
        /* fall thru */
    case JS_CFUNC_generic_magic:
        ret_val = func.generic_magic(ctx, this_obj, argc, arg_buf,
                                     p->u.cfunc.magic);
        break;
    case JS_CFUNC_getter:
        ret_val = func.getter(ctx, this_obj);
        break;
    case JS_CFUNC_setter:
        ret_val = func.setter(ctx, this_obj, arg_buf[0]);
        break;
    case JS_CFUNC_getter_magic:
        ret_val = func.getter_magic(ctx, this_obj, p->u.cfunc.magic);
        break;
    case JS_CFUNC_setter_magic:
        ret_val = func.setter_magic(ctx, this_obj, arg_buf[0], p->u.cfunc.magic);
        break;
    case JS_CFUNC_f_f:
        {
            double d1;

            if (unlikely(JS_ToFloat64(ctx, &d1, arg_buf[0]))) {
                ret_val = JS_EXCEPTION;
                break;
            }
            ret_val = JS_NewFloat64(ctx, func.f_f(d1));
        }
        break;
    case JS_CFUNC_f_f_f:
        {
            double d1, d2;

            if (unlikely(JS_ToFloat64(ctx, &d1, arg_buf[0]))) {
                ret_val = JS_EXCEPTION;
                break;
            }
            if (unlikely(JS_ToFloat64(ctx, &d2, arg_buf[1]))) {
                ret_val = JS_EXCEPTION;
                break;
            }
            ret_val = JS_NewFloat64(ctx, func.f_f_f(d1, d2));
        }
        break;
    case JS_CFUNC_iterator_next:
        {
            int done;
            ret_val = func.iterator_next(ctx, this_obj, argc, arg_buf,
                                         &done, p->u.cfunc.magic);
            if (!JS_IsException(ret_val) && done != 2) {
                ret_val = js_create_iterator_result(ctx, ret_val, done);
            }
        }
        break;
    default:
        abort();
    }

    rt->current_stack_frame = sf->prev_frame;
    return ret_val;
}

static JSValue js_call_bound_function(JSContext *ctx, JSValueConst func_obj,
                                      JSValueConst this_obj,
                                      int argc, JSValueConst *argv, int flags)
{
    JSObject *p;
    JSBoundFunction *bf;
    JSValueConst *arg_buf, new_target;
    int arg_count, i;

    p = JS_VALUE_GET_OBJ(func_obj);
    bf = p->u.bound_function;
    arg_count = bf->argc + argc;
    if (js_check_stack_overflow(ctx->rt, sizeof(JSValue) * arg_count))
        return JS_ThrowStackOverflow(ctx);
    arg_buf = alloca(sizeof(JSValue) * arg_count);
    for(i = 0; i < bf->argc; i++) {
        arg_buf[i] = bf->argv[i];
    }
    for(i = 0; i < argc; i++) {
        arg_buf[bf->argc + i] = argv[i];
    }
    if (flags & JS_CALL_FLAG_CONSTRUCTOR) {
        new_target = this_obj;
        if (js_same_value(ctx, func_obj, new_target))
            new_target = bf->func_obj;
        return JS_CallConstructor2(ctx, bf->func_obj, new_target,
                                   arg_count, arg_buf);
    } else {
        return JS_Call(ctx, bf->func_obj, bf->this_val,
                       arg_count, arg_buf);
    }
}

/* argument of OP_special_object */
typedef enum {
    OP_SPECIAL_OBJECT_ARGUMENTS,
    OP_SPECIAL_OBJECT_MAPPED_ARGUMENTS,
    OP_SPECIAL_OBJECT_THIS_FUNC,
    OP_SPECIAL_OBJECT_NEW_TARGET,
    OP_SPECIAL_OBJECT_HOME_OBJECT,
    OP_SPECIAL_OBJECT_VAR_OBJECT,
    OP_SPECIAL_OBJECT_IMPORT_META,
} OPSpecialObjectEnum;

#define FUNC_RET_AWAIT         0
#define FUNC_RET_YIELD         1
#define FUNC_RET_YIELD_STAR    2
#define FUNC_RET_INITIAL_YIELD 3

#ifdef OPCODE_ASM_LABEL
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-label"
#endif

/* argv[] is modified if (flags & JS_CALL_FLAG_COPY_ARGV) = 0. */
static JSValue JS_CallInternal(JSContext *caller_ctx, JSValueConst func_obj,
                               JSValueConst this_obj, JSValueConst new_target,
                               int argc, JSValue *argv, int flags)
{
    JSRuntime *rt = caller_ctx->rt;
    JSContext *ctx;
    JSObject *p;
    JSFunctionBytecode *b;
    JSStackFrame sf_s, *sf = &sf_s;
    const uint8_t *pc;
    int opcode, arg_allocated_size, i;
    JSValue *local_buf, *stack_buf, *var_buf, *arg_buf, *sp, ret_val, *pval;
    JSVarRef **var_refs;
    size_t alloca_size;

#if !DIRECT_DISPATCH
#define SWITCH(pc)      switch (opcode = *pc++)
#define CASE(op)        case op
#define DEFAULT         default
#define BREAK           break
#else
    static const void * const dispatch_table[256] = {
#define DEF(id, size, n_pop, n_push, f) && case_OP_ ## id,
#if SHORT_OPCODES
#define def(id, size, n_pop, n_push, f)
#else
#define def(id, size, n_pop, n_push, f) && case_default,
#endif
#include "quickjs-opcode.h"
        [ OP_COUNT ... 255 ] = &&case_default
    };
#define SWITCH(pc)      goto *dispatch_table[opcode = *pc++];
#ifdef OPCODE_ASM_LABEL
#define CASE(op)        case_ ## op: asm volatile("label_" #op ":\n.globl label_" #op); dummy_case_ ## op
#else
#define CASE(op)        case_ ## op
#endif
#define DEFAULT         case_default
#define BREAK           SWITCH(pc)
#endif

    if (js_poll_interrupts(caller_ctx))
        return JS_EXCEPTION;
    if (unlikely(JS_VALUE_GET_TAG(func_obj) != JS_TAG_OBJECT)) {
        if (flags & JS_CALL_FLAG_GENERATOR) {
            JSAsyncFunctionState *s = JS_VALUE_GET_PTR(func_obj);
            /* func_obj get contains a pointer to JSFuncAsyncState */
            /* the stack frame is already allocated */
            sf = &s->frame;
            p = JS_VALUE_GET_OBJ(sf->cur_func);
            b = p->u.func.function_bytecode;
            ctx = b->realm;
            var_refs = p->u.func.var_refs;
            local_buf = arg_buf = sf->arg_buf;
            var_buf = sf->var_buf;
            stack_buf = sf->var_buf + b->var_count;
            sp = sf->cur_sp;
            sf->cur_sp = NULL; /* cur_sp is NULL if the function is running */
            pc = sf->cur_pc;
            sf->prev_frame = rt->current_stack_frame;
            rt->current_stack_frame = sf;
            if (s->throw_flag)
                goto exception;
            else
                goto restart;
        } else {
            goto not_a_function;
        }
    }
    p = JS_VALUE_GET_OBJ(func_obj);
    if (unlikely(p->class_id != JS_CLASS_BYTECODE_FUNCTION)) {
        JSClassCall *call_func;
        call_func = rt->class_array[p->class_id].call;
        if (!call_func) {
        not_a_function:
            return JS_ThrowTypeError(caller_ctx, "not a function");
        }
        return call_func(caller_ctx, func_obj, this_obj, argc,
                         (JSValueConst *)argv, flags);
    }
    b = p->u.func.function_bytecode;

    if (unlikely(argc < b->arg_count || (flags & JS_CALL_FLAG_COPY_ARGV))) {
        arg_allocated_size = b->arg_count;
    } else {
        arg_allocated_size = 0;
    }

    alloca_size = sizeof(JSValue) * (arg_allocated_size + b->var_count +
                                     b->stack_size) +
        sizeof(JSVarRef *) * b->var_ref_count;
    if (js_check_stack_overflow(rt, alloca_size))
        return JS_ThrowStackOverflow(caller_ctx);

    sf->js_mode = b->js_mode;
    arg_buf = argv;
    sf->arg_count = argc;
    sf->cur_func = (JSValue)func_obj;
    var_refs = p->u.func.var_refs;

    local_buf = alloca(alloca_size);
    if (unlikely(arg_allocated_size)) {
        int n = min_int(argc, b->arg_count);
        arg_buf = local_buf;
        for(i = 0; i < n; i++)
            arg_buf[i] = JS_DupValue(caller_ctx, argv[i]);
        for(; i < b->arg_count; i++)
            arg_buf[i] = JS_UNDEFINED;
        sf->arg_count = b->arg_count;
    }
    var_buf = local_buf + arg_allocated_size;
    sf->var_buf = var_buf;
    sf->arg_buf = arg_buf;

    for(i = 0; i < b->var_count; i++)
        var_buf[i] = JS_UNDEFINED;

    stack_buf = var_buf + b->var_count;
    sf->var_refs = (JSVarRef **)(stack_buf + b->stack_size);
    for(i = 0; i < b->var_ref_count; i++)
        sf->var_refs[i] = NULL;
    sp = stack_buf;
    pc = b->byte_code_buf;
    sf->prev_frame = rt->current_stack_frame;
    rt->current_stack_frame = sf;
    ctx = b->realm; /* set the current realm */

 restart:
    for(;;) {
        int call_argc;
        JSValue *call_argv;

        SWITCH(pc) {
        CASE(OP_push_i32):
            *sp++ = JS_NewInt32(ctx, get_u32(pc));
            pc += 4;
            BREAK;
        CASE(OP_push_bigint_i32):
            *sp++ = __JS_NewShortBigInt(ctx, (int)get_u32(pc));
            pc += 4;
            BREAK;
        CASE(OP_push_const):
            *sp++ = JS_DupValue(ctx, b->cpool[get_u32(pc)]);
            pc += 4;
            BREAK;
#if SHORT_OPCODES
        CASE(OP_push_minus1):
        CASE(OP_push_0):
        CASE(OP_push_1):
        CASE(OP_push_2):
        CASE(OP_push_3):
        CASE(OP_push_4):
        CASE(OP_push_5):
        CASE(OP_push_6):
        CASE(OP_push_7):
            *sp++ = JS_NewInt32(ctx, opcode - OP_push_0);
            BREAK;
        CASE(OP_push_i8):
            *sp++ = JS_NewInt32(ctx, get_i8(pc));
            pc += 1;
            BREAK;
        CASE(OP_push_i16):
            *sp++ = JS_NewInt32(ctx, get_i16(pc));
            pc += 2;
            BREAK;
        CASE(OP_push_const8):
            *sp++ = JS_DupValue(ctx, b->cpool[*pc++]);
            BREAK;
        CASE(OP_fclosure8):
            *sp++ = js_closure(ctx, JS_DupValue(ctx, b->cpool[*pc++]), var_refs, sf, FALSE);
            if (unlikely(JS_IsException(sp[-1])))
                goto exception;
            BREAK;
        CASE(OP_push_empty_string):
            *sp++ = JS_AtomToString(ctx, JS_ATOM_empty_string);
            BREAK;
#endif
        CASE(OP_push_atom_value):
            *sp++ = JS_AtomToValue(ctx, get_u32(pc));
            pc += 4;
            BREAK;
        CASE(OP_undefined):
            *sp++ = JS_UNDEFINED;
            BREAK;
        CASE(OP_null):
            *sp++ = JS_NULL;
            BREAK;
        CASE(OP_push_this):
            /* OP_push_this is only called at the start of a function */
            {
                JSValue val;
                if (!(b->js_mode & JS_MODE_STRICT)) {
                    uint32_t tag = JS_VALUE_GET_TAG(this_obj);
                    if (likely(tag == JS_TAG_OBJECT))
                        goto normal_this;
                    if (tag == JS_TAG_NULL || tag == JS_TAG_UNDEFINED) {
                        val = JS_DupValue(ctx, ctx->global_obj);
                    } else {
                        val = JS_ToObject(ctx, this_obj);
                        if (JS_IsException(val))
                            goto exception;
                    }
                } else {
                normal_this:
                    val = JS_DupValue(ctx, this_obj);
                }
                *sp++ = val;
            }
            BREAK;
        CASE(OP_push_false):
            *sp++ = JS_FALSE;
            BREAK;
        CASE(OP_push_true):
            *sp++ = JS_TRUE;
            BREAK;
        CASE(OP_object):
            *sp++ = JS_NewObject(ctx);
            if (unlikely(JS_IsException(sp[-1])))
                goto exception;
            BREAK;
        CASE(OP_special_object):
            {
                int arg = *pc++;
                switch(arg) {
                case OP_SPECIAL_OBJECT_ARGUMENTS:
                    *sp++ = js_build_arguments(ctx, argc, (JSValueConst *)argv);
                    if (unlikely(JS_IsException(sp[-1])))
                        goto exception;
                    break;
                case OP_SPECIAL_OBJECT_MAPPED_ARGUMENTS:
                    *sp++ = js_build_mapped_arguments(ctx, argc, (JSValueConst *)argv,
                                                      sf, min_int(argc, b->arg_count));
                    if (unlikely(JS_IsException(sp[-1])))
                        goto exception;
                    break;
                case OP_SPECIAL_OBJECT_THIS_FUNC:
                    *sp++ = JS_DupValue(ctx, sf->cur_func);
                    break;
                case OP_SPECIAL_OBJECT_NEW_TARGET:
                    *sp++ = JS_DupValue(ctx, new_target);
                    break;
                case OP_SPECIAL_OBJECT_HOME_OBJECT:
                    {
                        JSObject *p1;
                        p1 = p->u.func.home_object;
                        if (unlikely(!p1))
                            *sp++ = JS_UNDEFINED;
                        else
                            *sp++ = JS_DupValue(ctx, JS_MKPTR(JS_TAG_OBJECT, p1));
                    }
                    break;
                case OP_SPECIAL_OBJECT_VAR_OBJECT:
                    *sp++ = JS_NewObjectProto(ctx, JS_NULL);
                    if (unlikely(JS_IsException(sp[-1])))
                        goto exception;
                    break;
                case OP_SPECIAL_OBJECT_IMPORT_META:
                    *sp++ = js_import_meta(ctx);
                    if (unlikely(JS_IsException(sp[-1])))
                        goto exception;
                    break;
                default:
                    abort();
                }
            }
            BREAK;
        CASE(OP_rest):
            {
                int first = get_u16(pc);
                pc += 2;
                first = min_int(first, argc);
                *sp++ = js_create_array(ctx, argc - first, (JSValueConst *)(argv + first));
                if (unlikely(JS_IsException(sp[-1])))
                    goto exception;
            }
            BREAK;

        CASE(OP_drop):
            JS_FreeValue(ctx, sp[-1]);
            sp--;
            BREAK;
        CASE(OP_nip):
            JS_FreeValue(ctx, sp[-2]);
            sp[-2] = sp[-1];
            sp--;
            BREAK;
        CASE(OP_nip1): /* a b c -> b c */
            JS_FreeValue(ctx, sp[-3]);
            sp[-3] = sp[-2];
            sp[-2] = sp[-1];
            sp--;
            BREAK;
        CASE(OP_dup):
            sp[0] = JS_DupValue(ctx, sp[-1]);
            sp++;
            BREAK;
        CASE(OP_dup2): /* a b -> a b a b */
            sp[0] = JS_DupValue(ctx, sp[-2]);
            sp[1] = JS_DupValue(ctx, sp[-1]);
            sp += 2;
            BREAK;
        CASE(OP_dup3): /* a b c -> a b c a b c */
            sp[0] = JS_DupValue(ctx, sp[-3]);
            sp[1] = JS_DupValue(ctx, sp[-2]);
            sp[2] = JS_DupValue(ctx, sp[-1]);
            sp += 3;
            BREAK;
        CASE(OP_dup1): /* a b -> a a b */
            sp[0] = sp[-1];
            sp[-1] = JS_DupValue(ctx, sp[-2]);
            sp++;
            BREAK;
        CASE(OP_insert2): /* obj a -> a obj a (dup_x1) */
            sp[0] = sp[-1];
            sp[-1] = sp[-2];
            sp[-2] = JS_DupValue(ctx, sp[0]);
            sp++;
            BREAK;
        CASE(OP_insert3): /* obj prop a -> a obj prop a (dup_x2) */
            sp[0] = sp[-1];
            sp[-1] = sp[-2];
            sp[-2] = sp[-3];
            sp[-3] = JS_DupValue(ctx, sp[0]);
            sp++;
            BREAK;
        CASE(OP_insert4): /* this obj prop a -> a this obj prop a */
            sp[0] = sp[-1];
            sp[-1] = sp[-2];
            sp[-2] = sp[-3];
            sp[-3] = sp[-4];
            sp[-4] = JS_DupValue(ctx, sp[0]);
            sp++;
            BREAK;
        CASE(OP_perm3): /* obj a b -> a obj b (213) */
            {
                JSValue tmp;
                tmp = sp[-2];
                sp[-2] = sp[-3];
                sp[-3] = tmp;
            }
            BREAK;
        CASE(OP_rot3l): /* x a b -> a b x (231) */
            {
                JSValue tmp;
                tmp = sp[-3];
                sp[-3] = sp[-2];
                sp[-2] = sp[-1];
                sp[-1] = tmp;
            }
            BREAK;
        CASE(OP_rot4l): /* x a b c -> a b c x */
            {
                JSValue tmp;
                tmp = sp[-4];
                sp[-4] = sp[-3];
                sp[-3] = sp[-2];
                sp[-2] = sp[-1];
                sp[-1] = tmp;
            }
            BREAK;
        CASE(OP_rot5l): /* x a b c d -> a b c d x */
            {
                JSValue tmp;
                tmp = sp[-5];
                sp[-5] = sp[-4];
                sp[-4] = sp[-3];
                sp[-3] = sp[-2];
                sp[-2] = sp[-1];
                sp[-1] = tmp;
            }
            BREAK;
        CASE(OP_rot3r): /* a b x -> x a b (312) */
            {
                JSValue tmp;
                tmp = sp[-1];
                sp[-1] = sp[-2];
                sp[-2] = sp[-3];
                sp[-3] = tmp;
            }
            BREAK;
        CASE(OP_perm4): /* obj prop a b -> a obj prop b */
            {
                JSValue tmp;
                tmp = sp[-2];
                sp[-2] = sp[-3];
                sp[-3] = sp[-4];
                sp[-4] = tmp;
            }
            BREAK;
        CASE(OP_perm5): /* this obj prop a b -> a this obj prop b */
            {
                JSValue tmp;
                tmp = sp[-2];
                sp[-2] = sp[-3];
                sp[-3] = sp[-4];
                sp[-4] = sp[-5];
                sp[-5] = tmp;
            }
            BREAK;
        CASE(OP_swap): /* a b -> b a */
            {
                JSValue tmp;
                tmp = sp[-2];
                sp[-2] = sp[-1];
                sp[-1] = tmp;
            }
            BREAK;
        CASE(OP_swap2): /* a b c d -> c d a b */
            {
                JSValue tmp1, tmp2;
                tmp1 = sp[-4];
                tmp2 = sp[-3];
                sp[-4] = sp[-2];
                sp[-3] = sp[-1];
                sp[-2] = tmp1;
                sp[-1] = tmp2;
            }
            BREAK;

        CASE(OP_fclosure):
            {
                JSValue bfunc = JS_DupValue(ctx, b->cpool[get_u32(pc)]);
                pc += 4;
                *sp++ = js_closure(ctx, bfunc, var_refs, sf, FALSE);
                if (unlikely(JS_IsException(sp[-1])))
                    goto exception;
            }
            BREAK;
#if SHORT_OPCODES
        CASE(OP_call0):
        CASE(OP_call1):
        CASE(OP_call2):
        CASE(OP_call3):
            call_argc = opcode - OP_call0;
            goto has_call_argc;
#endif
        CASE(OP_call):
        CASE(OP_tail_call):
            {
                call_argc = get_u16(pc);
                pc += 2;
                goto has_call_argc;
            has_call_argc:
                call_argv = sp - call_argc;
                sf->cur_pc = pc;
                ret_val = JS_CallInternal(ctx, call_argv[-1], JS_UNDEFINED,
                                          JS_UNDEFINED, call_argc, call_argv, 0);
                if (unlikely(JS_IsException(ret_val)))
                    goto exception;
                if (opcode == OP_tail_call)
                    goto done;
                for(i = -1; i < call_argc; i++)
                    JS_FreeValue(ctx, call_argv[i]);
                sp -= call_argc + 1;
                *sp++ = ret_val;
            }
            BREAK;
        CASE(OP_call_constructor):
            {
                call_argc = get_u16(pc);
                pc += 2;
                call_argv = sp - call_argc;
                sf->cur_pc = pc;
                ret_val = JS_CallConstructorInternal(ctx, call_argv[-2],
                                                     call_argv[-1],
                                                     call_argc, call_argv, 0);
                if (unlikely(JS_IsException(ret_val)))
                    goto exception;
                for(i = -2; i < call_argc; i++)
                    JS_FreeValue(ctx, call_argv[i]);
                sp -= call_argc + 2;
                *sp++ = ret_val;
            }
            BREAK;
        CASE(OP_call_method):
        CASE(OP_tail_call_method):
            {
                call_argc = get_u16(pc);
                pc += 2;
                call_argv = sp - call_argc;
                sf->cur_pc = pc;
                ret_val = JS_CallInternal(ctx, call_argv[-1], call_argv[-2],
                                          JS_UNDEFINED, call_argc, call_argv, 0);
                if (unlikely(JS_IsException(ret_val)))
                    goto exception;
                if (opcode == OP_tail_call_method)
                    goto done;
                for(i = -2; i < call_argc; i++)
                    JS_FreeValue(ctx, call_argv[i]);
                sp -= call_argc + 2;
                *sp++ = ret_val;
            }
            BREAK;
        CASE(OP_array_from):
            call_argc = get_u16(pc);
            pc += 2;
            ret_val = js_create_array_free(ctx, call_argc, sp - call_argc);
            sp -= call_argc;
            if (unlikely(JS_IsException(ret_val)))
                goto exception;
            *sp++ = ret_val;
            BREAK;

        CASE(OP_apply):
            {
                int magic;
                magic = get_u16(pc);
                pc += 2;
                sf->cur_pc = pc;

                ret_val = js_function_apply(ctx, sp[-3], 2, (JSValueConst *)&sp[-2], magic);
                if (unlikely(JS_IsException(ret_val)))
                    goto exception;
                JS_FreeValue(ctx, sp[-3]);
                JS_FreeValue(ctx, sp[-2]);
                JS_FreeValue(ctx, sp[-1]);
                sp -= 3;
                *sp++ = ret_val;
            }
            BREAK;
        CASE(OP_return):
            ret_val = *--sp;
            goto done;
        CASE(OP_return_undef):
            ret_val = JS_UNDEFINED;
            goto done;

        CASE(OP_check_ctor_return):
            /* return TRUE if 'this' should be returned */
            if (!JS_IsObject(sp[-1])) {
                if (!JS_IsUndefined(sp[-1])) {
                    JS_ThrowTypeError(caller_ctx, "derived class constructor must return an object or undefined");
                    goto exception;
                }
                sp[0] = JS_TRUE;
            } else {
                sp[0] = JS_FALSE;
            }
            sp++;
            BREAK;
        CASE(OP_check_ctor):
            if (JS_IsUndefined(new_target)) {
            non_ctor_call:
                JS_ThrowTypeError(ctx, "class constructors must be invoked with 'new'");
                goto exception;
            }
            BREAK;
        CASE(OP_init_ctor):
            {
                JSValue super, ret;
                sf->cur_pc = pc;
                if (JS_IsUndefined(new_target))
                    goto non_ctor_call;
                super = JS_GetPrototype(ctx, func_obj);
                if (JS_IsException(super))
                    goto exception;
                ret = JS_CallConstructor2(ctx, super, new_target, argc, (JSValueConst *)argv);
                JS_FreeValue(ctx, super);
                if (JS_IsException(ret))
                    goto exception;
                *sp++ = ret;
            }
            BREAK;
        CASE(OP_check_brand):
            {
                int ret = JS_CheckBrand(ctx, sp[-2], sp[-1]);
                if (ret < 0)
                    goto exception;
                if (!ret) {
                    JS_ThrowTypeError(ctx, "invalid brand on object");
                    goto exception;
                }
            }
            BREAK;
        CASE(OP_add_brand):
            if (JS_AddBrand(ctx, sp[-2], sp[-1]) < 0)
                goto exception;
            JS_FreeValue(ctx, sp[-2]);
            JS_FreeValue(ctx, sp[-1]);
            sp -= 2;
            BREAK;

        CASE(OP_throw):
            JS_Throw(ctx, *--sp);
            goto exception;

        CASE(OP_throw_error):
#define JS_THROW_VAR_RO             0
#define JS_THROW_VAR_REDECL         1
#define JS_THROW_VAR_UNINITIALIZED  2
#define JS_THROW_ERROR_DELETE_SUPER   3
#define JS_THROW_ERROR_ITERATOR_THROW 4
            {
                JSAtom atom;
                int type;
                atom = get_u32(pc);
                type = pc[4];
                pc += 5;
                if (type == JS_THROW_VAR_RO)
                    JS_ThrowTypeErrorReadOnly(ctx, JS_PROP_THROW, atom);
                else
                if (type == JS_THROW_VAR_REDECL)
                    JS_ThrowSyntaxErrorVarRedeclaration(ctx, atom);
                else
                if (type == JS_THROW_VAR_UNINITIALIZED)
                    JS_ThrowReferenceErrorUninitialized(ctx, atom);
                else
                if (type == JS_THROW_ERROR_DELETE_SUPER)
                    JS_ThrowReferenceError(ctx, "unsupported reference to 'super'");
                else
                if (type == JS_THROW_ERROR_ITERATOR_THROW)
                    JS_ThrowTypeError(ctx, "iterator does not have a throw method");
                else
                    JS_ThrowInternalError(ctx, "invalid throw var type %d", type);
            }
            goto exception;

        CASE(OP_eval):
            {
                JSValueConst obj;
                int scope_idx;
                call_argc = get_u16(pc);
                scope_idx = get_u16(pc + 2) + ARG_SCOPE_END;
                pc += 4;
                call_argv = sp - call_argc;
                sf->cur_pc = pc;
                if (js_same_value(ctx, call_argv[-1], ctx->eval_obj)) {
                    if (call_argc >= 1)
                        obj = call_argv[0];
                    else
                        obj = JS_UNDEFINED;
                    ret_val = JS_EvalObject(ctx, JS_UNDEFINED, obj,
                                            JS_EVAL_TYPE_DIRECT, scope_idx);
                } else {
                    ret_val = JS_CallInternal(ctx, call_argv[-1], JS_UNDEFINED,
                                              JS_UNDEFINED, call_argc, call_argv, 0);
                }
                if (unlikely(JS_IsException(ret_val)))
                    goto exception;
                for(i = -1; i < call_argc; i++)
                    JS_FreeValue(ctx, call_argv[i]);
                sp -= call_argc + 1;
                *sp++ = ret_val;
            }
            BREAK;
            /* could merge with OP_apply */
        CASE(OP_apply_eval):
            {
                int scope_idx;
                uint32_t len;
                JSValue *tab;
                JSValueConst obj;

                scope_idx = get_u16(pc) + ARG_SCOPE_END;
                pc += 2;
                sf->cur_pc = pc;
                tab = build_arg_list(ctx, &len, sp[-1]);
                if (!tab)
                    goto exception;
                if (js_same_value(ctx, sp[-2], ctx->eval_obj)) {
                    if (len >= 1)
                        obj = tab[0];
                    else
                        obj = JS_UNDEFINED;
                    ret_val = JS_EvalObject(ctx, JS_UNDEFINED, obj,
                                            JS_EVAL_TYPE_DIRECT, scope_idx);
                } else {
                    ret_val = JS_Call(ctx, sp[-2], JS_UNDEFINED, len,
                                      (JSValueConst *)tab);
                }
                free_arg_list(ctx, tab, len);
                if (unlikely(JS_IsException(ret_val)))
                    goto exception;
                JS_FreeValue(ctx, sp[-2]);
                JS_FreeValue(ctx, sp[-1]);
                sp -= 2;
                *sp++ = ret_val;
            }
            BREAK;

        CASE(OP_regexp):
            {
                sp[-2] = JS_NewRegexp(ctx, sp[-2], sp[-1]);
                sp--;
                if (JS_IsException(sp[-1]))
                    goto exception;
            }
            BREAK;

        CASE(OP_get_super):
            {
                JSValue proto;
                sf->cur_pc = pc;
                proto = JS_GetPrototype(ctx, sp[-1]);
                if (JS_IsException(proto))
                    goto exception;
                JS_FreeValue(ctx, sp[-1]);
                sp[-1] = proto;
            }
            BREAK;

        CASE(OP_import):
            {
                JSValue val;
                sf->cur_pc = pc;
                val = js_dynamic_import(ctx, sp[-2], sp[-1]);
                if (JS_IsException(val))
                    goto exception;
                JS_FreeValue(ctx, sp[-2]);
                JS_FreeValue(ctx, sp[-1]);
                sp--;
                sp[-1] = val;
            }
            BREAK;

        CASE(OP_get_var_undef):
        CASE(OP_get_var):
            {
                int idx;
                JSValue val;
                idx = get_u16(pc);
                pc += 2;
                val = *var_refs[idx]->pvalue;
                if (unlikely(JS_IsUninitialized(val))) {
                    JSClosureVar *cv = &b->closure_var[idx];
                    if (cv->is_lexical) {
                        JS_ThrowReferenceErrorUninitialized(ctx, cv->var_name);
                        goto exception;
                    } else {
                        sf->cur_pc = pc;
                        sp[0] = JS_GetPropertyInternal(ctx, ctx->global_obj,
                                                       cv->var_name,
                                                       ctx->global_obj,
                                                       opcode - OP_get_var_undef);
                        if (JS_IsException(sp[0]))
                            goto exception;
                    }
                } else {
                    sp[0] = JS_DupValue(ctx, val);
                }
                sp++;
            }
            BREAK;

        CASE(OP_put_var):
        CASE(OP_put_var_init):
            {
                int idx, ret;
                JSVarRef *var_ref;
                idx = get_u16(pc);
                pc += 2;
                var_ref = var_refs[idx];
                if (unlikely(JS_IsUninitialized(*var_ref->pvalue) ||
                             var_ref->is_const)) {
                    JSClosureVar *cv = &b->closure_var[idx];
                    if (var_ref->is_lexical) {
                        if (opcode == OP_put_var_init)
                            goto put_var_ok;
                        if (JS_IsUninitialized(*var_ref->pvalue))
                            JS_ThrowReferenceErrorUninitialized(ctx, cv->var_name);
                        else
                            JS_ThrowTypeErrorReadOnly(ctx, JS_PROP_THROW, cv->var_name);
                        goto exception;
                    } else {
                        sf->cur_pc = pc;
                        ret = JS_HasProperty(ctx, ctx->global_obj, cv->var_name);
                        if (ret < 0)
                            goto exception;
                        if (ret == 0 && is_strict_mode(ctx)) {
                            JS_ThrowReferenceErrorNotDefined(ctx, cv->var_name);
                            goto exception;
                        }
                        ret = JS_SetPropertyInternal(ctx, ctx->global_obj, cv->var_name, sp[-1],
                                                     ctx->global_obj, JS_PROP_THROW_STRICT);
                        sp--;
                        if (ret < 0)
                            goto exception;
                    }
                } else {
                put_var_ok:
                   set_value(ctx, var_ref->pvalue, sp[-1]);
                   sp--;
                }
            }
            BREAK;
        CASE(OP_get_loc):
            {
                int idx;
                idx = get_u16(pc);
                pc += 2;
                sp[0] = JS_DupValue(ctx, var_buf[idx]);
                sp++;
            }
            BREAK;
        CASE(OP_put_loc):
            {
                int idx;
                idx = get_u16(pc);
                pc += 2;
                set_value(ctx, &var_buf[idx], sp[-1]);
                sp--;
            }
            BREAK;
        CASE(OP_set_loc):
            {
                int idx;
                idx = get_u16(pc);
                pc += 2;
                set_value(ctx, &var_buf[idx], JS_DupValue(ctx, sp[-1]));
            }
            BREAK;
        CASE(OP_get_arg):
            {
                int idx;
                idx = get_u16(pc);
                pc += 2;
                sp[0] = JS_DupValue(ctx, arg_buf[idx]);
                sp++;
            }
            BREAK;
        CASE(OP_put_arg):
            {
                int idx;
                idx = get_u16(pc);
                pc += 2;
                set_value(ctx, &arg_buf[idx], sp[-1]);
                sp--;
            }
            BREAK;
        CASE(OP_set_arg):
            {
                int idx;
                idx = get_u16(pc);
                pc += 2;
                set_value(ctx, &arg_buf[idx], JS_DupValue(ctx, sp[-1]));
            }
            BREAK;

#if SHORT_OPCODES
        CASE(OP_get_loc8): *sp++ = JS_DupValue(ctx, var_buf[*pc++]); BREAK;
        CASE(OP_put_loc8): set_value(ctx, &var_buf[*pc++], *--sp); BREAK;
        CASE(OP_set_loc8): set_value(ctx, &var_buf[*pc++], JS_DupValue(ctx, sp[-1])); BREAK;

        CASE(OP_get_loc0): *sp++ = JS_DupValue(ctx, var_buf[0]); BREAK;
        CASE(OP_get_loc1): *sp++ = JS_DupValue(ctx, var_buf[1]); BREAK;
        CASE(OP_get_loc2): *sp++ = JS_DupValue(ctx, var_buf[2]); BREAK;
        CASE(OP_get_loc3): *sp++ = JS_DupValue(ctx, var_buf[3]); BREAK;
        CASE(OP_put_loc0): set_value(ctx, &var_buf[0], *--sp); BREAK;
        CASE(OP_put_loc1): set_value(ctx, &var_buf[1], *--sp); BREAK;
        CASE(OP_put_loc2): set_value(ctx, &var_buf[2], *--sp); BREAK;
        CASE(OP_put_loc3): set_value(ctx, &var_buf[3], *--sp); BREAK;
        CASE(OP_set_loc0): set_value(ctx, &var_buf[0], JS_DupValue(ctx, sp[-1])); BREAK;
        CASE(OP_set_loc1): set_value(ctx, &var_buf[1], JS_DupValue(ctx, sp[-1])); BREAK;
        CASE(OP_set_loc2): set_value(ctx, &var_buf[2], JS_DupValue(ctx, sp[-1])); BREAK;
        CASE(OP_set_loc3): set_value(ctx, &var_buf[3], JS_DupValue(ctx, sp[-1])); BREAK;
        CASE(OP_get_arg0): *sp++ = JS_DupValue(ctx, arg_buf[0]); BREAK;
        CASE(OP_get_arg1): *sp++ = JS_DupValue(ctx, arg_buf[1]); BREAK;
        CASE(OP_get_arg2): *sp++ = JS_DupValue(ctx, arg_buf[2]); BREAK;
        CASE(OP_get_arg3): *sp++ = JS_DupValue(ctx, arg_buf[3]); BREAK;
        CASE(OP_put_arg0): set_value(ctx, &arg_buf[0], *--sp); BREAK;
        CASE(OP_put_arg1): set_value(ctx, &arg_buf[1], *--sp); BREAK;
        CASE(OP_put_arg2): set_value(ctx, &arg_buf[2], *--sp); BREAK;
        CASE(OP_put_arg3): set_value(ctx, &arg_buf[3], *--sp); BREAK;
        CASE(OP_set_arg0): set_value(ctx, &arg_buf[0], JS_DupValue(ctx, sp[-1])); BREAK;
        CASE(OP_set_arg1): set_value(ctx, &arg_buf[1], JS_DupValue(ctx, sp[-1])); BREAK;
        CASE(OP_set_arg2): set_value(ctx, &arg_buf[2], JS_DupValue(ctx, sp[-1])); BREAK;
        CASE(OP_set_arg3): set_value(ctx, &arg_buf[3], JS_DupValue(ctx, sp[-1])); BREAK;
        CASE(OP_get_var_ref0): *sp++ = JS_DupValue(ctx, *var_refs[0]->pvalue); BREAK;
        CASE(OP_get_var_ref1): *sp++ = JS_DupValue(ctx, *var_refs[1]->pvalue); BREAK;
        CASE(OP_get_var_ref2): *sp++ = JS_DupValue(ctx, *var_refs[2]->pvalue); BREAK;
        CASE(OP_get_var_ref3): *sp++ = JS_DupValue(ctx, *var_refs[3]->pvalue); BREAK;
        CASE(OP_put_var_ref0): set_value(ctx, var_refs[0]->pvalue, *--sp); BREAK;
        CASE(OP_put_var_ref1): set_value(ctx, var_refs[1]->pvalue, *--sp); BREAK;
        CASE(OP_put_var_ref2): set_value(ctx, var_refs[2]->pvalue, *--sp); BREAK;
        CASE(OP_put_var_ref3): set_value(ctx, var_refs[3]->pvalue, *--sp); BREAK;
        CASE(OP_set_var_ref0): set_value(ctx, var_refs[0]->pvalue, JS_DupValue(ctx, sp[-1])); BREAK;
        CASE(OP_set_var_ref1): set_value(ctx, var_refs[1]->pvalue, JS_DupValue(ctx, sp[-1])); BREAK;
        CASE(OP_set_var_ref2): set_value(ctx, var_refs[2]->pvalue, JS_DupValue(ctx, sp[-1])); BREAK;
        CASE(OP_set_var_ref3): set_value(ctx, var_refs[3]->pvalue, JS_DupValue(ctx, sp[-1])); BREAK;
#endif

        CASE(OP_get_var_ref):
            {
                int idx;
                JSValue val;
                idx = get_u16(pc);
                pc += 2;
                val = *var_refs[idx]->pvalue;
                sp[0] = JS_DupValue(ctx, val);
                sp++;
            }
            BREAK;
        CASE(OP_put_var_ref):
            {
                int idx;
                idx = get_u16(pc);
                pc += 2;
                set_value(ctx, var_refs[idx]->pvalue, sp[-1]);
                sp--;
            }
            BREAK;
        CASE(OP_set_var_ref):
            {
                int idx;
                idx = get_u16(pc);
                pc += 2;
                set_value(ctx, var_refs[idx]->pvalue, JS_DupValue(ctx, sp[-1]));
            }
            BREAK;
        CASE(OP_get_var_ref_check):
            {
                int idx;
                JSValue val;
                idx = get_u16(pc);
                pc += 2;
                val = *var_refs[idx]->pvalue;
                if (unlikely(JS_IsUninitialized(val))) {
                    JS_ThrowReferenceErrorUninitialized2(ctx, b, idx, TRUE);
                    goto exception;
                }
                sp[0] = JS_DupValue(ctx, val);
                sp++;
            }
            BREAK;
        CASE(OP_put_var_ref_check):
            {
                int idx;
                idx = get_u16(pc);
                pc += 2;
                if (unlikely(JS_IsUninitialized(*var_refs[idx]->pvalue))) {
                    JS_ThrowReferenceErrorUninitialized2(ctx, b, idx, TRUE);
                    goto exception;
                }
                set_value(ctx, var_refs[idx]->pvalue, sp[-1]);
                sp--;
            }
            BREAK;
        CASE(OP_put_var_ref_check_init):
            {
                int idx;
                idx = get_u16(pc);
                pc += 2;
                if (unlikely(!JS_IsUninitialized(*var_refs[idx]->pvalue))) {
                    JS_ThrowReferenceErrorUninitialized2(ctx, b, idx, TRUE);
                    goto exception;
                }
                set_value(ctx, var_refs[idx]->pvalue, sp[-1]);
                sp--;
            }
            BREAK;
        CASE(OP_set_loc_uninitialized):
            {
                int idx;
                idx = get_u16(pc);
                pc += 2;
                set_value(ctx, &var_buf[idx], JS_UNINITIALIZED);
            }
            BREAK;
        CASE(OP_get_loc_check):
            {
                int idx;
                idx = get_u16(pc);
                pc += 2;
                if (unlikely(JS_IsUninitialized(var_buf[idx]))) {
                    JS_ThrowReferenceErrorUninitialized2(ctx, b, idx, FALSE);
                    goto exception;
                }
                sp[0] = JS_DupValue(ctx, var_buf[idx]);
                sp++;
            }
            BREAK;
        CASE(OP_get_loc_checkthis):
            {
                int idx;
                idx = get_u16(pc);
                pc += 2;
                if (unlikely(JS_IsUninitialized(var_buf[idx]))) {
                    JS_ThrowReferenceErrorUninitialized2(caller_ctx, b, idx, FALSE);
                    goto exception;
                }
                sp[0] = JS_DupValue(ctx, var_buf[idx]);
                sp++;
            }
            BREAK;
        CASE(OP_put_loc_check):
            {
                int idx;
                idx = get_u16(pc);
                pc += 2;
                if (unlikely(JS_IsUninitialized(var_buf[idx]))) {
                    JS_ThrowReferenceErrorUninitialized2(ctx, b, idx, FALSE);
                    goto exception;
                }
                set_value(ctx, &var_buf[idx], sp[-1]);
                sp--;
            }
            BREAK;
        CASE(OP_set_loc_check):
            {
                int idx;
                idx = get_u16(pc);
                pc += 2;
                if (unlikely(JS_IsUninitialized(var_buf[idx]))) {
                    JS_ThrowReferenceErrorUninitialized2(ctx, b, idx, FALSE);
                    goto exception;
                }
                set_value(ctx, &var_buf[idx], JS_DupValue(ctx, sp[-1]));
            }
            BREAK;
        CASE(OP_put_loc_check_init):
            {
                int idx;
                idx = get_u16(pc);
                pc += 2;
                if (unlikely(!JS_IsUninitialized(var_buf[idx]))) {
                    JS_ThrowReferenceError(ctx, "'this' can be initialized only once");
                    goto exception;
                }
                set_value(ctx, &var_buf[idx], sp[-1]);
                sp--;
            }
            BREAK;
        CASE(OP_close_loc):
            {
                int idx;
                idx = get_u16(pc);
                pc += 2;
                close_lexical_var(ctx, b, sf, idx);
            }
            BREAK;

        CASE(OP_make_loc_ref):
        CASE(OP_make_arg_ref):
        CASE(OP_make_var_ref_ref):
            {
                JSVarRef *var_ref;
                JSProperty *pr;
                JSAtom atom;
                int idx;
                atom = get_u32(pc);
                idx = get_u16(pc + 4);
                pc += 6;
                *sp++ = JS_NewObjectProto(ctx, JS_NULL);
                if (unlikely(JS_IsException(sp[-1])))
                    goto exception;
                if (opcode == OP_make_var_ref_ref) {
                    var_ref = var_refs[idx];
                    js_rc(var_ref)->ref_count++;
                } else {
                    var_ref = get_var_ref(ctx, sf, idx, opcode == OP_make_arg_ref);
                    if (!var_ref)
                        goto exception;
                }
                pr = add_property(ctx, JS_VALUE_GET_OBJ(sp[-1]), atom,
                                  JS_PROP_WRITABLE | JS_PROP_VARREF);
                if (!pr) {
                    free_var_ref(rt, var_ref);
                    goto exception;
                }
                pr->u.var_ref = var_ref;
                *sp++ = JS_AtomToValue(ctx, atom);
            }
            BREAK;
        CASE(OP_make_var_ref):
            {
                JSAtom atom;
                atom = get_u32(pc);
                pc += 4;
                sf->cur_pc = pc;

                if (JS_GetGlobalVarRef(ctx, atom, sp))
                    goto exception;
                sp += 2;
            }
            BREAK;

        CASE(OP_goto):
            pc += (int32_t)get_u32(pc);
            if (unlikely(js_poll_interrupts(ctx)))
                goto exception;
            BREAK;
#if SHORT_OPCODES
        CASE(OP_goto16):
            pc += (int16_t)get_u16(pc);
            if (unlikely(js_poll_interrupts(ctx)))
                goto exception;
            BREAK;
        CASE(OP_goto8):
            pc += (int8_t)pc[0];
            if (unlikely(js_poll_interrupts(ctx)))
                goto exception;
            BREAK;
#endif
        CASE(OP_if_true):
            {
                int res;
                JSValue op1;

                op1 = sp[-1];
                pc += 4;
                if ((uint32_t)JS_VALUE_GET_TAG(op1) <= JS_TAG_UNDEFINED) {
                    res = JS_VALUE_GET_INT(op1);
                } else {
                    res = JS_ToBoolFree(ctx, op1);
                }
                sp--;
                if (res) {
                    pc += (int32_t)get_u32(pc - 4) - 4;
                }
                if (unlikely(js_poll_interrupts(ctx)))
                    goto exception;
            }
            BREAK;
        CASE(OP_if_false):
            {
                int res;
                JSValue op1;

                op1 = sp[-1];
                pc += 4;
                /* quick and dirty test for JS_TAG_INT, JS_TAG_BOOL, JS_TAG_NULL and JS_TAG_UNDEFINED */
                if ((uint32_t)JS_VALUE_GET_TAG(op1) <= JS_TAG_UNDEFINED) {
                    res = JS_VALUE_GET_INT(op1);
                } else {
                    res = JS_ToBoolFree(ctx, op1);
                }
                sp--;
                if (!res) {
                    pc += (int32_t)get_u32(pc - 4) - 4;
                }
                if (unlikely(js_poll_interrupts(ctx)))
                    goto exception;
            }
            BREAK;
#if SHORT_OPCODES
        CASE(OP_if_true8):
            {
                int res;
                JSValue op1;

                op1 = sp[-1];
                pc += 1;
                if ((uint32_t)JS_VALUE_GET_TAG(op1) <= JS_TAG_UNDEFINED) {
                    res = JS_VALUE_GET_INT(op1);
                } else {
                    res = JS_ToBoolFree(ctx, op1);
                }
                sp--;
                if (res) {
                    pc += (int8_t)pc[-1] - 1;
                }
                if (unlikely(js_poll_interrupts(ctx)))
                    goto exception;
            }
            BREAK;
        CASE(OP_if_false8):
            {
                int res;
                JSValue op1;

                op1 = sp[-1];
                pc += 1;
                if ((uint32_t)JS_VALUE_GET_TAG(op1) <= JS_TAG_UNDEFINED) {
                    res = JS_VALUE_GET_INT(op1);
                } else {
                    res = JS_ToBoolFree(ctx, op1);
                }
                sp--;
                if (!res) {
                    pc += (int8_t)pc[-1] - 1;
                }
                if (unlikely(js_poll_interrupts(ctx)))
                    goto exception;
            }
            BREAK;
#endif
        CASE(OP_catch):
            {
                int32_t diff;
                diff = get_u32(pc);
                sp[0] = JS_NewCatchOffset(ctx, pc + diff - b->byte_code_buf);
                sp++;
                pc += 4;
            }
            BREAK;
        CASE(OP_gosub):
            {
                int32_t diff;
                diff = get_u32(pc);
                /* XXX: should have a different tag to avoid security flaw */
                sp[0] = JS_NewInt32(ctx, pc + 4 - b->byte_code_buf);
                sp++;
                pc += diff;
            }
            BREAK;
        CASE(OP_ret):
            {
                JSValue op1;
                uint32_t pos;
                op1 = sp[-1];
                if (unlikely(JS_VALUE_GET_TAG(op1) != JS_TAG_INT))
                    goto ret_fail;
                pos = JS_VALUE_GET_INT(op1);
                if (unlikely(pos >= b->byte_code_len)) {
                ret_fail:
                    JS_ThrowInternalError(ctx, "invalid ret value");
                    goto exception;
                }
                sp--;
                pc = b->byte_code_buf + pos;
            }
            BREAK;

        CASE(OP_for_in_start):
            sf->cur_pc = pc;
            if (js_for_in_start(ctx, sp))
                goto exception;
            BREAK;
        CASE(OP_for_in_next):
            sf->cur_pc = pc;
            if (js_for_in_next(ctx, sp))
                goto exception;
            sp += 2;
            BREAK;
        CASE(OP_for_of_start):
            sf->cur_pc = pc;
            if (js_for_of_start(ctx, sp, FALSE))
                goto exception;
            sp += 1;
            *sp++ = JS_NewCatchOffset(ctx, 0);
            BREAK;
        CASE(OP_for_of_next):
            {
                int offset = -3 - pc[0];
                pc += 1;
                sf->cur_pc = pc;
                if (js_for_of_next(ctx, sp, offset))
                    goto exception;
                sp += 2;
            }
            BREAK;
        CASE(OP_for_await_of_next):
            sf->cur_pc = pc;
            if (js_for_await_of_next(ctx, sp))
                goto exception;
            sp++;
            BREAK;
        CASE(OP_for_await_of_start):
            sf->cur_pc = pc;
            if (js_for_of_start(ctx, sp, TRUE))
                goto exception;
            sp += 1;
            *sp++ = JS_NewCatchOffset(ctx, 0);
            BREAK;
        CASE(OP_iterator_get_value_done):
            sf->cur_pc = pc;
            if (js_iterator_get_value_done(ctx, sp))
                goto exception;
            sp += 1;
            BREAK;
        CASE(OP_iterator_check_object):
            if (unlikely(!JS_IsObject(sp[-1]))) {
                JS_ThrowTypeError(ctx, "iterator must return an object");
                goto exception;
            }
            BREAK;

        CASE(OP_iterator_close):
            /* iter_obj next catch_offset -> */
            sp--; /* drop the catch offset to avoid getting caught by exception */
            JS_FreeValue(ctx, sp[-1]); /* drop the next method */
            sp--;
            if (!JS_IsUndefined(sp[-1])) {
                sf->cur_pc = pc;
                if (JS_IteratorClose(ctx, sp[-1], FALSE))
                    goto exception;
                JS_FreeValue(ctx, sp[-1]);
            }
            sp--;
            BREAK;
        CASE(OP_nip_catch):
            {
                JSValue ret_val;
                /* catch_offset ... ret_val -> ret_eval */
                ret_val = *--sp;
                while (sp > stack_buf &&
                       JS_VALUE_GET_TAG(sp[-1]) != JS_TAG_CATCH_OFFSET) {
                    JS_FreeValue(ctx, *--sp);
                }
                if (unlikely(sp == stack_buf)) {
                    JS_ThrowInternalError(ctx, "nip_catch");
                    JS_FreeValue(ctx, ret_val);
                    goto exception;
                }
                sp[-1] = ret_val;
            }
            BREAK;

        CASE(OP_iterator_next):
            /* stack: iter_obj next catch_offset val */
            {
                JSValue ret;
                sf->cur_pc = pc;
                ret = JS_Call(ctx, sp[-3], sp[-4],
                              1, (JSValueConst *)(sp - 1));
                if (JS_IsException(ret))
                    goto exception;
                JS_FreeValue(ctx, sp[-1]);
                sp[-1] = ret;
            }
            BREAK;

        CASE(OP_iterator_call):
            /* stack: iter_obj next catch_offset val */
            {
                JSValue method, ret;
                BOOL ret_flag;
                int flags;
                flags = *pc++;
                sf->cur_pc = pc;
                method = JS_GetProperty(ctx, sp[-4], (flags & 1) ?
                                        JS_ATOM_throw : JS_ATOM_return);
                if (JS_IsException(method))
                    goto exception;
                if (JS_IsUndefined(method) || JS_IsNull(method)) {
                    ret_flag = TRUE;
                } else {
                    if (flags & 2) {
                        /* no argument */
                        ret = JS_CallFree(ctx, method, sp[-4],
                                          0, NULL);
                    } else {
                        ret = JS_CallFree(ctx, method, sp[-4],
                                          1, (JSValueConst *)(sp - 1));
                    }
                    if (JS_IsException(ret))
                        goto exception;
                    JS_FreeValue(ctx, sp[-1]);
                    sp[-1] = ret;
                    ret_flag = FALSE;
                }
                sp[0] = JS_NewBool(ctx, ret_flag);
                sp += 1;
            }
            BREAK;

        CASE(OP_lnot):
            {
                int res;
                JSValue op1;

                op1 = sp[-1];
                if ((uint32_t)JS_VALUE_GET_TAG(op1) <= JS_TAG_UNDEFINED) {
                    res = JS_VALUE_GET_INT(op1) != 0;
                } else {
                    res = JS_ToBoolFree(ctx, op1);
                }
                sp[-1] = JS_NewBool(ctx, !res);
            }
            BREAK;

#define GET_FIELD_INLINE(name, keep, is_length)                         \
            {                                                           \
                JSValue val, obj;                                       \
                JSAtom atom;                                            \
                JSObject *p;                                            \
                JSProperty *pr;                                         \
                JSShapeProperty *prs;                                   \
                                                                        \
                if (is_length) {                                        \
                    atom = JS_ATOM_length;                              \
                } else {                                                \
                    atom = get_u32(pc);                                 \
                    pc += 4;                                            \
                }                                                       \
                                                                        \
                obj = sp[-1];                                           \
                if (likely(JS_VALUE_GET_TAG(obj) == JS_TAG_OBJECT)) {   \
                    p = JS_VALUE_GET_OBJ(obj);                          \
                    for(;;) {                                           \
                        prs = find_own_property(&pr, p, atom);          \
                        if (prs) {                                      \
                            /* found */                                 \
                            if (unlikely(prs->flags & JS_PROP_TMASK))   \
                                    goto name ## _slow_path;            \
                            val = JS_DupValue(ctx, pr->u.value);        \
                            break;                                      \
                        }                                               \
                        if (unlikely(p->is_exotic)) {                   \
                            /* XXX: should avoid the slow path for arrays \
                               and typed arrays by ensuring that 'prop' is \
                               not numeric */                           \
                            obj = JS_MKPTR(JS_TAG_OBJECT, p);           \
                            goto name ## _slow_path;                    \
                        }                                               \
                        p = p->shape->proto;                            \
                        if (!p) {                                       \
                            val = JS_UNDEFINED;                         \
                            break;                                      \
                        }                                               \
                    }                                                   \
                } else {                                                \
                name ## _slow_path:                                     \
                    sf->cur_pc = pc;                                    \
                    val = JS_GetPropertyInternal(ctx, obj, atom, sp[-1], 0); \
                    if (unlikely(JS_IsException(val)))                  \
                        goto exception;                                 \
                }                                                       \
                if (keep) {                                             \
                    *sp++ = val;                                        \
                } else {                                                \
                    JS_FreeValue(ctx, sp[-1]);                          \
                    sp[-1] = val;                                       \
                }                                                       \
            }


        CASE(OP_get_field):
            GET_FIELD_INLINE(get_field, 0, 0);
            BREAK;

        CASE(OP_get_field2):
            GET_FIELD_INLINE(get_field2, 1, 0);
            BREAK;

#if SHORT_OPCODES
        CASE(OP_get_length):
            GET_FIELD_INLINE(get_length, 0, 1);
            BREAK;
#endif

        CASE(OP_put_field):
            {
                int ret;
                JSValue obj;
                JSAtom atom;
                JSObject *p;
                JSProperty *pr;
                JSShapeProperty *prs;

                atom = get_u32(pc);
                pc += 4;

                obj = sp[-2];
                if (likely(JS_VALUE_GET_TAG(obj) == JS_TAG_OBJECT)) {
                    p = JS_VALUE_GET_OBJ(obj);
                    prs = find_own_property(&pr, p, atom);
                    if (!prs)
                        goto put_field_slow_path;
                    if (likely((prs->flags & (JS_PROP_TMASK | JS_PROP_WRITABLE |
                                              JS_PROP_LENGTH)) == JS_PROP_WRITABLE)) {
                        /* fast path */
                        set_value(ctx, &pr->u.value, sp[-1]);
                    } else {
                        goto put_field_slow_path;
                    }
                    JS_FreeValue(ctx, obj);
                    sp -= 2;
                } else {
                put_field_slow_path:
                    sf->cur_pc = pc;
                    ret = JS_SetPropertyInternal(ctx, obj, atom, sp[-1], obj,
                                                 JS_PROP_THROW_STRICT);
                    JS_FreeValue(ctx, obj);
                    sp -= 2;
                    if (unlikely(ret < 0))
                        goto exception;
                }

            }
            BREAK;

        CASE(OP_private_symbol):
            {
                JSAtom atom;
                JSValue val;

                atom = get_u32(pc);
                pc += 4;
                val = JS_NewSymbolFromAtom(ctx, atom, JS_ATOM_TYPE_PRIVATE);
                if (JS_IsException(val))
                    goto exception;
                *sp++ = val;
            }
            BREAK;

        CASE(OP_get_private_field):
            {
                JSValue val;

                val = JS_GetPrivateField(ctx, sp[-2], sp[-1]);
                JS_FreeValue(ctx, sp[-1]);
                JS_FreeValue(ctx, sp[-2]);
                sp[-2] = val;
                sp--;
                if (unlikely(JS_IsException(val)))
                    goto exception;
            }
            BREAK;

        CASE(OP_put_private_field):
            {
                int ret;
                ret = JS_SetPrivateField(ctx, sp[-3], sp[-1], sp[-2]);
                JS_FreeValue(ctx, sp[-3]);
                JS_FreeValue(ctx, sp[-1]);
                sp -= 3;
                if (unlikely(ret < 0))
                    goto exception;
            }
            BREAK;

        CASE(OP_define_private_field):
            {
                int ret;
                ret = JS_DefinePrivateField(ctx, sp[-3], sp[-2], sp[-1]);
                JS_FreeValue(ctx, sp[-2]);
                sp -= 2;
                if (unlikely(ret < 0))
                    goto exception;
            }
            BREAK;

        CASE(OP_define_field):
            {
                int ret;
                JSAtom atom;
                atom = get_u32(pc);
                pc += 4;

                ret = JS_DefinePropertyValue(ctx, sp[-2], atom, sp[-1],
                                             JS_PROP_C_W_E | JS_PROP_THROW);
                sp--;
                if (unlikely(ret < 0))
                    goto exception;
            }
            BREAK;

        CASE(OP_set_name):
            {
                int ret;
                JSAtom atom;
                atom = get_u32(pc);
                pc += 4;

                ret = JS_DefineObjectName(ctx, sp[-1], atom, JS_PROP_CONFIGURABLE);
                if (unlikely(ret < 0))
                    goto exception;
            }
            BREAK;
        CASE(OP_set_name_computed):
            {
                int ret;
                ret = JS_DefineObjectNameComputed(ctx, sp[-1], sp[-2], JS_PROP_CONFIGURABLE);
                if (unlikely(ret < 0))
                    goto exception;
            }
            BREAK;
        CASE(OP_set_proto):
            {
                JSValue proto;
                sf->cur_pc = pc;
                proto = sp[-1];
                if (JS_IsObject(proto) || JS_IsNull(proto)) {
                    if (JS_SetPrototypeInternal(ctx, sp[-2], proto, TRUE) < 0)
                        goto exception;
                }
                JS_FreeValue(ctx, proto);
                sp--;
            }
            BREAK;
        CASE(OP_set_home_object):
            js_method_set_home_object(ctx, sp[-1], sp[-2]);
            BREAK;
        CASE(OP_define_method):
        CASE(OP_define_method_computed):
            {
                JSValue getter, setter, value;
                JSValueConst obj;
                JSAtom atom;
                int flags, ret, op_flags;
                BOOL is_computed;
#define OP_DEFINE_METHOD_METHOD 0
#define OP_DEFINE_METHOD_GETTER 1
#define OP_DEFINE_METHOD_SETTER 2
#define OP_DEFINE_METHOD_ENUMERABLE 4

                is_computed = (opcode == OP_define_method_computed);
                if (is_computed) {
                    atom = JS_ValueToAtom(ctx, sp[-2]);
                    if (unlikely(atom == JS_ATOM_NULL))
                        goto exception;
                    opcode += OP_define_method - OP_define_method_computed;
                } else {
                    atom = get_u32(pc);
                    pc += 4;
                }
                op_flags = *pc++;

                obj = sp[-2 - is_computed];
                flags = JS_PROP_HAS_CONFIGURABLE | JS_PROP_CONFIGURABLE |
                    JS_PROP_HAS_ENUMERABLE | JS_PROP_THROW;
                if (op_flags & OP_DEFINE_METHOD_ENUMERABLE)
                    flags |= JS_PROP_ENUMERABLE;
                op_flags &= 3;
                value = JS_UNDEFINED;
                getter = JS_UNDEFINED;
                setter = JS_UNDEFINED;
                if (op_flags == OP_DEFINE_METHOD_METHOD) {
                    value = sp[-1];
                    flags |= JS_PROP_HAS_VALUE | JS_PROP_HAS_WRITABLE | JS_PROP_WRITABLE;
                } else if (op_flags == OP_DEFINE_METHOD_GETTER) {
                    getter = sp[-1];
                    flags |= JS_PROP_HAS_GET;
                } else {
                    setter = sp[-1];
                    flags |= JS_PROP_HAS_SET;
                }
                ret = js_method_set_properties(ctx, sp[-1], atom, flags, obj);
                if (ret >= 0) {
                    ret = JS_DefineProperty(ctx, obj, atom, value,
                                            getter, setter, flags);
                }
                JS_FreeValue(ctx, sp[-1]);
                if (is_computed) {
                    JS_FreeAtom(ctx, atom);
                    JS_FreeValue(ctx, sp[-2]);
                }
                sp -= 1 + is_computed;
                if (unlikely(ret < 0))
                    goto exception;
            }
            BREAK;

        CASE(OP_define_class):
        CASE(OP_define_class_computed):
            {
                int class_flags;
                JSAtom atom;

                atom = get_u32(pc);
                class_flags = pc[4];
                pc += 5;
                if (js_op_define_class(ctx, sp, atom, class_flags,
                                       var_refs, sf,
                                       (opcode == OP_define_class_computed)) < 0)
                    goto exception;
            }
            BREAK;

#define GET_ARRAY_EL_INLINE(name, keep)                                 \
            {                                                           \
                JSValue val, obj, prop;                                 \
                JSObject *p;                                            \
                uint32_t idx;                                           \
                                                                        \
                obj = sp[-2];                                           \
                prop = sp[-1];                                          \
                if (likely(JS_VALUE_GET_TAG(obj) == JS_TAG_OBJECT &&    \
                           JS_VALUE_GET_TAG(prop) == JS_TAG_INT)) {     \
                    p = JS_VALUE_GET_OBJ(obj);                          \
                    idx = JS_VALUE_GET_INT(prop);                       \
                    if (unlikely(p->class_id != JS_CLASS_ARRAY))        \
                        goto name ## _slow_path;                        \
                    if (unlikely(idx >= p->u.array.count))              \
                        goto name ## _slow_path;                        \
                    val = JS_DupValue(ctx, p->u.array.u.values[idx]);   \
                } else {                                                \
                    name ## _slow_path:                                 \
                    sf->cur_pc = pc;                                    \
                    val = JS_GetPropertyValue(ctx, obj, prop);          \
                    if (unlikely(JS_IsException(val))) {                \
                        if (keep)                                       \
                            sp[-1] = JS_UNDEFINED;                      \
                        else                                            \
                            sp--;                                       \
                        goto exception;                                 \
                    }                                                   \
                }                                                       \
                if (keep) {                                             \
                    sp[-1] = val;                                       \
                } else {                                                \
                    JS_FreeValue(ctx, obj);                             \
                    sp[-2] = val;                                       \
                    sp--;                                               \
                }                                                       \
            }

        CASE(OP_get_array_el):
            GET_ARRAY_EL_INLINE(get_array_el, 0);
            BREAK;

        CASE(OP_get_array_el2):
            GET_ARRAY_EL_INLINE(get_array_el2, 1);
            BREAK;

        CASE(OP_get_array_el3):
            {
                JSValue val;
                JSObject *p;
                uint32_t idx;

                if (likely(JS_VALUE_GET_TAG(sp[-2]) == JS_TAG_OBJECT &&
                           JS_VALUE_GET_TAG(sp[-1]) == JS_TAG_INT)) {
                    p = JS_VALUE_GET_OBJ(sp[-2]);
                    idx = JS_VALUE_GET_INT(sp[-1]);
                    if (unlikely(p->class_id != JS_CLASS_ARRAY))
                        goto get_array_el3_slow_path;
                    if (unlikely(idx >= p->u.array.count))
                        goto get_array_el3_slow_path;
                    val = JS_DupValue(ctx, p->u.array.u.values[idx]);
                } else {
                get_array_el3_slow_path:
                    switch (JS_VALUE_GET_TAG(sp[-1])) {
                    case JS_TAG_INT:
                    case JS_TAG_STRING:
                    case JS_TAG_SYMBOL:
                        /* undefined and null are tested in JS_GetPropertyValue() */
                        break;
                    default:
                        /* must be tested before JS_ToPropertyKey */
                        if (unlikely(JS_IsUndefined(sp[-2]) || JS_IsNull(sp[-2]))) {
                            JS_ThrowTypeError(ctx, "value has no property");
                            goto exception;
                        }
                        sf->cur_pc = pc;
                        ret_val = JS_ToPropertyKey(ctx, sp[-1]);
                        if (JS_IsException(ret_val))
                            goto exception;
                        JS_FreeValue(ctx, sp[-1]);
                        sp[-1] = ret_val;
                        break;
                    }
                    sf->cur_pc = pc;
                    val = JS_GetPropertyValue(ctx, sp[-2], JS_DupValue(ctx, sp[-1]));
                    if (unlikely(JS_IsException(val)))
                        goto exception;
                }
                *sp++ = val;
            }
            BREAK;

        CASE(OP_get_ref_value):
            {
                JSValue val;
                JSAtom atom;
                int ret;

                sf->cur_pc = pc;
                atom = JS_ValueToAtom(ctx, sp[-1]);
                if (atom == JS_ATOM_NULL)
                    goto exception;
                if (unlikely(JS_IsUndefined(sp[-2]))) {
                    JS_ThrowReferenceErrorNotDefined(ctx, atom);
                    JS_FreeAtom(ctx, atom);
                    goto exception;
                }
                ret = JS_HasProperty(ctx, sp[-2], atom);
                if (unlikely(ret <= 0)) {
                    if (ret < 0) {
                        JS_FreeAtom(ctx, atom);
                        goto exception;
                    }
                    if (is_strict_mode(ctx)) {
                        JS_ThrowReferenceErrorNotDefined(ctx, atom);
                        JS_FreeAtom(ctx, atom);
                        goto exception;
                    }
                    val = JS_UNDEFINED;
                } else {
                    val = JS_GetProperty(ctx, sp[-2], atom);
                }
                JS_FreeAtom(ctx, atom);
                if (unlikely(JS_IsException(val)))
                    goto exception;
                sp[0] = val;
                sp++;
            }
            BREAK;

        CASE(OP_get_super_value):
            {
                JSValue val;
                JSAtom atom;
                sf->cur_pc = pc;
                atom = JS_ValueToAtom(ctx, sp[-1]);
                if (unlikely(atom == JS_ATOM_NULL))
                    goto exception;
                val = JS_GetPropertyInternal(ctx, sp[-2], atom, sp[-3], FALSE);
                JS_FreeAtom(ctx, atom);
                if (unlikely(JS_IsException(val)))
                    goto exception;
                JS_FreeValue(ctx, sp[-1]);
                JS_FreeValue(ctx, sp[-2]);
                JS_FreeValue(ctx, sp[-3]);
                sp[-3] = val;
                sp -= 2;
            }
            BREAK;

        CASE(OP_put_array_el):
            {
                int ret;
                JSObject *p;
                uint32_t idx;

                if (likely(JS_VALUE_GET_TAG(sp[-3]) == JS_TAG_OBJECT &&
                           JS_VALUE_GET_TAG(sp[-2]) == JS_TAG_INT)) {
                    p = JS_VALUE_GET_OBJ(sp[-3]);
                    idx = JS_VALUE_GET_INT(sp[-2]);
                    if (unlikely(p->class_id != JS_CLASS_ARRAY))
                        goto put_array_el_slow_path;
                    if (unlikely(idx >= (uint32_t)p->u.array.count)) {
                        uint32_t new_len, array_len;
                        if (unlikely(idx != (uint32_t)p->u.array.count ||
                                     !p->fast_array ||
                                     !can_extend_fast_array(p))) {
                            goto put_array_el_slow_path;
                        }
                        if (likely(JS_VALUE_GET_TAG(p->prop[0].u.value) != JS_TAG_INT))
                            goto put_array_el_slow_path;
                        /* cannot overflow otherwise the length would not be an integer */
                        new_len = idx + 1;
                        if (unlikely(new_len > p->u.array.u1.size))
                            goto put_array_el_slow_path;
                        array_len = JS_VALUE_GET_INT(p->prop[0].u.value);
                        if (new_len > array_len) {
                            if (unlikely(!(get_shape_prop(p->shape)->flags & JS_PROP_WRITABLE)))
                                goto put_array_el_slow_path;
                            p->prop[0].u.value = JS_NewInt32(ctx, new_len);
                        }
                        p->u.array.count = new_len;
                        p->u.array.u.values[idx] = sp[-1];
                    } else {
                        set_value(ctx, &p->u.array.u.values[idx], sp[-1]);
                    }
                    JS_FreeValue(ctx, sp[-3]);
                    sp -= 3;
                } else {
                put_array_el_slow_path:
                    sf->cur_pc = pc;
                    ret = JS_SetPropertyValue(ctx, sp[-3], sp[-2], sp[-1], JS_PROP_THROW_STRICT);
                    JS_FreeValue(ctx, sp[-3]);
                    sp -= 3;
                    if (unlikely(ret < 0))
                        goto exception;
                }
            }
            BREAK;

        CASE(OP_put_ref_value):
            {
                int ret;
                JSAtom atom;
                sf->cur_pc = pc;
                atom = JS_ValueToAtom(ctx, sp[-2]);
                if (unlikely(atom == JS_ATOM_NULL))
                    goto exception;
                if (unlikely(JS_IsUndefined(sp[-3]))) {
                    if (is_strict_mode(ctx)) {
                        JS_ThrowReferenceErrorNotDefined(ctx, atom);
                        JS_FreeAtom(ctx, atom);
                        goto exception;
                    } else {
                        sp[-3] = JS_DupValue(ctx, ctx->global_obj);
                    }
                }
                ret = JS_HasProperty(ctx, sp[-3], atom);
                if (unlikely(ret <= 0)) {
                    if (unlikely(ret < 0)) {
                        JS_FreeAtom(ctx, atom);
                        goto exception;
                    }
                    if (is_strict_mode(ctx)) {
                        JS_ThrowReferenceErrorNotDefined(ctx, atom);
                        JS_FreeAtom(ctx, atom);
                        goto exception;
                    }
                }
                ret = JS_SetPropertyInternal(ctx, sp[-3], atom, sp[-1], sp[-3], JS_PROP_THROW_STRICT);
                JS_FreeAtom(ctx, atom);
                JS_FreeValue(ctx, sp[-2]);
                JS_FreeValue(ctx, sp[-3]);
                sp -= 3;
                if (unlikely(ret < 0))
                    goto exception;
            }
            BREAK;

        CASE(OP_put_super_value):
            {
                int ret;
                JSAtom atom;
                sf->cur_pc = pc;
                if (JS_VALUE_GET_TAG(sp[-3]) != JS_TAG_OBJECT) {
                    JS_ThrowTypeErrorNotAnObject(ctx);
                    goto exception;
                }
                atom = JS_ValueToAtom(ctx, sp[-2]);
                if (unlikely(atom == JS_ATOM_NULL))
                    goto exception;
                ret = JS_SetPropertyInternal(ctx, sp[-3], atom, sp[-1], sp[-4],
                                             JS_PROP_THROW_STRICT);
                JS_FreeAtom(ctx, atom);
                JS_FreeValue(ctx, sp[-4]);
                JS_FreeValue(ctx, sp[-3]);
                JS_FreeValue(ctx, sp[-2]);
                sp -= 4;
                if (ret < 0)
                    goto exception;
            }
            BREAK;

        CASE(OP_define_array_el):
            {
                int ret;
                ret = JS_DefinePropertyValueValue(ctx, sp[-3], JS_DupValue(ctx, sp[-2]), sp[-1],
                                                  JS_PROP_C_W_E | JS_PROP_THROW);
                sp -= 1;
                if (unlikely(ret < 0))
                    goto exception;
            }
            BREAK;

        CASE(OP_append):    /* array pos enumobj -- array pos */
            {
                sf->cur_pc = pc;
                if (js_append_enumerate(ctx, sp))
                    goto exception;
                JS_FreeValue(ctx, *--sp);
            }
            BREAK;

        CASE(OP_copy_data_properties):    /* target source excludeList */
            {
                /* stack offsets (-1 based):
                   2 bits for target,
                   3 bits for source,
                   2 bits for exclusionList */
                int mask;

                mask = *pc++;
                sf->cur_pc = pc;
                if (JS_CopyDataProperties(ctx, sp[-1 - (mask & 3)],
                                          sp[-1 - ((mask >> 2) & 7)],
                                          sp[-1 - ((mask >> 5) & 7)], 0))
                    goto exception;
            }
            BREAK;

        CASE(OP_add):
            {
                JSValue op1, op2;
                op1 = sp[-2];
                op2 = sp[-1];
                if (likely(JS_VALUE_IS_BOTH_INT(op1, op2))) {
                    int64_t r;
                    r = (int64_t)JS_VALUE_GET_INT(op1) + JS_VALUE_GET_INT(op2);
                    if (unlikely((int)r != r)) {
                        sp[-2] = __JS_NewFloat64(ctx, (double)r);
                    } else {
                        sp[-2] = JS_NewInt32(ctx, r);
                    }
                    sp--;
                } else if (JS_TAG_IS_FLOAT64(JS_VALUE_GET_TAG(op1)) ||
                           JS_TAG_IS_FLOAT64(JS_VALUE_GET_TAG(op2))) {
                    double d1, d2;
                    if (JS_TAG_IS_FLOAT64(JS_VALUE_GET_TAG(op1))) {
                        d1 = JS_VALUE_GET_FLOAT64(op1);
                    } else if (JS_VALUE_GET_TAG(op1) == JS_TAG_INT) {
                        d1 = JS_VALUE_GET_INT(op1);
                    } else {
                        goto add_slow_case;
                    }
                    if (JS_TAG_IS_FLOAT64(JS_VALUE_GET_TAG(op2))) {
                        d2 = JS_VALUE_GET_FLOAT64(op2);
                    } else if (JS_VALUE_GET_TAG(op2) == JS_TAG_INT) {
                        d2 = JS_VALUE_GET_INT(op2);
                    } else {
                        goto add_slow_case;
                    }
                    sp[-2] = __JS_NewFloat64(ctx, d1 + d2);
                    sp--;
                } else if (JS_IsString(op1) && JS_IsString(op2)) {
                    sp[-2] = JS_ConcatString(ctx, op1, op2);
                    sp--;
                    if (JS_IsException(sp[-1]))
                        goto exception;
                } else {
                add_slow_case:
                    sf->cur_pc = pc;
                    if (js_add_slow(ctx, sp))
                        goto exception;
                    sp--;
                }
            }
            BREAK;
        CASE(OP_add_loc):
            {
                JSValue op2;
                JSValue *pv;
                int idx;
                idx = *pc;
                pc += 1;

                op2 = sp[-1];
                pv = &var_buf[idx];
                if (likely(JS_VALUE_IS_BOTH_INT(*pv, op2))) {
                    int64_t r;
                    r = (int64_t)JS_VALUE_GET_INT(*pv) + JS_VALUE_GET_INT(op2);
                    if (unlikely((int)r != r)) {
                        *pv = __JS_NewFloat64(ctx, (double)r);
                    } else {
                        *pv = JS_NewInt32(ctx, r);
                    }
                    sp--;
                } else if (JS_VALUE_IS_BOTH_FLOAT(*pv, op2)) {
                    *pv = __JS_NewFloat64(ctx, JS_VALUE_GET_FLOAT64(*pv) +
                                               JS_VALUE_GET_FLOAT64(op2));
                    sp--;
                } else if (JS_VALUE_GET_TAG(*pv) == JS_TAG_STRING &&
                           JS_VALUE_GET_TAG(op2) == JS_TAG_STRING) {
                    sp--;
                    sf->cur_pc = pc;
                    if (JS_ConcatStringInPlace(ctx, JS_VALUE_GET_STRING(*pv), op2)) {
                        JS_FreeValue(ctx, op2);
                    } else {
                        op2 = JS_ConcatString(ctx, JS_DupValue(ctx, *pv), op2);
                        if (JS_IsException(op2))
                            goto exception;
                        set_value(ctx, pv, op2);
                    }
                } else {
                    JSValue ops[2];
                    /* In case of exception, js_add_slow frees ops[0]
                       and ops[1], so we must duplicate *pv */
                    sf->cur_pc = pc;
                    ops[0] = JS_DupValue(ctx, *pv);
                    ops[1] = op2;
                    sp--;
                    if (js_add_slow(ctx, ops + 2))
                        goto exception;
                    set_value(ctx, pv, ops[0]);
                }
            }
            BREAK;
        CASE(OP_sub):
            {
                JSValue op1, op2;
                op1 = sp[-2];
                op2 = sp[-1];
                if (likely(JS_VALUE_IS_BOTH_INT(op1, op2))) {
                    int64_t r;
                    r = (int64_t)JS_VALUE_GET_INT(op1) - JS_VALUE_GET_INT(op2);
                    if (unlikely((int)r != r)) {
                        sp[-2] = __JS_NewFloat64(ctx, (double)r);
                    } else {
                        sp[-2] = JS_NewInt32(ctx, r);
                    }
                    sp--;
                } else if (JS_TAG_IS_FLOAT64(JS_VALUE_GET_TAG(op1)) ||
                           JS_TAG_IS_FLOAT64(JS_VALUE_GET_TAG(op2))) {
                    double d1, d2;
                    if (JS_TAG_IS_FLOAT64(JS_VALUE_GET_TAG(op1))) {
                        d1 = JS_VALUE_GET_FLOAT64(op1);
                    } else if (JS_VALUE_GET_TAG(op1) == JS_TAG_INT) {
                        d1 = JS_VALUE_GET_INT(op1);
                    } else {
                        goto binary_arith_slow;
                    }
                    if (JS_TAG_IS_FLOAT64(JS_VALUE_GET_TAG(op2))) {
                        d2 = JS_VALUE_GET_FLOAT64(op2);
                    } else if (JS_VALUE_GET_TAG(op2) == JS_TAG_INT) {
                        d2 = JS_VALUE_GET_INT(op2);
                    } else {
                        goto binary_arith_slow;
                    }
                    sp[-2] = __JS_NewFloat64(ctx, d1 - d2);
                    sp--;
                } else {
                    goto binary_arith_slow;
                }
            }
            BREAK;
        CASE(OP_mul):
            {
                JSValue op1, op2;
                double d;
                op1 = sp[-2];
                op2 = sp[-1];
                if (likely(JS_VALUE_IS_BOTH_INT(op1, op2))) {
                    int32_t v1, v2;
                    int64_t r;
                    v1 = JS_VALUE_GET_INT(op1);
                    v2 = JS_VALUE_GET_INT(op2);
                    r = (int64_t)v1 * v2;
                    if (unlikely((int)r != r)) {
                        d = (double)r;
                        goto mul_fp_res;
                    }
                    /* need to test zero case for -0 result */
                    if (unlikely(r == 0 && (v1 | v2) < 0)) {
                        d = -0.0;
                        goto mul_fp_res;
                    }
                    sp[-2] = JS_NewInt32(ctx, r);
                    sp--;
                } else if (JS_TAG_IS_FLOAT64(JS_VALUE_GET_TAG(op1)) ||
                           JS_TAG_IS_FLOAT64(JS_VALUE_GET_TAG(op2))) {
                    double d1, d2;
                    if (JS_TAG_IS_FLOAT64(JS_VALUE_GET_TAG(op1))) {
                        d1 = JS_VALUE_GET_FLOAT64(op1);
                    } else if (JS_VALUE_GET_TAG(op1) == JS_TAG_INT) {
                        d1 = JS_VALUE_GET_INT(op1);
                    } else {
                        goto binary_arith_slow;
                    }
                    if (JS_TAG_IS_FLOAT64(JS_VALUE_GET_TAG(op2))) {
                        d2 = JS_VALUE_GET_FLOAT64(op2);
                    } else if (JS_VALUE_GET_TAG(op2) == JS_TAG_INT) {
                        d2 = JS_VALUE_GET_INT(op2);
                    } else {
                        goto binary_arith_slow;
                    }
                    d = d1 * d2;
                mul_fp_res:
                    sp[-2] = __JS_NewFloat64(ctx, d);
                    sp--;
                } else {
                    goto binary_arith_slow;
                }
            }
            BREAK;
        CASE(OP_div):
            {
                JSValue op1, op2;
                op1 = sp[-2];
                op2 = sp[-1];
                if (likely(JS_VALUE_IS_BOTH_INT(op1, op2))) {
                    int v1, v2;
                    v1 = JS_VALUE_GET_INT(op1);
                    v2 = JS_VALUE_GET_INT(op2);
                    sp[-2] = JS_NewFloat64(ctx, (double)v1 / (double)v2);
                    sp--;
                } else {
                    goto binary_arith_slow;
                }
            }
            BREAK;
        CASE(OP_mod):
            {
                JSValue op1, op2;
                op1 = sp[-2];
                op2 = sp[-1];
                if (likely(JS_VALUE_IS_BOTH_INT(op1, op2))) {
                    int v1, v2, r;
                    v1 = JS_VALUE_GET_INT(op1);
                    v2 = JS_VALUE_GET_INT(op2);
                    /* We must avoid v2 = 0, v1 = INT32_MIN and v2 =
                       -1 and the cases where the result is -0. */
                    if (unlikely(v1 < 0 || v2 <= 0))
                        goto binary_arith_slow;
                    r = v1 % v2;
                    sp[-2] = JS_NewInt32(ctx, r);
                    sp--;
                } else {
                    goto binary_arith_slow;
                }
            }
            BREAK;
        CASE(OP_pow):
        binary_arith_slow:
            sf->cur_pc = pc;
            if (js_binary_arith_slow(ctx, sp, opcode))
                goto exception;
            sp--;
            BREAK;

        CASE(OP_plus):
            {
                JSValue op1;
                uint32_t tag;
                op1 = sp[-1];
                tag = JS_VALUE_GET_TAG(op1);
                if (tag == JS_TAG_INT || JS_TAG_IS_FLOAT64(tag)) {
                } else if (tag == JS_TAG_NULL || tag == JS_TAG_BOOL) {
                    sp[-1] = JS_NewInt32(ctx, JS_VALUE_GET_INT(op1));
                } else {
                    sf->cur_pc = pc;
                    if (js_unary_arith_slow(ctx, sp, opcode))
                        goto exception;
                }
            }
            BREAK;
        CASE(OP_neg):
            {
                JSValue op1;
                uint32_t tag;
                int val;
                double d;
                op1 = sp[-1];
                tag = JS_VALUE_GET_TAG(op1);
                if (tag == JS_TAG_INT ||
                    tag == JS_TAG_BOOL ||
                    tag == JS_TAG_NULL) {
                    val = JS_VALUE_GET_INT(op1);
                    /* Note: -0 cannot be expressed as integer */
                    if (unlikely(val == 0)) {
                        d = -0.0;
                        goto neg_fp_res;
                    }
                    if (unlikely(val == INT32_MIN)) {
                        d = -(double)val;
                        goto neg_fp_res;
                    }
                    sp[-1] = JS_NewInt32(ctx, -val);
                } else if (JS_TAG_IS_FLOAT64(tag)) {
                    d = -JS_VALUE_GET_FLOAT64(op1);
                neg_fp_res:
                    sp[-1] = __JS_NewFloat64(ctx, d);
                } else {
                    sf->cur_pc = pc;
                    if (js_unary_arith_slow(ctx, sp, opcode))
                        goto exception;
                }
            }
            BREAK;
        CASE(OP_inc):
            {
                JSValue op1;
                int val;
                op1 = sp[-1];
                if (JS_VALUE_GET_TAG(op1) == JS_TAG_INT) {
                    val = JS_VALUE_GET_INT(op1);
                    if (unlikely(val == INT32_MAX))
                        goto inc_slow;
                    sp[-1] = JS_NewInt32(ctx, val + 1);
                } else {
                inc_slow:
                    sf->cur_pc = pc;
                    if (js_unary_arith_slow(ctx, sp, opcode))
                        goto exception;
                }
            }
            BREAK;
        CASE(OP_dec):
            {
                JSValue op1;
                int val;
                op1 = sp[-1];
                if (JS_VALUE_GET_TAG(op1) == JS_TAG_INT) {
                    val = JS_VALUE_GET_INT(op1);
                    if (unlikely(val == INT32_MIN))
                        goto dec_slow;
                    sp[-1] = JS_NewInt32(ctx, val - 1);
                } else {
                dec_slow:
                    sf->cur_pc = pc;
                    if (js_unary_arith_slow(ctx, sp, opcode))
                        goto exception;
                }
            }
            BREAK;
        CASE(OP_post_inc):
            {
                JSValue op1;
                int val;
                op1 = sp[-1];
                if (JS_VALUE_GET_TAG(op1) == JS_TAG_INT) {
                    val = JS_VALUE_GET_INT(op1);
                    if (unlikely(val == INT32_MAX))
                        goto post_inc_slow;
                    sp[0] = JS_NewInt32(ctx, val + 1);
                } else {
                post_inc_slow:
                    sf->cur_pc = pc;
                    if (js_post_inc_slow(ctx, sp, opcode))
                        goto exception;
                }
                sp++;
            }
            BREAK;
        CASE(OP_post_dec):
            {
                JSValue op1;
                int val;
                op1 = sp[-1];
                if (JS_VALUE_GET_TAG(op1) == JS_TAG_INT) {
                    val = JS_VALUE_GET_INT(op1);
                    if (unlikely(val == INT32_MIN))
                        goto post_dec_slow;
                    sp[0] = JS_NewInt32(ctx, val - 1);
                } else {
                post_dec_slow:
                    sf->cur_pc = pc;
                    if (js_post_inc_slow(ctx, sp, opcode))
                        goto exception;
                }
                sp++;
            }
            BREAK;
        CASE(OP_inc_loc):
            {
                JSValue op1;
                int val;
                int idx;
                idx = *pc;
                pc += 1;

                op1 = var_buf[idx];
                if (JS_VALUE_GET_TAG(op1) == JS_TAG_INT) {
                    val = JS_VALUE_GET_INT(op1);
                    if (unlikely(val == INT32_MAX))
                        goto inc_loc_slow;
                    var_buf[idx] = JS_NewInt32(ctx, val + 1);
                } else {
                inc_loc_slow:
                    sf->cur_pc = pc;
                    /* must duplicate otherwise the variable value may
                       be destroyed before JS code accesses it */
                    op1 = JS_DupValue(ctx, op1);
                    if (js_unary_arith_slow(ctx, &op1 + 1, OP_inc))
                        goto exception;
                    set_value(ctx, &var_buf[idx], op1);
                }
            }
            BREAK;
        CASE(OP_dec_loc):
            {
                JSValue op1;
                int val;
                int idx;
                idx = *pc;
                pc += 1;

                op1 = var_buf[idx];
                if (JS_VALUE_GET_TAG(op1) == JS_TAG_INT) {
                    val = JS_VALUE_GET_INT(op1);
                    if (unlikely(val == INT32_MIN))
                        goto dec_loc_slow;
                    var_buf[idx] = JS_NewInt32(ctx, val - 1);
                } else {
                dec_loc_slow:
                    sf->cur_pc = pc;
                    /* must duplicate otherwise the variable value may
                       be destroyed before JS code accesses it */
                    op1 = JS_DupValue(ctx, op1);
                    if (js_unary_arith_slow(ctx, &op1 + 1, OP_dec))
                        goto exception;
                    set_value(ctx, &var_buf[idx], op1);
                }
            }
            BREAK;
        CASE(OP_not):
            {
                JSValue op1;
                op1 = sp[-1];
                if (JS_VALUE_GET_TAG(op1) == JS_TAG_INT) {
                    sp[-1] = JS_NewInt32(ctx, ~JS_VALUE_GET_INT(op1));
                } else {
                    sf->cur_pc = pc;
                    if (js_not_slow(ctx, sp))
                        goto exception;
                }
            }
            BREAK;

        CASE(OP_shl):
            {
                JSValue op1, op2;
                op1 = sp[-2];
                op2 = sp[-1];
                if (likely(JS_VALUE_IS_BOTH_INT(op1, op2))) {
                    uint32_t v1, v2;
                    v1 = JS_VALUE_GET_INT(op1);
                    v2 = JS_VALUE_GET_INT(op2);
                    v2 &= 0x1f;
                    sp[-2] = JS_NewInt32(ctx, v1 << v2);
                    sp--;
                } else {
                    sf->cur_pc = pc;
                    if (js_binary_logic_slow(ctx, sp, opcode))
                        goto exception;
                    sp--;
                }
            }
            BREAK;
        CASE(OP_shr):
            {
                JSValue op1, op2;
                op1 = sp[-2];
                op2 = sp[-1];
                if (likely(JS_VALUE_IS_BOTH_INT(op1, op2))) {
                    uint32_t v2;
                    v2 = JS_VALUE_GET_INT(op2);
                    v2 &= 0x1f;
                    sp[-2] = JS_NewUint32(ctx,
                                          (uint32_t)JS_VALUE_GET_INT(op1) >>
                                          v2);
                    sp--;
                } else {
                    sf->cur_pc = pc;
                    if (js_shr_slow(ctx, sp))
                        goto exception;
                    sp--;
                }
            }
            BREAK;
        CASE(OP_sar):
            {
                JSValue op1, op2;
                op1 = sp[-2];
                op2 = sp[-1];
                if (likely(JS_VALUE_IS_BOTH_INT(op1, op2))) {
                    uint32_t v2;
                    v2 = JS_VALUE_GET_INT(op2);
                    v2 &= 0x1f;
                    sp[-2] = JS_NewInt32(ctx,
                                          (int)JS_VALUE_GET_INT(op1) >> v2);
                    sp--;
                } else {
                    sf->cur_pc = pc;
                    if (js_binary_logic_slow(ctx, sp, opcode))
                        goto exception;
                    sp--;
                }
            }
            BREAK;
        CASE(OP_and):
            {
                JSValue op1, op2;
                op1 = sp[-2];
                op2 = sp[-1];
                if (likely(JS_VALUE_IS_BOTH_INT(op1, op2))) {
                    sp[-2] = JS_NewInt32(ctx,
                                         JS_VALUE_GET_INT(op1) &
                                         JS_VALUE_GET_INT(op2));
                    sp--;
                } else {
                    sf->cur_pc = pc;
                    if (js_binary_logic_slow(ctx, sp, opcode))
                        goto exception;
                    sp--;
                }
            }
            BREAK;
        CASE(OP_or):
            {
                JSValue op1, op2;
                op1 = sp[-2];
                op2 = sp[-1];
                if (likely(JS_VALUE_IS_BOTH_INT(op1, op2))) {
                    sp[-2] = JS_NewInt32(ctx,
                                         JS_VALUE_GET_INT(op1) |
                                         JS_VALUE_GET_INT(op2));
                    sp--;
                } else {
                    sf->cur_pc = pc;
                    if (js_binary_logic_slow(ctx, sp, opcode))
                        goto exception;
                    sp--;
                }
            }
            BREAK;
        CASE(OP_xor):
            {
                JSValue op1, op2;
                op1 = sp[-2];
                op2 = sp[-1];
                if (likely(JS_VALUE_IS_BOTH_INT(op1, op2))) {
                    sp[-2] = JS_NewInt32(ctx,
                                         JS_VALUE_GET_INT(op1) ^
                                         JS_VALUE_GET_INT(op2));
                    sp--;
                } else {
                    sf->cur_pc = pc;
                    if (js_binary_logic_slow(ctx, sp, opcode))
                        goto exception;
                    sp--;
                }
            }
            BREAK;


#define OP_CMP(opcode, binary_op, slow_call)                            \
            CASE(opcode):                                               \
                {                                                       \
                JSValue op1, op2;                                       \
                op1 = sp[-2];                                           \
                op2 = sp[-1];                                           \
                if (likely(JS_VALUE_IS_BOTH_INT(op1, op2))) {           \
                    sp[-2] = JS_NewBool(ctx, JS_VALUE_GET_INT(op1) binary_op JS_VALUE_GET_INT(op2)); \
                    sp--;                                               \
                } else if (JS_TAG_IS_FLOAT64(JS_VALUE_GET_TAG(op1)) ||  \
                           JS_TAG_IS_FLOAT64(JS_VALUE_GET_TAG(op2))) {  \
                    double d1, d2;                                      \
                    if (JS_TAG_IS_FLOAT64(JS_VALUE_GET_TAG(op1))) {     \
                        d1 = JS_VALUE_GET_FLOAT64(op1);                 \
                    } else if (JS_VALUE_GET_TAG(op1) == JS_TAG_INT) {   \
                        d1 = JS_VALUE_GET_INT(op1);                     \
                    } else {                                            \
                        goto opcode ## _slow_case;                      \
                    }                                                   \
                    if (JS_TAG_IS_FLOAT64(JS_VALUE_GET_TAG(op2))) {     \
                        d2 = JS_VALUE_GET_FLOAT64(op2);                 \
                    } else if (JS_VALUE_GET_TAG(op2) == JS_TAG_INT) {   \
                        d2 = JS_VALUE_GET_INT(op2);                     \
                    } else {                                            \
                        goto opcode ## _slow_case;                      \
                    }                                                   \
                    sp[-2] = JS_NewBool(ctx, d1 binary_op d2);          \
                    sp--;                                               \
                } else {                                                \
                opcode ## _slow_case:                                   \
                    sf->cur_pc = pc;                                    \
                    if (slow_call)                                      \
                        goto exception;                                 \
                    sp--;                                               \
                }                                                       \
                }                                                       \
            BREAK

            OP_CMP(OP_lt, <, js_relational_slow(ctx, sp, opcode));
            OP_CMP(OP_lte, <=, js_relational_slow(ctx, sp, opcode));
            OP_CMP(OP_gt, >, js_relational_slow(ctx, sp, opcode));
            OP_CMP(OP_gte, >=, js_relational_slow(ctx, sp, opcode));

#define OP_CMP_EQ(opcode, inv)                                          \
            CASE(opcode):                                               \
                {                                                       \
                JSValue op1, op2;                                       \
                int res;                                                \
                uint32_t tag1, tag2;                                    \
                op1 = sp[-2];                                           \
                op2 = sp[-1];                                           \
                tag1 = JS_VALUE_GET_TAG(op1);                           \
                tag2 = JS_VALUE_GET_TAG(op2);                           \
                if (likely(tag1 == JS_TAG_INT)) {                       \
                    if (tag2 == JS_TAG_INT) {                           \
                        res = JS_VALUE_GET_INT(op1) == JS_VALUE_GET_INT(op2); \
                    } else if (JS_TAG_IS_FLOAT64(tag2)) {               \
                        res = (JS_VALUE_GET_INT(op1) == JS_VALUE_GET_FLOAT64(op2)); \
                    } else {                                            \
                        goto slow_eq ## inv;                            \
                    }                                                   \
                } else if (JS_TAG_IS_FLOAT64(tag1)) {                   \
                    if (tag2 == JS_TAG_INT) {                           \
                        res = JS_VALUE_GET_FLOAT64(op1) == JS_VALUE_GET_INT(op2); \
                    } else if (JS_TAG_IS_FLOAT64(tag2)) {               \
                        res = (JS_VALUE_GET_FLOAT64(op1) == JS_VALUE_GET_FLOAT64(op2)); \
                    } else {                                            \
                        goto slow_eq ## inv;                            \
                    }                                                   \
                } else if (tag1 == JS_TAG_OBJECT) {                     \
                    if (tag2 == JS_TAG_NULL || tag2 == JS_TAG_UNDEFINED) { \
                        JSObject *p = JS_VALUE_GET_OBJ(op1);            \
                        res = p->is_HTMLDDA;                            \
                        JS_FreeValue(ctx, op1);                         \
                    } else if (tag2 == JS_TAG_OBJECT) {                 \
                        res = JS_VALUE_GET_OBJ(op1) == JS_VALUE_GET_OBJ(op2); \
                        JS_FreeValue(ctx, op1);                         \
                        JS_FreeValue(ctx, op2);                         \
                    } else {                                            \
                        goto slow_eq ## inv;                            \
                    }                                                   \
                } else if (tag1 == JS_TAG_NULL || tag1 == JS_TAG_UNDEFINED) { \
                    if (tag2 == JS_TAG_NULL || tag2 == JS_TAG_UNDEFINED) { \
                        res = TRUE;                                     \
                    } else if (tag2 == JS_TAG_OBJECT) {                 \
                        JSObject *p = JS_VALUE_GET_OBJ(op2);            \
                        res = p->is_HTMLDDA;                            \
                        JS_FreeValue(ctx, op2);                         \
                    } else {                                            \
                        goto slow_eq ## inv;                            \
                    }                                                   \
                } else if (tag1 == JS_TAG_STRING && tag2 == JS_TAG_STRING) { \
                    res = js_string_eq(ctx, JS_VALUE_GET_STRING(op1),   \
                                       JS_VALUE_GET_STRING(op2));       \
                    JS_FreeValue(ctx, op1);                             \
                    JS_FreeValue(ctx, op2);                             \
                } else {                                                \
                    slow_eq ## inv:                                     \
                    sf->cur_pc = pc;                                    \
                    if (js_eq_slow(ctx, sp, inv))                       \
                        goto exception;                                 \
                    sp--;                                               \
                    goto slow_eq_done ## inv;                           \
                }                                                       \
                sp[-2] = JS_NewBool(ctx, res ^ inv);                    \
                sp--;                                                   \
                slow_eq_done ## inv: ;                                  \
                }                                                       \
            BREAK

            OP_CMP_EQ(OP_eq, 0);
            OP_CMP_EQ(OP_neq, 1);

#define OP_CMP_STRICT_EQ(opcode, inv)                                   \
            CASE(opcode):                                               \
                {                                                       \
                JSValue op1, op2;                                       \
                int res;                                                \
                uint32_t tag1, tag2;                                    \
                op1 = sp[-2];                                           \
                op2 = sp[-1];                                           \
                tag1 = JS_VALUE_GET_TAG(op1);                           \
                tag2 = JS_VALUE_GET_TAG(op2);                           \
                if (likely(tag1 == JS_TAG_INT)) {                       \
                    if (tag2 == JS_TAG_INT) {                           \
                        res = JS_VALUE_GET_INT(op1) == JS_VALUE_GET_INT(op2); \
                    } else if (JS_TAG_IS_FLOAT64(tag2)) {               \
                        res = (JS_VALUE_GET_INT(op1) == JS_VALUE_GET_FLOAT64(op2)); \
                    } else {                                            \
                        JS_FreeValue(ctx, op2);                         \
                        res = FALSE;                                    \
                    }                                                   \
                } else if (JS_TAG_IS_FLOAT64(tag1)) {                   \
                    if (tag2 == JS_TAG_INT) {                           \
                        res = JS_VALUE_GET_FLOAT64(op1) == JS_VALUE_GET_INT(op2); \
                    } else if (JS_TAG_IS_FLOAT64(tag2)) {               \
                        res = (JS_VALUE_GET_FLOAT64(op1) == JS_VALUE_GET_FLOAT64(op2)); \
                    } else {                                            \
                        JS_FreeValue(ctx, op2);                         \
                        res = FALSE;                                    \
                    }                                                   \
                } else if (tag1 == JS_TAG_OBJECT) {                     \
                    if (tag2 == JS_TAG_OBJECT) {                        \
                        res = JS_VALUE_GET_OBJ(op1) == JS_VALUE_GET_OBJ(op2); \
                    } else {                                            \
                        res = FALSE;                                    \
                    }                                                   \
                    JS_FreeValue(ctx, op1);                             \
                    JS_FreeValue(ctx, op2);                             \
                } else if (tag1 == JS_TAG_NULL || tag1 == JS_TAG_UNDEFINED) { \
                    res = (tag1 == tag2);                               \
                    JS_FreeValue(ctx, op2);                             \
                } else if (tag1 == JS_TAG_STRING && tag2 == JS_TAG_STRING) { \
                    res = js_string_eq(ctx, JS_VALUE_GET_STRING(op1),   \
                                       JS_VALUE_GET_STRING(op2));       \
                    JS_FreeValue(ctx, op1);                             \
                    JS_FreeValue(ctx, op2);                             \
                } else {                                                \
                    res = js_strict_eq2(ctx, op1, op2, JS_EQ_STRICT);   \
                    JS_FreeValue(ctx, op1);                             \
                    JS_FreeValue(ctx, op2);                             \
                }                                                       \
                sp[-2] = JS_NewBool(ctx, res ^ inv);                    \
                sp--;                                                   \
                }                                                       \
            BREAK

            OP_CMP_STRICT_EQ(OP_strict_eq, 0);
            OP_CMP_STRICT_EQ(OP_strict_neq, 1);

        CASE(OP_in):
            sf->cur_pc = pc;
            if (js_operator_in(ctx, sp))
                goto exception;
            sp--;
            BREAK;
        CASE(OP_private_in):
            sf->cur_pc = pc;
            if (js_operator_private_in(ctx, sp))
                goto exception;
            sp--;
            BREAK;
        CASE(OP_instanceof):
            sf->cur_pc = pc;
            if (js_operator_instanceof(ctx, sp))
                goto exception;
            sp--;
            BREAK;
        CASE(OP_typeof):
            {
                JSValue op1;
                JSAtom atom;

                op1 = sp[-1];
                atom = js_operator_typeof(ctx, op1);
                JS_FreeValue(ctx, op1);
                sp[-1] = JS_AtomToString(ctx, atom);
            }
            BREAK;
        CASE(OP_delete):
            sf->cur_pc = pc;
            if (js_operator_delete(ctx, sp))
                goto exception;
            sp--;
            BREAK;
        CASE(OP_delete_var):
            {
                JSAtom atom;
                int ret;

                atom = get_u32(pc);
                pc += 4;
                sf->cur_pc = pc;

                ret = JS_DeleteGlobalVar(ctx, atom);
                if (unlikely(ret < 0))
                    goto exception;
                *sp++ = JS_NewBool(ctx, ret);
            }
            BREAK;

        CASE(OP_to_object):
            if (JS_VALUE_GET_TAG(sp[-1]) != JS_TAG_OBJECT) {
                sf->cur_pc = pc;
                ret_val = JS_ToObject(ctx, sp[-1]);
                if (JS_IsException(ret_val))
                    goto exception;
                JS_FreeValue(ctx, sp[-1]);
                sp[-1] = ret_val;
            }
            BREAK;

        CASE(OP_to_propkey):
            switch (JS_VALUE_GET_TAG(sp[-1])) {
            case JS_TAG_INT:
            case JS_TAG_STRING:
            case JS_TAG_SYMBOL:
                break;
            default:
                sf->cur_pc = pc;
                ret_val = JS_ToPropertyKey(ctx, sp[-1]);
                if (JS_IsException(ret_val))
                    goto exception;
                JS_FreeValue(ctx, sp[-1]);
                sp[-1] = ret_val;
                break;
            }
            BREAK;

#if 0
        CASE(OP_to_string):
            if (JS_VALUE_GET_TAG(sp[-1]) != JS_TAG_STRING) {
                ret_val = JS_ToString(ctx, sp[-1]);
                if (JS_IsException(ret_val))
                    goto exception;
                JS_FreeValue(ctx, sp[-1]);
                sp[-1] = ret_val;
            }
            BREAK;
#endif
        CASE(OP_with_get_var):
        CASE(OP_with_put_var):
        CASE(OP_with_delete_var):
        CASE(OP_with_make_ref):
        CASE(OP_with_get_ref):
            {
                JSAtom atom;
                int32_t diff;
                JSValue obj, val;
                int ret, is_with;
                atom = get_u32(pc);
                diff = get_u32(pc + 4);
                is_with = pc[8];
                pc += 9;
                sf->cur_pc = pc;

                obj = sp[-1];
                ret = JS_HasProperty(ctx, obj, atom);
                if (unlikely(ret < 0))
                    goto exception;
                if (ret) {
                    if (is_with) {
                        ret = js_has_unscopable(ctx, obj, atom);
                        if (unlikely(ret < 0))
                            goto exception;
                        if (ret)
                            goto no_with;
                    }
                    switch (opcode) {
                    case OP_with_get_var:
                        /* in Object Environment Records, GetBindingValue() calls HasProperty() */
                        ret = JS_HasProperty(ctx, obj, atom);
                        if (unlikely(ret <= 0)) {
                            if (ret < 0)
                                goto exception;
                            if (is_strict_mode(ctx)) {
                                JS_ThrowReferenceErrorNotDefined(ctx, atom);
                                goto exception;
                            }
                            val = JS_UNDEFINED;
                        } else {
                            val = JS_GetProperty(ctx, obj, atom);
                            if (unlikely(JS_IsException(val)))
                                goto exception;
                        }
                        set_value(ctx, &sp[-1], val);
                        break;
                    case OP_with_put_var: /* used e.g. in for in/of */
                        /* in Object Environment Records, SetMutableBinding() calls HasProperty() */
                        ret = JS_HasProperty(ctx, obj, atom);
                        if (unlikely(ret <= 0)) {
                            if (ret < 0)
                                goto exception;
                            if (is_strict_mode(ctx)) {
                                JS_ThrowReferenceErrorNotDefined(ctx, atom);
                                goto exception;
                            }
                        }
                        ret = JS_SetPropertyInternal(ctx, obj, atom, sp[-2], obj,
                                                     JS_PROP_THROW_STRICT);
                        JS_FreeValue(ctx, sp[-1]);
                        sp -= 2;
                        if (unlikely(ret < 0))
                            goto exception;
                        break;
                    case OP_with_delete_var:
                        ret = JS_DeleteProperty(ctx, obj, atom, 0);
                        if (unlikely(ret < 0))
                            goto exception;
                        JS_FreeValue(ctx, sp[-1]);
                        sp[-1] = JS_NewBool(ctx, ret);
                        break;
                    case OP_with_make_ref:
                        /* produce a pair object/propname on the stack */
                        *sp++ = JS_AtomToValue(ctx, atom);
                        break;
                    case OP_with_get_ref:
                        /* produce a pair object/method on the stack */
                        /* in Object Environment Records, GetBindingValue() calls HasProperty() */
                        ret = JS_HasProperty(ctx, obj, atom);
                        if (unlikely(ret < 0))
                            goto exception;
                        if (!ret) {
                            val = JS_UNDEFINED;
                        } else {
                            val = JS_GetProperty(ctx, obj, atom);
                            if (unlikely(JS_IsException(val)))
                                goto exception;
                        }
                        *sp++ = val;
                        break;
                    }
                    pc += diff - 5;
                } else {
                no_with:
                    /* if not jumping, drop the object argument */
                    JS_FreeValue(ctx, sp[-1]);
                    sp--;
                }
            }
            BREAK;

        CASE(OP_await):
            ret_val = JS_NewInt32(ctx, FUNC_RET_AWAIT);
            goto done_generator;
        CASE(OP_yield):
            ret_val = JS_NewInt32(ctx, FUNC_RET_YIELD);
            goto done_generator;
        CASE(OP_yield_star):
        CASE(OP_async_yield_star):
            ret_val = JS_NewInt32(ctx, FUNC_RET_YIELD_STAR);
            goto done_generator;
        CASE(OP_return_async):
            ret_val = JS_UNDEFINED;
            goto done_generator;
        CASE(OP_initial_yield):
            ret_val = JS_NewInt32(ctx, FUNC_RET_INITIAL_YIELD);
            goto done_generator;

        CASE(OP_nop):
            BREAK;
        CASE(OP_is_undefined_or_null):
            if (JS_VALUE_GET_TAG(sp[-1]) == JS_TAG_UNDEFINED ||
                JS_VALUE_GET_TAG(sp[-1]) == JS_TAG_NULL) {
                goto set_true;
            } else {
                goto free_and_set_false;
            }
#if SHORT_OPCODES
        CASE(OP_is_undefined):
            if (JS_VALUE_GET_TAG(sp[-1]) == JS_TAG_UNDEFINED) {
                goto set_true;
            } else {
                goto free_and_set_false;
            }
        CASE(OP_is_null):
            if (JS_VALUE_GET_TAG(sp[-1]) == JS_TAG_NULL) {
                goto set_true;
            } else {
                goto free_and_set_false;
            }
            /* XXX: could merge to a single opcode */
        CASE(OP_typeof_is_undefined):
            /* different from OP_is_undefined because of isHTMLDDA */
            if (js_operator_typeof(ctx, sp[-1]) == JS_ATOM_undefined) {
                goto free_and_set_true;
            } else {
                goto free_and_set_false;
            }
        CASE(OP_typeof_is_function):
            if (js_operator_typeof(ctx, sp[-1]) == JS_ATOM_function) {
                goto free_and_set_true;
            } else {
                goto free_and_set_false;
            }
        free_and_set_true:
            JS_FreeValue(ctx, sp[-1]);
#endif
        set_true:
            sp[-1] = JS_TRUE;
            BREAK;
        free_and_set_false:
            JS_FreeValue(ctx, sp[-1]);
            sp[-1] = JS_FALSE;
            BREAK;
        CASE(OP_invalid):
        DEFAULT:
            JS_ThrowInternalError(ctx, "invalid opcode: pc=%u opcode=0x%02x",
                                  (int)(pc - b->byte_code_buf - 1), opcode);
            goto exception;
        }
    }
 exception:
    if (is_backtrace_needed(ctx, rt->current_exception)) {
        /* add the backtrace information now (it is not done
           before if the exception happens in a bytecode
           operation */
        sf->cur_pc = pc;
        build_backtrace(ctx, rt->current_exception, NULL, 0, 0, 0);
    }
    if (!rt->current_exception_is_uncatchable) {
        while (sp > stack_buf) {
            JSValue val = *--sp;
            JS_FreeValue(ctx, val);
            if (JS_VALUE_GET_TAG(val) == JS_TAG_CATCH_OFFSET) {
                int pos = JS_VALUE_GET_INT(val);
                if (pos == 0) {
                    /* enumerator: close it with a throw */
                    JS_FreeValue(ctx, sp[-1]); /* drop the next method */
                    sp--;
                    JS_IteratorClose(ctx, sp[-1], TRUE);
                } else {
                    *sp++ = rt->current_exception;
                    rt->current_exception = JS_UNINITIALIZED;
                    pc = b->byte_code_buf + pos;
                    goto restart;
                }
            }
        }
    }
    ret_val = JS_EXCEPTION;
    /* the local variables are freed by the caller in the generator
       case. Hence the label 'done' should never be reached in a
       generator function. */
    if (b->func_kind != JS_FUNC_NORMAL) {
    done_generator:
        sf->cur_pc = pc;
        sf->cur_sp = sp;
    } else {
    done:
        if (unlikely(b->var_ref_count != 0)) {
            /* variable references reference the stack: must close them */
            close_var_refs(rt, b, sf);
        }
        /* free the local variables and stack */
        for(pval = local_buf; pval < sp; pval++) {
            JS_FreeValue(ctx, *pval);
        }
    }
    rt->current_stack_frame = sf->prev_frame;
    return ret_val;
}

#ifdef OPCODE_ASM_LABEL
#pragma GCC diagnostic pop
#endif

JSValue JS_Call(JSContext *ctx, JSValueConst func_obj, JSValueConst this_obj,
                int argc, JSValueConst *argv)
{
    return JS_CallInternal(ctx, func_obj, this_obj, JS_UNDEFINED,
                           argc, (JSValue *)argv, JS_CALL_FLAG_COPY_ARGV);
}
