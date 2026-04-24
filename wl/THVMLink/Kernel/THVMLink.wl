(* ::Package:: *)
(* THVMLink - Wolfram Language bridge to the thvm interaction-net runtime.

   The C bridge (CSource/thvmlink.c) exports 14 scalar functions covering
   term packing/unpacking, heap access, the WNF entry point, and a few
   counters.  This package wraps them and adds high-level constructors
   (TLam / TApp / TSup / TDup), inspection helpers (TTermInfo, THeap,
   TTagName), and the IC-style heap renderer (THeapGraph).
*)

BeginPackage["THVMLink`", {"GeneralUtilities`"}];

(* === lifecycle === *)
TInit::usage      = "TInit[] initializes the runtime.  Returns True.";
TFree::usage      = "TFree[] tears the runtime down.";
TReset::usage     = "TReset[] zeroes the heap, the WNF stack, and the interaction counter.";

(* === atomic term object === *)
TTerm::usage      = "TTerm[id_Integer] wraps a packed 64-bit Term value.  Construct via TLam / TApp / TSup / TDup / TEra / TVarFor; or directly TTerm[<rawInteger>].  Indexing is supported: TTerm[id][\"tag\"|\"ext\"|\"val\"|\"sub\"|\"tagName\"|\"raw\"].";
TTermTag::usage   = "TTermTag[term] returns the tag (Integer).  Accepts either a TTerm or a raw Integer.";
TTermExt::usage   = "TTermExt[term] returns the EXT field.";
TTermVal::usage   = "TTermVal[term] returns the VAL field (heap loc, etc.).";
TTermSub::usage   = "TTermSub[term] returns the SUB flag (0 or 1).";
TTagName::usage   = "TTagName[tag] returns a string for a tag id.";

(* === heap === *)
THeapPos::usage   = "THeapPos[] returns the next free heap location.";
THeapAlloc::usage = "THeapAlloc[size] reserves `size` consecutive cells; returns the base loc.";
THeapRead::usage  = "THeapRead[loc] returns the Term at heap[loc].";
THeapSet::usage   = "THeapSet[loc, term] writes `term` to heap[loc].";
THeap::usage      = "THeap[] returns an Association snapshot with keys \"nextLoc\", \"cells\", \"Graph\".  See docs/heap_graph.md.";
THeapGraph::usage = "THeapGraph[] renders the heap state as an IC string-diagram Graph.  THeapGraph[term] also seeds discovery with `term` so heapless compounds held only by the WL caller appear.  THeapGraph[{t1, t2, ...}] seeds with several.  See docs/heap_graph.md.";
THeapDiagram::usage = "THeapDiagram[term] builds a Wolfram`DiagrammaticComputation`DiagramNetwork from the heap, with one Diagram per compound agent and one ERA Diagram per ERA cell.  Wires share string identifiers keyed off heap loc; VAR cells collapse to their binder loc.";

(* === reduce / stats === *)
TWnf::usage       = "TWnf[term] reduces `term` to weak normal form.";
TReduce::usage    = "TReduce[term] reduces `term` to WNF in-place and returns `term` (the original root, useful as a seed for THeapGraph after reduction).";
TItrs::usage      = "TItrs[] returns the cumulative interaction count.";
TTermExpr::usage  = "TTermExpr[term] walks the heap from `term` and returns a nested expression whose heads are tag-name strings (\"LAM\", \"APP\", \"SUP\", \"DUP\", \"DP0\", \"DP1\", \"VAR\", \"ERA\").  Useful for snapshotting / diffing pre and post TWnf states by direct equality (===).";
TTermTree::usage  = "TTermTree[term] = ExpressionTree[TTermExpr[term]] -- the same structure rendered as a Wolfram Tree object for visual inspection.";

