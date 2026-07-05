// Waldmeister CP-emission-order mirror.
//
// WM assigns every queued critical pair a FIFO age w2 = ++CPNr in
// EMISSION order, and the selection heap breaks equal-w1 ties on w2 --
// so reproducing WM's selection sequence exactly requires reproducing
// the ORDER in which one new fact's CP batch is emitted.  This module
// mirrors the order-relevant state of WM's discrimination trees
// (DSBaum) and leaf lists so the saturator can sort each new-fact CP
// batch into WM's emission order before pushing (the CP CONTENT still
// comes from thvm's own unification; the mirror only ranks).
//
// Decoded from waldmeister/sources -- the whole model is validated
// executable-spec-style against a full `wmcli -a 4` McCune-II trace
// (38/41 insertion batches byte-exact in emission order; the residual
// 3 batches differ in 5 CPs tied to one removal-avalanche corner,
// documented at the bottom of this header):
//
// 1. Phase structure per new fact (INF/Unifikation1.c
//    U1_KPsBildenZuRegel :1481-1548, U1_KPsBildenZuGleichung
//    :1556-1668).  New RULE: phase A = per TT(l) subterm in flatterm
//    preorder (vars skipped), unify with rule-tree tops (the new rule
//    itself excluded via the Ausschluss object) then equation-tree
//    tops; phase B = l into eTT (proper subterms) of the rule tree
//    then the equation tree.  New EQUATION adds the self-root phases
//    and the reversed-face phases: A, B, F (l =? l), C (r =? l,
//    stereo only), D (per TT(r): EQUATION tops then rule tops --
//    mirrored vs A, :1636-1641), E (r into eTT: equation tree then
//    rule tree, :1648-1653), G (r =? r).  Mono equations
//    (IstMonogleichung) run A, B, F only and index their
//    distinguished face only.
//
// 2. eTT enumeration (Unifikation1.c TermMitDSBaumTeiltermenUnifizieren
//    :877-892 = DSBaumKnoten.h BK_forRegeln :470-479) is NOT a tree
//    walk: it scans the tree's LEAF LIST; per leaf the fact chain;
//    per fact the stored face's proper subterms in flatterm preorder.
//
// 3. Leaf list (WDT/DSBaumOperationen.c BlattEinzeigern :365-442):
//    ascending by stored-term depth (TiefeBlaetter classes); a new
//    leaf whose depth class exists is inserted immediately AFTER the
//    class head (:382-390) => class = head + LIFO of the rest; a new
//    class splices between neighbor classes.  Removal (:949-981):
//    head succession to the next same-depth leaf.
//
// 4. Per-leaf fact chains PREPEND (RegelHinzufuegen :343-347) =>
//    newest first.
//
// 5. Tops enumeration = re-entrant DFS over the trie (Unifikation1.c
//    Delta* :626-820): at a query-FUNCTION cell try the exact-symbol
//    child first, then variable children by ascending symbol code
//    (variables are negative, SymbolOperationen.h:130, so ascending
//    code = DESCENDING variable index); at a query-VARIABLE cell try
//    variable children (ascending code) then jump exits
//    (Sprungausgaenge) in list order.  Every successful leaf arrival
//    emits (duplicate arrivals are real WM emissions; ranking by
//    FIRST arrival preserves the relative order of distinct CPs).
//
// 6. The trie is leaf-compressed (leaves hold the string Rest,
//    unified wholesale at arrival -- MGUMitBlattGefunden :610-613).
//    Every jump exit prepends at creation (RumpfSprungeintragSetzen
//    :283-299), so a node's outgoing list reads back as the reverse of
//    its jumps' creation order.  wmo_tree_insert is a faithful port of
//    BO_ObjektEinfuegen + BlattAufgeteilt: a real jump stack (Sprung-
//    stapel, WmoStapel here) carries the open parent-subterm pendings
//    through the descent, and a split runs the GleichPfad chain loop
//    (:628-654, popping chain pendings into the new inner nodes), the
//    new-leaf hang (NeuesBlattEinhaengen :444-484), AltesBlattPolieren
//    (:492-562) and NeueSpruengeInsAlteBlatt (:565-586) in WM's order
//    over the SAME stack (Stapel2Sichern mirrors the surviving counters
//    into a second index so the chain pendings serve both the new-leaf
//    drain and the old-leaf drain).  AltesBlattPolieren reworks the old
//    leaf's surviving in-jumps: a subterm reaching past the split
//    (PositionNachTeilterm > j) gets a fresh PARALLEL into the new leaf
//    placed immediately after the survivor (:523-526), except a strict-
//    ancestor enclosing jump whose new-leaf subterm is SHORTER heads the
//    start node's outgoing list (BooleanAxioms OrAssociativity @300); a
//    subterm closing inside the chain (PositionNachTeilterm <= j) is
//    RE-TARGETED in place onto the chain node (:534-560), and when it
//    closes at the very first chain node with an interior chain still
//    below it (PositionNachTeilterm == i+1, j > i+1) a fresh chain-node
//    jump is additionally head-inserted (WolframAxioms prefix @91).
//    Plain hangs (NeuesBlattEinhaengen from the main walk, Untergrenze =
//    EintragEins) pop every pending jump start except the bottom-most
//    into prepended new-leaf entries.  Removal collapse
//    (BO_ObjektEntfernen :991-1101): freed nodes' exits die; entries
//    targeting freed nodes rewire to the surviving sibling leaf with
//    exit-list position preserved (AlleEingehendenSpruengeUmsetzen
//    :808-822).
//
// Removal-exit order (BO_ObjektEntfernen Schrumpfen): when a multi-rule
// removal avalanche collapses a one-way branch to a single surviving
// leaf, that leaf re-hangs at the collapse-target node `up` (:1083) and
// the short jump reaching it is RE-ISSUED, so it heads up's outgoing
// exit list (RumpfSprungeintragSetzen :293-295); every other freed-node
// jump rewires in place (:814).  A later split that parallels an entry
// then lands its parallel AFTER that re-headed jump, matching WM's
// arrival order.  Validated 41/41 insertion batches byte-exact on the
// full wmcli -a 4 McCune-II trace (was 38/41; the 3 residual batches
// were exactly this corner).
//
// Identity: facts are tracked by their birth trace id (stable across
// slot compaction).  A side registry records every face inserted per
// trace id so removal needs no live Terms.

#define WMO_MAX_CELLS  768u
#define WMO_MAX_VARS   64u

typedef struct {
  u32 sym;       // ctr label, or var index (1-based, canonical per face)
  u8  is_var;
  u8  arity;
  // NOTE (2026-07-04, measured): a u16 `skip` cache in the 2 padding
  // bytes here (subterm end = i + skip, filled in wmo_cells_from_term,
  // wmo_sub_end reads it) was REJECTED by wall-clock A/B x5 alternating
  // pairs: 13.42s vs 13.56s mean on the OA C bench (~-0.15s, below the
  // 0.3s bar) -- the arity-chain scans are short and well-predicted.
  // Do not re-try.  Side finding kept for the record: the rank-cache
  // key memcmp (wmo_tops_rank) compares these 2 uninitialised padding
  // bytes, causing ~600 false misses/run on OA (hit rate 24.8% vs 26.6%
  // with deterministic padding) -- benign (a miss recomputes the
  // identical list), just a known noise source in the hit counters.
} WmoCell;

struct WmoNode;
struct WmoLeaf;

typedef struct WmoEntry {
  struct WmoNode  *start;
  WmoCell         *sub;       // owned copy of the skipped subterm cells
  u32              sub_len;
  void            *ziel;      // WmoNode* or WmoLeaf*
  u8               ziel_leaf;
  struct WmoEntry *next;      // exit-list order (next = consulted later)
} WmoEntry;

typedef struct WmoKid {
  WmoCell          sym;
  void            *child;     // WmoNode* or WmoLeaf*
  u8               is_leaf;
} WmoKid;

typedef struct WmoNode {
  WmoKid          *kids;
  u32              n_kids, cap_kids;
  WmoEntry        *exits;     // head = consulted first
  struct WmoNode  *parent;
} WmoNode;

typedef struct WmoChainEnt {
  u32 trace;                  // fact identity
  u8  face;                   // 0 = distinguished (lhs), 1 = reverse
} WmoChainEnt;

typedef struct WmoLeaf {
  WmoCell         *key;       // full face cell string (owned)
  u32              key_len;
  u32              hang;      // index of the edge cell reaching this leaf
  u32              depth;     // term depth of the face
  WmoChainEnt     *chain;     // newest first
  u32              n_chain, cap_chain;
  WmoNode         *parent;
  struct WmoLeaf  *ll_prev, *ll_next;   // WM leaf list
} WmoLeaf;

typedef struct {
  WmoNode *root;              // NULL = empty; root holds kids keyed by cell 0
  WmoLeaf *ll_head;
} WmoTree;

// Multi-entry arrival-list cache for wmo_tops_rank.  Each entry caches one
// distinct (tree, query-subterm)'s DFS-ordered arrival list (the leaves whose
// stored face unifies with the query).  The per-CP DFS that fills this is the
// WM-faithful path's dominant cp-gen cost, and a new fact's ~1000 CPs cycle
// through only ~dozens of distinct query positions, so an N-way cache turns
// almost every per-CP DFS into a skip.  WMO_TOPS_ARR_CAP is the per-entry
// arrival-buffer cap (heap-allocated lazily per slot, ~128KB each).
#define WMO_TOPS_ARR_CAP 16384u
#define WMO_RC_N 64u
#define WMO_LLR_N 4096u
#define WMO_DRC_N 2048u
typedef struct {
  u8        valid;
  u8        tree;               // 0 rules / 1 equations
  u64       rev;                // tree_rev at which this entry was filled
  u32       qn;                 // query cell count
  u32       n_out;              // arrival count held in out[]
  WmoCell   q[WMO_MAX_CELLS];   // query cells (the cache key)
  WmoLeaf **out;                // arrival buffer (lazily malloc'd, WMO_TOPS_ARR_CAP)
} WmoRankCacheEnt;

typedef struct {
  u32 trace;
  u8  tree;                   // 0 = rule tree, 1 = equation tree
  u8  face;
  u8  dist_rhs;               // 1 iff WM's distinguished face = thvm's stored RHS
  WmoCell *cells;             // owned copy (for removal lookup)
  u32 n_cells;
} WmoReg;

// One partner emitted by the single-walk Vater/Mutter enumerations.
typedef struct { u32 trace; u32 arrival; u8 face; } WmoPartnerHit;

typedef struct AtpWmOrder {
  WmoTree  tree[2];           // [0] rules, [1] equations
  WmoReg  *reg;
  u32      n_reg, cap_reg;
  // Multi-entry arrival-list cache for wmo_tops_rank (struct WmoRankCacheEnt
  // above).  wmo_tops_rank runs a fresh discrimination-tree DFS per CP rank
  // query -- the WM-faithful path's dominant cp-gen self-time.  A new fact's
  // ~1000 CPs cycle through only ~dozens of distinct (tree, query-subterm)
  // positions, so caching the N most-recent distinct queries' arrival lists
  // turns nearly every per-CP DFS into a skip (the old single entry caught
  // only an immediately-repeated query, ~58%).  tree_rev is bumped by every
  // tree/chain mutation (wmo_register / atp_wmo_remove_trace); each entry
  // gates rev==tree_rev, so a mutation invalidates the whole cache --
  // byte-identical to recomputing.
  u64      tree_rev;          // ++ on every tree/chain mutation
  u8       no_rankcache;      // THVM_WMO_NO_RANKCACHE: disable the cache
  u32      rc_next;           // round-robin eviction cursor
  WmoRankCacheEnt rc[WMO_RC_N];
  // Leaf-list rank memo for wmo_leaflist_rank: (tree, trace, face) ->
  // (found, ll, ch).  The normalize-redex ranking (atp_wmo_eq_leaflist_
  // rank) queries an O(leaves x chain) linear scan over a list that
  // only mutates between batches.
  // Direct-mapped, validated per entry against tree_rev (same
  // freeze-window discipline as the tops arrival cache above), so a
  // hit returns exactly what the scan would recompute.  Shares
  // no_rankcache as its disable switch.
  struct WmoLlrEnt {
    u64 rev;
    u32 trace;
    u32 ll, ch;
    u8  tree, face, found, valid;
  } llr[WMO_LLR_N];
  // wmo_trace_dist_rhs memo: trace -> dist_rhs verdict (present or the
  // unregistered-default 0), validated per entry against tree_rev
  // (bumped by every registry mutation -- register / remove --
  // so a stale entry cannot survive one).  The linear registry scan sits
  // on the walk's per-fact and per-rank-query hot paths.
  // Shares no_rankcache as its disable switch.
  struct WmoDrcEnt {
    u64 rev;
    u32 trace;
    u8  dr, valid;
  } drc[WMO_DRC_N];
  // WM-faithful AltesBlattPolieren construction (use_wm_trie_faithful): when
  // set, every BlattAufgeteilt parallel jump splices immediately AFTER the
  // surviving model in the start node's outgoing list (DSBaumOperationen.c
  // :521-526, "hinter den Eintrag setzen"), never head-inserts -- so the
  // runtime DFS reaches a split leaf's parallel face at WM's exact arrival
  // rank.  Set from AtpState.use_wm_trie_faithful at tree creation; default 0
  // keeps the historical head-insert special case (byte-identical OFF).
  u8       polier_after;
} AtpWmOrder;

// ---------- cells ----------

static u32 wmo_sub_end(const WmoCell *c, u32 i) {
  u32 need = 1;
  while (need) {
    need += (u32)c[i].arity;
    need--;
    i++;
  }
  return i;
}

static u32 wmo_term_depth(Term t) {
  if (term_tag(t) != TAG_CTR) return 1;
  u32 n = term_ctr_n(t), d = 0;
  for (u32 i = 0; i < n; i++) {
    u32 cd = wmo_term_depth(term_ctr_at(t, i));
    if (cd > d) d = cd;
  }
  return d + 1;
}

// Flatten `t` into cells with canonical first-occurrence var numbering
// (BO_TermpaarNormieren on the face side: lhs-first numbering means the
// face's own cells depend only on the face).  Returns cell count or 0
// on overflow.
static u32 wmo_cells_from_term(Term t, WmoCell *out, u32 cap,
                               u32 *vmap, u32 *n_vars) {
  if (term_tag(t) == TAG_FVR) {
    u32 v = term_ext(t);
    u32 id = 0;
    if (v < REWRITE_MAX_VAR && vmap[v] != 0u) {
      id = vmap[v];
    } else {
      id = ++(*n_vars);
      if (v < REWRITE_MAX_VAR) vmap[v] = id;
    }
    if (cap < 1u) return 0;
    out[0].sym = id; out[0].is_var = 1u; out[0].arity = 0u;
    return 1u;
  }
  if (term_tag(t) != TAG_CTR) return 0;
  u32 n = term_ctr_n(t);
  if (cap < 1u || n > 255u) return 0;
  out[0].sym = term_ext(t); out[0].is_var = 0u; out[0].arity = (u8)n;
  u32 len = 1u;
  for (u32 i = 0; i < n; i++) {
    u32 cl = wmo_cells_from_term(term_ctr_at(t, i), out + len, cap - len,
                                 vmap, n_vars);
    if (cl == 0u) return 0;
    len += cl;
  }
  return len;
}

static u32 wmo_face_cells(Term face, WmoCell *out, u32 cap) {
  u32 vmap[REWRITE_MAX_VAR] = {0};
  u32 nv = 0;
  return wmo_cells_from_term(face, out, cap, vmap, &nv);
}

static u8 wmo_cell_eq(const WmoCell *a, const WmoCell *b) {
  // Cell identity is the full discrimination-tree edge symbol.  WM keys every
  // edge by a symbol CODE whose arity is fixed (SO_Stelligkeit); its splits
  // compare codes (SO_SymbGleich, DSBaumOperationen.c :631).  thvm stores the
  // constructor label in `sym` and the arity separately, and an AC-flattened
  // operator reuses one label across arities (e.g. f/0 vs f/2), so a faithful
  // code comparison MUST include `arity` -- otherwise the split loop conflates
  // f/0 with f/2, over-runs past a real difference, and builds a leaf hung one
  // cell past its key (hang == key_len), which segfaults on later removal.
  // For fixed-arity theories (sym => arity) this is a no-op: byte-identical.
  return a->sym == b->sym && a->is_var == b->is_var && a->arity == b->arity;
}

// ---------- node / leaf helpers ----------

static WmoNode *wmo_node_new(WmoNode *parent) {
  WmoNode *n = (WmoNode *)calloc(1, sizeof(WmoNode));
  n->parent = parent;
  return n;
}

