(* einx.wlt -- einx-style verbs over the TTerm UOp graph.
   Validates the WL parser + alignment + per-verb lowering against
   hand-computed expected outputs and (where natural) existing thvm
   primitives (TSoftmax, TEmbeddingMatrix, TMatMul). *)

(* === parser sanity =============================================== *)

VerificationTest[
    WolframInstitute`THVMLink`Private`parseEinxPattern["b (h c) d -> b h c d"],
    <|
        "in"           -> {{"b", {"h", "c"}, "d"}},
        "out"          -> {"b", "h", "c", "d"},
        "bracketed"    -> {},
        "outBracketed" -> {},
        "outSpecified" -> True,
        "pattern"      -> "b (h c) d -> b h c d"
    |>,
    TestID -> "einx/parse/composite"
]

VerificationTest[
    WolframInstitute`THVMLink`Private`parseEinxPattern["b [s] d -> b d"],
    <|
        "in"           -> {{"b", "s", "d"}},
        "out"          -> {"b", "d"},
        "bracketed"    -> {"s"},
        "outBracketed" -> {},
        "outSpecified" -> True,
        "pattern"      -> "b [s] d -> b d"
    |>,
    TestID -> "einx/parse/bracketed"
]

VerificationTest[
    WolframInstitute`THVMLink`Private`parseEinxPattern["a b, b c -> a c"],
    <|
        "in"           -> {{"a", "b"}, {"b", "c"}},
        "out"          -> {"a", "c"},
        "bracketed"    -> {},
        "outBracketed" -> {},
        "outSpecified" -> True,
        "pattern"      -> "a b, b c -> a c"
    |>,
    TestID -> "einx/parse/twoInputs"
]

VerificationTest[
    WolframInstitute`THVMLink`Private`parseEinxPattern["b [s] d"],
    <|
        "in"           -> {{"b", "s", "d"}},
        "out"          -> {},
        "bracketed"    -> {"s"},
        "outBracketed" -> {},
        "outSpecified" -> False,
        "pattern"      -> "b [s] d"
    |>,
    TestID -> "einx/parse/noArrow"
]

(* === Rearrange =================================================== *)

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[
        Table[ N[1.0 + i + 10 j], {i, 0, 2}, {j, 0, 1}],
        "Real32"];
    Normal @ TTensorData @ TRealize @ TEinRearrange["a b -> b a", a],
    {{1.0, 2.0, 3.0}, {11.0, 12.0, 13.0}},
    TestID -> "einx/rearrange/transpose"
]

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[
        Table[ N[i], {i, 0, 11}],
        "Real32"];
    Normal @ TTensorData @ TRealize @ TEinRearrange[
        "(h c) -> h c", a, "h" -> 3],
    {{0.0, 1.0, 2.0, 3.0}, {4.0, 5.0, 6.0, 7.0}, {8.0, 9.0, 10.0, 11.0}},
    TestID -> "einx/rearrange/split"
]

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[
        Table[ N[i], {i, 0, 5}],
        "Real32"];
    (* {6} -> reshape {2,3} via input pattern -> merge back as {6} *)
    Normal @ TTensorData @ TRealize @ TEinRearrange[
        "(a b) -> (a b)", a, "a" -> 2],
    {0.0, 1.0, 2.0, 3.0, 4.0, 5.0},
    TestID -> "einx/rearrange/identityViaComposite"
]

(* === Sum ========================================================= *)

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[
        {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}},
        "Real32"];
    Normal @ TTensorData @ TRealize @ TEinSum["b [s] -> b", a],
    {6.0, 15.0},
    TestID -> "einx/sum/rowsum"
]

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[
        {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}},
        "Real32"];
    Normal @ TTensorData @ TRealize @ TEinSum["[b] s -> s", a],
    {5.0, 7.0, 9.0},
    TestID -> "einx/sum/colsum"
]

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[
        {{1.0, 2.0}, {3.0, 4.0}},
        "Real32"];
    Normal @ TTensorData @ TRealize @ TEinSum["[a] [b] ->", a],
    {10.0},
    TestID -> "einx/sum/total"
]

(* === Mean ======================================================== *)

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[
        {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}},
        "Real32"];
    Normal @ TTensorData @ TRealize @ TEinMean["b [s] -> b", a],
    {2.0, 5.0},
    TestID -> "einx/mean/row"
]

