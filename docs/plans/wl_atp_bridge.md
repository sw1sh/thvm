# WL ATP bridge: design memo (stage 8.7a)

> Decision document for stage 8.7's `TATP[axioms, conjecture]`
> Wolfram Language surface.

## Goal

Make the IC-native ATP reachable from Wolfram notebooks.  The
target shape:

```mathematica
TATP[
  { f[x_, e] == x,
    f[x_, i[x_]] == e,
    f[f[x_, y_], z_] == f[x_, f[y_, z_]] },
  f[a, i[a]] == e
]
(* -> Association[
       "Status" -> "PROVED",
       "Steps"  -> 1,
       "Rules"  -> 2,
       "Trace"  -> { ... PCL-shaped step list ... } ] *)
```

Today the ATP is C-only (`tests/test_bench_atp.c`,
`tools/bench_twee.c`); WL notebooks can't drive it.  The bridge
unblocks notebook-side experimentation, and is a step toward the
WL-as-frontend story in `docs/plans/waldmeister_ic_atp.md`.

## WL surface

```mathematica
TATP[axioms_List, conjecture_, opts___] := ...
```

- `axioms`: list of equational `lhs == rhs` pairs.  Variables
  appear as `Pattern[name, Blank[]]` (the `x_` syntax).
  Free variables (uppercase letters in TPTP-UEQ) are written
  uppercase here too: `X_`, `Y_`, `Z_`.
- `conjecture`: a single `lhs == rhs` pair.  Same encoding.
- Returns an `Association[...]`:
  - `"Status"`: one of `"PROVED"`, `"TIMEOUT"`, `"QUEUE_EMPTY"`,
    or `Failure[...]` for a parse error.
  - `"Steps"`, `"Rules"`, `"Trace"`: per-saturation diagnostics.
- Options:
  - `MaxSteps -> 64` (default; matches BENCH_STEP_BUDGET)
  - `Ordering -> "LPO"` (default; alternative `"KBO"`)

## WL-expression to Term encoding

| WL form | Term |
|---|---|
| `Symbol[s]` (e.g. `e`, `nil`, `zero`) | `term_new_ctr(label, NULL, 0)` -- nullary |
| `head[args...]` (e.g. `f[x, e]`) | `term_new_ctr(label, encoded_args, n)` |
| `Pattern[var, Blank[]]` (e.g. `x_`) | `term_new_fvr(var_id)` |
| `lhs == rhs` (or `Equal[lhs, rhs]`) | `(encoded_lhs, encoded_rhs)` pair fed to `thvm_atp_add_equation` |

Symbol-to-label allocation: a `WaldSpec`-style symbol table is
built at `TATP[]` invocation time.  Each new symbol encountered
gets a fresh CTR label (starting at 1; 0 reserved); stored in
`spec->symbols[]` for reuse.

Pattern-to-FVR allocation: each distinct `Pattern[var, _]`
encountered gets a fresh `var_id` (starting at 0).  Same name
in different occurrences shares the id.

## What about WL pattern constructs we don't model?

WL has many pattern types -- `_h` (typed Blank), `___` (blank
sequences), `Optional`, `Condition`, `RuleDelayed`, etc.  In v0
we accept only:

- Bare `Pattern[name, Blank[]]` (i.e. `x_`).
- Plain symbols and `head[...]` expressions on the term side.
- `Equal[lhs, rhs]` on the equation side.

Anything else returns `Failure["TATPParseError", ...]` with a
description of the offending construct.  Future extensions can
add typed patterns once 8.4's sort metadata propagates to the
WL surface.

## LibraryLink plumbing

Two layers:

### Layer 1: pre-encoded Term entry point (8.7b)

```c
EXTERN_C DLLEXPORT int thvm_wl_atp_run(
  WolframLibraryData libData, mint argc, MArgument *args, MArgument res
);
```

Takes:
- `axiom_lhs[]`, `axiom_rhs[]`: NumericArray Int64 of pre-built
  Term values (from `thvm_wl_term_new`-style synthesis on the
  WL side).
- `goal_lhs`, `goal_rhs`: scalar Int64 Term values.
- `max_steps`: mint.
- `ordering_kind`: mint (0 = KBO, 1 = LPO).

Returns: a packed Association as a String (PCL-shaped trace +
metadata).  Or a NumericArray of Term values for the trace +
scalar `mint` outputs for status / step / rules.

This entry point isolates the saturator-from-LibraryLink
plumbing.  Tests can drive it from WL with manually-built
Terms, sidestepping the encoder.

### Layer 2: WL-side encoder + wrapper (8.7c, 8.7d)

`TATP[axioms, conjecture, opts]` walks the input expressions,
allocates Term values via the existing `thvm_wl_term_new` API,
calls `thvm_wl_atp_run`, decodes the result.

The encoder is pure WL (no new C code) -- it builds Terms by
calling existing primitives.

## Symbol-table lifetime

Each `TATP[]` call constructs a fresh symbol table (no shared
state across calls).  Within one call, the same symbol always
maps to the same label.  Future iterations could cache
signatures across calls if the user threads a context object
through.

## Trace decoding

The C-side `thvm_atp_trace_serialize` returns PCL-shaped text:
`"<idx> (axiom): lhs = rhs\n<idx> (orient from N): ...\n..."`
A WL-side parser splits on `\n`, then on `: `, building an
`Association[]` per line.  Order-preserving so the trace can be
walked in saturation order.

## Verification (for stages 8.7b-d)

- 8.7b: a focused WL test that calls `thvm_wl_atp_run` directly
  with manually-built Terms; asserts status, n_rules, trace
  prefix.  Test in `wl/THVMLink/Tests/atp.wlt`.
- 8.7c: pure WL tests that build Terms from various expression
  shapes; assert the resulting Term tags / structure match
  expectation.
- 8.7d: end-to-end `TATP[group_axioms, goal] -> "PROVED"` test
  matching `tests/data/atp/group_right_inverse_to_e.pr`'s
  outcome.

## Stop conditions

If 8.7b reveals that the existing `thvm_wl_term_new` doesn't
expose a clean way to build CTR Terms with arbitrary children
from the WL side, extend it (`thvm_wl_term_new_ctr` -- new
LibraryLink entry) before continuing.

If 8.7c hits encoding ambiguity (e.g. `Equal[a, b]` where `a`
or `b` themselves contain `==`), document the disambiguation
rule and reject ambiguous cases via `Failure[...]`.

If 8.7d shows wall-clock dominated by the WL-to-Term encoder
rather than the saturator, that's expected for tiny problems
(saturator is sub-millisecond; WL pattern matching can take
several ms).  Document; not a regression.

## Out of scope

- TPTP-UEQ file parsing from WL (`TATP[File["foo.p"]]`):
  `wald_parse_file` already exists; a thin wrapper would work
  but is independent of 8.7's encoder work.  Defer.
- Proof-tree-as-graphics output (`Graph[]` rendering of the
  trace DAG): nice-to-have for notebooks but separate.  Defer.
- Spec-based dispatch (auto-detect KBO vs LPO from the symbol
  shape): rolls under 8.5d, already done.
