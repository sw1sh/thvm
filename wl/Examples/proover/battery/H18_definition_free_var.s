% Proof    : Problems/DEF100+1.p
% A "definition" whose body has a variable the defined symbol does not
% take: ![X]: sF0 = f(X) forces f constant -- not a conservative
% extension, and it refutes the consistent axiom f(a) != f(b).
fof(a1, axiom, f(a) != f(b), file('Problems/DEF100+1.p',a1)).
fof(c, conjecture, q, file('Problems/DEF100+1.p',c)).
fof(d1, plain, ![X]: sF0 = f(X), introduced(definition, [new_symbols(definition, [sF0])], [function_definition])).
fof(s1, plain, sF0 = f(a), inference(instantiate, [status(thm)], [d1])).
fof(s2, plain, sF0 = f(b), inference(instantiate, [status(thm)], [d1])).
fof(s3, plain, f(a) = f(b), inference(superposition, [status(thm)], [s1, s2])).
fof(s4, plain, $false, inference(resolution, [status(thm)], [a1, s3])).
