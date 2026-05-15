// schedule/tile.c -- tile-plan arena above scalar-UOps.
//
// Rangeify owns semantic lowering to ScalarUop; tile.c owns the
// schedule/memory plan that wraps those scalar bodies with explicit
// axes, local memory, barriers, reductions, and future MMA nodes.

fn void tile_reserve(KernelEntry *ke, u32 needed) {
  if (needed <= ke->tile_uops_cap) {
    return;
  }
  if (needed > TILE_MAX_CAP) {
    fprintf(stderr, "tile_reserve: needed=%u exceeds cap %u\n",
            needed, TILE_MAX_CAP);
    exit(1);
  }
  u32 new_cap = ke->tile_uops_cap == 0 ? TILE_INIT_CAP : ke->tile_uops_cap * 2;
  while (new_cap < needed) {
    new_cap *= 2;
  }
  ke->tile_uops = (TileUop *)realloc(ke->tile_uops,
                                     (size_t)new_cap * sizeof(TileUop));
  if (ke->tile_uops_cap == 0) {
    memset(&ke->tile_uops[0], 0, sizeof(TileUop));
    ke->n_tile_uops = 1;
  }
  for (u32 i = ke->tile_uops_cap == 0 ? 1 : ke->tile_uops_cap;
       i < new_cap; i++) {
    memset(&ke->tile_uops[i], 0, sizeof(TileUop));
  }
  ke->tile_uops_cap = new_cap;
}

fn u32 tile_emit(KernelEntry *ke, u8 op, u32 dtype,
                 u8 src_count, const u32 *src, u64 extra) {
  if (op == TILE_NONE || op >= TILE__COUNT) {
    fprintf(stderr, "tile_emit: bad op=%u\n", op);
    exit(1);
  }
  if (src_count > TILE_MAX_SRC) {
    fprintf(stderr, "tile_emit: src_count=%u exceeds max %u\n",
            src_count, TILE_MAX_SRC);
    exit(1);
  }
  tile_reserve(ke, ke->n_tile_uops + 1);
  u32 id       = ke->n_tile_uops++;
  TileUop *u   = &ke->tile_uops[id];
  u->op        = op;
  u->src_count = src_count;
  u->dtype     = dtype;
  u->extra     = extra;
  for (u8 i = 0; i < TILE_MAX_SRC; i++) {
    u->src[i] = (i < src_count && src != NULL) ? src[i] : 0;
  }
  return id;
}

fn u32 tile_emit_leaf(KernelEntry *ke, u8 op, u32 dtype, u64 extra) {
  return tile_emit(ke, op, dtype, 0, NULL, extra);
}

// Phase D2: TILE_LOCAL_ALLOC -- threadgroup/local memory allocation.
// extra = (scope << 32) | n_elements; dtype carries the element type.
// D3's reduce-broadcast lowering builds these for the cooperative-
// reduce accumulator buffer.  Render emits Metal `threadgroup` /
// `thread` storage qualifiers based on `scope`.
fn u32 tile_emit_alloc(KernelEntry *ke, u32 dtype, u32 scope, u32 n_elements) {
  u64 extra = ((u64)scope << 32) | (u64)n_elements;
  return tile_emit_leaf(ke, TILE_LOCAL_ALLOC, dtype, extra);
}

// Phase D2: TILE_BARRIER -- threadgroup synchronization point.
// extra = scope (TILE_MEM_*).  D3 emits one before any cross-thread
// read of a shared accumulator.  Render emits Metal
// `threadgroup_barrier(mem_flags::mem_threadgroup)` (or simdgroup
// flavor if scope indicates so).
fn u32 tile_emit_barrier(KernelEntry *ke, u32 scope) {
  return tile_emit_leaf(ke, TILE_BARRIER, DT_BOOL, (u64)scope);
}

// Phase D2: TILE_LOAD -- read N consecutive elements from a
// TILE_LOCAL_ALLOC (or other addressable scope) at `addr`.
// src[0] = alloc_id (the TILE_LOCAL_ALLOC node), src[1] = addr
// expression (typically a TILE_SCALAR_BODY wrapping a ScalarUop
// integer expression).  The dtype matches the alloc's element type.
// extra is reserved for future vectorisation hints (D3 ignores it).
fn u32 tile_emit_load(KernelEntry *ke, u32 dtype, u32 alloc_id, u32 addr_id) {
  u32 src[2] = { alloc_id, addr_id };
  return tile_emit(ke, TILE_LOAD, dtype, 2, src, 0);
}

// Phase D3: TILE_BLOCK -- ordered statement sequence.  src[0..n-1]
// are executed in order; the block's value is the LAST entry.  The
// canonical reduce-broadcast pattern emits:
//   TILE_BLOCK(alloc, reduce_into_alloc, barrier, load, body)
// `dtype` must match the last entry's dtype (the block's "value").
fn u32 tile_emit_block(KernelEntry *ke, u32 dtype,
                       u32 const *stmts, u8 n_stmts) {
  return tile_emit(ke, TILE_BLOCK, dtype, n_stmts, stmts, 0);
}

// Phase F: TILE_INPUT_BUF -- kernel input buffer reference.
// extra = input slot id (the index into ke->input_tids[] etc.).
// Used as src[0] of TILE_LOAD for loads from global memory.
fn u32 tile_emit_input_buf(KernelEntry *ke, u32 dtype, u32 input_slot) {
  return tile_emit_leaf(ke, TILE_INPUT_BUF, dtype, (u64)input_slot);
}

// Phase F: TILE_OUTPUT_BUF -- kernel output buffer reference.
// extra = output slot id (0 = primary; 1..n = extras for
// multi-output kernels).  Future TILE_STORE shapes that need
// explicit output binding (e.g. multi-output BN-grad fused
// kernels) reference this leaf.
fn u32 tile_emit_output_buf(KernelEntry *ke, u32 dtype, u32 output_slot) {
  return tile_emit_leaf(ke, TILE_OUTPUT_BUF, dtype, (u64)output_slot);
}

