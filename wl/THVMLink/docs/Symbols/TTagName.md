---
Template: Symbol
Name: TTagName
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TTagName
Keywords: [tag, name, string, term, inspector]
SeeAlso: [TTermTag, TTerm, Term]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TTagName]()[*tag*]</code> returns a `String` name for a numeric tag id (`"LAM"`, `"APP"`, `"SUP"`, ...).

## Details & Options

- Inverse of the `$Tag*` constants in `Kernel/THVMLink.wl`: `$TagLAM = 1`, `$TagSUP = 6`, ...
- The `*t*["tagName"]` indexer is an alias for <code>[TTagName]()[[TTermTag]()[*t*]]</code>.

## Basic Examples

```wl
TTagName[1]
```
<!-- => "LAM" -->

## Properties and Relations

[TTermTag]() gives the numeric tag of a `TTerm`.
