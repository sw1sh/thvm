(* extern_pin.wlt - WL-side observation of the managed pin handles
                    backing TTerm.  Verifies that the C-side pin
                    count drops when the surrounding scope exits
                    and the TTerm wrappers are no longer reachable. *)

BeginPackage["THVMLinkTests`Extern`Pin`",
    {"WolframInstitute`THVMLink`", "MUnit`"}];

Begin["`Private`"];

(* === a fresh TTerm pins a Term on construction; it is held, since a
       discarded wrapper may release its pin by the next library call === *)

VerificationTest[
    TInit[];
    TReset[];
    Module[{before, t, after},
        before = TExternPinCount[];
        t = TEra[];
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

(* === leaving a Module scope drops the wrappers: WL releases their handles
       and the pin count is back at its baseline === *)

VerificationTest[
    TInit[];
    TReset[];
    Module[{before, mid},
        before = TExternPinCount[];
        Module[{terms = {TEra[], TEra[], TEra[]}},
            mid = TExternPinCount[];
            Length[terms]
        ];
        {mid - before, TExternPinCount[] - before}
    ],
    {3, 0},
    TestID -> "extern-pin/scope-exit-drops-pins"
]

(* === discarded temporaries hold no pins (the evaluator may still hold the
       loop's last value) === *)

VerificationTest[
    TInit[];
    TReset[];
    Module[{before = TExternPinCount[]},
        Do[TEra[], {100}];
        TExternPinCount[] - before <= 1
    ],
    True,
    TestID -> "extern-pin/temporaries-release"
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

(* === a wrapper unpinned by hand releases nothing more when it is dropped:
       the other wrapper of the same term keeps its pin === *)

VerificationTest[
    TInit[];
    TReset[];
    Module[{before = TExternPinCount[], e2 = TEra[]},
        Module[{e1 = TEra[]}, TTermUnpin[e1]];
        TExternPinCount[] - before
    ],
    1,
    TestID -> "extern-pin/unpinned-wrapper-drop-keeps-other-pins"
]

End[];
EndPackage[];
