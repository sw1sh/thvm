# Tensors

Proposal for [PLAN.md](../PLAN.md) step 12: `TTensor` and `TUOp`
primitives for building computational graphs that compile to CPU and
Metal backends.

> Vocabulary: see [glossary.md](glossary.md) for definitions of
> *tensor*, *buffer*, *View*, *ShapeTracker*, *opcode*, *AST*,
> *kernelize*, *fusion*, *linearization*, *codegen*, *dispatch*,
> *firing*, etc.

This is a *design proposal* — nothing is implemented yet. It draws
on four reconnaissance passes:

- **tinygrad** (`/Users/swish/src/tinygrad/`) — the canonical UOp
  vocabulary for representing tensors as a lazy graph
  (`BUFFER`, `VIEW`, `LOAD`, `CONST`, `KERNEL`, ...).
- **TinyHVM** (`/Users/swish/src/TinyHVM/`) — how that vocabulary
  fits into an HVM4-style C runtime: `View`, `ShapeTracker`,
  refcount, backend vtable, post-reduce dispatch.
- **C-ML** (`/Users/swish/src/C-ML/`) — independent third-party
  C ML framework. Cross-checked for cleaner choices than TinyHVM
  on tensor struct, op enum layout, and backend dispatch.
- **HVM4** (`/Users/swish/src/HVM4/`) — the `TAG_OP2 ↔ TAG_NUM`
  WNF interaction template, the model for how `UOP_KERNEL` fires
  when its operands are concrete.

## Three new term tags

| tag        | id | encoding |
|-----|-----|-----|
| `TAG_TEN` | 8 | atom; `VAL` = tensor id (index into `TENS[]`), `EXT` = dtype |
| `TAG_UOP` | 9 | heap-backed; `VAL` = first heap cell of payload, `EXT` = UOp opcode |
| `TAG_NUM` | 10 | atom; `VAL` = 32-bit integer or bit-cast `f32`, `EXT` = dtype |

All three go at the end of the existing tag namespace (`TAG_DUP=7`).
`TAG_TEN` and `TAG_NUM` are atoms (no heap). `TAG_UOP` always
allocates on the heap and the per-opcode cell layout is fixed at
compile time (see the table further down).

`TAG_NUM` exists so that UOp opcodes that need scalar args (e.g.
`UOP_RESHAPE` carrying new dimension sizes) can put them inline as
heap cells without a side table. It is *not* the start of a `TAG_OP2`
arithmetic system — there are no `OP_ADD` / `OP_SUB` interaction
rules in step 12.

## UOp vocabulary

Aligned with tinygrad's `Ops` enum
(`/Users/swish/src/tinygrad/tinygrad/uop/__init__.py`) but
trimmed: the lazy-graph layer (BUFFER / VIEW / LOAD / STORE / DEVICE /
ASSIGN / SINK) is what tinygrad uses to represent both *user
intent* and *post-schedule kernel internals*. We split the two
roles: user intent in step 12; kernel internals in step 14.

### Step 12 — fires under reduce

```c
/* materialization trigger.  Rewrites the raw UOp subgraph into a
   scheduled DAG of UOP_KERNELs (with the required passes: schedule,
   kernelize, compile).  After firing, the resulting graph has
   compiled KERNELs sitting alongside whatever IC combinators the
   user had around them. */
#define UOP_MATERIALIZE  0   /* heap = [expr] */

/* compute boundary.  Its AST leaves are TAG_TEN / UOP_KERNEL /
   other UOps; when every leaf is TAG_TEN the kernel fires via the
   cached compiled binary.  NATURAL firing under reduce -- no
   special stack-frame wrapper required. */
#define UOP_KERNEL       1   /* heap = [output_buf, ast_root] */
```

**One reducer, two WL entry points.**

```mathematica
expr  = TUOpAdd[a, b];
mat   = TMaterialize[expr];     (* rewrite-only; no firing *)
THeapDiagram[mat]                (* inspect the scheduled DAG *)
out   = TWnf[mat];               (* natural reduction -- fires KERNELs *)
TTensorData[out]                  (* {4.0, 6.0} *)
```

`TMaterialize` is a WL-visible helper; `TWnf` is the ordinary
reducer. Under the hood `TMaterialize` runs `interact_uop_materialize`
as a direct heap rewrite (not through `wnf`), so `UOP_KERNEL`s
don't fire yet; then subsequent `TWnf` reduces naturally, firing
each kernel when its leaves have become `TAG_TEN`.

The one-call shortcut for users who don't want to inspect:

```mathematica
out = TWnf[TUOpMaterialize[expr]]   (* everything in one go *)
```

Both `UOP_MATERIALIZE` and `UOP_KERNEL` are just rules under the
reducer; what distinguishes `TMaterialize` from `TWnf` is only
*which* rules the entry-point lets fire (the latter lets all of
them).

### Step 12 — inert, user-built lazy graph

These appear in raw graphs, get walked by `kernelize`, and end up
inside a KERNEL's AST. They never have an interaction rule of their
own — under WNF they're values.

```c
/* literal scalar value; ext = dtype, val = bit-cast u32 (or heap loc for >32b) */
#define UOP_CONST    2

/* movement ops -- carry concrete args as additional heap cells.
   heap = [src, NUM(arg0), NUM(arg1), ...]                                */
#define UOP_RESHAPE  3
#define UOP_PERMUTE  4
#define UOP_EXPAND   5
#define UOP_PAD      6
#define UOP_SHRINK   7
#define UOP_FLIP     8

/* element-wise compute -- heap = [src0, src1, ...] */
#define UOP_ADD      9
#define UOP_MUL     10
#define UOP_NEG     11
#define UOP_RECIP   12
#define UOP_EXP2    13
#define UOP_LOG2    14
#define UOP_SQRT    15
#define UOP_CMPLT   16

/* reduce -- ext bits = (kind << 8) | axis;  heap = [src] */
#define UOP_REDUCE  17

#define UOP_COUNT   18
```

### Deferred (steps 13/14)

```
UOP_BUFFER    /* lazy buffer handle; needed to chain kernels */
UOP_VIEW      /* post-flatten ShapeTracker carrier */
UOP_LOAD      /* explicit indexed read inside a kernel AST */
UOP_STORE     /* explicit indexed write */
UOP_DEVICE    /* multi-device support; one CPU backend in step 12 */
UOP_ASSIGN    /* in-place writes / multi-output */
UOP_SINK      /* aggregate multiple ASSIGNs */
UOP_GRAD      /* reverse-mode autograd, rewrite rule */
UOP_EXEC      /* TinyHVM dispatch trigger; we use UOP_KERNEL instead */
```

