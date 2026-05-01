// aot/runtime_ops.c -- function-pointer ABI between the host runtime
// and AOT dylibs that get loaded at runtime via dlopen.
//
// Why function pointers, not direct linking?  The host runtime is a
// single-TU build with `static inline` everywhere (the `fn` macro).
// A dlopen'd dylib produced by clang at runtime can't link to those
// symbols.  Two ways out:
//
//   (a) Refactor every helper to extern.  ~50 LOC of header churn,
//       but makes every host call site a real function call (loses
//       the inline benefit).
//   (b) Pass a struct of function pointers at dylib init time.  The
//       dylib stashes the pointer in a global, the emitted code
//       calls through it (`OPS->term_new(...)`).  One pointer-chase
//       per call, but the host keeps its inline performance.
//
// We pick (b).  The cost in emitted code is one extra pointer indirect
// per helper call -- negligible compared to heap_alloc / wnf.
//
// Stable ABI: AOT dylibs that captured an old layout would crash if
// the host's struct grew or fields moved.  Pin a version field so
// we can fail-fast at init time if there's a mismatch.
//
// Each AOT dylib MUST export one entry function:
//
//     void aot_dylib_init(const AotRuntimeOps *ops, u32 def_slot);
//
// which the host calls right after dlopen.  Implementation stashes
// `ops` in a static global and registers the AOT function under
// `def_slot`.

#define AOT_RUNTIME_OPS_VERSION 1

typedef struct AotRuntimeOps {
    u32 version;          // = AOT_RUNTIME_OPS_VERSION

    // Term construction / inspection.  Note: term_new takes a u8 sub
    // (matches src/term/new.c); heap_alloc takes a u64 n.
    Term (*term_new)(u8 sub, u8 tag, u32 ext, u64 val);
    u8   (*term_tag)(Term t);
    u32  (*term_ext)(Term t);
    u64  (*term_val)(Term t);

    // CTR helpers (use the runtime-heap accessors; book-side
    // accessors are emit-time-only).
    Term (*term_new_ctr)(u32 label, const Term *children, u32 n);
    u32  (*term_ctr_n)(Term ctr);
    Term (*term_ctr_at)(Term ctr, u32 i);

    // Heap.
    u64  (*heap_alloc)(u64 n);
    void (*heap_set)(u64 loc, Term t);
    Term (*heap_read)(u64 loc);

    // Reduction + AOT primitives.
    Term (*wnf)(Term t);
    Term (*aot_pop_app_arg)(Term *stack, u32 *sp, u32 base);
    void (*aot_push_app_arg)(Term *stack, u32 *sp, u32 base, Term arg);
    Term (*aot_force)(Term t);
    Term (*aot_fallback)(u32 def_id);
    Term (*aot_new_app)(Term fun, Term arg);
    void (*aot_make_dup)(u32 lab, Term body, Term *dp0, Term *dp1);

    // Registration: AOT dylib calls this to install its function
    // pointer into AOT_FNS so wnf's TAG_REF dispatch picks it up.
    // The function-pointer signature is opaque (void *) so the dylib
    // doesn't need to share AotFn typedef with the host.
    void (*aot_register)(u32 name, void *aot_fn);

    // ITRS bump.  Emitted code calls `OPS->itrs_inc(n)` where the
    // interpreter would have done `ITRS += n`.  Routes through the
    // host so multi-context CURRENT_CTX switching works.
    void (*itrs_inc)(u64 n);
} AotRuntimeOps;

// Tiny shim around aot_register that takes a void* instead of AotFn.
// Lets the dylib stay opaque about the AotFn type.
static void aot_runtime_register_void(u32 name, void *aot_fn) {
    aot_register(name, (AotFn)aot_fn);
}

// Tiny shim around ITRS so the dylib doesn't have to import the
// CURRENT_CTX macro chain.
static void aot_runtime_itrs_inc(u64 n) {
    ITRS += n;
}

// Returns a singleton runtime-ops struct populated with the host's
// function pointers.  The struct is `static` in the host TU; the
// dylib gets a const pointer at init time.
fn const AotRuntimeOps *aot_runtime_ops(void) {
    static AotRuntimeOps OPS = {0};
    if (OPS.version == 0) {
        OPS.version = AOT_RUNTIME_OPS_VERSION;
        OPS.term_new          = term_new;
        OPS.term_tag          = term_tag;
        OPS.term_ext          = term_ext;
        OPS.term_val          = term_val;
        OPS.term_new_ctr      = term_new_ctr;
        OPS.term_ctr_n        = term_ctr_n;
        OPS.term_ctr_at       = term_ctr_at;
        OPS.heap_alloc        = heap_alloc;
        OPS.heap_set          = heap_set;
        OPS.heap_read         = heap_read;
        OPS.wnf               = wnf;
        OPS.aot_pop_app_arg   = aot_pop_app_arg;
        OPS.aot_push_app_arg  = aot_push_app_arg;
        OPS.aot_force         = aot_force;
        OPS.aot_fallback      = aot_fallback;
        OPS.aot_new_app       = aot_new_app;
        OPS.aot_make_dup      = aot_make_dup;
        OPS.aot_register      = aot_runtime_register_void;
        OPS.itrs_inc          = aot_runtime_itrs_inc;
    }
    return &OPS;
}
