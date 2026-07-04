# ATP normalize-cell shrink: amplifier de-risk + layout audit (VERDICT: NO-GO)

Status: DE-RISK DELIVERED 2026-07-04. This is the decisive experiment the
mission demanded BEFORE any multi-week rewrite of thvm's hottest data
structure (the 24-byte `AtpFtCell` traversed by `ftdt_descend3`, ~50% of
engine self-time on the byte-identical OrAssociativity/FEQ trajectory).

READ-ORDER: docs/plans/atp_inplace_rewrite.md (the payoff table + the
"normalize excess = per-node constant factor" finding, Section 2) ->
project_atp_wm_speed_profiled.md later-14/23/24/25/28 (the sub-bar
refutations) -> this doc.

THE HEADLINE VERDICT:

- **Cell size is NOT the lever.** The amplifier (a byte-neutral stride knob
  that widens the per-cell footprint WITHOUT changing any field read)
  measured normalize as only WEAKLY footprint-sensitive and SUB-LINEARLY so:
  DOUBLING the cell (24 -> 48 B) costs only +0.30s / +3.4% engine, while a
  realistic +8/+16 B widening is FLAT (~+0.02/+0.05s, within noise). The
  realistic SHRINK direction (24 -> 16 -> 12 B) sits in the empirically-flat
  regime and projects to ~-0.02 to -0.10s -- 4-20x below the 0.4s bar.
- **There is no shrink available anyway.** The layout audit finds native
  Waldmeister's own flatterm cell (`TermzellenT`) is ALSO 24 bytes,
  byte-for-byte the same size as thvm's `AtpFtCell`. Both are dominated by
  two 8-byte pointers (next+end = 16 B = 2/3 of the cell) and both round to
  24 by 8-byte pointer alignment. thvm already TIES native on cell size; the
  1.43x per-node cost (later-23) is not a size difference.
- **GO/NO-GO: NO-GO on the cell-shrink rewrite.** Normalize is COMPUTE-bound
  on the traversal/compares, not footprint/cache-bound. Every compute lever
  was already refuted (retrieval parity l23, recursion compiler-optimal l24,
  cheaper-fteq bounded <0.5s l14, interchange 0.57% l25). The ~1.25x FEQ gap
  is at its architectural floor. This negative SAVES the multi-week rewrite.

---

## 1. The amplifier (THVM_ATP_FT_CELL_PAD), and why it is decisive

`THVM_ATP_FT_CELL_PAD=N` (src/atp/ft_alloc.c, default 0) adds N bytes
(rounded to the cell's 8-byte alignment) of UNUSED padding to the per-cell
allocation STRIDE in BOTH arenas (persistent slab + scratch bump). The
`AtpFtCell` struct stays 24 bytes and every field is read at its original
offset -- only the SPACING between consecutive cells changes, so fewer cells
per 64-byte cache line / more TLB pages. It is byte-identity-preserving by
construction: `ftnfm` is content-addressed (FNV over the pre-order symbol
sequence, ft_norm.c:2788), so cell addresses/spacing never enter a normal
form. PAD=0 restores the exact 24-byte contiguous layout (ft_cell_at(base,i)
== &base[i], scratch bump == ++, block malloc == sizeof(AtpFtBlock)); the
shipping path is untouched.

This is the SIZE-SENSITIVITY AMPLIFIER: if normalize wall scales with the
stride, cell size IS the lever and shrinking pays; if wall is FLAT, normalize
is compute-bound and a shrink CANNOT close the gap. It decides the whole
question with a cheap experiment instead of a multi-week rewrite.

### Measurement (non-lazy FEQ proxy, byte-for-byte 263550/771/1654086)

Command (per level, 3 runs, /usr/bin/time -l, quiet-as-available box):

    THVM_ATP_RSS_ABORT_MB=5300 THVM_ATP_WALDMEISTER=1 THVM_ATP_LAZY_NORM=0 \
      THVM_ATP_FT_CELL_PAD=<N> \
      ./bin/test_atp_wolfram_bench \
      tools/baselines/wm_pr/WolframAxioms__OrAssociativity.pr 500000 240

steps/sec is CPU-time-based (`clock()`), the low-noise cache-sensitivity
metric (a cache/TLB-bound stall shows up as CPU busy time on the core).

| PAD | stride (B) | cells/64B line | steps/sec (median of 3) | engine (s) | dEngine vs 24B | RSS (GB) | pins |
|----:|-----------:|---------------:|------------------------:|-----------:|---------------:|---------:|------|
|   0 |         24 |           2.67 |                   30108 |       8.75 |          0.00  |     2.52 | EXACT 263550/771/1654086 |
|   8 |         32 |           2.00 |                   30053 |       8.77 |         +0.02  |     3.06 | EXACT |
|  16 |         40 |           1.60 |                   29950 |       8.80 |         +0.05  |     3.62 | EXACT |
|  24 |         48 |           1.33 |                   29109 |       9.05 |         +0.30  |     4.15 | EXACT |
|  32 |         56 |           1.14 |                   28830 |       9.14 |         +0.39  |     4.69 | EXACT |
| (48)|         72 |           0.89 |          (RSS artifact) |     (9.2)  |    (diverged)  |     5.69 | 261120/786/1630979 |

engine(s) = 263550 / steps_per_sec. PAD=48 DIVERGED the trajectory
(261120/786/1630979) NOT because padding breaks a normal form -- PAD=8/16/24/32
are all byte-EXACT -- but because its 3x FT-arena footprint (5.69 GB) crossed
the RSS_ABORT=5300 MB guard, whose recycle path perturbs CP selection. It is
an RSS-guard artifact, out of the clean window; the clean 5-point slope
(24..56 B) is what the verdict rests on.

### Reading the slope (the payoff projection)

The response is CONVEX / sub-linear: the first +8 B and +16 B barely move the
engine (+0.02/+0.05s, within run-to-run noise), and the effect only becomes
detectable at the DOUBLING (+24 B -> +0.30s) and beyond (+32 B -> +0.39s).
This is the cache-line-crossing signature: nothing happens until the average
cells-per-line drops toward 1, at which the sequential `->next`/`->end` walk
starts crossing an extra line per hop.

- Near-24 slope (24 -> 32 B, the regime a shrink lives NEXT to): 0.0025 s/B.
- Full-range slope (24 -> 48 B): 0.0125 s/B.
- Steep far slope (48 -> 56 B): 0.011 s/B.

The SHRINK direction (24 -> 16 -> 12 B) increases cells/line (2.67 -> 4 ->
5.33), i.e. it moves DEEPER into the flat regime, so the near-24 slope
(0.0025 s/B) is the honest one:

- 24 -> 16 B (-8 B, a 33% shrink): ~ -0.02s (near-24) ... -0.10s (full-range UB).
- 24 -> 12 B (-12 B, a 50% shrink): ~ -0.03s (near-24) ... -0.15s (full-range UB).

**#1 payoff number: ~-0.05s best case, sub-bar under EVERY reading (the
generous full-range upper bound is -0.15s; the honest near-24 slope is
-0.05s), vs the 0.4s bar.** Cell size is not a >0.4s lever.

