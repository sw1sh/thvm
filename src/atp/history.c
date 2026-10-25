// === Derivation-history export (learned clause selection) ============
//
// Opt-in, observation-only stream of the completion's derivation
// history for offline training of learned CP selection (see
// docs/ATP/atp_neural_selection.md and docs/ATP/enigma_portfolio.md
// section 3.2).  Armed by THVM_ATP_HISTORY_EXPORT=<path>; with the env
// unset every hook is one predictable branch and the engine is
// byte-identical.  The hooks only READ engine state: no IC heap
// allocation, no flatterm arena traffic, no cache traffic, no RNG -- the
// recorded run follows the unrecorded trajectory exactly.  Their feature
// walks and writes run inside work-clock exclusion windows
// (atp_work_pause / atp_work_resume), so under the CPU and instruction
// clocks the clock-gated disciplines (CP-set interreduction sweeps, LRS)
// fire at about the same point of the search as in the unrecorded run --
// about, because the hook's test, the call and the window's own framing
// still count.  Exact identity is tested under the logical step clock
// (THVM_ATP_WORK_CLOCK=steps, rerank.c), which no hook can advance:
// tests/atp_history_v2_obs.sh (gate G2.1) runs there.
//
// Format: fixed 64-byte little-endian records, appended to <path>.  One
// exporting process per file at a time: sessions are delimited by their
// 'B' record, so two processes appending to one path at once interleave
// sessions.  A --schedule run therefore gives each slice its own file
// (<path>.<slice>, tools/atp_cli/main.c); fof-split branches run one
// after another and append to their lane's file, tagged in 'B'.
// Version 2 keeps every version-1 record and adds the lane tag, the
// symbol table, rule content, rule deaths and the Otter replay.  Record
// tags (AtpHxRec layout unless named otherwise):
//   'B' session begin   (AtpHxHdrRec) seq = session, pri = sample
//                       threshold (u32 hash bound; a CP is sampled iff
//                       hx_hash(seq) < pri), kind = format version (0
//                       in version 1), lane = FINDPROOF_SLICE (the
//                       schedule slice, or --lane NAME), lane_len its
//                       length, flags bit 0 = lane truncated
//   'Y' symbol          (AtpHxSymRec) label -> name, arity; written by
//                       the driver after init (thvm_atp_history_symbol);
//                       flags bit 0 = name truncated, bit 1 = fresh
//                       (introduced by an input transform), bit 2 =
//                       arity saturated at 255
//   'P' push (SAMPLED CPs only, uniform by seq hash); the last 'P'
//                       per seq wins (the WM intake re-stamps axioms)
//   'S' selection       every non-orphan pop; aux = cp_seq_next at the
//                       pick, so push step of seq q = #{S : aux <= q};
//                       kind = cp_kind | pick << 4, pick = the selection
//                       dimension that chose it (ATP_HX_PICK_*; 0 in
//                       version 1, where it was not recorded)
//   'X' pop outcome     kind = ATP_HX_OUT_*, for the last 'S';
//                       aux = live queue after the pop, pa = trace size
//                       at that 'S', pb = trace size now (an 'R'
//                       entry's step is the step of the last 'X' whose
//                       pa <= its id: derived offline, np0_extract.py)
//   'W' reweight        queued CP re-scored in place (SAMPLED CPs
//                       only): pri = new cp_pri, aux = old, kind = 1
//                       when the content was repacked, feat = content
//   'D' deletion        queued CP removed without selection (SAMPLED
//                       CPs only); kind = ATP_HX_DEL_*
//   'K' / 'k'           parent death marks: rule uid / rule trace id
//                       (queued CPs of a dead parent are orphans)
//   'Q' rule content    a rule entered R or its content changed in
//                       place: kind = ATP_HX_RULE_*, seq = the rule's
//                       trace id, trace = uid, pri = the CP seq it came
//                       from (ATP_TRACE_NONE when none), uid_a = slot,
//                       uid_b = n_rules, depth = formation depth, pa /
//                       pb = encoded lhs / rhs byte lengths, aux =
//                       FNV-1a of the encoding, feat = the pair's
//                       structural features, flags = ATP_HX_Q_* |
//                       ATP_HX_F_FEAT / _GD / _GOALSYM, pos_len = number
//                       of 'q' records that follow (0 when the content
//                       is absent or truncated)
//   'q' continuation    (AtpHxContRec) index, then 62 bytes of the
//                       Q record's encoding
//   'Z' rule death      a rule leaves R (every path the rule journal
//                       logs): seq = trace id, trace = uid, uid_a =
//                       slot, uid_b = n_rules, depth = formation depth,
//                       flags = ATP_HX_Q_ORIENTED / _PASSIVE
//   'O' Otter replay    (--otter) kind = ATP_HX_OT_*, seq = CP seq,
//                       trace = passive rule uid, pa = its trace id,
//                       pri / depth = the CP's priority / depth when
//                       queued, aux = n_rules; flags bit 0 (retirements)
//                       = the passive rule was already gone.  Adoption,
//                       drop and retirement records are always written;
//                       the other outcomes for SAMPLED CPs only
//   'R' trace entry     every non-TRACE_CP entry at session end:
//                       seq = trace id, kind = reason, trace = child 0
//                       (parent / chain predecessor), pa = child 1
//                       (NORM_STEP: the rewriting rule's trace id),
//                       step = 0xffffffff (see 'X'), feat = the equation's
//                       features when its pair is materialized, flags
//                       bit 0 = pushed after the search ended (proof
//                       emission), which a search-time model never sees
//   'E' session end     kind = AtpStatus (0xff unknown), seq = n_trace,
//                       step = selections, trace = CPs pushed, pa =
//                       'P' records written, pb = flag bits (1 orphan
//                       murder, 2 uid orphans, 4 recorded normalization
//                       steps, 8 lean CP trace, 16 Otter loop, 32 LRS),
//                       pri = n_rules, uid_a = live queue, uid_b = 'S'
//                       records written, depth = trace cap, aux = trace
//                       size at the search's end
//   'F' push features   (THVM_ATP_HISTORY_CPFEAT=1 only) follows its
//                       'P': seq, then the engine's 14 CP features
//                       (AtpHxFeatRec); the last 'F' per seq wins
// 'P'/'S' carry: kind = cp_kind (TRACE_AXIOM/CP/SIMPLIFY), flags, pos_len
// (overlap position depth, 0xff = none), seq, step (selections so far),
// trace (the CP's trace id), pa / pb (parent rule trace ids: pa is the
// OUTER rule whose lhs subterm the inner rule pb rewrites), pri (cp_pri),
// uid_a / uid_b (parent uids), depth (formation depth), aux, feat.
//
// Rule content ('Q' + 'q') is read off the rule's flatterm mirror
// (lhs_ft / rhs_ft), never decoded into IC heap Terms: FindProof runs
// without a collector.  Encoding: the lhs cells in preorder, then the rhs
// cells; a variable is varint(id << 1 | 1), a function symbol or constant
// is varint(label << 1) followed by varint(arity).  The mirror holds the
// stored orientation with the rule's own variable ids (dense only as far
// as atp_push_rule's normalization makes them; an in-place compose keeps
// them), so neither the encoding nor its FNV in aux is a canonical id.
// The cross-lane key comes from the reader: it renames variables by first
// occurrence, renders both orientations with symbol names from the 'Y'
// records and takes the smaller (tools/atp_enigma/np0_extract.py).
//
// Coverage: every CP queue insert, pop and removal, every rule birth,
// in-place compose and death, and the Otter replay, in every lane family
// that runs the completion loop.  Searches outside the CP queue and R
// leave no records: the critical-goal queue and the greedy walk of the
// narrowing lanes (narrow.c), whose proofs cite the rules they narrowed
// with, and the CLI front ends (Horn release, horn-cd chaining), which
// run before the engine exists.
//
// Features are read off the queue's packed byte string (acp_pack
// preorder), the structural subset of thvm_atp_cp_features: the
// remaining columns there (KBO orientability, three weight modes) need
// heap Terms that the FT-routed push never materializes, and building
// them here would allocate on the IC heap -- the engine's own weight is
// recorded instead (pri).  The 'F' records are the exception, opted
// into separately: they decode the slot's bytes into transient heap
// Terms and run thvm_atp_cp_features on them with age = the CP's seq,
// the vector a push-time learned scorer computes.  The decode is the
// only IC heap allocation the export makes.  Without an armed collector
// (FindProof runs with none) those cells are never reclaimed, so a long
// export can exhaust the heap (BOO078-1 at 56 s of a 60 s twee run).

