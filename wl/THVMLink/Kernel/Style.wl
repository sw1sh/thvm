(* ::Package:: *)
(* Style.wl - shared visual style table for all THVMLink graph
   renderers (THeapGraph, TScheduleGraph, THeapDiagram, the gantt).

   One source of truth: $nodeStyle[type] returns an Association with
   {"Fill", "Edge", "Shape", "TextColor"} so every renderer paints
   the same logical category the same way: LAM is always green,
   KERNEL is always orange, an external TEN reads as a darker cyan
   than a kernel-emitted TEN, etc.

   Each renderer composes these primitives into its own VertexShape
   function (drawNode below produces the standard rounded-rect /
   triangle / disk in shared style).  Renderers should NEVER define
   their own colors: if a new node category is needed, add an
   entry here and let every renderer pick it up.

   Type keys (strings, NOT $TagFOO ints):
     IC compounds : "LAM", "APP", "SUP", "DUP"
     IC leaves    : "ERA", "VAR", "DP0", "DP1", "NUM"
     Compute      : "UOP" (default for elementwise/movement/reduce),
                    "CONST", "REDUCE", "GRAD", "KERNEL"
     Tensors      : "TEN" (kernel-emitted output), "ExternalTEN"
                    (host-side TenDesc with no kernel producer) *)

BeginPackage["THVMLink`"];

Begin["`Private`"];

(* === single source of truth === *)

(* Shape primitives, fully positional so any renderer can drop them
   in.  pos = vertex centre; sz = {hw, hh} half-extent.  Shapes:
     "Rect"          axis-aligned rounded rectangle
     "TriangleDown"  apex at bottom (LAM, DUP)
     "TriangleUp"    apex at top    (APP, SUP)
     "Disk"          circle of radius Min[sz]
     "Ring"          unfilled circle (ERA) *)
$shapeDraw["Rect"]         := {pos, sz} |-> Rectangle[pos - sz, pos + sz, RoundingRadius -> 0.18 Min[sz]]
$shapeDraw["TriangleDown"] := {pos, sz} |-> Triangle[{pos + {-sz[[1]], sz[[2]]}, pos + {sz[[1]], sz[[2]]}, pos + {0, -sz[[2]]}}]
$shapeDraw["TriangleUp"]   := {pos, sz} |-> Triangle[{pos + {-sz[[1]], -sz[[2]]}, pos + {sz[[1]], -sz[[2]]}, pos + {0, sz[[2]]}}]
$shapeDraw["Disk"]         := {pos, sz} |-> Disk[pos, Min[sz]]
$shapeDraw["Ring"]         := {pos, sz} |-> {AbsoluteThickness[1.4], Circle[pos, Min[sz]]}
$shapeDraw[_]              := $shapeDraw["Rect"]

(* Color helpers.  All renderers use LightDarkSwitched so the same
   palette reads in both themes. *)
nodeFill[c_] := LightDarkSwitched[Lighter[c, 0.6], Darker[c, 0.4]]
nodeEdge[c_] := LightDarkSwitched[c, Lighter[c, 0.2]]

styleEntry[shape_String, color_] := <|
    "Fill"      -> nodeFill[color],
    "Edge"      -> nodeEdge[color],
    "Shape"     -> shape,
    "TextColor" -> LightDarkSwitched[Black, White]
|>

(* Type -> style.  A category named here is what every renderer
   asks for; if a renderer needs a finer split (e.g. PERMUTE vs
   ADD), it should still funnel back into one of these strings. *)
