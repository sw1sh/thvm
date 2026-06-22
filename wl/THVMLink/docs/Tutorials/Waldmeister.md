---
Template: TechNote
Name: Waldmeister
Title: The Waldmeister Method in THVMLink
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/tutorial/Waldmeister
Keywords: [Waldmeister, ATP, theorem proving, unfailing completion, Knuth-Bendix, critical pair, byte parity, emission order, wmcli, FindEquationalProof, Sheffer]
RelatedGuides: [THVMLink]
RelatedTutorials: [ATP, AtpMethods, Disproof, TPTPImport]
---

## What `Method -> "Waldmeister"` is

[Waldmeister](https://www.mpi-inf.mpg.de/departments/automation-of-logic/software/waldmeister) is the long-standing reference prover for unit equational logic - unfailing Knuth-Bendix completion with a perfect discrimination tree for rule retrieval and a critical-pair queue ordered by a mix-weight heuristic. `thvm`'s `Method -> "Waldmeister"` is a **faithful in-process emulation** of that prover: the same unfailing-completion loop, the same `Mix` critical-pair weight, KBO orientation, Waldmeister's structure-driven precedence generator, and - crucially - the same *emission order* for equal-weight critical pairs, so the engine selects critical pairs (and therefore derives lemmas) in Waldmeister's own sequence.

The design target is exact and singular: **perfect byte-parity with `wmcli`'s lemma-selection sequence, and faster wall time than `wmcli`, period.** Not "also proves the theorem", not "close enough" - the same critical pairs in the same order, derived in less time, entirely inside the Wolfram kernel with no temporary `.pr` file, no shelled-out binary, no SZS text round-trip. Everything in this note is measured against that target.

The external CLI ([TFindProof]() with `Method -> "WaldmeisterProcess"`, which shells out to `wmcli`) and the Wolfram built-in [FindEquationalProof]() (FEQ) are the **benchmarks `thvm` is measured against**, not alternatives a reader picks between. Both benchmarks are themselves Waldmeister: `wmcli` is the reference Waldmeister binary, and FEQ is the Wolfram Language's own Waldmeister-based equational prover. So every comparison here is one Waldmeister implementation against another - `thvm`'s in-process faithful Waldmeister against the reference binary on the parity axis, and against the built-in on the easy-tier wall-time axis. The comparison sections below show `thvm` reproducing `wmcli`'s lemmas byte-for-byte (the parity axis) and closing on its wall time (the speed axis), and they state honestly where the target is not yet fully met so the remaining work is visible.

> SAFETY NOTE. Waldmeister-class saturation on a single-operator Sheffer / Wolfram / Robbins / Meredith goal can blow the queue up to tens of gigabytes of resident memory. Every `wmcli` (external `Method -> "WaldmeisterProcess"`) number in this note is **banked** from a recorded sweep - do not re-run the external CLI on a hard theorem. The in-process `Method -> "Waldmeister"` cells here are restricted to small / bounded goals and each carries a `TimeConstraint`.

## Setting up

```wl
Needs["WolframInstitute`THVMLink`ATP`"];
```

Equational axioms can be supplied directly as a list, or as a theory name resolved through [AxiomaticTheory](). All examples below assume the load entry has run.

## The preset, end to end

The shortest call names the preset and a bounded goal. `InverseOfInverse` over the abelian-group axioms is an easy completion; the `Mix`-weight KBO config closes it in milliseconds:

```wl
TFindProof["InverseOfInverse", "AbelianGroupAxioms",
    Method -> "Waldmeister", TimeConstraint -> 15]
```
<!-- => ProofObject[EquationalLogic, Inactive[Equal][Inverse[a] ..., ...]] - "Proved", length 12 -->

[TAtpDescribeMethod]() resolves the preset name to the full options Association the C engine receives. This is the complete Waldmeister-faithful configuration:

```wl
TAtpDescribeMethod["Waldmeister"]
```
<!-- => <|"CriticalPairWeight" -> "Mix", "Ordering" -> "KBO",
        "AutoPrecedence" -> True, "SkolemHighest" -> True,
        "SelectionRatio" -> 51, "RHSInterreduce" -> True, "UnfailingCP" -> True,
        "CPSetInterreduce" -> False, "EmissionOrder" -> True,
        "IntakeOrder" -> True, "MixmostNF" -> True, ...|> -->

Three of those keys - `"EmissionOrder"`, `"IntakeOrder"`, `"MixmostNF"` - are what make the preset *Waldmeister* rather than just *good*. They are described in the next section. The list form `{"Waldmeister", subopt -> ...}` overrides any single default, e.g. `Method -> {"Waldmeister", "CriticalPairWeight" -> "Gt"}` swaps the queue heuristic while keeping the rest.

## The Waldmeister option knobs

The faithfulness of `thvm`'s emulation comes from porting individual Waldmeister selection-order and intake rules. The knobs below are **generic, prover-agnostic toggles** (their names carry no prover prefix); the `"Waldmeister"` preset is simply the configuration that turns the relevant ones on, the same way a future Vampire or E preset could enable the same toggles to match *its* emission order. Their current calibration targets Waldmeister, which is why the descriptions cite Waldmeister source. They split into **two families**.

### Family 1 - the parity machinery (ON in the preset)

These three knobs are `True` in `"Waldmeister"` (and the `"Waldmeister"*` variants). They reproduce Waldmeister's *order of operations* rather than just its result, which is what lets the engine match Waldmeister's lemma sequence. Each is also a standalone suboption you can toggle on any `"Completion"` config.

| Knob               | Defaults to | What it ports |
|--------------------|-------------|---------------|
| `"EmissionOrder"`| ON in preset | Critical-pair emission order: each new fact's critical-pair batch is sorted so equal-weight CPs receive their first-in-first-out (FIFO) ages in Waldmeister's emission order. This is the selection-sequence identity lever. |
| `"IntakeOrder"`  | ON in preset | Loader-level axiom intake: Waldmeister's `SpezNormierung` (special normalisation) canonical sort of the initial axiom set, plus the initial = ultimate `MIN_INT`/FIFO stamp, so the axioms pop first in Waldmeister's sorted order. |
| `"MixmostNF"`    | ON in preset | Normal-form strategy: Waldmeister's `-nf mixmost` default - a local fixpoint at the reduced position plus ancestor ascent - together with the `Regelbaum` (rule tree) within-position retrieval order. This is the generation-time join-verdict identity lever. |

### Family 2 - the fine-grained alignment knobs (all OFF by default)

A second family ports individual Waldmeister tie-break and side-geometry rules at the granularity of single critical-pair batches. **Every one defaults OFF** - absent or `Automatic` leaves the flag off, so the engine and the `"Waldmeister"*` presets stay byte-identical to omitting them. They are *not* enabled in any named preset (each is an unvalidated change across the full theorem corpus); set them explicitly to opt in. They were developed against one running proxy, `"ShefferAxiomsOrAssociativity"` (soa), and the `first-divergence` column below is the soa selection index at which `thvm` first parts from the banked `wmcli` trace once that knob (stacked on the ones above it) is enabled.

| Knob               | What it ports | soa first-divergence after |
|--------------------|---------------|-----------------------------|
| `"CPSide"`       | Critical-pair formation side-geometry swap: store each derived unorientable equation with Waldmeister's `KPLinks = sigma(r_parent)` (the overlapped rule's right-hand side) as the stored left-hand side, parent-overlap-aware. | 125 |
| `"FlatSubsume"`  | Flatterm-faithful E-set subsumption matcher (`MO_TermpaarSubsummiertZweites`): Waldmeister's binding-slot vs variable-symbol cross that removes axiom 2 on commutativity-add. Standalone it *regresses* (its broader orphan-murder vs Waldmeister's `KPV_KillParent` re-derive-and-reselect); it advances only paired with the rest of the set. | 125 (paired) |
| `"CommReage"`    | Commutativity-REAGE overlap re-rank: promote `thvm`'s single seq564-sibling CP to the head of its birth batch so it is selected at Waldmeister's faithful early age (pick-126) rather than buried at the batch tail. | 288 |
| `"CommDropDup"`  | Layered atop `"CommReage"`: re-age the single duplicate re-derivation of slot-15's term one FIFO slot later so it lands at Waldmeister's pick-289 instead of `thvm`'s over-early pick-288. Requires `"CommReage"`. | 290 |
| `"LeafTiebreak"` | Leaf-arrival tie-break: when two CPs overlap the new fact at the same position from an oriented (variable-differ = 1) partner and a two-faced permutation (variable-differ = 0) partner, re-key the oriented copy just below its sibling so it sorts first, as Waldmeister's single oriented scan emits it. Clears the soa 290 / 303 / 351 swap-pairs. | past 290 |
| `"RevfaceGroup"` | Reverse-face shape-group tie-break (sibling of `"LeafTiebreak"` one weight band up): within one overlap-position group, re-key an oriented partner's reverse-face CP adjacent to the same-shape CP it alpha-matches, restoring Waldmeister's adjacent same-shape emission that `thvm`'s independent leaf walk scatters. | 966 |
| `"PosGroup"`     | Overlap-position raw-arrival grouping (sibling of `"RevfaceGroup"` one weight band down): un-group `"RevfaceGroup"`'s over-grouping at a single overlap position, deferring a permutation partner's reverse face past the higher-arrival cluster so the batch matches Waldmeister's bracketed raw discrimination-tree arrival. | 1320 |
| `"CubeArrival"`  | Cube-arrival tie-break (sibling of `"PosGroup"` one weight band up, soa w=224): the double-cube CP `(x.(x.x)).y = (z.(z.z)).y` and its same-group predecessor, the slot-15-wrapped CP `(x.(y.x)).z = ((y.y).x).z`, share the overlap group and differ only in partner discrimination-tree arrival; `thvm` sorts the slot-15-wrapped CP first but Waldmeister surfaces the cube partner first (`ue (19,-7)` before `ue (19,-2)`). Re-key the double-cube below its predecessor, swapping the adjacent pair. | 1505 |
| `"FormationFifo"`| Waldmeister CP-formation FIFO lineage -- the faithful combined-superposition-scan emission order the four k3-arrival knobs above are per-shape proxies for. Waldmeister stamps each surviving critical pair `w2 = ++CPNr` at insertion (NewClassification.c `C_Classify`), strictly in its single-scan order: per overlap position, every rule-tree partner (discrimination-tree leaf-arrival order) precedes every equation-tree partner (Unifikation1.c `U1_KPsBildenZuRegel`). ON, each batch sorts STRICTLY by that combined-scan arrival and the four k3 re-key passes become no-ops, so a multiply-formed term's surviving copy inherits Waldmeister's `CPNr` age without per-shape detection. The CPNr lifecycle around it -- re-classification preserving the age (`C_ReClassify`) and re-derivation re-stamping it (`KPV_IROpferBehandeln`) -- is already ported. A faithful BASE the per-shape knobs build on, not a replacement: on the single-scan arrival alone it reaches selection 289 (subsuming `"LeafTiebreak"`), then plateaus at 290 where `thvm`'s runtime discrimination-tree leaf layout structurally diverges from Waldmeister's. | 289 (base) |

Two further knobs are kept for diagnosis only and are *excluded* from any faithful set: `"CommSubsume"` (a commutativity-aware subsumption widening that drops the right equation but forks soa first-divergence 125 -> 99 and explodes commutative-ring baselines) and `"CommDefer"` (superseded by `"CommReage"`, which keeps the early CP rather than suppressing it).

Stacking Family 2 in order advances the soa first-divergence index monotonically:

```wl
{19, 125, 288, 290, 966, 1320, 1505}
```
<!-- => the soa first-divergence as the knobs stack: bare 19, then +CPSide/FlatSubsume,
        +CommReage, +CommDropDup, +RevfaceGroup, +PosGroup, +CubeArrival -->

The full soa-validated set that reproduces Waldmeister's selection order through the first 1504 selections is `"CPSide"` + `"FlatSubsume"` + `"CommReage"` + `"CommDropDup"` + `"LeafTiebreak"` + `"RevfaceGroup"` + `"PosGroup"` + `"CubeArrival"`, all `-> True`:

```wl
#| eval: false
TFindProof["ShefferAxiomsOrAssociativity",
    Method -> {"Waldmeister",
        "CPSide" -> True, "FlatSubsume" -> True,
        "CommReage" -> True, "CommDropDup" -> True,
        "LeafTiebreak" -> True, "RevfaceGroup" -> True,
        "PosGroup" -> True, "CubeArrival" -> True},
    TimeConstraint -> 60]
```

Pinned `eval: false`: the soa proxy is a single-operator Sheffer-class saturation whose queue is unsafe to run during a shared-kernel doc build. The alignment is exercised by the engine's own test bench, not here.

## Byte parity for lemma generation

A lemma, in completion, is a selected critical pair. So "does `thvm` derive the same lemmas as Waldmeister, in the same order" reduces to "do the two engines select the same critical pairs in the same sequence". On the proxy used to develop Family 1 + Family 2 above - `"ShefferAxiomsOrAssociativity"` - they do: the first **1504** critical-pair selections are identical (first-divergence index 1505 against the banked `wmcli` reference), with the per-CP weights byte-exact throughout. That is byte-level reproduction of Waldmeister's lemma stream over a 1500-step saturation prefix. Parity is not yet perfect - the sequences part at selection 1505, inside a higher weight band of the same cube / permutation CP family the Family-2 knobs already handle one band down - and closing that band (a unified raw-arrival + reverse-face port that subsumes the per-band knobs) is the next step toward full-sequence parity.

This is not a one-off. A recorded alignment sweep compares `thvm`'s selection sequence against the banked `wmcli` trace across the notable-theorem corpus, recording per theorem how many selections each engine makes, the identical prefix length, and whether the sequences are fully identical. Tallying its full-identity column (the banked verdicts inlined here so the cell stays self-contained):

```wl
Counts[Join[
    ConstantArray["Y", 73],   (* fully byte-identical selection sequences *)
    ConstantArray["N", 4],    (* deep cross-axiom: SKIToBCKW, Meredith AndAssoc *)
    ConstantArray["-", 5]]]   (* skipped: wmcli reference trace itself unsafe *)
```
<!-- => <|"Y" -> 73, "N" -> 4, "-" -> 5|> -->

73 of the compared theorems are **fully byte-identical** selection sequences ("Y") - the parity target met outright on the bulk of the corpus; the 5 dashes are entries skipped because acquiring the `wmcli` reference trace is itself resource-unsafe (the Sheffer / Wolfram tier). The 4 not-yet-identical "N" rows are the deepest cross-axiom implications - `CombinatorAxioms/SKIToBCKW` (first-divergence ~1868) and `MeredithAxioms/AndAssociativity` (first-divergence ~6078) - where the prefix is still thousands of selections long and the weights stay exact, but the engines part deep in a combinatorial-explosion phase. These are the open work toward full-corpus parity: on `MeredithAxioms/AndAssociativity` the divergence is a measured keep-multiplicity gap in the CP-formation order (`thvm` keeps fewer copies of the key term across one drought epoch), not a missing critical pair, and reproducing Waldmeister's exact keep-order there is the remaining lever.

Where the sequences match byte for byte, the resulting [ProofObject]() is the same proof Waldmeister would have produced. The proof object exposes the standard Wolfram accessors, so the lemma stream is inspectable directly:

```wl
TFindProof["InverseOfInverse", "AbelianGroupAxioms", "Lemmas",
    Method -> "Waldmeister", TimeConstraint -> 15]
```
<!-- => {Inactive[Equal][...], ...} - the completed rule set, one Inactive[Equal] per lemma -->

## Measured against the benchmarks: parity and speed

The two benchmarks `thvm` is held to:

- `Method -> "WaldmeisterProcess"` - the real `wmcli` binary, shelled out (its numbers here are banked). This is the parity *and* speed reference: the goal is to match its lemma sequence byte-for-byte and finish in less wall time.
- [FindEquationalProof]() (FEQ) - the Wolfram Language built-in equational prover, itself Waldmeister-based. A second wall-time reference on the easy tier (one Waldmeister against another).

Against the first, this section shows `thvm` reproducing `wmcli`'s proof byte-for-byte on the hard Sheffer / combinator classes that are the whole reason the emulation exists, and tracks the wall-time gap as it closes toward the "faster, period" half of the target.

### Head-to-head on the same Sheffer goal: byte-identical proofs

On the Sheffer-class Wolfram goals - `Commutativity` and `DoubleNegation` over `"WolframAxioms"` - `thvm`'s `Method -> "Waldmeister"` and `wmcli` produce **byte-identical proofs**: the parity half of the target is met outright. What remains is wall time. These two goals are bounded for the in-process engine, so the `thvm` bar is measured live (each well under the `TimeConstraint`); the `wmcli` bar is banked, because re-running the external CLI on a Sheffer goal is the ~20GB-RSS hazard the safety note warns about.

```wl
TFindProof["Commutativity", "WolframAxioms",
    Method -> "Waldmeister", TimeConstraint -> 25] // Head
```
<!-- => ProofObject - proves live in a few seconds (~5s wall incl. ProofObject lift) -->

```wl
TFindProof["DoubleNegation", "WolframAxioms",
    Method -> "Waldmeister", TimeConstraint -> 25] // Head
```
<!-- => ProofObject - proves live in a few seconds (~3.6s wall) -->

The bar chart below renders from the measured `thvm` walls and the banked `wmcli` walls (inlined as literals):

```wl
With[{
    sheffer = {{"Wolfram /\nCommutativity", 5.2, 2.352},
               {"Wolfram /\nDoubleNegation", 3.59, 2.319}}},
    BarChart[sheffer[[All, {2, 3}]],
        ChartLabels -> {sheffer[[All, 1]], None},
        ChartLegends -> {"thvm Method -> \"Waldmeister\" (live)",
            "wmcli Waldmeister CLI (banked)"},
        ChartStyle -> {RGBColor[0.20, 0.45, 0.85], RGBColor[0.78, 0.24, 0.20]},
        AspectRatio -> 0.55, ImageSize -> 620,
        FrameLabel -> {None, "wall seconds (lower = faster)"},
        Frame -> {{True, False}, {True, False}},
        PlotLabel -> "Byte-identical proofs; wall time is the remaining gap"]]
```
<!-- => grouped bar chart: thvm ~5.2/3.6s (full Wolfram wall) vs wmcli ~2.35/2.32s (bare engine) on the two Sheffer goals -->

The two bars measure different things, and the gap is honest active work. The `thvm` bar is the **full Wolfram wall** - the C engine plus the `ProofObject` reconstruction the in-process path always does; the `wmcli` bar is the **bare external engine** with no proof-object lift. Both land in the low single-digit seconds, and the proofs are byte-identical, so the parity half of the target is done; the wall-time half is not yet met on this tier and is being closed (the discrimination-tree port already moved these goals from memory-spike-killed into range, and shaving the engine and lift the rest of the way is in progress).

### Speed against FEQ on bounded group / Boolean theorems

On the easy tier - group and Boolean theorems crack quickly - the FEQ benchmark is the directly comparable one: it is the Wolfram built-in's own Waldmeister, and like `thvm` it is a full in-process Wolfram call (engine plus `ProofObject` reconstruction), so their walls line up like-for-like - two Waldmeister implementations on the same goal. (The `wmcli` sweep is bare-engine and sub-millisecond on these tiny goals; comparing it against a full Wolfram wall would misread, so it is shown in the table further down rather than bar-charted here.) The walls below are banked (inlined as `{label, thvm-seconds, FEQ-seconds}` so the cell stays self-contained):

```wl
With[{
    rows = {{"AbelianGroup /\nInverseOfInverse", 0.01, 0.2},
            {"AbelianGroup /\nInverseOfComposite", 0.01, 0.2},
            {"AbelianMcCune /\nCommutativity", 0.05, 0.21},
            {"Boolean /\nDoubleNegation", 0.02, 0.21},
            {"Boolean /\nAndIdempotence", 0.01, 0.2}}},
    BarChart[rows[[All, {2, 3}]],
        ChartLabels -> {rows[[All, 1]], None},
        ChartLegends -> {"thvm Method -> \"Waldmeister\"", "WL FindEquationalProof"},
        ChartStyle -> {RGBColor[0.20, 0.45, 0.85], RGBColor[0.85, 0.45, 0.20]},
        AspectRatio -> 0.45, ImageSize -> 760,
        FrameLabel -> {None, "wall seconds (lower = faster)"},
        Frame -> {{True, False}, {True, False}},
        PlotLabel -> "Both banked, full Wolfram wall (lower = faster)"]]
```
<!-- => grouped bar chart: thvm 0.01-0.05s per theorem vs FEQ ~0.2s (its fixed startup floor) -->

`thvm`'s `Method -> "Waldmeister"` finishes each of these in 10-50 ms against FEQ's roughly fixed 0.2 s startup floor. One class needs a suboption to stay in that range: the frustrated-vacuum-implication (FVI) gated goals - `ExcludedMiddle`, `Noncontradiction`, `McCuneAxioms/EqualityOfInverses` - run ~25 s on `ExcludedMiddle` under the plain preset, because their proof needs the FVI emission rule that the generic suboption `"FreeVarInstance" -> True` turns on; with `Method -> {"Waldmeister", "FreeVarInstance" -> True}` they drop back into the sub-second range. They are off the chart above because they need that suboption, not the plain preset.

### What the external CLI numbers look like

The banked `wmcli` critical-pair counts and walls for a spread of theorems, inlined here so the cell touches no files:

```wl
Grid[{{"theory / theorem", "wmcli CPs", "wmcli wall (s)"},
      {"AbelianGroupAxioms / InverseOfInverse", 26, 0.000},
      {"BooleanAxioms / DoubleNegation", 222, 0.001},
      {"BooleanAxioms / AndIdempotence", 94, 0.000},
      {"WolframAxioms / Commutativity", 738928, 2.352},
      {"WolframAxioms / DoubleNegation", 713503, 2.319}},
    Frame -> All, Alignment -> Left]
```
<!-- => Grid: the easy goals are sub-ms / few-hundred CPs; the two Wolfram goals
        are ~2.3-2.4 s and ~700k CPs (the Sheffer tier) -->

The Sheffer-tier rows (the two `WolframAxioms` entries) explain the safety posture: those are ~700,000-critical-pair saturations at ~2.3 s of `wmcli` wall, and the in-process equivalent of that queue is what spikes memory. That is exactly why this note banks every external-CLI number rather than re-deriving it.

### Capability: cracks more within a bound (stated precisely)

Within a comparable timeout, `thvm`'s in-process engine proves roughly 25 notable theorems that the recorded `wmcli` sweep did not crack within *its* bound - a median of ~0.65 s for `thvm`, though the six hardest cross-axiom implications take `thvm` 10-49 s. State this carefully: it means `thvm` **cracks more within a fixed bound**, not that Waldmeister is incapable of those theorems. Whether `wmcli` would crack them with more time, a different flag set, or a portfolio rotation is unverified here, and the comparison is against one recorded sweep, not against Waldmeister's best effort.

## The faithful path and its yardsticks

`Method -> "Waldmeister"` is the faithful path: the in-process emulation whose whole purpose is to hit byte-parity with `wmcli` and beat its wall time. The other entries here are not competing choices - they are the variant of the faithful path for one goal class and the two yardsticks that measure it.

- `Method -> "Waldmeister"` - the faithful in-process Waldmeister. Same unfailing-completion loop, same `Mix` weight, same emission order, no external dependency, `TimeConstrained` / `Abort[]` interrupt it. This is what the byte-parity and wall-time results above describe.
- `Method -> {"Waldmeister", "FreeVarInstance" -> True}` - the same faithful path with frustrated-vacuum-implication emission turned on via the generic `"FreeVarInstance"` suboption, the form needed for `ExcludedMiddle`, `Noncontradiction`, and `McCuneAxioms/EqualityOfInverses`. A suboption on the faithful path, not a different prover.
- `Method -> "WaldmeisterProcess"` - the real `wmcli` binary, the **parity-and-speed yardstick**: the source of the banked lemma traces parity is checked against and the wall times speed is measured against. Returns `Failure["ExternalNoProof", ...]` if the binary is unavailable. Resource-unsafe on Sheffer-class goals - the reason this note banks rather than runs it.
- [FindEquationalProof]() (FEQ) - the Wolfram built-in equational prover, which is itself Waldmeister-based, so comparing against it is one Waldmeister against another. The second wall-time yardstick on the easy tier.

The general Method surface is the [AtpMethods](paclet:WolframInstitute/THVMLink/tutorial/AtpMethods) tech note; the engine overview is [ATP](paclet:WolframInstitute/THVMLink/tutorial/ATP).
