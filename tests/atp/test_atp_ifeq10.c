// --emit-ifeq10 (tools/atp_cli/ifeq10.c) against TPTP's own -10 files.
// Each of the ten TPTP cases is a TPTP v9.2.1 problem with its include()s
// inlined in place and the cnf records of the -10 version TPTP ships; the
// encoder's output must equal them record for record, in order, once
// comments and whitespace are dropped.  The pairs cover one to three
// dispatchers, equational conditions and heads, mixed condition types
// (CAT010), X != Y elimination (CAT010), Skolem functions and constants,
// clause splitting, equivalences, the hypothesis role, the a != b goal
// funnel, tuple goals in an axiom (ALG069) and a conjecture (LCL423), and
// the lazily interned symbol order (PLA023, SEU323).  No case shares a
// name stem with a CASC-J13 problem; tests/atp/atp_ifeq10_tptp.sh runs the
// same ten, and SYN143-1 (too big to inline), through FindProof against a
// TPTP tree.

// mkdtemp and unlink: on glibc a strict -std=c11 build hides them unless
// a POSIX level is requested, while on macOS requesting one hides them.
#if defined(__linux__) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "../../tools/atp_cli/ifeq10.c"

#include <unistd.h>

static int g_total = 0;
static int g_fail = 0;

static const char *const k_lcl096_in[] = {
  "cnf(condensed_detachment,axiom,",
  "    ( ~ is_a_theorem(equivalent(X,Y))",
  "    | ~ is_a_theorem(X)",
  "    | is_a_theorem(Y) ) ).",
  "cnf(lg_2,axiom,",
  "    is_a_theorem(equivalent(equivalent(equivalent(equivalent(equivalent(X,Y),equivalent(X,Z)),equivalent(Y,Z)),U),U)) ).",
  "cnf(lg_3,axiom,",
  "    is_a_theorem(equivalent(equivalent(equivalent(equivalent(equivalent(equivalent(X,Y),equivalent(X,Z)),U),equivalent(equivalent(Y,Z),U)),V),V)) ).",
  "cnf(lg_4,axiom,",
  "    is_a_theorem(equivalent(equivalent(equivalent(equivalent(X,Y),Z),U),equivalent(equivalent(equivalent(X,V),Z),equivalent(equivalent(Y,V),U)))) ).",
  "cnf(prove_lg_1,negated_conjecture,",
  "    ~ is_a_theorem(equivalent(equivalent(equivalent(a,equivalent(equivalent(b,b),a)),c),c)) ).",
  NULL
};

static const char *const k_lcl096_out[] = {
  "cnf(ifeq_axiom,axiom,",
  "    ifeq(A,A,B,C) = B ).",
  "cnf(condensed_detachment,axiom,",
  "    ifeq(is_a_theorem(equivalent(X,Y)),true,ifeq(is_a_theorem(X),true,is_a_theorem(Y),true),true) = true ).",
  "cnf(lg_2,axiom,",
  "    is_a_theorem(equivalent(equivalent(equivalent(equivalent(equivalent(X,Y),equivalent(X,Z)),equivalent(Y,Z)),U),U)) = true ).",
  "cnf(lg_3,axiom,",
  "    is_a_theorem(equivalent(equivalent(equivalent(equivalent(equivalent(equivalent(X,Y),equivalent(X,Z)),U),equivalent(equivalent(Y,Z),U)),V),V)) = true ).",
  "cnf(lg_4,axiom,",
  "    is_a_theorem(equivalent(equivalent(equivalent(equivalent(X,Y),Z),U),equivalent(equivalent(equivalent(X,V),Z),equivalent(equivalent(Y,V),U)))) = true ).",
  "cnf(prove_lg_1,negated_conjecture,",
  "    is_a_theorem(equivalent(equivalent(equivalent(a,equivalent(equivalent(b,b),a)),c),c)) != true ).",
  NULL
};

static const char *const k_lcl423_in[] = {
  "cnf(condensed_detachment,axiom,",
  "    ( ~ is_a_theorem(implies(A,B))",
  "    | ~ is_a_theorem(A)",
  "    | is_a_theorem(B) ) ).",
  "cnf(luka23,axiom,",
  "    is_a_theorem(implies(implies(implies(A,B),implies(implies(implies(not(C),not(D)),E),C)),implies(F,implies(implies(C,A),implies(D,A))))) ).",
  "cnf(prove_luka_3_basis,negated_conjecture,",
  "    ( ~ is_a_theorem(implies(implies(a,b),implies(implies(b,c),implies(a,c))))",
  "    | ~ is_a_theorem(implies(implies(not(a),a),a))",
  "    | ~ is_a_theorem(implies(a,implies(not(a),b))) ) ).",
  NULL
};

static const char *const k_lcl423_out[] = {
  "cnf(ifeq_axiom,axiom,",
  "    ifeq(A,A,B,C) = B ).",
  "cnf(condensed_detachment,axiom,",
  "    ifeq(is_a_theorem(implies(A,B)),true,ifeq(is_a_theorem(A),true,is_a_theorem(B),true),true) = true ).",
  "cnf(luka23,axiom,",
  "    is_a_theorem(implies(implies(implies(A,B),implies(implies(implies(not(C),not(D)),E),C)),implies(F,implies(implies(C,A),implies(D,A))))) = true ).",
  "cnf(prove_luka_3_basis,negated_conjecture,",
  "    tuple(is_a_theorem(implies(implies(not(a),a),a)),is_a_theorem(implies(implies(a,b),implies(implies(b,c),implies(a,c)))),is_a_theorem(implies(a,implies(not(a),b)))) != tuple(true,true,true) ).",
  NULL
};

static const char *const k_ana004_in[] = {
  "cnf(right_identity,axiom,",
  "    add(X,n0) = X ).",
  "cnf(left_identity,axiom,",
  "    add(n0,X) = X ).",
  "cnf(reflexivity_of_less_than,axiom,",
  "    ~ less_than(X,X) ).",
  "cnf(transitivity_of_less_than,axiom,",
  "    ( ~ less_than(X,Y)",
  "    | ~ less_than(Y,Z)",
  "    | less_than(X,Z) ) ).",
  "cnf(axiom_2_1,axiom,",
  "    ( ~ less_than(n0,X)",
  "    | ~ less_than(n0,Y)",
  "    | less_than(n0,minimum(X,Y)) ) ).",
  "cnf(axiom_2_2,axiom,",
  "    ( ~ less_than(n0,X)",
  "    | ~ less_than(n0,Y)",
  "    | less_than(minimum(X,Y),X) ) ).",
  "cnf(axiom_2_3,axiom,",
  "    ( ~ less_than(n0,X)",
  "    | ~ less_than(n0,Y)",
  "    | less_than(minimum(X,Y),Y) ) ).",
  "cnf(axiom_3,axiom,",
  "    ( ~ less_than(X,half(Xa))",
  "    | ~ less_than(Y,half(Xa))",
  "    | less_than(add(X,Y),Xa) ) ).",
  "cnf(c_17,axiom,",
  "    ( ~ less_than(add(absolute(X),absolute(Y)),Xa)",
  "    | less_than(absolute(add(X,Y)),Xa) ) ).",
  "cnf(axiom_5,axiom,",
  "    add(add(X,Y),Z) = add(X,add(Y,Z)) ).",
  "cnf(axiom_6_1,axiom,",
  "    add(X,Y) = add(Y,X) ).",
  "cnf(axiom_6_2,axiom,",
  "    ( ~ less_than(n0,Xa)",
  "    | less_than(n0,half(Xa)) ) ).",
  "cnf(axiom_7,axiom,",
  "    ( ~ less_than(n0,Xa)",
  "    | less_than(n0,half(Xa)) ) ).",
  "cnf(axiom_8,axiom,",
  "    minus(add(X,Y)) = add(minus(X),minus(Y)) ).",
  "cnf(c_10,negated_conjecture,",
  "    ( ~ less_than(n0,X)",
  "    | less_than(n0,fp31(X)) ) ).",
  "cnf(c_11,negated_conjecture,",
  "    ( ~ less_than(n0,X)",
  "    | ~ less_than(absolute(add(Y,minus(a))),fp31(X))",
  "    | less_than(absolute(add(f(Y),minus(l1))),X) ) ).",
  "cnf(c_12,negated_conjecture,",
  "    ( ~ less_than(n0,X)",
  "    | less_than(n0,fp32(X)) ) ).",
  "cnf(c_13,negated_conjecture,",
  "    ( ~ less_than(n0,X)",
  "    | ~ less_than(absolute(add(Y,minus(a))),fp32(X))",
  "    | less_than(absolute(add(g(Y),minus(l2))),X) ) ).",
  "cnf(c_14,negated_conjecture,",
  "    less_than(n0,b) ).",
  "cnf(c_15,negated_conjecture,",
  "    ( ~ less_than(n0,X)",
  "    | less_than(absolute(add(fp33(X),minus(a))),X) ) ).",
  "cnf(c_16,negated_conjecture,",
  "    ( ~ less_than(n0,X)",
  "    | ~ less_than(absolute(add(add(f(fp33(X)),minus(l1)),add(g(fp33(X)),minus(l2)))),b) ) ).",
  NULL
};

