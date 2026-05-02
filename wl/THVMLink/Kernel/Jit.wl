(* ::Package:: *)
(* Jit.wl - capture / replay of a kernel-dispatch sequence.

   Phase 7 of the tinygrad-parity arc.  Per-iteration cost in a tight
   training loop (wl/Examples/beautiful-mnist/train.wls) is dominated
   by re-running the scheduler -- realize_classify, topo-sort,
   materialize -- on a graph whose shape is identical across
   iterations.  Only the input bytes change.

   TJit captures the kernel-dispatch sequence on the first call and
   replays it on subsequent calls.  Replay skips the scheduler
   entirely: it walks the recorded (kid, in_buf_ids, out_buf_id)
   tuples and dispatches each KernelEntry directly.  The dispatch
   target reads buffer bytes the caller refreshed (typically via
   TSet on a pre-allocated input TenDesc), so each replay computes
   against fresh data.

   Public surface
     TJit[fn]                     -- HoldFirst.  Returns a closure
                                     that captures fn on first call,
                                     replays on subsequent calls.
     TJitOpCount[closure]         -- captured op count (0 if not yet
                                     fired).
     TJitDrop[closure]            -- release the slot the closure
                                     holds; next call re-captures.

   The captured sequence is keyed only on slot id.  TJit does NOT
   track input shape changes -- if the wrapped fn's GRAPH SHAPE
   depends on inputs (e.g. branching on a Variable), the user
   should TJitDrop and re-capture when the shape changes. *)

BeginPackage["THVMLink`"];

TJit::usage        = "TJit[fn] returns a closure that captures fn's kernel-dispatch sequence on first call and replays it on subsequent calls.  Per-call wallclock drops from materialize+dispatch to just dispatch.  HoldFirst.  Recapture by TJitDrop[closure] then re-create.";
TJitOpCount::usage = "TJitOpCount[closure] returns the number of kernel dispatches captured for the JIT closure (0 before the first call).";
TJitCaptureOps::usage = "TJitCaptureOps[closure] returns the decoded captured TJit replay sequence as associations.  Dispatch rows include Kid, DispatchKind, ProgramKey, NInputs, OutBuf, Input0, Input1, OutputNumel, OpCount, ScalarUopCount, and TileUopCount; assign rows include DstTid and SrcTid.";
TJitCaptureRuns::usage = "TJitCaptureRuns[closure] groups a captured TJit replay sequence into consecutive dispatch runs split by assign records, with per-run dispatch-kind, program-key, output-size, and lowering summaries.";
TJitCaptureSummary::usage = "TJitCaptureSummary[closure] returns compact counts for a captured TJit replay sequence, including dispatch/assign counts, dispatch-kind counts, consecutive dispatch-run lengths, top program-key counts, and the largest dispatch runs.";
TJitDrop::usage    = "TJitDrop[closure] releases the JIT closure's capture slot.  After this, the closure re-captures on its next call.";
TJitClosure::usage = "TJitClosure[<|...|>] is the wrapped form returned by TJit -- treat as opaque; invoke through the documented surface.";

Begin["`Private`"];

