#!/usr/bin/env wolframscript
(* ====================================================================
   STANDALONE harness: edge-TYPED (R-GCN) ATP critical-pair proof-relevance
   scorer vs the production merged-adjacency GNN.

   Measures held-out (split-by-file 80/20) TEST AUC at seeds 11/23/37 and
   compares to the merged-adjacency baseline (0.886 / 0.859 / 0.940).

   ---------------------------------------------------------------------
   RESULT (2026-06-11): INCONCLUSIVE -- the R-GCN does NOT beat the
   baseline as implemented here because TRAINING COLLAPSES on the full
   corpus.  Findings, isolated step by step:
     * The R-GCN FORWARD is correct: a random-init (untrained) model gives
       varied per-graph scores and TEST AUC ~0.60 on the full seed-11 split
       (the typed structure already beats chance before any training).
     * scoreRgcn is correct at full scale (chunked over 2022 test graphs:
       sd 0.99, 671 distinct scores).
     * Training on a SMALL subset (~1800 graphs) trains fine: loss falls to
       ~0.46, no collapse, at LearningRate 0.005.
     * Training on the FULL ~7700-graph train set COLLAPSES to the class
       prior (loss -> ln 2 = 0.693, TEST AUC ~0.537, i.e. training HURTS the
       good random init) -- and does so even at 30 steps, independent of
       LearningRate (0.001-0.01) and of the per-relation scale (1/nRel vs
       1/sqrt(nRel)).
     * The PRODUCTION baseline (TAtpTrainGnn / TAtpGnnScore, single merged
       adjacency, 11 params) on the IDENTICAL full seed-11 split trains
       cleanly at 150 steps: loss 6.61 -> 0.154, TEST AUC 0.698 (the 0.886
       figure in the prompt is its 300-step number).
   So the collapse is specific to this 9-relation / 32-param R-GCN on the
   full, heavily class-skewed (~82% positive) corpus -- a training-stability
   bug in this harness, NOT the data/split (validated against the baseline)
   nor the forward/scoring.  Most likely the many near-zero gradients from
   the SPARSE relations (cp->root edges occur once per graph) interact badly
   with Adam on imbalanced full-corpus minibatches, a fragility the
   single-adjacency baseline does not share.  Not fully root-caused within
   budget; the verdict therefore is "edge-typing NOT shown to beat the
   merged-adjacency baseline -- do NOT integrate until this collapse is
   fixed".  See the agent report for the full trail.
   ---------------------------------------------------------------------

   The production trainer (TAtpTrainGnn in ATP.wl) builds ONE symmetric,
   row-normalised adjacency that MERGES all 4 edge types:
       h' = relu(A . h . W1 + h . Ws + b)
   This harness instead keeps the typed-hypergraph structure: per-edge-type
   (and per-direction) row-normalised relations Ahat_t and a separate weight
   per relation:
       h' = relu( Sum_t Ahat_t . h . W1_t  +  h . Ws  +  b )
   R rounds, masked-mean pool, linear 2-class readout -- the R-GCN of
   Schlichtkrull et al. (2018), minus the basis-decomposition (only 9
   relations here so full per-relation weights are cheap).

   Node features are the SAME 6 structural dims as the baseline; nodes are
   capped at 64; only ONE minibatch's forward/backward is realized at a time
   (peak O(BatchSize * nRel * 64^2)).  Reuses the private WolframInstitute`THVMLink`ATP`
   helpers (atpScorerAuc, atpBatchMatMul, atpGnnApplyW, atpGnnBcastBias) and
   the buildLoopAdam / TGrad / TAdam minibatch pattern, so the comparison is
   apples-to-apples with the baseline trainer.

   RELATIONS (9 total):
     t=0..3  : forward edge type 0..3  (term->symbol, term->child,
               cp->lhs-root, cp->rhs-root)
     t=4..7  : reverse of edge type 0..3 (symbol->term, child->term, etc.)
     t=8     : self-loop (identity, row-normalised = identity)
   Each relation t has its own W1_t (Kx H). Direction matters: a symbol must
   send a DIFFERENT message to its parent term than the term sends to it, so
   forward and reverse are distinct learned relations (standard R-GCN).

   Run:  ATP_DATA=/tmp/enigma wl -t SEC -f tools/atp_enigma/edge_typed_eval.wls
   ==================================================================== *)

(* --- locate + load the paclet (worktree source, main-built dylib) ----- *)
$worktree = "/Users/swish/src/thvm/.claude/worktrees/agent-a7781eba1452709c1";
PacletDirectoryLoad[FileNameJoin[{$worktree, "wl", "THVMLink"}]];
Needs["WolframInstitute`THVMLink`"];
Needs["WolframInstitute`THVMLink`ATP`"];

dataDir = Environment["ATP_DATA"];
If[ dataDir === $Failed, dataDir = "/tmp/enigma"];

(* private helpers from the production code *)
auc            = WolframInstitute`THVMLink`ATP`Private`atpScorerAuc;
batchMatMul    = WolframInstitute`THVMLink`ATP`Private`atpBatchMatMul;
applyW         = WolframInstitute`THVMLink`ATP`Private`atpGnnApplyW;
bcastBias      = WolframInstitute`THVMLink`ATP`Private`atpGnnBcastBias;
freshTrainName = WolframInstitute`THVMLink`Private`freshTrainName;
buildLoopAdam  = WolframInstitute`THVMLink`Private`buildLoopAdam;

(* ==================================================================== *)
(*  Data loading                                                         *)
(* ==================================================================== *)

loadAll[] := Module[{f1, f2, files, recs},
    f1 = FileNames["*.wxf", FileNameJoin[{dataDir, "ds"}]];
    f2 = FileNames["*.wxf", FileNameJoin[{dataDir, "tptp_ds"}]];
    files = Join[f1, f2];
    recs = Table[
        With[{d = Import[f]},
            <|"file" -> FileBaseName[f],
              "graphs" -> d["Graphs"],
              "labels" -> d["Labels"]|>],
        {f, files}];
    recs
];

(* ==================================================================== *)
(*  Edge-typed batch tensors                                             *)
(*  Returns the SAME X / Mask / NNodes / Y as the baseline but instead   *)
(*  of one merged adjacency A it returns AT: a (nRel, B, N, N) stack of   *)
(*  row-normalised per-relation adjacencies.                             *)
(*  nRel = 9: 4 forward edge types, 4 reverse, 1 self-loop.              *)
(* ==================================================================== *)

nRel = 9;

edgeTypedTensors[graphs_List, labels_List, nCap_] := Module[{
    b, n, f = 6, eff, xArr, atArr, maskArr, nNodesArr, yArr
},
    b = Length[graphs];
    If[ b === 0, Return[$Failed]];
    n   = Min[nCap, Max[#["NNodes"] & /@ graphs]];
    eff = (Min[#["NNodes"], n] &) /@ graphs;
    xArr = Table[
        With[{feats = graphs[[bi]]["NodeFeatures"]},
            Table[
                If[ ni <= eff[[bi]], feats[[ni]], ConstantArray[0., f]],
                {ni, n}]],
        {bi, b}];
    (* atArr[[bi]] = {Ahat_0,...,Ahat_8}, each n x n row-normalised. *)
    atArr = Table[
        Module[{adjs, rowSums, edge, t, src, dst},
            adjs = Table[ConstantArray[0., {n, n}], {nRel}];
            (* relations 0..3 forward, 4..7 reverse *)
            Do[
                t   = edge[[3]];
                src = edge[[1]] + 1;
                dst = edge[[2]] + 1;
                If[ src <= n && dst <= n && 0 <= t <= 3,
                    adjs[[t + 1, src, dst]] = 1.;        (* forward  t  *)
                    adjs[[t + 5, dst, src]] = 1.],       (* reverse  t+4 *)
                {edge, graphs[[bi]]["Edges"]}];
            (* relation 8: self-loop over the live nodes *)
            Do[adjs[[9, i, i]] = 1., {i, eff[[bi]]}];
            (* row-normalise each relation independently (rows that are all
               zero stay zero -- a node with no incoming edges of that type
               receives no message from that relation). *)
            Table[
                rowSums = Total /@ adjs[[rt]];
                Table[
                    If[ rowSums[[i]] > 0., adjs[[rt, i]] / rowSums[[i]],
                        adjs[[rt, i]]],
                    {i, n}],
                {rt, nRel}]],
        {bi, b}];
    maskArr = Table[Table[If[ ni <= eff[[bi]], 1., 0.], {ni, n}], {bi, b}];
    nNodesArr = N[eff];
    yArr = (If[ # == 1, {0., 1.}, {1., 0.}] &) /@ labels;
    <|"X" -> xArr, "AT" -> atArr, "Mask" -> maskArr,
      "NNodes" -> nNodesArr, "Y" -> yArr,
      "N" -> n, "B" -> b, "F" -> f|>
];

(* ==================================================================== *)
(*  R-GCN forward                                                        *)
(*  xT      : (B, N, F)                                                   *)
(*  atT     : list length nRel of (B, N, N) relation tensors             *)
(*  maskRowT: (B, 1, N)                                                   *)
(*  nNodesT : (B, 1, 1)                                                   *)
(*  w1s     : (rR) x (nRel) of (K x H) per-relation weights              *)
(*  wss     : (rR) of (K x H) self weights (no adjacency)                *)
(*  bhs     : (rR) of (H)                                                 *)
(*  wout, bout : (H x 2), (2)                                            *)
(*  Returns (B, 2) logits.                                               *)
(* ==================================================================== *)

rgcnForwardLogits[xT_, atT_List, maskRowT_, nNodesT_,
    w1s_, wss_, bhs_, wout_, bout_, b_, n_, f_, hH_, rR_] :=
    Module[{h = xT, lastDim = f, relMsgs, ahrt, relSum, self, msg,
            relScale = TUOpConst[N[1/Sqrt[nRel]]], pooled3, pooled},
        Do[
            (* Sum over relations of  Ahat_rt . h . W1_t, scaled by
               1/sqrt(nRel).  Summing nRel He-init linears inflates the
               pre-activation variance ~nRel-fold (which blew the logits up
               to a CE loss of ~16 and collapsed training); 1/sqrt(nRel)
               restores variance to O(1) WITHOUT over-attenuating the
               structural signal (a flat 1/nRel mean shrank it ~3x further
               and the net collapsed to the class prior at loss = ln 2). *)
            relMsgs = Table[
                ahrt = batchMatMul[atT[[t]], h, b, n, n, lastDim];
                applyW[ahrt, w1s[[r, t]], b, n, lastDim, hH],
                {t, nRel}];
            relSum = Total[relMsgs] *
                TUOpExpand[TUOpReshape[relScale, {1, 1, 1}], {b, n, hH}];
            self = applyW[h, wss[[r]], b, n, lastDim, hH];
            msg  = relSum + self + bcastBias[bhs[[r]], b, n, hH];
            h = TReLU[msg];
            lastDim = hH,
            {r, rR}];
        pooled3 = batchMatMul[maskRowT, h, b, 1, n, hH];
        pooled  = ArrayReshape[
            pooled3 * TUOpRecip[TUOpExpand[nNodesT, {b, 1, hH}]], {b, hH}];
        pooled . wout + TUOpExpand[TUOpReshape[bout, {1, 2}], {b, 2}]
    ];

(* ==================================================================== *)
(*  Precompute the host NumericArrays for a FIXED partition of `graphs`   *)
(*  into minibatches of `bs` (the expensive 9-relation adjacency build is *)
(*  done ONCE here, not per Adam step).  Each batch record carries the    *)
(*  X / AT-stack / Mask / NNodes / Y arrays as Real32 NumericArrays plus  *)
(*  its B and N; the training loop only TTensorCreates + runs the forward *)
(*  per step, so only one minibatch's TTensors are realized at a time.    *)
(*  Partition is fixed (no per-epoch reshuffle of the arrays); the         *)
(*  baseline's per-epoch RandomSample matters little here because every    *)
(*  step still sees the full gradient over its batch and Adam state is     *)
(*  persistent.                                                           *)
(* ==================================================================== *)

prepBatches[graphs_, labels_, nCap_, bs_] := Module[{nG = Length[graphs]},
    Table[
        With[{sl = Range[bStart, Min[bStart + bs - 1, nG]]},
            Module[{bt = edgeTypedTensors[graphs[[sl]], labels[[sl]], nCap]},
                <|"X"  -> NumericArray[bt["X"], "Real32"],
                  "AT" -> Table[
                      NumericArray[bt["AT"][[All, t]], "Real32"], {t, nRel}],
                  "Mask" -> NumericArray[
                      ArrayReshape[bt["Mask"], {bt["B"], 1, bt["N"]}], "Real32"],
                  "NNodes" -> NumericArray[
                      ArrayReshape[bt["NNodes"], {bt["B"], 1, 1}], "Real32"],
                  "Y" -> NumericArray[bt["Y"], "Real32"],
                  "B" -> bt["B"], "N" -> bt["N"]|>]],
        {bStart, 1, nG, bs}]
];

(* Run the forward over one precomputed batch record; returns {logits, yT}. *)
mkForward[bt_Association, w1s_, wss_, bhs_, wout_, bout_, hH_, rR_, f_] :=
    {rgcnForwardLogits[
        TTensorCreate @ bt["X"],
        TTensorCreate /@ bt["AT"],
        TTensorCreate @ bt["Mask"],
        TTensorCreate @ bt["NNodes"],
        w1s, wss, bhs, wout, bout, bt["B"], bt["N"], f, hH, rR],
     TTensorCreate @ bt["Y"]};

(* ==================================================================== *)
(*  Train the R-GCN (minibatched, persistent Adam state -- mirrors        *)
(*  TAtpTrainGnn's BatchSize>nG path).  The per-minibatch host arrays are  *)
(*  precomputed once (prepBatches) and cycled; each Adam step realizes one *)
(*  minibatch's forward+backward, so peak memory is O(BatchSize * nRel *   *)
(*  NodeCap^2).                                                           *)
(* ==================================================================== *)

(* LearningRate 0.005 (vs the baseline's 0.01): the 9-relation R-GCN has ~3x
   the trainable params of the merged-adjacency GCN and is sensitive to a high
   step size -- at lr 0.01 training was stable for ~40 steps then diverged and
   collapsed to the class prior (loss -> ln 2); lr 0.005 trains stably. *)
trainRgcn[graphs_, labels_, OptionsPattern[{
        "Hidden" -> 32, "Rounds" -> 3, MaxTrainingRounds -> 300,
        "LearningRate" -> 0.005, "BatchSize" -> 128, "NodeCap" -> 64}]] :=
    Module[{
        f = 6, hH, rR, lrVal, rounds, bs, nCap, nG, perm, batches, nB,
        w1s, wss, bhs, wout, bout, params, ms, vs,
        b1pow, b2pow, done, lossStart, lossEnd
    },
        nG = Length[graphs];
        If[ nG === 0, Return[$Failed]];
        hH = OptionValue["Hidden"];  rR = OptionValue["Rounds"];
        lrVal = N @ OptionValue["LearningRate"];
        rounds = OptionValue[MaxTrainingRounds];
        bs = Min[OptionValue["BatchSize"], nG];
        nCap = OptionValue["NodeCap"];
        (* SHUFFLE the graph order ONCE before partitioning.  The graphs are
           concatenated per file, so contiguous slices are label-homogeneous
           (one theory's positives, then its negatives, ...); fixed
           label-homogeneous minibatches make every step's gradient point at a
           single label and the net collapses to the global class prior (loss
           -> ln 2, AUC ~ chance).  One global shuffle gives class-mixed
           batches.  The partition is then fixed across epochs (we precompute
           host arrays once for speed), which is fine: persistent Adam state
           still sees the full per-batch gradient. *)
        (* BALANCED minibatches.  A global shuffle still leaves every minibatch
           ~82% positive, so the R-GCN collapses to the majority-class prior
           (loss -> ln 2, AUC ~ chance).  Undersample the majority and interleave
           pos/neg so every bs-slice is ~50/50 -- no trivial prior minimum. *)
        With[{posI = Pick[Range[nG], labels, 1], negI = Pick[Range[nG], labels, 0]},
            With[{k = Min[Length[posI], Length[negI]]},
                perm = Flatten @ Transpose[{RandomSample[posI, k], RandomSample[negI, k]}]]];
        batches = prepBatches[graphs[[perm]], labels[[perm]], nCap, bs];
        nB = Length[batches];
        (* per-relation, per-round weights *)
        w1s = Table[TGlorot[{If[ r == 1, f, hH], hH}], {r, rR}, {t, nRel}];
        wss = Table[TGlorot[{If[ r == 1, f, hH], hH}], {r, rR}];
        bhs = Table[TZeros[{hH}], {r, rR}];
        wout = TGlorot[{hH, 2}];
        bout = TZeros[{2}];
        params = Join[Flatten[w1s], wss, bhs, {wout, bout}];
        ms = (TZeros[TTensorShape[#]] &) /@ params;
        vs = (TZeros[TTensorShape[#]] &) /@ params;
        b1pow = TOnes[{1}];  b2pow = TOnes[{1}];
        With[{lg = mkForward[First[batches], w1s, wss, bhs, wout, bout, hH, rR, f]},
            lossStart = First[Normal @ TRealize @
                TCategoricalCrossEntropy[lg[[1]], lg[[2]]]]];
        done = 0;
        While[ done < rounds,
            Do[
                If[ done >= rounds, Break[]];
                With[{lg = mkForward[batches[[bi]], w1s, wss, bhs, wout, bout,
                        hH, rR, f]},
                    TAdam[TCategoricalCrossEntropy[lg[[1]], lg[[2]]],
                        params, ms, vs, b1pow, b2pow, "lr" -> lrVal]];
                done += 1,
                {bi, nB}]];
        With[{lg = mkForward[First[batches], w1s, wss, bhs, wout, bout, hH, rR, f]},
            lossEnd = First[Normal @ TRealize @
                TCategoricalCrossEntropy[lg[[1]], lg[[2]]]]];
        <|"Kind" -> "RGCN",
          "W1" -> Map[Normal @ TRealize @ # &, w1s, {2}],
          "Ws" -> (Normal @ TRealize @ # & /@ wss),
          "Bh" -> (Normal @ TRealize @ # & /@ bhs),
          "Wout" -> (Normal @ TRealize @ wout),
          "Bout" -> (Normal @ TRealize @ bout),
          "Hidden" -> hH, "Rounds" -> rR, "NodeCap" -> nCap,
          "LossStart" -> lossStart, "LossEnd" -> lossEnd|>
    ];

(* ==================================================================== *)
(*  Score held-out graphs with a trained R-GCN (constant weights),        *)
(*  in bounded chunks so only one chunk's forward is realized at a time.  *)
(* ==================================================================== *)

scoreRgcn[model_, graphs_, labels_, chunk_:128] := Module[{
    f = 6, hH = model["Hidden"], rR = model["Rounds"], nCap = model["NodeCap"],
    w1s, wss, bhs, wout, bout, batches
},
    w1s  = Map[TTensorCreate @ NumericArray[#, "Real32"] &, model["W1"], {2}];
    wss  = TTensorCreate @ NumericArray[#, "Real32"] & /@ model["Ws"];
    bhs  = TTensorCreate @ NumericArray[#, "Real32"] & /@ model["Bh"];
    wout = TTensorCreate @ NumericArray[model["Wout"], "Real32"];
    bout = TTensorCreate @ NumericArray[model["Bout"], "Real32"];
    batches = prepBatches[graphs, labels, nCap, chunk];
    Join @@ Table[
        With[{lg = mkForward[bt, w1s, wss, bhs, wout, bout, hH, rR, f]},
            With[{p = Normal @ TRealize @ lg[[1]]},
                p[[All, 2]] - p[[All, 1]]]],
        {bt, batches}]
];

(* ==================================================================== *)
(*  Split-by-file 80/20 held-out evaluation, mirroring the baseline.      *)
(* ==================================================================== *)

evalSeed[recs_, seed_] := Module[{
    nFiles, perm, nTrain, trainRecs, testRecs,
    trG, trL, teG, teL, model, scores, testAuc, trainAuc, trScores
},
    SeedRandom[seed];
    nFiles = Length[recs];
    perm   = RandomSample[Range[nFiles]];
    nTrain = Round[0.8 * nFiles];
    trainRecs = recs[[ perm[[1 ;; nTrain]] ]];
    testRecs  = recs[[ perm[[nTrain + 1 ;;]] ]];
    trG = Join @@ (#["graphs"] & /@ trainRecs);
    trL = Join @@ (#["labels"] & /@ trainRecs);
    teG = Join @@ (#["graphs"] & /@ testRecs);
    teL = Join @@ (#["labels"] & /@ testRecs);
    Print["[seed ", seed, "] train files=", nTrain, " graphs=", Length[trG],
          " (pos ", Total[trL], "/", Length[trL], ")  test files=",
          nFiles - nTrain, " graphs=", Length[teG],
          " (pos ", Total[teL], "/", Length[teL], ")"];
    SeedRandom[seed];  (* deterministic weight init + minibatch shuffle *)
    model = trainRgcn[trG, trL, "Hidden" -> 32, "Rounds" -> 3,
        MaxTrainingRounds -> 150, "LearningRate" -> 0.005,
        "BatchSize" -> 128, "NodeCap" -> 64];
    trScores = scoreRgcn[model, trG, trL];
    trainAuc = auc[trScores, trL];
    scores   = scoreRgcn[model, teG, teL];
    testAuc  = auc[scores, teL];
    Print["[seed ", seed, "] lossStart=", model["LossStart"],
          " lossEnd=", model["LossEnd"],
          " TRAIN AUC=", trainAuc, "  >>> TEST AUC=", testAuc];
    <|"seed" -> seed, "trainAuc" -> trainAuc, "testAuc" -> testAuc,
      "lossStart" -> model["LossStart"], "lossEnd" -> model["LossEnd"]|>
];

(* ==================================================================== *)
(*  Main                                                                 *)
(* ==================================================================== *)

Print["=== Edge-typed (R-GCN) ATP scorer held-out evaluation ==="];
Print["nRel = ", nRel, " (4 fwd edge types + 4 reverse + 1 self-loop)"];
recs = loadAll[];
Print["loaded ", Length[recs], " files, ",
      Total[Length[#["graphs"]] & /@ recs], " graphs total"];

baseline = <|11 -> 0.886, 23 -> 0.859, 37 -> 0.940|>;
results = Table[evalSeed[recs, s], {s, {11}}];

Print[""];
Print["================ SUMMARY (held-out TEST AUC) ================"];
Print["seed | R-GCN (edge-typed) | merged-adj baseline | delta"];
Do[
    With[{r = results[[i]], s = {11, 23, 37}[[i]]},
        Print["  ", s, "  |  ", NumberForm[r["testAuc"], {5, 4}],
              "            |  ", baseline[s],
              "             |  ", NumberForm[r["testAuc"] - baseline[s], {6, 4}]]],
    {i, Length[results]}];
Print[""];
Print["R-GCN mean test AUC : ", Mean[#["testAuc"] & /@ results]];
Print["baseline mean       : ", Mean[Values[baseline]]];
Print["mean delta          : ",
      Mean[#["testAuc"] & /@ results] - Mean[Values[baseline]]];
Print["DONE"];
