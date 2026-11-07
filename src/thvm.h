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
// Defining _POSIX_C_SOURCE disables glibc's *implicit* _DEFAULT_SOURCE,
// which hides the BSD extensions we use (MAP_ANONYMOUS in arena_map).
// Re-request it so a strict `-std=c11` test build (no -D_GNU_SOURCE, which
// the WL/py builds pass) still exposes them.  Linux only; macOS is fine.
#if defined(__linux__) && !defined(_DEFAULT_SOURCE)
#define _DEFAULT_SOURCE 1
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>        // INFINITY / fabsf etc. (uop_walk reduce init)
#include <stdatomic.h>   // _Atomic typing for the per-context counters
#include <sys/mman.h>    // mmap/munmap for the big heap + book arenas
#include <sys/stat.h>    // fstat for disk-tensor file size validation
#include <fcntl.h>       // open() for the mmap-backed disk tensor
#include <unistd.h>      // close/sysconf(_SC_PAGESIZE) for disk-tensor mmap

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

#define SUB_SHIFT 63
#define TAG_SHIFT 56
#define EXT_SHIFT 38

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

// === Tensor tags ===

#define TAG_TEN  8   // tensor handle.    atom; val = tensor id, ext = dtype
#define TAG_UOP  9   // uop graph node.   val = heap loc, ext = opcode
#define TAG_NUM  10  // 32-bit scalar.    atom; val = raw u32 / f32 bits, ext = dtype

// === Lazy named definitions + allocator (HVM4 style) ===
#define TAG_REF  11  // book reference.   val = name id (index into DEFS)
#define TAG_ALO  12  // lazy alloc.       val = dyn heap loc holding [book_term, NUM(state_id)]

// === Numeric switch + binary op (Phase-2 termination + counter) ===
#define TAG_OP2  13  // binary op.        val = heap loc -> [x, y], ext = opcode (OP_*)
#define TAG_MAT  14  // numeric switch.   val = heap loc -> [handler, fallback], ext = match value

// === IC-as-ATP layer ===
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
// Mirrors tinygrad's full dtype set (tinygrad/dtype.py:
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
    DK_INT4,    // signed nibble, 2 per byte
    DK_UINT4    // unsigned nibble
} DTypeKind;

typedef struct {
    u8          itemsize;       // bytes per element; 0 for packed/reserved
    u8          kind;           // DK_*
    u8          bits;           // bits per element (4 for nibble)
    u8          is_signed;      // 1 for signed integer/float, 0 otherwise
    char const *name;           // canonical short name ("f32", "i8", ...)
} DTypeInfo;

u64             dtype_storage_bytes(u32 dt, u64 numel);

// === UOp opcodes (TAG_UOP ext field) ===
// Each opcode's heap layout is noted beside its #define.

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
// UOP_DEFINE_VAR: the runtime value of a symbolic (kvar) extent, an int.
//   Heap = [NUM(packed_extent)] (kvar_pack_extent).  Arity 0.  tinygrad's
//   DEFINE_VAR (uop/ops.py); reduce_unparented multiplies a SUM reduce by
//   the extent of a range its body never reads (codegen/simplify.py:88), and
//   for a symbolic range that extent is this leaf.
#define UOP_DEFINE_VAR  18
// (slot 19 was UOP_FWD and slot 18 UOP_GRAD -- folded into TAG_DP0 /
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
                             //   sub-item (b).
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
#define UOP_RANGE       25   // heap = [NUM(axis_id), NUM(axis_type), NUM(extent)].
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
#define UOP_ISHR        51   // heap = [a, b]; right shift (a >> b), arithmetic
                             //   for a signed dtype.  Emitted by the fast_idiv
                             //   index rewrite to lower `x // c` to a
                             //   magic-number multiply-shift (uop/index_simplify.c)
                             //   and by Tensor.rshift, tinygrad's Ops.SHR.
#define UOP_ISHL        44   // heap = [a, b]; left shift (a << b), tinygrad's
                             //   Ops.SHL.  IAND, IOR, IXOR and the shifts are also
                             //   value ops, typed by their operands.
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
                             //         NUM(instance), NUM(d0), ...,
                             //         NUM(d_{ndim-1})];
                             //   ext = UOP_BUFFER opcode.  Hash-cons by
                             //   (scope, dtype, ndim, instance, dims);
                             //   instance = kernel slot + 1 (0 = output).
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
#define UOP_OPT          39  // heap = [target, NUM(kind), NUM(factor)];
                             //   Annotation node attaching an optimisation
                             //   directive to `target`.  factor=0 when the
                             //   directive carries no scalar (TC, LOCAL).
                             //   The renderer walks UOp shape + OptOp
                             //   annotations to fire specialised templates.
// === Unified rangeify boundary (mirrors tinygrad's Ops.STAGE) ===
// tinygrad renamed BUFFERIZE to STAGE in #16125; UOp.bufferize builds a
// STAGE (uop/ops.py:679).
// Heap layout: [value, NUM(addrspace), NUM(removable), NUM(n_ranges),
//               range_0, ..., range_{n_ranges-1}].
//   value         : the producer Term whose output this buffer holds.
//                   In tinygrad src=(new_src,)+closed_ranges; we mirror by
//                   storing the value at slot 0 and the closed ranges
//                   following the (addrspace, removable, n_ranges) header.
//   addrspace     : UOP_SCOPE_GLOBAL / LOCAL / REG (mirrors
//                   BufferizeOpts.addrspace in tinygrad/schedule/indexing.py).
//   removable     : 1 if a downstream pass may collapse this boundary into
//                   its single consumer (mirrors BufferizeOpts.removable,
//                   indexing.py:90).  0 keeps it materialized: tinygrad
//                   clears it for a src in ALWAYS_CONTIGUOUS or an input of
//                   a custom kernel.  A contiguous() STAGE (arg=None) or a
//                   COPY never gets here: prepare drops one with nothing to
//                   do and lowers the rest to a STORE into a fresh buffer
//                   (schedule/prepare.py:215-231).
//   n_ranges      : number of "closed" range Terms that follow.
//   range_i       : the UOP_RANGE leaves that index this buffer; their
//                   extents are the buffer's shape.  Mirrors tinygrad's
//                   `(new_src,)+closed_ranges` src tuple at
//                   tinygrad/schedule/indexing.py:92.
//
// pm_apply_rangeify in tinygrad emits a STAGE at every realize boundary
// the run_rangeify walk decided; thvm's unified pass writes UOP_BUFFERIZE
// onto the main heap at the same boundaries so materialize.c can read a
// single source of truth.
#define UOP_BUFFERIZE   40
// 41, 42 = UOP_IOR / UOP_IXOR (declared in the Symbolic INDEX layer above).
// Identity-forward, STOP-backward marker (tinygrad UOp detach / stop-grad).
// uop_grad treats it as a leaf w.r.t. its child (the cotangent dies), and
// materialize strips it (materialize_strip_detach) before lifting, so no
// kernel/render/walker path ever sees it.  Used by BatchNorm.calc_stats
// (`y = x - mean.detach()`) so the variance gradient does not flow back
// through the mean -- mathematically d(var)/d(mean)=0, so the result is
// identical but the backward graph is far smaller.
#define UOP_DETACH      43
// 44 = UOP_ISHL, 51 = UOP_ISHR (declared in the Symbolic INDEX layer above).
#define UOP_POW         45   // heap = [b, e]; b ** e, tinygrad's Ops.POW, which
                             //   uop/sym.c lowers to xpow or xpow_int at emit.
#define UOP_THREEFRY    46   // heap = [x, key]; tinygrad's Ops.THREEFRY over uint64 x and key,
                             //   thvm_threefry (codegen/threefry.c) in kernels and the walker.
// 47-50: unassigned.
// UOP_COPY: lazy device transfer.  Heap = [src, NUM(device+1)] (arity 1,
//   src is the only child; the trailing NUM holds the EXPLICIT target
//   device, device+1 so generic=-1 stores as 0).  The COPY carries src's
//   shape + dtype and uploads src to its target device at materialize
//   time.  Identity (no kernel) when the target already holds src (e.g.
//   CPU realize of a CPU host leaf).  An explicit device (CPU/METAL)
//   names the backend directly; the generic sentinel (-1) uploads to
//   CURRENT_BACKEND.  Mirrors tinygrad's Ops.COPY, which carries the
//   target as src[1] (uop/ops.py:660 copy_to_device, ops.py:756
//   COPY.device == src[1].device; engine/realize.py:158 exec_copy).
#define UOP_COPY        52
// UOP_GATHER: heap = [src, idx, NUM(dim)] (arity 2).  idx's shape, src's
//   dtype: out[i] = src[i with i_dim := idx[i]] when 0 <= idx[i] <
//   src.shape[dim], else 0 (tinygrad Tensor.gather, mixin/op.py:1023-1043).
//   See uop/gather.c.
// UOP_SCATTER: heap = [base, idx, src, NUM(dim), NUM(kind),
//   NUM(include_self)] (arity 3).  base's shape and dtype: src combined
//   into base at the positions idx names along dim, with a SCATTER_* kind
//   (tinygrad scatter_reduce / scatter, mixin/op.py:1126-1177).  See
//   uop/scatter.c.
#define UOP_GATHER      53
#define UOP_SCATTER     54
#define UOP_SIN         55   // heap = [src]
#define UOP_COUNT       56

// REDUCE kinds packed into the high bits of UOP_REDUCE's EXT field.
#define REDUCE_SUM   0
#define REDUCE_MAX   1
#define REDUCE_PROD  2   // tinygrad Ops.MUL

// UOP_SCATTER combine kinds (NUM(kind) cell).
#define SCATTER_SUM      0
#define SCATTER_PROD     1
#define SCATTER_MAX      2
#define SCATTER_MIN      3
#define SCATTER_REPLACE  4

// === Capacities ===
#define HEAP_CAP     (1ULL << 28)   // 256M cells * 8B = 2 GiB.  Cheney splits in half (1 GiB per semi-space).  Bumped from 1<<26 so the HVM bench fib_nat (~1.2B cells over its tree) fits without GC.
#define WNF_CAP      (1ULL << 16)   // 64K stack slots.
#define TENS_CAP     (1ULL << 24)   // 16M tensor descriptor slots (mmap-backed, virtual-only; the Krea 12B DiT forward generates >4M views, and the JIT capture of one forward must fit before replays reuse its buffers).
#define KERNELS_CAP  (1ULL << 18)   // 256K compiled kernels.
#define BOOK_CAP     (1ULL << 28)   // 256M cells of static def template heap (2 GiB) -- iter Z+? per-thread arena needs at V>=8 SAT collapse.
#define DEFS_CAP     256            // max named definitions for TAG_REF.
#define ALO_STATE_CAP (1ULL << 22)  // ALO substitution-chain entries.
#define MAX_DIM      16             // max tensor rank
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

// A lazy value as tinygrad's Tensor carries it (src/nn/): the UOp graph with its
// dtype and shape, so building a layer never walks the graph under it.  t == 0
// is a failed op.
typedef struct {
  Term  t;
  u32   dtype;
  Shape shape;
} Tensor;

// nn_elementwise's ops (src/nn/elementwise.c).
#define NN_EXP       0
#define NN_LOG       1
#define NN_SIGMOID   2
#define NN_TANH      3
#define NN_RELU      4
#define NN_GELU      5   // the tanh form
#define NN_GELU_ERF  6
#define NN_ERF       7
#define NN_RSQRT     8
#define NN_SQUARE    9
#define NN_SIGN     10
#define NN_ABS      11
#define NN_ELEMENTWISE_COUNT 12

// nn_call's ops (src/nn/call.c): the one entry the py, WL and FFI hosts call,
// which look the ids up by name (nn_op_id).  NN_OP_COUNT + NN_EXP .. are the
// elementwise ops.
#define NN_OP_MOVE              0
#define NN_OP_CAST              1
#define NN_OP_FULL              2
#define NN_OP_BINOP             3
#define NN_OP_REDUCE            4
#define NN_OP_WHERE             5
#define NN_OP_MAXIMUM           6
#define NN_OP_SOFTMAX           7
#define NN_OP_LAYERNORM         8
#define NN_OP_BATCHNORM         9
#define NN_OP_DOT              10
#define NN_OP_LINEAR           11
#define NN_OP_ARANGE           12
#define NN_OP_TRI              13
#define NN_OP_SPARSE_CE        14
#define NN_OP_SDPA             15
#define NN_OP_POOL             16
#define NN_OP_MAX_POOL2D       17
#define NN_OP_CONV2D           18
#define NN_OP_CONV_TRANSPOSE2D 19
#define NN_OP_POW              20
#define NN_OP_COUNT            21

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
  u32      assign_kvar_id;      // KV-cache append dst: the kvar whose RUNTIME
                                // value is the leading-axis ROW offset of this
                                // shrink-view alias (start_pos).  0 = not a
                                // kvar-offset assign dst (a plain offset-0
                                // weight ASSIGN).  Lets the JIT ASSIGN replay
                                // re-resolve the write offset from kvar_runtime
                                // at FIRE (the offset baked into view.offset at
                                // capture is only that step's row).
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
// tinygrad's applied-opt log; the per-axis kax_type and extents come from
// the lifted DAG's RANGE leaves (axes_collect, codegen/axis.c).  The
// codegen variant emitter walks these
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

#define MAX_AXES   32
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

typedef struct {
  u8  op;        // KOP_*
  u32 axis;      // axis_id; meaning depends on op.  Kept u32 as defense:
                 // uop_dag_renumber_axes now makes per-kernel axis_ids
                 // dense 0..n (small), but a u8 here would silently
                 // truncate at the construction-site casts if any kernel
                 // ever reached apply without the renumber -- and that
                 // truncation (344 -> 88) is exactly what made find_range
                 // miss, hand_opts apply nothing, and a conv kernel render
                 // fully-serial -> an 80-min nvrtc hang at BS=128.  u32
                 // makes that failure mode impossible for 4 bytes.
  u32 arg;       // op-specific (split factor for UPCAST/UNROLL, target
                 // axis index for SWAP, full axis size for GLOBAL,
                 // MMA tile size for TC, ...)
} KOpt;

typedef struct {
  // The applied_opts log + autotune bookkeeping bits; the axes themselves
  // live in the lifted DAG (axes_collect).
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
  u8   hand_coded_done;        // 1 = kernel_hand_coded_opts has run.
                               // Separate from `autotuned` so applying
                               // the hand-coded heuristic does NOT block
                               // a subsequent BEAM/autotune pass (the
                               // fire path runs hand_opts then autotune;
                               // sharing one flag let hand_opts suppress
                               // BEAM on every non-WL path).
} KpSchedule;

struct Backend {
  u32   id;
  // view_aware = 1 if dispatch_kernel pre-materializes non-contig
  // input TenDescs (sub-item f3b/c/d/e/g view-only aliases) via
  // view_strided_index before reading buffer bytes.  CPU sets this
  // (cpu_dispatch_kernel gathers a ShapeTracker chain into a contiguous
  // temp, and every rung reads a single strided view in place); Metal
  // does NOT (its shaders read bufs flat).  view_resolve (materialize.c)
  // gates on this flag, so view-only movement ops fall through to
  // kernel emission when the active backend can't consume aliases.
  u8    view_aware;
  int   (*init)(void);
  void  (*shutdown)(void);
  u32   (*buf_alloc)(u64 nbytes);
  void  (*buf_free) (u32 buf_id);
  void  (*buf_incref)(u32 buf_id);
  void  (*buf_decref)(u32 buf_id);
  // Optional: sticky JIT pin -- mark a buf as held by an active JIT
  // capture so subsequent realize/rollback cycles don't free it.
  // Cleared on jit_capture_drop.  NULL on backends that lack the
  // mechanic; the JIT then falls back to mark_preserved (which is
  // per-realize and gets cleared at end-of-realize).
  void  (*buf_jit_pin)(u32 buf_id);
  void  (*buf_jit_unpin)(u32 buf_id);
  int   (*buf_read) (u32 buf_id, void *dst, u64 nbytes);
  int   (*buf_write)(u32 buf_id, const void *src, u64 nbytes);
  int   (*buf_copy) (u32 dst_buf_id, u32 src_buf_id, u64 nbytes);
  // Offset-aware variants for an in-place ASSIGN whose dst is a SHRUNK
  // view of a persistent buffer (KV-cache append: a {1,dim} K_t into a
  // {nCtx,dim} cache at a runtime row offset).  buf_write_at writes the
  // host bytes at `byte_off`; buf_copy_at copies nbytes from src buf
  // (byte 0) into dst buf at `byte_off`.  Both bounds-check
  // byte_off+nbytes against the dst buffer's size.  May be NULL on
  // backends without offset writes -- interact_assign_with then falls
  // back to the byte-0 buf_copy/buf_write (whole-buffer) path.
  int   (*buf_write_at)(u32 buf_id, u64 byte_off, const void *src, u64 nbytes);
  int   (*buf_copy_at) (u32 dst_buf_id, u64 dst_byte_off, u32 src_buf_id, u64 nbytes);
  // Optional (may be NULL): refcount probe + free-list hand-off used by
  // the per-realize memory planner (materialize.c) for in-pass
  // physical-buffer reuse.  buf_freelist_push(b) marks b's storage
  // recyclable (refcount -> 0); buf_freelist_remove(b) un-recycles it
  // (refcount -> 1) if it's still on the list and wasn't re-issued.
  u32   (*buf_refcount)(u32 buf_id);
  void  (*buf_freelist_push)(u32 buf_id);
  void  (*buf_freelist_remove)(u32 buf_id);
  // Optional: storage-root walker for alias-aware analysis (JIT replay
  // dedup, materialize liveness).  Returns the buf id at the end of the
  // parent chain (the owning slot).  Two bufs alias storage iff their
  // roots match.  NULL on backends without view aliasing -- caller
  // should fall back to identity (`a == b`).
  u32   (*buf_storage_root)(u32 buf_id);
  // Optional: the logical size in bytes of buf_id, the bytes its allocation
  // requested (a best-fit freelist recycle can back it with a larger
  // block).  0 when unknown.  The BEAM isolated bench sizes a chain-composed
  // input's scratch to it: tinygrad benches every PARAM on its whole buffer
  // (codegen/opt/postrange.py:258).  NULL: that bench declines such kernels.
  u64   (*buf_nbytes)(u32 buf_id);
  // Optional: GPU-side zero-fill of a buffer's full extent, ordered before the
  // next dispatch by the backend's own hazard tracking (no host sync).  The
  // JIT replay loop calls this to clear a RECYCLED (replay_packed) output slot
  // before its producer fires, so a partial-write kernel leaves zeros -- not
  // the prior occupant's stale bytes -- in the elements it doesn't store.
  // NULL on backends whose buffers are always exactly producer-covered.
  void  (*buf_zero)(u32 buf_id);
  void  (*dispatch_begin)(void);
  void  (*dispatch_flush)(void);
  void  (*dispatch_end)(void);
  int   (*dispatch_kernel)(struct KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id);
  // Optional: on-device strided gather.  Compute dst[k] = src[strided(k)]
  // entirely on the backend, where the strided index is derived from
  // `view` (ndim, offset, dims[], strides[]).  Lets materialize_root_alias
  // flatten a non-contig realize-root view WITHOUT a GPU->host buf_read +
  // CPU gather + host->GPU buf_write (which forces a per-realize device
  // sync on Metal).  Returns 0 on success, -1 if the backend declines
  // (unsupported dtype / chained views / no impl) so the caller falls back
  // to the host read+gather+write path.  NULL on backends without it.
  int   (*gather_strided)(u32 dst_buf_id, u32 src_buf_id,
                          TenDesc const *view);
};

// Hard ceiling on a single backend buffer allocation, in bytes.  A
// pathological kernel program (e.g. an im2col-style EXPAND that
// materializes a multi-billion-element intermediate -- the conv-1
// backward dInput on the per-op fallback path is ~947M elements ≈
// 3.8 GiB at BS=512, and the doc records a 5.24-billion-element ≈
// 21 GiB variant)
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
// in_bufs is a heap array of in_cap slots, one per kernel input, so a
// lifted kernel takes any number of inputs: tinygrad caps kernel buffers
// only on WEBGPU (schedule/rangeify.py:170), and wide fusions are real
// (a Krea DiT kernel reads 64).  kernel_lift_reserve grows it and
// kernel_lift_release frees it.  A KernelEntry's cached_lift grows with
// the entry's other per-input arrays (kernel_inputs_reserve) and is
// released by kernel_free_arrays.
typedef struct {
  // Topmost UOP_STORE for the kernel's single output.
  Term  store_root;
  // Output buffer Term (the UOP_BUFFER the STORE writes to).
  Term  out_buf;
  Term *in_bufs;
  u32   in_cap;
  u32   n_inputs;
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
  // the input is non-contig (single-view stride pattern -- a multi-
  // view chain is gathered into a contiguous temp by
  // cpu_dispatch_kernel first, unless rangeify composed it into the
  // kernel INDEX), the renderer composes a strided index expression
  // `in%u[s0*c0(i) + ... + offset]` into the body loop, so a strided
  // read needs no pre-materialize pass.  Mirrors tinygrad's
  // codegen-time index UOP inlining (each input's stride pattern is
  // hardcoded into the generated kernel; different patterns get
  // different dylibs).
  View     *input_views;
  // For inputs that aren't statically a TenDesc (e.g., a free TAG_VAR
  // that gets bound to a TEN at fire time via APP-LAM beta), we
  // store the symbolic Term value here.  kernel_fire_by_id resolves
  // each non-zero entry through term_resolve before reading buffers.
  Term     *input_terms;
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
  // Embedded by-value; its in_bufs array is reserved with the
  // per-input arrays above (kernel_inputs_reserve) and freed by
  // kernel_free_arrays.
  KernelUopLift cached_lift;
  // One run's ops and bytes (cg_kernel_estimates), taken from est_root, a
  // past store_root compared only: a moved or new root takes them again.
  Term      est_root;
  u64       est_ops;
  u64       est_mem;

  // Snapshot of cached_lift.store_root at materialize time, before any
  // kernel_apply_opt DAG mutations.  axes_reset_to_default restores
  // cached_lift.store_root from this so autotune's bench-each-variant
  // flow can rewind DAG state after each candidate.  0 when the lift
  // declined.
  Term      cached_lift_init_root;

  // 1 = no opt pass (hand_opts, BEAM/autotune, kernel_apply_opt) touches
  // this kernel.  Set at emit for a multi-store (AFTER) root or an address
  // that depends on loaded data (uop_dag_has_dynamic_index): the opts
  // assume affine addresses, and a read-modify-write scatter is race-free
  // only as a serial loop (UPCAST lanes would lose duplicate-index updates).
  u8        no_opts;

  u32       consumer_count;        // # of OTHER kernels whose input_tids
                                   // trace back (via TENS[tid].producer_kid)
                                   // to this kernel.  Populated by
                                   // kernel_compute_consumer_counts.  Used
                                   // by the refcount-driven free pass to
                                   // decide when this kernel's output buf
                                   // is no longer needed.
  u8        stripped;              // kernel_gc_sweep freed its input
                                   // arrays + lift; firing it again is a
                                   // stale reference (kernel_fire_by_id).
  u8        dispatched;            // a dispatch has written the output, so
                                   // kernel_gc_sweep may take it as committed.
  u32       fire_gen;              // last KERNEL_FIRE_GEN this kernel
                                   // dispatched at.  kernel_fire_by_id skips
                                   // when fire_gen == current gen so a
                                   // kernel referenced by N consumers in one
                                   // realize fires once instead of N times.
                                   // The generation advances per realize
                                   // scope, top-level fire and ASSIGN.
  u64       fire_assign_seq;       // ASSIGN_SEQ at this kernel's last actual
                                   // dispatch (THVM_PRECISE_FIRE_MEMO path):
                                   // skip a re-fire when no input buffer was
                                   // ASSIGNed since, instead of re-firing on
                                   // every per-assign fire_gen bump.
  u32       fire_pass;             // ASSIGN_PASS_EPOCH at last actual dispatch.
                                   // The precise memo only skips a re-fire
                                   // for a kernel that fired in the CURRENT
                                   // pass -- so cross-realize / post-JIT-
                                   // replay reads (whose buffer writes the
                                   // memo can't see) always re-fire (sound).
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

  // === Raw custom-kernel program (injection bridge) ===================
  // Non-NULL custom_program marks this entry as a hand-written kernel
  // injected via kernel_emit_custom_program (the TKernel[<|"Program"->..|>]
  // bridge), not a materialize-emitted one.  cached_lift stays zeroed so
  // every lift-driven consumer (renderers, BLAS classifier, uop walker,
  // autotune proposer) declines naturally; dispatch routes to the
  // backend's custom path (cpu_custom_dispatch / cuda_custom_dispatch).
  // Mirrors tinygrad's raw Ops.PROGRAM (uop/ops.py ProgramInfo): name +
  // source + launch dims + an explicit outs set naming which argument
  // slots the kernel WRITES (codegen/__init__.py do_to_program: a raw
  // program without outs must be treated as writing every buffer, or
  // graph replay races on recycled scratch).  All argument buffers,
  // reads and writes alike, live in input_tids[0..n_inputs); output_tid
  // mirrors the first out slot so backend/liveness plumbing keyed on
  // "the output" stays valid.
  char      *custom_program;      // heap-owned source (C / CUDA-C); freed by kernel_free_arrays
  char      *custom_name;         // heap-owned entry-point symbol
  u32        custom_grid [3];
  u32        custom_block[3];
  u32        custom_shared_bytes; // dynamic shared memory (CUDA); CPU ignores
  u64        custom_outs_mask;    // bit i set = argument slot i is written
} KernelEntry;

// A KernelEntry carrying a raw injected program (see the custom_* block).
static inline int kernel_is_custom(struct KernelEntry const *ke) {
  return ke != NULL && ke->custom_program != NULL;
}

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
  // req_nbytes: the bytes the allocation requested, the buffer's logical
  // size.  A best-fit recycle (cpu_buf_freelist_try_pop) backs a request
  // with a larger parked block, whose size nbytes keeps.
  u64   req_nbytes;
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
  // jit_pinned: STICKY hold by an active JIT capture.  The per-realize
  // `preserved` flag is cleared at end-of-realize (cpu_buf_clear_preserved),
  // but a capture's recorded buffers must survive every sub-realize's
  // pool rollback AND every later replay until the capture is dropped.
  // jit_pin sets this flag (survives clear_preserved); the reclaim paths
  // (freelist push, pool rollback) skip a pinned buf.  Mirror of
  // CudaBuf.jit_pinned (backend/cuda/init.c + buf_pool.c).
  u8    jit_pinned;
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
// See docs/multicomputation/multicomputation.md (conceptual reading) and
// docs/multicomputation/multicomputation_trace.md (build trajectory).
//
// Off by default.  Build with -DTHVM_TRACE to enable; even in that
// build, the runtime flag CURRENT_CTX->trace must be set for events
// to be recorded.  Without -DTHVM_TRACE the multi_emit() macro
// expands to ((void)0) and no TContext fields exist; every
// interact_* / heap_* compiles to the same instructions as today.
// The default-build acceptance gate is: `bench-train` / `bench-atp`
// / AOT match their pre-trace numbers within noise.

// Family classification (docs/multicomputation/multicomputation_trace.md §4).
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
// duration of the call.  Reference design: external/TinyHVM/src/tinyhvm.h:1102.
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
    void      **kid_jit_fn;             // [KERNELS_CAP] CpuJitFn per-kid cache
                                        // (cpu/jit.c).  Per-context because it
                                        // indexes this context's `kernels`; a
                                        // shared global aliased kids across
                                        // contexts and dispatched the wrong
                                        // compiled fn (train-then-rerank crash).

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
       docs/multicomputation/multicomputation_trace.md).  Only present when the
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
#define KID_JIT_FN          (CURRENT_CTX->kid_jit_fn)

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
#else
#define multi_emit(rule, family, term_a, term_b, delta_label) \
  ((void)sizeof(term_a), (void)sizeof(term_b))
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

// === heap/ ===
fn u64  heap_alloc(u64 size);
fn Term heap_read(u64 loc);
fn void heap_set(u64 loc, Term t);

// === gc/ === (Cheney semi-space copying GC; defined in heap/collect.c)
// Runtime heap allowance in cells.  Defaults to HEAP_CAP; overridable
// once per process via the THVM_HEAP_CELLS env var (read at first call).
// Drives the mmap size, gc_init split, and the gc_mark visited bitmap.
fn u64  thvm_heap_cells(void);
// Permanent (never collected) heap tail past the semi-spaces, in cells:
// THVM_HEAP_PERM_CELLS, default 0.  thvm_heap_map_cells is the mapping
// size; heap_perm_alloc bump-allocates atom-only nodes there (0 = none).
fn u64  heap_perm_alloc(u64 n);
fn int  gc_enabled(void);
fn u64  gc_from_start(void);
fn u64  gc_from_end(void);
fn u64  gc_count(void);
fn void gc_collect(Term *roots, u32 n_roots);
fn u64  gc_forward_loc(u64 loc);

// === op2 + mat ===

// 8.1b: TAG_PRI ("primitive function call") -- a thin port of HVM4's
// PRI tag.  Each primitive is registered in a process-global table
// against an id (u32, fits in EXT).  A fresh PRI carries val=0;
// APP-PRI accumulates args into a heap cell `[NUM(count), arg_0, ...]`
// until `count == arity`, at which point the C function is called
// and its return Term replaces the redex.  Used in 8.1c-d to build
// SUP-encoded CP enumeration with unification as a primitive.
typedef Term (*PrimFn)(Term *args);
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
int          kvar_extent_is_var(u32 packed_extent);
u32          kvar_extent_var_id(u32 packed_extent);
u32          kvar_extent_static(u32 packed_extent);
u32          kvar_extent_runtime(u32 packed_extent);
u32          kvar_collect_from_dag(Term root, u32 *out_ids, u32 cap);
u32          kernel_kvar_value(struct KernelEntry const *ke, u32 var_id);

// === schedule ===
// bufferize_classify walks the UOp DAG rooted at `root`, marks
// kernel boundaries (root + multi-consumer + REDUCE), runs the named
// realize-rewrite rules and then the unified rangeify pass.
// UOpInfo / BUFFERIZE_NODES is the dense per-walked-UOp table that
// the classifier populates and materialize.c walks.
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
fn int  bufferize_reads_no_buffer(u64 loc, u8 op);

// Per-node consumer-list pool. Each producer node's UOpInfo.cmap_head
// indexes into CMAP_LL; chain via .next. Populated by the bufferize
// walker (same pass that increments consumer_count). Tinygrad parity:
// the `cmap` (Dict[UOp, List[UOp]]) in tinygrad/schedule/indexing.py:155-160.
// Cap sized for ~4x BUFFERIZE_NODES_CAP avg fanout.
#define CMAP_LL_CAP (BUFFERIZE_NODES_CAP * 4)
typedef struct { u64 consumer_loc; u32 next; } CMapNode;
extern CMapNode CMAP_LL[CMAP_LL_CAP];
extern u32      CMAP_LL_LEN;

// Why bufferize_classify realizes a node (UOpInfo.reasons bits).
#define BUFFERIZE_REASON_ROOT        (1u << 0)
#define BUFFERIZE_REASON_MULTI       (1u << 1)
#define BUFFERIZE_REASON_REDUCE      (1u << 2)
#define BUFFERIZE_REASON_FANIN_CAP   (1u << 3)
#define BUFFERIZE_REASON_INLINE      (1u << 4)
// Maxpool-input pre-realize (ROUTE A): the activation a REDUCE_MAX reduces must
// be a materialized buffer so the forward window-max and the backward argmax
// mask (CMPEQ) read ONE buffer and ties are bit-exact (else count 0 -> RECIP(0)
// NaN in the /count tie-split).  This is a CORRECTNESS realize, so the faithful
// seed honors it too (not just the heuristic MULTI seed).
#define BUFFERIZE_REASON_MAXPOOL_INPUT (1u << 5)
#define BUFFERIZE_REASON_MATMUL      (1u << 6)
// Maxpool backward mask pre-realize (ROUTE A, part 2): the argmax mask
// CMPEQ(a, lift(MAX(a))) and its mask_norm (= mask * RECIP(count)) must
// materialize in their OWN kernel so the windowed /count tie-split divide does
// not fuse into the downstream conv-weight backward SUM-reduce.  Fused into an
// N=1 weight-grad reduce, the walker's nested-reduce-iter mis-reads the count
// over the size-1 batch axis -> the mask matches nothing -> zero grad (a
// BatchNorm between relu and maxpool makes the activation a longer elementwise
// chain than the bare relu case).  Like MAXPOOL_INPUT this is a CORRECTNESS
// realize, so the faithful seed honors it too (not just the heuristic MULTI
// seed, which faithful drops).
#define BUFFERIZE_REASON_MAXPOOL_MASK (1u << 7)
// Bit 8 is unassigned.
// fp8 weight -> bf16 cast force-realized to feed a matmul (see
// bufferize_fp8_cast_feeds_matmul).  Without realizing the cast the scalar
// matmul re-decodes each fp8 weight element M times in its inner reduce loop
// (5x slower).  But the realized bf16 IS a single-use transient -- its only
// consumer is the matmul reduce in the SAME realize -- so it must be FREED at
// end-of-realize.  On Metal it gets a legacy (non-arena) alloc whose kloc+0
// output cell mark_heap_rooted_preserve would otherwise pin forever (the
// 13MB-per-matmul leak: 24GB base + per-step growth).  This reason flags the
// boundary so emit records its tid in FP8_TRANSIENT_TIDS and end-of-realize
// frees it after the consuming matmul has fired.
#define BUFFERIZE_REASON_FP8_TRANSIENT (1u << 9)
// A storage operand of an indexed op (GATHER slot 0, SCATTER slots 1 and 2)
// is read through an address the op builds itself (ru_index_leaf), so it
// must be a leaf the address reaches through views alone
// (uop_indexed_operand_is_structural) or a buffer.  This reason realizes
// the compute source under such an operand's view chain; the faithful
// seed honors it and rangeify reads the buffer through it.
#define BUFFERIZE_REASON_INDEXED_SRC (1u << 10)
// Every SCATTER is its own kernel: a serial read-modify-write of its
// output (rangeify_unified.c ru_lower_scatter).  The faithful seed honors it.
#define BUFFERIZE_REASON_SCATTER (1u << 11)

// Leaf utilities other compilation units (realize_classify,
// gc_mark, wnf/redex, interact/uop_kernel) reference.
fn u8   uop_arity(u8 op);
fn u8   uop_is_unary_elementwise(u8 op);
fn u8   uop_is_binary_elementwise(u8 op);
fn u8   uop_is_ternary_elementwise(u8 op);
fn int  term_shape_in(Term t, u32 env_id, Shape *out);
fn int  term_dtype_in(Term t, u32 env_id, u32 *out);

// applied_opts facade.  External linkage (no `fn`) so backend_metal.o
// can call these.
u32        tile_anno_applied_opts_count(struct KernelEntry const *ke);
KOpt const *tile_anno_applied_opts(struct KernelEntry const *ke);
// Recognize the im2col-fused Conv2D reduce template produced by the
// lowered UOp graph.  Renderers use this as a tile template instead
// of carrying backend-private conv pattern matchers.
int     tile_analyze_conv2d_flat(struct KernelEntry const *ke,
                                 TileConv2DInfo *out);
