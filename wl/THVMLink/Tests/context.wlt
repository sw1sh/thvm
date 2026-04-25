(* context.wlt -- multi-heap support via TContext.

   Reference design: TinyHVM/src/tinyhvm.h (struct TinyHVM).  Each
   TContext slot owns its own heap, book, defs, alo state, tensors,
   kernels, and backend default.  Slot 0 is the always-present
   default; slots 1..15 are user-allocated via TContextNew[].

   Tests cover: structural shape of the head, TInContext block scope
   (including exception unwind), independent heap evolution across
   contexts, TTerm tagging on non-default contexts, $labelCounter
   isolation, and TContextDestroy lifecycle. *)

(* === structural === *)

VerificationTest[
    Head[$TContext],
    TContext,
    TestID -> "context/$TContext-default-head"
]

VerificationTest[
    TInit[];
    TContextCurrent[],
    TContext[0],
    TestID -> "context/current-is-slot-0-by-default"
]

VerificationTest[
    TInit[];
    Module[{ctx = TContextNew[], head},
        head = Head[ctx];
        TContextDestroy[ctx];
        head
    ],
    TContext,
    TestID -> "context/new-returns-TContext-head"
]

VerificationTest[
    TInit[];
    Module[{ctx = TContextNew[], result},
        result = First[ctx] >= 1;
        TContextDestroy[ctx];
        result
    ],
    True,
    TestID -> "context/new-returns-non-default-slot"
]

(* === TInContext block scope === *)

VerificationTest[
    TInit[];
    Module[{ctx = TContextNew[], inside, after},
        inside = TInContext[ctx, TContextCurrent[]];
        after  = TContextCurrent[];
        TContextDestroy[ctx];
        {inside, after}
    ],
    {TContext[1], TContext[0]},
    TestID -> "context/in-context-restores-on-exit"
]

VerificationTest[
    (* Nested TInContext correctly unwinds to the outer ctx. *)
    TInit[];
    Module[{ctx1 = TContextNew[], ctx2, results},
        ctx2 = TContextNew[];
        results = TInContext[ctx1,
            {TContextCurrent[],
             TInContext[ctx2, TContextCurrent[]],
             TContextCurrent[]}
        ];
        TContextDestroy[ctx1];
        TContextDestroy[ctx2];
        results
    ],
    {TContext[1], TContext[2], TContext[1]},
    TestID -> "context/nested-in-context-unwinds"
]

VerificationTest[
    (* Exception during TInContext still restores the outer ctx. *)
    TInit[];
    Module[{ctx = TContextNew[], caught, after},
        caught = Catch[
            TInContext[ctx, Throw["x", "ctx-test"]],
            "ctx-test"
        ];
        after = TContextCurrent[];
        TContextDestroy[ctx];
        {caught, after}
    ],
    {"x", TContext[0]},
    TestID -> "context/in-context-restores-on-throw"
]

(* === independent heap evolution === *)

VerificationTest[
    TInit[];
    Module[{ctx = TContextNew[], pos0, pos1, posAfter},
        TInit[];
        TReset[];
        Module[{lam = TLam[x, x]}, lam];
        pos0 = THeapPos[];
        TInContext[ctx,
            TInit[];
            TReset[];
            Module[{lam2 = TLam[x, TApp[x, x]]}, lam2];
            pos1 = THeapPos[]
        ];
        posAfter = THeapPos[];
        TContextDestroy[ctx];
        {pos0 > 0, pos1 > pos0, posAfter === pos0}
    ],
    {True, True, True},
    TestID -> "context/heaps-independent"
]

(* === TTerm tagging === *)

VerificationTest[
    (* TTerm canonical form is always 2-arg; First[t] = ctx slot. *)
    TInit[];
    Module[{ctx = TContextNew[], t0, t1, slots},
        TReset[];
        t0 = TLam[x, x];
        TInContext[ctx, TInit[]; TReset[]; t1 = TLam[x, x]];
        slots = {Length[t0], Length[t1], First[t0], First[t1]};
        TContextDestroy[ctx];
        slots
    ],
    (* TTerm now has 3 elements: {ctxSlot, raw, externPinHandle}. *)
    {3, 3, 0, 1},
    TestID -> "context/term-tagged-with-slot"
]

VerificationTest[
    (* TTerm["ctx"] reports the slot tag for both arities. *)
    TInit[];
    Module[{ctx = TContextNew[], t0, t1, ctxs},
        TReset[];
        t0 = TLam[x, x];
        TInContext[ctx, TInit[]; TReset[]; t1 = TLam[x, x]];
        ctxs = {t0["ctx"], t1["ctx"]};
        TContextDestroy[ctx];
        ctxs
    ],
    {0, 1},
    TestID -> "context/term-ctx-accessor"
]

VerificationTest[
    (* Auto-switch: a TWnf on a tagged term routes to its home ctx
       even when called from outside any TInContext block. *)
    TInit[];
    Module[{ctx = TContextNew[], t1, after},
        TInContext[ctx, TInit[]; TReset[]; t1 = TLam[x, x]];
        after = TWnf[t1];
        TContextDestroy[ctx];
        {after["ctx"], TTagName[TTermTag[after]]}
    ],
    {1, "LAM"},
    TestID -> "context/auto-switch-on-twnf"
]

(* === per-context $labelCounter === *)

VerificationTest[
    (* TFreshLabel increments per-context independently. *)
    TInit[];
    Module[{ctx = TContextNew[], a, b, c, d},
        TReset[];
        a = TFreshLabel[];   (* slot 0: -> 2 *)
        b = TFreshLabel[];   (* slot 0: -> 3 *)
        TInContext[ctx,
            TInit[];
            TReset[];
            c = TFreshLabel[];   (* slot 1: -> 2 *)
        ];
        d = TFreshLabel[];   (* slot 0 again: -> 4 *)
        TContextDestroy[ctx];
        {a, b, c, d}
    ],
    {1, 2, 1, 3},
    TestID -> "context/labelcounter-per-context"
]

(* === lifecycle === *)

VerificationTest[
    (* Destroy returns the slot id and removes it from the table. *)
    TInit[];
    Module[{ctx = TContextNew[], slot, destroyed},
        slot = First[ctx];
        destroyed = TContextDestroy[ctx];
        {slot, destroyed, TContextCurrent[]}
    ],
    {1, 1, TContext[0]},
    TestID -> "context/destroy-returns-slot"
]

VerificationTest[
    (* Slot 0 cannot be destroyed via TContextDestroy. *)
    TInit[];
    TContextDestroy[TContext[0]];
    TContextCurrent[],
    TContext[0],
    TestID -> "context/destroy-skips-slot-0"
]

VerificationTest[
    (* Multiple contexts reuse freed slots after destroy. *)
    TInit[];
    Module[{c1, c2, c3, slot1, slot2, slot3},
        c1 = TContextNew[];   slot1 = First[c1];
        c2 = TContextNew[];   slot2 = First[c2];
        TContextDestroy[c1];
        c3 = TContextNew[];   slot3 = First[c3];   (* should reuse slot1 *)
        TContextDestroy[c2];
        TContextDestroy[c3];
        {slot1, slot2, slot3}
    ],
    {1, 2, 1},
    TestID -> "context/destroyed-slot-reused"
]
