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
TBookRead::usage  = "TBookRead[loc] returns the Term at book_heap[loc].  Useful for inspecting CTR cells allocated via TBookCtr or built by the Metal AOT kernel (which writes into book_heap via aot_book_alloc).";
THeapSet::usage   = "THeapSet[loc, term] writes `term` to heap[loc].";
TGCCollect::usage = "TGCCollect[] runs a Cheney semi-space collection of the dyn heap; returns the new HEAP_NEXT (live cell count).";
TGCCount::usage   = "TGCCount[] returns the number of GC cycles since thvm_init.";
(* TKernelSource / TKernelFlops / TKernelDispatchKind / TKernelDispatchCount /
   TKernelTotalUs / TKernelJitDylibPath / TKernelProfile / TProfileAll
   -- declared and defined in Kernel.wl as TKernel-property accessors. *)
THeap::usage      = "THeap[] returns an Association snapshot with keys \"nextLoc\", \"cells\", \"Graph\".  See docs/heap_graph.md.";
THeapGraph::usage = "THeapGraph[] renders the heap state as an IC string-diagram Graph.  THeapGraph[term] also seeds discovery with `term` so heapless compounds held only by the WL caller appear.  THeapGraph[{t1, t2, ...}] seeds with several.  See docs/heap_graph.md.";
THeapDiagram::usage = "THeapDiagram[term] builds a Wolfram`DiagrammaticComputation`DiagramNetwork from the heap, with one Diagram per compound agent and one ERA Diagram per ERA cell.  Wires share string identifiers keyed off heap loc; VAR cells collapse to their binder loc.";

(* === reduce / stats === *)
TWnf::usage       = "TWnf[term] reduces `term` to weak normal form.  TWnf[term, n] bails after at most `n` interactions and returns the partially reduced term; pending eliminator frames are exposed via TStack[].  n = 0 means unbounded (same as TWnf[term]).";
TNf::usage        = "TNf[term] reduces `term` to full normal form: nf-sweeps the live heap firing every redex via redex_fire, then runs cnf at the surviving root so the user-visible term is DP-free.  Where TWnf surfaces only the head, TNf reaches GRADs / KERNELs / OP2s nested anywhere in the graph.  Excludes TAG_REF / TAG_ALO from eager firing so recursive named definitions don't non-terminatingly unfold.";
TCnf::usage       = "TCnf[term] runs the cnf readback layer (src/cnf/_.c): reduces to WHNF then lifts the first SUP to the top, recursively driving plain DP projections through their dup interactions.  Use this when you need a DP-free reading of a term without paying for nf's whole-heap sweep.";
TCollapse::usage  = "TCollapse[t] / TCollapse[t, cap] enumerates the SUP-tree of `t` -- recursively walks SUP / ERA branches, collecting non-SUP non-ERA leaves.  Returns a List of TTerms.  Default cap is 65536 leaves; passes through the C-side thvm_collapse walker (src/collapse/_.c).";
TStep::usage      = "TStep[term] = TWnf[term, 1].  Fires exactly one interaction.  Inspect TStack[] for the pending frames.";
TStack::usage     = "TStack[] returns the eliminator frames pending at the most recent bail point of TStep / TWnf[_, n].  Each frame is a TTerm tagged APP / DP0 / DP1.  Empty list when no bail occurred.";
TRedexes::usage   = "TRedexes[] lists every redex in the live heap.  TRedexes[t] additionally DFS-walks `t` so a root the caller is holding directly is included.  Each entry is a TTerm uniquely identifying the redex by its packed Term value.";
TInteract::usage  = "TInteract[redex] fires exactly one interaction at `redex`.  Returns <| \"result\" -> TTerm, \"fresh\" -> {TTerm...} |> on success, or Failure[\"NotARedex\", ...] if `redex` is no longer reducible.  \"fresh\" lists the redex-status flips caused by this fire (locally produced + back-ref propagation into shared subgraphs).";
TReduce::usage    = "TReduce[term] reduces `term` to WNF in-place and returns `term` (the original root, useful as a seed for THeapGraph after reduction).";
TItrs::usage      = "TItrs[] returns the cumulative interaction count.";
TTermExpr::usage  = "TTermExpr[term] walks the heap from `term` and returns a nested expression whose heads are tag-name strings (\"LAM\", \"APP\", \"SUP\", \"DUP\", \"DP0\", \"DP1\", \"VAR\", \"ERA\").  Useful for snapshotting / diffing pre and post TWnf states by direct equality (===).";
TTermTree::usage  = "TTermTree[term] = ExpressionTree[TTermExpr[term]] -- the same structure rendered as a Wolfram Tree object for visual inspection.";
TTermSubexprs::usage = "TTermSubexprs[term] returns a List of `path -> TTerm` rules covering every position reachable from `term`, pre-order DFS.  Path is a List of integer heap offsets (root has empty path).  Sibling of TTermExpr/TTermTree: same traversal, different output shape -- pairs path-locators with the live subterms so callers can substitute or compare at specific positions.";
TSubexprAt::usage = "TSubexprAt[term, path] navigates `term` along `path` (a List of integer offsets) and returns the subterm as a TTerm.  Returns Missing[\"OutOfBounds\", path] when the route doesn't fit the term shape.";

(* === high-level constructors === *)
TFreshLabel::usage = "TFreshLabel[] returns the next integer from a monotonic SUP/DUP label counter, then bumps it.  Reset by TReset[].";
TEra::usage       = "TEra[] constructs an eraser term.";
TVarFor::usage    = "TVarFor[lamLoc] constructs a VAR pointing at a binder loc.";
TLam::usage       = "TLam[x, body] constructs a lambda; HoldAll, so `x` is the binder symbol and `body` is the lambda body referring to it (e.g. TLam[w, TUOpAdd[w, w]]).  When the body is a UOP graph and the first TApp's argument carries a shape, the APP-LAM interaction JIT-materializes the body into a UOP_KERNEL with the bound var as a symbolic input slot -- compile-once, dispatch with each subsequent arg.  Bodies that aren't UOP graphs (e.g. curried lambdas, TIfZero) skip the JIT step.";
TLamShape::usage  = "TLamShape[shape_List, x, body] constructs a lambda whose bound variable carries an explicit shape annotation in the lam_shape side table.  Useful when the body needs to materialize BEFORE any TApp (e.g. for inspection / direct TMaterialize on the body); for the common case TLam alone is enough since the JIT path infers the shape from the first applied argument.";
TApp::usage       = "TApp[fun, arg] constructs an application.";
TSup::usage       = "TSup[a, b] constructs a SUP with a fresh label.  TSup[label, a, b] uses an explicit label.  Bare-Integer children are lifted to i32 NUMs, so TSup[1, 2] === TSup[TNum[1], TNum[2]] (every heapTerm-based constructor does this -- see numCoerce).";
TDsu::usage       = "TDsu[label, a, b] constructs a dynamic-label SUP (HVM4 DSU): the label is a TTerm reduced strict-left at wnf time, after which DSU collapses to SUP^n / ERA / nested-SUP based on what the label resolved to.  Useful for pattern compilers that need a fresh label per match instance.";
TDdu::usage       = "TDdu[label, val, body] constructs a dynamic-label DUP (HVM4 DDU): same shape as TDsu but on the DUP side.  body must be a 2-arg LAM-pair; once label resolves to NUM(n), the DDU reduces to body(X0, X1) where X0/X1 are projections of DUP^n on val.";
TTermEq::usage    = "TTermEq[a, b] returns True if `a` and `b` cnf-reduce to structurally equal terms (modulo VAR alpha-aliasing), False otherwise.  Drives both sides through cnf so DP-rooted projections fire and SUP heads lift.  Use TTermSame for the no-reduction variant on already-CNF'd terms.";
TTermSame::usage = "TTermSame[a, b] returns True if `a` and `b` are structurally equal without further reduction.  Compares tag/ext/val and recurses on children; same-loc compounds are trivially equal.  Cheap when callers already have CNF terms; for general use prefer TTermEq.";
TDup::usage       = "TDup[body] constructs a DUP with a fresh label and returns the pair {dp0, dp1} (DP0/DP1 tags sharing one dup cell).  TDup[label, body] uses an explicit integer label.  TDup[body, k] / TDup[label, body, k] are CPS variants that call k[dp0, dp1] instead.";