static const char *const k_ana004_out[] = {
  "cnf(ifeq_axiom,axiom,",
  "    ifeq3(A,A,B,C) = B ).",
  "cnf(ifeq_axiom_001,axiom,",
  "    ifeq2(A,A,B,C) = B ).",
  "cnf(ifeq_axiom_002,axiom,",
  "    ifeq(A,A,B,C) = B ).",
  "cnf(right_identity,axiom,",
  "    add(X,n0) = X ).",
  "cnf(left_identity,axiom,",
  "    add(n0,X) = X ).",
  "cnf(transitivity_of_less_than,axiom,",
  "    ifeq3(less_than(Y,Z),true,ifeq3(less_than(X,Y),true,less_than(X,Z),true),true) = true ).",
  "cnf(axiom_2_1,axiom,",
  "    ifeq3(less_than(n0,Y),true,ifeq3(less_than(n0,X),true,less_than(n0,minimum(X,Y)),true),true) = true ).",
  "cnf(axiom_2_2,axiom,",
  "    ifeq3(less_than(n0,Y),true,ifeq3(less_than(n0,X),true,less_than(minimum(X,Y),X),true),true) = true ).",
  "cnf(axiom_2_3,axiom,",
  "    ifeq3(less_than(n0,Y),true,ifeq3(less_than(n0,X),true,less_than(minimum(X,Y),Y),true),true) = true ).",
  "cnf(axiom_3,axiom,",
  "    ifeq3(less_than(Y,half(Xa)),true,ifeq3(less_than(X,half(Xa)),true,less_than(add(X,Y),Xa),true),true) = true ).",
  "cnf(c_17,axiom,",
  "    ifeq3(less_than(add(absolute(X),absolute(Y)),Xa),true,less_than(absolute(add(X,Y)),Xa),true) = true ).",
  "cnf(axiom_5,axiom,",
  "    add(add(X,Y),Z) = add(X,add(Y,Z)) ).",
  "cnf(axiom_6_1,axiom,",
  "    add(X,Y) = add(Y,X) ).",
  "cnf(axiom_6_2,axiom,",
  "    ifeq3(less_than(n0,Xa),true,less_than(n0,half(Xa)),true) = true ).",
  "cnf(axiom_7,axiom,",
  "    ifeq3(less_than(n0,Xa),true,less_than(n0,half(Xa)),true) = true ).",
  "cnf(axiom_8,axiom,",
  "    minus(add(X,Y)) = add(minus(X),minus(Y)) ).",
  "cnf(c_10,negated_conjecture,",
  "    ifeq3(less_than(n0,X),true,less_than(n0,fp31(X)),true) = true ).",
  "cnf(c_11,negated_conjecture,",
  "    ifeq3(less_than(absolute(add(Y,minus(a))),fp31(X)),true,ifeq3(less_than(n0,X),true,less_than(absolute(add(f(Y),minus(l1))),X),true),true) = true ).",
  "cnf(c_12,negated_conjecture,",
  "    ifeq3(less_than(n0,X),true,less_than(n0,fp32(X)),true) = true ).",
  "cnf(c_13,negated_conjecture,",
  "    ifeq3(less_than(absolute(add(Y,minus(a))),fp32(X)),true,ifeq3(less_than(n0,X),true,less_than(absolute(add(g(Y),minus(l2))),X),true),true) = true ).",
  "cnf(c_14,negated_conjecture,",
  "    less_than(n0,b) = true ).",
  "cnf(c_15,negated_conjecture,",
  "    ifeq3(less_than(n0,X),true,less_than(absolute(add(fp33(X),minus(a))),X),true) = true ).",
  "cnf(reflexivity_of_less_than,axiom,",
  "    ifeq2(less_than(X,X),true,a2,b2) = b2 ).",
  "cnf(c_16,negated_conjecture,",
  "    ifeq(tuple(less_than(n0,X),less_than(absolute(add(add(f(fp33(X)),minus(l1)),add(g(fp33(X)),minus(l2)))),b)),tuple(true,true),a2,b2) = b2 ).",
  "cnf(goal,negated_conjecture,",
  "    a2 != b2 ).",
  NULL
};

static const char *const k_puz131_in[] = {
  "fof(student_type,axiom,",
  "    ? [A] : student(A) ).",
  "fof(professor_type,axiom,",
  "    ? [A] : professor(A) ).",
  "fof(course_type,axiom,",
  "    ? [A] : course(A) ).",
  "fof(michael_type,axiom,",
  "    student(michael) ).",
  "fof(victor_type,axiom,",
  "    professor(victor) ).",
  "fof(csc410_type,axiom,",
  "    course(csc410) ).",
  "fof(coordinator_of_type,axiom,",
  "    ! [A] :",
  "      ( course(A)",
  "     => professor(coordinatorof(A)) ) ).",
  "fof(student_enrolled_axiom,axiom,",
  "    ! [X] :",
  "      ( student(X)",
  "     => ? [Y] :",
  "          ( course(Y)",
  "          & enrolled(X,Y) ) ) ).",
  "fof(professor_teaches,axiom,",
  "    ! [X] :",
  "      ( professor(X)",
  "     => ? [Y] :",
  "          ( course(Y)",
  "          & teaches(X,Y) ) ) ).",
  "fof(course_enrolled,axiom,",
  "    ! [X] :",
  "      ( course(X)",
  "     => ? [Y] :",
  "          ( student(Y)",
  "          & enrolled(Y,X) ) ) ).",
  "fof(course_teaches,axiom,",
  "    ! [X] :",
  "      ( course(X)",
  "     => ? [Y] :",
  "          ( professor(Y)",
  "          & teaches(Y,X) ) ) ).",
  "fof(coordinator_teaches,axiom,",
  "    ! [X] :",
  "      ( course(X)",
  "     => teaches(coordinatorof(X),X) ) ).",
  "fof(student_enrolled_taught,axiom,",
  "    ! [X,Y] :",
  "      ( ( student(X)",
  "        & course(Y) )",
  "     => ( enrolled(X,Y)",
  "       => ! [Z] :",
  "            ( professor(Z)",
  "           => ( teaches(Z,Y)",
  "             => taughtby(X,Z) ) ) ) ) ).",
  "fof(michael_enrolled_csc410_axiom,axiom,",
  "    enrolled(michael,csc410) ).",
  "fof(victor_coordinator_csc410_axiom,axiom,",
  "    coordinatorof(csc410) = victor ).",
  "fof(teaching_conjecture,conjecture,",
  "    taughtby(michael,victor) ).",
  NULL
};

static const char *const k_puz131_out[] = {
  "cnf(ifeq_axiom,axiom,",
  "    ifeq(A,A,B,C) = B ).",
  "cnf(student_type,axiom,",
  "    student(sK7_student_type_A) = true ).",
  "cnf(professor_type,axiom,",
  "    professor(sK6_professor_type_A) = true ).",
  "cnf(course_type,axiom,",
  "    course(sK5_course_type_A) = true ).",
  "cnf(michael_type,axiom,",
  "    student(michael) = true ).",
  "cnf(victor_type,axiom,",
  "    professor(victor) = true ).",
  "cnf(csc410_type,axiom,",
  "    course(csc410) = true ).",
  "cnf(coordinator_of_type,axiom,",
  "    ifeq(course(A),true,professor(coordinatorof(A)),true) = true ).",
  "cnf(student_enrolled_axiom_1,axiom,",
  "    ifeq(student(X),true,enrolled(X,sK4_student_enrolled_axiom_Y(X)),true) = true ).",
  "cnf(student_enrolled_axiom,axiom,",
  "    ifeq(student(X),true,course(sK4_student_enrolled_axiom_Y(X)),true) = true ).",
  "cnf(professor_teaches_1,axiom,",
  "    ifeq(professor(X),true,teaches(X,sK3_professor_teaches_Y(X)),true) = true ).",
  "cnf(professor_teaches,axiom,",
  "    ifeq(professor(X),true,course(sK3_professor_teaches_Y(X)),true) = true ).",
  "cnf(course_enrolled_1,axiom,",
  "    ifeq(course(X),true,enrolled(sK2_course_enrolled_Y(X),X),true) = true ).",
  "cnf(course_enrolled,axiom,",
  "    ifeq(course(X),true,student(sK2_course_enrolled_Y(X)),true) = true ).",
  "cnf(course_teaches_1,axiom,",
  "    ifeq(course(X),true,teaches(sK1_course_teaches_Y(X),X),true) = true ).",
  "cnf(course_teaches,axiom,",
  "    ifeq(course(X),true,professor(sK1_course_teaches_Y(X)),true) = true ).",
  "cnf(coordinator_teaches,axiom,",
  "    ifeq(course(X),true,teaches(coordinatorof(X),X),true) = true ).",
  "cnf(student_enrolled_taught,axiom,",
  "    ifeq(teaches(Z,Y),true,ifeq(enrolled(X,Y),true,ifeq(course(Y),true,ifeq(professor(Z),true,ifeq(student(X),true,taughtby(X,Z),true),true),true),true),true) = true ).",
  "cnf(michael_enrolled_csc410_axiom,axiom,",
  "    enrolled(michael,csc410) = true ).",
  "cnf(victor_coordinator_csc410_axiom,axiom,",
  "    coordinatorof(csc410) = victor ).",
  "cnf(teaching_conjecture,negated_conjecture,",
  "    taughtby(michael,victor) != true ).",
  NULL
};

