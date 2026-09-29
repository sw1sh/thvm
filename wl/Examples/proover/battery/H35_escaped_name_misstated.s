% Proof    : Problems/ESC000+1.p
fof(l1, axiom, q(a), file('Problems/ESC000+1.p','a\'1')).
fof(c, conjecture, q(a), file('Problems/ESC000+1.p',c)).
fof(s1, negated_conjecture, ~q(a), inference(negated_conjecture, [status(cth)], [c])).
fof(f1, plain, $false, inference(consequence, [status(thm)], [s1, l1])).
