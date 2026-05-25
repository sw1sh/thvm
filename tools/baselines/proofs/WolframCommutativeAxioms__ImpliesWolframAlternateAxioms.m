ProofObject["EquationalLogic", {ForAll[{\[FormalA], \[FormalB], \[FormalC]}, 
   (\[FormalA] \[CenterDot] ((\[FormalC] \[CenterDot] 
        \[FormalA]) \[CenterDot] \[FormalA])) \[CenterDot] 
     (\[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalA])) == 
    \[FormalC]]}, {ForAll[{\[FormalA], \[FormalB]}, 
   \[FormalA] \[CenterDot] \[FormalB] == \[FormalB] \[CenterDot] \[FormalA]], 
  ForAll[{\[FormalA], \[FormalB], \[FormalC]}, 
   (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
     (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalC])) == 
    \[FormalA]]}, <|"Variables" -> {\[FormalA], \[FormalB], \[FormalC], 
    \[FormalD], \[FormalE], \[FormalF], \[FormalG], \[FormalH]}, 
  "Constants" -> {}, "Proof" -> 
   {{"Axiom", 1} -> <|"Statement" -> HoldForm[\[FormalA] == 
         (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalC]))], 
      "Proof" -> <||>|>, {"Axiom", 2} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalB] \[CenterDot] \[FormalA]], "Proof" -> <||>|>, 
    {"Hypothesis", 1} -> <|"Statement" -> 
       HoldForm[(\[FormalF] \[CenterDot] ((\[FormalH] \[CenterDot] 
             \[FormalF]) \[CenterDot] \[FormalF])) \[CenterDot] 
          (\[FormalH] \[CenterDot] (\[FormalG] \[CenterDot] \[FormalF])) == 
         \[FormalH]], "Proof" -> <||>|>, {"CriticalPairLemma", 1} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           \[FormalA]) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) -> \[FormalA], "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          ((\[FormalB]_) \[CenterDot] (\[FormalC]_)), "MatchingConstruct" -> 
         {"Axiom", 1}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalB]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 1}, "Position" -> {}, 
        "Construct" -> {"Axiom", 2}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
           \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] \[FormalA])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 1}, "Position" -> {2}, 
        "Construct" -> {"Axiom", 2}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
           \[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
             (\[FormalA] \[CenterDot] \[FormalB]))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, {"SubstitutionLemma", 3} -> 
     <|"Statement" -> HoldForm[(\[FormalF] \[CenterDot] 
           ((\[FormalH] \[CenterDot] \[FormalF]) \[CenterDot] 
            \[FormalF])) \[CenterDot] (\[FormalH] \[CenterDot] 
           (\[FormalF] \[CenterDot] \[FormalG])) == \[FormalH]], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Position" -> {2, 2}, 
        "Construct" -> {"Axiom", 2}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalF] \[CenterDot] ((\[FormalH] \[CenterDot] 
               \[FormalF]) \[CenterDot] \[FormalF])) \[CenterDot] 
            (\[FormalH] \[CenterDot] (\[FormalF] \[CenterDot] \[FormalG])) == 
           \[FormalH]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 4} -> 
     <|"Statement" -> HoldForm[(\[FormalF] \[CenterDot] 
           ((\[FormalH] \[CenterDot] \[FormalF]) \[CenterDot] 
            \[FormalF])) \[CenterDot] ((\[FormalF] \[CenterDot] 
            \[FormalG]) \[CenterDot] \[FormalH]) == \[FormalH]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 3}, "Position" -> {2}, 
        "Construct" -> {"Axiom", 2}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalF] \[CenterDot] ((\[FormalH] \[CenterDot] 
               \[FormalF]) \[CenterDot] \[FormalF])) \[CenterDot] 
            ((\[FormalF] \[CenterDot] \[FormalG]) \[CenterDot] \[FormalH]) == 
           \[FormalH]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 5} -> 
     <|"Statement" -> HoldForm[(\[FormalF] \[CenterDot] 
           ((\[FormalF] \[CenterDot] \[FormalH]) \[CenterDot] 
            \[FormalF])) \[CenterDot] ((\[FormalF] \[CenterDot] 
            \[FormalG]) \[CenterDot] \[FormalH]) == \[FormalH]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 4}, 
        "Position" -> {1, 2, 1}, "Construct" -> {"Axiom", 2}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (\[FormalB]_) -> \[FormalB] \[CenterDot] \[FormalA], 
        "OutputExpression" -> HoldForm[(\[FormalF] \[CenterDot] 
             ((\[FormalF] \[CenterDot] \[FormalH]) \[CenterDot] 
              \[FormalF])) \[CenterDot] ((\[FormalF] \[CenterDot] 
              \[FormalG]) \[CenterDot] \[FormalH]) == \[FormalH]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 6} -> 
     <|"Statement" -> HoldForm[(\[FormalF] \[CenterDot] 
           (\[FormalF] \[CenterDot] (\[FormalF] \[CenterDot] 
             \[FormalH]))) \[CenterDot] ((\[FormalF] \[CenterDot] 
            \[FormalG]) \[CenterDot] \[FormalH]) == \[FormalH]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 5}, "Position" -> {1, 2}, 
        "Construct" -> {"Axiom", 2}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalF] \[CenterDot] (\[FormalF] \[CenterDot] 
              (\[FormalF] \[CenterDot] \[FormalH]))) \[CenterDot] 
            ((\[FormalF] \[CenterDot] \[FormalG]) \[CenterDot] \[FormalH]) == 
           \[FormalH]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 7} -> 
     <|"Statement" -> HoldForm[
        ((\[FormalF] \[CenterDot] \[FormalG]) \[CenterDot] 
           \[FormalH]) \[CenterDot] (\[FormalF] \[CenterDot] 
           (\[FormalF] \[CenterDot] (\[FormalF] \[CenterDot] \[FormalH]))) == 
         \[FormalH]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 6}, 
        "Position" -> {}, "Construct" -> {"Axiom", 2}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[((\[FormalF] \[CenterDot] \[FormalG]) \[CenterDot] 
             \[FormalH]) \[CenterDot] (\[FormalF] \[CenterDot] 
             (\[FormalF] \[CenterDot] (\[FormalF] \[CenterDot] 
               \[FormalH]))) == \[FormalH]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, {"SubstitutionLemma", 8} -> 
     <|"Statement" -> HoldForm[
        ((\[FormalF] \[CenterDot] \[FormalG]) \[CenterDot] 
           \[FormalH]) \[CenterDot] (\[FormalF] \[CenterDot] \[FormalH]) == 
         \[FormalH]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 7}, 
        "Position" -> {2}, "Construct" -> {"SubstitutionLemma", 2}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalA] \[CenterDot] \[FormalB], 
        "OutputExpression" -> HoldForm[
          ((\[FormalF] \[CenterDot] \[FormalG]) \[CenterDot] 
             \[FormalH]) \[CenterDot] (\[FormalF] \[CenterDot] \[FormalH]) == 
           \[FormalH]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 9} -> 
     <|"Statement" -> HoldForm[
        ((\[FormalF] \[CenterDot] \[FormalG]) \[CenterDot] 
           \[FormalH]) \[CenterDot] (\[FormalH] \[CenterDot] \[FormalF]) == 
         \[FormalH]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 8}, 
        "Position" -> {2}, "Construct" -> {"Axiom", 2}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[((\[FormalF] \[CenterDot] \[FormalG]) \[CenterDot] 
             \[FormalH]) \[CenterDot] (\[FormalH] \[CenterDot] \[FormalF]) == 
           \[FormalH]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 10} -> 
     <|"Statement" -> HoldForm[
        ((\[FormalG] \[CenterDot] \[FormalF]) \[CenterDot] 
           \[FormalH]) \[CenterDot] (\[FormalH] \[CenterDot] \[FormalF]) == 
         \[FormalH]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 9}, 
        "Position" -> {1, 1}, "Construct" -> {"Axiom", 2}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (\[FormalB]_) -> \[FormalB] \[CenterDot] \[FormalA], 
        "OutputExpression" -> HoldForm[
          ((\[FormalG] \[CenterDot] \[FormalF]) \[CenterDot] 
             \[FormalH]) \[CenterDot] (\[FormalH] \[CenterDot] \[FormalF]) == 
           \[FormalH]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 11} -> 
     <|"Statement" -> HoldForm[(\[FormalH] \[CenterDot] 
           (\[FormalG] \[CenterDot] \[FormalF])) \[CenterDot] 
          (\[FormalH] \[CenterDot] \[FormalF]) == \[FormalH]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 10}, "Position" -> {1}, 
        "Construct" -> {"Axiom", 2}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalH] \[CenterDot] (\[FormalG] \[CenterDot] 
              \[FormalF])) \[CenterDot] (\[FormalH] \[CenterDot] 
             \[FormalF]) == \[FormalH]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 12} -> 
     <|"Statement" -> HoldForm[(\[FormalH] \[CenterDot] 
           \[FormalF]) \[CenterDot] (\[FormalH] \[CenterDot] 
           (\[FormalG] \[CenterDot] \[FormalF])) == \[FormalH]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 11}, "Position" -> {}, 
        "Construct" -> {"Axiom", 2}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalH] \[CenterDot] \[FormalF]) \[CenterDot] 
            (\[FormalH] \[CenterDot] (\[FormalG] \[CenterDot] \[FormalF])) == 
           \[FormalH]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 13} -> 
     <|"Statement" -> HoldForm[(\[FormalH] \[CenterDot] 
           \[FormalF]) \[CenterDot] (\[FormalH] \[CenterDot] 
           (\[FormalF] \[CenterDot] \[FormalG])) == \[FormalH]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 12}, 
        "Position" -> {2, 2}, "Construct" -> {"Axiom", 2}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (\[FormalB]_) -> \[FormalB] \[CenterDot] \[FormalA], 
        "OutputExpression" -> HoldForm[(\[FormalH] \[CenterDot] 
             \[FormalF]) \[CenterDot] (\[FormalH] \[CenterDot] 
             (\[FormalF] \[CenterDot] \[FormalG])) == \[FormalH]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"Conclusion", 1} -> <|"Statement" -> HoldForm[\[FormalH] == \[FormalH]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 13}, "Position" -> {}, 
        "Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalH] == \[FormalH]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>}|>]
