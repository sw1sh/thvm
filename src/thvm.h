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
typedef int32_t  i32;
typedef float    f32;

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

#define TAG_COUNT 25

// === OP2 opcodes (TAG_OP2 ext field) ===
#define OP_ADD  0
#define OP_SUB  1
#define OP_MUL  2
#define OP_EQ   3   // returns NUM(1) for equal, NUM(0) otherwise
#define OP_LT   4   // less-than: NUM(1) if x<y else NUM(0)

// === Dtypes ===
#define DT_F32   0
#define DT_I32   1
#define DT_COUNT 2

// === UOp opcodes (TAG_UOP ext field) ===
// See docs/tensors.md for per-opcode heap layouts.

// (slot 0 is reserved/unused -- previously UOP_MATERIALIZE, now dropped
// in favour of calling thvm_materialize directly.)
#define UOP_KERNEL       1   // heap = [output_buf, ast_root]; ext bits: see uop_kernel.c
#define UOP_CONST        2   // heap = [NUM(bits)]; ext = dtype
#define UOP_RESHAPE      3   // heap = [src, NUM(ndim), NUM(d0), ..., NUM(d_{n-1})]
#define UOP_PERMUTE      4   // heap = [src, NUM(p0), ...]; ext = ndim
#define UOP_EXPAND       5   // heap = [src, NUM(ndim), NUM(d0), ...]
#define UOP_PAD          6   // heap = [src, NUM(b0), NUM(e0), ...]; ext = ndim
#define UOP_SHRINK       7   // heap = [src, NUM(b0), NUM(e0), ...]; ext = ndim
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
#define UOP_GRAD        18   // heap = [y, gy_seed, target]; rewrite rule
                             // (slot 19 was UOP_CONV2D -- removed; lowering
                             // happens entirely in WL via TUOpConv2DLowered.
                             // Slot left unused so existing opcode integers
                             // for CMPEQ/LOAD don't shift.)
#define UOP_CMPEQ       20   // heap = [a, b]; mask of (a == b), 0/1 floats
#define UOP_LOAD        21   // heap = [src]; explicit "read this tensor" boundary
                             //   marker (mirrors tinygrad's UOps.LOAD).  Slot
                             //   reserved -- constructor + materializer land in
                             //   sub-item (b); see TASKS.md UOP_LOAD arc.
#define UOP_COUNT       22

// REDUCE kinds packed into the high bits of UOP_REDUCE's EXT field.
#define REDUCE_SUM   0
#define REDUCE_MAX   1

// === Capacities ===
#define HEAP_CAP     (1ULL << 24)   // 16M cells * 8B = 128 MiB. Plenty for tests.
#define WNF_CAP      (1ULL << 16)   // 64K stack slots.
#define TENS_CAP     (1ULL << 16)   // 64K tensor descriptor slots.
#define KERNELS_CAP  (1ULL << 14)   // 16K compiled kernels.
#define BOOK_CAP     (1ULL << 18)   // 256K cells of static def template heap.
#define DEFS_CAP     256            // max named definitions for TAG_REF.
#define ALO_STATE_CAP (1ULL << 16)  // ALO substitution-chain entries.
#define MAX_DIM      8              // max tensor rank
#define KPROG_MAX_OPS    64         // max ops per kernel program
#define KERNEL_MAX_INPUT 8          // max input tensors per kernel
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

typedef struct {
  u32      dtype;               // DT_F32 / DT_I32 / ...
  u32      refcount;            // shared by DUP; decremented by ERA
  View     view;                // single view for step 12; ShapeTracker in step 14
  u32      buf_id;              // backend buffer handle (0 = no buffer yet)
  u32      producer_kid;        // kernel id that produces this tensor, 0 = external
  Backend *backend;             // vtable
} TenDesc;

// Forward declaration for the dispatch callback.
struct KernelEntry;

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

