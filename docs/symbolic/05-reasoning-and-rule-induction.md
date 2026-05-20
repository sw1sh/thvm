# 5. Reasoning engines and rule/program induction

This page exists because the repo already owns one half of the
"neural-proposes / symbolic-verifies" pattern -- [waldmeister](../../waldmeister)
is a real equational theorem prover sitting in the tree -- while the
`brain/experiments/` arc keeps hitting the *other* half: it has no
mechanism for inducing the rule that governs a game. So this page
surveys reasoning ENGINES (theorem provers, term rewriting, e-graphs)
and rule/program INDUCTION (inductive logic programming, Bayesian
program induction, analogy), and ties each to the engine and the
experiments already in this repo. Classical logical foundations
(syntax, semantics, soundness/completeness) are deferred to page 06;
here the focus is engines and learning.

(Abbreviations are page-local, expanded on first use here. AI =
artificial intelligence. KRR = knowledge representation and reasoning.
ATP = automated theorem proving. FOL = first-order logic. SAT =
Boolean satisfiability. SMT = satisfiability modulo theories. DPLL =
Davis-Putnam-Logemann-Loveland. CDCL = conflict-driven clause
learning. KB = Knuth-Bendix. KBO = Knuth-Bendix ordering. TRS = term
rewriting system. CP = critical pair. ILP = inductive logic
programming. MIL = meta-interpretive learning. dILP = differentiable
ILP. BPL = Bayesian program learning. PPL = probabilistic programming
language. DSL = domain-specific language. WL = Weisfeiler-Lehman.
SMT/SME collision avoided: SME = Structure-Mapping Engine. ARC =
Abstraction and Reasoning Corpus. LLM = large language model. MPC =
model-predictive control.)

## 1. Automated reasoning: resolution, superposition, SAT/SMT

The spine of classical ATP is **resolution** (J. A. Robinson, "A
Machine-Oriented Logic Based on the Resolution Principle," *Journal of
the ACM* 12:23-41, 1965, doi:10.1145/321250.321253). Resolution is a
single sound-and-complete inference rule for FOL refutation: negate
the goal, Skolemize to clauses, and derive the empty clause by
repeated unification-driven resolving. It is the substrate everything
else generalizes.

