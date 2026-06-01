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

// glibc gates POSIX symbols (CLOCK_MONOTONIC, popen/pclose, fileno,
// strdup) behind a feature-test macro when the TU is compiled with a
// strict `-std=c11`.  Request the POSIX.1-2008 surface before any
// system header is pulled in -- must precede the #includes below.
//
// Linux only: on macOS, defining _POSIX_C_SOURCE *hides* the BSD /
// Darwin extensions (e.g. _SC_NPROCESSORS_ONLN in aot/worker.c), and
// macOS libc already exposes the POSIX symbols unconditionally.  So
// gate the define on __linux__.
#if defined(__linux__) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>        // INFINITY / fabsf etc. (uop_walk reduce init)
#include <stdatomic.h>   // _Atomic typing for the per-context counters

// Windows (CPU-only mingw cross-build) lacks the POSIX/glibc functions
// the runtime uses; map them to Win32 equivalents.  No-op elsewhere.
#include "util/portable_win.h"

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

// === Dynamic-label SUP / DUP (HVM4 DSU / DDU) ===
// DSU(lab_term, a, b): like SUP{a,b} but the label is a *term* that
// must be reduced first.  Once the label resolves to NUM(n), DSU
// becomes SUP^n{a,b}.  Other resolutions: ERA -> ERA, SUP -> nested
// SUP via cross-product on (a, b).
//   val = heap loc; heap[val..val+2] = [lab_term, a, b]
// DDU(lab_term, val, body): dual of DSU; once label resolves to
// NUM(n), DDU becomes DUP^n{x0,x1}=val with body applied to (x0,x1).
//   val = heap loc; heap[val..val+2] = [lab_term, value, body]
#define TAG_DSU         29
#define TAG_DDU         30

// Frames pushed when wnf descends into a DSU's / DDU's label cell.
//   TAG_F_DSU_LAB ext = 0  val = DSU heap loc; on resume, label's WHNF
//                          is on top of stack -- dispatch into the
//                          DSU-{NUM,ERA,SUP,...} rule.
//   TAG_F_DDU_LAB ext = 0  val = DDU heap loc; symmetric.
#define TAG_F_DSU_LAB   31
#define TAG_F_DDU_LAB   32

// === Book-time projections (HVM4 BJ0 / BJ1) ===
// Mirror of TAG_DP0/DP1 but Levy-opaque under BOTH wnf and cnf -- they
// never fire DUP-XXX in place.  Inserted by parse-time auto-dup into
// book templates; alo_realize unfolds them into fresh dyn-heap
// TAG_DP0/TAG_DP1 cells per book copy via alo_dup_share so recursive
// bodies stay bounded.
//
// Field layout matches TAG_DP0/TAG_DP1 (val = dup loc, ext = label).
// DUP_GRAD_FLAG never appears on a BJ -- those stay on plain DPs.
#define TAG_BJ0         33
#define TAG_BJ1         34

#define TAG_COUNT 35

// === OP2 opcodes (TAG_OP2 ext field) ===
#define OP_ADD  0
#define OP_SUB  1
#define OP_MUL  2
#define OP_EQ   3   // returns NUM(1) for equal, NUM(0) otherwise
#define OP_LT   4   // less-than: NUM(1) if x<y else NUM(0)
#define OP_DIV  5   // unsigned div; OP_DIV by 0 -> NUM(0)
#define OP_MOD  6   // unsigned mod; OP_MOD by 0 -> NUM(0)
#define OP_XOR  7   // bitwise xor
#define OP_AND  8   // bitwise and
#define OP_OR   9   // bitwise or
#define OP_SHL  10  // left shift (yv & 31)
#define OP_SHR  11  // right shift (yv & 31)
#define OP_GT   12  // NUM(1) if x>y
#define OP_LE   13  // NUM(1) if x<=y
#define OP_GE   14  // NUM(1) if x>=y
#define OP_NE   15  // NUM(1) if x!=y

// === Dtypes ===
//
// Mirrors tinygrad's full dtype set (TinyHVM/tinygrad/tinygrad/dtype.py:
// 130-146) plus packed int4/uint4 for modern quantization.  Enum
// values are stable; new dtypes append at the bottom.  Reserved
// slots have itemsize=0 in src/dtype/info.c -- the call site aborts
// loudly so missing rows surface during incremental rollout.
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
#define UOP_REDUCE      17   // heap = [src, NUM(kind), NUM(n_axes), NUM(axis_0), ..., NUM(axis_{n-1})]
                             // Multi-axis REDUCE: src reduces over ALL n_axes
                             // simultaneously (matches tinygrad uop/ops.py
                             // Ops.REDUCE with arg=(op,()) + ranges in src[1:]
                             // and schedule/indexing.py:90 convert_reduce_to_reduce_with_ranges).
                             // n_axes==1 case retains 4-cell layout
                             // [src, kind, n_axes=1, axis_0] -- builder
                             // uop_reduce(kind, axis, src) wraps n=1.
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
// === Symbolic INDEX layer ===
// These opcodes give the UOp DAG a symbolic-address representation
// (per-axis ranges, integer arithmetic, conditional WHERE, INVALID
// sentinel for PAD masks, INDEX_E nodes pairing a buffer with a
// symbolic offset expression).
#define UOP_RANGE       25   // heap = [NUM(axis_type), NUM(extent)]; ext = axis_id.
                             //   Symbolic axis-iter leaf.  axis_type
                             //   uses the KAX_LOOP/REDUCE/UPCAST/UNROLL/
                             //   LOCAL/GLOBAL/GROUP_REDUCE encoding from
                             //   the KAX_* defines.
#define UOP_INDEX_E     26   // heap = [buffer_src, addr_expr]; symbolic INDEX.
                             //   buffer_src is a UOp tensor source; addr_expr
                             //   is a tree of UOP_I*/UOP_RANGE giving the
                             //   element offset.  Mirrors S_INDEX_E.
#define UOP_IADD        27   // heap = [a, b]; signed integer add.
#define UOP_ISUB        28   // heap = [a, b]; signed integer subtract.
#define UOP_IMUL        29   // heap = [a, b]; signed integer multiply.
#define UOP_IDIV        30   // heap = [a, b]; truncating signed divide.
#define UOP_IMOD        31   // heap = [a, b]; signed modulo.
#define UOP_ILT         32   // heap = [a, b]; less-than -> 0/1.
#define UOP_IAND        33   // heap = [a, b]; bitwise AND (boolean conjunction on 0/1).
#define UOP_IOR         41   // heap = [a, b]; bitwise OR.
#define UOP_IXOR        42   // heap = [a, b]; bitwise XOR.
#define UOP_IWHERE      34   // heap = [cond, then_v, else_v]; ternary select.
#define UOP_INVALID     35   // heap = [NUM(0)]; sentinel for PAD masking.
                             //   `IWHERE(in_bounds, load(...), INVALID)` is
                             //   the canonical PAD lowering; downstream
                             //   simplifier folds `LOAD(INVALID)` to the
                             //   reduce identity.
// === Buffer / scope layer (mirrors tinygrad's Ops.BUFFER) ===
// UOP_BUFFER is the explicit buffer-leaf opcode.  Scope
// distinguishes device-global storage from threadgroup-shared and
// per-thread register fragments.  T.Tensor argument becomes
// BUFFER(GLOBAL); T.alloc_shared becomes BUFFER(LOCAL);
// T.alloc_fragment becomes BUFFER(REG).
#define UOP_SCOPE_GLOBAL  0   // device memory (T.Tensor argument; default)
#define UOP_SCOPE_LOCAL   1   // threadgroup-shared (T.alloc_shared)
#define UOP_SCOPE_REG     2   // per-thread register fragment (T.alloc_fragment)
#define UOP_BUFFER      36   // heap = [NUM(scope), NUM(dtype), NUM(ndim),
                             //         NUM(d0), ..., NUM(d_{ndim-1})];
                             //   ext = UOP_BUFFER opcode.  Hash-cons by
                             //   (scope, dtype, ndim, dims).
#define UOP_STORE       37   // heap = [buf, addr, value];
                             //   Symmetric counterpart to UOP_INDEX_E.
                             //   Writes `value` to `buf` at the symbolic
                             //   address `addr` (a tree of UOP_RANGE/I*
                             //   like INDEX_E's addr_expr).
#define UOP_AFTER       38   // heap = [node, after_node];
                             //   Ordering annotation between sibling
                             //   side-effects (UOP_STOREs).  Backend
                             //   emits a barrier when AFTER crosses a
                             //   scope boundary (LOCAL <-> GLOBAL) or a
                             //   warp shuffle when crossing REG.  T.copy
                             //   = STORE+AFTER; T.async_copy = STORE +
                             //   AFTER + Linear ordering.
// === Opt annotation kinds ===
// Mirrors TileLang's OptOp directives.  Renderer pattern-matches
// (target, kind) to emit the appropriate code: UNROLL unrolls a
// RANGE k-loop; UPCAST unrolls an output dimension; TC selects a
// tensor-core path for matmul; LOCAL binds to thread-position;
// GROUP_REDUCE splits a REDUCE into threadgroup-cooperative chunks.
#define UOP_OPT_UNROLL        0
#define UOP_OPT_UPCAST        1
#define UOP_OPT_TC            2   // tensor core (T.gemm / T.wgmma_gemm)
#define UOP_OPT_LOCAL         3   // bind to thread position
#define UOP_OPT_GROUP_REDUCE  4
#define UOP_OPT_CONV          5   // conv2d_flat output kernel template
#define UOP_OPT_FAST_MATH     6   // fast::* intrinsic for unary ops under target
#define UOP_OPT_SIMD_REDUCE   7   // simd_sum/simd_max simdgroup-collective reduce
#define UOP_OPT_VEC_LOAD      8   // vectorized cooperative load:
                                  // wraps UOP_INDEX_E with floatN reinterpret_cast.
                                  // factor = lane width (2/4/8/16; typical 4 fp32).
                                  // See docs/plans/mlx_features_to_port.md feature 4.
#define UOP_OPT          39  // heap = [target, NUM(kind), NUM(factor)];
                             //   Annotation node attaching an optimisation
                             //   directive to `target`.  factor=0 when the
                             //   directive carries no scalar (TC, LOCAL).
                             //   The renderer walks UOp shape + OptOp
                             //   annotations to fire specialised templates.
// === Unified rangeify boundary (mirrors tinygrad's Ops.BUFFERIZE) ===
// Heap layout: [value, NUM(addrspace), NUM(removable), NUM(n_ranges),
//               range_0, ..., range_{n_ranges-1}].
//   value         : the producer Term whose output this buffer holds.
//                   In tinygrad src=(new_src,)+closed_ranges; we mirror by
//                   storing the value at slot 0 and the closed ranges
//                   following the (addrspace, removable, n_ranges) header.
//   addrspace     : UOP_SCOPE_GLOBAL / LOCAL / REG (mirrors
//                   BufferizeOpts.addrspace in tinygrad/schedule/indexing.py).
//   removable     : 1 if a downstream pass may collapse this boundary into
//                   its single consumer (mirrors BufferizeOpts.removable).
//                   0 forces it to materialize (e.g. CONTIGUOUS / COPY src).
//   n_ranges      : number of "closed" range Terms that follow.
//   range_i       : the UOP_RANGE leaves that index this buffer; their
//                   extents are the buffer's shape.  Mirrors tinygrad's
//                   `(new_src,)+closed_ranges` src tuple at
//                   tinygrad/schedule/indexing.py:77.
//
// pm_apply_rangeify in tinygrad emits BUFFERIZE at every realize boundary
// the run_rangeify walk decided; thvm's unified pass writes UOP_BUFFERIZE
// onto the main heap at the same boundaries so materialize.c can read a
// single source of truth.
#define UOP_BUFFERIZE   40
// 41, 42 = UOP_IOR / UOP_IXOR (declared in the Symbolic INDEX layer above).
// Identity-forward, STOP-backward marker (tinygrad UOp detach / stop-grad).
// uop_grad treats it as a leaf w.r.t. its child (the cotangent dies), and
// uop_graph_simplify unwraps it to its src BEFORE materialize, so no
// kernel/render/walker path ever sees it.  Used by BatchNorm.calc_stats
// (`y = x - mean.detach()`) so the variance gradient does not flow back
// through the mean -- mathematically d(var)/d(mean)=0, so the result is
// identical but the backward graph is far smaller.
#define UOP_DETACH      43
#define UOP_COUNT       44

// REDUCE kinds packed into the high bits of UOP_REDUCE's EXT field.
#define REDUCE_SUM   0
#define REDUCE_MAX   1

// === Capacities ===
#define HEAP_CAP     (1ULL << 28)   // 256M cells * 8B = 2 GiB.  Cheney splits in half (1 GiB per semi-space).  Bumped from 1<<26 so the HVM bench fib_nat (~1.2B cells over its tree) fits without GC.
#define WNF_CAP      (1ULL << 16)   // 64K stack slots.
#define TENS_CAP     (1ULL << 20)   // 1M tensor descriptor slots.
#define KERNELS_CAP  (1ULL << 18)   // 256K compiled kernels.
#define BOOK_CAP     (1ULL << 28)   // 256M cells of static def template heap (2 GiB) -- iter Z+? per-thread arena needs at V>=8 SAT collapse.
#define DEFS_CAP     256            // max named definitions for TAG_REF.
#define ALO_STATE_CAP (1ULL << 22)  // ALO substitution-chain entries.
#define MAX_DIM      8              // max tensor rank
#define KERNEL_INIT_INPUT 8         // initial input-arrays capacity (grows on demand)
#define KERNEL_MAX_INPUT  (1ULL<<20) // hard sanity bound (1M inputs/kernel)
                                    // (Conv2D fuses 2*kh*kw input/weight
                                    // tids into one kernel; 64 covers up
                                    // to 5x5 with headroom)
#define MAX_UOP_SRC  3              // max source slots per UOP node (CONV2D needs 3: input/weights/bias)

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
  u8       requires_grad;       // canonical "this tensor is a parameter" flag,
                                // consulted by uop_grad's leaf rule and by the
                                // Python frontend's backward() to enumerate
                                // parameters.  Set via py_ten_set_requires_grad.
  u32      buf_id;              // backend buffer handle (0 = no buffer yet)
  u32      producer_kid;        // kernel id that produces this tensor, 0 = external
  Backend *backend;             // vtable
  Term     grad;                // accumulated lazy gradient term; populated by
                                // grad_leaf_sup (target==0 path, requires_grad)
                                // as the chain rule walks; 0 = no grad yet.
                                // Python/WL backward reads this after realize.
} TenDesc;

// Forward declaration for the dispatch callback.
struct KernelEntry;

// === kvar visible-from-KernelEntry forward block =====================
// The per-kernel runtime-binding tables embedded in KernelEntry need
// KVAR_USED_CAP at type-definition time.  Hoist just the cap up here;
// the rest of the kvar surface (`KVAR_FLAG_BIT`, the helper fn decls)
// stays in the schedule section below so it sits next to its module.
#define KVAR_USED_CAP 8

// === KpSchedule ===
// Axis-typed scheduling structure carried per-KernelEntry, mirroring
// tinygrad's `Kernel.full_shape[]` + applied-opt log (no axis_types[]
// after E9; per-axis kax_type derives from output_shape +
// tail-reduce + scalar-reduce + applied_opts via
// `axes_resolve_kax_type`).  The codegen variant emitter walks these
// to produce a structured iteration nest -- nested loops, unrolled
// blocks, thread/threadgroup bindings (Metal) -- instead of one flat
// `for i = 0..numel-1`.
//
// Default state (set by axes_default_for at materialize-time): one
// LOOP axis per output dim plus a trailing REDUCE for tail-REDUCE
// kernels.  Equivalent to today's flat emit -- 393/393 must keep
// passing with no opts applied.
//
// Opts are applied via kernel_apply_opt and recorded in applied_opts[].
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
#define KOP_GLOBAL  10
#define KOP_FAST_MATH 11
#define KOP_SIMD_REDUCE 12
#define KOP_VEC_LOAD  13

typedef struct {
  u8  op;        // KOP_*
  u8  axis;      // 0-indexed; meaning depends on op
  u32 arg;       // op-specific (split factor for UPCAST/UNROLL, target
                 // axis index for SWAP, full axis size for GLOBAL,
                 // MMA tile size for TC, ...)
} KOpt;

typedef struct {
  // E9: axis_types[] is gone; per-axis kax_type derives on demand
  // from (output_shape + tail-reduce + scalar-reduce + applied_opts)
  // via axes_resolve_kax_type / axes_compute_axis_types in
  // codegen/axis.c.
  //
  // Per-axis extents + axis count derive on demand from
  // (output_shape + tail-reduce + scalar-reduce + applied_opts) via
  //   axes_resolve_full_shape(ke, d, *out)
  //   axes_resolve_n_axes(ke)
  // KpSchedule carries only the applied_opts log + autotune
  // bookkeeping bits.
  KOpt applied_opts[MAX_OPTS];
  u8   n_applied;
  u8   autotuned;              // 1 = kernel_autotune has run on this
                               // KpSchedule.  Guards the "fire-time
                               // autotune" path against re-running on
                               // every dispatch and against infinite
                               // recursion when autotune itself fires
                               // the kernel for benching.  Preserved
                               // across axes_reset_to_default so a
                               // proposer-explored variant doesn't
                               // re-trigger autotune mid-bench.
} KpSchedule;

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
  int   (*buf_copy) (u32 dst_buf_id, u32 src_buf_id, u64 nbytes);
  // Optional (may be NULL): refcount probe + free-list hand-off used by
  // the per-realize memory planner (materialize.c) for in-pass
  // physical-buffer reuse.  buf_freelist_push(b) marks b's storage
  // recyclable (refcount -> 0); buf_freelist_remove(b) un-recycles it
  // (refcount -> 1) if it's still on the list and wasn't re-issued.
  u32   (*buf_refcount)(u32 buf_id);
  void  (*buf_freelist_push)(u32 buf_id);
  void  (*buf_freelist_remove)(u32 buf_id);
  void  (*dispatch_begin)(void);
  void  (*dispatch_flush)(void);
  void  (*dispatch_end)(void);
  int   (*dispatch_kernel)(struct KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id);
};

// Hard ceiling on a single backend buffer allocation, in bytes.  A
// pathological kernel program (e.g. an im2col-style EXPAND that
// materializes a multi-billion-element intermediate -- the conv-1
// backward dInput on the per-op fallback path is ~947M elements ≈
// 3.8 GiB at BS=512, and the doc records a 5.24-billion-element ≈
// 21 GiB variant; see docs/plans/profiling_methodology.md sec 4.6)
// can otherwise drive cpu_buf_alloc / metal_buf_alloc to request many
// GB in one shot, which thrashes / OOM-kills the host before any other
// guardrail can react.  Default 1 GiB: ~25x the largest legitimate
// beautiful_mnist activation at BS=512 (conv-1 output ≈ 38 MiB), far
// below the documented pathological intermediates.  THVM_MAX_BUF_BYTES
// (bytes, 0 = unlimited) overrides for workloads with a genuinely huge
// single tensor (large-LM weights, big-batch transformers).
//
// `static inline` in the header so both the unity C build (cpu_buf_*)
// and the separately-compiled Metal .m (metal_buf_*) see one
// definition; the env read is memoized per-TU (env doesn't change).
static inline u64 thvm_buf_byte_ceiling(void) {
  static int  known = 0;
  static u64  limit = 1ull << 30;   // 1 GiB
  if (!known) {
    char const *e = getenv("THVM_MAX_BUF_BYTES");
    if (e != NULL && e[0] != '\0') limit = strtoull(e, NULL, 10);
    known = 1;
  }
  return limit;
}

// Hard ceiling on total live backend buffer bytes (working set +
// deferred-free backlog + per-op intermediates) at any instant.
// Backstop for the case where many individually-under-the-per-buffer-
// ceiling allocations accumulate -- e.g. the JIT *capture* run pins
// every kernel output (jit_capture_retain_buf) and the schedule does
// not yet reuse buffers across non-overlapping lifetimes, so the cold
// capture footprint = sum of all kernel outputs + materialized
// intermediates (observed ~2 GiB at BS=32, projecting to tens of GB at
// BS=512).  Default 8 GiB.  THVM_MAX_LIVE_BYTES (bytes, 0 = unlimited)
// overrides.  Checked in metal_record_memory_peak / its CPU analog.
static inline u64 thvm_live_byte_ceiling(void) {
  static int  known = 0;
  static u64  limit = 8ull << 30;   // 8 GiB
  if (!known) {
    char const *e = getenv("THVM_MAX_LIVE_BYTES");
    if (e != NULL && e[0] != '\0') limit = strtoull(e, NULL, 10);
    known = 1;
  }
  return limit;
}

// === KernelEntry ===
// A unit of compute produced by materialize; consumed by the
// backend's dispatch_kernel.  The kernel body lives on the lifted
// UOp DAG rooted at `cached_lift.store_root`; visit() walks the
// pre-bufferize UOp graph and records input bindings on the entry,
// then kernel_lift_to_uop produces the canonical post-lift DAG that
// the renderers / DAG-side encoder / cpu_uop_walk all consume.

#define KSRC_INPUT_FLAG  0x80000000u
#define KSRC_AS_INPUT(n) (KSRC_INPUT_FLAG | (u32)(n))
#define KSRC_IS_INPUT(s) (((s) & KSRC_INPUT_FLAG) != 0)
#define KSRC_INDEX(s)    ((s) & 0x7FFFFFFFu)

// === axis annotation type =============================================
// Per-axis info returned by tile_anno_axis_or_kernelaxes.  memory_scope
// and vector_width are 0-valued today; reserved for future use.
typedef struct {
  u32 kax_type;
  u32 extent;
  u32 memory_scope;
  u32 vector_width;
} TileAxisInfo;

// Matmul shape facts (M/N/K/ldA/ldB/flags/dtype/a_input/b_input)
// flow through UopDagGemmShape (src/uop/dag_scan.c) read from
// ke->cached_lift.store_root.

typedef struct {
  u32 dtype;
  u32 w_input;
  u32 x_input;
  u32 patch_input_base;
  u32 patch_input_count;
  u32 c_out;
  u32 c_in;
  u32 h;
  u32 w;
  u32 kh;
  u32 kw;
  u32 h_out;
  u32 w_out;
  u32 batch;
  u32 patches;         // total output patches = batch * h_out * w_out
  u32 spatial_patches; // h_out * w_out
  i32 w_offset;
  i32 w_stride0;
  i32 w_stride1;
  i32 x_offset;
  i32 x_stride_b;
  i32 x_stride0;
  i32 x_stride1;
  i32 x_stride2;
  u32 threads;            // Metal SIMT threads per threadgroup; 256 default
  u32 outputs_per_thread; // output elements computed by one Metal thread
  u32 reduce_unroll;      // unroll factor for the flattened conv reduction
} TileConv2DInfo;

