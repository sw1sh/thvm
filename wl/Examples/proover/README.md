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
| `VerifiedBad` | proof is invalid   | +2             | **-10**        |
| `VerifiedGood`       | proof is valid     | +1             | -1             |
| `Unknown`    | cannot decide      | 0              | 0              |

The asymmetry is the whole game: calling a buggy proof good costs **-10**, while
abstaining costs 0. The optimal checker is conservative: stamp `VerifiedGood` only
when every step is independently confirmed, and fall back to `Unknown` the
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

The competition entry is **`CheckProof`**, a single self-contained C
binary -- built from the same sources as the FindProof CASC UEQ prover (see `tools/proover/`) --
with no Wolfram kernel and no external prover. A first-order TSTP proof is read
by a hand-written recursive-descent parser into the engine's term
representation, and each derived step is validated by dispatching on its
inference rule. Every entailment obligation is discharged by the engine's own
clausal saturation prover (resolution with superposition and paramodulation for
equality), the same core the equational prover is built on, so the checker
reasons with full equality rather than treating `=` atoms opaquely.

| rule                  | obligation                                                              | method |
| --------------------- | ----------------------------------------------------------------------- | ------ |
| `instantiate` (thm)   | conclusion is a substitution instance of a parent                       | first-order matching, backed by entailment |
| `negated_conjecture` (cth) | conclusion is the logical negation of the parent                   | each side entails the other (mutual entailment), so a wrong quantifier dual is caught soundly |
| `skolemize` (esa)     | a *fresh* Skolem symbol is introduced and used in the result            | structural: freshness against all prior steps + occurs in conclusion |
| `consequence` / `horn` / `deduction` (thm) | conclusion is entailed by the parents              | clausify parents + negated conclusion, saturate to the empty clause |
| (final step)          | must be `$false`                                                        | refute the parents |

A step is accepted when the saturation derives the empty clause; rejected as
unsound (`VerifiedBad`, with the step named) only when a completed saturation
exhibits a counter-model; and left undecided otherwise. Undecided steps -- an
unknown rule, or an entailment the bounded saturation neither proves nor refutes
-- yield `Unknown`. A `VerifiedBad` verdict is emitted only when a step
is certainly unsound, honouring the -10 scoring. The whole pass is
resource-bounded: a saturation whose equality reasoning outgrows the term store
abstains to a clean `Unknown` rather than crashing, so every proof gets
exactly one verdict.

[proover.wl](proover.wl) is the Wolfram Language prototype of the same design:
it reuses thvm's `Wolfram`Parser`TPTPImport` FOF parser and decides each step in
the kernel (bounded Herbrand grounding + `TautologyQ` for entailment). The C
binary supersedes it as the standalone competition entry.

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

The C entry confirms three of these outright (the negation shape, and both
Skolem checks). For example2_e it abstains: establishing that a single disjunct
is *not* entailed by the equational disjunction needs more than the bounded
saturation certifies, so it reports `Unknown` rather than risk a wrong
verdict. The WL prototype decides it via bounded grounding.

## Running it

The competition path is the standalone C binary, one proof per invocation:

```
CheckProof --szs proof.p            # -> % SZS status <verdict> for proof.p
```

On the bundled 7-example corpus it scores **+9** with no -10 exposure: the three
valid proofs `VerifiedGood` (+3), and three of the four buggy proofs
`VerifiedBad` with the faulty step localised (+6). The fourth
(example2_e) sound-abstains to `Unknown`.

```
example1_proof.p    VerifiedGood
example2_proof.p    VerifiedGood
example3_proof.p    VerifiedGood
example1_e_proof.p  VerifiedBad   bad step: s1 (not the negation of the conjecture)
example3_e_proof.p  VerifiedBad   bad step: groom (Skolem symbol is not fresh)
example4_e_proof.p  VerifiedBad   bad step: groom (declared Skolem symbol absent from result)
example2_e_proof.p  Unknown      (deduction over an equational disjunction -- sound abstain)
```

The Wolfram Language prototype runs the same corpus and additionally rejects
example2_e via bounded grounding, for +11, trading the soundness margin the C
entry keeps:

```
wolframscript -f wl/Examples/proover/check.wls            # bundled corpus
wolframscript -f wl/Examples/proover/check.wls <dir>      # any corpus dir
```

`corpus/` holds the official `proover.zip` examples (correct/ and incorrect/);
the `correct/` vs `incorrect/` subtree is the gold label both checkers score
against.

## Two StarExec tarballs, one code base

Each competition gets its own self-contained submission, both built from the
same unity-build sources:

- `tools/casc/mk_tarball.sh` -> `FindProof.tgz` (CASC UEQ proving;
  `bin/starexec_run_default` runs the portfolio with `--schedule --szs --proof`)
- `tools/proover/mk_tarball.sh` -> `CheckProof.tgz` (ProoVer checking;
  `bin/starexec_run_default` runs `CheckProof --szs`; the binary name selects
  the checking mode)

Each `starexec_build` compiles its one binary; no external dependencies beyond
the C standard library and libm. See `tools/casc/README` and
`tools/proover/README`.

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
  `f(a)=f(b)` from `a=b` would land in `Unknown` rather than `VerifiedGood`).
- Handle the additional inference rules once the organizers publish them; until
  then unknown rules abstain.
- Optional: a LeanLink external cross-check on the `thm`-status entailment steps
  for proofs the internal backend marks `Unknown`, to convert abstentions
  into decisions without risking the -10.
