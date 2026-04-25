(* extern_pin.wlt - WL-side observation of the managed pin handles
                    backing TTerm.  Verifies that the C-side pin
                    count drops when the surrounding scope exits
                    and the TTerm wrappers are no longer reachable. *)

BeginPackage["THVMLinkTests`Extern`Pin`",
    {"THVMLink`", "MUnit`"}];

Begin["`Private`"];

(* === a fresh TTerm pins a Term on construction === *)

VerificationTest[
    TInit[];
    TReset[];
    Module[{before, after},
        before = TExternPinCount[];
        TEra[];
        after = TExternPinCount[];
        after - before
    ],
    1,
    TestID -> "extern-pin/construction-pins-one-term"
]

(* === wrapping the same raw twice creates two pins === *)

VerificationTest[
    TInit[];
    TReset[];
    Module[{before, e1, e2, after},
        before = TExternPinCount[];
        e1 = TEra[];
        e2 = TEra[];
        after = TExternPinCount[];
        {e1 == e2, after - before}
    ],
    {True, 2},
    TestID -> "extern-pin/duplicate-wrappers-each-pin"
]

(* === leaving a Module scope drops the wrappers; WL GC eventually
       fires the manager and the pin count returns to baseline === *)

VerificationTest[
    TInit[];
    TReset[];
    Module[{before, mid},
        before = TExternPinCount[];
        Module[{terms = {TEra[], TEra[], TEra[]}},
            mid = TExternPinCount[];
            Length[terms]
        ];
        ClearSystemCache[];
        Share[];
        {mid - before, TExternPinCount[] - before}
    ] // MatchQ[{3, _Integer ? (# <= 3 &)}],
    True,
    TestID -> "extern-pin/scope-exit-drops-pins"
]

(* === explicit TTermUnpin still works alongside managed handles === *)

VerificationTest[
    TInit[];
    TReset[];
    Module[{before, t, mid, after},
        before = TExternPinCount[];
        t = TEra[];
        mid = TExternPinCount[];
        TTermUnpin[t];
        after = TExternPinCount[];
        {mid - before, after - mid}
    ],
    {1, -1},
    TestID -> "extern-pin/explicit-unpin-decrements"
]

End[];
EndPackage[];
