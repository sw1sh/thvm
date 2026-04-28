(* ::Package:: *)
(* THVMLink - Wolfram Language bridge to the thvm interaction-net runtime.

   The C bridge (CSource/thvmlink.c) exports 14 scalar functions covering
   term packing/unpacking, heap access, the WNF entry point, and a few
   counters.  This package wraps them and adds high-level constructors
   (TLam / TApp / TSup / TDup), inspection helpers (TTermInfo, THeap,
   TTagName), and the IC-style heap renderer (THeapGraph).
*)

BeginPackage["THVMLink`", {"GeneralUtilities`"}];

(* === lifecycle === *)
TInit::usage      = "TInit[] initializes the runtime.  Returns True.";
TFree::usage      = "TFree[] tears the runtime down.";
TReset::usage     = "TReset[] zeroes the heap, the WNF stack, and the interaction counter.";

(* === atomic term object === *)
TTerm::usage      = "TTerm[id_Integer] wraps a packed 64-bit Term value.  Construct via TLam / TApp / TSup / TDup / TEra / TVarFor; or directly TTerm[<rawInteger>].  Indexing is supported: TTerm[id][\"tag\"|\"ext\"|\"val\"|\"sub\"|\"tagName\"|\"raw\"].";
TTermTag::usage   = "TTermTag[term] returns the tag (Integer).  Accepts either a TTerm or a raw Integer.";
TTermExt::usage   = "TTermExt[term] returns the EXT field.";
TTermVal::usage   = "TTermVal[term] returns the VAL field (heap loc, etc.).";
TTermSub::usage   = "TTermSub[term] returns the SUB flag (0 or 1).";
TTermUnpin::usage = "TTermUnpin[term] drops `term` from the extern-pinned-Terms GC root set.  Mostly superseded by the managed-handle auto-unpin attached to TTerm itself; kept for callers that want to release a pin without dropping the WL wrapper.";
TExternPinCount::usage = "TExternPinCount[] returns the current number of entries in the external-caller pin table.  Useful for observing that WL's standard GC has dropped TTerm wrappers between evaluations.";
TTagName::usage   = "TTagName[tag] returns a string for a tag id.";

(* === heap === *)
THeapPos::usage   = "THeapPos[] returns the next free heap location (the upper bound of the active heap region).";
THeapBase::usage  = "THeapBase[] returns the lower bound of the active heap region.  Equal to 0 in pre-Cheney layouts and after thvm_init; equal to gc_from_start() once the GC has swapped semi-spaces, so heap iterators should walk [THeapBase[], THeapPos[]) to cover live cells.";
THeapAlloc::usage = "THeapAlloc[size] reserves `size` consecutive cells; returns the base loc.";
THeapRead::usage  = "THeapRead[loc] returns the Term at heap[loc].";
THeapSet::usage   = "THeapSet[loc, term] writes `term` to heap[loc].";
TGCCollect::usage = "TGCCollect[] runs a Cheney semi-space collection of the dyn heap; returns the new HEAP_NEXT (live cell count).";
TGCCount::usage   = "TGCCount[] returns the number of GC cycles since thvm_init.";
THeap::usage      = "THeap[] returns an Association snapshot with keys \"nextLoc\", \"cells\", \"Graph\".  See docs/heap_graph.md.";
THeapGraph::usage = "THeapGraph[] renders the heap state as an IC string-diagram Graph.  THeapGraph[term] also seeds discovery with `term` so heapless compounds held only by the WL caller appear.  THeapGraph[{t1, t2, ...}] seeds with several.  See docs/heap_graph.md.";
THeapDiagram::usage = "THeapDiagram[term] builds a Wolfram`DiagrammaticComputation`DiagramNetwork from the heap, with one Diagram per compound agent and one ERA Diagram per ERA cell.  Wires share string identifiers keyed off heap loc; VAR cells collapse to their binder loc.";

(* === reduce / stats === *)
TWnf::usage       = "TWnf[term] reduces `term` to weak normal form.  TWnf[term, n] bails after at most `n` interactions and returns the partially reduced term; pending eliminator frames are exposed via TStack[].  n = 0 means unbounded (same as TWnf[term]).";
TNf::usage        = "TNf[term] reduces `term` to full normal form by sweeping the live heap and firing every redex via redex_fire.  Where TWnf surfaces only the head, TNf reaches GRADs / KERNELs / OP2s nested anywhere in the graph.  Excludes TAG_REF / TAG_ALO from eager firing so recursive named definitions don't non-terminatingly unfold.";
TStep::usage      = "TStep[term] = TWnf[term, 1].  Fires exactly one interaction.  Inspect TStack[] for the pending frames.";
TStack::usage     = "TStack[] returns the eliminator frames pending at the most recent bail point of TStep / TWnf[_, n].  Each frame is a TTerm tagged APP / DP0 / DP1.  Empty list when no bail occurred.";
TRedexes::usage   = "TRedexes[] lists every redex in the live heap.  TRedexes[t] additionally DFS-walks `t` so a root the caller is holding directly is included.  Each entry is a TTerm uniquely identifying the redex by its packed Term value.";
TInteract::usage  = "TInteract[redex] fires exactly one interaction at `redex`.  Returns <| \"result\" -> TTerm, \"fresh\" -> {TTerm...} |> on success, or Failure[\"NotARedex\", ...] if `redex` is no longer reducible.  \"fresh\" lists the redex-status flips caused by this fire (locally produced + back-ref propagation into shared subgraphs).";
TReduce::usage    = "TReduce[term] reduces `term` to WNF in-place and returns `term` (the original root, useful as a seed for THeapGraph after reduction).";
TItrs::usage      = "TItrs[] returns the cumulative interaction count.";
TTermExpr::usage  = "TTermExpr[term] walks the heap from `term` and returns a nested expression whose heads are tag-name strings (\"LAM\", \"APP\", \"SUP\", \"DUP\", \"DP0\", \"DP1\", \"VAR\", \"ERA\").  Useful for snapshotting / diffing pre and post TWnf states by direct equality (===).";
TTermTree::usage  = "TTermTree[term] = ExpressionTree[TTermExpr[term]] -- the same structure rendered as a Wolfram Tree object for visual inspection.";

