// codegen/render_uop.c - UOp DAG renderer.
//
// Walks the UOp DAG rooted at a kernel-output store and emits MSL.
// Coverage:
//
//   UOP_BUFFER         -- kernel arg or local alloc
//   UOP_INDEX_E        -- buf[addr]
//   UOP_STORE          -- buf[addr] = value;
//   UOP_AFTER          -- threadgroup_barrier when cross-scope
//   UOP_RANGE          -- for-loop or thread-position bind
//   UOP_OPT            -- annotation on target (UNROLL/UPCAST/TC/...)
//   UOP_CONST/ICONST   -- literal value
//   UOP_IADD/IMUL/etc. -- symbolic int expressions
//   UOP_ADD/MUL/NEG/...  -- float elementwise + transcendentals
//   UOP_REDUCE         -- hoisted accumulator (init/accum/finalize)
//   UOP_CAST/BITCAST   -- type conversion
//   UOP_IWHERE         -- ternary
//
// Pattern-matches for specialised templates:
//   OPT(REDUCE(MUL(LOAD,LOAD), SUM, k), TC, _) with K%8==0
//     -> 8x8 simdgroup_matrix<float, 8, 8> template.
//   OPT(_, UNROLL/UPCAST, factor) -> #pragma unroll(N).
//   OPT(_, LOCAL, _) -> bind to thread_position_in_threadgroup.

static void rmu_emit_term(Term t, FILE *fp);

// Renderer target.  The body emit (rmu_emit_*) is shared across all
// three; targets differ in preamble/signature plus a handful of
// branches.  CG_TARGET_C (the CPU JIT, F6) renders axis-type
// LOCAL/GLOBAL as plain for-loops (no thread-position bind) and uses
// a plain function signature.  CG_TARGET_METAL and CG_TARGET_CUDA are
// both GPU targets: they share the thread/block-position binding and
// the reduce-unroll lowerings, and differ only in target-specific
// syntax (preamble, builtins, intrinsic spellings, the
// simdgroup_matrix vs WMMA matmul template).
typedef enum {
  CG_TARGET_METAL = 0,
  CG_TARGET_C     = 1,
  CG_TARGET_CUDA  = 2,
} cg_target;
static cg_target RMU_TARGET = CG_TARGET_METAL;

// 1 when the kernel carries a KAX_GROUP_REDUCE axis (the cooperative
// shared-memory reduce template): block size = group_extent, grid =
// product(LOOP outputs), so the promoted output axis must decode from
// `tg` (the block index) instead of the flat `tid`.
// Without this the renderer emits `a0 = (tid/.) % .` over a 1280-block
// launch with block=16 and each block computes 16 different outputs
// into one shared accumulator: data-race / OOB.
static int RMU_HAS_GROUP_REDUCE = 0;

// When a GROUP_REDUCE axis COEXISTS with LOCAL axes (tinygrad's matvec:
// GROUP the reduce + LOCAL the output row), the grouped axis is the INNERMOST
// local dim: the threadgroup is `local_total * group_extent` threads, `tt`
// decodes group_idx = tt % group_extent and the LOCAL axes from tt /
// group_extent.  RMU_GROUP_EXTENT shifts the LOCAL decode strides up by that
// factor; RMU_GROUP_LOCAL_TOTAL sizes the shared accumulator (local_total *
// group_extent) and drives the per-local final combine.  Both are inert when
// no LOCAL axis coexists (local_total == 1) -- the GROUP-only / GROUPTOP path
// (conv reductions) is byte-identical.
static int RMU_GROUP_EXTENT      = 0;   // group factor, 0 if no GROUP_REDUCE
static u32 RMU_GROUP_LOCAL_TOTAL = 1;   // product of LOCAL extents in the kernel

// Compute capability of the active CUDA device as a 2-digit int (V100 ->
// 70, A100 -> 80, H100 -> 90), set by cg_render_uop_kernel_cuda_root from
// cuda_device_sm().  The WMMA matmul template reads it to gate bf16
// fragments (need sm>=80) -- on a pre-Ampere device it stays at the
// fp16-only path.  0 when CUDA is not the target (the probe never ran).
static int RMU_CUDA_SM = 0;
#ifdef THVM_HAS_CUDA
fn int cuda_device_sm(void);
#endif

// Forward decl: defined below.
static int rmu_dag_has_group_reduce(Term t);
// The OPT_GROUP_REDUCE FACTOR (e.g. 8 -- the cooperative thread count, NOT the
// reduce axis's full extent), found by walking the OPT tree.
static u32 rmu_group_reduce_factor(Term t) {
  if (term_tag(t) != TAG_UOP) return 0;
  u32 op = term_ext(t); u64 loc = term_val(t);
  if (op == UOP_OPT) {
    if ((u32)term_val(heap_read(loc + 1)) == UOP_OPT_GROUP_REDUCE)
      return (u32)term_val(heap_read(loc + 2));
    return rmu_group_reduce_factor(heap_read(loc + 0));
  }
  if (op == UOP_RANGE || op == UOP_BUFFER || op == UOP_CONST
      || op == UOP_INVALID) return 0;
  u8 ar = uop_arity((u8)op);
  for (u8 i = 0; i < ar; i++) {
    u32 f = rmu_group_reduce_factor(heap_read(loc + i));
    if (f != 0) return f;
  }
  return 0;
}
// Compute (group_factor, product-of-LOCAL-extents) for a GROUP_REDUCE kernel.
static void rmu_group_local_dims(Term root, int *group_ext, u32 *local_total) {
  *group_ext = (int)rmu_group_reduce_factor(root);
  *local_total = 1;
  u32 ids[MAX_AXES], types[MAX_AXES], exts[MAX_AXES];
  u32 n = uop_dag_collect_axes(root, ids, types, exts, MAX_AXES);
  for (u32 i = 0; i < n; i++)
    if (exts[i] != 0 && types[i] == KAX_LOCAL) *local_total *= exts[i];
}

// Emit an unroll pragma matching the current target's syntax.  C99 /
// clang accept `#pragma clang loop unroll_count(N)`; MSL / GPU targets
// use the GCC-style `#pragma unroll(N)`.  factor==0 means "full".
static void rmu_emit_unroll_pragma(FILE *fp, u32 factor) {
  if (RMU_TARGET == CG_TARGET_C) {
    if (factor > 0) fprintf(fp, "#pragma clang loop unroll_count(%u)\n", factor);
    else            fputs("#pragma clang loop unroll(full)\n", fp);
  } else {
    if (factor > 0) fprintf(fp, "#pragma unroll(%u)\n", factor);
    else            fputs("#pragma unroll\n", fp);
  }
}

// Reduce-axis loop unroll threshold.  When a scalar accumulator's
// reduce axis has extent <= this and the target is MSL (not C99), the
// renderer emits `#pragma unroll(<extent>)` immediately above the
// `for`-loop so the MSL compiler can straight-line the K MAD ops --
// matmul's K=25 contraction, conv's K=9/27, etc.  Larger reduces stay
// on the rolled loop to keep the generated body size sane.
//
// Lowered from 64 -> 16 (2026-05-27) after V100 measurement showed
// nvrtc spending 350-390 s cold-compiling conv kid=3 with all three
// reduce axes (32, 5, 5) `#pragma unroll`'d AND four parallel
// accumulators (F=4): 32*5*5*5 stmts = 4000 explicit PTX lines.  With
// threshold 16 the K=32 outer stays as a runtime for-loop; only the
// kH/kW (extent 5) inner stays unrolled.  nvcc handles 5*5 = 25
// inline iterations of a 5-stmt body easily.  Cold step 1 dropped
// from 390 s -> 54 s with this change.
#define RMU_REDUCE_UNROLL_MAX 16u

// Buffer name resolution.
//
// Production callers resolve buffer names through the UOP_BUFFER
// `instance` field (kernel_lift.c sets instance=0 on the output and
// instance=slot+1 on the i-th input).  rmu_buf_name(t) decodes
// instance directly:
//
//   instance == 0  -> "out" (resolved via the legacy map below)
//   instance >= 1  -> "in<instance-1>"
//
// This drops the renderer's prior dependency on Term-identity matches
// against an `in_bufs[]` array passed in from the caller for input
// naming; lift result and renderer agree on input naming via stable
// structural indices.  Slot 3 -> "in2" regardless of what Term the
// lifter happened to hash-cons for that slot in this session.
//
// The legacy Term-identity map (populated by rmu_buf_names_set) is
// retained for two reasons:
//   1. Synthetic test kernels (tests/test_render_uop.c) build
//      UOP_BUFFER leaves via uop_buffer(...) which leaves instance==0
//      on every leaf; the test entry point cg_render_uop_kernel(...)
//      registers each one explicitly so the structural fallback still
//      lands on "out" / "inN".
//   2. The output buffer's instance is 0 for both lifted and
//      synthetic kernels; the map disambiguates by Term identity (one
//      output per kernel makes this unambiguous in practice).
//
// Static globals are fine; the renderer isn't re-entrant in practice
// and the map is cleared at the start of each render.
// RMU_BUF_MAX bounds the number of distinct BUFFER / BUFFERIZE Terms
// rmu_discover_bufs_rec can promote to input slots in one kernel.
// Metal caps kernel-argument buffers at 31; the lift / Metal codegen
// gate that elsewhere.  On CPU / CUDA there is no hard limit, but the
// renderer's static slot table needs a fixed cap; 64 matches
// KERNEL_LIFT_MAX_INPUT (the kernel_lift cap on n_inputs) so the
// discover walk never spills on a kernel the lift accepted.
//
// Going below the lift cap was a real bug: when no REDUCE is force-
// realized, the BN-train mean/var/inv broadcast graphs fuse into the
// downstream MaxPool kernel and pull the BN params (mean, var, scale,
// shift, eps, plus the BUFFERIZE handles for the inner reduces) past
// the 32-slot ceiling -- the overflow BUFFERIZEs then hit
// rmu_buf_name's `buf<term_val>` fallback and the JIT'd C source
// references undeclared identifiers (build fails, dispatch falls to
// the walker, walker doesn't terminate at BS=2 on MNIST).
#define RMU_BUF_MAX 64
static struct { Term term; char name[16]; } RMU_BUF_NAMES[RMU_BUF_MAX];
static u32 RMU_BUF_NAMES_N;

static void rmu_buf_names_reset(void) {
  RMU_BUF_NAMES_N = 0;
}


// Register a Term -> name mapping in the legacy fallback map.  Used
// by the cg_render_uop_kernel(out_buf, in_bufs[]) entry points to
// keep test-built kernels (instance==0 everywhere) renderable, and to
// register the output's "out" name (whose lifter-assigned instance is
// 0 and so doesn't carry a structural slot).
static void rmu_buf_names_set(Term t, const char *name) {
  if (RMU_BUF_NAMES_N >= RMU_BUF_MAX) return;
  RMU_BUF_NAMES[RMU_BUF_NAMES_N].term = t;
  snprintf(RMU_BUF_NAMES[RMU_BUF_NAMES_N].name,
           sizeof(RMU_BUF_NAMES[0].name), "%s", name);
  RMU_BUF_NAMES_N++;
}

// Returns the symbolic name for buffer `t`, or NULL when unresolved.
//
// Resolution order:
//   1. UOP_BUFFER.instance >= 1 -> "in<instance-1>" (structural).
//   2. Legacy Term-identity map (the output's "out" entry, plus all
//      synthetic test kernels).
//   3. NULL -- caller decides how to handle (rmu_buf_name emits the
//      `buf<loc>` fallback for back-compat; INDEX_E emit path treats
//      NULL as "matches cpu_uop_walk's unresolved-load returns 0.0"
//      semantics and emits a zero constant instead).
static const char *rmu_buf_name_or_null(Term t) {
  // Structural path: instance>=1 means "input slot (instance-1)".
  // Lifted kernels use this for every input; the output (instance==0)
  // falls through to the legacy map.
  u32 inst = uop_buffer_inst_get(t);
  if (inst >= 1) {
    static char structural[16];
    snprintf(structural, sizeof(structural), "in%u", inst - 1);
    return structural;
  }
  for (u32 i = 0; i < RMU_BUF_NAMES_N; i++) {
    if (RMU_BUF_NAMES[i].term == t) return RMU_BUF_NAMES[i].name;
  }
  return NULL;
}

// Returns the symbolic name for buffer `t`.  Same as
// rmu_buf_name_or_null but synthesizes a `buf<loc>` fallback for the
// legacy callers that don't yet handle NULL.  New emit sites that
// need to specialise on unresolved should call _or_null directly.
static const char *rmu_buf_name(Term t) {
  const char *nm = rmu_buf_name_or_null(t);
  if (nm != NULL) return nm;
  static char fallback[24];
  snprintf(fallback, sizeof(fallback), "buf%llu",
           (unsigned long long)term_val(t));
  return fallback;
}

static const char *rmu_msl_type_name(u32 dtype) {
  switch (dtype) {
    case DT_FP32:  return "float";
    case DT_FP16:  return "half";
    // Metal 3.1+ (macOS 14+, Apple GPU family >= 6) has a native
    // `bfloat` scalar with hardware bfloat<->float conversion, so a
    // DT_BF16 buffer stays 2 bytes/element on the GPU and ops read it
    // as bfloat.  Mirrors tinygrad MetalRenderer.type_map =
    // {dtypes.bfloat16: "bfloat"} (tinygrad/renderer/cstyle.py:339).
    // The in-process JIT compile pins -std=metal3.1 (see
    // src/backend/metal/_.m metal_tile_jit_build) so `bfloat` resolves.
    case DT_BF16:  return "bfloat";
    case DT_INT32: return "int";
    case DT_INT64: return "long";
    case DT_INT8:  return "char";   // MSL char is signed 8-bit (tinygrad type_map)
    case DT_UINT8: return "uchar";
    default:       return "float";  // safe fallback for the renderer
  }
}

// CUDA scalar type names.  Differs from MSL on the fp16 / bf16 / byte
// types: CUDA spells half `__half` (cuda_fp16.h) and bfloat16
// `__nv_bfloat16` (cuda_bf16.h), bytes `unsigned char`.  Without the
// DT_BF16 case a bf16 buffer fell through to `float`, so kernels indexed
// a 2-byte-per-element buffer as 4-byte float -> writes overran (illegal
// address) and readback reinterpreted f32 bytes as bf16 pairs (garbage).
// __nv_bfloat16 has float<->bf16 conversion operators + arithmetic on
// sm_80+, mirroring the MSL native `bfloat`.
static const char *rmu_cuda_type_name(u32 dtype) {
  switch (dtype) {
    case DT_FP32:  return "float";
    case DT_FP16:  return "__half";
    case DT_BF16:  return "__nv_bfloat16";
    case DT_INT32: return "int";
    case DT_INT64: return "long long";
    case DT_INT8:  return "signed char";
    case DT_UINT8: return "unsigned char";
    default:       return "float";
  }
}

// GPU-target type name: dispatches MSL vs CUDA on RMU_TARGET.  The
// shared body emit calls this on either GPU target; the CPU JIT path
// calls rmu_c_type_name directly.
static const char *rmu_gpu_type_name(u32 dtype) {
  return (RMU_TARGET == CG_TARGET_CUDA) ? rmu_cuda_type_name(dtype)
                                        : rmu_msl_type_name(dtype);
}

// Cast / ALU type name (a kernel BODY value, not a buffer pointer).  On
// CUDA, bf16/fp16 are storage-only: their cuda_bf16.h / cuda_fp16.h
// arithmetic operators collide with a float accumulator -- e.g. a matmul
// MAC `float_acc + (__nv_bfloat16)a * (__nv_bfloat16)b` fails nvrtc with
// "more than one operator + matches: float + __nv_bfloat16".  So an
// intermediate cast to bf16/fp16 upcasts to float; the buffer POINTER
// keeps __nv_bfloat16*/__half* (rmu_cuda_type_name) and the store converts
// implicitly.  Metal's native bfloat/half auto-promote in arithmetic, so
// they stay as-is there (the simdgroup bf16-fragment path relies on it).
static const char *rmu_gpu_alu_type_name(u32 dtype) {
  if (RMU_TARGET == CG_TARGET_CUDA && (dtype == DT_BF16 || dtype == DT_FP16))
    return "float";
  return rmu_gpu_type_name(dtype);
}

// Kernel-signature dtype for a discovered buffer slot.  Slots come in
// two shapes: a real UOP_BUFFER (dtype in heap[loc+1]) or a bare
// TAG_TEN leaf the unified pass / kernel_lift left in the DAG (dtype in
// term_ext -- see thvm.h:127 `TAG_TEN ... ext = dtype`).  uop_buffer_dtype
// only handles the former and returns 0 for a TAG_TEN, which the type
// mappers then render as the default `float`.  That silently mis-typed
// every non-f32 TAG_TEN input (e.g. an FP16/BF16 weight or an i32 index
// tensor) as `device const float*`, so a 2-byte bf16 buffer was read 4
// bytes at a stride -> garbage.  Read the dtype from whichever leaf
// shape `t` actually is.
static u32 rmu_slot_dtype(Term t) {
  if (term_tag(t) == TAG_TEN) return term_ext(t);
  return uop_buffer_dtype(t);
}

// Store-value down-conversion at a `buf[addr] = <rhs>;` writeback.  The
// reduce / matmul accumulator (and any promoted compute expression) is
// `float`, but the destination buffer may be a narrower float type.
// Metal's `bfloat` lvalue assignment rejects an implicit float->bfloat
// narrowing (unlike `half`, which is permissive), so wrap the rhs in an
// explicit conversion when the destination is bf16/fp16.  f32 / int
// buffers emit nothing -> byte-identical output for the existing paths.
// Scoped to the Metal target: only MSL has the native `bfloat` lvalue
// with the strict assignment rule.  The C JIT promotes bf16/fp16 to
// float (rmu_c_type_name) so a float rhs is already correct, and CUDA
// bf16 output buffers aren't routed here today -- both fall through with
// no wrapper, keeping their source byte-identical.
static void rmu_store_cast_open(Term buf, FILE *fp) {
  if (RMU_TARGET != CG_TARGET_METAL) return;
  u32 dt = rmu_slot_dtype(buf);
  if (dt == DT_BF16 || dt == DT_FP16) {
    fprintf(fp, "%s(", rmu_msl_type_name(dt));
  }
}
static void rmu_store_cast_close(Term buf, FILE *fp) {
  if (RMU_TARGET != CG_TARGET_METAL) return;
  u32 dt = rmu_slot_dtype(buf);
  if (dt == DT_BF16 || dt == DT_FP16) {
    fputc(')', fp);
  }
}

// F6: C99 lacks `half` and `uchar`; emit equivalents that math.h /
// stdint.h cover. fp16 falls back to `float` for now -- caller (cpu/jit.c)
// will need to widen DT_FP16 inputs at the host boundary if they're
// ever routed through this path.
static const char *rmu_c_type_name(u32 dtype) {
  switch (dtype) {
    case DT_BOOL:  return "unsigned char";
    case DT_FP32:  return "float";
    case DT_FP64:  return "double";
    case DT_FP16:  return "float";   // promote-to-f32; load/store via fp_convert
    case DT_BF16:  return "float";   // ditto
    case DT_FP8E4M3:
    case DT_FP8E5M2: return "float"; // ditto
    case DT_INT8:  return "signed char";
    case DT_UINT8: return "unsigned char";
    case DT_INT16: return "short";
    case DT_UINT16:return "unsigned short";
    case DT_INT32: return "int";
    case DT_UINT32:return "unsigned int";
    case DT_INT64: return "long";
    case DT_UINT64:return "unsigned long";
    default:       return "float";
  }
}

static const char *rmu_int_op_name(u32 op) {
  switch (op) {
    case UOP_IADD: return "+";
    case UOP_ISUB: return "-";
    case UOP_IMUL: return "*";
    case UOP_IDIV: return "/";
    case UOP_IMOD: return "%";
    case UOP_ILT:  return "<";
    case UOP_IAND: return "&";
    case UOP_IOR:  return "|";
    case UOP_IXOR: return "^";
    case UOP_ISHR: return ">>";
    default:       return "?";
  }
}

// Hoisted-subexpression map: when the parallel-accumulator emit path
// (rmu_emit_store_reduce, path #2 of docs/tinygrad_late_passes.md)
// pre-emits a shared load to a local variable, every rmu_emit_term
// recursion below the substituted Term short-circuits to the variable
// name.  Keys are Terms in the ORIGINAL pre-rewrite red_src; the per-k
// uop_graph_rewrite is identity on non-UPCAST'd subtrees (hash-consed
// rebuild), so the same key matches in every per-k rewrite output.
// Modelled on tinygrad/codegen/late/devectorizer.py:81-117
// fold_expanded_index, which folds adjacent INDEX nodes into one wider
// CAT'd INDEX + per-lane GEP at the UOp-graph level; thvm doesn't have
// a full UOp-graph expander, so we do the equivalent at source-emission
// time in the renderer.
#define RMU_HOIST_MAX 64
typedef struct {
  Term       key;   // Term to substitute; 0 = empty slot
  char const *name; // variable name to emit instead (rmu_emit_term writes verbatim)
} RmuHoistSlot;
static RmuHoistSlot RMU_HOIST_MAP[RMU_HOIST_MAX];
static u32 RMU_HOIST_N = 0;

// Accumulator-lane suffix: when register-blocking an UPCAST'd output axis
// in the generic store path, each lane k gets its own `_acc<axis>_<k>`.
// rmu_emit_term appends this suffix to the `_acc<axis>` it emits for a
// UOP_REDUCE expression leaf, so a store value referencing the reduce
// picks up the right per-lane accumulator.  Empty string = no suffix (the
// default scalar / GPU parallel-acc paths set their names explicitly).
static char RMU_ACC_LANE_SUFFIX[16] = {0};

static void rmu_hoist_clear(void) {
  for (u32 i = 0; i < RMU_HOIST_N; i++) {
    RMU_HOIST_MAP[i].key  = 0;
    RMU_HOIST_MAP[i].name = NULL;
  }
  RMU_HOIST_N = 0;
}

static int rmu_hoist_add(Term key, char const *name) {
  if (RMU_HOIST_N >= RMU_HOIST_MAX) return 0;
  RMU_HOIST_MAP[RMU_HOIST_N].key  = key;
  RMU_HOIST_MAP[RMU_HOIST_N].name = name;
  RMU_HOIST_N++;
  return 1;
}

static char const *rmu_hoist_lookup(Term key) {
  for (u32 i = 0; i < RMU_HOIST_N; i++) {
    if (RMU_HOIST_MAP[i].key == key) return RMU_HOIST_MAP[i].name;
  }
  return NULL;
}

// True if `t` renders as an already-signed-`int` integer-binary expression.
// The int-binary renderer (below) casts each OPERAND to `int` so the whole
// integer subtree is signed -- but that cast is only needed on uint LEAVES
// (RANGE loop vars, declared uint).  A nested int binary is already int, so
// re-wrapping it in `(int)(...)` is pure bloat: thvm's index expressions nest
// deeply (strided/reshaped addressing), and the per-binary cast multiplied the
// rendered index MSL ~3-5x -- which dominated Metal compile time and made the
// fused-matmul A-staging (producer inlined x simdgroup unroll) explode to
// minutes-long compiles.  Dropping the redundant nested-binary cast keeps the
// subtree signed (leaves still cast) while shrinking the index MSL.
static int rmu_is_int_binary(Term t) {
  if (term_tag(t) != TAG_UOP) return 0;
  switch (term_ext(t)) {
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL:
    case UOP_IDIV: case UOP_IMOD: case UOP_ILT:
    case UOP_IAND: case UOP_IOR:  case UOP_IXOR: case UOP_ISHR:
      return 1;
    default: return 0;
  }
}

// Emit a symbolic int expression (UOP_RANGE / UOP_I* / UOP_IWHERE /
// UOP_INVALID / UOP_CONST).  Recursive; parenthesises binary ops to
// keep precedence unambiguous.
static void rmu_emit_term(Term t, FILE *fp) {
  if (RMU_HOIST_N > 0) {
    char const *hn = rmu_hoist_lookup(t);
    if (hn != NULL) { fputs(hn, fp); return; }
  }
  if (term_tag(t) != TAG_UOP) {
    if (term_tag(t) == TAG_NUM) {
      fprintf(fp, "%u", (u32)term_val(t));
      return;
    }
    fputs("/*?*/", fp);
    return;
  }
  u32 op = term_ext(t);
  u64 loc = term_val(t);
  switch (op) {
    case UOP_CONST: {
      u32 dtype = term_ext(heap_read(loc));
      u32 bits  = (u32)term_val(heap_read(loc));
      if (dtype == DT_FP32) {
        // Emit f32 as a bit-exact bitcast so the constant survives
        // decimal round-trip without compiler-side fp formatting
        // drift.  C target uses the THVM_BITCAST macro from the
        // prologue; Metal uses MSL's `as_type<float>`; CUDA uses the
        // `__uint_as_float` device intrinsic (nvrtc has no `as_type`).
        if (RMU_TARGET == CG_TARGET_C) {
          fprintf(fp, "THVM_BITCAST(float, 0x%08xu)", bits);
        } else if (RMU_TARGET == CG_TARGET_CUDA) {
          fprintf(fp, "__uint_as_float(0x%08xu)", bits);
        } else {
          fprintf(fp, "as_type<float>(0x%08xu)", bits);
        }
      } else if (dtype_is_float(dtype)) {
        // Non-f32 float dtypes (f64 / f16 / bf16 / fp8): the `bits`
        // field carries an f32 IEEE-754 bit pattern (the literal the
        // grad chain rule and gradOnesSeed emit -- see const_to_tendesc,
        // which decodes the same way at materialize time).  Decode it to
        // an f32 value and emit a decimal literal so it lands as the
        // correct numeric value in the kernel's compute type (`double`
        // for f64, `float` for f16/bf16/fp8 -- see rmu_c_type_name).
        // Emitting `(int)bits` here would inject the raw bit pattern
        // (e.g. 1065353216 for 1.0f) into the kernel.  f32->double is
        // exact, so %.17g round-trips losslessly.
        f32 fv; memcpy(&fv, &bits, sizeof(fv));
        fprintf(fp, "%.17g", (double)fv);
      } else {
        fprintf(fp, "%d", (int)bits);
      }
      return;
    }
    case UOP_RANGE: {
      u32 axis_id = term_val(heap_read(loc + 0));
      fprintf(fp, "a%u", axis_id);
      return;
    }
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL:
    case UOP_IDIV: case UOP_IMOD: case UOP_ILT:
    case UOP_IAND: case UOP_IOR:  case UOP_IXOR: case UOP_ISHR: {
      // Signed integer arithmetic.  RANGE loop vars are declared `uint`
      // (loop counters, always non-negative), but UOP_ISUB is a SIGNED
      // subtract (thvm.h:368) and may legitimately go negative -- e.g.
      // a conv/attention shifted index `r - k` or a guard `-1 < x`.
      // In unsigned space a negative ISUB wraps to ~UINT_MAX, so an
      // `ILT` against it (`-1 < x` promotes -1 to UINT_MAX) is always
      // false and a negative array index wraps far out of bounds.
      // Casting each operand to `int` makes the whole integer-expression
      // subtree signed: subtraction, comparison and idiv/imod are all
      // signed-correct, and a signed index used in `buf[expr]` is fine
      // on every target (the IWHERE/ILT bounds guard masks it).  This
      // matches tinygrad, which renders RANGE/index dtype as signed
      // `int` (tinygrad/renderer/cstyle.py:18-19,150).  Nested int
      // binaries already yield `int`; the extra `(int)` cast on them is
      // harmless.  Float ops are untouched -- this case only covers the
      // UOP_I* family.
      // Cast each operand to signed `int`, EXCEPT operands that are already
      // int binaries (re-casting them just bloats the index MSL -- see
      // rmu_is_int_binary).  Leaves (RANGE uint / CONST) keep the cast so the
      // whole subtree stays signed-correct (ISUB / ILT / IDIV / IMOD).
      Term a = heap_read(loc + 0);
      Term b = heap_read(loc + 1);
      fputc('(', fp);
      if (rmu_is_int_binary(a)) {
        rmu_emit_term(a, fp);
      } else {
        fputs("(int)(", fp);  rmu_emit_term(a, fp);  fputc(')', fp);
      }
      fprintf(fp, " %s ", rmu_int_op_name(op));
      if (rmu_is_int_binary(b)) {
        rmu_emit_term(b, fp);
      } else {
        fputs("(int)(", fp);  rmu_emit_term(b, fp);  fputc(')', fp);
      }
      fputc(')', fp);
      return;
    }
    // Float elementwise binary ops.  Same parenthesised shape as
    // the int binaries; renderer emits MSL infix operators.
    case UOP_ADD: case UOP_MUL: case UOP_CMPLT: case UOP_CMPEQ: {
      const char *infix = (op == UOP_ADD)   ? "+"
                        : (op == UOP_MUL)   ? "*"
                        : (op == UOP_CMPLT) ? "<"
                        :                     "==";
      Term a = heap_read(loc + 0);
      Term b = heap_read(loc + 1);
      fputs("(", fp);
      rmu_emit_term(a, fp);
      fprintf(fp, " %s ", infix);
      rmu_emit_term(b, fp);
      fputs(")", fp);
      return;
    }
    // Float elementwise unary ops.  RECIP -> 1.0f/x, EXP2 / LOG2 /
    // SQRT use MSL builtins.  NEG via prefix `-`.
    case UOP_NEG: {
      fputs("(-", fp);
      rmu_emit_term(heap_read(loc + 0), fp);
      fputs(")", fp);
      return;
    }
    case UOP_RECIP: {
      fputs("(1.0f/", fp);
      rmu_emit_term(heap_read(loc + 0), fp);
      fputs(")", fp);
      return;
    }
    case UOP_EXP2: case UOP_LOG2: case UOP_SQRT: {
      const char *fn_name = (op == UOP_EXP2) ? "exp2"
                          : (op == UOP_LOG2) ? "log2"
                          :                    "sqrt";
      fprintf(fp, "%s(", fn_name);
      rmu_emit_term(heap_read(loc + 0), fp);
      fputs(")", fp);
      return;
    }
    case UOP_CAST: case UOP_BITCAST: {
      // heap = [src, NUM(dst_dtype)].
      Term src      = heap_read(loc + 0);
      u32  dst_dt   = term_val(heap_read(loc + 1));
      const char *type_name = (RMU_TARGET == CG_TARGET_C)
                                 ? rmu_c_type_name(dst_dt)
                                 : rmu_gpu_alu_type_name(dst_dt);
      if (op == UOP_BITCAST) {
        if (RMU_TARGET == CG_TARGET_C) {
          // C99 bitcast via the THVM_BITCAST statement-expression
          // macro emitted in the C-target prologue.  Pattern:
          //   THVM_BITCAST(<dst>, <expr>)
          //  -> ({ <dst> _tmp; memcpy(&_tmp, &(_src), sizeof(_tmp)); _tmp; })
          // Statement-expressions are a GCC/clang extension; both
          // compilers we target accept them.
          fprintf(fp, "THVM_BITCAST(%s, ", type_name);
          rmu_emit_term(src, fp);
          fputs(")", fp);
        } else if (RMU_TARGET == CG_TARGET_CUDA) {
          // CUDA has no MSL `as_type<T>`: a 32-bit reinterpret uses
          // the `__*_as_*` device intrinsics.  Pick by dst dtype --
          // int<->float is the case the renderer actually emits (the
          // fp32-const bitcast above + integer-bit tricks); anything
          // wider falls back to a value cast (best effort -- the
          // renderer never bitcasts 64-bit types today).
          const char *intrin =
              (dst_dt == DT_FP32)  ? "__uint_as_float" :
              (dst_dt == DT_INT32) ? "__float_as_int"  : NULL;
          if (intrin != NULL) {
            fprintf(fp, "%s(", intrin);
            rmu_emit_term(src, fp);
            fputs(")", fp);
          } else {
            fprintf(fp, "((%s)", type_name);
            rmu_emit_term(src, fp);
            fputs(")", fp);
          }
        } else {
          fprintf(fp, "as_type<%s>(", type_name);
          rmu_emit_term(src, fp);
          fputs(")", fp);
        }
      } else if (dst_dt == DT_BOOL) {
        // CAST-to-bool is truthiness (!= 0), not numeric truncation.
        // Negative floats (`-2.0`) would otherwise wrap as 0xFE in u8;
        // tinygrad's BOOL semantics match Python's `bool(x)` instead.
        fputs("((", fp);
        fputs(type_name, fp);
        fputs(")(", fp);
        rmu_emit_term(src, fp);
        fputs(" != 0))", fp);
      } else {
        fprintf(fp, "((%s)", type_name);
        rmu_emit_term(src, fp);
        fputs(")", fp);
      }
      return;
    }
    case UOP_IWHERE: {
      Term cond = heap_read(loc + 0);
      Term tv   = heap_read(loc + 1);
      Term ev   = heap_read(loc + 2);
      fputs("(", fp);
      rmu_emit_term(cond, fp);
      fputs(" ? ", fp);
      rmu_emit_term(tv, fp);
      fputs(" : ", fp);
      rmu_emit_term(ev, fp);
      fputs(")", fp);
      return;
    }
    case UOP_INVALID:
      fputs("/*INVALID*/0", fp);
      return;
    case UOP_INDEX_E: {
      Term buf  = heap_read(loc + 0);
      Term addr = heap_read(loc + 1);
      // Buffer is rendered by name via rmu_buf_name_or_null; the
      // cg_render_uop_kernel_root / _c_root entries wire names through
      // rmu_discover_bufs_rec so the structural ("in<i-1>") and legacy
      // map ("out") paths resolve every UOP_BUFFER in the kernel.
      //
      // Unresolved buf: BUFFERIZE residue the unified pass declined to
      // inline (the producer's REDUCE-over-pool-axis whose closed_range
      // type=1 leaf bails materialize.c's 1-axis decomp).
      // rmu_discover_bufs_rec treats the BUFFERIZE as an OPAQUE leaf
      // and does NOT promote it to a slot; cpu_uop_walk's
      // uwalk_resolve_buf returns 0 for the same case (the BUFFERIZE
      // Term doesn't match any cached_lift.in_bufs[] entry) and INDEX_E
      // loads 0.0.  Mirror that semantics here -- emit a 0.0f literal
      // so the JIT'd C compiles AND produces byte-identical output to
      // the walker.  Without this the body emits `buf<loc>[addr]`
      // (undeclared identifier -> clang fails -> dispatch falls back
      // to the slow walker), and the [16,32,10,10] / [16,64,3,3]
      // backward kernels in beautiful_mnist stay on uop_walk (~19% of
      // total wall at BS=16).
      const char *bn = rmu_buf_name_or_null(buf);
      if (bn == NULL) {
        fputs("0.0f", fp);
        return;
      }
      // On CUDA, upcast a bf16/fp16 buffer load to float at the load site:
      // __nv_bfloat16/__half arithmetic operators (cuda_bf16.h/fp16.h) clash
      // with a float accumulator -- a raw matmul MAC `float_acc + in0[i]*in1[i]`
      // over bf16 buffers is "more than one operator + matches: float +
      // __nv_bfloat16".  Computing every bf16/fp16 value as float (storage
      // stays 2-byte; the store converts back implicitly) mirrors the C
      // target (bf16 -> "float") and Metal's native bfloat auto-promotion.
      u32 ldt = rmu_slot_dtype(buf);
      int cuda_half_load = (RMU_TARGET == CG_TARGET_CUDA
                            && (ldt == DT_BF16 || ldt == DT_FP16));
      if (cuda_half_load) fputs("((float)", fp);
      fprintf(fp, "%s[", bn);
      rmu_emit_term(addr, fp);
      fputs("]", fp);
      if (cuda_half_load) fputs(")", fp);
      return;
    }
    case UOP_BUFFER:
      fputs(rmu_buf_name(t), fp);
      return;
    case UOP_OPT: {
      // Annotation: render the target, ignore the directive in F0
      // (F1+ pattern-matches for specialised templates such as TC /
      // UPCAST / UNROLL / LOCAL / GROUP_REDUCE).
      Term inner = heap_read(loc + 0);
      rmu_emit_term(inner, fp);
      return;
    }
    case UOP_REDUCE: {
      // When REDUCE appears in an expression context (not directly as
      // STORE.value), the caller has hoisted an accumulator outside.
      // We emit the placeholder name `_acc<axis_0>`; the caller emits
      // the init / reduce-axis loop / combine code before the
      // expression and just substitutes here.  Multi-axis REDUCE uses
      // its first axis as the accumulator name suffix (the hoisted
      // accumulator is shared across all axes of one REDUCE node).
      Term tred = term_new(0, TAG_UOP, op, loc);
      u32 axis0 = uop_reduce_axis(tred, 0);
      // The accumulator is wider than the REDUCE's output dtype (it sums in
      // float -- rmu_emit_reduce_init/combine).  A REDUCE value feeding an
      // elementwise expression carries the REDUCE's dtype, so a narrow-float
      // (bf16/fp16) reduce must round to that dtype HERE -- exactly as a
      // realized buffer of that dtype would.  This makes a matmul-epilogue
      // fused store (THVM_FUSE_MATMUL_EPILOGUE: ELEMENTWISE(...REDUCE...))
      // bit-identical to the un-fused path (matmul -> bf16 buffer -> ew):
      // the bf16 round of the matmul output happens before the elementwise
      // in both.  f32/f64 reduces are unchanged (the accumulator already
      // matches), so softmax/LayerNorm/etc. are byte-identical.
      u32 red_dt = DT_FP32;
      char const *cast_open = "", *cast_close = "";
      if (term_dtype_in(tred, 0, &red_dt)) {
        if (red_dt == DT_BF16) { cast_open = "((bfloat)("; cast_close = "))"; }
        else if (red_dt == DT_FP16) { cast_open = "((half)("; cast_close = "))"; }
      }
      fprintf(fp, "%s_acc%u%s%s", cast_open, axis0, RMU_ACC_LANE_SUFFIX,
              cast_close);
      return;
    }
    default:
      fprintf(fp, "/*uop%u*/", op);
      return;
  }
}

// Walk a term tree collecting unique UOP_RANGE leaves in encounter
// order, plus optional UOP_OPT annotations attached to each range.
// Used by rmu_emit_store to wrap the store body in for-loops over
// every range that appears in the addr / value expressions.
//
// Up to MAX_DIM ranges per kernel; duplicates skipped (same axis_id
// only emits one for-loop).  When a range is encountered via a
// wrapping OPT(range, kind, factor) we record (kind, factor) into
// `opt_kinds[]` / `opt_factors[]`.  RMU_NO_OPT marks "no OPT wrap"
// distinctly from "OPT(_, UNROLL, _)" since UOP_OPT_UNROLL == 0
// would otherwise collide with the zero-init default.
#define RMU_NO_OPT 0xFFu

// Per-render cap on collected ranges.  Larger than MAX_DIM (tensor
// rank) because a kernel's iter scope can combine many axes: output
// axes + auxiliary LOOP axes inside reduce bodies + reduce-axes
// themselves.  Bench-train backward kernels touching 5-layer
// conv+BN+maxpool chains can reach ~12 distinct axes.  32 matches
// RMU_BUF_MAX (the Metal buffer-attribute cap) for symmetry.
#define RMU_MAX_RANGES 32

static void rmu_collect_ranges_rec_cap(Term t, Term *ranges,
                                       u32 *opt_kinds, u32 *opt_factors,
                                       u32 *n_out, u32 cap,
                                       u32 inherit_kind, u32 inherit_factor) {
  if (term_tag(t) != TAG_UOP) return;
  if (*n_out >= cap) return;
  u32 op = term_ext(t);
  u64 loc = term_val(t);
  if (op == UOP_RANGE) {
    u32 axis_id = term_val(heap_read(loc + 0));
    for (u32 i = 0; i < *n_out; i++) {
      u32 existing = term_val(heap_read(term_val(ranges[i]) + 0));
      if (existing == axis_id) return;
    }
    ranges     [*n_out] = t;
    opt_kinds  [*n_out] = inherit_kind;
    opt_factors[*n_out] = inherit_factor;
    (*n_out)++;
    return;
  }
  switch (op) {
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL: case UOP_IDIV:
    case UOP_IMOD: case UOP_ILT:  case UOP_IAND: case UOP_IOR: case UOP_IXOR:
    case UOP_ISHR:
    case UOP_ADD:  case UOP_MUL:  case UOP_CMPLT: case UOP_CMPEQ:
    case UOP_INDEX_E:
      rmu_collect_ranges_rec_cap(heap_read(loc + 0), ranges, opt_kinds,
                                 opt_factors, n_out, cap, RMU_NO_OPT, 0);
      rmu_collect_ranges_rec_cap(heap_read(loc + 1), ranges, opt_kinds,
                                 opt_factors, n_out, cap, RMU_NO_OPT, 0);
      return;
    case UOP_NEG:   case UOP_RECIP: case UOP_EXP2:
    case UOP_LOG2:  case UOP_SQRT:
      rmu_collect_ranges_rec_cap(heap_read(loc + 0), ranges, opt_kinds,
                                 opt_factors, n_out, cap, RMU_NO_OPT, 0);
      return;
    case UOP_IWHERE:
      rmu_collect_ranges_rec_cap(heap_read(loc + 0), ranges, opt_kinds,
                                 opt_factors, n_out, cap, RMU_NO_OPT, 0);
      rmu_collect_ranges_rec_cap(heap_read(loc + 1), ranges, opt_kinds,
                                 opt_factors, n_out, cap, RMU_NO_OPT, 0);
      rmu_collect_ranges_rec_cap(heap_read(loc + 2), ranges, opt_kinds,
                                 opt_factors, n_out, cap, RMU_NO_OPT, 0);
      return;
    case UOP_OPT: {
      // OPT(target, kind, factor): inherit annotation into the
      // recursed target's collection.  Stacked OPTs accumulate the
      // outermost kind (last-wins for now).
      u32 kind   = term_val(heap_read(loc + 1));
      u32 factor = term_val(heap_read(loc + 2));
      rmu_collect_ranges_rec_cap(heap_read(loc + 0), ranges, opt_kinds,
                                 opt_factors, n_out, cap, kind, factor);
      return;
    }
    case UOP_CAST: case UOP_BITCAST:
      rmu_collect_ranges_rec_cap(heap_read(loc + 0), ranges, opt_kinds,
                                 opt_factors, n_out, cap, RMU_NO_OPT, 0);
      return;
    default:
      return;
  }
}

// Default-cap wrapper used by callers that don't need more than MAX_DIM
// ranges (the most common case -- e.g. per-reduce body-scans for
// required_pos and reduce-axis lookup, where the iter scope is bounded
// by tensor rank).  rmu_emit_store uses the _cap variant directly with
// RMU_MAX_RANGES so kernel-iter scopes spanning many axes don't drop
// late LOOP axes.
static void rmu_collect_ranges_rec(Term t, Term *ranges,
                                   u32 *opt_kinds, u32 *opt_factors,
                                   u32 *n_out,
                                   u32 inherit_kind, u32 inherit_factor) {
  rmu_collect_ranges_rec_cap(t, ranges, opt_kinds, opt_factors, n_out,
                             MAX_DIM, inherit_kind, inherit_factor);
}

// Walks the term subgraph rooted at `t` (descending through every
// operand slot we care about) and returns 1 iff `needle` appears as
// a node anywhere inside.  Used to detect reduce nesting (the
// nested-reduce / body-rewrap shape) when computing `required_pos`
// in the generic store path.  Bounded recursion by a `depth` budget
// so a malformed cyclic DAG can't drive an infinite descent.
static int rmu_term_contains_rec(Term t, Term needle, u32 depth) {
  if (depth > 256) return 0;
  if (t == needle) return 1;
  if (term_tag(t) != TAG_UOP) return 0;
  u32 op = term_ext(t);
  u64 loc = term_val(t);
  switch (op) {
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL: case UOP_IDIV:
    case UOP_IMOD: case UOP_ILT:  case UOP_IAND: case UOP_IOR: case UOP_IXOR:
    case UOP_ISHR:
    case UOP_ADD:  case UOP_MUL:  case UOP_CMPLT: case UOP_CMPEQ:
    case UOP_INDEX_E:
      return rmu_term_contains_rec(heap_read(loc + 0), needle, depth + 1)
          || rmu_term_contains_rec(heap_read(loc + 1), needle, depth + 1);
    case UOP_NEG:   case UOP_RECIP: case UOP_EXP2:
    case UOP_LOG2:  case UOP_SQRT:
    case UOP_CAST:  case UOP_BITCAST:
    case UOP_OPT:
      return rmu_term_contains_rec(heap_read(loc + 0), needle, depth + 1);
    case UOP_IWHERE:
      return rmu_term_contains_rec(heap_read(loc + 0), needle, depth + 1)
          || rmu_term_contains_rec(heap_read(loc + 1), needle, depth + 1)
          || rmu_term_contains_rec(heap_read(loc + 2), needle, depth + 1);
    case UOP_REDUCE:
      // Recurse into the body: nesting can be transitive.
      return rmu_term_contains_rec(heap_read(loc + 0), needle, depth + 1);
    default:
      return 0;
  }
}
static int rmu_term_contains(Term t, Term needle) {
  return rmu_term_contains_rec(t, needle, 0);
}

// Returns 1 if `t`'s subtree references a UOP_RANGE with axis id
// `axis_id` as a FREE variable -- i.e. not bound by a nested UOP_REDUCE
// inside `t`.  The walk stops at every UOP_REDUCE boundary: a nested
// reduce binds its own reduce-axis (and uses its body's other axes
// privately), so its `_accN` result is an opaque leaf in `t`'s scope.
// Used to decide reduce-loop nesting: an inner reduce whose body
// references an OUTER reduce's reduce-axis var must be emitted INSIDE
// the outer reduce's loop, not hoisted above it (a hoist leaves the
// outer axis var undeclared at the inner reduce's emission point --
// nvrtc `identifier "aN" is undefined`, the bug-1 shape).  Descending
// THROUGH nested reduces would wrongly report a sibling reduce's
// private axis as "used" -- e.g. softmax's sum-of-exp body references
// the max reduce's `_accN`, but the max's reduce-axis is bound, not
// free, so the two reduces are siblings, not nested.  This is the thvm
// analogue of tinygrad's per-UOp `.ranges` set
// (tinygrad/uop/ops.py:352-370), where a REDUCE's `ended_ranges` are
// popped out of its result's range set: the reduce-axis stops flowing
// at the reduce, exactly this boundary.
static int rmu_term_uses_axis_rec(Term t, u32 axis_id, u32 depth) {
  if (depth > 256) return 0;
  if (term_tag(t) != TAG_UOP) return 0;
  u32 op = term_ext(t);
  u64 loc = term_val(t);
  if (op == UOP_RANGE) {
    return (u32)term_val(heap_read(loc + 0)) == axis_id;
  }
  if (op == UOP_REDUCE) {
    // A nested reduce binds its OWN axis (closing free references to
    // that axis_id below).  Other axes used inside its body remain FREE
    // in the enclosing scope: a nested reduce is NOT an opaque-value
    // leaf for those.  Descend, but treat the inner reduce's own axis
    // as bound.
    //
    // Soft-max correctness (sum-of-exp(x - max_acc)): when querying
    // axis_id == max.axis, this REDUCE's own_axis matches and we
    // return 0 -- sum_reduce's "uses" of max_reduce.axis are bound
    // privately inside max, sum stays sibling (mirrors the 63d390f9
    // bugfix, preserved by the own-axis bound check below).
    //
    // Chain-of-reduces (sum(axis=(a,b,c)) -> REDUCE_a(REDUCE_b(
    //   REDUCE_c(body)))): each inner reduce's body uses every outer
    // axis.  Without descending past the inner UOP_REDUCE shell, the
    // outer's body (which IS the inner reduce) looks like it doesn't
    // use the outer's axis, parent_idx misfires, and the inner reduce
    // hoists above the outer's loop -- rendering inner ref to outer's
    // axis var before declaration (the bug that orphaned the
    // a6/_acc6 loop in the multi-axis conv reduce).
    // Multi-axis: any of the REDUCE's own axes shadows axis_id (it's
    // bound, not free).  Mirrors tinygrad's per-REDUCE ranges-set
    // boundary (uop/ops.py:352-370).
    Term tred = term_new(0, TAG_UOP, op, loc);
    u32 n_axes = uop_reduce_n_axes(tred);
    for (u32 i = 0; i < n_axes; i++) {
      if (uop_reduce_axis(tred, i) == axis_id) return 0;
    }
    return rmu_term_uses_axis_rec(uop_reduce_src(tred), axis_id, depth + 1);
  }
  switch (op) {
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL: case UOP_IDIV:
    case UOP_IMOD: case UOP_ILT:  case UOP_IAND: case UOP_IOR: case UOP_IXOR:
    case UOP_ISHR:
    case UOP_ADD:  case UOP_MUL:  case UOP_CMPLT: case UOP_CMPEQ:
    case UOP_INDEX_E:
      return rmu_term_uses_axis_rec(heap_read(loc + 0), axis_id, depth + 1)
          || rmu_term_uses_axis_rec(heap_read(loc + 1), axis_id, depth + 1);
    case UOP_NEG:   case UOP_RECIP: case UOP_EXP2:
    case UOP_LOG2:  case UOP_SQRT:
    case UOP_CAST:  case UOP_BITCAST:
    case UOP_OPT:
      return rmu_term_uses_axis_rec(heap_read(loc + 0), axis_id, depth + 1);
    case UOP_IWHERE:
      return rmu_term_uses_axis_rec(heap_read(loc + 0), axis_id, depth + 1)
          || rmu_term_uses_axis_rec(heap_read(loc + 1), axis_id, depth + 1)
          || rmu_term_uses_axis_rec(heap_read(loc + 2), axis_id, depth + 1);
    default:
      return 0;
  }
}
static int rmu_term_uses_axis(Term t, u32 axis_id) {
  return rmu_term_uses_axis_rec(t, axis_id, 0);
}

// Returns 1 if `t`'s subtree contains a UOP_REDUCE node anywhere.
// Used by rmu_emit_store_reduce to decline the single-reduce
// specialisation when the reduce body itself contains a nested
// reduce -- those need the generic rmu_emit_store path, which
// post-order-collects every reduce and hoists each accumulator's
// declaration ahead of its consumers (rmu_emit_store_reduce only
// emits the outer accumulator, leaving the inner `_accN`
// undeclared -> MSL "use of undeclared identifier" -> per-op
// fallback for that kernel).
static int rmu_term_has_reduce(Term t, u32 depth) {
  if (depth > 256) return 0;
  if (term_tag(t) != TAG_UOP) return 0;
  u32 op = term_ext(t);
  u64 loc = term_val(t);
  if (op == UOP_REDUCE) return 1;
  switch (op) {
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL: case UOP_IDIV:
    case UOP_IMOD: case UOP_ILT:  case UOP_IAND: case UOP_IOR: case UOP_IXOR:
    case UOP_ISHR:
    case UOP_ADD:  case UOP_MUL:  case UOP_CMPLT: case UOP_CMPEQ:
    case UOP_INDEX_E:
      return rmu_term_has_reduce(heap_read(loc + 0), depth + 1)
          || rmu_term_has_reduce(heap_read(loc + 1), depth + 1);
    case UOP_NEG:   case UOP_RECIP: case UOP_EXP2:
    case UOP_LOG2:  case UOP_SQRT:
    case UOP_CAST:  case UOP_BITCAST:
    case UOP_OPT:
      return rmu_term_has_reduce(heap_read(loc + 0), depth + 1);
    case UOP_IWHERE:
      return rmu_term_has_reduce(heap_read(loc + 0), depth + 1)
          || rmu_term_has_reduce(heap_read(loc + 1), depth + 1)
          || rmu_term_has_reduce(heap_read(loc + 2), depth + 1);
    default:
      return 0;
  }
}

// Walk a term tree collecting UOP_REDUCE nodes for hoisting.  Each
// REDUCE produces a separate accumulator.  Up to MAX_DIM reduces.
static void rmu_collect_reduces(Term t, Term *reduces, u32 *n_out) {
  if (term_tag(t) != TAG_UOP) return;
  if (*n_out >= MAX_DIM) return;
  u32 op = term_ext(t);
  u64 loc = term_val(t);
  if (op == UOP_REDUCE) {
    // Dedup by REDUCE term identity.
    for (u32 i = 0; i < *n_out; i++) {
      if (reduces[i] == t) return;
    }
    // Post-order add: recurse into the body FIRST so any nested
    // REDUCE (e.g. body-rewrap form `IWHERE(_, INNER_REDUCE, INVALID)`
    // from kernel_lift's reduce-over-broadcast-axis fix) is collected
    // and emitted before this outer reduce that depends on its
    // `_acc<N>` placeholder.  Otherwise the renderer emits `_acc4`
    // for the inner reduce term while only `_acc5` (the outer) is
    // declared, yielding `undeclared identifier '_acc4'`.
    rmu_collect_reduces(heap_read(loc + 0), reduces, n_out);
    if (*n_out >= MAX_DIM) return;
    // Re-check dedup after recursion (in case the body referenced
    // this same outer term -- shouldn't happen but cheap to guard).
    for (u32 i = 0; i < *n_out; i++) {
      if (reduces[i] == t) return;
    }
    reduces[*n_out] = t;
    (*n_out)++;
    return;
  }
  switch (op) {
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL: case UOP_IDIV:
    case UOP_IMOD: case UOP_ILT:  case UOP_IAND: case UOP_IOR: case UOP_IXOR:
    case UOP_ISHR:
    case UOP_ADD:  case UOP_MUL:  case UOP_CMPLT: case UOP_CMPEQ:
    case UOP_INDEX_E:
      rmu_collect_reduces(heap_read(loc + 0), reduces, n_out);
      rmu_collect_reduces(heap_read(loc + 1), reduces, n_out);
      return;
    case UOP_NEG:   case UOP_RECIP: case UOP_EXP2:
    case UOP_LOG2:  case UOP_SQRT:
    case UOP_CAST:  case UOP_BITCAST:
    case UOP_OPT:
      rmu_collect_reduces(heap_read(loc + 0), reduces, n_out);
      return;
    case UOP_IWHERE:
      rmu_collect_reduces(heap_read(loc + 0), reduces, n_out);
      rmu_collect_reduces(heap_read(loc + 1), reduces, n_out);
      rmu_collect_reduces(heap_read(loc + 2), reduces, n_out);
      return;
    default:
      return;
  }
}

static void rmu_collect_ranges(Term t, Term *ranges, u32 *n_out) {
  u32 dummy_kinds[MAX_DIM]   = {0};
  u32 dummy_factors[MAX_DIM] = {0};
  rmu_collect_ranges_rec(t, ranges, dummy_kinds, dummy_factors, n_out,
                         0, 0);
}

static void rmu_collect_ranges_with_opts(Term t, Term *ranges,
                                         u32 *opt_kinds,
                                         u32 *opt_factors,
                                         u32 *n_out) {
  rmu_collect_ranges_rec(t, ranges, opt_kinds, opt_factors, n_out,
                         RMU_NO_OPT, 0);
}

// Like rmu_collect_ranges_rec but DESCENDS into UOP_REDUCE bodies.
// The REDUCE-blind primary collector exists because the outer store's
// ranges[] is for OUTPUT axes (axes that index the store position) --
// a reduce-axis used only inside a reduce body must not surface as an
// outer for-loop.  But for the per-reduce `required_pos` scan and the
// reduce-axis range-term lookup inside RMU_EMIT_ONE_REDUCE we need to
// see ranges transitively reachable through nested reduces: a reduce
// whose body contains another reduce whose body references some axis
// still depends on that axis structurally.  Stopping at the first
// UOP_REDUCE boundary (as the primary collector does) drops those
// axes and yields kernels with declared `_accN` accumulators whose
// for-loops were never emitted (the reduce-axis range was unreachable)
// or `required_pos` undercounts that flat-hoist a reduce above the
// output loops it actually needs.
// Per-recursion bound-axis tracker.  When descending into a UOP_REDUCE
// body, the reduce-axis is bound by the reduce loop and shouldn't
// surface as a free range in the caller's collection (it would consume
// a slot in the MAX_DIM=8 ranges[] cap and also conflict with
// RMU_EMIT_ONE_REDUCE's loop emit).  The bound-axis list is small
// (capped at 2*MAX_DIM since reduce-axes nest at most that deep) and
// scanned linearly per UOP_RANGE leaf.
#define RMU_BOUND_AXIS_CAP (2 * MAX_DIM)
static int rmu_axis_is_bound(u32 const *bound, u32 n_bound, u32 aid) {
  for (u32 i = 0; i < n_bound; i++) if (bound[i] == aid) return 1;
  return 0;
}

static void rmu_collect_ranges_rec_through_reduce_cap(
    Term t, Term *ranges, u32 *opt_kinds, u32 *opt_factors,
    u32 *n_out, u32 cap,
    u32 inherit_kind, u32 inherit_factor,
    u32 *bound_axes, u32 *n_bound) {
  if (term_tag(t) != TAG_UOP) return;
  if (*n_out >= cap) return;
  u32 op  = term_ext(t);
  u64 loc = term_val(t);
  if (op == UOP_REDUCE) {
    // Push EVERY reduce-axis id onto the bound set, descend into body,
    // then pop.  Multi-axis port: the REDUCE node binds all axes
    // simultaneously (mirrors tinygrad's per-REDUCE ranges-set boundary
    // in uop/ops.py:352-370).
    Term tred = term_new(0, TAG_UOP, op, loc);
    u32 n_axes = uop_reduce_n_axes(tred);
    u32 pushed = 0;
    for (u32 i = 0; i < n_axes; i++) {
      u32 r_aid = uop_reduce_axis(tred, i);
      if (!rmu_axis_is_bound(bound_axes, *n_bound, r_aid)
          && *n_bound < RMU_BOUND_AXIS_CAP) {
        bound_axes[*n_bound] = r_aid;
        (*n_bound)++;
        pushed++;
      }
    }
    rmu_collect_ranges_rec_through_reduce_cap(
        uop_reduce_src(tred), ranges, opt_kinds, opt_factors, n_out, cap,
        RMU_NO_OPT, 0, bound_axes, n_bound);
    *n_bound -= pushed;
    return;
  }
  if (op == UOP_RANGE) {
    u32 axis_id = term_val(heap_read(loc + 0));
    if (rmu_axis_is_bound(bound_axes, *n_bound, axis_id)) return;
    for (u32 i = 0; i < *n_out; i++) {
      u32 existing = term_val(heap_read(term_val(ranges[i]) + 0));
      if (existing == axis_id) return;
    }
    ranges     [*n_out] = t;
    opt_kinds  [*n_out] = inherit_kind;
    opt_factors[*n_out] = inherit_factor;
    (*n_out)++;
    return;
  }
  switch (op) {
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL: case UOP_IDIV:
    case UOP_IMOD: case UOP_ILT:  case UOP_IAND: case UOP_IOR: case UOP_IXOR:
    case UOP_ISHR:
    case UOP_ADD:  case UOP_MUL:  case UOP_CMPLT: case UOP_CMPEQ:
    case UOP_INDEX_E:
      rmu_collect_ranges_rec_through_reduce_cap(
          heap_read(loc + 0), ranges, opt_kinds, opt_factors, n_out, cap,
          RMU_NO_OPT, 0, bound_axes, n_bound);
      rmu_collect_ranges_rec_through_reduce_cap(
          heap_read(loc + 1), ranges, opt_kinds, opt_factors, n_out, cap,
          RMU_NO_OPT, 0, bound_axes, n_bound);
      return;
    case UOP_NEG:   case UOP_RECIP: case UOP_EXP2:
    case UOP_LOG2:  case UOP_SQRT:
      rmu_collect_ranges_rec_through_reduce_cap(
          heap_read(loc + 0), ranges, opt_kinds, opt_factors, n_out, cap,
          RMU_NO_OPT, 0, bound_axes, n_bound);
      return;
    case UOP_IWHERE:
      rmu_collect_ranges_rec_through_reduce_cap(
          heap_read(loc + 0), ranges, opt_kinds, opt_factors, n_out, cap,
          RMU_NO_OPT, 0, bound_axes, n_bound);
      rmu_collect_ranges_rec_through_reduce_cap(
          heap_read(loc + 1), ranges, opt_kinds, opt_factors, n_out, cap,
          RMU_NO_OPT, 0, bound_axes, n_bound);
      rmu_collect_ranges_rec_through_reduce_cap(
          heap_read(loc + 2), ranges, opt_kinds, opt_factors, n_out, cap,
          RMU_NO_OPT, 0, bound_axes, n_bound);
      return;
    case UOP_OPT: {
      u32 kind   = term_val(heap_read(loc + 1));
      u32 factor = term_val(heap_read(loc + 2));
      rmu_collect_ranges_rec_through_reduce_cap(
          heap_read(loc + 0), ranges, opt_kinds, opt_factors, n_out, cap,
          kind, factor, bound_axes, n_bound);
      return;
    }
    case UOP_CAST: case UOP_BITCAST:
      rmu_collect_ranges_rec_through_reduce_cap(
          heap_read(loc + 0), ranges, opt_kinds, opt_factors, n_out, cap,
          RMU_NO_OPT, 0, bound_axes, n_bound);
      return;
    default:
      return;
  }
}

// Default-cap wrappers (MAX_DIM) for callers that don't need more.
static void rmu_collect_ranges_through_reduce(
    Term t, Term *ranges, u32 *n_out) {
  u32 dummy_kinds[MAX_DIM]   = {0};
  u32 dummy_factors[MAX_DIM] = {0};
  u32 bound[RMU_BOUND_AXIS_CAP];
  u32 n_bound = 0;
  rmu_collect_ranges_rec_through_reduce_cap(
      t, ranges, dummy_kinds, dummy_factors, n_out, MAX_DIM,
      0, 0, bound, &n_bound);
}

static void rmu_collect_ranges_with_opts_through_reduce(
    Term t, Term *ranges, u32 *opt_kinds, u32 *opt_factors, u32 *n_out) {
  u32 bound[RMU_BOUND_AXIS_CAP];
  u32 n_bound = 0;
  rmu_collect_ranges_rec_through_reduce_cap(
      t, ranges, opt_kinds, opt_factors, n_out, MAX_DIM,
      RMU_NO_OPT, 0, bound, &n_bound);
}

// RMU_MAX_RANGES-cap range-only variant for the rmu_emit_store
// dependency analysis (required_pos + reduce-feeding-broadcast hoist).
// Those scans must see EVERY free range a reduce body references to
// place the reduce after all output loops it depends on; the MAX_DIM=8
// default truncates fused conv-backward reduce bodies (>8 free ranges)
// so a late output axis (e.g. an unfold window axis) is dropped from
// the dependency set, required_pos undercounts, and the reduce hoists
// above that axis's output loop -> use-before-declaration in the
// emitted kernel.  Placement-only: kernels whose reduce bodies fit in
// MAX_DIM ranges collect identically, so default-seed codegen is
// unchanged.
static void rmu_collect_ranges_through_reduce_kernel(
    Term t, Term *ranges, u32 *n_out) {
  u32 dummy_kinds[RMU_MAX_RANGES]   = {0};
  u32 dummy_factors[RMU_MAX_RANGES] = {0};
  u32 bound[RMU_BOUND_AXIS_CAP];
  u32 n_bound = 0;
  rmu_collect_ranges_rec_through_reduce_cap(
      t, ranges, dummy_kinds, dummy_factors, n_out, RMU_MAX_RANGES,
      0, 0, bound, &n_bound);
}

// RMU_MAX_RANGES-cap variants used by rmu_emit_store so kernel iter
// scopes spanning >MAX_DIM axes (output + aux LOOP + reduce) don't
// drop late LOOP axes to the cap.
static void rmu_collect_ranges_with_opts_through_reduce_kernel(
    Term t, Term *ranges, u32 *opt_kinds, u32 *opt_factors, u32 *n_out) {
  u32 bound[RMU_BOUND_AXIS_CAP];
  u32 n_bound = 0;
  rmu_collect_ranges_rec_through_reduce_cap(
      t, ranges, opt_kinds, opt_factors, n_out, RMU_MAX_RANGES,
      RMU_NO_OPT, 0, bound, &n_bound);
}

static void rmu_collect_ranges_with_opts_kernel(
    Term t, Term *ranges, u32 *opt_kinds, u32 *opt_factors, u32 *n_out) {
  rmu_collect_ranges_rec_cap(t, ranges, opt_kinds, opt_factors, n_out,
                             RMU_MAX_RANGES, RMU_NO_OPT, 0);
}

// Returns 1 if `kind`/`axis_type` indicates the axis binds directly to
// a thread/group position (no for-loop emitted).  These paths emit a
// `uint aN = tt;` or `uint aN = tg;` declaration instead.
static int rmu_axis_is_threadbound(u32 opt_kind, u32 axis_type) {
  if (RMU_TARGET == CG_TARGET_C) return 0;  // C99 has no thread positions
  return (opt_kind == UOP_OPT_LOCAL)
      || axis_type == 4  /* legacy KAX_LOCAL  */
      || axis_type == 5  /* legacy KAX_GLOBAL */;
}

// Per-call promoted-GLOBAL context.  Output axes (axis_id in the
// store's addr expression) that arrive as plain KAX_LOOP with no OPT
// wrap get promoted to parallel grid axes: instead of a serial
// `for`-loop, each thread decodes its axis tuple from the flat 1-D
// dispatch index `tid`.  rmu_emit_store builds this once pre-loop and
// threads it through to rmu_emit_range_open_ctx.
//
//   rmu_gd_g_mod(gd, axis_id)    = this axis's extent (0 if not promoted)
//   rmu_gd_g_stride(gd, axis_id) = product of inner promoted-GLOBAL
//                                  extents (1 for the innermost)
//   n_globals                    = count of promoted axes
//   total                        = product of all promoted extents
//                              (== output_numel; the dispatcher
//                              launches >= this many threads and the
//                              kernel guards `tid >= total`)
//
// Note: explicitly-pre-stamped axis_type==KAX_GLOBAL ranges (the TC
// matmul / conv2d_flat templates bind those themselves, and the one
// `render-uop/legacy-kax-global-emits-tg` test) are NOT entered here
// -- those still emit `uint aN = tg;` via rmu_emit_range_open_ctx's
// axis_type==5 branch.
//
// tg/tt split: when the kernel ALSO carries one or more LOCAL axes (a
// KOP_LOCAL split: OPT(_, LOCAL, f) wrapping the INNER half, the OUTER
// half a plain promoted-GLOBAL axis), the promoted-GLOBAL axes must
// decode from `tg` (threadgroup_position_in_grid), NOT `tid`
// (thread_position_in_grid) -- otherwise the GLOBAL decode would
// include the `tt` bits (`tid = tg*threads + tt`) and repeat values
// within each threadgroup.  tinygrad's `has_local && has_global`
// convention: GLOBAL extents -> grid (`groups`, indexed by tg), LOCAL
// extents -> threadgroup (`threads`, indexed by tt).  `has_local` is
// set by rmu_compute_global_decode_ctx when it sees a LOCAL-OPT'd (or
// legacy axis_type==4) range; with no LOCAL axis it's 0 and the
// existing `tid` decode (equivalent to tg*threads+tt with threads
// derived from the dispatch shape) is kept unchanged.
//
// Multi-LOCAL: with >=2 LOCAL axes the threadgroup index `tt` decodes
// each one with its own (stride, modulus): `uint lK = (tt / ltK) % leK`
// where ltK is the product of LOCAL extents inner to lK (1 for the
// innermost).  Mirrors the multi-GLOBAL `tg` decode.  The single-LOCAL
// fast path (`uint l0 = tt;`) is just the n==1 case (stride 1).  The
// threadgroup size is the product of all LOCAL extents; the heuristic
// keeps it <= 256 (Apple's maxTotalThreadsPerThreadgroup is 1024).
// Dense (axis_id, stride, modulus) tables.  Indexing by raw axis_id with
// a fixed [256] array silently dropped any axis whose id >= 256 -- and
// thvm axis_ids are GLOBAL (accumulate across the whole DAG), so deep
// backward graphs reach ids in the hundreds.  A dropped axis fell through
// to the serial-loop / raw-`tt` fallback in rmu_emit_range_open_ctx, so
// the full beautiful_mnist LeNet's conv kernels rendered single-threaded
// and nvrtc choked.  A kernel has <= RMU_MAX_RANGES axes, so a dense list
// + linear scan is both correct (no id ceiling) and cheap.
typedef struct {
  u32 g_aid   [RMU_MAX_RANGES]; // promoted-GLOBAL axis_id
  u32 g_stride[RMU_MAX_RANGES]; // stride into the `tg`/`tid` flat decode
  u32 g_mod   [RMU_MAX_RANGES]; // extent
  u32 n_globals;
  u64 total;                  // product of promoted-GLOBAL extents
  int has_local;              // 1 -> decode GLOBAL axes from `tg`, not `tid`
  // LOCAL-axis decode (mirrors the GLOBAL tables but over `tt`).
  u32 l_aid   [RMU_MAX_RANGES];
  u32 l_stride[RMU_MAX_RANGES];
  u32 l_mod   [RMU_MAX_RANGES];
  u32 n_locals;
  u64 local_total;            // product of LOCAL extents (threadgroup size)
} RmuGlobalDecode;

// Lookups by axis_id (linear scan; n is tiny).  Return 0 modulus = "this
// axis_id is not a promoted GLOBAL / LOCAL axis".
static u32 rmu_gd_g_mod(RmuGlobalDecode const *g, u32 aid) {
  for (u32 i = 0; i < g->n_globals; i++) if (g->g_aid[i] == aid) return g->g_mod[i];
  return 0;
}
static u32 rmu_gd_g_stride(RmuGlobalDecode const *g, u32 aid) {
  for (u32 i = 0; i < g->n_globals; i++) if (g->g_aid[i] == aid) return g->g_stride[i];
  return 0;
}
static u32 rmu_gd_l_mod(RmuGlobalDecode const *g, u32 aid) {
  for (u32 i = 0; i < g->n_locals; i++) if (g->l_aid[i] == aid) return g->l_mod[i];
  return 0;
}
static u32 rmu_gd_l_stride(RmuGlobalDecode const *g, u32 aid) {
  for (u32 i = 0; i < g->n_locals; i++) if (g->l_aid[i] == aid) return g->l_stride[i];
  return 0;
}

// Emit a loop opener (or thread-position bind) for a UOP_RANGE leaf.
// `opt_kind` is the OPT annotation (RMU_NO_OPT if none).
// `gctx` is the promoted-GLOBAL decode context (NULL for legacy callers).
// Patterns:
//   LOCAL                       -> `uint aN = tt;`
//   promoted GLOBAL, n==1       -> `uint aN = tid;`
//   promoted GLOBAL, n>=2       -> `uint aN = (tid/stride) % mod;`  (`tid % mod` for innermost)
//   axis_type==5 (legacy)       -> `uint aN = tg;`
//   axis_type==1 (REDUCE)       -> `for (...) /*reduce*/ {`
//   else                        -> `for (...) {`
//
// kvar wedge: when the range's raw extent has bit 31 (KVAR_FLAG) set,
// the low 31 bits hold a kvar id instead of a literal extent.  In
// that case the for-loop bound is emitted as `V_<name>` so a single
// MSL string covers all runtime values for that variable; the kernel
// signature emit in cg_render_uop_kernel_root adds the matching
// `constant uint &V_<name>` arg and the Metal dispatcher binds the
// per-fire runtime values via setBytes:.  Promoted-GLOBAL / LOCAL /
// legacy-GLOBAL paths use the worst-case kvar_hi(id) for the `ext=%u`
// comment; the parent thread owns wiring symbolic extents through
// those paths (the symbolic demo test only exercises the LOOP bound).
static void rmu_emit_range_open_ctx(Term r, FILE *fp, u32 depth,
                                    u32 opt_kind,
                                    RmuGlobalDecode const *gctx) {
  if (term_tag(r) != TAG_UOP || term_ext(r) != UOP_RANGE) return;
  u64 loc = term_val(r);
  u32 axis_id    = term_val(heap_read(loc + 0));
  u32 axis_type  = term_val(heap_read(loc + 1));
  u32 raw_extent = term_val(heap_read(loc + 2));
  int is_var = kvar_extent_is_var(raw_extent);
  u32 var_id = is_var ? kvar_extent_var_id(raw_extent) : 0;
  // `extent` is the integer to bake into the literal /* ext=%u */
  // comment (and as the for-loop bound for non-symbolic ranges).
  // For symbolic ranges the actual loop bound is `bound[]` below.
  u32 extent = is_var ? kvar_hi(var_id) : raw_extent;
  char bound[32];
  if (is_var) {
    const char *vn = kvar_name(var_id);
    if (vn == NULL) vn = "V";
    snprintf(bound, sizeof(bound), "V_%s", vn);
  } else {
    snprintf(bound, sizeof(bound), "%u", raw_extent);
  }
  for (u32 i = 0; i < depth; i++) fputs("  ", fp);
  // LOCAL via OPT annotation OR via axis_type == 4 (legacy KAX_LOCAL).
  // With a single LOCAL axis: `uint aN = tt;`.  With >=2, decode each
  // from `tt` with its own (stride, modulus) -- mirrors multi-GLOBAL.
  if (opt_kind == UOP_OPT_LOCAL || axis_type == 4) {
    // Decode (tt / stride) % mod when >=2 LOCAL axes OR a coexisting
    // GROUP_REDUCE shifted this single axis up a group_extent stride
    // (lstride > 1) -- the plain `aN = tt` fast path is only the lone-axis,
    // stride-1 case.
    u32 lstride = (gctx != NULL) ? rmu_gd_l_stride(gctx, axis_id) : 0;
    u32 lmod    = (gctx != NULL) ? rmu_gd_l_mod   (gctx, axis_id) : 0;
    if (lmod != 0 && (gctx->n_locals >= 2 || lstride > 1)) {
      if (lstride <= 1) {
        fprintf(fp, "uint a%u = tt %% %uu; /* local ext=%u */\n",
                axis_id, lmod, extent);
      } else {
        fprintf(fp, "uint a%u = (tt / %uu) %% %uu; /* local ext=%u */\n",
                axis_id, lstride, lmod, extent);
      }
      return;
    }
    fprintf(fp, "uint a%u = tt; /* local ext=%u */\n", axis_id, extent);
    return;
  }
  // Promoted output axis -> parallel grid axis.  Decoded from `tid`
  // (thread_position_in_grid) normally; from `tg`
  // (threadgroup_position_in_grid) when the kernel also has a LOCAL
  // axis (one threadgroup per GLOBAL tuple, LOCAL axis over `tt`).
  if (gctx != NULL && rmu_gd_g_mod(gctx, axis_id) != 0) {
    u32 stride = rmu_gd_g_stride(gctx, axis_id);
    u32 mod    = rmu_gd_g_mod(gctx, axis_id);
    char const *idx = (gctx->has_local || RMU_HAS_GROUP_REDUCE) ? "tg" : "tid";
    if (gctx->n_globals == 1) {
      fprintf(fp, "uint a%u = %s; /* global ext=%u */\n", axis_id, idx, extent);
    } else if (stride <= 1) {
      fprintf(fp, "uint a%u = %s %% %uu; /* global ext=%u */\n",
              axis_id, idx, mod, extent);
    } else {
      fprintf(fp, "uint a%u = (%s / %uu) %% %uu; /* global ext=%u */\n",
              axis_id, idx, stride, mod, extent);
    }
    return;
  }
  if (axis_type == 5 /* legacy KAX_GLOBAL (TC/conv templates, test) */) {
    fprintf(fp, "uint a%u = tg; /* global ext=%u */\n", axis_id, extent);
    return;
  }
  if (axis_type == 1 /*REDUCE*/) {
    fprintf(fp, "for (uint a%u = 0; a%u < %s; a%u++) /*reduce*/ {\n",
            axis_id, axis_id, bound, axis_id);
  } else {
    // Small LOOP-typed output axes (non-reduce, non-promoted): emit
    // `#pragma unroll` so the per-thread inner loop body unrolls in
    // the SASS rather than running with branch overhead per iter.
    // Cap at THVM_LOOP_UNROLL_MAX (default 4) to avoid blowing up the
    // body for huge axes; tinygrad's renderer unrolls similar small
    // sequential loops via the UNROLL OPT.  Compile-time penalty is
    // bounded by the cap.  Skip symbolic-bound and large-extent.
    if (RMU_TARGET != CG_TARGET_C && !is_var && extent > 1) {
      // Default 1 (effectively OFF): a 10% regression at BS=64
      // surfaced when defaulting to 4 (warm 154ms -> 138ms with =1).
      // Cross-BS validation pending; opt in per workload until a clean-
      // window sweep proves a default that's positive at every BS.
      static int loop_unroll_max = -1;
      if (loop_unroll_max < 0) {
        char const *e = getenv("THVM_LOOP_UNROLL_MAX");
        loop_unroll_max = (e != NULL && e[0] != '\0') ? atoi(e) : 1;
        if (loop_unroll_max < 0) loop_unroll_max = 0;
      }
      if ((int)extent <= loop_unroll_max) {
        fprintf(fp, "#pragma unroll(%u)\n", extent);
        for (u32 i = 0; i < depth; i++) fputs("  ", fp);
      }
    }
    fprintf(fp, "for (uint a%u = 0; a%u < %s; a%u++) {\n",
            axis_id, axis_id, bound, axis_id);
  }
}

// Legacy entry: dispatches to ctx-aware version with NULL ctx.
static void rmu_emit_range_open(Term r, FILE *fp, u32 depth,
                                u32 opt_kind) {
  rmu_emit_range_open_ctx(r, fp, depth, opt_kind, NULL);
}

// Returns 1 if range index `i` is a LOCAL axis (LOCAL-OPT'd, or legacy
// axis_type==4).  `opt_kinds[]` may be NULL.
static int rmu_range_is_local(Term const *ranges, u32 const *opt_kinds, u32 i) {
  Term r = ranges[i];
  if (term_tag(r) != TAG_UOP || term_ext(r) != UOP_RANGE) return 0;
  if (opt_kinds != NULL && opt_kinds[i] == UOP_OPT_LOCAL) return 1;
  u32 axis_type = (u32)term_val(heap_read(term_val(r) + 1));
  return axis_type == 4 /* legacy KAX_LOCAL */;
}

// Build the promoted-GLOBAL decode context.  `promote[]` is parallel
// to `ranges[]`: 1 means "this output axis was a plain KAX_LOOP with
// no OPT and should become a parallel grid axis".  Walks right-to-left
// so the innermost promoted axis gets stride 1 and each one to the
// left multiplies its inner's extent.  `opt_kinds[]` (parallel to
// `ranges[]`, may be NULL) is scanned for LOCAL-OPT'd ranges; when
// present, out->has_local is set so the GLOBAL decode uses `tg`
// instead of `tid` (one threadgroup per GLOBAL tuple, LOCAL axes
// over `tt`).  Legacy axis_type==4 (KAX_LOCAL) ranges also count.  The
// LOCAL axes get their own (stride, modulus) decode over `tt` -- the
// same right-to-left scan, in `ranges[]` order, so axis ids match the
// emission order; a single LOCAL axis ends up stride 1 (`uint aN = tt`).
static void rmu_compute_global_decode_ctx(Term const *ranges, u32 n_ranges,
                                          u8 const *promote,
                                          u32 const *opt_kinds,
                                          RmuGlobalDecode *out) {
  memset(out, 0, sizeof(*out));
  u32 stride = 1;
  u32 n_glb  = 0;
  u64 total  = 1;
  for (i32 i = (i32)n_ranges - 1; i >= 0; i--) {
    Term r = ranges[i];
    if (term_tag(r) != TAG_UOP || term_ext(r) != UOP_RANGE) continue;
    u64 loc = term_val(r);
    // Include both promote[]-marked (KAX_LOOP -> parallel grid) axes AND
    // axes that arrived pre-stamped axis_type==5 (legacy KAX_GLOBAL: the
    // matmul/TC templates).  Without the latter, a scalar-fallback matmul
    // with >=2 KAX_GLOBAL output axes (every fp32 matmul -- WMMA needs
    // fp16) had each emit `uint aN = tg;` via the legacy path below, so M
    // and N both collapsed to one grid index and only out[0] was written.
    // Treating them as globals here routes them through the multi-global
    // tid/stride%mod decode, which matches the flat grid (tid 0..numel-1).
    u32 axis_type_i = (u32)term_val(heap_read(loc + 1));
    if (!promote[i] && axis_type_i != 5) continue;
    u32 axis_id = (u32)term_val(heap_read(loc + 0));
    u32 extent  = (u32)term_val(heap_read(loc + 2));
    if (extent == 0 || n_glb >= RMU_MAX_RANGES) continue;
    out->g_aid   [n_glb] = axis_id;
    out->g_stride[n_glb] = stride;
    out->g_mod   [n_glb] = extent;
    n_glb++;
    stride *= extent;
    total  *= extent;
  }
  out->n_globals = n_glb;
  out->total     = (n_glb > 0) ? total : 0;
  // GPU-generic: LOCAL axes decode from the threadgroup/block-local
  // index (`tt`).  Both Metal threadgroups and CUDA thread blocks have
  // this notion; only the C target (serial loops) has no LOCAL decode.
  if (RMU_TARGET != CG_TARGET_C) {
    // LOCAL axes: right-to-left so the innermost (last in ranges[]) gets
    // stride 1.  `tt = sum_K lK * local_stride[K]` mirrors the GLOBAL
    // flat decode over `tg`.  When a GROUP_REDUCE axis coexists, it owns the
    // INNERMOST group_extent threads of `tt`, so the LOCAL axes start one
    // group_extent stride up (a1 = (tt / group_extent) % ext).
    u32 lstride = (RMU_GROUP_EXTENT > 0) ? (u32)RMU_GROUP_EXTENT : 1;
    u32 n_loc   = 0;
    u64 ltotal  = 1;
    for (i32 i = (i32)n_ranges - 1; i >= 0; i--) {
      if (!rmu_range_is_local(ranges, opt_kinds, (u32)i)) continue;
      Term r = ranges[i];
      u64 loc = term_val(r);
      u32 axis_id = (u32)term_val(heap_read(loc + 0));
      u32 extent  = (u32)term_val(heap_read(loc + 2));
      if (extent == 0 || n_loc >= RMU_MAX_RANGES) continue;
      out->l_aid   [n_loc] = axis_id;
      out->l_stride[n_loc] = lstride;
      out->l_mod   [n_loc] = extent;
      n_loc++;
      lstride *= extent;
      ltotal  *= extent;
    }
    out->n_locals     = n_loc;
    out->local_total  = (n_loc > 0) ? ltotal : 0;
    out->has_local    = (n_loc > 0);
  }
}

// Back-compat shim: no LOCAL axis (opt_kinds = NULL -> has_local = 0).
static void rmu_compute_global_decode(Term const *ranges, u32 n_ranges,
                                      u8 const *promote,
                                      RmuGlobalDecode *out) {
  rmu_compute_global_decode_ctx(ranges, n_ranges, promote, NULL, out);
}

// Shared prelude for the reduce-shaped emit paths (the generic
// accumulator path in rmu_emit_store_reduce).  Given the
// output-axis ranges (the reduce axis already split off) and the
// store position `addr`, this:
//   1. promotes every plain-LOOP output axis that actually indexes
//      the store position to a parallel grid axis (decoded from tid),
//   2. emits the `if (tid >= total) return;` bounds guard,
//   3. emits each output axis (thread-position bind for promoted /
//      LOCAL / explicit-GLOBAL, `for`-loop otherwise),
//   4. fills needs_close[] and returns the post-prelude body depth.
// Auxiliary loop axes (in red_src but NOT in addr -- rare) stay
// serial: promoting them would make threads race on the same output
// address.
static u32 rmu_emit_output_loops(Term addr, Term const *out_ranges,
                                 u32 const *out_kinds,
                                 u32 const *out_factors,
                                 u32 n_out, u32 depth, FILE *fp,
                                 int *needs_close) {
  Term addr_ranges[MAX_DIM];
  u32  addr_n = 0;
  rmu_collect_ranges(addr, addr_ranges, &addr_n);
  u32 addr_axes[MAX_DIM];
  for (u32 i = 0; i < addr_n; i++) {
    addr_axes[i] = (term_tag(addr_ranges[i]) == TAG_UOP
                    && term_ext(addr_ranges[i]) == UOP_RANGE)
                 ? (u32)term_val(heap_read(term_val(addr_ranges[i]) + 0))
                 : 0xFFFFFFFFu;
  }
  // C99 target has no thread positions: keep serial loops.  Otherwise:
  // promote every plain-KAX_LOOP output axis (no OPT wrapper) that
  // indexes the store position to a parallel grid axis decoded from
  // `tid`.  This composes with OPT'd axes: a UPCAST/LOCAL split leaves
  // the OUTER half as a plain KAX_LOOP (which we promote here) and the
  // INNER half as KAX_UPCAST/KAX_LOCAL with an OPT wrapper (which the
  // per-axis emit below handles -- `#pragma unroll` loop / `tt` bind).
  // So a UPCAST'd matmul still gets one-output-element-per-thread on
  // its M / N-outer axes while the N-inner axis is the unroll loop.
  // GPU-generic: parallel-grid promotion of plain output axes applies
  // to Metal AND CUDA -- both have a 1-D dispatch index (`tid`) from
  // which each thread decodes its axis tuple.  The C target keeps the
  // serial loop nest.
  u8 promote[MAX_DIM] = {0};
  if (RMU_TARGET != CG_TARGET_C) {
    for (u32 i = 0; i < n_out; i++) {
      Term r = out_ranges[i];
      if (term_tag(r) != TAG_UOP || term_ext(r) != UOP_RANGE) continue;
      if (out_kinds[i] != RMU_NO_OPT) continue;
      u32 axis_id   = (u32)term_val(heap_read(term_val(r) + 0));
      u32 axis_type = (u32)term_val(heap_read(term_val(r) + 1));
      if (axis_type != 0 /* KAX_LOOP */) continue;
      int is_output = 0;
      for (u32 j = 0; j < addr_n; j++) if (addr_axes[j] == axis_id) { is_output = 1; break; }
      if (is_output) promote[i] = 1;
    }
  }
  RmuGlobalDecode gd;
  rmu_compute_global_decode_ctx(out_ranges, n_out, promote, out_kinds, &gd);
  if (gd.n_globals > 0 && gd.total > 0) {
    for (u32 d = 0; d < depth; d++) fputs("  ", fp);
    fprintf(fp, "if (%s >= %lluu) return;\n",
            (gd.has_local || RMU_HAS_GROUP_REDUCE) ? "tg" : "tid",
            (unsigned long long)gd.total);
  }
  u32 body_depth = depth;
  for (u32 i = 0; i < n_out; i++) {
    Term r = out_ranges[i];
    u32 axis_id   = (term_tag(r) == TAG_UOP && term_ext(r) == UOP_RANGE)
                  ? (u32)term_val(heap_read(term_val(r) + 0)) : 0xFFFFFFFFu;
    u32 axis_type = (term_tag(r) == TAG_UOP && term_ext(r) == UOP_RANGE)
                  ? (u32)term_val(heap_read(term_val(r) + 1)) : 0;
    int promoted    = (rmu_gd_g_mod(&gd, axis_id) != 0);
    int threadbound = rmu_axis_is_threadbound(out_kinds[i], axis_type)
                   || promoted;
    if (out_kinds[i] != RMU_NO_OPT
        && (out_kinds[i] == UOP_OPT_UNROLL || out_kinds[i] == UOP_OPT_UPCAST)
        && !threadbound) {
      for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
      rmu_emit_unroll_pragma(fp, out_factors[i]);
    }
    rmu_emit_range_open_ctx(r, fp, body_depth, out_kinds[i], &gd);
    if (!threadbound) { needs_close[i] = 1; body_depth++; }
  }
  return body_depth;
}

// Recognise the canonical matmul shape:
//   STORE(C, addr_C, OPT(REDUCE(MUL(INDEX_E(A,_), INDEX_E(B,_)),
//                              SUM, k_axis), TC, _))
// The OPT wrapper marks the reduction as a tensor-core target.  The
// detection is structural; if the shape matches, returns 1 and fills
// `*out_red_value` with the inner REDUCE term so the caller can fall
// back to F1e's accumulator path while wrapping with TC markers.
// (F2b: emit a real simdgroup_matrix template instead of falling
// back.)

// Peel a single UOP_CAST/BITCAST wrapper off a MUL operand to reach the
// underlying INDEX_E.  A bf16-input matmul accumulating in f32 lifts each
// operand as CAST(INDEX_E(bf16_buf)) (the bf16->f32 widening feeding the
// f32 REDUCE).  The simdgroup template loads each operand as a
// simdgroup_matrix<bfloat> straight from the bf16 buffer (the MMA widens
// to the f32 accumulator), so the cast is redundant at TC-emit time --
// peel it to recover the INDEX_E + its buffer/address.  Returns the term
// unchanged when it is already an INDEX_E (or not a cast).
static Term rmu_peel_cast(Term t) {
  if (term_tag(t) != TAG_UOP) return t;
  u32 op = term_ext(t);
  if (op == UOP_CAST || op == UOP_BITCAST) {
    Term src = heap_read(term_val(t) + 0);
    if (term_tag(src) == TAG_UOP && term_ext(src) == UOP_INDEX_E) return src;
  }
  return t;
}

// === THVM_FUSE_MATMUL_INPUT: elementwise-producer-into-matmul-A-load ===
//
// With the scheduler flag on, a single-consumer elementwise chain feeding a
// matmul (e.g. the FLUX modulation ln*(sc+1)+sh, a SiLU, a cast) is un-realized
// (rangeify_unified.c rb_matmul_input_fusable), so the matmul's MUL operand A
// is no longer a bare INDEX_E over a realized buffer -- it is an inlined ALU
// tree whose leaves are INDEX_E reads over the original buffers, indexed at the
// matmul's (m, k) iteration.  These helpers let rmu_emit_matmul_tc recognise
// such an A operand and compute it inline in the TC A-staging load (keeping the
// simdgroup_matrix tensor cores) instead of falling to the scalar accumulator.
// Mirrors tinygrad: the elementwise stays in the matmul's reduce kernel and its
// compute is emitted in the load (schedule/rangeify.py:40-46 pm_syntactic_sugar
// pushes INDEX through elementwise; codegen lowers it at the operand load).

// Find the first INDEX_E leaf in an elementwise subtree (the representative for
// M/K axis + stride extraction).  Bounded recursion.  Returns 0 if none.
static Term rmu_fused_first_index(Term t, u32 depth) {
  if (depth > 40 || term_tag(t) != TAG_UOP) return 0;
  u32 op = term_ext(t);
  if (op == UOP_INDEX_E) return t;
  u8 ar = uop_arity(op);
  for (u8 i = 0; i < ar; i++) {
    Term r = rmu_fused_first_index(heap_read(term_val(t) + i), depth + 1);
    if (r != 0) return r;
  }
  return 0;
}

// 1 iff an INDEX_E address subtree contains an IDIV/IMOD (a movement-op
// swizzler the simple m/k staging decode cannot follow).  Bounded recursion.
static int rmu_addr_has_divmod(Term t, u32 depth) {
  if (depth > 64 || term_tag(t) != TAG_UOP) return 0;
  u32 op = term_ext(t);
  if (op == UOP_IDIV || op == UOP_IMOD) return 1;
  u8 ar = uop_arity(op);
  for (u8 i = 0; i < ar; i++) {
    if (rmu_addr_has_divmod(heap_read(term_val(t) + i), depth + 1)) return 1;
  }
  return 0;
}

// Verify `t` is a TC-fusable A producer: a pure ELEMENTWISE/CAST/CONST tree
// over INDEX_E leaves, where NO INDEX_E address depends on the matmul's N axis
// (n_axis) -- A is read independently of N -- and no address uses IDIV/IMOD
// (those need the m/k decode the simple staging loop can't follow).  The reduce
// (K) axis and the M axis are allowed; broadcast (CONST-0) axes are fine.
// Returns 1 if the whole tree is safe to emit inline at the A-staging load.
static int rmu_fused_producer_ok(Term t, u32 n_axis, u32 depth) {
  if (depth > 40) return 0;
  if (term_tag(t) == TAG_NUM) return 1;
  if (term_tag(t) != TAG_UOP) return 0;
  u32 op = term_ext(t);
  if (op == UOP_CONST) return 1;
  if (op == UOP_INDEX_E) {
    // Scan the address: bail on N-axis dependence or div/mod swizzlers.
    Term addr = heap_read(term_val(t) + 1);
    Term rngs[MAX_DIM]; u32 nr = 0;
    rmu_collect_ranges(addr, rngs, &nr);
    for (u32 i = 0; i < nr; i++) {
      if (term_val(heap_read(term_val(rngs[i]) + 0)) == n_axis) return 0;
    }
    if (rmu_addr_has_divmod(addr, 0)) return 0;
    return 1;
  }
  // Pure-value elementwise ops the A-staging can recompute per element.
  if (op == UOP_CAST || op == UOP_BITCAST
      || uop_is_unary_elementwise((u8)op)
      || uop_is_binary_elementwise((u8)op)
      || uop_is_ternary_elementwise((u8)op)) {
    u8 ar = uop_arity(op);
    for (u8 i = 0; i < ar; i++) {
      if (!rmu_fused_producer_ok(heap_read(term_val(t) + i), n_axis, depth + 1))
        return 0;
    }
    return 1;
  }
  return 0;  // reduce / unknown / movement: not inline-safe
}

// THVM_FUSE_MATMUL_EPILOGUE (default OFF): a single-consumer elementwise/cast
// chain that CONSUMES one TC matmul output is fused into the matmul kernel's
// STORE (the epilogue) instead of realizing a separate elementwise kernel.
// Mirrors tinygrad schedule/indexing.py:198-200 (a single-consumer op inherits
// its consumer's ranges -> the reduce/matmul's store IS the elementwise group's
// store).  The matmul stays on the TC path; the epilogue is applied per output
// element at the bf16 staging copy in rmu_emit_matmul_tc_tiled.
static int rmu_fuse_epilogue_on(void) {
  static int known = 0, on = 0;
  if (!known) {
    char const *e = getenv("THVM_FUSE_MATMUL_EPILOGUE");
    on = (e != NULL && e[0] == '1');
    known = 1;
  }
  return on;
}

// THVM_TC_BATCHED (default OFF): route a TRUE batched matmul (attention's
// Q@K^T and @V, batch=heads) through the register-blocked tiled emitter
// (rmu_emit_matmul_tc_tiled) instead of the ~2-TFLOPS parallel_tc per-8x8-tile
// body -- ~5x on the attention GEMM.  The emitter adds a per-batch base offset
// (_batch * M*K / K*N / M*N) to the A/B/C addresses; the dispatch grid becomes
// batch * m_tiles * n_tiles.  Flag-off keeps the byte-identical parallel_tc path.
static int rmu_tc_batched_on(void) {
  static int known = 0, on = 0;
  if (!known) {
    char const *e = getenv("THVM_TC_BATCHED");
    on = (e != NULL && e[0] == '1');
    known = 1;
  }
  return on;
}

// An index-layer integer op (the addr/predicate arithmetic the production
// lifter spells over UOP_RANGE leaves).  These are positionally bound to the
// output (m, n) ranges -- the epilogue renderer substitutes _m/_n for the M/N
// RANGE terms and rmu_emit_term spells the rest per output element -- so they
// are epilogue-safe (not data movement).  A broadcast EXPAND of a {1,dim} gate
// over the sequence axis surfaces here as an IWHERE(ILT(range, extent), val, 0)
// valid-mask / IMOD index, which is exactly this index arithmetic.
static int rmu_epilogue_is_index_int_op(u32 op) {
  return op == UOP_RANGE || op == UOP_IADD || op == UOP_ISUB || op == UOP_IMUL
      || op == UOP_IDIV  || op == UOP_IMOD || op == UOP_ILT  || op == UOP_IAND
      || op == UOP_IOR   || op == UOP_IXOR || op == UOP_ISHR || op == UOP_IWHERE;
}

// Walk a store-value tree: it must be a pure ELEMENTWISE/CAST tree over
// INDEX_E/CONST leaves plus EXACTLY ONE UOP_OPT(TC) leaf (the matmul).  A bare
// UOP_REDUCE, a non-TC UOP_OPT, or any unknown/movement op makes it non-fusable.
// Index-layer integer ALU (RANGE / I*) is accepted -- it is the address/valid-
// mask arithmetic the epilogue renders per output element (see
// rmu_epilogue_is_index_int_op).  Does not recurse into the OPT_TC (that is the
// matmul, emitted by the template).
static int rmu_epilogue_walk(Term t, Term *out_tc, int *n_tc, u32 depth) {
  if (depth > 48) return 0;
  if (term_tag(t) == TAG_NUM) return 1;
  if (term_tag(t) != TAG_UOP) return 1;       // RANGE / leaf var -- fine
  u32 op = term_ext(t);
  if (op == UOP_OPT) {
    if (uop_opt_kind(t) != UOP_OPT_TC) return 0;
    *out_tc = t; (*n_tc)++;
    return 1;
  }
  if (op == UOP_CONST || op == UOP_INDEX_E) return 1;
  if (op == UOP_REDUCE) return 0;
  if (op == UOP_CAST || op == UOP_BITCAST
      || uop_is_unary_elementwise((u8)op)
      || uop_is_binary_elementwise((u8)op)
      || uop_is_ternary_elementwise((u8)op)
      || rmu_epilogue_is_index_int_op(op)) {
    u8 ar = uop_arity(op);
    for (u8 i = 0; i < ar; i++) {
      if (!rmu_epilogue_walk(heap_read(term_val(t) + i), out_tc, n_tc, depth + 1))
        return 0;
    }
    return 1;
  }
  return 0;   // unknown/movement op: not epilogue-safe
}
static int rmu_epilogue_find_single_tc(Term value, Term *out_tc) {
  int n_tc = 0; Term tc = 0;
  if (!rmu_epilogue_walk(value, &tc, &n_tc, 0)) return 0;
  if (n_tc != 1 || tc == 0) return 0;
  *out_tc = tc;
  return 1;
}

static int rmu_detect_matmul_tc(Term store, Term *out_red_value) {
  if (term_tag(store) != TAG_UOP || term_ext(store) != UOP_STORE) return 0;
  Term value = heap_read(term_val(store) + 2);
  Term tc = value;
  // Epilogue: the store value may be an elementwise CHAIN over a single OPT_TC.
  if (!(term_tag(value) == TAG_UOP && term_ext(value) == UOP_OPT
        && uop_opt_kind(value) == UOP_OPT_TC)) {
    if (!rmu_fuse_epilogue_on()) return 0;
    if (!rmu_epilogue_find_single_tc(value, &tc)) return 0;
  }
  if (term_tag(tc) != TAG_UOP || term_ext(tc) != UOP_OPT
      || uop_opt_kind(tc) != UOP_OPT_TC) return 0;
  Term inner = uop_opt_target(tc);
  if (term_tag(inner) != TAG_UOP || term_ext(inner) != UOP_REDUCE) return 0;
  u64 rloc = term_val(inner);
  u32 kind = term_val(heap_read(rloc + 1));
  if (kind != REDUCE_SUM) return 0;
  Term mul = heap_read(rloc + 0);
  if (term_tag(mul) != TAG_UOP || term_ext(mul) != UOP_MUL) return 0;
  // LHS / RHS must be INDEX_E reads, possibly wrapped in a bf16->f32 CAST
  // (the widening feeding an f32 accumulator on a bf16-input matmul).
  Term lhs = rmu_peel_cast(heap_read(term_val(mul) + 0));
  Term rhs = rmu_peel_cast(heap_read(term_val(mul) + 1));
  // THVM_FUSE_MATMUL_INPUT: A may be a fused elementwise producer.  Accept it
  // iff it is a pure-elementwise/cast tree over INDEX_E leaves with no N-axis
  // dependence (rmu_fused_producer_ok); rmu_emit_matmul_tc computes it inline at
  // the A-staging load.  B must stay a bare INDEX_E (the clean weight read).
  if (!(term_tag(lhs) == TAG_UOP && term_ext(lhs) == UOP_INDEX_E)) {
    if (!ru_fuse_matmul_input_on()) return 0;
    if (rmu_fused_first_index(lhs, 0) == 0) return 0;  // needs at least one leaf
  }
  if (term_tag(rhs) != TAG_UOP || term_ext(rhs) != UOP_INDEX_E) return 0;
  if (out_red_value != NULL) *out_red_value = inner;
  return 1;
}

// Filter the collected ranges, splitting them into output ranges
// (axis_id != reduce_axis) and the reduce range (axis_id == reduce_axis,
// at most one).  Used by the REDUCE-as-store-value shape so the
// renderer can emit output loops outside, accumulator+inner loop
// inside.  Returns the index of the reduce range in the input array,
// or n_ranges if not found.
static u32 rmu_split_reduce(Term *ranges, u32 *opt_kinds,
                            u32 *opt_factors, u32 n_ranges,
                            u32 reduce_axis,
                            Term *out_ranges, u32 *out_kinds,
                            u32 *out_factors, u32 *n_out) {
  u32 reduce_idx = n_ranges;
  *n_out = 0;
  for (u32 i = 0; i < n_ranges; i++) {
    u32 axis_id = term_val(heap_read(term_val(ranges[i]) + 0));
    if (axis_id == reduce_axis) {
      reduce_idx = i;
      continue;
    }
    out_ranges  [*n_out] = ranges[i];
    out_kinds   [*n_out] = opt_kinds[i];
    out_factors [*n_out] = opt_factors[i];
    (*n_out)++;
  }
  return reduce_idx;
}

// Emit `<acc> = <combine(kind, acc, src)>;` per the REDUCE kind.
static void rmu_emit_reduce_combine(const char *acc_name, u32 kind,
                                    Term src, FILE *fp) {
  if (kind == REDUCE_MAX) {
    fprintf(fp, "%s = fmax(%s, ", acc_name, acc_name);
    rmu_emit_term(src, fp);
    fputs(");\n", fp);
  } else {
    // SUM (default).
    fprintf(fp, "%s = %s + ", acc_name, acc_name);
    rmu_emit_term(src, fp);
    fputs(";\n", fp);
  }
}

// Emit init expression for a REDUCE kind: 0.0f for SUM, -INFINITY for MAX.
static void rmu_emit_reduce_init(u32 kind, FILE *fp) {
  if (kind == REDUCE_MAX) fputs("-INFINITY", fp);
  else                    fputs("0.0f", fp);
}

// ---- Threadgroup-memory-tiled simdgroup_matrix matmul (Metal) ------
//
// Ports tinygrad's apply_tensor_cores + UPCAST(M)/UPCAST(N)/LOCAL(N)
// opt stack (tinygrad/codegen/opt/heuristic.py:36-44 + tinygrad/codegen/
// opt/tc.py:140 `metal` TC) into a single hand-emitted MSL kernel.
//
// The baseline parallel-TC kernel (one 32-thread simdgroup per 8x8
// output tile) is MEMORY-BOUND: each MMA reloads an A(8x8) + B(8x8)
// strip from device memory with zero reuse (~7 GB traffic for a
// 14.5-GFLOP matmul).  This kernel makes it COMPUTE-BOUND two ways,
// exactly as tinygrad's opt stack does:
//
//   1. Register blocking (UPCAST M & N): each simdgroup holds an
//      RM x RN grid of simdgroup_matrix<float,8,8> ACCUMULATORS and
//      reuses each loaded A-fragment across RN columns and each
//      B-fragment across RM rows -- (RM+RN)/(RM*RN) of the loads.
//   2. Threadgroup-memory staging (LOCAL N + the K GROUP): the
//      LOCAL_M x LOCAL_N simdgroups in a threadgroup cooperatively
//      load one A K-block [TILE_M x KB] + one B K-block [KB x TILE_N]
//      into `threadgroup` arrays ONCE, barrier, then every simdgroup
//      reads its fragments from threadgroup memory.  Each device byte
//      is read once per K-block and reused across LOCAL_M*LOCAL_N
//      simdgroups.
//
// Tile geometry (per threadgroup):
//   TILE_M = LOCAL_M * RM * 8   output rows
//   TILE_N = LOCAL_N * RN * 8   output cols
//   KB                          K-block staged in threadgroup memory
// Threadgroup memory budget: (TILE_M + TILE_N) * KB * 4 bytes
//   (default 64x64 tile, KB=32 -> (64+64)*32*4 = 16 KiB < 32 KiB cap).
//
// Dispatch (see cg_tile_metal_dispatch_shape):
//   grid        = (M/TILE_M) * (N/TILE_N) threadgroups
//   threadgroup = LOCAL_M * LOCAL_N * 32 threads
//
// Assumes the canonical row-major matmul address layout (A ld = K,
// B ld = N, C ld = N) -- the same assumption the legacy 8x8 emit makes
// when it passes k_extent / n_extent as the simdgroup_load leading
// dims.  Requires M % TILE_M == 0, N % TILE_N == 0, K % KB == 0; the
// caller picks divisor-respecting tile factors and falls back to the
// 8x8 path otherwise.
//
// RmuTcTile is declared at the top of render_metal.c (included first in
// the unity build) so cg_tile_metal_dispatch_shape can size the grid to
// the same geometry.

// Pick tile factors for an (M,N,K) matmul.  Returns 1 with *out filled
// Affine stride of `axis_id` in an INDEX_E address subtree (1 = unit
// stride, 0 = absent or non-affine).  Thin wrapper over the affine-coeff
// walker (defined later in the file) used by the tiled-TC operand-layout
// probe.
static int rmu_axis_affine_coeff(Term t, u32 axis_id, i64 *coeff);
static i64 rmu_range_coeff(Term t, u32 axis_id) {
  i64 c = 0;
  (void) rmu_axis_affine_coeff(t, axis_id, &c);
  return c;
}

// when a tiled kernel is worthwhile and the dims divide cleanly; 0 to
// tell the caller to emit the legacy 8x8 path.  Preference order
// targets the common FLUX GEMM shapes (M/N multiples of 64; K multiple
// of 32) while degrading gracefully for ragged M/N (e.g. M=24).
static int rmu_tc_pick_tile(u32 m_extent, u32 n_extent, u32 k_extent,
                            RmuTcTile *out) {
  if ((m_extent % 8) != 0 || (n_extent % 8) != 0 || (k_extent % 8) != 0) {
    return 0;
  }
  // THVM_TC_TILE=0 disables the tiled path (legacy 8x8 emit) -- for
  // A/B benchmarking against the baseline.
  {
    char const *e = getenv("THVM_TC_TILE");
    if (e && e[0] == '0') return 0;
  }
  // Small-GEMM guard: when M or N is tiny the matmul is latency-bound,
  // not bandwidth-bound -- the threadgroup-staging + barrier overhead of
  // the tiled kernel loses to the simple one-tile-per-simdgroup 8x8 path
  // (measured: M=24 regressed 1200 -> 557 GFLOPS).  Require both M and N
  // >= 64 (the smallest tile that fills multiple simdgroups with reuse).
  if (m_extent < 64 || n_extent < 64) return 0;
  u32 m8 = m_extent / 8, n8 = n_extent / 8;
  // Candidate tile geometries, ordered best-first by a gpu_us sweep on
  // M3 Max (THVM_KERNEL_PROFILE + TMetalGpuTime delta over the FLUX
  // GEMM shapes {768,3072}x{3072,3072/18432}).  Each row is
  // {local_m, local_n, rm, rn}: simdgroups along M/N and the register
  // 8x8 tile per simdgroup.  TILE_M = local_m*rm*8, TILE_N = local_n*rn*8.
  // Findings the order encodes:
  //   - 8 simdgroups / 256 threads per threadgroup wins on occupancy.
  //   - M-major local layout (local_m >= local_n) beats N-major.
  //   - a fat 4x4 register tile per simdgroup maximises A/B fragment
  //     reuse without spilling.
  // The picker takes the first row whose TILE_M divides M and TILE_N
  // divides N, degrading to smaller tiles (and finally a 16x16 tile)
  // for ragged shapes.  KB is fixed at 8 (one staged K-subtile): the
  // sweep showed larger K-blocks lose to threadgroup-memory pressure.
  struct { u32 lm, ln, rm, rn; } cands[] = {
    {4, 2, 4, 4},   // 128x64, 8 sg, 32 acc/sg  -- best on 768x3072x*
    {4, 2, 2, 4},   // 64x64,  8 sg
    {4, 2, 2, 2},   // 64x32,  8 sg
    {2, 2, 4, 4},   // 64x64,  4 sg
    {2, 2, 2, 4},   // 32x64,  4 sg
    {2, 2, 2, 2},   // 32x32,  4 sg
    {2, 1, 2, 2},   // 32x16
    {1, 2, 2, 2},   // 16x32
    {2, 1, 2, 1},   // 32x8
    {1, 2, 1, 2},   // 8x32
    {1, 1, 2, 2},   // 16x16
  };
  // Occupancy-aware selection.  Candidates are ordered largest-tile-first (best
  // A/B fragment reuse).  But a large tile on a small output under-fills the GPU:
  // FLUX's QKV {M=256,N=3072} with the 128x64 tile makes only (256/128)*(3072/64)
  // = 96 output tiles -- on a 10-40 core Apple GPU that is ~2-3 waves, so the last
  // (partial) wave runs a fraction of the cores (wave-quantization tail waste).
  // Measured: 128x64 ~4.7 vs the 64x64 step-down ~7.6 TFLOPS on that shape.  So
  // pick the LARGEST tile whose output-tile count is >= MIN_TG (enough waves that
  // the tail is a small fraction); only when no candidate clears MIN_TG (a
  // genuinely tiny GEMM) fall back to the smallest dividing tile (the most tiles).
  // Large GEMMs already clear MIN_TG at the biggest tile, so they are unchanged.
  // MIN_TG ~ a few x the core count; env THVM_TC_MIN_TG tunes it.
  u32 min_tg = 160;
  { char const *e = getenv("THVM_TC_MIN_TG"); if (e && e[0]) min_tg = (u32)atoi(e); }
  u32 best_lm = 0, best_ln = 0, best_rm = 0, best_rn = 0;
  u32 fb_lm = 0, fb_ln = 0, fb_rm = 0, fb_rn = 0, fb_tg = 0;
  for (u32 i = 0; i < sizeof(cands)/sizeof(cands[0]); i++) {
    u32 tm8 = cands[i].lm * cands[i].rm;
    u32 tn8 = cands[i].ln * cands[i].rn;
    if (m8 % tm8 != 0 || n8 % tn8 != 0) continue;
    u32 tg = (m8 / tm8) * (n8 / tn8);                 // output-tile (threadgroup) count
    if (best_lm == 0 && tg >= min_tg) {               // first (largest) meeting the floor
      best_lm = cands[i].lm; best_ln = cands[i].ln;
      best_rm = cands[i].rm; best_rn = cands[i].rn;
    }
    if (tg > fb_tg) {                                 // track the most-tiles dividing tile
      fb_tg = tg; fb_lm = cands[i].lm; fb_ln = cands[i].ln;
      fb_rm = cands[i].rm; fb_rn = cands[i].rn;
    }
  }
  if (best_lm == 0) {                                 // nothing cleared the floor: max tiles
    best_lm = fb_lm; best_ln = fb_ln; best_rm = fb_rm; best_rn = fb_rn;
  }
  if (best_lm == 0) return 0;
  if (getenv("THVM_TC_DBG"))
    fprintf(stderr, "[tc-pick] M=%u N=%u K=%u -> tile %ux%u (lm=%u ln=%u rm=%u rn=%u) tg=%u\n",
            m_extent, n_extent, k_extent, best_lm*best_rm*8u, best_ln*best_rn*8u,
            best_lm, best_ln, best_rm, best_rn,
            (m8/(best_lm*best_rm))*(n8/(best_ln*best_rn)));
  // The tiled kernel only pays off when there is reuse: require the
  // output tile to span more than a single 8x8 fragment.
  if (best_lm * best_rm * best_ln * best_rn <= 1) return 0;
  u32 kb = 8;
  out->local_m = best_lm; out->local_n = best_ln;
  out->rm = best_rm;      out->rn = best_rn;
  out->kb = kb;
  // Tuning overrides (THVM_TC_LM / _LN / _RM / _RN / _KB): force tile
  // factors for benchmarking.  Only applied when the forced tile still
  // divides the shape cleanly; ignored otherwise so a stray env var
  // can't emit an incorrect kernel.
  {
    char const *e;
    u32 lm = out->local_m, ln = out->local_n, rm = out->rm, rn = out->rn, kbo = out->kb;
    if ((e = getenv("THVM_TC_LM")) && e[0]) lm = (u32)atoi(e);
    if ((e = getenv("THVM_TC_LN")) && e[0]) ln = (u32)atoi(e);
    if ((e = getenv("THVM_TC_RM")) && e[0]) rm = (u32)atoi(e);
    if ((e = getenv("THVM_TC_RN")) && e[0]) rn = (u32)atoi(e);
    if ((e = getenv("THVM_TC_KB")) && e[0]) kbo = (u32)atoi(e);
    if (lm >= 1 && ln >= 1 && rm >= 1 && rn >= 1 && kbo >= 8
        && (kbo % 8) == 0
        && m8 % (lm * rm) == 0 && n8 % (ln * rn) == 0
        && k_extent % kbo == 0) {
      out->local_m = lm; out->local_n = ln;
      out->rm = rm;      out->rn = rn;
      out->kb = kbo;
    }
  }
  return 1;
}

// If the KOP_TC opt carried an autotune-chosen tile config (factor packed
// via tc_tile_pack, bit 31 set), unpack it and -- when it passes the SAME
// validity/SMEM/occupancy check the env override applies -- overwrite the
// heuristic-picked tile in *out.  An invalid or absent config leaves *out
// untouched (the heuristic stays), so the default (factor 0/8/16/32) path
// is byte-identical.  `unit`/`is_cuda` match the backend picker.
static void rmu_tc_apply_opt_config(u32 tc_opt_factor,
                                    u32 m_extent, u32 n_extent, u32 k_extent,
                                    u32 unit, int is_cuda, int c_is_bf,
                                    RmuTcTile *out) {
  if (!tc_tile_is_packed(tc_opt_factor)) return;
  u32 lm, ln, rm, rn, kb;
  tc_tile_unpack(tc_opt_factor, &lm, &ln, &rm, &rn, &kb);
  if (!tc_tile_valid(m_extent, n_extent, k_extent, lm, ln, rm, rn, kb,
                     unit, is_cuda, c_is_bf)) {
    if (getenv("THVM_TC_DBG"))
      fprintf(stderr, "[tc-opt] M=%u N=%u K=%u packed tile "
              "(lm=%u ln=%u rm=%u rn=%u kb=%u) INVALID -> heuristic\n",
              m_extent, n_extent, k_extent, lm, ln, rm, rn, kb);
    return;
  }
  out->local_m = lm; out->local_n = ln;
  out->rm = rm;      out->rn = rn;
  out->kb = kb;
  if (getenv("THVM_TC_DBG"))
    fprintf(stderr, "[tc-opt] M=%u N=%u K=%u -> packed tile "
            "(lm=%u ln=%u rm=%u rn=%u kb=%u)\n",
            m_extent, n_extent, k_extent, lm, ln, rm, rn, kb);
}

// CUDA WMMA tile picker: pick (local_m, local_n, rm, rn, kb) for an
// (M,N,K) matmul where the unit fragment is 16x16x16 (nvcuda::wmma)
// rather than Metal's 8x8 simdgroup_matrix.  TILE_M = local_m*rm*16,
// TILE_N = local_n*rn*16; the block has local_m*local_n warps (=32
// threads each) and KB K-staged per block.  Returns 1 with *out filled
// when a tile divides the shape; 0 to fall back to the naive WMMA path.
//
// A100 sweep (768x27648x3072, ms/TFLOPS at bf16, single-buffered):
//   128x256 32warp KB=32  1.18 ms  110 TFLOPS  (35% peak)  <- WINNER
//   128x128  8warp KB=64  2.85 ms   46 TFLOPS
//   128x128  4warp KB=32  2.95 ms   44 TFLOPS
//   128x128  8warp KB=32  3.99 ms   33 TFLOPS
// The 128x256 tile with 32 warps (1024 threads = max occupancy) and a
// modest 2x2 register block of 16x16 fragments dominates: huge A/B reuse
// across 32 warps, low register pressure (4 acc frags/warp), and the wide
// 256-N tile suits the large-N FLUX projections.  KB stays 32 -- a larger
// KB on the big tile overflows the 48 KiB/block static shared cap (the
// budget guard below rejects it).  Double-buffering gave no win (the
// kernel is not global-load-latency bound), so single-buffer is default.
// Candidates are ordered largest-tile-first; the picker takes the largest
// whose output-block count clears the A100's occupancy floor (108 SMs),
// then the most-blocks dividing tile, down to a 16x16 last resort.
static int rmu_tc_pick_tile_cuda(u32 m_extent, u32 n_extent, u32 k_extent,
                                 int c_is_bf, RmuTcTile *out) {
  if ((m_extent % 16) != 0 || (n_extent % 16) != 0 || (k_extent % 16) != 0) {
    return 0;
  }
  // THVM_TC_TILE=0 forces the naive one-warp-per-16x16 WMMA path (A/B).
  { char const *e = getenv("THVM_TC_TILE"); if (e && e[0] == '0') return 0; }
  u32 m16 = m_extent / 16, n16 = n_extent / 16;
  // {local_m, local_n, rm, rn}: warps along M/N and the 16x16 register tile per
  // warp.  TILE_M = local_m*rm*16, TILE_N = local_n*rn*16; warps = local_m*local_n.
  // The f32 and bf16/fp16 OUTPUT paths want DIFFERENT tiles (2026-06-17 A100
  // sweep):
  //  - f32 out stores the accumulator fragment DIRECTLY (no convert scratch), so
  //    a big 32-warp 128x256 tile maximises reuse and hits ~110 TFLOPS / 35% peak.
  //  - bf16/fp16 out needs a per-warp 256-float convert scratch (store_matrix_sync
  //    can't widen f32->bf16): a 32-warp tile's 32 KiB scratch craters occupancy
  //    (128x256/32w only reached ~49 TFLOPS).  The bf16 sweet spot is the
  //    OPPOSITE -- few warps, fat register block: a 4-warp 128x128 tile with a
  //    4x4 (16-fragment) per-warp accumulator (tiny 4 KiB scratch, high
  //    arithmetic intensity, many resident blocks) hit ~81 TFLOPS, +30% over the
  //    old 16-warp 128x128 and beating every >48 KiB tile.  KB=32 was optimal
  //    (KB=16 halved throughput; KB=64 was slightly worse).
  static const struct rmu_tc_cand { u32 lm, ln, rm, rn; }
  cands_f32[] = {
    {4, 8, 2, 2},   // 128x256, 32 warps  -- f32 sweep winner
    {8, 4, 2, 2},   // 256x128, 32 warps  (M-major fallback)
    {4, 4, 2, 2},   // 128x128, 16 warps
    {2, 4, 4, 2},   // 128x128, 8 warps, 8 acc/warp
    {4, 4, 2, 1},   // 128x64,  16 warps
    {2, 4, 2, 2},   // 64x128,  8 warps
    {2, 2, 4, 2},   // 128x64,  4 warps
    {2, 2, 2, 2},   // 64x64,   4 warps
    {2, 2, 2, 1},   // 64x32,   4 warps
    {2, 1, 2, 2},   // 64x32,   2 warps
    {1, 2, 2, 2},   // 32x64,   2 warps
    {2, 2, 1, 1},   // 32x32,   4 warps
    {1, 1, 2, 2},   // 32x32,   1 warp
    {1, 1, 1, 1},   // 16x16,   1 warp (last resort)
  },
  cands_bf16[] = {
    {2, 2, 4, 4},   // 128x128, 4 warps, 16 acc/warp -- bf16 sweep winner ~81 TFLOPS
    {2, 2, 4, 2},   // 128x64,  4 warps, 8 acc/warp
    {2, 2, 2, 4},   // 64x128,  4 warps, 8 acc/warp
    {2, 1, 4, 4},   // 128x64,  2 warps, 16 acc/warp
    {1, 2, 4, 4},   // 64x128,  2 warps, 16 acc/warp
    {2, 2, 2, 2},   // 64x64,   4 warps
    {1, 1, 4, 4},   // 64x64,   1 warp,  16 acc/warp
    {2, 2, 2, 1},   // 64x32,   4 warps
    {2, 1, 2, 2},   // 64x32,   2 warps
    {1, 2, 2, 2},   // 32x64,   2 warps
    {2, 2, 1, 1},   // 32x32,   4 warps
    {1, 1, 2, 2},   // 32x32,   1 warp
    {1, 1, 1, 1},   // 16x16,   1 warp (last resort)
  };
  const struct rmu_tc_cand *cands = c_is_bf ? cands_bf16 : cands_f32;
  u32 ncands = c_is_bf ? (u32)(sizeof(cands_bf16) / sizeof(cands_bf16[0]))
                       : (u32)(sizeof(cands_f32) / sizeof(cands_f32[0]));
  // Per-candidate KB + static shared-memory budget.  The emitter stages
  // A {tile_m x KB} + B {KB x tile_n}, each padded by 8 along the leading
  // dim, in 2-byte (bf16/fp16) elements; worst case (transposed B, staged
  // n-major) = (tile_m + tile_n) * (KB+8) * 2 bytes.  A bf16/fp16 OUTPUT
  // ALSO needs a dedicated nwarps*256-float convert scratch
  // (store_matrix_sync can't widen f32->bf16): nwarps*256*4 bytes.  nvcc's
  // static __shared__ cap is 48 KiB/block on sm_70..sm_86 -- a tile whose
  // budget exceeds it fails to launch (the KB=64 128x256 cliff; the 32-warp
  // bf16 scratch is 32 KiB).  Pick the largest dividing tile whose budget
  // FITS at KB=32 (or KB=16) AND clears the occupancy floor; fall back to
  // the most-blocks fitting tile when none clears the floor.
  // Block-count floor (tg >= min_tg) so the chosen tile launches enough blocks
  // to fill the GPU.  f32's big 128x256 tile wants ~2 blocks/SM (216 on a
  // 108-SM A100); the bf16 winner is a small 128x128/4-warp tile whose high
  // register-blocked arithmetic intensity stays efficient at very low block
  // counts (it ties or beats every alternative down to ~48 blocks -- the
  // 2026-06-17 small-M sweep), so bf16 uses a much lower floor to keep that
  // tile selected for the small attention-projection shapes too.
  u32 min_tg = c_is_bf ? 48u : 216u;
  { char const *e = getenv("THVM_TC_MIN_TG"); if (e && e[0]) min_tg = (u32)atoi(e); }
  // Static __shared__ per-block cap (48 KiB on sm_70..sm_86).  A tile whose
  // footprint exceeds it is skipped.  THVM_TC_SMEM_CAP opts into DYNAMIC shared
  // (the emitter emits one `extern __shared__` block + pointer aliases and the
  // launch sets CU_FUNC_ATTRIBUTE_MAX_DYNAMIC_SHARED_SIZE_BYTES + passes
  // sharedMemBytes) for tiles above 48 KiB -- but the 2026-06-17 sweep showed
  // every >48 KiB tile is SLOWER here (a 32-warp tile's 32 KiB f32 convert
  // scratch craters occupancy), so the default cap stays 48 KiB and all
  // default-selected tiles fit statically.  The dynamic path is kept as a
  // verified, opt-in capability for future arch/shape exploration.
  u32 cap = 48u * 1024u;
  { char const *e = getenv("THVM_TC_SMEM_CAP"); if (e && e[0]) cap = (u32)atoi(e); }
  u32 best_lm = 0, best_ln = 0, best_rm = 0, best_rn = 0, best_kb = 0;
  u32 fb_lm = 0, fb_ln = 0, fb_rm = 0, fb_rn = 0, fb_kb = 0, fb_tg = 0;
  for (u32 i = 0; i < ncands; i++) {
    u32 tm16 = cands[i].lm * cands[i].rm;
    u32 tn16 = cands[i].ln * cands[i].rn;
    if (m16 % tm16 != 0 || n16 % tn16 != 0) continue;
    u32 tile_m = tm16 * 16u, tile_n = tn16 * 16u;
    u32 nwarps = cands[i].lm * cands[i].ln;
    u64 c_scratch = c_is_bf ? (u64)nwarps * 256u * 4u : 0u;
    // Largest KB (32 then 16) whose staging + scratch fits the cap.
    u32 kb = 0;
    for (u32 cand_kb = (k_extent % 32 == 0) ? 32u : 16u; cand_kb >= 16u; cand_kb -= 16u) {
      if ((k_extent % cand_kb) != 0) continue;
      if ((u64)(tile_m + tile_n) * (u64)(cand_kb + 8u) * 2u + c_scratch <= (u64)cap) {
        kb = cand_kb; break;
      }
      if (cand_kb == 16u) break;
    }
    if (kb == 0) continue;                 // tile too large for shared: skip
    u32 tg = (m16 / tm16) * (n16 / tn16);
    if (best_lm == 0 && tg >= min_tg) {
      best_lm = cands[i].lm; best_ln = cands[i].ln;
      best_rm = cands[i].rm; best_rn = cands[i].rn; best_kb = kb;
    }
    if (tg > fb_tg) {
      fb_tg = tg; fb_lm = cands[i].lm; fb_ln = cands[i].ln;
      fb_rm = cands[i].rm; fb_rn = cands[i].rn; fb_kb = kb;
    }
  }
  if (best_lm == 0) {
    best_lm = fb_lm; best_ln = fb_ln; best_rm = fb_rm; best_rn = fb_rn; best_kb = fb_kb;
  }
  if (best_lm == 0) return 0;
  u32 kb = best_kb;
  out->local_m = best_lm; out->local_n = best_ln;
  out->rm = best_rm;      out->rn = best_rn;
  out->kb = kb;
  // Tuning overrides (THVM_TC_LM / _LN / _RM / _RN / _KB) for A/B; only
  // applied when the forced tile still divides the shape cleanly.
  {
    char const *e;
    u32 lm = out->local_m, ln = out->local_n, rm = out->rm, rn = out->rn, kbo = out->kb;
    if ((e = getenv("THVM_TC_LM")) && e[0]) lm = (u32)atoi(e);
    if ((e = getenv("THVM_TC_LN")) && e[0]) ln = (u32)atoi(e);
    if ((e = getenv("THVM_TC_RM")) && e[0]) rm = (u32)atoi(e);
    if ((e = getenv("THVM_TC_RN")) && e[0]) rn = (u32)atoi(e);
    if ((e = getenv("THVM_TC_KB")) && e[0]) kbo = (u32)atoi(e);
    if (lm >= 1 && ln >= 1 && rm >= 1 && rn >= 1 && kbo >= 16
        && (kbo % 16) == 0
        && m16 % (lm * rm) == 0 && n16 % (ln * rn) == 0
        && k_extent % kbo == 0
        && (lm * ln * 32u) <= 1024u) {
      out->local_m = lm; out->local_n = ln;
      out->rm = rm;      out->rn = rn;
      out->kb = kbo;
    }
  }
  if (getenv("THVM_TC_DBG"))
    fprintf(stderr, "[tc-pick-cuda] M=%u N=%u K=%u -> tile %ux%u "
            "(lm=%u ln=%u rm=%u rn=%u kb=%u) tg=%u\n",
            m_extent, n_extent, k_extent,
            out->local_m*out->rm*16u, out->local_n*out->rn*16u,
            out->local_m, out->local_n, out->rm, out->rn, out->kb,
            (m16/(out->local_m*out->rm))*(n16/(out->local_n*out->rn)));
  return 1;
}

// Upper bound on the shared-memory bytes a tiled-WMMA CUDA matmul kernel uses:
// A staging (nbuf * tile_m * (KB+PAD)) + B staging + the bf16/fp16 convert
// scratch (nwarps*256 floats, only when C is bf16/fp16), all in the kernel's
// element types (WMMA inputs are 2-byte half/__nv_bfloat16; the scratch is
// float).  The B-staging footprint differs by layout (transposed B stages
// tile_n rows of KB+PAD; row-major B stages KB rows of tile_n+PAD), so take the
// MAX over both -- this stays b_trans-INDEPENDENT, so the launch
// (cuda_tc_tile_dispatch_shape -> cuda_jit_launch) and the emitter agree on the
// use_dyn decision and the launch always requests >= what the kernel declares,
// even if their independently-derived transpose flags ever diverged.  For
// FLUX's transposed-B matmuls (tile_n > KB) the max IS the transposed case, so
// it equals the emitter's real footprint exactly.  A return > 48 KiB means the
// emitter switched to `extern __shared__` and the launch must opt into dynamic.
static u32 rmu_tc_tiled_cuda_shared_bytes(RmuTcTile t, int c_is_bf) {
  u32 PAD = 8u;
  u32 tile_m = t.local_m * t.rm * 16u;
  u32 tile_n = t.local_n * t.rn * 16u;
  u32 nwarps = t.local_m * t.local_n;
  int dbuf = 0;
  { char const *e = getenv("THVM_TC_DB"); if (e && e[0] == '1') dbuf = 1; }
  u32 nbuf = dbuf ? 2u : 1u;
  u32 a_bytes = nbuf * tile_m * (t.kb + PAD) * 2u;
  u32 b_bytes_t = nbuf * tile_n * (t.kb + PAD) * 2u;   // transposed B (col-major staged)
  u32 b_bytes_n = nbuf * t.kb * (tile_n + PAD) * 2u;   // row-major B
  u32 b_bytes = b_bytes_t > b_bytes_n ? b_bytes_t : b_bytes_n;
  u32 ab_bytes = (a_bytes + b_bytes + 15u) & ~15u;     // 16-align the float scratch
  u32 c_bytes = c_is_bf ? nwarps * 256u * 4u : 0u;
  return ab_bytes + c_bytes;
}

// Emit the shared-memory-staged, register-blocked tiled WMMA matmul body
// for CUDA (the m_par && n_par both-GLOBAL case), mirroring the Metal
// rmu_emit_matmul_tc_tiled but with nvcuda::wmma 16x16x16 fragments,
// __shared__ staging, and __syncthreads barriers.
//
// Data path (correctness-first single-buffered staging):
//   - The block owns output tile (_tm, _tn) decoded from tg=blockIdx.x.
//   - Each warp (sgi = threadIdx.x/32) owns an rm x rn grid of 16x16
//     accumulator fragments at sub-tile origin (_sm, _sn).
//   - Per K-block: all warps cooperatively stage A[_tm.. , k0..] into
//     row-major __shared__ _Asm[tile_m][KB] and B's K-block into
//     row-major __shared__ _Bsm[KB][tile_n], barrier, then each warp
//     loads its rm A-frags + rn B-frags FROM SHARED (row_major) and does
//     rm*rn mma_sync, barrier.
//   - Transposed B (B = Transpose[W], W{N,K} contiguous, b_nstride=K):
//     staged into the SAME row-major _Bsm[k*tile_n + n] by reading
//     W[(_tn+n)*b_nstride + (k0+k)] -- so the in-shared layout is always
//     row_major and the WMMA B load is uniform regardless of global
//     layout.
//   - bf16/fp16 output: each accumulator fragment is stored to a per-warp
//     __shared__ float scratch[256], then the 32 lanes convert+write into
//     C; f32 output stores directly with store_matrix_sync.
static void rmu_emit_matmul_tc_tiled_cuda(const char *a_name, const char *b_name,
                                          const char *c_name, u32 n_extent,
                                          u32 k_extent, RmuTcTile t,
                                          const char *a_elem, const char *b_elem,
                                          u32 dt_c, int b_trans, i64 b_nstride,
                                          FILE *fp, u32 depth) {
  u32 WM = 16u, WN = 16u, WK = 16u;
  (void)WK;
  u32 tile_m = t.local_m * t.rm * WM;     // output rows per block
  u32 tile_n = t.local_n * t.rn * WN;     // output cols per block
  u32 n_tiles_n = n_extent / tile_n;      // tile columns in the grid
  u32 nwarps = t.local_m * t.local_n;
  u32 nthreads = nwarps * 32u;
  int c_direct = (dt_c == DT_FP32);
  // Shared leading-dim padding: a 16x16 WMMA fragment loads 16 rows of the
  // staged tile, all reading the SAME column lane -> a row stride that is a
  // multiple of 32 banks (KB=32 bf16 = 64 B = 16 banks; tile_n=128 bf16 =
  // 256 B = 64 banks) collides all 16 reads onto the same banks.  Pad the
  // leading dim by 8 elements (16 B, one bank-group) so consecutive rows
  // land in distinct banks -- the standard skew that removes the conflict.
  u32 PAD = 8u;
  u32 a_ld = t.kb + PAD;                   // _Asm row stride (M-major, ld_k)
  // B staging layout depends on the global layout:
  //  - transposed B (B=Transpose[W], W{N,K} contiguous): stage n-MAJOR
  //    _Bsm[n*(KB+PAD) + k] = W[_tn+n][_k0+k] (a pure vectorized contiguous
  //    copy of W's K-run) and load the WMMA B fragment COL_MAJOR with
  //    ld = KB+PAD (frag[k][n] = _Bsm[k + n*ld] = W[n][k] = B[k][n]).  No
  //    scatter, no bank conflict on the store.
  //  - row-major B[k][n]: stage k-MAJOR _Bsm[k*(tile_n+PAD) + n] and load
  //    ROW_MAJOR with ld = tile_n+PAD (frag[k][n] = _Bsm[k*ld + n]).
  u32 b_ld   = b_trans ? (t.kb + PAD) : (tile_n + PAD);
  u32 b_rows = b_trans ? tile_n : t.kb;    // n-major: tile_n rows; else KB
  #define IND(D) for (u32 _i = 0; _i < (D); _i++) fputs("  ", fp)
  IND(depth); fprintf(fp,
    "/* TC WMMA tiled matmul: tile %ux%u, %ux%u warps, %ux%u frag/warp, KB=%u%s */\n",
    tile_m, tile_n, t.local_m, t.local_n, t.rm, t.rn, t.kb,
    b_trans ? ", B col-major staged" : "");
  // Shared staging tiles (padded leading dim).  _Asm: tile_m x (KB+PAD).
  // Single-buffered staging by default: the A100 sweep showed double-
  // buffering gives no win (the kernel is shared-bandwidth / compute
  // bound, not global-load-latency bound) while it doubles the shared
  // footprint (halving occupancy and risking the 48 KiB cap).  THVM_TC_DB=1
  // re-enables the software pipeline (two A/B halves ping-pong: while the
  // MMA consumes K-block N from one half, the load of N+1 streams into the
  // other, one __syncthreads per K-block instead of two).
  int dbuf = 0;
  { char const *e = getenv("THVM_TC_DB"); if (e && e[0] == '1') dbuf = 1; }
  u32 nbuf = dbuf ? 2u : 1u;
  u32 a_half = tile_m * a_ld;            // elements per A buffer half
  u32 b_half = b_rows * b_ld;            // elements per B buffer half
  // Total shared footprint (A+B staging + the bf16/fp16 convert scratch).
  // > 48 KiB exceeds nvcc's STATIC __shared__ per-block cap, so emit ONE
  // `extern __shared__` block and ALIAS _Asm / _Bsm / _cscratch as pointers
  // into it (CUDA allows a single dynamic shared array per kernel); the launch
  // sets CU_FUNC_ATTRIBUTE_MAX_DYNAMIC_SHARED_SIZE_BYTES + passes
  // sharedMemBytes (rmu_tc_tiled_cuda_shared_bytes / cuda_jit_launch).  Pointer
  // indexing _Asm[i] is identical to array indexing, so the body is unchanged.
  u32 a_bytes = nbuf * a_half * 2u;      // 2-byte WMMA-input elements
  u32 b_bytes = nbuf * b_half * 2u;
  u32 ab_bytes = (a_bytes + b_bytes + 15u) & ~15u;   // 16-align the float scratch
  u32 total_shared = rmu_tc_tiled_cuda_shared_bytes(t, !c_direct);
  int use_dyn = (total_shared > 48u * 1024u);
  if (use_dyn) {
    IND(depth); fprintf(fp,
      "/* dynamic shared: %u B > 48 KiB static cap -> extern block + aliases */\n",
      total_shared);
    IND(depth); fputs("extern __shared__ __align__(16) unsigned char _dynsm[];\n", fp);
    IND(depth); fprintf(fp, "%s* _Asm = (%s*)_dynsm;\n", a_elem, a_elem);
    IND(depth); fprintf(fp, "%s* _Bsm = (%s*)(_dynsm + %uu);\n", b_elem, b_elem, a_bytes);
  } else {
    IND(depth); fprintf(fp, "__shared__ %s _Asm[%u];\n", a_elem, nbuf * a_half);
    IND(depth); fprintf(fp, "__shared__ %s _Bsm[%u];\n", b_elem, nbuf * b_half);
  }
  // This block's output-tile origin.
  IND(depth); fprintf(fp, "uint _tm = (tg / %uu) * %uu;\n", n_tiles_n, tile_m);
  IND(depth); fprintf(fp, "uint _tn = (tg %% %uu) * %uu;\n", n_tiles_n, tile_n);
  // This warp's sub-tile origin within the block tile.
  IND(depth); fprintf(fp, "uint _sm = (sgi / %uu) * %uu;\n",
                      t.local_n, t.rm * WM);
  IND(depth); fprintf(fp, "uint _sn = (sgi %% %uu) * %uu;\n",
                      t.local_n, t.rn * WN);
  // Flat thread index in the block (for cooperative staging) + lane.
  IND(depth); fputs("uint _lid = threadIdx.x;\n", fp);
  // Register accumulator fragments, all zeroed.
  IND(depth); fprintf(fp,
    "wmma::fragment<wmma::accumulator, 16, 16, 16, float> _acc[%u];\n",
    t.rm * t.rn);
  IND(depth); fprintf(fp,
    "for (uint _i = 0u; _i < %uu; _i++) wmma::fill_fragment(_acc[_i], 0.0f);\n",
    t.rm * t.rn);
  // Vectorized cooperative loads.  A's K-run (unit stride) packs 8 bf16
  // as a uint4 (16 bytes).  A transposed B reads W's contiguous K-axis,
  // also vectorizable as uint4; a plain row-major B reads its unit-stride
  // N-run.  Scalar fallback when not 8-aligned.  bf16/fp16 are 2 bytes
  // so uint4 = 8 elements; gate on 8-divisibility of the inner extent.
  int a_vec = (t.kb % 8u == 0) && (k_extent % 8u == 0);
  int b_tvec = b_trans && (t.kb % 8u == 0) && (b_nstride % 8 == 0);
  int b_vec = !b_trans && (tile_n % 8u == 0) && (n_extent % 8u == 0);
  u32 a_elems = tile_m * t.kb, b_elems = t.kb * tile_n;
  u32 n_kblk = k_extent / t.kb;
  // Emit-helper macros: cooperatively stage one K-block into the buffer
  // half at element offset BUFOFF, reading from source K-origin K0.
  #define EMIT_LOAD_A(BO, K0)                                                  \
    do {                                                                       \
      if (a_vec) {                                                             \
        IND(depth + 1); fprintf(fp,                                            \
          "for (uint _i = _lid; _i < %uu; _i += %uu) {\n", a_elems/8u, nthreads);\
        IND(depth + 2); fprintf(fp,                                            \
          "uint _r = _i / %uu, _c = (_i %% %uu) * 8u;\n", t.kb/8u, t.kb/8u);   \
        IND(depth + 2); fprintf(fp,                                            \
          "*(uint4*)(&_Asm[(%s) + _r * %uu + _c]) = "                          \
          "*(const uint4*)(&%s[(_tm + _r) * %uu + (%s) + _c]);\n",             \
          BO, a_ld, a_name, k_extent, K0);                                     \
        IND(depth + 1); fputs("}\n", fp);                                      \
      } else {                                                                 \
        IND(depth + 1); fprintf(fp,                                            \
          "for (uint _i = _lid; _i < %uu; _i += %uu) {\n", a_elems, nthreads); \
        IND(depth + 2); fprintf(fp,                                            \
          "_Asm[(%s) + (_i / %uu) * %uu + _i %% %uu] = "                       \
          "%s[(_tm + _i / %uu) * %uu + (%s) + _i %% %uu];\n",                  \
          BO, t.kb, a_ld, t.kb, a_name, t.kb, k_extent, K0, t.kb);            \
        IND(depth + 1); fputs("}\n", fp);                                      \
      }                                                                        \
    } while (0)
  #define EMIT_LOAD_B(BO, K0)                                                  \
    do {                                                                       \
      if (b_trans && b_tvec) {                                                 \
        IND(depth + 1); fprintf(fp,                                            \
          "for (uint _j = _lid; _j < %uu; _j += %uu) {\n", b_elems/8u, nthreads);\
        IND(depth + 2); fprintf(fp,                                            \
          "uint _n = _j / %uu, _k8 = (_j %% %uu) * 8u;\n", t.kb/8u, t.kb/8u);  \
        IND(depth + 2); fprintf(fp,                                            \
          "*(uint4*)(&_Bsm[(%s) + _n * %uu + _k8]) = "                         \
          "*(const uint4*)(&%s[(_tn + _n) * %lluu + (%s) + _k8]);\n",          \
          BO, b_ld, b_name, (unsigned long long)b_nstride, K0);               \
        IND(depth + 1); fputs("}\n", fp);                                      \
      } else if (b_trans) {                                                    \
        IND(depth + 1); fprintf(fp,                                            \
          "for (uint _j = _lid; _j < %uu; _j += %uu) {\n", b_elems, nthreads); \
        IND(depth + 2); fprintf(fp,                                            \
          "uint _n = _j / %uu, _k = _j %% %uu;\n", t.kb, t.kb);                \
        IND(depth + 2); fprintf(fp,                                            \
          "_Bsm[(%s) + _n * %uu + _k] = %s[(_tn + _n) * %lluu + (%s) + _k];\n",\
          BO, b_ld, b_name, (unsigned long long)b_nstride, K0);               \
        IND(depth + 1); fputs("}\n", fp);                                      \
      } else if (b_vec) {                                                      \
        IND(depth + 1); fprintf(fp,                                            \
          "for (uint _i = _lid; _i < %uu; _i += %uu) {\n", b_elems/8u, nthreads);\
        IND(depth + 2); fprintf(fp,                                            \
          "uint _r = _i / %uu, _c = (_i %% %uu) * 8u;\n", tile_n/8u, tile_n/8u);\
        IND(depth + 2); fprintf(fp,                                            \
          "*(uint4*)(&_Bsm[(%s) + _r * %uu + _c]) = "                          \
          "*(const uint4*)(&%s[((%s) + _r) * %uu + _tn + _c]);\n",             \
          BO, b_ld, b_name, K0, n_extent);                                     \
        IND(depth + 1); fputs("}\n", fp);                                      \
      } else {                                                                 \
        IND(depth + 1); fprintf(fp,                                            \
          "for (uint _i = _lid; _i < %uu; _i += %uu) {\n", b_elems, nthreads); \
        IND(depth + 2); fprintf(fp,                                            \
          "_Bsm[(%s) + (_i / %uu) * %uu + _i %% %uu] = "                       \
          "%s[((%s) + _i / %uu) * %uu + _tn + _i %% %uu];\n",                  \
          BO, tile_n, b_ld, tile_n, b_name, K0, tile_n, n_extent, tile_n);    \
        IND(depth + 1); fputs("}\n", fp);                                      \
      }                                                                        \
    } while (0)
  // Inner MMA over a staged buffer half at element offsets (ACO, BCO).
  #define EMIT_MMA(ACO, BCO)                                                   \
    do {                                                                       \
      IND(depth + 1); fprintf(fp, "for (uint _kk = 0u; _kk < %uu; _kk += 16u) {\n", t.kb);\
      IND(depth + 2); fprintf(fp,                                             \
        "wmma::fragment<wmma::matrix_a, 16, 16, 16, %s, wmma::row_major> _af[%u];\n", a_elem, t.rm);\
      for (u32 mi = 0; mi < t.rm; mi++) {                                      \
        IND(depth + 2); fprintf(fp,                                           \
          "wmma::load_matrix_sync(_af[%u], &_Asm[(%s) + (_sm + %uu) * %uu + _kk], %uu);\n",\
          mi, ACO, mi * WM, a_ld, a_ld);                                      \
      }                                                                        \
      IND(depth + 2); fprintf(fp,                                             \
        "wmma::fragment<wmma::matrix_b, 16, 16, 16, %s, %s> _bf[%u];\n",      \
        b_elem, b_trans ? "wmma::col_major" : "wmma::row_major", t.rn);       \
      for (u32 ni = 0; ni < t.rn; ni++) {                                      \
        if (b_trans) {                                                         \
          IND(depth + 2); fprintf(fp,                                         \
            "wmma::load_matrix_sync(_bf[%u], &_Bsm[(%s) + (_sn + %uu) * %uu + _kk], %uu);\n",\
            ni, BCO, ni * WN, b_ld, b_ld);                                    \
        } else {                                                               \
          IND(depth + 2); fprintf(fp,                                         \
            "wmma::load_matrix_sync(_bf[%u], &_Bsm[(%s) + _kk * %uu + _sn + %uu], %uu);\n",\
            ni, BCO, b_ld, ni * WN, b_ld);                                    \
        }                                                                      \
      }                                                                        \
      for (u32 mi = 0; mi < t.rm; mi++)                                        \
        for (u32 ni = 0; ni < t.rn; ni++) {                                    \
          IND(depth + 2); fprintf(fp,                                         \
            "wmma::mma_sync(_acc[%u], _af[%u], _bf[%u], _acc[%u]);\n",        \
            mi * t.rn + ni, mi, ni, mi * t.rn + ni);                          \
        }                                                                      \
      IND(depth + 1); fputs("}\n", fp);                                       \
    } while (0)

  if (!dbuf) {
    // Single-buffered: stage block, barrier, MMA, barrier.
    IND(depth); fprintf(fp, "for (uint _k0 = 0u; _k0 < %uu; _k0 += %uu) {\n",
                        k_extent, t.kb);
    EMIT_LOAD_A("0u", "_k0");
    EMIT_LOAD_B("0u", "_k0");
    IND(depth + 1); fputs("__syncthreads();\n", fp);
    EMIT_MMA("0u", "0u");
    IND(depth + 1); fputs("__syncthreads();\n", fp);
    IND(depth); fputs("}\n", fp);
  } else {
    // Double-buffered software pipeline.  Prologue: stage block 0 into half
    // 0.  Loop: issue the load of block _kit+1 into the OTHER half (overlaps
    // the current MMA), MMA the current half, single barrier guards both.
    EMIT_LOAD_A("0u", "0u");
    EMIT_LOAD_B("0u", "0u");
    IND(depth); fputs("__syncthreads();\n", fp);
    IND(depth); fprintf(fp, "for (uint _kit = 0u; _kit < %uu; _kit++) {\n", n_kblk);
    IND(depth + 1); fputs("uint _cur = _kit & 1u;\n", fp);
    IND(depth + 1); fprintf(fp, "uint _aco = _cur * %uu, _bco = _cur * %uu;\n",
                            a_half, b_half);
    IND(depth + 1); fprintf(fp, "if (_kit + 1u < %uu) {\n", n_kblk);
    {
      char nbo_a[48], nbo_b[48], nk0[48];
      snprintf(nbo_a, sizeof(nbo_a), "(_cur ^ 1u) * %uu", a_half);
      snprintf(nbo_b, sizeof(nbo_b), "(_cur ^ 1u) * %uu", b_half);
      snprintf(nk0, sizeof(nk0), "(_kit + 1u) * %uu", t.kb);
      EMIT_LOAD_A(nbo_a, nk0);
      EMIT_LOAD_B(nbo_b, nk0);
    }
    IND(depth + 1); fputs("}\n", fp);
    EMIT_MMA("_aco", "_bco");
    IND(depth + 1); fputs("__syncthreads();\n", fp);
    IND(depth); fputs("}\n", fp);
  }
  #undef EMIT_LOAD_A
  #undef EMIT_LOAD_B
  #undef EMIT_MMA
  // Store the register tile to C (row-major, ld = N).
  if (c_direct) {
    for (u32 mi = 0; mi < t.rm; mi++) {
      for (u32 ni = 0; ni < t.rn; ni++) {
        IND(depth); fprintf(fp,
          "wmma::store_matrix_sync(&%s[(_tm + _sm + %uu) * %uu + _tn + _sn + %uu], "
          "_acc[%u], %uu, wmma::mem_row_major);\n",
          c_name, mi * WM, n_extent, ni * WN, mi * t.rn + ni, n_extent);
      }
    }
  } else {
    // bf16/fp16 output: store_matrix_sync has no f32->bf16 widening, so each
    // warp stores its f32 accumulator fragment to a per-warp 256-float shared
    // scratch, then its 32 lanes convert+write into C.  Dedicated scratch
    // sized to this kernel's exact warp count (nwarps * 256 floats); the tile
    // picker's SMEM budget (rmu_tc_pick_tile_cuda) reserves it when the output
    // is bf16/fp16 so staging + scratch stay under the cap.  When the kernel
    // uses dynamic shared (use_dyn), the scratch aliases the tail of the one
    // extern block, just past the A+B staging (16-byte-aligned base).
    if (use_dyn) {
      IND(depth); fprintf(fp, "float* _cscratch = (float*)(_dynsm + %uu);\n", ab_bytes);
    } else {
      IND(depth); fprintf(fp, "__shared__ float _cscratch[%u];\n", nwarps * 256u);
    }
    for (u32 mi = 0; mi < t.rm; mi++) {
      for (u32 ni = 0; ni < t.rn; ni++) {
        IND(depth); fprintf(fp,
          "wmma::store_matrix_sync(&_cscratch[sgi * 256u], _acc[%u], 16u, "
          "wmma::mem_row_major);\n", mi * t.rn + ni);
        IND(depth); fputs("__syncwarp();\n", fp);
        IND(depth); fprintf(fp,
          "{ uint _cb = (_tm + _sm + %uu) * %uu + _tn + _sn + %uu;\n",
          mi * WM, n_extent, ni * WN);
        IND(depth); fprintf(fp,
          "  for (uint _e = threadIdx.x %% 32u; _e < 256u; _e += 32u) "
          "%s[_cb + (_e / 16u) * %uu + (_e %% 16u)] = "
          "(%s)_cscratch[sgi * 256u + _e]; }\n",
          c_name, n_extent, rmu_cuda_type_name(dt_c));
        IND(depth); fputs("__syncwarp();\n", fp);
      }
    }
  }
  #undef IND
}

// Emit the threadgroup-staged, register-blocked tiled matmul body for
// the m_par && n_par (both-GLOBAL) case.  Buffer names + leading dims
// are passed in; the address layout is the canonical row-major matmul
// (A[m*K+k], B[k*N+n], C[m*N+n]).  `depth` is the base indent.
static void rmu_emit_matmul_tc_tiled(const char *a_name, const char *b_name,
                                     const char *c_name,
                                     u32 n_extent, u32 k_extent,
                                     RmuTcTile t, int c_is_bf, int stage_bf,
                                     int b_trans, i64 b_nstride,
                                     FILE *fp, u32 depth, int b_is_int8,
                                     Term a_val, u32 a_m_axis, u32 a_k_axis,
                                     Term epilogue_value, Term opt_tc_term,
                                     Term m_range_term, Term n_range_term,
                                     u32 batch_extent, u32 m_extent, u32 a_lead,
                                     u32 batch_stride_a, u32 batch_stride_b,
                                     u32 batch_stride_c) {
  // THVM_FUSE_MATMUL_INPUT: a_val != 0 means A is a fused elementwise producer.
  // The A-staging then computes a_val inline at (m = _tm + row, k = KK0 + col)
  // -- declaring `uint a<m>` / `uint a<k>` in scope so rmu_emit_term renders the
  // producer's RANGE leaves (aM/aK) + INDEX_E reads at that element.  a_val == 0
  // is the default flat-buffer A-load (byte-identical codegen).
  int a_fused = (a_val != 0);
  // Staging + fragment element type.  When BOTH inputs are bf16 the tiles are
  // staged as bfloat and the simdgroup_matrix fragments are <bfloat>, so the
  // MMA runs at the native bf16 tensor-core rate (2x float) accumulating into
  // an f32 register tile (Metal allows bfloat-operand / float-accumulate, as
  // the parallel_tc path does).  Otherwise stage as float (a lone bf16 operand
  // widens on the cooperative load).  Halving the threadgroup-staging bytes
  // also eases shared-memory pressure.
  const char *sel = stage_bf ? "bfloat" : "float";
  u32 tile_m = t.local_m * t.rm * 8u;     // output rows per threadgroup
  u32 tile_n = t.local_n * t.rn * 8u;     // output cols per threadgroup
  u32 n_tiles_n = n_extent / tile_n;      // tg columns in the grid
  u32 nthreads = t.local_m * t.local_n * 32u;
  u32 nsg = t.local_m * t.local_n;        // simdgroups per threadgroup
  #define IND(D) for (u32 _i = 0; _i < (D); _i++) fputs("  ", fp)
  IND(depth); fprintf(fp,
    "/* TC tiled matmul: tile %ux%u, %ux%u simdgroups, %ux%u reg, KB=%u */\n",
    tile_m, tile_n, t.local_m, t.local_n, t.rm, t.rn, t.kb);
  // Double-buffered (software-pipelined) staging.  Two threadgroup A/B
  // buffers ping-pong: while the MMA consumes K-block N from one buffer the
  // cooperative global->threadgroup load of block N+1 streams into the other,
  // hiding the global-load latency behind compute instead of stalling at a
  // barrier between every load and MMA.  Asm: [2][TILE_M x KB] row-major
  // (ld=KB).  Bsm: [2][KB x TILE_N] row-major (ld=TILE_N).
  IND(depth); fprintf(fp,
    "threadgroup %s _Asm[%u];\n", sel, 2u * tile_m * t.kb);
  IND(depth); fprintf(fp,
    "threadgroup %s _Bsm[%u];\n", sel, 2u * t.kb * tile_n);
  // bf16 output: a per-simdgroup f32 scratch slot for the f32->bf16 staged
  // store (simdgroup_store has no f32->bf16 widening, and Metal's
  // simdgroup_matrix has no f32->bf16 element-conversion constructor either).
  // Each of the NSG simdgroups owns a private 64-float slot, reused across its
  // register tiles.
  if (c_is_bf) {
    IND(depth); fprintf(fp, "threadgroup float _cstage[%u];\n", nsg * 64u);
    IND(depth); fputs("uint _sgi64 = sgi * 64u;\n", fp);
  }
  // This threadgroup's output-tile origin.
  // THVM_TC_BATCHED: a batched matmul folds the batch axis into the grid as
  // batch * m_tiles * n_tiles threadgroups.  Decode _batch + the per-batch tg
  // and offset A/B/C to this batch's contiguous slice (strides M*K / K*N / M*N);
  // `auto` gives the base pointers each buffer's own element type.  Then the rest
  // of the emitter is byte-unchanged -- it just reads from _abb/_bbb/_cbb.
  const char *_tgv = "tg";
  if (batch_extent > 0u) {
    u32 _mn = (m_extent / tile_m) * n_tiles_n;
    IND(depth); fprintf(fp, "uint _batch = tg / %uu; uint _tg = tg %% %uu;\n", _mn, _mn);
    IND(depth); fprintf(fp, "auto _abb = %s + _batch * %uu;\n", a_name, batch_stride_a);
    IND(depth); fprintf(fp, "auto _bbb = %s + _batch * %uu;\n", b_name, batch_stride_b);
    IND(depth); fprintf(fp, "auto _cbb = %s + _batch * %uu;\n", c_name, batch_stride_c);
    a_name = "_abb"; b_name = "_bbb"; c_name = "_cbb";
    _tgv = "_tg";
  }
  IND(depth); fprintf(fp, "uint _tm = (%s / %uu) * %uu;\n", _tgv, n_tiles_n, tile_m);
  IND(depth); fprintf(fp, "uint _tn = (%s %% %uu) * %uu;\n", _tgv, n_tiles_n, tile_n);
  // This simdgroup's sub-tile origin within the threadgroup tile.
  IND(depth); fprintf(fp, "uint _sm = (sgi / %uu) * %uu;\n",
                      t.local_n, t.rm * 8u);
  IND(depth); fprintf(fp, "uint _sn = (sgi %% %uu) * %uu;\n",
                      t.local_n, t.rn * 8u);
  // Flat thread index within the threadgroup, for cooperative staging.
  IND(depth); fputs("uint _lid = sgi * 32u + thread_index_in_simdgroup;\n", fp);
  // Register accumulator tile, all zero-initialised.
  IND(depth); fprintf(fp,
    "simdgroup_matrix<float, 8, 8> _acc[%u];\n", t.rm * t.rn);
  IND(depth); fprintf(fp,
    "for (uint _i = 0u; _i < %uu; _i++) "
    "_acc[_i] = simdgroup_matrix<float, 8, 8>(0);\n", t.rm * t.rn);
  // Vectorized cooperative loads (float4/bfloat4) along the contiguous inner
  // axis: A's KB run (unit-stride K) and B's TILE_N run (unit-stride N).  Each
  // valid when the inner extent and source leading dim are multiples of 4 (the
  // canonical packed FLUX shapes always are; scalar fallback otherwise).  Cuts
  // the staging-loop instruction count ~4x and saturates memory bandwidth.
  int a_vec = (t.kb % 4u == 0) && (k_extent % 4u == 0);
  int b_vec = !b_trans && (tile_n % 4u == 0) && (n_extent % 4u == 0);
  // A transposed B (B = Transpose[W], W {N,K} contiguous: B[k][n] = W[n][k])
  // can't vectorize its N-run (strided by b_nstride), but ITS K-run is W's
  // contiguous axis -- so read 4 consecutive K as a vector (coalesced) and
  // scatter to the strided threadgroup slots.  Needs kb and the W row stride
  // 4-aligned; else a scalar coalesced gather.
  int b_tvec = b_trans && (t.kb % 4u == 0) && (b_nstride % 4 == 0);
  u32 a_elems = tile_m * t.kb, b_elems = t.kb * tile_n;
  // Emit-helper macros for the cooperative load of one K-block.  BUFOFF is the
  // MSL expression selecting the destination buffer half (an element offset
  // into _Asm / _Bsm); KK0 is the MSL expression for the source K-origin.  Each
  // emits a vectorized (bfloat4/float4) loop when the inner axis is 4-aligned,
  // else a scalar loop.
  #define EMIT_LOAD_A(BUFOFF, KK0)                                            \
    do {                                                                      \
      if (a_fused) {                                                          \
        /* THVM_FUSE_MATMUL_INPUT: compute the elementwise producer inline at \
           (m = _tm + _r, k = KK0 + _c).  Scalar coalesced loop -- the         \
           producer compute (not the load) dominates, so dropping the float4   \
           A-vectorization is a non-issue; B stays vectorized + the MMA is      \
           unchanged. */                                                       \
        IND(depth + 1); fprintf(fp,                                           \
          "for (uint _i = _lid; _i < %uu; _i += %uu) {\n", a_elems, nthreads);\
        IND(depth + 2); fprintf(fp,                                           \
          "uint a%u = _tm + _i / %uu; uint a%u = (%s) + _i %% %uu;\n",        \
          a_m_axis, t.kb, a_k_axis, KK0, t.kb);                              \
        IND(depth + 2); fprintf(fp, "(_Asm + %s)[_i] = (%s)(", BUFOFF, sel); \
        rmu_emit_term(a_val, fp);                                             \
        fputs(");\n", fp);                                                    \
        IND(depth + 1); fputs("}\n", fp);                                     \
      } else if (a_vec) {                                                     \
        IND(depth + 1); fprintf(fp,                                           \
          "for (uint _i = _lid; _i < %uu; _i += %uu) {\n",                    \
          a_elems / 4u, nthreads);                                           \
        IND(depth + 2); fprintf(fp,                                           \
          "uint _r = _i / %uu, _c = (_i %% %uu) * 4u;\n",                     \
          t.kb / 4u, t.kb / 4u);                                            \
        IND(depth + 2); fprintf(fp,                                           \
          "((threadgroup %s4*)(_Asm + %s))[_i] = "                            \
          "*(device const %s4*)(%s + (_tm + _r) * %uu + (%s) + _c);\n",       \
          sel, BUFOFF, sel, a_name, a_lead, KK0);                            \
        IND(depth + 1); fputs("}\n", fp);                                     \
      } else {                                                                \
        IND(depth + 1); fprintf(fp,                                           \
          "for (uint _i = _lid; _i < %uu; _i += %uu) {\n", a_elems, nthreads);\
        IND(depth + 2); fprintf(fp,                                           \
          "(_Asm + %s)[_i] = %s[(_tm + _i / %uu) * %uu + (%s) + _i %% %uu];\n",\
          BUFOFF, a_name, t.kb, a_lead, KK0, t.kb);                          \
        IND(depth + 1); fputs("}\n", fp);                                     \
      }                                                                       \
    } while (0)
  #define EMIT_LOAD_B(BUFOFF, KK0)                                            \
    do {                                                                      \
      if (b_is_int8 && b_tvec) {                                              \
        /* q8: B = Transpose[Cast[W_int8, bf16]].  Read 4 consecutive K of    \
           the INT8 weight (W's contiguous axis) as a char4 (coalesced load,  \
           half the bytes of bfloat4), cast char -> sel, scatter to the       \
           strided _Bsm slots.  b_nstride is in ELEMENTS (= int8 bytes, same  \
           logical count as the bf16 view), so the stride is unchanged. */     \
        IND(depth + 1); fprintf(fp,                                           \
          "for (uint _j = _lid; _j < %uu; _j += %uu) {\n", b_elems / 4u, nthreads);\
        IND(depth + 2); fprintf(fp,                                           \
          "uint _n = _j / %uu, _k4 = (_j %% %uu) * 4u;\n", t.kb / 4u, t.kb / 4u);\
        IND(depth + 2); fprintf(fp,                                           \
          "char4 _bv = *(device const char4*)(%s + (_tn + _n) * %lluu + (%s) + _k4);\n",\
          b_name, (unsigned long long)b_nstride, KK0);                        \
        IND(depth + 2); fprintf(fp,                                           \
          "(_Bsm + %s)[_k4 * %uu + _n] = (%s)_bv.x;\n", BUFOFF, tile_n, sel); \
        IND(depth + 2); fprintf(fp,                                           \
          "(_Bsm + %s)[(_k4 + 1u) * %uu + _n] = (%s)_bv.y;\n", BUFOFF, tile_n, sel);\
        IND(depth + 2); fprintf(fp,                                           \
          "(_Bsm + %s)[(_k4 + 2u) * %uu + _n] = (%s)_bv.z;\n", BUFOFF, tile_n, sel);\
        IND(depth + 2); fprintf(fp,                                           \
          "(_Bsm + %s)[(_k4 + 3u) * %uu + _n] = (%s)_bv.w;\n", BUFOFF, tile_n, sel);\
        IND(depth + 1); fputs("}\n", fp);                                     \
      } else if (b_is_int8) {                                                 \
        /* q8 scalar coalesced fallback (kb or W stride not 4-aligned, OR a   \
           row-major B): read each INT8 weight element, cast char -> sel. */   \
        IND(depth + 1); fprintf(fp,                                           \
          "for (uint _j = _lid; _j < %uu; _j += %uu) {\n", b_elems, nthreads);\
        if (b_trans) {                                                        \
          IND(depth + 2); fprintf(fp,                                         \
            "uint _n = _j / %uu, _k = _j %% %uu;\n", t.kb, t.kb);             \
          IND(depth + 2); fprintf(fp,                                         \
            "(_Bsm + %s)[_k * %uu + _n] = (%s)%s[(_tn + _n) * %lluu + (%s) + _k];\n",\
            BUFOFF, tile_n, sel, b_name, (unsigned long long)b_nstride, KK0); \
        } else {                                                             \
          IND(depth + 2); fprintf(fp,                                         \
            "(_Bsm + %s)[_j] = (%s)%s[((%s) + _j / %uu) * %uu + _tn + _j %% %uu];\n",\
            BUFOFF, sel, b_name, KK0, tile_n, n_extent, tile_n);             \
        }                                                                     \
        IND(depth + 1); fputs("}\n", fp);                                     \
      } else if (b_tvec) {                                                    \
        /* B[k][n] = W[n][k]: read 4 consecutive K (W's contiguous axis) as a  \
           vector (coalesced global load), scatter to the strided _Bsm slots.  \
           _j -> (_n, _k4); stage _Bsm[(_k4+t)*tile_n + _n]. */                \
        IND(depth + 1); fprintf(fp,                                           \
          "for (uint _j = _lid; _j < %uu; _j += %uu) {\n", b_elems / 4u, nthreads);\
        IND(depth + 2); fprintf(fp,                                           \
          "uint _n = _j / %uu, _k4 = (_j %% %uu) * 4u;\n", t.kb / 4u, t.kb / 4u);\
        IND(depth + 2); fprintf(fp,                                           \
          "%s4 _bv = *(device const %s4*)(%s + (_tn + _n) * %lluu + (%s) + _k4);\n",\
          sel, sel, b_name, (unsigned long long)b_nstride, KK0);              \
        IND(depth + 2); fprintf(fp,                                           \
          "(_Bsm + %s)[_k4 * %uu + _n] = _bv.x;\n", BUFOFF, tile_n);          \
        IND(depth + 2); fprintf(fp,                                           \
          "(_Bsm + %s)[(_k4 + 1u) * %uu + _n] = _bv.y;\n", BUFOFF, tile_n);   \
        IND(depth + 2); fprintf(fp,                                           \
          "(_Bsm + %s)[(_k4 + 2u) * %uu + _n] = _bv.z;\n", BUFOFF, tile_n);   \
        IND(depth + 2); fprintf(fp,                                           \
          "(_Bsm + %s)[(_k4 + 3u) * %uu + _n] = _bv.w;\n", BUFOFF, tile_n);   \
        IND(depth + 1); fputs("}\n", fp);                                     \
      } else if (b_trans) {                                                   \
        /* scalar coalesced fallback (kb or W stride not 4-aligned). */        \
        IND(depth + 1); fprintf(fp,                                           \
          "for (uint _j = _lid; _j < %uu; _j += %uu) {\n", b_elems, nthreads);\
        IND(depth + 2); fprintf(fp,                                           \
          "uint _n = _j / %uu, _k = _j %% %uu;\n", t.kb, t.kb);               \
        IND(depth + 2); fprintf(fp,                                           \
          "(_Bsm + %s)[_k * %uu + _n] = %s[(_tn + _n) * %lluu + (%s) + _k];\n",\
          BUFOFF, tile_n, b_name, (unsigned long long)b_nstride, KK0);        \
        IND(depth + 1); fputs("}\n", fp);                                     \
      } else if (b_vec) {                                                     \
        IND(depth + 1); fprintf(fp,                                           \
          "for (uint _i = _lid; _i < %uu; _i += %uu) {\n",                    \
          b_elems / 4u, nthreads);                                           \
        IND(depth + 2); fprintf(fp,                                           \
          "uint _r = _i / %uu, _c = (_i %% %uu) * 4u;\n",                     \
          tile_n / 4u, tile_n / 4u);                                         \
        IND(depth + 2); fprintf(fp,                                           \
          "((threadgroup %s4*)(_Bsm + %s))[_i] = "                            \
          "*(device const %s4*)(%s + ((%s) + _r) * %uu + _tn + _c);\n",       \
          sel, BUFOFF, sel, b_name, KK0, (u32)b_nstride);                    \
        IND(depth + 1); fputs("}\n", fp);                                     \
      } else {                                                                \
        IND(depth + 1); fprintf(fp,                                           \
          "for (uint _i = _lid; _i < %uu; _i += %uu) {\n", b_elems, nthreads);\
        IND(depth + 2); fprintf(fp,                                           \
          "(_Bsm + %s)[_i] = %s[((%s) + _i / %uu) * %uu + _tn + _i %% %uu];\n",\
          BUFOFF, b_name, KK0, tile_n, (u32)b_nstride, tile_n);              \
        IND(depth + 1); fputs("}\n", fp);                                     \
      }                                                                       \
    } while (0)
  // Prologue: stage K-block 0 into buffer half 0.
  EMIT_LOAD_A("0u", "0u");
  EMIT_LOAD_B("0u", "0u");
  IND(depth); fputs("threadgroup_barrier(mem_flags::mem_threadgroup);\n", fp);
  // Software-pipelined K-block loop.  _kit = block index; the current buffer
  // half is (_kit&1), the next is the complement.  Each iteration first issues
  // the global load of block _kit+1 into the OTHER buffer half (independent of
  // the current MMA, so the load latency overlaps compute), then runs the MMA
  // on the current half, then a single barrier guards both the next-half loads
  // (consumed next iteration) and the current-half compute (before that half is
  // overwritten two iterations later).
  IND(depth); fprintf(fp, "for (uint _kit = 0u; _kit < %uu; _kit++) {\n",
                      k_extent / t.kb);
  IND(depth + 1); fputs("uint _cur = (_kit & 1u);\n", fp);
  IND(depth + 1); fprintf(fp, "uint _aco = _cur * %uu, _bco = _cur * %uu;\n",
                          tile_m * t.kb, t.kb * tile_n);
  IND(depth + 1); fprintf(fp,
    "if (_kit + 1u < %uu) {\n", k_extent / t.kb);
  {
    // Next buffer half + next K-origin.
    char nbo_a[64], nbo_b[64], nk0[64];
    snprintf(nbo_a, sizeof(nbo_a), "((_cur ^ 1u) * %uu)", tile_m * t.kb);
    snprintf(nbo_b, sizeof(nbo_b), "((_cur ^ 1u) * %uu)", t.kb * tile_n);
    snprintf(nk0, sizeof(nk0), "((_kit + 1u) * %uu)", t.kb);
    EMIT_LOAD_A(nbo_a, nk0);
    EMIT_LOAD_B(nbo_b, nk0);
  }
  IND(depth + 1); fputs("}\n", fp);
  #undef EMIT_LOAD_A
  #undef EMIT_LOAD_B
  // Inner K-subtile loop over the staged current block (steps of 8).
  IND(depth + 1); fprintf(fp, "for (uint _kk = 0u; _kk < %uu; _kk += 8u) {\n",
                          t.kb);
  // Load this simdgroup's A fragments (RM of them) from threadgroup mem.
  IND(depth + 2); fprintf(fp,
    "simdgroup_matrix<%s, 8, 8> _af[%u];\n", sel, t.rm);
  for (u32 mi = 0; mi < t.rm; mi++) {
    IND(depth + 2); fprintf(fp,
      "simdgroup_load(_af[%u], &_Asm[_aco + (_sm + %uu) * %uu + _kk], %uu);\n",
      mi, mi * 8u, t.kb, t.kb);
  }
  // Load this simdgroup's B fragments (RN of them) from threadgroup mem.
  IND(depth + 2); fprintf(fp,
    "simdgroup_matrix<%s, 8, 8> _bf[%u];\n", sel, t.rn);
  for (u32 ni = 0; ni < t.rn; ni++) {
    IND(depth + 2); fprintf(fp,
      "simdgroup_load(_bf[%u], &_Bsm[_bco + _kk * %uu + _sn + %uu], %uu);\n",
      ni, tile_n, ni * 8u, tile_n);
  }
  // Register-blocked MMA: every A fragment x every B fragment.
  for (u32 mi = 0; mi < t.rm; mi++) {
    for (u32 ni = 0; ni < t.rn; ni++) {
      IND(depth + 2); fprintf(fp,
        "simdgroup_multiply_accumulate(_acc[%u], _af[%u], _bf[%u], _acc[%u]);\n",
        mi * t.rn + ni, mi, ni, mi * t.rn + ni);
    }
  }
  IND(depth + 1); fputs("}\n", fp);                          // close _kk
  IND(depth + 1); fputs("threadgroup_barrier(mem_flags::mem_threadgroup);\n", fp);
  IND(depth); fputs("}\n", fp);                              // close _kit
  // Store the register tile to the output (row-major, ld = N).  f32 output:
  // simdgroup_store directly.  bf16 output: simdgroup_store has no f32->bf16
  // widening, so stage each 8x8 tile through this simdgroup's private f32 slot
  // (_cstage[sgi*64]) then cooperatively convert+write to bfloat; barriers
  // bracket the store/read and guard slot reuse across the rm*rn tiles.
  for (u32 mi = 0; mi < t.rm; mi++) {
    for (u32 ni = 0; ni < t.rn; ni++) {
      if (c_is_bf) {
        IND(depth); fprintf(fp,
          "simdgroup_store(_acc[%u], &_cstage[_sgi64], 8);\n", mi * t.rn + ni);
        IND(depth); fputs("simdgroup_barrier(mem_flags::mem_threadgroup);\n", fp);
        IND(depth); fprintf(fp,
          "{ uint _cb = (_tm + _sm + %uu) * %uu + _tn + _sn + %uu;\n",
          mi * 8u, n_extent, ni * 8u);
        if (epilogue_value != 0) {
          // Apply the elementwise epilogue per output element.  The matmul value
          // is substituted at the SAME dtype its un-fused realized output buffer
          // would carry, so the fused result is bit-identical to the separate-
          // kernel path: an f32-accumulating matmul (bf16 inputs -> f32 reduce)
          // realizes an f32 buffer, so the epilogue reads the full-precision f32
          // accumulator (_cstage is `threadgroup float`); a bf16-output matmul
          // rounds to bf16 at its store, so the epilogue rounds `(bfloat)` first.
          // _m/_n substitute the M/N range vars in the epilogue's gate[n] /
          // X[m,n] / activation index reads.
          u32 mm_dt = DT_FP32;
          (void)term_dtype_in(opt_tc_term, 0, &mm_dt);
          char const *mm_read = (mm_dt == DT_BF16)
                              ? "((bfloat)_cstage[_sgi64 + _e])"
                              : (mm_dt == DT_FP16)
                              ? "((half)_cstage[_sgi64 + _e])"
                              : "(_cstage[_sgi64 + _e])";
          IND(depth); fputs(
            "  for (uint _e = thread_index_in_simdgroup; _e < 64u; _e += 32u) {\n", fp);
          IND(depth); fprintf(fp,
            "    uint _m = _tm + _sm + %uu + _e / 8u; uint _n = _tn + _sn + %uu + _e %% 8u;"
            " (void)_m; (void)_n;\n", mi * 8u, ni * 8u);
          IND(depth); fprintf(fp,
            "    %s[_cb + (_e / 8u) * %uu + (_e %% 8u)] = (bfloat)(", c_name, n_extent);
          rmu_hoist_clear();
          rmu_hoist_add(opt_tc_term,  mm_read);
          rmu_hoist_add(m_range_term, "_m");
          rmu_hoist_add(n_range_term, "_n");
          rmu_emit_term(epilogue_value, fp);
          rmu_hoist_clear();
          fputs("); }\n", fp);
          IND(depth); fputs("}\n", fp);
        } else {
          IND(depth); fprintf(fp,
            "  for (uint _e = thread_index_in_simdgroup; _e < 64u; _e += 32u) "
            "%s[_cb + (_e / 8u) * %uu + (_e %% 8u)] = (bfloat)_cstage[_sgi64 + _e]; }\n",
            c_name, n_extent);
        }
        IND(depth); fputs("simdgroup_barrier(mem_flags::mem_threadgroup);\n", fp);
      } else {
        IND(depth); fprintf(fp,
          "simdgroup_store(_acc[%u], &%s[(_tm + _sm + %uu) * %uu + _tn + _sn + %uu], %uu);\n",
          mi * t.rn + ni, c_name, mi * 8u, n_extent, ni * 8u, n_extent);
      }
    }
  }
  #undef IND
}

// Specialised simdgroup_matrix MSL template for the matmul pattern.
// Called when rmu_detect_matmul_tc fires.  Emits an 8x8
// simdgroup_matrix-tiled K-loop: load A and B subblocks, multiply-
// accumulate into C, store final C.  Falls back to the generic
// accumulator path when the address shapes don't yield clean
// ptr+offset (e.g. non-contiguous strides).
static int rmu_emit_matmul_tc(Term store, Term tc_red, FILE *fp,
                              u32 depth) {
  // Extract inner pieces validated by rmu_detect_matmul_tc.
  u64 sloc = term_val(store);
  Term addr_c = heap_read(sloc + 1);
  Term buf_c  = heap_read(sloc + 0);
  // THVM_FUSE_MATMUL_EPILOGUE: if the store value is an elementwise CHAIN over
  // the OPT_TC (not the bare matmul), `epilogue_value` is that chain and
  // `opt_tc_term` the matmul node inside it.  The epilogue is applied per output
  // element at the bf16 staging store; the matmul stays on the TC path.  0 = no
  // epilogue (the default / flag-off path; byte-identical codegen).
  Term store_value   = heap_read(sloc + 2);
  Term epilogue_value = 0, opt_tc_term = 0;
  // tc_opt_node is the OPT_TC term itself (in BOTH the bare and the
  // epilogue case) -- its `factor` carries the autotune-chosen tile
  // config (tc_tile_pack), read below where the picker runs.
  Term tc_opt_node = store_value;
  if (!(term_tag(store_value) == TAG_UOP && term_ext(store_value) == UOP_OPT
        && uop_opt_kind(store_value) == UOP_OPT_TC)) {
    if (!rmu_epilogue_find_single_tc(store_value, &opt_tc_term)) return 0;
    epilogue_value = store_value;
    tc_opt_node = opt_tc_term;
  }
  u32 tc_opt_factor = uop_opt_factor(tc_opt_node);
  // TC matmul shape assumes a single reduce axis (K).  Multi-axis
  // REDUCE inputs would need a separate dispatch path.
  if (uop_reduce_n_axes(tc_red) != 1) return 0;
  u32 red_axis  = uop_reduce_axis(tc_red, 0);
  Term mul      = uop_reduce_src(tc_red);
  // Peel a bf16->f32 CAST off each operand (see rmu_peel_cast): the
  // simdgroup template loads bf16 buffers into bfloat fragments directly.
  Term lhs      = rmu_peel_cast(heap_read(term_val(mul) + 0));
  Term rhs      = rmu_peel_cast(heap_read(term_val(mul) + 1));
  // THVM_FUSE_MATMUL_INPUT: when A is a fused elementwise producer, `a_val` is
  // the value tree the A-staging load computes inline; `lhs` becomes a
  // REPRESENTATIVE INDEX_E leaf (one of the producer's reads) so the M/K/lda
  // layout discovery below sees a real {m,k} address.  a_val == 0 means A is a
  // plain buffer read (the default; byte-identical codegen).
  Term a_val    = 0;
  if (!(term_tag(lhs) == TAG_UOP && term_ext(lhs) == UOP_INDEX_E)
      && ru_fuse_matmul_input_on()) {
    Term rep = rmu_fused_first_index(lhs, 0);
    if (rep == 0) return 0;
    // Validate the whole producer is inline-safe; n_axis is discovered below,
    // but the producer must not read the matmul's N axis.  We re-check after
    // n_axis is known; here ensure it is at least a pure elementwise tree.
    a_val = lhs;
    lhs   = rep;
  }
  // Fused-A inline is implemented only for the Metal register-blocked tiled
  // emitter (the FLUX target).  On any other target a fused A bails to the
  // generic accumulator (correct, recomputes the producer per element).
  if (a_val != 0 && RMU_TARGET != CG_TARGET_METAL) return 0;
  Term buf_a    = heap_read(term_val(lhs) + 0);
  Term addr_a   = heap_read(term_val(lhs) + 1);
  Term buf_b    = heap_read(term_val(rhs) + 0);
  Term addr_b   = heap_read(term_val(rhs) + 1);
  // q8 weight-only-quantized matmul: the B operand is a CAST(int8 -> bf16)
  // over the INT8 weight buffer (TMatMul[x, Transpose[Cast[w_int8, bf16]]]).
  // rmu_peel_cast stripped the cast, so buf_b is the raw INT8 buffer.  The
  // tiled emitter reads it as `char` and casts char -> bfloat in the
  // cooperative B-staging (half the weight bandwidth, full TC rate); the
  // per-output-channel scale is applied OUTSIDE the matmul by the caller.
  int b_is_int8 = (rmu_slot_dtype(buf_b) == DT_INT8);

  // Find the K-axis extent by scanning addr_a and addr_b for the
  // RANGE leaf with axis_id == red_axis.
  Term ranges[MAX_DIM];
  u32  n_r = 0;
  rmu_collect_ranges(addr_a, ranges, &n_r);
  rmu_collect_ranges(addr_b, ranges, &n_r);
  u32 k_extent = 0;
  for (u32 i = 0; i < n_r; i++) {
    if (term_val(heap_read(term_val(ranges[i]) + 0)) == red_axis) {
      k_extent = term_val(heap_read(term_val(ranges[i]) + 2));
      break;
    }
  }
  if (k_extent == 0 || (k_extent % 8) != 0) {
    // Tile size mismatch -- fall back to the generic accumulator path.
    return 0;
  }

  // Batched-gemm detection: a TRUE batched matmul (attention's mhaBmm) has a
  // batch axis present in addr_a AND addr_b (besides K), with M/N each unique
  // to one operand.  Recover it FIRST so the M/N discovery below skips it.
  // The address terms already encode the per-batch base offset
  // (a{batch}*batch_stride), so rmu_emit_term handles the data layout; the
  // only extra work is decoding the batch axis from `tg` and (for dispatch)
  // sizing the grid to batch * m_tiles * n_tiles.  Falls through to the
  // plain 2-D path (batch_axis_id == 0xFFFFFFFF) for non-batched matmuls.
  u32 batch_axis_id = 0xFFFFFFFFu, batch_extent = 0;
  {
    Term ra[MAX_DIM]; u32 ra_n = 0;
    rmu_collect_ranges(addr_a, ra, &ra_n);
    Term rb[MAX_DIM]; u32 rb_n = 0;
    rmu_collect_ranges(addr_b, rb, &rb_n);
    for (u32 i = 0; i < ra_n && batch_axis_id == 0xFFFFFFFFu; i++) {
      u32 aid = term_val(heap_read(term_val(ra[i]) + 0));
      if (aid == red_axis) continue;
      for (u32 j = 0; j < rb_n; j++) {
        if (term_val(heap_read(term_val(rb[j]) + 0)) == aid) {
          batch_axis_id = aid;
          batch_extent = (u32)term_val(heap_read(term_val(ra[i]) + 2));
          break;
        }
      }
    }
  }

  // Identify M-axis (in addr_a, not red, not batch) and N-axis (in addr_b,
  // not red, not batch).  We need each extent + axis_id + axis_type so the
  // outer emission can either open for-loops (LOOP / default) or bind to
  // thread-position (LOCAL / GLOBAL) for parallel multi-SG dispatch.
  u32 m_axis_id = 0xFFFFFFFFu, m_extent = 0, m_axis_type = 0;
  u32 n_axis_id_v = 0xFFFFFFFFu, n_extent = 0, n_axis_type = 0;
  {
    Term ra[MAX_DIM]; u32 ra_n = 0;
    rmu_collect_ranges(addr_a, ra, &ra_n);
    for (u32 i = 0; i < ra_n; i++) {
      u32 aid = term_val(heap_read(term_val(ra[i]) + 0));
      if (aid != red_axis && aid != batch_axis_id) {
        m_axis_id = aid;
        m_axis_type = (u32)term_val(heap_read(term_val(ra[i]) + 1));
        m_extent = (u32)term_val(heap_read(term_val(ra[i]) + 2));
        break;
      }
    }
    Term rb[MAX_DIM]; u32 rb_n = 0;
    rmu_collect_ranges(addr_b, rb, &rb_n);
    for (u32 i = 0; i < rb_n; i++) {
      u32 aid = term_val(heap_read(term_val(rb[i]) + 0));
      if (aid != red_axis && aid != batch_axis_id) {
        n_axis_id_v = aid;
        n_axis_type = (u32)term_val(heap_read(term_val(rb[i]) + 1));
        n_extent = (u32)term_val(heap_read(term_val(rb[i]) + 2));
        break;
      }
    }
  }
  if (m_extent == 0 || n_extent == 0
      || (m_extent % 8) != 0 || (n_extent % 8) != 0
      || m_axis_id == 0xFFFFFFFFu || n_axis_id_v == 0xFFFFFFFFu) {
    // M/N also need to be 8-tiled for simdgroup_matrix<8,8>; bail.
    return 0;
  }
  // Epilogue: locate the M and N RANGE terms (from the output address) so the
  // store can substitute the per-element tiled m/n for them in the epilogue's
  // gate[n] / X[m,n] / activation index reads (rmu_emit_matmul_tc_tiled hoist).
  Term m_range_term = 0, n_range_term = 0;
  if (epilogue_value != 0) {
    Term rcr[MAX_DIM]; u32 rcr_n = 0;
    rmu_collect_ranges(addr_c, rcr, &rcr_n);
    for (u32 i = 0; i < rcr_n; i++) {
      u32 aid = term_val(heap_read(term_val(rcr[i]) + 0));
      if (aid == m_axis_id) m_range_term = rcr[i];
      else if (aid == n_axis_id_v) n_range_term = rcr[i];
    }
    // Need both range handles to remap the epilogue reads; bail if either is
    // missing (falls to the generic accumulator path, which handles the
    // epilogue correctly via the store-value-over-reduce machinery).
    if (m_range_term == 0 || n_range_term == 0) return 0;
  }

  // THVM_FUSE_MATMUL_INPUT: now that the N axis is known, validate the fused A
  // producer is inline-safe (pure elementwise/cast over INDEX_E leaves, no
  // N-axis dependence, no div/mod swizzle).  If not, bail to the generic
  // accumulator (still correct) rather than emit a wrong A-staging load.
  if (a_val != 0 && !rmu_fused_producer_ok(a_val, n_axis_id_v, 0)) return 0;

  // Operand layout from the address strides.  simdgroup_load(dst, ptr, ld)
  // reads dst[i][j] = ptr[i*ld + j], so each matrix needs ONE axis with unit
  // stride (the contiguous inner axis).  A is read as {M,K} and B as {K,N}:
  //   A: K unit-stride -> row-major, ld = M-stride (= k_extent when packed).
  //      M unit-stride -> transposed, ld = K-stride.
  //   B: N unit-stride -> row-major, ld = K-stride (= n_extent when packed).
  //      K unit-stride -> transposed, ld = N-stride  (a Transpose[w] RHS: its
  //      N-axis carries the source row stride, its K-axis carries stride 1).
  // Neither axis unit-stride (a genuinely strided gather) -> bail to scalar.
  i64 a_k = rmu_range_coeff(addr_a, red_axis);
  i64 a_m = rmu_range_coeff(addr_a, m_axis_id);
  i64 b_k = rmu_range_coeff(addr_b, red_axis);
  i64 b_n = rmu_range_coeff(addr_b, n_axis_id_v);
  int a_trans, b_trans; i64 lda, ldb;
  if      (a_k == 1) { a_trans = 0; lda = a_m; }
  else if (a_m == 1) { a_trans = 1; lda = a_k; }
  else return 0;
  if      (b_n == 1) { b_trans = 0; ldb = b_k; }
  else if (b_k == 1) { b_trans = 1; ldb = b_n; }
  else return 0;
  if (lda <= 0 || ldb <= 0) return 0;

  // CUDA WMMA path.  The simdgroup_matrix template below is Metal-
  // only; CUDA gets its own emit.  WMMA's natural fp32-accumulate
  // fragment is 16x16x16, so M/N/K must all be multiples of 16 for the
  // tensor-core template -- otherwise fall back to the generic scalar
  // accumulator (return 0), exactly as Metal falls back from
  // simdgroup_matrix when K%8!=0.  One warp per 16x16 output tile.
  if (RMU_TARGET == CG_TARGET_CUDA) {
    if ((m_extent % 16) != 0 || (n_extent % 16) != 0
        || (k_extent % 16) != 0) {
      // Non-conforming shape: scalar-accumulator fallback.
      return 0;
    }
    // A batched gemm (attention's mhaBmm: batch axis in A, B AND C) goes to
    // the NAIVE one-warp-per-16x16 WMMA path below, which decodes the batch
    // index from the flat grid (`output_numel` = batch*M*N threads, set by the
    // dispatch -- uop_dag_classify_matmul_shape bails on the 3-range operands
    // so cuda_tc_tile_dispatch_shape returns 0 and the flat launch fires) and
    // offsets A/B/C by the per-batch stride already encoded in the addresses.
    // The shared-staged TILED path below stays 2-D only (its tg->tile decode
    // has no batch dimension), so it is gated on batch_axis_id == none.
    // THVM_TC_BATCHED=0 forces batched matmuls back to the scalar accumulator
    // (the pre-batched-WMMA behaviour) -- a diagnostic A/B knob.
    if (batch_axis_id != 0xFFFFFFFFu) {
      char const *e = getenv("THVM_TC_BATCHED");
      if (e && e[0] == '0') return 0;
    }
    // WMMA matrix_a / matrix_b fragments need a 16-bit floating source.
    // fp16 fragments work on every WMMA-capable arch (>= sm_70);
    // __nv_bfloat16 fragments need Ampere (sm>=80).  Pick the fragment
    // element type by dtype and gate bf16 on RMU_CUDA_SM>=80 -- on a
    // pre-Ampere device a bf16 matmul falls back to the scalar tiled
    // accumulator (return 0).  An fp32 A/B matmul also bails: the
    // load_matrix_sync(16-bit-frag, const float*) would be a hard nvrtc
    // type error.  The accumulator fragment is always fp32.
    const char *a_elem, *b_elem;
    {
      u32 dt_a = uop_buffer_dtype(buf_a);
      u32 dt_b = uop_buffer_dtype(buf_b);
      if (dt_a == DT_FP16)      a_elem = "half";
      else if (dt_a == DT_BF16 && RMU_CUDA_SM >= 80) a_elem = "__nv_bfloat16";
      else return 0;
      if (dt_b == DT_FP16)      b_elem = "half";
      else if (dt_b == DT_BF16 && RMU_CUDA_SM >= 80) b_elem = "__nv_bfloat16";
      else return 0;
    }
    u32 dt_c = uop_buffer_dtype(buf_c);
    // Shared-staged, register-blocked tiled WMMA path.  Eligible when:
    //   - A is NOT transposed (the emitter stages A as {m,K} row-major;
    //     a transposed A falls to the naive per-warp path below),
    //   - M and N are BOTH GLOBAL (uop_recognise_tc_parallel stamped them,
    //     so cuda_tc_tile_dispatch_shape launches (M/tile_m)*(N/tile_n)
    //     blocks of nwarps*32 threads -- each block owns a unique output
    //     tile, no write race), and
    //   - the CUDA tile picker accepts the (M,N,K) shape.
    // The naive one-warp-per-16x16 path below stays as the fallback for
    // ragged / non-parallel / transposed-A shapes.
    {
      int m_par_c = (m_axis_type == 5 /* KAX_GLOBAL */);
      int n_par_c = (n_axis_type == 5 /* KAX_GLOBAL */);
      int c_is_bf = (dt_c == DT_BF16 || dt_c == DT_FP16);
      RmuTcTile tile;
      if (batch_axis_id == 0xFFFFFFFFu && !a_trans && m_par_c && n_par_c
          && rmu_tc_pick_tile_cuda(m_extent, n_extent, k_extent, c_is_bf, &tile)) {
        // Autotune-chosen tile (KOP_TC packed factor) overrides the CUDA
        // heuristic when it passes the wmma validity/SMEM/occupancy check.
        rmu_tc_apply_opt_config(tc_opt_factor, m_extent, n_extent, k_extent,
                                16u, /*is_cuda=*/1, c_is_bf, &tile);
        char a_nm[24], b_nm[24], c_nm[24];
        snprintf(a_nm, sizeof(a_nm), "%s", rmu_buf_name(buf_a));
        snprintf(b_nm, sizeof(b_nm), "%s", rmu_buf_name(buf_b));
        snprintf(c_nm, sizeof(c_nm), "%s", rmu_buf_name(buf_c));
        for (u32 d = 0; d < depth; d++) fputs("  ", fp);
        fputs("/* TC WMMA matmul (nvcuda::wmma 16x16x16) -- tiled */\n", fp);
        rmu_emit_matmul_tc_tiled_cuda(a_nm, b_nm, c_nm, n_extent, k_extent,
                                      tile, a_elem, b_elem, dt_c, b_trans,
                                      ldb, fp, depth);
        return 1;
      }
    }
    // A WMMA fragment's transpose is fixed by its row_major / col_major
    // layout at declaration, not a load-time flag.  A non-transposed
    // operand (the contiguous reduce axis is unit-stride) is row_major and
    // loads with leading dim = the OTHER axis's stride (lda/ldb computed
    // above).  A transposed operand (e.g. a Transpose[w] RHS: w {N,K}
    // row-major, b_trans=1, ldb=b_n=K) is col_major and reads w^T correctly
    // from &Bbuf[addr_b] with the SAME lda/ldb leading dim.
    const char *a_layout = a_trans ? "wmma::col_major" : "wmma::row_major";
    const char *b_layout = b_trans ? "wmma::col_major" : "wmma::row_major";
    // The C OUTPUT store is the snag: the accumulator fragment is float,
    // but FLUX matmul outputs are bf16, so store_matrix_sync(&C_bf16[..],
    // float_frag, ..) is a type mismatch.  f32 output stores directly;
    // a bf16/fp16 output stores the float fragment to a per-warp shared
    // f32 scratch tile, then the 32 lanes cooperatively convert+write the
    // 256 elements into C at C[addr_c + (e/16)*ld_c + (e%16)].
    int c_direct = (dt_c == DT_FP32);
    u32 n_tiles_n_w = n_extent / 16;
    for (u32 d = 0; d < depth; d++) fputs("  ", fp);
    fputs("/* TC WMMA matmul (nvcuda::wmma 16x16x16) */\n", fp);
    // Warp id from the flat thread index: one warp owns one 16x16
    // output tile.  warp = tid / 32; tiles linearise row-major over
    // (m_tile, n_tile).
    for (u32 d = 0; d < depth; d++) fputs("  ", fp);
    fputs("uint _warp = tid / 32u;\n", fp);
    // The flat launch sizes the grid to output_numel threads (one per output
    // element): non-batched (M/16)*(N/16)*256, batched batch*(M/16)*(N/16)*256
    // -- 8x the warps a 16x16-tile-per-warp kernel needs.  Bail the
    // over-launched warps BEFORE any load/store so they neither read A/B out of
    // bounds nor corrupt C (and never reach the per-warp __syncwarp in the bf16
    // store path -- the whole warp returns together).  A batched matmul decodes
    // the batch index from the warp (warp = batch*mn_tiles + m_tile*n_tiles +
    // n_tile); the per-batch base offset is already in addr_a/addr_b/addr_c via
    // a{batch_axis}, so binding a{batch_axis} here is all the data layout needs.
    u32 mn_tiles_w = (m_extent / 16u) * (n_extent / 16u);
    if (batch_axis_id != 0xFFFFFFFFu) {
      for (u32 d = 0; d < depth; d++) fputs("  ", fp);
      fprintf(fp, "if (_warp >= %uu) return;\n", batch_extent * mn_tiles_w);
      for (u32 d = 0; d < depth; d++) fputs("  ", fp);
      fprintf(fp, "uint a%u = _warp / %uu;\n", batch_axis_id, mn_tiles_w);
      for (u32 d = 0; d < depth; d++) fputs("  ", fp);
      fprintf(fp, "uint _mn = _warp %% %uu;\n", mn_tiles_w);
      for (u32 d = 0; d < depth; d++) fputs("  ", fp);
      fprintf(fp, "uint a%u = (_mn / %uu) * 16u;\n", m_axis_id, n_tiles_n_w);
      for (u32 d = 0; d < depth; d++) fputs("  ", fp);
      fprintf(fp, "uint a%u = (_mn %% %uu) * 16u;\n", n_axis_id_v, n_tiles_n_w);
    } else {
      for (u32 d = 0; d < depth; d++) fputs("  ", fp);
      fprintf(fp, "if (_warp >= %uu) return;\n", mn_tiles_w);
      for (u32 d = 0; d < depth; d++) fputs("  ", fp);
      fprintf(fp, "uint a%u = (_warp / %uu) * 16u;\n", m_axis_id, n_tiles_n_w);
      for (u32 d = 0; d < depth; d++) fputs("  ", fp);
      fprintf(fp, "uint a%u = (_warp %% %uu) * 16u;\n", n_axis_id_v, n_tiles_n_w);
    }
    for (u32 d = 0; d < depth; d++) fputs("  ", fp);
    fprintf(fp, "wmma::fragment<wmma::matrix_a, 16, 16, 16, %s, %s> "
          "_a_frag;\n", a_elem, a_layout);
    for (u32 d = 0; d < depth; d++) fputs("  ", fp);
    fprintf(fp, "wmma::fragment<wmma::matrix_b, 16, 16, 16, %s, %s> "
          "_b_frag;\n", b_elem, b_layout);
    for (u32 d = 0; d < depth; d++) fputs("  ", fp);
    fputs("wmma::fragment<wmma::accumulator, 16, 16, 16, float> "
          "_c_frag;\n", fp);
    for (u32 d = 0; d < depth; d++) fputs("  ", fp);
    fputs("wmma::fill_fragment(_c_frag, 0.0f);\n", fp);
    for (u32 d = 0; d < depth; d++) fputs("  ", fp);
    fprintf(fp, "for (uint a%u = 0; a%u < %u; a%u += 16) {\n",
            red_axis, red_axis, k_extent, red_axis);
    // A fragment loads from &A[addr_a] with leading dim = lda; B from
    // &B[addr_b] with leading dim = ldb.  For a row_major operand that ld
    // is the M/K stride; for a col_major (transposed) operand it is the
    // K/N stride -- exactly the lda/ldb decoded from the address coeffs.
    for (u32 d = 0; d < depth + 1; d++) fputs("  ", fp);
    fprintf(fp, "wmma::load_matrix_sync(_a_frag, &%s[", rmu_buf_name(buf_a));
    rmu_emit_term(addr_a, fp);
    fprintf(fp, "], %uu);\n", (u32)lda);
    for (u32 d = 0; d < depth + 1; d++) fputs("  ", fp);
    fprintf(fp, "wmma::load_matrix_sync(_b_frag, &%s[", rmu_buf_name(buf_b));
    rmu_emit_term(addr_b, fp);
    fprintf(fp, "], %uu);\n", (u32)ldb);
    for (u32 d = 0; d < depth + 1; d++) fputs("  ", fp);
    fputs("wmma::mma_sync(_c_frag, _a_frag, _b_frag, _c_frag);\n", fp);
    for (u32 d = 0; d < depth; d++) fputs("  ", fp);
    fputs("}\n", fp);
    if (c_direct) {
      // f32 output: store the float accumulator directly.  ld = C
      // row-stride = n_extent (the output is row-major {M,N}).
      for (u32 d = 0; d < depth; d++) fputs("  ", fp);
      fprintf(fp, "wmma::store_matrix_sync(&%s[", rmu_buf_name(buf_c));
      rmu_emit_term(addr_c, fp);
      fprintf(fp, "], _c_frag, %u, wmma::mem_row_major);\n", n_extent);
    } else {
      // bf16/fp16 output: store the float fragment to a per-warp f32
      // scratch tile, then convert+write to C.  The scratch is sized to a
      // fixed upper bound of 32 warps/block (block <= 1024 threads => <=32
      // warps), so it does not couple to the dispatch.  `sgi` is the
      // block-local warp index (threadIdx.x / 32) and the lane is
      // threadIdx.x % 32 -- the CUDA `tid` is the GLOBAL grid index, so it
      // must NOT index the per-block scratch.  store_matrix_sync writes
      // frag[i][j] -> scratch[i*16 + j]; the cooperative loop reads element
      // e (e=lane, lane+32, ...) as row e/16, col e%16, and writes to
      // C[addr_c + (e/16)*n_extent + (e%16)] (C row-major, row stride
      // n_extent).
      for (u32 d = 0; d < depth; d++) fputs("  ", fp);
      fputs("__shared__ float _cscratch[32][256];\n", fp);
      for (u32 d = 0; d < depth; d++) fputs("  ", fp);
      fputs("wmma::store_matrix_sync(_cscratch[sgi], _c_frag, 16u, "
            "wmma::mem_row_major);\n", fp);
      for (u32 d = 0; d < depth; d++) fputs("  ", fp);
      fputs("__syncwarp();\n", fp);
      for (u32 d = 0; d < depth; d++) fputs("  ", fp);
      fputs("for (uint _e = threadIdx.x % 32u; _e < 256u; _e += 32u) {\n", fp);
      for (u32 d = 0; d < depth + 1; d++) fputs("  ", fp);
      fprintf(fp, "%s[(", rmu_buf_name(buf_c));
      rmu_emit_term(addr_c, fp);
      fprintf(fp, ") + (_e / 16u) * %uu + (_e %% 16u)] = "
              "(%s)_cscratch[sgi][_e];\n",
              n_extent, rmu_cuda_type_name(dt_c));
      for (u32 d = 0; d < depth; d++) fputs("  ", fp);
      fputs("}\n", fp);
    }
    return 1;
  }

  // Metal simdgroup_matrix path (below) hardcodes a
  // simdgroup_matrix<float,8,8> A/B/C fragment, and Metal's
  // simdgroup_load requires the device pointer element type to match
  // the matrix scalar type (no implicit bfloat/half->float widening at
  // the load).  When A or B is a non-fp32 buffer (e.g. the FLUX bf16
  // linear weight) fall back to the scalar accumulator path: it loads
  // each operand at its declared type (`bfloat`/`half`), the hardware
  // widens to float in the multiply, and the accumulator is float --
  // an exact bf16-input / float-accumulate matmul.  Mirrors the CUDA
  // WMMA gate above (fp16-only fragments) and tinygrad's TC support
  // gating (tinygrad/renderer/cstyle.py MetalRenderer tensor_cores).
  {
    u32 dt_a = rmu_slot_dtype(buf_a);
    u32 dt_b = rmu_slot_dtype(buf_b);
    // f32 and bf16 operands are both handled below: the register-blocked
    // tiled path stages through `float` threadgroup memory (bfloat->float on
    // the cooperative load), and the parallel_tc path declares each fragment
    // as simdgroup_matrix<bfloat> matching its buffer, accumulating in f32.
    // An INT8 B operand (the q8 weight) is also handled, but ONLY by the
    // register-blocked tiled path, which reads it as `char` and casts
    // char -> bfloat in the cooperative B-staging.  Anything else (fp16,
    // fp8, int8 on A) still falls back to the scalar path.
    if (dt_a != DT_FP32 && dt_a != DT_BF16) return 0;
    if (dt_b != DT_FP32 && dt_b != DT_BF16 && !b_is_int8) return 0;
  }

  // Parallel-TC selector.  If the M and N axes carry GLOBAL axis_type
  // (Phase E annotation), the caller's dispatch shape binds each
  // threadgroup to a unique 8x8 output tile, so the multi-SG write
  // race is impossible -- drop the `sgi==0 && tg==0` guard and emit
  // position-bound m/n declarations instead of for-loops.  Coverage:
  //   m=GLOBAL && n=GLOBAL  -> 2D-folded `tg` linearises tiles:
  //                            m = (tg / N_tiles)*8, n = (tg % N_tiles)*8
  //   m=GLOBAL && n=LOOP    -> m = tg*8, n loops over N_tiles*8
  //   m=LOOP   && n=GLOBAL  -> n = tg*8, m loops over M_tiles*8
  //   else                  -> guarded sequential (legacy / safe)
  //
  // Dispatch (Phase E parallel TC, both GLOBAL):
  //   grid       = (num_m_tiles * num_n_tiles * 32, 1, 1)
  //   threadgroup = (32, 1, 1)
  // i.e. one simdgroup per TG, one TG per output tile.
  int m_par = (m_axis_type == 5 /* KAX_GLOBAL */);
  int n_par = (n_axis_type == 5 /* KAX_GLOBAL */);
  int parallel_tc = (m_par || n_par);
  u32 n_tiles_n = n_extent / 8;

  // A bf16 OUTPUT needs the threadgroup-staged f32->bf16 store, which assumes
  // one simdgroup owns the threadgroup (no scratch sharing) and a flat body
  // (threadgroup decls can't sit inside the per-axis for-loops).  Only the
  // both-GLOBAL parallel dispatch guarantees both; decline other bf16 cases to
  // the generic accumulator (correct, just not TC).  f32 output is unaffected.
  if (uop_buffer_dtype(buf_c) == DT_BF16 && !(m_par && n_par)) return 0;

  // Threadgroup-staged, register-blocked tiled path: only the
  // both-GLOBAL case (each tg owns a unique output tile, no write race)
  // is eligible.  When a worthwhile tile divides M/N/K cleanly, emit
  // the tiled kernel and return -- the dispatch grid in
  // cg_tile_metal_dispatch_shape sizes threadgroups to the larger tile.
  // bf16 inputs are accepted too: the global A/B reads are staged into
  // `threadgroup float` (an implicit bfloat->float widening), the MMA stays
  // float, and a bf16 output goes through a per-simdgroup f32->bf16 staged
  // store (see rmu_emit_matmul_tc_tiled).  The win is the tiled data-reuse
  // structure, not bf16 MMA rate.
  // For a fused A producer the staging element type is chosen to MATCH B (the
  // MMA needs A/B fragments to share a scalar type, and the producer is cast to
  // the staging type `sel` at the A-staging load regardless of its own dtypes).
  // A bf16 B -> stage bfloat (native TC rate); an f32 B -> stage float.  For a
  // plain buffer A, the staging follows A's buffer dtype as before.
  u32 _dtb = uop_buffer_dtype(buf_b);
  u32 _dta = (a_val != 0) ? ((_dtb == DT_BF16 || b_is_int8) ? DT_BF16 : DT_FP32)
                          : uop_buffer_dtype(buf_a);
  // INT8 B (the q8 weight) is eligible for the tiled path only: it stages
  // char -> bfloat in the cooperative B-load (see rmu_emit_matmul_tc_tiled's
  // b_is_int8 branch).  A is never int8 here (gated above).
  int _tc_dtype_ok = (_dta == DT_FP32 || _dta == DT_BF16)
                  && (_dtb == DT_FP32 || _dtb == DT_BF16 || b_is_int8);
  // The register-blocked tiled emitter assumes a row-major A[m*K+k] and
  // either a row-major B[k*N+n] OR a transposed B = Transpose[W] (W {N,K}
  // contiguous: B[k][n] = W[n][k], staged via b_nstride -- the contiguous
  // FLUX path that loads {out,in} weights and matmuls Transpose[W]).  It
  // honours neither a batch base-pointer offset nor a transposed A: the
  // batched gemm (attention's mhaBmm) has a batch offset and an A-transpose
  // falls through to the per-8x8-tile parallel_tc body below (which emits
  // the full affine address via rmu_emit_term, any stride/offset included).
  int is_batched = (batch_axis_id != 0xFFFFFFFFu);
  if (getenv("THVM_MATMUL_TRACE") != NULL) {
    RmuTcTile _dt; int _tiled = ((!is_batched || rmu_tc_batched_on()) && !a_trans
        && m_par && n_par && RMU_TARGET == CG_TARGET_METAL && _tc_dtype_ok
        && !(is_batched && (a_val != 0 || b_is_int8))
        && rmu_tc_pick_tile(m_extent, n_extent, k_extent, &_dt));
    fprintf(stderr, "[mm] M=%u N=%u K=%u batch=%u%s path=%s%s\n",
            m_extent, n_extent, k_extent, batch_extent,
            a_trans ? " Atrans" : "", _tiled ? "TILED" : "parallel_tc/other",
            epilogue_value != 0 ? " +epi" : "");
  }
  if ((!is_batched || rmu_tc_batched_on()) && !a_trans && m_par && n_par
      && RMU_TARGET == CG_TARGET_METAL && _tc_dtype_ok
      // batched-tiled handles only the plain bf16 attention GEMM for now:
      // no fused-A producer, no int8 weight, no epilogue (all 0 for attention).
      && !(is_batched && (a_val != 0 || b_is_int8 || epilogue_value != 0))) {
    RmuTcTile tile;
    if (rmu_tc_pick_tile(m_extent, n_extent, k_extent, &tile)) {
      // rmu_buf_name returns a pointer to a shared static buffer, so the
      // three names must be copied to distinct locals before the call
      // (a single argument list would alias them all to the last name).
      char a_nm[24], b_nm[24], c_nm[24];
      snprintf(a_nm, sizeof(a_nm), "%s", rmu_buf_name(buf_a));
      snprintf(b_nm, sizeof(b_nm), "%s", rmu_buf_name(buf_b));
      snprintf(c_nm, sizeof(c_nm), "%s", rmu_buf_name(buf_c));
      int c_is_bf = (uop_buffer_dtype(buf_c) == DT_BF16);
      // Autotune-chosen tile (KOP_TC packed factor) overrides the heuristic
      // when valid -- the same hook the env override uses, now driven by the
      // BEAM search instead of getenv.  Invalid/absent -> heuristic unchanged.
      rmu_tc_apply_opt_config(tc_opt_factor, m_extent, n_extent, k_extent,
                              8u, /*is_cuda=*/0, c_is_bf, &tile);
      // Stage as bfloat when A is bf16 and B is bf16 OR an int8 weight (the
      // q8 path casts char -> bfloat into the bfloat staging buffer, so the
      // MMA runs at the native bf16 tensor-core rate).  A bf16-A / int8-B
      // matmul (the FLUX q8 case) therefore stages bfloat; an f32-A / int8-B
      // matmul stages float (B casts char -> float).
      int stage_bf = (_dta == DT_BF16) && (_dtb == DT_BF16 || b_is_int8);
      // Epilogue fusion is implemented only for the bf16 staging store (the FLUX
      // target).  An f32-output matmul with an epilogue bails to the generic
      // accumulator path, which applies the epilogue via the store-value-over-
      // reduce machinery (correct, scalar -- f32 matmuls are rare in FLUX).
      if (epilogue_value != 0 && !c_is_bf) return 0;
      // Real per-axis strides (rmu_range_coeff) so the batched-tiled path is
      // correct for the STRIDED attention Q/K (transposed {heads,seq,head_dim}
      // views): A's leading dim = lda (a_m), and the per-batch slice strides are
      // the batch-axis coefficient in each address (NOT the contiguous M*K/K*N/
      // M*N).  Non-batched packed operands have lda==k_extent + unused strides,
      // so the emitted code is byte-identical to before.
      u32 a_lead = is_batched ? (u32)lda : k_extent;
      i64 bsa = is_batched ? rmu_range_coeff(addr_a, batch_axis_id) : 0;
      i64 bsb = is_batched ? rmu_range_coeff(addr_b, batch_axis_id) : 0;
      i64 bsc = is_batched ? rmu_range_coeff(addr_c, batch_axis_id) : 0;
      rmu_emit_matmul_tc_tiled(a_nm, b_nm, c_nm, n_extent,
                               k_extent, tile, c_is_bf, stage_bf,
                               b_trans, ldb, fp, depth, b_is_int8,
                               a_val, m_axis_id, red_axis,
                               epilogue_value, opt_tc_term,
                               m_range_term, n_range_term,
                               is_batched ? batch_extent : 0u, m_extent,
                               a_lead, (u32)bsa, (u32)bsb, (u32)bsc);
      return 1;
    }
  }

  // An epilogue that did not take the Metal tiled bf16 path above (tile too
  // small, A transposed, batched, non-TC dtype) bails to the generic
  // accumulator path, which applies the epilogue correctly (scalar).
  if (epilogue_value != 0) return 0;

  // A fused producer that did not fit the Metal tiled emitter (shape too small
  // for a worthwhile tile, A transposed, batched, or non-TC dtype) cannot use
  // the non-tiled / guarded simdgroup paths below -- those read A as a flat
  // a_name[addr_a] buffer, which would load the representative leaf, not the
  // producer.  Bail to the generic accumulator (rmu_emit_term recomputes the
  // whole producer subtree per element -- correct, just not TC).
  if (a_val != 0) return 0;

  if (parallel_tc) {
    // No guard.  Bind axes per axis_type.
    for (u32 d = 0; d < depth; d++) fputs("  ", fp);
    fputs("/* parallel TC: m/n bound to tg; one SG per output tile */\n", fp);
    if (m_par && n_par && is_batched) {
      // Batched: linearise tg as (batch, m_tile, n_tile).  The batch axis
      // is bound directly (each value is one independent gemm); m/n decode
      // within the per-batch tile block.  The A/B/C addresses already carry
      // a%u(batch)*batch_stride, so rmu_emit_term offsets each operand.
      u32 m_tiles_m = m_extent / 8u;
      u32 tiles_per_batch = m_tiles_m * n_tiles_n;
      for (u32 d = 0; d < depth; d++) fputs("  ", fp);
      fprintf(fp, "uint a%u = tg / %uu;\n", batch_axis_id, tiles_per_batch);
      for (u32 d = 0; d < depth; d++) fputs("  ", fp);
      fprintf(fp, "uint _t2 = tg %% %uu;\n", tiles_per_batch);
      for (u32 d = 0; d < depth; d++) fputs("  ", fp);
      fprintf(fp, "uint a%u = (_t2 / %uu) * 8u;\n", m_axis_id, n_tiles_n);
      for (u32 d = 0; d < depth; d++) fputs("  ", fp);
      fprintf(fp, "uint a%u = (_t2 %% %uu) * 8u;\n", n_axis_id_v, n_tiles_n);
    } else if (m_par && n_par) {
      for (u32 d = 0; d < depth; d++) fputs("  ", fp);
      fprintf(fp, "uint a%u = (tg / %uu) * 8u;\n", m_axis_id, n_tiles_n);
      for (u32 d = 0; d < depth; d++) fputs("  ", fp);
      fprintf(fp, "uint a%u = (tg %% %uu) * 8u;\n", n_axis_id_v, n_tiles_n);
    } else if (m_par) {
      for (u32 d = 0; d < depth; d++) fputs("  ", fp);
      fprintf(fp, "uint a%u = tg * 8u;\n", m_axis_id);
      for (u32 d = 0; d < depth; d++) fputs("  ", fp);
      fprintf(fp, "for (uint a%u = 0; a%u < %u; a%u += 8) {\n",
              n_axis_id_v, n_axis_id_v, n_extent, n_axis_id_v);
    } else { /* n_par */
      for (u32 d = 0; d < depth; d++) fputs("  ", fp);
      fprintf(fp, "uint a%u = tg * 8u;\n", n_axis_id_v);
      for (u32 d = 0; d < depth; d++) fputs("  ", fp);
      fprintf(fp, "for (uint a%u = 0; a%u < %u; a%u += 8) {\n",
              m_axis_id, m_axis_id, m_extent, m_axis_id);
    }
  } else {
    // Legacy guarded sequential path.  Multi-simdgroup race guard:
    // simdgroup_matrix ops cooperate on the calling simdgroup's 32
    // threads.  When the dispatch shape binds multiple SGs/TGs (the
    // default for output_numel >= 32), every SG runs the same code
    // and writes to the same output addresses concurrently; on M3
    // this race yields garbage outputs.  Gate the body so only the
    // first SG of the first TG runs; others idle.  Wasteful but
    // correct under any dispatch shape.
    for (u32 d = 0; d < depth; d++) fputs("  ", fp);
    fputs("if (sgi == 0u && tg == 0u) {\n", fp);
    for (u32 d = 0; d < depth + 1; d++) fputs("  ", fp);
    fprintf(fp, "for (uint a%u = 0; a%u < %u; a%u += 8) {\n",
            m_axis_id, m_axis_id, m_extent, m_axis_id);
    for (u32 d = 0; d < depth + 2; d++) fputs("  ", fp);
    fprintf(fp, "for (uint a%u = 0; a%u < %u; a%u += 8) {\n",
            n_axis_id_v, n_axis_id_v, n_extent, n_axis_id_v);
  }

  // body_depth depends on path:
  //   parallel_tc & both GLOBAL: depth (no opener)
  //   parallel_tc & one GLOBAL : depth + 1 (one for-loop opener)
  //   guarded                  : depth + 3 (guard + 2 for-loops)
  u32 body_depth;
  if (parallel_tc) {
    body_depth = (m_par && n_par) ? depth : (depth + 1);
  } else {
    body_depth = depth + 3;
  }

  // Input fragment element type follows each operand's buffer dtype: a bfloat
  // buffer loads into a simdgroup_matrix<bfloat> (Metal 3.1; loading it into a
  // <float> fragment is a type error).  The accumulator stays f32 for full
  // precision across the K loop (a bfloat accumulator rounds every 8-K block
  // and drifts ~10x); a bf16 OUTPUT is converted f32->bf16 at the store.
  const char *a_el = (uop_buffer_dtype(buf_a) == DT_BF16) ? "bfloat" : "float";
  const char *b_el = (uop_buffer_dtype(buf_b) == DT_BF16) ? "bfloat" : "float";
  int c_is_bf = (uop_buffer_dtype(buf_c) == DT_BF16);
  // simdgroup_multiply_accumulate requires A and B fragments to share a scalar
  // type; mixed bf16/f32 operands (the attention matmuls: f32 RoPE'd q/k, bf16
  // softmax attn x f32 V) can't be MMA'd directly.  The batched gemm also needs
  // per-batch base offsets that simdgroup_load's ld/transpose flag can't carry.
  // For both, stage each 8x8 sub-tile through threadgroup float (a cooperative
  // scalar load widens bf16->float and reads any strided/permuted view via the
  // operand stride coeffs), then MMA uniform <float> fragments.  The plain 2-D
  // same-dtype matmul keeps the direct simdgroup_load (byte-identical).  Only
  // the both-GLOBAL parallel dispatch (one simdgroup / threadgroup) is eligible
  // for staging -- its threadgroup scratch is owned by exactly this simdgroup.
  int need_stage = (is_batched || strcmp(a_el, b_el) != 0)
                   && parallel_tc && m_par && n_par;
  fputs("/* TC simdgroup_matrix matmul */\n", fp);
  if (need_stage) {
    // Per-element source strides for the 8x8 stage: A[m+i, k0+j] =
    // base_A + i*a_m + j*a_k; B[k0+i, n+j] = base_B + i*b_k + j*b_n.  The
    // base address (rmu_emit_term) carries the batch offset + current m/n/k0.
    for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
    fputs("threadgroup float _As[64];\n", fp);
    for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
    fputs("threadgroup float _Bs[64];\n", fp);
    for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
    fputs("simdgroup_matrix<float, 8, 8> _a_mat;\n", fp);
    for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
    fputs("simdgroup_matrix<float, 8, 8> _b_mat;\n", fp);
    for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
    fputs("simdgroup_matrix<float, 8, 8> _c_mat = simdgroup_matrix<float, 8, 8>(0);\n", fp);
    for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
    fprintf(fp, "for (uint a%u = 0; a%u < %u; a%u += 8) {\n",
            red_axis, red_axis, k_extent, red_axis);
    // base addresses for the current (m, n, k0, batch).
    for (u32 d = 0; d < body_depth + 1; d++) fputs("  ", fp);
    fprintf(fp, "uint _ab = ");
    rmu_emit_term(addr_a, fp);
    fputs(";\n", fp);
    for (u32 d = 0; d < body_depth + 1; d++) fputs("  ", fp);
    fprintf(fp, "uint _bb = ");
    rmu_emit_term(addr_b, fp);
    fputs(";\n", fp);
    // Cooperative stage: 32 threads fill 64 elements (2 each), row-major.
    for (u32 d = 0; d < body_depth + 1; d++) fputs("  ", fp);
    fputs("for (uint _e = thread_index_in_simdgroup; _e < 64u; _e += 32u) {\n", fp);
    for (u32 d = 0; d < body_depth + 2; d++) fputs("  ", fp);
    fputs("uint _i = _e / 8u, _j = _e % 8u;\n", fp);
    for (u32 d = 0; d < body_depth + 2; d++) fputs("  ", fp);
    fprintf(fp, "_As[_e] = (float)%s[_ab + _i*%lldu + _j*%lldu];\n",
            rmu_buf_name(buf_a), (long long)a_m, (long long)a_k);
    for (u32 d = 0; d < body_depth + 2; d++) fputs("  ", fp);
    fprintf(fp, "_Bs[_e] = (float)%s[_bb + _i*%lldu + _j*%lldu];\n",
            rmu_buf_name(buf_b), (long long)b_k, (long long)b_n);
    for (u32 d = 0; d < body_depth + 1; d++) fputs("  ", fp);
    fputs("}\n", fp);
    for (u32 d = 0; d < body_depth + 1; d++) fputs("  ", fp);
    fputs("simdgroup_barrier(mem_flags::mem_threadgroup);\n", fp);
    for (u32 d = 0; d < body_depth + 1; d++) fputs("  ", fp);
    fputs("simdgroup_load(_a_mat, _As, 8);\n", fp);
    for (u32 d = 0; d < body_depth + 1; d++) fputs("  ", fp);
    fputs("simdgroup_load(_b_mat, _Bs, 8);\n", fp);
    for (u32 d = 0; d < body_depth + 1; d++) fputs("  ", fp);
    fputs("simdgroup_multiply_accumulate(_c_mat, _a_mat, _b_mat, _c_mat);\n", fp);
    for (u32 d = 0; d < body_depth + 1; d++) fputs("  ", fp);
    fputs("simdgroup_barrier(mem_flags::mem_threadgroup);\n", fp);
    for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
    fputs("}\n", fp);
  } else {
  for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
  fprintf(fp, "simdgroup_matrix<%s, 8, 8> _a_mat;\n", a_el);
  for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
  fprintf(fp, "simdgroup_matrix<%s, 8, 8> _b_mat;\n", b_el);
  for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
  fputs("simdgroup_matrix<float, 8, 8> _c_mat = simdgroup_matrix<float, 8, 8>(0);\n", fp);
  for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
  fprintf(fp, "for (uint a%u = 0; a%u < %u; a%u += 8) {\n",
          red_axis, red_axis, k_extent, red_axis);
  for (u32 d = 0; d < body_depth + 1; d++) fputs("  ", fp);
  fprintf(fp, "simdgroup_load(_a_mat, &%s[", rmu_buf_name(buf_a));
  rmu_emit_term(addr_a, fp);
  if (a_trans) fprintf(fp, "], %lld, ulong2(0), true);\n", (long long)lda);
  else         fprintf(fp, "], %lld);\n", (long long)lda);
  for (u32 d = 0; d < body_depth + 1; d++) fputs("  ", fp);
  fprintf(fp, "simdgroup_load(_b_mat, &%s[", rmu_buf_name(buf_b));
  rmu_emit_term(addr_b, fp);
  if (b_trans) fprintf(fp, "], %lld, ulong2(0), true);\n", (long long)ldb);
  else         fprintf(fp, "], %lld);\n", (long long)ldb);
  for (u32 d = 0; d < body_depth + 1; d++) fputs("  ", fp);
  fputs("simdgroup_multiply_accumulate(_c_mat, _a_mat, _b_mat, _c_mat);\n", fp);
  for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
  fputs("}\n", fp);
  }
  // Store the f32 accumulator.  f32 output: direct simdgroup_store.  bf16
  // output (`device bfloat*`): simdgroup_store has no f32->bf16 widening and
  // there is no <float>-><bfloat> matrix cast, so stage the 8x8 tile through
  // threadgroup f32 memory then cooperatively convert+write to bfloat.  Gated
  // upstream to the both-GLOBAL dispatch (one simdgroup per threadgroup), so
  // the per-threadgroup scratch is owned by exactly this simdgroup.
  if (c_is_bf) {
    for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
    fputs("threadgroup float _ctile[64];\n", fp);
    for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
    fputs("simdgroup_store(_c_mat, _ctile, 8);\n", fp);
    for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
    fputs("simdgroup_barrier(mem_flags::mem_threadgroup);\n", fp);
    for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
    fputs("uint _cbase = ", fp);
    rmu_emit_term(addr_c, fp);
    fputs(";\n", fp);
    for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
    fputs("for (uint _e = thread_index_in_simdgroup; _e < 64u; _e += 32u) {\n", fp);
    for (u32 d = 0; d < body_depth + 1; d++) fputs("  ", fp);
    fprintf(fp, "%s[_cbase + (_e / 8u) * %uu + (_e %% 8u)] = (bfloat)_ctile[_e];\n",
            rmu_buf_name(buf_c), n_extent);
    for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
    fputs("}\n", fp);
  } else {
    for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
    fprintf(fp, "simdgroup_store(_c_mat, &%s[", rmu_buf_name(buf_c));
    rmu_emit_term(addr_c, fp);
    fprintf(fp, "], %u);\n", n_extent);
  }

  // Close any blocks opened above based on which path we took.
  if (parallel_tc) {
    if (!(m_par && n_par)) {
      // One for-loop opener (the non-GLOBAL axis); close it.
      for (u32 d = 0; d < depth; d++) fputs("  ", fp);
      fputs("}\n", fp);
    }
    // both-GLOBAL case: nothing to close (no openers).
  } else {
    // Guarded path: close N, M, then the sgi/tg guard.
    for (u32 d = 0; d < depth + 2; d++) fputs("  ", fp);
    fputs("}\n", fp);
    for (u32 d = 0; d < depth + 1; d++) fputs("  ", fp);
    fputs("}\n", fp);
    for (u32 d = 0; d < depth; d++) fputs("  ", fp);
    fputs("}\n", fp);
  }
  return 1;
}

// GROUP_REDUCE-shaped emission: parallel cooperative reduce using a
// threadgroup-shared accumulator + barrier + per-thread serial walk
// + final single-thread combine.  Fires when one of the reduce ranges
// was stamped with UOP_OPT_GROUP_REDUCE by hand_opts (KOP_GROUPTOP).
//
// Single-axis shape (the only one rmu_emit_group_reduce_single covers):
//   __shared__ float _accN[L];
//   _accN[tt] = init;
//   __syncthreads();
//   for (uint k = tt; k < red_extent; k += L) _accN[tt] = combine(_accN[tt], body(k));
//   __syncthreads();
//   if (tt == 0) {
//     float total = init;
//     for (uint i = 0; i < L; i++) total = combine(total, _accN[i]);
//     out[addr] = total;
//   }
//
// Multi-axis shape (rmu_emit_group_reduce_multi): outer serial REDUCE
// loops wrap the cooperative inner GROUP_REDUCE axis.  Lets the conv
// reduce (C_in, kH, kW: 3 axes, one GROUP'd) get the parallel template
// without bailing.
//
// Caller passes the open output-axis loop nest in `n_out` /
// `needs_close[]` so this function can close them in the same order
// as the scalar path.

// Internal helper: write the combine LHS = LHS OP body line (or the
// max-ternary form).  acc_lhs is the full LHS expression including
// any `[tt]` subscript; rmu_emit_term renders the body in place.
static void rmu_emit_group_combine_line(const char *acc_lhs, u32 red_kind,
                                        Term body, FILE *fp) {
  fprintf(fp, "%s = ", acc_lhs);
  fprintf(fp, "%s", acc_lhs);
  if (red_kind == REDUCE_SUM)      fputs(" + ", fp);
  else if (red_kind == REDUCE_MAX) fputs(" > ", fp);   // placeholder
  else                              fputs(" + ", fp);
  rmu_emit_term(body, fp);
  if (red_kind == REDUCE_MAX) {
    fprintf(fp, " ? %s : ", acc_lhs);
    rmu_emit_term(body, fp);
  }
  fputs(";\n", fp);
}

static int rmu_emit_group_reduce(Term buf, Term addr,
                                 Term const *red_ranges,
                                 u32 const *red_kind_opts,
                                 u32 const *red_factor_opts,
                                 u32 red_n_axes,
                                 Term red_src,
                                 u32 red_kind,
                                 FILE *fp, u32 body_depth,
                                 u32 n_out, int const *needs_close) {
  // Find the (single) GROUP_REDUCE axis and the cooperative factor.
  // Multi-GROUP-REDUCE-axes-per-reduce isn't reachable from the gate
  // (hand_opts applies GROUPTOP to one REDUCE axis at a time) but the
  // code defends against it explicitly.
  i32 grp_idx = -1;
  for (u32 ai = 0; ai < red_n_axes; ai++) {
    if (red_kind_opts[ai] == UOP_OPT_GROUP_REDUCE) {
      if (grp_idx >= 0) return 0;        // multiple GROUP_REDUCE axes: bail
      grp_idx = (i32)ai;
    }
  }
  if (grp_idx < 0) return 0;
  u32 group_extent = red_factor_opts[grp_idx];
  if (group_extent == 0) return 0;
  Term red_range = red_ranges[grp_idx];
  if (term_tag(red_range) != TAG_UOP || term_ext(red_range) != UOP_RANGE) {
    return 0;
  }
  u32 red_axis   = uop_range_axis_id(red_range);
  u32 red_extent = uop_range_extent(red_range);
  // Target-specific spellings: Metal -> `threadgroup` + `threadgroup_barrier`;
  // CUDA -> `__shared__` + `__syncthreads()`; C target bails (no shared mem).
  if (RMU_TARGET == CG_TARGET_C) return 0;
  const char *shared_kw = (RMU_TARGET == CG_TARGET_CUDA)
                          ? "__shared__ float"
                          : "threadgroup float";
  const char *barrier_stmt = (RMU_TARGET == CG_TARGET_CUDA)
                             ? "__syncthreads();"
                             : "threadgroup_barrier(mem_flags::mem_threadgroup);";
  char acc_name[32];
  snprintf(acc_name, sizeof(acc_name), "_acc%u", red_axis);
  char acc_slot[40];
  snprintf(acc_slot, sizeof(acc_slot), "%s[tt]", acc_name);
  // Shared-mem accumulator declaration.  One slot per THREAD: with a
  // coexisting LOCAL axis the threadgroup is local_total*group_extent and
  // each thread owns _acc[tt] (group is the innermost tt dim).  GROUP-only
  // (local_total==1) reduces to the plain `_acc[group_extent]`.
  u32 grp_local_total = (RMU_GROUP_LOCAL_TOTAL > 0) ? RMU_GROUP_LOCAL_TOTAL : 1;
  u32 acc_size        = grp_local_total * group_extent;
  for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
  fprintf(fp, "%s %s[%u];\n", shared_kw, acc_name, acc_size);
  // Per-thread init.
  for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
  fprintf(fp, "%s = ", acc_slot);
  rmu_emit_reduce_init(red_kind, fp);
  fputs(";\n", fp);
  // Pre-loop barrier so every thread sees a clean slot.
  for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
  fprintf(fp, "%s\n", barrier_stmt);
  // Multi-axis: open serial REDUCE loops for the non-GROUP axes around
  // the cooperative strided walk over the GROUP axis.  Each thread
  // iterates the full cross-product of non-grouped extents AND a 1/L
  // slice of the grouped axis; the shared accumulator collects all
  // partial sums into `_acc[tt]`.  Inner combine line touches body once.
  u32 loop_depth = body_depth;
  for (u32 ai = 0; ai < red_n_axes; ai++) {
    if ((i32)ai == grp_idx) continue;
    Term r = red_ranges[ai];
    if (term_tag(r) != TAG_UOP || term_ext(r) != UOP_RANGE) return 0;
    u32 ax  = uop_range_axis_id(r);
    u32 ext = uop_range_extent(r);
    for (u32 d = 0; d < loop_depth; d++) fputs("  ", fp);
    fprintf(fp, "for (uint a%u = 0; a%u < %u; a%u++) {\n",
            ax, ax, ext, ax);
    loop_depth++;
  }
  // Per-thread strided walk over the GROUP axis.  The group sub-index is
  // tt % group_extent (the innermost tt dim); the LOCAL row is tt /
  // group_extent.  GROUP-only -> tt % group_extent == tt (threadgroup is
  // exactly group_extent), so this is unchanged there.
  for (u32 d = 0; d < loop_depth; d++) fputs("  ", fp);
  fprintf(fp, "for (uint a%u = (tt %% %uu); a%u < %u; a%u += %u) {\n",
          red_axis, group_extent, red_axis, red_extent, red_axis, group_extent);
  loop_depth++;
  for (u32 d = 0; d < loop_depth; d++) fputs("  ", fp);
  rmu_emit_group_combine_line(acc_slot, red_kind, red_src, fp);
  // Close GROUP-axis loop + non-GROUP serial loops.
  for (u32 ai = 0; ai <= red_n_axes; ai++) {
    if (loop_depth <= body_depth) break;
    loop_depth--;
    for (u32 d = 0; d < loop_depth; d++) fputs("  ", fp);
    fputs("}\n", fp);
  }
  // Post-loop barrier.
  for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
  fprintf(fp, "%s\n", barrier_stmt);
  // Final combine + store, ONE thread per LOCAL row: the threads whose group
  // sub-index is 0 (tt % group_extent == 0).  Each owns group_extent
  // contiguous slots [tt, tt+group_extent) (group is the innermost tt dim),
  // sums them, and writes its own output addr (a_local = tt / group_extent).
  // GROUP-only -> the lone tt==0 thread sums _acc[0..group_extent), unchanged.
  for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
  fprintf(fp, "if (tt %% %uu == 0u) {\n", group_extent);
  for (u32 d = 0; d < body_depth + 1; d++) fputs("  ", fp);
  fputs("float _total = ", fp);
  rmu_emit_reduce_init(red_kind, fp);
  fputs(";\n", fp);
  for (u32 d = 0; d < body_depth + 1; d++) fputs("  ", fp);
  fprintf(fp, "for (uint _i = 0; _i < %u; _i++) {\n", group_extent);
  for (u32 d = 0; d < body_depth + 2; d++) fputs("  ", fp);
  if (red_kind == REDUCE_SUM) {
    fprintf(fp, "_total = _total + %s[tt + _i];\n", acc_name);
  } else {
    fprintf(fp, "_total = (_total > %s[tt + _i]) ? _total : %s[tt + _i];\n",
            acc_name, acc_name);
  }
  for (u32 d = 0; d < body_depth + 1; d++) fputs("  ", fp);
  fputs("}\n", fp);
  for (u32 d = 0; d < body_depth + 1; d++) fputs("  ", fp);
  fprintf(fp, "%s[", rmu_buf_name(buf));
  rmu_emit_term(addr, fp);
  fputs("] = ", fp);
  rmu_store_cast_open(buf, fp);
  fputs("_total", fp);
  rmu_store_cast_close(buf, fp);
  fputs(";\n", fp);
  for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
  fputs("}\n", fp);
  // Close any open output-axis loops opened by the caller.
  for (i32 i = (i32)n_out - 1; i >= 0; i--) {
    if (!needs_close[i]) continue;
    body_depth--;
    for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
    fputs("}\n", fp);
  }
  return 1;
}

// Conv vectorized-M substitution: replace each UOP_RANGE leaf at axis_id
// `up_axes[i]` with the integer constant `up_vals[i]`.  Used by the
// register-blocked reduce emit paths to vectorize the matmul output axes -- N
// separate accumulators sharing one conv-input load per reduce iteration
// (or, for multi-axis UPCAST, two register-blocking dimensions).
//
// Multi-axis form: when both cOut and wOut are UPCAST'd, we get an Um*Uw
// rectangle of accumulators.  Each (kU_cOut, kU_wOut) pair makes one
// statement; the MSL compiler CSEs the weight load (only cOut-dep) and the
// conv-input load (only wOut-dep) across the rectangle so the inner reduce
// body does Um*Uw MAD ops with just Um + Uw loads, not Um*Uw loads.
#define RMU_CONV_UPCAST_MAX 4
typedef struct {
  u32 n;
  u32 up_axes[RMU_CONV_UPCAST_MAX];
  u32 up_vals[RMU_CONV_UPCAST_MAX];
} RmuConvMSubstCtx;
static Term rmu_conv_m_subst_rule(Term t, void *user) {
  RmuConvMSubstCtx *cx = (RmuConvMSubstCtx *)user;
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_RANGE) return 0;
  u32 axis_id = (u32)term_val(heap_read(term_val(t) + 0));
  for (u32 i = 0; i < cx->n; i++) {
    if (cx->up_axes[i] == axis_id) return uop_const(DT_INT32, cx->up_vals[i]);
  }
  return 0;
}

// Inner-of-reduce-decomposition detector.  After hand_opts applies
// KOP_UPCAST/UNROLL to the reduce axis, uop_dag_apply_split rewrites
// the original K leaf into IADD(IMUL(outer, k), OPT(inner, UPCAST, k))
// where `outer` keeps the original axis_id and `inner` gets a fresh
// axis_id (red_axis+1).  The REDUCE node still names only `outer`,
// so the renderer's rmu_split_reduce pulls `outer` into red_range
// but leaves `inner` in out_ranges -- where the renderer would
// (incorrectly) treat it as an output dimension that gets a loop
// OUTSIDE the reduce, overwriting the per-output accumulator with
// per-(a8) partial sums (or, worse with MAX_DIM=8 cap, drop `inner`
// entirely and reference undeclared `a8`).
//
// Correct semantics: inner is an UPCAST/UNROLL-flagged extra reduce
// axis that the REDUCE forgot to list (because hand_opts edits the
// RANGE leaves but not the REDUCE.axes tuple for the split's inner
// fresh id).  Tinygrad models this directly via expander.py:do_expand
// (UNROLL'd RANGE -> CONTRACT(vec) of body, REDUCE then horizontally
// reduces both the range axis AND the contract vector;
// codegen/late/expander.py:116-125 fix_reduce_unroll handles it).
// Our equivalent: emit a nested `#pragma unroll` for-loop over the
// inner axis INSIDE the reduce-axis loop, before the combine, so all
// k inner values fold into the same accumulator.
//
// Returns 1 if `range_term` is an inner-K-decomp axis (axis NOT in
// addr) AND was marked UPCAST/UNROLL by hand_opts.
static int rmu_range_is_inner_reduce_decomp(Term range_term, u32 opt_kind,
                                            Term addr) {
  if (term_tag(range_term) != TAG_UOP || term_ext(range_term) != UOP_RANGE) return 0;
  if (opt_kind != UOP_OPT_UPCAST && opt_kind != UOP_OPT_UNROLL) return 0;
  u32 axis_id = (u32)term_val(heap_read(term_val(range_term) + 0));
  // True output axes appear in addr (the store-position index tree).
  // Reduce-decomp inner axes don't (they only live inside red_src).
  // Use the RMU_MAX_RANGES-cap variant: a UPCAST/LOCAL split of an
  // output axis surfaces BOTH the outer (in addr) and the inner (an
  // OPT-wrapped fresh leaf) so addr range count can exceed MAX_DIM
  // when several output axes are split.
  Term addr_ranges[RMU_MAX_RANGES];
  u32  addr_kinds  [RMU_MAX_RANGES] = {0};
  u32  addr_factors[RMU_MAX_RANGES] = {0};
  u32  addr_n = 0;
  rmu_collect_ranges_rec_cap(addr, addr_ranges, addr_kinds, addr_factors,
                             &addr_n, RMU_MAX_RANGES, RMU_NO_OPT, 0);
  for (u32 i = 0; i < addr_n; i++) {
    if (term_tag(addr_ranges[i]) != TAG_UOP
        || term_ext(addr_ranges[i]) != UOP_RANGE) continue;
    u32 aid = (u32)term_val(heap_read(term_val(addr_ranges[i]) + 0));
    if (aid == axis_id) return 0;  // axis indexes the output -> real output
  }
  return 1;
}

// Partition out_ranges in place: shift inner-reduce-decomposition
// ranges to the end of the array and report the split point.  Returns
// the count of TRUE output ranges (axes that index the store
// position).  The tail (n_out - returned_count) entries are the
// reduce-decomp inner axes; rmu_emit_store_reduce opens them
// inside the reduce-axis loop with #pragma unroll.
static u32 rmu_partition_out_ranges(Term *out_ranges, u32 *out_kinds,
                                    u32 *out_factors, u32 n_out, Term addr) {
  u32 head = 0;  // count of true outputs at the front
  for (u32 i = 0; i < n_out; i++) {
    if (rmu_range_is_inner_reduce_decomp(out_ranges[i], out_kinds[i], addr)) {
      continue;  // leave reduce-decomp at slot i; sweep them to tail below
    }
    if (head != i) {
      Term tr = out_ranges[head]; out_ranges[head] = out_ranges[i]; out_ranges[i] = tr;
      u32  tk = out_kinds  [head]; out_kinds  [head] = out_kinds  [i]; out_kinds  [i] = tk;
      u32  tf = out_factors[head]; out_factors[head] = out_factors[i]; out_factors[i] = tf;
    }
    head++;
  }
  return head;
}

// Emit inner-reduce-decomposition ranges as nested #pragma unroll
// for-loops at `depth`, returning the new depth (depth + count).
// Caller closes the loops symmetrically after the combine.
static u32 rmu_emit_inner_reduce_decomp_loops(Term const *ranges,
                                              u32 const *kinds,
                                              u32 const *factors,
                                              u32 n, FILE *fp, u32 depth) {
  (void)kinds;  // axis_type carries everything we need; kinds is for symmetry
  for (u32 i = 0; i < n; i++) {
    Term r = ranges[i];
    if (term_tag(r) != TAG_UOP || term_ext(r) != UOP_RANGE) continue;
    u32 ext = (u32)term_val(heap_read(term_val(r) + 2));
    // An explicit OPT(UNROLL) factor emits its pragma on EVERY target,
    // matching the reduce-axis UNROLL path -- otherwise a TOpt[UNROLL]
    // that splits a reduce axis silently drops its pragma on C (the
    // unrolled factor lands here, not on the reduce-axis loop).  The
    // default small-K unroll (factors[i]==0) stays non-C: clang -O3
    // already unrolls small constant-trip loops, so a pragma is noise.
    if (factors[i]) {
      for (u32 d = 0; d < depth; d++) fputs("  ", fp);
      rmu_emit_unroll_pragma(fp, factors[i]);
    } else if (RMU_TARGET != CG_TARGET_C && ext <= RMU_REDUCE_UNROLL_MAX) {
      for (u32 d = 0; d < depth; d++) fputs("  ", fp);
      rmu_emit_unroll_pragma(fp, ext);
    }
    rmu_emit_range_open(r, fp, depth, RMU_NO_OPT);
    depth++;
  }
  return depth;
}

// Chain-reduce emission: STORE(buf, addr, REDUCE(REDUCE(...REDUCE(leaf,
// kind_n, axis_n)..., kind_1, axis_1), kind_0, axis_0)) where every link
// is a plain UOP_REDUCE (no OPT wrapping in the chain) and the leaf body
// contains no REDUCE.  Each chain link gets its own accumulator
// reinitialised inside the enclosing link's loop; combines bubble
// inner_acc up into outer_acc.  Returns 1 on emit, 0 to fall through.
static int rmu_emit_chain_reduce(Term store, FILE *fp, u32 depth) {
  if (term_tag(store) != TAG_UOP || term_ext(store) != UOP_STORE) return 0;
  u64 sloc = term_val(store);
  Term value = heap_read(sloc + 2);
  Term chain[MAX_DIM];
  u32  n_chain = 0;
  Term cur = value;
  while (term_tag(cur) == TAG_UOP && term_ext(cur) == UOP_REDUCE
         && n_chain < MAX_DIM) {
    chain[n_chain++] = cur;
    cur = heap_read(term_val(cur) + 0);
  }
  if (n_chain < 2) return 0;
  Term leaf = cur;
  if (rmu_term_has_reduce(leaf, 0)) return 0;

  Term buf  = heap_read(sloc + 0);
  Term addr = heap_read(sloc + 1);

  Term ranges[MAX_DIM];
  u32  opt_kinds[MAX_DIM]   = {0};
  u32  opt_factors[MAX_DIM] = {0};
  u32  n_ranges = 0;
  rmu_collect_ranges_with_opts(addr, ranges, opt_kinds, opt_factors, &n_ranges);
  rmu_collect_ranges_with_opts(leaf, ranges, opt_kinds, opt_factors, &n_ranges);

  u32 chain_axes[MAX_DIM];
  for (u32 i = 0; i < n_chain; i++) {
    chain_axes[i] = term_val(heap_read(term_val(chain[i]) + 2));
  }
  Term out_ranges[MAX_DIM];
  u32  out_kinds[MAX_DIM]   = {0};
  u32  out_factors[MAX_DIM] = {0};
  u32  n_out = 0;
  Term red_range_per_chain    [MAX_DIM] = {0};
  u32  red_kind_opt_per_chain [MAX_DIM];
  for (u32 i = 0; i < MAX_DIM; i++) red_kind_opt_per_chain[i] = RMU_NO_OPT;
  for (u32 i = 0; i < n_ranges; i++) {
    if (term_tag(ranges[i]) != TAG_UOP
        || term_ext(ranges[i]) != UOP_RANGE) continue;
    u32 axis_id = term_val(heap_read(term_val(ranges[i]) + 0));
    int is_chain = 0;
    u32 chain_pos = 0;
    for (u32 c = 0; c < n_chain; c++) {
      if (chain_axes[c] == axis_id) { is_chain = 1; chain_pos = c; break; }
    }
    if (is_chain) {
      red_range_per_chain    [chain_pos] = ranges[i];
      red_kind_opt_per_chain [chain_pos] = opt_kinds[i];
    } else {
      out_ranges  [n_out] = ranges[i];
      out_kinds   [n_out] = opt_kinds[i];
      out_factors [n_out] = opt_factors[i];
      n_out++;
    }
  }
  for (u32 i = 0; i < n_chain; i++) {
    if (red_range_per_chain[i] == 0) return 0;
  }

  int needs_close[MAX_DIM] = {0};
  u32 body_depth = rmu_emit_output_loops(addr, out_ranges, out_kinds,
                                          out_factors, n_out, depth, fp,
                                          needs_close);

  char acc_names[MAX_DIM][32];
  u32  red_kinds[MAX_DIM];
  for (u32 i = 0; i < n_chain; i++) {
    u64 rloc_i = term_val(chain[i]);
    red_kinds[i] = term_val(heap_read(rloc_i + 1));
    u32 ax = term_val(heap_read(rloc_i + 2));
    snprintf(acc_names[i], sizeof(acc_names[i]), "_acc%u", ax);
  }
  u32 cur_depth = body_depth;
  for (u32 i = 0; i < n_chain; i++) {
    for (u32 d = 0; d < cur_depth; d++) fputs("  ", fp);
    fprintf(fp, "float %s = ", acc_names[i]);
    rmu_emit_reduce_init(red_kinds[i], fp);
    fputs(";\n", fp);
    // GPU-generic: small-extent reduce unroll for Metal AND CUDA.
    if (red_kind_opt_per_chain[i] == RMU_NO_OPT && RMU_TARGET != CG_TARGET_C) {
      u32 red_extent = uop_range_extent(red_range_per_chain[i]);
      if (red_extent > 0 && red_extent <= RMU_REDUCE_UNROLL_MAX) {
        for (u32 d = 0; d < cur_depth; d++) fputs("  ", fp);
        fprintf(fp, "#pragma unroll(%u)\n", red_extent);
      }
    }
    rmu_emit_range_open(red_range_per_chain[i], fp, cur_depth,
                        red_kind_opt_per_chain[i]);
    cur_depth++;
  }
  for (u32 d = 0; d < cur_depth; d++) fputs("  ", fp);
  rmu_emit_reduce_combine(acc_names[n_chain - 1], red_kinds[n_chain - 1],
                          leaf, fp);
  for (i32 i = (i32)n_chain - 1; i > 0; i--) {
    cur_depth--;
    for (u32 d = 0; d < cur_depth; d++) fputs("  ", fp);
    fputs("}\n", fp);
    for (u32 d = 0; d < cur_depth; d++) fputs("  ", fp);
    if (red_kinds[i - 1] == REDUCE_MAX) {
      fprintf(fp, "%s = fmax(%s, %s);\n",
              acc_names[i - 1], acc_names[i - 1], acc_names[i]);
    } else {
      fprintf(fp, "%s = %s + %s;\n",
              acc_names[i - 1], acc_names[i - 1], acc_names[i]);
    }
  }
  cur_depth--;
  for (u32 d = 0; d < cur_depth; d++) fputs("  ", fp);
  fputs("}\n", fp);

  for (u32 d = 0; d < cur_depth; d++) fputs("  ", fp);
  fprintf(fp, "%s[", rmu_buf_name(buf));
  rmu_emit_term(addr, fp);
  fputs("] = ", fp);
  rmu_store_cast_open(buf, fp);
  fputs(acc_names[0], fp);
  rmu_store_cast_close(buf, fp);
  fputs(";\n", fp);

  for (i32 i = (i32)n_out - 1; i >= 0; i--) {
    if (!needs_close[i]) continue;
    body_depth--;
    for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
    fputs("}\n", fp);
  }
  return 1;
}

// Does the subtree rooted at `t` contain any UOP_RANGE leaf whose
// axis_id is listed in `up_axes[0..n_up)`?  Used by the parallel-acc
// emit path (path #2 of docs/tinygrad_late_passes.md) to classify
// subexpressions as "shared across all F register-blocked lanes"
// (returns 0 -- safe to hoist once before the F MADs) versus
// "per-lane" (returns 1 -- must stay in the per-k MAD body).
//
// Walks UOP_RANGE / UOP_OPT / generic UOp children; treats UOP_BUFFER /
// UOP_CONST / UOP_INVALID / TAG_NUM as leaves with no axis dependence.
static int rmu_term_depends_on_upcast(Term t, u32 const *up_axes, u32 n_up) {
  if (n_up == 0) return 0;
  if (term_tag(t) != TAG_UOP) return 0;
  Term stack[256];
  u32  sp = 0;
  stack[sp++] = t;
  while (sp > 0) {
    Term cur = stack[--sp];
    if (term_tag(cur) != TAG_UOP) continue;
    u32 op = term_ext(cur);
    if (op == UOP_RANGE) {
      u32 aid = (u32)term_val(heap_read(term_val(cur) + 0));
      for (u32 i = 0; i < n_up; i++) {
        if (up_axes[i] == aid) return 1;
      }
      continue;
    }
    if (op == UOP_BUFFER || op == UOP_CONST || op == UOP_INVALID) continue;
    if (op == UOP_OPT) {
      Term tgt = heap_read(term_val(cur) + 0);
      if (term_tag(tgt) == TAG_UOP && sp < 256) stack[sp++] = tgt;
      continue;
    }
    u8 ar = uop_arity(op);
    u64 loc = term_val(cur);
    for (u8 i = 0; i < ar && i < MAX_UOP_SRC && sp < 256; i++) {
      Term child = heap_read(loc + i);
      if (term_tag(child) == TAG_UOP) stack[sp++] = child;
    }
  }
  return 0;
}

// Walk red_src and register every UOP_INDEX_E subterm whose address
// does NOT depend on any UPCAST'd axis as a "shared load": one local
// variable, emitted once before the F MAD lines, reused across all F
// lanes.  Returns the number of distinct shared loads installed in the
// global hoist map.  Caller (rmu_emit_store_reduce parallel-acc branch)
// emits the declarations and calls rmu_hoist_clear afterwards.
//
// Naming: `_sh<red_axis>_<idx>` (idx is 0-based in walk order).
//
// This is the source-level equivalent of tinygrad's
// codegen/late/devectorizer.py:81-117 fold_expanded_index, which folds
// adjacent INDEX nodes into one wider CAT'd INDEX + per-lane GEP at the
// UOp-graph level.  thvm has no full UOp-graph expander so we do the
// shared-load amortisation at source-emission time.  Without this the
// F per-lane MADs each re-emit the shared `in<k>[...]` load verbatim;
// nvcc CSEs them eventually but at huge compile cost on V100 (cold
// step 1 was 349 s with path #1 alone; warm wall regressed to 3541 ms
// from a 540 ms baseline).
static u32 rmu_hoist_shared_loads(Term red_src,
                                  u32 const *up_axes, u32 n_up,
                                  u32 red_axis,
                                  char *name_pool, u32 name_pool_cap,
                                  FILE *fp, u32 indent_depth) {
  if (n_up == 0) return 0;
  Term stack[256];
  u32  sp = 0;
  if (term_tag(red_src) != TAG_UOP) return 0;
  stack[sp++] = red_src;
  // Dedupe seen Terms across the walk so we visit each INDEX_E once.
  Term seen[RMU_HOIST_MAX];
  u32  n_seen = 0;
  u32  n_hoist = 0;
  u32  pool_used = 0;
  while (sp > 0) {
    Term cur = stack[--sp];
    if (term_tag(cur) != TAG_UOP) continue;
    u32 op = term_ext(cur);
    if (op == UOP_BUFFER || op == UOP_CONST || op == UOP_INVALID
        || op == UOP_RANGE) {
      continue;
    }
    if (op == UOP_INDEX_E) {
      // Already considered?
      int dup = 0;
      for (u32 i = 0; i < n_seen; i++) {
        if (seen[i] == cur) { dup = 1; break; }
      }
      if (!dup && n_seen < RMU_HOIST_MAX) seen[n_seen++] = cur;
      if (dup) continue;
      // Only the addr matters for dependence: the buffer pointer is
      // constant.
      Term addr = heap_read(term_val(cur) + 1);
      Term buf  = heap_read(term_val(cur) + 0);
      if (rmu_term_depends_on_upcast(addr, up_axes, n_up)) {
        // Per-lane load; can't hoist.  Descend into addr in case a
        // nested INDEX_E inside (rare) is shared.
        if (term_tag(addr) == TAG_UOP && sp < 256) stack[sp++] = addr;
        continue;
      }
      // Shared: install in the hoist map and emit one declaration.
      // Skip if the buffer is unresolved (rmu_buf_name_or_null == NULL):
      // INDEX_E with NULL buf falls through to the `0.0f` literal
      // semantics in rmu_emit_term; hoisting a literal would just be a
      // const float local with no benefit, and would prematurely freeze
      // the literal across the F MADs.
      const char *bn = rmu_buf_name_or_null(buf);
      if (bn == NULL) continue;
      if (n_hoist >= RMU_HOIST_MAX) continue;
      // Compute the name and reserve pool space.
      char tmp[32];
      int  tn = snprintf(tmp, sizeof(tmp), "_sh%u_%u", red_axis, n_hoist);
      if (tn <= 0 || (u32)tn + 1 > name_pool_cap - pool_used) continue;
      char *slot_name = name_pool + pool_used;
      memcpy(slot_name, tmp, (u32)tn + 1);
      pool_used += (u32)tn + 1;
      if (!rmu_hoist_add(cur, slot_name)) continue;
      // Emit:  float _sh<r>_<i> = in<k>[<addr>];
      for (u32 d = 0; d < indent_depth; d++) fputs("  ", fp);
      fprintf(fp, "float %s = %s[", slot_name, bn);
      rmu_emit_term(addr, fp);
      fputs("];\n", fp);
      n_hoist++;
      continue;
    }
    if (op == UOP_OPT) {
      Term tgt = heap_read(term_val(cur) + 0);
      if (term_tag(tgt) == TAG_UOP && sp < 256) stack[sp++] = tgt;
      continue;
    }
    u8 ar = uop_arity(op);
    u64 loc = term_val(cur);
    for (u8 i = 0; i < ar && i < MAX_UOP_SRC && sp < 256; i++) {
      Term child = heap_read(loc + i);
      if (term_tag(child) == TAG_UOP) stack[sp++] = child;
    }
  }
  return n_hoist;
}

// Read an integer CONST Term's value.  Returns 1 + value when t is an
// integer UOP_CONST, else 0.  (Mirrors index_simplify.c's uop_iconst_value,
// which is static in a later TU.)
static int rmu_const_int(Term t, i64 *out) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_CONST) return 0;
  Term num = heap_read(term_val(t) + 0);
  if (term_tag(num) != TAG_NUM) return 0;
  if (!dtype_is_int(term_ext(num))) return 0;
  *out = (i64)(i32)term_val(num);  // DT_INT32 sign-extension
  return 1;
}

// Does `t` reference `axis_id` anywhere (a RANGE leaf with that id)?
// UOP_OPT (an UPCAST/UNROLL/LOCAL annotation wrapping a RANGE) is
// transparent -- only its target (slot 0) is a recursable child.
static int rmu_term_refs_axis(Term t, u32 axis_id) {
  if (term_tag(t) != TAG_UOP) return 0;
  u32 op = term_ext(t);
  if (op == UOP_RANGE)
    return (u32)term_val(heap_read(term_val(t) + 0)) == axis_id;
  if (op == UOP_CONST || op == UOP_BUFFER || op == UOP_INVALID) return 0;
  if (op == UOP_OPT)
    return rmu_term_refs_axis(heap_read(term_val(t) + 0), axis_id);
  u8 ar = uop_arity(op);
  u64 loc = term_val(t);
  for (u8 i = 0; i < ar && i < MAX_UOP_SRC; i++)
    if (rmu_term_refs_axis(heap_read(loc + i), axis_id)) return 1;
  return 0;
}

// Is `op` an integer (index-space) UOp?  Used to pick the `int` vs `float`
// type for a hoisted lane-invariant local.
static int rmu_op_is_int(u32 op) {
  switch (op) {
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL: case UOP_IDIV:
    case UOP_IMOD: case UOP_ILT:  case UOP_IAND: case UOP_IOR:
    case UOP_IXOR: case UOP_ISHR: case UOP_IWHERE: case UOP_INVALID:
    case UOP_RANGE:
      return 1;
    default:
      return 0;
  }
}

// Is `t` a "worth hoisting" non-leaf node?  Bare RANGE/CONST/INVALID/
// BUFFER leaves are not worth a local (they emit as a single token); a
// single INDEX_E load IS worth it (the shared-load amortisation), and any
// compound integer-mask subtree (IWHERE/IAND/ILT/IDIV/IMOD chain) is worth
// it because the lane fan-out replicates it N times.
static int rmu_hoist_worth(Term t) {
  if (term_tag(t) != TAG_UOP) return 0;
  u32 op = term_ext(t);
  if (op == UOP_INDEX_E) return 1;
  switch (op) {
    case UOP_IWHERE: case UOP_IAND: case UOP_IOR: case UOP_IXOR:
    case UOP_ILT:    case UOP_IDIV: case UOP_IMOD: case UOP_ISHR:
    case UOP_IADD:   case UOP_ISUB: case UOP_IMUL:
    case UOP_ADD:    case UOP_MUL:  case UOP_CMPLT: case UOP_CMPEQ:
      return 1;
    default:
      return 0;
  }
}

// Generalised lane-invariant hoist (Lever 2).  In the lane-block reduce
// combine, the per-lane fan-out re-emits the ENTIRE rewritten subtree per
// lane k -- so any maximal subterm of `red_src` that does NOT depend on a
// lane (UPCAST) axis is emitted N times verbatim, N-1 of them redundant.
// The dominant cost is the col2im/_pool validity-mask chain (IWHERE/ILT/
// IDIV/IMOD over the row half `oh = a_out + a_reduce*stride`, which is
// lane-INVARIANT) replicated 16x.  This walks red_src top-down and, for
// each MAXIMAL subterm that (a) is worth hoisting (rmu_hoist_worth) and
// (b) references NO lane axis (rmu_term_depends_on_upcast == 0), emits ONE
// `int`/`float` local and registers it in the hoist map; rmu_emit_term
// then short-circuits every per-lane occurrence to that local name.
//
// Maximal: a lane-invariant node is hoisted whole and NOT descended (its
// entire subtree is invariant); a lane-VARIANT node is descended so its
// invariant sub-subtrees (e.g. the oh-half of a mixed oh/ow mask) hoist.
// rmu_lane_subst's uop_graph_rewrite is identity on subtrees containing no
// lane axis (hash-consed rebuild preserves Term identity), so the map keyed
// on the ORIGINAL red_src matches every per-lane rewrite result -- the same
// invariant the conv parallel-acc hoist relies on.
//
// This is the source-emission analogue of tinygrad codegen/late/
// expander.py:do_expand, which only replicates UOps that depend on the
// expand axis and leaves lane-invariant UOps as a single shared node.
// Every RANGE axis in `t` a member of allowed[0..n_allowed)?  Used to keep
// the hoist from lifting a subterm that references an axis opened DEEPER
// than the hoist point (e.g. a nested reduce's K-axis) -- that local would
// reference an undeclared `aN`.
static int rmu_term_axes_in_set(Term t, u32 const *allowed, u32 n_allowed) {
  if (term_tag(t) != TAG_UOP) return 1;
  u32 op = term_ext(t);
  if (op == UOP_RANGE) {
    u32 aid = (u32)term_val(heap_read(term_val(t) + 0));
    for (u32 i = 0; i < n_allowed; i++) if (allowed[i] == aid) return 1;
    return 0;
  }
  if (op == UOP_CONST || op == UOP_BUFFER || op == UOP_INVALID) return 1;
  if (op == UOP_OPT)
    return rmu_term_axes_in_set(heap_read(term_val(t) + 0), allowed, n_allowed);
  u8 ar = uop_arity(op);
  u64 loc = term_val(t);
  for (u8 i = 0; i < ar && i < MAX_UOP_SRC; i++)
    if (!rmu_term_axes_in_set(heap_read(loc + i), allowed, n_allowed)) return 0;
  return 1;
}

static u32 rmu_hoist_lane_invariant(Term t,
                                    u32 const *up_axes, u32 n_up,
                                    u32 red_axis,
                                    u32 const *scope_axes, u32 n_scope,
                                    char *name_pool, u32 name_pool_cap,
                                    u32 *pool_used, u32 *n_hoist,
                                    FILE *fp, u32 indent_depth) {
  if (n_up == 0 || term_tag(t) != TAG_UOP) return 0;
  u32 op = term_ext(t);
  if (op == UOP_BUFFER || op == UOP_CONST || op == UOP_INVALID
      || op == UOP_RANGE)
    return 0;
  // Already hoisted (a shared subtree reached twice)?  Skip.
  if (rmu_hoist_lookup(t) != NULL) return 0;
  if (rmu_hoist_worth(t) && !rmu_term_depends_on_upcast(t, up_axes, n_up)
      && rmu_term_axes_in_set(t, scope_axes, n_scope)) {
    // Maximal lane-invariant subtree: hoist it whole.
    if (*n_hoist >= RMU_HOIST_MAX) return 0;
    const char *ty = rmu_op_is_int(op) ? "int" : "float";
    char tmp[40];
    int tn = snprintf(tmp, sizeof(tmp), "_sh%u_%u", red_axis, *n_hoist);
    if (tn <= 0 || (u32)tn + 1 > name_pool_cap - *pool_used) return 0;
    char *slot_name = name_pool + *pool_used;
    memcpy(slot_name, tmp, (u32)tn + 1);
    // Emit the RHS FIRST (before the key is in the map) so rmu_emit_term
    // renders the full subtree -- not a self-referential `_shN = _shN;`.
    for (u32 d = 0; d < indent_depth; d++) fputs("  ", fp);
    fprintf(fp, "%s %s = ", ty, slot_name);
    rmu_emit_term(t, fp);
    fputs(";\n", fp);
    if (!rmu_hoist_add(t, slot_name)) return 0;
    *pool_used += (u32)tn + 1;
    (*n_hoist)++;
    return 1;
  }
  // Lane-variant (or not-worth, or out-of-scope) node: descend to find
  // invariant subtrees.  UOP_OPT is transparent.
  u64 loc = term_val(t);
  if (op == UOP_OPT) {
    rmu_hoist_lane_invariant(heap_read(loc + 0), up_axes, n_up, red_axis,
                             scope_axes, n_scope,
                             name_pool, name_pool_cap, pool_used, n_hoist,
                             fp, indent_depth);
    return 0;
  }
  // Do NOT descend into a nested UOP_REDUCE: its body opens its own K-loop
  // DEEPER than this hoist point, so any local lifted from inside it would
  // reference an axis not yet declared here.  The nested reduce hoists its
  // own invariants when it emits (it is itself a lane-block reduce).
  if (op == UOP_REDUCE) return 0;
  u8 ar = uop_arity(op);
  for (u8 i = 0; i < ar && i < MAX_UOP_SRC; i++)
    rmu_hoist_lane_invariant(heap_read(loc + i), up_axes, n_up, red_axis,
                             scope_axes, n_scope,
                             name_pool, name_pool_cap, pool_used, n_hoist,
                             fp, indent_depth);
  return 0;
}

// Affine-coefficient extractor: compute the integer coefficient of
// `axis_id` in the affine index expression `t`.  Returns 1 + writes *coeff
// when `t` is affine in axis_id over the {IADD,ISUB,IMUL-by-const,RANGE,
// CONST} index grammar; returns 0 when axis_id appears under a non-affine
// op (IDIV/IMOD/IWHERE/ILT/... -> a gather or data-dependent index) so the
// caller falls back to the scalar store.  A subtree NOT referencing axis_id
// contributes coefficient 0.  This is the structural equivalent of probing
// addr[axis+1]-addr[axis], without relying on the int simplifier to cancel
// two large symbolic trees.
static int rmu_axis_affine_coeff(Term t, u32 axis_id, i64 *coeff) {
  if (term_tag(t) != TAG_UOP) { *coeff = 0; return 1; }
  u32 op = term_ext(t);
  if (op == UOP_RANGE) {
    *coeff = ((u32)term_val(heap_read(term_val(t) + 0)) == axis_id) ? 1 : 0;
    return 1;
  }
  if (op == UOP_CONST || op == UOP_BUFFER || op == UOP_INVALID) {
    *coeff = 0; return 1;
  }
  u64 loc = term_val(t);
  // UOP_OPT (UPCAST/UNROLL/LOCAL annotation) is transparent: the lane
  // axis RANGE is wrapped in an OPT that carries no arithmetic -- recurse
  // into its target (slot 0) with the coefficient unchanged.
  if (op == UOP_OPT)
    return rmu_axis_affine_coeff(heap_read(loc + 0), axis_id, coeff);
  if (op == UOP_IADD || op == UOP_ISUB) {
    i64 ca, cb;
    if (!rmu_axis_affine_coeff(heap_read(loc + 0), axis_id, &ca)) return 0;
    if (!rmu_axis_affine_coeff(heap_read(loc + 1), axis_id, &cb)) return 0;
    *coeff = (op == UOP_IADD) ? (ca + cb) : (ca - cb);
    return 1;
  }
  if (op == UOP_IMUL) {
    Term a = heap_read(loc + 0), b = heap_read(loc + 1);
    i64 av, bv;
    int a_const = rmu_const_int(a, &av);
    int b_const = rmu_const_int(b, &bv);
    // axis must sit on exactly one side of a const*var product to stay
    // affine; var*var with axis inside is non-affine.
    if (a_const && !rmu_term_refs_axis(b, axis_id)) { *coeff = 0; return 1; }
    if (b_const && !rmu_term_refs_axis(a, axis_id)) { *coeff = 0; return 1; }
    if (a_const) {
      i64 cb;
      if (!rmu_axis_affine_coeff(b, axis_id, &cb)) return 0;
      *coeff = av * cb; return 1;
    }
    if (b_const) {
      i64 ca;
      if (!rmu_axis_affine_coeff(a, axis_id, &ca)) return 0;
      *coeff = bv * ca; return 1;
    }
    // Neither side const: affine only if axis appears in neither side.
    if (!rmu_term_refs_axis(t, axis_id)) { *coeff = 0; return 1; }
    return 0;
  }
  // Any other op (IDIV/IMOD/IWHERE/ILT/IAND/...): affine only if axis_id
  // does not appear inside it at all (then coeff 0); otherwise non-affine.
  if (!rmu_term_refs_axis(t, axis_id)) { *coeff = 0; return 1; }
  return 0;
}

// Index stride of `addr` along `axis_id`: the affine coefficient of that
// axis.  Returns 1 + writes *stride when affine; 0 for a non-affine
// (gather) index -> not float4-vectorizable.
static int rmu_axis_index_stride(Term addr, u32 axis_id, i64 *stride) {
  return rmu_axis_affine_coeff(addr, axis_id, stride);
}

// REDUCE-shaped emission: STORE(buf, addr, REDUCE(src, kind, axis)).
// Hoists an accumulator outside the reduce-axis loop and references it
// in the store statement.  Returns 1 if the shape matched and was
// emitted; 0 if the caller should fall back to the generic path.
//
// Dispatches to the simdgroup_matrix template when the recogniser
// pre-pass (uop_recognise_tc) wrapped the value:
//   OPT(_, TC, _) -> rmu_emit_matmul_tc  (simdgroup_matrix MMA)
// TC falls through to the scalar accumulator on tile-size mismatch
// (K%8 != 0).  Conv2d flows through the generic accumulator path
// below -- the flattened STORE(REDUCE(MUL(INDEX_E(W),X))) shape is
// handled correctly there with no conv-specialised template.
static int rmu_emit_store_reduce(Term store, FILE *fp, u32 depth) {
  if (term_tag(store) != TAG_UOP || term_ext(store) != UOP_STORE) return 0;
  u64 sloc = term_val(store);
  Term value = heap_read(sloc + 2);
  // F2b: dispatch to simdgroup_matrix template when matmul-shaped AND
  // tile dims fit; falls back to F1e accumulator otherwise.
  Term tc_red = 0;
  if (rmu_detect_matmul_tc(store, &tc_red)) {
    if (rmu_emit_matmul_tc(store, tc_red, fp, depth)) return 1;
    // Fall through to accumulator path.
    for (u32 d = 0; d < depth; d++) fputs("  ", fp);
    fputs("/* TC tile mismatch: falling back to scalar accumulator */\n", fp);
    value = tc_red;
  }
  if (term_tag(value) != TAG_UOP || term_ext(value) != UOP_REDUCE) return 0;
  Term buf  = heap_read(sloc + 0);
  Term addr = heap_read(sloc + 1);
  u64 rloc      = term_val(value);
  Term red_src  = heap_read(rloc + 0);
  // REDUCE(REDUCE(...)) chain: dispatch to chain-reduce template so each
  // chain link's accumulator reinitialises inside the enclosing link's
  // loop.  Falls through to the generic store path only when the body
  // contains a non-immediate nested reduce (e.g. mean/var-of-reductions
  // inside an elementwise body) -- the generic post-order hoist handles
  // those because the inner accumulator is referenced as a sibling
  // expression, not as the outer's reduce-body directly.
  if (term_tag(red_src) == TAG_UOP && term_ext(red_src) == UOP_REDUCE) {
    if (rmu_emit_chain_reduce(store, fp, depth)) return 1;
  }
  if (rmu_term_has_reduce(red_src, 0)) return 0;
  // emit_store_reduce now handles SINGLE-axis AND multi-axis REDUCE.
  // Multi-axis: collect every reduce axis_id; rmu_split_reduce_multi
  // peels them all out of the range set; the parallel-acc / legacy
  // emit opens them as nested loops in REDUCE.src[1:] order
  // (outermost axis_0, innermost axis_{n-1}; mirrors tinygrad
  // lowerer.py).
  u32  red_kind   = uop_reduce_kind(value);
  u32  red_n_axes = uop_reduce_n_axes(value);
  if (red_n_axes == 0 || red_n_axes > MAX_DIM) return 0;
  u32  red_axes[MAX_DIM];
  for (u32 ai = 0; ai < red_n_axes; ai++) {
    red_axes[ai] = uop_reduce_axis(value, ai);
  }
  u32  red_axis = red_axes[0];     // legacy alias used for accumulator name

  // Collect ranges from addr + red_src; split into output vs reduce.
  // RMU_MAX_RANGES cap (not MAX_DIM): hand_opts can split BOTH the K
  // reduce axis and several output axes via UPCAST/LOCAL/UNROLL,
  // surfacing 2*rank leaves -- the inner half of each split lives as
  // a fresh RANGE alongside the outer.  Truncating at MAX_DIM=8 drops
  // late leaves silently and yields MSL with references to undeclared
  // identifiers.
  Term ranges[RMU_MAX_RANGES];
  u32  opt_kinds  [RMU_MAX_RANGES] = {0};
  u32  opt_factors[RMU_MAX_RANGES] = {0};
  u32  n_ranges = 0;
  rmu_collect_ranges_rec_cap(addr,    ranges, opt_kinds, opt_factors,
                             &n_ranges, RMU_MAX_RANGES, RMU_NO_OPT, 0);
  rmu_collect_ranges_rec_cap(red_src, ranges, opt_kinds, opt_factors,
                             &n_ranges, RMU_MAX_RANGES, RMU_NO_OPT, 0);

  // Multi-axis split: peel every reduce-axis leaf out of the range
  // set in REDUCE.src[1:] order; tail of `out_ranges` is the true
  // output axes (still in their original ordering relative to ranges[]).
  Term out_ranges[RMU_MAX_RANGES];
  u32  out_kinds  [RMU_MAX_RANGES] = {0};
  u32  out_factors[RMU_MAX_RANGES] = {0};
  u32  n_out = 0;
  Term red_ranges       [MAX_DIM] = {0};
  u32  red_kind_opts    [MAX_DIM];
  u32  red_factor_opts  [MAX_DIM];
  for (u32 ai = 0; ai < red_n_axes; ai++) {
    red_kind_opts  [ai] = RMU_NO_OPT;
    red_factor_opts[ai] = 0;
  }
  for (u32 i = 0; i < n_ranges; i++) {
    if (term_tag(ranges[i]) != TAG_UOP || term_ext(ranges[i]) != UOP_RANGE) {
      out_ranges [n_out] = ranges[i];
      out_kinds  [n_out] = opt_kinds  [i];
      out_factors[n_out] = opt_factors[i];
      n_out++;
      continue;
    }
    u32 axis_id = (u32)term_val(heap_read(term_val(ranges[i]) + 0));
    int is_red = -1;
    for (u32 ai = 0; ai < red_n_axes; ai++) {
      if (red_axes[ai] == axis_id) { is_red = (int)ai; break; }
    }
    if (is_red >= 0) {
      red_ranges      [is_red] = ranges[i];
      red_kind_opts   [is_red] = opt_kinds  [i];
      red_factor_opts [is_red] = opt_factors[i];
    } else {
      out_ranges [n_out] = ranges[i];
      out_kinds  [n_out] = opt_kinds  [i];
      out_factors[n_out] = opt_factors[i];
      n_out++;
    }
  }
  for (u32 ai = 0; ai < red_n_axes; ai++) {
    // Missing reduce-axis leaf for any axis: bail to the generic path.
    if (red_ranges[ai] == 0) return 0;
  }

  // Partition out_ranges: true output axes vs inner-reduce-decomp
  // (UPCAST/UNROLL'd inner half of K split) which belong INSIDE the
  // reduce loop, NOT around it.  See rmu_range_is_inner_reduce_decomp.
  u32 n_out_true = rmu_partition_out_ranges(out_ranges, out_kinds,
                                            out_factors, n_out, addr);
  u32 n_inner    = n_out - n_out_true;
  // Legacy aliases for the single-axis-K branches below.
  Term red_range      = red_ranges[0];
  u32  red_kind_opt   = red_kind_opts[0];
  u32  red_factor_opt = red_factor_opts[0];

  // Parallel-accumulator path: when any true-output axis is UPCAST'd,
  // emit F register-blocked accumulators sharing ONE reduce loop --
  // the tinygrad expander.py:do_expand + devectorizer.py:reduce_to_acc
  // (lines 311-328) semantics.  Each UPCAST'd output range becomes F
  // literal indices substituted into the reduce body / store address,
  // never opened as a runtime for-loop.  The classic register-blocked
  // matmul: F MADs per K iter, with the K-iter's shared input load
  // amortised across all F accumulators (the MSL/nvcc compiler CSEs
  // the load once it sees F independent accumulators with overlapping
  // address arithmetic).
  //
  // Gates: register-blocking helps EVERY renderer including C (Lever B).
  // On Metal/CUDA the F independent accumulators feed ILP/occupancy; on
  // the C target they give clang independent reduction lanes (acc0[N])
  // so it can vectorize the FP reduce WITHOUT -ffast-math -- the scalar
  // `_acc` it could not reorder.  Other gates: single-K reduce with no
  // OPT-K-axis wrap (UNROLL/GROUP_REDUCE), no inner-reduce-decomp (the
  // inner-axis fold uses the SHARED accumulator and would conflict with
  // per-element accumulators).  When gated off, fall through to the
  // legacy single-acc / serial-UPCAST-loop emit.
  // Multi-axis-K reject: no GROUP_REDUCE on any reduce axis (the
  // GROUP_REDUCE shape is single-axis only) and the parallel-acc emit
  // can only open NON-OPT'd reduce loops (UNROLL is honoured; UPCAST
  // on a reduce axis is the "inner-reduce-decomp" pattern handled via
  // n_inner != 0, already gated below).
  int multi_axis_reject = 0;
  for (u32 ai = 0; ai < red_n_axes; ai++) {
    if (red_kind_opts[ai] == UOP_OPT_GROUP_REDUCE) {
      multi_axis_reject = 1; break;
    }
  }
  RmuConvMSubstCtx pa_up = {0};
  u32 pa_up_exts[RMU_CONV_UPCAST_MAX] = {0};
  u32 pa_total = 1;
  if (n_inner == 0 && !multi_axis_reject) {
    for (u32 i = 0; i < n_out_true && pa_up.n < RMU_CONV_UPCAST_MAX; i++) {
      if (out_kinds[i] != UOP_OPT_UPCAST) continue;
      Term r = out_ranges[i];
      if (term_tag(r) != TAG_UOP || term_ext(r) != UOP_RANGE) continue;
      u32 e  = (u32)term_val(heap_read(term_val(r) + 2));
      if (e < 2 || e > 16) continue;
      // Cap straight-line accumulator count at 32 (tinygrad's
      // upcast_size budget); skip the next UPCAST'd axis if it'd push
      // total over the cap.
      if ((u64)pa_total * e > 32) continue;
      pa_up.up_axes[pa_up.n] = (u32)term_val(heap_read(term_val(r) + 0));
      pa_up_exts[pa_up.n]    = e;
      pa_up.n++;
      pa_total *= e;
    }
  }
  if (pa_up.n > 0) {
    // Filtered output ranges (UPCAST'd axes removed) drive the runtime
    // loop / thread-bind emit.  The UPCAST'd axes become straight-line
    // accumulator indices, NOT for-loops.
    Term pa_f_ranges[RMU_MAX_RANGES];
    u32  pa_f_kinds  [RMU_MAX_RANGES] = {0};
    u32  pa_f_factors[RMU_MAX_RANGES] = {0};
    u32  pa_f_n = 0;
    for (u32 i = 0; i < n_out_true; i++) {
      Term r = out_ranges[i];
      int skip = 0;
      if (term_tag(r) == TAG_UOP && term_ext(r) == UOP_RANGE) {
        u32 ax = (u32)term_val(heap_read(term_val(r) + 0));
        for (u32 j = 0; j < pa_up.n; j++) {
          if (pa_up.up_axes[j] == ax) { skip = 1; break; }
        }
      }
      if (skip) continue;
      pa_f_ranges [pa_f_n] = r;
      pa_f_kinds  [pa_f_n] = out_kinds  [i];
      pa_f_factors[pa_f_n] = out_factors[i];
      pa_f_n++;
    }
    int pa_needs_close[RMU_MAX_RANGES] = {0};
    u32 pa_body_depth = rmu_emit_output_loops(addr, pa_f_ranges, pa_f_kinds,
                                              pa_f_factors, pa_f_n, depth,
                                              fp, pa_needs_close);
    char pa_acc_name[32];
    snprintf(pa_acc_name, sizeof(pa_acc_name), "_acc%u", red_axis);
    // F parallel accumulator declarations.
    for (u32 k = 0; k < pa_total; k++) {
      for (u32 d = 0; d < pa_body_depth; d++) fputs("  ", fp);
      fprintf(fp, "float %s_%u = ", pa_acc_name, k);
      rmu_emit_reduce_init(red_kind, fp);
      fputs(";\n", fp);
    }
    // Reduce loops, nested outermost (axis_0) -> innermost
    // (axis_{n-1}).  One open per reduce axis; honour OPT(UNROLL)
    // per-axis, otherwise default-unroll small K.  Mirrors tinygrad
    // lowerer.py: REDUCE.src=(body,)+tuple(ranges) opens each axis as
    // its own loop in src order.
    u32 pa_red_depth = pa_body_depth;
    for (u32 ai = 0; ai < red_n_axes; ai++) {
      if (red_kind_opts[ai] == UOP_OPT_UNROLL) {
        for (u32 d = 0; d < pa_red_depth; d++) fputs("  ", fp);
        rmu_emit_unroll_pragma(fp, red_factor_opts[ai]);
      } else if (red_kind_opts[ai] == RMU_NO_OPT) {
        u32 red_extent = uop_range_extent(red_ranges[ai]);
        if (red_extent > 0 && red_extent <= RMU_REDUCE_UNROLL_MAX) {
          for (u32 d = 0; d < pa_red_depth; d++) fputs("  ", fp);
          fprintf(fp, "#pragma unroll(%u)\n", red_extent);
        }
      }
      rmu_emit_range_open(red_ranges[ai], fp, pa_red_depth, red_kind_opts[ai]);
      pa_red_depth++;
    }
    // Shared-load hoist (path #2 of docs/tinygrad_late_passes.md).
    // Inside the K loop body, before the F MAD lines, emit ONE local
    // variable per UOP_INDEX_E whose address doesn't depend on any
    // UPCAST'd axis -- the load is identical across all F lanes, so
    // pre-loading once + reusing the local in every MAD shrinks the
    // emitted body from F*2 reads per K-iter to F+(shared count) reads.
    // The per-k uop_graph_rewrite below is identity on the hoisted
    // subtrees (the rewrite only touches UPCAST'd RANGE leaves and the
    // hash-consed rebuilder preserves Term identity on unchanged
    // subtrees), so the hoist map keyed on the ORIGINAL red_src matches
    // every per-k rewrite result.  See tinygrad
    // codegen/late/devectorizer.py:81-117 fold_expanded_index for the
    // UOp-graph-level analogue (folds adjacent INDEX nodes into one
    // wider CAT'd INDEX + per-lane GEP).
    rmu_hoist_clear();
    char hoist_name_pool[RMU_HOIST_MAX * 32] = {0};
    u32 n_hoist = rmu_hoist_shared_loads(red_src,
                                         pa_up.up_axes, pa_up.n,
                                         red_axis,
                                         hoist_name_pool,
                                         (u32)sizeof(hoist_name_pool),
                                         fp, pa_red_depth);
    (void)n_hoist;  // emitted inline; declarations precede the F MADs
    // F MAD statements, each with the UPCAST'd RANGE leaves substituted
    // by a literal CONST in the row-major-decoded position.  Innermost
    // UPCAST axis (last in pa_up.up_axes[]) varies fastest so adjacent
    // k indices share outer-axis values -- the compiler CSEs any
    // outer-axis-only address arithmetic across the rectangle.  Shared
    // loads are already pre-emitted via the hoist map above; the
    // rmu_emit_term recursion short-circuits to the local-variable
    // name whenever it encounters a hoisted Term.
    UOpGraphRewriteRule pa_rules[1] = {
      { "parallel_acc_subst", rmu_conv_m_subst_rule }
    };
    for (u32 k = 0; k < pa_total; k++) {
      u32 rem = k;
      for (i32 i = (i32)pa_up.n - 1; i >= 0; i--) {
        pa_up.up_vals[i] = rem % pa_up_exts[i];
        rem /= pa_up_exts[i];
      }
      Term rk = uop_graph_rewrite(red_src, pa_rules, 1, &pa_up);
      for (u32 d = 0; d < pa_red_depth; d++) fputs("  ", fp);
      if (red_kind == REDUCE_MAX) {
        fprintf(fp, "%s_%u = fmax(%s_%u, ", pa_acc_name, k, pa_acc_name, k);
        rmu_emit_term(rk, fp);
        fputs(");\n", fp);
      } else {
        fprintf(fp, "%s_%u = %s_%u + ", pa_acc_name, k, pa_acc_name, k);
        rmu_emit_term(rk, fp);
        fputs(";\n", fp);
      }
    }
    rmu_hoist_clear();
    // Close all reduce loops (innermost first).
    for (i32 ai = (i32)red_n_axes - 1; ai >= 0; ai--) {
      pa_red_depth--;
      for (u32 d = 0; d < pa_red_depth; d++) fputs("  ", fp);
      fputs("}\n", fp);
    }
    // float4 store probe (Lever B / NEON, C target, f32 out).  When the
    // innermost UPCAST output axis is contiguous stride-1 in the store
    // address and its extent is a multiple of 4, the lanes group into
    // contiguous 4-wide runs whose accumulators pack into one
    // `*((float4*)..)=(float4){..}` store.  This is the load-bearing change:
    // with N scalar `out[..]=_accN_k;` stores clang's SLP cost model
    // DECLINES the upstream lane chain (each _accN_k -> scalar `fmadd`);
    // once the store is a float4, clang packs the lanes into `fmla.4s`.
    // Mirrors tinygrad's `*((float4*)data0)=(float4){alu..}` final store
    // (codegen/late/devectorizer.py).  Value-exact: each lane keeps its own
    // sequential reduction order; the float4 only re-groups the final
    // per-lane accumulators (verified bit-exact vs the scalar store).
    u32 pa_vaxis = pa_up.up_axes[pa_up.n - 1];
    u32 pa_vext  = pa_up_exts[pa_up.n - 1];
    u32 pa_out_dt = uop_buffer_dtype(buf);
    int pa_vec4 = 0;
    if (RMU_TARGET == CG_TARGET_C && pa_out_dt == DT_FP32 && pa_vext % 4 == 0) {
      i64 s;
      if (rmu_axis_index_stride(addr, pa_vaxis, &s) && s == 1) pa_vec4 = 1;
    }
    if (pa_vec4) {
      // Group every 4 consecutive lanes (the fastest-varying innermost
      // UPCAST axis spans 4 contiguous positions) into one float4 store.
      for (u32 g = 0; g * 4 < pa_total; g++) {
        u32 rem0 = g * 4;
        for (i32 i = (i32)pa_up.n - 1; i >= 0; i--) {
          pa_up.up_vals[i] = rem0 % pa_up_exts[i];
          rem0 /= pa_up_exts[i];
        }
        Term ak0 = uop_graph_rewrite(addr, pa_rules, 1, &pa_up);
        for (u32 d = 0; d < pa_body_depth; d++) fputs("  ", fp);
        fprintf(fp, "*((float4*)(&%s[", rmu_buf_name(buf));
        rmu_emit_term(ak0, fp);
        fprintf(fp, "])) = (float4){%s_%u, %s_%u, %s_%u, %s_%u};\n",
                pa_acc_name, g * 4 + 0, pa_acc_name, g * 4 + 1,
                pa_acc_name, g * 4 + 2, pa_acc_name, g * 4 + 3);
      }
    } else
    // F final stores.
    for (u32 k = 0; k < pa_total; k++) {
      u32 rem = k;
      for (i32 i = (i32)pa_up.n - 1; i >= 0; i--) {
        pa_up.up_vals[i] = rem % pa_up_exts[i];
        rem /= pa_up_exts[i];
      }
      Term ak = uop_graph_rewrite(addr, pa_rules, 1, &pa_up);
      for (u32 d = 0; d < pa_body_depth; d++) fputs("  ", fp);
      fprintf(fp, "%s[", rmu_buf_name(buf));
      rmu_emit_term(ak, fp);
      fputs("] = ", fp);
      rmu_store_cast_open(buf, fp);
      fprintf(fp, "%s_%u", pa_acc_name, k);
      rmu_store_cast_close(buf, fp);
      fputs(";\n", fp);
    }
    // Close runtime output loops in reverse order.
    for (i32 i = (i32)pa_f_n - 1; i >= 0; i--) {
      if (!pa_needs_close[i]) continue;
      pa_body_depth--;
      for (u32 d = 0; d < pa_body_depth; d++) fputs("  ", fp);
      fputs("}\n", fp);
    }
    return 1;
  }

  // Legacy single-accumulator path (no UPCAST'd output axis, or gated
  // off): one `_accN`, one runtime UPCAST loop per UPCAST'd output
  // (emitted by rmu_emit_output_loops as `#pragma unroll`).
  // Emit output ranges: promoted-GLOBAL (tid decode) for plain-LOOP
  // output axes, threadbinds for LOCAL/explicit-GLOBAL, serial loops
  // otherwise.  Emits the `if (tid >= total) return;` bounds guard.
  int needs_close[RMU_MAX_RANGES] = {0};
  u32 body_depth = rmu_emit_output_loops(addr, out_ranges, out_kinds,
                                         out_factors, n_out_true, depth, fp,
                                         needs_close);
  // GROUP_REDUCE detection has to fire BEFORE we declare the scalar
  // accumulator -- otherwise the rendered output would have both decls
  // and the threadgroup-shared `_acc[L]` collides with the prior
  // `float _acc;`.  Multi-axis is supported: serial REDUCE loops wrap
  // the cooperative GROUP-axis strided walk.
  {
    int any_group = 0;
    for (u32 ai = 0; ai < red_n_axes; ai++) {
      if (red_kind_opts[ai] == UOP_OPT_GROUP_REDUCE) { any_group = 1; break; }
    }
    if (any_group) {
      return rmu_emit_group_reduce(buf, addr, red_ranges, red_kind_opts,
                                   red_factor_opts, red_n_axes, red_src,
                                   red_kind, fp, body_depth, n_out_true,
                                   needs_close);
    }
  }
  // Emit accumulator decl using the reduce_axis as the unique id.
  char acc_name[32];
  snprintf(acc_name, sizeof(acc_name), "_acc%u", red_axis);
  for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
  fprintf(fp, "float %s = ", acc_name);
  rmu_emit_reduce_init(red_kind, fp);
  fputs(";\n", fp);
  // Reduce-axis loops, nested outermost (axis_0) -> innermost
  // (axis_{n-1}).  Per-axis: honour OPT(UNROLL) if present, otherwise
  // default-unroll small extents (matmul K=25, conv kH=5, etc.).
  u32 red_depth = body_depth;
  for (u32 ai = 0; ai < red_n_axes; ai++) {
    if (red_kind_opts[ai] != RMU_NO_OPT
        && red_kind_opts[ai] == UOP_OPT_UNROLL) {
      for (u32 d = 0; d < red_depth; d++) fputs("  ", fp);
      rmu_emit_unroll_pragma(fp, red_factor_opts[ai]);
    } else if (red_kind_opts[ai] == RMU_NO_OPT
               && RMU_TARGET != CG_TARGET_C) {
      u32 red_extent = uop_range_extent(red_ranges[ai]);
      if (red_extent > 0 && red_extent <= RMU_REDUCE_UNROLL_MAX) {
        for (u32 d = 0; d < red_depth; d++) fputs("  ", fp);
        fprintf(fp, "#pragma unroll(%u)\n", red_extent);
      }
    }
    rmu_emit_range_open(red_ranges[ai], fp, red_depth, red_kind_opts[ai]);
    red_depth++;
  }
  // Inner-K decomposition: nested unrolled loops INSIDE the innermost
  // reduce loop, BEFORE the combine, so all inner-axis iterations fold
  // into the single accumulator (tinygrad expander.py:do_expand
  // semantics).
  u32 reduce_body_depth = rmu_emit_inner_reduce_decomp_loops(
      out_ranges + n_out_true, out_kinds + n_out_true,
      out_factors + n_out_true, n_inner, fp, red_depth);
  // Combine inside the (innermost) reduce loop.
  for (u32 d = 0; d < reduce_body_depth; d++) fputs("  ", fp);
  rmu_emit_reduce_combine(acc_name, red_kind, red_src, fp);
  for (u32 i = 0; i < n_inner; i++) {
    reduce_body_depth--;
    for (u32 d = 0; d < reduce_body_depth; d++) fputs("  ", fp);
    fputs("}\n", fp);
  }
  // Close reduce loops (innermost first).
  for (i32 ai = (i32)red_n_axes - 1; ai >= 0; ai--) {
    red_depth--;
    for (u32 d = 0; d < red_depth; d++) fputs("  ", fp);
    fputs("}\n", fp);
  }
  // Final store.
  for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
  fprintf(fp, "%s[", rmu_buf_name(buf));
  rmu_emit_term(addr, fp);
  fputs("] = ", fp);
  rmu_store_cast_open(buf, fp);
  fputs(acc_name, fp);
  rmu_store_cast_close(buf, fp);
  fputs(";\n", fp);
  // Close output loops.
  for (i32 i = (i32)n_out_true - 1; i >= 0; i--) {
    if (!needs_close[i]) continue;
    body_depth--;
    for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
    fputs("}\n", fp);
  }
  return 1;
}

// Emit one REDUCE accumulator at `emit_depth`:
//   float _accN = init;
//   for (reduce-axis) { <nested reduces using this axis>; _accN = combine; }
// `reduces` / `n_reduces` are the full collected-reduce table;
// `parent_idx[i]` says which reduce (by
// index, or RMU_REDUCE_NO_PARENT for a root) should emit reduces[i].  When
// `parent_idx[k]` points at THIS reduce, reduces[k] is a child whose body
// references this reduce's reduce-axis -- it is emitted recursively INSIDE
// this reduce's loop so the inner reference to the outer axis var is in
// scope.  Without that nesting an inner reduce wrapped in an elementwise op
// hoists above the outer reduce's loop and references `aN` before its
// declaration (nvrtc `identifier "aN" is undefined`).
//
// Ported from tinygrad's reduce-loop placement: tinygrad/uop/ops.py:352-370
// makes each REDUCE carry every RANGE its body transitively depends on in
// `.ranges`, and tinygrad/codegen/late/linearizer.py:56-90 schedules each
// RANGE/END so a RANGE that another RANGE depends on is opened first --
// i.e. an inner reduce-loop nests inside every reduce-loop it depends on.
#define RMU_REDUCE_NO_PARENT 0xFFFFFFFFu

// Register-blocking lane context for the generic store path: when one or
// more UPCAST'd OUTPUT axes are register-blocked, every reduce emits
// `n_lanes = prod(up_exts)` independent accumulators `_acc<axis>_<k>`
// that SHARE the reduce's loop -- N straight-line combines per K-iter, so
// clang sees N independent reduction chains it can vectorize WITHOUT
// -ffast-math (Lever B).  The store value referencing the reduce picks up
// the right lane via RMU_ACC_LANE_SUFFIX (set per lane around the body
// emit).  n_lanes == 1 (lb == NULL) reproduces the legacy scalar path
// byte-for-byte.  Mirrors tinygrad codegen/late/devectorizer.py
// reduce_to_acc + expander.py do_expand (UPCAST -> per-lane DEFINE_ACC).
typedef struct {
  u32 n;                          // count of register-blocked axes
  u32 up_axes[RMU_CONV_UPCAST_MAX];
  u32 up_exts[RMU_CONV_UPCAST_MAX];
  u32 n_lanes;                    // prod(up_exts)
} RmuLaneBlock;

// Decode linear lane k into per-axis literal values + install the
// UPCAST-axis -> CONST rewrite ctx; also set the global `_<k>` acc suffix.
static Term rmu_lane_subst(Term t, const RmuLaneBlock *lb, u32 k,
                           RmuConvMSubstCtx *cx) {
  cx->n = lb->n;
  u32 rem = k;
  for (i32 i = (i32)lb->n - 1; i >= 0; i--) {
    cx->up_axes[i] = lb->up_axes[i];
    cx->up_vals[i] = rem % lb->up_exts[i];
    rem /= lb->up_exts[i];
  }
  UOpGraphRewriteRule rules[1] = { { "lane_subst", rmu_conv_m_subst_rule } };
  return uop_graph_rewrite(t, rules, 1, cx);
}

static void rmu_emit_one_reduce_lb(Term red, u32 emit_depth,
                                const Term *reduces,
                                const u32 *parent_idx, u32 self_idx,
                                u32 n_reduces, FILE *fp, Term store_addr,
                                const RmuLaneBlock *lb) {
  u32  r_kind   = uop_reduce_kind(red);
  // Multi-axis REDUCE: one shell, one shared accumulator named after
  // axis_0; emit a nested for-loop PER reduce axis with the innermost
  // loop (last axis in the builder list) carrying the combine.
  // Mirrors tinygrad lowerer.py: REDUCE.src=(body,)+tuple(ranges)
  // expands into nested DEFINE_ACC/ASSIGN over each range
  // (uop/ops.py + codegen/late/devectorizer.py:311 reduce_to_acc).
  u32  r_n_axes = uop_reduce_n_axes(red);
  if (r_n_axes == 0) return;
  u32  r_axis = uop_reduce_axis(red, 0);     // accumulator name axis
  Term r_src  = uop_reduce_src(red);
  u32  n_lanes = (lb != NULL && lb->n_lanes > 1) ? lb->n_lanes : 1;
  // Lane-blocking only on the C target (Lever B); GPU keeps its existing
  // register-block path in rmu_emit_store_reduce.
  if (n_lanes > 1 && RMU_TARGET != CG_TARGET_C) n_lanes = 1;
  char acc_name[32];
  snprintf(acc_name, sizeof(acc_name), "_acc%u", r_axis);
  if (n_lanes > 1) {
    RmuConvMSubstCtx cx = {0};
    for (u32 k = 0; k < n_lanes; k++) {
      for (u32 d = 0; d < emit_depth; d++) fputs("  ", fp);
      fprintf(fp, "float %s_%u = ", acc_name, k);
      rmu_emit_reduce_init(r_kind, fp);
      fputs(";\n", fp);
    }
    (void)cx;
  } else {
    for (u32 d = 0; d < emit_depth; d++) fputs("  ", fp);
    fprintf(fp, "float %s = ", acc_name);
    rmu_emit_reduce_init(r_kind, fp);
    fputs(";\n", fp);
  }
  // RMU_MAX_RANGES (32), NOT MAX_DIM (8): a faithful-seed fused conv-bwd
  // reduce body is a 7-D MUL [N,Cout,oh,ow,Cin,kh,kw] whose distinct RANGE
  // leaves exceed 8 once hand_opts adds UPCAST/LOCAL split axes.  With the
  // narrow MAX_DIM cap the collector truncated, the reduce-axis lookup below
  // missed an (N,oh,ow) leaf, and the loop for that axis was skipped ->
  // accumulation over extent 20/20/32 dropped -> the CUDA ~60x-low grad
  // (379254528 vs 22559834112).  Mirrors the wide cap already used by
  // rmu_emit_store_reduce / the conv template / rmu_range_is_inner_reduce_decomp.
  Term r_ranges[RMU_MAX_RANGES];
  u32  r_kinds[RMU_MAX_RANGES]   = {0};
  u32  r_factors[RMU_MAX_RANGES] = {0};
  u32  n_r_ranges = 0;
  // Descend through nested UOP_REDUCE bodies: a reduce-axis RANGE leaf
  // we need to open a for-loop on may live inside a nested reduce body.
  // Without this descent the lookup misses, the loop is skipped, and
  // `_accN` is declared but never updated.
  rmu_collect_ranges_with_opts_through_reduce_kernel(
      r_src, r_ranges, r_kinds, r_factors, &n_r_ranges);
  // Look up every reduce-axis's RANGE term + extent.  Order axes per
  // the builder list (axis_0 outermost; tinygrad lowerer.py opens them
  // in the same order as REDUCE.src[1:]).
  Term axis_ranges [MAX_DIM] = {0};
  u32  axis_extents[MAX_DIM] = {0};
  u32  axis_ids    [MAX_DIM] = {0};
  for (u32 ai = 0; ai < r_n_axes; ai++) {
    u32 want = uop_reduce_axis(red, ai);
    axis_ids[ai] = want;
    for (u32 j = 0; j < n_r_ranges; j++) {
      u32 ax = term_val(heap_read(term_val(r_ranges[j]) + 0));
      if (ax == want) {
        axis_ranges [ai] = r_ranges[j];
        axis_extents[ai] = uop_range_extent(r_ranges[j]);
        break;
      }
    }
    // A missing RANGE leaf for any axis collapses the entire reduce
    // (no contributions on that axis).  Match the legacy single-axis
    // bail-and-return-identity behaviour.
    if (axis_ranges[ai] == 0) return;
  }
  u32  reduce_extent     = axis_extents[0];
  // Multi-axis nested for-loops: outer = axis_0, innermost = axis_{n-1}.
  // GPU-generic: small-extent reduce unroll on the outermost.
  if (RMU_TARGET != CG_TARGET_C && reduce_extent > 0
      && reduce_extent <= RMU_REDUCE_UNROLL_MAX) {
    for (u32 d = 0; d < emit_depth; d++) fputs("  ", fp);
    fprintf(fp, "#pragma unroll(%u)\n", reduce_extent);
  }
  for (u32 ai = 0; ai < r_n_axes; ai++) {
    rmu_emit_range_open(axis_ranges[ai], fp, emit_depth + ai, 0);
  }
  u32 inner_depth = emit_depth + r_n_axes;
  // Inner-reduce-decomposition: when hand_opts UNROLL/UPCAST'd this
  // reduce's K axis, the inner (fresh) half is an UNROLL/UPCAST-flagged
  // RANGE in r_ranges that the REDUCE node DID NOT add to its axis tuple
  // (uop_dag_apply_split edits the leaf, not REDUCE.axes).  It is NOT an
  // output axis (absent from store_addr) and NOT a named reduce axis.
  // It must fold INTO this accumulator -- a nested loop INSIDE the K
  // loop, before the combine -- exactly as rmu_emit_store_reduce does
  // via rmu_emit_inner_reduce_decomp_loops.  Without
  // this the generic rmu_emit_store path (which handles STORE(ADD(REDUCE,
  // bias)) matmul+bias kernels) opened it as an OUTER loop, resetting
  // _acc each iteration + storing inside -> a partial sum, wrong value.
  // See rmu_range_is_inner_reduce_decomp's banner (tinygrad do_expand).
  Term  decomp_ranges [RMU_MAX_RANGES];
  u32   decomp_factors[RMU_MAX_RANGES] = {0};
  u32   n_decomp = 0;
  for (u32 j = 0; j < n_r_ranges; j++) {
    if (term_tag(r_ranges[j]) != TAG_UOP
        || term_ext(r_ranges[j]) != UOP_RANGE) continue;
    if (r_kinds[j] != UOP_OPT_UNROLL && r_kinds[j] != UOP_OPT_UPCAST) continue;
    u32 ax = (u32)term_val(heap_read(term_val(r_ranges[j]) + 0));
    int is_named_reduce = 0;
    for (u32 ai = 0; ai < r_n_axes; ai++)
      if (axis_ids[ai] == ax) { is_named_reduce = 1; break; }
    if (is_named_reduce) continue;
    if (rmu_term_uses_axis(store_addr, ax)) continue;  // real output axis
    decomp_ranges [n_decomp]   = r_ranges[j];
    decomp_factors[n_decomp]   = r_factors[j];
    n_decomp++;
  }
  inner_depth = rmu_emit_inner_reduce_decomp_loops(
      decomp_ranges, NULL, decomp_factors, n_decomp, fp, inner_depth);
  // Emit nested reduces that depend on THIS reduce's axis var INSIDE the
  // innermost loop, before the combine that references their `_accN`.
  // Each child's own reduce-axis loop -- and any deeper nesting -- is
  // emitted by the recursive call.  reduces[] is post-order; a child
  // always precedes its parent.
  for (u32 k = 0; k < n_reduces; k++) {
    if (parent_idx[k] != self_idx) continue;
    rmu_emit_one_reduce_lb(reduces[k], inner_depth,
                        reduces, parent_idx, k, n_reduces, fp,
                        store_addr, (n_lanes > 1) ? lb : NULL);
  }
  if (n_lanes > 1) {
    // Lane-invariant hoist (Lever 2): the per-lane fan-out below re-emits
    // the whole body per lane.  Hoist every maximal subterm of r_src that
    // references no lane axis (the col2im/_pool validity-mask row-half and
    // shared loads) into ONE local before the N combines, so each lane's
    // body short-circuits to the local instead of recomputing it.  Save/
    // restore the global hoist map -- the conv parallel-acc path also uses
    // it but never overlaps this reduce's emit.  rmu_lane_subst is identity
    // on lane-invariant subtrees (hash-cons preserves Term identity), so the
    // map keyed on the ORIGINAL r_src matches every per-lane rewrite result.
    u32 saved_hoist_n = RMU_HOIST_N;
    char lane_hoist_pool[RMU_HOIST_MAX * 40] = {0};
    u32 lane_pool_used = 0;
    u32 lane_n_hoist = 0;
    // Axes in scope at inner_depth: the output-loop axes (store_addr), this
    // reduce's own axes, and the inner-reduce-decomp axes opened above.  A
    // hoisted local may reference ONLY these (not a nested reduce's K-axis).
    u32 scope_axes[RMU_MAX_RANGES];
    u32 n_scope = 0;
    {
      Term sa_ranges[RMU_MAX_RANGES];
      u32  sa_n = 0;
      rmu_collect_ranges(store_addr, sa_ranges, &sa_n);
      for (u32 i = 0; i < sa_n && n_scope < RMU_MAX_RANGES; i++) {
        if (term_tag(sa_ranges[i]) == TAG_UOP
            && term_ext(sa_ranges[i]) == UOP_RANGE)
          scope_axes[n_scope++] =
              (u32)term_val(heap_read(term_val(sa_ranges[i]) + 0));
      }
      for (u32 ai = 0; ai < r_n_axes && n_scope < RMU_MAX_RANGES; ai++)
        scope_axes[n_scope++] = axis_ids[ai];
      for (u32 i = 0; i < n_decomp && n_scope < RMU_MAX_RANGES; i++)
        if (term_tag(decomp_ranges[i]) == TAG_UOP
            && term_ext(decomp_ranges[i]) == UOP_RANGE)
          scope_axes[n_scope++] =
              (u32)term_val(heap_read(term_val(decomp_ranges[i]) + 0));
    }
    rmu_hoist_lane_invariant(r_src, lb->up_axes, lb->n, r_axis,
                             scope_axes, n_scope,
                             lane_hoist_pool, (u32)sizeof(lane_hoist_pool),
                             &lane_pool_used, &lane_n_hoist, fp, inner_depth);
    // N straight-line lane combines inside the shared K loop: each lane's
    // accumulator is independent, so clang vectorizes the N-wide update.
    RmuConvMSubstCtx cx = {0};
    for (u32 k = 0; k < n_lanes; k++) {
      Term r_src_k = rmu_lane_subst(r_src, lb, k, &cx);
      char lane_acc[40];
      snprintf(lane_acc, sizeof(lane_acc), "%s_%u", acc_name, k);
      // Nested-acc references inside r_src_k must pick up the same lane
      // suffix (the inner reduce was lane-blocked above).
      snprintf(RMU_ACC_LANE_SUFFIX, sizeof(RMU_ACC_LANE_SUFFIX), "_%u", k);
      for (u32 d = 0; d < inner_depth; d++) fputs("  ", fp);
      rmu_emit_reduce_combine(lane_acc, r_kind, r_src_k, fp);
      RMU_ACC_LANE_SUFFIX[0] = '\0';
    }
    // Drop this reduce's hoist entries so they don't leak into a sibling
    // reduce or the store fan-out (their `_sh` locals are out of scope once
    // we close the reduce loop).
    while (RMU_HOIST_N > saved_hoist_n) {
      RMU_HOIST_N--;
      RMU_HOIST_MAP[RMU_HOIST_N].key  = 0;
      RMU_HOIST_MAP[RMU_HOIST_N].name = NULL;
    }
  } else {
    for (u32 d = 0; d < inner_depth; d++) fputs("  ", fp);
    rmu_emit_reduce_combine(acc_name, r_kind, r_src, fp);
  }
  // Close inner-reduce-decomp loops (innermost first).
  for (u32 i = 0; i < n_decomp; i++) {
    inner_depth--;
    for (u32 d = 0; d < inner_depth; d++) fputs("  ", fp);
    fputs("}\n", fp);
  }
  for (u32 ai = 0; ai < r_n_axes; ai++) {
    u32 close_depth = emit_depth + r_n_axes - 1 - ai;
    for (u32 d = 0; d < close_depth; d++) fputs("  ", fp);
    fputs("}\n", fp);
  }
}

// Emit a single UOP_STORE statement, wrapping with for-loops over
// every UOP_RANGE that appears in the addr/value tree.  When a range
// was wrapped in UOP_OPT(_, UNROLL, factor), emit `#pragma unroll(N)`
// above the for-loop.  UPCAST/LOCAL/etc. handling lands in F1d+.
static void rmu_emit_store(Term store, FILE *fp, u32 depth) {
  if (term_tag(store) != TAG_UOP || term_ext(store) != UOP_STORE) return;
  // Try the REDUCE-shape specialisation first.  Falls through to the
  // generic path when value isn't a UOP_REDUCE or shape doesn't match.
  if (rmu_emit_store_reduce(store, fp, depth)) return;
  u64 loc = term_val(store);
  Term buf   = heap_read(loc + 0);
  Term addr  = heap_read(loc + 1);
  Term value = heap_read(loc + 2);

  Term ranges[RMU_MAX_RANGES];
  u32  opt_kinds[RMU_MAX_RANGES]   = {0};
  u32  opt_factors[RMU_MAX_RANGES] = {0};
  u32  n_ranges = 0;
  rmu_collect_ranges_with_opts_kernel(
      addr,  ranges, opt_kinds, opt_factors, &n_ranges);
  // Descend into UOP_REDUCE bodies on the value side so auxiliary LOOP
  // axes minted inside a reduce body (e.g. a swizzler-decomposed pool
  // window axis whose enclosing UOP_REDUCE chain was elsewhere lost)
  // still get a for-loop in the emitted kernel.  Reduce-axes that bind
  // their own loop via RMU_EMIT_ONE_REDUCE get filtered out by the
  // bound-axis tracker so they don't double-emit.
  rmu_collect_ranges_with_opts_through_reduce_kernel(
      value, ranges, opt_kinds, opt_factors, &n_ranges);

  // Collect output-axis ids: ranges in the addr expression are
  // output axes (they index the store position).  Ranges that
  // appear ONLY in the value expression are reduce or auxiliary.
  Term addr_ranges[RMU_MAX_RANGES];
  u32  addr_n = 0;
  rmu_collect_ranges(addr, addr_ranges, &addr_n);
  u32 addr_axes[RMU_MAX_RANGES];
  for (u32 i = 0; i < addr_n; i++) {
    addr_axes[i] = (term_tag(addr_ranges[i]) == TAG_UOP
                    && term_ext(addr_ranges[i]) == UOP_RANGE)
                 ? (u32)term_val(heap_read(term_val(addr_ranges[i]) + 0))
                 : 0xFFFFFFFFu;
  }

  // Inner-reduce-decomp axes: an UNROLL/UPCAST'd RANGE that does NOT
  // index the store position (absent from addr) is the inner half of a
  // hand_opts-split REDUCE (K) axis.  It must fold INTO the reduce
  // accumulator (rmu_emit_one_reduce_lb opens it inside the K loop), NOT
  // wrap the accumulator as an outer loop.  So exclude it here from both
  // the required_pos depth computation and the outer range-loop emit.
  // Mirrors rmu_range_is_inner_reduce_decomp (the rmu_emit_store_reduce
  // partition).  See its banner for the tinygrad do_expand
  // correspondence.
  int range_is_inner_decomp[RMU_MAX_RANGES] = {0};
  for (u32 i = 0; i < n_ranges; i++) {
    range_is_inner_decomp[i] =
        rmu_range_is_inner_reduce_decomp(ranges[i], opt_kinds[i], addr);
  }

  // Register-blocking (Lever B, C target only): collect UPCAST'd OUTPUT
  // axes into a lane block.  Each becomes `n_lanes = prod(exts)` straight-
  // line accumulator lanes that SHARE every reduce's loop -- so clang sees
  // N independent reduction chains it can vectorize without -ffast-math,
  // instead of one scalar `_acc` it refuses to reorder.  The lane axes are
  // NOT emitted as output loops (excluded from required_pos + the outer
  // range emit); the final store fans out into N lane stores.  Mirrors
  // rmu_emit_store_reduce's parallel-acc path / tinygrad do_expand.
  RmuLaneBlock lb = {0};
  lb.n_lanes = 1;
  int lane_is_axis[RMU_MAX_RANGES] = {0};
  if (RMU_TARGET == CG_TARGET_C) {
    u32 total = 1;
    for (u32 i = 0; i < n_ranges && lb.n < RMU_CONV_UPCAST_MAX; i++) {
      Term r = ranges[i];
      if (term_tag(r) != TAG_UOP || term_ext(r) != UOP_RANGE) continue;
      if (opt_kinds[i] != UOP_OPT_UPCAST) continue;
      if (range_is_inner_decomp[i]) continue;     // inner-K, folds in reduce
      u32 axis_id = (u32)term_val(heap_read(term_val(r) + 0));
      int is_output = 0;
      for (u32 j = 0; j < addr_n; j++)
        if (addr_axes[j] == axis_id) { is_output = 1; break; }
      if (!is_output) continue;                    // only true-output lanes
      u32 e = (u32)term_val(heap_read(term_val(r) + 2));
      if (e < 2 || e > 16) continue;
      if ((u32)(total * e) > 32) continue;         // cap lanes at 32
      lb.up_axes[lb.n] = axis_id;
      lb.up_exts[lb.n] = e;
      lb.n++;
      total *= e;
      lane_is_axis[i] = 1;
    }
    lb.n_lanes = total;
  }

  // Reduce-feeding-broadcast hoist: collect the set of output-axis ids
  // that any reduce in the store value structurally depends on.  An
  // output axis NOT in this set (but a reduce-dependent axis IS) is a
  // pure broadcast axis -- e.g. softmax's column `c`: the row max/sum
  // reduce uses only the row axis `r`, and `c` just selects an output
  // column.  Promoting `c` to a grid thread makes every one of the C
  // column threads recompute the full row reduction (the 82x-slower
  // one-thread-per-output shape).  Keeping `c` a serial loop INSIDE
  // the row thread instead lets the row reduction run ONCE per row and
  // the column loop reuse `_accN` -- O(R*C) work, not O(R*C^2).
  // Dense list of output axis_ids some reduce depends on (was a [256]
  // bool keyed by axis_id; axis_ids exceed 255 in deep graphs, so the
  // fixed array silently dropped large-id axes -> they were never seen
  // as reduce-dependent and got mis-promoted).  <= RMU_MAX_RANGES axes.
  u32 reduce_dep_axes[RMU_MAX_RANGES];
  u32 n_reduce_dep = 0;
  int any_reduce_dep = 0;
  {
    Term hoist_reduces[RMU_MAX_RANGES];
    u32  n_hoist = 0;
    rmu_collect_reduces(value, hoist_reduces, &n_hoist);
    for (u32 i = 0; i < n_hoist; i++) {
      Term r_src = heap_read(term_val(hoist_reduces[i]) + 0);
      Term r_ranges[RMU_MAX_RANGES];
      u32  r_n = 0;
      rmu_collect_ranges_through_reduce_kernel(r_src, r_ranges, &r_n);
      for (u32 j = 0; j < r_n; j++) {
        if (term_tag(r_ranges[j]) != TAG_UOP
            || term_ext(r_ranges[j]) != UOP_RANGE) continue;
        u32 ax = (u32)term_val(heap_read(term_val(r_ranges[j]) + 0));
        // Only output axes count: the reduce's own reduce-axis is not
        // a broadcast lever.  An axis is an output axis iff it indexes
        // the store position.
        for (u32 k = 0; k < addr_n; k++) {
          if (addr_axes[k] == ax) {
            int seen = 0;
            for (u32 m = 0; m < n_reduce_dep; m++)
              if (reduce_dep_axes[m] == ax) { seen = 1; break; }
            if (!seen && n_reduce_dep < RMU_MAX_RANGES)
              reduce_dep_axes[n_reduce_dep++] = ax;
            any_reduce_dep = 1;
            break;
          }
        }
      }
    }
  }

  // Default-parallelise: every output axis (axis_id in addr_axes)
  // that's still a plain KAX_LOOP with no OPT wrap becomes a GLOBAL
  // grid axis.  Without this the renderer emits a serial for-loop
  // nest -- the dispatcher launches N threads (one per output
  // element) but every thread re-runs the full nest, producing ~N x
  // over-work.  Reduce / UPCAST / UNROLL / LOCAL / GROUP axes and
  // any axis already KAX_GLOBAL are left alone (the TC matmul and
  // conv2d_flat templates run before this and bind their own
  // parallel axes).  The decode context below treats the promoted
  // ranges identically to ranges that arrived axis_type==KAX_GLOBAL.
  // This composes with OPT'd axes: a UPCAST/LOCAL split's OUTER half
  // is a plain KAX_LOOP and gets promoted; the INNER half carries the
  // OPT wrapper and emits via its OPT-specific path (`#pragma unroll`
  // for UPCAST/UNROLL, `tt` bind for LOCAL).
  // GPU-generic: default-parallelise output axes onto the grid for
  // Metal AND CUDA (both decode the axis tuple from a 1-D `tid`); the
  // C target keeps the serial for-loop nest.
  u8 promote_global[RMU_MAX_RANGES] = {0};
  if (RMU_TARGET != CG_TARGET_C) {
    for (u32 i = 0; i < n_ranges; i++) {
      Term r = ranges[i];
      if (term_tag(r) != TAG_UOP || term_ext(r) != UOP_RANGE) continue;
      if (opt_kinds[i] != RMU_NO_OPT) continue;
      u32 axis_id   = (u32)term_val(heap_read(term_val(r) + 0));
      u32 axis_type = (u32)term_val(heap_read(term_val(r) + 1));
      if (axis_type != 0 /* KAX_LOOP */) continue;
      int is_output = 0;
      for (u32 j = 0; j < addr_n; j++) if (addr_axes[j] == axis_id) { is_output = 1; break; }
      if (!is_output) continue;
      // Reduce-feeding-broadcast hoist: when the kernel has a reduce
      // that depends on a DIFFERENT output axis, a pure-broadcast
      // output axis (one no reduce depends on) stays a serial loop so
      // the reduce runs once per reduce-axis tuple, not once per
      // broadcast element.  Pure-elementwise kernels (no reduce, or
      // reduces with no output-axis dependence) keep full promotion.
      int is_reduce_dep = 0;
      for (u32 m = 0; m < n_reduce_dep; m++)
        if (reduce_dep_axes[m] == axis_id) { is_reduce_dep = 1; break; }
      if (any_reduce_dep && !is_reduce_dep) {
        continue;
      }
      promote_global[i] = 1;
    }
  }

  // Multi-GLOBAL decode context: when >=2 GLOBAL axes flow through
  // tid, each needs its own (stride, modulus) decode from the flat
  // 1-D dispatch index.  Single-GLOBAL kernels emit `uint a0 = tid;`
  // (one axis, stride 1).  If the kernel also has a LOCAL axis the
  // GLOBAL axes decode from `tg` instead -- see RmuGlobalDecode.
  RmuGlobalDecode g_decode;
  rmu_compute_global_decode_ctx(ranges, n_ranges, promote_global,
                                opt_kinds, &g_decode);

  // Collect reduces and compute per-reduce *required emission depth*.
  // A reduce must emit AFTER all output-axis loops it depends on are
  // open (so its body can reference those axes), and as EARLY as
  // possible thereafter (so it doesn't redundantly recompute per
  // inner-loop iteration).  We measure "depth" as the position+1 of
  // the deepest output range the reduce body references (in the
  // order ranges[] are emitted below).  required_pos[i] == 0 means
  // the reduce is fully hoistable (no output-axis dependence) -- the
  // legacy "hoistable softmax sum" case.  required_pos[i] == n_ranges
  // means it depends on every output axis (innermost emission).
  // Anything in between (e.g. softmax max_r depends only on the row
  // axis) emits BETWEEN output loops, avoiding redundant recompute
  // per inner-axis iteration.
  Term reduces[RMU_MAX_RANGES];
  u32  n_reduces = 0;
  rmu_collect_reduces(value, reduces, &n_reduces);
  u32 required_pos[RMU_MAX_RANGES] = {0};
  for (u32 i = 0; i < n_reduces; i++) {
    Term r_src = heap_read(term_val(reduces[i]) + 0);
    Term r_ranges[RMU_MAX_RANGES];
    u32  r_n = 0;
    // Descend through nested UOP_REDUCE bodies so axes referenced only
    // by a transitively-nested reduce still bump required_pos.  Without
    // this an outer reduce hoists above output loops whose axes its
    // inner reduces actually need, producing undeclared-axis MSL.
    // RMU_MAX_RANGES cap: fused conv-backward reduce bodies reference
    // >MAX_DIM free ranges, so the MAX_DIM-capped collector dropped late
    // unfold-window axes from the dependency set and hoisted the reduce
    // above their output loops (undeclared `aN`, the kid=64 a13 bug).
    rmu_collect_ranges_through_reduce_kernel(r_src, r_ranges, &r_n);
    u32 max_pos = 0;
    for (u32 j = 0; j < r_n; j++) {
      if (term_tag(r_ranges[j]) != TAG_UOP
          || term_ext(r_ranges[j]) != UOP_RANGE) continue;
      u32 axis = (u32)term_val(heap_read(term_val(r_ranges[j]) + 0));
      for (u32 k = 0; k < n_ranges; k++) {
        if (term_tag(ranges[k]) != TAG_UOP
            || term_ext(ranges[k]) != UOP_RANGE) continue;
        // Inner-reduce-decomp axes (fold inside the reduce) and register-
        // blocked lane axes (straight-line accumulators) don't open an
        // outer loop, so they don't bump the reduce's emission depth.
        if (range_is_inner_decomp[k]) continue;
        if (lane_is_axis[k]) continue;
        u32 oaxis = (u32)term_val(heap_read(term_val(ranges[k]) + 0));
        if (oaxis == axis && (k + 1) > max_pos) max_pos = k + 1;
      }
    }
    required_pos[i] = max_pos;
  }
  // Transitive propagation for nested reduces.  rmu_collect_ranges
  // stops at UOP_REDUCE boundaries (the body's `_accN` reference is
  // a leaf to the outer scope), so the direct max_pos above does not
  // see output axes that an INNER reduce's body uses.  But the outer
  // reduce's emission references `_acc<inner>` at its own emit depth,
  // so the inner reduce must already be declared at that point.  We
  // achieve this by pushing the outer's required_pos to at least the
  // inner's required_pos for every reduce nested inside it.
  // Reduces are in post-order (inner before outer; see
  // rmu_collect_reduces's post-order add), so a single
  // forward pass suffices: by the time we visit `i`, every reduce
  // appearing in reduces[i]'s body subtree has been fully updated.
  for (u32 i = 0; i < n_reduces; i++) {
    Term r_src_i = heap_read(term_val(reduces[i]) + 0);
    for (u32 j = 0; j < i; j++) {
      // Is reduces[j] structurally inside reduces[i]'s body?  Walk the
      // body subgraph (including descending into other UOP_REDUCE
      // bodies, since nesting can be transitive) and check for term
      // identity.  Bounded recursion -- DAG is finite + small.
      if (rmu_term_contains(r_src_i, reduces[j])
          && required_pos[j] > required_pos[i]) {
        required_pos[i] = required_pos[j];
      }
    }
  }

  // Reduce-loop nesting: a reduce whose body references an OUTER
  // reduce's reduce-axis var must emit INSIDE that outer reduce's loop
  // (not hoisted to the output-loop depth -- that leaves the outer axis
  // var undeclared, the bug-1 shape).  parent_idx[i] = the index of the
  // innermost enclosing reduce that reduces[i] depends on by axis var,
  // or RMU_REDUCE_NO_PARENT if reduces[i] is a root (no enclosing-reduce
  // axis dependence).  Root reduces are emitted by Pass-0 / the output-
  // loop interleave at their required_pos; non-root reduces are emitted
  // recursively by rmu_emit_one_reduce_lb inside their parent's loop.  This
  // is the thvm port of tinygrad's per-RANGE scheduling: a reduce-loop
  // RANGE nests inside every reduce-loop RANGE it depends on (see
  // rmu_emit_one_reduce_lb's header for the tinygrad file:line).
  u32 parent_idx[RMU_MAX_RANGES];
  for (u32 i = 0; i < n_reduces; i++) parent_idx[i] = RMU_REDUCE_NO_PARENT;
  for (u32 i = 0; i < n_reduces; i++) {
    Term r_src_i = uop_reduce_src(reduces[i]);
    // Candidate enclosing reduces: those that structurally contain
    // reduces[i] AND ANY of whose reduce-axis vars reduces[i]'s body
    // references.  Multi-axis: parent qualifies if it shares any axis
    // with the inner.
    u32 best = RMU_REDUCE_NO_PARENT;
    for (u32 j = 0; j < n_reduces; j++) {
      if (j == i) continue;
      Term r_src_j = uop_reduce_src(reduces[j]);
      if (!rmu_term_contains(r_src_j, reduces[i])) continue;
      u32 nj = uop_reduce_n_axes(reduces[j]);
      int uses_any = 0;
      for (u32 ai = 0; ai < nj; ai++) {
        if (rmu_term_uses_axis(r_src_i, uop_reduce_axis(reduces[j], ai))) {
          uses_any = 1; break;
        }
      }
      if (!uses_any) continue;
      // reduces[j] is a candidate parent.  Keep the INNERMOST: the
      // candidate whose body is itself contained in every other
      // candidate's body (the nesting along i's enclosing chain is a
      // total order, so the innermost is unambiguous).
      if (best == RMU_REDUCE_NO_PARENT) { best = j; continue; }
      Term r_src_best = uop_reduce_src(reduces[best]);
      if (rmu_term_contains(r_src_best, reduces[j])) best = j;
    }
    parent_idx[i] = best;
  }

  // Bounds guard for promoted-GLOBAL kernels: the dispatcher launches
  // ceil(total/256)*256 threads (one per output element, rounded up to
  // a threadgroup multiple), so threads with tid >= total must do
  // nothing.  With a LOCAL axis the grid is exactly `total`
  // threadgroups (one per GLOBAL tuple), so the `tg >= total` form is
  // a no-op -- but emit it for symmetry.  Emit before any reduce/loop.
  if (g_decode.n_globals > 0 && g_decode.total > 0) {
    for (u32 d = 0; d < depth; d++) fputs("  ", fp);
    fprintf(fp, "if (%s >= %lluu) return;\n",
            (g_decode.has_local || RMU_HAS_GROUP_REDUCE) ? "tg" : "tid",
            (unsigned long long)g_decode.total);
  }

  // Register-block only when at least one reduce exists (a pure
  // elementwise UPCAST is already vectorizable as a serial unroll loop).
  RmuLaneBlock *lbp = (lb.n_lanes > 1 && n_reduces > 0) ? &lb : NULL;
  if (lbp == NULL)
    for (u32 i = 0; i < n_ranges; i++) lane_is_axis[i] = 0;

  // Pass 0: emit ROOT reduces with required_pos == 0 BEFORE any output
  // loop.  These are fully hoistable -- their body uses no output axis.
  // Non-root reduces (parent_idx set: their body references an enclosing
  // reduce's axis var) are emitted recursively inside that parent's loop
  // by rmu_emit_one_reduce_lb, never here.
  for (u32 i = 0; i < n_reduces; i++) {
    if (required_pos[i] != 0) continue;
    if (parent_idx[i] != RMU_REDUCE_NO_PARENT) continue;
    rmu_emit_one_reduce_lb(reduces[i], depth,
                        reduces, parent_idx, i,
                        n_reduces, fp, addr, lbp);
  }

  // Track which output ranges opened a `{` (thread-bound axes
  // don't), so we close the right number at the end.
  u32 body_depth = depth;
  int needs_close[RMU_MAX_RANGES] = {0};
  for (u32 i = 0; i < n_ranges; i++) {
    Term r = ranges[i];
    // Inner-reduce-decomp axes are emitted INSIDE the reduce accumulator
    // (rmu_emit_one_reduce_lb), never as an outer loop -- skip here.
    if (range_is_inner_decomp[i]) continue;
    // Register-blocked lane axes are straight-line accumulator indices,
    // not loops -- the final store fans them out below.
    if (lane_is_axis[i]) continue;
    u32 axis_id   = (term_tag(r) == TAG_UOP && term_ext(r) == UOP_RANGE)
                  ? (u32)term_val(heap_read(term_val(r) + 0)) : 0xFFFFFFFFu;
    u32 axis_type = (term_tag(r) == TAG_UOP && term_ext(r) == UOP_RANGE)
                  ? (u32)term_val(heap_read(term_val(r) + 1)) : 0;
    // Promoted output axes are thread-bound (decoded from `tid`, no
    // `{` block) just like LOCAL / explicit-GLOBAL axes.
    int promoted = (rmu_gd_g_mod(&g_decode, axis_id) != 0);
    int threadbound = rmu_axis_is_threadbound(opt_kinds[i], axis_type)
                   || promoted;
    if (opt_kinds[i] != RMU_NO_OPT
        && (opt_kinds[i] == UOP_OPT_UNROLL
            || opt_kinds[i] == UOP_OPT_UPCAST)
        && !threadbound) {
      for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
      rmu_emit_unroll_pragma(fp, opt_factors[i]);
    }
    rmu_emit_range_open_ctx(r, fp, body_depth, opt_kinds[i], &g_decode);
    if (!threadbound) {
      needs_close[i] = 1;
      body_depth++;
    }
    // After opening output axis at position i, emit any ROOT reduces
    // whose required_pos == i+1.  They depend on the axes opened so far
    // (and only on those), so emitting them between i and i+1 avoids
    // redundant recompute inside deeper output loops.  Non-root reduces
    // (parent_idx set) are emitted by their parent reduce's loop, not
    // here -- their required_pos is irrelevant.
    for (u32 r_i = 0; r_i < n_reduces; r_i++) {
      if (required_pos[r_i] != i + 1) continue;
      if (parent_idx[r_i] != RMU_REDUCE_NO_PARENT) continue;
      rmu_emit_one_reduce_lb(reduces[r_i], body_depth,
                          reduces, parent_idx, r_i,
                          n_reduces, fp, addr, lbp);
    }
  }
  if (lbp != NULL) {
    // float4 store probe (Lever B / NEON, C target, f32 out).  When the
    // innermost lane axis is contiguous stride-1 in the store address and
    // its extent is a multiple of 4, the lanes group into contiguous 4-wide
    // runs whose 4 scalar epilogue values pack into one `*((float4*)..)=`
    // store.  This is the load-bearing change: with N scalar `out[..]=`
    // stores the epilogue's mask/bias LOAD defeats clang's SLP cost model
    // and the upstream lane accumulators compile to scalar `fmadd`; once the
    // store is a float4 the whole lane chain packs to `fmla.4s`.  The
    // accumulators + combines stay scalar (clang SLP repacks them) -- this
    // mirrors tinygrad's `float acc0[16]` + `*((float4*)data0)=(float4){..}`
    // (codegen/late/devectorizer.py fold_expanded_index + the float4 store).
    // Value-exact: each lane keeps its own sequential reduction order; the
    // float4 only re-groups the final per-lane epilogue results.
    u32 vlane_axis = lb.up_axes[lb.n - 1];
    u32 vlane_ext  = lb.up_exts[lb.n - 1];
    u32 out_dt = uop_buffer_dtype(buf);
    int vec4 = 0;
    if (RMU_TARGET == CG_TARGET_C && out_dt == DT_FP32 && vlane_ext % 4 == 0) {
      i64 s;
      if (rmu_axis_index_stride(addr, vlane_axis, &s) && s == 1) vec4 = 1;
    }
    RmuConvMSubstCtx cx = {0};
    if (vec4) {
      // Group every 4 consecutive lanes into a float4 store.  Lane decode
      // makes the innermost axis the fastest-varying, so lanes [4g..4g+3]
      // share all outer axes and span 4 contiguous innermost positions.
      for (u32 g = 0; g * 4 < lb.n_lanes; g++) {
        Term addr0 = rmu_lane_subst(addr, &lb, g * 4, &cx);
        for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
        fprintf(fp, "*((float4*)(&%s[", rmu_buf_name(buf));
        rmu_emit_term(addr0, fp);
        fputs("])) = (float4){", fp);
        for (u32 j = 0; j < 4; j++) {
          Term value_k = rmu_lane_subst(value, &lb, g * 4 + j, &cx);
          snprintf(RMU_ACC_LANE_SUFFIX, sizeof(RMU_ACC_LANE_SUFFIX),
                   "_%u", g * 4 + j);
          if (j > 0) fputs(", ", fp);
          rmu_emit_term(value_k, fp);
          RMU_ACC_LANE_SUFFIX[0] = '\0';
        }
        fputs("};\n", fp);
      }
    } else
    // Fan the store out into N lane stores: per-lane addr + value with the
    // UPCAST lane axes substituted to literals and the `_acc` references
    // suffixed `_<k>` so each lane reads its own accumulator(s).
    for (u32 k = 0; k < lb.n_lanes; k++) {
      Term addr_k  = rmu_lane_subst(addr,  &lb, k, &cx);
      Term value_k = rmu_lane_subst(value, &lb, k, &cx);
      snprintf(RMU_ACC_LANE_SUFFIX, sizeof(RMU_ACC_LANE_SUFFIX), "_%u", k);
      for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
      fprintf(fp, "%s[", rmu_buf_name(buf));
      rmu_emit_term(addr_k, fp);
      fputs("] = ", fp);
      rmu_store_cast_open(buf, fp);
      rmu_emit_term(value_k, fp);
      rmu_store_cast_close(buf, fp);
      fputs(";\n", fp);
      RMU_ACC_LANE_SUFFIX[0] = '\0';
    }
  } else {
    for (u32 i = 0; i < body_depth; i++) fputs("  ", fp);
    fprintf(fp, "%s[", rmu_buf_name(buf));
    rmu_emit_term(addr, fp);
    fputs("] = ", fp);
    rmu_store_cast_open(buf, fp);
    rmu_emit_term(value, fp);
    rmu_store_cast_close(buf, fp);
    fputs(";\n", fp);
  }
  // Close braces (innermost first), only for ranges that opened one.
  for (i32 i = (i32)n_ranges - 1; i >= 0; i--) {
    if (!needs_close[i]) continue;
    body_depth--;
    for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
    fputs("}\n", fp);
  }
}

// Walk an AFTER chain bottom-up, emitting each store followed by a
// barrier when the next happens-after pair crosses a scope boundary.
// In F0 we just emit the leading store; the chain semantics get fully
// wired when the renderer flips its primary path.
static void rmu_emit_after(Term after, FILE *fp, u32 depth) {
  if (term_tag(after) != TAG_UOP || term_ext(after) != UOP_AFTER) return;
  u64 loc = term_val(after);
  Term node       = heap_read(loc + 0);
  Term after_node = heap_read(loc + 1);
  // Emit the prior side-effect first, then the barrier, then `node`.
  if (term_ext(after_node) == UOP_STORE) rmu_emit_store(after_node, fp, depth);
  if (term_ext(after_node) == UOP_AFTER) rmu_emit_after(after_node, fp, depth);
  // Cross-scope check: emit barrier when storing into LOCAL and the
  // next store reads from / writes to a different scope.
  Term prev_buf = (term_ext(after_node) == UOP_STORE)
                  ? heap_read(term_val(after_node) + 0) : 0;
  Term curr_buf = (term_ext(node) == UOP_STORE)
                  ? heap_read(term_val(node) + 0) : 0;
  u32 prev_scope = uop_buffer_scope(prev_buf);
  u32 curr_scope = uop_buffer_scope(curr_buf);
  if (prev_scope == UOP_SCOPE_LOCAL || curr_scope == UOP_SCOPE_LOCAL) {
    for (u32 i = 0; i < depth; i++) fputs("  ", fp);
    fputs("threadgroup_barrier(mem_flags::mem_threadgroup);\n", fp);
  }
  if (term_ext(node) == UOP_STORE) rmu_emit_store(node, fp, depth);
  if (term_ext(node) == UOP_AFTER) rmu_emit_after(node, fp, depth);
}

// Walk the DAG rooted at `root` and collect the unique UOP_BUFFER
// terms keyed by `instance` (kernel_lift.c sets instance=0 on the
// output and instance=slot+1 on input slot `slot`).  Slot 0 of the
// returned `slot_bufs[]` array holds the output (instance=0), slot
// k>=1 holds input (k-1).  Returns the highest input slot+1 used,
// i.e. n_inputs.
//
// For lifted kernels every BUFFER carries a structural instance, so
// this walk is the source of truth for the kernel signature: shapes
// + dtypes come from the BUFFER terms themselves, not from any
// external in_bufs[] array.  Tests that build BUFFERs with
// instance==0 throughout (no slot disambiguation) cannot use this
// helper -- they go through the explicit cg_render_uop_kernel(root,
// out_buf, in_bufs[]) entry instead.
//
// Capacity matches RMU_BUF_MAX (32 slots: 1 output + up to 31 inputs,
// matching the Metal buffer-attribute cap).
#define RMU_DISCOVER_MAX RMU_BUF_MAX
static void rmu_discover_bufs_rec(Term t, Term *slot_bufs, u32 *n_inputs_out) {
  // TAG_TEN: bare tensor leaves the unified pass / kernel_lift left in
  // the DAG instead of converting to UOP_BUFFER input slots.  Treat as
  // input slots: promote to the next free slot and register so
  // rmu_buf_name finds them; otherwise the body's INDEX_E call hits
  // the `buf{term_val}` fallback and Metal compile fails on the
  // undeclared identifier.
  if (term_tag(t) == TAG_TEN) {
    for (u32 i = (*n_inputs_out) + 1; i < RMU_DISCOVER_MAX; i++) {
      if (slot_bufs[i] == t) return;
      if (slot_bufs[i] == 0) {
        slot_bufs[i] = t;
        *n_inputs_out = i;
        return;
      }
    }
    return;
  }
  if (term_tag(t) != TAG_UOP) return;
  u32 op = term_ext(t);
  u64 loc = term_val(t);
  if (op == UOP_BUFFERIZE) {
    // Residual UOP_BUFFERIZE that the unified pass / materialize.c
    // BUFFERIZE-inline rewriter declined (e.g. INDEX_E(BUFFERIZE(REDUCE...),
    // IDIV(R, C)) where R is a reduce-axis RANGE -- materialize.c bails on
    // the type=1 leaf to stay correct on pool-style gradients).
    //
    // Treat the BUFFERIZE as an OPAQUE LEAF -- DO NOT recurse into its
    // value subtree.  The body's INDEX_E(BUFFERIZE, addr) emit reads the
    // BUFFERIZE Term as a single buffer load (the producer-side
    // computation does NOT execute in this kernel).  Recursing into the
    // value subtree pulls in producer-side UOP_BUFFER / TAG_TEN leaves
    // that are NOT referenced by this kernel's body but consume slots in
    // slot_bufs[].  When the inner inst=K UOP_BUFFER lands in slot K
    // before the consumer-side inst=K UOP_BUFFER reaches the walker, the
    // consumer's inst=K UOP_BUFFER hits the collision path and gets
    // promoted to a high slot -- the renderer's signature declares
    // slot_bufs[K] as the inner Term (typically dtype=0 ->
    // unsigned char) while the body emits `inK-1` via the structural
    // path expecting the canonical inst=K UOP_BUFFER's float dtype.
    // Dispatch passes ins_v[K-1] = float buffer, the body declares
    // const unsigned char *inK-1 = ins_v[K-1], and the loop reads
    // BN-scale floats as bytes -> garbage *garbage = ~exp10 floats ->
    // 192M loss on beautiful_mnist BS=16.
    //
    // cpu_uop_walk doesn't suffer this: it indexes input slots by Term
    // identity against cached_lift.in_bufs[] (built from input_tids[],
    // not from the discover walk), and resolves the residue BUFFERIZE
    // to 0.0 via uwalk_resolve_buf's fall-through return.  Mirror that
    // semantics in the INDEX_E emit (rmu_emit_term below): an
    // unresolved buf -> rmu_buf_name_or_null returns NULL -> emit
    // 0.0f literal.  Same numerical result as the walker, no dispatch
    // contract changes.
    return;
  }
  if (op == UOP_BUFFER) {
    u32 inst = uop_buffer_inst_get(t);
    if (inst >= RMU_DISCOVER_MAX) return;
    // Already filled by this exact Term -- a reuse (same BUFFER appears
    // in multiple positions of the DAG).  Nothing to do.
    if (slot_bufs[inst] == t) return;
    if (slot_bufs[inst] == 0) {
      slot_bufs[inst] = t;
      if (inst >= 1 && inst > *n_inputs_out) *n_inputs_out = inst;
      return;
    }
    // Slot collision with a DIFFERENT Term.  Common case: bench-train
    // backward kernels carry multiple bare UOP_BUFFER nodes with inst=0
    // (the unified pass / kernel_lift fold left some external buffers
    // unstamped).  Without promotion these get silently dropped from
    // the kernel signature, and rmu_buf_name's `buf{loc}` fallback
    // fires in the body -- the resulting MSL references undeclared
    // identifiers (`buf53991`, etc.) and Metal compile fails.  Promote
    // to the next free input slot.
    for (u32 i = (*n_inputs_out) + 1; i < RMU_DISCOVER_MAX; i++) {
      if (slot_bufs[i] == t) return;       // already promoted by an
                                            // earlier visit of the same Term
      if (slot_bufs[i] == 0) {
        slot_bufs[i] = t;
        *n_inputs_out = i;
        return;
      }
    }
    return;
  }
  // Recurse over operand slots.  Mirrors rmu_collect_ranges' op
  // coverage; conservative -- walks any UOp's heap operands.  Each
  // UOp's heap layout puts operand Terms in successive slots after a
  // small header; we walk a fixed window large enough to cover the
  // widest existing UOp shape (UOP_INDEX_E + UOP_STORE = 3 operands;
  // UOP_REDUCE = 3; UOP_IWHERE = 3; UOP_OPT = 1 + 2 NUM headers).
  // BUFFER terms are leaves so they only ever appear in operand
  // slots, never in header NUM slots.
  switch (op) {
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL: case UOP_IDIV:
    case UOP_IMOD: case UOP_ILT:  case UOP_IAND: case UOP_IOR: case UOP_IXOR:
    case UOP_ISHR:
    case UOP_ADD:  case UOP_MUL:  case UOP_CMPLT: case UOP_CMPEQ:
    case UOP_INDEX_E:
      rmu_discover_bufs_rec(heap_read(loc + 0), slot_bufs, n_inputs_out);
      rmu_discover_bufs_rec(heap_read(loc + 1), slot_bufs, n_inputs_out);
      return;
    case UOP_NEG:   case UOP_RECIP: case UOP_EXP2:
    case UOP_LOG2:  case UOP_SQRT:
      rmu_discover_bufs_rec(heap_read(loc + 0), slot_bufs, n_inputs_out);
      return;
    case UOP_CAST:  case UOP_BITCAST:
      // [src, NUM(dst_dtype)]
      rmu_discover_bufs_rec(heap_read(loc + 0), slot_bufs, n_inputs_out);
      return;
    case UOP_IWHERE:
      rmu_discover_bufs_rec(heap_read(loc + 0), slot_bufs, n_inputs_out);
      rmu_discover_bufs_rec(heap_read(loc + 1), slot_bufs, n_inputs_out);
      rmu_discover_bufs_rec(heap_read(loc + 2), slot_bufs, n_inputs_out);
      return;
    case UOP_OPT:
      // [target, NUM(kind), NUM(factor)]
      rmu_discover_bufs_rec(heap_read(loc + 0), slot_bufs, n_inputs_out);
      return;
    case UOP_REDUCE:
      // [src, NUM(kind), NUM(axis)]
      rmu_discover_bufs_rec(heap_read(loc + 0), slot_bufs, n_inputs_out);
      return;
    case UOP_STORE:
      // [buf, addr, value]
      rmu_discover_bufs_rec(heap_read(loc + 0), slot_bufs, n_inputs_out);
      rmu_discover_bufs_rec(heap_read(loc + 1), slot_bufs, n_inputs_out);
      rmu_discover_bufs_rec(heap_read(loc + 2), slot_bufs, n_inputs_out);
      return;
    case UOP_AFTER:
      // [node, after_node]
      rmu_discover_bufs_rec(heap_read(loc + 0), slot_bufs, n_inputs_out);
      rmu_discover_bufs_rec(heap_read(loc + 1), slot_bufs, n_inputs_out);
      return;
    case UOP_RANGE:
    case UOP_CONST: case UOP_INVALID:
    case UOP_BUFFER:
      return;
    default:
      return;
  }
}

// Public: return the highest input slot (UOP_BUFFER instance) the
// renderer will emit a buffer parameter for, given a finalized kernel
// store_root.  This is the SAME discovery walk the metal/CPU signature
// emitters use (rmu_discover_bufs_rec), so the count returned here is
// exactly the number of `in0..in{N-1}` params the compiled function
// declares.  materialize.c calls this to reconcile ke->n_inputs (the
// dispatch-time bind count) with the rendered signature, dropping any
// trailing input slots the unified-rewrite collapsed away (e.g. two
// view-clones of one buffer fold to a single UOP_BUFFER inst, or a
// fused-away producer leaves a recorded-but-unread slot).  Mirrors
// tinygrad's bufs_from_ast (codegen/opt/postrange.py:330): the kernel's
// buffer list is derived FROM the final AST's PARAM nodes, never a
// separately-maintained list that can drift.
fn u32 cg_render_input_inst_count(Term root) {
  Term slot_bufs[RMU_DISCOVER_MAX] = {0};
  u32 n_inputs = 0;
  rmu_discover_bufs_rec(root, slot_bufs, &n_inputs);
  return n_inputs;
}

// Render a kernel rooted at `root`.  The root is typically a
// UOP_STORE (single-store kernel) or UOP_AFTER chain (multi-store
// kernel).  `kernel_name` and a list of input buffers + the output
// buffer drive the kernel signature.
//
// This is the legacy entry point retained for synthetic test
// kernels (instance==0 across all BUFFERs) and call sites that
// haven't yet migrated.  Production callers in render_metal.c +
// backend/cpu/jit.c use cg_render_uop_kernel_root() below, which
// discovers buffer slots from the DAG via UOP_BUFFER.instance and
// no longer requires the caller to pass in_bufs[].
fn void cg_render_uop_kernel(Term root, const char *kernel_name,
                             Term out_buf, Term const *in_bufs,
                             u32 n_inputs, FILE *fp) {
  if (fp == NULL) fp = stderr;
  if (kernel_name == NULL) kernel_name = "uop_kernel";
  // Populate buffer-name map: out_buf -> "out", in_bufs[i] -> "inN".
  // For lifted kernels (every BUFFER has instance != 0 except
  // out_buf) the in_bufs[] entries are ignored at lookup time --
  // rmu_buf_name decodes instance directly.  Registration here is
  // load-bearing only for the synthetic test path.
  rmu_buf_names_reset();
  rmu_buf_names_set(out_buf, "out");
  for (u32 i = 0; i < n_inputs; i++) {
    char name[16];
    snprintf(name, sizeof(name), "in%u", i);
    rmu_buf_names_set(in_bufs[i], name);
  }
  fputs("#include <metal_stdlib>\n", fp);
  fputs("using namespace metal;\n\n", fp);
  fprintf(fp, "kernel void %s(\n", kernel_name);
  // Output goes to buffer(0); each input goes to buffer(1+i).
  u32 out_dtype = rmu_slot_dtype(out_buf);
  fprintf(fp, "    device %s *out [[ buffer(0) ]]",
          rmu_msl_type_name(out_dtype));
  for (u32 i = 0; i < n_inputs; i++) {
    u32 dt = rmu_slot_dtype(in_bufs[i]);
    fprintf(fp, ",\n    device const %s *in%u [[ buffer(%u) ]]",
            rmu_msl_type_name(dt), i, i + 1);
  }
  fputs(",\n    uint tid [[ thread_position_in_grid ]],\n", fp);
  fputs("    uint tg [[ threadgroup_position_in_grid ]],\n", fp);
  fputs("    uint tt [[ thread_position_in_threadgroup ]],\n", fp);
  fputs("    uint sgi [[ simdgroup_index_in_threadgroup ]],\n", fp);
  fputs("    uint thread_index_in_simdgroup "
        "[[ thread_index_in_simdgroup ]]) {\n", fp);
  // Body.  In F0 we just dispatch on the root op and emit the
  // contained store (or AFTER chain).  Range-loop wrapping happens
  // when the root is wrapped in a RANGE chain (future work).
  if (root != 0 && term_tag(root) == TAG_UOP) {
    u32 op = term_ext(root);
    if (op == UOP_STORE)      rmu_emit_store(root, fp, 1);
    else if (op == UOP_AFTER) rmu_emit_after(root, fp, 1);
    else {
      fputs("  /* unsupported root op */\n", fp);
    }
  } else {
    fputs("  /* empty kernel */\n", fp);
  }
  fputs("}\n", fp);
}

// Structural-mode MSL renderer.  Walks `root` to discover every
// UOP_BUFFER node by `instance` (output at slot 0, input at
// slot k+1).  No out_buf/in_bufs[] parameters: the caller passes
// the post-lift root (ke->cached_lift.store_root) and the renderer
// derives the kernel signature from the DAG itself.
//
// Production callers (cg_emit_via_uop in render_metal.c) use this
// entry; it produces output bit-equal with the legacy entry point
// when invoked on the same root, since rmu_buf_name's structural
// resolution path is identical for input slots and the output is
// registered the same way.
fn void cg_render_uop_kernel_root(Term root, const char *kernel_name,
                                  FILE *fp) {
  if (fp == NULL) fp = stderr;
  if (kernel_name == NULL) kernel_name = "uop_kernel";
  // Piece #4 route gate: opt-rich kernels (those carrying KAX_UPCAST /
  // KAX_UNROLL RANGE leaves) try the new expander + devectorize +
  // linearize + render_linearized pipeline first.  If any stage bails
  // we fall through to the legacy emit below.  The route reads from a
  // scratch buffer so a bail leaves `fp` untouched.
  if (uop_has_upcast_or_unroll(root)) {
    Term r2 = root;
    r2 = uop_expand_graph(r2);
    // sym pass between expander + devectorize: re-fires constructor-
    // time identities (ADD-zero, MUL-one, CONST*CONST fold, NEG-NEG,
    // GEP-on-STACK) that the expander left behind around hash-consed
    // children.  Mirrors tinygrad/codegen/__init__.py:full_rewrite_to_sink
    // which runs `sym + pm_pre_expander + pm_group_for_reduce + expander`
    // in one combined rewrite pass.
    r2 = uop_symbolic_rewrite(r2);
    r2 = uop_devectorize_graph(r2);
    // sym pass between devectorize + load_store_fold: catches the
    // CONST(0)-acc-init + GEP(STACK,(i,)) + STACK-singleton folds the
    // devectorizer scatters as it builds per-lane STACKs.  Mirrors
    // tinygrad/codegen/__init__.py:full_rewrite_to_sink "postopt symbolic".
    r2 = uop_symbolic_rewrite(r2);
    r2 = uop_load_store_fold_graph(r2);
    // sym pass after load_store_fold: the wide-load + per-lane GEP
    // rewrite synthesises new STACK/GEP/UNROLL chains around the
    // folded loads; one more sym sweep collapses any residual
    // GEP(STACK(...),(i,)) the fold leaves behind.  Mirrors
    // tinygrad/codegen/__init__.py:full_rewrite_to_sink "post index
    // symbolic".
    r2 = uop_symbolic_rewrite(r2);
    LinKernel lk;
    if (uop_linearize(r2, &lk)) {
      char scratch[131072];
      FILE *sfp = fmemopen(scratch, sizeof(scratch) - 1, "w");
      if (sfp != NULL) {
        int ok = cg_render_linearized_metal(&lk, kernel_name, sfp);
        long sn = ftell(sfp);
        fclose(sfp);
        if (ok && sn > 0) {
          scratch[sn] = 0;
          fputs(scratch, fp);
          return;
        }
      }
    }
  }
  Term slot_bufs[RMU_DISCOVER_MAX] = {0};
  u32 n_inputs = 0;
  rmu_discover_bufs_rec(root, slot_bufs, &n_inputs);
  Term out_buf = slot_bufs[0];
  rmu_buf_names_reset();
  // Output's instance is 0; rmu_buf_name falls through to the
  // identity map for it.  Inputs are resolved structurally so we
  // don't bother registering them.
  if (out_buf != 0) rmu_buf_names_set(out_buf, "out");
  // Register names for any BUFFERs the discover promoted (their
  // .instance field is still 0 but they landed at slot[i>=1]).  Without
  // this rmu_buf_name's structural path (`in<inst-1>`) returns "in-1"
  // for them and the body falls to the `buf{loc}` fallback.
  for (u32 i = 1; i <= n_inputs; i++) {
    Term b = slot_bufs[i];
    if (b == 0) continue;
    u32 inst = uop_buffer_inst_get(b);
    if (inst == i) continue;  // structural name "in<i-1>" is already correct
    char nm[16];
    snprintf(nm, sizeof(nm), "in%u", i - 1);
    rmu_buf_names_set(b, nm);
  }
  fputs("#include <metal_stdlib>\n", fp);
  fputs("using namespace metal;\n\n", fp);
  fprintf(fp, "kernel void %s(\n", kernel_name);
  u32 out_dtype = rmu_slot_dtype(out_buf);
  fprintf(fp, "    device %s *out [[ buffer(0) ]]",
          rmu_msl_type_name(out_dtype));
  for (u32 i = 0; i < n_inputs; i++) {
    Term in_buf = slot_bufs[i + 1];
    u32 dt = rmu_slot_dtype(in_buf);
    fprintf(fp, ",\n    device const %s *in%u [[ buffer(%u) ]]",
            rmu_msl_type_name(dt), i, i + 1);
  }
  // kvar wedge: scan the DAG for variable-bound ranges and emit
  // matching `constant uint &V_<name> [[ buffer(K) ]]` args.  The
  // metal_tile_jit_encode dispatcher binds them at the same buffer
  // indices via setBytes:; both sides walk the DAG with
  // kvar_collect_from_dag so the order is stable across calls.
  // Buffer indices land directly after the input buffers
  // (1 + n_inputs ..); kernels that are also conv-shaped today
  // bind their conv cfg at 1+n_inputs in the encoder but the
  // renderer signature omits it, so this slice doesn't have to
  // interleave -- the demo path is non-conv.
  {
    u32 used_vars[KVAR_USED_CAP];
    u32 n_vars = kvar_collect_from_dag(root, used_vars, KVAR_USED_CAP);
    for (u32 i = 0; i < n_vars; i++) {
      const char *vn = kvar_name(used_vars[i]);
      if (vn == NULL) vn = "V";
      fprintf(fp, ",\n    constant uint &V_%s [[ buffer(%u) ]]",
              vn, (u32)(1 + n_inputs + i));
    }
  }
  fputs(",\n    uint tid [[ thread_position_in_grid ]],\n", fp);
  fputs("    uint tg [[ threadgroup_position_in_grid ]],\n", fp);
  fputs("    uint tt [[ thread_position_in_threadgroup ]],\n", fp);
  fputs("    uint sgi [[ simdgroup_index_in_threadgroup ]],\n", fp);
  fputs("    uint thread_index_in_simdgroup "
        "[[ thread_index_in_simdgroup ]]) {\n", fp);
  // Cooperative shared-mem reduce (KAX_GROUP_REDUCE) launches grid =
  // output product, threadgroup = group_extent; the output decode must
  // come from `tg` (one threadgroup per output tuple).  Same flag the
  // CUDA entry uses.
  RMU_HAS_GROUP_REDUCE = rmu_dag_has_group_reduce(root);
  rmu_group_local_dims(root, &RMU_GROUP_EXTENT, &RMU_GROUP_LOCAL_TOTAL);
  if (root != 0 && term_tag(root) == TAG_UOP) {
    u32 op = term_ext(root);
    if (op == UOP_STORE)      rmu_emit_store(root, fp, 1);
    else if (op == UOP_AFTER) rmu_emit_after(root, fp, 1);
    else {
      fputs("  /* unsupported root op */\n", fp);
    }
  } else {
    fputs("  /* empty kernel */\n", fp);
  }
  RMU_HAS_GROUP_REDUCE = 0;
  RMU_GROUP_EXTENT = 0; RMU_GROUP_LOCAL_TOTAL = 1;
  fputs("}\n", fp);
}

// F6: render the same UOp DAG as a C99 kernel for the CPU JIT.
// Buffer-binding convention is the CPU-JIT contract dlsym'd by
// cpu_jit_dispatch: `void k(void *out_v, const void *const *ins_v,
//                           unsigned n, const unsigned *in_numels)`.
// The body emit shares all rmu_emit_* helpers with the MSL path; the
// RMU_TARGET == CG_TARGET_C path flips axis-binding (LOCAL/GLOBAL ->
// for-loop).
// `uint` is typedef'd to `unsigned int` so the body emit's `uint aN`
// / `for (uint a; ...)` patterns compile as C99.
//
// Scope: single-store elementwise / reduce-tail kernels (matmul TC
// template stays MSL-only since C lacks simdgroup_matrix). Caller
// gates on uop_recognise_tc NOT having wrapped the root.
// Emit `unsigned V_<name> = kvar_vals[i];` for each symbolic dim the kernel
// uses (kvar_collect_from_dag order, matching cpu/jit.c's cpu_jit_kvar_vals).
// The loop-bound emit (cg_emit_range_open) references these `V_<name>` locals.
static void cg_emit_cpu_kvar_decls(Term root, FILE *fp) {
  u32 used_vars[KVAR_USED_CAP];
  u32 n_vars = kvar_collect_from_dag(root, used_vars, KVAR_USED_CAP);
  for (u32 i = 0; i < n_vars; i++) {
    const char *vn = kvar_name(used_vars[i]);
    fprintf(fp, "  unsigned V_%s = kvar_vals[%u];\n", vn ? vn : "V", i);
  }
  fputs("  (void)kvar_vals;\n", fp);
}

fn void cg_render_uop_kernel_c(Term root, const char *kernel_name,
                               Term out_buf, Term const *in_bufs,
                               u32 n_inputs, FILE *fp) {
  if (fp == NULL) fp = stderr;
  if (kernel_name == NULL) kernel_name = "uop_kernel";
  rmu_buf_names_reset();
  rmu_buf_names_set(out_buf, "out");
  for (u32 i = 0; i < n_inputs; i++) {
    char name[16];
    snprintf(name, sizeof(name), "in%u", i);
    rmu_buf_names_set(in_bufs[i], name);
  }
  fputs("#include <stdint.h>\n", fp);
  fputs("#include <math.h>\n", fp);
  fputs("#include <string.h>\n", fp);
  fputs("typedef unsigned int uint;\n", fp);
  // UOP_BITCAST renders to THVM_BITCAST(dst, expr); macro expands
  // to a memcpy-based reinterpret. Statement-expression form keeps
  // the use-site syntactically an expression (composes with the
  // surrounding emit). GCC/clang extension; both compilers we
  // target accept it.
  fputs("#define THVM_BITCAST(t, x) "
        "({ t _t; __typeof__(x) _x = (x); "
        "memcpy(&_t, &_x, sizeof(_t)); _t; })\n", fp);
  // float4 vector type for the lane-block float4 store (Lever B / NEON):
  // the renderer emits `*((float4*)(&out[base])) = (float4){l0,l1,l2,l3}`
  // for contiguous 4-wide output lane groups so clang packs the upstream
  // accumulator chain into `fmla.4s` instead of scalar `fmadd`.  GCC/clang
  // ext_vector_type extension; both CPU JIT compilers accept it.  aligned(4)
  // (element alignment, NOT 16) makes the cast an UNALIGNED vector store so
  // it is safe for any contiguous base offset, regardless of the arena
  // buffer's alignment -- arm64 NEON handles unaligned `str q` natively and
  // it still emits the packed fmla.4s (verified bit-exact + 20 fmla.4s).
  fputs("typedef float float4 __attribute__((aligned(4),"
        "ext_vector_type(4)));\n", fp);
  // CPU-JIT entry-point signature; cpu/jit.c dlsyms "k" and calls
  // it directly with caller pointers.
  fprintf(fp, "void %s(void *out_v, const void *const *ins_v,\n", kernel_name);
  fputs("              unsigned n, const unsigned *in_numels,\n", fp);
  fputs("              const unsigned *kvar_vals) {\n", fp);
  fputs("  (void)n; (void)in_numels;\n", fp);
  u32 out_dtype = uop_buffer_dtype(out_buf);
  fprintf(fp, "  %s *out = (%s *)out_v;\n",
          rmu_c_type_name(out_dtype), rmu_c_type_name(out_dtype));
  for (u32 i = 0; i < n_inputs; i++) {
    u32 dt = uop_buffer_dtype(in_bufs[i]);
    fprintf(fp, "  const %s *in%u = (const %s *)ins_v[%u];\n",
            rmu_c_type_name(dt), i, rmu_c_type_name(dt), i);
  }
  cg_emit_cpu_kvar_decls(root, fp);
  RMU_TARGET = CG_TARGET_C;
  // Late fast_idiv lowering (tinygrad get_late_rewrite_patterns,
  // codegen/__init__.py:89): rewrite every constant-divisor `x // c`,
  // `x % c` over a bounded non-negative index into the vectorizable
  // `(x*m) >> s` magic multiply-shift across the WHOLE kernel AST.  The C
  // target's scalar `/c`,`%c` serialize on clang's idiv unit and block
  // SIMD (the fused conv _pool window-decode is all such div/mod); the
  // mul-shift form vectorizes.  Applied bottom-up by uop_graph_rewrite, so
  // a numerator's own nested div/mod lower first (and stay bounded via
  // uop_int_bounds' ISHR case).  C target only: METAL has a compiler bug
  // with the magic multiply and CUDA renders via its own path -- mirrors
  // fast_idiv's device gate (decompositions.py:284).  No-op where the
  // bounds / int32-overflow guards fail, so it can never change a value.
  Term emit_root = root;
  if (root != 0) {
    UOpGraphRewriteRule fi_rules[1] = { { "fast_idiv", uop_fast_idiv_rule } };
    emit_root = uop_graph_rewrite(root, fi_rules, 1, NULL);
  }
  if (emit_root != 0 && term_tag(emit_root) == TAG_UOP) {
    u32 op = term_ext(emit_root);
    if (op == UOP_STORE)      rmu_emit_store(emit_root, fp, 1);
    else if (op == UOP_AFTER) rmu_emit_after(emit_root, fp, 1);
    else {
      fputs("  /* unsupported root op */\n", fp);
    }
  } else {
    fputs("  /* empty kernel */\n", fp);
  }
  RMU_TARGET = CG_TARGET_METAL;
  fputs("}\n", fp);
}

// Structural-mode C99 renderer.  Counterpart of
// cg_render_uop_kernel_root for the CPU JIT path.  Discovers buffer
// slots from `root` via UOP_BUFFER.instance instead of trusting an
// out_buf/in_bufs[] tuple from the caller.  cpu/jit.c uses this to
// pass ke->cached_lift.store_root directly.
fn void cg_render_uop_kernel_c_root(Term root, const char *kernel_name,
                                    FILE *fp) {
  if (fp == NULL) fp = stderr;
  if (kernel_name == NULL) kernel_name = "uop_kernel";
  // Piece #4 route gate -- see comments on cg_render_uop_kernel_root.
  if (uop_has_upcast_or_unroll(root)) {
    Term r2 = root;
    r2 = uop_expand_graph(r2);
    // sym pass between expander + devectorize: re-fires constructor-
    // time identities (ADD-zero, MUL-one, CONST*CONST fold, NEG-NEG,
    // GEP-on-STACK) that the expander left behind around hash-consed
    // children.  Mirrors tinygrad/codegen/__init__.py:full_rewrite_to_sink
    // which runs `sym + pm_pre_expander + pm_group_for_reduce + expander`
    // in one combined rewrite pass.
    r2 = uop_symbolic_rewrite(r2);
    r2 = uop_devectorize_graph(r2);
    // sym pass between devectorize + load_store_fold: catches the
    // CONST(0)-acc-init + GEP(STACK,(i,)) + STACK-singleton folds the
    // devectorizer scatters as it builds per-lane STACKs.  Mirrors
    // tinygrad/codegen/__init__.py:full_rewrite_to_sink "postopt symbolic".
    r2 = uop_symbolic_rewrite(r2);
    r2 = uop_load_store_fold_graph(r2);
    // sym pass after load_store_fold: the wide-load + per-lane GEP
    // rewrite synthesises new STACK/GEP/UNROLL chains around the
    // folded loads; one more sym sweep collapses any residual
    // GEP(STACK(...),(i,)) the fold leaves behind.  Mirrors
    // tinygrad/codegen/__init__.py:full_rewrite_to_sink "post index
    // symbolic".
    r2 = uop_symbolic_rewrite(r2);
    LinKernel lk;
    if (uop_linearize(r2, &lk)) {
      char scratch[131072];
      FILE *sfp = fmemopen(scratch, sizeof(scratch) - 1, "w");
      if (sfp != NULL) {
        int ok = cg_render_linearized_c(&lk, kernel_name, sfp);
        long sn = ftell(sfp);
        fclose(sfp);
        if (ok && sn > 0) {
          scratch[sn] = 0;
          fputs(scratch, fp);
          return;
        }
      }
    }
  }
  Term slot_bufs[RMU_DISCOVER_MAX] = {0};
  u32 n_inputs = 0;
  rmu_discover_bufs_rec(root, slot_bufs, &n_inputs);
  Term out_buf = slot_bufs[0];
  rmu_buf_names_reset();
  if (out_buf != 0) rmu_buf_names_set(out_buf, "out");
  fputs("#include <stdint.h>\n", fp);
  fputs("#include <math.h>\n", fp);
  fputs("#include <string.h>\n", fp);
  fputs("typedef unsigned int uint;\n", fp);
  fputs("#define THVM_BITCAST(t, x) "
        "({ t _t; __typeof__(x) _x = (x); "
        "memcpy(&_t, &_x, sizeof(_t)); _t; })\n", fp);
  // float4 vector type for the lane-block float4 store (Lever B / NEON):
  // the renderer emits `*((float4*)(&out[base])) = (float4){l0,l1,l2,l3}`
  // for contiguous 4-wide output lane groups so clang packs the upstream
  // accumulator chain into `fmla.4s` instead of scalar `fmadd`.  aligned(4)
  // = an UNALIGNED vector store, safe for any contiguous base offset (arm64
  // NEON `str q` handles unaligned natively); still emits packed fmla.4s.
  fputs("typedef float float4 __attribute__((aligned(4),"
        "ext_vector_type(4)));\n", fp);
  fprintf(fp, "void %s(void *out_v, const void *const *ins_v,\n", kernel_name);
  fputs("              unsigned n, const unsigned *in_numels,\n", fp);
  fputs("              const unsigned *kvar_vals) {\n", fp);
  fputs("  (void)n; (void)in_numels;\n", fp);
  u32 out_dtype = uop_buffer_dtype(out_buf);
  fprintf(fp, "  %s *out = (%s *)out_v;\n",
          rmu_c_type_name(out_dtype), rmu_c_type_name(out_dtype));
  for (u32 i = 0; i < n_inputs; i++) {
    Term in_buf = slot_bufs[i + 1];
    u32 dt = uop_buffer_dtype(in_buf);
    fprintf(fp, "  const %s *in%u = (const %s *)ins_v[%u];\n",
            rmu_c_type_name(dt), i, rmu_c_type_name(dt), i);
  }
  cg_emit_cpu_kvar_decls(root, fp);
  RMU_TARGET = CG_TARGET_C;
  // Late fast_idiv lowering (tinygrad get_late_rewrite_patterns,
  // codegen/__init__.py:89): rewrite every constant-divisor `x // c`,
  // `x % c` over a bounded non-negative index into the vectorizable
  // `(x*m) >> s` magic multiply-shift across the WHOLE kernel AST.  The C
  // target's scalar `/c`,`%c` serialize on clang's idiv unit and block
  // SIMD (the fused conv _pool window-decode is all such div/mod); the
  // mul-shift form vectorizes.  Applied bottom-up by uop_graph_rewrite, so
  // a numerator's own nested div/mod lower first (and stay bounded via
  // uop_int_bounds' ISHR case).  C target only: METAL has a compiler bug
  // with the magic multiply and CUDA renders via its own path -- mirrors
  // fast_idiv's device gate (decompositions.py:284).  No-op where the
  // bounds / int32-overflow guards fail, so it can never change a value.
  Term emit_root = root;
  if (root != 0) {
    UOpGraphRewriteRule fi_rules[1] = { { "fast_idiv", uop_fast_idiv_rule } };
    emit_root = uop_graph_rewrite(root, fi_rules, 1, NULL);
  }
  if (emit_root != 0 && term_tag(emit_root) == TAG_UOP) {
    u32 op = term_ext(emit_root);
    if (op == UOP_STORE)      rmu_emit_store(emit_root, fp, 1);
    else if (op == UOP_AFTER) rmu_emit_after(emit_root, fp, 1);
    else {
      fputs("  /* unsupported root op */\n", fp);
    }
  } else {
    fputs("  /* empty kernel */\n", fp);
  }
  RMU_TARGET = CG_TARGET_METAL;
  fputs("}\n", fp);
}

// True iff `t`'s subtree contains a UOP_RANGE whose axis_type ==
// KAX_GROUP_REDUCE -- the structural marker that hand_opts applied
// KOP_GROUP/GROUPTOP to one of the reduce axes.  Used by the CUDA
// (and Metal) entry point to set RMU_HAS_GROUP_REDUCE before the body
// emit so the GLOBAL output decode uses `tg` (one block per output
// tuple) -- matching the launch geometry cuda_dag_dispatch_shape sets
// (grid = output product, block = group_extent).  Without this the
// output axes decode from a flat `tid` across a launch sized for
// cooperative-reduce semantics, racing 16 different outputs onto the
// same shared accumulator (-> CUDA_ERROR_ILLEGAL_ADDRESS at module
// load on the resulting register-thrashing kernel).
static int rmu_dag_has_group_reduce(Term t) {
  if (term_tag(t) != TAG_UOP) return 0;
  u32 op  = term_ext(t);
  u64 loc = term_val(t);
  if (op == UOP_RANGE) {
    return (u32)term_val(heap_read(loc + 1)) == KAX_GROUP_REDUCE;
  }
  switch (op) {
    case UOP_OPT:
      return rmu_dag_has_group_reduce(heap_read(loc + 0));
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL: case UOP_IDIV:
    case UOP_IMOD: case UOP_ILT:  case UOP_IAND: case UOP_IOR: case UOP_IXOR:
    case UOP_ISHR:
    case UOP_ADD:  case UOP_MUL:  case UOP_CMPLT: case UOP_CMPEQ:
    case UOP_INDEX_E: case UOP_AFTER:
      return rmu_dag_has_group_reduce(heap_read(loc + 0))
          || rmu_dag_has_group_reduce(heap_read(loc + 1));
    case UOP_NEG:   case UOP_RECIP: case UOP_EXP2:
    case UOP_LOG2:  case UOP_SQRT:
    case UOP_CAST:  case UOP_BITCAST:
    case UOP_REDUCE:
      return rmu_dag_has_group_reduce(heap_read(loc + 0));
    case UOP_IWHERE:
    case UOP_STORE:
      return rmu_dag_has_group_reduce(heap_read(loc + 0))
          || rmu_dag_has_group_reduce(heap_read(loc + 1))
          || rmu_dag_has_group_reduce(heap_read(loc + 2));
    default:
      return 0;
  }
}

// Walk the DAG for a UOP_OPT(_, TC, _) annotation -- the tell that the
// matmul tensor-core template will fire.  The CUDA entry point uses
// this to decide whether to emit the WMMA headers (#include <mma.h> +
// <cuda_fp16.h>); a non-matmul kernel skips them.
static int rmu_dag_has_tc(Term t) {
  if (term_tag(t) != TAG_UOP) return 0;
  u32 op  = term_ext(t);
  u64 loc = term_val(t);
  switch (op) {
    case UOP_OPT:
      if ((u32)term_val(heap_read(loc + 1)) == UOP_OPT_TC) return 1;
      return rmu_dag_has_tc(heap_read(loc + 0));
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL: case UOP_IDIV:
    case UOP_IMOD: case UOP_ILT:  case UOP_IAND: case UOP_IOR: case UOP_IXOR:
    case UOP_ISHR:
    case UOP_ADD:  case UOP_MUL:  case UOP_CMPLT: case UOP_CMPEQ:
    case UOP_INDEX_E: case UOP_AFTER:
      return rmu_dag_has_tc(heap_read(loc + 0))
          || rmu_dag_has_tc(heap_read(loc + 1));
    case UOP_NEG:   case UOP_RECIP: case UOP_EXP2:
    case UOP_LOG2:  case UOP_SQRT:
    case UOP_CAST:  case UOP_BITCAST:
    case UOP_REDUCE:
      return rmu_dag_has_tc(heap_read(loc + 0));
    case UOP_IWHERE:
    case UOP_STORE:
      return rmu_dag_has_tc(heap_read(loc + 0))
          || rmu_dag_has_tc(heap_read(loc + 1))
          || rmu_dag_has_tc(heap_read(loc + 2));
    default:
      return 0;
  }
}

// Structural-mode CUDA renderer.  Counterpart of
// cg_render_uop_kernel_root for the CUDA backend.  Discovers buffer
// slots from `root` via UOP_BUFFER.instance, exactly like the MSL and
// C99 entry points; emits an `extern "C" __global__` kernel.
//
// Differences from the MSL entry (cg_render_uop_kernel_root):
//   - No `#include <metal_stdlib>` / `using namespace metal`; instead
//     the WMMA headers (gated on a TC annotation in the DAG).
//   - Signature: `extern "C" __global__ void k(T *out, const T *in0,
//     ...)` -- plain pointers, no `[[ buffer(N) ]]` attributes.
//   - Thread builtins are computed in a prologue from blockIdx /
//     blockDim / threadIdx rather than bound via `[[ ... ]]`:
//       tid = blockIdx.x*blockDim.x + threadIdx.x   (grid index)
//       tg  = blockIdx.x                            (block index)
//       tt  = threadIdx.x                           (block-local idx)
//       sgi = threadIdx.x / 32                      (warp in block)
//     thread_index_in_simdgroup has no separate `uint` here; CUDA
//     templates that need a lane index spell `threadIdx.x % 32` inline.
fn void cg_render_uop_kernel_cuda_root(Term root, const char *kernel_name,
                                       FILE *fp) {
  if (fp == NULL) fp = stderr;
  if (kernel_name == NULL) kernel_name = "uop_kernel";
  // THVM_ROUTE_TRACE=1: log which render path each kernel takes
  // (linearized-ptx / linearized-c / legacy) -- diagnostic for whether
  // the PTX path is actually exercised.
  static int route_trace = -1;
  if (route_trace < 0) {
    char const *e = getenv("THVM_ROUTE_TRACE");
    route_trace = (e != NULL && e[0] != '0') ? 1 : 0;
  }
  // Piece #4 route gate -- see comments on cg_render_uop_kernel_root.
  if (uop_has_upcast_or_unroll(root)) {
    if (route_trace) fprintf(stderr, "[route] %s: linearized-path entry\n", kernel_name);
    Term r2 = root;
    r2 = uop_expand_graph(r2);
    // sym pass between expander + devectorize: re-fires constructor-
    // time identities (ADD-zero, MUL-one, CONST*CONST fold, NEG-NEG,
    // GEP-on-STACK) that the expander left behind around hash-consed
    // children.  Mirrors tinygrad/codegen/__init__.py:full_rewrite_to_sink
    // which runs `sym + pm_pre_expander + pm_group_for_reduce + expander`
    // in one combined rewrite pass.
    r2 = uop_symbolic_rewrite(r2);
    r2 = uop_devectorize_graph(r2);
    // sym pass between devectorize + load_store_fold: catches the
    // CONST(0)-acc-init + GEP(STACK,(i,)) + STACK-singleton folds the
    // devectorizer scatters as it builds per-lane STACKs.  Mirrors
    // tinygrad/codegen/__init__.py:full_rewrite_to_sink "postopt symbolic".
    r2 = uop_symbolic_rewrite(r2);
    r2 = uop_load_store_fold_graph(r2);
    // sym pass after load_store_fold: the wide-load + per-lane GEP
    // rewrite synthesises new STACK/GEP/UNROLL chains around the
    // folded loads; one more sym sweep collapses any residual
    // GEP(STACK(...),(i,)) the fold leaves behind.  Mirrors
    // tinygrad/codegen/__init__.py:full_rewrite_to_sink "post index
    // symbolic".
    r2 = uop_symbolic_rewrite(r2);
    LinKernel lk;
    if (uop_linearize(r2, &lk)) {
      char scratch[131072];
      // THVM_CUDA_PTX=1: emit PTX assembly directly (the jit's passthrough
      // detects the `.version` prefix + skips nvrtc).  This is the
      // nvrtc-frontend bypass; falls through to the C-source emit if the
      // PTX renderer bails on a shape it doesn't cover.  sm defaults to 70
      // (V100); the driver JITs sm_70 PTX forward to any newer device.
      static int ptx_init = 0, ptx_on = 0;
      if (!ptx_init) {
        char const *e = getenv("THVM_CUDA_PTX");
        ptx_on = (e != NULL && e[0] != '0');
        ptx_init = 1;
      }
      // THVM_CUDA_PTX_MAX=N caps the number of kernels that go through
      // the PTX renderer (0 = disable, default = unlimited).  Used to
      // bisect a wrong-results regression: find the smallest N that
      // makes the workload diverge, then dump kernel N's PTX.
      static int ptx_max_init = 0;
      static int ptx_max = -1;
      static int ptx_count = 0;
      if (!ptx_max_init) {
        char const *e = getenv("THVM_CUDA_PTX_MAX");
        ptx_max = (e != NULL) ? atoi(e) : -1;
        ptx_max_init = 1;
      }
      int ptx_allowed = ptx_on && (ptx_max < 0 || ptx_count < ptx_max);
      if (ptx_allowed) {
        FILE *pfp = fmemopen(scratch, sizeof(scratch) - 1, "w");
        if (pfp != NULL) {
          int pok = cg_render_linearized_ptx(&lk, kernel_name, 0, pfp);
          long pn = ftell(pfp);
          fclose(pfp);
          if (pok && pn > 0) {
            ptx_count++;
            if (route_trace) fprintf(stderr, "[route] %s: linearized-PTX #%d\n", kernel_name, ptx_count);
            scratch[pn] = 0; fputs(scratch, fp); return;
          }
          if (route_trace) fprintf(stderr, "[route] %s: PTX bailed\n", kernel_name);
        }
      }
      FILE *sfp = fmemopen(scratch, sizeof(scratch) - 1, "w");
      if (sfp != NULL) {
        int ok = cg_render_linearized_cuda(&lk, kernel_name, sfp);
        long sn = ftell(sfp);
        fclose(sfp);
        if (ok && sn > 0) {
          if (route_trace) fprintf(stderr, "[route] %s: linearized-C\n", kernel_name);
          scratch[sn] = 0;
          fputs(scratch, fp);
          return;
        }
        if (route_trace) fprintf(stderr, "[route] %s: linearized-C bailed\n", kernel_name);
      }
    } else if (route_trace) {
      fprintf(stderr, "[route] %s: linearize failed\n", kernel_name);
    }
  }
  if (route_trace) fprintf(stderr, "[route] %s: legacy rmu_emit\n", kernel_name);
  // Matmul TC parallelisation: stamp the M and N output RANGE leaves
  // KAX_GLOBAL (when K/M/N are all multiples of 8) so rmu_emit_matmul_tc's
  // tiled / parallel branch fires -- one block per output tile, decoded
  // from tg=blockIdx.x.  cuda_tc_tile_dispatch_shape re-applies the same
  // (idempotent) transform to size the launch grid/block to match.
  // Mirrors render_metal.c's uop_recognise_tc_parallel wrap.  No-op for
  // non-matmul kernels; the batched-gemm + conv2d recognisers run too so
  // those templates fire identically to the Metal path.
  root = uop_recognise_tc_parallel(root);
  Term slot_bufs[RMU_DISCOVER_MAX] = {0};
  u32 n_inputs = 0;
  rmu_discover_bufs_rec(root, slot_bufs, &n_inputs);
  Term out_buf = slot_bufs[0];
  rmu_buf_names_reset();
  if (out_buf != 0) rmu_buf_names_set(out_buf, "out");
  RMU_TARGET = CG_TARGET_CUDA;
  // Device compute capability for the WMMA matmul template (bf16
  // fragments need sm>=80).  0 if CUDA is not ready (no GPU probe).
#ifdef THVM_HAS_CUDA
  RMU_CUDA_SM = cuda_device_sm();
#else
  RMU_CUDA_SM = 0;
#endif
  // Cooperative shared-mem reduce: the promoted output axis must decode
  // from `tg` for the GROUP_REDUCE template (block = group_extent, grid =
  // output product).
  RMU_HAS_GROUP_REDUCE = rmu_dag_has_group_reduce(root);
  rmu_group_local_dims(root, &RMU_GROUP_EXTENT, &RMU_GROUP_LOCAL_TOTAL);
  // bf16/fp16 scalar types are used by elementwise kernels (casts,
  // copies) as well as tensor-core kernels, so the headers must be
  // present unconditionally -- not only on the TC path.
  fputs("#include <cuda_fp16.h>\n", fp);
  fputs("#include <cuda_bf16.h>\n", fp);
  if (rmu_dag_has_tc(root)) {
    fputs("#include <mma.h>\n", fp);
    fputs("using namespace nvcuda;\n", fp);
  }
  fputs("typedef unsigned int uint;\n", fp);
  // nvrtc device compilation predefines no <math.h> macros; the
  // REDUCE_MAX accumulator init (-INFINITY) and any +INFINITY guard
  // need a definition.  __int_as_float of the fp32 +inf bit pattern is
  // a device-side constant -- same idiom as the fp32 bitcast else-arm.
  fputs("#ifndef INFINITY\n"
        "#define INFINITY __int_as_float(0x7f800000)\n"
        "#endif\n\n", fp);
  // __launch_bounds__(block_x) tells nvcc the maximum threads-per-block
  // so it can size the per-thread register file accordingly.  Compute
  // block_x from the same LOCAL/GROUP_REDUCE extents the dispatch
  // shape uses; mirrors cuda_dag_dispatch_shape's KAX_LOCAL +
  // KAX_GROUP_REDUCE accumulation.  Skip the annotation when block
  // can't be determined (caller falls back to the flat 256-thread
  // dispatch shape).
  {
    u32 lb_ids[MAX_AXES], lb_types[MAX_AXES], lb_exts[MAX_AXES];
    u32 lb_n = uop_dag_collect_axes(root, lb_ids, lb_types, lb_exts, MAX_AXES);
    u32 block_size = 1;
    int has_group_reduce = 0;
    for (u32 i = 0; i < lb_n; i++) {
      if (lb_exts[i] == 0) continue;
      if (lb_types[i] == KAX_LOCAL) block_size *= lb_exts[i];
      else if (lb_types[i] == KAX_GROUP_REDUCE) has_group_reduce = 1;
    }
    // Tiled WMMA matmul: M/N are GLOBAL (not LOCAL), so the loop above
    // leaves block_size=1.  The block is local_m*local_n*32 threads; size
    // __launch_bounds__ from the picked tile so nvcc allocates the per-
    // thread register file for the actual occupancy (the 8-acc-fragment
    // 128x128 kernel needs the hint to avoid spills).  Only when the tiled
    // CUDA emit will actually fire (mirrors rmu_emit_matmul_tc_tiled_cuda's
    // gate: both M/N GLOBAL, !transA, bf16/fp16, picker accepts).
    if (RMU_TARGET == CG_TARGET_CUDA && block_size == 1) {
      u32 n_glob = 0;
      for (u32 i = 0; i < lb_n; i++) if (lb_types[i] == KAX_GLOBAL) n_glob++;
      if (n_glob >= 2 && term_tag(root) == TAG_UOP
          && term_ext(root) == UOP_STORE) {
        Term v = heap_read(term_val(root) + 2);
        if (term_tag(v) == TAG_UOP && term_ext(v) == UOP_OPT
            && uop_opt_kind(v) == UOP_OPT_TC) {
          // uop_matmul_mn_axes / uop_classify_matmul read a BARE matmul
          // (they decline an already-TC value), so peel the OPT into a
          // temporary unwrapped STORE just for the shape probe.
          Term bare = uop_store(heap_read(term_val(root) + 0),
                                heap_read(term_val(root) + 1),
                                uop_opt_target(v));
          u32 mE = 0, nE = 0, kE = 0, ua = 0;
          u32 dtc = uop_buffer_dtype(heap_read(term_val(root) + 0));
          int cbf = (dtc == DT_BF16 || dtc == DT_FP16);
          if (uop_matmul_mn_axes(bare, NULL, &mE, NULL, &nE)
              && uop_classify_matmul(bare, &kE, &ua) && ua == 0
              && mE && nE && kE) {
            RmuTcTile tl;
            if (rmu_tc_pick_tile_cuda(mE, nE, kE, cbf, &tl)) {
              block_size = tl.local_m * tl.local_n * 32u;
            }
          }
        }
      }
    }
    if (block_size > 1 && block_size <= 1024 && !has_group_reduce) {
      fprintf(fp, "extern \"C\" __global__ void __launch_bounds__(%u) %s(\n",
              block_size, kernel_name);
    } else {
      fprintf(fp, "extern \"C\" __global__ void %s(\n", kernel_name);
    }
  }
  u32 out_dtype = uop_buffer_dtype(out_buf);
  fprintf(fp, "    %s *out", rmu_cuda_type_name(out_dtype));
  for (u32 i = 0; i < n_inputs; i++) {
    Term in_buf = slot_bufs[i + 1];
    u32 dt = uop_buffer_dtype(in_buf);
    fprintf(fp, ",\n    const %s *in%u", rmu_cuda_type_name(dt), i);
  }
  // kvar wedge: same DAG scan as the MSL entry, emitted as plain
  // `unsigned` value args (no `constant ... &` Metal reference form).
  {
    u32 used_vars[KVAR_USED_CAP];
    u32 n_vars = kvar_collect_from_dag(root, used_vars, KVAR_USED_CAP);
    for (u32 i = 0; i < n_vars; i++) {
      const char *vn = kvar_name(used_vars[i]);
      if (vn == NULL) vn = "V";
      fprintf(fp, ",\n    unsigned V_%s", vn);
    }
  }
  fputs(") {\n", fp);
  // Thread-builtin prologue: the body emit references tid/tg/tt/sgi
  // exactly as on Metal; here they are ordinary locals derived from
  // the CUDA launch builtins instead of `[[ ... ]]`-bound kernel args.
  fputs("  uint tid = blockIdx.x * blockDim.x + threadIdx.x;\n", fp);
  fputs("  uint tg  = blockIdx.x;\n", fp);
  fputs("  uint tt  = threadIdx.x;\n", fp);
  fputs("  uint sgi = threadIdx.x / 32u;\n", fp);
  fputs("  (void)tid; (void)tg; (void)tt; (void)sgi;\n", fp);
  if (root != 0 && term_tag(root) == TAG_UOP) {
    u32 op = term_ext(root);
    if (op == UOP_STORE)      rmu_emit_store(root, fp, 1);
    else if (op == UOP_AFTER) rmu_emit_after(root, fp, 1);
    else {
      fputs("  /* unsupported root op */\n", fp);
    }
  } else {
    fputs("  /* empty kernel */\n", fp);
  }
  RMU_TARGET = CG_TARGET_METAL;
  RMU_HAS_GROUP_REDUCE = 0;
  RMU_GROUP_EXTENT = 0; RMU_GROUP_LOCAL_TOTAL = 1;
  fputs("}\n", fp);
}
