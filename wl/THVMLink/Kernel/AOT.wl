(* ::Package:: *)
(* AOT.wl -- WL surface for ahead-of-time compilation.

   The Bend2-style fork-emitting AOT lives in src/aot/: the runtime
   (Task / Result / cont / worker) and the emitter that translates a
   TDef'd body into compilable C source (par_<name>_entry +
   par_<name>_cont_K + dispatch table).  This file exposes the emitter
   to WL.

   Usage:

       TInit[];
       TDef["count", TLam[n, TMatChain[
         <|0 -> TNum[1],
           1 -> TLam[p, TOp2["+", TApp[TRef["count"], TVar[p]],
                                  TApp[TRef["count"], TVar[p]]]]|>,
         TLam[ig, TEra[]]][n]]];

       src = TAOTEmit["count"];
       (* `src` is the C source string -- inspect, save, or compile
          via tests/test_aot_e2e.c-style harness.  See docs/aot.md. *)

   TAOTCompile (compile + dlopen + run in-process) lands once the
   runtime-OPS ABI is in place -- without it, the dylib's #include
   "thvm.c" gets its own copy of every global (HEAP, DEFS, ...) and
   the host can't share heap state.  For now TAOTEmit returns the
   source and tests can verify it via the standalone-binary path
   proven in tests/test_aot_e2e.c.
*)

BeginPackage["THVMLink`"];

GeneralUtilities`SetUsage[TAOTEmit, "TAOTEmit[name$] returns the C source string that thvm_aot_emit_program produces for the def registered under name$.
Inspect, save, or compile manually; TAOTCompile and TAOTRun automate those steps on top of this lower-level surface."];

GeneralUtilities`SetUsage[TAOTCompile, "TAOTCompile[name$] emits the def under name$, wraps it with the thvm runtime plus an aot_program_<name>_run entry, invokes clang to produce a dylib, stashes the path so TAOTRun can dlopen it, and returns the dylib path string.
Caching is by content, so an unchanged source reuses the same path and skips the clang call."];

GeneralUtilities`SetUsage[TAOTRun, "TAOTRun[name$, input$] dlopens the AOT'd dylib for name$ (TAOTCompile'd first), invokes its run entry with input$ as the argument Term, and returns the result wrapped as a TTerm. input$ may be a TTerm or a raw Integer (wrapped as TNum).
TAOTRun[name$, {arg$1, arg$2, $$}] runs a multi-argument def (up to 4 args); missing slots default to 0.
TAOTRun[name$, args$, Method \[Rule] spec$] routes to a backend: spec$ is \"CPU\", \"Metal\", or a list head with options such as {\"CPU\", \"NumThreads\" \[Rule] n$}.
NUM-reducing programs return a self-contained TTerm carrying the scalar; CTR-returning programs reference the dylib heap and need TAOTEmit plus a custom harness to decode host-side."];

GeneralUtilities`SetUsage[TAOTPath, "TAOTPath[name$] returns the dylib path stashed by TAOTCompile[name$], or Missing[\"NotCompiled\"] if the def has never been TAOTCompile'd."];

GeneralUtilities`SetUsage[TAOTSpSolve, "TAOTSpSolve[cnf$, nVars$] runs the Survey Propagation plus decimation SAT solver via Metal and returns {status$, assignment$}.
status$ is \"SAT\", \"UNSAT\", \"GAVE_UP\", or \"ERROR\"; assignment$ is a per-variable list of {-1, +1} values, meaningful only when status$ is \"SAT\". The solver hands off to the bitmask kernel once 24 or fewer variables remain unfixed.
Options: \"MaxIters\", \"Damping\", \"Threshold\"."];

GeneralUtilities`SetUsage[TAOTSurveyPropagate, "TAOTSurveyPropagate[cnf$, nVars$] runs Survey Propagation on the CNF formula via Metal and returns the per-edge final eta vector (length equals the total literals across clauses) after convergence or the iteration cap.
cnf$ is a list of clauses, each a list of signed integers (positive = +var index, negative = -var index, 1-based). Targets random k-SAT near the phase transition where CDCL struggles.
Options: \"MaxIters\", \"Damping\", \"Threshold\"."];

GeneralUtilities`SetUsage[TAOTSatBitmask, "TAOTSatBitmask[cnf$, nVars$] evaluates the CNF formula at every assignment in [0, 2^nVars$) on Metal via the aot_cnf_bitmask kernel.
cnf$ is a list of clauses, each a list of signed integers (positive = positive literal, negative = negated literal, magnitude = 1-based variable index).
Returns a packed Integer list of length 2^nVars$ where entry i$ is 1 if assignment i$ (bit j$ = value of variable j$+1) satisfies the formula, else 0. Direct bitwise CNF evaluation that bypasses IC reduction; requires nVars$ at most 30."];

GeneralUtilities`SetUsage[TAOTIcCollapse, "TAOTIcCollapse[term$, depth$] dispatches the static aot_ic_collapse PSO with grid 2^depth$ over a BOOK_HEAP-rooted SUP-tree at term$ (for example the result of TAOTRun[$$, Method \[Rule] \"Metal\"] run with THVM_AOT_METAL_KEEP_BOOK=1 set).
Each thread decodes its id into a binary path through the SUP-tree, drives the final leaf to WHNF on-thread via per-thread IC interaction inlines, and writes the resulting Term to its slot.
Returns a list of 2^depth$ TTerm leaves; filter ERA sentinels via TTermTag."];

GeneralUtilities`SetUsage[TAOTBatchOp2Fold, "TAOTBatchOp2Fold[rootLocs$] dispatches the batch kernel aot_eval_op2_fold_batch over a list of book_heap locations, each pointing at an OP2(NUM, NUM) cell, and returns the corresponding list of folded NUM TTerms.
Amortizes Metal kernel-launch overhead across many independent OP2 folds. Build the OP2 cells via TBookAlloc and TBookSet first; the kernel reads from book_heap directly."];

(* Forward-declare symbols owned by alphabetically-later siblings
   (Ref.wl, Switch.wl) that load AFTER AOT.wl.  Without these stub
   declarations, bare references in the `Private` block would resolve
   to fresh THVMLink`Private` symbols rather than the public ones the
   sibling files later attach downvalues to.  Same pattern Format.wl
   uses for late-loading TOpt / TKernelOpts / TKernelVariant. *)
{TDefName, TNum};

Begin["`Private`"];

(* Lazy-loaded library functions (matches the pattern used by every
   other Kernel/*.wl file -- `load` is shared from THVMLink.wl via
   the THVMLink`Private namespace). *)