(* === tag constants (mirror src/thvm.h) === *)
$TagAPP::usage = $TagLAM::usage = $TagVAR::usage = $TagERA::usage =
  $TagDP0::usage = $TagDP1::usage = $TagSUP::usage = $TagDUP::usage =
  $TagTEN::usage = $TagUOP::usage = $TagNUM::usage = $TagCTR::usage =
  $TagREF::usage = $TagALO::usage = $TagOP2::usage = $TagMAT::usage =
  $TagEQL::usage = $TagAND::usage = $TagOR::usage  = $TagANY::usage =
  $TagINC::usage = $TagWHEN::usage = $TagFVR::usage = $TagBRI::usage =
  $TagANN::usage = $TagPRI::usage = $TagDSU::usage = $TagDDU::usage =
    "Tag id; mirrors the corresponding TAG_* in src/thvm.h.";

(* === dtype + opcode constants (mirror src/thvm.h) === *)
$DTBool::usage = $DTInt8::usage = $DTUInt8::usage =
  $DTInt16::usage = $DTUInt16::usage = $DTInt32::usage =
  $DTUInt32::usage = $DTInt64::usage = $DTUInt64::usage =
  $DTFp8E4M3::usage = $DTFp8E5M2::usage =
  $DTFp16::usage = $DTBf16::usage =
  $DTFp32::usage = $DTFp64::usage =
  $DTInt4::usage = $DTUInt4::usage =
    "Dtype id; mirrors DT_* in src/thvm.h.";

$UopMaterialize::usage = $UopKernel::usage = $UopConst::usage =
  $UopReshape::usage    = $UopPermute::usage = $UopExpand::usage =
  $UopPad::usage        = $UopShrink::usage = $UopFlip::usage =
  $UopAdd::usage        = $UopMul::usage = $UopNeg::usage =
  $UopRecip::usage      = $UopExp2::usage = $UopLog2::usage =
  $UopSqrt::usage       = $UopCmplt::usage = $UopReduce::usage =
  $UopGrad::usage       = $UopCmpeq::usage =
  $UopLoad::usage       = $UopAssign::usage =
  $UopCast::usage       = $UopBitcast::usage =
  $UopRange::usage      = $UopIndexE::usage =
  $UopIAdd::usage       = $UopISub::usage = $UopIMul::usage =
  $UopIDiv::usage       = $UopIMod::usage = $UopILt::usage =
  $UopIAnd::usage       = $UopIWhere::usage = $UopInvalid::usage =
  $UopBuffer::usage     = $UopStore::usage = $UopAfter::usage =
  $UopOpt::usage        =
    "UOp opcode id; mirrors UOP_* in src/thvm.h.";

$ReduceSum::usage = $ReduceMax::usage =
    "Reduce-kind id; mirrors REDUCE_* in src/thvm.h.";

$UopScopeGlobal::usage = $UopScopeLocal::usage = $UopScopeReg::usage =
    "UOP_BUFFER scope tag; mirrors UOP_SCOPE_GLOBAL/LOCAL/REG in src/thvm.h.";

(* === tensors === *)
TTensor::usage         = "TTensor[shape, dtype] allocates a tensor and returns a TTerm wrapping a TAG_TEN handle.  TTensor[shape, data_List] also writes initial values.  dtype defaults to \"f32\".";
TTensorCreate::usage   = "TTensorCreate[data] / TTensorCreate[data, dtype] builds a TTerm tensor whose shape and dtype are inferred from `data` (or coerced to the supplied dtype = \"f32\" / \"i32\").  On the active backend the buffer is shared with the input NumericArray (zero copy on CPU).  PackedArrays and nested lists are first lifted to a NumericArray (one copy) then shared.  Backend selection is global (TBackend / DEV env var); per-tensor backend is a Phase 14+ extension.";
TTensorShape::usage    = "TTensorShape[t] returns the tensor's shape as a list of integers.";
TTermShape::usage      = "TTermShape[t] runs the runtime's `term_shape_in` shape inference: returns a list of dim extents for a TEN, a shape-inferable UOP, or a TVAR with a registered shape annotation (TLamShape).  Returns {} when the shape cannot be determined.";
TTensorDType::usage    = "TTensorDType[t] returns the dtype as a string (\"f32\" / \"i32\").";
TTensorData::usage     = "TTensorData[t] reads the tensor's buffer as a NumericArray whose type matches the dtype (Real32 for f32, Integer32 for i32).  Wrap in `Normal` to get a plain list.";
TTensorRefcount::usage = "TTensorRefcount[t] returns the descriptor refcount (TENS[id].refcount).";
TRequiresGrad::usage   = "TRequiresGrad[t] / TRequiresGrad[t, True|False] sets TenDesc.requires_grad, the canonical \"this tensor is a parameter\" flag consulted by uop_grad's leaf rule.  Mirrors PyTorch / tinygrad .requires_grad_(); returns t for chaining.";
TRequiresGradQ::usage  = "TRequiresGradQ[t] reads TenDesc.requires_grad (True / False).";
TRealize::usage        = "TRealize[expr] = TWnf[TMaterialize[expr]].  Fires the whole pipeline: heap-walk materialize (in-place rewrite UOPs to UOP_KERNELs) then beta-reduce + dispatch kernels.";
TMaterialize::usage    = "TMaterialize[expr] runs the schedule + kernelize + linearize rewrite directly (no wnf) and returns the scheduled DAG term.  Fires no kernels.  Use to visualize the graph after scheduling but before dispatch.";
(* TKernelCount / TKernelProgramCacheSize / TKernelInfo  --  declared
   and defined in Kernel.wl.

   TKernelTable / TKernelInputs / TTensTable / TCpuBufTable /
   TMetalBufTable / TTensCount / TTotalBufBytes  --  declared and
   defined in MemoryPlan.wl ("mp1 bridge tables"). *)

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
TUOpCast::usage      = "TUOpCast[src, dtype] builds a UOP_CAST node.  dtype is one of \"f32\"/\"i32\"/\"i8\" etc.  Backward gradient (under TGrad) is a CAST back to src.dtype.";
TUOpGrad::usage      = "TUOpGrad[y, gy] builds the BWD projection of a dup-flavored grad cell holding [y, gy].  gy is the cotangent (must match y's shape).  Reducing under TWnf threads gy down via the per-operator adjoint chain rule and emits SUP^{leaf_tid}(zero, gy_at_leaf) at each TEN leaf; outer DUPs at the WL surface (TGrad) extract the per-target gradient.";
TUOpLoad::usage      = "TUOpLoad[src] builds a UOP_LOAD node wrapping src.  Structural marker mirroring tinygrad's UOps.LOAD; runtime semantics are identity (memcpy in the cpu kernel).";