// === Kernel lift to UOp DAG (forward decl) ===
// Full prose lives near the kernel_lift_to_uop declaration further down;
// the typedef is hoisted here so KernelEntry can embed it by-value as
// `cached_lift`.
//
// KERNEL_LIFT_MAX_INPUT bounds the in_bufs[] inline array for stack
// safety -- KERNEL_MAX_INPUT (1M) is a sanity cap, not a typical
// fan-in.  Real workloads max out at ~30 inputs (Conv2D fuses
// kh*kw input/weight tids per kernel).
#define KERNEL_LIFT_MAX_INPUT 64
typedef struct {
  // Topmost UOP_STORE for the kernel's single output.
  Term store_root;
  // Output buffer Term (the UOP_BUFFER the STORE writes to).
  Term out_buf;
  Term in_bufs[KERNEL_LIFT_MAX_INPUT];
  u32  n_inputs;
} KernelUopLift;

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
  // per-input-slot bufferize source-buffer id.  visit() populates
  // this whenever the input slot was created for another realized
  // boundary, so rangeify and other consumers can call
  // bufferize_edge_summary with `(this kernel's loc, source loc)`
  // to read the canonical edge-local chain.  0 means "leaf input
  // or unknown source".
  u32      *input_source_buffer_ids;
  // Per-slot flag (heap array): 1 iff rangeify folded this input's
  // ShapeTracker prior_views chain into the kernel INDEX expression
  // (composed-index, the tinygrad approach -- the strided view is
  // read in-kernel with zero materialisation).  When set,
  // cpu_dispatch_kernel / metal_dispatch_kernel SKIP the per-input
  // chained pre-materialise gather for that slot; when 0 (rangeify
  // declined / didn't compose), the pre-mat fires exactly as before.
  // Allocated/zeroed/freed in kernel_alloc.c.
  u8       *input_chain_composed;

  u32       output_tid;            // TenDesc id we write to
  u32       output_dtype;
  Shape     output_shape;
  u32       output_numel;

  // Original root UOP term that this kernel was built from.  The
  // walker rewrites parent cells to UOP_KERNEL but leaves the
  // source UOP cells in the heap (now "orphaned" -- no live
  // references reach them through the rewritten parents).  Grad
  // chain-rule walks this term directly: heap_read on its child
  // slots gives whatever they point at now (often other kernels,
  // each of which carries its own source_uop), so the walk
  // recurses into the original UOp graph naturally.
  Term      source_uop;

  // Phase C dual-write: post-lift UOp DAG root for this kernel's
  // compute (the UOP_STORE produced by kernel_lift_to_uop).  0 / NULL
  // when the lift hasn't been attempted (gemm/conv2d-only kernels
  // before dispatch) or declines.  Populated by emit_kernel_for_boundary
  // alongside cached_lift below.  Heap-resident terms are evacuated by
  // gc_evacuate_side_tables (heap/collect.c).
  //
  // The topo walker selects this kernel's boundary from
  // RU_BUFFERIZE_TERM[] (the unified-pass main-heap UOP_BUFFERIZE node)
  // and stashes it here. Mirror source: tinygrad/schedule/indexing.py:77
  // (the BUFFERIZE node consumed by create_kernel via the lowered DAG).
  // Heap-resident; gc_evacuate_side_tables walks it.
  Term      compute_bufferize;

  // Cached output of kernel_lift_to_uop, populated by
  // emit_kernel_for_boundary.  When the lift declines,
  // cached_lift.store_root stays 0.  All Term-typed fields
  // (store_root, out_buf, in_bufs[0..n_inputs)) are heap-resident
  // and walked by gc_evacuate_side_tables across collections.
  // Embedded by-value (~528 B per slot, KERNELS_CAP-bounded) so
  // there's no extra allocation / lifetime management.
  KernelUopLift cached_lift;

  // Snapshot of cached_lift.store_root at materialize time, before any
  // kernel_apply_opt DAG mutations.  axes_reset_to_default restores
  // cached_lift.store_root from this so autotune's bench-each-variant
  // flow can rewind DAG state after each candidate.  0 when the lift
  // declined.
  Term      cached_lift_init_root;

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
  // Axis-typed scheduling plan.  `schedule` is a pointer for
  // historical reasons; today it always aims at `_local_schedule`
  // below (each kernel owns its own plan).
  KpSchedule  *schedule;
  KpSchedule   _local_schedule;

  // kvar wedge: per-dispatch runtime values for any symbolic-shape
  // Variables bound to RANGE leaves in this kernel.  Sparse: each
  // slot (kvar_runtime_ids[i], kvar_runtime_vals[i]) binds one var
  // id to its current runtime value (e.g. BS=4 or BS=32).  The Metal
  // encoder iterates over the kvars referenced by this kernel in
  // sorted-id order and looks up each id here; missing entries fall
  // back to kvar_hi(id) so non-symbolic kernels keep working.
  // Caller stamps these before each dispatch.
  u8         n_kvar_runtime;
  u32        kvar_runtime_ids [KVAR_USED_CAP];
  u32        kvar_runtime_vals[KVAR_USED_CAP];
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
  // skip_freelist: when set, cpu_buf_pool_rollback_with_preserve
  // real-frees this buf via cpu_buf_free instead of parking it on
  // CPU_FREELIST.  Set on per-realize arena CpuBufs (each one is a
  // 100-700MB one-shot block, sized to that pass's max-live working
  // set; recycling it via best-fit try_pop would either snag it for
  // smaller requests in the next realize -- wasting most of the
  // bytes -- or leave it parked indefinitely while a fresh arena is
  // calloc'd, which is exactly the cross-step leak that motivated
  // this flag.  Mirror: tinygrad allocates a fresh arena per
  // memory_plan_rewrite call and lets Python GC reclaim the previous
  // arena's BUFFER_VIEW chain.)
  u8    skip_freelist;
  void *handle;
  void (*on_release)(void *handle);
  // Arena views: when non-zero, this buf is an external view into
  // parent_buf_id's data.  cpu_buf_free decrements the parent's
  // refcount so the arena dies once its last view is freed.  Mirror:
  // tinygrad/schedule/memory.py:60 BUFFER_VIEW(arena, nbytes, offset)
  // -- the arena UOp src keeps the underlying buffer alive while any
  // BUFFER_VIEW persists.
  u32   parent_buf_id;
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
    u64 jit_replay_calls;
    u64 jit_replay_dispatches;
    u64 jit_replay_assigns;
    u64 jit_graph_runs;
    u64 jit_graph_dispatches;
} HotCounters;

#define HOT_COUNTER_COUNT 15

// === MultiEvent: multicomputation reduction trace ===
//
// See docs/multicomputation.md (conceptual reading) and
// docs/plans/multicomputation_trace.md (build trajectory).
//
// Off by default.  Build with -DTHVM_TRACE to enable; even in that
// build, the runtime flag CURRENT_CTX->trace must be set for events
// to be recorded.  Without -DTHVM_TRACE the multi_emit() macro
// expands to ((void)0) and no TContext fields exist; every
// interact_* / heap_* compiles to the same instructions as today.
// The default-build acceptance gate is: `bench-train` / `bench-atp`
// / AOT match their pre-trace numbers within noise.

// Family classification (docs/plans/multicomputation_trace.md §4).
#define MULTI_TERM   0  // within-branch compute event (states-graph edge)
#define MULTI_SLIDE  1  // re-foliation (APP-SUP commute, cnf lift, INC rules)
#define MULTI_FORK   2  // 1 -> 2 (seeded SUP, DUP-CTR/LAM/NOD)
#define MULTI_SPLIT  3  // DUP-SUP distinct labels: branchial cross product
#define MULTI_MERGE  4  // DUP-SUP same label: branches reconverge
#define MULTI_PRUNE  5  // ERA absorbs neighbour: dead branch
#define MULTI_DIST   6  // DUP distributes through a non-SUP term
                        // (DUP-NUM / DUP-TEN / DUP-ANY atom-copy,
                        //  DUP-NOD / DUP-UOP structural clone)

// Rule kinds.  One per `src/interact/<name>.c`; numeric values are
// an internal enum (not an ABI) -- the WL side will eventually decode
// by symbolic name supplied alongside.  `DUP_SUP` splits in two
// because the same .c file emits different (family, rule) pairs for
// the same-label (annihilate -> MULTI_MERGE) and different-label
// (commute -> MULTI_SPLIT) cases.  Inline ITRS++ sites in
// `src/wnf/_.c` (e.g. OP2-NUM-NUM, MAT-CTR pattern matches) are
// *not* covered yet -- a follow-up.
#define RULE_APP_LAM         0  // beta
#define RULE_APP_ERA         1
#define RULE_APP_SUP         2
#define RULE_APP_BRI         3
#define RULE_APP_PRI         4
#define RULE_APP_MAT_SUP     5
#define RULE_ANN_LAM         6
#define RULE_ANN_BRI         7
#define RULE_DUP_LAM         8
#define RULE_DUP_BRI         9
#define RULE_DUP_CTR        10
#define RULE_DUP_NOD        11  // generic eager-commute through n-ary node
                                // (OP2/MAT/EQL/AND/OR/WHEN/ANN/DSU/DDU/INC);
                                // term_a carries the term so trace can
                                // recover the inner tag via term_tag(term_a)
#define RULE_DUP_ERA        14
#define RULE_DUP_NUM        15
#define RULE_DUP_TEN        16
#define RULE_DUP_UOP        17
#define RULE_DUP_ANY        18
#define RULE_DUP_SUP_ANN    19  // same-label annihilate -> MULTI_MERGE
#define RULE_DUP_SUP_COM    20  // different-label commute -> MULTI_SPLIT (stuck)
#define RULE_OP2_SUP        21
#define RULE_OP2_NUM_SUP    22
#define RULE_DSU_NUM        23
#define RULE_DSU_SUP        24
#define RULE_DSU_ERA        25
#define RULE_DDU_NUM        26
#define RULE_DDU_SUP        27
#define RULE_DDU_ERA        28
#define RULE_UOP_ASSIGN     29
#define RULE_UOP_KERNEL     30
// Inline WHNF-frame rules (no dedicated src/interact/<name>.c -- the
// stack machine in src/wnf/_.c handles them via per-frame cases).
#define RULE_GRAD_FWD       31  // grad-cell DP0 forward passthrough
#define RULE_GRAD_BWD       32  // grad-cell DP1 backward chain-rule step
#define RULE_MAT_DISPATCH   33  // MAT/SWI: scrutinee reduced, branch on match
#define RULE_OP2_NUM_NUM    34  // OP2 with both operands NUM: literal fold
#define RULE_EQL_NUM        35  // EQL with both operands NUM: equality fold
#define RULE_EQL_ERA        36  // EQL meets ERA: propagate ERA
#define RULE_EQL_ANY        37  // EQL meets ANY (wildcard): result 1
#define RULE_EQL_SUP        38  // EQL commutes through SUP
#define RULE_AND_NUM        39  // AND short-circuit on a literal first operand
#define RULE_AND_ERA        40  // AND meets ERA: propagate ERA
#define RULE_AND_SUP        41  // AND commutes through SUP
#define RULE_OR_NUM         42  // OR short-circuit on a literal first operand
#define RULE_OR_ERA         43  // OR meets ERA: propagate ERA
#define RULE_OR_SUP         44  // OR commutes through SUP
#define RULE_WHEN_NUM       45  // WHEN branch on a literal condition
#define RULE_WHEN_ERA       46  // WHEN meets ERA: propagate ERA
#define RULE_WHEN_SUP       47  // WHEN commutes through SUP

// Sentinel for `consumed[i]`: "no producer recorded".  Means the cell
// the event read was either pre-trace (never WIRE_PROV_BUMP'd) or the
// event has fewer than two consumed slots (n_consumed < 2 / 1).
#define MULTI_WIRE_NONE ((u32)0xFFFFFFFFu)

typedef struct MultiEvent {
    u64 id;            // monotone; == ITRS at the point this rule fired
    u8  rule;          // RULE_*
    u8  family;        // MULTI_*
    u8  n_consumed;    // 0..2; number of valid consumed[] slots (M1)
    u8  _pad[5];       // align next u64
    u64 term_a;        // active-pair Term words, captured pre-rewrite
    u64 term_b;
    u32 delta_label;   // SUP/DUP label (FORK/SPLIT/MERGE only; else 0)
    u32 _pad2;
    // M1: wire provenance -- the event ids that last wrote to the
    // active-pair payload cells (term_val(term_a) / term_val(term_b)),
    // captured at multi_emit time before this event's own heap_sets
    // mutate them.  MULTI_WIRE_NONE means "no producer recorded" --
    // either the cell predates the trace, or the corresponding slot
    // isn't populated.  The host-side causal graph is built from
    // these: edge `F -> E` iff `E.consumed[*] == F.id`.
    u32 consumed[2];
    // v2+ will add: produced[] (WireIds), loc_a/loc_b (redex locs),
    // n_alloc/alloc_base (allocated products), cyl_off/cyl_len.
} MultiEvent;

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
// Per-thread WNF spine state.  Lives inside TContext (singleton on
// the main thread) and inside each parallel WnfWorker; a thread-
// local pointer (`CURRENT_WNF_STATE`) selects which one the
// `WNF_STACK` / `WNF_S_POS` macros dereference.  This is the
// minimal scaffolding the parallel WNF / NF pool needs without
// retyping the whole TContext layout.
typedef struct {
    Term *stack;
    u32   s_pos;
} WnfThreadState;

typedef struct TContext {
    /* Heap-allocated arrays (calloc on context_create). */
    Term       *heap;
    WnfThreadState wnf_state;            // .stack / .s_pos -- main thread's
                                         // routing target for WNF_STACK /
                                         // WNF_S_POS when no worker is bound
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
    u64 heap_next;          // bumped via __atomic_fetch_add by heap_alloc
    u32 wnf_last_stack_len;
    _Atomic u64 itrs;       // interactions counter; atomic so the
                            // parallel WNF / NF pool can `ITRS++`
                            // safely from any worker thread
    u32 tens_next;
    u32 kernels_next;
    u64 book_next;
    u32 alo_states_next;
    u64 cpu_bufs_next;
    u32 cpu_freelist_len;

    /* Hot-path counters (see HotCounters). */
    HotCounters hot;

#ifdef THVM_TRACE
    /* Multicomputation trace (see MultiEvent above and
       docs/plans/multicomputation_trace.md).  Only present when the
       binary is built with -DTHVM_TRACE; even then, events are
       appended only while the runtime flag `trace` is non-zero. */
    MultiEvent *multi_events;
    u64         multi_events_len;
    u64         multi_events_cap;
    /* M1: wire provenance.  multi_wire_prov[loc] = id of the event
       that last WIRE_PROV_BUMP'd that loc, or MULTI_WIRE_NONE if no
       traced event has written there yet.  Grown lazily on demand; a
       loc outside [0, multi_wire_prov_cap) reads as MULTI_WIRE_NONE
       without growing.  Allocated by multi_trace_init, freed by
       multi_trace_free. */
    u32        *multi_wire_prov;
    u64         multi_wire_prov_cap;
    u8          trace;
#endif
} TContext;

#define THVM_MAX_BACKENDS 4
#define THVM_DEV_CPU      0
#define THVM_DEV_METAL    1
#define THVM_DEV_CUDA     2

#define CONTEXTS_CAP 16
extern TContext *CURRENT_CTX;
extern TContext *CONTEXTS[CONTEXTS_CAP];

// Thread-local pointer to the WNF spine state for the current thread.
// On the main thread, points at `CURRENT_CTX->wnf_state`; on a parallel
// pool worker, points at that worker's WnfThreadState.  Initialised by
// `thvm_init` for the main thread; set/reset by the worker pool driver
// for spawned pthreads.
extern _Thread_local WnfThreadState *CURRENT_WNF_STATE;

// Macro layer -- existing global names redirect through CURRENT_CTX
// so all the C code under src/heap, src/term, src/wnf, src/schedule,
// src/alo, src/uop, src/book, src/backend keeps compiling without
// per-call ctx threading.  WNF spine routes through CURRENT_WNF_STATE
// so worker threads transparently get their own stacks.
#define HEAP                (CURRENT_CTX->heap)
#define HEAP_NEXT           (CURRENT_CTX->heap_next)
#define WNF_STACK           (CURRENT_WNF_STATE->stack)
#define WNF_S_POS           (CURRENT_WNF_STATE->s_pos)
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
#define HOT_JIT_REPLAY_CALLS    (CURRENT_CTX->hot.jit_replay_calls)
#define HOT_JIT_REPLAY_DISPATCHES (CURRENT_CTX->hot.jit_replay_dispatches)
#define HOT_JIT_REPLAY_ASSIGNS  (CURRENT_CTX->hot.jit_replay_assigns)
#define HOT_JIT_GRAPH_RUNS      (CURRENT_CTX->hot.jit_graph_runs)
#define HOT_JIT_GRAPH_DISPATCHES (CURRENT_CTX->hot.jit_graph_dispatches)

// Multicomputation trace (see MultiEvent above).  All gated by
// THVM_TRACE: the macro expands to ((void)0) by default, so call
// sites in interact_* / heap_* contribute zero instructions to a
// default build.  Even in a THVM_TRACE build the recorded path runs
// only when CURRENT_CTX->trace is non-zero, behind a single
// well-predicted branch.
#ifdef THVM_TRACE
#define MULTI_TRACE_ON          (CURRENT_CTX->trace)
fn void multi_emit_body(u8 rule, u8 family,
                        u64 term_a, u64 term_b,
                        u32 delta_label);
#define multi_emit(rule, family, term_a, term_b, delta_label)         \
    do {                                                              \
        if (__builtin_expect(MULTI_TRACE_ON, 0)) {                    \
            multi_emit_body((u8)(rule), (u8)(family),                 \
                            (u64)(term_a), (u64)(term_b),             \
                            (u32)(delta_label));                      \
        }                                                             \
    } while (0)
// M1: stamp `loc` with the in-flight event's id (= ITRS - 1, since
// every interact_* / inline wnf rule does ITRS++ at the head before
// any heap mutation).  Cells written before the first event ever
// fired stamp with (u32)-1 == MULTI_WIRE_NONE, which is correct: they
// were produced outside the recorded trace.  Out-of-line into
// multi_wire_prov_bump_body so the call site stays one branch.
fn void multi_wire_prov_bump_body(u64 loc);
#define WIRE_PROV_BUMP(loc)                                           \
    do {                                                              \
        if (__builtin_expect(MULTI_TRACE_ON, 0)) {                    \
            multi_wire_prov_bump_body((u64)(loc));                    \
        }                                                             \
    } while (0)
fn u32                 multi_wire_prov_get(u64 loc);  // sentinel-safe read
fn void                multi_trace_init(u64 initial_cap);
fn void                multi_trace_reset(void);
fn void                multi_trace_free(void);
fn u64                 multi_trace_count(void);
fn const MultiEvent  * multi_trace_get(u64 i);
fn const char        * multi_rule_name(u8 r);
fn const char        * multi_family_name(u8 f);
#else
#define multi_emit(rule, family, term_a, term_b, delta_label) ((void)0)
#define WIRE_PROV_BUMP(loc) ((void)0)
#endif

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
// Runtime heap allowance in cells.  Defaults to HEAP_CAP; overridable
// once per process via the THVM_HEAP_CELLS env var (read at first call).
// Drives the calloc size, gc_init split, and the gc_mark visited bitmap.
fn u64  thvm_heap_cells(void);
fn void gc_init(u64 space_words);
fn void gc_reset(void);
fn int  gc_enabled(void);
fn u64  gc_from_start(void);
fn u64  gc_from_end(void);
fn u64  gc_count(void);
fn void gc_collect(Term *roots, u32 n_roots);

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

// === schedule/kvar -- symbolic-shape Variable registry ============
// A kvar represents a kernel-shape parameter (e.g. "BS") that should
// flow through the schedule as a symbol rather than a concrete u32.
// A RANGE leaf whose extent is bound to a kvar bakes the kvar id
// into the MSL source (as `V_<name>`) and the actual numeric extent
// gets bound as a `constant uint` kernel arg at dispatch time.  See
// src/schedule/kvar.c for the full design notes.
#define KVAR_FLAG_BIT 31u
#define KVAR_FLAG     (1u << KVAR_FLAG_BIT)
#define KVAR_ID_MASK  (KVAR_FLAG - 1u)
// KVAR_USED_CAP is hoisted near `struct KernelEntry;` above so the
// per-kernel binding tables can use it at type-definition time.
//
// These are PLAIN (non-`fn`) functions -- defined in
// src/schedule/kvar.c -- because the Metal backend (a separate TU)
// calls a few of them; `static inline` wouldn't link cross-TU.
// `struct KernelEntry` is the same type as the `KernelEntry` typedef
// further down (forward-decl + `typedef struct KernelEntry { ... }`),
// used in the struct-tag form so the signatures compile before the
// full body is seen.
u32          kvar_alloc(const char *name, u32 lo, u32 hi);
const char  *kvar_name(u32 id);
u32          kvar_lo(u32 id);
u32          kvar_hi(u32 id);
u32          kvar_count(void);
void         kvar_reset(void);
u32          kvar_pack_extent(u32 var_id);
int          kvar_extent_is_var(u32 packed_extent);
u32          kvar_extent_var_id(u32 packed_extent);
u32          kvar_extent_static(u32 packed_extent);
u32          kvar_collect_from_dag(Term root, u32 *out_ids, u32 cap);
int          kernel_kvar_bind (struct KernelEntry *ke, u32 var_id, u32 value);
u32          kernel_kvar_value(struct KernelEntry const *ke, u32 var_id);

// === schedule ===
// bufferize_classify walks the UOp DAG rooted at `root`, marks
// kernel boundaries (root + multi-consumer + REDUCE), runs the named
// realize-rewrite rules, and finalises the bufferize graph.
// UOpInfo / BUFFERIZE_NODES is the dense per-walked-UOp table that
// the classifier populates; materialize.c still walks it directly
// 
#define BUFFERIZE_NODES_CAP 16384
typedef struct {
  u64 loc;
  u32 consumer_count;
  u32 reasons;
  u32 cmap_head;          // linked-list head into CMAP_LL; UINT32_MAX = empty
  u8  op;
  u8  realized;
} UOpInfo;
extern UOpInfo BUFFERIZE_NODES[BUFFERIZE_NODES_CAP];
extern u32     BUFFERIZE_NODES_LEN;
fn u32  bufferize_info_find(u64 loc);
fn u8   bufferize_is_realized(Term uop_term);
fn u32  bufferize_consumer_count(Term uop_term);
fn u32  bufferize_reasons(Term uop_term);

// Per-node consumer-list pool. Each producer node's UOpInfo.cmap_head
// indexes into CMAP_LL; chain via .next. Populated by the bufferize
// walker (same pass that increments consumer_count). Tinygrad parity:
// the `cmap` (Dict[UOp, List[UOp]]) in tinygrad/schedule/indexing.py:155-160.
// Cap sized for ~4x BUFFERIZE_NODES_CAP avg fanout.
#define CMAP_LL_CAP (BUFFERIZE_NODES_CAP * 4)
typedef struct { u64 consumer_loc; u32 next; } CMapNode;
extern CMapNode CMAP_LL[CMAP_LL_CAP];
extern u32      CMAP_LL_LEN;
// Fill up to `cap` consumer_locs for `producer_loc`. Returns true count
// (may exceed cap; caller is responsible for sizing).
fn u32  bufferize_consumers_for_loc(u64 producer_loc, u64 *out_locs, u32 cap);