We deliberately keep `TAG_OP2`-style scalar arithmetic (HVM4's
`OP_ADD` over 32-bit ints) out of `TAG_UOP`. Numeric scalar ops are
a separate tag (would be `TAG_OP2` if/when we need them). UOps are
strictly tensor-shaped.

### Per-opcode heap layout

Mirrors [term.md](term.md)'s tag table. Heap layout is
*per-opcode*; the size is recoverable from a compile-time
`uop_layout[]` table.

| opcode         | EXT meaning             | Heap cells                                       |
|-----|-----|-----|
| `UOP_KERNEL`   | unused                  | `[output_buf, ast_root]`                         |
| `UOP_REALIZE`  | unused                  | `[expr]`                                         |
| `UOP_CONST`    | dtype                   | `[NUM(bits)]` — bits is `u32` raw of the value   |
| `UOP_RESHAPE`  | new ndim                | `[src, NUM(d0), NUM(d1), ..., NUM(d_{n-1})]`     |
| `UOP_PERMUTE`  | ndim                    | `[src, NUM(p0), NUM(p1), ..., NUM(p_{n-1})]`     |
| `UOP_EXPAND`   | new ndim                | `[src, NUM(d0), ..., NUM(d_{n-1})]`              |
| `UOP_PAD`      | ndim                    | `[src, NUM(b0), NUM(e0), NUM(b1), NUM(e1), ...]` |
| `UOP_SHRINK`   | ndim                    | `[src, NUM(b0), NUM(e0), ...]`                   |
| `UOP_FLIP`     | unused                  | `[src, NUM(axes_bitmask)]`                       |
| `UOP_ADD/MUL/CMPLT`     | unused        | `[src0, src1]`                                   |
| `UOP_NEG/RECIP/EXP2/LOG2/SQRT` | unused | `[src]`                                          |
| `UOP_REDUCE`   | `(kind << 8) \| axis`  | `[src]`                                          |

`NUM(x)` here means a `TAG_NUM` term carrying `x` — the same scalar
tag we'd use for `TAG_OP2`. Step 12 reserves `TAG_NUM` (id 10) too.

### How a Tensor becomes a UOp graph (step 12 form)

```
TTensor[{2}, {1.0, 2.0}]
  =>  TAG_TEN ext=DT_F32 val=tensor_id   (concrete; backed by a buffer)

TUOpAdd[a, b]
  =>  TAG_UOP ext=UOP_ADD val=loc
      Heap[loc..loc+1] = [a, b]

TWnf[TUOpRealize[expr]]
  =>  reduce hits UOP_REALIZE -> kernelize rewrites expr into:
      TAG_UOP ext=UOP_KERNEL val=loc
      Heap[loc..loc+1] = [output_buf, ast_root]
  =>  reduce continues; KERNEL fires when leaves of ast_root are
      TAG_TEN; backend dispatches; result is a fresh TAG_TEN
```

Tinygrad's BUFFER / DEVICE / VIEW chain is *not* part of step 12 —
those are post-flatten shapes that step 14's linearizer produces
inside a kernel AST.

## View + ShapeTracker (verbatim from TinyHVM)

We adopt TinyHVM's `View` and `ShapeTracker` as-is, including the
4-view stack and compound-mask slots, so movement-op composition
works without later migration:

```c
#define MAX_DIM       8
#define ST_MAX_VIEWS  4

typedef struct {
  u32 ndim;
  u32 dims[MAX_DIM];
} Shape;

typedef struct {
  Shape shape;
  i32   strides[MAX_DIM];          /* 0 = broadcast, negative = flip */
  i32   offset;
  u32   numel;
  u8    contiguous;
  u8    has_mask;
  u32   mask_begin[MAX_DIM];
  u32   mask_end[MAX_DIM];
  u8    n_compound_masks;
  struct { u8 dim_a, dim_b; i32 stride_a; u32 begin, end; } compound_masks[2];
  u32   mod_size[MAX_DIM];
} View;

typedef struct {
  View views[ST_MAX_VIEWS];
  u8   n_views;                    /* 0 = unused; 1+ = stacked */
} ShapeTracker;
```

Movement ops follow TinyHVM's "mutate or push" rule:

```c
/* In src/tensor/view/<op>.c — analog of TinyHVM/src/tensor/view/<op>.c */
ShapeTracker view_reshape(ShapeTracker st, Shape new_shape) {
  View merged = view_try_reshape(st.views[st.n_views - 1], new_shape);
  if (merged.numel != 0) {
    st.views[st.n_views - 1] = merged;        /* merged into top view */
  } else if (st.n_views < ST_MAX_VIEWS) {
    st.views[st.n_views++] = view_create(new_shape);   /* push fresh */
  }
  return st;
}
```

Indexing walks the ShapeTracker right-to-left: logical index →
inner View strides → middle → outer View strides → buffer offset.

## TenDesc — minimal v1, with refcount

Stripped from TinyHVM's `TensorMeta`: keep refcount, View, dtype,
backend vtable, host pointer; drop the conv/pool/fusion bookkeeping
until step 14 needs it.

```c
typedef struct Backend Backend;

typedef struct {
  u32          dtype;          /* DT_F32 / DT_I32 / ... */
  u32          refcount;       /* shared by DUP; decremented by ERA */
  ShapeTracker st;             /* multi-view; supports broadcast/flip */
  u32          buf_id;         /* backend buffer handle */
  void        *host_ptr;       /* cached host copy if BACKEND_CPU */
  Backend     *backend;        /* vtable */
  u8           requires_grad;
} TenDesc;

extern TenDesc *TENS;
extern u32      TENS_NEXT;
```

## Refcount + IC integration

This is what the user explicitly asked for. Verbatim from TinyHVM
(`src/interact/combinators.c`):

- **Allocation**: `tensor_alloc()` sets `refcount = 1`.
- **DUP on a `TAG_TEN`**: when a DUP rule reads a SUB cell whose
  value is a tensor, it calls `tensor_incref(id)` so both aux ports
  end up holding a live reference to the same buffer:

  ```c
  if (term_tag(v) == TAG_TEN) tensor_incref(ctx, term_val(v));
  ```

