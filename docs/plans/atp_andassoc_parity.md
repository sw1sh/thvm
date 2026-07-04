# andassoc <-> Waldmeister trajectory-parity dissection (2026-07-04)

Scoped follow-up plan. This document pins the *matched* goal/config, the
Waldmeister reference trajectory, thvm's trajectory, the first divergence
(config + engine), and the scoped arc for closing the residual engine
divergence. Written at thvm HEAD `25240924`; no code landed by this
dissection (see "Why no fix landed").

## TL;DR

thvm is **already faithful** to Waldmeister's KBO trajectory for **2922
consecutive selections** on the `andassoc` problem (weights byte-exact,
zero content-delta), *once the FIFO/SelectionRatio config is matched to
the reference mode*. The headline "thvm 436 rules vs WM 1673 rules"
comparison in the mission brief was **config-mismatched** on TWO axes:

1. **Ordering.** WM's canonical `andassoc.pr` is **LPO** (1673 rules);
   thvm's default proof of `goal_andassoc` used **KBO** (436 rules).
   Different reduction orderings => structurally different completions.
2. **FIFO / SelectionRatio.** thvm's `Waldmeister` preset enables the
   age/weight FIFO interleave (SR=51, tuned to `wmcli -auto`); WM's
   *plain* `andassoc.pr` run (the external tool named in the brief) uses
   **no** FIFO interleave.

With both axes matched (thvm `.pr`-mode KBO + `THVM_ATP_FIFO_THRESHOLD=0`
vs `wmcli andassoc_kbo.pr` plain), the selection streams agree for 2922
picks. The residual first *engine* divergence is at pick **2923**, deep
in the post-avalanche near-confluent basis: an intra-equal-weight-band
selection-**order** flip (WM w2=CPNr formation-order tiebreak vs thvm
cp_seq). The selected CP *sets* are identical (`content-delta wm-only=0`);
only the micro-order of two sibling overlaps differs. This is the known
deep CP-formation-order arc (same family as the Meredith `soa` work),
NOT a quick config knob (`FORMATION_FIFO` was measured a no-op here).

## 1. Matched goal-form / config (RESOLVED)

The mission brief's premise "WM's andassoc.pr uses universal p,q,r" is
**incorrect**. In `andassoc.pr`:

- `SIGNATURE  nand: ANY ANY -> ANY ; p: -> ANY ; q: -> ANY ; r: -> ANY`
  => `p,q,r` are **0-arity constants**, and appear in the ORDERING
  precedence chain (`p > q > r > nand`).
- `VARIABLES  a,b,c` are the AXIOM variables (used only in the equation).
- `EQUATIONS  nand(nand(nand(a,b),c),nand(a,nand(nand(a,c),a))) = c`
- `CONCLUSION` is a **rigid ground instance** of AndAssociativity in the
  constants p,q,r.

thvm match: **`goal_andassoc`** (rigid; `tests/test_atp_wolfram_bench.c:173`,
`konst(L_P/Q/R)`), NOT `goal_andassocu` (universal `fv`, :187). Verified:
thvm `and2(x,y)=nand(nand(x,y),nand(x,y))` expands the conclusion
term-for-term to WM's; thvm `axiom_inst` (:66) is byte-equal to WM's
axiom.

WM ships **two** ordering variants (both precedence `p>q>r>nand`):

| file | ordering | WM rules / eq / CP | wall | RSS |
|---|---|---|---|---|
| `andassoc.pr` | **LPO** | 1673 / 102 / 4,574,980 | 9.94s | 822MB |
| `andassoc_kbo.pr` | **KBO** (wts 1) | 1601 / 74 / 3,750,287 | 6.10s | 788MB |
| `-auto andassoc.pr` | WM-chosen | 1613 / 56 / 3,648,262 | 6.52s | 528MB |

thvm side (`THVM_ATP_WALDMEISTER=1`):

