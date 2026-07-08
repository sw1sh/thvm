# The ~1.6x speed gap to Waldmeister: root cause + roadmap

Date: 2026-07-08. Deep read-only scout (workflow wf_05ce0f86) of thvm's
FT term/index/emission/KBO layout vs Waldmeister's, with the WM sources
at `/Users/swish/src/wolfram/waldmeister` as ground truth.

## Verdict: it is NOT cell size or allocator design

thvm's `AtpFtCell` (24B, `src/atp/ft.h:45-52`) is a byte-for-byte clone
of WM's `TermzellenT` (`Nachf`/`Ende`/`Symbol`/`substFlag`,
`waldmeister/include/TermOperationen.h:66-73`). The dual-arena allocator
(`ft_alloc.c`) clones WM's SV nested manager including the O(1)
chain-push span free. Same profile SHAPE as WM (retrieval-dominant:
thvm ftdt 22% of samples, WM `MO_RegelGefunden` 28%). The residual
~1.6x is per-node-visit COST from four locality/traversal differences.
Byte-parity constrains VERDICTS and emitted byte streams only -- cell
addresses never enter a normal form (ftnfm is content-addressed FNV over
the preorder symbol sequence, `ft_alloc.c:77-79`; the PAD-amplifier
experiment already proved stride/spacing is trajectory-invisible) -- so
all four are freely fixable. One known landmine, untouched by all
proposals: byte-parity elsewhere depends on libc qsort tie order over
wmo var-kids.

### The four gaps

1. **Allocation order (the big one).** WM builds every new term in
   streaming PREORDER straight off the nested free list
   (`SV_ErstesNextFree`/`SV_AufNextFree`, `SpeicherVerwaltung.h:286-302`;
   `SigmaRGanzNeu`, `MatchOperationen.c:1397`), so `Nachf` is almost
   always the physically-next cell and a preorder walk is a near-
   sequential memory scan. thvm's `ftnew_ctr` (`ft.c:165-192`) builds
   BOTTOM-UP (children before parents), so preorder chains hop backward/
   around in memory and every `->next` is a dependent random load.
   Normalize is 35% of thvm wall vs 6% in real WM -- this is the block.

2. **Index node shape.** WM's DSBaum child lookup is one symbol-indexed
   load (`BK_Nachf(K,Symbol)=K->Variante.Innen.Nachf[Symbol]`,
   `DSBaumKnoten.h:178`) with backtrack state inside the node; thvm's
   `ftdt_descend3` touches a 104B `FtDtNode` plus up to three separately-
   malloc'd side arrays (labels/kids/kid_mask) per branch node.

3. **CP emission.** thvm's vcp emit walks the GC'd tagged Term heap with
   a `vcp_resolve` subst-chain chase plus `heap_read` per arity cell and
   per child (`_.c:2210-2430`), re-walking each binding subterm at every
   occurrence and TWICE (count pass + emit pass). WM never walks a
   composite -- it materializes sigma(r) once, preorder-contiguously.

4. **KBO.** thvm re-encodes both terms into `KboFlatNode` arrays per
   comparison (`ft_order.c:89-`, 12B written per node) before the slice
   compare; WM's `Vortest` (`ORD/KBO.c:254-301`) is a single lockstep
   `Schwanz` walk with zero writes.

## Ranked roadmap (each parity-certifiable, verdict-only)

### P1 -- Preorder-streaming term construction  [BEST: biggest win, zero parity risk]
Adopt WM's `SV_AufNextFree` discipline. Add a builder API
(`ft_build_begin` / `ft_build_append(sym,arity)` / `ft_build_end`) that
allocates cells in PREORDER along the free list / scratch bump, threading
`next` as it goes and backpatching `end` via a small stack exactly like
`SigmaRGanzNeu` (`MatchOperationen.c:1397`). Convert the hot construction
sites -- `ft_subst_apply`, splice rebuild (`ft_splice.c`), `ft_from_term`
(`ft.c:208+`), `vcp_ft_build_rec` (`_.c:2506`) -- away from bottom-up
`ftnew_ctr`. `ft_free_span` already preserves chain adjacency, so preorder
construction makes `->next` physically-next for fresh terms AND keeps that
adjacency across recycle generations, converting every downstream walk
(descend3 subject chase, ft_eq, KBO encode, match, normalize) from random
pointer-chasing to near-sequential scans.
- Expected: **10-20% engine wall** (upper-bounded by the PAD-amplifier
  result: if wall was stride-sensitive, locality is the lever).
