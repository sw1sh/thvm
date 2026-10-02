// === Rule-set journal ===================================================
//
// Every change to the live rule set R, in order: a slot appended
// (atp_push_rule), a slot's content stamped with the trace record that
// states it (its birth ORIENT / FVI record, or an in-place RHS compose),
// a rule leaving R (slot compaction or soft-delete), and the new-rule
// slice an interreduction pass reduces with.  Each ORIENT / FVI /
// SIMPLIFY record also logs the journal position of the rule set its
// pair was computed on: where it was pushed, or -- for the records whose
// own rules enter R between the normalization and the push (the pop's
// orient_and_add, the Otter adoption) -- where that normalization ran
// (AtpState.rj_hold).
//
// The trace alone cannot give that rule set: a retired Otter passive, a
// joined interreduction victim, a subsumed or ground-joinable equation
// leaves no record, and an in-place compose moves a slot's content to a
// newer record without moving the slot.  thvm_atp_rederive rebuilds the
// exact rule set, slot order and orientation each record saw from here.
//
// Always on, and the same with or without --proof: nothing in the search
// reads it and every event costs the same stores in both modes.  A
// record past the default trace cap exists only in a proof run; its
// view goes to the side array rj_x inside the push's work-clock
// exclusion window, its bytes counted in trace_x_bytes, so the main
// journal grows identically in both runs.

static void atp_rj_push(AtpState *s, u32 kind, u32 uid, u8 orient, u32 tid) {
  if (s->rj_n == s->rj_cap) {
    if (s->rj_lost) return;
    u32 cap = s->rj_cap ? s->rj_cap * 2u : 4096u;
    AtpRjEv *n = (AtpRjEv *)realloc(s->rj, (size_t)cap * sizeof(AtpRjEv));
    if (n == NULL) {
      s->rj_lost = 1u;
      return;
    }
    s->rj = n;
    s->rj_cap = cap;
  }
  AtpRjEv *e = &s->rj[s->rj_n++];
  e->w   = (kind << 29) | ((u32)(orient & 1u) << 28) | (uid & ATP_RJ_UID_MASK);
  e->tid = tid;
  e->nt  = s->n_trace;
}

// Slot `slot` was just appended (its uid, orientation and content set).
static void atp_rj_birth(AtpState *s, u32 slot) {
  atp_rj_push(s, ATP_RJ_BIRTH, s->r_uid[slot], s->r_orient[slot],
              ATP_TRACE_NONE);
}

// Slot `slot` now holds the pair of trace record r_trace[slot].
static void atp_rj_content(AtpState *s, u32 slot) {
  atp_rj_push(s, ATP_RJ_CONTENT, s->r_uid[slot], s->r_orient[slot],
              s->r_trace[slot]);
}

// The rule in slot `slot` leaves R (called before the slot is vacated).
static void atp_rj_death(AtpState *s, u32 slot) {
  atp_hx_rule_death(s, slot);
  atp_rj_push(s, ATP_RJ_DEATH, s->r_uid[slot], 0u, ATP_TRACE_NONE);
}

// An interreduction pass reduces with the n rules from slot `first`
// (consecutive uids); n == 0 ends the pass.
static void atp_rj_slice(AtpState *s, u32 first, u32 n) {
  atp_rj_push(s, ATP_RJ_SLICE, n ? s->r_uid[first] : 0u, 0u, n);
}

// Record `t` (ORIENT / FVI / SIMPLIFY) was just pushed.
static void atp_rj_view(AtpState *s, u32 t) {
  u32 pos = s->rj_hold ? s->rj_hold - 1u : s->rj_n;
  if (t < s->t_vis) {
    atp_rj_push(s, ATP_RJ_VIEW, 0u, 0u, pos);
    return;
  }
  if (s->rj_x_n + 2u > s->rj_x_cap) {
    u32 cap = s->rj_x_cap ? s->rj_x_cap * 2u : 4096u;
    u32 *n = (u32 *)realloc(s->rj_x, (size_t)cap * sizeof(u32));
    if (n == NULL) return;   // that record just re-derives on its push position
    s->trace_x_bytes += (u64)(cap - s->rj_x_cap) * sizeof(u32);
    s->rj_x = n;
    s->rj_x_cap = cap;
  }
  s->rj_x[s->rj_x_n++] = t;
  s->rj_x[s->rj_x_n++] = pos;
}

// Journal position of the rule set when record `t` was pushed: every
// event logged before the push (n_trace <= t then; n_trace is monotone
// while the search runs).
static u32 atp_rj_pos_at_push(const AtpState *s, u32 t) {
  u32 lo = 0u, hi = s->rj_n;
  while (lo < hi) {
    u32 mid = lo + (hi - lo) / 2u;
    if (s->rj[mid].nt <= t) lo = mid + 1u; else hi = mid;
  }
  return lo;
}

// THVM_ATP_RJ_CHECK=1: after every step, replay the new journal events
// into a shadow of R and compare it with the live slots (uid order,
// content record, orientation, soft-deleted slots dead); the first
// mismatch is reported once.  =2 aborts on it.
static int atp_rj_check_mode(void) {
  static int m = -1;
  if (m < 0) {
    const char *e = getenv("THVM_ATP_RJ_CHECK");
    m = (e == NULL || e[0] == '\0' || e[0] == '0') ? 0 : (e[0] == '2' ? 2 : 1);
  }
  return m;
}