---

## 2. Layout audit: thvm AtpFtCell vs native TermzellenT (both 24 bytes)

### thvm `AtpFtCell` (src/atp/ft.h:45), 24 B, 8-byte aligned

| field  | off | width | hot-path reader | why that width |
|--------|----:|------:|-----------------|----------------|
| `next` |   0 |   8 B | ftdt_descend3 every hop (`sc=sc->next`); ft_eq lockstep; find_redex pre-order | pre-order link (WM Nachf); STRUCTURAL 8-byte pointer |
| `end`  |   8 |   8 B | ftdt_descend3 subterm skip (`sc->end->next`); ft_eq; ft_wm_pattern_before | last-cell-of-subterm (WM Ende); STRUCTURAL 8-byte pointer -- the O(1) skip |
| `sym`  |  16 |   4 B | ftdt_descend3 EVERY node (var-bit test + label compare + first-cell fail-fast); ft_eq | id + WF_VAR_BIT (0x80000000) var marker; low 31 bits id. Only ~16 bits used on FEQ/OA |
| `arity`|  20 |   2 B | ft_child / atp_ft_arity / KBO encode -- NOT ftdt_descend3 | cached arity 0..16 (REWRITE_MAX_ARITY=16, needs 5 bits). thvm-SPECIFIC cache (native has none) |
| `flags`|  22 |   1 B | find_redex (subst_fresh), ftnew_ctr (ground fold), KBO (ground/rank) -- NOT ftdt_descend3 | 3 bits: subst_fresh (WM substFlag) + ground + lpo_rank_valid |
| `_pad` |  23 |   1 B | never read | alignment tail |

Useful: 128 (ptrs) + ~16 (sym) + 5 (arity) + 3 (flags) = ~152 bits = 19 B,
padded to 24 by the two pointers' 8-byte alignment.

### native `TermzellenT` (waldmeister include/TermOperationen.h:66-73), 24 B

    typedef struct TermzellenTS {
      TermzellenZeigerT Nachf, Ende;   // next + end, two 8-byte pointers
      SymbolT           Symbol;        // signed short (2 B); sign bit = variable
      char              substFlag;     // 1 B, innermost-rewrite skip
    } TermzellenT;                      // sizeof == 24 (confirmed, arm64 64-bit)

- Symbol is a 2-byte SIGNED short: positive = function/const, negative =
  variable (SO_NummerVar = -(n)) -- native packs var-ness into the sign bit
  of a 16-bit field (thvm uses the high bit of a 32-bit field: same idea,
  2 bytes wider).