// Read TILE_LOCAL_ALLOC's (scope, n_elements) -- mirrors
// tile_axis_unpack but for the alloc's two-field packing.
typedef struct {
  u32 scope;
  u32 n_elements;
} TileAllocInfo;

static inline TileAllocInfo tile_alloc_unpack(u64 extra) {
  TileAllocInfo info;
  info.n_elements = (u32)(extra & 0xFFFFFFFFu);
  info.scope      = (u32)((extra >> 32) & 0xFFu);
  return info;
}

fn void tile_free(KernelEntry *ke) {
  if (ke->tile_uops != NULL) {
    free(ke->tile_uops);
  }
  ke->tile_uops     = NULL;
  ke->n_tile_uops   = 0;
  ke->tile_uops_cap = 0;
  ke->tile_root     = 0;
  ke->tile_axes_hash    = 0;
}

// Phase F prep: tile-IR pretty-printer.  `tile_dump(ke, fp)` walks
// the tile_uops graph from ke->tile_root and prints a nested
// representation with indentation.  Axis nodes show kax_type +
// extent + memory_scope + vector_width; alloc nodes show scope +
// n_elements; barriers show scope; scalar bodies show the
// referenced ScalarUop slot id.  Used by `DUMP_TILE_IR=1` env-gate
// in materialize.c (wired below) and as the foundation Phase F's
// Metal renderer walks.

static void tile_dump_indent(FILE *fp, u32 depth) {
  for (u32 i = 0; i < depth; i++) fputs("  ", fp);
}

fn void tile_dump_node(KernelEntry const *ke, u32 id, FILE *fp, u32 depth);

fn void tile_dump_node(KernelEntry const *ke, u32 id, FILE *fp, u32 depth) {
  if (ke == NULL || ke->tile_uops == NULL || id == 0
      || id >= ke->n_tile_uops || depth > 32) {
    tile_dump_indent(fp, depth);
    fprintf(fp, "TILE_? id=%u\n", id);
    return;
  }
  TileUop const *u = &ke->tile_uops[id];
  tile_dump_indent(fp, depth);
  switch (u->op) {
    case TILE_AXIS: {
      TileAxisInfo info = tile_axis_unpack(u->extra);
      fprintf(fp, "TILE_AXIS<%s ext=%u", tile_axis_name(info.kax_type),
              info.extent);
      if (info.memory_scope != 0) fprintf(fp, " scope=%u", info.memory_scope);
      if (info.vector_width != 0) fprintf(fp, " vw=%u", info.vector_width);
      fprintf(fp, ">\n");
      return;
    }
    case TILE_LOCAL_ALLOC: {
      TileAllocInfo info = tile_alloc_unpack(u->extra);
      fprintf(fp, "TILE_LOCAL_ALLOC<scope=%u n=%u dtype=%u>\n",
              info.scope, info.n_elements, u->dtype);
      return;
    }
    case TILE_BARRIER:
      fprintf(fp, "TILE_BARRIER<scope=%u>\n", (u32)u->extra);
      return;
    case TILE_SCALAR_BODY:
      fprintf(fp, "TILE_SCALAR_BODY S%u dtype=%u\n", (u32)u->extra, u->dtype);
      return;
    case TILE_REDUCE:
      fprintf(fp, "TILE_REDUCE S%u dtype=%u\n", (u32)u->extra, u->dtype);
      for (u8 s = 0; s < u->src_count; s++) {
        tile_dump_node(ke, u->src[s], fp, depth + 1);
      }
      return;
    case TILE_LOAD:
      fprintf(fp, "TILE_LOAD dtype=%u\n", u->dtype);
      for (u8 s = 0; s < u->src_count; s++) {
        tile_dump_node(ke, u->src[s], fp, depth + 1);
      }
      return;
    case TILE_STORE:
      fprintf(fp, "TILE_STORE S%u dtype=%u\n", (u32)u->extra, u->dtype);
      for (u8 s = 0; s < u->src_count; s++) {
        tile_dump_node(ke, u->src[s], fp, depth + 1);
      }
      return;
    case TILE_BLOCK:
      fprintf(fp, "TILE_BLOCK n_stmts=%u\n", u->src_count);
      for (u8 s = 0; s < u->src_count; s++) {
        tile_dump_node(ke, u->src[s], fp, depth + 1);
      }
      return;
    case TILE_LOOP_NEST:
      fprintf(fp, "TILE_LOOP_NEST dtype=%u\n", u->dtype);
      for (u8 s = 0; s < u->src_count; s++) {
        tile_dump_node(ke, u->src[s], fp, depth + 1);
      }
      return;
    case TILE_INPUT_BUF:
      fprintf(fp, "TILE_INPUT_BUF<slot=%u dtype=%u>\n",
              (u32)u->extra, u->dtype);
      return;
    case TILE_OUTPUT_BUF:
      fprintf(fp, "TILE_OUTPUT_BUF<slot=%u dtype=%u>\n",
              (u32)u->extra, u->dtype);
      return;
    default:
      fprintf(fp, "TILE_? op=%u src_count=%u\n", u->op, u->src_count);
      return;
  }
}

fn void tile_dump(KernelEntry const *ke, FILE *fp) {
  if (fp == NULL) fp = stderr;
  if (ke == NULL || ke->tile_uops == NULL) {
    fprintf(fp, "tile_dump: <empty>\n");
    return;
  }
  fprintf(fp, "tile_dump: n_tile_uops=%u root=%u\n",
          ke->n_tile_uops, ke->tile_root);
  tile_dump_node(ke, ke->tile_root, fp, 1);
}

