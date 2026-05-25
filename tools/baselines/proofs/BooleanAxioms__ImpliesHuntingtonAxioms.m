ProofObject["EquationalLogic", {ForAll[{\[FormalA], \[FormalB]}, 
   \[FormalA] \[CirclePlus] \[FormalB] == \[FormalB] \[CirclePlus] 
     \[FormalA]], ForAll[{\[FormalA], \[FormalB], \[FormalC]}, 
   \[FormalA] \[CirclePlus] (\[FormalB] \[CirclePlus] \[FormalC]) == 
    (\[FormalA] \[CirclePlus] \[FormalB]) \[CirclePlus] \[FormalC]], 
  ForAll[{\[FormalA], \[FormalB]}, 
   OverBar[OverBar[\[FormalA]] \[CirclePlus] \[FormalB]] \[CirclePlus] 
     OverBar[OverBar[\[FormalA]] \[CirclePlus] OverBar[\[FormalB]]] == 
    \[FormalA]]}, {ForAll[{\[FormalA], \[FormalB]}, 
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
    \[FormalK], \[FormalL], \[FormalM], \[FormalN], \[FormalO], \[FormalP], 
    \[FormalQ], \[FormalR], \[FormalS], \[FormalT], \[FormalU]}, 
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
     <|"Statement" -> HoldForm[(\[FormalQ] \[CirclePlus] 
           \[FormalR]) \[CirclePlus] \[FormalS] == \[FormalQ] \[CirclePlus] 
          (\[FormalR] \[CirclePlus] \[FormalS])], "Proof" -> <||>|>, 
    {"Hypothesis", 2} -> <|"Statement" -> 
       HoldForm[OverBar[OverBar[\[FormalT]] \[CirclePlus] 
            \[FormalU]] \[CirclePlus] OverBar[
           OverBar[\[FormalT]] \[CirclePlus] OverBar[\[FormalU]]] == 
         \[FormalT]], "Proof" -> <||>|>, {"Hypothesis", 3} -> 
     <|"Statement" -> HoldForm[\[FormalO] \[CirclePlus] \[FormalP] == 
         \[FormalP] \[CirclePlus] \[FormalO]], "Proof" -> <||>|>, 
    {"CriticalPairLemma", 1} -> 
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
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          (\[FormalB] \[CirclePlus] \[FormalC]) == 
         \[FormalB] \[CircleTimes] \[FormalA] \[CirclePlus] 
          \[FormalA] \[CircleTimes] \[FormalC]], 
      "Proof" -> <|"Construct" -> {"Axiom", 4}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] (\[FormalB]_) \[CirclePlus] 
           (\[FormalA]_) \[CircleTimes] (\[FormalC]_) -> 
          \[FormalA] \[CircleTimes] (\[FormalB] \[CirclePlus] \[FormalC]), 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CircleTimes] 
          (\[FormalB]_), "MatchingConstruct" -> {"Axiom", 3}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleTimes] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CircleTimes] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 4} -> 
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
        "Position" -> {}|>|>, {"Conclusion", 1} -> 
     <|"Statement" -> HoldForm[\[FormalP] \[CirclePlus] \[FormalO] == 
         \[FormalP] \[CirclePlus] \[FormalO]], 
      "Proof" -> <|"Input" -> {"Hypothesis", 3}, "Position" -> {}, 
        "Construct" -> {"Axiom", 6}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CirclePlus] (\[FormalB]_) -> 
          \[FormalB] \[CirclePlus] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalP] \[CirclePlus] \[FormalO] == 
           \[FormalP] \[CirclePlus] \[FormalO]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, {"CriticalPairLemma", 5} -> 
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
    {"CriticalPairLemma", 6} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CirclePlus] 
          \[FormalB] \[CircleTimes] \[FormalC] == 
         (\[FormalB] \[CirclePlus] \[FormalA]) \[CircleTimes] 
          (\[FormalA] \[CirclePlus] \[FormalC])], 
      "Proof" -> <|"Construct" -> {"Axiom", 5}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CirclePlus] (\[FormalB]_)) \[CircleTimes] 
           ((\[FormalA]_) \[CirclePlus] (\[FormalC]_)) -> 
          \[FormalA] \[CirclePlus] \[FormalB] \[CircleTimes] \[FormalC], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CirclePlus] 
          (\[FormalB]_), "MatchingConstruct" -> {"Axiom", 6}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CirclePlus] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CirclePlus] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 7} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CirclePlus] 
          \[FormalB] \[CircleTimes] \[FormalC] == 
         (\[FormalA] \[CirclePlus] \[FormalB]) \[CircleTimes] 
          (\[FormalC] \[CirclePlus] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"Axiom", 5}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CirclePlus] (\[FormalB]_)) \[CircleTimes] 
           ((\[FormalA]_) \[CirclePlus] (\[FormalC]_)) -> 
          \[FormalA] \[CirclePlus] \[FormalB] \[CircleTimes] \[FormalC], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CirclePlus] 
          (\[FormalC]_), "MatchingConstruct" -> {"Axiom", 6}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CirclePlus] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CirclePlus] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 8} -> 
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
        "Position" -> {}|>|>, {"CriticalPairLemma", 9} -> 
     <|"Statement" -> HoldForm[\[FormalA] == \[FormalA] \[CirclePlus] 
          OverBar[\[FormalB] \[CirclePlus] OverBar[\[FormalB]]]], 
      "Proof" -> <|"Construct" -> {"Axiom", 2}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CirclePlus] (\[FormalB]_) \[CircleTimes] 
            OverBar[\[FormalB]_] -> \[FormalA], "Side" -> 1, 
        "Subpattern" -> (\[FormalB]_) \[CircleTimes] OverBar[\[FormalB]_], 
        "MatchingConstruct" -> {"CriticalPairLemma", 1}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((\[FormalA]_) \[CirclePlus] OverBar[\[FormalA]_]) \[CircleTimes] 
           (\[FormalB]_) -> \[FormalB], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 10} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] \[FormalB] == 
         \[FormalA] \[CircleTimes] (OverBar[\[FormalA]] \[CirclePlus] 
           \[FormalB])], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 
          5}, "Orientation" -> 1, "Rule" -> 
         (\[FormalA]_) \[CircleTimes] OverBar[\[FormalA]_] \[CirclePlus] 
           (\[FormalB]_) -> \[FormalB], "Side" -> 1, "Subpattern" -> 
         (\[FormalA]_) \[CircleTimes] OverBar[\[FormalA]_] \[CirclePlus] 
          (\[FormalB]_), "MatchingConstruct" -> {"Axiom", 4}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleTimes] (\[FormalB]_) \[CirclePlus] 
           (\[FormalA]_) \[CircleTimes] (\[FormalC]_) -> 
          \[FormalA] \[CircleTimes] (\[FormalB] \[CirclePlus] \[FormalC]), 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"CriticalPairLemma", 11} -> 
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
    {"CriticalPairLemma", 12} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          (\[FormalA] \[CirclePlus] \[FormalB]) == \[FormalA] \[CirclePlus] 
          \[FormalA] \[CircleTimes] \[FormalB]], 
      "Proof" -> <|"Construct" -> {"Axiom", 4}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] (\[FormalB]_) \[CirclePlus] 
           (\[FormalA]_) \[CircleTimes] (\[FormalC]_) -> 
          \[FormalA] \[CircleTimes] (\[FormalB] \[CirclePlus] \[FormalC]), 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CircleTimes] 
          (\[FormalB]_), "MatchingConstruct" -> {"CriticalPairLemma", 11}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleTimes] (\[FormalA]_) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 13} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          (\[FormalB] \[CirclePlus] \[FormalA]) == 
         \[FormalA] \[CircleTimes] \[FormalB] \[CirclePlus] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"Axiom", 4}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] (\[FormalB]_) \[CirclePlus] 
           (\[FormalA]_) \[CircleTimes] (\[FormalC]_) -> 
          \[FormalA] \[CircleTimes] (\[FormalB] \[CirclePlus] \[FormalC]), 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CircleTimes] 
          (\[FormalC]_), "MatchingConstruct" -> {"CriticalPairLemma", 11}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleTimes] (\[FormalA]_) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          (\[FormalB] \[CirclePlus] \[FormalA]) == \[FormalA] \[CirclePlus] 
          \[FormalA] \[CircleTimes] \[FormalB]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 13}, "Position" -> {}, 
        "Construct" -> {"Axiom", 6}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CirclePlus] (\[FormalB]_) -> 
          \[FormalB] \[CirclePlus] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CircleTimes] (\[FormalB] \[CirclePlus] 
             \[FormalA]) == \[FormalA] \[CirclePlus] 
            \[FormalA] \[CircleTimes] \[FormalB]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 14} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CirclePlus] 
          OverBar[OverBar[\[FormalA]]] == \[FormalA]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 8}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CirclePlus] 
           OverBar[\[FormalA]_] \[CircleTimes] (\[FormalB]_) -> 
          \[FormalA] \[CirclePlus] \[FormalB], "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CirclePlus] 
          OverBar[\[FormalA]_] \[CircleTimes] (\[FormalB]_), 
        "MatchingConstruct" -> {"Axiom", 2}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CirclePlus] 
           (\[FormalB]_) \[CircleTimes] OverBar[\[FormalB]_] -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"CriticalPairLemma", 15} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          OverBar[OverBar[OverBar[\[FormalA]]]] == \[FormalA] \[CircleTimes] 
          OverBar[\[FormalA]]], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 10}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] 
           (OverBar[\[FormalA]_] \[CirclePlus] (\[FormalB]_)) -> 
          \[FormalA] \[CircleTimes] \[FormalB], "Side" -> 1, 
        "Subpattern" -> OverBar[\[FormalA]_] \[CirclePlus] (\[FormalB]_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 14}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CirclePlus] OverBar[OverBar[\[FormalA]_]] -> 
          \[FormalA], "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 16} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          OverBar[\[FormalA]] == OverBar[\[FormalB] \[CirclePlus] 
           OverBar[\[FormalB]]]], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 9}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CirclePlus] OverBar[
            (\[FormalB]_) \[CirclePlus] OverBar[\[FormalB]_]] -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CirclePlus] 
          OverBar[(\[FormalB]_) \[CirclePlus] OverBar[\[FormalB]_]], 
        "MatchingConstruct" -> {"CriticalPairLemma", 5}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleTimes] OverBar[\[FormalA]_] \[CirclePlus] 
           (\[FormalB]_) -> \[FormalB], "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 17} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[\[FormalA]]] \[CirclePlus] 
          \[FormalA] == OverBar[OverBar[\[FormalA]]] \[CirclePlus] 
          \[FormalA] \[CircleTimes] OverBar[\[FormalA]]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 4}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CirclePlus] 
           (\[FormalB]_) \[CircleTimes] OverBar[\[FormalA]_] -> 
          \[FormalA] \[CirclePlus] \[FormalB], "Side" -> 1, 
        "Subpattern" -> (\[FormalB]_) \[CircleTimes] OverBar[\[FormalA]_], 
        "MatchingConstruct" -> {"CriticalPairLemma", 15}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleTimes] OverBar[OverBar[
             OverBar[\[FormalA]_]]] -> \[FormalA] \[CircleTimes] 
           OverBar[\[FormalA]], "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[\[FormalA]]] \[CirclePlus] 
          \[FormalA] == OverBar[OverBar[\[FormalA]]]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 17}, "Position" -> {}, 
        "Construct" -> {"Axiom", 2}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CirclePlus] (\[FormalB]_) \[CircleTimes] 
            OverBar[\[FormalB]_] -> \[FormalA], "OutputExpression" -> 
         HoldForm[OverBar[OverBar[\[FormalA]]] \[CirclePlus] \[FormalA] == 
           OverBar[OverBar[\[FormalA]]]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, {"SubstitutionLemma", 3} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CirclePlus] 
          OverBar[OverBar[\[FormalA]]] == OverBar[OverBar[\[FormalA]]]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 2}, "Position" -> {}, 
        "Construct" -> {"Axiom", 6}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CirclePlus] (\[FormalB]_) -> 
          \[FormalB] \[CirclePlus] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CirclePlus] OverBar[OverBar[\[FormalA]]] == 
           OverBar[OverBar[\[FormalA]]]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, {"SubstitutionLemma", 4} -> 
     <|"Statement" -> HoldForm[\[FormalA] == OverBar[OverBar[\[FormalA]]]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 3}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 14}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CirclePlus] OverBar[
            OverBar[\[FormalA]_]] -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == OverBar[OverBar[\[FormalA]]]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 18} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA]] \[CircleTimes] 
          \[FormalB] == OverBar[\[FormalA]] \[CircleTimes] 
          (\[FormalA] \[CirclePlus] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 10}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CircleTimes] 
           (OverBar[\[FormalA]_] \[CirclePlus] (\[FormalB]_)) -> 
          \[FormalA] \[CircleTimes] \[FormalB], "Side" -> 1, 
        "Subpattern" -> OverBar[\[FormalA]_], "MatchingConstruct" -> 
         {"SubstitutionLemma", 4}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> OverBar[OverBar[\[FormalA]_]] -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"CriticalPairLemma", 19} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA]] \[CirclePlus] 
          \[FormalB] == OverBar[\[FormalA]] \[CirclePlus] 
          \[FormalA] \[CircleTimes] \[FormalB]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 8}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CirclePlus] 
           OverBar[\[FormalA]_] \[CircleTimes] (\[FormalB]_) -> 
          \[FormalA] \[CirclePlus] \[FormalB], "Side" -> 1, 
        "Subpattern" -> OverBar[\[FormalA]_], "MatchingConstruct" -> 
         {"SubstitutionLemma", 4}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> OverBar[OverBar[\[FormalA]_]] -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"CriticalPairLemma", 20} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA]] \[CirclePlus] 
          \[FormalB] == OverBar[\[FormalA]] \[CirclePlus] 
          \[FormalB] \[CircleTimes] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 4}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CirclePlus] 
           (\[FormalB]_) \[CircleTimes] OverBar[\[FormalA]_] -> 
          \[FormalA] \[CirclePlus] \[FormalB], "Side" -> 1, 
        "Subpattern" -> OverBar[\[FormalA]_], "MatchingConstruct" -> 
         {"SubstitutionLemma", 4}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> OverBar[OverBar[\[FormalA]_]] -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"CriticalPairLemma", 21} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CirclePlus] 
          (\[FormalA] \[CirclePlus] \[FormalB]) == \[FormalA] \[CirclePlus] 
          OverBar[\[FormalA]] \[CircleTimes] \[FormalB]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 8}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CirclePlus] 
           OverBar[\[FormalA]_] \[CircleTimes] (\[FormalB]_) -> 
          \[FormalA] \[CirclePlus] \[FormalB], "Side" -> 1, 
        "Subpattern" -> OverBar[\[FormalA]_] \[CircleTimes] (\[FormalB]_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 18}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         OverBar[\[FormalA]_] \[CircleTimes] ((\[FormalA]_) \[CirclePlus] 
            (\[FormalB]_)) -> OverBar[\[FormalA]] \[CircleTimes] \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 5} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CirclePlus] 
          (\[FormalA] \[CirclePlus] \[FormalB]) == \[FormalA] \[CirclePlus] 
          \[FormalB]], "Proof" -> <|"Input" -> {"CriticalPairLemma", 21}, 
        "Position" -> {}, "Construct" -> {"CriticalPairLemma", 8}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CirclePlus] 
           OverBar[\[FormalA]_] \[CircleTimes] (\[FormalB]_) -> 
          \[FormalA] \[CirclePlus] \[FormalB], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CirclePlus] (\[FormalA] \[CirclePlus] 
             \[FormalB]) == \[FormalA] \[CirclePlus] \[FormalB]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 22} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CirclePlus] \[FormalB] == 
         \[FormalA] \[CirclePlus] (\[FormalB] \[CirclePlus] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 5}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CirclePlus] 
           ((\[FormalA]_) \[CirclePlus] (\[FormalB]_)) -> 
          \[FormalA] \[CirclePlus] \[FormalB], "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CirclePlus] (\[FormalB]_), 
        "MatchingConstruct" -> {"Axiom", 6}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CirclePlus] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CirclePlus] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 23} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CirclePlus] 
          \[FormalB] \[CircleTimes] (\[FormalA] \[CirclePlus] \[FormalC]) == 
         (\[FormalA] \[CirclePlus] \[FormalB]) \[CircleTimes] 
          (\[FormalA] \[CirclePlus] \[FormalC])], 
      "Proof" -> <|"Construct" -> {"Axiom", 5}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CirclePlus] (\[FormalB]_)) \[CircleTimes] 
           ((\[FormalA]_) \[CirclePlus] (\[FormalC]_)) -> 
          \[FormalA] \[CirclePlus] \[FormalB] \[CircleTimes] \[FormalC], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CirclePlus] 
          (\[FormalC]_), "MatchingConstruct" -> {"SubstitutionLemma", 5}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CirclePlus] ((\[FormalA]_) \[CirclePlus] 
            (\[FormalB]_)) -> \[FormalA] \[CirclePlus] \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 6} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CirclePlus] 
          \[FormalB] \[CircleTimes] (\[FormalA] \[CirclePlus] \[FormalC]) == 
         \[FormalA] \[CirclePlus] \[FormalB] \[CircleTimes] \[FormalC]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 23}, "Position" -> {}, 
        "Construct" -> {"Axiom", 5}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CirclePlus] (\[FormalB]_)) \[CircleTimes] 
           ((\[FormalA]_) \[CirclePlus] (\[FormalC]_)) -> 
          \[FormalA] \[CirclePlus] \[FormalB] \[CircleTimes] \[FormalC], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CirclePlus] 
            \[FormalB] \[CircleTimes] (\[FormalA] \[CirclePlus] 
              \[FormalC]) == \[FormalA] \[CirclePlus] 
            \[FormalB] \[CircleTimes] \[FormalC]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 24} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CirclePlus] 
          (\[FormalB] \[CirclePlus] \[FormalA]) \[CircleTimes] \[FormalC] == 
         (\[FormalA] \[CirclePlus] \[FormalB]) \[CircleTimes] 
          (\[FormalA] \[CirclePlus] \[FormalC])], 
      "Proof" -> <|"Construct" -> {"Axiom", 5}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CirclePlus] (\[FormalB]_)) \[CircleTimes] 
           ((\[FormalA]_) \[CirclePlus] (\[FormalC]_)) -> 
          \[FormalA] \[CirclePlus] \[FormalB] \[CircleTimes] \[FormalC], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CirclePlus] 
          (\[FormalB]_), "MatchingConstruct" -> {"CriticalPairLemma", 22}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CirclePlus] ((\[FormalB]_) \[CirclePlus] 
            (\[FormalA]_)) -> \[FormalA] \[CirclePlus] \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"SubstitutionLemma", 7} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CirclePlus] 
          (\[FormalB] \[CirclePlus] \[FormalA]) \[CircleTimes] \[FormalC] == 
         \[FormalA] \[CirclePlus] \[FormalB] \[CircleTimes] \[FormalC]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 24}, "Position" -> {}, 
        "Construct" -> {"Axiom", 5}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CirclePlus] (\[FormalB]_)) \[CircleTimes] 
           ((\[FormalA]_) \[CirclePlus] (\[FormalC]_)) -> 
          \[FormalA] \[CirclePlus] \[FormalB] \[CircleTimes] \[FormalC], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CirclePlus] 
            (\[FormalB] \[CirclePlus] \[FormalA]) \[CircleTimes] 
             \[FormalC] == \[FormalA] \[CirclePlus] \[FormalB] \[CircleTimes] 
             \[FormalC]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 25} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[\[FormalA]]] \[CircleTimes] 
          (\[FormalA] \[CircleTimes] \[FormalB]) == 
         OverBar[OverBar[\[FormalA]]] \[CircleTimes] 
          (OverBar[\[FormalA]] \[CirclePlus] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 18}, 
        "Orientation" -> -1, "Rule" -> OverBar[\[FormalA]_] \[CircleTimes] 
           ((\[FormalA]_) \[CirclePlus] (\[FormalB]_)) -> 
          OverBar[\[FormalA]] \[CircleTimes] \[FormalB], "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CirclePlus] (\[FormalB]_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 19}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         OverBar[\[FormalA]_] \[CirclePlus] (\[FormalA]_) \[CircleTimes] 
            (\[FormalB]_) -> OverBar[\[FormalA]] \[CirclePlus] \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 8} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          (\[FormalA] \[CircleTimes] \[FormalB]) == 
         OverBar[OverBar[\[FormalA]]] \[CircleTimes] 
          (OverBar[\[FormalA]] \[CirclePlus] \[FormalB])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 25}, "Position" -> {1}, 
        "Construct" -> {"SubstitutionLemma", 4}, "Orientation" -> -1, 
        "Rule" -> OverBar[OverBar[\[FormalA]_]] -> \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CircleTimes] 
            (\[FormalA] \[CircleTimes] \[FormalB]) == 
           OverBar[OverBar[\[FormalA]]] \[CircleTimes] 
            (OverBar[\[FormalA]] \[CirclePlus] \[FormalB])], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 9} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          (\[FormalA] \[CircleTimes] \[FormalB]) == 
         OverBar[OverBar[\[FormalA]]] \[CircleTimes] \[FormalB]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 8}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 18}, "Orientation" -> -1, 
        "Rule" -> OverBar[\[FormalA]_] \[CircleTimes] 
           ((\[FormalA]_) \[CirclePlus] (\[FormalB]_)) -> 
          OverBar[\[FormalA]] \[CircleTimes] \[FormalB], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CircleTimes] 
            (\[FormalA] \[CircleTimes] \[FormalB]) == 
           OverBar[OverBar[\[FormalA]]] \[CircleTimes] \[FormalB]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 10} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          (\[FormalA] \[CircleTimes] \[FormalB]) == \[FormalA] \[CircleTimes] 
          \[FormalB]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 9}, 
        "Position" -> {1}, "Construct" -> {"SubstitutionLemma", 4}, 
        "Orientation" -> -1, "Rule" -> OverBar[OverBar[\[FormalA]_]] -> 
          \[FormalA], "OutputExpression" -> HoldForm[
          \[FormalA] \[CircleTimes] (\[FormalA] \[CircleTimes] \[FormalB]) == 
           \[FormalA] \[CircleTimes] \[FormalB]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 26} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          (\[FormalA] \[CircleTimes] \[FormalB] \[CirclePlus] \[FormalC]) == 
         \[FormalA] \[CircleTimes] \[FormalB] \[CirclePlus] 
          \[FormalA] \[CircleTimes] \[FormalC]], 
      "Proof" -> <|"Construct" -> {"Axiom", 4}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] (\[FormalB]_) \[CirclePlus] 
           (\[FormalA]_) \[CircleTimes] (\[FormalC]_) -> 
          \[FormalA] \[CircleTimes] (\[FormalB] \[CirclePlus] \[FormalC]), 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CircleTimes] 
          (\[FormalB]_), "MatchingConstruct" -> {"SubstitutionLemma", 10}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleTimes] ((\[FormalA]_) \[CircleTimes] 
            (\[FormalB]_)) -> \[FormalA] \[CircleTimes] \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"SubstitutionLemma", 11} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          (\[FormalA] \[CircleTimes] \[FormalB] \[CirclePlus] \[FormalC]) == 
         \[FormalA] \[CircleTimes] (\[FormalB] \[CirclePlus] \[FormalC])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 26}, "Position" -> {}, 
        "Construct" -> {"Axiom", 4}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] (\[FormalB]_) \[CirclePlus] 
           (\[FormalA]_) \[CircleTimes] (\[FormalC]_) -> 
          \[FormalA] \[CircleTimes] (\[FormalB] \[CirclePlus] \[FormalC]), 
        "OutputExpression" -> HoldForm[\[FormalA] \[CircleTimes] 
            (\[FormalA] \[CircleTimes] \[FormalB] \[CirclePlus] 
             \[FormalC]) == \[FormalA] \[CircleTimes] 
            (\[FormalB] \[CirclePlus] \[FormalC])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 27} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          (\[FormalB] \[CirclePlus] \[FormalA] \[CircleTimes] \[FormalC]) == 
         \[FormalA] \[CircleTimes] \[FormalB] \[CirclePlus] 
          \[FormalA] \[CircleTimes] \[FormalC]], 
      "Proof" -> <|"Construct" -> {"Axiom", 4}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] (\[FormalB]_) \[CirclePlus] 
           (\[FormalA]_) \[CircleTimes] (\[FormalC]_) -> 
          \[FormalA] \[CircleTimes] (\[FormalB] \[CirclePlus] \[FormalC]), 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CircleTimes] 
          (\[FormalC]_), "MatchingConstruct" -> {"SubstitutionLemma", 10}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleTimes] ((\[FormalA]_) \[CircleTimes] 
            (\[FormalB]_)) -> \[FormalA] \[CircleTimes] \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 12} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          (\[FormalB] \[CirclePlus] \[FormalA] \[CircleTimes] \[FormalC]) == 
         \[FormalA] \[CircleTimes] (\[FormalB] \[CirclePlus] \[FormalC])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 27}, "Position" -> {}, 
        "Construct" -> {"Axiom", 4}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] (\[FormalB]_) \[CirclePlus] 
           (\[FormalA]_) \[CircleTimes] (\[FormalC]_) -> 
          \[FormalA] \[CircleTimes] (\[FormalB] \[CirclePlus] \[FormalC]), 
        "OutputExpression" -> HoldForm[\[FormalA] \[CircleTimes] 
            (\[FormalB] \[CirclePlus] \[FormalA] \[CircleTimes] 
              \[FormalC]) == \[FormalA] \[CircleTimes] 
            (\[FormalB] \[CirclePlus] \[FormalC])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 28} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CirclePlus] 
          \[FormalB] \[CircleTimes] OverBar[\[FormalB]] == 
         \[FormalA] \[CirclePlus] \[FormalB] \[CircleTimes] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 6}, 
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
        "Position" -> {2}|>|>, {"SubstitutionLemma", 13} -> 
     <|"Statement" -> HoldForm[\[FormalA] == \[FormalA] \[CirclePlus] 
          \[FormalB] \[CircleTimes] \[FormalA]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 28}, "Position" -> {}, 
        "Construct" -> {"Axiom", 2}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CirclePlus] (\[FormalB]_) \[CircleTimes] 
            OverBar[\[FormalB]_] -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == \[FormalA] \[CirclePlus] 
            \[FormalB] \[CircleTimes] \[FormalA]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 29} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          OverBar[\[FormalA]] == \[FormalB] \[CircleTimes] 
          (\[FormalA] \[CircleTimes] OverBar[\[FormalA]])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 13}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CirclePlus] 
           (\[FormalB]_) \[CircleTimes] (\[FormalA]_) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CirclePlus] 
          (\[FormalB]_) \[CircleTimes] (\[FormalA]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 5}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CircleTimes] 
            OverBar[\[FormalA]_] \[CirclePlus] (\[FormalB]_) -> \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"CriticalPairLemma", 30} -> 
     <|"Statement" -> HoldForm[\[FormalA] == \[FormalA] \[CirclePlus] 
          \[FormalA] \[CircleTimes] \[FormalB]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 13}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CirclePlus] 
           (\[FormalB]_) \[CircleTimes] (\[FormalA]_) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CircleTimes] 
          (\[FormalA]_), "MatchingConstruct" -> {"Axiom", 3}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleTimes] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CircleTimes] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 31} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA]] \[CircleTimes] 
          (\[FormalB] \[CircleTimes] \[FormalA]) == 
         OverBar[\[FormalA]] \[CircleTimes] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 18}, 
        "Orientation" -> -1, "Rule" -> OverBar[\[FormalA]_] \[CircleTimes] 
           ((\[FormalA]_) \[CirclePlus] (\[FormalB]_)) -> 
          OverBar[\[FormalA]] \[CircleTimes] \[FormalB], "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CirclePlus] (\[FormalB]_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 13}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CirclePlus] (\[FormalB]_) \[CircleTimes] 
            (\[FormalA]_) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 14} -> 
     <|"Statement" -> HoldForm[(\[FormalA]_) \[CircleTimes] 
          ((\[FormalB]_) \[CirclePlus] (\[FormalA]_)) -> \[FormalA]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 1}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 30}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CirclePlus] (\[FormalA]_) \[CircleTimes] 
            (\[FormalB]_) -> \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalA]_) \[CircleTimes] ((\[FormalB]_) \[CirclePlus] 
             (\[FormalA]_)) -> \[FormalA]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 15} -> 
     <|"Statement" -> HoldForm[(\[FormalA]_) \[CircleTimes] 
          ((\[FormalA]_) \[CirclePlus] (\[FormalB]_)) -> \[FormalA]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 12}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 30}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CirclePlus] (\[FormalA]_) \[CircleTimes] 
            (\[FormalB]_) -> \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalA]_) \[CircleTimes] ((\[FormalA]_) \[CirclePlus] 
             (\[FormalB]_)) -> \[FormalA]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 32} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          (OverBar[\[FormalA]] \[CircleTimes] \[FormalB]) == 
         \[FormalA] \[CircleTimes] OverBar[\[FormalA]]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 10}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CircleTimes] 
           (OverBar[\[FormalA]_] \[CirclePlus] (\[FormalB]_)) -> 
          \[FormalA] \[CircleTimes] \[FormalB], "Side" -> 1, 
        "Subpattern" -> OverBar[\[FormalA]_] \[CirclePlus] (\[FormalB]_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 30}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CirclePlus] (\[FormalA]_) \[CircleTimes] 
            (\[FormalB]_) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 16} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA]] \[CircleTimes] 
          (\[FormalB] \[CircleTimes] \[FormalA]) == \[FormalA] \[CircleTimes] 
          OverBar[\[FormalA]]], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 31}, "Position" -> {}, 
        "Construct" -> {"Axiom", 3}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] (\[FormalB]_) -> 
          \[FormalB] \[CircleTimes] \[FormalA], "OutputExpression" -> 
         HoldForm[OverBar[\[FormalA]] \[CircleTimes] 
            (\[FormalB] \[CircleTimes] \[FormalA]) == 
           \[FormalA] \[CircleTimes] OverBar[\[FormalA]]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 33} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CircleTimes] 
           \[FormalB]) \[CircleTimes] OverBar[\[FormalA] \[CircleTimes] 
            \[FormalB]] == OverBar[\[FormalA] \[CircleTimes] 
            \[FormalB]] \[CircleTimes] (\[FormalB] \[CircleTimes] 
           OverBar[\[FormalB]])], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 16}, "Orientation" -> 1, 
        "Rule" -> OverBar[\[FormalA]_] \[CircleTimes] 
           ((\[FormalB]_) \[CircleTimes] (\[FormalA]_)) -> 
          \[FormalA] \[CircleTimes] OverBar[\[FormalA]], "Side" -> 1, 
        "Subpattern" -> (\[FormalB]_) \[CircleTimes] (\[FormalA]_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 16}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         OverBar[\[FormalA]_] \[CircleTimes] ((\[FormalB]_) \[CircleTimes] 
            (\[FormalA]_)) -> \[FormalA] \[CircleTimes] OverBar[\[FormalA]], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 17} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CircleTimes] 
           \[FormalB]) \[CircleTimes] OverBar[\[FormalA] \[CircleTimes] 
            \[FormalB]] == \[FormalB] \[CircleTimes] OverBar[\[FormalB]]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 33}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 29}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] ((\[FormalB]_) \[CircleTimes] 
            OverBar[\[FormalB]_]) -> \[FormalB] \[CircleTimes] 
           OverBar[\[FormalB]], "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CircleTimes] \[FormalB]) \[CircleTimes] 
            OverBar[\[FormalA] \[CircleTimes] \[FormalB]] == 
           \[FormalB] \[CircleTimes] OverBar[\[FormalB]]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 34} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA] \[CircleTimes] 
            \[FormalB]] \[CirclePlus] OverBar[\[FormalB]] == 
         OverBar[\[FormalA] \[CircleTimes] \[FormalB]] \[CirclePlus] 
          \[FormalB] \[CircleTimes] OverBar[\[FormalB]]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 20}, 
        "Orientation" -> -1, "Rule" -> OverBar[\[FormalA]_] \[CirclePlus] 
           (\[FormalB]_) \[CircleTimes] (\[FormalA]_) -> 
          OverBar[\[FormalA]] \[CirclePlus] \[FormalB], "Side" -> 1, 
        "Subpattern" -> (\[FormalB]_) \[CircleTimes] (\[FormalA]_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 16}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         OverBar[\[FormalA]_] \[CircleTimes] ((\[FormalB]_) \[CircleTimes] 
            (\[FormalA]_)) -> \[FormalA] \[CircleTimes] OverBar[\[FormalA]], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 18} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA] \[CircleTimes] 
            \[FormalB]] \[CirclePlus] OverBar[\[FormalB]] == 
         OverBar[\[FormalA] \[CircleTimes] \[FormalB]]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 34}, "Position" -> {}, 
        "Construct" -> {"Axiom", 2}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CirclePlus] (\[FormalB]_) \[CircleTimes] 
            OverBar[\[FormalB]_] -> \[FormalA], "OutputExpression" -> 
         HoldForm[OverBar[\[FormalA] \[CircleTimes] \[FormalB]] \[CirclePlus] 
            OverBar[\[FormalB]] == OverBar[\[FormalA] \[CircleTimes] 
             \[FormalB]]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 35} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[\[FormalA]] \[CircleTimes] 
            \[FormalB]] \[CirclePlus] \[FormalA] == 
         OverBar[OverBar[\[FormalA]] \[CircleTimes] \[FormalB]] \[CirclePlus] 
          \[FormalA] \[CircleTimes] OverBar[\[FormalA]]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 20}, 
        "Orientation" -> -1, "Rule" -> OverBar[\[FormalA]_] \[CirclePlus] 
           (\[FormalB]_) \[CircleTimes] (\[FormalA]_) -> 
          OverBar[\[FormalA]] \[CirclePlus] \[FormalB], "Side" -> 1, 
        "Subpattern" -> (\[FormalB]_) \[CircleTimes] (\[FormalA]_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 32}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleTimes] (OverBar[\[FormalA]_] \[CircleTimes] 
            (\[FormalB]_)) -> \[FormalA] \[CircleTimes] OverBar[\[FormalA]], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 19} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[\[FormalA]] \[CircleTimes] 
            \[FormalB]] \[CirclePlus] \[FormalA] == 
         OverBar[OverBar[\[FormalA]] \[CircleTimes] \[FormalB]]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 35}, "Position" -> {}, 
        "Construct" -> {"Axiom", 2}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CirclePlus] (\[FormalB]_) \[CircleTimes] 
            OverBar[\[FormalB]_] -> \[FormalA], "OutputExpression" -> 
         HoldForm[OverBar[OverBar[\[FormalA]] \[CircleTimes] 
              \[FormalB]] \[CirclePlus] \[FormalA] == 
           OverBar[OverBar[\[FormalA]] \[CircleTimes] \[FormalB]]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 20} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA]] \[CirclePlus] 
          OverBar[\[FormalB] \[CircleTimes] \[FormalA]] == 
         OverBar[\[FormalB] \[CircleTimes] \[FormalA]]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 18}, "Position" -> {}, 
        "Construct" -> {"Axiom", 6}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CirclePlus] (\[FormalB]_) -> 
          \[FormalB] \[CirclePlus] \[FormalA], "OutputExpression" -> 
         HoldForm[OverBar[\[FormalA]] \[CirclePlus] 
            OverBar[\[FormalB] \[CircleTimes] \[FormalA]] == 
           OverBar[\[FormalB] \[CircleTimes] \[FormalA]]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 36} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CirclePlus] 
          \[FormalB] \[CircleTimes] (\[FormalC] \[CirclePlus] \[FormalB]) == 
         \[FormalA] \[CirclePlus] (\[FormalB] \[CirclePlus] 
           \[FormalA] \[CircleTimes] \[FormalC])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 7}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CirclePlus] 
           ((\[FormalB]_) \[CirclePlus] (\[FormalA]_)) \[CircleTimes] 
            (\[FormalC]_) -> \[FormalA] \[CirclePlus] 
           \[FormalB] \[CircleTimes] \[FormalC], "Side" -> 1, 
        "Subpattern" -> ((\[FormalB]_) \[CirclePlus] 
           (\[FormalA]_)) \[CircleTimes] (\[FormalC]_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 7}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CirclePlus] (\[FormalB]_)) \[CircleTimes] 
           ((\[FormalC]_) \[CirclePlus] (\[FormalA]_)) -> 
          \[FormalA] \[CirclePlus] \[FormalB] \[CircleTimes] \[FormalC], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 21} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CirclePlus] \[FormalB] == 
         \[FormalA] \[CirclePlus] (\[FormalB] \[CirclePlus] 
           \[FormalA] \[CircleTimes] \[FormalC])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 36}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 14}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] ((\[FormalB]_) \[CirclePlus] 
            (\[FormalA]_)) -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CirclePlus] \[FormalB] == 
           \[FormalA] \[CirclePlus] (\[FormalB] \[CirclePlus] 
             \[FormalA] \[CircleTimes] \[FormalC])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 37} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CirclePlus] \[FormalB] == 
         \[FormalA] \[CirclePlus] (\[FormalA] \[CircleTimes] 
            \[FormalC] \[CirclePlus] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 21}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CirclePlus] 
           ((\[FormalB]_) \[CirclePlus] (\[FormalA]_) \[CircleTimes] 
             (\[FormalC]_)) -> \[FormalA] \[CirclePlus] \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CirclePlus] 
          (\[FormalA]_) \[CircleTimes] (\[FormalC]_), "MatchingConstruct" -> 
         {"Axiom", 6}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CirclePlus] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CirclePlus] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 38} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CirclePlus] \[FormalB] == 
         \[FormalA] \[CirclePlus] (\[FormalB] \[CirclePlus] 
           \[FormalC] \[CircleTimes] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 21}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CirclePlus] 
           ((\[FormalB]_) \[CirclePlus] (\[FormalA]_) \[CircleTimes] 
             (\[FormalC]_)) -> \[FormalA] \[CirclePlus] \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CircleTimes] 
          (\[FormalC]_), "MatchingConstruct" -> {"Axiom", 3}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleTimes] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CircleTimes] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 39} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CirclePlus] \[FormalB] == 
         \[FormalA] \[CirclePlus] (\[FormalC] \[CircleTimes] 
            \[FormalA] \[CirclePlus] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 37}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CirclePlus] 
           ((\[FormalA]_) \[CircleTimes] (\[FormalB]_) \[CirclePlus] 
            (\[FormalC]_)) -> \[FormalA] \[CirclePlus] \[FormalC], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CircleTimes] 
          (\[FormalB]_), "MatchingConstruct" -> {"Axiom", 3}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleTimes] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CircleTimes] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2, 1}|>|>, {"CriticalPairLemma", 40} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CirclePlus] 
           \[FormalB]) \[CirclePlus] \[FormalC] == 
         (\[FormalA] \[CirclePlus] \[FormalB]) \[CirclePlus] 
          (\[FormalC] \[CirclePlus] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 38}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CirclePlus] 
           ((\[FormalB]_) \[CirclePlus] (\[FormalC]_) \[CircleTimes] 
             (\[FormalA]_)) -> \[FormalA] \[CirclePlus] \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalC]_) \[CircleTimes] 
          (\[FormalA]_), "MatchingConstruct" -> {"SubstitutionLemma", 15}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CirclePlus] (\[FormalA]_) \[CircleTimes] 
            (\[FormalB]_) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 41} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CirclePlus] 
           \[FormalB]) \[CirclePlus] \[FormalC] == 
         (\[FormalA] \[CirclePlus] \[FormalB]) \[CirclePlus] 
          (\[FormalB] \[CirclePlus] \[FormalC])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 39}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CirclePlus] 
           ((\[FormalB]_) \[CircleTimes] (\[FormalA]_) \[CirclePlus] 
            (\[FormalC]_)) -> \[FormalA] \[CirclePlus] \[FormalC], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CircleTimes] 
          (\[FormalA]_), "MatchingConstruct" -> {"SubstitutionLemma", 14}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CirclePlus] (\[FormalA]_) \[CircleTimes] 
            (\[FormalB]_) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2, 1}|>|>, {"CriticalPairLemma", 42} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          (\[FormalB] \[CirclePlus] \[FormalB] \[CircleTimes] \[FormalC]) == 
         \[FormalA] \[CircleTimes] (\[FormalB] \[CircleTimes] 
           (\[FormalA] \[CirclePlus] \[FormalC]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 11}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CircleTimes] 
           ((\[FormalA]_) \[CircleTimes] (\[FormalB]_) \[CirclePlus] 
            (\[FormalC]_)) -> \[FormalA] \[CircleTimes] 
           (\[FormalB] \[CirclePlus] \[FormalC]), "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CircleTimes] 
           (\[FormalB]_) \[CirclePlus] (\[FormalC]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 3}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CircleTimes] 
            (\[FormalB]_) \[CirclePlus] (\[FormalB]_) \[CircleTimes] 
            (\[FormalC]_) -> \[FormalB] \[CircleTimes] 
           (\[FormalA] \[CirclePlus] \[FormalC]), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 22} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] \[FormalB] == 
         \[FormalA] \[CircleTimes] (\[FormalB] \[CircleTimes] 
           (\[FormalA] \[CirclePlus] \[FormalC]))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 42}, "Position" -> {2}, 
        "Construct" -> {"CriticalPairLemma", 30}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CirclePlus] (\[FormalA]_) \[CircleTimes] 
            (\[FormalB]_) -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CircleTimes] \[FormalB] == 
           \[FormalA] \[CircleTimes] (\[FormalB] \[CircleTimes] 
             (\[FormalA] \[CirclePlus] \[FormalC]))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 43} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] \[FormalB] == 
         \[FormalA] \[CircleTimes] ((\[FormalA] \[CirclePlus] 
            \[FormalC]) \[CircleTimes] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 22}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CircleTimes] 
           ((\[FormalB]_) \[CircleTimes] ((\[FormalA]_) \[CirclePlus] 
             (\[FormalC]_))) -> \[FormalA] \[CircleTimes] \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CircleTimes] 
          ((\[FormalA]_) \[CirclePlus] (\[FormalC]_)), "MatchingConstruct" -> 
         {"Axiom", 3}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleTimes] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CircleTimes] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 44} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] \[FormalB] == 
         \[FormalA] \[CircleTimes] (\[FormalB] \[CircleTimes] 
           (\[FormalC] \[CirclePlus] \[FormalA]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 22}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CircleTimes] 
           ((\[FormalB]_) \[CircleTimes] ((\[FormalA]_) \[CirclePlus] 
             (\[FormalC]_))) -> \[FormalA] \[CircleTimes] \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CirclePlus] 
          (\[FormalC]_), "MatchingConstruct" -> {"Axiom", 6}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CirclePlus] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CirclePlus] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 45} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] \[FormalB] == 
         \[FormalA] \[CircleTimes] ((\[FormalC] \[CirclePlus] 
            \[FormalA]) \[CircleTimes] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 43}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CircleTimes] 
           (((\[FormalA]_) \[CirclePlus] (\[FormalB]_)) \[CircleTimes] 
            (\[FormalC]_)) -> \[FormalA] \[CircleTimes] \[FormalC], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CirclePlus] 
          (\[FormalB]_), "MatchingConstruct" -> {"Axiom", 6}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CirclePlus] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CirclePlus] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2, 1}|>|>, {"CriticalPairLemma", 46} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CircleTimes] 
           \[FormalB]) \[CircleTimes] \[FormalC] == 
         (\[FormalA] \[CircleTimes] \[FormalB]) \[CircleTimes] 
          (\[FormalC] \[CircleTimes] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 44}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CircleTimes] 
           ((\[FormalB]_) \[CircleTimes] ((\[FormalC]_) \[CirclePlus] 
             (\[FormalA]_))) -> \[FormalA] \[CircleTimes] \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalC]_) \[CirclePlus] 
          (\[FormalA]_), "MatchingConstruct" -> {"CriticalPairLemma", 30}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CirclePlus] (\[FormalA]_) \[CircleTimes] 
            (\[FormalB]_) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 47} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CircleTimes] 
           \[FormalB]) \[CircleTimes] \[FormalC] == 
         (\[FormalA] \[CircleTimes] \[FormalB]) \[CircleTimes] 
          (\[FormalB] \[CircleTimes] \[FormalC])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 45}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CircleTimes] 
           (((\[FormalB]_) \[CirclePlus] (\[FormalA]_)) \[CircleTimes] 
            (\[FormalC]_)) -> \[FormalA] \[CircleTimes] \[FormalC], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CirclePlus] 
          (\[FormalA]_), "MatchingConstruct" -> {"SubstitutionLemma", 13}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CirclePlus] (\[FormalB]_) \[CircleTimes] 
            (\[FormalA]_) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2, 1}|>|>, {"SubstitutionLemma", 23} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CirclePlus] 
          OverBar[OverBar[\[FormalA]] \[CircleTimes] \[FormalB]] == 
         OverBar[OverBar[\[FormalA]] \[CircleTimes] \[FormalB]]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 19}, "Position" -> {}, 
        "Construct" -> {"Axiom", 6}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CirclePlus] (\[FormalB]_) -> 
          \[FormalB] \[CirclePlus] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CirclePlus] OverBar[
             OverBar[\[FormalA]] \[CircleTimes] \[FormalB]] == 
           OverBar[OverBar[\[FormalA]] \[CircleTimes] \[FormalB]]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 48} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CirclePlus] 
           \[FormalB]) \[CirclePlus] \[FormalC] == 
         (\[FormalC] \[CirclePlus] \[FormalA]) \[CirclePlus] 
          (\[FormalA] \[CirclePlus] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 40}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CirclePlus] 
            (\[FormalB]_)) \[CirclePlus] ((\[FormalC]_) \[CirclePlus] 
            (\[FormalA]_)) -> (\[FormalA] \[CirclePlus] 
            \[FormalB]) \[CirclePlus] \[FormalC], "Side" -> 1, 
        "Subpattern" -> ((\[FormalA]_) \[CirclePlus] 
           (\[FormalB]_)) \[CirclePlus] ((\[FormalC]_) \[CirclePlus] 
           (\[FormalA]_)), "MatchingConstruct" -> {"Axiom", 6}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CirclePlus] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CirclePlus] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 49} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CircleTimes] 
           \[FormalB]) \[CircleTimes] \[FormalC] == 
         (\[FormalC] \[CircleTimes] \[FormalA]) \[CircleTimes] 
          (\[FormalA] \[CircleTimes] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 46}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CircleTimes] 
            (\[FormalB]_)) \[CircleTimes] ((\[FormalC]_) \[CircleTimes] 
            (\[FormalA]_)) -> (\[FormalA] \[CircleTimes] 
            \[FormalB]) \[CircleTimes] \[FormalC], "Side" -> 1, 
        "Subpattern" -> ((\[FormalA]_) \[CircleTimes] 
           (\[FormalB]_)) \[CircleTimes] ((\[FormalC]_) \[CircleTimes] 
           (\[FormalA]_)), "MatchingConstruct" -> {"Axiom", 3}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleTimes] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CircleTimes] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"SubstitutionLemma", 24} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CirclePlus] 
           \[FormalB]) \[CirclePlus] \[FormalC] == 
         (\[FormalC] \[CirclePlus] \[FormalA]) \[CirclePlus] \[FormalB]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 48}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 41}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CirclePlus] (\[FormalB]_)) \[CirclePlus] 
           ((\[FormalB]_) \[CirclePlus] (\[FormalC]_)) -> 
          (\[FormalA] \[CirclePlus] \[FormalB]) \[CirclePlus] \[FormalC], 
        "OutputExpression" -> HoldForm[(\[FormalA] \[CirclePlus] 
             \[FormalB]) \[CirclePlus] \[FormalC] == 
           (\[FormalC] \[CirclePlus] \[FormalA]) \[CirclePlus] \[FormalB]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 25} -> 
     <|"Statement" -> HoldForm[(\[FormalR] \[CirclePlus] 
           \[FormalQ]) \[CirclePlus] \[FormalS] == \[FormalQ] \[CirclePlus] 
          (\[FormalR] \[CirclePlus] \[FormalS])], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Position" -> {1}, 
        "Construct" -> {"Axiom", 6}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CirclePlus] (\[FormalB]_) -> 
          \[FormalB] \[CirclePlus] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalR] \[CirclePlus] \[FormalQ]) \[CirclePlus] 
            \[FormalS] == \[FormalQ] \[CirclePlus] (\[FormalR] \[CirclePlus] 
             \[FormalS])], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 26} -> 
     <|"Statement" -> HoldForm[(\[FormalR] \[CirclePlus] 
           \[FormalQ]) \[CirclePlus] \[FormalS] == \[FormalQ] \[CirclePlus] 
          (\[FormalS] \[CirclePlus] \[FormalR])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 25}, "Position" -> {2}, 
        "Construct" -> {"Axiom", 6}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CirclePlus] (\[FormalB]_) -> 
          \[FormalB] \[CirclePlus] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalR] \[CirclePlus] \[FormalQ]) \[CirclePlus] 
            \[FormalS] == \[FormalQ] \[CirclePlus] (\[FormalS] \[CirclePlus] 
             \[FormalR])], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 27} -> 
     <|"Statement" -> HoldForm[(\[FormalR] \[CirclePlus] 
           \[FormalQ]) \[CirclePlus] \[FormalS] == 
         (\[FormalS] \[CirclePlus] \[FormalR]) \[CirclePlus] \[FormalQ]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 26}, "Position" -> {}, 
        "Construct" -> {"Axiom", 6}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CirclePlus] (\[FormalB]_) -> 
          \[FormalB] \[CirclePlus] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalR] \[CirclePlus] \[FormalQ]) \[CirclePlus] 
            \[FormalS] == (\[FormalS] \[CirclePlus] \[FormalR]) \[CirclePlus] 
            \[FormalQ]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"Conclusion", 2} -> 
     <|"Statement" -> HoldForm[(\[FormalS] \[CirclePlus] 
           \[FormalR]) \[CirclePlus] \[FormalQ] == 
         (\[FormalS] \[CirclePlus] \[FormalR]) \[CirclePlus] \[FormalQ]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 27}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 24}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CirclePlus] (\[FormalB]_)) \[CirclePlus] 
           (\[FormalC]_) -> (\[FormalC] \[CirclePlus] 
            \[FormalA]) \[CirclePlus] \[FormalB], "OutputExpression" -> 
         HoldForm[(\[FormalS] \[CirclePlus] \[FormalR]) \[CirclePlus] 
            \[FormalQ] == (\[FormalS] \[CirclePlus] \[FormalR]) \[CirclePlus] 
            \[FormalQ]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 28} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CircleTimes] 
           \[FormalB]) \[CircleTimes] \[FormalC] == 
         (\[FormalC] \[CircleTimes] \[FormalA]) \[CircleTimes] \[FormalB]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 49}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 47}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CircleTimes] (\[FormalB]_)) \[CircleTimes] 
           ((\[FormalB]_) \[CircleTimes] (\[FormalC]_)) -> 
          (\[FormalA] \[CircleTimes] \[FormalB]) \[CircleTimes] \[FormalC], 
        "OutputExpression" -> HoldForm[(\[FormalA] \[CircleTimes] 
             \[FormalB]) \[CircleTimes] \[FormalC] == 
           (\[FormalC] \[CircleTimes] \[FormalA]) \[CircleTimes] \[FormalB]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 50} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CircleTimes] 
           \[FormalB]) \[CircleTimes] \[FormalC] == \[FormalA] \[CircleTimes] 
          (\[FormalB] \[CircleTimes] \[FormalC])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 28}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CircleTimes] 
            (\[FormalB]_)) \[CircleTimes] (\[FormalC]_) <-> 
          ((\[FormalC]_) \[CircleTimes] (\[FormalA]_)) \[CircleTimes] 
           (\[FormalB]_), "Side" -> 1, "Subpattern" -> 
         ((\[FormalA]_) \[CircleTimes] (\[FormalB]_)) \[CircleTimes] 
          (\[FormalC]_), "MatchingConstruct" -> {"Axiom", 3}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleTimes] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CircleTimes] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"SubstitutionLemma", 29} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          (\[FormalB] \[CircleTimes] OverBar[\[FormalA] \[CircleTimes] 
             \[FormalB]]) == \[FormalB] \[CircleTimes] OverBar[\[FormalB]]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 17}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 50}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CircleTimes] (\[FormalB]_)) \[CircleTimes] 
           (\[FormalC]_) -> \[FormalA] \[CircleTimes] 
           (\[FormalB] \[CircleTimes] \[FormalC]), "OutputExpression" -> 
         HoldForm[\[FormalA] \[CircleTimes] (\[FormalB] \[CircleTimes] 
             OverBar[\[FormalA] \[CircleTimes] \[FormalB]]) == 
           \[FormalB] \[CircleTimes] OverBar[\[FormalB]]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 51} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CirclePlus] 
          \[FormalB] \[CircleTimes] OverBar[
            OverBar[\[FormalA]] \[CircleTimes] \[FormalB]] == 
         \[FormalA] \[CirclePlus] \[FormalB] \[CircleTimes] 
           OverBar[\[FormalB]]], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 8}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CirclePlus] 
           OverBar[\[FormalA]_] \[CircleTimes] (\[FormalB]_) -> 
          \[FormalA] \[CirclePlus] \[FormalB], "Side" -> 1, 
        "Subpattern" -> OverBar[\[FormalA]_] \[CircleTimes] (\[FormalB]_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 29}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleTimes] ((\[FormalB]_) \[CircleTimes] 
            OverBar[(\[FormalA]_) \[CircleTimes] (\[FormalB]_)]) -> 
          \[FormalB] \[CircleTimes] OverBar[\[FormalB]], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 30} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CirclePlus] 
          \[FormalB] \[CircleTimes] OverBar[
            OverBar[\[FormalA]] \[CircleTimes] \[FormalB]] == \[FormalA]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 51}, "Position" -> {}, 
        "Construct" -> {"Axiom", 2}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CirclePlus] (\[FormalB]_) \[CircleTimes] 
            OverBar[\[FormalB]_] -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CirclePlus] \[FormalB] \[CircleTimes] 
             OverBar[OverBar[\[FormalA]] \[CircleTimes] \[FormalB]] == 
           \[FormalA]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 52} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          (\[FormalB] \[CirclePlus] OverBar[
            OverBar[\[FormalB]] \[CircleTimes] \[FormalA]]) == 
         \[FormalA] \[CircleTimes] \[FormalB]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 12}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CircleTimes] 
           ((\[FormalB]_) \[CirclePlus] (\[FormalA]_) \[CircleTimes] 
             (\[FormalC]_)) -> \[FormalA] \[CircleTimes] 
           (\[FormalB] \[CirclePlus] \[FormalC]), "Side" -> 1, 
        "Subpattern" -> (\[FormalB]_) \[CirclePlus] 
          (\[FormalA]_) \[CircleTimes] (\[FormalC]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 30}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CirclePlus] 
           (\[FormalB]_) \[CircleTimes] OverBar[
             OverBar[\[FormalA]_] \[CircleTimes] (\[FormalB]_)] -> 
          \[FormalA], "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 31} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          OverBar[OverBar[\[FormalB]] \[CircleTimes] \[FormalA]] == 
         \[FormalA] \[CircleTimes] \[FormalB]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 52}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 23}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CirclePlus] OverBar[
            OverBar[\[FormalA]_] \[CircleTimes] (\[FormalB]_)] -> 
          OverBar[OverBar[\[FormalA]] \[CircleTimes] \[FormalB]], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CircleTimes] 
            OverBar[OverBar[\[FormalB]] \[CircleTimes] \[FormalA]] == 
           \[FormalA] \[CircleTimes] \[FormalB]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 53} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA]] \[CirclePlus] 
          OverBar[OverBar[\[FormalB]] \[CircleTimes] \[FormalA]] == 
         OverBar[\[FormalA]] \[CirclePlus] \[FormalA] \[CircleTimes] 
           \[FormalB]], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 
          19}, "Orientation" -> -1, "Rule" -> 
         OverBar[\[FormalA]_] \[CirclePlus] (\[FormalA]_) \[CircleTimes] 
            (\[FormalB]_) -> OverBar[\[FormalA]] \[CirclePlus] \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CircleTimes] 
          (\[FormalB]_), "MatchingConstruct" -> {"SubstitutionLemma", 31}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleTimes] OverBar[
            OverBar[\[FormalB]_] \[CircleTimes] (\[FormalA]_)] -> 
          \[FormalA] \[CircleTimes] \[FormalB], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 32} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[\[FormalA]] \[CircleTimes] 
           \[FormalB]] == OverBar[\[FormalB]] \[CirclePlus] 
          \[FormalB] \[CircleTimes] \[FormalA]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 53}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 20}, "Orientation" -> 1, 
        "Rule" -> OverBar[\[FormalA]_] \[CirclePlus] 
           OverBar[(\[FormalB]_) \[CircleTimes] (\[FormalA]_)] -> 
          OverBar[\[FormalB] \[CircleTimes] \[FormalA]], 
        "OutputExpression" -> HoldForm[
          OverBar[OverBar[\[FormalA]] \[CircleTimes] \[FormalB]] == 
           OverBar[\[FormalB]] \[CirclePlus] \[FormalB] \[CircleTimes] 
             \[FormalA]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 33} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[\[FormalA]] \[CircleTimes] 
           \[FormalB]] == OverBar[\[FormalB]] \[CirclePlus] \[FormalA]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 32}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 19}, "Orientation" -> -1, 
        "Rule" -> OverBar[\[FormalA]_] \[CirclePlus] 
           (\[FormalA]_) \[CircleTimes] (\[FormalB]_) -> 
          OverBar[\[FormalA]] \[CirclePlus] \[FormalB], 
        "OutputExpression" -> HoldForm[
          OverBar[OverBar[\[FormalA]] \[CircleTimes] \[FormalB]] == 
           OverBar[\[FormalB]] \[CirclePlus] \[FormalA]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 54} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA]] \[CirclePlus] 
          OverBar[\[FormalB]] == OverBar[\[FormalB] \[CircleTimes] 
           \[FormalA]]], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 
          33}, "Orientation" -> 1, "Rule" -> 
         OverBar[OverBar[\[FormalA]_] \[CircleTimes] (\[FormalB]_)] -> 
          OverBar[\[FormalB]] \[CirclePlus] \[FormalA], "Side" -> 1, 
        "Subpattern" -> OverBar[\[FormalA]_], "MatchingConstruct" -> 
         {"SubstitutionLemma", 4}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> OverBar[OverBar[\[FormalA]_]] -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {1, 1}|>|>, 
    {"SubstitutionLemma", 34} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalU] \[CirclePlus] 
            OverBar[\[FormalT]]] \[CirclePlus] OverBar[
           OverBar[\[FormalT]] \[CirclePlus] OverBar[\[FormalU]]] == 
         \[FormalT]], "Proof" -> <|"Input" -> {"Hypothesis", 2}, 
        "Position" -> {1, 1}, "Construct" -> {"Axiom", 6}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CirclePlus] 
           (\[FormalB]_) -> \[FormalB] \[CirclePlus] \[FormalA], 
        "OutputExpression" -> HoldForm[
          OverBar[\[FormalU] \[CirclePlus] OverBar[\[FormalT]]] \[CirclePlus] 
            OverBar[OverBar[\[FormalT]] \[CirclePlus] OverBar[\[FormalU]]] == 
           \[FormalT]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 35} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[\[FormalT]] \[CirclePlus] 
            OverBar[\[FormalU]]] \[CirclePlus] OverBar[
           \[FormalU] \[CirclePlus] OverBar[\[FormalT]]] == \[FormalT]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 34}, "Position" -> {}, 
        "Construct" -> {"Axiom", 6}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CirclePlus] (\[FormalB]_) -> 
          \[FormalB] \[CirclePlus] \[FormalA], "OutputExpression" -> 
         HoldForm[OverBar[OverBar[\[FormalT]] \[CirclePlus] 
              OverBar[\[FormalU]]] \[CirclePlus] OverBar[
             \[FormalU] \[CirclePlus] OverBar[\[FormalT]]] == \[FormalT]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 36} -> 
     <|"Statement" -> HoldForm[OverBar[(\[FormalU] \[CirclePlus] 
            OverBar[\[FormalT]]) \[CircleTimes] 
           (OverBar[\[FormalT]] \[CirclePlus] OverBar[\[FormalU]])] == 
         \[FormalT]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 35}, 
        "Position" -> {}, "Construct" -> {"CriticalPairLemma", 54}, 
        "Orientation" -> 1, "Rule" -> OverBar[\[FormalA]_] \[CirclePlus] 
           OverBar[\[FormalB]_] -> OverBar[\[FormalB] \[CircleTimes] 
            \[FormalA]], "OutputExpression" -> HoldForm[
          OverBar[(\[FormalU] \[CirclePlus] OverBar[
               \[FormalT]]) \[CircleTimes] (OverBar[\[FormalT]] \[CirclePlus] 
              OverBar[\[FormalU]])] == \[FormalT]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 37} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[\[FormalT]] \[CirclePlus] 
           \[FormalU] \[CircleTimes] OverBar[\[FormalU]]] == \[FormalT]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 36}, "Position" -> {1}, 
        "Construct" -> {"CriticalPairLemma", 6}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CirclePlus] (\[FormalB]_)) \[CircleTimes] 
           ((\[FormalB]_) \[CirclePlus] (\[FormalC]_)) -> 
          \[FormalB] \[CirclePlus] \[FormalA] \[CircleTimes] \[FormalC], 
        "OutputExpression" -> HoldForm[
          OverBar[OverBar[\[FormalT]] \[CirclePlus] \[FormalU] \[CircleTimes] 
              OverBar[\[FormalU]]] == \[FormalT]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 38} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[\[FormalT]] \[CirclePlus] 
           OverBar[\[FormalU] \[CirclePlus] OverBar[\[FormalU]]]] == 
         \[FormalT]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 37}, 
        "Position" -> {1, 2}, "Construct" -> {"CriticalPairLemma", 16}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CircleTimes] 
           OverBar[\[FormalA]_] -> OverBar[\[FormalU] \[CirclePlus] 
            OverBar[\[FormalU]]], "OutputExpression" -> 
         HoldForm[OverBar[OverBar[\[FormalT]] \[CirclePlus] 
             OverBar[\[FormalU] \[CirclePlus] OverBar[\[FormalU]]]] == 
           \[FormalT]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 39} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[\[FormalT]] \[CirclePlus] 
           OverBar[OverBar[\[FormalU]] \[CirclePlus] \[FormalU]]] == 
         \[FormalT]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 38}, 
        "Position" -> {1, 2, 1}, "Construct" -> {"Axiom", 6}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CirclePlus] 
           (\[FormalB]_) -> \[FormalB] \[CirclePlus] \[FormalA], 
        "OutputExpression" -> HoldForm[
          OverBar[OverBar[\[FormalT]] \[CirclePlus] OverBar[
              OverBar[\[FormalU]] \[CirclePlus] \[FormalU]]] == \[FormalT]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 40} -> 
     <|"Statement" -> HoldForm[
        OverBar[OverBar[OverBar[\[FormalU]] \[CirclePlus] 
             \[FormalU]] \[CirclePlus] OverBar[\[FormalT]]] == \[FormalT]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 39}, "Position" -> {1}, 
        "Construct" -> {"Axiom", 6}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CirclePlus] (\[FormalB]_) -> 
          \[FormalB] \[CirclePlus] \[FormalA], "OutputExpression" -> 
         HoldForm[OverBar[OverBar[OverBar[\[FormalU]] \[CirclePlus] 
               \[FormalU]] \[CirclePlus] OverBar[\[FormalT]]] == \[FormalT]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 41} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[\[FormalT] \[CircleTimes] 
            (OverBar[\[FormalU]] \[CirclePlus] \[FormalU])]] == \[FormalT]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 40}, "Position" -> {1}, 
        "Construct" -> {"CriticalPairLemma", 54}, "Orientation" -> 1, 
        "Rule" -> OverBar[\[FormalA]_] \[CirclePlus] OverBar[\[FormalB]_] -> 
          OverBar[\[FormalB] \[CircleTimes] \[FormalA]], 
        "OutputExpression" -> HoldForm[
          OverBar[OverBar[\[FormalT] \[CircleTimes] (OverBar[
                \[FormalU]] \[CirclePlus] \[FormalU])]] == \[FormalT]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 42} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[\[FormalT] \[CircleTimes] 
            (\[FormalU] \[CirclePlus] OverBar[\[FormalU]])]] == \[FormalT]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 41}, 
        "Position" -> {1, 1, 2}, "Construct" -> {"Axiom", 6}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CirclePlus] 
           (\[FormalB]_) -> \[FormalB] \[CirclePlus] \[FormalA], 
        "OutputExpression" -> HoldForm[
          OverBar[OverBar[\[FormalT] \[CircleTimes] (\[FormalU] \[CirclePlus] 
               OverBar[\[FormalU]])]] == \[FormalT]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 43} -> 
     <|"Statement" -> HoldForm[\[FormalT] \[CircleTimes] 
          (\[FormalU] \[CirclePlus] OverBar[\[FormalU]]) == \[FormalT]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 42}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 4}, "Orientation" -> -1, 
        "Rule" -> OverBar[OverBar[\[FormalA]_]] -> \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalT] \[CircleTimes] 
            (\[FormalU] \[CirclePlus] OverBar[\[FormalU]]) == \[FormalT]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"Conclusion", 3} -> <|"Statement" -> HoldForm[\[FormalT] == \[FormalT]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 43}, "Position" -> {}, 
        "Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] ((\[FormalB]_) \[CirclePlus] 
            OverBar[\[FormalB]_]) -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalT] == \[FormalT]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>}|>]