int     tile_rejects_conv2d_flat_cin1(struct KernelEntry const *ke);
// Matmul shape facts flow through uop_dag_classify_matmul_shape
// over ke->cached_lift.store_root; dispatch consumers route through
// the lifter-based path.

// Autotune: walk the proposer's candidates, time each variant
// against the baseline (no opts) with direct kernel dispatch, expand
// the best variants into short opt sequences when enabled, pick the
// winner, and leave KpSchedule mutated to the winning sequence.
// Returns 1 if a winning opt sequence was applied, 0 if baseline won.
// Propose tinygrad-style opt candidates for `kid`, beam-search them by
// timing each variant on FRESH isolated scratch buffers (tinygrad
// codegen/opt/search.py _time_program + postrange.py bufs_from_ast --
// never the live TENS graph buffers), apply + disk-cache the winner.
// Benching on isolated buffers is what lets the search run in a shared
// context (the FLUX qwen/DiT/VAE captures) without perturbing the live
// activations or the tile-JIT shape table.  Returns 1 if a winner was
// applied.  No-op while a JIT capture is recording (jit_is_capturing).
fn int kernel_autotune(u32 kid);

// Cheap predicate used by the fire-time auto-tune trigger.  True
// iff (env opt-in `AUTOTUNE=1` or `BEAM>0`) AND (this KpSchedule
// hasn't been autotuned yet) AND (the proposer has at least one
// candidate for the bare kernel).
fn int kernel_should_autotune(struct KernelEntry *ke);

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

// GC-invariant structural content hash of the lifted UOp DAG rooted at
// `root` (Merkle-style: mix(op, dtype, payload, child hashes), memoized
// over the DAG).  Used to key the Metal PSO / tile-JIT cache by kernel
// CONTENT instead of the raw heap location of store_root, which GC
// relocates mid-session.  Returns 0 for an empty root.  External
// linkage so the separately-compiled Metal backend (.m) can call it.
u64  uop_dag_content_hash(Term root);

// Time `n_runs` back-to-back fires of `kid`; return min wallclock
// us.  Used by autotune internally + exposed via LibraryLink for
// the WL TKernelVariants surface.
fn u64 kernel_bench_us(u32 kid, u32 n_runs, u64 early_stop);

// === tensor/ ===
// Tensor descriptor lifecycle.  Step 12: bump-only allocation in TENS[];
// refcount + backend-level buffer refcount govern the buffer lifetime.
fn u32  tensor_alloc  (Backend *b, Shape shape, u32 dtype);
fn void tensor_release(u32 id);   // decref + buf_decref; free at 0
fn u32  tensor_view_of(u32 src_id, View new_view);  // alias; bumps buf_incref
fn void kernel_assign_write_record(void *backend, u32 buf_id);   // a host write, for the fire memo

// === view/ ===

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
fn void materialized_loc_clear        (void);

// Metal buffer pool hooks.  Real implementations live in
// src/backend/metal/_.m; the C stub supplies no-op variants on
// non-Metal builds so schedule code can call them unconditionally.
u32  thvm_metal_buf_pool_begin(void);
void thvm_metal_buf_pool_rollback_with_preserve(u32 wm);
void thvm_metal_buf_mark_preserved(u32 buf_id);
// GPU-side zero-fill of a buffer's full extent (blit fillBuffer), ordered
// before the next dispatch by Metal's hazard tracking.  Used by the JIT replay
// loop to clear a recycled (replay_packed) output slot before a partial-write
// kernel fires.  No-op stub on non-Metal builds.
void thvm_metal_buf_gpu_zero(u32 buf_id);
void thvm_metal_buf_clear_preserved(u32 wm);
u64  thvm_metal_buf_free_unpreserved_all(void);
// One-shot decref of a single-use transient buffer (fp8->bf16 cast consumed by
// a matmul this realize).  No-op stub on non-Metal builds.
void thvm_metal_buf_decref_one(u32 buf_id);
// Current Metal device live-buffer bytes (0 on non-Metal builds).  Drives the
// realize-boundary device-byte GC trigger (thvm_realize_maybe_gc).
u64  thvm_metal_live_bytes(void);
// Cold-start lever: parallel-compile every Metal-routed tile kernel in
// [kid_lo, kid_hi) whose PSO isn't already cached, BEFORE the serial fire
// loop dispatches them one-at-a-time (mirrors cpu_jit_precompile_range).
// The MSL->AIR + pipeline-state compiles run concurrently via dispatch_apply;
// the populated PSO cache makes the subsequent dispatch all cache hits.
// Idempotent + hash-keyed.  No-op stub on non-Metal builds.
// Knob THVM_METAL_JIT_PARALLEL=0 reverts to the lazy per-dispatch compile.
void thvm_metal_precompile_range(u32 kid_lo, u32 kid_hi);
u32  thvm_metal_buf_count(void);
void thvm_metal_buf_get(u32 i, u64 *nbytes_out, u32 *refcount_out);
// Zero-copy wrap of a page-aligned host region (a disk-mmap weight's DiskMap
// base) as a borrowed MTLBuffer, registered in METAL_BUFS.  `minor` is the
// within-region byte offset of the wrapped tensor's element 0 (the weight
// starts `minor` bytes past the page-aligned base); it is stored as the
// slot's byte_offset and applied at every kernel input bind so the kernel's
// contiguous index 0 lands on the weight.  Returns the buf_id, or 0 on
// failure (caller falls back to the staged-upload copy path).  The slot is
// borrowed: teardown drops only the MTLBuffer ARC ref, never the host mmap
// pages, and the recycle freelist never reclaims it.
u32  thvm_metal_buf_wrap_external(void *page_base, u64 maplen, u64 minor);
// Per-realize ARENA planner (schedule/materialize.c) Metal handles.  The
// planner allocates ONE device buffer for a pass's working set, then hands
// out per-boundary views of it (tinygrad: arena[offset:offset+nbytes]
// .bitcast(dtype), a SHRINK/BITCAST pair since Ops.SLICE was removed in
// #17492); non-overlapping lifetimes share bytes.  Recycling a slice is correct under the ICB because Apple hazard-
// tracks accesses that alias within one MTLBuffer (a separate-MTLBuffer
// recycle is not ordered by the per-command [cmd setBarrier] on Apple9).
// Mirror of the CUDA arena helpers (backend/cuda/buf_alloc.c) and tinygrad
// ops_metal.py:192 (_offset) + schedule/memory.py:55-57.
//
// thvm_metal_buf_arena_alloc: allocate the shared arena buffer of `nbytes`,
// marked skip_freelist (freed wholesale at end-of-realize).  Returns 0 on
// failure.
u32  thvm_metal_buf_arena_alloc(u64 nbytes);
// thvm_metal_buf_arena_view: a new buf_id whose .buf is arena_buf_id's
// MTLBuffer at byte_offset `offset`, length `nbytes`, lifetime tied to the
// arena (owns_data == 0; increfs the parent).  Zeroes the [offset,
// offset+nbytes) window before returning.  Returns 0 on failure.
u32  thvm_metal_buf_arena_view(u32 arena_buf_id, u64 offset, u64 nbytes);
// thvm_metal_buf_arena_release: drop the arena allocation's producer ref at
// end-of-realize; the buffer frees once its last view releases.
void thvm_metal_buf_arena_release(u32 arena_buf_id);
// True iff buf_id is a borrowed (newBufferWithBytesNoCopy) disk-mmap wrap.
int  thvm_metal_buf_is_borrowed(u32 buf_id);
// Packed {borrowed:bit0, jit_pinned:bit1, owns_data:bit2, preserved:bit3,
// refcount:bits8+} for the warm-replay NaN bisect.
u32  thvm_metal_buf_meta(u32 buf_id);
// Explicitly free a borrowed disk-mmap wrap (flushes dispatch, drops the
// MTLBuffer ARC ref; the mmap pages stay owned by the CPU-side DiskMap).
// No-op on a non-borrowed slot.  The streaming per-block loader calls this
// (via thvm_kernel_input_drop_wrap) to release a block's weight wraps after
// the block's matmuls retire so wraps don't accumulate across blocks.
void thvm_metal_buf_free_borrowed(u32 buf_id);

// === uop/ ===
// Constructors for raw UOp graph nodes.  Each helper allocates the
// heap cells its opcode's #define describes and returns a TAG_UOP term.
// They do NOT reduce or fire kernels -- they just build graph
// structure.  Materialization (commit 3) consumes a graph built
// from these.
fn Term uop_const  (u32 dtype, u32 bits);
// REDUCE-node accessors (centralised so the heap layout can evolve
// without touching every call site).  Caller passes a TAG_UOP cell with
// op==UOP_REDUCE.
fn u32  uop_reduce_kind   (Term red);
fn u32  uop_reduce_n_axes (Term red);
fn u32  uop_reduce_axis   (Term red, u32 i);   // i in [0..n_axes)
fn Term uop_reduce_src    (Term red);

// === Symbolic INDEX layer ===
// Constructors for UOP_RANGE / UOP_INDEX_E / UOP_I* / UOP_IWHERE / UOP_INVALID.
// Hash-cons via uop_mov_cache like the movement opcodes.
fn Term uop_int_binary(u32 opcode, Term a, Term b);              // IADD/ISUB/IMUL/IDIV/IMOD/ILT/IAND/IOR/IXOR/ISHR
fn Term uop_fast_idiv_rule(Term t, void *user);                  // late: x//c -> (x*m)>>s, x%c -> x-c*(x//c)
fn Term uop_iwhere   (Term cond, Term then_v, Term else_v);

// === Movement-op range swizzlers ===
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
// tinygrad's split_reduceop (src/schedule/split_reduceop.c), run before
// bufferize_classify; split_reduceop_keeps names the first REDUCEs it made.
fn void split_reduceop                         (Term root);
fn int  split_reduceop_keeps                   (u64 loc);
// THVM_FUSE_MATMUL_INPUT gate (default OFF): fuse a single-consumer elementwise
// producer into its consuming matmul's reduce/A-staging load.  Read by both the
// scheduler (rangeify_unified.c) and the codegen (render_uop.c).
fn int  ru_fuse_matmul_input_on                (void);
// THVM_FUSE_MATMUL_EPILOGUE gate (default OFF): un-realize a single-consumer
// OPT_TC matmul reduce so its consuming elementwise chain fuses into the matmul
// kernel's STORE (the epilogue).  Read by both the scheduler (rangeify_unified.c,
// remove_bufferize) and the recogniser (recognise_tc.c, which must wrap the
// nested REDUCE in OPT_TC even though it is no longer the bare store value).
fn int  ru_fuse_epilogue_on                    (void);
fn void pm_apply_rangeify                      (Term root);
// Main-heap UOP_STORE Term at this node's realize boundary (0 if not
// a boundary or if dtype inference declined).  Structurally equivalent
// to `cached_lift.store_root` from kernel_lift_to_uop; serves as the
// substrate for the lifter-bypass cutover.
fn Term rangeify_unified_store_root_at         (u32 node_idx);
// === UOP_RANGE field accessors ===
// Wrap the [NUM(axis_id), NUM(axis_type), NUM(extent)] heap layout;
// 0 on a tag mismatch.
fn u32  uop_range_axis_id  (Term r);
fn u32  uop_range_extent   (Term r);

// 1 iff REDUCE `red` owns `inner_aid`, the inner (UNROLL/UPCAST) half of
// one of its axes split by uop_dag_apply_split: REDUCE.axes names only the
// outer half, so renderers and the CPU walker fold the inner half into
// exactly the reduce this returns 1 for.  Defined in uop/apply_opt_dag.c.
fn int uop_reduce_owns_split_axis(Term red, u32 inner_aid);

// === Buffer leaf ===

// Variant with `instance` disambiguator.  Two UOP_BUFFER terms with
// the same (scope, dtype, ndim, dims) but different instance fields
// hash-cons to distinct Terms.  Used to keep kernel-arg buffers
// distinguishable when slots happen to share shape.
fn Term uop_buffer_inst(u32 scope, u32 dtype, u32 ndim, const u32 *dims,
                        u32 instance);

// Read accessors for UOP_BUFFER fields (returns 0 on tag mismatch).
fn u32  uop_buffer_scope(Term t);
fn u32  uop_buffer_dtype(Term t);
fn u32  uop_buffer_ndim(Term t);
fn u32  uop_buffer_dim(Term t, u32 d);
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
// Per-axis (axis_id, coeff) pairs of an address, used by the contraction
// classifiers (uop/dag_scan.c) and by hand_coded_optimizations' stride
// heuristic for UPCAST axis selection (codegen/hand_opts.c).  Entries past
// n_axes are not initialised.
#define UOP_DAG_ADDR_MAX_AXES 8
typedef struct {
  u32 axis_ids[UOP_DAG_ADDR_MAX_AXES];
  u32 coeffs  [UOP_DAG_ADDR_MAX_AXES];
  u32 n_axes;
  i32 offset;
  int ok;
} UopDagAddrCoeffsView;
void uop_dag_decode_addr_coeffs(Term addr, UopDagAddrCoeffsView *out);
u32  uop_dag_addr_coeff_lookup (UopDagAddrCoeffsView const *a, u32 axis_id);
// The distinct INDEX_E and STORE addresses under `root`, tinygrad's bufs.
u32  uop_dag_collect_buf_addrs (Term root, Term *out_addrs, u32 cap);
// 1 iff `root` holds a CAST to a 16- or 8-bit float.
fn int uop_dag_has_narrow_cast(Term root);

// Should this kernel get the hand-coded opt heuristic on its next
// fire?  True iff the env opt-in is on (default ON; HAND_CODED_OPTS=0
// or NOOPT=1 disables) AND the per-shape autotuned flag is still 0.
fn int kernel_should_hand_code_opts(struct KernelEntry const *ke);
// Non-static export of the first-fire opt decision for the separately
// compiled Metal TU (backend/metal/_.m).  Applies kernel_hand_coded_opts
// (idempotent) and returns 1 if the kernel is eligible for parallel
// precompile, 0 if it must defer to the lazy fire-time compile (autotune).
int thvm_kernel_apply_first_fire_opts(struct KernelEntry *ke);

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

// TRUE batched matmul: A{batch,M,K} . B{batch,K,N} -> C{batch,M,N}, the batch
// axis present in A, B, AND the output (each batch an independent gemm) --
// distinct from the conv-contraction's A-shared / M-side batch.  Operands may
// be PERMUTED views (the attention heads), so ld/trans/batch-stride are read
// straight from the per-axis address coefficients (no canonical-layout check).
// *_span = per-batch element span (max addressed index + 1) for buffer-size
// validation.  Routed to a loop of cblas_sgemm by blas_try_batched_gemm.
typedef struct {
  u32 dtype;
  u32 a_input, b_input;
  u32 M, N, K, batch;
  u32 ldA, ldB, ldC;
  u32 transA, transB;
  u32 batch_stride_a, batch_stride_b, batch_stride_c;
  u32 a_span, b_span, c_span;
} UopDagBatchedGemmShape;

int uop_dag_classify_batched_gemm(Term root,
                                  struct KernelEntry const *ke,
                                  UopDagBatchedGemmShape *out);

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
// uop_dag_decode_addr_coeffs to distribute IMUL over IADD so the
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

// === Store + After ===
// UOP_STORE writes `value` to `buf` at symbolic `addr`.  T.copy maps to
// `STORE(dst, addr, INDEX_E(src, addr))`.  Hash-cons by (buf, addr, value).
fn Term uop_store(Term buf, Term addr, Term value);

// A kernel root is one STORE or an AFTER chain of STOREs (a kernel that
// writes its output in ordered passes).  uop_arity(AFTER) is 0, so a pass
// that walks by arity must go through these to see every store:
// uop_root_map_stores rebuilds the root with `pass` applied to each store (0
// if any call returns 0), uop_root_each_store ORs `pass` over all of them,
// and uop_root_store_buf is the buffer they write.  A non-AFTER root is its
// own single store.
typedef Term (*UopStoreMapFn) (Term store, void *ctx);
typedef int  (*UopStorePredFn)(Term store, void *ctx);

// === Opt annotation ===

fn Term uop_opt_target(Term t);   // 0 on tag mismatch
fn u32  uop_opt_kind  (Term t);

// Detection-only: returns 1 if `root` is a matmul-shape STORE
// (REDUCE-of-MUL on two distinct INDEX_E buffers, SUM kind). Fills
// *out_k_extent with the reduce-axis extent if statically known
// (zero otherwise). Used by uop_recognise_tc to decide whether to
// wrap with UOP_OPT(_, TC) so render_uop's simdgroup_matrix template
// fires (when K%8==0) versus falling back to its generic accumulator.
fn int uop_classify_matmul(Term root, u32 *out_k_extent, u32 *out_unit_axis);

// Extract the M-axis (non-reduce range in operand A's address) and the
// N-axis (non-reduce range in operand B's address) of a matmul-shape
// STORE, plus their extents.  Returns 1 + fills the out params on a
// matmul match; 0 otherwise.  See src/uop/recognise_tc.c.
fn int uop_matmul_mn_axes(Term root, u32 *out_m_axis, u32 *out_m_extent,
                          u32 *out_n_axis, u32 *out_n_extent);

// Producer-side parallel-TC wrap: when the matmul shape qualifies for
// the simdgroup_matrix template (K, M, N all multiples of 8), wraps the
// REDUCE with OPT(_, TC, 0) AND re-stamps the M and N RANGE leaves
// KAX_GLOBAL so render_uop emits the parallel (one-simdgroup-per-tile)
// body and cg_tile_metal_dispatch_shape launches a tile grid.  Falls
// back to the plain (guarded) uop_recognise_tc wrap for ragged shapes.
fn Term uop_recognise_tc_parallel(Term root);

// TRUE batched-matmul (batch axis in A, B AND output) structural
// classifier + producer-side parallel-TC wrap.  uop_classify_batched_matmul
// fills the batch/M/N axis ids + extents and K extent on a 3-range-per-operand
// match (the 2-D recogniser above rejects this shape); 0 otherwise.
// uop_recognise_batched_tc_parallel wraps the REDUCE with OPT(_, TC, 0) and
// stamps batch/M/N GLOBAL when M,N,K are all %8, so rmu_emit_matmul_tc takes
// the batched parallel branch.  See src/uop/recognise_tc.c.
fn int uop_classify_batched_matmul(Term root,
                                   u32 *out_batch_axis, u32 *out_batch_extent,
                                   u32 *out_m_axis, u32 *out_m_extent,
                                   u32 *out_n_axis, u32 *out_n_extent,
                                   u32 *out_k_extent);
fn Term uop_recognise_batched_tc_parallel(Term root);

// Forward decl: re-stamp every UOP_RANGE leaf with axis_id == `axis_id`
// from KAX_LOOP to KAX_GLOBAL.  Defined in src/uop/apply_opt_dag.c,
// which is included after recognise_tc.c in the unity build, so the
// parallel-TC wrap needs this prototype to call it.
fn Term uop_dag_apply_global(Term root, u32 axis_id);

// === UOp DAG renderer ===

// Structural-mode entry points.  Discover the kernel's buffer slots
// from the DAG itself via UOP_BUFFER.instance (kernel_lift.c sets
// instance=0 on the output and instance=slot+1 on input slot k).
// Production callers (cg_emit_via_uop, cpu_jit_build) pass
// ke->cached_lift.store_root directly; no out_buf/in_bufs[] tuple
// needed.
fn void cg_render_uop_kernel_root(Term root, const char *kernel_name,
                                  FILE *fp);

fn u8   uop_is_movement(u8 op);

typedef Term (*UOpGraphRewriteFn)(Term t, void *user);
typedef struct {
  char const        *name;
  UOpGraphRewriteFn apply;
} UOpGraphRewriteRule;

// Declarative UOp pattern, matched structurally by upat_match
// (uop/upat.c).
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

// upat_match: definition in src/uop/upat.c.  Single-TU unity
// build (no header forward decl needed).
fn Term uop_graph_rewrite(Term root,
                          UOpGraphRewriteRule const *rules,
                          u32 n_rules,
                          void *user);

fn u32  uop_define_var_extent(Term t);

fn int  scatter_identity_bits(u32 kind, u32 dtype, u32 *bits);

// === schedule/ ===
// Top-level materialize driver: heap-walk pass that in-place rewrites
// UOP cells reachable from `term` into UOP_KERNEL cells, propagating
// shapes through APP-LAM bindings.  Called directly from the WL bridge
// (TMaterialize / TRealize).
fn Term thvm_materialize(Term term);

// One-shot materialize + wnf under a per-call buffer-pool boundary
// (src/schedule/realize.c).  The C entry the WL TRealize wraps; also
// callable from other in-tree passes (e.g. the in-engine GNN scorer)
// that need to drive a UOP graph to a realized TAG_TEN and read the
// result buffer back.  thvm_realize_many bundles a TAG_CTR of
// independent roots into one pass, ordering roots that read a buffer
// before roots that overwrite it; it returns 0 when those orderings form
// a cycle, and thvm_realize_error() then says why ("" otherwise).
fn Term thvm_realize(Term expr);
fn Term thvm_realize_many(Term ctr_term);

// === codegen/ + profile/ ===
// UOp-DAG source emitter (render_uop.c) + per-kid dispatch profiling.
// Exposed cross-TU so the Metal .m file (compiled separately under
// -DTHVM_HAS_METAL) can render MSL via cg_emit_metal ->
// cg_emit_tile_metal -> cg_emit_via_uop and record dispatches into
// the shared K_PROFILE table.  The Metal side and the CPU JIT both
// route through render_uop's lifter.

// Per-kid route taken by the most recent fire.  Mirrored by the WL
// surface decoder (TKernelDispatchKind) -- keep the names + ids
// stable across TUs.
typedef enum {
  KDISPATCH_NONE        = 0,
  KDISPATCH_BLAS_GEMM   = 3,   // 1 and 2 are unused
  KDISPATCH_JIT         = 4,   // CPU JIT (clang -shared)
  KDISPATCH_INTERPRETER = 5,   // CPU interpreter
  KDISPATCH_METAL_OP    = 7,   // Metal: per-op shader fallback (DAG-side encoder over cached_lift)
  KDISPATCH_METAL_TILE  = 9,   // Metal: render_uop UOp-DAG -> MSL -> single-encoder dispatch
  KDISPATCH_METAL_MPS   = 10,  // Metal: MPSMatrixMultiplication GEMM (per-op replay only, not ICB-batchable)
                               // reuses the slot vacated by the retired metal_try_gemm (4e30432b)
  KDISPATCH_METAL_ALIAS = 13,  // Metal: metadata-only alias, no command encoding
  KDISPATCH_CUDA_JIT    = 14,  // CUDA: nvrtc-compiled kernel via cuLaunchKernel
  KDISPATCH_CUSTOM      = 15,  // raw injected program (kernel_emit_custom_program)
} KDispatchKind;

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
// True when THVM_KERNEL_PROFILE=N (N != 0) is requesting a per-kernel
// dump.  The Metal backend reads this to route the batched ICB replay
// through the per-op command-buffer path for TRUE per-kernel GPU time.
int   cg_profile_kernel_enabled(void);

// === backend/ ===
// CPU backend -- only backend for step 12.  Installed by thvm_init.
// Metal lands in step 14 behind the same Backend struct.
extern Backend CPU_BACKEND;
extern Backend METAL_BACKEND;
// Install (on first use) + return the backend for device `dev`
// (THVM_DEV_*) in the current context; NULL if unavailable.  See thvm.c.
Backend *ctx_ensure_backend(i32 dev);
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
// Serial-encoder replay: encode a run of captured ops onto ONE
// MTLDispatchTypeSerial compute encoder, one commit + wait.  kinds[i]:
// 0 = tile dispatch, 1 = custom dispatch, 2 = strided gather (out_buf_id =
// dst, in_buf_ids[0] = src, gshells[i] = view shell).  zero_first[i] zeroes
// the out slot on the same encoder before the dispatch.  0 on success (all n
// executed), -1 to decline with nothing committed.
int thvm_metal_serial_run(JitReplayDispatch const *recs, u32 n,
                          u8 const *kinds, u8 const *zero_first,
                          u32 const *groups_x, u32 const *threads_x,
                          TenDesc const *gshells);
// Trace-capture route stamp: classify the dispatch route `ke` would take and
// cg_profile_record it, WITHOUT GPU work.  Lets jit_capture_finalize read the
// correct dispatch kind for ICB eligibility when THVM_JIT_TRACE_CAPTURE skipped
// the eager dispatch.  Returns the stamped KDispatchKind.
int thvm_metal_kernel_route_classify(struct KernelEntry *ke, u32 *in_buf_ids,
                                     u32 out_buf_id);
// Read the process-wide Metal GPU-time accumulator.  *out_total_us =
// summed [cmd GPUEndTime]-[cmd GPUStartTime] microseconds across every
// command-buffer flush/submit; *out_flush_count = number of
// flushes/submits observed (incl. empty ones).  Either pointer may be
// NULL.  Used by the WL TMetalGpuTime[] surface for per-step GPU-time
// measurement.
void thvm_metal_gpu_time(u64 *out_total_us, u64 *out_flush_count);
// Drop every in-memory compiled MTLComputePipelineState (the JIT MSL
// PSO cache, 256 slots).  The BEAM autotune search compiles hundreds
// of throwaway candidate PSOs that overflow the fixed-size cache;
// once full, metal_jit_lookup_idx returns table-full and EVERY later
// dispatch bypasses the cache and recompiles (bypass>0, warm replay
// regresses).  Calling this after a kernel's search clears the
// candidate garbage so the warm replay repopulates with only the
// live (heuristic-or-winner) kernels and never thrashes.
void thvm_metal_jit_drop_in_memory_psos(void);
#endif

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

// === kbo/ ===
// Knuth-Bendix ordering on first-order terms (TAG_CTR + TAG_FVR).
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
// Memo-free comparator: identical verdict to thvm_kbo (the per-term
// weight memo is a pure cache -- THVM_KBO_NO_WCACHE is the long-standing
// A/B witness), but every divergent-subtree accumulation takes the
// single-pass walk, and the shared memo/epoch state is left untouched.
// Cheaper when the operands are small FRESH terms (new cells, weighed
// once): the memo path hashes every cold node just to key the cache.
// The ATP CP-insert weigh is exactly that regime.
fn KboCmp thvm_kbo_nomemo(Term s, Term t, const KboConfig *cfg);
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
// non-CTR/non-FVR head.  Byte-identical verdict vs thvm_kbo.
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
// problems.
typedef enum {
  LPO_EQ =  0,
  LPO_GT =  1,
  LPO_LT = -1,
  LPO_UN =  2,    // incomparable
} LpoCmp;

typedef struct {
  const u32 *precedence;   // higher value = greater symbol; distinct
                           // symbols of equal value are incomparable
  u32        n_labels;
} LpoConfig;

fn LpoCmp thvm_lpo(Term s, Term t, const LpoConfig *cfg);
// thvm_lpo's verdict for an observer, leaving the persistent memo, the
// statistics and the deadline poll as they were.  0 when a side is too
// large for the flat encoding (use thvm_lpo).
fn int    thvm_lpo_quiet(Term s, Term t, const LpoConfig *cfg, LpoCmp *out);

// === rpo/ ===
// Recursive Path Ordering (Dershowitz 1982).  Generalises LPO with
// per-symbol status (LEX or MUL); strict-precedence only, no weights.
// LPO is the all-LEX special case; MPO (multiset path ordering) is the
// all-MUL one.  Used as a third reduction ordering for problems where
// KBO weights or LPO's strict-lex bias rule out useful orientations.
typedef enum {
  RPO_EQ =  0,
  RPO_GT =  1,
  RPO_LT = -1,
  RPO_UN =  2,
} RpoCmp;

typedef enum {
  RPO_STATUS_LEX = 0,
  RPO_STATUS_MUL = 1,
} RpoStatus;

typedef struct {
  const u32       *precedence;   // higher value = greater symbol; distinct
                                 // symbols of equal value are incomparable
  const RpoStatus *status;       // status[label] in {LEX, MUL}; NULL => all-LEX
  u32              n_labels;
} RpoConfig;

fn RpoCmp thvm_rpo(Term s, Term t, const RpoConfig *cfg);
fn void   thvm_rpo_invalidate(void);

// === wpo/ ===
// Weighted Path Ordering (Yamada-Kusakari-Sakabata 2014).  Unifies
// KBO, LPO and RPO: per-symbol weight + precedence + status.  Compare
// strict weight first; on tie, fall back to an RPO-style precedence/
// status comparison.  Variable-multiplicity dominance is enforced both
// directions, as in KBO.  Admissibility (weights > 0 on unary symbols)
// is the caller's problem; the comparator is sound but only
// well-founded under admissible weights.
typedef enum {
  WPO_EQ =  0,
  WPO_GT =  1,
  WPO_LT = -1,
  WPO_UN =  2,
} WpoCmp;

typedef enum {
  WPO_STATUS_LEX = 0,
  WPO_STATUS_MUL = 1,
} WpoStatus;

typedef struct {
  const u32       *weights;    // per-symbol weight (size n_labels)
  const u32       *precedence; // per-symbol precedence (size n_labels)
  const WpoStatus *status;     // per-symbol status; NULL => all-LEX
  u32              n_labels;
  u32              var_weight; // weight contribution per variable occurrence
} WpoConfig;

fn WpoCmp thvm_wpo(Term s, Term t, const WpoConfig *cfg);
fn void   thvm_wpo_invalidate(void);

// === fol/ ===
// First-order clausal reasoning -- generalises the unit-equational
// engine (src/atp/) to full FOL.  A literal is a signed atom (a CTR
// term representing a predicate application or an equation), a clause
// is a disjunction of literals, and the empty clause witnesses
// unsatisfiability.  This is the foundation -- types + binary
// resolution + factoring.  Saturation (subsumption, paramodulation,
// selection) layers on in follow-up modules.
typedef struct {
  Term atom;
  u8   sign;     // 0 = positive, 1 = negative
} FolLit;

// Inference rule that produced a clause -- recorded per-clause for
// proof reconstruction.  INPUT marks a user-supplied clause; the
// rest match the inferences in src/fol.
typedef enum {
  FOL_INF_INPUT     = 0,
  FOL_INF_RESOLVE   = 1,
  FOL_INF_FACTOR    = 2,
  FOL_INF_PARAMOD   = 3,
  FOL_INF_REFLEX    = 4,
  FOL_INF_EQ_FACTOR = 5,
  FOL_INF_DEMOD     = 6,
  // AVATAR-lite component intro: the clause is one variable-disjoint
  // component of parent_a, guarded by the assumption whose index is
  // cguard[id]-1 (see the sat.c splitting section).
  FOL_INF_SPLIT     = 7,
} FolInference;

#define FOL_NO_PARENT 0xFFFFFFFFu

// Forward-demodulation provenance (proof mode, cnf_set_retain_dead): a
// stored clause's generating inference concluded `pre`; `n` unit
// equations (clause ids rule[0..n)) then rewrote it, through the
// intermediate clauses mid[0..n-1), to the stored clause.
typedef struct {
  struct FolClauseS *pre;
  u32                n;
  u32               *rule;
  struct FolClauseS **mid;
} CnfFdRec;

typedef struct FolClauseS {
  FolLit *lits;
  u32     n_lits;
} FolClause;

// Per-clause provenance.  Lives in a parallel array in CnfState
// (one entry per clause id) rather than inside FolClause so the
// inference functions in src/fol/_.c don't need to know about
// saturation-level concepts.
typedef struct {
  FolInference rule;
  u32          parent_a;
  u32          parent_b;
} FolTrace;

fn FolClause *fol_clause_new (u32 n_lits);
fn void       fol_clause_free(FolClause *c);
// Renumber the clause's variables to a dense [0, k), all literals
// sharing one numbering.  Alpha-renaming; keeps stored ids clear of
// the REWRITE_MAX_VAR matcher cliff after repeated rename-apart
// inferences.
fn void       fol_clause_canon_vars(FolClause *c);
fn u8         fol_clause_eq  (const FolClause *a, const FolClause *b);
fn u8         fol_clause_is_empty(const FolClause *c);
fn FolClause *fol_resolve    (const FolClause *c1, u32 i,
                              const FolClause *c2, u32 j);
fn FolClause *fol_factor     (const FolClause *c, u32 i, u32 j);

// Paramodulation: superpose a positive equality literal (s = t) from
// `eq_clause` into a subterm `u` (at `path`) of `target`'s
// `target_idx`-th literal.  `swap` chooses orientation (0: s->t,
// 1: t->s).  Equality literals are atoms with the FOL_LAB_EQ label.
// Returns the paramodulant or NULL.
fn FolClause *fol_paramodulate(const FolClause *eq_clause, u32 eq_idx,
                               u8 swap,
                               const FolClause *target, u32 target_idx,
                               const u32 *path, u32 path_len);

// Reflexivity-resolution (a.k.a. equality resolution): a clause with
// a negative equality ¬(s = t) becomes σ(C \ ¬(s=t)) when s and t
// unify with mgu σ.  Returns NULL when the literal isn't a negative
// equality / atoms don't unify.
fn FolClause *fol_reflex_resolve(const FolClause *c, u32 idx);

// Equality factoring (Vampire-style): given C = (s1 = t1) v (s2 = t2)
// v R and σ unifying s1 with s2, derive σ((s1=t1) v ¬(t1=t2) v R).
// Together with paramodulation + reflex-resolve, this completes the
// equality-inference family for refutation-complete FOL+equality.
fn FolClause *fol_eq_factor(const FolClause *c, u32 i, u32 j);

// Demodulation: rewrite `target`'s literal atoms using the unit
// positive equality from `eq_clause` as an oriented rule `s -> t`.
// Returns a fresh clause if any literal rewrote, NULL otherwise.
// Reuses thvm_match + thvm_subst_apply.  Naïve / non-ordering-aware:
// callers must give a properly-orientable equation (no σ(s) > σ(t)
// check yet).
fn FolClause *fol_demodulate     (const FolClause *eq_clause,
                                  const FolClause *target);
fn Term       fol_demodulate_atom(Term s, Term t, Term atom);

// Is the atom an equality (CTR with the FOL_LAB_EQ label, arity 2)?
fn u8         fol_atom_is_eq(Term atom);

// Clause subsumption: does c1 subsume c2 (i.e. exists σ with
// σ(c1) ⊆ c2 as multisets)?  Variables of c2 are renamed apart so
// they can't be bound by the matcher.  Worst-case |c1|! backtracking
// match; bounded by FOL_MAX_LITS = 64.
fn u8         fol_subsumes(const FolClause *c1, const FolClause *c2);
// Subsumption against pre-frozen target literals (vars already
// renamed apart); freeze once via fol_freeze_lits and test many
// patterns without re-renaming.
fn u8         fol_subsumes_frozen(const FolClause *c1,
                                  const FolLit *frozen, u32 n2);
fn u8         fol_subsumes_frozen_nv(const FolClause *c1,
                                     const FolLit *frozen, u32 n2, u32 nvars);
fn void       fol_freeze_lits(const FolClause *c, FolLit *out);
// Max variable id + 1 across the clause (== distinct-var count for
// canon clauses); the snapshot width for subsumption backtracking.
fn u32        fol_clause_nvars(const FolClause *c);

// Tautology check: a clause is a tautology iff
//   (a) it contains both ±A for some atom A, or
//   (b) it contains a positive equality (s = s).
// Cheap O(n^2) scan.
fn u8         fol_is_tautology(const FolClause *c);

// CnfState declarations follow AtpStatus -- see search for
// "FOL saturation loop" below.

