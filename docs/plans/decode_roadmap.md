# Decode roadmap: eliminate the fixed window + incremental KV-cache

GPT-2 generation (Tutorials/Gpt2.md, Examples/gpt2_inference.wls) currently runs a
FIXED-window forward: a `{maxSeq, vocab}` one-hot, padded to `maxSeq`, recomputed
each step as a TJit replay. `maxSeq` is the window you pad to. Two levers remove it:

1. **Symbolic sequence length** -- build the forward over a symbolic seq dim so it
   runs at any length with no padding / no window (`maxSeq` disappears entirely).
2. **Incremental KV-cache** -- O(1)/step decode instead of recomputing the whole
   `{maxSeq, ...}` forward + `{maxSeq, maxSeq}` attention every step.

---

## 1. Symbolic sequence length (drop `maxSeq`)

### State: the symbolic-shape infra is PRESENT but DORMANT (never wired)

Ported from tinygrad, but with **zero producers/binders** -- confirmed by grep:
`kvar_alloc` / `kvar_pack_extent` / `kernel_kvar_bind` have no call sites and
`n_kvar_runtime` is never set > 0. What exists:

- `src/schedule/kvar.c` -- the Variable registry: `kvar_alloc(name, lo, hi)`,
  `kvar_hi(id)` (upper bound for buffer/dispatch sizing), `kvar_pack_extent(id)`
  (pack a var into a RANGE extent via `KVAR_FLAG` bit 31), `kernel_kvar_bind(ke,
  id, value)` (per-dispatch binding) + `kvar_runtime_ids/vals[]` on `KernelEntry`.
- `src/codegen/render_uop.c` -- already branches on `kvar_extent_is_var` when
  emitting a loop bound.
- `src/uop/index_simplify.c` -- the ported tinygrad symbolic INDEX simplifier
  uses a kvar extent's registered upper bound.

So the *machinery* (a dim = a Variable, sized at `hi`, bound at dispatch) is there;
nothing creates, binds, or surfaces one. tinygrad's "we have symbolic shapes" is
true at this layer -- but unwired end to end.

### What it takes (the wiring chain)

1. **Producer + WL surface.** A symbolic-dim constructor (e.g. `TSymbolicDim["s",
   1, nCtx]`) and a tensor / lam whose seq axis carries it. The view dim sizes at
   `kvar_hi`; materialize emits a kvar RANGE for that axis instead of a literal.
   Needs a tensor-axis -> kvar side table (views are integer dims today).
2. **Op coverage** -- a transformer's worth, each threading the kvar extent and
   surviving the index-simplifier: matmul with a symbolic M (`onehot . tokT`,
   `hidden . tokT^T`), reduce / softmax over the symbolic axis, shrink
   (`posTable[[1 ;; S]]`), the `{S, S}` causal mask (symbolic iota + compare),
   broadcast / EXPAND.
3. **Realize.** Schedule the kvar RANGEs; size buffers at `kvar_hi` (worst case);
   the per-realize memory planner must handle symbolic sizes.
4. **Dispatch.** `kernel_kvar_bind` the actual `S` before each fire, on every
   backend (CPU interpreter + JIT, Metal, CUDA already read `KVAR_FLAG`).
5. **JIT.** Rebind the kvar per replay -- analogous to the input_replace port
   (`9bec261e`), but for the dim, not the input buffer.

### Risks / hard parts

- **The inner-symbolic addr layer (M2.5 -- the `{S, S}` blocker). INTERPRETER
  DONE.** An OUTER symbolic dim (`{S, dim}`, all of GPT-2 so far) works because
  the kvar never enters a stride product. An INNER symbolic dim (`{A, S}`, and
  the `{S, S}` attention scores) does: a tensor's outer stride = product of its
  inner extents, and the lift baked that coefficient straight from the raw kvar
  extent -> a kvar-packed stride -> wild addresses (a `{3, S}` reduce segfaulted
  in `uwalk_load_f64`). The rule: split every extent read by intent -- **stride
  / sizing / addr-coefficient -> `kvar_extent_static` (hi); loop bound ->
  `kvar_extent_runtime`**. DONE: `view_create` + `shape_numel` (sizing) and
  `ru_build_addr_with_dims` (the lift's addr stride coefficient,
  `rangeify_unified.c`). Now `{A, S}` AND `{S, S}` (both axes symbolic) REALIZE
  on the interpreter (`tests/test_sym_inner_realize.c`: a `{S,S}` row-reduce ->
  S at two bound S). REMAINING for the JIT path: the ~dozen `render_uop` codegen
  sites (same audit) so symbolic kernels compile rather than fall to the
  interpreter -- folds into the cpu-jit/Metal bound-passing below.
