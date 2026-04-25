(* adam_session.wlt -- TAdamSessionInit / TAdamSessionStep /
   TAdamSessionDrop session-scoped Adam state arena.  Mirrors
   adam_host.wlt's reference numbers but with the m/v running
   buffers stored under a key in $adamSessions instead of being
   threaded through the caller. *)

VerificationTest[
    TInit[];
    TAdamSessionDrop[];   (* clean slate *)
    weights = {NumericArray[{2.0}, "Real32"]};
    grads   = {NumericArray[{4.0}, "Real32"]};
    TAdamSessionInit["case1", weights];
    wNew = TAdamSessionStep["case1", weights, grads,
                            0.1, 0.9, 0.999, 1.0*^-8, 1];
    Normal @ First @ wNew,
    {1.9},
    SameTest -> (Max[Abs[#1 - #2]] < 1.0*^-4 &),
    TestID -> "adam-session/single-step-matches-host-step"
]

(* Two iterations using the session keep m/v alive across calls and
   match the explicit m/v threading from adam_host.wlt. *)
VerificationTest[
    TInit[];
    TAdamSessionDrop[];
    w = {NumericArray[{2.0}, "Real32"]};
    TAdamSessionInit["case2", w];
    Do[
        gNew = {NumericArray[2.0 * Normal @ First @ w, "Real32"]};
        w    = TAdamSessionStep["case2", w, gNew,
                                0.1, 0.9, 0.999, 1.0*^-8, t];
        ,
        {t, 2}
    ];
    Normal @ First @ w,
    {1.7999996554374693},
    SameTest -> (Max[Abs[#1 - #2]] < 0.01 &),
    TestID -> "adam-session/two-step-matches-host-loop"
]

(* Independent keys carry independent state -- a step on key A
   must not perturb key B's m/v. *)
VerificationTest[
    TInit[];
    TAdamSessionDrop[];
    wA = {NumericArray[{2.0}, "Real32"]};
    wB = {NumericArray[{2.0}, "Real32"]};
    TAdamSessionInit["A", wA];
    TAdamSessionInit["B", wB];
    (* Step A only. *)
    wA = TAdamSessionStep["A", wA, {NumericArray[{4.0}, "Real32"]},
                          0.1, 0.9, 0.999, 1.0*^-8, 1];
    (* Now step B with the same recipe -- should land at the same
       value as A (B's m/v are still zero, fresh). *)
    wB = TAdamSessionStep["B", wB, {NumericArray[{4.0}, "Real32"]},
                          0.1, 0.9, 0.999, 1.0*^-8, 1];
    Normal /@ {First @ wA, First @ wB},
    {{1.9}, {1.9}},
    SameTest -> (Max[Abs[Flatten[#1 - #2]]] < 1.0*^-4 &),
    TestID -> "adam-session/independent-keys-do-not-interfere"
]

(* Drop frees the entry; subsequent step on the dropped key
   returns $Failed. *)
VerificationTest[
    TInit[];
    TAdamSessionDrop[];
    w = {NumericArray[{1.0}, "Real32"]};
    TAdamSessionInit["doomed", w];
    TAdamSessionDrop["doomed"];
    Quiet @ TAdamSessionStep["doomed", w,
                              {NumericArray[{1.0}, "Real32"]},
                              0.1, 0.9, 0.999, 1.0*^-8, 1],
    $Failed,
    TestID -> "adam-session/drop-then-step-returns-failure"
]

(* Multi-tensor independence: per-tensor updates inside a single
   session match the multi-tensor reference in adam_host.wlt. *)
VerificationTest[
    TInit[];
    TAdamSessionDrop[];
    w = {NumericArray[{2.0, 4.0}, "Real32"],
         NumericArray[{{1.0, 1.0}, {1.0, 1.0}}, "Real32"]};
    g = {NumericArray[{4.0, 8.0}, "Real32"],
         NumericArray[{{2.0, 2.0}, {2.0, 2.0}}, "Real32"]};
    TAdamSessionInit["multi", w];
    wNew = TAdamSessionStep["multi", w, g,
                            0.1, 0.9, 0.999, 1.0*^-8, 1];
    {AllTrue[Flatten[Normal @ First @ wNew - {1.9, 3.9}],
             Abs[#] < 1.0*^-4 &],
     AllTrue[Flatten[Normal @ Last  @ wNew - {{0.9, 0.9}, {0.9, 0.9}}],
             Abs[#] < 1.0*^-4 &]},
    {True, True},
    TestID -> "adam-session/multi-tensor-step-matches-host"
]
