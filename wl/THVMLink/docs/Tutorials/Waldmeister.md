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
        "IntakeOrder" -> True, "MixmostNF" -> True,
        "CPSide" -> True, "FlatSubsume" -> True, "FormationFifo" -> True,
        "EsetDistdir" -> True, ...|> -->

Three of those keys - `"EmissionOrder"`, `"IntakeOrder"`, `"MixmostNF"` - and the CP-selection-faithful stack (`"CPSide"` / `"FlatSubsume"` / `"FormationFifo"` / `"EsetDistdir"`, Family 2 below) are what make the preset *Waldmeister* rather than just *good*. **`Method -> "Waldmeister"` enables that faithful stack by default**, so the preset is WM-selection-faithful out of the box and `TAtpDescribeMethod["Waldmeister"]` returns the resolved config. They are described in the next section. The list form `{"Waldmeister", subopt -> ...}` overrides any single default, e.g. `Method -> {"Waldmeister", "CriticalPairWeight" -> "Gt"}` swaps the queue heuristic while keeping the rest.

## The Waldmeister option knobs

The faithfulness of `thvm`'s emulation comes from porting individual Waldmeister selection-order and intake rules. The knobs below are **generic, prover-agnostic toggles** (their names carry no prover prefix); the `"Waldmeister"` preset is simply the configuration that turns the relevant ones on, the same way a future Vampire or E preset could enable the same toggles to match *its* emission order. Their current calibration targets Waldmeister, which is why the descriptions cite Waldmeister source. They split into **two families**.

### Family 1 - the parity machinery (ON in the preset)

These three knobs are `True` in `"Waldmeister"` (and the `"Waldmeister"*` variants). They reproduce Waldmeister's *order of operations* rather than just its result, which is what lets the engine match Waldmeister's lemma sequence. Each is also a standalone suboption you can toggle on any `"Completion"` config.

| Knob               | Defaults to | What it ports |
|--------------------|-------------|---------------|
| `"EmissionOrder"`| ON in preset | Critical-pair emission order: each new fact's critical-pair batch is sorted so equal-weight CPs receive their first-in-first-out (FIFO) ages in Waldmeister's emission order. This is the selection-sequence identity lever. |
| `"IntakeOrder"`  | ON in preset | Loader-level axiom intake: Waldmeister's `SpezNormierung` (special normalisation) canonical sort of the initial axiom set, plus the initial = ultimate `MIN_INT`/FIFO stamp, so the axioms pop first in Waldmeister's sorted order. |
| `"MixmostNF"`    | ON in preset | Normal-form strategy: Waldmeister's `-nf mixmost` default - a local fixpoint at the reduced position plus ancestor ascent - together with the `Regelbaum` (rule tree) within-position retrieval order. This is the generation-time join-verdict identity lever. |

### Family 2 - the side-geometry and subsumption knobs (ON in the preset)

Critical-pair FORMATION ORDER itself needs no alignment knobs: under `"EmissionOrder"` the engine's single-walk former emits each new fact's critical pairs natively in Waldmeister's `U1_KPsBildenZuFaktum` order - the Vater/toplevel sweep position-major over the new fact's face (rule-tree partners before equation-tree partners per position, discrimination-tree leaf-arrival order within each), then the Mutter/proper-subterm sweep, with the self-overlaps interleaved by position - so `cp_seq` equals Waldmeister's FIFO age `w2 = ++CPNr` **by construction** (Waldmeister stamps it on each surviving critical pair at insertion, NewClassification.c `C_Classify`; the joinable drop precedes the stamp on both sides).  A historical family of eleven per-batch re-key corrections (`CommDefer` / `CommReage` / `CommDropDup` / `LeafTiebreak` / `RevfaceGroup` / `PosGroup` / `CubeArrival` / `MeredDmgu` / `CommDropDupClassGate` / `CorankOwnArr` / `LeafTiebreakFacegate`) approximated that order after the fact; they were deleted once the native walk was proven full-stream byte-identical to the corrected batch former.

What remains WL-exposed is the side-geometry / subsumption family. **`Method -> "Waldmeister"` enables it by default**; on a bare `"Completion"` config each defaults OFF.

