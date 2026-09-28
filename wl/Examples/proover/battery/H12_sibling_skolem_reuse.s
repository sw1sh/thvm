% Proof    : Problems/SIB000+1.p
% Two sibling esa records from ONE parent introduce the same Skolem
% FUNCTION for two different existentials: p(sk) & q(sk).
fof(a1, axiom, (?[X]: p(X)) & (?[X]: q(X)), file('Problems/SIB000+1.p',a1)).
fof(c, conjecture, ?[X]: (p(X) & q(X)), file('Problems/SIB000+1.p',c)).
fof(s1, negated_conjecture, ~ ?[X]: (p(X) & q(X)), inference(negated_conjecture, [status(cth)], [c])).
fof(k1, plain, p(sk), inference(cnf_transformation, [status(esa), new_symbols(skolem, [sk])], [a1])).
fof(k2, plain, q(sk), inference(cnf_transformation, [status(esa), new_symbols(skolem, [sk])], [a1])).
fof(f1, plain, $false, inference(consequence, [status(thm)], [s1, k1, k2])).