static const char *const k_kle090_in[] = {
  "fof(additive_commutativity,axiom,",
  "    ! [A,B] : addition(A,B) = addition(B,A) ).",
  "fof(additive_associativity,axiom,",
  "    ! [C,B,A] : addition(A,addition(B,C)) = addition(addition(A,B),C) ).",
  "fof(additive_identity,axiom,",
  "    ! [A] : addition(A,zero) = A ).",
  "fof(additive_idempotence,axiom,",
  "    ! [A] : addition(A,A) = A ).",
  "fof(multiplicative_associativity,axiom,",
  "    ! [A,B,C] : multiplication(A,multiplication(B,C)) = multiplication(multiplication(A,B),C) ).",
  "fof(multiplicative_right_identity,axiom,",
  "    ! [A] : multiplication(A,one) = A ).",
  "fof(multiplicative_left_identity,axiom,",
  "    ! [A] : multiplication(one,A) = A ).",
  "fof(right_distributivity,axiom,",
  "    ! [A,B,C] : multiplication(A,addition(B,C)) = addition(multiplication(A,B),multiplication(A,C)) ).",
  "fof(left_distributivity,axiom,",
  "    ! [A,B,C] : multiplication(addition(A,B),C) = addition(multiplication(A,C),multiplication(B,C)) ).",
  "fof(right_annihilation,axiom,",
  "    ! [A] : multiplication(A,zero) = zero ).",
  "fof(left_annihilation,axiom,",
  "    ! [A] : multiplication(zero,A) = zero ).",
  "fof(order,axiom,",
  "    ! [A,B] :",
  "      ( leq(A,B)",
  "    <=> addition(A,B) = B ) ).",
  "fof(domain1,axiom,",
  "    ! [X0] : multiplication(antidomain(X0),X0) = zero ).",
  "fof(domain2,axiom,",
  "    ! [X0,X1] : addition(antidomain(multiplication(X0,X1)),antidomain(multiplication(X0,antidomain(antidomain(X1))))) = antidomain(multiplication(X0,antidomain(antidomain(X1)))) ).",
  "fof(domain3,axiom,",
  "    ! [X0] : addition(antidomain(antidomain(X0)),antidomain(X0)) = one ).",
  "fof(domain4,axiom,",
  "    ! [X0] : domain(X0) = antidomain(antidomain(X0)) ).",
  "fof(codomain1,axiom,",
  "    ! [X0] : multiplication(X0,coantidomain(X0)) = zero ).",
  "fof(codomain2,axiom,",
  "    ! [X0,X1] : addition(coantidomain(multiplication(X0,X1)),coantidomain(multiplication(coantidomain(coantidomain(X0)),X1))) = coantidomain(multiplication(coantidomain(coantidomain(X0)),X1)) ).",
  "fof(codomain3,axiom,",
  "    ! [X0] : addition(coantidomain(coantidomain(X0)),coantidomain(X0)) = one ).",
  "fof(codomain4,axiom,",
  "    ! [X0] : codomain(X0) = coantidomain(coantidomain(X0)) ).",
  "fof(goals,conjecture,",
  "    ! [X0,X1] :",
  "      ( addition(X0,X1) = X1",
  "     => addition(antidomain(X1),antidomain(X0)) = antidomain(X0) ) ).",
  NULL
};

static const char *const k_kle090_out[] = {
  "cnf(ifeq_axiom,axiom,",
  "    ifeq2(A,A,B,C) = B ).",
  "cnf(ifeq_axiom_001,axiom,",
  "    ifeq(A,A,B,C) = B ).",
  "cnf(additive_commutativity,axiom,",
  "    addition(A,B) = addition(B,A) ).",
  "cnf(additive_associativity,axiom,",
  "    addition(A,addition(B,C)) = addition(addition(A,B),C) ).",
  "cnf(additive_identity,axiom,",
  "    addition(A,zero) = A ).",
  "cnf(additive_idempotence,axiom,",
  "    addition(A,A) = A ).",
  "cnf(multiplicative_associativity,axiom,",
  "    multiplication(A,multiplication(B,C)) = multiplication(multiplication(A,B),C) ).",
  "cnf(multiplicative_right_identity,axiom,",
  "    multiplication(A,one) = A ).",
  "cnf(multiplicative_left_identity,axiom,",
  "    multiplication(one,A) = A ).",
  "cnf(right_distributivity,axiom,",
  "    multiplication(A,addition(B,C)) = addition(multiplication(A,B),multiplication(A,C)) ).",
  "cnf(left_distributivity,axiom,",
  "    multiplication(addition(A,B),C) = addition(multiplication(A,C),multiplication(B,C)) ).",
  "cnf(right_annihilation,axiom,",
  "    multiplication(A,zero) = zero ).",
  "cnf(left_annihilation,axiom,",
  "    multiplication(zero,A) = zero ).",
  "cnf(order_1,axiom,",
  "    ifeq(leq(A,B),true,addition(A,B),B) = B ).",
  "cnf(order,axiom,",
  "    ifeq2(addition(A,B),B,leq(A,B),true) = true ).",
  "cnf(domain1,axiom,",
  "    multiplication(antidomain(X0),X0) = zero ).",
  "cnf(domain2,axiom,",
  "    addition(antidomain(multiplication(X0,X1)),antidomain(multiplication(X0,antidomain(antidomain(X1))))) = antidomain(multiplication(X0,antidomain(antidomain(X1)))) ).",
  "cnf(domain3,axiom,",
  "    addition(antidomain(antidomain(X0)),antidomain(X0)) = one ).",
  "cnf(domain4,axiom,",
  "    domain(X0) = antidomain(antidomain(X0)) ).",
  "cnf(codomain1,axiom,",
  "    multiplication(X0,coantidomain(X0)) = zero ).",
  "cnf(codomain2,axiom,",
  "    addition(coantidomain(multiplication(X0,X1)),coantidomain(multiplication(coantidomain(coantidomain(X0)),X1))) = coantidomain(multiplication(coantidomain(coantidomain(X0)),X1)) ).",
  "cnf(codomain3,axiom,",
  "    addition(coantidomain(coantidomain(X0)),coantidomain(X0)) = one ).",
  "cnf(codomain4,axiom,",
  "    codomain(X0) = coantidomain(coantidomain(X0)) ).",
  "cnf(goals,negated_conjecture,",
  "    addition(sK2_goals_X0,sK1_goals_X1) = sK1_goals_X1 ).",
  "cnf(goals_1,negated_conjecture,",
  "    addition(antidomain(sK1_goals_X1),antidomain(sK2_goals_X0)) != antidomain(sK2_goals_X0) ).",
  NULL
};

