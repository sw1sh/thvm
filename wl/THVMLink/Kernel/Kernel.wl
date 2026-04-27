(* ::Package:: *)
(* Kernel.wl - TKernel object: a typed wrapper around a UOP_KERNEL
   TTerm that exposes the C-side KernelEntry contents (input tids,
   output shape/dtype, program ops) through a uniform Information[]
   surface, dispatches via call syntax `k[]`, and auto-coerces back
   to its underlying TTerm so it can be embedded inside other UOps
   (TUOpAdd[k, x] etc.) without an explicit unwrap.

   Public surface:
     TKernel[t_TTerm]      wrap a TTerm whose tag is UOP_KERNEL
     TKernel[kid_Integer]  resolve a kid back to its pinned heap
                           kernel_term and wrap that
     TKernel[<|"InputTensors", "OutputShape", "OutputDtype",
               "Program"|>]
                           [planned] construct a fresh KernelEntry +
                           UOP_KERNEL term from a hand-written program;
                           currently returns Failure -- needs the
                           thvm_wl_kernel_emit_program C bridge.

     Information[k, "Properties"]   list of property names
     Information[k, prop_String]    fetch one property
     k[prop_String]                 shortcut for Information[k, prop]
     k[]                            dispatch the kernel via TWnf
                                    (no-op if already fired)

   Auto-coercion:
     ttermRaw[k_TKernel]      -> packed Term value of the wrapped UOP
     TTermVal/Tag/Ext[k]      -> the underlying TTerm's fields
   so a TKernel passes transparently anywhere a TTerm is accepted. *)

BeginPackage["THVMLink`"];

TKernel::usage = "TKernel[t_TTerm] wraps a UOP_KERNEL term as a typed object.  TKernel[kid_Integer] resolves a kernel id back to its pinned heap term and wraps that.  Use Information[k, \"Properties\"] for the queryable property list, k[\"name\"] to fetch one, and k[] to dispatch.  A TKernel auto-coerces to its underlying TTerm in any UOp constructor.";

TKernelQ::usage = "TKernelQ[k] returns True if k is a well-formed TKernel object wrapping a UOP_KERNEL term.";

TKernelProgram::usage = "TKernelProgram[k] returns the kernel's program as a list of associations <|\"Op\", \"Sources\", \"Arg\", \"Numel\", \"Dtype\"|>.  Sources are tagged KIn[slot] for kernel-input references and KOp[idx] for SSA references to earlier program slots.";

TScheduleGraph::usage = "TScheduleGraph[] returns a Graph of the live kernel schedule: one vertex per emitted kernel, directed edges from producer kernel to consumer kernel labeled by the connecting TenDesc id.  External inputs (TenDescs with no producer kernel -- weights, host tensors) appear as cyan TEN-shaped vertices when \"ShowExternalInputs\" -> True (default).  Disconnected kernels render as isolated vertices.  Accepts all standard Graph options.";

KIn::usage = "KIn[slot] tags a kernel-program source operand that reads from input slot `slot` (one of TKernelInputs).  Returned by TKernelProgram in each op's Sources list.";

KOp::usage = "KOp[idx] tags a kernel-program source operand that reads from the output of program op index `idx`.  Returned by TKernelProgram in each op's Sources list.";

TKernelDispatch::usage = "TKernelDispatch[k] dispatches the kernel by TWnf-firing its underlying term.  Same as calling `k[]`.";

Begin["`Private`"];

(* === underlying-Term predicate ===
   tKernelTermQ[t]  True iff t is a TTerm whose tag is UOP and whose
   opcode is KERNEL.  Used by both TKernel construction and the
   guard on Information / dispatch UpValues. *)
tKernelTermQ[t_TTerm] := TTermTag[t] === $TagUOP && TTermExt[t] === $UopKernel
tKernelTermQ[___]     := False