(* === high-level constructors === *)
TFreshLabel::usage = "TFreshLabel[] returns the next integer from a monotonic SUP/DUP label counter, then bumps it.  Reset by TReset[].";
TEra::usage       = "TEra[] constructs an eraser term.";
TVarFor::usage    = "TVarFor[lamLoc] constructs a VAR pointing at a binder loc.";
TLam::usage       = "TLam[builder] constructs a lambda; `builder` receives the bound var and returns the body.";
TApp::usage       = "TApp[fun, arg] constructs an application.";
TSup::usage       = "TSup[a, b] constructs a SUP with a fresh label.  TSup[label, a, b] uses an explicit label.";
TDup::usage       = "TDup[body, k] constructs a DUP with a fresh label and calls `k[dp0, dp1]`.  TDup[label, body, k] uses an explicit label.";

(* === tag constants (mirror src/thvm.h) === *)
$TagAPP::usage = $TagLAM::usage = $TagVAR::usage = $TagERA::usage =
  $TagDP0::usage = $TagDP1::usage = $TagSUP::usage = $TagDUP::usage =
  $TagTEN::usage = $TagUOP::usage = $TagNUM::usage =
    "Tag id; mirrors the corresponding TAG_* in src/thvm.h.";

(* === dtype + opcode constants (mirror src/thvm.h) === *)
$DTF32::usage = $DTI32::usage =
    "Dtype id; mirrors DT_* in src/thvm.h.";

$UopMaterialize::usage = $UopKernel::usage = $UopConst::usage =
  $UopReshape::usage    = $UopPermute::usage = $UopExpand::usage =
  $UopPad::usage        = $UopShrink::usage = $UopFlip::usage =
  $UopAdd::usage        = $UopMul::usage = $UopNeg::usage =
  $UopRecip::usage      = $UopExp2::usage = $UopLog2::usage =
  $UopSqrt::usage       = $UopCmplt::usage = $UopReduce::usage =
    "UOp opcode id; mirrors UOP_* in src/thvm.h.";

$ReduceSum::usage = $ReduceMax::usage =
    "Reduce-kind id; mirrors REDUCE_* in src/thvm.h.";

(* === tensors === *)
TTensor::usage         = "TTensor[shape, dtype] allocates a tensor and returns a TTerm wrapping a TAG_TEN handle.  TTensor[shape, data_List] also writes initial values.  dtype defaults to \"f32\".";
TTensorShape::usage    = "TTensorShape[t] returns the tensor's shape as a list of integers.";
TTensorDType::usage    = "TTensorDType[t] returns the dtype as a string (\"f32\" / \"i32\").";
TTensorData::usage     = "TTensorData[t] reads the tensor's buffer back into a flat WL list.";
TTensorRefcount::usage = "TTensorRefcount[t] returns the descriptor refcount (TENS[id].refcount).";

(* === UOp graph constructors === *)
TUOpConst::usage     = "TUOpConst[value, dtype] builds a UOP_CONST node carrying a scalar.";
TUOpReshape::usage   = "TUOpReshape[src, shape_List] builds a UOP_RESHAPE node.";
TUOpPermute::usage   = "TUOpPermute[src, axes_List] builds a UOP_PERMUTE node.";
TUOpExpand::usage    = "TUOpExpand[src, shape_List] builds a UOP_EXPAND node.";
TUOpPad::usage       = "TUOpPad[src, ranges_List] builds a UOP_PAD node.  ranges = {{b0,e0},{b1,e1},...}.";
TUOpShrink::usage    = "TUOpShrink[src, ranges_List] builds a UOP_SHRINK node.";
TUOpFlip::usage      = "TUOpFlip[src, axes_List] builds a UOP_FLIP node; axes is a list of axis indices to flip.";
TUOpAdd::usage       = "TUOpAdd[a, b] builds a UOP_ADD node.";
TUOpMul::usage       = "TUOpMul[a, b] builds a UOP_MUL node.";
TUOpNeg::usage       = "TUOpNeg[a] builds a UOP_NEG node.";
TUOpRecip::usage     = "TUOpRecip[a] builds a UOP_RECIP node.";
TUOpExp2::usage      = "TUOpExp2[a] builds a UOP_EXP2 node.";
TUOpLog2::usage      = "TUOpLog2[a] builds a UOP_LOG2 node.";
TUOpSqrt::usage      = "TUOpSqrt[a] builds a UOP_SQRT node.";
TUOpCmplt::usage     = "TUOpCmplt[a, b] builds a UOP_CMPLT node.";
TUOpReduce::usage    = "TUOpReduce[src, axis, kind] builds a UOP_REDUCE node; kind = \"SUM\" or \"MAX\".";
TUOpMaterialize::usage = "TUOpMaterialize[expr] wraps a raw UOp graph; reducing it via TWnf fires the materialize rule (or use TMaterialize for inspection).";
TUOpKind::usage      = "TUOpKind[u] returns the opcode name for a UOp term.";
TUOpSrcs::usage      = "TUOpSrcs[u] returns the source-cell terms for a UOp term, in heap order.";

