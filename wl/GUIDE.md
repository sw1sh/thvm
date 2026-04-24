# wl/ style guide

Conventions for everything under `wl/`: paclet kernel sources (`*.wl`),
test specs (`*.wlt`), runner scripts (`*.wls`), and the LibraryLink C
bridge (`CSource/*.c`). Adapted from the
[TinyHVM WL guide](../TinyHVM/wl/GUIDE.md), with project-specific rules
added below.

## Rules the user has explicitly called out

These are non-negotiable. Strip on sight.

### No `Print`

Never call `Print` in a `.wl`, `.wlt`, or `.wls` file. If textual output
is needed, define a local helper `debugPrint` that wraps either
`WriteString[$Output, ...]` (notebook / kernel context) or
`WriteString["stdout", ...]` (`wolframscript -c` / `-f` context) and
call that.

```wolfram
debugPrint[args___] := WriteString["stdout", StringJoin @@ Map[ToString, {args}], "\n"]
```

`Print` also trips an IDE lint warning ("Suspicious use of session
symbol Print") and is noisy in batch test runs.

### No em dashes (`—`, U+2014)

Don't write em dashes in source files, docs, comments, or commit
messages. Use a plain hyphen (`-`), a colon, or a sentence break.

### No Unicode box-drawing characters

Don't use `─` (U+2500) or related box-drawing chars (`┌ ┐ │ ┴ ─`) as
visual section banners in comments. Use plain ASCII:

```c
// === lifecycle ===     // good
// --- lifecycle ---     // good
// ─── lifecycle ───     // BAD
```

Same applies in `.wl` (`(* === lifecycle === *)`) and `.md`
(`## Lifecycle`).

### No decorative Unicode arrows in source

Use `->` (or `>` in shell prompt strings) instead of `→`, `←`, etc.
Same spirit as the rules above: ASCII-only in source files unless the
character carries meaning (mathematical typesetting in a comment is
fine when relevant).

### Dark mode + Standard colors

Always design WL output for both light and dark Wolfram themes from
the start.

- Use **Standard color names** (`StandardBlue`, `StandardRed`,
  `StandardGreen`, `StandardOrange`, `StandardYellow`,
  `StandardPurple`, `StandardGray`) instead of `RGBColor[...]` or
  `Darker[Blue, 0.4]`. They read correctly on light and dark
  backgrounds.
- Use `LightDarkSwitched[lightValue, darkValue]` when a value
  (color, opacity, thickness) genuinely needs to differ between
  modes. Avoid hard-coded `White`/`Black` for `Background`,
  `FaceForm`, etc.; either let the theme drive it, or wrap in
  `LightDarkSwitched`.
- Don't unconditionally pin `Background -> White` on a Graph or
  Graphics. If a static PNG export needs an explicit background,
  set it on the Export call, not on the Graph.

## Indentation and spacing

- 4-space indentation. Never 2.
- Spaces around infix operators and pattern tests: `t_ ? NumericQ`,
  not `t_?NumericQ`.
- Prefer structural indentation over column alignment.
- One semantic unit per line in long forms (`Which`, `Switch`,
  `Association`, `Table`, option lists).
- For multi-line `If`, put a space after the opening bracket so the
  test argument lines up with the branches:

  ```wolfram
  If[ Length[dirs] === 0,
      debugPrint["no examples to run"];
      Exit[1]
  ]
  ```

  Single-line `If[cond, then, else]` does not need the leading space.

## Definitions

Prefer `Block` for local workspaces unless `Module`'s unique-symbol
guarantee is actually required. Don't add a trailing `;` to
`SetDelayed` definitions.

```wolfram
f[x_] := x + 1

g[args__] := Block[{
    x = ...,
    y
},
    body
]
```

## Naming

- Public symbols: `CamelCase` (e.g. `TLam`, `THeap`, `TWnf`).
- Internal helpers: `lowerCamelCase` (e.g. `debugPrint`, `loadFn`).
- Don't prefix internal helpers with `i...`.

## Tests

Use `VerificationTest` from the standard testing framework. Test specs
live in `*.wlt` files; the runner is a `.wls` script invoked by
`make wl-test`.

```wolfram
VerificationTest[
    expression,
    expectedOutput,
    TestID -> "human-readable id"
]
```

Don't put `Print` inside test bodies. The runner reports outcomes via
`TestReport`.

## File structure

When practical, organize a `.wl` file in this order:

1. Short file comment if needed.
2. `BeginPackage` declarations and public `::usage` strings.
3. `Begin["`Private`"]`.
4. Small general helpers (e.g. `debugPrint`).
5. Domain-specific helpers.
6. Main entry-point definitions near the end.
7. `End[]; EndPackage[];`

## Comments

- Comment non-obvious behavior, quirks, or external format
  constraints (LibraryLink calling conventions, paclet layout
  expectations, etc.).
- Don't narrate obvious code.
- Prefer one short section comment over many tiny inline comments.

## CSource (LibraryLink bridge)

The `CSource/*.c` bridge files follow the C `src/` style guide
([../AGENTS.md](../AGENTS.md)) but with one extra rule: **scalar in,
scalar out**. Every exported `EXTERN_C DLLEXPORT int <name>(...)`
function takes only `Integer` arguments and returns a single `Integer`
via `MArgument_setInteger(res, ...)`.

Higher-level constructors that would naturally return tuples
(`TLam` returning `{lam, var}`, `TDup` returning `{dp0, dp1}`) are
synthesized on the WL side from these scalar primitives. This keeps
the C surface tiny and the bridge testable from C alone.
