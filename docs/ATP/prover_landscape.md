# The Automated Theorem Prover Landscape

A reference survey of the algorithms, data structures, and heuristics
used by the major automated theorem provers (ATPs) - the systems that
compete at the CADE ATP System Competition (CASC) over the Thousands of
Problems for Theorem Provers (TPTP) library - and of the hammers that
bridge them to interactive proof assistants.

## Why this document exists

thvm carries its own equational ATP: an Interaction-Calculus-native port
of Waldmeister's unfailing Knuth-Bendix completion (see
[docs/plans/atp_ic_native.md](../plans/atp_ic_native.md) and the ATP
vocabulary in [docs/glossary.md](../glossary.md)). To know which ideas are
worth porting next, and to understand where a completion-based equational
engine sits in the broader field, we surveyed the source code, papers, and
competition records of the systems that define the state of the art. This
document is the result. It is written for an implementer: the emphasis is
on the actual mechanism and on why each technique helps, not on feature
checklists.

The field splits into four families, one per section below. They are not
mutually exclusive - modern systems borrow across the lines, and the
hammers orchestrate several backends at once - but the split reflects
genuinely different core algorithms.

## Map of the territory

1. **Saturation-based first-order provers** (Vampire, E, SPASS, the
   Otter/Prover9 lineage). The dominant paradigm at CASC. They run the
   given-clause loop over the superposition calculus, deriving consequences
   and discarding redundant clauses until they find the empty clause. This
   is the family thvm's engine is closest to in spirit, though thvm
   specializes to unit equations.

2. **Equational and completion-based provers** (Twee, Waldmeister,
   Zipperposition). Specialists in pure equational logic and unit-equality
   (UEQ) problems. They run unfailing Knuth-Bendix completion: orient
   equations into rewrite rules under a reduction order, compute critical
   pairs, and saturate. **This section is the most directly relevant to
   thvm**, which is a Waldmeister port; it includes the efficient
   Knuth-Bendix Ordering (KBO) and Lexicographic Path Ordering (LPO)
   comparison algorithms that any completion engine lives or dies by.

3. **SMT, instantiation-based, and higher-order provers** (cvc5, Z3,
   iProver, Leo-III, Satallax, Lash, the connection-tableau systems).
   Different cores entirely: a Conflict-Driven Clause Learning solver
   modulo theories, ground-abstraction-guided instantiation, or
   higher-order unification.

4. **Hammers, premise selection, and learning-guided proving**
   (Sledgehammer, CoqHammer, the Lean hammer stack, ENIGMA, Deepire, the
   reinforcement-learning connection provers). The integration and
   machine-learning layer: how proof assistants call out to the provers
   above, how the relevant facts are selected from a huge library, and how
   learned guidance is reshaping the inner search loop.

A short shared vocabulary recurs throughout. The **given-clause loop** is
the outer control loop common to saturation provers: a passive set of
unprocessed clauses and an active set of processed ones, with each
iteration selecting one passive clause, inferring against the active set,
and simplifying. A **reduction order** (KBO, LPO) is a well-founded order
on terms used to orient equations and keep rewriting terminating. A
**critical pair** is the equational-logic analogue of a resolvent: the two
ways of rewriting an overlap of two rules. **Redundancy** and **saturation**
are the completeness machinery: a clause is redundant if it follows from
smaller clauses, and a set is saturated when every non-redundant inference
is already present. **Term indexing** (discrimination trees, substitution
trees, fingerprint indexing, feature-vector indexing) is what makes the
millions of matching and subsumption queries per second tractable. Each
section defines these in its own context; the equational section treats the
orderings and completion in the most depth.

---

## Saturation-Based First-Order Provers

This section surveys the saturation-based automated theorem provers (ATPs)
for first-order logic with equality: Vampire, E (eprover), SPASS, and the
Otter / Prover9 / Mace4 lineage. The goal is to give an implementer a working
mental model of the actual algorithms, data structures, and heuristics these
systems use, and of *why* each technique helps. We begin with the shared
theory that all of them rest on - the superposition calculus, redundancy and
saturation, the given-clause loop, term orderings, and term indexing - and
then treat each system in turn.

The empirical context: at the 30th CADE ATP System Competition (CASC-30,
Stuttgart, July 2025) Vampire became the first system in the competition's
history to win all eight categories, solving more problems than all other
competitors combined. E remains the strongest open-source competitor and the
default backend for many interactive-prover hammers. So the design choices
described below are not academic curiosities; they are the difference between
proving and not proving in practice.

---

### 1. The Shared Theory

#### 1.1 Clausal logic and the saturation idea

All four systems work on *clauses*: a clause is a finite multiset of literals
read disjunctively, e.g. `~P(x) | Q(x,a) | f(x) = g(x)`. A first-order problem
(a set of axioms plus a conjecture) is negated and converted to Conjunctive
Normal Form (CNF), so the prover's job becomes: show the clause set is
*unsatisfiable* by deriving the empty clause (the contradiction, written as a
box or `$false`). This is refutational theorem proving.

Saturation is the strategy. Rather than search forward from axioms toward a
goal in a goal-directed way, a saturation prover repeatedly applies *inference
rules* to the clause set, adding conclusions back into the set, until either
the empty clause appears (refutation found) or the set is *saturated* - closed
under all non-redundant inferences (the input was satisfiable, so no proof
exists). The art is doing this without the clause set exploding.

#### 1.2 The superposition calculus

The modern engine is the *superposition calculus* (Bachmair and Ganzinger,
"Rewrite-Based Equational Theorem Proving with Selection and Simplification",
Journal of Logic and Computation, 1994). Superposition unifies ordered
resolution with ordered paramodulation and Knuth-Bendix completion into one
refutationally complete calculus for first-order logic with equality. Its
power comes from being *ordered* and using *redundancy*: a term ordering and a
literal selection function drastically restrict which inferences are
performed, while completeness is preserved.

The core generating rules (schematically):

```
Superposition (the equality-into-clause rule):
    s = t | C        L[s'] | D
  ----------------------------------  if sigma = mgu(s, s'),
        (L[t] | C | D) sigma           s sigma not < t sigma,
                                        and the rewritten side is maximal/selected

Equality Resolution:
    s != t | C
   --------------    if sigma = mgu(s, t)
       C sigma

Equality Factoring:
    s = t | s' = t' | C
   ----------------------  if sigma = mgu(s, s'), with ordering side conditions
    (s = t | t != t' | C) sigma
```

For non-equality literals, predicates `P(...)` are encoded as equations
`P(...) = true`, so ordered resolution and factoring fall out as special cases
of superposition. The crucial point for implementers: superposition only
rewrites with the *larger* side of an equation into the *larger* (or selected)
literals of the other clause. Most syntactically possible paramodulations are
forbidden by these ordering side conditions, which is what makes the calculus
tractable.

#### 1.3 Redundancy and the saturation invariant

A clause is *redundant* if it is entailed by smaller clauses already in the
set; an inference is redundant if its conclusion is. Redundant clauses and
inferences can be deleted or skipped without losing completeness. This is the
single most important practical idea in the whole field: the generating rules
*add* clauses, but the *simplification* (a.k.a. reduction) rules *remove or
shrink* them, and a good prover spends most of its time simplifying.

Key simplification rules:

- **Demodulation (rewriting):** use a unit equation `l = r` with `l > r` to
  rewrite occurrences of `l sigma` to `r sigma` in other clauses. This is
  oriented rewriting, exactly as in Knuth-Bendix completion.
- **Subsumption:** if clause `C` subsumes `D` (there is a substitution sigma
  with `C sigma` a sub-multiset of `D`), then `D` is redundant and deleted.
  Forward subsumption checks a new clause against existing ones; backward
  subsumption checks existing clauses against the new one.
- **Subsumption resolution** (a.k.a. contextual literal cutting):
  a combined subsumption-plus-resolution step that deletes a literal from a
  clause when a near-subsuming clause resolves it away. Cheap and very
  effective at shrinking clauses.
- **Tautology deletion**, including equational tautologies like `... | s = s`.

The *saturation invariant* a correct prover maintains is: every clause in the
"processed" set has had all its non-redundant inferences with other processed
clauses performed. When the unprocessed set empties out, the processed set is
saturated.

#### 1.4 The given-clause loop: Otter loop vs DISCOUNT loop

All saturation provers organize the search as a *given-clause* loop, an idea
from McCune's Otter. There are two clause sets:

- **Active / Processed (P):** clauses that have already been selected and have
  participated in inferences. These are kept fully indexed.
- **Passive / Unprocessed (U):** clauses waiting to be selected.

The loop is:

```
while U not empty:
    g := select_best(U)        # the "given clause"
    move g from U to P
    simplify g using P         # forward simplification
    if g is redundant or tautology: continue
    simplify P using g         # backward simplification (may delete from P)
    perform all inferences between g and clauses in P
    add new clauses (after cheap simplification) to U
    if empty clause derived: return UNSAT
return SAT (saturated)
```

The two classic variants differ in *which clauses are used for
simplification*:

- **Otter loop:** both Passive and Active clauses are used to simplify (and
  are simplified by) new clauses. So Passive clauses are kept fully indexed.
  This maximizes simplification power but costs memory and indexing time on
  clauses that may never be selected.
