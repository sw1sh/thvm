# WolframInstitute`ATP`

Completed equational systems from thvm's prover, for simplifiers that ask
many normal-form questions of one theory. `TAtpComplete` completes the axioms
once by unfailing Knuth-Bendix completion (Waldmeister's configuration) and
keeps the engine state; `TAtpNormalForm` and `TAtpEqualQ` answer from it by
ordered rewriting without completing again. The functions are THVMLink's
(`wl/THVMLink/Kernel/ATP/AtpSystem.wl`), standalone: the package needs only
its library and loads beside THVMLink in one kernel.

```wl
PacletDirectoryLoad["wl/ATP"]; Get["WolframInstitute`ATP`"]
sys = TAtpComplete[{ForAll[{x, y}, h[x, y] == h[y, x]], ForAll[{x, y, z}, h[h[x, y], z] == h[x, h[y, z]]]}];
TAtpNormalForm[sys, h[c, h[b, a]]]      (* h[a, h[b, c]] *)
TAtpEqualQ[sys, h[a, b], h[b, a]]       (* True *)
```

## API

| Function | Result |
| --- | --- |
| `TAtpComplete[axioms]`, `TAtpComplete[axioms, order]`, `TAtpComplete["Theory"]` | a `TAtpSystem` |
| `sys["Rules"]` | rules `l -> r` and unorientable equations `l <-> r` |
| `sys["Ordering"]` | `<|"Ordering", "Precedence", "SymbolWeights"|>` |
| `sys["Status"]` | `"Saturated"` or `"BudgetExhausted"` |
| `sys["Complete"]` | whether the system is ground convergent |
| `TAtpNormalForm[sys, term]`, `TAtpNormalForm[sys, {terms}]` | normal forms |
| `TAtpEqualQ[sys, a, b]` | `True`, `Failure["NotEntailed", ...]` (complete system, distinct normal forms), or `Indeterminate`; never `False` |

- Axioms are equations, quantified with `ForAll` or written with patterns.
- `order` is a precedence list `{s1, s2, ...}` for the lexicographic path
  ordering or `{{s1, w1}, ...}` for the Knuth-Bendix ordering with weights. The
  default is the Knuth-Bendix ordering with weight 1 for every symbol and W15's
  precedence: the axioms' function symbols by descending arity, then their
  constants.
- A query's symbols and numbers outside the axioms become constants ranked
  below every axiom symbol, in canonical order, so a normal form does not
  depend on how the query is written.
- Options of `TAtpComplete`: `MaxSteps` (2000 selected equations),
  `"AllocationLimit"` (2^24 heap cells, about a second), `TimeConstraint`
  (`Infinity`). Both budgets end the completion cleanly with the system so
  far, at the same point on every machine.

## Library

`libthvm_atp` (`CSource/libthvm_atp.c`) is one translation unit over thvm's
runtime (`src/thvm.c`) and the prover's bridge
(`wl/THVMLink/CSource/thvmlink_atp.c`), without THVMLink's own entries, so the
tensor and GPU code is left out. It needs a C11 compiler and the LibraryLink
headers, and links only the C library (plus Accelerate on macOS).

```bash
make atp-lib     # wl/ATP/LibraryResources/$SystemID/libthvm_atp.{dylib,so}
```

Built with clang it is 1.4 MB on macOS arm64 and 1.7 MB on Linux x86-64
(clang 14). The package loads it from `LibraryResources/$SystemID` next to
`Kernel/`, and sets `THVM_HEAP_CELLS` to THVMLink's Waldmeister heap
(536870912 cells, address space only) unless it is set.

## Determinism

`CSource/determinism.c` runs the package's completions and queries through
the library's own entries without a Wolfram kernel and prints every
completed system and normal form in prefix form:

```bash
make bin/atp_lib_determinism
bin/atp_lib_determinism cases.txt > out.txt
```

`WolframInstitute`ATP`Private`determinismCase[name, axioms, order, queries]`
writes a case with exactly the arguments the package sends. On the 19 cases
of v2's FullSimplify benchmark plus five more (AC, the group under three
orderings, Boolean algebra), the output is byte-identical on macOS arm64 and
Linux x86-64 with the same `THVM_HEAP_CELLS`, budget stopping points
included.

## Tests

`Tests/ATP.wlt`, run with the package loaded (and THVMLink too, for the
agreement test).
