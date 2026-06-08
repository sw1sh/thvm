(* safetensors.wlt -- TSafeSave / TSafeLoad round-trip + the mmap-backed
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
    TSafeSave[<|"w" -> w, "b" -> b|>, path];
    ld = TSafeLoad[path];
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
    TSafeSave[<|"x" -> x|>, path];
    ld = TSafeLoad[path];
    res = {TTensorDType[ld["x"]], Normal[ld["x"]]};
    Quiet @ DeleteFile[path];
    res,
    {"i64", {1, -2, 9223372036854775807, 0}},
    TestID -> "safetensors/roundtrip/i64"
]

(* === spec byte layout: first 8 bytes = JSON length LE; JSON parses;
       data_offsets correct === *)

VerificationTest[
    TInit[]; TReset[];
    w = TTensorCreate @ NumericArray[{{1.0, 2.0}, {3.0, 4.0}}, "Real32"];  (* 16 bytes *)
    b = TTensorCreate @ NumericArray[{5.0, 6.0, 7.0}, "Real32"];          (* 12 bytes *)
    path = FileNameJoin[{$TemporaryDirectory,
        "thvm_st_" <> ToString[$ProcessID] <> "_spec.safetensors"}];
    TSafeSave[<|"w" -> w, "b" -> b|>, path];
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
    TSafeSave[<|"w" -> w|>, path];
    ld = TSafeLoad[path];
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
