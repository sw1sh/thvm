# Vampire-parity feasibility probe

Date: 2026-07-04. Bounded feasibility probe (no corpus sweep; one
theorem run). Question posed by the ATP parity loop after the
Waldmeister track substantially closed: the loop names "Vapire /
VampireProcess" as the next external tool. Is a Vampire-parity arc
ACTIONABLE and WELL-POSED here, on the Waldmeister model?

Verdict up front: **(C) ILL-POSED as a byte-trajectory parity arc**,
with the refinement that an external Vampire reference IS present (so
this is not the "no reference on the box" failure mode). The only
well-posed thvm-vs-Vampire comparison is proof-existence + structural
proof-shape (construct count), and that comparison is **already built,
run, and committed**. There is no Waldmeister-style byte-identical
selection-stream parity to pursue, because thvm and Vampire run
different calculi.

## 1. Inventory (what actually exists)

External binary: **present.** Vampire 5.0.1 via Homebrew at
`/opt/homebrew/bin/vampire` (`../Cellar/vampire/5.0.1/bin/vampire`).
`brew list` confirms the `vampire` formula.

thvm integration, WL side:
- `wl/THVMLink/Kernel/ATP/Vampire.wl` -- `TVampireProof[file]` /
  `TVampireProof[theory, thm]` shells out to the CLI
  (`vampire --mode casc --proof tptp -t <sec> <file>`), parses the SZS
  derivation via `Wolfram`Parser`TPTPImport[..., "SZS"]`, folds
  parse-time `reorient_equations`, and returns
  `<|Status, Strategy, Seconds, ProofLength, Inferences, RawSZS|>`.
