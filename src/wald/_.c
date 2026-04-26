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
