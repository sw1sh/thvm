(* ::Package:: *)
(* SafeTensors.wl - safetensors save/load + the mmap-backed disk tensor.

   Ported from tinygrad/nn/state.py (safe_load_metadata / safe_load /
   safe_save) + tinygrad/runtime/ops_disk.py (the DISK device: a tensor
   is a view into an mmap of a file).

   The safetensors on-disk format:
     [0:8]      little-endian u64 = JSON header byte length H
     [8:8+H]    JSON header, space-padded to a multiple of 8:
                  {name: {"dtype", "shape", "data_offsets":[begin,end]},
                   "__metadata__": {...}}
                begin/end are byte offsets INTO the data section.
     [8+H:]     concatenated little-endian tensor bytes.

   Public surface
     TSafeTensorLoad[path]        -> Association name -> TTerm, each tensor a
                               LAZY mmap-backed disk view at its
                               data_offset (thvm_tensor_mmap), reshaped
                               per the header.  Bytes page in on demand.
     TSafeTensorSave[assoc, path] -> write `assoc` (name -> TTerm) as a valid
                               .safetensors file.  Returns `path`.
     TTensorMMap[path, byteOffset, nbytes, dtype, shape]
                            -> the low-level disk-tensor constructor.
*)

BeginPackage["THVMLink`"];

TSafeTensorLoad::usage = "TSafeTensorLoad[path] loads a .safetensors file and returns an Association `name -> TTerm`, where each tensor is a LAZY mmap-backed disk view (TTensorMMap) into the file at its data offset, reshaped to the header's shape.  The bytes page in on demand (tinygrad's DISK device); a CPU op consumes them directly, a realize on a non-CPU backend uploads via UOP_COPY.  Ported from tinygrad/nn/state.py safe_load.";

TSafeTensorSave::usage = "TSafeTensorSave[assoc, path] saves an Association `name -> TTerm` to `path` as a valid .safetensors file: an 8-byte little-endian header length, a space-padded JSON header (dtype / shape / data_offsets per tensor), then each tensor's little-endian bytes.  Each TTerm is realized + read once.  TSafeTensorSave[assoc, path, metadata] additionally writes a string-valued `__metadata__` Association into the header (tinygrad safe_save's metadata).  Returns `path`.  Ported from tinygrad/nn/state.py safe_save.";

TSafeTensorLoadMetadata::usage = "TSafeTensorLoadMetadata[path] returns the `__metadata__` Association of a .safetensors file (an empty Association if absent), without loading any tensors.  Ported from tinygrad/nn/state.py safe_load_metadata.";

TTensorMMap::usage = "TTensorMMap[path, byteOffset, nbytes, dtype, shape] maps the file region [byteOffset, byteOffset + nbytes) of `path` read-only and wraps it as a CPU TTerm of the given `dtype` (a thvm dtype string like \"f32\") and integer `shape` list.  This is tinygrad's DISK device: a lazy, mmap-backed, zero-copy tensor view; the bytes page in on demand and the mapping is munmap'd when the tensor is released.  Used by TSafeTensorLoad.";

Begin["`Private`"];

(* Forward-declare sibling-owned symbols so bare references resolve to the
   real THVMLink` symbols rather than phantom Private ones (alphabetical
   load order means Tensor.wl loads AFTER this file). *)
{TTensorData, TTensorShape, TTensorDType, TRealize, dtypeCode, dtypeName, ensureInit};

$tensorMMapFn := $tensorMMapFn = load["thvm_wl_tensor_mmap", {"UTF8String", Integer, Integer, Integer, {Integer, 1}}, Integer]

(* safetensors dtype name <-> thvm dtype string (tinygrad safe_dtypes /
   inverse_safe_dtypes).  Only the byte-aligned dtypes the thvm runtime
   wires through the NumericArray carrier; the nibble + narrow-float
   dtypes are not part of the round-trip surface. *)
$safeToThvm = <|
    "BOOL" -> "bool",
    "I8" -> "i8",
    "U8" -> "u8",
    "I16" -> "i16",
    "U16" -> "u16",
    "I32" -> "i32",
    "U32" -> "u32",
    "I64" -> "i64",
    "U64" -> "u64",
    "F32" -> "f32",
    "F64" -> "f64"
|>;
$thvmToSafe = Association[Reverse /@ Normal[$safeToThvm]];

(* thvm dtype string -> WL NumericArray type.  Drives the little-endian
   byte write in TSafeTensorSave. *)
$thvmNAType = <|
    "bool" -> "UnsignedInteger8",
    "i8" -> "Integer8",
    "u8" -> "UnsignedInteger8",
    "i16" -> "Integer16",
    "u16" -> "UnsignedInteger16",
    "i32" -> "Integer32",
    "u32" -> "UnsignedInteger32",
    "i64" -> "Integer64",
    "u64" -> "UnsignedInteger64",
    "f32" -> "Real32",
    "f64" -> "Real64"
|>;

(* thvm dtype string -> on-disk byte width per element. *)
$thvmByteWidth = <|
    "bool" -> 1,
    "i8" -> 1,
    "u8" -> 1,
    "i16" -> 2,
    "u16" -> 2,
    "i32" -> 4,
    "u32" -> 4,
    "i64" -> 8,
    "u64" -> 8,
    "f32" -> 4,
    "f64" -> 8
|>;

(* Disk-tensor constructor: dtype string + integer shape. *)
TTensorMMap[path_String, byteOffset_Integer, nbytes_Integer, dtype_String, shape_List] := (
    ensureInit[];
    With[{t = $tensorMMapFn[path, byteOffset, nbytes, dtypeCode[dtype], shape]},
        If[IntegerQ[t] && t =!= 0, TTerm[t], $Failed]
    ]
)

(* Read the 8-byte LE header-length + the JSON header from a file.
   Returns {dataStart, metadataAssoc} where dataStart is the absolute
   byte offset of the data section (8 + H). *)
safeReadHeader[path_String] := Module[{strm, hlen, jsonBytes, json},
    strm = OpenRead[path, BinaryFormat -> True];
    hlen = BinaryRead[strm, "UnsignedInteger64", ByteOrdering -> -1];
    jsonBytes = BinaryReadList[strm, "Byte", hlen];
    Close[strm];
    json = Developer`ReadRawJSONString[FromCharacterCode[jsonBytes, "UTF8"]];
    {8 + hlen, json}
]