// Invalidate thvm_lpo's persistent (s,t)->verdict memo.  Call when term
// cells move (GC) or the precedence changes (new run).
fn void   thvm_lpo_invalidate(void);
// Opt in to persistent LPO memoization (default off = fresh per call).
// The caller then MUST invalidate on cell movement (the ATP does, on GC).
fn void   thvm_lpo_set_persist(u8 on);

// Install a zero-arg wall-deadline hook polled inside the LPO recursion,
// so a single huge-term comparison self-aborts once the wall has passed
// (returns LPO_UN and unwinds) instead of overrunning --wall.  NULL
// disarms.  Only perturbs verdicts AFTER the deadline, so a pre-wall
// comparison is unchanged.  The ATP engine installs its wall poll here.
fn void   thvm_lpo_set_deadline_hook(int (*hook)(void));

// Diagnostic counters for lpo_flat_rec / compute / memo (the AndAssoc
// faithful-port profile shows lpo_flat_rec_compute is 41% of CPU).
fn void thvm_lpo_flat_stats(u64 *rec_calls, u64 *memo_hits,
                            u64 *compute_calls, u64 *top_calls);
fn void thvm_lpo_flat_stats_reset(void);
#ifdef THVM_LPO_SELFCHECK
// -DTHVM_LPO_SELFCHECK: every flat compare is also run through the
// reference flat recursion; prints pairs, mismatches and timings.
fn void thvm_lpo_selfcheck_report(FILE *f);
#endif

// === rewrite/ ===
// One-shot equational rewriter on TAG_CTR + TAG_FVR.  No recursive descent into
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
extern const char *g_thvm_phase;
fn Term thvm_rewrite_step     (Term t, const Term *lhs, const Term *rhs,
                               u32 n_rules);
fn Term thvm_rewrite_normalize(Term t, const Term *lhs, const Term *rhs,
                               u32 n_rules, u32 step_cap);

// === unify/ ===
// Most-general-unifier on TAG_CTR + TAG_FVR terms (Robinson with
// occurs check).
// Reuses RewriteSubst for the result; caller zero-inits before the
// first call.  Returns 1 on success, 0 if the mgu doesn't exist.
fn u8   thvm_unify        (Term s, Term t, RewriteSubst *subst);
// Bound-id undo log: a hot caller keeps one persistent all-zero
// RewriteSubst and brackets each unify attempt with undo_begin/undo
// instead of re-zeroing the 512-byte struct per attempt.  While armed,
// thvm_unify logs every id it binds; thvm_unify_undo re-zeroes exactly
// those slots (full clear on log overflow).  Byte-identical semantics
// to a fresh zero-init.
fn void thvm_unify_undo_begin(void);
fn void thvm_unify_undo   (RewriteSubst *subst);
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
// 64: Waldmeister enumerates EVERY non-variable position with no depth
// cap; 16 silently skipped 7835 deep-overlap positions on the
// WolframAxioms DoubleNegation run (250 missing CPs in batch-637 alone
// -- the @1288 alignment fork vs wmcli -auto).  The pos[] arrays are
// transient per-CP buffers, so the cost is bytes, not asymptotics.
// The position walks recurse once per path step (cp_walk_positions,
// cp_replace_at, ft_walk_positions, ft_replace_at_span): this cap is
// also their C-stack bound.
#define CP_MAX_DEPTH 64
typedef struct {
  Term lhs;
  Term rhs;
  Term peak;            // sigma(li): the overlapped term both CP sides descend from
  u8   pos[CP_MAX_DEPTH];
  u8   pos_len;
  // Face-combo index of the overlap that produced this CP (0..3 per
  // atp_overlap_ij: 0/1 = outer rule i is the Vater [overlapped] face,
  // 2/3 = i's reversed face is the Vater).  0xff = unknown (a CP from a
  // path that does not tag combos).  The CP-formation side geometry
  // (use_cp_side) reads this to decide whether thvm's stored
  // orientation already matches WM's KPLinks = sigma(r_Vater) face or
  // needs the physical swap -- a per-combo distinction, not a blanket
  // reorder.  Threaded onto the TRACE_CP record so it survives to the
  // orient site (atp_wmo_cp_combo reads it back at pop time).
  u8   combo;
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

// Proper-position variant of the above: the root of `li` is not a
// superposition site (WM eTT enumeration; see the definition for the
// root-overlap ownership discipline between the saturator's (i, j)
// and (j, i) visits of one fact pair).
fn u32 thvm_critical_pairs_pair_noroot(Term li, Term ri, Term lj, Term rj,
                                       CriticalPair *out, u32 cap, u32 count);

// Diagnostic counters for the CP enumeration caps:
//   *out_dropped       -- per-pair buffer cap-hits (ctx->count >= ctx->cap)
//   *out_depth_capped  -- positions stranded under a CTR at CP_MAX_DEPTH
// thvm_cp_caps_reset() zeros both before a fresh measurement.
fn void thvm_cp_caps_reset(void);
fn void thvm_cp_caps_get(u64 *out_dropped, u64 *out_depth_capped);

// === atp/ ===
// Saturation loop state.  AtpState is heap-
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

// === FOL saturation loop (Otter-style given-clause) =================
// CnfState holds the clause store + active/passive split.  Each
// cnf_step pops a passive clause, moves it to active, generates all
// cross-inferences with the prior active set + self-inferences,
// then pushes derived clauses through tautology + subsumption
// filters before queuing.  See src/fol/sat.c.
// Selection-function policies for the saturation loop.  When set on
// CnfState, cross-resolution / paramodulation only fire on a
// clause's SELECTED literal, dramatically pruning the inference
// space.  Falls back to "all literals" when the selected policy
// finds no matching literal (preserves completeness in the trivial
// sense for Horn-like clauses without the target polarity).
typedef enum {
  CNF_SELECT_NONE        = 0,   // try every literal (default behavior)
  CNF_SELECT_NEGATIVE    = 1,   // first negative literal, else all
  CNF_SELECT_POSITIVE    = 2,   // first positive literal, else all
  CNF_SELECT_NEG_LARGEST = 3,   // largest negative literal (symbol
                                // count; ties keep the first), else all
} CnfSelection;

// Literal-index bucket: the (atom root label, sign) key's entry list.
// Entries are packed u64s -- the resolution index stores
// (clause_id << 8 | literal_idx); the subsumption indexes store bare
// clause ids.  Buckets live in an open-addressing power-of-two hash
// table (FolIdx); NULLed clause slots are skipped lazily at retrieval.
typedef struct {
  u64  key;    // 1 + ((label << 1) | sign); 0 = empty slot
  u64 *ents;
  u32  n, cap;
} FolIdxBucket;

typedef struct {
  FolIdxBucket *slots;
  u32           cap;      // power of two; 0 = not yet allocated
  u32           n_used;
} FolIdx;

// AVATAR guard-set bitset: one bit per propositional guard.  A fixed
// POD struct (assigned/copied by value) so per-clause assumption sets
// stay inline in the parallel arrays; all set algebra goes through
// the gm_* helpers.
#define CNF_AVATAR_MAX_GUARDS 256u
#define CNF_GMASK_WORDS (CNF_AVATAR_MAX_GUARDS / 64u)
typedef struct { u64 w[CNF_GMASK_WORDS]; } CnfGmask;

fn void cnf_gm_zero(CnfGmask *m) {
  for (u32 i = 0; i < CNF_GMASK_WORDS; i++) m->w[i] = 0ull;
}
fn void cnf_gm_fill(CnfGmask *m) {
  for (u32 i = 0; i < CNF_GMASK_WORDS; i++) m->w[i] = ~0ull;
}
fn u8 cnf_gm_is_zero(const CnfGmask *m) {
  u64 acc = 0ull;
  for (u32 i = 0; i < CNF_GMASK_WORDS; i++) acc |= m->w[i];
  return (acc == 0ull) ? 1u : 0u;
}
fn u8 cnf_gm_eq(const CnfGmask *a, const CnfGmask *b) {
  for (u32 i = 0; i < CNF_GMASK_WORDS; i++)
    if (a->w[i] != b->w[i]) return 0u;
  return 1u;
}
fn void cnf_gm_or(CnfGmask *a, const CnfGmask *b) {   // a |= b
  for (u32 i = 0; i < CNF_GMASK_WORDS; i++) a->w[i] |= b->w[i];
}
fn void cnf_gm_set(CnfGmask *m, u32 g) {
  m->w[g >> 6] |= 1ull << (g & 63u);
}
fn u8 cnf_gm_test(const CnfGmask *m, u32 g) {
  return (u8)((m->w[g >> 6] >> (g & 63u)) & 1ull);
}
fn u8 cnf_gm_subset(const CnfGmask *a, const CnfGmask *b) {  // a is a subset of b
  u64 acc = 0ull;
  for (u32 i = 0; i < CNF_GMASK_WORDS; i++) acc |= a->w[i] & ~b->w[i];
  return (acc == 0ull) ? 1u : 0u;
}
fn u8 cnf_gm_intersects(const CnfGmask *a, const CnfGmask *b) {
  u64 acc = 0ull;
  for (u32 i = 0; i < CNF_GMASK_WORDS; i++) acc |= a->w[i] & b->w[i];
  return (acc != 0ull) ? 1u : 0u;
}
fn u32 cnf_gm_pop(const CnfGmask *m) {
  u32 n = 0u;
  for (u32 i = 0; i < CNF_GMASK_WORDS; i++)
    n += (u32)__builtin_popcountll(m->w[i]);
  return n;
}
fn u32 cnf_gm_first(const CnfGmask *m) {   // lowest set bit; ~0u when empty
  for (u32 i = 0; i < CNF_GMASK_WORDS; i++)
    if (m->w[i] != 0ull)
      return (i << 6) + (u32)__builtin_ctzll(m->w[i]);
  return ~0u;
}

typedef struct {
  FolClause **clauses;
  u32         n;
  u32         cap;
  u32        *active;
  u32         n_active;
  u32         cap_active;
  u32        *passive;
  u32         n_passive;
  u32         cap_passive;
  u32         passive_head;   // FIFO (age-lane) dequeue index
  // Pick-best given-clause selection: a binary min-heap of passive
  // clause ids keyed by (symbol-count weight, id).  The append-only
  // passive[] log doubles as the age lane -- every age_ratio-th pick
  // takes the oldest surviving passive clause instead (fairness /
  // completeness).  A clause picked from either lane is stamped in
  // cpicked[] and skipped lazily by the other.
  u32        *pheap;
  u32         n_pheap;
  u32         cap_pheap;
  u32        *cweight;        // per-clause symbol-count weight (parallel to clauses)
  u32        *cpri;           // pick-best heap key: weight, plus a large
                              // penalty for clauses with NO conjecture
                              // ancestry -- goal descendants always pop
                              // first (their cone is small; the FIFO age
                              // lane keeps everything else fair)
  u64        *cmask;          // per-clause (root label, sign) bloom mask --
                              // subsumption prefilter: a can subsume c only
                              // if cmask[a] & ~cmask[c] == 0
  u8         *cpicked;        // 1 once taken as the given clause
  // Per-clause inference-eligibility cache (lazy; invalidated by the
  // cnf_set_* config calls): selected literal index (or ~0u) and the
  // bitmask of ordering-maximal literals.  Both are static per clause
  // for a fixed ordering + selection config, so the O(lits^2) KBO
  // scan runs once per clause instead of once per candidate pair.
  u32        *csel;
  u64        *cmaxlits;
  u8         *celig_valid;
  // Goal-direction: 1 when the clause descends from a conjecture
  // (negated-conjecture inputs marked by the caller; inference
  // conclusions inherit from either parent).  Goal-related clauses
  // get a pick-best weight bonus -- the coarse analog of E's
  // conjecture-symbol weighting.
  u8         *cgoal;
  // s2a (Vampire sine_to_age): per-clause SInE level (max over the
  // clause's symbols of s2a_sym_slvl; a goal-unreachable symbol
  // contributes ATP_S2A_REMOTE_LVL).  Stamped at add when s2a_coeff > 0;
  // cnf_pop_passive's age lane then orders live passive clauses by
  // effective age = insertion index + s2a_coeff*csine, postponing goal-
  // distant clauses.  NULL/0 (off) is the byte-identical default.
  u16        *csine;
  // s2a per-symbol level table (label-indexed), copied by
  // cnf_set_sine_levels from the CLI's D-relation over the input clauses.
  // NULL when s2a is off.  Freed by cnf_free.
  u8         *s2a_sym_slvl;
  u32         s2a_n_labels;
  u32         s2a_coeff;
  // Frozen (rename-apart) literal copies, built once at store time --
  // the reusable TARGET side of subsumption.  malloc'd FolLit arrays,
  // freed by cnf_free.
  FolLit    **cfrozen;
  // Per-clause variable-slot count (fol_clause_nvars), the snapshot
  // width for subsumption backtracking.
  u32        *cnvars;
  u32         pick_count;
  u32         age_ratio;      // K: every K-th pick is FIFO (1 = pure FIFO,
                              // 0 = pure best-first); THVM_FOL_AGE_RATIO
  u32         goal_tier;      // 1 (default): cpri adds the non-goal penalty
                              // so the conjecture cone pops first; 0 makes
                              // pick-best a flat weight order over ALL
                              // clauses; THVM_FOL_GOAL_TIER
  // Active unit positive equalities (the demod rule list): ids append
  // when such a clause moves to active; slots NULLed by backward
  // subsumption are skipped lazily.
  u32        *ueq;
  u32         n_ueq;
  u32         cap_ueq;
  // ueq_lhs[k]/ueq_rhs[k] cache ueq[k]'s equation renamed apart by
  // FOL_RENAME_OFFSET, filled ONCE when ueq[k] is appended (sat.c
  // cnf_step's "register on the demod rule list") -- every later
  // forward-demod match attempt against this rule reuses the cached
  // pair instead of re-running thvm_rename_vars (a whole-tree
  // walk + allocation) on every attempt.  Index-parallel to ueq[],
  // grown in lockstep.
  Term       *ueq_lhs;
  Term       *ueq_rhs;
  // Root-label index over ueq_lhs[]: (LHS root label) -> ueq-slot
  // index, so cnf_forward_demod's rewrite walk retrieves ONLY the
  // candidate rules whose LHS root could possibly match a given
  // subterm position instead of scanning all of ueq[] there.  A rule
  // whose LHS is a bare variable (matches any subterm shape
  // unconditionally -- see sat.c cnf_demod_index_insert) has no root
  // label to key on; it lives on ueq_wc[] instead, always scanned.
  // Both populated at the same append point as ueq[] itself; neither
  // needs a rebuild-on-config-change path (unlike idx_res), since a
  // rule's LHS root label is a structural fact of the clause, not a
  // function of the ordering/selection config.  A bucket-insert
  // failure degrades to the wildcard list rather than disabling
  // anything wholesale: demodulation is a redundancy simplifier, not
  // an inference generator, so a missed index entry costs efficiency,
  // never soundness or refutational completeness.
  FolIdx      idx_demod;
  u32        *ueq_wc;
  u32         n_ueq_wc, cap_ueq_wc;
  // ueq_lhs_mask[k]: a 64-bit bloom mask of every label appearing
  // ANYWHERE in ueq_lhs[k] (all levels, not just the root), cached
  // once at the same append point.  The root-label bucket above is a
  // one-level filter -- on a clause set dominated by a few shallow
  // wrapper labels (e.g. an ifeq/eq encoding, where nearly every
  // literal and nearly every rule shares the SAME outermost symbol),
  // a bucket can still hold most of ueq[].  The full-depth mask is a
  // NECESSARY, cheap (one AND + compare) refinement WITHIN a bucket:
  // substitution only ever fills in variable positions, so every
  // concrete symbol the pattern demands must already be present
  // somewhere in the subject, at any depth -- see sat.c
  // cnf_demod_find_at's bottom-up subtree mask.  False positives
  // (hash collisions) just fall through to the real thvm_match; there
  // are no false negatives, so this never changes which rule wins,
  // only how many provably-impossible attempts get skipped before it.
  u64        *ueq_lhs_mask;
  // Deferred-free queue for backward subsumption: when a derived
  // clause subsumes an older one, the slot in clauses[] is NULLed
  // immediately but the FolClause* gets pushed here and freed at
  // end-of-step.  Avoids dangling pointers in mid-step inference
  // loops that captured the clause pointer at entry.
  FolClause **deferred_free;
  u32         n_deferred;
  u32         cap_deferred;
  u32         step;
  u32         step_cap;
  AtpStatus   status;
  // Optional reduction ordering for demodulation.  When set, demod
  // only applies a rewrite `s -> t` at a position where σ(s) > σ(t)
  // under the configured ordering -- the standard well-foundedness
  // gate.  When all four are NULL, demod falls back to naïve
  // unconditional application (the CNF_DEMOD_BUDGET cap protects).
  // Priority WPO > RPO > LPO > KBO when multiple are set.
  const KboConfig *cnf_kbo;
  const LpoConfig *cnf_lpo;
  const RpoConfig *cnf_rpo;
  const WpoConfig *cnf_wpo;
  // Selection function: opt-in literal-pick policy.  Default
  // CNF_SELECT_NONE = current "try every literal" behavior.
  CnfSelection cnf_select;
  // Limited-resource strategy (cnf_set_lrs / THVM_FOL_LRS): passive
  // clauses heavier than lrs_wcap are discarded lazily at pop -- they
  // are unreachable before the wall at the observed pick rate
  // (cnf_lrs_tick recomputes the watermark from the caller's
  // remaining-pick estimate).  Discarding only LOSES refutations,
  // never invents one, so soundness is untouched; but a drained queue
  // after any drop is a resource artifact, not a saturation --
  // cnf_step reports ATP_ABORTED instead of ATP_QUEUE_EMPTY then.
  u8  lrs_on;
  u32 lrs_wcap;        // ~0u = no cap yet
  u32 lrs_dropped;
  // Set of support (THVM_FOL_SOS): generating inferences require at
  // least one conjecture-descended (cgoal) parent; self-inferences a
  // cgoal clause.  The Vampire sos=on restriction -- axiom-axiom
  // saturation is skipped entirely.
  u8  sos_on;
  // Non-goal weight coefficient (THVM_FOL_NWC, fixed-point x10):
  // when nonzero the pick-best key for a non-goal clause is
  // w * nwc10 / 10 (Vampire's multiplicative nwc) instead of the
  // legacy additive CNF_NONGOAL_PENALTY tier.
  u32 nwc10;
  // Per-clause inference trace, parallel to `clauses[]`.  Each
  // entry records the inference rule that produced the clause and
  // up to two parent ids.  INPUT marks user-added clauses; ~0u
  // (FOL_NO_PARENT) is the no-parent sentinel.
  FolTrace *trace;
  u32       trace_cap;
  // Proof-output mode (cnf_set_retain_dead): clauses removed by
  // backward subsumption / backward demodulation move to corpse[]
  // (keyed by id, parallel to clauses[]) instead of being freed, so
  // a refutation DAG walk can still print every cited parent's
  // literals.  Off by default -- zero overhead outside --proof runs.
  u8          retain_dead;
  FolClause **corpse;
  // fdrec[id]: the forward-demodulation steps behind stored clause id
  // (NULL when none fired); fd_pend collects the steps of the clause
  // cnf_consider is processing until the store attaches them.
  CnfFdRec  **fdrec;
  u32         fdrec_cap;
  CnfFdRec    fd_pend;
  FolClause  *fd_pend_final;
  // --- literal indexing (per-(root label, sign) partner lists) ------
  // Replaces the O(active) resolution cross-scan and the O(n)
  // subsumption walks with bucket retrievals; candidate SETS and the
  // visit ORDER are reproduced exactly, so the clause-store evolution
  // is byte-identical to the full scans (see sat.c "literal index").
  // THVM_FOL_INDEX=0 disables (the A/B lever); any index allocation
  // failure also clears idx_on -- the full scans need no index.
  u8          idx_on;
  u8          mask_fine;     // cmask family: argument-fingerprint bits
                             // (latched at init with idx_on; never
                             // flips mid-run so stored masks stay one
                             // family)
  u8          idx_res_valid; // active-side index tracks the ordering/
                             // selection config; cnf_set_* clears it
                             // and cnf_step rebuilds lazily
  FolIdx      idx_res;       // ACTIVE eligible literals: (label, sign)
                             //   -> (clause_id << 8 | lit_idx)
  FolIdx      idx_fsub;      // ALL stored: first-literal FINE key
                             //   (label, sign, arg0 fingerprint) -> id
                             //   (forward-subsumption candidates)
  FolIdx      idx_bsub;      // ALL stored: every distinct coarse
                             //   (label, sign) key -> id (backward-
                             //   subsumption, VAR-arg0 patterns)
  FolIdx      idx_bsub_f;    // ALL stored: every distinct FINE key
                             //   -> id (backward, CTR-arg0 patterns)
  u32        *idx_peq;       // ACTIVE paramod sources (eligible positive
  u32         n_peq, cap_peq;//  equality, selection not negative)
  u32        *idx_fb;        // ACTIVE unindexable clauses (an eligible
  u32         n_fb, cap_fb;  //  non-CTR atom / lit_idx > 255): always
                             //  visited, like the full scan would
  u32        *idx_fsub_wc;   // ALL stored with non-CTR/absent first
  u32         n_fsw, cap_fsw;//  literal (forward wildcard patterns)
  u32        *cact;          // activation ordinal (position in active[])
  u8         *cpeq;          // per-clause paramod-source flag (active)
  u8         *cfbl;          // per-clause fallback flag (active)
  u32        *cstamp;        // candidate stamp, epoch-keyed
  u32         idx_epoch;
  // --- AVATAR-lite component splitting (cnf_set_avatar / THVM_FOL_AVATAR)
  // A stored clause whose literals partition into >= 2 variable-
  // disjoint components is replaced by one clause per component, each
  // guarded by a fresh propositional assumption; inference conclusions
  // union their parents' assumption sets, an empty clause under
  // assumptions A records the SAT clause ~(/\A), and the run closes
  // (PROVED) when the split clauses + refutation clauses are
  // propositionally unsatisfiable.  See sat.c "AVATAR-lite" for the
  // semantics, soundness argument, and divergence from full AVATAR.
  u8          avatar_on;
  CnfGmask   *cassume;       // per-clause assumption-guard bitset (the
                             //   parents' union; {g} for a component)
  u32        *cguard;        // component clauses: guard index + 1 (else 0)
  u32         n_guards;      // guards allocated (<= CNF_AVATAR_MAX_GUARDS)
  u32         guard_comp[CNF_AVATAR_MAX_GUARDS];  // guard -> component id
  CnfGmask   *split_pos;     // SAT split clauses ~A -> \/ a_g:
  CnfGmask   *split_neg;     //   pos = component guards, neg = parent A
  u32        *split_orig;    //   the split clause's id (proof citation)
  u32         n_split, cap_split;
  CnfGmask   *refut_mask;    // SAT refutation clauses ~(/\A) -- one per
  u32        *refut_id;      //   conditional empty clause (its id)
  u32         n_refut, cap_refut;
  // SAT-model-guided activation (full AVATAR's focusing discipline):
  // model = the DPLL model's TRUE-guard bitset (unassigned and fresh
  // guards TRUE; all-ones = the keep-all fallback).  A clause is
  // ACTIVE iff its assumption set is all-TRUE under the model.
  // Inactive clauses are parked (skipped as given without being
  // consumed, skipped as inference partners) and re-enter the pick
  // lanes when a model change flips their guards -- a previously
  // GIVEN clause re-enters passive and redoes its given-step, so the
  // partner pairs skipped while its branch was off are regenerated
  // (the re-given discipline; completeness).  cstale[id] = 1 while id
  // sits in parked[] (the once-per-deactivation dedup bit).
  CnfGmask    model;
  u32        *parked;
  u32         n_parked, cap_parked;
  u8         *cstale;
  u32         n_models;      // model moves (diagnostic)
} CnfState;

fn CnfState *cnf_init        (u32 step_cap);
fn void      cnf_free        (CnfState *s);
fn i32       cnf_add_clause  (CnfState *s, FolClause *c);  // takes ownership
// As cnf_add_clause, additionally marking the input goal-related
// (conjecture-descended) for the pick-best weight bonus.
fn i32       cnf_add_clause_goal(CnfState *s, FolClause *c, u8 goal_related);
fn AtpStatus cnf_step        (CnfState *s);
fn AtpStatus cnf_run         (CnfState *s);

// Attach a reduction ordering for ordering-aware demodulation.
// When set, demod gates each rewrite `s -> t` on σ(s) > σ(t).
// When all four are NULL, demod fires unconditionally.  Setting
// more than one is allowed; the dispatch priority is WPO > RPO >
// LPO > KBO.
fn void      cnf_set_kbo     (CnfState *s, const KboConfig *kbo);
fn void      cnf_set_lpo     (CnfState *s, const LpoConfig *lpo);
fn void      cnf_set_rpo     (CnfState *s, const RpoConfig *rpo);
fn void      cnf_set_wpo     (CnfState *s, const WpoConfig *wpo);

// Arm (seconds>0) or disarm (<=0) a single-step wall self-abort: the hot
// loops inside cnf_step (backward subsumption, the per-given conclusion
// flood) poll it so one long step trips ATP_ABORTED instead of overrunning
// the caller's between-steps wall poll.  File-global; disarmed by default.
fn void      cnf_set_wall_deadline(double seconds_from_now);
// The same self-abort against a work budget on atp_work_now_us's clock
// (seconds of work from now; <=0 disarms): trips at the same point of
// the search under any load.  Both may be armed; either fires.
fn void      cnf_set_work_deadline(double seconds_from_now);

// Attach a selection function (default NONE).  Affects which literal
// of a clause is allowed to participate in resolution + paramodulation.
fn void      cnf_set_select  (CnfState *s, CnfSelection sel);
fn void      cnf_set_lrs     (CnfState *s, u8 on);
fn void      cnf_lrs_tick    (CnfState *s, u32 remaining_picks);

// AVATAR-lite component splitting on/off (overrides the THVM_FOL_AVATAR
// default read at cnf_init).  Call before any clause is added.
fn void      cnf_set_avatar  (CnfState *s, u8 on);

// Partition a clause's literals into variable-disjoint components (two
// literals connect iff they share a variable; ground literals are
// singleton components).  Fills comp_of[i] (when non-NULL, size
// n_lits) with dense component labels in first-occurrence order and
// returns the component count.  Unsplittable shapes (>= 64 distinct
// variables) report 1.
fn u32       fol_components  (const FolClause *c, u8 *comp_of);

// Retain backward-subsumed / backward-demodulated clauses in a
// corpse[] array (keyed by clause id) instead of freeing them, so a
// proof emitter can print every parent a trace entry cites.  Call
// once after cnf_init, before any clause is added.
fn void      cnf_set_retain_dead(CnfState *s);
// The clause for `id`, live or retained-dead; NULL when unavailable.
fn const FolClause *cnf_clause_at(const CnfState *s, u32 id);
// Forward-demodulation steps behind stored clause `id`, or NULL.
fn const CnfFdRec  *cnf_fdrec_at(const CnfState *s, u32 id);

// Walk the provenance DAG rooted at `root_id` (typically the empty
// clause's id when status == ATP_PROVED) and write a text proof
// to `out`.  Format: each line is
//   c<id>: <rule> [c<parent_a>[, c<parent_b>]]
// followed by the clause's literal list.  Cycles cannot occur (every
// derived clause's parents have strictly smaller ids).
fn void      cnf_print_proof (const CnfState *s, void *out, u32 root_id);

// === FOL formula -> CNF pipeline ====================================
// Formulas are Term trees with reserved CTR labels for connectives.
// Predicate / function symbols use labels < FOL_LAB_NOT; equality
// follows the existing FOL_LAB_EQ = 0 convention.
fn u8   fol_is_connective(u32 label);
fn Term fol_mk_not(Term p);
fn Term fol_mk_and(Term p, Term q);
fn Term fol_mk_or (Term p, Term q);
fn Term fol_mk_imp(Term p, Term q);
fn Term fol_mk_iff(Term p, Term q);
fn Term fol_mk_all(Term var, Term body);
fn Term fol_mk_ex (Term var, Term body);

// Negation normal form: pushes ¬ down to atoms; eliminates -> and <->.
// Atoms (non-connective CTRs + FVRs) are returned wrapped in ¬ iff
// the outer rewriting demanded a negation.  Quantifier-preserving.
fn Term fol_nnf(Term f);

// Skolemization: replaces every `∃y` with sk_n(x1..xk), where x1..xk
// are the enclosing ∀-bound vars (past REWRITE_MAX_ARITY of them, only
// those the ∃-body mentions).  Drops the surviving `∀`
// quantifiers; output is a quantifier-free formula whose free
// variables are implicitly universal.  Skolem function symbols sit
// in the [FOL_LAB_SKOLEM_BASE, FOL_LAB_NOT) reserved range.
//
// Caller must run `fol_nnf` first (the rewriter handles -> / <->
// defensively, but soundness relies on the input being NNF).  Reset
// the Skolem counter between independent runs via fol_reset_skolem.
fn Term fol_skolemize    (Term f);
fn void fol_reset_skolem (void);
fn u8   fol_is_skolem    (u32 label);
// 1 once a Skolem or Tseitin label family ran out since its reset: the
// clausification produced since then must be discarded.
fn u8   fol_labels_exhausted(void);

// Distribute ∨ over ∧ to produce a conjunction-of-disjunctions.
// Caller must run fol_nnf and fol_skolemize first.  Worst-case
// exponential in nested-OR depth (the standard CNF blowup);
// Tseitin-style structural CNF is a follow-up if a real bench hits
// the blowup.
fn Term fol_distribute(Term f);

// Walk a CNF-shaped Term (AND-of-OR-of-literals) and extract one
// FolClause per top-level disjunct.  Returns a heap-allocated
// FolClause** of size *n_out; caller owns the array AND each clause.
fn FolClause **fol_extract_clauses(Term cnf_formula, u32 *n_out);

// End-to-end: f -> NNF -> Skolem -> distribute -> extract.  Resets
// the Skolem counter so each call is independent.  Caller owns the
// returned array + every clause.
fn FolClause **fol_formula_to_clauses(Term f, u32 *n_out);

// Tseitin structural CNF: produces a linear-size clausal encoding
// by introducing fresh auxiliary predicates for each compound
// subformula.  For deeply-nested ∨/∧, avoids the exponential blowup
// of the naïve distribute path -- e.g. Pelletier P12 (iff
// associativity) drops from 16k+ saturation steps to a handful.
//
// Aux predicates live at `FOL_LAB_TSEITIN_BASE + counter` and carry
// the subformula's free vars as arguments.  Reset the counter via
// fol_reset_tseitin before each independent run.
fn FolClause **fol_tseitin_extract_clauses(Term nnf_skolemized, u32 *n_out);
// Move the Tseitin family to `floor` and the Skolem family 0x10000 above
// it (for a signature whose labels reach past 0x10000); 0 = no room.
fn u8          fol_set_label_floor(u32 floor);
fn FolClause **fol_formula_to_clauses_tseitin(Term f, u32 *n_out);
fn void        fol_reset_tseitin(void);
fn u8          fol_is_tseitin_aux(u32 label);
// The pipeline's walkers keep their work on heap stacks (any formula
// depth).  A caller bounding clausification by time installs a hook the
// walkers call once per 2^16 work items; the hook abandons the
// clausification (longjmp, never returns) once its deadline passed.
// NULL removes it.
fn void        fol_set_poll(void (*hook)(void));
// Nesting depth of a formula or term (a leaf is 1), counted up to `cap`:
// returns min(depth, cap).
fn u32         fol_depth(Term t, u32 cap);

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

// Sentinel for "no successful normalize witness yet" in the per-CP
// cp_last_norm_r_revision cookie consulted by atp_cp_set_interreduce.
// Compared with the live r_revision (always < UINT32_MAX in practice),
// so a fresh / mutated slot's cookie never spuriously enables the
// fast-path skip.  See AtpState.cp_last_norm_r_revision / r_revision.
#define ATP_CP_NORM_COOKIE_NONE 0xFFFFFFFFu

// Per-rule-slot ground-joinability status (AtpState.r_gj_status) -- the
// thvm image of WM's GZ_statusT (general.h:94-95).  STICKY statuses
// (never retested): JOINABLE (the fact is sterile -- WM
// Grundzusammenfuehrbar + the `!Faktum->Grundzusammenfuehrbar` guard,
// Hauptkomponenten.c:268/:288), VALUABLE (A/C/extended-C shape, WM
// GZ_wertvoll via PROTECT_3_PERMS), HOPELESS (<= 1 distinct variable
// or both sides ground, WM GZ_aussichtslos -- the backward walk skips
// `gzfbStatus <= GZ_aussichtslos`, Grundzusammenfuehrung.c:824).
// FAILED facts are retested on every new fact (WM retests them
// witness-gated; thvm skips the witness cache -- pure perf).
#define ATP_GJ_ST_UNKNOWN   0u
#define ATP_GJ_ST_JOINABLE  1u
#define ATP_GJ_ST_VALUABLE  2u
#define ATP_GJ_ST_HOPELESS  3u
#define ATP_GJ_ST_FAILED    4u
// Sentinel for "no victim excluded" in the GJ driver
// (AtpState.gj_exclude): any value >= n_rules works.
#define ATP_GJ_NO_EXCLUDE   0xFFFFFFFFu

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
// TRACE_FVI: Waldmeister's `RechtsUnfreiErzeugen` "free-variable
// instance" event (RUndEVerwaltung.c:366-397).  When the precedence is
// total and an equation is unorientable under KBO/LPO, WM also
// generates a grounded instance of the equation by substituting the
// reserved minimal constant for every variable in the RHS that does
// not occur on the LHS.  The grounded instance is order-decidable and
// drives completion past the unorientable stall (gates ExcludedMiddle,
// Noncontradiction, EqualityOfInverses).  Recorded as a separate
// reason so the proof renderer can label these steps "FreeVarInstance".
// Children: same layout as TRACE_ORIENT.
#define TRACE_FVI      6u
#define ATP_TRACE_NONE 0xFFFFFFFFu

// Rule-set journal event (AtpState.rj; src/atp/rule_journal.c).
// w = kind << 29 | orient << 28 | rule uid; nt = n_trace when logged.
//   BIRTH    a slot appended to R (tid unused)
//   CONTENT  the uid's content is trace record tid, orientation orient
//   DEATH    the uid left R
//   VIEW     record nt - 1 was computed on the rule set at position tid
//   SLICE    an interreduction pass reduces with the tid rules whose
//            uids start at uid (tid 0: the pass ended)
typedef struct { u32 w; u32 tid; u32 nt; } AtpRjEv;
#define ATP_RJ_BIRTH    0u
#define ATP_RJ_CONTENT  1u
#define ATP_RJ_DEATH    2u
#define ATP_RJ_VIEW     3u
#define ATP_RJ_SLICE    4u
#define ATP_RJ_UID_MASK 0x0fffffffu

// CTR label for the synthetic `Cp(lhs, rhs)` head used by the FV
// subsumption index to span both sides of a queued CP in a single
// term.  Distinct from the TRACE_* labels so a Cp head is never
// confused with a trace entry.
#define ATP_CP_LABEL    16u  // Cp[lhs, rhs]

// === Reserved CTR labels (engine-introduced constants) ==============
//
// Waldmeister's signature analysis pre-reserves `SO_const1` /
// `SO_const2` at the top of the symbol table (SymbolOperationen.c:386-
// 389) so the FVI rewrite has a constant to substitute for free
// variables when generating a grounded instance, and ranks both below
// every signature symbol (NeueFunktionenEinbauen, "install new
// functions", :210-216).  thvm reserves the SAME role at the BOTTOM of
// the user-label range (labels 1 and 2); the WL encoder pre-seeds its
// symbol table with these so user labels start at 3, and the C engine
// materializes the 0-arity terms in `thvm_atp_init`.  In the .pr layout
// the reserved labels take precedence 1 and 2 and every user symbol a
// value from 3 up; `atp_precedence_floor_reserved` restores that after a
// precedence scheme ranks the user labels from 1 (the Vampire ports,
// --precedence) or leaves the reserved labels at the unseen rank 0 (the
// frequency ports).
#define ATP_RESERVED_LABEL_MIN_CONST 1u   // SO_minimaleKonstante
#define ATP_RESERVED_LABEL_MIN_CONST2 2u  // SO_const2 (reserved, unused)
#define ATP_RESERVED_LABEL_FIRST     1u
#define ATP_RESERVED_LABEL_LAST      2u
#define ATP_RESERVED_LABEL_COUNT     2u

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
//     application (one PRI per rule); combined with APP-SUP fan-out it lets
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
                              // the cp_seq age tie-break (oldest-first
                              // within a bucket) dominates the heap
                              // selection.
  ATP_CP_WEIGHT_WMQ = 14,    // Waldmeister -auto (Automodus/StdS) CP measure:
                              // weight = S*S + S - 1, S = total symbol count of
                              // the CP (symcount(l)+symcount(r)).  DERIVED exactly
                              // from wmcli -a4 verbose weights on WolframAxioms/
                              // OrAssociativity (S -> w: 14->209, 26->701,
                              // 30->929; constant 2nd difference => quadratic
                              // S^2+S-1).  Depends ONLY on total size S, not the
                              // l/r split, so structurally-close CPs share a
                              // weight bucket and the cp_seq FIFO age tie-break
                              // decides -- matching WM's best-first.  Mix's
                              // split-aware g factor over-distinguishes same-S
                              // CPs (OrAssoc rule-52 compound vs collapser: both
                              // S=30/w=929 in WM, but 1109 vs 701 under Mix,
                              // which is why thvm picks the collapser at rule 52
                              // where WM picks the older compound).
                              // SUPERSEDED by WMQ2: WMQ is only exact for
                              // COLLAPSERS (min side = 1); it mis-weights every
                              // split CP.  See WMQ2.
  ATP_CP_WEIGHT_WMQ2 = 15,   // Waldmeister -auto CP measure, DERIVED from the
                              // full wmcli emission trace (wm3.txt, 3780 CP/
                              // weight pairs, WolframAxioms/OrAssociativity):
                              // weight = b*b + 2*b + a*(b+1), a = min side
                              // symbol count, b = max side symbol count.  Fits
                              // 96.5% of WM's real selection weights exactly
                              // (WMQ's S*S+S-1 fit only 23%: it is the a==1
                              // special case of this).  The 131 large-CP misses
                              // fit totalS*(totalS+2) (a second regime).  This
                              // is WM's genuine split-AWARE measure; it should
                              // reproduce WM's selection order far better than
                              // WMQ, which only ties collapsers.
  ATP_CP_WEIGHT_LAST = 16,
} AtpCpWeightMode;

