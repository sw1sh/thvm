(* pending_tensor_numeric.wlt -- numerical tests that exercise
   build -> materialize -> dispatch end-to-end.

   Files in this directory named pending_*.wlt are run by run.wls
   as INFORMATIONAL: a failure here does not break the build.  They
   document what commit 4's TWnf + interact_kernel must make green.

   Style notes:
     - Use the WL numeric UpValues (+, *, -, ^(1/2)) instead of
       explicit TUOpAdd/TUOpMul/etc.  The UpValues in Tensor.wl
       rewrite Plus/Times/Minus/Power to UOp graphs automatically.
     - TRealize[expr] == TWnf[TUOpMaterialize[expr]] -- the
       one-liner end of the pipeline.
     - TTensorData returns a NumericArray; wrap in Normal to
       compare against plain lists.

   Once commit 4 lands and these pass, rename the file to drop the
   `pending_` prefix.
*)

(* === elementwise via UpValues === *)

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0, 4.0},    "Real32"];
    b = TTensorCreate @ NumericArray[{10.0, 20.0, 30.0, 40.0}, "Real32"];
    Normal @ TTensorData @ TRealize[a + b],
    {11.0, 22.0, 33.0, 44.0},
    TestID -> "numeric/add-1d"
]

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0, 4.0},    "Real32"];
    b = TTensorCreate @ NumericArray[{10.0, 20.0, 30.0, 40.0}, "Real32"];
    Normal @ TTensorData @ TRealize[a * b],
    {10.0, 40.0, 90.0, 160.0},
    TestID -> "numeric/mul-1d"
]

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, -2.0, 3.0}, "Real32"];
    Normal @ TTensorData @ TRealize[-a],
    {-1.0, 2.0, -3.0},
    TestID -> "numeric/neg"
]

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 4.0, 9.0}, "Real32"];
    Normal @ TTensorData @ TRealize[a ^ (1/2)],
    {1.0, 2.0, 3.0},
    TestID -> "numeric/sqrt-power"
]

(* === scalar broadcast === *)

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    (* 2 * a + 1 -- scalars lift to UOP_CONST. *)
    Normal @ TTensorData @ TRealize[2.0 * a + 1.0],
    {3.0, 5.0, 7.0},
    TestID -> "numeric/scalar-broadcast"
]

(* === reductions === *)

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0, 4.0}, "Real32"];
    Normal @ TTensorData @ TRealize @ TUOpReduce[a, 0, "SUM"],
    {10.0},
    TestID -> "numeric/reduce-sum-1d"
]

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 5.0, 2.0, 4.0}, "Real32"];
    Normal @ TTensorData @ TRealize @ TUOpReduce[a, 0, "MAX"],
    {5.0},
    TestID -> "numeric/reduce-max-1d"
]

(* === compound via UpValues === *)

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    b = TTensorCreate @ NumericArray[{4.0, 5.0, 6.0}, "Real32"];
    c = TTensorCreate @ NumericArray[{7.0, 8.0, 9.0}, "Real32"];
    Normal @ TTensorData @ TRealize[(a + b) * c],
    {35.0, 56.0, 81.0},
    TestID -> "numeric/compound-add-mul"
]

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    (* 2a^2 + 1 *)
    Normal @ TTensorData @ TRealize[2.0 * a * a + 1.0],
    {3.0, 9.0, 19.0},
    TestID -> "numeric/compound-polynomial"
]

(* === 2D === *)

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{{1.0, 2.0}, {3.0, 4.0}}, "Real32"];
    b = TTensorCreate @ NumericArray[{{10.0, 20.0}, {30.0, 40.0}}, "Real32"];
    Normal @ TTensorData @ TRealize[a + b],
    {{11.0, 22.0}, {33.0, 44.0}},
    TestID -> "numeric/add-2d"
]