// Phase F prep: emit MSL skeleton from tile_uops.  Produces a
// pseudo-MSL kernel with axis loops, alloc declarations, barriers,
// and store statements -- enough structure to verify the IR carries
// what a real renderer needs.  Doesn't substitute scalar bodies;
// emits placeholder `/* scalar S<id> */` for those.  Future Phase F
// work expands the placeholders into actual MSL via the existing
// rmt_emit_value helpers.

static void tile_render_msl_node(KernelEntry const *ke, u32 id, FILE *fp,
                                 u32 depth);

static void tile_render_msl_indent(FILE *fp, u32 depth) {
  for (u32 i = 0; i < depth; i++) fputs("  ", fp);
}

static void tile_render_msl_axis(KernelEntry const *ke, u32 id, FILE *fp,
                                 u32 depth, u32 axis_idx) {
  TileUop const *u = &ke->tile_uops[id];
  TileAxisInfo info = tile_axis_unpack(u->extra);
  tile_render_msl_indent(fp, depth);
  switch (info.kax_type) {
    case KAX_LOOP:
      fprintf(fp, "for (uint a%u = 0; a%u < %u; a%u++) {\n",
              axis_idx, axis_idx, info.extent, axis_idx);
      break;
    case KAX_REDUCE:
      fprintf(fp, "for (uint a%u = 0; a%u < %u; a%u++) /*reduce*/ {\n",
              axis_idx, axis_idx, info.extent, axis_idx);
      break;
    case KAX_GLOBAL:
      fprintf(fp, "uint a%u = threadgroup_position_in_grid; /*global*/\n",
              axis_idx);
      break;
    case KAX_LOCAL:
      fprintf(fp, "uint a%u = thread_position_in_threadgroup; /*local*/\n",
              axis_idx);
      break;
    case KAX_GROUP_REDUCE:
      fprintf(fp, "uint a%u = thread_position_in_threadgroup; /*group_reduce ext=%u*/\n",
              axis_idx, info.extent);
      break;
    default:
      fprintf(fp, "uint a%u = 0; /*kax=%u ext=%u*/\n",
              axis_idx, info.kax_type, info.extent);
      break;
  }
}

