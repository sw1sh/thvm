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

BeginPackage["THVMLink`", {
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
        (* Non-CONST UOPs key on the producer's base instead of the
           cell loc, so multi-reference (a single UOP feeding N
           consumer slots) collapses to one shared wire -- DC then
           draws a spider where the producer fans out to all the
           consumers.  CONST stays per-loc because we render a
           CONST leaf per consumer cell. *)
        $TagUOP,
            If[ext === $UopConst,
                "w"   <> ToString[loc],
                "uop" <> ToString[val]
            ],
        _,                 "w" <> ToString[loc]
    ]
]

binderWire[base_Integer] := "var" <> ToString[base]

(* Find the cell whose value carries a given agent's term.  None if
   the agent is heapless (held only as a WL return value, e.g. the
   root term).  DUP is not stored as a term in any slot - DP0/DP1
   reference it instead - so principalCellOf for DUP is always None.

   The cell must live inside a *reachable* agent's slot range -
   pre-rewrite cells that survive a TWnf would otherwise grab the
   principal wire and route the diagram through dead heap. *)
agentSlotsOf[agents_Association, opcodes_Association] := Catenate[
    KeyValueMap[
        Function[{base, tag},
            With[{
                n = If[ tag === $TagUOP,
                        walkArity[Lookup[opcodes, base, 0]],
                        agentArity[tag]
                    ]
            },
                Range[base, base + n - 1]
            ]
        ],
        agents
    ]
]