Begin["`Private`"];

$libDir = FileNameJoin[{
    DirectoryName[$InputFileName],
    "..", "LibraryResources", $SystemID
}];

$lib = FileNameJoin[{$libDir, "THVMLink" <> Switch[$OperatingSystem,
    "MacOSX", ".dylib", "Windows", ".dll", _, ".so"]}];

debugPrint[args___] := WriteString[$Output, StringJoin @@ Map[ToString, {args}], "\n"]

If[ ! FileExistsQ[$lib],
    debugPrint["[THVMLink] Library not built.  Run `make wl` from the repo root."];
    debugPrint["[THVMLink] Expected at: ", $lib]
];

(* Tag constants - keep in sync with src/thvm.h *)
$TagAPP = 0; $TagLAM = 1; $TagVAR = 2; $TagERA = 3;
$TagDP0 = 4; $TagDP1 = 5; $TagSUP = 6; $TagDUP = 7;
$TagTEN = 8; $TagUOP = 9; $TagNUM = 10;

$tagNames = <|
    0 -> "APP", 1 -> "LAM", 2 -> "VAR",  3 -> "ERA",
    4 -> "DP0", 5 -> "DP1", 6 -> "SUP",  7 -> "DUP",
    8 -> "TEN", 9 -> "UOP", 10 -> "NUM"
|>;

(* Dtype constants - keep in sync with src/thvm.h *)
$DTF32 = 0; $DTI32 = 1;

dtypeCode["f32"] = $DTF32;  dtypeCode["i32"] = $DTI32;
dtypeCode[$DTF32] = $DTF32; dtypeCode[$DTI32] = $DTI32;
dtypeName[$DTF32] = "f32";  dtypeName[$DTI32] = "i32";

(* UOp opcode constants - keep in sync with src/thvm.h *)
$UopMaterialize = 0;  $UopKernel = 1;  $UopConst = 2;
$UopReshape = 3;      $UopPermute = 4; $UopExpand = 5;
$UopPad = 6;          $UopShrink = 7;  $UopFlip = 8;
$UopAdd = 9;          $UopMul = 10;    $UopNeg = 11;
$UopRecip = 12;       $UopExp2 = 13;   $UopLog2 = 14;
$UopSqrt = 15;        $UopCmplt = 16;  $UopReduce = 17;

$uopNames = <|
    0  -> "MATERIALIZE", 1  -> "KERNEL", 2  -> "CONST",
    3  -> "RESHAPE",     4  -> "PERMUTE",5  -> "EXPAND",
    6  -> "PAD",         7  -> "SHRINK", 8  -> "FLIP",
    9  -> "ADD",         10 -> "MUL",    11 -> "NEG",
    12 -> "RECIP",       13 -> "EXP2",   14 -> "LOG2",
    15 -> "SQRT",        16 -> "CMPLT",  17 -> "REDUCE"
|>;

