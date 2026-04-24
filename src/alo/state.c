// alo/state.c - substitution-chain helpers used by alo_realize.
//
// An AloState entry binds a book-heap loc (the address of a binder
// in the static template) to a freshly allocated dynamic-heap loc
// (the same binder in the realised copy).  Each ALO descent into a
// LAM appends a new entry; ALO-VAR walks the chain looking for the
// latest binding for the var's loc.
//
// State id 0 = empty chain.

u32 alo_state_push(u32 parent, u64 old_loc, u64 new_loc) {
  if (ALO_STATES_NEXT >= ALO_STATE_CAP) {
    fprintf(stderr, "alo_state_push: substitution chain overflow\n");
    exit(1);
  }
  u32 id = ALO_STATES_NEXT++;
  ALO_STATES[id].parent  = parent;
  ALO_STATES[id].old_loc = old_loc;
  ALO_STATES[id].new_loc = new_loc;
  return id;
}

int alo_state_lookup(u32 state_id, u64 old_loc, u64 *out_new_loc) {
  for (u32 cur = state_id; cur != 0; cur = ALO_STATES[cur].parent) {
    if (ALO_STATES[cur].old_loc == old_loc) {
      *out_new_loc = ALO_STATES[cur].new_loc;
      return 1;
    }
  }
  return 0;
}
