ProofObject["EquationalLogic", {ForAll[{\[FormalA], \[FormalB]}, 
   \[FormalA] == \[FormalA] \[CircleTimes] (\[FormalA] \[CirclePlus] 
      \[FormalB])]}, {ForAll[{\[FormalA], \[FormalB]}, 
   \[FormalA] \[CircleTimes] \[FormalB] == \[FormalB] \[CircleTimes] 
     \[FormalA]], ForAll[{\[FormalA], \[FormalB], \[FormalC]}, 
   \[FormalA] \[CircleTimes] (\[FormalB] \[CirclePlus] \[FormalC]) == 
    \[FormalA] \[CircleTimes] \[FormalB] \[CirclePlus] 
     \[FormalA] \[CircleTimes] \[FormalC]], ForAll[{\[FormalA], \[FormalB]}, 
   \[FormalA] \[CirclePlus] \[FormalB] \[CircleTimes] OverBar[\[FormalB]] == 
    \[FormalA]], ForAll[{\[FormalA], \[FormalB]}, 
   \[FormalA] \[CircleTimes] (\[FormalB] \[CirclePlus] 
      OverBar[\[FormalB]]) == \[FormalA]], ForAll[{\[FormalA], \[FormalB]}, 
   \[FormalA] \[CirclePlus] \[FormalB] == \[FormalB] \[CirclePlus] 
     \[FormalA]], ForAll[{\[FormalA], \[FormalB], \[FormalC]}, 
   \[FormalA] \[CirclePlus] \[FormalB] \[CircleTimes] \[FormalC] == 
    (\[FormalA] \[CirclePlus] \[FormalB]) \[CircleTimes] 
     (\[FormalA] \[CirclePlus] \[FormalC])]}, 
 <|"Variables" -> {\[FormalA], \[FormalB], \[FormalC], \[FormalD], 
    \[FormalE], \[FormalF], \[FormalG], \[FormalH], \[FormalI], \[FormalJ], 
    \[FormalK], \[FormalL], \[FormalM], \[FormalN], \[FormalO], \[FormalP]}, 
  "Constants" -> {}, "Proof" -> 
   {{"Axiom", 1} -> <|"Statement" -> HoldForm[\[FormalA] == 
         \[FormalA] \[CircleTimes] (\[FormalB] \[CirclePlus] 
           OverBar[\[FormalB]])], "Proof" -> <||>|>, 
    {"Axiom", 2} -> <|"Statement" -> HoldForm[\[FormalA] == 
         \[FormalA] \[CirclePlus] \[FormalB] \[CircleTimes] 
           OverBar[\[FormalB]]], "Proof" -> <||>|>, 
    {"Axiom", 3} -> <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          \[FormalB] == \[FormalB] \[CircleTimes] \[FormalA]], 
      "Proof" -> <||>|>, {"Axiom", 4} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          (\[FormalB] \[CirclePlus] \[FormalC]) == 
         \[FormalA] \[CircleTimes] \[FormalB] \[CirclePlus] 
          \[FormalA] \[CircleTimes] \[FormalC]], "Proof" -> <||>|>, 
    {"Axiom", 5} -> <|"Statement" -> HoldForm[
        (\[FormalA] \[CirclePlus] \[FormalB]) \[CircleTimes] 
          (\[FormalA] \[CirclePlus] \[FormalC]) == \[FormalA] \[CirclePlus] 
          \[FormalB] \[CircleTimes] \[FormalC]], "Proof" -> <||>|>, 
    {"Axiom", 6} -> <|"Statement" -> HoldForm[
        \[FormalA] \[CirclePlus] \[FormalB] == \[FormalB] \[CirclePlus] 
          \[FormalA]], "Proof" -> <||>|>, {"Hypothesis", 1} -> 
     <|"Statement" -> HoldForm[\[FormalO] \[CircleTimes] 
          (\[FormalO] \[CirclePlus] \[FormalP]) == \[FormalO]], 
      "Proof" -> <||>|>, {"CriticalPairLemma", 1} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CirclePlus] 
           OverBar[\[FormalA]]) \[CircleTimes] \[FormalB] == \[FormalB]], 
      "Proof" -> <|"Construct" -> {"Axiom", 3}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CircleTimes] (\[FormalA]_), "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CircleTimes] (\[FormalB]_), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CircleTimes] 
           ((\[FormalB]_) \[CirclePlus] OverBar[\[FormalB]_]) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"CriticalPairLemma", 2} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          (\[FormalB] \[CirclePlus] OverBar[\[FormalA]]) == 
         \[FormalA] \[CircleTimes] \[FormalB]], 
      "Proof" -> <|"Construct" -> {"Axiom", 4}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] (\[FormalB]_) \[CirclePlus] 
           (\[FormalA]_) \[CircleTimes] (\[FormalC]_) -> 
          \[FormalA] \[CircleTimes] (\[FormalB] \[CirclePlus] \[FormalC]), 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CircleTimes] 
           (\[FormalB]_) \[CirclePlus] (\[FormalA]_) \[CircleTimes] 
           (\[FormalC]_), "MatchingConstruct" -> {"Axiom", 2}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CirclePlus] (\[FormalB]_) \[CircleTimes] 
            OverBar[\[FormalB]_] -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 3} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CirclePlus] 
          \[FormalB] \[CircleTimes] OverBar[\[FormalA]] == 
         \[FormalA] \[CirclePlus] \[FormalB]], 
      "Proof" -> <|"Construct" -> {"Axiom", 5}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CirclePlus] (\[FormalB]_)) \[CircleTimes] 
           ((\[FormalA]_) \[CirclePlus] (\[FormalC]_)) -> 
          \[FormalA] \[CirclePlus] \[FormalB] \[CircleTimes] \[FormalC], 
        "Side" -> 1, "Subpattern" -> ((\[FormalA]_) \[CirclePlus] 
           (\[FormalB]_)) \[CircleTimes] ((\[FormalA]_) \[CirclePlus] 
           (\[FormalC]_)), "MatchingConstruct" -> {"Axiom", 1}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleTimes] ((\[FormalB]_) \[CirclePlus] 
            OverBar[\[FormalB]_]) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 4} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
           OverBar[\[FormalA]] \[CirclePlus] \[FormalB] == \[FormalB]], 
      "Proof" -> <|"Construct" -> {"Axiom", 6}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CirclePlus] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CirclePlus] (\[FormalA]_), "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CirclePlus] (\[FormalB]_), 
        "MatchingConstruct" -> {"Axiom", 2}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CirclePlus] 
           (\[FormalB]_) \[CircleTimes] OverBar[\[FormalB]_] -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"CriticalPairLemma", 5} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CirclePlus] \[FormalB] == 
         \[FormalA] \[CirclePlus] OverBar[\[FormalA]] \[CircleTimes] 
           \[FormalB]], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 1}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CirclePlus] 
            OverBar[\[FormalA]_]) \[CircleTimes] (\[FormalB]_) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> ((\[FormalA]_) \[CirclePlus] 
           OverBar[\[FormalA]_]) \[CircleTimes] (\[FormalB]_), 
        "MatchingConstruct" -> {"Axiom", 5}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> ((\[FormalA]_) \[CirclePlus] 
            (\[FormalB]_)) \[CircleTimes] ((\[FormalA]_) \[CirclePlus] 
            (\[FormalC]_)) -> \[FormalA] \[CirclePlus] 
           \[FormalB] \[CircleTimes] \[FormalC], "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 6} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] \[FormalB] == 
         \[FormalA] \[CircleTimes] (OverBar[\[FormalA]] \[CirclePlus] 
           \[FormalB])], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 
          4}, "Orientation" -> 1, "Rule" -> 
         (\[FormalA]_) \[CircleTimes] OverBar[\[FormalA]_] \[CirclePlus] 
           (\[FormalB]_) -> \[FormalB], "Side" -> 1, "Subpattern" -> 
         (\[FormalA]_) \[CircleTimes] OverBar[\[FormalA]_] \[CirclePlus] 
          (\[FormalB]_), "MatchingConstruct" -> {"Axiom", 4}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleTimes] (\[FormalB]_) \[CirclePlus] 
           (\[FormalA]_) \[CircleTimes] (\[FormalC]_) -> 
          \[FormalA] \[CircleTimes] (\[FormalB] \[CirclePlus] \[FormalC]), 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"CriticalPairLemma", 7} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] \[FormalA] == 
         \[FormalA]], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 2}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CircleTimes] 
           ((\[FormalB]_) \[CirclePlus] OverBar[\[FormalA]_]) -> 
          \[FormalA] \[CircleTimes] \[FormalB], "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CircleTimes] 
          ((\[FormalB]_) \[CirclePlus] OverBar[\[FormalA]_]), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CircleTimes] 
           ((\[FormalB]_) \[CirclePlus] OverBar[\[FormalB]_]) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"CriticalPairLemma", 8} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          (\[FormalA] \[CirclePlus] \[FormalB]) == \[FormalA] \[CirclePlus] 
          \[FormalA] \[CircleTimes] \[FormalB]], 
      "Proof" -> <|"Construct" -> {"Axiom", 4}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] (\[FormalB]_) \[CirclePlus] 
           (\[FormalA]_) \[CircleTimes] (\[FormalC]_) -> 
          \[FormalA] \[CircleTimes] (\[FormalB] \[CirclePlus] \[FormalC]), 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CircleTimes] 
          (\[FormalB]_), "MatchingConstruct" -> {"CriticalPairLemma", 7}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleTimes] (\[FormalA]_) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 9} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CirclePlus] 
          OverBar[OverBar[\[FormalA]]] == \[FormalA]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 5}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CirclePlus] 
           OverBar[\[FormalA]_] \[CircleTimes] (\[FormalB]_) -> 
          \[FormalA] \[CirclePlus] \[FormalB], "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CirclePlus] 
          OverBar[\[FormalA]_] \[CircleTimes] (\[FormalB]_), 
        "MatchingConstruct" -> {"Axiom", 2}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CirclePlus] 
           (\[FormalB]_) \[CircleTimes] OverBar[\[FormalB]_] -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"CriticalPairLemma", 10} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          OverBar[OverBar[OverBar[\[FormalA]]]] == \[FormalA] \[CircleTimes] 
          OverBar[\[FormalA]]], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 6}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] 
           (OverBar[\[FormalA]_] \[CirclePlus] (\[FormalB]_)) -> 
          \[FormalA] \[CircleTimes] \[FormalB], "Side" -> 1, 
        "Subpattern" -> OverBar[\[FormalA]_] \[CirclePlus] (\[FormalB]_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 9}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CirclePlus] OverBar[OverBar[\[FormalA]_]] -> 
          \[FormalA], "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 11} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[\[FormalA]]] \[CirclePlus] 
          \[FormalA] == OverBar[OverBar[\[FormalA]]] \[CirclePlus] 
          \[FormalA] \[CircleTimes] OverBar[\[FormalA]]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 3}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CirclePlus] 
           (\[FormalB]_) \[CircleTimes] OverBar[\[FormalA]_] -> 
          \[FormalA] \[CirclePlus] \[FormalB], "Side" -> 1, 
        "Subpattern" -> (\[FormalB]_) \[CircleTimes] OverBar[\[FormalA]_], 
        "MatchingConstruct" -> {"CriticalPairLemma", 10}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleTimes] OverBar[OverBar[
             OverBar[\[FormalA]_]]] -> \[FormalA] \[CircleTimes] 
           OverBar[\[FormalA]], "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[\[FormalA]]] \[CirclePlus] 
          \[FormalA] == OverBar[OverBar[\[FormalA]]]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 11}, "Position" -> {}, 
        "Construct" -> {"Axiom", 2}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CirclePlus] (\[FormalB]_) \[CircleTimes] 
            OverBar[\[FormalB]_] -> \[FormalA], "OutputExpression" -> 
         HoldForm[OverBar[OverBar[\[FormalA]]] \[CirclePlus] \[FormalA] == 
           OverBar[OverBar[\[FormalA]]]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CirclePlus] 
          OverBar[OverBar[\[FormalA]]] == OverBar[OverBar[\[FormalA]]]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 1}, "Position" -> {}, 
        "Construct" -> {"Axiom", 6}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CirclePlus] (\[FormalB]_) -> 
          \[FormalB] \[CirclePlus] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CirclePlus] OverBar[OverBar[\[FormalA]]] == 
           OverBar[OverBar[\[FormalA]]]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, {"SubstitutionLemma", 3} -> 
     <|"Statement" -> HoldForm[\[FormalA] == OverBar[OverBar[\[FormalA]]]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 2}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 9}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CirclePlus] OverBar[
            OverBar[\[FormalA]_]] -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == OverBar[OverBar[\[FormalA]]]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 12} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA]] \[CircleTimes] 
          \[FormalB] == OverBar[\[FormalA]] \[CircleTimes] 
          (\[FormalA] \[CirclePlus] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 6}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CircleTimes] 
           (OverBar[\[FormalA]_] \[CirclePlus] (\[FormalB]_)) -> 
          \[FormalA] \[CircleTimes] \[FormalB], "Side" -> 1, 
        "Subpattern" -> OverBar[\[FormalA]_], "MatchingConstruct" -> 
         {"SubstitutionLemma", 3}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> OverBar[OverBar[\[FormalA]_]] -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"CriticalPairLemma", 13} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CirclePlus] 
          (\[FormalA] \[CirclePlus] \[FormalB]) == \[FormalA] \[CirclePlus] 
          OverBar[\[FormalA]] \[CircleTimes] \[FormalB]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 5}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CirclePlus] 
           OverBar[\[FormalA]_] \[CircleTimes] (\[FormalB]_) -> 
          \[FormalA] \[CirclePlus] \[FormalB], "Side" -> 1, 
        "Subpattern" -> OverBar[\[FormalA]_] \[CircleTimes] (\[FormalB]_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 12}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         OverBar[\[FormalA]_] \[CircleTimes] ((\[FormalA]_) \[CirclePlus] 
            (\[FormalB]_)) -> OverBar[\[FormalA]] \[CircleTimes] \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 4} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CirclePlus] 
          (\[FormalA] \[CirclePlus] \[FormalB]) == \[FormalA] \[CirclePlus] 
          \[FormalB]], "Proof" -> <|"Input" -> {"CriticalPairLemma", 13}, 
        "Position" -> {}, "Construct" -> {"CriticalPairLemma", 5}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CirclePlus] 
           OverBar[\[FormalA]_] \[CircleTimes] (\[FormalB]_) -> 
          \[FormalA] \[CirclePlus] \[FormalB], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CirclePlus] (\[FormalA] \[CirclePlus] 
             \[FormalB]) == \[FormalA] \[CirclePlus] \[FormalB]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 14} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CirclePlus] 
          \[FormalB] \[CircleTimes] (\[FormalA] \[CirclePlus] \[FormalC]) == 
         (\[FormalA] \[CirclePlus] \[FormalB]) \[CircleTimes] 
          (\[FormalA] \[CirclePlus] \[FormalC])], 
      "Proof" -> <|"Construct" -> {"Axiom", 5}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CirclePlus] (\[FormalB]_)) \[CircleTimes] 
           ((\[FormalA]_) \[CirclePlus] (\[FormalC]_)) -> 
          \[FormalA] \[CirclePlus] \[FormalB] \[CircleTimes] \[FormalC], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CirclePlus] 
          (\[FormalC]_), "MatchingConstruct" -> {"SubstitutionLemma", 4}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CirclePlus] ((\[FormalA]_) \[CirclePlus] 
            (\[FormalB]_)) -> \[FormalA] \[CirclePlus] \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 5} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CirclePlus] 
          \[FormalB] \[CircleTimes] (\[FormalA] \[CirclePlus] \[FormalC]) == 
         \[FormalA] \[CirclePlus] \[FormalB] \[CircleTimes] \[FormalC]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 14}, "Position" -> {}, 
        "Construct" -> {"Axiom", 5}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CirclePlus] (\[FormalB]_)) \[CircleTimes] 
           ((\[FormalA]_) \[CirclePlus] (\[FormalC]_)) -> 
          \[FormalA] \[CirclePlus] \[FormalB] \[CircleTimes] \[FormalC], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CirclePlus] 
            \[FormalB] \[CircleTimes] (\[FormalA] \[CirclePlus] 
              \[FormalC]) == \[FormalA] \[CirclePlus] 
            \[FormalB] \[CircleTimes] \[FormalC]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 15} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CirclePlus] 
          \[FormalB] \[CircleTimes] OverBar[\[FormalB]] == 
         \[FormalA] \[CirclePlus] \[FormalB] \[CircleTimes] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 5}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CirclePlus] 
           (\[FormalB]_) \[CircleTimes] ((\[FormalA]_) \[CirclePlus] 
             (\[FormalC]_)) -> \[FormalA] \[CirclePlus] 
           \[FormalB] \[CircleTimes] \[FormalC], "Side" -> 1, 
        "Subpattern" -> (\[FormalB]_) \[CircleTimes] 
          ((\[FormalA]_) \[CirclePlus] (\[FormalC]_)), "MatchingConstruct" -> 
         {"CriticalPairLemma", 2}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CircleTimes] 
           ((\[FormalB]_) \[CirclePlus] OverBar[\[FormalA]_]) -> 
          \[FormalA] \[CircleTimes] \[FormalB], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 6} -> 
     <|"Statement" -> HoldForm[\[FormalA] == \[FormalA] \[CirclePlus] 
          \[FormalB] \[CircleTimes] \[FormalA]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 15}, "Position" -> {}, 
        "Construct" -> {"Axiom", 2}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CirclePlus] (\[FormalB]_) \[CircleTimes] 
            OverBar[\[FormalB]_] -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == \[FormalA] \[CirclePlus] 
            \[FormalB] \[CircleTimes] \[FormalA]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, {"SubstitutionLemma", 7} -> 
     <|"Statement" -> HoldForm[\[FormalO] \[CirclePlus] 
          \[FormalO] \[CircleTimes] \[FormalP] == \[FormalO]], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 8}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] ((\[FormalA]_) \[CirclePlus] 
            (\[FormalB]_)) -> \[FormalA] \[CirclePlus] 
           \[FormalA] \[CircleTimes] \[FormalB], "OutputExpression" -> 
         HoldForm[\[FormalO] \[CirclePlus] \[FormalO] \[CircleTimes] 
             \[FormalP] == \[FormalO]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, {"SubstitutionLemma", 8} -> 
     <|"Statement" -> HoldForm[\[FormalO] \[CirclePlus] 
          \[FormalP] \[CircleTimes] \[FormalO] == \[FormalO]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 7}, "Position" -> {2}, 
        "Construct" -> {"Axiom", 3}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] (\[FormalB]_) -> 
          \[FormalB] \[CircleTimes] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalO] \[CirclePlus] \[FormalP] \[CircleTimes] 
             \[FormalO] == \[FormalO]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"Conclusion", 1} -> <|"Statement" -> HoldForm[\[FormalO] == \[FormalO]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 8}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 6}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CirclePlus] (\[FormalB]_) \[CircleTimes] 
            (\[FormalA]_) -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalO] == \[FormalO]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>}|>]
