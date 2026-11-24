(* jit_inputs.wlt - the arguments of a TJit function: each call's tensor arguments are that call's
   inputs, whether they come realized or lazy.  A lazy argument is computed before the capture, as
   tinygrad's TinyJit realizes its inputs before it captures (engine/jit.py:93, 127), so the kernels that
   compute it are not replayed.  Small tensors on the default device. *)

(* A realized argument: every replay computes on that call's tensor. *)
VerificationTest[
    TInit[]; TReset[];
    Module[{f = TJit[Function[x, TRealize[x * 2. + 1.]]]},
        Normal[f[TTensorCreate[#]]] & /@ {{1., 2., 3.}, {10., 20., 30.}, {-1., 0., 5.}, {4., 4., 4.}}],
    {{3., 5., 7.}, {21., 41., 61.}, {-1., 1., 11.}, {9., 9., 9.}},
    TestID -> "TJit/each-call-computes-on-its-own-argument"
]

(* A lazy argument, a cast of a tensor that changes between the calls: the replays compute on the new
   tensor, not on the one the capture saw. *)
VerificationTest[
    TInit[]; TReset[];
    Module[{f = TJit[Function[x, TRealize[x * 2. + 1.]]]},
        Normal[f[TUOpCast[TTensorCreate[#], "bf16"]]] & /@ {{1., 2., 3.}, {10., 20., 30.}, {-1., 0., 5.}, {4., 4., 4.}}],
    {{3., 5., 7.}, {21., 41., 61.}, {-1., 1., 11.}, {9., 9., 9.}},
    TestID -> "TJit/a-lazy-cast-argument-is-each-call's-own"
]

(* A lazy argument that is arithmetic on a running value, with a second argument that also changes: the
   shape of a sampling loop, z <- z + dt f(cast(z), t).  The loop through TJit is the loop without it. *)
VerificationTest[
    TInit[]; TReset[];
    Module[{w = TTensorCreate[{{0.5, -0.25, 0.125}, {0.25, 0.5, -0.5}, {-0.125, 0.25, 0.5}}], body, loop},
        body = Function[{z, t}, TRealize[TMatMul[z, w] * t + 0.5]];
        loop[step_] := Module[{z = TTensorCreate[{{1., 2., 3.}, {-2., 0., 4.}}]},
            Do[z = TRealize[z + TUOpCast[step[TUOpCast[z, "bf16"], TTensorCreate[{{0.5 k}}]], "f32"] * (-0.25)], {k, 4}];
            Normal[z]];
        loop[TJit[body]] === loop[body]],
    True,
    TestID -> "TJit/a-sampling-loop-with-a-lazy-argument-is-the-loop-without-TJit"
]

(* A lazy sum as the argument. *)
VerificationTest[
    TInit[]; TReset[];
    Module[{f = TJit[Function[x, TRealize[x * x]]]},
        Normal[f[TTensorCreate[#] + 1.]] & /@ {{1., 2.}, {3., 4.}, {0., -3.}}],
    {{4., 9.}, {16., 25.}, {1., 4.}},
    TestID -> "TJit/a-lazy-sum-argument-is-each-call's-own"
]
