// uop/upat.c - declarative pattern layer over UOp DAGs: structural
// matching with positional bindings.  Each UPat node captures op /
// src-count / dtype / optional binding slot; nested UPats describe
// child structure.

fn int upat_match(UPat const *pat, Term t, Term *bindings) {
  if (pat == NULL) return 0;

  // "any" op (0) accepts any UOp tag; otherwise op must match.
  if (term_tag(t) != TAG_UOP) {
    // A non-UOp leaf has zero children, so leaf-wildcards require
    // op=0 plus nsrc in {0, 0xFF/any}.  A specific op (>0), an
    // op_alt set, or a specific arity (>=1) all reject leaves.
    if (pat->op == 0 && pat->op_alt == NULL
        && (pat->nsrc == 0 || pat->nsrc == 0xFF)) {
      if (pat->bind >= 0 && pat->bind < UPAT_NUM_BINDINGS) {
        bindings[pat->bind] = t;
      }
      return 1;
    }
    return 0;
  }

  u8 t_op = term_ext(t);
  if (pat->op_alt != NULL) {
    int found = 0;
    for (u8 const *p = pat->op_alt; *p != 0; p++) {
      if (*p == t_op) { found = 1; break; }
    }
    if (!found) return 0;
  } else if (pat->op != 0 && pat->op != t_op) {
    return 0;
  }

  u8 arity = uop_arity(t_op);
  // When the pattern pinned the op (op != 0 or op_alt != NULL), trust
  // the user's nsrc -- they know the heap layout and may want to match
  // through opcodes that uop_arity() doesn't yet enumerate
  // (UOP_INDEX_E / UOP_STORE / UOP_OPT / UOP_I* / UOP_IWHERE / etc).
  // Otherwise (op == 0 AND op_alt == NULL), a fixed nsrc must match
  // arity -- without that we'd accept e.g. UOP_NEG (arity 1) for a
  // pattern that asked for "any op with 2 children".
  int op_pinned = (pat->op != 0) || (pat->op_alt != NULL);
  if (!op_pinned && pat->nsrc != 0xFF && pat->nsrc != arity) return 0;

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
