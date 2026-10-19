// src/aot/build.c -- compile one def to a dylib and run it.  The emitted
// program (emit.c) follows the embedded runtime source in one TU (translation
// unit) and is built with clang into /tmp/aot_<name>_<hash>.dylib, cached by
// content.  The run entries take the host's TContext, so the dylib works on
// the host's heap, DEFS and callbacks.

#ifndef THVM_AOT_BUILD_INCLUDED
#define THVM_AOT_BUILD_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <dlfcn.h>  // Windows: dlopen family shimmed in util/portable_win.h
#endif

// src/thvm.c with its own includes inlined (build/thvm_runtime_blob.c, made by
// tools/inline_includes.py and tools/embed_blob.py).  A precompiled object
// would not do: the helpers the emitted code calls are static inline.
extern const char     thvm_runtime_src[];
extern const uint64_t thvm_runtime_src_len;

// FNV-1a, 64-bit: the content hash in the cache path.
static u64 aot_fnv1a64(const char *s) {
  u64 h = 0xcbf29ce484222325ULL;
  for (; *s; s++) {
    h ^= (u8)*s;
    h *= 0x100000001b3ULL;
  }
  return h;
}

// The dylib's source: runtime, emitted program, run entries.  Caller frees.
static char *aot_wrap_emit_for_dylib(const char *emit_src,
                                     const char *name) {
  size_t cap = (size_t)thvm_runtime_src_len + strlen(emit_src)
             + strlen(name) * 8 + 8192;
  char *out = (char *)malloc(cap);
  if (!out) return NULL;

  int n = snprintf(out, cap,
    "// auto-wrapped by thvm_aot_compile_to_dylib for \"%s\"\n"
    "//\n"
    "// Shared-heap dylib (Phase 6 iter A + X): the thvm runtime\n"
    "// (src/thvm.c flattened) is INLINED at the top of this file.\n"
    "// Static-inline helpers must live in the same translation\n"
    "// unit to be reachable from the per-program emit, so linking\n"
    "// against a precompiled .o would require an extern-ifying\n"
    "// refactor of `fn = static inline` -- deferred.  At run()\n"
    "// entry the host's CURRENT_CTX is wired in so every helper\n"
    "// operates on the SAME heap, DEFS, ITRS, callback tables as\n"
    "// the host.\n"
    "//\n"
    "// Host calls: aot_program_%s_run(host_ctx, input_term).\n"
    "\n"
    "// === BEGIN inlined thvm runtime ===\n",
    name, name);

  if (n < 0 || (size_t)n >= cap) { free(out); return NULL; }
  size_t pos = (size_t)n;

  memcpy(out + pos, thvm_runtime_src, (size_t)thvm_runtime_src_len);
  pos += (size_t)thvm_runtime_src_len;
  const char *runtime_end = "\n// === END inlined thvm runtime ===\n\n";
  size_t runtime_end_len = strlen(runtime_end);
  memcpy(out + pos, runtime_end, runtime_end_len);
  pos += runtime_end_len;
  out[pos] = '\0';

  size_t emit_len = strlen(emit_src);
  if (pos + emit_len + 1 >= cap) {
    cap = pos + emit_len + 4096;
    char *nb = (char *)realloc(out, cap);
    if (!nb) { free(out); return NULL; }
    out = nb;
  }
  memcpy(out + pos, emit_src, emit_len);
  pos += emit_len;
  out[pos] = '\0';

  // Run entries: the host passes its CURRENT_CTX and up to 4 input Terms.
  int m = snprintf(out + pos, cap - pos,
    "\n"
    "u64 aot_program_%s_run(void *host_ctx, u64 in0, u64 in1, u64 in2, u64 in3) {\n"
    "  // Point this dylib's CURRENT_CTX pointer at the host's\n"
    "  // TContext struct.  All subsequent runtime calls inline'd\n"
    "  // from src/thvm.c read/write through CURRENT_CTX, so they\n"
    "  // operate on the host's HEAP / DEFS / ITRS / callbacks /\n"
    "  // device buffers.  No thvm_init -- that would re-zero the\n"
    "  // host's state.\n"
    "  CURRENT_CTX = (TContext *)host_ctx;\n"
    "  // Bind the calling thread's TLS WNF spine to the shared\n"
    "  // ctx's wnf_state.  Required because the dispatch fast-path\n"
    "  // calls wnf() on non-WHNF inputs (TAG_OP2/TAG_REF/etc.) --\n"
    "  // wnf reads CURRENT_WNF_STATE which is per-thread TLS.  The\n"
    "  // dylib has its own TLS slot, zero-initialised; bind it now.\n"
    "  CURRENT_WNF_STATE = &CURRENT_CTX->wnf_state;\n"
    "  // Populate this dylib's static PRIM_TABLE with the core\n"
    "  // primitives (THVM_PRIM_PRI, etc.).  The host already did\n"
    "  // this during its own thvm_init, but the dylib's #include\n"
    "  // \"src/thvm.c\" gave it a SEPARATE static PRIM_TABLE -- so\n"
    "  // a TPri redex inside the AOT'd code would otherwise miss\n"
    "  // the core PRI prim and refuse to fire.  Call once per run;\n"
    "  // re-registration is idempotent (same C function pointers).\n"
    "  thvm_register_core_prims();\n"
    "  AotProgram p;\n"
    "  aot_program_%s_register(&p);\n"
    "  AotTask root = aot_make_task(FN_%s, AOT_RET_ROOT,\n"
    "                                (Term)in0, (Term)in1, (Term)in2, (Term)in3);\n"
    "  Term r = aot_run_serial(&p, root);\n"
    "  return (u64)r;\n"
    "}\n"
    "\n"
    "// Phase 7 iter J: parallel variant -- runs the AOT'd def via the\n"
    "// work-stealing pool with n_threads workers.  n_threads <= 1\n"
    "// short-circuits to aot_run_serial inside aot_run_parallel.\n"
    "// iter Q wired per-worker CURRENT_WNF_STATE in aot_worker_main\n"
    "// so spawned pthreads no longer SEGV on wnf re-entry.\n"
    "u64 aot_program_%s_run_pooled(void *host_ctx, u32 n_threads,\n"
    "                              u64 in0, u64 in1, u64 in2, u64 in3) {\n"
    "  CURRENT_CTX = (TContext *)host_ctx;\n"
    "  CURRENT_WNF_STATE = &CURRENT_CTX->wnf_state;\n"
    "  thvm_register_core_prims();\n"
    "  AotProgram p;\n"
    "  aot_program_%s_register(&p);\n"
    "  AotTask root = aot_make_task(FN_%s, AOT_RET_ROOT,\n"
    "                                (Term)in0, (Term)in1, (Term)in2, (Term)in3);\n"
    "  Term r = aot_run_parallel(&p, root, n_threads);\n"
    "  return (u64)r;\n"
    "}\n",
    name, name, name,
    name, name, name);

  if (m < 0 || (size_t)m >= cap - pos) { free(out); return NULL; }
  return out;
}

