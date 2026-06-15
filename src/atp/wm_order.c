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
//    Jump exits prepend at creation (RumpfSprungeintragSetzen
//    :283-299).  On a leaf split (BlattAufgeteilt :589-678) the main
//    descent stack is WIPED (StapelParallelInitialisieren :623 resets
//    StapelIndex), the GleichPfad chain is built, the new-leaf pass
//    runs first (:658) and the old-leaf pass second (:666) -- so a
//    fresh chain node's exit list bottoms out as [old, new]; the old
//    leaf's surviving in-entries get PARALLEL new-leaf entries placed
//    immediately AFTER them (AltesBlattPolieren :523-526) when the
//    jump still lands in the old suffix, or are REWIRED in place to
//    the chain node (:534-560) otherwise.  The one exception to
//    after-old placement: a strict-ancestor enclosing jump (start above
//    the leaf-found position) whose new-leaf subterm is SHORTER than the
//    old leaf's at that start heads the start node's exit list instead
//    (the more-general jump is consulted first), reproducing the
//    head-insert NeuesBlattEinhaengen would have made for the fully
//    closed shorter subterm -- BooleanAxioms OrAssociativity @300.
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

typedef struct {
  u32 trace;
  u8  tree;                   // 0 = rule tree, 1 = equation tree
  u8  face;
  u8  dist_rhs;               // 1 iff WM's distinguished face = thvm's stored RHS
  WmoCell *cells;             // owned copy (for removal lookup)
  u32 n_cells;
} WmoReg;

