// FindProof: thvm's ATP completion engine as a standalone CLI --
// the CASC UEQ entry.  (Former name: thvm-atp; bin/thvm-atp remains an
// alias.)
//
// Exposes the same knob surface as TFindProof's Method option, mapped
// onto the engine's env-var toggles, and runs the exact code path of
// tests/test_atp_wolfram_bench.c (this file includes that translation
// unit with its main() renamed, so a CLI run is byte-identical to a
// bench run given the same knobs).
//
//   thvm-atp [--preset=waldmeister|waldmeister-lazy|none]
//            [--<knob>[=<value>] ...]
//            [--max-steps=N] [--wall=SEC] [--szs] [--proof]
//            problem.pr | problem.p
//
// CASC compatibility: a TPTP CNF UEQ problem (unit equalities, one or
// more negative units) is converted in-process to Waldmeister .pr form
// -- include('Axioms/X.ax') directives resolve against the problem's
// directory, $TPTP, and the CWD, and include('X', [n1, ...]) keeps only
// the listed formulas -- and the SZS result line uses the
// CNF ontology (Unsatisfiable / Satisfiable / GaveUp / ResourceOut /
// Timeout).  --proof prints an SZS CNFRefutation block: the input
// clauses, the derivation of every rule the closing rewrite chain
// cites (walked from the completion trace), and the goal chain itself.
//
// A multi-goal CONCLUSION block (or several negative TPTP units) is
// proved as a conjunction: every goal must join.  For several negative
// units that is sound for Unsatisfiable but stricter than refutation
// requires (any single one suffices); CASC UEQ problems carry exactly
// one.
//
// Encoded-Horn preprocessing: problems that encode Horn logic through
// a dispatcher axiom ifeqN(X,X,Y,Z) = Y (TPTP's -10 UEQ class) are
// decoded and a budgeted forward-chaining release loop adds the
// modus-ponens consequences as extra unit equations before the engine
// runs -- see tp_horn_release.  --horn=0 disables.
//
// FOF front-end: fof() records (and cnf() records that are not unit
// equations) are clausified (src/fol NNF + Skolemization +
// distribution; Tseitin on distribution blow-up) and equationalized:
// the Horn fragment with exactly that ifeq scheme, non-Horn clauses
// through a sound-but-incomplete or/eq reification -- see tp_fof_load.
// Success on a conjecture reports SZS Theorem; failure is always
// GaveUp (never CounterSatisfiable).  --fof=0 disables; --fof-or=0
// restores the Horn-only give-up on non-Horn input.

#define main atp_bench_main
#include "../tests/test_atp_wolfram_bench.c"
#undef main

// Concurrent-portfolio process management (run_schedule).
#include <sys/wait.h>
#include <sys/time.h>
#include <signal.h>
#include <fcntl.h>
#if defined(__linux__)
#include <sys/prctl.h>
#endif

// TPTP include() expansion: the checker's problem loading and the TPTP
// front-end below read problems through the same code.
#include "atp_cli/tptp_include.c"

// --check mode: validate a first-order TSTP proof (the ProoVer task).
// Reuses the src/fol saturation engine already linked into this binary.
#include "proof_check.c"

#include "atp_cli/knobs.c"
#include "atp_cli/tptp_ueq.c"
#include "atp_cli/equals_encode.c"
#include "atp_cli/def_unfold.c"
#include "atp_cli/curry.c"
#include "atp_cli/canon_input.c"
#include "atp_cli/horn_release.c"
#include "atp_cli/horn_back.c"
#include "atp_cli/fof_prepass.c"
#include "atp_cli/provenance.c"
#include "atp_cli/fof_split.c"
#include "atp_cli/szs.c"
#include "atp_cli/ifeq10.c"
#include "atp_cli/main.c"
