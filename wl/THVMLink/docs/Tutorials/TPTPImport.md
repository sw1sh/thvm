---
Template: TechNote
Name: TPTPImport
Title: Importing TPTP Benchmarks
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/tutorial/TPTPImport
Keywords: [TPTP, CNF, FOF, benchmark, import, theorem proving, Sheffer, AbelianGroupAxioms]
RelatedGuides: [THVMLink]
RelatedTutorials: [ATP, SMT, Overview]
---

## What the TPTP path covers

The TPTP (Thousands of Problems for Theorem Provers) benchmark suite is the standard cross-prover problem corpus - Vampire, E, Twee, Waldmeister all run on it - and ATP literature quotes timings against TPTP slugs like `LCL129-1.p` or `ShefferAxioms/AndAssociativity.p`. A reader who wants to reproduce a result, or who already has a problem in TPTP form, needs an importer.

[TPTPImport](paclet:Wolfram/WolframParser/ref/TPTPImport) parses a TPTP file or string into an Association of `"Axioms"` and `"Conjecture"`. Better yet, [TFindProof]() accepts `File["..."]` or an inline TPTP string directly, so the parse happens behind the dispatch:

- Variable-bearing axioms route to the default unfailing Knuth-Bendix completion.
- Ground problems route through congruence closure when you add `Method -> "SMT"`.
- A goal-less file gives back the saturated rule set via the single-argument completion form.

Function-symbol names come back as String heads (<code>"and"[x_, y_]</code> rather than <code>and[x_, y_]</code>) so they cannot collide with any user-level Wolfram symbol.

## Setting up

