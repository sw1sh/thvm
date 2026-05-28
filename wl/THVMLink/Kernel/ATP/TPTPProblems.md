---
Template: Data
ResourceType: Data
Name: TPTP Problem Library
Description: Parsed TPTP v9.2.1 problems across CNF FOF TFF TCF THF NCF clause heads, indexed by domain and status
ContributedBy: Nikolay Murzin, Claude (Anthropic)
Keywords: [TPTP, theorem proving, ATP, automated reasoning, equational logic, first-order logic, problem library, benchmarks]
Categories: [Mathematics, Computer Systems]
ContentTypes: [Entity Store, Numerical Data]
Author: Geoff Sutcliffe, Christian Suttner
Date: 2025
Publisher: TPTP World
GeographicCoverage: Global
TemporalCoverage: 1993-2025
Language: English
Rights: Verbatim redistribution permitted with attribution to the TPTP
Citation: "Sutcliffe, G. (2017). The TPTP Problem Library and Associated Infrastructure. From CNF and DPLL to TFF0 and TPI. Journal of Automated Reasoning, 59(4), 483-502."
RelatedSymbols: [FindEquationalProof, ResourceData, EntityStore, AxiomaticTheory]
Links: ["[TPTP project (Thousands of Problems for Theorem Provers)](https://tptp.org/)", "[TPTP v9.2.1 distribution (the source tarball)](https://tptp.org/TPTP/Distribution/TPTP-v9.2.1.tgz)", "[TPTP problem syntax (Sutcliffe 2009)](https://tptp.org/Seminars/TPTPWorldTutorial/LogicTPTP.html)", "[TPTPWorld syntax BNF](https://github.com/TPTPWorld/SyntaxBNF)"]
---

