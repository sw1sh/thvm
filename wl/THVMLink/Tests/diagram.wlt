(* diagram.wlt -- regression tests for THeapDiagram (Diagram.wl).

   Pins down node / port topology for the patterns the user has
   complained about so refactors can't silently regress them:

     - root term held externally still surfaces its principal port
       (`p<base>`) as a DiagramNetwork output -- APP not "missing a
       port" when there's no in-heap consumer
     - DUP renders both projection outputs even when neither has an
       in-heap consumer (always-fork rendering)
     - post-APP_LAM step shows the SUP body as a real compound node
       wired via `var<binder>` (no atom-style Disk leaf duplicating
       the substituted compound)
     - post-DUP_SUP_ANN step shows BOTH branches via the auto-added
       sibling DP projection in the seed list

   Uses Wolfram`DiagrammaticComputation`'s native pattern API:
     - DiagramCases[d, DiagramPattern[exprPat]] finds subdiagrams.
     - port["Name"] returns the clean wire name (no PortDual wrap).
     - Through[ports["Name"]] applies Name to each port in a list. *)

Needs["Wolfram`DiagrammaticComputation`"];

(* Compound nodes label as Column[{"NAME", "@loc"}, ...]; atoms
   (NUM / ERA / TEN / ...) label as a bare String "NAME\n<info>".
   labelPat covers both. *)
