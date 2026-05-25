ProofObject["EquationalLogic", 
 {ForAll[\[FormalA], OverTilde[1] \[CircleTimes] \[FormalA] == \[FormalA]]}, 
 {ForAll[{\[FormalA], \[FormalB], \[FormalC]}, 
   \[FormalA] \[CircleTimes] (\[FormalB] \[CircleTimes] \[FormalC]) == 
    (\[FormalA] \[CircleTimes] \[FormalB]) \[CircleTimes] \[FormalC]], 
  ForAll[\[FormalA], \[FormalA] \[CircleTimes] OverTilde[1] == \[FormalA]], 
  ForAll[\[FormalA], \[FormalA] \[CircleTimes] OverBar[\[FormalA]] == 
    OverTilde[1]]}, <|"Variables" -> {\[FormalA], \[FormalB], \[FormalC], 
    \[FormalD], \[FormalE], \[FormalF]}, "Constants" -> {}, 
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
       HoldForm[OverTilde[1] \[CircleTimes] \[FormalF] == \[FormalF]], 
      "Proof" -> <||>|>, {"CriticalPairLemma", 1} -> 
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
        "Position" -> {1}|>|>, {"CriticalPairLemma", 3} -> 
     <|"Statement" -> HoldForm[OverTilde[1] \[CircleTimes] 
          OverBar[OverBar[\[FormalA]]] == \[FormalA] \[CircleTimes] 
          OverTilde[1]], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 
          2}, "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CircleTimes] 
           (OverBar[\[FormalA]_] \[CircleTimes] (\[FormalB]_)) -> 
          OverTilde[1] \[CircleTimes] \[FormalB], "Side" -> 1, 
        "Subpattern" -> OverBar[\[FormalA]_] \[CircleTimes] (\[FormalB]_), 
        "MatchingConstruct" -> {"Axiom", 3}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CircleTimes] 
           OverBar[\[FormalA]_] -> OverTilde[1], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[OverTilde[1] \[CircleTimes] 
          OverBar[OverBar[\[FormalA]]] == \[FormalA]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 3}, "Position" -> {}, 
        "Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] OverTilde[1] -> \[FormalA], 
        "OutputExpression" -> HoldForm[OverTilde[1] \[CircleTimes] 
            OverBar[OverBar[\[FormalA]]] == \[FormalA]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 4} -> 
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
        "Position" -> {}, "Construct" -> {"CriticalPairLemma", 4}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CircleTimes] 
           OverBar[OverBar[\[FormalB]_]] -> \[FormalA] \[CircleTimes] 
           \[FormalB], "OutputExpression" -> HoldForm[
          OverTilde[1] \[CircleTimes] \[FormalA] == \[FormalA]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"Conclusion", 1} -> <|"Statement" -> HoldForm[\[FormalF] == \[FormalF]], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 2}, "Orientation" -> 1, 
        "Rule" -> OverTilde[1] \[CircleTimes] (\[FormalA]_) -> \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalF] == \[FormalF]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>}|>]