| config | thvm PROVED steps / rules / CP |
|---|---|
| built-in `andassoc` (KBO, FIFO on) | 87124 / 436 / 1,320,749 |
| `.pr andassoc_kbo.pr` (KBO, FIFO on) | 278807 / 753 / 2,642,990 |
| `.pr andassoc_kbo.pr` (KBO, FIFO **off**) | 274055 / 751 / 2,476,857 |
| built-in `andassoc` LPO (SKOLEMS_HIGH) | RUNNING at 90s (no crack) |

The `.pr`-mode run is the correct alignment vehicle: it reads WM's exact
file (same symbol names `nand/p/q/r`, same reserved FVI constants
`const2`/min-const that WM introduces at `SO_Extrakonstante`), emits
`PRSYM`, and uses the label numbering `tools/wm_align_sweep/align.py`
expects. `.pr` bench-loader rejects LPO (`test_atp_wolfram_bench.c:441`),
so the KBO variant is the matched apples-to-apples reference.

Note (not a bug): thvm `.pr` mode on `WolframAxioms__OrAssociativity.pr`
and `...__AndAssociativity.pr` yield IDENTICAL totals (278807/753/2642990)
because And/Or-associativity are nand-duals over the same axiom and both
close at the same completion depth; `DoubleNegation.pr` closes earlier
(2848/254/768876), confirming goal-direction is live.

## 2. First divergence #1 -- pick 117 -- FIFO/SelectionRatio (CONFIG)

thvm `.pr` KBO **FIFO ON** vs WM plain: `prefix=116, firstdiv=117`,
weights EXACT on the prefix. thvm picks 1..116 are all weight-root
(`j=0`); pick 117 is thvm's FIRST age/FIFO pick (`j=191 seq=4 pri=1109`)
while WM continues a weight-929 band.

- thvm mechanism: `Waldmeister` preset calls `thvm_atp_set_selection_ratio`
  (`test_atp_wolfram_bench.c:859`) with SR=51, derived from `wmcli -auto`'s
  `-pq interleave=1.50` (:849-852). `THVM_ATP_FIFO_THRESHOLD=0` restores
  the plain-wmcli no-FIFO behavior (:852).
- WM mechanism: `sources/INF/KPVerwaltung.c:857`
  `return GetKPV_AnzAktivierterRE() % moduloCP < thresholdCP;` is the
  age-vs-heuristic gate. `-auto` sets `interleave=1.50` =>
  `moduloCP=51, thresholdCP=1` (fires every 51st activated rule). **Plain**
  wmcli leaves `thresholdCP=0` => the gate never fires => NO FIFO picks.
  (`sources/WASIC/ParseInterleave.c`, `sources/ANA/YFiles.c:121`.)

Verified: thvm `.pr` KBO with `THVM_ATP_FIFO_THRESHOLD=0` reproduces
WM-plain's full weight sequence (including its interreduction weight-drops
at picks 4/15/57/61/85/113) and extends the aligned prefix from 116 to
**2922**.

This is a **config** divergence, not an engine bug: thvm's preset FIFO-on
is *correct* for the `-auto` (FEQ) reference the paclet actually targets;
the mismatch is purely that the mission compared the preset against WM's
*plain* `.pr` run. Resolution: match FIFO mode to the reference (FIFO-off
for plain-`.pr`; FIFO-on for `-auto`). No preset change.

## 3. First divergence #2 -- pick 2923 -- CP-formation order (ENGINE, DEEP)