static void *wmo_kid_get(const WmoNode *n, const WmoCell *sym, u8 *is_leaf) {
  for (u32 i = 0; i < n->n_kids; i++) {
    if (wmo_cell_eq(&n->kids[i].sym, sym)) {
      if (is_leaf) *is_leaf = n->kids[i].is_leaf;
      return n->kids[i].child;
    }
  }
  return NULL;
}

static void wmo_kid_set(WmoNode *n, const WmoCell *sym, void *child,
                        u8 is_leaf) {
  for (u32 i = 0; i < n->n_kids; i++) {
    if (wmo_cell_eq(&n->kids[i].sym, sym)) {
      n->kids[i].child = child;
      n->kids[i].is_leaf = is_leaf;
      return;
    }
  }
  if (n->n_kids == n->cap_kids) {
    n->cap_kids = n->cap_kids ? n->cap_kids * 2u : 4u;
    n->kids = (WmoKid *)realloc(n->kids, n->cap_kids * sizeof(WmoKid));
  }
  n->kids[n->n_kids].sym = *sym;
  n->kids[n->n_kids].child = child;
  n->kids[n->n_kids].is_leaf = is_leaf;
  n->n_kids++;
}

static void wmo_kid_del(WmoNode *n, const WmoCell *sym) {
  for (u32 i = 0; i < n->n_kids; i++) {
    if (wmo_cell_eq(&n->kids[i].sym, sym)) {
      n->kids[i] = n->kids[n->n_kids - 1u];
      n->n_kids--;
      return;
    }
  }
}

static u32 wmo_node_depth(const WmoNode *n) {
  u32 d = 0;
  while (n->parent != NULL) { n = n->parent; d++; }
  return d;
}

// ---------- leaf list (BlattEinzeigern / Auszeigern) ----------

static WmoLeaf *wmo_class_head(WmoTree *t, u32 depth) {
  for (WmoLeaf *l = t->ll_head; l != NULL; l = l->ll_next) {
    if (l->depth == depth) return l;   // first leaf of the class = head
    if (l->depth > depth) return NULL;
  }
  return NULL;
}

static void wmo_ll_insert(WmoTree *t, WmoLeaf *leaf) {
  WmoLeaf *head = wmo_class_head(t, leaf->depth);
  if (head != NULL) {                  // insert right AFTER the class head
    leaf->ll_prev = head;
    leaf->ll_next = head->ll_next;
    if (head->ll_next) head->ll_next->ll_prev = leaf;
    head->ll_next = leaf;
    return;
  }
  WmoLeaf *prev = NULL, *cur = t->ll_head;
  while (cur != NULL && cur->depth <= leaf->depth) { prev = cur; cur = cur->ll_next; }
  leaf->ll_prev = prev;
  leaf->ll_next = cur;
  if (prev) prev->ll_next = leaf; else t->ll_head = leaf;
  if (cur) cur->ll_prev = leaf;
}

static void wmo_ll_remove(WmoTree *t, WmoLeaf *leaf) {
  if (leaf->ll_prev) leaf->ll_prev->ll_next = leaf->ll_next;
  else t->ll_head = leaf->ll_next;
  if (leaf->ll_next) leaf->ll_next->ll_prev = leaf->ll_prev;
  leaf->ll_prev = leaf->ll_next = NULL;
}

// ---------- entry bookkeeping over the whole tree ----------

static void wmo_walk_entries(WmoNode *n, void (*cb)(WmoNode *, void *),
                             void *ctx) {
  if (n == NULL) return;
  cb(n, ctx);
  for (u32 i = 0; i < n->n_kids; i++) {
    if (!n->kids[i].is_leaf) {
      wmo_walk_entries((WmoNode *)n->kids[i].child, cb, ctx);
    }
  }
}

typedef struct { void *target; } WmoKillCtx;

static void wmo_kill_entries_to_cb(WmoNode *n, void *raw) {
  WmoKillCtx *c = (WmoKillCtx *)raw;
  WmoEntry **slot = &n->exits;
  while (*slot != NULL) {
    if ((*slot)->ziel == c->target) {
      WmoEntry *dead = *slot;
      *slot = dead->next;
      free(dead->sub);
      free(dead);
    } else {
      slot = &(*slot)->next;
    }
  }
}

static void wmo_kill_entries_to(WmoTree *t, void *target) {
  WmoKillCtx c = { target };
  wmo_walk_entries(t->root, wmo_kill_entries_to_cb, &c);
}

typedef struct { void *from; void *to; u8 to_leaf; WmoNode *up; } WmoRewireCtx;

// Rewire every exit that targeted a freed node onto the surviving sibling.
// At the collapse-target node `up` (where the sibling re-hangs,
// BO_ObjektEntfernen :1083) the short jump reaching the sibling is re-issued
// for the re-hung leaf and PREPENDS to up's outgoing exit list per the
// standard RumpfSprungeintragSetzen head-insert (DSBaumOperationen.c
// :293-295); at every other node the freed-node-targeting jump is rewired in
// place (AlleEingehendenSpruengeUmsetzen :814 preserves the outgoing-list
// position).
static void wmo_rewire_cb(WmoNode *n, void *raw) {
  WmoRewireCtx *c = (WmoRewireCtx *)raw;
  WmoEntry **slot = &n->exits;
  while (*slot != NULL) {
    WmoEntry *e = *slot;
    if (e->ziel == c->from) {
      e->ziel = c->to;
      e->ziel_leaf = c->to_leaf;
      if (n == c->up && e != n->exits) {
        // move e to the head of n's exit list
        *slot = e->next;
        e->next = n->exits;
        n->exits = e;
        continue;   // *slot now points at the entry after the moved one
      }
    }
    slot = &(*slot)->next;
  }
}

// Sprung-compressed-leaf re-issue after a collapse.  The surviving leaf `sib`
// re-hangs as a compressed leaf at `up`, sharing `up` with one or more model
// leaves (the OTHER children of up) whose left side begins with the same
// function and arg1 cells.  While the now-freed chain still existed `sib` was
// deep-hung and its ancestor jumps were prepended (RumpfSprungeintragSetzen
// head-insert, DSBaumOperationen.c :293-295).  In WM that deep leaf arose
// instead as a BlattAufgeteilt split, so its ancestor jumps were placed by
// AltesBlattPolieren -- after-model for a binary split (single model) or as
// the most-recent head-insert for the newest leaf of a wider fan (several
// models).  `wmo_sprung_reissue_cb` handles the single-model after-model
// placement; `wmo_sprung_reissue_multi_cb` the multi-model head placement.
// The `models` array carries up's other leaf children to both callbacks.
typedef struct {
  WmoLeaf  *sib;
  WmoLeaf **models;
  u32       n_models;
  u32       up_depth;
} WmoReissueCtx;

static u32 wmo_shared_prefix(const WmoCell *a, u32 alen,
                             const WmoCell *b, u32 blen) {
  u32 k = 0;
  while (k < alen && k < blen && wmo_cell_eq(&a[k], &b[k])) k++;
  return k;
}

static u8 wmo_is_model(const WmoReissueCtx *c, const void *z) {
  for (u32 i = 0; i < c->n_models; i++)
    if ((const void *)c->models[i] == z) return 1u;
  return 0u;
}

static void wmo_sprung_reissue_cb(WmoNode *n, void *raw) {
  WmoReissueCtx *c = (WmoReissueCtx *)raw;
  if (wmo_node_depth(n) >= c->up_depth) return;
  // Locate the single head-prepended jump to `sib` and the best-matching
  // model jump (the one sharing the longest prefix) at this start node.
  WmoEntry *sib_e = NULL;
  u32 n_sib = 0;
  WmoEntry *best = NULL;
  u32 best_shared = 0, best_pos = 0, pos = 0;
  for (WmoEntry *e = n->exits; e != NULL; e = e->next) {
    if (e->ziel == (void *)c->sib) { sib_e = e; n_sib++; }
  }
  if (n_sib != 1u || n->exits != sib_e) return;   // need exactly one, at head
  for (WmoEntry *e = n->exits; e != NULL; e = e->next, pos++) {
    if (!wmo_is_model(c, e->ziel)) continue;
    u32 sh = wmo_shared_prefix(sib_e->sub, sib_e->sub_len, e->sub, e->sub_len);
    if (best == NULL || sh > best_shared) { best = e; best_shared = sh; best_pos = pos; }
  }
  if (best == NULL || best_shared < 2u) return;    // need the shared f + arg1
  if (best_pos == 0u) return;                      // model also at head -> no move
  // Unlink sib_e (currently the head) and splice it right after `best`.
  n->exits = sib_e->next;
  sib_e->next = best->next;
  best->next = sib_e;
}

// Multi-model collapse re-issue.  When `sib` re-hangs at `up` sharing it with
// SEVERAL model leaves (a multi-way pure branch, e.g. SKIToBCKW's depth-5
// C5/C6/C8/C9 fan after W's inner node collapses), `sib` is the newest leaf
// of the branch: in WM its ancestor long-jumps are the most-recent
// head-inserts at each start node (RumpfSprungeintragSetzen :293-295), so the
// re-hung leaf's jump heads each start node's exit list, ahead of the older
// model-group jumps.  thvm's deep-hang left those long jumps mid-list; restore
// the head position the most-recent head-insert gives.  Only the single jump
// to `sib` that shares the leading function + arg1 prefix with a model jump
// (the parallel group `sib` belongs to) moves, so unrelated long jumps and
// nodes at/below `up` stay put.
static void wmo_sprung_reissue_multi_cb(WmoNode *n, void *raw) {
  WmoReissueCtx *c = (WmoReissueCtx *)raw;
  if (wmo_node_depth(n) >= c->up_depth) return;
  WmoEntry *sib_e = NULL, **sib_slot = NULL;
  u32 n_sib = 0;
  for (WmoEntry **slot = &n->exits; *slot != NULL; slot = &(*slot)->next) {
    if ((*slot)->ziel == (void *)c->sib) { sib_e = *slot; sib_slot = slot; n_sib++; }
  }
  if (n_sib != 1u || sib_e == n->exits) return;    // one jump, not already head
  // Restrict to sib's own parallel group: a model jump sharing the leading
  // function + arg1 cells.  The newest leaf's long jump is the most-recent
  // head-insert at this start node, so it heads the exit list ahead of the
  // older model-group jumps; restore that.  But ONLY when reaching the head
  // does not cross a same-parallel-group jump: a sib that sits behind a
  // group member is not its group's head-most (most-recent) construction, so
  // WM keeps it spliced after that member (AltesBlattPolieren after-model),
  // not at the absolute head.  Crossing only unrelated jumps (different
  // parallel) restores the newest-first head-insert WM gives the group head.
  u32 best_shared = 0;
  for (WmoEntry *e = n->exits; e != NULL; e = e->next) {
    if (!wmo_is_model(c, e->ziel)) continue;
    u32 sh = wmo_shared_prefix(sib_e->sub, sib_e->sub_len, e->sub, e->sub_len);
    if (sh > best_shared) best_shared = sh;
  }
  if (best_shared < 2u) return;                    // need the shared f + arg1
  for (WmoEntry *e = n->exits; e != NULL && e != sib_e; e = e->next) {
    if (wmo_shared_prefix(sib_e->sub, sib_e->sub_len, e->sub, e->sub_len) >= 2u)
      return;                                      // a group member precedes sib
  }
  *sib_slot = sib_e->next;
  sib_e->next = n->exits;
  n->exits = sib_e;
}

// Middle-leaf removal re-issue.  When a branch node loses a child leaf that
// is neither its smallest nor its largest symbol (BO_ObjektEntfernen
// :1043-1046, SprunglistenBereinigenEinfach with NachbarBlatt = the node's
// smallest-symbol child), no path shrinks, but the surviving smallest-symbol
// leaf `minc` (the most-recently-introduced variable child = a fresh hang
// whose ancestor jumps were head-prepended) must restore the
// AltesBlattPolieren parallel order against the OTHER surviving leaf siblings:
// in WM that fresh leaf arose as a split of a sibling's Sprung-compressed
// leaf, so each ancestor jump was spliced AFTER the sibling's own jump
// (DSBaumOperationen.c :521-525, "hinter den Eintrag setzen").  At every node
// a single jump to `minc` that precedes a later sibling-leaf jump from the
// same enclosing subterm (sharing the leading function cell) moves to just
// after the last such later sibling jump.
typedef struct {
  WmoLeaf  *minc;
  WmoLeaf **sibs;
  u32       n_sibs;
} WmoMiddleCtx;

static void wmo_middle_reissue_cb(WmoNode *n, void *raw) {
  WmoMiddleCtx *c = (WmoMiddleCtx *)raw;
  // Locate the single jump to `minc` and its list position.
  WmoEntry *mj = NULL;
  u32 n_minc = 0;
  for (WmoEntry *e = n->exits; e != NULL; e = e->next) {
    if (e->ziel == (void *)c->minc) { mj = e; n_minc++; }
  }
  if (n_minc != 1u) return;
  // Find the last sibling-leaf jump that sits AFTER mj and shares the leading
  // function cell with mj's subterm.
  WmoEntry *best = NULL;
  u8 seen_mj = 0;
  for (WmoEntry *e = n->exits; e != NULL; e = e->next) {
    if (e == mj) { seen_mj = 1u; continue; }
    if (!seen_mj) continue;
    u8 is_sib = 0;
    for (u32 k = 0; k < c->n_sibs; k++)
      if (e->ziel == (void *)c->sibs[k]) { is_sib = 1u; break; }
    if (!is_sib) continue;
    if (wmo_shared_prefix(mj->sub, mj->sub_len, e->sub, e->sub_len) < 1u)
      continue;
    best = e;   // last qualifying sibling jump after mj
  }
  if (best == NULL) return;
  // Unlink mj from its current position and splice it right after `best`.
  WmoEntry **slot = &n->exits;
  while (*slot != mj) slot = &(*slot)->next;
  *slot = mj->next;
  mj->next = best->next;
  best->next = mj;
}

// ---------- insertion ----------

static WmoLeaf *wmo_leaf_new(WmoTree *t, const WmoCell *key, u32 key_len,
                             u32 hang, u32 depth, WmoNode *parent,
                             u32 trace, u8 face) {
  WmoLeaf *leaf = (WmoLeaf *)calloc(1, sizeof(WmoLeaf));
  leaf->key = (WmoCell *)malloc(key_len * sizeof(WmoCell));
  memcpy(leaf->key, key, key_len * sizeof(WmoCell));
  leaf->key_len = key_len;
  leaf->hang = hang;
  leaf->depth = depth;
  leaf->parent = parent;
  leaf->cap_chain = 2u;
  leaf->chain = (WmoChainEnt *)malloc(2u * sizeof(WmoChainEnt));
  leaf->chain[0].trace = trace;
  leaf->chain[0].face = face;
  leaf->n_chain = 1u;
  wmo_ll_insert(t, leaf);
  return leaf;
}

static void wmo_chain_prepend(WmoLeaf *leaf, u32 trace, u8 face) {
  if (leaf->n_chain == leaf->cap_chain) {
    leaf->cap_chain *= 2u;
    leaf->chain = (WmoChainEnt *)realloc(leaf->chain,
                                         leaf->cap_chain * sizeof(WmoChainEnt));
  }
  memmove(leaf->chain + 1, leaf->chain, leaf->n_chain * sizeof(WmoChainEnt));
  leaf->chain[0].trace = trace;
  leaf->chain[0].face = face;
  leaf->n_chain++;
}

// Create a jump exit and PREPEND it at its start node's outgoing list,
// the faithful RumpfSprungeintragSetzen "Ausgangsseitige Verkettung"
// (DSBaumOperationen.c :293-295) -- every WM jump is head-inserted at
// creation, so a node's exit list reads back as the reverse of its
// jumps' creation order.  No dedupe: WM creates each jump exactly once
// per construction event and never merges (AltesBlattPolieren issues a
// fresh parallel even when a structurally equal jump to a different leaf
// is present).
// Gated jump-construction trace (env THVM_WMO_CT): logs each jump's start
// node depth, target leaf rule-trace, and emission mechanism so the
// head-prepend vs AltesBlattPolieren-splice routing for a specific rule
// pair (e.g. SKIToBCKW @1868 W/Y) can be inspected.  Off by default.
static void wmo_ct(const char *mech, WmoNode *start, void *ziel, u8 ziel_leaf) {
  static int on = -1;
  if (on < 0) on = (getenv("THVM_WMO_CT") != NULL) ? 1 : 0;
  if (!on) return;
  u32 tr = 0xffffffffu;
  if (ziel_leaf && ziel != NULL) {
    WmoLeaf *l = (WmoLeaf *)ziel;
    if (l->n_chain > 0u) tr = l->chain[0].trace;
  }
  fprintf(stderr, "WMOCT %s depth=%u leaf=%u trace=%u\n", mech,
          wmo_node_depth(start), ziel_leaf, tr);
}

