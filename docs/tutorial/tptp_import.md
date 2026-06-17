# TPTP Import

A practitioner's guide to the TPTP (Thousands of Problems for Theorem
Provers) input path: how to point `TFindProof` at a benchmark `.p`
file or an inline string, what subset of the TPTP grammar is supported,
and how the imported clauses become the `Axioms / Conjecture` shape
thvm's ATP and SMT entries consume.

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

The importer also lets `TFindProof` — both the default equational
saturator and the `Method -> "SMT"` QF_UF decision procedure (see
`docs/tutorial/smt.md`) — accept a `File["..."]` or inline string
without forcing the caller to hand-translate clauses to WL.

## 2. Loading

`TPTPImport` and the dispatch overloads live in `THVMLink\`ATP\`` --
the single load entry brings every ATP / SMT public symbol into scope
by bare name:

```mathematica
<< WolframInstitute`THVMLink`ATP`
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
TFindProof[
    "cnf(a1, axiom, a = b).
     cnf(a2, axiom, b = c).
     cnf(g,  negated_conjecture, a != c).", Method -> "SMT"]
(* -> <|"Status" -> "Proved", "Method" -> "CongruenceClosure", ...|> *)
```

Under `Method -> "SMT"`, ground inputs (no `X`-style universal
variables) route through congruence closure for a near-linear-time
decision instead of saturation. Non-ground inputs are rejected with a
clear message:

```mathematica
TFindProof[
    "cnf(a, axiom, and(X, Y) = and(Y, X)).", Method -> "SMT"]
(* TFindProof::nonground: ... -- drop Method -> "SMT". *)
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
| `cnf` (multi-literal disjunction) | YES | NO at the UEQ layer | Returns `Or[lit1, lit2, ...]`. Predicate atoms and negated literals (`~atom`) supported. The saturator is UEQ-only so it cannot consume `Or[...]` directly; route through `TFindProof[..., Method -> "SMT"]` for ground inputs. |
| `fof` (single `! [...] : (l = r)` or bare equation) | YES | YES | Iter 4 (commit 98bf9243). |
| `fof` (full Boolean combination: `&` / `\|` / `~` / `=>` / `<=` / `<=>` / `<~>` / `~&` / `~\|`) | YES | YES at the SMT layer | Returns nested `And` / `Or` / `Not` / `Implies` / `Equivalent` / `Xor` (with `~&` -> `Not[And[..]]`, `~\|` -> `Not[Or[..]]`). `TSmtDecide` handles ground Boolean combinations of `Equal`/`Unequal` atoms via DPLL(T). |
| `fof` (existentials) | YES (structural) | NO | Inner `? [V1, ..., Vn] : body` becomes `Exists[{v1, ...}, body]` with bound variables as fresh `Unique[]` symbols (proper scoping via snapshot+restore of the per-clause var map). Leading universals (`! [...]`) are still stripped; inner `! [...]` becomes `ForAll[...]`. Downstream consumers must skolemize for ATP/SMT use. |
| `fof` (predicate atoms, `$true`/`$false`, `$`-defined functions) | YES | At SMT for ground propositions | Predicates parse as `"p"[args]` terms; `$true` / `$false` lift to `True` / `False`. `$sum`, `$less`, `$distinct`, `$ite`, etc. parse as generic `"$name"[args]` compounds via the `$`-in-identifier-charset path. |
| `tff` (typed first-order) | YES | YES (when sort distinctions are irrelevant) | `tff(name, type, ...)` signature declarations are silently skipped. Sort annotations (`X:srt`) are stripped from quantifier var lists and atoms, so `! [X:nat] : p(X)` parses identically to its untyped fof equivalent. Sound for homogeneous-untyped use; problems that rely on type distinctions for soundness will mis-prove. |
| `tcf` (typed cnf) | YES | NO at the UEQ layer | Same as `tff` for typing; same as `cnf` for the body grammar (single literal or `Or` of literals). |
| `thf` (higher-order) | YES (structural) | NO | `thf(name, type, ...)` signature declarations skipped. Formula bodies parse with the fof Boolean grammar plus `^ [V1, ..., Vn] : body` lambda (-> WL `Function[{...}, body]`) and `@` left-associative application (`f @ x @ y` -> `f[x][y]`, with a special-case collapse of `"f"[][...]` to `"f"[...]`). Sort annotations stripped. No higher-order semantic engine on the downstream side. |
| `ncf` (non-classical) | YES (structural) | NO | `ncf(name, role, formula)` parses via the fof Boolean grammar; modal operators like `$box(p)`, `$dia(p)` ride the `$`-defined path as generic compounds. No modal-logic decision procedure on the downstream side. |
| `tpi` (process instruction) | NO (silently skipped) | n/a | Meta-directive; ignored. |
| `include('...')` and `include('...', [name1, ...])` | YES | n/a | Path resolved relative to the importing file's directory; falls back to `$TPTP` and `$TPTP/Problems` env-var roots. Included clauses splice into the enclosing scan; the optional clause-name selector filters which clauses are admitted. Nested includes work via per-file directory threading. |

**Atomic / term-level coverage:**

| Construct | Parser support | Output shape |
|-|-|-|
| Bare identifier `foo` | YES | `"foo"[]` |
| Variable (`X`, `Var2`) | YES | `Pattern[Unique["v"], Blank[]]` |
| Single-quoted atom `'a b c'` | YES | `"a b c"[]` (contents become the String head; backslash escapes honoured) |
| Double-quoted distinct object `"foo"` | YES | `"\"foo\""[]` (literal quotes preserved in head to distinguish from plain atom) |
| Unsigned integer `42` | YES | `"42"[]` |
| Signed integer `-42` | YES | `"-42"[]` |
| Rational `3/4` | YES | `"3/4"[]` |
| Real `3.14` | YES | `"3.14"[]` |
| Scientific `1.5e-3` | YES | `"1.5e-3"[]` |
| `$true` / `$false` (formula context) | YES | `True` / `False` (lifted by `liftConstant`) |
| `$true` / `$false` (term context) | YES | `"$true"[]` / `"$false"[]` (generic constant) |
| `$sum`, `$less`, etc. ($-defined function) | YES | `"$sum"[args]` (generic compound) |
| Sequent `lhs1, lhs2 --> rhs1, rhs2` | YES | `Implies[And[lhs1, lhs2], Or[rhs1, rhs2]]` |
| `$let` / `$ite_f` with formula args | NO (parses but malformed-arg-list guard bails) | Use term-arg variants. |

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
ground shape -> route through `TFindProof[..., Method -> "SMT"]`; everything else
(typed, higher-order, modal, general first-order with non-equality
predicates) is unsupported.

## 5. Supported grammar subset

The full TPTP grammar (`github.com/TPTPWorld/SyntaxBNF`, ~735 lines)
covers `cnf / fof / tff / thf / tcf / ncf / tpi`. The importer
handles **`cnf`**, **`fof`**, **`tff`**, **`tcf`** clause heads plus
**`include`** directives. Remaining heads (`thf` higher-order,
`ncf` non-classical, `tpi` meta-directive) are skipped with a console
warning so the rest of the file still parses.

### 5.1 `cnf(name, role, formula).`

```
cnf(<name>, <role>, <literal>).
cnf(<name>, <role>, <literal1> | <literal2> | ... | <literalN>).
```

- `<name>` is a clause identifier (consumed and discarded).
- `<role>` is one of `axiom | hypothesis | lemma | conjecture |
  negated_conjecture`. The axiom-shaped roles fold into `Axioms`;
  `conjecture` becomes `Conjecture`. `negated_conjecture` carries
  the negation of the goal, so the importer un-negates: a single
  `lhs != rhs` flips to `lhs == rhs`; a disjunction of disequations
  flips to the corresponding conjunction of equations; other shapes
  get a plain `Not[...]` wrapper.
- `<literal>` is `lhs = rhs`, `lhs != rhs`, a predicate atom
  `p(args)`, or `~ atom` (negated atom). Variables are clause-scoped
  uppercase identifiers, each renamed to a fresh
  `Pattern[Unique[], Blank[]]`.
- A single literal is returned in its bare WL form (`Equal[...]`,
  `Unequal[...]`, `"p"[args]`, `Not[...]`); multi-literal clauses
  return `Or[lit1, lit2, ...]`.

### 5.2 `fof(name, role, formula).`

The full first-order Boolean grammar. Connectives parse with
left-associative `&` and `|`, and (right-associative for the
implementation; TPTP requires parenthesisation when chained) the
non-associative connectives `<=>`, `=>`, `<=`, `<~>`, `~|`, `~&`:

| TPTP | WL output |
|-|-|
| `~ phi` | `Not[phi]` (or folded for equational atoms: `~ (a=b)` -> `Unequal[a,b]`) |
| `phi & psi` | `And[phi, psi]` (flat for chains) |
| `phi \| psi` | `Or[phi, psi]` (flat for chains) |
| `phi => psi` | `Implies[phi, psi]` |
| `phi <= psi` | `Implies[psi, phi]` |
| `phi <=> psi` | `Equivalent[phi, psi]` |
| `phi <~> psi` | `Xor[phi, psi]` |
| `phi ~& psi` | `Not[And[phi, psi]]` |
| `phi ~\| psi` | `Not[Or[phi, psi]]` |
| `! [V1, ..., Vn] : body` | leading universal stripped; inner -> `ForAll[{...}, body]` |
| `? [V1, ..., Vn] : body` | `Exists[{...}, body]` with fresh bare Symbol bound vars |
| `lhs = rhs`, `lhs != rhs` | `Equal[...]` / `Unequal[...]` |
| `p(args)`, `$true`, `$false` | `"p"[args]`, `True`, `False` |

The leading `! [V1, ..., Vn] :` universal binding is peeled off
because the cnf default (Pattern-variable universals) already
expresses universal binding. Free variables in a bare fof body are
also treated as universals. Inner `!` / `?` quantifiers keep their
`ForAll` / `Exists` wrappers so the SMT path sees the full structure.

### 5.3 `tff(name, role, formula).`

Typed first-order. Two clause shapes:

- `tff(name, type, sym: srt).` -- signature declaration, silently
  skipped (no semantic content for the homogeneous-untyped saturator).
- `tff(name, role, formula).` -- same grammar as `fof`, with sort
  annotations (`X:srt`, `f(X:srt)`, etc.) stripped at preprocessing.
  Sound for homogeneous-untyped use; problems whose soundness depends
  on type distinctions will mis-prove.

### 5.4 `tcf(name, role, formula).`

Typed cnf -- the cnf grammar from §5.1, with `tff`-style sort
annotations stripped. Same `type` / `axiom` / `conjecture` /
`negated_conjecture` role handling.

### 5.5 `thf(name, role, formula).`

Typed higher-order. Type declarations skipped like `tff`. Formula
bodies use the fof Boolean grammar plus two thf-specific extensions:

- `^ [V1, ..., Vn] : body` -- lambda abstraction. Produces WL
  `Function[{v1, ..., vn}, body]` (HoldAll-safe via `With`).
- `f @ x @ y` -- left-associative explicit application. Produces
  `f[x][y]` (curried). The first application against a 0-arity
  constant collapses `"f"[][x]` to `"f"[x]` so the surface shape
  matches the equivalent `f(x)` term form.

### 5.6 `ncf(name, role, formula).`

Non-classical formulas (modal, intuitionistic). The parser handles
these identically to `fof` -- the standard `$box` / `$dia` modal
operators ride the `$`-defined identifier path and come out as
generic `"$box"[arg]`, `"$dia"[arg]` compounds. There is no modal
semantic engine on the downstream side; the output shape is for
downstream consumers that want to do their own translation.

### 5.7 `include('path').` / `include('path', [name1, ...]).`

The included file's clauses splice into the enclosing scan.
Path resolution order:

1. The `path` string as given (absolute paths and "exists as-is").
2. Relative to the current file's directory (or to `Directory[]` for
   inline strings).
