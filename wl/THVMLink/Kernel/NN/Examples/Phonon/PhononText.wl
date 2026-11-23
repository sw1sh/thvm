(* PhononText.wl -- text for Phonon: the SentencePiece Unigram tokenizer of the model's tokenizer.json,
   pocket_tts's prompt preparation and sentence chunking (models/text_chunking.py), and a normaliser for
   the symbols the model was not trained to read.

   The tokenizer follows the HuggingFace `tokenizers` pipeline the JSON names: the normaliser prepends the
   word mark U+2581, Metaspace turns each space into the mark and splits before every mark, and each word
   takes its best-scoring split into vocabulary pieces (Viterbi).  A character no piece covers costs the
   lowest score minus 10 and becomes the byte tokens of its UTF-8 encoding (byte_fallback). *)

BeginPackage["WolframInstitute`THVMLink`Examples`", {"WolframInstitute`THVMLink`"}];

Begin["`Private`"];

(* === tokenizer ======================================================== *)

$phMark = "\:2581";

phTokenizer[path_String] := Module[{vocab = Import[path, "RawJSON"]["model"]["vocab"], pieces, scores},
    pieces = vocab[[All, 1]];
    scores = N @ vocab[[All, 2]];
    <|"Piece" -> AssociationThread[pieces, Transpose[{Range[0, Length[pieces] - 1], scores}]], "Pieces" -> pieces,
      "MaxLength" -> Max[StringLength /@ pieces], "UnkScore" -> Min[scores] - 10.|>
]