The TPTP parser itself - [TPTPImport](paclet:Wolfram/WolframParser/ref/TPTPImport) - lives in the <code>[Wolfram\`Parser\`](paclet:Wolfram/WolframParser/guide/WolframParser)</code> paclet. The dispatch overloads on [TFindProof]() that accept a `File` or string call it through transparently, so loading just <code>THVMLink\`ATP\`</code> is enough for the file / string entry points to work:

```wl
Needs["WolframInstitute`THVMLink`ATP`"];
Needs["Wolfram`Parser`"];
```

Reach for `Needs["Wolfram`Parser`"]` explicitly only when you want to call [TPTPImport](paclet:Wolfram/WolframParser/ref/TPTPImport) directly to inspect the parsed Association before dispatch.

## Parsing a problem

The simplest call - just parse, don't prove - shows the output shape:

```wl
TPTPImport["cnf(a, axiom, and(X, Y) = and(Y, X))."]
```
<!-- => <|"Axioms" -> {"and"[v$_, w$_] == "and"[w$_, v$_]}, "Conjecture" -> None|> -->

Universal variables (`X`, `Y` in the TPTP source) come through as the Wolfram pattern-variable convention (`v$_`, `w$_`) so they bind correctly against the saturation engine's matcher. The file overload eats benchmark `.p` files directly:

```wl
TPTPImport["% TPTP problem: GroupAxioms::InverseOfInverse
% axioms = 3, conjuncts total = 1
cnf(ax1, axiom, and(X1,and(X2,X3)) = and(and(X1,X2),X3)).
cnf(ax2, axiom, and(X1,op_overtilde(k1)) = X1).
cnf(ax3, axiom, and(X1,not(X1)) = op_overtilde(k1)).
cnf(goal, negated_conjecture, not(not(sk_c1)) != sk_c1).
"]
```
<!-- => <|"Axioms" -> {4 equational axioms}, "Conjecture" -> "not"["not"["skC1"[]]] == "skC1"[]|> -->

Underscored TPTP names (`sk_c1`, `op_overtilde`) come through verbatim as String heads. Skolem constants - introduced by the TPTP tooling when a goal is universally quantified - sit ready to drop into the conjecture or as ground witnesses.

## Proving from a file or string

The same TPTP source can ride straight into [TFindProof]():

```wl
TFindProof[
    "cnf(a1, axiom, and(X, Y) = and(Y, X)).
     cnf(a2, axiom, and(X, and(Y, Z)) = and(and(X, Y), Z)).
     cnf(g,  negated_conjecture, and(and(p, q), r) != and(r, and(q, p)))."]
```
<!-- => ProofObject[...] -->

Three clauses: two universally-quantified equational axioms (commutativity and associativity of `and`) and a ground inequality conjecture. The dispatch parses the clauses, routes them as `Axioms` / `Conjecture`, and the saturator closes the goal.

Files behave the same way:

```wl
TFindProof["% TPTP problem: AbelianGroupAxioms::InverseOfInverse
% axioms = 4, conjuncts total = 1
cnf(ax1, axiom, and(X1,and(X2,X3)) = and(and(X1,X2),X3)).
cnf(ax2, axiom, and(X1,X2) = and(X2,X1)).
cnf(ax3, axiom, and(X1,op_overtilde(k1)) = X1).
cnf(ax4, axiom, and(X1,not(X1)) = op_overtilde(k1)).
cnf(goal, negated_conjecture, not(not(sk_c1)) != sk_c1).
", TimeConstraint -> 10]
```
<!-- => ProofObject[...] -->

If you want a non-default `Method`, pull the parsed shape out and feed it back through the standard two-argument call:

```wl
imported = TPTPImport["% TPTP problem: AbelianGroupAxioms::InverseOfInverse
% axioms = 4, conjuncts total = 1
cnf(ax1, axiom, and(X1,and(X2,X3)) = and(and(X1,X2),X3)).
cnf(ax2, axiom, and(X1,X2) = and(X2,X1)).
cnf(ax3, axiom, and(X1,op_overtilde(k1)) = X1).
cnf(ax4, axiom, and(X1,not(X1)) = op_overtilde(k1)).
cnf(goal, negated_conjecture, not(not(sk_c1)) != sk_c1)."];
TFindProof[imported["Conjecture"], imported["Axioms"],
    Method -> "VampireUEQ", TimeConstraint -> 30]
```

Goal-less inputs - no `conjecture` or `negated_conjecture` clause - fall through to the single-argument completion form: saturate the axioms within the budget, return the completed rule set as a list of `Inactive[Equal]` equations.

```wl
TFindProof[
    "cnf(a1, axiom, mul(X, e) = X).
     cnf(a2, axiom, mul(e, X) = X).",
    TimeConstraint -> 5]
```
<!-- => {Inactive[Equal][mul[v_, e], v_], Inactive[Equal][mul[e, v_], v_]} -->

Ground problems route through [TFindProof]() with `Method -> "SMT"` just as cleanly - the importer is shared; only the dispatch differs. See the [SMT](paclet:WolframInstitute/THVMLink/tutorial/SMT) tech note for the ground/QF_UF path.

## Coverage

What TPTP encodes vs what thvm consumes:

| TPTP form         | Status | Notes                                         |
|-------------------|--------|-----------------------------------------------|
| `cnf` equations   | Full   | Each `=`/`!=` literal -> an axiom / goal      |
| `cnf` non-equality literals | Partial | Universal `p(...)` and Skolemized goals route through; predicate-only clauses become equational `p(...) = TRUE` |
| `fof` equational  | Full   | Universal quantifier handled via WL `Pattern`  |
| `fof` Boolean structure | Partial | `&`, `\|`, `~`, `=>`, `<=>` translated to WL Boolean equivalents |
| `fof` mixed quantifiers | Skolemized | Existentials get fresh Skolem constants     |
| `tff`/`tcf`/`thf` | Skeleton | Type/term distinction parsed; type annotations dropped before dispatch |
| `include('path')` | Full   | Recursive include; relative to source file    |
| Sequents          | Partial | `lhs --> rhs` routes as goals; one-side empty supported |
| Annotations / comments | Full | `% ...` and `/* ... */` stripped; <code>inference(...)</code> annotations skipped |

The grammar follows the canonical [TPTPWorld BNF](https://github.com/TPTPWorld/SyntaxBNF); the parser is implemented over `EBNFParse` so coverage extends naturally as further productions become useful for the benchmarks at hand.

## String heads vs Wolfram symbols

The importer returns function applications as String-headed compound terms (<code>"f"[x_, y_]</code>) rather than as `Symbol`-headed (`f[x_, y_]`). This is deliberate:

- TPTP symbol names regularly collide with `System` symbols (`p`, `f`, `c`, `Plus`, etc.) and with user-defined ones. String heads sidestep every namespace concern.
- The engine's term canonicalizer normalizes Strings the same way it normalizes Symbols, so there is no downstream cost.
- A round-trip through [TFindProof]() preserves the String shape - all output (proof object, lemmas, raw trace) refers to the same Strings, so a downstream pipeline can pattern-match against the original TPTP names.

If you want to coerce a particular String head into a Symbol after the fact, `expr /. h_String[args___] :> Symbol[h][args]` is the one-liner.

## Where the code lives

- [TPTPImport](paclet:Wolfram/WolframParser/ref/TPTPImport) and its `EBNFParse`-driven grammar sit in the [Wolfram/WolframParser](paclet:Wolfram/WolframParser/guide/WolframParser) paclet.  See the [Parsing TPTP](paclet:Wolfram/WolframParser/tutorial/ParsingTPTP) tech note for the grammar internals.
- `wl/THVMLink/Kernel/ATP/ATP.wl` - the `TFindProof[File[...]]` / `TFindProof[String]` dispatch overloads.
- `wl/THVMLink/Kernel/ATP/SMT.wl` - the parallel `Method -> "SMT"` congruence-closure dispatch (see the [SMT](paclet:WolframInstitute/THVMLink/tutorial/SMT) tech note).
- `wl/THVMLink/Tests/atp_tptp.wlt` - end-to-end coverage on representative `.p` files from the UEQ division.

Extending coverage to a new TPTP form is mostly about adding productions to the BNF grammar in Wolfram/WolframParser; the dispatch surface above usually picks the new shape up without changes.
