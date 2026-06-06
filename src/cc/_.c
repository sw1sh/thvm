// cc/_.c - ground congruence closure (QF_UF decision procedure).
//
// A self-contained Downey-Sethi-Tarjan congruence closure over the
// module's own integer-encoded egraph nodes (NOT thvm's Term/IC heap;
// a later phase integrates that).  Decides ground quantifier-free
// equality with uninterpreted functions: union-find over subterm
// nodes, a per-class use list, a signature table for hash-consing
// applications by (fn-label, arg class-reps), and congruence
// propagation through a pending-merge queue.  Near-linear, no search.
//
// Matches the semantics of the WL prototype TSatEUF
// (wl/THVMLink/Kernel/ATP/SMT.wl): SAT/UNSAT where UNSAT iff some
// asserted disequality's two sides are congruent, plus readable
// equivalence classes on SAT via cc_find.

// === egraph node store ===
//
// Every node is either an atom (a fresh constant) or an application
// f(a0..a_{k-1}).  Applications are hash-consed: cc_app interns by the
// signature (fn_id, find(a0), .., find(a_{k-1})) so structurally
// congruent terms built from already-equal args share a node from the
// start.  Re-canonicalization after a merge is handled by the use list.

#define CC_NONE 0xffffffffu

typedef struct {
  u32  fn_id;     // application label; CC_NONE for an atom
  u32  atom_id;   // atom label (when fn_id == CC_NONE)
  u32  nargs;
  u32 *args;      // owned; child node ids (original, not canonicalized)
} CcNode;

// A signature-table entry: a canonical application key -> node id.
// `key` holds {fn_id, rep(a0), .., rep(a_{k-1})} captured at insert
// time; lookups recompute the canonical key and linear-probe.
typedef struct {
  u32  hash;
  u32  node;      // CC_NONE => empty slot
  u32  nkey;
  u32 *key;       // owned
} CcSigEntry;

// === undo-trail record ===
//
// One destructive op.  On cc_pop the records above the mark replay in
// reverse:
//   TR_UNION   : undo a union.  `a` = the redirected child root (rb),
//                `b` = the surviving root (ra), `r` = ra's old rank.
//                Restores parent[rb]=rb and rank[ra]=r.  (cc_find does
//                no path compression, so the union's redirected root is
//                the ONLY parent edge a merge creates -- the record
//                reverts it exactly.)
//   TR_SIG_PUT : a signature entry was inserted under canonical `key`
//                (length `nkey`).  Undo = remove it.
//   TR_SIG_DEL : a signature entry was removed; undo = reinsert `key`
//                -> node `a`.
//   TR_USE     : app `a` was prepended to class-rep `b`'s use list;
//                `r` = the prior use_head[b].  Undo = restore use_head[b]
//                and reclaim the pool cell (use_len shrinks since the
//                cell is always the freshest one).
//   TR_NODE    : a node was created (n_nodes grew by one).  Undo =
//                shrink n_nodes, freeing the node's args buffer.
//   TR_NE      : a disequality was asserted (ne_len grew).  Undo =
//                shrink ne_len.
typedef enum {
  TR_UNION = 0,
  TR_SIG_PUT,
  TR_SIG_DEL,
  TR_USE,
  TR_USE_SPLICE,
  TR_NODE,
  TR_NE,
} CcTrailTag;

typedef struct {
  u32  tag;
  u32  a;
  u32  b;
  u32  r;
  // TR_UNION use-list splice undo: rb's list (use_head[rb]) was spliced
  // onto ra's head.  `splice_tail` = the last cell of rb's old list
  // (CC_NONE if rb's list was empty, i.e. no splice happened);
  // `ra_old_head` = use_head[ra] before the splice (== use_next[tail]
  // afterward).  Undo restores use_head[rb]=a's old head was rb's head,
  // use_head[ra]=ra_old_head, use_next[tail]=CC_NONE.
  u32  splice_tail;
  u32  ra_old_head;
  u32 *key;   // owned (TR_SIG_PUT / TR_SIG_DEL only); NULL otherwise
  u32  nkey;
  u32  hash;
} CcTrail;

struct CcState {
  // node store
  CcNode *nodes;
  u32     n_nodes;
  u32     cap_nodes;