- Parity risk: NONE for verdicts (addresses invisible). One audit:
  allocation-count/order side effects in arena statistics certifiers read
  (`n_persistent_alive` is write-only on hot paths).
- Blast radius: MEDIUM -- ~100-line builder in ft.c + rewiring ~4 sites;
  allocator, cell layout, all traversal code untouched; per-site kill-switch.

### P2 -- Subst-resolved binding pre-flattening for VCP emission
Per armed CP, resolve each DISTINCT binding `subst->bindings[k]` ONCE into
a cached record {packed byte span with symbolic var slots, node count, phi
contribution, per-var occurrence counts, FT span for the treated class};
then `vcp_count_side` becomes O(skeleton)+cached-size arithmetic and
`vcp_emit_side` walks the skeleton once, splicing each occurrence's cached
span with a per-occurrence var-slot remap (AcpNvMap first-occurrence order
preserved -> byte-identical trace bytes). Kills the 2x(count+emit) x
per-occurrence re-walk of shared sigma subtrees plus most heap_read/
vcp_resolve traffic.
- Expected: most of the 15% emission cluster, **~8-12% wall**.
- Parity risk: LOW (highest of the three) -- emitted byte stream + fused-
  KBO phidiff/balances must be bit-equal; certify with the differential
  pattern (env-gated like `THVM_ATPFT_KBO_DIFF`).
- Blast radius: CONTAINED -- inside the vcp block of _.c (~400 lines) +
  a binding-cache struct; classic fallback already exists.

### P3 -- FTDT node packing (one-cache-line query view + interleaved edge pool)
Split `FtDtNode` into (a) a hot 16B query header array {n_edge:u16,
n_var_edge:u16, label0:u32, kid0:u32, sub_mask:u8, edge_ofs} covering the
dominant single-edge chain hop with ONE sequential-array load, and (b) a
single shared edge pool of interleaved 12B records {label,kid,kid_mask}
replacing the three per-node mallocs; leaf/aux/cold fields in a parallel
cold array. Optionally add WM-style direct symbol indexing (`BK_Nachf`)
for high-fanout root nodes on small signatures. Exploration order
unchanged -> identical candidate sets/verdicts.
- Expected: ~half the descend3 edge-resolution cost, **~5-8% wall**.
- Parity risk: none (pure layout); certify with `THVM_FT_DTV2_DIFF`.
- Blast radius: MEDIUM-HIGH -- ft_norm.c index build + all three descend
  variants (~600 lines), but mechanical.

### P4 (rider, if the KBO cluster survives P1) -- direct lockstep KBO compare
Port WM's `Vortest`/`TermeKuerzen` (`ORD/KBO.c:241-301`) to walk the two
cell chains directly (phidiff + var-balance in one pass, no `KboFlatNode`
re-encode, subst inlined via `ft_subst_lookup`), replacing
`atpft_kbo_flat_encode(+_subst)+slice` in the unorient gate
(`ft_norm.c:2392-2485`). ~half of the 9% KBO cluster is encode-side write
traffic. Verdict-only parity; certifier `THVM_ATPFT_KBO_DIFF` exists;
blast radius small (ft_order.c + a comparator beside kbo_flat_slice).

## MEASURED CORRECTION (2026-07-08, P1 partially built)