#include <stddef.h>

#define ATP_HX_REC_SIZE   64u
#define ATP_HX_NFEAT      10u
#define ATP_HX_VERSION    2u
#define ATP_HX_LANE_MAX   36u
#define ATP_HX_NAME_MAX   56u
#define ATP_HX_CONT_BYTES 62u
#define ATP_HX_CONT_MAX   255u

// Pop outcomes ('X' kind).
#define ATP_HX_OUT_KEPT     1u   // oriented into R/E
#define ATP_HX_OUT_JOINED   2u   // selection-time normalize joined
#define ATP_HX_OUT_SJOINED  3u   // simpler-join joined
#define ATP_HX_OUT_PERMSUB  4u   // AC-permutation subsumed
#define ATP_HX_OUT_SUBSUMED 5u   // subsumed by an existing equation
#define ATP_HX_OUT_POPGJ    6u   // pop-time ground-joinable
#define ATP_HX_OUT_REFUSED  7u   // orient refused (duplicate)
#define ATP_HX_OUT_LAZYGJ   8u   // lazy ground-join drop before normalize

// Deletion reasons ('D' kind).
#define ATP_HX_DEL_ORPHAN_POP   1u   // orphan discarded at pop
#define ATP_HX_DEL_ORPHAN_SWEEP 2u   // eager orphan sweep
#define ATP_HX_DEL_IR_ORPHAN    3u   // CP-set interreduction: orphan
#define ATP_HX_DEL_IR_SUBSUMED  4u   // CP-set interreduction: rule-subsumed
#define ATP_HX_DEL_IR_JOINED    5u   // CP-set interreduction: joinable
#define ATP_HX_DEL_LRS          6u   // limited-resource-strategy prune

// 'P'/'S' flag bits.
#define ATP_HX_F_ULTIMATE 0x01u
#define ATP_HX_F_RAW      0x02u   // queued raw (WM KPBehandelt >= 50 class)
#define ATP_HX_F_DEPRIO   0x04u
#define ATP_HX_F_FEAT     0x08u   // feat[] valid
#define ATP_HX_F_GD       0x10u   // goal-derived
#define ATP_HX_F_GOALSYM  0x20u   // goal symbol set known (feat[7] valid)
#define ATP_HX_F_IMPLICIT 0x40u   // deferred descriptor slot
#define ATP_HX_F_RESTAMP  0x80u   // re-emitted after the WM intake permutation

// Selection dimension ('S' kind >> 4).
#define ATP_HX_PICK_WEIGHT 0u   // the weight key's minimum (heap root)
#define ATP_HX_PICK_AGE    1u   // FIFO / age dimension (--awr, WM interleave)
#define ATP_HX_PICK_W2     2u   // secondary weight dimension (w2_modulo)
#define ATP_HX_PICK_GOAL   3u   // goal-interleave pick
#define ATP_HX_PICK_RANDOM 4u   // random pick
#define ATP_HX_PICK_KDH    5u   // KD-heap / WM itl age dimension
#define ATP_HX_PICK_TOPK   6u   // top-K re-ranker's pick in the weight dimension
#define ATP_HX_PICK_GUIDE  7u   // guided twin's pick in the weight dimension (src/atp/guide.c)

// Rule content events ('Q' kind).
#define ATP_HX_RULE_BORN    1u   // a selected CP oriented into R/E
#define ATP_HX_RULE_FVI     2u   // free-variable instance born with it
#define ATP_HX_RULE_COMPOSE 3u   // right-hand side rewritten in place
#define ATP_HX_RULE_PASSIVE 4u   // Otter passive demodulator adopted
#define ATP_HX_RULE_INSTALL 5u   // pre-oriented axiom installed

// 'Q' / 'Z' flag bits (0x08, 0x10, 0x20 as ATP_HX_F_FEAT / _GD / _GOALSYM).
#define ATP_HX_Q_ORIENTED  0x01u
#define ATP_HX_Q_TRUNC     0x02u   // encoding over the continuation cap: omitted
#define ATP_HX_Q_NOCONTENT 0x04u   // no flatterm mirror to read
#define ATP_HX_Q_PASSIVE   0x40u   // an Otter passive demodulator

// Otter replay outcomes ('O' kind).
#define ATP_HX_OT_ADOPT      1u    // adopted as a passive demodulator
#define ATP_HX_OT_JOINED     2u    // normalized to an identity: no demodulator
#define ATP_HX_OT_UNORIENT   3u    // unorientable: no demodulator
#define ATP_HX_OT_REFUSED    4u    // the rule is already in R
#define ATP_HX_OT_LRS        5u    // beyond the LRS horizon: not adopted
#define ATP_HX_OT_DEFERRED   6u    // descriptor slot: never adopted
#define ATP_HX_OT_GONE       7u    // left the queue before its replay
#define ATP_HX_OT_DROP       8u    // adopted rule not oriented: dropped at once
#define ATP_HX_OT_RETIRE_SEL 9u    // its CP was selected: rule retired
#define ATP_HX_OT_RETIRE_LRS 10u   // its CP was LRS-pruned: rule retired

typedef struct {
  u8  tag;
  u8  kind;
  u8  flags;
  u8  pos_len;
  u32 seq;
  u32 step;
  u32 trace;
  u32 pa;
  u32 pb;
  u32 pri;
  u32 uid_a;
  u32 uid_b;
  u32 depth;
  u32 aux;
  u16 feat[ATP_HX_NFEAT];
} AtpHxRec;
_Static_assert(sizeof(AtpHxRec) == ATP_HX_REC_SIZE, "AtpHxRec must stay 64 bytes");