static const char *const k_seu323_in[] = {
  "fof(fc3_tops_1,axiom,",
  "    ! [A,B] :",
  "      ( ( topological_space(A)",
  "        & top_str(A)",
  "        & closed_subset(B,A)",
  "        & element(B,powerset(the_carrier(A))) )",
  "     => open_subset(subset_complement(the_carrier(A),B),A) ) ).",
  "fof(rc6_pre_topc,axiom,",
  "    ! [A] :",
  "      ( ( topological_space(A)",
  "        & top_str(A) )",
  "     => ? [B] :",
  "          ( element(B,powerset(the_carrier(A)))",
  "          & closed_subset(B,A) ) ) ).",
  "fof(involutiveness_k3_subset_1,axiom,",
  "    ! [A,B] :",
  "      ( element(B,powerset(A))",
  "     => subset_complement(A,subset_complement(A,B)) = B ) ).",
  "fof(reflexivity_r1_tarski,axiom,",
  "    ! [A,B] : subset(A,A) ).",
  "fof(existence_l1_struct_0,axiom,",
  "    ? [A] : one_sorted_str(A) ).",
  "fof(dt_k3_subset_1,axiom,",
  "    ! [A,B] :",
  "      ( element(B,powerset(A))",
  "     => element(subset_complement(A,B),powerset(A)) ) ).",
  "fof(dt_k6_pre_topc,axiom,",
  "    ! [A,B] :",
  "      ( ( top_str(A)",
  "        & element(B,powerset(the_carrier(A))) )",
  "     => element(topstr_closure(A,B),powerset(the_carrier(A))) ) ).",
  "fof(dt_l1_struct_0,axiom,",
  "    $true ).",
  "fof(fc2_tops_1,axiom,",
  "    ! [A,B] :",
  "      ( ( topological_space(A)",
  "        & top_str(A)",
  "        & element(B,powerset(the_carrier(A))) )",
  "     => closed_subset(topstr_closure(A,B),A) ) ).",
  "fof(fc4_tops_1,axiom,",
  "    ! [A,B] :",
  "      ( ( topological_space(A)",
  "        & top_str(A)",
  "        & open_subset(B,A)",
  "        & element(B,powerset(the_carrier(A))) )",
  "     => closed_subset(subset_complement(the_carrier(A),B),A) ) ).",
  "fof(existence_l1_pre_topc,axiom,",
  "    ? [A] : top_str(A) ).",
  "fof(existence_m1_subset_1,axiom,",
  "    ! [A] :",
  "    ? [B] : element(B,A) ).",
  "fof(dt_k1_tops_1,axiom,",
  "    ! [A,B] :",
  "      ( ( top_str(A)",
  "        & element(B,powerset(the_carrier(A))) )",
  "     => element(interior(A,B),powerset(the_carrier(A))) ) ).",
  "fof(dt_k1_zfmisc_1,axiom,",
  "    $true ).",
  "fof(dt_l1_pre_topc,axiom,",
  "    ! [A] :",
  "      ( top_str(A)",
  "     => one_sorted_str(A) ) ).",
  "fof(dt_m1_subset_1,axiom,",
  "    $true ).",
  "fof(dt_u1_struct_0,axiom,",
  "    $true ).",
  "fof(rc1_tops_1,axiom,",
  "    ! [A] :",
  "      ( ( topological_space(A)",
  "        & top_str(A) )",
  "     => ? [B] :",
  "          ( element(B,powerset(the_carrier(A)))",
  "          & open_subset(B,A) ) ) ).",
  "fof(t3_subset,axiom,",
  "    ! [A,B] :",
  "      ( element(A,powerset(B))",
  "    <=> subset(A,B) ) ).",
  "fof(d1_tops_1,axiom,",
  "    ! [A] :",
  "      ( top_str(A)",
  "     => ! [B] :",
  "          ( element(B,powerset(the_carrier(A)))",
  "         => interior(A,B) = subset_complement(the_carrier(A),topstr_closure(A,subset_complement(the_carrier(A),B))) ) ) ).",
  "fof(t51_tops_1,conjecture,",
  "    ! [A] :",
  "      ( ( topological_space(A)",
  "        & top_str(A) )",
  "     => ! [B] :",
  "          ( element(B,powerset(the_carrier(A)))",
  "         => open_subset(interior(A,B),A) ) ) ).",
  NULL
};

static const char *const k_seu323_out[] = {
  "cnf(ifeq_axiom,axiom,",
  "    ifeq2(A,A,B,C) = B ).",
  "cnf(ifeq_axiom_001,axiom,",
  "    ifeq(A,A,B,C) = B ).",
  "cnf(fc3_tops_1,axiom,",
  "    ifeq(element(B,powerset(the_carrier(A))),true,ifeq(closed_subset(B,A),true,ifeq(topological_space(A),true,ifeq(top_str(A),true,open_subset(subset_complement(the_carrier(A),B),A),true),true),true),true) = true ).",
  "cnf(rc6_pre_topc_1,axiom,",
  "    ifeq(topological_space(A),true,ifeq(top_str(A),true,element(sK7_rc6_pre_topc_B(A),powerset(the_carrier(A))),true),true) = true ).",
  "cnf(rc6_pre_topc,axiom,",
  "    ifeq(topological_space(A),true,ifeq(top_str(A),true,closed_subset(sK7_rc6_pre_topc_B(A),A),true),true) = true ).",
  "cnf(involutiveness_k3_subset_1,axiom,",
  "    ifeq2(element(B,powerset(A)),true,subset_complement(A,subset_complement(A,B)),B) = B ).",
  "cnf(reflexivity_r1_tarski,axiom,",
  "    subset(A,A) = true ).",
  "cnf(existence_l1_struct_0,axiom,",
  "    one_sorted_str(sK6_existence_l1_struct_0_A) = true ).",
  "cnf(dt_k3_subset_1,axiom,",
  "    ifeq(element(B,powerset(A)),true,element(subset_complement(A,B),powerset(A)),true) = true ).",
  "cnf(dt_k6_pre_topc,axiom,",
  "    ifeq(element(B,powerset(the_carrier(A))),true,ifeq(top_str(A),true,element(topstr_closure(A,B),powerset(the_carrier(A))),true),true) = true ).",
  "cnf(fc2_tops_1,axiom,",
  "    ifeq(element(B,powerset(the_carrier(A))),true,ifeq(topological_space(A),true,ifeq(top_str(A),true,closed_subset(topstr_closure(A,B),A),true),true),true) = true ).",
  "cnf(fc4_tops_1,axiom,",
  "    ifeq(open_subset(B,A),true,ifeq(element(B,powerset(the_carrier(A))),true,ifeq(topological_space(A),true,ifeq(top_str(A),true,closed_subset(subset_complement(the_carrier(A),B),A),true),true),true),true) = true ).",
  "cnf(existence_l1_pre_topc,axiom,",
  "    top_str(sK5_existence_l1_pre_topc_A) = true ).",
  "cnf(existence_m1_subset_1,axiom,",
  "    element(sK4_existence_m1_subset_1_B(A),A) = true ).",
  "cnf(dt_k1_tops_1,axiom,",
  "    ifeq(element(B,powerset(the_carrier(A))),true,ifeq(top_str(A),true,element(interior(A,B),powerset(the_carrier(A))),true),true) = true ).",
  "cnf(dt_l1_pre_topc,axiom,",
  "    ifeq(top_str(A),true,one_sorted_str(A),true) = true ).",
  "cnf(rc1_tops_1_1,axiom,",
  "    ifeq(topological_space(A),true,ifeq(top_str(A),true,open_subset(sK3_rc1_tops_1_B(A),A),true),true) = true ).",
  "cnf(rc1_tops_1,axiom,",
  "    ifeq(topological_space(A),true,ifeq(top_str(A),true,element(sK3_rc1_tops_1_B(A),powerset(the_carrier(A))),true),true) = true ).",
  "cnf(t3_subset_1,axiom,",
  "    ifeq(subset(A,B),true,element(A,powerset(B)),true) = true ).",
  "cnf(t3_subset,axiom,",
  "    ifeq(element(A,powerset(B)),true,subset(A,B),true) = true ).",
  "cnf(d1_tops_1,axiom,",
  "    ifeq2(element(B,powerset(the_carrier(A))),true,ifeq2(top_str(A),true,subset_complement(the_carrier(A),topstr_closure(A,subset_complement(the_carrier(A),B))),interior(A,B)),interior(A,B)) = interior(A,B) ).",
  "cnf(t51_tops_1,negated_conjecture,",
  "    top_str(sK2_t51_tops_1_A) = true ).",
  "cnf(t51_tops_1_1,negated_conjecture,",
  "    topological_space(sK2_t51_tops_1_A) = true ).",
  "cnf(t51_tops_1_2,negated_conjecture,",
  "    element(sK1_t51_tops_1_B,powerset(the_carrier(sK2_t51_tops_1_A))) = true ).",
  "cnf(t51_tops_1_3,negated_conjecture,",
  "    open_subset(interior(sK2_t51_tops_1_A,sK1_t51_tops_1_B),sK2_t51_tops_1_A) != true ).",
  NULL
};

