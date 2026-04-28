# Grad: dup-like VJP with gy threading

This doc describes the current automatic-differentiation design in
thvm: what `TGrad` builds, how it reduces, and how the runtime
materializes it.  Supersedes the older [`grad-roadmap.md`](grad-roadmap.md),
which described an earlier (target-in-cell, JVP-equivalent) model
that has since been replaced.

The pieces:

1. **Cell layout** — what a grad cell actually is in heap memory
2. **Chain rule** — per-operator adjoints emitted by `interact_grad`
3. **Leaf rule + DUP routing** — how multiple-target gradients are
   extracted by the WL surface
4. **Higher-order** — composing TGrad with itself
5. **Materialization** — how the chain-rule output gets compiled
   into kernels
6. **Profiling** — using `TProfile` to spot allocation leaks

---

## 1. Cell layout

A grad cell is a regular dup-style heap cell with two slots:

```
heap[loc + 0] = y    (the value being differentiated)
heap[loc + 1] = gy   (the cotangent at y's shape)
```

Two HVM4-style dup projections share this cell:

| Term                            | Role                                                                                          |
|---------------------------------|-----------------------------------------------------------------------------------------------|
| `TAG_DP0` + `DUP_GRAD_FLAG`     | **FWD** (forward projection): on force, returns `cell[0] = y`                                 |
| `TAG_DP1` + `DUP_GRAD_FLAG`     | **BWD** (backward projection): on force, fires the chain rule via `interact_grad`             |

Constructors live in [`src/uop/grad.c`](../src/uop/grad.c):
`uop_grad_cell(y, gy)` allocates the 2 slots; `uop_grad(y, gy)`
returns the BWD term; `uop_fwd(y, gy)` returns the FWD term.

The `DUP_GRAD_FLAG` bit on the projection's ext field tells
`wnf` (in [`src/wnf/_.c`](../src/wnf/_.c)) and `redex_fire`
(in [`src/wnf/redex.c`](../src/wnf/redex.c)) to dispatch the
grad-flavored interaction instead of a regular DUP/SUP rule.

The wnf dispatch is HVM4-style stack-based: the `enter` phase
pushes the BWD frame and descends into `cell[0]`; the `apply` phase
pops the frame, sees the resolved `y` in head form, and calls
`interact_grad` (commit `d59b862`).  This avoids re-entrant
`wnf()` calls and lets nested DP structures unfold cleanly.

---

## 2. Chain rule

`interact_grad` (in [`src/interact/uop_grad.c`](../src/interact/uop_grad.c))
walks `y`'s outermost UOp, computes the per-child cotangent
`gy_for_a_i`, and emits a sub-cell `[a_i, gy_for_a_i]` plus an
outer combiner.  The adjoint table mirrors tinygrad / JAX:

| Op                | `gy_for_a` adjoint                                                  |
|-------------------|---------------------------------------------------------------------|
| `ADD(a, b)`       | `gy` (passthrough, both children)                                   |
| `MUL(a, b)`       | `gy_for_a = MUL(b, gy)`; `gy_for_b = MUL(a, gy)`                    |
| `NEG(a)`          | `NEG(gy)`                                                           |
| `RECIP(a)`        | `MUL(gy, NEG(MUL(RECIP(a), RECIP(a))))`                             |
| `EXP2(a)`         | `MUL(gy, MUL(EXP2(a), CONST(ln 2)))`                                |
| `LOG2(a)`         | `MUL(gy, MUL(RECIP(a), CONST(1/ln 2)))`                             |
| `SQRT(a)`         | `MUL(gy, MUL(CONST(0.5), RECIP(SQRT(a))))`                          |
| `REDUCE_SUM(a, x)`| `EXPAND(reshape_keepdim(gy, x), a.shape)`                           |
| `REDUCE_MAX(a, x)`| `MUL(mask, EXPAND(reshape_keepdim(gy, x), a.shape))`                |
| `EXPAND(a, S)`    | `REDUCE_SUM` along expanded axes (handles rank-increase via leading-1 padding) |
| `RESHAPE(a, S)`   | `RESHAPE(gy, a.shape)`                                              |
| `PERMUTE(a, p)`   | `PERMUTE(gy, inv_perm)`                                             |
| `FLIP(a, m)`      | `FLIP(gy, m)`                                                       |
| `PAD(a, b/e)`     | `SHRINK(gy, [b_i, b_i + a.dim_i])`                                  |
| `SHRINK(a, b/e)`  | `PAD(gy, [b_i, src.dim_i - e_i])`                                   |
| `CMPLT/CMPEQ`     | non-differentiable; emits zero at op shape                          |
| `CONST/LOAD`      | leaf cotangent dies                                                 |