$aotEmitFn    := $aotEmitFn    = load["thvm_wl_aot_emit_program",
    {Integer, "UTF8String"}, "UTF8String"]
$aotCompileFn := $aotCompileFn = load["thvm_wl_aot_compile",
    {Integer, "UTF8String"}, "UTF8String"]
$aotRunFn     := $aotRunFn     = load["thvm_wl_aot_run",
    {"UTF8String", "UTF8String", Integer}, Integer]
$aotRun4Fn    := $aotRun4Fn    = load["thvm_wl_aot_run4",
    {"UTF8String", "UTF8String", Integer, Integer, Integer, Integer},
    Integer]
$aotRun4PooledFn := $aotRun4PooledFn = load["thvm_wl_aot_run4_pooled",
    {"UTF8String", "UTF8String", Integer,
     Integer, Integer, Integer, Integer},
    Integer]

(* Path map populated by TAOTCompile so TAOTRun knows which dylib to
   dlopen for a given name.  Per-session; not persisted. *)
$aotPaths = <||>

TAOTEmit[name_String]  := (ensureInit[]; $aotEmitFn[TDefName[name], name])
TAOTEmit[name_Integer] := (ensureInit[]; $aotEmitFn[name, "def" <> ToString[name]])

TAOTCompile[name_String] := (
    ensureInit[];
    Module[{path = $aotCompileFn[TDefName[name], name]},
        If[ StringLength[path] == 0,
            $Failed,
            $aotPaths[name] = path;
            path
        ]
    ]
)
TAOTCompile[name_Integer] := (
    ensureInit[];
    Module[{nm = "def" <> ToString[name], path},
        path = $aotCompileFn[name, nm];
        If[ StringLength[path] == 0,
            $Failed,
            $aotPaths[nm] = path;
            path
        ]
    ]
)