3. Relative to the `$TPTP` environment variable, if set.
4. Relative to `$TPTP/Problems/`.

The optional clause-name selector `[name1, name2, ...]` admits only
clauses whose name appears in the list; without a selector every
clause is admitted. Nested includes work via per-file directory
threading.

### 5.8 Sequents

The `lhs1, lhs2, ... --> rhs1, rhs2, ...` sequent form rewrites to
the equivalent implication `Implies[And[lhs_i], Or[rhs_j]]`. Each
side may optionally be wrapped in `[...]` (the TPTP `fof_tuple`
syntax). An empty lhs becomes `True`; an empty rhs becomes `False`.

### 5.9 Comments

Line comments (`% ...`) and block comments (`/* ... */`) are stripped
before parsing.

### 5.10 What's NOT supported

| Construct | Status |
|-|-|
| `cnf` / `fof` with non-equational predicates fed into the UEQ saturator | Parsed (as `"p"[args]` terms / `Or[...]`), but the default `TFindProof` saturator will reject -- it is unit-equational. Use `TFindProof[..., Method -> "SMT"]` for ground Boolean combinations. |
| `tpi` (process instruction) | Silently skipped. |
| `$let` / `$ite_f` with formula args | Mostly parsed via the generic compound path; formulas as term arguments would wedge the parser, so the anti-loop guard in `readArgs` bails out on the first non-comma after a parse step, returning a partial term. |
| Equational rewriting modulo theory annotations | Not part of UEQ; not supported. |
| Predicates in Boolean combos at the SMT layer | Parser produces the right shape; `TSmtDecide`'s `collectAtoms` currently only picks up `Equal`/`Unequal` atoms. Bare predicates ride through structure but don't participate in T-checking. |

