# Grad: dup-like VJP with gy threading

Automatic differentiation in thvm: what `TGrad` builds, how it
reduces, and how the runtime materializes it.

The pieces:

1. **Cell layout** — what a grad cell actually is in heap memory
2. **Chain rule** — per-operator adjoints emitted by `interact_grad`
3. **Leaf rule + target routing** — how the per-target gradient gets extracted
4. **Reduction** — wnf as the only driver; "stronger HNF" via active-child descent
5. **Higher-order** — composing TGrad with itself
6. **Materialization** — how the chain-rule output gets compiled into kernels
7. **Profiling** — using `TProfile` to spot allocation leaks

---

## 1. Cell layout

A grad cell is a regular dup-style heap cell with three slots:

```
heap[loc + 0] = y        (the value being differentiated)
heap[loc + 1] = gy       (the cotangent at y's shape)
heap[loc + 2] = target   (TVAR/TEN to differentiate w.r.t., or 0 for legacy SUP/DUP path)
```

Two HVM4-style dup projections share this cell:

| Term                            | Role                                                                                          |
|---------------------------------|-----------------------------------------------------------------------------------------------|
| `TAG_DP0` + `DUP_GRAD_FLAG`     | **FWD** (forward projection): on force, returns `cell[0] = y`                                 |
| `TAG_DP1` + `DUP_GRAD_FLAG`     | **BWD** (backward projection): on force, fires the chain rule via `interact_grad`             |

Constructors live in [`src/uop/grad.c`](../../src/uop/grad.c):

  - `uop_grad(y, gy)` — BWD projection, `target = 0`
  - `uop_fwd(y, gy)` — FWD projection of the same cell
  - `uop_grad_with_target(y, gy, target)` — BWD with target stored in slot 2

The `DUP_GRAD_FLAG` bit on the projection's ext field is what
distinguishes a grad cell from a regular DUP/SUP — `wnf` and
`redex_fire` dispatch to `interact_grad` instead of
`interact_dup_X`.

