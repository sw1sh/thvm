(* ::Package:: *)
(* Pri.wl - TAG_PRI surface: build PRI redexes + register WL callbacks
   that fire from inside wnf-driven recursive terms.

   ONE primitive (THVM_PRIM_PRI in src/term/prims_core.c, arity 3):

       APP(APP(APP(PRI(PRI), slot_NUM), val), cont)

   Pipeline at fire time:
       1. wnf(val): forces side effects (kernel chains, ASSIGN, ...)
       2. if slot != 0, append (slot, snapshotted-value) to a C-side
          queue.  WL drains via TPriDrain[] and dispatches to the
          per-slot callback.
       3. return cont: the surrounding redex rewrites to it.

   slot=0 is the PURE-SEQUENCER mode: just force val, no callback.
   slot>0 dispatches a registered WL function (logging, metrics,
   custom instrumentation).

   ASYNC by design.  Three documented synchronous re-entry paths all
   have unfixable constraints in single-threaded mode:
     - callLibraryCallbackFunction works re-entrantly but ONLY accepts
       CompiledFunction (rejects Function, FunctionCompile, anything
       with Print/$var/patterns, which rules out every typical
       loss-logging callback).
     - WSTP EvaluatePacket from inside a LibraryFunction call deadlocks
       (kernel is blocked waiting for our return; can't process
       incoming packets).
     - FunctionCompile output (CompiledCodeFunction) is rejected by
       Connect; LibraryFunction same.
   So we queue + drain.  Snapshotting (pri_snapshot_value in C)
   ensures recursive-loop iterations each see THEIR value rather than
   the buffer's last-written state.

   PUBLIC SURFACE
     TPri[fn,   val, cont]      inline WL callback (auto-slot)
     TPri[slot, val, cont]      low-level form with explicit slot int
     TPriForce[val, cont]       sugar for TPri[0, val, cont] (no cb)
     TPriRegister[slot, fn]     explicit slot -> fn binding
     TPriDrain[]                fire all queued callbacks; clear
     TPriCallbacks[]            inspect the slot -> fn registration *)

BeginPackage["WolframInstitute`THVMLink`"];

GeneralUtilities`SetUsage[TPri, "TPri[fn$, val$, cont$] builds a PRI redex that, when wnf reduces it, forces val$ (firing kernel chains and ASSIGN side effects), enqueues the snapshotted value for the WL callback fn$ unless fn$ is None, then returns cont$.
fn$ auto-registers under a fresh slot on first use and reuses that slot on later calls.
TPri[slot$, val$, cont$] is the low-level form with an explicit integer slot; slot 0 is the pure-sequencer mode (no callback).
Callbacks fire when the host calls TPriDrain[]; their return values are ignored."];

GeneralUtilities`SetUsage[TPriForce, "TPriForce[val$, cont$] is sugar for TPri[0, val$, cont$]: a pure sequencer with no WL callback that forces val$ via wnf as a side effect and returns cont$."];

