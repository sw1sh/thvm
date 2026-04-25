(* cmpeq.wlt -- UOP_CMPEQ elementwise mask of (a == b).  0/1 floats
   in the input dtype.  Mirror of CMPLT; primarily used by the
   REDUCE_MAX grad rule for the one-hot argmax indicator. *)

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0, 4.0}, "Real32"];
    b = TTensorCreate @ NumericArray[{1.0, 5.0, 3.0, 7.0}, "Real32"];
    r = TRealize @ TUOpCmpeq[a, b];
    Normal @ TTensorData[r],
    {1.0, 0.0, 1.0, 0.0},
    TestID -> "cmpeq/elementwise-mask"
]

VerificationTest[
    TInit[];
    (* Broadcast: scalar vs vector. *)
    a = TTensorCreate @ NumericArray[{2.0}, "Real32"];
    b = TTensorCreate @ NumericArray[{1.0, 2.0, 2.0, 3.0}, "Real32"];
    r = TRealize @ TUOpCmpeq[a, b];
    Normal @ TTensorData[r],
    {0.0, 1.0, 1.0, 0.0},
    TestID -> "cmpeq/broadcast-scalar-vs-vector"
]

VerificationTest[
    TInit[];
    (* Argmax-style use: (a == max(a)) yields a one-hot at the
       argmax position.  Inputs where the max is unique. *)
    a = TTensorCreate @ NumericArray[{1.0, 5.0, 3.0, 2.0}, "Real32"];
    mx = TUOpReduce[a, 0, "MAX"];   (* shape {1}, value 5 *)
    onehot = TRealize @ TUOpCmpeq[a, mx];
    Normal @ TTensorData[onehot],
    {0.0, 1.0, 0.0, 0.0},
    TestID -> "cmpeq/argmax-one-hot-via-broadcast"
]
