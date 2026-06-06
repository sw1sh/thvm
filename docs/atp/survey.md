# thvm/atp - Where thvm Sits Among Theorem Provers

A positioning survey: what thvm's reasoning stack covers today, and
how that compares to the established automated-reasoning tools -
SMT solvers, first-order ATPs, and unit-equational provers.

The short version: thvm is a **mature unit-equational (UEQ) prover**
with a **complete-but-single-theory SMT core** alongside it. It is
deliberately *not* a general SMT solver, and this document is precise
about which claims thvm can and cannot make.

For the algorithms, see [algorithms.md](algorithms.md); for what is
implemented vs open, [roadmap.md](roadmap.md). The WL surface that
wraps it all is in `wl/THVMLink/docs/Tutorials/` (`ATP.md`, `SMT.md`,
`Disproof.md`, `TPTPImport.md`).

## Two engines, not one

The single most important fact for a reader comparing thvm to Z3 or
Vampire: thvm's "SMT framework" and thvm's "theorem prover" are
**different engines with different maturity**, dispatched by problem
shape behind one `TFindProof` entry point.

| | Engine | Decides | Quantifiers? | Source |
|---|---|---|---|---|
| **SMT core** | congruence closure + lazy DPLL(T) | QF_UF (ground equality + UF), and Boolean combinations of equality atoms | no - ground only | `wl/.../ATP/SMT.wl` |
| **Equational ATP** | unfailing Knuth-Bendix completion | unit-equational (UEQ) theories | yes - universally quantified equations | `src/atp/` |
| **FOL layer** | superposition / resolution given-clause | first-order logic with equality | yes - full CNF + Skolem | `src/fol/` |

A reader who expects "SMT" to mean "Z3-class, many theories,
quantifier instantiation" will be surprised: in thvm the **SMT path
is ground-only and single-theory**, and everything quantified is
handled by the completion / FOL engines instead. That split is the
defining architectural fact of the stack.

## The SMT core: complete, but one theory deep

`SMT.wl` implements exactly one SMT-LIB theory: **QF_UF**, the
quantifier-free theory of equality with uninterpreted functions.

* `TSatEUF` - congruence closure (Downey-Sethi-Tarjan union-find over
  the subterm DAG). A **sound and complete decision procedure** for
  ground QF_UF: it decides satisfiability of a conjunction of ground
  equalities / disequalities, and on SAT returns the quotient (one
  element per congruence class) as a finite model.
* `TSmtDecide` - a Boolean shell over `TSatEUF` via **lazy
  (offline) DPLL(T)**: abstract each equality atom to a propositional
  variable, get a propositional model from Wolfram
  `SatisfiabilityInstances`, check it with congruence closure, and on
  a theory conflict add a blocking clause and re-query. Decides
  arbitrary Boolean combinations of equality atoms.
* `Method -> "SMT"` on `TFindProof` - ground entailment, returning a
  verdict Association on a proved goal and a `CounterexampleObject`
  (the refuting quotient model) on a refuted one. See
  [Disproof.md](../../wl/THVMLink/docs/Tutorials/Disproof.md).

Within QF_UF this is the real thing - complete, and with a
first-class **prove-or-disprove duality** (the countermodel surface)
that most SMT cores keep internal. What it is *not*:

* **Not multi-theory.** No linear/nonlinear integer or real
  arithmetic (LIA/LRA/NIA/NRA), bitvectors (BV), arrays (AX),
  algebraic datatypes (DT), strings, floating point (FP), or sets.
  These are the theories that make a tool a general SMT solver; thvm
  has none of them.
* **Not online CDCL(T).** The DPLL(T) integration is the textbook
  *lazy* form - a black-box propositional solver re-queried with
  blocking clauses - not the *online* CDCL(T) that production solvers
  use (incremental theory propagation, theory-lemma learning,
  conflict-driven backjumping through the theory). It is correct but
  asymptotically weaker on hard Boolean structure.
* **Not incremental.** No assertion stack / push-pop, no
  named-assertion unsat cores, no SMT-LIB 2 input or output.
* **No SMT-style quantifiers.** No E-matching, no model-based
  quantifier instantiation (MBQI). Quantified goals never touch this
  engine.

### vs the SMT solver field

| Capability | thvm | Z3 / CVC5 / Yices / MathSAT / Bitwuzla |
|---|---|---|
| QF_UF (equality + UF) | yes, complete | yes |
| Boolean combination (DPLL/CDCL over theory atoms) | yes, lazy DPLL(T) | yes, online CDCL(T) |
| Linear arithmetic (LIA / LRA) | no | yes |
| Nonlinear arithmetic (NIA / NRA) | no | partial-to-yes |
| Bitvectors (BV) | no | yes |
| Arrays (AX) | no | yes |
| Datatypes / strings / FP / sets | no | yes (varies by solver) |
| Quantifiers (E-matching / MBQI) | no (handled by ATP engine instead) | yes |
| Incremental solving (push/pop) | no | yes |
| Unsat cores / proofs | countermodel on SAT only | yes |
| SMT-LIB 2 I/O | no | yes |
| Model / countermodel on SAT | yes (`CounterexampleObject`) | yes |

Read this matrix as: **thvm covers the equality column completely and
the rest not at all.** EUF is the theory every SMT solver shares and
the one thvm reasons about natively; the differentiator of the SMT
field - the *combination* of EUF with arithmetic, bitvectors, and
arrays under Nelson-Oppen / model-based theory combination - is
entirely outside thvm's current scope.