principalCellOf[agentBase_Integer, agentTag_Integer,
                agents_Association, opcodes_Association] := Block[{
    n = THeapPos[], inSlot
},
    inSlot = agentSlotsOf[agents, opcodes];
    SelectFirst[
        inSlot,
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

(* IC arities only.  UOP arity, opcode names, output-shape inference,
   bit decoding, and shape arithmetic come from sibling Uop.wl /
   Shape.wl which share THVMLink`Private` -- no qualification needed. *)
agentArity[$TagLAM] = 1; agentArity[$TagDUP] = 1;
agentArity[$TagAPP] = 2; agentArity[$TagSUP] = 2;

(* slotSide: which DC list the agent's slot lives in (top = inputs,
   bottom = outputs).  slotIsDualed: whether that slot's port is
   wrapped in SuperStar.  Used to decide the opposite side and
   polarity for an ERA / TEN leaf sitting in this slot. *)
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

(* UOP slots are all top+plain (sources flow IN from above into the
   apex-down compute triangle; result flows out the bottom apex). *)
slotSide[$TagUOP, _]    := "Top";
slotIsDualed[$TagUOP, _] := False;

(* Find which (base, tag, offset) owns cell at loc, given the
   discovered agents association.  Returns None if loc is not in
   any compound agent's slot range.  UOP arity comes from the
   opcodes Association. *)
locOwner[loc_Integer, agents_Association, opcodes_Association] := Catch[
    KeyValueMap[
        Function[{base, tag},
            With[{n = If[ tag === $TagUOP,
                          uopArity[Lookup[opcodes, base, 0]],
                          agentArity[tag]
                      ]},
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
agentStyle[$TagTEN] := Directive[EdgeForm[White], FaceForm[Darker[StandardCyan,   0.45]]]
(* UOP fill: orange for GRAD (it's the "rewrite" UOP, distinct from
   compute), blue for everything else. *)
uopStyle[$UopGrad] := Directive[EdgeForm[White], FaceForm[Darker[StandardOrange, 0.35]]]
uopStyle[_]        := Directive[EdgeForm[White], FaceForm[Darker[StandardBlue,   0.45]]]

(* Apex of the triangle = the principal port.
   Triangle (apex up)            -> principal is an INPUT at the top
   UpsideDownTriangle (apex down) -> principal is an OUTPUT at the bottom
*)
agentShape[$TagLAM] := "RoundedUpsideDownTriangle"  (* principal output *)
agentShape[$TagDUP] := "RoundedTriangle"            (* principal input  *)
agentShape[$TagAPP] := "RoundedTriangle"            (* principal input  *)
agentShape[$TagSUP] := "RoundedUpsideDownTriangle"  (* principal output *)
agentShape[$TagUOP] := "RoundedUpsideDownTriangle"  (* principal output *)
agentShape[$TagTEN] := "RoundedUpsideDownTriangle"  (* leaf output      *)

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

(* Label format: header line "OPCODE@<heap-loc>#<id>" (id only when
   the opcode carries an extra handle -- KERNEL points at a kernel
   id, GRAD at the target tensor id), optional shape on a second
   line, optional scalar value (CONST) on a third.  Single-line
   header keeps the "where in the heap is this" reading at a glance.

   Most UOPs only have heap base, so "MUL@8" / "ADD@12".  *)
uopLabelText[base_Integer, opcode_Integer] := With[{shape = uopShapeOf[base]},
    Column[
        Join[
            {uopHeader[base, opcode]},
            If[ListQ[shape], {shapeText[shape]}, {}]
        ],
        Center, Spacings -> 0
    ]
]

uopHeader[base_Integer, $UopKernel] := With[{
    kid = TTermVal[THeapRead[base + 1]]
},
    "KERNEL@" <> ToString[base] <> "#" <> ToString[kid]
]

uopHeader[base_Integer, $UopGrad] := With[{
    targetCell = THeapRead[base + 2]
},
    "GRAD@" <> ToString[base] <>
        If[TTermTag[targetCell] === $TagTEN,
            "#" <> ToString[TTermVal[targetCell]],
            ""
        ]
]

uopHeader[base_Integer, opcode_Integer] :=
    uopName[opcode] <> "@" <> ToString[base]

(* TEN leaf label: just the tensor handle id + optional shape.
   No "@<cell-loc>" -- the cell holding the TAG_TEN reference is
   *owned by the parent UOP* (it's a slot of that UOP's heap
   block), so showing the loc would conflict with the parent's
   "@<base>" label.  The diagram structure already makes the
   parent slot visually clear. *)
tenLabelText[loc_Integer, id_Integer] := With[{shape = tenShapeOf[id]},
    Column[
        Join[
            {"TEN#" <> ToString[id]},
            If[ ListQ[shape], {shapeText[shape]}, {}]
        ],
        Center, Spacings -> 0
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
agentDiagram[base_Integer, $TagLAM, principal_, agents_Association, opcodes_Association] := Block[{
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
    owner = If[principal === None, None, locOwner[principal, agents, opcodes]];
    If[ MatchQ[owner, {_, $TagAPP, 1}], DiagramFlip[d], d]
]

(* APP(f, x): apex-up shape (principal input).  f is the
   incoming principal input (plain) at the top apex.  Aux ports
   at the bottom are {x, out*} with x plain outgoing and out
   dualed (SuperStar = arrow reversed). *)
agentDiagram[base_Integer, $TagAPP, principal_, _Association, _Association] := With[{
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
agentDiagram[base_Integer, $TagSUP, principal_, _Association, _Association] := With[{
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

(* UOP: apex-down shape (principal output at bottom).  Compute
   sources at top (one wire per slot 0..n-1 where n =
   uopArity[opcode]); NUM slots beyond compute arity are
   surfaced via the label, not as wires.  Style is opcode-driven
   (orange for GRAD, blue otherwise).

   GRAD is special-cased: it follows the DUP shape (apex-up, one
   principal input at top, two aux outputs at bottom = forward
   value + backward gradient).  Cell layout in heap is
   [y, gy, target]; we route y as the principal input, surface
   target's tensor id as "#<tid>" in the label, and let gy stay
   as an unconnected sub-graph.  The "bwd" output is the wire
   downstream code consumes (= principal cell wire); "fwd" is a
   dangling synthetic wire so the diagram makes the fwd/bwd
   branch explicit. *)
agentDiagram[base_Integer, $TagUOP, principal_, _Association, opcodes_Association] := Block[{
    opcode = Lookup[opcodes, base, 0]
},
    If[ opcode === $UopGrad,
        gradDiagram[base, principal],
        plainUopDiagram[base, principal, opcode]
    ]
]

plainUopDiagram[base_Integer, principal_, opcode_Integer] := Block[{
    n = uopArity[opcode], pWire
},
    (* "uop<base>" matches the wire name any consumer cell holding
       TAG_UOP(base) would resolve via wireFor -- see the TAG_UOP
       branch there.  Even for a heapless seed (no consumer) the
       convention keeps producer + consumer naming aligned. *)
    pWire = If[ principal === None, "uop" <> ToString[base], wireFor[principal]];
    With[{
        label   = uopLabelText[base, opcode],
        inputs  = Table[wireFor[base + i], {i, 0, n - 1}],
        outputs = {pWire},
        shape   = agentShape[$TagUOP], style = uopStyle[opcode]
    },
        Diagram[label, inputs, outputs, "Shape" -> shape, "Style" -> style]
    ]
]

gradDiagram[base_Integer, principal_] := Block[{
    label, yWire, fwdWire, bwdWire
},
    label   = uopHeader[base, $UopGrad];     (* "GRAD@<base>#<tid>" *)
    yWire   = wireFor[base];
    bwdWire = If[ principal === None, "uop" <> ToString[base], wireFor[principal]];
    fwdWire = "fwd" <> ToString[base];
    With[{
        ins  = {yWire},
        outs = {fwdWire, bwdWire},
        shape = "RoundedTriangle", style = uopStyle[$UopGrad]
    },
        Diagram[label, ins, outs, "Shape" -> shape, "Style" -> style]
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
agentDiagram[base_Integer, $TagDUP, _, _Association, _Association] := With[{lab = dupLabelFor[base]},
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
eraDiagram[loc_Integer, agents_Association, opcodes_Association] := Block[{
    owner, side, dualedQ, name = wireFor[loc], port
},
    owner = locOwner[loc, agents, opcodes];
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

(* TEN leaf: a TAG_TEN cell at `loc` is an inline tensor handle
   sitting in some slot.  Render as a cyan apex-down triangle
   with one port whose side+polarity is chosen the same way as
   ERA (opposite side from the slot, matching polarity) so the
   slot wire has exactly one dual + one non-dual port. *)
tenLeafDiagram[loc_Integer, agents_Association, opcodes_Association] := Block[{
    t, id, owner, side, dualedQ, name = wireFor[loc], port
},
    t  = THeapRead[loc];
    id = TTermVal[t];
    owner = locOwner[loc, agents, opcodes];
    If[ owner === None,
        side = "Bottom"; dualedQ = False
    ,
        side = If[ slotSide[owner[[2]], owner[[3]]] === "Top",
                   "Bottom", "Top" ];
        dualedQ = slotIsDualed[owner[[2]], owner[[3]]]
    ];
    port = If[dualedQ, SuperStar[name], name];
    With[{
        ins   = If[side === "Top",    {port}, {}],
        outs  = If[side === "Bottom", {port}, {}],
        label = tenLabelText[loc, id]
    },
        Diagram[label, ins, outs,
            "Shape" -> agentShape[$TagTEN], "Style" -> agentStyle[$TagTEN]
        ]
    ]
]

(* CONST leaf: a TAG_UOP cell at `loc` whose opcode is CONST.  Each
   reference renders its own triangle (no shared CONST agent), so
   multi-referenced constants don't need a DUP to fan out.  Label
   surfaces the heap base + decoded scalar value (via Shape.wl's
   scalarTextFromCell). *)
constLabelText[base_Integer] := With[{
    text = scalarTextFromCell[THeapRead[base]]
},
    Column[
        Join[
            {"CONST", "@" <> ToString[base]},
            If[text =!= "", {text}, {}]
        ],
        Center, Spacings -> 0
    ]
]

constLeafDiagram[loc_Integer, agents_Association, opcodes_Association] := Block[{
    t, base, owner, side, dualedQ, name = wireFor[loc], port
},
    t    = THeapRead[loc];
    base = TTermVal[t];
    owner = locOwner[loc, agents, opcodes];
    If[ owner === None,
        side = "Bottom"; dualedQ = False
    ,
        side = If[ slotSide[owner[[2]], owner[[3]]] === "Top",
                   "Bottom", "Top" ];
        dualedQ = slotIsDualed[owner[[2]], owner[[3]]]
    ];
    port = If[dualedQ, SuperStar[name], name];
    With[{
        ins   = If[side === "Top",    {port}, {}],
        outs  = If[side === "Bottom", {port}, {}],
        label = constLabelText[base]
    },
        Diagram[label, ins, outs,
            "Shape" -> "RoundedUpsideDownTriangle",
            "Style" -> uopStyle[$UopConst]
        ]
    ]
]

(* === discovery === *)

agentRule[t_] := With[{tag = TTermTag[t], val = TTermVal[t]},
    Which[
        tag === $TagLAM || tag === $TagAPP || tag === $TagSUP ||
        tag === $TagDUP || tag === $TagUOP,
            val -> tag,
        tag === $TagVAR,
            val -> $TagLAM,
        tag === $TagDP0 || tag === $TagDP1,
            val -> $TagDUP,
        True,
            Nothing
    ]
]

uopOpcodeRule[t_] := If[
    TTermTag[t] === $TagUOP,
    TTermVal[t] -> TTermExt[t],
    Nothing
]

discoverAgentsHere[seedTerms_List] := Block[{n = THeapPos[], terms},
    terms = Join[seedTerms, Table[THeapRead[loc], {loc, 0, n - 1}]];
    Association[agentRule /@ terms]
]

discoverUopOpcodesHere[seedTerms_List] := Block[{n = THeapPos[], terms},
    terms = Join[seedTerms, Table[THeapRead[loc], {loc, 0, n - 1}]];
    Association[uopOpcodeRule /@ terms]
]

discoverErasHere[] := Block[{n = THeapPos[]},
    Select[Range[0, n - 1], TTermTag[THeapRead[#]] === $TagERA &]
]

(* Reachability for tensor (UOP/TEN) world only -- IC agents stay
   on the full-heap-walk path because they're orthogonal.

   The heap is append-only: pre-WNF cells survive a TWnf rewrite,
   so a fresh discovery from the post-WNF root would otherwise
   surface dangling old subgraphs.  Walking forward from the seed
   keeps only what the rendered term actually references.

   Walk arity for GRAD is 1 (only follow the y branch -- gy and
   target are intentionally hidden), uopArity[op] for everything
   else. *)
walkArity[op_] := If[op === $UopGrad, 1, uopArity[op]]

(* CONST UOPs are constants -- 0 compute inputs, just a NUM payload.
   We render them as PER-REFERENCE leaves (similar to TEN handles)
   instead of one shared agent, so a CONST referenced from N slots
   draws N triangles without needing DUPs to share the value.  Skip
   them during the BFS so they don't end up in reachOps. *)
reachableUopsHere[seedTerms_List] := Block[{seen = <||>, queue, t, base, op, n},
    queue = seedTerms;
    While[ Length[queue] > 0,
        t = First[queue]; queue = Rest[queue];
        If[ TTermTag[t] === $TagUOP,
            base = TTermVal[t]; op = TTermExt[t];
            If[ op =!= $UopConst && ! KeyExistsQ[seen, base],
                seen[base] = op;
                n = walkArity[op];
                queue = Join[queue, Table[THeapRead[base + i], {i, 0, n - 1}]]
            ]
        ]
    ];
    seen
]

(* Cells inside reachable UOP slots that hold inline atoms we'll
   render as leaves.  TAG_TEN cells -> tenLeafDiagram, TAG_UOP
   cells with opcode CONST -> constLeafDiagram. *)
reachableSlotCells[reachOps_Association, pred_] := Catenate[
    KeyValueMap[
        Function[{base, op}, With[{n = walkArity[op]},
            Select[Range[base, base + n - 1], pred[THeapRead[#]] &]
        ]],
        reachOps
    ]
]

reachableTenCells[reachOps_Association] :=
    reachableSlotCells[reachOps, TTermTag[#] === $TagTEN &]

reachableConstCells[reachOps_Association] := reachableSlotCells[reachOps,
    TTermTag[#] === $TagUOP && TTermExt[#] === $UopConst &]

THeapDiagram[t_] := Block[{
    fullAgents, reachOps, agents, opcodes, eras, tens, consts, ds,
    $uopOpcodeContext
},
    fullAgents = discoverAgentsHere[{t}];
    reachOps   = reachableUopsHere[{t}];
    (* Stash full UOP opcode table (CONSTs included even though they
       aren't in reachOps) so uopShapeOf in Uop.wl can resolve every
       base without re-scanning the heap.  Block above scopes the
       rebind to this render. *)
    $uopOpcodeContext = discoverUopOpcodesHere[{t}];
    (* Keep IC agents from full discovery; replace UOP entries with
       only the reachable ones so old pre-rewrite UOPs (plus their
       transitively-reached TENs) drop out of the diagram. *)
    agents = Join[
        KeySelect[fullAgents, fullAgents[#] =!= $TagUOP &],
        Association[(# -> $TagUOP) & /@ Keys[reachOps]]
    ];
    opcodes = reachOps;
    eras    = discoverErasHere[];
    tens    = reachableTenCells[reachOps];
    consts  = reachableConstCells[reachOps];
    ds = Join[
        KeyValueMap[
            agentDiagram[#1, #2, principalCellOf[#1, #2, agents, opcodes],
                         agents, opcodes] &,
            agents
        ],
        eraDiagram[#, agents, opcodes]      & /@ eras,
        tenLeafDiagram[#, agents, opcodes]   & /@ tens,
        constLeafDiagram[#, agents, opcodes] & /@ consts
    ];
    DiagramNetwork @@ ds
]

End[];
EndPackage[];
