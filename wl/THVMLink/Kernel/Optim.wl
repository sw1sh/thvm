(* ::Package:: *)
(* Optim.wl - optimizer constructors that produce recursive lambda
   terms (TLam + TDef + TRef + TIfZero) suitable for TRealize.

   Mirrors the SGD-as-lambda pattern from wl/THVMLink/Tests/sgd.wlt.

   Public surface
     TOptim["SGD",  lr][gradFn, w0, n]
                       -- SGD recursion: w_{k+1} = w_k - lr * gradFn(w_k)
                          for n iterations.  Returns a TTerm; TRealize
                          to evaluate.
     TOptim["Adam", lr, beta1, beta2, eps][gradFn, w0, n]
                       -- (Phase-1 next item; stub raises Failure.)

   gradFn is a Wolfram Function that takes a TTerm w and returns the
   loss gradient w.r.t. w as a TTerm UOp graph.
*)

BeginPackage["THVMLink`"];

TOptim::usage = "TOptim[\"SGD\", lr] returns a function {gradFn, w0, n} -> TTerm that, when realised, performs n SGD steps from w0 with learning rate `lr`.  TOptim[\"Adam\", lr, beta1, beta2, eps] likewise for Adam.  gradFn is a host-side function w_TTerm -> TTerm carrying the gradient w.r.t. w.";

(* Forward-declare symbols owned by later-loading siblings (Ref.wl,
   Switch.wl).  Without this, bare references to TDef / TIfZero /
   TRef / TOp2 / TNum below would resolve to phantom symbols in
   THVMLink`Private` (since those symbols don't yet exist on the
   $ContextPath when this file is Get'd), leading to silently
   broken term construction at call time. *)
{TDef, TRef, TIfZero, TOp2, TNum, TMatNum};

Begin["`Private`"];

$optimDefCounter = 0;

freshOptimDefName[algo_String] := (
    $optimDefCounter += 1;
    algo <> "_loop_" <> ToString[$optimDefCounter]
)

(* === Adam helpers ===

   tZerosLike[wTen]  -- given a TTerm wrapping TAG_TEN, returns a
                        fresh f32 zero TTensor with the same shape.
                        Used to seed Adam's m and v running buffers.
   tF32[x]           -- shorthand for TUOpConst[x, "f32"].  Adam's
                        body needs many scalar-CONST operands
                        (1-beta1, eps, etc.) and the wrapper keeps
                        them readable. *)

tZerosLike[wTen_TTerm] := With[{shape = TTensorShape[wTen]},
    TTensorCreate @ NumericArray[ConstantArray[0., shape], "Real32"]
]

tF32[x_] := TUOpConst[N[x], "f32"]

(* SGD body: build the recursive lambda + invocation as one TTerm. *)
sgdRecursiveTerm[gradFn_, lr_, w0_, n_] :=
    Module[{defName = freshOptimDefName["sgd"], w, k},
        TDef[defName,
            TLam[w, TLam[k,
                TIfZero[k, w,
                    TApp[
                        TApp[TRef[defName],
                            TUOpAdd[w, TUOpNeg[TUOpMul[lr, gradFn[w]]]]],
                        TOp2["-", k, TNum[1]]]]]]];
        TApp[TApp[TRef[defName], w0], TNum[n]]
    ]

(* === Public dispatch.  Pattern: TOptim[name, hyperparams] returns a
   Function that the user invokes with (gradFn, w0, n). === *)

TOptim["SGD", lr_TTerm] :=
    Function[{gradFn, w0, n}, sgdRecursiveTerm[gradFn, lr, w0, n]]

(* Adam stub -- next task item replaces this with real numerics. *)
TOptim["Adam", lr_TTerm, beta1_, beta2_, eps_] :=
    Function[{gradFn, w0, n},
        Failure["NotImplemented",
            <| "Message" -> "TOptim[\"Adam\", ...] not yet implemented",
               "lr" -> lr, "beta1" -> beta1, "beta2" -> beta2, "eps" -> eps |>]
    ]

End[];
EndPackage[];