// === bufferize schedule IR ===
// Explicit B_BUFFERIZE/B_STORE/B_INDEX graph that future phases of
// docs/plans/bufferize.md make authoritative.  Today the graph is
// seeded from BUFFERIZE_NODES, mirrored through every named rule's
// mark/unmark, and finalised with one B_STORE for the realize root.
// `DUMP_BUFFERIZE=1` prints the per-buffer table after each pass.
#define BUFFERIZE_GRAPH_CAP BUFFERIZE_NODES_CAP
#define BUFFERIZE_REASON_ROOT        (1u << 0)
#define BUFFERIZE_REASON_MULTI       (1u << 1)
#define BUFFERIZE_REASON_REDUCE      (1u << 2)
#define BUFFERIZE_REASON_FANIN_CAP   (1u << 3)
#define BUFFERIZE_REASON_INLINE      (1u << 4)
#define BUFFERIZE_REASON_MATMUL      (1u << 6)
typedef struct {
  u64 loc;             // heap loc of the underlying UOp value
  u32 buffer_id;       // 1-based stable id within this graph
  u32 reasons;         // BUFFERIZE_REASON_*
  u32 consumer_count;  // direct UOp consumer count
  u8  op;              // UOP_* of the value being bufferized
  u8  is_root;         // 1 iff this buffer is the realize root
  u8  realized;        // 1 = currently realized, 0 = removed by a rule
  char const *removed_by; // name of the rule that cleared realized; NULL otherwise
  char const *added_by;   // name of the rule that introduced this buffer; NULL if seeded
  // Cost-model inputs (populated by bufferize_finalize_stores).
  // recompute_ops counts the pure UOps in this buffer's producer
  // subtree (constants and loads excluded; reduces stop the walk).
  // output_numel is the element count of the buffer's value shape;
  // 0 means "shape was unavailable at compute time".
  // recompute_total is recompute_ops * max(consumer_count, 1) -- a
  // first-cut estimate of the work multiplier if this buffer were
  // removed and every consumer recomputed it independently.
  // subtree_has_reduce is 1 iff the producer subtree
  // contains a REDUCE (the buffer's own op being REDUCE counts);
  // future reduce-aware rules use it to gate recompute removals
  // because reductions amplify recompute cost.
  u32 recompute_ops;
  u64 output_numel;
  u64 recompute_total;
  u8  subtree_has_reduce;
  // Reduce metadata.  Populated only when this buffer's
  // op is UOP_REDUCE; otherwise all three fields stay 0.
  //   reduce_kind  : REDUCE_SUM / REDUCE_MAX / ... (the kind cell)
  //   reduce_axis  : axis being reduced (post-rewrite collapsed)
  //   reduce_axis_size : extent of that axis on the source side
  // Future reduce-aware rules consult these to gate accumulator
  // schedules, broadcast fusion, and group-reduce decisions.
  u8  reduce_kind;
  u8  reduce_axis;
  u32 reduce_axis_size;
  // Lifetime fields (populated by bufferize_finalize_stores):
  // lifetime_start is this buffer's topological depth (1 for
  // buffers with no producer-buffer source); lifetime_end is the
  // max depth among its consumer buffers (== lifetime_start for
  // buffers with no consumer, e.g. the realize root).  output_bytes
  // is `output_numel * dtype_itemsize(dtype)`; 0 when the dtype
  // could not be resolved.
  u32 lifetime_start;
  u32 lifetime_end;
  u64 output_bytes;
} BBufferize;
typedef struct {
  u32 buffer_id;      // destination B_BUFFERIZE id
  u64 loc;            // heap loc of the stored value
} BStore;
// B_INDEX records one producer-buffer to consumer-buffer edge with
// the movement-op chain that sits between them in the consumer's
// compute tree.  Stops at producer buffers and skips leaf
// (TEN/VAR) inputs; later phases will add leaf edges and the full
// edge-local index expression / valid mask once rangeify consumes
// them.  has_* flags are independent: a chain can have multiple
// movement ops of different kinds.
//
// Each chain entry carries enough per-op data (op, src_dims,
// out_dims, pad_widths, axis_perm, flip_mask) for rangeify to
// drive RngsCtx from the bufferize edge graph.
#define BUFFERIZE_INDEX_CHAIN_MAX 8
typedef struct {
  u8  op;                        // UOP_RESHAPE/PERMUTE/EXPAND/PAD/SHRINK/FLIP
  u8  src_ndim;
  u8  out_ndim;
  u8  flip_mask;                 // FLIP only: per-axis bitmask
  u32 src_dims [MAX_DIM];
  u32 out_dims [MAX_DIM];
  u32 pad_widths[2 * MAX_DIM];   // PAD/SHRINK: interleaved {b0,e0,b1,e1,...}
  u8  axis_perm [MAX_DIM];       // PERMUTE: out axis i comes from src axis_perm[i]
} BIndexChainOp;
typedef struct {
  u32 source_buffer_id;       // 1-based id of producer B_BUFFERIZE
  u32 consumer_buffer_id;     // 1-based id of consumer B_BUFFERIZE
  u8  movement_chain_len;     // total movement ops on the edge (>= sum of has_* flags only when no op repeats)
  u8  has_pad;
  u8  has_reshape;
  u8  has_permute;
  u8  has_expand;
  u8  has_shrink;
  u8  has_flip;
  u8  chain_op_count;         // length of chain_ops below
  BIndexChainOp chain_ops[BUFFERIZE_INDEX_CHAIN_MAX];
} BIndex;
// bufferize_classify is the public schedule entry point.  Walks the
// UOp DAG rooted at `root`, marks ROOT/MULTI/REDUCE boundaries, runs
// the named realize-rewrite rules, and finalises the bufferize graph
// (B_BUFFERIZE + B_INDEX + B_STORE).  Replaces realize_classify; the
// old name is retained as a 1-line forward in realize_classify.c.
fn void              bufferize_classify(Term root);
fn void              bufferize_seed_from_nodes(Term root);
fn void              bufferize_finalize_stores(Term root);
fn u32               bufferize_buffer_count(void);
fn u32               bufferize_realized_count(void);
fn BBufferize const *bufferize_buffer_at(u32 i);
fn u32               bufferize_find_by_loc(u64 loc);
fn u32               bufferize_store_count(void);
fn BStore const     *bufferize_store_at(u32 i);
fn u32               bufferize_index_count(void);
fn BIndex const     *bufferize_index_at(u32 i);
// Fill `out` with the indices of every B_INDEX whose
// consumer_buffer_id matches `consumer_buffer_id`.  Returns the
// number written (capped at `cap`).  Pass cap=0 to just count.
fn u32               bufferize_indexes_for_consumer(u32 consumer_buffer_id,
                                                    u32 *out, u32 cap);
// Symmetric to bufferize_indexes_for_consumer: enumerate every
// B_INDEX edge whose source_buffer_id matches `source_buffer_id`,
// returning the count and writing up to `cap` indices into `out`.
fn u32               bufferize_indexes_for_source(u32 source_buffer_id,
                                                  u32 *out, u32 cap);
// Look up the first B_INDEX edge for the
// (consumer_loc -> source_loc) pair and copy its chain summary into
// `out`.  Returns 1 if an edge was found, 0 otherwise.  Both locs
// must be heap locs of B_BUFFERIZEs.  Future rangeify and
// materialize callers can use this to consult the canonical
// edge-local context instead of recovering it from the heap walk.
fn int               bufferize_edge_summary(u64 consumer_loc, u64 source_loc,
                                            BIndex *out);
// Named edge rewrite rules.  Each kind of movement op
// becomes one rule that "fires" once per B_INDEX edge carrying
// that op flag.  Hit counts are populated by bufferize_finalize_stores
// and surfaced through these accessors so DUMP_BUFFERIZE can show
// "index-reshape hits=N" alongside the realize-rule stats.
fn u32               bufferize_index_rule_count(void);
fn char const       *bufferize_index_rule_name(u32 i);
fn u32               bufferize_index_rule_hits(char const *name);
// Edge transform: number of identity reshape ops elided
// from B_INDEX chains during the most recent realize_classify
// pass.  Each elision drops one chain entry whose
// src_dims == out_dims and decrements the edge's
// movement_chain_len; future transforms can read this counter to
// gauge how much trivial folding the bufferize graph absorbed.
fn u32               bufferize_identity_reshape_elision_hits(void);

// Removal-candidate score.  Higher score = more attractive
// removal (single-use, small recompute budget).  The score is a
// first-cut heuristic; future cost-model rules will refine it.
// Returns 0 for unknown buffer ids and for buffers whose score was
// not computed (non-realized or no shape).
fn u64               bufferize_removal_score(u32 buffer_id);

// Lifetime accessors for memory-planning callers.  Returns
// 1 on success and writes the start/end topological depths through
// the out pointers; returns 0 for unknown or non-realized buffers
// and leaves the outputs unchanged.  Depth 1 is the leaf-most
// buffer (no producer source); higher depth = later in execution.
fn int               bufferize_buffer_lifetime(u32 buffer_id,
                                               u32 *lifetime_start,
                                               u32 *lifetime_end);

// Deterministic schedule key over the post-rewrite
// bufferize graph.  Hash mixes each realized buffer's
// (op, reasons, recompute_ops, output_numel) and each B_INDEX
// (source, consumer, chain flags) -- fields that depend on the
// schedule shape but not on heap locs, so the key stays stable
// across runs that produce the same schedule.  Autotune uses this
// to look up cached decisions per (graph, schedule) pair.
fn u64               bufferize_schedule_key(void);
// Aggregates: sum of output_bytes across realized buffers,
// max lifetime_end (the highest topological depth reached), and
// sum of recompute_ops across realized buffers.  These give
// autotune one-number summaries to pre-filter schedule candidates
// before the full per-buffer cost walk.
fn u64               bufferize_total_realized_bytes(void);
fn u32               bufferize_max_lifetime_depth(void);
fn u64               bufferize_total_recompute_ops(void);

// g2a: after realize_classify populates the boundary set, the
// scheduler topo-sorts those boundaries by producer-to-consumer
// depth.  These accessors expose the sorted order so tests / future
// code-emit can iterate boundaries in dependency order.
fn u32  materialize_boundary_count(void);
fn u64  materialize_boundary_at(u32 i);
// Depth + last-use lookup for the i-th boundary in BOUNDARY_ORDER.
// Returns 0 when the boundary index is out of range.
// validation: these should agree with bufferize's lifetime_start
// and lifetime_end for the same loc, since both are computed from
// the same edge structure (just at different times in the
// pipeline).  Memory planning will eventually consume the
// bufferize lifetimes once the parity is established.
fn u32  materialize_boundary_depth_at(u32 i);
fn u32  materialize_boundary_last_use_at(u32 i);

// Per-input-slot bufferize source id read from a
// materialized KernelEntry.  Returns the 1-based buffer id stored
// during visit() (0 when the slot's source is a leaf or was not
// resolvable to a bufferize buffer).  Pass `kid = 0` to query the
// default-context kernel pool.  The companion
// kernel_entry_input_edge_summary pulls the BIndex chain summary
// for that slot via bufferize_edge_summary, looking up the
// consumer loc from the kernel's source_uop.
fn u32  kernel_entry_input_source_buffer_id(u32 kid, u32 slot);
// Single-output kernel accessors.  Slot 0 returns the legacy
// `output_tid` family; other slots return 0 / sentinels.
// `kernel_entry_output_count` returns 1 for every live kernel.
fn u32  kernel_entry_output_count(u32 kid);
fn u32  kernel_entry_output_tid_at(u32 kid, u32 idx);
fn u32  kernel_entry_output_dtype_at(u32 kid, u32 idx);
fn int  kernel_entry_output_shape_at(u32 kid, u32 idx, Shape *out);
fn int  kernel_entry_input_edge_summary(u32 kid, u32 slot, BIndex *out);
// Per-USE variant: select the `edge_idx`-th BIndex record whose
// (consumer, source) pair matches the kernel's source_uop and the
// slot's source buffer.  Returns 1 on success, 0 when there are
// fewer than `edge_idx + 1` matching records.
fn int  kernel_entry_input_edge_at(u32 kid, u32 slot, u32 edge_idx,
                                   BIndex *out);

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
// Resolve a single axis's KAX_ type from the lifted DAG's post-opt
// RANGE leaves.  Returns KAX_LOOP when ke/axes are NULL,
// d >= n_axes, or the DAG is empty / overflows.  Used by
// tile_anno.c readers as the only axis-type read source -- no
// direct axis_types[i] reads remain in codegen/tile_anno.c.
fn u8   axes_resolve_kax_type(struct KernelEntry const *ke, u32 d);

// Derive per-axis full_shape extents from the higher-level signals
// (output_shape + tail-reduce + scalar-reduce + applied_opts).
// Mirrors the writer trio (axes_default_for +
// axes_ensure_scalar_reduce + axes_apply_opt) exactly.  Returns the
// number of extents written; 0 on overflow / unknown opt / invalid
// replay.  Used by axes_resolve_full_shape.
fn u32  axes_compute_full_shape(struct KernelEntry const *ke, u32 *out,
                                u32 cap);

// Per-axis full_shape resolver.  Canonical (and only) read path for
// axis extents.  Writes the derived extent for axis `d` into
// `*out_extent` and returns 1 on success; 0 (with `*out_extent = 0`)
// when ke/axes are NULL, d is out of range, or the simulator can't
// speak.
fn u32  axes_resolve_full_shape(struct KernelEntry const *ke, u32 d,
                                u32 *out_extent);

// Axis-count resolver.  Canonical (and only) read path for n_axes.
// Returns the derived axis count (output_shape.ndim clipped to
// MAX_AXES-1, plus 1 if a trailing REDUCE-class axis is present,
// plus the count of split-class applied_opts).
fn u32  axes_resolve_n_axes(struct KernelEntry const *ke);

// Apply one TOpt to a KernelEntry's axis structure.  Split-class opts
// (UPCAST/UNROLL/LOCAL/GROUP/GROUPTOP) split the indicated axis,
// growing n_axes by one; KOP_GLOBAL marks a LOOP axis as GLOBAL via
// the applied_opts log; KOP_SWAP exchanges two axes in-place; KOP_TC
// is metadata-only.  Returns 0 on validation failure (axis out of
// range, arg doesn't divide, applied_opts full, unsupported opt).
//
// Per-axis kax_type is not stored on KpSchedule; the writer records
// the opt and axes_resolve_kax_type derives the type from the
// applied_opts log on read.
fn int kernel_apply_opt(struct KernelEntry *ke, KOpt opt);

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

// === axis annotation read API (codegen/tile_anno.c) ===
// Resolves axis count + per-axis TileAxisInfo from the KpSchedule
// signal trio (output_shape + tail-reduce + scalar-reduce + applied_opts).
fn int  tile_anno_axis_or_kernelaxes(struct KernelEntry const *ke, u32 d,
                                     TileAxisInfo *out);
fn u32  tile_anno_axis_count_or_kernelaxes(struct KernelEntry const *ke);

// applied_opts facade.  External linkage (no `fn`) so backend_metal.o
// can call these.
u32        tile_anno_applied_opts_count(struct KernelEntry const *ke);
KOpt const *tile_anno_applied_opts(struct KernelEntry const *ke);
// Hash all per-axis (kax_type, extent) into the running FNV-1a state.
// Used by cache-key generation in autotune.c.
u64        tile_anno_hash_axes(struct KernelEntry const *ke, u64 h);
// Writer-side facade: thin wrapper over kernel_apply_opt.
int        tile_anno_apply_opt(struct KernelEntry *ke, KOpt opt);
// Reset axes to the default LOOP/REDUCE shape (autotune between-
// candidates baseline; preserves autotuned + version).
void       tile_anno_axes_reset(struct KernelEntry *ke);
// Recognize the im2col-fused Conv2D reduce template produced by the
// lowered UOp graph.  Renderers use this as a tile template instead
// of carrying backend-private conv pattern matchers.
int     tile_analyze_conv2d_flat(struct KernelEntry const *ke,
                                 TileConv2DInfo *out);
int     tile_rejects_conv2d_flat_cin1(struct KernelEntry const *ke);
// Matmul shape facts flow through uop_dag_classify_matmul_shape
// over ke->cached_lift.store_root; dispatch consumers route through
// the lifter-based path.

fn u32 kernel_opts_propose(struct KernelEntry const *ke, KOpt *out, u32 cap);

// Autotune: walk the proposer's candidates, time each variant
// against the baseline (no opts) with direct kernel dispatch, expand
// the best variants into short opt sequences when enabled, pick the
// winner, and leave KpSchedule mutated to the winning sequence.
// Returns 1 if a winning opt sequence was applied, 0 if baseline won.
fn int kernel_autotune(u32 kid);
fn u64 kautotune_structural_key(struct KernelEntry const *ke);

// Cheap predicate used by the fire-time auto-tune trigger.  True
// iff (env opt-in `AUTOTUNE=1` or `BEAM>0`) AND (this KpSchedule
// hasn't been autotuned yet) AND (proposer has at least one
// candidate).
fn int kernel_should_autotune(struct KernelEntry const *ke);

// Case-insensitive match of a DEV env string against a backend name
// ("cpu"/"metal"/"cuda"); a trailing ":renderer" suffix is ignored.
int thvm_dev_name_is(char const *want, char const *name);

// Temporarily suppress TJit capture recording while internal
// benchmark fires run.  The surrounding user kernel still gets
// captured normally after autotune finishes.
#define JIT_CAPTURE_EXPORT_ROW_WIDTH 16
#define JIT_REPLAY_MAX_INPUTS 64
typedef struct {
  u32 kid;
  u32 n_inputs;
  u32 out_buf_id;
  u32 in_buf_ids[JIT_REPLAY_MAX_INPUTS];
} JitReplayDispatch;
fn void jit_capture_pause(void);
fn void jit_capture_resume(void);
fn u32  jit_capture_export_ops(u32 slot, u64 *out, u32 cap_words);

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
fn void tensor_release(u32 id);   // decref + buf_decref; free at 0
fn void tensor_mark_buf_preserved(u32 id);
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

// Cross-realize loc -> tid cache (defined in schedule/materialize.c).
// Records the TenDesc tid produced by each emitted kernel's source UOP
// heap loc.  Consulted by:
//   - thvm_materialize entry, to short-circuit a UOP whose previous
//     realize pass already produced a TenDesc;
//   - bufferize_walk_rec (schedule/bufferize_classify.c), to stop the
//     classify walk at a cached UOP so subsequent passes do not
//     re-bufferize+re-emit a shared forward intermediate the first
//     pass already realized.
// Cleared by tracing GC + kernel GC sweeps when their respective IDs
// move/recycle.
fn u32  materialized_loc_lookup       (u64 loc);
fn void materialized_loc_insert       (u64 loc, u32 tid);
fn void materialized_loc_clear        (void);
fn void materialized_loc_scope_enter  (void);
fn void materialized_loc_scope_leave  (void);
fn u32  materialized_loc_scope_depth  (void);

// Build a contiguous View from a Shape.  Step 14 adds the movement ops
// (reshape / permute / expand / pad / shrink / flip).
fn View view_create(Shape shape);
fn u32  shape_numel(Shape s);

// Metal buffer pool hooks.  Real implementations live in
// src/backend/metal/_.m; the C stub supplies no-op variants on
// non-Metal builds so schedule code can call them unconditionally.
u32  thvm_metal_buf_pool_begin(void);
void thvm_metal_buf_pool_rollback_with_preserve(u32 wm);
void thvm_metal_buf_mark_preserved(u32 buf_id);
void thvm_metal_buf_clear_preserved(u32 wm);

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
// Multi-axis REDUCE: reduces over all `n_axes` axes simultaneously.
// Mirrors tinygrad uop/ops.py Ops.REDUCE with multiple range srcs.
// `uop_reduce(kind, axis, src)` is the n_axes==1 convenience.
fn Term uop_reduce_multi(u32 kind, u32 n_axes, u32 const *axes, Term src);
// REDUCE-node accessors (centralised so the heap layout can evolve
// without touching every call site).  Caller passes a TAG_UOP cell with
// op==UOP_REDUCE.
fn u32  uop_reduce_kind   (Term red);
fn u32  uop_reduce_n_axes (Term red);
fn u32  uop_reduce_axis   (Term red, u32 i);   // i in [0..n_axes)
fn Term uop_reduce_src    (Term red);
fn Term uop_reshape(Term src, u32 ndim, const u32 *dims);
fn Term uop_permute(Term src, u32 ndim, const u32 *perm);
fn Term uop_expand (Term src, u32 ndim, const u32 *dims);
fn Term uop_pad    (Term src, u32 ndim, const u32 *begin_end);   // begin_end[2*ndim]
fn Term uop_shrink (Term src, u32 ndim, const u32 *begin_end);
fn Term uop_flip   (Term src, u32 axes_bitmask);
fn Term uop_cast   (Term src, u32 dst_dtype);                    // value-preserving cast
fn Term uop_bitcast(Term src, u32 dst_dtype);                    // same-itemsize reinterpret

// === Symbolic INDEX layer ===
// Constructors for UOP_RANGE / UOP_INDEX_E / UOP_I* / UOP_IWHERE / UOP_INVALID.
// Hash-cons via uop_mov_cache like the movement opcodes.
fn Term uop_range    (u32 axis_id, u32 axis_type, u32 extent);
fn Term uop_index_e  (Term buffer, Term addr);
fn Term uop_int_binary(u32 opcode, Term a, Term b);              // IADD/ISUB/IMUL/IDIV/IMOD/ILT/IAND/IOR/IXOR
fn Term uop_iwhere   (Term cond, Term then_v, Term else_v);
fn Term uop_invalid  (void);

// === UOP_BUFFERIZE main-heap allocator ===
// 1-to-1 port of tinygrad/schedule/indexing.py:77 (`UOp(Ops.BUFFERIZE,
// s.dtype, src=(new_src,)+closed_ranges, arg=opts)`).  Hash-cons via
// uop_mov_cache.  addrspace uses UOP_SCOPE_GLOBAL / LOCAL / REG;
// removable mirrors BufferizeOpts.removable.  Accessors return 0 /
// zero-init on tag mismatch (caller validates).
fn Term uop_bufferize_new(Term value, u32 addrspace, u32 removable,
                          u32 n_ranges, Term const *ranges);
fn Term uop_bufferize_value     (Term b);
fn u32  uop_bufferize_addrspace (Term b);
fn u32  uop_bufferize_n_ranges  (Term b);
fn Term uop_bufferize_range_at  (Term b, u32 i);

// === Movement-op range swizzlers ===
// 1-to-1 port of tinygrad/schedule/indexing.py:apply_movement_op (line 129)
// per-op cases. Each rewrites the consumer's per-axis range expressions
// (`out_rngs`) into the producer's per-axis expressions (`in_rngs`).
// All variants hash-cons through uop_mov_cache.  `ndim` bounded by
// MAX_DIM. In-place (in_rngs == out_rngs) is unsafe (PERMUTE).
fn void apply_movement_op_shrink (u32 ndim, u32 const *begin_end,
                                  Term const *out_rngs, Term *in_rngs);
fn void apply_movement_op_permute(u32 ndim, u32 const *perm,
                                  Term const *out_rngs, Term *in_rngs);
fn void apply_movement_op_flip   (u32 ndim, u32 const *in_shape,
                                  u32 const *flip_mask,
                                  Term const *out_rngs, Term *in_rngs);
fn void apply_movement_op_expand (u32 ndim, u32 const *in_shape,
                                  u32 const *out_shape,
                                  Term const *out_rngs, Term *in_rngs);
fn void apply_movement_op_pad    (u32 ndim, u32 const *in_shape,
                                  u32 const *begin_end,
                                  Term const *out_rngs, Term *in_rngs);
// RESHAPE handler. Maps consumer's `out_rngs` (rank = out_ndim, with extents
// `out_shape[]`) into the producer's `in_rngs` (rank = in_ndim, extents
// `in_shape[]`) by aligning groups of axes whose cumulative products match.
// Returns 1 when the alignment succeeds (covers the common "merge contiguous
// axes" and "split one axis" cases); 0 when the shapes don't decompose into
// matching groups (e.g. a stride-trick rank-merge that needs tinygrad's
// `_apply_reshape` / `pm_simplify_valid` post-rewrite, not yet ported).
fn int  apply_movement_op_reshape(u32 out_ndim, u32 const *out_shape,
                                  u32 in_ndim,  u32 const *in_shape,
                                  Term const *out_rngs, Term *in_rngs);

