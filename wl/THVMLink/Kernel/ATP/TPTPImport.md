---
Template: FunctionResource
ResourceType: Function
Name: TPTPImport
Description: Parse a TPTP problem from a file or string into axioms and a conjecture
ContributedBy: Nikolay Murzin, Claude (Anthropic)
Keywords: [TPTP, ATP, theorem prover, equational logic, parser, first-order logic, higher-order logic]
Categories: [Strings & Text, Core Language & Structure]
SeeAlso: [Import, FindEquationalProof, Resolve]
Links: ["[TPTP project (Thousands of Problems for Theorem Provers)](https://tptp.org/)", "[TPTPWorld syntax BNF](https://github.com/TPTPWorld/SyntaxBNF)", "[TPTP problem syntax (Sutcliffe 2009)](https://tptp.org/Seminars/TPTPWorldTutorial/LogicTPTP.html)"]
EntrySymbol: TPTPImport
---

`TPTPImport` parses a [TPTP](https://tptp.org/) (Thousands of Problems for Theorem Provers) problem file or string into the canonical `<|"Axioms" -> {…}, "Conjecture" -> …|>` shape that the Wolfram Language's [FindEquationalProof]() and SMT-style decision procedures consume.

The parser handles every clause head defined by the [TPTPWorld BNF](https://github.com/TPTPWorld/SyntaxBNF) - `cnf` / `fof` / `tff` / `tcf` / `thf` / `ncf` - plus `include` directives, sequents, quoted atoms, distinct objects, signed numeric literals, and the full `$`-defined atom family. Function symbols come back as [String]()-headed compounds (`"and"[X, Y]` rather than `Symbol["and"]`) so a parsed TPTP problem can never collide with a user-level Wolfram Language binding.

## Definition

The implementation is a single self-contained `.wl` file inlined here at conversion time via the `#| file:` cell option. The resource notebook therefore carries the parser body verbatim, and the same `.wl` is `Get`-ed inside the thvm `THVMLink`ATP`` package - one source of truth shared by the in-tree wrapper and the deployed Function Repository resource:

```wl
#| file: TPTPImport.resource.wl
```

## Usage

<code>[TPTPImport]()[*tptp*]</code> parses *tptp* - a [File]() reference, a path [String](), or a raw [String]() containing TPTP clauses - and returns an [Association]() with `"Axioms"` and `"Conjecture"` keys.

<code>[TPTPImport]()[[File]()[*path*]]</code> reads *path* as TPTP text; relative `include` directives are resolved against the file's directory.

<code>[TPTPImport]()[*string*]</code> parses *string*; relative `include` directives are resolved against [Directory]().

## Details & Options

- The result is an [Association]() of shape `<|"Axioms" -> {phi1, phi2, …}, "Conjecture" -> psi|>`. `"Conjecture"` is [None]() when no `conjecture` or `negated_conjecture` clause is present.
- Function and predicate symbols are returned as [String]()-headed compounds: `cnf(a, axiom, and(X, Y) = and(Y, X))` parses as `"and"[X_, Y_] == "and"[Y_, X_]`. [String]()s are not bound in any context, so a parsed TPTP `and` cannot shadow a user-level binding.
- Variables keep their original TPTP names: each upper-case identifier inside one clause names the same universally bound variable; the same name in a later clause is independent ([Pattern]() variables are scope-local to a rule, so two clauses both referencing `X` do not cross-bind). Underscores in TPTP variable names (the rare legal form `X_var`) fold to `$` since [Symbol]() identifiers cannot contain `_`.
- Single equational literals come back as bare [Equal]() or [Unequal]() (the unit-equality fragment most equational provers target); multi-literal `cnf` clauses come back as [Or]() of the literals. Inner `! [...]` / `? [...]` quantifiers stay as [ForAll]() and [Exists]() so the downstream consumer sees the full structure; existential bound variables are bare [Symbol]()s with proper snapshot-and-restore scope around the quantifier body.
- A `negated_conjecture` clause carries the negation of the goal. The importer un-negates: a single `s != t` flips to [Equal]() of `s` and `t`; a disjunction of disequations flips to the corresponding conjunction of equations; anything else gets a plain [Not]() wrapper.
- `tff`, `tcf`, and `thf` strip `X:srt` sort annotations at preprocessing (the homogeneous-untyped surface). `type`-role clauses (signature declarations) are silently skipped.
- `thf` formula bodies pick up `^ [V1, ..., Vn] : body` lambdas, which become [Function]() expressions, and left-associative `f @ x @ y` application, which becomes the curried form `f[x][y]`.
- Sequents `lhs1, lhs2 --> rhs1, rhs2` (each side optionally bracketed in `[...]`) rewrite to [Implies]() of [And]() of the left literals and [Or]() of the right literals.
- `include('path', [name1, ...])` accepts an optional clause-name selector that filters which clauses from the included file are admitted. Path resolution order: as given (absolute or relative-to-cwd), then relative to the importing file's directory, then relative to the `$TPTP` environment variable, then relative to `$TPTP/Problems`.
- Term-level coverage includes single-quoted atoms (`'name with space'`), double-quoted distinct objects (TPTP semantics: pairwise non-equal by built-in axiom), and signed numeric literals (integer, rational, real, scientific). Numeric literals are wrapped as 0-arity [String]()-headed compounds to dodge [Equal]()'s eager evaluation on bare numeric atoms.

## Basic Examples

Parse a single-clause TPTP string:

```wl
TPTPImport["cnf(a, axiom, and(X, Y) = and(Y, X))."]
```

---

A negated conjecture flips its disequation back to the un-negated goal under the `"Conjecture"` key, matching the proof-by-contradiction convention every TPTP unit-equality problem uses:

```wl
TPTPImport[
    "cnf(a1, axiom, and(X, Y) = and(Y, X)).
     cnf(a2, axiom, and(X, and(Y, Z)) = and(and(X, Y), Z)).
     cnf(g,  negated_conjecture, and(and(p, q), r) != and(r, and(q, p)))."]
```

---

`fof` clauses carry the full Boolean grammar; the leading universal quantifier is peeled and free uppercase identifiers become universals:

```wl
TPTPImport["fof(comm, axiom, ! [X, Y] : (and(X, Y) = and(Y, X)))."]
```

## Scope

### File input

A [File]() argument reads the path as TPTP text; relative `include` directives inside the file are resolved against the file's directory. Round-trip a single-clause file through the parser:

```wl
Module[{tmp = CreateFile[]},
    Export[tmp, "cnf(a, axiom, foo(X) = X).\n", "Text"];
    TPTPImport[File[tmp]]
]
```

### `fof` Boolean connectives

The implication `=>` becomes [Implies]():

```wl
TPTPImport["fof(a, axiom, p(X) => q(X))."]["Axioms"][[1]]
```

---

The biconditional `<=>` becomes [Equivalent]():

```wl
TPTPImport["fof(a, axiom, p <=> q)."]["Axioms"][[1]]
```

---

The negated-conjunction shorthand `~&` becomes [Not]() of [And]():

```wl
TPTPImport["fof(a, axiom, p ~& q)."]["Axioms"][[1]]
```

---

Left-associative `&` flattens into an n-ary [And]():

```wl
TPTPImport["fof(a, axiom, p & q & r & s)."]["Axioms"][[1]]
```

### Quantifiers

Inner `! [X] : body` becomes `ForAll[{X}, body]`; inner `? [X] : body` becomes `Exists[{X}, body]` with fresh bare-Symbol bound vars and proper snapshot+restore scope:

```wl
TPTPImport["fof(a, axiom, ? [X] : p(X))."]["Axioms"][[1]]
```

### `tff` / `tcf` typed first-order

`type`-role signature declarations are silently skipped; sort annotations (`X:srt`) are stripped at preprocessing so the body parses identically to its untyped fof equivalent:

```wl
TPTPImport[
"tff(p_type, type, p: $i > $o).
tff(a, axiom, ! [X:$i] : p(X))."]
```

### `thf` typed higher-order

A `! [X:srt]` universal over a higher-order argument applies `@` against the bound variable:

```wl
TPTPImport["thf(a, axiom, ! [X:$i] : (p @ X))."]["Axioms"][[1]]
```

---

A `^ [V1, ..., Vn] : body` lambda becomes a [Function]() expression:

```wl
TPTPImport["thf(a, axiom, ^ [X:$i] : (f @ X))."]["Axioms"][[1]] // Head
```

### Sequents

A sequent (left side, arrow, right side) rewrites to [Implies]() of [And]() of the left literals and [Or]() of the right literals; either side may be bracketed per TPTP `fof_tuple` syntax:

```wl
TPTPImport["fof(a, axiom, p & q --> r | s)."]["Axioms"][[1]] // FullForm
```

### Numeric literals

An unsigned integer parses as a 0-arity [String]()-headed compound:

```wl
TPTPImport["cnf(a, axiom, foo(42) = bar)."]["Axioms"][[1]]
```

---

A signed integer keeps its sign in the head:

```wl
TPTPImport["cnf(a, axiom, foo(-42) = bar)."]["Axioms"][[1]]
```

---

Scientific-notation reals round-trip as a single compound:

```wl
TPTPImport["cnf(a, axiom, foo(-1.5e-3) = bar)."]["Axioms"][[1]]
```

### Quoted atoms and distinct objects

A single-quoted atom carries its contents verbatim as the [String]() head:

```wl
TPTPImport["cnf(a, axiom, eq(a, 'name with spaces'))."]["Axioms"][[1]]
```

---

A double-quoted distinct object preserves the literal quote characters in the head, which keeps it visually distinguishable from a plain quoted atom:

```wl
TPTPImport[
    "cnf(a, axiom, eq(\"distinct1\", \"distinct2\"))."]["Axioms"][[1]]
```

### `include`

Without a selector, every clause from the included file is admitted. Build an axioms file with three clauses, include the whole file, and count the resulting axioms:

```wl
Module[{tmpdir = CreateDirectory[]},
    Export[FileNameJoin[{tmpdir, "ax.ax"}],
        "cnf(a1, axiom, mul(X, e) = X).\n" <>
        "cnf(a2, axiom, mul(e, X) = X).\n" <>
        "cnf(a3, axiom, mul(inv(X), X) = e).\n", "Text"];
    Export[FileNameJoin[{tmpdir, "main.p"}],
        "include('ax.ax').\n", "Text"];
    Length @ TPTPImport[
        File @ FileNameJoin[{tmpdir, "main.p"}]]["Axioms"]
]
```

---

With a clause-name selector, only the listed clauses are admitted:

```wl
Module[{tmpdir = CreateDirectory[]},
    Export[FileNameJoin[{tmpdir, "ax.ax"}],
        "cnf(a1, axiom, mul(X, e) = X).\n" <>
        "cnf(a2, axiom, mul(e, X) = X).\n" <>
        "cnf(a3, axiom, mul(inv(X), X) = e).\n", "Text"];
    Export[FileNameJoin[{tmpdir, "main.p"}],
        "include('ax.ax', [a1, a3]).\n", "Text"];
    Length @ TPTPImport[
        File @ FileNameJoin[{tmpdir, "main.p"}]]["Axioms"]
]
```

## Applications

Hand the parsed axioms and conjecture straight to [FindEquationalProof]() to discharge a Wolfram-AxiomaticTheory-style equational obligation imported from a TPTP benchmark:

```wl
r = TPTPImport[
    "cnf(a1, axiom, and(X, Y) = and(Y, X)).
     cnf(a2, axiom, and(X, and(Y, Z)) = and(and(X, Y), Z)).
     cnf(g,  negated_conjecture, and(and(p, q), r) != and(r, and(q, p)))."];
FindEquationalProof[r["Conjecture"], r["Axioms"]]
```

---

Build an [EntityStore]() over a directory of TPTP problem files, one entity per problem. Set up a tiny inline corpus first:

```wl
$tptpCorpus = CreateDirectory[];
Export[FileNameJoin[{$tptpCorpus, "GroupAxioms__Identity.p"}],
    "cnf(a, axiom, mul(X, e) = X).\n" <>
    "cnf(g, negated_conjecture, mul(a, e) != a).\n", "Text"];
Export[FileNameJoin[{$tptpCorpus, "BooleanAxioms__DoubleNeg.p"}],
    "cnf(a, axiom, not(not(X)) = X).\n" <>
    "cnf(g, negated_conjecture, not(not(p)) != p).\n", "Text"]
```

---

Parse each problem file into a property [Association](), keyed by the file's basename and carrying the inferred theory name plus the parsed axioms and conjecture:

```wl
$tptpEntities = Association @ Map[
    Function[path,
        With[{name = FileBaseName[path], r = TPTPImport[File[path]]},
            name -> <|
                "Name"       -> name,
                "Theory"     -> First @ StringSplit[name, "__"],
                "Conjecture" -> r["Conjecture"],
                "AxiomCount" -> Length @ r["Axioms"]
            |>]],
    FileNames["*.p", $tptpCorpus]]
```

---

Wrap the per-problem associations in an [EntityStore](), then query it like any other entity type:

```wl
$tptpStore = EntityStore[<|"TPTPProblem" -> <|
    "Label" -> "TPTP problem",
    "Properties" -> AssociationMap[<|"Label" -> ToLowerCase[#]|> &,
        {"Name", "Theory", "Conjecture", "AxiomCount"}],
    "Entities" -> $tptpEntities|>|>];
EntityValue[
    EntityList[$tptpStore["TPTPProblem"]],
    {"Theory", "AxiomCount"}]
```

## Possible Issues

A clause body containing a syntax error in an argument position (a Boolean operator where a term was expected) triggers the parser's anti-infinite-loop guard in `readArgs`: the cursor stops advancing, the loop bails, and a partial parse is returned. The result is structurally well-formed but semantically truncated relative to the input:

```wl
TPTPImport["ncf(a, axiom, $dia(p & q))."]
```
