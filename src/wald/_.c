// wald/_.c - Waldmeister .pr spec parser (stage 6.3).
//
// 6.3a: data model + init/free.  Subsequent stages add the lexer
// (6.3b), section drivers (6.3c), term parser (6.3d), equations
// section (6.3e), top-level driver (6.3f), and end-to-end tests
// against the group example (6.3g).

fn WaldSpec *wald_init(void) {
  WaldSpec *s = (WaldSpec *)calloc(1, sizeof(WaldSpec));
  if (s == NULL) return NULL;
  // Default to PROOF mode; explicit MODE keyword overrides.
  s->mode_proof = 1;
  // CTR label 0 is the "anonymous tuple" convention; start
  // signature labels at 1 so they don't collide.
  s->next_label = 1;
  return s;
}

fn void wald_free(WaldSpec *s) {
  if (s == NULL) return;
  free(s);
}

// === 6.3b: lexer ====================================================
//
// Tokenizes a NUL-terminated source buffer.  Skips whitespace and
// `%`-to-end-of-line comments.  Recognizes:
//
//   ident     [A-Za-z_][A-Za-z0-9_]*       (truncated to NAME_LEN-1)
//   :         WT_COLON
//   ->        WT_ARROW
//   =         WT_EQ
//   (  )      WT_LPAREN / WT_RPAREN
//   ,         WT_COMMA
//   >         WT_GT
//   <other>   WT_ERR (caller decides whether to bail)
//
// Section keywords (NAME, MODE, ...) come back as WT_IDENT; the
// section drivers (6.3c) compare lex->tok_text.

static u8 wald_is_alpha(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}
static u8 wald_is_digit(char c) {
  return c >= '0' && c <= '9';
}
static u8 wald_is_ident(char c) {
  return wald_is_alpha(c) || wald_is_digit(c);
}

fn void wald_lex_init(WaldLex *lex, const char *src) {
  if (lex == NULL) return;
  lex->src     = src ? src : "";
  lex->pos     = 0;
  lex->len     = (u32)strlen(lex->src);
  lex->tok_len = 0;
  lex->tok_text[0] = '\0';
  lex->have_peek    = 0;
  lex->peeked_kind  = WT_END;
  lex->peeked_len   = 0;
  lex->peeked_text[0] = '\0';
}

// Skip whitespace and `%`-to-end-of-line comments in place.
static void wald_lex_skip_ws(WaldLex *lex) {
  while (lex->pos < lex->len) {
    char c = lex->src[lex->pos];
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
      lex->pos++;
    } else if (c == '%') {
      while (lex->pos < lex->len && lex->src[lex->pos] != '\n') lex->pos++;
    } else {
      break;
    }
  }
}

fn WaldTokKind wald_lex_next(WaldLex *lex) {
  if (lex == NULL) return WT_ERR;
  // Consume the peeked token if there is one.
  if (lex->have_peek) {
    lex->have_peek = 0;
    if (lex->peeked_kind == WT_IDENT) {
      u32 n = lex->peeked_len;
      if (n >= WALD_NAME_LEN) n = WALD_NAME_LEN - 1;
      for (u32 i = 0; i < n; i++) lex->tok_text[i] = lex->peeked_text[i];
      lex->tok_text[n] = '\0';
      lex->tok_len = n;
    } else {
      lex->tok_text[0] = '\0';
      lex->tok_len     = 0;
    }
    return lex->peeked_kind;
  }
  wald_lex_skip_ws(lex);
  lex->tok_len = 0;
  lex->tok_text[0] = '\0';
  if (lex->pos >= lex->len) return WT_END;

  char c = lex->src[lex->pos];

  // Identifier.
  if (wald_is_alpha(c) || wald_is_digit(c)) {
    u32 start = lex->pos;
    while (lex->pos < lex->len && wald_is_ident(lex->src[lex->pos])) {
      lex->pos++;
    }
    u32 raw_len = lex->pos - start;
    u32 cap     = WALD_NAME_LEN - 1;
    u32 keep    = (raw_len < cap) ? raw_len : cap;
    for (u32 i = 0; i < keep; i++) lex->tok_text[i] = lex->src[start + i];
    lex->tok_text[keep] = '\0';
    lex->tok_len = keep;
    return WT_IDENT;
  }

  // Punctuation.
  switch (c) {
    case ':': lex->pos++; return WT_COLON;
    case '=': lex->pos++; return WT_EQ;
    case '(': lex->pos++; return WT_LPAREN;
    case ')': lex->pos++; return WT_RPAREN;
    case ',': lex->pos++; return WT_COMMA;
    case '>': lex->pos++; return WT_GT;
    case '-': {
      if (lex->pos + 1 < lex->len && lex->src[lex->pos + 1] == '>') {
        lex->pos += 2;
        return WT_ARROW;
      }
      lex->pos++;
      return WT_ERR;
    }
    default: {
      lex->pos++;
      return WT_ERR;
    }
  }
}

