% SZS status Unsatisfiable for BOO001-1.p
% SZS output start CNFRefutation for BOO001-1.p
cnf(associativity, axiom, multiply(multiply(V,W,X),Y,multiply(V,W,Z)) = multiply(V,W,multiply(X,Y,Z))).
cnf(ternary_multiply_1, axiom, multiply(Y,X,X) = X).
cnf(ternary_multiply_2, axiom, multiply(X,X,Y) = X).
cnf(left_inverse, axiom, multiply(inverse(Y),Y,X) = X).
cnf(right_inverse, axiom, multiply(X,Y,inverse(Y)) = X).
cnf(prove_inverse_is_self_cancelling, negated_conjecture, inverse(inverse(a)) != a).
cnf(cpl1, plain, multiply(V,W,multiply(X,Y,W)) = multiply(multiply(V,W,X),Y,W), inference(sup, [status(thm)], [associativity, ternary_multiply_1])).
cnf(cpl2, plain, multiply(V,X,multiply(inverse(X),W,X)) = multiply(V,W,X), inference(sup, [status(thm)], [cpl1, right_inverse])).
cnf(cpl3, plain, multiply(V,inverse(W),W) = multiply(V,W,inverse(W)), inference(sup, [status(thm)], [cpl2, ternary_multiply_2])).
cnf(sl1, plain, multiply(V,inverse(W),W) = V, inference(rw, [status(thm)], [cpl3, right_inverse])).
cnf(cpl4, plain, inverse(inverse(V)) = V, inference(sup, [status(thm)], [sl1, left_inverse])).
cnf(contradiction, plain, $false, inference(rw, [status(thm)], [prove_inverse_is_self_cancelling, cpl4])).
% SZS output end CNFRefutation for BOO001-1.p