// Deferred-CP (`implicit_pair`) arc: lightweight per-CP descriptor that
// replaces the fully unified packed byte string `cp_packed[i]` carries.
// Waldmeister stores its passive set as implicit term pairs and
// materializes only on treatment (KPVerwaltung.c / TPR_TP2ParIntermed);
// thvm's slots are TRACE-BACKED instead of re-unified -- the raw
// var-normalized unified terms already live unconditionally in the
// TRACE_CP entry at `cp_trace[i]` (a GC root set), so materialization at
// selection reads them back for the pop-time normalize and no parent
// re-unification is needed.  Compared to a typical 30-200B packed byte
// string (plus malloc-header overhead), this 20-byte flat struct is a
// 4-10x shrink and zero heap allocations on the push path.  Storage: a
// parallel array `cp_implicit[]` sized like `cp_packed[]`, gated by the
// per-slot `cp_is_implicit` bitset (set iff slot i uses `cp_implicit[i]`
// AND `cp_packed[i] == NULL`).  Lazily allocated on first push when
// `use_implicit_cp` is on; stays NULL when the flag is off so
// `atp_ensure_cp_cap`'s realloc loop is byte-identical.
typedef struct {
  union {
    struct {               // trace-backed lane (use_implicit_cp)
      u32 parent_a_trace_id;   // index into s->trace[] of parent rule A's TRACE_ORIENT/TRACE_FVI entry (dead-parent check without a trace walk)
      u32 parent_b_trace_id;   // index into s->trace[] of parent rule B; both are real rules (axiom/initial CPs stay on the eager packed path)
      i32 overlap_position;    // reserved; 0 under the trace-backed scheme (the overlap path lives in the TRACE_CP entry)
    };
    struct {               // raw-descriptor lane (use_raw_desc / use_raw_desc_cert):
                           // WM-style re-derivation of the raw >= 50 KPBehandelt
                           // class from its parents at pop.  The face refs index
                           // s->rdesc_faces, whose span pointers are content-frozen
                           // for the state's lifetime (rule drops move mirrors to
                           // the dead-save slots; RHS composes abandon the old span
                           // in Arena A -- nothing ever recycles a mirror cell), so
                           // the descriptor survives every parent mutation.
      u32 out_face;            // rdesc face ref: outer (lo, ro) as passed to the walk former
      u32 in_face;             // rdesc face ref: inner source (lin, rin), pre-rename
      u32 sub_pre;             // preorder cell index of the overlap subterm within lo
    };
  };
  u32 weight;              // desc lane: cp_nodes at push; CERT mode: the eager packed byte length (memcmp bound)
  u32 priority;            // same shape as s->cp_pri[i] (read back by the priority rebuild for implicit slots)
} AtpCpImplicit;
_Static_assert(sizeof(AtpCpImplicit) == 20, "AtpCpImplicit must stay 20 bytes");

// Raw-descriptor face-pair table entry: one (lo, ro) span-pointer pair
// as the walk former consumed it (outer face possibly side-swapped for
// an unorientable rule's reverse-face overlap; inner face pre-rename).
// Interned once per distinct pair; entries are never freed -- the spans
// they alias outlive every descriptor (see AtpCpImplicit).
typedef struct AtpRdescFace {
  const struct AtpFtCell *lo;
  const struct AtpFtCell *ro;
} AtpRdescFace;

// CP-heap node: one heap POSITION of the CP priority queue.  `row`
// names the stationary payload row (the phase-1 indirection); pri/seq/
// aux are CACHES of the sift-hot comparator keys so one compare touches
// ONE cache line instead of taking a dependent random payload load per
// key (cp_heap[pos] -> cp_pri[row] / cp_seq[row] / cp_ultimate[row]).
//
// Refresh invariant: at every point where a comparator (atp_cp_before /
// atp_kdh_gt) can run,
//   pri == cp_pri[row]
//   seq == cp_seq[row]
//   aux == (cp_ultimate == NULL ? 0 : cp_ultimate[row])
// for every live position.  Every site that writes cp_pri / cp_seq /
// cp_ultimate for a QUEUED row restamps the caches via atp_cp_heap_set
// (see each mutation site); row moves (atp_cp_row_move callers) rewire
// only `row` -- the move copies the keys verbatim, so the caches stay
// exact.  The mode-gated extra comparator keys (cp_goal / cp_pri2 /
// cp_packed rhs-node counts, all read only under non-default modes)
// stay row loads -- they are NOT cached here; 16 bytes is the cap.
typedef struct {
  u32 pri;   // cache of cp_pri[row]
  u32 seq;   // cache of cp_seq[row]
  u32 row;   // stationary payload row at this heap position
  u32 aux;   // cache of cp_ultimate[row] (0 while cp_ultimate == NULL)
} AtpCpHeapNode;
_Static_assert(sizeof(AtpCpHeapNode) == 16, "AtpCpHeapNode must stay 16 bytes");

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
  // Waldmeister `SO_minimaleKonstante` (SymbolOperationen.c:121): the
  // lowest-precedence 0-arity CTR reserved by the WL encoder at label
  // ATP_RESERVED_LABEL_MIN_CONST (see thvm.h's ATP_RESERVED_LABEL
  // block).  Used by the FVI rule emission in `thvm_atp_orient_and_add`
  // to substitute for free variables in an unorientable equation's
  // RHS that do not occur on its LHS.  Materialized in `thvm_atp_init`
  // as `term_new_ctr(ATP_RESERVED_LABEL_MIN_CONST, NULL, 0)`.
  Term  min_const;
  // FT-skip mode (atp_cp_trivially_joinable) probe cache: highest
  // n_rules value at which the FT mirror was confirmed fully populated
  // (every i in [0, n) has lhs[i]==0 OR lhs_ft[i]!=NULL).  When current
  // n_rules <= this cap, the probe is amortised O(1).
  u32   ft_mirror_probed_n_rules;
  u8    ft_mirror_full;      // 1 if last probe found mirror fully synced

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
  // Per-CP origin class (TRACE_AXIOM / TRACE_CP / TRACE_SIMPLIFY),
  // stamped at push and riding the slot through sifts/backfills exactly
  // like cp_trace.  The proof-trace soft cap turns cp_trace into
  // ATP_TRACE_NONE for late pushes, but the WM-order mirror still needs
  // the popped equation's ORIGIN to place its distinguished face
  // (atp_wmo_eq_dist_rhs): a superposition CP stores WM's (KPLinks,
  // KPRechts) so the distinguished face is the stored LHS, while a
  // SIMPLIFY re-derivation keeps it on the stored RHS.  One byte per CP
  // keeps that decision exact past the cap (WolframAxioms
  // OrAssociativity fact 318: the trace-less registration mis-walked
  // the B/E sweep faces and flipped a weight tie at fact 533).
  u8   *cp_kind;
  u32   n_cps;
  // 7c': the CP queue is a binary min-heap keyed on
  // (cp_pri, cp_seq) -- cp_pri is atp_cp_priority computed once at
  // push, cp_seq is a monotonic insertion counter breaking ties.
  // Selection is O(log n) pop-min; this replaces the old
  // rebuild-an-INC-SUP-tree-every-step O(n) scan.
  u32  *cp_pri;
  u32  *cp_seq;
  // CP FORMATION DEPTH (Twee getDepth): how many overlap generations deep
  // this CP was derived -- max(parent rule depths) + 1.  Twee's CP score
  // adds depthweight*depth (src/Twee/CP.hs), penalizing deeply-derived CPs
  // so completion stays focused on shallow derivations.  Used only by the
  // ATP_CP_WEIGHT_TWEE (mode 8) score in place of the syntactic term depth
  // (which stays ~1-2 for flattened flat-names and gives NO derivation
  // penalty -> thvm explores the deep-derivation space Twee prunes).  The
  // parallel r_depth[] carries the same counter on active rules.
  u32  *cp_depth;
  // GOAL-DERIVED bit (Vampire spb=goal / nwc analog, derivation-level):
  // 1 iff this CP descends from a conjecture-derived input equation --
  // seeded by thvm_atp_mark_gd_next on the goal-cone tgt definitions
  // (and conjecture-role units), then propagated max-parent-wise like
  // cp_depth.  Consumed only when THVM_ATP_SPB_GOAL is armed: the CP
  // priority of NON-goal-derived CPs is scaled by THVM_ATP_NWC
  // (default 2.0), Vampire's nongoal_weight_coefficient applied at the
  // derivation level.  The parallel r_gd[] carries the bit on active
  // rules (a rule inherits its birth CP's bit).
  u8   *cp_gd;
  // THVM_ATP_GJ_KEEP: ground-joinable CPs are KEPT (so they age the FIFO
  // dimension faithfully, as Waldmeister keeps them in its 168k unselected
  // set) but forced to never-weight-selectable priority (WM Act_never) --
  // the weight pick skips them; the FIFO pick reaches them (no-op join).
  // Replaces GroundJoin's drop, which loses the FIFO agers and over-selects
  // a heavier non-ground-joinable CP in their place (the firstdiv-251 bug).
  u8   *cp_deprio;
  // Goal-interleave: per-CP goal-directed weight (CPinGoal), parallel to
  // cp_pri.  Filled at push only when use_goal_interleave > 0; the
  // selection then takes a goal-min pick every use_goal_interleave-th
  // step (E-style ratio: size-based system-building + goal steering).
  u32  *cp_goal;
  // s2a (Vampire sine_to_age): per-CP SInE level (max over the CP's
  // symbols of sym_slvl[]; a goal-unreachable symbol contributes
  // ATP_S2A_REMOTE_LVL).  Filled at push only when s2a_coeff > 0.  The
  // FIFO/age selection then orders by effective age = cp_seq +
  // s2a_coeff*cp_sine, so goal-distant CPs are made younger (postponed)
  // and lose to goal-near ones.  Byte-identical when s2a_coeff == 0.
  u32  *cp_sine;
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
  // Heap-position indirection: the binary/KD heap orders POSITIONS
  // [0, n_cps) while the ~14 payload arrays above are indexed by
  // stationary ROWS.  cp_heap[pos] is a 16-byte AtpCpHeapNode: .row
  // names the row at heap position pos and .pri/.seq/.aux cache that
  // row's sift-hot comparator keys (see the AtpCpHeapNode refresh
  // invariant); cp_pos[row] is the position (invariant:
  // cp_pos[cp_heap[p].row] == p for every p < n_cps, and
  // cp_heap[0..n_cps).row is a permutation of the live rows
  // [0..n_cps)).  A sift swaps only whole nodes + the cp_pos mirror;
  // payload rows move only on pop-backfill / sweep densification
  // (keeping the live rows dense in [0, n_cps)).  Layout-neutral by
  // construction: the cached keys are exact copies of the payload keys
  // at every compare, so every comparator verdict -- hence every sift
  // swap, hence the position-level heap evolution -- is byte-identical
  // to the historical row-load heap.  cp_deprio deliberately stays
  // POSITION-indexed (the historical swap and backfill never moved
  // it); see atp_cp_rebuild_priorities.
  AtpCpHeapNode *cp_heap;
  u32  *cp_pos;

  // Deferred-CP (`implicit_pair`) arc: parallel array of compact
  // 20-byte trace-backed descriptors (parent trace ids + cached
  // weight/priority).  Populated by `atp_cp_implicit_push` for slots
  // tagged in `cp_is_implicit` (one bit per slot, `(cp_cap + 7) / 8`
  // bytes); the tag bit invariant is `cp_is_implicit[i] set <=>
  // cp_implicit[i] live AND cp_packed[i] == NULL`.  Both arrays are
  // lazily allocated on the first push under `use_implicit_cp == 1`;
  // while the flag is off they stay NULL and `atp_ensure_cp_cap`'s
  // realloc loop is byte-identical.  Selection materializes the raw
  // pair off the slot's TRACE_CP entry (atp_cp_implicit_materialize).
  AtpCpImplicit *cp_implicit;     // sized cp_cap, only when allocated
  u8            *cp_is_implicit;  // bitset, (cp_cap + 7) / 8 bytes
  u32            n_cps_implicit;  // diagnostics: CPs pushed as descriptors

  // Simpler-join pop-time peak stash (use_simpler_join != 0): per-slot
  // packed byte string of the CP's formation peak sigma(l_outer) --
  // Twee's overlap `top` -- renumbered through the SAME dense var map
  // as the slot's cp_packed pair, so the checkOrder/step3 legs of the
  // pop/interreduce simpler-join re-checks compare the peak against the
  // popped sides with consistent variable identities (Twee re-derives
  // the overlap at pop; we stash its top at push).  Lazily allocated on
  // the first stash; NULL (zero cost, byte-identical engine) whenever
  // simpler-join is off.  Slot lifetime mirrors cp_packed exactly
  // (row moves, drops, pop, destroy).  A NULL slot degrades that CP's
  // pop re-check to the peakless step1+2 verdict (the pre-stash
  // behaviour, documented in ft_norm.c).
  u8           **cp_peak;         // sized cp_cap, only when allocated
  // Raw-overlap stash (use_otter_raw): per-slot packed byte string of
  // the CP's RAW superposition pair, before the push-time treatment.
  // The Otter replay and the selection normalize start from it.  NULL
  // slot = the queued pair is already raw (the untreated >= 50 class,
  // requeued interreduction victims, input equations).  Lazily
  // allocated; lifetime mirrors cp_packed (atp_cp_stash_clear/_move).
  u8           **cp_raw;          // sized cp_cap, only when allocated

  // Raw-descriptor lane (`use_raw_desc`) face-pair intern table: the
  // walk former's (lo, ro) span pairs, deduplicated so a descriptor
  // carries two u32 refs instead of four pointers.  `rdesc_hash` is an
  // open-addressing map of (lo, ro) -> table index (capacity a power
  // of two, 0xFFFFFFFF = empty).  Lazily allocated on the first
  // descriptor push; NULL while the lane is off.
  AtpRdescFace  *rdesc_faces;
  u32            rdesc_n;
  u32            rdesc_cap;
  u32           *rdesc_hash;
  u32            rdesc_hash_cap;
  u64            n_cps_rawdesc;      // diagnostics: raw CPs stored as descriptors
  u64            n_rawdesc_mat;      // diagnostics: pop-time materializations

  // Per-CP "last successful normalize" cookie for atp_cp_set_interreduce.
  // Holds the `r_revision` value at which slot i was most recently
  // observed to be in normal form (a no-op `atp_rewrite_normalize` on
  // both sides).  On the next IR sweep, if `r_revision ==
  // cp_last_norm_r_revision[i]` the rule set is byte-for-byte unchanged
  // since the witness was recorded (r_revision bumps on EVERY rule add
  // and EVERY rule drop, so a stable counter value uniquely identifies
  // the live rule set -- a plain `n_rules` check is insufficient because
  // a drop+add can leave n_rules unchanged while the rule set differs).
  // The CP is still in NF and we can skip the two `atp_rewrite_normalize`
  // calls (the dominant per-CP cost).  Fresh slots and slots whose CP
  // was rewritten / repacked carry `ATP_CP_NORM_COOKIE_NONE`
  // (UINT32_MAX) -- a value never equal to a real `r_revision`, so the
  // fast-path stays inert until a fixpoint normalize sets the cookie.
  // Sized to cp_cap, swapped with the other parallel arrays.  Always
  // present (zero overhead when the IR sweep is unused).
  u32  *cp_last_norm_r_revision;

  // Monotone "rule-set revision" counter.  Bumped on every rule add
  // (orient_and_add) and every rule drop (thvm_atp_interreduce, both
  // the LHS-rewritten branch and the RHS-rewritten branch).  Two
  // observations of the same r_revision value witness that the rule
  // set `(s->lhs[0..n_rules], s->rhs[0..n_rules])` is bytewise
  // unchanged between the two observations -- the soundness contract
  // the per-CP IR-normalize cookie depends on.  Never decreases, never
  // wraps in any realistic completion run (a u32 takes ~4G mutations
  // to wrap).
  u32   r_revision;
  // In-place right-side rewrites of unorientable equations (the RHS
  // compose of thvm_atp_interreduce outside the WM preset reaches them;
  // r_revision does not count them).  Both faces of such an equation
  // rewrite, so the incremental goal check (goal_incr.c) treats any such
  // edit since its last full normalization as a rule-set change.
  u32   unorient_rhs_edits;

  // Waldmeister history-driven Act_ultimate (NewClassification.c:314):
  // an input axiom is classified with action `ultimate`, forcing its CP
  // weight to minimalWeight() = INT32_MIN so it pops FIRST regardless of
  // the heuristic.  Cuts the "first N pops are heap-resorts of the
  // axiom + cheap descendants" overhead a plain min-weight queue pays
  // -- on a Sheffer/single-axiom problem the first ~10 selections
  // dominate the next 1000 rule trajectory.  cp_ultimate[i] is 1 iff
  // slot i was enqueued as TRACE_AXIOM (or a database lemma) AND
  // use_initial_ultimate was on at insert time.  Ranks ultimate CPs
  // strictly before all non-ultimate ones in atp_cp_before; survives
  // reheapify because the bit lives outside cp_pri.  Swapped/moved
  // with the parallel CP arrays.  Default 0 (engine byte-identical
  // when the flag is off).
  u8   *cp_ultimate;
  // Trace-cap-proof orphan identity (WM Waisenmord parity).  The proof
  // trace soft cap leaves post-cap rules with r_trace == ATP_TRACE_NONE,
  // making atp_trace_mark_dead a no-op and their queued CPs un-orphanable
  // -- WM's KPV_KillParent kills by RULE NUMBER with no cap, so late WM
  // orphans died where thvm's survived (WolframAxioms DoubleNegation
  // firstdiv 796).  r_uid gives every R/E slot a stable birth identity
  // (1-based; 0 = none), cp_uid_a/b stamp each queued CP's parent uids at
  // push, uid_dead is the kill bitmap.  All allocated only under
  // use_orphan_murder (NULL otherwise, slot-move sites NULL-guarded).
  u32  *r_uid;
  // Per-rule FORMATION DEPTH (Twee getDepth): axioms = 0; a rule oriented
  // from a selected CP inherits that CP's cp_depth.  See cp_depth above.
  u32  *r_depth;
  // Per-rule GOAL-DERIVED bit (see cp_gd above): a rule inherits its
  // birth CP's bit; CP formation ORs the two parents' bits.
  u8   *r_gd;
  u32  *cp_uid_a;
  u32  *cp_uid_b;
  u8   *uid_dead;
  u32   uid_dead_cap;
  u32   next_rule_uid;
  // uid -> current rule slot reverse map (0xffffffff = none), maintained
  // at the rule-add site and the three slot-compaction shift loops so the
  // walk former's sw_uid_to_idx is O(1) instead of an O(n_rules) scan per
  // partner hit.  Read side verifies r_uid[idx] == uid and falls back to
  // the scan (then repairs), so a missed maintenance site degrades to the
  // old cost, never to a wrong answer.  THVM_ATP_NO_UIDIDX=1 bypasses the
  // probe (kill switch; the scan is the reference behavior).
  u32  *uid_idx;
  u32   uid_idx_cap;
  u8    use_initial_ultimate;
  // Waldmeister `database=ultimate` action (Parameter.c:166 -- the WM
  // default along with initial=ultimate).  Tags CPs derived during
  // CP-set IR (overlap-derived from already-stored rules) as ultimate
  // so they jump to the heap front.  Creates a depth-first bias on
  // newly-derived chains, which is what lets WM crack hard cases
  // like wolfram commutativity in 2.5s where breadth-first thvm runs
  // 90s+ without closing.  Engine byte-identical when off.
  u8    use_database_ultimate;
  u32   n_cps_ultimate;       // diagnostic: count of axiom-tagged CPs added
  // Waldmeister CP-queue interleaving: selection alternates between
  // the weight key (cp_pri) and the FIFO key (cp_seq, oldest first).
  // cp_select_count is the running selection counter that drives the
  // ratio -- see thvm_atp_select_cp / ATP_CP_FIFO_MODULO.
  u32   cp_select_count;
  // n_activated_re mirrors WM `AnzAktivierterRE`: the count of selected
  // CPs that actually became a rule or equation (passed the joined /
  // perm-subsume / pop-subsume filters and oriented in).  WM's
  // CPdimension() (INF/KPVerwaltung.c:584) keys the FIFO-vs-heuristic
  // dimension switch on THIS counter, not the raw selection count --
  // joined/subsumed pops never advance the schedule.  Drives the FIFO
  // dimension pick so thvm fires it on WM's exact cadence.
  u32   n_activated_re;

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
  // scan, byte-for-byte.  Opaque pointer so the struct layout does
  // not leak the index internals.
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

  // GOAL-NARROW: the ground goal-narrowing endgame (opaque pointer +
  // runtime gate).  A budgeted congruence closure over the ground
  // slice of R -- extended by bounded two-way e-matching over the
  // goal cone -- that closes goals whose joining chain needs
  // EXPANDING (R->L) ground rewrites the ordered goal normalize can
  // never take (the twee/vampire flatten-lane endgame the CASC GRP
  // dissection isolated: GRP725-1/678-1/690-1).  Created lazily on
  // the first gated goal_check; env THVM_ATP_GOAL_NARROW / CLI
  // --goal-narrow / thvm_atp_set_use_goal_narrow.  Default OFF:
  // byte-identical trajectories.  See src/atp/goal_narrow.c.
  struct AtpGoalNarrow *gnarrow;
  u8 use_goal_narrow;

  // NARROW-QUEUE: critical-goal best-first existential narrowing (the
  // WM PM_Existenzziele / GKC query_preference=1 lane; section at the
  // end of src/atp/_.c).  A store of narrowed goal variants
  // ("critical goals") selected lightest-first, each narrowed against
  // every rule face (unorientable equations contribute BOTH faces),
  // with new rules sweeping the already-processed set incrementally
  // (WM RueckwartigeUeberlappung); a CG whose two sides UNIFY closes
  // the conjecture.  Replaces the greedy destructive walk in
  // goal_check's existential branch when armed.  Created lazily on
  // the first gated existential goal_check; env THVM_ATP_NARROW_QUEUE
  // / CLI --narrow-queue / thvm_atp_set_use_narrow_queue.  Default
  // OFF: byte-identical trajectories.
  struct AtpCgq *cgq;
  u8 use_narrow_queue;

  // Narrowing proof recorder (--proof emission support; armed via
  // thvm_atp_set_record_narrow, default OFF = byte-identical
  // trajectories).  An existential goal closes by NARROWING (the
  // greedy walk above or the critical-goal queue), and the closing
  // derivation -- instantiate goal variables, rewrite, repeat -- has
  // no single-NF chain for the proof extractor: every such win
  // emitted the "no single-NF chain extractable" marker (the CASC-J13
  // 15-problem empty-proof class).  When armed, both narrowing lanes
  // record COMPACT step descriptors (trace id of the equation used,
  // side, redex path, face -- no Terms, so no GC rooting and no heap
  // retention), enough for the CLI emitter to REPLAY the derivation
  // from the pristine conjecture pair and print it as an
  // instantiate/rw record chain.  Soundness never rests on the
  // recording: the emitter re-derives every term itself and falls
  // back to the honest marker if any step fails to replay.
  // narrow_rec_active scopes recording to goal_check's greedy loop
  // (thvm_atp_narrow_step is also a public API used on non-goal
  // pairs, which must not pollute the chain).  Layout in narrow.c.
  struct AtpNrec *nrec;
  u8 record_narrow;
  u8 narrow_rec_active;
  // Pristine conjecture pair as set by thvm_atp_set_goal_existential
  // (GC-rooted): the narrowing lanes rewrite the goal slots
  // destructively, but the emitted negated_conjecture record must
  // print the ORIGINAL pair so its file()/provenance citation matches
  // the problem.
  Term goal_orig_lhs, goal_orig_rhs;

  // Refutational goal driver (opt-in; the atp_goal_drive_* lane in
  // src/atp/narrow.c).  Admits the
  // ground negated goal as an ACTIVE participant: a backward
  // condensed-detachment regression over the RAW ifeq-guarded
  // equations, run interleaved with completion (from goal_check) so
  // the live rule set R both bridges connective forms (definitional
  // rewriting, in the goal-side normal form) and discharges the ifeq
  // guards (against completion-derived theorem facts).  The
  // ground-goal complement to the narrow queue: no goal variables to
  // instantiate, only subterm regression through the dispatcher rules.
  // THVM_ATP_GOAL_DRIVE for CLI runs; WL callers use
  // thvm_atp_set_use_goal_drive.  Default OFF: byte-identical
  // trajectories (one branch test in goal_check when unarmed).
  struct AtpGoalDrive *gdrive;
  u8 use_goal_drive;

  // Waldmeister-faithful RHS interreduction (IR_InterreduktionRechts /
  // RMRechtsInterred, INF/Interreduktion.c:329-360) in the -irrp CLI
  // default mode FALSE = "modify rule" (RUN/Parameter.c:334-343).  When
  // set, thvm_atp_interreduce treats surviving slots' RHSs like WM:
  //  - ORIENTED rules: if the new object applies anywhere in the RHS
  //    (one step, ObjektAngewendet NF/NFBildung.c:686-713), the stepped
  //    term is brought to FULL R+E normal form (NF_NormalformstRE
  //    :715-724 -> NF_NormalformRE = doR+doE both TRUE, NFBildung.h:78)
  //    and committed IN PLACE -- no drop, no requeue, no CP, no
  //    symbol-count guard.  Keeps R interreduced (the canonical/reduced
  //    system WM maintains) so the CP set stays small and the goal's
  //    normal form is reached.
  //  - UNORIENTABLE equations: a reducible RHS face drops the E-member
  //    and re-queues it (WM GMInterred, Interreduktion.c:280-293, both
  //    directed twins; the LHS face is the first interreduce loop).
  // 0 (default) keeps the legacy slice-only composition (right_reduce)
  // with its symbol-count guard and no RHS-face E-drop, so test_atp /
  // atp.wlt default behaviour is unchanged; the WL surface flips it for
  // Method -> "Waldmeister".  Set via thvm_atp_set_use_rhs_interreduce.
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
  // GJ all-rules discrimination index (every live rule's stored l->r LHS,
  // no orientable filter) for the ground-join rewrite (gj_rewrite_step):
  // replaces its per-step linear O(R) scan with an indexed candidate
  // collect gated by the model order.  Lazily built; rebuilt on
  // (n_rules, r_revision) change.  Off unless THVM_ATP_GJ_INDEX.
  struct AtpRuleIndex *gj_rule_index;
  // Opt-in flatterm fast-path for the MIXED (orientable + unorientable)
  // normalize loop.  OFF by default: the engine is byte-identical to the
  // tree mixed loop.  When set, atp_rewrite_normalize_ordered's mixed
  // branch keeps the subject in the shared flat arrays across BOTH the
  // orientable indexed fixpoint and the unorientable pass, splicing every
  // rewrite in place (no per-step re-flatten / tree rebuild).  Set from
  // THVM_ATP_FLATTERM=1 at init, or via thvm_atp_set_use_flatterm.  Same
  // normal forms as the tree path (asserted by ATP_FLATTERM_DIFF).
  u8                   use_flatterm;
  // Opt-in faithful Waldmeister interreduction PHASE ORDER
  // (thvm_atp_interreduce_wm).  THVM_ATP_WM_INTERRED=1 to enable.  When set,
  // interreduction runs WM's strict phases (all E-member drops / GMInterred,
  // then all R-LHS drops / RMLinksInterred, then R-RHS compose /
  // RMRechtsInterred) instead of thvm's single interleaved pass.  Default OFF
  // = the existing pass runs byte-identical.  Faithful only when
  // keep_lhs_reducible==0 and use_eager_orphan_sweep==0 (the WM Method layout).
  u8                   use_wm_interred;
  // Opt-in WM-FPA flat normalize path (src/wmfpa/wmfpa.h flatterm rep +
  // DSBaum), OFF by default (THVM_ATP_WMFPA=1 to enable).  Arms the
  // WM-POLICY FT hook (ft_norm.c atp_rules_only_normalize_wf): under the
  // Waldmeister preset (use_mixmost_nf) the doR-only full-range treated-CP
  // push normalize runs on the flat substrate with the engine's exact
  // mixmost walk + Regelbaum-DFS winner -- byte-identical NFs, engine
  // fallback on anything the flat path cannot decide.  The historic
  // Term-side binding of this flag (atp_rewrite_normalize_wmfpa:
  // leftmost-outermost, lowest-slot -- a DIFFERENT redex policy) is
  // re-gated behind THVM_ATP_WMFPA_TERM so one flag never arms two
  // policies.
  u8                   use_wmfpa;
  // Cached WM-FPA discrimination tree of the TERM-side path (opaque;
  // void* to keep wmfpa.h out of the public header) and the R size it
  // reflects.  Rebuilt lazily when n_rules changes, like rule_index.
  // Only touched on the THVM_ATP_WMFPA_TERM path; NULL otherwise.
  void                *wmfpa_tree;
  u32                  wmfpa_built;
  // Private staleness bit for the Term-side WM-FPA tree, set at the same
  // sites that set rule_index_dirty (an in-place rule edit that need not
  // change n_rules).  A private bit avoids fighting the indexed path over
  // the shared rule_index_dirty clear.  (The WM-policy FT mirror keys on
  // r_revision instead and hooks the RHS compose directly.)
  u8                   wmfpa_dirty;
  // Live differential for the WM-policy FT hook: when set
  // (THVM_ATP_WMFPA_CHECK!=0) every flat-decided push normalize ALSO runs
  // the engine mixmost walk and aborts on the first NF-byte divergence
  // (cap-hit partials included); the engine result is returned.  The
  // Term-side path's historic probes moved to THVM_ATP_WMFPA_TERM_CHECK.
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
  // Origin class (cp_kind) of the popped CP -- valid alongside
  // last_popped_trace and, unlike it, immune to the proof-trace soft
  // cap.  Consumed by the WM-order mirror's distinguished-face rule
  // (atp_wmo_eq_dist_rhs) when the trace id is ATP_TRACE_NONE.
  u8   last_popped_kind;
  // Formation depth (cp_depth) of the popped CP -- a rule oriented from it
  // inherits this as its r_depth (Twee getDepth).  0 before the first pop,
  // so initial axioms added via orient_and_add get depth 0.
  u32  last_popped_depth;
  // Goal-derived bit (cp_gd) of the popped CP -- a rule oriented from it
  // inherits this as its r_gd (spb=goal derivation tracking).
  u8   last_popped_gd;
  // Queue sequence number (cp_seq) of the popped CP.  The Otter-loop
  // layer (use_otter) keys a queued CP's passive demodulator on it.
  u32  last_popped_seq;
  // Vampire Otter/LRS simplification set (THVM_ATP_OTTER, --otter; see
  // src/atp/otter.c).  Every orientable queued CP is a PASSIVE rule: it
  // rewrites wherever R rewrites but forms no overlaps until its CP is
  // selected.  Passive status is keyed by rule uid (stable across every
  // slot compaction); otter_map_* maps a queued CP's seq to its passive
  // uid.  otter_rows/otter_seqs record the CPs queued during the current
  // step (armed by otter_rec) for the post-batch replay.  Default OFF:
  // nothing below is touched and the engine is byte-identical.
  u8   use_otter;
  // Raw-overlap demodulation (THVM_ATP_OTTER_RAW, --otter-raw; see
  // src/atp/otter.c): the Otter replay and the selection normalize
  // start from each CP's raw overlap (cp_raw) instead of its push-time
  // treated pair.  Default OFF: the engine is byte-identical.
  u8   use_otter_raw;
  // Interreduction under the run's ordering (THVM_ATP_IR_RUN_ORDER,
  // --ir-run-order): an older rule's sides are rewritten by the new
  // rules through atp_proof_rewrite_step_slice -- leftmost-outermost
  // steps from the root, each unorientable-equation instance checked
  // with atp_compare, i.e. the run's ordering -- instead of the FT walk,
  // whose unorientable leg checks KBO whatever the run's ordering.
  // Default OFF: the engine is byte-identical.
  u8   use_ir_run_order;
  // Vampire bd=preordered (THVM_ATP_BD_PREORDERED): interreduction
  // (backward demodulation of R) only by rules the ordering orients --
  // an unorientable new equation simplifies nothing.  Default OFF.
  u8   use_bd_preordered;
  // Vampire AgeQueue (THVM_ATP_AGE_DEPTH): the age picks of the --awr
  // interleave take the minimal (formation depth, weight, seq) instead
  // of the oldest seq, and the age/weight phase advances on
  // activations (a pop that joins away was never passive in Vampire),
  // age pick first (AWPassiveClauseContainer balance).  Default OFF.
  u8   use_age_depth;
  // Vampire LRS limits (THVM_ATP_LRS_AGE): the horizon prune keeps a CP
  // that either the weight picks OR the age picks of the --awr
  // interleave can still reach (AWPassiveClauseContainer::
  // exceedsAllLimits); lrs_age_horizon is the age-key cutoff.
  u8   use_lrs_age;
  // Prover9/LADR clause-weight penalties (ladr/weight.c clause_weight;
  // THVM_ATP_NEST_PEN / THVM_ATP_DEPTH_PEN): each same-symbol
  // parent/child pair under an arity <= 2 parent adds nest_pen, and
  // the equation's depth (1 + max side depth) times depth_pen is
  // added to the CP priority.  0 = off (default).
  u32  ladr_nest_pen, ladr_depth_pen;
  // LADR lex-dependent demodulation (ladr/demod.c; THVM_ATP_LEX_DEP_DEMOD
  // = lex_dep_demod_lim, 0 = off): an unorientable equation with equal
  // side symbol counts and n_l + n_r + 1 <= the limit demodulates in
  // EVERY normalization (rules-only ones included), each instance step
  // gated by the run's ordering; no other unorientable equation does.
  u32  lex_dep_lim;
  u64  lrs_age_horizon;
  u8   otter_rec;
  u8   otter_primed;        // the input queue got its replay (first pop)
  u32 *otter_rows;
  u32 *otter_seqs;
  u32  otter_n, otter_cap;
  u8  *uid_passive;
  u32  uid_passive_cap;
  u32 *otter_map_key;       // seq + 1 (0 = empty bucket)
  u32 *otter_map_uid;       // passive uid (0 = retired)
  u32  otter_map_cap, otter_map_used;
  u64  n_otter_adopted, n_otter_retired, n_otter_joined, n_otter_unorient,
       n_otter_refused;
  // Folded-signature label band [fold_lo, fold_hi): the fresh
  // definitional symbols (sF*/sFlat*/f_g*) the pr-layer transformations
  // introduced, registered via thvm_atp_set_fold_band.  Consumed by the
  // drc=off analog (THVM_ATP_DRC_OFF): a CP whose sides carry a folded
  // symbol is NOT discarded when its sides join -- it is kept as a
  // rule/equation (Vampire drc=off keeps demodulation-redundant clauses;
  // the GRP loop-theory oracle proofs pass through such folded lemmas).
  // fold_hi == 0 (default) disarms every check.
  u32  fold_lo;
  u32  fold_hi;
  // drc=off keep counters (diagnostic): CPs kept at the push-time join
  // gate / at the pop-time join gate that classic completion discards.
  u32  n_cps_drc_kept_push;
  u32  n_cps_drc_kept_pop;
  // needle (THVM_ATP_DRC_NEEDLE / --drc-needle): narrows the drc=off
  // keep set above to the GRP678/692-shaped folded lemmas whose
  // sF-UNFOLDING (substituting the tgt=3 definitions back to the
  // original signature) is either an alpha-instance of one of the
  // ORIGINAL (pre-tgt) axioms or trivially x=x -- the two shapes
  // Vampire's GRP loop-theory proofs actually route through, vs. the
  // much larger haystack wholesale drc=off keeps.  fold_def_body is
  // indexed by (label - fold_lo); body == 0 means "no definition
  // registered" (unfold fails closed -- the candidate is dropped, same
  // as pre-needle).  needle_ax_l/r hold the pre-transform EQUATIONS
  // block (n_needle_ax == 0 disarms the instance test but not x=x).
  // Populated once at problem intake via thvm_atp_set_fold_defs /
  // thvm_atp_set_orig_axioms (tests/atp/test_atp_wolfram_bench.c); both
  // Term arrays are GC roots (thvm_atp_gc_collect).  See
  // atp_drc_needle_ok in src/atp/_.c.  Inert (0 cost) unless
  // THVM_ATP_DRC_NEEDLE is armed -- byte-identical otherwise.
