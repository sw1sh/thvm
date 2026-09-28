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
exhibits a counter-model **of a `thm`/`cth` claim** (`esa` steps are never
judged by entailment -- equisatisfiability is not consequence); and left
undecided otherwise.  The first saturation runs without a term ordering, and
its demodulation then rewrites naively: an equation stated smaller side first
(`sk10 = sk7(sk10)`) grows an ever deeper tower and buries the refutation, so
an exhausted queue there witnesses nothing.  A counter-model is only reported
when an **ordered** re-run (ordered resolution, paramodulation and
demodulation under KBO) exhausts its queue too; an ordered refutation confirms
the step. Undecided steps yield `Unknown`. The whole pass is
resource-bounded: a saturation whose equality reasoning outgrows the term store
abstains to a clean `Unknown` rather than crashing, so every proof gets
exactly one verdict.

After ProoVer 2026 (-110/150, last place: 18 invalid proofs accepted at -10
each) the checker was rebuilt around abstain-by-default. The competition
binary (`CheckProof`, the *strict* profile; `FindProof --check` keeps the
legacy trusted-leaf profile for certifying FindProof's own proofs) adds:

- **Leaf verification.** The problem file named by each `file()` directive is
  loaded and the cited formula alpha-compared; a *semantic* mismatch
  (countermodel) is `VerifiedBad`, a benign variant abstains, `introduced()`
  abstains. The 2026 entry never opened the problem file, so leaf lies,
  `introduced()` smuggling, and wrong-conjecture imports all verified.
- **No silent drops.** A record the parser cannot read caps the verdict at
  `Unknown` (the old checker skipped such records and verified the rest).
- **Structure pass.** Duplicate names with differing formulas and citation
  cycles are `VerifiedBad` before any step check; parent resolution is
  order-independent (the rules impose no record order).
- **External-ATP hook.** Unsettled `thm` steps (and the `negated_conjecture`
  equivalence) are discharged by re-exec'ing the same binary as a prover
  (`THVM_CHECKPROOF_PROVE=1`) on exactly that step and its premises, as the
  rules permit, budgeted against the 30s wall
  (`THVM_PROOFCHECK_ATP` overrides the oracle, `THVM_PROOFCHECK_CACHE`
  caches decided obligations).
- **Cone-aware Skolem freshness.** A Skolem symbol may occur only in the
  introduction step's DAG descendants and never in the problem file;
  multi-symbol `new_symbols(skolem, [...])` lists are re-existentialised in
  declaration order for the positive parent-entailment check.

Validation runs on two corpora: `synth/` (the 2026-era self-generated set,
kept as a regression floor) and `tools/proover/openworld/` (harvest real
FindProof/Vampire TSTP proofs, mutate them with a spec-derived bug battery,
score with a 30% held-out slice whose hard requirement is zero false
accepts).

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

## The published 2026 set (a DEV metric)

The organizers published the 100 competition proofs after the event
(`ProoVer2026.zip`; 9 later withdrawn, 91 scored out of 138). The set is
public now, so its score is a development measurement, never a forecast:
every fix below is a general mechanism justified by the ProoVer rules or
the TSTP semantics, and must also hold on the open-world held-out slice.

| build | score /138 | invalid caught | valid confirmed | false accepts | false rejects |
| ----- | ---------- | -------------- | --------------- | ------------- | ------------- |
| competition entry (2026-07) | -103 | 25/47 | 28/44 | 17 | 11 |
| abstain-by-default rebuild  | -1   | 33/47 | 27/44 | 8  | 14 |
| + specified-rule checks     | +137 | 47/47 | 43/44 | 0  | 0  |

What the published proofs taught (each bullet is a mechanism, not a case):

- **The specified `skolemize` rule is syntactic.** Status `esa` and nothing
  else; one parent; one `new_symbols(skolem, [sK])`; a `skolemize(Var,
  sK(...))` annotation naming an existential *in polarity* (a positive `?`
  or a negative `!`, never under `<=>`); Skolem arguments covering the
  universals the eliminated subformula depends on; and the result exactly
  the parent with that one binder removed. Several invalid proofs were
  semantically harmless (skolemizing a universal is just an instance) and
  still invalid by the rule.
- **Status claims are checked by their meaning.** `thm`: premises entail
  the conclusion. `esa`: equisatisfiable by a model *expansion*, so a step
  introducing no new symbol must be entailed. `cth`: brings a hypothesis
  into the refutation, legitimate only as the negation of the conjecture.
- **The conjecture enters a refutation only through its negation**; a
  derivation of `$false` that cites the conjecture any other way assumed
  its goal.
- **Leaves are imports.** A record with no source, a cited name absent
  from the problem file, and a dangling citation are all violations.
- **Variables are binder-fresh.** Every quantifier binding gets its own
  id, so shadowed binders (`![X]:..?[X]:..`) can never be conflated by
  the clausifier, the matcher, or the shape checks.

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