- **ERA on a `TAG_TEN`**: `tensor_release(id)` decrements; when
  refcount hits zero it calls `backend->buf_decref(buf_id)` to free
  the actual storage.

- **View aliasing** (e.g. reshape): `tensor_view_of(src)` creates a
  new `TenDesc` sharing `buf_id`, then bumps `backend->buf_incref`
  on the buffer. This lets reshape/permute/etc. produce views
  without copying.

## Backend vtable

Direct adoption of TinyHVM's `Backend` interface, trimmed to the
ops that step 12 actually exercises (alloc/free/incref/decref +
host I/O); the compute callbacks come online in step 14.

```c
struct Backend {
  u32   id;
  int   (*init)(void);
  void  (*shutdown)(void);
  u32   (*buf_alloc)(u64 nbytes);
  void  (*buf_free)(u32 buf_id);
  void  (*buf_incref)(u32 buf_id);          /* shared aliasing */
  void  (*buf_decref)(u32 buf_id);          /* dec; free at zero */
  int   (*buf_read) (u32 buf_id, void *dst, u64 nbytes);
  int   (*buf_write)(u32 buf_id, const void *src, u64 nbytes);
  /* op_unary / op_binary / dispatch_kernel arrive in step 14 */
};
```

CPU backend (step 12): `buf_alloc` is `aligned_alloc`,
`buf_free` is `free`, refcount in a parallel `BUFS[]` table.

Metal backend (step 14): `buf_alloc` returns an `id<MTLBuffer>`
slot, refcount on the `MTLBuffer`. Linked behind
`-framework Metal` so the CPU build doesn't pay for it.

## End-to-end pipeline: tinygrad arch under natural IC reduction

The full tinygrad lifecycle is:

```
Tensor ops  -->  lazy UOp graph  -->  materialize (schedule + kernelize + compile) -->
                                      KERNEL+ASSIGN+SINK DAG        -->
                                      fire each KERNEL bottom-up in natural reduce
```

We map that onto two rules under one reducer:

| stage              | rule                   | what it does                                                                                                       | result                                                  |
|-----|-----|-----|-----|
| **build**          | — (WL constructors)   | — no reduction, just TAG_UOP nodes in heap                                                                          | raw UOp graph                                            |
| **materialize**    | `UOP_MATERIALIZE`     | walks `expr`, picks kernel boundaries, allocates fresh `UOP_BUFFER`s for outputs, hits the kernel cache (compiles on miss), emits `UOP_KERNEL[output_buf, ast_root]` nodes spliced back into the graph | scheduled DAG: compiled KERNELs interleaved with IC combinators |
| **firing**         | `UOP_KERNEL`          | when every AST leaf is `TAG_TEN`, invokes the compiled binary via the backend, returns a fresh `TAG_TEN`            | `TAG_TEN` (or `SINK` of TENs)                           |

**Two WL entry points, one reducer**:

- `TMaterialize[expr]` runs the materialization rewrite directly —
  it does NOT go through `wnf`, so `UOP_KERNEL`s don't fire. You
  get the scheduled DAG for inspection.
- `TWnf[t]` is the normal reducer. If `t` contains a
  `UOP_MATERIALIZE`, that fires first, then the produced KERNELs
  fire naturally as their leaves become `TAG_TEN`. If `t` is
  already materialized, only the firing happens.

Between the two:

```mathematica
expr  = TUOpAdd[a, b];
mat   = TMaterialize[expr];     (* scheduled DAG, not fired *)
THeapGraph[mat]                  (* visualize *)
THeapDiagram[mat]
out   = TWnf[mat];               (* fire bottom-up *)
```

### What `interact_uop_materialize` does

Five sub-passes inside one rewrite. Each owns a file under
`src/schedule/`; the top-level `materialize.c` drives them:

```c
fn Term interact_uop_materialize(Term expr) {
  /* 1. schedule(expr)
        Pick fusion boundaries.  Step-12 policy:
          - Elementwise chains fuse (ADD, MUL, NEG, RECIP,
            EXP2, LOG2, SQRT, CMPLT, CONST at the leaves).
          - REDUCE terminates a kernel (its own output shape
            differs from its input).
          - Movement ops (RESHAPE, PERMUTE, ...) terminate a
            kernel (post-step-14 they absorb into the next
            kernel's VIEW, but step 12 keeps them separate).
          - Existing user-built UOP_KERNEL is opaque -- a leaf
            to the scheduler, fed in as an input to the next
            kernel above it.
        Result: a forest of "kernel spans" rooted at each output.

     2. kernelize(span)
        For each span, allocate a fresh UOP_BUFFER for the output
        and emit a UOP_KERNEL[output_buf, ast_root] node wrapping
        the fused compute subgraph.

     3. linearize(ast_root)
        Flatten the kernel's AST into an SSA-over-indices
        program.  Output is a KernelEntry.program[] array where
        each entry is {opcode, dtype, src_indices[], arg} and
        references earlier positions in the same array.  This
        is the tinygrad "pickled UOp list" shape -- the same
        representation the PYTHON device consumes.  (No
        memory planning: every intermediate gets its own
        temporary slot.  No optimization passes.)

     4. compile(program)
        Step 12 = nop for the auto path.  The CPU backend's
        "compiled kernel" is the program itself; the dispatcher
        is an interpreter.  For TCompileKernel, this is where
        cc -shared produces a .so and dlsym gives us a function
        pointer.  Kernels from both paths land as KernelEntry
        structs with a dispatch_fn.

     5. splice
        Walk expr bottom-up; replace each fused span with its
        UOP_KERNEL term.  Non-UOP content (LAMs, APPs, DUPs)
        passes through unchanged.  Return the scheduled DAG.
  */
}
```

For step 12 we pick the dumbest-but-complete fusion policy
(elementwise chains until a shape-changing op), get the whole
pipeline working end-to-end, then step 14 swaps in smarter
boundaries / memory planning / actual codegen without touching the
IC-facing interfaces.

### Linearized kernel format

Each `KernelEntry` carries:

