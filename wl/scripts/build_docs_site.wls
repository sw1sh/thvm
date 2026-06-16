#!/usr/bin/env wolframscript
(* ::Package:: *)

(* Build and deploy a custom single-page documentation browser for THVMLink.

   Adapted from PureMath/scripts/build_docs_site.wls.

   Rationale: the stock paclet shingle deploys each documentation page as a
   "delayed embed" that fetches /statichtml/<page>.nb?maxwaitmillis=1000 at
   request time -- one second is not enough to render these heavy notebooks, so
   every page except a pre-warmed one comes back empty.  And the in-content
   links inside those rendered pages point back at the stock embed pages, which
   carry the old sidebar + header.

   So we do NOT use the base HTML.  This script RE-HOSTS every documentation
   page (the guide, the tutorials, and the symbol reference) as a clean,
   self-contained static file under <account>/THVMLinkDocs/{guide,tutorial,ref}/
   <Name>.html (rendered once via the cloud's statichtml endpoint, with internal
   links rewritten to point at our own re-hosted pages), then deploys ONE page --
   index.html -- that frames them: a persistent collapsible tree on the left, a
   single <iframe> in the middle.  Clicking a node only swaps the iframe src, and
   a same-origin click hook inside the iframe routes in-content links through the
   same loader, so navigation never leaves our shell.  Static files serve
   instantly (no per-request render), which is the speed-up.

   The home / main view is the resource definition page (hero + examples),
   loaded as-is from the deployed resource (publish via deploy_paclet.wls first).

   Usage:
     wl -f wl/scripts/build_docs_site.wls          (build + deploy)
     DRYRUN=1 wl -f wl/scripts/build_docs_site.wls (print tree, no fetch/deploy) *)

pacletDir    = FileNameJoin[{ParentDirectory[DirectoryName[$InputFileName]], "THVMLink"}];
docsDir      = FileNameJoin[{pacletDir, "docs"}];
guidesDir    = FileNameJoin[{docsDir, "Guides"}];
symbolsDir   = FileNameJoin[{docsDir, "Symbols"}];
tutorialsDir = FileNameJoin[{docsDir, "Tutorials"}];
webDir       = FileNameJoin[{docsDir, "web"}];

(* ---- config ---- *)
$target      = "THVMLinkDocs";   (* cloud dir for our shell + re-hosted pages, under the account root *)
$resourceRel = "DeployedResources/Paclet/WolframInstitute/THVMLink";  (* where deploy_paclet.wls deploys the paclet *)
$rootGuide   = "THVMLink";
$nbWidth     = 900;
$waitMs      = 25000;
$fetchPar    = 16;
(* $base (the deployed paclet resource) and $base2 (our docs dir) are derived from
   the connected cloud account below, so this runs under any cloud id (e.g. CI). *)

If[ ! DirectoryQ[guidesDir], Print["Guides dir not found: ", guidesDir]; Exit[1] ];

(* ---- doc-page Title from the markdown frontmatter (Title: / Name:), else basename ---- *)
mdTitle[file_] := Module[{m},
    m = StringCases[Import[file, "Text"],
            StartOfLine ~~ ("Title:" | "Name:") ~~ Whitespace... ~~ t : Except["\n"].. :> StringTrim[t], 1];
    If[m === {}, FileBaseName[file], First[m]]
];

(* ---- build the tree: one guide, a Tutorials group, a flat Reference group ---- *)
guideFile     = FileNameJoin[{guidesDir, $rootGuide <> ".md"}];
tutorialFiles = Sort @ FileNames["*.md", tutorialsDir];
symbolFiles   = Sort @ FileNames["*.md", symbolsDir];

tutNode[f_] := <|"name" -> FileBaseName[f], "title" -> mdTitle[f], "type" -> "tutorial"|>;
symNode[f_] := <|"name" -> FileBaseName[f], "title" -> FileBaseName[f], "type" -> "symbol"|>;

topNodes = {
    <|"name" -> $rootGuide, "title" -> mdTitle[guideFile], "type" -> "guide"|>,
    <|"name" -> "Tutorials", "title" -> "Tutorials", "type" -> "group", "open" -> True,
      "children" -> (tutNode /@ tutorialFiles)|>,
    <|"name" -> "Reference", "title" -> "Reference", "type" -> "group", "open" -> False,
      "children" -> (symNode /@ symbolFiles)|>
};

