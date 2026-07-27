// ===================================================================
// Existential-goal equals encoding (Twee addNarrowing port)
// ===================================================================
//
// Twee never narrows a non-ground goal directly: addNarrowing
// (twee executable/SequentialMain.hs:721) encodes each existential
// goal  s != t  (a variable in the re-negated conjecture is
// existentially quantified) equationally --
//     eq(X,X) = true          (reflexivity)
//     eq(s,t) = false         (one per non-ground goal; the goal's
//                              variables turn universally quantified)
//     true   != false         (the single GROUND replacement goal)
// -- and completion narrows the eq(s,t) axiom at its argument
// positions until the two arguments unify, at which point the root
// overlap with reflexivity yields true = false: the refutation closes
// through an ordinary ground-goal join, no narrowing lane involved.
//
// Conservativity (Twee tags the step "esa" -- equisatisfiable): with
// eq/true/false FRESH symbols,
//   (=>) if S u { s != t } is unsatisfiable, some instance s@ = t@ is
//        S-derivable (Birkhoff), so eq(s@,t@) rewrites to true via
//        reflexivity AND to false via the goal axiom: true = false,
//        contradicting the new goal -- the encoding is unsatisfiable.
//   (<=) a model M of S u { forall X. s != t } extends to the
//        encoding: the goal instances give two distinct elements
//        a != b; set true^M = a, false^M = b, eq^M(u,v) = a if u = v
//        else b.  Reflexivity holds, each eq(s,t) = false holds
//        (every instance of s,t differs in M), true != false holds --
//        the encoding is satisfiable.
// Unsatisfiable therefore transfers both ways.  A saturation under
// the encoding still reports GaveUp, never Satisfiable
// (g_cli_equals_applied, the g_cli_*_applied gate pattern) -- belt
// and suspenders on top of the g_cli_goal_existential gate.
//
// OPT-IN (--equals-goal=1), not the default: the engine's narrowing
// lane decides most of the corpus's existential goals instantly
// (COL004-1 proves at 0.0s narrowing but TIMES OUT encoded), while
// the encoding cracks the goals narrowing starves on (COL009-1
// proves at 0.1s encoded but times out narrowing) -- complementary
// lanes, so the schedule runs the encoding as its own slice.
// --equals-goal=2 additionally EXITS (give-up) when the encoding
// does not apply, so the slice never duplicates the WM preset on
// ground-goal (or .pr) problems.

static int g_cli_equals_applied = 0;

