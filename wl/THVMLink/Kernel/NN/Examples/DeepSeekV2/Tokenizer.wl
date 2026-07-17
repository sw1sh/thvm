(* Tokenizer.wl -- DeepSeek-V2-Lite byte-level BPE tokenizer for thvm, a
   faithful pure-Wolfram port of the HuggingFace `tokenizers` fast tokenizer
   described by the model's tokenizer.json.  Byte-exact against
   transformers AutoTokenizer (encode with add_special_tokens=False, and the
   .decode round trip); the companion oracle check is
   tools/thvm_llm_service/tokenizer_validate.wls.

   The pipeline mirrors the JSON exactly:

       text --(added-token split)--> segments
       segment --(pre_tokenizer.Sequence)--> pieces
                 [0] Split [\r\n]              (newlines isolated)
                 [1] Split \s?[letters]+       (leading-space word runs)
                 [2] Split \s?[punct]+         (leading-space punctuation runs)
                 [3] Split \s+$                (trailing whitespace)
                 [4] Split [CJK]+              (CJK runs)
                 [5] Digits individual_digits  (each digit its own piece)
                 [6] ByteLevel use_regex=false (byte-encode, no split)
       piece --(UTF-8 bytes -> bytes_to_unicode chars)--> char string
       char string --(BPE merges by rank)--> symbols --(vocab)--> ids

   Every constant (vocab, merge ranks, the pretokenizer regexes, the added
   special tokens) is read from the JSON at load time; nothing is retyped.  The
   byte<->unicode table is the standard GPT-2 bytes_to_unicode map. *)

BeginPackage["WolframInstitute`THVMLink`Examples`", {"WolframInstitute`THVMLink`"}];

dsvTokenizer::usage = "dsvTokenizer[jsonPath] loads a DeepSeek-V2 tokenizer.json and returns a tokenizer association: \"Vocab\" (token string -> id), \"IdToTok\" (id -> token string), \"MergeRank\" (\"A B\" -> rank), \"ByteToChar\"/\"CharToByte\" (the GPT-2 byte-level maps), \"Stages\" (the precompiled pre_tokenizer split/digits stages in order), \"SpecialToId\"/\"IdToSpecial\" (the added special tokens).  Memoized per path.";
dsvEncode::usage = "dsvEncode[tok, text] returns the list of 0-indexed integer token ids for text under tokenizer tok, matching HuggingFace encode(text, add_special_tokens=False): no BOS/EOS is added, but a literal <|begin of sentence|> / <|end of sentence|> substring emits its id (100000 / 100001) and only the gaps between such tokens are byte-BPE encoded.";
dsvDecode::usage = "dsvDecode[tok, ids] returns the String decoded from the 0-indexed token id list ids, matching HuggingFace decode(ids): special ids render as their literal content strings and each maximal run of regular tokens is byte-level decoded (token chars -> bytes -> UTF-8).";

Begin["`Private`"];

(* === byte-level table (GPT-2 bytes_to_unicode) =================== *)

(* keep = the printable byte values that map to themselves; every other byte
   (extra, in ascending order) maps to a fresh code point starting at 256, so no
   token char is an actual control byte or space.  Returns {byte -> char,
   char -> byte}. *)
dsvByteMaps[] := dsvByteMaps[] = Block[{keep, extra, allBytes, chars},
    keep = Join[Range[33, 126], Range[161, 172], Range[174, 255]];
    extra = Complement[Range[0, 255], keep];
    allBytes = Join[keep, extra];
    chars = FromCharacterCode /@ Join[keep, 256 + Range[0, Length[extra] - 1]];
    {AssociationThread[allBytes -> chars], AssociationThread[chars -> allBytes]}
]

(* === tokenizer load ============================================== *)

(* Turn one pre_tokenizer entry into a stage: a RegularExpression delimiter for
   a Split, the "Digits" marker for the Digits pretokenizer, Nothing for the
   trailing ByteLevel (its byte-encode is dsvPieceIds, not a split). *)
dsvStage[s_Association] := Switch[s["type"],
    "Split", RegularExpression[s["pattern"]["Regex"]],
    "Digits", "Digits",
    _, Nothing
]

