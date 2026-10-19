---
Template: Symbol
Name: TWnf
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TWnf
Keywords: [wnf, reduce, head, weak normal form, interaction]
SeeAlso: [TStep, TStack, TReduce, TNf, TCnf, TCollapse, TItrs]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TWnf]()[*term*]</code> reduces *term* to weak normal form and gives the reduced term.

<code>[TWnf]()[*term*, *n*]</code> stops after at most *n* interactions and gives the partly reduced term.

## Details & Options

- Weak normal form reduces only the head: interactions fire at the head position until it is a value such as a number, a lambda, a superposition or a constructor. The body of a lambda and the branches of a superposition are left as they are.
- Each interaction that fires, such as an application of a lambda, an integer operation, a duplication or a match, adds 1 to [TItrs]().
- Reduction rewrites the heap in place and consumes *term*. To continue, reduce the term [TWnf]() gives, not *term* again.
- With a budget *n*, the reduction stops after *n* interactions. [TStack]() then gives the eliminators still waiting on the head, and [TWnf]() of the result continues where it stopped. A budget of 0 means no limit.
- [TStep]() is <code>[TWnf]()[*term*, 1]</code>. [TNf]() reduces everywhere, [TCnf]() also lifts a superposition to the top, and [TCollapse]() lists the values of a superposition.

## Basic Examples

Reduce an application of a lambda:

```wl
TWnf[TApp[TLam[x, x + 1], TNum[41]]]
```

<!-- => a TNum summary box showing number 42 and dtype i32 -->

---

The value of the reduced term:

```wl
TTermVal[TWnf[TApp[TLam[x, x + 1], TNum[41]]]]
```

<!-- => 42 -->

## Scope

Start from an empty heap so the interaction counter starts at 0:

```wl
TReset[];
```

Stop after one interaction:

```wl
r = TWnf[TApp[TLam[x, x + 1], TNum[41]], 1]
```

<!-- => a TTerm summary box showing tag OP2 -->

The application has fired, and the sum is still to be done:

```wl
Term[r]
```

<!-- => Term["OP2", "+", Term["NUM", 41], Term["NUM", 1]] -->

One interaction has fired:

```wl
TItrs[]
```

<!-- => 1 -->

Reducing the result continues where the reduction stopped:

```wl
TTermVal[TWnf[r]]
```

<!-- => 42 -->

The sum was the second interaction:

```wl
TItrs[]
```

<!-- => 2 -->

---

A budget of 0 means no limit:

```wl
TTermVal[TWnf[TApp[TLam[x, x + 1], TNum[41]], 0]]
```

<!-- => 42 -->

---

The branches of a superposition are left as they are:

```wl
Term[TWnf[TSup[9, TNum[1] + TNum[2], 5]]]
```

<!-- => Term["SUP", 9, Term["OP2", "+", Term["NUM", 1], Term["NUM", 2]], Term["NUM", 5]] -->

---

Start from an empty heap so the binder location is predictable:

```wl
TReset[];
```

The body of a lambda is left as it is:

```wl
Term[TWnf[TLam[y, TNum[1] + TNum[2]]]]
```

<!-- => Term["LAM", 0, Term["OP2", "+", Term["NUM", 1], Term["NUM", 2]]] -->

## Properties and Relations

[TNf]() reduces inside the branches too:

```wl
Term[TNf[TSup[9, TNum[1] + TNum[2], 5]]]
```

<!-- => Term["SUP", 9, Term["NUM", 3], Term["NUM", 5]] -->

---

[TStep]() is [TWnf]() with a budget of one interaction:

```wl
Term[TStep[TNum[3] + TNum[4]]] === Term[TWnf[TNum[3] + TNum[4], 1]]
```

<!-- => True -->

---

Start from an empty heap:

```wl
TReset[];
```

A number is already in weak normal form:

```wl
TWnf[TNum[5]] == TNum[5]
```

<!-- => True -->

Reducing it fires nothing:

```wl
TItrs[]
```

<!-- => 0 -->

## Possible Issues

Bind a lambda:

```wl
f = TLam[x, x + 1]
```

<!-- => a TTerm summary box showing tag LAM -->

Applying it and reducing gives 42:

```wl
TTermVal[TWnf[TApp[f, TNum[41]]]]
```

<!-- => 42 -->

The reduction consumed the lambda, so a second application reads the substituted body instead of adding 1:

```wl
TTermVal[TWnf[TApp[f, TNum[1]]]]
```

<!-- => 41 -->

---

Duplicate a lambda with [TDup]() to apply it twice:

```wl
{g0, g1} = TDup[TLam[x, x + 1]]
```

<!-- => a list of two TTerm summary boxes, tags DP0 and DP1 -->

The first copy adds 1 to 41:

```wl
TTermVal[TWnf[TApp[g0, TNum[41]]]]
```

<!-- => 42 -->

The second copy adds 1 to 1:

```wl
TTermVal[TWnf[TApp[g1, TNum[1]]]]
```

<!-- => 2 -->
