# AOT-on-Metal

Emit a Metal compute kernel from a TDef'd body, compile it via
`xcrun -sdk macosx metal/metallib`, and dispatch on the GPU.  Sister
to the CPU AOT path (`docs/aot.md`); chosen by the `Method` option on
`TAOTRun`.

## Quickstart

```wolfram
(* Define a function *)
TInit[];
TDef["add2", TLam[a, TLam[b, TOp2["+", a, b]]]];

(* Run it on the GPU *)
TAOTRun["add2", {TNum[3], TNum[4]}, Method -> "Metal"]    (* -> 7 *)

(* Or single-thread CPU (auto-compiled dlopen path) *)
TAOTRun["add2", {TNum[3], TNum[4]}, Method -> "CPU"]      (* -> 7 *)

(* Or worker-pool parallel CPU *)
TAOTRun["add2", {TNum[3], TNum[4]},
        Method -> {"CPU", "NumThreads" -> 4}]              (* -> 7 *)
```

`Method -> "Metal"` is the default.  See "Backends" below.

## Pipeline (Metal path)

```
TDef body
   |
   v
src/aot/metal_emit.c
   thvm_aot_metal_emit(def_id, name) -> char *MSL_source
   |
   v
xcrun -sdk macosx metal -c src.metal -o air      (cached on disk by hash)
xcrun -sdk macosx metallib air -o metallib       (cached on disk by hash)
   |
   v
[device newLibraryWithURL:metallib]              (cached in-memory PSO)
[device newComputePipelineStateWithFunction:]
   |
   v
encode buffers (heap, args, result, book_next)
dispatchThreads(1, 1, 1)
waitUntilCompleted
   |
   v
result Term (read back via [resultBuf contents])
```

The `device Term *heap` MTLBuffer is **zero-copy** on Apple Silicon
(`newBufferWithBytesNoCopy` over the host's 16KB-aligned `book_heap`,
options `MTLResourceStorageModeShared`).  Args are copied via
`newBufferWithBytes` (small).  Result + `book_next` are tiny shared
buffers read back after the dispatch.

## Backends

| Method spec                              | Backend            | Implementation       |
| ---------------------------------------- | ------------------ | -------------------- |
| `"Metal"`                                | GPU                | this doc             |
| `"CPU"`                                  | single-thread C    | `docs/aot.md`        |
| `{"CPU", "NumThreads" -> n}` (`n>1`)     | C + work-stealing  | `docs/aot.md`        |

Default `Method -> "Metal"`.  Method dispatcher in
`wl/THVMLink/Kernel/AOT.wl` routes to the right path.  Bare `"CPU"`
auto-compiles via `TAOTCompile` if the dylib isn't cached.

## Body-shape support matrix

The Metal emitter understands the same shapes the CPU C-emit does, with
some caveats around heap allocation.

| Tag      | Metal emit | Notes                                           |
| -------- | ---------- | ----------------------------------------------- |
| TLam     | yes        | peeled at top; binders -> `args[K]`             |
| TVar     | yes        | resolves through bind table                     |
| TNum     | yes        | u32 literal                                     |
| TOp2     | yes        | ADD, SUB, MUL, EQ, LT folded recursively        |
| TApp     | yes        | only as call site of REF (cross-def call)       |
| TRef     | yes        | inlined at the call site; recursion bails to NULL |
| TMat     | yes        | App-of-MAT-of-TVar dispatch (numeric-switch)    |
| TCtr     | yes (build)| as body root: alloc + init via `aot_book_alloc` |
| DP0/DP1  | yes        | per-emit memo dedups multi-use binder values    |
| TLam-arm | yes        | inside MAT arm: peels & binds destructured CTR  |
| TUOp     | no         | tensor ops not on the Metal AOT path            |
| TSup     | no         | superpositions need IC duplication semantics    |
| TPri     | no         | side-effect mechanism stays on CPU              |

For unsupported shapes, the emitter sets a failure flag with a
specific reason.  `THVM_AOT_METAL_DUMP=1` env var prints the emitted
MSL to stderr for debugging.

## Heap-allocation gotcha (BOOK_HEAP vs HEAP)

The kernel's `device Term *heap` parameter is the host's `book_heap`
(zero-copy via `newBufferWithBytesNoCopy`).  CTR inputs whose `val`
references the **dynamic** HEAP (e.g., constructed via `term_new_ctr`
or `TCtr[label, c1, ...]`) cannot be destructured by the kernel,
because `heap[scrutinee_val + 1]` indexes book_heap at the wrong
address.

**Workaround**: use `TBookCtr[label, c1, ...]` to allocate input CTRs
in book_heap directly.

