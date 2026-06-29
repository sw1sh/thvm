(* ::Package:: *)
(* Expr.wl - WL <-> TTerm coercion.

   Two non-lazy primitives that bridge native WL expressions and
   the IC-side TTerm representation:

     ToTTerm[expr]: pack a WL value into a TTerm:
                          Integer       -> NUM
                          Symbol        -> 0-ary CTR labelled by name
                          List          -> Tuple-CTR ($LazyTuple)
                          head[args__]  -> CTR labelled by Head
                          TTerm passes through unchanged.
     FromTTerm[t]: decode a TTerm to a WL value, walking the
                          heap lazily through TCnf when the cell
                          isn't already a WHNF leaf.

   These primitives are not lazy themselves: the laziness is in the
   Cons-stream structures the encoded values participate in.  They
   sit in their own file so Lazy.wl can focus on stream combinators.
   The underlying private workhorses (`tlazyEncode`, `tlazyDecode`,
   `tlazyDecodeRaw`, `tlazyConsToList`, `$LazyCons`, `$LazyNil`,
   `$LazyTuple`, `$lazySymLabel`, `$lazyLabelSym`, `symLabelFor`,
   `freshSymLabel`, `encodeAsConsList`, `consTerm`, `nilTerm`)
   remain in Lazy.wl so the TLazyXxx generators can still reach
   them. *)

BeginPackage["WolframInstitute`THVMLink`", {"GeneralUtilities`"}];

SetUsage[ToTTerm, "ToTTerm[expr$] packs a WL value into a TTerm.
An Integer becomes a NUM; a Symbol a 0-ary CTR labelled by the symbol name (interned via symLabelFor); a List a Tuple-CTR (label $LazyTuple); a compound head$[args$] a CTR labelled by Head with the encoded arguments as children. A TTerm argument passes through unchanged."];
SetUsage[FromTTerm, "FromTTerm[t$] decodes a TTerm t$ to a WL value: a NUM becomes an Integer; a CTR becomes the labelled symbol applied to its decoded children, a List, or similar. Opaque heads are auto-forced through TCnf so DP-rooted Cons cells fire their readback duplication."];

(* Forward refs to the workhorses still owned by Lazy.wl. *)
{tlazyEncode, tlazyDecode};

Begin["`Private`"];

ToTTerm[v_] := tlazyEncode[v]
FromTTerm[t_] := tlazyDecode[t]

End[];

EndPackage[];
