// thvm.h - public surface of the thvm runtime
//
// Single-TU build: every .c file under src/ is #included by src/thvm.c
// in dependency order. Tests #include "../src/thvm.c" directly.
//
// Conventions:
//   - File path = function name. `wnf/_.c` defines wnf().
//                                `interact/app_lam.c` defines
//                                interact_app_lam().
//   - `fn` macro = static inline. Used on every internal function.

#ifndef THVM_H
#define THVM_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// === Types ===
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;
typedef float    f32;
typedef double   f64;

typedef u64 Term;

#define fn static inline

// === Term bit layout ===
//
//   bit  63        62..56     55..38         37..0
//        [SUB:1]   [TAG:7]    [EXT:18]       [VAL:38]
//
//   SUB = substitution flag - when 1, the heap cell is the *value* a
//         variable was substituted to (read by VAR/DP0/DP1 enter rules).
//   TAG = term type (128 max).
//   EXT = label / opcode / arity. Per-tag meaning.
//   VAL = heap location (38 bits ≈ 256 GiB worth of u64 cells).

#define SUB_BITS  1
#define TAG_BITS  7
#define EXT_BITS  18
#define VAL_BITS  38

#define SUB_SHIFT 63
#define TAG_SHIFT 56
#define EXT_SHIFT 38
#define VAL_SHIFT 0

#define SUB_MASK  0x1ULL
#define TAG_MASK  0x7FULL
#define EXT_MASK  0x3FFFFULL

// === DUP-cell flavor ===
// TAG_DP0/DP1 reuse the dup-cell mechanism for two distinct cell
// types.  Bit 17 of the ext (the label) marks the projection as
// "grad-flavored" -- the cell holds [y] and the projections do
// FWD/BWD instead of HVM-style DUP-X dispatch.  The remaining 17
// bits of ext are the dup label (matched against SUP^L for projection
// in the regular case; unused / target-tid by convention in the grad
// case).
#define DUP_GRAD_FLAG  (1U << 17)
#define DUP_LABEL_MASK (0x1FFFFU)            // bits 0..16

// === LAM ext flags ===
// Bit 17 of a TAG_LAM's ext (separate namespace from DP0/DP1's ext)
// marks "binder unused in body".  When set, APP-LAM and DUP-LAM skip
// the heap_subst_var step (no VAR -> binder cell exists, so the
// substitution would dangle).  Set by lam_body_uses_var at every
// LAM-construction site.  Mirrors HVM4's LAM_ERA_MASK (hvm.c:102).
#define LAM_ERA_MASK   (1U << 17)
#define VAL_MASK  0x3FFFFFFFFFULL

// === Tags (minimal initial set) ===
// Order chosen so the hot interaction tags sit in the low byte.

#define TAG_APP  0   // (f x).             Heap[loc..loc+1] = [f, x]
#define TAG_LAM  1   // λx.body.           Heap[loc]        = body; var subst at loc
#define TAG_VAR  2   // bound variable.    val = binder loc
#define TAG_ERA  3   // erased.            no heap
#define TAG_DP0  4   // dup projection 0.  val = dup loc, ext = label
#define TAG_DP1  5   // dup projection 1.  val = dup loc, ext = label
#define TAG_SUP  6   // {a, b}.            Heap[loc..loc+1] = [a, b]; ext = label
#define TAG_DUP  7   // ! &L{x0,x1} = e; … val = dup loc; Heap[loc] = e

// === Tensor tags (PLAN.md step 12) ===
// See docs/tensors.md for the full design.

#define TAG_TEN  8   // tensor handle.    atom; val = tensor id, ext = dtype
#define TAG_UOP  9   // uop graph node.   val = heap loc, ext = opcode
#define TAG_NUM  10  // 32-bit scalar.    atom; val = raw u32 / f32 bits, ext = dtype

// === Lazy named definitions + allocator (HVM4 / TinyHVM style) ===
#define TAG_REF  11  // book reference.   val = name id (index into DEFS)
#define TAG_ALO  12  // lazy alloc.       val = dyn heap loc holding [book_term, NUM(state_id)]

// === Numeric switch + binary op (Phase-2 termination + counter) ===
#define TAG_OP2  13  // binary op.        val = heap loc -> [x, y], ext = opcode (OP_*)
#define TAG_MAT  14  // numeric switch.   val = heap loc -> [handler, fallback], ext = match value

// === IC-as-ATP layer (PLAN: docs/plans/waldmeister_ic_atp.md) ===
#define TAG_EQL  15  // structural eq.    val = heap loc -> [a, b]; strict both sides
#define TAG_AND  16  // short-circuit AND.val = heap loc -> [a, b]; strict on a, lazy on b
#define TAG_OR   17  // short-circuit OR. val = heap loc -> [a, b]; strict on a, lazy on b
#define TAG_ANY  18  // wildcard.         atom; matches anything under EQL, dups to itself
#define TAG_INC  19  // priority wrapper. val = heap loc -> [body]; observed by collapse_ordered
#define TAG_CTR  20  // labelled constructor (HVM4 CTR).
                     //   val = heap loc -> [NUM(arity), c_0, ..., c_{n-1}],
                     //   ext = constructor label (0 = anonymous tuple).
                     //   Passive in the IC reducer (no DUP-CTR / ERA-CTR
                     //   yet -- added when an IC consumer needs them); used
                     //   today by k0c's multi-target interact_grad to
                     //   bundle one cotangent Term per requires_grad target.
#define TAG_WHEN 21  // boolean filter.   val = heap loc -> [cond, body]; truthy -> body, 0/ERA -> ERA
#define TAG_FVR  22  // first-order variable (FOL). atom; ext = variable id.
                     //   Distinct from TAG_VAR (the IC's bound variable, tied
                     //   to a binder loc).  Used to encode the universally /
                     //   existentially quantified variables of equational
                     //   logic: f(x, e) = x has FVR(x_id) at the leaves where
                     //   x appears.  Atomic: no heap cells.
#define TAG_BRI  23  // Bridge (ICC Val: θx.body, dual of LAM).  Heap[loc] =
                     //   body; bound x = VAR(loc) (same convention as LAM).
                     //   Reduces under APP via the ICC type-backward rule:
                     //     APP (θx.body) arg = θx (APP body[x <- λ$k.x] ANN($k, arg))
#define TAG_ANN  24  // Annotation {val : typ}.  Heap[loc..loc+1] = [val, typ].
                     //   Reduces by inspecting typ:
                     //     ANN val (λx.body) = λx ANN(APP val $k) body[x <- θ$k.x]
                     //     ANN val (θx.body) = body[x <- val]   (type erasure)
                     //     ANN val var       = stuck
#define TAG_PRI  25  // Primitive function call (HVM4-style).  ext = prim_id.
                     //   val = 0 for a fresh (zero-arg) PRI, else heap_loc
                     //   to an accumulator cell [NUM(count), arg_0, ...].
                     //   APP-PRI accumulates args; once count == arity, the
                     //   registered C function is called with the args and
                     //   its return Term replaces the redex.  Stage 8.1b.

// === Frame-only tags (HVM4 alignment) ===
// These tags only ever appear on the wnf eliminator stack -- never
// in user-facing terms.  HVM4 uses them as intermediate frames for
// strict eliminators that need to drive multiple slots in sequence:
// after the first strict slot reduces to a value worth keeping
// (NUM, etc.), push one of these frames with that value baked in
// and descend into the next slot.
//
//   TAG_F_OP2_NUM   ext = OP_*       val = x's NUM raw bits (32 bits)
//                                          + dtype in upper 6 bits.
//                   x has been reduced to NUM; descending into y.
//   TAG_F_EQL_R     ext = 0          val = EQL cell loc; a's WHNF
//                   stored back in heap[loc + 0].  Descending into b.
//   TAG_F_UOP_CHILD ext = child_idx  val = uop loc.  Inside wnf's
//                   UOP-WHNF descent: this child slot was active
//                   (DP1_GRAD / DUP-projection / nested ASSIGN /
//                   nested KERNEL) and was driven via stack-frame
//                   recursion.  Apply phase heap_sets the resolved
//                   value back in place, then scans for the next
//                   active sibling; when none remain, the UOP
//                   itself is WHNF.
#define TAG_F_OP2_NUM   26
#define TAG_F_EQL_R     27
#define TAG_F_UOP_CHILD 28

#define TAG_COUNT 29

// === OP2 opcodes (TAG_OP2 ext field) ===
#define OP_ADD  0
#define OP_SUB  1
#define OP_MUL  2
#define OP_EQ   3   // returns NUM(1) for equal, NUM(0) otherwise
#define OP_LT   4   // less-than: NUM(1) if x<y else NUM(0)

// === Dtypes ===
//
// Mirrors tinygrad's full dtype set (TinyHVM/tinygrad/tinygrad/dtype.py:
// 130-146) plus packed int4/uint4 for modern quantization.  Enum
// values are stable; new dtypes append at the bottom.  Phase A
// reserves every slot but only wires DT_FP32 and DT_INT32; subsequent
// phases activate the remaining rows.  See src/dtype/info.c for the
// metadata table (itemsize / kind / signed-ness / canonical name).
//
// IDs intentionally fit a 6-bit ext field; static-asserted in
// src/dtype/info.c.
#define DT_BOOL      0
#define DT_INT8      1
#define DT_UINT8     2
#define DT_INT16     3
#define DT_UINT16    4
#define DT_INT32     5
#define DT_UINT32    6
#define DT_INT64     7
#define DT_UINT64    8
#define DT_FP8E4M3   9
#define DT_FP8E5M2  10
#define DT_FP16     11
#define DT_BF16     12
#define DT_FP32     13
#define DT_FP64     14
#define DT_INT4     15   // packed nibble (signed)
#define DT_UINT4    16   // packed nibble (unsigned)
#define DT_COUNT    17

// Family kinds carried in DTypeInfo.kind.  Used by predicates
// (dtype_is_int / dtype_is_float / ...) and by future kernel dispatch.
typedef enum {
    DK_RESERVED = 0,
    DK_BOOL,
    DK_SINT,    // signed int8/16/32/64
    DK_UINT,    // unsigned int8/16/32/64
    DK_FLOAT,   // f32 / f64 (native ALU)
    DK_FP16,    // ieee754 half (promote-to-fp32 ALU)
    DK_BF16,    // bfloat16    (promote-to-fp32 ALU)
    DK_FP8,     // fp8e4m3 / fp8e5m2 (promote-to-fp32 ALU)
    DK_INT4,    // signed nibble  (eager unpack-to-int8)
    DK_UINT4    // unsigned nibble
} DTypeKind;

typedef struct {
    u8          itemsize;       // bytes per element; 0 for packed/reserved
    u8          kind;           // DK_*
    u8          bits;           // bits per element (4 for nibble)
    u8          is_signed;      // 1 for signed integer/float, 0 otherwise
    char const *name;           // canonical short name ("f32", "i8", ...)
} DTypeInfo;

// Per-dtype metadata accessor.  Returns NULL on out-of-range; the
// table itself lives in src/dtype/info.c.
DTypeInfo const *dtype_info(u32 dt);
u32             dtype_itemsize    (u32 dt);
u64             dtype_storage_bytes(u32 dt, u64 numel);
char const     *dtype_name        (u32 dt);
u8              dtype_kind        (u32 dt);
int             dtype_is_float    (u32 dt);
int             dtype_is_int      (u32 dt);
int             dtype_is_signed   (u32 dt);
int             dtype_is_bool     (u32 dt);
int             dtype_is_packed   (u32 dt);

// === UOp opcodes (TAG_UOP ext field) ===
// See docs/tensors.md for per-opcode heap layouts.

// slot 0 reserved.
#define UOP_KERNEL       1   // heap = [output_buf, ast_root]; ext bits: see uop_kernel.c
#define UOP_CONST        2   // heap = [NUM(bits)]; ext = dtype
#define UOP_RESHAPE      3   // heap = [src, NUM(ndim), NUM(d0), ..., NUM(d_{n-1})]
#define UOP_PERMUTE      4   // heap = [src, NUM(ndim), NUM(p0), ...]
#define UOP_EXPAND       5   // heap = [src, NUM(ndim), NUM(d0), ...]
#define UOP_PAD          6   // heap = [src, NUM(ndim), NUM(b0), NUM(e0), ...]
#define UOP_SHRINK       7   // heap = [src, NUM(ndim), NUM(b0), NUM(e0), ...]
#define UOP_FLIP         8   // heap = [src, NUM(axes_bitmask)]
#define UOP_ADD          9   // heap = [a, b]
#define UOP_MUL         10   // heap = [a, b]
#define UOP_NEG         11   // heap = [src]
#define UOP_RECIP       12   // heap = [src]
#define UOP_EXP2        13   // heap = [src]
#define UOP_LOG2        14   // heap = [src]
#define UOP_SQRT        15   // heap = [src]
#define UOP_CMPLT       16   // heap = [a, b]
#define UOP_REDUCE      17   // heap = [src, NUM(kind), NUM(axis)]
// (slots 18, 19 were UOP_GRAD / UOP_FWD -- folded into TAG_DP0 /
// TAG_DP1 with the DUP_GRAD_FLAG bit set on the ext (label) field.
// A grad cell is just a regular dup-style cell holding [y]; its two
// aux ports are TAG_DP0 (forward projection: passthrough to y) and
// TAG_DP1 (backward projection: chain-rule rewrite via interact_grad).
// The DUP_GRAD_FLAG distinguishes "grad-flavored" cell projections
// from regular DUP/SUP projections in TAG_DP{0,1}'s redex dispatch.
// See src/uop/grad.c + src/wnf/redex.c.)
#define UOP_CMPEQ       20   // heap = [a, b]; mask of (a == b), 0/1 floats
#define UOP_LOAD        21   // heap = [src]; explicit "read this tensor" boundary
                             //   marker (mirrors tinygrad's UOps.LOAD).  Slot
                             //   reserved -- constructor + materializer land in
                             //   sub-item (b); see TASKS.md UOP_LOAD arc.
