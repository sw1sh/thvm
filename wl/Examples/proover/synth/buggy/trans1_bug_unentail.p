% synth ProoVer proof (gold: buggy, unentail) -- family trans1
fof(ax1, axiom,
    p = q, file('synth',ax1)).
fof(ax2, axiom,
    q = r, file('synth',ax2)).
fof(c, conjecture,
    p = r, file('synth',c)).
fof(nc, negated_conjecture,
    p != r, inference(negated_conjecture, [status(cth)], [c])).
fof(ded, plain,
    p = r, inference(deduction, [status(thm)], [ax1, ax2])).
fof(bogus_step, plain,
    unrelated_pred(bogus_c), inference(deduction, [status(thm)], [ax1])).
fof(false, plain,
    $false, inference(consequence, [status(thm)], [nc, ded])).