static void tile_render_msl_node(KernelEntry const *ke, u32 id, FILE *fp,
                                 u32 depth) {
  if (id == 0 || id >= ke->n_tile_uops) return;
  TileUop const *u = &ke->tile_uops[id];
  switch (u->op) {
    case TILE_LOOP_NEST: {
      // src[0] = body, src[1..] = axes.
      for (u8 s = 1; s < u->src_count; s++) {
        tile_render_msl_axis(ke, u->src[s], fp, depth, s - 1);
      }
      // Emit the row-major output index from the output axes.  Walks
      // axes left-to-right; each output axis (LOOP/UPCAST/LOCAL/GLOBAL)
      // contributes `a<i> * stride` where stride is the product of
      // later output axes' extents.  Reduce/Unroll axes are skipped
      // (they're in-kernel reductions that don't index output).
      u32 out_axis_idxs[MAX_AXES];
      u32 out_axis_extents[MAX_AXES];
      u32 n_out = 0;
      for (u8 s = 1; s < u->src_count; s++) {
        TileUop const *axis = &ke->tile_uops[u->src[s]];
        TileAxisInfo info = tile_axis_unpack(axis->extra);
        if (info.kax_type == KAX_LOOP || info.kax_type == KAX_UPCAST
            || info.kax_type == KAX_LOCAL || info.kax_type == KAX_GLOBAL) {
          out_axis_idxs[n_out]    = (u32)(s - 1);
          out_axis_extents[n_out] = info.extent;
          n_out++;
        }
      }
      if (n_out > 0) {
        tile_render_msl_indent(fp, depth + 1);
        fputs("uint _idx = ", fp);
        for (u32 i = 0; i < n_out; i++) {
          if (i > 0) fputs(" + ", fp);
          u32 stride = 1;
          for (u32 j = i + 1; j < n_out; j++) stride *= out_axis_extents[j];
          if (stride == 1) {
            fprintf(fp, "a%u", out_axis_idxs[i]);
          } else {
            fprintf(fp, "a%u * %u", out_axis_idxs[i], stride);
          }
        }
        fputs(";\n", fp);
      }
      tile_render_msl_node(ke, u->src[0], fp, depth + 1);
      // Close LOOP/REDUCE braces.
      for (u8 s = 1; s < u->src_count; s++) {
        TileUop const *axis = &ke->tile_uops[u->src[s]];
        TileAxisInfo info = tile_axis_unpack(axis->extra);
        if (info.kax_type == KAX_LOOP || info.kax_type == KAX_REDUCE) {
          tile_render_msl_indent(fp, depth);
          fputs("}\n", fp);
        }
      }
      return;
    }
    case TILE_STORE: {
      // Render the value-producing src first (so any inner emits land
      // before the store statement).
      for (u8 s = 0; s < u->src_count; s++) {
        tile_render_msl_node(ke, u->src[s], fp, depth);
      }
      tile_render_msl_indent(fp, depth);
      // The kernel's primary output goes to buffer(0) as `out`.  The
      // address is computed elsewhere (LOOP_NEST iters); for now use
      // a flat tid as the index, mirroring the FLAT_GRID dispatch
      // shape.  Phase F's renderer proper will emit a fully indexed
      // store using the LOOP_NEST iter walk.
      u32 v_src = (u->src_count > 0) ? u->src[0] : 0;
      const char *value_expr = "_v"; // last TILE_LOAD/TILE_REDUCE leaves _v in scope
      if (v_src != 0 && v_src < ke->n_tile_uops
          && ke->tile_uops[v_src].op == TILE_SCALAR_BODY) {
        // Single scalar body store; emit a literal scalar reference.
        fprintf(fp, "out[_idx] = s%u; /* TILE_STORE S%u */\n",
                (u32)ke->tile_uops[v_src].extra, (u32)u->extra);
      } else {
        fprintf(fp, "out[_idx] = %s; /* TILE_STORE S%u */\n",
                value_expr, (u32)u->extra);
      }
      return;
    }
    case TILE_BLOCK: {
      tile_render_msl_indent(fp, depth);
      fputs("/* TILE_BLOCK begin */\n", fp);
      for (u8 s = 0; s < u->src_count; s++) {
        tile_render_msl_node(ke, u->src[s], fp, depth);
      }
      tile_render_msl_indent(fp, depth);
      fputs("/* TILE_BLOCK end */\n", fp);
      return;
    }
    case TILE_LOCAL_ALLOC: {
      TileAllocInfo info = tile_alloc_unpack(u->extra);
      tile_render_msl_indent(fp, depth);
      fprintf(fp, "threadgroup float _alloc%u[%u]; /*scope=%u*/\n",
              id, info.n_elements, info.scope);
      return;
    }
    case TILE_BARRIER:
      tile_render_msl_indent(fp, depth);
      fputs("threadgroup_barrier(mem_flags::mem_threadgroup);\n", fp);
      return;
    case TILE_REDUCE: {
      tile_render_msl_indent(fp, depth);
      fprintf(fp, "float _acc = 0.0f; /* TILE_REDUCE S%u */\n",
              (u32)u->extra);
      for (u8 s = 0; s < u->src_count; s++) {
        tile_render_msl_node(ke, u->src[s], fp, depth);
      }
      return;
    }
    case TILE_LOAD: {
      tile_render_msl_indent(fp, depth);
      // src[0] is either TILE_LOCAL_ALLOC or TILE_INPUT_BUF.
      // src[1] is the address (typically a TILE_SCALAR_BODY).
      u32 src0 = u->src[0];
      u32 src1 = (u->src_count > 1) ? u->src[1] : 0;
      char addr_buf[32];
      if (src1 != 0 && src1 < ke->n_tile_uops
          && ke->tile_uops[src1].op == TILE_SCALAR_BODY) {
        snprintf(addr_buf, sizeof(addr_buf), "s%u",
                 (u32)ke->tile_uops[src1].extra);
      } else {
        snprintf(addr_buf, sizeof(addr_buf), "/*addr*/");
      }
      if (src0 < ke->n_tile_uops
          && ke->tile_uops[src0].op == TILE_INPUT_BUF) {
        u32 slot = (u32)ke->tile_uops[src0].extra;
        fprintf(fp, "float _v = in%u[%s]; /* TILE_LOAD from input %u */\n",
                slot, addr_buf, slot);
      } else {
        fprintf(fp, "float _v = _alloc%u[%s]; /* TILE_LOAD */\n",
                src0, addr_buf);
      }
      return;
    }
    case TILE_INPUT_BUF:
      tile_render_msl_indent(fp, depth);
      fprintf(fp, "/* TILE_INPUT_BUF slot=%u */\n", (u32)u->extra);
      return;
    case TILE_OUTPUT_BUF:
      tile_render_msl_indent(fp, depth);
      fprintf(fp, "/* TILE_OUTPUT_BUF slot=%u */\n", (u32)u->extra);
      return;
    case TILE_SCALAR_BODY: {
      tile_render_msl_indent(fp, depth);
      // For trivial scalar leaves (S_CONST / S_ICONST), emit the
      // literal value directly so the rendered body reflects what
      // the kernel actually computes.  Phase F's renderer proper
      // will walk every ScalarUop op (S_LOAD, S_BINARY, ...) via
      // the existing rmt_emit_value emitter.
      u32 sid = (u32)u->extra;
      if (ke->scalar_uops != NULL && sid != 0
          && sid < ke->n_scalar_uops) {
        ScalarUop const *s = &ke->scalar_uops[sid];
        if (s->op == S_CONST) {
          union { u32 b; float f; } pun = { .b = (u32)s->extra };
          fprintf(fp, "float s%u = %ff; /* S_CONST */\n", sid, pun.f);
          return;
        }
        if (s->op == S_ICONST) {
          fprintf(fp, "int s%u = %d; /* S_ICONST */\n", sid,
                  (int)(i64)s->extra);
          return;
        }
      }
      fprintf(fp, "/* scalar body S%u */\n", sid);
      return;
    }
    case TILE_AXIS:
      // Standalone TILE_AXIS at non-loop-nest position; shouldn't
      // happen but emit as comment.
      tile_render_msl_indent(fp, depth);
      fputs("/* unexpected TILE_AXIS */\n", fp);
      return;
    default:
      tile_render_msl_indent(fp, depth);
      fprintf(fp, "/* unknown TILE_? op=%u */\n", u->op);
      return;
  }
}

// Map a thvm dtype to its MSL scalar type name.  Stays conservative:
// types Phase F's renderer does not yet emit code for return "?".
static const char *tile_msl_type_name(u32 dtype) {
  switch (dtype) {
    case DT_FP32:  return "float";
    case DT_FP16:  return "half";
    case DT_INT32: return "int";
    case DT_INT64: return "long";
    case DT_UINT8: return "uchar";
    default:       return "?";
  }
}