#define UOP_ASSIGN      22   // heap = [dst, src]; in-place buffer write.
                             //   Both children must reduce to TAG_TEN.  Wnf-fired
                             //   (interact_assign) -- copies src.buf -> dst.buf
                             //   and rewrites the redex to the dst Term.  Mirrors
                             //   tinygrad's UOps.ASSIGN.  Materialize bails so
                             //   the UOp survives until both children are TENs.
// (slot 23 was UOP_CTR_AT -- removed; CTR destructuring now goes
// through APP-MAT-CTR per HVM4 idiom, no dedicated projection opcode
// needed.  See TGradMany in wl/THVMLink/Kernel/Tensor.wl.)
#define UOP_CAST        23   // heap = [src, NUM(dst_dtype)].
                             //   Value-preserving cast (sint/uint/float).
                             //   tinygrad's Ops.CAST -- backward rule:
                             //   gy.cast(src.dtype).
#define UOP_BITCAST     24   // heap = [src, NUM(dst_dtype)].
                             //   Bit-level reinterpret; src and dst must
                             //   share itemsize.  tinygrad's Ops.BITCAST
                             //   -- backward returns CONST(0).
#define UOP_COUNT       25

// REDUCE kinds packed into the high bits of UOP_REDUCE's EXT field.
#define REDUCE_SUM   0
#define REDUCE_MAX   1

// === Capacities ===
#define HEAP_CAP     (1ULL << 28)   // 256M cells * 8B = 2 GiB.  Cheney splits in half (1 GiB per semi-space).  Bumped from 1<<26 so the HVM bench fib_nat (~1.2B cells over its tree) fits without GC.
#define WNF_CAP      (1ULL << 16)   // 64K stack slots.
#define TENS_CAP     (1ULL << 20)   // 1M tensor descriptor slots.
#define KERNELS_CAP  (1ULL << 18)   // 256K compiled kernels.
#define BOOK_CAP     (1ULL << 18)   // 256K cells of static def template heap.
#define DEFS_CAP     256            // max named definitions for TAG_REF.
#define ALO_STATE_CAP (1ULL << 22)  // ALO substitution-chain entries.
#define MAX_DIM      8              // max tensor rank
#define KPROG_INIT_OPS   8          // initial program capacity (grows on demand)
#define KPROG_MAX_OPS    (1ULL<<20) // hard sanity bound (1M ops/kernel)
                                    // (Conv2D Fold-add chains run ~5 ops
                                    // per partial * kh*kw partials + adds;
                                    // 5x5 conv backward = ~150 ops)
#define KERNEL_INIT_INPUT 8         // initial input-arrays capacity (grows on demand)
#define KERNEL_MAX_INPUT  (1ULL<<20) // hard sanity bound (1M inputs/kernel)
                                    // (Conv2D fuses 2*kh*kw input/weight
                                    // tids into one kernel; 64 covers up
                                    // to 5x5 with headroom)
#define MAX_UOP_SRC  3              // max source slots per KProgOp (CONV2D needs 3: input/weights/bias)

// === Tensor descriptor + backend ===

typedef struct {
  u32 ndim;
  u32 dims[MAX_DIM];
} Shape;

typedef struct {
  Shape shape;
  i32   strides[MAX_DIM];       // 0 = broadcast, negative = flip
  i32   offset;                 // element offset into buffer
  u32   numel;                  // product of shape
  u8    contiguous;             // 1 if row-major from offset 0
} View;

typedef struct Backend Backend;

// ShapeTracker = primary `view` (the public-facing shape) + an
// OPTIONAL chain of prior_views composed from outermost to innermost.
// Mirrors tinygrad's `ShapeTracker = tuple[View, ...]` where the
// last view is what the user sees and the index into the underlying
// buffer is computed by composing through the chain.  Required for
// patterns where a movement op can't be absorbed into a single view
// (e.g. RESHAPE on a non-contig view): rather than bail or copy, we
// APPEND a fresh canonical view at the front and walk the chain at
// dispatch time via view_strided_index_chain.
//
// Chain semantics (matching tinygrad's views_to_indexed_uops):
//   - `view` is the OUTERMOST; flat_idx is unravelled through its
//     shape, strides+offset applied to produce a flat idx in the
//     next-inner view's shape.
//   - prior_views[nviews-1] ... prior_views[0] are the inner views;
//     each step unravels the incoming flat idx through its shape,
//     applies its strides+offset, passes outward.
//   - prior_views[0] (innermost) maps to the buffer index.
//
// Most TenDescs have nviews == 0 (just the primary `view`); the
// chain is only allocated when truly needed.
typedef struct {
  u32      dtype;               // DT_BOOL .. DT_UINT4 (see info.c)
  u32      refcount;            // shared by DUP; decremented by ERA
  View     view;                // primary (outermost, public-facing)
  View    *prior_views;         // NULL when nviews == 0; else heap array of nviews entries
  u8       nviews;              // 0 = simple single view, >0 = chain depth
  u32      buf_id;              // backend buffer handle (0 = no buffer yet)
  u32      producer_kid;        // kernel id that produces this tensor, 0 = external
  Backend *backend;             // vtable
} TenDesc;

// Forward declaration for the dispatch callback.
struct KernelEntry;

// === KernelAxes ===
// Axis-typed scheduling structure carried per-KernelEntry, mirroring
// tinygrad's `Kernel.axis_types[]` / `Kernel.full_shape[]`.  The
// codegen variant emitter (cg_emit_variants) walks these to produce
// a structured iteration nest -- nested loops, unrolled blocks,
// thread/threadgroup bindings (Metal) -- instead of one flat
// `for i = 0..numel-1`.
//
// Default state (set by axes_default_for at materialize-time): one
// LOOP axis per output dim plus a trailing REDUCE for tail-REDUCE
// kernels.  Equivalent to today's flat emit -- 393/393 must keep
// passing with no opts applied.
//
// Opts are applied via axes_apply_opt and recorded in applied_opts[].
// The C-side state is the source of truth; WL is a thin LibraryLink
// wrapper.

// Scaffolding: recorded by apply_opt; renderer honors only KAX_PARALLEL today.
#define KAX_LOOP          0    // default: nested for-loop in the iter nest
#define KAX_REDUCE        1    // tail-REDUCE k-loop (default for reduce kernels)
#define KAX_UPCAST        2    // unrolled inner output axis (no loop emitted)
#define KAX_UNROLL        3    // unrolled reduce-axis (k-loop unrolled)
#define KAX_LOCAL         4    // Metal: bound to thread_position_in_threadgroup
#define KAX_GLOBAL        5    // Metal: bound to threadgroup_position_in_grid
#define KAX_GROUP_REDUCE  6    // Metal: threadgroup-shared accumulator + barrier

#define MAX_AXES   16
#define MAX_OPTS   32

// KOpt op encoding -- matches WL TOpt's op_String at the LibraryLink
// boundary (translated by thvm_wl_kernel_apply_opt).  KOP_NONE = 0
// is a sentinel for empty applied_opts[] entries.
#define KOP_NONE     0
#define KOP_UPCAST   1
#define KOP_UNROLL   2
#define KOP_LOCAL    3
#define KOP_GROUP    4
#define KOP_GROUPTOP 5
#define KOP_SWAP     6
#define KOP_PADTO    7
#define KOP_NOLOCALS 8
#define KOP_TC       9

typedef struct {
  u8  op;        // KOP_*
  u8  axis;      // 0-indexed; meaning depends on op
  u32 arg;       // op-specific (split factor for UPCAST/UNROLL, target
                 // axis index for SWAP, pad target for PADTO, ...)
} KOpt;

typedef struct {
  u8   axis_types[MAX_AXES];   // KAX_*
  u32  full_shape[MAX_AXES];
  u8   n_axes;                 // 0 = uninitialized (not yet defaulted)
  KOpt applied_opts[MAX_OPTS];
  u8   n_applied;
  u8   autotuned;              // 1 = kernel_autotune has run on this
                               // KernelAxes (per-program-shape via the
                               // KpCacheSlot).  Guards the "fire-time
                               // autotune" path against re-running on
                               // every dispatch and against infinite
                               // recursion when autotune itself fires
                               // the kernel for benching.  Preserved
                               // across axes_reset_to_default so a
                               // proposer-explored variant doesn't
                               // re-trigger autotune mid-bench.
} KernelAxes;

struct Backend {
  u32   id;
  // view_aware = 1 if dispatch_kernel pre-materializes non-contig
  // input TenDescs (sub-item f3b/c/d/e/g view-only aliases) via
  // view_strided_index before reading buffer bytes.  CPU sets this
  // (cpu_interpret has the pre-materialize loop); Metal does NOT
  // (its shaders read bufs flat).  materialize_uop_in_env's f3
  // view-only branches gate on this flag and fall through to
  // kernel emission when the active backend can't consume aliases
  // -- the reflected wins land for view-aware backends only.
  u8    view_aware;
  int   (*init)(void);
  void  (*shutdown)(void);
  u32   (*buf_alloc)(u64 nbytes);
  void  (*buf_free) (u32 buf_id);
  void  (*buf_incref)(u32 buf_id);
  void  (*buf_decref)(u32 buf_id);
  int   (*buf_read) (u32 buf_id, void *dst, u64 nbytes);
  int   (*buf_write)(u32 buf_id, const void *src, u64 nbytes);
  int   (*dispatch_kernel)(struct KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id);
};

// === KernelEntry ===
// A linearized compute program produced by materialize; consumed by
// the backend's dispatch_kernel (cpu_interpret for the CPU backend).
// Each program slot is an SSA-style op whose sources reference
// either an earlier slot (by index) or a kernel input tensor
// (high bit set).  Matches tinygrad's pickled UOp list that
// `tinygrad/runtime/ops_python.py` interprets.

#define KSRC_INPUT_FLAG  0x80000000u
#define KSRC_AS_INPUT(n) (KSRC_INPUT_FLAG | (u32)(n))
#define KSRC_IS_INPUT(s) (((s) & KSRC_INPUT_FLAG) != 0)
#define KSRC_INDEX(s)    ((s) & 0x7FFFFFFFu)

typedef struct {
  u8    opcode;                    // UOP_* opcode
  u8    dtype;                     // DT_*
  u8    n_src;                     // 0..MAX_UOP_SRC
  u8    src0_ndim;                 // ndim of source slot 0; 0 = unknown
                                   // (movement ops: EXPAND uses this to
                                   // distinguish leading-axis vs
                                   // trailing-axis broadcasts; other
                                   // ops ignore the src_dims block)
  u8    out_ndim;                  // ndim of this op's output;
                                   //   0 = unknown / unused
  u32   src[MAX_UOP_SRC];          // KSRC_AS_INPUT(n) or program index
  u32   arg;                       // CONST bits, REDUCE kind+axis, ...
  u32   numel;                     // output numel (for broadcast detection)
  u32   src0_dims[MAX_DIM];        // per-axis dims of source slot 0
                                   // (only meaningful when src0_ndim > 0;
                                   // used by axis-aware EXPAND in v1, can
                                   // generalise to RESHAPE/PERMUTE later)
  u32   out_dims [MAX_DIM];        // per-axis dims of this op's output;
                                   //   only meaningful when out_ndim > 0
  u8    pad_widths[2 * MAX_DIM];   // PAD only: interleaved per-axis
                                   //   {b0, e0, b1, e1, ...} pad widths
                                   //   (u8 caps each width at 255 -- plenty
                                   //   for transposed-conv kh/kw - 1)
  u8    axis_perm [MAX_DIM];       // PERMUTE only: out_axis i comes from
                                   //   src axis axis_perm[i].  u8 fits
                                   //   since MAX_DIM=8.
} KProgOp;

