---
Template: Symbol
Name: TTerm
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TTerm
Keywords: [TTerm, packed term, handle, summary box, term fields]
SeeAlso: [TTermTag, TTermExt, TTermVal, TTermSub, TTagName, Term]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TTerm]()[*raw*]</code> represents the runtime term whose packed 64-bit value is the integer *raw*.

<!-- #| annotation: 26.10.19: The full form is TTerm[ctx, raw, ExternPin[n]]: TTerm[raw] fills in the current context slot and a fresh pin handle, which keeps the term reachable for the runtime's garbage collector while Wolfram Language holds the handle. The summary box reads only the packed bits, never the heap. -->

## Details & Options

- A [TTerm]() is what every term constructor gives: [TLam](), [TApp](), [TSup](), [TDup](), [TEra](), [TNum](), [TOp2]() and [TVarFor]().
- A [TTerm]() displays as a summary box with its tag, its location or payload, and its EXT field; a number term shows its value and dtype instead.
- The packed value has four fields: a 1-bit SUB flag, a 7-bit tag, an 18-bit EXT field and a 38-bit VAL field, read by [TTermSub](), [TTermTag](), [TTermExt]() and [TTermVal]().
- <code>*t*["*prop*"]</code> gives one field of the term *t*. The properties are `"tag"`, `"tagName"`, `"ext"`, `"val"`, `"sub"`, `"raw"` (the packed value) and `"ctx"` (the slot of the runtime [TContext]() the term was built in).
- <code>*t*[*arg*]</code> gives the application <code>[TApp]()[*t*, *arg*]</code>.
- Two handles to the same packed value are [Equal]() but not [SameQ](), since each handle carries its own reference.
- [TReset]() rewinds the heap, so a handle made before it refers to cells that later terms reuse.

## Basic Examples

A packed value with the NUM tag wraps as a term holding the number 42:

```wl
TTerm[720577314768814122]
```

<!-- => a TNum summary box showing number 42 and dtype i32 -->

---

Every term constructor gives a [TTerm]():

```wl
TLam[x, x]
```

<!-- => a TTerm summary box showing tag LAM and ext 0 -->

---

Bind a number term:

```wl
t = TNum[42]
```

<!-- => a TNum summary box showing number 42 and dtype i32 -->

Its tag is the integer 10:

```wl
t["tag"]
```

<!-- => 10 -->

The tag name:

```wl
t["tagName"]
```

<!-- => "NUM" -->

The VAL field holds the number:

```wl
t["val"]
```

<!-- => 42 -->

The packed value is the integer the term wraps:

```wl
t["raw"]
```

<!-- => 720577314768814122 -->

## Scope

Bind a number term:

```wl
t = TNum[42]
```

<!-- => a TNum summary box showing number 42 and dtype i32 -->

For a number the EXT field is the dtype code, 5 for `"i32"`:

```wl
t["ext"]
```

<!-- => 5 -->

A freshly built term has its SUB flag clear:

```wl
t["sub"]
```

<!-- => 0 -->

The term belongs to the default runtime context, slot 0:

```wl
t["ctx"]
```

<!-- => 0 -->

---

Bind a lambda:

```wl
f = TLam[x, x + 1]
```

<!-- => a TTerm summary box showing tag LAM -->

Applying a term to an argument builds an application:

```wl
f[TNum[41]]["tagName"]
```

<!-- => "APP" -->

---

Applying a lambda and reducing the application gives its value:

```wl
TTermVal[TWnf[TLam[x, x + 1][TNum[41]]]]
```

<!-- => 42 -->

## Properties and Relations

Bind a lambda:

```wl
f = TLam[x, x]
```

<!-- => a TTerm summary box showing tag LAM -->

Wrapping its packed value again gives an equal term:

```wl
TTerm[f["raw"]] == f
```

<!-- => True -->

The two handles are not identical, since each carries its own reference:

```wl
TTerm[f["raw"]] === f
```

<!-- => False -->

---

The tag name is [TTagName]() of the tag:

```wl
TTagName[TNum[42]["tag"]]
```

<!-- => "NUM" -->

## Possible Issues

Start from an empty heap:

```wl
TReset[];
```

Bind a lambda, which takes the first heap cell:

```wl
f = TLam[x, x + 1]
```

<!-- => a TTerm summary box showing tag LAM -->

Reset the heap again:

```wl
TReset[];
```

A new term reuses the first heap cell:

```wl
g = TLam[y, TNum[7]]
```

<!-- => a TTerm summary box showing tag LAM -->

The handle made before the reset now reads the new term's body:

```wl
Term[f]
```

<!-- => Term["LAM", 0, Term["NUM", 7]] -->