#define ATP_NEEDLE_MAX_DEFS    512u
#define ATP_NEEDLE_MAX_AXIOMS  64u
  u8   fold_def_arity[ATP_NEEDLE_MAX_DEFS];
  Term fold_def_body[ATP_NEEDLE_MAX_DEFS];
  Term needle_ax_l[ATP_NEEDLE_MAX_AXIOMS];
  Term needle_ax_r[ATP_NEEDLE_MAX_AXIOMS];
  u32  n_needle_ax;
  u32  n_cps_drc_needle_kept;
  u32  n_cps_drc_needle_dropped;
  u32  n_cps_drc_needle_overcap;

  // FIFO live-scan floor: every queued CP with cp_seq < fifo_floor_seq
  // is PROVEN dead (its scan-time death test failed and death marks --
  // r_trace_dead / uid_dead -- are set-only) or already extracted, so
  // the scan skips it with one compare instead of re-running the
  // orphan tests on the whole dead prefix at every FIFO pick.  Sound
  // because cp_seq stamps are handed out monotonically (a new push can
  // never appear below the floor) and the scanned minimum is always
  // extracted right after the pick (selected or discarded).
  u32  fifo_floor_seq;

  // Transient: set by thvm_atp_orient_and_add's KBO_UN branch to 1 when
  // the CP-formation side geometry swap (use_cp_side) physically fired
  // for the just-stored unorientable equation, 0 otherwise.  The WM
  // emission-order mirror (atp_wmo_eq_dist_rhs) reads it back so the
  // distinguished-face flag matches the physical orientation even for the
  // axiom case, whose swap decision depends on the PRE-swap term order
  // (not recoverable from the stored, post-swap sides).
  u8   last_cp_wm_side_swapped;

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
  // Goal-check revision cache: the goal's joinability changes only when R
  // changes.  thvm_atp_goal_check runs twice per step (top-of-step +
  // post-cp-gen); the two calls at the same r_revision recompute the same
  // (large) goal normalize.  Stores r_revision+1 of the last RUNNING
  // verdict (0 = never); a matching r_revision skips the redundant
  // normalize.  Runtime profiling showed goal_check is a hidden ~1/3 of
  // wall on big goals (OrAssociativity).  PROVED is never cached (it stops
  // the loop), so a stale cache can only DELAY, never miss, a proof.
  u32 goal_check_running_rev;

  // Multi-goal conjunction (FindEquationalProof[{g1, g2}, axioms]):
  // every conjunct proved off ONE saturation.  goals_lhs/goals_rhs are
  // the n_goals conjunct pairs (heap arrays owned by the state, set by
  // thvm_atp_set_goals); goals_lhs_nf/goals_rhs_nf the per-goal live
  // normal forms (0 until that goal's first goal_check).  Bit g of
  // goals_joined_mask latches once goal g has joined -- R only grows,
  // so a joined goal stays joined -- and goal_check returns ATP_PROVED
  // only when all n_goals bits are set.  n_goals is capped at
  // ATP_MAX_GOALS == 64 (the mask width).  goal_lhs/goal_rhs above
  // stay the single-goal storage AND, in multi-goal mode, the alias to
  // the FIRST UNJOINED goal, which every goal-directed heuristic
  // (CPinGoal, SoS, goal-interleave, MNF) reads -- selection
  // heuristics never affect soundness, so steering by one goal at a
  // time is sound.
  Term *goals_lhs;
#ifdef THVM_ATPFT_RULES
  // Per-conjunct FT mirrors + NF decode caches (the multi-goal twin of
  // goal_lhs_ft / goal_nf_ft_l): the conjunct loop's checks clone the
  // mirror instead of re-encoding the pair per check, and reuse the
  // previous NF Term when the span content is unchanged.
  struct AtpFtCell **goals_lhs_ft;
  struct AtpFtCell **goals_rhs_ft;
  struct AtpFtCell **goals_nf_ft_l;
  struct AtpFtCell **goals_nf_ft_r;
#endif
  Term *goals_rhs;
  Term *goals_lhs_nf;
  Term *goals_rhs_nf;
  u32   n_goals;
  u64   goals_joined_mask;

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
  // s2a (Vampire sine_to_age): per-symbol SInE level via the Hoder-
  // Voronkov D-relation (generality-triggered), seeded from the
  // conjecture symbols.  0 = in the goal; k = k SInE-defines-steps away;
  // 255 (ATP_REL_LEVEL_REMOTE) = goal-unreachable.  Distinct from
  // sym_level[] (a plain co-occurrence BFS): the D-relation triggers only
  // through a clause's near-least-general symbols, so a common hub symbol
  // does NOT drag every axiom to level 1.  Recomputed by
  // atp_compute_sine_levels (called from thvm_atp_set_goals) only when
  // s2a_coeff > 0.  Read by atp_cp_sine_level.  Sized WALD_MAX_SYMBOLS.
  u8 sym_slvl[64];
  // 1 once atp_compute_sine_levels has filled sym_slvl for the current
  // goal (it is also run, decoupled from selection, when the GNN re-rank
  // or the graph-dataset export needs the goal_closeness node feature).
  // Guards the calloc-zeroed default: an unfilled sym_slvl is all-0, which
  // would otherwise read as "every symbol is a level-0 goal symbol".
  u8 goal_levels_ready;
  // s2a axiom snapshot: per-input-axiom symbol mask, captured at enqueue
  // (atp_enqueue_equation) when s2a_coeff > 0.  Input equations are
  // enqueued as CPs long before any becomes a rule, so
  // atp_compute_sine_levels sources the D-relation generalities from this
  // snapshot rather than the (still-empty) rule set.  Growable; freed at
  // thvm_atp_free.  NULL/0 for the byte-identical default engine.
  u64 *s2a_ax_mask;
  u32  n_s2a_ax;
  u32  cap_s2a_ax;

  // Reduction ordering (caller-owned).  When `lpo` is non-NULL,
  // it takes precedence over `kbo`: orient_and_add dispatches to
  // `thvm_lpo`.  When both are NULL, every comparison is
  // incomparable (KBO_UN) -- saturation falls into unfailing
  // fallback.
  const KboConfig *kbo;
  const LpoConfig *lpo;
  const RpoConfig *rpo;
  const WpoConfig *wpo;

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
  // The default cap the search sees (ATP_MAX_TRACE): an entry at or past
  // it exists only when a proof raised t_max, and the search treats it
  // as absent (orphan tests, death marks, implicit CPs, the heap tail,
  // the work-clock window), so a proof run searches as the no-proof run.
  // Only a test lowers it, to emulate a no-proof run at a small cap.
  u32   t_vis;
  // Bytes held only by entries past the default cap (ATP_MAX_TRACE),
  // which exist only when the cap is raised for a proof: their nodes
  // live in the permanent heap tail and their cost is off the work
  // clock; the RSS guard discounts these bytes, so the search sees the
  // default-cap run's memory.
  u64   trace_x_bytes;
  // Some entry past the default cap sits in the collected heap (a full
  // permanent tail, a non-atom node, a later rebuild): the collector then
  // roots the whole trace instead of the default-cap prefix.
  u8    trace_x_heap;
  // Rule-set journal (src/atp/rule_journal.c): every birth, content
  // stamp and death of a rule, in order, plus the rule-set position each
  // ORIENT / FVI / SIMPLIFY record was computed at.  Always on and the
  // same with or without --proof; only the proof re-derivation reads it.
  // Views of records past the default trace cap (proof only) go to
  // rj_x as (record, position) pairs, their bytes in trace_x_bytes.
  AtpRjEv *rj;
  u32   rj_n;
  u32   rj_cap;
  u8    rj_lost;          // an allocation failed: the journal is incomplete
  u32   rj_hold;          // position + 1 the next records were computed at (0: now)
  u32  *rj_x;
  u32   rj_x_n;
  u32   rj_x_cap;

  // Off-heap trace-equation store (env THVM_ATP_TRACE_PACK).  The proof
  // DAG's per-step equation (children 2,3 of each trace entry) is the
  // dominant IC live set on deep completions (a 1601-rule run pins
  // ~130M cells of trace Terms, exhausting the from-space at ~rule 250).
  // When trace_pack is on, atp_trace_push stores those two Terms as
  // packed bytes here and leaves NUM(0) sentinels in the term, so the
  // GC-rooted trace spine stays tiny; reads reconstruct via
  // atp_trace_eq_load.  Default OFF -> byte-identical legacy behaviour.
  u8    trace_pack;
  u64  gc_live_after;       // live cells after the last collection
  u8   heap_full;           // a pressure collection left the heap above the abort fraction
  u8   *trace_eq_buf;     // append-only packed (lhs,rhs) pairs
  u64   trace_eq_len;     // bytes used
  u64   trace_eq_cap;     // bytes allocated
  u64  *trace_eq_off;     // [t_cap] byte offset of entry i's pair (+1; 0=none)

  // Caller-owned extra GC roots.  thvm_atp_gc_collect evacuates the
  // state's own live set only; a caller holding Term handles across a
  // collection (the WL bridge keeps the encoder-built axiom/goal wire
  // terms for the post-PROVED EXT-state re-seed) registers them here so
  // every collect relocates them in place instead of leaving forwarding
  // stubs behind (a stubbed handle decodes WL-side with a garbage arity
  // -- the kernel-killing 28GB DAG-unfold).  NULL/0 (the default and
  // every non-bridge caller) is byte-identical to the historical root
  // set.
  Term *gc_extra_roots;
  u32   n_gc_extra_roots;

  // Lean CP trace (env THVM_ATP_CP_TRACE_LEAN).  Like trace_pack it plants
  // NUM(0) sentinels for a TRACE_CP entry's children 2/3, but stores the
  // pair NOWHERE -- the queued CP already carries its terms in cp_packed
  // (selection unpacks them there), and a CP that becomes a rule has the
  // rule's own terms for the proof DAG.  The millions of joinable CPs that
  // never enter the proof simply drop their terms.  Requires the packed
  // CP path (not IMPLICIT_CP, which reads children 2/3 back).  Trades the
  // exact CP-lemma statement in the proof object for the ability to hold
  // WM's full working set; the search + goal-join are unaffected.
  u8    cp_trace_lean;

  // Stage 7.1: count of CPs dropped at generate-time because both
  // sides normalize to the same term under current R (trivial
  // joinability -- Waldmeister's `Grundzusammenfuehrung`,
  // "ground-merging" criterion).  Useful for benchmarking the
  // pruning power; not consulted by saturation logic.
  u32  n_cps_dropped_joinable;

  // Stage 7.2b: count of CPs that are source-rule-disjoint
  // connected (joinable under R \ {rule_a, rule_b}, the two rules
  // that birthed the CP).  By a domination lemma, this is bounded above
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
  // Runtime gate for LAZY (selection-time) ground-joinability.  When set,
  // the expensive ordered-set-partition GJ check runs on the SELECTED,
  // R+E-normalized CP (~1 per pick) instead of on every generated CP at
  // push time.  Twee's redundancy discipline: push-side GJ is O(generated
  // CPs) and dominated GRP766 cp-gen at 8.6s/92%; lazy GJ pays it once at
  // pop.  Sound (a ground-joinable CP is redundant; dropping before
  // orientation preserves completeness).  Set via
  // thvm_atp_set_use_lazy_ground_join; push-side use_ground_join should be
  // off under this mode so GJ is paid once.
  u8   use_lazy_ground_join;

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
  // s2a (Vampire sine_to_age) coefficient.  0 = off (engine byte-
  // identical: no sine-level pass, no cp_sine writes read, the FIFO/age
  // lane keeps its raw-cp_seq selection).  N>0 arms it: the FIFO/age
  // dimension orders live CPs by effective age = cp_seq + N*cp_sine so a
  // goal-distant CP (high SInE level) is postponed.  Large N approaches
  // Vampire's lexicographic (sine_level, age).  Set via thvm_atp_set_s2a
  // / env THVM_ATP_S2A.
  u32  s2a_coeff;
  // Waldmeister CPdimension fairness ratio (YFiles `Schrittweiten`):
  // 1 FIFO (oldest-CP) pick per `fifo_modulo` selections, the rest by
  // weight.  Prevents the smallest-weight heap from starving an older
  // heavy CP.  Default 11 (1:10, the most-fair Waldmeister setting);
  // Waldmeister also uses 50/100/200.  0 is treated as the default.
  u32  fifo_modulo;
  // Age picks per fifo_modulo selections (0 = the historical single
  // pick).  Set only by the --awr=a:w form (Vampire's age:weight
  // ratio with a > 1, e.g. Prover9-style age-heavy 3:2).
  u32  fifo_age_part;
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
  // Waldmeister MaxWeight: discard a critical pair whose combined term
  // weight exceeds this (0 = unbounded).  Bounds the search on
  // self-overlapping axioms (the single Wolfram NAND axiom) so the
  // goal-directed selector is not starved by runaway pairs.
  u32  max_cp_weight;

  // Twee --max-term-size (per-side CP node cap; 0 = unbounded): discard a
  // critical pair when EITHER side's node count exceeds this, i.e. bound
  // max(|lhs|,|rhs|) <= cap.  Twee applies cfg_accept_term to BOTH overlap
  // sides in dequeue (Twee.hs:423-424), checkSize n t = KBO.size t <= n
  // (SequentialMain.hs:194); with default weights KBO.size is the raw node
  // count.  This is NOT max_cp_weight, which bounds the SUM |lhs|+|rhs|: a
  // monster CP with one huge side + one small side clears any generous sum
  // bound yet is dropped per-side, the mechanism that lets Twee's bounded
  // completion terminate on LAT138-1 where the sum cap floods.  Opt-in,
  // completeness-breaking (see cli_strategy_complete).
  u32  max_cp_side;

  // Hard CP-queue-SIZE cap (0 = unbounded): when the live CP count reaches
  // this, new critical pairs are dropped at push (n_cps_dropped_qcap ticks)
  // so the queue arrays + packed byte strings stay bounded -- a MEMORY leash
  // for runaway saturations (the weight caps bound per-CP SIZE, not COUNT;
  // a prolific problem grows the queue in count and balloons C memory past
  // the host limit).  Lossy like max_cp_weight: the periodic FIFO / priority
  // selection still fires over the kept CPs, but dropped CPs are gone, so
  // this trades completeness for a memory bound.  Opt-in.
  u32  max_cp_queue;
  u64  n_cps_dropped_qcap;          // CPs dropped by the queue-size cap

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
  // r_revision+1 at which auto_max_cp_weight_cur was last computed (0 =
  // never).  The deepest-rule scan is O(n_rules) and ran every step;
  // cached here so it re-scans only when R actually changed.
  u32  auto_maxw_rev;
                                    // (monotone; feeds the auto bound so it
                                    // need not rescan R on every CP push)
  // Overflow stash for CPs deferred by the growing bound.  Parallel to
  // the cp_* arrays but unordered (a plain growable list): a packed
  // byte string + trace id + node count.  Drained into the heap when
  // the bound grows past their weight.
  u8 **cp_stash_packed;
  u32 *cp_stash_trace;
  u32 *cp_stash_nodes;
  u8  *cp_stash_ultimate;  // parallel u8 flag: WM Act_ultimate carry-over
  u8  *cp_stash_kind;      // parallel origin class (see cp_kind)
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
  // Twee task discipline for the CP-set sweep (Twee.hs:978 newTask +
  // Task.hs taskDue + shouldSimplifyQueue).  When cp_set_ir_budget > 0
  // it REPLACES the rule-count period gate above: the sweep runs at
  // most once per ATP_CP_SET_IR_FREQ_US, only while the cumulative
  // sweep time stays under budget% of the elapsed run (Twee
  // cfg_renormalise_percent, default 5), and only when >= 20% of a
  // 100-CP random sample of the queue is dead -- joinable under the
  // current R or orphaned (Twee cfg_renormalise_threshold = 20 over
  // cfg_cp_sample_size = 100).  0 = legacy every-period-rules sweep.
  // The three stamps read atp_work_now_us: process CPU microseconds by
  // default (Twee's getCPUTime clock), so the gates ignore time spent
  // descheduled; THVM_ATP_WORK_CLOCK=instr makes them deterministic.
  u32  cp_set_ir_budget;          // percent; 0 -> period discipline
  u64  cp_set_ir_spent_us;        // cumulative sweep + sample-check CPU
  u64  cp_set_ir_last_us;         // last discipline check (freq gate)
  u64  cp_set_ir_start_us;        // discipline epoch (first check)
  u32  n_cp_set_ir_passes;        // diagnostics: passes run
  u32  n_cp_set_ir_deleted;       // diagnostics: CPs deleted (joinable)
  u32  n_cp_set_ir_reweighted;    // diagnostics: CPs reweighted
  // Incremental IR (THVM_ATP_INCR_IR=1): track the rule count at the
  // end of the last IR pass and a bitmap of TOP SYMBOLS appearing in
  // rules added since.  CPs whose terms contain none of those symbols
  // cannot fire any new rule, so the per-CP normalize is a guaranteed
  // no-op and we skip it.  Soundness: a normalize that would have been
  // a no-op anyway is byte-equivalent to skipping.  Empirical: cuts
  // interreduce cost from O(Q*R*term) to O(Q*term+matches*R*term) on
  // wolfram (where each new rule typically affects <10% of queued CPs).
  u8   use_incr_ir;
  u32  ir_rule_watermark;         // n_rules at end of last IR pass
  // Top-symbol bitmap of rules added since watermark.  64-bit fits the
  // small symbol vocabularies thvm bench uses (L_NAND..L_NOT = 1..6);
  // for larger sigs, falls back to "always re-normalize" (bit 63 = any).
  u64  ir_new_rule_top_syms;
  u32  n_cp_set_ir_skipped;       // diagnostic: CPs skipped this run

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
  // Eager orphan sweep at interreduce time (the ATP_ORPHAN_KILL pass in
  // thvm_atp_interreduce): when a rule drops, walk the whole CP queue and
  // delete its descendant CPs immediately, then reheapify.  This is a
  // thvm EXTRA over WM -- WM only does the lazy at-pop discard above --
  // and it changes live-queue composition (heap size, FIFO ages, FV-index
  // contents) versus WM's layout.  Default ON (the historical compiled-in
  // behavior); Method->"Waldmeister" selects WM's layout (lazy ON, eager
  // OFF) via thvm_atp_set_use_eager_orphan_sweep.  No-op unless built
  // with -DATP_ORPHAN_KILL.
  u8   use_eager_orphan_sweep;
  u8  *r_trace_dead;              // bitset over trace ids (8 ids / byte)
  u32  r_trace_dead_cap;          // capacity in bits (== trace ids)
  u32  n_cps_dropped_orphan;      // diagnostics: orphan CPs skipped at pop

  // Deferred-CP (`implicit_pair`) arc: opt-in flag that switches
  // `atp_push_cps_traced` to record rule-x-rule CPs as compact trace-
  // backed `AtpCpImplicit` descriptors instead of packed byte strings
  // (axiom/initial, trace-capped, and auto-MaxWeight over-bound CPs
  // stay eager).  Materialization is paid at selection time via
  // `atp_cp_implicit_materialize`, which reads the raw pair off the
  // slot's TRACE_CP entry (zero-copy).  Trajectory under the flag can
  // differ from eager (no queue-vs-queue subsumption on the implicit
  // passive set; pop-normalize starts from the raw overlap), so the
  // flag-ON gate is proof validity, not byte parity.  Default 0
  // (OFF): engine byte-identical.
  u8   use_implicit_cp;

  // Raw-descriptor lane (THVM_ATP_RAW_DESC): store the WM preset's
  // raw >= 50 KPBehandelt class -- the walk former's FT-direct byte
  // emits, ~84% of all packed CPs -- as 20-byte parent+position
  // descriptors instead of malloc'd packed byte strings, and re-derive
  // the identical bytes from the parent face spans at pop (rename +
  // ft_unify + vcp_ft_emit_fused, the exact push-time pipeline).  Every
  // push-side gate, weight, seq and uid stamp runs unchanged -- only
  // the STORAGE differs, so the trajectory is byte-identical to eager.
  // The < 50 treated class, anomaly fallbacks and every non-FT-routed
  // push stay eager.  Default 0 (OFF): engine byte-identical.
  u8   use_raw_desc;
  // Certifier mode (THVM_ATP_RAW_DESC_CERT): store BOTH the eager
  // packed bytes (slot behaves fully eager, tag bit clear) and the
  // descriptor; every pop re-derives from the descriptor and memcmps
  // against the stored bytes, thvm_fatal on any mismatch.  The proof
  // of byte-exactness for use_raw_desc.  Default 0.
  u8   use_raw_desc_cert;

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

  // WM `-ks` "s" stage (KPV_Select, INF/KPVerwaltung.c:667): a popped
  // CP whose normalized pair is UNORIENTABLE (compare == KBO_UN, WM
  // Unvergleichbar) and subsumed by an existing unorientable equation
  // (SS_TermpaarSubsummiertVonGM, INF/Subsumption.c:91) is dropped
  // before orientation.  Subsumption = the pair, or the subpair
  // reached by descending the unique differing-subterm path (both
  // sides identical outside one hole per level), is an instance of a
  // live E-member under ONE substitution covering both sides, either
  // orientation (WM stores each equation as Gleichung + Antigleichung
  // twins, RUndEVerwaltung.c:407-470, so both directions sit in the
  // Gleichungsbaum the matcher walks).  Rules never subsume here --
  // WM consults RE_Gleichungsbaum only.  Default OFF (engine
  // byte-identical); ON in the "Waldmeister"* presets (`-ks` default
  // "r:e:s:p", RUN/Parameter.c:407) via Method "PopSubsume" /
  // thvm_atp_set_use_pop_subsume.
  u8   use_pop_subsume;
  u32  n_cps_dropped_pop_subsumed;

  // Push-time queue-vs-queue subsumption (atp_cp_queue_subsumed): a
  // freshly-generated CP that is a substitution instance of some
  // already-QUEUED CP (either side order) is dropped before it reaches
  // the heap.  thvm-native filter with NO Waldmeister counterpart: WM
  // queues every generated CP that survives KPBehandelt
  // (recentCPinsert, INF/KPVerwaltung.c:383-417, straight KDH_insert;
  // SS_TermpaarSubsummiertTermpaar's only set-level caller is the
  // E-set sweep, Interreduktion.c:262, never the passive queue), and
  // every insert consumes a w2 = ++CPNr FIFO age, so dropping here
  // shifts every later CP's age relative to WM.  Default ON (the
  // historical thvm engine); the "Waldmeister"* presets turn it OFF
  // for WM-exact queue composition via Method "QueueSubsume" /
  // thvm_atp_set_use_queue_subsume.
  u8   use_queue_subsume;

  // WM E-set subsumption on new-equation entry (GMSubsummierenMit-
  // Gleichung, INF/Interreduktion.c:251-274; reached from
  // IR_InterreduktionLinks :366-378 for every non-rule new fact,
  // BEFORE RE_FaktumEinfuegen inserts it -- ArbeitsAufnahme,
  // INF/Hauptkomponenten.c:311-312).  Every existing live E-member
  // (distinguished direction only, TP_RichtungAusgezeichnet :261)
  // that the new unorientable equation subsumes
  // (SS_TermpaarSubsummiertTermpaar, INF/Subsumption.c:104-110: one
  // substitution covering both sides, either pattern orientation,
  // context-stripping descent) is removed AND destroyed, twin
  // included (FinaleKillprozSubsumption, Interreduktion.c:236-245)
  // -- NO requeue, no CP made.  thvm stores one slot per equation,
  // so the kill is the bwd-subsume soft-delete (dead-sentinel faces
  // + r_dead[i] + save slots).  Default OFF (engine byte-identical);
  // ON in the "Waldmeister"* presets via Method "ESetSubsume" /
  // thvm_atp_set_use_eset_subsume.
  u8   use_eset_subsume;
  u32  n_eqs_dropped_eset_subsumed;
  // When set, atp_eset_subsume_by_new uses the WM flatterm-faithful
  // (counter-cross, over-eager) matcher atp_wm_flat_subsumes_pair instead
  // of the correct tree matcher -- needed to reproduce WM removing axiom2
  // on commutativity-add (firstdiv-19, ShefferAxioms OrAssociativity).
  // Default OFF; ON in the "Waldmeister"* presets alongside use_eset_subsume.
  u8   use_flat_subsume;
  // Commutativity-aware E-set subsumption (env THVM_ATP_COMM_SUBSUME,
  // DEFAULT OFF).  After the plain GMSubsummierenMitGleichung test in
  // atp_eset_subsume_by_new, also drop an older equation whose LHS matches
  // the new equation's LHS EXACTLY and whose RHS equals the new RHS modulo
  // ONE top-`.`-child swap (a.b == b.a), gated on a LIVE `f(x,y)=f(y,x)`
  // equation being present (atp_op_is_live_commutative).  This is WM's
  // commutativity-normalised Interreduktion -- a single top swap, NOT
  // AC-saturation/equivalence-class enumeration.  It drops the soa slot15
  // `x.(y.x)=(y.y).x` exactly as WM does when eqn-10 `x.(y.x)=x.(y.y)`
  // enters, one top swap apart.  Diagnostic, NOT a parity win: ON forks
  // soa firstdiv 125->99 (slot15 is the unique parent of the displaced
  // pick-99 COMM copy) and explodes a commutative-ring baseline via
  // remove-and-rederive thrash.  See tools/baselines/wm_align_reports/soa.txt.
  u8   use_comm_subsume;
  u32  n_eqs_dropped_comm_subsumed;

  // WM CP-emission ORDER (FIFO age assignment): when set, each new
  // fact's CP batch is sorted into Waldmeister's emission order before
  // pushing, so equal-weight CPs receive their w2/FIFO ages in WM's
  // order.  WM emits per new fact in phase order (Unifikation1.c
  // U1_KPsBildenZuRegel :1481 / U1_KPsBildenZuGleichung :1556): tops
  // phases walk the new face's subterms in flatterm preorder against
  // rule-tree then equation-tree tops (trie-DFS arrival order within a
  // position); eTT phases scan the discrimination tree's LEAF LIST
  // (depth-sorted, insert-after-class-head -- DSBaumOperationen.c
  // BlattEinzeigern :365-442) x per-leaf chains (newest first) x
  // per-fact proper subterms in preorder.  `wmo` holds the
  // order-mirror state (src/atp/wm_order.c); facts register on
  // creation and deregister at every removal site, keyed by birth
  // trace id (compaction-proof).  Default OFF (engine byte-identical);
  // ON in the "Waldmeister"* presets via Method "EmissionOrder" /
  // thvm_atp_set_use_emission_order / THVM_ATP_EMISSION_ORDER.
  u8   use_emission_order;
  void *wmo;

  // Waldmeister loader-level axiom canonicalization + intake
  // semantics (src/atp/wm_intake.c).  WM's spec loader canonically
  // SORTS the initial equation set (SpezNormierung: symbol order ->
  // per-equation side order -> variable renumber -> equation sort,
  // WASIC/SpezNormierung.c:758-791) and the default classification
  // `initial=ultimate` (RUN/Parameter.c:165-167) stamps every axiom
  // w1 = minimalWeight() = INT32_MIN with w2 = ++CPNr in SORTED
  // order (CLAS/NewClassification.c:315-330), so axioms pop FIRST,
  // in canonical-sort FIFO order.  When on, the first thvm_atp_step
  // call permutes the queued axiom slots' cp_seq stamps into that
  // order and tags them ultimate (wm_intake_done latches the
  // once-only flush).  Default OFF (engine byte-identical); ON in
  // the "Waldmeister"* presets via Method "IntakeOrder" /
  // thvm_atp_set_use_intake_order / THVM_ATP_INTAKE_ORDER.
  u8   use_intake_order;
  u8   wm_intake_done;

  // Waldmeister normal-form STRATEGY (the `-nf` option, default
  // "mixmost", RUN/Parameter.c:418-419; NF_Normalform =
  // NormalformMixMost, NF/NFBildung.c:837-840 = the path-stack walk
  // NormalformZuRegelnOderGleichungenAufNochmal :349-377): preorder
  // try-reduce-before-descend with a local fixpoint at each reduced
  // position, then an ancestor ascent, then resume at the last
  // reduced position -- never a rescan from the root.  thvm's legacy
  // FT normalize is WM's "outermost" (BL_NormalformOutermost `goto
  // root`, :591-613), which reaches DIFFERENT normal forms on a
  // non-confluent mid-completion R and flipped generation-time CP
  // joinability verdicts vs WM -- the duplicate-CP multiplicity
  // divergence class of the alignment matrix (McCune
  // EqualityOfInverses cp 1893, HigmanNeumann Associativity cp 597).
  // Default OFF (engine byte-identical); ON in the "Waldmeister"*
  // presets via Method "MixmostNF" /
  // thvm_atp_set_use_mixmost_nf / THVM_ATP_MIXMOST_NF.
  u8   use_mixmost_nf;

  // WM backward ground-joinability sterilization
  // (RueckwaertsGrundzusammenfuehrbarkeit, INF/Hauptkomponenten.c:
  // 260-306, called at the END of ArbeitsAufnahme :329 AFTER CP
  // generation + the IR-victim drain).  After every new fact, WM
  // re-tests each EXISTING rule (RE_forRegelnRobust :265) and each
  // distinguished-direction equation (:286) for ground joinability
  // against the system extended by the new fact -- the victim itself
  // excluded from rewriting (DarfNichtReduzieren :267/:287, consumed
  // MatchOperationen.c:641/742/1660/1697) and its maximal side(s)
  // root-protected by the strict-encompassment Dreieck gate
  // (Grundzusammenfuehrung.c:235-240 via NF_geschuetzteNormalformRE,
  // NFBildung.c:767-779; sSch for rules / allesSch for equations,
  // :841).  A fact shown ground-joinable is STERILIZED per the
  // compiled GZ_ZSFB_BEHALTEN=1 (Grundzusammenfuehrung.h:52): it
  // STAYS in R/E for rewriting, but (a) KPV_KillParent
  // (KPVerwaltung.c:343-351) sets lebtNoch=FALSE so its queued CPs
  // are dropped as orphans at pop (selectNonOrphan :540 -- thvm
  // analog: atp_trace_mark_dead + atp_cp_is_orphan), and (b) it is
  // excluded from ALL future CP formation as either parent
  // (Weggefiltert, Unifikation1.c:967-972; new-fact skip :1518/:1588).
  // The same -gj flag also runs the FORWARD fact test at creation
  // (RUndEVerwaltung.c:182-183 rules / :457-460 equations), which
  // additionally tags A/C/extended-C shaped facts GZ_wertvoll
  // (PROTECT_3_PERMS, Grundzusammenfuehrung.c:808-814) so they are
  // never sterilized.
  //
  // Default OFF, matching WM: the -gj CLI option defaults FALSE
  // (RUN/Parameter.c:317, applied :635) and NO strategy table emits it
  // (YFiles.c:125-128 `gj()` has zero call sites), so the unconfigured
  // .pr run bails at Hauptkomponenten.c:263.  Opt-in via Method
  // {... "BackwardGroundJoin" -> True} / THVM_ATP_BWD_GROUND_JOIN.
  // The test reuses the Twee-style groundJoin driver
  // (atp_cp_ground_joinable) -- the same ground-joinability criterion
  // as WM's MN90 enumeration, with two WM-conservative envelopes:
  // root protection is kept for the WHOLE test (WM drops it once a
  // side reduces / once the instantiated sides become comparable --
  // a permissive refinement; keeping it only under-sterilizes), and
  // WM's failure-witness retest gate (USE_WITNESSES,
  // Grundzusammenfuehrung.c:828-840) is not cached -- FAILED facts
  // are simply retested per new fact (pure perf, same verdicts).
  u8   use_bwd_ground_join;
  // Twee interreduce1 port (Twee.hs:787 deleteActive): when a backward
  // fact-test finds a rule ground-joinable, DELETE it from R (soft-delete,
  // dead-sentinel recipe) instead of only sterilizing its CP formation.
  // WM KEEPS ground-joinable facts as live rewriters (GZ_ZSFB_BEHALTEN,
  // _.c:20456-20463), so this DIVERGES from WM -- strictly opt-in, default
  // OFF, so the WM preset (test_atp byte-parity) is untouched.  Requires
  // use_bwd_ground_join to run the walk that reaches the delete site.
  u8   use_gj_delete_rule;
  // Twee interreduce/interreduce1 port (Twee.hs:774): re-normalize EVERY
  // live rule's LHS against the WHOLE current rule set (excluding itself)
  // each periodic pass, and collapse (drop+requeue the reduced equation)
  // any rule whose LHS is reducible by a COMBINATION of the others.  thvm's
  // thvm_atp_interreduce is INCREMENTAL (reduces old rules only against the
  // newest 1-2), so rules that become reducible by a later combination
  // persist -- the dominant source of the CASC AC/loop rule-set bloat
  // (MVA009/GRP721 keep 500-880 rules where twee keeps ~44).  Opt-in,
  // default OFF (diverges from WM's incremental interreduce), so the WM
  // preset (test_atp byte-parity) is untouched.
  u8   use_full_interreduce;
  // Encompassment demodulation (Twee reducesSkolem, Rule.hs:425): permit an
  // UNORIENTABLE equation to rewrite when its instance is strictly
  // decreasing under the SKOLEM-GROUND KBO (variables read as fresh
  // constants: weight var_weight, precedence below every function
  // symbol, ordered by id) even though the open-term instance
  // comparison is incomparable.  Canonicalizes variable-permuted
  // variants (commutativity et al), which is what keeps Twee's rule
  // set lean on the AC-heavy CASC families.  Each step strictly
  // decreases the subject's skolem image in a total well-founded
  // reduction order, so every normalize terminates.  Opt-in, default
  // OFF (byte-identical): env THVM_ATP_ENCOMPASS / --encompass-demod.
  u8   use_encompass_demod;
  // Simpler-join CP discipline (Twee Join.hs:55 joinCriticalPair,
  // allSteps = step1 >=> step2 >=> step3): when the standard join
  // normalize leaves a CP's sides distinct, re-normalize both sides
  // admitting oriented rules AND unorientable equations, with every
  // step gated on the rewriting equation being strictly SIMPLER than
  // the CP equation (Twee simplerThan over lessEqSkolem -- the
  // skolem-ground lex compare in ft_order.c), and unorientable
  // instances admitted when strictly decreasing under the skolem-ground
  // KBO (Twee reducesSkolem) even where the open-term compare is
  // incomparable.  Joins far more CPs than plain doR/doE joinability,
  // keeping the queue and R lean on the AC/loop CASC families.
  // VERDICT-ONLY: kept CPs queue their standard NFs, so trace/replay
  // are untouched.  Value 1: runs at generation-time treatment
  // (atp_cp_trivially_joinable[_ft], vcp_treat_joinable) AND at
  // selection (the pop joined gate).  Value 2: selection only --
  // Twee's own placement (consider runs at dequeue), skipping the
  // per-generated-CP push tax.  Opt-in, default OFF (byte-identical):
  // env THVM_ATP_SIMPLER_JOIN / --simpler-join[=2].
  // Completeness caveat mirrors use_encompass_demod: the skolem steps
  // carry no below-peak connectedness constraint, so a saturation
  // under this mode reports GaveUp, never Satisfiable
  // (cli_strategy_complete).
  u8   use_simpler_join;
  // Twee pop-join base discipline (THVM_ATP_POP_RULES_ONLY): the
  // pop-time join verdict's base normalize runs ORIENTED RULES ONLY
  // (Twee Rule.hs `simplify` / simplifyOverlap; no ungated doE) and the
  // CP-set-ir sweep's per-CP re-normalize does the same (Twee
  // simplifyPassive).  Unorientable equations still join CPs, but only
  // through the simplerThan-gated pass (use_simpler_join).  Default
  // OFF: WM's ungated doR+doE pop treatment, byte-identical.
  u8   use_pop_rules_only;
  // Twee Join.hs ground-joinability CASE-SPLIT at pop (opt-in,
  // THVM_ATP_POP_GJ / --pop-gj): a popped CP whose R+E normal forms
  // stay distinct is tested by the faithful port of Twee's
  // groundJoin/groundJoinFrom loop (Join.hs:173-256, commit d425378):
  // extract a total-order model from the branch context, join under it
  // with model-parameterised rewriting, weaken the model to the
  // weakest still-joining chain, and case-split on the negation of its
  // literal diagram (<, = per link); `=` links unify the tied atoms
  // and recurse on the substituted instance.  Default OFF:
  // byte-identical engine.
  u8   use_pop_gj;
  // Case-ordering cap for use_pop_gj: max models examined per CP
  // (Twee --ground-joining-limit, cfg_ground_join_limit Join.hs:26).
  // 0 = unlimited (Twee's default).  NOT the same thing as
  // THVM_ATP_GJ_BUDGET (that one counts enumeration branches of the
  // legacy gj_cover driver and WEAKENS its exhaustive default).
  u32  pop_gj_limit;
  // CPs discarded by the pop case-split (bench stats line).
  u32  n_cps_dropped_pop_gj;
  // Total case-ordering models examined by the pop case-split.
  u32  n_pop_gj_ticks;
  // CPs dropped by the simpler-join pass (push treat + pop select).
  u32  n_cps_dropped_simpler_join;
  // Count of facts sterilized by the backward walk (bench stats line).
  u32  n_facts_bwd_ground_joinable;
  // Count of rules DELETED from R by the use_gj_delete_rule port.
  u32  n_facts_gj_deleted;
  // Count of rules collapsed by use_full_interreduce (drop+requeue).
  u32  n_full_ir_collapsed;
  // Per-rule-slot WM gzfbStatus cache (general.h:94-95 GZ_statusT,
  // "Reihenfolge wichtig" -- order matters: backward skips
  // status <= GZ_aussichtslos = {joinable, valuable, hopeless}).
  // Grows/compacts in lockstep with lhs[]/rhs[] (atp_ensure_rule_cap /
  // the interreduce shift loop); reset per slot in atp_push_rule.
  u8  *r_gj_status;
  // Overlap-exhausted bit (use_overlap_exhaust only): set once an
  // UNORIENTABLE equation slot has enumerated its full CP batch at birth.
  // A WM fact forms its superposition lane exactly once; thereafter a
  // newly-added fact does NOT re-superpose against it (WM keeps the
  // re-derived fresh copy in the overlap set, not the stale original).
  // Grows in lockstep with lhs[]/rhs[]; reset per slot in atp_push_rule.
  u8  *r_overlap_done;
  // GJ-driver victim exclusion (WM DarfNichtReduzieren): rule slot the
  // gj_rewrite_step loop must skip.  ATP_GJ_NO_EXCLUDE when inactive
  // (the forward CP-drop path -- byte-identical to pre-port).
  u32  gj_exclude;
  // Dreieck root-protection anchors: the victim's ORIGINAL sides.  A
  // root step on a protected side fires only via a rule whose LHS is
  // STRICTLY more general than the anchor (WM DreieckOK,
  // Grundzusammenfuehrung.c:235-240).  0 = side unprotected.
  Term gj_protect_l;
  Term gj_protect_r;

  // === Waldmeister CP-generation filter knobs (KPFilterErgaenzen,
  // INF/Unifikation1.c:1947-2014).  WM registers a chain of per-overlap
  // filters; Weggefiltert (Unifikation1.c:967-980) drops a candidate CP
  // when any filter rejects (Vater, VaterStelle, Mutter), Vater being
  // the fact whose LHS is overlapped, VaterStelle the overlap position.
  // Each is a default-OFF CLI flag (the unconfigured .pr Orkus run leaves
  // them inactive), so the engine default + the "Waldmeister"* presets
  // stay byte-identical with all of these off. ===

  // -einsstern "nur kritische Paare an 1* betrachten" (only consider
  // critical pairs at 1*, the leftmost-argument spine).  EinsSternUeber-
  // lappung (Unifikation1.c:1039-1055) keeps a CP iff its overlap
  // position si lies on the "1*" spine of Vater's LHS s: AnEinsSternIn
  // (:1028-1036) descends from the root taking the FIRST subterm
  // (TO_ErsterTeilterm) repeatedly and returns TRUE iff it reaches si.
  // In thvm's CriticalPair geometry that is exactly: every recorded
  // child index is 0 (pos[d] == 0 for all d; pos_len == 0 = root is
  // trivially on the spine).  WM's existential-goal branch (the s/Mutter
  // top == SO_EQ cases at :1043-1047) is inert on the ground-goal
  // corpus.  Default OFF (-einsstern defaultWert FALSE,
  // RUN/Parameter.c:250); ON gates CP generation -- NOT in the
  // "Waldmeister"* presets (faithful default is off on both sides).
  // Method "Einsstern" / thvm_atp_set_use_einsstern /
  // THVM_ATP_EINSSTERN.
  u8   use_einsstern;
  u32  n_cps_dropped_einsstern;

  // -nusfu "nicht unterhalb von Skolemfunktionen ueberlappen" (no overlap
  // below skolem functions).  NusfUeberlappung (Unifikation1.c:1082-1090)
  // calls Nusfu(si, s) (:1063-1079), which walks Vater's LHS flatterm and
  // for every skolem function symbol checks whether the overlap position
  // si lies physically inside its argument subterm -- returning FALSE
  // (drop) when it does.  WM's SO_IstSkolemSymbol (SymbolOperationen.h:
  // 150-151) = a function symbol that is neither an extra-constant nor
  // the equality symbol; skolem functions only arise from existential-
  // conjecture skolemization (the FOL layer), so on the ground-goal
  // corpus no symbol is a skolem and the filter is inert.  thvm marks
  // skolem labels explicitly via thvm_atp_add_skolem_label
  // (skolem_labels[] below, empty by default), and the gate drops a CP
  // when a skolem label appears on the i-face LHS path ABOVE the overlap
  // position.  WM's -nusfu CLI default is TRUE (Parameter.c:264), but it
  // is registered only when the flag is read at U1_InitAposteriori and is
  // inert without skolem symbols; the thvm Method default is OFF so the
  // engine stays byte-identical, with the registry empty it is a no-op
  // even when ON.  Method "NoOverlapBelowSkolem" /
  // thvm_atp_set_use_no_overlap_below_skolem /
  // THVM_ATP_NO_OVERLAP_BELOW_SKOLEM.
  u8   use_no_overlap_below_skolem;
  u32  n_cps_dropped_nusfu;
  // Skolem-function label registry consulted by the nusfu gate.  Empty
  // on every ground-goal path (no symbol is a skolem there); a caller /
  // test registers labels via thvm_atp_add_skolem_label.