// === Unified rangeify pass ===
// 1-to-1 port of tinygrad/schedule/indexing.py:148-269 (run_rangeify)
// and :101-110 (pm_apply_rangeify).  Pre-condition: caller has run
// bufferize_classify(root) so BUFFERIZE_NODES + CMAP_LL are populated.
// Populates side tables RU_RANGE_MAP / RU_REALIZE_MAP /
// RU_ENDING_RANGES / RU_SUBST in-place and writes UOP_BUFFERIZE Terms
// onto the main heap at realize boundaries.  See
// src/schedule/rangeify_unified.c for design notes.
fn void run_rangeify_unified                   (Term root);
fn void pm_apply_rangeify                      (Term root);
fn u32  rangeify_unified_last_nodes_walked     (void);
fn u32  rangeify_unified_range_idx_counter     (void);
fn int  rangeify_unified_has_ranges_at         (u32 node_idx);
fn u32  rangeify_unified_out_ndim_at           (u32 node_idx);
fn Term rangeify_unified_out_rng_at            (u32 node_idx, u32 axis);
fn int  rangeify_unified_is_realized           (u32 node_idx);
fn Term rangeify_unified_subst_at              (u32 node_idx);
// Main-heap UOP_BUFFERIZE Term at this node's realize boundary (0 if not
// a boundary). Mirrors tinygrad's Ops.BUFFERIZE landing in the tsink at
// indexing.py:77.
fn Term rangeify_unified_bufferize_at          (u32 node_idx);
// Main-heap UOP_STORE Term at this node's realize boundary (0 if not
// a boundary or if dtype inference declined).  Structurally equivalent
// to `cached_lift.store_root` from kernel_lift_to_uop; serves as the
// substrate for the lifter-bypass cutover.
fn Term rangeify_unified_store_root_at         (u32 node_idx);
fn u32  rangeify_unified_last_bufferizes_emitted(void);
// Reduce-ranges attached to a UOP_REDUCE node by the unified pass.
// Mirrors tinygrad's `src=(value,)+tuple(new_ranges)` from
// convert_reduce_to_reduce_with_ranges (indexing.py:90-96).  Returns 0
// for non-REDUCE nodes or REDUCE nodes whose reduce axes are extent-1
// (which collapse to UOP_CONST(0) per `resolve(s!=1)`).
fn u32  rangeify_unified_reduce_n_ranges_at    (u32 node_idx);
fn Term rangeify_unified_reduce_range_at       (u32 node_idx, u32 i);
// Per-axis range terms preserved at INDEX_E construction in
// pm_apply_rangeify.  Mirror: tinygrad's `BUFFERIZE.index(*per_axis_ranges)`
// at indexing.py:78.  Returns the number of axes written to out_rngs (0
// if no per-axis info was recorded for this INDEX_E heap loc).
fn u8   rangeify_unified_index_axes_lookup(u64 index_loc, Term *out_rngs,
                                           u8 cap);
// Reverse lookup: producer node_idx for a BUFFERIZE Term (scans
// RU_BUFFERIZE_TERM[]).  0xFFFFFFFFu if no match.
fn u32  rangeify_unified_node_idx_for_bufferize(Term buf);
fn u8   rangeify_unified_axes_mask_at(u32 node_idx);
fn void rangeify_unified_index_axes_register(Term index_e_term,
                                             Term const *rngs, u8 ndim);
// === UOP_RANGE field accessors + axis_type rewriter ===
// Read/write seam for UPatRule[]-driven KpSchedule -> UOP_RANGE.axis_type
// rule bodies.  Wrap the [NUM(axis_id), NUM(axis_type), NUM(extent)]
// heap layout so callers don't poke heap slots directly.  Returns 0
// / unchanged on tag mismatch.  See src/uop/index.c for design
// notes.
fn u32  uop_range_axis_id  (Term r);
fn u32  uop_range_axis_type(Term r);
fn u32  uop_range_extent   (Term r);
fn Term uop_range_with_axis_type(Term r, u32 new_axis_type);

// === uop_range_split primitive ===
// Splits one UOP_RANGE leaf into a (outer, inner) pair plus a
// `linear_index` Term that reconstructs the original axis position
// (`outer * k + inner`).  Replaces the structural-replay block in
// kernel_lift.c at the UOp DAG level: instead of mutating a SplitAxis
// vector and emitting fresh leaves, callers (a UPatRule) substitute
// `linear_index` for any consumer that referenced the original leaf.
//
// Field layout:
//   outer        : UOP_RANGE(axis=N,   axis_type=old.axis_type, extent=E/k)
//   inner        : UOP_RANGE(axis=N+1, axis_type=inner_axis_type, extent=k)
//   linear_index : IADD(IMUL(outer, k), inner)
//
// Returns {0, 0, 0} on tag mismatch / k == 0 / extent % k != 0.  The
// caller is responsible for shifting any subsequent UOP_RANGE leaves'
// axis_ids by +1 (every split inserts a new axis between N and N+1
// in the post-replay axis sequence) -- the primitive itself only
// constructs the pair; the rule body composes it into the DAG.
//
// Hash-cons: outer/inner/linear_index flow through the canonical
// uop_range / uop_int_binary / uop_const constructors, so re-running
// uop_range_split with the same inputs returns hash-cons-identical
// Terms.  See src/uop/index.c for design notes.
typedef struct {
  Term outer;
  Term inner;
  Term linear_index;
} UopRangeSplit;
fn UopRangeSplit uop_range_split(Term old_range, u32 k, u32 inner_axis_type);

// === KOP_GLOBAL UPatRule mirror (src/uop/apply_opt.c) ===
// Walks the DAG rooted at `root` and stamps any UOP_RANGE leaf whose
// axis_id matches a KOP_GLOBAL entry in `applied_opts` (with arg ==
// extent and current axis_type == KAX_LOOP) to a fresh UOP_RANGE
// with axis_type=KAX_GLOBAL.  Mirrors codegen/apply_opt.c's
// KpSchedule write; both representations stay live.  Idempotent:
// re-running the rule on a previously-stamped DAG is a no-op (the
// LOOP guard rejects KAX_GLOBAL leaves).
fn Term uop_apply_kop_global(Term root, KOpt const *applied_opts,
                             u32 n_applied);

// === KOP_SWAP UPatRule mirror (src/uop/apply_opt.c) =====
// Walks the DAG rooted at `root` and stamps each UOP_RANGE leaf
// with the axis_type produced by simulating the KOP_GLOBAL +
// KOP_SWAP history in `applied_opts` (initial state KAX_LOOP for
// all positions).  Mirrors codegen/apply_opt.c's pairwise axis_type
// swap.  Composes KOP_GLOBAL stamps and KOP_SWAPs in order so
// SWAP-after-GLOBAL produces the relabelled axis_type at the
// destination position.
// Split-class opts and KOP_TC are ignored here (axis-insertion
// drift is handled by uop_apply_split_dag).  Idempotent: the
// simulated desired state is a pure function of applied_opts.
fn Term uop_apply_kop_swap(Term root, KOpt const *applied_opts,
                           u32 n_applied);

// === Split-class UPatRule mirror (src/uop/apply_opt.c) ===
// Walks the DAG rooted at `root` and stamps each UOP_RANGE leaf
// with the axis_type produced by simulating the full applied_opts
// history (split-class KOP_UPCAST/UNROLL/LOCAL/GROUP/GROUPTOP +
// KOP_GLOBAL + KOP_SWAP) on a desired[MAX_AXES] vector.  Each split
// inserts a new position at o.axis+1 (shifting later positions
// right): outer at o.axis keeps its axis_type, inner at o.axis+1
// takes the opt's KAX_ type (UPCAST/UNROLL/LOCAL/GROUP_REDUCE).
// GLOBAL stamps KAX_GLOBAL on a LOOP position; SWAP swaps two
// positions.  Other KOP_* (KOP_TC, KOP_PADTO, KOP_NOLOCALS) are
// no-ops (mirroring axes_apply_opt).
//
// Stamp-only: this rule fixes up axis_type on already-emitted
// leaves; the DAG-level split itself lives in uop_apply_split_dag.
// Idempotent: desired[a] is a pure function of applied_opts.
fn Term uop_apply_kop_split(Term root, KOpt const *applied_opts,
                            u32 n_applied);

// === KOP_TC UPatRule mirror (src/uop/apply_opt.c) =========
// Walks the DAG rooted at `root` and stamps each UOP_RANGE leaf
// with the axis_type produced by simulating the full applied_opts
// history.  KOP_TC is kernel-aware metadata (tensor-core hint) and
// contributes NO axis_type mutation: its effect lives in
// render_uop.c's matmul-TC pattern matcher and the uop_recognise_tc
// producer that wraps the matmul reduce in UOP_OPT(_, TC, 0).
// Mirrors codegen/apply_opt.c's kernel_apply_opt routing of KOP_TC
// through tile_anno_record_opt (appends to applied_opts[] without
// touching axis_types[]).  Composes with the other rules via the
// shared sim_kop_history.  Idempotent: desired[a] is a pure
// function of applied_opts.
fn Term uop_apply_kop_tc(Term root, KOpt const *applied_opts,
                        u32 n_applied);

// === uop_apply_split_dag UPatRule =====================================
// Walks the DAG rooted at `root`, applies every split-class entry in
// `applied_opts` (UPCAST/UNROLL/LOCAL/GROUP/GROUPTOP) at the UOp DAG
// level via the uop_range_split primitive, and returns the rewritten
// root.  Operates on EMITTED UOp DAGs: replaces each pre-split
// UOP_RANGE leaf at axis A with the (outer * k + inner) sub-expression
// uop_range_split returns, and propagates the change through every
// IADD/IMUL chain that consumed the original leaf (E8's
// uop_arity / uop_graph_rebuild_with_srcs descent makes the rewriter
// reach the leaves nested inside INDEX_E.addr trees).
//
// Pre-condition: the input DAG has no split block applied yet -- each
// pre-split axis position N appears as a UOP_RANGE leaf with axis_id=N
// and the pre-split extent.  GLOBAL / SWAP / TC stamping is the job of
// uop_apply_kernel_opts (which composes via the same simulator);
// this rule deliberately ignores those classes.
//
// Idempotent: a second pass detects post-split sentinels (RANGE leaves
// at axis_id > a with the inner_kax-stamped axis_type and arg-matched
// extent) and bails when ALL referenced split opts are already
// represented.  See src/uop/apply_opt.c for the full simulation.
fn Term uop_apply_split_dag(Term root, KOpt const *applied_opts,
                            u32 n_applied);

// === Buffer leaf ===
// Construct a UOP_BUFFER leaf with `scope` (UOP_SCOPE_GLOBAL/LOCAL/REG),
// `dtype` (DT_FP32/etc.), and `ndim` dimensions in `dims`.  Hash-cons via
// uop_mov_cache: identical (scope, dtype, ndim, dims) tuples share heap
// loc.  Defaults: scope=GLOBAL captures today's implicit-buffer behavior
// and lets D'1 land without consumer rewrites.
fn Term uop_buffer(u32 scope, u32 dtype, u32 ndim, const u32 *dims);

// Variant with `instance` disambiguator.  Two UOP_BUFFER terms with
// the same (scope, dtype, ndim, dims) but different instance fields
// hash-cons to distinct Terms.  Used to keep kernel-arg buffers
// distinguishable when slots happen to share shape.
fn Term uop_buffer_inst(u32 scope, u32 dtype, u32 ndim, const u32 *dims,
                        u32 instance);

// Read accessors for UOP_BUFFER fields (returns 0 on tag mismatch).
fn u32  uop_buffer_scope(Term t);
fn u32  uop_buffer_dtype(Term t);
fn u32  uop_buffer_ndim (Term t);
fn u32  uop_buffer_dim  (Term t, u32 d);   // 0 if d >= ndim
fn u32  uop_buffer_inst_get(Term t);       // 0 if not UOP_BUFFER

// === DAG read-side scanners ===
// Helpers used by metal_kernel_supported / propose.c to derive
// per-kernel facts from a lifted UOp DAG without re-running the
// lifter.  Every helper returns a safe default (0 / "uniform") on
// `root == 0` so callers can chain them with a
// `cached_lift.store_root != 0` gate.

// 1 iff every dtype-carrying node (BUFFER, CONST, CAST/BITCAST dst)
// reachable from `root` has dtype `dt`.  Treats RANGE / I* / INVALID
// (index-domain) as uniform.  External linkage so the metal backend
// (compiled as a separate translation unit) can call it.
int uop_dag_dtype_uniform(Term root, u32 dt);

// Find the first UOP_RANGE leaf with axis_type == KAX_REDUCE (==1)
// reachable from `root` and return its extent; 0 if none.
u32 uop_dag_reduce_axis_extent(Term root);

// 1 iff at least one UOP_REDUCE is reachable from `root` AND every
// reachable op is in the metal reduce-unroll accepted set (mirrors
// propose_metal_reduce_unroll_kernel's UOp-DAG gate).
int uop_dag_is_reduce_unroll_kernel(Term root);

// Enumerate the distinct UOP_RANGE leaves of the DAG rooted at `root`,
// ordered by ascending axis_id; writes (axis_id, axis_type, extent)
// triples to the parallel out_* arrays.  Returns the count (clipped to
// `cap`).  Used by kernel_hand_coded_opts to inspect the current axis
// structure between kernel_apply_opt mutations.
u32 uop_dag_collect_axes(Term root, u32 *out_axis_id, u32 *out_axis_type,
                         u32 *out_extent, u32 cap);

// Apply tinygrad's hand_coded_optimizations heuristic to a finalized
// kernel: inspects shape/dtype and applies a sensible sequence of
// KOpts via kernel_apply_opt (TC -> UPCAST -> LOCAL -> GROUP ->
// UNROLL).  Idempotent: marks ke->schedule->autotuned so it doesn't
// re-run.  Gated behind HAND_CODED_OPTS (see hand_opts.c).
// Returns the number of opts successfully applied.
fn u32 kernel_hand_coded_opts(struct KernelEntry *ke);

// === Address-coefficient decode (UOP_INDEX_E.addr) ===================
// Externally-callable wrappers over dag_scan.c's static decoders so the
// hand_coded_optimizations heuristic (codegen/hand_opts.c) can implement
// tinygrad's stride heuristic for UPCAST axis selection.
//
// UopDagAddrCoeffsView: minimal projection of UdgAddrCoeffs (axis-id +
// coeff pairs).  HAND_OPT_MAX_AXES bounds n; offsets beyond the count
// are not initialised.
#define UOP_DAG_ADDR_MAX_AXES 8
typedef struct {
  u32 axis_ids[UOP_DAG_ADDR_MAX_AXES];
  u32 coeffs  [UOP_DAG_ADDR_MAX_AXES];
  u32 n_axes;
  i32 offset;
  int ok;
} UopDagAddrCoeffsView;

// Decode an UOP_INDEX_E.addr term into per-axis (axis_id, coeff) pairs
// and an absolute byte offset.  Mirrors tinygrad's index.backward_slice
// + IADD/IMUL distribution.  On bail (unrecognized leaf shape, e.g. an
// IDIV/IMOD/IWHERE remnant from a conv2d-flat index that the simplifier
// didn't collapse), out->ok stays 0 but out->n_axes carries whatever was
// decoded so far.
void uop_dag_decode_addr_coeffs(Term addr, UopDagAddrCoeffsView *out);

// Look up a coefficient by axis_id.  Returns 0 when absent (which the
// stride heuristic also treats as "stride 0 on this axis" -- the axis
// does not contribute to the address).
u32  uop_dag_addr_coeff_lookup(UopDagAddrCoeffsView const *a, u32 axis_id);

// Enumerate every distinct UOP_INDEX_E.addr term reachable from `root`
// (dedup by Term identity).  Writes up to `cap` addresses to out_addrs.
// Returns the count.  The caller decodes each via
// uop_dag_decode_addr_coeffs.  Mirrors tinygrad's Scheduler.bufs (the
// reversed list of Ops.INDEX nodes the heuristic iterates).
u32  uop_dag_collect_index_e_addrs(Term root, Term *out_addrs, u32 cap);

// Should this kernel get the hand-coded opt heuristic on its next
// fire?  True iff the env opt-in is on (default ON; HAND_CODED_OPTS=0
// or NOOPT=1 disables) AND the per-shape autotuned flag is still 0.
fn int kernel_should_hand_code_opts(struct KernelEntry const *ke);

// === Slice 5 decode shims (Metal-TU-callable) =========================
// Thin external-linkage wrappers over heap_read / term_* / UOp
// predicates so the Metal backend (separate TU) can walk a DAG without
// pulling the main TU's static-inline accessors transitively.
//
// uop_dag_decode_uop: returns 1 with (*out_op, *out_loc) populated
// when `t` is a TAG_UOP; 0 otherwise.
int uop_dag_decode_uop    (Term t, u32 *out_op, u64 *out_loc);

// UOP_BUFFER's instance disambiguator (0 = output buffer; 1..N =
// input slot N-1).
u32 uop_dag_buffer_instance(Term t);

// UOP_CONST payload decode: dtype + raw bits.  Returns 1 on success.
int uop_dag_const_payload (Term t, u32 *out_dtype, u32 *out_bits);

// heap_read shim: read `offset`-th cell of the heap slot at `loc`.
Term uop_dag_heap_read    (u64 loc, u32 offset);

// Elementwise-classification predicates with external linkage.
int uop_dag_is_unary_ew   (u32 op);
int uop_dag_is_binary_ew  (u32 op);

// === DAG-side GEMM-shape extractor =====================================
// Recover the matmul facts (M, N, K, input slot mapping, ldA/ldB,
// transpose flags) from the lifted UOp DAG
// (ke->cached_lift.store_root) plus ke->input_views[].
// See src/uop/dag_scan.c for the matching strategy.
typedef struct {
  u32 dtype;
  u32 M;
  u32 N;
  u32 K;
  u32 a_input;
  u32 b_input;
  u32 ldA;
  u32 ldB;
  u32 flags;     // bit 0 = transposed A, bit 1 = transposed B
} UopDagGemmShape;

int uop_dag_classify_matmul_shape(Term root,
                                  struct KernelEntry const *ke,
                                  UopDagGemmShape *out);

// Extract per-arm coefficients from a 2-D matmul-style INDEX_E
// address built by lift_scalar_index.  The
// address pattern for the matmul A operand under a 3-axis [M,K,N] lift
// is either `IADD(IMUL(r_m, ICONST(K)), r_k)` (untransposed; ldA=K) or
// `IADD(r_m, IMUL(r_k, ICONST(M)))` (transposed; ldA=M) -- and similarly
// for B with the {k,n} pair, GEMV-W with {m,k}.  Both arms are decoded
// raw: each is either bare RANGE (coefficient = 1) or IMUL(RANGE, ICONST)
// (coefficient = ICONST.value).  The caller derives `ld` / `trans` from
// the side-specific BLAS convention:
//   matmul A: ldA = other_coeff, transA = (red_coeff != 1)
//             except both flip when the address's k arm carries the
//             non-1 coefficient, so robust formula:
//                 ldA   = max(red_coeff, other_coeff)
//                 transA = (red_coeff != 1)
//   matmul B: ldB = max(red_coeff, other_coeff)
//             transB = (other_coeff != 1)   -- B's "natural" layout
//                                              has red_coeff != 1
//   gemv W : same as matmul A
// Returns 1 on success.  Returns 0 when the address doesn't match
// IADD-of-(arm,arm) with each arm RANGE or IMUL(RANGE,ICONST), or when
// neither arm references `red_axis_id`, or when both arms reference the
// same axis.
int uop_dag_extract_matmul_strides_from_addr(Term addr, u32 red_axis_id,
                                             u32 *out_red_coeff,
                                             u32 *out_other_coeff,
                                             u32 *out_other_axis_id);

// DAG-side shape extractors for DOT and GEMV.  Mirror
// uop_dag_classify_matmul_shape for the two simpler BLAS shapes.
//
// DOT: STORE(out_buf, _, REDUCE_SUM(MUL(INDEX_E(A, k), INDEX_E(B, k))))
//   - out_buf is rank-0 or rank-1 numel=1
//   - A, B are rank-1 contig with K elements
//   - Recovers (dtype, K, a_input, b_input).
//
// GEMV: STORE(out_buf, _, REDUCE_SUM(MUL(INDEX_E(W, m*K+k),
//                                         INDEX_E(x, k))))
//   - out_buf is rank-1 numel=M
//   - W view is rank-2 {M, K} contig with strides {K, 1}
//   - x view is rank-2 {M, K} broadcast strides {0, 1} or rank-1 {K}
//     strides {1}, etc.
//   - Recovers (dtype, M, K, w_input, x_input, ldW, transW).
typedef struct {
  u32 dtype;
  u32 K;
  u32 a_input;
  u32 b_input;
} UopDagDotShape;

typedef struct {
  u32 dtype;
  u32 M;
  u32 K;
  u32 w_input;
  u32 x_input;
  u32 ldW;
  u32 flags;     // bit 0 = transposed W (storage strides {1, M})
} UopDagGemvShape;

int uop_dag_classify_dot_shape (Term root,
                                struct KernelEntry const *ke,
                                UopDagDotShape *out);

int uop_dag_classify_gemv_shape(Term root,
                                struct KernelEntry const *ke,
                                UopDagGemvShape *out);

// DAG-side structural gate for conv2d_flat kernels.  Checks that
// cached_lift.store_root's STORE.value is a UOP_REDUCE with
// REDUCE_SUM kind, optionally wrapped in UOP_OPT(_, CONV, 0) by the
// conv recogniser.  No shape extraction is needed -- the caller
// still reads extents from ke->input_views[].
//
// Returns 1 on match.  No extents-out parameter -- conv2d_flat's
// shape lives in input_views, not in the lifted DAG.
int uop_dag_classify_conv2d_flat_shape(Term root);

// === Generalized contraction-shape extractor ==========================
//
// Covers a generalized tensor contraction of the form
//
//   out[M_axes, N_axes] = sum_{K} A[K_axis, N_axes] * B[M_axes, K_axis]
//
// (with M_axes and N_axes being compound, axis ordering arbitrary on
// each operand).  The conv-backward x-gradient kernel
// `out[a0,a1,a2,a3,a4,a5] = sum_{a6} W[a6,a3,a4,a5] * G[a0,a6,a1,a2]`
// is the canonical instance: M=(a0,a1,a2), N=(a3,a4,a5), K=a6.  When
// the strides on each side compose as a contiguous nest with at most
// ONE outer "batch" M-axis (whose stride in B is K*inner_M, separating
// the inner-M block from K), this dispatches as a batched cblas_sgemm
// without materializing any permuted operand.
//
// Per-call GEMM layout (after batching over `batch`):
//   sub_A : shape (K, N)                stride (ldA, 1)   -- W slice
//   sub_B : shape (K, inner_M)          stride (ldB, 1)   -- G slice
//   sub_C : shape (inner_M, N)          stride (ldC, 1)   -- out slice
//   cblas_sgemm(RowMajor, Trans, NoTrans, inner_M, N, K,
//               1, sub_B, ldB, sub_A, ldA, 0, sub_C, ldC)
typedef struct {
  u32 dtype;
  u32 a_input;            // L (typically weights / "(K, N)")
  u32 b_input;            // R (typically activations / "(M, K)" possibly batched)
  u32 K;                  // reduce extent
  u32 N;                  // product of N-axis extents (axes in A only)
  u32 inner_M;            // product of M-axis extents excluding the batch axis
  u32 batch;              // outer-loop iteration count (1 when no batch axis)
  u32 ldA;                // leading dim for A's (K, N) slice    (== N here)
  u32 ldB;                // leading dim for B's (K, inner_M) slice (== inner_M)
  u32 ldC;                // leading dim for C's (inner_M, N) slice (== N)
  u32 batch_stride_a;     // element-stride between A batches (0 if A has no batch axis)
  u32 batch_stride_b;     // element-stride between B batches
  u32 batch_stride_c;     // element-stride between C batches
  u32 flags;              // reserved
} UopDagContractionShape;

int uop_dag_classify_contraction_shape(Term root,
                                       struct KernelEntry const *ke,
                                       UopDagContractionShape *out);

