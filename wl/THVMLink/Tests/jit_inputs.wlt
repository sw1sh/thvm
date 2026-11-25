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

(* "Prune" -> True: a dispatch that reads no input, here the doubling of a fixed tensor, runs in the
   capture call only (tinygrad's TinyJit prune, engine/jit.py:14-22).  The replays still compute each
   call's own result, and the capture records one dispatch fewer to replay. *)
replayedDispatches[f_] := Count[TJitCaptureOps[f], row_ /; KeyExistsQ[row, "Kid"] && ! TrueQ[row["ReplaySkip"]]]

VerificationTest[
    TInit[]; TReset[];
    Module[{w = TRealize[TTensorCreate[{1., 2., 3., 4.}]], body, plain, pruned, xs, want},
        body = Function[x, TRealize[TRealize[w * 2.] + x]];
        plain = TJit[body];
        pruned = TJit[body, "Prune" -> True];
        xs = {{1., 1., 1., 1.}, {10., 20., 30., 40.}, {-1., 0., 5., 2.}, {4., 4., 4., 4.}};
        want = ({2., 4., 6., 8.} + #) & /@ xs;
        {Normal[plain[TTensorCreate[#]]] & /@ xs, Normal[pruned[TTensorCreate[#]]] & /@ xs,
         replayedDispatches[plain], replayedDispatches[pruned]} === {want, want, 2, 1}],
    True,
    TestID -> "TJit/prune-runs-an-input-free-dispatch-once"
]

(* The same on Metal, where the capture records without running and the first replay fills the results:
   the pruned dispatch's output must still be there for the replays that read it. *)
VerificationTest[
    TInit[]; TReset[];
    Module[{ctx = TContextNew["metal"], ok},
        If[ ctx === 0, Return[True]];
        ok = TInContext[ctx,
            Module[{w = TRealize[TTensorCreate[N @ Range[64]]], pruned, xs},
                pruned = TJit[Function[x, TRealize[TRealize[w * 2.] + x]], "Prune" -> True];
                xs = Table[N[k] + 0. Range[64], {k, {1, 7, -3, 20}}];
                {(Normal[pruned[TTensorCreate[#]]] & /@ xs) === ((2. Range[64] + #) & /@ xs), replayedDispatches[pruned]}]];
        TContextDestroy[ctx];
        ok === {True, 1}],
    True,
    TestID -> "TJit/prune-on-metal-keeps-the-pruned-output"
]

(* Several input-free dispatches of one size, each read at a different point of the chain
   y <- c_k w_k + y.  On a graphics processor the replay gives their outputs one buffer, which holds the
   last one's bytes once the capture has run: a pruned output is saved as its dispatch finishes, where a
   copy taken afterwards gave every reader the last weight. *)
VerificationTest[
    TInit[]; TReset[];
    Module[{ctx = TContextNew["metal"], ok},
        If[ ctx === 0, Return[True]];
        ok = TInContext[ctx,
            Module[{hs = Table[N[Mod[k Range[64], 7]] + k, {k, 4}], ws, body, pruned, xs, want},
                ws = TRealize[TTensorCreate[#]] & /@ hs;
                body = Function[x, Fold[Function[{acc, kw}, TRealize[TRealize[kw[[2]] * kw[[1]]] + acc]], x, Transpose[{{2., 3., 5., 7.}, ws}]]];
                pruned = TJit[body, "Prune" -> True];
                xs = Table[N[j] + 0. Range[64], {j, {1, 10, -3, 4}}];
                want = (2. hs[[1]] + 3. hs[[2]] + 5. hs[[3]] + 7. hs[[4]] + #) & /@ xs;
                {(Normal[pruned[TTensorCreate[#]]] & /@ xs) === want, replayedDispatches[pruned]}]];
        TContextDestroy[ctx];
        ok === {True, 4}],
    True,
    TestID -> "TJit/prune-saves-outputs-that-share-a-replay-buffer"
]

(* A tensor the body makes and the caller keeps: a loader that prepares a weight on first use and stores
   it, as a model's lazy weight loader does inside its captured forward.  Pruned, the weight is made once,
   so it must keep its own buffer through the replays (tinygrad holds every buffer a live tensor reaches
   out of its planner, engine/jit.py:161): the kept weights still read their values afterwards, and a
   second capture that reuses them computes with the right ones. *)
VerificationTest[
    TInit[]; TReset[];
    Module[{ctx = TContextNew["metal"], ok},
        If[ ctx === 0, Return[True]];
        ok = TInContext[ctx,
            Module[{hs = Table[N[Mod[k Range[64], 7]] + k, {k, 4}], ws, cache = <||>, load, first, second, xs, r1, kept, r2},
                ws = TRealize[TTensorCreate[#]] & /@ hs;
                load = Function[k, Lookup[cache, k, cache[k] = TRealize[ws[[k]] * 2.]]];
                first = TJit[Function[x, Fold[Function[{acc, k}, TRealize[load[k] + acc]], x, Range[4]]], "Prune" -> True];
                xs = Table[N[j] + 0. Range[64], {j, {1, 10, -3, 4}}];
                r1 = Normal[first[TTensorCreate[#]]] & /@ xs;
                kept = Normal /@ Values[KeySort[cache]];
                second = TJit[Function[x, Fold[Function[{acc, k}, TRealize[load[k] * 3. + acc]], x, Range[4]]], "Prune" -> True];
                r2 = Normal[second[TTensorCreate[#]]] & /@ xs;
                {r1 === ((2. Total[hs] + #) & /@ xs), kept === (2. # & /@ hs), r2 === ((6. Total[hs] + #) & /@ xs)}]];
        TContextDestroy[ctx];
        ok],
    {True, True, True},
    TestID -> "TJit/prune-keeps-a-tensor-the-caller-holds"
]