typedef struct KernelEntry {
  u32       n_inputs;
  u32       input_tids   [KERNEL_MAX_INPUT];  // TenDesc id, or 0 if symbolic
  u32       input_dtypes [KERNEL_MAX_INPUT];
  u32       input_numels [KERNEL_MAX_INPUT];

  // For inputs that aren't statically a TenDesc (e.g., a free TAG_VAR
  // that gets bound to a TEN at fire time via APP-LAM beta), we
  // store the symbolic Term value here.  kernel_fire_by_id resolves
  // each non-zero entry through term_resolve before reading buffers.
  Term      input_terms  [KERNEL_MAX_INPUT];

  u32       output_tid;            // TenDesc id we write to
  u32       output_dtype;
  Shape     output_shape;
  u32       output_numel;

  u32       n_ops;
  KProgOp   program[KPROG_MAX_OPS];

  // Original root UOP term that this kernel was built from.  The
  // walker rewrites parent cells to UOP_KERNEL but leaves the
  // source UOP cells in the heap (now "orphaned" -- no live
  // references reach them through the rewritten parents).  Grad
  // chain-rule walks this term directly: heap_read on its child
  // slots gives whatever they point at now (often other kernels,
  // each of which carries its own source_uop), so the walk
  // recurses into the original UOp graph naturally.
  Term      source_uop;

  u8        fired;                 // 1 once the kernel has run
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
  void     *compiled;              // backend-specific; NULL for interpreter
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

// === ShapeBinding (used by schedule/shape_env.c) ===
// Threaded through the materialize walk so a deeper VAR(binder_loc)
// can be kernelised with a known output shape.  Linked-list parent
// chain; ID 0 = empty environment.  Embedded inline in TContext so
// each context has its own non-overlapping environment.
#define SHAPE_ENV_CAP (1ULL << 14)
typedef struct {
    u32   parent;
    u64   var_loc;
    Shape shape;
} ShapeBinding;

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

#define CPU_BUFS_CAP     (1ULL << 16)
#define CPU_FREELIST_CAP 4096

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
    ShapeBinding *shape_env;             // materialize-pass arena
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
    u32 shape_env_next;
    u64 cpu_bufs_next;
    u32 cpu_freelist_len;
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
#define SHAPE_ENV           (CURRENT_CTX->shape_env)
#define SHAPE_ENV_NEXT      (CURRENT_CTX->shape_env_next)
#define CPU_BUFS            (CURRENT_CTX->cpu_bufs)
#define CPU_BUFS_NEXT       (CURRENT_CTX->cpu_bufs_next)
#define CPU_FREELIST        (CURRENT_CTX->cpu_freelist)
#define CPU_FREELIST_LEN    (CURRENT_CTX->cpu_freelist_len)

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

// === heap-walk materialize ===
// Visits cells reachable from `root`, propagates shapes through
// APP[LAM, arg], and rewrites UOPs into UOP_KERNELs in place.  See
// src/schedule/walk.c.
Term materialize_walk(Term root);

// f1c: classify which UOPs in the DAG rooted at `root` must
// realize into a backing buffer; populates a private table
// consulted via realize_is_realized / realize_consumer_count.
// f1d's selective materializer reads this; f1c on its own is
// read-only.  See src/schedule/realize_classify.c.
fn void realize_classify(Term root);
fn u8   realize_is_realized(Term uop_term);
fn u32  realize_consumer_count(Term uop_term);

// f1d toggle: 0 = legacy behavior (per-UOp kernels, no
// fusion); 1 = consult realize_is_realized to inline
// un-realized upstream compute into the consumer's program.
// Default 0; flipped on once f1d-b/c land.
extern u8 MATERIALIZE_USE_REALIZE_INFO;

// f1d-b1: emit ONE kernel for a realized UOp that inlines its
// un-realized elementwise upstream compute.  Returns 0 if the
// chain contains a non-elementwise un-realized UOp (caller
// falls back to legacy per-UOp kernel emission).
fn Term materialize_kernel_inlined(Term realized_uop_term);

// True for elementwise + UOP_CONST (the ops the inlined helper
// can absorb into a parent kernel's program).  Movement ops
// (RESHAPE/EXPAND/SHRINK/PERMUTE/PAD/FLIP) return false so the
// f1d hook in materialize_uop_in_env falls them through to the
// legacy view-only alias / kernel emit path.
fn u8 inline_is_inlinable(u8 op);

// f1d-d4b2: per-realize dedup memo for the materialize entry
// points.  Without it, shared UOps in grad chains emit one
// kernel per reaching path; with it, each UOp loc maps to a
// single emitted Term (UOP_KERNEL or TAG_TEN) for the duration
// of one thvm_materialize call.  See src/schedule/materialize_memo.c.
fn void materialize_memo_clear (void);
fn Term materialize_memo_lookup(u64 loc);
fn void materialize_memo_store (u64 loc, Term t);

// === interact/ ===
// One file per active pair.  Each rule increments ITRS when it fires.
fn Term interact_app_lam(Term lam, Term arg);
fn Term interact_app_era(void);
fn Term interact_dup_sup(u32 lab, u64 loc, u8 side, Term sup);
fn Term interact_dup_era(u8 side, u64 loc, Term era);
fn Term interact_dup_lam(u32 lab, u64 loc, u8 side, Term lam);

// === tensor/ ===
// Tensor descriptor lifecycle.  Step 12: bump-only allocation in TENS[];
// refcount + backend-level buffer refcount govern the buffer lifetime.
fn u32  tensor_alloc  (Backend *b, Shape shape, u32 dtype);
fn void tensor_incref (u32 id);
fn void tensor_decref (u32 id);
fn void tensor_release(u32 id);   // decref + buf_decref; free at 0
fn u32  tensor_view_of(u32 src_id, View new_view);  // alias; bumps buf_incref

// === view/ ===
// Map an output flat index to the underlying buffer index through
// a (possibly non-contiguous) View.  Contiguous views short-circuit
// to flat_idx + offset; strided views walk per-axis strides.
fn u32 view_strided_index(View const *v, u32 flat_idx);

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

// Build a UOP_GRAD node.  y is the function output, gy is the
// cotangent seed (typically a CONST(1) for top-level VJP), target is
// the leaf TAG_TEN to differentiate against.  Reduces under TWnf via
// the chain-rule rewrite rule defined in interact/uop_grad.c.
fn Term uop_grad      (Term y, Term gy, Term target);
fn Term uop_grad_multi(Term y, Term gy, const Term *targets, u32 n);
fn u32  uop_grad_n    (Term grad_term);
fn Term uop_grad_target(Term grad_term, u32 i);

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
// docs/plans/saturation_loop.md for the design.  AtpState is heap-
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

  // Reduction ordering (caller-owned).
  const KboConfig *kbo;

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
} AtpState;

