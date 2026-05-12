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
        (* TAG_TEN: key the wire on the *tensor handle* (tid), not the
           cell loc.  Every cell holding a reference to the same tid
           collapses onto one shared wire, so a kernel reading another
           kernel's output_buf draws a direct edge between the two
           kernels (their two TAG_TEN cells -- producer's at kloc+0,
           consumer's anywhere -- share "tenTid<tid>").  External
           tensors used by multiple kernels likewise spider out from
           one TEN leaf. *)
        $TagTEN,           "tenTid" <> ToString[val],
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
                n = Which[
                    tag === $TagUOP, walkArity[Lookup[opcodes, base, 0]],
                    tag === $TagCTR, ctrSpan[base],
                    True,            agentArity[tag]
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
   Shape.wl which share THVMLink`Private` -- no qualification needed.

   ALO holds (body, state); MAT holds (scrut, case-tree); OP2 holds
   (lhs, rhs); CTR has variable arity (heap[val]=NUM(n), heap[val+1..val+n]
   = children) -- ctrArity[base] reads it from the cell. *)
agentArity[$TagLAM] = 1; agentArity[$TagDUP] = 1;
agentArity[$TagAPP] = 2; agentArity[$TagSUP] = 2;
agentArity[$TagALO] = 2; agentArity[$TagMAT] = 2;
agentArity[$TagOP2] = 2;
agentArity[$TagDSU] = 3; agentArity[$TagDDU] = 3;

(* CTR's heap layout: arity NUM at base, n children at base+1..base+n.
   The total span is 1 + n cells.  Used by locOwner to claim every
   cell that belongs to this CTR. *)
ctrSpan[base_Integer] := 1 + TTermVal[THeapRead[base]]

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

(* ALO/CTR/MAT/OP2 input slots: all top+plain (sources flow IN from
   above into the apex-down result triangle).  The wrapped body of an
   ALO sits at slot 0; its state cell at slot 1 is metadata, not a
   wire.  CTR's slot 0 holds the arity NUM (also metadata); slots
   1..n are the data children.  MAT slot 0 = scrut, slot 1 = case
   tree.  OP2 slot 0 = lhs, slot 1 = rhs. *)
slotSide[$TagALO, _]    := "Top"; slotIsDualed[$TagALO, _] := False;
slotSide[$TagCTR, _]    := "Top"; slotIsDualed[$TagCTR, _] := False;
slotSide[$TagMAT, _]    := "Top"; slotIsDualed[$TagMAT, _] := False;
slotSide[$TagOP2, _]    := "Top"; slotIsDualed[$TagOP2, _] := False;
slotSide[$TagDSU, _]    := "Top"; slotIsDualed[$TagDSU, _] := False;
slotSide[$TagDDU, _]    := "Top"; slotIsDualed[$TagDDU, _] := False;

(* Find which (base, tag, offset) owns cell at loc, given the
   discovered agents association.  Returns None if loc is not in
   any compound agent's slot range.  UOP arity comes from the
   opcodes Association; CTR arity from the leading NUM cell. *)
locOwner[loc_Integer, agents_Association, opcodes_Association] := Catch[
    KeyValueMap[
        Function[{base, tag},
            With[{n = Which[
                tag === $TagUOP, uopArity[Lookup[opcodes, base, 0]],
                tag === $TagCTR, ctrSpan[base],
                True,            agentArity[tag]
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
(* Lazy / book / case nodes -- match Style.wl's THeapGraph palette. *)
agentStyle[$TagREF] := Directive[EdgeForm[White], FaceForm[Darker[StandardYellow, 0.4]]]
agentStyle[$TagALO] := Directive[EdgeForm[White], FaceForm[Darker[StandardYellow, 0.55]]]
agentStyle[$TagCTR] := Directive[EdgeForm[White], FaceForm[Darker[StandardRed,    0.45]]]
agentStyle[$TagMAT] := Directive[EdgeForm[White], FaceForm[Darker[StandardRed,    0.55]]]
agentStyle[$TagOP2] := Directive[EdgeForm[White], FaceForm[Darker[StandardBlue,   0.55]]]
agentStyle[$TagNUM] := Directive[EdgeForm[White], FaceForm[GrayLevel[0.5]]]
agentStyle[$TagDSU] := Directive[EdgeForm[White], FaceForm[Darker[StandardOrange, 0.65]]]
agentStyle[$TagDDU] := Directive[EdgeForm[White], FaceForm[Darker[StandardPurple, 0.65]]]
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
(* CTR / ALO / OP2 produce a value (their output flows out the
   bottom apex into a parent slot); MAT consumes a scrut into its
   case-tree (apex up; principal input).  REF / NUM are leaves
   rendered as disks per-reference. *)
agentShape[$TagCTR] := "RoundedUpsideDownTriangle"
agentShape[$TagALO] := "RoundedUpsideDownTriangle"
agentShape[$TagOP2] := "RoundedUpsideDownTriangle"
agentShape[$TagMAT] := "RoundedUpsideDownTriangle"
agentShape[$TagREF] := "Disk"
agentShape[$TagNUM] := "Disk"
(* DSU produces a SUP-like value (apex-down output); DDU consumes
   like DUP (apex-up principal input).  Both have the label as an
   extra strict input slot. *)
agentShape[$TagDSU] := "RoundedUpsideDownTriangle"
agentShape[$TagDDU] := "RoundedTriangle"

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
    Which[
        opcode === $UopGrad,   gradDiagram[base, principal],
        opcode === $UopKernel, kernelDiagram[base, principal],
        True,                  plainUopDiagram[base, principal, opcode]
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

(* KERNEL agent: like plainUopDiagram but with N additional input
   ports drawn from the C-side input_tids[] array (the kernel's
   actual upstream tensors -- not present in any heap cell of the
   UOP_KERNEL itself, which only carries (output_buf, NUM(kid))).
   Each input wire keys on the input tid via "tenTid<tid>", so
   another kernel's output_buf TEN cell -- whose wireFor is the
   same string -- auto-merges into a single edge.  External inputs
   (weights, the SGD targets) get a synthetic TEN leaf rendered
   separately by externalKernelInputLeaves below. *)
kernelDiagram[base_Integer, principal_] := Block[{
    kid, inputTids, outBufWire, kidLabelWire, inputWires, pWire,
    label
},
    kid          = TTermVal[THeapRead[base + 1]];
    inputTids    = TKernelInputs[kid];
    label        = uopHeader[base, $UopKernel];
    outBufWire   = wireFor[base];           (* "tenTid<output_tid>" *)
    kidLabelWire = wireFor[base + 1];       (* NUM(kid) cell -- label only *)
    inputWires   = ("tenTid" <> ToString[#]) & /@ inputTids;
    pWire        = If[ principal === None, "uop" <> ToString[base], wireFor[principal]];
    With[{
        ins   = Join[{outBufWire, kidLabelWire}, inputWires],
        outs  = {pWire},
        shape = agentShape[$TagUOP], style = uopStyle[$UopKernel]
    },
        Diagram[label, ins, outs, "Shape" -> shape, "Style" -> style]
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
dupLabelFor[base_Integer] := Block[{lo = THeapBase[], n = THeapPos[], cell},
    cell = SelectFirst[
        Range[lo, n - 1],
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

(* === Lazy / book / case agents === *)

(* ALO(body, state): body wire feeds in at the top, principal
   output flows down into the parent slot.  state is metadata,
   surfaced in the label. *)
aloLabelText[base_Integer] := With[{stateCell = THeapRead[base + 1]},
    Column[{"ALO", "@" <> ToString[base],
            "s" <> ToString[TTermVal[stateCell]]}, Center, Spacings -> 0]
]

agentDiagram[base_Integer, $TagALO, principal_, _Association, _Association] :=
    With[{outWire = If[ principal === None,
                        "p" <> ToString[base],
                        wireFor[principal]]},
        With[{
            label   = aloLabelText[base],
            inputs  = {wireFor[base]},
            outputs = {outWire},
            shape   = agentShape[$TagALO], style = agentStyle[$TagALO]
        },
            Diagram[label, inputs, outputs, "Shape" -> shape, "Style" -> style]
        ]
    ]

(* CTR with ext = ctrTag.  heap[base] = NUM(arity) (metadata, not a
   wire); heap[base+1..base+n] = data children.  Each data slot is a
   top input wire; the principal output goes to the parent slot. *)
ctrLabelText[base_Integer, ctrTag_Integer] := With[{n = TTermVal[THeapRead[base]]},
    Column[{"CTR#" <> ToString[ctrTag],
            "@" <> ToString[base],
            "n=" <> ToString[n]}, Center, Spacings -> 0]
]

agentDiagram[base_Integer, $TagCTR, principal_, _Association, _Association] :=
    Block[{n, ctrTag, outWire, inWires, lab},
        n       = TTermVal[THeapRead[base]];
        ctrTag  = ctrTagFor[base];
        outWire = If[ principal === None, "p" <> ToString[base], wireFor[principal]];
        inWires = Table[wireFor[base + 1 + i], {i, 0, n - 1}];
        lab     = ctrLabelText[base, ctrTag];
        With[{ins = inWires, outs = {outWire},
              shape = agentShape[$TagCTR], style = agentStyle[$TagCTR]},
            Diagram[lab, ins, outs, "Shape" -> shape, "Style" -> style]
        ]
    ]

(* Read the ext field of any CTR cell whose val == base.  CTR
   carries its ctor tag in ext on the referencing cell; we only
   have `base` here, so search the heap for any cell holding
   TAG_CTR(base).  Fallback to 0 if missing (shouldn't happen if
   `base` came from agentRule's discovery walk).  Same pattern as
   dupLabelFor[base] above. *)
ctrTagFor[base_Integer] := Block[{lo = THeapBase[], n = THeapPos[], cell},
    cell = SelectFirst[
        Range[lo, n - 1],
        With[{t = THeapRead[#]},
            TTermTag[t] === $TagCTR && TTermVal[t] === base] &,
        None
    ];
    If[cell === None, 0, TTermExt[THeapRead[cell]]]
]

(* MAT(scrut, case-tree).  ext carries the matched ctr tag. *)
matLabelText[base_Integer, ctrTag_Integer] :=
    Column[{"MAT#" <> ToString[ctrTag],
            "@" <> ToString[base]}, Center, Spacings -> 0]

heapReadMatCell[base_Integer] := Block[{lo = THeapBase[], n = THeapPos[], cell},
    cell = SelectFirst[
        Range[lo, n - 1],
        With[{t = THeapRead[#]},
            TTermTag[t] === $TagMAT && TTermVal[t] === base] &,
        None
    ];
    If[cell === None, 0, TTermExt[THeapRead[cell]]]
]

agentDiagram[base_Integer, $TagMAT, principal_, _Association, _Association] :=
    With[{outWire = If[ principal === None,
                        "p" <> ToString[base],
                        wireFor[principal]],
          ctrTag  = heapReadMatCell[base]},
        With[{
            label   = matLabelText[base, ctrTag],
            inputs  = {wireFor[base], wireFor[base + 1]},
            outputs = {outWire},
            shape   = agentShape[$TagMAT], style = agentStyle[$TagMAT]
        },
            Diagram[label, inputs, outputs, "Shape" -> shape, "Style" -> style]
        ]
    ]

(* OP2(lhs, rhs) -- ext = op code (+ - * == <). *)
op2LabelText[base_Integer, opcode_Integer] :=
    Column[{"OP2 " <> Lookup[$op2Names, opcode, "?" <> ToString[opcode]],
            "@" <> ToString[base]}, Center, Spacings -> 0]

heapReadOp2Cell[base_Integer] := Block[{lo = THeapBase[], n = THeapPos[], cell},
    cell = SelectFirst[
        Range[lo, n - 1],
        With[{t = THeapRead[#]},
            TTermTag[t] === $TagOP2 && TTermVal[t] === base] &,
        None
    ];
    If[cell === None, 0, TTermExt[THeapRead[cell]]]
]

agentDiagram[base_Integer, $TagOP2, principal_, _Association, _Association] :=
    With[{outWire = If[ principal === None,
                        "p" <> ToString[base],
                        wireFor[principal]],
          opcode  = heapReadOp2Cell[base]},
        With[{
            label   = op2LabelText[base, opcode],
            inputs  = {wireFor[base], wireFor[base + 1]},
            outputs = {outWire},
            shape   = agentShape[$TagOP2], style = agentStyle[$TagOP2]
        },
            Diagram[label, inputs, outputs, "Shape" -> shape, "Style" -> style]
        ]
    ]

(* DSU(lab, a, b) -- dynamic-label SUP.  Three top inputs (label
   strict, a, b); principal output flows to whatever cell holds
   this DSU.  Label as the leftmost slot reads naturally as "the
   thing being computed first". *)
agentDiagram[base_Integer, $TagDSU, principal_, _Association, _Association] :=
    With[{outWire = If[ principal === None,
                        "p" <> ToString[base],
                        wireFor[principal]]},
        With[{
            label   = Column[{"DSU", "@" <> ToString[base]}, Center, Spacings -> 0],
            inputs  = {wireFor[base], wireFor[base + 1], wireFor[base + 2]},
            outputs = {outWire},
            shape   = agentShape[$TagDSU], style = agentStyle[$TagDSU]
        },
            Diagram[label, inputs, outputs, "Shape" -> shape, "Style" -> style]
        ]
    ]

(* DDU(lab, val, body) -- dynamic-label DUP.  Same shape as DSU
   but on the DUP side; principal incoming at the apex. *)
agentDiagram[base_Integer, $TagDDU, principal_, _Association, _Association] :=
    With[{outWire = If[ principal === None,
                        "p" <> ToString[base],
                        wireFor[principal]]},
        With[{
            label   = Column[{"DDU", "@" <> ToString[base]}, Center, Spacings -> 0],
            inputs  = {wireFor[base], wireFor[base + 1], wireFor[base + 2]},
            outputs = {outWire},
            shape   = agentShape[$TagDDU], style = agentStyle[$TagDDU]
        },
            Diagram[label, inputs, outputs, "Shape" -> shape, "Style" -> style]
        ]
    ]

(* === REF / NUM leaves: rendered per-reference, like CONST.  The
   leaf cell sits in some parent slot; we render a yellow disk (REF)
   or gray disk (NUM) bound to the slot's wire so a network of
   matchings shows the bookref / scalar leaf at the right slot. *)

refLabelText[defId_Integer] := "REF\nd" <> ToString[defId]
numLabelText[loc_Integer]   := With[{t = THeapRead[loc]},
    "NUM\n" <> ToString[TTermVal[t]] <> "@" <> ToString[loc]
]

(* Substituted-binder leaf: a LAM cell whose body is SUB-flagged is
   logically dead (its binder has been beta-substituted out).  We
   drop the LAM agent from the diagram and replace its binder wire
   (`var<binder>`) with a leaf carrying the substituted value -- so
   VAR cells in surviving slots still have something to join to,
   showing the literal that the variable was substituted with. *)
deadLamBinderLabel[binder_Integer] := Block[
    {cell = THeapRead[binder], tag, val},
    tag = TTermTag[cell]; val = TTermVal[cell];
    Switch[tag,
        $TagNUM, "NUM\n" <> ToString[val] <> "@" <> ToString[binder],
        $TagERA, "ERA",
        _,       TTagName[tag] <> "@" <> ToString[binder]
    ]
]

deadLamBinderLeaf[binder_Integer] := Diagram[
    deadLamBinderLabel[binder], {}, {SuperStar["var" <> ToString[binder]]},
    "Shape" -> agentShape[$TagNUM],
    "Style" -> agentStyle[$TagNUM]
]

leafSideAndPolarity[loc_Integer, agents_Association, opcodes_Association] := Block[{
    owner, side, dualedQ
},
    owner = locOwner[loc, agents, opcodes];
    If[ owner === None,
        side = "Bottom"; dualedQ = False
    ,
        side = If[ slotSide[owner[[2]], owner[[3]]] === "Top",
                   "Bottom", "Top" ];
        dualedQ = slotIsDualed[owner[[2]], owner[[3]]]
    ];
    {side, dualedQ}
]

refLeafDiagram[loc_Integer, agents_Association, opcodes_Association] := Block[{
    t, defId, sd, side, dualedQ, name, port, label
},
    t       = THeapRead[loc];
    defId   = TTermExt[t];
    sd      = leafSideAndPolarity[loc, agents, opcodes];
    side    = sd[[1]]; dualedQ = sd[[2]];
    name    = wireFor[loc];
    port    = If[dualedQ, SuperStar[name], name];
    label   = refLabelText[defId];
    With[{
        ins  = If[side === "Top",    {port}, {}],
        outs = If[side === "Bottom", {port}, {}]
    },
        Diagram[label, ins, outs,
            "Shape" -> agentShape[$TagREF], "Style" -> agentStyle[$TagREF]
        ]
    ]
]

numLeafDiagram[loc_Integer, agents_Association, opcodes_Association] := Block[{
    sd, side, dualedQ, name, port, label
},
    sd      = leafSideAndPolarity[loc, agents, opcodes];
    side    = sd[[1]]; dualedQ = sd[[2]];
    name    = wireFor[loc];
    port    = If[dualedQ, SuperStar[name], name];
    label   = numLabelText[loc];
    With[{
        ins  = If[side === "Top",    {port}, {}],
        outs = If[side === "Bottom", {port}, {}]
    },
        Diagram[label, ins, outs,
            "Shape" -> agentShape[$TagNUM], "Style" -> agentStyle[$TagNUM]
        ]
    ]
]

(* Standalone NUM atom: a TTerm whose tag is NUM and val carries the
   integer.  No heap cell of its own (the value is packed directly
   into the Term word).  Render as a one-port leaf so reductions that
   end on a literal -- e.g. (\x.x+1) 5 collapsing to NUM[6] -- still
   have a picture instead of an empty DiagramNetwork. *)
numAtomLeafDiagram[val_Integer] := With[
    {label = "NUM\n" <> ToString[val],
     wire  = "natom" <> ToString[val]},
    Diagram[label, {}, {wire},
        "Shape" -> agentShape[$TagNUM],
        "Style" -> agentStyle[$TagNUM]
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
        tag === $TagDUP || tag === $TagUOP ||
        tag === $TagALO || tag === $TagCTR ||
        tag === $TagMAT || tag === $TagOP2 ||
        tag === $TagDSU || tag === $TagDDU,
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

discoverAgentsHere[seedTerms_List] := Block[{lo = THeapBase[], n = THeapPos[], terms},
    terms = Join[seedTerms, Table[THeapRead[loc], {loc, lo, n - 1}]];
    Association[agentRule /@ terms]
]

discoverUopOpcodesHere[seedTerms_List] := Block[{lo = THeapBase[], n = THeapPos[], terms},
    terms = Join[seedTerms, Table[THeapRead[loc], {loc, lo, n - 1}]];
    Association[uopOpcodeRule /@ terms]
]

(* BFS the heap from `seedTerms` forward, returning only those agents
   transitively reachable through their slot cells.  Mirror of
   `reachableICAgents` in Visualization.wl but using the
   diagram's agentRule shape (base -> tag) and reusing the
   `agentChildSlots` walker (defined in Visualization.wl; shared
   THVMLink`Private context).  Restricting THeapDiagram[term] to
   the reachable closure of `term` keeps stale pre-WNF cells, prior
   constructions, and unrelated heap garbage out of the diagram, so
   the wire names of independent agents never get mixed up. *)
(* A LAM whose body cell at base is SUB-flagged was consumed by a
   prior APP-LAM beta -- the LAM is fully dead and rendering it
   would show a ghost with the substituted literal dangling from
   its body slot.  DUPs are NOT in this category: their SUB-flagged
   body is the *other projection's* value waiting to be consumed,
   so the DUP and that value must stay visible in the diagram. *)
agentRuleIsDead[base_Integer, tag_Integer] :=
    tag === $TagLAM && TTermSub[THeapRead[base]] === 1

reachableAgentsHere[seedTerms_List] := Block[
    {result = <||>, queue = seedTerms, t, rule, base, tag},
    While[ Length[queue] > 0,
        t    = First[queue]; queue = Rest[queue];
        rule = agentRule[t];
        If[ rule =!= Nothing,
            base = First[rule]; tag = Last[rule];
            If[ ! agentRuleIsDead[base, tag] && ! KeyExistsQ[result, base],
                result[base] = tag;
                queue = Join[queue, THeapRead /@ agentChildSlots[t]]]]
    ];
    result
]

(* Slot cells holding a VAR whose binder (heap[val]) is SUB-flagged.
   Each unique binder loc gets one synthesized leaf (above) -- DC
   spider-joins it with every VAR cell that names the same wire. *)
deadLamBinders[agents_Association, opcodes_Association] := DeleteDuplicates @
    Cases[
        agentSlotsOf[agents, opcodes],
        slot_Integer /; With[{t = THeapRead[slot]},
            TTermTag[t] === $TagVAR && TTermSub[THeapRead[TTermVal[t]]] === 1
        ] :> TTermVal[THeapRead[slot]]
    ]

reachableUopOpcodesHere[seedTerms_List] := Block[
    {result = <||>, seen = <||>, queue = seedTerms, t, rule, base, op},
    While[ Length[queue] > 0,
        t    = First[queue]; queue = Rest[queue];
        rule = uopOpcodeRule[t];
        If[ rule =!= Nothing,
            base = First[rule]; op = Last[rule];
            If[ ! KeyExistsQ[result, base], result[base] = op]];
        rule = agentRule[t];
        If[ rule =!= Nothing,
            base = First[rule];
            (* Visited-set on the descent side: agentRule's `base` is
               the heap loc of the compound, and an identity-style
               λx.x (heap[loc] = VAR(loc)) would otherwise re-enqueue
               itself forever. *)
            If[ ! KeyExistsQ[seen, base],
                seen[base] = True;
                queue = Join[queue, THeapRead /@ agentChildSlots[t]]]]
    ];
    result
]

discoverErasHere[] := Block[{lo = THeapBase[], n = THeapPos[]},
    Select[Range[lo, n - 1], TTermTag[THeapRead[#]] === $TagERA &]
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

(* Wire-bearing slot offsets per agent tag.  Skips metadata slots:
   CTR's leading NUM(arity), ALO's trailing NUM(state).  UOP / kernel
   slots come from walkArity. *)
wireSlots[base_Integer, $TagLAM] := {0}
wireSlots[base_Integer, $TagDUP] := {0}
wireSlots[base_Integer, $TagAPP] := {0, 1}
wireSlots[base_Integer, $TagSUP] := {0, 1}
wireSlots[base_Integer, $TagMAT] := {0, 1}
wireSlots[base_Integer, $TagOP2] := {0, 1}
wireSlots[base_Integer, $TagALO] := {0}                (* slot 1 = state metadata *)
wireSlots[base_Integer, $TagCTR] :=
    Range[1, TTermVal[THeapRead[base]]]                (* skip arity NUM at slot 0 *)
wireSlots[base_Integer, $TagDSU] := {0, 1, 2}          (* lab, a, b *)
wireSlots[base_Integer, $TagDDU] := {0, 1, 2}          (* lab, val, body *)
wireSlots[base_Integer, $TagUOP] := {}                  (* handled via reachOps separately *)
wireSlots[___] := {}

(* Walk every agent's wire slots across the agents map; for each
   slot cell that matches `pred`, return the cell loc.  Used to find
   REF / NUM leaves embedded in lazy-term slots. *)
agentWireSlotCells[agents_Association, pred_] := Catenate[
    KeyValueMap[
        Function[{base, tag},
            With[{offsets = wireSlots[base, tag]},
                Select[(base + #) & /@ offsets, pred[THeapRead[#]] &]
            ]
        ],
        agents
    ]
]

(* Synthetic TEN leaf for a kernel input tid that has no
   corresponding TAG_TEN cell in any rendered UOP slot.  The wire
   "tenTid<tid>" is the same one the kernel's input port uses, so
   DiagramNetwork connects them.  Apex-down, label = "TEN#<tid>". *)
externalKernelInputLeaf[tid_Integer] := Diagram[
    "TEN#" <> ToString[tid],
    {},
    {"tenTid" <> ToString[tid]},
    "Shape" -> agentShape[$TagTEN],
    "Style" -> agentStyle[$TagTEN]
]

(* THeapDiagram[]      -- render every agent on the heap
   THeapDiagram[t]     -- render only the agents reachable from `t`
   THeapDiagram[{...}] -- render the union of agents reachable from
                          each seed term

   Reachable-only mode (the seeded forms) is the default for inspecting
   a specific value -- stale cells from prior reductions never leak
   into the wire-name space.  The no-arg form keeps the legacy
   "everything on the heap" behaviour as an escape hatch. *)
THeapDiagram[]                       := iThvmHeapDiagram[{},  All]
THeapDiagram[ts : {___}]             := iThvmHeapDiagram[ts,  "Reachable"]
THeapDiagram[t_]                     := iThvmHeapDiagram[{t}, "Reachable"]

iThvmHeapDiagram[seeds_List, mode_] := Block[{
    fullAgents, reachOps, allKernels, kernelKids, allInputTids,
    coveredTids, externalInputTids, agents, opcodes, eras, tens,
    consts, refs, nums, atomSeeds, ds, $uopOpcodeContext
},
    {fullAgents, reachOps, $uopOpcodeContext} = If[ mode === "Reachable",
        {reachableAgentsHere[seeds],
         reachableUopsHere[seeds],
         reachableUopOpcodesHere[seeds]},
        {discoverAgentsHere[seeds],
         reachableUopsHere[seeds],
         discoverUopOpcodesHere[seeds]}
    ];
    (* UOP_KERNEL cells store their input tids OUTSIDE the heap (in
       the C-side KERNELS table), so non-sink kernels are unreachable
       via the BFS even though they're alive in the heap.  Pull every
       KERNEL UOP from the full opcode discovery so the diagram
       surfaces the entire kernel population, not just the sink. *)
    allKernels = Select[Keys[$uopOpcodeContext],
                        $uopOpcodeContext[#] === $UopKernel &];
    reachOps = Join[reachOps,
                    Association[(# -> $UopKernel) & /@ allKernels]];
    (* Keep IC agents from the discovery; replace UOP entries with
       only the reachable ones so old pre-rewrite UOPs (plus their
       transitively-reached TENs) drop out of the diagram. *)
    agents = Join[
        KeySelect[fullAgents, fullAgents[#] =!= $TagUOP &],
        Association[(# -> $TagUOP) & /@ Keys[reachOps]]
    ];
    opcodes = reachOps;
    (* ERAs: full-heap walk in no-arg mode; in reachable mode, keep
       only those sitting inside a wire slot of a reachable agent
       (otherwise stale ERAs from prior reductions sneak in and add
       dangling wires). *)
    eras = If[ mode === "Reachable",
        agentWireSlotCells[agents, TTermTag[#] === $TagERA &],
        discoverErasHere[]];
    tens    = reachableTenCells[reachOps];
    consts  = reachableConstCells[reachOps];
    (* REF / NUM leaves embedded in wire slots of any rendered IC /
       CTR / MAT / OP2 / ALO agent.  Per-reference: each occurrence
       gets its own disk leaf bound to that slot's wire. *)
    refs    = agentWireSlotCells[agents, TTermTag[#] === $TagREF &];
    nums    = agentWireSlotCells[agents, TTermTag[#] === $TagNUM &];
    (* Synthetic external-input leaves for kernel input_tids that
       aren't produced by another kernel and aren't sitting in any
       rendered TAG_TEN cell.  These are weights / TTensorCreate
       inputs the kernel reads externally. *)
    kernelKids = TTermVal[THeapRead[# + 1]] & /@ allKernels;
    allInputTids = DeleteDuplicates @ Flatten[TKernelInputs /@ kernelKids];
    coveredTids = DeleteDuplicates @ Join[
        TTermVal[THeapRead[#]] & /@ tens,
        TTermVal[THeapRead[# + 0]] & /@ allKernels   (* output_buf tids *)
    ];
    externalInputTids = Complement[allInputTids, coveredTids];
    (* Standalone NUM atom seeds (e.g. the NUM[6] left after fully
       reducing (\x.x+1) 5) carry no heap loc, so they're invisible
       to the agent BFS.  Surface them as one-port leaves so the
       network isn't empty. *)
    atomSeeds = If[ mode === "Reachable",
        Select[seeds, TTermTag[#] === $TagNUM &],
        {}];
    (* Dead LAM binders: filter agents to drop SUB-flagged LAMs/DUPs,
       then collect the binder locs of surviving VAR slots so the
       leaf-attached-to-wire trick fills the picture. *)
    agents = KeySelect[agents,
        Function[base, ! agentRuleIsDead[base, agents[base]]]];
    ds = Join[
        KeyValueMap[
            agentDiagram[#1, #2, principalCellOf[#1, #2, agents, opcodes],
                         agents, opcodes] &,
            agents
        ],
        eraDiagram[#, agents, opcodes]      & /@ eras,
        tenLeafDiagram[#, agents, opcodes]   & /@ tens,
        constLeafDiagram[#, agents, opcodes] & /@ consts,
        refLeafDiagram[#, agents, opcodes]   & /@ refs,
        numLeafDiagram[#, agents, opcodes]   & /@ nums,
        externalKernelInputLeaf /@ externalInputTids,
        numAtomLeafDiagram[TTermVal[#]] & /@ atomSeeds,
        deadLamBinderLeaf /@ deadLamBinders[agents, opcodes]
    ];
    DiagramNetwork @@ ds
]

End[];
EndPackage[];
