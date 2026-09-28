#!/usr/bin/env python3
"""Independent finite-model checker for FindProof's FNT answers.

Usage: model_check.py PROBLEM.p SOLUTION.out [--tptp ROOT]

Reads the SZS status line and the FiniteModel block (Vampire's tff form:
declarations, a finite_domain axiom, one definition per symbol) from
SOLUTION.out, parses the problem's fof/cnf records (includes resolved
against the problem directory and ROOT, selection lists honoured), and
evaluates every record in the model: axiom-like records must be true, the
conjecture false.  It also checks the claim itself: Satisfiable needs no
conjecture, CounterSatisfiable exactly one, and every symbol of the
problem needs a total interpretation at its arity.

Shares no code with the C prover: its own TPTP tokenizer, parser and
evaluator.  Prints one line, "VERIFIED ..." (exit 0) or "FAIL: ..."
(exit 1).
"""
import itertools
import os
import re
import sys

sys.setrecursionlimit(1000000)


class Fail(Exception):
    pass


# --------------------------------------------------------------------------
# tokenizer
# --------------------------------------------------------------------------
OPS = ["<~>", "<=>", "=>", "<=", "~|", "~&", "!=", "!", "?", "~", "&", "|",
       "=", "(", ")", "[", "]", ",", ".", ":", "*", ">"]


def tokenize(text):
    toks = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c.isspace():
            i += 1
        elif c == "%":
            j = text.find("\n", i)
            i = n if j < 0 else j + 1
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            if j < 0:
                raise Fail("unterminated block comment")
            i = j + 2
        elif c == "'":
            j, buf = i + 1, []
            while True:
                if j >= n:
                    raise Fail("unterminated quoted atom")
                if text[j] == "\\" and j + 1 < n:
                    buf.append(text[j + 1])
                    j += 2
                elif text[j] == "'":
                    j += 1
                    break
                else:
                    buf.append(text[j])
                    j += 1
            # 'cat' and cat are the same atomic word (TPTP BNF).
            toks.append(("name", "".join(buf)))
            i = j
        elif c == '"':
            j, buf = i + 1, []
            while True:
                if j >= n:
                    raise Fail("unterminated distinct object")
                if text[j] == "\\" and j + 1 < n:
                    buf.append(text[j + 1])
                    j += 2
                elif text[j] == '"':
                    j += 1
                    break
                else:
                    buf.append(text[j])
                    j += 1
            toks.append(("distinct", "".join(buf)))
            i = j
        elif c.isdigit() or (c in "+-" and i + 1 < n and text[i + 1].isdigit()):
            m = re.compile(r"[+-]?[0-9]+(/[0-9]+|\.[0-9]+([eE][+-]?[0-9]+)?|[eE][+-]?[0-9]+)?").match(text, i)
            toks.append(("number", m.group(0)))
            i = m.end()
        elif c == "$":
            m = re.compile(r"\$\$?[A-Za-z0-9_]*").match(text, i)
            toks.append(("dollar", m.group(0)))
            i = m.end()
        elif c.isalpha() or c == "_":
            m = re.compile(r"[A-Za-z0-9_]+").match(text, i)
            w = m.group(0)
            toks.append(("var" if (w[0].isupper() or w[0] == "_") else "name", w))
            i = m.end()
        else:
            for op in OPS:
                if text.startswith(op, i):
                    toks.append(("op", op))
                    i += len(op)
                    break
            else:
                raise Fail("unexpected character %r" % c)
    toks.append(("eof", ""))
    return toks