- Buffers sized at the upper bound (memory ~ `nCtx`, not `S`) unless dynamic alloc.
- Every backend must bind the kvar; cpu-jit / Metal / CUDA still pass the bound
  to the compiled loop (the interpreter reads it -- M1).

### Milestone ladder

- **M1** minimal: a symbolic dim realized + executed at a runtime-bound length.
  **DONE** (`tests/test_uop_symbolic_shape.c`): a `{S}` tensor (S a kvar in [1,16],
  buffer sized at the upper bound) summed over the symbolic axis yields S; rebinding
  the SAME dim across realizes works (S=3->3 then S=7->7, the generation pattern).
  Wiring: a per-realize kvar runtime registry (`kvar_set_runtime` / `kvar_runtime` /
  `kvar_extent_runtime` in kvar.c), `shape_numel` sizing symbolic dims at the upper
  bound (`kvar_extent_static`), and `uop_walk` looping at the bound value at its two
  RANGE-extent reads. Identity for literal dims (test_cc 86463/86463 unchanged). The
  rest of the infra (rangeify `ru_new_range`, render_uop var extents) was already
  kvar-ready. Backends still to cover for M2+: cpu-jit + Metal/CUDA pass the bound to
  the compiled loop (uop_walk interpreter only, today).
- **M2**: the compiled backends + matmul.  Op coverage on the interpreter is
  basically FREE -- elementwise + multi-axis reduce over a symbolic dim already
  work (`tests/test_sym_m2.c`), since they inherit the RANGE path.  The real M2
  work is two things: (a) **compiled-kernel bound-passing -- cpu-jit DONE.** The
  CPU JIT now compiles + runs symbolic kernels at the bound: `CpuJitFn` gained a
  `const unsigned *kvar_vals` param, the CPU render emits `unsigned V_<name> =
  kvar_vals[i]` from it (`cg_emit_cpu_kvar_decls`, both C entry points), and the
  dispatch fills it via `cpu_jit_kvar_vals` (`kvar_runtime` in
  `kvar_collect_from_dag` order).  Before, `V_s` was undeclared so a symbolic
  kernel failed to compile and fell to the interpreter; now it JIT-dispatches
  (verified: a `{3,S}` reduce JITs with `V=5`, rebinds to `V=4` on the same
  compiled fn -- the bound is a per-dispatch arg, not baked).  Identity for
  non-symbolic kernels (`test_cc 86463/86463`, jit-forced too).  Metal / CUDA
  already declare the arg; their launchers (`setBytes:` / `cuLaunchKernel`)
  still need the same pass -- pending.
  (b) **matmul / GEMM -- DONE** (`tests/test_sym_matmul.c`): a symbolic-`M`
  matmul is GEMM-dispatched, and `blas_try_gemm` now resolves `gemm.M/N/K/ld`
  via `kvar_extent_runtime` after classify, so `{S,K}.{K,N}` runs on cblas at a
  bound `S` (correct at S=40 and S=96, GEMM confirmed firing; the output buffer
  is sized at the kvar upper bound, GEMM writes the first `S` rows).  Remaining:
  the cpu-jit/Metal/CUDA elementwise/reduce bound-passing (a), and a mini
  single-head attention over `{S, S}` (its `{S,S}` mask is an M3 hard part).