static const char *const k_pla023_in[] = {
  "cnf(and_definition,axiom,",
  "    ( holds(and(X,Y),State)",
  "    | ~ holds(X,State)",
  "    | ~ holds(Y,State) ) ).",
  "cnf(pickup_1,axiom,",
  "    ( holds(holding(X),do(pickup(X),State))",
  "    | ~ holds(empty,State)",
  "    | ~ holds(clear(X),State)",
  "    | ~ differ(X,table) ) ).",
  "cnf(pickup_2,axiom,",
  "    ( holds(clear(Y),do(pickup(X),State))",
  "    | ~ holds(on(X,Y),State)",
  "    | ~ holds(clear(X),State)",
  "    | ~ holds(empty,State) ) ).",
  "cnf(pickup_3,axiom,",
  "    ( holds(on(X,Y),do(pickup(Z),State))",
  "    | ~ holds(on(X,Y),State)",
  "    | ~ differ(X,Z) ) ).",
  "cnf(pickup_4,axiom,",
  "    ( holds(clear(X),do(pickup(Z),State))",
  "    | ~ holds(clear(X),State)",
  "    | ~ differ(X,Z) ) ).",
  "cnf(putdown_1,axiom,",
  "    ( holds(empty,do(putdown(X,Y),State))",
  "    | ~ holds(holding(X),State)",
  "    | ~ holds(clear(Y),State) ) ).",
  "cnf(putdown_2,axiom,",
  "    ( holds(on(X,Y),do(putdown(X,Y),State))",
  "    | ~ holds(holding(X),State)",
  "    | ~ holds(clear(Y),State) ) ).",
  "cnf(putdown_3,axiom,",
  "    ( holds(clear(X),do(putdown(X,Y),State))",
  "    | ~ holds(holding(X),State)",
  "    | ~ holds(clear(Y),State) ) ).",
  "cnf(putdown_4,axiom,",
  "    ( holds(on(X,Y),do(putdown(Z,W),State))",
  "    | ~ holds(on(X,Y),State) ) ).",
  "cnf(putdown_5,axiom,",
  "    ( holds(clear(Z),do(putdown(X,Y),State))",
  "    | ~ holds(clear(Z),State)",
  "    | ~ differ(Z,Y) ) ).",
  "cnf(symmetry_of_differ,axiom,",
  "    ( differ(X,Y)",
  "    | ~ differ(Y,X) ) ).",
  "cnf(differ_a_b,axiom,",
  "    differ(a,b) ).",
  "cnf(differ_a_c,axiom,",
  "    differ(a,c) ).",
  "cnf(differ_a_d,axiom,",
  "    differ(a,d) ).",
  "cnf(differ_a_table,axiom,",
  "    differ(a,table) ).",
  "cnf(differ_b_c,axiom,",
  "    differ(b,c) ).",
  "cnf(differ_b_d,axiom,",
  "    differ(b,d) ).",
  "cnf(differ_b_table,axiom,",
  "    differ(b,table) ).",
  "cnf(differ_c_d,axiom,",
  "    differ(c,d) ).",
  "cnf(differ_c_table,axiom,",
  "    differ(c,table) ).",
  "cnf(differ_d_table,axiom,",
  "    differ(d,table) ).",
  "cnf(initial_state1,axiom,",
  "    holds(on(a,table),s0) ).",
  "cnf(initial_state2,axiom,",
  "    holds(on(b,table),s0) ).",
  "cnf(initial_state3,axiom,",
  "    holds(on(c,d),s0) ).",
  "cnf(initial_state4,axiom,",
  "    holds(on(d,table),s0) ).",
  "cnf(initial_state5,axiom,",
  "    holds(clear(a),s0) ).",
  "cnf(initial_state6,axiom,",
  "    holds(clear(b),s0) ).",
  "cnf(initial_state7,axiom,",
  "    holds(clear(c),s0) ).",
  "cnf(initial_state8,axiom,",
  "    holds(empty,s0) ).",
  "cnf(clear_table,axiom,",
  "    holds(clear(table),State) ).",
  "cnf(prove_DAC,negated_conjecture,",
  "    ~ holds(and(on(d,a),on(a,c)),State) ).",
  NULL
};

static const char *const k_pla023_out[] = {
  "cnf(ifeq_axiom,axiom,",
  "    ifeq(A,A,B,C) = B ).",
  "cnf(and_definition,axiom,",
  "    ifeq(holds(Y,State),true,ifeq(holds(X,State),true,holds(and(X,Y),State),true),true) = true ).",
  "cnf(pickup_1,axiom,",
  "    ifeq(differ(X,table),true,ifeq(holds(empty,State),true,ifeq(holds(clear(X),State),true,holds(holding(X),do(pickup(X),State)),true),true),true) = true ).",
  "cnf(pickup_2,axiom,",
  "    ifeq(holds(on(X,Y),State),true,ifeq(holds(empty,State),true,ifeq(holds(clear(X),State),true,holds(clear(Y),do(pickup(X),State)),true),true),true) = true ).",
  "cnf(pickup_3,axiom,",
  "    ifeq(differ(X,Z),true,ifeq(holds(on(X,Y),State),true,holds(on(X,Y),do(pickup(Z),State)),true),true) = true ).",
  "cnf(pickup_4,axiom,",
  "    ifeq(differ(X,Z),true,ifeq(holds(clear(X),State),true,holds(clear(X),do(pickup(Z),State)),true),true) = true ).",
  "cnf(putdown_1,axiom,",
  "    ifeq(holds(holding(X),State),true,ifeq(holds(clear(Y),State),true,holds(empty,do(putdown(X,Y),State)),true),true) = true ).",
  "cnf(putdown_2,axiom,",
  "    ifeq(holds(holding(X),State),true,ifeq(holds(clear(Y),State),true,holds(on(X,Y),do(putdown(X,Y),State)),true),true) = true ).",
  "cnf(putdown_3,axiom,",
  "    ifeq(holds(holding(X),State),true,ifeq(holds(clear(Y),State),true,holds(clear(X),do(putdown(X,Y),State)),true),true) = true ).",
  "cnf(putdown_4,axiom,",
  "    ifeq(holds(on(X,Y),State),true,holds(on(X,Y),do(putdown(Z,W),State)),true) = true ).",
  "cnf(putdown_5,axiom,",
  "    ifeq(differ(Z,Y),true,ifeq(holds(clear(Z),State),true,holds(clear(Z),do(putdown(X,Y),State)),true),true) = true ).",
  "cnf(symmetry_of_differ,axiom,",
  "    ifeq(differ(Y,X),true,differ(X,Y),true) = true ).",
  "cnf(differ_a_b,axiom,",
  "    differ(a,b) = true ).",
  "cnf(differ_a_c,axiom,",
  "    differ(a,c) = true ).",
  "cnf(differ_a_d,axiom,",
  "    differ(a,d) = true ).",
  "cnf(differ_a_table,axiom,",
  "    differ(a,table) = true ).",
  "cnf(differ_b_c,axiom,",
  "    differ(b,c) = true ).",
  "cnf(differ_b_d,axiom,",
  "    differ(b,d) = true ).",
  "cnf(differ_b_table,axiom,",
  "    differ(b,table) = true ).",
  "cnf(differ_c_d,axiom,",
  "    differ(c,d) = true ).",
  "cnf(differ_c_table,axiom,",
  "    differ(c,table) = true ).",
  "cnf(differ_d_table,axiom,",
  "    differ(d,table) = true ).",
  "cnf(initial_state1,axiom,",
  "    holds(on(a,table),s0) = true ).",
  "cnf(initial_state2,axiom,",
  "    holds(on(b,table),s0) = true ).",
  "cnf(initial_state3,axiom,",
  "    holds(on(c,d),s0) = true ).",
  "cnf(initial_state4,axiom,",
  "    holds(on(d,table),s0) = true ).",
  "cnf(initial_state5,axiom,",
  "    holds(clear(a),s0) = true ).",
  "cnf(initial_state6,axiom,",
  "    holds(clear(b),s0) = true ).",
  "cnf(initial_state7,axiom,",
  "    holds(clear(c),s0) = true ).",
  "cnf(initial_state8,axiom,",
  "    holds(empty,s0) = true ).",
  "cnf(clear_table,axiom,",
  "    holds(clear(table),State) = true ).",
  "cnf(prove_DAC,negated_conjecture,",
  "    holds(and(on(d,a),on(a,c)),State) != true ).",
  NULL
};

static const char *const k_lat183_in[] = {
  "cnf(idempotence_of_meet,axiom,",
  "    meet(X,X) = X ).",
  "cnf(idempotence_of_join,axiom,",
  "    join(X,X) = X ).",
  "cnf(absorption1,axiom,",
  "    meet(X,join(X,Y)) = X ).",
  "cnf(absorption2,axiom,",
  "    join(X,meet(X,Y)) = X ).",
  "cnf(commutativity_of_meet,axiom,",
  "    meet(X,Y) = meet(Y,X) ).",
  "cnf(commutativity_of_join,axiom,",
  "    join(X,Y) = join(Y,X) ).",
  "cnf(associativity_of_meet,axiom,",
  "    meet(meet(X,Y),Z) = meet(X,meet(Y,Z)) ).",
  "cnf(associativity_of_join,axiom,",
  "    join(join(X,Y),Z) = join(X,join(Y,Z)) ).",
  "cnf(complement_join,axiom,",
  "    join(X,complement(X)) = one ).",
  "cnf(complement_meet,axiom,",
  "    meet(X,complement(X)) = zero ).",
  "cnf(meet_join_complement,axiom,",
  "    ( meet(X,Y) != zero",
  "    | join(X,Y) != one",
  "    | complement(X) = Y ) ).",
  "cnf(equation_H7,axiom,",
  "    meet(X,join(Y,meet(X,Z))) = meet(X,join(Y,meet(X,join(meet(X,Y),meet(Z,join(X,Y)))))) ).",
  "cnf(prove_distributivity,negated_conjecture,",
  "    meet(a,join(b,c)) != join(meet(a,b),meet(a,c)) ).",
  NULL
};

