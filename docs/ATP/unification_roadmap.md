# thvm/atp - Unifying SMT (Z3-class) with Completion

A design roadmap for the question: *what would it take to give thvm
Z3-class SMT capability (many theories, CDCL(T), quantifier
instantiation) unified with its completion / superposition engine,
rather than as a separate bolted-on solver?*

This is a research-grade target, not a checklist. But thvm starts from
a rare position - it already owns **both** a congruence-closure engine
([survey.md](survey.md) "SMT core") **and** a mature completion engine
("equational ATP") - and that changes the shape of the work. The two
are not two things to glue. They are one calculus at two operating
points.

## The keystone: CC and completion are the same algorithm

The conceptual foundation is **abstract congruence closure** (Bachmair,
Tiwari, Vigneron, *Abstract Congruence Closure*, JAR 2003; cf. Kapur,
*Shostak's congruence closure as completion*, 1997): ground congruence
closure is exactly Knuth-Bendix completion run on **ground** equations
with **extension constants** (flattening each subterm `f(a,b)` to a
fresh constant `c` plus the equation `f(a,b) = c`). Ground completion
always terminates and yields a convergent rewrite system whose normal
form is the congruence-class representative - i.e. it *is* congruence
closure.

So thvm's `TSatEUF` (Downey-Sethi-Tarjan CC) and the saturation engine
in `src/atp/` are the **same procedure** at two settings:

| Setting | Terms | Termination | Ordering role | This is... |
|---|---|---|---|---|
| ground, theory-combined | flat constants | always | pick class reps | **SMT** (congruence closure) |
| non-ground, saturating | full terms | by reduction order | control + completeness | **superposition** |

The unification is therefore not "make two engines talk". It is: **run
one saturation engine, let an SMT-style Boolean+theory controller drive
and prune it, and let it drop to the ground/terminating regime exactly
where a decision procedure exists.** That is the same insight behind
hierarchic superposition and DPLL(Gamma); thvm is unusual in already
having the saturation half built and battle-tested.

## Target architecture

```
                +-------------------------------+
                |   CDCL(T) / AVATAR core        |  online Boolean spine,
                |   (clause-splitting oracle)    |  conflict learning
                +---------------+---------------+
                                | asserts ground literals /
                                | selects clause components
     +--------------+----------+-----------+----------------+
     |              |                      |                |
+----v----+   +-----v------+        +------v------+   +-----v---------+
|  EUF    |   | Arithmetic |  ...   |  Arrays /   |   | Superposition |
| (ground |<->|  (Simplex/ |<-N.O.->|  BV / DT    |   | / completion  |
| complet)|   |   FM)      |        |             |   | (1st-order,   |
+----+----+   +------------+        +-------------+   |  modulo T)    |
     |                                                +-------+-------+
     |          abstract congruence closure                  |
     +-------------- shared term + equality substrate --------+
                 emits ground instances (lemmas) upward;
                 receives the core's partial model downward
```

The superposition engine is "just another theory" from the core's view,
but a distinguished one: it is **refutation-complete** for the quantified
equality fragment and it **emits ground instances** the cheap theories
can consume. Heuristic E-matching (over thvm's existing discrimination
tree) is the fast, incomplete instantiation path; superposition is the
complete fallback. Z3 has only the former; Waldmeister/Twee have only
the latter; the union is the goal.

## Phases

Each lands behind a flag; the default engine stays unchanged at every
step (thvm's standing discipline - see [roadmap.md](roadmap.md)).

### Phase 0 - shared substrate: CC as ground completion

Operationalize the keystone. Run `TSatEUF`'s decision **through** the
saturation engine via extension-constant flattening, and differential
it against the standalone CC on the QF_UF bench. Outcome: the term DAG,
discrimination-tree index, and equality reasoning become **one**
substrate that every later theory and the superposition engine share.
Low risk (both halves exist); it is the keystone that makes the eventual
integration *white-box* rather than Vampire-style black-box.

*Reuses:* `TSatEUF`, the saturation engine, `AtpRuleIndex`.

### Phase 1 - online CDCL(T) spine

Replace the current **offline lazy** DPLL(T) (`SatisfiabilityInstances`
+ blocking clauses, see `SMT.wl`) with an **online** CDCL(T) core doing
theory propagation and conflict explanation (Nieuwenhuis, Oliveras,
Tinelli, *Solving SAT and SAT Modulo Theories*, JACM 2006). T_0 = EUF
from Phase 0, upgraded to **proof-producing, incremental** congruence
closure - minimal conflict explanations + assert/backtrack
(Nieuwenhuis-Oliveras). This is the single highest-leverage step: it
fixes the weakest current piece and is the prerequisite for every theory
below.

*Capability gain:* real, scalable QF_UF SMT. *Validation:* SMT-LIB
QF_UF, large Boolean structure where blocking-clause DPLL(T) blows up.

### Phase 2 - second theory + Nelson-Oppen combination

Add linear arithmetic (Simplex for DPLL(T), Dutertre-de Moura, 2006) as
T_1. Combine EUF + LIA/LRA by Nelson-Oppen equality propagation over
shared constants - which the Phase-0 substrate already names - or by
model-based theory combination (de Moura-Bjorner, 2008). Arrays (lazy
axiom instantiation), bitvectors (bit-blast or word-level), and
datatypes then slot into the same combination frame.

*Capability gain:* the first genuinely Z3-like multi-theory ground
decisions (QF_UFLIA, QF_ALIA, ...). *Validation:* the corresponding
SMT-LIB divisions.

### Phase 3 - quantifier instantiation (cheap, incomplete)

E-matching over the discrimination tree (de Moura-Bjorner, *Efficient
E-matching for SMT*, CADE 2007): trigger-driven instantiation feeds
ground clauses into the CDCL(T) core. Add model-based quantifier
instantiation (Ge-de Moura, 2009) for the finite-model-complete
fallback. thvm already has the index E-matching needs.

*Capability gain:* Z3-style quantified SMT - but heuristic and
incomplete. *Validation:* quantified SMT-LIB (UF, UFLIA).

### Phase 4 - superposition modulo theories: the actual unification

Make the completion / superposition engine a **theory solver for the
quantified fragment, running modulo the background theories**
(hierarchic superposition: Bachmair, Ganzinger, Waldmann, 1994; SUP(T) /
Beagle / Zipperposition: Cruanes, 2015). Background-sorted terms are
abstracted and their constraints shipped to the ground solvers;
superposition reasons about the foreground first-order clauses with the
ordering + redundancy machinery thvm already has (KBO/LPO/RPO/WPO,
ground joinability, connectedness). Feedback loop: superposition emits
ground theory lemmas to the CDCL(T) core; the core's partial model
guides superposition's clause selection.

This is where **completeness for first-order equality + theories** comes
from - precisely what pure instantiation (Phase 3) cannot give. It is
the genuine superset of both Z3 (which is incomplete on quantifiers) and
Waldmeister/Twee (which have no background arithmetic).

*Capability gain:* complete (r.e.) first-order-with-equality modulo the
ground theories. *Validation:* TPTP TFF (typed first-order + arithmetic)
and quantified-theory goals where E-matching diverges.

*Reuses:* `src/fol/` (resolution/paramodulation/superposition, CNF,
Skolem), the orderings, the redundancy filters.

### Phase 5 - AVATAR as the unifying frame

Re-cast the whole stack as AVATAR (Voronkov, *AVATAR: The New
Architecture for First-Order Theorem Provers*, CAV 2014): the CDCL(T)
core (Phases 1-2) is the **splitting oracle**; superposition (Phase 4)
is the **saturation engine**; first-order clauses are split on
variable-disjoint components, the SMT core picks the asserted component
set, and theory/saturation conflicts return as SAT clauses. Vampire +
Z3 is the proof that this works at scale. thvm's advantage: because it
owns both engines and shares the abstract-CC substrate (Phase 0), it can
do **AVATAR-modulo-theories with a white-box SMT core** rather than
Vampire's black-box Z3 calls - tighter conflict sharing, one proof
object, one model.

This is the clean end-state architecture; Phases 1-4 converge here.

### Phase 6 - surfaces

Unify the outward interfaces: a single proof object spanning the SMT
resolution proof and the superposition refutation DAG (extend the
existing `s->trace[]` with the per-step substitution - the gap already
noted in [roadmap.md](roadmap.md) "External proof certificates");
incrementality (push/pop); an SMT-LIB 2 front end; and unified model
generation (CC quotient + arithmetic assignment + Herbrand/finite model
from saturation, generalizing today's `CounterexampleObject`).

## Two sequencing strategies

The phases have a partial order, not a total one. Two coherent routes:

* **Bottom-up (SMT-first):** 0 -> 1 -> 2 -> 3, then 4 -> 5. Delivers
  Z3-like *ground* multi-theory capability and heuristic quantifiers
  early and usably, then adds completeness. Lower risk; matches the
  literal "Z3-like" ask; the completion engine waits in the wings until
  Phase 4. **Recommended** when the SMT capability is the goal, because
  Phases 1-2 are the long pole and unlock everything downstream.

* **Top-down (AVATAR-first):** build AVATAR (Phase 5) around the
  *existing* completion engine immediately, using even the current
  offline SMT as a weak splitter, then strengthen the SMT core
  underneath (1 -> 2). Mirrors Vampire's actual history and cashes in
  thvm's mature completion engine on day one.

Either way **Phase 0 comes first** - it is what makes the integration a
true unification (shared substrate) instead of two solvers passing
strings.

## The single highest-leverage first step

**Phase 0 + Phase 1: proof-producing, incremental congruence closure
expressed as ground completion, driving an online CDCL core.** One step
that (a) replaces the weakest current piece (the offline blocking-clause
DPLL(T)), (b) establishes the shared substrate every later phase needs,
and (c) is a hard prerequisite for arithmetic, combination, E-matching,
and AVATAR alike. Everything after it is additive theories on a sound,
shared spine.

The code-grounded build for this step - the `src/cc/` module, the
incremental proof-producing CC, the `src/sat/` CDCL(T) core, and the
flag-gated landing sequence against the current `SMT.wl` / `src/atp/`
seam - is written up in
[unification_phase01_plan.md](unification_phase01_plan.md).

## References

- Bachmair, Tiwari, Vigneron. *Abstract Congruence Closure.* JAR 2003.
- Kapur. *Shostak's Congruence Closure as Completion.* RTA 1997.
- Nieuwenhuis, Oliveras, Tinelli. *Solving SAT and SAT Modulo Theories.*
  JACM 2006. (the DPLL(T) abstract framework)
- Nieuwenhuis, Oliveras. *Proof-Producing Congruence Closure.* RTA 2005.
- Dutertre, de Moura. *A Fast Linear-Arithmetic Solver for DPLL(T).*
  CAV 2006.
- de Moura, Bjorner. *Model-based Theory Combination.* 2008; *Efficient
  E-matching for SMT Solvers.* CADE 2007.
- Ge, de Moura. *Complete Instantiation for Quantified SMT.* CAV 2009.
- Bachmair, Ganzinger, Waldmann. *Refutational Theorem Proving for
  Hierarchic First-Order Theories.* 1994. (hierarchic superposition)
- Cruanes. *Extending Superposition with Integer Arithmetic, ...*
  (Zipperposition) PhD thesis, 2015.
- Voronkov. *AVATAR: The New Architecture for First-Order Theorem
  Provers.* CAV 2014. (and AVATAR-modulo-theories follow-ups)

## See also

- [survey.md](survey.md) - where thvm sits today (the starting point).
- [roadmap.md](roadmap.md) - the per-arc C-engine coverage, including
  the "Theory reasoning" arc that Phase 2 fills in.
- [algorithms.md](algorithms.md) - the completion calculus this builds on.
