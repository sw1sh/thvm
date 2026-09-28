% Proof    : Problems/DEF000+1.p
% Two sibling "definition" clauses over a fresh PREDICATE d jointly
% smuggle q(a) out of p(a).  A predicate cannot be abstracted into a
% first-order witness variable; neither record may confirm.
fof(a1, axiom, p(a), file('Problems/DEF000+1.p',a1)).
fof(c, conjecture, q(a), file('Problems/DEF000+1.p',c)).
fof(s1, negated_conjecture, ~q(a), inference(negated_conjecture, [status(cth)], [c])).
fof(d1, plain, q(a) | ~d, inference(cnf_transformation, [status(esa), new_symbols(definition, [d])], [a1])).
fof(d2, plain, d, inference(cnf_transformation, [status(esa)], [a1])).
fof(s2, plain, q(a), inference(resolution, [status(thm)], [d1, d2])).
fof(f1, plain, $false, inference(resolution, [status(thm)], [s1, s2])).
