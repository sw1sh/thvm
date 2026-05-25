ProofObject["EquationalLogic", {ForAll[{\[FormalA], \[FormalB], \[FormalC]}, 
   (\[FormalA] \[CircleTimes] \[FormalB]) \[CircleTimes] \[FormalC] == 
    \[FormalA] \[CircleTimes] (\[FormalB] \[CircleTimes] \[FormalC])]}, 
 {ForAll[{\[FormalA], \[FormalB]}, \[FormalA] \[CircleTimes] \[FormalB] == 
    \[FormalB] \[CircleTimes] \[FormalA]], 
  ForAll[{\[FormalA], \[FormalB], \[FormalC]}, 
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
    \[FormalK], \[FormalL], \[FormalM], \[FormalN], \[FormalO], \[FormalP], 
    \[FormalQ]}, "Constants" -> {}, 
  "Proof" -> {{"Axiom", 1} -> <|"Statement" -> 
       HoldForm[\[FormalA] == \[FormalA] \[CirclePlus] 
          \[FormalB] \[CircleTimes] OverBar[\[FormalB]]], "Proof" -> <||>|>, 
    {"Axiom", 2} -> <|"Statement" -> HoldForm[\[FormalA] == 
         \[FormalA] \[CircleTimes] (\[FormalB] \[CirclePlus] 
           OverBar[\[FormalB]])], "Proof" -> <||>|>, 
    {"Axiom", 3} -> <|"Statement" -> HoldForm[
        \[FormalA] \[CirclePlus] \[FormalB] == \[FormalB] \[CirclePlus] 
          \[FormalA]], "Proof" -> <||>|>, {"Axiom", 4} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CirclePlus] 
          \[FormalB] \[CircleTimes] \[FormalC] == 
         (\[FormalA] \[CirclePlus] \[FormalB]) \[CircleTimes] 
          (\[FormalA] \[CirclePlus] \[FormalC])], "Proof" -> <||>|>, 
    {"Axiom", 5} -> <|"Statement" -> HoldForm[
        \[FormalA] \[CircleTimes] \[FormalB] \[CirclePlus] 
          \[FormalA] \[CircleTimes] \[FormalC] == \[FormalA] \[CircleTimes] 
          (\[FormalB] \[CirclePlus] \[FormalC])], "Proof" -> <||>|>, 
    {"Axiom", 6} -> <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          \[FormalB] == \[FormalB] \[CircleTimes] \[FormalA]], 
      "Proof" -> <||>|>, {"Hypothesis", 1} -> 
     <|"Statement" -> HoldForm[(\[FormalO] \[CircleTimes] 
           \[FormalP]) \[CircleTimes] \[FormalQ] == \[FormalO] \[CircleTimes] 
          (\[FormalP] \[CircleTimes] \[FormalQ])], "Proof" -> <||>|>, 
    {"CriticalPairLemma", 1} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
           OverBar[\[FormalA]] \[CirclePlus] \[FormalB] == \[FormalB]], 
      "Proof" -> <|"Construct" -> {"Axiom", 3}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CirclePlus] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CirclePlus] (\[FormalA]_), "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CirclePlus] (\[FormalB]_), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CirclePlus] 
           (\[FormalB]_) \[CircleTimes] OverBar[\[FormalB]_] -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"CriticalPairLemma", 2} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CirclePlus] 
          \[FormalB] \[CircleTimes] OverBar[\[FormalA]] == 
         \[FormalA] \[CirclePlus] \[FormalB]], 
      "Proof" -> <|"Construct" -> {"Axiom", 4}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CirclePlus] (\[FormalB]_)) \[CircleTimes] 
           ((\[FormalA]_) \[CirclePlus] (\[FormalC]_)) -> 
          \[FormalA] \[CirclePlus] \[FormalB] \[CircleTimes] \[FormalC], 
        "Side" -> 1, "Subpattern" -> ((\[FormalA]_) \[CirclePlus] 
           (\[FormalB]_)) \[CircleTimes] ((\[FormalA]_) \[CirclePlus] 
           (\[FormalC]_)), "MatchingConstruct" -> {"Axiom", 2}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleTimes] ((\[FormalB]_) \[CirclePlus] 
            OverBar[\[FormalB]_]) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 3} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          (\[FormalB] \[CirclePlus] OverBar[\[FormalA]]) == 
         \[FormalA] \[CircleTimes] \[FormalB]], 
      "Proof" -> <|"Construct" -> {"Axiom", 5}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] (\[FormalB]_) \[CirclePlus] 
           (\[FormalA]_) \[CircleTimes] (\[FormalC]_) -> 
          \[FormalA] \[CircleTimes] (\[FormalB] \[CirclePlus] \[FormalC]), 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CircleTimes] 
           (\[FormalB]_) \[CirclePlus] (\[FormalA]_) \[CircleTimes] 
           (\[FormalC]_), "MatchingConstruct" -> {"Axiom", 1}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CirclePlus] (\[FormalB]_) \[CircleTimes] 
            OverBar[\[FormalB]_] -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 4} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CirclePlus] 
           OverBar[\[FormalA]]) \[CircleTimes] \[FormalB] == \[FormalB]], 
      "Proof" -> <|"Construct" -> {"Axiom", 6}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CircleTimes] (\[FormalA]_), "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CircleTimes] (\[FormalB]_), 
        "MatchingConstruct" -> {"Axiom", 2}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CircleTimes] 
           ((\[FormalB]_) \[CirclePlus] OverBar[\[FormalB]_]) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"CriticalPairLemma", 5} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          (\[FormalB] \[CirclePlus] \[FormalC]) == 
         \[FormalA] \[CircleTimes] \[FormalB] \[CirclePlus] 
          \[FormalC] \[CircleTimes] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"Axiom", 5}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] (\[FormalB]_) \[CirclePlus] 
           (\[FormalA]_) \[CircleTimes] (\[FormalC]_) -> 
          \[FormalA] \[CircleTimes] (\[FormalB] \[CirclePlus] \[FormalC]), 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CircleTimes] 
          (\[FormalC]_), "MatchingConstruct" -> {"Axiom", 6}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleTimes] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CircleTimes] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 6} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] \[FormalB] == 
         \[FormalA] \[CircleTimes] (OverBar[\[FormalA]] \[CirclePlus] 
           \[FormalB])], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 
          1}, "Orientation" -> 1, "Rule" -> 
         (\[FormalA]_) \[CircleTimes] OverBar[\[FormalA]_] \[CirclePlus] 
           (\[FormalB]_) -> \[FormalB], "Side" -> 1, "Subpattern" -> 
         (\[FormalA]_) \[CircleTimes] OverBar[\[FormalA]_] \[CirclePlus] 
          (\[FormalB]_), "MatchingConstruct" -> {"Axiom", 5}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleTimes] (\[FormalB]_) \[CirclePlus] 
           (\[FormalA]_) \[CircleTimes] (\[FormalC]_) -> 
          \[FormalA] \[CircleTimes] (\[FormalB] \[CirclePlus] \[FormalC]), 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"CriticalPairLemma", 7} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CirclePlus] \[FormalB] == 
         \[FormalA] \[CirclePlus] OverBar[\[FormalA]] \[CircleTimes] 
           \[FormalB]], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 4}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CirclePlus] 
            OverBar[\[FormalA]_]) \[CircleTimes] (\[FormalB]_) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> ((\[FormalA]_) \[CirclePlus] 
           OverBar[\[FormalA]_]) \[CircleTimes] (\[FormalB]_), 
        "MatchingConstruct" -> {"Axiom", 4}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CirclePlus] 
            (\[FormalB]_)) \[CircleTimes] ((\[FormalA]_) \[CirclePlus] 
            (\[FormalC]_)) -> \[FormalA] \[CirclePlus] 
           \[FormalB] \[CircleTimes] \[FormalC], "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 8} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CirclePlus] \[FormalA] == 
         \[FormalA]], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 2}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CirclePlus] 
           (\[FormalB]_) \[CircleTimes] OverBar[\[FormalA]_] -> 
          \[FormalA] \[CirclePlus] \[FormalB], "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CirclePlus] 
          (\[FormalB]_) \[CircleTimes] OverBar[\[FormalA]_], 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CirclePlus] 
           (\[FormalB]_) \[CircleTimes] OverBar[\[FormalB]_] -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"CriticalPairLemma", 9} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CirclePlus] 
          \[FormalA] \[CircleTimes] \[FormalB] == \[FormalA] \[CircleTimes] 
          (\[FormalA] \[CirclePlus] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"Axiom", 4}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CirclePlus] (\[FormalB]_)) \[CircleTimes] 
           ((\[FormalA]_) \[CirclePlus] (\[FormalC]_)) -> 
          \[FormalA] \[CirclePlus] \[FormalB] \[CircleTimes] \[FormalC], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CirclePlus] 
          (\[FormalB]_), "MatchingConstruct" -> {"CriticalPairLemma", 8}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CirclePlus] (\[FormalA]_) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 10} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CirclePlus] 
          \[FormalB] \[CircleTimes] \[FormalA] == 
         (\[FormalA] \[CirclePlus] \[FormalB]) \[CircleTimes] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"Axiom", 4}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CirclePlus] (\[FormalB]_)) \[CircleTimes] 
           ((\[FormalA]_) \[CirclePlus] (\[FormalC]_)) -> 
          \[FormalA] \[CirclePlus] \[FormalB] \[CircleTimes] \[FormalC], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CirclePlus] 
          (\[FormalC]_), "MatchingConstruct" -> {"CriticalPairLemma", 8}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CirclePlus] (\[FormalA]_) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CirclePlus] 
          \[FormalB] \[CircleTimes] \[FormalA] == \[FormalA] \[CircleTimes] 
          (\[FormalA] \[CirclePlus] \[FormalB])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 10}, "Position" -> {}, 
        "Construct" -> {"Axiom", 6}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] (\[FormalB]_) -> 
          \[FormalB] \[CircleTimes] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CirclePlus] \[FormalB] \[CircleTimes] 
             \[FormalA] == \[FormalA] \[CircleTimes] 
            (\[FormalA] \[CirclePlus] \[FormalB])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 11} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          OverBar[OverBar[\[FormalA]]] == \[FormalA]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 6}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CircleTimes] 
           (OverBar[\[FormalA]_] \[CirclePlus] (\[FormalB]_)) -> 
          \[FormalA] \[CircleTimes] \[FormalB], "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CircleTimes] 
          (OverBar[\[FormalA]_] \[CirclePlus] (\[FormalB]_)), 
        "MatchingConstruct" -> {"Axiom", 2}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CircleTimes] 
           ((\[FormalB]_) \[CirclePlus] OverBar[\[FormalB]_]) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"CriticalPairLemma", 12} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CirclePlus] 
          OverBar[OverBar[OverBar[\[FormalA]]]] == \[FormalA] \[CirclePlus] 
          OverBar[\[FormalA]]], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 7}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CirclePlus] 
           OverBar[\[FormalA]_] \[CircleTimes] (\[FormalB]_) -> 
          \[FormalA] \[CirclePlus] \[FormalB], "Side" -> 1, 
        "Subpattern" -> OverBar[\[FormalA]_] \[CircleTimes] (\[FormalB]_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 11}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleTimes] OverBar[OverBar[\[FormalA]_]] -> 
          \[FormalA], "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 13} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[\[FormalA]]] \[CircleTimes] 
          \[FormalA] == OverBar[OverBar[\[FormalA]]] \[CircleTimes] 
          (\[FormalA] \[CirclePlus] OverBar[\[FormalA]])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 3}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CircleTimes] 
           ((\[FormalB]_) \[CirclePlus] OverBar[\[FormalA]_]) -> 
          \[FormalA] \[CircleTimes] \[FormalB], "Side" -> 1, 
        "Subpattern" -> (\[FormalB]_) \[CirclePlus] OverBar[\[FormalA]_], 
        "MatchingConstruct" -> {"CriticalPairLemma", 12}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CirclePlus] OverBar[OverBar[OverBar[
              \[FormalA]_]]] -> \[FormalA] \[CirclePlus] OverBar[\[FormalA]], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[\[FormalA]]] \[CircleTimes] 
          \[FormalA] == OverBar[OverBar[\[FormalA]]]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 13}, "Position" -> {}, 
        "Construct" -> {"Axiom", 2}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] ((\[FormalB]_) \[CirclePlus] 
            OverBar[\[FormalB]_]) -> \[FormalA], "OutputExpression" -> 
         HoldForm[OverBar[OverBar[\[FormalA]]] \[CircleTimes] \[FormalA] == 
           OverBar[OverBar[\[FormalA]]]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, {"SubstitutionLemma", 3} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          OverBar[OverBar[\[FormalA]]] == OverBar[OverBar[\[FormalA]]]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 2}, "Position" -> {}, 
        "Construct" -> {"Axiom", 6}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] (\[FormalB]_) -> 
          \[FormalB] \[CircleTimes] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CircleTimes] OverBar[OverBar[\[FormalA]]] == 
           OverBar[OverBar[\[FormalA]]]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, {"SubstitutionLemma", 4} -> 
     <|"Statement" -> HoldForm[\[FormalA] == OverBar[OverBar[\[FormalA]]]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 3}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 11}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] OverBar[
            OverBar[\[FormalA]_]] -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == OverBar[OverBar[\[FormalA]]]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 14} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA]] \[CirclePlus] 
          \[FormalB] == OverBar[\[FormalA]] \[CirclePlus] 
          \[FormalA] \[CircleTimes] \[FormalB]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 7}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CirclePlus] 
           OverBar[\[FormalA]_] \[CircleTimes] (\[FormalB]_) -> 
          \[FormalA] \[CirclePlus] \[FormalB], "Side" -> 1, 
        "Subpattern" -> OverBar[\[FormalA]_], "MatchingConstruct" -> 
         {"SubstitutionLemma", 4}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> OverBar[OverBar[\[FormalA]_]] -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"CriticalPairLemma", 15} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          (\[FormalA] \[CircleTimes] \[FormalB]) == \[FormalA] \[CircleTimes] 
          (OverBar[\[FormalA]] \[CirclePlus] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 6}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CircleTimes] 
           (OverBar[\[FormalA]_] \[CirclePlus] (\[FormalB]_)) -> 
          \[FormalA] \[CircleTimes] \[FormalB], "Side" -> 1, 
        "Subpattern" -> OverBar[\[FormalA]_] \[CirclePlus] (\[FormalB]_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 14}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         OverBar[\[FormalA]_] \[CirclePlus] (\[FormalA]_) \[CircleTimes] 
            (\[FormalB]_) -> OverBar[\[FormalA]] \[CirclePlus] \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 5} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          (\[FormalA] \[CircleTimes] \[FormalB]) == \[FormalA] \[CircleTimes] 
          \[FormalB]], "Proof" -> <|"Input" -> {"CriticalPairLemma", 15}, 
        "Position" -> {}, "Construct" -> {"CriticalPairLemma", 6}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CircleTimes] 
           (OverBar[\[FormalA]_] \[CirclePlus] (\[FormalB]_)) -> 
          \[FormalA] \[CircleTimes] \[FormalB], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CircleTimes] (\[FormalA] \[CircleTimes] 
             \[FormalB]) == \[FormalA] \[CircleTimes] \[FormalB]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 16} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] \[FormalB] == 
         \[FormalA] \[CircleTimes] (\[FormalB] \[CircleTimes] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 5}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CircleTimes] 
           ((\[FormalA]_) \[CircleTimes] (\[FormalB]_)) -> 
          \[FormalA] \[CircleTimes] \[FormalB], "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CircleTimes] (\[FormalB]_), 
        "MatchingConstruct" -> {"Axiom", 6}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CircleTimes] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CircleTimes] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 17} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          (\[FormalB] \[CirclePlus] \[FormalA] \[CircleTimes] \[FormalC]) == 
         \[FormalA] \[CircleTimes] \[FormalB] \[CirclePlus] 
          \[FormalA] \[CircleTimes] \[FormalC]], 
      "Proof" -> <|"Construct" -> {"Axiom", 5}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] (\[FormalB]_) \[CirclePlus] 
           (\[FormalA]_) \[CircleTimes] (\[FormalC]_) -> 
          \[FormalA] \[CircleTimes] (\[FormalB] \[CirclePlus] \[FormalC]), 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CircleTimes] 
          (\[FormalC]_), "MatchingConstruct" -> {"SubstitutionLemma", 5}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleTimes] ((\[FormalA]_) \[CircleTimes] 
            (\[FormalB]_)) -> \[FormalA] \[CircleTimes] \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 6} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          (\[FormalB] \[CirclePlus] \[FormalA] \[CircleTimes] \[FormalC]) == 
         \[FormalA] \[CircleTimes] (\[FormalB] \[CirclePlus] \[FormalC])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 17}, "Position" -> {}, 
        "Construct" -> {"Axiom", 5}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] (\[FormalB]_) \[CirclePlus] 
           (\[FormalA]_) \[CircleTimes] (\[FormalC]_) -> 
          \[FormalA] \[CircleTimes] (\[FormalB] \[CirclePlus] \[FormalC]), 
        "OutputExpression" -> HoldForm[\[FormalA] \[CircleTimes] 
            (\[FormalB] \[CirclePlus] \[FormalA] \[CircleTimes] 
              \[FormalC]) == \[FormalA] \[CircleTimes] 
            (\[FormalB] \[CirclePlus] \[FormalC])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 18} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          (\[FormalB] \[CircleTimes] \[FormalA] \[CirclePlus] \[FormalC]) == 
         \[FormalA] \[CircleTimes] \[FormalB] \[CirclePlus] 
          \[FormalA] \[CircleTimes] \[FormalC]], 
      "Proof" -> <|"Construct" -> {"Axiom", 5}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] (\[FormalB]_) \[CirclePlus] 
           (\[FormalA]_) \[CircleTimes] (\[FormalC]_) -> 
          \[FormalA] \[CircleTimes] (\[FormalB] \[CirclePlus] \[FormalC]), 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CircleTimes] 
          (\[FormalB]_), "MatchingConstruct" -> {"CriticalPairLemma", 16}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleTimes] ((\[FormalB]_) \[CircleTimes] 
            (\[FormalA]_)) -> \[FormalA] \[CircleTimes] \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"SubstitutionLemma", 7} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          (\[FormalB] \[CircleTimes] \[FormalA] \[CirclePlus] \[FormalC]) == 
         \[FormalA] \[CircleTimes] (\[FormalB] \[CirclePlus] \[FormalC])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 18}, "Position" -> {}, 
        "Construct" -> {"Axiom", 5}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] (\[FormalB]_) \[CirclePlus] 
           (\[FormalA]_) \[CircleTimes] (\[FormalC]_) -> 
          \[FormalA] \[CircleTimes] (\[FormalB] \[CirclePlus] \[FormalC]), 
        "OutputExpression" -> HoldForm[\[FormalA] \[CircleTimes] 
            (\[FormalB] \[CircleTimes] \[FormalA] \[CirclePlus] 
             \[FormalC]) == \[FormalA] \[CircleTimes] 
            (\[FormalB] \[CirclePlus] \[FormalC])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 19} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          (\[FormalB] \[CirclePlus] OverBar[\[FormalB]]) == 
         \[FormalA] \[CircleTimes] (\[FormalB] \[CirclePlus] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 6}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CircleTimes] 
           ((\[FormalB]_) \[CirclePlus] (\[FormalA]_) \[CircleTimes] 
             (\[FormalC]_)) -> \[FormalA] \[CircleTimes] 
           (\[FormalB] \[CirclePlus] \[FormalC]), "Side" -> 1, 
        "Subpattern" -> (\[FormalB]_) \[CirclePlus] 
          (\[FormalA]_) \[CircleTimes] (\[FormalC]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 2}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CirclePlus] 
           (\[FormalB]_) \[CircleTimes] OverBar[\[FormalA]_] -> 
          \[FormalA] \[CirclePlus] \[FormalB], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 8} -> 
     <|"Statement" -> HoldForm[\[FormalA] == \[FormalA] \[CircleTimes] 
          (\[FormalB] \[CirclePlus] \[FormalA])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 19}, "Position" -> {}, 
        "Construct" -> {"Axiom", 2}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] ((\[FormalB]_) \[CirclePlus] 
            OverBar[\[FormalB]_]) -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == \[FormalA] \[CircleTimes] 
            (\[FormalB] \[CirclePlus] \[FormalA])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 20} -> 
     <|"Statement" -> HoldForm[\[FormalA] == \[FormalA] \[CircleTimes] 
          (\[FormalA] \[CirclePlus] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 8}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CircleTimes] 
           ((\[FormalB]_) \[CirclePlus] (\[FormalA]_)) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CirclePlus] 
          (\[FormalA]_), "MatchingConstruct" -> {"Axiom", 3}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CirclePlus] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CirclePlus] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 9} -> 
     <|"Statement" -> HoldForm[(\[FormalA]_) \[CirclePlus] 
          (\[FormalB]_) \[CircleTimes] (\[FormalA]_) -> \[FormalA]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 1}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 20}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] ((\[FormalA]_) \[CirclePlus] 
            (\[FormalB]_)) -> \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalA]_) \[CirclePlus] (\[FormalB]_) \[CircleTimes] 
             (\[FormalA]_) -> \[FormalA]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 10} -> 
     <|"Statement" -> HoldForm[(\[FormalA]_) \[CirclePlus] 
          (\[FormalA]_) \[CircleTimes] (\[FormalB]_) -> \[FormalA]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 9}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 20}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] ((\[FormalA]_) \[CirclePlus] 
            (\[FormalB]_)) -> \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalA]_) \[CirclePlus] (\[FormalA]_) \[CircleTimes] 
             (\[FormalB]_) -> \[FormalA]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 21} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          (\[FormalB] \[CirclePlus] \[FormalC] \[CircleTimes] \[FormalB]) == 
         \[FormalA] \[CircleTimes] (\[FormalB] \[CircleTimes] 
           (\[FormalA] \[CirclePlus] \[FormalC]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 7}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CircleTimes] 
           ((\[FormalB]_) \[CircleTimes] (\[FormalA]_) \[CirclePlus] 
            (\[FormalC]_)) -> \[FormalA] \[CircleTimes] 
           (\[FormalB] \[CirclePlus] \[FormalC]), "Side" -> 1, 
        "Subpattern" -> (\[FormalB]_) \[CircleTimes] 
           (\[FormalA]_) \[CirclePlus] (\[FormalC]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 5}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CircleTimes] 
            (\[FormalB]_) \[CirclePlus] (\[FormalC]_) \[CircleTimes] 
            (\[FormalA]_) -> \[FormalA] \[CircleTimes] 
           (\[FormalB] \[CirclePlus] \[FormalC]), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 11} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] \[FormalB] == 
         \[FormalA] \[CircleTimes] (\[FormalB] \[CircleTimes] 
           (\[FormalA] \[CirclePlus] \[FormalC]))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 21}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 9}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CirclePlus] (\[FormalB]_) \[CircleTimes] 
            (\[FormalA]_) -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CircleTimes] \[FormalB] == 
           \[FormalA] \[CircleTimes] (\[FormalB] \[CircleTimes] 
             (\[FormalA] \[CirclePlus] \[FormalC]))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 22} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] \[FormalB] == 
         \[FormalA] \[CircleTimes] ((\[FormalA] \[CirclePlus] 
            \[FormalC]) \[CircleTimes] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 11}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CircleTimes] 
           ((\[FormalB]_) \[CircleTimes] ((\[FormalA]_) \[CirclePlus] 
             (\[FormalC]_))) -> \[FormalA] \[CircleTimes] \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CircleTimes] 
          ((\[FormalA]_) \[CirclePlus] (\[FormalC]_)), "MatchingConstruct" -> 
         {"Axiom", 6}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleTimes] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CircleTimes] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 23} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] \[FormalB] == 
         \[FormalA] \[CircleTimes] (\[FormalB] \[CircleTimes] 
           (\[FormalC] \[CirclePlus] \[FormalA]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 11}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CircleTimes] 
           ((\[FormalB]_) \[CircleTimes] ((\[FormalA]_) \[CirclePlus] 
             (\[FormalC]_))) -> \[FormalA] \[CircleTimes] \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CirclePlus] 
          (\[FormalC]_), "MatchingConstruct" -> {"Axiom", 3}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CirclePlus] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CirclePlus] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 24} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] \[FormalB] == 
         \[FormalA] \[CircleTimes] ((\[FormalC] \[CirclePlus] 
            \[FormalA]) \[CircleTimes] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 22}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CircleTimes] 
           (((\[FormalA]_) \[CirclePlus] (\[FormalB]_)) \[CircleTimes] 
            (\[FormalC]_)) -> \[FormalA] \[CircleTimes] \[FormalC], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CirclePlus] 
          (\[FormalB]_), "MatchingConstruct" -> {"Axiom", 3}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CirclePlus] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CirclePlus] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2, 1}|>|>, {"CriticalPairLemma", 25} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CircleTimes] 
           \[FormalB]) \[CircleTimes] \[FormalC] == 
         (\[FormalA] \[CircleTimes] \[FormalB]) \[CircleTimes] 
          (\[FormalC] \[CircleTimes] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 23}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CircleTimes] 
           ((\[FormalB]_) \[CircleTimes] ((\[FormalC]_) \[CirclePlus] 
             (\[FormalA]_))) -> \[FormalA] \[CircleTimes] \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalC]_) \[CirclePlus] 
          (\[FormalA]_), "MatchingConstruct" -> {"SubstitutionLemma", 10}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleTimes] ((\[FormalA]_) \[CirclePlus] 
            (\[FormalB]_)) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 26} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CircleTimes] 
           \[FormalB]) \[CircleTimes] \[FormalC] == 
         (\[FormalA] \[CircleTimes] \[FormalB]) \[CircleTimes] 
          (\[FormalB] \[CircleTimes] \[FormalC])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 24}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CircleTimes] 
           (((\[FormalB]_) \[CirclePlus] (\[FormalA]_)) \[CircleTimes] 
            (\[FormalC]_)) -> \[FormalA] \[CircleTimes] \[FormalC], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CirclePlus] 
          (\[FormalA]_), "MatchingConstruct" -> {"SubstitutionLemma", 9}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleTimes] ((\[FormalA]_) \[CirclePlus] 
            (\[FormalB]_)) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2, 1}|>|>, {"CriticalPairLemma", 27} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CircleTimes] 
           \[FormalB]) \[CircleTimes] \[FormalC] == 
         (\[FormalC] \[CircleTimes] \[FormalA]) \[CircleTimes] 
          (\[FormalA] \[CircleTimes] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 25}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CircleTimes] 
            (\[FormalB]_)) \[CircleTimes] ((\[FormalC]_) \[CircleTimes] 
            (\[FormalA]_)) -> (\[FormalA] \[CircleTimes] 
            \[FormalB]) \[CircleTimes] \[FormalC], "Side" -> 1, 
        "Subpattern" -> ((\[FormalA]_) \[CircleTimes] 
           (\[FormalB]_)) \[CircleTimes] ((\[FormalC]_) \[CircleTimes] 
           (\[FormalA]_)), "MatchingConstruct" -> {"Axiom", 6}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleTimes] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CircleTimes] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"SubstitutionLemma", 12} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CircleTimes] 
           \[FormalB]) \[CircleTimes] \[FormalC] == 
         (\[FormalC] \[CircleTimes] \[FormalA]) \[CircleTimes] \[FormalB]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 27}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 26}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CircleTimes] (\[FormalB]_)) \[CircleTimes] 
           ((\[FormalB]_) \[CircleTimes] (\[FormalC]_)) -> 
          (\[FormalA] \[CircleTimes] \[FormalB]) \[CircleTimes] \[FormalC], 
        "OutputExpression" -> HoldForm[(\[FormalA] \[CircleTimes] 
             \[FormalB]) \[CircleTimes] \[FormalC] == 
           (\[FormalC] \[CircleTimes] \[FormalA]) \[CircleTimes] \[FormalB]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 13} -> 
     <|"Statement" -> HoldForm[(\[FormalP] \[CircleTimes] 
           \[FormalO]) \[CircleTimes] \[FormalQ] == \[FormalO] \[CircleTimes] 
          (\[FormalP] \[CircleTimes] \[FormalQ])], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Position" -> {1}, 
        "Construct" -> {"Axiom", 6}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] (\[FormalB]_) -> 
          \[FormalB] \[CircleTimes] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalP] \[CircleTimes] \[FormalO]) \[CircleTimes] 
            \[FormalQ] == \[FormalO] \[CircleTimes] 
            (\[FormalP] \[CircleTimes] \[FormalQ])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 14} -> 
     <|"Statement" -> HoldForm[(\[FormalP] \[CircleTimes] 
           \[FormalO]) \[CircleTimes] \[FormalQ] == \[FormalO] \[CircleTimes] 
          (\[FormalQ] \[CircleTimes] \[FormalP])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 13}, "Position" -> {2}, 
        "Construct" -> {"Axiom", 6}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] (\[FormalB]_) -> 
          \[FormalB] \[CircleTimes] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalP] \[CircleTimes] \[FormalO]) \[CircleTimes] 
            \[FormalQ] == \[FormalO] \[CircleTimes] 
            (\[FormalQ] \[CircleTimes] \[FormalP])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 15} -> 
     <|"Statement" -> HoldForm[(\[FormalP] \[CircleTimes] 
           \[FormalO]) \[CircleTimes] \[FormalQ] == 
         (\[FormalQ] \[CircleTimes] \[FormalP]) \[CircleTimes] \[FormalO]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 14}, "Position" -> {}, 
        "Construct" -> {"Axiom", 6}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] (\[FormalB]_) -> 
          \[FormalB] \[CircleTimes] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalP] \[CircleTimes] \[FormalO]) \[CircleTimes] 
            \[FormalQ] == (\[FormalQ] \[CircleTimes] 
             \[FormalP]) \[CircleTimes] \[FormalO]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"Conclusion", 1} -> <|"Statement" -> 
       HoldForm[(\[FormalQ] \[CircleTimes] \[FormalP]) \[CircleTimes] 
          \[FormalO] == (\[FormalQ] \[CircleTimes] \[FormalP]) \[CircleTimes] 
          \[FormalO]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 15}, 
        "Position" -> {}, "Construct" -> {"SubstitutionLemma", 12}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CircleTimes] 
            (\[FormalB]_)) \[CircleTimes] (\[FormalC]_) -> 
          (\[FormalC] \[CircleTimes] \[FormalA]) \[CircleTimes] \[FormalB], 
        "OutputExpression" -> HoldForm[(\[FormalQ] \[CircleTimes] 
             \[FormalP]) \[CircleTimes] \[FormalO] == 
           (\[FormalQ] \[CircleTimes] \[FormalP]) \[CircleTimes] \[FormalO]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>}|>]
