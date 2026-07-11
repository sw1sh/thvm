% SZS status Unsatisfiable for BOO001-1.p
% SZS output start CNFRefutation for BOO001-1.p
cnf(t0, axiom, multiply(multiply(V,W,X),Y,multiply(V,W,Z)) = multiply(V,W,multiply(X,Y,Z))).
cnf(t8, plain, multiply(multiply(V,W,X),Y,multiply(V,W,Z)) = multiply(V,W,multiply(X,Y,Z)), inference(orient, [status(thm)], [t0])).
cnf(t1, axiom, multiply(V,W,W) = W).
cnf(t5, plain, multiply(V,W,W) = W, inference(orient, [status(thm)], [t1])).
cnf(t13, plain, multiply(V,W,multiply(X,Y,W)) = multiply(multiply(V,W,X),Y,W), inference(cp, [status(thm)], [t8, t5])).
cnf(t23, plain, multiply(multiply(V,W,X),Y,W) = multiply(V,W,multiply(X,Y,W)), inference(orient, [status(thm)], [t13])).
cnf(t4, axiom, multiply(V,W,inverse(W)) = V).
cnf(t17, plain, multiply(V,W,inverse(W)) = V, inference(orient, [status(thm)], [t4])).
cnf(t26, plain, multiply(V,W,multiply(inverse(W),X,W)) = multiply(V,X,W), inference(cp, [status(thm)], [t23, t17])).
cnf(t36, plain, multiply(V,W,multiply(inverse(W),X,W)) = multiply(V,X,W), inference(orient, [status(thm)], [t26])).
cnf(t2, axiom, multiply(V,V,W) = V).
cnf(t7, plain, multiply(V,V,W) = V, inference(orient, [status(thm)], [t2])).
cnf(t40, plain, multiply(V,inverse(W),W) = multiply(V,W,inverse(W)), inference(cp, [status(thm)], [t36, t7])).
cnf(t57, plain, multiply(V,inverse(W),W) = V, inference(step, [status(thm)], [t40, t17])).
cnf(t46, plain, multiply(V,inverse(W),W) = V, inference(orient, [status(thm)], [t57])).
cnf(t3, axiom, multiply(inverse(V),V,W) = W).
cnf(t6, plain, multiply(inverse(V),V,W) = W, inference(orient, [status(thm)], [t3])).
cnf(t48, plain, inverse(inverse(V)) = V, inference(cp, [status(thm)], [t46, t6])).
cnf(t52, plain, inverse(inverse(V)) = V, inference(orient, [status(thm)], [t48])).
cnf(goal_0, negated_conjecture, inverse(inverse(a)) != a).
cnf(g0_0, plain, a != a, inference(rw, [status(thm)], [goal_0, t52])).
cnf(contradiction_0, plain, $false, inference(trivial_inequality_removal, [status(thm)], [g0_0])).
% SZS output end CNFRefutation for BOO001-1.p