typedef struct {
  u8    tag;
  u8    kind;
  u8    flags;
  u8    pad;
  u32   seq;
  float feat[ATP_CP_FEATURE_DIM];
} AtpHxFeatRec;
_Static_assert(sizeof(AtpHxFeatRec) == ATP_HX_REC_SIZE, "AtpHxFeatRec must stay 64 bytes");

// 'B': seq and pri sit where AtpHxRec has them, so a version-1 reader
// still finds the session and the sample threshold.
typedef struct {
  u8   tag;
  u8   kind;
  u8   flags;
  u8   lane_len;
  u32  seq;
  u32  step;
  u32  trace;
  u32  pa;
  u32  pb;
  u32  pri;
  char lane[ATP_HX_LANE_MAX];
} AtpHxHdrRec;
_Static_assert(sizeof(AtpHxHdrRec) == ATP_HX_REC_SIZE, "AtpHxHdrRec must stay 64 bytes");
_Static_assert(offsetof(AtpHxHdrRec, seq) == offsetof(AtpHxRec, seq), "B seq");
_Static_assert(offsetof(AtpHxHdrRec, pri) == offsetof(AtpHxRec, pri), "B pri");

typedef struct {
  u8   tag;
  u8   arity;
  u8   flags;
  u8   name_len;
  u32  label;
  char name[ATP_HX_NAME_MAX];
} AtpHxSymRec;
_Static_assert(sizeof(AtpHxSymRec) == ATP_HX_REC_SIZE, "AtpHxSymRec must stay 64 bytes");

typedef struct {
  u8 tag;
  u8 index;
  u8 bytes[ATP_HX_CONT_BYTES];
} AtpHxContRec;
_Static_assert(sizeof(AtpHxContRec) == ATP_HX_REC_SIZE, "AtpHxContRec must stay 64 bytes");

typedef struct {
  FILE           *fp;
  const AtpState *s;
  u32   thresh;
  u32   session;
  u8   *gsym;           // goal symbol membership by label
  u32   gsym_cap;
  u8    gsym_ready;
  u8    raw_pending;    // raw_untreated flag for the next heap insert
  u8    repacked;       // next reweight follows an in-place repack
  u8    cpfeat;         // THVM_ATP_HISTORY_CPFEAT: 'F' records
  u32   last_sel_seq;
  u32   sel_ntrace;     // trace size at the last 'S'
  u32   n_trace_search;   // trace size when the search ended (0 = unmarked)
  u64   n_rec_push;
  u64   n_rec_sel;
  u8   *enc;            // rule-content encoding scratch
  u32   enc_cap;
  u32  *open;           // rule-content walk: arguments left per open cell
  u32   open_cap;
} AtpHx;

static AtpHx g_hx;

// Defined in state.c / cp_store.c (included after this file).
static int atp_uid_orphan_on(void);
static int atp_env_on(const char *name);
static void acp_unpack(const u8 *buf, Term *lhs, Term *rhs);
static inline u8 atp_otter_passive(const AtpState *s, u32 i);

static inline u8 atp_hx_on(const AtpState *s) {
  return g_hx.fp != NULL && g_hx.s == s;
}

// Uniform CP sampling by seq: a fixed integer mix (lowbias32), so the
// offline reader recomputes membership from the recorded threshold.
static inline u32 atp_hx_hash(u32 x) {
  x ^= x >> 16;
  x *= 0x7feb352du;
  x ^= x >> 15;
  x *= 0x846ca68bu;
  x ^= x >> 16;
  return x;
}

static inline u8 atp_hx_sampled(u32 seq) {
  return g_hx.thresh == 0xffffffffu || atp_hx_hash(seq) < g_hx.thresh;
}

static void atp_hx_write(const AtpHxRec *r) {
  fwrite(r, ATP_HX_REC_SIZE, 1u, g_hx.fp);
}

static void atp_hx_rec_init(AtpHxRec *r, u8 tag, const AtpState *s) {
  memset(r, 0, sizeof *r);
  r->tag     = tag;
  r->pos_len = 0xffu;
  r->step    = s->cp_select_count;
  r->trace   = ATP_TRACE_NONE;
  r->pa      = ATP_TRACE_NONE;
  r->pb      = ATP_TRACE_NONE;
}

// --- structural features --------------------------------------------

typedef struct {
  u32 nodes[2];
  u32 depth[2];
  u32 var_occ;
  u32 n_vars;
  u32 var_id[64];
  u32 var_cnt[64];
  u8  var_side[64];
  u32 n_syms;
  u32 sym[128];
  u32 goal_occ;
  u8  top_var[2];
  u32 top_sym[2];
  u8  overflow;
} AtpHxFeat;

static void atp_hx_feat_var(AtpHxFeat *f, u32 id, u32 side) {
  f->var_occ++;
  for (u32 k = 0; k < f->n_vars; k++) {
    if (f->var_id[k] == id) {
      f->var_cnt[k]++;
      f->var_side[k] |= (u8)(1u << side);
      return;
    }
  }
  if (f->n_vars < 64u) {
    f->var_id[f->n_vars]   = id;
    f->var_cnt[f->n_vars]  = 1u;
    f->var_side[f->n_vars] = (u8)(1u << side);
    f->n_vars++;
  } else {
    f->overflow = 1u;
  }
}

static void atp_hx_feat_sym(AtpHxFeat *f, u32 label) {
  if (g_hx.gsym != NULL && label < g_hx.gsym_cap && g_hx.gsym[label]) {
    f->goal_occ++;
  }
  for (u32 k = 0; k < f->n_syms; k++) {
    if (f->sym[k] == label) {
      return;
    }
  }
  if (f->n_syms < 128u) {
    f->sym[f->n_syms++] = label;
  } else {
    f->overflow = 1u;
  }
}

static inline u64 atp_hx_varint(const u8 **pp) {
  u64 v = 0u;
  u32 shift = 0u;
  u8  b;
  do {
    b = *(*pp)++;
    v |= (u64)(b & 0x7fu) << shift;
    shift += 7u;
  } while (b & 0x80u);
  return v;
}

