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

BeginPackage["THVMLink`"];

Begin["`Private`"];

(* === structural validity tests === *)

tHeapPayloadQ[a_Association] :=
    KeyExistsQ[a, "nextLoc"] && KeyExistsQ[a, "cells"] && KeyExistsQ[a, "Graph"]
tHeapPayloadQ[___] := False

tHeapQ[THeap[a_Association]] := tHeapPayloadQ[a]
tHeapQ[___] := False

tTermQ[TTerm[_Integer, _Integer, _]] := True
tTermQ[___]                       := False

(* More specific tests that match on a TTerm AND its tag.  Used to
   dispatch the MakeBoxes UpValue by tag so the summary box for a
   tensor differs from the one for an IC combinator. *)
tTermTagQ[t_TTerm, tag_Integer] := $termTagFn[ttermRaw[t]] === tag
tTermTagQ[___, ___] := False

tTenQ[t_] := tTermTagQ[t, $TagTEN]
tUopQ[t_] := tTermTagQ[t, $TagUOP]
tNumQ[t_] := tTermTagQ[t, $TagNUM]

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

(* Colored-square icon for TTensor: cyan fill matches the heap-graph
   TAG_TEN styling (docs/tensors.md). *)
tenSummaryIcon[] := Graphics[
    {EdgeForm[LightDarkSwitched[Black, White]],
     FaceForm[LightDarkSwitched[Lighter[StandardCyan, 0.55], Darker[StandardCyan, 0.45]]],
     Rectangle[{-0.45, -0.45}, {0.45, 0.45}]},
    ImageSize -> Dynamic[{Automatic, 3.0 CurrentValue["FontCapHeight"] / AbsoluteCurrentValue[Magnification]}],
    PlotRangePadding -> Scaled[0.05]
]

(* Colored-rectangle icon for TUOp. *)
uopSummaryIcon[] := Graphics[
    {EdgeForm[LightDarkSwitched[Black, White]],
     FaceForm[LightDarkSwitched[Lighter[StandardBlue, 0.55], Darker[StandardBlue, 0.45]]],
     Rectangle[{-0.6, -0.3}, {0.6, 0.3}]},
    ImageSize -> Dynamic[{Automatic, 3.0 CurrentValue["FontCapHeight"] / AbsoluteCurrentValue[Magnification]}],
    PlotRangePadding -> Scaled[0.05]
]

(* Orange rounded-rectangle icon for TKernel: matches the KERNEL
   palette in Style.wl + THeapGraph + TScheduleGraph. *)
tKernelSummaryIcon[] := Graphics[
    {
        EdgeForm[LightDarkSwitched[Black, White]],
        FaceForm[LightDarkSwitched[Lighter[StandardOrange, 0.55], Darker[StandardOrange, 0.4]]],
        Rectangle[{-0.6, -0.6}, {0.6, 0.6}, RoundingRadius -> 0.18]
    },
    ImageSize -> Dynamic[{Automatic, 3.5 CurrentValue["FontCapHeight"] / AbsoluteCurrentValue[Magnification]}],
    PlotRangePadding -> Scaled[0.05]
]