// WM-faithful jump dedup (DSBaumOperationen.c RumpfSprungeintragSetzen via
// the mirror's _add_entry :88-90): a jump with the same (sub cells, target)
// already at this start node is never re-created.  Returns the existing entry,
// or NULL if none.  Only consulted on the WM-faithful construction path.
static WmoEntry *wmo_jump_find(const WmoNode *start, const WmoCell *sub,
                               u32 sub_len, const void *ziel, u8 ziel_leaf) {
  for (WmoEntry *e = start->exits; e != NULL; e = e->next) {
    if (e->ziel == ziel && e->ziel_leaf == ziel_leaf &&
        e->sub_len == sub_len &&
        memcmp(e->sub, sub, (size_t)sub_len * sizeof(WmoCell)) == 0)
      return e;
  }
  return NULL;
}

static u8 g_wmo_polier_after = 0u;

static WmoEntry *wmo_jump_prepend(WmoNode *start, const WmoCell *sub,
                                  u32 sub_len, void *ziel, u8 ziel_leaf) {
  if (g_wmo_polier_after) {
    WmoEntry *dup = wmo_jump_find(start, sub, sub_len, ziel, ziel_leaf);
    if (dup != NULL) return dup;
  }
  wmo_ct("PREPEND", start, ziel, ziel_leaf);
  WmoEntry *e = (WmoEntry *)calloc(1, sizeof(WmoEntry));
  e->start = start;
  e->sub = (WmoCell *)malloc(sub_len * sizeof(WmoCell));
  memcpy(e->sub, sub, sub_len * sizeof(WmoCell));
  e->sub_len = sub_len;
  e->ziel = ziel;
  e->ziel_leaf = ziel_leaf;
  e->next = start->exits;
  start->exits = e;
  return e;
}

// The WM jump stack (Sprungstapel, DSBaumOperationen.c :200-260): the
// pending parent subterm jumps still open during a descent.  Each entry
// records its start node, the subterm's start position in the key, and a
// remaining-cell counter (Zaehler) driving the close-on-zero pops that
// emit RumpfSprungeintragSetzen jumps in WM's exact left-to-right order.
// Index 0 is a dummy with Zaehler 0 (so `!(--TopZaehler)` short-circuits
// safely at the root); Push raises the index, Pop lowers it.  A parallel
// counter (zaehler2) mirrors the Stapel2 used by NeueSpruengeInsAlteBlatt.
typedef struct {
  WmoNode *start;     // Startknoten
  u32      pos;       // start position of the subterm in the key
  u32      zaehler;   // remaining subterms to close (TopZaehler)
  u32      zaehler2;  // Stapel2 mirror (TopZaehler2)
} WmoStapelEnt;

typedef struct {
  WmoStapelEnt e[WMO_MAX_CELLS + 1u];
  u32          idx;    // StapelIndex (main, drives zaehler)
  u32          idx2;   // StapelIndex2 (Stapel2, drives zaehler2)
} WmoStapel;

// NeuesBlattEinhaengen (DSBaumOperationen.c :444-484): hang a fresh leaf
// `leaf` at `node` keyed by its intro cell key[i], then drain the jump
// stack for every pending subterm closing inside the new leaf's suffix,
// emitting a RumpfSprungeintragSetzen jump into the leaf.  `untergrenze`
// is the Stapeluntergrenze: EintragEins(1) leaves the root pending on the
// stack (the main-walk hang), EintragUngueltig(0) drains all the way to
// the dummy (the BlattAufgeteilt new-leaf hang, where the chain owns the
// pendings).  Cells: the new leaf hangs at the node covering cell i,
// keyed by key[i]; its suffix is key[i+1 ..].  A pending subterm starting
// at position p closes at cell wmo_sub_end(key,p).
static void wmo_blatt_einhaengen(WmoStapel *stk, WmoNode *node,
                                 const WmoCell *key, u32 i,
                                 WmoLeaf *leaf, u32 untergrenze) {
  const WmoCell *sym = &key[i];
  if (!sym->is_var && sym->arity > 0u) {                       // Push intro
    u32 ix = ++stk->idx;
    stk->e[ix].start = node; stk->e[ix].pos = i;
    stk->e[ix].zaehler = sym->arity;
  } else {
    // Constant intro: may itself be a short jump (function symbol, not at
    // the root) reaching this leaf (:466-467).
    if (!sym->is_var && i > 0u)
      wmo_jump_prepend(node, sym, 1u, leaf, 1u);
    // Then continue draining the stack as the constant closes its parents
    // (:468-469).  TopZaehler -= 1 per the consumed leaf cell.
    while (stk->idx > 0u && --stk->e[stk->idx].zaehler == 0u &&
           stk->idx > untergrenze) {
      WmoStapelEnt *top = &stk->e[stk->idx];
      wmo_jump_prepend(top->start, key + top->pos,
                       wmo_sub_end(key, top->pos) - top->pos, leaf, 1u);
      stk->idx--;
    }
  }
  // Suffix cells key[i+1 ..]: each closes parents, popping into the leaf
  // (:475-481).
  for (u32 p = i + 1u; p < leaf->key_len; p++) {
    if (stk->idx == 0u) break;
    stk->e[stk->idx].zaehler += key[p].arity;
    while (stk->idx > 0u && --stk->e[stk->idx].zaehler == 0u &&
           stk->idx > untergrenze) {
      WmoStapelEnt *top = &stk->e[stk->idx];
      wmo_jump_prepend(top->start, key + top->pos,
                       wmo_sub_end(key, top->pos) - top->pos, leaf, 1u);
      stk->idx--;
    }
  }
}

// NeueSpruengeInsAlteBlatt (DSBaumOperationen.c :565-586): the chain's
// jumps that lead into the OLD leaf, driven by the saved Stapel2 counters
// (Stapel2Sichern copied the live main-stack counters into zaehler2 before
// the new-leaf hang consumed the main stack).  Walks the OLD leaf's cells
// (old->key) from the branch cell old->key[j] onward.  `vorg` is the chain
// node directly above the old leaf (covers cell j); `last` is that node;
// stk->e[*].start are the chain nodes already recorded by the GleichPfad
// double-pushes.
static void wmo_neue_spruenge(WmoStapel *stk, WmoLeaf *old, WmoNode *last,
                              u32 j, u32 anc_base) {
  const WmoCell *bk = old->key;
  const WmoCell *intro = &bk[j];
  if (!intro->is_var && intro->arity > 0u) {            // Push2 intro (:571)
    u32 ix = ++stk->idx2;
    stk->e[ix].start = last; stk->e[ix].pos = j;
    stk->e[ix].zaehler2 = intro->arity;
  } else {
    if (!intro->is_var)
      wmo_jump_prepend(last, intro, 1u, old, 1u);
    while (stk->idx2 > anc_base && --stk->e[stk->idx2].zaehler2 == 0u) {
      WmoStapelEnt *top = &stk->e[stk->idx2];
      wmo_jump_prepend(top->start, bk + top->pos,
                       wmo_sub_end(bk, top->pos) - top->pos, old, 1u);
      stk->idx2--;
    }
  }
  for (u32 p = j + 1u; p < old->key_len; p++) {
    if (stk->idx2 <= anc_base) break;
    stk->e[stk->idx2].zaehler2 += bk[p].arity;
    while (stk->idx2 > anc_base && --stk->e[stk->idx2].zaehler2 == 0u) {
      WmoStapelEnt *top = &stk->e[stk->idx2];
      wmo_jump_prepend(top->start, bk + top->pos,
                       wmo_sub_end(bk, top->pos) - top->pos, old, 1u);
      stk->idx2--;
    }
  }
}

// AltesBlattPolieren (DSBaumOperationen.c :492-562): rework the old leaf's
// surviving in-jumps after the split.  For each in-jump whose subterm
// reaches past the split into the old suffix (closes at cell > j, so it
// "still lands in a leaf"), build a PARALLEL jump to the new leaf inserted
// immediately AFTER the surviving jump in the start node's outgoing list
// (the if-branch :503-530).  Otherwise the subterm closes inside the new
// chain (cell e with i < e <= j): re-target the existing in-jump in place
// onto the chain node covering cell e, preserving its outgoing-list
// position (the else-branch :534-560 touches only Zielknoten and the
// Sprungeingaenge lists, never Sprungausgaenge).  node_at[m] covers cell
// i+1+m.
static void wmo_altes_blatt_polieren(WmoTree *t, WmoLeaf *old, WmoLeaf *leaf,
                                     WmoNode **node_at, u32 i, u32 j,
                                     u8 polier_after) {
  // Snapshot the in-jumps to `old` before mutation (a tree walk; the trie
  // keeps no incoming list, so gather by target).  Outgoing-list order at
  // each node is preserved because the parallel is spliced right after its
  // model and the re-target touches no Sprungausgaenge.
  typedef struct { WmoNode *n; WmoEntry *e; } Hit;
  Hit hits[1024];
  u32 n_hits = 0;
  WmoNode *stack[2048];
  u32 sp = 0;
  if (t->root) stack[sp++] = t->root;
  while (sp > 0u) {
    WmoNode *n = stack[--sp];
    for (WmoEntry *e = n->exits; e != NULL; e = e->next) {
      if (e->ziel == (void *)old && n_hits < 1024u) {
        hits[n_hits].n = n; hits[n_hits].e = e; n_hits++;
      }
    }
    for (u32 kk = 0; kk < n->n_kids; kk++) {
      if (!n->kids[kk].is_leaf && sp < 2048u)
        stack[sp++] = (WmoNode *)n->kids[kk].child;
    }
  }
  for (u32 h = 0; h < n_hits; h++) {
    WmoNode *sn = hits[h].n;
    WmoEntry *surv = hits[h].e;
    u32 start_pos = wmo_node_depth(sn);
    // PositionNachTeilterm of the surviving in-jump = the old subterm's
    // close cell (e_old).  WM's if/else split (:499) is PositionNachTeilterm
    // vs the new suffix position j+1: a subterm closing past the split
    // (e_old > j) "still lands in a leaf" -> parallel into the new leaf; one
    // closing inside the chain (e_old <= j) is re-targeted onto the chain
    // node.  WM-faithful: PositionNachTeilterm is the survivor's OWN stored
    // extent (start_pos + sub_len), the mirror's `land = start_pos +
    // len(ent.sub)` (:204/:230), not a re-walk of the old key from start_pos
    // -- a short jump (constant / partial subterm) closes EARLIER than the
    // natural subterm end, so re-walking the key would mis-place it.  OFF
    // keeps the historical key re-walk (byte-identical).
    u32 e_old = polier_after ? (start_pos + surv->sub_len)
                             : wmo_sub_end(old->key, start_pos);
    if (e_old > j) {
      // if-branch (:503-530): a fresh parallel jump to the NEW leaf,
      // carrying the NEW leaf's subterm at this start (its own length,
      // walking TermIndexNeu to its end :517-520).
      u32 e_new = wmo_sub_end(leaf->key, start_pos);
      WmoEntry *par = (WmoEntry *)calloc(1, sizeof(WmoEntry));
      par->start = sn;
      par->sub = (WmoCell *)malloc((e_new - start_pos) * sizeof(WmoCell));
      memcpy(par->sub, leaf->key + start_pos,
             (e_new - start_pos) * sizeof(WmoCell));
      par->sub_len = e_new - start_pos;
      par->ziel = leaf;
      par->ziel_leaf = 1u;
      // Outgoing-list placement.  WM's BlattAufgeteilt parallel for a genuine
      // enclosing subterm goes immediately AFTER the model survivor
      // (DSBaumOperationen.c :521-525, the NaechsterZieleintrag splice --
      // "hinter den Eintrag setzen, zu dem eine Parallele aufgebaut wird").
      // The ONE exception is the NeuesBlattEinhaengen RumpfSprungeintragSetzen
      // jump (:466-467), which is head-inserted: it is emitted for the
      // enclosing subterm whose first cell is the leaf-found branch key[i] --
      // an IMMEDIATE strict ancestor (i - start_pos == 1) that closes inside
      // the new leaf (e_new < e_old) -- and WM consults this most-general
      // outgoing jump FIRST.  This head-insert is WM-faithful in BOTH
      // construction modes: it reproduces WM's CPNr/FIFO order regardless of
      // the AltesBlattPolieren splice convention (BooleanAxioms OrAssociativity
      // @300; soa rule-44 partner batch @1884, where the repeated-var partner's
      // immediate-ancestor parallel must precede the distinct-var partner's).
      // The polier_after splice-after convention applies only to enclosing
      // subterms opened FURTHER above (i - start_pos > 1, the MeredithAxioms
      // And/OrAssoc @340 R27 jump start_pos=2 i=4 j=5 + the soa rule-35 @1953
      // batch), which are genuine parallels and go after the survivor.
      // Var-var twin branches (both leaves diverge on VARIABLE cells,
      // e.g. the OrAssociativity 655/658 f(x0,x1)/f(x1,x0) pair) are NOT
      // the NeuesBlattEinhaengen enclosing-subterm jump -- WM keeps the
      // older twin's exit first there (@2961) -- so the head-insert is
      // restricted to branches where at least one side is a function cell.
      // WM ground truth (WMJUMP dump, rule 658 split on OrAssociativity):
      // an AltesBlattPolieren parallel ALWAYS splices AFTER the survivor
      // (DSBaumOperationen.c :527-530); WM's head-inserted jumps come from
      // the NeuesBlattEinhaengen hang path (RumpfSprungeintragSetzen
      // macro), not from split parallels.  The former local-predicate
      // head exception (i-start_pos==1 && e_new<e_old, the @300/@1884
      // plain-wmcli cases) mis-ordered the OrAssociativity 655/658 twin
      // exits under -auto; if a case later needs the head jump, port the
      // hang-pending stack mechanism rather than a local predicate.
      {
        // WM-faithful (DSBaumOperationen.c :523-526): splice the parallel
        // immediately AFTER the survivor it parallels ("hinter den Eintrag
        // setzen").  Under the historical (non-faithful) construction this
        // ran only for the non-immediate-ancestor parallels; the faithful
        // construction extends it to all but the immediate-ancestor head jump.
        wmo_ct("POLIER-PAR-AFTER", sn, leaf, 1u);
        par->next = surv->next;
        surv->next = par;
      }
    } else {
      // else-branch (:534-560): re-target the survivor in place onto the
      // chain node covering cell e_old, preserving its outgoing-list
      // position (only Zielknoten / Sprungeingaenge change).
      WmoNode *chain_node = node_at[e_old - (i + 1u)];
      surv->ziel = chain_node;
      surv->ziel_leaf = 0u;
      // A STRICT-ANCESTOR subterm closing at the FIRST chain node (e_old ==
      // i+1; its last cell is the leaf-found branch key[i], shared by both
      // leaves) additionally gets a FRESH jump to that chain node, head-
      // inserted at the start node -- the SprungeintragSetzenVonPopOhne-
      // PraefixverweisNULL the unbounded BlattAufgeteilt chain pop emits for
      // the fully closed enclosing subterm (DSBaumOperationen.c :644-647) so
      // the more-general jump is consulted first.  This applies only when the
      // GleichPfad chain has interior nodes beyond the first (j > i+1): a
      // minimal split (j == i+1) builds no enclosing chain and keeps the
      // re-target alone.  WolframAxioms Commutativity / DoubleNegation prefix
      // @91 needs the fresh head jump; CombinatorAxioms SKIToBCKW @303 needs
      // the minimal split to keep its re-target order.
      //
      // The split branch (key[j], old->key[j]) must reach the two leaves via
      // distinct EDGES, not a shared jump.  When both branch cells are
      // FUNCTION symbols the leaves sit on direct exact-symbol children
      // (MitSelbemSymbolAb), so the ancestor's chain-node jump never gates
      // their relative DFS order -- the fresh head insert would only mis-rank
      // the equal-weight CPs born from those leaves (CombinatorAxioms
      // SKIToBCKW @1113: the S(SW)* vs S(SY)* sibling batch, where rule 564's
      // overlaps against the W-branch must precede the Y-branch, matching WM's
      // CPNr/FIFO order).  The same mis-rank arises when both branch cells are
      // VARIABLES: the two leaves sit on var children consulted (newest var
      // first) before any jump exit, so the ancestor chain-node jump again
      // does not gate their DFS order, and a head-inserted fresh jump only
      // pulls the wider-prefix (distinct-var) subtree ahead of the
      // narrower-prefix (repeated-var) one -- the MeredithAxioms And/Or
      // Associativity @166 (rule)x((x.y) vs (x.x)) tops batch, where WM's
      // CPNr/FIFO forms the repeated-var partner first.  Keep the fresh jump
      // only for a MIXED function/variable branch, where it is load-bearing
      // (WolframAxioms Commutativity / DoubleNegation prefix @91).
      u8 new_fun = (j < leaf->key_len) && !leaf->key[j].is_var;
      u8 old_fun = (j < old->key_len) && !old->key[j].is_var;
      wmo_ct((start_pos < i && e_old == i + 1u && j > i + 1u && (new_fun != old_fun))
             ? "POLIER-FRESH-HEAD" : "POLIER-ELSE-RETARGET", sn, leaf, 1u);
      if (start_pos < i && e_old == i + 1u && j > i + 1u
          && (new_fun != old_fun)) {
        WmoEntry *fresh = (WmoEntry *)calloc(1, sizeof(WmoEntry));
        fresh->start = sn;
        fresh->sub = (WmoCell *)malloc((e_old - start_pos) * sizeof(WmoCell));
        memcpy(fresh->sub, leaf->key + start_pos,
               (e_old - start_pos) * sizeof(WmoCell));
        fresh->sub_len = e_old - start_pos;
        fresh->ziel = chain_node;
        fresh->ziel_leaf = 0u;
        fresh->next = sn->exits;
        sn->exits = fresh;
      }
    }
  }
}

