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
| `spb=goal` | `--spb-prec` (CLI) | `src/atp/precedence.c::atp_vampire_precedence_g` | PORTED as the precedence boost Vampire implements (Kernel/Ordering.cpp BoostWrapper: goal symbols outrank the rest, the `sp=` scheme ranking inside each band).  Load-bearing for the `otter-lpo` lane. |
| `sp=arity` / `const_min` / `const_max` / `const_frequency` / `unary_first` / `unary_frequency` / `occurrence` | `--sym-prec=<scheme>` (CLI) | `src/atp/precedence.c::atp_vampire_precedence_g` | PORTED from sortAuxBySymbolPrecedence; tie-break is the label id (the .pr signature is alphabetized, Vampire uses first occurrence). |
| `spb=intro` | GAP | — | "Introduced" literal selection. 14 wins. |
| `lrs_first_time_check(N)` | `"LRS" -> True` (param N hardcoded) | `src/atp/_.c::thvm_atp_set_use_lrs` | LRS = Limited Resource Strategy.  N=100 ms first-check appeared in 7 wins; thvm's LRS uses a different cadence. |

## Redundancy filters

| Vampire flag | thvm option | thvm C source | Notes |
|---|---|---|---|
| `fgj=on` (forward ground joinability) | `"GroundJoin" -> True` | `src/atp/_.c::gj_less_in` | Martin-Nipkow/Twee ground-joinability test.  KBO-only currently. |
| `fd=preordered` | `--pop-rules-only` (CLI) | `src/atp/rerank.c` pop normalize | Oriented rules only at selection; the generation-time treatment is already oriented-only (WM `-kg r`). |
| `bd=preordered` / `bd=all_1` / `bd=all` | `"BackwardDemod" -> True` | `src/atp/_.c::thvm_atp_set_use_bwd_demod` | Backward demodulation = rewrite OLD rules' LHS using a NEW rule.  Vampire `bd=all` = exhaustive; thvm matches the all-variant. |
| `bd=preordered` (strict) | `--bd-preordered` (CLI) | `src/atp/wm_drain.c::thvm_atp_interreduce` | An unorientable new equation interreduces nothing. |
| `sa=lrs` / `sa=otter` simplification set | `--otter` (CLI) | `src/atp/otter.c` | Passive (queued) orientable clauses demodulate before selection; the batch is replayed newest-first (the LIFO-to-FIFO unprocessed order).  Vampire `dis` (DISCOUNT) fails where `lrs`/`ott` solve on RNG027-5. |
| age queue (derivation depth) | `--age-depth` (CLI) | `src/atp/cp_heap.c` FIFO dimension | AgeQueue::lessThan: depth, then weight; age pick first. |
| LRS age+weight limits | `--lrs-age` (CLI) | `src/atp/cp_heap.c::atp_lrs_recompute_horizon` | exceedsAllLimits: prune only CPs beyond both reaches. |
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
3. ~~**`fd=preordered` + `bd=preordered`**~~ — PORTED as
   `--pop-rules-only` + `--bd-preordered`: `preordered` means an
   unorientable equation never demodulates (forward or backward), not a
   performance shortcut.  On RNG027-5 the default WM interreduction let
   commutativity retire `-X+X -> 0` and orphan its children.  The
   decisive companion is the Otter simplification set (`--otter`), see
   the table above.
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

## Prover9 (LADR-2026-6A) ports

Prover9-2026 (CASC-J13) is the only prover that solves the whole RNG
Moufang cluster (RNG027-5/7/8/9/10, 028-5/7/8, 029-5/6/7) -- 4.5-10s in
`-casc` mode on the dissection pod, every forced portfolio strategy that
solves RNG027-5 and RNG029-5 being LPO with age-heavy selection and the
nest/depth penalties.  Its route differs from Vampire's: the LADR symbol
order ranks the defined operators on top, so the associator/commutator
definitions unfold, and the AC arrangement is normalized by
lex-dependent demodulation.

| Prover9 / LADR | thvm option | thvm C source | Notes |
|---|---|---|---|
| symbol order (`lex_compare_arity_0213`, fewer occurrences greater, `inverse_order`) | `--sym-prec=ladr` | `src/atp/precedence.c` | The `otter-ladr` lane: RNG130-1 in 0.1s.  The multi-order trial (`multi_order_trial`: frequency / reverse frequency / arity / goal-directed, most orientable equations wins) is a GAP. |
| `nest_penalty` / `depth_penalty` (`ladr/weight.c`) | `--nest-penalty` / `--depth-penalty` | `src/atp/cp_weight.c` | Every CP, including the FT-treated route (whose Terms are zero sentinels; its NF spans are scored).  `otter-lpo-cmin` carries 2/2. |
| `max_weight` (default 100, penalized) | `--max-weight=N-1` | `src/atp/cp_heap.c` | With the penalties on, the MaxWeight cap bounds symbol count + penalty (LADR also counts the `=` symbol). |
| `lex_dep_demod` / `_lim` (11) / `_sane` | `--lex-dep-demod=N` | `src/atp/ft_norm.c`, `src/atp/ordered_rewrite.c` | An unorientable equation with equal side sizes and n_l + n_r + 1 <= N demodulates in every normalization, each step gated by the run's ordering (the FT leg compares under LPO). |
| `age_part` / `false_part` / `true_part` / `weight_part` | `--awr=a:w` | `src/atp/cp_heap.c` | Without interpretations `false` = negative clauses (the goal family), `true` = positive ones; the goal is no clause here, so A3 F4 T4 W2 is about `--awr=2`. |
| Otter loop (kept sos clauses demodulate) | `--otter` | `src/atp/otter.c` | The same simplification set as Vampire `sa=otter`. |
| paramodulation into the negated goal (`false` picks) | GAP | -- | Prover9 selects goal variants; thvm only normalizes the goal. |

Measured on the cluster (--proof singles, 240s): the ported LADR route
(order + penalties + `--max-weight=99` + `--lex-dep-demod=11`) proves
none of the Moufang members -- about 1800 selections with ~450 rules
where Prover9 needs 170-320 given clauses; without `max_weight` the
unfolded terms reach 1000 symbols and the LPO compares explode.  Open
divergence: align the kept-rule stream against Prover9's given clauses,
content-canonically as for the Vampire alignment, before further ports.

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
