# SMT: Congruence Closure and Lazy DPLL(T)

A practitioner's tour of thvm's quantifier-free SMT (Satisfiability Modulo
Theories) surface. Covers the QF_UF (Quantifier-Free first-order
Uninterpreted Function symbols) theory solver, the lazy DPLL(T)
(Davis-Putnam-Logemann-Loveland with theory consultation) shell on top of
it, the TPTP (Thousands of Problems for Theorem Provers) input overloads,
and the `Method -> "SMT"` short-circuit in `TFindProof`.

Abbreviations used throughout (spelled out on first appearance):

- SMT: Satisfiability Modulo Theories.
- QF_UF: the SMT-LIB fragment of quantifier-free first-order equality
  with uninterpreted function symbols.
- CC: Congruence Closure (the standard QF_UF decision procedure).
- DPLL(T): the lazy SMT architecture of a propositional SAT solver
  (DPLL/CDCL) paired with a theory solver T that consulted at each
  candidate model.
- DST: the Downey-Sethi-Tarjan congruence-closure algorithm: union-find
  over subterms + per-class use-lists + congruence propagation on merge.
- CDCL: Conflict-Driven Clause Learning (the modern flavour of DPLL).
- ATP: Automated Theorem Prover.

## 1. Why SMT alongside an equational saturator

`TFindProof` (see `docs/tutorial/atp_methods.md`) is an unfailing
Knuth-Bendix completion engine: it handles universally-quantified
equational axioms by saturating critical pairs (CPs). That is the right
tool when axioms have free variables (e.g. `and(X, Y) = and(Y, X)`).

When the conjecture and every axiom are *ground* (no variables), the
problem collapses to QF_UF: a finite system of equalities and
disequalities over a fixed term universe. Congruence closure decides
that in near-linear time, whereas a saturator may spin on irrelevant
critical pairs until the wall budget runs out.

`Method -> "SMT"` exposes that fast path. For genuinely first-order
inputs `TFindProof` (the default `Method -> Automatic`) remains the
right entry; SMT is a focused short-circuit for the ground fragment and
for Boolean combinations of equality atoms (which the saturator does
not handle at all).

## 2. The three entries

### 2.1 `TSatEUF[eqs, diseqs]` -- raw decision procedure

```mathematica
THVMLink`SMT`TSatEUF[
    {a == b, b == c},
    {a != c}
]
(* <|"Status" -> "UNSAT", "Witness" -> a != c|> *)

THVMLink`SMT`TSatEUF[
    {a == b},
    {f[a] != f[b]}
]
(* <|"Status" -> "UNSAT", "Witness" -> f[a] != f[b]|> *)
```

Returns `<|"Status" -> "UNSAT", "Witness" -> diseq|>` when the union of
equalities collapses some disequality, or
`<|"Status" -> "SAT", "Classes" -> {{...}, ...}|>` with the inferred
equivalence classes of every subterm seen.

Under the hood: the Downey-Sethi-Tarjan algorithm. Each subterm starts
in its own union-find class with a use-list of compound parents whose
arguments mention it. Each merge unions two classes and propagates
congruence: for every pair `(f(x1,...,xn), f(y1,...,yn))` in the merged
class's use-list with matching head and arity, if all `xi` and `yi`
are equivalent, the parents merge too. Path-compressed find + union-by-
rank give the standard near-linear bound.

### 2.2 `TSmtDecide[formula]` -- DPLL(T) over equality atoms

Boolean combinations of `Equal` and `Unequal` atoms via `And`, `Or`,
`Not`, `Implies`, `Equivalent`, `Xor`:

```mathematica
THVMLink`SMT`TSmtDecide[
    (a == b || b == c) && a != b && b != c
]
(* <|"Status" -> "UNSAT"|> *)

THVMLink`SMT`TSmtDecide[
    ((a == b && c == d) || x == y) &&
        f[a, c] != f[b, d] && x != y
]
(* <|"Status" -> "UNSAT"|> -- the T-solver fires inside the
   (a==b && c==d) branch and discovers f[a,c]=f[b,d]; the
   x==y branch is excluded by the diseq. *)

THVMLink`SMT`TSmtDecide[(a == b || c == d) && c == e]
(* <|"Status" -> "SAT", "Model" -> <|a == b -> True, ...|>|> *)
```

Algorithm: each equality/disequality atom is abstracted as a fresh
propositional variable. The Boolean abstraction is handed to Wolfram's
built-in `SatisfiabilityInstances` as the propositional kernel. Every
candidate model is theory-checked via `TSatEUF`. On T-conflict, the
exact assignment is forbidden by a blocking clause and the kernel is
re-queried. Loop terminates: there are `2^|atoms|` assignments and
each iteration prunes one.

This is the standard lazy SMT architecture. It is sound and complete
for QF_UF.

### 2.3 `TFindProofSMT[goal, hypotheses]` -- entailment surface

Reduces an entailment `H1, ..., Hn |= G` to a satisfiability query
on `H1 /\ ... /\ Hn /\ ~G`:

```mathematica
THVMLink`SMT`TFindProofSMT[a == c, {a == b, b == c}]
(* <|"Status" -> "Proved", "Method" -> "CongruenceClosure",
     "Goal" -> a == c, "Hypotheses" -> {...}, "Witness" -> ...|> *)

THVMLink`SMT`TFindProofSMT[Implies[a == b && b == c, a == c]]
(* <|"Status" -> "Proved", "Method" -> "DPLL(T)+CongruenceClosure",
     ...|> *)

THVMLink`SMT`TFindProofSMT[a == c, {a == b}]
(* $Failed -- counter-model exists *)
```

The `Method` field discloses which engine handled it: pure CC for
equality literal goals, DPLL(T)+CC for Boolean-combination goals.

## 3. TPTP overloads

`TFindProofSMT[File["...p"]]` and `TFindProofSMT["...cnf/fof string..."]`
parse a TPTP fragment via `THVMLink\`TPTPImport\`tptpImport` (see
`wl/THVMLink/Kernel/ATP/TPTPImport.wl`) and dispatch the same way:

```mathematica
THVMLink`SMT`TFindProofSMT[
    "cnf(a1, axiom, a = b).
     cnf(a2, axiom, b = c).
     cnf(g, negated_conjecture, a != c)."]
(* <|"Status" -> "Proved", ...|> *)

THVMLink`SMT`TFindProofSMT[
    "fof(a1, axiom, a = b).
     fof(g, negated_conjecture, f(a) != f(b))."]
(* <|"Status" -> "Proved", ...|> *)
```

A ground-gate rejects inputs with universally-quantified variables
(Pattern/Blank heads) -- congruence closure is a quantifier-free
decision procedure:

```mathematica
THVMLink`SMT`TFindProofSMT[
    "cnf(a1, axiom, and(X, Y) = and(Y, X)).
     cnf(g, negated_conjecture, and(a, b) != and(b, a))."]
(* TFindProofSMT::nonground: TFindProofSMT skipping non-ground input ...
   $Failed *)
```

The user is pointed at `TFindProof` for variable-bearing axioms (the
parser succeeded; only the SMT dispatch refused).

## 4. `Method -> "SMT"` in `TFindProof`

A single suboption turns the standard `TFindProof` call into an SMT
dispatch:

```mathematica
TFindProof[a == c, {a == b, b == c}, Method -> "SMT"]
(* <|"Status" -> "Proved", "Method" -> "CongruenceClosure", ...|> *)

TFindProof[
    Implies[a == b && b == c, a == c],
    {}, Method -> "SMT"]
(* <|"Status" -> "Proved", "Method" -> "DPLL(T)+CongruenceClosure", ...|> *)
```

The default `TFindProof` call with no `Method` still goes to the
saturator and returns a regular WL `ProofObject`. `Method -> "SMT"`
returns the SMT Association directly, not a `ProofObject` -- the result
shape is different on purpose, since the SMT-style witness (a
disequality whose two sides collapsed) is not a proof tree.

## 5. When to reach for which

| Input shape                                | Use                            |
|--------------------------------------------|--------------------------------|
| Ground equational, single goal             | `Method -> "SMT"` (or `TFindProofSMT`) |
| Ground, Boolean combination of (in)equalities | `TSmtDecide` / `TFindProofSMT` |
| Variable-bearing equational axioms         | `TFindProof` (default)         |
| TPTP UEQ benchmark with universal vars     | `TFindProof[File["...p"]]`     |
| TPTP ground problem                        | `TFindProofSMT[File["...p"]]`  |

## 6. Scope and limits

What is decided:

- QF_UF (ground first-order equality + uninterpreted functions). Full
  decision procedure: returns SAT or UNSAT in finite time.
- Boolean combinations of equality atoms via lazy DPLL(T). Same.

What is NOT decided:

- Quantified formulas (use `TFindProof` for the equational fragment).
- Linear arithmetic, arrays, bit-vectors -- separate theory solvers
  not yet ported. Could be added behind the same DPLL(T) shell.
- Anything where the propositional structure has exponentially many
  models: the lazy approach pays for each one. Future work: an eager
  T-propagation pass that pushes congruence facts into the SAT kernel
  as additional clauses.

## 7. Where the code lives

- `wl/THVMLink/Kernel/ATP/SMT.wl` -- the package. `TSatEUF`,
  `TSmtDecide`, `TFindProofSMT`, the TPTP overloads, the DPLL(T)
  loop, and the DST procedure.
- `wl/THVMLink/Kernel/ATP.wl` -- the `Method -> "SMT"` guard in
  `TFindProof`.
- `wl/THVMLink/Tests/atp_smt.wlt` -- 27 `VerificationTest`s covering
  every API entry and every gate.
- `wl/THVMLink/Kernel/ATP/TPTPImport.wl` -- the TPTP parser the
  string/File overloads share with `TFindProof`.