GeneralUtilities`SetUsage[TPriRegister, "TPriRegister[slot$, fn$] associates WL function fn$ with PRI slot slot$, returning slot$.
Use it for explicit control over slot ids (e.g. cross-session stable slots); TPri[fn$, $$] auto-registers transparently in the common case.
fn$ = None clears the slot."];

GeneralUtilities`SetUsage[TPriDrain, "TPriDrain[] dequeues every (slot, value) pair recorded by TPri firings since the last drain, dispatches each to its registered WL callback, and returns the number of callbacks fired.
Slots with no registered callback are skipped silently."];

GeneralUtilities`SetUsage[TPriCallbacks, "TPriCallbacks[] returns the current slot-to-fn registration as a read-only Association."];

Begin["`Private`"];

(* === C-side prim id (must match THVM_PRIM_PRI in src/thvm.h) === *)
$ThvmPrimPri = 16

(* === bridge function bindings === *)
$termNewPriFn  := $termNewPriFn  = load["thvm_wl_term_new_pri",
    {Integer}, Integer]
$priDrainFn    := $priDrainFn    = load["thvm_wl_pri_drain",
    {}, {Integer, 1}]
$priBindSlotFn   := $priBindSlotFn   = load["thvm_wl_bind_pri_slot",
    {Integer, Integer}, Integer]
$priUnbindSlotFn := $priUnbindSlotFn = load["thvm_wl_unbind_pri_slot",
    {Integer}, Integer]
$priLastCbIdFn   := $priLastCbIdFn   = load["thvm_wl_pri_last_cb_id",
    {}, Integer]

(* === slot -> fn registration ===
   Per-session.  Three paths in priority order:
     (A) FOREIGN CALLBACK (preferred, arbitrary WL): wrap fn in
         CreateForeignCallback to get a libffi closure; pass the
         closure pointer to thvm_pri_bind_foreign via ForeignFunction-
         Load.  prim_pri calls the closure pointer directly when
         firing; libffi handles the kernel re-entry.  Works for
         Function, Symbol, anything.
     (B) COMPILED CALLBACK: if fn is a CompiledFunction, use the
         LibraryLink callback registry (callLibraryCallbackFunction).
         Slightly cheaper than (A) but body must be numerical.
     (C) QUEUED: prim_pri enqueues; TPriDrain[] dispatches.  Used
         when neither (A) nor (B) are wired (e.g. ForeignFunction-
         Interface paclet not loaded).

   $priCallbacks holds the WL fn (used by queued path AND keeps the
   ForeignCallback object alive across GC).  $priCbId holds the
   LibraryLink id for path (B).  $priForeignCb holds the closure
   object for path (A).  Registration replaces; TPriRegister[slot,
   None] clears all three. *)
$priCallbacks = <||>
$priFnSlot    = <||>     (* fn -> slot (deduplicated) *)
$priCbId      = <||>     (* slot -> LibraryLink callback id *)
$priForeignCb = <||>     (* slot -> ForeignCallback (keep alive) *)
$priNextSlot  = 1        (* slot 0 reserved for pure-sequencer *)

(* Bridge function loaders for paths (A) and (B).  Lazy: Needs[]
   the FFI paclet on first use; the load itself is also memoized
   via Set inside SetDelayed.  Quiet suppresses noisy benign warnings
   (SystemSymbolQ shadowing from CompileUtilities` vs GeneralUtilities`)
   that would otherwise trip Check; we Quiet the messages but check
   for $Failed return values explicitly. *)
loadForeignBridge[name_String, type_] := Module[{r},
    (* Quiet the benign SystemSymbolQ::shdw warning that Needs raises
       when the FFI paclet's CompileUtilities` shadows GeneralUtilities`.
       Also Quiet ForeignFunctionLoad's general Off-message channel. *)
    Quiet @ Needs["ForeignFunctionInterface`"];
    (* Off the post-Needs context-shadow message that fires AFTER Quiet
       returns: WL stages it during evaluation but emits later when
       the message channel flushes. *)
    Off[CompileUtilities`Symbols`SystemSymbolQ::shdw];
    r = Quiet @ ForeignFunctionLoad[$lib, name, type];
    If[ !MatchQ[r, _ForeignFunction], $Failed, r]
]

$priBindForeignFn   := $priBindForeignFn   = loadForeignBridge[
    "thvm_pri_bind_foreign",   {"CInt", "OpaqueRawPointer"} -> "Void"]
$priUnbindForeignFn := $priUnbindForeignFn = loadForeignBridge[
    "thvm_pri_unbind_foreign", {"CInt"} -> "Void"]

(* Wrap a user fn so it returns the right shape for the int64 return
   FFI signature: 0 means "no override, use cont"; any other Integer
   becomes the new redex result.  Plain Functions that return TTerm
   get unwrapped to their raw value; Null/Nothing return 0.
   Already-wrapped CFs (returning Integer directly) pass through. *)
wrapForeignFn[fn_] := v |-> With[{r = fn[TTerm[v]]},
    Which[
        IntegerQ[r],          r,           (* explicit override *)
        MatchQ[r, _TTerm],    ttermRaw[r], (* TTerm wrapper *)
        True,                 0            (* trace mode *)
    ]
]

(* Path (A): wrap fn in libffi closure, hand the pointer to C.  Accepts
   either a plain WL function (auto-wrapped via wrapForeignFn so the
   int64 return path works) or an already-built ForeignCallback object
   (used as-is, signature must be {"Integer64"} -> "Integer64"). *)