static void wmo_tree_insert(WmoTree *t, const WmoCell *key, u32 key_len,
                            u32 depth, u32 trace, u8 face, u8 polier_after) {
  if (key_len == 0u || key_len > WMO_MAX_CELLS) return;
  if (t->root == NULL) t->root = wmo_node_new(NULL);
  WmoNode *node = t->root;
  u32 i = 0;
  WmoStapel stk;
  stk.idx = 0u;
  stk.idx2 = 0u;
  stk.e[0].start = NULL; stk.e[0].pos = 0u;
  stk.e[0].zaehler = 0u; stk.e[0].zaehler2 = 0u;
  while (1) {
    const WmoCell *sym = &key[i];
    u8 child_is_leaf = 0;
    void *child = wmo_kid_get(node, sym, &child_is_leaf);
    if (child == NULL) {
      // NeuesBlattEinhaengen from the main walk (Untergrenze = EintragEins:
      // the root pending stays on the stack, BO_ObjektEinfuegen :724).
      WmoLeaf *leaf = wmo_leaf_new(t, key, key_len, i, depth, node,
                                   trace, face);
      wmo_kid_set(node, sym, leaf, 1u);
      wmo_blatt_einhaengen(&stk, node, key, i, leaf, /*untergrenze=*/1u);
      return;
    }
    if (child_is_leaf) {
      WmoLeaf *old = (WmoLeaf *)child;
      // identical string -> chain prepend (RegelHinzufuegen :343-347)
      if (old->key_len == key_len) {
        u32 k = i;
        while (k < key_len && wmo_cell_eq(&old->key[k], &key[k])) k++;
        if (k == key_len) { wmo_chain_prepend(old, trace, face); return; }
      }
      // ----- BlattAufgeteilt (DSBaumOperationen.c :589-678) -----
      u32 j = i + 1u;
      while (j < key_len && j < old->key_len &&
             wmo_cell_eq(&key[j], &old->key[j])) j++;
      // StapelParallelInitialisieren (:623): reset only the dummy's parallel
      // counter; the main-stack counters survive from the descent.
      stk.e[0].zaehler2 = 0u;
      // Ancestor boundary: pendings that opened ABOVE the leaf-found position
      // (strict ancestors, start cell < i) are already on the stack from the
      // descent.  The chain loop and the new-leaf hang only emit jumps for
      // pendings that close inside the chain or the new leaf; an ancestor
      // whose subterm reaches past the split is reworked instead by
      // AltesBlattPolieren (its pre-existing jump to the old leaf is
      // paralleled / re-targeted), so the new-leaf hang must not also pop it.
      u32 anc_base = stk.idx;
      // GleichPfad: build the chain nodes covering cells i+1 .. j, walking
      // the new key's cells i+1 .. j-1 left to right (the :628-654 loop;
      // GleichPfadEnde = node covering cell j).  PushDoppelt records both
      // the new- and old-side subterm starts at the chain node so the same
      // pending serves the new-leaf hang (main stack) and the later
      // NeueSpruengeInsAlteBlatt (Stapel2) drains.
      WmoNode *node_at[WMO_MAX_CELLS];   // node_at[m] covers cell i+1+m
      // The initial GleichPfad node covers cell i+1, reached from `node`
      // (cell i) via the leaf-found edge key[i].  The caller's
      // BK_NachfolgenLassen sets that edge; key[i] is NOT (re)processed in
      // the chain loop -- if it is a non-nullary function it was already
      // pushed onto the main stack during the descent.
      WmoNode *first = wmo_node_new(node);
      wmo_kid_set(node, sym, first, 0u);
      node_at[0] = first;
      WmoNode *cur = first;            // GleichPfadEnde, covers cell p
      // Chain loop (:628-654): process cells key[i+1] .. key[j-1].  Each
      // iteration creates NeuerKnoten (cell p+1) under cur (cell p) keyed by
      // key[p], then handles the closure of key[p].
      for (u32 p = i + 1u; p < j; p++) {
        const WmoCell *c = &key[p];
        WmoNode *nxt = wmo_node_new(cur);
        wmo_kid_set(cur, c, nxt, 0u);
        node_at[p - i] = nxt;          // covers cell p+1
        if (!c->is_var && c->arity > 0u) {
          // PushDoppelt (:637): pending starts at cur (the node covering cell
          // p), counter over the new key.  Stapel2Sichern (:657) snapshots
          // the surviving counters into zaehler2 after this loop.
          u32 ix = ++stk.idx;
          stk.e[ix].start = cur; stk.e[ix].pos = p;
          stk.e[ix].zaehler = c->arity;
        } else {
          // nullary chain cell.  Constant short jump cur -> nxt (:640-643).
          if (!c->is_var)
            wmo_jump_prepend(cur, c, 1u, nxt, 0u);
          // pop chain pendings that close here (:644-647); Ziel = nxt.  Stop
          // at the ancestor boundary: an ancestor closing inside the chain is
          // re-targeted by AltesBlattPolieren, not duplicated here.
          while (stk.idx > anc_base && --stk.e[stk.idx].zaehler == 0u) {
            WmoStapelEnt *top = &stk.e[stk.idx];
            wmo_jump_prepend(top->start, key + top->pos,
                             wmo_sub_end(key, top->pos) - top->pos, nxt, 0u);
            stk.idx--;
          }
        }
        cur = nxt;
      }
      WmoNode *last = node_at[j - i - 1u];   // GleichPfadEnde, covers cell j
      // Stapel2Sichern (:262-270, :657): set StapelIndex2 = StapelIndex and
      // copy the surviving main counters into zaehler2.  The main hang then
      // pops via idx; NeueSpruengeInsAlteBlatt walks via the independent idx2
      // so the chain pendings survive for it (Pop and Pop2 move different
      // indices over the same shared array).
      stk.idx2 = stk.idx;
      for (u32 ix = 1u; ix <= stk.idx; ix++)
        stk.e[ix].zaehler2 = stk.e[ix].zaehler;
      // Hang the new leaf: drain the chain pendings (down to anc_base) via
      // the new key's suffix (:658, EintragUngueltig but bounded by the
      // ancestor frontier; the strict ancestors are AltesBlattPolieren's).
      // NOTE (2026-07-05, measured): WM's split hang drains UNBOUNDED
      // (Stapeluntergrenze = EintragUngueltig, DSBaumOperationen.c:680),
      // popping strict-ancestor pendings into head-inserted new-leaf
      // jumps.  Porting that here (untergrenze 0) was A/B-refuted: it
      // reorders the initial-axiom CP batches of the split-heavy
      // combinator/Ring trees (CombinatorAxioms BCKWToSKI c2 firstdiv
      // 109->1, Ring family ~110, BooleanAxioms AndAssociativity
      // 605-identity lost) because thvm's descent counters differ from
      // WM's around the skipped leaf-found cell.  The BooleanAxioms @300
      // twin order needs no split-hang change: with the WM-faithful
      // IR-Links-before-FaktumEinfuegen registration order (_.c
      // activation site) the new fact plain-hangs into the retired
      // victim's slot and the MAIN-WALK hang emits the head jump.
      WmoLeaf *leaf = wmo_leaf_new(t, key, key_len, j, depth, last,
                                   trace, face);
      if (j < key_len) wmo_kid_set(last, &key[j], leaf, 1u);
      old->hang = j;
      old->parent = last;
      if (j < old->key_len) wmo_kid_set(last, &old->key[j], old, 1u);
      wmo_blatt_einhaengen(&stk, last, key, j, leaf, /*untergrenze=*/anc_base);
      // AltesBlattPolieren (:665) then NeueSpruengeInsAlteBlatt (:666).
      wmo_altes_blatt_polieren(t, old, leaf, node_at, i, j, polier_after);
      wmo_neue_spruenge(&stk, old, last, j, anc_base);
      return;
    }
    // descend into inner node (BO_ObjektEinfuegen :707-719)
    if (!sym->is_var && sym->arity > 0u) {
      u32 ix = ++stk.idx;
      stk.e[ix].start = node; stk.e[ix].pos = i;
      stk.e[ix].zaehler = sym->arity;
    } else {
      // nullary: close parents whose subterm ends here.  The jumps go into
      // the already-existing inner node, so WM emits none (:711-713) -- just
      // pop.  The root pending is never popped (a further symbol follows).
      while (stk.idx > 0u && --stk.e[stk.idx].zaehler == 0u) stk.idx--;
    }
    node = (WmoNode *)child;
    i++;
  }
}

// ---------- removal ----------

static WmoLeaf *wmo_find_leaf(WmoTree *t, const WmoCell *key, u32 key_len,
                              u32 trace, u8 face) {
  for (WmoLeaf *l = t->ll_head; l != NULL; l = l->ll_next) {
    if (l->key_len != key_len) continue;
    u32 k = 0;
    while (k < key_len && wmo_cell_eq(&l->key[k], &key[k])) k++;
    if (k != key_len) continue;
    for (u32 c = 0; c < l->n_chain; c++) {
      if (l->chain[c].trace == trace && l->chain[c].face == face) return l;
    }
  }
  return NULL;
}

static void wmo_leaf_free(WmoLeaf *l) {
  free(l->key);
  free(l->chain);
  free(l);
}

static void wmo_node_free_one(WmoNode *n) {
  WmoEntry *e = n->exits;
  while (e != NULL) {
    WmoEntry *nx = e->next;
    free(e->sub);
    free(e);
    e = nx;
  }
  free(n->kids);
  free(n);
}

static void wmo_tree_remove(WmoTree *t, const WmoCell *key, u32 key_len,
                            u32 trace, u8 face) {
  WmoLeaf *leaf = wmo_find_leaf(t, key, key_len, trace, face);
  if (leaf == NULL) return;
  // chain removal
  for (u32 c = 0; c < leaf->n_chain; c++) {
    if (leaf->chain[c].trace == trace && leaf->chain[c].face == face) {
      memmove(leaf->chain + c, leaf->chain + c + 1,
              (leaf->n_chain - c - 1u) * sizeof(WmoChainEnt));
      leaf->n_chain--;
      break;
    }
  }
  if (leaf->n_chain > 0u) return;
  // leaf dies
  wmo_ll_remove(t, leaf);
  wmo_kill_entries_to(t, leaf);
  WmoNode *parent = leaf->parent;
  u8  removed_isvar = leaf->key[leaf->hang].is_var;
  u32 removed_sym   = leaf->key[leaf->hang].sym;
  wmo_kid_del(parent, &leaf->key[leaf->hang]);
  if (parent == t->root) {
    wmo_leaf_free(leaf);
    if (t->root->n_kids == 0u) {
      wmo_node_free_one(t->root);
      t->root = NULL;
    }
    return;
  }
  if (parent->n_kids == 1u && parent->kids[0].is_leaf) {
    // collapse (BO_ObjektEntfernen Schrumpfen)
    WmoLeaf *sib = (WmoLeaf *)parent->kids[0].child;
    WmoNode *freed[WMO_MAX_CELLS];
    u32 n_freed = 0;
    WmoNode *node = parent;
    WmoNode *up = NULL;
    while (1) {
      freed[n_freed++] = node;
      up = node->parent;
      // delete node's edge in up
      for (u32 k = 0; k < up->n_kids; k++) {
        if (up->kids[k].child == (void *)node) {
          up->kids[k] = up->kids[up->n_kids - 1u];
          up->n_kids--;
          break;
        }
      }
      if (up == t->root || up->n_kids > 0u) break;
      node = up;
      if (n_freed >= WMO_MAX_CELLS) break;
    }
    u32 new_hang = sib->hang - n_freed;
    sib->hang = new_hang;
    sib->parent = up;
    wmo_kid_set(up, &sib->key[new_hang], sib, 1u);
    // exits of freed nodes die with the nodes; entries targeting freed
    // nodes rewire to the sibling -- position preserved everywhere except
    // at `up`, where the re-issued short jump heads the exit list
    // (see wmo_rewire_cb).
    for (u32 f = 0; f < n_freed; f++) {
      WmoRewireCtx rc = { freed[f], sib, 1u, up };
      wmo_walk_entries(t->root, wmo_rewire_cb, &rc);
    }
    // Sprung-compressed-leaf re-issue: `sib` re-hangs at `up` sharing it with
    // the OTHER leaf children (its model siblings), restoring the
    // AltesBlattPolieren parallel order of `sib`'s ancestor long jumps.  Two
    // cases per how WM built `sib`'s leaf:
    //   - ONE model (a binary split): the deep-hung leaf was an
    //     AltesBlattPolieren split of the lone model's Sprung-compressed leaf,
    //     so each ancestor jump was spliced AFTER the model's own jump
    //     (DSBaumOperationen.c :521-526, "hinter den Eintrag setzen") ->
    //     wmo_sprung_reissue_cb after-model placement.
    //   - SEVERAL models and `sib` is the newest leaf of the pure fan: its
    //     ancestor long jumps were the most-recent RumpfSprungeintragSetzen
    //     head-inserts (:293-295), so each heads its start node's exit list
    //     ahead of the older model-group jumps -> wmo_sprung_reissue_multi_cb
    //     head placement (only where reaching the head crosses no same-group
    //     jump; see that callback).
    {
      WmoLeaf *models[WMO_MAX_VARS + 2u];
      u32 n_models = 0;
      u32 sib_trace = sib->n_chain ? sib->chain[0].trace : 0u;
      u8  sib_newest = 1u;            // sib is the most-recently-born leaf kid
      for (u32 k = 0; k < up->n_kids; k++) {
        if (up->kids[k].is_leaf && up->kids[k].child != (void *)sib &&
            n_models < WMO_MAX_VARS + 2u) {
          WmoLeaf *m = (WmoLeaf *)up->kids[k].child;
          models[n_models++] = m;
          if (m->n_chain && m->chain[0].trace > sib_trace) sib_newest = 0u;
        }
      }
      WmoReissueCtx ric = { sib, models, n_models, wmo_node_depth(up) };
      static int wmo_ct_dbg = -1;
      if (wmo_ct_dbg < 0) wmo_ct_dbg = getenv("THVM_WMO_CT") != NULL;
      if (wmo_ct_dbg) {
        fprintf(stderr, "WMOCOLLAPSE sib_uid=%u n_models=%u sib_newest=%u path=%s\n",
                sib->n_chain ? sib->chain[0].trace : 0xffffffffu,
                n_models, (unsigned)sib_newest,
                n_models == 1u ? "single" :
                (n_models >= 2u && sib_newest) ? "multi" : "NONE");
      }
      if (n_models == 1u) {
        wmo_walk_entries(t->root, wmo_sprung_reissue_cb, &ric);
      } else if (n_models >= 2u && sib_newest) {
        wmo_walk_entries(t->root, wmo_sprung_reissue_multi_cb, &ric);
      }
    }
    for (u32 f = 0; f < n_freed; f++) wmo_node_free_one(freed[f]);
  } else if (parent->n_kids >= 2u && removed_isvar) {
    // No path-shrink (the removed leaf was not the only child).  WM's
    // SprunglistenBereinigenEinfach middle-branch (BO_ObjektEntfernen
    // :1043-1046) re-cleans against the parent's smallest-symbol child.
    // Restrict to the genuine MIDDLE-variable case the AltesBlattPolieren
    // parallel order depends on: every surviving leaf child is a variable
    // edge (a pure-variable branch, as a single-operator theory produces),
    // and the removed variable id sits strictly between the smallest and
    // largest surviving ids (neither min nor max symbol).  The surviving
    // smallest-symbol leaf `minc` (largest first-occurrence id = most
    // negative WM code) is the fresh-hang child whose head-prepended
    // ancestor jumps must move AFTER its variable siblings' jumps.
    WmoLeaf *minc = NULL;
    u32      minc_sym = 0;      // largest surviving id (smallest WM symbol)
    u32      lo_sym   = 0;      // smallest surviving id (largest WM symbol)
    WmoLeaf *sibs[WMO_MAX_VARS + 2u];
    u32      n_sibs = 0;
    u32      n_leaf_kids = 0;
    u8       all_var = 1u;
    for (u32 k = 0; k < parent->n_kids; k++) {
      // A pure-variable branch: every edge (to leaf or inner node) is a
      // variable edge.  Any function edge disqualifies the reorder.
      if (!parent->kids[k].sym.is_var) { all_var = 0u; break; }
      if (!parent->kids[k].is_leaf) continue;
      u32 sym = parent->kids[k].sym.sym;
      if (n_leaf_kids == 0u || sym > minc_sym) {
        minc = (WmoLeaf *)parent->kids[k].child;
        minc_sym = sym;
      }
      if (n_leaf_kids == 0u || sym < lo_sym) lo_sym = sym;
      n_leaf_kids++;
    }
    if (all_var && n_leaf_kids >= 2u && minc != NULL &&
        removed_sym > lo_sym && removed_sym < minc_sym) {
      for (u32 k = 0; k < parent->n_kids; k++) {
        if (parent->kids[k].is_leaf &&
            parent->kids[k].child != (void *)minc &&
            n_sibs < WMO_MAX_VARS + 2u) {
          sibs[n_sibs++] = (WmoLeaf *)parent->kids[k].child;
        }
      }
      WmoMiddleCtx mc = { minc, sibs, n_sibs };
      wmo_walk_entries(t->root, wmo_middle_reissue_cb, &mc);
    }
  }
  wmo_leaf_free(leaf);
}