(* === high-level constructors === *)
TFreshLabel::usage = "TFreshLabel[] returns the next integer from a monotonic SUP/DUP label counter, then bumps it.  Reset by TReset[].";
TEra::usage       = "TEra[] constructs an eraser term.";
TVarFor::usage    = "TVarFor[lamLoc] constructs a VAR pointing at a binder loc.";
TLam::usage       = "TLam[x, body] constructs a lambda; HoldAll, so `x` is the binder symbol and `body` is the lambda body referring to it (e.g. TLam[w, TUOpAdd[w, w]]).  When the body is a UOP graph and the first TApp's argument carries a shape, the APP-LAM interaction JIT-materializes the body into a UOP_KERNEL with the bound var as a symbolic input slot -- compile-once, dispatch with each subsequent arg.  Bodies that aren't UOP graphs (e.g. curried lambdas, TIfZero) skip the JIT step.";
TLamShape::usage  = "TLamShape[shape_List, x, body] constructs a lambda whose bound variable carries an explicit shape annotation in the lam_shape side table.  Useful when the body needs to materialize BEFORE any TApp (e.g. for inspection / direct TMaterialize on the body); for the common case TLam alone is enough since the JIT path infers the shape from the first applied argument.";
TApp::usage       = "TApp[fun, arg] constructs an application.";
TSup::usage       = "TSup[a, b] constructs a SUP with a fresh label.  TSup[label, a, b] uses an explicit label.";
TDup::usage       = "TDup[body, k] constructs a DUP with a fresh label and calls `k[dp0, dp1]`.  TDup[label, body, k] uses an explicit label.";

(* === tag constants (mirror src/thvm.h) === *)
$TagAPP::usage = $TagLAM::usage = $TagVAR::usage = $TagERA::usage =
  $TagDP0::usage = $TagDP1::usage = $TagSUP::usage = $TagDUP::usage =
  $TagTEN::usage = $TagUOP::usage = $TagNUM::usage =
    "Tag id; mirrors the corresponding TAG_* in src/thvm.h.";

(* === dtype + opcode constants (mirror src/thvm.h) === *)
$DTF32::usage = $DTI32::usage =
    "Dtype id; mirrors DT_* in src/thvm.h.";

$UopMaterialize::usage = $UopKernel::usage = $UopConst::usage =
  $UopReshape::usage    = $UopPermute::usage = $UopExpand::usage =
  $UopPad::usage        = $UopShrink::usage = $UopFlip::usage =
  $UopAdd::usage        = $UopMul::usage = $UopNeg::usage =
  $UopRecip::usage      = $UopExp2::usage = $UopLog2::usage =
  $UopSqrt::usage       = $UopCmplt::usage = $UopReduce::usage =
  $UopGrad::usage       = $UopCmpeq::usage =
  $UopLoad::usage       =
    "UOp opcode id; mirrors UOP_* in src/thvm.h.";

$ReduceSum::usage = $ReduceMax::usage =
    "Reduce-kind id; mirrors REDUCE_* in src/thvm.h.";

(* === tensors === *)
TTensor::usage         = "TTensor[shape, dtype] allocates a tensor and returns a TTerm wrapping a TAG_TEN handle.  TTensor[shape, data_List] also writes initial values.  dtype defaults to \"f32\".";
TTensorCreate::usage   = "TTensorCreate[data] builds a tensor whose shape and dtype are inferred from `data`.  On the CPU backend the buffer is shared with the input NumericArray (zero copy).  PackedArrays and nested lists are first lifted to a NumericArray (one copy) then shared.";
TTensorShape::usage    = "TTensorShape[t] returns the tensor's shape as a list of integers.";
TTermShape::usage      = "TTermShape[t] runs the runtime's `term_shape_in` shape inference: returns a list of dim extents for a TEN, a shape-inferable UOP, or a TVAR with a registered shape annotation (TLamShape).  Returns {} when the shape cannot be determined.";
TTensorDType::usage    = "TTensorDType[t] returns the dtype as a string (\"f32\" / \"i32\").";
TTensorData::usage     = "TTensorData[t] reads the tensor's buffer as a NumericArray whose type matches the dtype (Real32 for f32, Integer32 for i32).  Wrap in `Normal` to get a plain list.";
TTensorRefcount::usage = "TTensorRefcount[t] returns the descriptor refcount (TENS[id].refcount).";
TRealize::usage        = "TRealize[expr] = TWnf[TMaterialize[expr]].  Fires the whole pipeline: heap-walk materialize (in-place rewrite UOPs to UOP_KERNELs) then beta-reduce + dispatch kernels.";
TMaterialize::usage    = "TMaterialize[expr] runs the schedule + kernelize + linearize rewrite directly (no wnf) and returns the scheduled DAG term.  Fires no kernels.  Use to visualize the graph after scheduling but before dispatch.";
TKernelCount::usage    = "TKernelCount[] returns the number of compiled KernelEntrys in the kernel side table.";
TKernelProgramCacheSize::usage = "TKernelProgramCacheSize[] returns the number of distinct KProgOp[] arrays interned in the kernel-program hash-cons cache.  After a TRealize, this is at most TKernelCount[]-1; structurally identical kernels (e.g. successive iters of a recursive lambda's step) share a single entry.";
TTensCount::usage     = "TTensCount[] returns the number of allocated TenDescs (excluding the reserved slot 0).";
TTotalBufBytes::usage = "TTotalBufBytes[] returns the sum of live CPU buffer bytes (refcount > 0).";

TKernelTable::usage   = "TKernelTable[] returns a list of {n_inputs, output_tid, fired, spliced, consumer_count, output_numel, output_dtype} per kernel (kid 1 .. KERNELS_NEXT - 1).  Used by TMemoryPlan to derive per-buf alloc/last_use depths.";
TKernelInputs::usage  = "TKernelInputs[kid] returns the input_tids of kernel `kid` (length n_inputs).";
TTensTable::usage     = "TTensTable[] returns a list of {producer_kid, buf_id, dtype, view_numel, view_contiguous, refcount, backend_id} per TenDesc (tid 1 .. TENS_NEXT - 1).  backend_id is 1 for CPU, 2 for Metal, 0 for unbound.";
TCpuBufTable::usage   = "TCpuBufTable[] returns a list of {nbytes, refcount, preserved, freeable, owns_data} per CPU buffer (buf_id 1 .. CPU_BUFS_NEXT - 1).";
TMetalBufTable::usage = "TMetalBufTable[] returns a list of {nbytes, refcount} per Metal buffer.  Empty when the dylib was built without Metal support.";
TKernelInfo::usage     = "TKernelInfo[kid] returns an Association describing the linearized program stored at KERNELS[kid].";