## 6. Output shape

`TPTPImport` returns:

```mathematica
<|
    "Axioms"     -> {l1 == r1, l2 == r2, ...},
    "Conjecture" -> l == r   (* or None when no conjecture clause *)
|>
```

`TFindProof` consumes this directly (with `Method -> "SMT"` for the
ground congruence-closure path). When `Conjecture` is `None`,
`TFindProof[file]` falls through to the single-argument completion form
(saturate the axioms, return the derived lemmas).

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

At dispatch time, the file/string overloads of `TFindProof`
internally convert String heads to Symbols in a private context
(`THVMLink\`ATP\`Private\`Tptp$and` etc.) before
calling the encoder, since the WL `ProofObject` verifier expects
Symbol heads. The conversion is one-way: the parser's user-facing
output stays String-headed for clean `InputForm` display. Underscored
TPTP names (`sk_c1`, `op_overtilde`) get CamelCase-folded at
conversion time (`Tptp$skC1`, `Tptp$opOvertilde`) since `Symbol[]`
rejects identifiers containing `_`.

## 8. The dispatch surface

```mathematica
TFindProof[File["path.p"], opts]                  (* ATP, file *)
TFindProof["...cnf/fof source...", opts]          (* ATP, inline *)

TFindProof[File["path.p"], Method -> "SMT"]       (* SMT, file *)
TFindProof["...source...", Method -> "SMT"]       (* SMT, inline *)
```