tryConnectForeign[slot_Integer, fcb_ /; MatchQ[fcb, _ManagedObject]] := (
    If[ $priBindForeignFn === $Failed, Return[$Failed]];
    $priBindForeignFn[slot, fcb];
    $priForeignCb[slot] = fcb;
    fcb
)
tryConnectForeign[slot_Integer, fn_] := Module[{cb},
    If[ $priBindForeignFn === $Failed, Return[$Failed]];
    cb = Quiet @ Check[
        CreateForeignCallback[wrapForeignFn[fn],
            {"Integer64"} -> "Integer64"],
        $Failed];
    If[ cb === $Failed, Return[$Failed]];
    $priBindForeignFn[slot, cb];
    $priForeignCb[slot] = cb;     (* must outlive the binding *)
    cb
]

(* Path (B): connect a CompiledFunction. *)
tryConnectCompiled[slot_Integer, fn_CompiledFunction] := Module[{ok, id},
    ok = Quiet @ Check[ConnectLibraryCallbackFunction["thvm_pri_cb", fn],
                       $Failed];
    If[ ok =!= True, Return[$Failed]];
    id = $priLastCbIdFn[];
    If[ ! IntegerQ[id] || id <= 0, Return[$Failed]];
    $priBindSlotFn[slot, id];
    $priCbId[slot] = id;
    id
]
tryConnectCompiled[_, _] := $Failed

clearSync[slot_Integer] := (
    If[ KeyExistsQ[$priCbId, slot],
        $priUnbindSlotFn[slot];
        $priCbId = KeyDrop[$priCbId, slot]
    ];
    If[ KeyExistsQ[$priForeignCb, slot],
        If[ $priUnbindForeignFn =!= $Failed, $priUnbindForeignFn[slot]];
        $priForeignCb = KeyDrop[$priForeignCb, slot]
    ];
)

TPriRegister[slot_Integer, fn_] := Module[{result},
    clearSync[slot];
    If[ fn === None,
        $priCallbacks = KeyDrop[$priCallbacks, slot];
        $priFnSlot    = Select[$priFnSlot, # =!= slot &],
        $priCallbacks[slot] = fn;
        $priFnSlot[fn]      = slot;
        (* Try foreign first (arbitrary WL), fall back to compiled
           (numerical only), else just queue. *)
        result = tryConnectForeign[slot, fn];
        If[ result === $Failed, tryConnectCompiled[slot, fn]]
    ];
    slot
]

TPriCallbacks[] := $priCallbacks

(* Auto-allocate a slot for `fn` (or reuse if already registered).
   Identity-keyed: same fn expression -> same slot, no leak across
   repeated TPri[fn, ...] calls. *)
ensureSlot[None] := 0
ensureSlot[fn_] := If[ KeyExistsQ[$priFnSlot, fn],
    $priFnSlot[fn],
    With[{slot = $priNextSlot},
        $priNextSlot += 1;
        TPriRegister[slot, fn];
        slot
    ]
]

(* === PRI term constructors ===
   Both forms reduce to APP(APP(APP(PRI(PRI_id), slot_NUM), val), cont).
   Each APP step accumulates one arg into the PRI's heap layout
   (interact_app_pri does the bookkeeping); the redex fires once the
   third arg is supplied. *)
TPri[slot_Integer, val_, cont_] := (
    ensureInit[];
    TApp[
        TApp[
            TApp[TTerm[$termNewPriFn[$ThvmPrimPri]], TNum[slot]],
            val
        ],
        cont
    ]
)

TPri[fn_ /; ! IntegerQ[fn], val_, cont_] :=
    TPri[ensureSlot[fn], val, cont]

TPriForce[val_, cont_] := TPri[0, val, cont]

(* === drain: poll the C queue, fire callbacks, clear === *)
TPriDrain[] := Module[{raw, n, fired = 0},
    raw = Normal @ $priDrainFn[];
    n   = Length[raw] / 2;
    Do[
        With[{
            slot  = raw[[2 i - 1]],
            valTm = TTerm[raw[[2 i]]]
        },
            If[ KeyExistsQ[$priCallbacks, slot],
                $priCallbacks[slot][valTm];
                fired += 1
            ]
        ],
        {i, n}
    ];
    fired
]

End[];
EndPackage[];
