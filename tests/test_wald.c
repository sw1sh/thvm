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

  thvm_free();
  TEST_REPORT();
}
