fn int upat_match(UPat const *pat, Term t, Term *bindings) {
  if (pat == NULL) return 0;

  if (term_tag(t) != TAG_UOP) {
    // A non-UOp leaf has no sources: only an any-op pattern without op_alt
    // and with nsrc 0 or 0xFF matches it.
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
  // A pinned op trusts the pattern's nsrc, which may reach Terms uop_arity
  // does not count (AFTER, KERNEL); otherwise a fixed nsrc must be the arity.
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