// One packed preorder term (acp_pack_term layout); returns its depth.
static u32 atp_hx_feat_bytes(AtpHxFeat *f, const u8 **pp, u32 side, u8 top) {
  // Open CTR records on an explicit stack: `left` children still to
  // read, `d` the deepest child depth so far.  Records are read and
  // counted in preorder, as the recursion did.
  typedef struct { u32 left; u32 d; } HxFrame;
  enum { HX_INL = 64 };
  HxFrame  fr_inl[HX_INL];
  HxFrame *fr  = fr_inl;
  u32      nfr = 0u;
  u32      cfr = HX_INL;
  u32      v   = 1u;
  for (;;) {
    f->nodes[side]++;
    u8 leaf = 1u;
    u8 disc = *(*pp)++;
    switch (disc) {
      case 'C': {
        u32 label = (u32)atp_hx_varint(pp);
        u32 n     = (u32)atp_hx_varint(pp);
        atp_hx_feat_sym(f, label);
        if (top) {
          f->top_sym[side] = label;
        }
        if (n > 0u) {
          if (nfr == cfr) {
            fr = (HxFrame *)term_stack_grow(fr, fr_inl, &cfr, sizeof *fr);
          }
          fr[nfr].left = n;
          fr[nfr].d    = 0u;
          nfr++;
          leaf = 0u;
        }
        break;
      }
      case 'N': {
        (void)atp_hx_varint(pp);
        (void)atp_hx_varint(pp);
        break;
      }
      case 'V': {
        u32 id = (u32)atp_hx_varint(pp);
        atp_hx_feat_var(f, id, side);
        if (top) {
          f->top_var[side] = 1u;
        }
        break;
      }
      default:
        break;
    }
    top = 0u;
    if (leaf) {
      v = 1u;
      // A finished child's depth folds into its parent; a parent whose
      // last child finished is one deeper than its deepest child.
      while (nfr > 0u) {
        HxFrame *h = &fr[nfr - 1u];
        if (v > h->d) {
          h->d = v;
        }
        if (--h->left > 0u) {
          break;
        }
        v = h->d + 1u;
        nfr--;
      }
      if (nfr == 0u) {
        break;
      }
    }
  }
  term_stack_release(fr, fr_inl);
  return v;
}

static u32 atp_hx_feat_term(AtpHxFeat *f, Term t, u32 side, u8 top) {
  // Open CTRs on an explicit stack (child i at heap_read(base + 1 + i));
  // `d` is the deepest child depth so far.  Preorder visits, as the
  // recursion made.
  typedef struct { u64 base; u32 i; u32 n; u32 d; } HxTermFrame;
  enum { HX_INL = 64 };
  HxTermFrame  fr_inl[HX_INL];
  HxTermFrame *fr  = fr_inl;
  u32          nfr = 0u;
  u32          cfr = HX_INL;
  u32          v   = 1u;
  for (;;) {
    f->nodes[side]++;
    u8 leaf = 1u;
    switch (term_tag(t)) {
      case TAG_CTR: {
        u32 label = term_ext(t);
        u32 n     = term_ctr_n(t);
        atp_hx_feat_sym(f, label);
        if (top) {
          f->top_sym[side] = label;
        }
        if (n > 0u) {
          if (nfr == cfr) {
            fr = (HxTermFrame *)term_stack_grow(fr, fr_inl, &cfr, sizeof *fr);
          }
          fr[nfr].base = term_val(t);
          fr[nfr].i    = 0u;
          fr[nfr].n    = n;
          fr[nfr].d    = 0u;
          nfr++;
          leaf = 0u;
        }
        break;
      }
      case TAG_FVR: {
        atp_hx_feat_var(f, term_ext(t), side);
        if (top) {
          f->top_var[side] = 1u;
        }
        break;
      }
      default:
        break;
    }
    top = 0u;
    if (leaf) {
      v = 1u;
      while (nfr > 0u) {
        HxTermFrame *h = &fr[nfr - 1u];
        if (v > h->d) {
          h->d = v;
        }
        if (++h->i < h->n) {
          break;
        }
        v = h->d + 1u;
        nfr--;
      }
      if (nfr == 0u) {
        break;
      }
    }
    HxTermFrame *h = &fr[nfr - 1u];
    t = heap_read(h->base + 1u + (u64)h->i);
  }
  term_stack_release(fr, fr_inl);
  return v;
}

static inline u16 atp_hx_sat16(u32 v) {
  return (u16)(v > 0xffffu ? 0xffffu : v);
}

// feat[]: 0 lhs nodes, 1 rhs nodes, 2 lhs depth, 3 rhs depth,
// 4 distinct vars, 5 var occurrences, 6 distinct function symbols,
// 7 goal-symbol occurrences, 8 max single-variable multiplicity,
// 9 shape bits (1 lhs top var, 2 rhs top var, 4 same top symbol,
// 8 lhs ground, 16 rhs ground, 32 vars(rhs) in vars(lhs),
// 64 vars(lhs) in vars(rhs), 128 feature overflow).
static void atp_hx_feat_finish(const AtpHxFeat *f, u16 *out) {
  u32 max_mult = 0u;
  u8  r_in_l = 1u, l_in_r = 1u, l_ground = 1u, r_ground = 1u;
  for (u32 k = 0; k < f->n_vars; k++) {
    if (f->var_cnt[k] > max_mult) {
      max_mult = f->var_cnt[k];
    }
    if (f->var_side[k] & 1u) {
      l_ground = 0u;
    }
    if (f->var_side[k] & 2u) {
      r_ground = 0u;
    }
    if ((f->var_side[k] & 2u) && !(f->var_side[k] & 1u)) {
      r_in_l = 0u;
    }
    if ((f->var_side[k] & 1u) && !(f->var_side[k] & 2u)) {
      l_in_r = 0u;
    }
  }
  u32 shape = 0u;
  if (f->top_var[0]) {
    shape |= 1u;
  }
  if (f->top_var[1]) {
    shape |= 2u;
  }
  if (!f->top_var[0] && !f->top_var[1] && f->top_sym[0] == f->top_sym[1]) {
    shape |= 4u;
  }
  if (l_ground) {
    shape |= 8u;
  }
  if (r_ground) {
    shape |= 16u;
  }
  if (r_in_l) {
    shape |= 32u;
  }
  if (l_in_r) {
    shape |= 64u;
  }
  if (f->overflow) {
    shape |= 128u;
  }
  out[0] = atp_hx_sat16(f->nodes[0]);
  out[1] = atp_hx_sat16(f->nodes[1]);
  out[2] = atp_hx_sat16(f->depth[0]);
  out[3] = atp_hx_sat16(f->depth[1]);
  out[4] = atp_hx_sat16(f->n_vars);
  out[5] = atp_hx_sat16(f->var_occ);
  out[6] = atp_hx_sat16(f->n_syms);
  out[7] = atp_hx_sat16(f->goal_occ);
  out[8] = atp_hx_sat16(max_mult);
  out[9] = (u16)shape;
}

