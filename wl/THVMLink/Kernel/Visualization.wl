(* ::Package:: *)
(* Visualization.wl - heap-graph rendering for THVMLink.

   Public:
     THeapGraph[]                 walk every heap cell, render
     THeapGraph[term]             include `term` in seed walk
     THeapGraph[{t1, t2, ...}]    multiple seeds

   Style: NO local color tables.  Every node category goes through
   $nodeStyle / drawNode in Style.wl so THeapGraph, TScheduleGraph,
   THeapDiagram and the gantt all paint a "KERNEL" or a "TEN" the
   same way.  Edge direction is *data flow*: a UOP that reads from
   a TEN draws TEN -> UOP (sources point at the consumer).  This is
   the opposite of the heap-pointer direction (UOP cell holds a
   pointer to its source) but matches what readers want -- the
   arrow shows where bytes move at fire time.

   KERNEL-aware: a UOP_KERNEL cell's compute inputs aren't in heap
   children (they live in C-side KernelEntry.input_tids[]).  We
   pull them via TKernelInputs[kid] and emit edges from each input
   tid's vertex (a TEN, possibly aliasing another kernel's output)
   to the kernel.  Same convention as TScheduleGraph.

   Vertex id convention:
       a<base>    IC compound (LAM/APP/SUP/DUP)  at args base <base>
       e<loc>     ERA cell at heap loc <loc>
       u<loc>     non-KERNEL TAG_UOP at heap loc <loc>
       k<loc>     UOP_KERNEL cell at heap loc <loc>
       t<id>      TAG_TEN tensor handle for tid <id>           *)

BeginPackage["THVMLink`"];

THeapGraph::usage = "THeapGraph[] / THeapGraph[term] / THeapGraph[{t1, t2, ...}] renders the heap as a Graph: every IC agent + UOP cell + TEN handle becomes a vertex, edges follow *data flow* (sources point at consumers).  UOP_KERNEL cells expose their TKernelInputs[] as input edges so a fused-graph view shows the same kernel-DAG topology as TScheduleGraph.  Style comes from $nodeStyle in Style.wl -- consistent with every other renderer.  Options: \"ShowEdgeLabels\" -> False (default; labels hidden so dense graphs read clean), plus all standard Graph options.";

TScheduleGraph::usage = "TScheduleGraph[] returns a Graph of the live kernel schedule: one vertex per emitted kernel, directed edges from producer kernel to consumer kernel labeled by the connecting TenDesc id.  External inputs (TenDescs with no producer kernel -- weights, host tensors) appear as cyan TEN-shaped vertices when \"ShowExternalInputs\" -> True (default).  Disconnected kernels render as isolated vertices.  Accepts all standard Graph options.";

Begin["`Private`"];

(* Forward refs to private symbols owned by Style.wl + Kernel.wl;
   they share the THVMLink`Private` context but the alphabetical
   load order means they may not exist yet at first parse. *)
{drawNode, nodeShapeFn, edgeStyleDirective, styleFor};

(* === vertex id constructors === *)
icVertexId [base_Integer] := "a" <> ToString[base]
eraVertexId[loc_Integer]  := "e" <> ToString[loc]
uopVertexId[loc_Integer]  := "u" <> ToString[loc]
kerVertexId[loc_Integer]  := "k" <> ToString[loc]
tenVertexId[id_Integer]   := "t" <> ToString[id]

(* === IC port specs (offset, port name) === *)
icPorts[$TagLAM] := {{0, "body"}}
icPorts[$TagAPP] := {{0, "f"}, {1, "x"}}
icPorts[$TagSUP] := {{0, "L"}, {1, "R"}}
icPorts[$TagDUP] := {{0, "body"}}

(* === discovery ===
   Walks every heap cell + each seed term once; categorizes each
   referenced term into one of four buckets (IC, UOP, KERNEL, TEN)
   keyed by the convention above.  We map UOP_KERNEL to the "k"
   prefix so it's distinguishable from a generic UOP at the
   styling layer without re-inspecting the heap. *)

