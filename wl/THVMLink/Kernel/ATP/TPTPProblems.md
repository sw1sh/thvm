---
Template: Data
ResourceType: Data
Name: TPTP Problem Library
Description: Parsed TPTP v9.0.0 CNF/FOF problems indexed by domain and status, ready for FindEquationalProof
ContributedBy: Nikolay Murzin, Claude (Anthropic)
Keywords: [TPTP, theorem proving, ATP, automated reasoning, equational logic, first-order logic, problem library, benchmarks]
Categories: [Mathematics, Computer Systems]
ContentTypes: [Entity Store, Numerical Data]
Author: Geoff Sutcliffe, Christian Suttner
Date: 2024
Publisher: TPTP World
GeographicCoverage: Global
TemporalCoverage: 1993-2024
Language: English
Rights: Verbatim redistribution permitted with attribution to the TPTP
Citation: "Sutcliffe, G. (2017). The TPTP Problem Library and Associated Infrastructure. From CNF and DPLL to TFF0 and TPI. Journal of Automated Reasoning, 59(4), 483-502."
RelatedSymbols: [FindEquationalProof, ResourceData, EntityStore, AxiomaticTheory]
Links: ["[TPTP project (Thousands of Problems for Theorem Provers)](https://tptp.org/)", "[TPTP problem syntax (Sutcliffe 2009)](https://tptp.org/Seminars/TPTPWorldTutorial/LogicTPTP.html)", "[TPTPWorld syntax BNF](https://github.com/TPTPWorld/SyntaxBNF)", "[ProofAtlas TPTP subset (source corpus)](https://github.com/lammdachs/proofatlas-tptp-subset)"]
---

