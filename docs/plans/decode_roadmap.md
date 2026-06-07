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

- The `{S, S}` causal mask + attention (symbolic on BOTH axes).
- Buffers sized at the upper bound (memory ~ `nCtx`, not `S`) unless dynamic alloc.
- Every backend must bind the kvar; the CPU interpreter path (`uop_walk`) has no
  kvar handling yet.

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
  work is two things: (a) **compiled-kernel bound-passing** -- `render_uop`
  already DECLARES the kvar as a kernel arg (`unsigned V_s` on C/CUDA, a
  `constant uint &V_s` buffer arg on Metal), so the cpu-jit / Metal / CUDA
  launchers just need to PASS `kvar_runtime(id)` for each (today a kvar kernel
  either gets it or cpu-jit declines and it falls to the correct interpreter);
  (b) **matmul / GEMM** -- a symbolic-`M` matmul is GEMM-dispatched, so the GEMM
  call must read `M` via `kvar_extent_runtime`, not the static dim.  Target: a
  mini single-head attention over `{S, S}` JITted, correct at two `S`.
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
