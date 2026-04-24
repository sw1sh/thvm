(* ::Package:: *)
(* Ref.wl - lazy named definitions on top of TAG_REF + TAG_ALO.

   `TDef[name, body]` snapshots `body` into the immutable book heap
   and registers it under an integer slot derived from `name`
   (string -> stable integer via $defNames).  `TRef[name]` returns
   a TTerm wrapping a TAG_REF cell that, when reduced, lazily
   unfolds to the def's body via the ALO machinery -- so a
   self-referential def doesn't blow up at construction time.

   Names are intentionally kept as integers under the hood (the C
   side uses an array of DEFS_CAP slots) but a small WL-side string
   table makes the API ergonomic.  Users can pass either an integer
   slot (raw) or a string (looked up / minted on first use). *)

BeginPackage["THVMLink`"];

TDef::usage = "TDef[name, body] registers `body` as a named definition reachable via TRef[name].  `name` may be an integer slot (0..255) or a string (auto-mapped to a stable slot).  `body` is a TTerm built by the usual constructors; it gets snapshot into a static template heap so subsequent dynamic-heap mutations don't affect the def.";
TRef::usage = "TRef[name] returns a TTerm wrapping a TAG_REF that lazily unfolds to the body registered under `name`.  Reducing it via TWnf walks one ALO layer per fire (matches HVM4's REF -> ALO unfolding).";
TDefName::usage = "TDefName[name] returns the integer slot a given string `name` is mapped to (interns it on first use).";
TDefSlots::usage = "TDefSlots[] returns the current name->slot table as an Association.";

Begin["`Private`"];

(* String-name interning -- maintained per-session.  TInit doesn't
   reset this; the underlying DEFS table is repopulated on every
   TDef call so stale slot ids are fine to keep around. *)
$defNames = <||>;
$defNext  = 0;

TDefName[s_String] /; KeyExistsQ[$defNames, s] := $defNames[s]
TDefName[s_String] := With[{slot = $defNext},
    $defNames[s] = slot;
    $defNext = slot + 1;
    slot
]
TDefName[i_Integer] := i

TDefSlots[] := $defNames

(* Library function bindings -- loaded lazily like the others. *)
$defRegisterFn := $defRegisterFn = load["thvm_wl_def_register",
    {Integer, Integer}, Integer]
$termNewRefFn  := $termNewRefFn  = load["thvm_wl_term_new_ref",
    {Integer},          Integer]

TDef[name_, body_TTerm] := (
    ensureInit[];
    $defRegisterFn[TDefName[name], ttermRaw[body]];
    name
)

TRef[name_] := (
    ensureInit[];
    TTerm[$termNewRefFn[TDefName[name]]]
)

End[];

EndPackage[];