static void atp_hx_gsym_mark(Term t) {
  // Pending subterms on an explicit stack, children pushed right to
  // left: the recursion's preorder.
  enum { HX_INL = 64 };
  Term  st_inl[HX_INL];
  Term *st  = st_inl;
  u32   nst = 0u;
  u32   cst = HX_INL;
  for (;;) {
    if (t != 0 && term_tag(t) == TAG_CTR) {
      u32 label = term_ext(t);
      u8  grown = 1u;   // a failed grow skips this subterm, as the recursion did
      if (label < (1u << 20)) {
        if (label >= g_hx.gsym_cap) {
          u32 ncap = g_hx.gsym_cap ? g_hx.gsym_cap : 256u;
          while (ncap <= label) {
            ncap *= 2u;
          }
          u8 *ng = (u8 *)realloc(g_hx.gsym, ncap);
          if (ng == NULL) {
            grown = 0u;
          } else {
            memset(ng + g_hx.gsym_cap, 0, ncap - g_hx.gsym_cap);
            g_hx.gsym = ng;
            g_hx.gsym_cap = ncap;
          }
        }
        if (grown) {
          g_hx.gsym[label] = 1u;
        }
      }
      u32 n = grown ? term_ctr_n(t) : 0u;
      while (nst + n > cst) {
        st = (Term *)term_stack_grow(st, st_inl, &cst, sizeof *st);
      }
      for (u32 i = n; i-- > 0u;) {
        st[nst++] = term_ctr_at(t, i);
      }
    }
    if (nst == 0u) {
      break;
    }
    t = st[--nst];
  }
  term_stack_release(st, st_inl);
}

// The goal is installed after the axioms are queued: build the symbol
// set on the first record that sees it.
static void atp_hx_gsym_ensure(const AtpState *s) {
  if (g_hx.gsym_ready || s->goal_lhs == 0) {
    return;
  }
  atp_hx_gsym_mark(s->goal_lhs);
  atp_hx_gsym_mark(s->goal_rhs);
  for (u32 g = 0; g < s->n_goals; g++) {
    if (s->goals_lhs != NULL) {
      atp_hx_gsym_mark(s->goals_lhs[g]);
    }
    if (s->goals_rhs != NULL) {
      atp_hx_gsym_mark(s->goals_rhs[g]);
    }
  }
  g_hx.gsym_ready = 1u;
}

static void atp_hx_feat_packed(AtpHxRec *r, const u8 *packed) {
  if (packed == NULL) {
    return;
  }
  AtpHxFeat f;
  memset(&f, 0, sizeof f);
  const u8 *p = packed;
  f.depth[0] = atp_hx_feat_bytes(&f, &p, 0u, 1u);
  f.depth[1] = atp_hx_feat_bytes(&f, &p, 1u, 1u);
  atp_hx_feat_finish(&f, r->feat);
  r->flags |= ATP_HX_F_FEAT;
  if (g_hx.gsym_ready) {
    r->flags |= ATP_HX_F_GOALSYM;
  }
}

static u8 atp_hx_feat_pair(AtpHxRec *r, Term lhs, Term rhs) {
  if (lhs == 0 || rhs == 0 || term_tag(lhs) == TAG_NUM ||
      term_tag(rhs) == TAG_NUM) {
    return 0u;
  }
  AtpHxFeat f;
  memset(&f, 0, sizeof f);
  f.depth[0] = atp_hx_feat_term(&f, lhs, 0u, 1u);
  f.depth[1] = atp_hx_feat_term(&f, rhs, 1u, 1u);
  atp_hx_feat_finish(&f, r->feat);
  r->flags |= ATP_HX_F_FEAT;
  if (g_hx.gsym_ready) {
    r->flags |= ATP_HX_F_GOALSYM;
  }
  return 1u;
}

// --- rule content -------------------------------------------------------

// Encoding sink: `len` counts every byte of the encoding; bytes past the
// continuation cap are counted but not stored (the record then omits the
// content and says so).
typedef struct {
  u8  *buf;
  u32  cap;
  u32  len;
  u8   over;
} AtpHxEnc;

static void atp_hx_enc_byte(AtpHxEnc *e, u8 b) {
  u32 at = e->len++;
  if (e->over || at >= ATP_HX_CONT_MAX * ATP_HX_CONT_BYTES) {
    e->over = 1u;
    return;
  }
  if (at >= e->cap) {
    u32 ncap = e->cap ? e->cap * 2u : 256u;
    u8 *nb = (u8 *)realloc(e->buf, ncap);
    if (nb == NULL) {
      e->over = 1u;
      return;
    }
    e->buf = nb;
    e->cap = ncap;
  }
  e->buf[at] = b;
}

static void atp_hx_enc_varint(AtpHxEnc *e, u64 v) {
  while (v >= 0x80u) {
    atp_hx_enc_byte(e, (u8)(v | 0x80u));
    v >>= 7;
  }
  atp_hx_enc_byte(e, (u8)v);
}

// FNV-1a: a content id for integrity checks and exact-duplicate tests,
// not the cross-lane key.
static u32 atp_hx_fnv(const u8 *p, u32 n) {
  u32 h = 2166136261u;
  for (u32 i = 0; i < n; i++) {
    h ^= p[i];
    h *= 16777619u;
  }
  return h;
}

// The export's malloc'd scratch, freed at session end (and by the unit
// tests that encode outside a session).
static void atp_hx_scratch_free(void) {
  free(g_hx.enc);
  free(g_hx.open);
  g_hx.enc      = NULL;
  g_hx.enc_cap  = 0u;
  g_hx.open     = NULL;
  g_hx.open_cap = 0u;
}

#ifdef THVM_ATPFT_RULES
// Open a function cell at walk depth `at` with `arity` arguments to come;
// 0 when the walk stack cannot grow.
static u8 atp_hx_open_push(u32 at, u32 arity) {
  if (at >= g_hx.open_cap) {
    u32 ncap = g_hx.open_cap ? g_hx.open_cap * 2u : 64u;
    u32 *no = (u32 *)realloc(g_hx.open, (size_t)ncap * sizeof *no);
    if (no == NULL) {
      return 0u;
    }
    g_hx.open     = no;
    g_hx.open_cap = ncap;
  }
  g_hx.open[at] = arity;
  return 1u;
}

// One flatterm side, the cells of its span in preorder: encodes it and
// accumulates the 'P' feature walk's counters; returns its depth.  The
// open function cells live on a growable stack instead of the C stack: a
// rule is as deep as the engine let it grow.
static u32 atp_hx_ft_side(AtpHxFeat *f, AtpHxEnc *e, const AtpFtCell *c,
                          u32 side) {
  const AtpFtCell *stop = (c->end != NULL) ? c->end->next : NULL;
  u32 depth  = 0u;
  u32 n_open = 0u;
  for (const AtpFtCell *p = c; p != stop; p = p->next) {
    f->nodes[side]++;
    if (n_open + 1u > depth) {
      depth = n_open + 1u;
    }
    if ((p->sym & WF_VAR_BIT) != 0u) {
      u32 id = p->sym & ~WF_VAR_BIT;
      atp_hx_feat_var(f, id, side);
      if (p == c) {
        f->top_var[side] = 1u;
      }
      atp_hx_enc_varint(e, ((u64)id << 1) | 1u);
    } else {
      atp_hx_feat_sym(f, p->sym);
      if (p == c) {
        f->top_sym[side] = p->sym;
      }
      atp_hx_enc_varint(e, (u64)p->sym << 1);
      atp_hx_enc_varint(e, p->arity);
      if (p->arity > 0u) {
        if (!atp_hx_open_push(n_open, p->arity)) {
          e->over     = 1u;
          f->overflow = 1u;
          break;
        }
        n_open++;
        continue;
      }
    }
    // A leaf ends an argument of the innermost open cell; a cell whose
    // last argument ended ends an argument of its parent in turn.
    while (n_open > 0u && --g_hx.open[n_open - 1u] == 0u) {
      n_open--;
    }
  }
  return depth;
}
#endif