(* Reduce-kind constants *)
$ReduceSum = 0; $ReduceMax = 1;
reduceKindCode["SUM"] = $ReduceSum;  reduceKindCode["MAX"] = $ReduceMax;

TTagName[t_Integer] := Lookup[$tagNames, t, "TAG?" <> ToString[t]]

(* === library function loaders === *)
load[name_String, args_, ret_] := LibraryFunctionLoad[$lib, name, args, ret]

$initFn      := $initFn      = load["thvm_wl_init",       {},                       Integer];
$freeFn      := $freeFn      = load["thvm_wl_free",       {},                       Integer];
$resetFn     := $resetFn     = load["thvm_wl_reset",      {},                       Integer];

$termNewFn   := $termNewFn   = load["thvm_wl_term_new",   {Integer, Integer, Integer, Integer}, Integer];
$termTagFn   := $termTagFn   = load["thvm_wl_term_tag",   {Integer},                Integer];
$termExtFn   := $termExtFn   = load["thvm_wl_term_ext",   {Integer},                Integer];
$termValFn   := $termValFn   = load["thvm_wl_term_val",   {Integer},                Integer];
$termSubFn   := $termSubFn   = load["thvm_wl_term_sub",   {Integer},                Integer];

$heapPosFn   := $heapPosFn   = load["thvm_wl_heap_pos",   {},                       Integer];
$heapAllocFn := $heapAllocFn = load["thvm_wl_heap_alloc", {Integer},                Integer];
$heapReadFn  := $heapReadFn  = load["thvm_wl_heap_read",  {Integer},                Integer];
$heapSetFn   := $heapSetFn   = load["thvm_wl_heap_set",   {Integer, Integer},       Integer];

$wnfFn       := $wnfFn       = load["thvm_wl_wnf",        {Integer},                Integer];
$itrsFn      := $itrsFn      = load["thvm_wl_itrs",       {},                       Integer];

(* tensor *)
$tensorAllocFn   := $tensorAllocFn   = load["thvm_wl_tensor_alloc",   {Integer, {Integer, 1}}, Integer];
$tensorWriteFn   := $tensorWriteFn   = load["thvm_wl_tensor_write",   {Integer, {Real, 1}},    Integer];
$tensorWriteIFn  := $tensorWriteIFn  = load["thvm_wl_tensor_write",   {Integer, {Integer, 1}}, Integer];
$tensorReadFn    := $tensorReadFn    = load["thvm_wl_tensor_read",    {Integer},               {Real, 1}];
$tensorReadIFn   := $tensorReadIFn   = load["thvm_wl_tensor_read",    {Integer},               {Integer, 1}];
$tensorShapeFn   := $tensorShapeFn   = load["thvm_wl_tensor_shape",   {Integer},               {Integer, 1}];
$tensorRcFn      := $tensorRcFn      = load["thvm_wl_tensor_refcount",{Integer},               Integer];

(* uop graph *)
$uopConstFn    := $uopConstFn    = load["thvm_wl_uop_const",    {Integer, Real},                     Integer];
$uopUnaryFn    := $uopUnaryFn    = load["thvm_wl_uop_unary",    {Integer, Integer},                  Integer];
$uopBinaryFn   := $uopBinaryFn   = load["thvm_wl_uop_binary",   {Integer, Integer, Integer},         Integer];
$uopReduceFn   := $uopReduceFn   = load["thvm_wl_uop_reduce",   {Integer, Integer, Integer},         Integer];
$uopReshapeFn  := $uopReshapeFn  = load["thvm_wl_uop_reshape",  {Integer, {Integer, 1}},             Integer];
$uopPermuteFn  := $uopPermuteFn  = load["thvm_wl_uop_permute",  {Integer, {Integer, 1}},             Integer];
$uopExpandFn   := $uopExpandFn   = load["thvm_wl_uop_expand",   {Integer, {Integer, 1}},             Integer];
$uopPadFn      := $uopPadFn      = load["thvm_wl_uop_pad",      {Integer, {Integer, 1}},             Integer];
$uopShrinkFn   := $uopShrinkFn   = load["thvm_wl_uop_shrink",   {Integer, {Integer, 1}},             Integer];
$uopFlipFn     := $uopFlipFn     = load["thvm_wl_uop_flip",     {Integer, Integer},                  Integer];
$uopMatFn      := $uopMatFn      = load["thvm_wl_uop_materialize", {Integer},                        Integer];