(* Phase E UOp constructors. *)
TUOpRange::usage   = "TUOpRange[axisId, axisType, extent] builds a UOP_RANGE leaf.  axisType is one of $KaxLoop / $KaxReduce / $KaxUpcast / $KaxUnroll / $KaxLocal / $KaxGlobal / $KaxGroupReduce.";
TUOpBuffer::usage  = "TUOpBuffer[scope, dtype, dims, instance] builds a UOP_BUFFER node.  scope is $UopScopeGlobal/Local/Reg; dtype is the dtype id; dims is a list of integers; instance disambiguates input slots (0 = output, 1.. = inputs).";
TUOpIndexE::usage  = "TUOpIndexE[buf, addr] builds a UOP_INDEX_E node pairing a buffer with a symbolic address tree.";
TUOpIAdd::usage = TUOpISub::usage = TUOpIMul::usage = TUOpIDiv::usage =
  TUOpIMod::usage = TUOpILt::usage = TUOpIAnd::usage =
    "TUOpI<X>[a, b] builds the matching integer-binary UOP node.  Used to construct symbolic address trees over UOP_RANGE leaves and UOP_CONST stride coefficients.";
TUOpIWhere::usage  = "TUOpIWhere[cond, then, else] builds a UOP_IWHERE ternary select.";
TUOpInvalid::usage = "TUOpInvalid[] builds a UOP_INVALID sentinel (used for PAD-mask padding).";
TUOpOpt::usage     = "TUOpOpt[target, kind, factor] wraps `target` with a UOP_OPT annotation.  `kind` is one of $OptUnroll / $OptUpcast / $OptTC / $OptLocal / $OptGroupReduce / $OptConv / $OptFastMath / $OptSimdReduce / $OptVecLoad.";
TUOpStore::usage   = "TUOpStore[buf, addr, value] builds a UOP_STORE root.  Typical kernel root: STORE(out_buf, out_addr, value-expression).";
TUOpAfter::usage   = "TUOpAfter[node, afterNode] builds a UOP_AFTER ordering edge: `node` happens-after `afterNode`.";
TUOpIConst::usage  = "TUOpIConst[v] builds a UOP_CONST(DT_INT32) for use as a stride coefficient.  The DAG-side classifiers pattern-match on UOP_CONST, not on bare TAG_NUM atoms.";

TAssign::usage       = "TAssign[dst, src] builds a UOP_ASSIGN node.  Wnf-fired in-place buffer write: once `src` reduces to a TAG_TEN, backend memcpy copies src.buf into dst.buf and the redex rewrites to dst.  Mirrors tinygrad's UOps.ASSIGN.  Use to mutate weight tensors in optimizer loops without allocating fresh tids per step.";

(* TConv2D / TConv2D / TConv2D all live in NN.wl now. *)

(* (see above) *)
TGrad::usage         = "TGrad[y, target] computes d(y)/d(target) via VJP.  Default cotangent seed = ones-at-y.shape (CONST(1.0) optionally expanded).  For non-default seeds use TGrad[y, target, gy].";
TGradMany::usage     = "TGradMany[y, {x_1, ..., x_n}] computes d(y)/d(x_i) for every target via per-target TUOpGradWithTarget calls sharing the y subgraph by heap-loc identity.  Returns a List of n TTerm wrappers.  Caller decides how to realize -- forward kernels dedup across calls via TenDesc.producer_kid once any target's realize fires them; grad chain-rule work does NOT share across targets (the per-realize grad_memo keys on target).";
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
$TagEQL = 15; $TagAND = 16; $TagOR  = 17; $TagANY = 18;
$TagINC = 19; $TagCTR = 20; $TagWHEN = 21; $TagFVR = 22;
$TagBRI = 23; $TagANN = 24; $TagPRI = 25;
$TagDSU = 29; $TagDDU = 30;

(* DUP-cell flavor flag: TAG_DP{0,1} with this bit set on ext is a
   grad-flavored projection (DP0=FWD passthrough, DP1=BWD chain rule).
   See DUP_GRAD_FLAG in src/thvm.h. *)
$DupGradFlag = 2^17;

$tagNames = <|
    0  -> "APP", 1  -> "LAM", 2  -> "VAR",  3  -> "ERA",
    4  -> "DP0", 5  -> "DP1", 6  -> "SUP",  7  -> "DUP",
    8  -> "TEN", 9  -> "UOP", 10 -> "NUM",
    11 -> "REF", 12 -> "ALO", 13 -> "OP2",  14 -> "MAT",
    15 -> "EQL", 16 -> "AND", 17 -> "OR",   18 -> "ANY",
    19 -> "INC", 20 -> "CTR", 21 -> "WHEN", 22 -> "FVR",
    23 -> "BRI", 24 -> "ANN", 25 -> "PRI",
    29 -> "DSU", 30 -> "DDU"
|>;

$op2Names = <| 0 -> "+", 1 -> "-", 2 -> "*", 3 -> "==", 4 -> "<" |>;

(* Dtype constants - keep in sync with src/thvm.h.  The enum mirrors
   tinygrad's full dtype set; Phase A wires only f32/i32 through the
   bridge, but the constants for every slot are reserved up front so
   later phases land without churning every WL-facing name. *)
$DTBool   =  0;  $DTInt8   =  1;  $DTUInt8   =  2;
$DTInt16  =  3;  $DTUInt16 =  4;  $DTInt32   =  5;
$DTUInt32 =  6;  $DTInt64  =  7;  $DTUInt64  =  8;
$DTFp8E4M3=  9;  $DTFp8E5M2= 10;  $DTFp16    = 11;
$DTBf16   = 12;  $DTFp32   = 13;  $DTFp64    = 14;
$DTInt4   = 15;  $DTUInt4  = 16;

dtypeCode["bool"]    = $DTBool;
dtypeCode["i8"]      = $DTInt8;     dtypeCode["u8"]   = $DTUInt8;
dtypeCode["i16"]     = $DTInt16;    dtypeCode["u16"]  = $DTUInt16;
dtypeCode["i32"]     = $DTInt32;    dtypeCode["u32"]  = $DTUInt32;
dtypeCode["i64"]     = $DTInt64;    dtypeCode["u64"]  = $DTUInt64;
dtypeCode["fp8e4m3"] = $DTFp8E4M3;  dtypeCode["fp8e5m2"] = $DTFp8E5M2;
dtypeCode["f16"]     = $DTFp16;     dtypeCode["bf16"] = $DTBf16;
dtypeCode["f32"]     = $DTFp32;     dtypeCode["f64"]  = $DTFp64;
dtypeCode["i4"]      = $DTInt4;     dtypeCode["u4"]   = $DTUInt4;

