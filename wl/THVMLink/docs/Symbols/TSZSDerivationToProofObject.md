---
Template: Symbol
Name: TSZSDerivationToProofObject
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TSZSDerivationToProofObject
Keywords: [SZS, TPTP, theorem proving, ATP, derivation, proof steps, external prover]
SeeAlso: [TVampireProofObject, TWaldmeisterProofObject, TEproverProofObject, TTweeProofObject, TVampireProof, TWaldmeisterProof, TFindProof]
RelatedGuides: [THVMLink]
RelatedTutorials: ["[Methods and Presets for TFindProof](AtpMethods)"]
---

## Usage

<code>[TSZSDerivationToProofObject]()[*derivation*]</code> converts the list of inferences *derivation* of an external prover into an Association of proof steps keyed as in the proofs of [TFindProof]().

## Details & Options

- *derivation* is the `"Inferences"` list of [TVampireProof](), [TEproverProof]() or [TWaldmeisterProof](): one Association for each inference, with its `"Name"`, `"Role"`, `"Formula"`, `"Rule"` and `"Parents"`.
- The result has the keys `"Backend"` (`"SZS"`, for the SZS (Sutcliffe-Zimmer-Schulz) proof format), `"Status"`, `"Goal"`, `"Axioms"`, `"ProofDataset"`, `"ProofLength"` and `"RuleHistogram"`.
- `"ProofDataset"` keys each step as `{"Axiom", n}`, `{"Hypothesis", n}`, `{"CriticalPairLemma", n}`, `{"SubstitutionLemma", n}` or `{"Conclusion", 1}`, with its `"Statement"` and the steps it is derived from.
- The step type comes from the inference rule: `"superposition"` and `"resolution"` give a `"CriticalPairLemma"`, a demodulation or rewrite gives a `"SubstitutionLemma"`, and a final contradiction gives the `"Conclusion"`. The rule names of the E prover map the same way: `"spm"` and `"pm"` give a `"CriticalPairLemma"`, `"rw"` a `"SubstitutionLemma"` and `"cn"` the `"Conclusion"`. The Association `$SZSRuleToConstruct` holds this correspondence, and a rule not in it gives a `"SubstitutionLemma"`.
- Steps that only reorient or copy an equation are left out.
- `"RuleHistogram"` counts the inference rules of the derivation.
- The following option can be given:

| | | |
| --- | --- | --- |
| `"ParseFormulas"` | [False]() | whether to parse the formulas into expressions |

## Basic Examples

Write a Waldmeister problem:

```wl
file = Export[FileNameJoin[{$TemporaryDirectory, "involution.pr"}], "NAME involution\nMODE PROOF\nSORTS ANY\nSIGNATURE a: -> ANY\n          f: ANY -> ANY\nORDERING LPO\n         f > a\nVARIABLES x : ANY\nEQUATIONS f(f(x)) = x\nCONCLUSION f(f(f(a))) = f(a)\n", "Text"]
```

<!-- => the path of involution.pr in $TemporaryDirectory -->

The inferences of its Waldmeister proof:

```wl
inferences = TWaldmeisterProof[file]["Inferences"]
```

<!-- => {<|"Head" -> "wm", "Name" -> "1", "Role" -> "axiom", "Formula" -> "x1 = f(f(x1))", "Rule" -> "file", "Parents" -> {}|>, ..., <|"Head" -> "wm", "Name" -> "5", "Role" -> "axiom", "Formula" -> "f(a()) = f(a())", "Rule" -> "trivial_inequality_removal", "Parents" -> {"4"}|>} -->

Convert them into proof steps:

```wl
TSZSDerivationToProofObject[inferences]
```

<!-- => <|"Backend" -> "SZS", "Status" -> "Proved", "Goal" -> "f(f(f(a()))) = f(a())", "Axioms" -> {"x1 = f(f(x1))"}, "ProofDataset" -> <|{"Axiom", 1} -> ..., {"Hypothesis", 1} -> ..., {"SubstitutionLemma", 1} -> ..., {"Conclusion", 1} -> ...|>, "ProofLength" -> 4, "RuleHistogram" -> <|"forward_demodulation" -> 1, "orient" -> 1, "trivial_inequality_removal" -> 1|>|> -->

## Scope

Write the problem to a file:

```wl
file = Export[FileNameJoin[{$TemporaryDirectory, "involution.p"}], "cnf(involution, axiom, f(f(X)) = X).\ncnf(goal, negated_conjecture, f(f(f(a))) != f(a)).\n", "Text"]
```

<!-- => the path of involution.p in $TemporaryDirectory -->

Convert the inferences of a Vampire proof:

```wl
TSZSDerivationToProofObject[TVampireProof[file, TimeConstraint -> 5]["Inferences"]]
```

<!-- => <|"Backend" -> "SZS", "Status" -> "Proved", "Goal" -> ..., "Axioms" -> {"(\n  ( ! [X0] : (f(f(X0)) = X0) ))"}, "ProofDataset" -> <|{"Axiom", 1} -> ..., {"Hypothesis", 1} -> ..., {"Conclusion", 1} -> ...|>, "ProofLength" -> 2, "RuleHistogram" -> <|"forward_subsumption_resolution" -> 1|>|> -->

---

The rules of a Waldmeister proof of a group theorem:

```wl
TSZSDerivationToProofObject[TWaldmeisterProof[FileNameJoin[{Directory[], "tools", "baselines", "wm_pr", "GroupAxioms__LeftIdentity.pr"}]]["Inferences"]]["RuleHistogram"]
```

<!-- => <|"forward_demodulation" -> 3, "orient" -> 8, "superposition" -> 4, "trivial_inequality_removal" -> 1|> -->

## Options

### ParseFormulas

Write the problem to a file:

```wl
file = Export[FileNameJoin[{$TemporaryDirectory, "involution.pr"}], "NAME involution\nMODE PROOF\nSORTS ANY\nSIGNATURE a: -> ANY\n          f: ANY -> ANY\nORDERING LPO\n         f > a\nVARIABLES x : ANY\nEQUATIONS f(f(x)) = x\nCONCLUSION f(f(f(a))) = f(a)\n", "Text"]
```

<!-- => the path of involution.pr in $TemporaryDirectory -->

With `"ParseFormulas" -> True`, the formulas are parsed into expressions:

```wl
TSZSDerivationToProofObject[TWaldmeisterProof[file]["Inferences"], "ParseFormulas" -> True]["Goal"]
```

<!-- => "f"["f"["f"["a"[]]]] == "f"["a"[]] -->

## Properties and Relations

The correspondence between inference rules and step types:

```wl
$SZSRuleToConstruct
```

<!-- => <|"file" -> "Axiom", "superposition" -> "CriticalPairLemma", "forward_demodulation" -> "SubstitutionLemma", ..., "paramodulation" -> "CriticalPairLemma", "spm" -> "CriticalPairLemma", "pm" -> "CriticalPairLemma", "rw" -> "SubstitutionLemma", "cn" -> "Conclusion"|> -->

---

[TWaldmeisterProofObject]() runs the prover and the conversion together:

```wl
TWaldmeisterProofObject["GroupAxioms", "LeftIdentity", "LiftToProofObject" -> False]["RuleHistogram"]
```

<!-- => <|"forward_demodulation" -> 3, "orient" -> 8, "superposition" -> 4, "trivial_inequality_removal" -> 1|> -->

## Possible Issues

Input that is not a list returns unevaluated:

```wl
TSZSDerivationToProofObject["proof"]
```

<!-- => TSZSDerivationToProofObject["proof"] -->
