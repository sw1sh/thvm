// codegen/render_uop.c - UOp DAG kernel renderer.
//
// Walks a lifted kernel's UOp DAG (a STORE, or an AFTER chain of stores) and
// prints C99 for the CPU JIT, Metal or CUDA.  RANGEs become loops or thread
// binds by axis type and OPT annotation; TC matmuls use simdgroup / WMMA tiles.

static void rmu_emit_term(Term t, FILE *fp);
static u32  rmu_dtype_of(Term t);

// Renderer target.  The body emit is shared: C renders LOCAL/GLOBAL axes as
// plain loops, while Metal and CUDA bind them to thread positions.
typedef enum {
  CG_TARGET_METAL = 0,
  CG_TARGET_C     = 1,
  CG_TARGET_CUDA  = 2,
} cg_target;
static cg_target RMU_TARGET = CG_TARGET_METAL;

// 1 when the kernel has a KAX_GROUP_REDUCE axis: a block's threads cooperate on
// its outputs, so the grid axes decode from the block index `tg`, not `tid`.
static int RMU_HAS_GROUP_REDUCE = 0;
// The C kernel rendering and its UPCAST lane count, the vector width tinygrad's
// heuristic picked (1: none).
static Term RMU_C_ROOT  = 0;
static u32  RMU_C_LANES = 1;

static u32 rmu_dag_upcast_lanes(Term root) {
  u32 ids[MAX_AXES], types[MAX_AXES], exts[MAX_AXES];
  u32 n = uop_dag_collect_axes(root, ids, types, exts, MAX_AXES);
  u32 lanes = 1;
  for (u32 i = 0; i < n; i++) {
    if (types[i] == KAX_UPCAST) lanes *= exts[i];
  }
  return lanes;
}

// 1 when an address steps output loop `aid` by more than the lane count, so
// clang's loop vectorizer would gather or scatter across it: with AVX-512 it
// vectorized a strided copy over its rows (array_packing 1.9 -> 8 ms), where
// the lanes alone vectorize.  An address that does not decode (a gated one)
// leaves the loop to clang.
static int rmu_c_loop_strided(u32 aid) {
  Term addrs[64];
  u32  n = uop_dag_collect_buf_addrs(RMU_C_ROOT, addrs, 64);
  for (u32 i = 0; i < n; i++) {
    UopDagAddrCoeffsView cv;
    uop_dag_decode_addr_coeffs(addrs[i], &cv);
    if (cv.ok && uop_dag_addr_coeff_lookup(&cv, aid) > RMU_C_LANES) return 1;
  }
  return 0;
}

// A GROUP_REDUCE beside LOCAL axes (tinygrad's matvec) is the innermost local
// dim: the block holds local_total * group_extent threads, and the LOCAL axes
// decode from `tt / group_extent`.
static int RMU_GROUP_EXTENT      = 0;   // group factor, 0 if no GROUP_REDUCE
static u32 RMU_GROUP_LOCAL_TOTAL = 1;   // product of LOCAL extents in the kernel

// Compute capability of the CUDA device as two digits (70 = V100, 80 = A100),
// 0 off CUDA; bf16 WMMA fragments need >= 80.
static int RMU_CUDA_SM = 0;
#ifdef THVM_HAS_CUDA
fn int cuda_device_sm(void);
#endif

// Defined in uop/dag_scan.c, which the unity build includes after this file.
static int udg_classify_matmul_store(Term store, u32 *out_k_extent,
                                     Term *out_a_idx, Term *out_b_idx,
                                     u32 *out_unit_axis);

static int rmu_dag_has_group_reduce(Term t);
// The OPT_GROUP_REDUCE factor (cooperating threads, not the axis extent), or 0.
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
static void rmu_group_local_dims(Term root, int *group_ext, u32 *local_total) {
  *group_ext = (int)rmu_group_reduce_factor(root);
  *local_total = 1;
  u32 ids[MAX_AXES], types[MAX_AXES], exts[MAX_AXES];
  u32 n = uop_dag_collect_axes(root, ids, types, exts, MAX_AXES);
  for (u32 i = 0; i < n; i++)
    if (exts[i] != 0 && types[i] == KAX_LOCAL) *local_total *= exts[i];
}

static void rmu_emit_unroll_pragma(FILE *fp, u32 factor) {
  if (RMU_TARGET == CG_TARGET_C) {
    if (factor > 0) fprintf(fp, "#pragma clang loop unroll_count(%u)\n", factor);
    else            fputs("#pragma clang loop unroll(full)\n", fp);
  } else {
    if (factor > 0) fprintf(fp, "#pragma unroll(%u)\n", factor);
    else            fputs("#pragma unroll\n", fp);
  }
}

// Reduce loops up to this extent get `#pragma unroll`; longer ones stay rolled
// to bound the kernel body and its compile time.
#define RMU_REDUCE_UNROLL_MAX 16u

// Buffer names: a lifted input has instance = slot + 1 and renders "in<slot>";
// the output and test-built buffers (instance 0) resolve through this map.
// It grows on demand, as CPU and CUDA kernels take any number of buffers.
typedef struct {
  Term term;
  char name[16];
} RmuBufName;
static RmuBufName *RMU_BUF_NAMES;
static u32         RMU_BUF_NAMES_N;
static u32         RMU_BUF_NAMES_CAP;

static void rmu_buf_names_reset(void) {
  RMU_BUF_NAMES_N = 0;
}

static void rmu_buf_names_set(Term t, const char *name) {
  if (RMU_BUF_NAMES_N == RMU_BUF_NAMES_CAP) {
    u32 cap = RMU_BUF_NAMES_CAP ? RMU_BUF_NAMES_CAP * 2 : 64;
    RmuBufName *grown = (RmuBufName *)realloc(RMU_BUF_NAMES,
                                              (size_t)cap * sizeof(RmuBufName));
    if (grown == NULL) {
      thvm_fatal("rmu_buf_names_set: out of memory");
    }
    RMU_BUF_NAMES     = grown;
    RMU_BUF_NAMES_CAP = cap;
  }
  RMU_BUF_NAMES[RMU_BUF_NAMES_N].term = t;
  snprintf(RMU_BUF_NAMES[RMU_BUF_NAMES_N].name,
           sizeof(RMU_BUF_NAMES[0].name), "%s", name);
  RMU_BUF_NAMES_N++;
}

// The buffer's name (possibly a static buffer the next call reuses), or NULL
// when unresolved: an INDEX_E load of it renders 0.
static const char *rmu_buf_name_or_null(Term t) {
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

// rmu_buf_name_or_null with a `buf<loc>` fallback.  The loc makes structurally
// equal kernels render different source and miss the compile cache; the usual
// cause is a BUFFERIZE leaf that rangeify did not promote to a slot.
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
    // Metal 3.1 `bfloat`, as tinygrad MetalRenderer.type_map
    // (tinygrad/renderer/cstyle.py:366); the JIT compiles with -std=metal3.1.
    case DT_BF16:  return "bfloat";
    case DT_INT32: return "int";
    case DT_INT64: return "long";
    case DT_INT8:  return "char";   // MSL char is signed 8-bit
    case DT_UINT8: return "uchar";
    // fp8 has no GPU scalar: the pointer is the raw byte and the load site
    // decodes it (rmu_emit_fp8_helpers).
    case DT_FP8E4M3:
    case DT_FP8E5M2: return "uchar";
    default:       return "float";
  }
}

static const char *rmu_cuda_type_name(u32 dtype) {
  switch (dtype) {
    case DT_FP32:  return "float";
    case DT_FP16:  return "__half";
    case DT_BF16:  return "__nv_bfloat16";
    case DT_INT32: return "int";
    case DT_INT64: return "long long";
    case DT_INT8:  return "signed char";
    case DT_UINT8: return "unsigned char";
    // fp8: the raw byte, decoded at the load site as on Metal.
    case DT_FP8E4M3:
    case DT_FP8E5M2: return "unsigned char";
    default:       return "float";
  }
}

static const char *rmu_gpu_type_name(u32 dtype) {
  return (RMU_TARGET == CG_TARGET_CUDA) ? rmu_cuda_type_name(dtype)
                                        : rmu_msl_type_name(dtype);
}

// Type of a body value.  On CUDA a half/bf16 value computes in float: their
// operators make `float + __nv_bfloat16` ambiguous.  The store converts back.
static const char *rmu_gpu_alu_type_name(u32 dtype) {
  if (RMU_TARGET == CG_TARGET_CUDA && (dtype == DT_BF16 || dtype == DT_FP16))
    return "float";
  return rmu_gpu_type_name(dtype);
}

// GPU fp8 -> float decoders, ports of fp8e4m3_to_f32 / fp8e5m2_to_f32
// (src/dtype/fp8.c): widen the byte to half bits, then convert natively.
static void rmu_emit_fp8_helpers(FILE *fp) {
  if (RMU_TARGET == CG_TARGET_CUDA) {
    fputs(
      "__device__ __forceinline__ float thvm_fp8e5m2_decode(unsigned char x){\n"
      "  unsigned short ur=(unsigned short)x<<8;\n"
      "  if((ur&0x7FFFu)>0x7C00u) ur=0x7FFFu;\n"
      "  return __half2float(__ushort_as_half(ur));\n"
      "}\n"
      "__device__ __forceinline__ float thvm_fp8e4m3_decode(unsigned char x){\n"
      "  unsigned short ur=(unsigned short)x<<8;\n"
      "  unsigned short sign=(unsigned short)(ur&0x8000u);\n"
      "  unsigned short exponent=(unsigned short)((((unsigned int)ur&0x7800u)>>1)+0x2000u);\n"
      "  unsigned short mantissa=(unsigned short)((ur&0x0700u)>>1);\n"
      "  unsigned int absx=(unsigned int)x&0x7Fu;\n"
      "  if(absx==0x7Fu){ ur=0x7FFFu; }\n"
      "  else if(exponent==0x2000u){\n"
      "    if(mantissa!=0u){\n"
      "      mantissa<<=1;\n"
      "      while((mantissa&0x0400u)==0u){ mantissa<<=1; exponent-=0x0400u; }\n"
      "      mantissa&=0x03FFu;\n"
      "    } else { exponent=0; }\n"
      "    ur=(unsigned short)(sign|exponent|mantissa);\n"
      "  } else { ur=(unsigned short)(sign|exponent|mantissa); }\n"
      "  return __half2float(__ushort_as_half(ur));\n"
      "}\n", fp);
  } else {
    fputs(
      "static inline float thvm_fp8e5m2_decode(uchar x){\n"
      "  ushort ur=(ushort)x<<8;\n"
      "  if((ur&0x7FFFu)>0x7C00u) ur=0x7FFFu;\n"
      "  return float(as_type<half>(ur));\n"
      "}\n"
      "static inline float thvm_fp8e4m3_decode(uchar x){\n"
      "  ushort ur=(ushort)x<<8;\n"
      "  ushort sign=(ushort)(ur&0x8000u);\n"
      "  ushort exponent=(ushort)((((uint)ur&0x7800u)>>1)+0x2000u);\n"
      "  ushort mantissa=(ushort)((ur&0x0700u)>>1);\n"
      "  uint absx=(uint)x&0x7Fu;\n"
      "  if(absx==0x7Fu){ ur=0x7FFFu; }\n"
      "  else if(exponent==0x2000u){\n"
      "    if(mantissa!=0u){\n"
      "      mantissa<<=1;\n"
      "      while((mantissa&0x0400u)==0u){ mantissa<<=1; exponent-=0x0400u; }\n"
      "      mantissa&=0x03FFu;\n"
      "    } else { exponent=0; }\n"
      "    ur=(ushort)(sign|exponent|mantissa);\n"
      "  } else { ur=(ushort)(sign|exponent|mantissa); }\n"
      "  return float(as_type<half>(ur));\n"
      "}\n", fp);
  }
}

static u32 rmu_slot_dtype(Term t);
// 1 when a slot (0 = output, 1..n_inputs = inputs) is fp8.
static int rmu_kernel_has_fp8_input(Term const *slot_bufs, u32 n_inputs) {
  for (u32 i = 0; i <= n_inputs; i++) {
    if (slot_bufs[i] == 0) continue;
    u32 dt = rmu_slot_dtype(slot_bufs[i]);
    if (dt == DT_FP8E4M3 || dt == DT_FP8E5M2) return 1;
  }
  return 0;
}

// Dtype of a slot: a UOP_BUFFER, or a bare TAG_TEN leaf whose dtype sits in
// term_ext (uop_buffer_dtype returns 0 for it).
static u32 rmu_slot_dtype(Term t) {
  if (term_tag(t) == TAG_TEN) return term_ext(t);
  return uop_buffer_dtype(t);
}

// An MSL bfloat lvalue rejects implicit float narrowing, so a bf16/fp16 Metal
// store wraps its rhs.  C types those buffers float; CUDA converts implicitly.
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

// C has no half/bfloat/fp8 scalar; they type as float, and cpu_jit_eligible
// declines kernels with narrow-float buffers.
static const char *rmu_c_type_name(u32 dtype) {
  switch (dtype) {
    case DT_BOOL:  return "unsigned char";
    case DT_FP32:  return "float";
    case DT_FP64:  return "double";
    case DT_FP16:  return "float";
    case DT_BF16:  return "float";
    case DT_FP8E4M3:
    case DT_FP8E5M2: return "float";
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
    case UOP_ISHL: return "<<";
    default:       return "?";
  }
}

// Hoisted subterms (path #2 of docs/tinygrad_late_passes.md): rmu_emit_term
// prints the name in place of the key.  Keys are original reduce-source Terms,
// which the per-lane rewrite rebuilds unchanged off the UPCAST'd subtrees.
#define RMU_HOIST_MAX 64
typedef struct {
  Term       key;
  char const *name;
} RmuHoistSlot;
static RmuHoistSlot RMU_HOIST_MAP[RMU_HOIST_MAX];
static u32 RMU_HOIST_N = 0;

// Lane suffix ("_<k>") that rmu_emit_term appends to a REDUCE leaf's
// `_acc<axis>` when the generic store register-blocks an UPCAST'd output axis.
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