// Encode rule slot `slot` into g_hx.enc; fills r's lengths, features,
// content flags and continuation count.  Returns the stored byte count.
static u32 atp_hx_rule_encode(const AtpState *s, u32 slot, AtpHxRec *r) {
  r->pos_len = 0u;
#ifdef THVM_ATPFT_RULES
  const AtpFtCell *l  = (s->lhs_ft != NULL) ? s->lhs_ft[slot] : NULL;
  const AtpFtCell *rr = (s->rhs_ft != NULL) ? s->rhs_ft[slot] : NULL;
  if (l == NULL || rr == NULL) {
    r->flags |= ATP_HX_Q_NOCONTENT;
    return 0u;
  }
  AtpHxFeat f;
  memset(&f, 0, sizeof f);
  AtpHxEnc e = { g_hx.enc, g_hx.enc_cap, 0u, 0u };
  f.depth[0] = atp_hx_ft_side(&f, &e, l, 0u);
  r->pa = e.len;
  f.depth[1] = atp_hx_ft_side(&f, &e, rr, 1u);
  r->pb = e.len - r->pa;
  g_hx.enc     = e.buf;
  g_hx.enc_cap = e.cap;
  atp_hx_feat_finish(&f, r->feat);
  r->flags |= ATP_HX_F_FEAT;
  if (g_hx.gsym_ready) {
    r->flags |= ATP_HX_F_GOALSYM;
  }
  if (e.over) {
    r->flags |= ATP_HX_Q_TRUNC;
    return 0u;
  }
  r->pos_len = (u8)((e.len + ATP_HX_CONT_BYTES - 1u) / ATP_HX_CONT_BYTES);
  r->aux = atp_hx_fnv(e.buf, e.len);
  return e.len;
#else
  (void)s;
  (void)slot;
  r->flags |= ATP_HX_Q_NOCONTENT;
  return 0u;
#endif
}

static void atp_hx_write_cont(u32 len) {
  for (u32 k = 0; k * ATP_HX_CONT_BYTES < len; k++) {
    AtpHxContRec q;
    memset(&q, 0, sizeof q);
    q.tag   = 'q';
    q.index = (u8)k;
    u32 off = k * ATP_HX_CONT_BYTES;
    u32 n   = len - off;
    if (n > ATP_HX_CONT_BYTES) {
      n = ATP_HX_CONT_BYTES;
    }
    memcpy(q.bytes, g_hx.enc + off, n);
    fwrite(&q, ATP_HX_REC_SIZE, 1u, g_hx.fp);
  }
}

static void atp_hx_rule_flags(const AtpState *s, u32 slot, AtpHxRec *r) {
  if (s->r_orient[slot]) {
    r->flags |= ATP_HX_Q_ORIENTED;
  }
  if (atp_otter_passive(s, slot)) {
    r->flags |= ATP_HX_Q_PASSIVE;
  }
}

// Parents + overlap depth off the CP's own trace entry (TRACE_CP:
// children 0/1 = outer/inner rule trace ids, child 4 = position length;
// TRACE_AXIOM / TRACE_SIMPLIFY: child 0 = the source).
static void atp_hx_parents(const AtpState *s, AtpHxRec *r, u32 tid) {
  if (tid == ATP_TRACE_NONE || tid >= s->n_trace) {
    return;
  }
  Term e = s->trace[tid];
  if (term_tag(e) != TAG_CTR || term_ctr_n(e) < 2u) {
    return;
  }
  r->pa = (u32)term_val(term_ctr_at(e, 0));
  r->pb = (u32)term_val(term_ctr_at(e, 1));
  if (term_ext(e) == TRACE_CP && term_ctr_n(e) >= 6u) {
    r->pos_len = (u8)term_val(term_ctr_at(e, 4));
  }
}

static void atp_hx_row(const AtpState *s, AtpHxRec *r, u32 row) {
  r->kind  = s->cp_kind[row];
  r->seq   = s->cp_seq[row];
  r->trace = s->cp_trace[row];
  r->pri   = s->cp_pri[row];
  r->depth = (s->cp_depth != NULL) ? s->cp_depth[row] : 0u;
  if (s->cp_uid_a != NULL) {
    r->uid_a = s->cp_uid_a[row];
    r->uid_b = s->cp_uid_b[row];
  }
  if (s->cp_ultimate != NULL && s->cp_ultimate[row]) {
    r->flags |= ATP_HX_F_ULTIMATE;
  }
  if (s->cp_deprio[row]) {
    r->flags |= ATP_HX_F_DEPRIO;
  }
  if (s->cp_gd != NULL && s->cp_gd[row]) {
    r->flags |= ATP_HX_F_GD;
  }
  atp_hx_parents(s, r, r->trace);
}

// 'F' record of row `row` (seq already stamped), content `packed`.
static void atp_hx_cpfeat(const AtpState *s, u32 row, const u8 *packed) {
  if (!g_hx.cpfeat || packed == NULL) {
    return;
  }
  Term l = 0, r = 0;
  acp_unpack(packed, &l, &r);
  AtpHxFeatRec f;
  memset(&f, 0, sizeof f);
  f.tag = 'F';
  f.seq = s->cp_seq[row];
  thvm_atp_cp_features(s, l, r, s->cp_seq[row], f.feat);
  fwrite(&f, ATP_HX_REC_SIZE, 1u, g_hx.fp);
}

// --- hooks -------------------------------------------------------------

static void atp_hx_open(const AtpState *s) {
  const char *path = getenv("THVM_ATP_HISTORY_EXPORT");
  if (path == NULL || path[0] == '\0') {
    return;
  }
  if (g_hx.fp != NULL) {  // one recorded engine at a time
    return;
  }
  FILE *fp = fopen(path, "ab");
  if (fp == NULL) {
    fprintf(stderr, "history export: cannot open %s\n", path);
    return;
  }
  setvbuf(fp, NULL, _IOFBF, 1u << 20);
  static u32 session = 0u;
  double rate = 1.0 / 16.0;
  const char *re = getenv("THVM_ATP_HISTORY_RATE");
  if (re != NULL && re[0] != '\0') {
    rate = atof(re);
  }
  if (rate <= 0.0) {
    rate = 0.0;
  }
  u32 thresh = (rate >= 1.0) ? 0xffffffffu
                             : (u32)(rate * 4294967296.0);
  memset(&g_hx, 0, sizeof g_hx);
  g_hx.fp      = fp;
  g_hx.s       = s;
  g_hx.thresh  = thresh;
  g_hx.session = session++;
  g_hx.cpfeat  = atp_env_on("THVM_ATP_HISTORY_CPFEAT") ? 1u : 0u;
  AtpHxHdrRec h;
  memset(&h, 0, sizeof h);
  h.tag   = 'B';
  h.kind  = ATP_HX_VERSION;
  h.seq   = g_hx.session;
  h.step  = s->cp_select_count;
  h.trace = ATP_TRACE_NONE;
  h.pa    = (u32)getpid();
  h.pb    = ATP_TRACE_NONE;
  h.pri   = thresh;
  const char *br = getenv("THVM_ATP_FOF_BRANCH_ID");
  if (br != NULL && br[0] != '\0') {
    h.pb     = (u32)atoi(br);
    h.flags |= 2u;
  }
  const char *lane = getenv("FINDPROOF_SLICE");
  if (lane != NULL) {
    size_t n = strlen(lane);
    if (n > ATP_HX_LANE_MAX) {
      n = ATP_HX_LANE_MAX;
      h.flags |= 1u;
    }
    memcpy(h.lane, lane, n);
    h.lane_len = (u8)n;
  }
  fwrite(&h, ATP_HX_REC_SIZE, 1u, g_hx.fp);
}

