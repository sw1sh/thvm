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

TInit[];

(* Compound nodes label as Column[{"NAME", "@loc"}, ...]; matchExpr
   reduces that to a `Column[{name, ___}, ___]` pattern. *)
labelPat[name_String] := Column[{name, ___}, ___];
findOne[d_, name_String] :=
    First @ DiagramCases[d, DiagramPattern[labelPat[name]]];
findAll[d_, name_String] :=
    DiagramCases[d, DiagramPattern[labelPat[name]]];

(* === root APP held externally surfaces principal as network out === *)
VerificationTest[
    TInit[];
    Module[{t = TLam[x, x + TNum[3]][TSup[1, 2]], d, app},
        d   = THeapDiagram[{t}];
        app = findOne[d, "APP"];
        {
            app["InputArity"],
            app["OutputArity"],
            MemberQ[Through[d["OutputPorts"]["Name"]], "p5"]
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
    Module[{t = First @ TDup[0, TLam[x, x + TNum[3]][TSup[0, 1, 2]]], d, dup},
        d   = THeapDiagram[{t}];
        dup = findOne[d, "DUP"];
        dup["OutputArity"]],
    2,
    TestID -> "THeapDiagram: DUP exposes both projection output ports"
]

(* === post-APP_LAM: SUP rendered as compound + wired via var<binder>,
   not duplicated as a free Disk leaf at the dead-LAM binder loc. *)
VerificationTest[
    TInit[];
    Module[{t = First @ TDup[0, TLam[x, x + TNum[3]][TSup[0, 1, 2]]],
            steps, d, sup, op2},
        steps = TMultiSteps[t];
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
        }],
    {True, True, True, True},
    TestID -> "THeapDiagram: post-APP_LAM SUP body renders as compound wired via var<binder>"
]

(* === post-DUP_SUP_ANN: both branches visible (sibling auto-seeded).
   DP0's branch surfaces OP2(NUM 1, ...); the sibling DP1 surfaces
   OP2(NUM 2, ...) through heap[loc]'s SUB flag. *)
VerificationTest[
    TInit[];
    Module[{t = First @ TDup[0, TLam[x, x + TNum[3]][TSup[0, 1, 2]]],
            steps, d},
        steps = TMultiSteps[t];
        d     = steps[[4]]["Diagram"];                (* post DUP_SUP_ANN *)
        Length @ findAll[d, "OP2 +"]],
    2,
    TestID -> "THeapDiagram: post-DUP_SUP_ANN both branches visible via sibling auto-seed"
]
