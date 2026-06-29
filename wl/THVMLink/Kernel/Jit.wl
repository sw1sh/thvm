(* ::Package:: *)
(* Jit.wl - capture / replay of a kernel-dispatch sequence.

   Per-iteration cost in a tight training loop
   (wl/Examples/beautiful-mnist/train.wls) is dominated by re-running
   the scheduler -- realize_classify, topo-sort, materialize -- on a
   graph whose shape is identical across iterations.  Only the input
   bytes change.

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

BeginPackage["WolframInstitute`THVMLink`", {"GeneralUtilities`"}];

SetUsage[TJit, "TJit[fn$] returns a closure that captures fn$'s kernel-dispatch sequence on the first call and replays it on later calls, returning the same result handle whose buffer each replay rewrites.
closure$[x$] passes per-call TTerm inputs: the first call captures over x$, and each later call rebinds its input in place (tinygrad's input_replace), so a fixed forward generates by closure$[next$] with no explicit TSet.
Per-call wallclock drops from materialize+dispatch to dispatch only. HoldFirst; recapture with TJitDrop[closure$] then re-create."];
SetUsage[TJitOpCount, "TJitOpCount[closure$] returns the number of kernel dispatches captured for the JIT closure (0 before the first call)."];
SetUsage[TJitCaptureOps, "TJitCaptureOps[closure$] returns the decoded captured TJit replay sequence as a list of associations.
Dispatch rows carry Kid, DispatchKind, ProgramKey, NInputs, OutBuf, Input0, Input1, OutputNumel, OpCount, ScalarUopCount, TileUopCount, ReplaySkip, and ReplayPacked; assign rows carry DstTid and SrcTid."];
SetUsage[TJitCaptureRuns, "TJitCaptureRuns[closure$] groups a captured TJit replay sequence into consecutive dispatch runs split by assign records, with per-run dispatch-kind, program-key, output-size, and lowering summaries."];
SetUsage[TJitCaptureGraphRuns, "TJitCaptureGraphRuns[closure$] groups a captured TJit replay sequence into Metal ICB-eligible replay chunks of live metal-tile dispatches (metal-alias records consumed but not encoded), split by assign and non-tile dispatch records."];
SetUsage[TJitCaptureSummary, "TJitCaptureSummary[closure$] returns compact counts for a captured TJit replay sequence: dispatch/assign counts, dispatch-kind counts, consecutive dispatch-run lengths, top program-key counts, and the largest dispatch runs."];
SetUsage[TJitDrop, "TJitDrop[closure$] releases the JIT closure's capture slot so the closure re-captures on its next call."];
SetUsage[TJitClosure, "TJitClosure[assoc$] is the wrapped form returned by TJit; treat it as opaque and invoke it through the documented surface."];

Begin["`Private`"];

(* Library-function loaders for the JIT bridge.  All return Integer;
   begin / end take no args, the others take a slot id.  Capture
   slots are 1-indexed; 0 means "no slot available" / "no capture
   active". *)
$jitCaptureBeginFn := $jitCaptureBeginFn = load["thvm_wl_jit_capture_begin", {}, Integer]
$jitCaptureEndFn := $jitCaptureEndFn = load["thvm_wl_jit_capture_end", {}, Integer]
$jitCaptureEndResultFn := $jitCaptureEndResultFn = load["thvm_wl_jit_capture_end_result", {Integer}, Integer]
$jitCaptureEndResultMultiFn := $jitCaptureEndResultMultiFn = load["thvm_wl_jit_capture_end_result_multi", {{Integer, 1}}, Integer]
$jitCaptureDropFn := $jitCaptureDropFn = load["thvm_wl_jit_capture_drop", {Integer}, Integer]
$jitCaptureOpCountFn := $jitCaptureOpCountFn = load["thvm_wl_jit_capture_op_count", {Integer}, Integer]
$jitCaptureOpsFn := $jitCaptureOpsFn = load["thvm_wl_jit_capture_ops", {Integer}, {Integer, 1}]
$jitReplayFn := $jitReplayFn = load["thvm_wl_jit_replay", {Integer}, Integer]
$jitCaptureSetInputsFn := $jitCaptureSetInputsFn = load["thvm_wl_jit_capture_set_inputs", {Integer, {Integer, 1}}, Integer]
$jitReplayWithInputsFn := $jitReplayWithInputsFn = load["thvm_wl_jit_replay_with_inputs", {Integer, {Integer, 1}}, Integer]

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
   association content's hash.

   The "id" key (a per-call Unique symbol) gives each closure its
   own identity: copying a closure value preserves the id, so the
   copy replays the original capture; but two closures built by
   separate TJit calls -- even over a structurally identical fn --
   get distinct ids, hence distinct hashes, so the second one's
   first call captures fresh instead of colliding on the first's
   slot and short-circuiting to a Null-returning replay. *)
$tJitState = <||>

(* TJit -- HoldFirst.  Returns a TJitClosure that wraps the fn.
   No slot is allocated until the first call so wrapping a function
   that's never invoked is free. *)
SetAttributes[TJit, HoldFirst]
TJit[fn_] := TJitClosure[<|
    "id" -> Unique["jit$"],
    "fn" -> Function[args, fn[Sequence @@ args]]
|>]

(* Direct invocation: dispatch through the side store.  Two paths:
     (a) closure has captured -> replay only
     (b) first call -> capture-begin, run fn, capture-end, store slot. *)
(* The TenDesc tids of the call's TTerm arguments, realized so each has a
   live buffer.  These are the per-call INPUTS: on capture they declare the
   input_replace baseline; on replay each call's fresh tids are substituted
   at the captured input sites, so passing a new input rebinds it with no
   explicit TSet.  No TTerm args (the slot-feeding TJit[fn] usage) -> {}. *)
jitArgTids[args_List] := TTermVal /@ TRealize /@ Cases[args, _TTerm]

(* One capture pass over the held fn: begin a slot, run the body, end+pin the
   result roots, declare the input-rebind sites.  Returns the state assoc
   <|slot, result|> or a Failure when the slot pool is full.

   The input args are REALIZED inside the capture scope (jit_is_capturing)
   so materialize_copy pins + caches each upload by copy-loc.  The fn body
   then re-realizes the same input arg (a TToDevice / lazy leaf) and HITS
   that cache, reusing the very buffer we declare as the input_replace
   baseline.  Without this, the input realize would happen OUTSIDE the
   capture (jit_is_capturing false -> uncached), and the fn body's own
   re-realize inside the capture would upload a SECOND, distinct buffer:
   the captured kernels would read that derived buffer while the declared
   input named the first one, so input_replace finds 0 rebind sites and a
   warm replay re-reads the capture-time bytes (the conv / im2col VAE
   replay collapse -- every gen reuses the first prompt's latent).  Realize
   once, inside the scope, so declared buffer == kernel-read buffer. *)
jitCaptureOnce[a_Association, args_List] := Module[{slot, fnRes, inTids},
    slot = $jitCaptureBeginFn[];
    If[ slot === 0,
        Failure["TJit", <|"MessageTemplate" -> "TJit capture-slot pool full (cap = 16)"|>],
        inTids = jitArgTids[args];
        Internal`WithLocalSettings[
            Null,
            fnRes = a["fn"][args],
            (* Mark EVERY returned tensor handle (a bare TTerm, or any
               in a list / nested structure) as a capture result, so all
               their output buffers are pinned + replayed.  A list return
               (TGrad over several params) needs each root pinned, else
               it is reclaimed as garbage. *)
            With[{roots = Cases[{fnRes}, t_TTerm :> ttermRaw[t], Infinity]},
                If[roots === {}, $jitCaptureEndResultFn[0], $jitCaptureEndResultMultiFn[roots]]]
        ];
        (* Declare this call's input tensors so later calls rebind them
           in place of an explicit TSet. *)
        If[inTids =!= {}, $jitCaptureSetInputsFn[slot, inTids]];
        <|"slot" -> slot, "result" -> fnRes|>
    ]
]

(* Direct invocation: dispatch through the side store.  Two paths:
     (a) closure has captured -> replay only
     (b) first call -> capture-begin, run fn, capture-end, store slot.

   POST-CAPTURE AUTOTUNE APPLY (tinygrad jit.py:289-300): under JITBEAM, the
   capture's jit_capture_autotune_finalized (C) beam-searches each captured
   kernel on isolated buffers and APPLIES the winning opt-config to the captured
   KernelEntry's schedule in place.  The replay's Metal ICB build looks up each
   kernel's PSO by a hash that folds its applied opts (metal_tile_jit_hash), so
   the FIRST replay -- which runs after this capture-end search -- builds the
   TUNED PSOs with no re-capture needed (the opts are already on the captured
   kernels).  This is the re-bake apply; re-capture is unnecessary and unsafe
   here (its fresh intermediate tids change the structural key, so its
   cache-load misses anyway). *)
TJitClosure[a_Association][args___] := Module[{key = Hash[a], rec, inTids, st},
    ensureInit[];
    rec = $tJitState[key];
    If[ ! MissingQ[rec]
        ,
        (* (a) replay: realize this call's inputs to get their fresh per-call
           buffers, substitute them at the captured sites (tinygrad
           input_replace), re-dispatch, and return the SAME result handle --
           the replay rewrites its pinned output buffer, so reading it
           surfaces the fresh result.  No inputs -> plain re-dispatch (the
           slot-feeding usage). *)
        inTids = jitArgTids[{args}];
        If[inTids === {}, $jitReplayFn[rec["slot"]], $jitReplayWithInputsFn[rec["slot"], inTids]];
        rec["result"]
        ,
        (* (b) capture: jitCaptureOnce realizes the inputs INSIDE the capture
           scope so the upload caches by copy-loc and the fn body reuses the
           declared input buffer (see jitCaptureOnce). *)
        st = jitCaptureOnce[a, {args}];
        If[ FailureQ[st],
            st,
            $tJitState[key] = st;
            st["result"]]
    ]
]

TJitOpCount[TJitClosure[a_Association]] := Module[{rec},
    rec = $tJitState[Hash[a]];
    If[MissingQ[rec], 0, $jitCaptureOpCountFn[rec["slot"]]]
]

TJitCaptureOps[TJitClosure[a_Association]] := Module[{rec, raw, n, rowWidth},
    rec = $tJitState[Hash[a]];
    If[MissingQ[rec], Return[{}]];
    raw = Normal @ $jitCaptureOpsFn[rec["slot"]];
    If[Length[raw] < 2, Return[{}]];
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
                    "TileUopCount" -> raw[[base + 12]],
                    "ReplaySkip" -> (rowWidth >= 15 && raw[[base + 15]] =!= 0),
                    "ReplayPacked" -> (rowWidth >= 16 && raw[[base + 16]] =!= 0)
                |>
                ,
                <|
                    "Kind" -> "ASSIGN",
                    "DstTid" -> raw[[base + 13]],
                    "SrcTid" -> raw[[base + 14]],
                    "ReplaySkip" -> (rowWidth >= 15 && raw[[base + 15]] =!= 0),
                    "ReplayPacked" -> False
                |>
            ]
        ]
        ,
        {i, n}
    ]
]

captureCounts[rows_, key_] := ReverseSort @ Counts[Lookup[rows, key, 0] /. 0 -> Nothing]

jitGraphRunLimit[] := Module[{raw = Environment["THVM_METAL_GRAPH_MAX_DISPATCHES"], n},
    If[! StringQ[raw] || StringLength[StringTrim[raw]] == 0, Return[256]];
    If[! StringMatchQ[StringTrim[raw], DigitCharacter ..], Return[256]];
    n = ToExpression[StringTrim[raw]];
    If[2 <= n <= 512, n, 256]
]

TJitCaptureRuns[c_TJitClosure] := Module[{ops = TJitCaptureOps[c], runs = {}, cur = {}, start = 0, flush},
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

TJitCaptureGraphRuns[c_TJitClosure] := Module[{ops = TJitCaptureOps[c], runs = {}, i = 1, n, j, consumed, aliases, skipped, encoded, tileCount, jitCount, blocker, blockerOp, op, kind, limit = jitGraphRunLimit[]},
    n = Length[ops];
    While[i <= n,
        j = i;
        consumed = 0;
        aliases = 0;
        skipped = 0;
        encoded = {};
        tileCount = 0;
        jitCount = 0;
        blocker = "end";
        blockerOp = <||>;
        While[j <= n && consumed < limit,
            op = ops[[j]];
            If[TrueQ[op["ReplaySkip"]],
                skipped++;
                consumed++;
                j++;
                Continue[]];
            If[op["Kind"] =!= "DISPATCH",
                blocker = "assign";
                blockerOp = op;
                Break[]];
            kind = op["DispatchKind"];
            Which[
                kind === "metal-alias",
                    aliases++;
                    consumed++;
                    j++,
                kind === "metal-tile",
                    encoded = Append[encoded, op];
                    tileCount++;
                    consumed++;
                    j++,
                True,
                    blocker = kind;
                    blockerOp = op;
                    Break[]]];
        If[Length[encoded] >= 2 && consumed >= 2,
            AppendTo[runs, <|
                "Start" -> i,
                "End" -> i + consumed - 1,
                "Consumed" -> consumed,
                "EncodedDispatches" -> Length[encoded],
                "TileDispatches" -> tileCount,
                "JitDispatches" -> jitCount,
                "AliasDispatches" -> aliases,
                "SkippedDispatches" -> skipped,
                "Blocker" -> blocker,
                "BlockerKid" -> Lookup[blockerOp, "Kid", 0],
                "BlockerProgramKey" -> Lookup[blockerOp, "ProgramKey", 0],
                "BlockerOutputNumel" -> Lookup[blockerOp, "OutputNumel", 0],
                "BlockerOpCount" -> Lookup[blockerOp, "OpCount", 0],
                "TopProgramKeys" -> Take[captureCounts[encoded, "ProgramKey"], UpTo[8]],
                "TopOutputNumels" -> Take[captureCounts[encoded, "OutputNumel"], UpTo[8]],
                "TopOpCounts" -> Take[captureCounts[encoded, "OpCount"], UpTo[8]],
                "ScalarLowered" -> Count[Lookup[encoded, "ScalarUopCount", 0], _?(# > 0 &)],
                "TileLowered" -> Count[Lookup[encoded, "TileUopCount", 0], _?(# > 0 &)],
                "TotalOutputNumel" -> Total[Lookup[encoded, "OutputNumel", 0]]
            |>];
            i += consumed,
            i++]];
    runs
]

TJitCaptureSummary[c_TJitClosure] := Module[{ops = TJitCaptureOps[c], dispatches, runs, graphRuns, programCounts, liveDispatches},
    dispatches = Select[ops, #["Kind"] === "DISPATCH" &];
    liveDispatches = Select[dispatches, !TrueQ[#["ReplaySkip"]] &];
    runs = TJitCaptureRuns[c];
    graphRuns = TJitCaptureGraphRuns[c];
    programCounts = captureCounts[dispatches, "ProgramKey"];
    <|
        "OpCount" -> Length[ops],
        "KindCounts" -> Counts[Lookup[ops, "Kind", ""]],
        "DispatchKindCounts" -> Counts[Lookup[dispatches, "DispatchKind", ""]],
        "ReplaySkipped" -> Count[Lookup[ops, "ReplaySkip", False], True],
        "ReplayPackedDispatches" -> Count[Lookup[dispatches, "ReplayPacked", False], True],
        "ReplayLiveDispatches" -> Length[liveDispatches],
        "DispatchRuns" -> Lookup[runs, "Count", {}],
        "RunCount" -> Length[runs],
        "GraphRunCount" -> Length[graphRuns],
        "GraphEncodedDispatches" -> Total[Lookup[graphRuns, "EncodedDispatches", {}]],
        "GraphTileDispatches" -> Total[Lookup[graphRuns, "TileDispatches", {}]],
        "GraphJitDispatches" -> Total[Lookup[graphRuns, "JitDispatches", {}]],
        "GraphAliasDispatches" -> Total[Lookup[graphRuns, "AliasDispatches", {}]],
        "GraphSkippedDispatches" -> Total[Lookup[graphRuns, "SkippedDispatches", {}]],
        "GraphRunEncodedCounts" -> Lookup[graphRuns, "EncodedDispatches", {}],
        "GraphRunTileCounts" -> Lookup[graphRuns, "TileDispatches", {}],
        "GraphBlockers" -> Counts[Lookup[graphRuns, "Blocker", {}]],
        "TopProgramKeys" -> Take[programCounts, UpTo[12]],
        "TopOutputNumels" -> Take[captureCounts[dispatches, "OutputNumel"], UpTo[12]],
        "TopRuns" -> Take[ReverseSortBy[runs, #["Count"] &], UpTo[8]],
        "TopGraphRuns" -> Take[ReverseSortBy[graphRuns, #["EncodedDispatches"] &], UpTo[8]]
    |>
]

TJitDrop[TJitClosure[a_Association]] := Module[{key = Hash[a], rec},
    rec = $tJitState[key];
    If[ ! MissingQ[rec],
        $jitCaptureDropFn[rec["slot"]];
        $tJitState = KeyDrop[$tJitState, key]];
    Null
]

End[];
EndPackage[];