// === scalar UOp lowering (Phase A of scalar_uops_lowering.md) ===
// Tinygrad-style scalar-level UOps for the new schedule lowering
// pass.  Each ScalarUop is one node in a per-kernel scalar-level
// dataflow graph: explicit RANGE iterators, INDEX pointer arith,
// LOAD/STORE memory access, and ALU ops on scalar values.  The
// graph is transient infrastructure -- the realize pipeline
// linearizes it down to KProgOp[] (the existing kernel-program
// dispatch format) before kernel_fire_by_id runs.
//
// Stored on the KernelEntry as an introspectable snapshot
// (`ke->scalar_uops`, NULL when the kernel was emitted via the
// legacy per-tensor-UOp visit() path).  WL-side surface:
// `TKernelScalarUops[kid]`.
//
// Slot 0 is reserved as the NONE sentinel; callers check
// `src[i] == 0` to test "no source".
typedef enum {
  S_NONE = 0,
  // Loop iterators + memory addressing.
  S_RANGE,         // extra = (axis_type << 32) | extent
                   //   axis_type: 0 = LOOP (default), 1 = REDUCE,
                   //              2 = UNROLL (Phase F), 3 = GLOBAL (Phase F)
                   //   src: none (RANGE is a leaf).
  S_DEFINE_PARAM,  // extra = input slot index into ke->input_*.  Leaf
                   //   that names a buffer pointer for INDEX/LOAD/STORE.
  S_DEFINE_OUTPUT, // marker for the kernel's output buffer.  Leaf.
  S_INDEX,         // src[0] = buffer (DEFINE_PARAM/OUTPUT), src[1..] = ranges
  S_LOAD,          // src[0] = INDEX
  S_STORE,         // src[0] = INDEX dest, src[1] = value
  S_BUFFERIZE,     // arena root: src[0] = body STORE, src[1..] = ranges.
                   //   One per kernel; identifies the root output binding.
  // Constants.
  S_CONST,         // extra = raw bits (f32 in low 32, or int).  dtype set.
  // ALU (mirror tinygrad's GroupOp.ALU subset we'll lower to first).
  S_ADD, S_MUL, S_NEG, S_RECIP, S_EXP2, S_LOG2, S_SQRT,
  S_CMPLT, S_CMPEQ,
  // Reductions.  src[0] = body inside the REDUCE-typed RANGE.
  S_REDUCE_SUM, S_REDUCE_MAX,
  // Type conversion.  src[0] = source value; dispatcher reads the
  // source op's dtype, decodes, converts to u->dtype, re-encodes.
  // (BITCAST is identity at the scalar level -- same bits, downstream
  // dtype interpretation flows through u->dtype, so it does NOT
  // need a separate opcode.)
  S_CAST,
  // SHRINK / PAD index transforms.  src[0] = body to evaluate
  // under shifted ranges.  src[1..ndim] = the ranges to shift.
  // S_SHRINK: extra packs per-axis begin offsets (u16 each, up
  //           to 3 axes).  Body sees range_iter[d] += begin[d];
  //           dispatcher save/restores around the body eval.
  // S_PAD:   like S_SHRINK but the body is gated -- the output
  //           is the source value at (iter - begin) IFF the
  //           shifted iter is in [0, src_dim); otherwise 0.
  //           extra packs (begin u8, src_dim u8) per axis (up to
  //           4 axes).
  S_SHRINK,
  S_PAD,
  // Bit-pattern-preserving load.  Like S_LOAD but never widens
  // narrow FPs (fp16/bf16/fp8) to f32 -- returns the raw nibble/
  // byte/halfword bits in the low bits of the u64 register.  Emitted
  // by BITCAST(narrow-FP -> int) so the original bit pattern survives
  // to the STORE.  src[0] = INDEX, dtype = source dtype (informational).
  S_LOAD_RAW,
  // Per-axis index reversal (UOP_FLIP).  src[0] = body, src[1..ndim] =
  // LOOP ranges to potentially flip; extra holds a u8 bitmask -- bit d
  // set means flip axis d (replace iter with extent-1-iter for the
  // body eval, restore after).
  S_FLIP,
  // Iter-coord shape reinterpret (UOP_RESHAPE when src0_dims !=
  // out_dims AND a downstream PAD/SHRINK consumes the new shape).
  // Legacy "shared LOOP refs" form: src[1..nrng) are LOOP ranges
  // used as both input and output via in-place iter shift.  extra
  // packs out_dims (low 32, 4xu8) and in_dims (high 32, 4xu8).
  // Body S_LOAD's strides match in_dims (caller's responsibility).
  S_RESHAPE,
  // Iter-coord shape reinterpret (split-src form).  Mirrors
  // tinygrad's RESHAPE-as-flat-roundtrip in apply_movement_op.
  //
  // Encoding:
  //   src[0]                       = body (the wrapped expression)
  //   extra[bits 0..7]             = N_out (number of output range refs)
  //   src[1 .. 1+N_out)            = output range refs (iters drive
  //                                   flat_idx; typically LOOP type)
  //   src[1+N_out .. src_count)    = input range refs (S_RESHAPE_V
  //                                   writes their iters from the
  //                                   flat_idx decomposition; typically
  //                                   VIRT type when ranks differ from
  //                                   the kernel's LOOP nest)
  //
  // Each range's extent is read from its S_RANGE.extra (low 32 bits),
  // so this form supports arbitrary u32 extents (legacy S_RESHAPE
  // packs dims as u8s and is capped at 255 per axis).
  // At eval:
  //   flat_idx = sum_d(iter[out_d] * out_stride[d])  -- row-major over output extents
  //   for d in 0..N_in:
  //     iter[in_d] = (flat_idx / in_stride[d]) % in_extent[d]
  //   v = eval body
  //   restore in iters
  //   return v
  // Used for rank-mismatch RESHAPE (input rank != output rank or !=
  // kernel LOOP rank); input refs point to fresh S_AXIS_VIRT ranges.
  S_RESHAPE_V,
  // ===========================================================
  // Integer iter-arithmetic ops -- the backbone for porting
  // tinygrad's apply_movement_op (indexing.py:128-145).  Each
  // returns an i64 value (in the low bits of u64) computed from
  // its sources.  All sources must themselves return integer
  // values (S_RANGE iter, S_ICONST, or other S_I* ops).  Used to
  // build address expressions for S_INDEX_E.
  //
  // Examples:
  //   SHRINK adds a per-axis begin to an iter:
  //     S_IADD(iter, S_ICONST(begin))
  //   FLIP negates: S_ISUB(S_ICONST(extent-1), iter)
  //   RESHAPE flat-roundtrip: S_IMOD(S_IDIV(flat, S_ICONST(s)), S_ICONST(d))
  //   PAD bounds-checked load: S_IWHERE(S_IAND(S_ILT(begin, iter),
  //                                            S_ILT(iter, begin+sd)),
  //                                     S_LOAD(...), S_ICONST(0))
  S_ICONST,    // extra = signed integer literal (i64 reinterpret)
  S_IADD,      // src[0] + src[1]
  S_ISUB,      // src[0] - src[1]
  S_IMUL,      // src[0] * src[1]
  S_IDIV,      // src[0] / src[1] (integer truncating divide)
  S_IMOD,      // src[0] % src[1]
  S_ILT,       // src[0] < src[1] -> 0 or 1
  S_IAND,      // src[0] & src[1] (bitwise; used for boolean AND of 0/1 values)
  S_IWHERE,    // src[0] ? src[1] : src[2]
  // Expression-based INDEX.  src[0] = buffer (DEFINE_PARAM/OUTPUT),
  // src[1] = integer expression giving the byte/element offset.  No
  // per-axis stride packing -- the caller builds the address as a
  // tree of S_I* ops over S_RANGE iters.  Mirrors tinygrad's INDEX
  // (a BinaryOp on a pointer + an arbitrary symbolic offset).
  // The output of S_LOAD/S_STORE on this INDEX behaves identically
  // to the legacy S_INDEX form -- only the address computation
  // differs.
  S_INDEX_E,
  S__COUNT
} ScalarOp;

// Axis types for S_RANGE.extra high 32 bits.
#define S_AXIS_LOOP    0
#define S_AXIS_REDUCE  1
#define S_AXIS_UNROLL  2
#define S_AXIS_GLOBAL  3
// Virtual / placeholder range (mirrors tinygrad's AxisType.PLACEHOLDER).
// Owns a slot in the dispatcher's range_iter[] array but is NOT iterated
// by the outer LOOP nest.  An S_RESHAPE wrapper writes its iter at body
// eval time (decomposed from output ranges' flat_idx) and restores after.
// Lets a sub-expression at a different rank than the kernel's LOOP nest
// flow through the same scalar-uop graph.
#define S_AXIS_VIRT    4

// SCALAR_MAX_SRC = 1 buffer/store + up to MAX_DIM range refs.
// Sized so a single S_INDEX or S_BUFFERIZE can carry every LOOP/
// REDUCE range of an arbitrary-rank kernel without chaining.
#define SCALAR_MAX_SRC (MAX_DIM + 1)

typedef struct {
  u8  op;                    // ScalarOp
  u8  src_count;             // number of valid src[] entries
  u32 dtype;                 // DT_*
  u32 src[SCALAR_MAX_SRC];   // indices into the same ScalarUop[]; 0 = unused
  u64 extra;                 // op-specific payload (CONST bits, RANGE extent + type, ...)
} ScalarUop;

#define SUOP_INIT_CAP   16
#define SUOP_MAX_CAP    (1u << 20)

// === tile UOp plan =====================================================
// TileUop is the next scheduling layer above ScalarUop.  It does not
// replace rangeify: it wraps a proven scalar graph with an explicit
// tile / loop / memory plan that renderers can later lower to CPU
// loops, Metal threadgroups, local memory, barriers, and eventually
// MMA intrinsics.
//
// MVP invariant: tile_build_from_scalar creates
//   TILE_LOOP_NEST(TILE_STORE(TILE_SCALAR_BODY(value_id)), TILE_AXIS...)
// from a KernelEntry's scalar_uops[] + KernelAxes.  Dispatch ignores
// tile_uops for now; they are an introspectable optimization plan.
typedef enum {
  TILE_NONE = 0,
  TILE_AXIS,        // extra = (KAX_* << 32) | extent
  TILE_SCALAR_BODY, // extra = ScalarUop id of value expression
  TILE_LOOP_NEST,   // src[0] = TILE_STORE body, src[1..] = TILE_AXIS nodes
  TILE_LOCAL_ALLOC, // future: threadgroup/local memory allocation
  TILE_LOAD,        // future: cooperative tile load
  TILE_STORE,       // src[0] = TILE_SCALAR_BODY value, extra = scalar S_STORE id
  TILE_BARRIER,     // future: target barrier between tile stages
  TILE_REDUCE,      // future: tiled/tree reduction
  TILE_MMA,         // future: tensor-core / simdgroup matmul
  TILE__COUNT
} TileOp;

#define TILE_MAX_SRC  (MAX_AXES + 1)
#define TILE_INIT_CAP 16
#define TILE_MAX_CAP  (1u << 20)

typedef struct {
  u8  op;                    // TileOp
  u8  src_count;             // number of valid src[] entries
  u32 dtype;                 // DT_* where meaningful, DT_COUNT otherwise
  u32 src[TILE_MAX_SRC];     // indices into TileUop[]; 0 = unused
  u64 extra;                 // op-specific payload
} TileUop;

typedef struct {
  u32 root_id;
  u32 store_tile_id;
  u32 body_tile_id;
  u32 scalar_store_id;
  u32 scalar_index_id;
  u32 scalar_value_id;
  u32 dtype;
  u32 n_axes;
  u32 axis_ids    [MAX_AXES];
  u32 axis_types  [MAX_AXES];
  u32 axis_extents[MAX_AXES];
} TilePlanInfo;

typedef struct KernelEntry {
  // Input-tensor arrays: dynamically grown.  inputs_cap is the
  // allocated length; n_inputs is the number of slots actually used.
  // Use kernel_inputs_reserve() before writing past n_inputs.
  u32       n_inputs;
  u32       inputs_cap;
  u32      *input_tids;            // TenDesc id, or 0 if symbolic
  u32      *input_dtypes;
  u32      *input_numels;
  // Per-input-slot view metadata.  When the input is a contig
  // TenDesc, input_views[i] mirrors TENS[input_tids[i]].view (the
  // canonical layout) and the renderer emits flat `in%u[i]`.  When
  // the input is non-contig (single-view stride pattern -- multi-
  // view chains still bail), the renderer composes a strided
  // index expression `in%u[s0*c0(i) + ... + offset]` into the
  // body loop, eliminating the cpu_interpret pre-mat pass for
  // strided reads.  Mirrors tinygrad's codegen-time index UOP
  // inlining (each input's stride pattern is hardcoded into the
  // generated kernel; different patterns get different dylibs).
  View     *input_views;
  // For inputs that aren't statically a TenDesc (e.g., a free TAG_VAR
  // that gets bound to a TEN at fire time via APP-LAM beta), we
  // store the symbolic Term value here.  kernel_fire_by_id resolves
  // each non-zero entry through term_resolve before reading buffers.
  Term     *input_terms;

  u32       output_tid;            // TenDesc id we write to
  u32       output_dtype;
  Shape     output_shape;
  u32       output_numel;

  // Program: dynamically grown.  ops_cap is the allocated length;
  // n_ops is the count actually used.  Use kernel_program_reserve()
  // before writing past n_ops.
  u32       n_ops;
  u32       ops_cap;
  KProgOp  *program;
  // 1 = `program` points into the kernel-program cache (owned by
  // the cache, not by this kernel).  kernel_free_arrays must not
  // free() it.  Set by emit_kernel_for_boundary after a cache hit
  // or after the program is interned into the cache.  Sharing is
  // safe because KProgOp[] is structurally keyed (input slot
  // indices, not concrete tids), and per-kernel I/O lives in
  // input_tids[] / output_tid which are NOT shared.
  u8        program_shared;

  // Original root UOP term that this kernel was built from.  The
  // walker rewrites parent cells to UOP_KERNEL but leaves the
  // source UOP cells in the heap (now "orphaned" -- no live
  // references reach them through the rewritten parents).  Grad
  // chain-rule walks this term directly: heap_read on its child
  // slots gives whatever they point at now (often other kernels,
  // each of which carries its own source_uop), so the walk
  // recurses into the original UOp graph naturally.
  Term      source_uop;

  u8        spliced;               // 1 if the kernel's program was inlined
                                   // into a parent via
                                   // materialize_splice_into; kernel_fire_by_id
                                   // skips dispatch (the parent now produces
                                   // this kernel's output buffer too).
  u32       consumer_count;        // # of OTHER kernels whose input_tids
                                   // trace back (via TENS[tid].producer_kid)
                                   // to this kernel.  Populated by
                                   // kernel_compute_consumer_counts.  Used
                                   // by the refcount-driven free pass to
                                   // decide when this kernel's output buf
                                   // is no longer needed.
  u32       fire_gen;              // last KERNEL_FIRE_GEN this kernel
                                   // dispatched at.  kernel_fire_by_id skips
                                   // when fire_gen == current gen so a
                                   // kernel referenced by N consumers in one
                                   // realize fires once instead of N times.
                                   // Bumped per top-level interact_kernel.
  void     *compiled;              // backend-specific; NULL for interpreter

  // Axis-typed scheduling plan.  Phase 16 per-program-shape
  // sharing: `axes` is a POINTER, normally aimed at the
  // KernelAxes embedded in this kernel's kernel_program_cache
  // slot so every kid with the same KProgOp[] sees the same opts.
  // Apply once -> propagates to all sharing kids; the C-side
  // proposer can attach opts to a program shape and every future
  // training-loop iter inherits them automatically.
  //
  // `_local_axes` is the fallback storage for kernels that didn't
  // make it into the cache (n_ops == 0 or cache full); `axes`
  // points at it in that case.
  KernelAxes  *axes;
  KernelAxes   _local_axes;

  // Scalar-UOp lowering snapshot (Phase A of scalar_uops_lowering.md).
  // NULL when the kernel was emitted via the legacy per-tensor-UOp
  // visit() path; non-NULL when rangeify lowered it.  Slot 0 is the
  // S_NONE sentinel; live ops occupy [1, n_scalar_uops).
  // Owned by the KernelEntry; freed by kernel_free_arrays.
  ScalarUop *scalar_uops;
  u32        n_scalar_uops;
  u32        scalar_uops_cap;

  // Tile-level schedule/memory plan above scalar_uops.  NULL until
  // tile_build_from_scalar (or a future tile planner) populates it.
  // Slot 0 is TILE_NONE; live ops occupy [1, n_tile_uops).
  // Owned by the KernelEntry; freed by kernel_free_arrays.
  TileUop   *tile_uops;
  u32        n_tile_uops;
  u32        tile_uops_cap;
  u32        tile_root;       // root TileUop id, usually TILE_LOOP_NEST; 0 = none
} KernelEntry;