fn void tile_render_msl_skeleton(KernelEntry const *ke, FILE *fp) {
  if (fp == NULL) fp = stderr;
  if (ke == NULL || ke->tile_uops == NULL || ke->tile_root == 0) {
    fputs("// tile_render_msl_skeleton: <empty>\n", fp);
    return;
  }
  fputs("// === tile_render_msl_skeleton ===\n", fp);
  fputs("#include <metal_stdlib>\n", fp);
  fputs("using namespace metal;\n\n", fp);
  // Kernel signature derived from KernelEntry metadata.  Output goes
  // to buffer(0); each input goes to buffer(1+i).  Phase F's renderer
  // proper will switch to emitting TILE_INPUT_BUF / TILE_OUTPUT_BUF
  // leaves and reading the buffer arg list from those.
  fputs("kernel void tile_kernel(\n", fp);
  fprintf(fp, "    device %s *out [[ buffer(0) ]]",
          tile_msl_type_name(ke->output_dtype));
  for (u32 i = 0; i < ke->n_inputs; i++) {
    u32 dt = (ke->input_dtypes != NULL) ? ke->input_dtypes[i] : DT_FP32;
    fprintf(fp, ",\n    device const %s *in%u [[ buffer(%u) ]]",
            tile_msl_type_name(dt), i, i + 1);
  }
  fputs(",\n    uint tid [[ thread_position_in_grid ]],\n", fp);
  fputs("    uint tg [[ threadgroup_position_in_grid ]],\n", fp);
  fputs("    uint tt [[ thread_position_in_threadgroup ]]) {\n", fp);
  tile_render_msl_node(ke, ke->tile_root, fp, 1);
  fputs("}\n", fp);
}

fn const char *tile_op_name(u8 op) {
  switch (op) {
    case TILE_NONE:        return "TILE_NONE";
    case TILE_AXIS:        return "TILE_AXIS";
    case TILE_SCALAR_BODY: return "TILE_SCALAR_BODY";
    case TILE_LOOP_NEST:   return "TILE_LOOP_NEST";
    case TILE_LOCAL_ALLOC: return "TILE_LOCAL_ALLOC";
    case TILE_LOAD:        return "TILE_LOAD";
    case TILE_STORE:       return "TILE_STORE";
    case TILE_BARRIER:     return "TILE_BARRIER";
    case TILE_REDUCE:      return "TILE_REDUCE";
    case TILE_BLOCK:       return "TILE_BLOCK";
    case TILE_INPUT_BUF:   return "TILE_INPUT_BUF";
    case TILE_OUTPUT_BUF:  return "TILE_OUTPUT_BUF";
    default:               return "TILE_?";
  }
}

// Phase F prep: tile-IR-native dispatch shape derivation.  Walks the
// tile_root's TILE_AXIS children and computes (groups, threads) the
// same way render_metal's CtKernelInfo path does -- but reads kax_type
// + extent directly from TILE_AXIS instead of going through the
// KpSchedule side channel.  Returns 1 on success, 0 if the kernel
// can't be dispatched as tile-only (no axes, overflow, or
// GROUP_REDUCE > 256).
//
// Three modes mirror rmt_axis_mode():
//   LOCAL_GLOBAL: at least one KAX_LOCAL + at least one KAX_GLOBAL
//                 -> groups = product(non-LOCAL extents)
//                    threads = LOCAL extent
//   GROUP_REDUCE: at least one KAX_GROUP_REDUCE
//                 -> groups = product(non-GROUP_REDUCE extents)
//                    threads = GROUP_REDUCE extent
//   FLAT_GRID:    everything else (typically all KAX_LOOP)
//                 -> total = product(extents); threads = min(256, total);
//                    groups = ceil(total / threads)
//
// No consumer yet -- the renderer rewrite (Phase F proper) will swap
// cg_tile_metal_dispatch_shape's CtKernelInfo path to call this.
fn int tile_compute_dispatch_shape(KernelEntry const *ke, u32 *groups_out,
                                   u32 *threads_out) {
  if (ke == NULL || ke->tile_uops == NULL || ke->tile_root == 0) {
    return 0;
  }
  if (ke->tile_root >= ke->n_tile_uops) {
    return 0;
  }
  TileUop const *root = &ke->tile_uops[ke->tile_root];
  if (root->op != TILE_LOOP_NEST || root->src_count < 2) {
    return 0;
  }

  u32 has_local = 0, has_global = 0, has_group_reduce = 0;
  u32 local_extent = 1, group_reduce_extent = 1;
  u64 group_total = 1, total = 1;
  for (u8 s = 1; s < root->src_count; s++) {
    u32 ax_id = root->src[s];
    if (ax_id >= ke->n_tile_uops
        || ke->tile_uops[ax_id].op != TILE_AXIS) {
      return 0;
    }
    TileAxisInfo info = tile_axis_unpack(ke->tile_uops[ax_id].extra);
    if (info.extent == 0) return 0;
    if (info.kax_type == KAX_LOCAL) {
      has_local = 1;
      local_extent = info.extent;
      total *= (u64)info.extent;
    } else if (info.kax_type == KAX_GLOBAL) {
      has_global = 1;
      group_total *= (u64)info.extent;
      total *= (u64)info.extent;
    } else if (info.kax_type == KAX_GROUP_REDUCE) {
      has_group_reduce = 1;
      group_reduce_extent = info.extent;
    } else if (info.kax_type == KAX_LOOP || info.kax_type == KAX_UPCAST) {
      // Output axes contribute to dispatch grid.
      group_total *= (u64)info.extent;
      total *= (u64)info.extent;
    }
    // KAX_REDUCE / KAX_UNROLL: in-kernel sequential reduction, do not
    // contribute to dispatch grid (matches ct_axis_is_output filter).
  }
  if (total == 0 || total > 0xFFFFFFFFu) return 0;

  u32 groups = 1, threads = 1;
  if (has_group_reduce) {
    if (group_reduce_extent == 0 || group_reduce_extent > 256) {
      return 0;
    }
    if (group_total > 0xFFFFFFFFu) return 0;
    groups  = (u32)group_total;
    threads = group_reduce_extent;
  } else if (has_local && has_global) {
    if (group_total == 0 || group_total > 0xFFFFFFFFu) return 0;
    groups  = (u32)group_total;
    threads = local_extent;
  } else {
    threads = total < 256 ? (u32)total : 256u;
    groups  = (u32)((total + (u64)threads - 1) / (u64)threads);
  }
  if (groups == 0 || threads == 0) return 0;

  if (groups_out  != NULL) *groups_out  = groups;
  if (threads_out != NULL) *threads_out = threads;
  return 1;
}

