(* flux_text.wlt - the host side of FLUX's text encoder (NN/Examples/FLUX): the tokenizer's files, the
   normal form of a prompt, bf16 words as reals, and the rows of the embedding table read from the
   weights file.  The files are small ones written here; no model is loaded. *)

Get["WolframInstitute`THVMLink`Examples`"]

(* A tokenizer directory that cannot be read gives a Failure, and the Failure is not kept for the next call. *)
VerificationTest[
    With[{dir = CreateDirectory[]},
        With[{r = Quiet[WolframInstitute`THVMLink`Examples`Private`qwTokenizerData[dir], FluxGenerate::tokenizer]},
            DeleteDirectory[dir, DeleteContents -> True];
            {FailureQ[r], r["Tag"], FreeQ[DownValues[WolframInstitute`THVMLink`Examples`Private`qwTokenizerData], dir]}]],
    {True, "tokenizer", True},
    TestID -> "flux-text/a-tokenizer-that-cannot-be-read-is-a-failure"
]

(* A text with no character from U+0300 on is its own normal form; a combining mark is composed. *)
VerificationTest[
    {
        WolframInstitute`THVMLink`Examples`Private`qwNfc["a red apple on a wooden table"],
        WolframInstitute`THVMLink`Examples`Private`qwNfc["caf\[EAcute]"],
        ToCharacterCode @ WolframInstitute`THVMLink`Examples`Private`qwNfc[FromCharacterCode[{101, 769}]]
    },
    {"a red apple on a wooden table", "caf\[EAcute]", {233}},
    TestID -> "flux-text/normal-form"
]

(* Where CharacterNormalize does nothing, a plain text is still normalized and a text that needs it is
   no string, which the entry of FluxGenerate turns into a Failure. *)
VerificationTest[
    Block[{CharacterNormalize},
        {
            WolframInstitute`THVMLink`Examples`Private`qwNfc["plain"],
            StringQ @ WolframInstitute`THVMLink`Examples`Private`qwNfc[FromCharacterCode[{101, 769}]]
        }],
    {"plain", False},
    TestID -> "flux-text/normal-form-without-the-kernel-function"
]

(* bf16 words by their bits: 0x3F80 is 1, 0xC000 is -2, 0x3E80 is 0.25, 0x4049 is 3.140625, 0 and
   0x8000 are zero, 0x0001 is the subnormal 2^-133; a matrix keeps its shape. *)
VerificationTest[
    {
        WolframInstitute`THVMLink`Examples`Private`qwBf16ToF32[{16256, 49152, 16000, 16457, 0, 32768}],
        WolframInstitute`THVMLink`Examples`Private`qwBf16ToF32[{1}] === {2.^-133},
        WolframInstitute`THVMLink`Examples`Private`qwBf16ToF32[{{16256, 49152}, {16000, 0}}]
    },
    {{1., -2., 0.25, 3.140625, 0., 0.}, True, {{1., -2.}, {0.25, 0.}}},
    TestID -> "flux-text/bf16-words-as-reals"
]

(* A bf16 table {5, 3} written after another tensor in the second of two files: the table is found there
   with the byte its data starts at, and its rows read from the file are the rows of the loaded tensor,
   in the order of the ids. *)
VerificationTest[
    Module[{dir = CreateDirectory[], rows, words, header, bytes, first, second, strm, source, fromFile, fromTensor},
        rows = {{1., 2., -0.5}, {0.25, 3., -4.}, {0., 8., 0.75}, {-1.5, 16., 0.125}, {100., -0.375, 5.}};
        words = Flatten[Drop[#, 2] & /@ Partition[Normal @ ExportByteArray[Flatten[rows], "Real32"], 4]];
        header[json_] := ToCharacterCode[json <> StringRepeat[" ", Mod[-StringLength[json], 8]], "UTF8"];
        first = FileNameJoin[{dir, "model-00001-of-00002.safetensors"}];
        second = FileNameJoin[{dir, "model-00002-of-00002.safetensors"}];
        bytes = header["{\"other\":{\"dtype\":\"F32\",\"shape\":[1],\"data_offsets\":[0,4]}}"];
        strm = OpenWrite[first, BinaryFormat -> True];
        BinaryWrite[strm, Length[bytes], "UnsignedInteger64", ByteOrdering -> -1];
        BinaryWrite[strm, bytes, "Byte"]; BinaryWrite[strm, {0, 0, 128, 63}, "Byte"]; Close[strm];
        bytes = header["{\"lead\":{\"dtype\":\"F32\",\"shape\":[1],\"data_offsets\":[0,4]},\"table\":{\"dtype\":\"BF16\",\"shape\":[5,3],\"data_offsets\":[4,34]}}"];
        strm = OpenWrite[second, BinaryFormat -> True];
        BinaryWrite[strm, Length[bytes], "UnsignedInteger64", ByteOrdering -> -1];
        BinaryWrite[strm, bytes, "Byte"]; BinaryWrite[strm, {0, 0, 0, 64}, "Byte"]; BinaryWrite[strm, words, "Byte"]; Close[strm];
        source = WolframInstitute`THVMLink`Examples`Private`qwEmbedSource[{first, second}, "table"];
        fromFile = Normal @ TRealize @ WolframInstitute`THVMLink`Examples`Private`qwEmbed[source, {4, 0, 2, 4}];
        fromTensor = Normal @ TRealize @ WolframInstitute`THVMLink`Examples`Private`qwEmbed[TSafeTensorLoad[second]["table"], {4, 0, 2, 4}];
        With[{result = {
                source["File"] === second, source["At"] === 8 + Length[bytes] + 4, source["Shape"],
                fromFile, fromFile === fromTensor,
                WolframInstitute`THVMLink`Examples`Private`qwEmbedSource[{first, second}, "absent"]}},
            DeleteDirectory[dir, DeleteContents -> True];
            result]],
    {True, True, {5, 3}, {{100., -0.375, 5.}, {1., 2., -0.5}, {0., 8., 0.75}, {100., -0.375, 5.}}, True, Missing["NotFound", "absent"]},
    TestID -> "flux-text/embedding-rows-from-the-file"
]
