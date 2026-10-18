// An AloState entry maps a template binder's book loc to its realized dyn loc;
// each realized LAM pushes one, ALO-VAR takes the newest match; 0 is empty.

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
