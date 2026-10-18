// ext = name, the index into DEFS; no heap cells.
fn Term term_new_ref(u32 name) {
  return term_new(0, TAG_REF, name, 0);
}
