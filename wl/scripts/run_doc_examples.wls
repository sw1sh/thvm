#!/usr/bin/env wolframscript
(* Run every ```wl code cell in the markdown doc sources as a test
   (github issue #93). The cells are evaluated the way the BUILT page evaluates
   them, so a cell that would echo unevaluated under red messages for a reader
   fails here.

   Adapted for THVMLink from WolframInstitute/PureMath's scripts/run_doc_examples.wls.

   Usage:
     wl -f wl/scripts/run_doc_examples.wls              (every THVMLink doc page)
     wl -f wl/scripts/run_doc_examples.wls a.md b.md    (specific pages)

   Prints a per-file line and an overall summary; exits 1 if any cell fails
   (a message, an abort, or a binding the built page has already thrown away),
   suitable for CI.

   ---- The rendering rule this runner mirrors ----

   MarkdownToNotebook evaluates a page's executable cells in ONE kernel session,
   in document order, inside a private per-page context, and it RESETS that
   state as it goes (option "EvaluateSeparator"). The reset is not the naive
   "everything dies at every heading":

     * A reset BOUNDARY is a thematic break (a line of three or more identical
       "-", "_" or "*") or ANY ATX heading, at any level. A boundary inside a
       fenced block is code, not a boundary.
     * The PROTECTED BASELINE is the page's SETUP SECTION - a heading whose whole
       text is "Definition", "Content" or "Initialization" - snapshotted when that
       section ends, at the first heading of its own level or higher. Bindings made
       in it survive every later boundary.
     * A page with NO setup section starts from an EMPTY baseline, so its very
       first boundary already clears everything it has bound.
     * Every boundary ClearAll-s every page symbol outside the baseline.
       Definitions that live OUTSIDE the page context (a paclet symbol's UpValues,
       $Assumptions, Off[...], SetOptions) are never cleared.

   A house-style symbol page (Usage / Details & Options / Basic Examples / Scope /
   ...) has no setup section, so NOTHING carries across a heading or a "---":
   every cell must stand on its own within the boundaries around it.

   This runner used to model an older converter, where a snapshot armed at the
   first canonical example heading and Basic-Examples bindings were kept for the
   whole page. MarkdownToNotebook 486a861 (2026-09-02, "clear the document's
   symbols at every delimiter") replaced that rule with the setup-section baseline
   above, and this runner kept passing pages the converter had started breaking -
   GeodesicBall.md read PASS here while every cell of its Properties and Relations
   echoed unevaluated on the built page. Mirror the converter, and when the
   converter moves, re-probe it (docs/notes/doc-example-state.md).

   Tutorials are built with "EvaluateSeparator" -> None (PacletPage's TechNote setting),
   which disables boundaries entirely, so a tutorial legitimately threads state
   from its first cell to its last. This runner applies None to docs/*/Tutorials
   for the same reason.

   A cell's <!-- => ... --> hint declares the messages the cell must issue: every
   Sym::tag named in the hint is expected, and a hint naming none expects none.
   A cell FAILS when it issues a message its hint does not name, when it fails to
   issue one the hint names, when it aborts, or - the reset-specific case - when
   it reads a page symbol whose definition a boundary threw away, either echoing
   it back in its output or quietly computing something else from it. That last
   check is what catches the reader-visible "2 + b1" echo, which produces no
   message at all. *)

repoRoot = ParentDirectory[DirectoryName[$InputFileName]]
pacletDir = FileNameJoin[{repoRoot, "THVMLink"}]
RunDocExamples`pacletDir = pacletDir

argFiles = Rest[$ScriptCommandLine]
docFiles = If[argFiles === {},
	Select[
		Flatten[FileNames["*.md", FileNameJoin[{pacletDir, "docs", #}]] & /@ {"Symbols", "Tutorials", "Guides"}],
		StringContainsQ[Import[#, "Text"], "```wl"] &],
	ExpandFileName /@ argFiles
]

If[docFiles === {},
	Print["run_doc_examples: no doc files with wl cells found"];
	Exit[1]
]

(* per-cell wall clock; generous - a doc example that needs longer than this is
   wedged, and one wedged page must not hold the whole tree hostage *)
$cellTimeout = 600;

(* ---------------------------------------------------------------- *)
(* Segmentation: the ordered stream of executable cells and reset     *)
(* boundaries, parsed the way the converter parses a page.            *)
(* ---------------------------------------------------------------- *)

(* the ONLY headings the converter treats as a setup section (MarkdownToNotebook
   $setupSections); a page with none of them protects nothing *)
RunDocExamples`setupSections = {"definition", "content", "initialization"};

RunDocExamples`fenceQ[line_String] := StringMatchQ[StringTrim[line], "```" ~~ ___]
RunDocExamples`fenceLen[line_String] :=
	StringLength[FirstCase[StringCases[StringTrim[line], StartOfString ~~ f : "`" .. :> f], _String, ""]]
RunDocExamples`headingQ[line_String] := StringMatchQ[line, "#" .. ~~ " " ~~ ___]
RunDocExamples`headingLevel[line_String] :=
	StringLength[FirstCase[StringCases[StringTrim[line], StartOfString ~~ h : "#" .. :> h], _String, ""]]
RunDocExamples`headingText[line_String] :=
	StringTrim[StringReplace[StringTrim[line], StartOfString ~~ "#" .. ~~ " " -> ""]]
(* a thematic break: three or more of the SAME character, from - _ * *)
RunDocExamples`separatorQ[line_String] := Module[{RunDocExamples`chars = Characters[StringTrim[line]]},
	Length[RunDocExamples`chars] >= 3 &&
		MemberQ[{"-", "_", "*"}, First[RunDocExamples`chars, ""]] &&
		Length[DeleteDuplicates[RunDocExamples`chars]] === 1
]

(* frontmatter, then HTML comments outside fenced blocks - a "#|" comment is the
   cell-option directive and survives as a bare "#|" line, everything else is
   author-only and is dropped, exactly as the converter drops it *)
RunDocExamples`dropFrontmatter[lines_List] := Module[{RunDocExamples`i, RunDocExamples`j},
	RunDocExamples`i = SelectFirst[Range[Length[lines]], StringTrim[lines[[#]]] =!= "" &, 0];
	If[RunDocExamples`i === 0 || StringTrim[lines[[RunDocExamples`i]]] =!= "---", Return[lines]];
	RunDocExamples`j = SelectFirst[Range[RunDocExamples`i + 1, Length[lines]],
		StringTrim[lines[[#]]] === "---" &, 0];
	If[RunDocExamples`j === 0, lines, Drop[lines, RunDocExamples`j]]
]

RunDocExamples`stripComments[lines_List] := Module[
	{RunDocExamples`out = {}, RunDocExamples`buf = {}, RunDocExamples`inFence = False,
		RunDocExamples`openLen = 0, RunDocExamples`flush},
	RunDocExamples`flush[] := (
		If[RunDocExamples`buf =!= {},
			RunDocExamples`out = Join[RunDocExamples`out,
				StringSplit[
					StringReplace[StringRiffle[RunDocExamples`buf, "\n"],
						"<!--" ~~ RunDocExamples`inner : Shortest[___] ~~ "-->" :>
							Which[
								StringStartsQ[StringTrim[RunDocExamples`inner], "#|"],
									StringReplace[StringTrim[RunDocExamples`inner], "\n" ~~ WhitespaceCharacter ... -> " "],
								(* an output hint survives as a marker line, so the cell it follows
								   learns which messages it is expected to issue *)
								StringStartsQ[StringTrim[RunDocExamples`inner], "=>"],
									"#=>" <> StringReplace[StringTrim[RunDocExamples`inner], "\n" ~~ WhitespaceCharacter ... -> " "],
								True, ""]],
					"\n", All]]];
		RunDocExamples`buf = {}
	);
	Do[
		Which[
			! RunDocExamples`inFence && RunDocExamples`fenceQ[RunDocExamples`line],
				RunDocExamples`flush[]; RunDocExamples`inFence = True;
				RunDocExamples`openLen = RunDocExamples`fenceLen[RunDocExamples`line];
				AppendTo[RunDocExamples`out, RunDocExamples`line],
			RunDocExamples`inFence && RunDocExamples`fenceQ[RunDocExamples`line] &&
				RunDocExamples`fenceLen[RunDocExamples`line] >= RunDocExamples`openLen,
				AppendTo[RunDocExamples`out, RunDocExamples`line];
				RunDocExamples`inFence = False; RunDocExamples`openLen = 0,
			RunDocExamples`inFence, AppendTo[RunDocExamples`out, RunDocExamples`line],
			True, AppendTo[RunDocExamples`buf, RunDocExamples`line]
		],
		{RunDocExamples`line, lines}
	];
	RunDocExamples`flush[];
	RunDocExamples`out
]

(* {"Code", source, hint} | {"Heading", text} | {"Separator"} in document order,
   the hint being the text of the <!-- => ... --> comment that follows a cell. Only
   EXECUTABLE cells become "Code": the converter's orderedCode keeps a fenced
   block only when its language is wl/wolfram/mathematica, its "eval" option is
   not False and it is not a literal-boxes cell. *)
RunDocExamples`parseItems[src_String] := Module[
	{RunDocExamples`lines, RunDocExamples`items = {}, RunDocExamples`inFence = False,
		RunDocExamples`openLen = 0, RunDocExamples`lang = "", RunDocExamples`body = {},
		RunDocExamples`pending = <||>, RunDocExamples`opts, RunDocExamples`optLines,
		RunDocExamples`codeLines},
	RunDocExamples`lines = RunDocExamples`stripComments[
		RunDocExamples`dropFrontmatter[StringSplit[src, "\n", All]]];
	Do[
		Which[
			! RunDocExamples`inFence && RunDocExamples`fenceQ[RunDocExamples`line],
				RunDocExamples`inFence = True;
				RunDocExamples`openLen = RunDocExamples`fenceLen[RunDocExamples`line];
				RunDocExamples`lang = ToLowerCase[First[StringSplit[StringDelete[
					StringDrop[StringTrim[RunDocExamples`line], RunDocExamples`openLen], {"{", "}"}]], ""]];
				RunDocExamples`body = {},
			RunDocExamples`inFence && RunDocExamples`fenceQ[RunDocExamples`line] &&
				RunDocExamples`fenceLen[RunDocExamples`line] >= RunDocExamples`openLen,
				RunDocExamples`inFence = False;
				RunDocExamples`optLines = TakeWhile[RunDocExamples`body,
					StringMatchQ[StringTrim[#], "#|" ~~ ___] &];
				RunDocExamples`codeLines = Drop[RunDocExamples`body, Length[RunDocExamples`optLines]];
				RunDocExamples`opts = Join[RunDocExamples`pending,
					Association[StringTrim[First[#]] -> StringTrim[Last[#]] & /@
						Select[StringSplit[StringReplace[StringTrim[#], StartOfString ~~ "#|" -> ""], ":", 2] & /@
							RunDocExamples`optLines, Length[#] === 2 &]]];
				RunDocExamples`pending = <||>;
				If[ MemberQ[{"wl", "wolfram", "mathematica"}, RunDocExamples`lang] &&
						! MemberQ[{"false", "False"}, Lookup[RunDocExamples`opts, "eval", "true"]] &&
						! KeyExistsQ[RunDocExamples`opts, "boxes"],
					AppendTo[RunDocExamples`items,
						{"Code", StringRiffle[RunDocExamples`codeLines, "\n"], ""}]],
			RunDocExamples`inFence, AppendTo[RunDocExamples`body, RunDocExamples`line],
			StringTrim[RunDocExamples`line] === "", Null,
			StringStartsQ[StringTrim[RunDocExamples`line], "#=>"],
				If[ RunDocExamples`items =!= {} && First[Last[RunDocExamples`items]] === "Code",
					RunDocExamples`items[[-1, 3]] =
						StringTrim[StringDrop[StringTrim[RunDocExamples`line], 3]]],
			StringStartsQ[StringTrim[RunDocExamples`line], "#|"],
				RunDocExamples`pending = Join[RunDocExamples`pending,
					Association[StringTrim[First[#]] -> StringTrim[Last[#]] & /@
						Select[{StringSplit[StringReplace[StringTrim[RunDocExamples`line],
							StartOfString ~~ "#|" -> ""], ":", 2]}, Length[#] === 2 &]]],
			RunDocExamples`headingQ[RunDocExamples`line],
				AppendTo[RunDocExamples`items,
					{"Heading", RunDocExamples`headingText[RunDocExamples`line],
						RunDocExamples`headingLevel[RunDocExamples`line]}],
			StringMatchQ[StringTrim[RunDocExamples`line], "![" ~~ ___ ~~ "](" ~~ ___ ~~ ")"], Null,
			RunDocExamples`separatorQ[RunDocExamples`line],
				AppendTo[RunDocExamples`items, {"Separator"}],
			True, Null
		],
		{RunDocExamples`line, RunDocExamples`lines}
	];
	RunDocExamples`items
]

(* the page's evaluation context path, as the converter assembles it: the
   frontmatter Context, the paclet's own context, System`, the converter's frame
   packages, and the sub-contexts of those - notably NOT Global` *)
RunDocExamples`frontmatterValue[src_String, key_String] := Module[{RunDocExamples`m},
	RunDocExamples`m = StringCases[src,
		StartOfLine ~~ key ~~ ":" ~~ RunDocExamples`v : Except["\n"] .. :> StringTrim[RunDocExamples`v]];
	If[RunDocExamples`m === {}, "", First[RunDocExamples`m]]
]

RunDocExamples`framePackages = {"ResourceSystemClient`", "DefinitionNotebookClient`", "DocumentationTools`"};

RunDocExamples`contextPath[src_String] := Module[{RunDocExamples`base},
	RunDocExamples`base = DeleteDuplicates[DeleteCases[Flatten[{
		RunDocExamples`frontmatterValue[src, "Context"],
		StringTrim /@ StringSplit[StringTrim[RunDocExamples`frontmatterValue[src, "ContextPath"], "[" | "]"], ","],
		StringReplace[RunDocExamples`frontmatterValue[src, "Paclet"], "/" -> "`"] <> "`",
		"System`"}], "" | "`"]];
	DeleteDuplicates[Join[
		RunDocExamples`base,
		RunDocExamples`framePackages,
		Flatten[Function[RunDocExamples`c,
			Select[Contexts[RunDocExamples`c <> "*"], ! StringEndsQ[#, "Private`"] &]] /@
				DeleteCases[RunDocExamples`base, "System`"]]]]
]

(* ---------------------------------------------------------------- *)
(* Evaluation                                                        *)
(* ---------------------------------------------------------------- *)

SetAttributes[RunDocExamples`definedQ, HoldFirst];
RunDocExamples`definedQ[RunDocExamples`s_Symbol] :=
	OwnValues[RunDocExamples`s] =!= {} || DownValues[RunDocExamples`s] =!= {} ||
	SubValues[RunDocExamples`s] =!= {} || UpValues[RunDocExamples`s] =!= {} ||
	NValues[RunDocExamples`s] =!= {} || FormatValues[RunDocExamples`s] =!= {}
RunDocExamples`definedNameQ[name_String] :=
	TrueQ[ToExpression[name, InputForm, RunDocExamples`definedQ]]

(* A cleared binding shows up in a cell in two ways: the cell's output still
   mentions the symbol (the reader's unevaluated echo), or the cell reads it and
   quietly computes something else. Both are the page being wrong, so both fail -
   except where the cell binds the name itself, as a pattern variable, an
   iterator, or a Module/Block/With/Function local, which is a different symbol
   wearing the same name and no defect at all. *)
RunDocExamples`localizedQ[code_, RunDocExamples`sym_] :=
	! FreeQ[code, List[RunDocExamples`sym, ___]] ||
	! FreeQ[code, Verbatim[Pattern][RunDocExamples`sym, _]] ||
	AnyTrue[{Module, Block, With, Function},
		! FreeQ[code, HoldPattern[#][List[___, RunDocExamples`sym, ___], ___]] &]

(* assumes the name is currently undefined - the caller checks that first, so
   that ToExpression here yields the symbol and not some binding's value *)
RunDocExamples`staleHitQ[name_String, outs_, code_] := Module[{RunDocExamples`sym},
	RunDocExamples`sym = ToExpression[name];
	! FreeQ[outs, RunDocExamples`sym] ||
		(! FreeQ[code, RunDocExamples`sym] &&
			! RunDocExamples`localizedQ[code, RunDocExamples`sym])
]

(* the messages a hint names, as "Sym::tag" strings *)
RunDocExamples`hintMessages[hint_String] :=
	DeleteDuplicates[StringCases[hint,
		RegularExpression["\\b[A-Za-z$][A-Za-z0-9$]*::[A-Za-z][A-Za-z0-9]*\\b"]]]

(* every message a cell issues while on (a quieted one is off and is not the page's
   concern); General::stop only says that one of them repeated *)
RunDocExamples`recordMessage[Hold[Message[MessageName[RunDocExamples`s_, RunDocExamples`t_String], ___], True]] :=
	AppendTo[RunDocExamples`issued, SymbolName[Unevaluated[RunDocExamples`s]] <> "::" <> RunDocExamples`t]
RunDocExamples`recordMessage[_] := Null

(* One file: parse it into cells and boundaries, then walk them keeping exactly
   the state the built page keeps. Every harness local and iterator is
   context-qualified: a Block/Do binding named like a symbol a cell uses
   (k, src, cells, ...) would CAPTURE it during the cell's evaluation - a doc
   cell's momentum k once evaluated to its own cell index this way. *)
(* A subkernel the pool resurrects after a death comes back without the paclet and
   without the frame packages, and then every paclet symbol on a page is inert: no
   message, no value, a cascade of iterator and part failures that look like page
   defects. The worker checks its own state before every file and loads what it lacks. *)
RunDocExamples`ensureLoaded[] :=
	If[ Names["WolframInstitute`THVMLink`TRealize"] === {},
		Off[General::shdw];
		PacletDirectoryLoad[RunDocExamples`pacletDir];
		Needs["WolframInstitute`THVMLink`"]; Needs["WolframInstitute`THVMLink`ATP`"];
		Quiet[Check[Needs[#], Null]] & /@ RunDocExamples`framePackages;
		$harnessGlobalSymbols = Names["Global`*"]
	]

runFile[file_] := Block[{
		RunDocExamples`src, RunDocExamples`items, RunDocExamples`mode, RunDocExamples`ctx,
		RunDocExamples`failures = {}, RunDocExamples`nCells = 0, RunDocExamples`protected = None,
		RunDocExamples`defLevel = None, RunDocExamples`setupQ,
		RunDocExamples`lost = {}, RunDocExamples`lastBoundary = "",
		RunDocExamples`toRemove, RunDocExamples`held, RunDocExamples`outs, RunDocExamples`res,
		RunDocExamples`expected, RunDocExamples`issued, RunDocExamples`unexpected,
		RunDocExamples`missing, RunDocExamples`stale},
	RunDocExamples`ensureLoaded[];
	(* a fresh runtime per page, as the build's converter (wl/scripts/pacletpage.wl) gives it *)
	WolframInstitute`THVMLink`TInit[];
	Quiet[Clear @@ Complement[Names["Global`*"], $harnessGlobalSymbols]];
	RunDocExamples`src = Import[file, "Text"];
	RunDocExamples`items = RunDocExamples`parseItems[RunDocExamples`src];
	(* the build converts docs/<variant>/Tutorials with
	   "EvaluateSeparator" -> None, so a tutorial threads state end to end *)
	RunDocExamples`mode = If[MemberQ[FileNameSplit[file], "Tutorials"], None, Automatic];
	RunDocExamples`ctx = "PMDoc`P" <> IntegerString[Hash[file], 36] <> "`";
	RunDocExamples`setupQ[RunDocExamples`it_] :=
		RunDocExamples`it[[1]] === "Heading" &&
			MemberQ[RunDocExamples`setupSections, ToLowerCase[StringTrim[RunDocExamples`it[[2]]]]];
	(* no setup section means an empty baseline from the start, so the first boundary
	   already clears whatever the page has bound *)
	RunDocExamples`protected =
		If[AnyTrue[RunDocExamples`items, RunDocExamples`setupQ], None, {}];
	Block[{$Context = RunDocExamples`ctx,
			$ContextPath = RunDocExamples`contextPath[RunDocExamples`src]},
		Do[
			Which[
				RunDocExamples`item[[1]] === "Heading" || RunDocExamples`item[[1]] === "Separator",
					(* the baseline is captured when the SETUP SECTION ends - the first heading
					   at its own level or higher - and before that boundary resets anything *)
					If[ RunDocExamples`protected === None && IntegerQ[RunDocExamples`defLevel] &&
							RunDocExamples`item[[1]] === "Heading" && ! RunDocExamples`setupQ[RunDocExamples`item] &&
							RunDocExamples`item[[3]] <= RunDocExamples`defLevel,
						RunDocExamples`protected = Names[RunDocExamples`ctx <> "*"]];
					If[ RunDocExamples`setupQ[RunDocExamples`item],
						RunDocExamples`defLevel = RunDocExamples`item[[3]]];
					If[ RunDocExamples`mode =!= None && RunDocExamples`protected =!= None,
						RunDocExamples`toRemove = Complement[
							Names[RunDocExamples`ctx <> "*"], RunDocExamples`protected];
						RunDocExamples`lastBoundary = If[RunDocExamples`item[[1]] === "Heading",
							"the \"" <> RunDocExamples`item[[2]] <> "\" heading", "a --- separator"];
						(* remember only the ones that carried a DEFINITION: a bare symbolic
						   variable (the x of an Integrate) is cleared too and losing it is
						   not a defect. An entry stays until the page defines the symbol
						   again, so a binding lost at one boundary is still reported at the
						   next one. *)
						With[{RunDocExamples`gone = Select[RunDocExamples`toRemove, RunDocExamples`definedNameQ]},
							RunDocExamples`lost = Join[
								DeleteCases[RunDocExamples`lost, {Alternatives @@ RunDocExamples`gone, _}],
								{#, RunDocExamples`lastBoundary} & /@ RunDocExamples`gone]];
						If[RunDocExamples`toRemove =!= {},
							Quiet[ClearAll @@ RunDocExamples`toRemove]]
					],
				RunDocExamples`item[[1]] === "Code",
					RunDocExamples`nCells++;
					RunDocExamples`outs = {};
					RunDocExamples`expected = RunDocExamples`hintMessages[RunDocExamples`item[[3]]];
					RunDocExamples`issued = {};
					RunDocExamples`held = Quiet[ToExpression[RunDocExamples`item[[2]], InputForm, Hold]];
					(* each cell is its own evaluation on the built page, so the repeat count
					   behind General::stop starts afresh with every cell *)
					RunDocExamples`res = Block[{$MessageList = {}},
						Internal`HandlerBlock[{"Message", RunDocExamples`recordMessage},
						TimeConstrained[
							If[ Head[RunDocExamples`held] === Hold,
								(* the converter evaluates a cell statement by statement and shows an
								   output for each one that does not end in a semicolon *)
								Do[
									With[{RunDocExamples`stmt =
											Extract[RunDocExamples`held, RunDocExamples`i, Hold]},
										If[ MatchQ[RunDocExamples`stmt, Hold[_CompoundExpression]],
											ReleaseHold[RunDocExamples`stmt],
											AppendTo[RunDocExamples`outs, ReleaseHold[RunDocExamples`stmt]]]],
									{RunDocExamples`i, Length[RunDocExamples`held]}],
								AppendTo[RunDocExamples`outs,
									ToExpression[RunDocExamples`item[[2]], InputForm]]],
							$cellTimeout]]];
					RunDocExamples`issued = DeleteDuplicates[DeleteCases[RunDocExamples`issued, "General::stop"]];
					RunDocExamples`unexpected = Complement[RunDocExamples`issued, RunDocExamples`expected];
					RunDocExamples`missing = Complement[RunDocExamples`expected, RunDocExamples`issued];
					(* the reset-specific failure: the reader sees the cell echo a binding the
					   built page threw away at the last boundary *)
					RunDocExamples`stale = Select[RunDocExamples`lost,
						! RunDocExamples`definedNameQ[First[#]] &&
							RunDocExamples`staleHitQ[First[#], RunDocExamples`outs,
								RunDocExamples`held] &];
					If[ RunDocExamples`unexpected =!= {} || RunDocExamples`missing =!= {} ||
							RunDocExamples`res === $Aborted || RunDocExamples`stale =!= {},
						AppendTo[RunDocExamples`failures, {
							RunDocExamples`nCells,
							Which[
								RunDocExamples`stale =!= {},
									"stale " <> StringRiffle[
										Last[StringSplit[First[#], "`"]] & /@ RunDocExamples`stale, ", "] <>
										" cleared at " <> RunDocExamples`stale[[1, 2]],
								RunDocExamples`res === $Aborted, "timeout",
								True, StringRiffle[DeleteCases[{
									If[RunDocExamples`unexpected =!= {},
										"message " <> StringRiffle[RunDocExamples`unexpected, ", "], ""],
									If[RunDocExamples`missing =!= {},
										"missing message " <> StringRiffle[RunDocExamples`missing, ", "], ""]},
									""], "; "]],
							StringTake[RunDocExamples`item[[2]], UpTo[70]]}]],
				True, Null
			],
			{RunDocExamples`item, RunDocExamples`items}
		]
	];
	Quiet[ClearAll[RunDocExamples`ctx <> "*"]];
	Quiet[Remove[RunDocExamples`ctx <> "*"]];
	{file, RunDocExamples`nCells, RunDocExamples`failures}
]

$harnessGlobalSymbols = Names["Global`*"];

(* worker pool as in run_tests.wls: each worker loads the paclet and captures
   its own harness baseline; ParallelMap preserves file order for reporting.
   Scheduling is FinestGrained: clearing a binding at a boundary can make the
   cell that reads it afterwards slow (a symbolic integral that no longer has
   its integrand), and with the default contiguous batches one such page holds
   its whole batch, and the run, hostage. *)
LaunchKernels[Min[4, Length[docFiles]]]
DistributeDefinitions["RunDocExamples`"]
DistributeDefinitions[runFile, $cellTimeout]
With[{pd = pacletDir},
	ParallelEvaluate[
		(* a shadow warning reflects the order this harness happened to load its
		   packages in, not anything the page does: the converter's kernel loads the
		   same contexts in its own order and the built page carries no such message *)
		Off[General::shdw];
		PacletDirectoryLoad[pd];
		Needs["WolframInstitute`THVMLink`"]; Needs["WolframInstitute`THVMLink`ATP`"];
		(* the converter Needs its frame packages before evaluating a page, which
		   puts them on the page's context path; mirror that once per worker *)
		Quiet[Check[Needs[#], Null]] & /@ RunDocExamples`framePackages;
		$harnessGlobalSymbols = Names["Global`*"];
	]
]
Off[General::shdw]
PacletDirectoryLoad[pacletDir]
Needs["WolframInstitute`THVMLink`"]; Needs["WolframInstitute`THVMLink`ATP`"]
Quiet[Check[Needs[#], Null]] & /@ RunDocExamples`framePackages;
$harnessGlobalSymbols = Names["Global`*"];

results = ParallelMap[runFile, docFiles, Method -> "FinestGrained"]

Scan[
	Function[r,
		Print[
			If[r[[3]] === {}, "  ok    ", "  FAIL  "],
			StringDrop[r[[1]], StringLength[repoRoot] + 1], "  (", r[[2]], " cells)"
		];
		Scan[
			f |-> Print["          cell ", f[[1]], " failed (", f[[2]], "): ", f[[3]]],
			r[[3]]
		]
	],
	results
]

totalCells = Total[#[[2]] & /@ results]
failedFiles = Select[results, #[[3]] =!= {} &]

Print[""]
Print[
	If[failedFiles === {}, "PASS  ", "FAIL  "],
	totalCells, " cells across ", Length[results], " file(s)",
	If[failedFiles === {}, "", "; " <> ToString[Length[failedFiles]] <> " file(s) with failures"]
]
If[failedFiles =!= {}, Exit[1]]