// === 6.3c1: section-detect infrastructure ===========================

fn WaldTokKind wald_lex_peek(WaldLex *lex) {
  if (lex == NULL) return WT_ERR;
  if (lex->have_peek) return lex->peeked_kind;
  // Read the next token into tok_text via the regular path, then
  // copy it into the peek slot and arm have_peek.
  WaldTokKind k = wald_lex_next(lex);
  lex->peeked_kind = k;
  lex->peeked_len  = lex->tok_len;
  for (u32 i = 0; i < lex->tok_len + 1; i++) {
    lex->peeked_text[i] = lex->tok_text[i];
  }
  lex->have_peek = 1;
  return k;
}

fn WaldSection wald_section_from_ident(const char *name) {
  if (name == NULL) return WSEC_NONE;
  if (strcmp(name, "NAME")       == 0) return WSEC_NAME;
  if (strcmp(name, "MODE")       == 0) return WSEC_MODE;
  if (strcmp(name, "SORTS")      == 0) return WSEC_SORTS;
  if (strcmp(name, "SIGNATURE")  == 0) return WSEC_SIGNATURE;
  if (strcmp(name, "VARIABLES")  == 0) return WSEC_VARIABLES;
  if (strcmp(name, "ORDERING")   == 0) return WSEC_ORDERING;
  if (strcmp(name, "EQUATIONS")  == 0) return WSEC_EQUATIONS;
  if (strcmp(name, "CONCLUSION") == 0) return WSEC_CONCLUSION;
  if (strcmp(name, "EXISTS")     == 0) return WSEC_EXISTS;
  return WSEC_NONE;
}

// 8.9d: parse the EXISTS section.  Each ident is a variable name
// to flag as existential; commas allowed between names.  Names
// are looked up against spec->vars[]; on hit, the FVR id is
// stored in spec->existential_var_ids[].  Section ends at the
// next section keyword (or EOF).  Permissive: unknown names are
// skipped; doesn't error out.
fn WaldSection wald_parse_exists(WaldSpec *spec, WaldLex *lex) {
  for (;;) {
    WaldTokKind k = wald_lex_peek(lex);
    if (k == WT_END) return WSEC_NONE;
    if (k == WT_COMMA) { wald_lex_next(lex); continue; }
    if (k != WT_IDENT) { wald_lex_next(lex); continue; }

    WaldSection sec = wald_section_from_ident(lex->peeked_text);
    if (sec != WSEC_NONE) {
      wald_lex_next(lex);
      return sec;
    }

    wald_lex_next(lex);   // consume var name
    if (spec == NULL) continue;
    // Look up by name in spec->vars[].
    for (u32 i = 0; i < spec->n_vars; i++) {
      const char *vn = spec->vars[i].name;
      u32 j = 0;
      while (j < lex->tok_len && vn[j] == lex->tok_text[j]) j++;
      if (j == lex->tok_len && vn[j] == '\0') {
        if (spec->n_existential < REWRITE_MAX_VAR) {
          spec->existential_var_ids[spec->n_existential++] =
            spec->vars[i].var_id;
        }
        break;
      }
    }
  }
}

// Eat tokens until the next section keyword (or EOF).  Returns the
// detected section enum; the lexer is positioned just past the
// section keyword.  Used by 6.3c2..c5 to recover from unstructured
// content within a section -- when each section parser hits
// something it doesn't understand, it falls back to skipping until
// it finds the next section.
fn WaldSection wald_skip_to_section(WaldLex *lex) {
  for (;;) {
    WaldTokKind t = wald_lex_next(lex);
    if (t == WT_END) return WSEC_NONE;
    if (t == WT_IDENT) {
      WaldSection sec = wald_section_from_ident(lex->tok_text);
      if (sec != WSEC_NONE) return sec;
    }
  }
}

