ProofObject["EquationalLogic", {ForAll[{\[FormalA], \[FormalB], \[FormalC]}, 
   \[FormalA] \[CircleTimes] (\[FormalB] \[CircleTimes] \[FormalC]) == 
    (\[FormalA] \[CircleTimes] \[FormalB]) \[CircleTimes] \[FormalC]]}, 
 {ForAll[{\[FormalA], \[FormalB], \[FormalC]}, 
   ((\[FormalA] \[CircleTimes] \[FormalB]) \[CircleTimes] 
      \[FormalC]) \[CircleTimes] OverBar[\[FormalA] \[CircleTimes] 
       \[FormalC]] == \[FormalB]]}, 
 <|"Variables" -> {\[FormalA], \[FormalB], \[FormalC], \[FormalD], 
    \[FormalE], \[FormalF], \[FormalAlpha]}, "Constants" -> {}, 
  "Proof" -> {{"Axiom", 1} -> <|"Statement" -> 
       HoldForm[\[FormalA] == ((\[FormalB] \[CircleTimes] 
            \[FormalA]) \[CircleTimes] \[FormalC]) \[CircleTimes] 
          OverBar[\[FormalB] \[CircleTimes] \[FormalC]]], "Proof" -> <||>|>, 
    {"Hypothesis", 1} -> <|"Statement" -> 
       HoldForm[(\[FormalD] \[CircleTimes] \[FormalE]) \[CircleTimes] 
          \[FormalF] == \[FormalD] \[CircleTimes] (\[FormalE] \[CircleTimes] 
           \[FormalF])], "Proof" -> <||>|>, {"CriticalPairLemma", 1} -> 
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
         OverBar[\[FormalA]] \[CircleTimes] OverBar[\[FormalB] \[CircleTimes] 
            OverBar[\[FormalB]]]], "Proof" -> <|"Construct" -> {"Axiom", 1}, 
        "Orientation" -> -1, "Rule" -> 
         (((\[FormalA]_) \[CircleTimes] (\[FormalB]_)) \[CircleTimes] 
            (\[FormalC]_)) \[CircleTimes] OverBar[
            (\[FormalA]_) \[CircleTimes] (\[FormalC]_)] -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> ((\[FormalA]_) \[CircleTimes] 
           (\[FormalB]_)) \[CircleTimes] (\[FormalC]_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 1}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CircleTimes] OverBar[\[FormalB]_]) \[CircleTimes] 
           OverBar[\[FormalA]_] -> OverBar[\[FormalB]], "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 6} -> 
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
        "Position" -> {1}|>|>, {"CriticalPairLemma", 7} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA] \[CircleTimes] 
           OverBar[\[FormalA] \[CircleTimes] \[FormalB]]] == \[FormalB]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 6}, 
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
    {"CriticalPairLemma", 8} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA]] == 
         OverBar[OverBar[OverBar[\[FormalA]]]]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 7}, 
        "Orientation" -> 1, "Rule" -> OverBar[(\[FormalA]_) \[CircleTimes] 
            OverBar[(\[FormalA]_) \[CircleTimes] (\[FormalB]_)]] -> 
          \[FormalB], "Side" -> 1, "Subpattern" -> 
         (\[FormalA]_) \[CircleTimes] OverBar[(\[FormalA]_) \[CircleTimes] 
            (\[FormalB]_)], "MatchingConstruct" -> {"CriticalPairLemma", 6}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         OverBar[\[FormalA]_] \[CircleTimes] OverBar[
            (\[FormalB]_) \[CircleTimes] OverBar[\[FormalA]_]] -> 
          OverBar[\[FormalB]], "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 9} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA] \[CircleTimes] 
           OverBar[\[FormalA] \[CircleTimes] \[FormalB]]] == 
         (\[FormalC] \[CircleTimes] \[FormalB]) \[CircleTimes] 
          OverBar[\[FormalC]]], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 1}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CircleTimes] OverBar[
             \[FormalB]_]) \[CircleTimes] OverBar[\[FormalA]_] -> 
          OverBar[\[FormalB]], "Side" -> 1, "Subpattern" -> 
         OverBar[\[FormalB]_], "MatchingConstruct" -> {"CriticalPairLemma", 
          7}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         OverBar[(\[FormalA]_) \[CircleTimes] OverBar[
             (\[FormalA]_) \[CircleTimes] (\[FormalB]_)]] -> \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {1, 2}|>|>, 
    {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CircleTimes] 
           \[FormalA]) \[CircleTimes] OverBar[\[FormalB]]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 9}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 7}, "Orientation" -> 1, 
        "Rule" -> OverBar[(\[FormalA]_) \[CircleTimes] 
            OverBar[(\[FormalA]_) \[CircleTimes] (\[FormalB]_)]] -> 
          \[FormalB], "OutputExpression" -> HoldForm[\[FormalA] == 
           (\[FormalB] \[CircleTimes] \[FormalA]) \[CircleTimes] 
            OverBar[\[FormalB]]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 10} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA] \[CircleTimes] 
           OverBar[\[FormalA] \[CircleTimes] \[FormalB]]] == 
         OverBar[OverBar[\[FormalB]]]], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 8}, "Orientation" -> -1, 
        "Rule" -> OverBar[OverBar[OverBar[\[FormalA]_]]] -> 
          OverBar[\[FormalA]], "Side" -> 1, "Subpattern" -> 
         OverBar[\[FormalA]_], "MatchingConstruct" -> {"CriticalPairLemma", 
          7}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         OverBar[(\[FormalA]_) \[CircleTimes] OverBar[
             (\[FormalA]_) \[CircleTimes] (\[FormalB]_)]] -> \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {1, 1}|>|>, 
    {"SubstitutionLemma", 3} -> 
     <|"Statement" -> HoldForm[\[FormalA] == OverBar[OverBar[\[FormalA]]]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 10}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 7}, "Orientation" -> 1, 
        "Rule" -> OverBar[(\[FormalA]_) \[CircleTimes] 
            OverBar[(\[FormalA]_) \[CircleTimes] (\[FormalB]_)]] -> 
          \[FormalB], "OutputExpression" -> HoldForm[\[FormalA] == 
           OverBar[OverBar[\[FormalA]]]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 11} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA] \[CircleTimes] 
           OverBar[\[FormalB]]] == OverBar[\[FormalA]] \[CircleTimes] 
          OverBar[OverBar[\[FormalB]]]], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 2}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CircleTimes] (\[FormalB]_)) \[CircleTimes] 
           OverBar[\[FormalA]_] -> \[FormalB], "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CircleTimes] (\[FormalB]_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 6}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         OverBar[\[FormalA]_] \[CircleTimes] OverBar[
            (\[FormalB]_) \[CircleTimes] OverBar[\[FormalA]_]] -> 
          OverBar[\[FormalB]], "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"SubstitutionLemma", 4} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA] \[CircleTimes] 
           OverBar[\[FormalB]]] == OverBar[\[FormalA]] \[CircleTimes] 
          \[FormalB]], "Proof" -> <|"Input" -> {"CriticalPairLemma", 11}, 
        "Position" -> {2}, "Construct" -> {"SubstitutionLemma", 3}, 
        "Orientation" -> -1, "Rule" -> OverBar[OverBar[\[FormalA]_]] -> 
          \[FormalA], "OutputExpression" -> HoldForm[
          OverBar[\[FormalA] \[CircleTimes] OverBar[\[FormalB]]] == 
           OverBar[\[FormalA]] \[CircleTimes] \[FormalB]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 5} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA]] \[CircleTimes] 
          (OverBar[\[FormalB]] \[CircleTimes] \[FormalB]) == 
         OverBar[\[FormalA]]], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 5}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 4}, "Orientation" -> 1, 
        "Rule" -> OverBar[(\[FormalA]_) \[CircleTimes] 
            OverBar[\[FormalB]_]] -> OverBar[\[FormalA]] \[CircleTimes] 
           \[FormalB], "OutputExpression" -> HoldForm[
          OverBar[\[FormalA]] \[CircleTimes] 
            (OverBar[\[FormalB]] \[CircleTimes] \[FormalB]) == 
           OverBar[\[FormalA]]], "ConstructSide" -> 1, "InputOrientation" -> 
         -1, "Side" -> 1|>|>, {"SubstitutionLemma", 6} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA]] \[CircleTimes] 
          (\[FormalA] \[CircleTimes] \[FormalB]) == \[FormalB]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 7}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 4}, "Orientation" -> 1, 
        "Rule" -> OverBar[(\[FormalA]_) \[CircleTimes] 
            OverBar[\[FormalB]_]] -> OverBar[\[FormalA]] \[CircleTimes] 
           \[FormalB], "OutputExpression" -> HoldForm[
          OverBar[\[FormalA]] \[CircleTimes] (\[FormalA] \[CircleTimes] 
             \[FormalB]) == \[FormalB]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 12} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] \[FormalB] == 
         \[FormalB] \[CircleTimes] OverBar[OverBar[\[FormalA]]]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 2}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CircleTimes] 
            (\[FormalB]_)) \[CircleTimes] OverBar[\[FormalA]_] -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CircleTimes] 
          (\[FormalB]_), "MatchingConstruct" -> {"SubstitutionLemma", 6}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         OverBar[\[FormalA]_] \[CircleTimes] ((\[FormalA]_) \[CircleTimes] 
            (\[FormalB]_)) -> \[FormalB], "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 7} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] \[FormalB] == 
         \[FormalB] \[CircleTimes] \[FormalA]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 12}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Orientation" -> -1, 
        "Rule" -> OverBar[OverBar[\[FormalA]_]] -> \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CircleTimes] 
            \[FormalB] == \[FormalB] \[CircleTimes] \[FormalA]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 13} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         OverBar[\[FormalB]] \[CircleTimes] (\[FormalA] \[CircleTimes] 
           \[FormalB])], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 
          6}, "Orientation" -> 1, "Rule" -> 
         OverBar[\[FormalA]_] \[CircleTimes] ((\[FormalA]_) \[CircleTimes] 
            (\[FormalB]_)) -> \[FormalB], "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CircleTimes] (\[FormalB]_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 7}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleTimes] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CircleTimes] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 14} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA]] == 
         OverBar[\[FormalB] \[CircleTimes] \[FormalA]] \[CircleTimes] 
          \[FormalB]], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 13}, 
        "Orientation" -> -1, "Rule" -> OverBar[\[FormalA]_] \[CircleTimes] 
           ((\[FormalB]_) \[CircleTimes] (\[FormalA]_)) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CircleTimes] 
          (\[FormalA]_), "MatchingConstruct" -> {"CriticalPairLemma", 13}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         OverBar[\[FormalA]_] \[CircleTimes] ((\[FormalB]_) \[CircleTimes] 
            (\[FormalA]_)) -> \[FormalB], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 8} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA]] == 
         \[FormalB] \[CircleTimes] OverBar[\[FormalB] \[CircleTimes] 
            \[FormalA]]], "Proof" -> <|"Input" -> {"CriticalPairLemma", 14}, 
        "Position" -> {}, "Construct" -> {"SubstitutionLemma", 7}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CircleTimes] 
           (\[FormalB]_) -> \[FormalB] \[CircleTimes] \[FormalA], 
        "OutputExpression" -> HoldForm[OverBar[\[FormalA]] == 
           \[FormalB] \[CircleTimes] OverBar[\[FormalB] \[CircleTimes] 
              \[FormalA]]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 9} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA]] \[CircleTimes] 
          (\[FormalB] \[CircleTimes] OverBar[\[FormalB]]) == 
         OverBar[\[FormalA]]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 5}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 7}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] (\[FormalB]_) -> 
          \[FormalB] \[CircleTimes] \[FormalA], "OutputExpression" -> 
         HoldForm[OverBar[\[FormalA]] \[CircleTimes] 
            (\[FormalB] \[CircleTimes] OverBar[\[FormalB]]) == 
           OverBar[\[FormalA]]], "ConstructSide" -> 1, "InputOrientation" -> 
         1, "Side" -> 1|>|>, {"CriticalPairLemma", 15} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[\[FormalA]]] == 
         \[FormalA] \[CircleTimes] (\[FormalB] \[CircleTimes] 
           OverBar[\[FormalB]])], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 9}, "Orientation" -> 1, 
        "Rule" -> OverBar[\[FormalA]_] \[CircleTimes] 
           ((\[FormalB]_) \[CircleTimes] OverBar[\[FormalB]_]) -> 
          OverBar[\[FormalA]], "Side" -> 1, "Subpattern" -> 
         OverBar[\[FormalA]_], "MatchingConstruct" -> {"SubstitutionLemma", 
          3}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         OverBar[OverBar[\[FormalA]_]] -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 10} -> 
     <|"Statement" -> HoldForm[\[FormalA] == \[FormalA] \[CircleTimes] 
          (\[FormalB] \[CircleTimes] OverBar[\[FormalB]])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 15}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Orientation" -> -1, 
        "Rule" -> OverBar[OverBar[\[FormalA]_]] -> \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalA] == 
           \[FormalA] \[CircleTimes] (\[FormalB] \[CircleTimes] 
             OverBar[\[FormalB]])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 16} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA] \[CircleTimes] 
           \[FormalB]] == \[FormalC] \[CircleTimes] 
          OverBar[((\[FormalA] \[CircleTimes] \[FormalC]) \[CircleTimes] 
             \[FormalB]) \[CircleTimes] (\[FormalAlpha] \[CircleTimes] 
             OverBar[\[FormalAlpha]])]], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 2}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CircleTimes] (\[FormalB]_)) \[CircleTimes] 
           OverBar[(((\[FormalC]_) \[CircleTimes] (
                \[FormalA]_)) \[CircleTimes] 
              (\[FormalAlpha]_)) \[CircleTimes] (\[FormalB]_)] -> 
          OverBar[\[FormalC] \[CircleTimes] \[FormalAlpha]], "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CircleTimes] (\[FormalB]_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 10}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleTimes] ((\[FormalB]_) \[CircleTimes] 
            OverBar[\[FormalB]_]) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 11} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA] \[CircleTimes] 
           \[FormalB]] == \[FormalC] \[CircleTimes] 
          OverBar[(\[FormalA] \[CircleTimes] \[FormalC]) \[CircleTimes] 
            \[FormalB]]], "Proof" -> <|"Input" -> {"CriticalPairLemma", 16}, 
        "Position" -> {2, 1}, "Construct" -> {"SubstitutionLemma", 10}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CircleTimes] 
           ((\[FormalB]_) \[CircleTimes] OverBar[\[FormalB]_]) -> \[FormalA], 
        "OutputExpression" -> HoldForm[OverBar[\[FormalA] \[CircleTimes] 
             \[FormalB]] == \[FormalC] \[CircleTimes] 
            OverBar[(\[FormalA] \[CircleTimes] \[FormalC]) \[CircleTimes] 
              \[FormalB]]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 17} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[(\[FormalA] \[CircleTimes] 
             \[FormalB]) \[CircleTimes] \[FormalC]]] == 
         \[FormalB] \[CircleTimes] OverBar[OverBar[\[FormalA] \[CircleTimes] 
             \[FormalC]]]], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 8}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] OverBar[
            (\[FormalA]_) \[CircleTimes] (\[FormalB]_)] -> 
          OverBar[\[FormalB]], "Side" -> 1, "Subpattern" -> 
         (\[FormalA]_) \[CircleTimes] (\[FormalB]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 11}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CircleTimes] 
           OverBar[((\[FormalB]_) \[CircleTimes] 
              (\[FormalA]_)) \[CircleTimes] (\[FormalC]_)] -> 
          OverBar[\[FormalB] \[CircleTimes] \[FormalC]], "MatchingSide" -> 1, 
        "Position" -> {2, 1}|>|>, {"SubstitutionLemma", 12} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CircleTimes] 
           \[FormalB]) \[CircleTimes] \[FormalC] == \[FormalB] \[CircleTimes] 
          OverBar[OverBar[\[FormalA] \[CircleTimes] \[FormalC]]]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 17}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Orientation" -> -1, 
        "Rule" -> OverBar[OverBar[\[FormalA]_]] -> \[FormalA], 
        "OutputExpression" -> HoldForm[(\[FormalA] \[CircleTimes] 
             \[FormalB]) \[CircleTimes] \[FormalC] == 
           \[FormalB] \[CircleTimes] OverBar[OverBar[
              \[FormalA] \[CircleTimes] \[FormalC]]]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 13} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CircleTimes] 
           \[FormalB]) \[CircleTimes] \[FormalC] == \[FormalB] \[CircleTimes] 
          (\[FormalA] \[CircleTimes] \[FormalC])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 12}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Orientation" -> -1, 
        "Rule" -> OverBar[OverBar[\[FormalA]_]] -> \[FormalA], 
        "OutputExpression" -> HoldForm[(\[FormalA] \[CircleTimes] 
             \[FormalB]) \[CircleTimes] \[FormalC] == 
           \[FormalB] \[CircleTimes] (\[FormalA] \[CircleTimes] \[FormalC])], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 14} -> 
     <|"Statement" -> HoldForm[(\[FormalE] \[CircleTimes] 
           \[FormalD]) \[CircleTimes] \[FormalF] == \[FormalD] \[CircleTimes] 
          (\[FormalE] \[CircleTimes] \[FormalF])], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Position" -> {1}, 
        "Construct" -> {"SubstitutionLemma", 7}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] (\[FormalB]_) -> 
          \[FormalB] \[CircleTimes] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalE] \[CircleTimes] \[FormalD]) \[CircleTimes] 
            \[FormalF] == \[FormalD] \[CircleTimes] 
            (\[FormalE] \[CircleTimes] \[FormalF])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"Conclusion", 1} -> <|"Statement" -> 
       HoldForm[\[FormalD] \[CircleTimes] (\[FormalE] \[CircleTimes] 
           \[FormalF]) == \[FormalD] \[CircleTimes] 
          (\[FormalE] \[CircleTimes] \[FormalF])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 14}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 13}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CircleTimes] (\[FormalB]_)) \[CircleTimes] 
           (\[FormalC]_) -> \[FormalB] \[CircleTimes] 
           (\[FormalA] \[CircleTimes] \[FormalC]), "OutputExpression" -> 
         HoldForm[\[FormalD] \[CircleTimes] (\[FormalE] \[CircleTimes] 
             \[FormalF]) == \[FormalD] \[CircleTimes] 
            (\[FormalE] \[CircleTimes] \[FormalF])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>}|>]