(* Canonical internal form: TKernel[<|"Term" -> ..., "Kid" -> ...|>].
   tKernelInternalQ guards every UpValue so callers can't construct
   ill-formed TKernel[] shells that downstream code would crash on. *)
tKernelInternalQ[TKernel[a_Association]] :=
    KeyExistsQ[a, "Term"] && KeyExistsQ[a, "Kid"]
tKernelInternalQ[___] := False

TKernelQ = tKernelInternalQ;

(* Read kid out of the UOP_KERNEL cell's NUM(kid) child at base+1.
   kloc = TTermVal[t]; heap[kloc + 1] is the TAG_NUM kid cell. *)
kidOfKernelTerm[t_TTerm] := TTermVal[THeapRead[TTermVal[t] + 1]]

(* === constructors === *)

(* From a kernel TTerm: validate, extract kid, wrap canonically. *)
TKernel[t_TTerm /; tKernelTermQ[t]] := TKernel[<|
    "Term" -> t,
    "Kid"  -> kidOfKernelTerm[t]
|>]

(* From a kid integer: scan the heap for the pinned UOP_KERNEL cell
   whose NUM(kid) matches.  Pinning is done by emit_kernel_for_boundary
   in src/schedule/materialize.c -- one heap cell per emitted kernel
   carrying the kernel_term, so this scan always finds something for
   every live kid (1 .. KERNELS_NEXT - 1). *)