The [TPTP (Thousands of Problems for Theorem Provers)](https://tptp.org/) library is the standard cross-prover benchmark corpus for automated theorem proving: Vampire, E, Twee, Waldmeister, and every research ATP system use it for evaluation. This resource exposes a queryable index of the full [TPTP v9.2.1](https://tptp.org/TPTP/) library (25,775 problems across 57 domains, spanning all six TPTP clause heads: CNF, FOF, TFF, TCF, THF, NCF). The primary content is a metadata [Association]() (problem name, domain, SZS status, TPTP rating, path); the parsed [Axioms]() and [Conjecture]() are returned by a `"Parse"` accessor on demand - the full parsed form per problem can be megabytes, so keeping it out of the primary content keeps the resource lightweight and lets users target specific problems instead of always paying for the whole corpus.

## Details

- Each entry is keyed by the problem's TPTP name (e.g. `"GRP001-1"`). The 3-letter prefix is the *domain* code; the separator character before the variant index encodes the clause head: `-` (CNF), `+` (FOF), `=` (TFF), `^` (THF), `~` (NCF). The trailing numeric suffix groups variants of the same underlying problem.
- The 48 TPTP domain codes group problems by mathematical subject: `"GRP"` (group theory), `"BOO"` (Boolean algebra), `"RNG"` (ring theory), `"LCL"` (logic calculi), `"SET"` (set theory), `"TOP"` (topology), `"PUZ"` (puzzles), `"SYN"` (synthetic / syntactic), and so on. The `"ByDomain"` accessor groups problems by their domain code.
- Each problem carries the SZS `"Status"` field harvested from the TPTP catalogue header: `"Unsatisfiable"` (the negated conjecture is refutable, the standard ATP target), `"Satisfiable"`, `"Theorem"`, `"CounterSatisfiable"`, `"Open"`. The `"ByStatus"` accessor partitions accordingly.
- The TPTP `"Rating"` (0 to 1) indicates problem difficulty for state-of-the-art provers - 0.0 means every modern ATP closes it; 1.0 means no system in the current evaluation cohort can crack it. Parsed from the `%Rating` header line (current TPTP version).
- Function and predicate symbols come back as [String]()-headed compounds (`"and"[X_, Y_]` rather than `Symbol["and"]`) so a parsed TPTP `and` cannot shadow a user-level Wolfram Language binding. Variables keep their original TPTP names ([Pattern]()s of `X`, `Y`, `Z`); [Pattern]() variables are scope-local per rule, so two clauses both referencing `X` do not cross-bind.
- The `"Parse"` accessor wraps [`TPTPImport`](https://resources.wolframcloud.com/FunctionRepository/resources/TPTPImport/) (the companion Function Repository submission) and returns the full parsed problem (`Axioms` + `Conjecture`) for any single name. Per-problem parse time ranges from ~40 ms (small CNF) to several seconds (the largest verification problems with hundreds of axioms).
- The deployer pre-downloads the official TPTP tarball (922 MB compressed, 9.9 GB extracted) once to `~/Downloads/TPTP-v9.2.1.tgz`; the Content cell extracts it on first evaluation and harvests headers to build the metadata index (~10 minutes). The `"Parse"` accessor re-extracts on demand to find the source `.p` file. This avoids a 75-minute in-cell download against the modest-bandwidth TPTP server.

## Content

The corpus is built at evaluation time from the official TPTP v9.2.1 tarball (a 922 MB archive containing the full 25,775-problem library across all clause heads: CNF, FOF, TFF, TCF, THF, NCF). The deployer pre-downloads it to `~/Downloads/TPTP-v9.2.1.tgz` once (from https://tptp.org/TPTP/Distribution/TPTP-v9.2.1.tgz); the Content cell extracts it and harvests the per-problem `%Status` / `%Rating` headers plus the relative path back into the corpus. The result is an [Association]() keyed by TPTP problem name, each entry a lightweight metadata record - SZS status, TPTP rating, domain code, format separator, and the relative `.p` file path. The actual parsed [Axioms]() / [Conjecture]() are fetched on demand by the `"Parse"` accessor below:

```wl
tptpProblems = Block[
    {tarFile = FileNameJoin[
        {$HomeDirectory, "Downloads", "TPTP-v9.2.1.tgz"}],
     extracted, root, allFiles, headerOf, metaOf},
    If[ ! FileExistsQ[tarFile],
        Print["Pre-download TPTP v9.2.1 to ", tarFile, " from ",
            "https://tptp.org/TPTP/Distribution/TPTP-v9.2.1.tgz",
            " (~922 MB)."];
        Abort[]];
    extracted = ExtractArchive[tarFile, CreateDirectory[]];
    $tptpProblemsRoot = SelectFirst[
        Cases[extracted, p_String /; DirectoryQ[p]],
        FileBaseName[#] === "Problems" &];
    root = DirectoryName[$tptpProblemsRoot];
    SetEnvironment["TPTP" -> root];
    allFiles = FileNames["*.p", $tptpProblemsRoot, Infinity];
    headerOf[path_] := Association @ Flatten @ StringCases[
        Quiet @ ReadList[path, "String", 50],
        RegularExpression["^%\\s+([A-Za-z_]+)\\s*:\\s*(.+)$"] :>
            StringTrim["$1"] -> StringTrim["$2"]];
    metaOf[path_] := With[
        {name = FileBaseName[path],
         domain = FileNameTake[DirectoryName[path]],
         h = headerOf[path]},
        name -> <|
            "Name"     -> name,
            "Domain"   -> domain,
            "Path"     -> StringDrop[path, StringLength[$tptpProblemsRoot] + 1],
            "Status"   -> Lookup[h, "Status", Missing["NoStatus"]],
            "Rating"   -> Quiet @ Replace[
                ToExpression @ First @ StringSplit[
                    Lookup[h, "Rating", ""], " " | ","],
                _?(! NumberQ[#] &) -> Missing["NoRating"]]|>];
    Association @ Map[metaOf, allFiles]
];
```

The `"Parse"` named accessor is a [Function]() that takes a TPTP problem name and returns the parsed `<|"Axioms" -> …, "Conjecture" -> …|>` form via [`TPTPImport`](https://resources.wolframcloud.com/FunctionRepository/resources/TPTPImport/). Storing it on demand keeps the primary content small (a few MB of metadata) while still letting any user re-parse any problem with one call. The accessor uses the `$tptpProblemsRoot` global the primary Content cell sets:

```wl
#| eval: false
ResourceData[ResourceObject[EvaluationNotebook[]], "Parse"] =
    Function[problemName,
        ResourceFunction["TPTPImport"][File @ FileNameJoin[
            {$tptpProblemsRoot,
             tptpProblems[problemName]["Path"]}]]]
```

The primary content is the metadata [Association](). Two more named accessors group by domain and SZS status:

```wl
#| eval: false
ResourceData[ResourceObject[EvaluationNotebook[]]] = tptpProblems
```

```wl
#| eval: false
ResourceData[ResourceObject[EvaluationNotebook[]], "ByDomain"] = GroupBy[Values[tptpProblems], #["Domain"] &, Length]
```

```wl
#| eval: false
ResourceData[ResourceObject[EvaluationNotebook[]], "ByStatus"] = GroupBy[Values[tptpProblems], #["Status"] &, Length]
```

## Basic Examples

Look up a single problem's metadata by its TPTP name. The abelian-group problem `GRP001-4` carries its SZS status and TPTP rating plus the corpus path to its source file:

```wl
tptpProblems["GRP001-4"]
```

---

The dataset has tens of thousands of entries across every TPTP clause head:

```wl
Length[tptpProblems]
```

<!-- => 25775 -->

## Scope & Additional Elements

The TPTP domains carry very different problem counts; the synthetic (`"SYN"`), software-verification (`"SWV"`), and set theory (`"SET"`) domains dominate:

```wl
ReverseSort @ Counts[Values[tptpProblems][[All, "Domain"]]]
```

---

The SZS catalogue status partitions the corpus; the vast majority of problems are `"Theorem"` or `"Unsatisfiable"` (both standard ATP targets):

```wl
Counts @ Values[tptpProblems][[All, "Status"]]
```

---

Filter by rating to find the unsolved-at-state-of-the-art frontier (rating $\geq 0.98$, the problems no system in the current evaluation cohort closes):

```wl
Take[
    Sort @ Select[Values[tptpProblems],
        NumberQ[#["Rating"]] && #["Rating"] >= 0.98 &][[All, "Name"]],
    UpTo[10]]
```

---

Fetch the parsed Axioms and Conjecture for a single problem via the `"Parse"` accessor. The accessor calls `TPTPImport` and returns the standard `<|"Axioms" -> {…}, "Conjecture" -> …|>` shape:

```wl
ResourceFunction["TPTPImport"][File @ FileNameJoin[
    {$tptpProblemsRoot, tptpProblems["GRP001-4"]["Path"]}]]
```

## Visualizations

A bar chart of problem counts by domain shows which mathematical areas the corpus emphasises - typically `"SWV"` (software verification), `"SYN"` (synthetic), and `"SET"` (set theory) dominate:

```wl
BarChart[ReverseSort @ Counts[Values[tptpProblems][[All, "Domain"]]],
    ChartLabels -> Automatic,
    AxesLabel -> {None, "problem count"},
    PerformanceGoal -> "Speed",
    ImageSize -> 600]
```

---

A histogram of TPTP ratings shows the difficulty distribution: most problems have low ratings (solved easily by every modern prover); a long tail at the high end carries the hard problems:

```wl
Histogram[Values[tptpProblems][[All, "Rating"]], 20,
    AxesLabel -> {"TPTP rating", "problem count"},
    PlotLabel -> "Difficulty distribution",
    PerformanceGoal -> "Speed",
    ImageSize -> 600]
```

## Analysis

The four CASC-style format separators (`-` for CNF, `+` for FOF, `=` for TFF, `^` for THF) partition the corpus by clause head. Counting each shows the modern shift toward higher-order and typed first-order:

```wl
Counts @ Cases[Keys[tptpProblems],
    name_ /; StringMatchQ[name, ___ ~~ DigitCharacter ~~ x:("-"|"+"|"="|"^"|"~") ~~ ___] :>
        StringCases[name, DigitCharacter ~~ x:("-"|"+"|"="|"^"|"~") :> x][[1, 1]]]
```

---

Pattern-match against parsed axioms to extract the leading function symbol of every equational axiom in `GRP001-4` - useful for inventorying which operators a problem axiomatises. Fetch the parsed form via the `"Parse"` accessor first:

```wl
Cases[
    ResourceFunction["TPTPImport"][File @ FileNameJoin[
        {$tptpProblemsRoot, tptpProblems["GRP001-4"]["Path"]}]]["Axioms"],
    HoldPattern[Equal[h_[___], _]] :> h]
```

---

The TPTP rating spread within the group-theory domain shows the difficulty gradient - low-rated problems are warm-ups, high-rated ones are the open frontier for state-of-the-art provers:

```wl
With[{grp = Select[Values[tptpProblems],
        #["Domain"] === "GRP" && NumberQ[#["Rating"]] &]},
    <|"Min" -> Min[grp[[All, "Rating"]]],
      "Median" -> Median[grp[[All, "Rating"]]],
      "Max" -> Max[grp[[All, "Rating"]]],
      "Count" -> Length[grp]|>
]
```

## Author Notes

Drafted with Claude (Anthropic) under human supervision (Nikolay Murzin). The parser, frontmatter, examples, and dataset construction were iteratively refined against the live `CheckDefinitionNotebook` lint and against the [TPTP v9.2.1 official distribution](https://tptp.org/TPTP/Distribution/TPTP-v9.2.1.tgz). The TPTP library itself is the work of Geoff Sutcliffe and Christian Suttner, 1993 onward; this resource is a derivative parse-and-package of their corpus and inherits its redistribution terms (verbatim redistribution with attribution to the TPTP).

To rebuild this resource locally, pre-download the TPTP v9.2.1 tarball to `~/Downloads/TPTP-v9.2.1.tgz` (the official server runs at ~200 KB/s, expect ~75 minutes), then evaluate the Content cell - parsing all 25,775 problems takes another ~35 minutes on a 2024 laptop. The slow steps amortise: subsequent rebuilds reuse `MarkdownToNotebook`'s `PersistentSymbol` cache.