#define ATP_MAX_SKOLEM_LABELS 64u
  u32  skolem_labels[ATP_MAX_SKOLEM_LABELS];
  u32  n_skolem_labels;

  // -reclas "reclassification of unselected equations during SUE-"
  // (the CP-set interreduction sweep).  ReClassifyProc / C_ReClassify
  // (CLAS/NewClassification.c:226/398-430) re-derive w1 (keeping w2 ==
  // the FIFO age) for a CP TOUCHED by the CP-set IR sweep,
  // KPV_KPMengeInterreduzieren (INF/KPVerwaltung.c:996-1008), using the
  // -reclas criteria list.  The default InfoString is "" so
  // ReClasCriteria is empty and the reweight reduces to the base
  // C_Classify (no extra reclassification) -- AND the sweep that calls
  // it (-ki/-cpi) is itself default OFF (CPSetInterreduce -> False in
  // the thvm presets).  Distinct from DemoteOnLhsSimplify, which is the
  // rule-DEMOTE victim requeue (KPV_IROpferBehandeln) -- a different
  // mechanism.  Exposed for coverage; OFF by default and inert unless
  // CPSetInterreduce is also enabled.  Method "Reclassify" /
  // thvm_atp_set_use_reclassify / THVM_ATP_RECLASSIFY.
  u8   use_reclassify;

  // -kern "auf den Kopf gestellte Vervollstaendigung" (head-stand /
  // reversed completion).  KernUeberlappung (Unifikation1.c:1243-1268)
  // is a CP filter gated on the consultation stage KO_Stufe (Konsulat.c
  // combinator escalation) and the existential backward-reasoning
  // apparatus (IstVerdrehterKombinator / IstRechtsInZiel) -- it only
  // fires on the combinator/existential-goal lane (PM_Existenzziele),
  // VACUOUS on the ground-goal comparison surface (thvm routes
  // existential conjectures through its narrowing lane, a separate
  // architecture).  Exposed for coverage; OFF by default and inert on
  // ground goals.  Method "ReversedCompletion" /
  // thvm_atp_set_use_reversed_completion / THVM_ATP_REVERSED_COMPLETION.
  u8   use_reversed_completion;

  // -sue "SUEM" (Selected-Unselected-Equation Management).  The -sue
  // InfoString (default "", RUN/Parameter.c:145) only configures which
  // SUE statistics module is reported (SUE_ParamInfo, the sueModule
  // fall-through in RUN/Parameter.c:786-833); it carries no trajectory-
  // affecting parameter of its own (the actual queue economics are the
  // -clas/-pq/-ki knobs, already covered by CriticalPairWeight /
  // SelectionRatio / CPSetInterreduce).  Exposed for coverage; OFF by
  // default and a pure statistics selector.  Method "SUEManagement" /
  // thvm_atp_set_use_sue_management / THVM_ATP_SUE_MANAGEMENT.
  u8   use_sue_management;

  // -cg "interreduction of critical goals" (KPV_CGMengeInterreduzieren,
  // INF/KPVerwaltung.c:835-849) and -cgclas "classification of critical
  // goals" (the CG analog of -clas).  Both act ONLY on the critical-goal
  // heap, which is gated on PM_Existenzziele (a conclusion containing a
  // variable, WASIC/SymbolOperationen.c:315-326): every conclusion in
  // the ground-goal corpus is skC*-ground, so the CG heap is empty and
  // the lane is provably INERT (inventory row B/G, double vacuity:
  // -cg default "" never schedules SP_IstStichpunkt, and the heap it
  // would walk is empty).  Exposed for coverage; both OFF/inert by
  // default.  Methods "CriticalGoalInterreduce" / "CriticalGoalWeight" /
  // thvm_atp_set_use_critical_goal_interreduce /
  // thvm_atp_set_use_critical_goal_weight / THVM_ATP_CG_INTERREDUCE /
  // THVM_ATP_CG_WEIGHT.
  u8   use_critical_goal_interreduce;
  u8   use_critical_goal_weight;

  // -back "Rueckwaertsargumentieren bei kritischen Zielen" (backward-
  // argue critical goals).  RueckwartigeUeberlappung
  // (Unifikation1.c:1313-...) is registered alongside the combinator
  // apparatus (TO_KombinatorapparatAktivieren + RichtigeZielseiteSuchen,
  // :2005-2008) and drives WM's backward critical-goal reasoning -- the
  // existential / CG-paramodulation lane, against which thvm has no
  // comparable baseline (existential conjectures take thvm's narrowing
  // budget).  Exposed for coverage; OFF by default and inert on the
  // universal/ground-goal surface.  Method "BackwardGoalArgue" /
  // thvm_atp_set_use_backward_goal_argue / THVM_ATP_BACKWARD_GOAL_ARGUE.
  u8   use_backward_goal_argue;

  // Limited Resource Strategy (Riazanov & Voronkov, JSC 36, 2003).  When
  // a wall-clock budget is set, LRS estimates from the observed selection
  // rate how many MORE CPs the saturator will pop before the deadline
  // (rate and remaining budget on the work clock -- process CPU time by
  // default -- the load-blind CASC shape of Vampire's instruction-count
  // estimate; see atp_lrs_recompute_horizon), then prunes the queue of
  // CPs heavier than the predicted-reachable weight -- the saturator
  // only ever reaches the lightest `predicted_remaining_selections` CPs,
  // so the heavy tail wastes index / normalize / heap effort it never
  // spends.
  // Soundness: discarded CPs are unreachable in budget, so the proof (if
  // found) does not depend on them -- the same "incomplete in principle,
  // complete in budget" tradeoff Vampire ships.  Default OFF: every field
  // stays 0 and thvm_atp_select_cp is byte-identical to the milestone-9
  // path.
  // Flipped on by Method -> {... "LRS" -> True} via
  // thvm_atp_set_use_lrs, or by setting THVM_ATP_LRS=1 at init.
  u8   use_lrs;
  u64  lrs_start_us;              // epoch (atp_work_now_us at first select)
  u64  lrs_start_wall_us;         // epoch on the wall clock: the simulated
                                  // CPU budget is deadline - this (under
                                  // a deterministic work clock, see
                                  // wall_budget_us)
  u32  lrs_recompute_period;      // selections between horizon recomputes
  u32  lrs_warmup_selections;     // selections before the first horizon
  u32  lrs_last_recompute_at;     // cp_select_count at the last recompute
  u32  lrs_horizon;               // current weight cutoff (0 == no cutoff)
  u32  n_cps_dropped_lrs;         // diagnostics: CPs pruned by horizon
  u32  n_lrs_recomputes;          // diagnostics: horizon recomputations

  // ENIGMA Tier 2: GNN-guided CP-queue re-rank.  When gnn_rerank_period
  // is non-zero AND a GNN model is loaded (thvm_atp_gnn_loaded), the
  // saturation loop calls thvm_atp_gnn_rerank every gnn_rerank_period
  // selections to permute the queue by the network's proof-relevance
  // score.  Default OFF (period 0): the step path is byte-identical to
  // the unguided engine.  Flipped on by THVM_ATP_GNN_RERANK_PERIOD at
  // init or by thvm_atp_set_gnn_rerank_period.
  u32  gnn_rerank_period;         // selections between GNN re-ranks (0 = off)
  u32  n_gnn_reranks;             // diagnostics: GNN re-rank invocations
  // Parental pre-filter: cap the number of CPs the GNN forward scores per
  // re-rank to the top gnn_topk by the CHEAP score (the Tier-1 logit if
  // loaded, else the classical cp_pri, read without unpacking terms), so a
  // re-rank costs O(gnn_topk) forwards no matter how large the live queue
  // grows (queues reach millions).  0 = score the whole queue (the phase-1
  // behavior).  THVM_ATP_GNN_TOPK sets it; default 512 once a GNN is armed.
  u32  gnn_topk;
  // ENIGMA coop: when set, the GNN re-rank writes the SECONDARY priority
  // (cp_pri2) instead of the primary heap (cp_pri), so the GNN drives the
  // w2 coop dimension -- every w2_modulo-th selection picks the GNN's top
  // CP while the primary heap stays the (e.g. Waldmeister) hand heuristic.
  // This is "GNN as a coop queue with the WM preset": set gnn_coop=1 and
  // w2_modulo=ratio.  When 0, the re-rank overwrites cp_pri (GT-mode).
  u8   gnn_coop;                  // GNN re-rank targets cp_pri2 (coop) not cp_pri
  // Selection-time top-K re-ranker state (src/atp/topk.c, an AtpTopk):
  // NULL unless THVM_ATP_RERANK_TOPK / THVM_ATP_RERANK_MODEL arm it.
  void *topk;
  // ENIGMA selection layer state (src/atp/guide.c, an AtpGuide): NULL
  // unless THVM_ATP_GUIDE or thvm_atp_guide_arm arms it.
  void *guide;

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
  // clause flow): WM `KPBehandelt` (INF/KPVerwaltung.c:439-467) gated by
  // `lohntSichBehandlung` = combined RAW size < 50 (:435-438).  BELOW the
  // gate, the CP is treated at generation: doR-only full-R normalize
  // (-kg "r") + joined-drop; survivors queue and weigh on the TREATED
  // form.  AT OR ABOVE the gate there is NO treatment at all --
  // KPBehandelt returns FALSE without touching the pair, the caller
  // queues the RAW overlap and `recentCPinsert`'s C_Classify (:396)
  // weighs the RAW sides, so deep-overlap instances stay buried at raw
  // weight until the heap (weight order or the FIFO dimension) genuinely
  // reaches them.  Joinability for the raw class is decided at selection
  // (the `kbo_eq(l, r)` check after the pop normalize that ALREADY
  // runs), so no non-joinable CP is ever dropped -- completeness is
  // unchanged.  The raw class also bypasses the auto-MaxWeight stash
  // (see atp_cp_heap_push): WM has no deferral lane, and the stash has
  // no FIFO dimension, so parking raw CPs there would swallow the class.
  // Default OFF (engine byte-identical: eager push-time normalize). On in
  // the bench WM preset + Method "LazyNormalize" -> True via
  // thvm_atp_set_use_lazy_normalize.
  u8   use_lazy_normalize;
  // DISCOUNT given-clause discipline (opt-in, THVM_ATP_DISCOUNT / --discount).
  // Rides the lazy-push path (implies use_lazy_normalize) but forces the WM
  // KPBehandeln size gate to 0, so EVERY critical pair queues RAW (no
  // formation-time doR normalize, no push-time joined-drop) -- passive
  // clauses stay in their formation-time form and are only R+E normalized /
  // joined at activation (pop), the Vampire/E DISCOUNT saturation shape.
  // Completeness-preserving: raw CPs are never dropped at push (joinable is
  // decided at pop), and the FIFO age dimension keeps the queue fair.
  // Default OFF (engine byte-identical). See thvm_atp_set_discount.
  u8   use_discount;
  u64  n_cps_push_normalized;     // diagnostics: full-R normalizes at push

  // -auto combinator-definition force-orientation.  WM -auto orients a
  // variable-duplicating combinator definition (S x y z = (xz)(yz)) as a
  // DIRECTED rule and applies it unconditionally, which no reduction order
  // can (multiset var-condition fails).  When set, thvm_atp_orient_and_add
  // detects the combinator redex on either side (bench .pr keeps it on the
  // LHS; the WL wire SpezNormierung flips it to the RHS) and force-orients
  // redex -> reduct with r_orient=1 so goal normalization applies it.
  // Default OFF (engine byte-identical); on for the WM -auto preset +
  // THVM_ATP_FORCE_DEF_ORIENT env.  The criterion excludes symmetric axioms
  // (nand on both sides) and self-referential fixpoints, so non-combinator
  // theorems are untouched.
  u8   use_force_def_orient;

  // FVI rule emission (Waldmeister `RechtsUnfreiErzeugen`,
  // RUndEVerwaltung.c:366-397): when an unorientable equation is added
  // to R, also push a grounded sibling that substitutes
  // ATP_RESERVED_LABEL_MIN_CONST for every free RHS variable absent
  // from the LHS.  Unblocks FVI-gated theorems (ExcludedMiddle,
  // Noncontradiction, EqualityOfInverses).  Default OFF for byte-
  // identical baseline; ON for Method->"Waldmeister" via
  // thvm_atp_set_use_fvi or THVM_ATP_FVI env.
  u8   use_fvi;

  // Set-of-Support (SOS) heuristic: bias CP priority toward CPs whose
  // terms share symbols with the goal.  Sound (completeness preserved
  // -- no CP is dropped; only the heap key is reduced for goal-touching
  // CPs so they surface earlier).  Wired to Method "SetOfSupport".
  // Default OFF.
  u8   use_sos;
  u32  goal_sym_mask[8];          // bit-set of goal symbols (CTR ext labels)
                                  // -- 8 * 32 = 256 distinct labels indexable;
                                  // cap with mod for larger.
  // Structural goal-relevance CP selection (THVM_ATP_GOAL_SUBTERM,
  // default OFF): a genuinely different metric from SOS (symbol-level,
  // useless on a single-operator signature) and CPinGoal
  // (whole-side match, slow) -- discount a CP's priority by the size of
  // the LARGEST ground SUBTERM it shares with the goal's normal forms,
  // surfacing CPs that operate on the goal's specific substructure.
  // Cheap (a subterm-set scan, no unification).  Sound: only heap
  // ordering changes, nothing dropped.
  u8   use_goal_subterm;
  u8   goal_sub_ready;            // goal-subterm set collected for this goal
  u16  goal_sub_n;               // number of distinct ground goal subterms
  u64  goal_sub_hash[128];        // structural hashes of the goal's ground subterms
  u16  goal_sub_size[128];        // parallel: symbol count of each

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

  // Waldmeister remove-and-re-derive root-overlap ownership.  When an
  // E-member is removed by E-set subsumption (atp_eset_subsume_by_new)
  // and an equation of the SAME shape is later re-derived, WM does NOT
  // re-enumerate the root-x-root overlaps that the subsuming rule's
  // ORIGINAL batch already formed against the removed equation -- that
  // root overlap is "owned" by the original batch.  thvm's flat-subsume
  // path reproduces the remove/re-derive lifecycle, so it must reproduce
  // the ownership too.
  //
  // The cutoff is the SUBSUMING rule's FINAL birth trace.  That trace is
  // NOT yet stamped at subsumption time (atp_eset_subsume_by_new runs
  // inside orient_and_add, before the step loop stamps r_trace), so the
  // subsumer's lhs/rhs SHAPE is saved per dead slot instead; the cutoff
  // is resolved lazily at re-derivation time by locating the live rule
  // of that shape and reading its (now final) r_trace.  When a fresh rule
  // kbo_eq-matches dead slot k's saved shape, that rule's r_rederive_cut[]
  // is set to the resolved cutoff; in atp_overlap_ij the re-derived rule
  // suppresses root overlaps against any older rule whose birth trace is
  // <= the cutoff.  ATP_TRACE_NONE in r_rederive_cut means "not
  // re-derived"; a 0 r_dead_subsumer_lhs slot means "not subsumption-
  // removed".  Only populated under use_flat_subsume, so non-flat-
  // subsume runs are untouched.
  Term *r_dead_subsumer_lhs;      // per dead slot: subsuming eq lhs (GC-rooted)
  Term *r_dead_subsumer_rhs;      // per dead slot: subsuming eq rhs (GC-rooted)
  u32 *r_rederive_cut;            // per rule: cutoff (NONE = not re-derived)

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

  // Waldmeister-faithful interreduction-victim demotion
  // (`KPV_IROpferBehandeln`, INF/KPVerwaltung.c:514-528, drained by
  // `IR_PufferAuslesen`, INF/Interreduktion.c:387-392).  When set, a
  // rule/equation that interreduction demotes is NOT re-queued
  // immediately with its slice-reduced sides; the victim's ORIGINAL
  // (lhs, rhs) is buffered and drained only after the new fact's
  // critical pairs are generated -- WM's work order (ArbeitsAufnahme,
  // INF/Hauptkomponenten.c:308-331) makes the IR buffer the LAST
  // queue mutation of the step, so the victim's FIFO age lands AFTER
  // every CP the new fact produced.  At drain time each victim gets
  // WM's `KPBehandelt` treatment under the default `-kg r` flags
  // (KPVerwaltung.c:435-467): below the lohntSichBehandlung gate
  // (combined size < 50 symbols) both sides are normalized against
  // the full CURRENT rule set with ORIENTED rules only (doR -- WM's
  // Regelbaum; equations never rewrite at requeue treatment), and a
  // victim whose sides join is discarded outright.  Survivors enter
  // the queue with a fresh heuristic weight and a fresh FIFO age.
  // Default OFF: victims re-queue during interreduce with the
  // slice-reduced pair (engine byte-identical).  Wired to Method
  // {"DemoteOnLhsSimplify" -> True} (in the "Waldmeister" presets)
  // via thvm_atp_set_use_wm_demote.
  u8    use_wm_demote;
  // firstdiv-11 probe (env THVM_ATP_KEEP_LHS_REDUCIBLE, default OFF): keep an
  // LHS-reducible oriented rule live instead of dropping it at the always-on
  // interreduce site, to test whether dropping -auto rule 3 ranks the carrier late.
  u8    keep_lhs_reducible;
  // Overlap-exhausted-equation gate (env THVM_ATP_OVERLAP_EXHAUST, default
  // OFF): a newly-added fact does not re-superpose against an old
  // unorientable equation whose birth-batch already enumerated its CP set
  // (WM forms a fact's superposition lane once).  See r_overlap_done.
  u8    use_overlap_exhaust;
  // Waldmeister CP-formation FIFO lineage (env THVM_ATP_FORMATION_FIFO,
  // DEFAULT OFF).  The faithful-stack master knob.  CP formation itself
  // emits natively in WM's U1 order (the single-walk former), so this now
  // arms only the shared WM order refinements outside formation: the
  // IR-victim drain-order keys (use_drain_chainpos / use_drain_revface)
  // and the distinguished-direction E-set subsumption (use_eset_distdir).
  // Re-classification (atp_cp_set_interreduce) preserves cp_seq and
  // re-derivation (atp_wm_demote_drain) re-stamps it, mirroring WM's
  // C_ReClassify ("w2 wird nicht geaendert") / KPV_IROpferBehandeln.
  u8    use_formation_fifo;
  // Waldmeister IR-victim drain within-leaf chain tiebreak (env
  // THVM_ATP_DRAIN_CHAINPOS, DEFAULT OFF; also turned ON under
  // use_formation_fifo).  WM drains the IR REPuffer that GMInterred /
  // RMLinksInterred filled (Interreduktion.c:288-339) via
  // RE_forGleichungenRobust / RE_forRegelnRobust = BK_forRegelnRobust
  // (DSBaumKnoten.h:482-495): leaves in BK_NachfBlatt order, and WITHIN a
  // leaf the fact chain head-first following TP_Nachf -- i.e. the leaf's
  // chain order (PREPEND on insert => newest first, so head-first = chain
  // index 0,1,2..).  atp_wmo_victim_drain_key keyed victims only by their
  // leaf-list rank, so two victims sharing a leaf (the w=224 nested
  // cube-mirror pair at soa pick 1505: traces 2570 at chainpos 2 and 2817
  // at chainpos 1 in equation leaf rank 1) collided on one key and the
  // stable sort fell back to thvm's slot-scan push order -- the OPPOSITE
  // of WM's chain order.  This gate folds the victim's within-leaf chain
  // index into the drain key as the low-order tiebreak (after leaf-list
  // rank), so same-leaf victims drain head-first like BK_Regeln -> Nachf.
  // OFF byte-identical; advances soa firstdiv 1505 -> beyond.  See
  // tools/baselines/wm_align_reports/soa.txt.
  u8    use_drain_chainpos;
  // Waldmeister GMInterred reducible-face drain order (env
  // THVM_ATP_DRAIN_REVFACE, DEFAULT OFF; also turned ON under
  // use_formation_fifo).  GMInterred fills the IR REPuffer via
  // RE_forGMReferenzen = BK_ReferenzDurchlauf (DSBaumKnoten.h:499-514),
  // which pulls each equation through the face the new object REDUCES
  // (NF_ObjektAnwendbar tested per Gleichungsbaum twin), not its
  // distinguished (LRSortieren) face.  atp_wmo_victim_drain_key keyed
  // victims by their face-0 leaf only, so a pair whose distinguished faces
  // coincide (the soa cube-mirror `x.x = cube` pair at pick 1558: traces
  // 2092/-14 and 3089/-18 both index `x.x` in equation leaf rank 1)
  // collided and fell back to chain order, draining 3089 before 2092 --
  // the OPPOSITE of WM, whose distinct reducible cube faces sit in leaves
  // 14 (2092) and 15 (3089), so WM drains 2092 first.  This gate ranks the
  // victim by its reduced side's wmo face (reduced_thvm_side ^ dist_rhs)
  // leaf-list position, matching GMInterred.  OFF byte-identical; advances
  // soa firstdiv 1558 -> beyond.  See tools/baselines/wm_align_reports/soa.txt.
  u8    use_drain_revface;
  // WM-faithful distinguished-direction E-set subsumption (env
  // THVM_ATP_ESET_DISTDIR, default OFF; also turned ON under
  // use_formation_fifo).  thvm's flat new-equation E-subsumption
  // (atp_wm_flat_subsumes_pair) is 4-WAY: it tries both PATTERN orientations
  // AND both SUBJECT orientations of the old equation.  WM's
  // GMSubsummierenMitGleichung (INF/Interreduktion.c:251-279) gates the subject
  // on TP_RichtungAusgezeichnet (:261) so it tests each old equation ONLY in its
  // distinguished (stored) orientation -- never the reversed face -- while
  // SS_TermpaarSubsummiertTermpaar (Subsumption.c:104-110) still tries both
  // pattern orientations.  The two subject-swapped combos are thvm's over-match.
  // With them dropped (atp_wm_flat_subsumes_pair_distdir) the Meredith eqn-12
  // content `a.((a.b).c) = a.(c.(b.a))` (and its sibling) -- which the 3-var-swap
  // `x.(y.z) = (z.y).x` subsumes only in eqn-12's REVERSED orientation -- survives
  // exactly as WM keeps it, with NO content gate.  soa relies on the 4-way ONLY
  // in that it must agree with the 1-way: across the whole soa run the subject-swap
  // combos fire ZERO times, so soa stays byte-identical (firstdiv 2808, CPSEL
  // unchanged).  Advances Meredith firstdiv 6078 -> 6110 (better than the gate's
  // 6081: the faithful test also retains the eqn-12 sibling).
  u8    use_eset_distdir;
  // WM-faithful discrimination-tree construction (env THVM_ATP_WM_TRIE_FAITHFUL,
  // default OFF; also turned ON under use_formation_fifo).  WM's
  // AltesBlattPolieren (DSBaumOperationen.c :523-526) ALWAYS splices a
  // BlattAufgeteilt parallel jump immediately AFTER the surviving model in the
  // start node's outgoing list ("hinter den Eintrag setzen"); thvm's historical
  // path head-inserts the parallel in one special case (POLIER-PAR-HEAD), which
  // makes the runtime tops DFS reach a split leaf's parallel face at the wrong
  // arrival rank.  At soa rule-35's tops batch this scatters a partner family
  // across leaf arrivals (the B-leaf `dot(v1,dot(v2,v1))` is visited at arrival
  // 6 instead of WM's arrival 8, ahead of the two deeper function/jump leaves),
  // grouping a round-robin batch (picks 1953-1960).  This gate restores WM's
  // splice-after construction so the DFS leaf-arrival order matches WM's.
  // OFF byte-identical (the gated branch keeps the head-insert).  See
  // tools/baselines/wm_align_reports/soa.txt.
  u8    use_wm_trie_faithful;
  // Waldmeister equation-tree INSERTION ORDER (env THVM_ATP_WMO_INSERT_LR,
  // default OFF).  WM's RUndEVerwaltung GleichungEinfuegen inserts an
  // unorientable equation's two faces into the Gleichungsbaum as flat(l)
  // THEN flat(r) -- stored-lhs face first, stored-rhs face second
  // (RUndEVerwaltung.c GleichungEinfuegen).  thvm's atp_wmo_insert_fact_ex
  // historically inserted the WM-DISTINGUISHED face first, which for a
  // CP-derived equation with dist_rhs=1 (distinguished = stored rhs) is the
  // STORED-RHS face -- the opposite order.  Because BlattAufgeteilt is
  // order-sensitive (the second face splits the first leaf and splices its
  // jump AFTER), inserting rhs-first inverted the two faces of soa's 3rd
  // axiom at the depth-1 d node, which (via the AltesBlattPolieren parallels
  // the E29/E30-family leaves splice after) flipped the rule-35 round-robin
  // emission order (firstdiv 1953: the 8 weight-109 collapse CPs all form at
  // pos[0]/k1=1; only their eq-tree DFS-arrival groups diverge, and the E30
  // group must arrive before the E29 group as WM emits A,B,C round-robin).
  // This gate inserts lhs-first/rhs-second like WM; the dist_rhs bit STILL
  // drives the rank-key face remap, now carried purely by each leaf chain's
  // stored WM-face LABEL (lhs leaf = dist_rhs, rhs leaf = !dist_rhs), so the
  // wmo_tops_rank/wmo_leaflist_rank partner lookup is byte-identical to the
  // prior dist-first registration -- only the physical tree exit order
  // changes.  OFF byte-identical.  See tools/baselines/wm_align_reports/soa.txt.
  u8    use_wmo_insert_lr;
  // Waldmeister LRSortieren side-canonicalisation (SpezNormierung.c
  // :517-534, env THVM_ATP_LR_SORTIEREN, default OFF): a derived
  // unorientable equation is stored with WM's canonical left/right side
  // order (variable < non-variable, preorder).  DEFAULT OFF -- a blanket
  // re-sort regresses the Sheffer OrAssociativity prefix (WM keeps the
  // CP-formation side order for most derived E-members; only intake
  // axioms run LRSortieren).  Opt-in only.  See atp_lr_sortieren_rec.
  u8    use_lr_sortieren;
  // Waldmeister col3 combinator ordering (Ordnungen.c:101-117 +
  // TermOperationen.c IstKombinatorTupel; Automodus strategy KombS =
  // rose() for the Kombinatorlogik classes): s > t iff s is a
  // combinator redex `C x1 .. xn` on the apply spine (C a constant,
  // x1..xn pairwise-distinct variables) and t is built only from the
  // apply operator plus symbols occurring in s, OR s is a bare
  // constant and t is ground.  Orients the duplicating combinator
  // definition axioms ((W x) y -> (x y) y) no simplification order
  // can.  col3_apply = the applicative binary label (WM
  // TO_ApplyOperator).  0 = off (default).
  u32   col3_apply;
  // Waldmeister CP-formation side geometry (Unifikation1.c:916-917, env
  // THVM_ATP_CP_SIDE, default OFF): store each derived UNORIENTABLE
  // equation with WM's geometric side order -- KPLinks = sigma(r_i) (the
  // overlapped rule's RHS, WM's distinguished face) as the stored LHS,
  // KPRechts = sigma(l_i[p<-r_j]) (the reduct) as the stored RHS.  thvm's
  // popped CP carries the OPPOSITE order; swapping it matches WM's stored
  // term structure so the equation's own CP batch overlaps the same redex
  // set (Sheffer OrAssociativity C-shape).  When set, atp_wmo_eq_dist_rhs
  // returns the FLIPPED flag (distinguished = stored LHS) so the emission
  // mirror stays consistent.  Unlike use_lr_sortieren this is a geometry
  // swap, not a structural size sort.
  u8    use_cp_side;
  // IR-victim buffer (use_wm_demote only): original sides + the
  // TRACE_SIMPLIFY parent captured at drop time.  Filled by
  // thvm_atp_interreduce, drained by thvm_atp_step after CP
  // generation -- but rooted in thvm_atp_gc_collect because the
  // heap-pressure GC can run inside generate_cps between the two.
  Term *irv_lhs;
  Term *irv_rhs;
  u32  *irv_parent;
