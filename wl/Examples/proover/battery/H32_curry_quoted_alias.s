% Proof    : Problems/WID003+1.p
% INVALID: 'ap2:' and 'wide:f' are symbols of the problem, unrelated to
% the 17-ary f; read as the encoding's own names, the conjecture would
% be the axiom.
fof(ax, axiom, f(a,a,a,a,a,a,a,a,a,a,a,a,a,a,a,a,a) = c, file('Problems/WID003+1.p',ax)).
fof(c0, conjecture, 'ap2:'('ap2:'('ap2:'('ap2:'('ap2:'('ap2:'('ap2:'('ap2:'('ap2:'('ap2:'('ap2:'('ap2:'('ap2:'('ap2:'('ap2:'('ap2:'('ap2:'('wide:f',a),a),a),a),a),a),a),a),a),a),a),a),a),a),a),a),a) = c, file('Problems/WID003+1.p',c0)).
fof(nc, negated_conjecture, 'ap2:'('ap2:'('ap2:'('ap2:'('ap2:'('ap2:'('ap2:'('ap2:'('ap2:'('ap2:'('ap2:'('ap2:'('ap2:'('ap2:'('ap2:'('ap2:'('ap2:'('wide:f',a),a),a),a),a),a),a),a),a),a),a),a),a),a),a),a),a) != c, inference(negated_conjecture, [status(cth)], [c0])).
fof(f1, plain, $false, inference(superposition, [status(thm)], [nc, ax])).
