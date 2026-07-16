(* deepseek_decode.wlt - DeepSeek-V2-Lite decode step (NN/Examples/DeepSeekV2/Decode.wl)
   on the CPU backend: the 2-pass argmax kernels against a host argmax, and a
   scaled-down 2-layer decode pipeline (custom rope/cache/MLA3/route/moe_full
   kernels + native bf16 projections, replayed through TJit) against the
   independent host-precision reference (dsvRefStep) at bf16 tolerance. *)

Get["WolframInstitute`THVMLink`Examples`"]

$cfg = dsvConfig[
    "Dim" -> 64,
    "NLayers" -> 2,
    "NHeads" -> 4,
    "KvLora" -> 32,
    "DNope" -> 16,
    "DRope" -> 8,
    "DV" -> 16,
    "NExperts" -> 8,
    "NShared" -> 2,
    "TopK" -> 3,
    "MoeInter" -> 48,
    "DenseInter" -> 96,
    "Vocab" -> 256,
    "MaxSeq" -> 32
]

VerificationTest[
    TInit[]; TReset[];
    (* 2-pass argmax over a vocab that spans several partial blocks (vocab 1000
       -> 4 blocks x chunk 250), winner planted off the block boundaries *)
    Block[{acfg, k, lg, pv, pi, tok, host},
        acfg = dsvConfig["Vocab" -> 1000];
        k = dsvKernels[acfg];
        SeedRandom[11];
        host = RandomReal[{-1., 1.}, 1000];
        host[[777]] = 2.5;
        lg = TRealize @ TTensorCreate @ NumericArray[host, "Real32"];
        pv = TTensor[{acfg["ArgmaxBlocks"]}, "f32"];
        pi = TTensor[{acfg["ArgmaxBlocks"]}, "i32"];
        tok = TTensor[{1}, "i32"];
        k["ArgmaxPart"][pv, pi, lg];
        k["ArgmaxFin"][tok, pv, pi];
        First @ Normal @ TTensorData @ tok
    ],
    776,
    TestID -> "deepseek-decode/argmax-2pass"
]

VerificationTest[
    (* 6 self-fed decode steps through the captured TJit step; per-step logits
       vs the host reference (same synthetic bf16 weights, same input token
       stream) stay within bf16 tolerance *)
    state = dsvSynthInit[$cfg];
    ref = dsvRefInit[state];
    jit = dsvDecodeJit[state];
    diffs = Table[
        Block[{tokIn, refOut, logits},
            tokIn = dsvToken[state];
            refOut = dsvRefStep[ref, tokIn];
            ref = refOut["State"];
            logits = First @ Normal @ Last @ jit[];
            Max @ Abs[logits - refOut["Logits"]] / Max @ Abs[refOut["Logits"]]
        ],
        {6}
    ];
    Max[diffs] < 2.*^-2,
    True,
    TestID -> "deepseek-decode/2layer-vs-host-reference"
]

VerificationTest[
    (* the device length advanced once per step (6 steps from L = 1) and the
       fed-back token id is a valid vocab index *)
    {dsvPosition[state], 0 <= dsvToken[state] < $cfg["Vocab"]},
    {7, True},
    TestID -> "deepseek-decode/device-length-and-token"
]

VerificationTest[
    TReset[];
    True,
    True,
    TestID -> "deepseek-decode/cleanup"
]