  // union-find over node ids
  u32    *parent;
  u32    *rank;

  // per-class use list: for class rep r, the app nodes whose (original)
  // args include a node currently in r's class.  use_head[r] indexes
  // into the use_next/use_node pools (a singly-linked list); CC_NONE
  // terminates.
  u32    *use_head;
  u32    *use_node;   // pooled list cells: the app node id
  u32    *use_next;   // pooled list cells: next index
  u32     use_len;
  u32     use_cap;

  // signature table (open-addressed)
  CcSigEntry *sig;
  u32         sig_cap;   // power of two
  u32         sig_count;

  // pending merge queue: pairs of node ids to union
  u32    *pend_a;
  u32    *pend_b;
  u32     pend_len;
  u32     pend_cap;

  // asserted disequalities (node pairs)
  u32    *ne_a;
  u32    *ne_b;
  u32     ne_len;
  u32     ne_cap;

  // === undo trail ===
  //
  // Every destructive mutation pushes one CcTrail record; cc_pop(mark)
  // replays the records above `mark` in reverse to restore the state
  // byte-for-byte.  cc_push returns the current trail length as a mark.
  // The pending queue is transient (always drained to empty by
  // cc_propagate before control returns), so it carries no trail.
  CcTrail *trail;
  u32      trail_len;
  u32      trail_cap;
};

// === growable-array helpers ===

static void cc_grow_nodes(CcState *s, u32 need) {
  if (need <= s->cap_nodes) return;
  u32 cap = s->cap_nodes ? s->cap_nodes : 16u;
  while (cap < need) cap *= 2u;
  s->nodes    = (CcNode *)realloc(s->nodes,    cap * sizeof(CcNode));
  s->parent   = (u32 *)realloc(s->parent,      cap * sizeof(u32));
  s->rank     = (u32 *)realloc(s->rank,        cap * sizeof(u32));
  s->use_head = (u32 *)realloc(s->use_head,    cap * sizeof(u32));
  s->cap_nodes = cap;
}

static u32 cc_use_cell(CcState *s) {
  if (s->use_len >= s->use_cap) {
    s->use_cap = s->use_cap ? s->use_cap * 2u : 32u;
    s->use_node = (u32 *)realloc(s->use_node, s->use_cap * sizeof(u32));
    s->use_next = (u32 *)realloc(s->use_next, s->use_cap * sizeof(u32));
  }
  return s->use_len++;
}

// === undo trail ===

// Reserve and return a fresh trail record (zeroed key fields).
static CcTrail *cc_trail_push(CcState *s, u32 tag) {
  if (s->trail_len >= s->trail_cap) {
    s->trail_cap = s->trail_cap ? s->trail_cap * 2u : 64u;
    s->trail = (CcTrail *)realloc(s->trail, s->trail_cap * sizeof(CcTrail));
  }
  CcTrail *e = &s->trail[s->trail_len++];
  e->tag  = tag;
  e->key  = NULL;
  e->nkey = 0;
  e->hash = 0;
  e->a = e->b = e->r = 0;
  e->splice_tail = 0;
  e->ra_old_head = 0;
  return e;
}

// Prepend app node `app` to class rep `r`'s use list.
static void cc_use_add(CcState *s, u32 r, u32 app) {
  CcTrail *e = cc_trail_push(s, TR_USE);
  e->a = app;
  e->b = r;
  e->r = s->use_head[r];     // prior head, restored on undo
  u32 c = cc_use_cell(s);
  s->use_node[c] = app;
  s->use_next[c] = s->use_head[r];
  s->use_head[r] = c;
}

// === union-find ===

// No path compression: this union-find must be exactly invertible by the
// undo trail.  Compression would repoint interior nodes straight at a
// root, and a later cc_pop that detaches that root could not restore
// those interior parents (they were never the union's redirected child).
// Union-by-rank keeps find at O(log n) without compression, so a single
// TR_UNION record (the redirected root + ra's old rank) fully reverts a
// merge.
fn u32 cc_find(CcState *s, u32 n) {
  while (s->parent[n] != n) n = s->parent[n];
  return n;
}

// === signature table ===

