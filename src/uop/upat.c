// uop/upat.c - declarative pattern layer over UOp DAGs.
//
// Minimal core: structural pattern matching with positional
// bindings.  Each UPat node captures op / src-count / dtype /
// optional binding-slot; nested UPats describe child structure.
//
// Adapter: a UPatRule compiles into a UOpGraphRewriteFn (existing
// thvm harness) by wrapping the matcher + the user's rewrite
// function.  See docs/plans/autotune_beam_profile.md Level 45.
//
// Smoke-test scope: structural matching only.  No pdict
// fast-dispatch yet (every rule is checked against every visited
// node); that's an optimization for after the first port lands.

fn int upat_match(UPat const *pat, Term t, Term *bindings) {
  if (pat == NULL) return 0;

  // "any" op (0) accepts any UOp tag; otherwise op must match.
  if (term_tag(t) != TAG_UOP) {
    // Allow any op pattern to match a non-UOp leaf only when
    // there's no nested src constraint (leaf wildcards).
    if (pat->op == 0 && pat->nsrc == 0) {
      if (pat->bind >= 0 && pat->bind < UPAT_NUM_BINDINGS) {
        bindings[pat->bind] = t;
      }
      return 1;
    }
    return 0;
  }

  u8 t_op = term_ext(t);
  if (pat->op != 0 && pat->op != t_op) return 0;

  u8 arity = uop_arity(t_op);
  if (pat->nsrc != 0xFF && pat->nsrc != arity) return 0;

  u64 loc = term_val(t);

  if (pat->src != NULL) {
    u8 to_match = (pat->nsrc == 0xFF) ? arity : pat->nsrc;
    for (u8 i = 0; i < to_match; i++) {
      Term child = term_resolve(heap_read(loc + i));
      if (!upat_match(&pat->src[i], child, bindings)) return 0;
    }
  }

  if (pat->bind >= 0 && pat->bind < UPAT_NUM_BINDINGS) {
    bindings[pat->bind] = t;
  }
  return 1;
}
