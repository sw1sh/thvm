(* ::Package:: *)
(* AOT.wl -- WL surface for the ahead-of-time compiled definition path.
   See src/aot/_.c for the runtime contract.

   Usage today (hand-coded programs only):

       TInit[];
       TDef["add", ...]; TDef["fib", ...]; TDef["u32", ...];
       prog = TAOT["fib_nat", {"add", "fib", "u32"}];
       TWnf @ TApp[TApp[TRef["u32"], TApp[TRef["fib"], nat[25]]], TNum[0]]
       (* same answer as without TAOT, ~14x faster on fib(25) *)

   The TDef bodies are still required because the AOT falls back to
   the lazy ALO interpreter for any pattern it didn't compile a case
   for (partial application, unexpected tag, etc.).  Removing the
   TDef would break that deopt path.

   TAOT returns a TAOTProgram[<|...|>] wrapper that renders with a
   summary box (name, defs, slots, live call count) -- think of it
   as the AOT analogue of CompiledFunction.

   Future: TAOTCompile[<list of def names>] will run the auto-emitter
   in src/aot/emit.c (not yet shipped), produce specialised C, build
   it via clang, dlopen, and register the resulting function pointers
   automatically -- replacing the hand-coded `TAOT["fib_nat", ...]`
   call with `TAOTCompile[{"add", "fib", "u32"}]`. *)

BeginPackage["THVMLink`"];