// Canonical key for an app node: {fn_id, find(arg0), .., find(argk-1)}.
// Written into `out` (caller provides 1 + nargs slots), returns hash.
static u32 cc_sig_key(CcState *s, u32 app, u32 *out) {
  CcNode *nd = &s->nodes[app];
  u32 h = 0x811c9dc5u;
  out[0] = nd->fn_id;
  h = (h ^ nd->fn_id) * 0x01000193u;
  for (u32 i = 0; i < nd->nargs; i++) {
    u32 r = cc_find(s, nd->args[i]);
    out[1 + i] = r;
    h = (h ^ r) * 0x01000193u;
  }
  return h;
}

static int cc_key_eq(const u32 *a, u32 na, const u32 *b, u32 nb) {
  if (na != nb) return 0;
  for (u32 i = 0; i < na; i++) if (a[i] != b[i]) return 0;
  return 1;
}

static void cc_sig_init(CcState *s, u32 cap) {
  s->sig_cap = cap;
  s->sig_count = 0;
  s->sig = (CcSigEntry *)malloc(cap * sizeof(CcSigEntry));
  for (u32 i = 0; i < cap; i++) {
    s->sig[i].node = CC_NONE;
    s->sig[i].key = NULL;
    s->sig[i].nkey = 0;
  }
}

// Look up canonical (key,nkey,hash); return interned node id or CC_NONE,
// and set *slot to the probe index where it lives / would be inserted.
static u32 cc_sig_lookup(CcState *s, u32 hash, const u32 *key, u32 nkey,
                         u32 *slot) {
  u32 mask = s->sig_cap - 1u;
  u32 i = hash & mask;
  for (;;) {
    CcSigEntry *e = &s->sig[i];
    if (e->node == CC_NONE) { *slot = i; return CC_NONE; }
    if (e->hash == hash && cc_key_eq(e->key, e->nkey, key, nkey)) {
      *slot = i;
      return e->node;
    }
    i = (i + 1u) & mask;
  }
}

static void cc_sig_rehash(CcState *s);

// Insert (key,node) at a known-empty probe slot, copying the key.  RAW:
// no trail record (used both by the trailing wrapper and by cc_pop's
// TR_SIG_DEL reinsert path).
static void cc_sig_put_raw(CcState *s, u32 slot, u32 hash, const u32 *key,
                           u32 nkey, u32 node) {
  CcSigEntry *e = &s->sig[slot];
  e->hash = hash;
  e->node = node;
  e->nkey = nkey;
  e->key = (u32 *)malloc((nkey ? nkey : 1u) * sizeof(u32));
  for (u32 i = 0; i < nkey; i++) e->key[i] = key[i];
  s->sig_count++;
  if (s->sig_count * 4u >= s->sig_cap * 3u) cc_sig_rehash(s);
}

// Trailing insert: record the insertion so cc_pop can remove it.
static void cc_sig_put(CcState *s, u32 slot, u32 hash, const u32 *key,
                       u32 nkey, u32 node) {
  CcTrail *tr = cc_trail_push(s, TR_SIG_PUT);
  tr->hash = hash;
  tr->nkey = nkey;
  tr->key  = (u32 *)malloc((nkey ? nkey : 1u) * sizeof(u32));
  for (u32 i = 0; i < nkey; i++) tr->key[i] = key[i];
  // cc_sig_put_raw may rehash (relocating entries); the trail stores the
  // canonical (hash,key), not a slot, so the recorded entry is found on
  // undo regardless of relocation.
  cc_sig_put_raw(s, slot, hash, key, nkey, node);
}

static void cc_sig_rehash(CcState *s) {
  u32 old_cap = s->sig_cap;
  CcSigEntry *old = s->sig;
  cc_sig_init(s, old_cap * 2u);
  for (u32 i = 0; i < old_cap; i++) {
    if (old[i].node == CC_NONE) continue;
    u32 slot;
    cc_sig_lookup(s, old[i].hash, old[i].key, old[i].nkey, &slot);
    // re-insert without re-copying: steal the old key buffer
    CcSigEntry *e = &s->sig[slot];
    e->hash = old[i].hash;
    e->node = old[i].node;
    e->nkey = old[i].nkey;
    e->key  = old[i].key;
    s->sig_count++;
  }
  free(old);
}

