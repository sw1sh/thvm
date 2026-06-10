(* safetensors.wlt -- TSafeTensorSave / TSafeTensorLoad round-trip + the mmap-backed
   disk tensor (TTensorMMap).  Ported from tinygrad/nn/state.py +
   ops_disk.py.  The anchor test is a bit-identical round-trip; we also
   assert the on-disk bytes match the safetensors spec (8-byte LE JSON
   length, parseable JSON, correct data offsets) so a thvm-saved file is
   a valid .safetensors. *)

(* === round-trip: f32 (the anchor) === *)

VerificationTest[
    TInit[]; TReset[];
    w = TTensorCreate @ NumericArray[{{1.5, -2.0, 3.25}, {0.0, 100.0, -0.5}}, "Real32"];
    b = TTensorCreate @ NumericArray[{0.125, -7.0}, "Real32"];
    path = FileNameJoin[{$TemporaryDirectory,
        "thvm_st_" <> ToString[$ProcessID] <> "_f32.safetensors"}];
    TSafeTensorSave[<|"w" -> w, "b" -> b|>, path];
    ld = TSafeTensorLoad[path];
    res = {Normal[ld["w"]], Normal[ld["b"]]};
    Quiet @ DeleteFile[path];
    res,
    {{{1.5, -2.0, 3.25}, {0.0, 100.0, -0.5}}, {0.125, -7.0}},
    TestID -> "safetensors/roundtrip/f32-bit-identical"
]

(* === round-trip: i64 === *)

VerificationTest[
    TInit[]; TReset[];
    x = TTensorCreate @ NumericArray[{1, -2, 9223372036854775807, 0}, "Integer64"];
    path = FileNameJoin[{$TemporaryDirectory,
        "thvm_st_" <> ToString[$ProcessID] <> "_i64.safetensors"}];
    TSafeTensorSave[<|"x" -> x|>, path];
    ld = TSafeTensorLoad[path];
    res = {TTensorDType[ld["x"]], Normal[ld["x"]]};
    Quiet @ DeleteFile[path];
    res,
    {"i64", {1, -2, 9223372036854775807, 0}},
    TestID -> "safetensors/roundtrip/i64"
]

(* === bf16 LOAD (the FLUX / modern-LLM weight format) ===
   bf16 has no WL NumericArray carrier, so it is synthesised on disk (the
   high 2 bytes of each LE f32, exact for bf16-representable values), mmap'd
   by TSafeTensorLoad as a bf16 tensor, then cast to f32 for readback.  This
   is the path the FLUX importer uses (bf16 weights -> TUOpCast -> compute). *)

