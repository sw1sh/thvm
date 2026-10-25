% Theorem (TPTP SYO561+1): distinct objects are unequal.  The unquoting
% pass renames both to fresh constants (dob<n>), whose equality holds in
% one element, so the gate finds a "model" of the negated conjecture; a
% CounterSatisfiable claim would be unsound.
fof(apple_not_microsoft,conjecture,"Apple" != "Microsoft").
