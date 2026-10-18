# THVMLink documentation sources

The literate-markdown sources for the THVMLink paclet's documentation
notebooks. Built through `MarkdownToNotebook` into `Documentation/English/`
when the paclet is deployed.

## Layout

- [`ResourceDefinition.md`](ResourceDefinition.md) - the paclet's
  Paclet Repository definition (`Template: Paclet`).
- [`Guides/THVMLink.md`](Guides/THVMLink.md) - the paclet's main guide
  page (`Template: Guide`), listing every public symbol grouped by
  subsystem.
- [`Symbols/`](Symbols/) - one `Template: Symbol` reference page per
  documented symbol. The current set is a representative subset of the
  ~412 `::usage`-bearing public symbols; add more by following the same
  shape (the [`wolfram-symbol-page`](https://github.com/sw1sh/MarkdownToNotebook/blob/main/skills/wolfram-symbol-page/SKILL.md)
  skill is the canonical authoring guide).
  - [`Symbols/TTensorCreate.md`](Symbols/TTensorCreate.md) - tensor ingest, zero-copy from `NumericArray`.
  - [`Symbols/TUOpAdd.md`](Symbols/TUOpAdd.md) - representative UOp constructor; the rest of the `TUOp*` family follows the same shape.
  - [`Symbols/TGrad.md`](Symbols/TGrad.md) - `loss.backward()`, full-graph and target-aware VJP forms.
  - [`Symbols/TKernel.md`](Symbols/TKernel.md) - typed kernel object with property surface, autotune, source.
  - [`Symbols/THeapGraph.md`](Symbols/THeapGraph.md) - IC string-diagram visualization of the live heap.
  - [`Symbols/TMemoryPlan.md`](Symbols/TMemoryPlan.md) - per-buffer alive-span snapshot.
  - [`Symbols/TContextSnapshot.md`](Symbols/TContextSnapshot.md) - portable snapshot of the live runtime.
  - [`Symbols/TBench.md`](Symbols/TBench.md) - training-loop benchmark + memory peak.
  - [`Symbols/TFindProof.md`](Symbols/TFindProof.md) - C-engine equational ATP returning a Wolfram `ProofObject`.
- [`Tutorials/`](Tutorials/) - free-flowing tech notes (`Template: TechNote`).
  - [`Tutorials/Overview.md`](Tutorials/Overview.md) - end-to-end tour: heap, tensors, autodiff, kernels, snapshots, ATP.

## Authoring

The four [`MarkdownToNotebook`](https://github.com/sw1sh/MarkdownToNotebook)
skills define the format:

- [`wolfram-paclet`](https://github.com/sw1sh/MarkdownToNotebook/blob/main/skills/wolfram-paclet/SKILL.md) - paclet `ResourceDefinition.md`.
- [`wolfram-guide-page`](https://github.com/sw1sh/MarkdownToNotebook/blob/main/skills/wolfram-guide-page/SKILL.md) - guide pages.
- [`wolfram-symbol-page`](https://github.com/sw1sh/MarkdownToNotebook/blob/main/skills/wolfram-symbol-page/SKILL.md) - symbol reference pages.
- [`wolfram-tech-note`](https://github.com/sw1sh/MarkdownToNotebook/blob/main/skills/wolfram-tech-note/SKILL.md) - tutorials / tech notes.

The [AccessibleColors](https://github.com/sw1sh/AccessibleColors) paclet is
the worked example every skill points to.

## Building, releasing and submitting

[PacletPage](https://github.com/WolframInstitute/PacletPage) does all three, through three thin
scripts in [`wl/scripts`](../../scripts) that share [`pacletpage.wl`](../../scripts/pacletpage.wl).
Build the platform libraries first, since the examples and the archive use them:

```
make wl-cross                                   # one library per Wolfram $SystemID
wl -f wl/scripts/build_docs.wls                 # docs/**.md -> Documentation/English, ResourceDefinition.nb
```

[`build_docs.wls`](../../scripts/build_docs.wls) runs `BuildPacletDocumentation`: every page becomes
an authoring notebook under `Documentation/English/`, and `ResourceDefinition.md` becomes
`ResourceDefinition.nb` at the paclet root. A page converts again only when its source, the
paclet's code or the converter changed (`--no-cache` evaluates every example afresh). The pages
in `$unevaluatedPages` (`pacletpage.wl`) are built without evaluating their examples until the
tensor-lane crash they would hit is fixed.

Release and submit as the Wolfram Institute, which owns the `WolframInstitute` publisher, with
`--dry-run` first:

```
wl -f ~/.claude/skills/wolfram-cloud-account/run-as.wls wolframinstitute wl/scripts/release.wls --dry-run
wl -f ~/.claude/skills/wolfram-cloud-account/run-as.wls wolframinstitute wl/scripts/release.wls
wl -f ~/.claude/skills/wolfram-cloud-account/run-as.wls wolframinstitute wl/scripts/submit.wls --dry-run
wl -f ~/.claude/skills/wolfram-cloud-account/run-as.wls wolframinstitute wl/scripts/submit.wls
```

[`release.wls`](../../scripts/release.wls) builds the documentation, deploys the paclet resource,
publishes the documentation site around it and checks every page;
[`submit.wls`](../../scripts/submit.wls) submits the paclet to the Paclet Repository, after the
definition notebook's own check and a fresh build that must hold every page and load alone.

## What is and is not in this scope

The `Symbols/` directory ships pages for the top-level user-facing wrappers.
The other files [`Kernel/README.md`](../Kernel/README.md) catalogs (LibraryLink
loaders, tag/dtype/opcode constants, side-table accessors) are documented in
the in-source `::usage` strings and are reachable from the guide; promoting
one to a full symbol page is a one-file change (frontmatter + `Usage` +
`Basic Examples`) using the `wolfram-symbol-page` skill.
