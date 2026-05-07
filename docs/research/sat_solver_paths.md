# SAT Solving on Interaction Calculus, on GPU, on thvm

A research dossier on what algorithmic moves are possible from where
we are now (brute-force SUP-tree on Metal, V=2..10), and where they
actually plant flags.

## TL;DR

Static brute-force enumeration is the wrong demo and the wrong target.
The SAT *decision* problem belongs to CDCL on industrial instances and
to SLS / Survey Propagation on random 3SAT near the phase transition.
GPU CDCL has been an open problem for 15+ years; the consensus answer
is "it doesn't work" because clause learning is intrinsically
sequential.  Two real moves are open to thvm:

1. **Reframe the headline as #SAT / AllSAT / model enumeration**, where
   IC's optimal-sharing genuinely buys us something and where there is
   no CDCL-shaped opponent.  Compare against ApproxMC, sharpSAT,
   cmsgen.

2. **Prototype dynamic labels (HVM4's DSU/DDU) inside the SUP-tree**
   to get unit-propagation and decision-heuristic-flavored search
   without abandoning the IC substrate.  This is the lever the bench
   has been missing; static labels commit at parse time to "branch on
   every variable independently", which is exactly the brute-force
   semantics we have.  Dynamic labels let the next-decision-variable
   be chosen based on the formula's *current reduced state* — closer
   to DPLL+BCP than to CDCL, but a real algorithmic upgrade over
   pure brute force.

Survey Propagation is an attractive future direction (it's the
algorithm that does best on the regime CDCL is weakest on, has GPU
prior art, and is structurally a natural fit for our substrate), but
it requires a separate kernel architecture and is best done as a
companion engine rather than a replacement.

The full ranked recommendation is in §7.  The rest of this document
backs up each claim.

## 1. Where we are

`bench/sat_collapse/profile_aot_metal.wls` measured (commit 45cb1995):

| V  | n_leaves | num/n         | collapse ms | WL ms | factor |
|----|---------:|--------------:|------------:|------:|-------:|
| 2  | 128      | 128/128       | 4-5         | 0.05  | 100x slower |
| 5  | 1024     | 1024/1024     | 22          | 0.07  | 314x        |
| 8  | 8192     | 8192/8192     | 108         | 0.09  | 1200x       |
| 10 | 32768    | 32576/32768   | 412         | 0.11  | 3700x       |

The factor against `WL`'s `SatisfiableQ` widens with V because we are
brute-force enumerating 2^V assignments and WL is running CDCL on a
search tree whose effective depth grows polylogarithmically on
structured instances.  This is an algorithmic gap, not a constant
factor.  A faster GPU does not close it.

What we *do* have: a deterministic, parallel, IC-correct evaluator of
Church-encoded propositional formulas across all 2^V assignments
simultaneously.  That is genuinely useful — for the right problem
class.

## 2. Why "beat CDCL on SAT decision" is unwinnable

The literature is consistent: CDCL has resisted GPU acceleration for
15+ years.  The state of the art in GPU-assisted SAT is **ParaFROST**
(Osama, Wijs, Biere, TACAS 2021): it uses GPU only for *inprocessing*
(Bounded Variable Elimination, subsumption, hyper-binary resolution)
and runs the CDCL search on CPU.  This is the verdict.  Every
attempt to put CDCL search on the GPU — CUD@SAT (2014), various
academic prototypes — concludes that branch divergence and the
trail-dependent nature of clause learning fight the GPU memory
hierarchy.  The 2025 SAT Competition parallel UNSAT track was won by
**MallobSat** (Schreiber 2025), pure CPU, scaling to thousands of
cores via portfolio + clause sharing.  No GPU CDCL solver has won a
SAT-Competition track.  Le Berre et al. ("Too much information: why
CDCL solvers need to forget learned clauses") explain the underlying
issue: each learned clause is a function of the entire trail at
conflict time, making clause-database management the dominant cost
and the dominant communication bottleneck.

The HVM "1 second vs 3 minutes Rust brute force" demo is real but
its baseline is a strawman.  Hirrolot's analysis ("Solving SAT via
positive supercompilation") and Asperti's bounds ("About the efficient
reduction of lambda terms", arXiv 1701.04240) make this rigorous: IC's
optimal-sharing gives constant-factor and sometimes super-polynomial
wins on terms with the right structure, but the bookkeeping cost is
non-elementary in the worst case.  For arbitrary 3-SAT there is no
known sub-exponential reduction in IC.

## 3. The algorithm landscape

| Algorithm                       | Best for                                          | GPU-friendly? | IC-natural? |
|---------------------------------|---------------------------------------------------|--------------:|------------:|
| Brute force enumeration         | Tiny V (V<10); IC demos                           | Yes            | Yes          |
| DPLL + BCP                      | Mid V; teaching-baseline; no learning             | Partial        | Yes (with dynamic labels) |
| **CDCL** (Kissat, MapleSAT)     | **Industrial / structured SAT**; canonical SOTA   | **No**         | No           |
| Cube-and-Conquer                | Hard combinatorial (Boolean Pythagorean Triples)  | Cubes parallel; CDCL leaves not | No |
| **WalkSAT / ProbSAT (SLS)**     | **Random 3-SAT, hard satisfiable instances**       | **Yes** (embarrassingly) | Yes (local moves) |
| FFT-gradient SLS (FastFourierSAT) | Random k-SAT, MaxSAT-style                       | **Yes**        | Less so      |
| **Survey Propagation (SP/BSP)** | **Random k-SAT near phase transition (α≈4.267)**  | **Yes** (Manolios & Zhang 9x demonstrated) | Yes (factor-graph stencil) |
| Belief Propagation              | Tractable factor graphs; loopy graphs degrade     | Yes            | Yes          |
| Resolution / DP elimination     | Theoretical; preprocessing                        | Limited        | Partial      |
| Parallel CDCL (portfolio)       | Add cores → Nx                                    | Mediocre       | No           |

The two algorithm classes that consistently produce real GPU wins are
**SLS** and **Survey Propagation**.  Both target random k-SAT near
the phase transition where CDCL melts down.  Both have local-update
structure that maps cleanly to GPU threadgroups.  SP is the more
sophisticated of the two (message passing on a factor graph, derived
from the cavity method in statistical physics) and has the strongest
single-paper signal: Manolios & Zhang's GPU-SP gets 9x over CPU, scales
to millions of variables on hard random instances.

## 4. The "what does IC actually win at" question

Asperti's bounds and Lamping's optimal lambda reduction define the
ceiling.  IC reduction shines on programs where the **same subterm is
referenced many times AND the references resolve to the same value**.
Optimal-sharing replaces N evaluations with 1 evaluation + N reads.
If the references resolve to *different* values (the multi-use SAT
variable problem we hit on the curried-LAM batch attempt), DUPs are
forced and the wins evaporate.

Concrete IC-natural problem classes:

- **Type checking / dependent type elaboration**: shared subterms in
  type expressions resolve to equal types.  Bend2's actual product.
- **Symbolic execution** of pure functional programs: partial
  evaluation across multiple call paths shares overlapping reductions.
- **Theorem proving search**: branch-and-search where most branches
  share a common context.
- **#SAT / model counting**: every assignment must be visited; sharing
  is genuine because formula sub-evaluations repeat across siblings.
- **AllSAT**: enumerate all satisfying assignments; same story.
- **MaxSAT / Weighted MaxSAT**: similar — enumerate to find optimum.
- **Counting models for probabilistic inference / WMC**: a major
  application of #SAT.

For SAT *decision*, IC's optimal-sharing helps only when the formula
has structure (repeated literals across clauses, common
sub-expressions).  Random 3-SAT with disjoint clauses gets little
sharing benefit — every branch evaluates a different residual.

## 5. HVM4 non-standard combinators evaluated

Five combinators beyond the standard {LAM, APP, SUP, DP0/DP1, ERA,
NUM, REF} set were investigated.  Findings:

### Bridges (BRI/ANN, "ICC")

A third combinator pair (`θx. body` and `{term : type}`) introduced in
Taelin's *Interaction Calculus of Constructions* repo, NOT in HVM4
mainline.  Designed for dependent types: type-checking happens by net
reduction, and ill-typed nets collapse to ERA.

The naming is a trap: thvm's `TAG_BJ0/BJ1/BJV` are unrelated; they're
the "binary join" tags HVM4 uses internally for collapse-readback of
quoted de Bruijn variables, not the ICC bridge.

**Useful for SAT?** No.  Bridges are about pruning *type-incorrect
synthesized programs* (SupGen-style), not propositional search.  An
ANN-BRI annihilation could in principle prune an unsat clause if you
encoded clauses as "types" and assignments as "values", but you'd be
fighting against the substrate — OP2/MAT already evaluate
propositional clauses fine.  Skip.

### Dynamic labels (DSU/DDU)

SUP/DUP whose label is a runtime-evaluated NUM expression.  Tags `DSU=34,
DDU=35` in `/Users/swish/src/HVM4/clang/hvm.c`.  Rewrite rules in
`wnf/dsu_num.c` and `ddu_num.c`, ~10 lines each: when the label
expression reduces to a NUM, the node rewrites to a regular SUP/DUP
with that numeric label.  Subsequent annihilation/commutation follows
standard SUP/DUP rules.

Bend2 exposes them via `&(expr){a,b}` and `!&(expr){x,y}=v;b`.  Working
HVM4 test at `test/fork_dynamic.hvm`.

**Useful for SAT?  Yes — this is the key non-standard lever.**  With
static labels, "var i gets label i" is committed at parse time, which
forces 2^V brute force with sharing.  With dynamic labels:

- The next decision variable can be chosen based on the formula's
  *current* reduced state.  The label expression `(decide_next clauses)`
  is itself an HVM function whose result becomes a label.  This is
  the core of any unit-propagation-aware search.
- Identical labels emitted in different subtrees collapse via
  DUP-SUP annihilation regardless of where in the tree they sit.
  This gives a primitive form of subtree merging that brute force
  doesn't have.
- Unit propagation is encodable as: when a clause becomes unit, emit
  a *deterministic* assignment with a label that doesn't open a new
  dimension.  Effectively collapses the would-be 2^k subtree into a
  single path.

This is closer to BCP-flavored DPLL than to CDCL — there is no
clause learning, no non-chronological backjumping — but it is a
genuine algorithmic step beyond static brute force.

The implementation cost is small: two new tags, ~10 lines of MSL
each for the rewrite rules, and a host-side decision function
emitting label values.

The main risk: dynamic labels need the formula reduction to be lazy
enough that the decide function sees the current state of the search
before each branch.  HVM4's WHNF schedule may need adjustment to
ensure the decision-heuristic function runs after partial unit-
propagation has happened.  Worth measuring interaction counts on a
small instance to confirm the speedup is real.

### Unordered SUPs (USUP/UDUP)

A SUP/DUP variant where ports are interchangeable: `%L{a,b} == %L{b,a}`.
The unordered DUP has a single output port acting as a "ref-counted
producer that is never consumed" — clauses pop elements rather than
splitting in two.

Source: gist `93c327e5b4e752b744d7798687977f8a` ("Truly Optimal
Evaluation with Unordered Superpositions").  *Not yet in HVM4
mainline*; the gist describes an HVM3 prototype.

**Useful for SAT?**  Yes, as an optimization.  Two reasons:

- A clause `(x1 ∨ x2 ∨ x3)` reads each variable's superposed
  assignment once per occurrence.  Ordered DUPs across many clauses
  build O(n²) DUP-chain scaffolding; UDUPs collapse this to O(n).
  The gist reports linear vs quadratic interaction counts on Peano
  unification benchmarks.
- The "swap ports freely" property means a work-stealing scheduler
  can take either branch onto whichever GPU lane is free.  Our
  current ordered SUP commits left=lane-low, right=lane-high.  This
  is what static brute-force sub-optimally hard-codes.

Implementation is invasive: every DUP-X rewrite rule needs an
unordered variant.  Wins are constant-factor on small V; compound on
larger formulas with many clauses.

### INC (priority wrapper, tag 41 in HVM4)

Transparent to all rewrites except *collapse*; INC-wrapped subterms
get lower priority in the work-stealing PQ.

**Useful for SAT?**  Yes, cheaply.  Wrap "expensive guesses"
(deep-decision branches) in INC so cheap unit-propagation paths
flatten first.  This is beam-search-flavored collapse without
changing reduction semantics.  Implementation: one tag, transparent
to DUP/SUP/APP/LAM rules, only the collapse PQ ordering changes.

### ANY (top wildcard, tag 40)

`ANY === x` reduces to 1; duplicates itself.

**Useful for SAT?**  Modestly.  Encodes "don't care" assignments for
variables that don't appear in the residual formula.  Lets the
solver avoid branching on irrelevant vars after BCP eliminates them
from the clause set.  Requires the decision function to detect "var
i has no remaining clauses" and emit ANY in its place — coordinated
with dynamic labels.

### EQL/AND/OR (strict structural eq + short-circuit)

Already in HVM4 mainline, distribute through SUP via `wnf/and_sup.c`
etc.  When one literal in a clause is true, the SUP it short-circuits
past doesn't get explored.  This is "free" unit-propagation living
already in the substrate; the question is whether our SAT encoding
uses it correctly.  Worth auditing the bench's `TBoolAnd / TBoolOr`
helpers to confirm we're going through these tags rather than going
through generic APP-SUP commutes.

## 6. Critical sources

The 15 most important references found, with one-line takeaways:

1. **Taelin, "Simple SAT Solver via superpositions"**
   <https://gist.github.com/VictorTaelin/9061306220929f04e7e6980f23ade615>
   The canonical "1s vs 3min" baseline.  Brute force with implicit BCP
   from short-circuit; not CDCL.

2. **HVM2 paper (Taelin)**
   <https://raw.githubusercontent.com/HigherOrderCO/HVM/main/paper/HVM2.pdf>
   The runtime spec.  SAT is a showcase, not a benchmarked claim.

3. **Hirrolot, "Solving SAT via Positive Supercompilation"**
   <https://hirrolot.github.io/posts/sat-supercompilation.html>
   IC-style sharing trick is theoretically interesting but worst case
   still exponential.

4. **Asperti, "About the efficient reduction of lambda terms"**
   <https://arxiv.org/abs/1701.04240>
   Optimal lambda reduction's theoretical ceiling: bookkeeping cost
   non-elementary in the worst case.

5. **Mézard, Parisi, Zecchina, "Survey propagation: an algorithm for
   satisfiability"**  <https://arxiv.org/abs/cs/0212002>
   The SP algorithm.  Local message updates ⇒ embarrassingly parallel.

6. **Marino, Parisi, Ricci-Tersenghi, "The backtracking survey
   propagation algorithm"**
   <https://www.nature.com/articles/ncomms12996>
   BSP solves random K-SAT essentially up to αc in linear time.

7. **Manolios & Zhang, "Implementing Survey Propagation on GPUs"**
   <https://www.khoury.northeastern.edu/home/pete/pub/gpu-sat.pdf>
   ~9x over CPU SP, millions of variables on hard random instances.

8. **Osama, Wijs, Biere, "ParaFROST" (TACAS 2021)**
   <https://github.com/muhos/ParaFROST>
   GPU only for inprocessing; CDCL search stays on CPU.  The current
   verdict on GPU CDCL.

9. **Yang & Vardi, "FastFourierSAT" (AAAI 2024)**
   <https://arxiv.org/abs/2308.15020>
   FFT-gradient continuous local search; ~15x PAR-2 over CPU.

10. **Schreiber, "MallobSat in SAT Competition 2025"**
    <https://github.com/domschrei/mallob>
    2025 parallel UNSAT track winner.  Pure CPU portfolio.

11. **Heule, Kullmann, Biere, "Cube and Conquer"**
    <https://www.cs.utexas.edu/~marijn/publications/cube.pdf>
    Hybrid: parallel cubes + CDCL leaves.

12. **Jiresch, "Towards a GPU-based implementation of interaction
    nets"**  <https://arxiv.org/abs/1404.0076>
    The honest precedent: GPU INs feasible but "mostly performs
    weaker than existing evaluators".

13. **Le Berre et al., "Why CDCL solvers need to forget learned
    clauses"**  <https://pmc.ncbi.nlm.nih.gov/articles/PMC9417043/>
    The structural reason CDCL has resisted GPU acceleration.

14. **HVM4 source: dynamic-label rewrites**
    `/Users/swish/src/HVM4/clang/wnf/dsu_num.c`,
    `/Users/swish/src/HVM4/clang/wnf/ddu_num.c`,
    `/Users/swish/src/HVM4/test/fork_dynamic.hvm`
    The DSU/DDU implementation — ~10 lines each, plus a working test.

15. **Taelin, "Truly Optimal Evaluation with Unordered Superpositions"
    (gist)**
    <https://gist.github.com/VictorTaelin/93c327e5b4e752b744d7798687977f8a>
    USUP/UDUP: linear vs quadratic interaction counts on Peano-style
    benchmarks.

## 7. Recommendation

### Rank 1 — Pivot the headline benchmark to AllSAT / #SAT

This is where IC genuinely wins and CDCL solvers don't compete.
Counting and enumerating models is what the SUP-tree was designed for;
every leaf must be visited regardless of the algorithm, so optimal-
sharing of common formula sub-evaluations is doing real work.  The
comparators are sharpSAT, ApproxMC, cmsgen — not Kissat or
SatisfiableQ.  Implementation: change `bench/sat_collapse/` to count
NUM-1 leaves instead of asserting "is any True" and to materialize
the assignment per leaf.  Existing collapse infrastructure works as-is.

This is a story we can tell honestly: "GPU IC reducer enumerates
2^V satisfying assignments in parallel, with constant-factor sharing
wins from common formula sub-evaluations."  No fight against CDCL.

**Effort: 1-2 days.**  Reuse the existing pipeline; add a "Col_i +
Join" collapser tree (Victor's `VICTOR_NOTES.md` already specifies it)
and reformat the bench output as model counts and assignment lists.

### Rank 2 — Prototype dynamic labels for unit-propagation in the SUP-tree

Add HVM4's DSU/DDU tags to thvm.  Use them to encode a decision
heuristic that selects the next branching variable based on the
formula's current reduced state.  Use INC to prioritize unit-clause
short-circuit paths in the collapse work-stealing PQ.  Goal: turn
the current 2^V brute force into BCP-flavored DPLL, on the same
substrate, without rewriting the kernel architecture.

This is the lever the bench has been missing.  At V=20 (currently
out of reach because 2^20 = 1M leaves), BCP could prune the search
to ~10K-100K live branches — a 10-100x reduction.

**Effort: 2-3 days for the substrate changes (two new tags,
rewrite rules in CPU runtime + Metal shaders).  Plus 1-2 days for
the decision heuristic and BCP encoding in WL.**  Risk: needs
laziness in the WHNF schedule for the decide function to see
post-BCP state; may require WHNF-order adjustments.  Test plan:
count interactions on V=10..20 instances vs static brute force.

### Rank 3 — Survey Propagation as a companion engine

Build a separate Metal kernel that runs SP on the formula's factor
graph.  This is structurally different from the SUP-tree
(message-passing stencil, not reduction) and best done as a
separate codepath rather than retrofitting the existing collapse.
Targets the random 3-SAT phase transition where SP/BSP genuinely
beat CDCL.

Manolios & Zhang's GPU-SP code provides a reference implementation;
we'd port the kernel structure and integrate via a new
`TAOTSurveyPropagate[formula]` WL surface.

**Effort: 1-2 weeks.**  Larger lift but the payoff is
genuine: a SAT engine that wins on a problem class CDCL is bad at,
on hardware that's a natural fit.

### What NOT to do

- **GPU CDCL.**  15+ years of negative results.  ParaFROST's
  CPU-search-with-GPU-inprocessing design is the verdict.
- **Bridges (BRG/ANN).**  Wrong domain — they're about
  type-pruning of synthesized dependent-type programs, not
  propositional search.  Revisit if/when thvm pivots to
  type-checking demos.
- **Per-leaf curried-LAM batch reduction.**  Already learned this
  fails on multi-use vars (see `aot_metal_z2_atomicity.md`).  The
  TSup-encoded SUP-tree handles multi-use implicitly via APP-SUP
  commutes.

## 8. Concrete next iteration

Following Rank 1+2 in order:

1. Implement the AllSAT collapser tree (Col_i + Join) per Victor's
   `VICTOR_NOTES.md`.  Reformat the bench to report model counts and
   the assignment list.  Compare against sharpSAT / cmsgen on small V.
   Commit as `feat(sat): AllSAT enumeration via SUP-tree + collapser`.

2. Add HVM4 DSU/DDU dynamic-label tags to thvm's runtime + Metal
   shaders.  Port the rewrite rules from
   `/Users/swish/src/HVM4/clang/wnf/dsu_num.c` and `ddu_num.c`.
   Verify with `test/fork_dynamic.hvm`-equivalent unit test.

3. Add HVM4 INC priority-wrapper tag.  Wire into the collapse work-
   stealing PQ ordering.

4. Build a WL helper that emits a curried CNF def using DSU labels
   computed from a "next-decision-var" function: argmax over
   clause-occurrence counts among live clauses.  Test on V=10..20
   to confirm interaction-count reduction over static brute force.

5. Survey Propagation companion kernel (separate iter).

The combined effect of (1)+(2)+(3)+(4) gives thvm a credible
SAT-adjacent demo: AllSAT enumeration with BCP-flavored search,
honest about not competing with CDCL on industrial benchmarks but
genuinely useful for model-counting workloads where IC is the right
substrate.