class Parser:
    def __init__(self, toks):
        self.t = toks
        self.p = 0

    def peek(self, k=0):
        return self.t[self.p + k]

    def next(self):
        tok = self.t[self.p]
        self.p += 1
        return tok

    def is_op(self, op, k=0):
        tok = self.peek(k)
        return tok[0] == "op" and tok[1] == op

    def expect(self, op):
        tok = self.next()
        if tok != ("op", op):
            raise Fail("expected %r, got %r" % (op, tok))

    # ---- terms -----------------------------------------------------------
    def term(self):
        kind, val = self.next()
        if kind == "var":
            return ("var", val)
        if kind in ("number", "distinct"):
            return ("special", kind, val)
        if kind in ("name", "dollar"):
            args = []
            if self.is_op("("):
                self.next()
                args.append(self.term())
                while self.is_op(","):
                    self.next()
                    args.append(self.term())
                self.expect(")")
            return ("app", val, tuple(args))
        raise Fail("term expected, got %r" % ((kind, val),))

    # ---- formulas --------------------------------------------------------
    def logic(self):
        f = self.unit()
        tok = self.peek()
        if tok[0] == "op" and tok[1] in ("<=>", "=>", "<=", "<~>", "~|", "~&"):
            self.next()
            g = self.unit()
            return ("bin", tok[1], f, g)
        if tok == ("op", "|") or tok == ("op", "&"):
            op = tok[1]
            parts = [f]
            while self.is_op(op):
                self.next()
                parts.append(self.unit())
            if self.peek()[0] == "op" and self.peek()[1] in ("|", "&"):
                raise Fail("unparenthesised mix of & and |")
            return ("and" if op == "&" else "or", tuple(parts))
        return f

    def unit(self):
        tok = self.peek()
        if tok == ("op", "~"):
            self.next()
            return ("not", self.unit())
        if tok == ("op", "!") or tok == ("op", "?"):
            self.next()
            self.expect("[")
            vs = [self.var_decl()]
            while self.is_op(","):
                self.next()
                vs.append(self.var_decl())
            self.expect("]")
            self.expect(":")
            body = self.unit()
            return ("all" if tok[1] == "!" else "ex", tuple(vs), body)
        if tok == ("op", "("):
            self.next()
            f = self.logic()
            self.expect(")")
            return f
        return self.atomic()

    def var_decl(self):
        kind, val = self.next()
        if kind != "var":
            raise Fail("variable expected in quantifier")
        if self.is_op(":"):          # typed variable (tff): X:$i
            self.next()
            self.type_expr()
        return val

    def atomic(self):
        tok = self.peek()
        if tok == ("dollar", "$true"):
            self.next()
            return ("const", True)
        if tok == ("dollar", "$false"):
            self.next()
            return ("const", False)
        lhs = self.term()
        if self.is_op("="):
            self.next()
            return ("eq", lhs, self.term())
        if self.is_op("!="):
            self.next()
            return ("not", ("eq", lhs, self.term()))
        if lhs[0] != "app":
            raise Fail("atom expected, got %r" % (lhs,))
        return ("pred", lhs[1], lhs[2])

    # ---- tff types (model declarations) ---------------------------------
    def type_expr(self):
        """Returns (argument count, result type name)."""
        if self.is_op("("):
            self.next()
            n = 1
            self.type_atom()
            while self.is_op("*"):
                self.next()
                self.type_atom()
                n += 1
            self.expect(")")
            if self.is_op(">"):
                self.next()
                return (n, self.type_atom())
            if n != 1:
                raise Fail("product type without arrow")
            return (0, "?")
        t = self.type_atom()
        if self.is_op(">"):
            self.next()
            return (1, self.type_atom())
        return (0, t)

    def type_atom(self):
        kind, val = self.next()
        if kind not in ("dollar", "name"):
            raise Fail("type expected")
        return val


# --------------------------------------------------------------------------
# problem reading
# --------------------------------------------------------------------------
def split_records(text):
    """Top-level annotated records: (kind, token list of the body)."""
    toks = tokenize(text)
    recs = []
    i = 0
    while toks[i][0] != "eof":
        kind, val = toks[i]
        if kind != "name" or toks[i + 1] != ("op", "("):
            raise Fail("record expected at %r" % ((kind, val),))
        depth, j = 0, i + 1
        while True:
            if toks[j][0] == "eof":
                raise Fail("unterminated record")
            if toks[j] == ("op", "(") or toks[j] == ("op", "["):
                depth += 1
            elif toks[j] == ("op", ")") or toks[j] == ("op", "]"):
                depth -= 1
                if depth == 0:
                    break
            j += 1
        if toks[j + 1] != ("op", "."):
            raise Fail("record not terminated by '.'")
        recs.append((val, toks[i + 2:j] + [("eof", "")]))
        i = j + 2
    return recs


