// schedule/kvar.c -- symbolic-shape Variable registry.
//
// A kvar represents a kernel-shape parameter (e.g. "BS") that should
// flow through the schedule as a SYMBOL rather than a concrete u32.
// A RANGE leaf whose extent is bound to a kvar bakes the per-process
// kvar id into the MSL source (as `V_<name>`) and the actual numeric
// extent gets bound as a `constant uint` kernel argument at dispatch
// time.  Two kernels with the same scalar/UOp shape but different
// kvar runtime values therefore share a single MSL string -- one PSO
// covers all runtime values.  (tinygrad's `Variable("BS", lo, hi)`.)
//
// This is the minimal vertical slice: a flat array of KVar records,
// id assigned in declaration order (1..N).  Slot 0 is reserved as
// "none".  No deallocation; the registry lives for the process.
//
// All exported symbols are plain (non-`fn`) functions because the
// Metal backend (build/backend_metal.o, a separate TU) calls a few
// of them -- `static inline` wouldn't be visible across the TU
// boundary.  Declarations live in src/thvm.h next to the schedule
// section.
//
// Used by:
//   - uop/index.c       : uop_range_extent_is_var / _var_id / _static_extent
//   - render_uop.c       : `V_<name>` loop bound + `constant uint &V_<name>`
//                           kernel-signature arg
//   - backend/metal/_.m  : metal_tile_jit_hash excludes kvar-bound numels;
//                           metal_tile_jit_encode setBytes:s the runtime values

#define KVAR_NAME_MAX  16
#define KVAR_CAP       64

typedef struct {
  char name[KVAR_NAME_MAX];
  u32  lo;
  u32  hi;
} KVar;

static KVar KVARS[KVAR_CAP];
static u32  KVARS_NEXT = 1;   // slot 0 reserved

u32 kvar_alloc(const char *name, u32 lo, u32 hi) {
  if (KVARS_NEXT >= KVAR_CAP) {
    fprintf(stderr, "kvar_alloc: registry full (cap=%u)\n", KVAR_CAP);
    return 0;
  }
  if (hi == 0 || hi < lo) {
    fprintf(stderr, "kvar_alloc: bad range lo=%u hi=%u\n", lo, hi);
    return 0;
  }
  u32 id = KVARS_NEXT++;
  if (name != NULL) {
    size_t n = strlen(name);
    if (n >= KVAR_NAME_MAX) n = KVAR_NAME_MAX - 1;
    memcpy(KVARS[id].name, name, n);
    KVARS[id].name[n] = '\0';
  } else {
    snprintf(KVARS[id].name, KVAR_NAME_MAX, "V%u", id);
  }
  KVARS[id].lo = lo;
  KVARS[id].hi = hi;
  return id;
}

const char *kvar_name(u32 id) {
  if (id == 0 || id >= KVARS_NEXT) return NULL;
  return KVARS[id].name;
}

u32 kvar_lo(u32 id) {
  if (id == 0 || id >= KVARS_NEXT) return 0;
  return KVARS[id].lo;
}

u32 kvar_hi(u32 id) {
  if (id == 0 || id >= KVARS_NEXT) return 0;
  return KVARS[id].hi;
}

u32 kvar_count(void) { return KVARS_NEXT - 1; }

// Reset the registry.  Used by tests that exercise multiple
// independent kvar scenarios in one process.
void kvar_reset(void) {
  memset(KVARS, 0, sizeof(KVARS));
  KVARS_NEXT = 1;
}

// ---- RANGE-extent encoding helpers ---------------------------------
// A RANGE leaf packs its extent as NUM(extent) in the UOP_RANGE
// heap layout.  When the
// KVAR_FLAG bit (bit 31) is set, the remaining 31 bits hold the kvar
// id; the static numeric extent is kvar_hi(id) (the upper bound --
// BS=512 covers BS=4 and BS=32 in the symbolic regime, so buffers /
// dispatch shapes are sized against the worst case and the actual
// per-fire value is what's bound via setBytes:).
//
// Layout (within a u32 extent slot):
//   bit 31      : KVAR_FLAG  (1 = variable, 0 = literal extent)
//   bits 30..0  : var_id when flag set, literal extent otherwise
// (KVAR_FLAG_BIT / KVAR_FLAG / KVAR_ID_MASK / KVAR_USED_CAP are in
// thvm.h so the renderer + Metal backend can use them directly.)

u32 kvar_pack_extent(u32 var_id) {
  return KVAR_FLAG | (var_id & KVAR_ID_MASK);
}

int kvar_extent_is_var(u32 packed_extent) {
  return (packed_extent & KVAR_FLAG) != 0;
}

u32 kvar_extent_var_id(u32 packed_extent) {
  if (!kvar_extent_is_var(packed_extent)) return 0;
  return packed_extent & KVAR_ID_MASK;
}

// Resolve a packed extent to a concrete u32.  For literal extents this
// is identity; for kvar-bound extents it returns kvar_hi(id) (the
// static upper bound, used for buffer sizing / dispatch shape).
u32 kvar_extent_static(u32 packed_extent) {
  if (!kvar_extent_is_var(packed_extent)) return packed_extent;
  return kvar_hi(kvar_extent_var_id(packed_extent));
}