// The host's -D flags, which the module's own runtime copy must share:
// THVM_TRACE changes the shared TContext, and ATP (theorem prover) globals
// exist only under their defines.  Lists every ATP define the Makefile sets.
fn char const *aot_host_defines(void) {
  return ""
#ifdef THVM_TRACE
    "-DTHVM_TRACE "
#endif
#ifdef ATP_AUTO_PREC
    "-DATP_AUTO_PREC "
#endif
#ifdef ATP_CP_DIAG
    "-DATP_CP_DIAG "
#endif
#ifdef ATP_CP_GROUND_JOIN
    "-DATP_CP_GROUND_JOIN "
#endif
#ifdef ATP_FLATTERM_DIFF
    "-DATP_FLATTERM_DIFF "
#endif
#ifdef ATP_FLATTERM_SELFCHECK
    "-DATP_FLATTERM_SELFCHECK "
#endif
#ifdef ATP_FV_INDEX
    "-DATP_FV_INDEX "
#endif
#ifdef ATP_GOAL_HEURISTIC
    "-DATP_GOAL_HEURISTIC "
#endif
#ifdef ATP_MNF
    "-DATP_MNF "
#endif
#ifdef ATP_MNF_DIAG
    "-DATP_MNF_DIAG "
#endif
#ifdef ATP_ORDERED_REWRITE
    "-DATP_ORDERED_REWRITE "
#endif
#ifdef ATP_ORPHAN_KILL
    "-DATP_ORPHAN_KILL "
#endif
#ifdef ATP_RULE_INDEX
    "-DATP_RULE_INDEX "
#endif
#ifdef ATP_VAR_NORM
    "-DATP_VAR_NORM "
#endif
#ifdef THVM_ATPFT_ALLOC
    "-DTHVM_ATPFT_ALLOC "
#endif
#ifdef THVM_ATPFT_CONVERT
    "-DTHVM_ATPFT_CONVERT "
#endif
#ifdef THVM_ATPFT_CPQ
    "-DTHVM_ATPFT_CPQ "
#endif
#ifdef THVM_ATPFT_LPO
    "-DTHVM_ATPFT_LPO "
#endif
#ifdef THVM_ATPFT_MATCH
    "-DTHVM_ATPFT_MATCH "
#endif
#ifdef THVM_ATPFT_NORM
    "-DTHVM_ATPFT_NORM "
#endif
#ifdef THVM_ATPFT_RI
    "-DTHVM_ATPFT_RI "
#endif
#ifdef THVM_ATPFT_RULES
    "-DTHVM_ATPFT_RULES "
#endif
#ifdef THVM_ATPFT_UNIFY
    "-DTHVM_ATPFT_UNIFY "
#endif
#ifdef THVM_ATP_AC
    "-DTHVM_ATP_AC "
#endif
#ifdef THVM_ATP_GUIDE_CHECK
    "-DTHVM_ATP_GUIDE_CHECK "
#endif
#ifdef THVM_ATP_LPO_ORIENT_CACHE
    "-DTHVM_ATP_LPO_ORIENT_CACHE "
#endif
#ifdef THVM_ATP_TEST_CLOCK
    "-DTHVM_ATP_TEST_CLOCK "
#endif
    "";
}

