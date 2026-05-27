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

A single parsed problem is an [Association]() with `"Axioms"` and `"Conjecture"` keys. Variables keep their original TPTP names ([Pattern]()s named `X`, `Y`, `Z`); function symbols come back as [String]()-headed compounds so they cannot shadow user bindings. Inspect the axioms of an abelian-group problem:

```wl
Module[{problem = <|
    "Axioms" -> {
        "multiply"["e", X_, X_],
        "multiply"[X_, "e", X_],
        "multiply"[X_, "inverse"[X_], "e"],
        "multiply"["inverse"[X_], X_, "e"]},
    "Conjecture" -> Equal[
        "multiply"["a", "b", "c"],
        "multiply"["c", "b", "a"]]|>},
    problem["Axioms"]
]
```

---

Extract the leading function symbol of every equational axiom with [Cases]() to see which operators a problem axiomatises. Three axioms of three different operators come back as a list of three [String]() heads:

```wl
Module[{axioms = {
    "and"[X_, Y_] == "and"[Y_, X_],
    "or"[X_, "or"[Y_, Z_]] == "or"["or"[X_, Y_], Z_],
    "not"["not"[X_]] == X_}},
    Cases[axioms, Equal[h_[___], _] :> h]
]
```

---

The TPTP domain prefix (the first three letters of every problem name) groups problems by mathematical subject. Look up one in the `"Domains"` content element:

```wl
Module[{domains = <|
    "GRP" -> "Group theory", "BOO" -> "Boolean algebra",
    "RNG" -> "Ring theory",  "LCL" -> "Logic calculi",
    "SET" -> "Set theory",   "TOP" -> "Topology"|>},
    domains["GRP"]
]
```

---

The `"EntityStore"` content element wraps the corpus in an [EntityStore](). Each entity carries the parsed `"Axioms"` and `"Conjecture"` plus metadata (`"Domain"`, `"AxiomCount"`, `"Status"`, `"Rating"`). The store's [Association]() of entities lives at `store[[1]]["TPTPProblem", "Entities"]`; iterate over it directly without registering the store. Find the three smallest group-theory problems:

```wl
Module[{store, entities},
    store = EntityStore[<|"TPTPProblem" -> <|
        "Label" -> "TPTP problem",
        "Properties" -> AssociationMap[<|"Label" -> ToLowerCase[#]|> &,
            {"Domain", "AxiomCount", "Status"}],
        "Entities" -> <|
            "GRP001-1" -> <|"Domain" -> "GRP", "AxiomCount" -> 9,
                "Status" -> "unsatisfiable"|>,
            "GRP001-4" -> <|"Domain" -> "GRP", "AxiomCount" -> 4,
                "Status" -> "unsatisfiable"|>,
            "GRP002-1" -> <|"Domain" -> "GRP", "AxiomCount" -> 4,
                "Status" -> "unsatisfiable"|>|>|>|>];
    entities = store[[1]]["TPTPProblem", "Entities"];
    TakeSmallestBy[
        Select[Values @ entities, #["Domain"] === "GRP" &],
        #["AxiomCount"] &, 3]
]
```

---

Aggregate axiom counts by domain to see which areas of mathematics carry the heaviest axiomatic load in the corpus:

```wl
Module[{entities = <|
    "GRP001-1" -> <|"Domain" -> "GRP", "AxiomCount" -> 9|>,
    "GRP001-4" -> <|"Domain" -> "GRP", "AxiomCount" -> 4|>,
    "BOO001-1" -> <|"Domain" -> "BOO", "AxiomCount" -> 5|>,
    "RNG001-1" -> <|"Domain" -> "RNG", "AxiomCount" -> 7|>|>},
    ReverseSort @ GroupBy[Values @ entities,
        #["Domain"] &, Total[#[[All, "AxiomCount"]]] &]
]
```

## Hero Image

```wl
Module[{counts = <|
    "GRP" -> 1230, "BOO" -> 421,  "RNG" -> 612, "LCL" -> 904,
    "SET" -> 1108, "TOP" -> 318,  "ALG" -> 487, "FLD" -> 256,
    "PUZ" -> 199,  "SYN" -> 2110|>},
    BarChart[Sort @ counts,
        ChartLabels -> Automatic, BarOrigin -> Left,
        AxesLabel -> {"problems", None},
        ImageSize -> 600, AspectRatio -> 1]
]
```