(* Numeric -> numeric identity (for code that already passes a code). *)
Do[ With[ {c = i }, dtypeCode[c] = c ], { i, 0, 16 } ];

dtypeName[$DTBool   ] = "bool";
dtypeName[$DTInt8   ] = "i8";       dtypeName[$DTUInt8   ] = "u8";
dtypeName[$DTInt16  ] = "i16";      dtypeName[$DTUInt16  ] = "u16";
dtypeName[$DTInt32  ] = "i32";      dtypeName[$DTUInt32  ] = "u32";
dtypeName[$DTInt64  ] = "i64";      dtypeName[$DTUInt64  ] = "u64";
dtypeName[$DTFp8E4M3] = "fp8e4m3";  dtypeName[$DTFp8E5M2 ] = "fp8e5m2";
dtypeName[$DTFp16   ] = "f16";      dtypeName[$DTBf16    ] = "bf16";
dtypeName[$DTFp32   ] = "f32";      dtypeName[$DTFp64    ] = "f64";
dtypeName[$DTInt4   ] = "i4";       dtypeName[$DTUInt4   ] = "u4";

(* UOp opcode constants - keep in sync with src/thvm.h *)
$UopMaterialize = 0;  $UopKernel = 1;  $UopConst = 2;
$UopReshape = 3;      $UopPermute = 4; $UopExpand = 5;
$UopPad = 6;          $UopShrink = 7;  $UopFlip = 8;
$UopAdd = 9;          $UopMul = 10;    $UopNeg = 11;
$UopRecip = 12;       $UopExp2 = 13;   $UopLog2 = 14;
$UopSqrt = 15;        $UopCmplt = 16;  $UopReduce = 17;
$UopGrad = 18;        $UopFwd = 19;     $UopCmpeq = 20;
$UopLoad = 21;        $UopAssign = 22;
$UopCast = 23;        $UopBitcast = 24;
(* Phase E additions -- INDEX layer + BUFFER/STORE/AFTER/OPT *)
$UopRange = 25;       $UopIndexE = 26;
$UopIAdd = 27;        $UopISub = 28;    $UopIMul = 29;
$UopIDiv = 30;        $UopIMod = 31;    $UopILt = 32;
$UopIAnd = 33;        $UopIWhere = 34;  $UopInvalid = 35;
$UopBuffer = 36;      $UopStore = 37;   $UopAfter = 38;
$UopOpt = 39;
(* UOP_BUFFER scope tag.  Mirrors UOP_SCOPE_GLOBAL/LOCAL/REG in src/thvm.h:
   GLOBAL = device memory (Tensor argument default), LOCAL = threadgroup-
   shared, REG = per-thread fragment. *)
$UopScopeGlobal = 0; $UopScopeLocal = 1; $UopScopeReg = 2;
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
    21 -> "LOAD",        22 -> "ASSIGN",
    23 -> "CAST",        24 -> "BITCAST",
    (* Phase E additions: INDEX layer + BUFFER/STORE/AFTER/OPT.  After
       extending these, TTermExpr emits stable string heads instead of
       "UOP?<n>" fallbacks for ops 25-39, which the WL rewrite layer
       (Rewrite.wl) pattern-matches against in KOpt rules. *)
    25 -> "RANGE",       26 -> "INDEX_E",
    27 -> "IADD",        28 -> "ISUB",   29 -> "IMUL",
    30 -> "IDIV",        31 -> "IMOD",   32 -> "ILT",
    33 -> "IAND",        34 -> "IWHERE", 35 -> "INVALID",
    36 -> "BUFFER",      37 -> "STORE",  38 -> "AFTER",
    39 -> "OPT"
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
$termNewDsuFn := $termNewDsuFn = load["thvm_wl_term_new_dsu", {Integer, Integer, Integer}, Integer];
$termNewDduFn := $termNewDduFn = load["thvm_wl_term_new_ddu", {Integer, Integer, Integer}, Integer];
$termEqStructFn := $termEqStructFn = load["thvm_wl_term_eq_struct", {Integer, Integer}, Integer];
$termEqFn       := $termEqFn       = load["thvm_wl_term_eq",        {Integer, Integer}, Integer];
$lamSealExtFn := $lamSealExtFn = load["thvm_wl_lam_seal_ext", {Integer, Integer},      Integer];
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
$kernelSourceCFn     := $kernelSourceCFn     = load["thvm_wl_kernel_source_c",     {Integer}, "UTF8String"];
$kernelSourceMetalFn := $kernelSourceMetalFn = load["thvm_wl_kernel_source_metal", {Integer}, "UTF8String"];
$kernelFlopsFn         := $kernelFlopsFn         = load["thvm_wl_kernel_flops",          {Integer}, Integer];
$kernelStoreRootFn     := $kernelStoreRootFn     = load["thvm_wl_kernel_store_root",     {Integer}, Integer];
$kernelDispatchKindFn  := $kernelDispatchKindFn  = load["thvm_wl_kernel_dispatch_kind",  {Integer}, Integer];
$kernelDispatchCountFn := $kernelDispatchCountFn = load["thvm_wl_kernel_dispatch_count", {Integer}, Integer];
$kernelTotalUsFn       := $kernelTotalUsFn       = load["thvm_wl_kernel_total_us",       {Integer}, Integer];
$kernelJitDylibPathFn  := $kernelJitDylibPathFn  = load["thvm_wl_kernel_jit_dylib_path", {Integer}, "UTF8String"];

$wnfFn          := $wnfFn          = load["thvm_wl_wnf",            {Integer},          Integer];
$wnfNFn         := $wnfNFn         = load["thvm_wl_wnf_n",          {Integer, Integer}, Integer];
$nfFn           := $nfFn           = load["thvm_wl_nf",              {Integer},          Integer];
$cnfFn          := $cnfFn          = load["thvm_wl_cnf",             {Integer},          Integer];
$collapseFn     := $collapseFn     = load["thvm_wl_collapse",        {Integer, Integer}, {Integer, 1}];
$stackSizeFn    := $stackSizeFn    = load["thvm_wl_stack_size",     {},                 Integer];
$stackGetFn     := $stackGetFn     = load["thvm_wl_stack_get",      {Integer},          Integer];
$redexSnapFn    := $redexSnapFn    = load["thvm_wl_redex_snapshot", {{Integer, 1}},     Integer];
$redexGetFn     := $redexGetFn     = load["thvm_wl_redex_get",      {Integer},          Integer];
$interactFn     := $interactFn     = load["thvm_wl_interact",       {Integer},          Integer];
$stepBeginFn    := $stepBeginFn    = load["thvm_wl_step_begin",     {{Integer, 1}},     Integer];
$stepFireFn     := $stepFireFn     = load["thvm_wl_step_fire",      {Integer},          Integer];
$stepFreshFn    := $stepFreshFn    = load["thvm_wl_step_fresh",     {},                 Integer];
$stepEndFn      := $stepEndFn      = load["thvm_wl_step_end",       {},                 Integer];
$itrsFn         := $itrsFn         = load["thvm_wl_itrs",           {},                 Integer];
$hotCountersFn      := $hotCountersFn      = load["thvm_wl_hot_counters",       {}, {Integer, 1}];
$hotCountersResetFn := $hotCountersResetFn = load["thvm_wl_hot_counters_reset", {}, Integer];