def read_problem(path, tptp, selection=None, out=None, depth=0):
    if depth > 16:
        raise Fail("include nesting too deep")
    if out is None:
        out = []
    with open(path, encoding="utf-8", errors="strict") as fh:
        text = fh.read()
    for kind, body in split_records(text):
        p = Parser(body)
        if kind == "include":
            fk, fname = p.next()
            if fk != "name":
                raise Fail("include file name expected")
            sel = None
            if p.is_op(","):
                p.next()
                p.expect("[")
                sel = set()
                if not p.is_op("]"):
                    sel.add(p.next()[1])
                    while p.is_op(","):
                        p.next()
                        sel.add(p.next()[1])
                p.expect("]")
            cands = [os.path.join(os.path.dirname(path), fname)]
            if tptp:
                cands.append(os.path.join(tptp, fname))
            for c in cands:
                if os.path.isfile(c):
                    read_problem(c, tptp, sel, out, depth + 1)
                    break
            else:
                raise Fail("cannot resolve include %s" % fname)
            continue
        if kind not in ("fof", "cnf"):
            raise Fail("unsupported record kind %s" % kind)
        nk, name = p.next()
        p.expect(",")
        rk, role = p.next()
        if rk != "name":
            raise Fail("role expected")
        p.expect(",")
        f = p.logic()
        if p.peek()[0] != "eof" and not p.is_op(","):
            raise Fail("trailing text in record %s" % name)
        if selection is not None and name not in selection:
            continue
        out.append((kind, name, role, f))
    return out


# --------------------------------------------------------------------------
# model reading (Vampire-style FiniteModel block)
# --------------------------------------------------------------------------
def read_model(block):
    decls = {}        # symbol -> (arity, result type)
    domain = None
    defs = []         # formulas to read as definitions
    for kind, body in split_records(block):
        if kind != "tff":
            raise Fail("model record kind %s" % kind)
        p = Parser(body)
        name = p.next()[1]
        p.expect(",")
        role = p.next()[1]
        p.expect(",")
        if role == "type":
            sk, sym = p.next()
            p.expect(":")
            arity, res = p.type_expr()
            if sym in decls:
                raise Fail("symbol %s declared twice" % sym)
            decls[sym] = (arity, res)
            continue
        if role != "axiom":
            raise Fail("model role %s" % role)
        f = p.logic()
        if name.startswith("finite_domain"):
            if f[0] != "all" or len(f[1]) != 1:
                raise Fail("finite_domain shape")
            v = f[1][0]
            body_ = f[2]
            alts = body_[1] if body_[0] == "or" else (body_,)
            elems = []
            for a in alts:
                if a[0] != "eq" or a[1] != ("var", v) or a[2][0] != "app" or a[2][2]:
                    raise Fail("finite_domain literal shape")
                elems.append(a[2][1])
            if len(set(elems)) != len(elems) or domain is not None:
                raise Fail("finite_domain elements")
            domain = elems
        elif name.startswith("distinct_domain"):
            pass
        else:
            defs.append(f)
    if domain is None:
        raise Fail("no finite_domain axiom")
    index = {e: k for k, e in enumerate(domain)}
    for e in domain:
        if decls.get(e) != (0, "$i"):
            raise Fail("domain element %s not declared as $i" % e)
    funcs, preds = {}, {}
    for sym, (arity, res) in decls.items():
        if sym in index:
            continue
        if res == "$i":
            funcs[(sym, arity)] = {}
        elif res == "$o":
            preds[(sym, arity)] = {}
        else:
            raise Fail("type %s" % res)

    def elt(t):
        if t[0] != "app" or t[2] or t[1] not in index:
            raise Fail("non-element argument %r" % (t,))
        return index[t[1]]

    def define(lit):
        neg = False
        while lit[0] == "not":
            neg = not neg
            lit = lit[1]
        if lit[0] == "eq":
            if neg:
                raise Fail("negated function definition")
            lhs, rhs = lit[1], lit[2]
            if lhs[0] != "app":
                raise Fail("definition lhs")
            key = (lhs[1], len(lhs[2]))
            if key not in funcs:
                raise Fail("undeclared function %s/%d" % key)
            args = tuple(elt(a) for a in lhs[2])
            if args in funcs[key]:
                raise Fail("function %s/%d defined twice" % key)
            funcs[key][args] = elt(rhs)
        elif lit[0] == "pred":
            key = (lit[1], len(lit[2]))
            if key not in preds:
                raise Fail("undeclared predicate %s/%d" % key)
            args = tuple(elt(a) for a in lit[2])
            if args in preds[key]:
                raise Fail("predicate %s/%d defined twice" % key)
            preds[key][args] = not neg
        else:
            raise Fail("definition shape %r" % (lit[0],))

    for f in defs:
        for lit in (f[1] if f[0] == "and" else (f,)):
            define(lit)
    n = len(domain)
    for table in list(funcs.items()) + list(preds.items()):
        (sym, arity), tab = table
        if len(tab) != n ** arity:
            raise Fail("partial table for %s/%d" % (sym, arity))
    return n, funcs, preds