// Remove the entry for canonical (hash,key) if present.  Open-addressed
// deletion via backward-shift so probe chains stay intact.  RAW: no
// trail record (used by the trailing wrapper and by cc_pop's TR_SIG_PUT
// undo path).
static void cc_sig_remove_raw(CcState *s, u32 hash, const u32 *key, u32 nkey) {
  u32 mask = s->sig_cap - 1u;
  u32 slot;
  u32 removed = cc_sig_lookup(s, hash, key, nkey, &slot);
  if (removed == CC_NONE) return;
  free(s->sig[slot].key);
  s->sig[slot].node = CC_NONE;
  s->sig[slot].key = NULL;
  s->sig_count--;
  // backward-shift deletion
  u32 i = (slot + 1u) & mask;
  while (s->sig[i].node != CC_NONE) {
    u32 home = s->sig[i].hash & mask;
    // is slot within [home, i] cyclically? if so, the entry can move back
    int can_move = (slot - home) % s->sig_cap <= (i - home) % s->sig_cap;
    if (can_move) {
      s->sig[slot] = s->sig[i];
      s->sig[i].node = CC_NONE;
      s->sig[i].key = NULL;
      slot = i;
    }
    i = (i + 1u) & mask;
  }
}

// Trailing remove: record (key -> node) so cc_pop can reinsert it.
static void cc_sig_remove(CcState *s, u32 hash, const u32 *key, u32 nkey) {
  u32 slot;
  u32 removed = cc_sig_lookup(s, hash, key, nkey, &slot);
  if (removed == CC_NONE) return;
  CcTrail *tr = cc_trail_push(s, TR_SIG_DEL);
  tr->a    = removed;
  tr->hash = hash;
  tr->nkey = nkey;
  tr->key  = (u32 *)malloc((nkey ? nkey : 1u) * sizeof(u32));
  for (u32 i = 0; i < nkey; i++) tr->key[i] = key[i];
  cc_sig_remove_raw(s, hash, key, nkey);
}

// === pending queue ===

static void cc_pend_push(CcState *s, u32 a, u32 b) {
  if (s->pend_len >= s->pend_cap) {
    s->pend_cap = s->pend_cap ? s->pend_cap * 2u : 16u;
    s->pend_a = (u32 *)realloc(s->pend_a, s->pend_cap * sizeof(u32));
    s->pend_b = (u32 *)realloc(s->pend_b, s->pend_cap * sizeof(u32));
  }
  s->pend_a[s->pend_len] = a;
  s->pend_b[s->pend_len] = b;
  s->pend_len++;
}

// === lifecycle ===

fn CcState *cc_init(void) {
  CcState *s = (CcState *)calloc(1, sizeof(CcState));
  cc_sig_init(s, 64u);
  return s;
}

fn void cc_free(CcState *s) {
  if (s == NULL) return;
  for (u32 i = 0; i < s->n_nodes; i++) free(s->nodes[i].args);
  free(s->nodes);
  free(s->parent);
  free(s->rank);
  free(s->use_head);
  free(s->use_node);
  free(s->use_next);
  for (u32 i = 0; i < s->sig_cap; i++) free(s->sig[i].key);
  free(s->sig);
  free(s->pend_a);
  free(s->pend_b);
  free(s->ne_a);
  free(s->ne_b);
  for (u32 i = 0; i < s->trail_len; i++) free(s->trail[i].key);
  free(s->trail);
  free(s);
}

// === term construction ===

static u32 cc_new_node(CcState *s) {
  cc_grow_nodes(s, s->n_nodes + 1u);
  u32 n = s->n_nodes++;
  s->parent[n]   = n;
  s->rank[n]     = 0;
  s->use_head[n] = CC_NONE;
  cc_trail_push(s, TR_NODE);   // undo = shrink n_nodes, free its args
  return n;
}

fn u32 cc_atom(CcState *s, u32 atom_id) {
  u32 n = cc_new_node(s);
  s->nodes[n].fn_id   = CC_NONE;
  s->nodes[n].atom_id = atom_id;
  s->nodes[n].nargs   = 0;
  s->nodes[n].args    = NULL;
  return n;
}