// ---- Per-kernel runtime-value bindings -----------------------------
// Caller stamps before dispatch; kernel_kvar_bind returns 1 on
// success, 0 if the table is full.  Idempotent: rebinding an existing
// var id overwrites its value.  kernel_kvar_value reads it back,
// falling through to kvar_hi(id) (worst-case upper bound) when no
// binding exists, so legacy / un-wired kernels keep working.

int kernel_kvar_bind(struct KernelEntry *ke, u32 var_id, u32 value) {
  if (ke == NULL || var_id == 0) return 0;
  for (u32 i = 0; i < ke->n_kvar_runtime; i++) {
    if (ke->kvar_runtime_ids[i] == var_id) {
      ke->kvar_runtime_vals[i] = value;
      return 1;
    }
  }
  if (ke->n_kvar_runtime >= KVAR_USED_CAP) return 0;
  u32 slot = ke->n_kvar_runtime++;
  ke->kvar_runtime_ids [slot] = var_id;
  ke->kvar_runtime_vals[slot] = value;
  return 1;
}

u32 kernel_kvar_value(struct KernelEntry const *ke, u32 var_id) {
  if (ke != NULL) {
    for (u32 i = 0; i < ke->n_kvar_runtime; i++) {
      if (ke->kvar_runtime_ids[i] == var_id) {
        return ke->kvar_runtime_vals[i];
      }
    }
  }
  return kvar_hi(var_id);
}

// ---- DAG / arena scans: collect kvars referenced by a kernel -------
// Both produce the unique kvar ids in stable ascending order so the
// renderer (kernel signature) and dispatcher (setBytes order) agree
// on buffer-index assignment regardless of walk order.

static void kvar_scan_rec(Term t, u32 *out_ids, u32 *n_out, u32 cap) {
  if (term_tag(t) != TAG_UOP) return;
  u32 op = term_ext(t);
  if (op == UOP_RANGE) {
    u64 loc = term_val(t);
    u32 ext = (u32)term_val(heap_read(loc + 2));
    if (kvar_extent_is_var(ext)) {
      u32 vid = kvar_extent_var_id(ext);
      if (vid != 0) {
        for (u32 i = 0; i < *n_out; i++) {
          if (out_ids[i] == vid) return;
        }
        if (*n_out < cap) out_ids[(*n_out)++] = vid;
      }
    }
    return;
  }
  u64 loc = term_val(t);
  switch (op) {
    case UOP_STORE: {
      kvar_scan_rec(heap_read(loc + 0), out_ids, n_out, cap);  // dest INDEX_E
      kvar_scan_rec(heap_read(loc + 1), out_ids, n_out, cap);  // addr (RANGE chain)
      kvar_scan_rec(heap_read(loc + 2), out_ids, n_out, cap);  // value
      break;
    }
    case UOP_INDEX_E:
    case UOP_AFTER: {
      kvar_scan_rec(heap_read(loc + 0), out_ids, n_out, cap);
      kvar_scan_rec(heap_read(loc + 1), out_ids, n_out, cap);
      break;
    }
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL: case UOP_IDIV:
    case UOP_IMOD: case UOP_ILT:  case UOP_IAND:
    case UOP_ADD:  case UOP_MUL:  case UOP_CMPLT: case UOP_CMPEQ: {
      kvar_scan_rec(heap_read(loc + 0), out_ids, n_out, cap);
      kvar_scan_rec(heap_read(loc + 1), out_ids, n_out, cap);
      break;
    }
    case UOP_IWHERE: {
      kvar_scan_rec(heap_read(loc + 0), out_ids, n_out, cap);
      kvar_scan_rec(heap_read(loc + 1), out_ids, n_out, cap);
      kvar_scan_rec(heap_read(loc + 2), out_ids, n_out, cap);
      break;
    }
    case UOP_REDUCE:
    case UOP_OPT:
    case UOP_LOAD: case UOP_CAST: case UOP_BITCAST:
    case UOP_NEG:  case UOP_RECIP: case UOP_EXP2: case UOP_LOG2:
    case UOP_SQRT: {
      kvar_scan_rec(heap_read(loc + 0), out_ids, n_out, cap);
      break;
    }
    default: break;
  }
}

static void kvar_sort_ascending(u32 *ids, u32 n) {
  for (u32 i = 0; i + 1 < n; i++) {
    u32 mi = i;
    for (u32 j = i + 1; j < n; j++) {
      if (ids[j] < ids[mi]) mi = j;
    }
    if (mi != i) { u32 t = ids[i]; ids[i] = ids[mi]; ids[mi] = t; }
  }
}

u32 kvar_collect_from_dag(Term root, u32 *out_ids, u32 cap) {
  u32 n = 0;
  if (out_ids == NULL || cap == 0) return 0;
  kvar_scan_rec(root, out_ids, &n, cap);
  kvar_sort_ascending(out_ids, n);
  return n;
}

