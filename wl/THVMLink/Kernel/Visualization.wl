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
   pointer to its source) but matches what readers want: the
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

BeginPackage["WolframInstitute`THVMLink`", {"GeneralUtilities`"}];

SetUsage[THeapGraph, "THeapGraph[] renders the whole heap as a Graph: every IC agent, UOP cell and TEN handle becomes a vertex, with edges following data flow (sources point at consumers).
THeapGraph[term$] seeds the walk from term$, including only the agents it reaches.
THeapGraph[{t$1, t$2, $$}] seeds the walk from several terms.
UOP_KERNEL cells expose their TKernelInputs as input edges, so a fused view shows the same kernel-DAG topology as TScheduleGraph; styling matches the other renderers.
Options: \"ShowEdgeLabels\" (default False), plus all standard Graph options."];

SetUsage[TScheduleGraph, "TScheduleGraph[] returns a Graph of the live kernel schedule: one vertex per emitted kernel, with directed edges from producer kernel to consumer kernel labeled by the connecting TenDesc id.
External inputs (TenDescs with no producer kernel, such as weights and host tensors) appear as TEN-shaped vertices when \"ShowExternalInputs\" is True (default); disconnected kernels render as isolated vertices.
Options: \"ShowExternalInputs\", plus all standard Graph options."];

SetUsage[TMemoryPlanGantt, "TMemoryPlanGantt[plan$] returns a Gantt-style Graphics chart of buffer lifecycles for a TMemoryPlan, with topological depth on the kernel DAG along x and one packed row per buffer.
Each bar spans the buffer's live depth range, colored by status, with hover tooltips exposing buf id, nbytes, dtype, status, depths and alias tids.
Options: \"TopN\" (largest buffers to show), \"BarHeight\" (\"Linear\" or \"Log\")."];

Begin["`Private`"];