(* === UOp graph constructors === *)
TUOpConst::usage     = "TUOpConst[value, dtype] builds a UOP_CONST node carrying a scalar.";
TUOpReshape::usage   = "TUOpReshape[src, shape_List] builds a UOP_RESHAPE node.";
TUOpPermute::usage   = "TUOpPermute[src, axes_List] builds a UOP_PERMUTE node.";
TUOpExpand::usage    = "TUOpExpand[src, shape_List] builds a UOP_EXPAND node.";
TUOpPad::usage       = "TUOpPad[src, ranges_List] builds a UOP_PAD node.  ranges = {{b0,e0},{b1,e1},...}.";
TUOpShrink::usage    = "TUOpShrink[src, ranges_List] builds a UOP_SHRINK node.";
TUOpFlip::usage      = "TUOpFlip[src, axes_List] builds a UOP_FLIP node; axes is a list of axis indices to flip.";
TUOpAdd::usage       = "TUOpAdd[a, b] builds a UOP_ADD node.";
TUOpMul::usage       = "TUOpMul[a, b] builds a UOP_MUL node.";
TUOpNeg::usage       = "TUOpNeg[a] builds a UOP_NEG node.";
TUOpRecip::usage     = "TUOpRecip[a] builds a UOP_RECIP node.";
TUOpExp2::usage      = "TUOpExp2[a] builds a UOP_EXP2 node.";
TUOpLog2::usage      = "TUOpLog2[a] builds a UOP_LOG2 node.";
TUOpSqrt::usage      = "TUOpSqrt[a] builds a UOP_SQRT node.";
TUOpCmplt::usage     = "TUOpCmplt[a, b] builds a UOP_CMPLT node.";
TUOpCmpeq::usage     = "TUOpCmpeq[a, b] builds a UOP_CMPEQ node (elementwise a == b mask).";
TUOpReduce::usage    = "TUOpReduce[src, axis, kind] builds a UOP_REDUCE node; kind = \"SUM\" or \"MAX\".";
TUOpGrad::usage      = "TUOpGrad[y, gy] builds the BWD projection of a dup-flavored grad cell holding [y, gy].  gy is the cotangent (must match y's shape).  Reducing under TWnf threads gy down via the per-operator adjoint chain rule and emits SUP^{leaf_tid}(zero, gy_at_leaf) at each TEN leaf; outer DUPs at the WL surface (TGrad) extract the per-target gradient.";
TUOpLoad::usage      = "TUOpLoad[src] builds a UOP_LOAD node wrapping src.  Structural marker mirroring tinygrad's UOps.LOAD; runtime semantics are identity (memcpy in the cpu kernel).";

TAssign::usage       = "TAssign[dst, src] builds a UOP_ASSIGN node.  Wnf-fired in-place buffer write: once `src` reduces to a TAG_TEN, backend memcpy copies src.buf into dst.buf and the redex rewrites to dst.  Mirrors tinygrad's UOps.ASSIGN.  Use to mutate weight tensors in optimizer loops without allocating fresh tids per step.";

TUOpConv2D::usage    = "TUOpConv2D[input, weights, bias] builds a stride-1, no-padding 2-D convolution.  input shape {C_in, H, W}; weights {C_out, C_in, kh, kw}; bias {C_out}; output {C_out, H-kh+1, W-kw+1}.  Dispatches to TUOpConv2DLowered so autograd flows through primitives via the chain rule.";

TUOpConv2DLowered::usage = "TUOpConv2DLowered[input, weights, bias] builds the same valid 2-D convolution as TUOpConv2D but as a kh*kw-unrolled chain of primitive UOPs (SHRINK + RESHAPE + EXPAND + MUL + REDUCE_SUM + ADD).  No new opcodes; pure WL composition.";
TGrad::usage         = "TGrad[y, target] computes d(y)/d(target) via VJP.  Default cotangent seed = ones-at-y.shape (CONST(1.0) optionally expanded).  For non-default seeds use TGrad[y, target, gy].";
TGradMany::usage     = "TGradMany[y, {x_1, ..., x_n}] computes d(y)/d(x_i) for every target in one realize.  Returns a List of n TTerm wrappers.  Forward DAG is shared via heap-loc identity so the per-realize memo dedups every kernel emitted from those forward UOps across all n targets.";
TUOpKind::usage      = "TUOpKind[u] returns the opcode name for a UOp term.";
TUOpSrcs::usage      = "TUOpSrcs[u] returns the source-cell terms for a UOp term, in heap order.";
(* TATP::usage and TATP[] live in Kernel/ATP.wl (loaded via the
   sibling-file scan at the bottom of this file). *)