fn const char *tile_axis_name(u32 axis_type) {
  switch (axis_type) {
    case KAX_LOOP:         return "LOOP";
    case KAX_REDUCE:       return "REDUCE";
    case KAX_UPCAST:       return "UPCAST";
    case KAX_UNROLL:       return "UNROLL";
    case KAX_LOCAL:        return "LOCAL";
    case KAX_GLOBAL:       return "GLOBAL";
    case KAX_GROUP_REDUCE: return "GROUP_REDUCE";
    default:               return "?";
  }
}

static u8 tile_axis_from_scalar_axis(u32 axis_type) {
  switch (axis_type) {
    case S_AXIS_LOOP:    return KAX_LOOP;
    case S_AXIS_REDUCE:  return KAX_REDUCE;
    case S_AXIS_UNROLL:  return KAX_UNROLL;
    case S_AXIS_GLOBAL:  return KAX_GLOBAL;
    case S_AXIS_VIRT:    return KAX_LOOP;
    default:             return KAX_LOOP;
  }
}

static int tile_axis_type_ok(u32 axis_type) {
  return axis_type <= KAX_GROUP_REDUCE;
}

static int tile_id_ok(KernelEntry const *ke, u32 id) {
  return id != 0 && id < ke->n_tile_uops;
}

#define TILE_REDUCE_KIND(arg)  (((arg) >> 24) & 0xFFu)

// Matmul shapes are canonicalised as MUL+REDUCE+OPT_TC scalar_uops by
// rangeify and lifted into the UOp DAG by kernel_lift_to_uop.
// Downstream consumers (BLAS GEMM/DOT/GEMV dispatch, apply_opt KOP_TC
// gate, propose KOP_TC tile-size proposer) read shape facts from
// ke->cached_lift.store_root via uop_dag_classify_matmul_shape /
// uop_dag_classify_dot_shape / uop_dag_classify_gemv_shape
// (src/uop/dag_scan.c).

static u32 tile_isqrt_exact(u32 x) {
  for (u32 r = 1; r <= x / r; r++) {
    if (r * r == x) {
      return r;
    }
  }
  return 0;
}

static u32 tile_conv2d_threads_from_opts(KernelEntry const *ke) {
  u32 threads = 256;
  u32 n_app = tile_anno_applied_opts_count(ke);
  KOpt const *opts = tile_anno_applied_opts(ke);
  for (u32 i = 0; i < n_app; i++) {
    KOpt opt = opts[i];
    if (opt.op == KOP_LOCAL && opt.arg > 0 && opt.arg <= 256) {
      threads = opt.arg;
    }
  }
  return threads;
}

static u32 tile_conv2d_outputs_from_opts(KernelEntry const *ke) {
  u32 outputs = 1;
  u32 n_app = tile_anno_applied_opts_count(ke);
  KOpt const *opts = tile_anno_applied_opts(ke);
  for (u32 i = 0; i < n_app; i++) {
    KOpt opt = opts[i];
    if (opt.op == KOP_UPCAST && opt.arg > 0 && opt.arg <= 16) {
      outputs = opt.arg;
    }
  }
  return outputs;
}

static u32 tile_conv2d_reduce_unroll_from_opts(KernelEntry const *ke,
                                               u32 reduce_extent) {
  u32 unroll = 1;
  if (reduce_extent == 0) return unroll;
  u32 n_app = tile_anno_applied_opts_count(ke);
  KOpt const *opts = tile_anno_applied_opts(ke);
  for (u32 i = 0; i < n_app; i++) {
    KOpt opt = opts[i];
    if (opt.op == KOP_UNROLL && opt.arg > 1 && opt.arg <= 16
        && reduce_extent % opt.arg == 0) {
      unroll = opt.arg;
    }
  }
  return unroll;
}

// Counters for the conv2d-flat DAG migration.  DAG path fires when
// `cached_lift.store_root != 0` AND
// `uop_dag_classify_conv2d_flat_shape` accepts.  LEGACY path fires
// when the lifter declined (store_root == 0) and we fall back to the
// program[]-side last-op check.  Tests assert both counters increment
// for their respective fixtures (production vs hand-built KProgOp).
static u64 TILE_CONV2D_FLAT_DAG    = 0;
static u64 TILE_CONV2D_FLAT_LEGACY = 0;

fn u64 tile_conv2d_flat_dag_count   (void) { return TILE_CONV2D_FLAT_DAG;    }
fn u64 tile_conv2d_flat_legacy_count(void) { return TILE_CONV2D_FLAT_LEGACY; }
fn void tile_conv2d_flat_counters_reset(void) {
  TILE_CONV2D_FLAT_DAG    = 0;
  TILE_CONV2D_FLAT_LEGACY = 0;
}

