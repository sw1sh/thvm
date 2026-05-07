# thvm docs

Architecture notes, one self-contained markdown per piece. Every page
links the C source it documents so the prose and the implementation
move together.

## Reading order

If you are new to the project, read these in order:

1. [term.md](term.md): how a 64-bit `Term` is packed.
2. [heap.md](heap.md): the flat heap, the bump allocator, and the
   substitution model that makes interactions work.
3. [normal_form.md](normal_form.md): two reducers -- `wnf` (weak
   normal form, two-phase enter/apply stack machine) and `nf`
   (full normal form, redex-list sweep).
4. [interact/_.md](interact/_.md): index of the active-pair rules,
   one page per rule.
5. [wl.md](wl.md): the Wolfram LibraryLink bridge and the high-level
   constructors that sit on top of it.
6. [heap_graph.md](heap_graph.md): the `THeapGraph[]` snapshot model,
   port conventions, and worked-example diagrams.
7. [grad.md](grad.md): automatic differentiation -- dup-like grad
   cells with gy threading, chain-rule adjoint table, leaf-SUP +
   DUP routing, higher-order, materialization integration, and
   `TProfile` for spotting allocation leaks.
8. [cpu.md](cpu.md): the CPU backend (`src/backend/cpu/`) and the
   codegen pipeline (`src/codegen/`).  Walks the
   `cpu_dispatch_kernel` order (BLAS, optional tile path, JITs,
   interpreters), the `propose -> apply_opt` chain, and the JIT
   cache key.  (Doc has a 2026-05-08 status banner: render_c.c is
   gone; CPU JIT compiles through `cg_render_uop_kernel_c`.)

## What's not here yet

Tracked in [PLAN.md](../PLAN.md). Anything labelled "step N" in the
status table at [README.md](../README.md) is intentionally absent
until that step lands.

## Plans and references

- [plans/hvm4_cross_reference.md](plans/hvm4_cross_reference.md):
  side-by-side of every piece of our runtime vs the corresponding
  HVM4 source. Refresh when a new interaction lands or HVM4 itself
  moves.
- [plans/waldmeister_ic_atp.md](plans/waldmeister_ic_atp.md):
  IC-native ATP design memo.  Summary of Waldmeister's unfailing
  Knuth-Bendix completion, prior art on interaction-net + ATP
  (Twee, Vampire, egg, HVM4 SupGen), and a SupGen/NeoGen-style
  build trajectory.  Read alongside the
  *Equational reasoning and the IC-as-ATP layer* section of
  [glossary.md](glossary.md) -- "superposition" means two different
  things on the two sides.
- [plans/tile_uops.md](plans/tile_uops.md): tile-level schedule
  plan above scalar UOps.  Documents the opt-in CPU/Metal tile paths
  and the remaining work toward an autotuning target.
- [plans/rewrite_fusion.md](plans/rewrite_fusion.md): tinygrad-style
  rewrite-driven fusion plan.  Documents the named realize-map rewrite
  harness and the path from boundary rewrites to scalar/tile
  canonicalization.
- [plans/bufferize.md](plans/bufferize.md): comprehensive plan for
  first-class `BUFFERIZE`/`INDEX` schedule IR, edge-local movement
  contexts, bufferize rewrites, memory planning, and autotune
  integration.
- [bench/history.md](bench/history.md): consolidated benchmark
  history and current canaries for training, GPT-2, Metal, tile, and
  autotune work.

## Conventions

- C source under [src/](../src/) follows path-is-the-function-name
  (see [AGENTS.md](../AGENTS.md) and HVM4's
  [STYLEGUIDE.md](../TinyHVM/HVM4/clang/STYLEGUIDE.md)).
- WL source under [wl/](../wl/) follows [wl/GUIDE.md](../wl/GUIDE.md).
- Each interaction lives in [src/interact/<name>.c](../src/interact/)
  and has a matching page in [docs/interact/<name>.md](interact/).
  The C file's leading comment block mirrors the sequent calculus
  rule from the doc -- if they disagree, the doc wins and the comment
  needs updating.
