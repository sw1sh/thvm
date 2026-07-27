% Proof    : Problems/SKO000+1.p
fof(a1, axiom, ?[X] : p(X), file('Problems/SKO000+1.p',a1)).
fof(c, conjecture, ?[Y] : p(Y), file('Problems/SKO000+1.p',c)).
fof(s1, negated_conjecture, ![Y] : ~p(Y), inference(negated_conjecture, [status(cth)], [c])).
fof(s2, plain, p(sk0), inference(cnf_transformation, [status(esa), new_symbols(skolem,[sk0])], [a1])).
fof(f1, plain, $false, inference(resolution, [status(thm)], [s1, s2])).