// === im2col conv contraction extractor ================================
//
// Covers two duals of the same im2col fingerprint:
//
//   (A) BACKWARD weight-grad:
//       dW[Cout, Cin, KH, KW] = sum_{B, OH, OW}
//                                dY[B, Cout, OH, OW] * X[B, Cin, OH+KH, OW+KW]
//
//   (B) FORWARD conv:
//       out[B, Cout, OH, OW] = sum_{Cin, KH, KW}
//                                X[B, Cin, OH+KH, OW+KW] * W[Cout, Cin, KH, KW]
//
// In both cases two of the reduce K-axes share their stride coefficient
// in operand A (X) with a non-reduce output axis.  C7.1 extends
// udg_decode_addr_coeffs to distribute IMUL over IADD so the
// `(OH+KH)*W` and `(OW+KW)*1` address arms expand and each axis -- K
// AND patch-M -- gets its own coefficient entry.  The classifier picks
// the patch-M axes out of the output axes (any output axis whose
// A-stride matches one of the K-axes is reclassified from N-axis to
// patch-M).
//
// The two forms differ only in which axis plays the "outer batch" role:
//   (A) BWD: B is a K-axis (sum over batch), Cin is a clean N axis.
//            The outer loop walks B and dW accumulates (beta=1 from b>0).
//   (B) FWD: Cin is a K-axis (sum over channels), B is a clean N axis.
//            The outer loop walks B and writes a different output slice
//            each iteration (beta=0).
//
// Tinygrad lowers BOTH forms through the same `_pool` ShapeTracker view
// (mixin/movement.py:569); thvm doesn't have a `_pool` lowering yet, so
// this classifier-plus-dispatch combo is the equivalent shortcut.
//
// Per-call layouts:
//   BWD: patches[Cin*KH*KW, OH*OW] from X[b]; sub_dY = dY[b] (Cout, OH*OW);
//        sgemm(NoTrans, Trans, M=Cout, N=Cin*KH*KW, K=OH*OW, dY[b], patches,
//              beta=(b==0?0:1), dW)  -- dW shape (Cout, Cin*KH*KW).
//   FWD: patches[OH*OW, Cin*KH*KW] from X[b]; W (Cout, Cin*KH*KW);
//        sgemm(NoTrans, Trans, M=Cout, N=OH*OW, K=Cin*KH*KW, W, patches,
//              beta=0, out[b])  -- out[b] shape (Cout, OH*OW).
typedef struct {
  u32 dtype;
  u32 a_input;            // X (the im2col operand) -- both forms
  u32 b_input;            // dY (BWD) or W (FWD)    -- the clean operand
  u32 M;                  // GEMM M = Cout-style axis (output axis with bc!=0)
  u32 N_patchless;        // BWD: product of non-patch clean-N axes (Cin)
                          // FWD: product of non-patch unpaired-K axes (Cin)
  u32 KH_total;           // product of patch-M extents (KH*KW)
  u32 N;                  // sgemm-N in BWD = N_patchless * KH_total = Cin*KH*KW
                          // sgemm-N in FWD = K_row * K_col = OH*OW
                          // (kept for backward-compat: BWD-only field)
  u32 K_row;              // OH-style patch-paired output axis extent
  u32 K_col;              // OW-style patch-paired output axis extent (innermost K)
  u32 K_outer;            // BWD: B-style outer-K extent (loop bound)
                          // FWD: Cin-style outer-K extent (inner sgemm-K factor)
  u32 X_W;                // X width stride (a-stride of K_row patch axis)
  u32 X_H;                // X height (inferred from X_Cin_stride / X_W)
  u32 X_Cin_stride;       // X's Cin stride (a-stride of the unpaired K axis in FWD
                          // or of the clean-N axis in BWD; equals H*W either way)
  u32 X_outer_stride;     // X's batch stride
                          // BWD: a-stride of K_outer (= N_extent_used * X_Cin_stride)
                          // FWD: a-stride of N_outer (= K_outer.extent * X_Cin_stride)
  u32 Y_outer_stride;     // BWD: dY's batch stride (b-stride of K_outer)
                          // FWD: 0 (W has no batch axis)
  u32 Y_M_stride;         // b-stride of M-axis in operand B
                          // BWD: dY's Cout stride = OH*OW
                          // FWD: W's   Cout stride = Cin*KH*KW
  u32 out_M_stride;       // out's M-axis stride
                          // BWD: dW's   Cout stride = Cin*KH*KW (= N)
                          // FWD: out's Cout stride = OH*OW
  u32 patch_n;            // number of patch axes (2 for KH+KW)
  u32 patch_extent[8];    // per-patch-axis extent (KH first, then KW)
  u32 patch_out_coeff[8]; // per-patch-axis out-coeff
  u32 patch_a_stride[8];  // per-patch-axis a-stride (gathered into patches)
  u32 forward_conv;       // 0 = BWD weight-grad (default), 1 = FWD conv
  u32 N_outer_extent;     // FWD only: B's extent (clean-N axis = outer loop bound)
                          // BWD: equal to K_outer (B's extent)
  u32 N_outer_out_stride; // FWD only: B's out-stride (= Cout*OH*OW)
                          // BWD: 0 (dW has no batch dimension; same slot every iter)
} UopDagIm2colShape;

int uop_dag_classify_im2col_contraction(Term root,
                                        struct KernelEntry const *ke,
                                        UopDagIm2colShape *out);

// === conv2d-flat full-shape extractor ==================================
// Inverts the conv2d-flat IDIV/IMOD address decomposition so the
// conv2d shape facts can flow from the lifted DAG instead of from
// ke->input_views[].
//
// In-scope: single-input non-degenerate conv2d (c_in*kh*kw > 1, kh>=1,
// kw>=1; multi-input IWHERE chain handled by the caller via the
// existing input_views fallback).
//
// Returns 1 with `out` filled iff the W and X addresses both decode
// cleanly; 0 otherwise (caller falls back to the input_views reader).
typedef struct {
  u32 c_out;
  u32 c_in;
  u32 kh;
  u32 kw;
  u32 h_out;
  u32 w_out;
  u32 batch;
  u32 patches;          // batch * spatial_patches
  u32 spatial_patches;  // h_out * w_out
  i32 w_offset;
  i32 w_stride0;
  i32 w_stride1;
  i32 x_offset;
  i32 x_stride_b;
  i32 x_stride0;
  i32 x_stride1;
  i32 x_stride2;
  u32 axis_r_out;       // RANGE id for the LOOP/output axis (= 0 from lifter)
  u32 axis_r_q;         // RANGE id for the REDUCE/q axis    (= 1 from lifter)
} UopDagConv2dFlatShape;

int uop_dag_extract_conv2d_flat_shape(Term addr_w, Term addr_x,
                                      UopDagConv2dFlatShape *out);

// === Store + After ===
// UOP_STORE writes `value` to `buf` at symbolic `addr`.  T.copy maps to
// `STORE(dst, addr, INDEX_E(src, addr))`.  Hash-cons by (buf, addr, value).
fn Term uop_store(Term buf, Term addr, Term value);

// Read accessors.
fn Term uop_store_buf  (Term t);   // 0 on tag mismatch
fn Term uop_store_addr (Term t);
fn Term uop_store_value(Term t);

// UOP_AFTER expresses ordering between sibling side-effects.  `node`
// happens after `after_node`.  Backends emit a barrier when this
// crosses a scope boundary, or a warp shuffle when crossing REG.
// Hash-cons by (node, after_node).
fn Term uop_after(Term node, Term after_node);

fn Term uop_after_node      (Term t);
fn Term uop_after_after_node(Term t);

// === Opt annotation ===
// Attach an optimisation directive (UOP_OPT_UNROLL, UPCAST, TC,
// LOCAL, GROUP_REDUCE) to `target`.  `factor` is 0 when the kind
// carries no scalar (TC, LOCAL).  Hash-cons by (target, kind, factor).
fn Term uop_opt(Term target, u32 kind, u32 factor);

fn Term uop_opt_target(Term t);   // 0 on tag mismatch
fn u32  uop_opt_kind  (Term t);
fn u32  uop_opt_factor(Term t);

// Recognise the matmul shape on a UOP_STORE root and wrap the inner
// REDUCE with UOP_OPT(_, TC, 0) so render_uop's simdgroup_matrix
// template fires. Returns the input root unchanged on any non-match.
// See src/uop/recognise_tc.c for the exact pattern.
fn Term uop_recognise_tc(Term root);

// Detection-only: returns 1 if `root` is a matmul-shape STORE
// (REDUCE-of-MUL on two distinct INDEX_E buffers, SUM kind). Fills
// *out_k_extent with the reduce-axis extent if statically known
// (zero otherwise). Used by uop_recognise_tc to decide whether to
// wrap with UOP_OPT(_, TC) so render_uop's simdgroup_matrix template
// fires (when K%8==0) versus falling back to its generic accumulator.
fn int uop_classify_matmul(Term root, u32 *out_k_extent);

// Structural classifiers for the DOT and GEMV
// shapes.  Mirrors uop_classify_matmul but with different range-count
// signatures: DOT addresses each touch exactly 1 distinct UOP_RANGE
// (the reduce axis); GEMV has one address with 2 ranges (matrix m,k)
// and the other with 1 (broadcast vector k).  *out_w_first selects
// which MUL operand carries the matrix (1 = src[0], 0 = src[1]).
// Both return 1 + reduce-axis extent on match; 0 otherwise.
fn int uop_classify_dot (Term root, u32 *out_k_extent);
fn int uop_classify_gemv(Term root, u32 *out_k_extent, int *out_w_first);

// Recognise the conv2d_flat shape on a UOP_STORE root and wrap the
// inner REDUCE with UOP_OPT(_, CONV, 0) so render_uop's conv template
// fires. Returns the input root unchanged on any non-match.
// See src/uop/recognise_conv.c for the exact pattern.
fn Term uop_recognise_conv(Term root);

// Detection-only: returns 1 if `root` is a conv2d_flat-shape STORE
// (REDUCE-of-MUL with W*X structure where at least one INDEX_E address
// contains UOP_IDIV / UOP_IMOD, signalling decomposed conv axes).
// Fills *out_kred with the reduce-axis extent if statically known
// (zero otherwise). The CONV template currently always emits the
// generic accumulator path so kred isn't load-bearing yet -- present
// for symmetry with uop_classify_matmul + future tile-size gates.
fn int uop_classify_conv2d(Term root, u32 *out_kred);

// Direct-multi-axis conv classifier: detects the conv2d shape the
// multi-axis-REDUCE port (commit 598055ee) emits when shapes stay
// un-flattened (reduce has separate Cin/kH/kW axes, no IDIV/IMOD).
// Fills *out_kred with the product of all reduce-axis extents.
fn int uop_classify_conv2d_direct(Term root, u32 *out_kred);

// "Either form" wrapper.  Used by callers that just need to know if
// this is a conv kernel (flat OR direct multi-axis), without caring
// which form -- e.g. hand_opt_is_conv_kernel for LOCAL/UPCAST gating.
fn int uop_classify_conv2d_any(Term root, u32 *out_kred);

// === Kernel lift to UOp DAG ===
// Package the unified-rangeify pass's store_root for a kernel entry
// into a UOp DAG root suitable for cg_render_uop_kernel.  The lifter
// today is a thin short-circuit -- it looks up the unified pass's
// per-kernel output and wraps it as a KernelUopLift; the renderer /
// DAG-side encoder / cpu_uop_walk all consume the result.
//
// `KernelUopLift` + `KERNEL_LIFT_MAX_INPUT` are forward-defined above
// (right before `KernelEntry`) so the kernel entry can embed the lift
// output by-value as `cached_lift`.  See that block for the typedef.
fn int kernel_lift_to_uop(struct KernelEntry const *ke,
                          KernelUopLift *out);

// Shadow-render counters.  Track how many scheduled kernels
// lift cleanly to UOp DAG 
// path flip readiness).  Reset by thvm_init / thvm_free.
fn u64  kernel_lift_attempts(void);
fn u64  kernel_lift_successes(void);
fn void kernel_lift_count_attempt(void);
fn void kernel_lift_count_success(void);

// Bypass-substitution telemetry: how many kernels the unified-pass
// store_root replaced the legacy lifter's output for, vs how many
// the safety gates declined.  Per-gate breakdown distinguishes
// residual-BUFFERIZE / stranded-RANGE / broadcast-input declines.
fn u64  bypass_kernel_total_count       (void);
fn u64  bypass_kernel_used_unified_count(void);
fn u64  bypass_gate_resid_count         (void);
fn u64  bypass_gate_stranded_count      (void);
fn u64  bypass_gate_bcast_count         (void);

// === UOp DAG renderer ===
// Walks the UOp DAG rooted at `root` and emits pseudo-MSL.  Replaces
// the kernel-output store walker.
//
// `root` is typically a UOP_STORE (single-store kernel) or a chain
// of UOP_AFTER nodes (multi-store kernel).  `out_buf` and `in_bufs`
// provide the kernel's buffer-binding contract; the renderer types
// them using uop_buffer_dtype.
fn void cg_render_uop_kernel(Term root, const char *kernel_name,
                             Term out_buf, Term const *in_bufs,
                             u32 n_inputs, FILE *fp);
// F6: same UOp DAG, emitted as a C99 kernel for the CPU JIT path.
// Function signature is the CPU-JIT contract dlsym'd by
// cpu_jit_dispatch:
//   void k(void *out_v, const void *const *ins_v,
//          unsigned n, const unsigned *in_numels);
fn void cg_render_uop_kernel_c(Term root, const char *kernel_name,
                               Term out_buf, Term const *in_bufs,
                               u32 n_inputs, FILE *fp);

// Structural-mode entry points.  Discover the kernel's buffer slots
// from the DAG itself via UOP_BUFFER.instance (kernel_lift.c sets
// instance=0 on the output and instance=slot+1 on input slot k).
// Production callers (cg_emit_via_uop, cpu_jit_build) pass
// ke->cached_lift.store_root directly; no out_buf/in_bufs[] tuple
// needed.
fn void cg_render_uop_kernel_root(Term root, const char *kernel_name,
                                  FILE *fp);
fn void cg_render_uop_kernel_c_root(Term root, const char *kernel_name,
                                    FILE *fp);
// CUDA counterpart: emits an `extern "C" __global__` kernel with the
// thread-builtin prologue (blockIdx/blockDim/threadIdx) in place of
// Metal `[[ ... ]]` attributes.  The CUDA backend runtime (Stage 2)
// passes the post-lift store root, same as the MSL/C99 entries.
fn void cg_render_uop_kernel_cuda_root(Term root, const char *kernel_name,
                                       FILE *fp);
// Renumber a<N>/_acc<N> ids to a dense per-kernel sequence so identical
// kernels render byte-identically across steps (JIT cache hit).
fn char *cg_canonicalize_axis_ids(const char *src);

// === Per-USE movement-chain resolver ===
// Strip UOP_PERMUTE/RESHAPE/EXPAND/PAD/SHRINK/FLIP layers from `src`,
// outside-in, transforming `iters[ndim_io]` to the iter context the
// bottom buffer expects.  Returns the bottom term (non-movement).
// Returns 0 on shape mismatch so callers can bail.  Per-USE caller
// builds the final UOP_INDEX_E from the
// resolved iters + bottom buffer shape.
//
// `valid_mask_io` accumulates PAD bounds-check expressions (IAND'd
// per-axis ILT's).  Initialise *valid_mask_io = 0 = "no constraint";
// after resolution, the caller wraps the LOAD in
// IWHERE(*valid_mask_io, LOAD, INVALID).  Pass NULL to disallow PAD
// (helper bails when it hits one).
fn Term uop_resolve_movement_chain(Term src, Term *iters, u32 *ndim_io,
                                   Term *valid_mask_io);

typedef struct {
  Term term;
  u8   op;
  u8   arity;
  u64  loc;
  Term src[MAX_UOP_SRC];
} UOpView;
fn u8   uop_is_movement(u8 op);
fn int  uop_view(Term t, UOpView *out);
fn int  uop_view_op(Term t, u8 op, UOpView *out);
fn Term uop_view_src(UOpView const *view, u8 idx);

typedef Term (*UOpGraphRewriteFn)(Term t, void *user);
typedef struct {
  char const        *name;
  UOpGraphRewriteFn apply;
} UOpGraphRewriteRule;

// Declarative UPat layer on top of UOpGraphRewriteRule.  See
// docs/plans/autotune_beam_profile.md Level 45.  Each UPatRule
// compiles into a UOpGraphRewriteFn that does the pattern match
// then calls the user-provided rewrite fn.
//
// op     : expected op, or 0 (UOP_NONE) for "any op"
// nsrc   : expected src count, or 0xFF for "any"
// dtype  : expected dtype, or 0 for "any"
// bind   : index into the rule's Term bindings[N] array, or -1
// src    : pointer to nsrc UPat children (NULL if nsrc == 0)
// op_alt : NULL, or pointer to a 0-terminated u8 array of
//          acceptable opcodes; when non-NULL, t_op must appear in
//          the array and `op` is ignored.  Lets one pattern match
//          a small set, e.g. {UOP_ADD, UOP_MUL, 0} for the recurring
//          "ALU with const sibling" subpatterns in bufferize chain
//          walkers.  Existing 5-field static initializers stay
//          source-compatible (op_alt zero-inits to NULL).
typedef struct UPat {
  u8                op;
  u8                nsrc;
  u8                dtype;
  i8                bind;
  struct UPat const *src;
  u8 const         *op_alt;
} UPat;

#define UPAT_NUM_BINDINGS 8

typedef Term (*UPatRewriteFn)(Term const *bindings, void *ctx);

typedef struct {
  UPat const   *pat;
  UPatRewriteFn rewrite;   // renamed from `fn` to avoid #define fn collision
} UPatRule;

// upat_match: definition in src/uop/upat.c.  Single-TU unity
// build (no header forward decl needed).
fn Term uop_graph_rewrite(Term root,
                          UOpGraphRewriteRule const *rules,
                          u32 n_rules,
                          void *user);
fn u32  uop_graph_rewrite_stat_hits(char const *name);
fn Term uop_graph_simplify(Term root);
fn Term uop_graph_simplify_checked(Term root, u32 env_id);
fn Term uop_graph_simplify_materialize(Term root, u32 env_id);

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

// Build a UOP_DETACH node wrapping `src` (stop-gradient; see UOP_DETACH).
fn Term uop_detach(Term src);


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
// UOp-DAG source emitter (render_uop.c) + per-kid dispatch profiling.
// Exposed cross-TU so the Metal .m file (compiled separately under
// -DTHVM_HAS_METAL) can render MSL via cg_emit_metal ->
// cg_emit_tile_metal -> cg_emit_via_uop and record dispatches into
// the shared K_PROFILE table.  cg_supports remains the pre-build
// gate for the CPU JIT (clang -shared); the Metal side and the CPU
// JIT both route through render_uop's lifter (post-F6).

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
  KDISPATCH_METAL_JIT   = 6,   // [retired in 88f536c3 -- metal_jit_encode deleted]
  KDISPATCH_METAL_OP    = 7,   // Metal: per-op shader fallback (DAG-side encoder over cached_lift)
  KDISPATCH_CPU_TILE    = 8,   // [retired -- cpu_dispatch_tile deleted; slot reserved]
  KDISPATCH_METAL_TILE  = 9,   // Metal: render_uop UOp-DAG -> MSL -> single-encoder dispatch
  KDISPATCH_METAL_GEMM  = 10,  // [retired in 4e30432b -- metal_try_gemm deleted]
  // 11 was KDISPATCH_METAL_CONV (retired; metal_try_conv2d_flat was a
  // diagnostic-only branch gated on THVM_METAL_SPECIALIZED, deleted in
  // 97d58c32 -- conv shapes now route through render_uop's generic
  // accumulator). 12 was KDISPATCH_METAL_GEMV (retired; rank-1 matvec
  // routes through METAL_GEMM). Slots reserved to keep
  // KDISPATCH_METAL_ALIAS = 13 stable for any external integer-keyed
  // consumer of dispatch kinds.
  KDISPATCH_METAL_ALIAS = 13,  // Metal: metadata-only alias, no command encoding
  KDISPATCH_CUDA_JIT    = 14,  // CUDA: nvrtc-compiled kernel via cuLaunchKernel
} KDispatchKind;

int   cg_supports(KernelEntry const *ke);
char *cg_emit_metal(KernelEntry const *ke);   // caller frees
char *cg_emit_tile_metal(KernelEntry const *ke);   // caller frees
int   cg_tile_metal_dispatch_shape(KernelEntry *ke, u32 *groups_x, u32 *threads_x);
u64   cg_now_us(void);
void  cg_profile_record(u32 kid, KDispatchKind kind, u64 elapsed_us);
// Record a true per-kernel GPU-time sample (us).  Metal-only, gated on
// THVM_METAL_PROFILE_PEROP=1.  External linkage so the .m TU can call it.
void  cg_profile_record_gpu(u32 kid, u64 gpu_us);
u32   cg_kernel_dispatch_kind(u32 kid);
// THVM_KERNEL_PROFILE=N: dump top-N kernels by cumulative wall time
// at thvm_free. N=0 -> default 20. Uses K_PROFILE + KERNELS[kid].output_shape.
void  cg_profile_dump(FILE *fp, u32 top_n);

// === backend/ ===
// CPU backend -- only backend for step 12.  Installed by thvm_init.
// Metal lands in step 14 behind the same Backend struct.
extern Backend CPU_BACKEND;
extern Backend METAL_BACKEND;
// CUDA backend -- defined only in the Linux+CUDA build (THVM_HAS_CUDA).
// Plain C99 (driver API + nvrtc are C), so it lives in this single-TU
// build rather than a separate object like the Objective-C Metal one.
#ifdef THVM_HAS_CUDA
extern Backend CUDA_BACKEND;
#endif
#ifdef THVM_HAS_METAL
int thvm_metal_jit_replay_dispatch_ready(JitReplayDispatch const *op);
int thvm_metal_jit_replay_run(u32 slot, u32 start_op,
                              JitReplayDispatch const *ops, u32 n_ops);
// Read the process-wide Metal GPU-time accumulator.  *out_total_us =
// summed [cmd GPUEndTime]-[cmd GPUStartTime] microseconds across every
// command-buffer flush/submit; *out_flush_count = number of
// flushes/submits observed (incl. empty ones).  Either pointer may be
// NULL.  Used by the WL TMetalGpuTime[] surface for per-step GPU-time
// measurement.
void thvm_metal_gpu_time(u64 *out_total_us, u64 *out_flush_count);
#endif

fn void cpu_jit_cache_reset(void);

fn void backend_dispatch_begin_all(void);
fn void backend_dispatch_flush_all(void);
fn void backend_dispatch_end_all(void);
fn void kernel_fire_gen_bump(void);
fn void kernel_fire_scope_begin(void);
fn void kernel_fire_scope_end(void);
// Claim an UOP_ASSIGN cell (by heap loc) for firing this pass: returns
// 1 the first time the loc is seen in the current fire-gen, 0 on every
// re-visit (so a multiply-reachable ASSIGN writes its buffer once).
fn int assign_fire_claim(u64 loc);

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

// === cnf/ ===
// Collapsed normal form: reduces to WHNF then lifts the first SUP
// to the top, recursively.  Plain DPs are Levy-opaque under wnf
// (they sit as WHNF roots); cnf is the readback layer where their
// duplication actually fires.  See src/cnf/_.c.
fn Term cnf(Term term);
fn Term cnf_at(Term term, u32 depth);

// === eval/ ===
// Single-threaded SUP-tree enumeration on top of cnf.  Walks every
// branch FIFO ordered by INC priority and writes pure leaves into
// `out`.  Returns the count actually written, capped at `cap`.
fn u64 eval_collapse(Term term, Term *out, u64 cap);

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
// Invalidate thvm_kbo's persistent per-term weight memo.  Call when term
// cells move (GC) or the config changes (new run / new weights).
fn void   thvm_kbo_invalidate(void);
// Opt in to persistent KBO weight memoization (default off = fresh per
// call).  The caller then MUST invalidate on cell movement (the ATP does,
// on GC).  Keeps thvm_kbo sound for any direct caller that does not opt in.
fn void   thvm_kbo_set_persist(u8 on);
// Total KBO weight (Σ symbol-weight) of a single term, served from the
// per-term memo.  Same lifecycle as thvm_kbo's memo (invalidate on GC /
// config change).  Lets the CP-weight heuristics weigh terms in O(1) on a
// repeat instead of a fresh full traversal.
fn long long thvm_kbo_term_weight(const KboConfig *cfg, Term t);

// Flatterm KBO: same verdict as thvm_kbo, but flattens each operand into
// a contiguous pre-order node array so the comparison reads cache-dense
// sequential memory instead of chasing the IC term's pointer graph.  The
// ATP order-gate routes through this on the flatterm path (use_flatterm);
// falls back to thvm_kbo for terms deeper than its flat buffer or with a
// non-CTR/non-FVR head.  Byte-identical verdict (ATP_KBO_FLAT_SELFCHECK).
fn KboCmp thvm_kbo_flat(Term s, Term t, const KboConfig *cfg);

