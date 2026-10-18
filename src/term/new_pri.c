// A native C function: ext = prim_id into PRIM_TABLE. A fresh PRI holds no args
// (val 0); APP-PRI (interact/app_pri.c) gathers them until the arity is met.
fn Term term_new_pri(u32 prim_id) {
  return term_new(0, TAG_PRI, prim_id, 0);
}

// One table for the process, shared by every context.
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
