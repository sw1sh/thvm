ProofObject["EquationalLogic", 
 {ForAll[{\[FormalA], \[FormalB], \[FormalC], \[FormalD]}, 
   \[FormalA] \[CircleTimes] OverBar[\[FormalB] \[CircleTimes] 
       (((\[FormalC] \[CircleTimes] OverBar[\[FormalC]]) \[CircleTimes] 
         OverBar[\[FormalD] \[CircleTimes] \[FormalB]]) \[CircleTimes] 
        \[FormalA])] == \[FormalD]]}, 
 {ForAll[{\[FormalA], \[FormalB], \[FormalC]}, 
   \[FormalA] \[CircleTimes] (\[FormalB] \[CircleTimes] \[FormalC]) == 
    (\[FormalA] \[CircleTimes] \[FormalB]) \[CircleTimes] \[FormalC]], 
  ForAll[{\[FormalA], \[FormalB]}, \[FormalA] \[CircleTimes] \[FormalB] == 
    \[FormalB] \[CircleTimes] \[FormalA]], ForAll[\[FormalA], 
   \[FormalA] \[CircleTimes] OverTilde[1] == \[FormalA]], 
  ForAll[\[FormalA], \[FormalA] \[CircleTimes] OverBar[\[FormalA]] == 
    OverTilde[1]]}, <|"Variables" -> {\[FormalA], \[FormalB], \[FormalC], 
    \[FormalD], \[FormalE], \[FormalF], \[FormalG], \[FormalH], \[FormalI], 
    \[FormalJ], \[FormalK]}, "Constants" -> {}, 
  "Proof" -> {{"Axiom", 1} -> <|"Statement" -> 
       HoldForm[\[FormalA] == \[FormalA] \[CircleTimes] OverTilde[1]], 
      "Proof" -> <||>|>, {"Axiom", 2} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] \[FormalB] == 
         \[FormalB] \[CircleTimes] \[FormalA]], "Proof" -> <||>|>, 
    {"Axiom", 3} -> <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          (\[FormalB] \[CircleTimes] \[FormalC]) == 
         (\[FormalA] \[CircleTimes] \[FormalB]) \[CircleTimes] \[FormalC]], 
      "Proof" -> <||>|>, {"Axiom", 4} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          OverBar[\[FormalA]] == OverTilde[1]], "Proof" -> <||>|>, 
    {"Hypothesis", 1} -> <|"Statement" -> 
       HoldForm[\[FormalH] \[CircleTimes] OverBar[\[FormalI] \[CircleTimes] 
            (((\[FormalJ] \[CircleTimes] OverBar[\[FormalJ]]) \[CircleTimes] 
              OverBar[\[FormalK] \[CircleTimes] \[FormalI]]) \[CircleTimes] 
             \[FormalH])] == \[FormalK]], "Proof" -> <||>|>, 
    {"CriticalPairLemma", 1} -> 
     <|"Statement" -> HoldForm[OverTilde[1] \[CircleTimes] \[FormalA] == 
         \[FormalA]], "Proof" -> <|"Construct" -> {"Axiom", 2}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CircleTimes] 
           (\[FormalB]_) <-> (\[FormalB]_) \[CircleTimes] (\[FormalA]_), 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CircleTimes] 
          (\[FormalB]_), "MatchingConstruct" -> {"Axiom", 1}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleTimes] OverTilde[1] -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"CriticalPairLemma", 2} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          (OverBar[\[FormalA]] \[CircleTimes] \[FormalB]) == 
         OverTilde[1] \[CircleTimes] \[FormalB]], 
      "Proof" -> <|"Construct" -> {"Axiom", 3}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CircleTimes] (\[FormalB]_)) \[CircleTimes] 
           (\[FormalC]_) -> \[FormalA] \[CircleTimes] 
           (\[FormalB] \[CircleTimes] \[FormalC]), "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CircleTimes] (\[FormalB]_), 
        "MatchingConstruct" -> {"Axiom", 4}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CircleTimes] 
           OverBar[\[FormalA]_] -> OverTilde[1], "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          (OverBar[\[FormalA]] \[CircleTimes] \[FormalB]) == \[FormalB]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 2}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 1}, "Orientation" -> 1, 
        "Rule" -> OverTilde[1] \[CircleTimes] (\[FormalA]_) -> \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CircleTimes] 
            (OverBar[\[FormalA]] \[CircleTimes] \[FormalB]) == \[FormalB]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 3} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[\[FormalA]]] == 
         \[FormalA] \[CircleTimes] OverTilde[1]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 1}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CircleTimes] 
           (OverBar[\[FormalA]_] \[CircleTimes] (\[FormalB]_)) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> OverBar[\[FormalA]_] \[CircleTimes] 
          (\[FormalB]_), "MatchingConstruct" -> {"Axiom", 4}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleTimes] OverBar[\[FormalA]_] -> OverTilde[1], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[\[FormalA]]] == \[FormalA]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 3}, "Position" -> {}, 
        "Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] OverTilde[1] -> \[FormalA], 
        "OutputExpression" -> HoldForm[OverBar[OverBar[\[FormalA]]] == 
           \[FormalA]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 4} -> 
     <|"Statement" -> HoldForm[\[FormalA] == \[FormalB] \[CircleTimes] 
          (\[FormalA] \[CircleTimes] OverBar[\[FormalB]])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 1}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CircleTimes] 
           (OverBar[\[FormalA]_] \[CircleTimes] (\[FormalB]_)) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> OverBar[\[FormalA]_] \[CircleTimes] 
          (\[FormalB]_), "MatchingConstruct" -> {"Axiom", 2}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleTimes] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CircleTimes] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 5} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         OverBar[\[FormalB]] \[CircleTimes] (\[FormalB] \[CircleTimes] 
           \[FormalA])], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 
          1}, "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CircleTimes] 
           (OverBar[\[FormalA]_] \[CircleTimes] (\[FormalB]_)) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> OverBar[\[FormalA]_], 
        "MatchingConstruct" -> {"SubstitutionLemma", 2}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         OverBar[OverBar[\[FormalA]_]] -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2, 1}|>|>, {"CriticalPairLemma", 6} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         OverBar[\[FormalB]] \[CircleTimes] (\[FormalA] \[CircleTimes] 
           \[FormalB])], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 
          4}, "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CircleTimes] 
           ((\[FormalB]_) \[CircleTimes] OverBar[\[FormalA]_]) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> OverBar[\[FormalA]_], 
        "MatchingConstruct" -> {"SubstitutionLemma", 2}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         OverBar[OverBar[\[FormalA]_]] -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 7} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA]] == 
         OverBar[\[FormalA] \[CircleTimes] \[FormalB]] \[CircleTimes] 
          \[FormalB]], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 6}, 
        "Orientation" -> -1, "Rule" -> OverBar[\[FormalA]_] \[CircleTimes] 
           ((\[FormalB]_) \[CircleTimes] (\[FormalA]_)) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CircleTimes] 
          (\[FormalA]_), "MatchingConstruct" -> {"CriticalPairLemma", 5}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         OverBar[\[FormalA]_] \[CircleTimes] ((\[FormalA]_) \[CircleTimes] 
            (\[FormalB]_)) -> \[FormalB], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 3} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA]] == 
         \[FormalB] \[CircleTimes] OverBar[\[FormalA] \[CircleTimes] 
            \[FormalB]]], "Proof" -> <|"Input" -> {"CriticalPairLemma", 7}, 
        "Position" -> {}, "Construct" -> {"Axiom", 2}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] (\[FormalB]_) -> 
          \[FormalB] \[CircleTimes] \[FormalA], "OutputExpression" -> 
         HoldForm[OverBar[\[FormalA]] == \[FormalB] \[CircleTimes] 
            OverBar[\[FormalA] \[CircleTimes] \[FormalB]]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 8} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA] \[CircleTimes] 
           OverBar[\[FormalB]]] == \[FormalB] \[CircleTimes] 
          OverBar[\[FormalA]]], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 1}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] 
           (OverBar[\[FormalA]_] \[CircleTimes] (\[FormalB]_)) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> OverBar[\[FormalA]_] \[CircleTimes] 
          (\[FormalB]_), "MatchingConstruct" -> {"SubstitutionLemma", 3}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleTimes] OverBar[(\[FormalB]_) \[CircleTimes] 
             (\[FormalA]_)] -> OverBar[\[FormalB]], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 4} -> 
     <|"Statement" -> HoldForm[\[FormalH] \[CircleTimes] 
          OverBar[(((\[FormalJ] \[CircleTimes] OverBar[
                \[FormalJ]]) \[CircleTimes] OverBar[\[FormalK] \[CircleTimes] 
                \[FormalI]]) \[CircleTimes] \[FormalH]) \[CircleTimes] 
            \[FormalI]] == \[FormalK]], "Proof" -> 
       <|"Input" -> {"Hypothesis", 1}, "Position" -> {2, 1}, 
        "Construct" -> {"Axiom", 2}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] (\[FormalB]_) -> 
          \[FormalB] \[CircleTimes] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalH] \[CircleTimes] OverBar[
             (((\[FormalJ] \[CircleTimes] OverBar[\[FormalJ]]) \[CircleTimes] 
                OverBar[\[FormalK] \[CircleTimes] \[FormalI]]) \[CircleTimes] 
               \[FormalH]) \[CircleTimes] \[FormalI]] == \[FormalK]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 5} -> 
     <|"Statement" -> HoldForm[\[FormalH] \[CircleTimes] 
          OverBar[((\[FormalJ] \[CircleTimes] OverBar[
               \[FormalJ]]) \[CircleTimes] OverBar[\[FormalK] \[CircleTimes] 
               \[FormalI]]) \[CircleTimes] (\[FormalH] \[CircleTimes] 
             \[FormalI])] == \[FormalK]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 4}, "Position" -> {2, 1}, 
        "Construct" -> {"Axiom", 3}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CircleTimes] (\[FormalB]_)) \[CircleTimes] 
           (\[FormalC]_) -> \[FormalA] \[CircleTimes] 
           (\[FormalB] \[CircleTimes] \[FormalC]), "OutputExpression" -> 
         HoldForm[\[FormalH] \[CircleTimes] OverBar[
             ((\[FormalJ] \[CircleTimes] OverBar[\[FormalJ]]) \[CircleTimes] 
               OverBar[\[FormalK] \[CircleTimes] \[FormalI]]) \[CircleTimes] 
              (\[FormalH] \[CircleTimes] \[FormalI])] == \[FormalK]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 6} -> 
     <|"Statement" -> HoldForm[\[FormalH] \[CircleTimes] 
          OverBar[(\[FormalJ] \[CircleTimes] OverBar[
              \[FormalJ]]) \[CircleTimes] (OverBar[\[FormalK] \[CircleTimes] 
               \[FormalI]] \[CircleTimes] (\[FormalH] \[CircleTimes] 
              \[FormalI]))] == \[FormalK]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 5}, "Position" -> {2, 1}, 
        "Construct" -> {"Axiom", 3}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CircleTimes] (\[FormalB]_)) \[CircleTimes] 
           (\[FormalC]_) -> \[FormalA] \[CircleTimes] 
           (\[FormalB] \[CircleTimes] \[FormalC]), "OutputExpression" -> 
         HoldForm[\[FormalH] \[CircleTimes] OverBar[
             (\[FormalJ] \[CircleTimes] OverBar[\[FormalJ]]) \[CircleTimes] 
              (OverBar[\[FormalK] \[CircleTimes] \[FormalI]] \[CircleTimes] (
                \[FormalH] \[CircleTimes] \[FormalI]))] == \[FormalK]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 7} -> 
     <|"Statement" -> HoldForm[\[FormalH] \[CircleTimes] 
          OverBar[\[FormalJ] \[CircleTimes] 
            (OverBar[\[FormalJ]] \[CircleTimes] 
             (OverBar[\[FormalK] \[CircleTimes] \[FormalI]] \[CircleTimes] 
              (\[FormalH] \[CircleTimes] \[FormalI])))] == \[FormalK]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 6}, "Position" -> {2, 1}, 
        "Construct" -> {"Axiom", 3}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CircleTimes] (\[FormalB]_)) \[CircleTimes] 
           (\[FormalC]_) -> \[FormalA] \[CircleTimes] 
           (\[FormalB] \[CircleTimes] \[FormalC]), "OutputExpression" -> 
         HoldForm[\[FormalH] \[CircleTimes] OverBar[\[FormalJ] \[CircleTimes] 
              (OverBar[\[FormalJ]] \[CircleTimes] (OverBar[
                 \[FormalK] \[CircleTimes] \[FormalI]] \[CircleTimes] 
                (\[FormalH] \[CircleTimes] \[FormalI])))] == \[FormalK]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 8} -> 
     <|"Statement" -> HoldForm[\[FormalH] \[CircleTimes] 
          OverBar[OverBar[\[FormalK] \[CircleTimes] 
              \[FormalI]] \[CircleTimes] (\[FormalH] \[CircleTimes] 
             \[FormalI])] == \[FormalK]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 7}, "Position" -> {2, 1}, 
        "Construct" -> {"SubstitutionLemma", 1}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] 
           (OverBar[\[FormalA]_] \[CircleTimes] (\[FormalB]_)) -> \[FormalB], 
        "OutputExpression" -> HoldForm[\[FormalH] \[CircleTimes] 
            OverBar[OverBar[\[FormalK] \[CircleTimes] 
                \[FormalI]] \[CircleTimes] (\[FormalH] \[CircleTimes] 
               \[FormalI])] == \[FormalK]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, {"SubstitutionLemma", 9} -> 
     <|"Statement" -> HoldForm[\[FormalH] \[CircleTimes] 
          OverBar[(\[FormalH] \[CircleTimes] \[FormalI]) \[CircleTimes] 
            OverBar[\[FormalK] \[CircleTimes] \[FormalI]]] == \[FormalK]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 8}, "Position" -> {2, 1}, 
        "Construct" -> {"Axiom", 2}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] (\[FormalB]_) -> 
          \[FormalB] \[CircleTimes] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalH] \[CircleTimes] OverBar[
             (\[FormalH] \[CircleTimes] \[FormalI]) \[CircleTimes] 
              OverBar[\[FormalK] \[CircleTimes] \[FormalI]]] == \[FormalK]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 10} -> 
     <|"Statement" -> HoldForm[\[FormalH] \[CircleTimes] 
          OverBar[\[FormalH] \[CircleTimes] (\[FormalI] \[CircleTimes] 
             OverBar[\[FormalK] \[CircleTimes] \[FormalI]])] == \[FormalK]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 9}, "Position" -> {2, 1}, 
        "Construct" -> {"Axiom", 3}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CircleTimes] (\[FormalB]_)) \[CircleTimes] 
           (\[FormalC]_) -> \[FormalA] \[CircleTimes] 
           (\[FormalB] \[CircleTimes] \[FormalC]), "OutputExpression" -> 
         HoldForm[\[FormalH] \[CircleTimes] OverBar[\[FormalH] \[CircleTimes] 
              (\[FormalI] \[CircleTimes] OverBar[\[FormalK] \[CircleTimes] 
                 \[FormalI]])] == \[FormalK]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 11} -> 
     <|"Statement" -> HoldForm[\[FormalH] \[CircleTimes] 
          OverBar[\[FormalH] \[CircleTimes] OverBar[\[FormalK]]] == 
         \[FormalK]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 10}, 
        "Position" -> {2, 1, 2}, "Construct" -> {"SubstitutionLemma", 3}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CircleTimes] 
           OverBar[(\[FormalB]_) \[CircleTimes] (\[FormalA]_)] -> 
          OverBar[\[FormalB]], "OutputExpression" -> 
         HoldForm[\[FormalH] \[CircleTimes] OverBar[\[FormalH] \[CircleTimes] 
              OverBar[\[FormalK]]] == \[FormalK]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 12} -> 
     <|"Statement" -> HoldForm[\[FormalH] \[CircleTimes] 
          (\[FormalK] \[CircleTimes] OverBar[\[FormalH]]) == \[FormalK]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 11}, "Position" -> {2}, 
        "Construct" -> {"CriticalPairLemma", 8}, "Orientation" -> 1, 
        "Rule" -> OverBar[(\[FormalA]_) \[CircleTimes] 
            OverBar[\[FormalB]_]] -> \[FormalB] \[CircleTimes] 
           OverBar[\[FormalA]], "OutputExpression" -> 
         HoldForm[\[FormalH] \[CircleTimes] (\[FormalK] \[CircleTimes] 
             OverBar[\[FormalH]]) == \[FormalK]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"Conclusion", 1} -> <|"Statement" -> HoldForm[\[FormalK] == \[FormalK]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 4}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] ((\[FormalB]_) \[CircleTimes] 
            OverBar[\[FormalA]_]) -> \[FormalB], "OutputExpression" -> 
         HoldForm[\[FormalK] == \[FormalK]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>}|>]
