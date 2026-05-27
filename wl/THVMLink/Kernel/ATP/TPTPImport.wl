(* TPTPImport.wl - thvm wrapper around the standalone TPTPImport
   resource implementation (TPTPImport.resource.wl in this same
   directory).

   The resource file is the self-contained, package-shell-free
   definition of `TPTPImport`; it is also the body the WFR
   submission notebook scrapes via the `#| file:` inline directive
   in the sibling `TPTPImport.md` authoring document.  Keeping the
   implementation in one literal file means thvm and the public
   Function Repository resource share a single source of truth.

   This wrapper pre-declares the public `TPTPImport` symbol in
   `THVMLink`ATP``, then `Get`s the resource from inside `Private`
   so the parser's helpers (parseClauses, readTerm, ...) land in
   the private context while `TPTPImport` resolves to the public
   symbol via the active context path. *)

BeginPackage["THVMLink`ATP`", {"THVMLink`"}];

TPTPImport::usage =
    "TPTPImport[File[\"file.p\"]] | TPTPImport[\"... source ...\"] " <>
    "returns <|\"Axioms\" -> {...}, \"Conjecture\" -> ...|>.  Function " <>
    "symbols come back as String-headed terms (\"and\"[X, Y] etc.) so " <>
    "they cannot collide with user-level WL symbols.  Handles cnf, " <>
    "fof, tff, tcf, thf, ncf clause heads (including multi-literal " <>
    "cnf disjunctions, the full fof Boolean grammar, thf lambdas + " <>
    "@-application, sequents) plus include directives with optional " <>
    "clause-name selectors.";

Begin["`Private`"];

Get @ FileNameJoin[{DirectoryName[$InputFileName],
    "TPTPImport.resource.wl"}];

End[];

EndPackage[];