`opts` is the usual `TFindProof` option set (see
`docs/tutorial/atp_methods.md` §3 / §4). All Method presets,
suboptions, and return specs work the same way as with WL-form
input — the importer is purely an alternative encoding path for the
same `Axioms / Conjecture` pair.

## 9. Where the code lives

- `wl/THVMLink/Kernel/ATP/TPTPImport.resource.wl` -- the parser
  implementation. Self-contained, bare top-level definitions, no
  package shell -- the body that the Wolfram Function Repository
  resource notebook scrapes. Recursive-descent over the Boolean
  precedence layers (binary connectives, `|`, `&`, unary
  `~`/quantifier prefixes, atomic formulas) with a separate path
  for cnf (`|`-split disjunction of literals) and pre-pass
  sort-stripping for tff/tcf/thf.
- `wl/THVMLink/Kernel/ATP/TPTPImport.wl` -- the in-tree wrapper.
  `BeginPackage`s `WolframInstitute`THVMLink`ATP``, pre-declares `TPTPImport` in the
  public context, and `Get`s the resource from inside `Private` so
  helpers land in the private context while `TPTPImport` resolves
  to the public symbol. One source of truth shared between thvm
  and the deployed Function Repository resource.
- `wl/THVMLink/Kernel/ATP/TPTPImport.md` -- the WFR authoring
  document. Inlines the resource implementation via the
  `MarkdownToNotebook` `#| file:` cell option; runs through
  `ResourceFunction["MarkdownToNotebook"]` to produce the
  Function-Repository-shaped `.nb` for submission.
