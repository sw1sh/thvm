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
       sibling DP projection in the seed list *)

Needs["Wolfram`DiagrammaticComputation`"];

TInit[];

(* Subdiagram with a given label.  The label is wrapped in HoldForm[]
   by DC and may be a bare string ("LAM") or a Column[{"APP", "@5..6"}, ...]
   for compounds with @-suffix. *)
subDiagramByLabel[d_, lab_String] := SelectFirst[
    d["SubDiagrams"],
    With[{name = ReleaseHold @ #["Name"]},
        Or[ name === lab,
            MatchQ[name, Column[{lab, ___}, ___]]]] &,
    Missing[]
];
subDiagramsByLabel[d_, lab_String] := Select[
    d["SubDiagrams"],
    With[{name = ReleaseHold @ #["Name"]},
        Or[ name === lab,
            MatchQ[name, Column[{lab, ___}, ___]]]] &
];
(* Port is an Atom from Wolfram`DiagrammaticComputation` -- Part access
   doesn't work; use the property accessor.  Strip a PortDual wrapper
   so dual / undual wires share the same name in queries. *)
portWireName[p_]      := Replace[p["Expression"], PortDual[w_] :> w];
portWireNames[ps_List]:= portWireName /@ ps;
networkOutputWires[d_]:= portWireNames[d["Ports"]];

(* === root APP held externally surfaces principal as network out === *)
VerificationTest[
    TInit[];
    Module[{t = TLam[x, x + TNum[3]][TSup[1, 2]], d, app},
        d   = THeapDiagram[{t}];
        app = subDiagramByLabel[d, "APP"];
        {
            app["InputArity"],                        (* 1 incoming *)
            app["OutputArity"],                       (* 2: arg + principal *)
            MemberQ[networkOutputWires[d], "p5"]      (* principal exposed *)
        }],
    {1, 2, True},
    TestID -> "THeapDiagram: root APP has principal port surfaced as network output"
]

(* === DUP renders both projection outputs ===
   For `t = First @ TDup[L, body]` the held DP0 has no in-heap
   consumer; DP1 was discarded.  Both projection outputs must
   still render (DUP is structurally a fork). *)
VerificationTest[
    TInit[];
    Module[{t = First @ TDup[0, TLam[x, x + TNum[3]][TSup[0, 1, 2]]],
            d, dup},
        d   = THeapDiagram[{t}];
        dup = subDiagramByLabel[d, "DUP"];
        dup["OutputArity"]],
    2,
    TestID -> "THeapDiagram: DUP exposes both projection output ports"
]

(* === post-APP_LAM: SUP rendered as compound + wired via var<binder>,
   not duplicated as a free Disk leaf at the dead-LAM binder loc. *)
VerificationTest[
    TInit[];
    Module[{t = First @ TDup[0, TLam[x, x + TNum[3]][TSup[0, 1, 2]]],
            steps, d, sup},
        steps = TMultiSteps[t];
        d     = steps[[2]]["Diagram"];                (* post APP_LAM *)
        sup   = subDiagramByLabel[d, "SUP"];
        {
            (* Compound SUP present (not just a leaf) *)
            sup["InputArity"] === 2 && sup["OutputArity"] === 1,
            (* SUP's principal output IS var0 -- routed to VAR consumers *)
            MemberQ[portWireNames[sup["Ports"]], "var0"],
            (* No duplicate "SUP@0" atom leaf *)
            MissingQ[subDiagramByLabel[d, "SUP@0"]]
        }],
    {True, True, True},
    TestID -> "THeapDiagram: post-APP_LAM SUP body renders as compound wired via var<binder>"
]

(* === post-DUP_SUP_ANN: both branches visible (sibling auto-seeded).
   DP0's branch reaches OP2@9 (NUM 1 side); the sibling DP1 surfaces
   OP2@11 (NUM 2 side) through heap[loc]'s SUB flag. *)
VerificationTest[
    TInit[];
    Module[{t = First @ TDup[0, TLam[x, x + TNum[3]][TSup[0, 1, 2]]],
            steps, d, op2s},
        steps = TMultiSteps[t];
        d     = steps[[4]]["Diagram"];                (* post DUP_SUP_ANN *)
        op2s  = subDiagramsByLabel[d, "OP2 +"];
        Length[op2s]],
    2,
    TestID -> "THeapDiagram: post-DUP_SUP_ANN both branches visible via sibling auto-seed"
]