// Slice entrypoint: same decision as thvm_kbo_flat, but takes pre-
// encoded KboFlatNode arrays from the caller.  Lets the ATP hot path
// bypass the per-call kbo_flat_encode when a rule side or subject is
// already flat in the surrounding normalize context.  na/nb are
// accepted for caller-side assertions; the decision walks via each
// node's `sz` field so the totals are not consulted internally.
struct KboFlatNode;
fn KboCmp thvm_kbo_flat_slice(const struct KboFlatNode *a, u32 na,
                              const struct KboFlatNode *b, u32 nb,
                              const KboConfig *cfg);

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
// Invalidate thvm_lpo's persistent (s,t)->verdict memo.  Call when term
// cells move (GC) or the precedence changes (new run).
fn void   thvm_lpo_invalidate(void);
// Opt in to persistent LPO memoization (default off = fresh per call).
// The caller then MUST invalidate on cell movement (the ATP does, on GC).
fn void   thvm_lpo_set_persist(u8 on);

// Diagnostic counters for lpo_flat_rec / compute / memo (the AndAssoc
// faithful-port profile shows lpo_flat_rec_compute is 41% of CPU).
fn void thvm_lpo_flat_stats(u64 *rec_calls, u64 *memo_hits,
                            u64 *compute_calls, u64 *top_calls);
fn void thvm_lpo_flat_stats_reset(void);

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
// 7c: canonically renumber a stored equation/rule's variables to a
// dense [0, k) set shared across both sides (alpha-renaming).
fn void thvm_normalize_vars(Term *lhs, Term *rhs);

// === cp/ ===
// Critical-pair enumeration for an oriented rule set (stage 4).
// CriticalPair holds the two terms produced by overlapping rules
// at a non-variable position; both sides should be joinable for the
// system to be locally confluent.
//
// pos[0..pos_len) records the superposition geometry: the child-index
// path to the non-variable subterm of rule i's lhs that rule j's lhs
// unified with.  pos_len == 0 is a top (outermost) overlap.  This is
// the provenance a Waldmeister-PCL-shaped proof needs to present the
// CP as a CriticalPairLemma.
#define CP_MAX_DEPTH 16
typedef struct {
  Term lhs;
  Term rhs;
  Term peak;            // sigma(li): the overlapped term both CP sides descend from
  u8   pos[CP_MAX_DEPTH];
  u8   pos_len;
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

// Single-pair superposition core: overlap the (lj -> rj) face into
// (li -> ri), appending CPs from `count`.  Variables of (lj, rj) must
// be renamed apart from (li, ri).  See the definition for why the
// saturator drives both faces of an unorientable equation through it.
fn u32 thvm_critical_pairs_pair(Term li, Term ri, Term lj, Term rj,
                                CriticalPair *out, u32 cap, u32 count);

// Diagnostic counters for the CP enumeration caps:
//   *out_dropped       -- per-pair buffer cap-hits (ctx->count >= ctx->cap)
//   *out_depth_capped  -- positions stranded under a CTR at CP_MAX_DEPTH
// thvm_cp_caps_reset() zeros both before a fresh measurement.
fn void thvm_cp_caps_reset(void);
fn void thvm_cp_caps_get(u64 *out_dropped, u64 *out_depth_capped);

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
  ATP_ABORTED     = 5,   // host abort hook fired (e.g. WL Abort/TimeConstrained)
} AtpStatus;

// Initial heap capacities for the growable rule / CP arrays in
// AtpState.  The arrays double on demand (see atp_ensure_rule_cap /
// atp_ensure_cp_cap), so these are starting sizes, not ceilings --
// a long completion run grows them as far as host memory allows.
#define ATP_INIT_RULES 256
#define ATP_INIT_CPS   4096
// The completion trace records one TAG_CTR entry per input axiom,
// per oriented rule, and per critical pair that survives the
// joinable / subsumption filters.  A hard single-axiom completion
// (the Wolfram NAND axiom) generates tens of thousands of surviving
// CPs; the proof-DAG export (thvm_wl_atp_run_proof) needs the WHOLE
// trace so every rule cited in the extracted chain still has a live
// r_trace[] lineage back to its birthing TRACE_CP.  Sized to hold a
// DoubleNegation completion run with headroom; entries past the cap
// are dropped (atp_trace_push returns ATP_TRACE_NONE) and their
// rules lose their derivation history.
#define ATP_MAX_TRACE  131072

// Sentinel for "no rule excluded" in the connectedness check
// (atp_cp_source_disjoint_connected): any value >= n_rules works,
// but callers pass this explicitly.
#define ATP_RULE_NONE  0xFFFFFFFFu

// Reason labels for trace entries (used as the CTR label).
// Each TraceEntry is a TAG_CTR with label = reason and children =
// [NUM(parent_a), NUM(parent_b), lhs, rhs].  Parent index sentinel
// ATP_TRACE_NONE means "no parent" (e.g., for input axioms).
#define TRACE_AXIOM    1u   // initial input equation pushed via add_equation
#define TRACE_ORIENT   2u   // CP normalized + KBO-oriented into a rule
#define TRACE_CP       3u   // critical pair generated from two rules
// TRACE_SIMPLIFY: an older rule whose lhs simplified under newer
// rules was dropped by thvm_atp_interreduce and re-queued as the
// equation (reduced_lhs == old_rhs).  parent_a is the trace index
// of the dropped rule; the reduction is a forward-rewrite chain on
// the old lhs that a proof consumer replays under the final R.
// Recording this (instead of a fresh TRACE_AXIOM) keeps the proof
// DAG connected through interreduction -- mirrors Waldmeister PCL's
// `Reduktion` events.
#define TRACE_SIMPLIFY 4u
// TRACE_NORM_STEP: one rewrite step the C engine applied while
// normalizing a CP equation toward its oriented rule.  Pushed by
// thvm_atp_step when AtpState.record_norm_steps is set, so a proof
// consumer walks (TRACE_CP) -> (TRACE_NORM_STEP){0,1,...,N} ->
// (TRACE_ORIENT) linearly instead of re-deriving the chain by search.
// Children:
//   [NUM(parent_a), NUM(rule_idx), lhs (after step), rhs (after step),
//    NUM(pos_len), NUM(pos_0), ..., NUM(side), NUM(fwd)]
// rule_idx is the alive-rule index used; side is 0 for an lhs-of-
// equation rewrite, 1 for an rhs-of-equation rewrite; fwd is 1 for an
// lhs->rhs rule application, 0 for an unorientable rule fired
// reversed under ordered rewriting.
#define TRACE_NORM_STEP 5u
#define ATP_TRACE_NONE 0xFFFFFFFFu

// 8a: CTR labels for the IC-native CP-set graph (-DATP_CP_GRAPH).
// ATP_CP_LABEL labels a 2-child `Cp[lhs,rhs]` leaf; ATP_CPSET_LABEL
// labels the `CpSet[...]` container whose children are those leaves
// in cp_lhs[] / cp_rhs[] slot order.  Distinct from the TRACE_*
// labels so a leaf is never confused with a trace entry.
#define ATP_CP_LABEL    16u  // Cp[lhs, rhs]
#define ATP_CPSET_LABEL 17u  // CpSet[Cp, Cp, ...]

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

// CP-priority weight modes -- ports of Waldmeister's `ClasHeuristics`
// module ("classification heuristics"; sources/CLAS/ClasHeuristics.c).
// Each names the Waldmeister `CH_*Weight` function it mirrors.  The
// cheapest CP wins, so a lower number is selected first.
//
//   ADD    -- CH_AddWeight  : symbol_count(lhs) + symbol_count(rhs).
//             The bare symbol-count sum; reachable via
//             thvm_atp_set_cp_weight_mode for callers that want it.
//   MAX    -- CH_MaxWeight  : max(symbol_count(lhs), symbol_count(rhs)).
//   ORD    -- CH_OrdWeight  : KBO-weight sum (CF_Phi_KBO) -- the
//             ordering's own weight function rather than raw count.
//   GT     -- CH_GtWeight   : ordering-directed -- GT picks the
//             lhs size, LT the rhs size, otherwise the sum.  The
//             engine default (thvm_atp_init): it cuts the corpus
//             saturation step count vs ADD and proves the
//             distance-1 CriticalPairLemma cpl1 in one step.
//   MIX    -- CH_MixWeight  : (wl+wr)*g + g + (wl+wr), where g is
//             the GtWeight value -- a graded blend.
//   MIX2   -- CH_MixWeight2 : g*10 + (wl+wr).
//   UNIF   -- CH_Unifikationsmass ("unification measure") :
//             (wl+wr) * depth-weighted term-disagreement count.
typedef enum {
  ATP_CP_WEIGHT_ADD  = 0,
  ATP_CP_WEIGHT_MAX  = 1,
  ATP_CP_WEIGHT_ORD  = 2,
  ATP_CP_WEIGHT_GT   = 3,
  ATP_CP_WEIGHT_MIX  = 4,
  ATP_CP_WEIGHT_MIX2 = 5,
  ATP_CP_WEIGHT_UNIF = 6,
  ATP_CP_WEIGHT_GOAL = 7,   // Waldmeister CPinGoal: weight a CP by its
                            // structural match to the goal (Clas_CP_Goal.c)
  ATP_CP_WEIGHT_TWEE = 8,    // Twee KB-completion CP weighting (Smallbone
                             // 2020+):
                             //   lhsweight * size(larger side) +
                             //   rhsweight * size(smaller side) +
                             //   depthweight * depth
                             // with defaults lhs=4, rhs=1, depth=2, varweight
                             // 6/7.  Twee.CP.score (src/Twee/CP.hs:240).
                             // Asymmetric: biases toward CPs whose REDUCT
                             // is small regardless of how big the
                             // peak is.  Strong on Sheffer/nand-style
                             // single-operator saturations where Mix2 / Add
                             // wall.
  ATP_CP_WEIGHT_LEARNED = 9, // ENIGMA-style learned scorer: a trained
                            // logistic regression over CP features ranks
                            // CPs by predicted proof-relevance.
  ATP_CP_WEIGHT_CONJSYM = 10, // E-style conjecture-symbol weight (port
                              // of HEURISTICS/che_funweights.c::
                              // ConjectureSymbolWeightInit).  Walks both
                              // sides of the CP; nodes whose function
                              // symbol appears in the conjecture get
                              // weight 1 (conj_fweight), nodes whose
                              // symbol does NOT get weight 4
                              // (fweight=4*conj_fweight): a 4x discount
                              // biases CP selection toward critical
                              // pairs structurally similar to the goal,
                              // similar in spirit to ATP_CP_WEIGHT_GOAL
                              // but using a cheap symbol-set bitmask
                              // instead of a full structural match.
  ATP_CP_WEIGHT_DIVERSITY = 11, // E-style diversity weight (port of
                              // HEURISTICS/che_diversityweight.c::
                              // DiversityWeightCompute).  Walks both
                              // sides; combines the base symbol count
                              // with a linear penalty in #distinct
                              // CTR labels and #distinct FVR ids:
                              //   base + f_distinct + v_distinct
                              // (E's defaults are configurable; we
                              // use the linear fdiff1=1, fdiff2=0,
                              // vdiff1=1, vdiff2=0 shape).  Penalizes
                              // CPs whose two sides drag in many
                              // unrelated symbols / variables --
                              // surfaces structurally "compact" CPs
                              // first.  Distinct-set tracking uses
                              // u64 bitmasks (WALD_MAX_SYMBOLS=64).
  ATP_CP_WEIGHT_RELLEVEL = 12, // E-style relevance-level weight (port
                              // of HEURISTICS/che_funweights.c::
                              // RelevanceLevelWeight + init_relevance_-
                              // vector).  Two levels in our port:
                              //   level 0 = symbol appears in conjecture
                              //   level 1 = symbol appears in an axiom
                              //             that itself shares a symbol
                              //             with the conjecture
                              //   level 2 = remote symbol (otherwise)
                              // Per-node weight 1 / 2 / 4 (E's linear
                              // poly w_0=1, w_1=2, escalating to a 4x
                              // remote penalty matching ConjSym's off-
                              // sym multiplier).  Variable nodes weight
                              // 1.  Cached level masks recomputed when
                              // the goal is set.
  ATP_CP_WEIGHT_STAGGERED = 13, // E StaggeredWeight (HEURISTICS/
                              // che_varweights.c::StaggeredWeightCompute).
                              // base_weight / max(1, max_axiom_weight/2).
                              // Buckets CPs by integer stagger group so
                              // FifoTiebreak fairness (oldest-first
                              // within a bucket) dominates the heap
                              // selection.  Pair with FifoTiebreak->True
                              // for the intended behaviour.
  ATP_CP_WEIGHT_LAST = 14,
} AtpCpWeightMode;

