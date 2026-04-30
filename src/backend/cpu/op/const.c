// backend/cpu/op/const.c - materialize a scalar CONST into a buffer.
//
// Writes a single element at out[0].  The CpuBuf for the output
// was allocated with numel=1 by materialize_expr, so downstream
// elementwise ops broadcast it via the n_elems check in the
// per-op loops.
//
// Phase A: 32-bit constants only (the KProgOp arg field is u32).
// 64-bit dtypes (i64/u64/f64) need either a widened arg or a
// two-cell payload; deferred to Phase C.  For now i64/u64 store
// the low 32 bits sign-extended (matching the WL surface that
// caps user-passed scalar constants at i32 range).

fn void cpu_op_const(void *out, void **srcs, u32 const *src_numels,
                     KProgOp const *p, u32 out_numel) {
  (void)srcs; (void)src_numels; (void)out_numel;
  switch (p->dtype) {
    case DT_FP32: {
      f32 v;
      u32 bits = p->arg;
      memcpy(&v, &bits, sizeof(v));
      ((f32 *)out)[0] = v;
      break;
    }
    case DT_BOOL:   ((u8  *)out)[0] = (u8 )(p->arg & 1);                     break;
    case DT_INT8:   ((i8  *)out)[0] = (i8 )(int32_t)p->arg;                   break;
    case DT_UINT8:  ((u8  *)out)[0] = (u8 )p->arg;                            break;
    case DT_INT16:  ((i16 *)out)[0] = (i16)(int32_t)p->arg;                   break;
    case DT_UINT16: ((u16 *)out)[0] = (u16)p->arg;                            break;
    case DT_INT32:  ((i32 *)out)[0] = (i32)p->arg;                            break;
    case DT_UINT32: ((u32 *)out)[0] = (u32)p->arg;                            break;
    case DT_INT64:  ((i64 *)out)[0] = (i64)(int32_t)p->arg;                   break;
    case DT_UINT64: ((u64 *)out)[0] = (u64)p->arg;                            break;
    case DT_INT4:   {
      i8 v8 = (i8)((i32)p->arg);
      pack_int4((u8 *)out, &v8, 1);
      break;
    }
    case DT_UINT4:  {
      u8 v8 = (u8)(p->arg & 0xFu);
      pack_uint4((u8 *)out, &v8, 1);
      break;
    }
    case DT_FP16:
    case DT_BF16:
    case DT_FP64:
    case DT_FP8E4M3:
    case DT_FP8E5M2: {
      // Interpret the u32 arg as f32 bits, then convert to the
      // narrow / wide float.  Phase C: 64-bit constants exact only
      // up to f32 precision; the WL bridge clamps user-passed
      // scalars to that range until the two-cell payload lands.
      f32 v;
      u32 bits = p->arg;
      memcpy(&v, &bits, sizeof(v));
      from_fp32_lane(out, p->dtype, &v, 1);
      break;
    }
    default:
      fprintf(stderr, "cpu_op_const: dtype %u not supported\n", p->dtype);
      abort();
  }
}