| Knob               | What it ports |
|--------------------|---------------|
| `"CPSide"`       | Critical-pair formation side-geometry swap: store each derived unorientable equation with Waldmeister's `KPLinks = sigma(r_parent)` (the overlapped rule's right-hand side) as the stored left-hand side, parent-overlap-aware. |
| `"FlatSubsume"`  | Flatterm-faithful E-set subsumption matcher (`MO_TermpaarSubsummiertZweites`): Waldmeister's binding-slot vs variable-symbol cross that removes axiom 2 on commutativity-add. |
| `"FormationFifo"`| The faithful-stack master knob. CP formation already emits natively in Waldmeister's order, so this arms the shared order refinements *outside* formation: the IR-victim drain-order keys (within-leaf chain order, `BK_Regeln -> TP_Nachf` head-first, and the GMInterred reducible-face order, `BK_ReferenzDurchlauf`; not WL-exposed individually) and the distinguished-direction E-set subsumption (`"EsetDistdir"`, below). The CPNr lifecycle -- re-classification preserving the age (`C_ReClassify`) and re-derivation re-stamping it (`KPV_IROpferBehandeln`) -- is likewise ported. |

One further knob is kept for diagnosis only and is *excluded* from any faithful set: `"CommSubsume"` (a commutativity-aware subsumption widening that drops the right equation but forks the soa selection prefix and explodes commutative-ring baselines).

Distinct from the emission-order machinery, `"EsetDistdir"` is a subsumption-faithfulness knob -- it changes which equations the E-set *retains* (the ones Waldmeister keeps) rather than how CPs are ranked. It ports Waldmeister's distinguished-direction E-set subsumption (`Interreduktion.c:261`): test each old equation only in its distinguished (stored) orientation, dropping the two subject-swapped match attempts of the general 4-way flat subsumer. Auto-on under `"FormationFifo"`; `Automatic` (the default) leaves it at FormationFifo's value.

```wl
#| eval: false
TFindProof["ShefferAxiomsOrAssociativity",
    Method -> "Waldmeister",
    TimeConstraint -> 60]
```

Pinned `eval: false`: the soa proxy is a single-operator Sheffer-class saturation whose queue is unsafe to run during a shared-kernel doc build. The alignment is exercised by the engine's own test bench, not here.

## Byte parity for lemma generation

A lemma, in completion, is a selected critical pair. So "does `thvm` derive the same lemmas as Waldmeister, in the same order" reduces to "do the two engines select the same critical pairs in the same sequence". On the proxy used to develop Family 1 + Family 2 above - `"ShefferAxiomsOrAssociativity"` - they do, and now over the *entire* proof: under the preset the selection sequences are byte-identical through first-divergence index 2808 -- the soa proof completes (Waldmeister saturates at pick 2807), with the per-CP weights byte-exact throughout. That is byte-level reproduction of Waldmeister's complete lemma stream for the soa proof. The same configuration carries MeredithAxioms `OrAssociativity` deep into the cross-axiom prefix (the `wmcli -a4` reference runs ~16307 picks), the deepest cross-axiom prefix measured.

This is not a one-off. A recorded alignment sweep compares `thvm`'s selection sequence against the banked `wmcli` trace across the notable-theorem corpus, recording per theorem how many selections each engine makes, the identical prefix length, and whether the sequences are fully identical. Tallying its full-identity column (the banked verdicts inlined here so the cell stays self-contained):

```wl
Counts[Join[
    ConstantArray["Y", 73],   (* fully byte-identical selection sequences *)
    ConstantArray["N", 4],    (* deep cross-axiom: SKIToBCKW, Meredith AndAssoc *)
    ConstantArray["-", 5]]]   (* skipped: wmcli reference trace itself unsafe *)
```
<!-- => <|"Y" -> 73, "N" -> 4, "-" -> 5|> -->

73 of the compared theorems are **fully byte-identical** selection sequences ("Y") - the parity target met outright on the bulk of the corpus; the 5 dashes are entries skipped because acquiring the `wmcli` reference trace is itself resource-unsafe (the Sheffer / Wolfram tier). The 4 not-yet-identical "N" rows are the deepest cross-axiom implications - `CombinatorAxioms/SKIToBCKW` (first-divergence ~1868) and the Meredith cross-axiom family, where `MeredithAxioms/OrAssociativity` reaches deep into the ~16307-pick `wmcli -a4` reference - where the prefix is thousands of selections long and the weights stay exact, but the engines part deep in a combinatorial-explosion phase. These are the open work toward full-corpus parity: on the Meredith cross-axiom goals the divergence is a measured keep-multiplicity gap in the CP-formation order (`thvm` keeps fewer copies of the key term across one drought epoch), not a missing critical pair, and reproducing Waldmeister's exact keep-order there is the remaining lever.

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
