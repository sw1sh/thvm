// thvm_atp_* - saturation loop state (stage 5.1).
//
// Heap-allocated AtpState plus init / free / add_equation / set_goal
// helpers.  The actual saturation step (thvm_atp_step) lands in 5.2;
// the priority-aware CP selection in 5.3; the recursive-descent
// rewriter feeding step 4 of the algorithm in 5.4.  This file just
// gives the loop a place to live.
//
// See docs/plans/saturation_loop.md for the algorithm.

fn AtpState *thvm_atp_init(const KboConfig *cfg, u32 step_cap) {
  AtpState *s = (AtpState *)calloc(1, sizeof(AtpState));
  if (s == NULL) return NULL;
  s->kbo      = cfg;
  s->step_cap = step_cap;
  return s;
}

fn void thvm_atp_free(AtpState *s) {
  if (s == NULL) return;
  free(s);
}

// Push an axiom / pending equation onto the CP queue.  The
// saturation loop's orient + generate machinery processes it
// uniformly with later-derived CPs.  Returns 1 on success, 0 if the
// queue is full.
fn u8 thvm_atp_add_equation(AtpState *s, Term lhs, Term rhs) {
  if (s == NULL || s->n_cps >= ATP_MAX_CPS) return 0;
  s->cp_lhs[s->n_cps] = lhs;
  s->cp_rhs[s->n_cps] = rhs;
  s->n_cps++;
  return 1;
}

// Set the conjecture (single equation goal_lhs == goal_rhs).
// Calling with goal_lhs == 0 clears the goal (completion mode).
fn void thvm_atp_set_goal(AtpState *s, Term lhs, Term rhs) {
  if (s == NULL) return;
  s->goal_lhs = lhs;
  s->goal_rhs = rhs;
}

// Pop the front CP (FIFO).  Shift the rest of the queue down by
// one slot to keep the array dense.  Returns 1 on success, 0 if
// the queue was empty.  Stage 5.3 will replace this with a
// priority-collapse selection over INC-wrapped CPs.
fn u8 thvm_atp_select_cp(AtpState *s, Term *lhs_out, Term *rhs_out) {
  if (s == NULL || s->n_cps == 0) return 0;
  *lhs_out = s->cp_lhs[0];
  *rhs_out = s->cp_rhs[0];
  for (u32 i = 1; i < s->n_cps; i++) {
    s->cp_lhs[i - 1] = s->cp_lhs[i];
    s->cp_rhs[i - 1] = s->cp_rhs[i];
  }
  s->n_cps--;
  return 1;
}