```c
typedef struct {
  u32       n_inputs;
  u32       input_dtypes[MAX_INPUTS];
  View      input_views[MAX_INPUTS];

  u32       n_outputs;              /* = 1 in step 12 */
  u32       output_dtype;
  View      output_view;

  u32       n_ops;                  /* size of program[] */
  KProgOp   program[PROG_CAP];      /* SSA-over-indices */

  u64       sig;                    /* structural signature for cache */
  int     (*dispatch_fn)(KernelEntry *, Backend *, u32 *in_buf_ids, u32 out_buf_id);
  void     *compiled;               /* backend-specific compiled data (.so, MTLFunction, NULL for interpreter) */
} KernelEntry;

typedef struct {
  u8  opcode;                       /* UOP_ADD, UOP_MUL, ... */
  u8  dtype;
  u8  n_src;
  u32 src[MAX_UOP_SRC];             /* indices into program[] or input ids (high bit = input) */
  u32 arg;                          /* REDUCE axis, CONST bits, ... */
} KProgOp;
```

For the CPU interpreter, `dispatch_fn` is
`cpu_interpret(ke, backend, in_bufs, out_buf)`; `compiled` is
`NULL`. For a user-compiled CPU kernel, `compiled` is a handle to
the `.so` and `dispatch_fn` calls its entry-point. For Metal
(step 14), `compiled` is an `MTLFunction` and `dispatch_fn` builds
the `MTLComputeCommandEncoder`.

### Interpreter dispatch (CPU, step 12)

`src/backend/cpu/interpret.c` walks `program[]`, one op at a time,
dispatching to a per-op C function:

```c
int cpu_interpret(KernelEntry *ke, Backend *b, u32 *in_bufs, u32 out_buf) {
  /* allocate a temporary f32[] per op for intermediate results */
  void **regs = alloca(ke->n_ops * sizeof(void *));
  for (u32 i = 0; i < ke->n_ops; i++) {
    KProgOp *op = &ke->program[i];
    void   **srcs = resolve_srcs(regs, in_bufs, op);
    switch (op->opcode) {
      case UOP_ADD:   cpu_op_add  (regs[i], srcs, op);  break;
      case UOP_MUL:   cpu_op_mul  (regs[i], srcs, op);  break;
      case UOP_NEG:   cpu_op_neg  (regs[i], srcs, op);  break;
      case UOP_REDUCE: cpu_op_reduce(regs[i], srcs, op); break;
      ...
    }
  }
  /* last program entry writes into out_buf directly */
  return 0;
}
```

Each per-op function lives in its own file
(`src/backend/cpu/op/<op>.c`) per the project convention:

```c
/* src/backend/cpu/op/add.c */
void cpu_op_add(void *out, void **srcs, KProgOp *op) {
  const f32 *a = (const f32 *)srcs[0];
  const f32 *b = (const f32 *)srcs[1];
  f32       *o = (f32 *)out;
  u32        n = op_elems(op);
  for (u32 i = 0; i < n; i++) o[i] = a[i] + b[i];
}
```

This is deliberately the same shape as tinygrad's
`tinygrad/runtime/ops_python.py` — a switch over opcodes that
reads source slots and writes a destination slot. The difference
is only that we're in C with raw pointers instead of Python with
lists.

### Does tinygrad compile everything to source?

No. `tinygrad/runtime/ops_python.py` is an interpreter device:
it takes a pickled `list[tuple[Ops, DType|None, list[int], Any]]`
(the same SSA-over-indices representation we use in `program[]`)
and evaluates it directly, op by op, in Python. No compiler, no
source string.

Codegen devices (CLANG, METAL, CUDA, ...) receive the same
linearized list, then render it into backend source and compile.
Our step 12 CPU backend is the PYTHON-device equivalent: same
inputs, interpreted dispatch. Step 14 adds the codegen path.

### Why KERNEL firing is natural

Under `TWnf`, a `UOP_KERNEL` with `TAG_TEN` leaves is exactly
analogous to an `OP2` with `NUM` operands. The reducer already
knows how to descend into operands and drive them to concrete
values (WNF stack protocol). Reusing that machinery for KERNELs
means no new dispatch trigger is needed — a KERNEL is just a
"compute node whose operands are tensors instead of numbers".

## UOP_KERNEL: layout, multiple inputs, output writes

A KERNEL has **one output and N inputs** (N discovered by walking
the AST). Heap layout is fixed at 2 cells:

```
Heap[loc + 0] = output_buf      (UOP_BUFFER or TAG_TEN)
Heap[loc + 1] = ast_root        (compute subtree)
```

EXT field of the `UOP_KERNEL` term is unused (the opcode itself is
KERNEL).

**Why not put inputs as direct heap children?** Inputs are *implicit
in the AST*. Walking `ast_root` reveals every `UOP_BUFFER` /
`UOP_LOAD` leaf — those are the inputs. Encoding them twice would
mean keeping two views consistent.