static const char *const k_lat183_out[] = {
  "cnf(ifeq_axiom,axiom,",
  "    ifeq(A,A,B,C) = B ).",
  "cnf(idempotence_of_meet,axiom,",
  "    meet(X,X) = X ).",
  "cnf(idempotence_of_join,axiom,",
  "    join(X,X) = X ).",
  "cnf(absorption1,axiom,",
  "    meet(X,join(X,Y)) = X ).",
  "cnf(absorption2,axiom,",
  "    join(X,meet(X,Y)) = X ).",
  "cnf(commutativity_of_meet,axiom,",
  "    meet(X,Y) = meet(Y,X) ).",
  "cnf(commutativity_of_join,axiom,",
  "    join(X,Y) = join(Y,X) ).",
  "cnf(associativity_of_meet,axiom,",
  "    meet(meet(X,Y),Z) = meet(X,meet(Y,Z)) ).",
  "cnf(associativity_of_join,axiom,",
  "    join(join(X,Y),Z) = join(X,join(Y,Z)) ).",
  "cnf(complement_join,axiom,",
  "    join(X,complement(X)) = one ).",
  "cnf(complement_meet,axiom,",
  "    meet(X,complement(X)) = zero ).",
  "cnf(meet_join_complement,axiom,",
  "    ifeq(join(X,Y),one,ifeq(meet(X,Y),zero,complement(X),Y),Y) = Y ).",
  "cnf(equation_H7,axiom,",
  "    meet(X,join(Y,meet(X,Z))) = meet(X,join(Y,meet(X,join(meet(X,Y),meet(Z,join(X,Y)))))) ).",
  "cnf(prove_distributivity,negated_conjecture,",
  "    meet(a,join(b,c)) != join(meet(a,b),meet(a,c)) ).",
  NULL
};

static const char *const k_alg069_in[] = {
  "fof(ax1,axiom,",
  "    ! [U] :",
  "      ( sorti1(U)",
  "     => ! [V] :",
  "          ( sorti1(V)",
  "         => sorti1(op1(U,V)) ) ) ).",
  "fof(ax2,axiom,",
  "    ! [U] :",
  "      ( sorti2(U)",
  "     => ! [V] :",
  "          ( sorti2(V)",
  "         => sorti2(op2(U,V)) ) ) ).",
  "fof(ax3,axiom,",
  "    ! [U] :",
  "      ( sorti1(U)",
  "     => ? [V] :",
  "          ( sorti1(V)",
  "          & op1(V,V) = U ) ) ).",
  "fof(ax4,axiom,",
  "    ~ ! [U] :",
  "        ( sorti2(U)",
  "       => ? [V] :",
  "            ( sorti2(V)",
  "            & op2(V,V) = U ) ) ).",
  "fof(co1,conjecture,",
  "    ( ( ! [U] :",
  "          ( sorti1(U)",
  "         => sorti2(h(U)) )",
  "      & ! [V] :",
  "          ( sorti2(V)",
  "         => sorti1(j(V)) ) )",
  "   => ~ ( ! [W] :",
  "            ( sorti1(W)",
  "           => ! [X] :",
  "                ( sorti1(X)",
  "               => h(op1(W,X)) = op2(h(W),h(X)) ) )",
  "        & ! [Y] :",
  "            ( sorti2(Y)",
  "           => ! [Z] :",
  "                ( sorti2(Z)",
  "               => j(op2(Y,Z)) = op1(j(Y),j(Z)) ) )",
  "        & ! [X1] :",
  "            ( sorti2(X1)",
  "           => h(j(X1)) = X1 )",
  "        & ! [X2] :",
  "            ( sorti1(X2)",
  "           => j(h(X2)) = X2 ) ) ) ).",
  NULL
};

static const char *const k_alg069_out[] = {
  "cnf(ifeq_axiom,axiom,",
  "    ifeq2(A,A,B,C) = B ).",
  "cnf(ifeq_axiom_001,axiom,",
  "    ifeq(A,A,B,C) = B ).",
  "cnf(ax1,axiom,",
  "    ifeq(sorti1(V),true,ifeq(sorti1(U),true,sorti1(op1(U,V)),true),true) = true ).",
  "cnf(ax2,axiom,",
  "    ifeq(sorti2(V),true,ifeq(sorti2(U),true,sorti2(op2(U,V)),true),true) = true ).",
  "cnf(ax3_1,axiom,",
  "    ifeq(sorti1(U),true,sorti1(sK2_ax3_V(U)),true) = true ).",
  "cnf(ax3,axiom,",
  "    ifeq2(sorti1(U),true,op1(sK2_ax3_V(U),sK2_ax3_V(U)),U) = U ).",
  "cnf(ax4_1,axiom,",
  "    tuple(op2(V,V),sorti2(V)) != tuple(sK1_ax4_U,true) ).",
  "cnf(ax4,axiom,",
  "    sorti2(sK1_ax4_U) = true ).",
  "cnf(co1,negated_conjecture,",
  "    ifeq(sorti1(U),true,sorti2(h(U)),true) = true ).",
  "cnf(co1_1,negated_conjecture,",
  "    ifeq2(sorti1(X),true,ifeq2(sorti1(W),true,op2(h(W),h(X)),h(op1(W,X))),h(op1(W,X))) = h(op1(W,X)) ).",
  "cnf(co1_2,negated_conjecture,",
  "    ifeq2(sorti1(X2),true,j(h(X2)),X2) = X2 ).",
  "cnf(co1_3,negated_conjecture,",
  "    ifeq(sorti2(V),true,sorti1(j(V)),true) = true ).",
  "cnf(co1_4,negated_conjecture,",
  "    ifeq2(sorti2(Z),true,ifeq2(sorti2(Y),true,op1(j(Y),j(Z)),j(op2(Y,Z))),j(op2(Y,Z))) = j(op2(Y,Z)) ).",
  "cnf(co1_5,negated_conjecture,",
  "    ifeq2(sorti2(X1),true,h(j(X1)),X1) = X1 ).",
  NULL
};

static const char *const k_cat010_in[] = {
  "cnf(equivalence_implies_existence1,axiom,",
  "    ( ~ equivalent(X,Y)",
  "    | there_exists(X) ) ).",
  "cnf(equivalence_implies_existence2,axiom,",
  "    ( ~ equivalent(X,Y)",
  "    | X = Y ) ).",
  "cnf(existence_and_equality_implies_equivalence1,axiom,",
  "    ( ~ there_exists(X)",
  "    | X != Y",
  "    | equivalent(X,Y) ) ).",
  "cnf(domain_has_elements,axiom,",
  "    ( ~ there_exists(domain(X))",
  "    | there_exists(X) ) ).",
  "cnf(codomain_has_elements,axiom,",
  "    ( ~ there_exists(codomain(X))",
  "    | there_exists(X) ) ).",
  "cnf(composition_implies_domain,axiom,",
  "    ( ~ there_exists(compose(X,Y))",
  "    | there_exists(domain(X)) ) ).",
  "cnf(domain_codomain_composition1,axiom,",
  "    ( ~ there_exists(compose(X,Y))",
  "    | domain(X) = codomain(Y) ) ).",
  "cnf(domain_codomain_composition2,axiom,",
  "    ( ~ there_exists(domain(X))",
  "    | domain(X) != codomain(Y)",
  "    | there_exists(compose(X,Y)) ) ).",
  "cnf(associativity_of_compose,axiom,",
  "    compose(X,compose(Y,Z)) = compose(compose(X,Y),Z) ).",
  "cnf(compose_domain,axiom,",
  "    compose(X,domain(X)) = X ).",
  "cnf(compose_codomain,axiom,",
  "    compose(codomain(X),X) = X ).",
  "cnf(ab_exists,hypothesis,",
  "    there_exists(compose(a,b)) ).",
  "cnf(prove_codomain_of_ab_equals_codomain_of_a,negated_conjecture,",
  "    codomain(compose(a,b)) != codomain(a) ).",
  NULL
};