(* multicomputation reduction trace (Multicomputation.wl owns the
   surface).  Present in every standard `make wl` dylib (built with
   -DTHVM_TRACE); a custom trace-free build has stub versions, and
   $multiTraceSupportedFn[] reports 0. *)
$multiTraceSupportedFn := $multiTraceSupportedFn = load["thvm_wl_multi_trace_supported", {},        Integer];
$multiTraceInitFn      := $multiTraceInitFn      = load["thvm_wl_multi_trace_init",      {Integer}, Integer];
$multiTraceResetFn     := $multiTraceResetFn     = load["thvm_wl_multi_trace_reset",     {},        Integer];
$multiTraceFreeFn      := $multiTraceFreeFn      = load["thvm_wl_multi_trace_free",      {},        Integer];
$multiTraceSetFn       := $multiTraceSetFn       = load["thvm_wl_multi_trace_set",       {Integer}, Integer];
$multiTraceCountFn     := $multiTraceCountFn     = load["thvm_wl_multi_trace_count",     {},        Integer];
$multiTraceSnapshotFn  := $multiTraceSnapshotFn  = load["thvm_wl_multi_trace_snapshot",  {},        {Integer, 2}];
$multiRuleNameFn       := $multiRuleNameFn       = load["thvm_wl_multi_rule_name",       {Integer}, "UTF8String"];
$multiFamilyNameFn     := $multiFamilyNameFn     = load["thvm_wl_multi_family_name",     {Integer}, "UTF8String"];
$multiWireProvSnapshotFn := $multiWireProvSnapshotFn = load["thvm_wl_multi_wire_prov_snapshot", {}, {Integer, 1}];

(* tensor *)
$tensorAllocFn   := $tensorAllocFn   = load["thvm_wl_tensor_alloc",   {Integer, {Integer, 1}}, Integer];
$tensorWriteFn   := $tensorWriteFn   = load["thvm_wl_tensor_write",   {Integer, {Real, 1}},    Integer];
$tensorWriteIFn  := $tensorWriteIFn  = load["thvm_wl_tensor_write",   {Integer, {Integer, 1}}, Integer];
$tensorReadFn    := $tensorReadFn    = load["thvm_wl_tensor_read",    {Integer},               "NumericArray"];
$tensorShapeFn   := $tensorShapeFn   = load["thvm_wl_tensor_shape",   {Integer},               {Integer, 1}];
$tensorRcFn      := $tensorRcFn      = load["thvm_wl_tensor_refcount",{Integer},               Integer];
$tensorSetReqGradFn := $tensorSetReqGradFn = load["thvm_wl_tensor_set_requires_grad", {Integer, Integer}, Integer];
$tensorReqGradFn := $tensorReqGradFn = load["thvm_wl_tensor_requires_grad",     {Integer},          Integer];
$tensorViewDbgFn := $tensorViewDbgFn = load["thvm_wl_tensor_view_debug", {Integer}, {Integer, 1}];

(* Zero-copy tensor-from-NumericArray.  The "Shared" passing mode
   tells WL to give the C side a shared reference; C bridge stores
   the handle and disowns on release.  *)
$tensorFromNAFn  := $tensorFromNAFn  = load["thvm_wl_tensor_from_na", {{"NumericArray", "Shared"}}, Integer];
$tensorFromNATypedFn := $tensorFromNATypedFn = load["thvm_wl_tensor_from_na_typed", {{"NumericArray", "Shared"}, Integer, {Integer, 1}}, Integer];

(* f16 / bf16 round-trip helpers: pack a Real list into a
   UnsignedInteger16 NumericArray of raw narrow-float bytes; unpack the
   inverse direction.  Used by Tensor.wl's TFP16ToReal / TRealToFP16
   / TBf16ToReal / TRealToBf16 surface. *)
$fp16PackFn   := $fp16PackFn   = load["thvm_wl_fp16_pack",   {{Real, 1}, Integer}, "NumericArray"];
$fp16UnpackFn := $fp16UnpackFn = load["thvm_wl_fp16_unpack", {{"NumericArray", "Constant"}, Integer}, {Real, 1}];
$fp8PackFn    := $fp8PackFn    = load["thvm_wl_fp8_pack",    {{Real, 1}, Integer}, "NumericArray"];
$fp8UnpackFn  := $fp8UnpackFn  = load["thvm_wl_fp8_unpack",  {{"NumericArray", "Constant"}, Integer}, {Real, 1}];
$int4PackFn   := $int4PackFn   = load["thvm_wl_int4_pack",   {{Integer, 1}, Integer}, "NumericArray"];
$int4UnpackFn := $int4UnpackFn = load["thvm_wl_int4_unpack", {{"NumericArray", "Constant"}, Integer, Integer}, {Integer, 1}];

(* ATP loaders, encoder, and TATP[] surface live in Kernel/ATP.wl. *)

(* uop graph *)
$uopConstFn    := $uopConstFn    = load["thvm_wl_uop_const",    {Integer, Real},                     Integer];
$uopUnaryFn    := $uopUnaryFn    = load["thvm_wl_uop_unary",    {Integer, Integer},                  Integer];
$uopCastFn     := $uopCastFn     = load["thvm_wl_uop_cast",     {Integer, Integer},                  Integer];
$uopBitcastFn  := $uopBitcastFn  = load["thvm_wl_uop_bitcast",  {Integer, Integer},                  Integer];
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

(* Phase E UOp constructors (RANGE / INDEX_E / IADD..IAND / IWHERE /
   INVALID / BUFFER / STORE / AFTER / OPT) -- mirror the matching
   py_uop_* exports.  Used by Rewrite.wl + rewrite.wlt to build
   canonical DAGs for cross-validation against C apply_opt_dag. *)
$uopRangeFn      := $uopRangeFn      = load["thvm_wl_uop_range",      {Integer, Integer, Integer},      Integer];
$uopBufferFn     := $uopBufferFn     = load["thvm_wl_uop_buffer",     {Integer, Integer, {Integer, 1}, Integer}, Integer];
$uopIndexEFn     := $uopIndexEFn     = load["thvm_wl_uop_index_e",    {Integer, Integer},               Integer];
$uopIntBinaryFn  := $uopIntBinaryFn  = load["thvm_wl_uop_int_binary", {Integer, Integer, Integer},      Integer];
$uopIWhereFn     := $uopIWhereFn     = load["thvm_wl_uop_iwhere",     {Integer, Integer, Integer},      Integer];
$uopInvalidFn    := $uopInvalidFn    = load["thvm_wl_uop_invalid",    {},                               Integer];
$uopOptFn        := $uopOptFn        = load["thvm_wl_uop_opt",        {Integer, Integer, Integer},      Integer];
$uopStoreFn      := $uopStoreFn      = load["thvm_wl_uop_store",      {Integer, Integer, Integer},      Integer];
$uopAfterFn      := $uopAfterFn      = load["thvm_wl_uop_after",      {Integer, Integer},               Integer];
$termIConstFn    := $termIConstFn    = load["thvm_wl_term_iconst",    {Integer},                        Integer];