(* Forward refs to private symbols owned by Style.wl + Kernel.wl;
   they share the WolframInstitute`THVMLink`Private` context but the alphabetical
   load order means they may not exist yet at first parse. *)
{drawNode, nodeShapeFn, edgeStyleDirective, styleFor};

(* === vertex id constructors === *)
icVertexId[base_Integer] := "a" <> ToString[base]
eraVertexId[loc_Integer] := "e" <> ToString[loc]
uopVertexId[loc_Integer] := "u" <> ToString[loc]
kerVertexId[loc_Integer] := "k" <> ToString[loc]
tenVertexId[id_Integer] := "t" <> ToString[id]
ctrVertexId[base_Integer] := "c" <> ToString[base]
matVertexId[base_Integer] := "m" <> ToString[base]
op2VertexId[base_Integer] := "p" <> ToString[base]
aloVertexId[base_Integer] := "o" <> ToString[base]
refVertexId[defId_Integer] := "r" <> ToString[defId]
numVertexId[loc_Integer] := "n" <> ToString[loc]
dsuVertexId[base_Integer] := "y" <> ToString[base]   (* dyn SUP *)
dduVertexId[base_Integer] := "z" <> ToString[base]   (* dyn DUP *)

(* === IC port specs (offset, port name) === *)
icPorts[$TagLAM] := {{0, "body"}}
icPorts[$TagAPP] := {{0, "f"}, {1, "x"}}
icPorts[$TagSUP] := {{0, "L"}, {1, "R"}}
icPorts[$TagDUP] := {{0, "body"}}

(* CTR ports: heap[val] = NUM(arity), heap[val+1..val+n] = children.
   Variable arity, computed from the NUM cell at val. *)
ctrPorts[base_Integer] := With[{n = TTermVal[THeapRead[base]]},
    Table[{i, "c" <> ToString[i - 1]}, {i, n}]
]

(* MAT ports: heap[val+0] = scrutinee, heap[val+1] = case-tree LAM. *)
matPorts[] := {{0, "s"}, {1, "k"}}

(* OP2 ports: heap[val+0] = lhs, heap[val+1] = rhs. *)
op2Ports[] := {{0, "L"}, {1, "R"}}

(* ALO ports: heap[val] = wrapped book-template term;
   heap[val+1] = NUM(state_id), atomic, surfaced in the label. *)
aloPorts[] := {{0, "body"}}

(* DSU ports: heap[val+0] = label term, heap[val+1] = a, +2 = b. *)
dsuPorts[] := {{0, "lab"}, {1, "a"}, {2, "b"}}

(* DDU ports: heap[val+0] = label term, heap[val+1] = value, +2 = body. *)
dduPorts[] := {{0, "lab"}, {1, "v"}, {2, "bod"}}

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
        $TagREF,
            refVertexId[TTermExt[term]] -> <|"tag" -> $TagREF, "defId" -> TTermExt[term]|>,
        $TagALO,
            aloVertexId[TTermVal[term]] -> <|"tag" -> $TagALO|>,
        $TagCTR,
            ctrVertexId[TTermVal[term]] -> <|"tag" -> $TagCTR, "ext" -> TTermExt[term]|>,
        $TagMAT,
            matVertexId[TTermVal[term]] -> <|"tag" -> $TagMAT, "ext" -> TTermExt[term]|>,
        $TagOP2,
            op2VertexId[TTermVal[term]] -> <|"tag" -> $TagOP2, "opcode" -> TTermExt[term]|>,
        $TagDSU,
            dsuVertexId[TTermVal[term]] -> <|"tag" -> $TagDSU|>,
        $TagDDU,
            dduVertexId[TTermVal[term]] -> <|"tag" -> $TagDDU|>,
        _,
            Nothing
    ]
]

discoverAgents[seedTerms_List : {}] := Block[{lo = THeapBase[], n = THeapPos[], terms},
    terms = Join[seedTerms, Table[THeapRead[loc], {loc, lo, n - 1}]];
    Association[agentFromTerm /@ terms]
]

(* Slot cells to follow when BFS'ing from a compound: each agent
   tag exposes some number of heap children at base+0 .. base+n-1.
   These are the cells the agent points at; their contents are the
   referenced child terms.  Atoms (NUM/ERA/REF/TEN/VAR/DP0/DP1)
   have no follow-up cells (their term word is self-contained),
   so they return {}. *)
agentChildSlots[term_] := With[{tag = TTermTag[term], base = TTermVal[term]},
    Switch[tag,
        $TagLAM | $TagDUP, {base},
        $TagAPP | $TagSUP | $TagMAT | $TagOP2, {base, base + 1},
        $TagALO, {base}, (* slot 1 = state metadata *)
        $TagDSU | $TagDDU, {base, base + 1, base + 2},
        $TagCTR, Range[base + 1, base + TTermVal[THeapRead[base]]],
        $TagVAR, {base}, (* heap[base] = LAM body *)
        $TagDP0 | $TagDP1, {base}, (* heap[base] = DUP body *)
        _, {}
    ]
]

(* Atom-as-seed vertex constructor: a seed term whose tag is an atom
   (NUM, ERA, REF, TEN, ANY) has no heap cell of its own.  We mint a
   distinct vertex id so the graph has at least one node when the
   seed itself is the whole show.  Heap-resident NUM cells keep
   numVertexId; seed atoms use the "natom" / "eraatom" / etc.
   prefixes so they never collide. *)
agentFromAtomSeed[term_] := With[{tag = TTermTag[term], val = TTermVal[term]},
    Switch[tag,
        $TagNUM, "natom" <> ToString[val] -> <|"tag" -> $TagNUM, "value" -> val|>,
        $TagERA, "eraatom" -> <|"tag" -> $TagERA|>,
        $TagREF, refVertexId[TTermExt[term]] -> <|"tag" -> $TagREF, "defId" -> TTermExt[term]|>,
        $TagTEN, tenVertexId[val] -> <|"tag" -> $TagTEN, "dtype" -> TTermExt[term]|>,
        _, Nothing
    ]
]

(* BFS the heap from the seed terms forward, collecting only the
   agents transitively reachable through their slot cells.  Pre-WNF
   stale cells, heap garbage, and unrelated terms left over from
   prior constructions are excluded; the rendered graph then
   matches the user's mental model of "what this term references".
   No-arg THeapGraph[] keeps the full-heap walk; only the seeded
   forms route through here.

   Top-level atom seeds (NUM/ERA/REF/TEN) get a synthetic vertex via
   agentFromAtomSeed so a one-atom seed still has something to draw.
   Transitively-reached atoms (e.g. NUM children of an OP2 or APP)
   stay inlined via the parent's edge to a heap-loc-keyed vertex,
   no double-rendering. *)
(* Dead LAM: body cell SUB-flagged after APP-LAM beta.  Drop the
   wrapper AND the literal in its body slot.

   Dead DUP: drop the wrapper only if the substituted body is a
   compound agent (SUP / LAM / DUP / ...): the compound is
   reachable from the sibling projection's chase, so the wrapper
   adds no information.  If the body is an ATOM (NUM/ERA/TEN/...),
   atoms aren't first-class agents (they render inline on slot
   edges), and the DUP wrapper is the only node that ties both
   projections to the atom.  Keep the wrapper visible in that case. *)
agentIsDead[term_] := Block[{tag = TTermTag[term], body, btag, lam},
    Which[
        tag === $TagLAM,
            TTermSub[THeapRead[TTermVal[term]]] === 1,
        tag === $TagDUP,
            body = THeapRead[TTermVal[term]];
            btag = TTermTag[body];
            TTermSub[body] === 1 && btag =!= $TagNUM && btag =!= $TagERA && btag =!= $TagTEN && btag =!= $TagANY && btag =!= $TagREF,
        tag === $TagAPP,
            (* APP-LAM has fired: left slot is a LAM whose binder
               is SUB-flagged.  The APP wrapper is logically gone. *)
            lam = THeapRead[TTermVal[term]];
            TTermTag[lam] === $TagLAM && TTermSub[THeapRead[TTermVal[lam]]] === 1,
        True, False
    ]
]
agentIsDeadVar[term_] := Block[{tag = TTermTag[term], isVarLike},
    isVarLike = (tag === $TagVAR || tag === $TagDP0 || tag === $TagDP1);
    isVarLike && TTermSub[THeapRead[TTermVal[term]]] === 1
]

reachableICAgents[seedTerms_List] := Block[{result = <||>, queue, t, rule, vid, info, atomRule},
    Do[
        atomRule = agentFromAtomSeed[seed];
        If[ atomRule =!= Nothing,
            {vid, info} = {First[atomRule], Last[atomRule]};
            If[! KeyExistsQ[result, vid], result[vid] = info]],
        {seed, seedTerms}
    ];
    queue = seedTerms;
    While[ Length[queue] > 0,
        t = First[queue]; queue = Rest[queue];
        (* Skip VAR/DP0/DP1 whose binder is SUB-flagged: the
           substituted value is surfaced by childVertexId as an edge
           endpoint, no LAM/DUP agent should be registered. *)
        If[agentIsDeadVar[t], Continue[]];
        rule = agentFromTerm[t];
        If[ rule =!= Nothing,
            {vid, info} = {First[rule], Last[rule]};
            (* Dead DUP / LAM: drop the wrapper but still walk into
               the body so the substituted value (the arg
               substituted for VAR by APP-LAM, the sibling
               projection through a DUP-X firing) gets registered.
               heap[lam_loc] / heap[dup_loc] holds the substituted
               content after the firing, SUB-flagged. *)
            Which[
                (TTermTag[t] === $TagDUP || TTermTag[t] === $TagLAM) && TTermSub[THeapRead[TTermVal[t]]] === 1,
                    queue = Append[queue, THeapRead[TTermVal[t]]],
                agentIsDead[t],
                    Null,
                ! KeyExistsQ[result, vid],
                    result[vid] = info;
                    queue = Join[queue, THeapRead /@ agentChildSlots[t]]
            ]]
    ];
    result
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

(* True iff a VAR/DP0/DP1 cell has been substituted out: the
   binder cell at `val` is SUB-flagged, meaning a prior interaction
   (e.g. APP-LAM beta) wrote the substituted value there.  For
   visualisation purposes we treat the substituted value as the
   logical content of the slot and resolve "through" the dead binder. *)
varIsSubResolved[loc_Integer] := Block[{t = THeapRead[loc], tag, binder},
    tag = TTermTag[t];
    If[ tag === $TagVAR || tag === $TagDP0 || tag === $TagDP1,
        binder = THeapRead[TTermVal[t]];
        TTermSub[binder] === 1,
        False
    ]
]

(* Resolve any cell at `loc` to its source vertex id (or Nothing if
   the cell is something we don't render, which today is just raw
   TAG_NUM children we'd rather surface via the parent's label).
   When the cell is a VAR/DP0/DP1 pointing at a SUB-flagged binder,
   re-route the wire to a vertex keyed on the binder loc holding the
   substituted value (so post-beta diagrams show the substituted
   literal where the variable used to be, instead of a ghost LAM). *)
childVertexId[loc_Integer] := Block[{t = THeapRead[loc], tag, val, ext, binder, btag, varTag, subResolved},
    tag = TTermTag[t]; val = TTermVal[t]; ext = TTermExt[t];
    varTag = (tag === $TagVAR || tag === $TagDP0 || tag === $TagDP1);
    subResolved = varTag && TTermSub[THeapRead[val]] === 1;
    If[ subResolved,
        binder = THeapRead[val];
        btag = TTermTag[binder];
        Switch[btag,
            $TagNUM, numVertexId[val],
            $TagERA, eraVertexId[val],
            _, numVertexId[val]
        ],
        Switch[tag,
            $TagLAM | $TagAPP | $TagSUP | $TagDUP, icVertexId[val],
            $TagVAR, icVertexId[val],
            $TagDP0 | $TagDP1, icVertexId[val],
            $TagERA, eraVertexId[loc],
            $TagUOP,
                If[ext === $UopKernel, kerVertexId[val], uopVertexId[val]],
            $TagTEN, tenVertexId[val],
            $TagREF, refVertexId[ext],
            $TagALO, aloVertexId[val],
            $TagCTR, ctrVertexId[val],
            $TagMAT, matVertexId[val],
            $TagOP2, op2VertexId[val],
            $TagDSU, dsuVertexId[val],
            $TagDDU, dduVertexId[val],
            $TagNUM, numVertexId[loc],
            _, Nothing
        ]
    ]
]

icPortRecord[base_Integer, offset_Integer, port_String] := Block[{loc = base + offset, t = THeapRead[base + offset], tag, src},
    tag = TTermTag[t];
    src = childVertexId[loc];
    If[ src === Nothing, Nothing,
        {src, icVertexId[base],
         port <> Switch[tag,
             $TagVAR, " var",
             $TagDP0, " dp0",
             $TagDP1, " dp1",
             _, ""]}
    ]
]

icEdgeRecords[base_Integer, tag_Integer] := icPortRecord[base, #[[1]], #[[2]]] & /@ icPorts[tag]

(* For non-KERNEL UOPs, walk their compute slots and emit
   src -> uop edges. *)
uopEdgeRecord[loc_Integer, offset_Integer] := Block[{cellLoc = loc + offset, t, tag, val},
    t = THeapRead[cellLoc];
    tag = TTermTag[t];
    val = TTermVal[t];
    Switch[tag,
        $TagUOP,
            {If[TTermExt[t] === $UopKernel, kerVertexId[val], uopVertexId[val]], uopVertexId[loc], "src" <> ToString[offset]},
        $TagTEN,
            {tenVertexId[val], uopVertexId[loc], "src" <> ToString[offset]},
        $TagNUM,
            Nothing, (* numeric arg, surfaced via vertex label *)
        $TagLAM | $TagAPP | $TagSUP | $TagDUP,
            {icVertexId[val], uopVertexId[loc], "src" <> ToString[offset]},
        $TagERA,
            {eraVertexId[cellLoc], uopVertexId[loc], "src" <> ToString[offset]},
        _, Nothing
    ]
]

uopEdgeRecords[loc_Integer, opcode_Integer] /; opcode =!= $UopKernel := Table[uopEdgeRecord[loc, off], {off, 0, uopArity[opcode] - 1}]

(* For UOP_KERNEL cells, the heap children (output_buf TEN, NUM kid)
   describe the OUTPUT, not inputs.  Pull TKernelInputs[kid] from
   the C side and emit one TEN -> KERNEL edge per input tid.  Same
   topology TScheduleGraph builds. *)
kernelEdgeRecords[loc_Integer] := Block[{kid = TTermVal[THeapRead[loc + 1]], outTid = TTermVal[THeapRead[loc]], consVid = kerVertexId[loc], inputs},
    inputs = TKernelInputs[kid];
    Append[
        ({tenVertexId[#], consVid, "in"} & /@ inputs),
        (* output edge: KERNEL -> output TEN, so the TEN appears
           downstream of the kernel that produces it. *)
        {consVid, tenVertexId[outTid], "out"}
    ]
]

uopEdgeRecords[loc_Integer, $UopKernel] := kernelEdgeRecords[loc]

(* Generic edge record for a fixed-arity parent at `base` whose
   children sit at base + offset.  Targets the parent's `vid`. *)
genericPortRecord[parentVid_String, base_Integer, offset_Integer, port_String] := Block[{src = childVertexId[base + offset]},
    If[src === Nothing, Nothing, {src, parentVid, port}]
]

ctrEdgeRecords[base_Integer] := Block[{parentVid = ctrVertexId[base], n = TTermVal[THeapRead[base]]},
    Table[genericPortRecord[parentVid, base, i, "c" <> ToString[i - 1]], {i, n}]
]

matEdgeRecords[base_Integer] := With[{parentVid = matVertexId[base]},
    {
        genericPortRecord[parentVid, base, 0, "scrut"],
        genericPortRecord[parentVid, base, 1, "case"]
    }
]

op2EdgeRecords[base_Integer] := With[{parentVid = op2VertexId[base]},
    {
        genericPortRecord[parentVid, base, 0, "L"],
        genericPortRecord[parentVid, base, 1, "R"]
    }
]

aloEdgeRecords[base_Integer] := With[{parentVid = aloVertexId[base]},
    {genericPortRecord[parentVid, base, 0, "body"]}
]

dsuEdgeRecords[base_Integer] := With[{parentVid = dsuVertexId[base]},
    {
        genericPortRecord[parentVid, base, 0, "lab"],
        genericPortRecord[parentVid, base, 1, "a"],
        genericPortRecord[parentVid, base, 2, "b"]
    }
]

dduEdgeRecords[base_Integer] := With[{parentVid = dduVertexId[base]},
    {
        genericPortRecord[parentVid, base, 0, "lab"],
        genericPortRecord[parentVid, base, 1, "v"],
        genericPortRecord[parentVid, base, 2, "bod"]
    }
]

(* dispatch *)
agentEdgeRecords[vid_String, info_Association] := Switch[info["tag"],
    $TagLAM | $TagAPP | $TagSUP | $TagDUP,
        icEdgeRecords[ToExpression[StringDrop[vid, 1]], info["tag"]],
    $TagUOP,
        uopEdgeRecords[ToExpression[StringDrop[vid, 1]], info["opcode"]],
    $TagCTR, ctrEdgeRecords[ToExpression[StringDrop[vid, 1]]],
    $TagMAT, matEdgeRecords[ToExpression[StringDrop[vid, 1]]],
    $TagOP2, op2EdgeRecords[ToExpression[StringDrop[vid, 1]]],
    $TagALO, aloEdgeRecords[ToExpression[StringDrop[vid, 1]]],
    $TagDSU, dsuEdgeRecords[ToExpression[StringDrop[vid, 1]]],
    $TagDDU, dduEdgeRecords[ToExpression[StringDrop[vid, 1]]],
    _, {}
]

(* === SUB-decoration (dashed outline) === *)
icSubVertices[base_Integer, tag_Integer] := Table[
    With[{loc = base + p[[1]]},
        If[TTermSub[THeapRead[loc]] == 1, icVertexId[base], Nothing]],
    {p, icPorts[tag]}
]

subVerticesForAgent[vid_String, info_Association] := Switch[info["tag"],
    $TagLAM | $TagAPP | $TagSUP | $TagDUP,
        icSubVertices[ToExpression[StringDrop[vid, 1]], info["tag"]],
    _, {}
]

(* === labels === *)

icLabel[base_Integer, tag_Integer] := With[{arity = Length[icPorts[tag]]},
    Column[{TTagName[tag], If[arity > 1, "@" <> ToString[base] <> ".." <> ToString[base + arity - 1], "@" <> ToString[base]]}, Center, Spacings -> 0]
]

(* For KERNEL: surface kid + program op count for at-a-glance read.
   uopHeader (defined in Diagram.wl) returns "KERNEL@<base>#<kid>"
   already, but kid alone is more useful in this view. *)
kernelLabel[loc_Integer] := Block[{kid = TTermVal[THeapRead[loc + 1]]},
    "KERNEL\nk" <> ToString[kid]
]

(* CONST: surface decoded scalar value via scalarTextFromCell from
   Shape.wl, so "CONST" reads as "1.0" / "0" instead of an opaque
   loc.  Many CONST agents in a post-grad heap aren't redundant:
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
   producer_kid in TENS table is 0, a host-side input not produced
   by any kernel; we render it darker so the eye picks weights /
   inputs out of a sea of intermediate TENs. *)
$externalTids := Block[{tens = TTensTable[]},
    Select[Range[Length[tens]], tens[[#, 1]] === 0 &]
]

vertexCategory[vid_String, info_Association] := Switch[info["tag"],
    $TagUOP,
        Switch[info["opcode"],
            $UopKernel, "KERNEL",
            $UopConst, "CONST",
            $UopReduce, "REDUCE",
            $UopGrad, "GRAD",
            _, "UOP"],
    $TagTEN,
        With[{id = ToExpression[StringDrop[vid, 1]]},
            If[MemberQ[$externalTids, id], "ExternalTEN", "TEN"]],
    _, TTagName[info["tag"]]
]

(* Label helpers for the new tag types. *)
ctrLabel[base_Integer, ctrTag_Integer] := Column[{"CTR#" <> ToString[ctrTag], "@" <> ToString[base]}, Center, Spacings -> 0]

matLabel[base_Integer, ctrTag_Integer] := Column[{"MAT#" <> ToString[ctrTag], "@" <> ToString[base]}, Center, Spacings -> 0]

op2Label[base_Integer, opcode_Integer] := Column[{"OP2 " <> Lookup[$op2Names, opcode, "?" <> ToString[opcode]], "@" <> ToString[base]}, Center, Spacings -> 0]

aloLabel[base_Integer] := Block[{stateCell = THeapRead[base + 1]},
    Column[{"ALO", "s" <> ToString[TTermVal[stateCell]] <> "@" <> ToString[base]}, Center, Spacings -> 0]
]

refLabel[defId_Integer] := "REF\nd" <> ToString[defId]

numLabel[loc_Integer] := Block[{t = THeapRead[loc]},
    Column[{ToString[TTermVal[t]], "@" <> ToString[loc]}, Center, Spacings -> 0]
]

(* Atom-NUM vertices come from seed terms (NUM packed in a Term word,
   no heap cell of its own).  The vertex id is "natom" <> value to
   keep it disjoint from heap NUM vertex ids "n" <> loc. *)
numAtomLabel[val_Integer] := ToString[val]

vertexLabel[vid_String, info_Association] := Switch[info["tag"],
    $TagLAM | $TagAPP | $TagSUP | $TagDUP,
        icLabel[ToExpression[StringDrop[vid, 1]], info["tag"]],
    $TagUOP,
        uopLabel[ToExpression[StringDrop[vid, 1]], info["opcode"]],
    $TagTEN,
        tenLabel[ToExpression[StringDrop[vid, 1]], info["dtype"]],
    $TagREF, refLabel[info["defId"]],
    $TagALO, aloLabel[ToExpression[StringDrop[vid, 1]]],
    $TagCTR, ctrLabel[ToExpression[StringDrop[vid, 1]], info["ext"]],
    $TagMAT, matLabel[ToExpression[StringDrop[vid, 1]], info["ext"]],
    $TagOP2, op2Label[ToExpression[StringDrop[vid, 1]], info["opcode"]],
    $TagDSU, Column[{"DSU", "@" <> ToString[ToExpression[StringDrop[vid, 1]]]}, Center, Spacings -> 0],
    $TagDDU, Column[{"DDU", "@" <> ToString[ToExpression[StringDrop[vid, 1]]]}, Center, Spacings -> 0],
    $TagNUM,
        If[StringStartsQ[vid, "natom"], numAtomLabel[ToExpression[StringDrop[vid, 5]]], numLabel[ToExpression[StringDrop[vid, 1]]]],
    _, ""
]

(* === public entry === *)

Options[THeapGraph] = Join[{"ShowEdgeLabels" -> False}, Options[Graph]];

(* ERA locs that appear in some slot of one of `agents`; gives the
   reachable-mode entry points the ERA list they need to render ERA
   vertices with the correct shape (the no-arg path uses
   discoverEras[] which walks the full heap). *)
icAgentSpan[vid_String, info_Association] := With[{tag = info["tag"], base = ToExpression[StringDrop[vid, 1]]},
    Which[
        tag === $TagLAM || tag === $TagDUP, {base},
        tag === $TagAPP || tag === $TagSUP || tag === $TagMAT || tag === $TagOP2 || tag === $TagALO, {base, base + 1},
        tag === $TagDSU || tag === $TagDDU, {base, base + 1, base + 2},
        tag === $TagCTR, Range[base + 1, base + TTermVal[THeapRead[base]]],
        True, {}
    ]
]

erasReachableFrom[agents_Association] := DeleteDuplicates @ Cases[
    Catenate[KeyValueMap[icAgentSpan, agents]],
    loc_Integer /; TTermTag[THeapRead[loc]] === $TagERA
]

THeapGraph[opts : OptionsPattern[]] := buildHeapGraph[discoverAgents[{}], discoverEras[], opts]
THeapGraph[ts_List, opts : OptionsPattern[]] := With[{agents = reachableICAgents[ts]},
    buildHeapGraph[agents, erasReachableFrom[agents], opts]]
THeapGraph[term_, opts : OptionsPattern[]] := With[{agents = reachableICAgents[{term}]},
    buildHeapGraph[agents, erasReachableFrom[agents], opts]]

buildHeapGraph[agents_Association, eras_List, userOpts : OptionsPattern[THeapGraph]] := Block[{edgeRecords, edges, edgeLabels, vertices, vshapes, subVertices, layout, showLabels},
    showLabels = OptionValue["ShowEdgeLabels"];
    edgeRecords = Flatten[KeyValueMap[agentEdgeRecords, agents], 1];
    (* Tag each edge with its port name so DirectedEdge[a, b, "src0"]
       and DirectedEdge[a, b, "src1"] are distinct: a UOP reading
       the same TEN through two slots gets two parallel arrows.  The
       caller can hide labels via "ShowEdgeLabels" -> False (default)
       but the tag still keeps Graph from collapsing the edges. *)
    edges = DirectedEdge[#[[1]], #[[2]], #[[3]]] & /@ edgeRecords;
    edgeLabels = MapThread[Rule, {edges, Last /@ edgeRecords}];

    vertices = DeleteDuplicates @ Join[
        Keys[agents],
        eraVertexId /@ eras,
        (* Surface every vertex referenced as an edge source or
           target: a kernel input may name a TenDesc whose
           TAG_TEN doesn't appear in any walked heap cell (the
           original referencing UOP got rewritten to a kernel and
           is now orphaned).  Without this, the Graph would
           silently drop edges to those tids. *)
        edges /. DirectedEdge[a_, b_, ___] :> Sequence[a, b]
    ];

    subVertices = Flatten[KeyValueMap[subVerticesForAgent, agents]];

    (* Build a (vid -> info) map that includes any synthesized
       vertices added via edge endpoints (TEN handles whose
       TAG_TEN cell isn't in the walked heap, NUM children of
       compound parents, etc).  Prefix dispatches to the appropriate
       tag stub. *)
    Module[{augmented = agents},
        Do[
            If[ ! KeyExistsQ[augmented, v],
                Switch[StringTake[v, 1],
                    "t", augmented[v] = <|"tag" -> $TagTEN, "dtype" -> 0|>,
                    "n", augmented[v] = <|"tag" -> $TagNUM|>,
                    "r", augmented[v] = <|"tag" -> $TagREF, "defId" -> ToExpression[StringDrop[v, 1]]|>,
                    _, Null]],
            {v, vertices}
        ];
        vshapes = Join[
            KeyValueMap[{vid, info} |-> vid -> nodeShapeFn[vertexCategory[vid, info], vertexLabel[vid, info]], augmented],
            (eraVertexId[#] -> nodeShapeFn["ERA", ""]) & /@ eras
        ]
    ];

    layout = Replace[OptionValue[GraphLayout], Automatic -> "LayeredDigraphEmbedding"];

    Graph[vertices, edges,
        FilterRules[{userOpts}, Except[GraphLayout | VertexSize | "ShowEdgeLabels"]],
        VertexShapeFunction -> vshapes,
        VertexSize -> 0.45,
        EdgeLabels -> If[showLabels, Map[#[[1]] -> Placed[#[[2]], 0.5] &, edgeLabels], None],
        EdgeLabelStyle -> Directive[FontFamily -> "Helvetica", FontSize -> 9, LightDarkSwitched[Black, White]],
        EdgeStyle -> edgeStyleDirective,
        GraphLayout -> layout,
        PerformanceGoal -> "Quality",
        ImagePadding -> 25
    ]
]

(* === TScheduleGraph - DAG-of-kernels view ===

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
   isolated vertices in the layered embedding; they're still
   part of the schedule, they just don't share TenDescs with the
   rest. *)

(* Forward refs to private symbols owned by Kernel.wl + Style.wl;
   they share WolframInstitute`THVMLink`Private` with this file so resolution is
   load-order-agnostic via SetDelayed. *)
{decodeKernelInfo, kernelRowAsoc, tenTermFromTid};

