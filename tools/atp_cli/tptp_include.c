// ===================================================================
// TPTP include() directives
// ===================================================================
//
// One implementation for FindProof's TPTP front-end (tp_preprocess) and
// CheckProof's problem loading (pc_load_problem), so the prover and the
// checker read the same problem:
//
//   include('F').               every formula of F
//   include('F', *).            every formula of F
//   include('F', [n1, ..., nk]). only the formulas named n1 .. nk
//
// F resolves against dirname(problem), $TPTP, then the CWD, and against
// no other root.  Includes inside an included file resolve the same way
// (TPTP include paths are relative to the library root, not to the
// including file), up to TP_INC_DEPTH levels, and a selection filters
// everything its file expands to.  An unresolvable file, a listed name
// the expansion does not define, a namespace argument (TPTP's third
// include field, whose renaming this does not model), a malformed
// directive and a nesting cycle all fail the expansion: FindProof then
// stops with an input error, and CheckProof counts the problem as only
// partly read.

#define TP_INC_DEPTH 8

static char *tp_read_file(const char *path, size_t *len_out) {
  FILE *fp = fopen(path, "rb");
  if (fp == NULL) return NULL;
  fseek(fp, 0, SEEK_END);
  long n = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  char *buf = (char *)malloc((size_t)n + 1u);
  if (buf == NULL) { fclose(fp); return NULL; }
  size_t rd = fread(buf, 1, (size_t)n, fp);
  fclose(fp);
  buf[rd] = '\0';
  if (len_out != NULL) *len_out = rd;
  return buf;
}

// Resolve an include('NAME') against dirname(problem), $TPTP, CWD.
static char *tp_resolve_include(const char *problem_path, const char *inc) {
  static char cand[4096];
  const char *slash = strrchr(problem_path, '/');
  if (slash != NULL) {
    snprintf(cand, sizeof cand, "%.*s/%s",
             (int)(slash - problem_path), problem_path, inc);
    char *b = tp_read_file(cand, NULL);
    if (b != NULL) return b;
  }
  const char *tptp = getenv("TPTP");
  if (tptp != NULL) {
    snprintf(cand, sizeof cand, "%s/%s", tptp, inc);
    char *b = tp_read_file(cand, NULL);
    if (b != NULL) return b;
  }
  return tp_read_file(inc, NULL);
}

// Blank TSTP comments (% to end of line, /* ... */) outside single
// quotes, in place; offsets and length are kept.
static void tp_blank_comments(char *s) {
  int inq = 0;
  for (char *p = s; *p != '\0'; p++) {
    if (inq) {
      if (*p == '\'') {
        inq = 0;
      }
      continue;
    }
    if (*p == '\'') {
      inq = 1;
      continue;
    }
    if (*p == '%') {
      while (*p != '\0' && *p != '\n') {
        *p++ = ' ';
      }
    } else if (p[0] == '/' && p[1] == '*') {
      *p++ = ' ';
      *p++ = ' ';
      while (*p != '\0' && !(p[0] == '*' && p[1] == '/')) {
        *p++ = ' ';
      }
      if (*p != '\0') {
        p[0] = ' ';
        p[1] = ' ';
        p++;
      }
    }
    if (*p == '\0') {
      break;
    }
  }
}

static const char *tp_skip_blank(const char *s) {
  for (;;) {
    while (isspace((unsigned char)*s)) {
      s++;
    }
    if (*s == '%') {
      while (*s != '\0' && *s != '\n') {
        s++;
      }
    } else if (s[0] == '/' && s[1] == '*') {
      const char *e = strstr(s + 2, "*/");
      s = (e != NULL) ? e + 2 : s + strlen(s);
    } else {
      return s;
    }
  }
}

// The closing quote of the single-quoted token opening at q, or NULL.
static const char *tp_quote_end(const char *q) {
  for (q++; *q != '\0'; q++) {
    if (q[0] == '\\' && q[1] != '\0') {
      q++;
    } else if (*q == '\'') {
      return q;
    }
  }
  return NULL;
}