// An int binary already renders as `int`, so the operand cast below skips it:
// only uint leaves need it, and nested casts bloat deep index expressions.
static int rmu_is_int_binary(Term t) {
  if (term_tag(t) != TAG_UOP) return 0;
  switch (term_ext(t)) {
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL:
    case UOP_IDIV: case UOP_IMOD: case UOP_ILT:
    case UOP_IAND: case UOP_IOR:  case UOP_IXOR: case UOP_ISHR: case UOP_ISHL:
      return 1;
    default: return 0;
  }
}

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
        // Bit-exact, free of decimal formatting drift.
        if (RMU_TARGET == CG_TARGET_C) {
          fprintf(fp, "THVM_BITCAST(float, 0x%08xu)", bits);
        } else if (RMU_TARGET == CG_TARGET_CUDA) {
          fprintf(fp, "__uint_as_float(0x%08xu)", bits);
        } else {
          fprintf(fp, "as_type<float>(0x%08xu)", bits);
        }
      } else if (dtype_is_float(dtype)) {
        // `bits` is an f32 pattern for every float dtype, as const_to_tendesc
        // reads it, and %.17g prints it exactly.  inf / nan print INFINITY /
        // NAN as in tinygrad (renderer/cstyle.py:25-26); nvrtc lacks NAN.
        f32 fv; memcpy(&fv, &bits, sizeof(fv));
        if (isnan(fv)) {
          fputs(RMU_TARGET == CG_TARGET_CUDA ? "__int_as_float(0x7fffffff)" : "NAN", fp);
        } else if (isinf(fv)) {
          fputs(fv < 0.0f ? "(-INFINITY)" : "INFINITY", fp);
        } else {
          char lit[32];
          snprintf(lit, sizeof lit, "%.17g", (double)fv);
          // C reads `-0` as an int, so its literals stay floating (tinygrad
          // renderer/cstyle.py:37); a negative is parenthesized against `--`.
          char const *frac = (RMU_TARGET == CG_TARGET_C && strpbrk(lit, ".e") == NULL)
                           ? ".0" : "";
          fprintf(fp, signbit(fv) ? "(%s%s)" : "%s%s", lit, frac);
        }
      } else if (dtype_kind(dtype) == DK_UINT) {
        // An unsigned payload wraps to the dtype width (as const_to_tendesc
        // fills it) and prints unsigned (tinygrad renderer/cstyle.py:29-30).
        u32 width = dtype_info(dtype)->bits;
        fprintf(fp, "%uu", (width < 32) ? (bits & ((1u << width) - 1u)) : bits);
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
    case UOP_DEFINE_VAR: {
      // The kernel declares V_<name> for every kvar kvar_collect_from_dag
      // finds, this leaf included.
      const char *vn = kvar_name(kvar_extent_var_id(uop_define_var_extent(t)));
      fprintf(fp, "((int)V_%s)", vn != NULL ? vn : "V");
      return;
    }
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL:
    case UOP_IDIV: case UOP_IMOD: case UOP_ILT:
    case UOP_IAND: case UOP_IOR:  case UOP_IXOR: case UOP_ISHR: case UOP_ISHL: {
      // RANGE vars are `uint`, but ISUB is signed and may go negative (`r - k`,
      // `-1 < x`), so operands cast to `int`; tinygrad's RANGE vars are signed
      // too (renderer/cstyle.py:18-19).  A nested int binary is already int.
      Term a = heap_read(loc + 0);
      Term b = heap_read(loc + 1);
      u32  dt = rmu_dtype_of(t);
      if ((op == UOP_IAND || op == UOP_IOR || op == UOP_IXOR || op == UOP_ISHR
           || op == UOP_ISHL)
          && dtype_is_int(dt) && dt != DT_INT32 && !dtype_is_bool(dt)) {
        // A value bit op of another int dtype computes in its own type: a
        // uint32 shift right is logical, an int64 one keeps its high bits.
        char const *ty = (RMU_TARGET == CG_TARGET_C) ? rmu_c_type_name(dt)
                                                     : rmu_gpu_alu_type_name(dt);
        fprintf(fp, "((%s)(", ty);
        rmu_emit_term(a, fp);
        fprintf(fp, ") %s (%s)(", rmu_int_op_name(op), ty);
        rmu_emit_term(b, fp);
        fputs("))", fp);
        return;
      }
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
    case UOP_EXP2: case UOP_LOG2: case UOP_SQRT: case UOP_SIN: {
      int xexp2f = op == UOP_EXP2 && RMU_TARGET == CG_TARGET_C && rmu_dtype_of(t) == DT_FP32;
      const char *fn_name = xexp2f ? "thvm_xexp2f"
                          : (op == UOP_EXP2) ? "exp2"
                          : (op == UOP_LOG2) ? "log2"
                          : (op == UOP_SIN)  ? "sin"
                          :                    "sqrt";
      fprintf(fp, "%s(", fn_name);
      rmu_emit_term(heap_read(loc + 0), fp);
      fputs(")", fp);
      return;
    }
    case UOP_CAST: case UOP_BITCAST: {
      Term src      = heap_read(loc + 0);
      u32  dst_dt   = term_val(heap_read(loc + 1));
      const char *type_name = (RMU_TARGET == CG_TARGET_C)
                                 ? rmu_c_type_name(dst_dt)
                                 : rmu_gpu_alu_type_name(dst_dt);
      if (op == UOP_BITCAST) {
        if (RMU_TARGET == CG_TARGET_C) {
          // THVM_BITCAST comes from the C prologue.
          fprintf(fp, "THVM_BITCAST(%s, ", type_name);
          rmu_emit_term(src, fp);
          fputs(")", fp);
        } else if (RMU_TARGET == CG_TARGET_CUDA) {
          // nvrtc has no as_type: 32-bit reinterprets use the __*_as_*
          // intrinsics, and other dtypes fall back to a value cast.
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
        // A bool cast is truthiness (x != 0), not truncation.
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
      // An unresolved buffer (a BUFFERIZE leaf the discover walk keeps opaque)
      // loads 0.0, as cpu_uop_walk does when uwalk_resolve_buf fails.
      const char *bn = rmu_buf_name_or_null(buf);
      if (bn == NULL) {
        fputs("0.0f", fp);
        return;
      }
      u32 ldt = rmu_slot_dtype(buf);
      // A GPU fp8 load decodes the raw byte (rmu_emit_fp8_helpers).
      int fp8_load = (RMU_TARGET != CG_TARGET_C
                      && (ldt == DT_FP8E4M3 || ldt == DT_FP8E5M2));
      if (fp8_load) {
        fprintf(fp, "thvm_fp8e%s_decode(%s[",
                (ldt == DT_FP8E4M3) ? "4m3" : "5m2", bn);
        rmu_emit_term(addr, fp);
        fputs("])", fp);
        return;
      }
      // CUDA computes bf16/fp16 loads in float (rmu_gpu_alu_type_name).
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
      // Loop and template emission read the directive; the value is the target.
      Term inner = heap_read(loc + 0);
      rmu_emit_term(inner, fp);
      return;
    }
    case UOP_REDUCE: {
      // A REDUCE inside an expression names the accumulator its caller hoisted,
      // `_acc<first axis>` plus the lane suffix.
      Term tred = term_new(0, TAG_UOP, op, loc);
      u32 axis0 = uop_reduce_axis(tred, 0);
      // The accumulator is float; a bf16/fp16 REDUCE rounds to its dtype here,
      // as a stored buffer would, so a fused epilogue matches unfused kernels.
      u32 red_dt = DT_FP32;
      char const *cast_open = "", *cast_close = "";
      if (term_dtype_in(tred, 0, &red_dt)
          && (red_dt == DT_BF16 || red_dt == DT_FP16)) {
        switch (RMU_TARGET) {
          case CG_TARGET_METAL:
            // bfloat/half round on conversion and promote back in arithmetic.
            cast_open  = (red_dt == DT_BF16) ? "((bfloat)(" : "((half)(";
            cast_close = "))";
            break;
          case CG_TARGET_CUDA:
            // CUDA computes half/bf16 in float (rmu_gpu_alu_type_name): round
            // through the 2-byte type and back.
            cast_open  = (red_dt == DT_BF16)
                       ? "__bfloat162float(__float2bfloat16("
                       : "__half2float(__float2half(";
            cast_close = "))";
            break;
          default:
            // C has no narrow scalar, and the CPU JIT declines narrow floats.
            break;
        }
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

// Recursion budget of the rmu_term_* predicates.  Past it they answer NO,
// which is wrong rather than safe, so it must exceed any real DAG depth.
#define RMU_MAX_WALK_DEPTH 8192u

// No OPT wrap; distinct from UOP_OPT_UNROLL, which is 0.
#define RMU_NO_OPT 0xFFu

// Per-kernel cap on collected ranges, above MAX_DIM: a kernel spans its output
// axes, the LOOP axes inside reduce bodies and the reduce axes.
#define RMU_MAX_RANGES 32

// Leading heap slots of `op` that value walks descend: the value and index
// ops.  Every other op (RANGE, BUFFER, CONST, REDUCE, STORE, ...) is a leaf.
static u32 rmu_walk_arity(u32 op) {
  switch (op) {
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL: case UOP_IDIV:
    case UOP_IMOD: case UOP_ILT:  case UOP_IAND: case UOP_IOR: case UOP_IXOR:
    case UOP_ISHR: case UOP_ISHL:
    case UOP_ADD:  case UOP_MUL:  case UOP_CMPLT: case UOP_CMPEQ:
    case UOP_INDEX_E:
      return 2;
    case UOP_NEG:  case UOP_RECIP: case UOP_EXP2: case UOP_LOG2: case UOP_SQRT: case UOP_SIN:
    case UOP_CAST: case UOP_BITCAST: case UOP_OPT:
      return 1;
    case UOP_IWHERE:
      return 3;
    default:
      return 0;
  }
}

// rmu_walk_arity for a whole-kernel walk, which also descends a REDUCE's
// body, both AFTER operands and a STORE's buffer, address and value.
static u32 rmu_walk_arity_kernel(u32 op) {
  switch (op) {
    case UOP_REDUCE: return 1;
    case UOP_AFTER:  return 2;
    case UOP_STORE:  return 3;
    default:         return rmu_walk_arity(op);
  }
}

// Collect the distinct RANGEs of `t` in encounter order, each with the kind
// and factor of the OPT that directly wraps it (RMU_NO_OPT if none).
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
  // An OPT tags the range it directly wraps (innermost of a stack wins).
  u32 kind   = RMU_NO_OPT;
  u32 factor = 0;
  if (op == UOP_OPT) {
    kind   = term_val(heap_read(loc + 1));
    factor = term_val(heap_read(loc + 2));
  }
  for (u32 i = 0, n = rmu_walk_arity(op); i < n; i++) {
    rmu_collect_ranges_rec_cap(heap_read(loc + i), ranges, opt_kinds,
                               opt_factors, n_out, cap, kind, factor);
  }
}

// MAX_DIM-cap collection, for scans bounded by tensor rank.
static void rmu_collect_ranges_rec(Term t, Term *ranges,
                                   u32 *opt_kinds, u32 *opt_factors,
                                   u32 *n_out,
                                   u32 inherit_kind, u32 inherit_factor) {
  rmu_collect_ranges_rec_cap(t, ranges, opt_kinds, opt_factors, n_out,
                             MAX_DIM, inherit_kind, inherit_factor);
}

static int rmu_term_contains_rec(Term t, Term needle, u32 depth) {
  if (depth > RMU_MAX_WALK_DEPTH) return 0;
  if (t == needle) return 1;
  if (term_tag(t) != TAG_UOP) return 0;
  u32 op = term_ext(t);
  u64 loc = term_val(t);
  // A REDUCE's body counts too: nesting can be transitive.
  u32 n = op == UOP_REDUCE ? 1 : rmu_walk_arity(op);
  for (u32 i = 0; i < n; i++) {
    if (rmu_term_contains_rec(heap_read(loc + i), needle, depth + 1)) return 1;
  }
  return 0;
}
static int rmu_term_contains(Term t, Term needle) {
  return rmu_term_contains_rec(t, needle, 0);
}

// 1 if `t` uses axis `axis_id` free, not bound by a nested REDUCE: an inner
// reduce that uses an outer reduce's axis renders inside that loop.  tinygrad's
// UOp.ranges ends a REDUCE's ranges the same way (uop/ops.py:470-499).
static int rmu_term_uses_axis_rec(Term t, u32 axis_id, u32 depth) {
  if (depth > RMU_MAX_WALK_DEPTH) return 0;
  if (term_tag(t) != TAG_UOP) return 0;
  u32 op = term_ext(t);
  u64 loc = term_val(t);
  if (op == UOP_RANGE) {
    return (u32)term_val(heap_read(loc + 0)) == axis_id;
  }
  if (op == UOP_REDUCE) {
    // A nested REDUCE binds only its own axes, so the walk descends into it:
    // REDUCE_b(body) still uses an outer axis that `body` reads.
    Term tred = term_new(0, TAG_UOP, op, loc);
    u32 n_axes = uop_reduce_n_axes(tred);
    for (u32 i = 0; i < n_axes; i++) {
      if (uop_reduce_axis(tred, i) == axis_id) return 0;
    }
    return rmu_term_uses_axis_rec(uop_reduce_src(tred), axis_id, depth + 1);
  }
  for (u32 i = 0, n = rmu_walk_arity(op); i < n; i++) {
    if (rmu_term_uses_axis_rec(heap_read(loc + i), axis_id, depth + 1)) return 1;
  }
  return 0;
}
static int rmu_term_uses_axis(Term t, u32 axis_id) {
  return rmu_term_uses_axis_rec(t, axis_id, 0);
}

// rmu_emit_store_reduce declines a reduce whose body holds another: only
// rmu_emit_store declares every nested accumulator.
static int rmu_term_has_reduce(Term t, u32 depth) {
  if (depth > RMU_MAX_WALK_DEPTH) return 0;
  if (term_tag(t) != TAG_UOP) return 0;
  u32 op = term_ext(t);
  u64 loc = term_val(t);
  if (op == UOP_REDUCE) return 1;
  for (u32 i = 0, n = rmu_walk_arity(op); i < n; i++) {
    if (rmu_term_has_reduce(heap_read(loc + i), depth + 1)) return 1;
  }
  return 0;
}

// Collect the distinct REDUCEs of `t` (up to MAX_DIM), inner before outer.
static void rmu_collect_reduces(Term t, Term *reduces, u32 *n_out) {
  if (term_tag(t) != TAG_UOP) return;
  if (*n_out >= MAX_DIM) return;
  u32 op = term_ext(t);
  u64 loc = term_val(t);
  if (op == UOP_REDUCE) {
    for (u32 i = 0; i < *n_out; i++) {
      if (reduces[i] == t) return;
    }
    // Body first: a nested REDUCE's `_acc` is declared before its reader's.
    rmu_collect_reduces(heap_read(loc + 0), reduces, n_out);
    if (*n_out >= MAX_DIM) return;
    for (u32 i = 0; i < *n_out; i++) {
      if (reduces[i] == t) return;
    }
    reduces[*n_out] = t;
    (*n_out)++;
    return;
  }
  for (u32 i = 0, n = rmu_walk_arity(op); i < n; i++) {
    rmu_collect_reduces(heap_read(loc + i), reduces, n_out);
  }
}

static void rmu_collect_ranges(Term t, Term *ranges, u32 *n_out) {
  u32 dummy_kinds[MAX_DIM]   = {0};
  u32 dummy_factors[MAX_DIM] = {0};
  rmu_collect_ranges_rec(t, ranges, dummy_kinds, dummy_factors, n_out,
                         0, 0);
}

// A walk's bound axes, those of its enclosing REDUCEs, are not free ranges.
#define RMU_BOUND_AXIS_CAP (2 * MAX_DIM)
static int rmu_axis_is_bound(u32 const *bound, u32 n_bound, u32 aid) {
  for (u32 i = 0; i < n_bound; i++) if (bound[i] == aid) return 1;
  return 0;
}

// rmu_collect_ranges_rec_cap that also descends REDUCE bodies, minus the axes
// each REDUCE binds: a reduce depends on every free axis of the reduces it
// nests, which the required_pos scan and the reduce-axis lookup must see.
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
    // Every axis of the REDUCE ends at it (tinygrad uop/ops.py:470-499).
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
  u32 kind   = RMU_NO_OPT;
  u32 factor = 0;
  if (op == UOP_OPT) {
    kind   = term_val(heap_read(loc + 1));
    factor = term_val(heap_read(loc + 2));
  }
  for (u32 i = 0, n = rmu_walk_arity(op); i < n; i++) {
    rmu_collect_ranges_rec_through_reduce_cap(
        heap_read(loc + i), ranges, opt_kinds, opt_factors, n_out, cap,
        kind, factor, bound_axes, n_bound);
  }
}

// RMU_MAX_RANGES-cap scan for rmu_emit_store's reduce placement: a fused
// conv-backward reduce body can reference more than MAX_DIM free ranges.
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

// RMU_MAX_RANGES-cap collections for rmu_emit_store, whose scope can exceed
// MAX_DIM axes.
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

// 1 if the axis binds a thread position instead of opening a loop.
static int rmu_axis_is_threadbound(u32 opt_kind, u32 axis_type) {
  if (RMU_TARGET == CG_TARGET_C) return 0;  // C99 has no thread positions
  return (opt_kind == UOP_OPT_LOCAL)
      || axis_type == 4  /* KAX_LOCAL */
      || axis_type == 5  /* KAX_GLOBAL */;
}

// Thread decode: promoted LOOP output axes and KAX_GLOBAL axes take
// `(idx / stride) % extent` of `tid`, or of the block index `tg` when LOCAL
// axes decode from `tt`.  Axis ids are DAG-global, hence dense lists.
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

// Lookups by axis id; 0 means not a GLOBAL / LOCAL axis.
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

// Open a loop, or bind a thread position, for RANGE `r`; `gctx` (may be NULL)
// holds the thread decode.  A kvar extent (KVAR_FLAG) bounds the loop by the
// kernel argument `V_<name>`; only loop bounds take a kvar extent.
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
  if (opt_kind == UOP_OPT_LOCAL || axis_type == 4) {
    // `aN = tt` only for a lone LOCAL axis at stride 1; a coexisting
    // GROUP_REDUCE shifts it up by the group extent.
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
  // A GLOBAL axis decodes from `tg` when the block has LOCAL or GROUP threads.
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
  if (axis_type == 5 /* KAX_GLOBAL without a decode context */) {
    fprintf(fp, "uint a%u = tg; /* global ext=%u */\n", axis_id, extent);
    return;
  }
  if (RMU_C_LANES > 1 && axis_type == KAX_LOOP && opt_kind != UOP_OPT_UPCAST
      && opt_kind != UOP_OPT_UNROLL && rmu_c_loop_strided(axis_id)) {
    fputs("#pragma clang loop vectorize(disable)\n", fp);
    for (u32 i = 0; i < depth; i++) fputs("  ", fp);
  }
  if (axis_type == 1 /*REDUCE*/) {
    fprintf(fp, "for (uint a%u = 0; a%u < %s; a%u++) /*reduce*/ {\n",
            axis_id, axis_id, bound, axis_id);
  } else {
    fprintf(fp, "for (uint a%u = 0; a%u < %s; a%u++) {\n",
            axis_id, axis_id, bound, axis_id);
  }
}

static void rmu_emit_range_open(Term r, FILE *fp, u32 depth,
                                u32 opt_kind) {
  rmu_emit_range_open_ctx(r, fp, depth, opt_kind, NULL);
}

// 1 if ranges[i] is LOCAL, by OPT or axis type; `opt_kinds` may be NULL.
static int rmu_range_is_local(Term const *ranges, u32 const *opt_kinds, u32 i) {
  Term r = ranges[i];
  if (term_tag(r) != TAG_UOP || term_ext(r) != UOP_RANGE) return 0;
  if (opt_kinds != NULL && opt_kinds[i] == UOP_OPT_LOCAL) return 1;
  u32 axis_type = (u32)term_val(heap_read(term_val(r) + 1));
  return axis_type == 4 /* KAX_LOCAL */;
}

// Build the thread decode: `promote[i]` marks a plain LOOP output axis that
// becomes a grid axis.  Strides grow right to left, so the last axis of each
// kind has stride 1; any LOCAL axis sets has_local.
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
    // KAX_GLOBAL axes (the matmul templates) join the decode, so several of
    // them split the flat grid instead of each reading `tg`.
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
  if (RMU_TARGET != CG_TARGET_C) {
    // LOCAL axes decode from `tt`, innermost at stride 1; a coexisting
    // GROUP_REDUCE owns the innermost group_extent threads.
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

// Open a reduce store's output axes: plain LOOP axes of `addr` become grid axes
// behind a `tid >= total` guard; axes outside `addr` stay serial, or threads
// would race on one output.  Fills needs_close[]; returns the body depth.
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
  // GPU only.  An UPCAST/LOCAL split leaves its outer half a plain LOOP, which
  // is promoted here, and its inner half OPT-wrapped for the per-axis emit.
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

// Peel a CAST/BITCAST off a MUL operand to reach its INDEX_E: a bf16 matmul
// widens each operand to f32, but the simdgroup template loads bf16 directly.
static Term rmu_peel_cast(Term t) {
  if (term_tag(t) != TAG_UOP) return t;
  u32 op = term_ext(t);
  if (op == UOP_CAST || op == UOP_BITCAST) {
    Term src = heap_read(term_val(t) + 0);
    if (term_tag(src) == TAG_UOP && term_ext(src) == UOP_INDEX_E) return src;
  }
  return t;
}

// THVM_FUSE_MATMUL_INPUT: a single-consumer elementwise producer of matmul A
// (rb_matmul_input_fusable) computes inline at the TC A-staging load.  tinygrad
// schedule/indexing.py:242-244: an op with one consumer inherits its ranges.

// The first INDEX_E leaf of an elementwise tree, or 0.
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

// 1 if an address uses IDIV/IMOD, which the m/k staging decode cannot follow.
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

// 1 if `t` can compute inline at the A-staging load: an elementwise/CAST/CONST
// tree over INDEX_E leaves whose addresses use no N axis and no IDIV/IMOD.
static int rmu_fused_producer_ok(Term t, u32 n_axis, u32 depth) {
  if (depth > 40) return 0;
  if (term_tag(t) == TAG_NUM) return 1;
  if (term_tag(t) != TAG_UOP) return 0;
  u32 op = term_ext(t);
  if (op == UOP_CONST) return 1;
  if (op == UOP_INDEX_E) {
    Term addr = heap_read(term_val(t) + 1);
    Term rngs[MAX_DIM]; u32 nr = 0;
    rmu_collect_ranges(addr, rngs, &nr);
    for (u32 i = 0; i < nr; i++) {
      if (term_val(heap_read(term_val(rngs[i]) + 0)) == n_axis) return 0;
    }
    if (rmu_addr_has_divmod(addr, 0)) return 0;
    return 1;
  }
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

// THVM_FUSE_MATMUL_EPILOGUE (default off): an elementwise chain over one TC
// matmul output fuses into the matmul's store, applied per element at the bf16
// staging copy (tinygrad schedule/indexing.py:242-244).
static int rmu_fuse_epilogue_on(void) {
  static int known = 0, on = 0;
  if (!known) {
    char const *e = getenv("THVM_FUSE_MATMUL_EPILOGUE");
    on = (e != NULL && e[0] == '1');
    known = 1;
  }
  return on;
}

// THVM_TC_BATCHED (default off, as a full FLUX generation decodes wrong with
// it): batched matmuls (attention's Q@K^T, @V) take the tiled emitter, offset
// by each operand's real batch stride (rmu_range_coeff).
static int rmu_tc_batched_on(void) {
  static int known = 0, on = 0;
  if (!known) {
    char const *e = getenv("THVM_TC_BATCHED");
    if (e != NULL && e[0] == '1') on = 1;
    known = 1;
  }
  return on;
}

// Index arithmetic over RANGE leaves (addresses, valid masks): the epilogue
// renders it per output element with _m/_n substituted, so it is epilogue-safe.
static int rmu_epilogue_is_index_int_op(u32 op) {
  return op == UOP_RANGE || op == UOP_IADD || op == UOP_ISUB || op == UOP_IMUL
      || op == UOP_IDIV  || op == UOP_IMOD || op == UOP_ILT  || op == UOP_IAND
      || op == UOP_IOR   || op == UOP_IXOR || op == UOP_ISHR || op == UOP_ISHL || op == UOP_IWHERE;
}

// 1 if `t` is an elementwise/CAST/index tree over INDEX_E/CONST leaves and
// OPT(TC) leaves, counted in *n_tc; an OPT(TC) is not descended.
static int rmu_epilogue_walk(Term t, Term *out_tc, int *n_tc, u32 depth) {
  if (depth > 48) return 0;
  if (term_tag(t) == TAG_NUM) return 1;
  if (term_tag(t) != TAG_UOP) return 1;
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

// Match STORE(C, addr, OPT(REDUCE(MUL(INDEX_E(A), INDEX_E(B)), SUM), TC)), the
// value optionally an epilogue over it; sets `*out_red_value` to the REDUCE.
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
  Term lhs = rmu_peel_cast(heap_read(term_val(mul) + 0));
  Term rhs = rmu_peel_cast(heap_read(term_val(mul) + 1));
  // THVM_FUSE_MATMUL_INPUT: A may be a fused elementwise producer, which
  // rmu_emit_matmul_tc vets (rmu_fused_producer_ok); B stays a bare INDEX_E.
  if (!(term_tag(lhs) == TAG_UOP && term_ext(lhs) == UOP_INDEX_E)) {
    if (!ru_fuse_matmul_input_on()) return 0;
    if (rmu_fused_first_index(lhs, 0) == 0) return 0;  // needs at least one leaf
  }
  if (term_tag(rhs) != TAG_UOP || term_ext(rhs) != UOP_INDEX_E) return 0;
  if (out_red_value != NULL) *out_red_value = inner;
  return 1;
}

// 1 if an accumulator or hoisted local of dtype `dt` is held in its own int or
// bool type: on C, as tinygrad allocates the accumulator like the reduce
// (reduce_ranges_to_acc, codegen/__init__.py:197-204).
static int rmu_holds_int(u32 dt) {
  return RMU_TARGET == CG_TARGET_C
      && (dtype_is_bool(dt) || (dtype_is_int(dt) && !dtype_is_packed(dt)));
}

// 1 iff it is held in a double: the C target computes an f64 value as a
// double (rmu_c_type_name), and a float accumulator keeps f32 precision.
static int rmu_holds_double(u32 dt) {
  return RMU_TARGET == CG_TARGET_C && dt == DT_FP64;
}

// Every other float, and every value on the GPU targets (which type an
// f64 buffer as float), is held in a float.
static const char *rmu_hold_type(u32 dt) {
  if (rmu_holds_int(dt)) {
    return rmu_c_type_name(dt);
  }
  return rmu_holds_double(dt) ? "double" : "float";
}

static u32 rmu_dtype_of(Term t) {
  u32 dt = DT_FP32;
  (void)term_dtype_in(t, 0, &dt);
  return dt;
}

// `<acc> = combine(acc, src);` for the REDUCE kind.  A MAX in C compares, as
// tinygrad lowers MAX without a native op (codegen/decomp/op.py:84); a bool
// SUM or MAX is an OR (uop/symbolic.py:163-166).
static void rmu_emit_reduce_combine(const char *acc_name, u32 kind, u32 dt,
                                    Term src, FILE *fp) {
  if (rmu_holds_int(dt) && dtype_is_bool(dt)) {
    fprintf(fp, "%s = %s | ((", acc_name, acc_name);
    rmu_emit_term(src, fp);
    fputs(") != 0);\n", fp);
  } else if (kind == REDUCE_MAX && RMU_TARGET == CG_TARGET_C) {
    // A float too: one maxss, where fmaxf's NaN and signed-zero rules take a
    // call without -ffinite-math-only, and the walker's compare besides.
    fprintf(fp, "{ %s _v = ", rmu_hold_type(dt));
    rmu_emit_term(src, fp);
    fprintf(fp, "; %s = (%s < _v) ? _v : %s; }\n", acc_name, acc_name, acc_name);
  } else if (kind == REDUCE_MAX) {
    fprintf(fp, "%s = fmax(%s, ", acc_name, acc_name);
    rmu_emit_term(src, fp);
    fputs(");\n", fp);
  } else {
    fprintf(fp, "%s = %s + ", acc_name, acc_name);
    rmu_emit_term(src, fp);
    fputs(";\n", fp);
  }
}

// Init of a REDUCE accumulator, tinygrad's identity_element (uop/ops.py:61):
// 0 for SUM; for MAX, dt.min (dtype.py:70-72): -2^(bits-1) for a signed int,
// 0 for unsigned and bool, -INFINITY for a float.
static void rmu_emit_reduce_init(u32 kind, u32 dt, FILE *fp) {
  if (!rmu_holds_int(dt)) {
    if (kind == REDUCE_MAX) {
      fputs("-INFINITY", fp);
    } else {
      fputs(rmu_holds_double(dt) ? "0.0" : "0.0f", fp);
    }
    return;
  }
  if (kind != REDUCE_MAX || !dtype_is_signed(dt)) {
    fputs("0", fp);
    return;
  }
  switch (dtype_info(dt)->bits) {
    case 8:  fputs("INT8_MIN", fp);  break;
    case 16: fputs("INT16_MIN", fp); break;
    case 32: fputs("INT32_MIN", fp); break;
    default: fputs("INT64_MIN", fp); break;
  }
}

static int rmu_axis_affine_coeff(Term t, u32 axis_id, i64 *coeff);
// Affine stride of `axis_id` in an address (0: absent or not affine).
static i64 rmu_range_coeff(Term t, u32 axis_id) {
  i64 c = 0;
  (void) rmu_axis_affine_coeff(t, axis_id, &c);
  return c;
}

// Tile factors for an (M,N,K) matmul, or 0 for the 8x8 path.  The occupancy
// floor counts batch_extent * per-batch tiles, so a batched matmul can take a
// larger tile.  RmuTcTile lives in render_metal.c, which sizes the dispatch.
static int rmu_tc_pick_tile(u32 m_extent, u32 n_extent, u32 k_extent,
                            u32 batch_extent, RmuTcTile *out) {
  if ((m_extent % 8) != 0 || (n_extent % 8) != 0 || (k_extent % 8) != 0) {
    return 0;
  }
  // Small M or N is latency-bound: staging and barriers lose to the 8x8 path.
  if (m_extent < 64 || n_extent < 64) return 0;
  u32 m8 = m_extent / 8, n8 = n_extent / 8;
  // {local_m, local_n, rm, rn}, best first: 8 simdgroups of 32 threads,
  // M-major, a 4x4 register tile.  KB is one 8-deep K-subtile; larger
  // K-blocks lose to threadgroup-memory pressure.
  struct { u32 lm, ln, rm, rn; } cands[] = {
    {4, 2, 4, 4},   // 128x64, 8 sg
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
  // Take the largest tile with at least min_tg threadgroups, else the dividing
  // tile with the most: too few tiles leave the last wave mostly idle.
  u32 min_tg = 160;
  u32 bmul = (batch_extent > 1u) ? batch_extent : 1u;
  u32 best_lm = 0, best_ln = 0, best_rm = 0, best_rn = 0;
  u32 fb_lm = 0, fb_ln = 0, fb_rm = 0, fb_rn = 0, fb_tg = 0;
  for (u32 i = 0; i < sizeof(cands)/sizeof(cands[0]); i++) {
    u32 tm8 = cands[i].lm * cands[i].rm;
    u32 tn8 = cands[i].ln * cands[i].rn;
    if (m8 % tm8 != 0 || n8 % tn8 != 0) continue;
    u32 tg = (m8 / tm8) * (n8 / tn8) * bmul;
    if (best_lm == 0 && tg >= min_tg) {
      best_lm = cands[i].lm; best_ln = cands[i].ln;
      best_rm = cands[i].rm; best_rn = cands[i].rn;
    }
    if (tg > fb_tg) {
      fb_tg = tg; fb_lm = cands[i].lm; fb_ln = cands[i].ln;
      fb_rm = cands[i].rm; fb_rn = cands[i].rn;
    }
  }
  if (best_lm == 0) {
    best_lm = fb_lm; best_ln = fb_ln; best_rm = fb_rm; best_rn = fb_rn;
  }
  if (best_lm == 0) return 0;
  // Without reuse (one 8x8 fragment per tile) tiling does not pay.
  if (best_lm * best_rm * best_ln * best_rn <= 1) return 0;
  u32 kb = 8;
  out->local_m = best_lm; out->local_n = best_ln;
  out->rm = best_rm;      out->rn = best_rn;
  out->kb = kb;
  return 1;
}

// CUDA WMMA tile picker over 16x16x16 fragments: TILE_M = local_m*rm*16,
// TILE_N = local_n*rn*16, local_m*local_n warps per block, KB staged per
// block.  Returns 0 when no tile divides the shape (the naive WMMA path).
static int rmu_tc_pick_tile_cuda(u32 m_extent, u32 n_extent, u32 k_extent,
                                 int c_is_bf, RmuTcTile *out) {
  if ((m_extent % 16) != 0 || (n_extent % 16) != 0 || (k_extent % 16) != 0) {
    return 0;
  }
  u32 m16 = m_extent / 16, n16 = n_extent / 16;
  // {local_m, local_n, rm, rn}.  f32 output stores fragments directly, so a big
  // 32-warp tile wins; bf16/fp16 output needs a per-warp convert scratch, which
  // favors few warps with a fat 4x4 register block.
  static const struct rmu_tc_cand { u32 lm, ln, rm, rn; }
  cands_f32[] = {
    {4, 8, 2, 2},   // 128x256, 32 warps
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
    {2, 2, 4, 4},   // 128x128, 4 warps, 16 acc/warp
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
  // Block-count floor: f32's big tile wants ~2 blocks per A100 SM, while the
  // bf16 tile stays efficient at very low block counts.
  u32 min_tg = c_is_bf ? 48u : 216u;
  // Shared budget: A {tile_m x KB} + B {KB x tile_n}, padded by 8, in 2-byte
  // elements, plus nwarps * 256 floats of convert scratch for a 16-bit C,
  // under the 48 KiB static __shared__ cap of sm_70..sm_86.
  u32 cap = 48u * 1024u;
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
  return 1;
}

// CUDA port of rmu_emit_matmul_tc_tiled: warps stage each K-block of A and B
// in __shared__ (a transposed B n-major, loaded col_major); a 16-bit C goes
// through a per-warp float scratch, as store_matrix_sync cannot narrow.
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
  // Pad the staged leading dim by 8 elements: a fragment load reads 16 rows of
  // one column, and an unpadded row stride maps them all to the same banks.
  u32 PAD = 8u;
  u32 a_ld = t.kb + PAD;                   // _Asm row stride (M-major, ld_k)
  // A transposed B (W{N,K} contiguous) stages n-major, a contiguous copy of W's
  // K-runs, and loads col_major; a row-major B stages k-major, loads row_major.
  u32 b_ld   = b_trans ? (t.kb + PAD) : (tile_n + PAD);
  u32 b_rows = b_trans ? tile_n : t.kb;    // n-major: tile_n rows; else KB
  #define IND(D) for (u32 _i = 0; _i < (D); _i++) fputs("  ", fp)
  IND(depth); fprintf(fp,
    "/* TC WMMA tiled matmul: tile %ux%u, %ux%u warps, %ux%u frag/warp, KB=%u%s */\n",
    tile_m, tile_n, t.local_m, t.local_n, t.rm, t.rn, t.kb,
    b_trans ? ", B col-major staged" : "");
  // Single-buffered: the kernel is not bound by global-load latency, and double
  // buffering would double the shared footprint.
  IND(depth); fprintf(fp, "__shared__ %s _Asm[%u];\n", a_elem, tile_m * a_ld);
  IND(depth); fprintf(fp, "__shared__ %s _Bsm[%u];\n", b_elem, b_rows * b_ld);
  // This block's output-tile origin.
  IND(depth); fprintf(fp, "uint _tm = (tg / %uu) * %uu;\n", n_tiles_n, tile_m);
  IND(depth); fprintf(fp, "uint _tn = (tg %% %uu) * %uu;\n", n_tiles_n, tile_n);
  // This warp's sub-tile origin within the block tile.
  IND(depth); fprintf(fp, "uint _sm = (sgi / %uu) * %uu;\n",
                      t.local_n, t.rm * WM);
  IND(depth); fprintf(fp, "uint _sn = (sgi %% %uu) * %uu;\n",
                      t.local_n, t.rn * WN);
  // Flat thread index in the block, for cooperative staging.
  IND(depth); fputs("uint _lid = threadIdx.x;\n", fp);
  // Register accumulator fragments, all zeroed.
  IND(depth); fprintf(fp,
    "wmma::fragment<wmma::accumulator, 16, 16, 16, float> _acc[%u];\n",
    t.rm * t.rn);
  IND(depth); fprintf(fp,
    "for (uint _i = 0u; _i < %uu; _i++) wmma::fill_fragment(_acc[_i], 0.0f);\n",
    t.rm * t.rn);
  // Cooperative loads move 8 bf16/fp16 as one uint4 along each unit-stride run
  // (A's K, a transposed B's K, a row-major B's N) if 8-aligned, else scalars.
  int a_vec = (t.kb % 8u == 0) && (k_extent % 8u == 0);
  int b_tvec = b_trans && (t.kb % 8u == 0) && (b_nstride % 8 == 0);
  int b_vec = !b_trans && (tile_n % 8u == 0) && (n_extent % 8u == 0);
  u32 a_elems = tile_m * t.kb, b_elems = t.kb * tile_n;
  // Emit-helper macros: cooperatively stage one K-block at element offset
  // BO, reading from source K-origin K0.
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
  // Inner MMA over the staged tiles at element offsets (ACO, BCO).
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

  // Stage block, barrier, MMA, barrier.
  IND(depth); fprintf(fp, "for (uint _k0 = 0u; _k0 < %uu; _k0 += %uu) {\n",
                      k_extent, t.kb);
  EMIT_LOAD_A("0u", "_k0");
  EMIT_LOAD_B("0u", "_k0");
  IND(depth + 1); fputs("__syncthreads();\n", fp);
  EMIT_MMA("0u", "0u");
  IND(depth + 1); fputs("__syncthreads();\n", fp);
  IND(depth); fputs("}\n", fp);
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
    // store_matrix_sync cannot narrow, so each warp stores to its 256-float
    // scratch (reserved by rmu_tc_pick_tile_cuda) and its lanes convert into C.
    IND(depth); fprintf(fp, "__shared__ float _cscratch[%u];\n", nwarps * 256u);
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

// Metal tiled matmul body (M and N both GLOBAL), tinygrad's TC + UPCAST + LOCAL
// stack (codegen/opt/heuristic.py:31-44, renderer/tc.py:138): each simdgroup
// holds RM x RN 8x8 accumulators over K-blocks staged once per threadgroup.
static void rmu_emit_matmul_tc_tiled(const char *a_name, const char *b_name,
                                     const char *c_name,
                                     u32 n_extent, u32 k_extent,
                                     RmuTcTile t, int c_is_bf, int stage_bf,
                                     int b_trans, i64 b_nstride,
                                     FILE *fp, u32 depth, int b_is_int8,
                                     int b_is_fp8, int b_fp8_e5m2,
                                     Term a_val, u32 a_m_axis, u32 a_k_axis,
                                     Term epilogue_value, Term opt_tc_term,
                                     Term m_range_term, Term n_range_term,
                                     u32 batch_extent, u32 m_extent, u32 a_lead,
                                     u32 batch_stride_a, u32 batch_stride_b,
                                     u32 batch_stride_c, u32 c_row_stride,
                                     int a_buf_bf, int b_buf_bf) {
  // THVM_FUSE_MATMUL_INPUT: a nonzero a_val is a fused A producer, computed at
  // each staged element with its M/K RANGE vars declared as `a<m>` / `a<k>`.
  int a_fused = (a_val != 0);
  // Two bf16 inputs stage as bfloat, so the MMA runs bfloat fragments into a
  // float accumulator at the native rate; otherwise staging widens to float.
  const char *sel = stage_bf ? "bfloat" : "float";
  // The prologue emits both fp8 decoders when any slot is fp8.
  const char *b_fp8_dec = b_fp8_e5m2 ? "thvm_fp8e5m2_decode"
                                     : "thvm_fp8e4m3_decode";
  u32 tile_m = t.local_m * t.rm * 8u;     // output rows per threadgroup
  u32 tile_n = t.local_n * t.rn * 8u;     // output cols per threadgroup
  u32 n_tiles_n = n_extent / tile_n;      // tg columns in the grid
  u32 nthreads = t.local_m * t.local_n * 32u;
  u32 nsg = t.local_m * t.local_n;        // simdgroups per threadgroup
  #define IND(D) for (u32 _i = 0; _i < (D); _i++) fputs("  ", fp)
  IND(depth); fprintf(fp,
    "/* TC tiled matmul: tile %ux%u, %ux%u simdgroups, %ux%u reg, KB=%u */\n",
    tile_m, tile_n, t.local_m, t.local_n, t.rm, t.rn, t.kb);
  // Double-buffered staging: the MMA consumes one A/B buffer pair while the
  // next K-block loads into the other, hiding global-load latency.
  IND(depth); fprintf(fp,
    "threadgroup %s _Asm[%u];\n", sel, 2u * tile_m * t.kb);
  IND(depth); fprintf(fp,
    "threadgroup %s _Bsm[%u];\n", sel, 2u * t.kb * tile_n);
  // bf16 output: simdgroup_store cannot narrow, so each simdgroup converts
  // through its own 64-float threadgroup slot.
  if (c_is_bf) {
    IND(depth); fprintf(fp, "threadgroup float _cstage[%u];\n", nsg * 64u);
    IND(depth); fputs("uint _sgi64 = sgi * 64u;\n", fp);
  }
  // THVM_TC_BATCHED: the grid is batch * m_tiles * n_tiles; each threadgroup
  // offsets A/B/C by the real batch strides, as attention operands are strided
  // views.  `auto` keeps each base pointer's own element type.
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
  // This threadgroup's output-tile origin.
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
  // Vector loads (float4/bfloat4) along the unit-stride runs need 4-aligned
  // extents and a device buffer whose element type is `sel`: a raw reinterpret
  // of a bf16 buffer as float4 reads the wrong bytes.
  int a_sel_match = (a_buf_bf != 0) == (stage_bf != 0);
  int b_sel_match = (b_buf_bf != 0) == (stage_bf != 0);
  int a_vec = a_sel_match && (t.kb % 4u == 0) && (k_extent % 4u == 0);
  int b_vec = b_sel_match && !b_trans && (tile_n % 4u == 0) && (n_extent % 4u == 0);
  // A transposed B (B[k][n] = W[n][k]) vector-loads 4 consecutive K of W and
  // scatters them to the strided threadgroup slots.
  int b_tvec = b_sel_match && b_trans && (t.kb % 4u == 0) && (b_nstride % 4 == 0);
  u32 a_elems = tile_m * t.kb, b_elems = t.kb * tile_n;
  // Cooperative load of one K-block: BUFOFF is the element offset of the
  // destination half of _Asm / _Bsm, KK0 the source K origin.
  #define EMIT_LOAD_A(BUFOFF, KK0)                                            \
    do {                                                                      \
      if (a_fused) {                                                          \
        /* THVM_FUSE_MATMUL_INPUT: compute the producer at (m = _tm + _r,     \
           k = KK0 + _c) in a scalar loop: its compute, not the load,         \
           dominates. */                                                      \
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
        /* q8: B = Transpose[Cast[W_int8, bf16]] reads 4 consecutive K of W   \
           as one char4 and casts to sel; b_nstride counts elements. */       \
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
        /* q8 scalar fallback: read each int8 element, cast to sel. */        \
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
      } else if (b_is_fp8 && b_tvec) {                                        \
        /* fp8 weight: the int8 path with a byte decode for the char cast. */ \
        IND(depth + 1); fprintf(fp,                                           \
          "for (uint _j = _lid; _j < %uu; _j += %uu) {\n", b_elems / 4u, nthreads);\
        IND(depth + 2); fprintf(fp,                                           \
          "uint _n = _j / %uu, _k4 = (_j %% %uu) * 4u;\n", t.kb / 4u, t.kb / 4u);\
        IND(depth + 2); fprintf(fp,                                           \
          "uchar4 _bv = *(device const uchar4*)(%s + (_tn + _n) * %lluu + (%s) + _k4);\n",\
          b_name, (unsigned long long)b_nstride, KK0);                        \
        IND(depth + 2); fprintf(fp,                                           \
          "(_Bsm + %s)[_k4 * %uu + _n] = (%s)%s(_bv.x);\n", BUFOFF, tile_n, sel, b_fp8_dec);\
        IND(depth + 2); fprintf(fp,                                           \
          "(_Bsm + %s)[(_k4 + 1u) * %uu + _n] = (%s)%s(_bv.y);\n", BUFOFF, tile_n, sel, b_fp8_dec);\
        IND(depth + 2); fprintf(fp,                                           \
          "(_Bsm + %s)[(_k4 + 2u) * %uu + _n] = (%s)%s(_bv.z);\n", BUFOFF, tile_n, sel, b_fp8_dec);\
        IND(depth + 2); fprintf(fp,                                           \
          "(_Bsm + %s)[(_k4 + 3u) * %uu + _n] = (%s)%s(_bv.w);\n", BUFOFF, tile_n, sel, b_fp8_dec);\
        IND(depth + 1); fputs("}\n", fp);                                     \
      } else if (b_is_fp8) {                                                  \
        /* fp8 scalar fallback: decode each weight byte. */                   \
        IND(depth + 1); fprintf(fp,                                           \
          "for (uint _j = _lid; _j < %uu; _j += %uu) {\n", b_elems, nthreads);\
        if (b_trans) {                                                        \
          IND(depth + 2); fprintf(fp,                                         \
            "uint _n = _j / %uu, _k = _j %% %uu;\n", t.kb, t.kb);             \
          IND(depth + 2); fprintf(fp,                                         \
            "(_Bsm + %s)[_k * %uu + _n] = (%s)%s(%s[(_tn + _n) * %lluu + (%s) + _k]);\n",\
            BUFOFF, tile_n, sel, b_fp8_dec, b_name, (unsigned long long)b_nstride, KK0);\
        } else {                                                             \
          IND(depth + 2); fprintf(fp,                                         \
            "(_Bsm + %s)[_j] = (%s)%s(%s[((%s) + _j / %uu) * %uu + _tn + _j %% %uu]);\n",\
            BUFOFF, sel, b_fp8_dec, b_name, KK0, tile_n, n_extent, tile_n);   \
        }                                                                     \
        IND(depth + 1); fputs("}\n", fp);                                     \
      } else if (b_tvec) {                                                    \
        /* B[k][n] = W[n][k]: vector-load 4 consecutive K of W, scatter them  \
           to the strided _Bsm slots. */                                      \
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
  // Each K-block step loads block _kit + 1 into the other buffer half, runs the
  // MMA on the current half, then one barrier guards both halves.
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
  // Store the register tile, C rows c_row_stride apart.  A bf16 C stages each
  // 8x8 tile through the simdgroup's f32 slot and converts, as simdgroup_store
  // cannot narrow.
  for (u32 mi = 0; mi < t.rm; mi++) {
    for (u32 ni = 0; ni < t.rn; ni++) {
      if (c_is_bf) {
        IND(depth); fprintf(fp,
          "simdgroup_store(_acc[%u], &_cstage[_sgi64], 8);\n", mi * t.rn + ni);
        IND(depth); fputs("simdgroup_barrier(mem_flags::mem_threadgroup);\n", fp);
        IND(depth); fprintf(fp,
          "{ uint _cb = (_tm + _sm + %uu) * %uu + _tn + _sn + %uu;\n",
          mi * 8u, c_row_stride, ni * 8u);
        if (epilogue_value != 0) {
          // The epilogue reads the matmul value at the dtype its unfused output
          // buffer would hold: a bf16-input matmul rounds to bf16 first, so the
          // fused result matches the separate kernels bit for bit.
          u32 mm_dt = DT_FP32;
          (void)term_dtype_in(opt_tc_term, 0, &mm_dt);
          if (stage_bf && mm_dt == DT_FP32) mm_dt = DT_BF16;
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
            "    %s[_cb + (_e / 8u) * %uu + (_e %% 8u)] = (bfloat)(", c_name, c_row_stride);
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
            c_name, c_row_stride);
        }
        IND(depth); fputs("simdgroup_barrier(mem_flags::mem_threadgroup);\n", fp);
      } else {
        IND(depth); fprintf(fp,
          "simdgroup_store(_acc[%u], &%s[(_tm + _sm + %uu) * %uu + _tn + _sn + %uu], %uu);\n",
          mi * t.rn + ni, c_name, mi * 8u, c_row_stride, ni * 8u, c_row_stride);
      }
    }
  }
  #undef IND
}

// The simdgroup / WMMA matmul template once rmu_detect_matmul_tc matches;
// returns 0, for the generic accumulator path, when the shapes, layouts or
// dtypes do not fit.
static int rmu_emit_matmul_tc(Term store, Term tc_red, FILE *fp,
                              u32 depth) {
  u64 sloc = term_val(store);
  Term addr_c = heap_read(sloc + 1);
  Term buf_c  = heap_read(sloc + 0);
  // THVM_FUSE_MATMUL_EPILOGUE: a store value that is a chain over the OPT_TC is
  // the epilogue, and opt_tc_term the matmul inside it.
  Term store_value   = heap_read(sloc + 2);
  Term epilogue_value = 0, opt_tc_term = 0;
  if (!(term_tag(store_value) == TAG_UOP && term_ext(store_value) == UOP_OPT
        && uop_opt_kind(store_value) == UOP_OPT_TC)) {
    if (!rmu_epilogue_find_single_tc(store_value, &opt_tc_term)) return 0;
    epilogue_value = store_value;
  }
  if (uop_reduce_n_axes(tc_red) != 1) return 0;
  u32 red_axis  = uop_reduce_axis(tc_red, 0);
  Term mul      = uop_reduce_src(tc_red);
  Term lhs      = rmu_peel_cast(heap_read(term_val(mul) + 0));
  Term rhs      = rmu_peel_cast(heap_read(term_val(mul) + 1));
  // THVM_FUSE_MATMUL_INPUT: a fused A producer becomes a_val, and `lhs` one of
  // its INDEX_E reads, so the layout discovery below sees a real {m,k} address.
  Term a_val    = 0;
  if (!(term_tag(lhs) == TAG_UOP && term_ext(lhs) == UOP_INDEX_E)
      && ru_fuse_matmul_input_on()) {
    Term rep = rmu_fused_first_index(lhs, 0);
    if (rep == 0) return 0;
    // Vetted below once the N axis is known (rmu_fused_producer_ok).
    a_val = lhs;
    lhs   = rep;
  }
  // Only the Metal tiled emitter computes a fused A inline.
  if (a_val != 0 && RMU_TARGET != CG_TARGET_METAL) return 0;
  Term buf_a    = heap_read(term_val(lhs) + 0);
  Term addr_a   = heap_read(term_val(lhs) + 1);
  Term buf_b    = heap_read(term_val(rhs) + 0);
  Term addr_b   = heap_read(term_val(rhs) + 1);
  // q8 matmul (B = Transpose[Cast[w_int8, bf16]], the cast peeled): the tiled
  // emitter stages the int8 weight cast to bfloat; the scale applies outside.
  int b_is_int8 = (rmu_slot_dtype(buf_b) == DT_INT8);
  // fp8 weights likewise stage as bytes decoded to bfloat (thvm_fp8*_decode).
  u32 b_slot_dt = rmu_slot_dtype(buf_b);
  int b_is_fp8      = (b_slot_dt == DT_FP8E4M3 || b_slot_dt == DT_FP8E5M2);
  int b_fp8_e5m2    = (b_slot_dt == DT_FP8E5M2);

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
    return 0;
  }

  // A batched matmul (attention) has a batch axis in both addresses besides K;
  // find it first so the M/N discovery skips it.  The addresses already carry
  // the batch offset, so only the grid decode and dispatch size change.
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

  // M is A's remaining axis and N is B's; their axis types pick loops or
  // thread binds.
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
  // The epilogue substitutes the tiled m/n for the M/N RANGEs of the output
  // address (the rmu_emit_matmul_tc_tiled hoist).
  Term m_range_term = 0, n_range_term = 0;
  if (epilogue_value != 0) {
    Term rcr[MAX_DIM]; u32 rcr_n = 0;
    rmu_collect_ranges(addr_c, rcr, &rcr_n);
    for (u32 i = 0; i < rcr_n; i++) {
      u32 aid = term_val(heap_read(term_val(rcr[i]) + 0));
      if (aid == m_axis_id) m_range_term = rcr[i];
      else if (aid == n_axis_id_v) n_range_term = rcr[i];
    }
    // Without both, the generic path renders the epilogue.
    if (m_range_term == 0 || n_range_term == 0) return 0;
  }

  // THVM_FUSE_MATMUL_INPUT: with N known, vet the fused A producer.
  if (a_val != 0 && !rmu_fused_producer_ok(a_val, n_axis_id_v, 0)) return 0;

  // simdgroup_load reads dst[i][j] = ptr[i*ld + j], so each operand needs a
  // unit-stride axis: the inner one (row-major) or the outer (transposed).
  i64 a_k = rmu_range_coeff(addr_a, red_axis);
  i64 a_m = rmu_range_coeff(addr_a, m_axis_id);
  i64 b_k = rmu_range_coeff(addr_b, red_axis);
  i64 b_n = rmu_range_coeff(addr_b, n_axis_id_v);
  // The C store steps M rows by c_m over a unit-stride N: c_m is N for packed
  // outputs, but heads * headDim for the M-major attention @V output.
  i64 c_m = rmu_range_coeff(addr_c, m_axis_id);
  i64 c_n = rmu_range_coeff(addr_c, n_axis_id_v);
  if (c_m <= 0) c_m = (i64)n_extent;   // non-batched packed output: c_m == N
  int a_trans, b_trans; i64 lda, ldb;
  if      (a_k == 1) { a_trans = 0; lda = a_m; }
  else if (a_m == 1) { a_trans = 1; lda = a_k; }
  else return 0;
  if      (b_n == 1) { b_trans = 0; ldb = b_k; }
  else if (b_k == 1) { b_trans = 1; ldb = b_n; }
  else return 0;
  if (lda <= 0 || ldb <= 0) return 0;
  // Both C stores write 8 contiguous N elements per row, so a batched output
  // whose N axis is not unit-stride goes to the scalar accumulator.
  if (batch_axis_id != 0xFFFFFFFFu && c_n != 1) return 0;

  // CUDA WMMA: 16x16x16 fp32-accumulate fragments need M, N, K % 16.
  if (RMU_TARGET == CG_TARGET_CUDA) {
    if ((m_extent % 16) != 0 || (n_extent % 16) != 0
        || (k_extent % 16) != 0) {
      return 0;
    }
    // A batched matmul takes the naive one-warp-per-tile path below, which
    // decodes the batch from the flat grid; the tiled path is 2-D only.
    // THVM_TC_BATCHED=0 sends batched matmuls to the scalar accumulator.
    if (batch_axis_id != 0xFFFFFFFFu) {
      char const *e = getenv("THVM_TC_BATCHED");
      if (e && e[0] == '0') return 0;
    }
    // WMMA a/b fragments need a 16-bit float source: fp16 on sm_70+, bf16 on
    // sm_80+; f32 operands would not compile, so they bail.
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
    // The tiled path needs an untransposed A and M, N both GLOBAL, so each
    // block owns one output tile of the (M/tile_m)*(N/tile_n) launch.
    {
      int m_par_c = (m_axis_type == 5 /* KAX_GLOBAL */);
      int n_par_c = (n_axis_type == 5 /* KAX_GLOBAL */);
      int c_is_bf = (dt_c == DT_BF16 || dt_c == DT_FP16);
      RmuTcTile tile;
      if (batch_axis_id == 0xFFFFFFFFu && !a_trans && m_par_c && n_par_c
          && rmu_tc_pick_tile_cuda(m_extent, n_extent, k_extent, c_is_bf, &tile)) {
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
    // A fragment's transpose is its declared row_major / col_major layout; a
    // transposed operand loads col_major with the same leading dim.
    const char *a_layout = a_trans ? "wmma::col_major" : "wmma::row_major";
    const char *b_layout = b_trans ? "wmma::col_major" : "wmma::row_major";
    // A bf16/fp16 C cannot take store_matrix_sync of a float fragment: it goes
    // through a per-warp f32 scratch that the lanes convert into C.
    int c_direct = (dt_c == DT_FP32);
    u32 n_tiles_n_w = n_extent / 16;
    for (u32 d = 0; d < depth; d++) fputs("  ", fp);
    fputs("/* TC WMMA matmul (nvcuda::wmma 16x16x16) */\n", fp);
    // One warp per 16x16 output tile, tiles row-major over (m_tile, n_tile).
    for (u32 d = 0; d < depth; d++) fputs("  ", fp);
    fputs("uint _warp = tid / 32u;\n", fp);
    // The flat launch has one thread per output element, more warps than
    // tiles: surplus warps return before any load or store (and so before the
    // bf16 store's __syncwarp).  A batched warp also decodes its batch index.
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
    // lda/ldb, decoded from the address coefficients, are each fragment's
    // leading dim in either layout.
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
      // An f32 C stores directly, rows n_extent apart.
      for (u32 d = 0; d < depth; d++) fputs("  ", fp);
      fprintf(fp, "wmma::store_matrix_sync(&%s[", rmu_buf_name(buf_c));
      rmu_emit_term(addr_c, fp);
      fprintf(fp, "], _c_frag, %u, wmma::mem_row_major);\n", n_extent);
    } else {
      // A bf16/fp16 C stages through a per-warp f32 scratch sized for 32 warps
      // (1024 threads), indexed by the block-local warp `sgi` since `tid` is
      // grid-global; the lanes then convert into C rows n_extent apart.
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

  {
    u32 dt_a = rmu_slot_dtype(buf_a);
    u32 dt_b = rmu_slot_dtype(buf_b);
    // Metal takes f32 and bf16 operands (simdgroup_load needs the pointer type
    // to match the fragment's, as in tinygrad's MetalRenderer.tensor_cores); an
    // int8 / fp8 B is decoded only by the tiled path's staging.
    if (dt_a != DT_FP32 && dt_a != DT_BF16) return 0;
    if (dt_b != DT_FP32 && dt_b != DT_BF16 && !b_is_int8 && !b_is_fp8) return 0;
  }

  // Parallel TC: a GLOBAL M or N gives each threadgroup (one simdgroup) its own
  // output tiles, so no write guard is needed; with neither GLOBAL the body
  // runs guarded and sequential.
  int m_par = (m_axis_type == 5 /* KAX_GLOBAL */);
  int n_par = (n_axis_type == 5 /* KAX_GLOBAL */);
  int parallel_tc = (m_par || n_par);
  u32 n_tiles_n = n_extent / 8;

  // A bf16 C needs the staged store, which needs one simdgroup per threadgroup
  // and a flat body: only the both-GLOBAL dispatch gives both.
  if (uop_buffer_dtype(buf_c) == DT_BF16 && !(m_par && n_par)) return 0;

  // The tiled path needs M and N both GLOBAL, one tile per threadgroup
  // (cg_tile_metal_dispatch_shape).  A fused A stages at B's type, as the MMA
  // needs matching fragment types; a plain A stages at its own.
  u32 _dtb = uop_buffer_dtype(buf_b);
  u32 _dta = (a_val != 0)
      ? ((_dtb == DT_BF16 || b_is_int8 || b_is_fp8) ? DT_BF16 : DT_FP32)
      : uop_buffer_dtype(buf_a);
  // An int8 / fp8 B fits only the tiled path (a quantized A bailed above).
  int _tc_dtype_ok = (_dta == DT_FP32 || _dta == DT_BF16)
                  && (_dtb == DT_FP32 || _dtb == DT_BF16 || b_is_int8 || b_is_fp8);
  // The tiled emitter takes a row-major A and a row-major or transposed B; a
  // transposed A takes the per-tile parallel_tc body, which renders the full
  // affine addresses.
  int is_batched = (batch_axis_id != 0xFFFFFFFFu);
  if ((!is_batched || rmu_tc_batched_on()) && !a_trans && m_par && n_par
      && RMU_TARGET == CG_TARGET_METAL && _tc_dtype_ok
      // A batched tiled matmul takes no fused A, quantized B or epilogue.
      && !(is_batched && (a_val != 0 || b_is_int8 || b_is_fp8
                          || epilogue_value != 0))) {
    RmuTcTile tile;
    if (rmu_tc_pick_tile(m_extent, n_extent, k_extent,
                         is_batched ? batch_extent : 0u, &tile)) {
      // rmu_buf_name returns one static buffer, so copy each name first.
      char a_nm[24], b_nm[24], c_nm[24];
      snprintf(a_nm, sizeof(a_nm), "%s", rmu_buf_name(buf_a));
      snprintf(b_nm, sizeof(b_nm), "%s", rmu_buf_name(buf_b));
      snprintf(c_nm, sizeof(c_nm), "%s", rmu_buf_name(buf_c));
      int c_is_bf = (uop_buffer_dtype(buf_c) == DT_BF16);
      // Stage bfloat when A is bf16 and B is bf16 or a quantized weight decoded
      // to bfloat, so the MMA runs at the native bf16 rate.
      int stage_bf = (_dta == DT_BF16)
                     && (_dtb == DT_BF16 || b_is_int8 || b_is_fp8);
      // The epilogue rides only the bf16 staging store; an f32-output epilogue
      // takes the generic path.
      if (epilogue_value != 0 && !c_is_bf) return 0;
      // Batched operands use their real strides (rmu_range_coeff): attention's
      // Q/K are strided {heads, seq, head_dim} views.
      u32 a_lead = is_batched ? (u32)lda : k_extent;
      i64 bsa = is_batched ? rmu_range_coeff(addr_a, batch_axis_id) : 0;
      i64 bsb = is_batched ? rmu_range_coeff(addr_b, batch_axis_id) : 0;
      i64 bsc = is_batched ? rmu_range_coeff(addr_c, batch_axis_id) : 0;
      // C's real row stride, heads * headDim for the M-major @V output.
      u32 c_row = is_batched ? (u32)c_m : n_extent;
      // Vector loads reinterpret the buffer as `sel4`, valid only when its type
      // is `sel`; a fused A or a quantized B never takes that load.
      int a_buf_bf = (a_val != 0) ? stage_bf : (uop_buffer_dtype(buf_a) == DT_BF16);
      int b_buf_bf = (b_is_int8 || b_is_fp8)
                       ? stage_bf : (uop_buffer_dtype(buf_b) == DT_BF16);
      rmu_emit_matmul_tc_tiled(a_nm, b_nm, c_nm, n_extent,
                               k_extent, tile, c_is_bf, stage_bf,
                               b_trans, ldb, fp, depth, b_is_int8,
                               b_is_fp8, b_fp8_e5m2,
                               a_val, m_axis_id, red_axis,
                               epilogue_value, opt_tc_term,
                               m_range_term, n_range_term,
                               is_batched ? batch_extent : 0u, m_extent,
                               a_lead, (u32)bsa, (u32)bsb, (u32)bsc, (u32)c_row,
                               a_buf_bf, b_buf_bf);
      return 1;
    }
  }

  // An epilogue off the tiled path takes the generic accumulator.
  if (epilogue_value != 0) return 0;

  // So does a fused A: the paths below read A as a flat buffer.
  if (a_val != 0) return 0;

  if (parallel_tc) {
    // No guard.  Bind axes per axis_type.
    for (u32 d = 0; d < depth; d++) fputs("  ", fp);
    fputs("/* parallel TC: m/n bound to tg; one SG per output tile */\n", fp);
    if (m_par && n_par && is_batched) {
      // Batched: tg linearises (batch, m_tile, n_tile); the addresses already
      // carry the batch offset.
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
    // Guarded sequential path: with several simdgroups launched, each would
    // write the same outputs, so only the first simdgroup of the first
    // threadgroup runs.
    for (u32 d = 0; d < depth; d++) fputs("  ", fp);
    fputs("if (sgi == 0u && tg == 0u) {\n", fp);
    for (u32 d = 0; d < depth + 1; d++) fputs("  ", fp);
    fprintf(fp, "for (uint a%u = 0; a%u < %u; a%u += 8) {\n",
            m_axis_id, m_axis_id, m_extent, m_axis_id);
    for (u32 d = 0; d < depth + 2; d++) fputs("  ", fp);
    fprintf(fp, "for (uint a%u = 0; a%u < %u; a%u += 8) {\n",
            n_axis_id_v, n_axis_id_v, n_extent, n_axis_id_v);
  }

  // Openers: none for both GLOBAL, one loop for one GLOBAL, else a guard and
  // two loops.
  u32 body_depth;
  if (parallel_tc) {
    body_depth = (m_par && n_par) ? depth : (depth + 1);
  } else {
    body_depth = depth + 3;
  }

  // Fragments take each operand's buffer type (a bfloat buffer needs a bfloat
  // fragment); the accumulator stays f32 across K, as bfloat would drift.
  const char *a_el = (uop_buffer_dtype(buf_a) == DT_BF16) ? "bfloat" : "float";
  const char *b_el = (uop_buffer_dtype(buf_b) == DT_BF16) ? "bfloat" : "float";
  int c_is_bf = (uop_buffer_dtype(buf_c) == DT_BF16);
  // The MMA needs A and B of one type, and simdgroup_load cannot carry a batch
  // offset, so mixed-type and batched matmuls stage each 8x8 sub-tile through
  // threadgroup float; only the both-GLOBAL dispatch owns that scratch alone.
  int need_stage = (is_batched || strcmp(a_el, b_el) != 0)
                   && parallel_tc && m_par && n_par;
  fputs("/* TC simdgroup_matrix matmul */\n", fp);
  if (need_stage) {
    // A[m+i, k0+j] = _ab + i*a_m + j*a_k; B[k0+i, n+j] = _bb + i*b_k + j*b_n.
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
  // A bf16 C stages the 8x8 tile through threadgroup f32, as simdgroup_store
  // cannot narrow; the both-GLOBAL gate gives this simdgroup the scratch alone.
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
            rmu_buf_name(buf_c), (u32)c_m);
    for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
    fputs("}\n", fp);
  } else {
    for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
    fprintf(fp, "simdgroup_store(_c_mat, &%s[", rmu_buf_name(buf_c));
    rmu_emit_term(addr_c, fp);
    fprintf(fp, "], %u);\n", (u32)c_m);
  }

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

// `acc = acc + body;`, or for MAX `acc = acc > body ? acc : body;`.
static void rmu_emit_group_combine_line(const char *acc_lhs, u32 red_kind,
                                        Term body, FILE *fp) {
  fprintf(fp, "%s = ", acc_lhs);
  fprintf(fp, "%s", acc_lhs);
  if (red_kind == REDUCE_SUM)      fputs(" + ", fp);
  else if (red_kind == REDUCE_MAX) fputs(" > ", fp);
  else                              fputs(" + ", fp);
  rmu_emit_term(body, fp);
  if (red_kind == REDUCE_MAX) {
    fprintf(fp, " ? %s : ", acc_lhs);
    rmu_emit_term(body, fp);
  }
  fputs(";\n", fp);
}

// Cooperative GROUP_REDUCE (KOP_GROUPTOP): each thread folds a strided slice of
// the group axis into _acc[tt]; group index 0 of each LOCAL row then combines
// and stores, and the caller's open output loops close.  0 = not emitted.
static int rmu_emit_group_reduce(Term buf, Term addr,
                                 Term const *red_ranges,
                                 u32 const *red_kind_opts,
                                 u32 const *red_factor_opts,
                                 u32 red_n_axes,
                                 Term red_src,
                                 u32 red_kind,
                                 FILE *fp, u32 body_depth,
                                 u32 n_out, int const *needs_close) {
  // hand_opts groups one reduce axis at a time; bail on more.
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
  // C has no shared memory.
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
  // One shared slot per thread: local_total * group_extent, the group being
  // the innermost `tt` dim.
  u32 grp_local_total = (RMU_GROUP_LOCAL_TOTAL > 0) ? RMU_GROUP_LOCAL_TOTAL : 1;
  u32 acc_size        = grp_local_total * group_extent;
  for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
  fprintf(fp, "%s %s[%u];\n", shared_kw, acc_name, acc_size);
  for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
  fprintf(fp, "%s = ", acc_slot);
  rmu_emit_reduce_init(red_kind, DT_FP32, fp);
  fputs(";\n", fp);
  // Pre-loop barrier so every thread sees a clean slot.
  for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
  fprintf(fp, "%s\n", barrier_stmt);
  // Serial loops over the non-GROUP reduce axes wrap the strided GROUP walk.
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
  // The group index is tt % group_extent; the LOCAL row is tt / group_extent.
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
  for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
  fprintf(fp, "%s\n", barrier_stmt);
  // Group index 0 of each LOCAL row sums the row's group_extent slots
  // [tt, tt + group_extent) and stores its output.
  for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
  fprintf(fp, "if (tt %% %uu == 0u) {\n", group_extent);
  for (u32 d = 0; d < body_depth + 1; d++) fputs("  ", fp);
  fputs("float _total = ", fp);
  rmu_emit_reduce_init(red_kind, DT_FP32, fp);
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

// Lane substitution for register-blocked reduces: each RANGE at up_axes[i]
// becomes the constant up_vals[i].  With two UPCAST axes the compiler shares
// each load across the lanes that do not use it (Um + Uw loads for Um*Uw MADs).
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

// 1 if `range_term` is the UPCAST/UNROLL inner half of a split reduce axis: not
// in `addr` and absent from the REDUCE's axes, it loops inside the reduce, as
// tinygrad's expand_reduce folds unrolled lanes (codegen/__init__.py:49-63).
static int rmu_range_is_inner_reduce_decomp(Term range_term, u32 opt_kind,
                                            Term addr) {
  if (term_tag(range_term) != TAG_UOP || term_ext(range_term) != UOP_RANGE) return 0;
  if (opt_kind != UOP_OPT_UPCAST && opt_kind != UOP_OPT_UNROLL) return 0;
  u32 axis_id = (u32)term_val(heap_read(term_val(range_term) + 0));
  // Split output axes surface both halves in `addr`, which can exceed MAX_DIM.
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

// Move the inner split ranges to the tail of out_ranges and return the count
// of true output ranges before them.
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

// Owners of the inner split axes (uop_reduce_owns_split_axis): nested bodies
// all hold the inner half, and only its owner may loop it, or an enclosing SUM
// re-accumulates.  Owner 0 (no split composition): every reduce folds it.
static u32  RMU_SPLIT_AID  [RMU_MAX_RANGES];
static Term RMU_SPLIT_OWNER[RMU_MAX_RANGES];
static u32  RMU_SPLIT_N = 0;

static int rmu_reduce_folds_split_axis(Term red, u32 aid) {
  for (u32 i = 0; i < RMU_SPLIT_N; i++) {
    if (RMU_SPLIT_AID[i] == aid) {
      return RMU_SPLIT_OWNER[i] == 0 || RMU_SPLIT_OWNER[i] == red;
    }
  }
  return 1;
}

// Open the inner split ranges as unrolled loops; returns the new depth, and the
// caller closes them after the combine.
static u32 rmu_emit_inner_reduce_decomp_loops(Term const *ranges,
                                              u32 const *kinds,
                                              u32 const *factors,
                                              u32 n, FILE *fp, u32 depth) {
  (void)kinds;  // axis_type carries everything we need; kinds is for symmetry
  for (u32 i = 0; i < n; i++) {
    Term r = ranges[i];
    if (term_tag(r) != TAG_UOP || term_ext(r) != UOP_RANGE) continue;
    u32 ext = (u32)term_val(heap_read(term_val(r) + 2));
    // An explicit UNROLL factor gets its pragma on every target; the default
    // small-K unroll is GPU-only, as clang -O3 unrolls short loops itself.
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

// 1 if `t` uses an UPCAST axis of up_axes[0..n_up): a subterm without one is
// shared by every lane and hoists out of the per-lane MADs (path #2).
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

// Hoist each INDEX_E of red_src whose address no UPCAST axis reaches into a
// local `_sh<red_axis>_<i>`, declared here and shared by every lane (path #2).
// Returns how many it installed in the hoist map, which the caller clears.
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
      int dup = 0;
      for (u32 i = 0; i < n_seen; i++) {
        if (seen[i] == cur) { dup = 1; break; }
      }
      if (!dup && n_seen < RMU_HOIST_MAX) seen[n_seen++] = cur;
      if (dup) continue;
      // Only the address can depend on a lane.
      Term addr = heap_read(term_val(cur) + 1);
      Term buf  = heap_read(term_val(cur) + 0);
      if (rmu_term_depends_on_upcast(addr, up_axes, n_up)) {
        // A per-lane load; a nested INDEX_E in its address may still be shared.
        if (term_tag(addr) == TAG_UOP && sp < 256) stack[sp++] = addr;
        continue;
      }
      // An unresolved buffer renders as the literal 0.0f; nothing to share.
      const char *bn = rmu_buf_name_or_null(buf);
      if (bn == NULL) continue;
      if (n_hoist >= RMU_HOIST_MAX) continue;
      char tmp[32];
      int  tn = snprintf(tmp, sizeof(tmp), "_sh%u_%u", red_axis, n_hoist);
      if (tn <= 0 || (u32)tn + 1 > name_pool_cap - pool_used) continue;
      char *slot_name = name_pool + pool_used;
      memcpy(slot_name, tmp, (u32)tn + 1);
      pool_used += (u32)tn + 1;
      if (!rmu_hoist_add(cur, slot_name)) continue;
      // Emit:  T _sh<r>_<i> = in<k>[<addr>];  T holds the buffer's values.
      for (u32 d = 0; d < indent_depth; d++) fputs("  ", fp);
      fprintf(fp, "%s %s = %s[", rmu_hold_type(rmu_slot_dtype(buf)), slot_name, bn);
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

// 1 and *out for an integer UOP_CONST, else 0 (as index_simplify.c's static
// uop_iconst_value, which the unity build includes after this file).
static int rmu_const_int(Term t, i64 *out) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_CONST) return 0;
  Term num = heap_read(term_val(t) + 0);
  if (term_tag(num) != TAG_NUM) return 0;
  if (!dtype_is_int(term_ext(num))) return 0;
  *out = (i64)(i32)term_val(num);  // DT_INT32 sign-extension
  return 1;
}

// 1 if `t` has a RANGE leaf `axis_id`; an OPT is transparent.
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

// 1 if `t` is index math, which a hoisted local holds in an `int`: an IWHERE
// is when both branches are (a masked address, INVALID on one side), not when
// it selects a value (a padded load `j < 5 ? w[j] : 0`).
static int rmu_term_is_index(Term t) {
  if (term_tag(t) != TAG_UOP) {
    return 1;
  }
  u64 loc = term_val(t);
  switch (term_ext(t)) {
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL: case UOP_IDIV:
    case UOP_IMOD: case UOP_ILT:  case UOP_IAND: case UOP_IOR:
    case UOP_IXOR: case UOP_ISHR: case UOP_ISHL: case UOP_INVALID: case UOP_RANGE:
    case UOP_DEFINE_VAR:
      return 1;
    case UOP_CONST:
      return !dtype_is_float(term_ext(heap_read(loc)));
    case UOP_OPT:
      return rmu_term_is_index(heap_read(loc));
    case UOP_IWHERE:
      return rmu_term_is_index(heap_read(loc + 1))
          && rmu_term_is_index(heap_read(loc + 2));
    default:
      return 0;
  }
}

// 1 if `t` is worth a local: a load, or integer / float arithmetic the lane
// fan-out would replicate; a bare leaf prints as one token anyway.
static int rmu_hoist_worth(Term t) {
  if (term_tag(t) != TAG_UOP) return 0;
  u32 op = term_ext(t);
  if (op == UOP_INDEX_E) return 1;
  switch (op) {
    case UOP_IWHERE: case UOP_IAND: case UOP_IOR: case UOP_IXOR:
    case UOP_ILT:    case UOP_IDIV: case UOP_IMOD: case UOP_ISHR: case UOP_ISHL:
    case UOP_IADD:   case UOP_ISUB: case UOP_IMUL:
    case UOP_ADD:    case UOP_MUL:  case UOP_CMPLT: case UOP_CMPEQ:
      return 1;
    default:
      return 0;
  }
}

// 1 if every RANGE of `t` is in allowed[0..n_allowed): a hoisted local must
// not use an axis opened deeper than the hoist point (a nested reduce's K).
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

// Hoist each maximal lane-invariant subterm worth a local, as tinygrad's
// expander leaves invariant UOps unexpanded (codegen/__init__.py:78-84): index
// math as `int`, a value as `__auto_type`, which keeps its C type (C only).
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
    const char *ty = rmu_term_is_index(t) ? "int" : "__auto_type";
    char tmp[40];
    int tn = snprintf(tmp, sizeof(tmp), "_sh%u_%u", red_axis, *n_hoist);
    if (tn <= 0 || (u32)tn + 1 > name_pool_cap - *pool_used) return 0;
    char *slot_name = name_pool + *pool_used;
    memcpy(slot_name, tmp, (u32)tn + 1);
    // Print the RHS before the key enters the map, or it would print `_shN`.
    for (u32 d = 0; d < indent_depth; d++) fputs("  ", fp);
    fprintf(fp, "%s %s = ", ty, slot_name);
    rmu_emit_term(t, fp);
    fputs(";\n", fp);
    if (!rmu_hoist_add(t, slot_name)) return 0;
    *pool_used += (u32)tn + 1;
    (*n_hoist)++;
    return 1;
  }
  // Otherwise descend to the invariant subterms; an OPT is transparent.
  u64 loc = term_val(t);
  if (op == UOP_OPT) {
    rmu_hoist_lane_invariant(heap_read(loc + 0), up_axes, n_up, red_axis,
                             scope_axes, n_scope,
                             name_pool, name_pool_cap, pool_used, n_hoist,
                             fp, indent_depth);
    return 0;
  }
  // A nested REDUCE's body runs inside its own deeper K-loop; it hoists its
  // own invariants when it emits.
  if (op == UOP_REDUCE) return 0;
  u8 ar = uop_arity(op);
  for (u8 i = 0; i < ar && i < MAX_UOP_SRC; i++)
    rmu_hoist_lane_invariant(heap_read(loc + i), up_axes, n_up, red_axis,
                             scope_axes, n_scope,
                             name_pool, name_pool_cap, pool_used, n_hoist,
                             fp, indent_depth);
  return 0;
}

// 1 and *coeff, the coefficient of `axis_id` in index expression `t`, when `t`
// is affine in it over IADD / ISUB / IMUL-by-const; 0 when the axis sits under
// any other op (a gather).  A subterm without the axis contributes 0.
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
  // An OPT carries no arithmetic.
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

// rmu_axis_affine_coeff for an address; 0 is a gather, not vectorizable.
static int rmu_axis_index_stride(Term addr, u32 axis_id, i64 *stride) {
  return rmu_axis_affine_coeff(addr, axis_id, stride);
}

// STORE(buf, addr, REDUCE(src)): an accumulator hoisted outside the reduce
// loops, or the TC template for an OPT(TC) value (uop_recognise_tc).  Returns 0
// for the generic rmu_emit_store path.
static int rmu_emit_store_reduce(Term store, FILE *fp, u32 depth) {
  if (term_tag(store) != TAG_UOP || term_ext(store) != UOP_STORE) return 0;
  u64 sloc = term_val(store);
  Term value = heap_read(sloc + 2);
  // A TC matmul takes its template when the tiles fit, else the accumulator.
  Term tc_red = 0;
  if (rmu_detect_matmul_tc(store, &tc_red)) {
    if (rmu_emit_matmul_tc(store, tc_red, fp, depth)) return 1;
    for (u32 d = 0; d < depth; d++) fputs("  ", fp);
    fputs("/* TC tile mismatch: falling back to scalar accumulator */\n", fp);
    value = tc_red;
  }
  if (term_tag(value) != TAG_UOP || term_ext(value) != UOP_REDUCE) return 0;
  Term buf  = heap_read(sloc + 0);
  Term addr = heap_read(sloc + 1);
  u64 rloc      = term_val(value);
  Term red_src  = heap_read(rloc + 0);
  // Nested reduces, REDUCE(REDUCE(...)) included, take rmu_emit_store, which
  // nests each reduce inside the one whose axis its body reads.
  if (rmu_term_has_reduce(red_src, 0)) return 0;
  // The reduce axes open as nested loops in REDUCE.src[1:] order.
  u32  red_kind   = uop_reduce_kind(value);
  u32  red_dt     = rmu_dtype_of(value);
  u32  red_n_axes = uop_reduce_n_axes(value);
  if (red_n_axes == 0 || red_n_axes > MAX_DIM) return 0;
  u32  red_axes[MAX_DIM];
  for (u32 ai = 0; ai < red_n_axes; ai++) {
    red_axes[ai] = uop_reduce_axis(value, ai);
  }
  u32  red_axis = red_axes[0];     // names the accumulator

  // RMU_MAX_RANGES cap: splits of K and of several output axes surface both
  // halves of each, up to 2 * rank leaves.
  Term ranges[RMU_MAX_RANGES];
  u32  opt_kinds  [RMU_MAX_RANGES] = {0};
  u32  opt_factors[RMU_MAX_RANGES] = {0};
  u32  n_ranges = 0;
  rmu_collect_ranges_rec_cap(addr,    ranges, opt_kinds, opt_factors,
                             &n_ranges, RMU_MAX_RANGES, RMU_NO_OPT, 0);
  rmu_collect_ranges_rec_cap(red_src, ranges, opt_kinds, opt_factors,
                             &n_ranges, RMU_MAX_RANGES, RMU_NO_OPT, 0);

  // Split the ranges into the reduce axes, in REDUCE.src[1:] order, and the
  // rest, in collection order.
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

  // The inner halves of a split K loop inside the reduce, not around it.
  u32 n_out_true = rmu_partition_out_ranges(out_ranges, out_kinds,
                                            out_factors, n_out, addr);
  u32 n_inner    = n_out - n_out_true;

  // Parallel accumulators (path #1 of docs/tinygrad_late_passes.md): UPCAST
  // output axes become F accumulators over one loop, on C too (independent
  // lanes vectorize without -ffast-math), a split K's inner halves looping
  // inside the reduce as in the single-accumulator path (tinygrad's CPU gemm,
  // UPCAST with an UNROLLed K); not beside GROUP_REDUCE.
  int multi_axis_reject = 0;
  for (u32 ai = 0; ai < red_n_axes; ai++) {
    if (red_kind_opts[ai] == UOP_OPT_GROUP_REDUCE) {
      multi_axis_reject = 1; break;
    }
  }
  RmuConvMSubstCtx pa_up = {0};
  u32 pa_up_exts[RMU_CONV_UPCAST_MAX] = {0};
  u32 pa_total = 1;
  if (!multi_axis_reject) {
    for (u32 i = 0; i < n_out_true && pa_up.n < RMU_CONV_UPCAST_MAX; i++) {
      if (out_kinds[i] != UOP_OPT_UPCAST) continue;
      Term r = out_ranges[i];
      if (term_tag(r) != TAG_UOP || term_ext(r) != UOP_RANGE) continue;
      u32 e  = (u32)term_val(heap_read(term_val(r) + 2));
      if (e < 2 || e > 16) continue;
      // At most 32 accumulators, tinygrad's upcast_size budget.
      if ((u64)pa_total * e > 32) continue;
      pa_up.up_axes[pa_up.n] = (u32)term_val(heap_read(term_val(r) + 0));
      pa_up_exts[pa_up.n]    = e;
      pa_up.n++;
      pa_total *= e;
    }
  }
  if (pa_up.n > 0) {
    // The non-UPCAST output axes open as loops or thread binds.
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
      fprintf(fp, "%s %s_%u = ", rmu_hold_type(red_dt), pa_acc_name, k);
      rmu_emit_reduce_init(red_kind, red_dt, fp);
      fputs(";\n", fp);
    }
    // One loop per reduce axis, outermost first; OPT(UNROLL) keeps its
    // factor, and a short unwrapped axis unrolls fully.
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
    u32 pa_inner_depth = rmu_emit_inner_reduce_decomp_loops(
        out_ranges + n_out_true, out_kinds + n_out_true,
        out_factors + n_out_true, n_inner, fp, pa_red_depth);
    // Shared loads (path #2) hoist once per K iteration, before the F MADs.
    rmu_hoist_clear();
    char hoist_name_pool[RMU_HOIST_MAX * 32] = {0};
    u32 n_hoist = rmu_hoist_shared_loads(red_src,
                                         pa_up.up_axes, pa_up.n,
                                         red_axis,
                                         hoist_name_pool,
                                         (u32)sizeof(hoist_name_pool),
                                         fp, pa_inner_depth);
    (void)n_hoist;  // emitted inline; declarations precede the F MADs
    // F MADs, each with the UPCAST RANGEs replaced by constants; the innermost
    // UPCAST axis varies fastest, so adjacent lanes share outer-axis math.
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
      char lane_acc[48];
      snprintf(lane_acc, sizeof(lane_acc), "%s_%u", pa_acc_name, k);
      for (u32 d = 0; d < pa_inner_depth; d++) fputs("  ", fp);
      rmu_emit_reduce_combine(lane_acc, red_kind, red_dt, rk, fp);
    }
    rmu_hoist_clear();
    for (u32 i = 0; i < n_inner; i++) {
      pa_inner_depth--;
      for (u32 d = 0; d < pa_inner_depth; d++) fputs("  ", fp);
      fputs("}\n", fp);
    }
    // Close all reduce loops (innermost first).
    for (i32 ai = (i32)red_n_axes - 1; ai >= 0; ai--) {
      pa_red_depth--;
      for (u32 d = 0; d < pa_red_depth; d++) fputs("  ", fp);
      fputs("}\n", fp);
    }
    // C, f32 out, unit-stride innermost UPCAST axis: lanes store as float4, as
    // tinygrad's memory_coalescing (codegen/late/coalesce.py) groups stores; a
    // scalar store makes clang's SLP vectorizer decline the lane chain.
    u32 pa_vaxis = pa_up.up_axes[pa_up.n - 1];
    u32 pa_vext  = pa_up_exts[pa_up.n - 1];
    u32 pa_out_dt = uop_buffer_dtype(buf);
    int pa_vec4 = 0;
    if (RMU_TARGET == CG_TARGET_C && pa_out_dt == DT_FP32 && pa_vext % 4 == 0) {
      i64 s;
      if (rmu_axis_index_stride(addr, pa_vaxis, &s) && s == 1) pa_vec4 = 1;
    }
    if (pa_vec4) {
      // The innermost UPCAST axis varies fastest: lanes 4g..4g+3 are adjacent.
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

  // Single accumulator: UPCAST output axes open as unrolled loops.
  int needs_close[RMU_MAX_RANGES] = {0};
  u32 body_depth = rmu_emit_output_loops(addr, out_ranges, out_kinds,
                                         out_factors, n_out_true, depth, fp,
                                         needs_close);
  // A GROUP_REDUCE declares its own shared `_acc`, so it branches off before
  // the scalar accumulator would collide with it.
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
  fprintf(fp, "%s %s = ", rmu_hold_type(red_dt), acc_name);
  rmu_emit_reduce_init(red_kind, red_dt, fp);
  fputs(";\n", fp);
  // One loop per reduce axis, outermost first: OPT(UNROLL) keeps its factor,
  // and on GPU a short unwrapped axis unrolls fully.
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
  // Inner split halves loop inside the innermost reduce loop, folding into the
  // one accumulator (tinygrad expand_reduce, codegen/__init__.py:49-63).
  u32 reduce_body_depth = rmu_emit_inner_reduce_decomp_loops(
      out_ranges + n_out_true, out_kinds + n_out_true,
      out_factors + n_out_true, n_inner, fp, red_depth);
  // Combine inside the (innermost) reduce loop.
  for (u32 d = 0; d < reduce_body_depth; d++) fputs("  ", fp);
  rmu_emit_reduce_combine(acc_name, red_kind, red_dt, red_src, fp);
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

// parent_idx[] of a reduce emitted at the top level.
#define RMU_REDUCE_NO_PARENT 0xFFFFFFFFu

// Register blocking in the generic store path (C only): each reduce keeps
// n_lanes accumulators `_acc<axis>_<k>` over one shared loop, independent
// chains clang vectorizes without -ffast-math.
typedef struct {
  u32 n;                          // count of register-blocked axes
  u32 up_axes[RMU_CONV_UPCAST_MAX];
  u32 up_exts[RMU_CONV_UPCAST_MAX];
  u32 n_lanes;                    // prod(up_exts)
} RmuLaneBlock;

// `t` for lane k: each UPCAST RANGE becomes its lane value (row-major in k).
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

// Emit reduce `red` at emit_depth; each reduce whose parent_idx is self_idx
// (its body uses this reduce's axes) nests inside its loops, as tinygrad nests
// ranges (uop/ops.py:470-499, codegen/late/linearizer.py:53-85).
static void rmu_emit_one_reduce_lb(Term red, u32 emit_depth,
                                const Term *reduces,
                                const u32 *parent_idx, u32 self_idx,
                                u32 n_reduces, FILE *fp, Term store_addr,
                                const RmuLaneBlock *lb) {
  u32  r_kind   = uop_reduce_kind(red);
  u32  r_dt     = rmu_dtype_of(red);
  // One accumulator, named after axis 0, and one loop per reduce axis.
  u32  r_n_axes = uop_reduce_n_axes(red);
  if (r_n_axes == 0) return;
  u32  r_axis = uop_reduce_axis(red, 0);     // accumulator name axis
  Term r_src  = uop_reduce_src(red);
  u32  n_lanes = (lb != NULL && lb->n_lanes > 1) ? lb->n_lanes : 1;
  // GPU targets register-block in rmu_emit_store_reduce instead.
  if (n_lanes > 1 && RMU_TARGET != CG_TARGET_C) n_lanes = 1;
  char acc_name[32];
  snprintf(acc_name, sizeof(acc_name), "_acc%u", r_axis);
  if (n_lanes > 1) {
    RmuConvMSubstCtx cx = {0};
    for (u32 k = 0; k < n_lanes; k++) {
      for (u32 d = 0; d < emit_depth; d++) fputs("  ", fp);
      fprintf(fp, "%s %s_%u = ", rmu_hold_type(r_dt), acc_name, k);
      rmu_emit_reduce_init(r_kind, r_dt, fp);
      fputs(";\n", fp);
    }
    (void)cx;
  } else {
    for (u32 d = 0; d < emit_depth; d++) fputs("  ", fp);
    fprintf(fp, "%s %s = ", rmu_hold_type(r_dt), acc_name);
    rmu_emit_reduce_init(r_kind, r_dt, fp);
    fputs(";\n", fp);
  }
  // RMU_MAX_RANGES: a fused conv-backward body has more than MAX_DIM RANGEs
  // once UPCAST/LOCAL splits add axes, and a missed axis would skip its loop.
  Term r_ranges[RMU_MAX_RANGES];
  u32  r_kinds[RMU_MAX_RANGES]   = {0};
  u32  r_factors[RMU_MAX_RANGES] = {0};
  u32  n_r_ranges = 0;
  // A reduce axis may appear only inside a nested reduce's body.
  rmu_collect_ranges_with_opts_through_reduce_kernel(
      r_src, r_ranges, r_kinds, r_factors, &n_r_ranges);
  // Each reduce axis's RANGE, axis 0 outermost (REDUCE.src[1:] order).
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
    // Without the RANGE the reduce is empty: the accumulator keeps its init.
    if (axis_ranges[ai] == 0) return;
  }
  u32  reduce_extent     = axis_extents[0];
  // On GPU a short outermost reduce axis unrolls fully.
  if (RMU_TARGET != CG_TARGET_C && reduce_extent > 0
      && reduce_extent <= RMU_REDUCE_UNROLL_MAX) {
    for (u32 d = 0; d < emit_depth; d++) fputs("  ", fp);
    fprintf(fp, "#pragma unroll(%u)\n", reduce_extent);
  }
  for (u32 ai = 0; ai < r_n_axes; ai++) {
    rmu_emit_range_open(axis_ranges[ai], fp, emit_depth + ai, 0);
  }
  u32 inner_depth = emit_depth + r_n_axes;
  // Inner split halves of this reduce's axes (UNROLL/UPCAST RANGEs that are
  // neither named axes nor in store_addr) fold into this accumulator inside the
  // K loop; r_ranges spans nested bodies, so only owned halves fold.
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
    if (!rmu_reduce_folds_split_axis(red, ax)) {
      continue;
    }
    decomp_ranges [n_decomp]   = r_ranges[j];
    decomp_factors[n_decomp]   = r_factors[j];
    n_decomp++;
  }
  inner_depth = rmu_emit_inner_reduce_decomp_loops(
      decomp_ranges, NULL, decomp_factors, n_decomp, fp, inner_depth);
  // Nested reduces that use this reduce's axes emit inside the innermost loop,
  // before the combine that reads their `_acc`; reduces[] is post-order.
  for (u32 k = 0; k < n_reduces; k++) {
    if (parent_idx[k] != self_idx) continue;
    rmu_emit_one_reduce_lb(reduces[k], inner_depth,
                        reduces, parent_idx, k, n_reduces, fp,
                        store_addr, (n_lanes > 1) ? lb : NULL);
  }
  if (n_lanes > 1) {
    // Hoist the lane-invariant subterms of r_src before the N combines; the
    // global hoist map is saved and restored around this reduce.
    u32 saved_hoist_n = RMU_HOIST_N;
    char lane_hoist_pool[RMU_HOIST_MAX * 40] = {0};
    u32 lane_pool_used = 0;
    u32 lane_n_hoist = 0;
    // Axes in scope here: the output axes, this reduce's axes and its inner
    // split halves; a hoisted local may use only these.
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
      rmu_emit_reduce_combine(lane_acc, r_kind, r_dt, r_src_k, fp);
      RMU_ACC_LANE_SUFFIX[0] = '\0';
    }
    // Drop this reduce's hoists: their `_sh` locals end with its loop.
    while (RMU_HOIST_N > saved_hoist_n) {
      RMU_HOIST_N--;
      RMU_HOIST_MAP[RMU_HOIST_N].key  = 0;
      RMU_HOIST_MAP[RMU_HOIST_N].name = NULL;
    }
  } else {
    for (u32 d = 0; d < inner_depth; d++) fputs("  ", fp);
    rmu_emit_reduce_combine(acc_name, r_kind, r_dt, r_src, fp);
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

// Emit one STORE inside loops (or thread binds) over every RANGE of its address
// and value; reduces in the value hoist their accumulators ahead of the store.
static void rmu_emit_store(Term store, FILE *fp, u32 depth) {
  if (term_tag(store) != TAG_UOP || term_ext(store) != UOP_STORE) return;
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
  // Value ranges include LOOP axes inside reduce bodies, so they get loops;
  // the axes each reduce binds are left to rmu_emit_one_reduce_lb.
  rmu_collect_ranges_with_opts_through_reduce_kernel(
      value, ranges, opt_kinds, opt_factors, &n_ranges);
  // tinygrad orders UPCAST axes last: in C a plain store loops its lanes
  // innermost, beside the contiguous loads of the axis they were split from.
  if (RMU_TARGET == CG_TARGET_C && !rmu_term_has_reduce(value, 0)) {
    u32 m = 0;
    for (u32 i = 0; i < n_ranges; i++) {
      if (opt_kinds[i] == UOP_OPT_UPCAST) continue;
      Term r = ranges[i];
      u32  k = opt_kinds[i], f = opt_factors[i];
      for (u32 j = i; j > m; j--) {
        ranges[j]      = ranges[j - 1];
        opt_kinds[j]   = opt_kinds[j - 1];
        opt_factors[j] = opt_factors[j - 1];
      }
      ranges[m] = r; opt_kinds[m] = k; opt_factors[m] = f;
      m++;
    }
  }

  // Output axes are the ranges of `addr`.
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

  // Inner split halves of a K axis (see rmu_range_is_inner_reduce_decomp) fold
  // inside their reduce, so they open no outer loop and set no required_pos.
  int range_is_inner_decomp[RMU_MAX_RANGES] = {0};
  for (u32 i = 0; i < n_ranges; i++) {
    range_is_inner_decomp[i] =
        rmu_range_is_inner_reduce_decomp(ranges[i], opt_kinds[i], addr);
  }

  // Register blocking (C only): UPCAST output axes become lanes sharing every
  // reduce's loops (RmuLaneBlock), not output loops; the store fans out.
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

  // Output axes some reduce depends on.  The others are broadcast axes (the
  // column of a row softmax): they stay serial loops inside the reduce's
  // thread, so the reduce runs once per row instead of once per element.
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
        // Only output axes count.
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

  // GPU: plain LOOP output axes become grid axes (an UPCAST/LOCAL split's
  // outer half among them), but broadcast axes beside a reduce-dependent one
  // stay serial.
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
      // A broadcast axis beside a reduce-dependent one stays serial.
      int is_reduce_dep = 0;
      for (u32 m = 0; m < n_reduce_dep; m++)
        if (reduce_dep_axes[m] == axis_id) { is_reduce_dep = 1; break; }
      if (any_reduce_dep && !is_reduce_dep) {
        continue;
      }
      promote_global[i] = 1;
    }
  }

  RmuGlobalDecode g_decode;
  rmu_compute_global_decode_ctx(ranges, n_ranges, promote_global,
                                opt_kinds, &g_decode);

  Term reduces[RMU_MAX_RANGES];
  u32  n_reduces = 0;
  rmu_collect_reduces(value, reduces, &n_reduces);
  RMU_SPLIT_N = 0;
  for (u32 i = 0; i < n_ranges && RMU_SPLIT_N < RMU_MAX_RANGES; i++) {
    if (!range_is_inner_decomp[i]) {
      continue;
    }
    u32 aid = (u32)term_val(heap_read(term_val(ranges[i]) + 0));
    Term owner = 0;
    for (u32 k = 0; k < n_reduces && owner == 0; k++) {
      if (uop_reduce_owns_split_axis(reduces[k], aid)) {
        owner = reduces[k];
      }
    }
    RMU_SPLIT_AID  [RMU_SPLIT_N] = aid;
    RMU_SPLIT_OWNER[RMU_SPLIT_N] = owner;
    RMU_SPLIT_N++;
  }
  // required_pos[i]: 1 + the position of the deepest output loop reduce i's
  // body uses, so it emits right after that loop opens; 0 hoists it above all
  // output loops.
  u32 required_pos[RMU_MAX_RANGES] = {0};
  for (u32 i = 0; i < n_reduces; i++) {
    Term r_src = heap_read(term_val(reduces[i]) + 0);
    Term r_ranges[RMU_MAX_RANGES];
    u32  r_n = 0;
    // Through nested bodies, at RMU_MAX_RANGES: a nested reduce's axes count
    // too, and a fused conv-backward body exceeds MAX_DIM ranges.
    rmu_collect_ranges_through_reduce_kernel(r_src, r_ranges, &r_n);
    u32 max_pos = 0;
    for (u32 j = 0; j < r_n; j++) {
      if (term_tag(r_ranges[j]) != TAG_UOP
          || term_ext(r_ranges[j]) != UOP_RANGE) continue;
      u32 axis = (u32)term_val(heap_read(term_val(r_ranges[j]) + 0));
      for (u32 k = 0; k < n_ranges; k++) {
        if (term_tag(ranges[k]) != TAG_UOP
            || term_ext(ranges[k]) != UOP_RANGE) continue;
        // Inner split halves and lane axes open no outer loop.
        if (range_is_inner_decomp[k]) continue;
        if (lane_is_axis[k]) continue;
        u32 oaxis = (u32)term_val(heap_read(term_val(ranges[k]) + 0));
        if (oaxis == axis && (k + 1) > max_pos) max_pos = k + 1;
      }
    }
    required_pos[i] = max_pos;
  }
  // An outer reduce reads its nested reduces' accumulators, so it emits no
  // earlier than any of them; reduces[] is post-order, so one pass suffices.
  for (u32 i = 0; i < n_reduces; i++) {
    Term r_src_i = heap_read(term_val(reduces[i]) + 0);
    for (u32 j = 0; j < i; j++) {
      if (rmu_term_contains(r_src_i, reduces[j])
          && required_pos[j] > required_pos[i]) {
        required_pos[i] = required_pos[j];
      }
    }
  }

  // parent_idx[i]: the innermost reduce whose axes reduces[i]'s body uses; it
  // emits inside that parent's loops (rmu_emit_one_reduce_lb), a root
  // (RMU_REDUCE_NO_PARENT) at its required_pos.
  u32 parent_idx[RMU_MAX_RANGES];
  for (u32 i = 0; i < n_reduces; i++) parent_idx[i] = RMU_REDUCE_NO_PARENT;
  for (u32 i = 0; i < n_reduces; i++) {
    Term r_src_i = uop_reduce_src(reduces[i]);
    // A candidate contains reduces[i] and has an axis its body uses.
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
      // Keep the innermost candidate; the enclosing chain is totally ordered.
      if (best == RMU_REDUCE_NO_PARENT) { best = j; continue; }
      Term r_src_best = uop_reduce_src(reduces[best]);
      if (rmu_term_contains(r_src_best, reduces[j])) best = j;
    }
    parent_idx[i] = best;
  }

  // The grid rounds up to whole threadgroups, so threads past `total` return;
  // the `tg` form, exact with LOCAL axes, is kept for symmetry.
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

  // Root reduces that use no output axis emit before every output loop.
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
    if (range_is_inner_decomp[i]) continue;
    if (lane_is_axis[i]) continue;
    u32 axis_id   = (term_tag(r) == TAG_UOP && term_ext(r) == UOP_RANGE)
                  ? (u32)term_val(heap_read(term_val(r) + 0)) : 0xFFFFFFFFu;
    u32 axis_type = (term_tag(r) == TAG_UOP && term_ext(r) == UOP_RANGE)
                  ? (u32)term_val(heap_read(term_val(r) + 1)) : 0;
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
    // Root reduces whose deepest output axis is this one emit here.
    for (u32 r_i = 0; r_i < n_reduces; r_i++) {
      if (required_pos[r_i] != i + 1) continue;
      if (parent_idx[r_i] != RMU_REDUCE_NO_PARENT) continue;
      rmu_emit_one_reduce_lb(reduces[r_i], body_depth,
                          reduces, parent_idx, r_i,
                          n_reduces, fp, addr, lbp);
    }
  }
  if (lbp != NULL) {
    // C, f32 out, unit-stride innermost lane axis: lanes store as float4, which
    // lets clang's SLP vectorizer pack the whole lane chain (tinygrad
    // memory_coalescing, codegen/late/coalesce.py).
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
      // Lanes 4g..4g+3 differ only in the innermost axis, which varies fastest.
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
    // One store per lane, reading its own `_acc..._<k>` accumulators.
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

// Emit an AFTER chain: the earlier side effect first, then a threadgroup
// barrier when either store touches a LOCAL buffer, then `node`.
static void rmu_emit_after(Term after, FILE *fp, u32 depth) {
  if (term_tag(after) != TAG_UOP || term_ext(after) != UOP_AFTER) return;
  u64 loc = term_val(after);
  Term node       = heap_read(loc + 0);
  Term after_node = heap_read(loc + 1);
  if (term_ext(after_node) == UOP_STORE) rmu_emit_store(after_node, fp, depth);
  if (term_ext(after_node) == UOP_AFTER) rmu_emit_after(after_node, fp, depth);
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

// Buffer slots of a kernel: bufs[0] is the output (instance 0), bufs[k] input
// k - 1 (instance k).  This walk is the source of truth for the signature;
// test-built kernels (instance 0 throughout) take cg_render_uop_kernel.
typedef struct {
  Term *bufs;
  u32   cap;
} RmuSlots;

static void rmu_slots_reserve(RmuSlots *s, u32 need) {
  if (need <= s->cap) {
    return;
  }
  u32 cap = s->cap ? s->cap : 16;
  while (cap < need) {
    cap *= 2;
  }
  Term *grown = (Term *)realloc(s->bufs, (size_t)cap * sizeof(Term));
  if (grown == NULL) {
    thvm_fatal("rmu_slots_reserve: out of memory");
  }
  for (u32 i = s->cap; i < cap; i++) {
    grown[i] = 0;
  }
  s->bufs = grown;
  s->cap  = cap;
}

// Place `t` in the first free slot above *n_inputs_out unless a slot
// there already holds it.
static void rmu_slots_promote(RmuSlots *s, Term t, u32 *n_inputs_out) {
  for (u32 i = (*n_inputs_out) + 1; ; i++) {
    rmu_slots_reserve(s, i + 1);
    if (s->bufs[i] == t) {
      return;
    }
    if (s->bufs[i] == 0) {
      s->bufs[i] = t;
      *n_inputs_out = i;
      return;
    }
  }
}

static void rmu_discover_bufs_rec(Term t, RmuSlots *slots, u32 *n_inputs_out) {
  // A bare TAG_TEN leaf is an input too: it takes the next free slot, or the
  // body would name it `buf<loc>`, undeclared.
  if (term_tag(t) == TAG_TEN) {
    rmu_slots_promote(slots, t, n_inputs_out);
    return;
  }
  if (term_tag(t) != TAG_UOP) return;
  u32 op = term_ext(t);
  u64 loc = term_val(t);
  if (op == UOP_BUFFERIZE) {
    // A residual BUFFERIZE is an opaque leaf: its producer does not run here,
    // and walking it would let its buffers steal this kernel's slots.  Its
    // loads render 0.0, as cpu_uop_walk's do (rmu_emit_term).
    return;
  }
  if (op == UOP_BUFFER) {
    u32 inst = uop_buffer_inst_get(t);
    if (inst > KERNEL_MAX_INPUT) return;   // no kernel has such a slot
    rmu_slots_reserve(slots, inst + 1);
    if (slots->bufs[inst] == t) return;
    if (slots->bufs[inst] == 0) {
      slots->bufs[inst] = t;
      if (inst >= 1 && inst > *n_inputs_out) *n_inputs_out = inst;
      return;
    }
    // Another Term holds this slot (several unstamped instance-0 BUFFERs, say):
    // promote it to the next free input slot instead of dropping it.
    rmu_slots_promote(slots, t, n_inputs_out);
    return;
  }
  for (u32 i = 0, n = rmu_walk_arity_kernel(op); i < n; i++) {
    rmu_discover_bufs_rec(heap_read(loc + i), slots, n_inputs_out);
  }
}

// Discover `root`'s buffer slots into a table that covers at least
// slots 0..*n_inputs_out.  Release it with rmu_slots_free.
static RmuSlots rmu_slots_open(Term root, u32 *n_inputs_out) {
  RmuSlots slots = { NULL, 0 };
  *n_inputs_out = 0;
  rmu_discover_bufs_rec(root, &slots, n_inputs_out);
  rmu_slots_reserve(&slots, *n_inputs_out + 1);
  return slots;
}

static void rmu_slots_free(RmuSlots *slots) {
  free(slots->bufs);
  slots->bufs = NULL;
  slots->cap  = 0;
}

// The number of inputs the rendered signature declares, from the same discover
// walk; materialize.c reconciles ke->n_inputs with it.  As tinygrad's
// args_from_ast reads buffers off the AST (codegen/opt/postrange.py:256-258).
fn u32 cg_render_input_inst_count(Term root) {
  u32      n_inputs = 0;
  RmuSlots slots    = rmu_slots_open(root, &n_inputs);
  rmu_slots_free(&slots);
  return n_inputs;
}

// Metal kernel for `root` (a STORE or an AFTER chain) with explicit buffers:
// out_buf is "out" and in_bufs[i] "in<i>", for test-built kernels whose
// BUFFERs carry no instance.
fn void cg_render_uop_kernel(Term root, const char *kernel_name,
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
  fputs("#include <metal_stdlib>\n", fp);
  fputs("using namespace metal;\n\n", fp);
  // fp8 decode helpers if any input/output is fp8 (rmu_emit_fp8_helpers).
  {
    int has_fp8 = (rmu_slot_dtype(out_buf) == DT_FP8E4M3
                   || rmu_slot_dtype(out_buf) == DT_FP8E5M2);
    for (u32 i = 0; i < n_inputs && !has_fp8; i++) {
      u32 dt = rmu_slot_dtype(in_bufs[i]);
      if (dt == DT_FP8E4M3 || dt == DT_FP8E5M2) has_fp8 = 1;
    }
    if (has_fp8) rmu_emit_fp8_helpers(fp);
  }
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

// Metal kernel for a lifted `root`, its signature from the discover walk
// (output in slot 0, input k in slot k + 1); cg_emit_via_uop renders with it.
fn void cg_render_uop_kernel_root(Term root, const char *kernel_name,
                                  FILE *fp) {
  if (fp == NULL) fp = stderr;
  if (kernel_name == NULL) kernel_name = "uop_kernel";
  u32      n_inputs  = 0;
  RmuSlots slots     = rmu_slots_open(root, &n_inputs);
  Term    *slot_bufs = slots.bufs;
  Term     out_buf   = slot_bufs[0];
  rmu_buf_names_reset();
  // The output (instance 0) resolves through the name map.
  if (out_buf != 0) rmu_buf_names_set(out_buf, "out");
  // Promoted BUFFERs sit in a slot their instance does not name.
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
  // fp8 decode helpers if any input/output is fp8 (rmu_emit_fp8_helpers).
  if (rmu_kernel_has_fp8_input(slot_bufs, n_inputs)) rmu_emit_fp8_helpers(fp);
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
  // Symbolic dims: `constant uint &V_<name>` args after the inputs, in
  // kvar_collect_from_dag order, as metal_tile_jit_encode_on binds them; that
  // encoder puts a conv kernel's cfg first, which this signature omits.
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
  rmu_slots_free(&slots);
}

// Declare `unsigned V_<name> = kvar_vals[i]` per symbolic dim, in the
// kvar_collect_from_dag order of cpu/jit.c's cpu_jit_kvar_vals.
static void cg_emit_cpu_kvar_decls(Term root, FILE *fp) {
  u32 used_vars[KVAR_USED_CAP];
  u32 n_vars = kvar_collect_from_dag(root, used_vars, KVAR_USED_CAP);
  for (u32 i = 0; i < n_vars; i++) {
    const char *vn = kvar_name(used_vars[i]);
    fprintf(fp, "  unsigned V_%s = kvar_vals[%u];\n", vn ? vn : "V", i);
  }
  fputs("  (void)kvar_vals;\n", fp);
}

// C99 kernel for the CPU JIT with explicit buffers (test-built kernels), in
// the signature cpu/jit.c dlsyms: `void k(void *out_v, const void *const
// *ins_v, unsigned n, const unsigned *in_numels, const unsigned *kvar_vals)`.
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
  // A statement expression (GCC/clang) keeps THVM_BITCAST an expression.
  fputs("#define THVM_BITCAST(t, x) "
        "({ t _t; __typeof__(x) _x = (x); "
        "memcpy(&_t, &_x, sizeof(_t)); _t; })\n", fp);
  // float4 for the lane-block stores; aligned(4) makes them unaligned vector
  // stores, valid at any base offset.
  fputs("typedef float float4 __attribute__((vector_size(16),"
        "aligned(4)));\n", fp);
  // Guarded: several kernels can share one source file.
  fputs("#ifndef THVM_XEXP2F\n#define THVM_XEXP2F\n", fp);
  fputs(THVM_XEXP2F_SRC, fp);
  fputs("\n#endif\n", fp);
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
  RMU_C_ROOT  = root;
  RMU_C_LANES = rmu_dag_upcast_lanes(root);
  // Late fast_idiv (tinygrad codegen/__init__.py:360, off upstream by
  // DISABLE_FAST_IDIV, helpers.py:249): constant div/mod of bounded indices
  // become a vectorizable multiply-shift.  C only: Metal miscompiles it.
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
  RMU_C_ROOT  = 0;
  RMU_C_LANES = 1;
  fputs("}\n", fp);
}

static void rmu_c_decline(char const *why, u32 slot) {
  static int known = 0, on = 0;
  if (!known) {
    char const *e = getenv("THVM_TRACE_RENDER_DECLINE");
    on = (e != NULL && e[0] == '1');
    known = 1;
  }
  if (on) {
    fprintf(stderr, "render_c decline: %s (slot %u)\n", why, slot);
  }
}

static int rmu_ident_char(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
      || (c >= '0' && c <= '9') || c == '_';
}

// 1 iff C source `src` names input `in<k>` as a whole identifier.
static int rmu_src_names_input(char const *src, u32 k) {
  char name[16];
  int  len = snprintf(name, sizeof name, "in%u", k);
  for (char const *p = strstr(src, name); p != NULL; p = strstr(p + 1, name)) {
    if ((p == src || !rmu_ident_char(p[-1])) && !rmu_ident_char(p[len])) {
      return 1;
    }
  }
  return 0;
}

// 1 iff `src` names an input slot (1..n_inputs) that no UOP_BUFFER claimed.
static int rmu_src_names_hole(char const *src, Term const *slots, u32 n_inputs) {
  for (u32 i = 1; i <= n_inputs; i++) {
    if (slots[i] == 0 && rmu_src_names_input(src, i - 1)) {
      return 1;
    }
  }
  return 0;
}

// C99 kernel for the CPU JIT from a lifted `root`, slots from the discover
// walk.  Writes nothing (cpu/jit.c then interprets) when a slot is untyped or
// the body reads an input no BUFFER claimed.
fn void cg_render_uop_kernel_c_root(Term root, const char *kernel_name,
                                    FILE *fp) {
  if (fp == NULL) fp = stderr;
  if (kernel_name == NULL) kernel_name = "uop_kernel";
  u32      n_inputs  = 0;
  RmuSlots slots     = rmu_slots_open(root, &n_inputs);
  Term    *slot_bufs = slots.bufs;
  Term     out_buf   = slot_bufs[0];
  // Decline, writing nothing so cpu_jit_render_canon falls back to the
  // interpreter, on a slot that holds no UOP_BUFFER (a bare TAG_TEN leaf): it
  // has no dtype to declare.  A BUFFER's dtype 0 is DT_BOOL, an unsigned char.
  // A hole (an unread input) gets no declaration.
  for (u32 i = 1; i <= n_inputs; i++) {
    Term b = slot_bufs[i];
    if (b != 0 && !(term_tag(b) == TAG_UOP && term_ext(b) == UOP_BUFFER)) {
      rmu_c_decline("slot is not a typed buffer", i);
      rmu_slots_free(&slots);
      return;
    }
  }
  char  *src    = NULL;
  size_t src_sz = 0;
  FILE  *out_fp = fp;
  fp = open_memstream(&src, &src_sz);
  if (fp == NULL) {
    rmu_slots_free(&slots);
    return;
  }
  rmu_buf_names_reset();
  if (out_buf != 0) rmu_buf_names_set(out_buf, "out");
  fputs("#include <stdint.h>\n", fp);
  fputs("#include <math.h>\n", fp);
  fputs("#include <string.h>\n", fp);
  fputs("typedef unsigned int uint;\n", fp);
  fputs("#define THVM_BITCAST(t, x) "
        "({ t _t; __typeof__(x) _x = (x); "
        "memcpy(&_t, &_x, sizeof(_t)); _t; })\n", fp);
  // float4 for the lane-block stores; aligned(4) makes them unaligned vector
  // stores, valid at any base offset.
  fputs("typedef float float4 __attribute__((vector_size(16),"
        "aligned(4)));\n", fp);
  // Guarded: several kernels can share one source file.
  fputs("#ifndef THVM_XEXP2F\n#define THVM_XEXP2F\n", fp);
  fputs(THVM_XEXP2F_SRC, fp);
  fputs("\n#endif\n", fp);
  fprintf(fp, "void %s(void *out_v, const void *const *ins_v,\n", kernel_name);
  fputs("              unsigned n, const unsigned *in_numels,\n", fp);
  fputs("              const unsigned *kvar_vals) {\n", fp);
  fputs("  (void)n; (void)in_numels;\n", fp);
  u32 out_dtype = uop_buffer_dtype(out_buf);
  fprintf(fp, "  %s *out = (%s *)out_v;\n",
          rmu_c_type_name(out_dtype), rmu_c_type_name(out_dtype));
  for (u32 i = 0; i < n_inputs; i++) {
    Term in_buf = slot_bufs[i + 1];
    if (in_buf == 0) {
      continue;
    }
    u32 dt = uop_buffer_dtype(in_buf);
    fprintf(fp, "  const %s *in%u = (const %s *)ins_v[%u];\n",
            rmu_c_type_name(dt), i, rmu_c_type_name(dt), i);
  }
  cg_emit_cpu_kvar_decls(root, fp);
  RMU_TARGET = CG_TARGET_C;
  RMU_C_ROOT  = root;
  RMU_C_LANES = rmu_dag_upcast_lanes(root);
  // Late fast_idiv (tinygrad codegen/__init__.py:360, off upstream by
  // DISABLE_FAST_IDIV, helpers.py:249): constant div/mod of bounded indices
  // become a vectorizable multiply-shift.  C only: Metal miscompiles it.
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
  RMU_C_ROOT  = 0;
  RMU_C_LANES = 1;
  fputs("}\n", fp);
  fclose(fp);
  if (rmu_src_names_hole(src, slot_bufs, n_inputs)) {
    rmu_c_decline("body reads an unclaimed input slot", 0);
  } else {
    fputs(src, out_fp);
  }
  free(src);
  rmu_slots_free(&slots);
}

// 1 if a RANGE in `t` is KAX_GROUP_REDUCE, for RMU_HAS_GROUP_REDUCE: the grid
// axes decode from `tg`, as cuda_dag_dispatch_shape launches one block per
// grid-axis tuple.
static int rmu_dag_has_group_reduce(Term t) {
  if (term_tag(t) != TAG_UOP) return 0;
  u32 op  = term_ext(t);
  u64 loc = term_val(t);
  if (op == UOP_RANGE) {
    return (u32)term_val(heap_read(loc + 1)) == KAX_GROUP_REDUCE;
  }
  for (u32 i = 0, n = rmu_walk_arity_kernel(op); i < n; i++) {
    if (rmu_dag_has_group_reduce(heap_read(loc + i))) return 1;
  }
  return 0;
}

// 1 if `t` holds an OPT(TC); the CUDA entry includes <mma.h> only then.
static int rmu_dag_has_tc(Term t) {
  if (term_tag(t) != TAG_UOP) return 0;
  u32 op  = term_ext(t);
  u64 loc = term_val(t);
  if (op == UOP_OPT && (u32)term_val(heap_read(loc + 1)) == UOP_OPT_TC) {
    return 1;
  }
  for (u32 i = 0, n = rmu_walk_arity_kernel(op); i < n; i++) {
    if (rmu_dag_has_tc(heap_read(loc + i))) return 1;
  }
  return 0;
}

// CUDA kernel for a lifted `root`: an `extern "C" __global__` function over
// plain pointers, slots from the discover walk, with tid / tg / tt / sgi
// computed from blockIdx, blockDim and threadIdx in a prologue.
fn void cg_render_uop_kernel_cuda_root(Term root, const char *kernel_name,
                                       FILE *fp) {
  if (fp == NULL) fp = stderr;
  if (kernel_name == NULL) kernel_name = "uop_kernel";
  // Stamp a TC matmul's M and N KAX_GLOBAL (K, M, N multiples of 8), as
  // render_metal.c does; cuda_tc_tile_dispatch_shape repeats this to size the
  // launch.
  root = uop_recognise_tc_parallel(root);
  u32      n_inputs  = 0;
  RmuSlots slots     = rmu_slots_open(root, &n_inputs);
  Term    *slot_bufs = slots.bufs;
  Term     out_buf   = slot_bufs[0];
  rmu_buf_names_reset();
  if (out_buf != 0) rmu_buf_names_set(out_buf, "out");
  RMU_TARGET = CG_TARGET_CUDA;
#ifdef THVM_HAS_CUDA
  RMU_CUDA_SM = cuda_device_sm();
#else
  RMU_CUDA_SM = 0;
#endif
  RMU_HAS_GROUP_REDUCE = rmu_dag_has_group_reduce(root);
  rmu_group_local_dims(root, &RMU_GROUP_EXTENT, &RMU_GROUP_LOCAL_TOTAL);
  // Elementwise kernels use the half/bf16 types too.
  fputs("#include <cuda_fp16.h>\n", fp);
  fputs("#include <cuda_bf16.h>\n", fp);
  if (rmu_dag_has_tc(root)) {
    fputs("#include <mma.h>\n", fp);
    fputs("using namespace nvcuda;\n", fp);
  }
  fputs("typedef unsigned int uint;\n", fp);
  // fp8 decode helpers if any input/output is fp8 (rmu_emit_fp8_helpers).
  if (rmu_kernel_has_fp8_input(slot_bufs, n_inputs)) rmu_emit_fp8_helpers(fp);
  // nvrtc predefines no <math.h> macros, and the MAX init uses -INFINITY.
  fputs("#ifndef INFINITY\n"
        "#define INFINITY __int_as_float(0x7f800000)\n"
        "#endif\n\n", fp);
  // __launch_bounds__ from the LOCAL extents, the block cuda_dag_dispatch_shape
  // launches, so nvcc sizes registers for it; none for a GROUP_REDUCE or a flat
  // launch.
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
    // A tiled WMMA matmul's block is local_m * local_n * 32 threads; declare it
    // only where the tiled emit fires (M, N GLOBAL, untransposed A, 16-bit
    // operands, a tile the picker takes).
    if (RMU_TARGET == CG_TARGET_CUDA && block_size == 1) {
      u32 n_glob = 0;
      for (u32 i = 0; i < lb_n; i++) if (lb_types[i] == KAX_GLOBAL) n_glob++;
      if (n_glob >= 2 && term_tag(root) == TAG_UOP
          && term_ext(root) == UOP_STORE) {
        Term v = heap_read(term_val(root) + 2);
        if (term_tag(v) == TAG_UOP && term_ext(v) == UOP_OPT
            && uop_opt_kind(v) == UOP_OPT_TC) {
          // The shape probes decline a TC value, so probe an unwrapped STORE.
          Term bare = uop_store(heap_read(term_val(root) + 0),
                                heap_read(term_val(root) + 1),
                                uop_opt_target(v));
          u32 mE = 0, nE = 0, kE = 0, ua = 0;
          u32 dtc = uop_buffer_dtype(heap_read(term_val(root) + 0));
          int cbf = (dtc == DT_BF16 || dtc == DT_FP16);
          // Mirror the tiled emit's operand gate (rmu_emit_matmul_tc): an f32
          // TC matmul renders scalar and launches flat 256-thread blocks, which
          // a smaller __launch_bounds__ would make fail to launch.
          Term ab_a = 0, ab_b = 0;
          u32  ab_k = 0, ab_unit = 0;
          int  ab16 = 0;
          if (udg_classify_matmul_store(bare, &ab_k, &ab_a, &ab_b, &ab_unit)
              && ab_a != 0 && ab_b != 0) {
            u32 dt_a = uop_buffer_dtype(heap_read(term_val(ab_a) + 0));
            u32 dt_b = uop_buffer_dtype(heap_read(term_val(ab_b) + 0));
            int a16 = (dt_a == DT_FP16)
                   || (dt_a == DT_BF16 && RMU_CUDA_SM >= 80);
            int b16 = (dt_b == DT_FP16)
                   || (dt_b == DT_BF16 && RMU_CUDA_SM >= 80);
            ab16 = a16 && b16;
          }
          if (ab16
              && uop_matmul_mn_axes(bare, NULL, &mE, NULL, &nE)
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
  // Symbolic dims as plain `unsigned` args, in the Metal entry's order.
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
  // The body's thread indices, from the CUDA launch builtins.
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
  rmu_slots_free(&slots);
}