// KERNELS / KERNELS_NEXT now live in TContext (see below); the
// macros at the bottom of this file resolve them through CURRENT_CTX.

// AloState chain entries -- each one binds an old book loc to a fresh
// dynamic loc (used by ALO-VAR / ALO-LAM to retarget VARs into the
// realised heap).  state_id 0 = empty chain.
typedef struct {
    u32 parent;     // upstream state id (0 if root)
    u64 old_loc;    // book-heap loc of the binder
    u64 new_loc;    // freshly allocated dyn-heap loc that replaces it
} AloState;

// === CpuBuf (used by backend/cpu/) ===
// Parallel table to TENS[]: each TenDesc.buf_id indexes into ctx->cpu_bufs.
// Multiple TenDescs can share a buf_id (view aliasing); refcount controls
// storage lifetime separately from TenDesc.refcount.
typedef struct {
  void *data;
  u64   nbytes;
  u32   refcount;
  u8    owns_data;
  u8    preserved;
  u8    freeable;
  void *handle;
  void (*on_release)(void *handle);
} CpuBuf;

#define CPU_BUFS_CAP     (1ULL << 20)
#define CPU_FREELIST_CAP 4096

// === HotCounters ===
// Per-context hot-path counters for WL-side debugging.  Bumped from
// the heaviest loops (heap_replace cascade, is_redex, redex_enumerate,
// wnf, realize, materialize, kernel/grad fires) and snapshotted via
// `THotCounters[]` from WL.  Use to confirm whether per-step time is
// dominated by the substitution cascade (heap_replace_cells), the
// chain-rule expansion (grad_fires), or kernel emit/dispatch.
//
// When you add a counter, append its field here AND the same name in
// `$hotCounterNames` in `wl/THVMLink/Kernel/Profile.wl`; the WL side
// decodes the {Integer, 1} payload by position.
typedef struct {
    u64 heap_replace_calls;
    u64 heap_replace_cells;     // sum of HEAP_NEXT at each call -- the cascade-cost integral
    u64 is_redex_calls;
    u64 redex_enum_calls;
    u64 redex_enum_cells;       // sum of HEAP_NEXT at each call
    u64 wnf_calls;
    u64 realize_calls;
    u64 materialize_calls;
    u64 kernel_fires;
    u64 grad_fires;
} HotCounters;

#define HOT_COUNTER_COUNT 10

// === TContext ===
// Bundles every piece of mutable runtime state into one struct so users
// can hold multiple coexisting heaps via TContextNew[] / TInContext[].
// The legacy single-runtime API (TInit / TWnf / ...) operates on slot
// 0 by default; explicit TContext args switch CURRENT_CTX for the
// duration of the call.  Reference design: TinyHVM/src/tinyhvm.h:1102.
//
// All existing globals (HEAP, BOOK_HEAP, DEFS, ALO_STATES, TENS,
// KERNELS, CURRENT_BACKEND, plus the file-scope statics for the WNF
// last-stack snapshot, the shape-env arena, the book-ref-visited
// bitmap, and the CPU buf/freelist pools) become fields of this
// struct.  Macros below redirect each old global identifier through
// CURRENT_CTX so the rest of the runtime keeps compiling unchanged.
typedef struct TContext {
    /* Heap-allocated arrays (calloc on context_create). */
    Term       *heap;
    Term       *wnf_stack;
    Term       *wnf_last_stack;          // snapshot on wnf_n bail
    TenDesc    *tens;
    KernelEntry*kernels;
    Term       *book_heap;
    AloState   *alo_states;
    CpuBuf     *cpu_bufs;                // CPU backend buf table

    /* Inline small arrays (per-context, zero-init in BSS). */
    Term        defs[DEFS_CAP];
    u8          book_ref_visited[DEFS_CAP];
    u32         cpu_freelist[CPU_FREELIST_CAP];

    /* Backend registry: per-tensor backends are stored on TenDesc;
       this array is the registry of available backends + the index
       used for newly allocated tensors. */
    Backend    *backends[4];             // THVM_DEV_CPU=0, THVM_DEV_METAL=1
    u32         n_backends;
    u32         default_device;

    /* Scalars / counters. */
    u64 heap_next;
    u32 wnf_s_pos;
    u32 wnf_last_stack_len;
    u64 itrs;
    u32 tens_next;
    u32 kernels_next;
    u64 book_next;
    u32 alo_states_next;
    u64 cpu_bufs_next;
    u32 cpu_freelist_len;

    /* Hot-path counters (see HotCounters). */
    HotCounters hot;
} TContext;

#define THVM_MAX_BACKENDS 4
#define THVM_DEV_CPU      0
#define THVM_DEV_METAL    1

#define CONTEXTS_CAP 16
extern TContext *CURRENT_CTX;
extern TContext *CONTEXTS[CONTEXTS_CAP];

// Macro layer -- existing global names redirect through CURRENT_CTX
// so all the C code under src/heap, src/term, src/wnf, src/schedule,
// src/alo, src/uop, src/book, src/backend keeps compiling without
// per-call ctx threading.  Single-threaded today; multi-thread would
// need a thread-local CURRENT_CTX.
#define HEAP                (CURRENT_CTX->heap)
#define HEAP_NEXT           (CURRENT_CTX->heap_next)
#define WNF_STACK           (CURRENT_CTX->wnf_stack)
#define WNF_S_POS           (CURRENT_CTX->wnf_s_pos)
#define WNF_LAST_STACK      (CURRENT_CTX->wnf_last_stack)
#define WNF_LAST_STACK_LEN  (CURRENT_CTX->wnf_last_stack_len)
#define ITRS                (CURRENT_CTX->itrs)
#define TENS                (CURRENT_CTX->tens)
#define TENS_NEXT           (CURRENT_CTX->tens_next)
#define KERNELS             (CURRENT_CTX->kernels)
#define KERNELS_NEXT        (CURRENT_CTX->kernels_next)
#define BOOK_HEAP           (CURRENT_CTX->book_heap)
#define BOOK_NEXT           (CURRENT_CTX->book_next)
#define DEFS                (CURRENT_CTX->defs)
#define ALO_STATES          (CURRENT_CTX->alo_states)
#define ALO_STATES_NEXT     (CURRENT_CTX->alo_states_next)
#define BOOK_REF_VISITED    (CURRENT_CTX->book_ref_visited)
#define CPU_BUFS            (CURRENT_CTX->cpu_bufs)
#define CPU_BUFS_NEXT       (CURRENT_CTX->cpu_bufs_next)
#define CPU_FREELIST        (CURRENT_CTX->cpu_freelist)
#define CPU_FREELIST_LEN    (CURRENT_CTX->cpu_freelist_len)

// Hot-path counters (see HotCounters / instrument/hot_counters.c).
#define HOT_HEAP_REPLACE_CALLS  (CURRENT_CTX->hot.heap_replace_calls)
#define HOT_HEAP_REPLACE_CELLS  (CURRENT_CTX->hot.heap_replace_cells)
#define HOT_IS_REDEX_CALLS      (CURRENT_CTX->hot.is_redex_calls)
#define HOT_REDEX_ENUM_CALLS    (CURRENT_CTX->hot.redex_enum_calls)
#define HOT_REDEX_ENUM_CELLS    (CURRENT_CTX->hot.redex_enum_cells)
#define HOT_WNF_CALLS           (CURRENT_CTX->hot.wnf_calls)
#define HOT_REALIZE_CALLS       (CURRENT_CTX->hot.realize_calls)
#define HOT_MATERIALIZE_CALLS   (CURRENT_CTX->hot.materialize_calls)
#define HOT_KERNEL_FIRES        (CURRENT_CTX->hot.kernel_fires)
#define HOT_GRAD_FIRES          (CURRENT_CTX->hot.grad_fires)

// Replaces the old CURRENT_BACKEND global -- "default backend for
// newly allocated tensors only".  Per-tensor ops use ten->backend.
#define DEFAULT_BACKEND     (CURRENT_CTX->backends[CURRENT_CTX->default_device])
// Compatibility alias for code that still says CURRENT_BACKEND.
#define CURRENT_BACKEND     DEFAULT_BACKEND

// === Context lifecycle API ===
// Create a fresh context; returns its slot id (1..CONTEXTS_CAP-1) or
// 0 on failure.  default_device picks which backend new tensors use.
u32 thvm_context_create(const char *default_device);
// Set CURRENT_CTX to the given slot; returns the previous slot id.
u32 thvm_context_select(u32 slot);
// Returns the current slot id (0 = default).
u32 thvm_context_current(void);
// Free everything in the given slot.  Slot 0 (default) is a no-op
// (same as thvm_free) since it cannot be re-created without leaking
// the static DEFAULT_CTX_STORAGE.
void thvm_context_destroy(u32 slot);

// === term/ ===
fn Term term_new(u8 sub, u8 tag, u32 ext, u64 val);
fn u8   term_tag(Term t);
fn u32  term_ext(Term t);
fn u64  term_val(Term t);
fn u8   term_sub_get(Term t);
fn Term term_sub_set(Term t, u8 sub);

// === heap/ ===
fn u64  heap_alloc(u64 size);
fn Term heap_read(u64 loc);
fn void heap_set(u64 loc, Term t);
fn Term heap_take(u64 loc);                                 // read + zero
fn void heap_subst_var(u64 loc, Term value);
fn Term heap_subst_cop(u8 side, u64 loc, Term r0, Term r1); // pair subst

// === gc/ === (Cheney semi-space copying GC; defined in heap/collect.c)
fn void gc_init(u64 space_words);
fn void gc_reset(void);
fn int  gc_enabled(void);
fn u64  gc_from_start(void);
fn u64  gc_from_end(void);
fn u64  gc_count(void);
fn void gc_collect(Term *roots, u32 n_roots);
fn Term gc_collect_with(Term result);

// === book heap (static templates) ===
fn u64  book_alloc(u64 size);
fn Term book_read (u64 loc);
fn void book_set  (u64 loc, Term t);

// Snapshot a dynamic term tree into the book heap.  Returns a
// book-domain term whose val refers to BOOK_HEAP rather than HEAP.
Term thvm_book_from_dynamic(Term body);

// Register `body` as the definition for `name`.  Snapshots the
// body into the book heap (so subsequent mutations to the dynamic
// graph don't affect the def) and stores the root book term in
// DEFS[name].  TRef[name] then unfolds via ALO on demand.
void thvm_def_register(u32 name, Term body);

// === ALO ===
//
// `state_id` references an AloState chain mapping book locs to
// freshly allocated dynamic locs.  `alo_realize` walks one layer of
// the static `book_term` into dynamic cells, returning a dynamic
// term; child slots are themselves wrapped in fresh ALOs (lazy).
// `alo_force` does the same starting from a TAG_ALO term.
u32  alo_state_push  (u32 parent, u64 old_loc, u64 new_loc);
int  alo_state_lookup(u32 state_id, u64 old_loc, u64 *out_new_loc);
Term alo_realize     (Term book_term, u32 state_id);
Term alo_force       (Term alo_term);

// === ref ===
fn Term term_new_ref (u32 name);
fn Term term_new_alo (Term book_term, u32 state_id);

// === op2 + mat ===
// Both allocate a 2-cell dyn heap block.  OP2's cells are [x, y];
// MAT's cells are [handler, fallback].  See wnf for the firing rules.
fn Term term_new_op2 (u32 opcode, Term x, Term y);
fn Term term_new_mat (u32 match_val, Term handler, Term fallback);
fn Term term_new_eql (Term a, Term b);
fn Term term_new_and (Term a, Term b);
fn Term term_new_or  (Term a, Term b);
fn Term term_new_any (void);
fn Term term_new_inc (Term body);
fn Term term_new_when(Term cond, Term body);
fn Term term_new_fvr (u32 var_id);
fn Term term_new_bri (Term body);
fn Term term_new_ann (Term val, Term typ);

// 8.1b: TAG_PRI ("primitive function call") -- a thin port of HVM4's
// PRI tag.  Each primitive is registered in a process-global table
// against an id (u32, fits in EXT).  A fresh PRI carries val=0;
// APP-PRI accumulates args into a heap cell `[NUM(count), arg_0, ...]`
// until `count == arity`, at which point the C function is called
// and its return Term replaces the redex.  Used in 8.1c-d to build
// SUP-encoded CP enumeration with unification as a primitive.
typedef Term (*PrimFn)(Term *args);
fn Term term_new_pri  (u32 prim_id);
fn u32  prim_register (u32 prim_id, PrimFn func, u32 arity);
fn PrimFn prim_fun    (u32 prim_id);
fn u32  prim_arity    (u32 prim_id);
#define PRIM_TABLE_CAP 64

// THVM core primitives (slots 16+, leaving room for ATP at 0-4).
//   THVM_PRIM_PRI : (slot_NUM, val, cont) -- wnf forces `val` (firing
//                   any kernel/ASSIGN chain it sits over), invokes
//                   the WL callback registered under `slot` with the
//                   resulting Term (slot=0 = no callback, pure
//                   sequencer), then returns `cont`.  This is the
//                   "TSeq without TSeq" + "log loss each iter" combo:
//                   one general PRI, customised by per-slot WL
//                   functions.  Bridge lives in CSource/thvmlink.c.
#define THVM_PRIM_PRI   16u
fn void thvm_register_core_prims(void);