fn u32 cc_app(CcState *s, u32 fn_id, const u32 *arg_nodes, u32 nargs) {
  // hash-cons by canonical signature: if an app with the same fn_id and
  // arg class-reps already exists, return it.
  u32 stackbuf[16];
  u32 *key = (nargs + 1u <= 16u) ? stackbuf
                                 : (u32 *)malloc((nargs + 1u) * sizeof(u32));
  // build the canonical key directly (args not yet stored in a node)
  u32 h = 0x811c9dc5u;
  key[0] = fn_id;
  h = (h ^ fn_id) * 0x01000193u;
  for (u32 i = 0; i < nargs; i++) {
    u32 r = cc_find(s, arg_nodes[i]);
    key[1 + i] = r;
    h = (h ^ r) * 0x01000193u;
  }
  u32 slot;
  u32 found = cc_sig_lookup(s, h, key, nargs + 1u, &slot);
  if (found != CC_NONE) {
    if (key != stackbuf) free(key);
    return found;
  }
  // fresh app node
  u32 n = cc_new_node(s);
  s->nodes[n].fn_id   = fn_id;
  s->nodes[n].atom_id = CC_NONE;
  s->nodes[n].nargs   = nargs;
  s->nodes[n].args    = (u32 *)malloc((nargs ? nargs : 1u) * sizeof(u32));
  for (u32 i = 0; i < nargs; i++) s->nodes[n].args[i] = arg_nodes[i];
  // register in the signature table under the canonical key
  cc_sig_put(s, slot, h, key, nargs + 1u, n);
  // add to the use list of each argument's current class rep
  for (u32 i = 0; i < nargs; i++) {
    u32 r = cc_find(s, arg_nodes[i]);
    cc_use_add(s, r, n);
  }
  if (key != stackbuf) free(key);
  return n;
}

// === congruence closure core ===

// Re-canonicalize signature for app `app`: remove its old entry, look up
// the new canonical key; if a different node already holds it, queue a
// merge; otherwise reinsert.
static void cc_canonicalize(CcState *s, u32 app) {
  CcNode *nd = &s->nodes[app];
  u32 stackbuf[16];
  u32 *key = (nd->nargs + 1u <= 16u)
             ? stackbuf
             : (u32 *)malloc((nd->nargs + 1u) * sizeof(u32));
  u32 h = cc_sig_key(s, app, key);
  u32 slot;
  u32 other = cc_sig_lookup(s, h, key, nd->nargs + 1u, &slot);
  if (other == CC_NONE) {
    cc_sig_put(s, slot, h, key, nd->nargs + 1u, app);
  } else if (cc_find(s, other) != cc_find(s, app)) {
    cc_pend_push(s, other, app);
  }
  if (key != stackbuf) free(key);
}

// Process the pending queue to fixpoint, unioning classes and
// propagating congruences through use lists.
static void cc_propagate(CcState *s) {
  while (s->pend_len > 0) {
    s->pend_len--;
    u32 a = s->pend_a[s->pend_len];
    u32 b = s->pend_b[s->pend_len];
    u32 ra = cc_find(s, a);
    u32 rb = cc_find(s, b);
    if (ra == rb) continue;
    // union by rank; keep `ra` as the surviving root
    if (s->rank[ra] < s->rank[rb]) { u32 t = ra; ra = rb; rb = t; }
    // Trail the union: record the redirected child root rb, the
    // surviving root ra, and ra's OLD rank (restored verbatim on undo).
    {
      CcTrail *tr = cc_trail_push(s, TR_UNION);
      tr->a = rb;
      tr->b = ra;
      tr->r = s->rank[ra];
    }
    if (s->rank[ra] == s->rank[rb]) s->rank[ra]++;
    s->parent[rb] = ra;

    // The apps that used rb may now be congruent to others.  Re-canon
    // each, then splice rb's use list into ra's.  Re-canon BEFORE the
    // splice so cc_canonicalize's signature lookup sees the merged
    // class reps (parent[rb] already points at ra).
    u32 c = s->use_head[rb];
    while (c != CC_NONE) {
      cc_canonicalize(s, s->use_node[c]);
      c = s->use_next[c];
    }
    // splice rb's list onto ra's head
    if (s->use_head[rb] != CC_NONE) {
      u32 rb_head = s->use_head[rb];
      u32 tail = rb_head;
      while (s->use_next[tail] != CC_NONE) tail = s->use_next[tail];
      // Trail the splice (its own record; the TR_UNION pointer above may
      // have been invalidated by trail growth during re-canon).
      CcTrail *tr = cc_trail_push(s, TR_USE_SPLICE);
      tr->a = rb;                  // rb: head restored to rb_head
      tr->b = ra;                  // ra: head restored to ra_old_head
      tr->r = rb_head;             // rb's old head
      tr->splice_tail = tail;      // tail->next reset to CC_NONE
      tr->ra_old_head = s->use_head[ra];
      s->use_next[tail] = s->use_head[ra];
      s->use_head[ra] = rb_head;
      s->use_head[rb] = CC_NONE;
    }
  }
}