fn AtpState *thvm_atp_init        (const KboConfig *cfg, u32 step_cap);
fn void      thvm_atp_free        (AtpState *s);
fn u8        thvm_atp_add_equation(AtpState *s, Term lhs, Term rhs);
fn void      thvm_atp_set_goal    (AtpState *s, Term lhs, Term rhs);

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

typedef struct {
  char name[WALD_NAME_LEN];
  u32  label;       // CTR label assigned at parse time
  u32  arity;
  u32  prec_rank;   // 6.3c5: precedence position (0 = smallest;
                    //   higher index = greater).  All symbols start
                    //   at 0; ORDERING parser fills the chain in.
} WaldSym;

typedef struct {
  char name[WALD_NAME_LEN];
  u32  var_id;    // FVR id assigned at parse time
} WaldVar;

typedef struct {
  // Spec identity.  `mode_proof = 1` for "MODE PROOF", 0 for
  // "MODE COMPLETION" (defaults to 1 when unspecified).
  char    name[WALD_NAME_LEN];
  u8      mode_proof;

  // Signature: `symbols[0..n_symbols)` with monotonically-assigned
  // CTR labels via `next_label` (starts at 1; 0 is the CTR
  // "anonymous tuple" label so we skip it).
  WaldSym symbols[WALD_MAX_SYMBOLS];
  u32     n_symbols;
  u32     next_label;

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
} WaldSection;

fn WaldSection wald_section_from_ident(const char *name);
fn WaldSection wald_skip_to_section   (WaldLex *lex);

// 6.3c2: per-section parsers for the simple text sections.  Each
// takes a `WaldSpec *` (may be NULL to discard the parsed value),
// expects the lexer to be positioned just past the section header,
// reads the section content, and returns the next section's enum
// (or WSEC_NONE on EOF).  Falls back through `wald_skip_to_section`
// on unrecognized content.
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

// === runtime lifecycle ===
void thvm_init(void);
void thvm_free(void);

#endif // THVM_H
