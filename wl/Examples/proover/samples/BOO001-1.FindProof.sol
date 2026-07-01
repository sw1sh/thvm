% SZS status Unsatisfiable for BOO001-1.p
% SZS output start CNFRefutation for BOO001-1.p
cnf(ax1, axiom, multiply(multiply(V,W,X),Y,multiply(V,W,Z)) = multiply(V,W,multiply(X,Y,Z))).
cnf(ax2, axiom, multiply(Y,X,X) = X).
cnf(ax3, axiom, multiply(X,X,Y) = X).
cnf(ax4, axiom, multiply(inverse(Y),Y,X) = X).
cnf(ax5, axiom, multiply(X,Y,inverse(Y)) = X).
cnf(negated_conjecture, negated_conjecture, inverse(inverse(a)) != a).
cnf(cpl1, plain, multiply(V,W,multiply(X,Y,W)) = multiply(multiply(V,W,X),Y,W), inference(sup, [status(thm)], [ax1, ax2])).
cnf(cpl2, plain, multiply(V,X,multiply(inverse(X),W,X)) = multiply(V,W,X), inference(sup, [status(thm)], [cpl1, ax5])).
cnf(cpl3, plain, multiply(V,inverse(W),W) = multiply(V,W,inverse(W)), inference(sup, [status(thm)], [cpl2, ax3])).
cnf(sl1, plain, multiply(V,inverse(W),W) = V, inference(rw, [status(thm)], [cpl3, ax5])).
cnf(cpl4, plain, inverse(inverse(V)) = V, inference(sup, [status(thm)], [sl1, ax4])).
cnf(contradiction, plain, $false, inference(rw, [status(thm)], [negated_conjecture, cpl4])).
% SZS output end CNFRefutation for BOO001-1.p
