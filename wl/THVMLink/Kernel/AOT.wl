(* ::Package:: *)
(* AOT.wl -- WL surface for ahead-of-time compilation.

   The Bend2-style fork-emitting AOT lives in src/aot/.  Phase 1 built
   the runtime (Task / Result / cont / worker), Phase 2 built the
   emitter that translates a TDef'd body into compilable C source
   (par_<name>_entry + par_<name>_cont_K + dispatch table).  Phase 4
   (this file) exposes the emitter to WL.

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

TAOTEmit::usage =
  "TAOTEmit[name] returns the C source string that thvm_aot_emit_program \
produces for the def registered under `name`.  Inspect, save, or wrap \
manually for further compile.  TAOTCompile + TAOTRun automate those \
steps; this is the lower-level surface.";

TAOTCompile::usage =
  "TAOTCompile[name] emits the def under `name`, wraps it with the thvm \
runtime + an aot_program_<name>_run entry, invokes clang to produce a \
dylib, and stashes the resulting path so TAOTRun[name, input] can dlopen \
it.  Returns the dylib path string.  Cache-by-content: same source -> \
same path -> skips the clang call on subsequent invocations.";

TAOTRun::usage =
  "TAOTRun[name, input_TTerm] dlopens the AOT'd dylib for `name` (must \
have been TAOTCompile'd first), invokes its run entry with `input` as the \
argument Term, and returns the result wrapped as a TTerm.  For programs \
that reduce to a NUM the result is a self-contained TTerm carrying the \
scalar value.  CTR-returning programs reference the dylib's heap and \
can't be decoded host-side without further marshalling -- use TAOTEmit \
+ a custom harness for those today.";

TAOTPath::usage =
  "TAOTPath[name] returns the dylib path stashed by TAOTCompile[name], \
or Missing[\"NotCompiled\"] if the def has never been TAOTCompile'd.";

TAOTIcCollapse::usage =
  "TAOTIcCollapse[term_TTerm, depth_Integer] dispatches the static \
aot_ic_collapse PSO with grid = 2^depth over a BOOK_HEAP-rooted \
SUP-tree at `term` (e.g., the result of TAOTRun[..., Method -> \"Metal\"] \
with THVM_AOT_METAL_KEEP_BOOK=1 set).  Each thread decodes its tid \
into a binary path through the SUP-tree, drives the final leaf to \
WHNF on-thread via per-thread IC interaction inlines, and writes the \
resulting Term to result[tid].  Returns a List of TTerm leaves of \
length 2^depth (filter ERA sentinels via TTermTag).  Iter Z+1.";

TAOTBatchOp2Fold::usage =
  "TAOTBatchOp2Fold[root_locs] dispatches the iter B-2 batch kernel \
(aot_eval_op2_fold_batch) over a list of book_heap locs, each pointing \
at an OP2(NUM,NUM) cell.  Returns a list of N folded NUM Terms (as \
TTerms).  Amortizes Metal kernel-launch overhead across N redexes -- \
useful when you have many independent OP2 folds queued up.  Pre-built \
the OP2 cells via TBookAlloc/TBookSet first; the kernel reads from \
book_heap directly.";

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
$aotPaths = <||>;

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
);
TAOTCompile[name_Integer] := (
  ensureInit[];
  Module[{nm = "def" <> ToString[name],
          path},
    path = $aotCompileFn[name, nm];
    If[ StringLength[path] == 0,
      $Failed,
      $aotPaths[nm] = path;
      path
    ]
  ]
);

TAOTPath[name_String] := Lookup[$aotPaths, name, Missing["NotCompiled"]]

TAOTRun[name_String, input_TTerm] := (
  ensureInit[];
  Module[{path = TAOTPath[name], in, raw},
    If[ MissingQ[path], Return[$Failed]];
    in  = ttermRaw[input];
    raw = $aotRunFn[path, name, in];
    TTerm[raw]
  ]
);

