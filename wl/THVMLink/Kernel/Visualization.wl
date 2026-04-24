(* ::Package:: *)
(* Visualization.wl - heap-graph rendering for THVMLink.

   Loaded from THVMLink.wl after the public symbols are declared.
   Defines THeapGraph (public) plus all the per-tag styling helpers
   used to turn the heap state into a Wolfram Graph.

   Style follows wl/GUIDE.md: theme-aware Standard colors,
   LightDarkSwitched for fg/bg, real triangles for IC agents.
*)

(* === per-tag ports + identifiers === *)

agentPorts[$TagLAM] := {{0, "body"}}
agentPorts[$TagAPP] := {{0, "f"}, {1, "x"}}
agentPorts[$TagSUP] := {{0, "L"}, {1, "R"}}
agentPorts[$TagDUP] := {{0, "body"}}

agentVertexId[base_Integer] := "a" <> ToString[base]
eraVertexId[loc_Integer]    := "e" <> ToString[loc]

(* === agent / ERA discovery === *)

(* Accepts either a TTerm or a raw Integer; TTermTag / TTermVal handle both. *)
agentFromTerm[term_] := Switch[TTermTag[term],
    $TagLAM | $TagAPP | $TagSUP | $TagDUP, TTermVal[term] -> TTermTag[term],
    $TagVAR,                               TTermVal[term] -> $TagLAM,
    $TagDP0 | $TagDP1,                     TTermVal[term] -> $TagDUP,
    _,                                     Nothing
]

discoverAgents[seedTerms_List : {}] := Block[{n = THeapPos[], terms, rules},
    terms = Join[seedTerms, Table[THeapRead[loc], {loc, 0, n - 1}]];
    rules = agentFromTerm /@ terms;
    Association[rules]
]

