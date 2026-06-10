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

GeneralUtilities`SetUsage[TDef, "TDef[name$, body$] registers body$ as a named definition reachable via TRef[name$], and returns name$.
name$ may be an integer slot (0..255) or a string (auto-mapped to a stable slot); body$ is a TTerm built by the usual constructors. The body is snapshot into a static template heap, so later dynamic-heap mutations do not affect the def."];
GeneralUtilities`SetUsage[TRef, "TRef[name$] returns a TTerm wrapping a TAG_REF that lazily unfolds to the body registered under name$.
Reducing it via TWnf walks one ALO layer per fire (matching HVM4's REF-to-ALO unfolding)."];
GeneralUtilities`SetUsage[TDefName, "TDefName[name$] returns the integer slot the string name$ is mapped to, interning it on first use; an integer name$ is returned unchanged."];
GeneralUtilities`SetUsage[TDefSlots, "TDefSlots[] returns the current name-to-slot table as an Association."];
GeneralUtilities`SetUsage[TDefGet, "TDefGet[name$] returns the registered body of a def as a book-heap TTerm, or Missing[\"UnregisteredDef\"] if the slot is empty.
name$ may be an integer slot or a string name."];
GeneralUtilities`SetUsage[TDefExpr, "TDefExpr[name$] returns the structural expression of a def's body, walking the book heap (parallel to TTermExpr but for the immutable template).
Like TTermExpr, REF leaves stop expansion so self-referential defs render finitely."];
GeneralUtilities`SetUsage[TDefTree, "TDefTree[name$] returns ExpressionTree[TDefExpr[name$]], a Wolfram Tree of the def body for visual inspection."];

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
