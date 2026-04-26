// test_wald.c - Waldmeister .pr parser pieces.
//
// 6.3a: data model only -- WaldSpec init/free + default state.
// Later stages (6.3b lexer, 6.3c section drivers, 6.3d term parser,
// 6.3e equations, 6.3f top-level driver, 6.3g end-to-end) extend
// this file.

#include "../src/thvm.c"
#include "test.h"

int main(void) {
  thvm_init();

  TEST_BEGIN("wald/init-and-free");
  {
    WaldSpec *s = wald_init();
    CHECK(s != NULL);
    CHECK_EQ((int)s->mode_proof,    1);          // defaults to PROOF
    CHECK_EQ(s->n_symbols,          0u);
    CHECK_EQ(s->n_vars,             0u);
    CHECK_EQ(s->n_eqns,             0u);
    CHECK_EQ(s->next_label,         1u);         // skips the anonymous-tuple label
    CHECK_EQ(s->goal_lhs,           0u);
    CHECK_EQ(s->goal_rhs,           0u);
    CHECK_EQ((int)s->name[0],       0);          // empty name string
    wald_free(s);
  }

  TEST_BEGIN("wald/free-null-is-safe");
  {
    wald_free(NULL);   // no crash, no-op
  }

  TEST_BEGIN("wald/caps-are-defined");
  {
    // Sanity that the cap macros are sized roughly right -- catches
    // accidental zero-init regressions in thvm.h.
    CHECK(WALD_MAX_SYMBOLS >= 4u);
    CHECK(WALD_MAX_VARS    >= 3u);
    CHECK(WALD_MAX_EQNS    >= 3u);
    CHECK(WALD_NAME_LEN    >= 4u);
  }

  // === 6.3b lexer =====================================================

  TEST_BEGIN("wald/lex-empty-yields-end");
  {
    WaldLex lex;
    wald_lex_init(&lex, "");
    CHECK_EQ((int)wald_lex_next(&lex), (int)WT_END);
  }

  TEST_BEGIN("wald/lex-whitespace-only-yields-end");
  {
    WaldLex lex;
    wald_lex_init(&lex, "   \n\t\r  ");
    CHECK_EQ((int)wald_lex_next(&lex), (int)WT_END);
  }

  TEST_BEGIN("wald/lex-skip-percent-comment");
  {
    WaldLex lex;
    wald_lex_init(&lex, "%this is a comment\nname");
    CHECK_EQ((int)wald_lex_next(&lex), (int)WT_IDENT);
    CHECK_EQ((int)strcmp(lex.tok_text, "name"), 0);
  }

  TEST_BEGIN("wald/lex-ident-with-digits-and-underscore");
  {
    WaldLex lex;
    wald_lex_init(&lex, "abc_123 _x9");
    CHECK_EQ((int)wald_lex_next(&lex), (int)WT_IDENT);
    CHECK_EQ((int)strcmp(lex.tok_text, "abc_123"), 0);
    CHECK_EQ((int)wald_lex_next(&lex), (int)WT_IDENT);
    CHECK_EQ((int)strcmp(lex.tok_text, "_x9"), 0);
    CHECK_EQ((int)wald_lex_next(&lex), (int)WT_END);
  }

  TEST_BEGIN("wald/lex-arrow-vs-minus");
  {
    WaldLex lex;
    wald_lex_init(&lex, "->");
    CHECK_EQ((int)wald_lex_next(&lex), (int)WT_ARROW);
    CHECK_EQ((int)wald_lex_next(&lex), (int)WT_END);
  }

  TEST_BEGIN("wald/lex-punctuation-individually");
  {
    WaldLex lex;
    wald_lex_init(&lex, ": = ( ) , >");
    CHECK_EQ((int)wald_lex_next(&lex), (int)WT_COLON);
    CHECK_EQ((int)wald_lex_next(&lex), (int)WT_EQ);
    CHECK_EQ((int)wald_lex_next(&lex), (int)WT_LPAREN);
    CHECK_EQ((int)wald_lex_next(&lex), (int)WT_RPAREN);
    CHECK_EQ((int)wald_lex_next(&lex), (int)WT_COMMA);
    CHECK_EQ((int)wald_lex_next(&lex), (int)WT_GT);
    CHECK_EQ((int)wald_lex_next(&lex), (int)WT_END);
  }

  TEST_BEGIN("wald/lex-equation-stream");
  {
    // f(x, e) = x   -> IDENT LPAREN IDENT COMMA IDENT RPAREN EQ IDENT END
    WaldLex lex;
    wald_lex_init(&lex, "f(x, e) = x");
    CHECK_EQ((int)wald_lex_next(&lex), (int)WT_IDENT);
    CHECK_EQ((int)strcmp(lex.tok_text, "f"), 0);
    CHECK_EQ((int)wald_lex_next(&lex), (int)WT_LPAREN);
    CHECK_EQ((int)wald_lex_next(&lex), (int)WT_IDENT);
    CHECK_EQ((int)strcmp(lex.tok_text, "x"), 0);
    CHECK_EQ((int)wald_lex_next(&lex), (int)WT_COMMA);
    CHECK_EQ((int)wald_lex_next(&lex), (int)WT_IDENT);
    CHECK_EQ((int)strcmp(lex.tok_text, "e"), 0);
    CHECK_EQ((int)wald_lex_next(&lex), (int)WT_RPAREN);
    CHECK_EQ((int)wald_lex_next(&lex), (int)WT_EQ);
    CHECK_EQ((int)wald_lex_next(&lex), (int)WT_IDENT);
    CHECK_EQ((int)strcmp(lex.tok_text, "x"), 0);
    CHECK_EQ((int)wald_lex_next(&lex), (int)WT_END);
  }

  TEST_BEGIN("wald/lex-truncates-long-ident");
  {
    // Ident longer than WALD_NAME_LEN-1 truncates to fit.
    char buf[WALD_NAME_LEN + 16];
    for (u32 i = 0; i < sizeof(buf) - 1; i++) buf[i] = 'a';
    buf[sizeof(buf) - 1] = '\0';
    WaldLex lex;
    wald_lex_init(&lex, buf);
    CHECK_EQ((int)wald_lex_next(&lex), (int)WT_IDENT);
    CHECK_EQ(lex.tok_len, (u64)(WALD_NAME_LEN - 1));
    CHECK_EQ((int)lex.tok_text[WALD_NAME_LEN - 1], 0);   // null-terminated
    // Next token should be END (the trailing chars are still part of the
    // ident in the source; we consumed all of them, just truncated text).
    CHECK_EQ((int)wald_lex_next(&lex), (int)WT_END);
  }

  TEST_BEGIN("wald/lex-error-on-unknown-char");
  {
    WaldLex lex;
    wald_lex_init(&lex, "@");
    CHECK_EQ((int)wald_lex_next(&lex), (int)WT_ERR);
  }

  // === 6.3c1 section-detect infrastructure ===========================

  TEST_BEGIN("wald/section-from-ident-known-keywords");
  {
    CHECK_EQ((int)wald_section_from_ident("NAME"),       (int)WSEC_NAME);
    CHECK_EQ((int)wald_section_from_ident("MODE"),       (int)WSEC_MODE);
    CHECK_EQ((int)wald_section_from_ident("SORTS"),      (int)WSEC_SORTS);
    CHECK_EQ((int)wald_section_from_ident("SIGNATURE"),  (int)WSEC_SIGNATURE);
    CHECK_EQ((int)wald_section_from_ident("VARIABLES"),  (int)WSEC_VARIABLES);
    CHECK_EQ((int)wald_section_from_ident("ORDERING"),   (int)WSEC_ORDERING);
    CHECK_EQ((int)wald_section_from_ident("EQUATIONS"),  (int)WSEC_EQUATIONS);
    CHECK_EQ((int)wald_section_from_ident("CONCLUSION"), (int)WSEC_CONCLUSION);
  }

  TEST_BEGIN("wald/section-from-ident-unknown-yields-none");
  {
    CHECK_EQ((int)wald_section_from_ident("FOO"),    (int)WSEC_NONE);
    CHECK_EQ((int)wald_section_from_ident("name"),   (int)WSEC_NONE);  // case-sensitive
    CHECK_EQ((int)wald_section_from_ident(""),       (int)WSEC_NONE);
  }

  TEST_BEGIN("wald/lex-peek-then-next-returns-same-token");
  {
    WaldLex lex;
    wald_lex_init(&lex, "abc def");
    CHECK_EQ((int)wald_lex_peek(&lex), (int)WT_IDENT);
    CHECK_EQ((int)strcmp(lex.tok_text, "abc"), 0);
    // Peek again -- still the same token (no consumption).
    CHECK_EQ((int)wald_lex_peek(&lex), (int)WT_IDENT);
    CHECK_EQ((int)wald_lex_next(&lex), (int)WT_IDENT);
    CHECK_EQ((int)strcmp(lex.tok_text, "abc"), 0);
    CHECK_EQ((int)wald_lex_next(&lex), (int)WT_IDENT);
    CHECK_EQ((int)strcmp(lex.tok_text, "def"), 0);
    CHECK_EQ((int)wald_lex_next(&lex), (int)WT_END);
  }

  TEST_BEGIN("wald/lex-peek-end-survives-multiple-calls");
  {
    WaldLex lex;
    wald_lex_init(&lex, "");
    CHECK_EQ((int)wald_lex_peek(&lex), (int)WT_END);
    CHECK_EQ((int)wald_lex_peek(&lex), (int)WT_END);
    CHECK_EQ((int)wald_lex_next(&lex), (int)WT_END);
  }

  TEST_BEGIN("wald/skip-to-section-finds-keyword");
  {
    WaldLex lex;
    wald_lex_init(&lex, "blah blah blah ORDERING LPO");
    CHECK_EQ((int)wald_skip_to_section(&lex), (int)WSEC_ORDERING);
    // Lexer is positioned past ORDERING.
    CHECK_EQ((int)wald_lex_next(&lex), (int)WT_IDENT);
    CHECK_EQ((int)strcmp(lex.tok_text, "LPO"), 0);
  }

  TEST_BEGIN("wald/skip-to-section-eof-yields-none");
  {
    WaldLex lex;
    wald_lex_init(&lex, "blah blah no section here");
    CHECK_EQ((int)wald_skip_to_section(&lex), (int)WSEC_NONE);
  }

  TEST_BEGIN("wald/skip-to-section-from-empty-yields-none");
  {
    WaldLex lex;
    wald_lex_init(&lex, "");
    CHECK_EQ((int)wald_skip_to_section(&lex), (int)WSEC_NONE);
  }

  // === 6.3c2 NAME / MODE / SORTS parsers =============================

  TEST_BEGIN("wald/parse-name-stores-and-finds-next-section");
  {
    WaldLex lex;
    wald_lex_init(&lex, "group MODE PROOF");
    WaldSpec *s = wald_init();
    WaldSection next = wald_parse_name(s, &lex);
    CHECK_EQ((int)next, (int)WSEC_MODE);
    CHECK_EQ((int)strcmp(s->name, "group"), 0);
    wald_free(s);
  }

  TEST_BEGIN("wald/parse-name-empty-section-keeps-default");
  {
    // Section keyword right after NAME header -> empty NAME.
    WaldLex lex;
    wald_lex_init(&lex, "MODE PROOF");
    WaldSpec *s = wald_init();
    WaldSection next = wald_parse_name(s, &lex);
    CHECK_EQ((int)next, (int)WSEC_MODE);
    CHECK_EQ((int)s->name[0], 0);   // unchanged
    wald_free(s);
  }

  TEST_BEGIN("wald/parse-mode-proof-keeps-flag");
  {
    WaldLex lex;
    wald_lex_init(&lex, "PROOF SIGNATURE");
    WaldSpec *s = wald_init();
    s->mode_proof = 0;   // start cleared so we can see the parser set it
    WaldSection next = wald_parse_mode(s, &lex);
    CHECK_EQ((int)next, (int)WSEC_SIGNATURE);
    CHECK_EQ((int)s->mode_proof, 1);
    wald_free(s);
  }

  TEST_BEGIN("wald/parse-mode-completion-clears-flag");
  {
    WaldLex lex;
    wald_lex_init(&lex, "COMPLETION SIGNATURE");
    WaldSpec *s = wald_init();
    WaldSection next = wald_parse_mode(s, &lex);
    CHECK_EQ((int)next, (int)WSEC_SIGNATURE);
    CHECK_EQ((int)s->mode_proof, 0);
    wald_free(s);
  }

  TEST_BEGIN("wald/parse-sorts-consumes-list");
  {
    WaldLex lex;
    wald_lex_init(&lex, "ANY ANY1 ANY2 SIGNATURE foo");
    WaldSpec *s = wald_init();
    WaldSection next = wald_parse_sorts(s, &lex);
    CHECK_EQ((int)next, (int)WSEC_SIGNATURE);
    // The next token after the parser should be "foo".
    CHECK_EQ((int)wald_lex_next(&lex), (int)WT_IDENT);
    CHECK_EQ((int)strcmp(lex.tok_text, "foo"), 0);
    wald_free(s);
  }

  TEST_BEGIN("wald/parse-sorts-eof-yields-none");
  {
    WaldLex lex;
    wald_lex_init(&lex, "ANY ANY1");
    WaldSpec *s = wald_init();
    CHECK_EQ((int)wald_parse_sorts(s, &lex), (int)WSEC_NONE);
    wald_free(s);
  }

  // === 6.3c3 SIGNATURE parser =========================================

  TEST_BEGIN("wald/parse-signature-single-zero-arity");
  {
    // e: -> ANY  ORDERING ...
    WaldLex lex;
    wald_lex_init(&lex, "e: -> ANY ORDERING LPO");
    WaldSpec *s = wald_init();
    CHECK_EQ((int)wald_parse_signature(s, &lex), (int)WSEC_ORDERING);
    CHECK_EQ(s->n_symbols, 1u);
    CHECK_EQ((int)strcmp(s->symbols[0].name, "e"), 0);
    CHECK_EQ(s->symbols[0].arity, 0u);
    CHECK_EQ(s->symbols[0].label, 1u);   // first label
    CHECK_EQ(s->next_label, 2u);
    wald_free(s);
  }

  TEST_BEGIN("wald/parse-signature-three-entries-monotonic-labels");
  {
    WaldLex lex;
    wald_lex_init(&lex,
                  "e: -> ANY  i: ANY -> ANY  f: ANY ANY -> ANY  ORDERING LPO");
    WaldSpec *s = wald_init();
    CHECK_EQ((int)wald_parse_signature(s, &lex), (int)WSEC_ORDERING);
    CHECK_EQ(s->n_symbols, 3u);
    CHECK_EQ((int)strcmp(s->symbols[0].name, "e"), 0);
    CHECK_EQ(s->symbols[0].arity, 0u);
    CHECK_EQ(s->symbols[0].label, 1u);
    CHECK_EQ((int)strcmp(s->symbols[1].name, "i"), 0);
    CHECK_EQ(s->symbols[1].arity, 1u);
    CHECK_EQ(s->symbols[1].label, 2u);
    CHECK_EQ((int)strcmp(s->symbols[2].name, "f"), 0);
    CHECK_EQ(s->symbols[2].arity, 2u);
    CHECK_EQ(s->symbols[2].label, 3u);
    wald_free(s);
  }

  TEST_BEGIN("wald/parse-signature-empty-section");
  {
    // Section keyword right at the start: no signature entries.
    WaldLex lex;
    wald_lex_init(&lex, "ORDERING LPO");
    WaldSpec *s = wald_init();
    CHECK_EQ((int)wald_parse_signature(s, &lex), (int)WSEC_ORDERING);
    CHECK_EQ(s->n_symbols, 0u);
    wald_free(s);
  }

  TEST_BEGIN("wald/parse-signature-eof-mid-entry");
  {
    // Truncated input: "f: ANY ANY ->" without a result sort.
    WaldLex lex;
    wald_lex_init(&lex, "f: ANY ANY ->");
    WaldSpec *s = wald_init();
    CHECK_EQ((int)wald_parse_signature(s, &lex), (int)WSEC_NONE);
    // The half-parsed entry didn't get committed.
    CHECK_EQ(s->n_symbols, 0u);
    wald_free(s);
  }

  // === 6.3c4 VARIABLES parser =========================================

  TEST_BEGIN("wald/parse-variables-three-comma-separated");
  {
    WaldLex lex;
    wald_lex_init(&lex, "x,y,z : ANY EQUATIONS foo");
    WaldSpec *s = wald_init();
    CHECK_EQ((int)wald_parse_variables(s, &lex), (int)WSEC_EQUATIONS);
    CHECK_EQ(s->n_vars, 3u);
    CHECK_EQ((int)strcmp(s->vars[0].name, "x"), 0);
    CHECK_EQ(s->vars[0].var_id, 0u);
    CHECK_EQ((int)strcmp(s->vars[1].name, "y"), 0);
    CHECK_EQ(s->vars[1].var_id, 1u);
    CHECK_EQ((int)strcmp(s->vars[2].name, "z"), 0);
    CHECK_EQ(s->vars[2].var_id, 2u);
    // Lexer past EQUATIONS at "foo".
    CHECK_EQ((int)wald_lex_next(&lex), (int)WT_IDENT);
    CHECK_EQ((int)strcmp(lex.tok_text, "foo"), 0);
    wald_free(s);
  }

  TEST_BEGIN("wald/parse-variables-empty-section");
  {
    WaldLex lex;
    wald_lex_init(&lex, "EQUATIONS bar");
    WaldSpec *s = wald_init();
    CHECK_EQ((int)wald_parse_variables(s, &lex), (int)WSEC_EQUATIONS);
    CHECK_EQ(s->n_vars, 0u);
    wald_free(s);
  }

  TEST_BEGIN("wald/parse-variables-multi-decl");
  {
    // Two var-decl groups in one section.
    WaldLex lex;
    wald_lex_init(&lex, "x,y : ANY  z,w : ANY1  EQUATIONS");
    WaldSpec *s = wald_init();
    CHECK_EQ((int)wald_parse_variables(s, &lex), (int)WSEC_EQUATIONS);
    CHECK_EQ(s->n_vars, 4u);
    CHECK_EQ((int)strcmp(s->vars[0].name, "x"), 0);
    CHECK_EQ((int)strcmp(s->vars[1].name, "y"), 0);
    CHECK_EQ((int)strcmp(s->vars[2].name, "z"), 0);
    CHECK_EQ((int)strcmp(s->vars[3].name, "w"), 0);
    CHECK_EQ(s->vars[0].var_id, 0u);
    CHECK_EQ(s->vars[3].var_id, 3u);
    wald_free(s);
  }

  TEST_BEGIN("wald/parse-variables-truncated-eof-keeps-registered");
  {
    // EOF mid-list: registers x and y, then EOF -> WSEC_NONE.
    WaldLex lex;
    wald_lex_init(&lex, "x,y");
    WaldSpec *s = wald_init();
    CHECK_EQ((int)wald_parse_variables(s, &lex), (int)WSEC_NONE);
    CHECK_EQ(s->n_vars, 2u);
    wald_free(s);
  }

  // === 6.3c5 ORDERING parser =========================================

  // Helper: pre-populate a WaldSpec with the standard {e, i, f, a}
  // group signature.  Tests that exercise ORDERING use this so
  // the parser has symbols to look up.
  // (Inline rather than a static helper so the harness stays simple.)
  TEST_BEGIN("wald/parse-ordering-lpo-chain-assigns-ranks");
  {
    WaldSpec *s = wald_init();
    // Pre-populate signature: e (label 1), i (2), f (3), a (4).
    s->n_symbols = 4;
    strncpy(s->symbols[0].name, "e", WALD_NAME_LEN - 1);
    strncpy(s->symbols[1].name, "i", WALD_NAME_LEN - 1);
    strncpy(s->symbols[2].name, "f", WALD_NAME_LEN - 1);
    strncpy(s->symbols[3].name, "a", WALD_NAME_LEN - 1);
    s->next_label = 5;

    WaldLex lex;
    wald_lex_init(&lex, "LPO i > f > e > a EQUATIONS foo");
    CHECK_EQ((int)wald_parse_ordering(s, &lex), (int)WSEC_EQUATIONS);
    // Chain "i > f > e > a" -> ranks i=3, f=2, e=1, a=0.
    CHECK_EQ(s->symbols[1].prec_rank, 3u);   // i
    CHECK_EQ(s->symbols[2].prec_rank, 2u);   // f
    CHECK_EQ(s->symbols[0].prec_rank, 1u);   // e
    CHECK_EQ(s->symbols[3].prec_rank, 0u);   // a
    // Lexer past EQUATIONS at "foo".
    CHECK_EQ((int)wald_lex_next(&lex), (int)WT_IDENT);
    CHECK_EQ((int)strcmp(lex.tok_text, "foo"), 0);
    wald_free(s);
  }

  TEST_BEGIN("wald/parse-ordering-kbo-skips-weights-then-chain");
  {
    WaldSpec *s = wald_init();
    s->n_symbols = 4;
    strncpy(s->symbols[0].name, "e", WALD_NAME_LEN - 1);
    strncpy(s->symbols[1].name, "i", WALD_NAME_LEN - 1);
    strncpy(s->symbols[2].name, "f", WALD_NAME_LEN - 1);
    strncpy(s->symbols[3].name, "a", WALD_NAME_LEN - 1);

    // KBO with weight list, then precedence chain.
    WaldLex lex;
    wald_lex_init(&lex, "KBO i=0, f=1, e=1, a=1 i > f > e > a EQUATIONS");
    CHECK_EQ((int)wald_parse_ordering(s, &lex), (int)WSEC_EQUATIONS);
    // Same ranks as the LPO case (weights are discarded).
    CHECK_EQ(s->symbols[1].prec_rank, 3u);
    CHECK_EQ(s->symbols[2].prec_rank, 2u);
    CHECK_EQ(s->symbols[0].prec_rank, 1u);
    CHECK_EQ(s->symbols[3].prec_rank, 0u);
    wald_free(s);
  }

  TEST_BEGIN("wald/parse-ordering-empty-section-leaves-ranks-zero");
  {
    WaldSpec *s = wald_init();
    s->n_symbols = 1;
    strncpy(s->symbols[0].name, "e", WALD_NAME_LEN - 1);

    WaldLex lex;
    wald_lex_init(&lex, "EQUATIONS");
    CHECK_EQ((int)wald_parse_ordering(s, &lex), (int)WSEC_EQUATIONS);
    CHECK_EQ(s->symbols[0].prec_rank, 0u);
    wald_free(s);
  }

  TEST_BEGIN("wald/parse-ordering-lone-ident-no-chain");
  {
    // "LPO foo EQUATIONS" -- "foo" is read but no ">" follows, so
    // it's NOT part of the chain.  All ranks stay 0.
    WaldSpec *s = wald_init();
    s->n_symbols = 1;
    strncpy(s->symbols[0].name, "foo", WALD_NAME_LEN - 1);

    WaldLex lex;
    wald_lex_init(&lex, "LPO foo EQUATIONS");
    CHECK_EQ((int)wald_parse_ordering(s, &lex), (int)WSEC_EQUATIONS);
    CHECK_EQ(s->symbols[0].prec_rank, 0u);
    wald_free(s);
  }

  // === 6.3d term parser ==============================================

  TEST_BEGIN("wald/parse-term-variable-yields-fvr");
  {
    WaldSpec *s = wald_init();
    s->n_vars = 1;
    strncpy(s->vars[0].name, "x", WALD_NAME_LEN - 1);
    s->vars[0].var_id = 0;

    WaldLex lex;
    wald_lex_init(&lex, "x");
    Term t = wald_parse_term(s, &lex);
    CHECK_EQ(term_tag(t), TAG_FVR);
    CHECK_EQ(term_ext(t), 0u);
    wald_free(s);
  }

  TEST_BEGIN("wald/parse-term-constant-yields-zero-arity-ctr");
  {
    WaldSpec *s = wald_init();
    s->n_symbols = 1;
    strncpy(s->symbols[0].name, "e", WALD_NAME_LEN - 1);
    s->symbols[0].label = 1;
    s->symbols[0].arity = 0;
    s->next_label = 2;

    WaldLex lex;
    wald_lex_init(&lex, "e");
    Term t = wald_parse_term(s, &lex);
    CHECK_EQ(term_tag(t), TAG_CTR);
    CHECK_EQ(term_ext(t), 1u);
    CHECK_EQ(term_ctr_n(t), 0u);
    wald_free(s);
  }

  TEST_BEGIN("wald/parse-term-application-with-args");
  {
    // f(x, e) where f is arity-2 (label 3), x is var, e is arity-0 (label 1).
    WaldSpec *s = wald_init();
    s->n_symbols = 2;
    strncpy(s->symbols[0].name, "e", WALD_NAME_LEN - 1);
    s->symbols[0].label = 1;
    s->symbols[0].arity = 0;
    strncpy(s->symbols[1].name, "f", WALD_NAME_LEN - 1);
    s->symbols[1].label = 3;
    s->symbols[1].arity = 2;
    s->n_vars = 1;
    strncpy(s->vars[0].name, "x", WALD_NAME_LEN - 1);
    s->vars[0].var_id = 0;

    WaldLex lex;
    wald_lex_init(&lex, "f(x, e)");
    Term t = wald_parse_term(s, &lex);
    CHECK_EQ(term_tag(t), TAG_CTR);
    CHECK_EQ(term_ext(t), 3u);
    CHECK_EQ(term_ctr_n(t), 2u);
    Term a0 = term_ctr_at(t, 0);
    Term a1 = term_ctr_at(t, 1);
    CHECK_EQ(term_tag(a0), TAG_FVR);
    CHECK_EQ(term_ext(a0), 0u);
    CHECK_EQ(term_tag(a1), TAG_CTR);
    CHECK_EQ(term_ext(a1), 1u);
    wald_free(s);
  }

  TEST_BEGIN("wald/parse-term-nested");
  {
    // f(i(x), e) -- nested unary i around var x, plus constant e.
    WaldSpec *s = wald_init();
    s->n_symbols = 3;
    strncpy(s->symbols[0].name, "e", WALD_NAME_LEN - 1);
    s->symbols[0].label = 1;
    s->symbols[0].arity = 0;
    strncpy(s->symbols[1].name, "i", WALD_NAME_LEN - 1);
    s->symbols[1].label = 2;
    s->symbols[1].arity = 1;
    strncpy(s->symbols[2].name, "f", WALD_NAME_LEN - 1);
    s->symbols[2].label = 3;
    s->symbols[2].arity = 2;
    s->n_vars = 1;
    strncpy(s->vars[0].name, "x", WALD_NAME_LEN - 1);

    WaldLex lex;
    wald_lex_init(&lex, "f(i(x), e)");
    Term t = wald_parse_term(s, &lex);
    CHECK_EQ(term_ext(t), 3u);          // f
    Term inner = term_ctr_at(t, 0);
    CHECK_EQ(term_ext(inner), 2u);      // i
    Term innermost = term_ctr_at(inner, 0);
    CHECK_EQ(term_tag(innermost), TAG_FVR);   // x
    CHECK_EQ(term_ext(term_ctr_at(t, 1)), 1u);  // e
    wald_free(s);
  }

  TEST_BEGIN("wald/parse-term-unknown-ident-yields-zero");
  {
    WaldSpec *s = wald_init();
    WaldLex lex;
    wald_lex_init(&lex, "unknown");
    CHECK_EQ(wald_parse_term(s, &lex), 0u);
    wald_free(s);
  }

  TEST_BEGIN("wald/parse-term-arity-mismatch-yields-zero");
  {
    // i is arity 1; calling i(x, y) is invalid.
    WaldSpec *s = wald_init();
    s->n_symbols = 1;
    strncpy(s->symbols[0].name, "i", WALD_NAME_LEN - 1);
    s->symbols[0].label = 2;
    s->symbols[0].arity = 1;
    s->n_vars = 2;
    strncpy(s->vars[0].name, "x", WALD_NAME_LEN - 1);
    s->vars[0].var_id = 0;
    strncpy(s->vars[1].name, "y", WALD_NAME_LEN - 1);
    s->vars[1].var_id = 1;

    WaldLex lex;
    wald_lex_init(&lex, "i(x, y)");
    CHECK_EQ(wald_parse_term(s, &lex), 0u);
    wald_free(s);
  }

  TEST_BEGIN("wald/parse-term-constant-cannot-take-args");
  {
    // e is arity 0; e(x) should fail.
    WaldSpec *s = wald_init();
    s->n_symbols = 1;
    strncpy(s->symbols[0].name, "e", WALD_NAME_LEN - 1);
    s->symbols[0].label = 1;
    s->symbols[0].arity = 0;
    s->n_vars = 1;
    strncpy(s->vars[0].name, "x", WALD_NAME_LEN - 1);
    s->vars[0].var_id = 0;

    WaldLex lex;
    wald_lex_init(&lex, "e(x)");
    CHECK_EQ(wald_parse_term(s, &lex), 0u);
    wald_free(s);
  }

  // === 6.3e EQUATIONS / CONCLUSION parsers ===========================

  // Helper to set up the standard group signature.
  TEST_BEGIN("wald/parse-equations-three-axioms");
  {
    WaldSpec *s = wald_init();
    s->n_symbols = 4;
    strncpy(s->symbols[0].name, "e", WALD_NAME_LEN - 1);
    s->symbols[0].label = 1; s->symbols[0].arity = 0;
    strncpy(s->symbols[1].name, "i", WALD_NAME_LEN - 1);
    s->symbols[1].label = 2; s->symbols[1].arity = 1;
    strncpy(s->symbols[2].name, "f", WALD_NAME_LEN - 1);
    s->symbols[2].label = 3; s->symbols[2].arity = 2;
    strncpy(s->symbols[3].name, "a", WALD_NAME_LEN - 1);
    s->symbols[3].label = 4; s->symbols[3].arity = 0;
    s->n_vars = 3;
    strncpy(s->vars[0].name, "x", WALD_NAME_LEN - 1); s->vars[0].var_id = 0;
    strncpy(s->vars[1].name, "y", WALD_NAME_LEN - 1); s->vars[1].var_id = 1;
    strncpy(s->vars[2].name, "z", WALD_NAME_LEN - 1); s->vars[2].var_id = 2;

    WaldLex lex;
    wald_lex_init(&lex,
      "f(x, e) = x  "
      "f(x, i(x)) = e  "
      "f(f(x, y), z) = f(x, f(y, z))  "
      "CONCLUSION foo");
    CHECK_EQ((int)wald_parse_equations(s, &lex), (int)WSEC_CONCLUSION);
    CHECK_EQ(s->n_eqns, 3u);

    // Spot-check the first axiom: lhs is f(x, e), rhs is x.
    Term lhs0 = s->eqn_lhs[0];
    Term rhs0 = s->eqn_rhs[0];
    CHECK_EQ(term_tag(lhs0), TAG_CTR);
    CHECK_EQ(term_ext(lhs0), 3u);                // f
    CHECK_EQ(term_ext(term_ctr_at(lhs0, 1)), 1u); // e
    CHECK_EQ(term_tag(rhs0), TAG_FVR);
    CHECK_EQ(term_ext(rhs0), 0u);                 // x
    wald_free(s);
  }

  TEST_BEGIN("wald/parse-equations-empty-section");
  {
    WaldSpec *s = wald_init();
    WaldLex lex;
    wald_lex_init(&lex, "CONCLUSION foo");
    CHECK_EQ((int)wald_parse_equations(s, &lex), (int)WSEC_CONCLUSION);
    CHECK_EQ(s->n_eqns, 0u);
    wald_free(s);
  }

  TEST_BEGIN("wald/parse-conclusion-stores-goal");
  {
    WaldSpec *s = wald_init();
    s->n_symbols = 2;
    strncpy(s->symbols[0].name, "e", WALD_NAME_LEN - 1);
    s->symbols[0].label = 1; s->symbols[0].arity = 0;
    strncpy(s->symbols[1].name, "a", WALD_NAME_LEN - 1);
    s->symbols[1].label = 4; s->symbols[1].arity = 0;

    WaldLex lex;
    wald_lex_init(&lex, "a = e");
    CHECK_EQ((int)wald_parse_conclusion(s, &lex), (int)WSEC_NONE);
    CHECK_EQ(term_tag(s->goal_lhs), TAG_CTR);
    CHECK_EQ(term_ext(s->goal_lhs), 4u);   // a
    CHECK_EQ(term_tag(s->goal_rhs), TAG_CTR);
    CHECK_EQ(term_ext(s->goal_rhs), 1u);   // e
    wald_free(s);
  }

  TEST_BEGIN("wald/parse-conclusion-rejects-multiple");
  {
    WaldSpec *s = wald_init();
    s->n_symbols = 3;
    strncpy(s->symbols[0].name, "a", WALD_NAME_LEN - 1);
    s->symbols[0].label = 4; s->symbols[0].arity = 0;
    strncpy(s->symbols[1].name, "e", WALD_NAME_LEN - 1);
    s->symbols[1].label = 1; s->symbols[1].arity = 0;
    strncpy(s->symbols[2].name, "i", WALD_NAME_LEN - 1);
    s->symbols[2].label = 2; s->symbols[2].arity = 1;

    WaldLex lex;
    wald_lex_init(&lex, "a = e   i(a) = a");
    CHECK_EQ((int)wald_parse_conclusion(s, &lex), (int)WSEC_NONE);
    // Only the first pair stored: a = e
    CHECK_EQ(term_ext(s->goal_lhs), 4u);   // a (NOT i(a))
    CHECK_EQ(term_ext(s->goal_rhs), 1u);   // e (NOT a)
    wald_free(s);
  }

  // === 6.3f top-level wald_parse driver ==============================

  TEST_BEGIN("wald/parse-null-args-yields-err");
  {
    CHECK_EQ((int)wald_parse(NULL, NULL), (int)WALD_ERR_NULL);
  }

  TEST_BEGIN("wald/parse-empty-source-yields-no-section");
  {
    WaldSpec *s = wald_init();
    CHECK_EQ((int)wald_parse("", s), (int)WALD_ERR_NO_SECTION);
    wald_free(s);
  }

  TEST_BEGIN("wald/parse-only-junk-yields-no-section");
  {
    WaldSpec *s = wald_init();
    CHECK_EQ((int)wald_parse("foo bar baz", s), (int)WALD_ERR_NO_SECTION);
    wald_free(s);
  }

  TEST_BEGIN("wald/parse-full-group-spec");
  {
    // The standard group axiomatization from the example.pr file.
    const char *src =
      "NAME            group\n"
      "MODE            PROOF\n"
      "SORTS           ANY\n"
      "SIGNATURE       e: -> ANY\n"
      "                i: ANY -> ANY\n"
      "                f: ANY ANY -> ANY\n"
      "                a: -> ANY\n"
      "ORDERING        LPO\n"
      "                i > f > e > a\n"
      "VARIABLES       x,y,z : ANY\n"
      "EQUATIONS       f(x, e) = x\n"
      "                f(x, i(x)) = e\n"
      "                f(f(x, y), z) = f(x, f(y, z))\n"
      "CONCLUSION      f(a, i(a)) = e\n";

    WaldSpec *s = wald_init();
    CHECK_EQ((int)wald_parse(src, s), (int)WALD_OK);

    // Spec identity.
    CHECK_EQ((int)strcmp(s->name, "group"), 0);
    CHECK_EQ((int)s->mode_proof, 1);

    // Signature: 4 symbols with monotonic labels.
    CHECK_EQ(s->n_symbols, 4u);
    CHECK_EQ((int)strcmp(s->symbols[0].name, "e"), 0);
    CHECK_EQ(s->symbols[0].arity, 0u);
    CHECK_EQ((int)strcmp(s->symbols[1].name, "i"), 0);
    CHECK_EQ(s->symbols[1].arity, 1u);
    CHECK_EQ((int)strcmp(s->symbols[2].name, "f"), 0);
    CHECK_EQ(s->symbols[2].arity, 2u);
    CHECK_EQ((int)strcmp(s->symbols[3].name, "a"), 0);
    CHECK_EQ(s->symbols[3].arity, 0u);

    // Ordering: i > f > e > a -> ranks 3, 2, 1, 0 respectively.
    CHECK_EQ(s->symbols[1].prec_rank, 3u);   // i
    CHECK_EQ(s->symbols[2].prec_rank, 2u);   // f
    CHECK_EQ(s->symbols[0].prec_rank, 1u);   // e
    CHECK_EQ(s->symbols[3].prec_rank, 0u);   // a

    // Variables: x, y, z -> ids 0, 1, 2.
    CHECK_EQ(s->n_vars, 3u);
    CHECK_EQ((int)strcmp(s->vars[0].name, "x"), 0);
    CHECK_EQ(s->vars[0].var_id, 0u);
    CHECK_EQ((int)strcmp(s->vars[2].name, "z"), 0);
    CHECK_EQ(s->vars[2].var_id, 2u);

    // Three axioms.
    CHECK_EQ(s->n_eqns, 3u);

    // Goal: f(a, i(a)) = e
    CHECK(s->goal_lhs != 0u);
    CHECK(s->goal_rhs != 0u);
    CHECK_EQ(term_tag(s->goal_lhs), TAG_CTR);
    CHECK_EQ(term_ext(s->goal_lhs), s->symbols[2].label);   // f
    CHECK_EQ(term_tag(s->goal_rhs), TAG_CTR);
    CHECK_EQ(term_ext(s->goal_rhs), s->symbols[0].label);   // e
    wald_free(s);
  }

  // === 6.3g end-to-end: parsed spec feeds KBO + saturation ============

  // Helper: build a KboConfig from a parsed WaldSpec.  Weights default
  // to 1 (the .pr parser discards them); precedence maps directly
  // from spec->symbols[i].prec_rank with a +1 shift so rank 0 is
  // distinguishable from "unset".  var_weight = 1.
  TEST_BEGIN("wald/parsed-axioms-kbo-orient-correctly");
  {
    static const char *src =
      "NAME            group\n"
      "MODE            PROOF\n"
      "SORTS           ANY\n"
      "SIGNATURE       e: -> ANY\n"
      "                i: ANY -> ANY\n"
      "                f: ANY ANY -> ANY\n"
      "                a: -> ANY\n"
      "ORDERING        LPO\n"
      "                i > f > e > a\n"
      "VARIABLES       x,y,z : ANY\n"
      "EQUATIONS       f(x, e) = x\n"
      "                f(x, i(x)) = e\n"
      "                f(f(x, y), z) = f(x, f(y, z))\n"
      "CONCLUSION      f(a, i(a)) = e\n";

    WaldSpec *spec = wald_init();
    CHECK_EQ((int)wald_parse(src, spec), (int)WALD_OK);
    CHECK_EQ(spec->n_eqns, 3u);

    u32 max_label = 0;
    for (u32 i = 0; i < spec->n_symbols; i++) {
      if (spec->symbols[i].label > max_label) max_label = spec->symbols[i].label;
    }
    u32 weights[16] = {0};
    u32 prec[16]    = {0};
    for (u32 i = 0; i < spec->n_symbols; i++) {
      weights[spec->symbols[i].label] = 1;
      prec[spec->symbols[i].label]    = spec->symbols[i].prec_rank + 1;
    }
    KboConfig cfg = {
      .weights    = weights,
      .precedence = prec,
      .n_labels   = max_label + 1,
      .var_weight = 1,
    };

    // Each axiom should orient lhs > rhs under the LPO-derived KBO config.
    CHECK_EQ((int)thvm_kbo(spec->eqn_lhs[0], spec->eqn_rhs[0], &cfg), (int)KBO_GT);
    CHECK_EQ((int)thvm_kbo(spec->eqn_lhs[1], spec->eqn_rhs[1], &cfg), (int)KBO_GT);
    CHECK_EQ((int)thvm_kbo(spec->eqn_lhs[2], spec->eqn_rhs[2], &cfg), (int)KBO_GT);
    wald_free(spec);
  }

  TEST_BEGIN("wald/parsed-spec-feeds-saturation-and-proves");
  {
    // Same .pr file, but pushed end-to-end through the saturation
    // engine: parse -> build KboConfig -> push axioms -> set goal ->
    // thvm_atp_run -> ATP_PROVED.
    static const char *src =
      "NAME            group\n"
      "MODE            PROOF\n"
      "SORTS           ANY\n"
      "SIGNATURE       e: -> ANY\n"
      "                i: ANY -> ANY\n"
      "                f: ANY ANY -> ANY\n"
      "                a: -> ANY\n"
      "ORDERING        LPO\n"
      "                i > f > e > a\n"
      "VARIABLES       x,y,z : ANY\n"
      "EQUATIONS       f(x, e) = x\n"
      "                f(x, i(x)) = e\n"
      "                f(f(x, y), z) = f(x, f(y, z))\n"
      "CONCLUSION      f(a, i(a)) = e\n";

    WaldSpec *spec = wald_init();
    CHECK_EQ((int)wald_parse(src, spec), (int)WALD_OK);

    u32 max_label = 0;
    for (u32 i = 0; i < spec->n_symbols; i++) {
      if (spec->symbols[i].label > max_label) max_label = spec->symbols[i].label;
    }
    u32 weights[16] = {0};
    u32 prec[16]    = {0};
    for (u32 i = 0; i < spec->n_symbols; i++) {
      weights[spec->symbols[i].label] = 1;
      prec[spec->symbols[i].label]    = spec->symbols[i].prec_rank + 1;
    }
    KboConfig cfg = {
      .weights    = weights,
      .precedence = prec,
      .n_labels   = max_label + 1,
      .var_weight = 1,
    };

    AtpState *atp = thvm_atp_init(&cfg, 64);
    CHECK(atp != NULL);
    for (u32 i = 0; i < spec->n_eqns; i++) {
      CHECK(thvm_atp_add_equation(atp, spec->eqn_lhs[i], spec->eqn_rhs[i]));
    }
    thvm_atp_set_goal(atp, spec->goal_lhs, spec->goal_rhs);

    AtpStatus st = thvm_atp_run(atp);
    CHECK_EQ((int)st, (int)ATP_PROVED);
    CHECK(atp->step <= 20u);
    thvm_atp_free(atp);
    wald_free(spec);
  }

  // === 6.4a wald_parse_file: bad args + load example.pr from disk =====
  TEST_BEGIN("wald/parse-file/null-path");
  {
    WaldSpec *spec = wald_init();
    CHECK_EQ((int)wald_parse_file(NULL, spec), (int)WALD_ERR_NULL);
    wald_free(spec);
  }

  TEST_BEGIN("wald/parse-file/null-spec");
  {
    CHECK_EQ((int)wald_parse_file("waldmeister/documents/example.pr", NULL),
             (int)WALD_ERR_NULL);
  }

  TEST_BEGIN("wald/parse-file/missing-file");
  {
    WaldSpec *spec = wald_init();
    CHECK_EQ((int)wald_parse_file("waldmeister/documents/__NO_SUCH_FILE__.pr",
                                  spec),
             (int)WALD_ERR_FILE);
    wald_free(spec);
  }

  TEST_BEGIN("wald/parse-file/example.pr-from-disk");
  {
    // Tries to load the actual example.pr from the waldmeister/
    // symlink at the repo root.  If the symlink isn't present
    // (CI checkout without the vendored tree) we silently skip
    // the structural assertions -- this is a research fixture,
    // not a regression test.
    WaldSpec *spec = wald_init();
    WaldErr e = wald_parse_file("waldmeister/documents/example.pr", spec);
    if (e == WALD_OK) {
      // group axioms: e, i, f, a + 3 vars + 3 axioms + 1 goal.
      CHECK((int)strcmp(spec->name, "group") == 0);
      CHECK_EQ(spec->mode_proof, 1u);
      CHECK_EQ(spec->n_symbols, 4u);
      CHECK_EQ(spec->n_vars, 3u);
      CHECK_EQ(spec->n_eqns, 3u);
      CHECK(spec->goal_lhs != 0u);
      CHECK(spec->goal_rhs != 0u);
    } else {
      CHECK_EQ((int)e, (int)WALD_ERR_FILE);
    }
    wald_free(spec);
  }

  // === 6.4b end-to-end: parse .pr -> saturation -> PCL trace =========
  //
  // Load the actual example.pr from the vendored Waldmeister tree,
  // run saturation with a generous step budget, and validate the
  // PCL trace text.  Structural invariants we check regardless of
  // whether the goal is proved (the example.pr conclusion
  // f(a, i(a)) = f(i(a), a) needs left-inverse derived from
  // right-inverse + associativity + identity, which is harder than
  // what we can guarantee in a small step budget):
  //   - n_trace >= n_eqns (each axiom is recorded)
  //   - first n_eqns lines start with "<i> (axiom): "
  //   - at least one "orient" line exists (some rule was added)
  TEST_BEGIN("wald/example.pr/end-to-end-pcl-trace");
  {
    WaldSpec *spec = wald_init();
    WaldErr pe = wald_parse_file("waldmeister/documents/example.pr", spec);
    if (pe != WALD_OK) {
      // Symlink absent.  Skip silently.
      CHECK_EQ((int)pe, (int)WALD_ERR_FILE);
      wald_free(spec);
    } else {
      u32 max_label = 0;
      for (u32 i = 0; i < spec->n_symbols; i++) {
        if (spec->symbols[i].label > max_label)
          max_label = spec->symbols[i].label;
      }
      u32 weights[16] = {0};
      u32 prec[16]    = {0};
      for (u32 i = 0; i < spec->n_symbols; i++) {
        weights[spec->symbols[i].label] = 1;
        prec[spec->symbols[i].label]    = spec->symbols[i].prec_rank + 1;
      }
      KboConfig cfg = {
        .weights    = weights,
        .precedence = prec,
        .n_labels   = max_label + 1,
        .var_weight = 1,
      };

      AtpState *atp = thvm_atp_init(&cfg, 256);
      CHECK(atp != NULL);
      for (u32 i = 0; i < spec->n_eqns; i++) {
        CHECK(thvm_atp_add_equation(atp,
                                    spec->eqn_lhs[i], spec->eqn_rhs[i]));
      }
      thvm_atp_set_goal(atp, spec->goal_lhs, spec->goal_rhs);

      AtpStatus st = thvm_atp_run(atp);
      // Either proved or budget exhausted; both are acceptable for
      // this structural check.  Crucially: must NOT have errored.
      CHECK(st == ATP_PROVED || st == ATP_TIMEOUT || st == ATP_QUEUE_EMPTY);

      // n_trace at minimum equals the number of axioms pushed.
      CHECK(atp->n_trace >= spec->n_eqns);

      // Serialize and inspect.
      static char buf[8192];
      u32 n = thvm_atp_trace_serialize(atp, buf, sizeof(buf));
      CHECK(n > 0u);
      CHECK_EQ((int)buf[n], 0);

      // The first three lines must be the three axioms in order.
      CHECK(strstr(buf, "0 (axiom): ") != NULL);
      CHECK(strstr(buf, "1 (axiom): ") != NULL);
      CHECK(strstr(buf, "2 (axiom): ") != NULL);
      // At least one orient line exists.
      CHECK(strstr(buf, "(orient from ") != NULL);

      thvm_atp_free(atp);
      wald_free(spec);
    }
  }

  thvm_free();
  TEST_REPORT();
}
