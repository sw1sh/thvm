# Vampire → thvm Port Map

What Vampire's option flags resolve to, which ones thvm currently ports,
and where each lives in the source.  Cross-reference for
`Method -> "VampireUEQ"` / `"VampirePortfolio"` / `"VampirePortfolioCompact"`
+ for any future per-theorem tuning against a captured Vampire winning
strategy.

Vampire's flag surface is huge.  This doc only catalogues flags that
have *appeared in a Vampire winning strategy on our NotableTheorems
benchmark* (`tools/baselines/vampire_raw/*.out`) plus the
unit-equational-relevant family.  Saturation strategies that don't
apply to unit-equational reasoning (SAT-driven AVATAR with theory
reasoning, instantiation calculus, etc.) are intentionally out of scope.

Conventions:
* "PORTED" = a user-facing Method suboption in
  `wl/THVMLink/Kernel/ATP/ATP.wl` that round-trips through the C
  engine.
* "INTERNAL" = the C engine has the mechanism but it's reachable only
  via env vars or a sibling preset.
* "GAP" = the trick is not in thvm; the corresponding row is on the
  roadmap.

## Reduction ordering

| Vampire flag | thvm option | thvm C source | Notes |
|---|---|---|---|
| `to=kbo` | `"Ordering" -> "KBO"` | `src/kbo/_.c` | Default in `Waldmeister` preset. |
| `to=lpo` | `"Ordering" -> "LPO"` | `src/lpo/_.c` | Default in `VampireUEQ` preset. |
| `sp=arity` | `"AutoPrecedence" -> True` | `src/atp/precedence.c::atp_auto_precedence` | Fuchs arity ladder — same direction as Vampire's. |
| `sp=occurrence` | `"AutoPrecedence" -> "Occurrence"` | `src/atp/precedence.c::atp_occurrence_precedence` | Vampire `sp=occurrence` / E `-G InvFreqRank`. |
| `sp=reverse_arity` | GAP | — | Inverted arity ladder.  Worth porting — appeared 10× in winning configs. |
| `sp=reverse_frequency` | `"AutoPrecedence" -> "ReverseFrequency"` | `src/atp/precedence.c::atp_reverse_frequency_precedence` | PORTED (commit 3ba3f8f4).  Probe on 6 stuck NotableTheorems @ TC=60s with LPO+ReverseFrequency+GroundJoin+BackwardDemod+RHSInterreduce+UnfailingCP cracked 0 -- necessary but not sufficient; needs combination with the still-unported `spb`/`nwc`/`kws` knobs. |
| (explicit) | `"Precedence" -> {...}` | `wl/THVMLink/CSource/thvmlink_atp.c` args[17] → `LpoConfig.precedence` | Symbol-name list, highest-to-lowest.  Mirrors Waldmeister's `p > q > nand`. |
| (skolems-highest) | `"SkolemHighest" -> True` | same | Ranks goal's ground/skolemized constants above operators. |

## CP selection (heuristic + selection-function)

| Vampire flag | thvm option | thvm C source | Notes |
|---|---|---|---|
| (CASC heuristic) | `"CriticalPairWeight" -> {Add\|Max\|Ord\|Gt\|Mix\|Mix2\|Unif\|Goal}` | `src/atp/_.c` (`cp_weight_mode`) | Engine default `Gt`; Vampire's analog spans `Mix2`+`Unif`+`Goal` depending on the strategy slot. |
| (random pick) | `"RandomRatio" -> n`, `"RandomSeed" -> u64` | `src/atp/_.c::thvm_atp_set_random_modulo` / `_random_seed` | Every nth CP selection is a uniform-random queue pick — Vampire's `random_seed=...` cracking ingredient. |
| `nwc=N` (non-unit weighting) | GAP | — | Multiplies non-unit clause weight by N.  Bias toward unit-clause picks.  HIGH-VALUE PORT TARGET. |
| `kws=inv_precedence` | `"KboWeightScheme" -> "InvPrecedence"` | `wl/THVMLink/CSource/thvmlink_atp.c` args[29] | PORTED.  Derives per-symbol KBO weight from AutoPrecedence (weight = `max_prec - prec(sym) + 1`) when SymbolWeights is not provided.  Probe @ TC=60s combining `ReverseFrequency` + `InvPrecedence` + `GroundJoin` + `BackwardDemod` + `RHSInterreduce` + `UnfailingCP` cracked 0/6 of the residual NotableTheorems; needs `nwc=N` + `spb=*` companions for the full Vampire shape. |
| `kmz=on` | GAP | — | KBO "modular zero" — variant weight handling for zero-coefficient slots. |
| `spb=goal_then_units` | GAP | — | LITERAL selection priority: goal literals first, then units, then others.  Appeared 38× in wins — second-most common spb after `intro`/`goal`. |
| `spb=goal` | GAP | — | Goal-literal selection.  14 wins. |
| `spb=intro` | GAP | — | "Introduced" literal selection. 14 wins. |
| `lrs_first_time_check(N)` | `"LRS" -> True` (param N hardcoded) | `src/atp/_.c::thvm_atp_set_use_lrs` | LRS = Limited Resource Strategy.  N=100 ms first-check appeared in 7 wins; thvm's LRS uses a different cadence. |