(* Compose a short per-kernel label.  Two lines:
     kid, op count
     output shape and dtype (or "no out" for the rare bail). *)
scheduleKernelLabel[kid_Integer] := Block[{info = decodeKernelInfo[kid], row = kernelRowAsoc[kid], shape},
    shape = With[{tid = row["OutputTid"]},
        If[tid > 0, TTensorShape[tenTermFromTid[tid]], {}]];
    "k" <> ToString[kid] <> " (" <> ToString[info[[1]]["OpCount"]] <> "ops)" <> "\n" <> ToString[shape] <> " " <> row["OutputDtype"]
]

scheduleExternalLabel[tid_Integer] := Block[{shape = TTensorShape[tenTermFromTid[tid]], dtype = TTensorDType[tenTermFromTid[tid]]},
    "t" <> ToString[tid] <> "\n" <> ToString[shape] <> " " <> If[StringQ[dtype], dtype, "?"]
]

Options[TScheduleGraph] = Join[{"ShowExternalInputs" -> True}, Options[Graph]];

TScheduleGraph[opts : OptionsPattern[]] := Block[{nKernels = TKernelCount[] - 1, showExt = OptionValue["ShowExternalInputs"], tens, kernelVerts, edges, edgeLabels, extTids, extVerts, vshapes},
    If[nKernels <= 0, Return[Graph[{}, ImageSize -> 240]]];
    tens = TTensTable[];

    kernelVerts = Table["k" <> ToString[k], {k, nKernels}];

    (* For each kernel, walk its input_tids and emit one edge per
       tid back to the producing kernel (or to an external-TEN
       vertex if there's no producer).  Edge label is the tid. *)
    {edges, edgeLabels, extTids} = Block[{es = {}, ls = {}, exs = {}},
        Do[
            Block[{inTids = TKernelInputs[kid], consumerVert = "k" <> ToString[kid], producerKid, srcVert},
                Do[
                    producerKid = If[tid > 0 && tid <= Length[tens], tens[[tid, 1]], 0];
                    srcVert = If[producerKid =!= 0, "k" <> ToString[producerKid], "t" <> ToString[tid]];
                    If[producerKid === 0, AppendTo[exs, tid]];
                    AppendTo[es, DirectedEdge[srcVert, consumerVert]];
                    AppendTo[ls, DirectedEdge[srcVert, consumerVert] -> tid],
                    {tid, inTids}
                ]
            ],
            {kid, nKernels}
        ];
        {es, ls, DeleteDuplicates[exs]}
    ];

    extVerts = If[showExt, "t" <> ToString[#] & /@ extTids, {}];
    (* Drop edges pointing into hidden external vertices, otherwise
       Graph would auto-add stub vertices for them. *)
    If[ ! showExt,
        edges = Select[edges, ! StringStartsQ[#[[1]], "t"] &];
        edgeLabels = Select[edgeLabels, ! StringStartsQ[#[[1, 1]], "t"] &]
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
        VertexSize -> 0.45,
        EdgeLabelStyle -> Directive[FontFamily -> "Helvetica", FontSize -> 9, LightDarkSwitched[Black, White]],
        EdgeStyle -> edgeStyleDirective,
        GraphLayout -> "LayeredDigraphEmbedding",
        PerformanceGoal -> "Quality"
    ]
]

(* === TMemoryPlanGantt - buffer-lifecycle Gantt chart ===

   Renders a packed-strip view of a TMemoryPlan snapshot.  The
   plan-construction logic + helpers (formatBytes, statusFill /
   statusEdge, peakConcurrentLive, linearScanPack, backendsActive)
   live in MemoryPlan.wl.  This file owns just the Graphics
   composition since it's the visual / option surface; the
   numerical work that backs each card is shared with
   TMemoryPlanReport. *)

(* Forward refs to MemoryPlan.wl + Style.wl helpers (WolframInstitute`THVMLink`Private`
   is shared across all sibling files). *)
{linearScanPack, peakConcurrentLive, statusFill, statusEdge,
 formatBytes, backendsActive};

Options[TMemoryPlanGantt] = {"TopN" -> 40, "BarHeight" -> "Linear"};
TMemoryPlanGantt[TMemoryPlan[a_Association], opts : OptionsPattern[]] := Block[{allBufs = a["Bufs"], bufs, topN, barHeightMode, packed, maxDepth, totalBytes, peak, savingsPct, totalHeight, omittedCount, legendStatuses},
    topN = OptionValue["TopN"];
    barHeightMode = OptionValue["BarHeight"];
    If[allBufs === {}, Return[Graphics[{}, ImageSize -> 320]]];
    bufs = If[ MatchQ[topN, _Integer] && topN > 0 && Length[allBufs] > topN,
        TakeLargestBy[allBufs, #["nbytes"] &, topN],
        allBufs
    ];
    omittedCount = Length[allBufs] - Length[bufs];
    maxDepth = Max[#["last_use_depth"] & /@ allBufs];
    totalBytes = Total[#["nbytes"] & /@ allBufs];
    peak = peakConcurrentLive[allBufs];
    savingsPct = If[ peak["total_bytes"] > 0,
        Round[100. (peak["total_bytes"] - peak["peak_bytes"]) / peak["total_bytes"], 0.1],
        0
    ];
    {packed, totalHeight} = linearScanPack[bufs, barHeightMode];
    If[totalHeight === 0, totalHeight = 1];
    legendStatuses = DeleteDuplicates[#["status"] & /@ packed];
    Graphics[
        {
            b |-> Block[{y0 = b["y_range"][[1]], y1 = b["y_range"][[2]], h, inset, x0, x1},
                h = y1 - y0;
                inset = 0.05 h;
                x0 = b["alloc_depth"];
                x1 = If[ b["status"] === "Preserved",
                    maxDepth + 1,
                    b["last_use_depth"] + 1
                ];
                {
                    FaceForm[statusFill[b["status"]]],
                    EdgeForm[Directive[
                        statusEdge[b["status"]],
                        Thickness[0.0015]
                    ]],
                    Tooltip[
                        Rectangle[
                            {x0, y0 + inset},
                            {x1, y1 - inset},
                            RoundingRadius -> 0.06
                        ],
                        Column[{
                            Row[{"buf ", b["id"], " (", If[b["backend"] === 1, "CPU", If[b["backend"] === 2, "Metal", "?"]], ")"}],
                            Row[{"nbytes: ", formatBytes[b["nbytes"]]}],
                            Row[{"dtype: ", b["dtype"]}],
                            Row[{"status: ", b["status"]}],
                            Row[{"depth: ", b["alloc_depth"], " .. ", b["last_use_depth"]}],
                            Row[{"producer kid: ", b["producer_kid"]}],
                            Row[{"consumer kids: ", b["consumer_kids"]}],
                            Row[{"alias tids: ", b["alias_tids"]}]
                        }]
                    ],
                    If[ h > 0.05 totalHeight,
                        Text[
                            Style[
                                Column[{
                                    Row[{"buf", b["id"], " kid", b["producer_kid"], " ", formatBytes[b["nbytes"]]}],
                                    Row[{"tid:", StringRiffle[ToString /@ b["alias_tids"], ","]}]
                                }, ItemSize -> Automatic, Spacings -> 0],
                                FontSize -> Scaled[0.013],
                                FontFamily -> "Source Code Pro"
                            ],
                            {(x0 + x1)/2, (y0 + y1)/2}
                        ],
                        Sequence @@ {}
                    ]
                }
            ] /@ packed
        },
        Frame -> True,
        FrameTicks -> {
            Automatic,
            Block[{step, label, isLinear},
                isLinear = barHeightMode =!= "Log";
                label = If[isLinear, formatBytes, y |-> ToString[Round[y, 0.1]]];
                step = totalHeight / 6.0;
                If[step <= 0, step = 1];
                Table[{y, label[y]}, {y, 0, totalHeight, step}]
            ]
        },
        FrameLabel -> {
            "topological depth (kernel DAG)",
            If[barHeightMode === "Log", "stacked Log2[1+nbytes], linear-scan slot allocator", "memory (KiB), linear-scan slot allocator"]
        },
        Epilog -> {
            {Dashed, Thick, StandardRed,
                Line[{{peak["peak_depth"], 0}, {peak["peak_depth"], totalHeight}}]},
            Text[
                Style[
                    Row[{"peak ", formatBytes[peak["peak_bytes"]], " @ depth ", peak["peak_depth"]}],
                    FontSize -> Scaled[0.014],
                    FontFamily -> "Source Code Pro",
                    StandardRed
                ],
                {peak["peak_depth"], totalHeight},
                {-1, -1}
            ]
        },
        PlotLabel -> Column[{
            Row[{"TMemoryPlan / ", backendsActive[allBufs], " - ", Length[allBufs], " bufs / ", Length[a["Kernels"]], " kernels / ", formatBytes[totalBytes], " total / depth ", If[allBufs === {}, 0, maxDepth + 1]}],
            Row[{"peak concurrent: ", formatBytes[peak["peak_bytes"]], " at depth ", peak["peak_depth"], " (slot-reuse headroom ", savingsPct, "%)"}],
            If[ omittedCount > 0,
                With[{shownBytes = Total[#["nbytes"] & /@ bufs], shownPct = Round[100. Total[#["nbytes"] & /@ bufs] / totalBytes, 0.1]},
                    Row[{Style["showing top " <> ToString[Length[bufs]] <> " of " <> ToString[Length[allBufs]] <> " bufs (= " <> ToString[shownPct] <> "% of bytes)", Italic, GrayLevel[0.4]]}]
                ],
                Sequence @@ {}
            ]
        }, Alignment -> Center],
        ImageSize -> Large,
        AspectRatio -> 1/2,
        PlotRangePadding -> Scaled[0.02]
    ] // Legended[#,
        SwatchLegend[
            statusFill /@ legendStatuses,
            legendStatuses,
            LegendMarkers -> Graphics[{
                EdgeForm[Directive[Black, Thickness[0.02]]],
                Rectangle[{0, 0}, {1, 1}, RoundingRadius -> 0.18]
            }],
            LegendLabel -> "buffer status",
            LabelStyle -> {FontSize -> 11}
        ]
    ] &
]

End[];

EndPackage[];
