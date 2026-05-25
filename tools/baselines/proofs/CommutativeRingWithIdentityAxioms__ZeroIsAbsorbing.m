ProofObject["EquationalLogic", 
 {ForAll[\[FormalA], \[FormalA] \[CircleTimes] OverTilde[0] == 
    OverTilde[0]]}, {ForAll[{\[FormalA], \[FormalB], \[FormalC]}, 
   \[FormalA] \[CirclePlus] (\[FormalB] \[CirclePlus] \[FormalC]) == 
    (\[FormalA] \[CirclePlus] \[FormalB]) \[CirclePlus] \[FormalC]], 
  ForAll[{\[FormalA], \[FormalB]}, \[FormalA] \[CirclePlus] \[FormalB] == 
    \[FormalB] \[CirclePlus] \[FormalA]], ForAll[\[FormalA], 
   \[FormalA] \[CirclePlus] OverTilde[0] == \[FormalA]], 
  ForAll[\[FormalA], \[FormalA] \[CirclePlus] OverBar[\[FormalA]] == 
    OverTilde[0]], ForAll[{\[FormalA], \[FormalB], \[FormalC]}, 
   \[FormalA] \[CircleTimes] (\[FormalB] \[CirclePlus] \[FormalC]) == 
    \[FormalA] \[CircleTimes] \[FormalB] \[CirclePlus] 
     \[FormalA] \[CircleTimes] \[FormalC]], 
  ForAll[{\[FormalA], \[FormalB], \[FormalC]}, 
   \[FormalA] \[CircleTimes] (\[FormalB] \[CircleTimes] \[FormalC]) == 
    (\[FormalA] \[CircleTimes] \[FormalB]) \[CircleTimes] \[FormalC]], 
  ForAll[{\[FormalA], \[FormalB]}, \[FormalA] \[CircleTimes] \[FormalB] == 
    \[FormalB] \[CircleTimes] \[FormalA]], ForAll[\[FormalA], 
   \[FormalA] \[CircleTimes] OverTilde[1] == \[FormalA]]}, 
 <|"Variables" -> {\[FormalA], \[FormalB], \[FormalC], \[FormalD], 
    \[FormalE], \[FormalF], \[FormalG], \[FormalH], \[FormalI], \[FormalJ], 
    \[FormalK], \[FormalL], \[FormalM], \[FormalN], \[FormalO], \[FormalP], 
    \[FormalQ]}, "Constants" -> {}, 
  "Proof" -> {{"Axiom", 1} -> <|"Statement" -> 
       HoldForm[\[FormalA] == \[FormalA] \[CirclePlus] OverTilde[0]], 
      "Proof" -> <||>|>, {"Axiom", 2} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CirclePlus] \[FormalB] == 
         \[FormalB] \[CirclePlus] \[FormalA]], "Proof" -> <||>|>, 
    {"Axiom", 3} -> <|"Statement" -> HoldForm[
        \[FormalA] \[CirclePlus] (\[FormalB] \[CirclePlus] \[FormalC]) == 
         (\[FormalA] \[CirclePlus] \[FormalB]) \[CirclePlus] \[FormalC]], 
      "Proof" -> <||>|>, {"Axiom", 4} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CirclePlus] 
          OverBar[\[FormalA]] == OverTilde[0]], "Proof" -> <||>|>, 
    {"Axiom", 5} -> <|"Statement" -> HoldForm[
        \[FormalA] \[CircleTimes] \[FormalB] \[CirclePlus] 
          \[FormalA] \[CircleTimes] \[FormalC] == \[FormalA] \[CircleTimes] 
          (\[FormalB] \[CirclePlus] \[FormalC])], "Proof" -> <||>|>, 
    {"Hypothesis", 1} -> <|"Statement" -> 
       HoldForm[\[FormalQ] \[CircleTimes] OverTilde[0] == OverTilde[0]], 
      "Proof" -> <||>|>, {"CriticalPairLemma", 1} -> 
     <|"Statement" -> HoldForm[OverTilde[0] \[CirclePlus] \[FormalA] == 
         \[FormalA]], "Proof" -> <|"Construct" -> {"Axiom", 2}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CirclePlus] 
           (\[FormalB]_) <-> (\[FormalB]_) \[CirclePlus] (\[FormalA]_), 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CirclePlus] 
          (\[FormalB]_), "MatchingConstruct" -> {"Axiom", 1}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CirclePlus] OverTilde[0] -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"CriticalPairLemma", 2} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CirclePlus] 
          (OverBar[\[FormalA]] \[CirclePlus] \[FormalB]) == 
         OverTilde[0] \[CirclePlus] \[FormalB]], 
      "Proof" -> <|"Construct" -> {"Axiom", 3}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CirclePlus] (\[FormalB]_)) \[CirclePlus] 
           (\[FormalC]_) -> \[FormalA] \[CirclePlus] 
           (\[FormalB] \[CirclePlus] \[FormalC]), "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CirclePlus] (\[FormalB]_), 
        "MatchingConstruct" -> {"Axiom", 4}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CirclePlus] OverBar[\[FormalA]_] -> 
          OverTilde[0], "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 3} -> 
     <|"Statement" -> HoldForm[OverBar[OverTilde[0]] == OverTilde[0]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 1}, 
        "Orientation" -> 1, "Rule" -> OverTilde[0] \[CirclePlus] 
           (\[FormalA]_) -> \[FormalA], "Side" -> 1, "Subpattern" -> 
         OverTilde[0] \[CirclePlus] (\[FormalA]_), "MatchingConstruct" -> 
         {"Axiom", 4}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CirclePlus] OverBar[\[FormalA]_] -> OverTilde[0], 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CirclePlus] 
          (OverBar[\[FormalA]] \[CirclePlus] \[FormalB]) == \[FormalB]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 2}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 1}, "Orientation" -> 1, 
        "Rule" -> OverTilde[0] \[CirclePlus] (\[FormalA]_) -> \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CirclePlus] 
            (OverBar[\[FormalA]] \[CirclePlus] \[FormalB]) == \[FormalB]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 4} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[\[FormalA]]] == 
         \[FormalA] \[CirclePlus] OverTilde[0]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 1}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CirclePlus] 
           (OverBar[\[FormalA]_] \[CirclePlus] (\[FormalB]_)) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> OverBar[\[FormalA]_] \[CirclePlus] 
          (\[FormalB]_), "MatchingConstruct" -> {"Axiom", 4}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CirclePlus] OverBar[\[FormalA]_] -> OverTilde[0], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[\[FormalA]]] == \[FormalA]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 4}, "Position" -> {}, 
        "Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CirclePlus] OverTilde[0] -> \[FormalA], 
        "OutputExpression" -> HoldForm[OverBar[OverBar[\[FormalA]]] == 
           \[FormalA]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 5} -> 
     <|"Statement" -> HoldForm[\[FormalA] == \[FormalB] \[CirclePlus] 
          (\[FormalA] \[CirclePlus] OverBar[\[FormalB]])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 1}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CirclePlus] 
           (OverBar[\[FormalA]_] \[CirclePlus] (\[FormalB]_)) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> OverBar[\[FormalA]_] \[CirclePlus] 
          (\[FormalB]_), "MatchingConstruct" -> {"Axiom", 2}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CirclePlus] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CirclePlus] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 6} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         OverBar[\[FormalB]] \[CirclePlus] (\[FormalB] \[CirclePlus] 
           \[FormalA])], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 
          1}, "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CirclePlus] 
           (OverBar[\[FormalA]_] \[CirclePlus] (\[FormalB]_)) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> OverBar[\[FormalA]_], 
        "MatchingConstruct" -> {"SubstitutionLemma", 2}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         OverBar[OverBar[\[FormalA]_]] -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2, 1}|>|>, {"CriticalPairLemma", 7} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         OverBar[\[FormalB]] \[CirclePlus] (\[FormalA] \[CirclePlus] 
           \[FormalB])], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 
          5}, "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CirclePlus] 
           ((\[FormalB]_) \[CirclePlus] OverBar[\[FormalA]_]) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> OverBar[\[FormalA]_], 
        "MatchingConstruct" -> {"SubstitutionLemma", 2}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         OverBar[OverBar[\[FormalA]_]] -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 8} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA]] == 
         OverBar[\[FormalA] \[CirclePlus] \[FormalB]] \[CirclePlus] 
          \[FormalB]], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 7}, 
        "Orientation" -> -1, "Rule" -> OverBar[\[FormalA]_] \[CirclePlus] 
           ((\[FormalB]_) \[CirclePlus] (\[FormalA]_)) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CirclePlus] 
          (\[FormalA]_), "MatchingConstruct" -> {"CriticalPairLemma", 6}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         OverBar[\[FormalA]_] \[CirclePlus] ((\[FormalA]_) \[CirclePlus] 
            (\[FormalB]_)) -> \[FormalB], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 3} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA]] == 
         \[FormalB] \[CirclePlus] OverBar[\[FormalA] \[CirclePlus] 
            \[FormalB]]], "Proof" -> <|"Input" -> {"CriticalPairLemma", 8}, 
        "Position" -> {}, "Construct" -> {"Axiom", 2}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CirclePlus] (\[FormalB]_) -> 
          \[FormalB] \[CirclePlus] \[FormalA], "OutputExpression" -> 
         HoldForm[OverBar[\[FormalA]] == \[FormalB] \[CirclePlus] 
            OverBar[\[FormalA] \[CirclePlus] \[FormalB]]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 9} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA] \[CircleTimes] 
           \[FormalB]] == \[FormalA] \[CircleTimes] \[FormalC] \[CirclePlus] 
          OverBar[\[FormalA] \[CircleTimes] (\[FormalB] \[CirclePlus] 
             \[FormalC])]], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 3}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CirclePlus] OverBar[
            (\[FormalB]_) \[CirclePlus] (\[FormalA]_)] -> 
          OverBar[\[FormalB]], "Side" -> 1, "Subpattern" -> 
         (\[FormalB]_) \[CirclePlus] (\[FormalA]_), "MatchingConstruct" -> 
         {"Axiom", 5}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleTimes] (\[FormalB]_) \[CirclePlus] 
           (\[FormalA]_) \[CircleTimes] (\[FormalC]_) -> 
          \[FormalA] \[CircleTimes] (\[FormalB] \[CirclePlus] \[FormalC]), 
        "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"CriticalPairLemma", 10} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA] \[CircleTimes] 
           OverTilde[0]] == \[FormalA] \[CircleTimes] 
           \[FormalB] \[CirclePlus] OverBar[\[FormalA] \[CircleTimes] 
            \[FormalB]]], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 
          9}, "Orientation" -> -1, "Rule" -> 
         (\[FormalA]_) \[CircleTimes] (\[FormalB]_) \[CirclePlus] 
           OverBar[(\[FormalA]_) \[CircleTimes] ((\[FormalC]_) \[CirclePlus] 
              (\[FormalB]_))] -> OverBar[\[FormalA] \[CircleTimes] 
            \[FormalC]], "Side" -> 1, "Subpattern" -> 
         (\[FormalC]_) \[CirclePlus] (\[FormalB]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 1}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> OverTilde[0] \[CirclePlus] (\[FormalA]_) -> 
          \[FormalA], "MatchingSide" -> 1, "Position" -> {2, 1, 2}|>|>, 
    {"SubstitutionLemma", 4} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA] \[CircleTimes] 
           OverTilde[0]] == OverTilde[0]], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 10}, "Position" -> {}, 
        "Construct" -> {"Axiom", 4}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CirclePlus] OverBar[\[FormalA]_] -> 
          OverTilde[0], "OutputExpression" -> HoldForm[
          OverBar[\[FormalA] \[CircleTimes] OverTilde[0]] == OverTilde[0]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 11} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] OverTilde[0] == 
         OverBar[OverTilde[0]]], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 2}, "Orientation" -> 1, 
        "Rule" -> OverBar[OverBar[\[FormalA]_]] -> \[FormalA], "Side" -> 1, 
        "Subpattern" -> OverBar[\[FormalA]_], "MatchingConstruct" -> 
         {"SubstitutionLemma", 4}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> OverBar[(\[FormalA]_) \[CircleTimes] 
            OverTilde[0]] -> OverTilde[0], "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 5} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] OverTilde[0] == 
         OverTilde[0]], "Proof" -> <|"Input" -> {"CriticalPairLemma", 11}, 
        "Position" -> {}, "Construct" -> {"CriticalPairLemma", 3}, 
        "Orientation" -> 1, "Rule" -> OverBar[OverTilde[0]] -> OverTilde[0], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CircleTimes] 
            OverTilde[0] == OverTilde[0]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"Conclusion", 1} -> <|"Statement" -> HoldForm[OverTilde[0] == 
         OverTilde[0]], "Proof" -> <|"Input" -> {"Hypothesis", 1}, 
        "Position" -> {}, "Construct" -> {"SubstitutionLemma", 5}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CircleTimes] 
           OverTilde[0] -> OverTilde[0], "OutputExpression" -> 
         HoldForm[OverTilde[0] == OverTilde[0]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>}|>]
