% synth ProoVer proof (gold: valid) -- family trans2
fof(ax1, axiom,
    s0 = s1, file('synth',ax1)).
fof(ax2, axiom,
    s1 = s2, file('synth',ax2)).
fof(c, conjecture,
    s0 = s2, file('synth',c)).
fof(nc, negated_conjecture,
    s0 != s2, inference(negated_conjecture, [status(cth)], [c])).
fof(ded, plain,
    s0 = s2, inference(deduction, [status(thm)], [ax1, ax2])).
fof(false, plain,
    $false, inference(consequence, [status(thm)], [nc, ded])).