static const char *const k_cat010_out[] = {
  "cnf(ifeq_axiom,axiom,",
  "    ifeq3(A,A,B,C) = B ).",
  "cnf(ifeq_axiom_001,axiom,",
  "    ifeq2(A,A,B,C) = B ).",
  "cnf(ifeq_axiom_002,axiom,",
  "    ifeq(A,A,B,C) = B ).",
  "cnf(equivalence_implies_existence1,axiom,",
  "    ifeq(equivalent(X,Y),true,there_exists(X),true) = true ).",
  "cnf(equivalence_implies_existence2,axiom,",
  "    ifeq2(equivalent(X,Y),true,X,Y) = Y ).",
  "cnf(existence_and_equality_implies_equivalence1,axiom,",
  "    ifeq(there_exists(Y),true,equivalent(Y,Y),true) = true ).",
  "cnf(domain_has_elements,axiom,",
  "    ifeq(there_exists(domain(X)),true,there_exists(X),true) = true ).",
  "cnf(codomain_has_elements,axiom,",
  "    ifeq(there_exists(codomain(X)),true,there_exists(X),true) = true ).",
  "cnf(composition_implies_domain,axiom,",
  "    ifeq(there_exists(compose(X,Y)),true,there_exists(domain(X)),true) = true ).",
  "cnf(domain_codomain_composition1,axiom,",
  "    ifeq2(there_exists(compose(X,Y)),true,domain(X),codomain(Y)) = codomain(Y) ).",
  "cnf(domain_codomain_composition2,axiom,",
  "    ifeq(there_exists(domain(X)),true,ifeq3(domain(X),codomain(Y),there_exists(compose(X,Y)),true),true) = true ).",
  "cnf(associativity_of_compose,axiom,",
  "    compose(X,compose(Y,Z)) = compose(compose(X,Y),Z) ).",
  "cnf(compose_domain,axiom,",
  "    compose(X,domain(X)) = X ).",
  "cnf(compose_codomain,axiom,",
  "    compose(codomain(X),X) = X ).",
  "cnf(ab_exists,hypothesis,",
  "    there_exists(compose(a,b)) = true ).",
  "cnf(prove_codomain_of_ab_equals_codomain_of_a,negated_conjecture,",
  "    codomain(compose(a,b)) != codomain(a) ).",
  NULL
};

static const char *const k_nonhorn_in[] = {
  "cnf(c1,axiom, p(X) | q(X) ).",
  "cnf(c2,negated_conjecture, ~ p(a) ).",
  "cnf(c3,negated_conjecture, ~ q(a) ).",
  NULL
};

static const char *const k_sat1_in[] = {
  "cnf(c1,axiom, ~ q(X) | p(X) ).",
  "cnf(c2,negated_conjecture, ~ p(a) ).",
  NULL
};

static const char *const k_include_in[] = {
  "include('Axioms/NONE000-0.ax').",
  "cnf(c1,negated_conjecture, ~ p(a) ).",
  NULL
};

static const char *const k_mixed_in[] = {
  "cnf(c1,axiom, p(a) ).",
  "fof(g,conjecture, p(a) ).",
  NULL
};

static const char *const k_twoconj_in[] = {
  "fof(g1,conjecture, p(a) ).",
  "fof(g2,conjecture, p(b) ).",
  NULL
};

// A TPTP header: its Status carries over (a theorem encodes to an
// unsatisfiable problem), with the SPC that np0_pool.py filters on.
static const char *const k_header_in[] = {
  "%------------------------------------------------------------------------------",
  "% File     : HDR001+1 : header check",
  "% Status   : Theorem",
  "%------------------------------------------------------------------------------",
  "fof(a1,axiom, p(a) ).",
  "fof(g,conjecture, p(a) ).",
  NULL
};

static const char *const k_header_out[] = {
  "cnf(a1,axiom,",
  "    p(a) = true ).",
  "cnf(g,negated_conjecture,",
  "    p(a) != true ).",
  NULL
};

static const char *const k_header_raw[] = {
  "% Status   : Unsatisfiable\n",
  "% SPC      : CNF_UNS_RFO_PEQ_UEQ\n",
  "% Encoding : dispatchers 0, tuples 0, predicates 1, definitions 0\n",
  NULL
};

// Without a header there is no status to carry.  The Encoding line counts
// what the encoding added.
static const char *const k_lcl096_raw[] = {
  "!% Status",
  "!% SPC",
  "% Encoding : dispatchers 1, tuples 0, predicates 1, definitions 0\n",
  NULL
};

static const char *const k_lcl423_raw[] = {
  "% Encoding : dispatchers 1, tuples 1, predicates 1, definitions 0\n",
  NULL
};

// Unit equality already: the encoding is the identity and says so, which
// is how make_ifeq10_corpus.sh tells it from a real encoding.
static const char *const k_unit_in[] = {
  "cnf(left_identity,axiom, f(e,X) = X ).",
  "cnf(prove_it,negated_conjecture, f(e,a) != a ).",
  NULL
};

static const char *const k_unit_out[] = {
  "cnf(left_identity,axiom,",
  "    f(e,X) = X ).",
  "cnf(prove_it,negated_conjecture,",
  "    f(e,a) != a ).",
  NULL
};

static const char *const k_unit_raw[] = {
  "% Encoding : dispatchers 0, tuples 0, predicates 0, definitions 0\n",
  NULL
};

static char *join_lines(const char *const *lines) {
  size_t n = 1;
  for (int i = 0; lines[i] != NULL; i++) {
    n += strlen(lines[i]) + 1u;
  }
  char *s = (char *)malloc(n);
  if (s == NULL) {
    return NULL;
  }
  s[0] = '\0';
  size_t o = 0;
  for (int i = 0; lines[i] != NULL; i++) {
    size_t l = strlen(lines[i]);
    memcpy(s + o, lines[i], l);
    o += l;
    s[o++] = '\n';
  }
  s[o] = '\0';
  return s;
}

// Drops %-comment lines and every whitespace character.
static char *squash(const char *s) {
  char *d = (char *)malloc(strlen(s) + 1u);
  if (d == NULL) {
    return NULL;
  }
  size_t o = 0;
  int bol = 1, comment = 0;
  for (const char *p = s; *p != '\0'; p++) {
    if (bol && *p == '%') {
      comment = 1;
    }
    if (*p == '\n') {
      bol = 1;
      comment = 0;
      continue;
    }
    if (!isspace((unsigned char)*p)) {
      bol = 0;
    }
    if (!comment && !isspace((unsigned char)*p)) {
      d[o++] = *p;
    }
  }
  d[o] = '\0';
  return d;
}

static char *slurp(FILE *f) {
  long n = ftell(f);
  char *s = (char *)malloc((size_t)(n > 0 ? n : 0) + 1u);
  if (s == NULL) {
    return NULL;
  }
  rewind(f);
  size_t rd = fread(s, 1, (size_t)(n > 0 ? n : 0), f);
  s[rd] = '\0';
  return s;
}

// The first record where two squashed outputs differ, for the log.
static void show_diff(const char *got, const char *want) {
  size_t i = 0;
  while (got[i] != '\0' && got[i] == want[i]) {
    i++;
  }
  size_t from = (i > 120) ? i - 120 : 0;
  fprintf(stderr, "    first difference at byte %zu\n    got : %.240s\n    want: %.240s\n",
          i, got + from, want + from);
}

// raw: strings the unsquashed output must contain ("!..." must not).
static void check_case(const char *name, const char *const *in, const char *const *out,
                       int want_rc, const char *want_msg, const char *const *raw) {
  g_total++;
  char *text = join_lines(in);
  FILE *tmp = tmpfile();
  if (text == NULL || tmp == NULL) {
    fprintf(stderr, "  FAIL %s: cannot set up the case\n", name);
    g_fail++;
    free(text);
    if (tmp != NULL) {
      fclose(tmp);
    }
    return;
  }
  int rc = ifeq10_encode(text, NULL, tmp);
  char *got = slurp(tmp);
  fclose(tmp);
  free(text);
  int ok = (rc == want_rc);
  if (!ok) {
    fprintf(stderr, "  FAIL %s: exit %d, want %d (%s)\n", name, rc, want_rc, g_i10_msg);
  }
  if (ok && want_msg != NULL && strstr(g_i10_msg, want_msg) == NULL) {
    fprintf(stderr, "  FAIL %s: message '%s' lacks '%s'\n", name, g_i10_msg, want_msg);
    ok = 0;
  }
  if (ok && out != NULL) {
    char *want_text = join_lines(out);
    char *a = squash(got != NULL ? got : "");
    char *b = squash(want_text != NULL ? want_text : "");
    if (a == NULL || b == NULL || strcmp(a, b) != 0) {
      fprintf(stderr, "  FAIL %s: the encoding differs from TPTP's\n", name);
      if (a != NULL && b != NULL) {
        show_diff(a, b);
      }
      ok = 0;
    }
    free(a);
    free(b);
    free(want_text);
  }
  if (ok && out == NULL && got != NULL && got[0] != '\0') {
    fprintf(stderr, "  FAIL %s: a failed encoding wrote output\n", name);
    ok = 0;
  }
  for (int i = 0; ok && raw != NULL && raw[i] != NULL; i++) {
    int absent = (raw[i][0] == '!');
    const char *needle = raw[i] + absent;
    if ((got != NULL && strstr(got, needle) != NULL) == absent) {
      fprintf(stderr, "  FAIL %s: the output %s '%s'\n", name,
              absent ? "contains" : "lacks", needle);
      ok = 0;
    }
  }
  free(got);
  if (!ok) {
    g_fail++;
  }
}

