% Proof    : Problems/WID000+1.p
% INVALID: the axioms have the two-element model a != b, so the closing
% step does not follow.  Its clause set is non-Horn (X = a | X = b), and
% only through the curried encoding of the 17-ary f (four distinct
% partial applications, two elements) is it unsatisfiable.
fof(domain, axiom, ![X] : (X = a | X = b), file('Problems/WID000+1.p',domain)).
fof(r_aa, axiom, ![Z,W1,W2,W3,W4,W5,W6,W7,W8,W9,W10,W11,W12,W13,W14] : f(a,a,Z,W1,W2,W3,W4,W5,W6,W7,W8,W9,W10,W11,W12,W13,W14) = Z, file('Problems/WID000+1.p',r_aa)).
fof(r_ab_a, axiom, ![W1,W2,W3,W4,W5,W6,W7,W8,W9,W10,W11,W12,W13,W14] : f(a,b,a,W1,W2,W3,W4,W5,W6,W7,W8,W9,W10,W11,W12,W13,W14) = b, file('Problems/WID000+1.p',r_ab_a)).
fof(r_ab_b, axiom, ![W1,W2,W3,W4,W5,W6,W7,W8,W9,W10,W11,W12,W13,W14] : f(a,b,b,W1,W2,W3,W4,W5,W6,W7,W8,W9,W10,W11,W12,W13,W14) = a, file('Problems/WID000+1.p',r_ab_b)).
fof(r_ba, axiom, ![Z,W1,W2,W3,W4,W5,W6,W7,W8,W9,W10,W11,W12,W13,W14] : f(b,a,Z,W1,W2,W3,W4,W5,W6,W7,W8,W9,W10,W11,W12,W13,W14) = a, file('Problems/WID000+1.p',r_ba)).
fof(r_bb, axiom, ![Z,W1,W2,W3,W4,W5,W6,W7,W8,W9,W10,W11,W12,W13,W14] : f(b,b,Z,W1,W2,W3,W4,W5,W6,W7,W8,W9,W10,W11,W12,W13,W14) = b, file('Problems/WID000+1.p',r_bb)).
fof(c, conjecture, a = b, file('Problems/WID000+1.p',c)).
fof(nc, negated_conjecture, a != b, inference(negated_conjecture, [status(cth)], [c])).
fof(f1, plain, $false, inference(pigeonhole, [status(thm)], [domain,r_aa,r_ab_a,r_ab_b,r_ba,r_bb,nc])).
