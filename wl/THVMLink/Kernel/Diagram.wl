(* ::Package:: *)
(* Diagram.wl - Wolfram`DiagrammaticComputation`-backed renderer.

   Sits in its own subcontext (`THVMLink`Diagram`) so it can put the
   DC package on its $ContextPath via BeginPackage's import list and
   call `Diagram` / `DiagramNetwork` directly without ambiguity.

   Loaded from THVMLink.wl after the public symbol THeapDiagram is
   declared.

   Polarity convention (chosen so DC's NetGraph never inserts an
   identity binary spider on a wire and `diagram["Arrange"]["Grid"]`
   draws every arrow top-to-bottom without flipping):

     Inputs  are drawn at top, listed with SuperStar[name] (dualed)
     Outputs are drawn at bottom, listed plain (non-dual)

   Per-slot port types (every heap slot is an INPUT: the agent
   consumes the value stored there.  f is still an input slot in
   terms of DC position/direction; it is called "principal" only in
   the IC sense of where APP meets LAM):
     LAM body (cell[base])    = INPUT
     APP f    (cell[base])    = INPUT
     APP x    (cell[base+1])  = INPUT
     SUP L    (cell[base])    = INPUT
     SUP R    (cell[base+1])  = INPUT
     DUP body (cell[base])    = INPUT

   Every agent's own "result" wire (where its term is held in
   another agent's slot) has DYNAMIC direction: opposite of the
   carrier slot's type, so the wire has exactly one dual and one
   non-dual port and arrows never flip.  ERA's single port is
   dynamic for the same reason. *)

BeginPackage["THVMLink`Diagram`", {
    "THVMLink`",
    "Wolfram`DiagrammaticComputation`",
    "Wolfram`DiagrammaticComputation`Diagram`"
}];

Begin["`Private`"];

(* === wire names ===
   Each LAM has TWO distinct auxiliary wires:
     binder   = "var<base>"   where VAR cells reference this LAM
     body     = "w<base>"     the cell that holds LAM's body pointer
   so the principal-port wire and the binder wire never collapse to
   the same name (which would create three-port wires for terms like
   the K-combinator).

   DUP similarly has two aux wires keyed by the dup label, distinct
   from its body slot wire "w<base>". *)

wireFor[loc_Integer] := Block[{t, tag, val, ext},
    t   = THeapRead[loc];
    tag = TTermTag[t]; val = TTermVal[t]; ext = TTermExt[t];
    Switch[tag,
        $TagVAR,           "var" <> ToString[val],
        $TagDP0,           "dup" <> ToString[val] <> "_dp0_lab" <> ToString[ext],
        $TagDP1,           "dup" <> ToString[val] <> "_dp1_lab" <> ToString[ext],
        _,                 "w" <> ToString[loc]
    ]
]

binderWire[base_Integer] := "var" <> ToString[base]

(* Find the cell whose value carries a given agent's term.  None if
   the agent is heapless (held only as a WL return value, e.g. the
   root term).  DUP is not stored as a term in any slot - DP0/DP1
   reference it instead - so principalCellOf for DUP is always None. *)
