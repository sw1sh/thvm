% wide_skolem17_pp: Satisfiable (s = {a}, the Skolem witness of Y the
% value of X1, a != b).  Y's Skolem term takes all 17 universals, a
% node wider than the unifier and renamer handle; the non-Horn clause
% ~r(X1) | s(sk(X1..X17)) | w sends the set to the clausal pre-pass,
% where saturating it kept X1 unbound through each resolution and
% refuted the set.
fof(ax, axiom, ![X1,X2,X3,X4,X5,X6,X7,X8,X9,X10,X11,X12,X13,X14,X15,X16,X17] : ?[Y] : ((r(X1) => (s(Y) | w)) & (t(X1) => ~s(Y)))).
fof(nw, axiom, ~w).
fof(ra, axiom, r(a)).
fof(tb, axiom, t(b)).
fof(c0, conjecture, a = b).