**Multiple outputs.** A single KERNEL still produces one tensor;
multi-output kernels are modeled with `UOP_ASSIGN` and `UOP_SINK`
(tinygrad's pattern):

```
UOP_ASSIGN[target_buf, src_kernel]    -- pin a kernel result to a buffer
UOP_SINK  [assign_0, assign_1, ...]   -- aggregate
```

Step 12 lands the tags but only single-output is wired end-to-end.
Step 13 wires up `ASSIGN`/`SINK` for in-place updates and
multi-output graphs.

**In-place writes**: when a kernel's output is an existing
`TAG_TEN` (not a fresh `UOP_BUFFER`), the dispatcher writes into
that buffer instead of allocating a new one. Refcount is not
touched (the consumer was the producer).

## Firing kernels — modeled on `TAG_OP2 ↔ TAG_NUM`

`UOP_KERNEL` has an interaction rule that fires naturally under
`TWnf`. When the reducer enters a `UOP_KERNEL`, it uses the stack
protocol we already have for `TAG_OP2`:

- Push the `UOP_KERNEL` as a frame, descend into the first AST
  leaf that isn't yet `TAG_TEN`.
- That descent may pass through IC combinators (LAMs, APPs, DUPs)
  or into another `UOP_KERNEL`; those fire bottom-up.
- When a leaf resolves to `TAG_TEN`, pop back to the `UOP_KERNEL`
  frame, install the leaf, and check the next pending leaf.
- Once every AST leaf is `TAG_TEN`, call `interact_kernel` — which
  invokes the compiled binary via the backend and returns a fresh
  `TAG_TEN`.

How do we stop kernels firing *during* materialization? Simple:
`TMaterialize` doesn't go through `wnf`. It runs the rewrite
rule `interact_uop_materialize` directly on the term and returns
the result. Without a `wnf` descent, no `UOP_KERNEL` ever gets
visited in "enter" phase, so no firing happens.

This is the same pattern HVM4 uses for rules that need to run
without full reduction: skip the stack machinery and apply the
rewrite directly.

### Template: how HVM4's `OP2` reduces

From `/Users/swish/src/HVM4/src/wnf/_.c`:

```c
case OP2: {                        /* enter phase: descend into x */
  u64  loc = term_val(next);
  Term x   = heap_read(loc + 0);
  stack[s_pos++] = next;            /* push the OP2 term as a frame */
  next = x;
  goto enter;
}

case NUM: {                        /* apply phase: x is now NUM */
  u8 y_tag = term_tag(y);
  if (y_tag == NUM) {              /* both operands ready -> fire */
    whnf = wnf_op2_num_num_raw(opr, term_val(x), term_val(y));
    continue;
  }
  /* x is NUM, y isn't yet: push continuation frame, enter y */
  stack[s_pos++] = term_new(0, F_OP2_NUM, opr, term_val(x));
  next = y;
  goto enter;
}
```

Result of `(+ 3 5)`: a naked `term_new_num(8)` atom — no heap.

### `wnf` cases

```c
/* enter phase, in src/wnf/_.c: */
case TAG_UOP: {
  u32 op = term_ext(next);
  switch (op) {
    case UOP_MATERIALIZE:
      /* Pure rewrite; compute scheduled DAG and replace next. */
      next = interact_uop_materialize(ctx, next);
      goto enter;

    case UOP_KERNEL:
      /* Drive AST leaves to TAG_TEN, then fire. */
      stack[s_pos++] = next;
      next = kernel_next_pending_leaf(next);
      goto enter;

    default:
      /* CONST, movement ops, elementwise, REDUCE, ... all WNF. */
      whnf = next;
      goto pop;
  }
}

/* apply phase: when the top frame is a UOP_KERNEL and whnf is a
   TAG_TEN, install it into the pending leaf slot and continue
   with the next pending leaf; fire if all done. */
case F_KERNEL_AT: {
  Term   kernel  = frame_kernel(frame);
  u32    pending = frame_pending(frame);
  Term   ast     = heap_read(term_val(kernel) + 1);

  leaf_install(ast, pending, whnf);             /* whnf is TAG_TEN */
  u32 next_p = next_pending_leaf(ast);
  if (next_p != UINT32_MAX) {
    stack[s_pos++] = frame_with_pending(kernel, next_p);
    next = leaf_at(ast, next_p);
    goto enter;
  }
  /* All leaves TAG_TEN -- fire. */
  whnf = interact_kernel(ctx, kernel);
  continue;
}
```

The WL `TMaterialize` entry point does **not** call `wnf` on its
argument. It calls `interact_uop_materialize` directly and returns
the rewritten term. Because `wnf` never enters this rewritten
term, no `UOP_KERNEL` fires. That's how the intermediate scheduled
DAG stays visible.

`interact_uop_materialize(ctx, t)`:

1. Read `expr` = `Heap[term_val(t)]`.
2. Walk `expr`; find kernel boundaries (trivial: one per materialize).
3. For each kernel:
   - Compute structural signature.
   - Hit the kernel cache; compile via backend on miss.
   - Allocate a fresh `UOP_BUFFER` for the output.
   - Emit `UOP_KERNEL[output_buf, ast_root]`.
4. Splice KERNELs back into `expr`'s topology, preserving wrapping
   IC combinators (LAMs, APPs, DUPs).
5. Return the scheduled DAG term. **No kernel fires here.**

`interact_kernel(ctx, kernel)`:

1. Read `output_buf`, `ast_root`.
2. Collect leaf `TAG_TEN` ids by walking `ast_root`.
3. Resolve the compiled-kernel id (stored during materialization in
   a side table keyed by `term_val(kernel)`).
4. If `output_buf` is `UOP_BUFFER`, allocate and replace with
   `TAG_TEN`. If already `TAG_TEN`, write in-place.
5. Call `backend->dispatch_kernel(ctx, kid, leaf_ids, out_id)`.
6. Increment `ITRS` (one dispatch = one interaction).
7. Return the output `TAG_TEN`.

After a kernel fires, its frame pops and we're back in the parent
frame — either a higher `UOP_KERNEL` (whose leaf just became
`TAG_TEN`) or whichever IC term contained it. Exactly like `OP2`
reducing to `NUM` and the parent `F_OP2_NUM` resuming.

### What about other UOp kinds?

Per the user's instruction, **`TAG_UOP` is WNF for every opcode
except `UOP_KERNEL`**. So:

- `UOP_ADD`, `UOP_MUL`, `UOP_RESHAPE`, `UOP_VIEW`, `UOP_BUFFER`,
  `UOP_CONST`, etc. — all inert under WNF. They sit there as graph
  nodes until a kernelization pass (step 14) folds them into a
  `UOP_KERNEL`.
- `UOP_KERNEL` — the only opcode that has an interaction rule.
  Fires when all its `src_i` are `TAG_TEN`.

This matches HVM4's pattern exactly: most term tags are passive
data; a small number (`OP2`, `MAT`, `OPL`, ...) drive computation
by firing when their operands are concrete.

## What we cross-checked from C-ML

C-ML is a from-scratch C ML framework (`include/tensor/tensor.h`,
`include/ops/uops.h`, `include/ops/ir/internal.h`) that hits many of
the same problems but without the IC overlay. Notes on what we
adopt or reject:

**Adopt (matches our plan):**

- **Lazy creation ops are first-class.** C-ML's `UOP_CONST`,
  `UOP_RAND_UNIFORM`, `UOP_ARANGE_OP` carry data inline as op
  parameters; no separate "constant tensor" type. We do the same
  with `UOP_CONST` (atom or 1-cell heap entry, `EXT` = dtype).
- **Thin backend vtable.** C-ML's `BackendOps` is a flat struct of
  function pointers (`matmul`, `add`, `mul`, `sum`, ...); no class
  hierarchy, no `#ifdef` at call sites. Our `Backend` struct is the
  same shape, scoped to buffer ops in step 12 with compute callbacks
  added in step 14.
- **Switch on op-kind, not vtable.** C-ML dispatches compute by a
  direct `switch (uop->type)` rather than virtual calls. That's
  what `interact/uop_kernel.c` does — a `switch (term_ext(t))` on
  the opcode.
- **In-graph optimizer steps.** C-ML's `UOP_SGD_STEP`,
  `UOP_ADAM_STEP` are normal IR nodes — optimization can be folded
  into the same kernel-fusion machinery. We don't need them in
  step 12, but the design doesn't preclude them later.

**Reject (doesn't fit IC):**

- **`Tensor.ir_node` pointer.** C-ML embeds an `IRNode *` in every
  tensor so the lazy graph stays linked to its values. We don't
  need this — *the IR is the IC heap*. A `TAG_TEN` is a leaf in the
  same graph the rest of the program lives in; pointer chasing
  through a side-graph is exactly what tagged terms eliminate.
- **Hash-based IR equality.** C-ML interns IR nodes via FNV-1a hash
  for kernel caching. We get equality for free from term tags
  (`Term` is just a 64-bit integer); kernel caching uses
  `normalized_sig` only at the kernel boundary, not for every node.
- **Eager backward graph construction.** C-ML's `cml_backward()`
  walks the forward graph and builds a separate backward IR graph.
  TinyHVM's approach (UOP_GRAD as an *inline rewrite rule* in
  `interact/grad.c`, recursive peephole on each ALU kind, no tape)
  is the better fit for IC and the one we adopt.
- **`void *params` per op.** C-ML uses tagged-union-like
  `Conv2DParams`/`ReduceParams`/`ConstParams` blobs. We encode
  per-op state as heap cells laid out per opcode (HVM4 OP2 style),
  which keeps everything walkable as plain `Term`s.

**Defer (interesting but post step-12):**

- **Symbolic shapes** (`SymExpr` / `SymShape` in `src/symbolic/`) —
  shapes with named variables, constant folding, min/max bounds.
  Lets the same compiled kernel handle a range of `batch_size`. We
  ship concrete `Shape` for v1; revisit if step 14's kernel cache
  shows duplication that symbolic shapes would collapse.

## WL surface

```mathematica
(* concrete tensors *)
TTensor[shape_List, "f32"]                  (* TAG_TEN, allocates buffer *)
TTensor[shape_List, data_List]              (* TAG_TEN, with initial values *)

(* lazy graph builders -- step 12 set *)
TUOpConst[value_, "f32"]                    (* TAG_UOP UOP_CONST *)
TUOpReshape[src_, shape_List]
TUOpPermute[src_, axes_List]
TUOpExpand[src_, shape_List]
TUOpPad[src_, ranges_List]
TUOpShrink[src_, ranges_List]
TUOpFlip[src_, axes_List]
TUOpAdd[a_, b_]
TUOpMul[a_, b_]
TUOpNeg[a_]
TUOpRecip[a_]
TUOpExp2[a_]; TUOpLog2[a_]; TUOpSqrt[a_]
TUOpCmplt[a_, b_]
TUOpReduce[src_, axis_Integer, "SUM" | "MAX"]

(* materialization *)
TUOpMaterialize[expr_]                      (* wraps expr; fires under TWnf *)
TMaterialize[expr_]                         (* WL helper: runs the rewrite
                                               directly without firing kernels,
                                               returns a scheduled DAG Term   *)

(* custom kernels -- user provides source *)
TCompileKernel[src_String, sig_]            (* compile a backend source string;
                                               returns a kernel_id             *)
TUOpKernel[out_, inputs_List, kernelId_]    (* build a UOP_KERNEL that fires
                                               the custom compiled kernel     *)

(* inspection *)
TTensorShape[t_]
TTensorDType[t_]
TTensorRefcount[t_]
TTensorData[t_]                             (* reads host_ptr into a WL list *)

TUOpKind[u_]                                (* opcode name string *)
TUOpSrcs[u_]                                (* list of source Terms *)
```

`TUOpKernel` is a public constructor here because users building
custom kernels need it. Materialization produces KERNELs through
the same code path.

### Custom kernels in WL

A user writes a kernel in the backend's source language (C for CPU,
Metal Shading Language for Metal), compiles it once, and applies
it like any other op:

