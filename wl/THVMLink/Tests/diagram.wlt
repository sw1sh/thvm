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
labelPat[name_String] := Column[{name, ___}, ___] |
    _String ? (StringStartsQ[#, name] &);
findOne[d_, name_String] := First @ DiagramCases[d, DiagramPattern[labelPat[name]]];
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
        d   = THeapDiagram[{t}];
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
        d = THeapDiagram[{TSup[1, 2] + 3}], op2, sup, nums
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
        steps = TMultiSteps[t];
        d     = steps[[4]]["Diagram"];                (* post DUP_SUP_ANN *)
        Length @ findAll[d, "OP2 +"]
    ],
    2,
    TestID -> "THeapDiagram: post-DUP_SUP_ANN both branches visible via sibling auto-seed"
]
