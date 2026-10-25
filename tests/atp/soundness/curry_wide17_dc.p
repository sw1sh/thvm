% curry_wide17_dc: Satisfiable (a != b in the two-element model).  The
% domain is {a, b} (X = a | X = b) and the four binary residuals
% f(x, y, ...) of the 17-ary f are pairwise distinct, so no two-element
% model hosts the partial applications of f curried into a binary
% application symbol: reasoning over the curried form of this non-Horn
% set refutes it.  f is wider than a term node (16): the FOF front end
% gives up on it rather than read it through CheckProof's encoding (its
% clausal pre-pass would saturate the curried clauses).
cnf(domain, axiom, X = a | X = b).
cnf(r_aa, axiom, f(a,a,Z,W1,W2,W3,W4,W5,W6,W7,W8,W9,W10,W11,W12,W13,W14) = Z).
cnf(r_ab_a, axiom, f(a,b,a,W1,W2,W3,W4,W5,W6,W7,W8,W9,W10,W11,W12,W13,W14) = b).
cnf(r_ab_b, axiom, f(a,b,b,W1,W2,W3,W4,W5,W6,W7,W8,W9,W10,W11,W12,W13,W14) = a).
cnf(r_ba, axiom, f(b,a,Z,W1,W2,W3,W4,W5,W6,W7,W8,W9,W10,W11,W12,W13,W14) = a).
cnf(r_bb, axiom, f(b,b,Z,W1,W2,W3,W4,W5,W6,W7,W8,W9,W10,W11,W12,W13,W14) = b).
cnf(goal, negated_conjecture, a != b).