// ---------- tops DFS (arrival ranking) ----------

// Cell-level unification state: tree vars (stored side, shared along a
// path) and query vars in two binding tables.  Bindings map a var to a
// (cells, len) subterm of the OTHER side.  The tables form a union-find:
// a var slot may point to another var, so a representative is reached by
// following the chain (wmo_deref, modelling the spec's walk_b deref in
// tools/baselines/wm_order_sim/wm_order_sim2.py:121-123).

typedef struct {
  const WmoCell *cells;
  u32 off, len;
  u8 side;          // 0 = query, 1 = stored
} WmoBind;

typedef struct {
  WmoBind tvar[WMO_MAX_VARS + 1u];   // stored/tree vars by canonical id
  WmoBind qvar[WMO_MAX_VARS + 1u];   // query vars by canonical id
  // Highest var-slot index written so far (0 = no binding).  Faces carry
  // canonical first-occurrence numbering, so live var ids are a small
  // prefix (<=5 across the measured WM corpus, table sized for 64).  The
  // DFS copies the binding table per node descent for backtracking;
  // copying only [0..hi] instead of the full 64-slot table cuts the
  // dominant memmove cost.  A read of a slot index > hi is necessarily
  // unbound (hi is the write watermark), so wmo_deref/unify treat it as
  // NULL -- byte-identical to the full-table copy.
  u32 hi;
} WmoUnif;

// Copy only the live prefix [0..src->hi] of both var tables (see WmoUnif.hi).
static inline void wmo_unif_copy(WmoUnif *dst, const WmoUnif *src) {
  u32 hi = (src->hi > WMO_MAX_VARS) ? WMO_MAX_VARS : src->hi;
  u32 n = hi + 1u;
  memcpy(dst->tvar, src->tvar, n * sizeof(WmoBind));
  memcpy(dst->qvar, src->qvar, n * sizeof(WmoBind));
  dst->hi = src->hi;
}

// Extend the watermark to cover var id `vid`, zeroing any slots in the
// gap (old_hi, vid) so every index in [0..hi] is a valid binding cell
// (NULL = unbound).  Without this a later read at a gapped index would
// see uninitialised stack garbage instead of NULL.
static inline void wmo_unif_extend(WmoUnif *u, u32 vid) {
  if (vid <= u->hi) return;
  for (u32 k = u->hi + 1u; k <= vid && k <= WMO_MAX_VARS; k++) {
    u->tvar[k].cells = NULL;
    u->qvar[k].cells = NULL;
  }
  u->hi = vid;
}

// Follow a variable's binding chain to its representative.  Updates
// *cells/*len/*side in place to the deref'd term: either an unbound
// variable (single var cell) or a non-var subterm.  A var whose slot is
// bound to a multi-cell (function) subterm stops there; one bound to
// another single var follows the chain.  WMO_MAX_VARS*2 + 1 iterations
// bound the walk (the union-find has finitely many distinct var slots).
static void wmo_deref(WmoUnif *u, const WmoCell **cells, u32 *len, u8 *side) {
  u32 guard = 2u * (WMO_MAX_VARS + 1u) + 1u;
  while (guard-- != 0u && (*cells)->is_var && *len == 1u) {
    if ((*cells)->sym > u->hi) return;          // beyond watermark = unbound
    WmoBind *slot = (*side == 0u) ? &u->qvar[(*cells)->sym]
                                  : &u->tvar[(*cells)->sym];
    if (slot->cells == NULL) return;            // unbound representative
    *cells = slot->cells + slot->off;
    *len = slot->len;
    *side = slot->side;
  }
}

// Structural unification of two cell slices under the binding tables.
// Depth-bounded; returns 1 on success (tables updated), 0 on failure
// (tables may be partially updated -- callers copy-on-descend).
static u8 wmo_unify_cells(const WmoCell *a, u32 alen, u8 aside,
                          const WmoCell *b, u32 blen, u8 bside,
                          WmoUnif *u, u32 fuel) {
  if (fuel == 0u) return 0u;
  // Deref both sides to their union-find representatives before deciding
  // (the spec's walk_b on each operand, wm_order_sim2.py:131).
  wmo_deref(u, &a, &alen, &aside);
  wmo_deref(u, &b, &blen, &bside);
  u8 a_is_var = a->is_var && alen == 1u;
  u8 b_is_var = b->is_var && blen == 1u;
  if (a_is_var && b_is_var && aside == bside && a->sym == b->sym) return 1u;
  if (a_is_var) {
    wmo_unif_extend(u, a->sym);
    WmoBind *slot = (aside == 0u) ? &u->qvar[a->sym] : &u->tvar[a->sym];
    slot->cells = b; slot->off = 0u; slot->len = blen; slot->side = bside;
    return 1u;
  }
  if (b_is_var) {
    wmo_unif_extend(u, b->sym);
    WmoBind *slot = (bside == 0u) ? &u->qvar[b->sym] : &u->tvar[b->sym];
    slot->cells = a; slot->off = 0u; slot->len = alen; slot->side = aside;
    return 1u;
  }
  if (a->sym != b->sym || a->arity != b->arity) return 0u;
  u32 ai = 1u, bi = 1u;
  for (u32 c = 0; c < a->arity; c++) {
    u32 ae = wmo_sub_end(a, ai), be = wmo_sub_end(b, bi);
    if (!wmo_unify_cells(a + ai, ae - ai, aside, b + bi, be - bi, bside,
                         u, fuel - 1u)) return 0u;
    ai = ae; bi = be;
  }
  return 1u;
}

// Unify the var (canonical id `vid` in table `vside`: 0 = query, 1 = tree)
// against the term slice `t`.  Takes the id (not a raw slot pointer) so it
// can honour the WmoUnif.hi watermark: a slot index > hi is unbound (the
// per-descent copy only carries [0..hi]), and a fresh bind extends hi.
static u8 wmo_unify_var(u8 vside, u32 vid, const WmoCell *t, u32 tlen,
                        u8 tside, WmoUnif *u, u32 fuel) {
  WmoBind *slot = (vside == 0u) ? &u->qvar[vid] : &u->tvar[vid];
  if (vid <= u->hi && slot->cells != NULL) {
    return wmo_unify_cells(slot->cells + slot->off, slot->len, slot->side,
                           t, tlen, tside, u, fuel);
  }
  wmo_unif_extend(u, vid);
  slot->cells = t; slot->off = 0; slot->len = tlen; slot->side = tside;
  return 1u;
}

typedef struct {
  const WmoCell *q;        // query cells
  u32 q_len;
  WmoLeaf **out;           // arrival order, first arrival only
  u32 n_out, cap_out;
} WmoDfs;

static void wmo_dfs_emit(WmoDfs *d, WmoLeaf *leaf) {
  for (u32 k = 0; k < d->n_out; k++) if (d->out[k] == leaf) return;
  if (d->n_out < d->cap_out) d->out[d->n_out++] = leaf;
}

// Unify the leaf rest (stored cells from `si`) against the query
// remainder from `qi` (UnifiziertBis).
static u8 wmo_unify_rest(WmoDfs *d, WmoLeaf *leaf, u32 si, u32 qi,
                         WmoUnif *u) {
  const WmoCell *key = leaf->key;
  while (si < leaf->key_len) {
    if (qi >= d->q_len) return 0u;
    const WmoCell *sc = &key[si], *qc = &d->q[qi];
    if (!sc->is_var) {
      if (!qc->is_var) {
        if (sc->sym != qc->sym) return 0u;
        si++; qi++;
      } else {
        u32 se = wmo_sub_end(key, si);
        if (!wmo_unify_var(0u, qc->sym, key + si, se - si, 1u, u, 64u))
          return 0u;
        si = se; qi++;
      }
    } else {
      u32 qe = wmo_sub_end(d->q, qi);
      if (!wmo_unify_var(1u, sc->sym, d->q + qi, qe - qi, 0u, u, 64u))
        return 0u;
      si++; qi = qe;
    }
  }
  return qi == d->q_len ? 1u : 0u;
}

static int wmo_kid_cmp_vardesc(const void *pa, const void *pb) {
  const WmoKid *a = (const WmoKid *)pa, *b = (const WmoKid *)pb;
  // ascending symbol code = DESCENDING var index (vars are negative in WM)
  return (a->sym.sym < b->sym.sym) ? 1 : (a->sym.sym > b->sym.sym ? -1 : 0);
}

static void wmo_dfs(WmoDfs *d, void *n_raw, u8 is_leaf, u32 qi, u32 spos,
                    const WmoUnif *u, u32 depth_guard) {
  if (depth_guard == 0u) return;
  if (is_leaf) {
    WmoLeaf *leaf = (WmoLeaf *)n_raw;
    WmoUnif u2;
    wmo_unif_copy(&u2, u);
    u32 si = (spos != 0xffffffffu) ? spos : leaf->hang + 1u;
    if (wmo_unify_rest(d, leaf, si, qi, &u2)) wmo_dfs_emit(d, leaf);
    return;
  }
  WmoNode *n = (WmoNode *)n_raw;
  if (qi >= d->q_len) return;
  const WmoCell *qc = &d->q[qi];
  if (!qc->is_var) {
    // exact-symbol child first
    for (u32 k = 0; k < n->n_kids; k++) {
      if (!n->kids[k].sym.is_var && n->kids[k].sym.sym == qc->sym) {
        wmo_dfs(d, n->kids[k].child, n->kids[k].is_leaf, qi + 1u,
                0xffffffffu, u, depth_guard - 1u);
      }
    }
    // var children, descending var index
    WmoKid vkids[32];
    u32 nv = 0;
    for (u32 k = 0; k < n->n_kids && nv < 32u; k++) {
      if (n->kids[k].sym.is_var) vkids[nv++] = n->kids[k];
    }
    if (nv > 1u) qsort(vkids, nv, sizeof(WmoKid), wmo_kid_cmp_vardesc);
    u32 qe = wmo_sub_end(d->q, qi);
    for (u32 k = 0; k < nv; k++) {
      WmoUnif u2;
      wmo_unif_copy(&u2, u);
      if (wmo_unify_var(1u, vkids[k].sym.sym, d->q + qi, qe - qi, 0u,
                        &u2, 64u)) {
        wmo_dfs(d, vkids[k].child, vkids[k].is_leaf, qe, 0xffffffffu, &u2,
                depth_guard - 1u);
      }
    }
  } else {
    // var children first (descending index), then jump exits in order
    WmoKid vkids[32];
    u32 nv = 0;
    for (u32 k = 0; k < n->n_kids && nv < 32u; k++) {
      if (n->kids[k].sym.is_var) vkids[nv++] = n->kids[k];
    }
    if (nv > 1u) qsort(vkids, nv, sizeof(WmoKid), wmo_kid_cmp_vardesc);
    for (u32 k = 0; k < nv; k++) {
      WmoUnif u2;
      wmo_unif_copy(&u2, u);
      WmoCell vcell = vkids[k].sym;
      if (wmo_unify_cells(&d->q[qi], 1u, 0u, &vcell, 1u, 1u, &u2, 64u)) {
        wmo_dfs(d, vkids[k].child, vkids[k].is_leaf, qi + 1u, 0xffffffffu,
                &u2, depth_guard - 1u);
      }
    }
    for (WmoEntry *e = n->exits; e != NULL; e = e->next) {
      WmoUnif u2;
      wmo_unif_copy(&u2, u);
      if (wmo_unify_var(0u, qc->sym, e->sub, e->sub_len, 1u, &u2,
                        64u)) {
        u32 land = wmo_node_depth(e->start) + e->sub_len;
        wmo_dfs(d, e->ziel, e->ziel_leaf, qi + 1u,
                e->ziel_leaf ? land : 0xffffffffu, &u2, depth_guard - 1u);
      }
    }
  }
}

// ---------- public mirror API ----------

static AtpWmOrder *atp_wmo_new(void) {
  AtpWmOrder *w = (AtpWmOrder *)calloc(1, sizeof(AtpWmOrder));
  if (w != NULL) w->no_rankcache = getenv("THVM_WMO_NO_RANKCACHE") ? 1u : 0u;
  return w;
}

// Gated full eq/rule-tree structure dump (THVM_WMO_TREEDUMP): print every
// node's kids (edge cell + child kind) and exit list (sub cells + target +
// order) so thvm's discrimination-tree CONSTRUCTION can be diffed leaf/exit
// by leaf/exit against the validated mirror (wm_trie_mirror.py).  Env-gated,
// off in every normal run (one cached getenv, pure stderr).
static void wmo_treedump_node(FILE *fp, WmoNode *n, u32 depth) {
  for (u32 k = 0; k < n->n_kids; k++) {
    WmoCell *c = &n->kids[k].sym;
    for (u32 d = 0; d < depth; d++) fputc(' ', fp);
    fprintf(fp, "kid[%u] edge=%d %s\n", k,
            c->is_var ? -(int)c->sym : (int)c->sym,
            n->kids[k].is_leaf ? "LEAF" : "node");
    if (n->kids[k].is_leaf) {
      WmoLeaf *l = (WmoLeaf *)n->kids[k].child;
      for (u32 d = 0; d < depth + 2u; d++) fputc(' ', fp);
      fprintf(fp, "leaf_key=[");
      for (u32 cc = 0; cc < l->key_len; cc++)
        fprintf(fp, "%s%d", cc ? "," : "",
                l->key[cc].is_var ? -(int)l->key[cc].sym : (int)l->key[cc].sym);
      fprintf(fp, "] chain=");
      for (u32 cc = 0; cc < l->n_chain; cc++)
        fprintf(fp, "%s(t=%u,f=%u)", cc ? "," : "",
                l->chain[cc].trace, l->chain[cc].face);
      fputc('\n', fp);
    }
  }
  u32 ei = 0;
  for (WmoEntry *e = n->exits; e != NULL; e = e->next, ei++) {
    for (u32 d = 0; d < depth; d++) fputc(' ', fp);
    fprintf(fp, "exit[%u] sub=[", ei);
    for (u32 cc = 0; cc < e->sub_len; cc++)
      fprintf(fp, "%s%d", cc ? "," : "",
              e->sub[cc].is_var ? -(int)e->sub[cc].sym : (int)e->sub[cc].sym);
    fprintf(fp, "] -> %s", e->ziel_leaf ? "LEAF" : "node");
    if (e->ziel_leaf) {
      WmoLeaf *l = (WmoLeaf *)e->ziel;
      fprintf(fp, " key=[");
      for (u32 cc = 0; cc < l->key_len; cc++)
        fprintf(fp, "%s%d", cc ? "," : "",
                l->key[cc].is_var ? -(int)l->key[cc].sym : (int)l->key[cc].sym);
      fprintf(fp, "]");
    }
    fputc('\n', fp);
  }
  for (u32 k = 0; k < n->n_kids; k++) {
    if (!n->kids[k].is_leaf) {
      for (u32 d = 0; d < depth; d++) fputc(' ', fp);
      fprintf(fp, "node[edge=%d]:\n",
              n->kids[k].sym.is_var ? -(int)n->kids[k].sym.sym
                                    : (int)n->kids[k].sym.sym);
      wmo_treedump_node(fp, (WmoNode *)n->kids[k].child, depth + 1u);
    }
  }
}