# --------------------------------------------------------------------------
# evaluation
# --------------------------------------------------------------------------
def signature(f, fs, ps):
    tag = f[0]
    if tag == "not":
        signature(f[1], fs, ps)
    elif tag in ("and", "or"):
        for g in f[1]:
            signature(g, fs, ps)
    elif tag == "bin":
        signature(f[2], fs, ps)
        signature(f[3], fs, ps)
    elif tag in ("all", "ex"):
        signature(f[2], fs, ps)
    elif tag == "eq":
        term_sig(f[1], fs)
        term_sig(f[2], fs)
    elif tag == "pred":
        ps.add((f[1], len(f[2])))
        for a in f[2]:
            term_sig(a, fs)


def term_sig(t, fs):
    if t[0] == "special":
        raise Fail("number / distinct object in the problem")
    if t[0] == "app":
        if t[1].startswith("$"):
            raise Fail("defined function %s" % t[1])
        fs.add((t[1], len(t[2])))
        for a in t[2]:
            term_sig(a, fs)


def free_vars(f, bound=frozenset()):
    tag = f[0]
    if tag == "not":
        return free_vars(f[1], bound)
    if tag in ("and", "or"):
        return set().union(*[free_vars(g, bound) for g in f[1]])
    if tag == "bin":
        return free_vars(f[2], bound) | free_vars(f[3], bound)
    if tag in ("all", "ex"):
        return free_vars(f[2], bound | set(f[1]))
    if tag == "eq":
        return term_vars(f[1], bound) | term_vars(f[2], bound)
    if tag == "pred":
        return set().union(set(), *[term_vars(a, bound) for a in f[2]])
    return set()


def term_vars(t, bound):
    if t[0] == "var":
        return set() if t[1] in bound else {t[1]}
    if t[0] == "app":
        return set().union(set(), *[term_vars(a, bound) for a in t[2]])
    return set()


class Model:
    def __init__(self, n, funcs, preds):
        self.n, self.funcs, self.preds = n, funcs, preds

    def term(self, t, env):
        if t[0] == "var":
            return env[t[1]]
        return self.funcs[(t[1], len(t[2]))][tuple(self.term(a, env) for a in t[2])]

    def holds(self, f, env):
        tag = f[0]
        if tag == "const":
            return f[1]
        if tag == "not":
            return not self.holds(f[1], env)
        if tag == "and":
            return all(self.holds(g, env) for g in f[1])
        if tag == "or":
            return any(self.holds(g, env) for g in f[1])
        if tag == "bin":
            op, a, b = f[1], f[2], f[3]
            if op == "=>":
                return (not self.holds(a, env)) or self.holds(b, env)
            if op == "<=":
                return self.holds(a, env) or not self.holds(b, env)
            x, y = self.holds(a, env), self.holds(b, env)
            if op == "<=>":
                return x == y
            if op == "<~>":
                return x != y
            if op == "~|":
                return not (x or y)
            if op == "~&":
                return not (x and y)
        if tag in ("all", "ex"):
            vs, body = f[1], f[2]
            want = tag == "all"
            for vals in itertools.product(range(self.n), repeat=len(vs)):
                e = dict(env)
                e.update(zip(vs, vals))
                if self.holds(body, e) != want:
                    return not want
            return want
        if tag == "eq":
            return self.term(f[1], env) == self.term(f[2], env)
        if tag == "pred":
            return self.preds[(f[1], len(f[2]))][tuple(self.term(a, env) for a in f[2])]
        raise Fail("cannot evaluate %r" % (tag,))


