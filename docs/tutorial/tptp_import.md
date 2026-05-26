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

## 2. Loading

`TPTPImport` and the dispatch overloads live in `THVMLink\`ATP\`` --
the single load entry brings every ATP / SMT public symbol into scope
by bare name:

```mathematica
<< THVMLink`ATP`
```

(Equivalent to `Get["THVMLink\`ATP\`"]` / `Needs["THVMLink\`ATP\`"]`.)
All examples below assume this has run.

## 3. Quick start

### 3.1 Just parse, don't prove

```mathematica
TPTPImport["cnf(a, axiom, and(X, Y) = and(Y, X))."]
(* -> <|"Axioms" -> {"and"[v$_, w$_] == "and"[w$_, v$_]},
        "Conjecture" -> None|> *)
```

The parser returns an Association. Function-symbol names come back as
String heads (`"and"[X, Y]` etc.) so they cannot collide with any
user-level WL symbol -- see §7 below.

```mathematica
TPTPImport[File["AbelianGroupAxioms__InverseOfInverse.p"]]
(* -> <|"Axioms" -> {4 equational axioms},
        "Conjecture" -> "not"["not"["skC1"[]]] == "skC1"[]|> *)
```

Use the file overload for benchmark `.p` files. Underscored TPTP
names (`sk_c1`, `op_overtilde`) come through verbatim as Strings.

### 3.2 Inline string, ATP path

```mathematica
TFindProof[
    "cnf(a1, axiom, and(X, Y) = and(Y, X)).
     cnf(a2, axiom, and(X, and(Y, Z)) = and(and(X, Y), Z)).
     cnf(g,  negated_conjecture, and(and(p, q), r) != and(r, and(q, p)))."]
(* -> ProofObject[...] *)
```

Three clauses: two universally-quantified equational axioms
(commutativity + associativity of `and`) and a ground inequality
conjecture. The importer parses the clauses, the dispatch routes them
as `Axioms / Conjecture`, and the saturator closes the goal.

### 3.3 A file, ATP path

```mathematica
TFindProof[File["AbelianGroupAxioms__InverseOfInverse.p"],
    TimeConstraint -> 10]
(* -> ProofObject[...] *)
```

Same dispatch as the inline string. The file is read via
`Import[..., "Text"]` then handed to `TPTPImport`.

### 3.4 Use the parsed Association manually

```mathematica
imported = TPTPImport[File["MyProblem.p"]];
TFindProof[imported["Conjecture"], imported["Axioms"],
    Method -> "VampireUEQ", TimeConstraint -> 30]
```

If you want to pick a non-default `Method`, pass options to a
`TFindProof[conjecture, axioms, opts]` call directly -- the
File/string overloads always go through the default Automatic
dispatch.

### 3.5 Saturate (no conjecture)

```mathematica
TFindProof[
    "cnf(a1, axiom, mul(X, e) = X).
     cnf(a2, axiom, mul(e, X) = X).",
    TimeConstraint -> 5]
(* -> {Inactive[Equal][mul[v_, e], v_], Inactive[Equal][mul[e, v_], v_]} *)
```

When the input has no `conjecture` / `negated_conjecture` clause, the
dispatch falls through to `TFindProof[axioms]`'s single-argument
completion form: saturate the axioms within `TimeConstraint`, return
the completed rule set as a list of `Inactive[Equal]` equations.

### 3.6 Inline string, ground SMT path

```mathematica
TFindProofSMT[
    "cnf(a1, axiom, a = b).
     cnf(a2, axiom, b = c).
     cnf(g,  negated_conjecture, a != c)."]
(* -> <|"Status" -> "Proved", "Method" -> "CongruenceClosure", ...|> *)
```

Ground inputs (no `X`-style universal variables) route through
congruence closure for a near-linear-time decision instead of
saturation. Non-ground inputs are rejected with a clear message:

```mathematica
TFindProofSMT[
    "cnf(a, axiom, and(X, Y) = and(Y, X))."]
(* TFindProofSMT::nonground: ... -- use TFindProof instead. *)
(* -> $Failed *)
```

See `docs/tutorial/smt.md` for the SMT entry.

### 3.7 FOF (first-order form) with explicit universal

```mathematica
TPTPImport[
    "fof(comm, axiom, ! [X, Y] : (and(X, Y) = and(Y, X)))."]
(* -> <|"Axioms" -> {"and"[v$_, w$_] == "and"[w$_, v$_]}, ...|> *)
```

`fof` clauses with a leading `! [V1, ..., Vn] :` universal quantifier
(optionally wrapped in parens) parse the same as the equivalent
`cnf` form -- the quantifier is stripped, the body becomes a
universal equational axiom. Free variables in a bare `fof` (no
explicit quantifier) are also treated as universals.