TAOTPath[name_String] := Lookup[$aotPaths, name, Missing["NotCompiled"]]

TAOTRun[name_String, input_TTerm] := (
    ensureInit[];
    Module[{path = TAOTPath[name], in, raw},
        If[ MissingQ[path], Return[$Failed]];
        in = ttermRaw[input];
        raw = $aotRunFn[path, name, in];
        TTerm[raw]
    ]
)

(* Convenience: also accept raw Integer input (skips the TTerm
   wrap).  Useful for NUM-keyed dispatches. *)
TAOTRun[name_String, input_Integer] := TAOTRun[name, TNum[input]]

(* Multi-arg form: TAOTRun[name, {arg0, arg1, ...}].  Up to 4 args
   (matches AOT_MAX_ARGS).  Each arg can be a TTerm or a raw
   Integer (wrapped as TNum).  Trailing slots default to 0.
   Unlocks 2/3-arg defs (build, ack, gab_tak, ...). *)
toRawArg[t_TTerm]   := ttermRaw[t]
toRawArg[i_Integer] := ttermRaw[TNum[i]]
toRawArg[_]         := 0

TAOTRun[name_String, inputs_List] := (
    ensureInit[];
    Module[{path = TAOTPath[name], raws, raw},
        If[ MissingQ[path], Return[$Failed]];
        If[ Length[inputs] > 4,
            Message[TAOTRun::nargs, Length[inputs]];
            Return[$Failed]];
        raws = PadRight[toRawArg /@ inputs, 4, 0];
        raw = $aotRun4Fn[path, name, raws[[1]], raws[[2]], raws[[3]], raws[[4]]];
        TTerm[raw]
    ]
)
TAOTRun::nargs = "TAOTRun supports up to 4 args (got `1`).";
TAOTRun::method = "Method `1` not supported in this build.";

