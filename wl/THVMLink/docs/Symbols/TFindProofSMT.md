---
Template: Symbol
Name: TFindProofSMT
Context: THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TFindProofSMT
Keywords: [SMT, congruence closure, QF_UF, entailment, TPTP, decision procedure]
SeeAlso: [TSatEUF, TSmtDecide, TFindProof, FindEquationalProof]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TFindProofSMT]()[$goal$, $hypotheses$]</code> decides the ground entailment <code>$hypotheses$ |= $goal$</code> by reducing it to a QF_UF satisfiability query: assert each hypothesis, assert the negation of $goal$, and run congruence closure.  Returns a small `ProofObject`-shaped Association on UNSAT (entailment holds), `$Failed` on SAT (counter-model exists).

<code>[TFindProofSMT]()["...$cnf$ / $fof$ string..."]</code> parses a TPTP fragment via [TPTPImport](paclet:Wolfram/WolframParser/ref/TPTPImport) and dispatches the same way.  <code>[TFindProofSMT]()[File["..."]]</code> reads the file then routes through the same parser.

## Details & Options

- Single equality / disequality goals dispatch to [TSatEUF]() (pure congruence closure).
- Boolean-combination goals dispatch to [TSmtDecide]() (lazy DPLL(T) + congruence closure).  The returned Association's `"Method"` key discloses which engine handled the call (`"CongruenceClosure"` or `"DPLL(T)+CongruenceClosure"`).
- A ground-gate rejects inputs with universally-quantified variables (`Pattern` / `Blank` heads).  Congruence closure is a quantifier-free decision procedure; for variable-bearing axioms reach for [TFindProof]() (an unfailing Knuth-Bendix completion engine).
- `Method -> "SMT"` on the standard [TFindProof]() entry point routes through this dispatcher instead of the saturator.

## Examples

### Basic examples

Single-literal entailment:

```wl
TFindProofSMT[a == c, {a == b, b == c}]
```
<!-- => <|"Status" -> "Proved", "Method" -> "CongruenceClosure", ...|> -->

Boolean-combination goal (handed to the DPLL(T) loop):

```wl
TFindProofSMT[Implies[a == b && b == c, a == c]]
```
<!-- => <|"Status" -> "Proved", "Method" -> "DPLL(T)+CongruenceClosure", ...|> -->

A non-entailment returns `$Failed`:

```wl
TFindProofSMT[a == c, {a == b}]
```
<!-- => $Failed -->

### TPTP overloads

A `cnf` fragment as a literal string:

```wl
TFindProofSMT[
    "cnf(a1, axiom, a = b).
     cnf(a2, axiom, b = c).
     cnf(g, negated_conjecture, a != c)."]
```
<!-- => <|"Status" -> "Proved", ...|> -->

A `.p` file from disk:

```wl
TFindProofSMT[File["tools/baselines/vampire_tptp/AbelianGroupAxioms__InverseOfInverse.p"]]
```

A non-ground TPTP fragment is rejected:

```wl
TFindProofSMT[
    "cnf(a1, axiom, and(X, Y) = and(Y, X)).
     cnf(g, negated_conjecture, and(a, b) != and(b, a))."]
```
<!-- => TFindProofSMT::nonground; reach for TFindProof for variable-bearing axioms -->

## Properties & Relations

- The decision procedure is sound and complete for the ground QF_UF fragment (and for Boolean combinations of QF_UF atoms via the DPLL(T) lift).  No quantifier reasoning.
- [TFindProof]() with `Method -> "SMT"` is the same dispatcher reached through the equational entry point.  The result Association is identical; the entry differs only in whether you're declaring intent up-front.
- For variable-bearing axioms, [TFindProof]() (Knuth-Bendix completion) is the right tool.  The TPTP overload here will reject quantified clauses with a `TFindProofSMT::nonground` message.
- TPTP-side parsing is shared with [TFindProof]()'s file/string overload; both call into [TPTPImport](paclet:Wolfram/WolframParser/ref/TPTPImport) from the [Wolfram/WolframParser](paclet:Wolfram/WolframParser/guide/WolframParser) paclet.

## Possible Issues

- Inputs use Wolfram's `Equal` / `Unequal` heads; `Inactive` wrapping is not required.
- The TPTP overload's parser is the same one [TFindProof]() uses; benchmarks that lean on `tff` / `tcf` / `thf` type machinery come through as the underlying first-order skeleton (types are dropped before dispatch).
- The result Association is `$Failed` (not a SAT-shaped Association) when the entailment does not hold.  Use [TSmtDecide]() directly if you want the satisfying assignment for the counter-model.