pageType[node_] := Switch[node["type"], "symbol", "ref", "tutorial", "tutorial", _, "guide"];

(* flat list of every page to re-host: {type ("guide"|"tutorial"|"ref"), name} *)
collect[node_] := If[ node["type"] === "group",
    Join @@ (collect /@ node["children"]),
    {{pageType[node], node["name"]}}
];
entries = DeleteDuplicates[Join @@ (collect /@ topNodes)];
Print["build_docs_site: ", Length[entries], " pages (", Length[tutorialFiles], " tutorials, ",
    Length[symbolFiles], " symbols, 1 guide)"];

(* ---- DRYRUN: print the tree and stop (no cloud needed) ---- *)
If[ Environment["DRYRUN"] === "1",
    Module[{show},
        show[n_, d_] := (
            Print[StringRepeat["  ", d],
                Switch[n["type"], "symbol", "- ", "group", "= ", _, "+ "],
                n["name"], If[n["title"] =!= n["name"], " (" <> n["title"] <> ")", ""]];
            show[#, d + 1] & /@ Lookup[n, "children", {}];);
        show[#, 0] & /@ topNodes];
    Print["build_docs_site: DRYRUN, not deploying"]; Exit[0]
];

(* ---- connect to the cloud and derive the account-relative base URLs ----
   $CloudRootDirectory is the account root (CloudObject[".../obj/<account>"]); both
   the paclet resource and our docs dir live under it, so deriving from it makes
   this run under any cloud id (local or CI) without hardcoding the account. *)
If[ ! $CloudConnected,
    With[{u = Environment["WOLFRAM_CLOUD_USER"], p = Environment["WOLFRAM_CLOUD_PASSWORD"]},
        If[StringQ[u] && StringQ[p], CloudConnect[u, p], CloudConnect[]]]];
If[ ! $CloudConnected, Print["build_docs_site: cloud connection failed"]; Exit[1] ];
accountBase = First[$CloudRootDirectory];
$base  = accountBase <> "/" <> $resourceRel;
$base2 = accountBase <> "/" <> $target;
Print["build_docs_site: account root ", accountBase];

(* ---- URLs ---- *)
srcStatic[type_, name_] :=
    StringReplace[$base, "/obj/" -> "/statichtml/"] <> "/Documentation/" <> type <>
        "/" <> name <> ".nb?width=" <> ToString[$nbWidth] <>
        "&maxwaitmillis=" <> ToString[$waitMs];

esc[s_] := StringReplace[ToString[s],
    {"&" -> "&amp;", "<" -> "&lt;", ">" -> "&gt;", "\"" -> "&quot;"}];

(* template assets (separate .css/.js/.html so editors syntax-highlight them) *)
readAsset[f_] := Module[{p = FileNameJoin[{webDir, f}]},
    If[FileExistsQ[p], Import[p, "Text"], Print["build_docs_site: missing asset ", p]; Exit[1]]];

(* ---- sidebar <ul> (page links carry data-id; the loader builds the page URL;
   a "group" is a synthetic, non-page label that just wraps a collapsible <ul>) ---- *)
linkOf[node_] := With[{type = pageType[node], cls = If[node["type"] === "symbol", "sym", "gd"]},
    "<a class=\"" <> cls <> "\" data-id=\"" <> esc[type <> "/" <> node["name"]] <>
        "\" href=\"#" <> esc[type <> "/" <> node["name"]] <> "\">" <> esc[node["title"]] <> "</a>"
];
liOf[node_] := If[ node["type"] === "group",
    "<li class=\"branch\"><details" <> If[TrueQ[node["open"]], " open", ""] <>
        "><summary><span class=\"grp\">" <> esc[node["title"]] <> "</span></summary><ul>" <>
        StringJoin[liOf /@ node["children"]] <> "</ul></details></li>",
    "<li class=\"leaf\">" <> linkOf[node] <> "</li>"
];
sidebar = "<ul class=\"nav-root\">" <> StringJoin[liOf /@ topNodes] <> "</ul>";

homeURL   = $base <> "/";
homeTitle = $rootGuide;

(* ---- assemble index.html from the separate .css / .js / .html templates ---- *)
jsContent = TemplateApply[
    StringTemplate[readAsset["docs_site.js"]],
    <|"BASE" -> $base, "BASE2" -> $base2|>
];
indexHTML = TemplateApply[
    StringTemplate[readAsset["docs_site.html"]],
    <|
        "title"   -> esc[homeTitle],
        "css"     -> readAsset["docs_site.css"],
        "base"    -> esc[$base],
        "homeURL" -> esc[homeURL],
        "sidebar" -> sidebar,
        "js"      -> jsContent
    |>
];

If[ ! StringQ[indexHTML], Print["build_docs_site: index assembly failed"]; Exit[1] ];
Print["build_docs_site: index.html assembled, ", StringLength[indexHTML], " chars"];

(* ---- fetch every page's statichtml in parallel (curl) ----
   The full notebook render (examples included) lives in the statichtml output; it
   just needs the page-fix CSS prepended to collapse the notebook's fixed-size scroll
   containers (otherwise a giant blank gap pushes the examples far down). *)
rawDir = FileNameJoin[{$TemporaryDirectory, "thvmdocs_raw"}];
Quiet @ DeleteDirectory[rawDir, DeleteContents -> True];
CreateDirectory[rawDir];
rawPath[i_] := FileNameJoin[{rawDir, ToString[i] <> ".html"}];
manifest = StringRiffle[
    MapIndexed[(rawPath[First[#2]] <> " " <> srcStatic[#1[[1]], #1[[2]]]) &, entries], "\n"];
manifestFile = FileNameJoin[{rawDir, "_manifest.txt"}];
Export[manifestFile, manifest, "Text"];

Print["build_docs_site: fetching ", Length[entries], " pages (", $fetchPar, "-way parallel)..."];
fetch = RunProcess[{"/bin/bash", "-c",
    "xargs -P " <> ToString[$fetchPar] <> " -L 1 curl -fsS -o < " <> manifestFile}];
Print["build_docs_site: fetch exit ", fetch["ExitCode"]];

(* page-fix CSS (collapses the blank gap) prepended to every page *)
pageFix = "<style>" <> readAsset["docs_page.css"] <> "</style>\n";
(* rewrite the resource's in-content links to our re-hosted, chrome-free pages *)
rewrite[s_] := StringReplace[s, {
    $base <> "/Documentation/guide/"    ~~ n : (WordCharacter ..) ~~ ".html" :> $base2 <> "/guide/" <> n <> ".html",
    $base <> "/Documentation/tutorial/" ~~ n : (WordCharacter ..) ~~ ".html" :> $base2 <> "/tutorial/" <> n <> ".html",
    $base <> "/Documentation/ref/"      ~~ n : (WordCharacter ..) ~~ ".html" :> $base2 <> "/ref/" <> n <> ".html"
}];

pages = {}; bad = {};
MapIndexed[
    Module[{type = #1[[1]], name = #1[[2]], rp = rawPath[First[#2]]},
        If[ FileExistsQ[rp] && FileByteCount[rp] > 400,
            AppendTo[pages, {type <> "/" <> name <> ".html", pageFix <> rewrite[Import[rp, "Text"]]}],
            AppendTo[bad, type <> "/" <> name]
        ]
    ] &, entries];
AppendTo[pages, {"index.html", indexHTML}];
Print["build_docs_site: ", Length[pages], " pages ready",
    If[bad === {}, "", " (missing: " <> StringRiffle[Take[bad, UpTo[10]], ", "] <> If[Length[bad] > 10, " ...", ""] <> ")"]];

(* ---- deploy: one public CloudObject per file (path-based, overwrites cleanly) ---- *)
cleanCloud[co_] := (Module[{ch = Quiet @ CloudObjects[co]}, If[ListQ[ch], cleanCloud /@ ch]]; Quiet @ DeleteObject[co]);
cleanCloud[CloudObject[$target]];

Print["build_docs_site: deploying ", Length[pages], " files..."];
n = 0; failed = {};
Do[
    Module[{r = CloudExport[p[[2]], {"String", "text/html"},
                CloudObject[$target <> "/" <> p[[1]]], Permissions -> "Public"]},
        If[MatchQ[r, _CloudObject], n++, AppendTo[failed, p[[1]]]]],
    {p, pages}
];
Print["build_docs_site: deployed ", n, "/", Length[pages], " files",
    If[failed === {}, "", " (FAILED: " <> StringRiffle[Take[failed, UpTo[8]], ", "] <> ")"]];
Print["published: ", $base2, "/index.html"];
Exit[0]