(* Forward-declare symbols owned by sibling files (Pri.wl, ...) so
   references from TWnf below don't get bound to phantom Private
   symbols.  Bare-evaluating the symbol name at BeginPackage scope
   creates the public THVMLink`X symbol; Pri.wl's later
   `BeginPackage["THVMLink`"]` reuses the same symbol. *)
{TPriDrain};

Begin["`Private`"];

$libDir = FileNameJoin[{
    DirectoryName[$InputFileName],
    "..", "LibraryResources", $SystemID
}];

$lib = FileNameJoin[{$libDir, "THVMLink" <> Switch[$OperatingSystem,
    "MacOSX", ".dylib", "Windows", ".dll", _, ".so"]}];

debugPrint[args___] := WriteString[$Output, StringJoin @@ Map[ToString, {args}], "\n"]

If[ ! FileExistsQ[$lib],
    debugPrint["[THVMLink] Library not built.  Run `make wl` from the repo root."];
    debugPrint["[THVMLink] Expected at: ", $lib]
];

(* Tag constants - keep in sync with src/thvm.h *)
$TagAPP = 0; $TagLAM = 1; $TagVAR = 2; $TagERA = 3;
$TagDP0 = 4; $TagDP1 = 5; $TagSUP = 6; $TagDUP = 7;
$TagTEN = 8; $TagUOP = 9; $TagNUM = 10;
$TagREF = 11; $TagALO = 12; $TagOP2 = 13; $TagMAT = 14;

(* DUP-cell flavor flag: TAG_DP{0,1} with this bit set on ext is a
   grad-flavored projection (DP0=FWD passthrough, DP1=BWD chain rule).
   See DUP_GRAD_FLAG in src/thvm.h. *)
$DupGradFlag = 2^17;

$tagNames = <|
    0  -> "APP", 1  -> "LAM", 2  -> "VAR",  3  -> "ERA",
    4  -> "DP0", 5  -> "DP1", 6  -> "SUP",  7  -> "DUP",
    8  -> "TEN", 9  -> "UOP", 10 -> "NUM",
    11 -> "REF", 12 -> "ALO", 13 -> "OP2",  14 -> "MAT"
|>;

$op2Names = <| 0 -> "+", 1 -> "-", 2 -> "*", 3 -> "==", 4 -> "<" |>;

(* Dtype constants - keep in sync with src/thvm.h *)
$DTF32 = 0; $DTI32 = 1;

dtypeCode["f32"] = $DTF32;  dtypeCode["i32"] = $DTI32;
dtypeCode[$DTF32] = $DTF32; dtypeCode[$DTI32] = $DTI32;
dtypeName[$DTF32] = "f32";  dtypeName[$DTI32] = "i32";

(* UOp opcode constants - keep in sync with src/thvm.h *)
$UopMaterialize = 0;  $UopKernel = 1;  $UopConst = 2;
$UopReshape = 3;      $UopPermute = 4; $UopExpand = 5;
$UopPad = 6;          $UopShrink = 7;  $UopFlip = 8;
$UopAdd = 9;          $UopMul = 10;    $UopNeg = 11;
$UopRecip = 12;       $UopExp2 = 13;   $UopLog2 = 14;
$UopSqrt = 15;        $UopCmplt = 16;  $UopReduce = 17;
$UopGrad = 18;        $UopFwd = 19;     $UopCmpeq = 20;
$UopLoad = 21;        $UopAssign = 22;
(* UOP_GRAD / UOP_FWD form a dup-like grad combinator -- both share
   a heap cell holding [y].  UOP_GRAD = backward projection (chain
   rule); UOP_FWD = forward projection (passthrough). *)

$uopNames = <|
    0  -> "MATERIALIZE", 1  -> "KERNEL", 2  -> "CONST",
    3  -> "RESHAPE",     4  -> "PERMUTE",5  -> "EXPAND",
    6  -> "PAD",         7  -> "SHRINK", 8  -> "FLIP",
    9  -> "ADD",         10 -> "MUL",    11 -> "NEG",
    12 -> "RECIP",       13 -> "EXP2",   14 -> "LOG2",
    15 -> "SQRT",        16 -> "CMPLT",  17 -> "REDUCE",
    18 -> "GRAD",        19 -> "FWD",   20 -> "CMPEQ",
    21 -> "LOAD",        22 -> "ASSIGN"
|>;

(* Reduce-kind constants *)
$ReduceSum = 0; $ReduceMax = 1;
reduceKindCode["SUM"] = $ReduceSum;  reduceKindCode["MAX"] = $ReduceMax;

TTagName[t_Integer] := Lookup[$tagNames, t, "TAG?" <> ToString[t]]

(* === library function loaders === *)
load[name_String, args_, ret_] := LibraryFunctionLoad[$lib, name, args, ret]

$initFn      := $initFn      = load["thvm_wl_init",       {},                       Integer];
$freeFn      := $freeFn      = load["thvm_wl_free",       {},                       Integer];
$resetFn     := $resetFn     = load["thvm_wl_reset",      {},                       Integer];

$termNewFn   := $termNewFn   = load["thvm_wl_term_new",   {Integer, Integer, Integer, Integer}, Integer];
$termTagFn   := $termTagFn   = load["thvm_wl_term_tag",   {Integer},                Integer];
$termExtFn   := $termExtFn   = load["thvm_wl_term_ext",   {Integer},                Integer];
$termValFn   := $termValFn   = load["thvm_wl_term_val",   {Integer},                Integer];
$termSubFn   := $termSubFn   = load["thvm_wl_term_sub",   {Integer},                Integer];
$termUnpinFn := $termUnpinFn = load["thvm_wl_term_unpin", {Integer},                Integer];
$externPinAssociateFn := $externPinAssociateFn =
    load["thvm_wl_extern_pin_associate", {Integer, Integer}, Integer];
$externPinCountFn := $externPinCountFn =
    load["thvm_wl_extern_pin_count", {}, Integer];
$externPinHandleGetFn := $externPinHandleGetFn =
    load["thvm_wl_extern_pin_handle_get", {Integer}, Integer];

$heapPosFn   := $heapPosFn   = load["thvm_wl_heap_pos",   {},                       Integer];
$heapBaseFn  := $heapBaseFn  = load["thvm_wl_heap_base",  {},                       Integer];
$heapAllocFn := $heapAllocFn = load["thvm_wl_heap_alloc", {Integer},                Integer];
$heapReadFn  := $heapReadFn  = load["thvm_wl_heap_read",  {Integer},                Integer];
$heapSetFn   := $heapSetFn   = load["thvm_wl_heap_set",   {Integer, Integer},       Integer];
$gcCollectFn := $gcCollectFn = load["thvm_wl_gc_collect", {},                       Integer];
$gcCountFn   := $gcCountFn   = load["thvm_wl_gc_count",   {},                       Integer];

$wnfFn          := $wnfFn          = load["thvm_wl_wnf",            {Integer},          Integer];
$wnfNFn         := $wnfNFn         = load["thvm_wl_wnf_n",          {Integer, Integer}, Integer];
$nfFn           := $nfFn           = load["thvm_wl_nf",              {Integer},          Integer];
$stackSizeFn    := $stackSizeFn    = load["thvm_wl_stack_size",     {},                 Integer];
$stackGetFn     := $stackGetFn     = load["thvm_wl_stack_get",      {Integer},          Integer];
$redexSnapFn    := $redexSnapFn    = load["thvm_wl_redex_snapshot", {{Integer, 1}},     Integer];
$redexGetFn     := $redexGetFn     = load["thvm_wl_redex_get",      {Integer},          Integer];
$interactFn     := $interactFn     = load["thvm_wl_interact",       {Integer},          Integer];
$itrsFn         := $itrsFn         = load["thvm_wl_itrs",           {},                 Integer];

(* tensor *)
$tensorAllocFn   := $tensorAllocFn   = load["thvm_wl_tensor_alloc",   {Integer, {Integer, 1}}, Integer];
$tensorWriteFn   := $tensorWriteFn   = load["thvm_wl_tensor_write",   {Integer, {Real, 1}},    Integer];
$tensorWriteIFn  := $tensorWriteIFn  = load["thvm_wl_tensor_write",   {Integer, {Integer, 1}}, Integer];
$tensorReadFn    := $tensorReadFn    = load["thvm_wl_tensor_read",    {Integer},               "NumericArray"];
$tensorShapeFn   := $tensorShapeFn   = load["thvm_wl_tensor_shape",   {Integer},               {Integer, 1}];
$tensorRcFn      := $tensorRcFn      = load["thvm_wl_tensor_refcount",{Integer},               Integer];

(* Zero-copy tensor-from-NumericArray.  The "Shared" passing mode
   tells WL to give the C side a shared reference; C bridge stores
   the handle and disowns on release.  *)
$tensorFromNAFn  := $tensorFromNAFn  = load["thvm_wl_tensor_from_na", {{"NumericArray", "Shared"}}, Integer];

(* ATP loaders, encoder, and TATP[] surface live in Kernel/ATP.wl. *)

(* uop graph *)
$uopConstFn    := $uopConstFn    = load["thvm_wl_uop_const",    {Integer, Real},                     Integer];
$uopUnaryFn    := $uopUnaryFn    = load["thvm_wl_uop_unary",    {Integer, Integer},                  Integer];
$uopBinaryFn   := $uopBinaryFn   = load["thvm_wl_uop_binary",   {Integer, Integer, Integer},         Integer];
$uopReduceFn   := $uopReduceFn   = load["thvm_wl_uop_reduce",   {Integer, Integer, Integer},         Integer];
$uopReshapeFn  := $uopReshapeFn  = load["thvm_wl_uop_reshape",  {Integer, {Integer, 1}},             Integer];
$uopPermuteFn  := $uopPermuteFn  = load["thvm_wl_uop_permute",  {Integer, {Integer, 1}},             Integer];
$uopExpandFn   := $uopExpandFn   = load["thvm_wl_uop_expand",   {Integer, {Integer, 1}},             Integer];
$uopPadFn      := $uopPadFn      = load["thvm_wl_uop_pad",      {Integer, {Integer, 1}},             Integer];
$uopShrinkFn   := $uopShrinkFn   = load["thvm_wl_uop_shrink",   {Integer, {Integer, 1}},             Integer];
$uopFlipFn     := $uopFlipFn     = load["thvm_wl_uop_flip",     {Integer, Integer},                  Integer];
$uopGradFn     := $uopGradFn     = load["thvm_wl_uop_grad",        {Integer, Integer},               Integer];
$uopGradWithTargetFn := $uopGradWithTargetFn = load["thvm_wl_uop_grad_with_target", {Integer, Integer, Integer}, Integer];
$uopFwdFn      := $uopFwdFn      = load["thvm_wl_uop_fwd",         {Integer, Integer},               Integer];
$termCtrNFn    := $termCtrNFn    = load["thvm_wl_term_ctr_n",      {Integer},                        Integer];
$termCtrAtFn   := $termCtrAtFn   = load["thvm_wl_term_ctr_at",     {Integer, Integer},               Integer];
$uopLoadFn     := $uopLoadFn     = load["thvm_wl_uop_load",        {Integer},                        Integer];

(* direct materialize (no wnf) + kernel-entry introspection *)
$materializeFn := $materializeFn = load["thvm_wl_materialize",     {Integer},                        Integer];
$realizeFn     := $realizeFn     = load["thvm_wl_realize",         {Integer},                        Integer];
$kernelCountFn := $kernelCountFn = load["thvm_wl_kernel_count",    {},                               Integer];
$kernelProgramCacheSizeFn := $kernelProgramCacheSizeFn = load["thvm_wl_kernel_program_cache_size", {}, Integer];
$lamShapeSetFn   := $lamShapeSetFn   = load["thvm_wl_lam_shape_set",   {Integer, {Integer, 1}},     Integer];
$lamShapeCountFn := $lamShapeCountFn = load["thvm_wl_lam_shape_count", {},                           Integer];
$termShapeInFn   := $termShapeInFn   = load["thvm_wl_term_shape_in",   {Integer},                    {Integer, 1}];
$kernelInfoFn  := $kernelInfoFn  = load["thvm_wl_kernel_info",     {Integer},                        {Integer, 1}];
$tensCountFn   := $tensCountFn   = load["thvm_wl_tens_count",      {},                               Integer];
$totalBufBytesFn := $totalBufBytesFn = load["thvm_wl_total_buf_bytes", {},                            Integer];

(* === TMemoryPlan snapshot tables (mp1) ===
   Each returns a flat Integer-1 MTensor; MemoryPlan.wl reshapes
   them into Association lists keyed by the per-row schema. *)
$kernelTableFn   := $kernelTableFn   = load["thvm_wl_kernel_table",    {},        {Integer, 1}];
$kernelInputsFn  := $kernelInputsFn  = load["thvm_wl_kernel_inputs",   {Integer}, {Integer, 1}];
$tensTableFn     := $tensTableFn     = load["thvm_wl_tens_table",      {},        {Integer, 1}];
$cpuBufTableFn   := $cpuBufTableFn   = load["thvm_wl_cpu_buf_table",   {},        {Integer, 1}];
$metalBufTableFn := $metalBufTableFn = load["thvm_wl_metal_buf_table", {},        {Integer, 1}];

TKernelTable[]    := Partition[Normal @ $kernelTableFn[],   7]
TKernelInputs[k_Integer] := Normal @ $kernelInputsFn[k]
TTensTable[]      := Partition[Normal @ $tensTableFn[],     7]
TCpuBufTable[]    := Partition[Normal @ $cpuBufTableFn[],   5]
TMetalBufTable[]  := Partition[Normal @ $metalBufTableFn[], 2]

TTensCount[]    := $tensCountFn[]
TTotalBufBytes[] := $totalBufBytesFn[]

(* === fresh-label counter (WL-side; per-context, reset by TReset).
       Keyed by ctx slot id from $contextCurrentFn[]. *)
$labelCounters = <|0 -> 1|>
TFreshLabel[] := With[{slot = $contextCurrentFn[]},
    With[{n = Lookup[$labelCounters, slot, 1]},
        $labelCounters[slot] = n + 1;
        n
    ]
]

(* Backward-compat read-only view: callers that mutated $labelCounter
   directly (e.g. Heap.wl HeapInitialize) get a per-context value
   instead of a single global. *)
$labelCounter /: Set[$labelCounter, n_Integer] :=
    ($labelCounters[$contextCurrentFn[]] = n; n)
$labelCounter := Lookup[$labelCounters, $contextCurrentFn[], 1]

(* === public API === *)
$initializedContexts = <||>
$initialized /: Set[$initialized, v_] :=
    ($initializedContexts[$contextCurrentFn[]] = TrueQ[v]; v)
$initialized := TrueQ @ Lookup[$initializedContexts, $contextCurrentFn[], False]

(* Any op that touches the heap calls ensureInit[] first.  TInit /
   TReset / TFree all flip $initialized themselves so a manual
   teardown still does the right thing.  $initialized is per-context
   so a TInit[] in slot 1 doesn't fool a slot-0 caller. *)
ensureInit[] := If[ ! $initialized, TInit[]]

TInit[]      := ($labelCounters[$contextCurrentFn[]] = 1;
                 $initializedContexts[$contextCurrentFn[]] = True;
                 $initFn[] === 1)
TFree[]      := ($initializedContexts[$contextCurrentFn[]] = False;
                 $freeFn[])
TReset[]     := ($labelCounters[$contextCurrentFn[]] = 1;
                 ensureInit[];
                 $resetFn[])

(* Explicit-context lifecycle overloads.  TInit[ctx]/TReset[ctx] run
   the corresponding C-side function in the given context's slot
   (auto-restores on exit).  Useful when you've allocated a context
   via TContextNew[] and want to set it up without entering a
   TInContext[...] block. *)
TInit[ctx_TContext]  := TInContext[ctx, TInit[]]
TFree[ctx_TContext]  := TInContext[ctx, TFree[]]
TReset[ctx_TContext] := TInContext[ctx, TReset[]]

(* === TTerm atomic object ===
   `TTerm[id_Integer]` is the canonical wrapper around a packed 64-bit
   `Term` value.  All constructors (TLam, TApp, TSup, TDup, TEra,
   TVarFor) return TTerm-wrapped values; all inspectors and the heap
   API accept either a TTerm or a raw `Integer` so internal helpers
   (heapWith, etc.) can stay scalar-friendly.  The MakeBoxes summary
   box for TTerm lives in Format.wl. *)

(* TTerm canonical form is `TTerm[ctxSlot_Integer, raw_Integer,
   handle_]` where `handle` is a `ManagedLibraryExpression`-backed
   pin that anchors the underlying C-side Term as a GC root for
   as long as the WL value is reachable.  When WL collects the
   TTerm value, the handle's manager fires on the C side and the
   pin is dropped automatically.

   Bare 1-arg `TTerm[raw]` and 2-arg `TTerm[ctx, raw]` (from
   legacy code or fresh bridge results) auto-normalize to the
   tagged 3-arg form so pattern-matchers downstream only ever
   see 3-arg. *)
TTerm[id_Integer] := TTerm[$contextCurrentFn[], id]
TTerm[c_Integer, id_Integer] := TTerm[c, id, makePinHandle[id]]

(* Internal extractors.  3-arg TTerm with a handle: refresh the
   raw via the C-side pin table, which is the source of truth post-
   GC.  The cached `id` is the construction-time encoding; if a
   copying GC has moved the underlying heap loc since, the
   refreshed value reflects the new loc.  Fallback to the cached
   id if the handle's C-side entry is zero (e.g. the handle was
   dropped but the WL value is lingering). *)
ttermRaw[TTerm[_Integer, id_Integer, h_]] := With[
    {fresh = $externPinHandleGetFn[ManagedLibraryExpressionID[h]]},
    If[ fresh =!= 0, fresh, id]
]
ttermRaw[id_Integer]                     := id

ttermCtx[TTerm[c_Integer, _Integer, _]]  := c
ttermCtx[_Integer]                       := 0

(* Build a fresh ManagedLibraryExpression["ExternPin"] handle and
   wire it to `raw` on the C side.  The returned handle becomes
   part of the TTerm value; when WL collects the TTerm (and thus
   the handle), the registered manager calls extern_unpin_term
   for us.  Construct uniquely each time so independent TTerm
   wrappers don't share lifetime -- duplicate pins on the same
   Term are fine (the table holds duplicates and unpin removes
   one matching entry per call). *)
makePinHandle[raw_Integer] := With[
    {h = CreateManagedLibraryExpression["ExternPin", ExternPin]},
    $externPinAssociateFn[ManagedLibraryExpressionID[h], raw];
    h
]

(* Two TTerm wrappers are Equal (==) when they reference the
   same (ctx, raw).  The managed handle is part of the GC
   story, not the identity story; SameQ (===) is FullForm
   identity and correctly distinguishes wrappers with distinct
   handles, so use == for "same underlying Term" comparisons. *)
TTerm /: Equal[TTerm[c1_Integer, r1_Integer, _], TTerm[c2_Integer, r2_Integer, _]] :=
    c1 === c2 && r1 === r2

(* Pack a fresh TTerm from raw fields.  Private; callers use the
   high-level constructors.  The 1-arg `TTerm[raw]` immediately
   auto-normalizes to `TTerm[slot, raw]`. *)
packTerm[sub_Integer, tag_Integer, ext_Integer, val_Integer] :=
    TTerm[$termNewFn[sub, tag, ext, val]]

(* Auto-switch helper `withTermCtx` is defined in Context.wl (after
   `TContext` is registered in the public `THVMLink`` namespace).
   THVMLink.wl is parsed first, so a forward reference here would
   resolve `TContext` to `THVMLink`Private`TContext` and silently
   break the auto-switch -- a wrong-shadow that the catch-all
   `withCtx[_, expr_] := expr` then absorbs. *)

