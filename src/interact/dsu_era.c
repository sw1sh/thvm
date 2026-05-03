// &(&{}){a, b}
// ------------ DSU-ERA
// &{}
//
// Label is ERA -- the whole DSU collapses to ERA.  HVM4 returns
// ERA at the SUP root; thvm follows.  a and b are dropped (they're
// reachable only through this DSU; under linear discipline both
// were single-use and will be GC'd).
fn Term interact_dsu_era(void) {
  ITRS++;
  return term_new(0, TAG_ERA, 0, 0);
}