(* === Method dispatcher ==============================================
   When the call carries an explicit `Method -> spec` rule, route to
   the right backend.  Without the Method rule, falls through to the
   existing TAOTRun[name, input] / [name, inputs_List] overloads
   above (the default CPU/dlopen path).

   Method spec shapes:
     "Metal"                          -- GPU path: emit MSL,
                                          xcrun metallib, dispatch.
     "CPU"                            -- single-thread native (current
                                          dlopen'd C path).
     {"CPU", "NumThreads" -> n}       -- worker-pool parallel CPU.
*)

$aotMetalRun4Fn := $aotMetalRun4Fn = load[
    "thvm_wl_aot_metal_run4",
    {Integer, "UTF8String", Integer, Integer, Integer, Integer},
    Integer];

(* Variable-arity dispatch: defs with >4 args run via Method ->
   "Metal".  Args are packed into an Integer rank-1 MTensor and the
   kernel binds args[0..n-1] directly. *)
$aotMetalRunNFn := $aotMetalRunNFn = load[
    "thvm_wl_aot_metal_run_n",
    {Integer, "UTF8String", {Integer, 1}},
    Integer];

$aotMetalBatchOp2Fn := $aotMetalBatchOp2Fn = load[
    "thvm_wl_aot_metal_op2_fold_batch",
    {{Integer, 1}}, {Integer, 1}];

(* Parallel cnf+collapse on a BOOK_HEAP-rooted SUP-tree, dispatched
   via the static aot_ic_collapse PSO with grid = 2^depth.
   Caller gives the kernel-1 result Term + an estimated SUP-tree
   depth (max 30); each thread walks one leaf path and drives it to
   WHNF on-thread.  Returns a list of leaf TTerms (length 2^depth);
   ERA-pruned paths come back as TAG_ERA terms which the caller can
   filter. *)
$aotIcCollapseFn := $aotIcCollapseFn = load[
    "thvm_wl_aot_ic_collapse",
    {Integer, Integer}, {Integer, 1}];

TAOTIcCollapse[t_TTerm, depth_Integer] := Module[{raws},
    ensureInit[];
    raws = $aotIcCollapseFn[ttermRaw[t], depth];
    TTerm /@ raws
]

(* Bitmask CNF eval.  cnf = list of clauses, each clause a
   list of signed Integer literals (positive = +var_index, negative
   = -var_index; 1-based var indices).  Encodes to two parallel
   Integer arrays of clause-bitmasks (positive vars / negative vars
   per clause), dispatches the aot_cnf_bitmask kernel with grid =
   2^nVars, returns the per-leaf 0/1 result vector. *)
$aotCnfBitmaskFn := $aotCnfBitmaskFn = load[
    "thvm_wl_aot_cnf_bitmask",
    {{Integer, 1}, {Integer, 1}, Integer}, {Integer, 1}];

cnfToBitmasks[cnf_List, nVars_Integer] := Module[{posMasks, negMasks},
    posMasks = Table[
        BitOr @@ Append[Cases[clause, lit_ /; lit > 0 :> 2^(lit - 1)], 0],
        {clause, cnf}];
    negMasks = Table[
        BitOr @@ Append[Cases[clause, lit_ /; lit < 0 :> 2^(-lit - 1)], 0],
        {clause, cnf}];
    {posMasks, negMasks}
]

(* Survey Propagation.  Encodes CNF as flat literal list +
   clause-boundary array, dispatches the iteration loop on Metal,
   returns per-edge eta values. *)
$aotSpRunFn := $aotSpRunFn = load[
    "thvm_wl_aot_sp_run",
    {{Integer, 1}, {Integer, 1}, Integer, Integer, Real, Real},
    {Real, 1}];

Options[TAOTSurveyPropagate] = {
    "MaxIters" -> 100,
    "Damping" -> 0.5,
    "Threshold" -> 0.001
}
$aotSpSolveFn := $aotSpSolveFn = load[
    "thvm_wl_aot_sp_solve",
    {{Integer, 1}, {Integer, 1}, Integer, Integer, Real, Real},
    {Integer, 1}];

Options[TAOTSpSolve] = {
    "MaxIters" -> 100,
    "Damping" -> 0.5,
    "Threshold" -> 0.001
}
TAOTSpSolve[cnf_List, nVars_Integer,
    opts : OptionsPattern[]] := Module[{flat, bounds, maxIters, damping, threshold, raw, status, assignment, statusStr},
    ensureInit[];
    flat = Flatten[cnf];
    bounds = Prepend[Accumulate[Length /@ cnf], 0];
    maxIters = OptionValue["MaxIters"];
    damping = OptionValue["Damping"];
    threshold = OptionValue["Threshold"];
    raw = $aotSpSolveFn[
        Developer`ToPackedArray[flat, Integer],
        Developer`ToPackedArray[bounds, Integer],
        nVars, maxIters, N[damping], N[threshold]];
    status = First[raw];
    assignment = Rest[raw];
    statusStr = Switch[status,
        0,  "SAT",
        -1, "UNSAT",
        1,  "GAVE_UP",
        _,  "ERROR"];
    {statusStr, assignment}
]

TAOTSurveyPropagate[cnf_List, nVars_Integer,
    opts : OptionsPattern[]] := Module[{flat, bounds, maxIters, damping, threshold},
    ensureInit[];
    flat = Flatten[cnf];
    bounds = Prepend[Accumulate[Length /@ cnf], 0];
    maxIters = OptionValue["MaxIters"];
    damping = OptionValue["Damping"];
    threshold = OptionValue["Threshold"];
    $aotSpRunFn[
        Developer`ToPackedArray[flat, Integer],
        Developer`ToPackedArray[bounds, Integer],
        nVars, maxIters, N[damping], N[threshold]]
]

TAOTSatBitmask[cnf_List, nVars_Integer] := Module[{pos, neg},
    ensureInit[];
    {pos, neg} = cnfToBitmasks[cnf, nVars];
    $aotCnfBitmaskFn[
        Developer`ToPackedArray[pos, Integer],
        Developer`ToPackedArray[neg, Integer],
        nVars]
]

(* WL surface for the batch dispatcher.  Caller supplies a list of
   book_heap locs (Integers), each pointing at an OP2(NUM,NUM) cell.
   Returns a list of folded NUM TTerms. *)
TAOTBatchOp2Fold[rootLocs_List] := Module[{raws},
    ensureInit[];
    raws = $aotMetalBatchOp2Fn[
        Developer`ToPackedArray[rootLocs, Integer]];
    TTerm /@ raws
]

aotMetalRunImpl[name_String, args_List] := Module[{raws},
    raws = toRawArg /@ args;
    TTerm[$aotMetalRunNFn[TDefName[name], name, raws]]
]
aotMetalRunImpl[name_String, input_TTerm]   := aotMetalRunImpl[name, {input}]
aotMetalRunImpl[name_String, input_Integer] := aotMetalRunImpl[name, {TNum[input]}]

(* Generic per-def runner via the static aot_ic_def_run PSO.  No
   per-def MSL emit / xcrun roundtrip -- one PSO across all defs.
   Used when Method spec is {"Metal", "Generic" -> True} or as the
   fallback when the per-def emit would be too large to compile (the
   threshold is set at the WL surface for now). *)
$aotMetalIcDefRunFn := $aotMetalIcDefRunFn = load[
    "thvm_wl_aot_metal_ic_def_run",
    {Integer, {Integer, 1}}, Integer];

aotMetalIcDefRunImpl[name_String, args_List] := Module[{raws},
    raws = toRawArg /@ args;
    TTerm[
        $aotMetalIcDefRunFn[TDefName[name],
            Developer`ToPackedArray[raws, Integer]]]
]
aotMetalIcDefRunImpl[name_String, input_TTerm]   := aotMetalIcDefRunImpl[name, {input}]
aotMetalIcDefRunImpl[name_String, input_Integer] := aotMetalIcDefRunImpl[name, {TNum[input]}]