// === 6.3c2: NAME / MODE / SORTS section parsers ====================

// Peek the next token; if it's a section keyword consume + return
// the section enum, else return WSEC_NONE without consuming.  Used
// at the top of each section parser to handle empty sections (the
// next keyword can sit right at the start).
static WaldSection wald_consume_if_section(WaldLex *lex) {
  if (wald_lex_peek(lex) != WT_IDENT) return WSEC_NONE;
  WaldSection sec = wald_section_from_ident(lex->peeked_text);
  if (sec != WSEC_NONE) wald_lex_next(lex);
  return sec;
}

// NAME: one ident as the spec's identifier.  Empty NAME (immediate
// section keyword) leaves spec->name unchanged.
fn WaldSection wald_parse_name(WaldSpec *spec, WaldLex *lex) {
  WaldSection sec = wald_consume_if_section(lex);
  if (sec != WSEC_NONE) return sec;
  if (wald_lex_peek(lex) == WT_IDENT) {
    wald_lex_next(lex);
    if (spec != NULL) {
      u32 n = lex->tok_len;
      if (n >= WALD_NAME_LEN) n = WALD_NAME_LEN - 1;
      for (u32 i = 0; i < n; i++) spec->name[i] = lex->tok_text[i];
      spec->name[n] = '\0';
    }
  }
  return wald_skip_to_section(lex);
}

// MODE: "PROOF" -> mode_proof = 1; "COMPLETION" -> 0; anything else
// or empty -> leave default (PROOF).
fn WaldSection wald_parse_mode(WaldSpec *spec, WaldLex *lex) {
  WaldSection sec = wald_consume_if_section(lex);
  if (sec != WSEC_NONE) return sec;
  if (wald_lex_peek(lex) == WT_IDENT) {
    wald_lex_next(lex);
    if (spec != NULL) {
      spec->mode_proof = (strcmp(lex->tok_text, "COMPLETION") == 0) ? 0 : 1;
    }
  }
  return wald_skip_to_section(lex);
}

// === 6.3c3: SIGNATURE section parser ===============================
//
// Each entry: `name : arg_sort1 arg_sort2 ... -> result_sort`.
// Arity = number of arg sorts before the arrow.  Each symbol gets
// a fresh CTR label via spec->next_label++.
//
// The result sort is ignored under the homogeneous-signature
// assumption.  Section ends at the next section keyword.
fn WaldSection wald_parse_signature(WaldSpec *spec, WaldLex *lex) {
  for (;;) {
    if (wald_lex_peek(lex) == WT_END) return WSEC_NONE;
    if (wald_lex_peek(lex) != WT_IDENT) {
      // Stray punctuation; skip.
      wald_lex_next(lex);
      continue;
    }
    // Is the next ident a section keyword?
    WaldSection sec = wald_section_from_ident(lex->peeked_text);
    if (sec != WSEC_NONE) {
      wald_lex_next(lex);
      return sec;
    }

    // Symbol name.
    wald_lex_next(lex);
    if (spec == NULL || spec->n_symbols >= WALD_MAX_SYMBOLS) {
      return wald_skip_to_section(lex);
    }
    WaldSym *sym = &spec->symbols[spec->n_symbols];
    u32 nlen = lex->tok_len;
    if (nlen >= WALD_NAME_LEN) nlen = WALD_NAME_LEN - 1;
    for (u32 i = 0; i < nlen; i++) sym->name[i] = lex->tok_text[i];
    sym->name[nlen] = '\0';
    sym->label = spec->next_label++;
    sym->arity = 0;

    // Expect ':' between name and arg sorts.
    if (wald_lex_next(lex) != WT_COLON) return wald_skip_to_section(lex);

    // 0+ ident arg sorts terminated by '->'.  Stage 8.4b: capture
    // each arg-sort id into sym->arg_sorts[arity].
    u8 saw_arrow = 0;
    for (;;) {
      WaldTokKind t = wald_lex_next(lex);
      if (t == WT_ARROW) { saw_arrow = 1; break; }
      if (t == WT_END)   return WSEC_NONE;
      if (t != WT_IDENT) return wald_skip_to_section(lex);
      if (sym->arity < WALD_MAX_ARITY) {
        u32 sid = wald_sort_id_or_register(spec, lex->tok_text, lex->tok_len);
        if (sid < WALD_MAX_SORTS) sym->arg_sorts[sym->arity] = sid;
      }
      sym->arity++;
    }
    (void)saw_arrow;

    // Result sort: one ident; capture its id.
    if (wald_lex_next(lex) != WT_IDENT) return wald_skip_to_section(lex);
    {
      u32 sid = wald_sort_id_or_register(spec, lex->tok_text, lex->tok_len);
      if (sid < WALD_MAX_SORTS) sym->result_sort = sid;
    }

    spec->n_symbols++;
  }
}

