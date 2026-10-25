
%--------------------------------------------------------------------------
% File     : COL069-1 : TPTP v9.2.1. Released v1.2.0.
% Domain   : Combinatory Logic
% Problem  : Strong fixed point for B and L
% Version  : [WM88] (equality) axioms.
% English  : The strong fixed point property holds for the set
%            P consisting of the combinators B and L, where ((Bx)y)z
%            = x(yz), (Lx)y = x(yy).

% Refs     : [WM88]  Wos & McCune (1988), Challenge Problems Focusing on Eq
%          : [Wos93] Wos (1993), The Kernel Strategy and Its Use for the St
% Source   : [Wos93]
% Names    : Question 6 [Wos93]
%          : Question 16 [Wos93]

% Status   : Satisfiable
% Rating   : 1.00 v2.0.0
% Syntax   : Number of clauses     :    3 (   3 unt;   0 nHn;   1 RR)
%            Number of literals    :    3 (   3 equ;   1 neg)
%            Maximal clause size   :    1 (   1 avg)
%            Maximal term depth    :    4 (   2 avg)
%            Number of predicates  :    1 (   0 usr;   0 prp; 2-2 aty)
%            Number of functors    :    4 (   4 usr;   2 con; 0-2 aty)
%            Number of variables   :    6 (   0 sgn)
% SPC      : CNF_SAT_RFO_PEQ_UEQ

% Comments :
%--------------------------------------------------------------------------
cnf(b_definition,axiom,
    apply(apply(apply(b,X),Y),Z) = apply(X,apply(Y,Z)) ).

cnf(l_definition,axiom,
    apply(apply(l,X),Y) = apply(X,apply(Y,Y)) ).

cnf(prove_fixed_point,negated_conjecture,
    apply(Y,f(Y)) != apply(f(Y),apply(Y,f(Y))) ).

%--------------------------------------------------------------------------
