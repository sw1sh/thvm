% SZS status Unsatisfiable for BOO001-1.p
% SZS output start CNFRefutation for BOO001-1.p
cnf(t4, axiom, multiply(multiply(X1,X2,X3),X4,multiply(X1,X2,X5)) = multiply(X1,X2,multiply(X3,X4,X5)), file('BOO001-1.p')).
cnf(t9, plain, multiply(multiply(X1,X2,X3),X4,multiply(X1,X2,X5)) = multiply(X1,X2,multiply(X3,X4,X5)), inference(orient, [status(thm)], [t4])).
cnf(t1, axiom, multiply(X1,X2,X2) = X2, file('BOO001-1.p')).
cnf(t5, plain, multiply(X1,X2,X2) = X2, inference(orient, [status(thm)], [t1])).
cnf(t15, plain, multiply(X1,X2,multiply(X3,X4,X2)) = multiply(multiply(X1,X2,X3),X4,X2), inference(cp, [status(thm)], [t9, t5])).
cnf(t23, plain, multiply(multiply(X1,X2,X3),X4,X2) = multiply(X1,X2,multiply(X3,X4,X2)), inference(orient, [status(thm)], [t15])).
cnf(t2, axiom, multiply(X1,X2,inverse(X2)) = X1, file('BOO001-1.p')).
cnf(t6, plain, multiply(X1,X2,inverse(X2)) = X1, inference(orient, [status(thm)], [t2])).
cnf(t26, plain, multiply(X1,X2,multiply(inverse(X2),X3,X2)) = multiply(X1,X3,X2), inference(cp, [status(thm)], [t23, t6])).
cnf(t36, plain, multiply(X1,X2,multiply(inverse(X2),X3,X2)) = multiply(X1,X3,X2), inference(orient, [status(thm)], [t26])).
cnf(t0, axiom, multiply(X1,X1,X2) = X1, file('BOO001-1.p')).
cnf(t7, plain, multiply(X1,X1,X2) = X1, inference(orient, [status(thm)], [t0])).
cnf(t40, plain, multiply(X1,inverse(X2),X2) = multiply(X1,X2,inverse(X2)), inference(cp, [status(thm)], [t36, t7])).
cnf(t57, plain, multiply(X1,inverse(X2),X2) = X1, inference(step, [status(thm)], [t40, t6])).
cnf(t46, plain, multiply(X1,inverse(X2),X2) = X1, inference(orient, [status(thm)], [t57])).
cnf(t3, axiom, multiply(inverse(X1),X1,X2) = X2, file('BOO001-1.p')).
cnf(t8, plain, multiply(inverse(X1),X1,X2) = X2, inference(orient, [status(thm)], [t3])).
cnf(t48, plain, inverse(inverse(X1)) = X1, inference(cp, [status(thm)], [t46, t8])).
cnf(t52, plain, inverse(inverse(X1)) = X1, inference(orient, [status(thm)], [t48])).
cnf(goal_0, negated_conjecture, inverse(inverse(a)) != a, file('BOO001-1.p')).
cnf(g0_0, plain, a != a, inference(rw, [status(thm)], [goal_0, t52])).
cnf(contradiction_0, plain, $false, inference(trivial_inequality_removal, [status(thm)], [g0_0])).
% SZS output end CNFRefutation for BOO001-1.p