(* === fresh-label counter (WL-side; reset by TReset) === *)
$labelCounter = 1;
TFreshLabel[] := Block[{n = $labelCounter}, $labelCounter += 1; n]

(* === public API === *)
$initialized = False

(* Any op that touches the heap calls ensureInit[] first.  TInit /
   TReset / TFree all flip $initialized themselves so a manual
   teardown still does the right thing. *)
ensureInit[] := If[ ! $initialized, TInit[]]

TInit[]      := ($labelCounter = 1; $initialized = True; $initFn[] === 1)
TFree[]      := ($initialized = False; $freeFn[])
TReset[]     := ($labelCounter = 1; ensureInit[]; $resetFn[])

(* === TTerm atomic object ===
   `TTerm[id_Integer]` is the canonical wrapper around a packed 64-bit
   `Term` value.  All constructors (TLam, TApp, TSup, TDup, TEra,
   TVarFor) return TTerm-wrapped values; all inspectors and the heap
   API accept either a TTerm or a raw `Integer` so internal helpers
   (heapWith, etc.) can stay scalar-friendly.  The MakeBoxes summary
   box for TTerm lives in Format.wl. *)

(* Internal: pull the raw Integer out of a TTerm or pass through. *)
ttermRaw[TTerm[id_Integer]] := id
ttermRaw[id_Integer]        := id

(* Pack a fresh TTerm from raw fields.  Private; callers use the
   high-level constructors. *)
packTerm[sub_Integer, tag_Integer, ext_Integer, val_Integer] :=
    TTerm[$termNewFn[sub, tag, ext, val]]

(* Inspectors accept either TTerm or Integer. *)
TTermTag[t_]                    := $termTagFn[ttermRaw[t]]
TTermExt[t_]                    := $termExtFn[ttermRaw[t]]
TTermVal[t_]                    := $termValFn[ttermRaw[t]]
TTermSub[t_]                    := $termSubFn[ttermRaw[t]]

(* TTerm methods: TTerm[id]["tag"], etc. *)
TTerm[id_Integer]["raw"]        := id
TTerm[id_Integer]["tag"]        := $termTagFn[id]
TTerm[id_Integer]["ext"]        := $termExtFn[id]
TTerm[id_Integer]["val"]        := $termValFn[id]
TTerm[id_Integer]["sub"]        := $termSubFn[id]
TTerm[id_Integer]["tagName"]    := TTagName[$termTagFn[id]]
TTerm[id_Integer]["info"]       := <|
    "sub"     -> $termSubFn[id],
    "tag"     -> $termTagFn[id],
    "tagName" -> TTagName[$termTagFn[id]],
    "ext"     -> $termExtFn[id],
    "val"     -> $termValFn[id],
    "raw"     -> id
|>

THeapPos[]                       := (ensureInit[]; $heapPosFn[])
THeapAlloc[size_Integer]         := (ensureInit[]; $heapAllocFn[size])
THeapRead[loc_Integer]           := (ensureInit[]; TTerm[$heapReadFn[loc]])
THeapSet[loc_Integer, t_]        := (ensureInit[]; $heapSetFn[loc, ttermRaw[t]])

TWnf[t_]         := (ensureInit[]; TTerm[$wnfFn[ttermRaw[t]]])

(* TReduce reduces `t` to WNF in-place and returns the original root.
   Pairs with THeapGraph[t] / TTermTree[t] when you want the
   post-reduction state seeded from the term you constructed. *)