typedef struct {
  // Rule set R: growable parallel arrays sized for
  // thvm_rewrite_normalize / thvm_critical_pairs to consume
  // directly.  r_trace[i] is the trace-entry index that produced
  // rule i (TRACE_ORIENT for rules added by atp_step;
  // ATP_TRACE_NONE for rules manually injected by tests / setup
  // code that bypassed the saturation pipeline).  Heap-allocated by
  // thvm_atp_init at ATP_INIT_RULES capacity; atp_ensure_rule_cap
  // doubles on demand so saturation never silently drops a rule.
  Term *lhs;
  Term *rhs;
  u32  *r_trace;
  u8   *r_orient;            // r_orient[i]=1 iff rule i is KBO-oriented (lhs>rhs)
  u32   n_rules;
  u32   r_cap;
  u32   n_unorient;          // count of unorientable rules currently in R

  // CP queue (open-form: not INC-wrapped here; the priority encoding
  // happens at selection time in thvm_atp_select).  cp_trace[i]
  // holds the trace-entry index that birthed cp[i] (TRACE_AXIOM
  // for queued axioms, TRACE_CP for generated CPs in 6.1c, or
  // ATP_TRACE_NONE if tracing is disabled / unavailable).  Growable
  // like the rule arrays: heap-allocated at ATP_INIT_CPS,
  // atp_ensure_cp_cap doubles on demand.
  //
  // Waldmeister Stringterms port: a queued CP is NOT held as a pair
  // of IC heap term-graphs but as one packed preorder byte string
  // (acp_pack / acp_unpack in src/atp/_.c) -- `cp_packed[i]` is a
  // malloc'd `u8 *` outside the managed heap, so the Cheney collector
  // never copies it.  This is the structural fix for the late-game GC
  // wall (the CP queue was a ~62M-cell live set re-copied every
  // collection).  cp_packed[i] is owned by the queue: allocated on
  // push, freed on pop / drop.
  u8  **cp_packed;
  u32  *cp_trace;
  u32   n_cps;
  // 7c': the CP queue is a binary min-heap keyed on
  // (cp_pri, cp_seq) -- cp_pri is atp_cp_priority computed once at
  // push, cp_seq is a monotonic insertion counter breaking ties.
  // Selection is O(log n) pop-min; this replaces the old
  // rebuild-an-INC-SUP-tree-every-step O(n) scan.
  u32  *cp_pri;
  u32  *cp_seq;
  // Goal-interleave: per-CP goal-directed weight (CPinGoal), parallel to
  // cp_pri.  Filled at push only when use_goal_interleave > 0; the
  // selection then takes a goal-min pick every use_goal_interleave-th
  // step (E-style ratio: size-based system-building + goal steering).
  u32  *cp_goal;
  // K-D Heap secondary dimension (port of WM `CPdimension` in
  // KPVerwaltung.c).  cp_pri2 is a SECOND priority computed at push
  // time using w2_mode (a different ATP_CP_WEIGHT_* mode than cp_pri's
  // primary).  Every w2_modulo-th selection picks min-cp_pri2 instead
  // of min-cp_pri -- WM's alternating-dimension fairness that surfaces
  // structurally simple rules buried under the primary-weight ordering.
  // Default w2_modulo=0 (disabled, byte-identical engine); set via
  // thvm_atp_set_w2 / env THVM_ATP_W2_MODULO / W2_MODE.
  u32  *cp_pri2;
  u32   w2_modulo;       // 0 = disabled
  u8    w2_mode;         // ATP_CP_WEIGHT_* used for cp_pri2 computation
  u32   n_cps_w2_picks;  // diagnostic counter
  u32   cp_seq_next;
  u32   cp_cap;
  // Waldmeister CP-queue interleaving: selection alternates between
  // the weight key (cp_pri) and the FIFO key (cp_seq, oldest first).
  // cp_select_count is the running selection counter that drives the
  // ratio -- see thvm_atp_select_cp / ATP_CP_FIFO_MODULO.
  u32   cp_select_count;

  // 8a: IC-native CP-set representation.  Behind -DATP_CP_GRAPH the
  // CP queue is also held as ONE shared Term: a CTR `CpSet[...]`
  // whose children are 2-child `Cp[lhs,rhs]` CTR leaves, one per
  // queued CP, in the same slot order as cp_lhs[] / cp_rhs[].
  // Because thvm hash-conses every cell, two CPs sharing a subterm
  // share its heap cells -- cp_graph is a maximally-shared DAG.
  // Every CP mutation rebuilds cp_graph from the (still-maintained)
  // arrays so it stays in lockstep; the arrays are the synced
  // mirror tests/test_atp.c reads directly.  Selection stays the
  // 7c' heap over cp_pri / cp_seq -- INC-priority is 8d.  Flag OFF
  // this field is absent and the engine is byte-for-byte the
  // milestone-7 array engine.
#ifdef ATP_CP_GRAPH
  Term cp_graph;
#endif

  // 7d: feature-vector subsumption index over the CP queue.  Behind
  // -DATP_FV_INDEX, `atp_cp_queue_subsumed` consults this instead of
  // the O(n_cps) thvm_match scan: each queued CP carries a vector of
  // cheap integer features where a more-general (subsuming) term is
  // componentwise <= the candidate's, so retrieval = "find stored
  // FVs dominated by the query FV" via an FV-trie, then thvm_match
  // runs only on the retrieved candidates.  The index stores plain
  // ints (feature values, CP seq ids) plus mirror Term pairs that
  // ride the GC alongside cp_lhs[]/cp_rhs[].  Maintained
  // incrementally: insert on CP enqueue, mark-dead on dequeue.  Flag
  // OFF this field is absent and the engine is the milestone-7 array
  // scan, byte-for-byte.  Independent of -DATP_CP_GRAPH.  Opaque
  // pointer so the struct layout does not leak the index internals.
#ifdef ATP_FV_INDEX
  struct AtpFvIndex *fv_index;
#endif

  // Milestone 10: MNF goal-directed search state (opaque pointer).
  // Behind -DATP_MNF; created lazily on the first goal_check, then
  // drives a bidirectional rewrite search from the conjecture instead
  // of the passive single-normal-form check.
#ifdef ATP_MNF
  struct AtpMnf *mnf;
#endif

  // Milestone 10: runtime gate for the MNF goal-directed search.  The
  // dylib is COMPILED with -DATP_MNF so MNF is linked in, but the front
  // search stays inert unless this flag is set -- goal_check runs the
  // single-normal-form check alone when 0 (default), preserving
  // completion-only behaviour and timing.  Set via
  // `thvm_atp_set_use_mnf`; the WL surface flips it for
  // `Method -> "GoalDirected"`.  The field is always present (not behind
  // -DATP_MNF) so callers can set it unconditionally; goal_check only
  // consults it inside the `#ifdef ATP_MNF` block.
  u8 use_mnf;

  // Waldmeister-faithful RHS interreduction (IR_InterreduktionRechts /
  // RMRechtsInterred, Interreduktion.c:329).  When set, after a new rule
  // is oriented thvm_atp_interreduce also normalizes the RIGHT-HAND side
  // of every other rule against the current rule set; a rule whose RHS
  // shrinks is dropped and re-queued as the simplified equation
  // (old_lhs, reduced_rhs) via TRACE_SIMPLIFY (the same connected-DAG
  // path the LHS-collapse already uses).  Keeps R interreduced (= the
  // canonical/reduced rewrite system Waldmeister maintains) so the CP
  // set stays small and the goal's normal form is actually reached.
  // 0 (default) leaves the engine's prior LHS-only interreduction
  // untouched, so test_atp / atp.wlt behaviour is unchanged; the WL
  // surface flips it for Method -> "Waldmeister".  Set via
  // thvm_atp_set_use_rhs_interreduce.
  u8 use_rhs_interreduce;

  // Unfailing-completion BOTH-FACES superposition.  The default CP
  // enumerator overlaps a rule's STORED lhs only -- complete for an
  // oriented rule (l > r for every instance) but INCOMPLETE for an
  // unorientable equation u = v: unfailing completion must superpose
  // BOTH faces (u into the peer AND v into the peer), since either side
  // can be the larger/contracted side for some ground instance.  Set,
  // the saturator additionally overlaps the rhs-face of every
  // unorientable equation (cp/_.c thvm_critical_pairs_pair).  This is
  // the divergence that left the deep Sheffer/Wolfram theorems
  // UNREACHABLE at any budget: their proofs superpose the big RHS of an
  // incomparable equation, a CP the lhs-only enumerator never emits.
  // 0 (default) preserves the prior CP set exactly (test_atp / atp.wlt
  // unchanged); the WL surface flips it for Method -> "Waldmeister".
  u8 use_unfailing_cp;

  // 7e (lever 2): discrimination tree over the rule LHS terms.  Behind
  // -DATP_RULE_INDEX, the ATP-side normalizer's per-position redex
  // search consults this instead of `rewrite_try_top`'s O(n_rules)
  // linear LHS scan: it retrieves, for a subject subterm, the LOWEST
  // rule index whose LHS one-way matches there -- exactly the rule the
  // linear scan's first-match would pick.  Built lazily over
  // `lhs[0..n_rules)`; `rule_index_dirty` is set on every rule-set
  // mutation (orient_and_add append, interreduce drop) so the next
  // indexed normalize rebuilds.  Opaque pointer; flag OFF this field
  // is absent and the engine uses the linear scan, byte-for-byte.
#ifdef ATP_RULE_INDEX
  struct AtpRuleIndex *rule_index;
  u8                   rule_index_dirty;
  // Companion redex index over the UNORIENTABLE equations' faces (both
  // l->r and r->l, each indexed only when its replacement's vars are
  // contained in its matched face -- a face that cannot fire is not
  // indexed).  The flatterm unorientable pass (atp_ft_unorient_step)
  // descends this instead of the O(n_rules) linear scan, applying the
  // LPO order-gate to candidate faces only.  Rebuilt with rule_index
  // whenever R mutates (shares rule_index_dirty / n_rules_built).
  struct AtpRuleIndex *unorient_index;
  // Opt-in flatterm fast-path for the MIXED (orientable + unorientable)
  // normalize loop.  OFF by default: the engine is byte-identical to the
  // tree mixed loop.  When set, atp_rewrite_normalize_ordered's mixed
  // branch keeps the subject in the shared flat arrays across BOTH the
  // orientable indexed fixpoint and the unorientable pass, splicing every
  // rewrite in place (no per-step re-flatten / tree rebuild).  Set from
  // THVM_ATP_FLATTERM=1 at init, or via thvm_atp_set_use_flatterm.  Same
  // normal forms as the tree path (asserted by ATP_FLATTERM_DIFF).
  u8                   use_flatterm;
  // Opt-in faithful Waldmeister-FPA normalize path (src/wmfpa/wmfpa.h:
  // flatterm rep + DSBaum discrimination tree + NormalformInnermost).
  // OFF by default (THVM_ATP_WMFPA=1 to enable); when set, the orientable
  // normalize fixpoint encodes the subject to a flatterm once, retrieves
  // redexes by descending a discrimination tree built from s->lhs[], and
  // decodes back -- byte-identical normal form to the IC path (asserted
  // by the bench differential and the in-engine self-check).
  u8                   use_wmfpa;
  // Cached WM-FPA discrimination tree (opaque; void* to keep wmfpa.h out
  // of the public header) and the R size it reflects.  Rebuilt lazily
  // when n_rules changes, like rule_index.  Only touched on the gated
  // path (use_wmfpa); NULL otherwise.
  void                *wmfpa_tree;
  u32                  wmfpa_built;
  // Private staleness bit for the WM-FPA tree, set at the same sites that
  // set rule_index_dirty (an in-place rule edit that need not change
  // n_rules).  A private bit avoids fighting the indexed path over the
  // shared rule_index_dirty clear.
  u8                   wmfpa_dirty;
  // Gated correctness probe: when set (THVM_ATP_WMFPA_CHECK!=0) every
  // gated normalize first asserts the incrementally-maintained tree returns
  // the same lowest-index rule at every subject position as a tree freshly
  // rebuilt from the current rule set, aborting on the first divergence.
  u8                   wmfpa_check;
  // Incremental-resume watermark for the flatterm unorientable preorder
  // scan (atp_ft_unorient_step).  ON by default: the scan resumes past the
  // prefix proven clean since the last scan instead of restarting from
  // p=0, mirroring the orientable side's `clean_before` resume.  Cleared
  // (THVM_ATP_UNORIENT_RESUME=0) only for the resume-ON==OFF differential.
  u8                   ft_unorient_resume;
  // Opt-in CP-generation overlap-partner index.  OFF by default: the
  // engine scans all n_rules per new rule (O(n_rules) overlap attempts).
  // When set, thvm_atp_generate_cps_c queries cp_index -- a unification
  // discrimination tree over rule-LHS terms -- for the candidate partners
  // whose LHS could unify with a subterm of the new rule's LHS, running
  // the exact atp_overlap_ij only on those candidates.  The CP SET is
  // identical (the index returns a superset of true partners; the exact
  // unify in cp_visit still gates emission).  Set from THVM_ATP_CP_INDEX=1
  // at init, or via thvm_atp_set_use_cp_index.
  struct AtpRuleIndex *cp_index;
  // Companion to cp_index for the (old i x new j) overlap direction: a
  // discrimination tree over every NON-VAR SUBTERM of every rule LHS,
  // keyed by rule.  Querying it with the new rule's whole LHS lj returns
  // the old rules i whose li has a subterm unifiable with lj -- the dual
  // of cp_index's (new i x all j) whole-LHS query.
  struct AtpRuleIndex *cp_subindex;
  u8                   use_cp_index;
#endif

  // Transient: set by thvm_atp_select_cp to the trace-entry index
  // of the popped CP; consumed by thvm_atp_step right after the
  // pop so orient_and_add's TRACE_ORIENT entry can record the
  // source CP as its parent.
  u32  last_popped_trace;

  // Goal: a single conjecture goal_lhs == goal_rhs.  goal_lhs == 0
  // means "no goal set; run as completion".
  Term goal_lhs;
  Term goal_rhs;
  // Live goal normalized under the current R, refreshed by goal_check.
  // The goal-directed CP heuristic (CPinGoal) matches against this
  // shrinking form, not the original goal, so direction tracks progress
  // as Waldmeister's does.  0 until the first goal_check.
  Term goal_lhs_nf;
  Term goal_rhs_nf;

  // Bitset of TAG_CTR labels appearing in the conjecture (goal_lhs +
  // goal_rhs).  Recomputed by thvm_atp_set_goal.  Read by the
  // ATP_CP_WEIGHT_CONJSYM weight mode -- nodes whose symbol bit is set
  // are weighted 1, nodes whose bit is unset are weighted 4.
  // WALD_MAX_SYMBOLS == 64 so a u64 covers the whole label space.
  u64 conj_sym_mask;
  // Bitset of TAG_CTR labels at relevance level 1: a symbol appears
  // in some axiom that itself shares a symbol with the conjecture.
  // Distinct from conj_sym_mask (level 0).  Read by the
  // ATP_CP_WEIGHT_RELLEVEL weight mode.  Recomputed by
  // thvm_atp_set_goal alongside conj_sym_mask.
  u64 rel_lvl1_mask;
  // Per-symbol relevance LEVEL: 0 = appears in conjecture; k = appears
  // first at BFS depth k from the conjecture through the
  // "co-occurs-in-an-axiom" relation; 255 (ATP_REL_LEVEL_REMOTE) =
  // does not appear in any conjecture-reachable axiom.  Recomputed by
  // thvm_atp_set_goal.  Used by ATP_CP_WEIGHT_RELLEVEL whose per-node
  // weight is 1 + level (capped).  Caps at ATP_REL_LEVEL_MAX = 8 so
  // levels >8 collapse to the maximum penalty.  Array sized to
  // WALD_MAX_SYMBOLS == 64 (inlined; the macro is defined later in
  // this header so use the literal here for the AtpState declaration).
  u8 sym_level[64];

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
  // PCL-shaped serialization.  Heap-allocated and grown on demand
  // (atp_trace_ensure) up to t_max entries; a fixed embedded array
  // would both bloat sizeof(AtpState) and hard-cap the proof DAG at a
  // depth a 1601-rule completion exceeds.  t_max defaults to
  // ATP_MAX_TRACE (env THVM_ATP_TRACE_MAX raises it); entries past
  // t_max are dropped (atp_trace_push returns ATP_TRACE_NONE).
  Term *trace;
  u32   n_trace;
  u32   t_cap;
  u32   t_max;

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

  // ATP_CP_CLASSIFY: count of CPs dropped by the Waldmeister-style
  // critical-pair classifier (ported from `NewClassification` /
  // `ClasFunctions`, "new classification" / "classification
  // functions").  A CP is dropped when its killer-predicate
  // classification carries an `Act_never`-equivalent action --
  // the default config does this only for killer CPs that are
  // also rule-subsumed, a sound subset of the joinable drops.
  // Always 0 when the engine is built without -DATP_CP_CLASSIFY.
  u32  n_cps_dropped_classified;

  // Count of rules whose RHS was right-reduced (composition) in place
  // by thvm_atp_interreduce.  Diagnostic for the DISCOUNT-loop
  // right-reduction lever.
  u32  n_right_reduced;

  // Ground-joinability redundancy criterion (Martin-Nipkow / Twee CADE
  // 2021 sec 3.1; AHL 2003).  Ticked when a CP is provably ground-
  // joinable under EVERY total preorder of its variables (ordered set
  // partitions, ties included).  When `use_ground_join` is set the CP
  // is dropped (sound: ground-joinable CPs are redundant); otherwise it
  // is a counter-only measurement.  Only computed when the build defines
  // ATP_CP_GROUND_JOIN (the shipped paclet does; default C build off).
  u32  n_cps_ground_joinable;
  // Runtime gate for ground-joinability DELETION.  0 (default) = the
  // criterion only counts; 1 = drop ground-joinable CPs.  Set via
  // thvm_atp_set_use_ground_join (Method -> {... "GroundJoin" -> True}).
  u8   use_ground_join;

  // Runtime gate for Bachmair-Dershowitz connectedness DELETION (Twee
  // section 6.2).  0 (default) = off; 1 = drop a CP whose two sides
  // join through terms STRICTLY BELOW the peak sigma(li) under the
  // reduction order.  Sound: such a CP is a redundant consequence of
  // smaller overlaps, so removing it preserves the completion's
  // confluence target.  Set via thvm_atp_set_use_connectedness
  // (Method -> {... "Connectedness" -> True}).
  u8   use_connectedness;
  // Count of CPs the connectedness criterion deleted (measurement; ticks
  // only when use_connectedness drops a CP).
  u32  n_cps_dropped_connected_below_peak;

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
  //
  // Retained for backward compatibility: when `use_mix_heuristic`
  // is set, `atp_cp_priority` applies the +penalty regardless of
  // `cp_weight_mode`.  New code should prefer `cp_weight_mode`
  // (mode ATP_CP_WEIGHT_MIX subsumes this flag).
  u8   use_mix_heuristic;

  // CP-priority weight mode -- a port of the weight functions in
  // Waldmeister's `ClasHeuristics` module ("classification
  // heuristics", sources/CLAS/ClasHeuristics.c).  See the
  // `AtpCpWeightMode` enum for the per-mode formula.
  // `thvm_atp_init` sets this to ATP_CP_WEIGHT_GT, the
  // ordering-directed heuristic; ATP_CP_WEIGHT_ADD (the bare
  // symbol-count sum) stays reachable via
  // `thvm_atp_set_cp_weight_mode`.
  u8   cp_weight_mode;
  // 0 = off; N>0 = every N-th CP selection is a goal-directed (min
  // cp_goal) pick instead of the weight pick.  Pairs with max_cp_weight.
  u32  use_goal_interleave;
  // Waldmeister CPdimension fairness ratio (YFiles `Schrittweiten`):
  // 1 FIFO (oldest-CP) pick per `fifo_modulo` selections, the rest by
  // weight.  Prevents the smallest-weight heap from starving an older
  // heavy CP.  Default 11 (1:10, the most-fair Waldmeister setting);
  // Waldmeister also uses 50/100/200.  0 is treated as the default.
  u32  fifo_modulo;
  // Vampire-style random CP-selection seed + ratio.  Default 0 (off,
  // engine byte-identical).  When `random_modulo > 0`, every nth CP
  // selection picks a uniformly-random queued CP instead of the heap
  // root, driven by a deterministic xorshift64 stream seeded via
  // `thvm_atp_set_random_seed`.  Mirrors Vampire's `lrs+10_<n>` portfolio
  // entries whose distinctive ingredient on McCune is randomised clause
  // selection: deterministic-but-reseedable so the run is reproducible
  // while the trajectory differs from the strict heap-min path.
  u32  random_modulo;
  u64  rng_state;
  // Waldmeister `-:w1=fifo` secondary CP key.  The heap already breaks
  // equal-weight ties by cp_seq (insertion age), but the post-orient
  // CP-normalize sweep (atp_normalize_graph) reheapifies and reassigns
  // every cp_seq in heap-array order, scrambling the true insertion age.
  // When set, the sweep PRESERVES each surviving CP's original cp_seq, so
  // equal-weight ties resolve oldest-first across the whole run -- the
  // stable FIFO secondary sort key Waldmeister's selection uses.  Default
  // 0: reheapify reassigns cp_seq as before, engine byte-identical.
  u8   cp_fifo_tiebreak;
  // Waldmeister MaxWeight: discard a critical pair whose combined term
  // weight exceeds this (0 = unbounded).  Bounds the search on
  // self-overlapping axioms (the single Wolfram NAND axiom) so the
  // goal-directed selector is not starved by runaway pairs.
  u32  max_cp_weight;

  // Automatic, COMPLETENESS-PRESERVING growing CP-weight bound
  // (Waldmeister MaxWeight, but never permanently lossy).  When
  // `auto_max_cp_weight_base` > 0, atp_cp_heap_push compares a CP's
  // node count against the LIVE bound; over-bound CPs are NOT dropped
  // but parked on the `cp_stash_*` overflow list.  When the active
  // queue empties (or after a growth tick) the stash is drained back
  // in under the raised bound, so every CP is eventually selected and
  // no proof is lost.  0 = disabled (the default unbounded engine).
  u32  auto_max_cp_weight_base;     // bound = base + slope*max_rule_weight
  u32  auto_max_cp_weight_slope;    // multiplier on the deepest rule LHS
  u32  auto_max_cp_weight_cur;      // the live bound (recomputed on growth)
  u32  max_rule_lhs_weight;         // deepest rule LHS symbol count seen
                                    // (monotone; feeds the auto bound so it
                                    // need not rescan R on every CP push)
  // Overflow stash for CPs deferred by the growing bound.  Parallel to
  // the cp_* arrays but unordered (a plain growable list): a packed
  // byte string + trace id + node count.  Drained into the heap when
  // the bound grows past their weight.
  u8 **cp_stash_packed;
  u32 *cp_stash_trace;
  u32 *cp_stash_nodes;
  u32  n_cp_stash;
  u32  cp_stash_cap;

  // When set (via thvm_atp_set_record_norm_steps), thvm_atp_step
  // pushes a TRACE_NORM_STEP per rewrite the CP-normalize loop
  // applies, so a proof consumer can walk the chain CP ->
  // NORM_STEP* -> ORIENT linearly instead of reconstructing it by
  // search.  Off by default: per-step trace recording inflates the
  // trace by up to 2 * NORM_CAP entries per CP, which only matters
  // for runs whose caller will extract a ProofObject.
  u8   record_norm_steps;

  // Right-reduction (composition) toggle for interreduction.  When
  // set (default), thvm_atp_interreduce also rewrites a surviving
  // rule's RHS to its normal form under the just-added rules
  // (l -> r becomes l -> r' where r ->* r'), keeping RHSs canonical
  // so the critical pairs born from them stay small.  This is the
  // DISCOUNT-loop right-reduction step.  Defaults ON; can be
  // disabled via thvm_atp_set_right_reduce for A/B measurement or
  // if a proof-extraction regression is found.
  u8   right_reduce;

  // Periodic critical-pair-set interreduction (Waldmeister
  // KPV_KPMengeInterreduzieren / AP_generic, KPVerwaltung.c:1032).
  // When set, every `cp_set_ir_period`-th rule addition walks the whole
  // CP queue and, per queued CP: re-normalizes both sides against the
  // current rule set, DELETES it if it became joinable, and recomputes
  // its priority (reweight) so the heap order tracks the growing system.
  // This is what keeps Waldmeister's queue from drowning in CPs a later
  // rule would collapse.  Default OFF (cp_set_interreduce == 0): the
  // engine keeps its lazy pop-time normalization, so the default
  // trajectory is byte-identical.  Flipped on by Method->"Waldmeister"
  // via thvm_atp_set_cp_set_interreduce.
  u8   cp_set_interreduce;
  u32  cp_set_ir_period;          // 0 -> default period at run time
  u32  n_cp_set_ir_passes;        // diagnostics: passes run
  u32  n_cp_set_ir_deleted;       // diagnostics: CPs deleted (joinable)
  u32  n_cp_set_ir_reweighted;    // diagnostics: CPs reweighted

  // Lazy orphan murder (Waldmeister "Waisenmord", KPVerwaltung.c:535
  // selectNonOrphan + the per-rule `lebtNoch` liveness bit).  When a
  // rule is interreduced away, the queued CPs it birthed are redundant:
  // the simplified equation that replaces the rule regenerates whatever
  // those CPs would contribute.  WM does NOT sweep the queue on the drop;
  // it marks the dead parent rule and discards descendant CPs FOR FREE at
  // selection time -- the heap-min CP is skipped if either parent is dead.
  // `r_trace_dead` is a bitset keyed on a rule's birthing trace id (the
  // value stored in r_trace[]/cp parents); set when a rule is dropped,
  // tested at pop.  Default OFF (use_orphan_murder == 0) -> the engine is
  // byte-identical.  On for Method->"Waldmeister" via
  // thvm_atp_set_use_orphan_murder.
  u8   use_orphan_murder;
  u8  *r_trace_dead;              // bitset over trace ids (8 ids / byte)
  u32  r_trace_dead_cap;          // capacity in bits (== trace ids)
  u32  n_cps_dropped_orphan;      // diagnostics: orphan CPs skipped at pop

  // Permutation-subsumption (port of WM `GZ_ACVerzichtbar` in
  // INF/Grundzusammenfuehrung.c:137):  drop a CP whose two sides are
  // equal as multisets at the top.  Catches `nand(x,y) = nand(y,x)`
  // and prevents the cascade of commutativity-derived rules that
  // dominates the AndAssoc faithful-port trajectory (RULE 16
  // commutativity -> 68% unorientable rule set vs wmcli's 6%).
  u8   use_perm_subsume;
  u32  n_cps_dropped_perm_subsumed;

  // WM dokgS (KPVerwaltung.c:451 KPBehandelt): push-time rule-subsumption
  // drop.  Cheaper than full trivial-join normalize for CPs that are
  // directly subsumed by an existing rule's pattern.  Sound: a rule-
  // subsumed CP is joinable in one step via that rule's instance.
  u8   use_rule_subsume_drop;

  // Limited Resource Strategy (Riazanov & Voronkov, JSC 36, 2003).  When
  // a wall-clock budget is set, LRS estimates from the observed selection
  // rate how many MORE CPs the saturator will pop before the deadline,
  // then prunes the queue of CPs heavier than the predicted-reachable
  // weight -- the saturator only ever reaches the lightest
  // `predicted_remaining_selections` CPs, so the heavy tail wastes index
  // / normalize / heap effort it never spends.  Soundness: discarded CPs
  // are unreachable in budget, so the proof (if found) does not depend on
  // them -- the same "incomplete in principle, complete in budget"
  // tradeoff Vampire ships.  Default OFF: every field stays 0 and
  // thvm_atp_select_cp is byte-identical to the milestone-9 path.
  // Flipped on by Method -> {... "LRS" -> True} via
  // thvm_atp_set_use_lrs, or by setting THVM_ATP_LRS=1 at init.
  u8   use_lrs;
  u64  lrs_start_us;              // run start (atp_now_us at first select)
  u32  lrs_recompute_period;      // selections between horizon recomputes
  u32  lrs_warmup_selections;     // selections before the first horizon
  u32  lrs_last_recompute_at;     // cp_select_count at the last recompute
  u32  lrs_horizon;               // current weight cutoff (0 == no cutoff)
  u32  n_cps_dropped_lrs;         // diagnostics: CPs pruned by horizon
  u32  n_lrs_recomputes;          // diagnostics: horizon recomputations

  // Indexed unorientable-rewrite pass.  When set, the default tree mixed
  // normalizer replaces its O(n_rules) linear KBO-gated unorientable step
  // (atp_ordered_rewrite_step with the skip-oriented flag) with a
  // discrimination-tree retrieval over both faces of every unorientable
  // equation (atp_unorient_step_indexed -> atp_ft_unorient_step).  The
  // redex chosen is byte-identical (same outermost-leftmost preorder,
  // (rule asc, l->r then r->l) priority, KBO gate), so the normal form
  // and the whole saturation trajectory are unchanged.  Default OFF
  // (engine byte-identical).  On for Method->"Waldmeister" via
  // thvm_atp_set_use_unorient_index.
  u8   use_unorient_index;

  // Deferred-selection / lazy normalization (Waldmeister/DISCOUNT given-
  // clause flow).  Default completion EAGERLY normalizes every generated
  // critical pair against the full rule set R at push time
  // (atp_cp_trivially_joinable in atp_push_cps_traced) to discard the
  // ~74% trivially-joinable before they enter the queue -- so the
  // normalize work scales with GENERATIONS (~430k on andassoc).
  // Waldmeister does NOT (KPVerwaltung.c: KPEinfuegen just inserts the
  // raw overlap into the K-D heap; Hauptkomponenten.c HK_Vervollstaendigung
  // = "completion": pop one CP via KPMinimum/selectNonOrphan, THEN reduce
  // it (ZeileNormalisieren) and orient/discard).  When set, the push path
  // skips the full-R normalize and queues the var-normalized RAW overlap
  // with a cheap size weight; the existing select-time normalize in
  // thvm_atp_step (the `kbo_eq(l, r)` join check that ALREADY runs) does
  // the work -- so normalize cost scales with SELECTIONS, not generations.
  // Soundness is unchanged: a CP is discarded ONLY when its sides reduce
  // equal at selection (genuine join); no non-joinable CP is dropped.
  // Default OFF (engine byte-identical: eager push-time normalize). On for
  // Method->"Waldmeister" via thvm_atp_set_use_lazy_normalize.
  u8   use_lazy_normalize;
  u64  n_cps_push_normalized;     // diagnostics: full-R normalizes at push

  // Set-of-Support (SOS) heuristic: bias CP priority toward CPs whose
  // terms share symbols with the goal.  Sound (completeness preserved
  // -- no CP is dropped; only the heap key is reduced for goal-touching
  // CPs so they surface earlier).  Wired to Method "SetOfSupport".
  // Default OFF.
  u8   use_sos;
  u32  goal_sym_mask[8];          // bit-set of goal symbols (CTR ext labels)
                                  // -- 8 * 32 = 256 distinct labels indexable;
                                  // cap with mod for larger.

  // Forward subsumption pruning (analog of Vampire's --forward_subsumption
  // flag, restricted to unit equations -- which all our equations are).
  // When adding a rule l'=r' to R, atp_push_rule scans existing rules
  // and drops the add if any existing rule l=r subsumes the new one:
  // \E sigma s.t. l*sigma = l' AND r*sigma = r' (or the cross-orientation
  // l*sigma = r' AND r*sigma = l').  Sound + completeness-preserving:
  // the new rule is logically implied by the existing one, so dropping
  // it adds zero deductive power.
  // Default OFF (engine byte-identical).  Wired to Method
  // "ForwardSubsume" -> True via thvm_atp_set_use_fwd_subsume.
  u8   use_fwd_subsume;
  u64  n_rules_fwd_subsumed;      // diagnostic counter

  // Backward subsumption pruning (Vampire `bs=unit_only` analog).  After
  // atp_push_rule successfully stores rule N at position N, optionally
  // scan rules 0..N-1; for each existing rule subsumed by N, soft-delete
  // it by overwriting lhs[i] / rhs[i] with an out-of-range FVR sentinel
  // (TAG_FVR with id == 255 >= REWRITE_MAX_VAR == 64).  thvm_match and
  // thvm_unify both return 0 on a sentinel input, so every rewrite /
  // CP-generation site naturally skips dead rules without per-site
  // r_dead[] checks.  r_dead[i] = 1 marks the slot, and the original
  // (lhs, rhs) is preserved in r_dead_lhs_save[i] / r_dead_rhs_save[i]
  // for proof reconstruction (the trace cites rule indices that may have
  // since been killed -- the proof builder reads from the save slot when
  // r_dead[i]).
  // Default OFF.  Wired to Method "BackwardSubsume" -> True via
  // thvm_atp_set_use_bwd_subsume.
  u8   use_bwd_subsume;
  u8  *r_dead;                    // 1 iff slot soft-deleted
  Term *r_dead_lhs_save;          // original lhs at time of death
  Term *r_dead_rhs_save;          // original rhs at time of death
  u64  n_rules_bwd_subsumed;      // diagnostic counter

  // Backward demodulation (Vampire `bd=all` analog, LHS half -- the RHS
  // half is the existing use_rhs_interreduce option).  After each newly-
  // added rule batch, also try to normalize each older rule's LHS with
  // the new rule(s).  When the LHS reduces, drop the rule and re-queue
  // the simplified equation (reduced_lhs, old_rhs) -- atp_add_equation_-
  // simplified will re-orient it under the now-updated R.
  // Default OFF.  Wired to Method "BackwardDemod" -> True via
  // thvm_atp_set_use_bwd_demod.
  u8   use_bwd_demod;
  u64  n_rules_bwd_demodulated;   // diagnostic counter

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

  // Optional wall-clock deadline (microseconds since the Unix
  // epoch as monotonic-from-CLOCK_MONOTONIC isn't portable
  // through `time.h` alone, so we use clock_gettime CLOCK_REALTIME
  // returned us-since-epoch).  0 means no deadline (default).
  // When non-zero, `thvm_atp_step` returns `ATP_TIMEOUT` once the
  // current time crosses this value -- lets the saturator bail
  // on recursively-defined axioms (Y combinator's `Y x == x (Y x)`
  // generates infinite CP fan-out and would otherwise spin until
  // the OS sends SIGKILL).
  u64  wall_deadline_us;

  // === ENIGMA-style learned CP-selector training data ===============
  // When `record_cp_features` is set (via thvm_atp_set_record_cp_
  // features; DEFAULT 0), thvm_atp_select_cp appends one row per
  // PROCESSED critical pair to the growable arrays below: a fixed-
  // length feature vector (ATP_CP_FEATURE_DIM f32s) plus the CP's
  // trace-entry index (cp_feat_trace[]).  After a SUCCESSFUL proof
  // the trainer calls thvm_atp_cp_label (which walks the trace DAG
  // back from the proof's rules) to fill cp_feat_label[i] with 1 if
  // that selected CP is an ancestor of the goal-closing step, else 0.
  // OFF (the default) every field stays NULL/0 and select_cp does no
  // extra work -- the engine is byte-identical to the untracked run.
  u8     record_cp_features;
  float *cp_feat_rows;     // n_cp_feat * ATP_CP_FEATURE_DIM, row-major
  u32   *cp_feat_trace;    // trace id of the i-th recorded selected CP
  u8    *cp_feat_label;    // 0/1 proof-relevance label (filled post-hoc)
  u32    n_cp_feat;        // number of recorded rows
  u32    cp_feat_cap;      // capacity of the three parallel arrays

#ifdef THVM_ATPFT_RULES
  // Parallel AtpFt rule storage; see docs/atp/engineering.md.
  // Eagerly mirror every Term written into lhs[]/rhs[]/r_dead_*_save[]/
  // goal_* into AtpFtCell* slots that share the slot lifetime exactly
  // (allocate-with-grow, write-with-set, shift-with-compact).  The Term
  // path stays authoritative at Stage 4 -- every reader (rule_index,
  // CP gen, rewriter) keeps consuming Terms; the AtpFt mirror is
  // verification-only (THVM_ATPFT_VERIFY=1 cross-checks ft_eq + ft_hash
  // parity after each rule add).  Stage 5+ flips individual readers
  // (rule_index_ft, rewrite_normalize_ft, ...) onto the mirror.
  // AtpFt cells are address-stable across thvm_atp_gc_collect (the Term
  // collector moves Term cells; AtpFt has its own slab pool that the
  // collector never touches), so no GC fixup of these pointers is
  // needed -- the post-GC invariant ft_eq(lhs_ft[i], lhs[i]) holds by
  // construction.  ft_arena owns every cell here; thvm_atp_free
  // releases the whole arena in one ft_destroy.
  struct AtpFt    *ft_arena_ptr;     // points at ft_arena_storage below
                                     // (boxed pointer so the struct
                                     // forward-decl is enough in this
                                     // header; full AtpFt comes in via
                                     // ft.h at the .c boundary).
  struct AtpFtCell **lhs_ft;
  struct AtpFtCell **rhs_ft;
  struct AtpFtCell **r_dead_lhs_save_ft;
  struct AtpFtCell **r_dead_rhs_save_ft;
  struct AtpFtCell  *goal_lhs_ft;
  struct AtpFtCell  *goal_rhs_ft;
#endif

#ifdef THVM_ATPFT_CPQ
  // Parallel native AtpFt CP queue; see docs/atp/engineering.md.
  // Each populated slot owns its two FT spans in Arena A; the legacy
  // cp_packed[] byte queue stays populated in parallel (FV index +
  // cp_graph mirror + peek/stash consumers).  Capacity tracks
  // s->cp_cap (atp_ensure_cp_cap grows both arrays).  See
  // src/atp/ft_cpq.c for the entry layout and lifetime rules.
  //
  // Sized to cp_cap; populated slots are [0..n_cps).  Element type
  // declared as void* in this header to keep the public struct free
  // of the (in-TU) AtpCpEntry definition; the .c side casts back.
  void *cp_packed_ft;
#endif
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

// Milestone 10: enable/disable the MNF goal-directed front search at
// runtime.  No-op unless the dylib was compiled with -DATP_MNF.  When
// 1, goal_check augments the single-normal-form check with the
// bidirectional MNF collision search; when 0 (default) the single-NF
// check runs alone.  Lets one dylib carry MNF without paying for it
// on completion-only goals.
fn void      thvm_atp_set_use_mnf (AtpState *s, u8 on);
// Opt in to the flatterm fast-path for the mixed normalize loop (no
// effect unless the dylib is built with ATP_RULE_INDEX).  Same normal
// forms as the default tree mixed loop; a per-step speedup on rule sets
// with unorientable equations.  Off by default.
fn void      thvm_atp_set_use_flatterm(AtpState *s, u8 on);
// Opt in to the CP-generation overlap-partner index (no effect unless
// built with ATP_RULE_INDEX).  Same CP set as the unindexed n_rules scan
// (the index returns a superset of partners; the exact unify gates
// emission).  A per-step speedup as R deepens.  Off by default.
fn void      thvm_atp_set_use_cp_index(AtpState *s, u8 on);
// Opt in to ground-joinability CP deletion (no effect unless the dylib
// is built with ATP_CP_GROUND_JOIN).  Sound: ground-joinable CPs are
// redundant.  Off by default (the criterion only counts).
fn void      thvm_atp_set_use_ground_join(AtpState *s, u8 on);
fn void      thvm_atp_set_use_connectedness(AtpState *s, u8 on);
// Waldmeister CPdimension fairness ratio: 1 FIFO (oldest) pick per
// `modulo` CP selections.  0 = default (11).  Larger = more weight-
// greedy; Waldmeister uses 11/50/100/200 per problem analysis.
fn void      thvm_atp_set_selection_ratio(AtpState *s, u32 modulo);
// Vampire-style random CP-selection mode.  `modulo == 0` disables (engine
// byte-identical); `modulo > 0` makes every nth selection pick a
// uniformly-random queued CP from a deterministic xorshift64 stream.
fn void      thvm_atp_set_random_modulo(AtpState *s, u32 modulo);
fn void      thvm_atp_set_random_seed  (AtpState *s, u64 seed);
// Waldmeister `-:w1=fifo` secondary key: preserve each surviving CP's
// original insertion age (cp_seq) across the post-orient normalize
// sweep, so equal-weight ties resolve oldest-first run-wide.  0 = off
// (reheapify reassigns cp_seq; engine byte-identical).
fn void      thvm_atp_set_cp_fifo_tiebreak(AtpState *s, u8 on);

// Select the CP-priority weight mode (an `AtpCpWeightMode` value).
// `thvm_atp_init` defaults to ATP_CP_WEIGHT_GT; out-of-range
// values are clamped to ATP_CP_WEIGHT_ADD.  See `AtpCpWeightMode`
// for the per-mode formula, all ports of Waldmeister's
// `ClasHeuristics` module.
fn void      thvm_atp_set_cp_weight_mode(AtpState *s, u32 mode);
fn void      thvm_atp_set_max_cp_weight(AtpState *s, u32 w);
// Enable the automatic, completeness-preserving growing CP-weight
// bound (Waldmeister MaxWeight, but the deferred CPs are stashed and
// re-admitted, never dropped).  `base` seeds the bound (bound = base +
// 2 * deepest-current-rule-LHS-weight); base == 0 disables it (the
// default unbounded engine).  On the deep Sheffer benchmarks this
// shrinks the peak CP queue ~3-4x and cuts the step count to a goal.
fn void      thvm_atp_set_auto_max_cp_weight(AtpState *s, u32 base);
fn void      thvm_atp_set_goal_interleave(AtpState *s, u32 ratio);
fn void      thvm_atp_set_record_norm_steps(AtpState *s, u8 on);
fn void      thvm_atp_set_right_reduce(AtpState *s, u8 on);
fn void      thvm_atp_set_cp_set_interreduce(AtpState *s, u8 on);
fn void      thvm_atp_set_use_orphan_murder(AtpState *s, u8 on);
fn void      thvm_atp_set_use_perm_subsume(AtpState *s, u8 on);
fn void      thvm_atp_set_perm_subsume_mask(u64 mask);

#ifdef THVM_ATP_AC
// AC reasoning controls.  See src/atp/ac.c.  `set_ac_mask` registers
// a u64 bit-mask of CTR labels that are associative + commutative;
// `auto_ac` derives the mask by analyzing a caller-supplied axiom
// set (parallel `lhs[]` / `rhs[]` arrays + count).
// `atp_cp_trivially_joinable` then treats AC-equal normal forms as
// joinable for these symbols.
typedef struct AtpAcInfo {
  u64 ac_mask;          // bit i set iff CTR label i is AC
} AtpAcInfo;
fn void      thvm_atp_set_ac_mask(u64 mask);
fn u64       thvm_atp_get_ac_mask(void);
fn void      thvm_atp_auto_ac    (const Term *lhs, const Term *rhs, u32 n_eqns);

// AC unification at one CP-generation position.  cp_visit calls this
// when its current subterm `sub` and rule-j's lhs `lj` share the same
// AC top label.  Enumerates leaf-bijection AC unifiers (up to a small
// permutation cap), emits one CP per unifier into `out[count..]`, and
// returns the new `count`.  Defined in src/atp/ac.c.  No-op return
// (count unchanged) when AC mask is 0 / leaves don't bijection /
// permutation cap exceeded.
fn u32       atp_ac_unify_emit_cps(Term li, Term ri,
                                   Term sub, Term lj, Term rj,
                                   const u32 *p_path, u32 p_len,
                                   CriticalPair *out, u32 cap, u32 count);
#endif

fn void      thvm_atp_set_use_rule_subsume_drop(AtpState *s, u8 on);
fn void      thvm_atp_set_w2(AtpState *s, u32 modulo, u8 mode);
fn void      thvm_atp_set_use_unorient_index(AtpState *s, u8 on);
fn void      thvm_atp_set_use_lazy_normalize(AtpState *s, u8 on);
// Vampire-style Limited Resource Strategy.  When set together with a
// wall-clock deadline (thvm_atp_set_wall_deadline), thvm_atp_select_cp
// periodically prunes the CP queue of CPs above a budget-derived weight
// horizon -- the saturator concentrates effort on the proof-tractable
// subset it can actually reach.  Sound (incomplete in principle, complete
// in budget); 0 = off (default) -> engine byte-identical.
fn void      thvm_atp_set_use_lrs(AtpState *s, u8 on);
fn void      thvm_atp_set_use_sos(AtpState *s, u8 on);
fn void      thvm_atp_set_use_fwd_subsume(AtpState *s, u8 on);
fn void      thvm_atp_set_use_bwd_subsume(AtpState *s, u8 on);
fn void      thvm_atp_set_use_bwd_demod(AtpState *s, u8 on);

// Proof-trace capacity (entries).  Defaults to ATP_MAX_TRACE; overridable
// once per process via THVM_ATP_TRACE_MAX (read at first call).  An unset
// env is byte-identical to the historical fixed 131072-entry trace.
fn u32       thvm_atp_trace_cap(void);

// Set a wall-clock deadline.  `seconds_from_now` is a float duration
// (e.g. 5.0 = 5 seconds); pass 0.0 to clear the deadline.  The
// saturator periodically checks the deadline in `thvm_atp_step` and
// returns `ATP_TIMEOUT` once it's crossed -- the primary defense
// against recursively-defined axioms (Y combinator) that generate
// unbounded CP fan-out and would otherwise run forever.
fn void      thvm_atp_set_wall_deadline(AtpState *s, double seconds_from_now);

// Host abort hook.  When set, the saturation loop polls it and returns
// ATP_ABORTED as soon as it returns nonzero -- letting a host like the
// WL LibraryLink glue forward Abort[] / TimeConstrained[] into the C
// engine (which otherwise runs uninterruptible to completion).  NULL =
// no host abort source (the default).
extern int (*thvm_atp_abort_hook)(void);

// === atp/precedence -- algebraic-structure detection =================
// Ported from Waldmeister's `PhilMarlow` (algebraic-structure
// recognition; "Erkennung algebraischer Strukturen") and
// `Praezedenzgenerator` (precedence generator).  Given an axiom
// set, classify each function symbol's algebraic properties and
// auto-tune the KBO/LPO precedence so completion converges better.
//
// `atp_analyze_axioms` mirrors PhilMarlow's `ACSymboleSuchen`
// ("search for AC symbols") + `DistributivgesetzeSuchen` ("search
// for distributive laws"), plus the Sinai law table's idempotence
// / identity / inverse patterns.  Detection predicates port
// Waldmeister's `TO_IstKommutativitaet` / `TO_IstAssoziativitaet`
// / `TO_IstDistribution` (`WASIC/TermOperationen.c`).
//
// `atp_generate_precedence` ports `Praezedenzgenerator`'s
// `FuchsPraezedenz` ordering rule: equation symbols ranked by
// arity (n-ary > ... > unary > constant), with structurally
// "defining" symbols (units, inverses, distributors) promoted.

// Per-symbol algebraic-property record.  One entry per CTR label;
// `seen` is 0 for labels with no occurrence in the axiom set.
typedef struct {
  u8  seen;             // 1 if this label appears in the axiom set
  u32 arity;            // observed function-symbol arity
  u8  is_commutative;   // some axiom is f(x,y) = f(y,x)
  u8  is_associative;   // some axiom is f(f(x,y),z) = f(x,f(y,z))
  u8  is_idempotent;    // some axiom is f(x,x) = x
  u8  has_left_unit;    // some axiom is f(e,x) = x
  u8  has_right_unit;   // some axiom is f(x,e) = x
  u8  has_inverse;      // some axiom is f(i(x),x)=e or f(x,i(x))=e
  u8  is_unit_symbol;   // this label is the unit constant `e`
  u8  is_inverse_symbol;// this label is the inverse operator `i`
  u8  distributes;      // f distributes over some other operator
  u32 distributes_over; // label of the operator f distributes over
} AtpSymProps;

// Analyze `n_eqns` equation pairs (parallel `lhs[]` / `rhs[]`
// arrays of TAG_CTR + TAG_FVR terms).  Fills `out[0..n_labels)`
// with one AtpSymProps per CTR label.  `out` must hold at least
// `n_labels` entries; caller zero-inits not required (the routine
// clears it).
fn void atp_analyze_axioms(const Term *lhs, const Term *rhs, u32 n_eqns,
                           AtpSymProps *out, u32 n_labels);

// Generate a precedence array from a completed analysis.  Writes
// `prec[0..n_labels)`; higher value = greater symbol.  Labels not
// `seen` get rank 0.  Returns the number of distinct ranks used.
fn u32  atp_generate_precedence(const AtpSymProps *props, u32 n_labels,
                                u32 *prec);

// Convenience: analyze then generate in one call.  Equivalent to
// atp_analyze_axioms followed by atp_generate_precedence on a
// scratch AtpSymProps array (capped at WALD_MAX_SYMBOLS labels).
// Occurrence-frequency precedence (Vampire `sp=occurrence` / E
// `-G InvFreqRank`): rank by ASCENDING occurrence count -- rarest
// symbol gets the highest rank, most common the lowest.  Ignores
// structural detection; pure frequency.  See src/atp/precedence.c.
fn u32  atp_occurrence_precedence(const Term *lhs, const Term *rhs, u32 n_eqns,
                                  u32 n_labels, u32 *prec);
fn u32  atp_auto_precedence(const Term *lhs, const Term *rhs, u32 n_eqns,
                            u32 n_labels, u32 *prec);

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

// 7a: in-loop Cheney collection for the saturation engine.  Gathers
// every live Term reachable from the AtpState (rule set, CP queue,
// goal, trace entries, narrowing witness) into a root array, runs
// gc_collect, and writes the relocated Terms back.  thvm_atp_step
// calls this when the dyn heap crosses the half-full mark so a long
// completion run no longer exhausts from-space.  Returns 1 if a
// collection ran, 0 if GC is disabled or `s` is NULL.  Safe to call
// directly (e.g. from tests) -- it is a no-op without GC.
fn u8        thvm_atp_gc_collect     (AtpState *s);

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

// === proof extraction ================================================
// The trace[] array (above) records the COMPLETION derivation -- which
// critical pairs birthed which rules.  A *proof* is the orthogonal
// object: the equational rewrite chain that joins the two conjecture
// sides.  thvm_atp_proof_extract reconstructs it for a goal closed by
// the single-normal-form check (thvm_atp_goal_check's `kbo_eq(l, r)`
// path): it re-normalizes goal_lhs and goal_rhs under the rule set R,
// recording every leftmost-outermost forward rewrite.  The proof is
// the goal_lhs chain (side 0) then the goal_rhs chain (side 1), both
// forward -- it rewrites L down to its normal form, then R down to
// the same normal form, so the assembled equation L == R reaches the
// tautology NF == NF.
//
// Extraction normalizes against whatever rule set the passed AtpState
// holds.  For an axiom-cited (verifier-friendly) chain, pass a state
// whose R is the oriented input axioms only; a completion-saturated R
// yields a chain over derived rules instead.
//
// A goal closed only by the MNF bidirectional search (a symmetric
// conjecture whose two sides share no normal form) is NOT single-NF
// extractable; thvm_atp_proof_extract returns 0 for it.
#define ATP_PROOF_MAX_DEPTH 32   // redex-path depth cap per step
// Goal-rewrite chain-length cap, each side.  Must track thvm_atp_goal_check's
// NORM_CAP (1<<16): goal_check proves a goal by normalizing each side up to
// NORM_CAP rewrites, so the proof extractor that REPLAYS that normalization
// to build the closing chain needs at least the same reach -- a smaller cap
// stopped the replay short of the shared normal form on a deep completion
// (e.g. AndAssociativity over the single Sheffer/nand axiom), so
// thvm_atp_proof_extract returned 0 (empty MainSteps) and the ProofObject
// reconstruction failed even though the engine reported PROVED.
#define ATP_PROOF_MAX_STEPS (1u << 16)

typedef struct {
  u32  side;       // 0: a step on goal_lhs's chain; 1: on goal_rhs's
  u32  rule;       // index into s->lhs / s->rhs -- the rule applied
  u8   fwd;        // 1: rule fired lhs->rhs; 0: rhs->lhs (reversed)
  u8   pos_len;    // length of the redex path within the side's term
  u8   pos[ATP_PROOF_MAX_DEPTH];   // child-index path to the redex
  Term before;     // the rewritten side, immediately before the step
  Term after;      // the rewritten side, immediately after the step
} AtpProofStep;

// Fill out[0..cap) with the extracted proof steps; returns the step
// count, or 0 when the goal is not single-NF provable under the
// current R (no goal set, an existential goal, or an MNF-only join).
// The before/after Terms reference live heap cells -- consume them
// (or copy them out) before the next allocation, like witnesses.
fn u32       thvm_atp_proof_extract(AtpState *s, AtpProofStep *out,
                                    u32 cap);

// Milestone 10: extract a proof for a goal closed by the MNF
// bidirectional front search (a symmetric conjecture the single-NF
// path cannot close).  Walks the two parent chains up from the join
// term `meet`: GREEN (goal_lhs's front) emitted as side 0, RED
// (goal_rhs's front) as side 1, both running seed -> meet, so the
// assembled chain reaches `meet == meet`.  Each step's rule / position
// / direction is reconstructed by replaying a one-step rewrite under
// the final rule set.  Returns the step count, or 0 when there is no
// join, the dylib lacks -DATP_MNF, or an edge no longer replays.  Like
// thvm_atp_proof_extract the before/after Terms are live heap cells.
fn u32       thvm_atp_mnf_proof_extract(AtpState *s, AtpProofStep *out,
                                        u32 cap);

// Serialize an extracted proof to human-readable text into `buf`.
// Each line: "<L|R> rule <i> <fwd|rev> @<path>: <before> => <after>".
// Truncates silently on overflow.  Returns the byte count written.
fn u32       thvm_atp_proof_serialize(const AtpProofStep *steps,
                                      u32 n_steps, char *buf, u32 cap);

// === ENIGMA-style critical-pair feature extraction + dataset ========
// A LEARNED CP selector (Jakubuv-Urban ENIGMA, adapted to equational
// completion) needs labelled training data: for every CP the engine
// PROCESSED in a SUCCESSFUL proof, a fixed-length numeric feature
// vector + a binary label "did this CP contribute to the proof?".
// This block produces that dataset; training (logistic regression /
// GBDT) and the resulting fast C scorer are a separate later step.
//
// Feature layout (all f32; see thvm_atp_cp_features for the exact
// computation).  Index -> meaning:
//   0  size_sum        symbol_count(l) + symbol_count(r)
//   1  max_depth       max(term_depth(l), term_depth(r))
//   2  n_distinct_vars distinct FVR ids across l and r
//   3  n_var_occ       total FVR occurrences across l and r
//   4  weight_add      ADD-mode priority (symbol-count sum)
//   5  weight_gt       GT-mode priority (ordering-directed KBO weight)
//   6  weight_mix2     MIX2-mode priority (g*10 + (wl+wr))
//   7  goal_weight     CPinGoal weight (0 in completion mode)
//   8  age             cp_seq-style birth order == the row index
//   9  top_symbol_l    CTR label at l's root (0 if l is a var/atom)
//   10 top_symbol_r    CTR label at r's root
//   11 shares_goal_sub 1 if l or r shares a top-symbol subterm with
//                      the (normalized) goal, else 0
//   12 orientable      1 if the CP orients (KBO_GT/KBO_LT), else 0
//   13 unif_measure    depth-weighted l/r disagreement (U1 measure)
// ATP_CP_FEATURE_DIM is the vector length; bump it if you add features.
#define ATP_CP_FEATURE_DIM 14u

// Compute the feature vector for a CP (lhs, rhs) into out[0..DIM).
// `age` is the row's birth order (caller passes the running count).
// Pure read of `s` (uses the rule set / goal / KBO config for the
// weight + goal features); never mutates engine state.  Safe to call
// standalone (e.g. from a scorer) independent of the recording flag.
fn void      thvm_atp_cp_features(const AtpState *s, Term lhs, Term rhs,
                                  u32 age, float *out);

// Enable/disable recording of processed-CP feature rows.  OFF (0) by
// default: select_cp records nothing and the engine is byte-identical.
// ON (1): every CP returned by thvm_atp_select_cp appends a row
// (features + trace id, label initialized 0).  Toggle before running.
fn void      thvm_atp_set_record_cp_features(AtpState *s, u8 on);

// After a SUCCESSFUL proof, label each recorded row: 1 if the CP's
// trace id is an ancestor (in the trace DAG) of the goal-closing
// step, else 0.  The proof set is computed from thvm_atp_proof_extract
// (which RULES join the goal) -> those rules' r_trace[] TRACE_ORIENT
// ids -> transitive parent closure over trace[].  Returns the proof-
// set size (count of distinct selected-CP trace ids labelled 1), or 0
// if the goal is not single-NF extractable / nothing was recorded.
fn u32       thvm_atp_cp_label(AtpState *s);

// Append the labelled dataset to a TSV file at `path` (created if
// absent; `header` writes a column-name line first -- pass 1 only for
// the first proof in an accumulating run).  One row per recorded CP:
//   label \t f0 \t f1 \t ... \t f<DIM-1>
// Returns the number of rows written, or 0 on a NULL/empty/IO failure.
// Caller normally runs thvm_atp_cp_label first; unlabelled rows export
// with label 0.
fn u32       thvm_atp_cp_dataset_append(const AtpState *s,
                                        const char *path, u8 header);

// === wald/ ===
// Parser for Waldmeister .pr-style spec files.  Stage 6.3 of
// docs/plans/waldmeister_ic_atp_tasks.md.  WaldSpec holds the
// parsed signature + variable table + equations + single
// conclusion goal; downstream feeds it to thvm_atp_run.
#define WALD_MAX_SYMBOLS 64
#define ATP_REL_LEVEL_MAX 8u
#define ATP_REL_LEVEL_REMOTE 255u
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
// 7c': re-heapify the CP queue after a caller populated the queue
// slots / n_cps directly (the normal path uses the internal
// heap-push).  Required before select / peek on a hand-built queue.
fn void      thvm_atp_cp_reheapify(AtpState *s);

// Stringterms port: queue slot i holds a packed byte string, not a
// pair of heap Terms.  Callers that build the queue directly (chiefly
// tests) go through these accessors -- `cp_set` packs (lhs, rhs) into
// slot i (freeing any prior buffer there), `cp_get` unpacks it back to
// two fresh transient heap Terms.
fn void      thvm_atp_cp_set      (AtpState *s, u32 i, Term lhs, Term rhs);
fn void      thvm_atp_cp_get      (const AtpState *s, u32 i,
                                   Term *lhs, Term *rhs);

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

// Step session: persistent across calls so the WL stepper doesn't
// re-scan HEAP_NEXT on every TInteract.  attach() walks roots+heap
// once, builds a Term -> heap-loc inverse index plus a parent map,
// and returns the initial redex count.  step_fire() fires one redex
// using the inverse index for parent-slot patching.  drain_fresh()
// returns redex-status flips since the previous drain (post \ pre).
// detach() releases all session memory and restores legacy linear-
// scan paths.
fn u32  redex_step_attach(Term *roots, u32 n_roots);
fn Term redex_step_fire(Term redex);
fn u32  redex_step_drain_fresh(Term *out, u32 cap);
fn void redex_step_detach(void);

// === runtime lifecycle ===
void thvm_init(void);
void thvm_free(void);

#endif // THVM_H