(* Method spec parses two ways: a bare String "Metal"/"CPU" -> head
   only (no opts), or a List {head, opt -> val, ...} -> head + opts
   tail.  Anything else falls through to the failure message. *)
methodHead[spec_] := Replace[spec,
    {_String                -> spec,
     {h_String, ___}        :> h,
     _                      -> None}]
methodOpts[spec_] := Replace[spec,
    {{_String, o___}        :> {o},
     _                      -> {}}]

TAOTRun[name_String, args_, Method -> spec_] := Module[{head, opts},
    ensureInit[];
    head = methodHead[spec];
    opts = methodOpts[spec];
    Switch[head,
        "Metal",
            If[ TrueQ @ Lookup[opts, "Generic", False],
                aotMetalIcDefRunImpl[name, args],
                aotMetalRunImpl[name, args]],
        "CPU", aotCpuRunImpl[name, args, spec],
        _, Message[TAOTRun::method, spec]; $Failed
    ]
]

(* Method -> "CPU" / {"CPU", "NumThreads" -> n}: auto-compile via
   TAOTCompile if not yet compiled, then dispatch.

   Bare "CPU" or n=1: serial dispatch via existing TAOTRun[name, args].

   {"CPU", "NumThreads" -> n} with n>1: parallel dispatch through
   thvm_wl_aot_run4_pooled, which dlopens the dylib's
   aot_program_<name>_run_pooled entry and calls aot_run_parallel
   (the work-stealing pool). *)
aotCpuRunImpl[name_String, args_, spec_] := Module[
    {path, raws, slots, threads},
    path = TAOTPath[name];
    If[ MissingQ[path], path = TAOTCompile[name]];
    If[ path === $Failed || MissingQ[path],
        Message[TAOTRun::method, spec]; Return[$Failed]];
    threads = Replace[spec,
        { _String                            -> 1,
          { _String, OrderlessPatternSequence["NumThreads" -> n_Integer, ___] } :> n,
          _                                  -> 1
        }];
    If[ threads <= 1,
        TAOTRun[name, args],
        raws = toRawArg /@ args;
        slots = PadRight[raws, 4, 0];
        TTerm[$aotRun4PooledFn[path, name, threads,
            slots[[1]], slots[[2]], slots[[3]], slots[[4]]]]
    ]
]

End[];
EndPackage[];