- Native has NO per-cell ARITY -- it derives arity from the signature table
  by symbol. thvm caches arity on the cell (2 B) to avoid a per-visit table
  lookup; but ftdt_descend3 does not read it, so it does not cost the hot
  descent.
- Useful 16 (ptrs=... 16 B) + 2 + 1 = 19 B, padded to 24. **Byte-for-byte the
  same 24 as thvm.**

### What is shrinkable (concrete design + why it does not pay)

The two pointers are 16 B = 2/3 of the cell and are STRUCTURAL to BOTH engines
(next = pre-order walk; end = O(1) subterm skip `sc->end->next`, read on
every branch of ftdt_descend3). Native keeps both.

- Packing the scalar tail tighter (sym u32 -> u16, matching native's 2 B;
  arity+flags -> one u16) yields a 21-byte struct -- which STILL rounds to 24
  because the two 8-byte pointers force 8-byte alignment. **ZERO size
  reduction.** This is the crux: any cell that keeps two pointers is 24 B.
- Dropping `end` and deriving subterm-end by an arity-walk shrinks the cell
  to 16 B (one pointer + packed scalars) but converts the O(1) `sc->end->next`
  skip into an O(subterm) scan -- ADDING compute to the hot descent. Native
  keeps `end` precisely to avoid this. Rejected.
- The ONLY path below 24 B is INDEX-based cells: replace both 8-byte pointers
  with 32-bit arena-relative indices (next_idx + end_idx = 8 B) + a packed
  8-byte scalar tail = a 16-byte cell (33% shrink). BUT this converts every
  hot `->next`/`->end` deref into `base + idx*stride` address arithmetic --
  adding a multiply/add to the hottest per-node operation in the engine.
  Given the amplifier proves normalize is COMPUTE-bound (footprint barely
  moves it), this trades ~-0.05s of footprint for added per-node compute that
  the later-24 evidence (iterative descent added compute -> +1.15s) says
  DOMINATES. Projected net: ~0 to NET-NEGATIVE.

**Shrunk-cell design + #1 payoff:** the 16-byte index-cell is the only real
compaction, and it projects to ~-0.05s footprint saving OFFSET by added
index-arithmetic compute = ~0 to net-negative, vs the 0.4s bar. NO-GO.

---

## 3. Pivot: is the compute (ft_eq volume / descent) reducible? -- No.

With cell-size refuted, the residual 1.43x per-node cost is pure compute, and
every compute axis was already measured sub-bar or net-negative:

- Node-touch COUNT at parity with native (16.71 vs 16.16, later-23) -- a
  better retrieval index cannot cut below what thvm already ties.
- ft_eq volume: 264.8M attempts, but 68.6% die free on the inline first-cell
  sym compare; real ft_eq = 83M walks over 2.84-cell subjects. later-14
  bounded cheaper-fteq at <0.5s TOTAL, found the FTNFM cap not binding, and
  found no hot nodes to special-case (top-16 = 45.5% of visits, smooth decay).
- Recursion -> iterative backtrack-stack: BUILT, byte-identical, measured
  NET-NEGATIVE (+1.15s, later-24) -- the recursion is compiler-optimal.
- Heap-Term interchange: 0.57% self-time (later-25), not the gap.

The 1.43x is the irreducible constant factor of thvm's per-node C descent vs
native's hand-tuned goto-DFS on the SAME node count, SAME 24-byte cell, SAME
retrieval selectivity. The profile-guided bounded-optimization space in
normalize is EXHAUSTED. The only non-refuted candidate anywhere is orthogonal
to normalize (the CP binary heap vs native KDHeap, ~0.19-0.4s bound, later-27),
out of this mission's scope.

---

## 4. Disposition + gates

Landed: the amplifier knob THVM_ATP_FT_CELL_PAD (default 0 = shipping path,
byte-identical) as a certified, default-neutral diagnostic scaffold -- so the
negative is reproducible and the cell-shrink axis is not re-litigated. No
shrink increment is landed (refuted). Contained to src/atp/ft_alloc.c
(+ this doc); no header/ABI change, no struct change (AtpFtCell stays 24 B,
_Static_assert intact).

Byte-identity: PAD=0/8/16/24/32 all reproduce FEQ 263550/771/1654086 EXACT
(only RSS differs); the knob is byte-EXACT up to the RSS-guard ceiling
(PAD < 48 at RSS_ABORT=5300). The DEFAULT (unset = PAD=0) is byte-for-byte the
prior allocator.

GATES (verbatim, this HEAD):
- FEQ non-lazy 263550/771/1654086 EXACT (PAD unset, 0, 8, 16, 24, 32)
- lazy OA 278807/753/2642990 EXACT (default)
- DN 2848/254/768876 EXACT (default; + NORMCORE2_CHECK; + FT_EMIT_CHECK)
- bin/test_atp 136241/136241 (default)
- make rc=0; make wl rc=0
