ProofObject["EquationalLogic", 
 {ForAll[\[FormalA], OverBar[OverBar[\[FormalA]]] == \[FormalA]]}, 
 {ForAll[{\[FormalA], \[FormalB], \[FormalC]}, 
   ((\[FormalA] \[CircleTimes] \[FormalB]) \[CircleTimes] 
      \[FormalC]) \[CircleTimes] OverBar[\[FormalA] \[CircleTimes] 
       \[FormalC]] == \[FormalB]]}, 
 <|"Variables" -> {\[FormalA], \[FormalB], \[FormalC], \[FormalD], 
    \[FormalAlpha]}, "Constants" -> {}, 
  "Proof" -> {{"Axiom", 1} -> <|"Statement" -> 
       HoldForm[\[FormalA] == ((\[FormalB] \[CircleTimes] 
            \[FormalA]) \[CircleTimes] \[FormalC]) \[CircleTimes] 
          OverBar[\[FormalB] \[CircleTimes] \[FormalC]]], "Proof" -> <||>|>, 
    {"Hypothesis", 1} -> <|"Statement" -> 
       HoldForm[OverBar[OverBar[\[FormalD]]] == \[FormalD]], 
      "Proof" -> <||>|>, {"CriticalPairLemma", 1} -> 
     <|"Statement" -> HoldForm[\[FormalA] == \[FormalB] \[CircleTimes] 
          OverBar[(\[FormalC] \[CircleTimes] \[FormalB]) \[CircleTimes] 
            OverBar[\[FormalC] \[CircleTimes] \[FormalA]]]], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> (((\[FormalA]_) \[CircleTimes] 
             (\[FormalB]_)) \[CircleTimes] (\[FormalC]_)) \[CircleTimes] 
           OverBar[(\[FormalA]_) \[CircleTimes] (\[FormalC]_)] -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> ((\[FormalA]_) \[CircleTimes] 
           (\[FormalB]_)) \[CircleTimes] (\[FormalC]_), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (((\[FormalA]_) \[CircleTimes] 
             (\[FormalB]_)) \[CircleTimes] (\[FormalC]_)) \[CircleTimes] 
           OverBar[(\[FormalA]_) \[CircleTimes] (\[FormalC]_)] -> \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 2} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA] \[CircleTimes] 
           \[FormalB]] == (\[FormalC] \[CircleTimes] 
           \[FormalAlpha]) \[CircleTimes] OverBar[
           ((\[FormalA] \[CircleTimes] \[FormalC]) \[CircleTimes] 
             \[FormalB]) \[CircleTimes] \[FormalAlpha]]], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> (((\[FormalA]_) \[CircleTimes] 
             (\[FormalB]_)) \[CircleTimes] (\[FormalC]_)) \[CircleTimes] 
           OverBar[(\[FormalA]_) \[CircleTimes] (\[FormalC]_)] -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CircleTimes] 
          (\[FormalB]_), "MatchingConstruct" -> {"Axiom", 1}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (((\[FormalA]_) \[CircleTimes] (\[FormalB]_)) \[CircleTimes] 
            (\[FormalC]_)) \[CircleTimes] OverBar[
            (\[FormalA]_) \[CircleTimes] (\[FormalC]_)] -> \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {1, 1}|>|>, 
    {"CriticalPairLemma", 3} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA] \[CircleTimes] 
           \[FormalB]] == (\[FormalC] \[CircleTimes] 
           OverBar[\[FormalA] \[CircleTimes] \[FormalB]]) \[CircleTimes] 
          OverBar[\[FormalC]]], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 2}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CircleTimes] (\[FormalB]_)) \[CircleTimes] 
           OverBar[(((\[FormalC]_) \[CircleTimes] (
                \[FormalA]_)) \[CircleTimes] 
              (\[FormalAlpha]_)) \[CircleTimes] (\[FormalB]_)] -> 
          OverBar[\[FormalC] \[CircleTimes] \[FormalAlpha]], "Side" -> 1, 
        "Subpattern" -> (((\[FormalC]_) \[CircleTimes] 
            (\[FormalA]_)) \[CircleTimes] (\[FormalAlpha]_)) \[CircleTimes] 
          (\[FormalB]_), "MatchingConstruct" -> {"Axiom", 1}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (((\[FormalA]_) \[CircleTimes] (\[FormalB]_)) \[CircleTimes] 
            (\[FormalC]_)) \[CircleTimes] OverBar[
            (\[FormalA]_) \[CircleTimes] (\[FormalC]_)] -> \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"CriticalPairLemma", 4} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA] \[CircleTimes] 
           OverBar[(\[FormalB] \[CircleTimes] \[FormalA]) \[CircleTimes] 
             OverBar[\[FormalB] \[CircleTimes] \[FormalC]]]] == 
         (\[FormalAlpha] \[CircleTimes] OverBar[\[FormalC]]) \[CircleTimes] 
          OverBar[\[FormalAlpha]]], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 3}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CircleTimes] OverBar[
             (\[FormalB]_) \[CircleTimes] (\[FormalC]_)]) \[CircleTimes] 
           OverBar[\[FormalA]_] -> OverBar[\[FormalB] \[CircleTimes] 
            \[FormalC]], "Side" -> 1, "Subpattern" -> 
         (\[FormalB]_) \[CircleTimes] (\[FormalC]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 1}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CircleTimes] 
           OverBar[((\[FormalB]_) \[CircleTimes] 
              (\[FormalA]_)) \[CircleTimes] OverBar[
              (\[FormalB]_) \[CircleTimes] (\[FormalC]_)]] -> \[FormalC], 
        "MatchingSide" -> 1, "Position" -> {1, 2, 1}|>|>, 
    {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA]] == 
         (\[FormalB] \[CircleTimes] OverBar[\[FormalA]]) \[CircleTimes] 
          OverBar[\[FormalB]]], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 4}, "Position" -> {1}, 
        "Construct" -> {"CriticalPairLemma", 1}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] OverBar[
            ((\[FormalB]_) \[CircleTimes] (\[FormalA]_)) \[CircleTimes] 
             OverBar[(\[FormalB]_) \[CircleTimes] (\[FormalC]_)]] -> 
          \[FormalC], "OutputExpression" -> HoldForm[OverBar[\[FormalA]] == 
           (\[FormalB] \[CircleTimes] OverBar[\[FormalA]]) \[CircleTimes] 
            OverBar[\[FormalB]]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, {"CriticalPairLemma", 5} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA]] == 
         OverBar[\[FormalB]] \[CircleTimes] OverBar[\[FormalA] \[CircleTimes] 
            OverBar[\[FormalB]]]], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 1}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CircleTimes] OverBar[
             \[FormalB]_]) \[CircleTimes] OverBar[\[FormalA]_] -> 
          OverBar[\[FormalB]], "Side" -> 1, "Subpattern" -> 
         (\[FormalA]_) \[CircleTimes] OverBar[\[FormalB]_], 
        "MatchingConstruct" -> {"SubstitutionLemma", 1}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CircleTimes] OverBar[\[FormalB]_]) \[CircleTimes] 
           OverBar[\[FormalA]_] -> OverBar[\[FormalB]], "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 6} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA] \[CircleTimes] 
           OverBar[\[FormalA] \[CircleTimes] \[FormalB]]] == \[FormalB]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 5}, 
        "Orientation" -> -1, "Rule" -> OverBar[\[FormalA]_] \[CircleTimes] 
           OverBar[(\[FormalB]_) \[CircleTimes] OverBar[\[FormalA]_]] -> 
          OverBar[\[FormalB]], "Side" -> 1, "Subpattern" -> 
         OverBar[\[FormalA]_] \[CircleTimes] OverBar[
           (\[FormalB]_) \[CircleTimes] OverBar[\[FormalA]_]], 
        "MatchingConstruct" -> {"CriticalPairLemma", 1}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleTimes] OverBar[((\[FormalB]_) \[CircleTimes] 
              (\[FormalA]_)) \[CircleTimes] OverBar[
              (\[FormalB]_) \[CircleTimes] (\[FormalC]_)]] -> \[FormalC], 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"CriticalPairLemma", 7} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA]] == 
         OverBar[OverBar[OverBar[\[FormalA]]]]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 6}, 
        "Orientation" -> 1, "Rule" -> OverBar[(\[FormalA]_) \[CircleTimes] 
            OverBar[(\[FormalA]_) \[CircleTimes] (\[FormalB]_)]] -> 
          \[FormalB], "Side" -> 1, "Subpattern" -> 
         (\[FormalA]_) \[CircleTimes] OverBar[(\[FormalA]_) \[CircleTimes] 
            (\[FormalB]_)], "MatchingConstruct" -> {"CriticalPairLemma", 5}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         OverBar[\[FormalA]_] \[CircleTimes] OverBar[
            (\[FormalB]_) \[CircleTimes] OverBar[\[FormalA]_]] -> 
          OverBar[\[FormalB]], "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 8} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA] \[CircleTimes] 
           OverBar[\[FormalA] \[CircleTimes] \[FormalB]]] == 
         OverBar[OverBar[\[FormalB]]]], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 7}, "Orientation" -> -1, 
        "Rule" -> OverBar[OverBar[OverBar[\[FormalA]_]]] -> 
          OverBar[\[FormalA]], "Side" -> 1, "Subpattern" -> 
         OverBar[\[FormalA]_], "MatchingConstruct" -> {"CriticalPairLemma", 
          6}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         OverBar[(\[FormalA]_) \[CircleTimes] OverBar[
             (\[FormalA]_) \[CircleTimes] (\[FormalB]_)]] -> \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {1, 1}|>|>, 
    {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[\[FormalA] == OverBar[OverBar[\[FormalA]]]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 8}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 6}, "Orientation" -> 1, 
        "Rule" -> OverBar[(\[FormalA]_) \[CircleTimes] 
            OverBar[(\[FormalA]_) \[CircleTimes] (\[FormalB]_)]] -> 
          \[FormalB], "OutputExpression" -> HoldForm[\[FormalA] == 
           OverBar[OverBar[\[FormalA]]]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"Conclusion", 1} -> <|"Statement" -> HoldForm[\[FormalD] == \[FormalD]], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 2}, "Orientation" -> -1, 
        "Rule" -> OverBar[OverBar[\[FormalA]_]] -> \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalD] == \[FormalD]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>}|>]
