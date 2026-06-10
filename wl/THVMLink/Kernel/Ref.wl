(* ::Package:: *)
(* Ref.wl - lazy named definitions on top of TAG_REF + TAG_ALO.

   `TDef[name, body]` snapshots `body` into the immutable book heap
   and registers it under an integer slot derived from `name`
   (string -> stable integer via $defNames).  `TRef[name]` returns
   a TTerm wrapping a TAG_REF cell that, when reduced, lazily
   unfolds to the def's body via the ALO machinery, so a
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
TDefGet::usage = "TDefGet[name] returns the registered body of a def as a book-heap TTerm, or Missing[\"UnregisteredDef\"] if the slot is empty.  Accepts an integer slot or a string name.";
TDefExpr::usage = "TDefExpr[name] returns the structural expression of a def's body, walking the BOOK heap (parallel to TTermExpr but for the immutable template).  Like TTermExpr, REF leaves stop expansion so self-referential defs render finitely.";
TDefTree::usage = "TDefTree[name] = ExpressionTree[TDefExpr[name]] - a Wolfram Tree of the def body for visual inspection.";

Begin["`Private`"];

(* String-name interning, maintained per-session.  TInit doesn't
   reset this; the underlying DEFS table is repopulated on every
   TDef call so stale slot ids are fine to keep around. *)
$defNames = <||>
$defNext = 0

TDefName[s_String] /; KeyExistsQ[$defNames, s] := $defNames[s]
TDefName[s_String] := With[{slot = $defNext},
    $defNames[s] = slot;
    $defNext = slot + 1;
    slot
]
TDefName[i_Integer] := i

TDefSlots[] := $defNames

(* Library function bindings, loaded lazily like the others. *)
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

(* TDefGet[name]: read the registered body Term out of the C-side
   DEFS table.  Returns a TTerm wrapping a book-heap term (val
   points into the book heap, so structural traversal must use
   $bookReadFn, see TDefExpr / TDefTree) or Missing[..] when
   the slot was never registered. *)
TDefGet[name_] := (
    ensureInit[];
    With[{raw = $defGetFn[TDefName[name]]},
        If[ raw === 0,
            Missing["UnregisteredDef", name],
            TTerm[raw]
        ]
    ]
)

TDefExpr[name_] := With[{body = TDefGet[name]},
    If[ MatchQ[body, _TTerm],
        tTreeWalkWith[$bookReadFn, body, <||>],
        body
    ]
]

TDefTree[name_] := With[{e = TDefExpr[name]},
    If[ MatchQ[e, _Missing], e, ExpressionTree[e]]
]

End[];

EndPackage[];