(* Convenience: also accept raw Integer input (skips the TTerm
   wrap).  Useful for NUM-keyed dispatches. *)
TAOTRun[name_String, input_Integer] := TAOTRun[name, TNum[input]];

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
    raw  = $aotRun4Fn[path, name, raws[[1]], raws[[2]], raws[[3]], raws[[4]]];
    TTerm[raw]
  ]
);
TAOTRun::nargs = "TAOTRun supports up to 4 args (got `1`).";
TAOTRun::method = "Method `1` not supported in this build.";

(* === Method dispatcher (Phase 7) ====================================
   When the call carries an explicit `Method -> spec` rule, route to
   the right backend.  Without the Method rule, falls through to the
   existing TAOTRun[name, input] / [name, inputs_List] overloads
   above (the default CPU/dlopen path).

   Method spec shapes:
     "Metal"                          -- Phase 7 GPU path: emit MSL,
                                          xcrun metallib, dispatch.
     "CPU"                            -- single-thread native (current
                                          dlopen'd C path).
     {"CPU", "NumThreads" -> n}       -- worker-pool parallel CPU
                                          (Phase 1 wnf_pool integration
                                          -- not yet wired here).
*)

$aotMetalRun4Fn := $aotMetalRun4Fn = load[
    "thvm_wl_aot_metal_run4",
    {Integer, "UTF8String", Integer, Integer, Integer, Integer},
    Integer];

(* iter Y: variable-arity dispatch.  Replaces the 4-slot bridge so
   defs with >4 args can run via Method -> "Metal".  Args are packed
   into an Integer rank-1 MTensor and the kernel binds args[0..n-1]
   directly. *)
$aotMetalRunNFn := $aotMetalRunNFn = load[
    "thvm_wl_aot_metal_run_n",
    {Integer, "UTF8String", {Integer, 1}},
    Integer];

$aotMetalBatchOp2Fn := $aotMetalBatchOp2Fn = load[
    "thvm_wl_aot_metal_op2_fold_batch",
    {{Integer, 1}}, {Integer, 1}];

(* iter Z+1: parallel cnf+collapse on a BOOK_HEAP-rooted SUP-tree,
   dispatched via the static aot_ic_collapse PSO with grid = 2^depth.
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
];

(* Phase 7 iter QQ: WL surface for the batch dispatcher.  Caller
   supplies a list of book_heap locs (Integers), each pointing at an
   OP2(NUM,NUM) cell.  Returns a list of folded NUM TTerms. *)
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

(* Iter Z+2 step 4: generic per-def runner via the static aot_ic_def_run
   PSO.  No per-def MSL emit / xcrun roundtrip -- one PSO across all
   defs.  Used when Method spec is {"Metal", "Generic" -> True} or as
   the iter Z fallback when the per-def emit would be too large to
   compile (the threshold is set at the WL surface for now). *)
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
    "CPU",   aotCpuRunImpl[name, args, spec],
    _,       Message[TAOTRun::method, spec]; $Failed
  ]
]

(* Method -> "CPU" / {"CPU", "NumThreads" -> n}: auto-compile via
   TAOTCompile if not yet compiled, then dispatch.

   Bare "CPU" or n=1: serial dispatch via existing TAOTRun[name, args].

   {"CPU", "NumThreads" -> n} with n>1: parallel dispatch through
   thvm_wl_aot_run4_pooled, which dlopens the dylib's
   aot_program_<name>_run_pooled entry and calls aot_run_parallel
   (Phase 1's work-stealing pool).  iter Q wired per-worker
   CURRENT_WNF_STATE in aot_worker_main so spawned pthreads no
   longer SEGV on wnf re-entry. *)
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
    raws  = toRawArg /@ args;
    slots = PadRight[raws, 4, 0];
    TTerm[$aotRun4PooledFn[path, name, threads,
                       slots[[1]], slots[[2]], slots[[3]], slots[[4]]]]
  ]
]

End[];
EndPackage[];