```mathematica
src = "
void add_kernel(const float *a, const float *b, float *out, int n) {
  for (int i = 0; i < n; i++) out[i] = a[i] + b[i];
}
";

kid = TCompileKernel[src, <|
  "entry"   -> "add_kernel",
  "inputs"  -> {{2}, "f32"} -> {{2}, "f32"},   (* shape, dtype per input *)
  "output"  -> {{2}, "f32"}
|>];

a = TTensor[{2}, {1.0, 2.0}];
b = TTensor[{2}, {3.0, 4.0}];
k = TUOpKernel[TUOpBuffer[{2}, "f32"], {a, b}, kid];

out = TWnf[k];                              (* fires -- a, b are TAG_TEN *)
TTensorData[out]                             (* {4.0, 6.0} *)
```

This bypasses materialization entirely — useful for hand-tuned
kernels, MPS / vendor libraries, or just quick experiments. The
same `UOP_KERNEL` interaction rule fires it; `TCompileKernel` just
installs a `KernelEntry` whose `dispatch_fn` is a pointer to the
user's compiled function instead of a generated one.

## Graph visualization

Reuse `THeapGraph` and `THeapDiagram`:

- `TAG_TEN`  — square (atom shape, distinct from triangles).
- `TAG_UOP` — rectangle, label = opcode name + base.

Wires from a UOp to its sources follow the existing `agentPorts`
edge logic; each opcode gets an entry in `agentPorts[$TagUOP]`
keyed by opcode arity.

## Step 12 deliverables — minimal e2e

The scope is **the whole vertical slice**, not just scaffolding: a
program that builds a UOp graph, calls `TWnf` once, and gets back a
materialized `TAG_TEN`. Keep it tiny but end-to-end.

1. **Tags + structs.** `TAG_TEN`, `TAG_UOP` in `src/thvm.h`. UOp
   opcode constants. `Shape`, `View`, `ShapeTracker`, `TenDesc`,
   `Backend` structs.
2. **Side tables.** `TENS[]` for `TenDesc`s; one `KernelEntry[]`
   for compiled kernels (CAM-keyed by structural sig).
3. **`src/tensor/`.** `alloc.c`, `release.c`, `incref.c`,
   `decref.c`, `view_of.c`, plus
   `view/{create,reshape,permute,expand,pad,shrink,broadcast,stride}.c`
   adapted from TinyHVM.
