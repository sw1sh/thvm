# Glossary

Words that appear across [tensors.md](tensors.md), the future
fusion / scheduling / codegen docs, and PLAN.md steps 12–15. Pinned
down here so the rest of the docs can use them without parenthetical
re-definitions every time.

For the IC-side terminology (`term`, `agent`, `port`, `wire`, ...)
see [term.md](term.md).

## Tensor and storage

| word | meaning |
|-----|-----|
| **tensor** | Logical n-dimensional array. In our runtime, a `TAG_TEN` term whose `VAL` indexes into `TENS[]` for a `TenDesc`. |
| **TenDesc** | Side-table entry describing one tensor: dtype, refcount, ShapeTracker, backing buffer id, host-side cache pointer, backend pointer. |
| **buffer** | A region of device memory holding raw bytes. Owned by a `Backend`; addressed by `buf_id`. Multiple `TenDesc`s can share one buffer (view aliasing). |
| **buf_id** | Backend-specific opaque handle for a buffer. CPU = index into a malloc table. Metal = `id<MTLBuffer>` slot. |
| **dtype** | Data type tag (`f32`, `i32`, `f16`, ...). Stored in `TenDesc.dtype` and in the `EXT` field of `TAG_TEN`. |
| **shape** | Tuple of dimension sizes. Lives inside `View.shape` (not on `TenDesc` directly). |
| **stride** | Per-axis step (in elements) used to compute a buffer offset from a logical index. `0` = broadcast; negative = flip. |
| **offset** | Starting element offset into the buffer. Lets multiple views slice into the same buffer. |
| **contiguous** | A view whose stride pattern matches the row-major layout of `shape` with no offset. Most kernels are faster on contiguous inputs. |
| **View** | One layer of (`shape`, `strides`, `offset`, optional mask). Movement ops modify a View; if they can't, a new View is pushed onto the ShapeTracker. |
| **ShapeTracker** | Stack of up to `ST_MAX_VIEWS` Views, oldest at the bottom. Indexing walks the stack innermost-first to derive a buffer offset. |
| **broadcast** | Simulating a larger shape by reusing values along axes that have stride `0`. Cheap; no copy. |
| **mask** | Per-axis `[begin, end)` validity range; positions outside read as 0. Used by `pad`, `shrink`, conv-backward edge handling. |
| **refcount** | `TenDesc.refcount`. Bumped by DUP rules on `TAG_TEN`, decremented by ERA. Backend buffers also have their own refcount tracked via `Backend.buf_incref` / `buf_decref`. |

## UOps and the lazy graph

| word | meaning |
|-----|-----|
| **UOp** | A single node in a computational graph. In our runtime, a `TAG_UOP` term whose `EXT` carries the opcode and `VAL` points to its operand cells in the heap. |
| **opcode** | The integer in a UOp's `EXT` field that picks which kind it is (`UOP_ADD`, `UOP_RESHAPE`, `UOP_KERNEL`, ...). |
| **arity** | Number of source operands a UOp consumes. Different per opcode; recorded in a `uop_arity[]` table for the graph walker. |
| **leaf** | A UOp with no compute sources of its own — `UOP_CONST`, or a `TAG_TEN` reference. The bottom of an AST. |
| **AST** | The compute subgraph rooted at a UOp; specifically, the subtree wrapped by a `UOP_KERNEL` and walked by the dispatcher. |
| **lazy** | The default state of a UOp graph: built but not executed. Stays lazy until a `UOP_REALIZE` is reduced. |

## The compile-and-execute pipeline

The full lifecycle of a UOp, in the order a single `TWnf[realize]`
call walks through it. Each row is one verb you'll see in commits,
file paths, and other docs.

| stage | what it does | where it lives |
|-----|-----|-----|
| **build** | User constructs a UOp graph through WL surface (`TUOpAdd`, `TUOpReshape`, ...). No reduction yet. | wl/THVMLink/Kernel/Tensor.wl |
| **materialize** | Rewriting a raw UOp graph into a scheduled DAG of `UOP_KERNEL` nodes. Runs schedule + kernelize + compile in one rewrite. **Fires no kernels.** Reachable two ways: as a rule on `UOP_MATERIALIZE` under `TWnf`, or directly via the `TMaterialize` WL helper (which calls the rewrite without invoking `wnf`). | src/interact/uop_materialize.c |
| **schedule** | Decide which UOps become kernels and in what order. v1 = trivial (one kernel per materialize). Part of materialize. | src/schedule/schedule.c |
| **kernelize** | Rewrite the raw UOp graph into `UOP_KERNEL[output_buf, ast_root]` (and later `UOP_ASSIGN` / `UOP_SINK`) nodes. Part of materialize. | src/schedule/kernelize.c |
| **fusion** | Decide which compute ops run in one kernel without intermediate buffers. v1 = elementwise chains until a shape-changing op; step 14 = full producer-consumer fusion. Part of **schedule**. | src/schedule/schedule.c |
| **memory planning** | Assign physical buffers / reusable temporary slots to a kernel's intermediates. v1 = each `KProgOp` gets its own temp (no reuse); step 14 rewrites `program[]` with reused slots. | src/schedule/plan.c (step 14) |
| **linearization** | Flatten a kernel's AST into an SSA-over-indices list (`KernelEntry.program[]`). Each entry is `{opcode, dtype, src_indices, arg}` referencing earlier positions. Same representation tinygrad pickles to the PYTHON device. | src/schedule/linearize.c |
| **lowering** | Take a linearized kernel and turn it into a backend-specific IR (e.g. SSA over indices with register assignment). v1 = skipped (interpreter reads `program[]` directly). | src/lower/ (step 14) |
| **codegen** | Emit source code (C, Metal Shading Language) from the lowered IR. v1 = skipped; the CPU backend *is* the interpreter, analogous to tinygrad's PYTHON device. | src/codegen/ (step 14) |
| **render** | Synonym for codegen when the output is a string of source. (Tinygrad uses "render" specifically for source emission.) | src/codegen/ (step 14) |
| **compile** | Turn a linearized kernel into something `dispatch_fn` can invoke. v1 auto path = nop (store `program[]` + `dispatch_fn = cpu_interpret`). v1 custom path (`TCompileKernel`) = `cc -shared` + `dlsym`. Step 14 adds the codegen auto path. | src/schedule/compile.c |
| **kernel cache** | Content-addressed store keyed by the structural signature of a kernel AST (or by user-provided id for custom kernels). Compile once, reuse forever. | src/schedule/kernel_cache.c (step 14) |
| **dispatch** | Backend invocation of a compiled kernel with concrete input + output buffers. Triggered by `interact_kernel` once all of a `UOP_KERNEL`'s AST leaves are `TAG_TEN`. | backend->dispatch_kernel |
| **launch** | Synonym for dispatch when emphasising the GPU side. |  |
| **firing** | A `UOP_KERNEL` whose AST leaves are all `TAG_TEN` runs `interact_kernel`, which calls dispatch and returns its output `TAG_TEN`. The IC-level "interaction" event. Happens naturally under `TWnf`. | src/interact/uop_kernel.c |