agentFromTerm[term_] := With[{tag = TTermTag[term]},
    Switch[tag,
        $TagLAM | $TagAPP | $TagSUP | $TagDUP,
            icVertexId[TTermVal[term]] -> <|"tag" -> tag|>,
        $TagVAR,
            icVertexId[TTermVal[term]] -> <|"tag" -> $TagLAM|>,
        $TagDP0 | $TagDP1,
            icVertexId[TTermVal[term]] -> <|"tag" -> $TagDUP|>,
        $TagUOP,
            If[ TTermExt[term] === $UopKernel,
                kerVertexId[TTermVal[term]] -> <|"tag" -> $TagUOP, "opcode" -> $UopKernel|>,
                uopVertexId[TTermVal[term]] -> <|"tag" -> $TagUOP, "opcode" -> TTermExt[term]|>],
        $TagTEN,
            tenVertexId[TTermVal[term]] -> <|"tag" -> $TagTEN, "dtype" -> TTermExt[term]|>,
        _,
            Nothing
    ]
]

discoverAgents[seedTerms_List : {}] := Block[{lo = THeapBase[], n = THeapPos[], terms},
    terms = Join[seedTerms, Table[THeapRead[loc], {loc, lo, n - 1}]];
    Association[agentFromTerm /@ terms]
]

