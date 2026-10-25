%------------------------------------------------------------------------------
% File     : edge_include_select_fof
% Status   : CounterSatisfiable
% Comments : include('file', [names]) imports ONLY the named formulas.  With
%            just a1 the conjecture q fails (a != b, q false; two elements,
%            so the one-element gate does not decide it); splicing the whole
%            axiom file would add a2 = (a = b | q) and make q a "theorem".
%------------------------------------------------------------------------------
include('Axioms/edge_include_select_fof.ax',[a1]).

fof(goal,conjecture,
    q ).
%------------------------------------------------------------------------------