labelPat[name_String] :=
    Column[{_String ? (StringStartsQ[#, name] &), ___}, ___] |
    _String ? (StringStartsQ[#, name] &);
findOne[d_, name_String] :=
    Replace[DiagramCases[d, DiagramPattern[labelPat[name]]],
        {{}                 -> Missing["NotFound", name],
         {first_, ___}      :> first}];
findAll[d_, name_String] := DiagramCases[d, DiagramPattern[labelPat[name]]];

(* === root APP held externally surfaces principal as network out === *)
VerificationTest[
    TInit[];
    Block[{
        t = TLam[x, x + 3][TSup[1, 2]], d, app
    },
        d   = THeapDiagram[{t}];
        app = findOne[d, "APP"];
        {
            app["InputArity"],
            app["OutputArity"],
            MemberQ[Through[d["OutputPorts"]["Name"]], "p5"]
        }
    ],
    {1, 2, True},
    TestID -> "THeapDiagram: root APP has principal port surfaced as network output"
]

(* === DUP renders both projection outputs ===
   For `t = First @ TDup[L, body]` the held DP0 has no in-heap
   consumer; DP1 was discarded.  Both projection outputs must
   still render (DUP is structurally a fork). *)
VerificationTest[
    TInit[];
    Block[{t = First @ TDup[0, TLam[x, x + 3][TSup[0, 1, 2]]], d, dup},
        d   = THeapDiagram[t];
        dup = findOne[d, "DUP"];
        dup["OutputArity"]
    ],
    2,
    TestID -> "THeapDiagram: DUP exposes both projection output ports"
]

(* === post-APP_LAM: SUP rendered as compound + wired via var<binder>,
   not duplicated as a free Disk leaf at the dead-LAM binder loc. *)
VerificationTest[
    TInit[];
    Block[{
        t = First @ TDup[0, TLam[x, x + 3][TSup[0, 1, 2]]],
        steps, d, sup, op2
    },
        steps = TMultiTrace[t, All];
        d     = steps[[2]]["Diagram"];                (* post APP_LAM *)
        sup   = findOne[d, "SUP"];
        op2   = findOne[d, "OP2 +"];
        {
            sup["InputArity"] === 2,
            sup["OutputArity"] === 1,
            (* SUP and OP2 share var0 -- the LAM binder wire *)
            Intersection[
                Through[sup["Ports"]["Name"]],
                Through[op2["Ports"]["Name"]]] === {"var0"},
            (* No duplicate atom leaf for the substituted SUP *)
            DiagramCases[d, DiagramPattern["SUP@0"]] === {}
        }
    ],
    {True, True, True, True},
    TestID -> "THeapDiagram: post-APP_LAM SUP body renders as compound wired via var<binder>"
]

(* === TSup[1, 2] + 3 -- the canonical OP2-SUP commute example.
   No LAM, no enclosing DUP: just OP2 wrapping a SUP and a NUM.
   The initial diagram is a plain OP2 + SUP + NUMs network. *)
VerificationTest[
    TInit[];
    Block[{
        d = THeapDiagram[TSup[1, 2] + 3], op2, sup, nums
    },
        op2  = findOne[d, "OP2 +"];
        sup  = findOne[d, "SUP"];
        nums = findAll[d, "NUM"];
        {
            op2["InputArity"] === 2 && op2["OutputArity"] === 1,
            sup["InputArity"] === 2 && sup["OutputArity"] === 1,
            Length[nums] === 3,
            Intersection[
                Through[op2["Ports"]["Name"]],
                Through[sup["Ports"]["Name"]]] =!= {},
            MemberQ[Through[d["OutputPorts"]["Name"]], "p2"]
        }
    ],
    {True, True, True, True, True},
    TestID -> "THeapDiagram: TSup[1, 2] + 3 -- OP2 wired into SUP + NUM 3"
]

(* === post-DUP_SUP_ANN: both branches visible (sibling auto-seeded).
   DP0's branch surfaces OP2(NUM 1, ...); the sibling DP1 surfaces
   OP2(NUM 2, ...) through heap[loc]'s SUB flag. *)
VerificationTest[
    TInit[];
    Block[{
        t = First @ TDup[0, TLam[x, x + 3][TSup[0, 1, 2]]],
        steps, d
    },
        steps = TMultiTrace[t, All];
        d     = steps[[4]]["Diagram"];                (* post DUP_SUP_ANN *)
        Length @ findAll[d, "OP2 +"]
    ],
    2,
    TestID -> "THeapDiagram: post-DUP_SUP_ANN both branches visible via sibling auto-seed"
]

(* === TGrad over a tensor surfaces the full UOP DAG.
   `e = TGrad[2 x, x]` for x = TTensorCreate[...] yields a
   DP1 projection of a DUP wrapping MUL(EXPAND(CONST), TEN).
   All five nodes -- DUP, MUL, EXPAND, TEN, CONST -- must
   appear; in particular the TEN handle (the user's tensor)
   must not be silently dropped because reachableUopsHere
   bailed on the non-UOP DP1 seed. *)
VerificationTest[
    TInit[];
    Block[{
        x = TTensorCreate[{1, 2, 3}],
        e, d
    },
        e = TGrad[2 x, x];
        d = THeapDiagram[e];
        {
            findOne[d, "DUP"]["OutputArity"] === 2,
            ! MissingQ @ findOne[d, "MUL"],
            ! MissingQ @ findOne[d, "EXPAND"],
            ! MissingQ @ findOne[d, "TEN"],
            ! MissingQ @ findOne[d, "CONST"]
        }
    ],
    {True, True, True, True, True},
    TestID -> "THeapDiagram[TGrad[2 x, x]] surfaces DUP + MUL + EXPAND + TEN + CONST"
]

(* === per-step topology for `TLam[x, x + 3][TSup[1, 2]]`.
   No enclosing DUP -- the active term itself updates step-to-step
   and each step's diagram should reflect ONLY the currently live
   subgraph (dead APP/LAM cleaned out, no stale references from the
   original APP seed leaking forward). *)
nodeLabels[d_] := Sort @ Map[
    Function[s,
        Replace[ReleaseHold @ s["HoldExpression"], {
            Column[{name_, ___}, ___] :> name,
            s_String :> StringSplit[s, "\n"][[1]]}]],
    d["SubDiagrams"]];

VerificationTest[
    TInit[];
    Block[{steps = TMultiTrace[TLam[x, x + 3][TSup[1, 2]], All]},
        nodeLabels /@ steps[[All, "Diagram"]]
    ],
    {
        Sort @ {"APP", "LAM", "SUP", "OP2 +", "NUM", "NUM", "NUM"},   (* step 1: initial *)
        Sort @ {"OP2 +", "SUP", "NUM", "NUM", "NUM"},                 (* step 2: APP_LAM consumed APP+LAM *)
        Sort @ {"SUP", "OP2 +", "OP2 +", "DUP", "NUM", "NUM", "NUM"}, (* step 3: OP2_SUP commuted *)
        Sort @ {"SUP", "OP2 +", "OP2 +", "NUM", "NUM", "NUM"},        (* step 4: DUP_NUM atom-copied *)
        Sort @ {"SUP", "OP2 +", "NUM", "NUM", "NUM"},                 (* step 5: first OP2_NUM_NUM *)
        Sort @ {"SUP", "NUM", "NUM"}                                  (* step 6: final *)
    },
    TestID -> "TMultiTrace[lam[x, x + 3][sup[1, 2]]] -- per-step node-label sets"
]

(* Wires: a key topology invariant is that VAR(0) in the OP2 body
   shares its wire with the substituted SUP arg after APP_LAM. *)
VerificationTest[
    TInit[];
    Block[{
        steps = TMultiTrace[TLam[x, x + 3][TSup[1, 2]], All],
        d, op2, sup
    },
        d   = steps[[2]]["Diagram"];                  (* post APP_LAM *)
        op2 = findOne[d, "OP2 +"];
        sup = findOne[d, "SUP"];
        {
            (* var0 IS the wire connecting OP2's slot 0 to the SUP's
               principal -- the LAM's binder was substituted with the
               SUP arg. *)
            MemberQ[Through[op2["Ports"]["Name"]], "var0"],
            MemberQ[Through[sup["Ports"]["Name"]], "var0"],
            (* OP2's principal is exposed as the network output. *)
            Through[d["OutputPorts"]["Name"]] === {"p1"}
        }
    ],
    {True, True, True},
    TestID -> "TMultiSteps step 2: APP+LAM consumed, OP2 wired to SUP via var0"
]

(* Step 3: OP2_SUP commute introduces a DUP and a new outer SUP.
   The new SUP's two slots are OP2s; each OP2's slot 1 wire is one
   of the DUP's projection outputs. *)
VerificationTest[
    TInit[];
    Block[{
        steps = TMultiTrace[TLam[x, x + 3][TSup[1, 2]], All],
        d, dup, op2s, dupOuts, op2SecondSlotWires
    },
        d    = steps[[3]]["Diagram"];                 (* post OP2_SUP *)
        dup  = findOne[d, "DUP"];
        op2s = findAll[d, "OP2 +"];
        (* DUP outputs both projection wires. *)
        dupOuts = Sort @ Through[dup["OutputPorts"]["Name"]];
        (* Collect the SECOND slot wire (= the duped NUM 3 ref) from
           each OP2.  Should be one DP0 wire and one DP1 wire. *)
        op2SecondSlotWires = Sort @ Map[
            Function[op2, op2["InputPorts"][[2]]["Name"]],
            op2s];
        {
            dup["OutputArity"] === 2,
            Length[op2s] === 2,
            dupOuts === op2SecondSlotWires
        }
    ],
    {True, True, True},
    TestID -> "TMultiSteps step 3: OP2_SUP introduces DUP whose projections feed both new OP2s"
]