// Symbol table entry, written by the driver once the engine exists.
fn void thvm_atp_history_symbol(AtpState *s, u32 label, u32 arity,
                                u8 fresh, const char *name) {
  if (!atp_hx_on(s) || name == NULL) {
    return;
  }
  atp_work_pause();
  AtpHxSymRec y;
  memset(&y, 0, sizeof y);
  y.tag   = 'Y';
  y.arity = (u8)(arity > 0xffu ? 0xffu : arity);
  y.label = label;
  if (fresh) {
    y.flags |= 2u;
  }
  if (arity > 0xffu) {
    y.flags |= 4u;
  }
  size_t n = strlen(name);
  if (n > ATP_HX_NAME_MAX) {
    n = ATP_HX_NAME_MAX;
    y.flags |= 1u;
  }
  memcpy(y.name, name, n);
  y.name_len = (u8)n;
  fwrite(&y, ATP_HX_REC_SIZE, 1u, g_hx.fp);
  atp_work_resume();
}

// Queue insert, row `row` fully stamped.  `packed` = the slot's bytes
// (NULL for descriptor slots).
static void atp_hx_push(const AtpState *s, u32 row, const u8 *packed,
                        u32 nodes) {
  if (!atp_hx_on(s)) {
    return;
  }
  u8 raw = g_hx.raw_pending;
  g_hx.raw_pending = 0u;
  if (!atp_hx_sampled(s->cp_seq[row])) {
    return;
  }
  atp_work_pause();
  atp_hx_gsym_ensure(s);
  AtpHxRec r;
  atp_hx_rec_init(&r, 'P', s);
  atp_hx_row(s, &r, row);
  if (raw) {
    r.flags |= ATP_HX_F_RAW;
  }
  if (packed == NULL) {
    r.flags |= ATP_HX_F_IMPLICIT;
  }
  r.aux = nodes;
  atp_hx_feat_packed(&r, packed);
  atp_hx_write(&r);
  atp_hx_cpfeat(s, row, packed);
  g_hx.n_rec_push++;
  atp_work_resume();
}

// The WM intake permutes the queued axioms' seq stamps before the first
// pick: re-emit every queued row so the LAST 'P' record per seq holds
// the post-permutation content (flag ATP_HX_F_RESTAMP).
static void atp_hx_restamp_queue(const AtpState *s) {
  if (!atp_hx_on(s)) {
    return;
  }
  atp_work_pause();
  atp_hx_gsym_ensure(s);
  for (u32 pos = 0; pos < s->n_cps; pos++) {
    u32 row = s->cp_heap[pos].row;
    if (!atp_hx_sampled(s->cp_seq[row])) {
      continue;
    }
    AtpHxRec r;
    atp_hx_rec_init(&r, 'P', s);
    atp_hx_row(s, &r, row);
    r.flags |= ATP_HX_F_RESTAMP;
    if (s->cp_packed[row] == NULL) {
      r.flags |= ATP_HX_F_IMPLICIT;
    }
    atp_hx_feat_packed(&r, s->cp_packed[row]);
    atp_hx_write(&r);
    atp_hx_cpfeat(s, row, s->cp_packed[row]);
  }
  atp_work_resume();
}

// Non-orphan pop of row `row` (bytes still owned by the slot), chosen by
// selection dimension `pick` (ATP_HX_PICK_*).
static void atp_hx_select(const AtpState *s, u32 row, u8 pick) {
  if (!atp_hx_on(s)) {
    return;
  }
  atp_work_pause();
  atp_hx_gsym_ensure(s);
  AtpHxRec r;
  atp_hx_rec_init(&r, 'S', s);
  atp_hx_row(s, &r, row);
  r.kind = (u8)((r.kind & 0x0fu) | (u8)(pick << 4));
  if (s->cp_packed[row] == NULL) {
    r.flags |= ATP_HX_F_IMPLICIT;
  }
  r.aux = s->cp_seq_next;
  atp_hx_feat_packed(&r, s->cp_packed[row]);
  atp_hx_write(&r);
  g_hx.last_sel_seq = r.seq;
  g_hx.sel_ntrace   = s->n_trace;
  g_hx.n_rec_sel++;
  atp_work_resume();
}

// Queued CP re-scored in place (CP-set interreduction survivors, the
// reheapify rebuild): record the new priority (and the new content's
// features) when it changed, or when the slot was just repacked.
static void atp_hx_reweight(const AtpState *s, u32 row, u32 old_pri) {
  if (!atp_hx_on(s)) {
    return;
  }
  u8 forced = g_hx.repacked;
  g_hx.repacked = 0u;
  if (!forced && s->cp_pri[row] == old_pri) {
    return;
  }
  if (!atp_hx_sampled(s->cp_seq[row])) {
    return;
  }
  atp_work_pause();
  AtpHxRec r;
  atp_hx_rec_init(&r, 'W', s);
  r.seq  = s->cp_seq[row];
  r.pri  = s->cp_pri[row];
  r.aux  = old_pri;
  r.kind = forced;
  atp_hx_feat_packed(&r, s->cp_packed[row]);
  atp_hx_write(&r);
  atp_work_resume();
}

static void atp_hx_outcome(const AtpState *s, u8 code) {
  if (!atp_hx_on(s)) {
    return;
  }
  atp_work_pause();
  AtpHxRec r;
  atp_hx_rec_init(&r, 'X', s);
  r.kind = code;
  r.seq  = g_hx.last_sel_seq;
  r.pa   = g_hx.sel_ntrace;
  r.pb   = s->n_trace;
  r.aux  = s->n_cps;
  atp_hx_write(&r);
  atp_work_resume();
}

static void atp_hx_del(const AtpState *s, u32 seq, u8 reason) {
  if (!atp_hx_on(s) || !atp_hx_sampled(seq)) {
    return;
  }
  atp_work_pause();
  AtpHxRec r;
  atp_hx_rec_init(&r, 'D', s);
  r.kind = reason;
  r.seq  = seq;
  atp_hx_write(&r);
  atp_work_resume();
}

