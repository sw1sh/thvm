---
Template: Data
ResourceType: Data
Name: TPTP Problem Library
Description: Parsed TPTP v9.2.1 problems indexed by domain and status, parser auto-generated from the published BNF
ContributedBy: Nikolay Murzin, Claude (Anthropic)
Keywords: [TPTP, theorem proving, ATP, automated reasoning, equational logic, first-order logic, problem library, benchmarks, BNF, EBNFParse]
Categories: [Mathematics, Computer Systems]
ContentTypes: [Numerical Data, Text]
Author: Geoff Sutcliffe, Christian Suttner
Date: 2025
Publisher: TPTP World
GeographicCoverage: Global
TemporalCoverage: 1993-2025
Language: English
Rights: Verbatim redistribution permitted with attribution to the TPTP
Citation: "Sutcliffe, G. (2017). The TPTP Problem Library and Associated Infrastructure. From CNF and DPLL to TFF0 and TPI. Journal of Automated Reasoning, 59(4), 483-502."
RelatedSymbols: [FindEquationalProof, ResourceData, AxiomaticTheory]
Links: ["[TPTP project (Thousands of Problems for Theorem Provers)](https://tptp.org/)", "[TPTP v9.2.1 distribution (the source tarball)](https://tptp.org/TPTP/Distribution/TPTP-v9.2.1.tgz)", "[TPTP problem syntax (Sutcliffe 2009)](https://tptp.org/Seminars/TPTPWorldTutorial/LogicTPTP.html)", "[TPTPWorld published BNF](https://github.com/TPTPWorld/SyntaxBNF)", "[Wolfram/WolframParser paclet](https://resources.wolframcloud.com/PacletRepository/resources/Wolfram/WolframParser/)"]
---

