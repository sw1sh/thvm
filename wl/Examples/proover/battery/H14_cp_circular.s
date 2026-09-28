% Proof    : Problems/EQC000+1.p
% Out-of-order equational records: e2 (file-first) cites e1, and e1's
% cp check may not use e2 as a demodulator -- e2 descends from e1.
fof(e2, plain, a = b, inference(orient, [status(thm)], [e1])).
fof(e1, plain, a = b, inference(cp, [status(thm)], [a1, a1])).
fof(a1, axiom, f(a) = b, file('Problems/EQC000+1.p',a1)).
fof(c, conjecture, a = b, file('Problems/EQC000+1.p',c)).
fof(s1, negated_conjecture, a != b, inference(negated_conjecture, [status(cth)], [c])).
fof(f1, plain, $false, inference(consequence, [status(thm)], [s1, e2])).
