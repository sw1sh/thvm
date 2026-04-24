// term/new_ref.c - construct a TAG_REF pointing at DEFS[name].

fn Term term_new_ref(u32 name) {
  return term_new(0, TAG_REF, name, 0);
}