The modern workhorse for *equational* FOL is the **superposition
calculus** (Bachmair & Ganzinger, "Rewrite-Based Equational Theorem
Proving with Selection and Simplification," *Journal of Logic and
Computation* 4(3):217-247, 1994). Superposition is exactly the merger
this doc set keeps circling: it is resolution generalized to handle
equality by ordering-based rewriting, i.e. resolution fused with
unfailing Knuth-Bendix completion (section 2). It is refutationally
complete and is what the leading first-order provers run -- E (Schulz)
and Vampire (Kovacs & Voronkov, "First-Order Theorem Proving and
Vampire," CAV 2013, doi:10.1007/978-3-642-39799-8_1). E describes
itself as a *purely* equational prover: it simulates non-equational
inference via equality, which is precisely waldmeister's worldview
scaled to clauses.

For the propositional and decidable-fragment end:

- **SAT.** Branch-and-propagate descends from DPLL (Davis & Putnam
  1960; Davis, Logemann, Loveland 1962). Modern solvers add
  **CDCL** -- on a conflict, analyze the implication graph, learn a
  clause that forbids the conflict, and backjump non-chronologically
  (Marques-Silva & Sakallah, GRASP, 1996/1999). CDCL is why SAT is an
  industrial technology rather than a worst-case-exponential curiosity.
- **SMT.** SAT lifted with background theories (arithmetic,
  bit-vectors, arrays, uninterpreted functions). The canonical engine
  is **Z3** (de Moura & Bjorner, "Z3: An Efficient SMT Solver," TACAS
  2008, LNCS 4963, doi:10.1007/978-3-540-78800-3_24). Z3 is the
  verifier of choice when "symbolic-verifies" means checking a
  candidate against hard constraints.

Where ML touches this: **premise selection** and **learned proof
guidance** -- ranking which lemmas/clauses to try next -- is the live
research frontier, and the strongest demonstrated systems wrap a
learned proposer around a sound checker. That line (Lean tactics,
AlphaProof's AlphaZero-style search over formal Lean proofs) is
covered in [02-integration.md](02-integration.md) and is not repeated
here. The honest status: learned guidance reliably *speeds up* search;
it does not replace the sound symbolic core, and claims that an LLM
"proves" theorems without a checker remain hype.

## 2. Term rewriting and Knuth-Bendix completion (waldmeister)

This is the repo's native instance, so it gets the most weight.

A **term rewriting system** is a set of directed equations (rules)
`l -> r`; you compute by repeatedly replacing instances of `l` with
`r`. Two properties matter:

- **Termination** -- no infinite rewrite sequence. Proved with a
  reduction order; the classic one is the **Knuth-Bendix ordering**
  (a weight per function symbol plus a precedence), which is exactly
  what waldmeister uses.
- **Confluence** -- rewriting is order-independent (any two reduction
  paths can be rejoined). For a terminating system, confluence is
  decidable by checking **critical pairs**: the overlaps where two
  rules could fire at the same position. If every critical pair
  rewrites to a common term (is "joinable"), the system is confluent.

**Knuth-Bendix completion** (Knuth & Bendix, "Simple Word Problems in
Universal Algebras," 1970) takes a set of equations and tries to
orient them into a terminating, confluent TRS by repeatedly: computing
critical pairs, normalizing them, and adding any non-joinable pair
back as a new oriented rule. If it succeeds, you get a **decision
procedure** for the equational theory: two terms are equal iff their
normal forms are syntactically identical. Plain completion can fail
(an equation that no order can orient); **unfailing completion**
(Bachmair, Dershowitz, Plaisted) keeps such equations as
two-way-usable and gives a *semi-decision* procedure that is complete
as a proof method.

**waldmeister** ([../../waldmeister](../../waldmeister); the name is
German for the woodruff plant, *Galium odoratum*) is precisely an
unfailing-completion engine. Its own short documentation states it is
"based on unfailing Knuth-Bendix completion employed as proof
procedure" for equational logic, with a KBO carrying a per-symbol
weight. In *completion* mode it tries to derive a finite confluent
system (the German term in the completion literature is
*Vervollstaendigung*, literally "completion"); in *proof* mode it runs
until all hypotheses reduce to true. Waldmeister was for years the
reference unit-equational ATP system, repeatedly topping the UEQ
division of the CASC prover competition. For this repo it is a
ready-made, audited symbolic engine: a substrate for searching over
rewrite rules, which is the same machinery a DSL-based ARC rule
inducer needs ([04-implications-for-thvm.md](04-implications-for-thvm.md)).

## 3. E-graphs and equality saturation (egg)

An **e-graph** (equivalence graph) compactly represents a congruence
relation over a huge set of equivalent terms: e-nodes grouped into
e-classes, with congruence closure maintained as you assert
equalities. E-graphs originated in 1970s theorem provers (the same
era as completion) but their modern, optimization-facing
reincarnation is **equality saturation**: assert a term, apply a set
of rewrite rules *non-destructively* until the e-graph stops growing
(saturates) or hits a budget, then **extract** the cheapest
equivalent term by a cost model. Crucially, unlike completion or a
classic rewrite engine, you never have to choose a rewrite *direction*
and never destroy the original -- you keep all rewrites simultaneously
and pick the best at the end, which dodges the phase-ordering problem
that plagues compilers.

The reference implementation is **egg** (Willsey, Nandi, Wang, Flatt,
Tatlock, Panchekha, "egg: Fast and Extensible Equality Saturation,"
*Proc. ACM Program. Lang.* 5 (POPL), 2021, arXiv:2004.03082,
doi:10.1145/3434304). Its contributions -- amortized invariant
maintenance (rebuild congruence lazily) and e-class analyses (carry a
lattice-valued analysis like constant-folding alongside each class) --
made equality saturation fast and extensible enough for real compiler
and synthesis use.

The tie to this doc set: e-graphs are the *optimization/synthesis*
face of the same equational reasoning waldmeister does as
*proof*. And the link is concrete -- **DreamCoder's library learning**
(its sleep-abstraction step) refactors recurring sub-programs using
version-space / e-graph-style anti-unification to grow its DSL (see
[02-integration.md](02-integration.md)). Equality saturation is the
clean mechanism for "find the shared structure across solved tasks and
name it," which is exactly what a multi-game ARC inducer wants.

## 4. Learning symbolic rules: where ARC induction actually lives

The ARC rule-induction problem ("infer the transformation from a few
examples") is not a new field; it is the intersection of two old ones.

**Inductive Logic Programming.** ILP learns a logic program
(hypothesis) entailing positive and excluding negative examples, given
background knowledge. Lineage:

- **Progol** (Muggleton, "Inverse Entailment and Progol," *New
  Generation Computing* 13:245-286, 1995, doi:10.1007/BF03037227)
  introduced mode-directed inverse entailment: build the most-specific
  ("bottom") clause for an example, then search the subsumption
  lattice above it.
- **Metagol / meta-interpretive learning** (Muggleton, Lin, Pahlavi,
  Tamaddoni-Nezhad, "Meta-interpretive learning of higher-order dyadic
  Datalog: predicate invention revisited," *Machine Learning*
  100:49-73, 2015) learns recursive programs and *invents predicates*
  by instantiating second-order metarules through a modified Prolog
  meta-interpreter.
- **Popper** (Cropper & Morel, "Learning programs by learning from
  failures," *Machine Learning* 110:801-856, 2021, arXiv:2005.02259)
  reframes ILP as generate-test-constrain: when a hypothesis fails,
  derive constraints (too general / too specific) that prune the
  hypothesis space, using answer-set programming for the generate step.
  This is the current strong, scalable ILP system.
- **dILP** (Evans & Grefenstette, "Learning Explanatory Rules from
  Noisy Data," *Journal of Artificial Intelligence Research* 61:1-64,
  2018, arXiv:1711.04574) is the differentiable relaxation: rule
  templates with learnable weights, trained by gradient descent,
  robust to noisy/ambiguous data. *Honest limit:* it scales poorly in
  the number of rules/predicates because it materializes the relaxed
  rule space, the recurring failure mode of differentiable logic noted
  in [02-integration.md](02-integration.md).

**Bayesian / probabilistic program induction.** Represent a concept
as a probabilistic program and infer it by Bayesian posterior. The
landmark is **BPL** (Lake, Salakhutdinov, Tenenbaum, "Human-level
concept learning through probabilistic program induction," *Science*
350:1332-1338, 2015, doi:10.1126/science.aab3050), which learns
handwritten characters (Omniglot) as small stroke programs and matches
humans at one-shot classification and generation -- the cleanest
demonstration that *programs as concepts* buys sample efficiency.
Underneath sit **probabilistic programming languages**: **Church**
(Goodman, Mansinghka, Roy, Bonawitz, Tenenbaum, "Church: a language
for generative models," UAI 2008, arXiv:1206.3255) and the modern,
performance-oriented **Gen** (Cusumano-Towner, Saad, Lew, Mansinghka,
"Gen: a general-purpose probabilistic programming system with
programmable inference," PLDI 2019, doi:10.1145/3314221.3314642),
which exposes programmable inference so you can mix learned proposals
with exact/Monte-Carlo inference.

This is the literature the repo's experiment arc belongs to. Experiment
200 ([brain/experiments/200_symbolic_induction](../../brain/experiments/200_symbolic_induction/README.md))
is a hand-coded-DSL ILP-flavoured diagnostic that recovered wa30's
action mapping (a0=up..a4=stay) from data alone, confirming the stuck
games are rule-governed. Experiment 201 (201_vq_transition) is the
*learned* counterpart -- a vector-quantized transition model whose
discrete code is, in spirit, an induced rule token (VQ-VAE / latent-
action lineage), the connectionist relaxation of the same induction
problem. The honest framing: 200/201 are doing rule induction by hand
and by gradient respectively, and Popper/dILP/BPL are the named fields
that already formalize the search.

## 5. Analogy and structure-mapping

Many ARC tasks are most naturally read as *analogies*: "this is to
that as X is to ?". The foundational cognitive theory is Gentner's
**structure-mapping**: a good analogy maps *relational structure*
(systems of relations), not surface attributes, preferring deep,
interconnected, systematic correspondences (the "systematicity
principle"). Its operational realization is the **Structure-Mapping
Engine** (Falkenhainer, Forbus, Gentner, "The Structure-Mapping
Engine: Algorithm and Examples," *Artificial Intelligence* 41:1-63,
1989, doi:10.1016/0004-3702(89)90077-5), which builds all consistent
relational matches between a base and a target without backtracking
and scores them, typically polynomial (about O(N^2)) in the
representation size.

A different, fluid take is **Copycat** (Hofstadter & Mitchell; Mitchell,
*Analogy-Making as Perception*, MIT Press 1993; Hofstadter & Mitchell,
"The Copycat Project," in Hofstadter, *Fluid Concepts and Creative
Analogies*, Basic Books 1995). Copycat solves letter-string analogies
("abc -> abd, so iijjkk -> ?") via a stochastic, parallel
"perception" process where concepts have graded, context-sensitive
activation and mappings can "slip" (successor->predecessor). It is a
direct intellectual ancestor of framing ARC as analogy-making rather
than classification.

The repo reinvented exactly this. Experiments 205/206
([205_relational_retrieval](../../brain/experiments/205_relational_retrieval/README.md),
[206_transfer_utility](../../brain/experiments/206_transfer_utility/README.md))
parse each frame into connected-component objects, build a relational
graph (edges labelled by relative direction), and compute a WL-1
fingerprint to retrieve the relationally-nearest state in *another*
game. That is structure-mapping over a relational graph -- find the
deepest structural correspondence, not the pixel match -- and the WL
fingerprint is a cheap structural signature in the same spirit as
SME's relational match. The grounding literature is this section. The
*honest* outcome, which the literature would have predicted, is in
206: cross-game matches reliably MATCH structurally but do not
TRANSFER dynamics -- when an object moves, the direction is fixed by
the action (a shared convention), so the only relation-transferable
bit is the state-dependent "blocked" case, and the arc closed as a
dead end. Structure-mapping found analogues; it did not hand over a
usable rule. Experiment 207 (207_intrinsic_goal) then pivoted to
per-game intrinsic-goal exploration, abandoning cross-game transfer.

## Relevance to thvm

This repo is unusually well positioned on the *engine* side and
unusually exposed on the *induction* side. waldmeister gives a real,
audited unfailing-completion theorem prover -- a sound symbolic
verifier and a rewrite-rule search substrate already in the tree -- and
egg-style equality saturation is the natural way to do the
library-learning / refactoring step a multi-game DSL inducer needs.
What is missing is the rule/program *learner* that proposes candidate
transition rules for the engine to check, and the named fields for it
are ILP (Popper), differentiable ILP (dILP), and Bayesian program
induction (BPL/Gen), with structure-mapping (SME/Copycat) as the
framing for the cross-game analogy that experiments 205/206 stumbled
into. The actionable read, consistent with
[04-implications-for-thvm.md](04-implications-for-thvm.md): keep the
symbolic side concrete -- a DSL search over per-game transition rules,
verified against observed transitions, with waldmeister/e-graph
machinery for normalization and library learning -- and treat 205/206's
result as the empirical warning that structural analogy alone does not
transfer dynamics; the rule itself still has to be induced per game.

## Sources

New to this page (candidates for [references.md](references.md)):

- J. A. Robinson, *A Machine-Oriented Logic Based on the Resolution
  Principle*, J. ACM 12:23-41 (1965), doi:10.1145/321250.321253.
  <https://dl.acm.org/doi/10.1145/321250.321253>
- L. Bachmair & H. Ganzinger, *Rewrite-Based Equational Theorem
  Proving with Selection and Simplification*, J. Logic and Computation
  4(3):217-247 (1994).
  <https://pure.mpg.de/rest/items/item_1834970/component/file_1857487/content>
- L. Kovacs & A. Voronkov, *First-Order Theorem Proving and Vampire*,
  CAV 2013, doi:10.1007/978-3-642-39799-8_1.
  <https://link.springer.com/chapter/10.1007/978-3-642-39799-8_1>
- L. de Moura & N. Bjorner, *Z3: An Efficient SMT Solver*, TACAS 2008,
  LNCS 4963, doi:10.1007/978-3-540-78800-3_24.
  <https://link.springer.com/chapter/10.1007/978-3-540-78800-3_24>
- D. E. Knuth & P. B. Bendix, *Simple Word Problems in Universal
  Algebras*, in *Computational Problems in Abstract Algebra* (1970).
- waldmeister, *Short Documentation* (in-tree:
  [../../waldmeister/documents/ShortDocumentation.txt](../../waldmeister/documents/ShortDocumentation.txt));
  Hillenbrand & Lochner, the Waldmeister system (CASC UEQ division).
- M. Willsey et al., *egg: Fast and Extensible Equality Saturation*,
  PACMPL 5 (POPL) (2021), arXiv:2004.03082, doi:10.1145/3434304.
  <https://arxiv.org/abs/2004.03082>
- S. Muggleton, *Inverse Entailment and Progol*, New Generation
  Computing 13:245-286 (1995), doi:10.1007/BF03037227.
  <https://link.springer.com/article/10.1007/BF03037227>
- S. Muggleton et al., *Meta-interpretive learning of higher-order
  dyadic Datalog: predicate invention revisited*, Machine Learning
  100:49-73 (2015).
  <https://www.doc.ic.ac.uk/~atn/papers/metagol_mlj.pdf>
- A. Cropper & R. Morel, *Learning programs by learning from
  failures*, Machine Learning 110:801-856 (2021), arXiv:2005.02259.
  <https://arxiv.org/abs/2005.02259>
- R. Evans & E. Grefenstette, *Learning Explanatory Rules from Noisy
  Data*, JAIR 61:1-64 (2018), arXiv:1711.04574.
  <https://arxiv.org/abs/1711.04574>
- B. M. Lake, R. Salakhutdinov, J. B. Tenenbaum, *Human-level concept
  learning through probabilistic program induction*, Science
  350:1332-1338 (2015), doi:10.1126/science.aab3050.
  <https://www.science.org/doi/10.1126/science.aab3050>
- N. D. Goodman, V. K. Mansinghka, D. M. Roy, K. Bonawitz, J. B.
  Tenenbaum, *Church: a language for generative models*, UAI 2008,
  arXiv:1206.3255. <https://arxiv.org/abs/1206.3255>
- M. F. Cusumano-Towner, F. A. Saad, A. K. Lew, V. K. Mansinghka,
  *Gen: a general-purpose probabilistic programming system with
  programmable inference*, PLDI 2019, doi:10.1145/3314221.3314642.
  <https://dl.acm.org/doi/10.1145/3314221.3314642>
- B. Falkenhainer, K. D. Forbus, D. Gentner, *The Structure-Mapping
  Engine: Algorithm and Examples*, Artificial Intelligence 41:1-63
  (1989), doi:10.1016/0004-3702(89)90077-5.
  <https://www.sciencedirect.com/science/article/abs/pii/0004370289900775>
- M. Mitchell, *Analogy-Making as Perception*, MIT Press (1993);
  D. Hofstadter & M. Mitchell, *The Copycat Project*, in D. Hofstadter,
  *Fluid Concepts and Creative Analogies*, Basic Books (1995).