TReduce[t_] := (TWnf[t]; t)

TItrs[]          := (ensureInit[]; $itrsFn[])

(* === heap walker: term -> nested string-headed expression ===
   Walks from `t` through the heap and returns the structural shape
   with heads "LAM" / "APP" / "SUP" / "DUP" / "DP0" / "DP1" / "VAR" /
   "ERA".  Compound args bases are tracked in a `seen` association so
   cycles produce a `"Cycle"[loc]` leaf instead of looping.

   `TTermExpr` returns the raw nested expression (cheap structural
   equality via `===`).  `TTermTree` wraps it in `ExpressionTree[...]`
   so it renders as a Wolfram `Tree` object for visual inspection. *)

TTermExpr[t_] := tTreeWalk[t, <||>]
TTermTree[t_] := ExpressionTree[TTermExpr[t]]

tTreeWalk[t_, seen_] := Block[{
    raw = ttermRaw[t], tag, val, ext, seen2
},
    tag = $termTagFn[raw];
    val = $termValFn[raw];
    ext = $termExtFn[raw];
    Switch[tag,
        $TagERA, "ERA",
        $TagVAR, "VAR"[val],
        (* DP0 / DP1 recurse into the dup body so each projection's
           subtree is visible.  Trees can't share, so the same body
           appears once under each projection. *)
        $TagDP0, "DP0"[ext, tTreeWalk[$heapReadFn[val], seen]],
        $TagDP1, "DP1"[ext, tTreeWalk[$heapReadFn[val], seen]],
        $TagLAM,
            If[ KeyExistsQ[seen, val], "Cycle"[val],
                seen2 = Append[seen, val -> True];
                "LAM"[tTreeWalk[$heapReadFn[val], seen2]]],
        $TagAPP,
            If[ KeyExistsQ[seen, val], "Cycle"[val],
                seen2 = Append[seen, val -> True];
                "APP"[tTreeWalk[$heapReadFn[val], seen2],
                      tTreeWalk[$heapReadFn[val + 1], seen2]]],
        $TagSUP,
            If[ KeyExistsQ[seen, val], "Cycle"[val],
                seen2 = Append[seen, val -> True];
                "SUP"[ext,
                    tTreeWalk[$heapReadFn[val], seen2],
                    tTreeWalk[$heapReadFn[val + 1], seen2]]],
        $TagDUP,
            If[ KeyExistsQ[seen, val], "Cycle"[val],
                seen2 = Append[seen, val -> True];
                "DUP"[ext, tTreeWalk[$heapReadFn[val], seen2]]],
        _, "Unknown"[tag]
    ]
]

(* === high-level constructors (all return TTerm) === *)

