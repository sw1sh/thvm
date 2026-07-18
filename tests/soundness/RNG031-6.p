%--------------------------------------------------------------------------
% File     : RNG031-6 : TPTP v9.2.1. Released v1.0.0.
% Domain   : Ring Theory (Right alternative)
% Problem  : (W*W)*X*(W*W) = additive identity
% Version  : [Ste87] (equality) axioms : Reduced > Complete.
% English  :

% Refs     : [Ste87] Stevens (1987), Some Experiments in Nonassociative Rin
%          : [Ste92] Stevens (1992), Unpublished Note
% Source   : [Ste87]
% Names    : Conjecture 2 [Ste87]

% Status   : Satisfiable
% Rating   : 0.60 v9.1.0, 0.71 v9.0.0, 0.67 v8.2.0, 0.60 v8.1.0, 0.50 v7.1.0, 0.67 v7.0.0, 0.33 v6.4.0, 0.50 v6.3.0, 0.33 v6.2.0, 0.67 v6.1.0, 0.60 v6.0.0, 0.40 v5.4.0, 0.50 v5.3.0, 0.67 v4.1.0, 1.00 v4.0.1, 0.67 v3.3.0, 0.33 v3.2.0, 0.67 v3.1.0, 0.33 v2.6.0, 0.67 v2.5.0, 1.00 v2.0.0
% Syntax   : Number of clauses     :   15 (  15 unt;   0 nHn;   1 RR)
%            Number of literals    :   15 (  15 equ;   1 neg)
%            Maximal clause size   :    1 (   1 avg)
%            Maximal term depth    :    5 (   2 avg)
%            Number of predicates  :    1 (   0 usr;   0 prp; 2-2 aty)
%            Number of functors    :    8 (   8 usr;   3 con; 0-3 aty)
%            Number of variables   :   25 (   2 sgn)
% SPC      : CNF_SAT_RFO_PEQ_UEQ

% Comments : This conjecture has been shown true. See [Ste92].
%--------------------------------------------------------------------------
%----Don't Include nonassociative ring axioms.
%----The left alternative law has to be omitted.
% include('axioms/RNG003-0.ax').
%--------------------------------------------------------------------------
%----Commutativity for addition
cnf(commutativity_for_addition,axiom,
    add(X,Y) = add(Y,X) ).

%----Associativity for addition
cnf(associativity_for_addition,axiom,
    add(X,add(Y,Z)) = add(add(X,Y),Z) ).

%----There exists an additive identity element
cnf(left_additive_identity,axiom,
    add(additive_identity,X) = X ).

cnf(right_additive_identity,axiom,
    add(X,additive_identity) = X ).

%----Multiplicative zero
cnf(left_multiplicative_zero,axiom,
    multiply(additive_identity,X) = additive_identity ).

cnf(right_multiplicative_zero,axiom,
    multiply(X,additive_identity) = additive_identity ).

%----Existence of left additive additive_inverse
cnf(left_additive_inverse,axiom,
    add(additive_inverse(X),X) = additive_identity ).

cnf(right_additive_inverse,axiom,
    add(X,additive_inverse(X)) = additive_identity ).

%----Distributive property of product over sum
cnf(distribute1,axiom,
    multiply(X,add(Y,Z)) = add(multiply(X,Y),multiply(X,Z)) ).

cnf(distribute2,axiom,
    multiply(add(X,Y),Z) = add(multiply(X,Z),multiply(Y,Z)) ).

%----Inverse of additive_inverse of X is X
cnf(additive_inverse_additive_inverse,axiom,
    additive_inverse(additive_inverse(X)) = X ).

%----Right alternative law
cnf(right_alternative,axiom,
    multiply(multiply(X,Y),Y) = multiply(X,multiply(Y,Y)) ).

%----Left alternative law
% input_clause(left_alternative,axiom,
%     [++equal(multiply(multiply(X,X),Y),multiply(X,multiply(X,Y)))]).

%----Associator
cnf(associator,axiom,
    associator(X,Y,Z) = add(multiply(multiply(X,Y),Z),additive_inverse(multiply(X,multiply(Y,Z)))) ).

%----Commutator
cnf(commutator,axiom,
    commutator(X,Y) = add(multiply(Y,X),additive_inverse(multiply(X,Y))) ).

cnf(prove_conjecture_2,negated_conjecture,
    multiply(multiply(multiply(associator(x,x,y),associator(x,x,y)),x),multiply(associator(x,x,y),associator(x,x,y))) != additive_identity ).

%--------------------------------------------------------------------------