// Link flags for a standalone executable that includes the runtime source.
fn char const *aot_exe_link_flags(void) {
#ifdef __APPLE__
  return "-framework Accelerate";
#else
  return "-lm";
#endif
}

// The dylib's path (caller frees), or NULL with clang's errors on stderr.  A
// file already at the content-hashed path is reused.
fn char *thvm_aot_compile_to_dylib(u32 def_id, const char *name) {
  if (def_id >= DEFS_CAP || name == NULL) return NULL;
  if (DEFS[def_id] == 0) return NULL;

  char *src = thvm_aot_emit_program(def_id, name);
  if (!src) return NULL;

  char *wrapped = aot_wrap_emit_for_dylib(src, name);
  free(src);
  if (!wrapped) return NULL;

  // The host defines change TContext and the ATP globals but not the source
  // text, so they salt the hash: a module built for another layout would
  // corrupt the heap.
  u64 hash = aot_fnv1a64(wrapped) ^ aot_fnv1a64(aot_host_defines());
#ifdef THVM_TRACE
  // "M1" (wire provenance) versions the trace layout; bump it when the
  // THVM_TRACE part of TContext changes.
  hash ^= 0x5452414345ULL ^ 0x4D31ULL;  /* "TRACE" ^ "M1" */
#endif

  char src_path[512], dyl_path[512];
  snprintf(src_path, sizeof src_path,
           "/tmp/aot_%s_%016llx.c", name, (unsigned long long)hash);
  snprintf(dyl_path, sizeof dyl_path,
           "/tmp/aot_%s_%016llx.dylib", name, (unsigned long long)hash);

  struct stat st;
  if (stat(dyl_path, &st) == 0) {
    free(wrapped);
    return strdup(dyl_path);
  }

  FILE *f = fopen(src_path, "w");
  if (!f) { free(wrapped); return NULL; }
  fputs(wrapped, f);
  fclose(f);
  free(wrapped);

  // -DTHVM_HAS_WL_BRIDGE drops prims_core.c's weak no-op
  // thvm_pri_wl_invoke_returning, so the symbol binds at dlopen to the host's
  // (THVMLink.dylib, or a test binary) and a PRI reaches its Wolfram callbacks.
#ifdef __APPLE__
  const char *dylib_flags = "-dynamiclib -framework Accelerate "
                             "-undefined dynamic_lookup";
#else
  const char *dylib_flags = "-shared "
                             "-Wl,--unresolved-symbols=ignore-in-object-files";
#endif

  char cmd[2048];
  snprintf(cmd, sizeof cmd,
    "clang -O2 -fPIC -std=c11 -Wno-everything "
    "-DACCELERATE_NEW_LAPACK -DTHVM_HAS_WL_BRIDGE %s%s "
    "-o '%s' '%s' 2>&1",
    aot_host_defines(), dylib_flags, dyl_path, src_path);

  FILE *p = popen(cmd, "r");
  if (!p) return NULL;
  char line[1024];
  int saw_error = 0;
  while (fgets(line, sizeof line, p) != NULL) {
    if (strstr(line, "error:") != NULL) {
      fprintf(stderr, "thvm_aot_compile_to_dylib: clang: %s", line);
      saw_error = 1;
    }
  }
  int rc = pclose(p);
  if (rc != 0 || saw_error) {
    fprintf(stderr,
      "thvm_aot_compile_to_dylib: clang failed (rc=%d) for %s\n",
      rc, src_path);
    return NULL;
  }

  return strdup(dyl_path);
}