(* The byte tokens <0xXX> of a character no piece covers, or the unknown id 0 when the vocabulary lacks one. *)
phByteTokens[tok_, ch_String] := With[
    {ids = tok["Piece"]["<0x" <> ToUpperCase[IntegerString[#, 16, 2]] <> ">"] & /@ ToCharacterCode[ch, "UTF-8"]},
    If[FreeQ[ids, _Missing], ids[[All, 1]], {0}]
]

(* The ids of the best-scoring split of one word. *)
phEncodeWord[tok_, word_String] := Module[{chars = Characters[word], n, best, back, cand, single, ids = {}, j},
    n = Length[chars];
    best = Join[{0.}, ConstantArray[-Infinity, n]];
    back = ConstantArray[None, n + 1];
    Do[
        single = False;
        Do[
            cand = tok["Piece"][StringJoin[chars[[i ;; e]]]];
            If[! MissingQ[cand],
                If[e == i, single = True];
                If[best[[i]] + cand[[2]] > best[[e + 1]], best[[e + 1]] = best[[i]] + cand[[2]]; back[[e + 1]] = {i, {cand[[1]]}}]],
            {e, i, Min[n, i + tok["MaxLength"] - 1]}];
        If[! single && best[[i]] + tok["UnkScore"] > best[[i + 1]],
            best[[i + 1]] = best[[i]] + tok["UnkScore"]; back[[i + 1]] = {i, phByteTokens[tok, chars[[i]]]}],
        {i, n}];
    j = n + 1;
    While[j > 1, ids = Join[back[[j, 2]], ids]; j = back[[j, 1]]];
    ids
]

(* Token ids (0-based) of text, as tokenizers' encode without special tokens. *)
phEncode[tok_, ""] := {}
phEncode[tok_, text_String] := Flatten[phEncodeWord[tok, #] & /@
    StringCases[$phMark <> StringReplace[text, " " -> $phMark], $phMark ~~ Except[$phMark] ...]]

(* The text of token ids: the pieces joined, byte tokens read as UTF-8, marks as spaces, the first one
   dropped. *)
phDecode[tok_, ids_List] := Module[{parts},
    parts = Replace[tok["Pieces"][[ids + 1]],
        p_String /; StringMatchQ[p, "<0x" ~~ _ ~~ _ ~~ ">"] :> FromDigits[StringTake[p, {4, 5}], 16], 1];
    parts = Replace[Split[parts, IntegerQ[#1] && IntegerQ[#2] &],
        b : {__Integer} :> FromCharacterCode[b, "UTF-8"], 1];
    StringReplace[StringJoin[parts], {StartOfString ~~ $phMark -> "", $phMark -> " "}]
]

(* === prompt preparation =============================================== *)

(* Characters absent from the training data (pocket_tts config/english_2026-10.yaml, replace_characters). *)
$phReplaceCharacters = {"\[CloseCurlyQuote]" -> "'", "\[OpenCurlyQuote]" -> "'"};

$phTerminal = Characters[".!?\[Ellipsis]"];
$phWeak = Characters[",;:-\[Dash]\[LongDash]"];
$phClosers = Characters["\"'\[CloseCurlyDoubleQuote]\[CloseCurlyQuote])]\[RightGuillemet]"];

(* A prompt that ends with sentence-final punctuation: a trailing comma, colon or dash becomes a period,
   anything else gets one after its closing quotes or brackets (pocket_tts _ensure_terminal_punctuation). *)
phTerminate[text_String] := Module[{core, closers},
    core = StringTrim[text, (Alternatives @@ Append[$phClosers, " "]) .. ~~ EndOfString];
    closers = StringDelete[StringDrop[text, StringLength[core]], " "];
    Which[
        core === "" || MemberQ[$phTerminal, StringTake[core, -1]], text,
        MemberQ[$phWeak, StringTake[core, -1]], StringTrim[core, (Alternatives @@ Append[$phWeak, " "]) .. ~~ EndOfString] <> "." <> closers,
        True, text <> "."]
]

(* The prompt text for one chunk and the frames to keep generating after its end of speech: 3 for up to
   4 words, else 1 (pocket_tts prepare_text_prompt; the counts are the model's processor_config.json). *)
phPrepare[text_String] := Module[{t},
    t = StringRiffle[StringSplit[StringReplace[StringTrim[text], $phReplaceCharacters]], " "];
    t = StringReplace[t, p : (Alternatives @@ $phTerminal) ~~ " " ... ~~ ("," | ";" | ":") :> p];
    If[t === "", Return[{"", 1}]];
    If[! UpperCaseQ[StringTake[t, 1]], t = ToUpperCase[StringTake[t, 1]] <> StringDrop[t, 1]];
    {phTerminate[t], If[Length[StringSplit[t]] <= 4, 3, 1]}
]

(* === chunking ========================================================== *)

(* Segment starts (0-based token offsets, with 0 and the length at the ends): a segment begins at the
   first token after a run of boundary tokens, unless the run is the period of a decimal number
   (pocket_tts _find_boundary_indices). *)
phBoundaries[tok_, ids_List, boundary_List, skipDecimals_] := Module[{at = {0}, prev = False},
    Do[
        If[MemberQ[boundary, ids[[i]]],
            prev = True,
            If[prev && ! (skipDecimals && phDecimalQ[tok, ids, i - 1]), AppendTo[at, i - 1]];
            prev = False],
        {i, Length[ids]}];
    Append[at, Length[ids]]
]

phDecimalQ[tok_, ids_List, start_Integer] := With[
    {prefix = phDecode[tok, ids[[;; start]]], suffix = phDecode[tok, ids[[start + 1 ;;]]]},
    StringLength[prefix] >= 2 && StringTake[prefix, -1] === "." && DigitQ[StringTake[prefix, {-2}]] &&
        suffix =!= "" && DigitQ[StringTake[suffix, 1]]
]

phSegments[tok_, ids_List, at_List] := {#2 - #1, phDecode[tok, ids[[#1 + 1 ;; #2]]]} & @@@ Partition[at, 2, 1]

(* The chunks a text is spoken in: its sentences, packed up to maxTokens tokens per chunk, a longer
   sentence split at commas, semicolons and colons (pocket_tts split_into_best_sentences). *)
phChunks[tok_, text_String, maxTokens_Integer : 50] := Module[{prepared = First @ phPrepare[text], ids, segments, chunks = {}, cur = "", n = 0},
    If[prepared === "", Return[{}]];
    ids = phEncode[tok, prepared];
    segments = phSegments[tok, ids, phBoundaries[tok, ids, Rest @ phEncode[tok, ".!...?"], True]];
    segments = Join @@ Replace[segments, {
        {m_, s_} /; m > maxTokens :> With[{sub = phEncode[tok, StringTrim[s]]},
            Replace[phSegments[tok, sub, phBoundaries[tok, sub, Rest @ phEncode[tok, ",;:"], False]], {_} -> {{m, s}}]],
        seg_ :> {seg}}, 1];
    Do[
        Which[
            cur === "", cur = seg[[2]]; n = seg[[1]],
            n + seg[[1]] > maxTokens, AppendTo[chunks, StringTrim[cur]]; cur = seg[[2]]; n = seg[[1]],
            True, cur = cur <> " " <> seg[[2]]; n += seg[[1]]],
        {seg, segments}];
    If[cur =!= "", AppendTo[chunks, StringTrim[cur]]];
    chunks
]

(* === normaliser ======================================================== *)

$phMonths = <|"Jan" -> "January", "Feb" -> "February", "Mar" -> "March", "Apr" -> "April", "Jun" -> "June",
    "Jul" -> "July", "Aug" -> "August", "Sep" -> "September", "Sept" -> "September", "Oct" -> "October",
    "Nov" -> "November", "Dec" -> "December"|>;

(* A number with thousands separators, group by group: "2,500,000" reads "2 million 500 thousand". *)
phGroups[n_String] := With[{g = FromDigits /@ StringSplit[n, ","]},
    StringRiffle[MapThread[If[#1 == 0, Nothing, StringTrim[ToString[#1] <> " " <> #2]] &,
        {g, Take[{"trillion", "billion", "million", "thousand", ""}, -Length[g]]}], " "] /. "" -> "0"]

phCount[n_String, {one_String, many_String}] := n <> " " <> If[n === "1", one, many]

(* An amount of money: "$3.50" reads "3 dollars and 50 cents", "$0.99" reads "99 cents". *)
phMoney[unit_List, cent_List][whole_String, frac_String] := With[
    {w = phGroups[whole], c = If[frac === "", 0, FromDigits[frac]]},
    Which[
        c == 0, phCount[w, unit],
        w === "0", phCount[ToString[c], cent],
        True, phCount[w, unit] <> " and " <> phCount[ToString[c], cent]]]

$phCurrencies = <|"$" -> {{"dollar", "dollars"}, {"cent", "cents"}}, "\[Euro]" -> {{"euro", "euros"}, {"cent", "cents"}},
    "\[Sterling]" -> {{"pound", "pounds"}, {"penny", "pence"}}, "\[Yen]" -> {{"yen", "yen"}, {"sen", "sen"}}|>;

(* A web or mail address read out: the scheme and "www" letter by letter, the separators by name. *)
phAddress[a_String] := StringReplace[a, {
    StartOfString ~~ scheme : LetterCharacter .. ~~ "://" :> StringRiffle[Characters[ToUpperCase[scheme]], "-"] <> " colon slash slash ",
    "www." -> "W-W-W dot ", "." -> " dot ", "/" -> " slash ", "@" -> " at ", "-" -> " dash ", "_" -> " underscore "}]

(* Symbols written as the words the model reads, after the English rules of ptts's normaliser: money,
   thousands separators, decimal points, degrees, web and mail addresses, month abbreviations, operators,
   and the punctuation the model was not trained on.  Whole numbers stay as digits, which the model reads
   itself. *)
phNormalize[text_String] := Module[{t, amount = "(\\d{1,3}(?:,\\d{3})+|\\d+)(?:\\.(\\d\\d))?(?!\\d)"},
    t = StringReplace[text, {
        RegularExpression["[A-Za-z]+://\\S*[^\\s.,!?;:)]"] :> phAddress["$0"],
        RegularExpression["[\\w.+-]+@[\\w-]+(?:\\.[\\w-]+)+"] :> phAddress["$0"],
        RegularExpression["([$\[Euro]\[Sterling]\[Yen]])" <> amount] :> (phMoney @@ $phCurrencies["$1"])["$2", "$3"],
        RegularExpression["(\\d+)\[Cent]"] :> phCount["$1", {"cent", "cents"}],
        RegularExpression["(?<![\\w.,])\\d{1,3}(?:,\\d{3})+(?![\\d])"] :> phGroups["$0"],
        RegularExpression["(?<![\\w.])(\\d+)\\.(\\d+)(?![\\d%]|\\.\\d)"] :> "$1 point $2",
        RegularExpression["(?<![\\w.])-(\\d)"] :> "minus $1",
        RegularExpression["(\\d)\\s*[\[Dash]\[LongDash]]\\s*(\\d)"] :> "$1 to $2",
        RegularExpression["~(\\d)"] :> "about $1",
        "\[Degree]C" -> " degrees Celsius", "\[Degree]F" -> " degrees Fahrenheit", "\[Degree]" -> " degrees",
        RegularExpression["\\b(" <> StringRiffle[Keys[$phMonths], "|"] <> ")\\. (\\d)"] :> $phMonths["$1"] <> " $2",
        RegularExpression["\\bvs\\."] -> "versus",
        RegularExpression["\\s*\\+\\s*"] -> " plus ", RegularExpression["\\s*=\\s*"] -> " equals ",
        RegularExpression["\\s*\[Times]\\s*"] -> " times ", RegularExpression["\\s*\[Divide]\\s*"] -> " divided by ",
        "\[OpenCurlyDoubleQuote]" | "\[CloseCurlyDoubleQuote]" -> "\"", "\[OpenCurlyQuote]" | "\[CloseCurlyQuote]" -> "'",
        RegularExpression["\\s[\[Dash]\[LongDash]]\\s"] -> " ", "\[LongDash]" | "\[Dash]" -> ", ",
        "..." | "\[Ellipsis]" -> ".", ": " -> ", ", ";" -> ",",
        "(" -> ", ", ")" -> ",", "[" | "]" | "{" | "}" -> ""}];
    StringTrim @ StringReplace[
        StringReplace[t, {WhitespaceCharacter .. -> " "}],
        {RegularExpression["[ ,]*,[ ,]*([.!?])"] -> "$1", RegularExpression["[ ,]*,[ ,]*"] -> ", "}]
]

End[];

EndPackage[];
