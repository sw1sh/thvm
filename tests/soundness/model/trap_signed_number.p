% Unsatisfiable: numbers are interpreted as themselves, so -1 != 2.5.  The
% unquoting pass renames both to fresh constants (num<n>); the claim reads
% their spelling back from the rename table.
fof(a1,axiom,-1 = 2.5).