(* Three-circle icon for TContext: chain-of-runtime-state metaphor. *)
contextSummaryIcon[] := Graphics[
    {
        EdgeForm[LightDarkSwitched[Black, White]],
        FaceForm[LightDarkSwitched[Lighter[StandardOrange, 0.55], Darker[StandardOrange, 0.45]]],
        Disk[{-0.5, 0}, 0.25], Disk[{0, 0}, 0.25], Disk[{0.5, 0}, 0.25]
    },
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

TTerm /: MakeBoxes[t_TTerm /; tTermQ[Unevaluated[t]] && ! tTenQ[t] && ! tUopQ[t] && ! tNumQ[t], fmt_] := With[{
    id   = ttermRaw[t],
    icon = termSummaryIcon[]
},
    BoxForm`ArrangeSummaryBox[
        "TTerm",
        t,
        icon,
        {
            {
                BoxForm`SummaryItem[{"tag: ",
                    Row[{TTagName[$termTagFn[id]], "@", $termValFn[id]}]}],
                BoxForm`SummaryItem[{"ext: ", $termExtFn[id]}]
            }
        },
        {
            {
                BoxForm`SummaryItem[{"sub: ", $termSubFn[id]}],
                BoxForm`SummaryItem[{"raw: ", id}]
            }
        },
        fmt,
        "Interpretable" -> Automatic
    ]
]

(* Tag-specialized summary boxes: TAG_TEN / TAG_UOP / TAG_NUM each
   render with their own icon + domain-specific fields. *)

TTerm /: MakeBoxes[t_TTerm /; tTenQ[t], fmt_] := With[{
    id    = ttermRaw[t],
    icon  = tenSummaryIcon[],
    shape = TTensorShape[t],
    rc    = TTensorRefcount[t]
},
    BoxForm`ArrangeSummaryBox[
        "TTensor",
        t,
        icon,
        {
            {
                BoxForm`SummaryItem[{"shape: ", Row[shape, Times]}],
                BoxForm`SummaryItem[{"dtype: ", TTensorDType[t]}]
            }
        },
        {
            {
                BoxForm`SummaryItem[{"refcount: ", rc}],
                BoxForm`SummaryItem[{"tensor id: ", $termValFn[id]}]
            }
        },
        fmt,
        "Interpretable" -> Automatic
    ]
]

TTerm /: MakeBoxes[t_TTerm /; tUopQ[t], fmt_] := With[{
    id    = ttermRaw[t],
    icon  = uopSummaryIcon[],
    kind  = TUOpKind[t]
},
    BoxForm`ArrangeSummaryBox[
        "TUOp",
        t,
        icon,
        {
            {
                BoxForm`SummaryItem[{"kind: ", kind}],
                BoxForm`SummaryItem[{"loc: ",  $termValFn[id]}]
            }
        },
        {
            {
                BoxForm`SummaryItem[{"srcs: ", Length[TUOpSrcs[t]]}]
            }
        },
        fmt,
        "Interpretable" -> Automatic
    ]
]

TTerm /: MakeBoxes[t_TTerm /; tNumQ[t], fmt_] := With[{
    id   = ttermRaw[t],
    icon = termSummaryIcon[]
},
    BoxForm`ArrangeSummaryBox[
        "TNum",
        t,
        icon,
        {
            {
                BoxForm`SummaryItem[{"dtype: ", dtypeName[$termExtFn[id]]}],
                BoxForm`SummaryItem[{"bits: ",  $termValFn[id]}]
            }
        },
        {
            {BoxForm`SummaryItem[{"raw: ", id}]}
        },
        fmt,
        "Interpretable" -> Automatic
    ]
]

(* === TKernel summary box ===
   Mirrors the TUOp summary-box pattern but surfaces the kernel's
   identifying fields (kid, output shape, input count, fired flag).
   The TKernel constructor + property surface lives in Kernel.wl;
   here we only render. *)

