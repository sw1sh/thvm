---
Template: Example
ResourceType: Example
Name: TPTP Problem Library
Description: Parsed automated-theorem-proving problems from the TPTP library, indexed by domain and ready for FindEquationalProof
ContributedBy: Nikolay Murzin, Claude (Anthropic)
Keywords: [TPTP, theorem proving, ATP, automated reasoning, equational logic, first-order logic, problem library, benchmarks]
Categories: [Computer Science, Mathematics]
RelatedSymbols: [FindEquationalProof, ResourceFunction, EntityStore, AxiomaticTheory]
Links: ["[TPTP project (Thousands of Problems for Theorem Provers)](https://tptp.org/)", "[TPTP problem syntax (Sutcliffe 2009)](https://tptp.org/Seminars/TPTPWorldTutorial/LogicTPTP.html)", "[TPTPWorld syntax BNF](https://github.com/TPTPWorld/SyntaxBNF)", "[ProofAtlas TPTP subset](https://github.com/lammdachs/proofatlas-tptp-subset)"]
---

The [TPTP (Thousands of Problems for Theorem Provers)](https://tptp.org/) library is the standard cross-prover benchmark corpus for automated theorem proving. Vampire, E, Twee, Waldmeister, and every research ATP system use it for evaluation. This resource exposes a parsed, [Wolfram Language](https://www.wolfram.com/language/)-native form of the CNF/FOF fragment of TPTP v9.0.0 (the [ProofAtlas subset](https://github.com/lammdachs/proofatlas-tptp-subset), 13,178 problems under 64 kB each across 48 problem domains), ready to hand directly to [FindEquationalProof]() or to a custom saturator.

Each problem comes as an [Association]() with parsed `Axioms` (a [List]() of equations / formulas, variables preserved by name) and `Conjecture` (a single formula or [None]()). The corpus is also wrapped in an [EntityStore]() so domain-/status-/rating-based queries work without scanning the whole set.

## Content

The corpus is exposed under three content elements: `"Problems"` (the full Association keyed by problem name), `"Domains"` (the TPTP domain-code -> description map), and `"EntityStore"` (a query-able [EntityStore]() with one entity per problem).

```wl
#| eval: false
ResourceData[ResourceObject[EvaluationNotebook[]], "Problems"] = $tptpProblems;
```

```wl
#| eval: false
ResourceData[ResourceObject[EvaluationNotebook[]], "Domains"] = $tptpDomains;
```

```wl
#| eval: false
ResourceData[ResourceObject[EvaluationNotebook[]], "EntityStore"] = $tptpEntityStore;
```

## Examples

A single problem is an [Association]() with `"Axioms"` and `"Conjecture"` keys. Variables appear with their original TPTP names ([Pattern]()s of `X`, `Y`, `Z` rather than auto-generated symbols), function symbols come back as [String]()-headed compounds so they cannot shadow user bindings:

```wl
$tptpProblems = <|
    "GRP001-4" -> <|
        "Domain" -> "GRP",
        "Axioms" -> {
            "multiply"["e_1", X_, X_],
            "multiply"[X_, "e_2", X_],
            "multiply"[X_, "inverse"[X_], "e_1"],
            "multiply"["inverse"[X_], X_, "e_2"]
        },
        "Conjecture" -> Equal["multiply"["a", "b", "c"], "multiply"["c", "b", "a"]]
    |>
|>;
$tptpProblems["GRP001-4"]["Axioms"]
```

---

Hand a parsed problem straight to [FindEquationalProof](): the corpus is the unit-equality fragment most equational provers target, so the axioms-and-conjecture shape is exactly what the built-in expects:

```wl
problem = <|
    "Axioms" -> {
        "and"[X_, Y_] == "and"[Y_, X_],
        "and"[X_, "and"[Y_, Z_]] == "and"["and"[X_, Y_], Z_]
    },
    "Conjecture" -> Equal[
        "and"["and"["p", "q"], "r"],
        "and"["r", "and"["q", "p"]]]
|>;
FindEquationalProof[problem["Conjecture"], problem["Axioms"]]
```

---

The TPTP domain prefix (the first three letters of every problem name) groups problems by mathematical subject. Look up one with the `"Domains"` element:

```wl
$tptpDomains = <|
    "GRP" -> "Group theory", "BOO" -> "Boolean algebra",
    "RNG" -> "Ring theory", "LCL" -> "Logic calculi",
    "SET" -> "Set theory",  "TOP" -> "Topology"|>;
$tptpDomains["GRP"]
```

---

The `"EntityStore"` element wraps the corpus as a query-able [EntityStore](). Find the ten group-theory problems with the smallest axiom set:

```wl
$tptpEntityStore = EntityStore[<|"TPTPProblem" -> <|
    "Label" -> "TPTP problem",
    "Properties" -> AssociationMap[<|"Label" -> ToLowerCase[#]|> &,
        {"Name", "Domain", "AxiomCount", "Status", "Rating"}],
    "Entities" -> <|
        "GRP001-1" -> <|"Domain" -> "GRP", "AxiomCount" -> 9,
            "Status" -> "unsatisfiable", "Rating" -> 0.0|>,
        "GRP001-4" -> <|"Domain" -> "GRP", "AxiomCount" -> 4,
            "Status" -> "unsatisfiable", "Rating" -> 0.0|>,
        "GRP002-1" -> <|"Domain" -> "GRP", "AxiomCount" -> 4,
            "Status" -> "unsatisfiable", "Rating" -> 0.0|>|>|>|>];
TakeSmallestBy[
    Select[EntityList[$tptpEntityStore["TPTPProblem"]],
        #["Domain"] === "GRP" &],
    #["AxiomCount"] &, 3]
```

---

Aggregate axiom counts by domain to see which areas of mathematics dominate the corpus:

```wl
ReverseSortBy[
    GroupBy[
        EntityValue[
            EntityList[$tptpEntityStore["TPTPProblem"]],
            {"Domain", "AxiomCount"}],
        First -> Last,
        Total],
    Identity]
```

## Hero Image

```wl
BarChart[
    Sort @ KeyTake[
        Counts @ Lookup[
            Values @ $tptpProblems, "Domain"],
        Keys[$tptpDomains]],
    ChartLabels -> Automatic, BarOrigin -> Left,
    AxesLabel -> {"problems", None},
    ImageSize -> 600, AspectRatio -> 1]
```
