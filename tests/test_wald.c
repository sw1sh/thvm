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

  // === 6.4c structural cross-check vs Waldmeister's PCL ==============
  //
  // Waldmeister's PCL format (sources/INF/pcl.c):
  //   <id> : tes-eqn  : <lhs> = <rhs>  : initial             (axiom)
  //   <id> : tes-goal : <lhs> = <rhs>  : hypothesis          (goal)
  //   <id> : tes-rule : <lhs> -> <rhs> : orient(<src>,<dir>) (orient)
  //   <id> : tes-eqn  : <lhs> = <rhs>  : cp(<a>,<pa>,<b>,<pb>) (CP)
  //   <id> : tes-final: <lhs> = <rhs>  : <src>               (proof close)
  //
  // Our trace (src/atp/_.c):
  //   <id> (axiom): <lhs> = <rhs>
  //   <id> (orient from <src>): <lhs> = <rhs>
  //   <id> (cp from <a>, <b>): <lhs> = <rhs>
  //
  // Format differences (intentional, documented here):
  // - We don't render `tes-rule` with `->` distinct from `tes-eqn`;
  //   our pretty printer is `=`-only.  Stage 8.x can split.
  // - We don't render CP positions (<pa>, <pb>) -- generate_cps
  //   doesn't yet preserve overlap positions in the trace.
  // - We don't emit a `tes-final` line on proof close; the goal
  //   match is implicit in `thvm_atp_run` returning ATP_PROVED.
  //
  // What we DO match structurally:
  // - Monotonic ids starting at 0
  // - Axioms first (no parents), then derived (orient has 1 parent,
  //   cp has 2)
  // - Every parent id is < its child's id (DAG well-formed)
  TEST_BEGIN("wald/example.pr/pcl-dag-well-formed");
  {
    WaldSpec *spec = wald_init();
    WaldErr pe = wald_parse_file("waldmeister/documents/example.pr", spec);
    if (pe != WALD_OK) {
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
      for (u32 i = 0; i < spec->n_eqns; i++) {
        thvm_atp_add_equation(atp, spec->eqn_lhs[i], spec->eqn_rhs[i]);
      }
      thvm_atp_set_goal(atp, spec->goal_lhs, spec->goal_rhs);
      (void)thvm_atp_run(atp);

      // Walk atp->trace[] directly -- structurally rather than
      // text-parsing.  Each entry is a TAG_CTR with [p_a, p_b, lhs, rhs];
      // the ctr label is the TraceReason enum.
      //
      // Note: TRACE_AXIOM is pushed by every `thvm_atp_add_equation`
      // call, including the one that interreduce() makes when an
      // older rule's LHS simplifies and gets re-queued.  So we don't
      // require all axiom entries to come before any orient/cp.
      // Instead we just check: (1) the FIRST n_eqns trace entries
      // ARE the spec's axioms in order, with no parents; (2) every
      // orient/cp parent points strictly back into the trace
      // (DAG well-formedness, matching Waldmeister's PCL).
      CHECK(atp->n_trace >= spec->n_eqns);
      for (u32 i = 0; i < spec->n_eqns; i++) {
        Term entry  = atp->trace[i];
        u32  reason = term_ext(entry);
        u32  p_a    = (u32)term_val(term_ctr_at(entry, 0));
        u32  p_b    = (u32)term_val(term_ctr_at(entry, 1));
        CHECK_EQ(reason, TRACE_AXIOM);
        CHECK_EQ(p_a, ATP_TRACE_NONE);
        CHECK_EQ(p_b, ATP_TRACE_NONE);
      }

      u32 n_axiom = spec->n_eqns;
      u32 n_orient = 0, n_cp = 0;
      for (u32 i = spec->n_eqns; i < atp->n_trace; i++) {
        Term entry  = atp->trace[i];
        u32  reason = term_ext(entry);
        u32  p_a    = (u32)term_val(term_ctr_at(entry, 0));
        u32  p_b    = (u32)term_val(term_ctr_at(entry, 1));
        switch (reason) {
          case TRACE_AXIOM:
            // Re-queued via interreduction.  No parents recorded.
            CHECK_EQ(p_a, ATP_TRACE_NONE);
            CHECK_EQ(p_b, ATP_TRACE_NONE);
            n_axiom++;
            break;
          case TRACE_ORIENT:
            CHECK(p_a != ATP_TRACE_NONE);
            CHECK(p_a < i);
            n_orient++;
            break;
          case TRACE_CP:
            CHECK(p_a != ATP_TRACE_NONE);
            CHECK(p_b != ATP_TRACE_NONE);
            CHECK(p_a < i);
            CHECK(p_b < i);
            n_cp++;
            break;
          default:
            CHECK_EQ(reason, TRACE_AXIOM);   // unknown reason
            break;
        }
      }
      // At least one orient (tes-rule / orient(...) in PCL).
      CHECK(n_orient >= 1u);
      // Sanity: total accounting matches.
      CHECK_EQ(n_axiom + n_orient + n_cp, atp->n_trace);

      thvm_atp_free(atp);
      wald_free(spec);
    }
  }

  // === Stage 8.4b: multi-sort metadata ================================

  TEST_BEGIN("wald/sorts/empty-spec-has-no-sorts");
  {
    WaldSpec *spec = wald_init();
    CHECK_EQ(spec->n_sorts, 0u);
    wald_free(spec);
  }

  TEST_BEGIN("wald/sorts/register-and-lookup");
  {
    WaldSpec *spec = wald_init();
    u32 nat_id  = wald_sort_id_or_register(spec, "nat",  3);
    u32 list_id = wald_sort_id_or_register(spec, "list", 4);
    u32 nat_again = wald_sort_id_or_register(spec, "nat", 3);
    CHECK_EQ(nat_id, 0u);
    CHECK_EQ(list_id, 1u);
    CHECK_EQ(nat_again, 0u);                     // idempotent lookup
    CHECK_EQ(spec->n_sorts, 2u);
    CHECK_EQ((int)strcmp(spec->sorts[0], "nat"), 0);
    CHECK_EQ((int)strcmp(spec->sorts[1], "list"), 0);
    wald_free(spec);
  }

  TEST_BEGIN("wald/sort-check/homogeneous-mode-passes-everything");
  {
    // n_sorts == 0 means homogeneous mode; all terms pass.
    WaldSpec *spec = wald_init();
    Term t = term_new_ctr(7u, NULL, 0);
    CHECK_EQ((int)wald_term_sort(spec, t), 0);
    CHECK_EQ((int)wald_sort_check(spec, t), 1);
    // FVR also passes.
    Term v = term_new_fvr(0u);
    CHECK_EQ((int)wald_term_sort(spec, v), 0);
    CHECK_EQ((int)wald_sort_check(spec, v), 1);
    wald_free(spec);
  }

  TEST_BEGIN("wald/sort-check/well-sorted-multi-sort");
  {
    // nat / list signature; build cons(zero, nil) and verify it
    // returns sort `list` (id 1).
    static const char *src =
      "NAME            x\n"
      "MODE            PROOF\n"
      "SORTS           nat list\n"
      "SIGNATURE       zero: -> nat\n"
      "                nil: -> list\n"
      "                cons: nat list -> list\n"
      "VARIABLES       n : nat\n"
      "EQUATIONS       cons(zero, nil) = cons(zero, nil)\n"
      "CONCLUSION      zero = zero\n";
    WaldSpec *spec = wald_init();
    CHECK_EQ((int)wald_parse(src, spec), (int)WALD_OK);

    // Reconstruct cons(zero, nil) directly via labels.
    u32 lab_zero = spec->symbols[0].label;
    u32 lab_nil  = spec->symbols[1].label;
    u32 lab_cons = spec->symbols[2].label;
    Term zero = term_new_ctr(lab_zero, NULL, 0);
    Term nil  = term_new_ctr(lab_nil,  NULL, 0);
    Term cons_zero_nil =
      term_new_ctr(lab_cons, (Term[]){zero, nil}, 2);

    CHECK_EQ((int)wald_term_sort(spec, zero),           0); // nat
    CHECK_EQ((int)wald_term_sort(spec, nil),            1); // list
    CHECK_EQ((int)wald_term_sort(spec, cons_zero_nil),  1); // list
    CHECK_EQ((int)wald_sort_check(spec, cons_zero_nil), 1);

    // FVR n has var_id 0 and sort nat (id 0).
    Term v_n = term_new_fvr(0u);
    CHECK_EQ((int)wald_term_sort(spec, v_n), 0);
    wald_free(spec);
  }

  TEST_BEGIN("wald/sort-check/sort-mismatch-detected");
  {
    // Same nat/list signature; build cons(nil, zero) -- arg 0
    // expects nat but gets list.  Must report sort mismatch.
    static const char *src =
      "NAME            x\n"
      "MODE            PROOF\n"
      "SORTS           nat list\n"
      "SIGNATURE       zero: -> nat\n"
      "                nil: -> list\n"
      "                cons: nat list -> list\n"
      "VARIABLES       n : nat\n"
      "EQUATIONS       zero = zero\n"
      "CONCLUSION      zero = zero\n";
    WaldSpec *spec = wald_init();
    CHECK_EQ((int)wald_parse(src, spec), (int)WALD_OK);

    u32 lab_zero = spec->symbols[0].label;
    u32 lab_nil  = spec->symbols[1].label;
    u32 lab_cons = spec->symbols[2].label;
    Term zero = term_new_ctr(lab_zero, NULL, 0);
    Term nil  = term_new_ctr(lab_nil,  NULL, 0);
    // cons(nil, zero): args are swapped from the signature.
    Term ill = term_new_ctr(lab_cons, (Term[]){nil, zero}, 2);

    CHECK_EQ((int)wald_term_sort(spec, ill), (int)WALD_MAX_SORTS);
    CHECK_EQ((int)wald_sort_check(spec, ill), 0);
    wald_free(spec);
  }

  TEST_BEGIN("wald/sort-check/unknown-symbol-fails");
  {
    // CTR with a label that isn't in the symbol table.
    WaldSpec *spec = wald_init();
    wald_sort_id_or_register(spec, "any", 3);  // n_sorts > 0
    Term t = term_new_ctr(99u, NULL, 0);   // label 99: not registered
    CHECK_EQ((int)wald_sort_check(spec, t), 0);
    wald_free(spec);
  }

  TEST_BEGIN("wald/sort-check/unknown-fvr-fails");
  {
    // FVR with a var_id that isn't in the var table.
    WaldSpec *spec = wald_init();
    wald_sort_id_or_register(spec, "any", 3);  // n_sorts > 0
    Term v = term_new_fvr(99u);            // var_id 99: not registered
    CHECK_EQ((int)wald_sort_check(spec, v), 0);
    wald_free(spec);
  }

  // === Stage 8.4d: sort-check gating in saturation entry points ======

  TEST_BEGIN("wald/sort-gate/add-equation-rejects-mismatch");
  {
    static const char *src =
      "NAME            x\n"
      "MODE            PROOF\n"
      "SORTS           nat list\n"
      "SIGNATURE       zero: -> nat\n"
      "                nil: -> list\n"
      "                cons: nat list -> list\n"
      "VARIABLES       n : nat\n"
      "EQUATIONS       zero = zero\n"
      "CONCLUSION      zero = zero\n";
    WaldSpec *spec = wald_init();
    CHECK_EQ((int)wald_parse(src, spec), (int)WALD_OK);

    static const KboConfig CFG = {
      .weights = NULL, .precedence = NULL,
      .n_labels = 0, .var_weight = 1,
    };
    AtpState *atp = thvm_atp_init(&CFG, 32);
    thvm_atp_set_spec(atp, spec);

    // Well-sorted equation: zero = zero -- both sides are nat.
    u32 lab_zero = spec->symbols[0].label;
    Term zero1 = term_new_ctr(lab_zero, NULL, 0);
    Term zero2 = term_new_ctr(lab_zero, NULL, 0);
    CHECK_EQ((int)thvm_atp_add_equation(atp, zero1, zero2), 1);

    // Ill-sorted equation: zero = nil -- nat vs list.
    u32 lab_nil = spec->symbols[1].label;
    Term nil = term_new_ctr(lab_nil, NULL, 0);
    Term zero3 = term_new_ctr(lab_zero, NULL, 0);
    u32 cps_before = atp->n_cps;
    CHECK_EQ((int)thvm_atp_add_equation(atp, zero3, nil), 0);
    CHECK_EQ(atp->n_cps, cps_before);   // state unchanged

    thvm_atp_free(atp);
    wald_free(spec);
  }

  TEST_BEGIN("wald/sort-gate/set-goal-rejects-mismatch");
  {
    static const char *src =
      "NAME            x\n"
      "MODE            PROOF\n"
      "SORTS           nat list\n"
      "SIGNATURE       zero: -> nat\n"
      "                nil: -> list\n"
      "                cons: nat list -> list\n"
      "EQUATIONS       zero = zero\n"
      "CONCLUSION      zero = zero\n";
    WaldSpec *spec = wald_init();
    CHECK_EQ((int)wald_parse(src, spec), (int)WALD_OK);

    static const KboConfig CFG = {
      .weights = NULL, .precedence = NULL,
      .n_labels = 0, .var_weight = 1,
    };
    AtpState *atp = thvm_atp_init(&CFG, 32);
    thvm_atp_set_spec(atp, spec);

    u32 lab_zero = spec->symbols[0].label;
    u32 lab_nil  = spec->symbols[1].label;

    // Well-sorted goal accepted.
    Term z1 = term_new_ctr(lab_zero, NULL, 0);
    Term z2 = term_new_ctr(lab_zero, NULL, 0);
    CHECK_EQ((int)thvm_atp_set_goal(atp, z1, z2), 1);
    CHECK(atp->goal_lhs != 0u);

    // Ill-sorted goal rejected; previous goal preserved.
    Term zero  = term_new_ctr(lab_zero, NULL, 0);
    Term nil   = term_new_ctr(lab_nil,  NULL, 0);
    Term prev_lhs = atp->goal_lhs;
    Term prev_rhs = atp->goal_rhs;
    CHECK_EQ((int)thvm_atp_set_goal(atp, zero, nil), 0);
    CHECK_EQ(atp->goal_lhs, prev_lhs);
    CHECK_EQ(atp->goal_rhs, prev_rhs);

    // Clearing the goal (lhs == 0) is always accepted.
    CHECK_EQ((int)thvm_atp_set_goal(atp, 0, 0), 1);
    CHECK_EQ(atp->goal_lhs, 0u);

    thvm_atp_free(atp);
    wald_free(spec);
  }

  TEST_BEGIN("wald/sort-gate/no-spec-attached-passes-everything");
  {
    // Without a spec attached, the gate is a no-op even on
    // ill-shaped terms.  Confirms backward compatibility for
    // tests that use thvm_atp_init without a WaldSpec.
    static const KboConfig CFG = {
      .weights = NULL, .precedence = NULL,
      .n_labels = 0, .var_weight = 1,
    };
    AtpState *atp = thvm_atp_init(&CFG, 32);
    CHECK(atp->spec == NULL);

    Term lhs = term_new_ctr(99u, NULL, 0);   // unregistered label
    Term rhs = term_new_fvr(99u);            // unregistered var_id
    CHECK_EQ((int)thvm_atp_add_equation(atp, lhs, rhs), 1);
    CHECK_EQ((int)thvm_atp_set_goal(atp, lhs, rhs), 1);

    thvm_atp_free(atp);
  }

  TEST_BEGIN("wald/sorts/multi-sort-pr-fixture");
  {
    // A small sorted-list fragment: nat / list sorts.
    static const char *src =
      "NAME            nat_list\n"
      "MODE            PROOF\n"
      "SORTS           nat list\n"
      "SIGNATURE       zero: -> nat\n"
      "                succ: nat -> nat\n"
      "                nil: -> list\n"
      "                cons: nat list -> list\n"
      "VARIABLES       n,m : nat\n"
      "                xs : list\n"
      "EQUATIONS       cons(n, nil) = cons(n, nil)\n"
      "CONCLUSION      cons(zero, nil) = cons(zero, nil)\n";

    WaldSpec *spec = wald_init();
    CHECK_EQ((int)wald_parse(src, spec), (int)WALD_OK);

    // Sort table: nat (0), list (1).
    CHECK_EQ(spec->n_sorts, 2u);
    CHECK_EQ((int)strcmp(spec->sorts[0], "nat"), 0);
    CHECK_EQ((int)strcmp(spec->sorts[1], "list"), 0);

    // Symbol metadata: zero, succ, nil, cons.
    CHECK_EQ(spec->n_symbols, 4u);
    // zero: -> nat
    CHECK_EQ((int)strcmp(spec->symbols[0].name, "zero"), 0);
    CHECK_EQ(spec->symbols[0].arity, 0u);
    CHECK_EQ(spec->symbols[0].result_sort, 0u);   // nat
    // succ: nat -> nat
    CHECK_EQ((int)strcmp(spec->symbols[1].name, "succ"), 0);
    CHECK_EQ(spec->symbols[1].arity, 1u);
    CHECK_EQ(spec->symbols[1].arg_sorts[0], 0u);  // nat
    CHECK_EQ(spec->symbols[1].result_sort, 0u);   // nat
    // nil: -> list
    CHECK_EQ((int)strcmp(spec->symbols[2].name, "nil"), 0);
    CHECK_EQ(spec->symbols[2].arity, 0u);
    CHECK_EQ(spec->symbols[2].result_sort, 1u);   // list
    // cons: nat list -> list
    CHECK_EQ((int)strcmp(spec->symbols[3].name, "cons"), 0);
    CHECK_EQ(spec->symbols[3].arity, 2u);
    CHECK_EQ(spec->symbols[3].arg_sorts[0], 0u);  // nat
    CHECK_EQ(spec->symbols[3].arg_sorts[1], 1u);  // list
    CHECK_EQ(spec->symbols[3].result_sort, 1u);   // list

    // Variable metadata: n, m, xs.
    CHECK_EQ(spec->n_vars, 3u);
    CHECK_EQ((int)strcmp(spec->vars[0].name, "n"),  0);
    CHECK_EQ(spec->vars[0].sort, 0u);   // nat
    CHECK_EQ((int)strcmp(spec->vars[1].name, "m"),  0);
    CHECK_EQ(spec->vars[1].sort, 0u);   // nat (batch comma-share)
    CHECK_EQ((int)strcmp(spec->vars[2].name, "xs"), 0);
    CHECK_EQ(spec->vars[2].sort, 1u);   // list

    wald_free(spec);
  }

  thvm_free();
  TEST_REPORT();
}
