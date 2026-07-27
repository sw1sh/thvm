% Proof    : Problems/LIE000+1.p
fof(a1, axiom, p(a), file('Problems/LIE000+1.p',a1)).
fof(c, conjecture, q(b), file('Problems/LIE000+1.p',c)).
fof(s1, negated_conjecture, ~q(b), inference(negated_conjecture, [status(cth)], [c])).
fof(x1, plain, q(b), file('Problems/LIE000+1.p',a1)).
fof(f1, plain, $false, inference(consequence, [status(thm)], [s1, x1])).