The [TPTP (Thousands of Problems for Theorem Provers)](https://tptp.org/) library is the standard cross-prover benchmark corpus for automated theorem proving: Vampire, E, Twee, Waldmeister, and every research ATP system use it for evaluation. This resource exposes a queryable index of the full [TPTP v9.2.1](https://tptp.org/TPTP/) library (26,264 problems across 57 domains, spanning all six TPTP clause heads: CNF, FOF, TFF, TCF, THF, NCF). The primary content is a metadata [Association]() (problem name, domain, SZS status, TPTP rating, path); the parsed [Axioms]() and [Conjecture]() are returned by a `"Parse"` accessor on demand, driven by a parser auto-generated from the [published TPTPWorld BNF](https://github.com/TPTPWorld/SyntaxBNF) via `EBNFParse` from the [Wolfram/WolframParser](https://resources.wolframcloud.com/PacletRepository/resources/Wolfram/WolframParser/) paclet - no per-rule hand-coding.

## Details

- Each entry is keyed by the problem's TPTP name (e.g. `"GRP001-1"`). The 3-letter prefix is the *domain* code; the separator character before the variant index encodes the clause head: `-` (CNF), `+` (FOF), `=` (TFF), `^` (THF), `~` (NCF). The trailing numeric suffix groups variants of the same underlying problem.
- 57 TPTP domain codes group problems by mathematical subject: `"GRP"` (group theory), `"BOO"` (Boolean algebra), `"RNG"` (ring theory), `"LCL"` (logic calculi), `"SET"` (set theory), `"TOP"` (topology), `"PUZ"` (puzzles), `"SYN"` (synthetic / syntactic), and so on. The `"ByDomain"` accessor groups problems by their domain code.
- Each problem carries the SZS `"Status"` field harvested from the TPTP catalogue header: `"Unsatisfiable"` (the negated conjecture is refutable, the standard ATP target), `"Satisfiable"`, `"Theorem"`, `"CounterSatisfiable"`, `"Open"`. The `"ByStatus"` accessor partitions accordingly.
- The TPTP `"Rating"` (0 to 1) indicates problem difficulty for state-of-the-art provers - 0.0 means every modern ATP closes it; 1.0 means no system in the current evaluation cohort can crack it. Parsed from the `%Rating` header line of the current TPTP version.
- The `"Parse"` accessor returns the standard `<|"Includes" -> {…}, "Axioms" -> {…}, "Conjecture" -> phi|>` shape. Function symbols come back as [String]()-headed compounds (`"multiply"["X", "Y"]` rather than `Symbol["multiply"][…]`) so a parsed TPTP symbol cannot shadow a user-level [Wolfram Language](https://www.wolfram.com/language/) binding. Variables keep their original TPTP names as bare [String]()s. Quantifiers lift to [ForAll]() / [Exists](); Boolean connectives to [And]() / [Or]() / [Not]() / [Implies]() / [Equivalent]() / [Xor]() / [Nor]() / [Nand](); equational atoms to [Equal]() / [Unequal]().
- The parser is generated automatically from the [TPTPWorld BNF](https://github.com/TPTPWorld/SyntaxBNF) (338 rules in TPTP v9.2.1) by [`EBNFParse`](https://resources.wolframcloud.com/PacletRepository/resources/Wolfram/WolframParser/) - no per-rule hand-coding. An `"Actions"` map (~50 entries) lifts the raw parse tree to the canonical Wolfram-Language shape. When TPTP-v9.3 ships, the same `EBNFParse` call re-binds the parser; the actions are rule-name keyed so they survive grammar evolution.
- The deployer pre-downloads the official TPTP tarball (922 MB compressed, 9.9 GB extracted) once to `~/Downloads/TPTP-v9.2.1.tgz`; the Content cell extracts it on first evaluation and harvests headers to build the metadata index (~10 minutes). The `"Parse"` accessor re-extracts on demand to find the source `.p` file.

## Content

Set up the parser from the published BNF. [`EBNFParse`](https://resources.wolframcloud.com/PacletRepository/resources/Wolfram/WolframParser/) lowers the 354-rule grammar to a per-rule [Association]() of parser combinators; the `"Actions"` map wraps each named rule's combinator in a [ParseAction]() that lifts the raw parse tree to the desired output shape:

Load the paclet and fetch the canonical [TPTPWorld BNF](https://github.com/TPTPWorld/SyntaxBNF) (versioned, ~735 lines, ~30 KB):

```wl
Needs["Wolfram`Parser`"];
$tptpBnf = Import["https://raw.githubusercontent.com/TPTPWorld/SyntaxBNF/master/SyntaxBNF-v9.2.1.4", "Text"];
```

Three helpers used by several rule actions: `binConn` maps the six non-associative connective tokens to their Wolfram-Language heads, `rightList` flattens a right-recursive `head , tail` cons list, and `quant` dispatches `!`/`?` to [ForAll]() / [Exists]() while suppressing the variable-list message:

```wl
binConn[op_String, x_, y_] := Switch[op,
    "<=>", Equivalent[x, y], "=>", Implies[x, y], "<=", Implies[y, x],
    "<~>", Xor[x, y],        "~|", Nor[x, y],     "~&", Nand[x, y]];

rightList[args__] := Module[{a = {args}},
    Switch[Length[a], 1, {a[[1]]}, 3, Prepend[a[[3]], a[[1]]]]];

quant[q_, vs_, body_] := Quiet[
    Apply[If[q === "!", ForAll, Exists], {vs, body}],
    {ForAll::ivar, Exists::ivar}];
```

Term-level actions: terms lift to themselves, function application becomes `head[args]`, `=`/`!=` become [Equal]() / [Unequal]():

```wl
$tptpTermActions = <|
    "constant" -> Function[#1[]], "functor" -> Function[#1],
    "variable" -> Function[#1],   "fof_term" -> Function[#1],
    "fof_function_term" -> Function[#1],
    "fof_plain_term" -> Function[Block[{a = {##}},
        Switch[Length[a], 1, a[[1]], 4, a[[1]] @@ a[[3]]]]],
    "fof_arguments" -> rightList,
    "fof_atomic_formula"         -> Function[#1],
    "fof_plain_atomic_formula"   -> Function[#1],
    "fof_defined_atomic_formula" -> Function[#1],
    "fof_defined_plain_formula"  -> Function[#1],
    "fof_defined_infix_formula"  -> Function[Equal[#1, #3]],
    "fof_infix_unary"            -> Function[Unequal[#1, #3]]
|>;
```

Boolean connective actions: `&` / `|` / `~` / binary non-associative lift to [And]() / [Or]() / [Not]() / `binConn`:

```wl
$tptpConnActions = <|
    "nonassoc_connective" -> Function[Block[{a = {##}},
        If[Length[a] === 1, a[[1]], StringJoin @@ a]]],
    "fof_binary_nonassoc" -> Function[binConn[#2, #1, #3]],
    "fof_and_formula" -> Function[Block[{a = {##}},
        And @@ Join[{a[[1]], a[[3]]}, a[[4]][[All, 2]]]]],
    "fof_or_formula" -> Function[Block[{a = {##}},
        Or @@ Join[{a[[1]], a[[3]]}, a[[4]][[All, 2]]]]],
    "fof_unary_formula" -> Function[Block[{a = {##}},
        Switch[Length[a], 1, a[[1]], 2, Not[a[[2]]]]]]
|>;
```

Quantifier + passthrough actions: `!`/`?` lift to [ForAll]() / [Exists](); the unary/binary/unit/unitary passthroughs forward their argument unchanged:

```wl
$tptpQuantActions = <|
    "fof_quantifier" -> Function[#1], "fof_variable_list" -> rightList,
    "fof_quantified_formula" -> Function[quant[#1, #3, #6]],
    "fof_binary_assoc" -> Function[#1], "fof_binary_formula" -> Function[#1],
    "fof_logic_formula" -> Function[#1], "fof_unit_formula" -> Function[#1],
    "fof_unitary_formula" -> Function[Block[{a = {##}},
        Switch[Length[a], 1, a[[1]], 3, a[[2]]]]],
    "fof_formula" -> Function[#1]
|>;
```

CNF actions (`-`-separator clauses): single literal stays bare, multi-literal becomes [Or]() of the literals:

```wl
$tptpCnfActions = <|
    "cnf_literal" -> Function[Block[{a = {##}},
        Switch[Length[a], 1, a[[1]], 2, Not[a[[2]]], 4, Not[a[[3]]]]]],
    "cnf_disjunction" -> Function[Block[{a = {##}},
        If[Length[#2] === 0, #1, Or @@ Prepend[#2[[All, 2]], #1]]]],
    "cnf_formula" -> Function[Block[{a = {##}},
        Switch[Length[a], 1, a[[1]], 3, a[[2]]]]]
|>;
```

The TPTP-file action partitions clauses into includes / axioms / conjecture, with `negated_conjecture` flipped through [Not]():

```wl
tptpFile[cs__] := With[{c = FirstCase[{cs},
        KeyValuePattern["Role" -> "conjecture"], None],
        nc = FirstCase[{cs},
        KeyValuePattern["Role" -> "negated_conjecture"], None]},
    <|"Includes" -> Cases[{cs},
        kv:KeyValuePattern["Head" -> "include"] :> kv["File"]],
      "Axioms" -> Cases[{cs},
        kv:KeyValuePattern["Role" -> "axiom" | "hypothesis"] :> kv["Formula"]],
      "Conjecture" -> Which[
        c =!= None, c["Formula"],
        nc =!= None, Not[nc["Formula"]],
        True, None]|>];
```

Top-level clause actions: each annotated clause becomes a tagged [Association](); the `"TPTP_file"` action delegates to `tptpFile`:

```wl
$tptpFileActions = <|
    "cnf_annotated" -> Function[<|"Head" -> "cnf",
        "Name" -> #3, "Role" -> #5, "Formula" -> #7|>],
    "fof_annotated" -> Function[<|"Head" -> "fof",
        "Name" -> #3, "Role" -> #5, "Formula" -> #7|>],
    "tff_annotated" -> Function[<|"Head" -> "tff",
        "Name" -> #3, "Role" -> #5, "Formula" -> #7|>],
    "include" -> Function[<|"Head" -> "include", "File" -> #3|>],
    "TPTP_file" -> tptpFile
|>;
```

Build the parser. `EBNFParse` lowers the BNF + actions to a per-rule [Association]() of parser combinators; pull out the top-level `TPTP_file` rule as the entry point:

```wl
$tptpParsers = EBNFParse[$tptpBnf, "Actions" ->
    Join[$tptpTermActions, $tptpConnActions, $tptpQuantActions,
         $tptpCnfActions, $tptpFileActions]];
$tptpParser = $tptpParsers["TPTP_file"];
```

Extract the pre-downloaded tarball into a persistent sibling directory (skipped on subsequent runs since `~/Downloads/TPTP-v9.2.1/` already exists - the 9.9 GB extract should only happen once per machine, not once per notebook open). [ExtractArchive]() returns silently empty when called from `MarkdownToNotebook`'s evaluation kernel, so shell out to `tar` via [RunProcess]() instead. The `TPTP` environment variable is set so include directives resolve at parse time:

```wl
$tptpTar = FileNameJoin[
    {$HomeDirectory, "Downloads", "TPTP-v9.2.1.tgz"}];
$tptpRoot = FileNameJoin[
    {$HomeDirectory, "Downloads", "TPTP-v9.2.1"}];
If[ ! FileExistsQ[$tptpTar],
    Print["Pre-download TPTP v9.2.1 to ", $tptpTar, " from ",
        "https://tptp.org/TPTP/Distribution/TPTP-v9.2.1.tgz",
        " (~922 MB)."]; Abort[]];
If[ ! DirectoryQ[$tptpRoot],
    RunProcess[{"tar", "xzf", $tptpTar,
        "-C", DirectoryName[$tptpRoot]}]];
$tptpProblemsRoot = FileNameJoin[{$tptpRoot, "Problems"}];
SetEnvironment["TPTP" -> $tptpRoot];
```

Parse the `%`-prefixed key-value header lines of one `.p` file into an [Association]():

```wl
tptpHeaderOf[path_] := Association @ Flatten @ StringCases[
    Quiet @ ReadList[path, "String", 50],
    RegularExpression["^%\\s+([A-Za-z_]+)\\s*:\\s*(.+)$"] :>
        StringTrim["$1"] -> StringTrim["$2"]];

tptpRating[h_] := Quiet @ Replace[
    ToExpression @ First @ StringSplit[
        Lookup[h, "Rating", ""], " " | ","],
    _?(! NumberQ[#] &) -> Missing["NoRating"]];
```

Build one problem's metadata record from its file path:

```wl
tptpMetaOf[path_] := With[
    {name = FileBaseName[path], h = tptpHeaderOf[path]},
    name -> <|"Name" -> name,
        "Domain" -> FileNameTake[DirectoryName[path]],
        "Path"   -> StringDrop[path, StringLength[$tptpProblemsRoot] + 1],
        "Status" -> Lookup[h, "Status", Missing["NoStatus"]],
        "Rating" -> tptpRating[h]|>];
```

Walk every `.p` file under the corpus's `Problems/` tree and harvest its header into the primary content [Association]():

```wl
tptpProblems = Association @ Map[
    tptpMetaOf, FileNames["*.p", $tptpProblemsRoot, Infinity]];
```

The primary content is the metadata [Association](). Two named accessors group by domain and SZS status:

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

The `"Parse"` named accessor is a [Function]() that takes a TPTP problem name and returns the parsed `<|"Includes" -> …, "Axioms" -> …, "Conjecture" -> …|>` form via the auto-generated `$tptpParser`. Strips line comments first (the TPTP grammar treats `%` lines as whitespace but the published BNF doesn't include a comment rule). Loaded on demand keeps the primary content small while still letting any user re-parse any problem with one call:

```wl
#| eval: false
ResourceData[ResourceObject[EvaluationNotebook[]], "Parse"] =
    Function[problemName,
        Parse[$tptpParser,
            StringTrim @ StringReplace[
                Import[FileNameJoin[
                    {$tptpProblemsRoot,
                     tptpProblems[problemName]["Path"]}],
                    "Text"],
                RegularExpression["%[^\n]*"] -> ""]]]
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

<!-- => 26264 -->

## Scope & Additional Elements

The TPTP domains carry very different problem counts; the synthetic (`"SYN"`), software-verification (`"SWV"`), and set theory (`"SET"`) domains dominate. The top ten by count:

```wl
Take[ReverseSort @ Counts[Values[tptpProblems][[All, "Domain"]]], 10]
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

Fetch the parsed Axioms and Conjecture for a single problem via the auto-generated `$tptpParser`. The result is the standard `<|"Includes" -> {…}, "Axioms" -> {…}, "Conjecture" -> phi|>` shape:

```wl
Parse[$tptpParser, StringTrim @ StringReplace[
    Import[FileNameJoin[{$tptpProblemsRoot,
        tptpProblems["GRP001-4"]["Path"]}], "Text"],
    RegularExpression["%[^\n]*"] -> ""]]
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

Drafted with Claude (Anthropic) under human supervision. The parser comes for free from the [Wolfram/WolframParser](https://resources.wolframcloud.com/PacletRepository/resources/Wolfram/WolframParser/) paclet's `EBNFParse` applied to the published [TPTPWorld BNF](https://github.com/TPTPWorld/SyntaxBNF) (338 rules in TPTP v9.2.1); the action map (~50 entries) is the only piece tied to TPTP. When TPTP-v9.3 ships, the same `EBNFParse` call re-binds the parser; the actions are rule-name keyed and survive grammar evolution. The TPTP library itself is the work of Geoff Sutcliffe and Christian Suttner, 1993 onward; this resource is a derivative parse-and-package of their corpus and inherits its redistribution terms (verbatim redistribution with attribution to the TPTP).

To rebuild this resource locally, pre-download the TPTP v9.2.1 tarball to `~/Downloads/TPTP-v9.2.1.tgz` (~75 minutes against the TPTP server) and ensure the `Wolfram/WolframParser` paclet is installed (`PacletInstall["Wolfram/WolframParser"]`); the Content cell then extracts the tarball and builds the metadata index in ~80 seconds. The Parse accessor takes tens to hundreds of milliseconds per call.
