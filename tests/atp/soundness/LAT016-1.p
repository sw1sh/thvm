
%--------------------------------------------------------------------------
% File     : LAT016-1 : TPTP v9.2.1. Bugfixed v2.2.1.
% Domain   : Lattice Theory (Ortholattices)
% Problem  : E1 fails for Ortholattices.
% Version  : [McC98b] (equality) axioms.
% English  : Show that Ortholattices do not necessarily satisfy equation E1.

% Refs     : [McC98a] McCune (1998), Automatic Proofs and Counterexamples f
%          : [McC98b] McCune (1998), Email to G. Sutcliffe
% Source   : [McC98b]
% Names    : OL-1 [McC98b]

% Status   : Satisfiable
% Rating   : 0.20 v9.1.0, 0.43 v9.0.0, 0.44 v8.2.0, 0.40 v8.1.0, 0.25 v7.5.0, 0.00 v6.2.0, 0.33 v6.1.0, 0.40 v5.5.0, 0.60 v5.4.0, 0.50 v5.3.0, 0.67 v5.2.0, 0.33 v2.6.0, 0.67 v2.5.0, 1.00 v2.2.1
% Syntax   : Number of clauses     :   11 (  11 unt;   0 nHn;   1 RR)
%            Number of literals    :   11 (  11 equ;   1 neg)
%            Maximal clause size   :    1 (   1 avg)
%            Maximal term depth    :    9 (   2 avg)
%            Number of predicates  :    1 (   0 usr;   0 prp; 2-2 aty)
%            Number of functors    :    7 (   7 usr;   4 con; 0-2 aty)
%            Number of variables   :   19 (   2 sgn)
% SPC      : CNF_SAT_RFO_PEQ_UEQ

% Comments : Ortholattice lemmas are included in McCunes original, but have
%            been removed here.
%          : The smallest model has 10 elements.
% Bugfixes : v2.2.1 - Bugfix in LAT003-0.ax.
%--------------------------------------------------------------------------
%----Include ortholattice axioms
include('Axioms/LAT003-0.ax').
%--------------------------------------------------------------------------
%----Denial of equation E1
cnf(prove_e1,negated_conjecture,
    join(complement(join(meet(a,complement(b)),complement(a))),join(meet(a,complement(b)),join(meet(complement(a),meet(join(a,complement(b)),join(a,b))),meet(complement(a),complement(meet(join(a,complement(b)),join(a,b))))))) != n1 ).

%--------------------------------------------------------------------------
