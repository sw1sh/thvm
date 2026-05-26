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

## 3. Supported grammar subset

The full TPTP grammar (`github.com/TPTPWorld/SyntaxBNF`, ~735 lines)
covers `cnf / fof / tff / thf / tcf / ncf / tpi`. The importer
currently handles **`cnf`** and **`fof`** clauses with a single
equational literal. Other clause heads (`tff` / `thf` / `tcf` / `ncf`
/ `tpi` / `include`) are skipped with a console warning so the rest
of the file still parses.

### 3.1 `cnf(name, role, formula).`

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

### 3.2 `fof(name, role, formula).`

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

### 3.3 Comments

Line comments (`% ...`) and block comments (`/* ... */`) are stripped
before parsing.

### 3.4 What's NOT supported

| Construct | Status |
|-|-|
| `cnf` multi-literal clauses (`l1 \| l2 \| ...`) | Not supported (UEQ is unit-equality, so this is fine for UEQ benchmarks). |
| `fof` with `&` / `\|` / `~` / `?` outside `! [...] :` | Not supported -- `tptpImport::badfmla`. |
| `tff` (typed first-order) | Skipped (`tptpImport::skipnoncnf`). |
| `thf` (typed higher-order) | Skipped. |
| `tcf` (typed cnf) / `ncf` / `tpi` | Skipped. |
| `include('path').` | Skipped. The caller must resolve includes by passing the assembled file. |
| Equational rewriting modulo theory annotations | Not part of UEQ; not supported. |

## 4. Output shape

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

## 5. Namespacing

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

## 6. The dispatch surface

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

## 7. Where the code lives

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

## 8. Extending coverage

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
