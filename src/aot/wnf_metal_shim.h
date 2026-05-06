// aot/wnf_metal_shim.h -- macro-shim layer for the Phase 7 iter M
// "compile src/wnf/*.c as MSL" experiment.
//
// Mirrors HVM4's CUDA pattern (cuda/hvm.cu lines 20-30):
//
//     #define fn __device__ __forceinline__
//     #define ITRS_INC(name)     do { d_itrs++; } while(0)
//     #define __atomic_fetch_add(ptr, val, order)  atomicAdd(ptr, val)
//
// where the same .c source under clang/wnf/_.c compiles for both CPU
// and GPU because every host primitive is shimmed under MSL.
//
// This header is INCLUDED FIRST in a runtime-emitted .metal source
// (alongside the per-def kernel) before the wnf/redex/* sources get
// transitively #included.  It defines the equivalents Apple's MSL
// supports.
//
// === Status: scaffold only ===
//
// Iter M's full goal -- compile src/wnf/_.c, src/wnf/redex.c,
// src/wnf/nf.c (and their many transitive dependencies) verbatim
// under MSL -- is multi-iter work.  See docs at the bottom for the
// blocker inventory.  This header is the starting macro layer; later
// iters fill in or replace each blocker.
//
// === Macro shim layer (the easy part) ===

#ifndef THVM_AOT_WNF_METAL_SHIM_H
#define THVM_AOT_WNF_METAL_SHIM_H

// fn macro.  CPU side: `static inline`.  GPU/MSL: `inline` (no
// `static` because MSL kernels can't link static helpers across TUs;
// `inline` lets the same symbol be emitted in every shader that
// references it without ODR violations).
#define fn  inline

// Atomic ops.  CPU uses gcc-style __atomic_*; MSL has C11-style
// atomic_*_explicit functions on `device atomic_<T>` types.  Memory
// orders are constants in MSL too; we map relaxed/acquire/release
// directly.
#define __ATOMIC_RELAXED  metal::memory_order_relaxed
#define __ATOMIC_ACQUIRE  metal::memory_order_acquire
#define __ATOMIC_RELEASE  metal::memory_order_release
#define __ATOMIC_ACQ_REL  metal::memory_order_acq_rel
#define __ATOMIC_SEQ_CST  metal::memory_order_seq_cst

#define __atomic_fetch_add(ptr, val, order) \
    metal::atomic_fetch_add_explicit((ptr), (val), (order))
#define __atomic_load_n(ptr, order) \
    metal::atomic_load_explicit((ptr), (order))
#define __atomic_store_n(ptr, val, order) \
    metal::atomic_store_explicit((ptr), (val), (order))
#define __atomic_compare_exchange_n(ptr, expected, desired, weak, succ, fail) \
    (metal::atomic_compare_exchange_weak_explicit( \
        (ptr), (expected), (desired), (succ), (fail)))

// ITRS / hot-counter shim.  The CPU runtime bumps file-static
// counters via macros like `ITRS++`.  On GPU these would contend on
// a single global; mirror HVM4's pattern of per-thread shared-mem
// scratch addressed by [[thread_position_in_threadgroup]].
//
// For the iter M scaffold we just no-op; later iters wire to a
// per-threadgroup scratch slot.
#define ITRS_INC(n)         do { (void)(n); } while (0)
#define HOT_WNF_CALLS_INC() do { } while (0)

// Stubs for runtime-only features that don't exist on GPU.
//   getenv -- always returns NULL (env vars don't exist in shaders)
//   fprintf -- compile error on the CPU side, no-op here
//   malloc/free -- use aot_book_alloc instead (caller must rewrite)
//
// These macros let the source PARSE; the rewritten code paths must
// not call these in practice.  Hitting one at runtime is a bug.
#define getenv(s)              ((const constant char *)0)
#define fprintf(stream, ...)   ((int)0)
#define stderr                 ((const constant void *)0)

// === Blocker inventory (deeper work remaining) ===
//
// 1. RECURSION.  MSL kernels cannot recurse, even via inline.  src/wnf/
//    /_.c's wnf() doesn't actually recurse (it's an iterative stack
//    machine), but interaction rules in src/redex/ may call wnf()
//    from inside their handlers.  Need to refactor or bound.
//
// 2. FUNCTION POINTERS.  MSL allows `[[visible]]` indirect calls but
//    the syntax is restrictive.  src/wnf/pool.c uses WnfFireFn (fn
//    ptr) for the fire callback.  Workable but needs care.
//
// 3. PER-THREAD STATE.  CURRENT_WNF_STATE is `_Thread_local` on CPU.
//    MSL has no thread-local storage; the shader runs many threads in
//    parallel via [[thread_position_in_grid]].  Need to allocate a
//    per-thread WnfThreadState in shared memory or device memory and
//    index by tid.
//
// 4. PTHREAD POOL.  src/wnf/pool.c is pthread-based.  GPU has no
//    pthreads; the kernel launch IS the parallelism.  Replace with
//    "one thread per redex shard" pattern.
//
// 5. MALLOC / FREE.  src/wnf/_.c calls malloc for DFS_STACK,
//    STEP_USE_NEXT, seed buffer.  Replace with bump allocations
//    sized at kernel-launch time + passed in as MTLBuffers.
//
// 6. STRING I/O / DEBUG.  fprintf/getenv stubbed; debug paths
//    (DUMP_TILE_IR, etc.) compile to no-ops.
//
// 7. GLOBALS THROUGH CONTEXT.  CPU runtime accesses HEAP/DEFS/etc.
//    through CURRENT_CTX (a TContext *).  MSL kernels need explicit
//    parameters (no globals).  Either pass TContext as a buffer arg
//    or rewrite each macro to take a context handle.
//
// Each blocker becomes a Phase 7 iter (M.1 .. M.N) with its own
// CHANGELOG entry as the work proceeds.

#endif  // THVM_AOT_WNF_METAL_SHIM_H