// Calls the dylib's aot_program_<name>_run(CURRENT_CTX, in0..in3) (unused
// inputs 0) and returns the result Term, or 0 on failure.  The handle is never
// closed: dlopen of the same path returns the same refcounted handle.
fn u64 thvm_aot_load_and_run4(const char *dylib_path, const char *name,
                               u64 in0, u64 in1, u64 in2, u64 in3) {
  if (!dylib_path || !name) return 0;

  void *h = dlopen(dylib_path, RTLD_NOW | RTLD_LOCAL);
  if (!h) {
    fprintf(stderr, "thvm_aot_load_and_run: dlopen %s failed: %s\n",
            dylib_path, dlerror());
    return 0;
  }

  char sym[256];
  snprintf(sym, sizeof sym, "aot_program_%s_run", name);
  typedef u64 (*RunFn)(void *, u64, u64, u64, u64);
  RunFn run = (RunFn)dlsym(h, sym);
  if (!run) {
    fprintf(stderr, "thvm_aot_load_and_run: dlsym %s failed: %s\n",
            sym, dlerror());
    dlclose(h);
    return 0;
  }

  return run((void *)CURRENT_CTX, in0, in1, in2, in3);
}

// One input (C tests, the Wolfram bridge's integer entry).
fn u64 thvm_aot_load_and_run(const char *dylib_path, const char *name,
                              u64 input) {
  return thvm_aot_load_and_run4(dylib_path, name, input, 0, 0, 0);
}

// Likewise through aot_program_<name>_run_pooled, on n_threads workers
// (serial when n_threads <= 1).
fn u64 thvm_aot_load_and_run4_pooled(const char *dylib_path,
                                      const char *name,
                                      u32 n_threads,
                                      u64 in0, u64 in1, u64 in2, u64 in3) {
  if (!dylib_path || !name) return 0;
  void *h = dlopen(dylib_path, RTLD_NOW | RTLD_LOCAL);
  if (!h) {
    fprintf(stderr, "thvm_aot_load_and_run_pooled: dlopen %s failed: %s\n",
            dylib_path, dlerror());
    return 0;
  }
  char sym[256];
  snprintf(sym, sizeof sym, "aot_program_%s_run_pooled", name);
  typedef u64 (*RunPooledFn)(void *, u32, u64, u64, u64, u64);
  RunPooledFn run = (RunPooledFn)dlsym(h, sym);
  if (!run) {
    fprintf(stderr, "thvm_aot_load_and_run_pooled: dlsym %s failed: %s\n",
            sym, dlerror());
    dlclose(h);
    return 0;
  }
  return run((void *)CURRENT_CTX, n_threads, in0, in1, in2, in3);
}

#endif  // THVM_AOT_BUILD_INCLUDED