static void wmo_maybe_treedump(AtpWmOrder *w) {
  static int on = -1;
  if (on < 0) on = getenv("THVM_WMO_TREEDUMP") != NULL ? 1 : 0;
  if (!on) return;
  const char *at = getenv("THVM_WMO_TREEDUMP_AT");
  u32 target = at ? (u32)atoi(at) : 71u;
  static u32 dumped = 0u;
  if (w->n_reg != target || dumped == target) return;
  dumped = target;
  fprintf(stderr, "WMOTREE tree=1 nreg=%u\n", w->n_reg);
  if (w->tree[1].root != NULL) wmo_treedump_node(stderr, w->tree[1].root, 0u);
}

static void wmo_free_subtree(void *n_raw, u8 is_leaf) {
  if (n_raw == NULL) return;
  if (is_leaf) { wmo_leaf_free((WmoLeaf *)n_raw); return; }
  WmoNode *n = (WmoNode *)n_raw;
  for (u32 k = 0; k < n->n_kids; k++) {
    wmo_free_subtree(n->kids[k].child, n->kids[k].is_leaf);
  }
  wmo_node_free_one(n);
}

static void atp_wmo_free(AtpWmOrder *w) {
  if (w == NULL) return;
  for (u32 t = 0; t < 2u; t++) wmo_free_subtree(w->tree[t].root, 0u);
  for (u32 r = 0; r < w->n_reg; r++) free(w->reg[r].cells);
  free(w->reg);
  for (u32 e = 0; e < WMO_RC_N; e++) free(w->rc[e].out);
  free(w);
}

// Env-gated (THVM_WMO_CHECK=1) full structural verifier, run after every
// mirror mutation (register / remove).  Validates the invariants
// the collapse machinery depends on -- every tree leaf's parent/hang/edge
// cell agree, leaf-list membership equals tree membership, every chain
// entry has a registry row and vice versa -- and aborts at the FIRST
// violation, so a silent corruption is caught at its write site instead
// of a later garbage read.
#define WMO_CHK_MAX_LEAVES 8192u

static u32 wmo_chk_node(WmoNode *n, u32 depth, WmoLeaf **out, u32 *n_out,
                        const char *what, u8 tree) {
  u32 bad = 0;
  for (u32 k = 0; k < n->n_kids; k++) {
    WmoKid *kid = &n->kids[k];
    if (kid->is_leaf) {
      WmoLeaf *l = (WmoLeaf *)kid->child;
      if (l->parent != n) {
        fprintf(stderr, "WMOCHK %s tree=%u leaf %p parent %p != node %p\n",
                what, tree, (void *)l, (void *)l->parent, (void *)n);
        bad++;
      }
      if (l->hang != depth) {
        fprintf(stderr, "WMOCHK %s tree=%u leaf %p hang=%u != depth=%u\n",
                what, tree, (void *)l, l->hang, depth);
        bad++;
      }
      if (l->hang >= l->key_len ||
          !wmo_cell_eq(&l->key[l->hang], &kid->sym)) {
        fprintf(stderr, "WMOCHK %s tree=%u leaf %p edge cell mismatch\n",
                what, tree, (void *)l);
        bad++;
      }
      if (*n_out < WMO_CHK_MAX_LEAVES) out[(*n_out)++] = l;
    } else {
      WmoNode *c = (WmoNode *)kid->child;
      if (c->parent != n) {
        fprintf(stderr, "WMOCHK %s tree=%u node %p parent %p != %p\n",
                what, tree, (void *)c, (void *)c->parent, (void *)n);
        bad++;
      }
      bad += wmo_chk_node(c, depth + 1u, out, n_out, what, tree);
    }
  }
  return bad;
}

static void wmo_check(AtpWmOrder *w, const char *what) {
  static int on = -1;
  if (on < 0) on = (getenv("THVM_WMO_CHECK") != NULL) ? 1 : 0;
  if (!on) return;
  static WmoLeaf *tl[WMO_CHK_MAX_LEAVES];
  u32 bad = 0;
  for (u8 t = 0; t < 2u; t++) {
    u32 ntl = 0;
    if (w->tree[t].root != NULL)
      bad += wmo_chk_node(w->tree[t].root, 0u, tl, &ntl, what, t);
    for (WmoLeaf *l = w->tree[t].ll_head; l != NULL; l = l->ll_next) {
      u8 found = 0;
      for (u32 k = 0; k < ntl; k++) if (tl[k] == l) { found = 1u; break; }
      if (!found) {
        fprintf(stderr, "WMOCHK %s tree=%u ll leaf %p (chain0=%u) DETACHED "
                "from tree\n", what, t, (void *)l,
                l->n_chain ? l->chain[0].trace : 0xffffffffu);
        bad++;
      }
    }
    for (u32 k = 0; k < ntl; k++) {
      u8 found = 0;
      for (WmoLeaf *l = w->tree[t].ll_head; l != NULL; l = l->ll_next)
        if (l == tl[k]) { found = 1u; break; }
      if (!found) {
        fprintf(stderr, "WMOCHK %s tree=%u tree leaf %p (chain0=%u) NOT in "
                "leaf list\n", what, t, (void *)tl[k],
                tl[k]->n_chain ? tl[k]->chain[0].trace : 0xffffffffu);
        bad++;
      }
      for (u32 c = 0; c < tl[k]->n_chain; c++) {
        u8 reg = 0;
        for (u32 r = 0; r < w->n_reg; r++) {
          if (w->reg[r].trace == tl[k]->chain[c].trace &&
              w->reg[r].tree == t && w->reg[r].face == tl[k]->chain[c].face) {
            reg = 1u;
            break;
          }
        }
        if (!reg) {
          fprintf(stderr, "WMOCHK %s tree=%u leaf %p chain[%u]=%u.%u has NO "
                  "registry row (garbage?)\n", what, t, (void *)tl[k], c,
                  tl[k]->chain[c].trace, (unsigned)tl[k]->chain[c].face);
          bad++;
        }
      }
    }
    for (u32 r = 0; r < w->n_reg; r++) {
      if (w->reg[r].tree != t) continue;
      u8 found = 0;
      for (u32 k = 0; k < ntl && !found; k++) {
        for (u32 c = 0; c < tl[k]->n_chain; c++) {
          if (tl[k]->chain[c].trace == w->reg[r].trace &&
              tl[k]->chain[c].face == w->reg[r].face) { found = 1u; break; }
        }
      }
      if (!found) {
        fprintf(stderr, "WMOCHK %s tree=%u reg trace=%u face=%u LOST from "
                "tree\n", what, t, w->reg[r].trace, (unsigned)w->reg[r].face);
        bad++;
      }
    }
  }
  if (bad != 0u) {
    fprintf(stderr, "WMOCHK %s FAILED (%u violations)\n", what, bad);
    fflush(stderr);
    abort();
  }
}

static void wmo_register(AtpWmOrder *w, u32 trace, u8 tree, u8 face,
                         const WmoCell *cells, u32 n_cells, u32 depth,
                         u8 dist_rhs) {
  {
    static int ct = -1;
    if (ct < 0) ct = (getenv("THVM_WMO_CT") != NULL) ? 1 : 0;
    if (ct) fprintf(stderr, "WMOREG trace=%u tree=%u face=%u n_cells=%u\n",
                    trace, tree, face, n_cells);
  }
  wmo_tree_insert(&w->tree[tree], cells, n_cells, depth, trace, face,
                  w->polier_after);
  if (w->n_reg == w->cap_reg) {
    w->cap_reg = w->cap_reg ? w->cap_reg * 2u : 64u;
    w->reg = (WmoReg *)realloc(w->reg, w->cap_reg * sizeof(WmoReg));
  }
  WmoReg *r = &w->reg[w->n_reg++];
  r->trace = trace;
  r->tree = tree;
  r->face = face;
  r->dist_rhs = dist_rhs;
  r->cells = (WmoCell *)malloc(n_cells * sizeof(WmoCell));
  memcpy(r->cells, cells, n_cells * sizeof(WmoCell));
  r->n_cells = n_cells;
  w->tree_rev++;             // invalidate the wmo_tops_rank arrival memo
  wmo_maybe_treedump(w);
  {
    char lbl[64];
    snprintf(lbl, sizeof lbl, "post-register trace=%u face=%u", trace, face);
    wmo_check(w, lbl);
  }
}

// Whether WM's distinguished (indexed) face for the fact with birth
// trace id `trace` is thvm's STORED RHS (vs the default LHS).  Recorded
// at registration; queried by the single-walk former + the eq-rank
// wrappers to remap a thvm-face bit (0 = lhs, 1 = rhs) onto WM's face
// (0 = distinguished, 1 = reverse).  Returns 0 for an unregistered trace (orientable rules
// and intake equations keep distinguished = lhs).
static u8 wmo_trace_dist_rhs(AtpWmOrder *w, u32 trace) {
  // Memo probe (see AtpWmOrder.drc): the registry is frozen while
  // tree_rev holds, so a valid same-rev entry returns exactly what the
  // scan below would recompute (including the unregistered-default 0).
  struct WmoDrcEnt *e = NULL;
  if (!w->no_rankcache) {
    e = &w->drc[(trace * 2654435761u) & (WMO_DRC_N - 1u)];
    if (e->valid && e->rev == w->tree_rev && e->trace == trace)
      return e->dr;
  }
  u8 dr = 0u;
  for (u32 r = 0; r < w->n_reg; r++) {
    if (w->reg[r].trace == trace) { dr = w->reg[r].dist_rhs; break; }
  }
  if (e != NULL) {
    e->valid = 1u;
    e->rev   = w->tree_rev;
    e->trace = trace;
    e->dr    = dr;
  }
  return dr;
}

// uid -> WM-distinguished-face label for the engine's face-aware
// KillParent (atp_uid_kill_face in _.c; forward-declared there).
static u8 atp_wmo_dist_rhs_of(AtpState *s, u32 uid) {
  AtpWmOrder *w = (AtpWmOrder *)s->wmo;
  if (w == NULL) return 0u;
  return wmo_trace_dist_rhs(w, uid);
}

// Whether `t` contains no free variables (is a ground term).
static u8 wmo_term_is_ground(Term t) {
  if (t == 0) return 1u;
  if (term_tag(t) == TAG_FVR) return 0u;
  if (term_tag(t) == TAG_CTR) {
    u32 n = term_ctr_n(t);
    for (u32 i = 0; i < n; i++) {
      if (!wmo_term_is_ground(term_ctr_at(t, i))) return 0u;
    }
  }
  return 1u;
}

// Register the faces of the fact at `slot` (call right after
// atp_push_rule commits the slot; the fact must already be oriented).
//
// WM indexes its DISTINGUISHED face (the side it keeps as `links`) in
// the eTT B-phase; the reverse face runs the late E-phase.  WM's
// distinguished face is `selRec->lhs`:
//   - INTAKE axioms: the loader's LRSortieren (WASIC/SpezNormierung.c
//     :517-612), which atp_wm_intake_* already lands on thvm's stored
//     s->lhs -- distinguished = thvm LHS.
//   - CP-DERIVED unorientable equations: the selected CP's left side =
//     `KPLinks = MGU(TP_RechteSeite(Vater))` = sigma(outer-rule RHS)
//     (Unifikation1.c:916).  thvm's CP constructor puts sigma(r_i) on
//     cp.rhs (cp/_.c cp_visit: cp.lhs = sigma(l_i[p<-r_j]) = KPRechts,
//     cp.rhs = sigma(r_i) = KPLinks) and the pop-time normalize keeps
//     that lhs/rhs assignment -- so by DEFAULT WM's distinguished face is
//     thvm's STORED RHS.
//   - GROUND-vs-NON-GROUND CP-derived equations: thvm and WM can orient
//     the same equation from DIFFERENT representative CPs (a given
//     unorientable equation is formed by many overlaps, each with its
//     own KPLinks/KPRechts roles; the one selected FIRST fixes WM's
//     distinguished face).  When one side normalizes to a GROUND term and
//     the other carries a variable, WM's selected representative lands
//     KPLinks on the ground side: the ground side is produced by the
//     Vater whose RHS the absorbing / grounding rule rewrote to a
//     variable-free term, so WM keeps it as `links`.  thvm's stored
//     orientation already matches WM's display (the pop-time normalize
//     lands the ground side as thvm's LHS), but thvm's REPRESENTATIVE has
//     KPLinks on the variable side, so the cp.rhs=KPLinks default would
//     mislabel the distinguished face.  Pin dist_rhs to the ground side
//     here (CommRing ZeroIsAbsorbing eq `and(const2,~k1)=and(x,~k1)`:
//     dist_rhs=0, distinguished = the ground `and(const2,~k1)` LHS).
// The stored orientation is NOT changed (that would perturb CP
// generation via the formation-time KPAction order gate, which keys on
// cp.peak = sigma(l_i)); instead the WM-vs-thvm face flip is recorded
// per-trace as `dist_rhs` and the rank function remaps the face bits.
// `cp_derived` selects the source surface; for mono / symmetric-face
// equations the two faces alpha-renumber to each other, so the choice
// is moot (dist_rhs forced 0).
//
// Sync the WM-faithful trie-construction flag from the AtpState onto the
// AtpWmOrder + the file-static used by wmo_jump_prepend, so every subsequent
// wmo_register insert uses WM's splice-after AltesBlattPolieren + jump dedup
// (see AtpWmOrder.polier_after / AtpState.use_wm_trie_faithful).  Called by the
// setter and before each insert in atp_wmo_insert_fact_ex.
static void atp_wmo_sync_trie_faithful(AtpState *s) {
  if (s == NULL) return;
  AtpWmOrder *w = (AtpWmOrder *)s->wmo;
  if (w != NULL) w->polier_after = s->use_wm_trie_faithful ? 1u : 0u;
  g_wmo_polier_after = s->use_wm_trie_faithful ? 1u : 0u;
}

