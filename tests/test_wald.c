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

  thvm_free();
  TEST_REPORT();
}
