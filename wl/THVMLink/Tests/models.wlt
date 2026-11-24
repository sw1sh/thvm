(* models.wlt - what the example models share (NN/Examples/Models.wl): the pinned sources and the fetch
   from HuggingFace.  The sources of FLUX, Phonon and Whisper are checked as data.  With huggingface.co in
   reach, the fetch is run on FLUX's three tokenizer files (4.4 MB) into a temporary directory: files in
   subdirectories, a stopped download continued, a stopped download kept, wrong content refused.  No
   model is loaded and no weight file is fetched. *)

Get["WolframInstitute`THVMLink`Examples`"]

$modelsOnline = Quiet[RunProcess[{"curl", "-fsI", "--max-time", "8", "https://huggingface.co"}, "ExitCode"]] === 0;

(* Every source names a repository, a 40-character revision, and files with sizes and SHA-256. *)
VerificationTest[
    With[{sources = Join[
            {WolframInstitute`THVMLink`Examples`Private`fxSource[], WolframInstitute`THVMLink`Examples`Private`phSource[]},
            Values[WolframInstitute`THVMLink`Examples`Private`$whModels]]},
        {Length[sources],
         AllTrue[sources, StringQ[#Repository] && StringLength[#Revision] == 40 &&
             AllTrue[#Files, IntegerQ[#Bytes] && #Bytes > 0 && StringMatchQ[#SHA256, Repeated[HexadecimalCharacter, {64}]] &] &]}],
    {8, True},
    TestID -> "models/sources-are-pinned"
]

(* FLUX's seven files: the diffusers layout of the model's own repository, 14.9 GiB. *)
VerificationTest[
    With[{s = WolframInstitute`THVMLink`Examples`Private`fxSource[]},
        {s["Repository"], Sort @ Keys[s["Files"]], Total[#Bytes & /@ Values[s["Files"]]],
         WolframInstitute`THVMLink`Examples`Private`hubUrl[s, "vae/diffusion_pytorch_model.safetensors"]}],
    {"black-forest-labs/FLUX.2-klein-4B",
     {"text_encoder/model-00001-of-00002.safetensors", "text_encoder/model-00002-of-00002.safetensors", "tokenizer/added_tokens.json",
      "tokenizer/merges.txt", "tokenizer/vocab.json", "transformer/diffusion_pytorch_model.safetensors", "vae/diffusion_pytorch_model.safetensors"},
     15968662007,
     "https://huggingface.co/black-forest-labs/FLUX.2-klein-4B/resolve/e7b7dc27f91deacad38e78976d1f2b499d76a294/vae/diffusion_pytorch_model.safetensors"},
    TestID -> "models/flux-files"
]

(* A file counts when it is there with its pinned size. *)
VerificationTest[
    With[{dir = CreateDirectory[], s = WolframInstitute`THVMLink`Examples`Private`fxSource[]},
        CreateDirectory[FileNameJoin[{dir, "tokenizer"}]];
        BinaryWrite[FileNameJoin[{dir, "tokenizer", "added_tokens.json"}], ConstantArray[32, 707], "Byte"] // Close;
        BinaryWrite[FileNameJoin[{dir, "tokenizer", "merges.txt"}], {1, 2, 3}, "Byte"] // Close;
        With[{have = WolframInstitute`THVMLink`Examples`Private`hubHaveQ[s, dir, #] & /@
                {"tokenizer/added_tokens.json", "tokenizer/merges.txt", "tokenizer/vocab.json"}},
            DeleteDirectory[dir, DeleteContents -> True];
            have]],
    {True, False, False},
    TestID -> "models/a-file-counts-by-its-size"
]

VerificationTest[
    WolframInstitute`THVMLink`Examples`Private`hubMegabytes /@ {0, 49999, 64512352, 15968662007},
    {"0.0", "0.0", "64.5", "15968.7"},
    TestID -> "models/megabytes"
]

If[$modelsOnline,

    $modelsDir = FileNameJoin[{$TemporaryDirectory, "thvm-models-test-" <> ToString[$ProcessID]}];
    $modelsSmall = KeySelect[WolframInstitute`THVMLink`Examples`Private`$fxFiles, StringStartsQ[#, "tokenizer/"] &];
    modelsFetch[] := Block[{WolframInstitute`THVMLink`Examples`Private`$fxFiles = $modelsSmall},
        Quiet[WolframInstitute`THVMLink`Examples`Private`fxFetch[$modelsDir], FluxGenerate::fetch]];
    $modelsVocab = FileNameJoin[{$modelsDir, "tokenizer", "vocab.json"}];

    (* The files land under the names they have in the repository, and a second call fetches nothing. *)
    VerificationTest[
        {modelsFetch[], Sort[{FileNameTake[#, -2], FileByteCount[#]} & /@ Select[FileNames["*", $modelsDir, Infinity], ! DirectoryQ[#] &]],
         First @ AbsoluteTiming[modelsFetch[]] < 0.1},
        {True, {{"tokenizer/added_tokens.json", 707}, {"tokenizer/merges.txt", 1671853}, {"tokenizer/vocab.json", 2776833}}, True},
        TestID -> "models/fetch-into-subdirectories"
    ];

    $modelsBytes = BinaryReadList[$modelsVocab, "Byte"];

    (* A download that stopped at 1 MB is continued, not started again, and gives the same file. *)
    VerificationTest[
        (DeleteFile[$modelsVocab];
         BinaryWrite[$modelsVocab <> ".part", Take[$modelsBytes, 1000000], "Byte"] // Close;
         {modelsFetch[], BinaryReadList[$modelsVocab, "Byte"] === $modelsBytes, FileExistsQ[$modelsVocab <> ".part"]}),
        {True, True, False},
        TestID -> "models/a-stopped-download-is-continued"
    ];

    (* When the source fails, what has arrived is kept and the Failure says the next call continues it. *)
    VerificationTest[
        (DeleteFile[$modelsVocab];
         BinaryWrite[$modelsVocab <> ".part", Take[$modelsBytes, 1000000], "Byte"] // Close;
         With[{r = Block[{WolframInstitute`THVMLink`Examples`Private`$fxRevision = StringRepeat["0", 40]}, modelsFetch[]]},
             {FailureQ[r], r["Tag"], First[r["MessageParameters"]], StringContainsQ[Last[r["MessageParameters"]], "the next call continues it"],
              FileByteCount[$modelsVocab <> ".part"], modelsFetch[], FileByteCount[$modelsVocab]}]),
        {True, "fetch", "tokenizer/vocab.json", True, 1000000, True, 2776833},
        TestID -> "models/a-stopped-download-is-kept"
    ];

    (* The right size with the wrong bytes is refused and removed. *)
    VerificationTest[
        (DeleteFile[$modelsVocab];
         BinaryWrite[$modelsVocab <> ".part", ReplacePart[$modelsBytes, 5 -> Mod[$modelsBytes[[5]] + 1, 256]], "Byte"] // Close;
         With[{r = Block[{WolframInstitute`THVMLink`Examples`Private`$fxRevision = StringRepeat["0", 40]}, modelsFetch[]]},
             {FailureQ[r], r["Tag"], FileExistsQ[$modelsVocab <> ".part"], FileExistsQ[$modelsVocab]}]),
        {True, "fetch", False, False},
        TestID -> "models/wrong-content-is-refused"
    ];

    DeleteDirectory[$modelsDir, DeleteContents -> True]
]

(* FluxGenerate fetches before it builds a session: when the fetch fails, every form gives the Failure
   itself, and nothing of the model is touched. *)
VerificationTest[
    With[{dir = CreateDirectory[]},
        With[{rs = Block[{WolframInstitute`THVMLink`Examples`Private`hubFreeBytes = (1000 &)},
                Quiet[{FluxGenerate["a cat", "ModelDir" -> dir, ProgressReporting -> False],
                       FluxGenerate["a cat", "Latent", "ModelDir" -> dir, ProgressReporting -> False],
                       FluxGenerate[{"a cat", "a dog"}, "ModelDir" -> dir, ProgressReporting -> False],
                       FluxGenerate["a cat", 2, "ModelDir" -> dir, ProgressReporting -> False],
                       TImageSynthesize["a cat", "ModelDir" -> dir, ProgressReporting -> False]}, FluxGenerate::fetch]]},
            DeleteDirectory[dir, DeleteContents -> True];
            {AllTrue[rs, FailureQ], Union[#["Tag"] & /@ rs], Union[First[#["MessageParameters"]] & /@ rs]}]],
    {True, {"fetch"}, {"tokenizer/vocab.json"}},
    TestID -> "models/flux-gives-the-fetch-failure-in-every-form"
]

(* The free space of a disk is a number of bytes, and a fetch that would not fit fails before it starts. *)
VerificationTest[
    With[{dir = CreateDirectory[]},
        With[{free = WolframInstitute`THVMLink`Examples`Private`hubFreeBytes[dir],
              r = Block[{WolframInstitute`THVMLink`Examples`Private`hubFreeBytes = (1000 &)},
                  Quiet[WolframInstitute`THVMLink`Examples`Private`fxFetch[dir], FluxGenerate::fetch]]},
            With[{left = Select[FileNames["*", dir, Infinity], ! DirectoryQ[#] &]},
                DeleteDirectory[dir, DeleteContents -> True];
                {free === Infinity || (IntegerQ[free] && free > 0), FailureQ[r], r["Tag"], First[r["MessageParameters"]],
                 StringContainsQ[Last[r["MessageParameters"]], "need 15968.7 MB and the disk has 0.0 MB free"], left}]]],
    {True, True, "fetch", "tokenizer/vocab.json", True, {}},
    TestID -> "models/a-fetch-that-does-not-fit-fails-first"
]
