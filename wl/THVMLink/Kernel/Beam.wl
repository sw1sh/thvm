(* ::Package:: *)
(* Beam.wl - BEAM-style kernel-variant search via SUP-collapse.

   Tinygrad's BEAM picks among kernel-codegen variants (tile sizes,
   vector widths, unroll factors) by actually running each on a
   calibration input and timing it.  The IC-flavored framing: a
   SUP[label, v0, v1, ..., v(K-1)] over K candidates collapses to the
   lowest-microseconds branch; subsequent calls hit that branch
   directly.

   This file ships the WL-side of that idea: TBeamPick takes a
   list of "candidate functions" (each a Function[] that produces
   the same TTerm result), times each via TJit + AbsoluteTiming,
   caches the winner index, and replays only the winner from then
   on.  Each candidate runs under its OWN TJit closure so capture-
   replay amortizes the timing cost across the calibration phase
   too.

   Public surface
     TBeamPick[fns_List]              - HoldFirst.  Returns a
                                        closure that, on first
                                        call, runs each candidate
                                        once, times them, and
                                        locks in the winner.
                                        Subsequent calls dispatch
                                        only the winner via TJit
                                        replay.

     TBeamReport[closure]             - the full per-candidate
                                        timing record (Association
                                        keyed by index).

     TBeamWinner[closure]             - which index won.

     TBeamReset[closure]              - discard the cached winner;
                                        next call re-runs the
                                        calibration (useful when
                                        input shapes change).

   The candidate functions all produce TTerms.  TBeamPick
   internally TJit-wraps each, so the timing measurement is for
   the captured kernel sequence, which is what BEAM needs to
   compare cleanly (no scheduler overhead in the timed path). *)

BeginPackage["THVMLink`"];

GeneralUtilities`SetUsage[TBeamPick, "TBeamPick[{fn$1, fn$2, $$}] returns a closure that, on first call, runs each candidate once, times them via TJit and AbsoluteTiming, and locks in the fastest; subsequent calls dispatch only the winner.
Each fn$i is a no-arg Function[{}, body] producing the comparison result via in-place TSet, so candidates can write to the same output buffer."];
GeneralUtilities`SetUsage[TBeamReport, "TBeamReport[closure$] returns an Association of per-candidate timings (index to microseconds) for the closure, or an empty Association if calibration has not run yet."];
GeneralUtilities`SetUsage[TBeamWinner, "TBeamWinner[closure$] returns the 1-indexed index of the winning candidate, or 0 if calibration has not run yet."];
GeneralUtilities`SetUsage[TBeamReset, "TBeamReset[closure$] clears the closure's cached winner so the next call re-runs the calibration."];
GeneralUtilities`SetUsage[TBeamClosure, "TBeamClosure[assoc$] is the wrapped form returned by TBeamPick; treat it as opaque and invoke it through the documented surface."];

(* Forward-decl symbols owned by later-loading siblings (Jit.wl).
   Without this, a bare `TJitClosure` reference inside Begin["`Private`"]
   below resolves to THVMLink`Private`TJitClosure (a fresh phantom)
   instead of the public symbol Jit.wl declares. *)
{TJit, TJitClosure, TJitDrop};

Begin["`Private`"];

(* Side-store of {key -> <|"winner", "timings", "jits"|>} keyed by
   the closure association's hash.  Same idiom as Jit.wl: the
   closure value is immutable, state lives here. *)
$tBeamState = <||>

TBeamPick[fns_List] := TBeamClosure[<| "fns" -> fns |>]

(* On first call: TJit-wrap each candidate, fire each once on the
   passed args, time it, pick the lowest-us index.  Stash both the
   TJit closures (so replay is fast) and the per-candidate timings
   in the side-store. *)
TBeamClosure[a_Association][args___] := Module[{
    key = Hash[a],
    rec, jits, timings, winnerIdx, fns, n
},
    rec = $tBeamState[key];
    If[ !MissingQ[rec],
        rec["jits"][[rec["winner"]]][args];
        Null,

        fns = a["fns"];
        n = Length[fns];
        (* Build one TJitClosure per candidate fn, bypassing TJit's
           HoldFirst (Map already evaluated list elements before we
           reach here).  TJitClosure expects an "fn" key whose value
           is `Function[argList, body]`; it calls fn[{args}] from
           the closure's invocation pattern, so we adapt each
           caller-supplied fn through this wrapper. *)
        jits = (
            caller |-> TJitClosure[<|
                "fn" -> (argList |-> caller @@ argList)
            |>]
        ) /@ fns;
        (* First call captures; that capture cost is also the
           calibration measurement, so all candidates pay it once.
           Replays will dispatch only the winner. *)
        timings = AssociationThread[
            Range[n],
            Table[
                Round[First @ AbsoluteTiming[jits[[i]][args]] * 1.0*^6, 1.0],
                {i, n}
            ]
        ];
        winnerIdx = First @ PositionSmallest[Values[timings]];
        $tBeamState[key] = <|
            "winner" -> winnerIdx,
            "timings" -> timings,
            "jits" -> jits
        |>;
        Null
    ]
]

TBeamReport[TBeamClosure[a_Association]] := Module[{rec},
    rec = $tBeamState[Hash[a]];
    If[ MissingQ[rec], <||>, rec["timings"]]
]

TBeamWinner[TBeamClosure[a_Association]] := Module[{rec},
    rec = $tBeamState[Hash[a]];
    If[ MissingQ[rec], 0, rec["winner"]]
]

TBeamReset[TBeamClosure[a_Association]] := Module[{key = Hash[a], rec},
    rec = $tBeamState[key];
    If[ !MissingQ[rec],
        TJitDrop /@ rec["jits"];
        $tBeamState = KeyDrop[$tBeamState, key]
    ];
    Null
]

End[];
EndPackage[];