discoverEras[] := Block[{n = THeapPos[]},
    Select[Range[0, n - 1], TTermTag[THeapRead[#]] === $TagERA &]
]

(* === edge records === *)

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

(* === colours and shapes === *)

(* Foreground (outlines, label text, ERA stroke) contrasts with the
   background.  Edge colour is the StandardBlue brand colour so it
   stands out against a tinted vertex. *)
fgColor   := LightDarkSwitched[Black, White]
edgeColor := StandardBlue

(* Per-tag fill colour.  Light-mode uses the Standard* tints directly;
   dark-mode keeps the same hue family but darkens to stay readable
   under the white label text. *)
agentFill[$TagLAM] := LightDarkSwitched[Lighter[StandardGreen,  0.55], Darker[StandardGreen,  0.45]]
agentFill[$TagAPP] := LightDarkSwitched[Lighter[StandardBlue,   0.55], Darker[StandardBlue,   0.45]]
agentFill[$TagSUP] := LightDarkSwitched[Lighter[StandardOrange, 0.55], Darker[StandardOrange, 0.45]]
agentFill[$TagDUP] := LightDarkSwitched[Lighter[StandardPurple, 0.55], Darker[StandardPurple, 0.45]]

(* `size` from a VertexShapeFunction is `{halfWidth, halfHeight}` --
   honor it so VertexSize -> Tiny / Small / Large / Scaled[...] all
   work, plus the per-vertex overrides set in buildHeapGraph. *)

(* LAM, DUP: triangle with apex at the bottom (binder hangs down). *)
downTriShape[pos_, {hw_, hh_}] := Triangle[{
    pos + {-hw, hh}, pos + {hw, hh}, pos + {0, -hh}
}]

(* APP, SUP: triangle with apex at the top (principal port at top). *)
upTriShape[pos_, {hw_, hh_}] := Triangle[{
    pos + {-hw, -hh}, pos + {hw, -hh}, pos + {0, hh}
}]

agentEdgeForm[isSub_] := EdgeForm[
    If[ isSub,
        Directive[fgColor, AbsoluteThickness[1.2], Dashed],
        Directive[fgColor, AbsoluteThickness[1.2]]
    ]
]

agentShapeFn[tag_, isSub_] := With[{
    ef = agentEdgeForm[isSub], fc = FaceForm[agentFill[tag]],
    shape = If[ MemberQ[{$TagLAM, $TagDUP}, tag], downTriShape, upTriShape]
},
    Function[{pos, v, size}, {ef, fc, shape[pos, size]}]
]

(* Circle is a stroked primitive: stroke colour is set with a plain
   colour directive, not via EdgeForm.  Use the smaller of the two
   half-extents as the radius so the circle stays round under any
   VertexSize. *)
eraShapeFn := Function[{pos, v, size},
    {fgColor, AbsoluteThickness[1.2], Circle[pos, Min[size]]}]

(* Multi-line "TAG\n@<base>" or "TAG\n@<base>..<base+arity-1>" label. *)
agentLabel[base_Integer, tag_Integer] := With[{arity = Length[agentPorts[tag]]},
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

(* === main entry point === *)

THeapGraph[]                := buildHeapGraph[discoverAgents[{}]]
THeapGraph[ts_List]         := buildHeapGraph[discoverAgents[ts]]
THeapGraph[term_]           := buildHeapGraph[discoverAgents[{term}]]

buildHeapGraph[agents_Association] := Block[{
    eras = discoverEras[],
    edgeRecords, edges, edgeLabels, vertices, vlabels, subVertices, vshapes
},
    edgeRecords = Flatten[KeyValueMap[agentEdgeRecords, agents], 1];
    edges       = (DirectedEdge @@ Take[#, 2]) & /@ edgeRecords;
    edgeLabels  = MapThread[Rule, {edges, Last /@ edgeRecords}];

    vertices = DeleteDuplicates @ Join[
        agentVertexId /@ Keys[agents],
        eraVertexId   /@ eras
    ];

    vlabels = Join[
        KeyValueMap[agentVertexId[#1] -> agentLabel[#1, #2] &, agents],
        ((eraVertexId[#] -> "") &) /@ eras
    ];

    subVertices = Flatten[KeyValueMap[subVerticesForAgent, agents]];

    vshapes = Join[
        KeyValueMap[
            agentVertexId[#1] -> agentShapeFn[#2,
                MemberQ[subVertices, agentVertexId[#1]]] &,
            agents
        ],
        ((eraVertexId[#] -> eraShapeFn) &) /@ eras
    ];

    Graph[vertices, edges,
        VertexLabels        -> Map[#[[1]] -> Placed[#[[2]], Center] &, vlabels],
        VertexLabelStyle    -> Directive[FontFamily -> "Helvetica", FontSize -> 9, fgColor],
        VertexSize          -> Map[# -> If[ StringStartsQ[#, "e"], 0.10, 0.30] &, vertices],
        VertexShapeFunction -> vshapes,
        EdgeLabels          -> Map[#[[1]] -> Placed[#[[2]], 0.5] &, edgeLabels],
        EdgeLabelStyle      -> Directive[FontFamily -> "Helvetica", FontSize -> 8, fgColor],
        EdgeStyle           -> Directive[edgeColor, AbsoluteThickness[1.5]],
        EdgeShapeFunction   -> singleVertexLoopFn[vertices, edges, edgeColor],
        DirectedEdges       -> True,
        GraphLayout         -> If[ Length[vertices] <= 1,
                                   "SpringEmbedding",
                                   "LayeredDigraphEmbedding"],
        VertexCoordinates   -> If[ Length[vertices] <= 1, {{0, 0}}, Automatic],
        PlotRangePadding    -> Scaled[0.15],
        PerformanceGoal     -> "Quality",
        ImagePadding        -> 25
    ]
]

(* For a graph with a single vertex carrying a self-loop, Wolfram's
   default self-loop renderer collapses to nothing.  Draw the loop
   manually as a circular arc above the vertex. *)
singleVertexLoopFn[vertices_, edges_, color_] := If[
    Length[vertices] === 1 && Length[edges] >= 1,
    Function[{coords, edge},
        With[{p = coords[[1]]},
            {color, AbsoluteThickness[1.5],
             Circle[p + {0, 0.55}, 0.35],
             Arrow[BSplineCurve[{
                 p + {0, 0.55} + {0.30, -0.10},
                 p + {0.10, 0.40},
                 p + {0, 0.30}
             }]]}
        ]
    ],
    Automatic
]