(* The header's __metadata__ (string -> string), or an empty Association. *)
TSafeTensorLoadMetadata[path_String] := Lookup[Last @ safeReadHeader[path], "__metadata__", <||>]

(* One lazy mmap-backed disk tensor for a single safetensors header entry.
   $Failed if the entry's dtype is outside the byte-aligned round-trip set. *)
loadSafeEntry[path_String, dataStart_Integer, spec_Association] := Module[{thvmDt, begin, end},
    thvmDt = Lookup[$safeToThvm, spec["dtype"], $Failed];
    {begin, end} = spec["data_offsets"];
    If[ thvmDt === $Failed,
        $Failed,
        TTensorMMap[path, dataStart + begin, end - begin, thvmDt, spec["shape"]]
    ]
]

TSafeTensorLoad[path_String] := Module[{dataStart, json, entries},
    ensureInit[];
    {dataStart, json} = safeReadHeader[path];
    entries = KeyDrop[json, "__metadata__"];
    Map[loadSafeEntry[path, dataStart, #] &, entries]   (* maps values, keeps names *)
]

(* Realize a TTerm, read its NumericArray + shape + safetensors dtype. *)
saveTensorInfo[t_TTerm] := Module[{r, na, dt, safeDt, shape, numel},
    r = TRealize[t];
    dt = TTensorDType[r];
    safeDt = Lookup[$thvmToSafe, dt, $Failed];
    If[safeDt === $Failed, Return[$Failed]];
    na = TTensorData[r];                 (* NumericArray, tensor dtype *)
    shape = TTensorShape[r];
    numel = If[shape === {}, 1, Times @@ shape];
    <|
        "safeDt" -> safeDt,
        "naType" -> $thvmNAType[dt],
        "shape" -> shape,
        "na" -> na,
        "nbytes" -> numel * $thvmByteWidth[dt]
    |>
]

TSafeTensorSave[assoc_Association, path_String] := TSafeTensorSave[assoc, path, <||>]

TSafeTensorSave[assoc_Association, path_String, metadata_Association] := Module[{names, infos, offsets, offset, headers, json, pad, hbytes, strm},
    ensureInit[];
    names = Keys[assoc];
    infos = saveTensorInfo /@ Values[assoc];
    If[MemberQ[infos, $Failed], Return[$Failed]];
    (* Lay out the data section: each tensor's byte span end-to-end. *)
    offset = 0;
    offsets = Reap[
        Do[
            With[{nbytes = infos[[i]]["nbytes"]},
                Sow[{offset, offset + nbytes}];
                offset = offset + nbytes
            ],
            {i, Length[infos]}
        ]
    ][[2, 1]];
    (* JSON header: optional __metadata__ first (tinygrad safe_save), then
       one entry per tensor in insertion order.  Metadata values are
       coerced to strings (the safetensors spec requires string values). *)
    headers = Join[
        If[metadata === <||>, <||>, <|"__metadata__" -> (ToString /@ metadata)|>],
        Association @ MapThread[
            (#1 -> <|"dtype" -> #2["safeDt"], "shape" -> #2["shape"], "data_offsets" -> #3|>) &,
            {names, infos, offsets}
        ]
    ];
    json = Developer`WriteRawJSONString[headers, "Compact" -> True];
    (* Space-pad the JSON to a multiple of 8 BYTES (tinygrad safe_save
       pads len(j), where j is already the UTF-8 byte string).  A space
       is one UTF-8 byte, so byte-length padding stays a space-pad. *)
    hbytes = ToCharacterCode[json, "UTF8"];
    pad = Mod[-Length[hbytes], 8];
    hbytes = Join[hbytes, ConstantArray[32, pad]];   (* 32 = ASCII space *)
    (* Write: 8-byte LE header-byte-length, JSON, then each tensor's LE
       bytes. *)
    Quiet @ DeleteFile[path];
    strm = OpenWrite[path, BinaryFormat -> True];
    BinaryWrite[strm, Length[hbytes], "UnsignedInteger64", ByteOrdering -> -1];
    BinaryWrite[strm, hbytes, "Byte"];
    Do[
        BinaryWrite[strm, Normal[infos[[i]]["na"]], infos[[i]]["naType"], ByteOrdering -> -1],
        {i, Length[infos]}
    ];
    Close[strm];
    path
]

End[];

EndPackage[];