`target` matters for the chain rule's leaf handler: when set, the
recursion keeps a thread-static `GRAD_TARGET_STACK` so every
sub-cell allocated during the rewrite inherits it (see
[`src/interact/uop_grad.c:101-128`](../../src/interact/uop_grad.c#L101-L128)).
At a leaf the rule emits `gy` on a tid match and a scalar zero on
mismatch — no SUP/DUP scaffolding needed. When `target == 0` the
leaf falls back to emitting `SUP^{leaf_tid}(0, gy)` and the WL
surface wraps the BWD with a per-leaf-tid DUP nest to project the
target's gradient.

---

## 2. Chain rule

`interact_grad` (in [`src/interact/uop_grad.c`](../../src/interact/uop_grad.c))
walks `y`'s outermost UOp, computes the per-child cotangent
`gy_for_a_i`, and emits a sub-cell `[a_i, gy_for_a_i, target]`
plus an outer combiner. The adjoint table mirrors tinygrad / JAX:

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
| `REDUCE_MAX(a, x)`| `MUL(mask, EXPAND(reshape_keepdim(gy, x), a.shape))` with `mask = CMPEQ(a, lift(MAX(a, x)))` |
| `EXPAND(a, S)`    | `REDUCE_SUM` along expanded axes (handles rank-increase via leading-1 padding) |
| `RESHAPE(a, S)`   | `RESHAPE(gy, a.shape)`                                              |
| `PERMUTE(a, p)`   | `PERMUTE(gy, inv_perm)`                                             |
| `FLIP(a, m)`      | `FLIP(gy, m)`                                                       |
| `PAD(a, b/e)`     | `SHRINK(gy, [b_i, b_i + a.dim_i])`                                  |
| `SHRINK(a, b/e)`  | `PAD(gy, [b_i, src.dim_i - e_i])`                                   |
| `CAST(a, dt)`     | `CAST(gy, a.dtype)` — value-preserving                              |
| `IWHERE(c, t, e)` | `gy_for_t = where(c, gy, 0)`; `gy_for_e = where(c, 0, gy)`; `c` gets none (tinygrad `mixin/gradient.py:81`) |
| `GATHER(s, i, d)` | `gy_for_s = SCATTER(SUM, zeros(s.shape), i, gy, d, include_self=1)`: `gy` summed back into the positions `i` read; `i` gets none |
| `SCATTER(b, i, s, d, SUM, self)` | `gy_for_s = GATHER(gy, i, d)`; `gy_for_b = gy` when `self`, else `SCATTER(REPLACE, gy, i, zeros(i.shape), d)` (`gy` with every written position zeroed); `i` gets none |
| `SCATTER(b, i, s, d, MAX, self)` | With `M = SCATTER(MAX, full(I), i, s, d, 1)` and `T = b` (`self`) or `SCATTER(REPLACE, b, i, full(I), d)`: `g_M` / `g_T` = `gy` where `M` / `T` is larger, `gy * 0.5` where `M == T`, else 0; `gy_for_b = g_T`, written positions zeroed unless `self`; `gy_for_s = (s == G(M)) * G(1 / cnt) * G(g_M)` with `G` a gather along `d` and `cnt` the ties of `M` among the `K = i.shape[d]` one-hot entries; MIN mirrors it; `i` gets none |
| `CMPLT/CMPEQ`     | non-differentiable; emits scalar zero                               |
| `BITCAST/CONST/LOAD` | non-differentiable; cotangent dies                                |
| `ASSIGN(dst, v)`  | `gy` to `v`, none to `dst`: an assign or a clone passes the cotangent to the value it writes (tinygrad `mixin/gradient.py:98-99`) |
| `KERNEL(...)`     | `grad_kernel_backprop` (see below)                                  |

The outer combiner for binary ops is `UOP_ADD(BWD_a, BWD_b)`
(reverse-mode summation of contributions); for unary/movement ops
it's just `BWD_a`.  `IWHERE` combines its two value branches the same
way.

A value select, and the masks the `IWHERE` rule builds, go through
`uop_where` ([`src/uop/where.c`](../../src/uop/where.c)), not
`uop_iwhere`: `uop_iwhere` runs the index-layer folds, whose
IWHERE-of-IWHERE collapse turns nested value selects into tensor-level
`IAND`/`IOR` nodes that materialize cannot schedule.  `uop_where` builds
the same `UOP_IWHERE` node and folds only a constant condition or equal
branches.  The py tinygrad importer (`from_tinygrad`) maps `WHERE` (and
`MAX`, a select on `a < b`) through it.

The requires-grad single walk pre-counts each node's backward arrivals
before any chain rule fires (`grad_prewalk_fanin`); it credits exactly the
child slots the rule sends a cotangent to (`grad_op_diff_slot_mask`: every
arity slot of a differentiable op, none of a sink, `IWHERE`'s value slots
but not its condition, `GATHER`'s src, and `SCATTER`'s base and src but
never an index), so no slot waits for an arrival that never comes.

### Indexed ops

The `GATHER` and `SCATTER` rules are the gradients of tinygrad's one-hot
formulations (`Tensor.gather`, `mixin/op.py:1023-1041`, and
`scatter_reduce` sum / amax / amin, `mixin/op.py:1126-1130`, through
`mixin/gradient.py`), built from the same two native ops, so a backward is
linear in its operand sizes: no one-hot mask and no REDUCE.

  - The gather gradient accumulates in tinygrad's `sum_acc_dtype`
    (float32 for the 16- and 8-bit floats, `mixin/gradient.py:133`) and
    casts back.  Duplicate indices sum in ascending order along `dim`;
    an out-of-range index contributes nothing, as the forward read 0.
  - `SCATTER`'s base gradient without `include_self` is tinygrad's
    `_inv_mask(gy, 0)`: a `REPLACE` scatter of zeros, so a non-finite
    cotangent at a written position becomes an exact 0, as `where` gives.
  - A one-element cotangent (the scalar-zero convention of
    `grad_zero_at`) is broadcast to the node's shape first.
  - `MAX` and `MIN` (`grad_scatter_maxmin`) follow tinygrad's amax
    `M.maximum(T)` through its `MAX` rule (`mixin/gradient.py:78-79`), its
    `REDUCE MAX` rule (`:10-13`) and `WHERE`'s (`:81`): `M`, the src-only
    max, is recomputed as a scatter from the identity, tied maxima split
    `g_M` evenly, a tie between `M` and the base gives each side half, and
    the tie count includes the one-hot's non-hits when `M` is the identity
    (every hit is then the identity too), so `cnt = e * K + SA(c - G(e))`
    with `e = (M == I)` and `c = (src == G(M))`.  The count accumulates in
    `sum_acc_dtype`; the mask is multiplied in as tinygrad multiplies it,
    `(c * (1 / cnt)) * g_M`.  MIN mirrors MAX, down to the -0.0 that
    tinygrad's negated MAX gives a losing branch.
  - `PROD` and `REPLACE` scatters have no rule yet.  Their
    cell fires to zero and the chain rule records the first such node in
    its error slot (`grad_err_set`; the message also goes to stderr).
    `grad_err_take()` returns the pending message and clears it, or NULL;
    the frontends take it after a backward walk and raise.

### TEN-leaf short-circuit

When a child `a` is a `TAG_TEN`, the chain rule skips the cell
allocation and emits the leaf result directly via `grad_leaf_sup`.
For deeply nested y this saves O(leaves) heap cells per chain-rule
round.

### CONST cache

`uop_const(dtype, bits)` (in [`src/uop/const.c`](../../src/uop/const.c))
deduplicates by `(dtype, bits)` via a 16K open-addressed hash
table. CONSTs are atomic; sharing the heap loc across references
is always safe; materialize dedups by heap loc identity.

### Kernel fast-path: `grad_kernel_backprop`

When `y` is `TAG_UOP/UOP_KERNEL` (or a `TAG_TEN` whose
`producer_kid` resolves to one), the chain rule short-circuits
through [`grad_kernel_backprop`](../../src/interact/uop_grad.c#L567):
it walks the kernel's program in reverse, applying each opcode's
adjoint directly against the in-flight cotangent vector and
emitting fresh UOp sub-terms only at the kernel inputs. No DP1
cells are allocated for the kernel's interior. The per-input
contributions then go through `grad_bwd_for_child`, which routes
back into the regular cell-based recursion for any non-TEN inputs.

### Memoization (per-realize generation)

`grad_bwd_for_child(child, gy, target)` is deterministic in its
three keys, but the chain rule's straight recursion would re-walk
shared sub-DAGs once per parent. Two open-addressed tables short
this out:

  - **`GRAD_MEMO`** — keyed on `(child, gy, target)`, returns the
    cached BWD reference so subsequent visits share the existing
    grad cell.
  - **`GRAD_DEP`** — `grad_depends_on_target(t, target)` answers
    "does this subgraph reach `target` at all?". Independent
    subtrees collapse to a scalar zero immediately, skipping the
    chain rule entirely. Crucial for multi-target VJPs where most
    of `y` is independent of any given target.

Both tables use a generation counter bumped at
[`grad_memo_begin_realize`](../../src/interact/uop_grad.c#L300) (called
from `thvm_realize`) so entries survive across one realize call's
recursive fires but cannot stale across separate user calls.

---

## 3. Leaf rule + target routing

There are two paths, switched by whether the cell's `target` slot
is non-zero:

### Target-bearing path (the common case)

`TGrad[y, target]` calls `TUOpGradWithTarget[y, gy, target]` which
puts `target` in cell[2]. On entry, `interact_grad` pushes target
on the per-fire stack so every sub-cell inherits it. A child, a
cell's `y` and a `TAG_TEN` leaf all go through one test,
`grad_term_matches_target`, and get `gy` when they are the target:

  - the same bound variable once ALOs are forced;
  - the same tid;
  - the same value, read without firing anything: `term_resolve`,
    then through a DUP that has not fired to the body both of its
    projections copy.

The last case is a recursive loop past its first step. There the
bound `w` is a graph, and auto-dup hands the update, the loss and
the target three projections of one DUP over it. The rule sees `y`
unreduced, so the loss's `w` reaches the test before that DUP fires.
A leaf that is not the target returns `uop_const(target.dtype, 0)`,
a scalar zero that broadcasts through any outer ADD or MUL.

No SUP, no per-leaf-tid DUP nest, no cross-product fires across
distinct leaves. This is what makes TGrad cheap on graphs with
many independent leaves (LeNet has ~10).

### Legacy SUP/DUP path (target == 0)

When `target == 0`, the leaf rule emits:

```
SUP^{leaf.tid}(CONST(0), gy)
where CONST(0) is shape {1}, broadcasts via cpu_op_add/mul
      gy is at leaf.shape
```

The scalar-zero mismatch slot is critical: `cpu_op_add` / `cpu_op_mul`
broadcast `numel == 1` operands, so when the outer combiner sees
`ADD(matched_at_leaf_shape_X, CONST(0))`, the result is just
`matched_at_leaf_shape_X` regardless of `X`.

The WL surface then wraps the BWD term in a per-leaf-tid DUP nest
that projects each target's gradient. Used by `TGradPair` and any
caller that wants to extract multiple targets from one shared
chain-rule output without rebuilding it.

### Outer shape-pad

After the outermost grad fire, [`interact_grad`](../../src/interact/uop_grad.c#L1361-L1389)
checks the result's inferred shape against the target's shape and
wraps with `ADD(result, EXPAND(CONST(0), target.shape))` only on a
confirmed mismatch. Inner recursive fires don't pad (their result
is at leaf shape, not target shape, by design). Without the gate
every grad cell would emit an extra EXPAND+ADD kernel.

---

## 4. Reduction: completing the BWD branch via wnf

The realize loop is `wnf(res) → materialize → repeat` until no
fresh kernels emit
([`src/schedule/realize.c:88-97`](../../src/schedule/realize.c#L88-L97)).
**`wnf` is the only reducer in the hot path.** `nf` exists as an
inspector primitive (driven from `TNf` and a few profilers) but is
not used by realize.

The interesting wnf trigger is when it's pointed at a grad cell's
BWD projection (`DP1+DUP_GRAD_FLAG`). Apply-phase dispatch at
[`src/wnf/_.c:482-498`](../../src/wnf/_.c#L482-L498) calls
`interact_grad(frame)`; the chain rule returns a new term — for a
binary op typically `UOP_ADD(BWD_a, BWD_b)` whose children are
themselves fresh `DP1+grad` cells — and wnf re-enters on that
result. Without further help the re-entered wnf would stop at the
ADD head and leave the two grad children unfired, since plain WHNF
doesn't descend into UOP children.

A BWD projection whose `y` is a UOP other than a `KERNEL` runs the
rule on `y` as it stands, as tinygrad's `compute_gradient` never
evaluates the graph: reducing `y` first would fire its active
children, and a copy `ASSIGN` among them (a clone of a buffer) would
leave its `dst` in `y`, cutting the cotangent to the value it writes.
Only a `KERNEL` or a non-UOP `y` (a DP chain from an earlier fire, an
ALO) is reduced before the rule.

The TAG_F_UOP_CHILD stack-machine frames at
[`src/wnf/_.c:214-243`](../../src/wnf/_.c#L214-L243) and
[`src/wnf/_.c:787-828`](../../src/wnf/_.c#L787-L828) close that gap.
On entering any UOP, wnf checks for an *active child* — DP0/DP1+grad,
nested KERNEL, or nested ASSIGN — via
[`uop_next_active_child`](../../src/interact/uop_grad.c#L1327). When
found, it pushes a `TAG_F_UOP_CHILD` frame and descends into the
child slot:

  1. Child reduces; apply pops the frame, heap-sets the WHNF result
     back, then re-scans from `child_idx + 1` for the next active
     sibling.
  2. If the result is itself a UOP that *also* has active descendants
     (DUP-UOP commute can produce this), wnf reentrantly re-drives
     it before storing.
  3. Once all active children are drained, rebuild the parent UOP
     and treat it as WHNF.

So the relevant guarantee is conditional, not universal: wnf called
on a forward-only UOP doesn't go hunting for grad children — the
active-child scan still runs but `uop_child_is_active` returns
false on every slot (no DP1+grad, no KERNEL, no ASSIGN anywhere
in the subgraph), and wnf treats the UOP as WHNF immediately. The
descent only does work when an earlier grad fire has produced an
ADD-of-grad-cells (or wnf is walking a graph that already contained
a KERNEL/ASSIGN). In effect, the chain-rule recursion is driven by
the grad cells themselves; the active-child plumbing is the wnf-side
support that lets one outer `wnf(res)` call pop the entire BWD
branch in one descent.

The chain rule itself isn't "lazy" in any meaningful sense: it's a
normal `interact_grad` interaction fired by wnf's normal apply-phase
dispatch. It's not a deferred computation that some other pass needs
to pump.

The `uop_act_memo` generation-counter table makes the active-child
scan O(unique-cells) per outer wnf call rather than O(visits to
shared subgraphs).

---

## 5. Higher-order

`TGrad` returns a **symbolic** UOp graph — it's just a term, not a
materialized tensor. Apply `TGrad` to it again to get the Hessian
(or higher derivative). The chain rule walks the inner-`TGrad`'s
output as if it were any other UOp graph; the outer wnf pass drives
both rounds.

The apply phase for grad-DP1 sees the resolved `cell[0]` regardless
of how deeply DPs nest. Combined with the dynamic KernelEntry arrays
(kernels grow geometrically instead of being capped at 64 inputs /
256 ops), 4th-, 5th-, 6th-order derivatives all work.

Example: Newton's method using TGrad twice (in
[`wl/Examples/newton-1d/newton.wls`](../../wl/Examples/newton-1d/newton.wls)):

```wolfram
g = TGrad[f, x];        (* gradient *)
h = TGrad[g, x];        (* Hessian = gradient of gradient *)
xNew = x - g / h;       (* Newton step *)
```

Convergence is quadratic — five iterations to f32 precision for
`f(x) = 0.5*(x-3)^2 + 0.25*x^4`.

---

## 6. Materialization

`TRealize` calls `thvm_realize` (in
[`src/schedule/realize.c`](../../src/schedule/realize.c)) which loops
`wnf(res) → thvm_materialize` until no fresh kernels emit.
`thvm_materialize` (in [`src/schedule/materialize.c`](../../src/schedule/materialize.c))
walks the term, classifies realize boundaries (each compute UOP
becomes a candidate), and emits one `KernelEntry` per boundary.
Materialize never fires interactions; it's purely graph → kernel
compile.

Convergence is typically 2-3 iterations. Each iteration's wnf
fires every BWD via the active-child machinery above; materialize
then compiles the freshly exposed compute UOPs to kernels;
subsequent iterations exist mainly to expose UOP graphs that were
hidden behind a still-active grad cell on the prior round.

### Dynamic KernelEntry arrays

`KernelEntry::input_*` and `program` are heap-grown pointers with
`inputs_cap` / `ops_cap` fields. Helpers in
[`src/schedule/kernel_alloc.c`](../../src/schedule/kernel_alloc.c):

  - `kernel_inputs_reserve(ke, needed)` — `realloc` to geometric
    capacity (init=8, doubles).
  - `kernel_program_reserve(ke, needed)` — same for the program.
  - `kernel_free_arrays(ke)` — release on dealloc.

`KERNEL_MAX_INPUT` / `KPROG_MAX_OPS` act as 1M sanity bounds (only
trip on runaway allocation). The stack arrays in
`cpu/interpret.c`, `metal/_.m`, and `interact/uop_kernel.c` are VLAs
sized by `ke->n_inputs` / `ke->n_ops`, because a static
`[KERNEL_MAX_INPUT]` declaration with the 1M cap would request 4MB
of stack.

Memory: `KernelEntry` is ~136 bytes + proportional growth, giving a
~2MB KERNELS table baseline.

---

## 7. Profiling: TProfile

The `TProfile[]` WL function (in
[`wl/THVMLink/Kernel/Profile.wl`](../../wl/THVMLink/Kernel/Profile.wl))
returns an Association snapshotting heap cells by tag, tensors,
distinct buffer ids, alias counts, kernels, and per-kernel
inputs/ops. Use it to spot allocation leaks across chain-rule
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

The regression test [`wl/THVMLink/Tests/profile.wlt`](../../wl/THVMLink/Tests/profile.wlt)
locks in allocation counts for known workloads, so bloat trips a
test.

---

## See also

  - [`lowering_passes.md`](lowering_passes.md) - boundary
    classification and the materialize -> wnf loop
  - [`../core/heap.md`](../core/heap.md) - heap layout, cell tags, SUB-bit
    substitution semantics
  - [`../core/normal_form.md`](../core/normal_form.md) - wnf vs nf, the reduction
    discipline