(* Forward-declare symbols owned by later-loading siblings so
   references in AOT.wl resolve to THVMLink`Foo (not the
   THVMLink`Private`Foo placeholder Mathematica would otherwise
   manufacture).  Same trick as Format.wl. *)
{TDefName, TInit};

TAOT::usage = "TAOT[programName, defNames] enables a hand-coded AOT program.  `programName` selects the C-side `aot_program_<name>_register` function (currently only \"fib_nat\" is shipped); `defNames` is the ordered list of def-name strings the program expects -- their TDefName slots are passed verbatim to the registration call.  Returns a TAOTProgram[<|...|>] wrapper (renders as a summary box).  The matching defs must already be TDef'd; the AOT falls back to the interpreter for any pattern it doesn't recognise.";

TAOTProgram::usage = "TAOTProgram[<|name, defs, slots, registered, calls0|>] is the typed wrapper TAOT returns.  Indexable: prog[\"name\"], prog[\"defs\"], prog[\"slots\"], prog[\"registered\"], prog[\"calls\"] (live count since registration).  Renders as a summary box with the same fields.";

TAOTReset::usage = "TAOTReset[] clears every registered AOT function.  TInit / TReset already calls aot_reset() internally so this is a no-op against a fresh init -- expose for explicit teardown when swapping programs without a heap reset.";

TAOTCalls::usage = "TAOTCalls[] returns the cumulative count of WNF -> AOT dispatches (entries through the TAG_REF fast path) since the last TAOTReset / TInit.  Diagnostic only; the AOT does most of its work via direct C recursion that doesn't bump this counter.";

TAOTPrograms::usage = "TAOTPrograms[] returns the list of program names that ship a hand-coded AOT under src/aot/programs/.  Each can be registered via TAOT[name, defNames].";

TAOTCompile::usage = "TAOTCompile[name] runs the auto-emitter on the named TDef, writes the C source to /tmp/thvm_aot_<hash>.c, invokes clang -O2 -shared to build /tmp/thvm_aot_<hash>.dylib, dlopens it, and calls its aot_dylib_init() with the host's runtime-ops struct.  Returns 1 on success, 0 on failure.  The TAG_REF dispatch routes through the AOT immediately; subsequent TWnf goes through native code.  Phase coverage matches TAOTEmit -- bodies the emitter doesn't fully cover (e.g. TUOp / TSup / TBri) fall back to the interpreter for unsupported sub-trees.";

TAOTEmit::usage = "TAOTEmit[name] returns the C source the auto-emitter would produce for the def at TDefName[name].  Phase 0 of the auto-emitter: only constants and the identity lambda are supported; unsupported AST nodes emit a fallback comment.  Drop the result under src/aot/programs/<name>.c and rebuild thvm to use it.";

TAOT::badprog = "Unknown AOT program `1`; expected one of `2`.";
TAOT::badargc = "AOT program `1` expects `2` def name(s); got `3`.";

Begin["`Private`"];

(* === LibraryLink loaders ============================================ *)

$aotRegFibNatFn := $aotRegFibNatFn = load["thvm_wl_aot_register_fib_nat",
    {Integer, Integer, Integer}, Integer];
$aotRegGabTakFn := $aotRegGabTakFn = load["thvm_wl_aot_register_gab_tak",
    {Integer, Integer, Integer, Integer, Integer}, Integer];
$aotRegU32FibFn := $aotRegU32FibFn = load["thvm_wl_aot_register_u32_fib",
    {Integer}, Integer];
$aotCallsFn     := $aotCallsFn     = load["thvm_wl_aot_calls",     {}, Integer];
$aotEmitDefFn   := $aotEmitDefFn   = load["thvm_wl_aot_emit_def",  {Integer}, "UTF8String"];
$aotCompileFn   := $aotCompileFn   = load["thvm_wl_aot_compile",   {Integer}, Integer];

(* === Program registry ================================================
   Each entry: program-name -> {arity, register-fn taking that many
   integer slot ids}.  Adding a new hand-coded program means dropping
   it under src/aot/programs/, adding a thvm_wl_aot_register_<name>
   bridge in thvmlink.c, then one line here. *)

$aotPrograms = <|
    "fib_nat" -> <|"arity" -> 3,
                   "register" -> ($aotRegFibNatFn[#1, #2, #3] &)|>,
    "gab_tak" -> <|"arity" -> 5,
                   "register" -> ($aotRegGabTakFn[#1, #2, #3, #4, #5] &)|>,
    "u32_fib" -> <|"arity" -> 1,
                   "register" -> ($aotRegU32FibFn[#1] &)|>
|>;

TAOTPrograms[] := Keys[$aotPrograms]

TAOT[programName_String, defNames_List] := Module[{spec, ids, n, registered},
    ensureInit[];
    spec = Lookup[$aotPrograms, programName, None];
    If[ spec === None,
        Message[TAOT::badprog, programName, Keys[$aotPrograms]];
        Return[$Failed]
    ];
    n = spec["arity"];
    If[ Length[defNames] =!= n,
        Message[TAOT::badargc, programName, n, Length[defNames]];
        Return[$Failed]
    ];
    ids = TDefName /@ defNames;
    registered = Apply[spec["register"], ids];
    TAOTProgram[<|
        "name"       -> programName,
        "defs"       -> defNames,
        "slots"      -> ids,
        "registered" -> registered,
        "calls0"     -> $aotCallsFn[]
    |>]
]

TAOTCalls[] := (ensureInit[]; $aotCallsFn[])

TAOTReset[] := TInit[]

(* === Phase 0 auto-emitter ============================================
   TAOTEmit[name] returns the C source the auto-emitter generates for
   a TDef'd body.  Phase 0 supports just constants and the identity
   lambda; anything else emits "/* unsupported */" comments and a
   fallback to the interpreter.  Drop the result under
   src/aot/programs/<name>.c and rebuild thvm to use it.  Future
   phases will compile + dlopen automatically (TAOTCompile). *)

TAOTEmit[name_] := (
    ensureInit[];
    $aotEmitDefFn[TDefName[name]]
)

(* TAOTCompile[name]: end-to-end auto-AOT.  Emits the def's body
   to /tmp/thvm_aot_<hash>.c, invokes clang -O2 -shared, dlopens
   the resulting .dylib, and calls its aot_dylib_init() with the
   host's runtime-ops struct + the def's slot.  Returns 1 on
   success, 0 on failure (compile error, missing dlopen, etc.).
   The TAG_REF dispatch picks up the registered AOT immediately;
   subsequent TWnf calls go through native code instead of the
   lazy ALO interpreter. *)

TAOTCompile[name_] := (
    ensureInit[];
    $aotCompileFn[TDefName[name]]
)

(* === TAOTProgram payload accessors ================================== *)

(* Q-test mirrors Format.wl's pattern: structural shape check before
   the MakeBoxes UpValue fires. *)
tAOTProgramQ[TAOTProgram[a_Association]] :=
    KeyExistsQ[a, "name"] && KeyExistsQ[a, "defs"] &&
    KeyExistsQ[a, "slots"] && KeyExistsQ[a, "registered"] &&
    KeyExistsQ[a, "calls0"]
tAOTProgramQ[___] := False

(* Field access.  `prog["calls"]` returns the live count *delta* since
   registration -- distinct from the static "calls0" baseline. *)
TAOTProgram[a_Association ? AssociationQ][k_String] := Switch[k,
    "calls", $aotCallsFn[] - a["calls0"],
    _,       Lookup[a, k, Missing["KeyAbsent", k]]
]

(* === Summary-box icon =============================================== *)
(* Compiler-y wedge: incoming triangle (source) feeding into a
   colored disk (compiled artifact), suggesting "compiled to native". *)

aotSummaryIcon[] := Graphics[
    {
        EdgeForm[LightDarkSwitched[Black, White]],
        FaceForm[LightDarkSwitched[Lighter[StandardGreen, 0.55],
                                    Darker[StandardGreen, 0.4]]],
        Disk[{0.15, 0}, 0.5],
        FaceForm[LightDarkSwitched[Black, White]], EdgeForm[None],
        Polygon[{{-0.6, -0.55}, {-0.6, 0.55}, {-0.05, 0}}]
    },
    ImageSize -> Dynamic[{Automatic,
        3.0 CurrentValue["FontCapHeight"] /
            AbsoluteCurrentValue[Magnification]}],
    PlotRangePadding -> Scaled[0.05]
]

(* === MakeBoxes UpValue =============================================== *)

TAOTProgram /: MakeBoxes[p_TAOTProgram /; tAOTProgramQ[Unevaluated[p]], fmt_] :=
    With[{a = First[p], icon = aotSummaryIcon[], live = $aotCallsFn[] - First[p]["calls0"]},
        BoxForm`ArrangeSummaryBox[
            "TAOTProgram",
            p,
            icon,
            {
                {
                    BoxForm`SummaryItem[{"name: ", a["name"]}],
                    BoxForm`SummaryItem[{"defs: ",
                        Row[Riffle[a["defs"], ", "]]}]
                }
            },
            {
                {
                    BoxForm`SummaryItem[{"slots: ", a["slots"]}],
                    BoxForm`SummaryItem[{"registered: ", a["registered"]}]
                },
                {
                    BoxForm`SummaryItem[{"calls (live): ", live}]
                }
            },
            fmt,
            "Interpretable" -> Automatic
        ]
    ]

End[];
EndPackage[];
