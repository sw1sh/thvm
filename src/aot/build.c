// aot/build.c -- emit + clang + dlopen + register pipeline.
//
// Today: the LOADER half (no emit/clang yet -- those are ready, but
// the loader is what unblocks runtime AOT).  Caller hands in:
//   - a path to an already-compiled .dylib that exports
//     `void aot_dylib_init(const AotRuntimeOps *, u32 def_slot)`
//   - the def slot the AOT should register against
//
// We dlopen the dylib, dlsym aot_dylib_init, call it with
// aot_runtime_ops() and the slot.  The dylib's init function
// calls back through ops->aot_register, which installs the AOT
// in AOT_FNS[def_slot] so the WNF machine's TAG_REF dispatch
// picks it up.
//
// Future commits add:
//   - thvm_aot_compile_def(def_id): emit -> /tmp/thvm_aot_<hash>.c,
//     invoke clang, return the dylib path.  Pattern mirrors
//     src/backend/cpu/jit.c.
//   - hash-keyed dylib cache (skip the clang invocation if the
//     output already exists on disk).

#include <dlfcn.h>

typedef void (*AotDylibInit)(const AotRuntimeOps *ops, u32 def_slot);

// Slot registry -- keep dlopen handles around so we can dlclose
// when the runtime is reset.  Same shape as src/backend/cpu/jit.c
// (CpuJitSlot) so the caching strategy can later merge.
#define AOT_DYLIB_SLOTS 64

typedef struct {
    void *handle;          // dlopen handle, NULL if free
    u32   def_slot;        // which def we registered under
} AotDylibSlot;

static AotDylibSlot AOT_DYLIBS[AOT_DYLIB_SLOTS] = {0};

// Find a free dylib slot, or NULL if full.
static AotDylibSlot *aot_dylib_alloc_slot(void) {
    for (u32 i = 0; i < AOT_DYLIB_SLOTS; i++) {
        if (AOT_DYLIBS[i].handle == NULL) return &AOT_DYLIBS[i];
    }
    return NULL;
}

// Load + register an AOT dylib.  Returns 1 on success, 0 on failure.
fn int thvm_aot_load_dylib(const char *dylib_path, u32 def_slot) {
    void *h = dlopen(dylib_path, RTLD_NOW | RTLD_LOCAL);
    if (h == NULL) {
        fprintf(stderr, "thvm_aot_load_dylib: dlopen failed: %s\n", dlerror());
        return 0;
    }
    AotDylibInit init = (AotDylibInit)dlsym(h, "aot_dylib_init");
    if (init == NULL) {
        fprintf(stderr, "thvm_aot_load_dylib: dlsym aot_dylib_init failed: %s\n",
                dlerror());
        dlclose(h);
        return 0;
    }
    AotDylibSlot *slot = aot_dylib_alloc_slot();
    if (slot == NULL) {
        fprintf(stderr, "thvm_aot_load_dylib: out of dylib slots\n");
        dlclose(h);
        return 0;
    }
    slot->handle   = h;
    slot->def_slot = def_slot;
    init(aot_runtime_ops(), def_slot);
    return 1;
}

// Close all loaded AOT dylibs (called from thvm_reset).  Doesn't
// touch AOT_FNS; that's cleared separately by aot_reset().
fn void thvm_aot_close_all_dylibs(void) {
    for (u32 i = 0; i < AOT_DYLIB_SLOTS; i++) {
        if (AOT_DYLIBS[i].handle != NULL) {
            dlclose(AOT_DYLIBS[i].handle);
            AOT_DYLIBS[i].handle = NULL;
            AOT_DYLIBS[i].def_slot = 0;
        }
    }
}

// Emit + compile + load.  Returns the dylib path on success, NULL
// otherwise.  Caller owns the dylib path string.
//
// Strategy mirrors src/backend/cpu/jit.c: hash the source string,
// build /tmp/thvm_aot_<hash>.c if not cached, invoke clang, store
// the .dylib at /tmp/thvm_aot_<hash>.dylib.  Reload from cache if
// it already exists.
fn int thvm_aot_compile_and_load(u32 def_id) {
    char *src = thvm_aot_emit_def(def_id);
    if (src == NULL) {
        fprintf(stderr, "thvm_aot_compile_and_load: no def at slot %u\n", def_id);
        return 0;
    }

    // Quick & cheap hash for the cache key.  Robust enough for the
    // small set of programs we'll AOT in practice.
    u64 hash = 1469598103934665603ull;   // FNV-1a offset
    for (const char *p = src; *p; p++) {
        hash = (hash ^ (u64)(u8)*p) * 1099511628211ull;
    }

    char src_path[256], dl_path[256];
    snprintf(src_path, sizeof src_path, "/tmp/thvm_aot_%016llx.c",
             (unsigned long long)hash);
    snprintf(dl_path,  sizeof dl_path,  "/tmp/thvm_aot_%016llx.dylib",
             (unsigned long long)hash);

    // Cache hit?
    if (access(dl_path, F_OK) != 0) {
        // Write source -- prefix the abi.h include so the dylib
        // self-contains.  Caller's emit doesn't include any
        // headers; we add the line here so future emit revisions
        // don't have to know about the build pipeline.
        FILE *f = fopen(src_path, "w");
        if (f == NULL) { free(src); return 0; }
        fprintf(f, "#include \"%s/src/aot/abi.h\"\n\n", AOT_THVM_ROOT);
        fputs(src, f);
        // Append a default aot_dylib_init that finds the
        // aot_program_emit_<id>_register function we just emitted.
        // Since the emitter generates that helper inline, we just
        // call it from within init.
        fprintf(f,
            "\nconst AotRuntimeOps *OPS = NULL;\n"
            "void aot_dylib_init(const AotRuntimeOps *ops, u32 def_slot) {\n"
            "  if (!ops || ops->version != AOT_RUNTIME_OPS_VERSION) return;\n"
            "  OPS = ops;\n"
            "  aot_program_emit_%u_register(def_slot);\n"
            "}\n",
            def_id);
        fclose(f);

        char cmd[1024];
        snprintf(cmd, sizeof cmd,
                 "clang -O2 -fPIC -shared -DTHVM_AOT_DYLIB -o '%s' '%s'",
                 dl_path, src_path);
        if (system(cmd) != 0) {
            fprintf(stderr, "thvm_aot_compile_and_load: clang failed for %s\n",
                    src_path);
            free(src);
            return 0;
        }
    }
    free(src);

    return thvm_aot_load_dylib(dl_path, def_id);
}