$nodeStyle = <|
    (* IC compounds *)
    "LAM"         -> styleEntry["TriangleDown", StandardGreen],
    "APP"         -> styleEntry["TriangleUp",   StandardBlue],
    "SUP"         -> styleEntry["TriangleUp",   StandardOrange],
    "DUP"         -> styleEntry["TriangleDown", StandardPurple],
    (* IC leaves *)
    "VAR"         -> styleEntry["Disk",         StandardGreen],
    "DP0"         -> styleEntry["Disk",         StandardPurple],
    "DP1"         -> styleEntry["Disk",         StandardPurple],
    "NUM"         -> styleEntry["Disk",         StandardGray],
    "ERA"         -> styleEntry["Ring",         StandardGray],
    (* Compute UOPs *)
    "UOP"         -> styleEntry["Rect",         StandardBlue],
    "CONST"       -> styleEntry["Disk",         StandardYellow],
    "REDUCE"      -> styleEntry["Rect",         StandardPurple],
    "GRAD"        -> styleEntry["Rect",         StandardRed],
    "KERNEL"      -> styleEntry["Rect",         StandardOrange],
    (* Tensor handles *)
    "TEN"         -> styleEntry["Rect",         StandardCyan],
    "ExternalTEN" -> styleEntry["Rect",         Darker[StandardCyan, 0.2]],
    (* Lazy / book / case nodes *)
    "REF"         -> styleEntry["Disk",         StandardYellow],
    "ALO"         -> styleEntry["TriangleDown", Darker[StandardYellow, 0.15]],
    "CTR"         -> styleEntry["TriangleDown", StandardRed],
    "MAT"         -> styleEntry["TriangleUp",   StandardRed],
    "OP2"         -> styleEntry["Rect",         StandardBlue],
    (* Dynamic-label SUP / DUP (HVM4 DSU / DDU): same shapes as
       SUP / DUP, darker shade marks them as the strict-on-label
       variant. *)
    "DSU"         -> styleEntry["TriangleUp",   Darker[StandardOrange, 0.25]],
    "DDU"         -> styleEntry["TriangleDown", Darker[StandardPurple, 0.25]]
|>

(* Lookup with a sensible fallback so a new tag accidentally
   missing an entry renders as a gray rectangle instead of crashing
   the renderer.  Callers always go through styleFor[]. *)
styleFor[type_String] := Lookup[$nodeStyle, type, $nodeStyle["UOP"]]

(* === shared text-inside-shape vertex draw ===
   pos, sz from the caller's VertexShapeFunction; type picks the
   style; label is anything Pane can render (string, Column, Row).
   Pane's "ShrinkToFit" makes long labels readable without an
   external Inset tweak. *)
drawNode[pos_, sz_, type_String, label_] := Block[{
    s = styleFor[type],
    isHollow = MemberQ[{"Ring"}, styleFor[type]["Shape"]]
},
    {
        If[ isHollow, {}, FaceForm[s["Fill"]]],
        EdgeForm[Directive[s["Edge"], AbsoluteThickness[1.5]]],
        $shapeDraw[s["Shape"]][pos, sz],
        If[ label === None || label === "" || isHollow,
            {},
            Inset[
                Pane[
                    Style[label,
                        FontFamily -> "Source Code Pro",
                        FontSize   -> 9,
                        FontColor  -> s["TextColor"]],
                    {Round[160 sz[[1]]], Round[140 sz[[2]]]},
                    ImageSizeAction -> "ShrinkToFit",
                    Alignment       -> Center
                ],
                pos, Center, {2 sz[[1]], 2 sz[[2]]}
            ]
        ]
    }
]

(* Convenience: build a VertexShapeFunction closure for a single
   (type, label) pair.  Renderers pass this as the rule body in
   `vid -> nodeShapeFn["KERNEL", "k1\nMUL+REDUCE"]`. *)
nodeShapeFn[type_String, label_] := {pos, vid, sz} |-> drawNode[pos, sz, type, label]

(* Common edge style (every renderer uses the same arrow look). *)
edgeStyleDirective := Directive[
    LightDarkSwitched[Darker[StandardBlue, 0.1], Lighter[StandardBlue, 0.2]],
    AbsoluteThickness[1.4]
]

End[];
EndPackage[];