(* Inspectors accept either TTerm or Integer. *)
TTermTag[t_]                    := $termTagFn[ttermRaw[t]]
TTermExt[t_]                    := $termExtFn[ttermRaw[t]]
TTermVal[t_]                    := $termValFn[ttermRaw[t]]
TTermSub[t_]                    := $termSubFn[ttermRaw[t]]
TTermUnpin[t_]                  := (ensureInit[]; $termUnpinFn[ttermRaw[t]])
TExternPinCount[]               := (ensureInit[]; $externPinCountFn[])

(* TTerm methods: only the canonical 3-arg form is reachable; bare
   `TTerm[id]` and `TTerm[ctx, id]` auto-normalize before any
   rule fires. *)
TTerm[c_Integer, id_Integer, _]["raw"]     := id
TTerm[c_Integer, id_Integer, _]["tag"]     := $termTagFn[id]
TTerm[c_Integer, id_Integer, _]["ext"]     := $termExtFn[id]
TTerm[c_Integer, id_Integer, _]["val"]     := $termValFn[id]
TTerm[c_Integer, id_Integer, _]["sub"]     := $termSubFn[id]
TTerm[c_Integer, id_Integer, _]["tagName"] := TTagName[$termTagFn[id]]
TTerm[c_Integer, _Integer, _]["ctx"]       := c
TTerm[c_Integer, id_Integer, _]["info"]    := <|
    "sub"     -> $termSubFn[id],
    "tag"     -> $termTagFn[id],
    "tagName" -> TTagName[$termTagFn[id]],
    "ext"     -> $termExtFn[id],
    "val"     -> $termValFn[id],
    "raw"     -> id,
    "ctx"     -> c