typedef struct AtpWmOrder {
  WmoTree  tree[2];           // [0] rules, [1] equations
  WmoReg  *reg;
  u32      n_reg, cap_reg;
  u32      rank_misses;       // CPs the ranker could not place (fallback)
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
  return a->sym == b->sym && a->is_var == b->is_var;
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

// Entry creation; dedupe on identical (start, sub, ziel) -- the C
// chain-loop pop and an AltesBlattPolieren rewire can otherwise race
// to create the same logical entry twice.
static WmoEntry *wmo_entry_add(WmoNode *start, const WmoCell *sub,
                               u32 sub_len, void *ziel, u8 ziel_leaf,
                               WmoEntry *after) {
  for (WmoEntry *e = start->exits; e != NULL; e = e->next) {
    if (e->ziel == ziel && e->sub_len == sub_len) {
      u32 k = 0;
      while (k < sub_len && wmo_cell_eq(&e->sub[k], &sub[k])) k++;
      if (k == sub_len) return e;
    }
  }
  WmoEntry *e = (WmoEntry *)calloc(1, sizeof(WmoEntry));
  e->start = start;
  e->sub = (WmoCell *)malloc(sub_len * sizeof(WmoCell));
  memcpy(e->sub, sub, sub_len * sizeof(WmoCell));
  e->sub_len = sub_len;
  e->ziel = ziel;
  e->ziel_leaf = ziel_leaf;
  if (after != NULL) {
    e->next = after->next;
    after->next = e;
  } else {
    e->next = start->exits;
    start->exits = e;
  }
  return e;
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

// ---------- insertion ----------

typedef struct { WmoNode *node; u32 pos; } WmoPend;

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

// Pop pendings (innermost first); subterms closing at position p with
// lo < p <= hi land at the chain node node_at[p - (lo+1)], later
// closings land in `leaf`.  Skips pendings whose pop the caller
// already excluded.
static void wmo_pop_pendings(const WmoCell *key, WmoPend *pend, u32 n_pend,
                             u32 lo, u32 hi, WmoNode **node_at,
                             WmoLeaf *leaf) {
  for (u32 k = n_pend; k-- > 0;) {
    u32 e = wmo_sub_end(key, pend[k].pos);
    if (node_at != NULL && e > lo && e <= hi) {
      wmo_entry_add(pend[k].node, key + pend[k].pos, e - pend[k].pos,
                    node_at[e - (lo + 1u)], 0u, NULL);
    } else {
      wmo_entry_add(pend[k].node, key + pend[k].pos, e - pend[k].pos,
                    leaf, 1u, NULL);
    }
  }
}

static void wmo_tree_insert(WmoTree *t, const WmoCell *key, u32 key_len,
                            u32 depth, u32 trace, u8 face) {
  if (key_len == 0u || key_len > WMO_MAX_CELLS) return;
  if (t->root == NULL) t->root = wmo_node_new(NULL);
  WmoNode *node = t->root;
  u32 i = 0;
  WmoPend pend[WMO_MAX_CELLS];
  u32 n_pend = 0;
  while (1) {
    const WmoCell *sym = &key[i];
    u8 child_is_leaf = 0;
    void *child = wmo_kid_get(node, sym, &child_is_leaf);
    if (child == NULL) {
      // ----- plain hang (NeuesBlattEinhaengen, Untergrenze=EintragEins)
      WmoLeaf *leaf = wmo_leaf_new(t, key, key_len, i, depth, node,
                                   trace, face);
      wmo_kid_set(node, sym, leaf, 1u);
      if (!sym->is_var && sym->arity > 0u) {
        if (n_pend < WMO_MAX_CELLS) { pend[n_pend].node = node; pend[n_pend].pos = i; n_pend++; }
      } else if (!sym->is_var && i > 0u) {
        wmo_entry_add(node, sym, 1u, leaf, 1u, NULL);   // constant special case
      }
      // bottom-most pending (the root-position one) is NOT popped
      u32 from = (n_pend > 0u && pend[0].pos == 0u) ? 1u : 0u;
      wmo_pop_pendings(key, pend + from, n_pend - from, 0u, 0u, NULL, leaf);
      return;
    }
    if (child_is_leaf) {
      WmoLeaf *old = (WmoLeaf *)child;
      // identical string -> chain prepend (RegelHinzufuegen)
      if (old->key_len == key_len) {
        u32 k = i;
        while (k < key_len && wmo_cell_eq(&old->key[k], &key[k])) k++;
        if (k == key_len) { wmo_chain_prepend(old, trace, face); return; }
      }
      // ----- split (BlattAufgeteilt); main-descent pendings are wiped
      // (StapelParallelInitialisieren :623)
      u32 j = i + 1u;
      while (j < key_len && j < old->key_len &&
             wmo_cell_eq(&key[j], &old->key[j])) j++;
      // chain-internal closures of main-descent pendings (the
      // chain loop's unbounded pop, :644-647) -- collect BEFORE wiping.
      WmoPend anc[WMO_MAX_CELLS];
      u32 n_anc = 0;
      for (u32 k = 0; k < n_pend; k++) {
        u32 e = wmo_sub_end(key, pend[k].pos);
        if (e > i && e <= j && n_anc < WMO_MAX_CELLS) anc[n_anc++] = pend[k];
      }
      // GleichPfad: nodes covering positions i+1 .. j
      WmoNode *node_at[WMO_MAX_CELLS];   // node_at[m] covers position i+1+m
      WmoNode *cur = node;
      const WmoCell *edge = sym;
      for (u32 p = i + 1u; p <= j; p++) {
        WmoNode *nxt = wmo_node_new(cur);
        wmo_kid_set(cur, edge, nxt, 0u);
        node_at[p - (i + 1u)] = nxt;
        cur = nxt;
        if (p < j) edge = &key[p];
      }
      WmoNode *last = node_at[j - (i + 1u)];
      // hang the two leaves
      WmoLeaf *leaf = wmo_leaf_new(t, key, key_len, j, depth, last,
                                   trace, face);
      if (j < key_len) wmo_kid_set(last, &key[j], leaf, 1u);
      old->hang = j;
      old->parent = last;
      if (j < old->key_len) wmo_kid_set(last, &old->key[j], old, 1u);
      // snapshot the old leaf's in-entries BEFORE new entries appear
      // (AltesBlattPolieren input set)
      // 1) new-leaf pass: chain pendings + leading push (new cells)
      {
        WmoPend cp2[WMO_MAX_CELLS];
        u32 n2 = 0;
        for (u32 p = i + 1u; p < j; p++) {
          if (!key[p].is_var && key[p].arity > 0u && n2 < WMO_MAX_CELLS) {
            cp2[n2].node = node_at[p - (i + 1u)]; cp2[n2].pos = p; n2++;
          }
        }
        if (j < key_len) {
          if (!key[j].is_var && key[j].arity > 0u && n2 < WMO_MAX_CELLS) {
            cp2[n2].node = last; cp2[n2].pos = j; n2++;
          } else if (!key[j].is_var) {
            wmo_entry_add(last, &key[j], 1u, leaf, 1u, NULL);
          }
        }
        wmo_pop_pendings(key, cp2, n2, i, j, node_at, leaf);
      }
      // 2) old-leaf pass (NeueSpruengeInsAlteBlatt; old cells), prepends
      //    land in FRONT of the new pass's -> [old, new] per node
      {
        WmoPend cp2[WMO_MAX_CELLS];
        u32 n2 = 0;
        for (u32 p = i + 1u; p < j; p++) {
          if (!old->key[p].is_var && old->key[p].arity > 0u && n2 < WMO_MAX_CELLS) {
            cp2[n2].node = node_at[p - (i + 1u)]; cp2[n2].pos = p; n2++;
          }
        }
        if (j < old->key_len) {
          if (!old->key[j].is_var && old->key[j].arity > 0u && n2 < WMO_MAX_CELLS) {
            cp2[n2].node = last; cp2[n2].pos = j; n2++;
          } else if (!old->key[j].is_var) {
            wmo_entry_add(last, &old->key[j], 1u, old, 1u, NULL);
          }
        }
        wmo_pop_pendings(old->key, cp2, n2, i, j, node_at, old);
      }
      // 2b) ancestor pendings closing within the chain (the C chain
      //     loop pops these with Ziel = the chain node, prepended at
      //     their ancestor start nodes).
      //
      // A strict-ancestor jump (start above the leaf-found position i) is
      // shared structurally by the old and new leaf: WM keeps ONE outgoing
      // exit at that start node and AltesBlattPolieren's else-branch
      // (DSBaumOperationen.c :534-560) merely re-targets it from the old
      // leaf onto the inner chain node (the outgoing-list position is
      // preserved; it touches only Sprungeingaenge and Zielknoten, not
      // Sprungausgaenge).  No fresh parallel is built, because the new leaf
      // is reached THROUGH that same inner node.  When the old leaf already
      // carries the in-entry that pass 3 will rewire to this very chain
      // node, a fresh prepended `anc` entry would be a spurious duplicate
      // that reorders the start node's exit list (head-inserting it ahead
      // of older sibling jumps).  Skip it; pass 3's in-place rewire keeps
      // WM's order (HuntingtonAxioms DoubleNegation @79).  The fresh entry
      // is still created when the old leaf has no such in-entry (the new
      // leaf's ancestor subterm is structurally absent in the old leaf).
      for (u32 k = n_anc; k-- > 0;) {
        u32 e = wmo_sub_end(key, anc[k].pos);
        u32 span = e - anc[k].pos;
        u8 old_has = 0u;
        for (WmoEntry *oe = anc[k].node->exits; oe != NULL; oe = oe->next) {
          if (oe->ziel == (void *)old && oe->sub_len == span) {
            u32 c = 0;
            while (c < span && wmo_cell_eq(&oe->sub[c], &key[anc[k].pos + c])) c++;
            if (c == span) { old_has = 1u; break; }
          }
        }
        if (old_has) continue;
        wmo_entry_add(anc[k].node, key + anc[k].pos, span,
                      node_at[e - (i + 1u)], 0u, NULL);
      }
      // 3) AltesBlattPolieren on the old leaf's pre-existing in-entries
      {
        // gather entries targeting `old` that are NOT the ones just made
        // in pass 2 (those have start inside the fresh chain; ancestor
        // entries have start at depth <= i)
        typedef struct { WmoNode *n; WmoEntry *e; } Hit;
        Hit hits[256];
        u32 n_hits = 0;
        // walk all nodes
        // (recursive closure via explicit stack)
        WmoNode *stack[1024];
        u32 sp = 0;
        if (t->root) stack[sp++] = t->root;
        while (sp > 0u) {
          WmoNode *n = stack[--sp];
          u32 nd = wmo_node_depth(n);
          for (WmoEntry *e = n->exits; e != NULL; e = e->next) {
            if (e->ziel == (void *)old && nd <= i &&
                nd + e->sub_len > j && n_hits < 256u) {
              hits[n_hits].n = n; hits[n_hits].e = e; n_hits++;
            } else if (e->ziel == (void *)old && nd <= i &&
                       nd + e->sub_len > i && nd + e->sub_len <= j) {
              // lands within the chain -> rewire in place
              e->ziel = node_at[(nd + e->sub_len) - (i + 1u)];
              e->ziel_leaf = 0u;
            }
          }
          for (u32 kk = 0; kk < n->n_kids; kk++) {
            if (!n->kids[kk].is_leaf && sp < 1024u) {
              stack[sp++] = (WmoNode *)n->kids[kk].child;
            }
          }
        }
        for (u32 h = 0; h < n_hits; h++) {
          u32 start_pos = wmo_node_depth(hits[h].n);
          u32 e2 = wmo_sub_end(key, start_pos);
          u32 old_e2 = wmo_sub_end(old->key, start_pos);
          // Outgoing-list placement of the new leaf's parallel jump.
          // AltesBlattPolieren chains the parallel immediately AFTER the
          // old leaf's surviving entry (DSBaumOperationen.c :523-526) -- the
          // default.  The exception is a STRICT-ANCESTOR jump start
          // (start_pos < i, i.e. the enclosing function subterm opened above
          // the leaf-found position) whose new-leaf subterm is SHORTER than
          // the old leaf's at that start (old_e2 > e2): the new leaf is more
          // general at that position, and its jump heads the start node's
          // exit list (the RumpfSprungeintragSetzen head-insert :293-295
          // that NeuesBlattEinhaengen :461-481 would have produced for the
          // shorter, fully-closed enclosing subterm).  This is the
          // BooleanAxioms OrAssociativity @300 corner: the shorter
          // or(v,and(v,w)) parallel must precede the longer enclosing
          // or-jump that prepended ahead of it at leaf-insert time.  Equal /
          // longer new subterms keep AltesBlattPolieren's after-old order.
          WmoEntry *after =
            (start_pos < i && old_e2 > e2) ? NULL : hits[h].e;
          wmo_entry_add(hits[h].n, key + start_pos, e2 - start_pos,
                        leaf, 1u, after);
        }
      }
      return;
    }
    // descend into inner node (BO_ObjektEinfuegen :707-719)
    if (!sym->is_var && sym->arity > 0u) {
      if (n_pend < WMO_MAX_CELLS) { pend[n_pend].node = node; pend[n_pend].pos = i; n_pend++; }
    } else {
      u32 e = i + 1u;
      while (n_pend > 0u && wmo_sub_end(key, pend[n_pend - 1u].pos) == e) {
        n_pend--;   // closes at an existing inner node: entry exists
      }
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
    for (u32 f = 0; f < n_freed; f++) wmo_node_free_one(freed[f]);
  }
  wmo_leaf_free(leaf);
}

// ---------- tops DFS (arrival ranking) ----------

// Cell-level unification state: tree vars (stored side, shared along a
// path) and query vars in two binding tables.  Bindings map a var to a
// (cells, len) subterm of the OTHER side -- enough for an occurs-free
// small-scale unifier.  To keep this simple and allocation-free we
// bind to opaque subterm slices and compare structurally on demand.

typedef struct {
  const WmoCell *cells;
  u32 off, len;
  u8 side;          // 0 = query, 1 = stored
} WmoBind;

typedef struct {
  WmoBind tvar[WMO_MAX_VARS + 1u];   // stored/tree vars by canonical id
  WmoBind qvar[WMO_MAX_VARS + 1u];   // query vars by canonical id
} WmoUnif;

// Structural unification of two cell slices under the binding tables.
// Depth-bounded; returns 1 on success (tables updated), 0 on failure
// (tables may be partially updated -- callers copy-on-descend).
static u8 wmo_unify_cells(const WmoCell *a, u32 alen, u8 aside,
                          const WmoCell *b, u32 blen, u8 bside,
                          WmoUnif *u, u32 fuel);

static u8 wmo_unify_var(WmoBind *slot, const WmoCell *t, u32 tlen, u8 tside,
                        WmoUnif *u, u32 fuel) {
  if (slot->cells != NULL) {
    return wmo_unify_cells(slot->cells + slot->off, slot->len, slot->side,
                           t, tlen, tside, u, fuel);
  }
  slot->cells = t; slot->off = 0; slot->len = tlen; slot->side = tside;
  return 1u;
}

static u8 wmo_unify_cells(const WmoCell *a, u32 alen, u8 aside,
                          const WmoCell *b, u32 blen, u8 bside,
                          WmoUnif *u, u32 fuel) {
  if (fuel == 0u) return 0u;
  if (a->is_var) {
    WmoBind *slot = (aside == 0u) ? &u->qvar[a->sym] : &u->tvar[a->sym];
    if (b->is_var && aside == bside && a->sym == b->sym) return 1u;
    return wmo_unify_var(slot, b, blen, bside, u, fuel - 1u);
  }
  if (b->is_var) {
    WmoBind *slot = (bside == 0u) ? &u->qvar[b->sym] : &u->tvar[b->sym];
    return wmo_unify_var(slot, a, alen, aside, u, fuel - 1u);
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
        if (!wmo_unify_var(&u->qvar[qc->sym], key + si, se - si, 1u, u, 64u))
          return 0u;
        si = se; qi++;
      }
    } else {
      u32 qe = wmo_sub_end(d->q, qi);
      if (!wmo_unify_var(&u->tvar[sc->sym], d->q + qi, qe - qi, 0u, u, 64u))
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
    WmoUnif u2 = *u;
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
      WmoUnif u2 = *u;
      if (wmo_unify_var(&u2.tvar[vkids[k].sym.sym], d->q + qi, qe - qi, 0u,
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
      WmoUnif u2 = *u;
      WmoCell vcell = vkids[k].sym;
      if (wmo_unify_cells(&d->q[qi], 1u, 0u, &vcell, 1u, 1u, &u2, 64u)) {
        wmo_dfs(d, vkids[k].child, vkids[k].is_leaf, qi + 1u, 0xffffffffu,
                &u2, depth_guard - 1u);
      }
    }
    for (WmoEntry *e = n->exits; e != NULL; e = e->next) {
      WmoUnif u2 = *u;
      if (wmo_unify_var(&u2.qvar[qc->sym], e->sub, e->sub_len, 1u, &u2,
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
  return (AtpWmOrder *)calloc(1, sizeof(AtpWmOrder));
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
  free(w);
}

static void wmo_register(AtpWmOrder *w, u32 trace, u8 tree, u8 face,
                         const WmoCell *cells, u32 n_cells, u32 depth,
                         u8 dist_rhs) {
  wmo_tree_insert(&w->tree[tree], cells, n_cells, depth, trace, face);
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
}

// Whether WM's distinguished (indexed) face for the fact with birth
// trace id `trace` is thvm's STORED RHS (vs the default LHS).  Recorded
// at registration; queried by atp_wmo_rank to remap a CP's thvm-face
// bit (0 = lhs, 1 = rhs) onto WM's face (0 = distinguished, 1 = reverse)
// before classifying its emission phase and looking up the partner's
// indexed face.  Returns 0 for an unregistered trace (orientable rules
// and intake equations keep distinguished = lhs).
static u8 wmo_trace_dist_rhs(const AtpWmOrder *w, u32 trace) {
  for (u32 r = 0; r < w->n_reg; r++) {
    if (w->reg[r].trace == trace) return w->reg[r].dist_rhs;
  }
  return 0u;
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
static void atp_wmo_insert_fact_ex(AtpState *s, u32 slot, u8 cp_derived) {
  AtpWmOrder *w = (AtpWmOrder *)s->wmo;
  if (w == NULL) return;
  u32 trace = s->r_trace[slot];
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

// In-place identity rename (RHS compose repoints r_trace to a fresh
// TRACE_ORIENT entry while the fact keeps its tree position -- WM's
// RMRechtsInterred modifies the rule in place, Interreduktion.c
// :329-360).  Chains and registry follow the new id; leaf positions
// unchanged.
static void atp_wmo_rename_trace(AtpState *s, u32 old_t, u32 new_t) {
  AtpWmOrder *w = (AtpWmOrder *)s->wmo;
  if (w == NULL || old_t == new_t) return;
  for (u32 r = 0; r < w->n_reg; r++) {
    if (w->reg[r].trace == old_t) w->reg[r].trace = new_t;
  }
  for (u32 t = 0; t < 2u; t++) {
    for (WmoLeaf *l = w->tree[t].ll_head; l != NULL; l = l->ll_next) {
      for (u32 c = 0; c < l->n_chain; c++) {
        if (l->chain[c].trace == old_t) l->chain[c].trace = new_t;
      }
    }
  }
}

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
// victims (tree 1) -- equations sort first; low bits = the victim's
// distinguished-face (face 0) leaf-list rank in its tree.  Removals of
// EARLIER-captured victims only delete leaves (never reorder), so ranks
// captured at successive push moments preserve the pass-start relative
// order between any two surviving victims.
static u32 atp_wmo_victim_drain_key(AtpState *s, u32 trace) {
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
  u32 rank = 0;
  for (WmoLeaf *l = w->tree[tree].ll_head; l != NULL; l = l->ll_next) {
    for (u32 c = 0; c < l->n_chain; c++) {
      if (l->chain[c].trace == trace && l->chain[c].face == 0u) {
        return ((tree == 0u ? 1u : 0u) << 31) | (rank & 0x7fffffffu);
      }
    }
    rank++;
  }
  return 0x7fffffffu;
}

// Remove every registered face of the fact with birth trace id `trace`.
static void atp_wmo_remove_trace(AtpState *s, u32 trace) {
  AtpWmOrder *w = (AtpWmOrder *)s->wmo;
  if (w == NULL) return;
  u32 r = 0;
  while (r < w->n_reg) {
    if (w->reg[r].trace == trace) {
      wmo_tree_remove(&w->tree[w->reg[r].tree], w->reg[r].cells,
                      w->reg[r].n_cells, trace, w->reg[r].face);
      free(w->reg[r].cells);
      w->reg[r] = w->reg[w->n_reg - 1u];
      w->n_reg--;
    } else {
      r++;
    }
  }
}

// ---------- batch ranking ----------

// Phase constants (per-new-fact emission segments, U1_KPsBildenZu*):
//   rule:     A(0) tops, B(1) eTT
//   equation: A(0), B(1), F(2), C(3), D(4), E(5), G(6)
//
// Sort key, packed descending-significance:
//   [phase:4][k1:14][k2:2][k3:14][k4:14][k5:14]
//   A/D: k1 = preorder rank of the overlapped position in the new
//        fact's query face; k2 = tree consult order (A: R=0,E=1;
//        D: E=0,R=1); k3 = tops DFS arrival rank of the partner leaf;
//        k4 = chain rank; k5 = 0.
//   B/E: k1 = 0; k2 = tree consult order (B: R=0,E=1; E: E=0,R=1);
//        k3 = leaf-list rank of the partner's outer face leaf;
//        k4 = chain rank; k5 = preorder rank of the position in the
//        partner's face.
//   F/C/G: zeros.
static u64 wmo_pack_key(u32 phase, u32 k1, u32 k2, u32 k3, u32 k4, u32 k5) {
  if (k1 > 0x3fffu) k1 = 0x3fffu;
  if (k3 > 0x3fffu) k3 = 0x3fffu;
  if (k4 > 0x3fffu) k4 = 0x3fffu;
  if (k5 > 0x3fffu) k5 = 0x3fffu;
  return ((u64)phase << 58) | ((u64)k1 << 44) | ((u64)(k2 & 3u) << 42) |
         ((u64)k3 << 28) | ((u64)k4 << 14) | (u64)k5;
}

// Preorder rank of position path `pos[0..len)` within term `t`
// (counting every node, root = 0).
static u32 wmo_preorder_rank(Term t, const u8 *pos, u32 len) {
  u32 rank = 0;
  for (u32 d = 0; d < len; d++) {
    if (term_tag(t) != TAG_CTR) return rank;
    u32 n = term_ctr_n(t);
    u32 idx = pos[d];
    if (idx >= n) return rank;
    rank += 1u;
    for (u32 c = 0; c < idx; c++) {
      // add the subtree size of each left sibling
      Term sub = term_ctr_at(t, c);
      // subtree size = cell count
      WmoCell tmp[WMO_MAX_CELLS];
      u32 sz = wmo_face_cells(sub, tmp, WMO_MAX_CELLS);
      rank += (sz == 0u) ? 1u : sz;
    }
    t = term_ctr_at(t, idx);
  }
  return rank;
}

// Per-batch rank context: leaf-list ranks per tree and memoized tops
// DFS results per (tree, query position).
typedef struct {
  AtpState *s;
  AtpWmOrder *w;
  u32 f;                       // the new fact's slot
  u32 f_trace;
  // leaf-list rank lookup: linearized (trace, face) -> rank per tree
  // (computed lazily by scanning; the lists are short)
} WmoRankCtx;

static u8 wmo_leaflist_rank(AtpWmOrder *w, u8 tree, u32 trace, u8 face,
                            u32 *out_ll, u32 *out_chain) {
  u32 rank = 0;
  for (WmoLeaf *l = w->tree[tree].ll_head; l != NULL; l = l->ll_next) {
    for (u32 c = 0; c < l->n_chain; c++) {
      if (l->chain[c].trace == trace && l->chain[c].face == face) {
        *out_ll = rank;
        *out_chain = c;
        return 1u;
      }
    }
    rank++;
  }
  return 0u;
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
static u8 wmo_tops_rank(AtpWmOrder *w, u8 tree, Term query_sub,
                        u32 partner_trace, u8 partner_face,
                        u32 *out_arrival, u32 *out_chain) {
  WmoCell q[WMO_MAX_CELLS];
  u32 qn = wmo_face_cells(query_sub, q, WMO_MAX_CELLS);
  if (qn == 0u) return 0u;
  WmoLeaf *arr[512];
  WmoDfs d = { q, qn, arr, 0u, 512u };
  WmoUnif u;
  memset(&u, 0, sizeof u);
  if (w->tree[tree].root != NULL) {
    // dispatch from the root: kids keyed by cell 0
    wmo_dfs(&d, w->tree[tree].root, 0u, 0u, 0xffffffffu, &u, 200u);
  }
  for (u32 a = 0; a < d.n_out; a++) {
    WmoLeaf *l = d.out[a];
    u32 c_limit = (tree == 0u) ? 1u : l->n_chain;
    for (u32 c = 0; c < c_limit && c < l->n_chain; c++) {
      if (l->chain[c].trace == partner_trace &&
          l->chain[c].face == partner_face) {
        *out_arrival = a;
        *out_chain = c;
        return 1u;
      }
    }
  }
  return 0u;
}

// Compute the WM emission rank key for one tagged CP of the new fact
// `f`'s batch.  `i`/`j` are the overlap slots (i = outer, positions in
// i's face), `combo` = atp_overlap_ij face combo (bit0: j used its
// reverse face; bit1: i used its reverse face).
static u64 atp_wmo_rank(AtpState *s, u32 f, u32 i, u32 j, u8 combo,
                        const CriticalPair *cp) {
  AtpWmOrder *w = (AtpWmOrder *)s->wmo;
  u8 i_face = (combo >> 1) & 1u;
  u8 j_face = combo & 1u;
  // The CP's combo bits name the thvm face (0 = stored lhs, 1 = stored
  // rhs) each parent used.  WM classifies the emission phase (A/B vs D/E)
  // and indexes leaves by its OWN distinguished(0)/reverse(1) face; when
  // a fact's WM-distinguished face is thvm's rhs (CP-derived unorientable
  // equation, dist_rhs), the two numberings are flipped.  Remap each
  // parent's face onto WM's before phase/lookup; the OVERLAP TERM stays
  // the thvm face (that is the term the CP was actually built from).
  u8 i_dr = wmo_trace_dist_rhs(w, s->r_trace[i]);
  u8 j_dr = wmo_trace_dist_rhs(w, s->r_trace[j]);
  u8 i_face_wm = i_face ^ i_dr;
  u8 j_face_wm = j_face ^ j_dr;
  Term i_outer = i_face ? s->rhs[i] : s->lhs[i];
  if (i == f && j == f && cp->pos_len == 0u) {
    // self roots, classified by WM face (NOT raw thvm combo): F = l =? l
    // (both WM-distinguished), C = r =? l (mixed, stereo only), G = r =? r
    // (both WM-reverse).  When dist_rhs flips thvm's stored lhs onto WM's
    // reverse face, thvm's combo-0 (stored lhs x stored lhs) is WM's G,
    // and combo-3 (stored rhs x stored rhs) is WM's F -- the two swap.
    // Keying on i_face_wm/j_face_wm (= thvm face XOR dist_rhs) gives the
    // WM phase regardless of which thvm face carries the distinguished
    // side.
    u32 phase = (!i_face_wm && !j_face_wm) ? 2u           // F: l =? l
              : (i_face_wm && j_face_wm)   ? 6u           // G: r =? r
                                          : 3u;           // C: r =? l
    return wmo_pack_key(phase, 0, 0, 0, 0, 0);
  }
  if (i == f && j != f) {
    // tops phase: A (i WM-distinguished face) or D (i WM-reverse face)
    u32 phase = i_face_wm ? 4u : 0u;
    u32 k1 = wmo_preorder_rank(i_outer, cp->pos, cp->pos_len);
    u8 j_is_rule = s->r_orient[j] ? 1u : 0u;
    u8 tree = j_is_rule ? 0u : 1u;
    u32 k2 = i_face_wm ? (tree == 1u ? 0u : 1u) : (u32)tree;
    Term qsub = i_outer;
    for (u32 d = 0; d < cp->pos_len; d++) {
      if (term_tag(qsub) != TAG_CTR) break;
      qsub = term_ctr_at(qsub, cp->pos[d]);
    }
    u32 arr = 0, ch = 0;
    if (!wmo_tops_rank(w, tree, qsub, s->r_trace[j], j_face_wm, &arr, &ch)) {
      w->rank_misses++;
      arr = 0x3fffu;
    }
    return wmo_pack_key(phase, k1, k2, arr, ch, 0);
  }
  // eTT phase: B (j WM-distinguished face) or E (j WM-reverse face);
  // includes self-proper overlaps (i == j == f, pos_len > 0) at the new
  // fact's own leaves.
  u32 phase = j_face_wm ? 5u : 1u;
  u8 i_is_rule = s->r_orient[i] ? 1u : 0u;
  u8 tree = i_is_rule ? 0u : 1u;
  u32 k2 = j_face_wm ? (tree == 1u ? 0u : 1u) : (u32)tree;
  u32 ll = 0, ch = 0;
  if (!wmo_leaflist_rank(w, tree, s->r_trace[i], i_face_wm, &ll, &ch)) {
    w->rank_misses++;
    ll = 0x3fffu;
  }
  u32 k5 = wmo_preorder_rank(i_outer, cp->pos, cp->pos_len);
  return wmo_pack_key(phase, 0, k2, ll, ch, k5);
}
