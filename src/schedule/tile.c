// schedule/tile.c -- tile-plan arena above scalar-UOps.
//
// This is intentionally non-dispatching scaffolding.  rangeify owns
// semantic lowering to ScalarUop; tile.c owns a future schedule/memory
// plan that can wrap those scalar bodies with explicit axes, local
// memory, barriers, reductions, and MMA nodes.

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

fn void tile_free(KernelEntry *ke) {
  if (ke->tile_uops != NULL) {
    free(ke->tile_uops);
  }
  ke->tile_uops     = NULL;
  ke->n_tile_uops   = 0;
  ke->tile_uops_cap = 0;
  ke->tile_root     = 0;
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
    case TILE_MMA:         return "TILE_MMA";
    default:               return "TILE_?";
  }
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

fn u32 tile_loop_axis_count(KernelEntry const *ke) {
  if (ke == NULL || ke->tile_uops == NULL || !tile_id_ok(ke, ke->tile_root)) {
    return 0;
  }
  TileUop const *root = &ke->tile_uops[ke->tile_root];
  if (root->op != TILE_LOOP_NEST || root->src_count == 0) {
    return 0;
  }
  return (u32)root->src_count - 1;
}

fn u32 tile_loop_axis_type(KernelEntry const *ke, u32 axis) {
  if (axis >= tile_loop_axis_count(ke)) {
    return 0;
  }
  TileUop const *root = &ke->tile_uops[ke->tile_root];
  TileUop const *u = &ke->tile_uops[root->src[1 + axis]];
  return (u32)(u->extra >> 32);
}

fn u32 tile_loop_axis_extent(KernelEntry const *ke, u32 axis) {
  if (axis >= tile_loop_axis_count(ke)) {
    return 0;
  }
  TileUop const *root = &ke->tile_uops[ke->tile_root];
  TileUop const *u = &ke->tile_uops[root->src[1 + axis]];
  return (u32)(u->extra & 0xFFFFFFFFu);
}

fn int tile_validate(KernelEntry const *ke) {
  if (ke == NULL || ke->tile_uops == NULL || ke->n_tile_uops == 0) {
    return 0;
  }
  if (ke->tile_uops[0].op != TILE_NONE) {
    return 0;
  }
  if (!tile_id_ok(ke, ke->tile_root)) {
    return 0;
  }

  TileUop const *root = &ke->tile_uops[ke->tile_root];
  if (root->op != TILE_LOOP_NEST || root->src_count < 2) {
    return 0;
  }

  u32 body_id = root->src[0];
  if (!tile_id_ok(ke, body_id)) {
    return 0;
  }
  TileUop const *body = &ke->tile_uops[body_id];
  if (body->op != TILE_SCALAR_BODY || body->src_count != 0) {
    return 0;
  }
  u32 scalar_root = (u32)body->extra;
  if (ke->scalar_uops == NULL || scalar_root == 0
      || scalar_root >= ke->n_scalar_uops
      || ke->scalar_uops[scalar_root].op != S_BUFFERIZE) {
    return 0;
  }

  for (u32 i = 1; i < root->src_count; i++) {
    u32 axis_id = root->src[i];
    if (!tile_id_ok(ke, axis_id)) {
      return 0;
    }
    TileUop const *axis = &ke->tile_uops[axis_id];
    u32 axis_type = (u32)(axis->extra >> 32);
    u32 extent    = (u32)(axis->extra & 0xFFFFFFFFu);
    if (axis->op != TILE_AXIS || axis->src_count != 0
        || !tile_axis_type_ok(axis_type) || extent == 0) {
      return 0;
    }
  }
  return 1;
}

static u32 tile_find_scalar_bufferize(KernelEntry const *ke) {
  if (ke->scalar_uops == NULL) {
    return 0;
  }
  for (u32 i = 1; i < ke->n_scalar_uops; i++) {
    if (ke->scalar_uops[i].op == S_BUFFERIZE) {
      return i;
    }
  }
  return 0;
}

static u32 tile_emit_axes_from_kernel_axes(KernelEntry *ke, u32 *out, u32 cap) {
  if (ke->axes == NULL) {
    return 0;
  }
  if (ke->axes->n_axes == 0) {
    axes_default_for(ke);
  }
  if (ke->axes->n_axes == 0 || ke->axes->n_axes > cap) {
    return 0;
  }

  for (u32 i = 0; i < ke->axes->n_axes; i++) {
    u32 axis_type = ke->axes->axis_types[i];
    u32 extent    = ke->axes->full_shape[i];
    u64 extra     = ((u64)axis_type << 32) | (u64)extent;
    out[i] = tile_emit_leaf(ke, TILE_AXIS, DT_INT64, extra);
  }
  return ke->axes->n_axes;
}

static u32 tile_emit_axes_from_scalar_root(KernelEntry *ke, u32 root,
                                           u32 *out, u32 cap) {
  ScalarUop const *buf = &ke->scalar_uops[root];
  if (buf->op != S_BUFFERIZE || buf->src_count == 0) {
    return 0;
  }
  u32 n_axes = (u32)buf->src_count - 1;
  if (n_axes == 0 || n_axes > cap) {
    return 0;
  }

  for (u32 i = 0; i < n_axes; i++) {
    u32 rid = buf->src[1 + i];
    if (rid == 0 || rid >= ke->n_scalar_uops) {
      return 0;
    }
    ScalarUop const *r = &ke->scalar_uops[rid];
    if (r->op != S_RANGE) {
      return 0;
    }
    u32 scalar_axis = (u32)((r->extra >> 32) & 0xFFFFFFFFu);
    u32 extent      = (u32)(r->extra & 0xFFFFFFFFu);
    u32 axis_type   = tile_axis_from_scalar_axis(scalar_axis);
    u64 extra       = ((u64)axis_type << 32) | (u64)extent;
    out[i] = tile_emit_leaf(ke, TILE_AXIS, DT_INT64, extra);
  }
  return n_axes;
}

fn int tile_build_from_scalar(KernelEntry *ke) {
  u32 root = tile_find_scalar_bufferize(ke);
  if (root == 0) {
    return 0;
  }
  if (ke->scalar_uops[root].src_count == 0) {
    return 0;
  }

  tile_free(ke);

  u32 body = tile_emit_leaf(ke, TILE_SCALAR_BODY,
                            ke->scalar_uops[root].dtype, root);
  u32 axes[MAX_AXES];
  u32 n_axes = tile_emit_axes_from_kernel_axes(ke, axes, MAX_AXES);
  if (n_axes == 0) {
    n_axes = tile_emit_axes_from_scalar_root(ke, root, axes, MAX_AXES);
  }
  if (n_axes == 0) {
    tile_free(ke);
    return 0;
  }

  u32 src[TILE_MAX_SRC] = {body};
  for (u32 i = 0; i < n_axes; i++) {
    src[1 + i] = axes[i];
  }
  ke->tile_root = tile_emit(ke, TILE_LOOP_NEST, ke->scalar_uops[root].dtype,
                            (u8)(1 + n_axes), src, 0);
  if (!tile_validate(ke)) {
    tile_free(ke);
    return 0;
  }
  return 1;
}
