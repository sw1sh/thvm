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

(* === codegen / profiling surface (delegated to TKernel properties) === *)

TKernelSource::usage      = "TKernelSource[kid] / TKernelSource[kid, backend] returns the source kid's program would render to on the named backend (\"C\" / \"Metal\").  Default backend is the active one (THVM_BACKEND env var; \"C\" otherwise).  Empty string when the program contains ops outside cg_supports (REDUCE, movement) -- those fall back to the interpreter.  Property surface: TKernel[kid][\"Source\"] / TKernel[kid][\"Source\", backend].";
TKernelFlops::usage         = "TKernelFlops[kid] = TKernel[kid][\"Flops\"].  Static FLOPS estimate for one execution of kid (sum over KProgOp[]: 1 flop per elementwise op per element, 1 flop per REDUCE source element).  0 for movement / load.";
TKernelDispatchKind::usage  = "TKernelDispatchKind[kid] = TKernel[kid][\"DispatchKind\"].  The route the last fire of kid took: \"none\", \"blas-dot\", \"blas-gemv\", \"blas-gemm\", \"jit\", or \"interpreter\".";
TKernelDispatchCount::usage = "TKernelDispatchCount[kid] = TKernel[kid][\"DispatchCount\"].  Cumulative number of times kid has fired since thvm_init.";
TKernelTotalUs::usage       = "TKernelTotalUs[kid] = TKernel[kid][\"TotalUs\"].  Cumulative wallclock microseconds across every fire of kid.";
TKernelJitDylibPath::usage  = "TKernelJitDylibPath[kid] = TKernel[kid][\"JitDylibPath\"].  On-disk path the JIT cache uses for kid's compiled .dylib (deterministic from the program hash).  File may not exist if the JIT bailed at codegen.";
TKernelProfile::usage       = "TKernelProfile[kid] returns Information[TKernel[kid]] -- an Association of every property listed by Information[k, \"Properties\"], including the profiling fields.";
TProfileAll::usage          = "TProfileAll[] returns TKernelProfile for every live kernel, keyed by kid.";