// === 6.3d: term parser =============================================
//
// Grammar:
//
//   term ::= ident                           -- var (FVR) or 0-arity sym
//          | ident "(" term ("," term)* ")"   -- application
//
// Lookup rule: an ident is a variable iff it appears in the spec's
// var table; otherwise it's a signature symbol.  Arity is enforced
// against the signature: `f(x)` for an arity-2 `f` returns 0.
//
// Recursive-descent.  Returns 0 (invalid Term) on any syntax error
// or unknown ident; the caller propagates the failure upward.
fn Term wald_parse_term(WaldSpec *spec, WaldLex *lex) {
  if (spec == NULL) return 0;
  if (wald_lex_next(lex) != WT_IDENT) return 0;

  // Snapshot the ident name -- subsequent lex_next calls clobber tok_text.
  char name[WALD_NAME_LEN];
  u32  nlen = lex->tok_len;
  if (nlen >= WALD_NAME_LEN) nlen = WALD_NAME_LEN - 1;
  for (u32 i = 0; i < nlen; i++) name[i] = lex->tok_text[i];
  name[nlen] = '\0';

  // Variable?
  for (u32 i = 0; i < spec->n_vars; i++) {
    if (strcmp(spec->vars[i].name, name) == 0) {
      return term_new_fvr(spec->vars[i].var_id);
    }
  }

  // Signature symbol?
  const WaldSym *sym = NULL;
  for (u32 i = 0; i < spec->n_symbols; i++) {
    if (strcmp(spec->symbols[i].name, name) == 0) {
      sym = &spec->symbols[i];
      break;
    }
  }
  if (sym == NULL) return 0;

  // Constant: no parenthesized args.
  if (wald_lex_peek(lex) != WT_LPAREN) {
    if (sym->arity != 0) return 0;
    return term_new_ctr(sym->label, NULL, 0);
  }
  wald_lex_next(lex);   // consume '('

  // Empty arg list `name()` is permitted iff arity == 0.
  if (wald_lex_peek(lex) == WT_RPAREN) {
    wald_lex_next(lex);
    if (sym->arity != 0) return 0;
    return term_new_ctr(sym->label, NULL, 0);
  }

  Term args[REWRITE_MAX_ARITY];
  u32  n_args = 0;
  for (;;) {
    Term arg = wald_parse_term(spec, lex);
    if (arg == 0) return 0;
    if (n_args >= REWRITE_MAX_ARITY) return 0;
    args[n_args++] = arg;
    WaldTokKind nk = wald_lex_next(lex);
    if (nk == WT_COMMA)  continue;
    if (nk == WT_RPAREN) break;
    return 0;
  }
  if (n_args != sym->arity) return 0;
  return term_new_ctr(sym->label, args, n_args);
}

