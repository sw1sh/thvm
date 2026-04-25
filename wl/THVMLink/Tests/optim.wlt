(* optim.wlt -- TOptim["SGD"] / TOptim["Adam"] recursive lambda
   constructors.  The SGD case mirrors the recursion expected of any
   optimizer: a Wolfram-side gradient closure feeds the per-iteration
   step.  Numerics here match wl/THVMLink/Tests/sgd.wlt's
   "recursive-three-iters" check, which constructs the same loop by
   hand. *)

(* Shared gradient closure: d/dw [L2(w - target)] = 2(w - target).
   We build it as a TUOp graph each time the loop substitutes w. *)

VerificationTest[
    TInit[];
    target = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    lr     = TUOpConst[0.1, "f32"];
    gradFn = Function[w, TGrad[TL2Loss[TUOpAdd[w, TUOpNeg[target]]], w]];
    w0     = TTensorCreate @ NumericArray[{0.0, 0.0, 0.0}, "Real32"];
    out    = TOptim["SGD", lr][gradFn, w0, 3];
    Round[Normal @ TTensorData @ TRealize @ out, 0.001],
    {0.488, 0.976, 1.464},
    TestID -> "optim/sgd-three-iters-matches-by-hand-recursion"
]

(* zero iters returns w0 unchanged *)
VerificationTest[
    TInit[];
    target = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    lr     = TUOpConst[0.1, "f32"];
    gradFn = Function[w, TGrad[TL2Loss[TUOpAdd[w, TUOpNeg[target]]], w]];
    w0     = TTensorCreate @ NumericArray[{0.0, 0.0, 0.0}, "Real32"];
    out    = TOptim["SGD", lr][gradFn, w0, 0];
    Normal @ TTensorData @ TRealize @ out,
    {0.0, 0.0, 0.0},
    TestID -> "optim/sgd-zero-iters-returns-w0"
]

(* Adam stub for now -- the next task item replaces this with real
   numerics.  Currently returns a Failure so callers see the gap. *)
VerificationTest[
    TInit[];
    Head @ TOptim["Adam", TUOpConst[0.001, "f32"], 0.9, 0.999, 1.*^-8][
        Function[w, w], Null, 0],
    Failure,
    TestID -> "optim/adam-stub-returns-Failure"
]