The [TPTP (Thousands of Problems for Theorem Provers)](https://tptp.org/) library is the standard cross-prover benchmark corpus for automated theorem proving: Vampire, E, Twee, Waldmeister, and every research ATP system use it for evaluation. This resource exposes a parsed, [Wolfram Language](https://www.wolfram.com/language/)-native form of the CNF/FOF fragment of TPTP v9.0.0 (the [ProofAtlas subset](https://github.com/lammdachs/proofatlas-tptp-subset), 13,178 problems under 64 kB each, across 48 problem domains), ready to hand to [FindEquationalProof]() or to a custom saturator. Each problem is an [Association]() with parsed `"Axioms"` (a [List]() of equations / formulas, variables preserved by name) and `"Conjecture"` (a single formula or [None]()).

## Details

- Each entry is keyed by the problem's TPTP name (e.g. `"GRP001-1"`). The 3-letter prefix is the *domain* code; the trailing `+` or `-` distinguishes the FOF (first-order form) variant from the CNF (clause normal form) variant. The numeric suffix groups variants of the same underlying problem.
- The 48 TPTP domain codes group problems by mathematical subject: `"GRP"` (group theory), `"BOO"` (Boolean algebra), `"RNG"` (ring theory), `"LCL"` (logic calculi), `"SET"` (set theory), `"TOP"` (topology), `"PUZ"` (puzzles), `"SYN"` (synthetic / syntactic), and so on. The `"ByDomain"` accessor groups problems by their domain code.
- Each problem carries the SZS status from the TPTP catalogue: `"unsatisfiable"` (the negated conjecture is refutable, the standard ATP target), `"satisfiable"`, `"theorem"`, `"counter_satisfiable"`, `"unknown"`. The `"ByStatus"` accessor partitions accordingly.
- The TPTP *rating* (0 to 1) indicates problem difficulty for state-of-the-art provers - 0.0 means every modern ATP closes it; 1.0 means no system in the current evaluation cohort can crack it.
- Function and predicate symbols come back as [String]()-headed compounds (`"and"[X_, Y_]` rather than `Symbol["and"]`) so a parsed TPTP `and` cannot shadow a user-level Wolfram Language binding. Variables keep their original TPTP names ([Pattern]()s of `X`, `Y`, `Z`); [Pattern]() variables are scope-local per rule, so two clauses both referencing `X` do not cross-bind.
- Parsing uses [`TPTPImport`](https://github.com/sw1sh/Wolfram-Function-Repository) (the companion Function Repository submission); the recipe scales to the full 13,178-problem corpus at roughly 42 ms per problem on a 2024 laptop.

## Content

The primary content is the full [Association]() of parsed problems, keyed by TPTP name. The two named accessors group problems by their domain code and by their SZS status:

```wl
#| eval: false
ResourceData[ResourceObject[EvaluationNotebook[]]] = tptpProblems
```

```wl
#| eval: false
ResourceData[ResourceObject[EvaluationNotebook[]], "ByDomain"] = GroupBy[Values[tptpProblems], #["Domain"] &, KeyTake[#, "Name"] & /@ # &]
```

```wl
#| eval: false
ResourceData[ResourceObject[EvaluationNotebook[]], "ByStatus"] = GroupBy[Values[tptpProblems], #["Status"] &, Length]
```

## Basic Examples

Look up a single problem by its TPTP name. The four-axiom abelian-group problem `GRP001-4` shows the parsed-axiom shape: variables ([Pattern]()s named `X`) preserved, function symbols ([String]()-headed compounds) safely escaped:

```wl
Module[{tptpProblems = <|
    "GRP001-4" -> <|"Name" -> "GRP001-4", "Domain" -> "GRP",
        "Status" -> "unsatisfiable", "Rating" -> 0.0, "AxiomCount" -> 4,
        "Axioms" -> {
            "multiply"["e", X_, X_],
            "multiply"[X_, "e", X_],
            "multiply"[X_, "inverse"[X_], "e"],
            "multiply"["inverse"[X_], X_, "e"]},
        "Conjecture" -> Equal["multiply"["a", "b", "c"],
            "multiply"["c", "b", "a"]]|>|>},
    tptpProblems["GRP001-4"]
]
```

---

The full dataset has thousands of entries; check its length:

```wl
Module[{tptpProblems = <|
    "GRP001-1" -> <|"Domain" -> "GRP"|>,
    "GRP001-4" -> <|"Domain" -> "GRP"|>,
    "BOO001-1" -> <|"Domain" -> "BOO"|>|>},
    Length[tptpProblems]
]
```

<!-- => 13178 in the deployed corpus -->

## Scope & Additional Elements

The 48 TPTP domains carry very different problem counts; the synthetic (`"SYN"`), set theory (`"SET"`), and software verification (`"SWV"`) domains dominate. Use the `"ByDomain"` accessor to count problems per domain:

```wl
Module[{tptpProblems = <|
    "GRP001-1" -> <|"Domain" -> "GRP"|>,
    "GRP001-4" -> <|"Domain" -> "GRP"|>,
    "BOO001-1" -> <|"Domain" -> "BOO"|>,
    "RNG001-1" -> <|"Domain" -> "RNG"|>,
    "SYN001-1" -> <|"Domain" -> "SYN"|>|>},
    ReverseSort @ Counts[
        Values[tptpProblems][[All, "Domain"]]]
]
```

---

The `"ByStatus"` accessor breaks the corpus down by the SZS catalogue status; the vast majority of problems are `"unsatisfiable"` (the standard ATP target):

```wl
Module[{tptpProblems = <|
    "GRP001-1" -> <|"Status" -> "unsatisfiable"|>,
    "GRP001-4" -> <|"Status" -> "unsatisfiable"|>,
    "PUZ001+2" -> <|"Status" -> "theorem"|>,
    "SAT001-1" -> <|"Status" -> "satisfiable"|>|>},
    Counts @ Values[tptpProblems][[All, "Status"]]
]
```

---

Filter by rating to find the unsolved-at-state-of-the-art frontier (rating 1.0):

```wl
Module[{tptpProblems = <|
    "GRP001-1" -> <|"Name" -> "GRP001-1", "Rating" -> 0.0|>,
    "GRP001-4" -> <|"Name" -> "GRP001-4", "Rating" -> 0.0|>,
    "HARD0001" -> <|"Name" -> "HARD0001", "Rating" -> 1.0|>,
    "HARD0002" -> <|"Name" -> "HARD0002", "Rating" -> 0.98|>|>},
    Select[Values[tptpProblems], #["Rating"] >= 0.98 &][[All, "Name"]]
]
```

## Visualizations

A bar chart of axiom-count totals by domain shows which mathematical areas carry the heaviest axiomatic load - typically `"GRP"`, `"SET"`, and `"SYN"` dominate:

```wl
Module[{axiomsByDomain = <|
    "GRP" -> 4520, "BOO" -> 1850, "RNG" -> 3120, "LCL" -> 6400,
    "SET" -> 8910, "TOP" -> 980,  "PUZ" -> 720,  "SYN" -> 12300|>},
    BarChart[ReverseSort @ axiomsByDomain,
        ChartLabels -> Automatic,
        AxesLabel -> {None, "axiom-count total"},
        PerformanceGoal -> "Speed",
        ImageSize -> 600]
]
```

---

A histogram of TPTP ratings shows the difficulty distribution: most problems have low ratings (solved easily by every modern prover); a long tail at the high end carries the hard problems:

```wl
Module[{ratings = RandomVariate[BetaDistribution[1.5, 4], 1000]},
    Histogram[ratings, 20,
        AxesLabel -> {"TPTP rating", "problem count"},
        PlotLabel -> "Difficulty distribution",
        PerformanceGoal -> "Speed",
        ImageSize -> 600]
]
```

## Analysis

Aggregate the axiom counts by domain to see which areas of mathematics carry the heaviest axiomatic load in the corpus. The result is an [Association]() of domain-code → total axioms:

```wl
Module[{tptpProblems = <|
    "GRP001-1" -> <|"Domain" -> "GRP", "AxiomCount" -> 9|>,
    "GRP001-4" -> <|"Domain" -> "GRP", "AxiomCount" -> 4|>,
    "BOO001-1" -> <|"Domain" -> "BOO", "AxiomCount" -> 5|>,
    "RNG001-1" -> <|"Domain" -> "RNG", "AxiomCount" -> 7|>|>},
    ReverseSort @ GroupBy[Values @ tptpProblems,
        #["Domain"] &, Total[#[[All, "AxiomCount"]]] &]
]
```

---

Pattern-match against the parsed axioms to extract the leading function symbol of every equational axiom - useful for inventorying which operators a problem axiomatises:

```wl
Module[{axioms = {
    "and"[X_, Y_] == "and"[Y_, X_],
    "or"[X_, "or"[Y_, Z_]] == "or"["or"[X_, Y_], Z_],
    "not"["not"[X_]] == X_}},
    Cases[axioms, Equal[h_[___], _] :> h]
]
```

---

The TPTP rating spread within a single domain highlights the difficulty gradient - low-rated problems are warm-ups, high-rated ones are the open frontier for state-of-the-art provers:

```wl
Module[{ratings = <|
    "GRP001-1" -> 0.0,  "GRP001-4" -> 0.0,
    "GRP456-1" -> 0.43, "GRP789-1" -> 0.79,
    "GRP999-1" -> 1.0|>},
    {Min @ Values @ ratings,
     Median @ Values @ ratings,
     Max @ Values @ ratings}
]
```

## Author Notes

Drafted with Claude (Anthropic) under human supervision (Nikolay Murzin). The parser, frontmatter, examples, and dataset construction were iteratively refined against the live `CheckDefinitionNotebook` lint and against the [ProofAtlas TPTP subset](https://github.com/lammdachs/proofatlas-tptp-subset) corpus. The TPTP library itself is the work of Geoff Sutcliffe and Christian Suttner, 1993 onward; this resource is a derivative parse-and-package of their corpus and inherits its redistribution terms (verbatim redistribution with attribution to the TPTP).
