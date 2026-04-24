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

(* === reduce / stats === *)
TWnf::usage       = "TWnf[term] reduces `term` to weak normal form.";
TItrs::usage      = "TItrs[] returns the cumulative interaction count.";

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
    "Tag id; mirrors the corresponding TAG_* in src/thvm.h.";

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
TItrs[]          := (ensureInit[]; $itrsFn[])

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

TLam[builder_] := With[{loc = THeapAlloc[1]},
    THeapSet[loc, builder[TVarFor[loc]]];
    packTerm[0, $TagLAM, 0, loc]
]

TDup[body_, k_]                       := TDup[TFreshLabel[], body, k]
TDup[label_Integer, body_, k_] := With[{loc = heapWith[body]},
    k[packTerm[0, $TagDP0, label, loc],
      packTerm[0, $TagDP1, label, loc]]
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
