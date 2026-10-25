# ATP source modularization (post-CASC-J13 cleanup)

Started 2026-07-27, right after the J13 post-mortem
(`docs/ATP/casc_j13_postmortem.md`). One year to the next competition; first
structural investment is breaking the two monoliths into organized modules so
the coming year's engine work (endgame queue economics, FOF saturation,
checker rebuild) lands in reviewable units.

## Current shape

- `src/atp/_.c` -- 32,930 lines. Includes `lpo_cache.c`, `ac.c`,
  `goal_narrow.c`, `wm_order.c`, `wm_intake.c` inline. Everything else is one
  file: state, CP storage, emission, indexes, GC, heaps, weights, GNN, trace,
  MNF, subsumption, ground-join, step driver, narrowing.
- `tools/thvm_atp_cli.c` -- 9,496 lines: knobs, schedule, FOF prepass,
  soundness gates, SZS plumbing, main.
- The FT family (`ft_*.c`, 16 files) is already well-factored; leave it.
- Single-TU build: `tools/thvm_atp_cli.c` includes
  `tests/atp/test_atp_wolfram_bench.c` and `tools/proof_check.c`; the bench
  includes `src/atp/_.c`; `tests/atp/test_atp.c` includes it too.

## Ground rules (the mechanical pass)

1. **Single TU is preserved.** `src/atp/_.c` becomes a thin spine of ordered
   `#include "<module>.c"` lines. No linkage changes, no static-to-extern
   promotion, no build-system changes.
2. **Pure code movement.** No renames, no reordering, no cleanup, no comment
   edits in this pass. Semantic hygiene (naming, dead code, headers) is a
   later, separate pass on top of the modular layout.
3. **The identity gate.** Because the TU token stream must be unchanged, after
   every carve run the preprocessor over each of the three TUs and diff
   against the pre-refactor capture:

   ```
   cc -E <exact-flags> tests/atp/test_atp.c               | sed '/^#/d;/^$/d' > A
   cc -E <exact-flags> tests/atp/test_atp_wolfram_bench.c | sed '/^#/d;/^$/d' > B
   cc -E <exact-flags> tools/thvm_atp_cli.c           | sed '/^#/d;/^$/d' > C
   ```

   Byte-identical to baseline = the refactor provably changed nothing. The
   exact flag sets come from `make -n bin/test_atp`,
   `make -n bin/test_atp_wolfram_bench`, `make -n bin/FindProof` (captured:
   the ATP_*/THVM_ATPFT_* define block; bench adds `-DTHVM_ATP_AC -DATP_MNF`).
4. **Carve bottom-up** (highest line ranges first) so earlier banner line
   numbers stay valid throughout.
5. Commit per module (semantic messages). End-of-pass belt-and-braces:
   `bin/test_atp` exact count, OA/DN bench pins, `make && make wl`.

## Module map for src/atp/_.c (from its own section banners)

| new file | from banner (approx line at HEAD) | contents |
|---|---|---|
| `prim.c` | :92 | TAG_PRI primitives, state plumbing |
| `cp_store.c` | :1556, :1622, :2112 | implicit-CP slot bookkeeping, packed byte-string CPs, fused varnorm+pack+weigh |
| `cp_emit.c` | :2606, :2901, :3351 | FT-direct CP emission, treated <50 route, virtual order gate |
| `fv_index.c` | :4193 | CP-queue subsumption index |
| `rule_index.c` | :4938 | rule-LHS redex index |
| `norm_paths.c` | :6103, :6133, :6431 | FT discrim descent, WM-FPA normalize, mixed fast-path |
| `overlap_index.c` | :6999 | CP-gen overlap-partner index (U1_KPsBildenZuRegel) |
| `gc.c` | :7886, :7916 | heap checkpoint, in-loop GC, trace roots |
| `ordered_rewrite.c` | :11073, :11110, :11141 | var-occurrence helpers, ordered rewriting, FVI |
| `cp_weight.c` | :11752, :12102, :12183 | ClasHeuristics, goal-directed selection, goal-subterm |
| `gnn.c` | :12597 | ENIGMA Tier-2 in-engine GCN scorer |
| `cp_heap.c` | :13417, :13623 | binary min-heap, KD-heap port |
| `rerank.c` | :15295 | live re-rank seam (WL GNN) |
| `trace.c` | :18050, :18149 | PCL trace serializer, proof extraction |
| `enigma_data.c` | :18556, :18624 | CP feature extraction, hypergraph builder |
| `mnf.c` | :19136, :20573 | MNF goal search + MNF proof extraction |
| `wm_drain.c` | :21890, :23229 | drain-key snapshot, IR-victim drain |
| `proof_cone.c` | :23507 | post-PROVED norm re-derivation |
| `diag.c` | :24013 | diagnostics |
| `subsume.c` | :24741, :25256 | WM flatterm subsumption, comm-aware E-set |
| `ground_join.c` | :25531 | ground-joinability redundancy |
| `step.c` | :29306 | main step driver + engine API |
| `narrow.c` | :31642, :31690, :31774, :31884 | top-K peek, narrowing, critical-goal queue |

Boundaries are refined at carve time by reading the code around each banner
(a section's static helpers must move with it); the table is the shape, not a
contract. Existing includes (`lpo_cache.c`, `ac.c`, `goal_narrow.c`,
`wm_order.c`, `wm_intake.c`) stay where they are in the spine.

## Phase B: tools/thvm_atp_cli.c

Same treatment into `tools/atp_cli/`: knobs table, schedule + run_schedule,
FOF clausal prepass, soundness gates (one-element model, format sniff), SZS
output, main. The CLI's banner pairs at :60, :245, :949, :995, :1369, :1481,
:1683, :3478, :4780, :5274, :5603, :7685, :8224 delimit the sections.

## Phase C (separate, later): semantic hygiene

Only after the mechanical pass is landed and gated: header extraction for the
cross-module API surface, dead-code sweep (e.g. the refuted trace-backed
implicit-CP lane pending the endgame arc's verdict), naming normalization.
Each with the full behavior gates, none with the -E identity gate (they are
semantic changes by definition).
