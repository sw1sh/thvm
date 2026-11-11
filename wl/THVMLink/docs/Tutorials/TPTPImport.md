---
Template: TechNote
Name: TPTPImport
Title: Importing TPTP Benchmarks
Context: WolframInstitute`THVMLink`ATP`
ContextPath: [Global`, Wolfram`Parser`]
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/tutorial/TPTPImport
Keywords: [TPTP, CNF, FOF, benchmark, import, theorem proving, SZS, AbelianGroupAxioms]
RelatedGuides: [THVMLink]
RelatedTutorials: [ATP, SMT, Disproof]
---

The TPTP (Thousands of Problems for Theorem Provers) library is the standard collection of problems on which automated theorem provers such as Vampire, E, Twee and Waldmeister are compared, and its text format is how those provers exchange problems and proofs. [TPTPImport](paclet:Wolfram/WolframParser/ref/TPTPImport), from the WolframParser paclet (load it with `Needs["Wolfram`Parser`"]`), parses that format into Wolfram Language equations, and [TFindProof]() takes a TPTP problem as text or as a file and proves it:

| | |
| --- | --- |
| <code>[TPTPImport](paclet:Wolfram/WolframParser/ref/TPTPImport)["*text*"]</code> | parse a TPTP problem into its axioms and its conjecture |
| <code>[TPTPImport](paclet:Wolfram/WolframParser/ref/TPTPImport)[[File]()["*file*"]]</code> | parse a TPTP problem file |
| <code>[TFindProof]()["*text*"]</code> | prove the conjecture of a TPTP problem from its axioms |
| <code>[TFindProof]()[[File]()["*file*"]]</code> | prove the problem in a TPTP file |
| <code>[TFindProof]()[*problem*, "TPTP"]</code> | give the proof as a refutation in TPTP format |

## Parsing a problem

A problem in clause normal form (`cnf`) is a list of clauses, each with a name, a role and a formula. This one states the axioms of an abelian group, with the product `mult`, the unit `e` and the inverse `inv`, and negates the conjecture that the inverse of the inverse of `a` is `a`:

```wl
problem = "cnf(associativity, axiom, mult(X, mult(Y, Z)) = mult(mult(X, Y), Z)).
cnf(commutativity, axiom, mult(X, Y) = mult(Y, X)).
cnf(right_identity, axiom, mult(X, e) = X).
cnf(right_inverse, axiom, mult(X, inv(X)) = e).
cnf(inverse_of_inverse, negated_conjecture, inv(inv(a)) != a)."
```

[TPTPImport](paclet:Wolfram/WolframParser/ref/TPTPImport) gives an [Association]() of the axioms and the conjecture:

```wl
parsed = TPTPImport[problem]
```
<!-- => <|"Axioms" -> {"mult"[X_, "mult"[Y_, Z_]] == "mult"["mult"[X_, Y_], Z_], "mult"[X_, Y_] == "mult"[Y_, X_], "mult"[X_, "e"[]] == X_, "mult"[X_, "inv"[X_]] == "e"[]}, "Conjecture" -> "inv"["inv"["a"[]]] == "a"[]|> -->

The variables `X`, `Y` and `Z` of the clauses become the patterns `X_`, `Y_` and `Z_`, so each axiom holds for all values of its variables. A function symbol becomes a string head, and a constant such as `e` becomes a string head with no arguments. The negated conjecture is negated back, so the conjecture is the equation to prove:

```wl
parsed["Conjecture"]
```
<!-- => "inv"["inv"["a"[]]] == "a"[] -->

String heads cannot collide with Wolfram Language symbols: a TPTP problem may use names such as `f`, `c` or `plus` without touching any definition in the session. To work with symbols instead, replace each string head by the symbol of the same name:

```wl
parsed["Conjecture"] //. h_String[args___] :> Symbol[h][args]
```
<!-- => inv[inv[a[]]] == a[] -->

## Proving a problem

[TFindProof]() parses the text itself and proves the conjecture from the axioms:

```wl
TFindProof[problem]
```

The parsed pieces are equations [TFindProof]() accepts directly, which is the way to give a problem other settings, such as a [Method]():

```wl
TFindProof[parsed["Conjecture"], parsed["Axioms"], Method -> "VampireUEQ", TimeConstraint -> 10]
```

A problem in a file is given as <code>[File]()["*file*"]</code>. Write the problem to a temporary file:

```wl
file = Export[FileNameJoin[{$TemporaryDirectory, "InverseOfInverse.p"}], problem, "Text"]
```
<!-- => the path of InverseOfInverse.p in $TemporaryDirectory -->

[TFindProof]() reads and proves it:

```wl
TFindProof[File[file]]
```

The `"TPTP"` spec gives the proof in the format the problem came in: a refutation in the SZS (Sutcliffe-Zimmer-Schulz) output format, in which each derived clause cites the clauses it comes from and the last one is the contradiction `$false`. The problem is named after the file:

