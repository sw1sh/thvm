// A Term -> Term map that grows at half load: the memo of uop_substitute and
// uop_graph_rewrite and the seen set of UOpWalk, as tinygrad keeps a dict or a
// set.  A key is never 0 (a UOp or atom Term); a zeroed UOpMap is empty.
typedef struct {
  Term *keys;
  Term *vals;
  u32   cap;     // a power of two, or 0 before the first put
  u32   len;
} UOpMap;

static u32 uop_map_slot(UOpMap const *m, Term k) {
  u64 x = k;
  x ^= x >> 33; x *= 0xff51afd7ed558ccdULL;
  x ^= x >> 33; x *= 0xc4ceb9fe1a85ec53ULL;
  x ^= x >> 33;
  u32 i = (u32)x & (m->cap - 1);
  while (m->keys[i] != 0 && m->keys[i] != k) {
    i = (i + 1) & (m->cap - 1);
  }
  return i;
}

static void uop_map_grow(UOpMap *m) {
  u32    cap = m->cap ? 2 * m->cap : 64;
  UOpMap g   = { (Term *)calloc(cap, sizeof(Term)), (Term *)calloc(cap, sizeof(Term)), cap, 0 };
  if (g.keys == NULL || g.vals == NULL) {
    thvm_fatal("uop_map: out of memory");
  }
  for (u32 i = 0; i < m->cap; i++) {
    if (m->keys[i] != 0) {
      u32 j = uop_map_slot(&g, m->keys[i]);
      g.keys[j] = m->keys[i];
      g.vals[j] = m->vals[i];
      g.len++;
    }
  }
  free(m->keys);
  free(m->vals);
  *m = g;
}

// Sets k's value; 1 when k was not in the map.
fn int uop_map_put(UOpMap *m, Term k, Term v) {
  if (2 * (m->len + 1) > m->cap) {
    uop_map_grow(m);
  }
  u32 i     = uop_map_slot(m, k);
  int added = m->keys[i] == 0;
  if (added) {
    m->keys[i] = k;
    m->len++;
  }
  m->vals[i] = v;
  return added;
}

// 1, with k's value in *v, when k is in the map.
fn int uop_map_get(UOpMap const *m, Term k, Term *v) {
  if (m->cap == 0) {
    return 0;
  }
  u32 i = uop_map_slot(m, k);
  if (m->keys[i] != k) {
    return 0;
  }
  *v = m->vals[i];
  return 1;
}

fn void uop_map_free(UOpMap *m) {
  free(m->keys);
  free(m->vals);
  memset(m, 0, sizeof *m);
}
