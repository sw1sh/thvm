(* adam_host.wlt -- TAdamHostInit + TAdamHostStep multi-tensor
   host-side Adam update.  No TTerm involvement; pure WL numeric. *)

VerificationTest[
    TInit[];
    weights = {NumericArray[{1.0, 2.0, 3.0}, "Real32"],
               NumericArray[{{0.5, -0.5}, {0.0, 1.0}}, "Real32"]};
    {m0, v0} = TAdamHostInit[weights];
    {Map[Normal, m0], Map[Normal, v0]},
    {{{0., 0., 0.}, {{0., 0.}, {0., 0.}}},
     {{0., 0., 0.}, {{0., 0.}, {0., 0.}}}},
    TestID -> "adam-host/init-zeros-like"
]

(* Single-step manual reference: gradient of sum(w*w) wrt w is 2w.
   With lr=0.1, beta1=0.9, beta2=0.999, eps=1e-8, t=1, w_init={2}:
     g  = 4
     m  = 0.1 * g  = 0.4
     v  = 0.001 * g^2  = 0.016
     mHat = m / (1 - 0.9) = 4
     vHat = v / (1 - 0.999) = 16
     update = 0.1 * 4 / (sqrt(16) + 1e-8) ~ 0.1
     w_new = 2 - 0.1 = 1.9. *)
VerificationTest[
    TInit[];
    weights = {NumericArray[{2.0}, "Real32"]};
    grads   = {NumericArray[{4.0}, "Real32"]};
    {m0, v0} = TAdamHostInit[weights];
    {wNew, mOut, vOut} = TAdamHostStep[weights, grads,
                                        m0, v0, 0.1, 0.9,
                                        0.999, 1.0*^-8, 1];
    Normal @ First @ wNew,
    {1.9},
    SameTest -> (Max[Abs[#1 - #2]] < 1.0*^-4 &),
    TestID -> "adam-host/single-step-w-equals-1.9"
]

(* Two-step: gradient remains 2*w (recompute).  Verify the second
   step's m/v carry over correctly. *)
VerificationTest[
    TInit[];
    w   = {NumericArray[{2.0}, "Real32"]};
    {m, v} = TAdamHostInit[w];
    Do[
        gNew = {NumericArray[2.0 * Normal @ First @ w, "Real32"]};
        {w, m, v} = TAdamHostStep[w, gNew, m, v,
                                   0.1, 0.9, 0.999, 1.0*^-8, t];
        ,
        {t, 2}
    ];
    Normal @ First @ w,
    {1.7999996554374693},   (* approx; varies tiny with f32 vs f64 *)
    SameTest -> (Max[Abs[#1 - #2]] < 0.01 &),
    TestID -> "adam-host/two-step-loss-decrease"
]

(* Multi-tensor: per-tensor updates are independent. *)
VerificationTest[
    TInit[];
    w = {NumericArray[{2.0, 4.0}, "Real32"],
         NumericArray[{{1.0, 1.0}, {1.0, 1.0}}, "Real32"]};
    g = {NumericArray[{4.0, 8.0}, "Real32"],
         NumericArray[{{2.0, 2.0}, {2.0, 2.0}}, "Real32"]};
    {m0, v0} = TAdamHostInit[w];
    {wNew, mt, vt} = TAdamHostStep[w, g, m0, v0,
                                    0.1, 0.9, 0.999, 1.0*^-8, 1];
    (* Each entry shifts by ~0.1 toward zero (sign of -g). *)
    {AllTrue[Flatten[Normal @ First @ wNew - {1.9, 3.9}],
             Abs[#] < 1.0*^-4 &],
     AllTrue[Flatten[Normal @ Last  @ wNew - {{0.9, 0.9}, {0.9, 0.9}}],
             Abs[#] < 1.0*^-4 &]},
    {True, True},
    TestID -> "adam-host/multi-tensor-independent-updates"
]
