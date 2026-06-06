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

## Building the notebooks

Run [`build_docs.wls`](../../scripts/build_docs.wls) from anywhere; it scans
`docs/**.md`, routes each source to the standard paclet documentation layout
under `Documentation/English/`, and also builds the submission notebook at
`ResourceDefinition.nb` at the paclet root:

```
wolframscript -file wl/scripts/build_docs.wls
```

`build_docs.wls` fetches `MarkdownToNotebook` from its public cloud deployment
(`https://www.wolframcloud.com/obj/nikm/DeployedResources/Function/MarkdownToNotebook`),
so no local clone of the converter is required.

Then run `DocumentationBuild` over the assembled `Documentation/` tree to
produce the published `ref/`, `guide/`, and `tutorial/` pages.

Before submission to the Paclet Repository, run the docked *Check* button
on each notebook or, headless:

```wl
Needs["DefinitionNotebookClient`"];
UsingFrontEnd @ Block[{ nbo = NotebookOpen[File["ResourceDefinition.nb"]] },
    CurrentValue[nbo, CreateCellID] = True;
    SelectionMove[nbo, All, Notebook];
    FrontEndTokenExecute[nbo, "Save"];
    Normal @ DefinitionNotebookClient`CheckDefinitionNotebook[nbo]
]
```

## What is and is not in this scope

The `Symbols/` directory ships pages for the top-level user-facing wrappers.
The other files [`Kernel/README.md`](../Kernel/README.md) catalogs (LibraryLink
loaders, tag/dtype/opcode constants, side-table accessors) are documented in
the in-source `::usage` strings and are reachable from the guide; promoting
one to a full symbol page is a one-file change (frontmatter + `Usage` +
`Basic Examples`) using the `wolfram-symbol-page` skill.