## Redundancy filters

| Vampire flag | thvm option | thvm C source | Notes |
|---|---|---|---|
| `fgj=on` (forward ground joinability) | `"GroundJoin" -> True` | `src/atp/_.c::gj_less_in` | Martin-Nipkow/Twee ground-joinability test.  KBO-only currently. |
| `fd=preordered` | `"RHSInterreduce" -> True` (engine-level) | `src/atp/_.c::thvm_atp_set_use_rhs_interreduce` | Forward demodulation = simplification of newly-generated clauses against existing rules. |
| `bd=preordered` / `bd=all_1` / `bd=all` | `"BackwardDemod" -> True` | `src/atp/_.c::thvm_atp_set_use_bwd_demod` | Backward demodulation = rewrite OLD rules' LHS using a NEW rule.  Vampire `bd=all` = exhaustive; thvm matches the all-variant. |
| (forward subsumption) | `"ForwardSubsume" -> True` | `src/atp/_.c::thvm_atp_set_use_fwd_subsume` | Vampire `--forward_subsumption` analog. |
| `bs=on` / `bs=unit_only` | `"BackwardSubsume" -> True` | `src/atp/_.c::thvm_atp_set_use_bwd_subsume` | Vampire `bs=unit_only` analog: drop existing rules subsumed by a new one. |
| `fsr=off` | (default OFF) | — | Forward subsumption RESOLUTION disabled.  thvm doesn't have a separate `fsr` knob — `fwd_subsume` already covers the conservative case.  GAP for the resolution-on variant. |
| `fde=none` / `fde=unused` | (always ON for proved clauses) | `src/atp/_.c` rewrite path | Vampire's forward demod-equation variants.  thvm doesn't expose `fde` enable/disable separately. |
| `s2a=on` | GAP | — | "Split to arguments" — clause splitting heuristic.  AVATAR-class, low priority for UEQ. |
| `bsr=on` (backward subsume resolution) | GAP | — | Beyond plain backward-subsume.  Appears in some Vampire portfolio slots. |
| (CP-set interreduce) | `"CPSetInterreduce" -> True` | `src/atp/_.c` Waldmeister periodic pass | Waldmeister-style, NOT a Vampire flag — opt-in everywhere, mirroring the WM CLI `-ki` default (off). |

## Axiom-relevance filtering

| Vampire flag | thvm option | thvm C source | Notes |
|---|---|---|---|
| `sine_tolerance=t`, `sine_depth=d`, `sine_generality_threshold=g` | `"AxiomRelevance" -> {"SInE", "SineTolerance" -> t, ...}` | `src/atp/_.c` (SInE pass) | Hoder-Voronkov D-relation + bounded BFS from conjecture symbols.  thvm defaults `t=3`, `d=2`, `g=8` (mirrors Vampire `--sine_tolerance/--sine_depth/--sine_generality_threshold`). |
| `slsql=off` | (no analog) | — | "SAT-solver level sub-query language" — Vampire AVATAR config. |

## Inference family (out of scope for UEQ, listed for completeness)

