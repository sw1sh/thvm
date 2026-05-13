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

(* Forward-declare symbols owned by later-loading siblings whose
   summary boxes are rendered here.  Without this, references in
   Format.wl resolve to fresh `Private` symbols (Format.wl < their
   owners alphabetically), so the MakeBoxes UpValues never fire on
   the public symbol the user actually constructs. *)
{TOpt, TKernelOpts, TKernelVariant};

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

(* TraditionalForm of a TTerm: math-style render via `tTermTrad`
   with a Tooltip carrying the verbose TTermExpr.  Wrapped in
   InterpretationBox so the cell stays interpretable as the
   underlying TTerm (Convert -> Form -> StandardForm flips back to
   the summary box, etc.).  Other formats fall through to the
   existing collapsible summary box. *)
TTerm /: MakeBoxes[t_TTerm /; tTermQ[Unevaluated[t]] && ! tTenQ[t] && ! tUopQ[t] && ! tNumQ[t], fmt_] :=
    If[ fmt === TraditionalForm,
        With[
            {disp = ToBoxes[
                Tooltip[tTermTrad[t], tTermExprResolved[t]], fmt]},
            InterpretationBox[disp, t, SelectWithContents -> True]],
        With[{
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

TTerm /: MakeBoxes[t_TTerm /; tNumQ[t], fmt_] :=
    If[ fmt === TraditionalForm,
        With[
            {disp = ToBoxes[
                Tooltip[tTermTrad[t], tTermExprResolved[t]], fmt]},
            InterpretationBox[disp, t, SelectWithContents -> True]],
        tNumSummaryBox[t, fmt]];
tNumSummaryBox[t_, fmt_] := With[{
    id   = ttermRaw[t],
    icon = termSummaryIcon[]
},
    BoxForm`ArrangeSummaryBox[
        "TNum",
        t,
        icon,
        {
            {
                BoxForm`SummaryItem[{"number: ", $termValFn[id]}],
                BoxForm`SummaryItem[{"dtype: ",  dtypeName[$termExtFn[id]]}]
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

(* === TOpt summary box ===
   Small wedge icon (kernel-orange) + the (op, axis, arg) triple
   inline.  Collapsed view explains the op's effect for users new
   to the codegen-variant scaffold. *)

tOptQ[TOpt[op_String, axis_Integer, _]] := True
tOptQ[___] := False

tOptSummaryIcon[] := Graphics[
    {
        EdgeForm[LightDarkSwitched[Black, White]],
        FaceForm[LightDarkSwitched[Lighter[StandardOrange, 0.6], Darker[StandardOrange, 0.4]]],
        Polygon[{{-0.6, -0.5}, {0.5, 0}, {-0.6, 0.5}}]
    },
    ImageSize -> Dynamic[{Automatic, 3.0 CurrentValue["FontCapHeight"] / AbsoluteCurrentValue[Magnification]}],
    PlotRangePadding -> Scaled[0.05]
]

tOptExplanation[op_String] := Switch[op,
    "UPCAST",   "split inner output axis; unroll without a loop",
    "UNROLL",   "split reduce-axis; unroll the k-loop",
    "LOCAL",    "Metal: bind axis to thread_position_in_threadgroup",
    "GROUP" | "GROUPTOP",
                "Metal: threadgroup-shared accumulator + barrier",
    "SWAP",     "swap axis order in the iteration nest",
    "PADTO",    "pad axis up to a multiple of arg",
    "NOLOCALS", "disable LOCAL bindings on this kernel",
    "TC",       "Metal: choose MMA/GEMM tile size",
    _,          "(unknown op)"]

TOpt /: MakeBoxes[t:TOpt[op_String, axis_Integer, arg_] /; tOptQ[Unevaluated[t]], fmt_] :=
    With[{icon = tOptSummaryIcon[]},
        BoxForm`ArrangeSummaryBox[
            "TOpt",
            t,
            icon,
            {
                {
                    BoxForm`SummaryItem[{"op: ",   op}],
                    BoxForm`SummaryItem[{"axis: ", axis}],
                    BoxForm`SummaryItem[{"arg: ",  arg}]
                }
            },
            {
                {BoxForm`SummaryItem[{"effect: ", tOptExplanation[op]}]}
            },
            fmt,
            "Interpretable" -> Automatic
        ]
    ]

(* === TKernelOpts summary box ===
   Stacked-bars icon (one bar per axis, color-coded by axis type)
   + a one-line axis signature ("LOOP x 1 + UPCAST(4) + REDUCE")
   + applied-opt count.  Collapsed: full axis_types/full_shape
   table + the chronological applied list. *)

tKernelOptsQ[TKernelOpts[a_Association]] := AllTrue[
    {"Kid", "AxisTypes", "FullShape", "Applied"}, KeyExistsQ[a, #] &]
tKernelOptsQ[___] := False

(* Per-axis-type swatch color.  LOOP = neutral gray (passive);
   REDUCE = blue (k-loop); UPCAST/UNROLL = orange (unrolled);
   LOCAL/GLOBAL/GROUP_REDUCE = green (parallel binding). *)
$kaxColors = <|
    "LOOP"         -> StandardGray,
    "REDUCE"       -> StandardBlue,
    "UPCAST"       -> StandardOrange,
    "UNROLL"       -> StandardOrange,
    "LOCAL"        -> StandardGreen,
    "GLOBAL"       -> StandardGreen,
    "GROUP_REDUCE" -> StandardGreen
|>

tKernelOptsSummaryIcon[axisTypes_List] := Graphics[
    Table[
        With[{
            t = axisTypes[[i]],
            x = -0.6 + 1.2 * (i - 1) / Max[Length[axisTypes], 1]
        },
            {
                EdgeForm[LightDarkSwitched[Black, White]],
                FaceForm[LightDarkSwitched[
                    Lighter[Lookup[$kaxColors, t, StandardGray], 0.5],
                    Darker [Lookup[$kaxColors, t, StandardGray], 0.4]]],
                Rectangle[{x, -0.55}, {x + 1.0/Max[Length[axisTypes], 1], 0.55}]
            }],
        {i, Length[axisTypes]}],
    ImageSize -> Dynamic[{Automatic, 3.5 CurrentValue["FontCapHeight"] / AbsoluteCurrentValue[Magnification]}],
    PlotRange -> {{-0.7, 0.7}, {-0.7, 0.7}},
    PlotRangePadding -> Scaled[0.05]
]

(* One-line "LOOP x 2 + UPCAST(4) + REDUCE" signature, runs adjacent
   axes of same type into a "x N" group, annotates split-induced
   axes with their size in parens. *)
tKernelOptsAxisSignature[axisTypes_List, fullShape_List] := Module[{parts, runs},
    runs = Split[Transpose[{axisTypes, fullShape}], #1[[1]] === #2[[1]] &];
    parts = Table[
        With[{tag = run[[1, 1]], n = Length[run]},
            Which[
                MemberQ[{"UPCAST", "UNROLL", "LOCAL", "GLOBAL", "GROUP_REDUCE"}, tag],
                    StringJoin[tag, "(", ToString[run[[1, 2]]], ")"],
                n === 1, tag,
                True,    StringJoin[tag, " x ", ToString[n]]
            ]],
        {run, runs}];
    StringRiffle[parts, " + "]]

TKernelOpts /: MakeBoxes[k:TKernelOpts[a_Association] /; tKernelOptsQ[Unevaluated[k]], fmt_] :=
    With[{
        kid       = a["Kid"],
        axisTypes = a["AxisTypes"],
        fullShape = a["FullShape"],
        applied   = a["Applied"],
        icon      = tKernelOptsSummaryIcon[a["AxisTypes"]]
    },
        BoxForm`ArrangeSummaryBox[
            "TKernelOpts",
            k,
            icon,
            {
                {
                    BoxForm`SummaryItem[{"kid: ",     kid}],
                    BoxForm`SummaryItem[{"applied: ", Length[applied]}]
                },
                {
                    BoxForm`SummaryItem[{"axes: ", tKernelOptsAxisSignature[axisTypes, fullShape]}]
                }
            },
            {
                {
                    BoxForm`SummaryItem[{"AxisTypes: ", axisTypes}],
                    BoxForm`SummaryItem[{"FullShape: ", fullShape}]
                },
                {
                    BoxForm`SummaryItem[{"Applied: ", If[ applied === {}, "(none)", Column[applied]]}]
                }
            },
            fmt,
            "Interpretable" -> Automatic
        ]
    ]

(* === TKernelVariant summary box ===
   One row of the bench-the-candidates table: kid + (op, axis,
   arg) (or "baseline" if Opt is None) + measured WallUs.  Tiny
   bar icon scaled by speedup vs baseline -- Phase-16 inspection
   surface so notebooks render the bench results compactly. *)

tKernelVariantQ[TKernelVariant[a_Association]] := AllTrue[
    {"Kid", "Opt", "WallUs"}, KeyExistsQ[a, #] &]
tKernelVariantQ[___] := False

tKernelVariantSummaryIcon[isBaseline_] := Graphics[
    {
        EdgeForm[LightDarkSwitched[Black, White]],
        FaceForm @ If[ isBaseline,
            LightDarkSwitched[Lighter[StandardGray,   0.5], Darker[StandardGray,   0.4]],
            LightDarkSwitched[Lighter[StandardOrange, 0.5], Darker[StandardOrange, 0.4]]],
        Rectangle[{-0.5, -0.5}, {0.5, 0.5}]
    },
    ImageSize -> Dynamic[{Automatic, 3.0 CurrentValue["FontCapHeight"] / AbsoluteCurrentValue[Magnification]}],
    PlotRangePadding -> Scaled[0.05]
]

TKernelVariant /: MakeBoxes[v:TKernelVariant[a_Association] /; tKernelVariantQ[Unevaluated[v]], fmt_] :=
    With[{
        kid  = a["Kid"],
        opt  = a["Opt"],
        us   = a["WallUs"],
        icon = tKernelVariantSummaryIcon[a["Opt"] === None]
    },
        BoxForm`ArrangeSummaryBox[
            "TKernelVariant",
            v,
            icon,
            {
                {
                    BoxForm`SummaryItem[{"kid: ", kid}],
                    BoxForm`SummaryItem[{"us: ",  us}]
                },
                {
                    BoxForm`SummaryItem[{"opt: ",
                        If[ opt === None, "baseline (no opts)", opt]}]
                }
            },
            {
                {BoxForm`SummaryItem[{"Opt full: ", If[ opt === None, "(none)", opt]}]}
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

(* ===========================================================
   TraditionalForm rendering for TTerm.
   ===========================================================
   A clean, math-like display: OP2 as infix, SUP as angle
   brackets, DUP wrappers transparent, DP projections that
   resolve through SUB-flagged binders show the substituted
   value, otherwise as subscripted xs.  The full verbose form
   (with labels, locs, and Term-word identifiers) goes into a
   Tooltip so the structural detail is one hover away. *)

(* Symbol used for an unresolved DP-projection.  Subscript so
   the IDE highlights the binder loc clearly. *)
$opInactiveHeads = <|
    "+" -> Inactive[Plus],
    "-" -> Inactive[Subtract],
    "*" -> Inactive[Times],
    "/" -> Inactive[Divide]
|>;

(* SUB-resolving variant of TTermExpr: walks the heap, chasing
   through SUB-flagged DP/VAR projections so a resolved DP shows up
   as the substituted value (no DP wrapper).  Used as the Tooltip
   content for TraditionalForm rendering -- if the display says
   `1 + 3` then the tooltip should not contradict that with
   `OP2[+, NUM[1], DP0[1, NUM[3]]]`. *)
tTermExprResolved[t_TTerm] := tTermExprResolvedDepth[t, 16];
tTermExprResolvedDepth[t_TTerm, 0] := "?";
tTermExprResolvedDepth[t_TTerm, d_Integer] := Block[
    {tag, val, ext, cell},
    tag = TTermTag[t];
    val = TTermVal[t];
    ext = TTermExt[t];
    Switch[tag,
        $TagNUM, "NUM"[val],
        $TagERA, "ERA",
        $TagANY, "ANY",
        $TagTEN, "TEN"[val],
        $TagREF, "REF"[ext],
        $TagVAR,
            cell = THeapRead[val];
            If[ TTermSub[cell] === 1,
                tTermExprResolvedDepth[
                    packTerm[0, TTermTag[cell], TTermExt[cell],
                             TTermVal[cell]],
                    d - 1],
                "VAR"[val]],
        $TagDP0 | $TagDP1,
            cell = THeapRead[val];
            If[ TTermSub[cell] === 1,
                tTermExprResolvedDepth[
                    packTerm[0, TTermTag[cell], TTermExt[cell],
                             TTermVal[cell]],
                    d - 1],
                Superscript[If[ tag === $TagDP0, "DP0", "DP1"], ext][val]],
        $TagDUP,
            "DUP"[tTermExprResolvedDepth[THeapRead[val], d - 1]],
        $TagSUP,
            Superscript["SUP", ext][
                tTermExprResolvedDepth[THeapRead[val + 0], d - 1],
                tTermExprResolvedDepth[THeapRead[val + 1], d - 1]],
        $TagOP2,
            "OP2"[Lookup[$op2Names, ext, ToString[ext]],
                  tTermExprResolvedDepth[THeapRead[val + 0], d - 1],
                  tTermExprResolvedDepth[THeapRead[val + 1], d - 1]],
        $TagAPP,
            "APP"[tTermExprResolvedDepth[THeapRead[val + 0], d - 1],
                  tTermExprResolvedDepth[THeapRead[val + 1], d - 1]],
        $TagLAM,
            "LAM"[val, tTermExprResolvedDepth[THeapRead[val], d - 1]],
        _, "?" <> ToString[tag]]];

tTermTrad[t_TTerm] := tTermTradDepth[t, 16];
tTermTradDepth[t_TTerm, 0] := "..";
tTermTradDepth[t_TTerm, d_Integer] := Block[
    {tag, val, ext, cell, opName, head, a, b, body},
    tag = TTermTag[t];
    val = TTermVal[t];
    ext = TTermExt[t];
    Switch[tag,
        $TagNUM, val,
        $TagERA, Style["e", Italic],
        $TagANY, Style["*", Italic],
        $TagTEN, Subscript["T", val],
        $TagREF, Subscript["R", ext],
        $TagVAR, Subscript["x", val],
        $TagDP0 | $TagDP1,
            (* Both projections of a DUP are the SAME variable (IC
               notation `! &L{x, x} = body`); we distinguish them
               with a superscript projection index (0 or 1) and a
               subscript heap loc for the dup cell.  When the cell
               is SUB-flagged (a prior DUP-XXX fired), chase through
               -- the projection wrapper is gone. *)
            cell = THeapRead[val];
            If[ TTermSub[cell] === 1,
                tTermTradDepth[
                    packTerm[0, TTermTag[cell], TTermExt[cell],
                             TTermVal[cell]],
                    d - 1],
                Subscript[
                    Superscript["x", If[ tag === $TagDP0, "0", "1"]],
                    val]],
        $TagDUP,
            (* DUP wrapper is transparent at the TraditionalForm level. *)
            tTermTradDepth[THeapRead[val], d - 1],
        $TagSUP,
            AngleBracket[
                tTermTradDepth[THeapRead[val + 0], d - 1],
                tTermTradDepth[THeapRead[val + 1], d - 1]],
        $TagOP2,
            a = tTermTradDepth[THeapRead[val + 0], d - 1];
            b = tTermTradDepth[THeapRead[val + 1], d - 1];
            opName = Lookup[$op2Names, ext, ToString[ext]];
            head = Lookup[$opInactiveHeads, opName, Missing[]];
            If[ MissingQ[head],
                Row[{a, " ", opName, " ", b}],
                head[a, b]],
        $TagAPP,
            a = tTermTradDepth[THeapRead[val + 0], d - 1];
            b = tTermTradDepth[THeapRead[val + 1], d - 1];
            Inactive[a][b],
        $TagLAM,
            body = tTermTradDepth[THeapRead[val], d - 1];
            HoldForm[Function[Subscript["x", val], body]],
        $TagUOP,
            Inactive[Symbol["uop" <> ToString[ext]]] @@
                Table[tTermTradDepth[THeapRead[val + i], d - 1],
                      {i, 0, Min[4, uopArity[ext] - 1]}],
        _, "?" <> ToString[tag]]];

(* TraditionalForm of a TTerm dispatches inside the existing
   MakeBoxes UpValue rules above (search for `fmt === TraditionalForm`).
   No separate rule needed -- adding one didn't beat the broader
   UpValues on specificity. *)

End[];

EndPackage[];