```wl
TFindProof[File[file], "TPTP"]
```
<!-- => "% SZS status Unsatisfiable for InverseOfInverse.p\n% SZS output start CNFRefutation for InverseOfInverse.p\ncnf(ax1, axiom, mult(X,mult(Y,Z)) = mult(mult(X,Y),Z)).\n...\ncnf(sl2, plain, inv(inv(X)) = X, inference(rw, [status(thm)], [cpl3, ax3])).\ncnf(contradiction, plain, $false, inference(rw, [status(thm)], [negated_conjecture, sl2])).\n% SZS output end CNFRefutation for InverseOfInverse.p" -->

A problem with no conjecture has nothing to prove, and [TFindProof]() completes its axioms instead. The `"Lemmas"` spec gives the rewrite rules the completion derives:

```wl
TFindProof["cnf(right_identity, axiom, mul(X, e) = X).
cnf(left_identity, axiom, mul(e, X) = X).", "Lemmas", TimeConstraint -> 5]
```
<!-- => {Inactive[Equal][mul[X, e], X], Inactive[Equal][mul[e, X], X]} -->

A conjecture that does not follow from the axioms is not proved. The axiom `f(f(X)) = X` completes to a rewrite system in which `f(a)` and `a` have different normal forms, so the search ends with the status `"Saturated"`:

```wl
TFindProof["cnf(involution, axiom, f(f(X)) = X).
cnf(goal, negated_conjecture, f(a) != a).", "Status", TimeConstraint -> 5]
```
<!-- => "Saturated" -->

The `"Counterexample"` spec gives a model in which the axioms hold and the conjecture fails, here the two-element model in which `f` swaps the elements:

```wl
TFindProof["cnf(involution, axiom, f(f(X)) = X).
cnf(goal, negated_conjecture, f(a) != a).", "Counterexample", TimeConstraint -> 5]
```

## First-order formulas and predicates

A problem in first-order form (`fof`) states its formulas with quantifiers: `![X]:` is "for all X" and `?[X]:` is "there exists X". A universally quantified axiom becomes an equation over patterns, and a `conjecture` clause is the goal itself:

```wl
TPTPImport["fof(involution, axiom, ![X]: f(f(X)) = X).
fof(goal, conjecture, f(f(f(a))) = f(a))."]
```
<!-- => <|"Axioms" -> {"f"["f"[X_]] == X_}, "Conjecture" -> "f"["f"["f"["a"[]]]] == "f"["a"[]]|> -->

[TFindProof]() proves it:

```wl
TFindProof["fof(involution, axiom, ![X]: f(f(X)) = X).
fof(goal, conjecture, f(f(f(a))) = f(a)).", "Status"]
```
<!-- => "Proved" -->

The connectives `&`, `|`, `~`, `=>` and `<=>` become [And](), [Or](), [Not](), [Implies]() and [Equivalent]():

```wl
TPTPImport["fof(all_men_are_mortal, axiom, ![X]: (man(X) => mortal(X))).
fof(socrates_is_a_man, axiom, man(socrates)).
fof(socrates_is_mortal, conjecture, mortal(socrates))."]
```
<!-- => <|"Axioms" -> {Implies["man"[X_], "mortal"[X_]], "man"["socrates"[]]}, "Conjecture" -> "mortal"["socrates"[]]|> -->

A clause problem with predicates is converted to an equational problem, and the proof is a `"Predicate/EquationalLogic"` [ProofObject]():

```wl
TFindProof["cnf(c1, axiom, p(X) | ~q(X)).
cnf(c2, axiom, q(a)).
cnf(c3, negated_conjecture, ~p(a))."]
```

Comments, whether `%` lines or `/* ... */` blocks, and the `inference(...)` annotations that proof output attaches to clauses are skipped:

```wl
TPTPImport["% a comment line
/* a comment block */ cnf(a1, axiom, f(X) = X, inference(rw, [], [])).
cnf(goal, negated_conjecture, f(a) != a)."]
```
<!-- => <|"Axioms" -> {"f"[X_] == X_}, "Conjecture" -> "f"["a"[]] == "a"[]|> -->

## Coverage

| TPTP input | Result |
| --- | --- |
| `cnf` equations and disequations | axioms and a conjecture over patterns |
| `cnf` clauses with predicates | Boolean clauses, which [TFindProof]() converts to an equational problem |
| `fof` with `!` | equations over patterns |
| `fof` with `?` | an [Exists]() formula |
| `fof` connectives | [And](), [Or](), [Not](), [Implies](), [Equivalent]() |
| comments and `inference` annotations | skipped |

The typed form `tff` is not converted to equations, so [TFindProof]() does not prove problems written in it. A ground problem, one with no variables, can also be decided by congruence closure with the `"SMT"` method, as described in [Congruence Closure and Lazy DPLL(T)](paclet:WolframInstitute/THVMLink/tutorial/SMT). The [Parsing TPTP](paclet:Wolfram/WolframParser/tutorial/ParsingTPTP) tutorial of the WolframParser paclet describes the grammar.