// k0a: build a TAG_CTR labelled constructor over `n` child Terms.
// Heap layout: [NUM(arity=n), c_0, ..., c_{n-1}].  ext = label
// (0 = anonymous tuple).  Passive in the IC reducer.  Used by
// k0c's multi-target interact_grad to bundle one cotangent per
// requires_grad target into a single result.  Accessor
// term_ctr_at returns 0 (invalid Term) on out-of-range.
fn Term term_new_ctr (u32 label, const Term *children, u32 n);
fn u32  term_ctr_n   (Term ctr_term);
fn Term term_ctr_at  (Term ctr_term, u32 i);

// === lazy outermost-layer resolver ===
// Follows VAR (SUB-bit chain) + ALO (memoised one-layer force);
// returns everything else unchanged.  Cheaper than wnf -- no
// kernel / materialize / grad firing.
fn Term term_resolve(Term t);

// === schedule ===
// realize_classify walks the UOp DAG rooted at `root` and marks
// kernel boundaries (root + multi-consumer + REDUCE) in REALIZE_INFO.
// materialize.c reads the table directly to topo-sort and emit.
#define REALIZE_INFO_CAP 16384
typedef struct {
  u64 loc;
  u32 consumer_count;
  u8  op;
  u8  realized;
} UOpInfo;
extern UOpInfo REALIZE_INFO[REALIZE_INFO_CAP];
extern u32     REALIZE_INFO_LEN;
fn u32  realize_info_find(u64 loc);
fn void realize_classify(Term root);
fn u8   realize_is_realized(Term uop_term);
fn u32  realize_consumer_count(Term uop_term);

// g2a: after realize_classify populates the boundary set, the
// scheduler topo-sorts those boundaries by producer-to-consumer
// depth.  These accessors expose the sorted order so tests / future
// code-emit can iterate boundaries in dependency order.
fn u32  materialize_boundary_count(void);
fn u64  materialize_boundary_at(u32 i);

// Leaf utilities other compilation units (realize_classify,
// gc_mark, wnf/redex, interact/uop_kernel) reference.
fn u8   uop_arity(u8 op);
fn u8   uop_is_unary_elementwise(u8 op);
fn u8   uop_is_binary_elementwise(u8 op);
fn int  term_shape_in(Term t, u32 env_id, Shape *out);
fn int  term_dtype_in(Term t, u32 env_id, u32 *out);
fn Term materialize_uop_in_env(Term t, u32 env_id);

// === interact/ ===
// One file per active pair.  Each rule increments ITRS when it fires.
fn Term interact_app_lam(Term lam, Term arg);
fn Term interact_app_era(void);
fn Term interact_dup_sup(u32 lab, u64 loc, u8 side, Term sup);
fn Term interact_dup_era(u8 side, u64 loc, Term era);
fn Term interact_dup_lam(u32 lab, u64 loc, u8 side, Term lam);
fn Term interact_dup_bri(u32 lab, u64 loc, u8 side, Term bri);
fn Term interact_dup_num(u8 side, u64 loc, Term num);
fn Term interact_dup_ten(u8 side, u64 loc, Term ten);
fn Term interact_dup_uop(u32 lab, u64 loc, u8 side, Term uop);
fn Term interact_kernel (Term kernel);

// === codegen/ axis ===
// Default-initialise a kernel's axis structure from its output shape +
// final REDUCE op (if any).  Idempotent: a no-op if axes.n_axes != 0
// (caller already configured it).  Called from materialize after
// kernel program is committed.
fn void axes_default_for(struct KernelEntry *ke);

// Apply one TOpt to the axis structure: split the indicated axis,
// mark the new inner axis with the opt's KAX_ type (UPCAST/UNROLL/
// LOCAL/etc.), append to applied_opts[].  SWAP swaps two axes
// in-place.  Returns 0 on validation failure (axis out of range,
// arg doesn't divide, applied_opts full).
fn int axes_apply_opt(KernelAxes *ax, KOpt opt);

// Shape-heuristic kernel opt proposer: looks at the kernel's
// program + axes and writes up to `cap` candidate KOpts into
// `out`.  Returns the number written.  The autotune loop applies
// each in isolation against the baseline and picks the winner.
// Today's heuristics are narrow (UNROLL on the reduce axis at
// {2,4,8,16}); add more rules here as new opt classes get codegen
// support.
// Kernel-arena GC: strip per-kernel program/input arrays for kernels
// whose output buffer was released by the cpu_buf pool rollback.
// Returns the number of kernels stripped.  Wired into thvm_realize
// so the per-kernel array memory stays bounded across long training
// loops (M4 of the beautiful-mnist parity arc).  See kernel_gc.c
// header for why slot ids are intentionally NOT recycled.
fn u32  kernel_gc_sweep(Term result);
fn u32  kernel_gc_freelist_pop(void);
fn void kernel_gc_reset(void);

// === scalar UOp arena ops (schedule/rangeify.c) ===
// Reserve room on a kernel's scalar_uops[] array.  Geometric
// growth; aborts past SUOP_MAX_CAP.  Idempotent at-or-below cap.
fn void rangeify_reserve(struct KernelEntry *ke, u32 needed);
// Append a new scalar UOp; returns its slot id (>= 1).  Slot 0
// is the S_NONE sentinel and never returned.  Initializes scalar_uops
// on first call.  src[] entries that are 0 mean "unused".
fn u32  rangeify_emit(struct KernelEntry *ke, u8 op, u32 dtype,
                      u8 src_count, const u32 *src, u64 extra);
// Convenience: emit a leaf (no sources).
fn u32  rangeify_emit_leaf(struct KernelEntry *ke, u8 op, u32 dtype, u64 extra);
// Convenience: emit a unary / binary op.
fn u32  rangeify_emit_unary (struct KernelEntry *ke, u8 op, u32 dtype, u32 a);
fn u32  rangeify_emit_binary(struct KernelEntry *ke, u8 op, u32 dtype, u32 a, u32 b);
// Free the per-kernel scalar arena.  Called from kernel_free_arrays.
fn void rangeify_free(struct KernelEntry *ke);
// Look up a scalar opname / axis-type name as a const C string for
// introspection / debug printing.
fn const char *scalar_op_name (u8 op);
fn const char *scalar_axis_name(u32 axis_type);
// Phase B: try to lower a fully-emitted KernelEntry's KProgOp[] to
// the scalar form.  Returns 1 on success (ke->scalar_uops populated;
// caller can dispatch through the scalar path) and 0 on bail.
fn int  rangeify_try_lower_elementwise(struct KernelEntry *ke);

// === tile UOp arena ops (schedule/tile.c) ===
// The tile plan is the optimization layer above scalar_uops.  These
// helpers mirror the scalar arena API: slot 0 is TILE_NONE, live ops
// start at 1, and src[] entries of 0 mean "unused".
fn void tile_reserve(struct KernelEntry *ke, u32 needed);
fn u32  tile_emit(struct KernelEntry *ke, u8 op, u32 dtype,
                  u8 src_count, const u32 *src, u64 extra);
fn u32  tile_emit_leaf(struct KernelEntry *ke, u8 op, u32 dtype, u64 extra);
fn void tile_free(struct KernelEntry *ke);
fn const char *tile_op_name(u8 op);
fn const char *tile_axis_name(u32 axis_type);
fn int  tile_validate(struct KernelEntry const *ke);
fn int  tile_collect_plan_info(struct KernelEntry const *ke,
                               TilePlanInfo *out);
fn u32  tile_loop_axis_count(struct KernelEntry const *ke);
fn u32  tile_loop_axis_type(struct KernelEntry const *ke, u32 axis);
fn u32  tile_loop_axis_extent(struct KernelEntry const *ke, u32 axis);
// Seed a TILE_LOOP_NEST(TILE_STORE(...)) plan from scalar_uops + KernelAxes.
// Returns 1 on success, 0 when scalar_uops is absent or malformed.
fn int  tile_build_from_scalar(struct KernelEntry *ke);

fn u32 kernel_opts_propose(struct KernelEntry const *ke, KOpt *out, u32 cap);

// Autotune: walk the proposer's candidates, time each variant
// against the baseline (no opts) via kernel_fire_by_id, pick the
// winner, leave the kernel's KernelAxes mutated to the winning
// opt.  Because axes live on the shared KpCacheSlot, the winner
// auto-applies to every other kid sharing this KProgOp[] (a
// training loop emitting one new kid per step inherits from
// iter 2 onward).  Returns 1 if a winning opt was applied,
// 0 if no candidate beat baseline (or no candidates).
fn int kernel_autotune(u32 kid);

// Cheap predicate used by the fire-time auto-tune trigger.  True
// iff (env opt-in `THVM_AUTOTUNE=1`) AND (this KernelAxes hasn't
// been autotuned yet) AND (proposer has at least one candidate).
fn int kernel_should_autotune(struct KernelEntry const *ke);

// Time `n_runs` back-to-back fires of `kid`; return min wallclock
// us.  Used by autotune internally + exposed via LibraryLink for
// the WL TKernelVariants surface.
fn u64 kernel_bench_us(u32 kid, u32 n_runs);

// Bench every proposer candidate (slot 0 = baseline, no opts) and
// write per-variant (KOpt, us) into out_opts/out_us.  Returns the
// number of slots written.  Restores axes to baseline at exit so
// the WL caller can pick what to apply via TKernelApplyOpt.
fn u32 kernel_bench_variants(u32 kid, KOpt *out_opts, u64 *out_us, u32 cap);

// === tensor/ ===
// Tensor descriptor lifecycle.  Step 12: bump-only allocation in TENS[];
// refcount + backend-level buffer refcount govern the buffer lifetime.
fn u32  tensor_alloc  (Backend *b, Shape shape, u32 dtype);
fn void tensor_incref (u32 id);
fn void tensor_decref (u32 id);
fn void tensor_release(u32 id);   // decref + buf_decref; free at 0
fn u32  tensor_view_of(u32 src_id, View new_view);  // alias; bumps buf_incref
// ShapeTracker chain extension: when a movement op can't be absorbed
// into a single view (RESHAPE on non-contig src), the caller calls this
// to make `new_outermost` the new public view and push the previous
// public view onto the prior_views chain.  Buf is shared (incref bumped).
fn u32  tensor_view_chain_append(u32 src_id, View new_outermost);

// === view/ ===
// Map an output flat index to the underlying buffer index through
// a (possibly non-contiguous) View.  Contiguous views short-circuit
// to flat_idx + offset; strided views walk per-axis strides.
fn u32 view_strided_index(View const *v, u32 flat_idx);
// Chain-aware index for a TenDesc's full ShapeTracker.  When
// nviews == 0, equivalent to view_strided_index(&t->view, ...).
fn u32 tendesc_strided_index(TenDesc const *t, u32 flat_idx);

// Build a contiguous View from a Shape.  Step 14 adds the movement ops
// (reshape / permute / expand / pad / shrink / flip).
fn View view_create(Shape shape);
fn u32  shape_numel(Shape s);

// === uop/ ===
// Constructors for raw UOp graph nodes.  Each helper allocates the
// heap cells laid out in docs/tensors.md and returns a TAG_UOP term.
// They do NOT reduce or fire kernels -- they just build graph
// structure.  Materialization (commit 3) consumes a graph built
// from these.
fn Term uop_const  (u32 dtype, u32 bits);
fn Term uop_unary  (u32 opcode, Term src);                       // NEG/RECIP/EXP2/LOG2/SQRT
fn Term uop_binary (u32 opcode, Term a, Term b);                 // ADD/MUL/CMPLT
fn Term uop_reduce (u32 kind, u32 axis, Term src);
fn Term uop_reshape(Term src, u32 ndim, const u32 *dims);
fn Term uop_permute(Term src, u32 ndim, const u32 *perm);
fn Term uop_expand (Term src, u32 ndim, const u32 *dims);
fn Term uop_pad    (Term src, u32 ndim, const u32 *begin_end);   // begin_end[2*ndim]
fn Term uop_shrink (Term src, u32 ndim, const u32 *begin_end);
fn Term uop_flip   (Term src, u32 axes_bitmask);
fn Term uop_cast   (Term src, u32 dst_dtype);                    // value-preserving cast
fn Term uop_bitcast(Term src, u32 dst_dtype);                    // same-itemsize reinterpret

// Build a UOP_GRAD node.  y is the function output, gy is the
// cotangent seed (typically a CONST(1) for top-level VJP), target is
// the leaf TAG_TEN to differentiate against.  Reduces under TWnf via
// the chain-rule rewrite rule defined in interact/uop_grad.c.
// Allocate a shared grad cell holding [y, gy].  Both projections (FWD
// and BWD) reference this cell as TAG_DP0 / TAG_DP1 with the
// DUP_GRAD_FLAG bit set on the ext (label) field.  FWD reads cell[0]
// (y); BWD reads both cell[0] and cell[1] (gy = cotangent seed) and
// runs the gy-threaded chain rule.
fn u64  uop_grad_cell  (Term y, Term gy, Term target);
fn Term uop_grad       (Term y, Term gy);   // BWD projection (TAG_DP1 + grad flag)
fn Term uop_fwd        (Term y, Term gy);   // FWD projection (TAG_DP0 + grad flag)
fn Term uop_grad_with_target(Term y, Term gy, Term target);
fn void uop_leaf_tids(Term root, u32 *out_tids, u32 cap, u32 *n_out);

// Build a UOP_LOAD node wrapping `src`.  Structural marker mirroring
// tinygrad's UOps.LOAD; runtime semantics are identity (memcpy in
// the cpu kernel).  Output shape == src shape; arity 1.
fn Term uop_load(Term src);


// === schedule/ ===
// Top-level materialize driver: heap-walk pass that in-place rewrites
// UOP cells reachable from `term` into UOP_KERNEL cells, propagating
// shapes through APP-LAM bindings.  Called directly from the WL bridge
// (TMaterialize / TRealize).
fn Term thvm_materialize(Term term);

