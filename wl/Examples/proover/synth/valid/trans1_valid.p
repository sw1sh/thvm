% synth ProoVer proof (gold: valid) -- family trans1
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
fof(false, plain,
    $false, inference(consequence, [status(thm)], [nc, ded])).
