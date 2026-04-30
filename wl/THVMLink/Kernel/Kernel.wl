(* ::Package:: *)
(* Kernel.wl - TKernel object: a typed wrapper around a UOP_KERNEL
   TTerm that exposes the C-side KernelEntry contents (input tids,
   output shape/dtype, program ops) plus the codegen / dispatch
   profiling surface (FLOPS, dispatch route, cumulative microseconds,
   rendered C / Metal source, JIT dylib path) through a uniform
   Information[] API; dispatches via call syntax `k[]`; auto-coerces
   back to its underlying TTerm so it can be embedded inside other
   UOps (TUOpAdd[k, x] etc.) without an explicit unwrap.

   Public surface:
     TKernel[t_TTerm]          wrap a TTerm whose tag is UOP_KERNEL
     TKernel[kid_Integer]      resolve a kid back to its pinned heap
                               kernel_term and wrap that

     Information[k]            full Association of all properties
     Information[k, "Properties"]  the property name list (public contract)
     Information[k, "Name"]    fetch one property
     k["Name"]                 shortcut for Information[k, "Name"]
     k[]                       dispatch via TWnf (no-op if already fired)

   Top-level convenience accessors all delegate to the property
   surface, so `TKernelFlops[kid] == TKernel[kid]["Flops"]`.

   Where the rest of TKernel lives:
     - MakeBoxes summary box ........ Format.wl
     - TScheduleGraph (kernel DAG) ... Visualization.wl
     - heap-graph KERNEL rendering ... Visualization.wl (via kerVertexId) *)

BeginPackage["THVMLink`"];

TKernel::usage = "TKernel[t_TTerm] wraps a UOP_KERNEL term as a typed object.  TKernel[kid_Integer] resolves a kernel id back to its pinned heap term and wraps that.  Use Information[k, \"Properties\"] for the queryable property list, k[\"Name\"] to fetch one, and k[] to dispatch.  A TKernel auto-coerces to its underlying TTerm in any UOp constructor.";

TKernelQ::usage = "TKernelQ[k] returns True if k is a well-formed TKernel object wrapping a UOP_KERNEL term.";

TKernelProgram::usage = "TKernelProgram[k] returns the kernel's program as a list of associations <|\"Op\", \"Sources\", \"Arg\", \"Numel\", \"Dtype\"|>.  Sources are tagged KIn[slot] for kernel-input references and KOp[idx] for SSA references to earlier program slots.";

KIn::usage = "KIn[slot] tags a kernel-program source operand that reads from input slot `slot` (one of TKernelInputs).  Returned by TKernelProgram in each op's Sources list.";

KOp::usage = "KOp[idx] tags a kernel-program source operand that reads from the output of program op index `idx`.  Returned by TKernelProgram in each op's Sources list.";

TKernelDispatch::usage = "TKernelDispatch[k] dispatches the kernel by TWnf-firing its underlying term.  Same as calling `k[]`.";

(* === kernel-entry introspection (kid-keyed, no TKernel wrap needed) === *)

TKernelCount::usage    = "TKernelCount[] returns the number of compiled KernelEntrys in the kernel side table.";
TKernelProgramCacheSize::usage = "TKernelProgramCacheSize[] returns the number of distinct KProgOp[] arrays interned in the kernel-program hash-cons cache.  After a TRealize, this is at most TKernelCount[]-1; structurally identical kernels (e.g. successive iters of a recursive lambda's step) share a single entry.";
TKernelInfo::usage     = "TKernelInfo[kid] returns an Association describing the linearized program stored at KERNELS[kid].  Equivalent to TKernel[kid][\"Program\"] paired with the header (n_inputs, n_ops, output_numel, output_dtype).";
TKernelScalarUops::usage = "TKernelScalarUops[kid] returns the post-lowering scalar-UOp graph snapshot stored at KERNELS[kid].scalar_uops, as a List of Associations (one per scalar op, with keys \"id\", \"op\", \"dtype\", \"src\", \"extra\", and -- for S_RANGE -- \"axis_type\" and \"extent\").  Returns Missing[\"NotLowered\"] when the kernel was emitted via the legacy per-tensor-UOp visit() path (i.e. rangeify lowering was off or didn't apply).  Slot 0 (S_NONE sentinel) is included so list indices match C-side ScalarUop[] indices; live ops occupy positions [2..].";