// === interact/uop_grad ===
// Forward-declared so materialize_expr can reduce UOP_GRAD nodes
// inline before kernelizing.  Defined later in src/interact/uop_grad.c.
fn Term interact_grad(Term grad_term);

// === interact/uop_assign ===
// Wnf-fired ASSIGN(dst_TEN, src_TEN): memcpy src.buf -> dst.buf, return
// dst Term.  Eligible (is_redex true) only when both children resolve
// to TAG_TEN -- otherwise wnf walks src's producer first.  Used as the
// in-place mutation primitive for optimizer loops; weights stay
// tid-stable while their buffer contents update each iteration.
//
// Two entries: _with takes already-resolved TENs (used by wnf/_.c so
// the heap cells stay un-mutated for re-fire); the no-suffix variant
// reads the heap and delegates.
fn Term interact_assign     (Term assign_term);
fn Term interact_assign_with(Term dst, Term src);

// === codegen/ + profile/ ===
// Backend-agnostic source emitters (render_c.c, render_metal.c) plus
// per-kid dispatch profiling.  Exposed cross-TU so the Metal .m file
// (compiled separately under -DTHVM_HAS_METAL) can render MSL via
// cg_emit_metal, gate on cg_supports, and record dispatches into the
// shared K_PROFILE table.

// Per-kid route taken by the most recent fire.  Mirrored by the WL
// surface decoder (TKernelDispatchKind) -- keep the names + ids
// stable across TUs.
typedef enum {
  KDISPATCH_NONE        = 0,
  KDISPATCH_BLAS_DOT    = 1,
  KDISPATCH_BLAS_GEMV   = 2,
  KDISPATCH_BLAS_GEMM   = 3,
  KDISPATCH_JIT         = 4,   // CPU JIT (clang -shared)
  KDISPATCH_INTERPRETER = 5,   // CPU interpreter
  KDISPATCH_METAL_JIT   = 6,   // Metal: cg_emit_metal -> MTLLibrary -> single-encoder dispatch
  KDISPATCH_METAL_OP    = 7,   // Metal: per-op shader fallback (one encoder per KProgOp)
} KDispatchKind;

int   cg_supports(KernelEntry const *ke);
u32   cg_program_dtype(KernelEntry const *ke);   // DT_COUNT on mixed
char *cg_emit_metal(KernelEntry const *ke);   // caller frees
u64   cg_now_us(void);
void  cg_profile_record(u32 kid, KDispatchKind kind, u64 elapsed_us);

// === backend/ ===
// CPU backend -- only backend for step 12.  Installed by thvm_init.
// Metal lands in step 14 behind the same Backend struct.
extern Backend CPU_BACKEND;
extern Backend METAL_BACKEND;

// Allocate a borrowed buffer: we don't own `data`, and on release we
// call `on_release(handle)` instead of free().  Used by the WL bridge
// to share a Shared NumericArray's bytes without copying.
fn u32 cpu_buf_alloc_external(void *data, u64 nbytes,
                              void (*on_release)(void *), void *handle);

// === wnf/ ===
// Stack-machine reducer to weak normal form.  See src/wnf/_.c for the
// enter/apply protocol.
//
// `wnf` runs to WHNF (unbounded interactions).  `wnf_n(t, max_steps)`
// bails after `max_steps` ITRS bumps and unwinds the eliminator
// stack via the standard "stuck term" path -- pre-unwind frames are
// snapshotted into WNF_LAST_STACK (length WNF_LAST_STACK_LEN,
// innermost-first) for inspection.  max_steps == 0 == unbounded.
fn Term wnf(Term t);
fn Term wnf_n(Term t, u64 max_steps);
// nf: explicit normal-form reducer.  WHNF (wnf) only surfaces the
// outermost head; nf sweeps the heap, fires every redex via
// redex_fire (covers GRAD, KERNEL, APP-LAM, OP2, etc. uniformly),
// loops until no progress.  Used by thvm_realize so chain-rule-
// produced UOps deep inside a graph reduce naturally.
fn Term nf(Term root);
// WNF_LAST_STACK / WNF_LAST_STACK_LEN live in TContext now -- macros
// at the bottom of this file resolve them.

// === collapse/ ===
// Enumerate the SUP-tree of a term.  Walks via WNF; on TAG_SUP at the
// head, recurses into both branches; TAG_ERA branches are dropped.
// Returns the number of leaves written into `out`, capped at `cap`.
//
// "Shallow" collapse: only follows SUPs the WNF has surfaced to the
// head.  Deep enumeration through APP / OP2 / EQL / ... lands as those
// SUP-commutation interactions are added.
fn u64 thvm_collapse(Term t, Term *out, u64 cap);

// === kbo/ ===
// Knuth-Bendix ordering on first-order terms (TAG_CTR + TAG_FVR).
// Stage 2 of the IC-native ATP roadmap (docs/plans/waldmeister_ic_atp.md).
typedef enum {
  KBO_EQ =  0,
  KBO_GT =  1,
  KBO_LT = -1,
  KBO_UN =  2,    // incomparable
} KboCmp;

// KboConfig: shared (read-only) tables for one signature.  The caller
// owns the memory; thvm_kbo only reads.  precedence is a total order
// on labels (higher value = greater); weights[l] is the per-symbol
// weight for label l; var_weight is the scalar w0 for TAG_FVR.
typedef struct {
  const u32 *weights;
  const u32 *precedence;
  u32        n_labels;
  u32        var_weight;
} KboConfig;

fn KboCmp thvm_kbo(Term s, Term t, const KboConfig *cfg);

// === lpo/ ===
// Lexicographic Path Ordering (LPO; Waldmeister's
// `Lexikografische-Pfad-Ordnung`, Dershowitz 1982).  An
// alternative reduction ordering on TAG_CTR + TAG_FVR terms,
// driven purely by a precedence relation on function symbols
// -- no per-symbol weights.  More discriminating than KBO on
// some rewrite systems; the standard pick for many TPTP-UEQ
// problems.  Stage 8.5b of `docs/plans/waldmeister_ic_atp.md` section 7.8.
typedef enum {
  LPO_EQ =  0,
  LPO_GT =  1,
  LPO_LT = -1,
  LPO_UN =  2,    // incomparable
} LpoCmp;

typedef struct {
  const u32 *precedence;   // higher value = greater symbol
  u32        n_labels;
} LpoConfig;

fn LpoCmp thvm_lpo(Term s, Term t, const LpoConfig *cfg);

// === rewrite/ ===
// One-shot equational rewriter on TAG_CTR + TAG_FVR (stage 3 of
// docs/plans/waldmeister_ic_atp.md).  No recursive descent into
// sub-terms yet -- rules match the top position only.
#define REWRITE_MAX_VAR    64
#define REWRITE_MAX_ARITY  16

typedef struct {
  Term bindings[REWRITE_MAX_VAR];   // 0 = unbound (0 isn't a valid Term)
} RewriteSubst;

// Returns 1 on successful match, populating subst's bindings with the
// matched sub-terms.  Variables seen multiple times must match the
// same sub-term (linear matching).
fn u8   thvm_match            (Term pattern, Term term, RewriteSubst *subst);
fn Term thvm_subst_apply      (Term term, const RewriteSubst *subst);
fn Term thvm_rewrite_step     (Term t, const Term *lhs, const Term *rhs,
                               u32 n_rules);
fn Term thvm_rewrite_normalize(Term t, const Term *lhs, const Term *rhs,
                               u32 n_rules, u32 step_cap);

// === unify/ ===
// Most-general-unifier on TAG_CTR + TAG_FVR terms (Robinson with
// occurs check).  Stage 4 of docs/plans/waldmeister_ic_atp.md.
// Reuses RewriteSubst for the result; caller zero-inits before the
// first call.  Returns 1 on success, 0 if the mgu doesn't exist.
fn u8   thvm_unify        (Term s, Term t, RewriteSubst *subst);
fn Term thvm_rename_vars  (Term t, u32 offset);
fn Term thvm_unify_apply  (Term t, const RewriteSubst *subst);

// === cp/ ===
// Critical-pair enumeration for an oriented rule set (stage 4).
// CriticalPair holds the two terms produced by overlapping rules
// at a non-variable position; both sides should be joinable for the
// system to be locally confluent.
typedef struct {
  Term lhs;
  Term rhs;
} CriticalPair;

fn u32 thvm_critical_pairs(const Term *lhs, const Term *rhs, u32 n_rules,
                           CriticalPair *out, u32 cap);

// Range-restricted CP enumeration.  Generates CPs only for the
// sub-rectangle [start_i, end_i) x [start_j, end_j) of the rule
// set.  Saturation uses this to compute (new x all) + (old x new)
// after a rule add, skipping the redundant (old x old).
fn u32 thvm_critical_pairs_range(const Term *lhs, const Term *rhs, u32 n_rules,
                                 u32 start_i, u32 end_i,
                                 u32 start_j, u32 end_j,
                                 CriticalPair *out, u32 cap);

// === atp/ ===
// Saturation loop state (stage 5).  See
// docs/plans/waldmeister_ic_atp.md section 7.1 for the design.  AtpState is heap-
// allocated by thvm_atp_init; thvm_atp_free reclaims.  Struct
// fields are public; tests / step / run helpers all read directly.
typedef enum {
  ATP_RUNNING     = 0,
  ATP_PROVED      = 1,
  ATP_REFUTED     = 2,
  ATP_TIMEOUT     = 3,
  ATP_QUEUE_EMPTY = 4,
} AtpStatus;

#define ATP_MAX_RULES 256
#define ATP_MAX_CPS   4096
#define ATP_MAX_TRACE 4096

// Reason labels for trace entries (used as the CTR label).
// Each TraceEntry is a TAG_CTR with label = reason and children =
// [NUM(parent_a), NUM(parent_b), lhs, rhs].  Parent index sentinel
// ATP_TRACE_NONE means "no parent" (e.g., for axioms).
#define TRACE_AXIOM    1u   // initial equation pushed via add_equation
#define TRACE_ORIENT   2u   // CP normalized + KBO-oriented into a rule
#define TRACE_CP       3u   // critical pair generated from two rules
#define ATP_TRACE_NONE 0xFFFFFFFFu

// 8.1c: ATP primitives registered into the TAG_PRI table by
// thvm_atp_init.  Tests registers them once; the saturation loop
// in 8.1d-e calls them via APP-PRI evaluation.
//   ATP_PRIM_UNIFY_APPLY:  arity 2; takes (s, t); returns
//     `thvm_unify_apply(s, &subst)` on successful unification, or
//     ERA on failure.
//   ATP_PRIM_UNIFY_APPLY3: arity 3; takes (s, t, target); returns
//     `thvm_unify_apply(target, &subst)` where σ = mgu(s, t), or
//     ERA on failure.  Used by 8.1e-ii's IC-routed CP enumerator
//     to route the per-position unify+apply step through the
//     TAG_PRI machinery.
#define ATP_PRIM_UNIFY_APPLY  0u
#define ATP_PRIM_UNIFY_APPLY3 1u
//   ATP_PRIM_KBO: arity 3; takes `(s, t, cfg_id_NUM)` and returns
//     `NUM(KboCmp)` -- the four-valued KBO comparison result.
//     `cfg_id_NUM` indexes into the process-global
//     `KBO_CFG_TABLE` (set up via `kbo_cfg_register`).
//     Lets IC code (e.g., 8.10's SupGen-style search) invoke the
//     KBO comparator from inside an APP-PRI evaluation chain.
//   ATP_PRIM_KBO_EQ_IC: arity 2; takes `(s, t)` and returns
//     `NUM(1)` if structurally equal, `NUM(0)` otherwise.
//     Implemented via IC-native structural recursion: the C body
//     handles the leaf cases (FVR equality, NUM comparison) and
//     for CTR builds an AND chain of self-recursive APP-PRI calls
//     -- the wnf reducer fires each child comparison in turn,
//     AND short-circuits on the first NUM(0).  Stage 8.2c proof
//     point that pure-IC structural recursion is viable.
//   ATP_PRIM_REWRITE_STEP: arity 3; takes `(lhs, rhs, target)`
//     and returns either `thvm_subst_apply(rhs, &σ)` on
//     successful match (where σ = match(lhs, target)) or `ERA`
//     on failure.  Dispatch entry-point for IC-native rule
//     application per `docs/plans/waldmeister_ic_atp.md` section 7.5's
//     Strategy B; combined with APP-SUP fan-out (8.3c) it lets
//     a SUP of partial-PRI rules run in parallel.
#define ATP_PRIM_KBO          2u
#define ATP_PRIM_KBO_EQ_IC    3u
#define ATP_PRIM_REWRITE_STEP 4u

// 8.2b: process-global registry mapping `cfg_id` (u32) to
// `const KboConfig *`.  Needed because `KboConfig*` doesn't fit
// cleanly in a Term's `val` field; `prim_kbo` looks up the
// pointer at fire time.
#define KBO_CFG_TABLE_CAP 16
fn u32                kbo_cfg_register(u32 cfg_id, const KboConfig *cfg);
fn const KboConfig   *kbo_cfg_get     (u32 cfg_id);

