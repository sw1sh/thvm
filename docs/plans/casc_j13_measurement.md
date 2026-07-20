# CASC-J13 FindProof: scores, soundness, and scouting

Measurement cycle 2026-07-19/20. All engine numbers are competition-faithful:
the concurrent schedule at `THVM_ATP_SCHED_MAX_PAR=16` (the CASC node is an
8-core / 16-thread Xeon E5-2620 v4, so `nproc = 16`), a single 240s wall, and
the correct TPTP axiom root. Competitor bars are the official CASC-30 results.

## Measured scores

### UEQ (unit equality) -- our strong division

| System | CASC-30 UEQ / 300 |
|--------|-------------------|
| **thvm FindProof (par=16)** | **265** |
| Vampire-5.0 | 263 |
| Twee-2.6.0 | 258 |
| E-3.3.0 | 222 |

We finish above both official contenders on CASC-30 UEQ. Caveat: the
measurement pod ran the 16 slices at full speed, whereas StarExec runs 16
threads on 8 physical cores (half speed), so 265 is a slight upper bound; it
sits right at the Vampire boundary either way. CASC-29 and J12 were previously
measured at par=8 (265 / 269) and undercount -- par=8-starved problems such as
BOO015-10 (CASC-29) and LAT148-1 (J12) are recovered at par=16 (4s, 45s), so
the honest cross-year numbers are higher and pending a fresh par=16 sweep.

### FOF (first-order form) -- our weak division

| System | CASC-30 FOF / 500 |
|--------|-------------------|
| CSI_Enigma | 466 |
| Vampire-5.0 | 455 |
| cvc5 | 402 |
| iProver | 367 |
| E-3.3.0 | 364 |
| Prover9 | 267 |
| **thvm FindProof (par=16)** | **~143** |
| SPASS-SCL | 119 |
| ConnectPP | 102 |

FOF is measured at ~143/500 (~29%): above the connection-calculus provers,
about half of Prover9, under a third of Vampire. thvm is an equational
completion engine translating FOF, not a native saturation prover, so it is
mid-field here while it wins UEQ. The recent work (schedule wall fix + par=16 +
FOF strategy knobs) lifted it from ~130 to ~143.

## Soundness verification (full TPTP)

Triggered by the CASC-J13 organizer's soundness report, then extended to the
whole library. Every problem with header status Satisfiable or
CounterSatisfiable was run under the competition schedule:

- Phase A (all 3388 Sat/CSA problems, 30s): 0 false proofs.
- Phase B (150 sampled, 240s): 0 false proofs.
- UEQ-satisfiable shard (260, 240s): 0 false proofs.
- Forced `--full-ac=2` over all 260 TPTP Satisfiable UEQ: 0 false proofs.

The extended sweep found and fixed three genuine unsoundness classes (all
canaried, all closed at the narrowest sound choke point):

1. `fol_paramodulate` accepted a whole-atom position (`path_len == 0`) and a
   bare-variable from-side, manufacturing variable-as-literal clauses that
   refuted the satisfiable CAT015-3. Both rejected.
2. Raw-text format sniffing used bare `strstr`, so a `%`-comment containing the
   word AXIOMS routed 46 higher-order files to the bench default -- a false
   Theorem. All format decisions now go through a comment-aware scanner; typed
   thf/tff inputs are rejected up front.
3. The eq/true/false equational encoding is faithful only where `true != false`;
   an input satisfiable in the one-element interpretation let the or-tables
   collapse the domain and refute. A budgeted one-element-model gate now gives
   up before any lane runs on such inputs.

UEQ was never exposed to any of the three (unit equalities reach none of the
paths); the FOF entry was, so the fixes protect the FOF resubmission.

## Key corrections this cycle

- **Schedule honored a fresh wall per wave, not one global wall.** Wave-2 slices
  stacked their own full wall on wave-1's; the competition hard kill truncated
  unsolved problems to NoOutput and let a `wall + grace` measurement over-count
  grace-window solves. Fixed with a single global deadline (each slice gets the
  remaining wall). This is the "one wall budget, every strategy racing"
  condition the schedule always claimed.
- **The competition core count is par=16, not par=8.** Earlier UEQ figures were
  measured at par=8 and both under- and over-counted; par=16 with the wall fix
  is the honest condition. Every problem previously called a "gap" must be
  re-checked at par=16 before being trusted -- several are par=8-starved and
  solve fine at par=16.

## Missing-knobs audit (Vampire 5307 strategies + E + Twee vs our surface)

No fundamental subsystem is missing; every gap is a bounded extension of a
mechanism we already have. Top three, all pre-wired:

1. `kws` (KBO per-symbol weight schemes) -- E's auto-ordering machinery that
   wins the open GRP661-695 cluster. The `weights[]` array is already engine-
   consumed; only the weight-generation fills were new. **Implemented** (opt-in,
   byte-identical when unset, sound; `kmz` rides free with a unary-only
   admissibility guard).
2. `s2a` / `slsq` (SInE-distance graded selection) -- the two most frequent
   options on Vampire's winning strategies; grade clause selection by distance
   from the goal. Extends our existing goal tier. Not yet built.
3. `spb=intro/non_intro` -- boost/demote preprocessing-introduced symbols;
   completes our own tgt-fold lane. Not yet built.

Confirmed dead ends for a completion core (do not chase): all AVATAR / splitting
/ resolution / CNF-transform families -- Vampire sets `av=off` on all 321 UEQ
strategies; unit equalities have nothing to split. `drc` and `fgj` are already
covered.

## Combination-grabs scouting (compose the new knobs on the residual)

Zero new solves from composing the recently-landed knobs, plus three findings:

- The apparent "buried slice" grabs (BOO015-10 via bare-raw, LAT148-1 via
  encompass) already solve at par=16 -- an artifact of the probe's low par.
- **RNG is selection/AC-blocked, not throughput-blocked** -- raising throughput
  5x and pruning CPs to 56k both fail. The old "85 CPs/pick" framing is refuted.
- **Anti-synergy**: the new throughput/size knobs (discount/awr, max-term-size)
  break the working bare-raw and encompass configs. They must never be blanket-
  composed onto completion slices.

## Residual and next levers (real engine work, both scouts agree)

- **RNG family** -- needs a sound-and-fast AC path. The unsound AC matcher found
  the short proofs (RNG021-6 in 3s); the sound one does not. Not a knob.
- **LCL / BOO condensed detachment** -- the goal driver engages maximally
  (315k discharges, 0 closures); forward-derived theorems never meet the
  backward subgoals. Needs a forward/backward bridge or splitting.
- **LAT138 lattice family** -- ground-joinability modulo AC; the completion
  trajectory diverges from Twee's after kept-rule ~50.
- **GRP66x/69x** -- genuine superposition depth (E 22-56s); `kws` reshapes the
  search into E's regime but does not crack it single-engine.
- **encompass determinism** -- LAT148-1 solves flakily (28-52s, straddles short
  walls; `--proof` mode diverges). A determinism fix makes it a clean flip.

The most leveraged next build is `s2a`/`slsq` (E's edge on exactly the FOF and
GRP problems we miss), plus native literal-selection saturation for FOF.