// === 6.3f: top-level driver ========================================
//
// Lex the source, find the first section keyword, then dispatch to
// each section's parser in turn -- each parser returns the NEXT
// section's enum, so the loop just chains.  Sections can appear in
// any order; the .pr grammar has a fixed order, but accepting any
// order matches Waldmeister's permissive parser and lets test
// fixtures focus on shape rather than placement.
fn WaldErr wald_parse(const char *src, WaldSpec *spec) {
  if (src == NULL || spec == NULL) return WALD_ERR_NULL;

  WaldLex lex;
  wald_lex_init(&lex, src);

  WaldSection sec = wald_skip_to_section(&lex);
  if (sec == WSEC_NONE) return WALD_ERR_NO_SECTION;

  while (sec != WSEC_NONE) {
    switch (sec) {
      case WSEC_NAME:       sec = wald_parse_name      (spec, &lex); break;
      case WSEC_MODE:       sec = wald_parse_mode      (spec, &lex); break;
      case WSEC_SORTS:      sec = wald_parse_sorts     (spec, &lex); break;
      case WSEC_SIGNATURE:  sec = wald_parse_signature (spec, &lex); break;
      case WSEC_VARIABLES:  sec = wald_parse_variables (spec, &lex); break;
      case WSEC_ORDERING:   sec = wald_parse_ordering  (spec, &lex); break;
      case WSEC_EQUATIONS:  sec = wald_parse_equations (spec, &lex); break;
      case WSEC_CONCLUSION: sec = wald_parse_conclusion(spec, &lex); break;
      case WSEC_EXISTS:     sec = wald_parse_exists    (spec, &lex); break;
      default:              sec = wald_skip_to_section(&lex); break;
    }
  }
  return WALD_OK;
}

// === 6.4a: file loader ============================================
//
// Open `path`, slurp the entire file into a heap buffer, NUL-terminate
// it, hand to `wald_parse`, then free the buffer.  Returns
// WALD_ERR_NULL for bad args, WALD_ERR_FILE on any I/O / alloc failure,
// or whatever `wald_parse` returns.  The expected file size for
// Waldmeister `.pr` specs is at most a few KB, so a single slurp is
// fine.
fn WaldErr wald_parse_file(const char *path, WaldSpec *spec) {
  if (path == NULL || spec == NULL) return WALD_ERR_NULL;

  FILE *f = fopen(path, "rb");
  if (f == NULL) return WALD_ERR_FILE;

  if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return WALD_ERR_FILE; }
  long n = ftell(f);
  if (n < 0) { fclose(f); return WALD_ERR_FILE; }
  if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return WALD_ERR_FILE; }

  char *buf = (char *)malloc((size_t)n + 1);
  if (buf == NULL) { fclose(f); return WALD_ERR_FILE; }

  size_t got = fread(buf, 1, (size_t)n, f);
  fclose(f);
  if (got != (size_t)n) { free(buf); return WALD_ERR_FILE; }
  buf[n] = '\0';

  WaldErr e = wald_parse(buf, spec);
  free(buf);
  return e;
}

// === 6.3e: EQUATIONS / CONCLUSION parsers ==========================
//
// Both sections are sequences of `term = term` pairs.  The parser
// peeks for a section keyword first (handles end-of-section or
// empty section), then reads lhs / "=" / rhs and stores the pair.
// On parse error it falls through to wald_skip_to_section so
// downstream parsers still get the next section keyword.
//
// EQUATIONS appends to spec->eqn_lhs/rhs[].  CONCLUSION writes to
// spec->goal_lhs / goal_rhs; subsequent pairs are parsed (so the
// section terminates correctly) but discarded -- proof mode
// constrains the spec to a single conjecture.

// Helper: parse one `term = term` pair.  Returns 1 on success
// (out-params populated) or 0 on parse error.
static u8 wald_parse_equation_pair(WaldSpec *spec, WaldLex *lex,
                                   Term *lhs_out, Term *rhs_out) {
  Term lhs = wald_parse_term(spec, lex);
  if (lhs == 0) return 0;
  if (wald_lex_next(lex) != WT_EQ) return 0;
  Term rhs = wald_parse_term(spec, lex);
  if (rhs == 0) return 0;
  *lhs_out = lhs;
  *rhs_out = rhs;
  return 1;
}

fn WaldSection wald_parse_equations(WaldSpec *spec, WaldLex *lex) {
  for (;;) {
    WaldTokKind k = wald_lex_peek(lex);
    if (k == WT_END) return WSEC_NONE;
    if (k == WT_IDENT) {
      WaldSection sec = wald_section_from_ident(lex->peeked_text);
      if (sec != WSEC_NONE) { wald_lex_next(lex); return sec; }
    }
    Term lhs = 0, rhs = 0;
    if (!wald_parse_equation_pair(spec, lex, &lhs, &rhs)) {
      return wald_skip_to_section(lex);
    }
    if (spec != NULL && spec->n_eqns < WALD_MAX_EQNS) {
      spec->eqn_lhs[spec->n_eqns] = lhs;
      spec->eqn_rhs[spec->n_eqns] = rhs;
      spec->n_eqns++;
    }
  }
}

