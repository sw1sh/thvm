% SZS status Theorem for SEU140+2.p
% SZS output start Proof for SEU140+2.p
fof(f10, axiom, ! [A,B] : ( disjoint(A,B) <=> set_intersection2(A,B) = empty_set ), file('SEU140+2.p', d7_xboole_0)).
fof(f10_nnf, plain, ! [A,B] : (((~(disjoint(A,B)) | set_intersection2(A,B) = empty_set) & (disjoint(A,B) | set_intersection2(A,B) != empty_set))), inference(nnf_transformation, [status(thm)], [f10])).
fof(f10_sk, plain, ! [A,B] : (((~(disjoint(A,B)) | set_intersection2(A,B) = empty_set) & (disjoint(A,B) | set_intersection2(A,B) != empty_set))), inference(skolemisation, [status(esa)], [f10_nnf])).
cnf(c39, plain, ~disjoint(X0,X1) | set_intersection2(X0,X1) = empty_set, inference(cnf_transformation, [status(esa)], [f10_sk])).
fof(f50, conjecture, ! [A,B,C] : ( ( subset(A,B) & disjoint(B,C) ) => disjoint(A,C) ), file('SEU140+2.p', t63_xboole_1)).
fof(f50_neg, negated_conjecture, ~(! [A,B,C] : ( ( subset(A,B) & disjoint(B,C) ) => disjoint(A,C) )), inference(negated_conjecture, [status(cth)], [f50])).
fof(f50_nnf, plain, ? [A,B,C] : (((subset(A,B) & disjoint(B,C)) & ~(disjoint(A,C)))), inference(nnf_transformation, [status(thm)], [f50_neg])).
fof(f50_sk, plain, ((subset(sk10,sk11) & disjoint(sk11,sk12)) & ~(disjoint(sk10,sk12))), inference(skolemisation, [status(esa), new_symbols(skolem, [sk10,sk11,sk12])], [f50_nnf])).
cnf(c91, plain, disjoint(sk11,sk12), inference(cnf_transformation, [status(esa)], [f50_sk])).
cnf(p147, plain, set_intersection2(sk11,sk12) = empty_set, inference(resolution, [status(thm)], [c39, c91])).
fof(f32, lemma, ! [A,B,C] : ( subset(A,B) => subset(set_intersection2(A,C),set_intersection2(B,C)) ), file('SEU140+2.p', t26_xboole_1)).
fof(f32_nnf, plain, ! [A,B,C] : ((~(subset(A,B)) | subset(set_intersection2(A,C),set_intersection2(B,C)))), inference(nnf_transformation, [status(thm)], [f32])).
fof(f32_sk, plain, ! [A,B,C] : ((~(subset(A,B)) | subset(set_intersection2(A,C),set_intersection2(B,C)))), inference(skolemisation, [status(esa)], [f32_nnf])).
cnf(c65, plain, ~subset(X0,X1) | subset(set_intersection2(X0,X2),set_intersection2(X1,X2)), inference(cnf_transformation, [status(esa)], [f32_sk])).
cnf(c90, plain, subset(sk10,sk11), inference(cnf_transformation, [status(esa)], [f50_sk])).
cnf(p409, plain, subset(set_intersection2(sk10,X0),set_intersection2(sk11,X0)), inference(resolution, [status(thm)], [c65, c90])).
cnf(p472, plain, subset(set_intersection2(sk10,sk12),empty_set), inference(superposition, [status(thm)], [p147, p409])).
fof(f43, lemma, ! [A] : ( subset(A,empty_set) => A = empty_set ), file('SEU140+2.p', t3_xboole_1)).
fof(f43_nnf, plain, ! [A] : ((~(subset(A,empty_set)) | A = empty_set)), inference(nnf_transformation, [status(thm)], [f43])).
fof(f43_sk, plain, ! [A] : ((~(subset(A,empty_set)) | A = empty_set)), inference(skolemisation, [status(esa)], [f43_nnf])).
cnf(c82, plain, ~subset(X0,empty_set) | X0 = empty_set, inference(cnf_transformation, [status(esa)], [f43_sk])).
cnf(p482, plain, set_intersection2(sk10,sk12) = empty_set, inference(resolution, [status(thm)], [p472, c82])).
cnf(c40, plain, disjoint(X0,X1) | set_intersection2(X0,X1) != empty_set, inference(cnf_transformation, [status(esa)], [f10_sk])).
cnf(p483, plain, disjoint(sk10,sk12), inference(resolution, [status(thm)], [p482, c40])).
cnf(c92, plain, ~disjoint(sk10,sk12), inference(cnf_transformation, [status(esa)], [f50_sk])).
cnf(p495, plain, $false, inference(resolution, [status(thm)], [p483, c92])).
% SZS output end Proof for SEU140+2.p
