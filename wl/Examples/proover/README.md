# ProoVer 2026: a thvm proof checker

Prep for the [ProoVer competition](https://proover-competition.github.io/) plus a
demo of the `TFindProof` -> `ProofObject` -> validation workflow.

## What ProoVer is

ProoVer is a proof *checking* competition (not proof finding, which is the
[CADE ATP System Competition, CASC](https://tptp.org/CASC/)). The organizers
(Julie Cailler, LORIA; Simon Guilloud, EPFL) hand each system 100 first-order
TSTP (Thousands of Solutions for Theorem Provers) proofs, 50 valid and 50
deliberately "buggy", and the system must emit one SZS (Solution Status Scheme)
verdict per proof:

| verdict          | meaning            | score if right | score if wrong |
| ---------------- | ------------------ | -------------- | -------------- |
| `FailedVerified` | proof is invalid   | +2             | **-10**        |
| `Verified`       | proof is valid     | +1             | -1             |
| `NotVerified`    | cannot decide      | 0              | 0              |

The asymmetry is the whole game: calling a buggy proof good costs **-10**, while
abstaining costs 0. The optimal checker is conservative: stamp `Verified` only
when every step is independently confirmed, and fall back to `NotVerified` the
instant a step cannot be settled. 30 second wall-clock per proof, no CPU-time
limit (multicore encouraged).

### Proof format

First-order form (FOF), by refutation: negate the conjecture, derive `$false`.
Each line is a record

```
fof(name, role, formula, inference(rule, [status(...)], [parents])).
```

with `role` in {`axiom`, `conjecture`, `negated_conjecture`, `plain`} and
`status` in {`thm`, `esa`, `cth`}. The published inference rules so far are
`instantiate`, `skolemize`, `consequence`, `horn` (the organizers note "the full
list of admissible rules will be provided later").

## The checker

[proover.wl](proover.wl) is a generalized `ProofObject["ProofFunction"]` for
TSTP: it walks the steps and validates each, dispatching on the rule. The
backend is **internal** -- it reuses thvm's `Wolfram`Parser`TPTPImport` FOF
parser and decides each step in the Wolfram kernel. No external prover is called.

| rule                  | obligation                                                              | method |
| --------------------- | ----------------------------------------------------------------------- | ------ |
| `instantiate` (thm)   | conclusion is a substitution instance of a parent                       | `MatchQ` (TPTPImport renders `![X]` as pattern `X_`, so instances match) |
| `negated_conjecture` (cth) | conclusion is the logical negation of the parent                   | re-parse `~(parent)` through the same parser and compare canonically |
| `skolemize` (esa)     | a *fresh* Skolem symbol is introduced and used in the result            | structural: freshness against all prior steps + occurs in conclusion |
| `consequence` / `horn` / `deduction` (thm) | conclusion is entailed by the parents              | bounded Herbrand grounding + propositional `TautologyQ` |
| (final step)          | must be `$false`                                                        | `=== False` |

Unknown rules fall back to `NotVerified` (conservative, given the -10).

### Why a per-step checker, not a single entailment check

The bugs are rule-specific, not just "the final entailment is wrong". For
example, [example1_e](corpus/incorrect/Proofs/example1_e_proof.p) negates
`![X]: p(X)` as `![X]: ~p(X)` (the correct negation is `?[X]: ~p(X)`). The bad
step is the `negated_conjecture`; the downstream `$false` step is still a valid
consequence of the (wrongly negated) hypotheses. A naive "every step entails its
parents" checker would miss it. You have to check the *negation shape* for
`negated_conjecture` and the *freshness* for `skolemize`, because Skolemization
is only equisatisfiable (esa), not a logical consequence.

### The four example bugs, and how each is caught

| proof        | bug                                                              | caught by |
| ------------ | --------------------------------------------------------------- | --------- |
| example1_e   | negates `![X]:p(X)` as `![X]:~p(X)` instead of `?[X]:~p(X)`      | negation-shape mismatch |
| example2_e   | derives one disjunct of `A | B` via `deduction`                 | not entailed (tautology fails) |
| example3_e   | skolemizes Groom by reusing Bride's symbol `sK0`                | Skolem symbol not fresh |
| example4_e   | annotation says `sK1` but the formula uses the bound var `Marriage` | declared Skolem symbol absent from result |

## Running it

```
wolframscript -f wl/Examples/proover/check.wls            # bundled corpus
wolframscript -f wl/Examples/proover/check.wls <dir>      # any corpus dir
```

On the bundled 7-example corpus the checker scores a perfect +11 (three valid
proofs at +1, four buggy proofs at +2, no penalties):

```
example1_proof.p   valid  Verified        +1
example2_proof.p   valid  Verified        +1
example3_proof.p   valid  Verified        +1
example1_e_proof.p buggy  FailedVerified  +2   bad step: s1 (not the negation of the conjecture)
example2_e_proof.p buggy  FailedVerified  +2   bad step: s2 (conclusion is NOT entailed by the parents)
example3_e_proof.p buggy  FailedVerified  +2   bad step: groom (Skolem symbol is not fresh)
example4_e_proof.p buggy  FailedVerified  +2   bad step: groom (declared Skolem symbol absent from result)
```

`corpus/` holds the official `proover.zip` examples (correct/ and incorrect/);
the `correct/` vs `incorrect/` subtree is the gold label `check.wls` scores
against.

## The validation-workflow demo

[demo.wls](demo.wls) shows the companion story: a thvm `ProofObject` carries two
independent validators.

1. **Internal.** `po["ProofFunction"][]` replays every rewrite step in the
   Wolfram kernel and returns `Success[...]` (or `Failure[...]`). The
   ProofObject self-validates; no external system is needed.

2. **External (LeanLink).** The `Wolfram`LeanLink` paclet's `ProofToLean[po]`
   transpiles the ProofObject into a Lean 4 environment, and
   `LeanState[env["FinalGoal"]]["Complete"]` re-derives the goal in Lean's
   trusted kernel. This is the "independently checkable artifact in an external
   prover" leg.

```
wolframscript -f wl/Examples/proover/demo.wls
```

This maps onto ProoVer's own design note that a checker should "call external
provers for certain steps while independently verifying other proof steps
internally": internal step checks (the parser + the per-rule obligations above)
plus an external trusted backend (LeanLink) for an independent stamp.

### LeanLink note

`ProofToLean` / `LeanState` use LeanLink's embedded Lean runtime directly and
work regardless of the kernel's working directory. `LeanImportString` is a
different path -- it shells out to the `lean` binary to compile a temp `.lean`
and then loads the resulting `.olean`. On a box where the elan default toolchain
(`leanprover/lean4:stable`, currently 4.30.0) differs from the version the
LeanLink shim was built against (4.29.0-rc6), `LeanImportString` fails with
"Failed to load compiled file" (an olean version mismatch). The demo and the
checker do not use `LeanImportString`, so this does not affect them.

## Next steps

- Congruence handling for equality atoms (the entailment check currently treats
  `=` atoms as opaque booleans; sound for the examples, but a step that needs
  `f(a)=f(b)` from `a=b` would land in `NotVerified` rather than `Verified`).
- Handle the additional inference rules once the organizers publish them; until
  then unknown rules abstain.
- Optional: a LeanLink external cross-check on the `thm`-status entailment steps
  for proofs the internal backend marks `NotVerified`, to convert abstentions
  into decisions without risking the -10.
