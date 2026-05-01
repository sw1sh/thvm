// aot/abi.h -- self-contained header for runtime-loaded AOT dylibs.
//
// AOT dylibs that get dlopen'd at runtime cannot include thvm.h
// (lots of internals, static inline helpers tied to per-context
// state).  This header is the SLIM contract:
//
//   - basic numeric typedefs
//   - Term opaque
//   - TAG_* / OP_* / DT_* constants the emitter references by name
//   - AotRuntimeOps struct (mirrored from runtime_ops.c)
//   - the dylib's required init signature
//
// Both the host and the dylib include this header.  The host
// additionally includes the full thvm.h for its own use; the dylib
// stops here.

#ifndef THVM_AOT_ABI_H
#define THVM_AOT_ABI_H

#include <stdint.h>
#include <stddef.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef u64      Term;

// --- Tags (mirror src/thvm.h) ---------------------------------------
#define TAG_APP   0
#define TAG_LAM   1
#define TAG_VAR   2
#define TAG_ERA   3
#define TAG_DP0   4
#define TAG_DP1   5
#define TAG_SUP   6
#define TAG_DUP   7
#define TAG_TEN   8
#define TAG_UOP   9
#define TAG_NUM  10
#define TAG_REF  11
#define TAG_ALO  12
#define TAG_OP2  13
#define TAG_MAT  14
#define TAG_CTR  20

// --- OP2 opcodes ----------------------------------------------------
#define OP_ADD  0
#define OP_SUB  1
#define OP_MUL  2
#define OP_EQ   3
#define OP_LT   4

// --- dtype ext values (used for TAG_NUM ext field) ------------------
#define DT_INT32  5

// --- AotRuntimeOps: function-pointer ABI between host and dylib -----
//
// The host populates this struct and passes a const pointer to each
// dylib's `aot_dylib_init()` after dlopen.  The dylib stashes the
// pointer in a global and calls helpers via `OPS->fn(...)`.
//
// Stable layout: bump version when fields move or are added (the
// host fails-fast on mismatch).  Today: version 1.

#define AOT_RUNTIME_OPS_VERSION 1

typedef struct AotRuntimeOps {
    u32 version;

    Term (*term_new)(u8 sub, u8 tag, u32 ext, u64 val);
    u8   (*term_tag)(Term t);
    u32  (*term_ext)(Term t);
    u64  (*term_val)(Term t);

    Term (*term_new_ctr)(u32 label, const Term *children, u32 n);
    u32  (*term_ctr_n)(Term ctr);
    Term (*term_ctr_at)(Term ctr, u32 i);

    u64  (*heap_alloc)(u64 n);
    void (*heap_set)(u64 loc, Term t);
    Term (*heap_read)(u64 loc);

    Term (*wnf)(Term t);
    Term (*aot_pop_app_arg)(Term *stack, u32 *sp, u32 base);
    void (*aot_push_app_arg)(Term *stack, u32 *sp, u32 base, Term arg);
    Term (*aot_force)(Term t);
    Term (*aot_fallback)(u32 def_id);
    Term (*aot_new_app)(Term fun, Term arg);
    void (*aot_make_dup)(u32 lab, Term body, Term *dp0, Term *dp1);

    void (*aot_register)(u32 name, void *aot_fn);
    void (*itrs_inc)(u64 n);
} AotRuntimeOps;

// Each AOT dylib MUST export:
//
//   void aot_dylib_init(const AotRuntimeOps *ops, u32 def_slot);
//
// which the host calls right after dlopen.  Implementation:
//   - validate ops->version == AOT_RUNTIME_OPS_VERSION
//   - stash ops in a static global (so emitted code can call OPS->...)
//   - call ops->aot_register(def_slot, /* this dylib's AotFn */)

// --- Compile mode selector ------------------------------------------
// When building an AOT dylib (via clang from build.c), the build
// driver passes -DTHVM_AOT_DYLIB.  In that mode every host-side
// helper name resolves to OPS->fn(...).  When the same emitted
// source is compiled INTO the host (via #include from
// src/aot/programs/*.c), -DTHVM_AOT_DYLIB is NOT defined and the
// host's regular static-inline helpers are used.

#ifdef THVM_AOT_DYLIB
extern const AotRuntimeOps *OPS;

// The host's `fn` macro doesn't exist in the dylib build; define
// it so the emitter's helper-prologue (`fn void aot_program_emit_X_register`)
// stays portable.
#ifndef fn
#define fn static inline
#endif


#define term_new(s, t, e, v)            (OPS->term_new((s), (t), (e), (v)))
#define term_tag(t)                     (OPS->term_tag(t))
#define term_ext(t)                     (OPS->term_ext(t))
#define term_val(t)                     (OPS->term_val(t))
#define term_new_ctr(label, ch, n)      (OPS->term_new_ctr((label), (ch), (n)))
#define term_ctr_n(t)                   (OPS->term_ctr_n(t))
#define term_ctr_at(t, i)               (OPS->term_ctr_at((t), (i)))
#define heap_alloc(n)                   (OPS->heap_alloc(n))
#define heap_set(loc, t)                (OPS->heap_set((loc), (t)))
#define heap_read(loc)                  (OPS->heap_read(loc))
#define wnf(t)                          (OPS->wnf(t))
#define aot_pop_app_arg(s, sp, b)       (OPS->aot_pop_app_arg((s), (sp), (b)))
#define aot_push_app_arg(s, sp, b, a)   (OPS->aot_push_app_arg((s), (sp), (b), (a)))
#define aot_force(t)                    (OPS->aot_force(t))
#define aot_fallback(d)                 (OPS->aot_fallback(d))
#define aot_new_app(f, a)               (OPS->aot_new_app((f), (a)))
#define aot_make_dup(lab, b, dp0, dp1)  (OPS->aot_make_dup((lab), (b), (dp0), (dp1)))
#define aot_register(name, fn)          (OPS->aot_register((name), (fn)))
#define aot_itrs_inc(n)                 (OPS->itrs_inc(n))
#endif // THVM_AOT_DYLIB

#endif // THVM_AOT_ABI_H
