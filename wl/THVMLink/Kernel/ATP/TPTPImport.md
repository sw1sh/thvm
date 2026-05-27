---
Template: FunctionResource
ResourceType: Function
Name: TPTPImport
Description: Parse a TPTP problem file or source string into axioms and a conjecture
ContributedBy: Nikolay Murzin, Claude (Anthropic)
Keywords: [TPTP, ATP, theorem prover, equational logic, parser, first-order logic, higher-order logic]
Categories: [Strings & Text, Core Language & Structure]
SeeAlso: [Import, FindEquationalProof, Resolve]
Links: ["[TPTP project (Thousands of Problems for Theorem Provers)](https://tptp.org/)", "[TPTPWorld syntax BNF](https://github.com/TPTPWorld/SyntaxBNF)", "[TPTP problem syntax (Sutcliffe 2009)](https://tptp.org/Seminars/TPTPWorldTutorial/LogicTPTP.html)"]
EntrySymbol: TPTPImport
---

`TPTPImport` parses a [TPTP](https://tptp.org/) (Thousands of Problems for Theorem Provers) problem file or source string into the canonical `<|"Axioms" -> {...}, "Conjecture" -> ...|>` shape that the Wolfram Language's [FindEquationalProof]() and SMT-style decision procedures consume.

The parser handles every clause head defined by the [TPTPWorld BNF](https://github.com/TPTPWorld/SyntaxBNF) - `cnf` / `fof` / `tff` / `tcf` / `thf` / `ncf` - plus `include` directives, sequents, quoted atoms, distinct objects, signed numeric literals, and the full `$`-defined atom family. Function symbols come back as [String]()-headed compounds (`"and"[X, Y]` rather than `Symbol["and"]`) so a parsed TPTP problem can never collide with a user-level Wolfram Language binding.

## Definition

The implementation is a single self-contained `.wl` file inlined here at conversion time via the `#| file:` cell option. The resource notebook therefore carries the parser body verbatim, and the same `.wl` is `Get`-ed inside the thvm `THVMLink`ATP`` package - one source of truth shared by the in-tree wrapper and the deployed Function Repository resource:

```wl
#| file: TPTPImport.resource.wl
```

## Usage

<code>[TPTPImport]()[*source*]</code> parses a TPTP source - a [File]() reference, a path [String](), or a raw [String]() containing TPTP clauses - and returns an [Association]() with `"Axioms"` and `"Conjecture"` keys.

<code>[TPTPImport]()[[File]()[*path*]]</code> reads *path* as TPTP text; relative `include` directives are resolved against the file's directory.

<code>[TPTPImport]()[*string*]</code> parses *string* as TPTP source; relative `include` directives are resolved against [Directory]().

## Details & Options

- The result is `<|"Axioms" -> {phi_1, phi_2, ...}, "Conjecture" -> psi|>`. `"Conjecture"` is [None]() when no `conjecture` / `negated_conjecture` clause is present.
- Function and predicate symbols are returned as *[String]()-headed compounds*: `cnf(a, axiom, and(X, Y) = and(Y, X))` parses as `"and"[v$_, w$_] == "and"[w$_, v$_]`. Strings are not bound in any context, so a parsed TPTP `and` cannot shadow a user-level binding.
- Variables are clause-scoped: each Uppercase identifier inside one clause names the same universally-bound variable; the same name in a later clause is independent. Each occurrence gets a fresh `Pattern[Unique["v"], Blank[]]`.
- Single equational literals come back as bare `Equal[..]` / `Unequal[..]` (the unit-equality fragment most equational provers target); multi-literal `cnf` clauses come back as `Or[lit_1, lit_2, ...]`. Inner `! [...]` / `? [...]` quantifiers stay as WL `ForAll` / `Exists`; existential vars are bare Symbols with proper snapshot+restore scope.
- `negated_conjecture` carries the negation of the goal; the importer un-negates: `s != t` flips to `s == t`; a disjunction of disequations flips to the corresponding conjunction of equations; anything else gets a plain `Not[...]` wrapper.
- `tff`, `tcf`, `thf` strip `X:srt` sort annotations at preprocessing (the homogeneous-untyped surface). `type`-role clauses (signature declarations) are silently skipped.
- `thf` formula bodies pick up `^ [V1, ..., Vn] : body` lambdas (-> [Function]()) and left-associative `f @ x @ y` application (-> `f[x][y]`).
- Sequents `lhs1, lhs2 --> rhs1, rhs2` (each side optionally bracketed) rewrite to `Implies[And[lhs_i], Or[rhs_j]]`.
- `include('path', [name1, ...])` accepts an optional clause-name selector that filters which clauses from the included file are admitted. Path resolution order: as given (absolute or relative-to-cwd), then relative to the importing file's directory, then `$TPTP`, then `$TPTP/Problems`.
- Term-level coverage includes single-quoted atoms (`'name with space'`), double-quoted distinct objects (`"foo"` -> `"\"foo\""[]` with literal quote chars preserved in the head to distinguish from plain atoms), and signed numeric literals (integer, rational, real, scientific - all wrapped as `"<numstr>"[]` to dodge [Equal]()'s eager evaluation on bare numeric atoms).

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

A [File]() argument reads the path as TPTP text. Relative `include` directives inside the file are resolved against the file's directory.

```wl
TPTPImport[
    File @ FileNameJoin[{$InstallationDirectory,
        "Documentation", "English", "System", "ReferencePages",
        "Symbols", "FindEquationalProof.nb"}]
] // Head
```

### `fof` Boolean connectives

`&`, `|`, `~`, `=>`, `<=`, `<=>`, `<~>`, `~&`, `~|` parse with the standard precedence (left-associative `&` / `|`; non-associative `<=>` / `=>` / etc.):

```wl
{TPTPImport["fof(a, axiom, p(X) => q(X))."]["Axioms"][[1]] // Head,
 TPTPImport["fof(a, axiom, p <=> q)."]["Axioms"][[1]] // Head,
 TPTPImport["fof(a, axiom, p ~& q)."]["Axioms"][[1]] // FullForm}
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

`^ [V1, ..., Vn] : body` lambdas become [Function]() expressions; `f @ x @ y` applies left-associatively:

```wl
TPTPImport["thf(a, axiom, ! [X:$i] : (p @ X))."]["Axioms"][[1]]
```

```wl
TPTPImport["thf(a, axiom, ^ [X:$i] : (f @ X))."]["Axioms"][[1]] // Head
```

### Sequents

A sequent (left side, arrow, right side) rewrites to [Implies]() of [And]() of the left literals and [Or]() of the right literals; either side may be bracketed per TPTP `fof_tuple` syntax:

```wl
TPTPImport["fof(a, axiom, p & q --> r | s)."]["Axioms"][[1]] // FullForm
```

### Numeric literals + quoted atoms

Signed and unsigned integers, rationals, reals, and scientific-notation reals all parse as 0-arity [String]()-headed compounds. Quoted atoms preserve their literal contents in the head:

```wl
TPTPImport[
"cnf(a, axiom, foo(42) = bar).
cnf(b, axiom, foo(3.14) = bar).
cnf(c, axiom, foo(-1.5e-3) = bar).
cnf(d, axiom, eq(a, 'name with spaces')).
cnf(e, axiom, eq(\"distinct1\", \"distinct2\"))."]["Axioms"]
```

### `include`

The included file's clauses splice into the enclosing scan. The optional clause-name selector filters which clauses are admitted:

```wl
Module[{tmpdir = CreateDirectory[], r},
    Export[FileNameJoin[{tmpdir, "ax.ax"}],
        "cnf(a1, axiom, mul(X, e) = X).\n" <>
        "cnf(a2, axiom, mul(e, X) = X).\n" <>
        "cnf(a3, axiom, mul(inv(X), X) = e).\n", "Text"];
    Export[FileNameJoin[{tmpdir, "main.p"}],
        "include('ax.ax', [a1, a3]).\n", "Text"];
    r = TPTPImport[File @ FileNameJoin[{tmpdir, "main.p"}]];
    {Length @ r["Axioms"], First /@ r["Axioms"]}
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

## Possible Issues

A clause body containing a syntax error in an argument position (a Boolean operator where a term was expected) triggers the parser's anti-infinite-loop guard in `readArgs`: the cursor stops advancing, the loop bails, and a partial parse is returned. The result is structurally well-formed but semantically truncated relative to the input:

```wl
TPTPImport["ncf(a, axiom, $dia(p & q))."]
```
