// (ANN val (θx. body))
// --------------------- ANN-BRI  (type erasure)
// = body[x ← val]
//
// The simplest of the three ICC reductions: when the type is itself
// a Bridge, the annotation flushes -- the bridge's bound x is
// substituted by val and the body is returned as-is.  Same shape
// as APP-LAM applied "from the right".

fn Term interact_ann_bri(Term val, Term bri) {
  ITRS++;
  multi_emit(RULE_ANN_BRI, MULTI_TERM, (u64)val, (u64)bri, 0);
  u64  bri_loc = term_val(bri);
  Term body    = heap_read(bri_loc);
  heap_subst_var(bri_loc, val);
  return body;
}
