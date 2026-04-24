(* ::Package:: *)
(* THVMLink - Wolfram Language bridge to the thvm interaction-net runtime.

   The C bridge (CSource/thvmlink.c) exports 14 scalar functions covering
   term packing/unpacking, heap access, the WNF entry point, and a few
   counters.  This package wraps them and adds high-level constructors
   (TLam / TApp / TSup / TDup), inspection helpers (TTermInfo, THeap,
   TTagName), and the IC-style heap renderer (THeapGraph).
*)

BeginPackage["THVMLink`", {"GeneralUtilities`"}];

(* === lifecycle === *)
TInit::usage      = "TInit[] initializes the runtime.  Returns True.";
TFree::usage      = "TFree[] tears the runtime down.";
TReset::usage     = "TReset[] zeroes the heap, the WNF stack, and the interaction counter.";

(* === term primitives (raw, scalar) === *)
TTermNew::usage   = "TTermNew[sub, tag, ext, val] packs a 64-bit Term.";
TTermTag::usage   = "TTermTag[term] returns the tag (Integer).";
TTermExt::usage   = "TTermExt[term] returns the EXT field.";
TTermVal::usage   = "TTermVal[term] returns the VAL field (heap loc, etc.).";
TTermSub::usage   = "TTermSub[term] returns the SUB flag (0 or 1).";
TTagName::usage   = "TTagName[tag] returns a string for a tag id.";
TTermInfo::usage  = "TTermInfo[term] returns an Association decoding sub/tag/ext/val.";

(* === heap === *)
THeapPos::usage   = "THeapPos[] returns the next free heap location.";
THeapAlloc::usage = "THeapAlloc[size] reserves `size` consecutive cells; returns the base loc.";
THeapRead::usage  = "THeapRead[loc] returns the Term at heap[loc].";
THeapSet::usage   = "THeapSet[loc, term] writes `term` to heap[loc].";
THeap::usage      = "THeap[] returns an Association snapshot with keys \"nextLoc\", \"cells\", \"Graph\".  See docs/heap_graph.md.";
THeapGraph::usage = "THeapGraph[] renders the heap state as an IC string-diagram Graph.  THeapGraph[term] also seeds discovery with `term` so heapless compounds held only by the WL caller appear.  THeapGraph[{t1, t2, ...}] seeds with several.  See docs/heap_graph.md.";

(* === reduce / stats === *)
TWnf::usage       = "TWnf[term] reduces `term` to weak normal form.";
TItrs::usage      = "TItrs[] returns the cumulative interaction count.";

(* === high-level constructors === *)
TFreshLabel::usage = "TFreshLabel[] returns the next integer from a monotonic SUP/DUP label counter, then bumps it.  Reset by TReset[].";
TEra::usage       = "TEra[] constructs an eraser term.";
TVarFor::usage    = "TVarFor[lamLoc] constructs a VAR pointing at a binder loc.";
TLam::usage       = "TLam[builder] constructs a lambda; `builder` receives the bound var and returns the body.";
TApp::usage       = "TApp[fun, arg] constructs an application.";
TSup::usage       = "TSup[a, b] constructs a SUP with a fresh label.  TSup[label, a, b] uses an explicit label.";
TDup::usage       = "TDup[body, k] constructs a DUP with a fresh label and calls `k[dp0, dp1]`.  TDup[label, body, k] uses an explicit label.";

(* === tag constants (mirror src/thvm.h) === *)
$TagAPP::usage = $TagLAM::usage = $TagVAR::usage = $TagERA::usage =
  $TagDP0::usage = $TagDP1::usage = $TagSUP::usage = $TagDUP::usage =
    "Tag id; mirrors the corresponding TAG_* in src/thvm.h.";

Begin["`Private`"];

$libDir = FileNameJoin[{
    DirectoryName[$InputFileName],
    "..", "LibraryResources", $SystemID
}];

$lib = FileNameJoin[{$libDir, "THVMLink" <> Switch[$OperatingSystem,
    "MacOSX", ".dylib", "Windows", ".dll", _, ".so"]}];

debugPrint[args___] := WriteString[$Output, StringJoin @@ Map[ToString, {args}], "\n"]

If[ ! FileExistsQ[$lib],
    debugPrint["[THVMLink] Library not built.  Run `make wl` from the repo root."];
    debugPrint["[THVMLink] Expected at: ", $lib]
];