fn WaldSection wald_parse_conclusion(WaldSpec *spec, WaldLex *lex) {
  for (;;) {
    WaldTokKind k = wald_lex_peek(lex);
    if (k == WT_END) return WSEC_NONE;
    if (k == WT_IDENT) {
      WaldSection sec = wald_section_from_ident(lex->peeked_text);
      if (sec != WSEC_NONE) { wald_lex_next(lex); return sec; }
    }
    Term lhs = 0, rhs = 0;
    if (!wald_parse_equation_pair(spec, lex, &lhs, &rhs)) {
      return wald_skip_to_section(lex);
    }
    // Store only the first conclusion; ignore subsequent pairs.
    if (spec != NULL && spec->goal_lhs == 0) {
      spec->goal_lhs = lhs;
      spec->goal_rhs = rhs;
    }
  }
}

// === 6.3c5: ORDERING section parser ================================
//
// Grammar:
//   "KBO" weight_list precedence
// |   "LPO" precedence
//
// where weight_list = `name = number, name = number, ...` and
// precedence = `f1 > f2 > ... > fN` (left = greatest, right =
// smallest).  We discard the weight list (the saturation engine's
// KboConfig is supplied separately in stages 5-7) and record the
// precedence chain via `prec_rank` on each symbol: chain index 0
// (leftmost) gets rank N-1, chain index N-1 (rightmost) gets rank 0.
//
// Strategy: read everything as a token stream, tracking the most
// recently seen ident.  When a `>` arrives, that ident becomes the
// next precedence-chain entry.  Anything other than ident/`>`
// resets the "pending ident" tracker.  Stops at the next section
// keyword (or EOF).
fn WaldSection wald_parse_ordering(WaldSpec *spec, WaldLex *lex) {
  // KBO / LPO header (or empty section).
  if (wald_lex_peek(lex) != WT_IDENT) return wald_skip_to_section(lex);
  WaldSection sec = wald_section_from_ident(lex->peeked_text);
  if (sec != WSEC_NONE) { wald_lex_next(lex); return sec; }
  wald_lex_next(lex);   // consume KBO / LPO

  // 8.5d: capture ordering kind.  Compare leading letter: 'L' -> LPO,
  // anything else (typically 'K') -> KBO (default).
  if (spec != NULL && lex->tok_len > 0) {
    spec->ordering_kind = (lex->tok_text[0] == 'L') ? WALD_ORDER_LPO
                                                    : WALD_ORDER_KBO;
  }

  // Most recently seen ident (candidate next chain entry).
  u8   has_last = 0;
  char last[WALD_NAME_LEN];

  // Chain (left = greatest, right = smallest).
  u32  pchain_idx[WALD_MAX_SYMBOLS];
  u32  pchain_n = 0;

  WaldSection terminator = WSEC_NONE;
  for (;;) {
    WaldTokKind k = wald_lex_peek(lex);
    if (k == WT_END) break;
    if (k == WT_IDENT) {
      WaldSection ssec = wald_section_from_ident(lex->peeked_text);
      if (ssec != WSEC_NONE) {
        wald_lex_next(lex);
        terminator = ssec;
        break;
      }
      wald_lex_next(lex);
      u32 nlen = lex->tok_len;
      if (nlen >= WALD_NAME_LEN) nlen = WALD_NAME_LEN - 1;
      for (u32 i = 0; i < nlen; i++) last[i] = lex->tok_text[i];
      last[nlen] = '\0';
      has_last = 1;
      continue;
    }
    if (k == WT_GT) {
      wald_lex_next(lex);
      if (has_last && spec != NULL) {
        for (u32 i = 0; i < spec->n_symbols; i++) {
          if (strcmp(spec->symbols[i].name, last) == 0) {
            if (pchain_n < WALD_MAX_SYMBOLS) pchain_idx[pchain_n++] = i;
            break;
          }
        }
      }
      has_last = 0;
      continue;
    }
    // Any other token (=, COMMA, etc.) -- consume and reset pending.
    wald_lex_next(lex);
    has_last = 0;
  }

  // After the loop: if we have a pending ident AND we've already
  // started a chain, the pending one is the chain's terminal.
  if (has_last && pchain_n > 0 && spec != NULL) {
    for (u32 i = 0; i < spec->n_symbols; i++) {
      if (strcmp(spec->symbols[i].name, last) == 0) {
        if (pchain_n < WALD_MAX_SYMBOLS) pchain_idx[pchain_n++] = i;
        break;
      }
    }
  }

  // Rank assignment: chain[0] is greatest, chain[n-1] is smallest.
  if (spec != NULL) {
    for (u32 i = 0; i < pchain_n; i++) {
      spec->symbols[pchain_idx[i]].prec_rank = pchain_n - 1 - i;
    }
  }

  return terminator;
}