| Vampire flag | thvm | Notes |
|---|---|---|
| `av=off` (AVATAR off) | (n/a — thvm has no SAT-driven splitting) | UEQ-irrelevant; thvm proves Robbins by other means (or doesn't). |
| `ins=N` (instantiation calculus) | GAP | UEQ-irrelevant. |
| `sas=fmb` (finite model builder SAT) | GAP | Saturation strategy switch. |

## Per-strategy mapping

The `tools/baselines/vampire_raw/<theory>__<thm>.out` files list
the winning strategy verbatim.  Three patterns dominate:

1. **`lrs+10_<TC>_to=lpo:sp=arity:spb=goal_then_units:nwc=1:random_seed=...:fgj=on:bd=all_1`**
   * Appears as the McCune `EqualityOfInverses` cracker.
   * Maps to thvm: `"VampireRandom"` preset (LPO + AutoPrecedence + SR=10
     + UnfailingCP + GroundJoin + BackwardDemod + RHSInterreduce +
     RandomRatio 32 + RandomSeed=3681690318 + LRS).
   * **Missing**: `spb=goal_then_units` (literal selection) +
     `nwc=1` (non-unit weight coefficient).
   * Empirical: thvm's `VampireRandom` does NOT crack McCune at 300s
     C-bench wall.  The two missing knobs are the suspected gap.

2. **`fde=unused:to=lpo:sp=reverse_frequency:fgj=on:bd=preordered:fsr=off:rawr=on_7`**
   * Most common winning shape (multiple WolframAxioms entailments).
   * Maps partially: thvm has `to=lpo`, `fgj=on`, `bd=preordered`.
   * **Missing**: `sp=reverse_frequency` (122 wins!) + `rawr=on`
     (random axiom-walk randomization).

3. **`nwc=3:kmz=on:random_seed=...:kws=inv_precedence:bd=preordered_7`**
   * Several `MeredithAxioms/Implies-*` slots.
   * Maps partially: `bd=preordered` + random seed.
   * **Missing**: `kws=inv_precedence` (KBO weight = inverse precedence)
     + `kmz=on` + `nwc=3` (non-unit weight 3x).

## High-value port targets (concrete next steps)

Ordered by win-frequency in `vampire_raw/`:

1. ~~**`sp=reverse_frequency` precedence** (122 wins)~~ — PORTED
   (commit 3ba3f8f4).  Solo probe on 6 hard cases cracked 0 / 6;
   needs combination with the remaining items below to move the bench.
2. **`spb=goal_then_units` literal selection** (38 wins) — at CP
   selection time, pick a clause that contains a goal literal first,
   then units, then others.  Lives next to `atp_select_cp` in
   `src/atp/_.c`.
3. **`fd=preordered` + `bd=preordered`** — currently thvm's rewrite
   path is "all preordered" with no toggle.  Vampire's `preordered`
   variant only applies a rule when the orientation IS preordered
   (skips KBO/LPO check at rewrite time, trusts the stored orient
   flag).  Mostly a perf knob.
4. **`nwc=N` non-unit weight coefficient** — multiply non-unit clause
   weight by N in `cp_weight_mode` evaluation.  Bias toward unit-clause
   selection.  Trivial 5-LOC addition.
5. ~~**`kws=inv_precedence` KBO weight scheme**~~ — PORTED as
   `"KboWeightScheme" -> "InvPrecedence"`.  Derives weight =
   `max_prec - prec(sym) + 1` when no explicit SymbolWeights set.
   Probe combining with `ReverseFrequency` cracked 0/6 stuck cases @
   TC=60s — needs `nwc=N` companion (Vampire's winning shape uses
   `nwc=3:kws=inv_precedence` together).
6. **`fsr=off` forward subsumption resolution toggle** — split the
   existing `"ForwardSubsume"` knob into `"ForwardSubsume" -> {True,
   False}` (current behavior) plus `"ForwardSubsumeResolution" ->
   {True, False}` for the resolution variant.

Each of these is a localized addition in `src/atp/` or
`src/atp/precedence.c` + a small WL surface change in ATP.wl.

## What is NOT a "Vampire port" but is sometimes confused with one

* **CP-set interreduce** (`CPSetInterreduce`) — Waldmeister
  `KPV_KPMengeInterreduzieren`, not a Vampire concept.
* **GoalDirected MNF front** — closer to the Bachmair-Plaisted /
  Lankford "mandatory normal form" front search than to any Vampire
  inference rule.
* **UnfailingCP** — completion-completeness requirement (Bachmair-
  Dershowitz-Plaisted 1989); both Vampire and Waldmeister implement,
  but it's not "a Vampire trick."

## Documentation discipline

When adding a new Vampire-flag port:
1. Add the row to the table above with C source + WL option.
2. Reference the original Vampire paper / `--help` text if non-obvious.
3. If the flag has appeared in `vampire_raw/*.out` winning configs,
   bump its priority in the "high-value targets" list.

Empirically benchmark each port on `tools/baselines/` before
committing — a new option that doesn't move the PROVED count or open
a previously-stuck case should NOT land just for completeness.