TKernel /: MakeBoxes[k:TKernel[a_Association] /; tKernelInternalQ[k], fmt_] :=
    With[{
        kid     = a["Kid"],
        row     = kernelRowAsoc[a["Kid"]],
        nProg   = TTermVal[a["Term"]],
        icon    = tKernelSummaryIcon[]
    },
        BoxForm`ArrangeSummaryBox[
            "TKernel",
            k,
            icon,
            {
                {
                    BoxForm`SummaryItem[{"kid: ",      kid}],
                    BoxForm`SummaryItem[{"inputs: ",   row["InputCount"]}]
                },
                {
                    BoxForm`SummaryItem[{"out tid: ",  row["OutputTid"]}],
                    BoxForm`SummaryItem[{"out dtype: ",row["OutputDtype"]}]
                }
            },
            {
                {
                    BoxForm`SummaryItem[{"out numel: ", row["OutputNumel"]}],
                    BoxForm`SummaryItem[{"fired: ",     row["Fired"]}]
                },
                {
                    BoxForm`SummaryItem[{"consumers: ", row["ConsumerCount"]}],
                    BoxForm`SummaryItem[{"spliced: ",   row["Spliced"]}]
                }
            },
            fmt,
            "Interpretable" -> Automatic
        ]
    ]

(* === TMemoryPlan summary box ===
   Mini stack-of-rectangles icon + counts + total bytes + active
   backend(s).  formatBytes / backendsActive are MemoryPlan.wl
   helpers in the shared THVMLink`Private` namespace. *)
{formatBytes, backendsActive};

memoryPlanSummaryIcon[] := Graphics[
    {
        EdgeForm[LightDarkSwitched[Black, White]],
        FaceForm[LightDarkSwitched[Lighter[StandardBlue,  0.55], Darker[StandardBlue,  0.4]]],
        Rectangle[{-0.7, -0.55}, { 0.3, -0.20}],
        FaceForm[LightDarkSwitched[Lighter[StandardGreen, 0.55], Darker[StandardGreen, 0.4]]],
        Rectangle[{-0.5, -0.15}, { 0.7,  0.20}],
        FaceForm[LightDarkSwitched[Lighter[StandardGray,  0.55], Darker[StandardGray,  0.4]]],
        Rectangle[{-0.7,  0.25}, { 0.5,  0.60}]
    },
    ImageSize -> Dynamic[{Automatic, 3.5 CurrentValue["FontCapHeight"] / AbsoluteCurrentValue[Magnification]}],
    PlotRangePadding -> Scaled[0.05]
]

memoryPlanQ[TMemoryPlan[a_Association]] :=
    KeyExistsQ[a, "Kernels"] && KeyExistsQ[a, "Tens"] && KeyExistsQ[a, "Bufs"]
memoryPlanQ[___] := False

TMemoryPlan /: MakeBoxes[p_TMemoryPlan /; memoryPlanQ[Unevaluated[p]], fmt_] :=
    With[{
        a    = First[p],
        icon = memoryPlanSummaryIcon[]
    }, Module[{
        bufs = a["Bufs"], totalBytes
    },
        totalBytes = Total[#["nbytes"] & /@ bufs];
        BoxForm`ArrangeSummaryBox[
            "TMemoryPlan",
            p,
            icon,
            {
                {
                    BoxForm`SummaryItem[{"kernels: ", Length[a["Kernels"]]}],
                    BoxForm`SummaryItem[{"bufs: ",    Length[bufs]}]
                }
            },
            {
                {
                    BoxForm`SummaryItem[{"live: ",    formatBytes[totalBytes]}],
                    BoxForm`SummaryItem[{"backend: ", backendsActive[bufs]}]
                }
            },
            fmt,
            "Interpretable" -> Automatic
        ]
    ]]

(* === TContext (multi-heap handle from Context.wl) === *)

tContextQ[TContext[_Integer]] := True
tContextQ[___]                := False

TContext /: MakeBoxes[c_TContext /; tContextQ[Unevaluated[c]], fmt_] := With[{
    slot = First[c]
},
    (* Probe the slot's runtime state by temporarily switching to it. *)
    Module[{prev = $contextCurrentFn[], heapPos, tens, kernels, isCurrent},
        $contextSelectFn[slot];
        heapPos   = $heapPosFn[];
        tens      = If[ $tensCountFn   === $tensCountFn,   $tensCountFn[],   "?"];
        kernels   = If[ $kernelCountFn === $kernelCountFn, $kernelCountFn[], "?"];
        $contextSelectFn[prev];
        isCurrent = (slot === prev);
        BoxForm`ArrangeSummaryBox[
            "TContext",
            c,
            contextSummaryIcon[],
            {
                {
                    BoxForm`SummaryItem[{"slot: ",   slot}],
                    BoxForm`SummaryItem[{"current: ", If[isCurrent, "yes", "no"]}]
                }
            },
            {
                {
                    BoxForm`SummaryItem[{"heap: ",    heapPos}],
                    BoxForm`SummaryItem[{"tensors: ", tens}]
                },
                {
                    BoxForm`SummaryItem[{"kernels: ", kernels}]
                }
            },
            fmt,
            "Interpretable" -> Automatic
        ]
    ]
]

End[];

EndPackage[];
