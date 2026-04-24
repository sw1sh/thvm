(* ::Package:: *)
(* Format.wl - summary boxes for THVMLink atomic objects.

   Loaded from THVMLink.wl inside Begin["`Private`"].  Adopts the
   QuantumFramework pattern (see ~/src/wolfram/QuantumFramework):
       - structural Q-test on the atomic head's payload shape
       - UpValue MakeBoxes guarded by the Q-test (with Unevaluated)
       - BoxForm`ArrangeSummaryBox for the visual

   Adding a new atomic object means: declare the constructor in
   THVMLink.wl (so it returns Head[<|...|>]), write a Q-test here,
   then add a MakeBoxes UpValue here.
*)

(* === structural validity tests === *)

tHeapPayloadQ[a_Association] :=
    KeyExistsQ[a, "nextLoc"] && KeyExistsQ[a, "cells"] && KeyExistsQ[a, "Graph"]
tHeapPayloadQ[___] := False

tHeapQ[THeap[a_Association]] := tHeapPayloadQ[a]
tHeapQ[___] := False

tTermInfoPayloadQ[a_Association] :=
    KeyExistsQ[a, "tag"] && KeyExistsQ[a, "tagName"] && KeyExistsQ[a, "raw"]
tTermInfoPayloadQ[___] := False

tTermInfoQ[TTermInfo[a_Association]] := tTermInfoPayloadQ[a]
tTermInfoQ[___] := False

(* === icons (small thumbnails for the summary box) === *)

heapSummaryIcon[] := Graphics[
    {
        EdgeForm[Black], FaceForm[White], Disk[{0, 0}, 0.5],
        Thick, Arrow[BSplineCurve[{{0.5, 0}, {1.5, 1}, {1.5, -1}, {0.5, 0}}]]
    },
    ImageSize -> Dynamic[{Automatic, 3.5 CurrentValue["FontCapHeight"] / AbsoluteCurrentValue[Magnification]}],
    PlotRangePadding -> Scaled[0.05]
]

termSummaryIcon[] := Graphics[
    {EdgeForm[Black], FaceForm[White], Disk[{0, 0}, 0.5]},
    ImageSize -> Dynamic[{Automatic, 3.0 CurrentValue["FontCapHeight"] / AbsoluteCurrentValue[Magnification]}],
    PlotRangePadding -> Scaled[0.05]
]

(* === MakeBoxes UpValues === *)

THeap /: MakeBoxes[s_THeap /; tHeapQ[Unevaluated[s]], fmt_] := With[{
    a = First[s],
    icon = heapSummaryIcon[]
},
    BoxForm`ArrangeSummaryBox[
        "THeap",
        s,
        icon,
        {
            {
                BoxForm`SummaryItem[{"nextLoc: ", a["nextLoc"]}],
                BoxForm`SummaryItem[{"cells: ",   Length[a["cells"]]}]
            }
        },
        {
            {
                BoxForm`SummaryItem[{"Graph: ", If[GraphQ[a["Graph"]],
                    Row[{VertexCount[a["Graph"]], " v / ", EdgeCount[a["Graph"]], " e"}],
                    "(none)"]}]
            }
        },
        fmt,
        "Interpretable" -> Automatic
    ]
]

TTermInfo /: MakeBoxes[ti_TTermInfo /; tTermInfoQ[Unevaluated[ti]], fmt_] := With[{
    a = First[ti],
    icon = termSummaryIcon[]
},
    BoxForm`ArrangeSummaryBox[
        "TTermInfo",
        ti,
        icon,
        {
            {
                BoxForm`SummaryItem[{"tag: ", Row[{a["tagName"], "@", a["val"]}]}],
                BoxForm`SummaryItem[{"ext: ", a["ext"]}]
            }
        },
        {
            {
                BoxForm`SummaryItem[{"sub: ", a["sub"]}],
                BoxForm`SummaryItem[{"raw: ", a["raw"]}]
            }
        },
        fmt,
        "Interpretable" -> Automatic
    ]
]
