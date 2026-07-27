% Proof    : Problems/COR900+1.p
fof(a1, axiom, p(a), file('Problems/COR900+1.p',a1)).
fof(c, conjecture, p(a), file('Problems/COR900+1.p',c)).
fof(bad, plain, p(a) = "distinct object", inference(magic, [status(thm)], [])).
fof(s1, negated_conjecture, ~p(a), inference(negated_conjecture, [status(cth)], [c])).
fof(f1, plain, $false, inference(consequence, [status(thm)], [s1, a1])).
