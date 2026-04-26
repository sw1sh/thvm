// term/new_pri.c - construct a TAG_PRI primitive function reference.
//
// 8.1b: bridges the IC reducer to native C functions.  A PRI carries
// a `prim_id` (u32, in EXT) into a process-global registry mapping
// id -> (PrimFn, arity).  A freshly-constructed PRI is "0-args"
// (val = 0, no heap); APP-PRI in `src/interact/app_pri.c` accumulates
// args into a heap cell and fires the function once arity is reached.
//
// This is the IC-side mechanism stage 8.1c uses to call `thvm_unify`
// from inside SUP-encoded CP enumeration.

fn Term term_new_pri(u32 prim_id) {
  return term_new(0, TAG_PRI, prim_id, 0);
}

// Process-global registry.  Indexed by prim_id; PRIM_TABLE_CAP
// (declared in thvm.h) bounds the total number of primitives.
typedef struct {
  PrimFn func;
  u32    arity;
} PrimDef;

static PrimDef PRIM_TABLE[PRIM_TABLE_CAP];

fn u32 prim_register(u32 prim_id, PrimFn func, u32 arity) {
  if (prim_id >= PRIM_TABLE_CAP) return 0;
  PRIM_TABLE[prim_id].func  = func;
  PRIM_TABLE[prim_id].arity = arity;
  return prim_id;
}

fn PrimFn prim_fun(u32 prim_id) {
  if (prim_id >= PRIM_TABLE_CAP) return NULL;
  return PRIM_TABLE[prim_id].func;
}

fn u32 prim_arity(u32 prim_id) {
  if (prim_id >= PRIM_TABLE_CAP) return 0;
  return PRIM_TABLE[prim_id].arity;
}