The outer combiner for binary ops is `UOP_ADD(BWD_a, BWD_b)`
(reverse-mode summation of contributions); for unary/movement ops
it's just `BWD_a`.

### TEN-leaf short-circuit (commit `dbc73f3`)

When a child `a` is a `TAG_TEN` (atomic tensor handle), the chain
rule skips the cell allocation entirely and emits the leaf SUP
directly via `grad_leaf_sup(ten, gy_for_leaf)`.  For deeply nested
y this saves O(leaves) heap cells per chain-rule round.  Non-TEN
children still go through `grad_cell_alloc + grad_bwd_of` so the
chain rule recurses normally.

### CONST cache (commit `caccb18`)

`uop_const(dtype, bits)` (in [`src/uop/const.c`](../src/uop/const.c))
deduplicates by `(dtype, bits)` via a 16K open-addressed hash
table.  CONSTs are atomic; sharing the heap loc across references
is always safe; materialize dedups by heap loc identity.

---

## 3. Leaf rule + DUP routing

At a `TAG_TEN` leaf with tid `t`, the chain rule emits:

```
SUP^t (zero, gy_for_leaf)
where zero = CONST(0)             -- shape {1}, broadcasts via cpu_op_add/mul
      gy_for_leaf is at leaf.shape
```

The `CONST(0)` mismatch slot is a **scalar** zero, not a leaf-shape
zero.  This is critical: `cpu_op_add` / `cpu_op_mul` broadcast
`numel == 1` operands cleanly, so when the outer combiner sees
`ADD(matched_at_leaf_shape_X, CONST(0))`, the result is just
`matched_at_leaf_shape_X` regardless of `X`.  The earlier
implementation emitted zero at leaf shape, which created
shape-inconsistent ADDs across sibling subtrees with different
leaf shapes -- the bug fixed by `eac4e17`.

The WL surface `TGrad[y, target]` (in
[`wl/THVMLink/Kernel/Tensor.wl`](../wl/THVMLink/Kernel/Tensor.wl))
extracts `target`'s gradient by:

1. Walking `y`'s leaves once at construction time (`gradLeafTids`)
   to collect the distinct tids.
2. Folding a `TDup` per tid around the inner `TUOpGrad[y, gy]`,
   using `matchProj = (a, b) -> b` for the target's tid (extract
   gy from its leaf SUP) and `mismatchProj = (a, b) -> a` for all
   others (extract the scalar zero, broadcast away).
3. Wrapping the result in `TUOpAdd[dupNest, EXPAND(CONST(0), target.shape)]`
   so when target doesn't appear in y at all (or the op is
   non-differentiable), the scalar-zero result lands at
   target.shape.

---

## 4. Higher-order

`TGrad` returns a **symbolic** UOp graph -- it's just a term, not
a materialized tensor.  Apply `TGrad` to it again to get the
Hessian (or higher derivative).  The chain rule walks the
inner-`TGrad`'s output as if it were any other UOp graph.

The wnf alignment in commit `d59b862` was the key fix: the apply
phase for grad-DP1 sees the resolved `cell[0]` regardless of how
deeply DPs nest from prior rounds.  Combined with the dynamic
KernelEntry arrays (commit `1b0c0a2` -- previously kernels capped
at 64 inputs / 256 ops, now grow geometrically), 4th-, 5th-,
6th-order derivatives all work.

Example: Newton's method using TGrad twice (in
[`wl/Examples/newton-1d/newton.wls`](../wl/Examples/newton-1d/newton.wls)):