#ifdef THVM_ATPFT_RULES
  // Victim FT spans (clones of the row's live mirrors, captured at
  // push time) so the drain's doR treatment runs FT-native -- no
  // ft_from_term re-encode, and a joined victim decodes nothing.
  // NULL when the push site had no mirror; the drain then falls back
  // to the Term route.  Cells live in Arena A (address-stable, freed
  // wholesale at thvm_atp_free); slots are consumed or explicitly
  // returned by the drain.
  struct AtpFtCell **irv_lhs_ft;
  struct AtpFtCell **irv_rhs_ft;
#endif
  // WM drain-order key per victim (use_emission_order only), captured
  // at push time BEFORE the victim leaves the wmo tree: WM's
  // IR_PufferAuslesen drains GMInterred (equation victims) before
  // RMLinksInterred (rule victims), each in its discrimination-tree
  // leaf-list order (BK_forRegelnRobust over Baum.ErstesBlatt).  Key =
  // (tree << 31) | leaf_list_rank so a stable sort reproduces that order.
  u32  *irv_wmo_key;
  // Victim's rule formation depth (r_depth) at drop time: the drain's
  // requeue re-enqueues at THIS depth (Twee semantics -- interreduction
  // preserves an active rule's depth; only overlaps increment it).
  u32  *irv_depth;
  u32   n_irv;
  u32   irv_cap;
  u64   n_wm_demote_requeued;     // victims re-queued at drain
  u64   n_wm_demote_joined;       // victims discarded at drain (joined)

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
  // The deadline's budget (us) and the engine's work clock when it was
  // set: under a deterministic work clock (atp_work_fixed) the LRS
  // budget is wall_budget_us less the work done before the LRS epoch,
  // not a wall reading, so it repeats run to run.
  u64  wall_budget_us;
  u64  wall_set_work_us;

  // Process-RSS baseline for the growth-relative memory guard, sampled
  // at the run's FIRST RSS poll (step 0).  0 = not yet sampled (calloc).
  // The guard aborts when the run's own RSS GROWTH -- live RSS minus
  // this baseline -- exceeds THVM_ATP_RSS_ABORT_MB (default 8 GB).  An
  // ABSOLUTE cap misattributed the host's ambient memory to the proof
  // run: inside WolframKernel a cold-JIT TNetTrain transiently lifts
  // ambient RSS past 8 GB, and every following TFindProof insta-aborted
  // at its step-0 poll ("poison 2", the silent post-train $Failed wedge)
  // until the ambient decayed.  Growth attribution keeps the original
  // protection (a diverging saturation still grows multi-GB and trips)
  // while an idle-but-large host process no longer counts against runs.
  long long rss_guard_baseline;

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

  // ENIGMA Tier 2 graph export (record_cp_graphs; DEFAULT 0).  Set
  // alongside record_cp_features: each PROCESSED critical pair also
  // serialises its anonymised hypergraph (thvm_atp_cp_graph) into the
  // growable byte arena below, one self-describing record per recorded
  // feature row (so the i-th record pairs with cp_feat_label[i]).  On a
  // SUCCESSFUL proof thvm_atp_cp_graph_dataset_append walks the arena and
  // writes label + graph per CP as one JSONL line.  OFF (the default) the
  // arena stays NULL and select_cp does no extra work -- byte-identical.
  u8      record_cp_graphs;
  u8     *cp_graph_arena;   // packed variable-length graph records
  size_t  cp_graph_len;     // bytes used in the arena
  size_t  cp_graph_cap;     // arena capacity in bytes
  size_t *cp_graph_off;     // arena offset of the i-th feature row's graph
                            // (SIZE_MAX = skipped: overflow/OOM), parallel
                            // to the cp_feat_* arrays and grown with them
  void   *cp_graph_scratch; // reusable AtpCpGraph build scratch (lazy)

#ifdef THVM_ATPFT_RULES
  // Parallel AtpFt rule storage; see docs/ATP/engineering.md.
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
  // Subsumer SHAPE spans for the remove-and-re-derive locate: exact
  // clones of the subsuming equation's mirrors at kill time (a lazy
  // subsumer's Term slots are sentinels, so the span is the only
  // faithful shape record; real subsumers store them too, keeping the
  // locate on one compare path).
  struct AtpFtCell **r_dead_subsumer_lhs_ft;
  struct AtpFtCell **r_dead_subsumer_rhs_ft;
  struct AtpFtCell **r_dead_lhs_save_ft;
  struct AtpFtCell **r_dead_rhs_save_ft;
  struct AtpFtCell  *goal_lhs_ft;
  struct AtpFtCell  *goal_rhs_ft;
  // Goal-NF decode cache: the flat NFs + their decoded Terms from the
  // previous goal check.  The goal's normal form is content-stable
  // across most checks (it changes only when a new rule reduces it),
  // so when the fresh NF span ft_eq-matches the cached one the check
  // reuses the cached Term instead of ft_to_term -- the last per-step
  // interchange conversion class.  Arena-A cells are never freed once
  // allocated (only fuse/CPQ/stash spans are explicitly returned), so
  // the retained spans stay valid; the cache is invalidated wherever
  // goal_lhs_ft/goal_rhs_ft are (re)assigned.  Terms goal_lhs_nf /
  // goal_rhs_nf (above) are the paired decoded values.
  struct AtpFtCell  *goal_nf_ft_l;
  struct AtpFtCell  *goal_nf_ft_r;
  // Incremental goal check (src/atp/goal_incr.c): the last full
  // normalization of the single universal goal (its normal forms, the
  // walks that reached them and the rules those fired), reused while the
  // same walks would run again.  NULL until the first goal check.
  struct AtpGoalIncr *goal_incr;
  // Cached FT image of s->min_const for the unfailing-instantiation
  // grounding (ft_norm's ftn_ground_subst encoded it fresh per call).
  // Keyed by the Term handle: a GC forward or a min-const change
  // invalidates and re-encodes (rare, safe either way -- the cell is
  // a read-only single-cell constant that substitution application
  // copies).
  struct AtpFtCell  *min_const_ft;
  Term               min_const_ft_src;
#endif

  // ifeq-encoded clause guards (TPTP's '-10' machine-equationalized FOF
  // class): dispatcher symbols detected at axiom intake -- a symbol f
  // whose axiom is f(A,A,B[,C]) = B with A, B (, C) distinct variables.
  // Encoded clauses look like ifeq(guard, true, then, true) = true;
  // plain completion floods on them because superpositions INTO
  // subterms buried under undecided guards dominate the CP queue while
  // only guard-DECIDING inferences make progress (Vampire cracks the
  // family through literal selection: it effectively only resolves the
  // guard).  The equational analog lives in atp_cp_priority_sized_ex:
  // when armed (use_ifeq_guard), a CP's selection weight is scaled by
  // THVM_ATP_IFEQ_PENALTY (default 8) once per UNDECIDED guard its
  // conclusion retains -- a pure function of the CP's terms, so every
  // re-score path (push, interreduce commit, rebuild) recomputes it
  // identically.  A guard-deciding CP drops one guard relative to its
  // parent, surfacing a penalty-factor earlier than its stuck siblings;
  // facts are unscaled.  Selection bias only: nothing is dropped,
  // completeness is untouched.
  // Arming: n_ifeq_syms > 0 (detector fired) AND THVM_ATP_IFEQ_GUARD
  // >= 1.  THVM_ATP_IFEQ=0 kills the detector.  Off (the default) the
  // engine is byte-identical.
  u32  ifeq_syms[8];              // detected dispatcher CTR labels
  u32  n_ifeq_syms;
  u8   use_ifeq_guard;

  // Guard-DECIDING CP boost (the complementary half of the guard
  // penalty above): a CP whose conclusion has the shape `g = true`,
  // where `g` fingerprint-matches a condition subterm registered as a
  // live guard (arg0 of some detected-ifeq application whose arg0 !=
  // arg1 syntactically, seen inside an active rule or the goal), gets
  // its priority scaled DOWN by THVM_ATP_IFEQ_DECIDE -- surfacing
  // exactly the dispatcher-collapsing steps Vampire's literal
  // selection performs for free.  ifeq_true_term is the derived
  // "true" constant (the term compared against at those same guard
  // sites); 0 = not yet derived / no armed dispatcher.
  // ifeq_guard_mask is a 4096-bucket fingerprint membership set (an
  // alpha-invariant structural hash of the guard subterm -- root
  // label + full shape, first-occurrence variable numbering so two
  // occurrences of "the same" guard schema under different rules'
  // local variable numbering fingerprint identically): a sound
  // over-approximation exactly like tgt_sym_mask below, so a
  // collision only ever causes a harmless extra boost, never a missed
  // one.  Populated at rule-add time (atp_push_rule) and goal-set
  // time (thvm_atp_set_goals); never swept on rule death (a stale
  // entry costs a mis-boost at worst -- see atp_ifeq_guard_register).
  // Arming: n_ifeq_syms > 0 (detector fired) AND THVM_ATP_IFEQ_DECIDE
  // armed (see atp_ifeq_decide_armed).  Off (the default) the engine
  // is byte-identical.
  u8   use_ifeq_decide;
  Term ifeq_true_term;
  u64  ifeq_guard_mask[64];

  // tgt: goal-targeted CP-SELECTION weights (the selection essence of
  // Vampire's goal-targeted KBO weight scheme, tgt=full / tgt=ground).
  // Symbols occurring in the conjecture weigh 1, every other symbol
  // weighs THVM_ATP_TGT_NONGOAL (default 2); tgt=ground additionally
  // raises variables to the non-goal weight.  Applied in
  // atp_cp_priority_sized_ex as a multiplicative bias on the CP
  // priority (base * weighted_count / plain_count), so it composes
  // with every weight mode and with the ifeq guard scale.  The
  // reduction-ordering KBO is untouched; selection bias only.
  // Arming: THVM_ATP_TGT_SEL >= 1 and a goal is set.  Off (the
  // default) the engine is byte-identical.  (THVM_ATP_TGT proper
  // selects the tgt PREPROCESSING port, pr_tgt_transform -- a .pr
  // intake transformation, no engine involvement.)
  // tgt_sym_mask is a 4096-bucket
  // bitset of the goal's CTR labels (union over conjuncts; larger
  // labels alias mod 4096 -- a sound over-approximation, a goal
  // symbol is never mis-penalized); collected lazily, invalidated by
  // thvm_atp_set_goals.  tgt_ext_mask holds externally-declared
  // goal-cone labels (thvm_atp_tgt_add_sym: the CLI feeds the TPTP
  // negated-conjecture units' symbols -- Vampire derives tgt weights
  // from the WHOLE negated conjecture, and on the '-10' encodings the
  // final goal equation is a bare `a != b` while the conjecture's
  // content lives in its sibling hypothesis units); the lazy
  // collection ORs it into tgt_sym_mask.
  u64  tgt_sym_mask[64];
  u64  tgt_ext_mask[64];
  u8   tgt_ready;

#ifdef THVM_ATPFT_CPQ
  // Parallel native AtpFt CP queue; see docs/ATP/engineering.md.
  // Each populated slot owns its two FT spans in Arena A; the legacy
  // cp_packed[] byte queue stays populated in parallel (FV index +
  // peek/stash consumers).  Capacity tracks
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
fn u8        thvm_atp_col3_pair   (Term a, Term b, u32 apply);
fn u8        thvm_atp_set_goal    (AtpState *s, Term lhs, Term rhs);
// Multi-goal conjunction: set n conjecture pairs (lhs[g], rhs[g]) to
// be proved off one saturation.  n == 0 clears the goal (completion
// mode).  n == 1 is exactly thvm_atp_set_goal.  Returns 0 on n >
// ATP_MAX_GOALS or any ill-sorted pair (state unmodified).  The
// conjecture symbol/relevance masks become the UNION over all goals'
// symbols.
#define ATP_MAX_GOALS 64u
fn u8        thvm_atp_set_goals   (AtpState *s, const Term *lhs,
                                   const Term *rhs, u32 n);

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
fn void      thvm_atp_set_rpo     (AtpState *s, const RpoConfig *rpo);
fn void      thvm_atp_set_wpo     (AtpState *s, const WpoConfig *wpo);

// Milestone 10: enable/disable the MNF goal-directed front search at
// runtime.  No-op unless the dylib was compiled with -DATP_MNF.  When
// 1, goal_check augments the single-normal-form check with the
// bidirectional MNF collision search; when 0 (default) the single-NF
// check runs alone.  Lets one dylib carry MNF without paying for it
// on completion-only goals.
fn void      thvm_atp_set_use_mnf (AtpState *s, u8 on);
fn void      thvm_atp_set_use_goal_narrow(AtpState *s, u8 on);
fn void      thvm_atp_set_use_narrow_queue(AtpState *s, u8 on);
// Narrowing proof recorder (see AtpState.nrec): arm/free.
fn void      thvm_atp_set_record_narrow(AtpState *s, u8 on);
fn void      thvm_atp_nrec_free(AtpState *s);
fn void      thvm_atp_set_use_goal_drive(AtpState *s, u8 on);
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
fn void      thvm_atp_set_use_lazy_ground_join(AtpState *s, u8 on);
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

// Select the CP-priority weight mode (an `AtpCpWeightMode` value).
// `thvm_atp_init` defaults to ATP_CP_WEIGHT_GT; out-of-range
// values are clamped to ATP_CP_WEIGHT_ADD.  See `AtpCpWeightMode`
// for the per-mode formula, all ports of Waldmeister's
// `ClasHeuristics` module.
fn void      thvm_atp_set_cp_weight_mode(AtpState *s, u32 mode);
fn void      thvm_atp_set_max_cp_weight(AtpState *s, u32 w);
// Twee --max-term-size: per-side CP node cap.  Drop a critical pair when
// EITHER side exceeds `w` (0 = unbounded).  See `max_cp_side`.
fn void      thvm_atp_set_max_cp_side(AtpState *s, u32 w);
// Hard CP-queue-size cap (memory leash): drop new CPs once the live queue
// reaches `n` (0 = unbounded).  Lossy; bounds C memory on runaway saturations.
fn void      thvm_atp_set_max_cp_queue(AtpState *s, u32 n);
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
fn void      thvm_atp_set_use_eager_orphan_sweep(AtpState *s, u8 on);
fn void      thvm_atp_set_use_perm_subsume(AtpState *s, u8 on);
fn void      thvm_atp_set_perm_subsume_mask(u64 mask);
// Waldmeister history-driven Act_ultimate for input axioms.  When on,
// CPs enqueued as TRACE_AXIOM rank strictly before every non-axiom CP
// regardless of heuristic weight (mirrors `initial = ultimate` in the
// default DEF block, NewClassification.c).  Off = engine byte-identical.
fn void      thvm_atp_set_use_initial_ultimate(AtpState *s, u8 on);
fn void      thvm_atp_set_use_intake_order(AtpState *s, u8 on);
// SpezNormierung canonical intake order over explicit axiom / goal
// term arrays (src/atp/wm_intake.c steps 2-5): order_out[r] = input
// index of the axiom at canonical rank r; swap_out[i] = 1 iff axiom
// i's canonical LR form swaps its input sides.  swap_out may be NULL.
// The WL TFindProof encoder calls this (via thvm_wl_atp_wm_intake_order)
// to present axioms in the same order WM's loader feeds them.
fn void      thvm_atp_wm_intake_order(const Term *ls, const Term *rs,
                                      u32 count, const Term *gls,
                                      const Term *grs, u32 n_goals,
                                      u32 *order_out, u8 *swap_out);
// WM normal-form strategy `-nf mixmost` (the CLI default; see
// AtpState.use_mixmost_nf).  Default OFF = the legacy outermost
// rescan walk; the "Waldmeister"* presets turn it ON.
fn void      thvm_atp_set_use_mixmost_nf(AtpState *s, u8 on);
// Waldmeister `database=ultimate` (Parameter.c:166).  When on, CPs
// derived from rule-database overlap also rank ultimate -- creates
// the depth-first bias on newly-derived chains that lets WM crack
// wolfram commutativity in 2.5s.  Off = engine byte-identical.
fn void      thvm_atp_set_use_database_ultimate(AtpState *s, u8 on);
// Incremental CP-set interreduce.  Tracks a watermark rule-count and a
// 64-bit top-symbol bitmap of rules added since.  Skips the per-CP
// normalize when no new rule's top symbol appears in the CP.  Cuts the
// interreduce-time hotspot on wolfram from 56% to <10%.  Soundness:
// only skips CPs proved to have no possible new-rule rewrite.  Off
// by default; engine byte-identical when the flag is off.
fn void      thvm_atp_set_use_incr_ir(AtpState *s, u8 on);
// Deferred-CP (`implicit_pair`) arc: opt-in toggle for the compact
// AtpCpImplicit descriptor storage path.  Rule-x-rule CPs queue as
// trace-backed descriptors; selection materializes the raw pair off
// the TRACE_CP entry.  Default OFF keeps the engine byte-identical.
fn void      thvm_atp_set_use_implicit_cp(AtpState *s, u8 on);
// Raw-descriptor lane (see AtpState.use_raw_desc / use_raw_desc_cert).
// Mutually exclusive with use_implicit_cp: enabling either raw-desc
// mode clears the trace-backed lane and vice versa.
fn void      thvm_atp_set_use_raw_desc(AtpState *s, u8 on);
fn void      thvm_atp_set_use_raw_desc_cert(AtpState *s, u8 on);
// Cert-mode whole-queue verify: re-derive + memcmp every live queued
// slot carrying a descriptor (fatal on mismatch); returns the count.
fn u64       thvm_atp_rawdesc_cert_sweep(AtpState *s);

// === Phase 0: ground congruence closure (QF_UF), src/cc/_.c ===
//
// A standalone Downey-Sethi-Tarjan congruence-closure decision
// procedure over its OWN integer-encoded egraph node ids (not tied to
// the Term/IC heap; a later phase integrates that).  Decides ground
// QF_UF: union-find over subterm nodes + per-class use lists +
// signature-table congruence propagation.  UNSAT iff some asserted
// disequality's two sides are congruent.  Mirrors the WL TSatEUF
// prototype (wl/THVMLink/Kernel/ATP/SMT.wl).
typedef struct CcState CcState;

typedef enum {
  CC_SAT   = 0,
  CC_UNSAT = 1,
} CcResult;

// lifecycle
fn CcState  *cc_init      (void);
fn void      cc_free      (CcState *s);
// term construction (returns an egraph node id)
fn u32       cc_atom      (CcState *s, u32 atom_id);
fn u32       cc_app       (CcState *s, u32 fn_id,
                           const u32 *arg_nodes, u32 nargs);
// assertions
fn void      cc_assert_eq (CcState *s, u32 a, u32 b);
fn void      cc_assert_ne (CcState *s, u32 a, u32 b);
// decision + class-representative readout (for SAT equivalence classes)
fn CcResult  cc_check     (CcState *s);
fn u32       cc_find      (CcState *s, u32 node);
// backtracking: cc_push returns a checkpoint mark; cc_pop undoes every
// destructive op (unions, signature-table edits, use-list prepends,
// node creations, disequality assertions) recorded since that mark.
fn u32       cc_push      (CcState *s);
fn void      cc_pop       (CcState *s, u32 mark);

// === Phase 0: finite-model "ExpressionPrune" enumeration, src/ffmep/_.c ===
//
// The C half of the hybrid "ExpressionPruneC" method of WL
// TFindFiniteModels.  WL clausifies the grounded relation into a clause
// DB (epClauseDB); this engine runs the hot enumeration the WL
// "ExpressionPrune" branch does (pruneSelectTuples + extend + FromDigits)
// so "ExpressionPruneC" is differential-identical to "ExpressionPrune".
//
// Clause DB: a list of clause-sets; each clause-set a list of clauses;
// each clause a list of literals.  A literal "cell c holds value v" is
// encoded as the integer c * k + v, where cell id c = op_off[op] + pos
// numbers every operation-table cell across all operators (op_off[op] =
// cumulative cell offset, op_size[op] = k^arity; ncells = op_off[nops]).
typedef struct {
  u32  k;                // domain size
  u32  ncells;           // total operation-table cells (op_off[nops])
  u32  nops;             // number of operators
  const u32 *op_off;     // [nops+1] cumulative cell offsets
  const u32 *op_size;    // [nops]   table size k^arity per operator
  const i64 *lit;        // all literal-ints, contiguous per clause
  const u32 *clause_off; // [nclauses+1] into lit[]
  u32  nsets;
  const u32 *set_off;    // [nsets+1] into set_clause[]
  const u32 *set_clause; // clause ids grouped per set
} FfmepDb;

// Solve the clause DB; returns a malloc'd i64 buffer of
// (*out_nmodels) * nops ints (row-major), one row per model (per-operator
// FromDigits index, operator order).  max_items < 0 == Infinity; it caps
// the number of solution tuples (pre-extend), mirroring the WL MapTake.
// Caller owns + frees the buffer.  NULL on alloc failure.
fn i64 *ffmep_solve(const FfmepDb *db, i64 max_items, u32 *out_nmodels);

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
// WM -ks "s" pop-time E-subsumption drop (see AtpState.use_pop_subsume).
fn void      thvm_atp_set_use_pop_subsume(AtpState *s, u8 on);
// WM E-set subsumption destroy on new-equation entry
// (GMSubsummierenMitGleichung; see AtpState.use_eset_subsume).
fn void      thvm_atp_set_use_eset_subsume(AtpState *s, u8 on);
// WM flatterm-faithful (over-eager counter-cross) eset-subsume matcher
// (see AtpState.use_flat_subsume).
fn void      thvm_atp_set_use_flat_subsume(AtpState *s, u8 on);
// Commutativity-aware E-set subsumption widening
// (see AtpState.use_comm_subsume).  DEFAULT OFF.
fn void      thvm_atp_set_use_comm_subsume(AtpState *s, u8 on);
// Waldmeister CP-formation FIFO lineage -- the faithful-stack master knob;
// ON it arms the shared drain-order / eset-distdir refinements (see
// AtpState.use_formation_fifo).  DEFAULT OFF.
fn void      thvm_atp_set_use_formation_fifo(AtpState *s, u8 on);
// WM IR-victim drain within-leaf chain tiebreak (see
// AtpState.use_drain_chainpos).  DEFAULT OFF; also turned ON under
// use_formation_fifo.
fn void      thvm_atp_set_use_drain_chainpos(AtpState *s, u8 on);
// WM GMInterred reducible-face drain order (see AtpState.use_drain_revface).
// DEFAULT OFF; also turned ON under use_formation_fifo.
fn void      thvm_atp_set_use_drain_revface(AtpState *s, u8 on);
// WM-faithful distinguished-direction E-set subsumption (see
// AtpState.use_eset_distdir).  DEFAULT OFF; also turned ON under
// use_formation_fifo.
fn void      thvm_atp_set_use_eset_distdir(AtpState *s, u8 on);
// WM-faithful discrimination-tree construction (see
// AtpState.use_wm_trie_faithful).  DEFAULT OFF; also turned ON under
// use_formation_fifo.
fn void      thvm_atp_set_use_wm_trie_faithful(AtpState *s, u8 on);
fn void      thvm_atp_set_use_wmo_insert_lr(AtpState *s, u8 on);
// Push-time queue-vs-queue subsumption gate (thvm-native, no WM
// counterpart; see AtpState.use_queue_subsume).  Default ON; the
// "Waldmeister"* presets turn it OFF.
fn void      thvm_atp_set_use_queue_subsume(AtpState *s, u8 on);
// WM CP-emission-order mirror (FIFO age parity; see
// AtpState.use_emission_order).  Default OFF; the "Waldmeister"*
// presets turn it ON.  Turning it on registers the already-live facts
// in slot order (an approximation of their insertion history when
// enabled mid-run; presets enable it before the axioms load).
fn void      thvm_atp_set_use_emission_order(AtpState *s, u8 on);
// WM backward ground-joinability sterilization (-gj,
// RueckwaertsGrundzusammenfuehrbarkeit; see
// AtpState.use_bwd_ground_join).  Default OFF = WM's -gj default.
fn void      thvm_atp_set_use_bwd_ground_join(AtpState *s, u8 on);
// WM CP-generation filter knobs (KPFilterErgaenzen,
// Unifikation1.c:1947-2014).  All default OFF = the unconfigured .pr
// Orkus run, so the engine + the "Waldmeister"* presets are byte-
// identical with these off.  Einsstern / NoOverlapBelowSkolem are live
// CP-gen gates; the rest are inert on the ground-goal surface (see the
// AtpState field comments).
fn void      thvm_atp_set_use_einsstern(AtpState *s, u8 on);
fn void      thvm_atp_set_use_no_overlap_below_skolem(AtpState *s, u8 on);
// Register a skolem-function label for the nusfu gate.  Empty on every
// ground-goal path; a caller / test adds labels here so
// use_no_overlap_below_skolem has something to act on.
fn void      thvm_atp_add_skolem_label(AtpState *s, u32 label);
fn void      thvm_atp_set_use_reclassify(AtpState *s, u8 on);
fn void      thvm_atp_set_use_reversed_completion(AtpState *s, u8 on);
fn void      thvm_atp_set_use_sue_management(AtpState *s, u8 on);
fn void      thvm_atp_set_use_critical_goal_interreduce(AtpState *s, u8 on);
fn void      thvm_atp_set_use_critical_goal_weight(AtpState *s, u8 on);
fn void      thvm_atp_set_use_backward_goal_argue(AtpState *s, u8 on);
fn void      thvm_atp_set_w2(AtpState *s, u32 modulo, u8 mode);
fn void      thvm_atp_set_use_unorient_index(AtpState *s, u8 on);
fn void      thvm_atp_set_use_lazy_normalize(AtpState *s, u8 on);
fn void      thvm_atp_set_discount(AtpState *s, u8 on);
fn void      thvm_atp_set_use_force_def_orient(AtpState *s, u8 on);
// Waldmeister `RechtsUnfreiErzeugen` FVI rule emission
// (RUndEVerwaltung.c:366-397).  Default OFF; ON makes
// thvm_atp_orient_and_add emit a grounded sibling rule for every
// unorientable equation whose RHS has variables absent from its LHS,
// substituting ATP_RESERVED_LABEL_MIN_CONST for the free variables.
// Required to crack the FVI-gated theorems (ExcludedMiddle,
// Noncontradiction, EqualityOfInverses) under Method->"Waldmeister".
fn void      thvm_atp_set_use_fvi(AtpState *s, u8 on);
// Vampire-style Limited Resource Strategy.  When set together with a
// wall-clock deadline (thvm_atp_set_wall_deadline), thvm_atp_select_cp
// periodically prunes the CP queue of CPs above a budget-derived weight
// horizon -- the saturator concentrates effort on the proof-tractable
// subset it can actually reach.  Sound (incomplete in principle, complete
// in budget); 0 = off (default) -> engine byte-identical.
fn void      thvm_atp_set_use_lrs(AtpState *s, u8 on);
// Vampire Otter/LRS simplification set (src/atp/otter.c): orientable
// queued CPs act as passive demodulators before they are selected.
// 0 = off (default) -> engine byte-identical.
fn void      thvm_atp_set_use_otter(AtpState *s, u8 on);
// Raw-overlap demodulation for the Otter replay and the selection
// normalize (src/atp/otter.c).  0 = off (default) -> byte-identical.
fn void      thvm_atp_set_use_otter_raw(AtpState *s, u8 on);
// Interreduction under the run's ordering (src/atp/wm_drain.c).
fn void      thvm_atp_set_use_ir_run_order(AtpState *s, u8 on);
// Vampire bd=preordered: interreduce only with oriented new rules.
fn void      thvm_atp_set_use_bd_preordered(AtpState *s, u8 on);
// Vampire AgeQueue for the age picks (derivation depth, then weight).
fn void      thvm_atp_set_use_age_depth(AtpState *s, u8 on);
// Vampire LRS two-limit discard (age OR weight reach keeps a CP).
fn void      thvm_atp_set_use_lrs_age(AtpState *s, u8 on);
// Selections between GNN re-ranks (0 = off, the default).  Only takes
// effect while a GNN model is loaded (thvm_atp_set_gnn_scorer).
fn void      thvm_atp_set_gnn_rerank_period(AtpState *s, u32 period);
// Parental pre-filter cap: each re-rank GNN-scores only the top `topk` CPs
// by the cheap score (0 = whole queue).  THVM_ATP_GNN_TOPK sets it too.
fn void      thvm_atp_set_gnn_topk(AtpState *s, u32 topk);
// ENIGMA coop: route the GNN re-rank to the secondary (cp_pri2) dimension
// and pick from it every `ratio`-th selection (sets gnn_coop + w2_modulo).
// ratio 0 disables coop (GNN overwrites the primary heap, GT-mode).
fn void      thvm_atp_set_gnn_coop(AtpState *s, u32 ratio);
fn void      thvm_atp_set_use_sos(AtpState *s, u8 on);
fn void      thvm_atp_set_use_goal_subterm(AtpState *s, u8 on);
fn void      thvm_atp_tgt_add_sym(AtpState *s, u32 label);
// Twee hasEqualsBonus symbol set (THVM_ATP_TWEE_EQB / --twee-eq-bonus):
// per-problem registration of the equals-machinery head labels whose
// near-solved eq(a,b) shapes score the discharged-branch minimum
// (Twee.CP.hs:262-269).  Reset before each registration pass; inert
// while the env knob is off or the set is empty.
fn void      atp_twee_eqb_reset(void);
fn void      atp_twee_eqb_mark(u32 sym);
// spb=goal derivation seeding: the NEXT equation added via
// thvm_atp_add_equation is conjecture-derived (goal-cone tgt definition
// or conjecture-role unit).  Inert unless THVM_ATP_SPB_GOAL arms the
// selection bias.  See AtpState.cp_gd.
fn void      thvm_atp_mark_gd_next(AtpState *s);
// Register the folded-signature fresh-symbol label band [lo, hi) for
// the drc=off analog (THVM_ATP_DRC_OFF).  See AtpState.fold_lo.
fn void      thvm_atp_set_fold_band(AtpState *s, u32 lo, u32 hi);
// needle (THVM_ATP_DRC_NEEDLE): register each fold-band label's
// definition (arity + body, body's FVR 0..arity-1 are the params) --
// arity[i]/body[i] describe label (fold_lo + i), n <= (fold_hi -
// fold_lo).  And register the pre-transform axiom list (the .pr
// EQUATIONS block, before tgt=3 appended its sF* definitions) for the
// post-unfold instance test.  Both are inert until THVM_ATP_DRC_NEEDLE
// arms the extra filter on top of THVM_ATP_DRC_OFF.  See AtpState
// fold_def_body / needle_ax_l comments.
fn void      thvm_atp_set_fold_defs(AtpState *s, const u8 *arity,
                                    const Term *body, u32 n);
fn void      thvm_atp_set_orig_axioms(AtpState *s, const Term *lhs,
                                      const Term *rhs, u32 n);
fn void      thvm_atp_set_use_fwd_subsume(AtpState *s, u8 on);
fn void      thvm_atp_set_use_bwd_subsume(AtpState *s, u8 on);
fn void      thvm_atp_set_use_bwd_demod(AtpState *s, u8 on);
// Waldmeister-faithful interreduction-victim demotion (see the
// AtpState.use_wm_demote comment): buffer demoted rules' ORIGINAL
// sides and re-queue them only after the new fact's CPs are
// generated, with WM's KPBehandelt `-kg r` treatment (size-gated
// oriented-rules-only renormalize + joined-discard) and fresh
// weight/FIFO age.  Default OFF (engine byte-identical); the WL
// "Waldmeister" presets set it via {"DemoteOnLhsSimplify" -> True}.
fn void      thvm_atp_set_use_wm_demote(AtpState *s, u8 on);
fn void      thvm_atp_set_use_overlap_exhaust(AtpState *s, u8 on);

