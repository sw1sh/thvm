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

  thvm_free();
  TEST_REPORT();
}