typedef struct {
  // Rule set R: parallel arrays sized for thvm_rewrite_normalize /
  // thvm_critical_pairs to consume directly.  r_trace[i] is the
  // trace-entry index that produced rule i (TRACE_ORIENT for rules
  // added by atp_step; ATP_TRACE_NONE for rules manually injected
  // by tests / setup code that bypassed the saturation pipeline).
  Term lhs[ATP_MAX_RULES];
  Term rhs[ATP_MAX_RULES];
  u32  r_trace[ATP_MAX_RULES];
  u32  n_rules;

  // CP queue (open-form: not INC-wrapped here; the priority encoding
  // happens at selection time in thvm_atp_select).  cp_trace[i]
  // holds the trace-entry index that birthed cp[i] (TRACE_AXIOM
  // for queued axioms, TRACE_CP for generated CPs in 6.1c, or
  // ATP_TRACE_NONE if tracing is disabled / unavailable).
  Term cp_lhs[ATP_MAX_CPS];
  Term cp_rhs[ATP_MAX_CPS];
  u32  cp_trace[ATP_MAX_CPS];
  u32  n_cps;

  // Transient: set by thvm_atp_select_cp to the trace-entry index
  // of the popped CP; consumed by thvm_atp_step right after the
  // pop so orient_and_add's TRACE_ORIENT entry can record the
  // source CP as its parent.
  u32  last_popped_trace;

  // Goal: a single conjecture goal_lhs == goal_rhs.  goal_lhs == 0
  // means "no goal set; run as completion".
  Term goal_lhs;
  Term goal_rhs;

  // Reduction ordering (caller-owned).  When `lpo` is non-NULL,
  // it takes precedence over `kbo` per Choice C of
  // `docs/plans/waldmeister_ic_atp.md` section 7.8: orient_and_add dispatches to
  // `thvm_lpo`.  When both are NULL, every comparison is
  // incomparable (KBO_UN) -- saturation falls into unfailing
  // fallback.
  const KboConfig *kbo;
  const LpoConfig *lpo;

  // Bounds.
  u32 step;
  u32 step_cap;

  // Proof trace.  Stage 6.1: each entry is a TAG_CTR (see the
  // TRACE_* labels above).  6.1b/c wire this into add_equation,
  // orient_and_add, and generate_cps; 6.2 walks it to emit a
  // PCL-shaped serialization.
  Term trace[ATP_MAX_TRACE];
  u32  n_trace;

  // Stage 7.1: count of CPs dropped at generate-time because both
  // sides normalize to the same term under current R (trivial
  // joinability -- Waldmeister's `Grundzusammenfuehrung`,
  // "ground-merging" criterion).  Useful for benchmarking the
  // pruning power; not consulted by saturation logic.
  u32  n_cps_dropped_joinable;

  // Stage 7.2b: count of CPs that are source-rule-disjoint
  // connected (joinable under R \ {rule_a, rule_b}, the two rules
  // that birthed the CP).  Per the domination lemma in
  // `docs/plans/waldmeister_ic_atp.md` section 7.2, this is bounded above
  // by `n_cps_dropped_joinable`; it ticks unconditionally for
  // measurement, even if 7.1's filter would also fire.  Useful
  // infrastructure for stage 7.4+ when AC theories may break
  // the domination.
  u32  n_cps_dropped_connected;

  // Stage 7.3a: count of CPs that are rule-subsumed -- there
  // exists `(l, r) ∈ R` and substitution σ such that
  // `(lhs, rhs) = (σl, σr)` (or the symmetric case).  Same
  // domination story as 7.2b: rule subsumption fires only when
  // the rule rewrites lhs directly to rhs in one step, which
  // 7.1 already catches via full normalization.  Counter ticks
  // unconditionally for empirical measurement; not a filter.
  u32  n_cps_dropped_rule_subsumed;

  // Stage 7.3b: count of CPs dropped because they are subsumed
  // by an already-queued CP `(s', t')` -- there exists σ such
  // that `(lhs, rhs) = (σs', σt')` (or symmetric).  Genuinely
  // orthogonal to 7.1 (the queue does not participate in
  // normalization), so this IS a filter -- the candidate is
  // discarded.
  u32  n_cps_dropped_queue_subsumed;

  // Stage 8.1e-i: feature flag.  When 0 (default), `thvm_atp_
  // generate_cps` runs the C-side critical-pair enumerator
  // directly.  When 1, it dispatches to `thvm_atp_generate_cps_ic`,
  // which routes the per-pair unification through the TAG_PRI /
  // APP-PRI machinery (8.1c).  The IC path is currently a
  // no-op wrapper (stage 8.1e-i) -- 8.1e-ii lands the actual
  // SUP+PRI routing.  Bench analysis in 8.1e-iii.
  u8   use_ic_cp_gen;

  // Stage 8.3e-i: feature flag for IC-routed rewriting.  When 0
  // (default), AtpState-internal callers use `thvm_rewrite_
  // normalize` directly.  When 1, they go through
  // `atp_rewrite_normalize` shim that dispatches to the IC path.
  // 8.3e-i lands the flag + plumbing with the IC path stubbed
  // to delegate to C; 8.3e-ii replaces the body with PRI routing
  // via `prim_rewrite_step`.
  u8   use_ic_rewrite;

  // Stage 8.8: CP-priority heuristic flag.  0 (default) uses the
  // `--add` heuristic from 5.3 -- priority = symbol_count(lhs)
  // + symbol_count(rhs), cheapest-first.  1 uses Waldmeister's
  // `--mix` heuristic, which adds a penalty for CPs that fail
  // to orient cleanly under the active KBO/LPO config (i.e.,
  // would land in unfailing-fallback).  The penalty makes the
  // saturator prefer CPs whose orientation is unambiguous --
  // typically a small win on hard problems.
  u8   use_mix_heuristic;

  // 8.4d: optional WaldSpec for sort-check gating in
  // `thvm_atp_add_equation` and `thvm_atp_set_goal`.  When NULL
  // (default), no sort checking happens (homogeneous-mode
  // behavior preserved).  Set via `thvm_atp_set_spec`.
  const struct WaldSpec *spec;

  // 8.9b: witness substitution accumulated during narrowing.
  // Populated by `thvm_atp_narrow_step` on each successful
  // narrow; queried via `thvm_atp_get_witness(s, var_id)`.
  // Contents are meaningful only when `goal_existential` is
  // set (8.9c) and the saturator has run a narrowing-mode
  // proof.  v0 stores raw bindings without filtering;
  // user-declared witness var ids stay in
  // `witness_var_ids[0..n_witness_vars)` for the WL surface
  // to use as a filter when reading back.
  RewriteSubst witness_subst;

  // 8.9c: goal-mode flag.  When 0 (default), `goal_check` uses
  // the existing rewrite-and-compare path: PROVED iff both sides
  // normalize to structurally equal terms.  When 1, `goal_check`
  // uses a narrow-and-extract path: tries `thvm_atp_narrow_step`
  // up to a small budget; PROVED iff both sides become
  // structurally equal (after sigma-application accumulated in
  // `witness_subst`).
  u8   goal_existential;
} AtpState;

fn AtpState *thvm_atp_init        (const KboConfig *cfg, u32 step_cap);
fn void      thvm_atp_free        (AtpState *s);
fn u8        thvm_atp_add_equation(AtpState *s, Term lhs, Term rhs);
fn u8        thvm_atp_set_goal    (AtpState *s, Term lhs, Term rhs);

// 8.4d: attach a WaldSpec for sort-check gating.  When set,
// `thvm_atp_add_equation` and `thvm_atp_set_goal` reject
// ill-sorted inputs (return 0) without modifying state.  Pass
// NULL to clear (homogeneous-mode default).
fn void      thvm_atp_set_spec    (AtpState *s,
                                   const struct WaldSpec *spec);

// 8.5c: attach an LpoConfig.  When set, `thvm_atp_orient_and_add`
// uses LPO instead of KBO.  Pass NULL to revert to KBO (the
// default).
fn void      thvm_atp_set_lpo     (AtpState *s, const LpoConfig *lpo);

// 8.10b: top-K peek into the CP queue.  Reuses the existing
// INC-priority + collapse_ordered pipeline from
// `thvm_atp_select_cp` but does NOT pop -- the queue is left
// unchanged.  Writes the top `k` cheapest CPs (or `n_cps` if
// fewer) into `out_lhs[]` / `out_rhs[]` in priority order
// (cheapest first).  Returns the actual count peeked.
//
// Useful for branchless lookahead heuristics, multi-CP batch
// processing, and debugging the priority ordering.
fn u32       thvm_atp_peek_top_k  (AtpState *s, u32 k,
                                   Term *out_lhs, Term *out_rhs);

// 8.9b: narrowing primitives.  `thvm_atp_narrow_step` walks every
// non-variable position of `lhs` and `rhs` (in that order),
// trying to unify each subterm with each rule's LHS.  On first
// success, it writes the σ-applied lhs / rhs into `out_lhs` /
// `out_rhs`, accumulates the binding into `witness->bindings[]`,
// and returns 1.  Returns 0 if no narrow step applies.
//
// `thvm_atp_get_witness(s, var_id)` reads
// `s->witness_subst.bindings[var_id]` (or 0 if unbound).
fn u8        thvm_atp_narrow_step (AtpState *s, Term lhs, Term rhs,
                                   Term *out_lhs, Term *out_rhs,
                                   RewriteSubst *witness);
fn Term      thvm_atp_get_witness (const AtpState *s, u32 var_id);

// 9.1b: bounded depth-first multi-witness narrowing.  Same starting
// (lhs, rhs) as `thvm_atp_narrow_step`, but enumerates up to
// `max_witnesses` successful witness paths within `max_depth` narrow
// steps each.  At every node, tries every (position, rule) pair on
// both sides; on a successful unification recurses with the
// sigma-applied terms and the composed substitution.  A leaf with
// `kbo_eq(lhs, rhs)` emits the accumulated subst into `witnesses[]`.
// Returns the number of witnesses written (<= max_witnesses).
//
// Stateless w.r.t. `s->witness_subst`: leaves `s` unchanged.
// `witnesses[]` must hold at least `max_witnesses` `RewriteSubst`
// slots.  v0 returns DFS-order raw witnesses without alpha-equivalent
// dedup; callers post-filter if needed.  See
// `docs/plans/waldmeister_ic_atp.md` section 7.4 for the algorithm sketch.
fn u32       thvm_atp_narrow_all  (AtpState *s, Term lhs, Term rhs,
                                   u32 max_depth, u32 max_witnesses,
                                   RewriteSubst *witnesses);

// 9.3: heap checkpoint/reset for saturation memory hygiene.
// IC-routed rewrites (use_ic_rewrite=1) allocate many cells per
// step; over a long saturation those accumulate into the 16M-cell
// HEAP_CAP.  Many of them are dead by the time the step completes
// (e.g. CPs that turn out to be trivially joinable -- the
// normalized lhs/rhs are not referenced afterward).
//
// `thvm_atp_heap_checkpoint()` snapshots HEAP_NEXT;
// `thvm_atp_heap_reset(c)` pops back, reclaiming the cells in
// between.  Caller is responsible for ensuring no live Term
// references those cells before resetting -- the saturation step
// resets only on the discard path (CP becomes trivially joined),
// where neither l nor r is used downstream.
fn u64       thvm_atp_heap_checkpoint(void);
fn void      thvm_atp_heap_reset     (u64 checkpoint);

// 8.9c: set an existential conjecture.  Sets goal_lhs / goal_rhs
// AND flips `s->goal_existential = 1` so `thvm_atp_goal_check`
// uses the narrowing path.  FVRs in lhs / rhs are interpreted
// existentially -- σ is solved for their bindings.  Caller
// queries the result via `thvm_atp_get_witness`.
//
// Returns 1 on success, 0 if the spec gate (8.4d) rejected the
// goal.  Like `thvm_atp_set_goal`, lhs == 0 clears (sets the
// flag back to 0).
fn u8        thvm_atp_set_goal_existential(AtpState *s,
                                           Term lhs, Term rhs);

// Serialize the trace[] array as Waldmeister-PCL-shaped text into
// `buf` (cap = capacity).  Each line: "<idx> (<reason> [from
// <p_a>[, <p_b>]]): <lhs> = <rhs>".  Term pretty-printer handles
// TAG_CTR / FVR / NUM / ERA; other tags render as "?T<tag>".
// Truncates silently on buffer overflow; caller can compare the
// returned byte count against `cap - 1` to detect that case.
fn u32       thvm_atp_trace_serialize(const AtpState *s, char *buf, u32 cap);

// === wald/ ===
// Parser for Waldmeister .pr-style spec files.  Stage 6.3 of
// docs/plans/waldmeister_ic_atp_tasks.md.  WaldSpec holds the
// parsed signature + variable table + equations + single
// conclusion goal; downstream feeds it to thvm_atp_run.
#define WALD_MAX_SYMBOLS 64
#define WALD_MAX_VARS    32
#define WALD_MAX_EQNS    64
#define WALD_NAME_LEN    32
#define WALD_MAX_SORTS   16   // 8.4b: max distinct sorts in a spec
#define WALD_MAX_ARITY   8    // 8.4b: max function-symbol arity

typedef struct {
  char name[WALD_NAME_LEN];
  u32  label;       // CTR label assigned at parse time
  u32  arity;
  u32  prec_rank;   // 6.3c5: precedence position (0 = smallest;
                    //   higher index = greater).  All symbols start
                    //   at 0; ORDERING parser fills the chain in.
  // 8.4b: per-symbol sort metadata.  arg_sorts[0..arity) are sort
  // ids (indices into spec->sorts[]).  result_sort is the sort
  // produced by this symbol.  For `.pr` files without a SORTS
  // section, sort ids are auto-registered on first reference.
  u32  arg_sorts[WALD_MAX_ARITY];
  u32  result_sort;
} WaldSym;

typedef struct {
  char name[WALD_NAME_LEN];
  u32  var_id;    // FVR id assigned at parse time
  u32  sort;      // 8.4b: sort id (index into spec->sorts[])
} WaldVar;

// 8.5d: ordering kind declared in the `.pr` file's ORDERING
// section.  0 = KBO (default), 1 = LPO.  Captured at parse time
// so callers can build the right config for the saturator.
#define WALD_ORDER_KBO 0u
#define WALD_ORDER_LPO 1u