(* === Max / Min =================================================== *)

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[
        {{1.0, 5.0, 3.0}, {7.0, 2.0, 6.0}},
        "Real32"];
    Normal @ TTensorData @ TRealize @ TEinMax["b [s] -> b", a],
    {5.0, 7.0},
    TestID -> "einx/max/row"
]

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[
        {{1.0, 5.0, 3.0}, {7.0, 2.0, 6.0}},
        "Real32"];
    Normal @ TTensorData @ TRealize @ TEinMin["b [s] -> b", a],
    {1.0, 2.0},
    TestID -> "einx/min/row"
]

(* === Dot ========================================================= *)

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[
        {{1.0, 2.0}, {3.0, 4.0}},
        "Real32"];
    b = TTensorCreate @ NumericArray[
        {{5.0, 6.0}, {7.0, 8.0}},
        "Real32"];
    Normal @ TTensorData @ TRealize @ TEinDot["i j, j k -> i k", a, b],
    {{19.0, 22.0}, {43.0, 50.0}},
    TestID -> "einx/dot/matmul"
]

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[
        {1.0, 2.0, 3.0},
        "Real32"];
    b = TTensorCreate @ NumericArray[
        {4.0, 5.0, 6.0},
        "Real32"];
    Normal @ TTensorData @ TRealize @ TEinDot["i, i ->", a, b],
    {32.0},
    TestID -> "einx/dot/inner"
]

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[
        Table[ N[1.0 + i + 10 j + 100 k], {k, 0, 1}, {j, 0, 1}, {i, 0, 2}],
        "Real32"];
    b = TTensorCreate @ NumericArray[
        Table[ N[1.0 + i + 10 j], {j, 0, 2}, {i, 0, 3}],
        "Real32"];
    (* Compare batched dot to plain Mathematica matmul. *)
    Normal @ TTensorData @ TRealize @ TEinDot["b s d, d e -> b s e", a, b],
    Normal[NumericArray[
        Map[#.Normal[NumericArray[
            Table[ N[1.0 + i + 10 j], {j, 0, 2}, {i, 0, 3}],
            "Real32"]] &,
            Table[ N[1.0 + i + 10 j + 100 k], {k, 0, 1}, {j, 0, 1}, {i, 0, 2}]],
        "Real32"]],
    TestID -> "einx/dot/batched"
]

(* === Add / Mul broadcasting ====================================== *)

VerificationTest[
    TInit[];
    x = TTensorCreate @ NumericArray[
        {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}},
        "Real32"];
    bias = TTensorCreate @ NumericArray[
        {10.0, 20.0, 30.0},
        "Real32"];
    Normal @ TTensorData @ TRealize @ TEinAdd["b d, d -> b d", x, bias],
    {{11.0, 22.0, 33.0}, {14.0, 25.0, 36.0}},
    TestID -> "einx/add/bias"
]

VerificationTest[
    TInit[];
    x = TTensorCreate @ NumericArray[
        {{1.0, 2.0}, {3.0, 4.0}},
        "Real32"];
    s = TTensorCreate @ NumericArray[
        {10.0, 20.0},
        "Real32"];
    Normal @ TTensorData @ TRealize @ TEinMul["b d, b -> b d", x, s],
    {{10.0, 20.0}, {60.0, 80.0}},
    TestID -> "einx/mul/perRow"
]

VerificationTest[
    TInit[];
    x = TTensorCreate @ NumericArray[
        {{10.0, 20.0}, {30.0, 40.0}},
        "Real32"];
    y = TTensorCreate @ NumericArray[
        {{1.0, 2.0}, {3.0, 4.0}},
        "Real32"];
    Normal @ TTensorData @ TRealize @ TEinSub["b d, b d -> b d", x, y],
    {{9.0, 18.0}, {27.0, 36.0}},
    TestID -> "einx/sub/elem"
]

VerificationTest[
    TInit[];
    x = TTensorCreate @ NumericArray[
        {{10.0, 20.0}, {30.0, 40.0}},
        "Real32"];
    y = TTensorCreate @ NumericArray[
        {{2.0, 4.0}, {5.0, 8.0}},
        "Real32"];
    Normal @ TTensorData @ TRealize @ TEinDiv["b d, b d -> b d", x, y],
    {{5.0, 5.0}, {6.0, 5.0}},
    TestID -> "einx/div/elem"
]

(* === Softmax vs TSoftmax ===================================== *)

VerificationTest[
    TInit[];
    x = TTensorCreate @ NumericArray[
        {{1.0, 2.0, 3.0}, {0.5, 0.0, -0.5}},
        "Real32"];
    With[{
        viaEinx = Normal @ TTensorData @ TRealize @ TEinSoftmax["b [d]", x],
        viaAxis = Normal @ TTensorData @ TRealize @ TSoftmax[x, 1]
    },
        Max @ Flatten @ Abs[viaEinx - viaAxis] < 1.0*^-5
    ],
    True,
    TestID -> "einx/softmax/matchesTSoftmax"
]

