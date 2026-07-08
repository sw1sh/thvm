% synth ProoVer proof (gold: buggy, unentail) -- family witness0
fof(ax, axiom,
    ! [X] : ? [Y] : ? [Z] : rel(Y, Z, X), file('synth',ax)).
fof(c, conjecture,
    ? [U] : ? [V] : ? [W] : rel(U, V, W), file('synth',c)).
fof(nc, negated_conjecture,
    ~(? [U] : ? [V] : ? [W] : rel(U, V, W)), inference(negated_conjecture, [status(cth)], [c])).
fof(sk1, plain,
    ! [X] : ? [Z] : rel(sK00(X), Z, X), inference(skolemize, [status(esa), new_symbols(skolem, [sK00])], [ax])).
fof(sk2, plain,
    ! [X] : rel(sK00(X), sK01(X), X), inference(skolemize, [status(esa), new_symbols(skolem, [sK01])], [sk1])).
fof(inst, plain,
    rel(sK00(a), sK01(a), a), inference(instantiate, [status(thm)], [sk2])).
fof(bogus_step, plain,
    unrelated_pred(bogus_c), inference(deduction, [status(thm)], [ax])).
fof(false, plain,
    $false, inference(consequence, [status(thm)], [nc, inst])).