- `wl/THVMLink/Kernel/ATP.wl` -- the `TFindProof[File | string]`
  overloads + `tptpDispatch` helper that routes to the appropriate
  ATP entry.
- `wl/THVMLink/Kernel/ATP/SMT.wl` -- the `tptpDispatchSMT` ground-input
  gate behind `Method -> "SMT"` (see `docs/tutorial/smt.md`).
- `wl/THVMLink/Tests/atp_tptp.wlt` -- 30 `VerificationTest`s
  exercising the cnf / fof / tff / tcf / thf / ncf / include /
  sequent paths plus the inline-string / file / no-conjecture /
  negated-conjecture / underscore-name / universal-quantifier /
  Boolean-connective / existential / multi-literal / tpi-skip /
  numeric-literal / quoted-atom / distinct-object / include-selector
  / $-defined / lambda-application / anti-loop-guard paths.
- `tools/baselines/vampire_tptp/` -- a collection of TPTP-form
  benchmarks (one per WL `AxiomaticTheory` notable theorem) used by
  the parallel Vampire baseline harness; doubles as a regression
  corpus for the importer.

## 10. Extending coverage

The parser now covers every clause head TPTPWorld's BNF defines
(`cnf` / `fof` / `tff` / `tcf` / `thf` / `ncf` plus `include`),
the full Boolean / quantifier / lambda / `@`-application grammar,
sequent rewrites, quoted atoms, distinct objects, signed numeric
literals, and `$`-defined predicates. Remaining work is downstream
(the prover and SMT layers), not parser:

- Predicate atoms inside Boolean combinations at the SMT layer. The
  parser already emits `"p"[args]` terms for non-equational atoms;
  `TSmtDecide`'s `collectAtoms` walks for `Equal` / `Unequal` only.
  Extending it to also pick up bare predicate atoms (with
  predicate-Boolean abstraction) opens up non-equality QF_UF ground
  goals.
- Skolemization of inner `?` quantifiers when the conjecture is a
  positive existential. The parser preserves `Exists` structure; a
  pre-encoder pass would replace existential bound vars with Skolem
  terms before encoding into the C engine.
- Sort-aware `tff` mode that threads sort signatures through to the
  C engine's sort-check gating, instead of stripping annotations.
- Higher-order solver for `thf` lambdas / `@` application beyond pure
  structural parsing.
- Modal-logic decision procedure for `ncf` modal operators.

These are all on the prover / SMT side; the parser's `<|"Axioms",
"Conjecture"|>` shape is the bridge each will plug into.
