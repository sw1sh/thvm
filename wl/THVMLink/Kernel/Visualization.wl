(* ::Package:: *)
(* Visualization.wl - heap-graph rendering for THVMLink.

   Lives in THVMLink`Private` (BeginPackage["THVMLink`"] + Begin["`Private`"]).
   Defines THeapGraph (public) plus all the per-tag styling helpers
   used to turn the heap state into a Wolfram Graph.

   Vertex identifier convention:  "<prefix><id>"
       a<base>    IC compound (LAM/APP/SUP/DUP)  at args base <base>
       e<loc>     ERA cell at heap loc <loc>
       u<loc>     TAG_UOP at heap loc <loc>
       t<id>      TAG_TEN at tensor id <id>

   Style follows wl/GUIDE.md: theme-aware Standard colors,
   LightDarkSwitched for fg/bg, real triangles for IC agents,
   labels drawn inside the vertex via Inset[Pane[...,
   ImageSizeAction -> "ShrinkToFit"]] so text auto-shrinks to fit.

   The agents Association is keyed by full vertex id strings; each
   value carries a small Association with at least "tag" + tag-specific
   fields (opcode for TAG_UOP, base/loc for IC, etc).
*)

BeginPackage["THVMLink`"];

Begin["`Private`"];

(* === per-tag ports + identifiers === *)

(* Vertex-id constructors -- each tag has its own prefix so vertices
   never collide across tags. *)
icVertexId [base_Integer]    := "a" <> ToString[base]
eraVertexId[loc_Integer]     := "e" <> ToString[loc]
uopVertexId[loc_Integer]     := "u" <> ToString[loc]
tenVertexId[id_Integer]      := "t" <> ToString[id]

(* IC agents have static port specs by tag.  Format: {offset, label}. *)
icPorts[$TagLAM] := {{0, "body"}}
icPorts[$TagAPP] := {{0, "f"}, {1, "x"}}
icPorts[$TagSUP] := {{0, "L"}, {1, "R"}}
icPorts[$TagDUP] := {{0, "body"}}