dsvTokenizer[jsonPath_String] := dsvTokenizer[jsonPath] = Block[{
    json, vocab, merges, byteToChar, charToByte, stages, added, specialToId
},
    json = Import[jsonPath, "RawJSON"];
    vocab = json["model", "vocab"];
    merges = json["model", "merges"];
    {byteToChar, charToByte} = dsvByteMaps[];
    stages = dsvStage /@ json["pre_tokenizer", "pretokenizers"];
    added = Select[json["added_tokens"], TrueQ[#["special"]] &];
    specialToId = Association[(#["content"] -> #["id"]) & /@ added];
    <|
        "Vocab" -> vocab,
        "IdToTok" -> Association[Reverse /@ Normal[vocab]],
        "MergeRank" -> AssociationThread[merges -> Range[Length[merges]] - 1],
        "ByteToChar" -> byteToChar,
        "CharToByte" -> charToByte,
        "Stages" -> stages,
        "SpecialToId" -> specialToId,
        "IdToSpecial" -> Association[Reverse /@ Normal[specialToId]]
    |>
]

(* === pre-tokenization ============================================ *)

(* Isolated split keeping the matched delimiter as its own piece and dropping
   empty gaps -- the HuggingFace "Isolated" behavior.  Digits isolates each
   ASCII digit (individual_digits=true). *)
dsvSplitPiece[piece_, re_RegularExpression] := StringSplit[piece, m : re :> m]

dsvSplitPiece[piece_, "Digits"] := StringSplit[piece, d : RegularExpression["[0-9]"] :> d]

dsvApplyStage[pieces_, stage_] := Flatten @ Map[p |-> dsvSplitPiece[p, stage], pieces]

(* === BPE ========================================================= *)

(* One merge pass: replace every non-overlapping adjacent (first, second) with
   the joined symbol, left to right (the GPT-2 reference bpe() inner pass;
   SequenceReplace is left-to-right non-overlapping). *)
dsvBpeMerge[word_, first_, second_] := SequenceReplace[word, {first, second} -> first <> second]

(* One BPE step: merge every occurrence of the lowest-rank adjacent pair (min by
   rank, ties to the leftmost), or return the word unchanged when no pair is in
   the merge table.  The fixed point is the fully merged word. *)
dsvBpeStep[word_, rank_] := If[ Length[word] < 2,
    word,
    Block[{pairs = Partition[word, 2, 1], ranks, mn, pos},
        ranks = Lookup[rank, (#[[1]] <> " " <> #[[2]]) & /@ pairs, Infinity];
        mn = Min[ranks];
        If[ mn === Infinity,
            word,
            pos = First @ FirstPosition[ranks, mn];
            dsvBpeMerge[word, pairs[[pos, 1]], pairs[[pos, 2]]]
        ]
    ]
]

dsvBpeWord[symbols_, rank_] := FixedPoint[dsvBpeStep[#, rank] &, symbols]

(* A single pre-token piece -> ids: UTF-8 bytes -> byte-level chars -> BPE
   -> vocab.  Byte-level guarantees every single-char symbol is in vocab. *)
dsvPieceIds[tok_, piece_] := Block[{chars, merged},
    chars = Lookup[tok["ByteToChar"], ToCharacterCode[piece, "UTF-8"]];
    merged = dsvBpeWord[chars, tok["MergeRank"]];
    Lookup[tok["Vocab"], merged]
]

(* A segment with no special tokens: fold the split stages over the piece list,
   then byte-BPE every resulting piece. *)
dsvEncodePlain[tok_, segment_] :=
    Flatten @ Map[p |-> dsvPieceIds[tok, p], Fold[dsvApplyStage, {segment}, tok["Stages"]]]

(* === public encode / decode ====================================== *)

dsvEncode[tok_Association, text_String] := Block[{specials = tok["SpecialToId"], segs},
    segs = If[text === "", {}, StringSplit[text, sp : (Alternatives @@ Keys[specials]) :> sp]];
    Flatten @ Map[
        seg |-> If[KeyExistsQ[specials, seg], {specials[seg]}, dsvEncodePlain[tok, seg]],
        segs
    ]
]

(* Split the ids into maximal runs of one kind (special vs regular), then join:
   a special run renders as its literal contents, a regular run byte-level
   decodes (token chars -> bytes -> UTF-8, the whole run as one string so a
   multibyte code point split across tokens still decodes). *)
dsvDecode[tok_Association, ids_List] := Block[{idToSpecial = tok["IdToSpecial"], idToTok = tok["IdToTok"], charToByte = tok["CharToByte"]},
    StringJoin @ Map[
        run |-> If[ KeyExistsQ[idToSpecial, First[run]],
            StringJoin[idToSpecial /@ run],
            FromCharacterCode[Lookup[charToByte, Characters @ StringJoin[idToTok /@ run]], "UTF-8"]
        ],
        Split[ids, KeyExistsQ[idToSpecial, #1] === KeyExistsQ[idToSpecial, #2] &]
    ]
]

End[];

EndPackage[];