## The equational ATP: a peer-level UEQ prover

This is where thvm is genuinely competitive. The unfailing
Knuth-Bendix completion engine (`src/atp/_.c`) is a serious
**unit-equational** prover in the lineage of Waldmeister and Twee:

* Reduction orderings KBO, LPO, RPO, WPO with automatic precedence.
* Discrimination-tree + FV term indexing.
* Redundancy: trivial join, forward / queue / permutation
  subsumption, ground joinability, Bachmair-Dershowitz connectedness.
* AC reasoning (`src/atp/ac.c`): AC match/canon, AC-KBO/AC-LPO,
  Stickel-style AC unification, extended overlaps. On the
  `test_atp_ac_bench` differential it **out-completes `wmcli`** on
  commutative-monoid, abelian-group inversion, lattice idempotence,
  and `0*a=0` (fewer rules / CPs).
* Prover-emulation presets (`VampireUEQ`, `Twee`, `Waldmeister`,
  `EProver`) and portfolios on the WL `Method` surface, plus an
  external-CLI lift to the real Vampire / Twee / Waldmeister / E
  binaries.

### vs UEQ and FOL provers

| Capability | thvm | Waldmeister / Twee (UEQ) | Vampire / E (FOL) |
|---|---|---|---|
| Unfailing completion (UEQ) | yes | yes | yes (subsumed by superposition) |
| KBO / LPO / RPO / WPO orderings | yes | KBO / LPO | yes |
| AC reasoning | yes | partial | yes |
| Ground-joinability / connectedness | yes (KBO) | yes | yes |
| Full FOL clauses (resolution + paramodulation) | yes, un-indexed | no | yes, indexed |
| Superposition with literal selection | partial | no | yes |
| AVATAR / clause splitting | no | no | yes |
| Term indexing for FOL subsumption | no (O(n^2)) | n/a | yes |
| TPTP input | yes (via `WolframParser`) | yes | yes |
| Replayable proof certificate (Coq/Lean) | no (DAG only) | no | yes (TSTP/PCL) |

thvm's UEQ engine sits **at parity with the dedicated UEQ provers**
and ahead on several AC benches. Its FOL layer (`src/fol/`) is real -
binary resolution, factoring, paramodulation, equality factoring,
NNF/Skolem/CNF (naive + Tseitin), forward+backward demodulation,
selection, proof DAG; it closes Pelletier P1-P19 - but it is
**un-indexed** (O(n^2) subsumption + CP scans) and lacks AVATAR and
maximal-literal selection, so it is far from Vampire/E throughput on
large FOL.

## Honest gaps and what closing them takes

In rough cost order, the work that would move thvm toward "general
SMT" parity:

1. **A real theory beyond EUF.** Linear integer/real arithmetic is
   the highest-value first addition (it unlocks most practical SMT
   queries). This is the [roadmap.md](roadmap.md#theory-reasoning)
   "Theory reasoning" arc: a Simplex / Fourier-Motzkin theory solver
   plus Nelson-Oppen combination with the existing EUF solver.
2. **Online CDCL(T).** Replace the blocking-clause re-query with an
   incremental SAT core that does theory propagation and learns
   theory lemmas. Without this the Boolean shell does not scale.
3. **SMT-LIB 2 front end + incrementality.** The standard interface
   (assertion stack, `check-sat`, `get-model`, `get-unsat-core`) is
   what makes a solver usable as a backend by other tools.
4. **Theory-aware quantifiers (E-matching / MBQI).** Today quantified
   goals are routed to completion/FOL instead; an SMT-native
   instantiation loop would let the ground theories handle quantified
   theory goals.
5. **Replayable proof certificates.** The UEQ trace records
   (before, after) Term pairs but not the per-step substitution; the
   FOL layer prints a refutation DAG. Emitting a TSTP/PCL-style proof
   (cf. roadmap "External proof certificates") would let an external
   checker (Coq/Lean/Isabelle) replay a thvm proof.

## One-paragraph positioning

thvm is best described as **a modern unit-equational theorem prover
(peer to Waldmeister/Twee, ahead on AC) with an emerging first-order
superposition layer and a complete-but-single-theory (EUF) SMT
core**. Its SMT framework decides QF_UF and Boolean combinations of
equality atoms soundly and completely, and uniquely exposes
countermodels as a first-class prove-or-disprove duality - but it
covers exactly one SMT-LIB theory, integrates the propositional
search lazily rather than as online CDCL(T), and has no arithmetic,
bitvector, array, incremental-solving, or SMT-LIB surface. Reasoning
that other tools call "SMT with quantifiers" thvm instead routes to
its completion / FOL engines. It should be compared to Z3/CVC5 only on
the EUF fragment, and to Vampire/E/Waldmeister on equational and
first-order proving, where it is genuinely competitive.

## See also

- [roadmap.md](roadmap.md) - the per-arc coverage detail behind this
  survey, including the "Theory reasoning" arc that would add a
  second SMT theory.
- [algorithms.md](algorithms.md) - the math the engines implement.
- `wl/THVMLink/docs/Tutorials/SMT.md` - the user-facing QF_UF /
  DPLL(T) decision surface.
- `wl/THVMLink/docs/Tutorials/Disproof.md` - the countermodel /
  prove-or-disprove duality.