(* Tag constants - keep in sync with src/thvm.h *)
$TagAPP = 0; $TagLAM = 1; $TagVAR = 2; $TagERA = 3;
$TagDP0 = 4; $TagDP1 = 5; $TagSUP = 6; $TagDUP = 7;

$tagNames = <|
    0 -> "APP", 1 -> "LAM", 2 -> "VAR", 3 -> "ERA",
    4 -> "DP0", 5 -> "DP1", 6 -> "SUP", 7 -> "DUP"
|>;

TTagName[t_Integer] := Lookup[$tagNames, t, "TAG?" <> ToString[t]]

(* === library function loaders === *)
load[name_String, args_, ret_] := LibraryFunctionLoad[$lib, name, args, ret]

$initFn      := $initFn      = load["thvm_wl_init",       {},                       Integer];
$freeFn      := $freeFn      = load["thvm_wl_free",       {},                       Integer];
$resetFn     := $resetFn     = load["thvm_wl_reset",      {},                       Integer];

$termNewFn   := $termNewFn   = load["thvm_wl_term_new",   {Integer, Integer, Integer, Integer}, Integer];
$termTagFn   := $termTagFn   = load["thvm_wl_term_tag",   {Integer},                Integer];
$termExtFn   := $termExtFn   = load["thvm_wl_term_ext",   {Integer},                Integer];
$termValFn   := $termValFn   = load["thvm_wl_term_val",   {Integer},                Integer];
$termSubFn   := $termSubFn   = load["thvm_wl_term_sub",   {Integer},                Integer];

$heapPosFn   := $heapPosFn   = load["thvm_wl_heap_pos",   {},                       Integer];
$heapAllocFn := $heapAllocFn = load["thvm_wl_heap_alloc", {Integer},                Integer];
$heapReadFn  := $heapReadFn  = load["thvm_wl_heap_read",  {Integer},                Integer];
$heapSetFn   := $heapSetFn   = load["thvm_wl_heap_set",   {Integer, Integer},       Integer];

$wnfFn       := $wnfFn       = load["thvm_wl_wnf",        {Integer},                Integer];
$itrsFn      := $itrsFn      = load["thvm_wl_itrs",       {},                       Integer];

(* === fresh-label counter (WL-side; reset by TReset) === *)
$labelCounter = 1;
TFreshLabel[] := Block[{n = $labelCounter}, $labelCounter += 1; n]

(* === public API === *)
TInit[]      := ($labelCounter = 1; $initFn[] === 1)
TFree[]      := $freeFn[]
TReset[]     := ($labelCounter = 1; $resetFn[])

TTermNew[sub_Integer, tag_Integer, ext_Integer, val_Integer] :=
    $termNewFn[sub, tag, ext, val]

TTermTag[t_Integer] := $termTagFn[t]
TTermExt[t_Integer] := $termExtFn[t]
TTermVal[t_Integer] := $termValFn[t]
TTermSub[t_Integer] := $termSubFn[t]

(* === atomic-object plumbing for THeap and TTermInfo ===
   QuantumFramework-style: a constructor (TTermInfo[t_Integer], THeap[])
   builds an atomic form (TTermInfo[<|...|>], THeap[<|...|>]) with a
   custom MakeBoxes UpValue defined in Format.wl.  Indexing forwards
   to the underlying Association so callers see the same access shape
   as before (snap["Graph"], info["tag"], etc.). *)

TTermInfo[t_Integer] := TTermInfo[<|
    "sub"     -> TTermSub[t],
    "tag"     -> TTermTag[t],
    "tagName" -> TTagName[TTermTag[t]],
    "ext"     -> TTermExt[t],
    "val"     -> TTermVal[t],
    "raw"     -> t
|>]

TTermInfo[a_Association][k_] := a[k]
TTermInfo /: KeyExistsQ[TTermInfo[a_Association], k_] := KeyExistsQ[a, k]
TTermInfo /: Keys[TTermInfo[a_Association]]          := Keys[a]
TTermInfo /: Values[TTermInfo[a_Association]]        := Values[a]
TTermInfo /: Normal[TTermInfo[a_Association]]        := a