(* direct materialize (no wnf) + kernel-entry introspection *)
$materializeFn := $materializeFn = load["thvm_wl_materialize",     {Integer},                        Integer];
$realizeFn     := $realizeFn     = load["thvm_wl_realize",         {Integer},                        Integer];
$realizeManyFn := $realizeManyFn = load["thvm_wl_realize_many",    {{Integer, 1}},                   Integer];
$kernelCountFn := $kernelCountFn = load["thvm_wl_kernel_count",    {},                               Integer];
$kernelProgramCacheSizeFn := $kernelProgramCacheSizeFn = load["thvm_wl_kernel_program_cache_size", {}, Integer];
$kernelProgramKeyFn       := $kernelProgramKeyFn       = load["thvm_wl_kernel_program_key",        {Integer}, Integer];
$kernelAxesGetFn          := $kernelAxesGetFn          = load["thvm_wl_kernel_axes_get",            {Integer}, {Integer, 1}];
$kernelApplyOptFn         := $kernelApplyOptFn         = load["thvm_wl_kernel_apply_opt",           {Integer, Integer, Integer, Integer}, Integer];
$kernelProposeFn          := $kernelProposeFn          = load["thvm_wl_kernel_propose",             {Integer}, {Integer, 1}];
$kernelAutotuneFn         := $kernelAutotuneFn         = load["thvm_wl_kernel_autotune",            {Integer}, Integer];
$kernelBenchUsFn          := $kernelBenchUsFn          = load["thvm_wl_kernel_bench_us",            {Integer, Integer}, Integer];
$kernelBenchVariantsFn    := $kernelBenchVariantsFn    = load["thvm_wl_kernel_bench_variants",      {Integer}, {Integer, 1}];
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
$metalBufSummaryFn := $metalBufSummaryFn = load["thvm_wl_metal_buf_summary", {},  {Integer, 1}];
$metalGpuTimeFn  := $metalGpuTimeFn  = load["thvm_wl_metal_gpu_time",  {},        {Integer, 1}];
$metalPerOpProfileFn := $metalPerOpProfileFn = load["thvm_wl_metal_perop_profile", {}, {Integer, 1}];

$uopLeafTidsFn   := $uopLeafTidsFn   = load["thvm_wl_uop_leaf_tids",   {Integer}, {Integer, 1}];

(* TKernelTable / TKernelInputs / TTensTable / TCpuBufTable /
   TMetalBufTable / TMetalBufSummary / TMetalMemoryProfile /
   TTensCount / TTotalBufBytes -- defined in MemoryPlan.wl. *)

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

   Bare 1-arg `TTerm[raw]` and 2-arg `TTerm[ctx, raw]` auto-
   normalize to the tagged 3-arg form so pattern-matchers downstream
   only ever see 3-arg.  Forward-compat for shorter ctor forms used
   by fresh bridge results. *)
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

(* numCoerce -- the term-constructor sugar that lifts a bare Integer
   field to a NUM -- is defined in Switch.wl (next to TNum, which it
   calls; defining it here would capture a stale private TNum symbol
   since TNum is declared later).  heapWith below references it as a
   forward symbol; Switch.wl's definition lands on the same
   THVMLink`Private`numCoerce. *)

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
TBookRead[loc_Integer]           := (ensureInit[]; TTerm[$bookReadFn[loc]])
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
TCnf[t_]                       := (ensureInit[]; withTermCtx[t, TTerm[$cnfFn[ttermRaw[t]]]])

(* TCollapse[t]: enumerate the SUP-tree of a term.  Walks the SUP
   structure under `t`, cnfing at each step to surface the head.
   SUP -> recurse into both branches; ERA -> drop; otherwise leaf.
   Returns a List of TTerms (the surviving pure leaves).  Default
   cap is 65536 leaves. *)
TCollapse[t_]                  := TCollapse[t, 65536]
TCollapse[t_, cap_Integer]     := (
    ensureInit[];
    TTerm /@ $collapseFn[ttermRaw[t], cap]
)

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
   but not stored anywhere in the heap.

   Internally drives the C-side step session: each call attaches
   (one heap walk to build the inverse-index + parent map), fires
   in O(uses-of-redex), drains the fresh set, then detaches.  Wrap
   in TStepBegin / TStepEnd to amortise the attach over many fires. *)
$stepSessionActive = False;

TInteract[redex_TTerm, roots___] := withTermCtx[redex, Module[{
    redexRaw, rootRaws, result, freshN, owned
},
    ensureInit[];
    redexRaw = ttermRaw[redex];
    rootRaws = ttermRaw /@ Flatten[{roots}];
    owned = !TrueQ[$stepSessionActive];
    If[ owned, $stepBeginFn[Join[{redexRaw}, rootRaws]] ];
    result = $stepFireFn[redexRaw];
    If[ result === 0,
        If[ owned, $stepEndFn[] ];
        Return @ Failure["NotARedex",
            <| "Message" -> "TInteract: cell is not a redex right now",
               "redex"   -> redex |>]
    ];
    freshN = $stepFreshFn[];
    Module[{ res = <| "result" -> TTerm[result],
                      "fresh"  -> TTerm /@ Table[$redexGetFn[i], {i, 0, freshN - 1}] |> },
        If[ owned, $stepEndFn[] ];
        res
    ]
]]

(* TStepBegin[roots] / TStepEnd[] -- amortise the attach over many
   TInteract calls.  Recommended for long stepping sessions where
   HEAP_NEXT is large; without these wrappers, every TInteract pays
   one attach (a single linear heap walk to build the inverse
   index).  Within a session, each TInteract is O(local). *)
TStepBegin[roots___] := (ensureInit[];
    $stepBeginFn[ttermRaw /@ Flatten[{roots}]];
    $stepSessionActive = True;
)

TStepEnd[] := (
    If[ TrueQ[$stepSessionActive],
        $stepEndFn[];
        $stepSessionActive = False;
    ]
)

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

(* === Subexpression traversal ====================================
   Sibling of TTermExpr/TTermTree: same per-tag arity logic, but
   the output is a flat List of (path -> TTerm) rules covering every
   reachable position.  Pattern.wl uses this to walk a term and try
   a rule at every subexpression; users can navigate to a specific
   position with TSubexprAt[t, path]. *)

(* (offset, child-raw) pairs for a heap term's structural children.
   Per-tag arities cover LAM/DUP/DP*/ALO (1), APP/SUP/OP2/MAT/EQL/
   AND/OR/WHEN/ANN/BRI (2), DSU/DDU (3), CTR (variable from leading
   NUM(arity)), UOP (uopArity[opcode]), and atoms (0).  The "offset"
   returned is the path-segment integer used by TSubexprAt to
   navigate; CTR data starts at offset 1 (the arity NUM at +0 is
   metadata). *)