def main(argv):
    args = argv[1:]
    tptp = os.environ.get("TPTP", "")
    if "--tptp" in args:
        k = args.index("--tptp")
        tptp = args[k + 1]
        del args[k:k + 2]
    if len(args) != 2:
        print(__doc__)
        return 2
    prob, sol = args
    try:
        with open(sol, encoding="utf-8", errors="replace") as fh:
            out = fh.read()
        m = re.search(r"^% SZS status (\S+) for ", out, re.M)
        if not m:
            raise Fail("no SZS status line")
        status = m.group(1)
        if status not in ("Satisfiable", "CounterSatisfiable"):
            raise Fail("status %s is not a model claim" % status)
        b = re.search(r"^% SZS output start FiniteModel for .*?\n(.*?)^% SZS output end FiniteModel for ",
                      out, re.M | re.S)
        if not b:
            raise Fail("no FiniteModel block")
        if out.find("% SZS status") < out.find("% SZS output start"):
            pass
        else:
            raise Fail("status line after the model block")
        n, funcs, preds = read_model(b.group(1))
        recs = read_problem(prob, tptp)
        conj = [r for r in recs if r[2] == "conjecture"]
        for r in recs:
            if r[2] not in ("axiom", "hypothesis", "definition", "assumption", "lemma",
                            "theorem", "corollary", "conjecture", "negated_conjecture",
                            "plain"):
                raise Fail("role %s" % r[2])
            if r[2] == "conjecture" and r[0] != "fof":
                raise Fail("cnf conjecture")
        if len(conj) > 1:
            raise Fail("several conjectures")
        want = "CounterSatisfiable" if conj else "Satisfiable"
        if status != want:
            raise Fail("status %s, the problem needs %s" % (status, want))
        fs, ps = set(), set()
        for r in recs:
            signature(r[3], fs, ps)
        for key in fs:
            if key not in funcs:
                raise Fail("function %s/%d not interpreted" % key)
        for key in ps:
            if key not in preds:
                raise Fail("predicate %s/%d not interpreted" % key)
        extra = (set(funcs) - fs) | (set(preds) - ps)
        names_f = {s for s, _ in fs}
        names_p = {s for s, _ in ps}
        if names_f & names_p:
            raise Fail("symbol used as function and predicate: %s" % sorted(names_f & names_p)[0])
        model = Model(n, funcs, preds)
        for kind, name, role, f in recs:
            fv = sorted(free_vars(f))
            closed = ("all", tuple(fv), f) if fv else f
            val = model.holds(closed, {})
            if role == "conjecture":
                if val:
                    raise Fail("conjecture %s is true in the model" % name)
            elif not val:
                raise Fail("%s record %s is false in the model" % (role, name))
        print("VERIFIED %s domain=%d records=%d functions=%d predicates=%d extra_symbols=%d"
              % (status, n, len(recs), len(fs), len(ps), len(extra)))
        return 0
    except Fail as e:
        print("FAIL: %s" % e)
        return 1
    except (KeyError, RecursionError, UnicodeDecodeError, OSError) as e:
        print("FAIL: %s: %s" % (type(e).__name__, e))
        return 1


if __name__ == "__main__":
    import threading
    # Deeply nested formulas recurse deeply: run on a large stack.
    threading.stack_size(512 * 1024 * 1024)
    rc = []
    th = threading.Thread(target=lambda: rc.append(main(sys.argv)))
    th.start()
    th.join()
    sys.exit(rc[0] if rc else 1)
