// schedule/gc_mark.c - the preserve mark: every buffer a live root reaches is
// marked preserved, so the pool rollback and the forward reclaim keep it.
// Nodes are walked by term_cells, the collector's own size table: a dyn term
// in HEAP, a def (DEFS, an unforced ALO's cell 0) in BOOK_HEAP, which holds
// book terms only.  The walk never forces an ALO, so it allocates nothing.

typedef struct { Term t; u8 book; } GcMarkItem;

typedef struct {
  u8         *dyn, *book;          // visited node bases, per HEAP / BOOK_HEAP loc
  u64         n_dyn, n_book;
  GcMarkItem *stack;
  u64         len, cap;
} GcMark;

// Set only by mark_gc_preserve_host_roots: a reached tensor also keeps the
// lazy gradient in its TenDesc.grad, which a later backward() accumulates
// into even after the frontend dropped its own grad handle.
static u8 *GC_MARK_GRAD_SEEN = NULL;

static void gc_mark_push(GcMark *m, Term t, u8 book) {
  if (t == 0) return;
  if (m->len == m->cap) {
    u64 cap = m->cap ? 2 * m->cap : 4096;
    GcMarkItem *s = (GcMarkItem *)realloc(m->stack, cap * sizeof(GcMarkItem));
    if (s == NULL) thvm_fatal("gc_mark: out of memory for the mark stack");
    m->stack = s;
    m->cap   = cap;
  }
  m->stack[m->len++] = (GcMarkItem){ t, book };
}

static void gc_mark_drain(GcMark *m) {
  while (m->len > 0) {
    GcMarkItem it  = m->stack[--m->len];
    Term       t   = it.t;
    u8         tag = term_tag(t);
    if (tag == TAG_TEN) {
      u32 tid = (u32)term_val(t);
      tensor_mark_buf_preserved(tid);
      if (GC_MARK_GRAD_SEEN != NULL && tid < TENS_NEXT && !GC_MARK_GRAD_SEEN[tid]) {
        GC_MARK_GRAD_SEEN[tid] = 1;
        gc_mark_push(m, TENS[tid].grad, 0);
      }
      continue;
    }
    if (tag == TAG_REF) {
      u32 name = term_ext(t);
      if (name < DEFS_CAP) gc_mark_push(m, DEFS[name], 1);
      continue;
    }
    Term const *mem  = it.book ? BOOK_HEAP : HEAP;
    u8         *seen = it.book ? m->book : m->dyn;
    u64         lim  = it.book ? m->n_book : m->n_dyn;
    u64         loc  = term_val(t);
    u32         n    = term_cells(t, mem);
    if (n == 0 || loc + n > lim || seen[loc]) continue;
    seen[loc] = 1;
    // An unforced ALO's cell 0 is a book term, its NUM state in cell 1.
    u8 alo_book = !it.book && tag == TAG_ALO && term_tag(mem[loc + 1]) == TAG_NUM;
    for (u32 i = 0; i < n; i++) {
      gc_mark_push(m, mem[loc + i], it.book || (i == 0 && alo_book));
    }
  }
}

static int gc_mark_begin(GcMark *m) {
  memset(m, 0, sizeof *m);
  m->n_dyn  = HEAP_NEXT > 0 ? HEAP_NEXT : 1;
  m->n_book = BOOK_NEXT > 0 ? BOOK_NEXT : 1;
  m->dyn    = (u8 *)calloc(m->n_dyn, 1);
  m->book   = (u8 *)calloc(m->n_book, 1);
  if (m->dyn == NULL || m->book == NULL) {
    free(m->dyn);
    free(m->book);
    return 0;
  }
  return 1;
}

static void gc_mark_end(GcMark *m) {
  gc_mark_drain(m);
  free(m->stack);
  free(m->dyn);
  free(m->book);
}

// Marks what `roots` reach and nothing else; 0 when a bitmap can't be
// allocated.
fn int gc_mark_terms(Term const *roots, u32 n) {
  GcMark m;
  if (!gc_mark_begin(&m)) return 0;
  for (u32 i = 0; i < n; i++) gc_mark_push(&m, roots[i], 0);
  gc_mark_end(&m);
  return 1;
}

// The whole root set: `result`, `extra` (n_extra more roots), WNF_LAST_STACK,
// DEFS and the WL-pinned Terms, uncapped (a Krea-scale capture pins tens of
// thousands of wrappers; a dropped one would let the capture's mark-park sweep
// recycle a live weight).  Returns 0 when a bitmap can't be allocated: the
// caller must then skip its sweep, as the marks would be incomplete.
fn int mark_gc_preserve_all_roots_plus(Term result, Term const *extra, u32 n_extra) {
  GcMark m;
  if (!gc_mark_begin(&m)) return 0;
  gc_mark_push(&m, result, 0);
  for (u32 i = 0; i < n_extra; i++) gc_mark_push(&m, extra[i], 0);
  for (u32 i = 0; i < WNF_LAST_STACK_LEN; i++) gc_mark_push(&m, WNF_LAST_STACK[i], 0);
  for (u32 i = 0; i < DEFS_CAP; i++) gc_mark_push(&m, DEFS[i], 1);
  for (u32 i = 0; i < EXTERN_PINNED_TERMS_LEN; i++) gc_mark_push(&m, EXTERN_PINNED_TERMS[i], 0);
  gc_mark_end(&m);
  return 1;
}

fn int mark_gc_preserve_all_roots(Term result) {
  return mark_gc_preserve_all_roots_plus(result, NULL, 0);
}

fn void mark_gc_preserve(Term result) {
  (void)mark_gc_preserve_all_roots(result);
}

// The host's root set for a table-wide buffer reclaim: every Term the host
// holds (`roots`, e.g. each live py Tensor's lazy or realized term) plus the
// runtime roots above, following TenDesc.grad of every tensor reached.
// Returns 0 when a bitmap can't be allocated; the caller must then free
// nothing.
fn int mark_gc_preserve_host_roots(Term const *roots, u32 n) {
  u64 tens = TENS_NEXT > 0 ? TENS_NEXT : 1;
  GC_MARK_GRAD_SEEN = (u8 *)calloc(tens, 1);
  if (GC_MARK_GRAD_SEEN == NULL) return 0;
  int ok = mark_gc_preserve_all_roots_plus(0, roots, n);
  free(GC_MARK_GRAD_SEEN);
  GC_MARK_GRAD_SEEN = NULL;
  return ok;
}