4. **CPU `Backend`.** Buffer ops in `src/backend/cpu/{init,buf_alloc,buf_free,buf_incref,buf_decref,buf_read,buf_write}.c`.
   Interpreter in `src/backend/cpu/interpret.c` — walks a
   linearized `KernelEntry.program[]`, resolving sources and
   dispatching each op to a per-op C function.
   Per-op files in `src/backend/cpu/op/<op>.c` (one file each):
   `const`, `add`, `mul`, `neg`, `recip`, `exp2`, `log2`, `sqrt`,
   `cmplt`, `reduce`, `reshape`, `permute`, `expand`, `pad`,
   `shrink`, `flip`. No codegen yet; this is our PYTHON-device
   equivalent.
5. **Refcount hooks.** `src/interact/dup_ten.c`, `src/interact/era_ten.c`.
6. **The two interaction rules.**
   - `src/interact/uop_materialize.c` — `interact_uop_materialize`.
     Rewrites `expr` into the scheduled DAG (kernelize + compile).
     Returns the rewritten term; **fires no kernels**. Callable as
     a rule by the WNF reducer (reached via `UOP_MATERIALIZE` term)
     *and* directly as a C helper from the `TMaterialize` WL entry
     point.
   - `src/interact/uop_kernel.c` — `interact_kernel`. Fires a
     kernel (dispatches via backend) once all its AST leaves are
     `TAG_TEN`. Driven by the existing WNF stack protocol; no
     special wrapper required.
7. **`src/schedule/`.** Five files driven by
   `src/schedule/materialize.c`:
   - `schedule.c` — pick fusion boundaries (elementwise chains;
     REDUCE / movement / user-KERNEL terminate).
   - `kernelize.c` — for each span, allocate an output
     `UOP_BUFFER` and emit the `UOP_KERNEL` term.
   - `linearize.c` — flatten each kernel's AST into an SSA
     `KProgOp` list; stash it in a `KernelEntry`.
   - `compile.c` — nop for the auto path (interpreter); cc / dlsym
     for user-provided sources via `TCompileKernel`.
   - `splice.c` — replace each fused span with its `UOP_KERNEL`
     term, preserving surrounding IC combinators.
   No memory planning; no optimization passes; every intermediate
   gets its own temporary slot. Smarter boundaries + planning are
   step-14 swaps, no interface churn.
8. **`src/wnf/_.c` extension.** Dispatch on `TAG_UOP` opcode:
   - `UOP_MATERIALIZE`: fire the rewrite immediately on enter.
   - `UOP_KERNEL`: push a frame and descend into pending AST leaves;
     `interact_kernel` once all leaves are `TAG_TEN`.
   - Every other opcode (`UOP_ADD`, `UOP_CONST`, movement, ...):
     inert (WNF).
9. **WL surface.** `TTensor`, `TUOpConst`, `TUOpReshape`,
   `TUOpPermute`, `TUOpExpand`, `TUOpPad`, `TUOpShrink`, `TUOpFlip`,
   `TUOpAdd`, `TUOpMul`, `TUOpNeg`, `TUOpRecip`, `TUOpExp2`,
   `TUOpLog2`, `TUOpSqrt`, `TUOpCmplt`, `TUOpReduce`,
   **`TUOpMaterialize`**, **`TMaterialize`** (WL helper that runs
   the rewrite without firing), **`TCompileKernel`**,
   **`TUOpKernel`**, plus inspection helpers (`TTensorShape`,
   `TUOpKind`, `TUOpSrcs`, ...). `TUOpBuffer` (as a user-facing
   constructor) / `TUOpLoad` / `TUOpView` / `TUOpAssign` /
   `TUOpSink` are *not* exposed yet.
10. **Heap-graph styling** for `TAG_TEN` (square) and `TAG_UOP`
    (rectangle, label = opcode name).
11. **End-to-end tests.**

    **Auto-kernelized path:**
    ```mathematica
    a    = TTensor[{2}, {1.0, 2.0}];    (* TAG_TEN *)
    b    = TTensor[{2}, {3.0, 4.0}];
    expr = TUOpAdd[a, b];               (* raw UOp *)
    mat  = TMaterialize[expr];          (* scheduled DAG, not fired *)
    THeapDiagram[mat]                    (* KERNEL visible here *)
    out  = TWnf[mat];                    (* fires naturally *)
    TTensorData[out]                     (* {4.0, 6.0} *)
    ```

    **Custom-kernel path:**
    ```mathematica
    kid = TCompileKernel["void k(float *a, float *b, float *c, int n){ for(int i=0;i<n;i++) c[i]=a[i]+b[i]; }",
                         <|"entry"->"k", "inputs"->{{{2},"f32"},{{2},"f32"}}, "output"->{{2},"f32"}|>];
    k   = TUOpKernel[TUOpBuffer[{2},"f32"], {a, b}, kid];
    out = TWnf[k];
    ```

    Both paths exercise `interact_kernel`; only the kernel-entry
    source differs (auto-walked AST vs user binary).

    `TAG_TEN` appears directly as a source in `UOP_ADD`. There is
    no `UOP_LOAD` wrapper in step 12 — load semantics arrive when
    step 14 introduces explicit index computation in flattened
    kernel ASTs.
12. **`wl/Examples/tensor-add/`** with rendered diagram (the same
    pipeline in the example database).

`UOP_GRAD`, `UOP_ASSIGN`, `UOP_SINK`, multi-output, fusion across
realizes, Metal backend → step 13/14.

## Known design issues

Things this design is *not* well-resolved on. Most are safely
deferred to step 13/14, but they're load-bearing decisions and
worth flagging before code lands.

### Architectural

1. **`TMaterialize` doesn't trigger IC reduction first.** If `expr`
   still has pending IC redexes — e.g. a `LAM` that hasn't met its
   `APP` yet, or a `DUP` that hasn't fired — then `TMaterialize`
   sees the un-reduced form and can't determine kernel boundaries
   or shapes. Two workable patterns:
   - User calls `TWnf[expr]` to beat the raw graph to IC-WNF
     first, *then* `TMaterialize`, then `TWnf` again to fire.
   - User wraps in `TUOpMaterialize` and calls `TWnf` once; the
     reducer gets to IC-WNF as a pre-pass, hits the
     `UOP_MATERIALIZE` rule, rewrites, then fires kernels. This
     is the intended primary path; `TMaterialize` is the
     inspection helper.
   Document both; make the one-call path the default in tests.

