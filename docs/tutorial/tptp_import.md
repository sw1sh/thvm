# TPTP Import

A practitioner's guide to the TPTP (Thousands of Problems for Theorem
Provers) input path: how to point `TFindProof` (and `TFindProofSMT`) at
a benchmark `.p` file or an inline string, what subset of the TPTP
grammar is supported, and how the imported clauses become the
`Axioms / Conjecture` shape thvm's ATP and SMT entries consume.

Abbreviations used throughout (spelled out on first appearance):

- TPTP: Thousands of Problems for Theorem Provers (`tptp.org`).
- UEQ: Unit-EQuality division of TPTP.
- CNF: Conjunctive Normal Form (TPTP `cnf(...)` clause).
- FOF: First-Order Form (TPTP `fof(...)` clause).
- BNF: Backus-Naur Form (the grammar at
  `github.com/TPTPWorld/SyntaxBNF`).

## 1. Why a TPTP path

The TPTP benchmark suite is the standard cross-prover problem corpus
(Vampire, E, Twee, Waldmeister all run on it). The ATP literature
quotes timings against TPTP slugs like `LCL129-1.p` or
`ShefferAxioms/AndAssociativity.p`. A reader who wants to reproduce a
result, or use thvm against a problem they already have in TPTP form,
needs an importer.

The importer also lets thvm's two entries — `TFindProof` (equational
saturator) and `TFindProofSMT` (QF_UF decision procedure, see
`docs/tutorial/smt.md`) — accept a `File["..."]` or inline string
without forcing the caller to hand-translate clauses to WL.

## 2. Quick start

### 2.1 An inline string, ATP path

```mathematica
TFindProof[
    "cnf(a1, axiom, and(X, Y) = and(Y, X)).
     cnf(a2, axiom, and(X, and(Y, Z)) = and(and(X, Y), Z)).
     cnf(g,  negated_conjecture, and(and(p, q), r) != and(r, and(q, p)))."]
(* -> ProofObject[...] *)
```

The string contains three clauses: two universally-quantified
equational axioms (commutativity + associativity of `and`) and a
ground inequality conjecture. The importer parses the clauses, the
dispatch routes them as `Axioms / Conjecture`, and the saturator
closes the goal.

### 2.2 A file, ATP path

```mathematica
TFindProof[File["AbelianGroupAxioms__InverseOfInverse.p"],
    TimeConstraint -> 10]
(* -> ProofObject[...] *)
```

Same dispatch. The file is parsed via `Import[..., "Text"]` and fed
through `tptpImport`.

### 2.3 An inline string, ground SMT path

```mathematica
THVMLink`SMT`TFindProofSMT[
    "cnf(a1, axiom, a = b).
     cnf(a2, axiom, b = c).
     cnf(g,  negated_conjecture, a != c)."]
(* -> <|"Status" -> "Proved", "Method" -> "CongruenceClosure", ...|> *)
```

Ground inputs (no `X`-style universal variables) route through
congruence closure for a near-linear-time decision instead of
saturation. Non-ground inputs are rejected with a clear message:

```mathematica
THVMLink`SMT`TFindProofSMT[
    "cnf(a, axiom, and(X, Y) = and(Y, X))."]
(* TFindProofSMT::nonground: ... -- use TFindProof instead. *)
(* -> $Failed *)
```

See `docs/tutorial/smt.md` for the SMT entry.

## 3. Coverage matrix

### 3.0 What TPTP encodes

The TPTP infrastructure (`tptp.org`, `github.com/TPTPWorld/SyntaxBNF`)
formalises two axes:

**Axis A -- the syntactic clause heads.** Each one encodes a
different logic fragment:

| Clause head | Encodes | Example |
|-|-|-|
| `cnf` | conjunctive normal form; disjunctions of literals, implicit universal quantification | `cnf(a, axiom, p(X) \| ~q(X)).` |
| `fof` | full first-order form with `&`, `\|`, `~`, `=>`, `<=>`, `! [...]`, `? [...]` | `fof(g, conjecture, ! [X] : (p(X) => q(X))).` |
| `tff` | typed first-order (Sigma-types over a base signature) | `tff(g, conjecture, ! [X:nat] : p(X)).` |
| `thf` | typed higher-order (with lambdas and partial application) | `thf(g, conjecture, ! [X:i > o] : X @ a).` |
| `tcf` | typed cnf (tff's cnf cousin) | `tcf(a, axiom, p(X:nat) \| ~q(X:nat)).` |
| `ncf` | non-classical FOF (modal, intuitionistic, ...) | `ncf(g, conjecture, $box(p)).` |
| `tpi` | TPTP process instruction (meta-directive, not a clause) | `tpi(set, axiom, $time_limit($i,30)).` |
| `include` | references another `.p` file, splices its clauses | `include('Axioms/GRP004-0.ax').` |

**Axis B -- the problem-class / domain divisions.** TPTP groups its
problems by topic, encoded as a 3-letter prefix in the file name (e.g.
`GRP004-1.p` is group-theory problem 4 variant 1):

| Division | Domain |
|-|-|
| `ALG` | Algebra (rings, monoids, lattice algebra outside groups) |
| `ANA` | Analysis (real-valued functions, limits) |
| `ARI` | Arithmetic (integer / rational) |
| `BOO` | Boolean algebra |
| `CAT` | Category theory |
| `COL` | Combinatory logic (S, K, I, B, C, W combinators) |
| `COM` | Communicating-systems / computer science |
| `CSR` | Common-sense reasoning |
| `DAT` | Data and information systems |
| `FLD` | Field theory |
| `GEO` | Geometry |
| `GRA` | Graph theory |
| `GRP` | Group theory (groups, abelian groups, McCune axioms) |
| `HEN` | Henkin models |
| `HWC` | Hardware creation / verification |
| `HWV` | Hardware verification |
| `KLE` | Kleene algebra |
| `KRS` | Knowledge representation |
| `LAT` | Lattices |
| `LCL` | Logic calculi (modal, dynamic, ...) |
| `LDA` | Left-distributive algebra |
| `MED` | Medicine |
| `MGT` | Management |
| `MSC` | Miscellaneous |
| `NLP` | Natural language processing |
| `NUM` | Number theory |
| `NUN` | Non-standard universal Horn (rare) |
| `PHI` | Philosophy / metaphysics |
| `PLA` | Planning |
| `PRD` | Predicate calculus |
| `PRO` | Process algebra |
| `PUZ` | Puzzles (Schubert's steamroller, etc.) |
| `QUA` | Quantum mechanics |
| `REL` | Relation algebra |
| `RNG` | Ring theory |
| `ROB` | Robbins algebra |
| `SCT` | Set theory (constructive) |
| `SET` | Set theory (classical) |
| `SEU` | Set theory union |
| `SEV` | Set theory ?? |
| `SWB` | Semantic web / OWL |
| `SWC` | Semantic web continued |
| `SWV` | Software verification |
| `SWW` | Semantic web wider |
| `SYN` | Pure syntactic (synthetic) |
| `SYO` | Syntactic ordered |
| `TOP` | Topology |

**Axis C -- the SZS status classifiers** (`Satisfiable`,
`Unsatisfiable`, `Theorem`, `CounterSatisfiable`, ...). TPTP's
problem-list catalogue files tag each problem with the expected SZS
status; the importer ignores these (only the clause set is parsed).

### 3.1 thvm coverage against each axis

**Axis A (clause heads):**

| Clause head | Parser support | Prover support | Notes |
|-|-|-|-|
| `cnf` (single equational literal) | YES | YES | The UEQ fragment thvm's saturator targets. |
| `cnf` (multi-literal disjunction) | NO | NO | `clauseToEquation` only handles a single `=` / `!=` literal. Non-UEQ. |
| `fof` (single `! [...] : (l = r)` or bare equation) | YES | YES | Iter 4 (commit 98bf9243). |
| `fof` (full Boolean combination of equality atoms, ground) | NO at parser layer | YES at the SMT layer | `TSmtDecide` handles ground Boolean combinations of `Equal`/`Unequal`. The parser does not yet build them; a user wanting this combines TPTPImport for the atoms with `TSmtDecide` directly. |
| `fof` (existentials / non-equality predicates / general first-order) | NO | NO | Saturator is unit-equational; SMT is QF_UF (no quantifiers, no general predicates yet). |
| `tff` / `tcf` (typed) | NO (skipped + warning) | NO | C engine has sort-check gating but WL surface assumes homogeneous mode. |
| `thf` (higher-order) | NO | NO | thvm has IC-native higher-order types but no first-order-to-higher-order bridge. |
| `ncf` (non-classical) | NO | NO | Modal / intuitionistic logics are out of scope. |
| `tpi` (process instruction) | NO (silently skipped) | n/a | Meta-directive; ignored. |
| `include('...')` | NO (skipped + warning) | n/a | Caller must inline the included file. |

**Axis B (problem-class divisions):** the saturator decides
satisfiability of conjunctions of universally-quantified equational
literals — UEQ. Every TPTP division contributes UEQ-shape problems,
but the bulk of each division is non-UEQ (Horn / general FOF). The
divisions thvm closes the most on are the ones whose problems are
*natively* equational:

| Division | UEQ coverage | Notes |
|-|-|-|
| `GRP` (Group theory) | High | Standard McCune / TPTP group axioms (associativity, identity, inverse, abelian-group variants) all encode as UEQ. thvm closes the easy `InverseOf*` family in seconds and the deeper `Implies*` cross-axiom direction with the schedule's structure-aware Sheffer front-load. |
| `ROB` (Robbins algebra) | Partial | Robbins itself is UEQ but the hardest theorem (`RobbinsAxioms / DoubleNegation`, McCune's 1996 EQP result) is in the iter-17 / 27 "currently out of reach" set. |
| `BOO` (Boolean algebra) | High | Boolean axiom systems (DeMorgan, absorption, distributivity, ...) are UEQ. The `Boolean symmetric` (e.g. `DoubleNegation`, `ExcludedMiddle`, `Noncontradiction`) class needs the GoalDirected MNF front; the structure-aware schedule already front-loads it. |
| `LAT` (Lattices) | High | Lattice axioms are UEQ; the `Lattice` schedule entry uses `GroundJoin` + `Gt` weight (Waldmeister `Verband` row). |
| `RNG` (Ring theory) | High | Ring axioms are UEQ; structure-precedence puts `*` above `+`. |
| `KLE` (Kleene algebra) | Partial | Kleene axioms involve a `*` (closure) operator that breaks termination of naive KBO; needs careful precedence + GroundJoin. |
| `COL` (Combinatory logic) | High for SKI / BCKW | Variable-duplicating S / W combinator rules need LPO + AutoPrecedence; the `Combinatory` schedule entry handles them. |
| `LDA` (Left-distributive algebra) | High | UEQ. |
| `LCL` (Logic calculi, Sheffer / Wolfram nand) | Partial | The easy `nand-Commutativity` family closes via the Sheffer GoalDirected front; the deep `Sheffer / AndAssociativity` and `Sheffer / ImpliesWolframAxioms` family is in the iter-27 "currently out of reach" set (see `atp_methods.md` 5.1). |
| `ALG`, `FLD`, `REL`, `MSC` | Variable | Whichever problems are natively UEQ work; mixed-shape ones bypass the saturator. |
| `SET`, `SCT`, `SEU`, `SEV` (Set theory) | Low | Set-theoretic axioms typically use binary predicates (`element_of`, `subset_of`) -- not equational. |
| `ARI`, `NUM`, `ANA` (Arithmetic / Analysis) | Low | Need linear arithmetic / real-valued theory solvers we have not ported (only EUF via `TSatEUF`). |
| `SWV`, `HWV`, `SWC`, `SWW`, `KRS` | Low to none | Software / hardware verification problems often need typed first-order + arithmetic; thvm's surface is homogeneous untyped. |
| Everything else (CAT, GEO, GRA, HEN, MED, MGT, NLP, NUN, PHI, PLA, PRD, PRO, PUZ, QUA, SYN, SYO, TOP, ...) | Variable | Subset of natively-UEQ problems works; rest needs theory extensions. |

**Axis C (SZS classifiers):** the importer does not read SZS tags.
For a UEQ problem with the standard `negated_conjecture` shape, a
`ProofObject` return means SZS `Theorem` / `Unsatisfiable`; a
`$Failed` return with a clean `Saturated` status means SZS
`Satisfiable` / `CounterSatisfiable` (the C engine reached a finite
complete system without closing the goal); a `TimedOut` / `Failed`
return is SZS `Timeout` / `GaveUp`. The `"Statistics"["Status"]` key
on a bundle (`docs/tutorial/atp_methods.md` 7.1) surfaces these
directly.

### 3.2 Bottom line

For TPTP **UEQ** division benchmarks the importer + saturator is a
complete drop-in: every UEQ problem parses, and the standard ATP
techniques apply. For wider TPTP divisions, coverage tracks the
underlying engine: equational shape -> works; Boolean-combination
ground shape -> route through `TFindProofSMT`; everything else
(typed, higher-order, modal, general first-order with non-equality
predicates) is unsupported.

## 4. Supported grammar subset

The full TPTP grammar (`github.com/TPTPWorld/SyntaxBNF`, ~735 lines)
covers `cnf / fof / tff / thf / tcf / ncf / tpi`. The importer
currently handles **`cnf`** and **`fof`** clauses with a single
equational literal. Other clause heads (`tff` / `thf` / `tcf` / `ncf`
/ `tpi` / `include`) are skipped with a console warning so the rest
of the file still parses.

### 4.1 `cnf(name, role, formula).`

```
cnf(<name>, <role>, <lhs> = <rhs>).
cnf(<name>, <role>, <lhs> != <rhs>).
```

- `<name>` is a clause identifier (consumed and discarded).
- `<role>` is one of `axiom | hypothesis | lemma | conjecture |
  negated_conjecture`. The axiom-shaped roles fold into `Axioms`;
  `conjecture` becomes `Conjecture`; `negated_conjecture` flips the
  literal direction (`a != b` -> `a == b`) then becomes
  `Conjecture` — matching the proof-by-contradiction convention every
  TPTP UEQ problem uses.
- `<formula>` is a single equational literal, either `lhs = rhs` or
  `lhs != rhs`. Variables are clause-scoped uppercase identifiers; each
  one gets a fresh `Pattern[Unique[]]` so subsequent clauses share no
  bound variables.

### 4.2 `fof(name, role, formula).`

```
fof(<name>, <role>, ! [V1, ..., Vn] : (<lhs> = <rhs>)).
fof(<name>, <role>, <lhs> = <rhs>).
```

The optional `! [V1, ..., Vn] :` universal quantifier is stripped
(universal binding is the `cnf` default anyway). Free variables in
the bare form are also treated as universals. The same `=` / `!=`
literal shape applies as `cnf`.

`fof` clauses with conjunctions / disjunctions / existentials /
negations outside this single-equation shape are not handled — the
importer returns `Missing` for those formulas and prints
`tptpImport::badfmla`.

### 4.3 Comments

Line comments (`% ...`) and block comments (`/* ... */`) are stripped
before parsing.

### 4.4 What's NOT supported

| Construct | Status |
|-|-|
| `cnf` multi-literal clauses (`l1 \| l2 \| ...`) | Not supported (UEQ is unit-equality, so this is fine for UEQ benchmarks). |
| `fof` with `&` / `\|` / `~` / `?` outside `! [...] :` | Not supported -- `tptpImport::badfmla`. |
| `tff` (typed first-order) | Skipped (`tptpImport::skipnoncnf`). |
| `thf` (typed higher-order) | Skipped. |
| `tcf` (typed cnf) / `ncf` / `tpi` | Skipped. |
| `include('path').` | Skipped. The caller must resolve includes by passing the assembled file. |
| Equational rewriting modulo theory annotations | Not part of UEQ; not supported. |

## 5. Output shape

`THVMLink\`TPTPImport\`tptpImport` returns:

```mathematica
<|
    "Axioms"     -> {l1 == r1, l2 == r2, ...},
    "Conjecture" -> l == r   (* or None when no conjecture clause *)
|>
```

Both `TFindProof` and `TFindProofSMT` consume this directly. When
`Conjecture` is `None`, `TFindProof[file]` falls through to the
single-argument completion form (saturate the axioms, return the
derived lemmas).

## 6. Namespacing

All symbols built from TPTP function-symbol names land in the private
context `THVMLink\`TPTPImport\`Tptp\``. This is load-bearing:
without it a TPTP `and` or `p` would shadow a user-level `and` or `p`
binding, corrupting the parsed clauses. So the parsed axiom

```
cnf(a, axiom, and(X, Y) = and(Y, X)).
```

prints as

```
THVMLink`TPTPImport`Tptp`and[v1_, v2_] == THVMLink`TPTPImport`Tptp`and[v2_, v1_]
```

(via `InputForm`). When passed back to `TFindProof` this works
transparently — the prover only cares about structural equality.

Underscores in TPTP names get folded to camelCase (`sk_c1` becomes
`skC1`) so the WL `Symbol[]` constructor accepts them. Names starting
with `$` (TPTP system symbols) get a `Tptp$` prefix for the same
reason.

## 7. The dispatch surface

```mathematica
TFindProof[File["path.p"], opts]           (* ATP, file *)
TFindProof["...cnf/fof source...", opts]   (* ATP, inline *)

THVMLink`SMT`TFindProofSMT[File["path.p"]] (* SMT, file *)
THVMLink`SMT`TFindProofSMT["...source..."] (* SMT, inline *)
```

`opts` is the usual `TFindProof` option set (see
`docs/tutorial/atp_methods.md` §3 / §4). All Method presets,
suboptions, and return specs work the same way as with WL-form
input — the importer is purely an alternative encoding path for the
same `Axioms / Conjecture` pair.

## 8. Where the code lives

- `wl/THVMLink/Kernel/ATP/TPTPImport.wl` -- the parser. ~390 LOC. Pure
  WL; no C-side dependency.
- `wl/THVMLink/Kernel/ATP.wl` -- the `TFindProof[File | string]`
  overloads + `tptpDispatch` helper that routes to the appropriate
  ATP entry.
- `wl/THVMLink/Kernel/ATP/SMT.wl` -- the parallel `TFindProofSMT[File
  | string]` overloads with the ground-input gate (see
  `docs/tutorial/smt.md`).
- `wl/THVMLink/Tests/atp_tptp.wlt` -- 11 `VerificationTest`s
  exercising the CNF / FOF / file / inline / no-conjecture /
  negated-conjecture / underscore-name / universal-quantifier paths.
- `tools/baselines/vampire_tptp/` -- a collection of TPTP-form
  benchmarks (one per WL `AxiomaticTheory` notable theorem) used by
  the parallel Vampire baseline harness; doubles as a regression
  corpus for the importer.

## 9. Extending coverage

For the standing UEQ benchmark corpus the current `cnf` + `fof`
subset is sufficient. Extensions toward the full TPTP grammar (the
TPTPWorld BNF reference) are tracked on a per-construct basis:

- Multi-literal `cnf` clauses (Horn / disjunctive). Mechanical
  extension of `clauseToEquation`. Useful for non-UEQ benchmarks.
- `fof` Boolean combinations (`&` / `\|` / `=>`). Routes naturally
  into `TFindProofSMT`'s DPLL(T) shell for the ground subset
  (`TSmtDecide` already handles Boolean combinations of equality
  atoms; the parser just needs to surface them).
- `tff` typed first-order. Needs the sort signature to be threaded
  through `atpEncodeProblem`; thvm has sort-check gating in the C
  engine but the WL surface currently assumes homogeneous mode.
- `include`. Mechanical: resolve the path relative to the importer
  file's directory, recursively `tptpImport` the included file, and
  splice the clause list.

Each is a localized change; the parser scanner is already structured
to dispatch new clause heads from `scanClauses`. Add the head to the
`Which` chain and a `consumeXxx` helper that returns the clause list.