// Two encodings of one input, compared byte for byte (the header's
// Encoding line included): no global state leaks from one run to the next.
static void check_repeat(const char *name, const char *const *in) {
  g_total++;
  char *text = join_lines(in);
  FILE *t1 = tmpfile();
  FILE *t2 = tmpfile();
  int ok = (text != NULL && t1 != NULL && t2 != NULL);
  char *a = NULL;
  char *b = NULL;
  if (ok) {
    int r1 = ifeq10_encode(text, NULL, t1);
    int r2 = ifeq10_encode(text, NULL, t2);
    a = slurp(t1);
    b = slurp(t2);
    ok = (r1 == I10_RC_OK && r2 == I10_RC_OK && a != NULL && b != NULL && a[0] != '\0' &&
          strcmp(a, b) == 0);
  }
  if (!ok) {
    fprintf(stderr, "  FAIL %s: two encodings of one input differ\n", name);
    if (a != NULL && b != NULL) {
      show_diff(a, b);
    }
    g_fail++;
  }
  free(a);
  free(b);
  free(text);
  if (t1 != NULL) {
    fclose(t1);
  }
  if (t2 != NULL) {
    fclose(t2);
  }
}

static int write_text(const char *path, const char *text) {
  FILE *f = fopen(path, "wb");
  if (f == NULL) {
    return 0;
  }
  int ok = (fputs(text, f) >= 0);
  return (fclose(f) == 0) && ok;
}

// include('outer.ax',[o1,i1,i2,zz]) of an axiom file that itself does
// include('inner.ax',[i1,i3]): the nested selection is the intersection,
// so exactly o1 and i1 arrive (the merged list is longer than the inner
// one, the case whose allocation was once too small).
static void check_nested_include(void) {
  g_total++;
  static const char *const want[] = {
    "cnf(o1,", "cnf(i1,", "cnf(c3,", "!cnf(o2,", "!cnf(i2,", "!cnf(i3,", "!w2", "!w3", "!w4", NULL
  };
  const char *td = getenv("TMPDIR");
  char dir[1024];
  snprintf(dir, sizeof dir, "%s/test_atp_ifeq10.XXXXXX", (td != NULL && td[0] != '\0') ? td : "/tmp");
  if (mkdtemp(dir) == NULL) {
    fprintf(stderr, "  FAIL nested include: mkdtemp\n");
    g_fail++;
    return;
  }
  char inner[1040], outer[1040], prob[1040];
  snprintf(inner, sizeof inner, "%s/inner.ax", dir);
  snprintf(outer, sizeof outer, "%s/outer.ax", dir);
  snprintf(prob, sizeof prob, "%s/prob.p", dir);
  const char *prob_text =
    "include('outer.ax',[o1,i1,i2,zz]).\n"
    "cnf(c3,negated_conjecture, ~ q(a) ).\n";
  int ok = write_text(inner,
                      "cnf(i1,axiom, ~ p(X) | q(X) ).\n"
                      "cnf(i2,axiom, w2(a) ).\n"
                      "cnf(i3,axiom, w3(a) ).\n") &&
           write_text(outer,
                      "cnf(o1,axiom, p(a) ).\n"
                      "cnf(o2,axiom, w4(a) ).\n"
                      "include('inner.ax',[i1,i3]).\n") &&
           write_text(prob, prob_text);
  FILE *tmp = ok ? tmpfile() : NULL;
  char *got = NULL;
  int rc = -1;
  if (tmp != NULL) {
    rc = ifeq10_encode(prob_text, prob, tmp);
    got = slurp(tmp);
    fclose(tmp);
  }
  ok = (rc == I10_RC_OK && got != NULL);
  if (!ok) {
    fprintf(stderr, "  FAIL nested include: exit %d (%s)\n", rc, g_i10_msg);
  }
  for (int i = 0; ok && want[i] != NULL; i++) {
    int absent = (want[i][0] == '!');
    const char *needle = want[i] + absent;
    if ((strstr(got, needle) != NULL) == absent) {
      fprintf(stderr, "  FAIL nested include: the output %s '%s'\n",
              absent ? "contains" : "lacks", needle);
      ok = 0;
    }
  }
  free(got);
  unlink(inner);
  unlink(outer);
  unlink(prob);
  rmdir(dir);
  if (!ok) {
    g_fail++;
  }
}

// An n-deep f(...f(a)...) side: refused past I10_NEST_MAX as an input
// error, not a stack overflow; encoded below it.
static void check_deep(int n, int want_rc) {
  g_total++;
  const char *pre = "cnf(ax, axiom, ";
  const char *post = " = b).\ncnf(goal, negated_conjecture, c != b).\n";
  size_t len = strlen(pre) + 3u * (size_t)n + 1u + strlen(post) + 1u;
  char *text = malloc(len);
  FILE *tmp = tmpfile();
  if (text == NULL || tmp == NULL) {
    fprintf(stderr, "  FAIL deep %d: cannot set up the case\n", n);
    g_fail++;
    free(text);
    if (tmp != NULL) {
      fclose(tmp);
    }
    return;
  }
  char *w = text;
  w += sprintf(w, "%s", pre);
  for (int i = 0; i < n; i++) {
    *w++ = 'f';
    *w++ = '(';
  }
  *w++ = 'a';
  for (int i = 0; i < n; i++) {
    *w++ = ')';
  }
  sprintf(w, "%s", post);
  int rc = ifeq10_encode(text, NULL, tmp);
  fclose(tmp);
  free(text);
  int ok = (rc == want_rc);
  if (ok && want_rc == I10_RC_INPUT && strstr(g_i10_msg, "nesting deeper than") == NULL) {
    ok = 0;
  }
  if (!ok) {
    fprintf(stderr, "  FAIL deep %d: exit %d, want %d (%s)\n", n, rc, want_rc, g_i10_msg);
    g_fail++;
  }
}

int main(void) {
  printf("test_atp_ifeq10\n");
  check_case("LCL096-1", k_lcl096_in, k_lcl096_out, I10_RC_OK, NULL, k_lcl096_raw);
  check_case("LCL423-1", k_lcl423_in, k_lcl423_out, I10_RC_OK, NULL, k_lcl423_raw);
  check_case("ANA004-1", k_ana004_in, k_ana004_out, I10_RC_OK, NULL, NULL);
  check_case("PUZ131+1", k_puz131_in, k_puz131_out, I10_RC_OK, NULL, NULL);
  check_case("KLE090+1", k_kle090_in, k_kle090_out, I10_RC_OK, NULL, NULL);
  check_case("SEU323+1", k_seu323_in, k_seu323_out, I10_RC_OK, NULL, NULL);
  check_case("PLA023-1", k_pla023_in, k_pla023_out, I10_RC_OK, NULL, NULL);
  check_case("LAT183-1", k_lat183_in, k_lat183_out, I10_RC_OK, NULL, NULL);
  check_case("ALG069+1", k_alg069_in, k_alg069_out, I10_RC_OK, NULL, NULL);
  check_case("CAT010-4", k_cat010_in, k_cat010_out, I10_RC_OK, NULL, NULL);
  // Global state resets between runs: a repeat is byte-identical.
  check_repeat("LCL096-1 twice", k_lcl096_in);
  check_case("header status", k_header_in, k_header_out, I10_RC_OK, NULL, k_header_raw);
  check_case("unit equality", k_unit_in, k_unit_out, I10_RC_OK, NULL, k_unit_raw);
  check_case("non-Horn", k_nonhorn_in, NULL, I10_RC_NONHORN, "not Horn", NULL);
  check_case("one-element model", k_sat1_in, NULL, I10_RC_SAT1, "one-element model", NULL);
  check_case("include without a path", k_include_in, NULL, I10_RC_INPUT,
             "cannot read include", NULL);
  check_nested_include();
  check_case("cnf and fof mixed", k_mixed_in, NULL, I10_RC_INPUT, "mixes cnf and fof", NULL);
  check_case("two conjectures", k_twoconj_in, NULL, I10_RC_INPUT, "more than one conjecture",
             NULL);
  check_deep(I10_NEST_MAX - 8, I10_RC_OK);
  check_deep(100000, I10_RC_INPUT);
  if (g_fail == 0) {
    printf("  ok    %d/%d\n", g_total, g_total);
    return 0;
  }
  printf("  FAIL  %d/%d\n", g_total - g_fail, g_total);
  return 1;
}