## 4. Coverage matrix

### 4.0 What TPTP encodes

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

### 4.1 thvm coverage against each axis

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

### 4.2 Bottom line

For TPTP **UEQ** division benchmarks the importer + saturator is a
complete drop-in: every UEQ problem parses, and the standard ATP
techniques apply. For wider TPTP divisions, coverage tracks the
underlying engine: equational shape -> works; Boolean-combination
ground shape -> route through `TFindProofSMT`; everything else
(typed, higher-order, modal, general first-order with non-equality
predicates) is unsupported.

## 5. Supported grammar subset

The full TPTP grammar (`github.com/TPTPWorld/SyntaxBNF`, ~735 lines)
covers `cnf / fof / tff / thf / tcf / ncf / tpi`. The importer
currently handles **`cnf`** and **`fof`** clauses with a single
equational literal. Other clause heads (`tff` / `thf` / `tcf` / `ncf`
/ `tpi` / `include`) are skipped with a console warning so the rest
of the file still parses.

### 5.1 `cnf(name, role, formula).`

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

### 5.2 `fof(name, role, formula).`

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
`TPTPImport::badfmla`.

### 5.3 Comments

Line comments (`% ...`) and block comments (`/* ... */`) are stripped
before parsing.

### 5.4 What's NOT supported

| Construct | Status |
|-|-|
| `cnf` multi-literal clauses (`l1 \| l2 \| ...`) | Not supported (UEQ is unit-equality, so this is fine for UEQ benchmarks). |
| `fof` with `&` / `\|` / `~` / `?` outside `! [...] :` | Not supported -- `TPTPImport::badfmla`. |
| `tff` (typed first-order) | Skipped (`TPTPImport::skipnoncnf`). |
| `thf` (typed higher-order) | Skipped. |
| `tcf` (typed cnf) / `ncf` / `tpi` | Skipped. |
| `include('path').` | Skipped. The caller must resolve includes by passing the assembled file. |
| Equational rewriting modulo theory annotations | Not part of UEQ; not supported. |

## 6. Output shape

`TPTPImport` returns:

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

## 7. String heads, not namespaced Symbols

TPTP function-symbol names come back as bare String heads, not
Symbols in some private context. So the parsed axiom

```
cnf(a, axiom, and(X, Y) = and(Y, X)).
```

prints (via `InputForm`) as

```
"and"[v$_, w$_] == "and"[w$_, v$_]
```

Strings are not bound to anything in any WL context, so a TPTP `and`
or `p` cannot shadow a user-level `and` or `p` binding -- no
namespace dance needed. Nullary constants come back as `"a"[]` (empty
argument list) rather than the bare String `"a"` because WL
short-circuits `Equal` on distinct String atoms (`"a" == "b"` would
eagerly evaluate to `False`); the compound form `"a"[] == "b"[]`
stays unevaluated, matching the shape `TFindProof` and `TSatEUF`
expect.

Variables are clause-scoped uppercase identifiers; the parser builds
a fresh `Pattern[Unique["v"], Blank[]]` per occurrence, so the same
name (`X`) in different clauses gets different WL variables and the
axioms cannot accidentally cross-bind.

At dispatch time, the file/string overloads of `TFindProof` /
`TFindProofSMT` internally convert String heads to Symbols in a
private context (`THVMLink\`ATP\`Private\`Tptp$and` etc.) before
calling the encoder, since the WL `ProofObject` verifier expects
Symbol heads. The conversion is one-way: the parser's user-facing
output stays String-headed for clean `InputForm` display. Underscored
TPTP names (`sk_c1`, `op_overtilde`) get CamelCase-folded at
conversion time (`Tptp$skC1`, `Tptp$opOvertilde`) since `Symbol[]`
rejects identifiers containing `_`.

## 8. The dispatch surface

```mathematica
TFindProof[File["path.p"], opts]           (* ATP, file *)
TFindProof["...cnf/fof source...", opts]   (* ATP, inline *)

TFindProofSMT[File["path.p"]] (* SMT, file *)
TFindProofSMT["...source..."] (* SMT, inline *)
```

`opts` is the usual `TFindProof` option set (see
`docs/tutorial/atp_methods.md` §3 / §4). All Method presets,
suboptions, and return specs work the same way as with WL-form
input — the importer is purely an alternative encoding path for the
same `Axioms / Conjecture` pair.

## 9. Where the code lives

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

## 10. Extending coverage

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
  file's directory, recursively `TPTPImport` the included file, and
  splice the clause list.

Each is a localized change; the parser scanner is already structured
to dispatch new clause heads from `scanClauses`. Add the head to the
`Which` chain and a `consumeXxx` helper that returns the clause list.
