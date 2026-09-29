% Proof    : Problems/WID004+1.p
% INVALID: the axioms hold with s = {a} and the Skolem witness of Y the
% value of X1.  Clausified with Y a Skolem term over all 17 universals,
% a node the unifier and renamer passed through whole, resolving
% s(sk(X1..X17)) | ~r(X1) with r(a) kept s(sk(X1..X17)) whole, and with
% its twin from t(b) closed a refutation (VerifiedGood).  The witness
% takes the universals its body mentions (sk(X1)), and the saturation
% finds the countermodel.
fof(ax, axiom, ![X1,X2,X3,X4,X5,X6,X7,X8,X9,X10,X11,X12,X13,X14,X15,X16,X17] : ?[Y] : ((r(X1) => s(Y)) & (t(X1) => ~s(Y))), file('Problems/WID004+1.p',ax)).
fof(ra, axiom, r(a), file('Problems/WID004+1.p',ra)).
fof(tb, axiom, t(b), file('Problems/WID004+1.p',tb)).
fof(c0, conjecture, a = b, file('Problems/WID004+1.p',c0)).
fof(f1, plain, $false, inference(resolution, [status(thm)], [ax, ra, tb])).