// === 6.3c4: VARIABLES section parser ===============================
//
// Section grammar:  { ident { "," ident } ":" sort_ident }
//
// Each ident gets registered into spec->vars[] with a sequential
// FVR id (= spec->n_vars at the time of registration).  Sort names
// are consumed and discarded (homogeneous-signature assumption).
//
// Section ends at the next section keyword.  Truncated input
// (EOF mid-list) returns WSEC_NONE; whatever vars were registered
// up to that point stay.
fn WaldSection wald_parse_variables(WaldSpec *spec, WaldLex *lex) {
  // 8.4b: track the start of the current comma-batch so we can
  // propagate the post-colon sort to every variable in the batch
  // (e.g. `x, y, z : nat` -> all three get sort `nat`).
  u32 batch_start = (spec != NULL) ? spec->n_vars : 0;
  for (;;) {
    WaldTokKind k = wald_lex_peek(lex);
    if (k == WT_END) return WSEC_NONE;
    if (k != WT_IDENT) {
      // Stray punctuation; skip.
      wald_lex_next(lex);
      continue;
    }
    // Section boundary?
    WaldSection sec = wald_section_from_ident(lex->peeked_text);
    if (sec != WSEC_NONE) {
      wald_lex_next(lex);
      return sec;
    }

    // Variable name; consume + register.
    wald_lex_next(lex);
    if (spec != NULL && spec->n_vars < WALD_MAX_VARS) {
      WaldVar *v = &spec->vars[spec->n_vars];
      u32 nlen = lex->tok_len;
      if (nlen >= WALD_NAME_LEN) nlen = WALD_NAME_LEN - 1;
      for (u32 i = 0; i < nlen; i++) v->name[i] = lex->tok_text[i];
      v->name[nlen] = '\0';
      v->var_id = spec->n_vars;
      spec->n_vars++;
    }

    // What follows?
    //   COMMA  -> consume + read another var name on the next iter
    //   COLON  -> consume + skip sort ident (which itself might be
    //             a section keyword for a degenerate empty-sort case)
    //   else   -> let the outer loop re-peek (handles section keyword)
    k = wald_lex_peek(lex);
    if (k == WT_COMMA) {
      wald_lex_next(lex);
      continue;
    }
    if (k == WT_COLON) {
      wald_lex_next(lex);
      if (wald_lex_peek(lex) == WT_IDENT) {
        WaldSection inner = wald_section_from_ident(lex->peeked_text);
        if (inner != WSEC_NONE) {
          wald_lex_next(lex);
          return inner;
        }
        wald_lex_next(lex);   // consume sort name
        // 8.4b: assign the sort to all variables in the current
        // batch (`x, y, z : nat` -> all three get sort nat).
        u32 sid = wald_sort_id_or_register(spec, lex->tok_text, lex->tok_len);
        if (sid < WALD_MAX_SORTS && spec != NULL) {
          for (u32 vi = batch_start; vi < spec->n_vars; vi++) {
            spec->vars[vi].sort = sid;
          }
          batch_start = spec->n_vars;
        }
      }
      continue;
    }
    // Anything else (END, ARROW, etc.): outer loop handles it.
  }
}