principalCellOf[agentBase_Integer, agentTag_Integer] := Block[{n = THeapPos[]},
    SelectFirst[
        Range[0, n - 1],
        With[{t = THeapRead[#]},
            TTermVal[t] === agentBase && TTermTag[t] === agentTag] &,
        None
    ]
]

(* === slot ownership lookup ===
   Number of heap cells used per agent type, and the port type of
   each slot offset.

   APP's slot 0 (the f position) is the APP's PRINCIPAL port, which
   is an OUTPUT in this convention (an APP reaches out through f to
   find a LAM to beta-reduce with; the wire on the f side carries
   that outgoing interaction).  All other slots are INPUTs (the
   agent reads/consumes the value stored there). *)

agentArity[$TagLAM] = 1; agentArity[$TagDUP] = 1;
agentArity[$TagAPP] = 2; agentArity[$TagSUP] = 2;

(* slotSide: which DC list the agent's slot lives in (top = inputs,
   bottom = outputs).  slotIsDualed: whether that slot's port is
   wrapped in SuperStar.  Used to decide the opposite side and
   polarity for an ERA sitting in this slot. *)
slotSide[$TagLAM, 0]    = "Top";        (* body at input list, plain *)
slotIsDualed[$TagLAM, 0] = False;

slotSide[$TagAPP, 0]    = "Top";        (* f at input list, plain *)
slotIsDualed[$TagAPP, 0] = False;

slotSide[$TagAPP, 1]    = "Bottom";     (* x* at output list, dualed *)
slotIsDualed[$TagAPP, 1] = True;

slotSide[$TagSUP, 0]    = "Top";
slotIsDualed[$TagSUP, 0] = False;
slotSide[$TagSUP, 1]    = "Top";
slotIsDualed[$TagSUP, 1] = False;

slotSide[$TagDUP, 0]    = "Top";        (* body = principal incoming, plain *)
slotIsDualed[$TagDUP, 0] = False;

(* Find which (base, tag, offset) owns cell at loc, given the
   discovered agents association.  Returns None if loc is not in
   any compound agent's slot range. *)
locOwner[loc_Integer, agents_Association] := Catch[
    KeyValueMap[
        Function[{base, tag},
            With[{n = agentArity[tag]},
                If[ NumberQ[n] && base <= loc < base + n,
                    Throw[{base, tag, loc - base}]
                ]
            ]
        ],
        agents
    ];
    None
]

(* === styling === *)

agentStyle[$TagLAM] := Directive[EdgeForm[White], FaceForm[Darker[StandardGreen,  0.45]]]
agentStyle[$TagAPP] := Directive[EdgeForm[White], FaceForm[Darker[StandardBlue,   0.45]]]
agentStyle[$TagSUP] := Directive[EdgeForm[White], FaceForm[Darker[StandardOrange, 0.45]]]
agentStyle[$TagDUP] := Directive[EdgeForm[White], FaceForm[Darker[StandardPurple, 0.45]]]

(* Apex of the triangle = the principal port.
   Triangle (apex up)            -> principal is an INPUT at the top
   UpsideDownTriangle (apex down) -> principal is an OUTPUT at the bottom
*)
agentShape[$TagLAM] := "RoundedUpsideDownTriangle"  (* principal output *)
agentShape[$TagDUP] := "RoundedTriangle"            (* principal input  *)
agentShape[$TagAPP] := "RoundedTriangle"            (* principal input  *)
agentShape[$TagSUP] := "RoundedUpsideDownTriangle"  (* principal output *)

(* Multi-line "TAG\n@<base>" or "TAG\n@<base>..<base+arity-1>" label
   matching the heap graph's vertex labels. *)
agentLabelText[base_Integer, tag_Integer] := With[{arity = agentArity[tag]},
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

(* === per-agent diagrams ===
   Diagram[expr, inputs, outputs] holds its args unevaluated; wrap
   the label, port lists, Shape, and Style in With so they evaluate
   first.

   Slot wires use SuperStar (dualed) on the slot side, plain on the
   producer side, so each wire has exactly one dual + one non-dual
   port. *)

(* LAM: principal is the single outgoing output (plain) at the
   bottom apex; aux at the top are {var, body*} with var plain
   outgoing and body dualed (SuperStar = arrow reversed).

   When LAM sits in an APP's arg (x) slot, DiagramFlip it so the
   flipped shape/ports match the surrounding context. *)
agentDiagram[base_Integer, $TagLAM, principal_, agents_Association] := Block[{
    pWire, owner, d
},
    pWire = If[ principal === None,
                "p" <> ToString[base],
                wireFor[principal]];
    d = With[{
        label   = agentLabelText[base, $TagLAM],
        inputs  = {SuperStar[binderWire[base]], wireFor[base]},
        outputs = {pWire},
        shape   = agentShape[$TagLAM], style = agentStyle[$TagLAM]
    },
        Diagram[label, inputs, outputs, "Shape" -> shape, "Style" -> style]
    ];
    owner = If[principal === None, None, locOwner[principal, agents]];
    If[ MatchQ[owner, {_, $TagAPP, 1}], DiagramFlip[d], d]
]

(* APP(f, x): apex-up shape (principal input).  f is the
   incoming principal input (plain) at the top apex.  Aux ports
   at the bottom are {x, out*} with x plain outgoing and out
   dualed (SuperStar = arrow reversed). *)
agentDiagram[base_Integer, $TagAPP, principal_, _Association] := With[{
    outWire = If[ principal === None,
                  "p" <> ToString[base],
                  wireFor[principal]]
},
    With[{
        label   = agentLabelText[base, $TagAPP],
        inputs  = {wireFor[base]},
        outputs = {SuperStar[wireFor[base + 1]], outWire},
        shape   = agentShape[$TagAPP], style = agentStyle[$TagAPP]
    },
        Diagram[label, inputs, outputs, "Shape" -> shape, "Style" -> style]
    ]
]

(* SUP: L and R are plain incoming inputs at the flat top;
   result is the outgoing output at the bottom apex. *)
agentDiagram[base_Integer, $TagSUP, principal_, _Association] := With[{
    rWire = If[ principal === None,
                "p" <> ToString[base],
                wireFor[principal]]
},
    With[{
        label   = agentLabelText[base, $TagSUP],
        inputs  = {wireFor[base], wireFor[base + 1]},
        outputs = {rWire},
        shape   = agentShape[$TagSUP], style = agentStyle[$TagSUP]
    },
        Diagram[label, inputs, outputs, "Shape" -> shape, "Style" -> style]
    ]
]

(* The DUP cell carries no label of its own; find one from any
   DP0/DP1 cell that references it.  Fallback 0 if none in heap. *)
dupLabelFor[base_Integer] := Block[{n = THeapPos[], cell},
    cell = SelectFirst[
        Range[0, n - 1],
        With[{t = THeapRead[#]},
            (TTermTag[t] === $TagDP0 || TTermTag[t] === $TagDP1) &&
            TTermVal[t] === base] &,
        None
    ];
    If[ cell === None, 0, TTermExt[THeapRead[cell]]]
]

(* DUP: principal is incoming plain input at the top apex (body to
   duplicate comes IN via cell[base]); dp0, dp1 are outgoing
   outputs at the flat bottom. *)
agentDiagram[base_Integer, $TagDUP, _, _Association] := With[{lab = dupLabelFor[base]},
    With[{
        label   = agentLabelText[base, $TagDUP],
        inputs  = {wireFor[base]},
        outputs = {
            "dup" <> ToString[base] <> "_dp0_lab" <> ToString[lab],
            "dup" <> ToString[base] <> "_dp1_lab" <> ToString[lab]
        },
        shape   = agentShape[$TagDUP], style = agentStyle[$TagDUP]
    },
        Diagram[label, inputs, outputs, "Shape" -> shape, "Style" -> style]
    ]
]

(* ERA has a single port whose side is OPPOSITE of the carrier
   slot (so the wire flows naturally without arrow-flips) and
   whose polarity matches the slot's polarity (so both ends of
   the wire have the same DualQ; DC joins them without a spider). *)
eraDiagram[loc_Integer, agents_Association] := Block[{
    owner, side, dualedQ, name = wireFor[loc], port
},
    owner = locOwner[loc, agents];
    If[ owner === None,
        side = "Bottom"; dualedQ = False
    ,
        side = If[ slotSide[owner[[2]], owner[[3]]] === "Top",
                   "Bottom", "Top" ];
        dualedQ = slotIsDualed[owner[[2]], owner[[3]]]
    ];
    port = If[dualedQ, SuperStar[name], name];
    With[{
        ins  = If[side === "Top",    {port}, {}],
        outs = If[side === "Bottom", {port}, {}]
    },
        Diagram["ERA", ins, outs,
            "Shape" -> "Disk",
            "Style" -> Directive[EdgeForm[White], FaceForm[GrayLevel[0.4]]]
        ]
    ]
]

(* === discovery === *)

agentRule[t_] := With[{tag = TTermTag[t], val = TTermVal[t]},
    Which[
        tag === $TagLAM || tag === $TagAPP || tag === $TagSUP || tag === $TagDUP,
            val -> tag,
        tag === $TagVAR,
            val -> $TagLAM,
        tag === $TagDP0 || tag === $TagDP1,
            val -> $TagDUP,
        True,
            Nothing
    ]
]

discoverAgentsHere[seedTerms_List] := Block[{n = THeapPos[], terms},
    terms = Join[seedTerms, Table[THeapRead[loc], {loc, 0, n - 1}]];
    Association[agentRule /@ terms]
]

discoverErasHere[] := Block[{n = THeapPos[]},
    Select[Range[0, n - 1], TTermTag[THeapRead[#]] === $TagERA &]
]

THeapDiagram[t_] := Block[{
    agents = discoverAgentsHere[{t}],
    eras   = discoverErasHere[],
    ds
},
    ds = Join[
        KeyValueMap[
            agentDiagram[#1, #2, principalCellOf[#1, #2], agents] &,
            agents
        ],
        eraDiagram[#, agents] & /@ eras
    ];
    DiagramNetwork @@ ds
]

End[];
EndPackage[];