static int tile_analyze_conv2d_flat_impl(KernelEntry const *ke,
                                         TileConv2DInfo *out,
                                         int allow_cin1) {
  if (ke == NULL || out == NULL || ke->n_inputs < 2) {
    return 0;
  }
  if (ke->input_dtypes[0] != DT_FP32 || ke->output_dtype != DT_FP32) {
    return 0;
  }
  for (u32 i = 1; i < ke->n_inputs; i++) {
    if (ke->input_dtypes[i] != DT_FP32) {
      return 0;
    }
  }

  // Prefer the DAG-side structural gate when the lifter has
  // populated cached_lift.store_root.  Under default
  // THVM_PHASE_C7_FREE_PROGRAM=1 program[] is freed post-materialize,
  // so the program[] reader would early-bail (program == NULL) for
  // production conv kernels.  The program[] fallback covers the
  // lift-decline path AND test_tile_graph fixtures that hand-build
  // KProgOp without running the lifter.
  if (ke->cached_lift.store_root != 0) {
    if (!uop_dag_classify_conv2d_flat_shape(ke->cached_lift.store_root, ke)) {
      return 0;
    }
    TILE_CONV2D_FLAT_DAG++;
  } else {
    if (ke->n_ops == 0 || ke->program == NULL) return 0;
    KProgOp const *last = &ke->program[ke->n_ops - 1];
    if (last->opcode != UOP_REDUCE || last->n_src != 1
        || TILE_REDUCE_KIND(last->arg) != REDUCE_SUM) {
      return 0;
    }
    TILE_CONV2D_FLAT_LEGACY++;
  }

  // Try the DAG-side full-shape extractor first.  When the lift
  // took the single-input path AND the address tree wasn't
  // degenerated by the simplifier, the extractor recovers every
  // shape field from the W and X INDEX_E.addr trees.  Multi-input
  // conv (IWHERE chain) and degenerate kh==kw==1 still fall through
  // to the input_views reader below.
  if (ke->cached_lift.store_root != 0 && ke->n_inputs == 2) {
    Term sroot = ke->cached_lift.store_root;
    if (term_tag(sroot) == TAG_UOP && term_ext(sroot) == UOP_STORE) {
      Term value = heap_read(term_val(sroot) + 2);
      if (term_tag(value) == TAG_UOP && term_ext(value) == UOP_OPT) {
        value = uop_opt_target(value);
      }
      if (term_tag(value) == TAG_UOP && term_ext(value) == UOP_REDUCE) {
        Term mul = heap_read(term_val(value) + 0);
        if (term_tag(mul) == TAG_UOP && term_ext(mul) == UOP_MUL) {
          Term ldW = heap_read(term_val(mul) + 0);
          Term ldX = heap_read(term_val(mul) + 1);
          if (term_tag(ldW) == TAG_UOP && term_ext(ldW) == UOP_INDEX_E
              && term_tag(ldX) == TAG_UOP && term_ext(ldX) == UOP_INDEX_E) {
            Term addr_w = heap_read(term_val(ldW) + 1);
            Term addr_x = heap_read(term_val(ldX) + 1);
            UopDagConv2dFlatShape ds = {0};
            if (uop_dag_extract_conv2d_flat_shape(addr_w, addr_x, ke, &ds)
                && (ds.c_in > 1 || allow_cin1)
                && ds.c_out * ds.patches == ke->output_numel) {
              // Cross-check output_shape too -- legacy did this gate.
              int out_flat = ke->output_shape.ndim == 2
                          && ke->output_shape.dims[0] == ds.c_out
                          && ke->output_shape.dims[1] == ds.patches;
              int out_chw  = ke->output_shape.ndim == 3
                          && ke->output_shape.dims[0] == ds.c_out
                          && ke->output_shape.dims[1] == ds.h_out
                          && ke->output_shape.dims[2] == ds.w_out;
              if (out_flat || out_chw) {
                memset(out, 0, sizeof(TileConv2DInfo));
                out->dtype     = DT_FP32;
                out->w_input   = 0;
                out->x_input   = 1;
                out->patch_input_base  = 0;
                out->patch_input_count = 0;
                out->c_out     = ds.c_out;
                out->c_in      = ds.c_in;
                out->h         = ds.h_out + ds.kh - 1;
                out->w         = ds.w_out + ds.kw - 1;
                out->kh        = ds.kh;
                out->kw        = ds.kw;
                out->h_out     = ds.h_out;
                out->w_out     = ds.w_out;
                out->batch     = ds.batch;
                out->patches   = ds.patches;
                out->spatial_patches = ds.spatial_patches;
                out->w_offset  = ds.w_offset;
                out->w_stride0 = ds.w_stride0;
                out->w_stride1 = ds.w_stride1;
                out->x_offset  = ds.x_offset;
                out->x_stride_b = ds.x_stride_b;
                out->x_stride0 = ds.x_stride0;
                out->x_stride1 = ds.x_stride1;
                out->x_stride2 = ds.x_stride2;
                out->threads   = tile_conv2d_threads_from_opts(ke);
                out->outputs_per_thread = tile_conv2d_outputs_from_opts(ke);
                u32 k_total = ds.c_in * ds.kh * ds.kw;
                out->reduce_unroll = tile_conv2d_reduce_unroll_from_opts(ke,
                                                                         k_total);
                return 1;
              }
            }
          }
        }
      }
    }
  }

  // DAG-side extractor declined (multi-input, degenerate, or
  // non-canonical address shape).  Fall back to the input_views[]
  // reader.
  if (ke->input_views == NULL) return 0;

  View const *wv = &ke->input_views[0];
  if (wv->shape.ndim != 3
      || (ke->output_shape.ndim != 2 && ke->output_shape.ndim != 3)) {
    return 0;
  }
  u32 c_out = wv->shape.dims[0];
  u32 k     = wv->shape.dims[1];
  u32 p     = wv->shape.dims[2];
  u32 batch = 1;
  u32 c_in = 0, h = 0, w = 0;
  i32 x_offset = 0;
  i32 x_stride_b = 0;
  i32 x_stride0 = 0, x_stride1 = 0, x_stride2 = 0;
  u32 patch_base = 0, patch_count = 0;
  if (c_out == 0 || k == 0 || p == 0) {
    return 0;
  }
  u32 kh = 0, kw = 0, h_out = 0, w_out = 0, spatial_patches = 0;
  int direct_x = ke->n_inputs == 2;
  if (direct_x) {
    View const *xv = &ke->input_views[1];
    if (xv->shape.ndim != 3 && xv->shape.ndim != 4) {
      return 0;
    }
    if (xv->shape.ndim == 3) {
      c_in      = xv->shape.dims[0];
      h         = xv->shape.dims[1];
      w         = xv->shape.dims[2];
      x_offset  = xv->offset;
      x_stride0 = xv->strides[0];
      x_stride1 = xv->strides[1];
      x_stride2 = xv->strides[2];
    } else {
      batch      = xv->shape.dims[0];
      c_in       = xv->shape.dims[1];
      h          = xv->shape.dims[2];
      w          = xv->shape.dims[3];
      x_offset   = xv->offset;
      x_stride_b = xv->strides[0];
      x_stride0  = xv->strides[1];
      x_stride1  = xv->strides[2];
      x_stride2  = xv->strides[3];
    }
    if (c_in == 0 || (c_in == 1 && !allow_cin1) || k % c_in != 0) {
      return 0;
    }
    u32 k_spatial = k / c_in;
    kh = tile_isqrt_exact(k_spatial);
    if (kh == 0) {
      return 0;
    }
    kw = kh;
    if (h < kh || w < kw) {
      return 0;
    }
    h_out = h - kh + 1;
    w_out = w - kw + 1;
    spatial_patches = h_out * w_out;
  } else {
    c_in = 1;
    kh = tile_isqrt_exact(k);
    if (kh == 0) {
      return 0;
    }
    kw = kh;
    patch_base = 1;
    patch_count = k;
    if (ke->n_inputs != 1 + patch_count) {
      return 0;
    }
    View const *pv0 = &ke->input_views[patch_base];
    if (pv0->shape.ndim == 3) {
      if (pv0->shape.dims[0] != 1) {
        return 0;
      }
      h_out = pv0->shape.dims[1];
      w_out = pv0->shape.dims[2];
    } else if (pv0->shape.ndim == 4) {
      if (pv0->shape.dims[0] != 1) {
        return 0;
      }
      batch = pv0->shape.dims[1];
      h_out = pv0->shape.dims[2];
      w_out = pv0->shape.dims[3];
    } else {
      return 0;
    }
    for (u32 i = 0; i < patch_count; i++) {
      View const *pv = &ke->input_views[patch_base + i];
      if (pv->shape.ndim != pv0->shape.ndim) {
        return 0;
      }
      if (pv->shape.ndim == 3) {
        if (pv->shape.dims[0] != 1 || pv->shape.dims[1] != h_out
            || pv->shape.dims[2] != w_out) {
          return 0;
        }
      } else if (pv->shape.dims[0] != 1 || pv->shape.dims[1] != batch
          || pv->shape.dims[2] != h_out || pv->shape.dims[3] != w_out) {
        return 0;
      }
    }
    h = h_out + kh - 1;
    w = w_out + kw - 1;
    spatial_patches = h_out * w_out;
  }
  if (kh == 0) {
    return 0;
  }
  if (batch == 0 || spatial_patches == 0 || batch * spatial_patches != p) {
    return 0;
  }
  int output_flat = ke->output_shape.ndim == 2
                 && ke->output_shape.dims[0] == c_out
                 && ke->output_shape.dims[1] == p;
  int output_chw = ke->output_shape.ndim == 3
                && ke->output_shape.dims[0] == c_out
                && ke->output_shape.dims[1] == h_out
                && ke->output_shape.dims[2] == w_out;
  if ((!output_flat && !output_chw) || ke->output_numel != c_out * p) {
    return 0;
  }
  if (wv->strides[2] != 0) {
    return 0;
  }

  memset(out, 0, sizeof(TileConv2DInfo));
  out->dtype     = DT_FP32;
  out->w_input   = 0;
  out->x_input   = direct_x ? 1 : 0;
  out->patch_input_base  = patch_base;
  out->patch_input_count = patch_count;
  out->c_out     = c_out;
  out->c_in      = c_in;
  out->h         = h;
  out->w         = w;
  out->kh        = kh;
  out->kw        = kw;
  out->h_out     = h_out;
  out->w_out     = w_out;
  out->batch     = batch;
  out->patches   = p;
  out->spatial_patches = spatial_patches;
  out->w_offset  = wv->offset;
  out->w_stride0 = wv->strides[0];
  out->w_stride1 = wv->strides[1];
  out->x_offset  = x_offset;
  out->x_stride_b = x_stride_b;
  out->x_stride0 = x_stride0;
  out->x_stride1 = x_stride1;
  out->x_stride2 = x_stride2;
  out->threads   = tile_conv2d_threads_from_opts(ke);
  out->outputs_per_thread = tile_conv2d_outputs_from_opts(ke);
  out->reduce_unroll = tile_conv2d_reduce_unroll_from_opts(ke, k);
  return 1;
}

int tile_analyze_conv2d_flat(KernelEntry const *ke, TileConv2DInfo *out) {
  return tile_analyze_conv2d_flat_impl(ke, out, 0);
}

int tile_rejects_conv2d_flat_cin1(KernelEntry const *ke) {
  TileConv2DInfo conv;
  if (!tile_analyze_conv2d_flat_impl(ke, &conv, 1)) {
    return 0;
  }
  return conv.c_in == 1 && conv.patch_input_count == 0;
}

static int tile_collect_axis_info(KernelEntry const *ke, u32 axis_id,
                                  u32 *axis_type, u32 *extent) {
  if (!tile_id_ok(ke, axis_id)) {
    return 0;
  }
  TileUop const *axis = &ke->tile_uops[axis_id];
  TileAxisInfo info = tile_axis_unpack(axis->extra);
  if (axis->op != TILE_AXIS || axis->src_count != 0
      || !tile_axis_type_ok(info.kax_type) || info.extent == 0) {
    return 0;
  }
  if (axis_type != NULL) {
    *axis_type = info.kax_type;
  }
  if (extent != NULL) {
    *extent = info.extent;
  }
  return 1;
}

// Matmul shape facts flow through ke->cached_lift.store_root via
// uop_dag_classify_matmul_shape.


