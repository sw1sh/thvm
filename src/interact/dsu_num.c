// &(#n){a, b}
// ----------- DSU-NUM
// SUP^n{a, b}
//
// The DSU's label term reduced to NUM(n).  Read a and b out of the
// DSU's heap cells, build a plain SUP^n on them, return.  We reuse
// the DSU's heap loc for the SUP -- both layouts have a/b at offset
// +1/+2, so we just write the new SUP at a fresh 2-cell block.
fn Term interact_dsu_num(u64 dsu_loc, Term lab_num) {
  u32 lab = (u32)term_val(lab_num);
  Term a  = heap_read(dsu_loc + 1);
  Term b  = heap_read(dsu_loc + 2);
  u64 c   = heap_alloc(2);
  heap_set(c + 0, a);
  heap_set(c + 1, b);
  ITRS++;
  multi_emit(RULE_DSU_NUM, MULTI_SLIDE, 0, (u64)lab_num, 0);
  return term_new(0, TAG_SUP, lab, c);
}