heapWith[fields__] := With[{loc = THeapAlloc[Length[{fields}]]},
    ScanIndexed[THeapSet[loc + First[#2] - 1, #1] &, {fields}];
    loc
]

heapTerm[tag_Integer, ext_Integer, fields__] :=
    packTerm[0, tag, ext, heapWith[fields]]

TEra[]                  := packTerm[0, $TagERA, 0, 0]
TVarFor[lamLoc_Integer] := packTerm[0, $TagVAR, 0, lamLoc]

TApp[fun_, arg_] := heapTerm[$TagAPP, 0, fun, arg]

TSup[a_, b_]                          := TSup[TFreshLabel[], a, b]
TSup[label_Integer, a_, b_]           := heapTerm[$TagSUP, label, a, b]

TLam[builder_] := Block[{$inTLamBinder = True}, With[{loc = THeapAlloc[1]},
    THeapSet[loc, builder[TVarFor[loc]]];
    packTerm[0, $TagLAM, 0, loc]
]]

(* Sugar:
       TTerm[id][arg]                   ==  TApp[TTerm[id], arg]
       (var |-> body)[TTerm[id]]        ==  TApp[TLam[var |-> body], TTerm[id]]
   The first lets the user write `id[era]` instead of `TApp[id, era]`.
   The second lets `(var |-> body)[arg]` work as a beta-redex literal
   without writing `TLam` or `TApp` explicitly.  The Function UpValue
   is guarded by `$inTLamBinder` so TLam's own internal call
   `builder[TVarFor[loc]]` does not trigger it (which would recurse
   infinitely back into TLam). *)
TTerm[id_Integer][y_TTerm]   := TApp[TTerm[id], y]
TTerm[id_Integer][y_Integer] := TApp[TTerm[id], y]

$inTLamBinder = False

TTerm /: HoldPattern[(f_Function)[t_TTerm]] /; ! $inTLamBinder :=
    TApp[TLam[f], t]

TDup[body_, k_]                       := TDup[TFreshLabel[], body, k]
TDup[label_Integer, body_, k_] := With[{loc = heapWith[body]},
    k[packTerm[0, $TagDP0, label, loc],
      packTerm[0, $TagDP1, label, loc]]
]

(* === tensor constructors === *)

TTensor[shape_List]                       := TTensor[shape, "f32"]
TTensor[shape_List, dtype_String]         := (
    ensureInit[];
    TTerm[$tensorAllocFn[dtypeCode[dtype], shape]]
)
TTensor[shape_List, data_List]            := TTensor[shape, data, "f32"]
TTensor[shape_List, data_List, dtype_String] := With[{
    t = TTensor[shape, dtype]
},
    If[ dtype === "f32",
        $tensorWriteFn [TTermVal[t], N @ Flatten[data]],
        $tensorWriteIFn[TTermVal[t], Round @ Flatten[data]]
    ];
    t
]

TTensorShape[t_]    := $tensorShapeFn[TTermVal[t]]
TTensorDType[t_]    := dtypeName[TTermExt[t]]
TTensorRefcount[t_] := $tensorRcFn[TTermVal[t]]

TTensorData[t_] := With[{id = TTermVal[t], dt = TTermExt[t]},
    If[ dt === $DTF32, $tensorReadFn[id], $tensorReadIFn[id]]
]

(* === UOp graph constructors === *)

(* All wrap the raw uop_* C calls into TTerms.  Sources are passed
   through ttermRaw so callers can mix TTerm and Integer freely. *)

TUOpConst[value_, dtype_String : "f32"] := (
    ensureInit[];
    TTerm[$uopConstFn[dtypeCode[dtype], N[value]]]
)

TUOpAdd[a_, b_]   := (ensureInit[]; TTerm[$uopBinaryFn[$UopAdd,   ttermRaw[a], ttermRaw[b]]])
TUOpMul[a_, b_]   := (ensureInit[]; TTerm[$uopBinaryFn[$UopMul,   ttermRaw[a], ttermRaw[b]]])
TUOpCmplt[a_, b_] := (ensureInit[]; TTerm[$uopBinaryFn[$UopCmplt, ttermRaw[a], ttermRaw[b]]])

TUOpNeg[a_]   := (ensureInit[]; TTerm[$uopUnaryFn[$UopNeg,   ttermRaw[a]]])
TUOpRecip[a_] := (ensureInit[]; TTerm[$uopUnaryFn[$UopRecip, ttermRaw[a]]])
TUOpExp2[a_]  := (ensureInit[]; TTerm[$uopUnaryFn[$UopExp2,  ttermRaw[a]]])
TUOpLog2[a_]  := (ensureInit[]; TTerm[$uopUnaryFn[$UopLog2,  ttermRaw[a]]])
TUOpSqrt[a_]  := (ensureInit[]; TTerm[$uopUnaryFn[$UopSqrt,  ttermRaw[a]]])

TUOpReduce[src_, axis_Integer, kind_String] := (
    ensureInit[];
    TTerm[$uopReduceFn[reduceKindCode[kind], axis, ttermRaw[src]]]
)

TUOpReshape[src_, shape_List] := (ensureInit[]; TTerm[$uopReshapeFn[ttermRaw[src], shape]])
TUOpPermute[src_, axes_List]  := (ensureInit[]; TTerm[$uopPermuteFn[ttermRaw[src], axes]])
TUOpExpand [src_, shape_List] := (ensureInit[]; TTerm[$uopExpandFn [ttermRaw[src], shape]])

(* PAD/SHRINK ranges arrive as {{b0,e0},{b1,e1},...}; flatten to
   the C-side begin/end pair stream. *)
TUOpPad   [src_, ranges_List] := (ensureInit[]; TTerm[$uopPadFn   [ttermRaw[src], Flatten[ranges]]])
TUOpShrink[src_, ranges_List] := (ensureInit[]; TTerm[$uopShrinkFn[ttermRaw[src], Flatten[ranges]]])

(* FLIP axes arrive as a list of axis indices; pack to a bitmask. *)
TUOpFlip[src_, axes_List] := With[{mask = Total[2^# & /@ axes]},
    ensureInit[];
    TTerm[$uopFlipFn[ttermRaw[src], mask]]
]

TUOpMaterialize[expr_] := (ensureInit[]; TTerm[$uopMatFn[ttermRaw[expr]]])

(* Inspection helpers for UOp terms. *)
TUOpKind[u_] := Lookup[$uopNames, TTermExt[u], "UOP?" <> ToString[TTermExt[u]]]

(* TUOpSrcs returns the heap cells that are the immediate sources of
   the UOp.  Per docs/tensors.md per-opcode heap layouts, each
   opcode has a fixed number of source cells (variable-arity
   movement ops are returned as just `[src]` for step 12; the
   trailing dimension cells stay implicit -- they're inspected via
   raw THeapRead by the materialize pass). *)
TUOpSrcs[u_] := With[{loc = TTermVal[u], op = TTermExt[u]},
    Module[{n},
        n = Which[
            op === $UopMaterialize,                                                            1,
            op === $UopKernel,                                                                  2,
            op === $UopConst,                                                                   1,
            MemberQ[{$UopReshape, $UopPermute, $UopExpand,
                     $UopPad, $UopShrink, $UopFlip}, op],                                      1,
            MemberQ[{$UopAdd, $UopMul, $UopCmplt}, op],                                        2,
            MemberQ[{$UopNeg, $UopRecip, $UopExp2, $UopLog2, $UopSqrt}, op],                   1,
            op === $UopReduce,                                                                  1,
            True,                                                                               1
        ];
        Table[THeapRead[loc + i], {i, 0, n - 1}]
    ]
]

(* === heap graph rendering ===
   Defined in Visualization.wl (loaded below).  Public symbol
   THeapGraph; per-tag shapes / colours are private. *)

THeap[] := Block[{n = THeapPos[]},
    THeap[<|
        "nextLoc" -> n,
        "cells"   -> Association @ Table[
            i -> THeapRead[i],
            {i, 0, n - 1}
        ],
        "Graph"   -> THeapGraph[]
    |>]
]

THeap[a_Association][k_] := a[k]
THeap /: KeyExistsQ[THeap[a_Association], k_] := KeyExistsQ[a, k]
THeap /: Keys[THeap[a_Association]]           := Keys[a]
THeap /: Values[THeap[a_Association]]         := Values[a]
THeap /: Normal[THeap[a_Association]]         := a

(* === sibling files === *)
With[{dir = DirectoryName[$InputFileName]},
    Get[FileNameJoin[{dir, "Visualization.wl"}]];
    Get[FileNameJoin[{dir, "Format.wl"}]]
]

End[];
EndPackage[];

(* Diagram.wl lives in its own subpackage (THVMLink`Diagram`) so it can
   import Wolfram`DiagrammaticComputation` without context-shadowing
   the rest of THVMLink`. *)
Get[FileNameJoin[{DirectoryName[$InputFileName], "Diagram.wl"}]]
