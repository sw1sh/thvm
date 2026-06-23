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

BeginPackage["WolframInstitute`THVMLink`"];

GeneralUtilities`SetUsage[TSafeTensorLoad, "TSafeTensorLoad[path$] loads a .safetensors file and returns an Association of name$ to TTerm.
Each tensor is a lazy mmap-backed disk view (TTensorMMap) into the file at its data offset, reshaped to the header's shape; bytes page in on demand, a CPU op consumes them directly and a realize on another backend uploads them."];

GeneralUtilities`SetUsage[TSafeTensorSave, "TSafeTensorSave[assoc$, path$] writes assoc$ (name$ to TTerm) to path$ as a .safetensors file and returns path$.
TSafeTensorSave[assoc$, path$, metadata$] also writes a string-valued __metadata__ Association into the header.
The file is an 8-byte little-endian header length, a space-padded JSON header (dtype, shape, data_offsets per tensor), then each tensor's little-endian bytes; each TTerm is realized and read once."];

GeneralUtilities`SetUsage[TSafeTensorLoadMetadata, "TSafeTensorLoadMetadata[path$] returns the __metadata__ Association of a .safetensors file (an empty Association if absent), without loading any tensors."];

GeneralUtilities`SetUsage[TTensorMMap, "TTensorMMap[path$, byteOffset$, nbytes$, dtype$, shape$] maps the file region [byteOffset$, byteOffset$ + nbytes$) of path$ read-only and wraps it as a CPU TTerm of the given dtype$ (a thvm dtype string like \"f32\") and integer shape$ list.
This is a lazy, mmap-backed, zero-copy disk-tensor view: bytes page in on demand and the mapping is released with the tensor."];

GeneralUtilities`SetUsage[TDiskDropWeight, "TDiskDropWeight[w$] streams a disk-mmap weight w$ (a lazy mmap-backed disk TTerm, e.g. from TSafeTensorLoad) out of resident memory after it has been used: it releases any Metal zero-copy wrap of the weight (the borrowed MTLBuffer aliasing its mmap pages) and MADV_DONTNEEDs the underlying disk mapping so the faulted-in pages leave RSS.
The mmap stays valid (file-backed, read-only), so a later use re-faults and re-wraps the weight fresh.  Used by a per-block streaming forward to keep only the active block's weights resident.  TDiskDropWeight[{w$1, w$2, ...}] drops a list.  A no-op (returns the weight unchanged) for a non-disk / non-wrapped tensor."];

GeneralUtilities`SetUsage[TDiskPrefetchAsync, "TDiskPrefetchAsync[path$] kicks off a detached background read of the whole file path$ so its bytes land in the OS page cache before a later mmap fault touches them; returns path$ immediately (non-blocking).
TDiskPrefetchAsync[{path$1, path$2, $$}] prefetches several files concurrently.  Used at FLUX-session build to warm the transformer / Qwen / VAE safetensors so the ~6 s of SSD read overlaps the JIT capture instead of stalling serially inside the first forward (the zero-copy wrap's fault-in then hits warm pages).  Best-effort: a missing/unreadable file is silently ignored."];

GeneralUtilities`SetUsage[TDiskWarmAsync, "TDiskWarmAsync[w$] background-faults a disk-mmap weight w$ (a lazy mmap-backed disk TTerm, e.g. from TSafeTensorLoad) resident in THIS process: a detached thread touches one byte per page across the weight's mapping so the OS faults every page in, then returns the weight at once (non-blocking).  The inverse of TDiskDropWeight.
TDiskWarmAsync[{w$1, w$2, $$}] warms a list of weights and returns the list.  TDiskPrefetchAsync only warms the OS page cache (a file read); the per-process minor fault that maps those pages into THIS address space still happens serially at the first matmul.  Warming an idle model's weights ahead of its forward (e.g. the FLUX transformer during the Qwen text-encode stage) moves that minor fault off the forward's critical path.  A no-op (returns the weight unchanged) for a non-disk / non-mapped tensor."];

Begin["`Private`"];

(* Forward-declare sibling-owned symbols so bare references resolve to the
   real WolframInstitute`THVMLink` symbols rather than phantom Private ones (alphabetical
   load order means Tensor.wl loads AFTER this file). *)
{TTensorData, TTensorShape, TTensorDType, TRealize, dtypeCode, dtypeName, ensureInit, ttermRaw};

$tensorMMapFn := $tensorMMapFn = load["thvm_wl_tensor_mmap", {"UTF8String", Integer, Integer, Integer, {Integer, 1}}, Integer]

$diskDropWeightFn := $diskDropWeightFn = load["thvm_wl_disk_drop_weight", {Integer}, Integer]

$filePrefetchFn := $filePrefetchFn = load["thvm_wl_file_prefetch_async", {"UTF8String"}, Integer]

$diskWarmFn := $diskWarmFn = load["thvm_wl_disk_warm_async", {Integer}, Integer]

(* Drop a disk-mmap weight's resident pages + Metal wrap (the streaming
   per-block reclaim).  Accepts a single TTerm or a list; returns the input. *)
TDiskDropWeight[ws_List] := (TDiskDropWeight /@ ws; ws)
TDiskDropWeight[w_TTerm] := (ensureInit[]; $diskDropWeightFn[ttermRaw[w]]; w)
TDiskDropWeight[w_]      := w

(* Non-blocking background page-cache warm of a safetensors file (or list).
   Returns the path(s); the read runs on a detached thread. *)
TDiskPrefetchAsync[paths_List] := (TDiskPrefetchAsync /@ paths; paths)
TDiskPrefetchAsync[path_String] := (ensureInit[]; $filePrefetchFn[path]; path)

(* Non-blocking background fault-in of a disk-mmap weight's pages into this
   process (inverse of TDiskDropWeight).  Accepts a single TTerm or a list;
   returns the input. *)
TDiskWarmAsync[ws_List] := (TDiskWarmAsync /@ ws; ws)
TDiskWarmAsync[w_TTerm] := (ensureInit[]; $diskWarmFn[ttermRaw[w]]; w)
TDiskWarmAsync[w_]      := w

(* safetensors dtype name <-> thvm dtype string (tinygrad safe_dtypes /
   inverse_safe_dtypes).  LOAD covers the byte-aligned dtypes PLUS the
   narrow floats f16/bf16/fp8 (mmap'd as raw bytes + a runtime dtype tag;
   a downstream TUOpCast lifts them to f32/f16 for compute -- bf16 is the
   FLUX / modern-LLM weight format).  SAVE stays byte-aligned only: the
   narrow floats have no WL NumericArray carrier, so $thvmToSafe omits
   them and TSafeTensorSave declines them ($Failed) rather than
   malforming the file (a bitcast-to-uint save path is a follow-up). *)
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
    "F8_E4M3" -> "fp8e4m3",
    "F8_E5M2" -> "fp8e5m2",
    "F16" -> "f16",
    "BF16" -> "bf16",
    "F32" -> "f32",
    "F64" -> "f64"
|>

(* SAVE map: the byte-aligned subset only.  The narrow floats (f16/bf16/
   fp8) load fine via mmap (raw bytes + a runtime dtype tag, dequant/cast
   downstream), but they have no WL NumericArray carrier, so TSafeTensorSave
   cannot emit them without a bitcast-to-uint path -- left out so save
   gracefully declines them ($Failed) rather than malforming the file. *)
$thvmToSafe = <|
    "bool" -> "BOOL",
    "i8" -> "I8", "u8" -> "U8",
    "i16" -> "I16", "u16" -> "U16",
    "i32" -> "I32", "u32" -> "U32",
    "i64" -> "I64", "u64" -> "U64",
    "f32" -> "F32", "f64" -> "F64"
|>

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
|>

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
|>

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

(* Sharded load: a HuggingFace `*.safetensors.index.json` (a "weight_map"
   of tensor-name -> shard-file, e.g. model-00001-of-00002.safetensors,
   the layout the FLUX Qwen3 text encoder ships in) loads + merges every
   referenced shard into one name -> TTerm Association.  Mirrors
   transformers' sharded from_pretrained.  More specific than the bare
   single-file def below, so it matches index paths first. *)
TSafeTensorLoad[indexPath_String] /; StringEndsQ[indexPath, ".index.json"] := Module[
    {idx, dir, shards},
    idx    = Developer`ReadRawJSONFile[indexPath];
    dir    = DirectoryName[indexPath];
    shards = DeleteDuplicates @ Values @ Lookup[idx, "weight_map", <||>];
    Join @@ (TSafeTensorLoad[FileNameJoin[{dir, #}]] & /@ shards)
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
    (* Lay out the data section: each tensor's byte span end-to-end.  An
       empty assoc Sows nothing, so Reap yields {} (not {{...}}); guard the
       [[2, 1]] Part for the valid degenerate empty-safetensors case. *)
    offset = 0;
    offsets = If[ infos === {},
        {},
        Reap[
            Do[
                With[{nbytes = infos[[i]]["nbytes"]},
                    Sow[{offset, offset + nbytes}];
                    offset = offset + nbytes
                ],
                {i, Length[infos]}
            ]
        ][[2, 1]]
    ];
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