(* Library-function loaders for the JIT bridge.  All return Integer;
   begin / end take no args, the others take a slot id.  Capture
   slots are 1-indexed; 0 means "no slot available" / "no capture
   active". *)
$jitCaptureBeginFn   := $jitCaptureBeginFn   = load["thvm_wl_jit_capture_begin",    {},        Integer]
$jitCaptureEndFn     := $jitCaptureEndFn     = load["thvm_wl_jit_capture_end",      {},        Integer]
$jitCaptureDropFn    := $jitCaptureDropFn    = load["thvm_wl_jit_capture_drop",     {Integer}, Integer]
$jitCaptureOpCountFn := $jitCaptureOpCountFn = load["thvm_wl_jit_capture_op_count", {Integer}, Integer]
$jitCaptureOpsFn     := $jitCaptureOpsFn     = load["thvm_wl_jit_capture_ops",      {Integer}, {Integer, 1}]
$jitReplayFn         := $jitReplayFn         = load["thvm_wl_jit_replay",           {Integer}, Integer]

(* Side-store of captured slot ids, keyed by the closure's
   association hash.  Each TJit closure starts un-captured; the
   first invocation runs through the capture path, allocates a slot,
   and parks (slot id, captured-flag) in this map.  Subsequent
   invocations of the SAME closure value (same hash) read the map
   and replay.

   Why a side store rather than mutating the closure: WL Set on a
   Module-bound symbol rebinds, but a TJit closure value held by
   the user can't be mutated from inside the call site without
   the user knowing the symbol name.  The side-store keys on the
   association content's hash, which is stable for the same
   structural closure. *)
$tJitState = <||>

(* TJit -- HoldFirst.  Returns a TJitClosure that wraps the fn.
   No slot is allocated until the first call so wrapping a function
   that's never invoked is free. *)
SetAttributes[TJit, HoldFirst]
TJit[fn_] := TJitClosure[<|
    "fn" -> Function[args, fn[Sequence @@ args]]
|>]

(* Direct invocation: dispatch through the side store.  Two paths:
     (a) closure has captured -> replay only
     (b) first call -> capture-begin, run fn, capture-end, store slot. *)
TJitClosure[a_Association][args___] := Module[{
    key = Hash[a],
    rec, slot, fnRes
},
    ensureInit[];
    rec = $tJitState[key];
    If[ !MissingQ[rec],
        (* (a) replay *)
        $jitReplayFn[rec["slot"]];
        Null,
        (* (b) capture *)
        slot = $jitCaptureBeginFn[];
        If[ slot === 0,
            Failure["TJit", <|
                "MessageTemplate" -> "TJit capture-slot pool full (cap = 16)"
            |>],
            Internal`WithLocalSettings[
                Null,
                fnRes = a["fn"][{args}],
                $jitCaptureEndFn[]
            ];
            $tJitState[key] = <|"slot" -> slot|>;
            fnRes
        ]
    ]
]

TJitOpCount[TJitClosure[a_Association]] := Module[{rec},
    rec = $tJitState[Hash[a]];
    If[ MissingQ[rec], 0, $jitCaptureOpCountFn[rec["slot"]]]
]

TJitCaptureOps[TJitClosure[a_Association]] := Module[{
    rec, raw, n, rowWidth
},
    rec = $tJitState[Hash[a]];
    If[ MissingQ[rec], Return[{}] ];
    raw = Normal @ $jitCaptureOpsFn[rec["slot"]];
    If[ Length[raw] < 2, Return[{}] ];
    n = raw[[1]];
    rowWidth = raw[[2]];
    Table[
        With[{base = 2 + (i - 1) * rowWidth},
            If[ raw[[base + 1]] === 0,
                <|
                    "Kind" -> "DISPATCH",
                    "Kid" -> raw[[base + 2]],
                    "DispatchKind" -> decodeDispatchKind[raw[[base + 3]]],
                    "NInputs" -> raw[[base + 4]],
                    "OutBuf" -> raw[[base + 5]],
                    "Input0" -> raw[[base + 6]],
                    "Input1" -> raw[[base + 7]],
                    "ProgramKey" -> raw[[base + 8]],
                    "OutputNumel" -> raw[[base + 9]],
                    "OpCount" -> raw[[base + 10]],
                    "ScalarUopCount" -> raw[[base + 11]],
                    "TileUopCount" -> raw[[base + 12]]
                |>,
                <|
                    "Kind" -> "ASSIGN",
                    "DstTid" -> raw[[base + 13]],
                    "SrcTid" -> raw[[base + 14]]
                |>
            ]
        ],
        {i, n}
    ]
]

captureCounts[rows_, key_] := ReverseSort @ Counts[
    Lookup[rows, key, 0] /. 0 -> Nothing]

TJitCaptureRuns[c_TJitClosure] := Module[{
    ops = TJitCaptureOps[c], runs = {}, cur = {}, start = 0,
    flush
},
    flush[end_] := If[Length[cur] > 0,
        AppendTo[runs, <|
            "Start" -> start,
            "End" -> end,
            "Count" -> Length[cur],
            "DispatchKindCounts" -> Counts[Lookup[cur, "DispatchKind", ""]],
            "TopProgramKeys" -> Take[captureCounts[cur, "ProgramKey"], UpTo[8]],
            "TopOutputNumels" -> Take[captureCounts[cur, "OutputNumel"], UpTo[8]],
            "TopOpCounts" -> Take[captureCounts[cur, "OpCount"], UpTo[8]],
            "ScalarLowered" -> Count[Lookup[cur, "ScalarUopCount", 0], _?(# > 0 &)],
            "TileLowered" -> Count[Lookup[cur, "TileUopCount", 0], _?(# > 0 &)],
            "TotalOutputNumel" -> Total[Lookup[cur, "OutputNumel", 0]]
        |>];
        cur = {}];
    Do[
        If[ops[[i]]["Kind"] === "DISPATCH",
            If[Length[cur] === 0, start = i];
            cur = Append[cur, ops[[i]]],
            flush[i - 1]
        ],
        {i, Length[ops]}];
    flush[Length[ops]];
    runs
]

TJitCaptureSummary[c_TJitClosure] := Module[{
    ops = TJitCaptureOps[c], dispatches, runs, programCounts
},
    dispatches = Select[ops, #["Kind"] === "DISPATCH" &];
    runs = TJitCaptureRuns[c];
    programCounts = captureCounts[dispatches, "ProgramKey"];
    <|
        "OpCount" -> Length[ops],
        "KindCounts" -> Counts[Lookup[ops, "Kind", ""]],
        "DispatchKindCounts" -> Counts[Lookup[dispatches, "DispatchKind", ""]],
        "DispatchRuns" -> Lookup[runs, "Count", {}],
        "RunCount" -> Length[runs],
        "TopProgramKeys" -> Take[programCounts, UpTo[12]],
        "TopOutputNumels" -> Take[captureCounts[dispatches, "OutputNumel"], UpTo[12]],
        "TopRuns" -> Take[ReverseSortBy[runs, #["Count"] &], UpTo[8]]
    |>
]

TJitDrop[TJitClosure[a_Association]] := Module[{key = Hash[a], rec},
    rec = $tJitState[key];
    If[ !MissingQ[rec],
        $jitCaptureDropFn[rec["slot"]];
        $tJitState = KeyDrop[$tJitState, key]];
    Null
]

End[];
EndPackage[];