typedef struct WaldSpec {
  // Spec identity.  `mode_proof = 1` for "MODE PROOF", 0 for
  // "MODE COMPLETION" (defaults to 1 when unspecified).
  char    name[WALD_NAME_LEN];
  u8      mode_proof;
  u8      ordering_kind;   // 8.5d: WALD_ORDER_KBO / WALD_ORDER_LPO

  // Signature: `symbols[0..n_symbols)` with monotonically-assigned
  // CTR labels via `next_label` (starts at 1; 0 is the CTR
  // "anonymous tuple" label so we skip it).
  WaldSym symbols[WALD_MAX_SYMBOLS];
  u32     n_symbols;
  u32     next_label;

  // 8.4b: sort name table.  Sort id 0..n_sorts-1 indexes into
  // `sorts[]`.  Populated by `wald_parse_sorts` from the SORTS
  // section, and lazily by `wald_sort_id_or_register` when
  // SIGNATURE / VARIABLES references a sort name not yet in the
  // table.  `n_sorts == 0` means "homogeneous mode": no sort
  // checking happens; all symbols / variables default to sort 0.
  char    sorts[WALD_MAX_SORTS][WALD_NAME_LEN];
  u32     n_sorts;

  // Variables: each gets a sequential FVR id.
  WaldVar vars[WALD_MAX_VARS];
  u32     n_vars;

  // Equations: parallel arrays; the saturation engine consumes
  // these via thvm_atp_add_equation in order.
  Term    eqn_lhs[WALD_MAX_EQNS];
  Term    eqn_rhs[WALD_MAX_EQNS];
  u32     n_eqns;

  // Single conjecture for proof mode.  goal_lhs == 0 means no
  // goal was specified (completion mode or empty CONCLUSION).
  Term    goal_lhs;
  Term    goal_rhs;

  // 8.9d: existential variables for narrowing.  Names in the
  // EXISTS section are looked up against vars[] and their FVR
  // ids are stored here.  When n_existential > 0, downstream
  // (bench harness, WL bridge) should use
  // `thvm_atp_set_goal_existential` instead of
  // `thvm_atp_set_goal` to put the saturator in narrow mode.
  u32     existential_var_ids[REWRITE_MAX_VAR];
  u32     n_existential;
} WaldSpec;

fn WaldSpec *wald_init(void);
fn void      wald_free(WaldSpec *s);

// 6.3b lexer.  WT_END is the sentinel; WT_ERR signals an
// unexpected character.  WT_IDENT carries text in lex->tok_text
// (NUL-terminated; truncated to fit WALD_NAME_LEN-1).
typedef enum {
  WT_END    = 0,
  WT_IDENT  = 1,
  WT_COLON  = 2,
  WT_ARROW  = 3,   // ->
  WT_EQ     = 4,
  WT_LPAREN = 5,
  WT_RPAREN = 6,
  WT_COMMA  = 7,
  WT_GT     = 8,
  WT_ERR    = 9,
} WaldTokKind;

typedef struct {
  const char *src;
  u32         pos;
  u32         len;
  // Last-read token text (only valid for WT_IDENT); NUL-terminated.
  char        tok_text[WALD_NAME_LEN];
  u32         tok_len;
  // 1-token peek: lookahead saved by wald_lex_peek; consumed on
  // the next wald_lex_next call.  When have_peek == 1, the next
  // call returns peeked_* and clears have_peek.
  u8           have_peek;
  WaldTokKind  peeked_kind;
  char         peeked_text[WALD_NAME_LEN];
  u32          peeked_len;
} WaldLex;

fn void        wald_lex_init(WaldLex *lex, const char *src);
fn WaldTokKind wald_lex_next(WaldLex *lex);
fn WaldTokKind wald_lex_peek(WaldLex *lex);

// 6.3c1: section-detect infrastructure.  WSEC_NONE means "no
// section / EOF / unknown ident"; the rest map 1:1 to .pr file
// sections.
typedef enum {
  WSEC_NONE       = 0,
  WSEC_NAME       = 1,
  WSEC_MODE       = 2,
  WSEC_SORTS      = 3,
  WSEC_SIGNATURE  = 4,
  WSEC_VARIABLES  = 5,
  WSEC_ORDERING   = 6,
  WSEC_EQUATIONS  = 7,
  WSEC_CONCLUSION = 8,
  WSEC_EXISTS     = 9,   // 8.9d: existential-variable declaration
} WaldSection;

fn WaldSection wald_section_from_ident(const char *name);
fn WaldSection wald_skip_to_section   (WaldLex *lex);

// 6.3c2: per-section parsers for the simple text sections.  Each
// takes a `WaldSpec *` (may be NULL to discard the parsed value),
// expects the lexer to be positioned just past the section header,
// reads the section content, and returns the next section's enum
// (or WSEC_NONE on EOF).  Falls back through `wald_skip_to_section`
// on unrecognized content.
// 8.4b: look up a sort name in spec->sorts[] and return its id;
// register a new entry if the name isn't present and there's
// room.  Returns WALD_MAX_SORTS on overflow (sentinel; caller
// can treat this as "no sort" or fail).
fn u32         wald_sort_id_or_register(WaldSpec *spec,
                                        const char *name, u32 len);

// 8.4c: top-down sort inference / verification.  Returns the
// sort id of `t` if well-sorted under `spec`, or WALD_MAX_SORTS
// (sentinel) on any sort mismatch / unknown symbol or variable.
//
// When `spec == NULL` or `spec->n_sorts == 0` (homogeneous mode),
// returns 0 unconditionally so existing single-sort fixtures
// continue to pass.
fn u32         wald_term_sort  (const WaldSpec *spec, Term t);

// 8.4c: convenience predicate -- 1 if well-sorted, 0 otherwise.
fn u8          wald_sort_check (const WaldSpec *spec, Term t);

fn WaldSection wald_parse_name     (WaldSpec *spec, WaldLex *lex);
fn WaldSection wald_parse_mode     (WaldSpec *spec, WaldLex *lex);
fn WaldSection wald_parse_sorts    (WaldSpec *spec, WaldLex *lex);
fn WaldSection wald_parse_signature(WaldSpec *spec, WaldLex *lex);
fn WaldSection wald_parse_variables(WaldSpec *spec, WaldLex *lex);
fn WaldSection wald_parse_ordering (WaldSpec *spec, WaldLex *lex);

// 6.3d: parse one term.  Returns 0 (invalid Term) on parse error
// (unknown ident, missing close paren, arity mismatch, ...).
fn Term        wald_parse_term     (WaldSpec *spec, WaldLex *lex);

// 6.3e: parse a sequence of `term = term` pairs.  EQUATIONS pushes
// onto spec->eqn_lhs/rhs[].  CONCLUSION stores in spec->goal_lhs/rhs;
// only the FIRST conclusion lands (subsequent pairs parsed but
// discarded -- matches the proof-mode constraint of one conjecture).
fn WaldSection wald_parse_equations (WaldSpec *spec, WaldLex *lex);
fn WaldSection wald_parse_conclusion(WaldSpec *spec, WaldLex *lex);

// 8.9d: parse `EXISTS x, y, ...` -- list of variable names that
// are to be treated existentially in the conjecture.  Each name
// is looked up against `spec->vars[]` and its FVR id stored in
// `spec->existential_var_ids[]`.  Names not in vars[] are
// silently skipped (the parser is permissive throughout).
fn WaldSection wald_parse_exists    (WaldSpec *spec, WaldLex *lex);

// 6.3f: top-level driver.  Returns WALD_OK on success or one of the
// WaldErr codes on structural failure.  Per-section parse errors
// inside a section don't bail -- they fall through via
// `wald_skip_to_section` so we still consume the rest of the file
// and end up with a partial-but-coherent spec.
typedef enum {
  WALD_OK             = 0,
  WALD_ERR_NULL       = 1,   // src or spec is NULL
  WALD_ERR_NO_SECTION = 2,   // input contained no recognizable section keyword
  WALD_ERR_FILE       = 3,   // 6.4a: open/read failure in wald_parse_file
} WaldErr;

fn WaldErr wald_parse(const char *src, WaldSpec *spec);

// 6.4a: file-loader convenience wrapper.  Opens `path`, slurps the
// whole file into a heap buffer, calls `wald_parse` on it, and frees
// the buffer.  Returns WALD_ERR_NULL if path/spec is NULL,
// WALD_ERR_FILE on open/read/alloc failure, or whatever `wald_parse`
// returns otherwise.  Suitable for a file the size of a Waldmeister
// `.pr` spec (KBs, not MBs); slurping the whole thing is fine.
fn WaldErr wald_parse_file(const char *path, WaldSpec *spec);

// Pop the next CP off the queue.  FIFO for now; 5.3 upgrades to
// priority-collapse over INC-wrapped CPs.  Returns 1 on success
// (out-params populated), 0 if the queue is empty.
fn u8        thvm_atp_select_cp   (AtpState *s, Term *lhs_out, Term *rhs_out);

// Index range of rules just added by orient_and_add.
//   {first: 0, count: 0}     -> nothing added (KBO_EQ, or R full)
//   {first: i, count: 1}     -> one rule at s->lhs[i] / s->rhs[i]
//   {first: i, count: 2}     -> two rules at i, i+1 (unfailing fallback)
typedef struct {
  u32 first;
  u32 count;
} AtpAddedRange;

// Orient `lhs == rhs` (assumed already reduced to NF and not
// trivially equal) and push the resulting rule(s) onto R.
//   KBO_GT -> add `lhs -> rhs`               (count = 1)
//   KBO_LT -> add `rhs -> lhs` (swap)         (count = 1)
//   KBO_EQ -> caller bug; returns count = 0  (caller should have
//             trivialized via kbo_eq first)
//   KBO_UN -> unfailing fallback: add both orientations (count = 2)
// Returns count = 0 if R doesn't have room for the rule(s).
fn AtpAddedRange thvm_atp_orient_and_add(AtpState *s, Term lhs, Term rhs);

// Generate fresh CPs after an orient_and_add: enumerate (new x all_R)
// + (old x new), push survivors onto the CP queue.  Returns count
// pushed.  Drops overflow silently.
fn u32 thvm_atp_generate_cps(AtpState *s, AtpAddedRange added);

// Interreduce: walk the older rules in R and drop any whose LHS
// reduces under the freshly-added rule(s); requeue each dropped
// rule's simplified equation onto the CP queue.  Top-only today;
// 5.4's recursive descent widens coverage automatically.  Returns
// the number of older rules dropped.
fn u32 thvm_atp_interreduce(AtpState *s, AtpAddedRange added);

// Goal check: normalize both sides of s->goal_{lhs,rhs} under R,
// return ATP_PROVED if they collide; ATP_RUNNING otherwise (also
// when goal_lhs == 0, i.e. completion mode).
fn AtpStatus thvm_atp_goal_check(AtpState *s);

// One saturation step: goal_check -> select -> normalize ->
// trivialize -> orient+add -> interreduce -> generate_cps ->
// goal_check.  Returns ATP_PROVED / ATP_TIMEOUT /
// ATP_QUEUE_EMPTY / ATP_RUNNING (continue).
fn AtpStatus thvm_atp_step(AtpState *s);

// Drive thvm_atp_step until it returns non-RUNNING.
fn AtpStatus thvm_atp_run (AtpState *s);

// Redex inspection / single-redex firing for the debugger interface.
// is_redex predicate; redex_fire dispatches the matching interaction
// and returns the result Term (0 if validation fails -- the input
// wasn't a redex any more).  redex_fire ALSO patches every heap cell
// still holding the old redex Term to point at the result, so
// nested redexes get their parent slots updated automatically.
// redex_enumerate scans the live heap for distinct redex Terms.
fn u8   is_redex(Term t);
fn Term redex_fire(Term redex);
fn u32  redex_enumerate(Term *roots, u32 n_roots, Term *out, u32 cap);

// Kernel-program hash-cons cache.  Reset from thvm_init.  Lookup
// returns the cached program pointer + n_ops on hit (NULL on
// miss); insert copies into a tight cache-owned buffer and
// returns its pointer (or NULL if the cache is full).  Both
// pointers are cache-owned; the caller marks its KernelEntry
// with program_shared=1 to suppress double-free.
fn void     kernel_program_cache_reset(void);
fn KProgOp *kernel_program_cache_lookup(KProgOp const *prog, u32 n_ops,
                                        u32 *out_n_ops);
fn KProgOp *kernel_program_cache_insert(KProgOp const *prog, u32 n_ops);
fn u32      kernel_program_cache_size(void);

// Slot-bearing variants used by Phase 16 per-program-shape opt
// sharing: materialize parks `&slot->axes` into KernelEntry.axes
// so every kid emitted with the same KProgOp[] reads/writes the
// same KernelAxes.  Apply once -> propagates to all sharing kids;
// the C-side proposer attaches opts to a program shape, not a kid.
typedef struct KpCacheSlot KpCacheSlot;
fn KpCacheSlot *kernel_program_cache_lookup_slot(KProgOp const *prog, u32 n_ops);
fn KpCacheSlot *kernel_program_cache_insert_slot(KProgOp const *prog, u32 n_ops);

// Lambda-bound-variable shape annotation table.  Populated by
// `TLamShape[shape, body]` at the WL surface so a TVAR(lam_loc)
// can be shape-resolved before APP-LAM beta substitutes a
// concrete value.  Used by term_shape_in (and downstream by
// materialize visit() to emit KSRC_AS_INPUT for shape-known
// TVARs).  Reset from thvm_init.  Propagated through
// clone_to_book_rec (dyn -> book) and alo_realize (book -> dyn).
fn void lam_shape_reset(void);
fn void lam_shape_set(u64 lam_loc, Shape const *shape);
fn int  lam_shape_lookup(u64 lam_loc, Shape *out);
fn void lam_shape_set_book(u64 book_loc, Shape const *shape);
fn int  lam_shape_lookup_book(u64 book_loc, Shape *out);
fn u32  lam_shape_count(void);

// Optional incremental worklist for nf.  When attached, redex_fire
// pushes locally-fresh redexes (the result + every newly allocated
// cell whose content is a redex) into the buffer, advancing *n_ptr.
// nf attaches before its main loop and detaches after.  Other
// callers (WL TInteract, TStep) leave the worklist detached and
// pay zero overhead.
fn void redex_worklist_attach(Term *buf, u32 *n_ptr, u32 cap);
fn void redex_worklist_detach(void);

// === runtime lifecycle ===
void thvm_init(void);
void thvm_free(void);

#endif // THVM_H