// === assertions ===

fn void cc_assert_eq(CcState *s, u32 a, u32 b) {
  cc_pend_push(s, a, b);
  cc_propagate(s);
}

fn void cc_assert_ne(CcState *s, u32 a, u32 b) {
  if (s->ne_len >= s->ne_cap) {
    s->ne_cap = s->ne_cap ? s->ne_cap * 2u : 8u;
    s->ne_a = (u32 *)realloc(s->ne_a, s->ne_cap * sizeof(u32));
    s->ne_b = (u32 *)realloc(s->ne_b, s->ne_cap * sizeof(u32));
  }
  s->ne_a[s->ne_len] = a;
  s->ne_b[s->ne_len] = b;
  s->ne_len++;
  cc_trail_push(s, TR_NE);   // undo = shrink ne_len
}

// === check ===

fn CcResult cc_check(CcState *s) {
  for (u32 i = 0; i < s->ne_len; i++) {
    if (cc_find(s, s->ne_a[i]) == cc_find(s, s->ne_b[i])) return CC_UNSAT;
  }
  return CC_SAT;
}

// === backtracking ===

// A checkpoint is just the current trail length: cc_pop undoes every
// record at index >= mark.
fn u32 cc_push(CcState *s) {
  return s->trail_len;
}

// Undo all trailed ops back to `mark`, restoring the state exactly.
// Records replay in strict reverse order; pool cells, node slots, and
// disequalities are reclaimed by shrinking their high-water marks (the
// freshest entry is always the one being undone, so a simple decrement
// is correct).
fn void cc_pop(CcState *s, u32 mark) {
  while (s->trail_len > mark) {
    CcTrail *e = &s->trail[--s->trail_len];
    switch (e->tag) {
      case TR_UNION:
        // e->a = rb (redirected child), e->b = ra (survivor), e->r = ra
        // old rank.  cc_find does no compression, so reverting parent[rb]
        // + rank[ra] restores the partition exactly.
        s->parent[e->a] = e->a;       // rb back to its own root
        s->rank[e->b]   = e->r;        // ra's old rank
        break;
      case TR_USE_SPLICE:
        // restore use_head[ra] and detach rb's list (tail->next = NONE).
        s->use_next[e->splice_tail] = CC_NONE;
        s->use_head[e->b] = e->ra_old_head;   // ra
        s->use_head[e->a] = e->r;             // rb head
        break;
      case TR_SIG_PUT:
        // entry was inserted under (hash,key); remove it.
        cc_sig_remove_raw(s, e->hash, e->key, e->nkey);
        free(e->key);
        break;
      case TR_SIG_DEL:
        // entry (key -> node e->a) was removed; reinsert it.
        {
          u32 slot;
          cc_sig_lookup(s, e->hash, e->key, e->nkey, &slot);
          cc_sig_put_raw(s, slot, e->hash, e->key, e->nkey, e->a);
        }
        free(e->key);
        break;
      case TR_USE:
        // app prepended to use_head[b]; restore the prior head and
        // reclaim the freshest pool cell.
        s->use_head[e->b] = e->r;
        s->use_len--;
        break;
      case TR_NODE:
        // shrink the node store; free the slot's args buffer.
        s->n_nodes--;
        free(s->nodes[s->n_nodes].args);
        s->nodes[s->n_nodes].args = NULL;
        break;
      case TR_NE:
        s->ne_len--;
        break;
      default:
        break;
    }
  }
}