static struct {
  const AtpState *owner;
  u32  done;
  u32  cap;
  u32  live;
  u8  *alive;
  u8  *orient;
  u32 *tid;
  u8   reported;
} g_rj_sh;

static void atp_rj_verify(AtpState *s) {
  if (g_rj_sh.owner != s) {
    free(g_rj_sh.alive); free(g_rj_sh.orient); free(g_rj_sh.tid);
    memset(&g_rj_sh, 0, sizeof g_rj_sh);
    g_rj_sh.owner = s;
  }
  if (g_rj_sh.reported) return;
  const char *why = NULL;
  u32 bad_slot = ATP_RULE_NONE, bad_uid = 0u;
  if (s->rj_lost) why = "journal lost (allocation failed)";
  for (u32 k = g_rj_sh.done; k < s->rj_n && why == NULL; k++) {
    u32 w = s->rj[k].w, kind = w >> 29, uid = w & ATP_RJ_UID_MASK;
    if (kind == ATP_RJ_VIEW || kind == ATP_RJ_SLICE) continue;
    if (uid >= g_rj_sh.cap) {
      u32 cap = g_rj_sh.cap ? g_rj_sh.cap : 1024u;
      while (cap <= uid) cap *= 2u;
      g_rj_sh.alive  = (u8 *)realloc(g_rj_sh.alive, cap);
      g_rj_sh.orient = (u8 *)realloc(g_rj_sh.orient, cap);
      g_rj_sh.tid    = (u32 *)realloc(g_rj_sh.tid, (size_t)cap * sizeof(u32));
      if (g_rj_sh.alive == NULL || g_rj_sh.orient == NULL || g_rj_sh.tid == NULL) {
        why = "shadow allocation failed";
        break;
      }
      for (u32 u = g_rj_sh.cap; u < cap; u++) {
        g_rj_sh.alive[u] = 0u; g_rj_sh.orient[u] = 0u; g_rj_sh.tid[u] = ATP_TRACE_NONE;
      }
      g_rj_sh.cap = cap;
    }
    u8 orient = (u8)((w >> 28) & 1u);
    if (kind == ATP_RJ_BIRTH) {
      if (g_rj_sh.alive[uid]) { why = "birth of a live uid"; bad_uid = uid; break; }
      g_rj_sh.alive[uid] = 1u;
      g_rj_sh.orient[uid] = orient;
      g_rj_sh.tid[uid] = ATP_TRACE_NONE;
      g_rj_sh.live++;
    } else if (kind == ATP_RJ_CONTENT) {
      if (!g_rj_sh.alive[uid]) { why = "content of a dead uid"; bad_uid = uid; break; }
      g_rj_sh.orient[uid] = orient;
      g_rj_sh.tid[uid] = s->rj[k].tid;
    } else if (kind == ATP_RJ_DEATH) {
      if (!g_rj_sh.alive[uid]) { why = "death of a dead uid"; bad_uid = uid; break; }
      g_rj_sh.alive[uid] = 0u;
      g_rj_sh.live--;
    }
  }
  if (why == NULL) g_rj_sh.done = s->rj_n;
  u32 live = 0u, prev = 0u;
  for (u32 i = 0; i < s->n_rules && why == NULL; i++) {
    u32 uid = s->r_uid[i];
    u8 alive = (uid < g_rj_sh.cap) ? g_rj_sh.alive[uid] : 0u;
    if (s->r_dead[i]) {
      if (alive) { why = "soft-deleted slot alive in the journal"; bad_slot = i; bad_uid = uid; }
      continue;
    }
    if (!alive) { why = "live slot dead in the journal"; bad_slot = i; bad_uid = uid; break; }
    if (uid <= prev) { why = "live slots out of uid order"; bad_slot = i; bad_uid = uid; break; }
    if (g_rj_sh.tid[uid] != s->r_trace[i]) {
      why = "content record differs"; bad_slot = i; bad_uid = uid; break;
    }
    if (g_rj_sh.orient[uid] != s->r_orient[i]) {
      why = "orientation differs"; bad_slot = i; bad_uid = uid; break;
    }
    prev = uid;
    live++;
  }
  if (why == NULL && live != g_rj_sh.live) why = "live rule count differs";
  if (why == NULL) return;
  g_rj_sh.reported = 1u;
  fprintf(stderr, "[rj-check] step=%u n_rules=%u journal=%u: %s (slot %u uid %u "
          "journal-tid %u slot-tid %u live %u/%u)\n",
          s->step, s->n_rules, s->rj_n, why, bad_slot, bad_uid,
          (bad_uid < g_rj_sh.cap) ? g_rj_sh.tid[bad_uid] : ATP_TRACE_NONE,
          (bad_slot < s->n_rules) ? s->r_trace[bad_slot] : ATP_TRACE_NONE,
          live, g_rj_sh.live);
  if (atp_rj_check_mode() == 2) abort();
}