- `Method -> "VampireProcess"` in `TFindProof` is the in-portfolio
  dispatch to that CLI (ATP.wl:4080-4105). `TVampireProofObject`
  threads the SZS result through `TSZSDerivationToProofObject` to lift
  it into a thvm-shaped ProofObject **for structural comparison**
  (docs say verbatim: "so proofs from differing Methods -- an internal
  preset versus the external Vampire CLI -- can be compared
  structurally").

thvm's `"VampireUEQ"` is a **WL Method preset, not an engine mode.**
It is a bundle of thvm's own unfailing-completion suboptions
(ATP.wl:2855): `Ordering->"LPO", AutoPrecedence->True,
SelectionRatio->10, UnfailingCP->True, AutoMaxWeight->True,
BackwardSubsume->True, BackwardDemod->True, RHSInterreduce->True`,
"modeled on the Vampire 5.0.1 portfolio entry that cracks
ShefferAxioms/AndAssociativity." Siblings `"VampireUEQDefault"`
(mirrors the first slot of Vampire's UEQ schedule, KBO + 1:1 + SInE)
and `"VampireRandom"` are likewise presets over the SAME thvm
completion engine. The C side (`src/atp/_.c`, `src/atp/precedence.c`)
has Vampire-INSPIRED knobs implemented inside thvm's completion loop
-- LRS queue-prune (`THVM_ATP_LRS`), random CP pick (`--sos`-spirit),
forward/backward subsumption analogs, `bd=all` demod, occurrence /
reverse-frequency precedence -- but these are analogs living in a
Knuth-Bendix completion engine, NOT an emulation of Vampire's
given-clause saturation loop. `docs/ATP/vampire_port.md` catalogs the
ported flags and explicitly separates them from concepts that are
"NOT a Vampire port" (CPSetInterreduce, MNF front, UnfailingCP).

Committed baseline artifacts (comparison already done):
- `tools/baselines/vampire_notable_theorems.tsv` -- 148 cases,
  `--mode portfolio`, status + seconds.
- `tools/baselines/vampire_raw/*.out` -- ~150 full SZS proof outputs.
- `tools/baselines/compare_vampireueq_vs_vampire.tsv` --
  thvm-VampireUEQ status/secs vs CLI status/secs -> match tag.
- `tools/baselines/parity_vampire.tsv` -- `preset_constructs` vs
  `vampire_constructs` with `cli_minus_preset` delta and notes.
- `tools/parity_diff_vampire.sh` -- joins thvm tracker vs CLI TSV on
  **status** (OK_OK / *_FAIL_only / BOTH_FAIL), not trajectory.
- `docs/ATP/vampire_case_teardown.md`, `docs/ATP/vampire_port.md` --
  prior deep teardown + flag-port catalog.
- `wl/THVMLink/docs/Symbols/TVampireProof*.md` -- documented surface.

## 2. Well-posedness

Waldmeister parity was tractable for one reason (stated in
`wl/THVMLink/docs/Tutorials/Waldmeister.md`): **every comparison there
is one Waldmeister implementation against another.** `wmcli` is the
reference Waldmeister binary and FEQ is the Wolfram Language's own
Waldmeister-based prover; thvm is an in-process faithful Waldmeister.
The same algorithm (unfailing Knuth-Bendix completion, same ordering +
selection heuristic) yields a single deterministic given-clause /
lemma stream that thvm reproduces byte-for-byte. thvm even ships an
env-gated CP-selection trajectory dump (`src/atp/_.c:10951`) whose diff
target is `wmcli -P verbose` -- a completion stream.

Vampire is a **different algorithm class**: superposition-based
saturation that refutes the NEGATED conjecture. Concretely, on the
simplest UEQ theorem in the corpus (`GroupAxioms/InverseOfInverse`,
3 axioms, goal `not(not(x))=x`), a fixed deterministic Vampire
strategy (`--saturation_algorithm otter --selection 10
--age_weight_ratio 1:10`) produces an SZS refutation whose steps are:

```
f5 negated_conjecture  not(not(sk_c1)) != sk_c1     (goal Skolemized)
f4 reorient_equations  ...                          (parse-time flip)
f7 superposition       f1,f2
f8 superposition       f1,f4
f44 forward_demodulation f31,f2
f68 superposition      f44,f55   ->  not(not(X0)) = X0
f76 superposition      f6,f68    ->  sk_c1 != sk_c1
f81 trivial_inequality_removal   ->  $false
```

The proof object is: clauses, a negated + Skolemized conjecture
(`sk_c1`), inference rules `superposition` / `forward_demodulation` /
`reorient_equations` / `trivial_inequality_removal`, terminating in
`$false`. thvm's completion proof of the same theorem is: orient the 3
axioms as rewrite rules, close critical pairs to derive the lemma
`not(not(X))=X`, join the goal LHS/RHS to a common normal form -- a
FORWARD equational derivation over rules/equations, with no negated
conjecture, no Skolem constant, no `$false`.

Consequence for a byte-trajectory (given-clause selection stream)
comparison:
- **Not well-defined.** The two streams range over different objects
  (Vampire: clauses in a refutation; thvm: rewrite rules in a
  completion), start from different problems (Vampire negates +
  Skolemizes the goal; thvm keeps the universal goal), and end
  differently (`$false` vs joined normal forms). There is no
  step-to-step alignment, even under a FIXED deterministic Vampire
  strategy. Fixing the strategy removes Vampire's portfolio
  nondeterminism but does not make its clause-selection loop into a
  completion CP-selection loop.
- Vampire's shipped default is worse for parity still: `--mode
  portfolio` / `casc` runs a large `-auto` UEQ schedule
  (`CASC/Schedules.cpp`) of dozens of strategies with random seeds
  (`random_seed=559079425` appears in the raw output), sliced by
  wall/instruction budget. Which strategy "wins" is time-dependent.
- The **meaningful** comparison is set-level / proof-shape:
  (a) proof existence (does thvm's VampireUEQ preset also prove it?),
  (b) proof length / construct count, (c) fuzzy lemma-overlap (does
  thvm's completion derive the same key intermediates Vampire's
  superposition proof uses, e.g. `and(e,X)=X`, `not(not(X))=X`?).
  Metrics (a) and (b) are exactly what the committed baselines
  measure -- and they already show EXPECTED nonzero deltas because the
  calculi differ: `parity_vampire.tsv` records
  `GroupAxioms/InverseOfInverse` 14 vs 18 constructs (+4, "vampire
  externalizes more steps"), `BooleanAxioms/DoubleNegation` +7
  ("different decomposition"), `AbelianGroupAxioms/InverseOfInverse`
  12 vs 12 (the lone exact match). A construct-count DELTA is the
  right yardstick for cross-calculus proofs; a byte-identical
  trajectory is not a target that can exist.

## 3. Simplest-theorem run (external Vampire only; no heavy thvm run)

Ran external Vampire on `GroupAxioms/InverseOfInverse`, fixed
deterministic strategy (above): PROVED, `$false` reached, 13 SZS
steps. proof-existence side confirmed. The thvm-side counterpart is
already banked: `compare_vampireueq_vs_vampire.tsv` row 1 shows
thvm-VampireUEQ AGREE-PROVED with the CLI on the easy abelian cases,
and `parity_vampire.tsv` shows the construct deltas above. No heavy
WolframKernel run was warranted -- it would only re-derive committed
numbers and cannot make an ill-posed trajectory comparison well-posed.
First divergence: N/A -- a stream comparison is not well-posed, so
there is no first-divergence index to find (contrast Waldmeister,
where "first divergent pick" is the core diagnostic).

## 4. Verdict

**(C) ILL-POSED** for a Waldmeister-style byte-trajectory parity arc,
qualified: the external Vampire reference IS on this box (rules out
(B)), and the well-posed comparison it supports -- proof existence +
proof-shape/construct-count -- is already built, run, and committed.
Vampire's superposition/saturation refutation has no byte-comparable
analog to Waldmeister's completion selection stream; "VampireUEQ" is
thvm's own completion engine wearing a Vampire-flavored preset, not a
Vampire emulator, so there is nothing whose trajectory could be
diffed against Vampire's.

Honest next directions for the loop (all real, none a "byte-parity"
arc):
1. **Speed** (well-posed, already scoped): `vampire_case_teardown.md`
   measures thvm-VampireUEQ 0.69s vs Vampire-CLI 0.06s -- an ~11x wall
   gap on a trivial 5-axiom problem. Closing it is a thvm-engine
   optimization arc, not a parity arc.
2. **Coverage / config** (well-posed): `parity_vampire.tsv` flags
   cases where the preset FAILs but Vampire proves
   (`HillmanAxioms/Commutativity`, `MeredithAxioms/Commutativity`,
   noted "design tension" -- a nearby config cracks them). Tune the
   preset toward the exact Vampire UEQ-schedule slot
   (KBO + 1:1 + SInE, per the teardown's config-gap table) and measure
   PROVED-count movement.
3. **Proof-shape / lemma-oracle** (partially done): use Vampire's SZS
   proofs as an oracle for which lemmas matter -- already the spirit
   of the ENIGMA learned CP scorer.

Recommendation: do NOT open a "Vampire byte-parity" round. The
Waldmeister track's parity axis is intrinsic to a same-algorithm
comparison and does not transfer. If the loop wants a Vampire arc,
frame it as speed (lever 1) or coverage (lever 2) against the existing
proof-existence baseline -- both measurable, both honest, neither
requiring a fictitious trajectory alignment.

## 5. Gates

No source touched (only this doc + a memory note). Bench/test gates
not exercised -- nothing to regress.