subexprChildren[raw_Integer] := Block[{
    tag = $termTagFn[raw], val = $termValFn[raw], ext = $termExtFn[raw], n
},
    Switch[ tag,
        $TagLAM | $TagDUP | $TagDP0 | $TagDP1 | $TagALO,
            {{0, $heapReadFn[val]}},
        $TagAPP | $TagSUP,
            {{0, $heapReadFn[val]}, {1, $heapReadFn[val + 1]}},
        $TagOP2 | $TagEQL | $TagAND | $TagOR | $TagWHEN | $TagANN | $TagMAT | $TagBRI,
            {{0, $heapReadFn[val]}, {1, $heapReadFn[val + 1]}},
        $TagDSU | $TagDDU,
            {{0, $heapReadFn[val + 0]},
             {1, $heapReadFn[val + 1]},
             {2, $heapReadFn[val + 2]}},
        $TagCTR,
            (n = $termValFn[$heapReadFn[val]];
             Table[{i, $heapReadFn[val + i]}, {i, 1, n}]),
        $TagUOP,
            With[{ar = uopArity[ext]},
                Table[{i, $heapReadFn[val + i]}, {i, 0, ar - 1}]],
        _, {}
    ]
]

walkSubexprsRaw[raw_Integer, path_List] := Block[{
    children = subexprChildren[raw]
},
    Prepend[
        Catenate[
            (walkSubexprsRaw[#[[2]], Append[path, #[[1]]]] & /@ children)
        ],
        path -> raw
    ]
]

TTermSubexprs[t_TTerm] := (
    ensureInit[];
    (#[[1]] -> TTerm[#[[2]]]) & /@ walkSubexprsRaw[ttermRaw[t], {}]
)

TSubexprAt[t_TTerm, path_List] := Block[{
    raw = ttermRaw[t], k, ch
},
    Catch[
        Do[
            ch = subexprChildren[raw];
            k  = SelectFirst[ch, First[#] === offset &, None];
            If[ k === None, Throw[Missing["OutOfBounds", path]]];
            raw = k[[2]],
            {offset, path}
        ];
        TTerm[raw]
    ]
]

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
    (* Movement ops: report 1 (the src) so TTermExpr renders
       UOP[RESHAPE | PERMUTE | EXPAND | ..., <src-subtree>].  The
       trailing NUM(d_i) / NUM(axis_i) cells are integer parameters,
       not children of structural interest. *)
    $UopReshape | $UopPermute | $UopExpand |
      $UopPad | $UopShrink | $UopFlip,                              1,
    $UopMaterialize,                                                1,
    $UopLoad,                                                       1,
    (* Phase E additions.  Heap layouts mirror src/thvm.h opcode
       comments.  RANGE, INDEX_E, IADD..IAND are pure structural;
       IWHERE has 3 children (cond, then, else).  BUFFER is variable-
       arity (NUM(scope), NUM(dtype), NUM(ndim), NUM(d0)..NUM(d_{ndim-1}));
       we report 0 here and special-case it in tTreeWalkWith below.
       OPT is (target, NUM(kind), NUM(factor)) -- 3 cells; AFTER and
       STORE are 2/3 cells respectively. *)
    $UopRange,                                                      3,
    $UopIndexE,                                                     2,
    $UopIAdd | $UopISub | $UopIMul | $UopIDiv |
      $UopIMod | $UopILt | $UopIAnd,                                2,
    $UopIWhere,                                                     3,
    $UopInvalid,                                                    1,
    $UopBuffer,                                                     0,
    $UopStore,                                                      3,
    $UopAfter,                                                      2,
    $UopOpt,                                                        3,
    _,                                                              0
]

(* tTreeWalkWith[reader, t, seen] is the structural decoder; the
   `reader` function (Integer loc -> raw Term) chooses which cell
   space to read from -- $heapReadFn for dyn (TTermTree default) or
   $bookReadFn for book-heap def bodies (TDefTree). *)
tTreeWalkWith[reader_, t_, seen_] := Block[{
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
        $TagDP0, "DP0"[ext, tTreeWalkWith[reader, reader[val], seen]],
        $TagDP1, "DP1"[ext, tTreeWalkWith[reader, reader[val], seen]],
        $TagLAM,
            If[ KeyExistsQ[seen, val], "Cycle"[val],
                seen2 = Append[seen, val -> True];
                "LAM"[tTreeWalkWith[reader, reader[val], seen2]]],
        $TagAPP,
            If[ KeyExistsQ[seen, val], "Cycle"[val],
                seen2 = Append[seen, val -> True];
                "APP"[tTreeWalkWith[reader, reader[val], seen2],
                      tTreeWalkWith[reader, reader[val + 1], seen2]]],
        $TagSUP,
            If[ KeyExistsQ[seen, val], "Cycle"[val],
                seen2 = Append[seen, val -> True];
                "SUP"[ext,
                    tTreeWalkWith[reader, reader[val], seen2],
                    tTreeWalkWith[reader, reader[val + 1], seen2]]],
        $TagDUP,
            If[ KeyExistsQ[seen, val], "Cycle"[val],
                seen2 = Append[seen, val -> True];
                "DUP"[ext, tTreeWalkWith[reader, reader[val], seen2]]],
        $TagALO,
            If[ KeyExistsQ[seen, val], "Cycle"[val],
                seen2 = Append[seen, val -> True];
                (* heap[val]   = wrapped term
                   heap[val+1] = state id (atomic, just record it) *)
                "ALO"[$termValFn[reader[val + 1]],
                      tTreeWalkWith[reader, reader[val], seen2]]],
        $TagOP2,
            If[ KeyExistsQ[seen, val], "Cycle"[val],
                seen2 = Append[seen, val -> True];
                "OP2"[Lookup[$op2Names, ext, "?" <> ToString[ext]],
                      tTreeWalkWith[reader, reader[val + 0], seen2],
                      tTreeWalkWith[reader, reader[val + 1], seen2]]],
        $TagMAT,
            If[ KeyExistsQ[seen, val], "Cycle"[val],
                seen2 = Append[seen, val -> True];
                "MAT"[ext,
                      tTreeWalkWith[reader, reader[val + 0], seen2],
                      tTreeWalkWith[reader, reader[val + 1], seen2]]],
        $TagCTR,
            If[ KeyExistsQ[seen, val], "Cycle"[val],
                seen2 = Append[seen, val -> True];
                Block[{nn = $termValFn[reader[val]]},
                    "CTR" @@ Prepend[
                        Table[
                            tTreeWalkWith[reader, reader[val + 1 + i], seen2],
                            {i, 0, nn - 1}
                        ],
                        ext
                    ]
                ]],
        $TagDSU,
            If[ KeyExistsQ[seen, val], "Cycle"[val],
                seen2 = Append[seen, val -> True];
                "DSU"[
                    tTreeWalkWith[reader, reader[val + 0], seen2],   (* label *)
                    tTreeWalkWith[reader, reader[val + 1], seen2],   (* a *)
                    tTreeWalkWith[reader, reader[val + 2], seen2]]], (* b *)
        $TagDDU,
            If[ KeyExistsQ[seen, val], "Cycle"[val],
                seen2 = Append[seen, val -> True];
                "DDU"[
                    tTreeWalkWith[reader, reader[val + 0], seen2],   (* label *)
                    tTreeWalkWith[reader, reader[val + 1], seen2],   (* value *)
                    tTreeWalkWith[reader, reader[val + 2], seen2]]], (* body *)
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
                    Module[{kid    = $termValFn[reader[val + 1]],
                            outTid = $termValFn[reader[val]],
                            inTids},
                        inTids = TKernelInputs[kid];
                        "UOP" @@ Join[
                            {"KERNEL", kid, "TEN"[outTid]},
                            "TEN" /@ inTids
                        ]
                    ],
                    If[ ext === $UopBuffer,
                        (* BUFFER is variable-arity: heap layout is
                           NUM(scope), NUM(dtype), NUM(ndim), then
                           NUM(d_0)..NUM(d_{ndim-1}), then optional
                           NUM(instance) at the end.  Read ndim from
                           cell 2 and surface (3 + ndim + 1) cells so
                           consumers see the full BUFFER signature in
                           a fixed shape: "UOP"["BUFFER", "NUM"[scope],
                           "NUM"[dtype], "NUM"[ndim], "NUM"[d_0]...,
                           "NUM"[instance]]. *)
                        Block[{ndim = $termValFn[reader[val + 2]], total},
                            total = 3 + ndim + 1; (* +1 for instance tail *)
                            "UOP" @@ Prepend[
                                Table[tTreeWalkWith[reader, reader[val + i], seen2],
                                      {i, 0, total - 1}],
                                "BUFFER"
                            ]
                        ],
                        "UOP" @@ Prepend[
                            Table[tTreeWalkWith[reader, reader[val + i], seen2], {i, 0, n - 1}],
                            Lookup[$uopNames, ext, "UOP?" <> ToString[ext]]
                        ]
                    ]
                ]],
        _, "Unknown"[tag]
    ]
]

(* Default tTreeWalk reads from the dynamic heap. *)
tTreeWalk[t_, seen_] := tTreeWalkWith[$heapReadFn, t, seen]

(* === high-level constructors (all return TTerm) === *)

heapWith[fields__] := With[{loc = THeapAlloc[Length[{fields}]]},
    (* numCoerce (defined in Switch.wl): a bare Integer field means
       "the NUM with that value".  Applying it here -- at the single
       funnel every heapTerm-based constructor (TApp / TSup / TLam /
       TDup / TCtr / TMat / TAnn / TBri / ...) goes through -- gives
       them all integer-lifting without per-agent boilerplate.  The
       low-level escape hatch THeapSet[loc, rawInt] still writes a raw
       word verbatim; only constructor args get coerced. *)
    ScanIndexed[THeapSet[loc + First[#2] - 1, numCoerce[#1]] &, {fields}];
    loc
]

heapTerm[tag_Integer, ext_Integer, fields__] :=
    packTerm[0, tag, ext, heapWith[fields]]

TEra[]                  := packTerm[0, $TagERA, 0, 0]
TVarFor[lamLoc_Integer] := packTerm[0, $TagVAR, 0, lamLoc]

TApp[fun_, arg_] := heapTerm[$TagAPP, 0, fun, arg]   (* heapWith coerces bare-Integer args *)

TSup[a_, b_]                          := TSup[TFreshLabel[], a, b]
TSup[label_Integer, a_, b_]           := heapTerm[$TagSUP, label, a, b]   (* heapWith coerces bare-Integer children *)
TSup[xs_List]                         := Fold[TSup, xs]

(* Dynamic-label SUP / DUP (HVM4 DSU / DDU).  The label is a Term
   that wnf reduces strict-left before dispatching the matching
   DSU-{NUM,ERA,SUP} / DDU-{NUM,ERA,SUP} interaction.  Use cases:
   pattern compilers that need a fresh label per match instance,
   rules-applying-rules where label allocation must come from the
   computation rather than be statically pre-assigned, label-as-term
   constructions for ATP-style enumeration. *)
TDsu[label_TTerm, a_TTerm, b_TTerm] := (
    ensureInit[];
    TTerm[$termNewDsuFn[ttermRaw[label], ttermRaw[a], ttermRaw[b]]]
)
TDdu[label_TTerm, val_TTerm, body_TTerm] := (
    ensureInit[];
    TTerm[$termNewDduFn[ttermRaw[label], ttermRaw[val], ttermRaw[body]]]
)

(* Term equality.  Both variants treat raw-Integer arguments as
   already-packed Term values (so callers can compose with
   ttermRaw without an extra TTerm wrapper). *)
TTermEq[a_TTerm, b_TTerm] :=
    (ensureInit[]; $termEqFn[ttermRaw[a], ttermRaw[b]] === 1)
TTermEq[a_Integer, b_Integer] :=
    (ensureInit[]; $termEqFn[a, b] === 1)

TTermSame[a_TTerm, b_TTerm] :=
    (ensureInit[]; $termEqStructFn[ttermRaw[a], ttermRaw[b]] === 1)
TTermSame[a_Integer, b_Integer] :=
    (ensureInit[]; $termEqStructFn[a, b] === 1)

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
    packTerm[0, $TagLAM, $lamSealExtFn[loc, 0], loc]
]

(* 1-arg form: `TLam[body]` -- a lambda whose binder is unused.
   Equivalent to `TLam[fresh, body]` with a unique-once symbol so
   the binder loc is never referenced from `body`.  Useful for
   MatChain default arms (`TLam[TEra[]]`), MAT-NUM handlers that
   return a constant, and any "thunk" pattern.  HoldAll preserves
   any heap-side construction inside `body` until evaluated. *)
TLam[body_] := With[{loc = THeapAlloc[1]},
    THeapSet[loc, body];
    packTerm[0, $TagLAM, $lamSealExtFn[loc, 0], loc]
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
    packTerm[0, $TagLAM, $lamSealExtFn[loc, 0], loc]
]

(* Sugar: TTerm[ctx, id][arg] == TApp[TTerm[ctx, id], arg] lets the
   user write `id[era]` instead of `TApp[id, era]`. *)
TTerm[c_Integer, id_Integer, _][y_TTerm]   := TApp[TTerm[c, id], y]
TTerm[c_Integer, id_Integer, _][y_Integer] := TApp[TTerm[c, id], y]

TDup[body_]                           := TDup[TFreshLabel[], body]
TDup[label_Integer, body_] := With[{loc = heapWith[body]},   (* heapWith coerces a bare-Integer body *)
    {packTerm[0, $TagDP0, label, loc],
     packTerm[0, $TagDP1, label, loc]}
]
TDup[body_, k_]                       := TDup[TFreshLabel[], body, k]
TDup[label_Integer, body_, k_]        := k @@ TDup[label, body]

(* `TDup[{dp0, dp1}]` -- inverse for the projection pair: given two
   TTerm projections that share a label and body loc, return the
   DUP wrapper TTerm at that loc.  Native operation: retag the
   projection's tag field (DP0=4 / DP1=5) to DUP (7), keeping ext
   (label) and val (loc) unchanged.  No heap access -- the body
   already lives at heap[loc] from the original TDup that produced
   the pair.  Mismatched projections (different DUPs) stay
   unevaluated rather than falling through to the body-constructor. *)
TDup[{dp0_TTerm, dp1_TTerm}] /; (
    TTermTag[dp0] === $TagDP0 && TTermTag[dp1] === $TagDP1
    && TTermExt[dp0] === TTermExt[dp1]
    && TTermVal[dp0] === TTermVal[dp1]) :=
    packTerm[0, $TagDUP, TTermExt[dp0], TTermVal[dp0]]
TDup[{_TTerm, _TTerm}] := $Failed

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