THeapPos[]                       := $heapPosFn[]
THeapAlloc[size_Integer]         := $heapAllocFn[size]
THeapRead[loc_Integer]           := $heapReadFn[loc]
THeapSet[loc_Integer, t_Integer] := $heapSetFn[loc, t]

TWnf[t_Integer]  := $wnfFn[t]
TItrs[]          := $itrsFn[]

(* === high-level constructors === *)

heapWith[fields__] := With[{loc = THeapAlloc[Length[{fields}]]},
    ScanIndexed[THeapSet[loc + First[#2] - 1, #1] &, {fields}];
    loc
]

heapTerm[tag_Integer, ext_Integer, fields__] :=
    TTermNew[0, tag, ext, heapWith[fields]]

TEra[]                  := TTermNew[0, $TagERA, 0, 0]
TVarFor[lamLoc_Integer] := TTermNew[0, $TagVAR, 0, lamLoc]

TApp[fun_Integer, arg_Integer]            := heapTerm[$TagAPP, 0,     fun, arg]

TSup[a_Integer, b_Integer]                := TSup[TFreshLabel[], a, b]
TSup[label_Integer, a_Integer, b_Integer] := heapTerm[$TagSUP, label, a, b]

TLam[builder_] := With[{loc = THeapAlloc[1]},
    THeapSet[loc, builder[TVarFor[loc]]];
    TTermNew[0, $TagLAM, 0, loc]
]

TDup[body_Integer, k_]                        := TDup[TFreshLabel[], body, k]
TDup[label_Integer, body_Integer, k_] := With[{loc = heapWith[body]},
    k[TTermNew[0, $TagDP0, label, loc],
      TTermNew[0, $TagDP1, label, loc]]
]

(* === heap graph (IC string diagram) === *)
(* Per-tag port info: list of {offset, portName} for an agent with the
   given tag.  `cellEdges`'s single source of truth.  Adding a tag means
   adding one branch here. *)
agentPorts[$TagLAM] := {{0, "body"}}
agentPorts[$TagAPP] := {{0, "f"}, {1, "x"}}
agentPorts[$TagSUP] := {{0, "L"}, {1, "R"}}
agentPorts[$TagDUP] := {{0, "body"}}

agentVertexId[base_Integer] := "a" <> ToString[base]
eraVertexId[loc_Integer]    := "e" <> ToString[loc]

(* Inferred agent for one term value: returns a key->tag rule, or
   Nothing if the term doesn't imply an agent.  VAR cells imply a LAM
   at the binder loc; DP0/DP1 cells imply a DUP at the body loc. *)
agentFromTerm[term_Integer] := Switch[TTermTag[term],
    $TagLAM | $TagAPP | $TagSUP | $TagDUP, TTermVal[term] -> TTermTag[term],
    $TagVAR,                               TTermVal[term] -> $TagLAM,
    $TagDP0 | $TagDP1,                     TTermVal[term] -> $TagDUP,
    _,                                     Nothing
]

(* Walk every populated heap cell plus any seed terms.  Compound terms
   contribute their args base as an agent of that tag.  Compound term
   cells take precedence over inferred-from-VAR/DP entries thanks to
   Association merge (last write wins; we put compounds last). *)
discoverAgents[seedTerms_List : {}] := Block[{n = THeapPos[], terms, rules},
    terms = Join[seedTerms, Table[THeapRead[loc], {loc, 0, n - 1}]];
    rules = agentFromTerm /@ terms;
    Association[rules]
]

(* ERA cells: every loc whose stored term has tag ERA. *)
discoverEras[] := Block[{n = THeapPos[]},
    Select[Range[0, n - 1], TTermTag[THeapRead[#]] === $TagERA &]
]

(* For one port-slot of an agent at args base `base`, produce a triple
   {srcId, dstId, portLabel} based on what's stored in that slot. *)
portRecord[base_Integer, offset_Integer, port_String] :=
    Block[{loc = base + offset, t, tag, val},
        t   = THeapRead[loc];
        tag = TTermTag[t];
        val = TTermVal[t];
        Switch[tag,
            $TagLAM | $TagAPP | $TagSUP | $TagDUP,
                {agentVertexId[base], agentVertexId[val], port},
            $TagVAR,
                {agentVertexId[base], agentVertexId[val], port <> " var"},
            $TagDP0,
                {agentVertexId[base], agentVertexId[val], port <> " dp0"},
            $TagDP1,
                {agentVertexId[base], agentVertexId[val], port <> " dp1"},
            $TagERA,
                {agentVertexId[base], eraVertexId[loc], port},
            _, Nothing
        ]
    ]

agentEdgeRecords[base_Integer, tag_Integer] :=
    portRecord[base, #[[1]], #[[2]]] & /@ agentPorts[tag]

(* SUB-tagged cells get dashed outline; we mark the corresponding vertex. *)
subVerticesForAgent[base_Integer, tag_Integer] :=
    Module[{result = {}},
        Do[
            With[{loc = base + p[[1]]},
                If[ TTermSub[THeapRead[loc]] == 1,
                    AppendTo[result, agentVertexId[base]]]],
            {p, agentPorts[tag]}
        ];
        result
    ]

THeapGraph[]              := buildHeapGraph[discoverAgents[{}]]
THeapGraph[term_Integer]  := buildHeapGraph[discoverAgents[{term}]]
THeapGraph[ts_List]       := buildHeapGraph[discoverAgents[ts]]

buildHeapGraph[agents_Association] := Block[{
    eras = discoverEras[],
    edgeRecords, edges, edgeLabels, vertices, vlabels, subVertices, vstyles
},
    edgeRecords = Flatten[
        KeyValueMap[agentEdgeRecords, agents],
        1
    ];
    edges      = (DirectedEdge @@ Take[#, 2]) & /@ edgeRecords;
    edgeLabels = MapThread[Rule, {edges, Last /@ edgeRecords}];

    vertices = DeleteDuplicates @ Join[
        agentVertexId /@ Keys[agents],
        eraVertexId   /@ eras
    ];

    vlabels = Join[
        KeyValueMap[
            agentVertexId[#1] -> (TTagName[#2] <> "@" <> ToString[#1]) &,
            agents
        ],
        ((eraVertexId[#] -> "") &) /@ eras
    ];

    subVertices = Flatten[KeyValueMap[subVerticesForAgent, agents]];
    vstyles = Map[
        # -> If[MemberQ[subVertices, #],
            Directive[EdgeForm[Dashed], FaceForm[White]],
            Automatic] &,
        vertices
    ];

    Graph[vertices, edges,
        VertexLabels       -> Map[#[[1]] -> Placed[#[[2]], Center] &, vlabels],
        VertexLabelStyle   -> Directive[FontFamily -> "Helvetica", FontSize -> 10, Black],
        VertexSize         -> Map[# -> If[StringStartsQ[#, "e"], 0.08, 0.45] &, vertices],
        VertexShapeFunction -> Map[
            # -> If[StringStartsQ[#, "e"],
                Function[{pos, v, size}, {EdgeForm[], FaceForm[Black], Disk[pos, size / 2]}],
                Function[{pos, v, size}, {EdgeForm[Black], FaceForm[White], Disk[pos, size]}]
            ] &,
            vertices
        ],
        EdgeLabels         -> Map[#[[1]] -> Placed[#[[2]], 0.5] &, edgeLabels],
        EdgeLabelStyle     -> Directive[FontFamily -> "Helvetica", FontSize -> 9, Gray],
        VertexStyle        -> vstyles,
        DirectedEdges      -> True,
        GraphLayout        -> "LayeredDigraphEmbedding",
        PerformanceGoal    -> "Quality",
        ImagePadding       -> 30
    ]
]

THeap[] := Block[{n = THeapPos[]},
    THeap[<|
        "nextLoc" -> n,
        "cells"   -> Association @ Table[
            i -> TTermInfo[THeapRead[i]],
            {i, 0, n - 1}
        ],
        "Graph"   -> THeapGraph[]
    |>]
]

THeap[a_Association][k_] := a[k]
THeap /: KeyExistsQ[THeap[a_Association], k_] := KeyExistsQ[a, k]
THeap /: Keys[THeap[a_Association]]           := Keys[a]
THeap /: Values[THeap[a_Association]]         := Values[a]
THeap /: Normal[THeap[a_Association]]         := a

(* === formatting (summary boxes) === *)
Get[FileNameJoin[{DirectoryName[$InputFileName], "Format.wl"}]]

End[];
EndPackage[];