(* UOP compute-input arity comes from Uop.wl's `uopArity[op]`. *)
uopComputeArity[op_] := uopArity[op]

(* === discovery ===

   discoverAll[seedTerms] walks every heap cell + every seed term,
   produces an Association keyed by vertex-id string with values
   <|"tag" -> tag, ...info...|>.  Three categories:

     IC  : key = icVertexId[base];   info = {"tag" -> tag}
     UOP : key = uopVertexId[loc];   info = {"tag" -> $TagUOP, "opcode" -> ext}
     TEN : key = tenVertexId[id];    info = {"tag" -> $TagTEN, "dtype" -> ext}

   ERAs are tracked separately because they live at the *cell* level,
   not as a referenced agent. *)

agentFromTerm[term_] := With[{tag = TTermTag[term]},
    Switch[tag,
        $TagLAM | $TagAPP | $TagSUP | $TagDUP,
            icVertexId[TTermVal[term]] -> <|"tag" -> tag|>,
        $TagVAR,
            icVertexId[TTermVal[term]] -> <|"tag" -> $TagLAM|>,
        $TagDP0 | $TagDP1,
            icVertexId[TTermVal[term]] -> <|"tag" -> $TagDUP|>,
        $TagUOP,
            uopVertexId[TTermVal[term]] -> <|"tag" -> $TagUOP, "opcode" -> TTermExt[term]|>,
        $TagTEN,
            tenVertexId[TTermVal[term]] -> <|"tag" -> $TagTEN, "dtype" -> TTermExt[term]|>,
        _,
            Nothing
    ]
]

discoverAgents[seedTerms_List : {}] := Block[{n = THeapPos[], terms},
    terms = Join[seedTerms, Table[THeapRead[loc], {loc, 0, n - 1}]];
    Association[agentFromTerm /@ terms]
]

discoverEras[] := Block[{n = THeapPos[]},
    Select[Range[0, n - 1], TTermTag[THeapRead[#]] === $TagERA &]
]

(* === edge records === *)

(* Walk one IC agent's compute slots, emit edge records of the
   form {srcVertex, dstVertex, edgeLabel}. *)
icPortRecord[base_Integer, offset_Integer, port_String] :=
    Block[{loc = base + offset, t, tag, val},
        t   = THeapRead[loc];
        tag = TTermTag[t];
        val = TTermVal[t];
        Switch[tag,
            $TagLAM | $TagAPP | $TagSUP | $TagDUP,
                {icVertexId[base], icVertexId[val], port},
            $TagVAR,
                {icVertexId[base], icVertexId[val], port <> " var"},
            $TagDP0,
                {icVertexId[base], icVertexId[val], port <> " dp0"},
            $TagDP1,
                {icVertexId[base], icVertexId[val], port <> " dp1"},
            $TagERA,
                {icVertexId[base], eraVertexId[loc], port},
            _, Nothing
        ]
    ]

icEdgeRecords[base_Integer, tag_Integer] :=
    icPortRecord[base, #[[1]], #[[2]]] & /@ icPorts[tag]

(* Walk one UOP's compute slots.  Arguments after the sources
   (NUMs etc) stay implicit and are surfaced via the vertex label. *)
uopEdgeRecord[loc_Integer, offset_Integer] :=
    Block[{cellLoc = loc + offset, t, tag, val, ext},
        t = THeapRead[cellLoc];
        tag = TTermTag[t];
        val = TTermVal[t];
        ext = TTermExt[t];
        Switch[tag,
            $TagUOP,
                {uopVertexId[loc], uopVertexId[val], "src" <> ToString[offset]},
            $TagTEN,
                {uopVertexId[loc], tenVertexId[val], "src" <> ToString[offset]},
            $TagNUM,
                Nothing,    (* arguments are inlined into the label *)
            $TagLAM | $TagAPP | $TagSUP | $TagDUP,
                {uopVertexId[loc], icVertexId[val], "src" <> ToString[offset]},
            $TagERA,
                {uopVertexId[loc], eraVertexId[cellLoc], "src" <> ToString[offset]},
            _, Nothing
        ]
    ]

uopEdgeRecords[loc_Integer, opcode_Integer] :=
    Table[uopEdgeRecord[loc, off], {off, 0, uopComputeArity[opcode] - 1}]

(* dispatch for one agent entry *)
agentEdgeRecords[vid_String, info_Association] := Switch[info["tag"],
    $TagLAM | $TagAPP | $TagSUP | $TagDUP,
        icEdgeRecords[ToExpression[StringDrop[vid, 1]], info["tag"]],
    $TagUOP,
        uopEdgeRecords[ToExpression[StringDrop[vid, 1]], info["opcode"]],
    _, {}
]

(* === SUB-decoration (dashed outline if any port cell is SUB) === *)

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

(* === colours and shapes === *)

fgColor   := LightDarkSwitched[Black, White]
edgeColor := StandardBlue

agentFill[$TagLAM] := LightDarkSwitched[Lighter[StandardGreen,  0.55], Darker[StandardGreen,  0.45]]
agentFill[$TagAPP] := LightDarkSwitched[Lighter[StandardBlue,   0.55], Darker[StandardBlue,   0.45]]
agentFill[$TagSUP] := LightDarkSwitched[Lighter[StandardOrange, 0.55], Darker[StandardOrange, 0.45]]
agentFill[$TagDUP] := LightDarkSwitched[Lighter[StandardPurple, 0.55], Darker[StandardPurple, 0.45]]
agentFill[$TagUOP] := LightDarkSwitched[Lighter[StandardBlue,   0.55], Darker[StandardBlue,   0.45]]
agentFill[$TagTEN] := LightDarkSwitched[Lighter[StandardCyan,   0.55], Darker[StandardCyan,   0.45]]

(* triangle / rectangle / disk shape primitives *)
downTriShape[pos_, {hw_, hh_}] := Triangle[{
    pos + {-hw, hh}, pos + {hw, hh}, pos + {0, -hh}
}]
upTriShape[pos_, {hw_, hh_}] := Triangle[{
    pos + {-hw, -hh}, pos + {hw, -hh}, pos + {0, hh}
}]
boxShape[pos_, {hw_, hh_}] := Rectangle[pos - {hw, hh}, pos + {hw, hh}]

agentEdgeForm[isSub_] := EdgeForm[
    If[ isSub,
        Directive[fgColor, AbsoluteThickness[1.2], Dashed],
        Directive[fgColor, AbsoluteThickness[1.2]]
    ]
]

labelInside[label_, pos_, {hw_, hh_}] := Inset[
    Pane[
        Style[label, FontFamily -> "Helvetica", FontColor -> fgColor, Bold],
        {Round[160 hw], Round[140 hh]},
        ImageSizeAction -> "ShrinkToFit",
        Alignment -> Center
    ],
    pos,
    Center,
    {2 hw, 2 hh}
]

icShapeFn[tag_, isSub_, label_] := With[{
    ef = agentEdgeForm[isSub], fc = FaceForm[agentFill[tag]],
    shape = If[ MemberQ[{$TagLAM, $TagDUP}, tag], downTriShape, upTriShape]
},
    Function[{pos, v, size},
        {ef, fc, shape[pos, size], labelInside[label, pos, size]}]
]

uopShapeFn[label_] := With[{
    ef = agentEdgeForm[False], fc = FaceForm[agentFill[$TagUOP]]
},
    Function[{pos, v, size},
        {ef, fc, boxShape[pos, size], labelInside[label, pos, size]}]
]

tenShapeFn[label_] := With[{
    ef = agentEdgeForm[False], fc = FaceForm[agentFill[$TagTEN]]
},
    Function[{pos, v, size},
        {ef, fc, boxShape[pos, size], labelInside[label, pos, size]}]
]

eraShapeFn := Function[{pos, v, size},
    {fgColor, AbsoluteThickness[1.2], Circle[pos, Min[size]]}]

(* === labels === *)

icLabel[base_Integer, tag_Integer] := With[{arity = Length[icPorts[tag]]},
    Column[
        {
            TTagName[tag],
            If[ arity > 1,
                "@" <> ToString[base] <> ".." <> ToString[base + arity - 1],
                "@" <> ToString[base]
            ]
        },
        Center,
        Spacings -> 0
    ]
]

(* For UOP, label = opcode name + "@<loc>". *)
uopLabel[loc_Integer, opcode_Integer] :=
    Column[{uopName[opcode], "@" <> ToString[loc]}, Center, Spacings -> 0]

(* TEN label: dtype + tensor id. *)
tenLabel[id_Integer, dtype_Integer] := Column[{
    "TEN",
    "#" <> ToString[id]
}, Center, Spacings -> 0]

(* dispatch *)
agentLabelFor[vid_String, info_Association] := Switch[info["tag"],
    $TagLAM | $TagAPP | $TagSUP | $TagDUP,
        icLabel[ToExpression[StringDrop[vid, 1]], info["tag"]],
    $TagUOP,
        uopLabel[ToExpression[StringDrop[vid, 1]], info["opcode"]],
    $TagTEN,
        tenLabel[ToExpression[StringDrop[vid, 1]], info["dtype"]],
    _, ""
]

agentShapeFor[vid_String, info_Association, isSub_, label_] := Switch[info["tag"],
    $TagLAM | $TagAPP | $TagSUP | $TagDUP,
        icShapeFn[info["tag"], isSub, label],
    $TagUOP,
        uopShapeFn[label],
    $TagTEN,
        tenShapeFn[label],
    _, eraShapeFn
]

(* === DiagramNetwork export ===
   Implementation lives in Diagram.wl (own subcontext THVMLink`Diagram`). *)

(* === main entry point === *)

Options[THeapGraph] = {
    GraphLayout      -> Automatic,
    ImageSize        -> Automatic,
    VertexSize       -> Automatic,
    PlotRange        -> Automatic,
    PlotRangePadding -> Scaled[0.15],
    Background       -> Automatic
}

THeapGraph[opts : OptionsPattern[]] :=
    buildHeapGraph[discoverAgents[{}], opts]
THeapGraph[ts_List, opts : OptionsPattern[]] :=
    buildHeapGraph[discoverAgents[ts], opts]
THeapGraph[term_, opts : OptionsPattern[]] :=
    buildHeapGraph[discoverAgents[{term}], opts]

buildHeapGraph[agents_Association, userOpts : OptionsPattern[THeapGraph]] := Block[{
    eras = discoverEras[],
    edgeRecords, edges, edgeLabels, vertices, vshapes, subVertices,
    layout
},
    edgeRecords = Flatten[KeyValueMap[agentEdgeRecords, agents], 1];
    edges       = (DirectedEdge @@ Take[#, 2]) & /@ edgeRecords;
    edgeLabels  = MapThread[Rule, {edges, Last /@ edgeRecords}];

    vertices = DeleteDuplicates @ Join[
        Keys[agents],
        eraVertexId /@ eras
    ];

    subVertices = Flatten[KeyValueMap[subVerticesForAgent, agents]];

    vshapes = Join[
        KeyValueMap[
            Function[{vid, info},
                vid -> agentShapeFor[vid, info,
                    MemberQ[subVertices, vid],
                    agentLabelFor[vid, info]
                ]
            ],
            agents
        ],
        ((eraVertexId[#] -> eraShapeFn) &) /@ eras
    ];

    layout = Replace[OptionValue[GraphLayout], Automatic -> "LayeredDigraphEmbedding"];

    With[{
        defaultVSize = If[ Length[vertices] === 1,
            Map[# -> If[ StringStartsQ[#, "e"], 0.18, 0.45] &, vertices],
            Map[# -> If[ StringStartsQ[#, "e"], 0.10, 0.30] &, vertices]
        ],
        defaultPlotRange = If[ Length[vertices] === 1,
            {{-1, 1}, {-1, 1}},
            All
        ]
    },
        Graph[vertices, edges,
            Sequence @@ FilterRules[{userOpts},
                Except[GraphLayout | VertexSize | PlotRange]],
            VertexShapeFunction -> vshapes,
            VertexSize          -> Replace[
                OptionValue[VertexSize], Automatic -> defaultVSize],
            EdgeLabels          -> Map[#[[1]] -> Placed[#[[2]], 0.5] &, edgeLabels],
            EdgeLabelStyle      -> Directive[FontFamily -> "Helvetica", FontSize -> 9, fgColor],
            EdgeStyle           -> Directive[edgeColor, AbsoluteThickness[1.5]],
            DirectedEdges       -> True,
            GraphLayout         -> layout,
            PlotRange           -> Replace[
                OptionValue[PlotRange], Automatic -> defaultPlotRange],
            PerformanceGoal     -> "Quality",
            ImagePadding        -> 25
        ]
    ]
]

End[];

EndPackage[];