// The '.' ending the record that starts at s: the first one outside
// quotes, brackets and comments, or NULL when the text ends first.
static const char *tp_record_end(const char *s) {
  int depth = 0;
  for (const char *p = s; *p != '\0'; p++) {
    switch (*p) {
      case '\'':
        p = tp_quote_end(p);
        if (p == NULL) {
          return NULL;
        }
        break;
      case '%':
        p = strchr(p, '\n');
        if (p == NULL) {
          return NULL;
        }
        break;
      case '/':
        if (p[1] == '*') {
          p = strstr(p + 2, "*/");
          if (p == NULL) {
            return NULL;
          }
          p++;
        }
        break;
      case '(':
      case '[':
        depth++;
        break;
      case ')':
      case ']':
        depth--;
        break;
      case '.':
        if (depth == 0) {
          return p;
        }
        break;
      default:
        break;
    }
  }
  return NULL;
}

static int tp_is_include(const char *s) {
  if (strncmp(s, "include", 7) != 0) {
    return 0;
  }
  s += 7;
  while (isspace((unsigned char)*s)) {
    s++;
  }
  return *s == '(';
}

// Copy a TPTP name into out: blanks trimmed, one pair of outer single
// quotes dropped ('n1' and n1 name one formula, as in the checker's
// tables).  0 when it is empty or does not fit.
static int tp_name_copy(const char *s, size_t n, char *out, size_t cap) {
  while (n > 0 && isspace((unsigned char)s[0])) {
    s++;
    n--;
  }
  while (n > 0 && isspace((unsigned char)s[n - 1])) {
    n--;
  }
  if (n >= 2 && s[0] == '\'' && s[n - 1] == '\'') {
    s++;
    n -= 2;
  }
  if (n == 0 || n >= cap) {
    return 0;
  }
  memcpy(out, s, n);
  out[n] = '\0';
  return 1;
}

// The name of the annotated formula at s (comment-blanked text): the
// first argument of `word(`.  0 when s has no such shape.
static int tp_record_name(const char *s, char *out, size_t cap) {
  const char *p = s;
  while (isalpha((unsigned char)*p)) {
    p++;
  }
  if (p == s) {
    return 0;
  }
  while (isspace((unsigned char)*p)) {
    p++;
  }
  if (*p != '(') {
    return 0;
  }
  const char *b = ++p;
  while (*p != '\0' && *p != ',') {
    if (*p == '\'') {
      p = tp_quote_end(p);
      if (p == NULL) {
        return 0;
      }
    }
    p++;
  }
  if (*p != ',') {
    return 0;
  }
  return tp_name_copy(b, (size_t)(p - b), out, cap);
}

typedef struct {
  char   file[1024];
  char **sel;     // the listed names, sorted and unique; NULL = whole file
  int    nsel;
} TpInclude;

static void tp_include_free(TpInclude *inc) {
  for (int i = 0; i < inc->nsel; i++) {
    free(inc->sel[i]);
  }
  free(inc->sel);
  inc->sel = NULL;
  inc->nsel = 0;
}

static int tp_str_cmp(const void *a, const void *b) {
  return strcmp(*(char *const *)a, *(char *const *)b);
}