P1's builder pattern is BUILT + byte-certified for two sites behind
THVM_ATP_FT_PREORDER (commits cfab3cf8, d98b3a18): ft_from_term and
ft_subst_apply/ft_deep_copy. Both are byte-identical (full RULEADD
streams vs baseline on the McCune/Sheffer/Meredith/WolframOA battery,
suite green) -- BUT both measured **~0-1% on OA**, because they are NOT
the hot construction path:
- ft_from_term is the goal-join / FT-mirror BOUNDARY (called O(goals),
  not O(CPs)).
- ft_subst_apply is the zero-copy ft_splice's differential-test
  BASELINE -- cold on the hot normalize path (which splices in place).
The ACTUAL hot construction on the OA WM-preset path is the FUSED vcp
pipeline: vcp_ft_build_emit_side / vcp_ft_build_rec (src/atp/_.c ~2506),
which builds FT cells + emits bytes + accumulates the KBO weight in ONE
walk. That is the site P1 must convert for the OA emulation win -- and
it is the most complex (fused), so the preorder discipline must thread
through the byte-emit + weight accumulation too, or be applied only to
the cell-build half. Separately, the GATE bottleneck (Hillman/Meredith
c3) is the TERM-SIDE ground-join normalize (gj_rewrite_step ->
thvm_subst_apply, not the FT path at all) -- a different builder again.
So: the two landed conversions are a validated, byte-safe FOUNDATION;
the real wins need (a) the fused vcp builder [OA speed] and (b) the
Term-side GJ subst-apply [gate throughput]. Re-profile each target
before converting; the ~1.6x may be less locality-bound than the
original scout estimated (two real conversions bought ~0%).

## ROOT CAUSE of the ~0% (2026-07-08, decisive -- P1 is a DEAD END as scoped)

Built + measured the preorder discipline; root-caused why it gives ~0%.
The hot builders (ft_subst_apply, vcp_ft_build_emit_rec) allocate on the
PERSISTENT arena (scratch=0), which is a FREE-LIST (ft_alloc_persistent_
raw: `cell = free_head; free_head = cell->next`). After a long run's
alloc/free churn the free-list order is SCRAMBLED, so "allocate parent
before children" does NOT yield physically-consecutive cells -- the
->next loads stay random. The scout's premise ("ft_free_span preserves
chain adjacency") holds for a single freed span but NOT across many
generations of interleaved alloc/free. So parent-first construction is
locality-neutral on the persistent arena; P1 as scoped (reorder
construction only) CANNOT deliver the win. Confirmed by ft_from_term
(~1%) AND ft_subst_apply (~0%): two correct conversions, ~0 speedup.

CONSEQUENCE -- roadmap re-derived:
- The ftdt_descend3 cost (22% of samples, the "random loads" the scout
  saw) is therefore NOT the subject term's construction -- it is the
  RULE-INDEX dt nodes (the 104B FtDtNode + 3 side arrays per branch),
  i.e. P3 (node packing), which does not depend on construction order.
  RE-ATTRIBUTE: P3 is the likely real ftdt lever, not P1.
- The genuine P1-style win requires ARENA LOCALITY: a bump or COMPACTING
  persistent arena (like WM's SV, which hands out sequential cells), NOT
  just construction order. That is a much larger change (a compacting
  collector over the FT arena) with its own risks.
- The two landed conversions (THVM_ATP_FT_PREORDER, byte-certified) stay
  as a validated foundation and are correct if a bump/compacting arena
  ever lands, but are ~0% today. Do NOT convert more sites (vcp builder)
  expecting a win -- same free-list, same ~0%.
NEXT speed lever to try: P3 (ftdt node packing -- one cache-line query
header + interleaved edge pool), which attacks the confirmed
ftdt_descend3 hot loop and is construction-order-independent.

## Sequencing note
P1 first (it changes the memory layout every other walk observes, so its
win partly SUBSUMES P3/P4's locality argument -- re-profile after P1
before committing to P3). Each behind a per-site kill switch, each
certified against a from-main baseline binary on the McCune/Sheffer/OA
battery (full RULEADD/stderr byte-identity) + the differential harness
where one exists. No sweeps -- one theorem battery per change.