// 8.4c: top-down sort inference.  Returns the sort id of t if
// well-sorted, or WALD_MAX_SORTS on mismatch / unknown identifier.
//
// FVR: looked up by var_id in spec->vars[]; returns the var's
//   sort (0 in homogeneous mode), or WALD_MAX_SORTS if the var_id
//   isn't registered.
// CTR: looked up by label in spec->symbols[]; arity must match;
//   each child's sort (recursively inferred) must equal the
//   symbol's arg_sorts[i]; returns the symbol's result_sort.
// Other tags: returns WALD_MAX_SORTS (not part of the FOL term
//   layer; the saturation engine doesn't construct these).
//
// Homogeneous-mode shortcut: spec == NULL or n_sorts == 0 returns
// 0 unconditionally so single-sort fixtures (which never populate
// the sort table beyond an auto-registered "ANY") continue to
// pass without sort policing.
fn u32 wald_term_sort(const WaldSpec *spec, Term t) {
  if (spec == NULL || spec->n_sorts == 0) return 0;

  switch (term_tag(t)) {
    case TAG_FVR: {
      u32 id = term_ext(t);
      for (u32 i = 0; i < spec->n_vars; i++) {
        if (spec->vars[i].var_id == id) return spec->vars[i].sort;
      }
      return WALD_MAX_SORTS;
    }
    case TAG_CTR: {
      u32 lab = term_ext(t);
      u32 n   = term_ctr_n(t);
      const WaldSym *sym = NULL;
      for (u32 i = 0; i < spec->n_symbols; i++) {
        if (spec->symbols[i].label == lab) {
          sym = &spec->symbols[i];
          break;
        }
      }
      if (sym == NULL)         return WALD_MAX_SORTS;
      if (sym->arity != n)     return WALD_MAX_SORTS;
      for (u32 i = 0; i < n; i++) {
        u32 child_sort = wald_term_sort(spec, term_ctr_at(t, i));
        if (child_sort != sym->arg_sorts[i]) return WALD_MAX_SORTS;
      }
      return sym->result_sort;
    }
    default:
      return WALD_MAX_SORTS;
  }
}

fn u8 wald_sort_check(const WaldSpec *spec, Term t) {
  return wald_term_sort(spec, t) != WALD_MAX_SORTS;
}

// 8.4b: look up sort by name, or register if new.  Returns
// WALD_MAX_SORTS (sentinel) on overflow.
fn u32 wald_sort_id_or_register(WaldSpec *spec,
                                const char *name, u32 len) {
  if (spec == NULL) return WALD_MAX_SORTS;
  if (len >= WALD_NAME_LEN) len = WALD_NAME_LEN - 1;
  for (u32 i = 0; i < spec->n_sorts; i++) {
    u32 j = 0;
    while (j < len && spec->sorts[i][j] == name[j]) j++;
    if (j == len && spec->sorts[i][len] == '\0') return i;
  }
  if (spec->n_sorts >= WALD_MAX_SORTS) return WALD_MAX_SORTS;
  u32 id = spec->n_sorts++;
  for (u32 i = 0; i < len; i++) spec->sorts[id][i] = name[i];
  spec->sorts[id][len] = '\0';
  return id;
}

// SORTS: list of sort names separated by whitespace.  Stage 8.4b:
// each sort name is registered in spec->sorts[] via
// wald_sort_id_or_register; SIGNATURE / VARIABLES reuse the same
// helper so sort ids are stable across sections.  Specs without
// a SORTS section are still parsed -- sorts get auto-registered
// lazily during SIGNATURE / VARIABLES parsing (homogeneous-mode
// fixtures continue to work).
fn WaldSection wald_parse_sorts(WaldSpec *spec, WaldLex *lex) {
  for (;;) {
    WaldTokKind k = wald_lex_peek(lex);
    if (k == WT_END) return WSEC_NONE;
    if (k == WT_IDENT) {
      WaldSection sec = wald_section_from_ident(lex->peeked_text);
      if (sec != WSEC_NONE) {
        wald_lex_next(lex);
        return sec;
      }
      // Sort name; consume + register.
      wald_lex_next(lex);
      (void)wald_sort_id_or_register(spec, lex->tok_text, lex->tok_len);
      continue;
    }
    // Anything else: skip.
    wald_lex_next(lex);
  }
}