```wolfram
g = TGrad[f, x];        (* gradient *)
h = TGrad[g, x];        (* Hessian = gradient of gradient *)
xNew = x - g / h;       (* Newton step *)
```

Convergence is quadratic — five iterations to f32 precision for
`f(x) = 0.5*(x-3)^2 + 0.25*x^4`.

---

## 5. Materialization

`TRealize` calls `thvm_realize` (in
[`src/schedule/realize.c`](../src/schedule/realize.c)) which loops
`nf(wnf(res))` then `thvm_materialize` until no fresh kernels
emit.  `thvm_materialize` (in
[`src/schedule/materialize.c`](../src/schedule/materialize.c))
walks the term, classifies realize boundaries (each compute UOP
becomes a candidate), and emits one `KernelEntry` per boundary
via `visit()`.

### Dynamic KernelEntry arrays (commit `1b0c0a2`)

Originally `KernelEntry::input_*` and `program` were inline
arrays sized `[KERNEL_MAX_INPUT=64]` and `[KPROG_MAX_OPS=256]`.
For higher-order grad chains they overflowed and `visit()`
bailed.

Now both are heap-grown pointers with `inputs_cap` / `ops_cap`
fields.  Helpers in [`src/schedule/kernel_alloc.c`](../src/schedule/kernel_alloc.c):

  - `kernel_inputs_reserve(ke, needed)` — `realloc` to
    geometric capacity (init=8, doubles).
  - `kernel_program_reserve(ke, needed)` — same for the program.
  - `kernel_free_arrays(ke)` — release on dealloc.

`KERNEL_MAX_INPUT` / `KPROG_MAX_OPS` became 1M sanity bounds
(only trip on runaway allocation).  The static stack arrays in
`cpu/interpret.c`, `metal/_.m`, and `interact/uop_kernel.c` had to
become VLAs sized by `ke->n_inputs` / `ke->n_ops` -- a static
`[KERNEL_MAX_INPUT]` declaration with the new 1M cap would
request 4MB of stack.

Memory: `KernelEntry` shrank from ~33KB inline to ~136 bytes +
proportional growth.  KERNELS table baseline went from ~528MB to
~2MB.

---

## 6. Profiling: TProfile

The `TProfile[]` WL function (in
[`wl/THVMLink/Kernel/Profile.wl`](../wl/THVMLink/Kernel/Profile.wl))
returns an Association snapshotting heap cells by tag, tensors,
distinct buffer ids, alias counts, kernels, and per-kernel
inputs/ops.  Use it to spot allocation leaks across chain-rule
rounds:

```wolfram
profiles = Table[
    TInit[];
    a = TTensorCreate[...];
    y = ...build expression...;
    TRealize[ Nest[ TGrad[#, a] &, y, k]];
    TProfile["round=" <> ToString[k]],
    {k, 1, 4}];

WriteString["stdout", TProfileReport[profiles] <> "\n"];
```

Output:

```
round=1: cells=86 (UOP=47 TEN=16 NUM=14 SUP=6 DP0=3)
  tensors=10 ... kernels=4 max_inputs=6 max_ops=9 ITRS=26
round=2: cells=494 ...
  growth: cells x5.74 kernels x2.25 max_inputs x2.67 max_ops x3.56
...
```

Companion APIs:

  - `TProfileTable[ps]`   — side-by-side Tabular comparison
  - `TProfileGrowth[ps]`  — round-over-round growth ratios
  - `TProfilePlot[ps, k]` — line plot of metric `k` across snapshots

The regression test [`wl/THVMLink/Tests/profile.wlt`](../wl/THVMLink/Tests/profile.wlt)
locks in current allocation counts for known workloads so future
bloat trips a test.

---

## See also

  - [`grad-roadmap.md`](grad-roadmap.md) — historical, describes
    the older target-in-cell design (now superseded)
  - [`kernelization.md`](kernelization.md) — boundary classification
    and the materialize -> wnf loop
  - [`heap.md`](heap.md) — heap layout, cell tags, SUB-bit
    substitution semantics
  - [`normal_form.md`](normal_form.md) — wnf vs nf, the reduction
    discipline