static void atp_wmo_insert_fact_ex(AtpState *s, u32 slot, u8 cp_derived) {
  AtpWmOrder *w = (AtpWmOrder *)s->wmo;
  if (w == NULL) return;
  w->polier_after = s->use_wm_trie_faithful ? 1u : 0u;
  g_wmo_polier_after = w->polier_after;
  u32 trace = s->r_uid[slot];   // stable identity (r_trace is NONE past the proof-trace cap)
  WmoCell cells[WMO_MAX_CELLS];
  if (s->r_orient[slot]) {
    u32 n = wmo_face_cells(s->lhs[slot], cells, WMO_MAX_CELLS);
    if (n == 0u) return;
    wmo_register(w, trace, 0u, 0u, cells, n, wmo_term_depth(s->lhs[slot]),
                 0u);
    return;
  }
  u8 mono = atp_eq_is_mono(s, slot);
  u8 dist_rhs = (cp_derived && !mono) ? 1u : 0u;
  if (cp_derived && !mono) {
    // Ground-side override (see header): exactly one side ground -> WM's
    // distinguished face is the ground side.
    u8 lhs_ground = wmo_term_is_ground(s->lhs[slot]);
    u8 rhs_ground = wmo_term_is_ground(s->rhs[slot]);
    if (lhs_ground != rhs_ground) {
      dist_rhs = rhs_ground ? 1u : 0u;
    }
  }
  // Tree INSERTION order follows the STORED orientation (lhs face first,
  // rhs face second) -- WM's RUndEVerwaltung GleichungEinfuegen inserts
  // flat(l) then flat(r) (RUndEVerwaltung.c:485-497), and the mirror
  // (wm_trie_mirror.py: ('E',eid,0)=flat(l) then ('E',eid,1)=flat(r))
  // matches.  The CONSTRUCTION (BlattAufgeteilt splice-after) is order
  // sensitive: inserting the rhs face first when dist_rhs=1 inverted the
  // two faces of the soa 3rd axiom at the depth-1 d node, flipping the
  // E29/E30 group exits and the rule-35 round-robin (firstdiv 1953).  The
  // dist_rhs bit STILL governs the rank-key face remap, but only via the
  // chain's stored WM-face LABEL (0 = distinguished = thvm dir XOR dist_rhs),
  // not the insertion order: the lhs leaf carries WM-face = dist_rhs (it is
  // the reverse face when the distinguished side is the rhs), the rhs leaf
  // carries WM-face = !dist_rhs.  wmo_tops_rank/wmo_leaflist_rank look the
  // partner up by that WM-face, so the remap is byte-identical to the prior
  // (dist-first) registration; only the physical tree exit order changes.
  if (s->use_wmo_insert_lr) {
    // WM GleichungEinfuegen order: WM inserts the equation's two faces into
    // the Gleichungsbaum as flat(l) then flat(r) where (l,r) is WM's STORED
    // (LRSortieren-canonical) orientation -- l is the side that is Kleiner
    // under the SpezNormierung side comparator (variable < non-variable,
    // preorder; SpezNormierung.c:517-534).  thvm does NOT reorient a derived
    // equation's stored sides (LRSortieren-on-store regresses CP formation),
    // so the stored lhs is not always WM's l.  Recover WM's insertion order
    // WITHOUT touching the stored orientation: insert the LR-smaller face
    // first.  The chain WM-face LABEL still follows dist_rhs (the partner
    // lookup keys on WM-face = thvm-dir XOR dist_rhs), so the rank is
    // byte-identical to the prior dist-first registration; only the physical
    // tree exit order moves to WM's.  See AtpState.use_wmo_insert_lr.
    Term lhs_face = s->lhs[slot];
    Term rhs_face = s->rhs[slot];
    // atp_lr_sortieren_rec(a,b) > 0 means a is Groesser (should be the rhs);
    // insert the Kleiner side first.  lhs_is_l = stored lhs is WM's l.
    u8 lhs_is_l = (atp_lr_sortieren_rec(lhs_face, rhs_face) <= 0) ? 1u : 0u;
    Term first  = lhs_is_l ? lhs_face : rhs_face;
    Term second = lhs_is_l ? rhs_face : lhs_face;
    // WM-face label of the first (= WM's l) face: it is the distinguished
    // face iff WM's l side carries the distinguished side.  thvm's stored lhs
    // is the distinguished side iff dist_rhs==0; so the lhs face's WM-face is
    // dist_rhs, and the side inserted first carries WM-face = (first is lhs)
    // ? dist_rhs : !dist_rhs.
    u8 first_face  = lhs_is_l ? dist_rhs : (u8)(dist_rhs ^ 1u);
    u8 second_face = (u8)(first_face ^ 1u);
    u32 n = wmo_face_cells(first, cells, WMO_MAX_CELLS);
    if (n == 0u) return;
    wmo_register(w, trace, 1u, first_face, cells, n, wmo_term_depth(first),
                 dist_rhs);
    if (!mono) {
      u32 n2 = wmo_face_cells(second, cells, WMO_MAX_CELLS);
      if (n2 != 0u) {
        wmo_register(w, trace, 1u, second_face, cells, n2,
                     wmo_term_depth(second), dist_rhs);
      }
    }
    return;
  }
  Term dist = dist_rhs ? s->rhs[slot] : s->lhs[slot];
  Term rev  = dist_rhs ? s->lhs[slot] : s->rhs[slot];
  u32 n = wmo_face_cells(dist, cells, WMO_MAX_CELLS);
  if (n == 0u) return;
  wmo_register(w, trace, 1u, 0u, cells, n, wmo_term_depth(dist), dist_rhs);
  if (!mono) {
    u32 n2 = wmo_face_cells(rev, cells, WMO_MAX_CELLS);
    if (n2 != 0u) {
      wmo_register(w, trace, 1u, 1u, cells, n2, wmo_term_depth(rev),
                   dist_rhs);
    }
  }
}

// Intake / rebuild default: distinguished face = stored lhs (LRSortieren).
static void atp_wmo_insert_fact(AtpState *s, u32 slot) {
  atp_wmo_insert_fact_ex(s, slot, 0u);
}

// NOTE: there is deliberately NO identity-rename operation.  The mirror
// keys every fact by its birth uid (r_uid, stable across slot compaction
// AND across the RHS compose -- WM's RMRechtsInterred, Interreduktion.c
// :329-360, right-reduces the rule in place without touching the
// DSBaum).  A former atp_wmo_rename_trace here was called with r_trace
// ids: renaming in TRACE space over uid-keyed chains hijacked any LIVE
// uid the trace id numerically collided with (BooleanAxioms
// OrAssociativity @300, uid 51).

// WM IR-victim drain-order key for the fact with birth trace id `trace`,
// captured BEFORE the victim is removed from the wmo tree.
//
// WM's IR_PufferAuslesen (Interreduktion.c:391) drains the FIFO REPuffer
// that IR_InterreduktionLinks filled by running GMInterred (equation
// victims) THEN RMLinksInterred (rule victims), each via
// RE_forRegelnRobust = BK_forRegelnRobust over Baum.ErstesBlatt ->
// BK_NachfBlatt -- the discrimination-tree LEAF LIST in order, the rule
// chain newest-first within a leaf.  So a stable sort of the buffered
// victims by (equation-before-rule, leaf-list rank) reproduces WM's
// re-queue order, hence the late FIFO ages (w2 = ++CPNr) of the
// re-entered equations.
//
// Key layout: bit 31 = 1 for rule victims (tree 0), 0 for equation
// victims (tree 1) -- equations sort first; the next field is the
// victim's distinguished-face (face 0) leaf-list rank in its tree.
// Removals of EARLIER-captured victims only delete leaves (never
// reorder), so ranks captured at successive push moments preserve the
// pass-start relative order between any two surviving victims.
//
// Within a leaf two victims share a leaf-list rank, so when
// use_drain_chainpos is set the victim's chain index within the leaf is
// folded into the low 8 bits as a tiebreak: WM's BK_forRegelnRobust walks
// each leaf's fact chain head-first following TP_Nachf (DSBaumKnoten.h:
// 482-495), and the chain prepends on insert (newest first), so
// head-first = chain index 0,1,2.. ascending -- exactly this tiebreak.
// (leafrank is shifted up by 8; both ranks are tiny, no overflow.)
//
// GMInterred (use_drain_revface) refinement: WM's REPuffer is NOT filled
// by the distinguished-face leaf list.  GMInterred (Interreduktion.c:
// 285-307) runs RE_forGMReferenzen = BK_ReferenzDurchlauf (DSBaumKnoten.h:
// 499-514), which walks the Gleichungsbaum leaf list and, per leaf, tests
// NF_ObjektAnwendbar(Objekt, .) -- i.e. it pulls each equation through the
// face the new object actually REDUCES, not through its distinguished
// (LRSortieren) face.  WM indexes BOTH directions of an equation (the
// blue/yellow twins), so the two faces sit in DIFFERENT leaves; the victim
// re-enters the FIFO at its REDUCIBLE face's leaf-list position.  For the
// soa cube-mirror pair (`x.x = cube`) the distinguished face is the small
// `x.x` side -- identical for both twins, so face-0 ranking collides them
// into one leaf and falls back to chain order (wrong); their reducible
// cube faces sit in distinct leaves whose leaf-list order is WM's true
// drain order.  reduced_thvm_side is the thvm side (0 lhs / 1 rhs) the new
// rule reduced; the wmo face is reduced_thvm_side ^ dist_rhs (the same
// face remap atp_wmo_eq_*_rank uses, wm_order.c:1834).

// Leaf-list rank (and within-leaf chain index) of the (trace, face) entry
// in `tree`.  Returns 0xffffffff if not found; *out_chain set to the chain
// index when found.
static u32 wmo_face_leafrank(AtpWmOrder *w, u8 tree, u32 trace, u8 face,
                             u32 *out_chain) {
  u32 rank = 0;
  for (WmoLeaf *l = w->tree[tree].ll_head; l != NULL; l = l->ll_next) {
    for (u32 c = 0; c < l->n_chain; c++) {
      if (l->chain[c].trace == trace && l->chain[c].face == face) {
        if (out_chain) *out_chain = c;
        return rank;
      }
    }
    rank++;
  }
  return 0xffffffffu;
}

static u32 atp_wmo_victim_drain_key(AtpState *s, u32 trace, u8 reduced_thvm_side) {
  AtpWmOrder *w = (AtpWmOrder *)s->wmo;
  if (w == NULL) return 0x7fffffffu;
  // Find which tree holds this trace's distinguished (face 0) entry.
  u8 tree = 0u;
  u8 found_tree = 0u;
  for (u32 r = 0; r < w->n_reg; r++) {
    if (w->reg[r].trace == trace && w->reg[r].face == 0u) {
      tree = w->reg[r].tree;
      found_tree = 1u;
      break;
    }
  }
  if (!found_tree) return 0x7fffffffu;
  u32 hi = (tree == 0u ? 1u : 0u) << 31;
  // The face WM pulls the victim through: the reduced side remapped onto
  // WM's distinguished/reverse face numbering.  Off (legacy) = face 0.
  u8 key_face = s->use_drain_revface
                  ? (u8)(reduced_thvm_side ^ wmo_trace_dist_rhs(w, trace))
                  : 0u;
  u32 chain = 0u;
  u32 rank = wmo_face_leafrank(w, tree, trace, key_face, &chain);
  if (rank == 0xffffffffu) {
    // The reduced face is not registered (e.g. a mono equation has only
    // face 0): fall back to the distinguished face.
    rank = wmo_face_leafrank(w, tree, trace, 0u, &chain);
    if (rank == 0xffffffffu) return 0x7fffffffu;
  }
  // Forensic key dump: THVM_ATP_IRV_KEY_DUMP=1 prints the victim's key
  // face + the FULL chain of the leaf it ranked in (uid.face entries,
  // head first), so a mirror-vs-WM chain-order divergence is visible.
  {
    static int key_dump = -1;
    if (key_dump < 0)
      key_dump = getenv("THVM_ATP_IRV_KEY_DUMP") != NULL ? 1 : 0;
    if (key_dump) {
      fprintf(stderr,
              "IRVKEY uid=%u side=%u tree=%u key_face=%u dist_rhs=%u "
              "rank=%u chain=%u leaf:[",
              trace, reduced_thvm_side, tree, key_face,
              wmo_trace_dist_rhs(w, trace), rank, chain);
      u32 rr = 0;
      for (WmoLeaf *l = w->tree[tree].ll_head; l != NULL;
           l = l->ll_next, rr++) {
        if (rr != rank) continue;
        for (u32 c = 0; c < l->n_chain; c++)
          fprintf(stderr, " %u.%u", l->chain[c].trace, l->chain[c].face);
        break;
      }
      fprintf(stderr, " ]\n");
    }
  }
  if (s->use_drain_chainpos) {
    // Fold the within-leaf chain index below the leaf-list rank, so two
    // victims sharing a leaf drain head-first (BK_Regeln -> TP_Nachf,
    // DSBaumKnoten.h:482-495; chain prepends newest-first at
    // RegelHinzufuegen, DSBaumOperationen.c:343-346) like WM.
    u32 cp = (chain > 0xffu) ? 0xffu : chain;
    return hi | (((rank & 0x7fffffu) << 8) | cp);
  }
  return hi | (rank & 0x7fffffffu);
}

// Remove every registered face of the fact with birth trace id `trace`.
// Env-gated (THVM_ATP_WMO_DUMP=1) forensic trie dump: DFS in stored
// kid order, per node the exit list (consult order), per leaf the
// chain (trace/face, newest first).  Diffed against the hand-executed
// WM DSBaum model to pin removal-rewire divergences (the DoubleNegation
// batch-793 group rotations).
static void wmo_dump_node(WmoNode *n, int depth, FILE *out) {
  if (n == NULL) return;
  fprintf(out, "%*sN %p exits:[", depth * 2, "", (void *)n);
  for (WmoEntry *e = n->exits; e != NULL; e = e->next) {
    fprintf(out, " %p->%p%s(sub%u)", (void *)e->start, e->ziel,
            e->ziel_leaf ? "L" : "N", e->sub_len);
  }
  fprintf(out, " ]\n");
  for (u32 k = 0; k < n->n_kids; k++) {
    WmoKid *kid = &n->kids[k];
    if (kid->is_leaf) {
      WmoLeaf *lf = (WmoLeaf *)kid->child;
      fprintf(out, "%*s L %p sym=%u%s hang=%u depth=%u chain:[",
              depth * 2 + 1, "", (void *)lf, kid->sym.sym,
              kid->sym.is_var ? "v" : "", lf->hang, lf->depth);
      for (u32 c = 0; c < lf->n_chain; c++)
        fprintf(out, " %u.%u", lf->chain[c].trace, lf->chain[c].face);
      fprintf(out, " ]\n");
    } else {
      fprintf(out, "%*s k sym=%u%s ->\n", depth * 2 + 1, "",
              kid->sym.sym, kid->sym.is_var ? "v" : "");
      wmo_dump_node((WmoNode *)kid->child, depth + 1, out);
    }
  }
}

static void wmo_dump_tree(AtpWmOrder *w, u8 tree, const char *label) {
  fprintf(stderr, "WMODUMP %s tree=%u\n", label, tree);
  wmo_dump_node(w->tree[tree].root, 1, stderr);
  fprintf(stderr, "WMODUMP-LL tree=%u:[", tree);
  for (WmoLeaf *lf = w->tree[tree].ll_head; lf != NULL; lf = lf->ll_next)
    fprintf(stderr, " %p(d%u)", (void *)lf, lf->depth);
  fprintf(stderr, " ]\n");
}

static void atp_wmo_remove_trace(AtpState *s, u32 trace) {
  AtpWmOrder *w = (AtpWmOrder *)s->wmo;
  if (w == NULL) return;
  static int wmo_dump = -1;
  if (wmo_dump < 0) wmo_dump = getenv("THVM_ATP_WMO_DUMP") != NULL ? 1 : 0;
  if (wmo_dump) {
    static u32 rm_n = 0;
    char lbl[64];
    snprintf(lbl, sizeof lbl, "pre-remove#%u trace=%u", ++rm_n, trace);
    wmo_dump_tree(w, 0u, lbl);
    wmo_dump_tree(w, 1u, lbl);
  }
  u32 r = 0;
  while (r < w->n_reg) {
    if (w->reg[r].trace == trace) {
      wmo_tree_remove(&w->tree[w->reg[r].tree], w->reg[r].cells,
                      w->reg[r].n_cells, trace, w->reg[r].face);
      free(w->reg[r].cells);
      w->reg[r] = w->reg[w->n_reg - 1u];
      w->n_reg--;
      w->tree_rev++;         // invalidate the wmo_tops_rank arrival memo
      {
        char lbl[64];
        snprintf(lbl, sizeof lbl, "post-remove trace=%u", trace);
        wmo_check(w, lbl);
      }
    } else {
      r++;
    }
  }
}

// ---------- order-mirror rank queries ----------

// Leaf-list rank of one registered (trace, face) within `tree`: the
// WM depth-ordered leaf-list position + within-leaf chain index.
static u8 wmo_leaflist_rank(AtpWmOrder *w, u8 tree, u32 trace, u8 face,
                            u32 *out_ll, u32 *out_chain) {
  // Memo probe (see AtpWmOrder.llr): the leaf list is frozen while
  // tree_rev holds, so a valid same-rev entry returns exactly what the
  // scan below would recompute.
  struct WmoLlrEnt *e = NULL;
  if (!w->no_rankcache) {
    u32 h = ((trace * 2654435761u) ^ ((u32)tree << 1) ^ (u32)face)
            & (WMO_LLR_N - 1u);
    e = &w->llr[h];
    if (e->valid && e->rev == w->tree_rev && e->trace == trace &&
        e->tree == tree && e->face == face) {
      if (!e->found) return 0u;
      *out_ll = e->ll;
      *out_chain = e->ch;
      return 1u;
    }
  }
  u32 rank = 0;
  u8  found = 0u;
  u32 ll = 0u, ch = 0u;
  for (WmoLeaf *l = w->tree[tree].ll_head; l != NULL && !found;
       l = l->ll_next) {
    for (u32 c = 0; c < l->n_chain; c++) {
      if (l->chain[c].trace == trace && l->chain[c].face == face) {
        ll = rank;
        ch = c;
        found = 1u;
        break;
      }
    }
    rank++;
  }
  if (e != NULL) {
    e->valid = 1u;
    e->rev   = w->tree_rev;
    e->trace = trace;
    e->tree  = tree;
    e->face  = face;
    e->found = found;
    e->ll    = ll;
    e->ch    = ch;
  }
  if (!found) return 0u;
  *out_ll = ll;
  *out_chain = ch;
  return 1u;
}

