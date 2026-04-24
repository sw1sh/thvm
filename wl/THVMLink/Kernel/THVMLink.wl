(* ::Package:: *)
(* THVMLink - Wolfram Language bridge to the thvm interaction-net runtime.

   The C bridge (CSource/thvmlink.c) exports 14 scalar functions covering
   term packing/unpacking, heap access, the WNF entry point, and a few
   counters.  This package wraps them and adds high-level constructors
   (TLam / TApp / TSup / TDup) plus inspection helpers (TTermInfo,
   THeap, TTagName).
*)

BeginPackage["THVMLink`"];

(* === lifecycle === *)
TInit::usage      = "TInit[] initializes the runtime.  Returns True.";
TFree::usage      = "TFree[] tears the runtime down.";
TReset::usage     = "TReset[] zeroes the heap, the WNF stack, and the interaction counter.";

(* === term primitives (raw, scalar) === *)
TTermNew::usage   = "TTermNew[sub, tag, ext, val] packs a 64-bit Term.";
TTermTag::usage   = "TTermTag[term] returns the tag (Integer).";
TTermExt::usage   = "TTermExt[term] returns the EXT field.";
TTermVal::usage   = "TTermVal[term] returns the VAL field (heap loc, etc.).";
TTermSub::usage   = "TTermSub[term] returns the SUB flag (0 or 1).";
TTagName::usage   = "TTagName[tag] returns a string for a tag id.";
TTermInfo::usage  = "TTermInfo[term] returns an Association decoding sub/tag/ext/val.";

(* === heap === *)
THeapPos::usage   = "THeapPos[] returns the next free heap location.";
THeapAlloc::usage = "THeapAlloc[size] reserves `size` consecutive cells; returns the base loc.";
THeapRead::usage  = "THeapRead[loc] returns the Term at heap[loc].";
THeapSet::usage   = "THeapSet[loc, term] writes `term` to heap[loc].";
THeap::usage      = "THeap[] returns an Association snapshot: <|\"nextLoc\" -> n, \"cells\" -> <|loc -> info, ...|>|>.";

(* === reduce / stats === *)
TWnf::usage       = "TWnf[term] reduces `term` to weak normal form.";
TItrs::usage      = "TItrs[] returns the cumulative interaction count.";

(* === high-level constructors === *)
TEra::usage       = "TEra[] constructs an eraser term.";
TVarFor::usage    = "TVarFor[lamLoc] constructs a VAR pointing at a binder loc.";
TLam::usage       = "TLam[builder] constructs a lambda.  `builder` receives the bound var and returns the body.";
TApp::usage       = "TApp[fun, arg] constructs an application.";
TSup::usage       = "TSup[label, a, b] constructs a superposition.";
TDup::usage       = "TDup[label, body, k] constructs a duplication; calls `k[dp0, dp1]` and returns its result.";

(* === tag constants (mirror src/thvm.h) === *)
$TagAPP::usage = $TagLAM::usage = $TagVAR::usage = $TagERA::usage =
  $TagDP0::usage = $TagDP1::usage = $TagSUP::usage = $TagDUP::usage =
    "Tag id; mirrors the corresponding TAG_* in src/thvm.h.";

Begin["`Private`"];

$libDir = FileNameJoin[{
    DirectoryName[$InputFileName],
    "..", "LibraryResources", $SystemID
}];

$lib = FileNameJoin[{$libDir, "THVMLink" <> Switch[$OperatingSystem,
    "MacOSX", ".dylib", "Windows", ".dll", _, ".so"]}];

debugPrint[args___] := WriteString[$Output, StringJoin @@ Map[ToString, {args}], "\n"]

If[!FileExistsQ[$lib],
    debugPrint["[THVMLink] Library not built.  Run `make wl` from the repo root."];
    debugPrint["[THVMLink] Expected at: ", $lib]
];

(* Tag constants - keep in sync with src/thvm.h *)
$TagAPP = 0; $TagLAM = 1; $TagVAR = 2; $TagERA = 3;
$TagDP0 = 4; $TagDP1 = 5; $TagSUP = 6; $TagDUP = 7;

$tagNames = <|
    0 -> "APP", 1 -> "LAM", 2 -> "VAR", 3 -> "ERA",
    4 -> "DP0", 5 -> "DP1", 6 -> "SUP", 7 -> "DUP"
|>;

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

(* === public API === *)
TInit[]      := ($initFn[] === 1)
TFree[]      := $freeFn[]
TReset[]     := $resetFn[]

TTermNew[sub_Integer, tag_Integer, ext_Integer, val_Integer] :=
    $termNewFn[sub, tag, ext, val]

TTermTag[t_Integer] := $termTagFn[t]
TTermExt[t_Integer] := $termExtFn[t]
TTermVal[t_Integer] := $termValFn[t]
TTermSub[t_Integer] := $termSubFn[t]

TTermInfo[t_Integer] := <|
    "sub"     -> TTermSub[t],
    "tag"     -> TTermTag[t],
    "tagName" -> TTagName[TTermTag[t]],
    "ext"     -> TTermExt[t],
    "val"     -> TTermVal[t],
    "raw"     -> t
|>

THeapPos[]                       := $heapPosFn[]
THeapAlloc[size_Integer]         := $heapAllocFn[size]
THeapRead[loc_Integer]           := $heapReadFn[loc]
THeapSet[loc_Integer, t_Integer] := $heapSetFn[loc, t]

THeap[] := Block[{n = THeapPos[]},
    <|
        "nextLoc" -> n,
        "cells"   -> Association @ Table[
            i -> TTermInfo[THeapRead[i]],
            {i, 0, n - 1}
        ]
    |>
]

TWnf[t_Integer]  := $wnfFn[t]
TItrs[]          := $itrsFn[]

(* === high-level constructors === *)

(* heapWith[v1, v2, ...]   alloc Length[{vs}] cells, write the values into
                           them in order, return the base loc. *)
heapWith[fields__] := With[{loc = THeapAlloc[Length[{fields}]]},
    MapIndexed[THeapSet[loc + First[#2] - 1, #1] &, {fields}];
    loc
]

(* heapTerm[tag, ext, vs...]   pack a heap-backed Term whose payload
                               cells are vs in order. *)
heapTerm[tag_Integer, ext_Integer, fields__] :=
    TTermNew[0, tag, ext, heapWith[fields]]

TEra[]                  := TTermNew[0, $TagERA, 0, 0]
TVarFor[lamLoc_Integer] := TTermNew[0, $TagVAR, 0, lamLoc]

TApp[fun_Integer, arg_Integer]            := heapTerm[$TagAPP, 0,     fun, arg]
TSup[label_Integer, a_Integer, b_Integer] := heapTerm[$TagSUP, label, a,   b  ]

(* TLam needs the loc before it can compute the body (binder is at loc),
   so it can't share heapWith. *)
TLam[builder_] := With[{loc = THeapAlloc[1]},
    THeapSet[loc, builder[TVarFor[loc]]];
    TTermNew[0, $TagLAM, 0, loc]
]

(* TDup returns a pair of projection terms over a shared cell. *)
TDup[label_Integer, body_Integer, k_] := With[{loc = heapWith[body]},
    k[TTermNew[0, $TagDP0, label, loc],
      TTermNew[0, $TagDP1, label, loc]]
]

End[];
EndPackage[];