TKernel[kid_Integer] := Module[{n = THeapPos[], hit},
    hit = SelectFirst[
        Range[0, n - 1],
        Block[{c = THeapRead[#]},
            TTermTag[c] === $TagUOP &&
            TTermExt[c] === $UopKernel &&
            kidOfKernelTerm[c] === kid
        ] &,
        $Failed
    ];
    If[ hit === $Failed,
        Failure["TKernel", <|
            "MessageTemplate" -> "No pinned UOP_KERNEL heap cell found for kid `1`",
            "MessageParameters" -> {kid}
        |>],
        TKernel[THeapRead[hit]]
    ]
]

(* From a custom program description -- planned, not yet wired.
   Needs a thvm_wl_kernel_emit_program C bridge that allocates a
   fresh KernelEntry, populates input_tids/program/output_*, and
   pins a UOP_KERNEL cell.  Stub returns Failure with the API
   shape so callers can scaffold against it. *)
TKernel[spec_Association] /;
  KeyExistsQ[spec, "OutputShape"] && KeyExistsQ[spec, "Program"] :=
    Failure["TKernel", <|
        "MessageTemplate" -> "TKernel[<|...|>] custom-program construction needs the thvm_wl_kernel_emit_program C bridge (not yet implemented).  Spec keys: `1`",
        "MessageParameters" -> {Keys[spec]}
    |>]

(* === auto-coerce: a TKernel is a TTerm anywhere a TTerm is expected ===
   ttermRaw + TTermVal/Tag/Ext are the four entry points that every
   downstream UOp constructor uses.  Routing them through the inner
   Term value lets you write TUOpAdd[k, x] without an explicit unwrap. *)
TKernel /: ttermRaw [k:TKernel[a_Association] /; tKernelInternalQ[k]] := ttermRaw [a["Term"]]
TKernel /: TTermVal [k:TKernel[a_Association] /; tKernelInternalQ[k]] := TTermVal [a["Term"]]
TKernel /: TTermTag [k:TKernel[a_Association] /; tKernelInternalQ[k]] := TTermTag [a["Term"]]
TKernel /: TTermExt [k:TKernel[a_Association] /; tKernelInternalQ[k]] := TTermExt [a["Term"]]
TKernel /: TTermExpr[k:TKernel[a_Association] /; tKernelInternalQ[k]] := TTermExpr[a["Term"]]

(* === property surface (used by both Information and k["name"]) === *)

(* Decode KSRC_AS_INPUT(slot) vs program-index references.  Mirrors
   the KSRC_INPUT_FLAG / KSRC_INDEX macros in src/thvm.h:247. *)
$kSrcInputFlag = 16^^80000000;
decodeKSrc[s_Integer] := If[ BitAnd[s, $kSrcInputFlag] =!= 0,
    KIn[BitAnd[s, 16^^7FFFFFFF]],
    KOp[s]
]

(* Decode the flat MTensor returned by thvm_wl_kernel_info into a
   list of association records.  Layout (mirrors thvmlink.c:836):
     [n_inputs, n_ops, output_numel, output_dtype,
      op0_opcode, op0_n_src, op0_src0, op0_src1, op0_arg, op0_numel,
      ... repeat per op ...] *)
decodeKernelInfo[kid_Integer] := Block[{raw, nInputs, nOps, header},
    raw = Normal @ $kernelInfoFn[kid];
    nInputs = raw[[1]];
    nOps    = raw[[2]];
    header  = <|
        "InputCount"  -> nInputs,
        "OpCount"     -> nOps,
        "OutputNumel" -> raw[[3]],
        "OutputDtype" -> dtypeName[raw[[4]]]
    |>;
    {header,
     Table[
        Block[{base = 4 + 6 (i - 1)},
            <|
                "Op"      -> Lookup[$uopNames, raw[[base + 1]], "?"],
                "Sources" -> Take[
                    {decodeKSrc[raw[[base + 3]]], decodeKSrc[raw[[base + 4]]]},
                    raw[[base + 2]]
                ],
                "Arg"     -> raw[[base + 5]],
                "Numel"   -> raw[[base + 6]]
            |>
        ],
        {i, nOps}
     ]}
]

(* Read the kid's row out of TKernelTable[].  Layout (mirrors
   thvm_wl_kernel_table): {n_inputs, output_tid, fired, spliced,
   consumer_count, output_numel, output_dtype}.  Returned as an
   association so individual properties can index by name. *)
kernelRowAsoc[kid_Integer] := Block[{row = TKernelTable[][[kid]]},
    <|
        "InputCount"    -> row[[1]],
        "OutputTid"     -> row[[2]],
        "Fired"         -> row[[3]] === 1,
        "Spliced"       -> row[[4]] === 1,
        "ConsumerCount" -> row[[5]],
        "OutputNumel"   -> row[[6]],
        "OutputDtype"   -> dtypeName[row[[7]]]
    |>
]

(* The canonical Information property list.  Listed in the same
   order Information[k, "Properties"] returns -- the order is the
   public contract, so don't permute without thought. *)
$tKernelProperties = {
    "Kid", "Term", "OutputTid", "OutputShape", "OutputDtype",
    "OutputNumel", "InputCount", "InputTids", "InputTensors",
    "OpCount", "Program", "Fired", "Spliced", "ConsumerCount"
};

(* Build a TAG_TEN-wrapped TTerm from a tid.  Looks up the tid's
   dtype out of TTensTable[] (column 3) so the packed Term has a
   correct ext field; without this, downstream TTensorShape /
   TTensorData calls on the result would mis-decode dtype. *)
tenTermFromTid[tid_Integer] := With[{
    dtype = TTensTable[][[tid, 3]]
},
    packTerm[0, $TagTEN, dtype, tid]
]

(* Single-property fetch.  Composed from kernelRowAsoc + decoded
   program; "Term" / "Kid" come straight from the wrapper. *)
tKernelProp[k:TKernel[a_Association], "Kid"]   := a["Kid"]
tKernelProp[k:TKernel[a_Association], "Term"]  := a["Term"]
tKernelProp[k:TKernel[a_Association], "InputTids"]    := TKernelInputs[a["Kid"]]
tKernelProp[k:TKernel[a_Association], "InputTensors"] :=
    tenTermFromTid /@ TKernelInputs[a["Kid"]]
tKernelProp[k:TKernel[a_Association], "Program"]      := decodeKernelInfo[a["Kid"]][[2]]
tKernelProp[k:TKernel[a_Association], "OpCount"]      := decodeKernelInfo[a["Kid"]][[1]]["OpCount"]
tKernelProp[k:TKernel[a_Association], "OutputShape"]  := With[{
    tid = kernelRowAsoc[a["Kid"]]["OutputTid"]
},
    If[ tid > 0, TTensorShape[tenTermFromTid[tid]], Missing["NoOutput"]]
]
tKernelProp[k:TKernel[a_Association], prop_String] := Lookup[
    kernelRowAsoc[a["Kid"]], prop, Missing["UnknownProperty", prop]
]

(* === Information surface ===
   Two UpValues: one for the property-list query, one for individual
   fetches.  The MatchQ pattern on the second covers any string a
   user passes in -- including ones we don't know about, which fall
   through to a Missing[] via tKernelProp's catchall. *)
TKernel /: Information[k:TKernel[_Association] /; tKernelInternalQ[k], "Properties"] :=
    $tKernelProperties

TKernel /: Information[k:TKernel[_Association] /; tKernelInternalQ[k], prop_String] :=
    tKernelProp[k, prop]

(* Property list as the default Information[k] form too, so
   `Information[k]` shows the summary-box + property list. *)
TKernel /: Information[k:TKernel[_Association] /; tKernelInternalQ[k]] :=
    Association[(# -> tKernelProp[k, #]) & /@ $tKernelProperties]

(* === call syntax ===
   k[]            -- dispatch (TWnf the underlying kernel term).
                     If the kernel has already fired this is a
                     no-op; if its inputs aren't yet realized the
                     wnf loop walks back through their producer
                     kernels first.
   k["prop"]      -- shorthand for Information[k, "prop"].
   k[args__TTerm] -- planned: rebind inputs and dispatch.  Stub
                     returns Failure for now (would need
                     materialize_inplace_rebind). *)
k_TKernel[] /; tKernelInternalQ[k] :=
    TWnf[k[[1]]["Term"]]

k_TKernel[prop_String] /; tKernelInternalQ[k] :=
    tKernelProp[k, prop]

k_TKernel[args__TTerm] /; tKernelInternalQ[k] :=
    Failure["TKernel", <|
        "MessageTemplate" -> "TKernel[][args] input rebinding is not yet implemented (`1` args).  Use TKernel[][] to dispatch with the baked-in input_tids.",
        "MessageParameters" -> {Length[{args}]}
    |>]

TKernelDispatch[k_TKernel /; tKernelInternalQ[k]] := k[]

TKernelProgram[k_TKernel /; tKernelInternalQ[k]] := tKernelProp[k, "Program"]

(* === MakeBoxes summary ===
   Mirrors the TUOp summary-box pattern but surfaces the kernel's
   identifying fields (kid, output shape, input count, fired flag).
   Two rows so the summary is informative-at-a-glance without
   requiring an Information[] follow-up. *)
tKernelSummaryIcon[] := Graphics[
    {
        EdgeForm[LightDarkSwitched[Black, White]],
        FaceForm[LightDarkSwitched[Lighter[StandardOrange, 0.55], Darker[StandardOrange, 0.4]]],
        Rectangle[{-0.6, -0.6}, {0.6, 0.6}, RoundingRadius -> 0.18]
    },
    ImageSize -> Dynamic[{Automatic, 3.5 CurrentValue["FontCapHeight"] / AbsoluteCurrentValue[Magnification]}],
    PlotRangePadding -> Scaled[0.05]
]

TKernel /: MakeBoxes[k:TKernel[a_Association] /; tKernelInternalQ[k], fmt_] :=
    With[{
        kid     = a["Kid"],
        row     = kernelRowAsoc[a["Kid"]],
        nProg   = TTermVal[a["Term"]],   (* base loc, useful for debug *)
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

(* === TScheduleGraph -- DAG-of-kernels view ===

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
   isolated vertices in the layered embedding -- they're still
   part of the schedule, they just don't share TenDescs with the
   rest. *)

(* Vertex draw funnels through the shared Style.wl primitives so a
   KERNEL here matches a KERNEL in THeapGraph and the Style.wl
   palette is the only place colors live. *)

(* Compose a short per-kernel label.  Two lines:
     kid, op count
     output shape and dtype (or "no out" for the rare bail). *)
scheduleKernelLabel[kid_Integer] := Block[{
    info = decodeKernelInfo[kid],
    row  = kernelRowAsoc[kid],
    shape
},
    shape = With[{tid = row["OutputTid"]},
        If[ tid > 0, TTensorShape[tenTermFromTid[tid]], {}]];
    "k" <> ToString[kid] <> " (" <> ToString[info[[1]]["OpCount"]] <> "ops)"
        <> "\n" <> ToString[shape] <> " " <> row["OutputDtype"]
]

scheduleExternalLabel[tid_Integer] := Block[{
    shape = TTensorShape[tenTermFromTid[tid]],
    dtype = TTensorDType[tenTermFromTid[tid]]
},
    "t" <> ToString[tid] <> "\n"
        <> ToString[shape] <> " "
        <> If[StringQ[dtype], dtype, "?"]
]

Options[TScheduleGraph] = Join[
    {"ShowExternalInputs" -> True},
    Options[Graph]
];

TScheduleGraph[opts : OptionsPattern[]] := Block[{
    nKernels = TKernelCount[] - 1,
    showExt = OptionValue["ShowExternalInputs"],
    tens, kernelVerts, edges, edgeLabels, extTids, extVerts,
    vshapes
},
    If[ nKernels <= 0, Return[Graph[{}, ImageSize -> 240]]];
    tens = TTensTable[];

    kernelVerts = Table["k" <> ToString[k], {k, nKernels}];

    (* For each kernel, walk its input_tids and emit one edge per
       tid back to the producing kernel (or to an external-TEN
       vertex if there's no producer).  Edge label is the tid. *)
    {edges, edgeLabels, extTids} = Block[{es = {}, ls = {}, exs = {}},
        Do[
            Block[{
                inTids = TKernelInputs[kid],
                consumerVert = "k" <> ToString[kid],
                producerKid, srcVert
            },
                Do[
                    producerKid = If[ tid > 0 && tid <= Length[tens],
                                      tens[[tid, 1]], 0];
                    srcVert = If[ producerKid =!= 0,
                                  "k" <> ToString[producerKid],
                                  "t" <> ToString[tid]];
                    If[ producerKid === 0, AppendTo[exs, tid]];
                    AppendTo[es, DirectedEdge[srcVert, consumerVert]];
                    AppendTo[ls, DirectedEdge[srcVert, consumerVert] -> tid],
                    {tid, inTids}
                ]
            ],
            {kid, nKernels}
        ];
        {es, ls, DeleteDuplicates[exs]}
    ];

    extVerts = If[ showExt, "t" <> ToString[#] & /@ extTids, {}];
    (* Drop edges pointing into hidden external vertices, otherwise
       Graph would auto-add stub vertices for them. *)
    If[ !showExt,
        edges = Select[edges, !StringStartsQ[#[[1]], "t"] &];
        edgeLabels = Select[edgeLabels, !StringStartsQ[#[[1, 1]], "t"] &]
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
        VertexSize          -> 0.45,
        EdgeLabelStyle      -> Directive[FontFamily -> "Helvetica", FontSize -> 9,
                                         LightDarkSwitched[Black, White]],
        EdgeStyle           -> edgeStyleDirective,
        GraphLayout         -> "LayeredDigraphEmbedding",
        PerformanceGoal     -> "Quality"
    ]
]

End[];
EndPackage[];