(* === codegen / profiling surface (delegated to TKernel properties) === *)

TKernelSource::usage      = "TKernelSource[kid] / TKernelSource[kid, backend] returns the source kid's program would render to on the named backend (\"C\" / \"Metal\").  Default backend is the active one (THVM_BACKEND env var; \"C\" otherwise).  Empty string when the program contains ops outside cg_supports (REDUCE, movement) -- those fall back to the interpreter.  Property surface: TKernel[kid][\"Source\"] / TKernel[kid][\"Source\", backend].";
TKernelFlops::usage         = "TKernelFlops[kid] = TKernel[kid][\"Flops\"].  Static FLOPS estimate for one execution of kid (sum over KProgOp[]: 1 flop per elementwise op per element, 1 flop per REDUCE source element).  0 for movement / load.";
TKernelDispatchKind::usage  = "TKernelDispatchKind[kid] = TKernel[kid][\"DispatchKind\"].  The route the last fire of kid took: \"none\", \"blas-dot\", \"blas-gemv\", \"blas-gemm\", \"jit\", or \"interpreter\".";
TKernelDispatchCount::usage = "TKernelDispatchCount[kid] = TKernel[kid][\"DispatchCount\"].  Cumulative number of times kid has fired since thvm_init.";
TKernelTotalUs::usage       = "TKernelTotalUs[kid] = TKernel[kid][\"TotalUs\"].  Cumulative wallclock microseconds across every fire of kid.";
TKernelJitDylibPath::usage  = "TKernelJitDylibPath[kid] = TKernel[kid][\"JitDylibPath\"].  On-disk path the JIT cache uses for kid's compiled .dylib (deterministic from the program hash).  File may not exist if the JIT bailed at codegen.";
TKernelProfile::usage       = "TKernelProfile[kid] returns Information[TKernel[kid]] -- an Association of every property listed by Information[k, \"Properties\"], including the profiling fields.";
TProfileAll::usage          = "TProfileAll[] returns TKernelProfile for every live kernel, keyed by kid.";

