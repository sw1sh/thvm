#!/usr/bin/env bash
# docs/aot_emitted_examples/_gen.sh
#
# Regenerate the emit snapshots in this directory.  Each snapshot
# is produced by a fresh wolframscript invocation -- chaining
# TInit/TDef/TAOTEmit in one session segfaults the kernel
# (reproducible going back to legacy AOT; tracked there too,
# unrelated to the new emit code).
#
# Run from repo root:
#     bash docs/aot_emitted_examples/_gen.sh
#
# Refresh after any non-trivial change to src/aot/emit.c.

set -euo pipefail
cd "$(git rev-parse --show-toplevel)"

OUT_DIR="docs/aot_emitted_examples"

emit_one() {
    local name="$1" wl_def="$2" header="$3"
    local out="$OUT_DIR/$name.c"

    echo "  emitting $out"

    local body
    body=$(wolframscript -code "
        PacletDirectoryLoad[\"wl/THVMLink\"]; Get[\"THVMLink\`\"];
        TInit[];
        TDef[\"$name\", $wl_def];
        WriteString[\"stdout\", TAOTEmit[\"$name\"]];
    ")

    local commented_header
    commented_header=$(printf '%s\n' "$header" | awk '{print "// " $0}')

    cat > "$out" <<EOF
// docs/aot_emitted_examples/$name.c
//
$commented_header
//
// Regenerate via: bash docs/aot_emitted_examples/_gen.sh

$body
EOF
}

# 1. identity_lam -- smallest possible AOT'd def
emit_one "identity_lam" \
    "TLam[x, x]" \
    "Identity (id : a -> a).  Smallest possible AOT'd def.

Shows:
  - the FN_<name> constant
  - par_<name>_entry as the only function (no conts)
  - aot_program_<name>_dispatch + aot_program_<name>_register
  - body uses the iter-1 stub fallback because identity isn't
    an App-of-Mat shape -- the emit returns NUM(0) regardless
    of input.  That's fine for the outer-scaffolding demo;
    real defs hit one of the recognised body shapes."

# 2. tree_sum -- TOp2 sibling SPLIT on distinct destructured children
#    Avoids the auto-dup that count(p,p) would trigger (using p twice
#    in WL inserts an IC DUP/DP0/DP1 pair the emitter doesn't yet
#    lower).  sum(node{l, r}) destructures into distinct l and r, so
#    each sub-call uses its own binding -- no DUP, clean SPLIT emit.
emit_one "tree_sum" \
    "TLam[t, TMatChain[
       <|0 -> TLam[v, v],
         1 -> TLam[l, TLam[r, TOp2[\"+\",
                              TApp[TRef[\"tree_sum\"], l],
                              TApp[TRef[\"tree_sum\"], r]]]]
       |>,
       TLam[ig, TEra[]]
     ][t]]" \
    "tree_sum: collapses a binary tree of NUM leaves into the sum.
Bend2-style:
  sum(leaf{v})    = v
  sum(node{l, r}) = sum(l) + sum(r)

Shows:
  - dead-arm pruning (CTR-only chain)
  - leaf arm: TLam[v, v] -> args[0] inlined directly
  - node arm: TLam[l, TLam[r, ...]] -> binds l = term_ctr_at(dv, 0),
    r = term_ctr_at(dv, 1) (multi-LAM CTR destructure)
  - sibling-pair TOp2 -> aot_alloc_cont + aot_make_split
  - OP2 fold cont: lv + rv constant-folded from OP_ADD
  - dispatch table: FN_tree_sum + CONT_tree_sum_0

Pairs with tree_build below; together they're the canonical
fork-friendly workload Bend2 uses to demo MT scaling."

# 3. tree_build -- TCtr2 sibling SPLIT + node{} cont.  Requires
#    DP* support (auto-dup of `dd` and `x` since each is used
#    twice in the node{} arm); landed alongside this example.
emit_one "tree_build" \
    "TLam[d, TLam[x, TMatChain[
       <|0 -> TCtr[1, x]|>,
       TLam[dd, TCtr[2, TApp[TApp[TRef[\"tree_build\"], dd], x],
                         TApp[TApp[TRef[\"tree_build\"], dd], x]]]
     ][d]]]" \
    "Bend2-style tree builder.  Returns a binary tree of depth
args[0] = d, with leaves carrying args[1] = x.  Source matches
TinyHVM/resources/gists/par_tree_sum_bend2_compiled.c's
\`build\` def (same fork shape, different surface syntax).

Shows:
  - default-arm TLam destructure (binds \`dd\` to dv -- the
    matched value, since this is the chain's catch-all)
  - sibling-pair TCtr2 -> aot_alloc_cont + aot_make_split,
    cont wraps in aot_make_ctr2(2u, t->args[0], t->args[1])
    (the \`node{l, r}\` reassembly)
  - 2-arity self-call via aot_make_task(FN_tree_build, ...)
  - DP* construction with shared runtime cells: \`dd\` and \`x\`
    are each used twice in the node{} arm, so auto-dup wraps
    them into TAG_DP0/TAG_DP1.  The emit memoizes by source
    dup_loc, so DP0 and DP1 of the SAME source dup share ONE
    runtime cell (named dup_K_loc) -- Bend2 sharing semantics,
    matches the hand-coded reference programs
  - Dispatch forces dv = wnf(t->args[0]) so DP-wrapped CTRs
    unwrap before the tag check fires"

echo
echo "done -- regenerate any time with: bash docs/aot_emitted_examples/_gen.sh"
