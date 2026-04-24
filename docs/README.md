# thvm docs

Architecture notes, one self-contained markdown per piece. Every page
links the C source it documents so the prose and the implementation
move together.

## Reading order

If you are new to the project, read these in order:

1. [term.md](term.md): how a 64-bit `Term` is packed.
2. [heap.md](heap.md): the flat heap, the bump allocator, and the
   substitution model that makes interactions work.
3. [wnf.md](wnf.md): the two-phase enter/apply stack-machine reducer.
4. [interact/_.md](interact/_.md): index of the active-pair rules,
   one page per rule.
5. [wl.md](wl.md): the Wolfram LibraryLink bridge and the high-level
   constructors that sit on top of it.
6. [heap_graph.md](heap_graph.md): the `THeapGraph[]` snapshot model,
   port conventions, and worked-example diagrams.

## What's not here yet

Tracked in [PLAN.md](../PLAN.md). Anything labelled "step N" in the
status table at [README.md](../README.md) is intentionally absent
until that step lands.

## Plans and references

- [plans/hvm4_cross_reference.md](plans/hvm4_cross_reference.md):
  side-by-side of every piece of our runtime vs the corresponding
  HVM4 source. Refresh when a new interaction lands or HVM4 itself
  moves.

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
