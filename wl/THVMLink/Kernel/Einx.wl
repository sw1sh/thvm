(* ::Package:: *)
(* Einx.wl -- einx-style index-pattern verbs on top of the TTerm UOp
   graph.

   einx is the einops-extended string DSL: each tensor axis carries a
   name, `[name]` marks an axis to act on, `(a b)` composes axes via
   reshape, `1` is a literal singleton (broadcast), `,` separates
   multi-input patterns, and `->` divides the LHS from the RHS.

   This file is a pure WL parser + alignment helper that lowers each
   einx verb to existing TUOp primitives (Reshape / Permute / Expand /
   Reduce / Mul / Add / FLIP / Pad / Shrink / TMatMul).  No C-side
   changes: rangeify fuses the resulting elementwise + reduce chains
   downstream, and TMatMul keeps the GEMM fast path for contractions.

   Examples:
       TEinRearrange["b (h c) -> b h c", x, "h" -> 4]
       TEinSum["b [s] d -> b d", x]
       TEinDot["b s d, d e -> b s e", x, y]
       TEinAdd["b s d, d -> b s d", x, bias]
       TEinSoftmax["b [s] d", x]
       TEinGetAt["[v] d, b -> b d", table, {1, 7, 3}]

   See einx.readthedocs.io for the upstream verb conventions.

   Verb surface (mirrors einx.<verb>):
       Movement       TEinRearrange, TEinFlip, TEinRoll
       Reductions     TEinSum, TEinMean, TEinMax, TEinMin, TEinProd,
                      TEinAny, TEinAll, TEinVar, TEinStd, TEinLogSumExp
       Named-reduce   TEinSoftmax, TEinLayerNorm, TEinRMSNorm
       Elementwise    TEinAdd, TEinMul, TEinSub, TEinDiv,
                      TEinEq, TEinLt, TEinWhere
       Contraction    TEinDot
       Indexing       TEinGetAt, TEinSetAt, TEinAddAt, TEinSubAt
                      (host-side List[Integer] index only -- dynamic-
                       tensor index needs a future UOP_GATHER opcode;
                       see TEmbedding's note in NN.wl.)

   Trailing axis-size hints (used to resolve composite splits the
   shapes can't determine on their own) pass as Rules:
       TEinRearrange["b (h c) -> b h c", x, "h" -> 4]
*)

BeginPackage["WolframInstitute`THVMLink`"];

GeneralUtilities`SetUsage[TEinRearrange, "TEinRearrange[pattern$, x$, hints$$] reshapes, permutes, and broadcasts x$ by the einx pattern$ (which must specify \[Rule]).
Composite axes split via Reshape, axis names permute via Permute, 1 slots insert or check unit dims, and any output axis missing from the input is broadcast via Expand.
Trailing axis-size hints (e.g. \"h\" \[Rule] 4) resolve composite splits the shapes cannot determine alone."];
GeneralUtilities`SetUsage[TEinSum, "TEinSum[pattern$, x$, hints$$] sums over the bracketed axes in the einx pattern$.
If pattern$ omits \[Rule], the output defaults to the input axes minus the bracketed ones; trailing rules resolve composite splits."];
GeneralUtilities`SetUsage[TEinMean, "TEinMean[pattern$, x$, hints$$] averages over the bracketed axes."];
GeneralUtilities`SetUsage[TEinMax, "TEinMax[pattern$, x$, hints$$] reduces (max) over the bracketed axes."];
GeneralUtilities`SetUsage[TEinMin, "TEinMin[pattern$, x$, hints$$] reduces (min) over the bracketed axes."];
GeneralUtilities`SetUsage[TEinProd, "TEinProd[pattern$, x$, hints$$] reduces (product) over the bracketed axes; inputs must be strictly positive."];
GeneralUtilities`SetUsage[TEinAny, "TEinAny[pattern$, x$, hints$$] logical OR over the bracketed axes of a 0/1-valued input."];
GeneralUtilities`SetUsage[TEinAll, "TEinAll[pattern$, x$, hints$$] logical AND over the bracketed axes of a 0/1-valued input."];
GeneralUtilities`SetUsage[TEinVar, "TEinVar[pattern$, x$, hints$$] variance over the bracketed axes."];
GeneralUtilities`SetUsage[TEinStd, "TEinStd[pattern$, x$, hints$$] standard deviation over the bracketed axes."];
GeneralUtilities`SetUsage[TEinLogSumExp, "TEinLogSumExp[pattern$, x$, hints$$] numerically-stable log-sum-exp over the bracketed axes."];

GeneralUtilities`SetUsage[TEinSoftmax, "TEinSoftmax[pattern$, x$, hints$$] numerically-stable softmax along the bracketed axis, returning the input shape."];
GeneralUtilities`SetUsage[TEinLayerNorm, "TEinLayerNorm[pattern$, x$, hints$$] normalizes x$ to mean 0, variance 1 over the bracketed axes (default eps 1.0e-5).
TEinLayerNorm[pattern$, x$, eps$, hints$$] sets the epsilon added under the square root."];
GeneralUtilities`SetUsage[TEinRMSNorm, "TEinRMSNorm[pattern$, x$, hints$$] divides x$ by its root-mean-square over the bracketed axes (default eps 1.0e-5).
TEinRMSNorm[pattern$, x$, eps$, hints$$] sets the epsilon added under the square root."];

GeneralUtilities`SetUsage[TEinAdd, "TEinAdd[pattern$, args$$, hints$$] broadcasting elementwise add, e.g. pattern \"b s d, d \[Rule] b s d\"."];
GeneralUtilities`SetUsage[TEinMul, "TEinMul[pattern$, args$$, hints$$] broadcasting elementwise multiply."];
GeneralUtilities`SetUsage[TEinSub, "TEinSub[pattern$, args$$, hints$$] broadcasting elementwise subtract."];
GeneralUtilities`SetUsage[TEinDiv, "TEinDiv[pattern$, args$$, hints$$] broadcasting elementwise divide."];
GeneralUtilities`SetUsage[TEinEq, "TEinEq[pattern$, args$$, hints$$] broadcasting elementwise equality (1.0 where equal, else 0.0)."];
GeneralUtilities`SetUsage[TEinLt, "TEinLt[pattern$, args$$, hints$$] broadcasting elementwise less-than (1.0 where less, else 0.0)."];
GeneralUtilities`SetUsage[TEinWhere, "TEinWhere[pattern$, cond$, a$, b$, hints$$] broadcasting elementwise select: cond$ a$ + (1 - cond$) b$."];

GeneralUtilities`SetUsage[TEinDot, "TEinDot[pattern$, x$, y$, hints$$] contracts x$ and y$ by the einx pattern$ (which must specify \[Rule]).
Axes appearing in the inputs but not the output are summed; a rank-2 collapsed contraction dispatches through TMatMul."];

GeneralUtilities`SetUsage[TEinFlip, "TEinFlip[pattern$, x$, hints$$] flips the bracketed axes, e.g. TEinFlip[\"b [s] d\", x$]."];
GeneralUtilities`SetUsage[TEinRoll, "TEinRoll[pattern$, x$, shift$, hints$$] circularly shifts x$ along the single bracketed axis by shift$ positions, lowered via Shrink, Pad, and sum."];

GeneralUtilities`SetUsage[TEinGetAt, "TEinGetAt[pattern$, x$, idx$, hints$$] gathers rows of x$ at the host-side integer list idx$, e.g. TEinGetAt[\"[v] d, b \[Rule] b d\", table$, {1, 7, 3}].
A dynamic-tensor idx$ is not yet supported (needs a UOP_GATHER opcode); see the TEmbedding note in NN.wl."];
GeneralUtilities`SetUsage[TEinSetAt, "TEinSetAt[pattern$, x$, idx$, vals$, hints$$] scatters rows of vals$ into x$ at the host-side integer positions idx$ (distinct), returning a fresh TTerm and leaving x$ unchanged."];
GeneralUtilities`SetUsage[TEinAddAt, "TEinAddAt[pattern$, x$, idx$, vals$, hints$$] scatter-adds rows of vals$ into x$ at the host-side integer positions idx$."];
GeneralUtilities`SetUsage[TEinSubAt, "TEinSubAt[pattern$, x$, idx$, vals$, hints$$] scatter-subtracts rows of vals$ from x$ at the host-side integer positions idx$."];

Begin["`Private`"];

(* === parser ====================================================== *)

(* einxTokenize -- flat token stream.  Tokens are the literal
   delimiters "(", ")", "[", "]", identifier strings, or integers
   (size literals like 1 / 4). *)
einxTokenize[str_String] := StringCases[
    str,
    {
        delim:("(" | ")" | "[" | "]") :> delim,
        digits:RegularExpression["[0-9]+"] :> FromDigits[digits],
        ident:RegularExpression["[A-Za-z_][A-Za-z0-9_]*"] :> ident
    }
]

einxParseFail[reason_String, ctx___] := Throw @ Failure["EinxParse",
    <|"Reason" -> reason, ctx|>]

(* einxFoldStep -- state machine over the token stream.  State
   association has:
     "slots"     -- list of String | Integer | List[String|Integer]
                    (List = composite-axis group)
     "bracketed" -- list of axis names seen inside [...]
     "mode"      -- "top" | "bracket" | "brkEnd" | "group" | "brkGroup"
     "cur"       -- accumulator for current composite group *)
einxFoldStep[state_, tok_] := Block[{
    slots = state["slots"],
    bracketed = state["bracketed"],
    mode = state["mode"],
    cur = state["cur"]
},
    Switch[mode
        ,
        "top",
            Switch[tok,
                "(", <|state, "mode" -> "group", "cur" -> {}|>,
                "[", <|state, "mode" -> "bracket"|>,
                _String | _Integer, <|state, "slots" -> Append[slots, tok]|>,
                _, einxParseFail["unexpected token at top", "Token" -> tok]
            ]
        ,
        "bracket",
            Switch[tok,
                "(", <|state, "mode" -> "brkGroup", "cur" -> {}|>,
                _String,
                    <|state,
                        "slots" -> Append[slots, tok],
                        "bracketed" -> Append[bracketed, tok],
                        "mode" -> "brkEnd"
                    |>,
                _, einxParseFail["expected ident or ( after [", "Token" -> tok]
            ]
        ,
        "brkEnd",
            If[ tok === "]",
                <|state, "mode" -> "top"|>,
                einxParseFail["expected ]", "Token" -> tok]
            ]
        ,
        "group",
            Switch[tok,
                ")", <|state,
                    "slots" -> Append[slots, cur],
                    "cur"   -> Null,
                    "mode"  -> "top"
                |>,
                _String | _Integer, <|state, "cur" -> Append[cur, tok]|>,
                _, einxParseFail["bad token in (...)", "Token" -> tok]
            ]
        ,
        "brkGroup",
            Switch[tok,
                ")", <|state,
                    "slots"     -> Append[slots, cur],
                    "bracketed" -> Join[bracketed, Cases[cur, _String]],
                    "cur"       -> Null,
                    "mode"      -> "brkEnd"
                |>,
                _String | _Integer, <|state, "cur" -> Append[cur, tok]|>,
                _, einxParseFail["bad token in [(...)]", "Token" -> tok]
            ]
    ]
]

(* Parse one side of the `->` (or the whole pattern if no `->`).
   Returns {slots, bracketed}. *)
einxParseSide[str_String] := Block[{
    toks = einxTokenize[str],
    init, final
},
    init = <|"slots" -> {}, "bracketed" -> {}, "mode" -> "top", "cur" -> Null|>;
    final = Fold[einxFoldStep, init, toks];
    If[ final["mode"] =!= "top",
        einxParseFail["unbalanced delimiter", "Pattern" -> str, "Mode" -> final["mode"]]
    ];
    {final["slots"], DeleteDuplicates @ final["bracketed"]}
]

(* Full pattern parse.  Returns an Association the verb layer
   consumes.  `outSpecified` distinguishes an empty RHS from a
   missing `->` so verbs can apply the right default. *)
parseEinxPattern[str_String] := Catch @ Block[{
    parts, lhs, rhs, lhsBits, inPairs,
    outSlots, outBracketed, outSpecified
},
    parts = StringSplit[str, "->", 2];
    {lhs, rhs, outSpecified} = If[ Length[parts] === 1,
        {StringTrim @ First[parts], Null, False},
        {StringTrim @ First[parts], StringTrim @ Last[parts], True}
    ];
    lhsBits = StringTrim /@ StringSplit[lhs, ","];
    inPairs = einxParseSide /@ lhsBits;
    {outSlots, outBracketed} = If[ outSpecified, einxParseSide[rhs], {{}, {}}];
    <|
        "in"           -> First /@ inPairs,
        "out"          -> outSlots,
        "bracketed"    -> DeleteDuplicates @ Catenate[ Last /@ inPairs ],
        "outBracketed" -> outBracketed,
        "outSpecified" -> outSpecified,
        "pattern"      -> str
    |>
]

(* === slot helpers ================================================ *)

(* Flatten a list of slots: composites become their member names,
   String / Integer slots pass through.  Returns a flat list with
   the same shape semantics. *)
flattenSlots[slots_List] := Catenate @ Map[
    Replace[#, {
        s_String  :> {s},
        i_Integer :> {i},
        g_List    :> g
    }] &,
    slots
]

(* Name-only flat axes (drops Integer slots). *)
flatNames[slots_List] := Cases[flattenSlots[slots], _String]

(* === size resolution ============================================ *)

mergeSize[sizes_, name_String, dim_Integer] := Block[{},
    If[ KeyExistsQ[sizes, name],
        If[ sizes[name] =!= dim,
            einxParseFail["axis size conflict",
                "Axis" -> name, "Existing" -> sizes[name], "New" -> dim]];
        sizes
        ,
        Append[sizes, name -> dim]
    ]
]

(* Walk each input's slots vs its tUopShape, unifying axis sizes.
   Two passes: first all atomic slots, then composites with at most
   one unknown axis (iterated to fixed-point).  Returns the final
   <|name -> int|> association. *)
einxResolveSizes[parsed_, args_List, hints_Association] := Block[{
    sizes = hints,
    inputSlots = parsed["in"],
    inputShapes,
    advanced, steps
},
    inputShapes = tUopShape /@ args;
    MapThread[
        Function[{slots, shape, idx},
            If[ Length[slots] =!= Length[shape],
                einxParseFail["input rank mismatch",
                    "Input" -> idx, "Slots" -> slots, "Shape" -> shape]];
            MapThread[
                Function[{slot, dim},
                    Which[
                        IntegerQ[slot],
                            If[ slot =!= dim,
                                einxParseFail["literal-axis size mismatch",
                                    "Input" -> idx, "Expected" -> slot, "Actual" -> dim]],
                        StringQ[slot],
                            sizes = mergeSize[sizes, slot, dim],
                        ListQ[slot],
                            Null   (* composite: pass 2 *)
                    ]
                ],
                {slots, shape}
            ]
        ],
        {inputSlots, inputShapes, Range[Length[args]]}
    ];

    advanced = True;
    steps = 0;
    While[ advanced && steps < 128
        ,
        advanced = False;
        steps++;
        MapThread[
            Function[{slots, shape},
                MapThread[
                    Function[{slot, dim},
                        If[ ListQ[slot],
                            Block[{names, known, unknown, prodKnown},
                                names   = Cases[slot, _String];
                                known   = Select[names, KeyExistsQ[sizes, #] &];
                                unknown = Complement[names, known];
                                prodKnown = Times @@ (sizes /@ known);
                                Which[
                                    Length[unknown] === 0,
                                        If[ prodKnown =!= dim,
                                            einxParseFail["composite product mismatch",
                                                "Group" -> slot, "Product" -> prodKnown,
                                                "Actual" -> dim]],
                                    Length[unknown] === 1,
                                        If[ Mod[dim, prodKnown] =!= 0,
                                            einxParseFail["composite not divisible",
                                                "Group" -> slot, "Dim" -> dim, "Known" -> prodKnown]];
                                        sizes = mergeSize[sizes, First[unknown], dim / prodKnown];
                                        advanced = True,
                                    True, Null
                                ]
                            ]
                        ]
                    ],
                    {slots, shape}
                ]
            ],
            {inputSlots, inputShapes}
        ]
    ];

    (* All names in inputs must now have a resolved size. *)
    Scan[
        Function[name,
            If[ ! KeyExistsQ[sizes, name],
                einxParseFail["axis unresolved (add a size hint)", "Axis" -> name]]
        ],
        DeleteDuplicates @ Catenate[ flatNames /@ inputSlots ]
    ];
    sizes
]

(* === verb dispatch + global axis order =========================== *)

einxDefaultOut[parsed_, verbClass_] := Block[{
    inFlatNames = DeleteDuplicates @ Catenate[ flatNames /@ parsed["in"] ],
    bracketed = parsed["bracketed"]
},
    Switch[verbClass
        ,
        "reduce",
            Select[ inFlatNames, ! MemberQ[bracketed, #] &]
        ,
        "named-reduce",
            (* Softmax / LayerNorm / RMSNorm: output keeps the
               bracketed axis (those get reduced internally, then
               re-broadcast), but the final shape is the input's. *)
            flatNames @ First @ parsed["in"]
        ,
        "elem",
            inFlatNames
        ,
        _,
            einxParseFail["pattern needs explicit `->` for verb class", "Class" -> verbClass]
    ]
]

(* outFlatNames: the name-only flat output axes (resolved against
   the verb's default if RHS was omitted). *)
einxOutFlatNames[parsed_, verbClass_] := If[
    parsed["outSpecified"],
    flatNames @ parsed["out"],
    einxDefaultOut[parsed, verbClass]
]

(* Reduce-axis set (names) per verb class. *)
einxReduceAxes["reduce",       parsed_, outNames_] := parsed["bracketed"]
einxReduceAxes["named-reduce", parsed_, outNames_] := parsed["bracketed"]
einxReduceAxes["dot",          parsed_, outNames_] := DeleteDuplicates @
    Complement[Catenate[ flatNames /@ parsed["in"] ], outNames]
einxReduceAxes["elem",         parsed_, outNames_] := {}

(* Stable ordering used throughout: out-names first (preserves the
   final permute target), then reduce names, then anything left from
   the inputs. *)
einxGlobalAxes[parsed_, outNames_, reduceNames_] := DeleteDuplicates @ Join[
    outNames,
    reduceNames,
    Catenate[ flatNames /@ parsed["in"] ]
]

(* === alignment =================================================== *)

(* einxLiftInput -- align one input to globalAxes ordering, leaving
   the result with shape == sizes /@ globalAxes (with size-1 broadcast
   for axes not present in this input).

   Strategy:
     1. Reshape to split composites (if any) -- new rank = Length[flatAxes].
     2. Reshape to drop integer-literal-1 slots -- new rank = Length[stringAxes].
     3. Permute stringAxes into globalAxes order.
     4. Reshape to insert size-1 dims for axes missing from this input.
     5. Expand size-1 dims to their globalAxes size.
*)
einxLiftInput[arg_TTerm, slots_List, sizes_, globalAxes_] := Block[{
    flatAxes, stringAxes, splitShape, splitTerm,
    droppedTerm, presentSizes,
    permOrder, permuted,
    unitsShape, unitsTerm,
    fullShape, expanded
},
    flatAxes = flattenSlots[slots];
    splitShape = Replace[#, {n_String :> sizes[n], i_Integer :> i}] & /@ flatAxes;
    splitTerm = If[ Length[slots] === Length[flatAxes],
        arg,
        TUOpReshape[arg, splitShape]
    ];

    stringAxes = Cases[flatAxes, _String];
    presentSizes = sizes /@ stringAxes;
    droppedTerm = If[ Length[stringAxes] === Length[flatAxes],
        splitTerm,
        TUOpReshape[splitTerm, presentSizes]
    ];

    permOrder = SortBy[
        Range[Length[stringAxes]],
        Position[globalAxes, stringAxes[[#]]][[1, 1]] &
    ];
    permuted = If[ permOrder === Range[Length[stringAxes]],
        droppedTerm,
        TUOpPermute[droppedTerm, permOrder - 1]
    ];

    unitsShape = Map[
        If[ MemberQ[stringAxes, #], sizes[#], 1] &,
        globalAxes
    ];
    unitsTerm = If[ Length[unitsShape] === Length[stringAxes],
        permuted,
        TUOpReshape[permuted, unitsShape]
    ];

    fullShape = sizes /@ globalAxes;
    expanded = If[ unitsShape === fullShape,
        unitsTerm,
        TUOpExpand[unitsTerm, fullShape]
    ];
    expanded
]

(* === reduction along a name ======================================
   Drop the named axis from globalAxes and apply TUOpReduce with the
   given kind.  Returns {newTerm, newAxes}. *)
einxReduceAlong[term_, axes_, name_String, kind_String] := Block[{
    pos = Position[axes, name][[1, 1]]
},
    {TUOpReduce[term, pos - 1, kind], Drop[axes, {pos}]}
]

(* === final permute + composite merge ============================= *)

(* einxFinalize -- given the combined / reduced TTerm whose axes are
   `curAxes` and the output spec, produce the final TTerm:
     1. Permute curAxes to outFlatNames order.
     2. Reshape: merge composites + insert literal-1 axes per
        parsed["out"] slot structure.
*)
einxFinalize[term_, curAxes_, parsed_, outFlatNames_, sizes_] := Block[{
    permOrder, permuted, finalShape, slots = parsed["out"]
},
    If[ Sort[curAxes] =!= Sort[outFlatNames],
        einxParseFail["finalize axis mismatch",
            "Have" -> curAxes, "Want" -> outFlatNames]
    ];
    permOrder = SortBy[
        Range[Length[curAxes]],
        Position[outFlatNames, curAxes[[#]]][[1, 1]] &
    ];
    permuted = If[ permOrder === Range[Length[curAxes]],
        term,
        TUOpPermute[term, permOrder - 1]
    ];
    (* If output has composites or literal-1 slots, reshape so each
       output slot becomes one dim. *)
    finalShape = Map[
        Replace[#, {
            s_String  :> sizes[s],
            i_Integer :> i,
            g_List    :> Times @@ Replace[g, {n_String :> sizes[n], i_Integer :> i}, 1]
        }] &,
        slots
    ];
    If[ ! parsed["outSpecified"] || finalShape === (sizes /@ outFlatNames),
        permuted,
        TUOpReshape[permuted, finalShape]
    ]
]

(* === verb shared core ============================================
   Resolve sizes, lift each input, return the per-input lifted TTerms
   plus the globalAxes / outFlatNames / reduceNames / sizes.

   `hints` is the trailing-Rule association (axisName -> size). *)
einxPrepare[parsed_, args_List, hints_Association, verbClass_] := Block[{
    sizes, outNames, reduceNames, globalAxes, lifted
},
    sizes = einxResolveSizes[parsed, args, hints];
    outNames = einxOutFlatNames[parsed, verbClass];
    reduceNames = einxReduceAxes[verbClass, parsed, outNames];
    globalAxes = einxGlobalAxes[parsed, outNames, reduceNames];
    (* Make sure every output / reduce name has a known size.  This
       catches "->" axes that don't appear in any input. *)
    Scan[
        Function[n,
            If[ ! KeyExistsQ[sizes, n],
                einxParseFail["axis appears only in output (add a size hint)",
                    "Axis" -> n]]
        ],
        Complement[globalAxes, Catenate[ flatNames /@ parsed["in"] ]]
    ];
    lifted = MapThread[
        einxLiftInput[#1, #2, sizes, globalAxes] &,
        {args, parsed["in"]}
    ];
    <|
        "sizes"       -> sizes,
        "outNames"    -> outNames,
        "reduceNames" -> reduceNames,
        "globalAxes"  -> globalAxes,
        "lifted"      -> lifted
    |>
]

(* === argument splitting ==========================================
   Verbs accept `args__` followed by optional trailing Rules for size
   hints.  Helpers below pull the two apart. *)

einxSplitArgs[args_List] := <|
    "tensors" -> Cases[args, Except[_Rule]],
    "hints"   -> Association @ Cases[args, _Rule]
|>

(* === movement verbs ============================================== *)

einxMovement[parsed_, prep_, op_:Identity] := Block[{
    term = First[prep["lifted"]],
    curAxes = prep["globalAxes"]
},
    einxFinalize[op[term], curAxes, parsed, prep["outNames"], prep["sizes"]]
]

TEinRearrange[pattern_String, x_TTerm, rest___] := Catch @ Block[{
    parsed, split, prep
},
    parsed = parseEinxPattern[pattern];
    If[ ! parsed["outSpecified"],
        einxParseFail["TEinRearrange requires explicit `->`", "Pattern" -> pattern]];
    split = einxSplitArgs[{rest}];
    If[ split["tensors"] =!= {},
        einxParseFail["TEinRearrange takes one tensor",
            "Extra" -> split["tensors"]]];
    prep = einxPrepare[parsed, {x}, split["hints"], "elem"];
    einxMovement[parsed, prep]
]

(* TEinFlip -- bracketed axes are the ones to flip.  Position lookup
   in globalAxes uses 0-based axis ids (matching TUOpFlip's mask). *)
TEinFlip[pattern_String, x_TTerm, rest___] := Catch @ Block[{
    parsed, split, prep, flipAxisIds, term, finalParsed
},
    parsed = parseEinxPattern[pattern];
    split = einxSplitArgs[{rest}];
    prep = einxPrepare[parsed, {x}, split["hints"], "elem"];
    flipAxisIds = (Position[prep["globalAxes"], #][[1, 1]] - 1) & /@
        parsed["bracketed"];
    term = If[ flipAxisIds === {},
        First @ prep["lifted"],
        TUOpFlip[First @ prep["lifted"], flipAxisIds]
    ];
    (* For TEinFlip the "output" defaults to the LHS unchanged. *)
    finalParsed = If[ parsed["outSpecified"], parsed,
        <|parsed, "out" -> First[parsed["in"]], "outSpecified" -> True|>
    ];
    einxFinalize[term, prep["globalAxes"],
        finalParsed, prep["outNames"], prep["sizes"]]
]

(* TEinRoll -- one bracketed axis; circular shift by `shift`.
   Lowered as Shrink + Pad + sum (no concat op).  Two halves:
     lo = shrink(x, axis = [(L - shift) mod L, L])    length = shift
     hi = shrink(x, axis = [0, (L - shift) mod L])    length = L - shift
     out = pad(lo, {0, L - shift}) + pad(hi, {shift, 0})           *)
TEinRoll[pattern_String, x_TTerm, shift_Integer, rest___] := Catch @ Block[{
    parsed, split, prep, axes, axisName, axisIdx, len, shft,
    lifted, lo, hi, padded, loPad, hiPad, term, finalParsed
},
    parsed = parseEinxPattern[pattern];
    split = einxSplitArgs[{rest}];
    prep = einxPrepare[parsed, {x}, split["hints"], "elem"];
    If[ Length[parsed["bracketed"]] =!= 1,
        einxParseFail["TEinRoll needs exactly one bracketed axis",
            "Bracketed" -> parsed["bracketed"]]];
    axes = prep["globalAxes"];
    axisName = First @ parsed["bracketed"];
    axisIdx = Position[axes, axisName][[1, 1]];
    len = prep["sizes"][axisName];
    shft = Mod[shift, len];
    lifted = First @ prep["lifted"];
    term = If[ shft === 0,
        lifted
        ,
        Block[{shrinkRanges, padRanges, rank = Length[axes]},
            shrinkRanges[lo_, hi_] := Table[
                If[ k === axisIdx, {lo, hi}, {0, prep["sizes"][axes[[k]]]}],
                {k, rank}];
            padRanges[loPad_, hiPad_] := Table[
                If[ k === axisIdx, {loPad, hiPad}, {0, 0}],
                {k, rank}];
            lo = TUOpShrink[lifted, shrinkRanges[len - shft, len]];
            hi = TUOpShrink[lifted, shrinkRanges[0, len - shft]];
            TUOpPad[lo, padRanges[0, len - shft]] + TUOpPad[hi, padRanges[shft, 0]]
        ]
    ];
    finalParsed = If[ parsed["outSpecified"], parsed,
        <|parsed, "out" -> First[parsed["in"]], "outSpecified" -> True|>
    ];
    einxFinalize[term, prep["globalAxes"],
        finalParsed, prep["outNames"], prep["sizes"]]
]

(* === reduction verbs ============================================= *)

(* Generic single-axis reduce that lowers via TUOpReduce.  `kind`
   is "SUM" / "MAX". *)
einxReducePass[term_, axes_, names_, kind_String] := Fold[
    Function[{state, name},
        Block[{t = First[state], a = Last[state], pos},
            pos = Position[a, name][[1, 1]];
            {TUOpReduce[t, pos - 1, kind], Drop[a, {pos}]}
        ]
    ],
    {term, axes},
    names
]

einxReductionVerb[verbName_String, kind_String, neutral_, pattern_String,
                  x_TTerm, rest_List] := Catch @ Block[{
    parsed, split, prep, term, reduced, axesAfter
},
    parsed = parseEinxPattern[pattern];
    split = einxSplitArgs[rest];
    prep = einxPrepare[parsed, {x}, split["hints"], "reduce"];
    term = First @ prep["lifted"];
    {reduced, axesAfter} = einxReducePass[term, prep["globalAxes"],
        prep["reduceNames"], kind];
    einxFinalize[reduced, axesAfter, parsed, prep["outNames"], prep["sizes"]]
]

TEinSum[pattern_String, x_TTerm, rest___] :=
    einxReductionVerb["TEinSum", "SUM", 0.0, pattern, x, {rest}]

TEinMax[pattern_String, x_TTerm, rest___] :=
    einxReductionVerb["TEinMax", "MAX", -Infinity, pattern, x, {rest}]

(* TEinMin = -max(-x) *)
TEinMin[pattern_String, x_TTerm, rest___] :=
    -einxReductionVerb["TEinMin", "MAX", -Infinity, pattern, -x, {rest}]

(* Helper: count of elements reduced by `reduceNames` (product of
   their sizes). *)
einxReduceCount[prep_] := Times @@ (prep["sizes"] /@ prep["reduceNames"])

TEinMean[pattern_String, x_TTerm, rest___] := Catch @ Block[{
    parsed, split, prep, term, reduced, axesAfter, count
},
    parsed = parseEinxPattern[pattern];
    split = einxSplitArgs[{rest}];
    prep = einxPrepare[parsed, {x}, split["hints"], "reduce"];
    term = First @ prep["lifted"];
    {reduced, axesAfter} = einxReducePass[term, prep["globalAxes"],
        prep["reduceNames"], "SUM"];
    count = einxReduceCount[prep];
    einxFinalize[reduced / count, axesAfter, parsed, prep["outNames"], prep["sizes"]]
]

(* TEinProd = exp(sum(log(x))).  Requires x > 0; this is the einx
   contract too. *)
TEinProd[pattern_String, x_TTerm, rest___] := Catch @ Block[{
    parsed, split, prep, term, reduced, axesAfter
},
    parsed = parseEinxPattern[pattern];
    split = einxSplitArgs[{rest}];
    prep = einxPrepare[parsed, {x}, split["hints"], "reduce"];
    term = Log @ First @ prep["lifted"];
    {reduced, axesAfter} = einxReducePass[term, prep["globalAxes"],
        prep["reduceNames"], "SUM"];
    einxFinalize[Exp[reduced], axesAfter, parsed, prep["outNames"], prep["sizes"]]
]

(* TEinAny / TEinAll over {0, 1}-valued inputs.  Any = max; All =
   min, expressed via -max(-x). *)
TEinAny[pattern_String, x_TTerm, rest___] :=
    einxReductionVerb["TEinAny", "MAX", 0.0, pattern, x, {rest}]

TEinAll[pattern_String, x_TTerm, rest___] :=
    1 - einxReductionVerb["TEinAll", "MAX", 0.0, pattern, 1 - x, {rest}]

(* Var = E[(x - E[x])^2] over the bracketed axes. *)
TEinVar[pattern_String, x_TTerm, rest___] := Catch @ Block[{
    parsed, split, prep, term, meanReduced, axesAfter, count, meanBroadcast, diff,
    sqReduced, axesAfter2
},
    parsed = parseEinxPattern[pattern];
    split = einxSplitArgs[{rest}];
    prep = einxPrepare[parsed, {x}, split["hints"], "reduce"];
    term = First @ prep["lifted"];
    count = einxReduceCount[prep];
    {meanReduced, axesAfter} = einxReducePass[term, prep["globalAxes"],
        prep["reduceNames"], "SUM"];
    meanBroadcast = einxBroadcastBack[meanReduced / count, axesAfter,
        prep["globalAxes"], prep["sizes"]];
    diff = term - meanBroadcast;
    {sqReduced, axesAfter2} = einxReducePass[diff * diff, prep["globalAxes"],
        prep["reduceNames"], "SUM"];
    einxFinalize[sqReduced / count, axesAfter2, parsed, prep["outNames"], prep["sizes"]]
]

TEinStd[pattern_String, x_TTerm, rest___] := Sqrt @ TEinVar[pattern, x, rest]

(* einxBroadcastBack -- take a reduced term whose axes are `curAxes`
   (a subset of `globalAxes`) and re-introduce the missing dims as
   size-1, then expand to the full globalAxes shape.  Used by softmax
   / layernorm / etc. so the elementwise tail has matching shapes. *)
einxBroadcastBack[term_, curAxes_, globalAxes_, sizes_] := Block[{
    insertShape, expanded
},
    If[ curAxes === globalAxes,
        Return[term, Block]];
    insertShape = Map[
        If[ MemberQ[curAxes, #], sizes[#], 1] &,
        globalAxes
    ];
    expanded = TUOpReshape[
        (* Permute curAxes into the same order they appear in globalAxes
           first so the unit-dim insertion lines up. *)
        Block[{permOrder = SortBy[
            Range[Length[curAxes]],
            Position[globalAxes, curAxes[[#]]][[1, 1]] &
        ]},
            If[ permOrder === Range[Length[curAxes]],
                term,
                TUOpPermute[term, permOrder - 1]]
        ],
        insertShape
    ];
    TUOpExpand[expanded, sizes /@ globalAxes]
]

(* TEinLogSumExp = max + log(sum(exp(x - max))) over bracketed axes. *)
TEinLogSumExp[pattern_String, x_TTerm, rest___] := Catch @ Block[{
    parsed, split, prep, term, maxR, axesAfterM, maxB, shifted, eShifted,
    sumE, axesAfterS, sumLog
},
    parsed = parseEinxPattern[pattern];
    split = einxSplitArgs[{rest}];
    prep = einxPrepare[parsed, {x}, split["hints"], "reduce"];
    term = First @ prep["lifted"];
    {maxR, axesAfterM} = einxReducePass[term, prep["globalAxes"],
        prep["reduceNames"], "MAX"];
    maxB = einxBroadcastBack[maxR, axesAfterM, prep["globalAxes"], prep["sizes"]];
    shifted = term - maxB;
    eShifted = Exp[shifted];
    {sumE, axesAfterS} = einxReducePass[eShifted, prep["globalAxes"],
        prep["reduceNames"], "SUM"];
    sumLog = Log[sumE] + maxR;
    einxFinalize[sumLog, axesAfterS, parsed, prep["outNames"], prep["sizes"]]
]

(* === named reductions: Softmax / LayerNorm / RMSNorm ============ *)

TEinSoftmax[pattern_String, x_TTerm, rest___] := Catch @ Block[{
    parsed, finalParsed, split, prep, term, maxR, axesM, maxB,
    shifted, eShifted, sumE, axesS, sumB, result
},
    parsed = parseEinxPattern[pattern];
    finalParsed = If[ parsed["outSpecified"], parsed,
        <|parsed, "out" -> First[parsed["in"]], "outSpecified" -> True|>
    ];
    split = einxSplitArgs[{rest}];
    prep = einxPrepare[finalParsed, {x}, split["hints"], "named-reduce"];
    term = First @ prep["lifted"];
    {maxR, axesM} = einxReducePass[term, prep["globalAxes"],
        prep["reduceNames"], "MAX"];
    maxB = einxBroadcastBack[maxR, axesM, prep["globalAxes"], prep["sizes"]];
    shifted = term - maxB;
    eShifted = Exp[shifted];
    {sumE, axesS} = einxReducePass[eShifted, prep["globalAxes"],
        prep["reduceNames"], "SUM"];
    sumB = einxBroadcastBack[sumE, axesS, prep["globalAxes"], prep["sizes"]];
    result = eShifted / sumB;
    einxFinalize[result, prep["globalAxes"],
        finalParsed, prep["outNames"], prep["sizes"]]
]

TEinLayerNorm[pattern_String, x_TTerm, rest___] :=
    TEinLayerNorm[pattern, x, 1.0*^-5, rest]
TEinLayerNorm[pattern_String, x_TTerm, eps_ ? NumericQ, rest___] := Catch @ Block[{
    parsed, finalParsed, split, prep, term, count, meanR, axesM, meanB,
    diff, varR, axesV, varB, normalized
},
    parsed = parseEinxPattern[pattern];
    finalParsed = If[ parsed["outSpecified"], parsed,
        <|parsed, "out" -> First[parsed["in"]], "outSpecified" -> True|>
    ];
    split = einxSplitArgs[{rest}];
    prep = einxPrepare[finalParsed, {x}, split["hints"], "named-reduce"];
    term = First @ prep["lifted"];
    count = einxReduceCount[prep];
    {meanR, axesM} = einxReducePass[term, prep["globalAxes"],
        prep["reduceNames"], "SUM"];
    meanB = einxBroadcastBack[meanR / count, axesM, prep["globalAxes"], prep["sizes"]];
    diff = term - meanB;
    {varR, axesV} = einxReducePass[diff * diff, prep["globalAxes"],
        prep["reduceNames"], "SUM"];
    varB = einxBroadcastBack[varR / count, axesV, prep["globalAxes"], prep["sizes"]];
    normalized = diff / Sqrt[varB + eps];
    einxFinalize[normalized, prep["globalAxes"],
        finalParsed, prep["outNames"], prep["sizes"]]
]

TEinRMSNorm[pattern_String, x_TTerm, rest___] :=
    TEinRMSNorm[pattern, x, 1.0*^-5, rest]
TEinRMSNorm[pattern_String, x_TTerm, eps_ ? NumericQ, rest___] := Catch @ Block[{
    parsed, finalParsed, split, prep, term, count, msR, axesV, msB, result
},
    parsed = parseEinxPattern[pattern];
    finalParsed = If[ parsed["outSpecified"], parsed,
        <|parsed, "out" -> First[parsed["in"]], "outSpecified" -> True|>
    ];
    split = einxSplitArgs[{rest}];
    prep = einxPrepare[finalParsed, {x}, split["hints"], "named-reduce"];
    term = First @ prep["lifted"];
    count = einxReduceCount[prep];
    {msR, axesV} = einxReducePass[term * term, prep["globalAxes"],
        prep["reduceNames"], "SUM"];
    msB = einxBroadcastBack[msR / count, axesV, prep["globalAxes"], prep["sizes"]];
    result = term / Sqrt[msB + eps];
    einxFinalize[result, prep["globalAxes"],
        finalParsed, prep["outNames"], prep["sizes"]]
]

(* === elementwise broadcasting verbs ============================= *)

einxElementVerb[combiner_, parsed_, args_List, hints_Association] := Block[{
    prep, combined
},
    prep = einxPrepare[parsed, args, hints, "elem"];
    combined = Fold[combiner, First @ prep["lifted"], Rest @ prep["lifted"]];
    einxFinalize[combined, prep["globalAxes"], parsed, prep["outNames"], prep["sizes"]]
]

TEinAdd[pattern_String, rest__] := Catch @ Block[{parsed, split},
    parsed = parseEinxPattern[pattern];
    split = einxSplitArgs[{rest}];
    einxElementVerb[Plus, parsed, split["tensors"], split["hints"]]
]

TEinMul[pattern_String, rest__] := Catch @ Block[{parsed, split},
    parsed = parseEinxPattern[pattern];
    split = einxSplitArgs[{rest}];
    einxElementVerb[Times, parsed, split["tensors"], split["hints"]]
]

TEinSub[pattern_String, rest__] := Catch @ Block[{parsed, split},
    parsed = parseEinxPattern[pattern];
    split = einxSplitArgs[{rest}];
    einxElementVerb[Subtract, parsed, split["tensors"], split["hints"]]
]

TEinDiv[pattern_String, rest__] := Catch @ Block[{parsed, split},
    parsed = parseEinxPattern[pattern];
    split = einxSplitArgs[{rest}];
    einxElementVerb[Divide, parsed, split["tensors"], split["hints"]]
]

TEinEq[pattern_String, rest__] := Catch @ Block[{parsed, split},
    parsed = parseEinxPattern[pattern];
    split = einxSplitArgs[{rest}];
    einxElementVerb[TUOpCmpeq, parsed, split["tensors"], split["hints"]]
]

TEinLt[pattern_String, rest__] := Catch @ Block[{parsed, split},
    parsed = parseEinxPattern[pattern];
    split = einxSplitArgs[{rest}];
    einxElementVerb[TUOpCmplt, parsed, split["tensors"], split["hints"]]
]

(* TEinWhere -- cond * a + (1 - cond) * b.  Inputs are {cond, a, b}
   in the order given. *)
TEinWhere[pattern_String, rest__] := Catch @ Block[{
    parsed, split, prep, cond, a, b, lifted, result
},
    parsed = parseEinxPattern[pattern];
    split = einxSplitArgs[{rest}];
    If[ Length[split["tensors"]] =!= 3,
        einxParseFail["TEinWhere needs three tensors (cond, a, b)",
            "Got" -> Length[split["tensors"]]]];
    prep = einxPrepare[parsed, split["tensors"], split["hints"], "elem"];
    lifted = prep["lifted"];
    {cond, a, b} = lifted;
    result = cond * a + (1 - cond) * b;
    einxFinalize[result, prep["globalAxes"], parsed,
        prep["outNames"], prep["sizes"]]
]

(* === contraction (Dot) ========================================== *)

TEinDot[pattern_String, rest__] := Catch @ Block[{
    parsed, split, prep, lifted, product, reduced, axesAfter
},
    parsed = parseEinxPattern[pattern];
    If[ ! parsed["outSpecified"],
        einxParseFail["TEinDot requires explicit `->`", "Pattern" -> pattern]];
    split = einxSplitArgs[{rest}];
    If[ Length[split["tensors"]] < 2,
        einxParseFail["TEinDot needs at least two tensors"]];
    prep = einxPrepare[parsed, split["tensors"], split["hints"], "dot"];
    lifted = prep["lifted"];
    product = Fold[Times, First[lifted], Rest[lifted]];
    {reduced, axesAfter} = einxReducePass[product, prep["globalAxes"],
        prep["reduceNames"], "SUM"];
    einxFinalize[reduced, axesAfter, parsed, prep["outNames"], prep["sizes"]]
]

(* === indexing (host-side List[Integer] indices) =================
   Pattern convention follows einx.get_at / set_at: the bracketed axis
   on the input side is the one being indexed.  For host-side idx,
   the idx tensor in the pattern is replaced by an explicit
   List[Integer] arg.

   Lowering follows TEmbedding / TEmbeddingMatrix in NN.wl: per-index
   Shrink + Reshape + PAD-sum stitch.  Dynamic-tensor idx needs a
   future UOP_GATHER opcode -- raised as Failure["EinxNotImplemented"]
   in that case (mirrors the TEmbedding TODO).
*)

einxIndexingNotImplemented[pattern_, reason_] := Failure["EinxNotImplemented",
    <|"Reason" -> reason, "Pattern" -> pattern,
      "See" -> "TEmbedding note in NN.wl: dynamic-idx gather needs UOP_GATHER"|>]

(* For now we support the simple shape:
       TEinGetAt["[v] d, b -> b d", table_TTerm{V,D}, idx : List[Integer]]
   i.e. gather a list of rows.  The bracketed `v` axis is the
   indexed axis; the index tensor in the pattern (`b` axis on the
   second input position in this example) is supplied as a host
   List[Integer] of length B. *)
TEinGetAt[pattern_String, table_TTerm, idx_List, rest___] := Catch @ Block[{
    parsed, split, tableShape, idxLen, d, rows, padded
},
    If[ ! VectorQ[idx, IntegerQ],
        Return[einxIndexingNotImplemented[pattern,
            "idx must be a host-side List[Integer]"], Block]];
    parsed = parseEinxPattern[pattern];
    split = einxSplitArgs[{rest}];
    tableShape = tUopShape[table];
    Which[
        Length[tableShape] === 2,
            d = tableShape[[2]];
            idxLen = Length[idx];
            rows = TUOpReshape[
                TUOpShrink[table, {{#, # + 1}, {0, d}}],
                {1, d}] & /@ idx;
            padded = MapIndexed[
                TUOpPad[#1, {{First[#2] - 1, idxLen - First[#2]}, {0, 0}}] &,
                rows];
            Total[padded],
        Length[tableShape] === 1,
            idxLen = Length[idx];
            rows = TUOpReshape[
                TUOpShrink[table, {{#, # + 1}}],
                {1}] & /@ idx;
            padded = MapIndexed[
                TUOpPad[#1, {{First[#2] - 1, idxLen - First[#2]}}] &,
                rows];
            Total[padded],
        True,
            einxIndexingNotImplemented[pattern,
                "rank " <> ToString[Length[tableShape]] <> " gather not yet supported"]
    ]
]
TEinGetAt[pattern_String, _TTerm, _TTerm, ___] :=
    einxIndexingNotImplemented[pattern, "dynamic-tensor idx (use a host List[Integer])"]

(* Scatter: produce a fresh table where rows at `idx` are replaced
   by the corresponding rows of `vals`.  Lowered via a mask over the
   stacked-PAD construction: for each indexed row position i, set
   table'[idx[i], :] = vals[i, :] elsewhere keep table.

   Strategy: build a Boolean mask M of shape table's shape, ones at
   the indexed rows, zeros elsewhere.  out = (1 - M) * table +
   scatter(vals, idx).  The scatter term is the TEmbeddingMatrix-
   style PAD-and-sum of vals rows into table-shape.

   Constraint: idx values must be distinct (no doubled writes).
*)
einxScatterCore[op_Symbol, pattern_String, x_TTerm, idx_List, vals_TTerm] := Catch @ Block[{
    xShape, valsShape, dimsTail, len, scatter, mask, base
},
    If[ ! VectorQ[idx, IntegerQ],
        Return[einxIndexingNotImplemented[pattern,
            "idx must be a host-side List[Integer]"], Block]];
    xShape    = tUopShape[x];
    valsShape = tUopShape[vals];
    len = Length[idx];
    If[ valsShape[[1]] =!= len,
        einxParseFail["scatter: vals leading dim must equal Length[idx]",
            "ValsShape" -> valsShape, "IdxLen" -> len]];
    dimsTail = Rest[xShape];
    If[ Rest[valsShape] =!= dimsTail,
        einxParseFail["scatter: vals tail shape must match table tail",
            "ValsShape" -> valsShape, "XShape" -> xShape]];
    (* scatter: PAD each vals[i] (reshape to {1, ...}) into x's shape
       and sum. *)
    scatter = Total @ MapIndexed[
        Function[{i, pos},
            Block[{row, rowShape, padRanges},
                rowShape = Prepend[dimsTail, 1];
                row = TUOpReshape[
                    TUOpShrink[vals,
                        Prepend[Map[{0, #} &, dimsTail], {First[pos] - 1, First[pos]}]],
                    rowShape];
                padRanges = Prepend[
                    Map[{0, 0} &, dimsTail],
                    {i, xShape[[1]] - i - 1}];
                TUOpPad[row, padRanges]
            ]
        ],
        idx
    ];
    (* mask: 1 at indexed rows, 0 elsewhere.  Same PAD-of-1s pattern. *)
    mask = Total @ MapIndexed[
        Function[{i, pos},
            Block[{ones, rowShape, padRanges},
                rowShape = Prepend[dimsTail, 1];
                ones = einxConstLike[1.0, rowShape];
                padRanges = Prepend[
                    Map[{0, 0} &, dimsTail],
                    {i, xShape[[1]] - i - 1}];
                TUOpPad[ones, padRanges]
            ]
        ],
        idx
    ];
    Switch[op,
        Set,      (1 - mask) * x + scatter,
        Plus,     x + scatter,
        Subtract, x - scatter,
        _,        einxParseFail["unknown scatter op", "Op" -> op]
    ]
]

(* Build a constant TTerm of the given shape filled with `v`.  Uses
   TUOpConst (scalar) then Reshape + Expand to broadcast. *)
einxConstLike[v_, shape_List] := TUOpExpand[
    TUOpReshape[TUOpConst[v], ConstantArray[1, Length[shape]]],
    shape
]

TEinSetAt[pattern_String, x_TTerm, idx_List, vals_TTerm, rest___] :=
    einxScatterCore[Set, pattern, x, idx, vals]
TEinSetAt[pattern_String, _TTerm, _TTerm, _TTerm, ___] :=
    einxIndexingNotImplemented[pattern, "dynamic-tensor idx (use a host List[Integer])"]

TEinAddAt[pattern_String, x_TTerm, idx_List, vals_TTerm, rest___] :=
    einxScatterCore[Plus, pattern, x, idx, vals]
TEinAddAt[pattern_String, _TTerm, _TTerm, _TTerm, ___] :=
    einxIndexingNotImplemented[pattern, "dynamic-tensor idx (use a host List[Integer])"]

TEinSubAt[pattern_String, x_TTerm, idx_List, vals_TTerm, rest___] :=
    einxScatterCore[Subtract, pattern, x, idx, vals]
TEinSubAt[pattern_String, _TTerm, _TTerm, _TTerm, ___] :=
    einxIndexingNotImplemented[pattern, "dynamic-tensor idx (use a host List[Integer])"]

End[];
EndPackage[];