discoverEras[] := Block[{lo = THeapBase[], n = THeapPos[]},
    Select[Range[lo, n - 1], TTermTag[THeapRead[#]] === $TagERA &]
]

(* === edge records === *)
(* Direction is DATA FLOW: source -> consumer.  Reversed from the
   heap-pointer convention (a parent cell holding a pointer to its
   child).  Per-tag walkers emit {sourceVertex, consumerVertex}
   pairs.  Edge labels (port names, src offsets) get rendered only
   if the caller asks via "ShowEdgeLabels" -> True. *)

icPortRecord[base_Integer, offset_Integer, port_String] :=
    Block[{loc = base + offset, t, tag, val},
        t   = THeapRead[loc];
        tag = TTermTag[t];
        val = TTermVal[t];
        Switch[tag,
            $TagLAM | $TagAPP | $TagSUP | $TagDUP,
                {icVertexId[val], icVertexId[base], port},
            $TagVAR,
                {icVertexId[val], icVertexId[base], port <> " var"},
            $TagDP0,
                {icVertexId[val], icVertexId[base], port <> " dp0"},
            $TagDP1,
                {icVertexId[val], icVertexId[base], port <> " dp1"},
            $TagERA,
                {eraVertexId[loc], icVertexId[base], port},
            _, Nothing
        ]
    ]

icEdgeRecords[base_Integer, tag_Integer] :=
    icPortRecord[base, #[[1]], #[[2]]] & /@ icPorts[tag]

(* For non-KERNEL UOPs, walk their compute slots and emit
   src -> uop edges. *)
uopEdgeRecord[loc_Integer, offset_Integer] :=
    Block[{cellLoc = loc + offset, t, tag, val},
        t = THeapRead[cellLoc];
        tag = TTermTag[t];
        val = TTermVal[t];
        Switch[tag,
            $TagUOP,
                {If[ TTermExt[t] === $UopKernel,
                     kerVertexId[val], uopVertexId[val]],
                 uopVertexId[loc], "src" <> ToString[offset]},
            $TagTEN,
                {tenVertexId[val], uopVertexId[loc], "src" <> ToString[offset]},
            $TagNUM,
                Nothing,    (* numeric arg, surfaced via vertex label *)
            $TagLAM | $TagAPP | $TagSUP | $TagDUP,
                {icVertexId[val], uopVertexId[loc], "src" <> ToString[offset]},
            $TagERA,
                {eraVertexId[cellLoc], uopVertexId[loc], "src" <> ToString[offset]},
            _, Nothing
        ]
    ]

uopEdgeRecords[loc_Integer, opcode_Integer] /; opcode =!= $UopKernel :=
    Table[uopEdgeRecord[loc, off], {off, 0, uopArity[opcode] - 1}]

(* For UOP_KERNEL cells, the heap children (output_buf TEN, NUM kid)
   describe the OUTPUT, not inputs.  Pull TKernelInputs[kid] from
   the C side and emit one TEN -> KERNEL edge per input tid.  Same
   topology TScheduleGraph builds. *)
kernelEdgeRecords[loc_Integer] := Block[{
    kid     = TTermVal[THeapRead[loc + 1]],
    outTid  = TTermVal[THeapRead[loc]],
    consVid = kerVertexId[loc],
    inputs
},
    inputs = TKernelInputs[kid];
    Append[
        ({tenVertexId[#], consVid, "in"} & /@ inputs),
        (* output edge: KERNEL -> output TEN, so the TEN appears
           downstream of the kernel that produces it. *)
        {consVid, tenVertexId[outTid], "out"}
    ]
]

uopEdgeRecords[loc_Integer, $UopKernel] := kernelEdgeRecords[loc]

(* dispatch *)
agentEdgeRecords[vid_String, info_Association] := Switch[info["tag"],
    $TagLAM | $TagAPP | $TagSUP | $TagDUP,
        icEdgeRecords[ToExpression[StringDrop[vid, 1]], info["tag"]],
    $TagUOP,
        uopEdgeRecords[ToExpression[StringDrop[vid, 1]], info["opcode"]],
    _, {}
]

(* === SUB-decoration (dashed outline) === *)
icSubVertices[base_Integer, tag_Integer] :=
    Module[{result = {}},
        Do[
            With[{loc = base + p[[1]]},
                If[ TTermSub[THeapRead[loc]] == 1,
                    AppendTo[result, icVertexId[base]]]],
            {p, icPorts[tag]}
        ];
        result
    ]

subVerticesForAgent[vid_String, info_Association] := Switch[info["tag"],
    $TagLAM | $TagAPP | $TagSUP | $TagDUP,
        icSubVertices[ToExpression[StringDrop[vid, 1]], info["tag"]],
    _, {}
]

(* === labels === *)

icLabel[base_Integer, tag_Integer] := With[{arity = Length[icPorts[tag]]},
    Column[
        {
            TTagName[tag],
            If[ arity > 1,
                "@" <> ToString[base] <> ".." <> ToString[base + arity - 1],
                "@" <> ToString[base]
            ]
        }, Center, Spacings -> 0]
]

(* For KERNEL: surface kid + program op count for at-a-glance read.
   uopHeader (defined in Diagram.wl) returns "KERNEL@<base>#<kid>"
   already, but kid alone is more useful in this view. *)
kernelLabel[loc_Integer] := Block[{kid = TTermVal[THeapRead[loc + 1]]},
    "KERNEL\nk" <> ToString[kid]
]

(* CONST: surface decoded scalar value via scalarTextFromCell from
   Shape.wl, so "CONST" reads as "1.0" / "0" instead of an opaque
   loc.  Many CONST agents in a post-grad heap aren't redundant --
   each is a fresh expand_to_target leaf for chain-rule scalars
   (1.0, 2.0, 0.5 etc.); they're orphaned post-fusion because the
   sink kernel reads the materialized buffers, not the CONST UOPs. *)
uopLabel[loc_Integer, opcode_Integer] := Switch[opcode,
    $UopKernel, kernelLabel[loc],
    $UopConst,
        With[{txt = scalarTextFromCell[THeapRead[loc]]},
            "CONST\n" <> If[StringQ[txt] && txt =!= "", txt, "?"]],
    _, uopHeader[loc, opcode]
]

tenLabel[id_Integer, dtype_Integer] := "t" <> ToString[id]

(* === per-vertex (style category, label) ===
   Picks a $nodeStyle key + label.  An "ExternalTEN" is a TEN whose
   producer_kid in TENS table is 0 -- a host-side input not produced
   by any kernel; we render it darker so the eye picks weights /
   inputs out of a sea of intermediate TENs. *)
$externalTids := Block[{tens = TTensTable[]},
    Select[Range[Length[tens]], tens[[#, 1]] === 0 &]
]

vertexCategory[vid_String, info_Association] := Switch[info["tag"],
    $TagLAM, "LAM",
    $TagAPP, "APP",
    $TagSUP, "SUP",
    $TagDUP, "DUP",
    $TagUOP,
        Switch[info["opcode"],
            $UopKernel, "KERNEL",
            $UopConst,  "CONST",
            $UopReduce, "REDUCE",
            $UopGrad,   "GRAD",
            _,          "UOP"],
    $TagTEN,
        With[{id = ToExpression[StringDrop[vid, 1]]},
            If[ MemberQ[$externalTids, id], "ExternalTEN", "TEN"]],
    _, "UOP"
]

vertexLabel[vid_String, info_Association] := Switch[info["tag"],
    $TagLAM | $TagAPP | $TagSUP | $TagDUP,
        icLabel[ToExpression[StringDrop[vid, 1]], info["tag"]],
    $TagUOP,
        uopLabel[ToExpression[StringDrop[vid, 1]], info["opcode"]],
    $TagTEN,
        tenLabel[ToExpression[StringDrop[vid, 1]], info["dtype"]],
    _, ""
]

(* === public entry === *)

Options[THeapGraph] = Join[
    {"ShowEdgeLabels" -> False},
    Options[Graph]
];

THeapGraph[opts : OptionsPattern[]] :=
    buildHeapGraph[discoverAgents[{}], opts]
THeapGraph[ts_List, opts : OptionsPattern[]] :=
    buildHeapGraph[discoverAgents[ts], opts]
THeapGraph[term_, opts : OptionsPattern[]] :=
    buildHeapGraph[discoverAgents[{term}], opts]

buildHeapGraph[agents_Association, userOpts : OptionsPattern[THeapGraph]] := Block[{
    eras = discoverEras[],
    edgeRecords, edges, edgeLabels, vertices, vshapes, subVertices,
    layout, showLabels
},
    showLabels  = OptionValue["ShowEdgeLabels"];
    edgeRecords = Flatten[KeyValueMap[agentEdgeRecords, agents], 1];
    (* Tag each edge with its port name so DirectedEdge[a, b, "src0"]
       and DirectedEdge[a, b, "src1"] are distinct -- a UOP reading
       the same TEN through two slots gets two parallel arrows.  The
       caller can hide labels via "ShowEdgeLabels" -> False (default)
       but the tag still keeps Graph from collapsing the edges. *)
    edges       = DirectedEdge[#[[1]], #[[2]], #[[3]]] & /@ edgeRecords;
    edgeLabels  = MapThread[Rule, {edges, Last /@ edgeRecords}];

    vertices = DeleteDuplicates @ Join[
        Keys[agents],
        eraVertexId /@ eras,
        (* Surface every vertex referenced as an edge source or
           target -- a kernel input may name a TenDesc whose
           TAG_TEN doesn't appear in any walked heap cell (the
           original referencing UOP got rewritten to a kernel and
           is now orphaned).  Without this, the Graph would
           silently drop edges to those tids. *)
        edges /. DirectedEdge[a_, b_, ___] :> Sequence[a, b]
    ];

    subVertices = Flatten[KeyValueMap[subVerticesForAgent, agents]];

    (* Build a (vid -> info) map that includes the synthesized
       TEN vertices we just added via edge endpoints. *)
    Module[{augmented = agents},
        Do[
            If[ ! KeyExistsQ[augmented, v] && StringStartsQ[v, "t"],
                augmented[v] = <|"tag" -> $TagTEN, "dtype" -> 0|>],
            {v, vertices}
        ];
        vshapes = Join[
            KeyValueMap[
                Function[{vid, info},
                    vid -> nodeShapeFn[
                        vertexCategory[vid, info],
                        vertexLabel[vid, info]
                    ]
                ],
                augmented
            ],
            (eraVertexId[#] -> nodeShapeFn["ERA", ""]) & /@ eras
        ]
    ];

    layout = Replace[OptionValue[GraphLayout], Automatic -> "LayeredDigraphEmbedding"];

    Graph[vertices, edges,
        Sequence @@ FilterRules[{userOpts},
            Except[GraphLayout | VertexSize | "ShowEdgeLabels"]],
        VertexShapeFunction -> vshapes,
        VertexSize          -> 0.45,
        EdgeLabels          -> If[ showLabels,
            Map[#[[1]] -> Placed[#[[2]], 0.5] &, edgeLabels],
            None
        ],
        EdgeLabelStyle      -> Directive[FontFamily -> "Helvetica", FontSize -> 9,
                                         LightDarkSwitched[Black, White]],
        EdgeStyle           -> edgeStyleDirective,
        GraphLayout         -> layout,
        PerformanceGoal     -> "Quality",
        ImagePadding        -> 25
    ]
]

(* === TScheduleGraph -- DAG-of-kernels view ===

   Builds straight from the C-side KERNELS / TENS tables (no heap
   walk).  For each emitted kernel kid in 1..KERNELS_NEXT-1:
     - vertex "k<kid>" with kernel-shaped style (orange rounded
       rectangle, label = kid + program op count + output shape)
     - one directed edge per input_tid: source = the producer
       kernel "k<producer_kid>" if any (=> kernel-to-kernel
       dependency), else "t<tid>" (an external TEN leaf, only
       added when "ShowExternalInputs" -> True)
     - edge label = tid

   Disconnected kernels (no producer or consumer) render as
   isolated vertices in the layered embedding -- they're still
   part of the schedule, they just don't share TenDescs with the
   rest. *)

(* Forward refs to private symbols owned by Kernel.wl + Style.wl;
   they share THVMLink`Private` with this file so resolution is
   load-order-agnostic via SetDelayed. *)
{decodeKernelInfo, kernelRowAsoc, tenTermFromTid};

(* Compose a short per-kernel label.  Two lines:
     kid, op count
     output shape and dtype (or "no out" for the rare bail). *)
scheduleKernelLabel[kid_Integer] := Block[{
    info = decodeKernelInfo[kid],
    row  = kernelRowAsoc[kid],
    shape
},
    shape = With[{tid = row["OutputTid"]},
        If[ tid > 0, TTensorShape[tenTermFromTid[tid]], {}]];
    "k" <> ToString[kid] <> " (" <> ToString[info[[1]]["OpCount"]] <> "ops)"
        <> "\n" <> ToString[shape] <> " " <> row["OutputDtype"]
]

scheduleExternalLabel[tid_Integer] := Block[{
    shape = TTensorShape[tenTermFromTid[tid]],
    dtype = TTensorDType[tenTermFromTid[tid]]
},
    "t" <> ToString[tid] <> "\n"
        <> ToString[shape] <> " "
        <> If[StringQ[dtype], dtype, "?"]
]

Options[TScheduleGraph] = Join[
    {"ShowExternalInputs" -> True},
    Options[Graph]
];

TScheduleGraph[opts : OptionsPattern[]] := Block[{
    nKernels = TKernelCount[] - 1,
    showExt = OptionValue["ShowExternalInputs"],
    tens, kernelVerts, edges, edgeLabels, extTids, extVerts,
    vshapes
},
    If[ nKernels <= 0, Return[Graph[{}, ImageSize -> 240]]];
    tens = TTensTable[];

    kernelVerts = Table["k" <> ToString[k], {k, nKernels}];

    (* For each kernel, walk its input_tids and emit one edge per
       tid back to the producing kernel (or to an external-TEN
       vertex if there's no producer).  Edge label is the tid. *)
    {edges, edgeLabels, extTids} = Block[{es = {}, ls = {}, exs = {}},
        Do[
            Block[{
                inTids = TKernelInputs[kid],
                consumerVert = "k" <> ToString[kid],
                producerKid, srcVert
            },
                Do[
                    producerKid = If[ tid > 0 && tid <= Length[tens],
                                      tens[[tid, 1]], 0];
                    srcVert = If[ producerKid =!= 0,
                                  "k" <> ToString[producerKid],
                                  "t" <> ToString[tid]];
                    If[ producerKid === 0, AppendTo[exs, tid]];
                    AppendTo[es, DirectedEdge[srcVert, consumerVert]];
                    AppendTo[ls, DirectedEdge[srcVert, consumerVert] -> tid],
                    {tid, inTids}
                ]
            ],
            {kid, nKernels}
        ];
        {es, ls, DeleteDuplicates[exs]}
    ];

    extVerts = If[ showExt, "t" <> ToString[#] & /@ extTids, {}];
    (* Drop edges pointing into hidden external vertices, otherwise
       Graph would auto-add stub vertices for them. *)
    If[ !showExt,
        edges = Select[edges, !StringStartsQ[#[[1]], "t"] &];
        edgeLabels = Select[edgeLabels, !StringStartsQ[#[[1, 1]], "t"] &]
    ];

    vshapes = Join[
        Table[
            With[{vid = "k" <> ToString[k], lab = scheduleKernelLabel[k]},
                vid -> nodeShapeFn["KERNEL", lab]],
            {k, nKernels}
        ],
        If[ showExt,
            Table[
                With[{vid = "t" <> ToString[t], lab = scheduleExternalLabel[t]},
                    vid -> nodeShapeFn["ExternalTEN", lab]],
                {t, extTids}
            ],
            {}
        ]
    ];

    Graph[
        Join[kernelVerts, extVerts],
        edges,
        FilterRules[{opts}, Options[Graph]],
        VertexShapeFunction -> vshapes,
        VertexSize          -> 0.45,
        EdgeLabelStyle      -> Directive[FontFamily -> "Helvetica", FontSize -> 9,
                                         LightDarkSwitched[Black, White]],
        EdgeStyle           -> edgeStyleDirective,
        GraphLayout         -> "LayeredDigraphEmbedding",
        PerformanceGoal     -> "Quality"
    ]
]

End[];

EndPackage[];
