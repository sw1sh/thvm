(* ::Package:: *)
(* Context.wl - multi-heap support for THVMLink.
   Reference design: TinyHVM/src/tinyhvm.h:1102-1169 (struct TinyHVM
   + per-tensor backend + ctx_default_backend).

   Three roles for the WL `TContext` symbol:

     1. **Head wrapper.**  `TContext[id_Integer]` is an opaque handle
        to a C-side context slot.  Slot 0 is the default; slots
        1..15 are user-allocated.

     2. **Default-context binding.**  `$TContext` is the implicit
        first arg for every context-polymorphic constructor.  The
        initial value is `TContext[0]`.  Users can rebind it via
        `Block[{$TContext = ctx}, ...]` or the convenience
        `TInContext[ctx, body]`.

     3. **Auto-switch tag on TTerm.**  Every `TTerm[ctxId, raw]`
        carries the slot id of the context it was allocated in;
        operations on the term auto-switch the C-side current
        context for the duration of the call.  This file provides
        the underlying primitives; the 2-arg TTerm form lives
        elsewhere.

   Loaded from THVMLink.wl inside Begin["`Private`"]; sees the
   private bridge symbols (load, ttermRaw, ...) without
   qualification. *)

BeginPackage["THVMLink`"];

GeneralUtilities`SetUsage[TContext, "TContext[id$] is an opaque handle to a runtime context (heap, book, defs, alo state, tensors, kernels, backends).
Slot 0 is the default singleton; slots 1..15 are user-allocated via TContextNew[]."];
GeneralUtilities`SetUsage[$TContext, "$TContext is the default context for the context-polymorphic API; initial value TContext[0].
Rebind via Block[{$TContext = ctx$}, $$] or TInContext[ctx$, $$]."];
GeneralUtilities`SetUsage[TContextNew, "TContextNew[] allocates a fresh context with the default device and returns TContext[id$].
TContextNew[device$] uses the named default device (\"cpu\", \"metal\", \"cuda\").
Returns a Failure if the context table is full (CONTEXTS_CAP=16)."];
GeneralUtilities`SetUsage[TContextDestroy, "TContextDestroy[TContext[id$]] frees the C-side state for that context and returns the destroyed slot id.
Slot 0 (default) is preserved and returns 0 as a no-op; use TFree[] to tear it down."];
GeneralUtilities`SetUsage[TContextCurrent, "TContextCurrent[] returns the C-side current context as TContext[id$]."];
GeneralUtilities`SetUsage[TContextList, "TContextList[] returns all allocated contexts as a list of TContext[id$] (slot 0 first)."];
GeneralUtilities`SetUsage[TInContext, "TInContext[ctx$, body$] evaluates body$ with $TContext rebound to ctx$ and the C-side current context switched to ctx$ for the duration.
HoldRest; restores via Internal`WithLocalSettings so a Throw still unwinds cleanly."];

Begin["`Private`"];

(* === C bridge loaders === *)

$contextCreateFn  := $contextCreateFn  = load["thvm_wl_context_create",  {Integer},  Integer]
$contextSelectFn  := $contextSelectFn  = load["thvm_wl_context_select",  {Integer},  Integer]
$contextCurrentFn := $contextCurrentFn = load["thvm_wl_context_current", {},         Integer]
$contextDestroyFn := $contextDestroyFn = load["thvm_wl_context_destroy", {Integer},  Integer]
$contextCountFn   := $contextCountFn   = load["thvm_wl_context_count",   {},         Integer]

(* Device-name -> integer code shared with the C bridge.
   -1 = "use default" (NULL on the C side). *)
deviceCode["cpu"]     := 0
deviceCode["metal"]   := 1
deviceCode["cuda"]    := 2
deviceCode[None]      := -1
deviceCode[Automatic] := -1
deviceCode[s_]        := -1

(* Inverse: device code (the THVM_DEV_ enum) -> name; -1/unknown -> None
   (the default device). *)
deviceName[0]  := "cpu"
deviceName[1]  := "metal"
deviceName[2]  := "cuda"
deviceName[_]  := None

(* === Default context binding === *)

$TContext = TContext[0]

(* === Constructors / destructors === *)

TContextNew[]                  := TContextNew[Automatic]
TContextNew[device_]           := With[{slot = $contextCreateFn[deviceCode[device]]},
    If[ slot === 0,
        Failure["TContextOverflow",
            <| "Message" -> "TContextNew: context table full (CONTEXTS_CAP=16)" |>],
        $labelCounters[slot] = 1;
        $initializedContexts[slot] = True;
        TContext[slot]
    ]
]

(* Slot 0 (default) is preserved -- tearing it down here would delete the
   default context's WL-side counters; use TFree[] for that.  Return 0 as
   the documented no-op value. *)
TContextDestroy[TContext[0]] := 0
TContextDestroy[TContext[slot_Integer]] := (
    KeyDropFrom[$labelCounters, slot];
    KeyDropFrom[$initializedContexts, slot];
    $contextDestroyFn[slot];
    slot
)

TContextCurrent[]              := TContext[$contextCurrentFn[]]

(* Walks slots 0..15 collecting live ones.  Slot 0 is always live
   (DEFAULT_CTX_STORAGE).  For slots 1..15 we exploit the C-side
   select-only-if-allocated semantics: $contextSelectFn returns the
   previous slot regardless, but CURRENT_CTX only changes for live
   slots, so a subsequent currentFn[] returning `slot` proves it was
   allocated. *)
TContextList[] := Module[{prev = $contextCurrentFn[], live},
    live = Table[
        $contextSelectFn[slot];
        If[ $contextCurrentFn[] === slot,
            TContext[slot],
            Nothing
        ],
        {slot, 1, 15}
    ];
    $contextSelectFn[prev];
    Prepend[live, TContext[0]]
]

(* === withCtx + TInContext === *)

(* Switches the C-side current context for the duration of `expr`
   and restores on unwind (including via Throw, Abort, etc.).
   HoldRest keeps `expr` from evaluating until after the switch. *)
SetAttributes[withCtx, HoldRest]
withCtx[TContext[slot_Integer], expr_] := Module[{prev},
    prev = $contextSelectFn[slot];
    Internal`WithLocalSettings[
        Null,
        expr,
        $contextSelectFn[prev]
    ]
]
withCtx[_, expr_] := expr   (* fallback: no-op if ctx is malformed *)

SetAttributes[TInContext, HoldRest]
TInContext[ctx_TContext, expr_] := Block[{$TContext = ctx},
    withCtx[ctx, expr]
]

(* Auto-switch helper: run `expr` under the C-side context that owns
   `t`.  Lives here (not in THVMLink.wl) because TContext must be a
   public `THVMLink`` symbol when the rule is parsed; otherwise
   it'd resolve to `THVMLink`Private`TContext` (shadow) and the
   catch-all `withCtx[_, expr_] := expr` would silently absorb every
   call, defeating auto-switch. *)
SetAttributes[withTermCtx, HoldRest]
withTermCtx[t_, expr_] := With[{c = ttermCtx[t]},
    If[ c === $contextCurrentFn[],
        expr,
        withCtx[TContext[c], expr]
    ]
]

End[];
EndPackage[];