// Parse `n1, ..., nk]` into inc->sel; returns the position after the
// ']', or NULL on an empty or malformed list.
static const char *tp_include_names(const char *s, TpInclude *inc) {
  int cap = 0;
  for (;;) {
    s = tp_skip_blank(s);
    const char *b = s;
    if (*s == '\'') {
      s = tp_quote_end(s);
      if (s == NULL) {
        return NULL;
      }
      s++;
    } else {
      if (*s == '+' || *s == '-') {
        s++;
      }
      while (isalnum((unsigned char)*s) || *s == '_') {
        s++;
      }
    }
    char nm[1024];
    if (!tp_name_copy(b, (size_t)(s - b), nm, sizeof nm)) {
      return NULL;
    }
    if (inc->nsel == cap) {
      cap = (cap > 0) ? 2 * cap : 16;
      char **ns = (char **)realloc(inc->sel, (size_t)cap * sizeof(char *));
      if (ns == NULL) {
        return NULL;
      }
      inc->sel = ns;
    }
    inc->sel[inc->nsel++] = strdup(nm);
    s = tp_skip_blank(s);
    if (*s == ']') {
      break;
    }
    if (*s != ',') {
      return NULL;
    }
    s++;
  }
  qsort(inc->sel, (size_t)inc->nsel, sizeof(char *), tp_str_cmp);
  int u = 0;
  for (int i = 0; i < inc->nsel; i++) {
    if (u > 0 && strcmp(inc->sel[u - 1], inc->sel[i]) == 0) {
      free(inc->sel[i]);
    } else {
      inc->sel[u++] = inc->sel[i];
    }
  }
  inc->nsel = u;
  return s + 1;
}

// Parse the directive at s -- include('F'), include('F', *) or
// include('F', [n1, ..., nk]) -- up to its terminating '.' or the end
// of the text; returns that position.  NULL when the directive is
// malformed or carries a namespace argument.
static const char *tp_include_parse(const char *s, TpInclude *inc) {
  memset(inc, 0, sizeof *inc);
  s = tp_skip_blank(s);
  if (!tp_is_include(s)) {
    return NULL;
  }
  s = tp_skip_blank(strchr(s, '(') + 1);
  const char *q = (*s == '\'') ? tp_quote_end(s) : NULL;
  size_t n = (q != NULL) ? (size_t)(q - s - 1) : 0;
  if (n == 0 || n >= sizeof inc->file) {
    return NULL;
  }
  memcpy(inc->file, s + 1, n);
  inc->file[n] = '\0';
  s = tp_skip_blank(q + 1);
  if (*s == ',') {
    s = tp_skip_blank(s + 1);
    if (*s == '*') {
      s = tp_skip_blank(s + 1);
    } else if (*s == '[') {
      s = tp_include_names(s + 1, inc);
      if (s == NULL) {
        tp_include_free(inc);
        return NULL;
      }
      s = tp_skip_blank(s);
    } else {
      return NULL;
    }
  }
  if (*s != ')') {
    tp_include_free(inc);
    return NULL;
  }
  s = tp_skip_blank(s + 1);
  if (*s != '.' && *s != '\0') {
    tp_include_free(inc);
    return NULL;
  }
  return s;
}

typedef struct {
  char  *s;
  size_t n, cap;
} TpBuf;

static void tp_buf_put(TpBuf *b, const char *s, size_t n) {
  if (b->n + n + 1 > b->cap) {
    size_t cap = (b->cap > 0) ? b->cap : 4096;
    while (cap < b->n + n + 1) {
      cap *= 2;
    }
    char *ns = (char *)realloc(b->s, cap);
    if (ns == NULL) {
      fprintf(stderr, "include(): out of memory\n");
      exit(2);
    }
    b->s = ns;
    b->cap = cap;
  }
  memcpy(b->s + b->n, s, n);
  b->n += n;
  b->s[b->n] = '\0';
}