```wolfram
(* Won't work end-to-end through MAT-arm destructure: *)
TAOTRun["pair_sum", {TCtr[2, TNum[7], TNum[35]]}, Method -> "Metal"]

(* Does: *)
TAOTRun["pair_sum", {TBookCtr[2, TNum[7], TNum[35]]}, Method -> "Metal"]
```

CTRs constructed BY the kernel live in book_heap automatically -
they're written via `aot_book_alloc`.

## Caching layers

Three caches make repeated calls fast:

1. **In-memory PSO cache** (`AOT_METAL_PSO_HASHES`/`_OBJS` in `_.m`):
   keyed by FNV-1a hash of the emitted MSL source.  Same content -> same
   PSO.  Survives across calls within a session; cleared on
   `metal_shutdown`.

2. **Persistent metallib disk cache** (`/tmp/thvm_aot_metal_<hash>.metallib`):
   `stat()` check before invoking `xcrun metal/metallib`.  Same content
   across sessions -> skip the ~3-second xcrun phase.  Cold cache
   add2-test ~840 ms total; warm ~64 ms (13x).

3. **Cached book_heap MTLBuffer wrapper**: book_heap is a fixed
   pointer; the wrap MTLBuffer is reused across dispatches.  Invalidated
   on shutdown.

## Diagnostics

| Env var                  | Effect                                              |
| ------------------------ | --------------------------------------------------- |
| `THVM_AOT_METAL_DUMP=1`  | Print emitted MSL source to stderr per dispatch     |
| `THVM_THREADS=N`         | Override default `NumThreads` for `Method -> "CPU"` |
| `THVM_NF_PARALLEL_STEP_SESSION=1` | Enable step session at T>1                 |

Failure messages include the def name and a brief reason:
```
thvm aot-metal: emit failed for def_id 4 ("recur"): recursive REF
  (def_id 4 calls itself) -- Metal emit doesn't support recursion
```

## Performance

Hot-path benchmark (M3 Max, after caches warm):
- 100 `add2(7, 11)` Metal dispatches: ~19 ms total
- ~190 us / call

Dominated by Metal kernel launch + readback; the actual fold is a
handful of nanoseconds.  Consider Metal when you have many
independent reductions to dispatch in batches (see "Batch dispatch"
below), not when you're spinning a tight reduce-tiny-thing loop.

CPU baseline for the same OP2(NUM,NUM) fold via `wnf` (no kernel
launch): ~0.26 us / call - ~1000x faster than Metal for tiny defs.

## Batch dispatch

When you have N independent OP2 redexes queued up, fire them in a
SINGLE Metal dispatch via the batch kernel
(`aot_eval_op2_fold_batch`):

```wolfram
(* Build N OP2(NUM,NUM) cells in book_heap (rootLocs each point at one). *)
results = TAOTBatchOp2Fold[rootLocs];   (* one dispatch, N folds *)
```

Bench numbers (M3 Max) for 256 OP2 ADD redexes:
- Sequential: 256 separate dispatches = ~43 ms (~169 us/call)
- Batch:      1 dispatch handles all 256 = ~187 us (~0.7 us/call)
- **Speedup: ~231x**

The batch path is the dominant win for Metal AOT.  Build OP2 cells
in book_heap (so the kernel's `heap` MTLBuffer can deref them) via
the private bridges - see `wl/THVMLink/Tests/aot_metal.wlt`'s
TAOTBatchOp2Fold test for a worked example.

## Source layout

| File                                              | Role                                |
| ------------------------------------------------- | ----------------------------------- |
| `src/aot/metal_emit.c`                            | MSL emitter                         |
| `src/backend/metal/shaders/aot_eval.metal`        | Hand-written MSL primitives         |
| `src/backend/metal/_.m`                           | Host dispatch + PSO cache           |
| `wl/THVMLink/CSource/thvmlink.c` (`thvm_wl_aot_metal_run_n`, `thvm_wl_aot_metal_op2_fold_batch`) | LibraryLink bridges |
| `wl/THVMLink/Kernel/AOT.wl` (`aotMetalRunImpl`, `TAOTBatchOp2Fold`) | WL Method dispatcher + batch surface |
| `tests/test_aot_metal_run.c`                      | C-level e2e tests                   |
| `wl/THVMLink/Tests/aot_metal.wlt`                 | WL e2e regression suite             |
| `src/aot/wnf_metal_shim.h`                        | wnf() macro-shim scaffold           |

## Coverage gaps

- HEAP-allocated CTRs flowing into the kernel still need the
  `TBookCtr` workaround; auto-marshal to `book_heap` is not wired.
- `TUOp` body emit (bridge to the tensor backend) is not yet
  available on the Metal AOT path.
