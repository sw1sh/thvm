ProofObject["EquationalLogic", {ForAll[{\[FormalA], \[FormalB]}, 
   OverBar[\[FormalA] \[CircleTimes] \[FormalB]] == 
    OverBar[\[FormalB]] \[CircleTimes] OverBar[\[FormalA]]]}, 
 {ForAll[{\[FormalA], \[FormalB], \[FormalC]}, 
   \[FormalA] \[CircleTimes] (\[FormalB] \[CircleTimes] \[FormalC]) == 
    (\[FormalA] \[CircleTimes] \[FormalB]) \[CircleTimes] \[FormalC]], 
  ForAll[\[FormalA], \[FormalA] \[CircleTimes] OverTilde[1] == \[FormalA]], 
  ForAll[\[FormalA], \[FormalA] \[CircleTimes] OverBar[\[FormalA]] == 
    OverTilde[1]]}, <|"Variables" -> {\[FormalA], \[FormalB], \[FormalC], 
    \[FormalD], \[FormalE], \[FormalF], \[FormalG]}, "Constants" -> {}, 
  "Proof" -> {{"Axiom", 1} -> <|"Statement" -> 
       HoldForm[\[FormalA] == \[FormalA] \[CircleTimes] OverTilde[1]], 
      "Proof" -> <||>|>, {"Axiom", 2} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          (\[FormalB] \[CircleTimes] \[FormalC]) == 
         (\[FormalA] \[CircleTimes] \[FormalB]) \[CircleTimes] \[FormalC]], 
      "Proof" -> <||>|>, {"Axiom", 3} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          OverBar[\[FormalA]] == OverTilde[1]], "Proof" -> <||>|>, 
    {"Hypothesis", 1} -> <|"Statement" -> 
       HoldForm[OverBar[\[FormalG]] \[CircleTimes] OverBar[\[FormalF]] == 
         OverBar[\[FormalF] \[CircleTimes] \[FormalG]]], "Proof" -> <||>|>, 
    {"CriticalPairLemma", 1} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          (OverTilde[1] \[CircleTimes] \[FormalB]) == 
         \[FormalA] \[CircleTimes] \[FormalB]], 
      "Proof" -> <|"Construct" -> {"Axiom", 2}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CircleTimes] (\[FormalB]_)) \[CircleTimes] 
           (\[FormalC]_) -> \[FormalA] \[CircleTimes] 
           (\[FormalB] \[CircleTimes] \[FormalC]), "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CircleTimes] (\[FormalB]_), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CircleTimes] OverTilde[1] -> 
          \[FormalA], "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 2} -> 
     <|"Statement" -> HoldForm[OverTilde[1] == \[FormalA] \[CircleTimes] 
          (\[FormalB] \[CircleTimes] OverBar[\[FormalA] \[CircleTimes] 
             \[FormalB]])], "Proof" -> <|"Construct" -> {"Axiom", 3}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CircleTimes] 
           OverBar[\[FormalA]_] -> OverTilde[1], "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CircleTimes] OverBar[\[FormalA]_], 
        "MatchingConstruct" -> {"Axiom", 2}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CircleTimes] 
            (\[FormalB]_)) \[CircleTimes] (\[FormalC]_) -> 
          \[FormalA] \[CircleTimes] (\[FormalB] \[CircleTimes] \[FormalC]), 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"CriticalPairLemma", 3} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          (OverBar[\[FormalA]] \[CircleTimes] \[FormalB]) == 
         OverTilde[1] \[CircleTimes] \[FormalB]], 
      "Proof" -> <|"Construct" -> {"Axiom", 2}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CircleTimes] (\[FormalB]_)) \[CircleTimes] 
           (\[FormalC]_) -> \[FormalA] \[CircleTimes] 
           (\[FormalB] \[CircleTimes] \[FormalC]), "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CircleTimes] (\[FormalB]_), 
        "MatchingConstruct" -> {"Axiom", 3}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CircleTimes] 
           OverBar[\[FormalA]_] -> OverTilde[1], "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 4} -> 
     <|"Statement" -> HoldForm[OverTilde[1] \[CircleTimes] 
          OverBar[OverBar[\[FormalA]]] == \[FormalA] \[CircleTimes] 
          OverTilde[1]], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 
          3}, "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CircleTimes] 
           (OverBar[\[FormalA]_] \[CircleTimes] (\[FormalB]_)) -> 
          OverTilde[1] \[CircleTimes] \[FormalB], "Side" -> 1, 
        "Subpattern" -> OverBar[\[FormalA]_] \[CircleTimes] (\[FormalB]_), 
        "MatchingConstruct" -> {"Axiom", 3}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CircleTimes] 
           OverBar[\[FormalA]_] -> OverTilde[1], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[OverTilde[1] \[CircleTimes] 
          OverBar[OverBar[\[FormalA]]] == \[FormalA]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 4}, "Position" -> {}, 
        "Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] OverTilde[1] -> \[FormalA], 
        "OutputExpression" -> HoldForm[OverTilde[1] \[CircleTimes] 
            OverBar[OverBar[\[FormalA]]] == \[FormalA]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 5} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          OverBar[OverBar[\[FormalB]]] == \[FormalA] \[CircleTimes] 
          \[FormalB]], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 1}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CircleTimes] 
           (OverTilde[1] \[CircleTimes] (\[FormalB]_)) -> 
          \[FormalA] \[CircleTimes] \[FormalB], "Side" -> 1, 
        "Subpattern" -> OverTilde[1] \[CircleTimes] (\[FormalB]_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 1}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         OverTilde[1] \[CircleTimes] OverBar[OverBar[\[FormalA]_]] -> 
          \[FormalA], "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[OverTilde[1] \[CircleTimes] \[FormalA] == 
         \[FormalA]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 1}, 
        "Position" -> {}, "Construct" -> {"CriticalPairLemma", 5}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CircleTimes] 
           OverBar[OverBar[\[FormalB]_]] -> \[FormalA] \[CircleTimes] 
           \[FormalB], "OutputExpression" -> HoldForm[
          OverTilde[1] \[CircleTimes] \[FormalA] == \[FormalA]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 3} -> 
     <|"Statement" -> HoldForm[(\[FormalA]_) \[CircleTimes] 
          (OverBar[\[FormalA]_] \[CircleTimes] (\[FormalB]_)) -> \[FormalB]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 3}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 2}, "Orientation" -> 1, 
        "Rule" -> OverTilde[1] \[CircleTimes] (\[FormalA]_) -> \[FormalA], 
        "OutputExpression" -> HoldForm[(\[FormalA]_) \[CircleTimes] 
            (OverBar[\[FormalA]_] \[CircleTimes] (\[FormalB]_)) -> 
           \[FormalB]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 6} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[\[FormalA]]] == 
         OverTilde[1] \[CircleTimes] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 2}, 
        "Orientation" -> 1, "Rule" -> OverTilde[1] \[CircleTimes] 
           (\[FormalA]_) -> \[FormalA], "Side" -> 1, "Subpattern" -> 
         OverTilde[1] \[CircleTimes] (\[FormalA]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 5}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CircleTimes] 
           OverBar[OverBar[\[FormalB]_]] -> \[FormalA] \[CircleTimes] 
           \[FormalB], "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"SubstitutionLemma", 4} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[\[FormalA]]] == \[FormalA]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 6}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 2}, "Orientation" -> 1, 
        "Rule" -> OverTilde[1] \[CircleTimes] (\[FormalA]_) -> \[FormalA], 
        "OutputExpression" -> HoldForm[OverBar[OverBar[\[FormalA]]] == 
           \[FormalA]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 7} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         OverBar[\[FormalB]] \[CircleTimes] (\[FormalB] \[CircleTimes] 
           \[FormalA])], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 
          3}, "Orientation" -> 1, "Rule" -> OverTilde[1] \[CircleTimes] 
           (\[FormalA]_) -> \[FormalA], "Side" -> 1, "Subpattern" -> 
         OverBar[\[FormalA]_], "MatchingConstruct" -> {"SubstitutionLemma", 
          4}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         OverBar[OverBar[\[FormalA]_]] -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2, 1}|>|>, {"CriticalPairLemma", 8} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          OverBar[\[FormalB] \[CircleTimes] \[FormalA]] == 
         OverBar[\[FormalB]] \[CircleTimes] OverTilde[1]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 7}, 
        "Orientation" -> -1, "Rule" -> OverBar[\[FormalA]_] \[CircleTimes] 
           ((\[FormalA]_) \[CircleTimes] (\[FormalB]_)) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CircleTimes] 
          (\[FormalB]_), "MatchingConstruct" -> {"CriticalPairLemma", 2}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleTimes] ((\[FormalB]_) \[CircleTimes] 
            OverBar[(\[FormalA]_) \[CircleTimes] (\[FormalB]_)]) -> 
          OverTilde[1], "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 5} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          OverBar[\[FormalB] \[CircleTimes] \[FormalA]] == 
         OverBar[\[FormalB]]], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 8}, "Position" -> {}, 
        "Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] OverTilde[1] -> \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CircleTimes] 
            OverBar[\[FormalB] \[CircleTimes] \[FormalA]] == 
           OverBar[\[FormalB]]], "ConstructSide" -> 1, "InputOrientation" -> 
         1, "Side" -> 2|>|>, {"CriticalPairLemma", 9} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA] \[CircleTimes] 
           \[FormalB]] == OverBar[\[FormalB]] \[CircleTimes] 
          OverBar[\[FormalA]]], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 7}, "Orientation" -> -1, 
        "Rule" -> OverBar[\[FormalA]_] \[CircleTimes] 
           ((\[FormalA]_) \[CircleTimes] (\[FormalB]_)) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CircleTimes] 
          (\[FormalB]_), "MatchingConstruct" -> {"SubstitutionLemma", 5}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleTimes] OverBar[(\[FormalB]_) \[CircleTimes] 
             (\[FormalA]_)] -> OverBar[\[FormalB]], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"Conclusion", 1} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalF] \[CircleTimes] 
           \[FormalG]] == OverBar[\[FormalF] \[CircleTimes] \[FormalG]]], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 9}, "Orientation" -> -1, 
        "Rule" -> OverBar[\[FormalA]_] \[CircleTimes] OverBar[\[FormalB]_] -> 
          OverBar[\[FormalB] \[CircleTimes] \[FormalA]], 
        "OutputExpression" -> HoldForm[OverBar[\[FormalF] \[CircleTimes] 
             \[FormalG]] == OverBar[\[FormalF] \[CircleTimes] \[FormalG]]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>}|>]