// The records of `all` the selection names, comment-blanked, one per
// line; NULL when a listed name matches no record.
static char *tp_include_select(const char *all, const TpInclude *inc,
                               char *why, size_t wcap) {
  char *t = strdup(all);
  u8 *hit = (u8 *)calloc((size_t)inc->nsel, 1);
  if (t == NULL || hit == NULL) {
    fprintf(stderr, "include(): out of memory\n");
    exit(2);
  }
  tp_blank_comments(t);
  TpBuf out = {0};
  tp_buf_put(&out, "", 0);
  const char *p = t;
  for (;;) {
    const char *r = tp_skip_blank(p);
    const char *dot = (*r != '\0') ? tp_record_end(r) : NULL;
    if (dot == NULL) {
      break;
    }
    char nm[1024];
    if (tp_record_name(r, nm, sizeof nm)) {
      const char *key = nm;
      char **k = (char **)bsearch(&key, inc->sel, (size_t)inc->nsel,
                                  sizeof(char *), tp_str_cmp);
      if (k != NULL) {
        hit[k - inc->sel] = 1;
        tp_buf_put(&out, r, (size_t)(dot + 1 - r));
        tp_buf_put(&out, "\n", 1);
      }
    }
    p = dot + 1;
  }
  for (int i = 0; i < inc->nsel; i++) {
    if (!hit[i]) {
      snprintf(why, wcap, "include('%s') selects %s, which the file does not define",
               inc->file, inc->sel[i]);
      free(out.s);
      out.s = NULL;
      break;
    }
  }
  free(hit);
  free(t);
  return out.s;
}

static char *tp_include_expand(const char *problem_path, const char *rec,
                               int depth, const char **end,
                               char *why, size_t wcap);

// Splice every include directive of `text` (owned: returned as is when
// it has none, freed otherwise) with its expansion one level deeper.
static char *tp_include_splice(const char *problem_path, char *text,
                               int depth, char *why, size_t wcap) {
  TpBuf out = {0};
  const char *done = text;
  const char *p = text;
  for (;;) {
    const char *r = tp_skip_blank(p);
    if (*r == '\0') {
      break;
    }
    const char *dot = tp_record_end(r);
    if (tp_is_include(r)) {
      const char *end = NULL;
      char *sub = tp_include_expand(problem_path, r, depth + 1, &end, why, wcap);
      if (sub == NULL) {
        free(out.s);
        free(text);
        return NULL;
      }
      tp_buf_put(&out, done, (size_t)(r - done));
      tp_buf_put(&out, "\n", 1);
      tp_buf_put(&out, sub, strlen(sub));
      tp_buf_put(&out, "\n", 1);
      free(sub);
      done = (*end == '.') ? end + 1 : end;
    }
    if (dot == NULL) {
      break;
    }
    p = dot + 1;
  }
  if (out.s == NULL) {
    return text;
  }
  tp_buf_put(&out, done, strlen(done));
  free(text);
  return out.s;
}

// The text the include directive at `rec` contributes, malloc'd: the
// formulas of its file, nested includes spliced in, the selection
// applied.  depth is the directive's nesting level (1 in the problem
// file); *end (when asked) receives the position of its terminating '.'
// (or the end of the text).  NULL on failure, with the reason in why.
static char *tp_include_expand(const char *problem_path, const char *rec,
                               int depth, const char **end,
                               char *why, size_t wcap) {
  TpInclude inc;
  const char *e = tp_include_parse(rec, &inc);
  if (e == NULL) {
    const char *r = tp_skip_blank(rec);
    size_t n = strcspn(r, "\n");
    snprintf(why, wcap, "unsupported include directive: %.*s",
             (int)(n < 200 ? n : 200), r);
    return NULL;
  }
  if (end != NULL) {
    *end = e;
  }
  char *out = NULL;
  if (depth > TP_INC_DEPTH) {
    snprintf(why, wcap, "include('%s') nests deeper than %d levels",
             inc.file, TP_INC_DEPTH);
  } else {
    char *body = tp_resolve_include(problem_path, inc.file);
    if (body == NULL) {
      snprintf(why, wcap, "cannot resolve include('%s')", inc.file);
    } else {
      out = tp_include_splice(problem_path, body, depth, why, wcap);
      if (out != NULL && inc.sel != NULL) {
        char *all = out;
        out = tp_include_select(all, &inc, why, wcap);
        free(all);
      }
    }
  }
  tp_include_free(&inc);
  return out;
}
