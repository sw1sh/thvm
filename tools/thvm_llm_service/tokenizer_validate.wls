#!/usr/bin/env wolframscript
(* tokenizer_validate.wls -- byte-exact check of the pure-Wolfram DeepSeek-V2
   tokenizer (wl/THVMLink/Kernel/NN/Examples/DeepSeekV2/Tokenizer.wl) against the
   HuggingFace transformers oracle.

   For every case it asserts
       dsvEncode[tok, s]        == HF.encode(s, add_special_tokens=False)
       dsvDecode[tok, hfIds]    == HF.decode(hfIds)
   and prints a per-case line plus a final "PASS k/k" for encode and decode.
   Exits nonzero if anything diverges.

   Run from the repo root:  wl -f tools/thvm_llm_service/tokenizer_validate.wls
   Optional first argument: the tokenizer snapshot directory (containing
   tokenizer.json); defaults to the cached DeepSeek-V2-Lite-Chat snapshot. *)

PacletDirectoryLoad["wl/THVMLink"];
Get["WolframInstitute`THVMLink`Examples`"];

args = Rest[$ScriptCommandLine];
tokDir = If[Length[args] >= 1 && DirectoryQ[args[[1]]],
    args[[1]],
    FileNameJoin[{$HomeDirectory, ".cache", "huggingface", "hub",
        "models--deepseek-ai--DeepSeek-V2-Lite-Chat", "snapshots",
        "85864749cd611b4353ce1decdb286193298f64c7"}]
];
jsonPath = FileNameJoin[{tokDir, "tokenizer.json"}];

If[ ! FileExistsQ[jsonPath],
    Print["tokenizer.json not found at ", jsonPath];
    Exit[1]
];

tok = dsvTokenizer[jsonPath];

tests = {
    "Hello world",
    "def f(x): return x+1",
    "  spaces  ",
    "café 北京 123",
    "line1\nline2",
    "The quick brown fox.",
    "3.14159 and 42",
    "col1\tcol2\tcol3",
    "I love 🍕!",
    " hi",
    "hi ",
    "",
    "Hello 世界, this is 北京 in 2024!",
    "<｜begin▁of▁sentence｜>User: Hi\n\nAssistant:",
    "Bye<｜end▁of▁sentence｜>"
};

(* --- HF oracle: shell out to python3 transformers -------------------- *)

work = FileNameJoin[{$TemporaryDirectory, "dsv_tok_validate"}];
If[! DirectoryQ[work], CreateDirectory[work]];
pyPath = FileNameJoin[{work, "oracle.py"}];
testsPath = FileNameJoin[{work, "tests.json"}];
outPath = FileNameJoin[{work, "oracle_out.json"}];

pySource = "import sys, json
from transformers import AutoTokenizer
tdir, tin, tout = sys.argv[1], sys.argv[2], sys.argv[3]
t = AutoTokenizer.from_pretrained(tdir)
tests = json.load(open(tin, encoding='utf-8'))
res = []
for s in tests:
    ids = t.encode(s, add_special_tokens=False)
    res.append({'ids': ids, 'decode': t.decode(ids)})
json.dump(res, open(tout, 'w', encoding='utf-8'), ensure_ascii=False)
";

Export[pyPath, pySource, "Text"];
Export[testsPath, tests, "RawJSON"];

proc = RunProcess[{"python3", pyPath, tokDir, testsPath, outPath}];
If[ proc["ExitCode"] =!= 0,
    Print["HF oracle failed (exit ", proc["ExitCode"], "):"];
    Print[proc["StandardError"]];
    Exit[1]
];

oracle = Import[outPath, "RawJSON"];

(* --- compare -------------------------------------------------------- *)

n = Length[tests];

oks = Table[
    Module[{s = tests[[i]], hfIds, hfDec, wlIds, wlDec, eOk, dOk},
        hfIds = oracle[[i]]["ids"];
        hfDec = oracle[[i]]["decode"];
        wlIds = dsvEncode[tok, s];
        wlDec = dsvDecode[tok, hfIds];
        eOk = wlIds === hfIds;
        dOk = wlDec === hfDec;
        Print[If[eOk && dOk, "PASS", "FAIL"], "  ", ToString[s, InputForm]];
        If[ ! eOk,
            Print["   encode WL: ", wlIds];
            Print["   encode HF: ", hfIds]
        ];
        If[ ! dOk,
            Print["   decode WL: ", ToString[wlDec, InputForm]];
            Print["   decode HF: ", ToString[hfDec, InputForm]]
        ];
        {eOk, dOk}
    ],
    {i, n}
];

encPass = Count[oks, {True, _}];
decPass = Count[oks, {_, True}];

Print[""];
Print["encode  PASS ", encPass, "/", n];
Print["decode  PASS ", decPass, "/", n];
If[ encPass === n && decPass === n,
    Print["PASS ", n, "/", n];
    Exit[0]
    ,
    Print["FAIL ", Min[encPass, decPass], "/", n];
    Exit[1]
]