- **DISCOUNT loop:** only Active (processed) clauses simplify. Passive clauses
  are *inert* - stored cheaply, not used for simplification, not indexed for
  it. The key observation (used in E's manual) is that only processed clauses
  can ever participate in generating inferences, so keeping Passive cheap is a
  big win. The cost is that a Passive clause may sit un-simplified until it is
  selected.

E is the canonical DISCOUNT-loop prover; Vampire historically implements both
and its *Limited Resource Strategy* (Section 2.4) is an Otter-style refinement.
The tradeoff - simplification power versus per-clause overhead - is one of the
fundamental dials in the field.

#### 1.5 Term orderings: KBO and LPO/RPO

Superposition is parameterized by a *simplification ordering* on terms: a
well-founded ordering that is stable under substitution and compatible with
contexts, and total on ground terms. Two families dominate.

**Knuth-Bendix Ordering (KBO).** KBO assigns each function symbol an integer
*weight* and each symbol a *precedence*. To compare two terms it first
compares their total weights (sum of symbol weights, with variable-occurrence
side conditions to keep the ordering stable under substitution); ties are
broken by precedence on the top symbol and then a lexicographic comparison of
arguments. KBO is the workhorse: comparisons run in (near) linear time, and it
tends to orient equations the "right" way for rewriting. Schulz's note "Things
to Know when Implementing KBO" (Loechner / Schulz tradition) documents the
fiddly cases (variable counting, the special handling of a unary symbol of
weight zero) that an implementer must get right.

**Lexicographic Path Ordering (LPO) / Recursive Path Ordering (RPO).** LPO
compares terms by a recursive procedure driven only by a precedence on symbols:
`s > t` if some argument of `s` already dominates `t`, or if `s`'s top symbol
outranks `t`'s and `s` dominates all of `t`'s arguments, with a lexicographic
recursion on arguments for equal top symbols. LPO directly guarantees the
subterm property (a term exceeds its proper subterms). It can orient some
equations KBO cannot, but checks are quadratic and it is generally slower in
practice. Most successful superposition provers default to KBO, falling back to
LPO/RPO for problems where KBO orients badly.

The ordering shows up everywhere: it decides which side of an equation is the
rewrite source for demodulation, which literals are *maximal* (and so eligible
for superposition), and whether an inference's conclusion is smaller than its
premises (so the inference is redundant). Recent work pushes these orders into
higher-order logic - the "lambda-free higher-order" KBO/LPO variants
(Bentkamp, Blanchette, et al.) and very recently "Term Orders for Optimistic
Lambda-Superposition" (Bentkamp, 2025).

#### 1.6 Literal selection

Even with ordering restrictions, superposition into every maximal literal is
expensive. A *literal selection function* may select a subset of negative
literals in a clause; when literals are selected, generating inferences are
restricted to act only on the selected ones, overriding the maximality
condition. Completeness is preserved as long as the selection meets a
well-formedness condition (selecting at least one negative literal when
present, or only maximal literals). Selection of negative literals tends to
make the prover behave more like goal-directed (set-of-support) reasoning and
is one of the most impactful and most tuned heuristics; Vampire ships dozens of
numbered selection functions and E exposes a similar family.

#### 1.7 Term indexing

A saturation prover performs millions of retrieval queries: "find all clauses
with a literal unifiable with this one" (for generating inferences), "find a
generalization of this term" (for forward demodulation/subsumption), "find an
instance of this term" (for backward simplification). Doing these by linear
scan is hopeless. *Term indexing* is therefore central to performance. The main
techniques:

- **Discrimination trees.** A trie keyed by the pre-order (flattened) string of
  symbols in a term, with variables collapsed (perfect or non-perfect). Excellent
  for *matching* queries (find generalizations / instances), which is what
  forward demodulation and forward subsumption need. Retrieval walks the trie
  following the query term's symbol sequence. Used heavily for rewrite-rule
  lookup.
- **Substitution trees** (Graf, "Substitution Tree Indexing", MPI, 1994). Nodes
  store substitutions rather than symbols; the term at a node is obtained by
  composing the substitutions on the path from the root. They are more compact
  and support both matching and unification, and Graf reported they outperform
  path indexing, discrimination trees, and abstraction trees in his
  experiments. The downside is implementation complexity and costly
  maintenance under insertion/deletion.
- **Code trees** (Voronkov). A discrimination-tree-like structure that is
  *compiled*: each path becomes a small program of matching instructions
  (an abstract machine), so retrieval is executed rather than interpreted.
  Vampire uses code trees for forward subsumption and matching; the
  compile-to-instructions idea makes the hot loop very fast.
- **Fingerprint indexing** (Schulz, "Fingerprint Indexing for Paramodulation
  and Rewriting", IJCAR 2012). Each term gets a short, fixed-length *fingerprint*:
  a vector that samples the top symbol at a fixed set of term positions, recording
  for each sampled position whether it holds a particular function symbol, a
  variable, or is "below a variable" / nonexistent. Two terms can possibly
  unify/match only if their fingerprints are compatible position-by-position
  (a constant-time comparison per sample). Fingerprints are organized in a trie.
  Why it is fast: the index is tiny, the per-comparison test is a fixed-size
  vector check, and it is a *non-perfect* filter - it cheaply rejects the
  overwhelming majority of non-candidates, leaving a small set to verify with
  full unification/matching. The algorithms are simple, which matters for
  correctness and maintenance, and in Schulz's experiments performance was
  competitive with or better than the more complex perfect indexes.
- **Feature vector indexing** (Schulz, "Simple and Efficient Clause Subsumption
  with Feature Vector Indexing", 2013) for *clause-level* subsumption. Each
  clause is summarized by a vector of cheap, subsumption-monotone features
  (number of literals, count of positive/negative literals, max symbol depth,
  per-symbol occurrence counts, etc.) chosen so that if `C` subsumes `D` then
  every feature of `C` is <= the corresponding feature of `D`. The vectors are
  stored in a trie; to find candidate subsumers of `D` you only descend branches
  whose feature values are <= `D`'s. This prunes the quadratic subsumption check
  to a handful of real multiset-matching tests.

Picking the right index per operation (matching vs unification, term-level vs
clause-level) is a defining implementation skill in this field.

---

### 2. Vampire

Vampire (Kovacs and Voronkov, "First-Order Theorem Proving and Vampire",
CAV 2013) is the dominant saturation prover. It implements the superposition
calculus with an enormous menu of options and several signature innovations.
The recent system overview is "The Vampire Diary" (Hozzova, Kovacs, Reger,
Suda, Voronkov et al., CAV 2025), which catalogs how the architecture has
grown to handle theories, higher-order logic, and induction.

#### 2.1 The saturation core

Vampire's core is a configurable given-clause loop offering both Otter and
DISCOUNT loops, full superposition with KBO (default) or LPO, configurable
literal selection, and the full suite of simplifications (demodulation,
subsumption, subsumption resolution, tautology deletion). It maintains active
clauses in term indexes (code trees for subsumption, substitution/discrimination
and fingerprint indexes for generating inferences and demodulation).

#### 2.2 AVATAR: SAT/SMT-based clause splitting

AVATAR ("Advanced Vampire Architecture for Theories And Resolution"; Voronkov,
"AVATAR: The Architecture for First-Order Theorem Provers", CAV 2014) is
Vampire's signature contribution and the technique most worth understanding in
detail.

The problem it solves is *clause splitting*. Observation: if a clause
`C1 | C2` has variable-disjoint parts `C1` and `C2`, then
`forall (C1 | C2)` is equivalent to `(forall C1) | (forall C2)`. Logically you
can split on this disjunction: either `C1` holds or `C2` holds. Long, multi-part
clauses are poison for superposition because every inference produces an even
longer clause; splitting replaces one heavy clause with cases of light clauses.
The question is how to manage the case analysis.

The *old* approach (SPASS-style splitting with backtracking, Section 4) builds
the case split into the proof search itself: pick a component, assume it, search;
if you hit a dead end, backtrack and try the other component, undoing all work
done under the abandoned assumption. This is a tree search baked into the
saturation loop, and it is brittle: backtracking throws away derived clauses,
the branch order is a heuristic gamble, and combining many splits is awkward.

AVATAR's idea is to *delegate the splitting decisions to a SAT (or SMT) solver*
and keep the first-order prover purely as a clause-deriving engine. The
architecture:

1. When a clause splits into components `C1, ..., Cn`, each component is mapped
   (consistently, via a normalized representation) to a propositional variable
   `[Ci]`. The first-order clause contributes the SAT clause `[C1] | ... | [Cn]`
   to a SAT solver's clause set.
2. The SAT solver computes a *model* of all such propositional clauses. The
   model selects, for each split, which components are currently "asserted".
   This model is the current *branch* of the case analysis - but chosen
   globally and optimally by the SAT solver, not by a local heuristic.
3. The first-order prover runs superposition only on the components asserted by
   the current model. Each derived clause is *labeled* with the set of
   propositional assumptions (the split components) it depends on.
4. If the prover derives the empty clause under some set of assumptions, that
   set becomes a new propositional (conflict) clause handed back to the SAT
   solver: "these assumptions together are inconsistent."
5. The SAT solver incorporates the new clause and produces a *new* model - a
   different branch. Clauses whose labels are not satisfied by the new model are
   deactivated (frozen), not deleted; clauses consistent with both old and new
   model stay active.

Why this beats naive splitting:

- **No wasted work on backtracking.** Frozen clauses are kept and reactivated if
  the SAT solver swings back to a compatible model. Splitting-with-backtracking
  would have deleted them.
- **The SAT solver does the case-analysis bookkeeping optimally.** Conflict-driven
  clause learning (CDCL) in the SAT solver means the propositional search over
  which combination of components to assume is handled by a mature, fast engine
  with learning and non-chronological backtracking, instead of an ad hoc branch
  manager inside the prover.
- **Light clauses dominate the first-order search.** The prover always works on
  short, split components, which is where superposition is efficient.
- **Splits compose cleanly.** Hundreds of independent splits are just more
  propositional variables; the SAT solver handles their combinatorics.

The empirical payoff was large: Voronkov reported AVATAR-enabled Vampire solving
hundreds of TPTP problems never before solved by any system. Further tuning is
explored in Reger, Suda, Voronkov, "Playing with AVATAR" (CADE 2015), which
studies component selection, when to split, and SAT-solver interaction policies.

#### 2.3 The age/weight ratio and clause selection queues

Selecting the given clause is the heuristic heart of the loop. Vampire keeps
*multiple priority queues* over the Passive set and interleaves them by a fixed
ratio:

- An **age queue** (first-in-first-out) that returns the *oldest* unprocessed
  clause. Selecting by age guarantees *fairness*: every clause is eventually
  picked, which is required for completeness, and it favors clauses derived
  early (closer to the input).
- A **weight queue** that returns the *lightest* clause (fewest symbols, roughly).
  Small clauses are more likely to be generalizations, are cheaper to work with,
  and are closer to the empty clause; selecting by weight is a greedy push toward
  contradiction.

The **age/weight ratio** (e.g. `1:5`) says: pick one clause by age, then five by
weight, repeat. Why the ratio matters: pure weight selection is greedy and can
get stuck forever generating ever-lighter clauses in a fruitless region while
never returning to an old, heavy-but-essential clause - it loses completeness in
practice and can starve. Pure age selection is fair but ignores the strong signal
that small clauses are good, so it is slow. The ratio blends a completeness-and-
exploration term (age) with an exploitation term (weight). The right ratio is
problem-dependent and is one of the most-tuned parameters in any portfolio.
Schulz and colleagues' "Old or Heavy? Decaying Gracefully with Age/Weight Shapes"
(CADE 2019) generalizes the two-queue ratio to smooth *priority shapes* that
interpolate age and weight, which both Vampire and E have drawn on.

#### 2.4 Limited Resource Strategy (LRS)

LRS (Riazanov and Voronkov, "Limited Resource Strategy in Resolution Theorem
Proving", Journal of Symbolic Computation 36, 2003) addresses a specific reality:
provers run under a *time limit*. Under the Otter loop, Passive clauses are fully
indexed and used for simplification, which is expensive; if the time limit hits
before those Passive clauses are ever selected, all that indexing was wasted.

LRS estimates, from statistics gathered during the run (the rate at which clauses
are processed and the distribution of their weights), how many of the current
Passive clauses could *possibly* be reached before the deadline. Clauses that
provably cannot be reached in time - those too heavy to ever be selected given
the observed processing rate - are discarded (or not kept indexed). The weight
limit is *adaptive*: it tightens or loosens as the run proceeds. The effect is to
spend the indexing/simplification budget only on clauses that have a chance of
mattering before the clock runs out. Riazanov and Voronkov showed LRS beats both
the plain Otter algorithm and the DISCOUNT algorithm under fixed time budgets.

#### 2.5 SInE: axiom selection for large theories

Large-theory problems (think Open CYC with millions of axioms, or large Mizar /
SUMO ontologies) have far more axioms than any proof needs - usually a few dozen.
Feeding everything to the prover drowns the search. *SInE* (Sumo INference Engine;
Hoder and Voronkov, "Sine Qua Non for Large Theory Reasoning", CADE-23, 2011)
selects a relevant subset before proving.

The idea is a symbol-based relevance relation. For each symbol, SInE computes how
often it occurs across the axiom set; a symbol's *trigger* axioms are those in
which it is the (least common, hence most defining) symbol. Starting from the
symbols in the conjecture, SInE pulls in axioms triggered by those symbols, then
the symbols introduced by those axioms, and so on, expanding outward by a
*tolerance* and *depth* parameter. Rare symbols are strong relevance signals;
ubiquitous symbols (like equality) are not allowed to drag in everything. This
is fast (a couple of passes over the axioms), and SInE won the CASC large-theory
division in 2008. The method was so effective it was adopted by competing systems
and by interactive-prover hammers as a relevance filter.

#### 2.6 Theory reasoning: Z3, theory instantiation, AVATAR modulo theories

Pure superposition is weak on arithmetic and other built-in theories. Vampire
integrates theory reasoning several ways:

- **AVATAR modulo theories** (Reger, Suda, Voronkov, "AVATAR Modulo Theories",
  GCAI 2016). Because AVATAR already uses a SAT solver to manage splitting, you
  can swap in an SMT solver. The ground theory literals among the split
  components are handed to the SMT solver (Z3), which decides their joint
  theory-consistency (linear arithmetic, uninterpreted functions, extensional
  arrays). The first-order prover then only explores branches that are
  theory-consistent on their ground part.
- **Theory instantiation** (Reger, Suda, Voronkov, "Unification with Abstraction
  and Theory Instantiation in Saturation-Based Reasoning", TACAS 2018). An SMT
  solver is used to find substitutions that satisfy the *pure-theory* literals of
  a clause, producing instances in which those theory literals can be removed -
  effectively letting the SMT solver discharge the arithmetic part while
  superposition handles the rest. Companion technique *unification with
  abstraction* relaxes syntactic unification so theory terms that are not
  syntactically equal but are theory-equal can still trigger inferences (the
  mismatch is abstracted into a side condition literal).
- **ALASCA** (Abstracting Linear Arithmetic Superposition Calculus), described in
  the Vampire Diary, integrates linear-arithmetic reasoning - inequality
  chaining, rewriting modulo linear arithmetic - directly into the calculus
  rather than fully delegating to Z3.

#### 2.7 Portfolio scheduling and "Sledgehammer mode"

No single strategy (one literal selection + one age/weight ratio + one ordering
+ AVATAR on/off + ...) is best across problems. Vampire's competition and CASC
mode runs a *portfolio*: a precomputed *schedule* of many strategies, each given
a time slice, tried in sequence (or in parallel across cores) until one succeeds.
The schedule is learned offline by clustering training problems and recording
which strategies solve which clusters fastest. Vampire also has a dedicated mode
for Isabelle's Sledgehammer that returns Sledgehammer-friendly output (the
unsatisfiable core / used axioms, so Isabelle can reconstruct an internal proof);
the same "give me the axioms you actually used" capability underlies its use as a
hammer backend generally.

#### 2.8 Higher-order Vampire

Vampire has been extended to higher-order logic via *combinatory superposition*
and lambda-free higher-order term orderings (the Bentkamp/Blanchette/Vukmirovic
line), avoiding the explosive higher-order unification by translating lambda
terms to combinators and adding superposition rules for them. The Vampire Diary
(CAV 2025) documents the higher-order mode that competed in (and won) the CASC
higher-order division.

---

### 3. E (eprover)

E (Schulz, "E - A Brainiac Theorem Prover", AI Communications 2002; and the more
recent "Faster, Higher, Stronger: E 2.3", CADE 2019) is the leading open-source
saturation prover and a research workhorse, used as a backend in Sledgehammer and
as the substrate for the ENIGMA machine-learning experiments.

#### 3.1 The DISCOUNT loop

E is built around the DISCOUNT loop (Section 1.4): only the (small) set of
processed clauses is used for and subjected to simplification; Passive clauses are
stored cheaply and inertly. This keeps memory and per-clause overhead low and is
why E scales gracefully. The proof procedure is the standard given-clause loop:
pick a clause, simplify it against the processed set, if it survives use it to
back-simplify the processed set and then perform all generating inferences with
the processed set, sending the (cheaply simplified) results to Passive.

#### 3.2 Clause Evaluation Functions and weighted priority queues

E's distinguishing design is its *clause evaluation function (CEF)* machinery for
selecting the given clause. A CEF is a pair of a *priority function* and a
*weight function* (with parameters). For each clause it produces a `(priority,
weight)` key, and the queue returns the clause with the smallest key
(priority first, then weight). Priority functions implement coarse preferences
(prefer goals, prefer unit clauses, prefer clauses with no "fresh" symbols);
weight functions implement fine-grained scoring (symbol-counting weight with
per-symbol multipliers, terms weighted by whether they appear in the goal,
clause-feature combinations).

Crucially, E selects the given clause from *several* CEFs simultaneously,
combined by a *weighted round-robin*, written as a heuristic specification like:

```
-H'(1*Clauseweight(...), 3*FIFOWeight(...), 2*Refinedweight(...))'
```

This says: maintain three priority queues, and over a cycle of 6 selections take
1 from the first, 3 from the second, 2 from the third. This generalizes Vampire's
two-queue age/weight ratio to an arbitrary mix of arbitrary evaluation functions,
giving E an extremely flexible, declaratively specified selection policy. The FIFO
(first-in-first-out) component supplies the fairness/age term needed for
completeness, exactly as the age queue does in Vampire.

#### 3.3 Literal selection and ordering

E offers a large family of literal selection strategies (selecting negative
literals by various size and polarity criteria) and uses KBO or LPO with
auto-generated symbol weights and precedences derived from the problem's signature
(e.g. by symbol frequency or arity). The rewriting/simplification machinery -
demodulation, rewriting with the processed unit equations, clause-orienting via
the term order - is the Knuth-Bendix completion heritage applied inside the loop.

#### 3.4 auto and auto-schedule

A user rarely wants to hand-write a heuristic specification, so E ships two
automatic modes:

- **auto mode:** E inspects features of the input problem (number of clauses,
  presence of equality, whether it is Horn, unit, the size of the signature, the
  axiom-to-conjecture structure, etc.) and *classifies* it into one of a set of
  problem classes. Each class has a single hand-tuned or learned "expert"
  heuristic (a full CEF specification plus ordering and selection choices) known
  to do well on that class. E configures itself accordingly and runs once.
- **auto-schedule mode:** instead of one expert, E runs a *schedule* of several
  experts, each for a fraction of the time budget, in the spirit of Vampire's
  portfolio. The schedule per class is precomputed offline by evaluating many
  strategies over a large training set (TPTP) and using a set-cover-style
  selection to find a small ordered set of strategies that jointly solve as many
  training problems as quickly as possible.

This offline-learned, feature-classified scheduling is the reason a single binary
is competitive across the wildly heterogeneous TPTP library.

#### 3.5 Watchlists

E supports *watchlists*: a user-supplied set of clauses (often lemmas, or clauses
from a known proof) that the prover watches for. When a derived clause matches a
watchlist clause, E can boost its priority (select it sooner) or record the hit.
This is how E supports proof-guided search, lemma reuse, and proof-completion
tasks - and it is the mechanism by which learned guidance (ENIGMA) and proof
mining steer the search. Watchlists are E's analogue of Otter/Prover9 *hints*
(Section 5).

#### 3.6 E as a backend

Because E is fast, open, and produces explicit proof objects with the used
axioms, it is a default backend for Isabelle's Sledgehammer and a common engine
behind other hammers. It is also the platform for the ENIGMA project (Jakubuv,
Urban, et al.), which replaces or augments the CEF-based given-clause selection
with a learned classifier (gradient-boosted trees, then neural models) trained on
features of clauses from successful proofs - effectively a learned clause
evaluation function plugged into E's existing selection slot.

---

### 4. SPASS

SPASS (Weidenbach et al., "SPASS Version 3.5", CADE 2009; original "SPASS:
Combining Superposition, Sorts and Splitting") is a superposition prover whose
name announces its three pillars.

#### 4.1 Sorted / typed superposition

SPASS treats *sorts* (monadic predicates that behave like types) specially. Many
problems carry a sort structure - `human(x)`, `list(y)` - and naive treatment
produces masses of sort literals. SPASS uses *sorted unification* and special
inference rules so that sort constraints are solved by dedicated machinery
("static soft typing" / sort theory) rather than by ordinary superposition on the
monadic literals. This prunes a great deal of redundant reasoning about
well-sortedness and is a precursor to the typed first-order reasoning that later
provers adopted.

#### 4.2 Splitting with backtracking (contrast with AVATAR)

SPASS pioneered explicit *clause splitting* inside a saturation prover, and its
mechanism is the historically important *splitting with backtracking* - the
direct foil to AVATAR. When SPASS splits `C1 | C2` (variable-disjoint), it picks
a branch: assume `C1`, push a backtracking point, and continue saturation with
`C1` added. If that branch derives the empty clause *using* the split assumption,
SPASS backtracks to the split point, discards the work done under `C1`, and tries
`C2`. The case analysis is a depth-first tree explored inside the loop.

Later SPASS versions improved this with *labelled splitting* (Fietzke and
Weidenbach, "Labelled Splitting", Annals of Mathematics and AI, 2009), which
attaches labels to clauses recording which split decisions they depend on. Labels
enable *non-chronological backtracking* (jump back past irrelevant split points)
and *branch condensing*, recovering some of the work that naive chronological
backtracking would throw away.

The instructive contrast: even labelled SPASS splitting keeps the case-analysis
*tree* inside the prover and still discards clauses on backtracking. AVATAR
externalizes the entire case analysis to a CDCL SAT solver, *freezes* rather than
deletes clauses, and lets propositional conflict learning manage the combinatorics
optimally. That is precisely why AVATAR-style splitting overtook
splitting-with-backtracking in the competition provers.

#### 4.3 FLOTTER: the clausifier

Turning a first-order formula into CNF is not trivial: naive distribution can blow
up the clause count exponentially, and the *choice* of Skolem functions and
*definitional* (Tseitin-style) introductions for shared subformulas dramatically
affects later proof search. FLOTTER is SPASS's CNF transformation module, notable
for doing this *well*: it applies formula renaming / definitional CNF to avoid
size explosion (introducing names for subformulas that would otherwise be
duplicated), optimized Skolemization, and various simplifications during
clausification. FLOTTER was good enough that it was used as a standalone
clausifier feeding other provers. The lesson for implementers: the clausifier is
part of the proof search, not a neutral preprocessing step - a better CNF
transform can decide whether a problem is provable in time.

---

### 5. Otter / Prover9 / Mace4 (the legacy lineage)

The McCune lineage defines much of the vocabulary used above. Otter ("Organized
Techniques for Theorem-proving and Effective Research"; McCune, Argonne) was for
years *the* reference prover and the system against which others were measured.
Prover9 is its successor (with Mace4 as the companion finite-model finder /
counterexample tool: if Prover9 cannot find a proof, Mace4 may find a finite model
showing the conjecture is not entailed).

#### 5.1 The original given-clause algorithm

The given-clause loop itself - the `set of support` / `usable` two-list structure,
selecting a "given clause", moving it from passive to active, performing all
inferences with the active list - originates here. The "Otter loop" of Section 1.4
is named after this system: Passive (called *sos*, the set of support) clauses are
kept available for simplification. Essentially every modern saturation prover is a
refinement of Otter's main loop.

#### 5.2 Set-of-support strategy

The *set-of-support (SOS) strategy* (Wos, Robinson, Carson, 1965, predating Otter
but central to it) is a completeness-preserving restriction: partition the input
into a satisfiable "background" (the axioms) and the set of support (typically the
negated conjecture plus chosen clauses), and require every inference to involve at
least one clause descended from the set of support. The intuition: the axioms
alone are consistent, so a refutation *must* use the negated goal; SOS forbids the
prover from wasting effort deriving consequences purely among the axioms. This is
the original goal-direction heuristic, and modern negative-literal selection
recovers much of the same effect within superposition.

#### 5.3 Weighting

Otter introduced user-controllable *weighting* of clauses and terms via
*weight templates*: patterns that assign costs to terms so the user can bias the
search toward or away from particular structures, and a *max_weight* cutoff that
discards clauses heavier than a bound (the manual ancestor of Vampire's adaptive
LRS weight limit). Clause selection by a pick-given ratio that interleaves
selection by weight with selection by age (FIFO) is, again, an Otter idea that
became the Vampire age/weight ratio and the E FIFO-plus-weight CEF mix.

#### 5.4 Demodulators and hints

Two more Otter ideas that propagated everywhere:

- **Demodulators.** Otter let the user (and the prover) designate oriented
  equations as *demodulators* - rewrite rules applied to simplify new clauses to
  normal form. This is exactly the demodulation simplification of Section 1.3, and
  the *dynamic demodulation* that turns newly derived oriented equations into
  rewrite rules is the Knuth-Bendix completion idea operating inside the loop.
- **Hints.** Otter/Prover9 *hints* are clauses the user expects to appear in or
  near a proof; the prover gives matching derived clauses preferential selection.
  Hints let an expert (or a previous proof attempt) steer search without changing
  the logic. E's *watchlists* (Section 3.5) are the direct descendant, and the
  whole modern enterprise of learned proof guidance is "hints, but learned".

#### 5.5 Historical importance

The contribution of this lineage is foundational rather than performance-leading
today: the given-clause loop, set of support, weighting and the pick-given ratio,
demodulation as in-loop completion, and hints all originate or were popularized
here. McCune's later EQP prover famously settled the Robbins conjecture (an open
mathematical problem) by automated equational reasoning, demonstrating that
saturation provers could do real mathematics, not just benchmark puzzles.

---

### 6. Synthesis for implementers

A few cross-cutting takeaways for anyone building a saturation prover:

- **Simplification dominates generation.** The generating rules of superposition
  are few; the engineering effort and the runtime go into redundancy elimination
  (demodulation, subsumption, subsumption resolution) and the indexes that make it
  fast. A prover that generates aggressively but simplifies weakly drowns.
- **Indexing is not optional, and the right index depends on the query.** Use
  matching indexes (discrimination/code trees) for forward demodulation and
  subsumption, unification-capable indexes (substitution trees, or
  fingerprint-filtered unification) for generating inferences, and feature-vector
  indexing for clause subsumption. Fingerprint indexing is the best
  effort-to-payoff ratio: cheap to implement, small, and a strong non-perfect
  filter.
- **Clause selection is where problems are won or lost.** The age/weight ratio (or
  E's CEF mix) balances fairness/completeness against the greedy pull toward small
  clauses. Get the fairness term right or the prover starves; get the weight term
  right or it wanders.
- **Splitting pays, and SAT-managed splitting (AVATAR) pays most.** Externalize the
  case analysis to a CDCL solver, freeze rather than delete, and let the SAT/SMT
  layer also carry theory reasoning.
- **No single strategy wins; portfolios do.** Offline-learned schedules (Vampire
  portfolio, E auto-schedule) over a feature-classified problem space are how one
  binary covers a heterogeneous benchmark library, and they are the reason these
  systems are practical rather than merely complete.

---

### References (selected, inline-cited above)

- Bachmair, Ganzinger. "Rewrite-Based Equational Theorem Proving with Selection
  and Simplification." Journal of Logic and Computation, 1994. (Superposition
  calculus, redundancy.)
- Kovacs, Voronkov. "First-Order Theorem Proving and Vampire." CAV 2013.
- Voronkov. "AVATAR: The Architecture for First-Order Theorem Provers." CAV 2014.
- Reger, Suda, Voronkov. "Playing with AVATAR." CADE 2015.
- Reger, Suda, Voronkov. "AVATAR Modulo Theories." GCAI 2016.
- Reger, Suda, Voronkov. "Unification with Abstraction and Theory Instantiation
  in Saturation-Based Reasoning." TACAS 2018.
- Riazanov, Voronkov. "Limited Resource Strategy in Resolution Theorem Proving."
  Journal of Symbolic Computation 36(1-2), 2003.
- Hoder, Voronkov. "Sine Qua Non for Large Theory Reasoning." CADE-23, 2011.
- Schulz, Mohrmann (and others). "Old or Heavy? Decaying Gracefully with
  Age/Weight Shapes." CADE 2019.
- Hozzova, Kovacs, Reger, Suda, Voronkov et al. "The Vampire Diary." CAV 2025.
- Schulz. "E - A Brainiac Theorem Prover." AI Communications, 2002.
- Schulz, Cruanes, Vukmirovic. "Faster, Higher, Stronger: E 2.3." CADE 2019.
- Schulz. "Fingerprint Indexing for Paramodulation and Rewriting." IJCAR 2012.
- Schulz. "Simple and Efficient Clause Subsumption with Feature Vector Indexing."
  2013.
- Graf. "Substitution Tree Indexing." MPI-I-94-251, 1994.
- Weidenbach et al. "SPASS Version 3.5." CADE 2009.
- Fietzke, Weidenbach. "Labelled Splitting." Annals of Mathematics and AI, 2009.
- Bentkamp, Blanchette, et al. "Superposition for Lambda-Free Higher-Order
  Logic." (LMCS); Bentkamp. "Term Orders for Optimistic Lambda-Superposition."
  arXiv:2510.18452, 2025.
- McCune et al. Otter / Prover9 / Mace4 documentation, Argonne National
  Laboratory.
- CASC-30 results, Stuttgart, July 2025 (Vampire wins all eight categories).

---


## Equational and Completion-Based Provers

This section surveys automated theorem provers (ATPs) that specialize in
*equational* logic - reasoning where the only predicate is equality (`=`) and
the only kind of axiom is an equation `l = r`. It is written for implementers,
and it is the most directly relevant part of this report to the thvm project,
because thvm's own ATP is a port of WALDMEISTER's unfailing Knuth-Bendix
completion. We therefore go deep on the two state-of-the-art unit-equality
provers, Twee and WALDMEISTER, on the term-ordering algorithms they depend on
(Knuth-Bendix Ordering and Lexicographic Path Ordering), and on the broader
superposition calculus that subsumes completion. We close with shorter
treatments of Zipperposition, E, and Prover9, and with the precise sense in
which completion is "superposition restricted to unit equations".

Throughout, we use `=` for the equational/equivalence relation under proof,
`->` for an oriented rewrite rule, `==` for syntactic identity of terms, and
`>` for the (strict) term ordering. We write t[s] for a term t with a
distinguished subterm s, and t[u] for the result of replacing that subterm by u.


### 1. The Problem: Equational Logic and Word Problems

An equational theory E is a finite set of equations between first-order terms.
The *word problem* asks: given E and a goal equation s = t, does s = t hold in
every model of E (equivalently, is s = t derivable from E by the equational
inference rules: reflexivity, symmetry, transitivity, congruence, and
substitution)? This is the validity problem `E |= s = t`.

Birkhoff's completeness theorem says `E |= s = t` if and only if s and t are
connected by a finite chain of equational *replacement* steps using instances
of equations in E, applied at arbitrary positions in either direction. The
trouble is the phrase "in either direction": a naive search applies every
equation both ways at every position, which branches explosively and rarely
terminates. The entire field of equational theorem proving is, in essence, the
search for ways to *orient* equations so that they may be applied in one
direction only, as *rewrite rules*, while preserving completeness.


### 2. Term Rewriting and Knuth-Bendix Completion

#### 2.1 Rewriting and Confluence

A *rewrite rule* l -> r is an oriented equation. A rewrite system R rewrites a
term t to t' (written t ->_R t') when some subterm of t matches l under a
substitution sigma (i.e. equals l*sigma) and is replaced by r*sigma. A term in
which no rule applies is a *normal form*.

R is *terminating* if there is no infinite rewrite sequence. R is *confluent*
if whenever t rewrites (in zero or more steps) to both a and b, then a and b
have a common reduct. A *convergent* (a.k.a. *complete*) system is one that is
both terminating and confluent; in a convergent system every term has a *unique*
normal form, and then `E |= s = t` reduces to the trivially decidable check
"normalise s and t with R and compare the results syntactically". Turning E
into such an R is the goal of completion.

#### 2.2 Critical Pairs and the Knuth-Bendix Algorithm

Termination of R is established by showing every rule l -> r satisfies l > r in
a *reduction order* > (a well-founded order on terms that is stable under
substitution and compatible with contexts; Section 5). Confluence is the hard
part. Knuth and Bendix's key insight (1970) is that for a terminating system,
confluence follows from *local* confluence, and local confluence can be checked
by a *finite* number of *critical pairs*.

A critical pair arises from an *overlap* between two rules. Take rules l1 -> r1
and l2 -> r2 (rename variables apart). If a non-variable subterm of l1 at
position p unifies with l2 under most general unifier (m.g.u.) sigma, then the
term l1*sigma can be rewritten two ways: at the root by l1 -> r1 giving
r1*sigma, and at p by l2 -> r2 giving l1*sigma[r2*sigma]_p. The pair

    (r1*sigma, l1*sigma[r2*sigma]_p)

is a *critical pair*. The Critical Pair Lemma states: a terminating R is
confluent if and only if every critical pair is *joinable* (both components
rewrite to a common normal form). This makes confluence decidable for finite
terminating systems.

The basic Knuth-Bendix completion loop:

    procedure KB(E, >):                       # E equations, > reduction order
      R := {}
      while E is non-empty:
        pick and remove an equation s = t from E
        s' := normal_form(s, R); t' := normal_form(t, R)
        if s' == t': continue                 # trivially joinable, discard
        if s' > t':   l, r := s', t'
        elif t' > s': l, r := t', s'
        else: FAIL "cannot orient s' = t'"    # <-- the failure point
        # interreduce: simplify existing rules with the new rule, and vice versa
        for each rule (g -> d) in R whose lhs g is reducible by l -> r:
          move it back to E (it may now reorient)
        R := simplify_rhs(R, l -> r)
        R := R + { l -> r }
        for each critical pair (a, b) between (l -> r) and rules of R:
          E := E + { a = b }
      return R

Two completions of the same E can produce different convergent systems, but if
both terminate they decide the same theory.


### 3. Why Plain Completion Fails, and Unfailing Completion

The line marked `FAIL` is the Achilles heel of classical Knuth-Bendix
completion. Some equations cannot be oriented in *either* direction because
neither side is greater than the other in any reduction order. The canonical
example is commutativity:

    x + y = y + x

Both sides have the same multiset of symbols and variables; for any reduction
order > we have x + y > y + x for some instances and y + x > x + y for others,
so neither orientation is sound for termination. Plain completion aborts. Since
commutativity, associativity-commutativity (AC), and other *permutative*
equations are ubiquitous (rings, lattices, groups, Boolean algebra), plain
completion is useless on most real problems.

#### 3.1 Ordered (Unfailing) Completion

Unfailing completion, due to Bachmair, Dershowitz and Plaisted ("Completion
without failure", 1989), removes the failure point and is *refutationally
complete* for equational logic: given an unsatisfiable goal it is guaranteed to
find a proof. The two central ideas:

1. **Keep unorientable equations as equations.** Instead of aborting on
   x + y = y + x, retain it as a two-way equation in a special pool E (alongside
   the oriented rules R).

2. **Rewrite with unorientable equations *as if* oriented, but only when the
   instance decreases.** An equation u = v (renamed apart) may rewrite a term
   t at position p with matcher sigma *provided that u*sigma > v*sigma in the
   reduction order*. This is *ordered rewriting*: the equation is allowed to
   fire in whichever direction makes the matched instance strictly smaller. The
   direction is decided per-instance, at rewrite time, by an ordering test - not
   once and for all. Because every ordered-rewrite step strictly decreases the
   term in the well-founded order >, ordered rewriting terminates even though
   the underlying equation is unorientable.

The reduction order > must be a *ground-total* reduction order (total on ground
terms), so that for every ground instance one side is definitely larger. The
Knuth-Bendix Ordering and Lexicographic Path Ordering with a total precedence
both qualify.

Refutational completeness comes from the following picture. Unfailing
completion saturates E into a system that is *ground convergent*: confluent and
terminating on *ground* terms (terms with no variables). To prove a goal s = t,
one adds the *negated, Skolemised* goal. For a unit-equality conjecture s = t
(existentially closed if it had variables, then Skolemised to ground terms
s0, t0), the prover normalises s0 and t0 with the ordered rewrite system as it
grows; the theory is proved exactly when s0 and t0 reach a common normal form,
i.e. when their difference becomes trivial. This is the standard
"prove by completing until the goal joins" strategy. Ground convergence (rather
than full convergence) suffices precisely because the goal is ground.

#### 3.2 The Unfailing Completion Loop (pseudocode)

The modern realization is the DISCOUNT loop (Denzinger, Kronenburg, Schulz,
1997): a "given clause" loop split into a *passive set* of unprocessed facts
and an *active set* of processed, fully-interreduced facts.

    procedure UNFAILING_COMPLETE(Axioms, Goal, >):
      Active  := {}                  # oriented rules + retained unorientable eqns
      Passive := orient_each(Axioms) # all axioms as candidate (critical-pair) facts
      g       := ground_skolemise(Goal)   # s0 = t0, ground
      while Passive is non-empty:
        e := select_best(Passive)           # heuristic choice (Section 4.3 / 7)
        Passive := Passive \ {e}
        e := interreduce_with(Active, e)     # ordered-normalise both sides of e
        (s, t) := sides(e)
        if s == t: continue                  # subsumed / trivially joined
        if redundant(e, Active): continue    # ground-joinable / connected (Section 6)
        orient e into one of:
            l -> r            (if s > t or t > s: an ordinary rule)
            u = v             (otherwise: a retained unorientable equation)
        # back-simplification: use e to reduce older active facts; demoted ones
        # whose lhs is now reducible go back to Passive
        Active := back_simplify(Active, e) + { e }
        for each f in Active:                # forward generation
          for each critical pair cp of (e, f) [both overlap directions]:
            cp := ordered_normalise(cp, Active)
            if sides(cp) differ and not redundant(cp, Active):
              Passive := Passive + { cp }
        g := ordered_normalise(g, Active)    # keep the goal reduced
        if sides(g) are equal: return "Theorem (goal joined)"
      return "Saturated: goal not provable from axioms"

Two subtleties an implementer must get right. First, critical pairs between
*unorientable* equations and between an unorientable equation and a rule must be
computed with the *ordered* superposition variant: only superpose into the side
that can be the larger one for some ground instance (an ordering *constraint*
check, not full orientation). Second, *back-simplification* (using a new fact to
reduce old ones) is what keeps the active set small and is essential for both
correctness of redundancy and performance; demoted facts must re-enter Passive
because reorientation can change them.


### 4. Twee

Twee, by Nick Smallbone (Chalmers), is the modern reference implementation of
unfailing completion for unit-equality problems. The 2021 system-description
paper ("Twee: An Equational Theorem Prover (System Description)", CADE-28)
reports that Twee came second in the Unit Equality (UEQ) division of CASC-J10
and solved problems no other system solved. It is about 5300 lines of Haskell
(versus ~65000 lines of C for WALDMEISTER) yet competitive in speed thanks to
careful low-level term operations. Three properties make Twee distinctive.

#### 4.1 Fixed Heuristics

Unlike provers that analyse the input and switch strategy, Twee uses one fixed
configuration for every problem: a fixed term order, a fixed critical-pair
scoring function, and so on. The bet is that good general-purpose strategies beat
fragile auto-configuration. (The paper notes the one place this hurts: ring
problems (RNG), where the choice of term order matters a lot.)

#### 4.2 Architecture and State

Twee natively handles only *unit equality with a ground goal*. Everything else
is compiled away by the companion tool Jukebox: full first-order formulas are
clausified, Horn clauses are encoded as equations (Section 4.7), and sorts are
encoded with extra functions. A non-ground goal `s = t` is made ground with an
old WALDMEISTER trick: introduce fresh symbols `eq`, `true`, `false`, add axioms
`forall X. eq(X, X) = true` and `eq(s, t) = false`, and replace the goal with
`true = false`. Now the goal is ground and the prover can proceed by completion.

The main loop is a DISCOUNT loop with state (R, J, Q):

- R - the *active set*: rewrite rules and retained unorientable equations.
- Q - the *passive set*: unprocessed critical pairs formed from R.
- J - a set of *ground-joinable* equations, used for subsumption.

The active set is small (|R| ~ 10,000 in a hard run) and stores full proof
objects; the passive set is enormous (|Q| ~ 10,000,000 and up) and is stored in
a stripped, space-efficient form (Section 4.6). The loop:

    (R, J, Q) := ({}, {}, Axioms)
    while Q is non-empty:
      P := remove lowest-scoring element of Q
      if P's parent rules are still present in R:        # else P is stale
        normalise P using R to get  t = u
        if t != u and (t = u) is not connected and (t = u) is not subsumed by J:
          if (t = u) is ground joinable:
            add t = u to J
          else:
            orient t = u and add it to R
            for all critical pairs cp of (t = u) and R:
              normalise cp using only the oriented rules in R
              if cp non-trivial: add cp to Q
            normalise goal using R
            if goal trivial: return "theorem"
            simplify rules in R wrt each other, capped at ~5% of total runtime
    return "countersatisfiable"

Note the explicit budget cap on interreduction (5% of runtime): interreduction
is valuable but can dominate, so Twee bounds it.

#### 4.3 Critical Pair Selection (Scoring)

Selecting *which* passive critical pair to process next is the single most
important search decision; the score function must be cheap (it runs on every
critical pair) yet predictive. Twee scores by a weighted size of the two sides
of the critical pair, with several adjustments:

- Base score: `4*weight(t) + weight(u)` where t is the larger side and u the
  smaller. The bigger side dominates (factor 4), biasing toward small,
  general critical pairs.
- Variables weigh slightly less than function symbols, to favour more general
  (more heavily-variabled) rules.
- The critical pair's *depth* is added (axioms have depth 0, their critical
  pairs depth 1, etc.), to encourage using *all* axioms rather than tunneling
  down one lineage.
- Repeated subterms are counted once (the term is weighed as a DAG, not a tree),
  on the rationale that identical subterms form the same critical pairs and get
  rewritten together.
- A critical pair of the form `eq(v, w) = false` (the goal-encoding function)
  with v and w *unifiable* is given a fixed cost of 1, because selecting it
  immediately proves the goal. This goal-detection shortcut is also used by
  WALDMEISTER and is vital for existential goals.

#### 4.4 Term Ordering

Twee always uses KBO (Knuth-Bendix Ordering) with all function symbols of weight
1, with the precedence set so that *more frequently occurring* function symbols
are *smaller*. Fixing weight 1 keeps KBO cheap and avoids per-problem tuning.

#### 4.5 Weak Rewrite Rules

Completion sometimes derives equations like `f(x, y) = g(x, z)` where a variable
(here z) occurs on only one side. These are awkward to rewrite with (what value
for z?). Twee splits such an equation into *nicely behaved* pieces. It defines a
*weak rewrite rule* `t ~~> u`, which is like an ordinary rule but only requires
`t >= u` (greater-or-equal under all ground substitutions) rather than `t > u`;
to keep termination, a weak step `t*sigma ~~> u*sigma` is only performed when
`t*sigma` and `u*sigma` are syntactically *different*. Using a minimal term
`bottom` of the order, `f(x, y) = g(x, z)` splits into the ordinary rule
`f(x, y) -> g(x, bottom)` and the weak rule `g(x, z) ~~> g(x, bottom)`. In
general any equation can be safely split into ordinary rules `t -> u` (t > u),
weak rules `t ~~> u` (t >= u), and unorientable equations `t = u` whose two sides
share the same variable set. Twee performs this split whenever an equation is
about to enter R.

#### 4.6 Indexing and the Passive Set

- **Term representation - flatterms.** A term is flattened into an array of
  symbols, each paired with the size of the subterm rooted there. E.g.
  `f(x, g(x, y))` becomes `[f:5][x:1][g:3][x:1][y:1]`. Each symbol is stored as
  an integer ID (functions positive, variables negative), so a term is a plain
  int array: equality is a `memcmp`, and a subterm is an array slice viewable as
  a flatterm in its own right. This minimizes garbage-collector pressure and
  makes matching/unification efficient tail-recursive loops over arrays. (This
  flatterm idea is taken directly from WALDMEISTER.)
- **Rewrite indexing - perfect discrimination trees**, including WALDMEISTER's
  refinements, used for finding rewrite-rule left-hand sides that match a
  subterm; the implementation avoids backtracking points unless necessary. There
  is no separate *unification* index, because that is not usually the bottleneck.
- **Passive-set compression.** The passive set grows quadratically in |R| (any
  pair of rules can overlap), reaching a hundred million entries. Early Twee ran
  out of memory in ~30 minutes. The fix, again from WALDMEISTER: do not store the
  terms of a critical pair at all. Store only (1) the two parent rule IDs, (2) the
  overlap position, and (3) the score. About 12 bytes per critical pair. When a
  critical pair is selected, its terms are *reconstructed* from the rule IDs and
  position. This lets Twee run for hours. (It also explains the loop's
  "if P's parent rules are still present in R" guard: a parent may have been
  back-simplified away, making the stored critical pair stale.)

#### 4.7 The Horn-Clause Extension

Twee is a *unit* equality prover, but Horn clauses (one positive literal,
several negative) are encoded as equations by the Claessen-Smallbone technique
(IJCAR 2018, "Efficient encodings of first-order Horn formulas in equational
logic"). A Horn clause is turned into conditional rewriting expressed purely
equationally, so the unit-equality engine can discharge Horn problems without a
separate calculus. This widens Twee's reach to the Horn fragment while keeping
the core a pure completion engine.

#### 4.8 Proof Output / Certificate

Soundness is guaranteed by an LCF-style kernel (in the Edinburgh-LCF sense):
every member of the active set carries a *proof object*, and a small trusted
checker (~one page of code) validates it. The only permitted proof steps are
reflexivity, symmetry, transitivity, congruence, and "apply an axiom or lemma".
A rule cannot enter the active set without a valid proof; any invalid step is a
fatal error. Crucially, only the *active* set carries proof objects (the passive
set does not), which is what keeps proof recording cheap. Once the goal is
proved, the proof object is flattened into a human-readable sequence of rewrite
steps, with lemmas introduced (any active rule may become a lemma) to avoid
exponential-size proofs.

#### 4.9 Goal Transformation (optional goal-direction)

Completion is goal-*agnostic*: it just completes until the goal joins. Twee's
frontend can optionally make the search goal-directed by a simple but effective
transformation: for every function term `f(...)` appearing in the goal,
introduce a fresh constant `a` and add the axiom `f(...) = a`. For a goal
`f(g(a), b) = h(c)`, add `f(g(a), b) = d1`, `g(a) = d2`, `h(c) = d3`;
simplification rewrites the first axiom to `f(d2, b) = d1` and the goal to
`d1 = d3`. Effects: (1) goal subterms normalise to constants, so critical pairs
touching goal terms get lower scores, and (2) new critical pairs involving these
constants are likely relevant to the goal. The CASC-style best configuration
*timeslices* between Twee-with-goal-transformation and plain Twee, since they
solve somewhat different problem sets.

#### 4.10 Performance Niche

Evaluated on all 981 unsatisfiable UEQ problems of TPTP 7.4.0 (5-minute limit),
the timesliced Twee comes close to WALDMEISTER and beats or matches E on many
classes. Twee is *strong* on lattices (LAT) and relation algebra (REL) - problem
classes full of permutative/commutative operators where its ground-joinability
and connectedness redundancy tests shine - and on unusual problems where no
prover has special heuristics. Its known weakness is rings (RNG), where the fixed
term order is suboptimal.


### 5. Term Orderings and Their Efficient Computation

A *reduction order* > used in (unfailing) completion must be: well-founded;
stable under substitution (s > t implies s*sigma > t*sigma); and monotone /
compatible with contexts (s > t implies u[s] > u[t]). For unfailing completion
it must also be *total on ground terms*. The two standard families are the
Knuth-Bendix Ordering (KBO) and the Lexicographic Path Ordering (LPO). Loechner's
two "Things to know when implementing ..." papers (KBO: Journal of Automated
Reasoning, 2006; LPO: International Journal on AI Tools, 2006) are the canonical
implementer references; their core result is that the *naive* implementations are
asymptotically bad (KBO quadratic, LPO exponential) and that careful
implementations bring KBO to linear and LPO to polynomial time. This is directly
actionable for thvm, so we give both algorithms concretely.

#### 5.1 Knuth-Bendix Ordering (KBO)

KBO is parameterized by a *weight function* w (a nonnegative integer weight per
symbol, with w(c) > 0 for constants and a special handling of unary symbols of
weight 0), a positive variable weight w0, and a *precedence* (a strict order on
function symbols). The weight of a term is the sum of the weights of its symbols.
The definition of `s >_KBO t`:

1. (Variable condition) Every variable occurs in s at least as often as in t -
   i.e. for each variable x, count(x, s) >= count(x, t). If this fails, s >_KBO t
   is false. (Stability under substitution forces this.)
2. If the variable condition holds, then compare:
   a. If weight(s) > weight(t): s >_KBO t.
   b. If weight(s) == weight(t), break the tie:
      - If head(s) has higher precedence than head(t): s >_KBO t.
      - If head(s) == head(t) == f and arities equal: compare the argument
        tuples *lexicographically* by >_KBO (first differing argument decides).

**Why naive is quadratic.** The naive recursion recomputes term weights and
variable counts at every recursive call: comparing two terms of size n triggers
O(n) recursive comparisons each of which independently traverses subterms of
size O(n) to compute weights/counts, giving O(n^2).

**Loechner's linear algorithm.** The trick is to *interleave* the weight
comparison, the variable-count comparison, and the lexicographic comparison into
a *single simultaneous traversal* of the two terms, accumulating partial
information rather than recomputing it. Maintain:

- A running integer `wbal` = weight(s-part seen) - weight(t-part seen), the
  *weight balance*.
- A *variable balance* table `vbal`: for each variable, count(in s) - count(in t),
  accumulated as the traversal visits variable occurrences; plus a small set of
  "variables seen with positive balance" / "with negative balance" flags so the
  variable condition (all balances of one sign) can be checked in O(1) amortized.
- A lexicographic-status value `lex` in {EQ, GT, LT, INCOMPARABLE} representing
  the precedence/lexicographic outcome of the structural comparison done so far.

A combined routine `kbo_compare(s, t)` walks s and t together. Where the two
terms have the same head it recurses argument-by-argument, threading and updating
`wbal`, `vbal`, and `lex`; where they differ in head it switches to two
*one-sided* traversals (`kbo_traverse_s` adds weights/var-counts of the remaining
s side to the balances, `kbo_traverse_t` subtracts the t side), so every symbol
of each term is visited exactly once. At the end:

- Reject if the variable balance violates the one-sided variable condition.
- Else decide by sign of `wbal`; on `wbal == 0` decide by `lex` (which already
  encoded the precedence-then-lexicographic tie-break, computed in the same pass).

Because each symbol of each term is touched exactly once and all per-symbol work
is O(1), the whole comparison is O(|s| + |t|): linear, asymptotically optimal.
The decisive engineering points Loechner stresses: (1) compute the weight
balance and variable balance *incrementally*, never via a separate full weight
function; (2) capture the lexicographic result *during* the same traversal; and
(3) treat the "heads differ" case by switching to one-sided accumulation rather
than restarting.

For unfailing completion an implementer also needs the *ground-instance* query
`t >=_C u` "for all ground substitutions satisfying a variable-order constraint
C", used by Twee's ground-joinability test (Section 6.1). For KBO this reduces to
checking whether a linear expression `weight(t) - weight(u)` (a linear combination
of the variables' weights) is nonnegative for all admissible variable values -
a small linear-arithmetic minimization, doable in linear time given a fixed
variable order, even though the *fully general* ordering-constrained KBO query is
NP-complete (Korovin-Voronkov, LICS 2000).

#### 5.2 Lexicographic Path Ordering (LPO)

LPO is defined purely from a precedence `>_F` on function symbols (no weights).
`s >_lpo t` holds iff one of:

- (LPO-1, "subterm") some argument s_i of s satisfies `s_i >_lpo t` or
  `s_i == t`; or
- (LPO-2, "precedence/major") `head(s) >_F head(t)` *and* s >_lpo t_j for every
  argument t_j of t; or
- (LPO-3, "lexicographic") `head(s) == head(t)`, the argument tuples are equal up
  to some index, the first differing argument satisfies `s_i >_lpo t_i`, *and*
  s >_lpo t_j for every later argument t_j of t.

Pseudocode for the ground/standard comparison (returns GT, LT, or NGE):

    function lpo_gt(s, t) -> bool:           # s >_lpo t ?
      # LPO-1: subterm dominance
      if s is a function term f(s1..sm):
        for s_i in s1..sm:
          if s_i == t or lpo_gt(s_i, t):
            return true
      # if t is a variable and not caught above, s !>_lpo t (var must occur in s)
      if t is a variable:
        return t occurs strictly inside s        # i.e. caught only via LPO-1
      # both are function terms now: g(t1..tn) = t
      let f = head(s), g = head(t)
      if precedence(f) > precedence(g):          # LPO-2
        return all( lpo_gt(s, t_j) for t_j in t1..tn )
      if f == g:                                  # LPO-3
        # find first differing argument
        for (s_i, t_i) in zip(args(s), args(t)):
          if s_i == t_i: continue
          return lpo_gt(s_i, t_i) and all( lpo_gt(s, t_j) for later t_j )
        return false                              # tuples identical -> equal
      return false                                # precedence(f) < precedence(g)

**Why naive is exponential.** The clauses LPO-2 and LPO-3 each contain an inner
"s >_lpo every remaining t_j" check, *and* the recursion in LPO-1 re-asks
`lpo_gt(s_i, t)` with the *same* t. The same pair of subterms is re-compared along
many different branches, and there is no memoization, so the call tree blows up
exponentially in the worst case.

**Loechner's polynomial algorithm.** Two ideas tame it:

1. **Return a three-valued result and reuse it.** Replace the boolean `lpo_gt`
   with a single function `lpo_cmp(s, t)` returning one of {GT, LT, EQ, NGE}
   (or at least {GT, LT/EQ, otherwise}). One traversal then yields enough to
   decide LPO-1, LPO-2 and LPO-3 without separate re-descents in each direction;
   in particular the "for all t_j" and "exists s_i" subchecks share the same
   recursive comparisons rather than recomputing them.
2. **Memoize on subterm-pair identity.** Because flatterm subterms are array
   slices with cheap identity, comparisons `lpo_cmp(s_i, t_j)` can be cached
   (a table keyed by the pair of subterm positions). With memoization there are
   O(|s| * |t|) distinct subterm pairs and each is computed once, giving a
   polynomial (roughly quadratic) bound instead of exponential.

The combined three-valued, memoized routine:

    function lpo_cmp(s, t) -> {GT, LT, EQ, NGE}:    # cached on (s, t) identity
      if s == t: return EQ
      if s is var: return (LT if s occurs in t else NGE)
      if t is var: return (GT if t occurs in s else NGE)
      f := head(s); g := head(t)
      # gather child comparisons once
      s_dominates_t := any( lpo_cmp(s_i, t) in {GT, EQ} for s_i in args(s) )
      if s_dominates_t: return GT                    # LPO-1 for s > t
      t_dominates_s := any( lpo_cmp(s, t_j) in {LT, EQ} for t_j in args(t) )
      if t_dominates_s: return LT                    # symmetric LPO-1 for t > s
      if precedence(f) > precedence(g): return GT    # LPO-2 (the "for all t_j" is
                                                     #   implied since not t_dom)
      if precedence(f) < precedence(g): return LT
      # f == g: lexicographic on arguments
      for (s_i, t_i) in zip(args(s), args(t)):
        c := lpo_cmp(s_i, t_i)
        if c == GT: return GT      # remaining t_j domination already excluded above
        if c == LT: return LT
        # c == EQ: continue to next argument
      return NGE                   # arities/structure differ with no decision

Subtlety: the "for all remaining t_j" conditions of LPO-2/LPO-3 are subsumed by
having *already* tested `t_dominates_s` (the symmetric LPO-1) and found it false;
this is exactly the reuse that the three-valued formulation buys, and it is why
memoizing pairwise child comparisons collapses the exponential blowup. Loechner's
paper measures several variants and confirms the polynomial version is the one to
ship.

#### 5.3 Precedence / Ordering Auto-Generation

A reduction order is only as good as its parameters (precedence, weights). Provers
*synthesise* these from the problem. WALDMEISTER's heuristic auto-generates a
precedence and KBO weights from a syntactic analysis of the axioms (e.g. ordering
symbols by occurrence count and arity so that more orientable rules result), and
offers several preset reduction-order strategies to timeslice over. Twee instead
*fixes* the order (all weights 1; more frequent symbols smaller) - the deliberate
"fixed heuristics" choice of Section 4.1. The lesson for thvm: the order choice is
a first-class strategic lever, and on some classes (rings) it is the difference
between solving and not.


### 6. Redundancy Criteria for Unorientable Equations

Once unorientable equations are kept, the search can drown in non-joinable but
*useless* critical pairs. Twee's two redundancy tests (its main performance edge
on permutative problems) directly answer "which non-joinable critical pairs can I
throw away?". Both generalise far beyond the special-cased AC handling other
provers use; they handle *any* permutative equation.

#### 6.1 Ground Joinability

A critical pair `s <- t -> u` may not be joinable as written, yet *every ground
instance* of it may be joinable. Such a critical pair is *ground joinable* and
hence redundant. Example with an associative-commutative `+`:

    x + (y + z)  <-  (x + y) + z  ->  z + (x + y)

is not joinable, but the ground instance with `a < b < c` joins:
`c + (a + b) -> a + (c + b) -> a + (b + c)`, and similarly for every ground
instance. Martin and Nipkow (1990) suggested testing ground joinability by case
analysis over *all total orderings of the variables*. That is correct but
explodes (too many orderings). Twee's algorithm (its take on
Avenhaus-Hillenbrand-Loechner 2003) considers fewer cases by (1) constraining
only a *subset* of variables and (2) using `<=` (allowing equal variables) rather
than only `<`. Sketch:

    1. Choose a strict total order on all variables using only "<", e.g. x<y<z.
    2. Show the critical pair joins under that order (normalise both sides with
       the order-parameterised rewrite relation >=_C and check they meet).
    3. Generalise that one case: remove variables from the ordering and/or
       replace "<" with "<=", as long as joinability is preserved.
    4. Repeat with an order not yet covered.
    5. When all strictly-ordered cases are covered, the remaining cases involve
       "==" (equal variables): unify the equal variables and recurse.

The engine ingredient is an order-*parameterised* rewrite relation: given a
variable-order constraint C, define `t >=_C u` to mean "for all grounding sigma
satisfying C, t*sigma > u*sigma". A *weakened* rewrite step `t -> u under C` is
allowed when `t >=_C u` and `t != u` syntactically (the weakening - allowing
"=" instances that do nothing - is what lets one rewrite proof cover both the
strict and the equal sub-cases at once). With this, "join under constraint C" is
just "ordered-normalise both sides under >=_C and compare". For KBO the only
order-specific primitive needed is the test `weight(t) - weight(u) >= 0 for all
admissible variable values` (Section 5.1) - so, pleasingly, "the rest of the
ground-joining code is independent of the term order; to support e.g. LPO one
just implements >=_C for it." Twee deliberately restricts constraints to
*variable* orderings (not arbitrary `x + y < z`-style term constraints) because
arbitrary ordering constraints make the case-split potentially infinite and make
the KBO constraint query NP-complete; the restricted form terminates and stays
cheap.

#### 6.2 Connectedness

Ground joinability is heavyweight (it builds and analyses a case split).
*Connectedness* is a cheaper, complementary criterion that works well when an
unorientable equation is applied *under* another function. A critical pair
`s <- t -> u` is *connected* if there is a rewrite proof
`s = t1 ... tn = u` in which every intermediate `t_i` is *strictly less than t*
(Bachmair-Dershowitz, "Critical pair criteria for completion", 1988). Any
connected critical pair is redundant. The practical relaxation: when joining
`s <- t -> u`, allow a rewrite step `v -> w` (using an unoriented equation, even
when you do not statically know `v >= w`) provided (1) `w < t` (connectedness:
intermediate stays below the peak t) and (2) `v*sigma > w*sigma` for the relevant
ground sigma (termination of the join). Condition (1) lets the join *increase*
the term mid-proof as long as it stays under the original peak; condition (2)
keeps the join terminating. The combination of ground joinability *and*
connectedness in Twee is empirically much stronger than either alone - each
catches cases the other misses - which is why Twee enables both.


### 7. WALDMEISTER

WALDMEISTER (Hillenbrand, Buch, Vogt, Loechner; MPI/Kaiserslautern) is the
classic high-performance unfailing-completion prover for UEQ and a long-running
CASC UEQ champion. thvm's ATP is a port of its engine, so this subsection is the
direct design reference. WALDMEISTER's "Journal of Automated Reasoning"
overview ("WALDMEISTER - High-Performance Equational Deduction", 1997) and the
later "New WALDMEISTER Loop" / "phytography" papers describe an explicit
*engineering* approach: identify the time- and space-critical points of unfailing
completion and attack each one.

#### 7.1 Three-Level Architecture

WALDMEISTER is organised in three layers, a useful template for a port:

- **Top level** - selection heuristics and reduction-ordering parameters
  (strategy).
- **Mid level** - the *inference machine*: the completion rules aggregated into
  the main loop (the saturation engine).
- **Bottom level** - the efficient basic operations: matching, unification,
  normalisation, indexing.

System behaviour is controlled by three parameter groups: (1) the high-level
reduction ordering plus selection function; (2) the handling of *unselected*
equations (a preprocessing/reclassification strategy for the passive set); and
(3) term-normalisation control (traversal strategy and backtracking behaviour).

#### 7.2 The Completion Loop

The "select - normalise - orient - generate" cycle:

    1. Select an equation from the set of critical pairs (the passive set),
       using the selection heuristic.
    2. Simplify (normalise) this equation to normal form using the current rules.
    3. Modify the set of rules according to the equation (orient it; back-simplify
       older rules that the new one reduces, demoting them if their lhs becomes
       reducible).
    4. Generate all new critical pairs from the new rule against existing rules.
    5. Add the equation to the set of rules (the active set).

This is the DISCOUNT-style passive/active split: a set of *waiting* facts
(critical pairs) and a set of *selected* facts (rules). The set sizes are
asymmetric exactly as in Twee (|active| modest, |passive| huge), which dictates
the data-structure choices below.

#### 7.3 Critical-Pair Selection Heuristics

WALDMEISTER's selection function assigns each waiting critical pair a numeric
*weight* (lower = selected sooner). The standard family of heuristics
("ClasHeuristics"):

- **AddWeight** - the *additive* measure: the cost of a critical pair `s = t` is
  the *sum* of the term sizes (weights), `weight(s) + weight(t)`. Cheap, and the
  classic default; it prefers small equations.
- **GtWeight** - the *greater-term* measure: cost driven by the larger of the two
  sides, `weight(greater(s, t))` (optionally with a multiplier), reflecting that
  the bigger side governs how much trouble the equation causes. (Twee's
  `4*weight(bigger) + weight(smaller)` of Section 4.3 is a tuned descendant of
  exactly this idea.)
- **MixWeight** - a *mixture* combining additive and greater-term contributions
  (e.g. a weighted blend), aiming to get the robustness of AddWeight with the
  selectivity of GtWeight.
- **CPinGoal** - a *goal-directed* measure: critical pairs whose terms occur in,
  or are relevant to, the goal are favoured (lower cost). This is the
  WALDMEISTER counterpart of Twee's goal transformation, and like Twee's
  `eq(v,w)=false` shortcut it sharply helps goal-reaching, especially on
  existential goals. The "if a critical pair would immediately prove the goal,
  give it minimal cost" rule originates here.

Because the selection function runs on millions of passive critical pairs, it
must be fast; the weighted-size measures are O(term size) and are computed when
the critical pair is created.

#### 7.4 Precedence / Reduction-Order Auto-Generation

WALDMEISTER does not ask the user for a precedence. It *auto-generates* the
reduction ordering (which KBO/LPO, with which precedence and weights) from a
syntactic analysis of the axioms, and ships several preset ordering strategies
that the CASC driver timeslices over. The heuristic favours precedences/weights
that make as many axioms as possible orientable and that keep right-hand sides
small. This auto-tuning is one reason WALDMEISTER outperforms a fixed-order prover
on a heterogeneous problem set (and conversely is the lever Twee deliberately
forgoes).

#### 7.5 Reduction-Order Machinery (KBO and LPO)

WALDMEISTER is where the efficient KBO/LPO algorithms of Section 5 were
engineered and measured (Loechner's two papers came directly out of this work).
The implementation uses the linear-time KBO comparison (single combined traversal
with weight balance and variable balance) and the polynomial memoized LPO
comparison. For a thvm port, these two routines are the load-bearing inner loops:
they are called on essentially every rewrite attempt and every orientation
decision, so their asymptotics matter in practice, not just in theory.

#### 7.6 Term Representation - Flatterms and Closure-Based Reduction

- **Flatterms.** WALDMEISTER represents terms as flattened symbol arrays (the
  representation Twee later borrowed): contiguous, cache-friendly, with O(1)
  subterm access and `memcmp` equality. This is the bottom-level term type
  underneath matching, unification and normalisation.
- **Closure-based reduction.** Repeatedly normalising terms with the rule set is
  the dominant cost. WALDMEISTER avoids materialising fully-substituted terms when
  it can, representing a substituted term as a *closure* (a term-plus-substitution
  pair) and only instantiating lazily / on demand during normalisation. This
  cuts both time (no eager copying) and space. Combined with careful control over
  the normalisation traversal and its *backtracking* (parameter group 3 of
  Section 7.1), it keeps the inner rewrite loop tight.
- **Indexing - perfect discrimination trees** for rewriting (the refinements Twee
  cites), to find candidate rule left-hand sides matching a subterm without
  scanning all rules. The implementation minimizes backtracking-point creation.

#### 7.7 Memory Architecture

WALDMEISTER pairs the algorithms above with *specialised memory management* and
*space-saving representations*. The decisive one is the compact passive-set
encoding (the source of Twee's 12-byte critical-pair trick of Section 4.6): a
waiting critical pair is *not* stored as two terms but as a tiny record - parent
rule references plus overlap position plus score - and reconstructed on
selection. Given that the passive set dominates memory (tens to hundreds of
millions of entries), this is the difference between running for hours and
running out of memory in minutes. WALDMEISTER's broader memory discipline (custom
allocators, arena-style management for the high-churn term and critical-pair
objects) is part of why a 65000-line C engine sustains very high deduction rates.


### 8. The Superposition Calculus for Equality

Completion is a special case of a more general machinery: *superposition*, the
state-of-the-art calculus for first-order logic *with equality*. Understanding
superposition clarifies exactly what completion is and is not.

#### 8.1 From Paramodulation to Superposition

*Paramodulation* (Robinson-Wos, 1969) is the equality analogue of resolution:
given a clause containing `l = r` and another clause with a subterm `s`, if l and
s unify with m.g.u. sigma, derive the clause with `s*sigma` replaced by `r*sigma`
(plus the remaining literals). Unrestricted paramodulation is complete but wildly
prolific: it paramodulates into and out of everything, in both directions,
including into variables.

*Superposition*, developed by Bachmair and Ganzinger (around 1990-1994), is
*ordered, restricted paramodulation* that remains refutationally complete while
pruning the vast majority of paramodulation inferences. Two restrictions do the
work:

1. **Ordering restrictions.** Fix a reduction order > total on ground terms.
   Superposition is only applied (a) using the *larger* side of an equation
   (`l > r`, so rewrite `l -> r` only), (b) into *maximal* terms of *maximal*
   literals of a clause, and (c) never into a variable. This is precisely the
   ordered-rewriting discipline of unfailing completion lifted to clauses.
2. **Selection functions.** A *selection function* may select some negative
   literals of a clause; if any literal is selected, inferences are restricted to
   the selected literals (overriding the ordering for negatives). Selection is a
   completeness-preserving strategy knob that drastically narrows the search.

#### 8.2 The Inference Rules

For clauses with equality, the superposition calculus consists of (Bachmair-
Ganzinger):

- **Superposition** (positive and negative variants): the ordered-paramodulation
  rule above. From `C \/ l = r` and `D \/ s[l'] (=|!=) t` with l, l' unifiable by
  sigma, under the ordering/selection side-conditions, derive
  `(C \/ D \/ s[r] (=|!=) t)*sigma`. This is "rewriting one clause's literal by
  another clause's equation, where allowed".
- **Equality Resolution.** From `C \/ s != t` with s, t unifiable by sigma,
  derive `C*sigma`. (Discharges a trivially-false negative equality - the
  equality analogue of resolving `s != s`.)
- **Equality Factoring** (alternatively Ordered Factoring + Merging
  Paramodulation). From `C \/ s = t \/ s' = t'` with s, s' unifiable, derive a
  clause that merges the two positive equalities. This handles the case two
  positive equality literals would otherwise need to be factored; it is required
  for completeness on clauses with multiple positive equalities.

#### 8.3 Simplification: Demodulation and Rewriting

Superposition provers spend most of their effort not *generating* but
*simplifying*. **Demodulation** (rewriting) uses an oriented unit equation
`l -> r` (with `l > r`) to rewrite matching subterms in other clauses,
*replacing* them - a *simplifying* (clause-deleting) inference, not a generating
one. Demodulation is precisely ordered rewriting inside the saturation loop, and
it is the bridge to completion: in a pure unit-equality problem, demodulation
*is* the rewriting of completion.

#### 8.4 Redundancy and Completeness

The theoretical heart of Bachmair-Ganzinger is an *abstract redundancy* notion:
a clause is *redundant* if it is entailed by smaller (in the clause order)
clauses in the set; an inference is redundant if its conclusion is entailed by
clauses smaller than its largest premise. Redundant clauses may be deleted and
redundant inferences skipped *without losing refutational completeness*. This
single abstraction justifies tautology deletion, subsumption, demodulation, and
- with a refined definition - the joinability and connectedness criteria of
Section 6. The completeness proof is *model-theoretic*: from a saturated,
satisfiable clause set one *constructs* a model by a term-rewriting / candidate-
model construction (the "model functor" built from the productive clauses under
the term order). A saturated set containing the empty clause is unsatisfiable;
otherwise the constructed model witnesses satisfiability. The practical payoff:
an implementer is free to add *any* simplification that fits the redundancy
abstraction, confident completeness is preserved - this is the formal license
behind every demodulation/subsumption/joinability optimization.

#### 8.5 Completion as Superposition Restricted to Unit Equations

The relationship the thvm port should keep in mind: **unfailing completion is
exactly the superposition calculus specialised to unit equations.** When every
clause is a single positive equality `l = r` (or, for the goal, a single negative
equality `s != t`):

- Superposition between two unit positive equalities, restricted to the larger
  side and to non-variable positions = *critical pair generation* of completion.
- Demodulation by an oriented unit equality = *ordered rewriting* /
  *interreduction*.
- Equality Resolution on the single negative goal literal `s0 != t0` = "the goal
  has joined" (s0 and t0 became unifiable / identical).
- Equality Factoring is *vacuous* (no clause has two positive equalities).
- The abstract redundancy notion specialises to joinability, ground joinability
  and connectedness.

So a unit-equality superposition prover *is* an unfailing-completion prover, and
vice versa. This is why Twee and WALDMEISTER, though described in
completion language, sit squarely inside Bachmair-Ganzinger theory and inherit
its completeness guarantees. For thvm, the consequence is reassuring: the
WALDMEISTER-style engine it ports is not an ad-hoc procedure but a complete
calculus, and any redundancy test that fits the abstract criterion (the
joinability and connectedness tests above) can be added soundly.


### 9. Zipperposition

Zipperposition (Simon Cruanes) is a research-oriented superposition prover, born
as a flexible OCaml testbed for experimenting with superposition extensions, and
it became the vehicle for extending superposition *to higher-order logic*. Its
relevance here is twofold: it shows where the superposition machinery generalises
beyond first-order equality, and it is a model for an *extensible* prover
architecture.

- **Lambda-free higher-order superposition** (Bentkamp, Blanchette, Cruanes,
  Waldmann; "Superposition for Lambda-Free Higher-Order Logic", IJCAR 2018). This
  is a *graceful* generalisation: a calculus for the lambda-free fragment
  (partial application and applied variables, but no lambda abstraction) that
  coincides with ordinary first-order superposition on first-order input. The
  technical hurdle is that the term order need no longer be *fully monotonic*
  (compatible with all contexts) in the higher-order setting; the calculus is
  therefore parameterised by an order that may lack full monotonicity, which
  lets it reuse lambda-free higher-order variants of LPO and KBO.
- **Superposition with lambdas** (Bentkamp et al., CADE 2019 / journal). Extends
  the calculus to *full* higher-order logic with lambda abstraction, handling the
  attendant unification (higher-order unification is undecidable and may yield
  infinitely many unifiers, so the calculus must enumerate them lazily) and the
  *extensionality* axiom (functions equal iff they agree on all arguments) by
  built-in inference rather than as a brittle axiom.
- **Extensional reasoning.** Naively asserting extensionality as an axiom floods
  the search; Zipperposition's calculi instead build extensional reasoning into
  dedicated inference rules, keeping it controlled.
- **Role as a testbed.** Because it is written for flexibility over raw speed,
  Zipperposition is where new superposition ideas (higher-order, induction,
  arithmetic extensions, the Matryoshka-project calculi) are prototyped before
  migrating into industrial-strength provers like E. It has performed strongly in
  the higher-order divisions of CASC.

The takeaway for an equational-prover implementer: the same ordered-paramodulation
skeleton, redundancy abstraction, and term-order dependence carry all the way up
to higher-order logic; the order just gets weaker (loses full monotonicity) and
unification gets harder (enumerative, possibly infinite).


### 10. Other Provers, Briefly

#### 10.1 E

E (Stephan Schulz) is a high-performance first-order superposition prover. Its
equational handling is textbook Bachmair-Ganzinger superposition with
demodulation, and it implements both KBO and LPO with the efficient comparison
algorithms. E's distinguishing strengths are its *clause selection* heuristics
(many weight functions, combined in priority queues, plus learned/auto
strategies), its sophisticated *term indexing* (perfect discrimination trees,
fingerprint indexing, feature-vector indexing for subsumption), and its
*auto-configuration* ("E auto" / "auto-schedule" picks heuristics from problem
features). In the UEQ niche E is competitive but typically behind the
completion-specialists WALDMEISTER and Twee, because the unit-equality redundancy
tests (ground joinability, connectedness) are not its focus. E is the production
prover into which Matryoshka-project (Zipperposition-prototyped) higher-order
extensions have been ported ("Faster, higher, stronger: E 2.3").

#### 10.2 Prover9

Prover9 (William McCune, successor to Otter) is a first-order resolution/
paramodulation prover. Its equality handling is *ordered paramodulation plus
demodulation* with KBO or LPO term orders, in the Otter "given-clause" loop, with
back- and forward-demodulation and dynamic ordering. It is not specialised for
UEQ - it predates the modern superposition redundancy machinery in its
optimisation focus - but it is historically central (McCune's EQP, a sibling, was
the prover that settled the Robbins conjecture, an equational problem, via
associative-commutative paramodulation). Prover9 is a good reference for a
*readable* implementation of paramodulation and the given-clause architecture.


### 11. Synthesis: What Matters for a WALDMEISTER-style Port (thvm)

For implementers of an equational prover that ports WALDMEISTER's unfailing
completion, the load-bearing pieces, in rough priority order:

1. **A ground-total reduction order with efficient comparison.** KBO with the
   linear-time combined-traversal comparison (weight balance + variable balance +
   in-pass lexicographic), and LPO with the three-valued, memoized polynomial
   comparison. These run on nearly every inference; naive versions are
   asymptotically fatal. (Section 5.)
2. **Ordered rewriting with retained unorientable equations.** Per-instance
   direction decided by the order test; this is what makes completion *unfailing*
   and refutationally complete on commutative/permutative theories. (Section 3.)
3. **A passive/active DISCOUNT loop with compact passive storage.** Store passive
   critical pairs as (parent IDs, overlap position, score) and reconstruct on
   selection; this is the single most important space optimization. (Sections
   4.6, 7.7.)
4. **A fast, predictive critical-pair selection heuristic** (AddWeight / GtWeight
   / MixWeight family, plus a CPinGoal goal-directed term and a goal-immediate
   shortcut). (Sections 4.3, 7.3.)
5. **Strong redundancy tests for unorientable equations**: ground joinability
   (case split over variable orders, generalised with `<=`) and connectedness.
   These are Twee's and WALDMEISTER's edge on the permutative-operator problems
   (lattices, relation algebra) that dominate UEQ. (Section 6.)
6. **Flatterm representation, discrimination-tree rewrite indexing, and
   closure-based (lazy-substitution) normalisation** as the bottom-level
   inner-loop engine. (Sections 4.6, 7.6.)
7. **An LCF-style proof certificate** carried only on active facts, checked by a
   small trusted kernel, with lemma extraction for compact human-readable proofs.
   (Section 4.8.)


### Sources

- Smallbone, N. "Twee: An Equational Theorem Prover (System Description)",
  CADE-28, 2021. https://smallbone.se/papers/twee.pdf and project page
  https://nick8325.github.io/twee/
- Hillenbrand, Buch, Vogt, Loechner. "WALDMEISTER - High-Performance Equational
  Deduction", Journal of Automated Reasoning, 1997.
  https://link.springer.com/article/10.1023/A:1005872405899 ; implementation
  notes https://www.mpi-inf.mpg.de/departments/automation-of-logic/software/waldmeister/implementation
- Loechner, B. "Things to Know when Implementing KBO", Journal of Automated
  Reasoning 36, 289-310, 2006.
  https://link.springer.com/article/10.1007/s10817-006-9031-4
- Loechner, B. "Things to Know when Implementing LPO", International Journal on
  Artificial Intelligence Tools, 2006.
  https://www.worldscientific.com/doi/abs/10.1142/S0218213006002564
- Bachmair, L., Dershowitz, N., Plaisted, D.A. "Completion without failure",
  Rewriting Techniques (1989).
- Bachmair, L., Dershowitz, N. "Critical pair criteria for completion",
  Journal of Symbolic Computation 6(1), 1988.
- Avenhaus, J., Hillenbrand, T., Loechner, B. "On using ground joinable equations
  in equational theorem proving", Journal of Symbolic Computation 36(1), 2003.
- Martin, U., Nipkow, T. "Ordered rewriting and confluence", CADE-10, 1990.
- Denzinger, J., Kronenburg, M., Schulz, S. "DISCOUNT - a distributed and learning
  equational prover", Journal of Automated Reasoning 18(2), 1997.
- Korovin, K., Voronkov, A. "A decision procedure for the existential theory of
  term algebras with the Knuth-Bendix ordering", LICS 2000.
- Claessen, K., Smallbone, N. "Efficient encodings of first-order Horn formulas in
  equational logic", IJCAR 2018.
- Bentkamp, A., Blanchette, J., Cruanes, S., Waldmann, U. "Superposition for
  Lambda-Free Higher-Order Logic", IJCAR 2018.
  https://matryoshka-project.github.io/pubs/lfhosup_paper.pdf ; "Superposition
  with Lambdas", CADE 2019, https://arxiv.org/abs/2102.00453
- Schulz, S., Cruanes, S., Vukmirovic, P. "Faster, higher, stronger: E 2.3",
  CADE 2019.
- Bachmair, L., Ganzinger, H. "Rewrite-based equational theorem proving with
  selection and simplification" / "Resolution theorem proving", Handbook of
  Automated Reasoning.
- Zucker, P. "Term Ordering Etudes: Ground Lexicographic Path Ordering".
  https://www.philipzucker.com/lpo_etudes/

---


## SMT, Instantiation-Based, and Higher-Order Provers

This section surveys three families of automated theorem provers (ATP systems)
that sit somewhat apart from the classical saturation-based first-order provers
(such as E, Vampire, and SPASS) covered elsewhere in this report. The three
families are:

1. Satisfiability Modulo Theories (SMT) solvers used as first-order theorem
   provers - principally cvc5 and Z3.
2. Instantiation-based provers, whose canonical representative is iProver and
   its underlying Inst-Gen calculus.
3. Higher-order logic (HOL) provers - Leo-III, Satallax, Lash, and agsyHOL -
   together with the connection-method provers leanCoP and nanoCoP, and a brief
   note on the TPTP (Thousands of Problems for Theorem Provers) library and the
   CADE ATP System Competition (CASC).

The emphasis throughout is on the algorithmic idea behind each system and the
engineering tradeoffs an implementer faces, rather than on raw benchmark
numbers. Where a design decision is forced by a fundamental result (for
instance the undecidability of higher-order unification), this is called out
explicitly.

A note on terminology: a "ground" term or literal contains no variables; an
"E-graph" is a congruence-closure data structure representing equivalence
classes of terms under a set of known equalities; a "model" is an
interpretation that satisfies a formula; "saturation" is the process of
deriving consequences with an inference system until no new non-redundant
clause can be produced. These recur below.

---

### 1. SMT solvers as theorem provers: cvc5 and Z3

#### 1.1 What an SMT solver is, and why it matters for ATP

A Satisfiability Modulo Theories (SMT) solver decides the satisfiability of
quantifier-free first-order formulas with respect to a combination of
background theories: linear and nonlinear arithmetic over integers and reals,
fixed-width bit-vectors, arrays, algebraic datatypes, strings, uninterpreted
functions, and others. The two dominant systems are Z3, developed at Microsoft
Research by Leonardo de Moura and Nikolaj Bjorner (de Moura and Bjorner,
"Z3: An Efficient SMT Solver", TACAS 2008,
https://link.springer.com/content/pdf/10.1007/978-3-540-78800-3_24.pdf), and
cvc5, developed by a consortium led by Clark Barrett (Stanford) and Cesare
Tinelli (Iowa), the latest in the Cooperating Validity Checker lineage that
runs CVC, CVC Lite, CVC3, CVC4, and now cvc5 (Barbosa et al., "cvc5: A
Versatile and Industrial-Strength SMT Solver", TACAS 2022,
https://hanielbarbosa.com/papers/tacas2022.pdf).

Although SMT solvers were designed for the quantifier-free fragment, they are
heavily used as theorem provers in two ways. First, they directly accept
quantified problems and reason about them with the instantiation machinery
described below; in TPTP/CASC terms they compete in the typed first-order with
arithmetic (TFA) and related divisions and back the Sledgehammer tool of
Isabelle/HOL. Second, even on pure first-order problems without arithmetic they
provide a useful complementary search strategy. The headline tradeoff is this:
SMT solvers dominate problems whose difficulty is in the theory content
(arithmetic constraints, datatype reasoning, large but shallow Boolean
structure) but historically lagged the saturation provers on pure first-order
problems requiring deep, many-step quantifier reasoning. The reasons for both
sides of that statement are exactly the architecture and the quantifier
handling.

#### 1.2 The CDCL(T) / DPLL(T) architecture

The core architecture is variously called DPLL(T) or, in its modern
conflict-driven form, CDCL(T): Conflict-Driven Clause Learning modulo a theory
T. The acronym DPLL is for Davis-Putnam-Logemann-Loveland, the classical
backtracking search for propositional satisfiability (SAT); CDCL is its modern
descendant that learns a new clause from each conflict and backjumps
non-chronologically.

The structure is a propositional SAT engine driving one or more theory solvers.
The pseudocode skeleton:

```
CDCL(T)(phi):
  # phi is a quantifier-free formula over theory T.
  # Boolean-abstract phi: replace each atom (e.g. x + y <= 3) by a fresh
  # propositional variable, giving a propositional formula phi_bool.
  atoms      = theory atoms of phi
  abstr      = map each atom to a fresh Boolean variable
  loop:
    M = SAT_decide_and_propagate(phi_bool)      # partial Boolean model
    if M is a full propositional model:
        # Hand the conjunction of asserted theory literals to T-solver.
        status, explanation = TSolver(concretize(M))
        if status == SAT:
            return SAT (with model)
        else:
            # T-solver found the literal set T-inconsistent.
            # Add the negation of the conflicting subset as a learned clause
            # (a "theory lemma") and continue.
            phi_bool = phi_bool AND clause(not explanation)
    else if conflict:
        if top-level conflict: return UNSAT
        analyze conflict, learn clause, backjump
```

Two refinements make this practical. (1) Theory propagation: the theory solver
does not wait for a full Boolean model but eagerly reports literals it can
deduce, pruning the Boolean search. (2) Incrementality: the theory solver
maintains its state across the assert/backtrack cycle so that each SAT decision
costs only the marginal theory work.

The key strength for ATP is that the Boolean structure of a problem - case
splits, disjunctions, large conjunctions of constraints - is handled by a
world-class CDCL SAT engine with two-watched-literal propagation, restarts, and
clause learning. Problems that overwhelm a saturation prover with combinatorial
clause explosion are routine here.

#### 1.3 Theory combination: Nelson-Oppen

Real problems mix theories: an array of integers, a bit-vector packed into an
arithmetic term, a datatype holding reals. The classical method for combining
decision procedures for disjoint, stably-infinite theories is the Nelson-Oppen
combination procedure (Nelson and Oppen, 1979). The idea:

- Purify the formula so each atom belongs to exactly one theory, introducing
  fresh shared variables for sub-terms that cross a theory boundary.
- Each theory solver reasons about its own atoms but the solvers must agree on
  which shared variables are equal. They exchange entailed equalities (and, for
  non-convex theories such as integer arithmetic, disjunctions of equalities)
  between shared variables until a fixpoint or a contradiction.

In a modern CDCL(T) solver, Nelson-Oppen equality sharing is realized through a
shared union-find / E-graph: the congruence-closure solver for uninterpreted
functions owns the equality infrastructure, and the other theory solvers
propagate interface equalities through it. This is why uninterpreted-function
congruence closure is the hub of an SMT solver and why the E-graph it builds is
also the substrate for quantifier instantiation (next).

#### 1.4 Handling quantifiers

CDCL(T) decides the quantifier-free fragment. To handle a formula with a
universal quantifier such as forall x. P(x), the solver must produce ground
instances P(t) that, conjoined with the rest, are enough to refute the negated
conjecture. There is no complete terminating procedure for first-order logic
(it is only semi-decidable), and adding theories like arithmetic makes the
quantified fragment outright undecidable, so every method here is a heuristic
for choosing instantiations. Four families are in use.

##### 1.4.1 E-matching (trigger-based instantiation)

E-matching is the workhorse, introduced for high performance by de Moura and
Bjorner ("Efficient E-matching for SMT Solvers", CADE 2007,
https://leodemoura.github.io/files/ematching.pdf). Each quantified formula is
annotated (manually, or by an automatic heuristic) with one or more triggers:
patterns - sub-terms containing the bound variables - that should provoke an
instantiation. For forall x. f(g(x)) = x a natural trigger is f(g(x)).

The mechanism: maintain the E-graph of all ground terms currently asserted.
Whenever a ground term in the E-graph matches the trigger pattern modulo the
current congruence (this is the "E" in E-matching - matching up to the
equalities, not just syntactically), instantiate the quantifier with the
matching substitution and add the ground instance as a new clause. The match is
computed incrementally as the E-graph grows, using compiled "code trees" and an
inverted path index so that only newly created terms trigger fresh match
attempts.

Why E-matching is incomplete but practical. It can only ever instantiate with
terms that already occur (up to congruence) in the problem; if the proof
requires a term that the trigger machinery never constructs, the solver loops
or gives up without an answer. Choosing triggers is a black art: too
restrictive and needed instances are missed (incompleteness); too permissive
and the solver drowns in a "matching loop" where each instance creates terms
that trigger further instances without bound. Despite this, E-matching is fast,
incremental, and astonishingly effective on the verification-conditions that
SMT solvers see in practice, where the relevant instantiation terms almost
always do appear syntactically. This is why E-matching dominates in deployment
and why it is the default for quantified problems in both Z3 and cvc5.

##### 1.4.2 Model-Based Quantifier Instantiation (MBQI)

MBQI, due to Ge and de Moura, takes the opposite stance: instead of
syntactically guessing instantiation terms, it builds a candidate model of the
quantifier-free part plus the currently-chosen instances, then checks each
universally quantified formula against that model. If the model falsifies
forall x. P(x), there is some witness value for which P fails; MBQI extracts a
term denoting that witness and instantiates with it, refuting the bad model and
forcing the next round to build a better one. This is a model-refutation loop:

```
MBQI loop:
  build candidate model M of ground part + current instances
  for each quantified formula (forall x. P(x)):
      if M does not satisfy it:
          find value v in M with not P(v)
          add instance P(t_v) where t_v denotes v
  if no instance was added: return SAT (M is a real model)
```

The decisive difference from E-matching is that MBQI is model-driven rather than
term-driven: it can discover that a quantifier is violated even when no existing
syntactic term is a useful trigger, and crucially it can return SAT - it can
prove a quantified formula satisfiable by exhibiting a model, which pure
E-matching cannot. For decidable fragments (for example the
Bernays-Schoenfinkel/effectively-propositional class, or the array property
fragment) MBQI is a decision procedure. The cost is that model construction and
checking are expensive, and for theories where the model has infinite domains
the witness-term extraction can fail to terminate. In practice Z3 and cvc5 run
E-matching first for speed and fall back to MBQI for completeness, especially
to detect satisfiable (non-theorem) instances.

##### 1.4.3 Conflict-based instantiation

Reynolds, Tinelli, and de Moura ("Finding Conflicting Instances of Quantified
Formulas in SMT", FMCAD 2014) observed that one should prioritize
instantiations that immediately cause a conflict, i.e. an instance that is
directly inconsistent with the current ground model. Rather than enumerate many
instances and hope one helps, the solver searches for a substitution that makes
some quantified body false under the present assignment and adds just that one.
Conflict-based instantiation is run as a fast pre-filter before E-matching: it
makes far fewer, far more relevant instantiations, sharply reducing the number
of useless ground clauses. It is incomplete on its own but excellent as a first
line.

##### 1.4.4 Enumerative and syntax-guided instantiation

When E-matching stalls and MBQI is too costly, cvc5 falls back to enumerative
instantiation: systematically enumerate ground terms (of the right sort, in
some fair order by size) and try them as instantiations. Niemetz, Preiner, and
collaborators generalized this to syntax-guided quantifier instantiation
(SyQI), which reuses cvc5's syntax-guided synthesis (SyGuS) engine to enumerate
candidate instantiation terms in a counterexample-guided loop, applicable to
any background theory (Niemetz et al., "Syntax-Guided Quantifier
Instantiation", TACAS 2021,
https://pmc.ncbi.nlm.nih.gov/articles/PMC7984542/). Enumeration is complete in
the limit for the pure first-order case (it will eventually try every term),
which is part of how cvc5 has become competitive in CASC's first-order
divisions. The same counterexample-guided instantiation (CEGQI) machinery
underlies cvc5's program-synthesis abilities (Reynolds et al.,
"Counterexample-Guided Quantifier Instantiation for Synthesis in SMT",
CAV 2015, http://theory.stanford.edu/~barrett/pubs/RDK+15.pdf).

#### 1.5 Finite model finding

For a satisfiable quantified problem over uninterpreted functions, cvc5 can
search for a finite model: fix a candidate domain size k for each uninterpreted
sort, add cardinality constraints, and try to satisfy all the (now finitely
many) ground instances; if it fails, increase k. Reynolds et al. developed the
quantifier instantiation needed to make this exhaustive over a fixed finite
domain. Finite model finding is what lets an SMT solver answer "this is a
non-theorem" with a concrete countermodel, populating the CASC FNT (first-order
non-theorem) division and serving Sledgehammer's counterexample-finding mode.

#### 1.6 Why SMT wins where it wins, and where it lags

Strengths: native theory reasoning (a single arithmetic fact that a saturation
prover would have to derive by many resolution steps is a one-line theory
deduction); world-class Boolean search for problems with heavy case structure;
incremental solving for interactive use; model and proof production. This is why
SMT solvers are the default backends for software verification, and why
Sledgehammer ships both cvc5 and Z3 alongside E, Vampire, and SPASS, frequently
finding the proof first on goals with arithmetic or datatype content (see the
Sledgehammer manual, https://isabelle.in.tum.de/doc/sledgehammer.pdf).

Limitation: deep first-order reasoning. E-matching only ever instantiates with
terms that exist; problems whose proofs need to invent intermediate terms (the
bread and butter of superposition with its term-building inferences) are exactly
where SMT historically fell behind. The instantiation heuristics do not perform
the controlled, redundancy-pruned term construction that ordered resolution and
superposition do. The enumerative and syntax-guided methods narrow this gap, but
a saturation prover with good term orderings and literal selection remains the
stronger tool on hard pure-first-order problems with deep quantifier nesting.

---

### 2. Instantiation-based proving: iProver and Inst-Gen

#### 2.1 The core insight

iProver, by Konstantin Korovin (Manchester), is the flagship
instantiation-based prover. Its foundation is the Inst-Gen calculus, introduced
by Ganzinger and Korovin ("New Directions in Instantiation-Based Theorem
Proving", LICS 2003) and developed into a modular framework by Korovin
("Inst-Gen - A Modular Approach to Instantiation-Based Automated Reasoning",
2013, https://link.springer.com/chapter/10.1007/978-3-642-37651-1_10).

The insight bridges propositional SAT and first-order logic. By the
Herbrand/Skolem-Goedel theorem, a set of first-order clauses is unsatisfiable
if and only if some finite set of ground instances is propositionally
unsatisfiable. So in principle one could enumerate ground instances and call a
SAT solver - but blind enumeration is hopeless. Inst-Gen makes the SAT solver
guide which instances to generate, closing the loop:

> Take a ground abstraction of the first-order clause set, decide it with a SAT
> solver, and use the propositional model the SAT solver returns to compute,
> by unification, exactly the first-order instances that the abstraction got
> wrong - then refine the abstraction with those instances and repeat.

#### 2.2 The mechanism

The ground abstraction is built with a distinguished "bottom" substitution,
written perp, that maps every variable to a single fixed constant (call it
star). Given the first-order clause set S, form S.perp by replacing every
variable with star. S.perp is a set of ground clauses, hand it to a SAT solver.

```
Inst-Gen(S):
  loop:
    ground   = { C.perp : C in S }          # collapse all vars to star
    status, M = SAT(ground)
    if status == UNSAT:
        return UNSAT                          # S is first-order unsatisfiable
    # M is a propositional model of the abstraction.
    # M selects, in each clause C, a literal L that M makes true (the "selected
    # literal" of C under M).
    for each pair of clauses C1, C2 with selected literals L1, L2
        such that L1 and (complement of L2) unify with mgu sigma
        and sigma is "proper" (does more than perp would):
            add the instances C1.sigma and C2.sigma to S    # Inst-Gen inference
    if no new instance was added:
        return SAT                            # the model lifts to first-order
```

The Inst-Gen inference rule is: given two clauses whose selected literals
(chosen consistently with the propositional model) are complementary-unifiable
with most-general unifier sigma, generate the two instantiated clauses C1.sigma
and C2.sigma. The intuition: the propositional model believed L1 and (not L2)
could be simultaneously true under the crude perp-abstraction, but at the
first-order level they unify, so the abstraction was too coarse there; adding
the sigma-instances forces the next SAT call to confront that conflict.

Termination and completeness: if the SAT solver reports the ground abstraction
unsatisfiable, the original set is first-order unsatisfiable (soundness of the
abstraction direction is immediate). If no Inst-Gen inference applies - every
pair of selected literals is non-unifiable beyond perp - then the propositional
model can be lifted to a first-order model and the set is satisfiable. The
calculus is refutationally complete for first-order logic, with a redundancy
notion (dismatching constraints and other techniques) that prunes instances
already subsumed, keeping the generated set under control.

#### 2.3 Why this is attractive

Two engineering payoffs. First, the heavy combinatorial work - propagation,
conflict analysis, backjumping over a huge ground clause set - is delegated to a
mature SAT/SMT solver (iProver uses MiniSat-style and other backends, and can
use an SMT solver to handle theory atoms in the ground abstraction). The
first-order layer only ever does unification to decide which instances to
manufacture. Second, the method is naturally a decision procedure for the
effectively-propositional (EPR, Bernays-Schoenfinkel) fragment: with no function
symbols the set of ground instances is finite, so the loop terminates. iProver
is consistently among the strongest systems in the CASC EPR division for exactly
this reason.

#### 2.4 Modern iProver: combining calculi

Pure instantiation handles equality poorly. iProver-Eq (Korovin and Sticksel,
IJCAR 2010, https://www.sticksel.info/christoph/files/IJCAR2010-iProver-Eq.pdf)
extended Inst-Gen to Inst-Gen-Eq, building equational reasoning (a superposition-
like component) into the instance-selection step so that the ground solver
reasons modulo equality. More recently, iProver gained a full superposition
calculus running alongside Inst-Gen, with a flexible simplification setup that
subsumes the classic DISCOUNT and Otter saturation loops (Duarte and Korovin,
"Implementing Superposition in iProver", IJCAR 2020,
https://pmc.ncbi.nlm.nih.gov/articles/PMC7324033/). Modern iProver therefore
runs instantiation, resolution, and superposition cooperatively, sharing the
clause database and a portfolio scheduler, which is why it is competitive across
the FOF (first-order form) division and not only EPR. The lesson for
implementers: instantiation-based and saturation-based search are complementary,
and the strongest systems blend them rather than pick one.

---

### 3. Higher-order provers

#### 3.1 What changes in higher-order logic

Classical higher-order logic (HOL), or simple type theory with Henkin
semantics, allows quantification over functions and predicates, lambda
abstraction, and (typically) extensionality and a choice operator. Two features
make automated HOL proving qualitatively harder than first-order:

1. Higher-order unification is undecidable (Goldfarb, 1981), and even when
   unifiers exist there can be infinitely many incomparable ones. Huet's
   semi-decision procedure enumerates a complete set of "pre-unifiers" via
   alternating imitation and projection bindings on flexible-rigid pairs, but it
   may not terminate and branches profusely. No prover can simply "call unify"
   the way a first-order prover does.

2. Extensionality (two functions are equal iff they agree everywhere; two
   propositions are equal iff equivalent) and choice are needed for many natural
   theorems but are awkward to build into a calculus.

The design space is defined by how a prover copes with point (1). The main
strategies: restrict to the decidable pattern fragment (Miller's higher-order
patterns, where each free-variable application is to distinct bound variables -
there unification is unitary and decidable) and only run full Huet enumeration
lazily and bounded; or avoid higher-order unification entirely by translating to
first-order logic.

#### 3.2 Leo-III: cooperative higher-order paramodulation

Leo-III, by Alexander Steen and Christoph Benzmueller ("The Higher-Order Prover
Leo-III", IJCAR 2018, https://arxiv.org/abs/1802.02732; extended account in
"Extensional Higher-Order Paramodulation in Leo-III", 2019,
https://arxiv.org/abs/1907.11501), is a saturation prover whose core calculus is
extensional higher-order paramodulation - paramodulation (the equality-handling
generalization of resolution) lifted to HOL with primitive equality, plus
dedicated rules for extensionality and choice. Higher-order unification is
performed by a bounded Huet-style procedure, with the pattern fragment handled
deterministically and the general case explored under depth and breadth limits.

Leo-III's signature design choice is cooperation. During saturation it
periodically translates its current higher-order clause set into many-sorted
first-order logic (monomorphic or polymorphic) and dispatches it to external
first-order provers (E, Vampire, and others) and SMT solvers, running them as
asynchronous agents; if any of them refutes the first-order abstraction, that
refutation lifts to a higher-order proof. This is the descendant of LEO-II's
cooperation idea, but where LEO-II translated to untyped first-order logic,
Leo-III translates to typed first-order logic, which is sounder and tighter. The
prover accepts all common TPTP dialects including the typed higher-order form
THF and its polymorphic extension TH1, and emits checkable proof certificates.
The takeaway: a higher-order prover gets much of its power by offloading the
first-order-shaped sub-problems to specialists, reserving its own machinery for
the genuinely higher-order steps (unification, extensionality, choice).

#### 3.3 Satallax and Lash: tableaux plus SAT (abstract consistency)

Satallax, by Chad Brown ("Satallax: An Automatic Higher-Order Prover", IJCAR
2012, https://link.springer.com/chapter/10.1007/978-3-642-31365-3_11), takes a
completely different route grounded in the abstract-consistency / model-existence
theory of higher-order logic. Its calculus is a ground tableau calculus whose
rules use only shallow syntactic information about the formulas they fire on.
The proof-search engine generates propositional clauses that encode which
tableau rules have fired, and hands them to the SAT solver MiniSat. The flow:

```
Satallax loop:
  maintain a set of tableau "instructions" in a priority queue
  repeat:
      pop the highest-priority applicable tableau step
      apply it; this creates new formulas and, importantly,
        emits propositional clauses naming the literals/branches involved
      run MiniSat on the accumulated propositional clauses
      if MiniSat reports UNSAT: the branch is closed -> proof found
```

The deep idea is that the open branches of a higher-order tableau correspond to
a candidate model, and a complete set of abstract-consistency conditions
characterizes when such a branch can be extended to a genuine Henkin model.
Satallax's rules are exactly those conditions; SAT-solving manages the
exponential branch combinatorics. The priority queue (heavily tuned, and the
target of machine-learned internal guidance - Faerber and Brown, "Internal
Guidance for Satallax") decides which higher-order instantiations and
mating steps to try, including the bounded higher-order unification needed for
the mating/connection rules.

Lash, by Chad Brown and Cezary Kaliszyk ("Lash 1.0", IJCAR 2022,
https://arxiv.org/abs/2205.06640), is a from-scratch performance reengineering
of Satallax: it replaces the OCaml term representation with a C implementation
of normal terms with perfect sharing (hash-consing) and a C implementation of
normalizing (beta-eta) substitution. Because the ground tableau calculus is
dominated by term construction, normalization, and equality checks, the
perfect-sharing representation makes Lash substantially faster than Satallax on
the shared calculus, while keeping the same SAT-guided priority-queue search.
The same C-term infrastructure was later extended to dependently typed
higher-order logic. Lesson: in HOL, the term-representation layer (sharing,
normalization, fast alpha-equivalence) is itself a first-class performance
concern, often more decisive than the high-level calculus.

#### 3.4 agsyHOL

agsyHOL, by Fredrik Lindblad (2014), is a smaller higher-order prover built on a
focused sequent calculus driven by a generic narrowing engine (the same "agsy"
search engine used in Agda's auto tactic). It is a useful point of comparison:
it shows that a generic, backtracking narrowing search over a focused calculus
can be competitive on smaller higher-order problems without the heavy
SAT-cooperation or first-order-offloading machinery of Leo-III and Satallax,
though it does not scale as far.

#### 3.5 Embedding higher-order into first-order: the Sledgehammer route

A pragmatically dominant approach in the Isabelle/HOL world is not to build a
native higher-order prover at all but to encode the higher-order problem into
first-order logic and call a first-order prover. This is what Sledgehammer does.
The encoding has to handle three things:

- Applications: a curried higher-order application f x y is encoded with an
  explicit binary apply symbol, "app(app(f,x),y)" - the applicative or
  "explicit application" encoding - so that partial application and
  variables-in-function-position become ordinary first-order terms.
- Lambda abstractions: these cannot survive directly in first-order logic.
  Sledgehammer offers two translations and can choose between them per problem:
  lambda-lifting (replace each lambda by a fresh top-level function symbol
  defined by an equation, also called supercombinator lifting) and SK-style
  combinators (rewrite every lambda into a fixed combinator basis - S, K, I,
  B, C - so that no bound variables remain). See the Sledgehammer manual,
  https://isabelle.in.tum.de/doc/sledgehammer.pdf.
- Types: polymorphism and type information must be encoded soundly, since
  erasing types can introduce unsound merges of distinct types; Sledgehammer
  uses guard- or tag-based type encodings, and monomorphizes (instantiates
  polymorphic facts at a finite set of ground types) for provers that lack
  polymorphism.

The tradeoff is sound-but-clumsy: the apply/combinator encoding obscures the
higher-order structure from the first-order prover (a single beta-reduction
becomes many rewrite steps over app and combinators), so deep higher-order goals
suffer. This motivated the Matryoshka/Nekoka line of work on lambda-free and
lambda-superposition calculi (Bentkamp, Blanchette, et al.), which extend the
superposition machinery of E and Vampire to handle higher-order terms more
natively, narrowing the gap between the encode-and-call approach and dedicated
higher-order provers (see "Superposition for Lambda-Free Higher-Order Logic",
https://matryoshka-project.github.io/pubs/lfhosup_paper.pdf, and the
lambda-superposition work,
https://nekoka-project.github.io/pubs/sup-lam-sup-intro.pdf). Modern Zipperposition,
E, and Vampire now compete directly in the THF division on this basis.

---

### 4. Tableau and connection-method provers: leanCoP and nanoCoP

#### 4.1 The connection method

The connection (or matrix / mating) method, and the closely related connection
tableaux of Bibel and of Letz/Stenz, is a goal-directed proof procedure that
contrasts sharply with saturation. A saturation prover works bottom-up,
generating all non-redundant consequences of the whole clause set until it
derives the empty clause; it must manage a large and growing clause database. A
connection prover works top-down from the goal, building a single connected
proof object and never materializing a global clause set.

The central data structure is the matrix: the clause set viewed as a
two-dimensional array of literals (clauses are columns, literals are rows). A
connection is a pair of literals with opposite polarity that are unifiable
(after a substitution). A proof is found when there is a spanning set of
connections - a way to pick connections such that every path through the matrix
(every way of choosing one literal per clause) contains at least one connection.
Operationally this is realized as connection tableaux: starting from a goal
literal, repeatedly extend the current branch by connecting its leaf to a
complementary literal of some clause (extension step) or to a literal already on
the branch (reduction step), applying the unifier globally, until every branch
is closed by a connection.

```
prove(Goal, Path):
  # Path is the list of literals on the current open branch.
  for each clause C and literal L in C with L unifiable with complement of Goal:
      apply mgu
      # Extension: open the remaining literals of C as new subgoals,
      # each extending Path with Goal.
      if all (L' in C, L' != L) succeed via prove(L', [Goal | Path]):
          succeed
  # Reduction: close against an ancestor on the path.
  if some literal in Path is unifiable with complement of Goal:
      succeed
```

#### 4.2 Why connection tableaux are interesting: goal-directed and compact

Two properties stand out for an implementer.

Goal-directed. Every inference is forced by the current open subgoal, which
descends from the conjecture. The prover only ever works on literals
reachable from the goal, so it never wastes effort deriving consequences
irrelevant to the conjecture - the chronic failure mode of saturation on
axiom-rich problems. This makes the connection method attractive for
large-theory reasoning where most axioms are irrelevant.

Compact. Because there is no global clause database - just the current branch,
the path, and the accumulated substitution - the entire prover fits in a
handful of lines. Jens Otten's leanCoP is the canonical demonstration: a complete
connection prover for classical first-order logic in roughly a dozen lines of
Prolog, exploiting Prolog's own unification and backtracking as the proof engine
(Otten and Bibel, "leanCoP: Lean Connection-Based Theorem Proving"; see the
20-year retrospective, Otten,
https://ceur-ws.org/Vol-3613/AReCCa2023_paper1.pdf). leanCoP 2.0 adds the
practical optimizations that make it competitive: regularity (forbid a literal
to recur on its own branch), lemmata (reuse already-closed subgoals), and -
decisively - restricted backtracking, which cuts the Prolog backtracking once a
subgoal closes, trading completeness-in-practice for a large search-space
reduction.

nanoCoP (Otten) lifts the same idea to non-clausal form: instead of first
converting to clause normal form (CNF), which destroys the formula's structure
and can blow it up, nanoCoP works directly on a non-clausal matrix, preserving
the original nesting and producing more natural, often shorter proofs. The
nanoCoP 2.0 family extends both leanCoP and nanoCoP to intuitionistic and modal
logics by adding prefix-unification constraints to the connection condition.

The compactness has a downside: the connection method has no built-in equality
handling (equality must be axiomatized, which is weak), and the aggressive
restricted backtracking sacrifices the clean completeness guarantee. It is a
specialist tool, strongest on structured, equality-light, goal-directed
problems.

#### 4.3 Machine-learned descendants

The compact, deterministic-skeleton structure of leanCoP made it the natural
substrate for learning-guided proof search, since every choice point (which
clause to use for the extension step) is a clean decision to be predicted. The
descendants - rlCoP (reinforcement learning guiding Monte-Carlo Tree Search over
the connection tableau), plCoP (a Prolog reimplementation with policy/value
guidance), and related systems - replace leanCoP's hand-tuned ordering and
restricted backtracking with a learned policy and value function (Kaliszyk et
al., "Machine Learning Guidance for Connection Tableaux",
https://link.springer.com/article/10.1007/s10817-020-09576-7). These are covered
in depth in the section on machine-learning hammers and guidance; the point here
is only that the connection calculus's small, well-defined decision points are
what make it such a clean target for learned guidance.

---

### 5. The TPTP / CASC ecosystem

All of these systems are measured against a shared infrastructure.

The TPTP (Thousands of Problems for Theorem Provers) library, curated by Geoff
Sutcliffe, is the standard problem collection and, just as importantly, the
standard input language family. The dialects form a ladder of expressiveness:
CNF (clause normal form), FOF (first-order form, full first-order logic with
quantifiers and connectives), TFF (typed first-order form, monomorphic TFF0 and
polymorphic TFF1, including arithmetic), and THF (typed higher-order form, with
the polymorphic extension TH1). The TPTP also standardizes the SZS result
ontology (Theorem, CounterSatisfiable, Satisfiable, and so on) so that outcomes
are machine-comparable, and provides tooling for proof certificates.

CASC (the CADE ATP System Competition), also run by Sutcliffe, is the annual
"world championship" for fully automatic provers, held at the CADE and IJCAR
conferences (Sutcliffe, "The CADE ATP System Competition - CASC", AI Magazine
2016, https://onlinelibrary.wiley.com/doi/full/10.1609/aimag.v37i2.2620; recent
edition CASC-J12, https://journals.sagepub.com/doi/10.1177/30504554241305110).
It is organized into divisions matching the TPTP dialects and problem character:

- FOF: first-order form theorems (the central, most-watched division).
- CNF: clause-normal-form unsatisfiability.
- FNT: first-order non-theorems (find a model / countermodel - the model-finding
  and finite-model-finding division).
- EPR: effectively propositional (Bernays-Schoenfinkel) clause sets, where
  iProver excels.
- UEQ: unit equality (pure equational logic, the home turf of completion-based
  and superposition provers such as Waldmeister and E).
- TFA: typed first-order with arithmetic, where SMT solvers (cvc5, Z3) and
  arithmetic-aware saturation provers compete.
- THF: typed higher-order form, contested by Leo-III, Lash, Satallax, agsyHOL,
  and the higher-order modes of Zipperposition, E, and Vampire.
- LTB: large-theory batch - many related problems sharing a huge axiom set,
  where premise selection and learned guidance matter most.

Problems are drawn from the current TPTP release, with fresh and previously
unseen problems used to discourage overfitting, under a fixed per-problem CPU
time limit (commonly 180 seconds). Winning a division means solving the most
problems within the limit (with proof/model output required for soundness
checking). For an implementer, CASC standings are the de facto signal of which
architecture is currently strongest on which class of problem: Vampire and E
lead FOF; iProver leads EPR; Waldmeister/E lead UEQ; cvc5 and Z3 lead the
arithmetic divisions; and the higher-order field is split between native provers
(Leo-III, Lash) and the increasingly strong superposition-based THF modes.

---

### 6. Summary for implementers

- SMT solvers (cvc5, Z3) layer theory solvers under a CDCL SAT engine via
  CDCL(T), combine theories with Nelson-Oppen equality sharing over a shared
  E-graph, and handle quantifiers by instantiation. E-matching is fast,
  incremental, and incomplete (it only instantiates with existing terms and can
  loop); MBQI is model-driven, can return SAT, and is a decision procedure on
  several fragments but is costly; conflict-based instantiation is a sharp
  pre-filter; enumerative/syntax-guided instantiation restores completeness in
  the limit. They win on theory-heavy and case-heavy problems and lag on deep
  pure-first-order reasoning that requires inventing terms.

- iProver's Inst-Gen uses a ground SAT/SMT abstraction (collapse all variables
  to one constant via the bottom substitution) and lets the propositional model
  guide which first-order instances to manufacture by unifying selected
  literals. It is a decision procedure for EPR and now blends instantiation with
  resolution and superposition.

- Higher-order provers must cope with undecidable higher-order unification.
  Leo-III uses bounded Huet-style unification with extensional paramodulation and
  cooperates with external first-order/SMT provers; Satallax and its faster
  C-reengineered successor Lash use a SAT-guided ground tableau calculus rooted
  in abstract-consistency theory; agsyHOL uses focused-sequent narrowing. The
  alternative is to encode higher-order logic into first-order logic
  (applicative encoding plus lambda-lifting or combinators, with sound type
  encodings and monomorphization), as Sledgehammer does, at the cost of
  obscuring higher-order structure - which the lambda-free and lambda-superposition
  calculi now mitigate.

- Connection-method provers (leanCoP, nanoCoP) are goal-directed and extremely
  compact: a complete classical prover in a dozen lines of Prolog, with no
  global clause set, optimized by regularity, lemmata, and restricted
  backtracking, and serving as the clean substrate for learning-guided
  descendants (rlCoP, plCoP).

- TPTP and CASC provide the shared problem library, input languages
  (CNF/FOF/TFF/THF), result ontology, and competition divisions that let all of
  these architectures be compared head to head.

### Sources

- de Moura and Bjorner, "Z3: An Efficient SMT Solver", TACAS 2008. https://link.springer.com/content/pdf/10.1007/978-3-540-78800-3_24.pdf
- de Moura and Bjorner, "Efficient E-matching for SMT Solvers", CADE 2007. https://leodemoura.github.io/files/ematching.pdf
- Barbosa et al., "cvc5: A Versatile and Industrial-Strength SMT Solver", TACAS 2022. https://hanielbarbosa.com/papers/tacas2022.pdf
- Niemetz, Preiner, et al., "Syntax-Guided Quantifier Instantiation", TACAS 2021. https://pmc.ncbi.nlm.nih.gov/articles/PMC7984542/
- Reynolds et al., "Counterexample-Guided Quantifier Instantiation for Synthesis in SMT", CAV 2015. http://theory.stanford.edu/~barrett/pubs/RDK+15.pdf
- Korovin, "Inst-Gen - A Modular Approach to Instantiation-Based Automated Reasoning", 2013. https://link.springer.com/chapter/10.1007/978-3-642-37651-1_10
- Korovin and Sticksel, "iProver-Eq: An Instantiation-Based Theorem Prover with Equality", IJCAR 2010. https://www.sticksel.info/christoph/files/IJCAR2010-iProver-Eq.pdf
- Duarte and Korovin, "Implementing Superposition in iProver", IJCAR 2020. https://pmc.ncbi.nlm.nih.gov/articles/PMC7324033/
- Steen and Benzmueller, "The Higher-Order Prover Leo-III", IJCAR 2018. https://arxiv.org/abs/1802.02732
- Steen and Benzmueller, "Extensional Higher-Order Paramodulation in Leo-III", 2019. https://arxiv.org/abs/1907.11501
- Brown, "Satallax: An Automatic Higher-Order Prover", IJCAR 2012. https://link.springer.com/chapter/10.1007/978-3-642-31365-3_11
- Brown and Kaliszyk, "Lash 1.0 (System Description)", IJCAR 2022. https://arxiv.org/abs/2205.06640
- Otten, "20 Years of leanCoP", AReCCa 2023. https://ceur-ws.org/Vol-3613/AReCCa2023_paper1.pdf
- Kaliszyk, Urban, et al., "Machine Learning Guidance for Connection Tableaux", JAR 2021. https://link.springer.com/article/10.1007/s10817-020-09576-7
- Bentkamp, Blanchette, et al., "Superposition for Lambda-Free Higher-Order Logic". https://matryoshka-project.github.io/pubs/lfhosup_paper.pdf
- Blanchette et al., Sledgehammer manual. https://isabelle.in.tum.de/doc/sledgehammer.pdf
- Sutcliffe, "The CADE ATP System Competition - CASC", AI Magazine 2016. https://onlinelibrary.wiley.com/doi/full/10.1609/aimag.v37i2.2620
- Sutcliffe, "The 12th IJCAR ATP System Competition - CASC-J12", 2025. https://journals.sagepub.com/doi/10.1177/30504554241305110

---


## Hammers, Premise Selection, and Learning-Guided Proving

This section surveys the machinery that connects interactive proof assistants (ITPs,
the systems where humans write proofs - Isabelle, Coq/Rocq, Lean, HOL4) to fully
automatic theorem provers (ATPs, the systems that search for proofs unattended -
E, Vampire, cvc5, Z3), and the layer of machine learning (ML) that increasingly
sits on top of and inside both. The audience is people who implement ATPs and people
who wire them into proof assistants.

The single most important idea in this whole area can be stated up front:

> The high-leverage, learnable decision points in automated reasoning are
> (1) *premise selection* - which of the tens of thousands of library facts to
> even hand to the prover, and (2) *clause or tactic selection* - which of the
> exploding set of derived facts (or available proof steps) to work on next.
> Everything else (the calculus, the unification engine, the kernel checker)
> is comparatively fixed. Learning is most productive when aimed at these two
> branching points, and most productive of all when it learns from the proofs
> it has itself found - the *expert-iteration* loop: prove, harvest the proofs
> as training data, retrain, prove more.

Keep that frame in mind; the rest of this document is mostly an elaboration of it
across different systems.

---

### 1. What a "hammer" is

A *hammer* is a single push-button tactic in a proof assistant that, given the
current goal and the surrounding library, tries to discharge the goal completely
and automatically by delegating to external automatic provers. The name comes
from Isabelle's `sledgehammer`. The canonical hammer pipeline has four stages,
and every hammer (Isabelle, Coq/Rocq, Lean, HOL4, Mizar) is some variation on it:

```
   GOAL + huge library
        |
        v
  (1) PREMISE SELECTION   pick ~hundreds of likely-relevant facts
        |                  out of tens of thousands
        v
  (2) TRANSLATION/ENCODING  rewrite the higher-order, polymorphic,
        |                   dependently-typed goal+premises into the
        |                   first-order (FOL) or SMT logic an external
        v                   prover can consume
  (3) EXTERNAL DISPATCH    run E / Vampire / SPASS / cvc5 / Z3 /
        |                  Zipperposition in parallel, with time limits
        v
  (4) RECONSTRUCTION       take the external prover's answer (often just
                           "unsat" plus an unsat core of premise names) and
                           rebuild a proof the ITP kernel will actually check
```

The reason all four stages exist, rather than just "call a prover", is twofold.
First, ATPs do not scale to libraries: handing E all of Mathlib or all of the
Isabelle Archive of Formal Proofs (AFP) is hopeless, so stage (1) is mandatory.
Second, ATPs do not speak the ITP's logic and are not trusted, so stages (2) and
(4) bracket the untrusted external search with a sound encoding going out and a
kernel-checked reconstruction coming back. The hammer therefore *trusts nothing*
from the external prover except a hint: the names of the premises it used.

---

### 2. Sledgehammer (Isabelle/HOL): the canonical hammer

Sledgehammer, by Lawrence Paulson, Jasmin Blanchette, Sascha Boehme, Tobias
Nipkow and many others (first released around 2007 and continuously developed
since), is the system every other hammer is modeled on. Walk the four stages.

#### 2.1 Premise selection: MePo and MaSh

Isabelle/HOL libraries hold hundreds of thousands of facts. Sledgehammer must
pick a few hundred before translation. It has two relevance filters that it can
combine.

**MePo - the Meng-Paulson relevance filter** (Jia Meng and Lawrence Paulson,
2009) is a *symbolic, iterative* filter. The idea is purely about shared
symbols. Start from the symbols appearing in the goal. Score each candidate fact
by how many of its symbols are already "relevant", normalized by the fact's size
(so that huge facts with one shared symbol do not flood in). Take the
highest-scoring facts, add their symbols to the relevant set, and iterate. This
is a transitive-closure-with-decay process: facts directly about goal symbols
come in first, then facts about *those* symbols, with the relevance weight
decaying each round. MePo needs no training data and is robust, but it is blind
to the statistical fact that some lemmas are simply used far more often than
others in successful proofs.

**MaSh - Machine Learning for Sledgehammer** (Daniel Kuehlwein, Blanchette,
Cezary Kaliszyk, Josef Urban, 2013) is the *learned* filter. It treats premise
selection as a multilabel classification / ranking problem. Each previously
proved fact is a training example: its features are the symbols and term
patterns occurring in its statement, and its labels are the premises that were
actually used to prove it. MaSh's default learner is a *sparse naive Bayes*
classifier (cheap, incremental, handles the enormous sparse feature space well),
with a *k-nearest-neighbors* (k-NN) alternative that ranks a candidate by how
often it was used to prove the goal's nearest neighbors in feature space. MaSh
updates online as the user proves new theorems, so it adapts to the local
development. In practice Sledgehammer uses **MeSh**, a weighted combination of
MePo and MaSh, getting the symbol-grounded robustness of one and the
usage-statistics of the other.

The deep point: premise selection is the first place ML entered the hammer, and
it is a near-ideal ML target. The decision (include/rank a fact) is cheap to
make, there is abundant supervised data (every existing proof is a labeled
example of "these premises sufficed for this goal"), and a wrong answer is only
mildly costly (you waste some prover time, you do not produce an unsound proof).

#### 2.2 Translation and encoding: from HOL to first-order logic

Isabelle/HOL is higher-order (quantification over functions and predicates) and
has rank-1 ML-style polymorphism. Most fast ATPs historically wanted
*untyped* or *many-sorted first-order* clauses. Bridging that gap soundly and
without blow-up is the deep technical art of the translation stage.

- **Lambda-lifting / combinators** remove higher-order features: lambda
  abstractions are either lifted to fresh named functions or compiled to
  SK-style combinators, and partial applications go through an explicit `app`
  operator. This yields applicative first-order terms.

- **Monomorphization** instantiates polymorphic facts at the (finitely many)
  ground types that actually appear, turning polymorphic HOL into a
  many-sorted or monomorphic problem. It is heuristic and bounded (you cannot
  enumerate all instances), but it produces small, clean problems when it works.

- **Type encodings - "guards versus tags"**, the work of Blanchette, Boehme,
  Andrei Popescu, and Nicholas Smallbone ("Encoding Monomorphic and Polymorphic
  Types", 2013-2016). The problem: if you simply erase all type information to
  reach untyped FOL, you can become *unsound*, because the untyped prover may
  unify terms of incompatible types. The two classic fixes are:
  - **Guards**: add type predicates as extra hypotheses, e.g. a clause says
    "if `x` is of type `nat` then ...". Guards constrain unification by adding
    literals.
  - **Tags**: wrap terms in type-tagging functions, e.g. `tag(nat, x)`, so the
    type travels with the term.
  Both are sound but bulky and slow the prover down. Their key insight was
  *monotonicity*: a sort is *monotone* if its models can always be extended with
  fresh elements, and for monotone sorts the type information can be *safely
  erased* with no guards or tags at all. By statically detecting monotone sorts,
  the encoding decorates only the few problematic sorts, producing translations
  that are simultaneously sound, complete, and far lighter than full guards or
  tags. The "cover"-based schemes extend this to polymorphism. This line of work
  is why modern Sledgehammer is both correct and competitive: the encoding is
  not an afterthought, it is a measured trade-off between soundness overhead and
  prover speed.

#### 2.3 Dispatching to external provers

Sledgehammer runs *several backends in parallel* with short time limits (a few
to tens of seconds each), because different provers win on different problems and
the marginal cost of one more parallel prover is low. The roster includes the
superposition / resolution first-order provers **E** (Stephan Schulz),
**Vampire** (Andrei Voronkov, Giles Reger, Martin Suda, Laura Kovacs), **SPASS**
(historically), and the higher-order superposition prover **Zipperposition**
(Simon Cruanes, Petar Vukmirovic, et al.), plus the SMT solvers **cvc5** and
**Z3**. SMT solvers get a different encoding (into SMT-LIB with theories like
linear arithmetic) and are strong on goals with arithmetic and large but shallow
case splits, where saturation provers are strong on deep equational and
quantifier reasoning. Running both families covers more ground.

#### 2.4 Reconstruction: why you cannot trust the external prover

The external prover is *not* part of Isabelle's trusted kernel. It may have bugs;
its proof object, if any, is in a foreign format. Sledgehammer therefore never
accepts the external "yes" directly. It takes the *unsat core* - the small set of
premises the prover actually used - and re-proves the goal *inside Isabelle* from
just those premises. Reconstruction options, roughly in order of preference:

- **The one-line `metis` call.** `metis` is an internal, kernel-checked
  resolution prover (a port of Joe Hurd's Metis). Sledgehammer emits
  `by (metis fact1 fact2 ...)`. Because `metis` only has to redo a search that
  is now known to succeed from a *handful* of facts, it almost always closes it
  fast, and every inference goes through the Isabelle kernel, so the result is
  trustworthy.
- **`smt`** reconstruction, which replays a Z3/cvc5 proof term through certified
  Isabelle inferences (used when `metis` cannot reconstruct an arithmetic-heavy
  proof).
- **Other tactics** (`simp`, `auto`, `blast`, `meson`) when they suffice.
- **Structured Isar proofs.** Sledgehammer can render a multi-step, human-readable
  `proof ... qed` skeleton in Isabelle's Isar language, useful when the one-liner
  is fragile or slow, and pedagogically nicer because the intermediate facts are
  visible.

Reconstruction is what makes the hammer *safe to ship*: the external provers are
an oracle for *finding* a proof, never for *certifying* one. This separation -
untrusted search, trusted check - is the architectural invariant every hammer
preserves.

---

### 3. CoqHammer (Coq/Rocq)

CoqHammer, by Lukasz Czajka and Cezary Kaliszyk (University of Innsbruck, system
paper 2018, "Hammer for Coq"), brought the Sledgehammer architecture to Coq (now
Rocq), whose logic is the Calculus of Inductive Constructions (CIC) - dependent
types, a much richer and harder source logic than HOL.

- **Premise selection.** Same idea as MaSh: learn from the existing library which
  lemmas tend to be used, with feature-based ML (naive Bayes / k-NN style
  predictors over symbol and term features) ranking premises for the goal.

- **Translation.** The hard part. CoqHammer defines a translation from CIC
  (with Coq's extensions) to *untyped* first-order logic. Dependent types,
  type-class instances, and proof-irrelevant `Prop` content all have to be
  approximated. The translation is deliberately *sound but only "sufficiently"
  complete*: it does not capture all of CIC (that is impossible to do
  efficiently), but it captures enough that the external ATPs solve a large
  fraction of real goals. It then dispatches to E, Vampire, cvc5, Z3 as usual.

- **Reconstruction via `sauto` / `hammer` / `crush`-style tactics.** This is where
  CoqHammer differs most from Sledgehammer. Coq has no `metis`. Instead Czajka
  built a proof-search procedure based on *proof search by type inhabitation*
  ("Practical proof search for Coq by type inhabitation"), shipped as the
  `sauto` tactic family (with tuned variants often nicknamed `crush`, `qauto`,
  `hauto`, etc.). Given the small premise set the external prover reported,
  `sauto` runs an `eauto`-style goal-directed search augmented with limited
  rewriting, congruence closure, and forward reasoning, and reconstructs a
  genuine Coq proof term that the Coq kernel checks. So again: external prover
  finds the relevant facts, internal trusted tactic rebuilds the proof.

CoqHammer is a shipped, widely used tool. Its existence is the proof of concept
that the hammer architecture survives the jump to full dependent type theory,
which directly motivated the Lean effort below.

---

### 4. Lean hammers: Duper, lean-auto, LeanHammer

Lean 4's logic is also dependent type theory (a variant of CIC). Until recently
Lean had strong *interactive* automation (`simp`, `omega`, `decide`, the
`aesop` proof-search tactic by Jannis Limperg and Asta Halkjaer From) and strong
*neural* provers (below), but no classical Sledgehammer-style hammer. As of
2024-2025 that gap has been closed by three interlocking projects. Be precise
about what each is.

#### 4.1 Duper - a superposition prover written *in* Lean

**Duper** (Joshua Clune, Yicheng Qian, Alexander Bentkamp, Jeremy Avigad; ITP
2024 system paper "Duper: A Proof-Producing Superposition Theorem Prover for
Dependent Type Theory") is a saturation-based ATP implemented *inside Lean
itself*. It runs the superposition calculus over dependently-typed terms,
performing first-order and some higher-order reasoning (it descends from the
higher-order superposition lineage of Zipperposition / the Lambda-superposition
work of Bentkamp, Blanchette, et al.).

The crucial property is that Duper is **proof-producing**: when it saturates and
finds a contradiction, it emits a Lean *proof term* that Lean's kernel checks.
This sidesteps the foreign-proof-format reconstruction headache - Duper *is*
native, so its output is natively trusted. Duper can be called standalone as a
terminal tactic (`duper [facts]`), and it was explicitly designed to serve as the
reconstruction backend of a future Lean hammer. Think of Duper as Lean's analogue
of `metis`, except it is a full modern superposition prover rather than a
minimal resolution checker.

#### 4.2 lean-auto - the translation / interface framework

**lean-auto** (primarily Yicheng Qian, with Clune, Avigad and others; paper
"Lean-auto: An Interface between Lean 4 and Automated Theorem Provers",
arXiv 2505.14929, 2025) is the translation layer - Lean's stage (2). It
implements a translation algorithm from Lean 4's dependent type theory to the
logics of external ATPs (monomorphic higher-order and first-order targets,
TPTP/SMT-LIB-style outputs), with a *soundness guarantee* on the main translation
procedure. It can feed external provers and, for backends that produce checkable
output, reconstruct and re-check proof terms in the Lean kernel; for backends
without proof output it can mark the goal solved but emits a warning that it is
*trusting* the ATP (the honest, explicit version of the trust boundary). lean-auto
is the general-purpose Lean-to-ATP interface, the thing that makes "call E or
Zipperposition from Lean" possible at all.

#### 4.3 LeanHammer - the end-to-end hammer

**LeanHammer** (Thomas Zhu, Joshua Clune, Jeremy Avigad, Albert Qiaochu Jiang,
Sean Welleck; "Premise Selection for a Lean Hammer", arXiv 2506.07477, 2025) is
the first end-to-end, domain-general hammer for Lean. It composes the pieces:

```
  Lean goal
    -> LeanPremise          (neural premise selection, below)
    -> lean-auto            (translate selected premises + goal to ATP logic)
    -> Zipperposition       (external superposition proof search)
    -> Duper                (reconstruct: given just the premises Zipperposition
    |                        used, rebuild a kernel-checked Lean proof term)
    -> aesop                (proof search glue / fallback)
```

Two reported facts worth quoting. First, the premise selector, **LeanPremise**,
is a *neural retrieval* model trained specifically for hammer use in dependent
type theory; it dynamically adapts to user-local context (it can recommend
premises from the user's own project and new lemmas it never trained on), and it
solves about **21% more goals than existing Lean premise selectors**, generalizing
to projects it was not trained on (evaluated on miniCTX-v2). Second, the authors
report that it is *rare* for Zipperposition to find a proof that Duper then fails
to reconstruct - i.e. the reconstruction stage, the historical weak point, is in
practice reliable here because both sides reason in compatible (super)position
calculi.

Status, stated honestly: Duper and lean-auto are usable, released tools.
LeanHammer is a recent research system (2025) that is publicly available and
demonstrably works on Mathlib and out-of-distribution projects, but it is newer
and less battle-hardened than Sledgehammer's decade-plus of deployment. It is the
real "Sledgehammer from Lean", finally.

#### 4.4 Premise selection for Mathlib, more broadly

Independently of LeanHammer, premise selection for Lean/Mathlib has been driven by
the neural-prover community:

- **LeanDojo / ReProver** (Kaiyu Yang, Aidan Swope, Alex Gu, Rahul Chalamala,
  ..., Anima Anandkumar; NeurIPS 2023 Datasets and Benchmarks). LeanDojo is the
  tooling+data platform: it does program analysis on Lean to extract a benchmark
  of 98,734 theorems with their proofs *and* the set of accessible premises and
  hard-negative premises at each proof state. **ReProver** is the model: an
  encoder-decoder transformer that, at each tactic step, *retrieves* relevant
  premises from the library (dense retrieval over premise embeddings) and
  concatenates them with the goal state to generate the next tactic. The
  retrieval is what lets a modest model beat non-retrieval baselines and even
  GPT-4 on their hard split, which is constructed to require generalizing to
  *novel premises never seen in training* - exactly the premise-selection
  challenge.

These models, the LeanPremise retriever, and the classical MePo/MaSh-style
selectors are all attacking the same stage-(1) problem with progressively
heavier machinery (symbol overlap -> sparse Bayes/k-NN -> dense neural retrieval).

---

### 5. Premise selection as a topic in its own right

Strip away the surrounding hammer and premise selection is a clean, important
problem:

> Given a conjecture `C` and a library `L` of |L| facts (|L| in the tens or
> hundreds of thousands), output a small ranked subset `S` subset `L`
> (typically dozens to a few hundred) such that an ATP can prove `C` from `S`
> within a time budget.

Why it is the highest-leverage learnable decision: ATP search cost grows steeply
(often super-linearly) with the number of axioms, because each irrelevant fact
multiplies the branching of the search. Halving the premise set can turn an
unsolvable problem into a one-second one. And the supervision is free and
abundant: every proof in the library is a positive example mapping a goal to the
premises that sufficed.

Approaches, in increasing sophistication:

- **Symbol-based / syntactic.**
  - **SInE** (the SUMO Inference Engine selection, Krystof Hoder and Andrei
    Voronkov) builds a "trigger" relation: each axiom is triggered by its
    least-common symbol, and selection follows trigger chains from the goal.
    Cheap, no learning, ships inside E and Vampire.
  - **MePo** (above): iterative symbol-overlap with size normalization and decay.

- **Classical ML over hand-engineered features.** Features are typically the
  symbols, subterms, and term-walk patterns of a fact's statement.
  - **Naive Bayes** (MaSh default): rank a candidate premise by the posterior
    probability that it is relevant given the goal's features. Cheap, sparse,
    incremental.
  - **k-nearest-neighbors** (MaSh alternative): find the goals most similar to
    `C` and recommend the premises that proved them.
  - **Gradient-boosted decision trees** (XGBoost), used heavily in the
    Mizar / MPTP and ENIGMA lines, often outperform Bayes/k-NN when given good
    features and enough data.

- **Deep learning.**
  - **DeepMath** (Alex A. Alemi, Francois Chollet, Niklas Een, Geoffrey Irving,
    Christian Szegedy, Josef Urban, 2016) was the first demonstration that
    convolutional and recurrent neural networks could do premise selection on
    Mizar at scale, learning features instead of hand-engineering them.
  - **Graph neural networks (GNNs)** embed the *term/formula graph* (sharing
    subterms, respecting structure) and are *symbol-independent* - they do not
    overfit to particular symbol names, so they transfer across libraries.
  - **Transformer retrievers** (ReProver, LeanPremise): encode goal and premise
    statements into a shared vector space and rank by similarity, with hard
    negatives mined from program analysis. These dominate current Lean work.

A practical note for integrators: the metric that matters is not
recall-at-fixed-k in isolation but *end-to-end goals proved* under a time budget,
because the ATP and the selector interact. A selector that ranks the one crucial
lemma at position 200 is useless if you only pass 100 premises. Tune the selector
and the premise-count cutoff together against actual ATP success.

---

### 6. ML-guided internal proof search

Premise selection guides what goes *into* the prover. The second high-leverage
decision point lives *inside* the prover's main loop. Two paradigms:
clause selection (saturation provers) and tactic selection (tactic-based ITPs and
neural provers).

#### 6.1 The given-clause loop and why clause selection is the bottleneck

Saturation provers (E, Vampire) run the *given-clause loop*:

```
  unprocessed := initial clauses
  processed   := empty
  loop:
    g := SELECT the "best" clause from unprocessed     <-- the learnable decision
    move g to processed
    for each clause c derivable from g and processed:   (resolution, superposition...)
       simplify c; if c is the empty clause -> PROVED
       add c to unprocessed
```

The set of derivable clauses explodes combinatorially. Which clause you pick as
`g` next determines whether you reach the empty clause in a second or never. The
default selection is a hand-tuned weighting (clause age, symbol weight,
literal count). This is precisely the place to insert a learned heuristic:
the action space is "rank the clauses in `unprocessed`", and a good ranker can be
orders of magnitude faster than the hand-tuned default.

#### 6.2 ENIGMA - learned clause selection in E

**ENIGMA** (Efficient Learning-based Inference Guiding Machine; Jan Jakubuv and
Josef Urban, 2017 onward) learns the clause-selection function for the E prover.
The training signal: run E on many problems, and label each clause that appeared
in a *found proof* as positive, the rest as negative. Then train a classifier to
predict, from a clause's features, whether it is "proof-like".

The evolution of ENIGMA is itself a lesson in the speed/accuracy trade-off that
governs internal guidance (the model is queried millions of times per proof, so
*inference latency is part of the algorithm*):

- **ENIGMA (2017):** fast feature-based linear classifier on hashed
  symbol/term features - cheap enough to call on every generated clause.
- **ENIGMA-NG (2019):** gradient-boosted trees (and early neural nets) for
  better accuracy at acceptable cost.
- **ENIGMA Anonymous (2020):** *symbol-independent* graph neural networks that
  embed terms and clauses without depending on symbol names, so the model
  transfers to problems with unseen vocabulary. A "context"-based mode evaluates
  clauses in *batches* against the set of already-selected clauses, letting the
  GNN amortize its cost and judge a candidate relative to what is already in play.

ENIGMA is the canonical demonstration that learned guidance, used with
expert-iteration (feed the newly found proofs back as training data and loop),
substantially beats hand-tuned clause selection in a state-of-the-art saturation
prover.

#### 6.3 Deepire - learned clause selection in Vampire

**Deepire** (Martin Suda, Czech Technical University in Prague, ~2021) attacks the
same clause-selection decision in Vampire, with a striking design choice:
Deepire 1.0 *ignores the logical content of the clause entirely* and judges a
clause only by its *derivation history* - the tree of inferences that produced
it. It unfolds a *recursive neural network (RvNN)* along that derivation tree,
embedding each node into a vector, and a small multilayer perceptron (MLP) turns
the clause's embedding into a quality score. The bet - which paid off - is that
*how a clause was derived* is a strong predictor of whether it will be useful,
and history is far cheaper to featurize than re-reading term structure.
**Deepire II** (Suda, 2025) adds a GNN for name-invariant processing of the input
clause-normal-form plus *two* RvNNs (one over derivation history, one over term
structure), trained in a reinforcement-learning-flavored setup on TPTP, reporting
roughly 20% more theorems proved than the baseline strategy.

#### 6.4 rlCoP / plCoP / lazyCoP - AlphaZero for the connection tableau

A different prover family: the *connection tableau / connection calculus* (leanCoP
lineage). Here the proof search is naturally a *sequence of choices* - at each
open branch you pick which clause to connect with - which makes it a clean
*Markov decision process*, hence a natural fit for reinforcement learning (RL)
and Monte Carlo Tree Search (MCTS).

- **rlCoP** (Cezary Kaliszyk, Josef Urban, Henryk Michalewski, Miroslav Olsak,
  NeurIPS 2018, "Reinforcement Learning of Theorem Proving") replaces leanCoP's
  hand-written heuristics with learned *policy* and *value* functions and an
  MCTS over the tableau, trained AlphaZero-style: self-play proof attempts
  generate data, the policy/value nets improve, and the improved nets find more
  proofs. This is the explicit "AlphaZero for theorem proving" line. (The earlier
  **FEMaLeCoP**, Kaliszyk and Urban 2015, was the supervised precursor.)
- **plCoP** (Zsolt Zombori et al.) reimplements the rlCoP learning-guided MCTS
  on top of a compact Prolog leanCoP, making it easier to extend.
- **lazyCoP** (Michael Rawson, Giles Reger) is built on *lazy paramodulation*
  (adding equality reasoning to the connection calculus) and is engineered so the
  learned guidance is *insulated from the inference loop's overhead*, allowing
  even deep networks with no measurable slowdown in raw inference rate - directly
  addressing the latency problem that constrains ENIGMA-style guidance.

The common thread: the connection calculus turns proving into a game tree, and
the AlphaZero recipe (MCTS + learned policy/value + self-play expert iteration)
applies almost verbatim.

#### 6.5 TacticToe - learned tactic selection in HOL4

**TacticToe** (Thibault Gauthier, Cezary Kaliszyk, Josef Urban, et al.) moves the
learning up to the *tactic* level in HOL4. Instead of guiding clauses, it learns
which *ITP tactic* (with which arguments) to apply next, from a database of human
tactic proofs, and runs an MCTS-like search over tactic sequences, predicting both
the tactic and its arguments by similarity to recorded proof states. It needs no
external ATP and no translation: it plays the proof assistant's own tactic
language. It is the spiritual ancestor of the neural tactic provers below.

#### 6.6 The neural proof-generation line: GPT-f, HTPS

The most recent paradigm replaces feature engineering and small models with large
language models (LLMs) that *generate* the next proof step as text.

- **GPT-f** (Stanislas Polu and Ilya Sutskever, 2020, "Generative Language
  Modeling for Automated Theorem Proving") trains a transformer on Metamath proof
  steps and runs a best-first search where the model proposes candidate next steps
  and a value model ranks them. Crucially it demonstrated *expert iteration* at
  LLM scale: train on proofs, prove new theorems with search, add the newly found
  proofs to the training set, retrain, repeat - and it kept improving, including
  on a curriculum of progressively harder statements. This is the same
  prove-learn-prove loop as rlCoP and ENIGMA, now with a generative transformer as
  the policy.

- **HyperTree Proof Search (HTPS)** (Guillaume Lample, Timothee Lacroix, et al.,
  Meta AI, NeurIPS 2022) generalizes best-first search to a proper AND/OR
  hypergraph search (a tactic can split a goal into several subgoals that must
  *all* be proved - an AND node - so a plain tree is wrong). HTPS adapts MCTS to
  this hypertree: it expands nodes with an LLM tactic generator, backs up values
  through the AND/OR structure, and trains online on its own search traces. On
  Metamath it lifted the state of the art from GPT-f's 56.5% to 65.4% (and to
  82.6% with online training), and on Lean's miniF2F-curriculum from 31% to 42%.
  HTPS is the clearest statement that *search structure matters*: getting the
  AND/OR backup right, plus online expert iteration, is what beat the prior LLM
  prover.

These connect back to the central thesis. GPT-f and HTPS make *tactic selection*
the learnable bottleneck (the generative analogue of clause selection), and both
live or die by the expert-iteration loop. Neural autoformalization (translating
informal math to formal statements, so the prover has more to train and prove on)
is the data-supply side of the same flywheel.

---

### 7. Synthesis: the two decision points and the loop

Pulling the threads together for an implementer:

**The learnable decision points.**

1. *Premise selection* (stage 1 of the hammer; external to the prover). Pick the
   few relevant facts from a huge library. Cheap decision, abundant supervision,
   low cost of error. Techniques scale from SInE/MePo (symbol overlap) through
   MaSh (naive Bayes, k-NN) and gradient-boosted trees to GNN and transformer
   retrievers (ReProver, LeanPremise). This is where you get the biggest win for
   the least risk, because it cannot make a proof unsound - it only changes what
   the prover sees.

2. *Clause / tactic selection* (inside the prover's main loop). Pick the next
   clause to process (ENIGMA in E, Deepire in Vampire, rlCoP/plCoP/lazyCoP in
   connection tableaux) or the next tactic to apply (TacticToe, GPT-f, HTPS).
   Higher leverage per-decision but harder: the model is queried millions of
   times, so inference latency is part of the algorithm, and the search structure
   (best-first vs MCTS vs AND/OR HTPS) interacts with the learned ranker.

Everything else - the calculus, unification, the trusted kernel - is held fixed.
You do not learn the inference rules; you learn *where to point them*.

**The expert-iteration loop** is the engine behind both:

```
  prove a batch of problems with the current model + search
  harvest the successful proofs  (which premises / clauses / tactics worked)
  retrain the selector/policy/value on this fresh, on-distribution data
  prove more (previously unsolved) problems
  repeat
```

This is the common skeleton of AlphaZero-style self-play (rlCoP), DAgger-style
imitation-with-correction, ENIGMA's looped retraining, and GPT-f/HTPS online
training. It works because a theorem prover *generates its own labeled data*
every time it succeeds: a found proof is simultaneously an achievement and a
training example. The library of existing proofs bootstraps the loop; the loop
then extends the library.

**The trust invariant**, finally, is what lets all of this be deployed in a proof
assistant without compromising soundness. The learned, external, possibly buggy
machinery is an *oracle for search only*. The result is always re-checked: by
`metis`/`smt` reconstruction (Isabelle), by `sauto` (Coq/Rocq), by Duper emitting
native proof terms (Lean), all bottoming out in the ITP's small trusted kernel. An
implementer integrating an ATP into a proof assistant should treat this as
non-negotiable: let the ML and the external prover be as clever and as untrusted
as you like, but never let their "yes" reach the user without a kernel-checked
proof behind it.

---

### 8. Quick reference: shipped versus research-stage

- **Shipped, mature.** Sledgehammer (Isabelle/HOL) with MePo/MaSh/MeSh, the
  Blanchette type encodings, multi-prover dispatch, and metis/smt/Isar
  reconstruction. CoqHammer (Coq/Rocq) with sauto reconstruction. E with ENIGMA
  guidance and Vampire with SInE; both heavily deployed.
- **Released and usable, newer.** Duper and lean-auto for Lean 4. LeanDojo +
  ReProver as a research platform and model.
- **Recent research systems (2022-2025), available but less hardened.**
  LeanHammer with the LeanPremise neural selector (2025); Deepire II (2025) in
  Vampire; HTPS (2022) and the GPT-f line as research provers rather than
  push-button tactics; rlCoP/plCoP/lazyCoP as connection-calculus research
  provers.

The trajectory is clear: premise selection and clause/tactic selection are
converging on learned, increasingly neural rankers, wrapped in expert-iteration
loops, bracketed by sound translation out and kernel-checked reconstruction back.
A hammer for thvm-style work would follow exactly this template - the only real
questions are which selector to train and which trusted reconstruction backend to
target.

---


## What This Means for thvm

thvm's equational ATP is a completion-based unit-equality prover - the
Twee/Waldmeister family of Section 2, not the full superposition family of
Section 1. That framing is what makes the survey actionable: most of the
machinery in Sections 1, 3, and 4 solves problems thvm does not yet have
(general clauses, theories, higher order, huge libraries), while Section 2
is a near-exact description of the engine we already run. The takeaways
below are ordered by how directly they bear on the current code.

### Confirmed and already landed

- **Efficient order comparison is not optional.** Loechner's results
  (Section 2) say the naive Lexicographic Path Ordering (LPO) recursion is
  worst-case exponential and naive Knuth-Bendix Ordering (KBO) is
  quadratic, and that both collapse to (near-)linear with the right
  algorithm. This is not academic: profiling thvm's own LPO on the Sheffer
  / Nand axioms measured roughly 105573 recursive `lpo_rec` calls per
  top-level comparison, because `lpo_some_arg_dominates`,
  `lpo_dominates_all_args`, and `lpo_lex` all re-descend overlapping
  subterm pairs. Memoizing the verdict per `(s, t)` pair (an
  epoch-stamped, direct-mapped cache, committed in `src/lpo/_.c`) cut the
  average to about 99 recursions and raised completed comparisons per
  second by roughly 143x, with zero correctness regressions. The next step
  here is to port Loechner's genuinely linear KBO (the single combined
  weight-and-variable-balance traversal described in Section 2) to replace
  thvm's `kbo_addto`/`kbo_diff` pair, which the saturation profile shows as
  the hot path for KBO-method runs.

- **CPinGoal goal-direction is a real Waldmeister method, faithfully
  ported.** Section 2's account of Waldmeister's ClasHeuristics
  (AddWeight, GtWeight, MixWeight, and the goal-directed CPinGoal measure)
  matches the `CriticalPairWeight` knobs thvm now exposes. The survey
  confirms these are the right levers and that the precedence
  auto-generation heuristic (PhilMarlow / Praezedenzgenerator) is part of
  the same package, which thvm implements as `AutoPrecedence`.

### High-value, low-friction ports (Section 2)

- **Ground joinability and connectedness redundancy (Twee).** Twee's most
  cited practical win is aggressively discarding critical pairs that are
  ground-joinable or otherwise redundant before they enter the passive
  set. thvm already has joinability and subsumption filters; the Twee
  ground-joinability test and the "connectedness" criterion are the
  documented next increments and are local changes to the critical-pair
  intake path.

- **Critical-pair selection is the search.** Both Twee and Waldmeister
  spend their intelligence on which critical pair to process next, scored
  by a weight that blends term size, age, and goal-relevance. thvm's deep
  WolframAxioms theorems (AndAssociativity, OrAssociativity, the Implies
  theorems) still fail at every budget even now that the LPO comparator is
  143x faster: the wall is selection / search direction, not raw
  comparator throughput. This is the single most important place to invest,
  and Section 2's selection-heuristic detail (especially CPinGoal scoring
  and Twee's passive-set ordering) is the menu.

- **Flatterm / closure-based reduction (Waldmeister).** Waldmeister's
  flatterm representation and closure-based reduction make its inner
  rewrite loop cache-friendly and avoid pointer-chasing. thvm's terms are
  shared Interaction-Calculus cells; whether a flatterm-style linearized
  representation would help the rewrite hot path is an open, measurable
  question raised by the survey.

### Architectural ideas worth tracking (Sections 1, 3, 4)

- **The DISCOUNT vs Otter loop choice (Section 1).** thvm's saturation
  loop should be classified explicitly against these two variants; the
  DISCOUNT loop (only the active set is kept interreduced) is what most
  modern equational provers use and matches thvm's interreduction story.

- **Term indexing beyond discrimination trees (Section 1).** thvm uses a
  discrimination-tree rule index and a feature-vector subsumption index.
  Fingerprint indexing (Section 1) is a cheap, well-documented upgrade for
  the matching queries; substitution trees are heavier but pay off at large
  active-set sizes.

- **AVATAR-style splitting (Section 1) is probably not for thvm yet.** It
  is the headline idea of the saturation family, but it targets general
  clauses with multiple literals; unit-equality completion has nothing to
  split. Worth understanding, not worth porting until thvm leaves the unit
  fragment.

- **Learned critical-pair selection (Section 4) is the long game.** ENIGMA
  and Deepire show that the clause-selection decision is the high-leverage
  learnable bottleneck, and the expert-iteration loop (prove, learn from
  the proof traces, prove more) is how the field is advancing. thvm already
  emits proof traces; an ENIGMA-style fast feature-based scorer over
  critical pairs, trained on thvm's own successful proofs, is the natural
  research arc once the hand-written selection heuristics are exhausted.

### The honest competitive picture

On the comparable shallow WolframAxioms theorems thvm already beats the
built-in Wolfram `FindEquationalProof`. The deep theorems remain open for
thvm and are hard for the built-in too (it proves four of five in roughly
7-14 seconds; one of them, ImpliesShefferAxioms, defeats the built-in
entirely within 75 seconds). Against the dedicated UEQ champions of Section
2 (Twee, and historically Waldmeister) thvm is not yet competitive on the
hardest unit-equality problems, and the survey makes the reasons concrete:
those systems pair an efficient reduction-order implementation (now matched
on the LPO side) with years of tuned critical-pair selection and redundancy
elimination (not yet matched). The roadmap that follows from this document
is therefore selection and redundancy first, representation second, and
learned guidance last.
