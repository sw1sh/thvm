(* pending_tensor_numeric.wlt -- numerical tests that exercise
   build -> materialize -> dispatch end-to-end.

   Files in this directory named pending_*.wlt are run by run.wls
   as INFORMATIONAL: a failure here does not break the build.  They
   document what commit 4's TWnf + interact_kernel must make green.

   Once commit 4 lands and these pass, rename the file to drop the
   `pending_` prefix.
*)

(* === elementwise === *)

VerificationTest[
    TInit[];
    a = TTensor[{4}, {1.0, 2.0, 3.0, 4.0}];
    b = TTensor[{4}, {10.0, 20.0, 30.0, 40.0}];
    TTensorData[TWnf[TUOpMaterialize[TUOpAdd[a, b]]]],
    {11.0, 22.0, 33.0, 44.0},
    TestID -> "numeric/add-1d"
]

VerificationTest[
    TInit[];
    a = TTensor[{4}, {1.0, 2.0, 3.0, 4.0}];
    b = TTensor[{4}, {10.0, 20.0, 30.0, 40.0}];
    TTensorData[TWnf[TUOpMaterialize[TUOpMul[a, b]]]],
    {10.0, 40.0, 90.0, 160.0},
    TestID -> "numeric/mul-1d"
]

VerificationTest[
    TInit[];
    a = TTensor[{3}, {1.0, -2.0, 3.0}];
    TTensorData[TWnf[TUOpMaterialize[TUOpNeg[a]]]],
    {-1.0, 2.0, -3.0},
    TestID -> "numeric/neg"
]

VerificationTest[
    TInit[];
    a = TTensor[{3}, {1.0, 4.0, 9.0}];
    TTensorData[TWnf[TUOpMaterialize[TUOpSqrt[a]]]],
    {1.0, 2.0, 3.0},
    TestID -> "numeric/sqrt"
]

(* === WL operator overloading via UpValues === *)

VerificationTest[
    TInit[];
    a = TTensor[{3}, {1.0, 2.0, 3.0}];
    b = TTensor[{3}, {4.0, 5.0, 6.0}];
    (* Plain WL Plus; the UpValue rewrites to TUOpAdd internally. *)
    TTensorData[TWnf[TUOpMaterialize[a + b]]],
    {5.0, 7.0, 9.0},
    TestID -> "numeric/plus-upvalue"
]

VerificationTest[
    TInit[];
    a = TTensor[{3}, {1.0, 2.0, 3.0}];
    b = TTensor[{3}, {4.0, 5.0, 6.0}];
    TTensorData[TWnf[TUOpMaterialize[a * b]]],
    {4.0, 10.0, 18.0},
    TestID -> "numeric/times-upvalue"
]

VerificationTest[
    TInit[];
    a = TTensor[{3}, {1.0, 2.0, 3.0}];
    TTensorData[TWnf[TUOpMaterialize[-a]]],
    {-1.0, -2.0, -3.0},
    TestID -> "numeric/minus-upvalue"
]

VerificationTest[
    TInit[];
    a = TTensor[{3}, {1.0, 2.0, 3.0}];
    (* Scalar lifts to UOP_CONST; tensor is broadcast. *)
    TTensorData[TWnf[TUOpMaterialize[2.0 * a + 1.0]]],
    {3.0, 5.0, 7.0},
    TestID -> "numeric/scalar-broadcast"
]

VerificationTest[
    TInit[];
    a = TTensor[{3}, {1.0, 4.0, 9.0}];
    TTensorData[TWnf[TUOpMaterialize[a ^ (1/2)]]],
    {1.0, 2.0, 3.0},
    TestID -> "numeric/power-half-sqrt"
]

(* === reductions === *)

VerificationTest[
    TInit[];
    a = TTensor[{4}, {1.0, 2.0, 3.0, 4.0}];
    TTensorData[TWnf[TUOpMaterialize[TUOpReduce[a, 0, "SUM"]]]],
    {10.0},
    TestID -> "numeric/reduce-sum-1d"
]

VerificationTest[
    TInit[];
    a = TTensor[{4}, {1.0, 5.0, 2.0, 4.0}];
    TTensorData[TWnf[TUOpMaterialize[TUOpReduce[a, 0, "MAX"]]]],
    {5.0},
    TestID -> "numeric/reduce-max-1d"
]

(* === compound expressions === *)

VerificationTest[
    TInit[];
    a = TTensor[{3}, {1.0, 2.0, 3.0}];
    b = TTensor[{3}, {4.0, 5.0, 6.0}];
    c = TTensor[{3}, {7.0, 8.0, 9.0}];
    (* (a + b) * c *)
    TTensorData[TWnf[TUOpMaterialize[(a + b) * c]]],
    {35.0, 56.0, 81.0},
    TestID -> "numeric/compound-add-mul"
]