VerificationTest[
    TInit[]; TReset[];
    vals = {1.0, 2.0, -0.5, 0.25, 3.0, -4.0, 0.0, 8.0};   (* all exact in bf16 *)
    bf16 = Flatten[Drop[#, 2] & /@ Partition[Normal @ ExportByteArray[N[vals], "Real32"], 4]];
    n = Length[vals]; nb = 2 n;
    json = "{\"t\":{\"dtype\":\"BF16\",\"shape\":[" <> ToString[n] <> "],\"data_offsets\":[0," <> ToString[nb] <> "]}}";
    json = json <> StringRepeat[" ", Mod[-StringLength[json], 8]];
    hbytes = ToCharacterCode[json, "UTF8"];
    path = FileNameJoin[{$TemporaryDirectory,
        "thvm_st_" <> ToString[$ProcessID] <> "_bf16.safetensors"}];
    strm = OpenWrite[path, BinaryFormat -> True];
    BinaryWrite[strm, Length[hbytes], "UnsignedInteger64", ByteOrdering -> -1];
    BinaryWrite[strm, hbytes, "Byte"];
    BinaryWrite[strm, bf16, "Byte"];
    Close[strm];
    ld = TSafeTensorLoad[path];
    res = {TTensorDType[ld["t"]], Normal @ TRealize @ TUOpCast[ld["t"], "f32"]};
    Quiet @ DeleteFile[path];
    res,
    {"bf16", {1.0, 2.0, -0.5, 0.25, 3.0, -4.0, 0.0, 8.0}},
    TestID -> "safetensors/load/bf16-flux-weight-format"
]

(* === sharded load: a HF *.safetensors.index.json + shard files (the
       layout the FLUX Qwen3 text encoder ships in) merge into one state
       dict, keyed off the index's weight_map === *)

VerificationTest[
    TInit[]; TReset[];
    dir = FileNameJoin[{$TemporaryDirectory, "thvm_shard_" <> ToString[$ProcessID]}];
    Quiet @ CreateDirectory[dir];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    b = TTensorCreate @ NumericArray[{{4.0, 5.0}, {6.0, 7.0}}, "Real32"];
    TSafeTensorSave[<|"enc.a" -> a|>, FileNameJoin[{dir, "model-00001-of-00002.safetensors"}]];
    TSafeTensorSave[<|"enc.b" -> b|>, FileNameJoin[{dir, "model-00002-of-00002.safetensors"}]];
    Export[FileNameJoin[{dir, "model.safetensors.index.json"}],
        "{\"weight_map\":{\"enc.a\":\"model-00001-of-00002.safetensors\",\"enc.b\":\"model-00002-of-00002.safetensors\"}}",
        "Text"];
    ld = TSafeTensorLoad[FileNameJoin[{dir, "model.safetensors.index.json"}]];
    res = {Sort @ Keys @ ld, Normal @ TRealize @ ld["enc.a"], Normal @ TRealize @ ld["enc.b"]};
    Quiet @ DeleteDirectory[dir, DeleteContents -> True];
    res,
    {{"enc.a", "enc.b"}, {1.0, 2.0, 3.0}, {{4.0, 5.0}, {6.0, 7.0}}},
    TestID -> "safetensors/load/sharded-index-json"
]

(* === spec byte layout: first 8 bytes = JSON length LE; JSON parses;
       data_offsets correct === *)

VerificationTest[
    TInit[]; TReset[];
    w = TTensorCreate @ NumericArray[{{1.0, 2.0}, {3.0, 4.0}}, "Real32"];  (* 16 bytes *)
    b = TTensorCreate @ NumericArray[{5.0, 6.0, 7.0}, "Real32"];          (* 12 bytes *)
    path = FileNameJoin[{$TemporaryDirectory,
        "thvm_st_" <> ToString[$ProcessID] <> "_spec.safetensors"}];
    TSafeTensorSave[<|"w" -> w, "b" -> b|>, path];
    strm = OpenRead[path, BinaryFormat -> True];
    hlen = BinaryRead[strm, "UnsignedInteger64", ByteOrdering -> -1];
    jsonBytes = BinaryReadList[strm, "Byte", hlen];
    Close[strm];
    json = Developer`ReadRawJSONString[FromCharacterCode[jsonBytes, "UTF8"]];
    Quiet @ DeleteFile[path];
    {
        (* header length is a multiple of 8 (space-padded JSON) *)
        Mod[hlen, 8] === 0,
        (* JSON parsed to an Association with both keys *)
        Sort[Keys[json]] === {"b", "w"},
        (* dtype + shape + data_offsets per spec *)
        json["w"]["dtype"], json["w"]["shape"], json["w"]["data_offsets"],
        json["b"]["dtype"], json["b"]["shape"], json["b"]["data_offsets"]
    },
    {True, True, "F32", {2, 2}, {0, 16}, "F32", {3}, {16, 28}},
    TestID -> "safetensors/spec/byte-layout"
]

(* === lazy mmap disk tensor: a CPU op over a loaded tensor === *)

VerificationTest[
    TInit[]; TReset[];
    w = TTensorCreate @ NumericArray[{10.0, 20.0, 30.0}, "Real32"];
    path = FileNameJoin[{$TemporaryDirectory,
        "thvm_st_" <> ToString[$ProcessID] <> "_lazy.safetensors"}];
    TSafeTensorSave[<|"w" -> w|>, path];
    ld = TSafeTensorLoad[path];
    (* +1 over the mmap-backed tensor proves it is a usable lazy tensor *)
    res = Normal @ TRealize @ TUOpAdd[ld["w"], TUOpConst[1.0]];
    Quiet @ DeleteFile[path];
    res,
    {11.0, 21.0, 31.0},
    TestID -> "safetensors/lazy/mmap-cpu-op"
]

(* === TTensorMMap directly: a raw region of a file === *)

VerificationTest[
    TInit[]; TReset[];
    path = FileNameJoin[{$TemporaryDirectory,
        "thvm_st_" <> ToString[$ProcessID] <> "_raw.bin"}];
    (* 5 junk header bytes (non-aligned offset), then 4 f32 values LE *)
    strm = OpenWrite[path, BinaryFormat -> True];
    BinaryWrite[strm, {7, 7, 7, 7, 7}, "Byte"];
    BinaryWrite[strm, {1.25, 2.5, -3.0, 4.75}, "Real32", ByteOrdering -> -1];
    Close[strm];
    t = TTensorMMap[path, 5, 16, "f32", {4}];
    res = Normal[t];
    Quiet @ DeleteFile[path];
    res,
    {1.25, 2.5, -3.0, 4.75},
    TestID -> "safetensors/mmap/raw-region-page-aligned-offset"
]
