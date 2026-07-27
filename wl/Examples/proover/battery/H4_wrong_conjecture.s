% Proof    : Problems/LIE000+1.p
fof(a1, axiom, p(a), file('Problems/LIE000+1.p',a1)).
fof(c, conjecture, p(a), file('Problems/LIE000+1.p',c)).
fof(s1, negated_conjecture, ~p(a), inference(negated_conjecture, [status(cth)], [c])).
fof(f1, plain, $false, inference(consequence, [status(thm)], [s1, a1])).