thvm `.pr` KBO **FIFO OFF** vs WM plain: `prefix=2922, firstdiv=2923`,
weights EXACT, `content-delta wm-only=0` (WM selects nothing thvm doesn't).
This is deep post-avalanche (rules collapsed to ~19-21; both engines
grinding the pri=109 idempotence mini-lemmas). At the fork the CP *sets*
match but the intra-band ORDER flips:

```
  2923 WM     x1 # nand(nand(x2,x1),nand(nand(x3,x2),x1))     <- WM takes nand(x3,x2) first
  2923 thvm   x1 # nand(nand(x2,x1),nand(nand(x2,x2),x1))     <- thvm takes nand(x2,x2) first (seq 733343)
  2926 thvm   x1 # nand(nand(x2,x1),nand(nand(x3,x2),x1))     <- thvm's nand(x3,x2) is 3 picks later (seq 733347)
```

Mechanism: within an equal-w1 band, the secondary key is the FIFO/age w2.
WM sets `wtPtr->w2 = ++CPNr` at classification (`sources/CLAS/
NewClassification.c:325`), i.e. w2 = **CP-formation order**. thvm's
secondary key is `cp_seq` (formation order in thvm's walk-former). The two
sibling overlaps `nand(x2,x2)` and `nand(x3,x2)` are formed in the OPPOSITE
relative order by thvm's walk-former vs WM's `U1_KPsBildenZuRegel`
(`sources/INF/Unifikation1.c`) DFS over a single rule's overlap set. So the
w2/seq tiebreak selects them in flipped order.

This is the same class as the Meredith `soa` CP-formation-order arc
(project_atp_wm_faithful_reference: `use_mered_dmgu`, `corank_own_arr`,
`eset_distdir`, ... a multi-fix session). `THVM_ATP_FORMATION_FIFO=1` was
measured a **no-op** here (identical prefix 2922), so the existing
formation-FIFO stack does not target this overlap-emit order.

## 4. Scoped arc to close pick 2923 (future dedicated round)

Goal: make thvm's per-rule overlap-emit order match WM's
`U1_KPsBildenZuRegel` so cp_seq == CPNr within an equal-w1 band, extending
the aligned prefix past 2923. Steps:

1. **Instrument the emit order.** For the single rule added just before
   pick 2923 (thvm seq window ~733340-733350), dump thvm's walk-former
   overlap-emit sequence AND WM's `-a4` `critical pair NNNN built with
   parents R and S` sequence for the same rule. Diff the sibling order.
   (Both parents/positions are in the traces already banked.)
2. **Locate the DFS-order knob.** Compare thvm's walk-former traversal
   (`src/atp/_.c` walk/batch formers, `vcp_*`) subterm-position order
   against WM `U1_KPsBildenZuRegel` Step-2/3 (subterm walk via
   `TO_TermAnStelle`, both faces). The likely axis: position enumeration
   order (pre-order vs the specific WM order) or the self- vs cross-overlap
   interleave.
3. **Port under a gate**, byte-identity discipline: soa/CPSEL-stream
   byte-identical on OA+DN (the existing pins), default-off, auto-on only
   under the faithful stack. Advance the andassoc firstdiv past 2923 and
   re-measure how far the new prefix reaches (candidate: to the next
   genuine formation-multiplicity fork).
4. **Do NOT chase to a full crack via order alone.** Even byte-perfect
   selection order leaves the post-avalanche *queue-economics* gap
   (project_atp_wm_sheffer_lpo 2026-06-12: 2.08M dead-parent raw CPs,
   orphan-pop chew, RSS) that binds thvm's wall on the endgame. That is a
   separate (implicit-pair storage / orphan-pop throughput) arc.

## 4b. Dedicated round (2026-07-05): pick 2923 PINNED to a jump-exit-order corner; naive port DISPROVEN; fundamental tension with OA -> SCOPED

A dedicated round instrumented thvm's walk-former emit order (CPFORM seq
trace, THVM_ATP_CP_FORM_TRACE + SEQ_LO/HI) for the pick-2923 batch AND
recovered WM's `-a4` formation ages (`critical pair N built with parents R
and S` / `added to SUE: w, age`) for the same batch. The divergence is now
pinned exactly; **no fix landed** (the faithful fix conflicts with OA -- see
below). All mission gates re-verified EXACT (no code changed).

### The pinned rule + the two overlaps

- **New fact = thvm rule 15 = WM rule 654** = `nand(nand(nand(x1,x2),x3),
  nand(x2,x3)) -> x3`. Its **Vater toplevel sweep (phase A)** at **position
  L.1.1** (subterm `nand(x1,x2)`, two fresh vars = fully general) forms the
  divergent equal-weight (w=109 raw / pri=109) band: A = `x1 #
  nand(nand(x2,x1),nand(nand(x2,x2),x1))` (inner `nand(x2,x2)`), B =
  `...nand(nand(x3,x2),x1)`, C = `...nand(nand(x2,x3),x1)`.
- Partner map (thvm slot j -> WM rule): j1->640, j2->641, j3->642, j4->643,
  **j13->652**, j14->653; the arg1=VARIABLE partners j10->649, j5->644,
  j12->651, j8->647.
- At L.1.1 both engines emit the **arg1=variable** partners (var-kids 649,
  644, 651, 647) FIRST, byte-identically. The divergence is entirely in the
  **arg1=function** partners, reached via the node's **jump exits**
  (`n->exits`, wm_order.c `wmo_dfs` else-branch):
  - **WM order:  652, 641, 643, 653, 640, 642**  (= tryFct, tryNVar, tryOVar)
  - **thvm order: 641, 643, 653, 652, 640, 642**  (= tryNVar, tryFct, tryOVar)
- **Only WM652 / thvm-j13** (`nand(nand(nand(x1,x2),x3),nand(x1,x3))`) is
  misplaced: WM emits it FIRST because its arg1 = `nand(nand(x1,x2),x3)` is
  FUNCTION-headed at arg1.arg1; thvm emits it THIRD, after the new-var-headed
  exits {641,643,653}. This flips the w2/cp_seq formation-age tiebreak ->
  the intra-band selection flip at pick 2923 (WM ages: B@733334 before
  A@733336; thvm seqs: A@733343 before B@733346).

### Why OA/DN coincide but andassoc diverges (mission hypothesis CONFIRMED)

The mission's "the two overlaps are at positions WM's DFS visits in an order
thvm's walk reverses only when the subterm shapes are X" is confirmed: X =
**one partner's arg1 is a DEEPER function-headed subterm (`nand(nand,.)`),
the others' arg1 are shallower (`nand(x,y)` / `nand(x,x)`)**, all coexisting
as jump exits at the SAME discrimination-tree node (root-nand, arg1
position). OA/DN's rule-completion histories never build a node carrying a
deeper function-headed jump-exit beside shallower variable-headed ones in
this exposing configuration; andassoc's specific completion (652, the deeper
`nand(nand(x1,x2),x3)` arg1, inserted AFTER 641/643/653/640/642 and splitting
their shared arg1-prefix leaf) creates exactly that node.

### Root + the naive port DISPROVEN

Root = the **jump-exit list order** (`n->exits`) at that node, built by
`wmo_tree_insert` (RumpfSprungeintragSetzen head-insert + BlattAufgeteilt /
`wmo_altes_blatt_polieren` splice). WM's BlattAufgeteilt places 652's deeper
function-headed exit at the head; thvm's places it after the shallower
new-var exits.

A traversal-time **discrimination-order sort of the jump exits** (Fct <
NewVar < OldVar, in `wmo_dfs`'s query-variable else-branch), gated behind an
env, was built and measured: it **REGRESSED andassoc firstdiv 2922 -> 17**
(content-delta wm-only=214, thvm-only=13). This DISPROVES the naive port:
WM's exit order is genuinely **insertion-history-dependent, NOT a pure
structural disc-order** -- forcing disc-order globally destroys the early
trajectory. The change was reverted (net-zero code; tree byte-identical).

### Fundamental tension with the OA crown jewel -> SCOPED

The faithful fix is a **surgical BlattAufgeteilt exit-placement correction
for the 652-insertion event** in `wmo_altes_blatt_polieren` /
`wmo_tree_insert`. The in-code comment (wm_order.c:921-934) records that a
head-insert exception for this exact placement was **REMOVED** because it
"mis-ordered the OrAssociativity 655/658 twin exits under -auto" -- and **OA
is the crown jewel of this track** (must stay byte-identical). So 652's
head-vs-after-model placement is the SAME `wmo_altes_blatt_polieren` decision
OA's exit order depends on, in the OPPOSITE direction: the documented "no
local discriminator / fundamental tension" of the trie-exit-placement class
([[project_atp_wm_orassoc_enclosing_jump]] DEFINITIVE WALL).

- **Candidate discriminator for a future dedicated round:** the andassoc 652
  case is a **FUNCTION-vs-VARIABLE** split branch (652's arg1.arg1 = nand vs
  641's arg1.arg1 = x), whereas OA 655/658 is a **VAR-VAR twin** (f(x0,x1) /
  f(x1,x0), both branch cells variables). The existing fresh-head guard
  already restricts to `(new_fun != old_fun)` (mixed function/variable,
  breakthrough #8 f0317c76). A BlattAufgeteilt refinement that head-places a
  function-headed split exit -- but NOT a var-var twin -- MIGHT close 652
  without perturbing OA. But it is in the highest-risk-to-OA/DN code, and
  validating it requires the FULL 66-theorem matrix sweep + OA/DN + Wolfram
  prefix (the orassoc memory: "the sweep is MANDATORY; regressions there are
  invisible to the standard gates"), which is out of scope under this
  session's safety constraints (serialize, no corpus sweeps). Deferred.

### Gates (all EXACT, no code landed)

- OA `WolframAxioms__OrAssociativity.pr 500000 280`: PROVED
  steps=278807 rules=753 cps=2642990 max_cps=2642990 (14.4s) EXACT.
- DN `WolframAxioms__DoubleNegation.pr`: PROVED 2848/254/768876 EXACT.
- non-lazy FEQ (OA.pr, THVM_ATP_LAZY_NORM=0): PROVED 263550/771/1654086 EXACT.
- bin/test_atp: 136241/136241; make && make wl green.
- andassoc baseline (KBO, FIFO off): prefix=2922 firstdiv=2923
  content-delta wm-only=0 (UNCHANGED). NEW firstdiv NOT advanced (no fix).

## 5. Why no fix landed in this dissection

- The pick-117 divergence is a config/reference-mode match, not an engine
  bug (preset FIFO-on is faithful to `-auto`/FEQ).
- The pick-2923 divergence is the deep formation-order arc; a faithful fix
  requires the multi-step walk-former overlap-order port above, gated on
  soa/CPSEL byte-identity -- a dedicated round, not a one-liner. Per the
  loop directive, it is scoped here rather than force-fixed.
- Gates confirmed intact (no code touched): `bin/test_atp` 136241/136241;
  OA.pr 278807/753/2642990; DN.pr 2848/254/768876 (all EXACT).

## 6. Reproduce

```
WM=~/.cache/thvm/wm_ref/l20_clean_build/wmcli_clean_head_57ef3dc
export DYLD_FALLBACK_FRAMEWORK_PATH="/Applications/Wolfram 15.0.app/Contents/Frameworks"
KBOPR=/Users/swish/src/wolfram/waldmeister/andassoc_kbo.pr

# WM reference (KBO), bounded key stream:
timeout 50 "$WM" -a 4 "$KBOPR" 2>&1 | grep -E 'touched\.| # ' \
  | awk '/touched\./{c++} c>3501{exit}{print}' > wm_keys.txt

# thvm matched (KBO, FIFO off) with CP-pick trace:
THVM_ATP_WALDMEISTER=1 THVM_ATP_FIFO_THRESHOLD=0 THVM_ATP_CP_PICK_TRACE=1 \
  ./bin/test_atp_wolfram_bench "$KBOPR" 500000 45 > thvm.out 2> thvm.err

python3 tools/wm_align_sweep/align.py --wm wm_keys.txt \
  --thvm-out thvm.out --thvm-trace thvm.err   # -> prefix=2922 firstdiv=2923
```

Banked artifacts: `~/.cache/thvm/wm_ref/andassoc_2026-07-04/`
(reference totals, WM key stream, thvm CPSEL prefix, alignment reports,
provenance.txt).