(* === axis-typed scheduling slot (mirrors tinygrad's Kernel.opt) ===
   WL is a thin shell -- the axis structure + apply-opt rewrite logic
   lives in C (src/codegen/axis.c + src/codegen/apply_opt.c).  This
   block just packages the C-side state into typed WL objects with
   summary boxes. *)

TOpt::usage = "TOpt[op_String, axis_Integer, arg_] is a typed kernel-optimization action.  Mirrors tinygrad's `Opt(OptOps.UPCAST, axis=2, arg=4)` (tinygrad/codegen/opt/kernel.py:Opt).  Valid ops: \"UPCAST\", \"UNROLL\", \"LOCAL\", \"GROUP\", \"GROUPTOP\", \"SWAP\", \"PADTO\", \"NOLOCALS\", \"TC\".  axis is 0-indexed.  arg meaning is op-specific (for UPCAST/UNROLL: split factor; for SWAP: target axis index).  Apply via `TKernelApplyOpt[kid, TOpt[...]]`; introspect via `TKernelOpts[kid]`.";

TKernelOpts::usage = "TKernelOpts[kid] returns a wrapped `TKernelOpts[<|\"Kid\", \"AxisTypes\", \"FullShape\", \"Applied\"|>]` summarising the kernel's axis-typed scheduling plan as carried in C.  AxisTypes parallels FullShape: each entry is one of \"LOOP\" / \"REDUCE\" / \"UPCAST\" / \"UNROLL\" / \"LOCAL\" / \"GLOBAL\" / \"GROUP_REDUCE\".  Default (no opts applied) is all LOOP for elementwise output axes plus a trailing REDUCE for reduce kernels.  Applied is the chronological list of TOpt actions.";

TKernelApplyOpt::usage = "TKernelApplyOpt[kid, TOpt[op, axis, arg]] mutates the kernel's C-side KernelAxes via axes_apply_opt: splits the indicated axis (UPCAST/UNROLL/LOCAL/GROUP), swaps two axes (SWAP), or records the opt for the codegen consumer (PADTO/NOLOCALS/TC).  Returns the updated TKernelOpts wrapper.  Returns Failure[\"opt-rejected\"] on validation failure (axis out of range, arg doesn't divide axis size, opts table full).";

TKernelProposed::usage = "TKernelProposed[kid] returns a list of `TOpt[...]` candidates suggested by the C-side shape-heuristic proposer (`kernel_opts_propose` in `src/codegen/propose.c`).  Today's heuristics propose UNROLL on the reduce axis at factors {2, 4, 8, 16} where divisible plus UPCAST on the output axis for elementwise kernels at the same factor set; future passes add LOCAL / GROUP rules as the codegen variant emitter grows.  TKernelAutotune / TKernelVariants consume this list.";

TKernelVariant::usage = "TKernelVariant[<|\"Kid\" -> _, \"Opt\" -> TOpt | None, \"WallUs\" -> _Real|>] is a typed wrapper for one proposed-or-measured kernel variant.  Returned by TKernelVariants[kid].  Carries summary boxes so notebook output renders the (op, axis, arg) triple + measured wallclock per fire next to the kid.";

TKernelVariants::usage = "TKernelVariants[kid] returns a list of TKernelVariant: one for the no-opt baseline followed by one per TKernelProposed candidate.  Each has WallUs measured by 5 back-to-back fires (min wallclock).  Like TKernelAutotune internally but reports every candidate's measurement instead of just applying the winner -- useful for inspecting what the proposer found and why a particular winner was picked.  Side effect: leaves the kernel's KernelAxes at the BASELINE (no opts applied) since the measurements imply the user wants to inspect, not commit.";

TKernelAutotune::usage = "TKernelAutotune[kid] benchmarks every TKernelProposed candidate against the no-opt baseline (5 dispatches each, min wallclock), applies the winning TOpt to the kernel's C-side KernelAxes, and returns the resulting TKernelOpts.  Because axes live on the shared KpCacheSlot (per-program-shape sharing), the winner auto-applies to every other kid with the same KProgOp[] -- a training loop that emits one new kid per step inherits the autotuned variant from iter 2 onward.  Returns the unchanged TKernelOpts (no opts applied) if no candidate beat baseline.";

TKernelAutotuneAll::usage = "TKernelAutotuneAll[] runs TKernelAutotune on every currently-live kernel (kid 1..TKernelCount[]-1) and returns an Association mapping kid -> TKernelOpts after tuning.  Useful as a one-shot pre-warm before a training loop: every program shape gets a winner cached in its KpCacheSlot, so subsequent dispatches use the optimised variant from iter 1.  Alternative to setting THVM_AUTOTUNE=1 (which auto-tunes on first dispatch).";

(* === C-side kernel side-table accessors ===
   Live here (rather than MemoryPlan.wl) because they're the core
   kernel-introspection bridge -- every consumer that walks
   kid -> kernel info needs them.  Tensor.wl owns the parallel
   TenDesc accessors; MemoryPlan.wl owns the per-backend buf
   accessors. *)
TKernelTable::usage  = "TKernelTable[] returns a list of {n_inputs, output_tid, fired, spliced, consumer_count, output_numel, output_dtype} per kernel (kid 1 .. KERNELS_NEXT - 1).  Snapshot, not a live view.";
TKernelInputs::usage = "TKernelInputs[kid] returns the input_tids of kernel `kid` (length n_inputs).";

(* Forward-decl TTensTable: defined in Tensor.wl which loads
   alphabetically AFTER Kernel.wl.  Without the public stub here,
   Kernel.wl Private references resolve to a private phantom and
   Tensor.wl's later DownValue is shadowed. *)
TTensTable;

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

(* === C-side kernel side-table accessors ===
   Loader symbols ($kernelTableFn etc.) live in THVMLink.wl alongside
   every other LibraryFunctionLoad call; both files share
   THVMLink`Private` so the references resolve. *)
TKernelTable[]           := (ensureInit[]; Partition[Normal @ $kernelTableFn[], 7])
TKernelInputs[k_Integer] := (ensureInit[]; Normal @ $kernelInputsFn[k])

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
TKernel[kid_Integer] := Module[{lo = THeapBase[], n = THeapPos[], hit},
    hit = SelectFirst[
        Range[lo, n - 1],
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
   public contract, so don't permute without thought.

   Three groups:
     identity / shape    : Kid, Term, OutputTid, OutputShape, OutputDtype,
                           OutputNumel, InputCount, InputTids, InputTensors,
                           OpCount, Program, Fired, Spliced, ConsumerCount
     codegen output      : Source[backend], JitDylibPath
     dispatch profile    : Flops, DispatchKind, DispatchCount, TotalUs,
                           AvgUs, GFlopsPerSec *)
$tKernelProperties = {
    "Kid", "Term", "OutputTid", "OutputShape", "OutputDtype",
    "OutputNumel", "InputCount", "InputTids", "InputTensors",
    "OpCount", "Program", "Fired", "Spliced", "ConsumerCount",
    "Source", "JitDylibPath",
    "Flops", "DispatchKind", "DispatchCount", "TotalUs",
    "AvgUs", "GFlopsPerSec"
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

(* C-side dispatch enum -> readable label.  Mirrors KDispatchKind in
   src/thvm.h. *)
$dispatchKindNames = <|
    0 -> "none",        1 -> "blas-dot",   2 -> "blas-gemv", 3 -> "blas-gemm",
    4 -> "jit",         5 -> "interpreter",
    6 -> "metal-jit",   7 -> "metal-op"
|>;
decodeDispatchKind[k_Integer] := Lookup[$dispatchKindNames, k, "unknown"]

(* Single-property fetch.  Composed from kernelRowAsoc + decoded
   program; "Term" / "Kid" come straight from the wrapper.  All
   profiling/source clauses ensureInit + call the loader fn. *)
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

(* === codegen / profile properties ===
   Each routes through the loader fn declared in THVMLink.wl.  Both
   files share THVMLink`Private` so the $...Fn symbols resolve. *)
tKernelProp[k:TKernel[a_Association], "Source"]              := TKernelSource[a["Kid"]]
tKernelProp[k:TKernel[a_Association], "Source", backend_String] := TKernelSource[a["Kid"], backend]
tKernelProp[k:TKernel[a_Association], "JitDylibPath"] := (ensureInit[]; $kernelJitDylibPathFn[a["Kid"]])
tKernelProp[k:TKernel[a_Association], "Flops"]         := (ensureInit[]; $kernelFlopsFn[a["Kid"]])
tKernelProp[k:TKernel[a_Association], "DispatchKind"]  := (ensureInit[];
    decodeDispatchKind[$kernelDispatchKindFn[a["Kid"]]])
tKernelProp[k:TKernel[a_Association], "DispatchCount"] := (ensureInit[]; $kernelDispatchCountFn[a["Kid"]])
tKernelProp[k:TKernel[a_Association], "TotalUs"]       := (ensureInit[]; $kernelTotalUsFn[a["Kid"]])
tKernelProp[k:TKernel[a_Association], "AvgUs"]         := With[{
    n = tKernelProp[k, "DispatchCount"], us = tKernelProp[k, "TotalUs"]
},
    If[n > 0, N[us / n], 0]
]
tKernelProp[k:TKernel[a_Association], "GFlopsPerSec"]  := With[{
    n     = tKernelProp[k, "DispatchCount"],
    us    = tKernelProp[k, "TotalUs"],
    flops = tKernelProp[k, "Flops"]
},
    If[us > 0 && n > 0, N[flops * n / (us * 1000)], 0]
]

(* Catchall: row-association lookup for any string we don't have a
   dedicated clause for, with a Missing[] fallback for unknown keys. *)
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

(* === top-level kid-keyed convenience accessors ===
   These call the loader fns directly rather than routing through
   TKernel[kid] -- TKernel's heap-walk constructor can't find the
   UOP_KERNEL cell once it's been substituted away post-fire, but
   the C-side KERNELS[kid] entry persists for the whole session and
   is what the loader fns read.  When a TKernel object is in hand,
   the property surface (`k["Flops"]`) and these accessors return
   identical values via the same loaders. *)
tActiveBackendName[] := With[{e = Environment["THVM_BACKEND"]},
    If[ StringQ[e] && ToLowerCase[e] === "metal", "Metal", "C"]]

TKernelSource[kid_Integer]                       := TKernelSource[kid, tActiveBackendName[]]
TKernelSource[kid_Integer, "C"]                  := (ensureInit[]; $kernelSourceCFn[kid])
TKernelSource[kid_Integer, "Metal"]              := (ensureInit[]; $kernelSourceMetalFn[kid])
TKernelSource[kid_Integer, b_String] := Failure["UnknownBackend",
    <|"Message" -> "TKernelSource backend must be \"C\" or \"Metal\"",
      "Backend" -> b|>]
TKernelFlops[kid_Integer]         := (ensureInit[]; $kernelFlopsFn[kid])
TKernelDispatchKind[kid_Integer]  := (ensureInit[];
    decodeDispatchKind[$kernelDispatchKindFn[kid]])
TKernelDispatchCount[kid_Integer] := (ensureInit[]; $kernelDispatchCountFn[kid])
TKernelTotalUs[kid_Integer]       := (ensureInit[]; $kernelTotalUsFn[kid])
TKernelJitDylibPath[kid_Integer]  := (ensureInit[]; $kernelJitDylibPathFn[kid])

(* TKernelProfile = same Association shape Information[TKernel[kid]]
   would return, but synthesized directly from the loader fns so it
   keeps working after the kernel term has fired (and its heap pin
   has been substituted away). *)
TKernelProfile[kid_Integer] := <|
    "Kid"           -> kid,
    "Flops"         -> TKernelFlops[kid],
    "DispatchKind"  -> TKernelDispatchKind[kid],
    "DispatchCount" -> TKernelDispatchCount[kid],
    "TotalUs"       -> TKernelTotalUs[kid],
    "AvgUs"         -> If[ TKernelDispatchCount[kid] > 0,
                           N[TKernelTotalUs[kid] / TKernelDispatchCount[kid]],
                           0],
    "GFlopsPerSec"  -> If[ TKernelTotalUs[kid] > 0 && TKernelDispatchCount[kid] > 0,
                           N[TKernelFlops[kid] * TKernelDispatchCount[kid] / (TKernelTotalUs[kid] * 1000)],
                           0],
    "JitDylibPath"  -> TKernelJitDylibPath[kid],
    "Source"        -> TKernelSource[kid]
|>

(* === axis-typed scheduling slot ============================
   Thin LibraryLink wrappers over the C-side KernelAxes
   (src/codegen/axis.c).  The C side owns the axis structure +
   apply-opt rewrite logic; WL just packages the snapshot into
   typed objects with summary boxes (Format.wl).  All previous
   WL-side scaffold (defaultAxisTypes, defaultFullShape,
   $kernelAppliedOpts side store) is retired -- C is the single
   source of truth. *)

(* String <-> KOP_ ordinal.  Order MUST match KOP_* in src/thvm.h.
   KOP_NONE = 0 is the empty-slot sentinel, never user-visible. *)
$kopNames = {"NONE", "UPCAST", "UNROLL", "LOCAL", "GROUP",
             "GROUPTOP", "SWAP", "PADTO", "NOLOCALS", "TC"}

kopOrdinal[op_String] := With[{i = FirstPosition[$kopNames, op, {-1}][[1]]},
    If[ i > 0, i - 1, $Failed]]

kopName[ord_Integer] := If[ ord >= 0 && ord < Length[$kopNames],
    $kopNames[[ord + 1]], "?"]

(* String <-> KAX_ ordinal.  Order MUST match KAX_* in src/thvm.h. *)
$kaxNames = {"LOOP", "REDUCE", "UPCAST", "UNROLL",
             "LOCAL", "GLOBAL", "GROUP_REDUCE"}

kaxName[ord_Integer] := If[ ord >= 0 && ord < Length[$kaxNames],
    $kaxNames[[ord + 1]], "?"]

(* Decode the packed {Integer, 1} payload from thvm_wl_kernel_axes_get
   into a {axisTypes, fullShape, applied} triple.  Layout:
     [0]  n_axes,  [1] n_applied,
     [2..2+n_axes-1]                  axis_types[i],
     [2+n_axes..2+2*n_axes-1]         full_shape[i],
     [2+2*n_axes..]                   applied as (op, axis, arg) triples. *)
decodeKernelAxes[packed_List] := Module[{nA, nO, base, axisTypes, fullShape, applied},
    If[ Length[packed] < 2, Return[{{}, {}, {}}] ];
    nA = packed[[1]]; nO = packed[[2]];
    axisTypes = kaxName /@ packed[[3 ;; 2 + nA]];
    fullShape = packed[[3 + nA ;; 2 + 2*nA]];
    base      = 2 + 2*nA;
    applied   = Table[
        TOpt[ kopName @ packed[[base + 3*(i - 1) + 1]],
              packed[[base + 3*(i - 1) + 2]],
              packed[[base + 3*(i - 1) + 3]] ],
        {i, nO}];
    {axisTypes, fullShape, applied}]

TKernelOpts[kid_Integer] := (ensureInit[];
    Module[{packed, decoded},
        packed  = Normal @ $kernelAxesGetFn[kid];
        decoded = decodeKernelAxes[packed];
        TKernelOpts[<|
            "Kid"       -> kid,
            "AxisTypes" -> decoded[[1]],
            "FullShape" -> decoded[[2]],
            "Applied"   -> decoded[[3]]
        |>]
    ])

TKernelApplyOpt[kid_Integer, TOpt[op_String, axis_Integer, arg_Integer]] :=
    (ensureInit[];
        Module[{ord = kopOrdinal[op]},
            If[ ord === $Failed, Return[Failure["unknown-opt", <|"Op" -> op|>]] ];
            If[ $kernelApplyOptFn[kid, ord, axis, arg] === 1,
                TKernelOpts[kid],
                Failure["opt-rejected", <|"Kid" -> kid, "Opt" -> TOpt[op, axis, arg]|>]
            ]
        ])

TKernelProposed[kid_Integer] := (ensureInit[];
    Module[{packed},
        packed = Normal @ $kernelProposeFn[kid];
        Table[
            TOpt[ kopName @ packed[[3*(i - 1) + 1]],
                  packed[[3*(i - 1) + 2]],
                  packed[[3*(i - 1) + 3]] ],
            {i, Length[packed]/3}
        ]
    ])

TKernelAutotune[kid_Integer] := (ensureInit[];
    $kernelAutotuneFn[kid];
    TKernelOpts[kid])

TKernelAutotuneAll[] := (ensureInit[];
    Association[ Table[k -> TKernelAutotune[k], {k, 1, TKernelCount[] - 1}] ])

(* Inspect-only sibling of TKernelAutotune: bench the no-opt
   baseline + each TKernelProposed candidate via the C-side
   kernel_bench_variants, return as a list of TKernelVariant.
   Slot 0 is always the baseline (Opt -> None); subsequent
   slots carry one TOpt each.  Leaves the kernel's axes at
   baseline so the user can pick what to apply via
   TKernelApplyOpt -- this is for inspection, not commit. *)
TKernelVariants[kid_Integer] := (ensureInit[];
    Module[{packed},
        packed = Normal @ $kernelBenchVariantsFn[kid];
        Table[
            With[{op = packed[[4*(i - 1) + 1]],
                  ax = packed[[4*(i - 1) + 2]],
                  ar = packed[[4*(i - 1) + 3]],
                  us = packed[[4*(i - 1) + 4]]},
                TKernelVariant[<|
                    "Kid"    -> kid,
                    "Opt"    -> If[ op === 0, None, TOpt[kopName[op], ax, ar]],
                    "WallUs" -> us
                |>]],
            {i, Length[packed]/4}
        ]
    ])


(* All currently-live kernels' profiles, indexed by kid. *)
TProfileAll[] := Association[
    Table[k -> TKernelProfile[k], {k, 1, TKernelCount[] - 1}]
]

(* === kernel-entry introspection ===
   Direct kid-keyed accessors.  The TKernel object's property
   surface ultimately routes through these too (decodeKernelInfo). *)
TKernelCount[]            := (ensureInit[]; $kernelCountFn[])
TKernelProgramCacheSize[] := (ensureInit[]; $kernelProgramCacheSizeFn[])

(* TKernelInfo[kid] returns a flat Association with the kernel's
   linearized program + shape metadata.  Used by tests + visualization
   overlays.  Same data as decodeKernelInfo above, repackaged with the
   snake_case keys callers expect. *)
TKernelInfo[kid_Integer] := Module[{raw = $kernelInfoFn[kid], n, nOps},
    n    = raw[[1]];
    nOps = raw[[2]];
    <|
      "n_inputs"     -> n,
      "n_ops"        -> nOps,
      "output_numel" -> raw[[3]],
      "output_dtype" -> dtypeName[raw[[4]]],
      "program"      -> Table[
          With[{base = 4 + (i - 1) * 6},
            <|
              "opcode" -> Lookup[$uopNames, raw[[base + 1]], "?"],
              "n_src"  -> raw[[base + 2]],
              "src"    -> { raw[[base + 3]], raw[[base + 4]] },
              "arg"    -> raw[[base + 5]],
              "numel"  -> raw[[base + 6]]
            |>
          ],
          {i, nOps}
      ]
    |>
]

(* Scalar-UOp introspection (Phase A of scalar_uops_lowering).
   Decodes the flat Integer MTensor from thvm_wl_kernel_scalar_uops
   into a List of Associations.  Returns Missing["NotLowered"] when
   the kernel didn't go through the rangeify path. *)
$scalarOpNames = <|
    0 -> "S_NONE",          1 -> "S_RANGE",         2 -> "S_DEFINE_PARAM",
    3 -> "S_DEFINE_OUTPUT", 4 -> "S_INDEX",         5 -> "S_LOAD",
    6 -> "S_STORE",         7 -> "S_BUFFERIZE",     8 -> "S_CONST",
    9 -> "S_ADD",          10 -> "S_MUL",          11 -> "S_NEG",
   12 -> "S_RECIP",        13 -> "S_EXP2",         14 -> "S_LOG2",
   15 -> "S_SQRT",         16 -> "S_CMPLT",        17 -> "S_CMPEQ",
   18 -> "S_REDUCE_SUM",   19 -> "S_REDUCE_MAX"
|>;

$scalarAxisNames = <|0 -> "LOOP", 1 -> "REDUCE", 2 -> "UNROLL", 3 -> "GLOBAL"|>;

TKernelScalarUops[kid_Integer] := Module[{raw, n, decoded},
    raw = Normal @ $kernelScalarUopsFn[kid];
    If[ raw === {} || First[raw] == 0,
      Return @ Missing["NotLowered"] ];
    n = First[raw];
    decoded = Table[
      With[{base = 1 + (i - 1) * 10,
            opCode = raw[[1 + (i - 1) * 10 + 1]],
            extra  = BitOr[
              raw[[1 + (i - 1) * 10 + 8]],
              BitShiftLeft[raw[[1 + (i - 1) * 10 + 9]], 32]]},
        With[{
          opName    = Lookup[$scalarOpNames, opCode, "S_?"],
          srcCount  = raw[[base + 3]],
          src       = raw[[base + 4 ;; base + 7]]
        },
          (* S_RANGE special-case: split extra into (extent, axis_type)
             so callers don't have to redo the bit math. *)
          If[ opName === "S_RANGE",
            <|
              "id"        -> i - 1,
              "op"        -> opName,
              "dtype"     -> dtypeName[raw[[base + 2]]],
              "src"       -> Take[src, srcCount],
              "extent"    -> BitAnd[extra, 16^^FFFFFFFF],
              "axis_type" -> Lookup[$scalarAxisNames,
                               BitShiftRight[extra, 32], "?"]
            |>,
            <|
              "id"     -> i - 1,
              "op"     -> opName,
              "dtype"  -> dtypeName[raw[[base + 2]]],
              "src"    -> Take[src, srcCount],
              "extra"  -> extra
            |>
          ]
        ]
      ],
      {i, n}];
    decoded
]

End[];
EndPackage[];