(* === axis-typed scheduling slot (mirrors tinygrad's Kernel.opt) === *)

Opt::usage = "Opt[op_, axis_, arg_] is a typed kernel-optimization action.  Mirrors tinygrad's `Opt(OptOps.UPCAST, axis=2, arg=4)` (tinygrad/codegen/opt/kernel.py:Opt).  Valid op names: \"UPCAST\", \"UNROLL\", \"LOCAL\", \"GROUP\", \"GROUPTOP\", \"SWAP\", \"PADTO\", \"NOLOCALS\", \"TC\".  axis is 0-indexed (thvm convention; tinygrad-side ports may want to subtract 1).  arg meaning is op-specific (for UPCAST/UNROLL: split factor; for SWAP: target axis index).  Apply via `TKernelApplyOpt[k, opt]`; introspect via `TKernelOpts[k]`.";

TKernelOpts::usage = "TKernelOpts[kid] returns the kernel's axis structure as <|\"AxisTypes\" -> {...}, \"FullShape\" -> {...}, \"Applied\" -> {Opt[...], ...}|>.  AxisTypes parallels FullShape: each entry is one of \"GLOBAL\" / \"LOCAL\" / \"LOOP\" / \"UPCAST\" / \"GROUP_REDUCE\" / \"REDUCE\" / \"UNROLL\".  Default (no opts applied) is all LOOP for elementwise output axes plus REDUCE for any reduction axes -- matching tinygrad's pre-opt Kernel.axis_types view.  Applied is the chronological list of `Opt[...]` actions.";

TKernelApplyOpt::usage = "TKernelApplyOpt[kid, opt_Opt] applies `opt` to the kernel's axis structure.  Today this is a WL-side scaffold that records the opt + updates the AxisTypes view (so TKernelOpts reflects it); the C-side codegen consumer is Phase 14+ (cg_emit_variants per docs/bench/phase10.md).  Returns the updated TKernelOpts Association.  Available opts: see Opt::usage.";

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
   WL-side scaffold for the kernel-optimization layer.  Tinygrad's
   `Kernel` (tinygrad/codegen/opt/kernel.py) carries `axis_types[]`
   alongside `full_shape[]`; opt actions like `Opt(UPCAST, 2, 4)`
   rewrite that axis structure (split N into N//4 + 4, mark the 4
   as UPCAST), and the linearizer emits the iteration nest
   accordingly.

   thvm's KernelEntry today holds a flat `KProgOp[]` post-
   linearization (one implicit `for i = 0..numel-1` loop), so the
   axis structure isn't materialised yet.  This block:

   - Defines the `Opt[op_String, axis_Integer, arg_]` typed object
     (mirrors tinygrad's `Opt(OptOps.X, axis, arg)`).
   - Synthesises a default AxisTypes view from the kernel's output
     shape: every output axis is LOOP, plus a REDUCE axis appended
     for kernels whose program ends in a UOP_REDUCE_* op.
   - Records applied opts in a per-kid side store and reflects them
     in TKernelOpts.

   The C-side codegen consumer (cg_emit_variants per
   docs/bench/phase10.md) is the Phase 14+ delivery.  Once it
   lands, this WL scaffold becomes the user-facing knob; the side
   store handoff becomes a kid-keyed dispatch the C-side reads. *)

(* Per-kid opts side store: kid -> <|"AxisTypes", "FullShape", "Applied"|>. *)
$kernelAppliedOpts = <||>

(* Resolve the output shape via the existing kid->row->tid chain.
   TTensorShape on the synthesised tenTerm decodes the TenDesc's
   view shape correctly (mirrors tKernelProp[..., "OutputShape"]). *)
kernelOutputShape[kid_Integer] := Module[{tid, term, shape},
    tid = kernelRowAsoc[kid]["OutputTid"];
    If[ ! IntegerQ[tid] || tid <= 0, Return[{}]];
    term  = tenTermFromTid[tid];
    shape = TTensorShape[term];
    If[ ListQ[shape], shape, {}]]

kernelEndsInReduce[kid_Integer] := Module[{prog = decodeKernelInfo[kid][[2]]},
    Length[prog] > 0 && StringMatchQ[Last[prog]["Op"], "REDUCE_" ~~ ___]]

defaultAxisTypes[kid_Integer] := With[{shape = kernelOutputShape[kid]},
    If[ kernelEndsInReduce[kid],
        Append[ConstantArray["LOOP", Length[shape]], "REDUCE"],
        ConstantArray["LOOP", Length[shape]]]]

defaultFullShape[kid_Integer] := With[{shape = kernelOutputShape[kid]},
    If[ kernelEndsInReduce[kid],
        (* Best-effort: append the last KProgOp's Numel / output_numel
           ratio as the reduce-axis size.  REDUCE collapses one axis,
           so input_numel / output_numel = reduce_axis_size. *)
        Module[{prog = decodeKernelInfo[kid][[2]], outNumel, redOp, axisSize},
            outNumel = TKernelInfo[kid][[1]]["OutputNumel"];
            redOp    = Last[prog];
            axisSize = If[ outNumel > 0, Quotient[redOp["Numel"], outNumel], 1];
            Append[shape, axisSize]],
        shape]]

TKernelOpts[kid_Integer] := With[{stored = Lookup[$kernelAppliedOpts, kid, <||>]},
    <|
        "AxisTypes" -> Lookup[stored, "AxisTypes", defaultAxisTypes[kid]],
        "FullShape" -> Lookup[stored, "FullShape", defaultFullShape[kid]],
        "Applied"   -> Lookup[stored, "Applied", {}]
    |>]

(* Apply an opt to the AxisTypes view.  WL-side bookkeeping today:
   rewrites the axis-types entry at `axis` to match the opt, splits
   the FullShape entry where the opt's arg is a split factor.  C-side
   codegen consumes this only once cg_emit_variants lands. *)
TKernelApplyOpt[kid_Integer, opt : Opt[op_String, axis_Integer, arg_]] :=
    Module[{cur, axisTypes, fullShape, applied, axTypeMap},
        cur       = TKernelOpts[kid];
        axisTypes = cur["AxisTypes"];
        fullShape = cur["FullShape"];
        applied   = cur["Applied"];
        axTypeMap = <|
            "UPCAST"   -> "UPCAST",  "UNROLL" -> "UNROLL",
            "LOCAL"    -> "LOCAL",   "GROUP"  -> "GROUP_REDUCE",
            "GROUPTOP" -> "GROUP_REDUCE"|>;
        Which[
            KeyExistsQ[axTypeMap, op],
                (* Split fullShape[[axis+1]] into outer + arg;
                   mark the new inner axis as op type. *)
                With[{outerSize = Quotient[fullShape[[axis + 1]], arg]},
                    fullShape = Insert[
                        ReplacePart[fullShape, axis + 1 -> outerSize],
                        arg, axis + 2];
                    axisTypes = Insert[axisTypes, axTypeMap[op], axis + 2]],
            op === "SWAP",
                With[{i = axis + 1, j = arg + 1},
                    fullShape = ReplacePart[fullShape,
                        {i -> fullShape[[j]], j -> fullShape[[i]]}];
                    axisTypes = ReplacePart[axisTypes,
                        {i -> axisTypes[[j]], j -> axisTypes[[i]]}]],
            True, Null];
        applied = Append[applied, opt];
        $kernelAppliedOpts[kid] = <|
            "AxisTypes" -> axisTypes,
            "FullShape" -> fullShape,
            "Applied"   -> applied|>;
        TKernelOpts[kid]
    ]


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
   legacy snake_case keys callers expect. *)
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

End[];
EndPackage[];