// Proof-trace capacity (entries).  Defaults to ATP_MAX_TRACE; overridable
// once per process via THVM_ATP_TRACE_MAX (read at first call).  An unset
// env is byte-identical to the historical fixed 131072-entry trace.
fn u32       thvm_atp_trace_cap(void);
fn void      thvm_atp_request_trace_cap(u32 cap);

// Set a wall-clock deadline.  `seconds_from_now` is a float duration
// (e.g. 5.0 = 5 seconds); pass 0.0 to clear the deadline.  The
// saturator periodically checks the deadline in `thvm_atp_step` and
// returns `ATP_TIMEOUT` once it's crossed -- the primary defense
// against recursively-defined axioms (Y combinator) that generate
// unbounded CP fan-out and would otherwise run forever.
fn void      thvm_atp_set_wall_deadline(AtpState *s, double seconds_from_now);

// Arm (seconds>0) / disarm (<=0) the LPO comparator's mid-comparison wall
// self-abort.  Independent of thvm_atp_set_wall_deadline: it bounds only
// the LPO recursion (which can spend tens of seconds in ONE huge-term
// comparison), not the engine's per-step polls.  Under --ord=lpo the
// completion driver arms this so a slow comparison cannot overrun --wall.
fn void      thvm_atp_arm_lpo_wall(double seconds_from_now);

// Host abort hook.  When set, the saturation loop polls it and returns
// ATP_ABORTED as soon as it returns nonzero -- letting a host like the
// WL LibraryLink glue forward Abort[] / TimeConstrained[] into the C
// engine (which otherwise runs uninterruptible to completion).  NULL =
// no host abort source (the default).
extern int (*thvm_atp_abort_hook)(void);

// Heap-exhaust recovery target.  When non-NULL, heap_alloc on exhaust
// longjmps to *thvm_heap_exhaust_jmp (with value 1) instead of exit(1).
// Set by a LibraryLink entry that wraps its run in setjmp; on a longjmp
// return the entry returns LIBRARY_FUNCTION_ERROR so WolframKernel
// keeps running and wolframscript can reap the kernel cleanly.  Without
// this hook a heap-exhaust orphans the kernel under wolframscript --
// the multi-GB ghost-kernel accumulation that crashes the box (see
// feedback_wolframscript_oom_risk.md).
#include <setjmp.h>
extern jmp_buf  *thvm_heap_exhaust_jmp;
extern volatile int thvm_heap_exhausted;
// Capture-recycle mark-park callback (extern fn-pointer because the
// separately-compiled Metal TU cannot see the core TU's static `fn` symbols).
// Set by jit_capture_begin to jit_caprecycle_release_hook's Metal wrapper;
// invoked per candidate by thvm_metal_buf_park_unpreserved_pinned -- the
// realize-end sweep that offers every dead (jit_pinned yet unpreserved after
// mark_gc_preserve) captured buffer to the release pool (see jit/capture.c).
extern void (*thvm_metal_caprelease_hook)(u32 buf_id);
void thvm_metal_buf_park_unpreserved_pinned(void);
// fp8-in-tile eligibility accept/decline census (defined in
// bufferize_classify.c; called from the Metal ceiling-trip census so a
// real-gen OOM names which pre-lift matmul shapes fell back to the bf16
// transient path).
void thvm_fp8_elig_dump(void);
// Unified fatal-condition recovery: longjmp via thvm_heap_exhaust_jmp
// when set, exit(1) otherwise.  Replaces ad-hoc exit(1) calls in OOM
// paths so a LibraryLink entry can recover and return
// LIBRARY_FUNCTION_ERROR instead of killing the WolframKernel.
__attribute__((noreturn))
fn void thvm_fatal(const char *msg);

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

// Vampire `sp=reverse_frequency`: common symbols outrank rare ones,
// inverse of atp_occurrence_precedence.  See src/atp/precedence.c.
fn u32  atp_reverse_frequency_precedence(const Term *lhs, const Term *rhs,
                                         u32 n_eqns, u32 n_labels, u32 *prec);

// Goal-inclusive frequency ports for the TPTP completion path (fold the
// goal equation into the census; uncapped label range).  common_high=0 =
// Vampire `sp=frequency` (rarest symbol highest), common_high=1 =
// `sp=reverse_frequency` (most frequent highest).  atp_frequency_weights_g
// is the KBO FREQUENCY weight-generation scheme (weight = max(freq,1)).
// See src/atp/precedence.c.
fn u32  atp_frequency_precedence_g(const Term *lhs, const Term *rhs, u32 n_eqns,
                                   Term goal_l, Term goal_r, u32 n_labels,
                                   u32 *prec, int common_high);
fn void atp_frequency_weights_g(const Term *lhs, const Term *rhs, u32 n_eqns,
                                Term goal_l, Term goal_r, u32 n_labels,
                                u32 *weights);

// Vampire symbol precedence schemes (sp=...) with the optional
// symbol_precedence_boost=goal (spb=goal) band, ported from
// Kernel/Ordering.cpp sortAuxBySymbolPrecedence; see precedence.c.
// Under LPO spb=goal is the ring-family lever: the goal operator
// (multiply) rises above the additive theory, so distributivity orients
// as sum-of-products expansion and the associator/commutator definitions
// orient as folds.  atp_vampire_sp_scheme maps a --sym-prec name to the
// enum (-1 = not a Vampire-port scheme).  `arity` is indexed by label.
enum {
  ATP_VSP_ARITY, ATP_VSP_REVERSE_ARITY, ATP_VSP_UNARY_FIRST,
  ATP_VSP_CONST_MAX, ATP_VSP_CONST_MIN, ATP_VSP_FREQUENCY,
  ATP_VSP_REVERSE_FREQUENCY, ATP_VSP_UNARY_FREQUENCY,
  ATP_VSP_CONST_FREQUENCY, ATP_VSP_OCCURRENCE, ATP_VSP_LADR
};
fn int  atp_vampire_sp_scheme(const char *name);
fn u32  atp_vampire_precedence_g(const Term *lhs, const Term *rhs, u32 n_eqns,
                                 Term goal_l, Term goal_r, u32 base,
                                 u32 n_labels, const u32 *arity, int scheme,
                                 int boost_goal, u32 *prec);

// Lift the user labels (3..n_labels-1) of a finished precedence, order and
// ties kept, so they all sit above the reserved constants, and give label
// 1 precedence 1 and label 2 precedence 2: Waldmeister's reserved
// constants below every symbol.  Identity on the .pr layout.  See
// src/atp/precedence.c.
fn void atp_precedence_floor_reserved(u32 *prec, u32 n_labels);

// KBO weight-generation schemes (Vampire kbo_weight_scheme / -kws,
// Kernel/KBO.cpp weightsFromOpts), goal-inclusive.  atp_arity_weights_g
// mode: 0 arity, 1 inv_arity, 2 arity_squared, 3 inv_arity_squared;
// atp_precedence_weights_g inverse: 0 precedence, 1 inv_precedence
// (reads the live precedence).  All yield weights >= 1 so KBO stays
// admissible against var_weight 1.  See src/atp/precedence.c.
fn void atp_arity_weights_g(const Term *lhs, const Term *rhs, u32 n_eqns,
                            Term goal_l, Term goal_r, u32 n_labels,
                            u32 *weights, int mode);
fn void atp_inv_frequency_weights_g(const Term *lhs, const Term *rhs,
                                    u32 n_eqns, Term goal_l, Term goal_r,
                                    u32 n_labels, u32 *weights);
fn void atp_precedence_weights_g(const Term *lhs, const Term *rhs, u32 n_eqns,
                                 Term goal_l, Term goal_r, u32 n_labels,
                                 const u32 *precedence, u32 *weights,
                                 int inverse);

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
// dedup; callers post-filter if needed.
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

// === Live CP-queue re-rank seam (WL-side GNN scorer) ================
// A scorer running mid-saturation pulls the current queue, scores each
// CP, then re-keys priorities to permute selection order.  Re-ranking
// only PERMUTES selection (no CP added/dropped), so completeness is
// preserved -- the periodic FIFO / force-drain selection still fires.

// Number of LIVE queued CPs.  The CP heap is a compact array: slots
// [0, n_cps) all hold a packed CP (pops backfill from the tail), so
// this is exactly s->n_cps.  NULL-safe (returns 0).
fn u32       thvm_atp_queued_cp_count(const AtpState *s);

// Snapshot the live queue for scoring.  For each live slot, unpacks the
// CP into a fresh pair of transient heap Terms and writes lhs/rhs/seq
// into the caller's out arrays (up to `cap` entries; any out pointer
// may be NULL to skip that column).  Returns the number written
// (min(n_cps, cap)).  Pure read of the queue; engine state untouched.
fn u32       thvm_atp_queued_cps  (const AtpState *s, Term *lhs_out,
                                   Term *rhs_out, u32 *seq_out, u32 cap);

// Re-key + re-heapify.  For each j in [0, n), finds the live slot whose
// cp_seq == seq[j] and sets its cp_pri = pri[j]; then rebuilds the heap
// (Floyd build-heap over atp_cp_sift_down) so the new priorities take
// effect on the next select.  Only PERMUTES selection order: no CP is
// added or dropped, cp_seq is preserved (the FV-index borrow stays
// sound, no rebuild needed), so completeness is unaffected.  The
// seq->slot resolution is an O(n + m) hashed pass (n live slots, m
// requests), never O(n*m).  Unknown / already-popped seqs are ignored.
fn void      thvm_atp_set_cp_pri_by_seq(AtpState *s, const u32 *seq,
                                        const u32 *pri, u32 n);
fn void      thvm_atp_set_cp_pri2_by_seq(AtpState *s, const u32 *seq,
                                         const u32 *pri, u32 n);

// Derivation-history export (src/atp/history.c): THVM_ATP_HISTORY_EXPORT
// =<path> arms an observation-only record stream of pushes (sampled at
// THVM_ATP_HISTORY_RATE, default 1/16), selections, deletions, parent
// deaths, rule content and deaths, the Otter replay and the rule-side
// trace DAG, headed by the format version and the lane (FINDPROOF_SLICE).
// symbol records one signature entry (label, arity, name; fresh = made by
// an input transform) for name-keyed rule content; call it after init.
// search_end marks where the search stopped (entries after it came from
// proof emission); finish dumps the trace entries and closes the session;
// call it after proof emission.  No-op when unarmed.
fn void      thvm_atp_history_symbol(AtpState *s, u32 label, u32 arity,
                                     u8 fresh, const char *name);
fn void      thvm_atp_history_finish(AtpState *s, u32 status);
fn void      thvm_atp_history_search_end(AtpState *s);

// Selection-time top-K re-ranker (src/atp/topk.c): load its head from a
// safetensors file (tools/atp_enigma/topk_net.py export).  Returns 1 on
// success.  The engine arms itself from THVM_ATP_RERANK_TOPK and
// THVM_ATP_RERANK_MODEL; this entry is for hosts and tests.
fn int       thvm_atp_topk_load(const char *path);

// Problem features phi(p) (docs/ATP/enigma_portfolio.md, section 3.1),
// the per-problem input of the selection layer.  tp_problem_features
// (tools/atp_cli/problem_features.c) fills it and FindProof
// --problem-features prints it, so training rows and the deployed layer
// read the same numbers.
//   gate bits   GOAL_EXISTENTIAL .. AX_GROUND_COMPOUND and CONJ_CONE_PARTIAL
//               are the exit-fast predicates of narrowq, equals*, ifeq,
//               horn-cd/horn-back, unfold*, tgt-ax and tgt-sel, read where
//               the gate reads them.  DISTRIB_LAW is only the shape the
//               otter-* gate looks for: that gate also needs the law to
//               orient as expansion under the lane's precedence.  FE_LIVE:
//               the front end's own gates pass; SINE: axiom selection runs.
//   algebra     AC_PAIRS .. DISTRIB_SYMS: operator counts from
//               Waldmeister's structure detection over the loaded .pr.
//   sizes       AXIOMS .. GOAL_VARS_MAX hold floor(log2(1 + n)) of the
//               problem as stated (a constant or a variable has depth 1).
//   CONJ_SYM_FRAC  conjecture-clause symbols / signature.
// CONJ_CONE_PARTIAL, DISTRIB_LAW, the algebra and CONJ_SYM_FRAC read the
// problem as the engine loads it, after axiom selection and the canonical
// presentation.  Everything is computed under the default front-end
// configuration, whatever the environment holds; a column whose stage
// the front end never reaches (it gave up first) is 0.  No TPTP header
// field is here: the deployed problems carry none, and the status is the
// label.
// ATP_PHI_VERSION changes with any column or its meaning; trained weights
// record the version they were trained on.
#define ATP_PHI_VERSION 1
enum {
  ATP_PHI_GOAL_EXISTENTIAL,
  ATP_PHI_EQUALS_GOAL,
  ATP_PHI_IFEQ_DISPATCHER,
  ATP_PHI_HORN_SLICE,
  ATP_PHI_DEF_UNFOLD,
  ATP_PHI_AX_GROUND_COMPOUND,
  ATP_PHI_CONJ_CONE_PARTIAL,
  ATP_PHI_DISTRIB_LAW,
  ATP_PHI_FE_LIVE,
  ATP_PHI_SINE,
  ATP_PHI_AC_PAIRS,
  ATP_PHI_COMM_SYMS,
  ATP_PHI_ASSOC_SYMS,
  ATP_PHI_IDEM_SYMS,
  ATP_PHI_UNIT_SYMS,
  ATP_PHI_INVERSE_SYMS,
  ATP_PHI_DISTRIB_SYMS,
  ATP_PHI_AXIOMS,
  ATP_PHI_HYPOTHESES,
  ATP_PHI_GOALS,
  ATP_PHI_SYMS,
  ATP_PHI_SYMS_AR0,
  ATP_PHI_SYMS_AR1,
  ATP_PHI_SYMS_AR2,
  ATP_PHI_SYMS_AR3,
  ATP_PHI_SYMS_AR4P,
  ATP_PHI_MAX_ARITY,
  ATP_PHI_VAR_NAMES,
  ATP_PHI_AX_SIZE_MAX,
  ATP_PHI_AX_SIZE_MEAN,
  ATP_PHI_AX_DEPTH_MAX,
  ATP_PHI_AX_DEPTH_MEAN,
  ATP_PHI_AX_VARS_MAX,
  ATP_PHI_AX_VARS_MEAN,
  ATP_PHI_AX_GROUND,
  ATP_PHI_GOAL_SIZE_MAX,
  ATP_PHI_GOAL_SIZE_MEAN,
  ATP_PHI_GOAL_DEPTH_MAX,
  ATP_PHI_GOAL_VARS_MAX,
  ATP_PHI_CONJ_SYM_FRAC,
  ATP_PHI_N
};
typedef struct { float v[ATP_PHI_N]; } AtpPhi;

// ENIGMA selection layer (src/atp/guide.c).  The problem features
// phi(p) it reads (docs/ATP/enigma_portfolio.md 3.1), one row per
// problem: set them in the process that runs the engine before its first
// weight pick (the layer folds them in then, and again whenever they are
// set anew); a head reading more than `n`, or a non-finite value, takes
// its training mean there.
#define ATP_GUIDE_PHI_MAX 64u
typedef struct {
  u32   n;
  float v[ATP_GUIDE_PHI_MAX];
} AtpGuidePhi;
fn void      thvm_atp_guide_set_phi(const AtpGuidePhi *phi);
// FindProof's phi(p) seam: how many phi(p) columns the head the
// environment arms reads (0: the environment arms no layer, or its head
// reads none).  A process that cannot give that head phi(p) calls
// thvm_atp_guide_env_off, which keeps the layer unarmed there and prints
// one "guide: <why>; selection layer off" line.
fn u32       thvm_atp_guide_env_phi_cols(void);
fn void      thvm_atp_guide_env_off(const char *why);
// Arm s with the compiled-in head `model_id` (NULL: the default) and the
// integration its family table gives `family`; zero = 1 forces a zero
// correction; `lane` names the lane the engine runs, for the head's lane
// one-hot (NULL or not in the head's lane table: the family entry's
// index).  Call before the first push.  Returns 1 on success; 0 also
// when the top-K re-ranker is armed (it would take every weight pick).
// thvm_atp_init arms from THVM_ATP_GUIDE / _MODEL / _ZERO, the lane from
// THVM_ATP_GUIDE_LANE or FINDPROOF_SLICE.
fn int       thvm_atp_guide_arm(AtpState *s, const char *family,
                                const char *model_id, u8 zero,
                                const char *lane);
// The layer's per-pick cost counters as one "% guide:" line, with the
// layer's time as a fraction of the time since arming (frac_wall);
// nothing when s is unarmed.
fn void      thvm_atp_guide_print_stats(const AtpState *s, FILE *f);

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

// === ENIGMA Tier 2 graph-dataset export ============================
// Enable per-CP anonymised-hypergraph recording (needs record_cp_features
// too, for the trace ids the labeller fills).  OFF by default; when off
// the record path is inert and the run is byte-identical.
fn void      thvm_atp_set_record_cp_graphs(AtpState *s, u8 on);

// On a SUCCESSFUL proof (after thvm_atp_cp_label filled the labels),
// append one JSONL line per recorded CP to `path` (created if absent,
// appended otherwise -- no header, so a SERIAL one-problem-per-process
// run accumulates a single dataset).  Each line is the anonymised graph
// the in-engine GCN scores plus its proof-relevance label:
//   {"y":<0|1>,"x":[[f0..f5],...],"e":[[src,dst],...]}
// x is the n_nodes list of 6 anonymised node features (is_term,
// is_symbol, is_var, arity, occurrence_count, is_cpsuper); e is the
// directed edge list (the trainer symmetrises + self-loops + row-
// normalises, matching the C forward's build_host).  Overflow graphs
// (n_nodes 0) are skipped.  Returns the number of lines written.
fn u32       thvm_atp_cp_graph_dataset_append(const AtpState *s,
                                              const char *path);

// === Runtime-loadable learned CP scorer (ENIGMA Tier 1) =============
// The baked-in ATP_LEARNED_W/B logistic regression is the default
// fallback for ATP_CP_WEIGHT_LEARNED.  This block lets a model trained
// in WL (TNetTrain over the exported dataset) be pushed into the engine
// at run time -- no recompile -- so the prove -> label -> train -> reload
// loop can close.  Two model kinds share one parameter blob:
//   LINEAR: score = b + sum_i w_lin[i] * z[i]
//   MLP:    h_j = relu(b1_j + sum_i W1[j][i] * z[i]); score = b2 + sum_j w2[j] h_j
// where z[i] = (feat[i] - mean[i]) * inv_std[i] is the standardized
// feature (mean/inv_std shipped with the model, so training standardizes
// freely and the C side stays a plain forward).  `score` is a raw logit
// (no sigmoid head): higher == more proof-relevant == selected sooner,
// matching the baked-in scorer's score -> priority mapping.
#define ATP_LEARNED_MAX_HIDDEN 64u
typedef enum {
  ATP_LEARNED_NONE   = 0,
  ATP_LEARNED_LINEAR = 1,
  ATP_LEARNED_MLP    = 2,
} AtpLearnedKind;
typedef struct {
  u8    kind;                                          // AtpLearnedKind
  u32   hidden;                                        // MLP width (0 for linear)
  float mean[ATP_CP_FEATURE_DIM];                      // per-feature mean
  float inv_std[ATP_CP_FEATURE_DIM];                   // 1 / per-feature std
  float w1[ATP_LEARNED_MAX_HIDDEN * ATP_CP_FEATURE_DIM]; // MLP layer-1, or w_lin in [0,14)
  float b1[ATP_LEARNED_MAX_HIDDEN];                    // MLP layer-1 bias
  float w2[ATP_LEARNED_MAX_HIDDEN];                    // MLP output weights
  float b2;                                            // output bias (linear bias too)
} AtpLearnedScorer;

// Push a trained model.  `blob` is a flat f64 parameter vector:
//   [0]      kind   (1 = LINEAR, 2 = MLP)
//   [1]      hidden (H; 0 for linear)
//   [2..16)  mean[14]
//   [16..30) inv_std[14]
//   LINEAR (len 45):  w_lin[14], b
//   MLP    (len 31 + 16*H):  W1[H*14], b1[H], w2[H], b2
// Returns 1 on success (model active for subsequent ATP_CP_WEIGHT_LEARNED
// runs), 0 on a malformed blob (the engine then keeps the baked-in
// scorer).  Process-global: set once, used by every later run until
// cleared.
fn int       thvm_atp_set_learned_scorer(const double *blob, u32 len);
// Drop the runtime model; ATP_CP_WEIGHT_LEARNED reverts to the baked-in
// logistic regression.
fn void      thvm_atp_clear_learned_scorer(void);

// === ENIGMA Tier 2: anonymised CP hypergraph (for a GNN) ============
// The Tier-1 path (thvm_atp_cp_features) hand-rolls a fixed 14-D vector;
// a graph neural network instead wants the CP's STRUCTURE as a typed
// graph it can message-pass over.  thvm_atp_cp_graph turns a critical
// pair (lhs, rhs) into a small typed hypergraph whose node features are
// PURELY STRUCTURAL -- they encode node kind / arity / occurrence count
// but NEVER the concrete symbol label, variable id, or numeric value.
// This anonymisation is the whole point: two CPs that are equal up to a
// consistent renaming of symbols + variables produce bit-identical
// graphs, so a model trained on one transfers to the other (and to
// symbols it never saw).  Concrete identities only drive the
// first-appearance dedup, which is itself rename-invariant because it is
// positional.
//
// Node kinds:
//   CPSUPER  one per graph (node 0): the critical-pair root.
//   TERM     one per subterm OCCURRENCE in the preorder walk of lhs/rhs.
//   SYMBOL   one per distinct CTR label (deduped, first-appearance order);
//            a TAG_NUM atom also lands here, deduped by its raw value, as
//            a nullary constant-like symbol.
//   VAR      one per distinct TAG_FVR id (deduped, first-appearance order).
//
// Edge kinds (all directed):
//   E_TERM_SYM   a TERM node  -> its root SYMBOL or VAR node.
//   E_TERM_CHILD a TERM node  -> each of its child TERM nodes (in order).
//   E_CP_LHS     the CPSUPER  -> the root TERM of lhs.
//   E_CP_RHS     the CPSUPER  -> the root TERM of rhs.
//
// Feature columns (ATP_CPG_FEAT_DIM, all f32).  Columns 0-9 are strictly
// structural / rename-invariant; columns 8-9 are goal-relevance signals
// that depend on the goal cone (populated only when a build passes a
// non-NULL AtpState AND the run is recording graphs / running the GNN
// re-rank / running s2a -- otherwise 0, so the anonymised default holds).
//   0 is_term           1 for a TERM node, else 0
//   1 is_symbol         1 for a SYMBOL node, else 0
//   2 is_var            1 for a VAR node, else 0
//   3 arity             CTR arity for a SYMBOL (0 for a NUM-atom symbol,
//                       var, term, cpsuper)
//   4 occurrence_count  how many times this symbol/var occurs across the
//                       two terms (1 for TERM/CPSUPER -- a single occ)
//   5 is_cpsuper        1 for the CP super-node, else 0
//   6 depth             TERM node's preorder depth within its CP side
//                       (side root = 0, each child +1); 0 for non-TERM
//   7 arg_index         TERM node's argument position within its parent
//                       (side root = 0); the positional signal the doc's
//                       child-edge embedding calls for, carried on the
//                       node so the untyped-adjacency forward sees it.
//                       0 for non-TERM.
//   8 goal_closeness    SYMBOL node: 1/(1+goal_level) from the reused s2a
//                       sine-level table (goal symbol -> 1.0, farther ->
//                       smaller, goal-unreachable / no table -> 0).  0 for
//                       non-SYMBOL.  Rename-invariant (a consistent
//                       relabel of the whole problem preserves levels).
//   9 is_goal_symbol    SYMBOL node: 1 if the symbol's label is in the
//                       conjecture cone (conj_sym_mask), else 0.  The
//                       exact, always-fresh goal membership SInE lacks.
//  10 occurs_lhs        1 if the node occurs on the CP's lhs side, else 0
//  11 occurs_rhs        1 if the node occurs on the CP's rhs side, else 0
#define ATP_CPG_FEAT_DIM  12u
#define ATP_CPG_MAX_NODES 1024u
#define ATP_CPG_MAX_EDGES 4096u

typedef enum {
  ATP_CPG_CPSUPER = 0,  // the critical-pair super-node (node 0)
  ATP_CPG_TERM    = 1,  // a subterm occurrence
  ATP_CPG_SYMBOL  = 2,  // a distinct CTR label / NUM-atom value
  ATP_CPG_VAR     = 3,  // a distinct FVR id
} AtpCpgNodeType;

typedef enum {
  ATP_CPG_E_TERM_SYM   = 0,  // term -> its root symbol/var
  ATP_CPG_E_TERM_CHILD = 1,  // term -> child term
  ATP_CPG_E_CP_LHS     = 2,  // cp-super -> lhs root term
  ATP_CPG_E_CP_RHS     = 3,  // cp-super -> rhs root term
} AtpCpgEdgeType;

typedef struct {
  u32   n_nodes;
  u8    node_type[ATP_CPG_MAX_NODES];                  // AtpCpgNodeType
  float node_feat[ATP_CPG_MAX_NODES * ATP_CPG_FEAT_DIM];
  // Concrete symbol identity per node, so the equation reconstructs
  // exactly: the intern key -- CTR label / FVR id / NUM value -- for a
  // SYMBOL or VAR node, 0 for a TERM or CP-super node.  The node_feat
  // columns stay anonymised (the GNN never sees this); a caller that
  // wants the un-anonymised symbol reads node_label.
  u64   node_label[ATP_CPG_MAX_NODES];
  u32   n_edges;
  u32   edge_src[ATP_CPG_MAX_EDGES];
  u32   edge_dst[ATP_CPG_MAX_EDGES];
  u8    edge_type[ATP_CPG_MAX_EDGES];                  // AtpCpgEdgeType
  u8    overflow;                                      // 1 if a cap was hit
} AtpCpGraph;

// Build the anonymised hypergraph for the CP (lhs, rhs) into `out`.  A
// pure read of the two Terms plus, when `s` is non-NULL, the goal cone
// (conj_sym_mask + the s2a sine-level table) for the two goal-relevance
// feature columns -- no mutation of `s`.  Pass s = NULL for a purely
// structural / anonymised graph (columns 8-9 stay 0), which is what the
// rename-invariance property and the offline parity harness assert.
// Returns 1 on success; on overflow (more than ATP_CPG_MAX_NODES /
// ATP_CPG_MAX_EDGES) it sets out->overflow = 1, leaves the partially-
// built graph in place, and returns 0 so the caller can fall back to the
// Tier-1 vector.
fn int       thvm_atp_cp_graph(const AtpState *s, Term lhs, Term rhs,
                               AtpCpGraph *out);

// === ENIGMA Tier 2: in-engine GCN critical-pair scorer ==============
// The Tier-2 GCN runs its forward on thvm's OWN tensor runtime (a UOP
// graph built + thvm_realize'd), not a hand-rolled matmul and not a WL
// round-trip.  It replicates the WL reference forward (atpGnnForwardLogits
// in wl/THVMLink/Kernel/ATP/ATP.wl) bit-for-bit: R rounds of
// row-normalised-adjacency message passing relu(A.H.W1 + H.Ws + b),
// masked-mean pool, and a two-class readout whose per-graph score is
// logit_pos - logit_neg.
//
// Blob layout (flat f64, mirroring thvm_atp_set_learned_scorer's style):
//   [0]              R   (message-passing rounds, R >= 1)
//   [1]              H   (hidden width, H >= 1)
// then, for each round r in [0, R), with in_r = ATP_CPG_FEAT_DIM (6) on
// round 0 and H on every later round, all row-major:
//   W1[r]  in_r * H      (A.H branch weight, shape {in_r, H})
//   Ws[r]  in_r * H      (self branch weight, shape {in_r, H})
//   Bh[r]  H             (per-round message bias)
// then the readout:
//   Wout   H * 2         (shape {H, 2})
//   Bout   2
// The total length is  2 + sum_r (2*in_r*H + H) + 2*H + 2.  Returns 1 on
// a well-formed blob (model active for subsequent gnn-score / rerank
// calls), 0 on a malformed one (the model stays whatever it was).
// Process-global, set once and reused, like the Tier-1 scorer.
fn int       thvm_atp_set_gnn_scorer(const double *blob, u32 len);
// Drop the loaded GNN model.  A later gnn-score / rerank call with no
// model loaded is a no-op.
fn void      thvm_atp_clear_gnn_scorer(void);
// 1 if a GNN model is currently loaded, else 0.
fn int       thvm_atp_gnn_loaded(void);

// M2 (correctness anchor): score a BATCH of `n` critical pairs with the
// loaded GNN.  For each i, builds thvm_atp_cp_graph(lhs[i], rhs[i]),
// pads every graph to a common node count, assembles the GCN forward as
// a thvm UOP graph, thvm_realize's it on the CPU backend, and writes the
// per-graph score (logit_pos - logit_neg) into out_scores[i].  Returns 1
// on success (model loaded, all graphs built); 0 if no model is loaded,
// a graph overflows the node cap, or n == 0.  A pure read of the Terms;
// no AtpState needed.
fn int       thvm_atp_gnn_score_batch(const Term *lhs, const Term *rhs,
                                      u32 n, float *out_scores);

// Score a batch of PRE-BUILT graphs (the same math as
// thvm_atp_gnn_score_batch, but from AtpCpGraph structs instead of
// Terms).  Used by the offline parity harness (exported graphs scored by
// the exact in-engine forward, cross-checked against the Python trainer).
// Returns 1 on success, 0 if no model is loaded or n == 0.
fn int       thvm_atp_gnn_score_graphs(const AtpCpGraph *gs, u32 n,
                                       float *out_scores);

// M3 (integration): re-rank the live CP queue with the loaded GNN.  The
// parental pre-filter (gnn_topk) keeps only the top-K CPs by the cheap
// score, builds their goal-aware graphs, scores them with
// thvm_atp_gnn_score_graphs, maps each score to a queue priority (higher
// score -> lower priority, the same 1e6 - 1e4*score band the Tier-1
// scorer uses), and pushes them back via thvm_atp_set_cp_pri_by_seq (or
// cp_pri2 in coop mode).  Only PERMUTES selection order, so completeness
// is preserved.  A no-op when no model is loaded or the queue is empty.
// Returns the number of CPs re-ranked.
fn u32       thvm_atp_gnn_rerank(AtpState *s);

// === wald/ ===
// Parser for Waldmeister .pr-style spec files.  WaldSpec holds the
// parsed signature + variable table + equations + single
// conclusion goal; downstream feeds it to thvm_atp_run.
#define WALD_MAX_SYMBOLS 64
#define ATP_REL_LEVEL_MAX 8u
#define ATP_REL_LEVEL_REMOTE 255u
// s2a (Vampire sine_to_age): the per-CP sine level a REMOTE (goal-
// unreachable) symbol contributes.  Real SInE levels sit below this, so a
// CP touching any goal-unreachable symbol is the most postponed.
#define ATP_S2A_REMOTE_LVL 32u
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
//   {first: i, count: 2}     -> two rules at i, i+1 (unfailing fallback OR
//                                base + FVI rule from RechtsUnfreiErzeugen)
// `fvi_count` (default 0) is the number of trailing rules in this range
// that are Waldmeister `RechtsUnfreiErzeugen` free-variable instances
// (RUndEVerwaltung.c:366-397) rather than direct orientations.  The
// trace pusher uses this to stamp those rules with TRACE_FVI instead
// of TRACE_ORIENT so the proof renderer can label them distinctly.
typedef struct {
  u32 first;
  u32 count;
  u32 fvi_count;
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

// Install a pre-oriented axiom as a forward rule (lhs -> rhs),
// bypassing KBO orientation.  Forces r_orient[idx] = 1 even when KBO
// says the direction is unorientable, and generates CPs against
// earlier rules immediately.  Caller is responsible for verifying
// variable-safety (vars(rhs) subset of vars(lhs)) and termination
// (a non-decreasing rule may loop the rewriter).  Returns the added
// range; count = 0 if the rule was rejected as a duplicate.
fn AtpAddedRange thvm_atp_install_oriented_rule(AtpState *s, Term lhs,
                                                Term rhs);

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

// The normal form of t under the current R and E by ordered rewriting;
// canonical for t's equivalence class once completion has saturated.
fn Term thvm_atp_normal_form(AtpState *s, Term t);

// One saturation step: goal_check -> select -> normalize ->
// trivialize -> orient+add -> interreduce -> generate_cps ->
// goal_check.  Returns ATP_PROVED / ATP_TIMEOUT /
// ATP_QUEUE_EMPTY / ATP_RUNNING (continue).
fn AtpStatus thvm_atp_step(AtpState *s);

// Drive thvm_atp_step until it returns non-RUNNING.
fn AtpStatus thvm_atp_run (AtpState *s);
// Rebuild the off-heap packed proof-trace into live Term entries after a
// terminated search (definition in src/atp/_.c).
fn void thvm_atp_materialize_trace(AtpState *s);
// Post-PROVED proof-cone norm re-derivation: replay the unrecorded
// engine normalizations behind goal-family TRACE_ORIENT /
// TRACE_SIMPLIFY entries over the historical rule set R(ti), splicing
// TRACE_NORM_STEP chains into the trace so the WL lift inherits them
// instead of BFS-bridging (definition in src/atp/_.c).  Run AFTER the
// search terminates and BEFORE trace materialization / extraction;
// entries whose replay misses the recorded equation stay untouched.
fn void thvm_atp_record_goal_cone(AtpState *s);

// Emission-time treatment re-derivation (the --proof emitter): for trace
// entry `ti` (TRACE_ORIENT / TRACE_FVI / TRACE_SIMPLIFY) whose pair is
// not its parent's, replay the engine's normalizers over the historical
// rule window and return the rewrite chain parent -> entry in out[]:
// out[k] is the pair after step k and the trace id of the equation it
// applies.  Returns the step count (0 = no replay reaches the entry's
// pair).  Nothing is pushed on the trace.  thvm_atp_rederive_end drops
// the window cache once emission is done.
typedef struct { Term l, r; u32 eq; } AtpRederiveStep;
fn u32  thvm_atp_rederive(AtpState *s, u32 ti, AtpRederiveStep *out, u32 cap);
fn void thvm_atp_rederive_end(AtpState *s);
// Wall deadline (us, atp_now_us clock) after which re-derivation gives up.
fn void thvm_atp_rederive_deadline(u64 wall_us);

// Optional incremental worklist for nf.  When attached, redex_fire
// pushes locally-fresh redexes (the result + every newly allocated
// cell whose content is a redex) into the buffer, advancing *n_ptr.
// nf attaches before its main loop and detaches after.  Other
// callers (WL TInteract, TStep) leave the worklist detached and
// pay zero overhead.

// === runtime lifecycle ===
// Cheap per-frame reclaim: rewind the dynamic heap + every dynamic
// descriptor/cache to a fresh state while keeping the arenas mapped,
// the backend device live, and the static book heap / DEFS intact.
// Backs TReset[].
void thvm_reset(void);

#endif // THVM_H
