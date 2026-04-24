#!/usr/bin/env wolframscript
(* generate.wls -- regenerate the PNGs embedded in docs/heap_graph.md.

   Run from the repo root:
       wolframscript -f docs/images/generate.wls

   Each example matches a snapshot section in docs/heap_graph.md.
   The renderer is THeapGraph[term] from the THVMLink paclet, exported
   at a small fixed pixel size so the images sit nicely next to the
   mermaid diagrams in the doc.
*)

PacletDirectoryLoad["wl/THVMLink"];
Needs["THVMLink`"];

debugPrint[args___] := WriteString["stdout", StringJoin @@ Map[ToString, {args}], "\n"]

$dir = "docs/images";
$size = {360, 240};

renderExample[name_String, builder_] := Block[{g, file},
    TReset[];
    g    = builder[];
    file = FileNameJoin[{$dir, name <> ".png"}];
    Export[file, g, "PNG", ImageSize -> $size, Background -> White];
    debugPrint["wrote ", file]
]

(* Example 1 (the identity lambda) is single-vertex with one self-loop.
   The self-loop disappears against the vertex disk in Wolfram's
   default layout, so the mermaid diagram in the doc is the better
   visual.  Skipping the WL render. *)

renderExample["heap_graph_2_id_app_era",
    {} |-> Block[{app}, app = TApp[TLam[var |-> var], TEra[]]; THeapGraph[app]]]

renderExample["heap_graph_4_dup_sup_setup",
    {} |-> TDup[TSup[TEra[], TEra[]],
                {dp0, dp1} |-> THeapGraph[dp0]]]

debugPrint["done"];
TFree[];