static void atp_hx_kill(const AtpState *s, u8 tag, u32 id) {
  if (!atp_hx_on(s)) {
    return;
  }
  atp_work_pause();
  AtpHxRec r;
  atp_hx_rec_init(&r, tag, s);
  r.seq = id;
  atp_hx_write(&r);
  atp_work_resume();
}

// Rule slot `slot` holds new content (kind = ATP_HX_RULE_*): its trace
// id, uid and flatterm mirror are final.  `src_seq` = the queued CP it
// came from (ATP_TRACE_NONE when none).
static void atp_hx_rule_slow(const AtpState *s, u32 slot, u8 kind,
                             u32 src_seq) {
  atp_work_pause();
  atp_hx_gsym_ensure(s);
  AtpHxRec r;
  atp_hx_rec_init(&r, 'Q', s);
  r.kind  = kind;
  r.seq   = s->r_trace[slot];
  r.trace = s->r_uid[slot];
  r.pri   = src_seq;
  r.uid_a = slot;
  r.uid_b = s->n_rules;
  r.depth = s->r_depth[slot];
  if (s->r_gd[slot]) {
    r.flags |= ATP_HX_F_GD;
  }
  atp_hx_rule_flags(s, slot, &r);
  u32 len = atp_hx_rule_encode(s, slot, &r);
  atp_hx_write(&r);
  atp_hx_write_cont(len);
  atp_work_resume();
}

static inline void atp_hx_rule(const AtpState *s, u32 slot, u8 kind,
                               u32 src_seq) {
  if (atp_hx_on(s)) {
    atp_hx_rule_slow(s, slot, kind, src_seq);
  }
}

// Rule slot `slot` is about to leave R (the rule journal's death event;
// the slot still holds the rule).
static void atp_hx_rule_death_slow(const AtpState *s, u32 slot) {
  atp_work_pause();
  AtpHxRec r;
  atp_hx_rec_init(&r, 'Z', s);
  r.seq   = s->r_trace[slot];
  r.trace = s->r_uid[slot];
  r.uid_a = slot;
  r.uid_b = s->n_rules;
  r.depth = s->r_depth[slot];
  atp_hx_rule_flags(s, slot, &r);
  atp_hx_write(&r);
  atp_work_resume();
}

static inline void atp_hx_rule_death(const AtpState *s, u32 slot) {
  if (atp_hx_on(s)) {
    atp_hx_rule_death_slow(s, slot);
  }
}

// Otter replay event `kind` (ATP_HX_OT_*) for the queued CP `seq` at
// payload row `row` (0xffffffff when it left the queue); `uid` / `tid` =
// its passive rule's uid and trace id (0 / ATP_TRACE_NONE when none).
// Outcomes that create or remove no rule are written for sampled CPs
// only.
static void atp_hx_otter_slow(const AtpState *s, u8 kind, u32 seq, u32 row,
                              u32 uid, u32 tid, u8 flags) {
  u8 always = (u8)(kind == ATP_HX_OT_ADOPT || kind == ATP_HX_OT_DROP ||
                   kind == ATP_HX_OT_RETIRE_SEL ||
                   kind == ATP_HX_OT_RETIRE_LRS);
  if (!always && !atp_hx_sampled(seq)) {
    return;
  }
  atp_work_pause();
  AtpHxRec r;
  atp_hx_rec_init(&r, 'O', s);
  r.kind  = kind;
  r.flags = flags;
  r.seq   = seq;
  r.pa    = tid;
  r.aux   = s->n_rules;
  if (uid != 0u) {
    r.trace = uid;
  }
  if (row < s->n_cps && s->cp_seq[row] == seq) {
    r.pri   = s->cp_pri[row];
    r.depth = (s->cp_depth != NULL) ? s->cp_depth[row] : 0u;
  }
  atp_hx_write(&r);
  atp_work_resume();
}

static inline void atp_hx_otter(const AtpState *s, u8 kind, u32 seq, u32 row,
                                u32 uid, u32 tid, u8 flags) {
  if (atp_hx_on(s)) {
    atp_hx_otter_slow(s, kind, seq, row, uid, tid, flags);
  }
}

// The search is over (proof emission comes next): later trace entries are
// flagged in the session-end dump.
fn void thvm_atp_history_search_end(AtpState *s) {
  if (!atp_hx_on(s)) {
    return;
  }
  g_hx.n_trace_search = s->n_trace;
}

// Session end: dump the non-CP trace entries (the rule-side derivation
// DAG), then the summary record.  Idempotent; status 0xff = unknown.
fn void thvm_atp_history_finish(AtpState *s, u32 status) {
  if (!atp_hx_on(s)) {
    return;
  }
  atp_hx_gsym_ensure(s);
  for (u32 i = 0; i < s->n_trace; i++) {
    Term e = s->trace[i];
    if (term_tag(e) != TAG_CTR || term_ctr_n(e) < 4u) {
      continue;
    }
    u32 reason = term_ext(e);
    if (reason == TRACE_CP) {
      continue;
    }
    AtpHxRec r;
    atp_hx_rec_init(&r, 'R', s);
    r.kind  = (u8)reason;
    r.seq   = i;
    r.trace = (u32)term_val(term_ctr_at(e, 0));
    r.pa    = (u32)term_val(term_ctr_at(e, 1));
    r.step  = 0xffffffffu;
    if (g_hx.n_trace_search != 0u && i >= g_hx.n_trace_search) {
      r.flags |= 1u;
    }
    if (reason == TRACE_NORM_STEP && term_ctr_n(e) >= 5u) {
      r.pos_len = (u8)term_val(term_ctr_at(e, 4));
    }
    Term lhs = term_ctr_at(e, 2), rhs = term_ctr_at(e, 3);
    if (reason != TRACE_NORM_STEP) {
      (void)atp_hx_feat_pair(&r, lhs, rhs);
    }
    atp_hx_write(&r);
  }
  AtpHxRec r;
  atp_hx_rec_init(&r, 'E', s);
  r.kind  = (u8)(status > 0xfeu ? 0xffu : status);
  r.seq   = s->n_trace;
  r.trace = s->cp_seq_next;
  r.pa    = (u32)g_hx.n_rec_push;
  r.pb    = (u32)((s->use_orphan_murder ? 1u : 0u)
                | (atp_uid_orphan_on() ? 2u : 0u)
                | (s->record_norm_steps ? 4u : 0u)
                | (s->cp_trace_lean ? 8u : 0u)
                | (s->use_otter ? 16u : 0u)
                | (s->use_lrs ? 32u : 0u));
  r.pri   = s->n_rules;
  r.uid_a = s->n_cps;
  r.uid_b = (u32)g_hx.n_rec_sel;
  r.depth = s->t_max;
  r.aux   = g_hx.n_trace_search;
  atp_hx_write(&r);
  fclose(g_hx.fp);
  free(g_hx.gsym);
  atp_hx_scratch_free();
  memset(&g_hx, 0, sizeof g_hx);
}