// Public wrapper: Gleichungsbaum (tree 1) leaf-list rank for one
// unorientable equation face, used by the normalize-redex selection to
// rank competing equation candidates in WM's MO_GleichungGefunden
// retrieval order.  `thvm_dir` is the candidate's rewrite direction
// (0 = match stored LHS, 1 = match stored RHS); the WM face it queries
// is `thvm_dir XOR dist_rhs` (CP-derived non-mono equations store the
// WM-distinguished face as thvm's RHS).  Returns 0 when the mirror is
// absent or the face is not registered (caller falls back to slot
// order).
static u8 atp_wmo_eq_leaflist_rank(AtpState *s, u32 trace, u8 thvm_dir,
                                   u32 *out_ll, u32 *out_chain) {
  AtpWmOrder *w = (AtpWmOrder *)s->wmo;
  if (w == NULL) return 0u;
  u8 wm_face = (u8)(thvm_dir ^ wmo_trace_dist_rhs(w, trace));
  return wmo_leaflist_rank(w, 1u, trace, wm_face, out_ll, out_chain);
}

// Tops arrival rank: DFS over `tree` with the query = subterm of
// `face_term` at CP position path; rank of the first arrival whose
// chain contains (trace, face).  Rule tree consults the chain HEAD
// only (BK_Regeln = first entry; MGUMitBlattGefundenAusschluss).
// Fill *d with the tops-DFS arrival list for `query_sub` against `tree`: the
// arrival-ordered leaves whose stored face unifies with the query.  `q` (caller
// buffer, WMO_MAX_CELLS) receives the query cells and *d->q points at it, so the
// caller must keep `q` alive while it reads *d.  Returns the query cell count
// (0 = empty/var query; *d untouched).  The N-way arrival-list cache (struct
// WmoRankCacheEnt, gated on tree_rev) lives here so BOTH callers benefit:
// wmo_tops_rank (find one partner's arrival) and wmo_tops_enum (enumerate all
// partners in arrival order, for the single-walk native CP formation).
//
// Sizing: combinator signatures (CombinatorAxioms / Meredith opCenterdot) build
// very wide trees -- one query subterm can unify with several thousand leaves --
// so the per-slot buffer is WMO_TOPS_ARR_CAP and heap-allocated; the depth_guard
// (4000) bounds the trie descent by query-cell count (a query face holds up to
// WMO_MAX_CELLS=768 cells).  An overflowing list (n_out at the cap) is still a
// valid cache entry: out[] holds the truncated prefix the DFS re-derives
// identically, and callers only read out[0..n_out).
u64 g_atp_wmo_rc_hit  = 0;
u64 g_atp_wmo_rc_miss = 0;
static u32 wmo_tops_dfs_fill(AtpWmOrder *w, u8 tree, Term query_sub,
                             WmoCell *q, WmoDfs *d) {
  u32 qn = wmo_face_cells(query_sub, q, WMO_MAX_CELLS);
  if (qn == 0u) return 0u;
  u32 rc_hit = WMO_RC_N;
  if (!w->no_rankcache) {
    for (u32 e = 0; e < WMO_RC_N; e++) {
      WmoRankCacheEnt *re = &w->rc[e];
      if (re->valid && re->rev == w->tree_rev && re->tree == tree &&
          re->qn == qn &&
          memcmp(re->q, q, (size_t)qn * sizeof(WmoCell)) == 0) {
        rc_hit = e;
        break;
      }
    }
  }
  if (rc_hit < WMO_RC_N) g_atp_wmo_rc_hit++; else g_atp_wmo_rc_miss++;
  if (rc_hit < WMO_RC_N) {
    *d = (WmoDfs){ q, qn, w->rc[rc_hit].out, w->rc[rc_hit].n_out, WMO_TOPS_ARR_CAP };
  } else {
    // Miss: run the DFS into a slot buffer and record it.  Round-robin
    // eviction; when the cache is disabled (THVM_WMO_NO_RANKCACHE) slot 0 is a
    // plain scratch buffer that is never recorded, so every call re-walks --
    // the ON-vs-OFF identity behaviour.
    u32 slot = w->no_rankcache ? 0u : w->rc_next;
    if (!w->no_rankcache) w->rc_next = (w->rc_next + 1u) % WMO_RC_N;
    if (w->rc[slot].out == NULL) {
      w->rc[slot].out =
          (WmoLeaf **)malloc(WMO_TOPS_ARR_CAP * sizeof(WmoLeaf *));
      if (w->rc[slot].out == NULL) return 0u;
    }
    *d = (WmoDfs){ q, qn, w->rc[slot].out, 0u, WMO_TOPS_ARR_CAP };
    WmoUnif u;
    memset(&u, 0, sizeof u);
    if (w->tree[tree].root != NULL) {
      // dispatch from the root: kids keyed by cell 0
      wmo_dfs(d, w->tree[tree].root, 0u, 0u, 0xffffffffu, &u, 4000u);
    }
    if (!w->no_rankcache) {
      w->rc[slot].valid = 1u;
      w->rc[slot].tree  = tree;
      w->rc[slot].rev   = w->tree_rev;
      w->rc[slot].qn    = qn;
      memcpy(w->rc[slot].q, q, (size_t)qn * sizeof(WmoCell));
      w->rc[slot].n_out = d->n_out;
    }
  }
  return qn;
}

static u8 wmo_tops_rank(AtpWmOrder *w, u8 tree, Term query_sub,
                        u32 partner_trace, u8 partner_face,
                        u32 *out_arrival, u32 *out_chain) {
  WmoCell q[WMO_MAX_CELLS];
  WmoDfs d;
  u32 qn = wmo_tops_dfs_fill(w, tree, query_sub, q, &d);
  if (qn == 0u) return 0u;
  // Arrival-dump probe (THVM_WMO_ARRDUMP): emit the eq-tree DFS leaf-arrival
  // order with each arrived leaf's chain (trace/face) so the runtime trie
  // walk can be compared leaf-by-leaf against WM's TT(l)=?E emission order.
  // Throttled to one dump per distinct (qn, query-cells) bucket and bounded
  // to the registration-count window THVM_WMO_ARRDUMP_LO/_HI; env-gated, off
  // in every normal run.
  static int arrdump = -1, arrdump_rule = -1;
  if (arrdump < 0) {
    arrdump      = getenv("THVM_WMO_ARRDUMP") != NULL;
    arrdump_rule = getenv("THVM_WMO_ARRDUMP_RULE") != NULL;
  }
  if ((tree == 1u || arrdump_rule) && arrdump) {
    // Gate to the registration window named by THVM_WMO_ARRDUMP_LO/HI (the
    // rule-35 era is ~64-66 registered faces); dump every distinct query once.
    const char *lo_s = getenv("THVM_WMO_ARRDUMP_LO");
    const char *hi_s = getenv("THVM_WMO_ARRDUMP_HI");
    u32 lo = lo_s ? (u32)atoi(lo_s) : 0u;
    u32 hi = hi_s ? (u32)atoi(hi_s) : 0xffffffffu;
    static u8 dumped[8192];   // throttle: one dump per (qn,cells-hash) bucket
    u32 h = qn;
    for (u32 c = 0; c < qn; c++) h = h * 131u + (q[c].is_var ? (9000u + q[c].sym) : q[c].sym);
    u32 bucket = h & 8191u;
    if (w->n_reg >= lo && w->n_reg <= hi && !dumped[bucket]) {
      dumped[bucket] = 1u;
      fprintf(stderr, "WMOARR nreg=%u tree=%u qn=%u n_out=%u q=[", w->n_reg, tree, qn, d.n_out);
      for (u32 c = 0; c < qn; c++)
        fprintf(stderr, "%s%d", c ? "," : "",
                q[c].is_var ? -(int)q[c].sym : (int)q[c].sym);
      fprintf(stderr, "]\n");
      for (u32 a = 0; a < d.n_out; a++) {
        WmoLeaf *l = d.out[a];
        fprintf(stderr, "  arr=%u leaf_key=[", a);
        for (u32 c = 0; c < l->key_len; c++)
          fprintf(stderr, "%s%d", c ? "," : "",
                  l->key[c].is_var ? -(int)l->key[c].sym : (int)l->key[c].sym);
        fprintf(stderr, "] chain=");
        for (u32 c = 0; c < l->n_chain; c++)
          fprintf(stderr, "%s(t=%u,f=%u)", c ? "," : "",
                  l->chain[c].trace, l->chain[c].face);
        fprintf(stderr, "\n");
      }
    }
  }
  u8 hit = 0u;
  for (u32 a = 0; a < d.n_out && !hit; a++) {
    WmoLeaf *l = d.out[a];
    u32 c_limit = (tree == 0u) ? 1u : l->n_chain;
    for (u32 c = 0; c < c_limit && c < l->n_chain; c++) {
      if (l->chain[c].trace == partner_trace &&
          l->chain[c].face == partner_face) {
        *out_arrival = a;
        *out_chain = c;
        hit = 1u;
        break;
      }
    }
  }
  return hit;
}

// Enumerate ALL partners (trace,face) whose stored face unifies with `query_sub`
// against `tree`, in tops-DFS arrival order -- the WM Vater-phase yield order
// (U1_KPsBildenZuRegel step 2).  Writes up to `max` hits to out[], returns the
// count.  Mirrors wmo_tops_rank's partner scan (rule tree: 1 chain entry per
// leaf; equation tree: all) but collects every partner rather than matching one.
// For thvm_atp_generate_cps_singlewalk (native CP formation, in progress).
static u32 wmo_tops_enum(AtpWmOrder *w, u8 tree, Term query_sub,
                         WmoPartnerHit *out, u32 max) {
  WmoCell q[WMO_MAX_CELLS];
  WmoDfs d;
  u32 qn = wmo_tops_dfs_fill(w, tree, query_sub, q, &d);
  if (qn == 0u) return 0u;
  u32 n = 0u;
  for (u32 a = 0; a < d.n_out && n < max; a++) {
    WmoLeaf *l = d.out[a];
    u32 c_limit = (tree == 0u) ? 1u : l->n_chain;
    for (u32 c = 0; c < c_limit && c < l->n_chain && n < max; c++) {
      out[n].trace   = l->chain[c].trace;
      out[n].face    = l->chain[c].face;
      out[n].arrival = a;
      n++;
    }
  }
  return n;
}

// Enumerate every registered face of `tree` in WM leaf-list order (ll_head ->
// ll_next), the Mutter-phase yield order (U1_KPsBildenZuRegel step 3: l =? eTT of
// the tree's faces).  out[].arrival = the leaf-list rank (one per leaf, shared by
// a leaf's chain entries), matching wmo_leaflist_rank.  This is the ORDER
// primitive; the single-walk Mutter step still unifies f's whole LHS into each
// listed face's subterms to decide which actually overlap (the filter) -- that
// per-leaf overlap test is the walk's job, not this enumerator's.
static u32 wmo_leaflist_enum(AtpWmOrder *w, u8 tree, WmoPartnerHit *out,
                             u32 max) {
  u32 n = 0u;
  u32 rank = 0u;
  for (WmoLeaf *l = w->tree[tree].ll_head; l != NULL && n < max; l = l->ll_next) {
    for (u32 c = 0; c < l->n_chain && n < max; c++) {
      out[n].trace   = l->chain[c].trace;
      out[n].face    = l->chain[c].face;
      out[n].arrival = rank;
      n++;
    }
    rank++;
  }
  return n;
}

// Public wrapper: WM MO_GleichungGefunden retrieval rank for one
// unorientable equation face against a concrete redex subterm.  WM's
// NormalformMixMost reduces a position by walking the Gleichungsbaum in
// a DFS that visits the matching function-symbol branch BEFORE the
// variable branch (MatchOperationen.c:660-661, 678-704: tryFct then
// tryNVar then tryOVar), so when two unorientable faces both match one
// redex the one reached earlier in that descent fires.  The depth-
// ordered leaf list (atp_wmo_eq_leaflist_rank) is NOT that order -- it
// ranks the more-general (variable-prefix) pattern before the specific
// function-prefix pattern, picking the wrong redex at competing
// positions (SKIToBCKW `S x K K`: leaf-list fires the W r->l face
// `(z y)y -> W z y` over the S l->r face, yielding `W (S x) K` where WM
// reduces to `(x K)(K K)`).  Ranking by the DFS arrival of the redex
// subterm reproduces WM's choice.  `thvm_dir` is the candidate's rewrite
// direction (0 = match stored lhs, 1 = match stored rhs); the WM face is
// `thvm_dir XOR dist_rhs`.  Returns 0 when the mirror is absent or the
// face is not reached (caller falls back to slot order).
static u8 atp_wmo_eq_tops_rank(AtpState *s, u32 trace, u8 thvm_dir,
                               Term redex_sub, u32 *out_arrival,
                               u32 *out_chain) {
  AtpWmOrder *w = (AtpWmOrder *)s->wmo;
  if (w == NULL) return 0u;
  u8 wm_face = (u8)(thvm_dir ^ wmo_trace_dist_rhs(w, trace));
  return wmo_tops_rank(w, 1u, redex_sub, trace, wm_face, out_arrival,
                       out_chain);
}

// Accumulate the per-variable occurrence counts of `t` into `cnt`
// (indexed by variable id, capped at WMO_VAR_CNT_CAP).
enum { WMO_VAR_CNT_CAP = 64u };
static void wmo_var_counts(Term t, u16 *cnt) {
  if (term_tag(t) == TAG_FVR) {
    u32 v = term_ext(t);
    if (v < WMO_VAR_CNT_CAP && cnt[v] < 0xffffu) cnt[v]++;
    return;
  }
  if (term_tag(t) == TAG_CTR) {
    u32 n = term_ctr_n(t);
    for (u32 i = 0; i < n; i++) wmo_var_counts(term_ctr_at(t, i), cnt);
  }
}

// Whether the two sides of an equation are NOT variable permutations of
// each other -- i.e. their per-variable occurrence-count PROFILES (each
// side's multiset of counts, sorted) differ.  A variable permutation
// (commutativity `f(x,y)=f(y,x)`, associativity rotation, or a
// role-swap like `f(x,g(y,y))=f(y,g(x,x))`) has the SAME sorted count
// profile on both sides and returns 0; an asymmetric equation whose one
// side introduces or drops occurrences (e.g. soa's `x*x = (y*(y*y))*x`:
// profile {2} vs {1,3}) returns 1.  Comparing SORTED profiles (not
// per-id counts) is renaming-invariant, so a pure role swap is correctly
// classified as a permutation.  Used to gate the walk's two-face
// co-rank collapse (sw_vater_visit / sw_corank_suppressed) to the
// equations WM stores oriented (whose single scan yields both unifiers)
// rather than the genuinely two-faced permutation equations WM keeps as
// distinct indexed leaves.
static u8 wmo_eq_sides_var_differ(Term lhs, Term rhs) {
  u16 cl[WMO_VAR_CNT_CAP] = {0};
  u16 cr[WMO_VAR_CNT_CAP] = {0};
  wmo_var_counts(lhs, cl);
  wmo_var_counts(rhs, cr);
  // Compare the sorted NONZERO count multisets.  Equivalent to the
  // full sorted-64-array compare (equal multisets over 64 slots <=>
  // equal nonzero multisets + equal zero counts <=> equal nonzero
  // multisets + equal lengths), but sorts the handful of live counts
  // (rules carry ~2-6 distinct vars) instead of qsorting 64 slots
  // twice -- this helper sits on the walk's per-hit hot path.
  u16 nl[WMO_VAR_CNT_CAP], nr[WMO_VAR_CNT_CAP];
  u32 a = 0, b = 0;
  for (u32 v = 0; v < WMO_VAR_CNT_CAP; v++) {
    if (cl[v] != 0u) nl[a++] = cl[v];
    if (cr[v] != 0u) nr[b++] = cr[v];
  }
  if (a != b) return 1u;
  for (u32 i = 1; i < a; i++) {          // insertion sort (tiny n)
    u16 x = nl[i];
    u32 k = i;
    while (k > 0u && nl[k - 1u] > x) { nl[k] = nl[k - 1u]; k--; }
    nl[k] = x;
  }
  for (u32 i = 1; i < b; i++) {
    u16 x = nr[i];
    u32 k = i;
    while (k > 0u && nr[k - 1u] > x) { nr[k] = nr[k - 1u]; k--; }
    nr[k] = x;
  }
  for (u32 v = 0; v < a; v++) {
    if (nl[v] != nr[v]) return 1u;
  }
  return 0u;
}
