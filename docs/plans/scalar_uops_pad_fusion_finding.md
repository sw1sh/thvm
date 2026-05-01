# PAD-after-RESHAPE_V fusion: why naive matching fails

## TL;DR

Spot-fusion of UOP_PAD into upstream S_RESHAPE_V (commit reverted, never
landed) produced wrong values on conv2d-helper-1ch-2outch-2x2-kernel:
got `{{{4.5, 8.5}, {16.5, 20.5}}, ...}` expected `{{{12.5, 16.5},
{24.5, 28.5}}, ...}`.

Root cause: extent-equality matching is ambiguous when the same dim
appears multiple times across `os.dims` and `reduce_size`.  Need full
rngs[] tracking to know *which* axis is the reduce axis.

## Failing kernel chain

For a 1ch×2outch 2×2 conv on a 3×3 input, the materializer emits
this 15-op kernel (output_numel=8 = [2, 4]):

```
op[0]  RESHAPE input[1] [1,2,2] -> [1,1,4]    -- weight tile kw=0
op[1]  PAD     op[0]    [1,1,4] -> [1,4,4] pad=(0/0, 0/3, 0/0)
op[2]  RESHAPE input[2] [1,2,2] -> [1,1,4]    -- weight tile kw=1
op[3]  PAD     op[2]    [1,1,4] -> [1,4,4] pad=(0/0, 1/2, 0/0)
op[4]  ADD     op[1] + op[3]    [1,4,4]
op[5]  RESHAPE input[3] [1,2,2] -> [1,1,4]    -- weight tile kw=2
op[6]  PAD     op[5]    [1,1,4] -> [1,4,4] pad=(0/0, 2/1, 0/0)
op[7]  ADD     op[4] + op[6]    [1,4,4]
op[8]  RESHAPE input[4] [1,2,2] -> [1,1,4]    -- weight tile kw=3
op[9]  PAD     op[8]    [1,1,4] -> [1,4,4] pad=(0/0, 3/0, 0/0)
op[10] ADD     op[7] + op[9]    [1,4,4]    -- summed weights
op[11] RESHAPE op[10]   [1,4,4] -> [1,4,4]    -- identity
op[12] EXPAND  op[11]   [1,4,4] -> [2,4,4]    -- broadcast oc
op[13] MUL     input[0] * op[12]  [2,4,4]
op[14] SUM_REDUCE op[13]  numel 32 -> 8 (= [2,4])
```

Trick: each PAD places one weight tile at a different position along
axis 1 (begin=0,1,2,3); the ADDs sum them into a layout where axis 1
is "kernel position".  After EXPAND broadcasts `oc`, MUL applies, and
the final SUM_REDUCE collapses axis 1 (the kernel positions).

## The matching ambiguity

PAD op[1] has `out_dims=[1,1,4]`.  Kernel `os.dims=[2,4]`.  Reduce_size
= 4.  Two extent-4 candidates for the size-4 axis 2:
- `loop_ranges[1]` (extent 4, the OW dim of output)
- `reduce_range`   (extent 4, the kernel-position axis getting reduced)

The current "match by extent equality, greedy first-fit" picks
`loop_ranges[1]`.  But axis 2 of the PAD's intermediate is *not* the OW
dim — it's the spatial position within the receptive field.  The
correct match for axis 2 is `loop_ranges[1]` (W within the input);
axis 1 of the PAD is the kernel-position axis (gets reduced).

But my matching code doesn't know about `reduce_range`, so it wastes
both LOOP iters on the wrong axes and falls back to size-1 VIRT
placeholders for unmatched non-1 dims — which is silently wrong
because the size-1 placeholder iter is always 0, so the bounds check
"orig < begin + src_dim" always passes.  Every padded position reads
the source instead of zeroing.

## The proper fix

Track `rngs[i]` (per-axis iter expressions) per `prog_value[i]` as
the materializer walks ops.  Each ALU/movement op updates `rngs[i]`
based on its source(s).  REDUCE consumes one specific axis (and
maps its iter to `reduce_range`).  When the SHRINK/PAD then needs
to address its source's axes, it reads the source's `rngs` to know
which iter drives each axis — no ambiguous extent-matching.

This is the tinygrad approach (`apply_movement_op` in
`indexing.py:128-145`), where `rngs` is an arbitrary symbolic
expression tuple that flows through the graph by simple
substitution rules.

The S_I* + S_INDEX_E infrastructure (committed in e271003) is the
right primitives for this -- the missing piece is the per-prog_value
rngs tracking + a per-op rule for how each KProgOp updates rngs.

Implementation sketch:

```c
// In rangeify_try_lower_elementwise:
typedef struct {
  u32 ndim;
  u32 refs[MAX_DIM];   // op_id of integer expressions
} RngsCtx;
RngsCtx rngs[nops_local];

// Initialize rngs for input loads
//   rngs[input_load_post[i]] = {os->ndim, [loop_ranges[d] for d ...]}
//   rngs[input_load_pre [i]] = {os->ndim+1, [loop_ranges..., reduce_range]}

// Per op:
switch (p->opcode) {
  case UOP_SHRINK:  // rngs[i] = rngs[src] with begin added per axis
  case UOP_PAD:     // similar but with bounds wrap via S_IWHERE
  case UOP_FLIP:    // negate per axis
  case UOP_PERMUTE: // reorder
  case UOP_EXPAND:  // const(0) for broadcast axes
  case UOP_RESHAPE: // flat-roundtrip via S_IDIV/S_IMOD
  case UOP_REDUCE:  // collapse one axis (becomes reduce_range)
  default:          // ALU: copy from src
}

// At input LOAD time: build addr from current rngs via S_INDEX_E.
```

## Estimate

~300-500 LOC of structural changes across `rangeify.c`.  Several
turns of careful work + grid validation.  Non-trivial because the
forward-walking model needs careful handling for ops whose source
shape differs from output shape (the place where rank-mismatch RESHAPE
would re-key rngs).

## Status

- e271003 `S_I*` arithmetic + `S_INDEX_E` -- the IR primitives exist.
- 8394f83 pre-INDEX rank-N uses `S_INDEX_E` -- proof of life.
- d2d4862 SHRINK fusion (works on its narrow case) + S_RESHAPE_V
  output-side accepts iter expressions.
- (this finding) PAD fusion via the same approach is unsafe due to
  the matching ambiguity.

Next session: implement rngs[] per `prog_value` tracking.