(* === LayerNorm =================================================== *)

VerificationTest[
    TInit[];
    x = TTensorCreate @ NumericArray[
        {{1.0, 2.0, 3.0, 4.0}, {0.0, 1.0, 2.0, 3.0}},
        "Real32"];
    With[{
        viaEinx = Normal @ TTensorData @ TRealize @ TEinLayerNorm["b [d]", x],
        viaNN   = Normal @ TTensorData @ TRealize @ TLayerNorm[x]
    },
        Max @ Flatten @ Abs[viaEinx - viaNN] < 1.0*^-4
    ],
    True,
    TestID -> "einx/layernorm/matchesTLayerNorm"
]

(* === Flip ======================================================== *)

VerificationTest[
    TInit[];
    x = TTensorCreate @ NumericArray[
        {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}},
        "Real32"];
    Normal @ TTensorData @ TRealize @ TEinFlip["b [d]", x],
    {{3.0, 2.0, 1.0}, {6.0, 5.0, 4.0}},
    TestID -> "einx/flip/lastAxis"
]

(* === LogSumExp =================================================== *)

VerificationTest[
    TInit[];
    x = TTensorCreate @ NumericArray[
        {{0.0, 1.0, 2.0}, {-1.0, 0.0, 1.0}},
        "Real32"];
    With[{
        viaEinx = Normal @ TTensorData @ TRealize @ TEinLogSumExp["b [d] -> b", x],
        expected = {Log[Exp[0.0] + Exp[1.0] + Exp[2.0]],
                    Log[Exp[-1.0] + Exp[0.0] + Exp[1.0]]}
    },
        Max[Abs[viaEinx - expected]] < 1.0*^-4
    ],
    True,
    TestID -> "einx/logsumexp/row"
]

(* === Var ========================================================= *)

VerificationTest[
    TInit[];
    x = TTensorCreate @ NumericArray[
        {{1.0, 2.0, 3.0, 4.0}},
        "Real32"];
    With[{
        viaEinx = Normal @ TTensorData @ TRealize @ TEinVar["b [d] -> b", x],
        (* population variance of {1,2,3,4} = 1.25 *)
        expected = {1.25}
    },
        Max[Abs[viaEinx - expected]] < 1.0*^-5
    ],
    True,
    TestID -> "einx/var/row"
]

(* === GetAt vs TEmbeddingMatrix =================================== *)

VerificationTest[
    TInit[];
    table = TTensorCreate @ NumericArray[
        Table[ N[10 v + i], {v, 0, 4}, {i, 0, 2}],
        "Real32"];
    With[{
        viaEinx = Normal @ TTensorData @ TRealize @ TEinGetAt["[v] d, b -> b d", table, {0, 3, 1}],
        viaEmb  = Normal @ TTensorData @ TRealize @ TEmbeddingMatrix[table, {0, 3, 1}]
    },
        viaEinx === viaEmb
    ],
    True,
    TestID -> "einx/getat/matchesTEmbeddingMatrix"
]

(* === SetAt: scatter overwrite ==================================== *)

VerificationTest[
    TInit[];
    table = TTensorCreate @ NumericArray[
        {{0.0, 0.0}, {1.0, 1.0}, {2.0, 2.0}, {3.0, 3.0}},
        "Real32"];
    vals = TTensorCreate @ NumericArray[
        {{99.0, 99.0}, {88.0, 88.0}},
        "Real32"];
    Normal @ TTensorData @ TRealize @ TEinSetAt["[v] d, b -> [v] d", table, {0, 2}, vals],
    {{99.0, 99.0}, {1.0, 1.0}, {88.0, 88.0}, {3.0, 3.0}},
    TestID -> "einx/setat/overwriteRows"
]

(* === AddAt: scatter-add ========================================== *)

VerificationTest[
    TInit[];
    table = TTensorCreate @ NumericArray[
        {{1.0, 1.0}, {2.0, 2.0}, {3.0, 3.0}},
        "Real32"];
    vals = TTensorCreate @ NumericArray[
        {{10.0, 20.0}},
        "Real32"];
    Normal @ TTensorData @ TRealize @ TEinAddAt["[v] d, b -> [v] d", table, {1}, vals],
    {{1.0, 1.0}, {12.0, 22.0}, {3.0, 3.0}},
    TestID -> "einx/addat/oneRow"
]