- **Symbolic attention compute VALIDATED** (`tests/test_sym_attn.c`): the three
  building blocks a single-head attention needs all compute correctly on the
  interpreter at a runtime-bound `S` -- (1) a matmul contracting a symbolic dim
  (`{M,S}.{S,N}`, the `scores.V` step), (2) a matmul with a symbolic `{S,S}`
  output (`Q.Kt`, both axes symbolic -> padded row-stride-`hi` layout, read
  strided), and (3) softmax over the symbolic key axis (reduce-max +
  broadcast-expand + exp + reduce-sum + recip, giving `1/S` on uniform rows).
  So the transformer block is symbolic-capable; M3 is now plumbing
  (`{S,S}` causal mask + the WL surface + the JIT path), not unknown compute.
- **WL symbolic-dim surface DONE** (`wl/THVMLink/Tests/symbolic.wlt`): three
  bridges (`thvm_wl_kvar_alloc` / `_kvar_set_runtime` / `_mark_symbolic_axis`)
  + the WL wrappers `TKVarAlloc[lo,hi]`, `TKVarSet[vid,val]`,
  `TSymbolicAxis[t,axis,vid]`.  `TSymbolicAxis` reinterprets a concrete
  `{hi, ..}` tensor's axis as a kvar (no copy -- the buffer is already the
  worst-case size); then ONE materialized graph runs at any length by
  rebinding: a `{S,4}` sum-over-S realizes to 5, 7, 12 with no re-lift between
  calls.  Low-level (`TUOpReduce` etc.); the sugar / `tUopShape` don't yet read
  kvar dims, and `TFromNet` doesn't yet build a symbolic-seq forward (M3).
- **M3**: the GPT-2 forward symbolic end to end -- no `maxSeq`, JIT-captured once,
  replayed at the running length. The example collapses to
  `step = TJit[... TFromNet[net, ids] ...]` over the raw growing ids (the one-hot
  trick also goes away once a symbolic GATHER lands -- see below).

> Related: the one-hot input is itself a workaround for the missing `UOP_GATHER`
> (TEmbedding's usage notes it). A symbolic GATHER over the ids would drop both the
> one-hot AND the padding; symbolic-seq and symbolic-gather are complementary.

---

## 2. Incremental KV-cache (O(1)/step decode)

### Current

Fixed-window recompute: the whole `{maxSeq, vocab}` forward each step (~25 ms/token),
including the full `{maxSeq, maxSeq}` attention -- O(maxSeq) work per token even
though only the last position's logits are used.

### Target

Cache each layer's K, V for past positions; each step computes only the NEW token's
Q/K/V (`{1, dim}`), appends to the cache, and attends to the cached `{t, dim}`. Per
step: O(1) projection + O(t) attention over the cache, vs today's O(maxSeq) full
recompute.

### What it takes

1. Per-layer KV-cache buffers (`{nCtx, dim}` each) updated IN PLACE -- a TSet /
   UOP_ASSIGN writing the new K, V at the current offset (a runtime/symbolic offset).
2. The forward specialized to a single new token: `Q = {1, dim}` attending to the
   cached `K, V = {t, dim}`.
3. The cache append: an in-place ASSIGN at position `t` (a symbolic offset -> wants
   the symbolic-seq infra above, or a fixed `nCtx` slot + a runtime write index).

### Dependency

The GENERAL KV-cache (cache length `t` varies) wants the symbolic-seq work (project
1) -- the cache slice `{t, dim}` is a symbolic shape. A fixed-`nCtx` cache is
possible without it (slot `nCtx`, masked attention) but keeps O(nCtx) attention.
So sequence: **symbolic-seq (M1->M3) first, then KV-cache on top.**

---

## Status

- Fixed-window forward: DONE + clean (TJit input-rebind, no slot/TSet; `maxSeq` only
  in the host one-hot padding). See [[Gpt2.md]].
- Symbolic-seq: **M1 DONE** (a symbolic dim realizes + rebinds on the CPU
  interpreter); M2 (symbolic matmul/reduce + the other backends) and M3 (symbolic
  GPT-2 + JIT kvar-rebind) remain.
- KV-cache: blocked on symbolic-seq for the general form; NOT started.