The v1 (step 12) implementation does **build → realize → schedule
(trivial) → kernelize (one kernel per realize) → dispatch
(interpreter)**. Everything in between (fusion, memory planning,
linearization, lowering, codegen, render, compile, kernel cache)
arrives in step 14, by which point the upstream stages will have
been made richer to feed them.

## Backend and device

| word | meaning |
|-----|-----|
| **Backend** | A vtable of buffer + dispatch hooks. CPU and Metal each have one. Tensors carry a `Backend *`. |
| **device** | The hardware target a backend talks to: a CPU, a GPU, an accelerator. Sometimes used loosely as a synonym for Backend. |
| **device id** | Disambiguates multiple devices of the same kind (`cpu:0`, `metal:0`, `metal:1`). Encoded in `UOP_DEVICE`. |
| **dispatch_kernel** | Backend hook: given a compiled kernel id and a tuple of input buffer ids + an output buffer id, run the kernel. |
| **MPS** | Metal Performance Shaders — Apple's pre-baked kernel library. The Metal backend can route some ops (matmul, conv) through MPS instead of custom MSL. |

## Autograd

| word | meaning |
|-----|-----|
| **forward** | The original UOp graph the user built. |
| **backward** | The graph that computes gradients with respect to leaves marked `requires_grad`. We build this lazily by rewriting `UOP_GRAD` rather than constructing a separate IR. |
| **VJP** | Vector-Jacobian product: reverse-mode gradient of a function at a point, against a cotangent vector. What `UOP_GRAD` computes. |
| **JVP** | Jacobian-vector product: forward-mode gradient. Tinygrad's `UOP_GRAD_FWD` / TinyHVM's equivalent; not in our scope yet. |
| **tape** | An eager autograd implementation records every forward op into a list (the tape) and replays it backwards. *We don't have one*; `UOP_GRAD` is a rewrite rule, not a tape. |
| **GRAD_PIN** | A handle that keeps a forward-pass tensor alive long enough to be used by backward, even if the forward path otherwise erased it. TinyHVM's term for the same thing. |

## Multi-output and writes

| word | meaning |
|-----|-----|
| **output buffer** | The destination tensor a kernel writes into. Held in `Heap[loc]` of a `UOP_KERNEL`. |
| **input buffer** | A tensor a kernel reads from. Discovered by walking the kernel's AST for `TAG_TEN` (or in step 14, `UOP_BUFFER` / `UOP_LOAD`) leaves. |
| **assign** | Pin a kernel's result to a specific buffer. `UOP_ASSIGN[target, src_kernel]`. |
| **sink** | A root that aggregates multiple `UOP_ASSIGN` nodes for graphs that produce more than one output. `UOP_SINK[a0, a1, ...]`. |
| **in-place** | A kernel whose output buffer is an existing live tensor (rather than a fresh one). Common for optimizer steps and accumulation. |
| **epoch** | A monotonic counter on a buffer, bumped on every write. Lets the kernel cache invalidate stale results without recomputing the kernel signature. (TinyHVM concept; we don't need it in step 12.) |

## Symbolic shapes (deferred, step 14+)

| word | meaning |
|-----|-----|
| **symbolic shape** | A shape whose dimensions can be variables (e.g. `batch_size`) instead of constants. Lets one compiled kernel serve a range of shapes. |
| **bind** | Substitute a symbolic variable with a concrete value at dispatch time. |
| **simplify** | Constant-fold + range-bound a symbolic expression so kernel-cache lookups don't see equivalent forms as distinct. |

## Kernel cache vocabulary (step 14+)

| word | meaning |
|-----|-----|
| **structural signature** | Hash of a kernel's AST shape, dtypes, and reduction axes — independent of the specific buffers it operates on. Two kernels with the same signature share a compiled binary. |
| **CAM** | Content-addressed memory; here, the kernel cache keyed by structural signature. |
| **JIT** | Just-in-time compilation. The first dispatch of a kernel signature compiles it; subsequent dispatches reuse the binary. |

## Visualization

| word | meaning |
|-----|-----|
| **THeapGraph** | The full heap-graph rendering ([heap_graph.md](heap_graph.md)). Shows every cell. |
| **THeapDiagram** | The IC string-diagram rendering ([diagrams.md](diagrams.md)). Shows agents and wires only. |
| **AST overlay** | (Future) A rendering mode that highlights the AST inside a `UOP_KERNEL` as a sub-region. |