2. **Custom `TUOpKernel` mixed with `TUOpMaterialize`.** If a user
   graph has a hand-built `UOP_KERNEL` nested inside a subgraph
   that `TUOpMaterialize` then tries to kernelize, what does
   materialize do with the manually-placed KERNEL? Treat it as an
   opaque leaf (and schedule *around* it, feeding its output as an
   input to other kernels)? Or try to fuse its AST in? Step 12
   picks **opaque**: a user-built KERNEL is a fixed node that
   materialize flows into/out of but does not descend into. Step
   14 revisits when fusion is real.

3. **Sharing requires DUP, but tensor compute graphs share
   freely.** IC is linear; `t * t` is not linear in `t`. Step 12
   does not exercise sharing. Three options when we do:
   - (a) Require explicit `TDup` at the WL layer. IC-correct,
     ergonomically painful.
   - (b) Auto-insert DUPs in the WL surface when a Term appears
     more than once. Hides linearity.
   - (c) `TAG_TEN` (and possibly `TAG_UOP`) skip DUP via a
     refcount-only sharing rule. Already half-true: `DUP` on
     `TAG_TEN` bumps the buffer refcount instead of actually
     duplicating.
   No decision yet; the step-12 e2e test stays linear until we pick.

4. **Multi-realize coordination.** Nested `UOP_MATERIALIZE` wrappers
   compose by natural reduction order (inner fires first, outer
   sees its result). *Sibling* materializations that share a
   subexpression currently compile + run the shared kernel twice.
   Step 14's kernel cache eliminates the redundant compile; step 14
   fusion eliminates the redundant run.

5. **Materialize rewrites into fresh heap cells; the raw subgraph
   leaks.** `interact_uop_materialize` allocates new cells for the
   scheduled DAG and returns a term pointing at them. The original
   raw-UOp cells are unreachable but still occupy `HEAP_NEXT`.
   Matches HVM4's "no GC; eraser absorbs" philosophy (see
   [heap.md](heap.md)), but if a user re-materializes the same
   `expr` twice (e.g. during debugging), the heap grows. Benign for
   step 12; step 14 either adds heap compaction or makes
   `UOP_MATERIALIZE` idempotent by recognising already-scheduled
   subgraphs.

### Concrete v1 simplifications

6. **Single-output `UOP_KERNEL`.** Heap layout is fixed at
   `[output_buf, ast_root]`. Multi-output kernels (tinygrad's
   `SINK` of `ASSIGN`s) need a variable-arity layout
   `[ast_root, out_0, out_1, ...]` with count in `EXT`. Step 12
   ships single-output only; step 13 widens when `ASSIGN` / `SINK`
   come online.

7. **`UOP_REDUCE` axis is a single integer.** Tinygrad's
   `REDUCE_AXIS` carries a tuple of axes. Step 12 packs
   `(kind << 8) | axis` into `EXT`, handling one axis at a time;
   multi-axis reduce becomes a chain of reduces. Cleanup when
   step 14's renderer wants the canonical form.

8. **No symbolic shapes.** Shape entries are `u32` constants; there
   is no `SymExpr`. Means materialize emits a distinct kernel
   signature for every concrete shape — acceptable for the step-12
   test surface, forces a refactor once the kernel cache starts
   carrying the weight in step 14.

9. **Interpreter allocates a temporary per `KProgOp`.** Step 12
   emits one host buffer per program entry (no reuse). Correct but
   wasteful — an `ADD` followed by a `MUL` using the same shape
   could share a temporary. Fixed in step 14's memory planning
   pass, which rewrites `KernelEntry.program[]` with reused
   register slots before dispatch.

### Bookkeeping

10. **`TENS[]` freelist not specced.** `TENS_NEXT` bumps on alloc;
    `tensor_release` decrements refcount and frees the backend
    buffer when it hits zero, but does *not* return the descriptor
    slot. Step 12 is bump-only; step 13 adds a freelist when we
    measure the leak in tests.

11. **Kernel cache eviction.** `KernelEntry[]` grows unbounded.
    Step 12 plans a bump allocator keyed by structural signature;
    hit-bounded LRU is step-14 territory. Not a problem until
    someone makes a test that builds many distinct-shape kernels.

12. **`UOP_BUFFER` lifecycle for materialize outputs.** When
    `interact_uop_materialize` allocates a fresh `UOP_BUFFER` for
    a kernel's output, its `TenDesc.refcount` starts at 0 (it's a
    promise, nothing's holding it yet). When the kernel fires, the
    slot gets replaced by `TAG_TEN` (refcount = 1). If firing never
    happens — e.g. the materialized term is discarded — the
    descriptor slot leaks. Linked to issue #10.

13. **Pending-leaf tracking is O(AST-size) per step.** The kernel
    firing rule walks the AST to find the next non-`TAG_TEN` leaf
    each time a leaf resolves. Fine for small ASTs (step 12's e2e
    has ~3 nodes); a side-table on `KernelEntry` listing leaf
    locations is the obvious faster approach and lands when profiles
    warrant it.

## Settled decisions

Locked in before step 12 code starts:

1. **Fusion on.** Materialize produces multiple `UOP_KERNEL`
   nodes, fused along elementwise chains; `REDUCE`, movement ops,
   and user-built `UOP_KERNEL`s are boundaries. One fused span per
   output.

2. **Whole pipeline wired.** Schedule → kernelize → linearize →
   compile (nop for auto path) → splice, all in
   `src/schedule/`. Memory planning and optimization are nops for
   v1; interfaces are in place so step 14 replaces the nops without
   touching IC code.

3. **Interpreter dispatch.** CPU backend is the PYTHON-device
   equivalent: `KernelEntry.program[]` is walked by
   `cpu_interpret`. No codegen. User-supplied sources through
   `TCompileKernel` go through `cc -shared` / `dlsym`; both paths
   end up with a `KernelEntry.dispatch_fn` pointer, so firing is
   uniform.

4. **Custom-kernel shape check at construct time.** `TUOpKernel[out, ins, kid]`
   validates input shapes/dtypes against the `KernelEntry.sig`;
   mismatches fail before `TWnf` rather than inside it.

5. **Source language backend-implicit.** `TCompileKernel[src, sig]`
   uses the current backend's language (C for CPU; MSL for Metal
   when it lands). No explicit `"language"` field for v1; revisit
   if multi-language per-backend becomes necessary.

6. **Per-op files.** Each UOp's CPU implementation lives in
   `src/backend/cpu/op/<op>.c` (e.g. `cpu_op_add` in `add.c`).
   Matches the `file = function name` convention already used
   across `src/interact/`, `src/heap/`, `src/term/`.