|>

THeapPos[]                       := (ensureInit[]; $heapPosFn[])
THeapBase[]                      := (ensureInit[]; $heapBaseFn[])
THeapAlloc[size_Integer]         := (ensureInit[]; $heapAllocFn[size])
THeapRead[loc_Integer]           := (ensureInit[]; TTerm[$heapReadFn[loc]])
THeapSet[loc_Integer, t_]        := (ensureInit[]; $heapSetFn[loc, ttermRaw[t]])
TGCCollect[]                     := (ensureInit[]; $gcCollectFn[])
TGCCount[]                       := (ensureInit[]; $gcCountFn[])

TWnf[t_]                       := Module[{r},
    ensureInit[];
    r = withTermCtx[t, TTerm[$wnfFn[ttermRaw[t]]]];
    (* Auto-drain queued PRI callbacks (Function/Symbol slots that
       prim_pri couldn't invoke synchronously).  CompiledFunction
       slots already fired during wnf via callLibraryCallbackFunction;
       this drain only catches the queued path.  No-op when the queue
       is empty. *)
    TPriDrain[];
    r
]
TWnf[t_, n_Integer /; n >= 0]  := Module[{r},
    ensureInit[];
    r = withTermCtx[t, TTerm[$wnfNFn[ttermRaw[t], n]]];
    TPriDrain[];
    r
]
TNf[t_]                        := (ensureInit[]; withTermCtx[t, TTerm[$nfFn[ttermRaw[t]]]])

(* TStep[t] = TWnf[t, 1] -- fire exactly one interaction, then return
   the partially reduced term.  The pending eliminator stack at the
   bail point is captured in TStack[]. *)
TStep[t_] := TWnf[t, 1]

(* TStack[] returns the eliminator frames that were still pending
   when the most recent TStep / TWnf[t, n] bailed.  Each frame is a
   TTerm tagged APP / DP0 / DP1; the `val` field points to the heap
   loc of the original cell.  Returns {} for unbounded TWnf or when
   the bounded run completed before hitting the budget. *)
TStack[] := (ensureInit[];
    With[{n = $stackSizeFn[]},
        Table[TTerm[$stackGetFn[i]], {i, 0, n - 1}]
    ]
)

(* TRedexes[] -- list every redex in the live heap (no root walk).
   TRedexes[roots___] -- additionally DFS-walks each root so Terms
   the caller is holding directly (which may not be stored in any
   heap cell) are included.  Returns a list of TTerm cells deduped
   by packed Term value. *)
snapshotRedexes[rootRaws_List] := Module[{n},
    n = $redexSnapFn[rootRaws];
    Table[$redexGetFn[i], {i, 0, n - 1}]
]

TRedexes[roots___] := (ensureInit[];
    TTerm /@ snapshotRedexes[ttermRaw /@ {roots}]
)

(* TInteract[redex] / TInteract[redex, root | {roots...}] -- fire
   ONE interaction at `redex`.  Returns
       <| "result" -> TTerm,     -- replaces the redex
          "fresh"  -> {TTerm...} -- redex-status flips caused by this fire
       |>
   on success, or Failure["NotARedex", ...] if `redex` isn't
   reducible right now.  Pass `root` (the caller's outer term) so
   the "fresh" diff sees status flips on cells reachable from root
   but not stored anywhere in the heap. *)
TInteract[redex_TTerm, roots___] := withTermCtx[redex, Module[{
    redexRaw, rootRaws, pre, result, post, fresh
},
    ensureInit[];
    redexRaw = ttermRaw[redex];
    rootRaws = ttermRaw /@ Flatten[{roots}];
    pre = snapshotRedexes[Join[{redexRaw}, rootRaws]];
    result = $interactFn[redexRaw];
    If[ result === 0,
        Return @ Failure["NotARedex",
            <| "Message" -> "TInteract: cell is not a redex right now",
               "redex"   -> redex |>]
    ];
    post  = snapshotRedexes[Join[{result}, rootRaws]];
    fresh = TTerm /@ Complement[post, pre];
    <| "result" -> TTerm[result], "fresh" -> fresh |>
]]

(* TReduce reduces `t` to WNF in-place and returns the original root.
   Pairs with THeapGraph[t] / TTermTree[t] when you want the
   post-reduction state seeded from the term you constructed. *)
TReduce[t_] := (TWnf[t]; t)

TItrs[]          := (ensureInit[]; $itrsFn[])

(* === heap walker: term -> nested string-headed expression ===
   Walks from `t` through the heap and returns the structural shape
   with heads "LAM" / "APP" / "SUP" / "DUP" / "DP0" / "DP1" / "VAR" /
   "ERA".  Compound args bases are tracked in a `seen` association so
   cycles produce a `"Cycle"[loc]` leaf instead of looping.

   `TTermExpr` returns the raw nested expression (cheap structural
   equality via `===`).  `TTermTree` wraps it in `ExpressionTree[...]`
   so it renders as a Wolfram `Tree` object for visual inspection. *)

TTermExpr[t_] := tTreeWalk[t, <||>]
TTermTree[t_] := ExpressionTree[TTermExpr[t]]

(* How many heap cells does a UOP store?  Mirrors the data-arity
   used in src/book/from_dynamic.c (NOT uop_arity, which counts
   compute operands -- e.g. CONST has arity 0 but stores 1 cell). *)
uopCellCount[op_] := Switch[op,
    $UopConst,                                                      1,
    $UopAdd | $UopMul | $UopCmplt | $UopCmpeq,                      2,
    $UopNeg | $UopRecip | $UopExp2 | $UopLog2 | $UopSqrt,           1,
    $UopReduce,                                                     3,
    $UopGrad,                                                       1,
    $UopFwd,                                                        1,
    $UopKernel,                                                     2,
    $UopAssign,                                                     2,
    (* RESHAPE: report 1 (the src) so TTermExpr renders UOP[RESHAPE,
       <src-subtree>].  The trailing NUM(d_i) cells are integer
       parameters, not children of structural interest. *)
    $UopReshape,                                                    1,
    $UopLoad,                                                       1,
    _,                                                              0
]

tTreeWalk[t_, seen_] := Block[{
    raw = ttermRaw[t], tag, val, ext, seen2, n
},
    tag = $termTagFn[raw];
    val = $termValFn[raw];
    ext = $termExtFn[raw];
    Switch[tag,
        $TagERA, "ERA",
        $TagVAR, "VAR"[val],
        (* Drop the dtype field for the common i32 case; surface it
           only when the cell holds an f32 (CONST scalar payload).
           Keeps "NUM"[2] readable for kernel kid / view dim cells. *)
        $TagNUM, If[ext === 0, "NUM"["f32", val], "NUM"[val]],
        $TagTEN, "TEN"[val],
        $TagREF, "REF"[ext],
        (* DP0 / DP1 recurse into the dup body so each projection's
           subtree is visible.  Trees can't share, so the same body
           appears once under each projection. *)
        $TagDP0, "DP0"[ext, tTreeWalk[$heapReadFn[val], seen]],
        $TagDP1, "DP1"[ext, tTreeWalk[$heapReadFn[val], seen]],
        $TagLAM,
            If[ KeyExistsQ[seen, val], "Cycle"[val],
                seen2 = Append[seen, val -> True];
                "LAM"[tTreeWalk[$heapReadFn[val], seen2]]],
        $TagAPP,
            If[ KeyExistsQ[seen, val], "Cycle"[val],
                seen2 = Append[seen, val -> True];
                "APP"[tTreeWalk[$heapReadFn[val], seen2],
                      tTreeWalk[$heapReadFn[val + 1], seen2]]],
        $TagSUP,
            If[ KeyExistsQ[seen, val], "Cycle"[val],
                seen2 = Append[seen, val -> True];
                "SUP"[ext,
                    tTreeWalk[$heapReadFn[val], seen2],
                    tTreeWalk[$heapReadFn[val + 1], seen2]]],
        $TagDUP,
            If[ KeyExistsQ[seen, val], "Cycle"[val],
                seen2 = Append[seen, val -> True];
                "DUP"[ext, tTreeWalk[$heapReadFn[val], seen2]]],
        $TagALO,
            If[ KeyExistsQ[seen, val], "Cycle"[val],
                seen2 = Append[seen, val -> True];
                (* heap[val]   = wrapped term
                   heap[val+1] = state id (atomic, just record it) *)
                "ALO"[$termValFn[$heapReadFn[val + 1]],
                      tTreeWalk[$heapReadFn[val], seen2]]],
        $TagOP2,
            If[ KeyExistsQ[seen, val], "Cycle"[val],
                seen2 = Append[seen, val -> True];
                "OP2"[Lookup[$op2Names, ext, "?" <> ToString[ext]],
                      tTreeWalk[$heapReadFn[val + 0], seen2],
                      tTreeWalk[$heapReadFn[val + 1], seen2]]],
        $TagMAT,
            If[ KeyExistsQ[seen, val], "Cycle"[val],
                seen2 = Append[seen, val -> True];
                "MAT"[ext,
                      tTreeWalk[$heapReadFn[val + 0], seen2],
                      tTreeWalk[$heapReadFn[val + 1], seen2]]],
        $TagUOP,
            If[ KeyExistsQ[seen, val], "Cycle"[val],
                seen2 = Append[seen, val -> True];
                n = uopCellCount[ext];
                If[ ext === $UopKernel,
                    (* Surface the C-side KernelEntry.input_tids[] as
                       additional "TEN"[tid] args so KERNEL renders as
                       "UOP"["KERNEL", kid, "TEN"[out_tid], "TEN"[in1],
                       "TEN"[in2], ...].  Without this the input_tids
                       are invisible (they don't appear in any heap
                       child of the UOP_KERNEL cell). *)
                    Module[{kid    = $termValFn[$heapReadFn[val + 1]],
                            outTid = $termValFn[$heapReadFn[val]],
                            inTids},
                        inTids = TKernelInputs[kid];
                        "UOP" @@ Join[
                            {"KERNEL", kid, "TEN"[outTid]},
                            "TEN" /@ inTids
                        ]
                    ],
                    "UOP" @@ Prepend[
                        Table[tTreeWalk[$heapReadFn[val + i], seen2], {i, 0, n - 1}],
                        Lookup[$uopNames, ext, "UOP?" <> ToString[ext]]
                    ]
                ]],
        _, "Unknown"[tag]
    ]
]

(* === high-level constructors (all return TTerm) === *)

heapWith[fields__] := With[{loc = THeapAlloc[Length[{fields}]]},
    ScanIndexed[THeapSet[loc + First[#2] - 1, #1] &, {fields}];
    loc
]

heapTerm[tag_Integer, ext_Integer, fields__] :=
    packTerm[0, tag, ext, heapWith[fields]]

TEra[]                  := packTerm[0, $TagERA, 0, 0]
TVarFor[lamLoc_Integer] := packTerm[0, $TagVAR, 0, lamLoc]

TApp[fun_, arg_] := heapTerm[$TagAPP, 0, fun, arg]

TSup[a_, b_]                          := TSup[TFreshLabel[], a, b]
TSup[label_Integer, a_, b_]           := heapTerm[$TagSUP, label, a, b]

(* TLam is JIT-aware by default: when the body is a UOP graph
   (compute) and the argument carries a shape (TEN), the APP-LAM
   interaction registers the bound var's shape, materializes the
   body into a UOP_KERNEL, and only then proceeds with the
   standard beta.  The kernel's TVAR input slot resolves through
   SUB to `arg` at fire time.  Bodies that aren't UOP graphs
   (curried lambdas, TIfZero, etc.) skip materialize -- it'd be
   a no-op anyway.  See src/interact/app_lam.c. *)
SetAttributes[TLam, HoldAll]
TLam[x_Symbol, body_] := With[{loc = THeapAlloc[1]},
    THeapSet[loc, Function[x, body][TVarFor[loc]]];
    packTerm[0, $TagLAM, 0, loc]
]

(* TLamShape[shape_list, x, body] -- explicit shape annotation
   for the bound variable.  Useful when the body needs to
   materialize BEFORE any TApp (e.g. inspection / direct
   TMaterialize on the body), or when the argument's shape can't
   be inferred at first APP.  Most callers don't need this; just
   use TLam and the JIT path will infer the shape from the
   first applied argument. *)
SetAttributes[TLamShape, HoldAll]
TLamShape[shape_List, x_Symbol, body_] := With[{loc = THeapAlloc[1]},
    THVMLink`Private`$lamShapeSetFn[loc, shape];
    THeapSet[loc, Function[x, body][TVarFor[loc]]];
    packTerm[0, $TagLAM, 0, loc]
]

(* Sugar: TTerm[ctx, id][arg] == TApp[TTerm[ctx, id], arg] lets the
   user write `id[era]` instead of `TApp[id, era]`. *)
TTerm[c_Integer, id_Integer, _][y_TTerm]   := TApp[TTerm[c, id], y]
TTerm[c_Integer, id_Integer, _][y_Integer] := TApp[TTerm[c, id], y]

TDup[body_, k_]                       := TDup[TFreshLabel[], body, k]
TDup[label_Integer, body_, k_] := With[{loc = heapWith[body]},
    k[packTerm[0, $TagDP0, label, loc],
      packTerm[0, $TagDP1, label, loc]]
]

(* === heap graph rendering ===
   Defined in Visualization.wl (loaded below).  Public symbol
   THeapGraph; per-tag shapes / colours are private. *)

THeap[] := Block[{lo = THeapBase[], n = THeapPos[]},
    THeap[<|
        "nextLoc" -> n,
        "cells"   -> Association @ Table[
            i -> THeapRead[i],
            {i, lo, n - 1}
        ],
        "Graph"   -> THeapGraph[]
    |>]
]

THeap[a_Association][k_] := a[k]
THeap /: KeyExistsQ[THeap[a_Association], k_] := KeyExistsQ[a, k]
THeap /: Keys[THeap[a_Association]]           := Keys[a]
THeap /: Values[THeap[a_Association]]         := Values[a]
THeap /: Normal[THeap[a_Association]]         := a

End[];
EndPackage[];

(* === sibling files ===
   Each sibling has its own BeginPackage["THVMLink`"] + Begin[`Private`]
   block, so they all land in the shared THVMLink`Private` context and
   can call each other's helpers without qualification.  Definition
   order doesn't matter (every cross-file reference uses SetDelayed),
   so we Get them in alphabetical order via FileNames -- adding a new
   sibling means dropping it in this directory; no edits here. *)
With[{here = $InputFileName},
    Scan[
        Get,
        Sort @ Select[
            FileNames["*.wl", DirectoryName[here]],
            FileBaseName[#] =!= "THVMLink" &
        ]
    ]
]
