ProofObject["EquationalLogic", {ForAll[{\[FormalA], \[FormalB], \[FormalC]}, 
   (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
     (((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
       (\[FormalC] \[CenterDot] \[FormalC])) \[CenterDot] 
      ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
       (\[FormalC] \[CenterDot] \[FormalC]))) == 
    (((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
       (\[FormalB] \[CenterDot] \[FormalB])) \[CenterDot] 
      ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
       (\[FormalB] \[CenterDot] \[FormalB]))) \[CenterDot] 
     (\[FormalC] \[CenterDot] \[FormalC])]}, 
 {ForAll[{\[FormalA], \[FormalB], \[FormalC]}, 
   (\[FormalA] \[CenterDot] ((\[FormalC] \[CenterDot] 
        \[FormalA]) \[CenterDot] \[FormalA])) \[CenterDot] 
     (\[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalA])) == 
    \[FormalC]]}, <|"Variables" -> {\[FormalA], \[FormalB], \[FormalC], 
    \[FormalD], \[FormalE], \[FormalF], \[FormalAlpha], \[FormalBeta]}, 
  "Constants" -> {}, "Proof" -> 
   {{"Axiom", 1} -> <|"Statement" -> HoldForm[\[FormalA] == 
         (\[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalB])) \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] \[FormalB]))], 
      "Proof" -> <||>|>, {"Hypothesis", 1} -> 
     <|"Statement" -> HoldForm[
        (((\[FormalD] \[CenterDot] \[FormalD]) \[CenterDot] 
            (\[FormalE] \[CenterDot] \[FormalE])) \[CenterDot] 
           ((\[FormalD] \[CenterDot] \[FormalD]) \[CenterDot] 
            (\[FormalE] \[CenterDot] \[FormalE]))) \[CenterDot] 
          (\[FormalF] \[CenterDot] \[FormalF]) == 
         (\[FormalD] \[CenterDot] \[FormalD]) \[CenterDot] 
          (((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
            (\[FormalF] \[CenterDot] \[FormalF])) \[CenterDot] 
           ((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
            (\[FormalF] \[CenterDot] \[FormalF])))], "Proof" -> <||>|>, 
    {"CriticalPairLemma", 1} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalA]) == 
         ((\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
             \[FormalA])) \[CenterDot] (\[FormalB] \[CenterDot] 
            (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
              \[FormalA])))) \[CenterDot] ((\[FormalA] \[CenterDot] 
            ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
             \[FormalA])) \[CenterDot] (\[FormalAlpha] \[CenterDot] 
            (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
              \[FormalA]))))], "Proof" -> <|"Construct" -> {"Axiom", 1}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (((\[FormalB]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
             (\[FormalA]_))) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalA]_))) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          (\[FormalA]_), "MatchingConstruct" -> {"Axiom", 1}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalB], "MatchingSide" -> 1, 
        "Position" -> {1, 2, 1}|>|>, {"CriticalPairLemma", 2} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
             \[FormalAlpha])) \[CenterDot] ((\[FormalA] \[CenterDot] 
             (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
               \[FormalAlpha]))) \[CenterDot] (\[FormalB] \[CenterDot] 
             (\[FormalC] \[CenterDot] \[FormalAlpha])))) \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalB], "Side" -> 1, 
        "Subpattern" -> (\[FormalC]_) \[CenterDot] (\[FormalA]_), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (((\[FormalB]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
             (\[FormalA]_))) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalA]_))) -> \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"CriticalPairLemma", 3} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CenterDot] 
           ((\[FormalA] \[CenterDot] ((\[FormalC] \[CenterDot] (
                (\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
                \[FormalC])) \[CenterDot] (\[FormalB] \[CenterDot] (
                \[FormalAlpha] \[CenterDot] \[FormalC])))) \[CenterDot] 
            ((\[FormalC] \[CenterDot] ((\[FormalB] \[CenterDot] 
                \[FormalC]) \[CenterDot] \[FormalC])) \[CenterDot] 
             (\[FormalB] \[CenterDot] (\[FormalAlpha] \[CenterDot] 
               \[FormalC]))))) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalC] \[CenterDot] ((\[FormalB] \[CenterDot] 
              \[FormalC]) \[CenterDot] \[FormalC])))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 2}, 
        "Orientation" -> -1, "Rule" -> 
         (((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
              (\[FormalC]_))) \[CenterDot] (((\[FormalAlpha]_) \[CenterDot] 
              ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
                (\[FormalC]_)))) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              ((\[FormalB]_) \[CenterDot] (\[FormalC]_))))) \[CenterDot] 
           ((\[FormalAlpha]_) \[CenterDot] (\[FormalA]_)) -> \[FormalAlpha], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          ((\[FormalB]_) \[CenterDot] (\[FormalC]_)), "MatchingConstruct" -> 
         {"Axiom", 1}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalB], "MatchingSide" -> 1, 
        "Position" -> {1, 1}|>|>, {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            ((\[FormalC] \[CenterDot] ((\[FormalB] \[CenterDot] 
                \[FormalC]) \[CenterDot] \[FormalC])) \[CenterDot] 
             (\[FormalB] \[CenterDot] (\[FormalAlpha] \[CenterDot] 
               \[FormalC]))))) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalC] \[CenterDot] ((\[FormalB] \[CenterDot] 
              \[FormalC]) \[CenterDot] \[FormalC])))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 3}, 
        "Position" -> {1, 2, 1, 2}, "Construct" -> {"Axiom", 1}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (((\[FormalB]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
             (\[FormalA]_))) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalA]_))) -> \[FormalB], 
        "OutputExpression" -> HoldForm[\[FormalA] == 
           (\[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalB]) \[CenterDot] ((\[FormalC] \[CenterDot] 
                ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
                 \[FormalC])) \[CenterDot] (\[FormalB] \[CenterDot] 
                (\[FormalAlpha] \[CenterDot] \[FormalC]))))) \[CenterDot] 
            (\[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] 
              ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
               \[FormalC])))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalB])) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalC] \[CenterDot] ((\[FormalB] \[CenterDot] 
              \[FormalC]) \[CenterDot] \[FormalC])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 1}, 
        "Position" -> {1, 2, 2}, "Construct" -> {"Axiom", 1}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (((\[FormalB]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
             (\[FormalA]_))) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalA]_))) -> \[FormalB], 
        "OutputExpression" -> HoldForm[\[FormalA] == 
           (\[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalB])) \[CenterDot] 
            (\[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] 
              ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
               \[FormalC])))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 4} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           ((\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
              ((\[FormalAlpha] \[CenterDot] ((\[FormalC] \[CenterDot] 
                  \[FormalAlpha]) \[CenterDot] \[FormalAlpha])) \[CenterDot] (
                \[FormalC] \[CenterDot] (\[FormalBeta] \[CenterDot] 
                 \[FormalAlpha]))))) \[CenterDot] (\[FormalB] \[CenterDot] 
             ((\[FormalAlpha] \[CenterDot] ((\[FormalC] \[CenterDot] 
                 \[FormalAlpha]) \[CenterDot] \[FormalAlpha])) \[CenterDot] 
              (\[FormalC] \[CenterDot] (\[FormalBeta] \[CenterDot] 
                \[FormalAlpha])))))) \[CenterDot] (\[FormalA] \[CenterDot] 
           \[FormalB])], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 
          2}, "Orientation" -> -1, "Rule" -> 
         (((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
              (\[FormalC]_))) \[CenterDot] (((\[FormalAlpha]_) \[CenterDot] 
              ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
                (\[FormalC]_)))) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              ((\[FormalB]_) \[CenterDot] (\[FormalC]_))))) \[CenterDot] 
           ((\[FormalAlpha]_) \[CenterDot] (\[FormalA]_)) -> \[FormalAlpha], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          (\[FormalC]_), "MatchingConstruct" -> {"Axiom", 1}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalB], "MatchingSide" -> 1, 
        "Position" -> {1, 1, 2}|>|>, {"SubstitutionLemma", 3} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           ((\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
              \[FormalC])) \[CenterDot] (\[FormalB] \[CenterDot] 
             ((\[FormalAlpha] \[CenterDot] ((\[FormalC] \[CenterDot] 
                 \[FormalAlpha]) \[CenterDot] \[FormalAlpha])) \[CenterDot] 
              (\[FormalC] \[CenterDot] (\[FormalBeta] \[CenterDot] 
                \[FormalAlpha])))))) \[CenterDot] (\[FormalA] \[CenterDot] 
           \[FormalB])], "Proof" -> <|"Input" -> {"CriticalPairLemma", 4}, 
        "Position" -> {1, 2, 1, 2, 2}, "Construct" -> {"Axiom", 1}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (((\[FormalB]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
             (\[FormalA]_))) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalA]_))) -> \[FormalB], 
        "OutputExpression" -> HoldForm[\[FormalA] == 
           ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
             ((\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
                \[FormalC])) \[CenterDot] (\[FormalB] \[CenterDot] (
                (\[FormalAlpha] \[CenterDot] ((\[FormalC] \[CenterDot] 
                   \[FormalAlpha]) \[CenterDot] \[FormalAlpha])) \[CenterDot] 
                (\[FormalC] \[CenterDot] (\[FormalBeta] \[CenterDot] 
                  \[FormalAlpha])))))) \[CenterDot] (\[FormalA] \[CenterDot] 
             \[FormalB])], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 4} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           ((\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
              \[FormalC])) \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalC]))) \[CenterDot] (\[FormalA] \[CenterDot] 
           \[FormalB])], "Proof" -> <|"Input" -> {"SubstitutionLemma", 3}, 
        "Position" -> {1, 2, 2, 2}, "Construct" -> {"Axiom", 1}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (((\[FormalB]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
             (\[FormalA]_))) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalA]_))) -> \[FormalB], 
        "OutputExpression" -> HoldForm[\[FormalA] == 
           ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
             ((\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
                \[FormalC])) \[CenterDot] (\[FormalB] \[CenterDot] 
               \[FormalC]))) \[CenterDot] (\[FormalA] \[CenterDot] 
             \[FormalB])], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 5} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalA]) == 
         (\[FormalB] \[CenterDot] (((\[FormalA] \[CenterDot] 
              ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
               \[FormalA])) \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalB])) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 2}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (((\[FormalB]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
             (\[FormalA]_))) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] (
                \[FormalC]_)) \[CenterDot] (\[FormalC]_)))) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          ((\[FormalC]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] (\[FormalC]_))), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (((\[FormalB]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
             (\[FormalA]_))) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalA]_))) -> \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 6} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalA]) == 
         ((\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalA])) \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
              \[FormalA])))) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 5}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((((\[FormalB]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
                 (\[FormalB]_)) \[CenterDot] (\[FormalB]_))) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
           (\[FormalB]_) -> \[FormalB] \[CenterDot] 
           ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalB]), 
        "Side" -> 1, "Subpattern" -> ((\[FormalB]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalB]_))) \[CenterDot] (\[FormalA]_), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (((\[FormalB]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
             (\[FormalA]_))) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalA]_))) -> \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {1, 2, 1}|>|>, 
    {"CriticalPairLemma", 7} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalA]) == 
         \[FormalA] \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 6}, 
        "Orientation" -> -1, "Rule" -> 
         (((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
              (\[FormalA]_))) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] (
                \[FormalA]_))))) \[CenterDot] (\[FormalA]_) -> 
          \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalA]), "Side" -> 1, 
        "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalA]_)))), 
        "MatchingConstruct" -> {"SubstitutionLemma", 2}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (((\[FormalA]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
              (\[FormalC]_)))) -> \[FormalB], "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 5} -> 
     <|"Statement" -> HoldForm[(((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             ((\[FormalB]_) \[CenterDot] (\[FormalA]_))))) \[CenterDot] 
          (\[FormalA]_) -> \[FormalA] \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 6}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 7}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] (\[FormalA]_)) -> 
          \[FormalA] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] (
                \[FormalA]_))) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
                (\[FormalA]_))))) \[CenterDot] (\[FormalA]_) -> 
           \[FormalA] \[CenterDot] \[FormalA]], "ConstructSide" -> 1, 
        "InputOrientation" -> -1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 8} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalB], "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (((\[FormalB]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           (\[FormalA]_)), "MatchingConstruct" -> {"CriticalPairLemma", 7}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] (\[FormalA]_)) -> 
          \[FormalA] \[CenterDot] \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 9} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
            \[FormalA])) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalB], "Side" -> 1, 
        "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          ((\[FormalC]_) \[CenterDot] (\[FormalA]_)), "MatchingConstruct" -> 
         {"CriticalPairLemma", 7}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA] \[CenterDot] \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 6} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 9}, "Position" -> {1}, 
        "Construct" -> {"CriticalPairLemma", 7}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] (\[FormalA]_)) -> 
          \[FormalA] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
             \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalA])], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 7} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           (((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalA])) \[CenterDot] \[FormalB] == 
         \[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
            \[FormalB]) \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 5}, 
        "Position" -> {1, 2, 1, 1}, "Construct" -> {"CriticalPairLemma", 7}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA] \[CenterDot] \[FormalA], 
        "OutputExpression" -> HoldForm[(\[FormalA] \[CenterDot] 
             (((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
               \[FormalA]) \[CenterDot] \[FormalA])) \[CenterDot] 
            \[FormalB] == \[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
              \[FormalB]) \[CenterDot] \[FormalB])], "ConstructSide" -> 1, 
        "InputOrientation" -> -1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 8} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           (((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalA])) \[CenterDot] \[FormalB] == 
         \[FormalB] \[CenterDot] \[FormalB]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 7}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 7}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] (\[FormalA]_)) -> 
          \[FormalA] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] (((\[FormalB] \[CenterDot] 
                \[FormalB]) \[CenterDot] \[FormalA]) \[CenterDot] 
              \[FormalA])) \[CenterDot] \[FormalB] == \[FormalB] \[CenterDot] 
            \[FormalB]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 10} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalA]) \[CenterDot] (\[FormalB] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalA])))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 8}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalA]_))) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalA]_), "MatchingConstruct" -> {"SubstitutionLemma", 6}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 11} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         ((\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalA])) \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
              \[FormalA])))) \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalA]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 4}, 
        "Orientation" -> -1, "Rule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (((\[FormalC]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] (
                \[FormalB]_))) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (\[FormalB]_)))) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalC], "Side" -> 1, 
        "Subpattern" -> (\[FormalC]_) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] (\[FormalB]_)), "MatchingConstruct" -> 
         {"CriticalPairLemma", 8}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalA]_))) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {1, 2, 1}|>|>, 
    {"CriticalPairLemma", 12} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalA]) == 
         (\[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalB])) \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 8}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((((\[FormalB]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
           (\[FormalB]_) -> \[FormalB] \[CenterDot] \[FormalB], "Side" -> 1, 
        "Subpattern" -> (\[FormalB]_) \[CenterDot] (\[FormalB]_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 6}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {1, 2, 1, 1}|>|>, 
    {"SubstitutionLemma", 9} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalB])) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 12}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 6}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalA] == 
           (\[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalB])) \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalA])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 13} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalA]) == 
         ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
           (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalB]))) \[CenterDot] ((\[FormalA] \[CenterDot] 
            ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
             \[FormalA])) \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalB])))], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalB], "Side" -> 1, 
        "Subpattern" -> (\[FormalB]_) \[CenterDot] (\[FormalA]_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 9}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {1, 2, 1}|>|>, 
    {"SubstitutionLemma", 10} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalA]) == 
         \[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
            ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
             \[FormalA])) \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalB])))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 13}, "Position" -> {1}, 
        "Construct" -> {"CriticalPairLemma", 8}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
              \[FormalA]) \[CenterDot] \[FormalA]) == \[FormalB] \[CenterDot] 
            ((\[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
                \[FormalA]) \[CenterDot] \[FormalA])) \[CenterDot] 
             (\[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] 
               \[FormalB])))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 14} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalA]) == 
         ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
           (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalB]))) \[CenterDot] ((\[FormalA] \[CenterDot] 
            ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
             \[FormalA])) \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 4}, 
        "Orientation" -> -1, "Rule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (((\[FormalC]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] (
                \[FormalB]_))) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (\[FormalB]_)))) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalC], "Side" -> 1, 
        "Subpattern" -> (\[FormalC]_) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] (\[FormalB]_)), "MatchingConstruct" -> 
         {"SubstitutionLemma", 9}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (((\[FormalB]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
             (\[FormalA]_))) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)) -> \[FormalB], "MatchingSide" -> 1, 
        "Position" -> {1, 2, 1}|>|>, {"SubstitutionLemma", 11} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalA]) == 
         \[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
            ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
             \[FormalA])) \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 14}, "Position" -> {1}, 
        "Construct" -> {"CriticalPairLemma", 8}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
              \[FormalA]) \[CenterDot] \[FormalA]) == \[FormalB] \[CenterDot] 
            ((\[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
                \[FormalA]) \[CenterDot] \[FormalA])) \[CenterDot] 
             \[FormalB])], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 15} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalB] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 8}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalA]_))) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          ((\[FormalB]_) \[CenterDot] (\[FormalA]_)), "MatchingConstruct" -> 
         {"SubstitutionLemma", 11}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] (
                \[FormalB]_)) \[CenterDot] (\[FormalB]_))) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalB] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalB]), 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 16} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
           (((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 10}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (\[FormalA]_)))) -> \[FormalA] \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           (\[FormalA]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalA]_))), 
        "MatchingConstruct" -> {"SubstitutionLemma", 11}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
              (\[FormalB]_))) \[CenterDot] (\[FormalA]_)) -> 
          \[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalB]), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 17} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalA])) \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalA]))) == 
         (\[FormalA] \[CenterDot] ((((\[FormalA] \[CenterDot] (
                \[FormalB] \[CenterDot] \[FormalA])) \[CenterDot] 
              (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
                (\[FormalB] \[CenterDot] \[FormalA])))) \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalA])) \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalB], "Side" -> 1, 
        "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          ((\[FormalC]_) \[CenterDot] (\[FormalA]_)), "MatchingConstruct" -> 
         {"CriticalPairLemma", 11}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (((\[FormalA]_) \[CenterDot] 
             ((\[FormalB]_) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              ((\[FormalB]_) \[CenterDot] (\[FormalA]_))))) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA] \[CenterDot] \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 12} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalA])) \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalA]))) == 
         (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalA])) \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 17}, 
        "Position" -> {1, 2, 1}, "Construct" -> {"SubstitutionLemma", 5}, 
        "Orientation" -> 1, "Rule" -> 
         (((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
              (\[FormalA]_))) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] (
                \[FormalA]_))))) \[CenterDot] (\[FormalA]_) -> 
          \[FormalA] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
              \[FormalA])) \[CenterDot] (\[FormalA] \[CenterDot] 
             (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
               \[FormalA]))) == (\[FormalA] \[CenterDot] 
             ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
              \[FormalA])) \[CenterDot] (\[FormalA] \[CenterDot] 
             \[FormalA])], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 13} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalA])) \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalA]))) == 
         (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 12}, "Position" -> {1}, 
        "Construct" -> {"CriticalPairLemma", 7}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] (\[FormalA]_)) -> 
          \[FormalA] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
              \[FormalA])) \[CenterDot] (\[FormalA] \[CenterDot] 
             (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
               \[FormalA]))) == (\[FormalA] \[CenterDot] 
             \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalA])], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 14} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalA])) \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalA]))) == \[FormalA]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 13}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 6}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "OutputExpression" -> HoldForm[(\[FormalA] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalA])) \[CenterDot] 
            (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
              (\[FormalB] \[CenterDot] \[FormalA]))) == \[FormalA]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 18} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalA]) == \[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalA])) \[CenterDot] ((\[FormalA] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalA])) \[CenterDot] 
            (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
              (\[FormalB] \[CenterDot] \[FormalA])))))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 14}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             ((\[FormalB]_) \[CenterDot] (\[FormalA]_)))) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          ((\[FormalB]_) \[CenterDot] (\[FormalA]_)), "MatchingConstruct" -> 
         {"SubstitutionLemma", 14}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             ((\[FormalB]_) \[CenterDot] (\[FormalA]_)))) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"SubstitutionLemma", 15} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalA]) == \[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalA])) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 18}, 
        "Position" -> {2, 2}, "Construct" -> {"SubstitutionLemma", 14}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             ((\[FormalB]_) \[CenterDot] (\[FormalA]_)))) -> \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalA]) == \[FormalA] \[CenterDot] 
            ((\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
               \[FormalA])) \[CenterDot] \[FormalA])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 19} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CenterDot] 
           ((\[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] (
                \[FormalC] \[CenterDot] \[FormalB])) \[CenterDot] 
              (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
                (\[FormalC] \[CenterDot] \[FormalB]))))) \[CenterDot] 
            ((\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
               \[FormalB])) \[CenterDot] (\[FormalB] \[CenterDot] 
              (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
                \[FormalB])))))) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] \[FormalB])))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 4}, 
        "Orientation" -> -1, "Rule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (((\[FormalC]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] (
                \[FormalB]_))) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (\[FormalB]_)))) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalC], "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 14}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalA]_))) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
              (\[FormalA]_)))) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {1, 1}|>|>, {"SubstitutionLemma", 16} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            ((\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
               \[FormalB])) \[CenterDot] (\[FormalB] \[CenterDot] 
              (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
                \[FormalB])))))) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] \[FormalB])))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 19}, 
        "Position" -> {1, 2, 1, 2}, "Construct" -> {"SubstitutionLemma", 14}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             ((\[FormalB]_) \[CenterDot] (\[FormalA]_)))) -> \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalA] == 
           (\[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalB]) \[CenterDot] ((\[FormalB] \[CenterDot] 
                (\[FormalC] \[CenterDot] \[FormalB])) \[CenterDot] (
                \[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
                 (\[FormalC] \[CenterDot] \[FormalB])))))) \[CenterDot] 
            (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
              (\[FormalC] \[CenterDot] \[FormalB])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 17} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalB])) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] \[FormalB])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 16}, 
        "Position" -> {1, 2, 2}, "Construct" -> {"SubstitutionLemma", 14}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             ((\[FormalB]_) \[CenterDot] (\[FormalA]_)))) -> \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalA] == 
           (\[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalB])) \[CenterDot] 
            (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
              (\[FormalC] \[CenterDot] \[FormalB])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 20} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
            \[FormalA]) \[CenterDot] \[FormalA]) == \[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 15}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
              (\[FormalA]_))) \[CenterDot] (\[FormalA]_)) -> 
          \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalA]), 
        "Side" -> 1, "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
          (\[FormalA]_), "MatchingConstruct" -> {"SubstitutionLemma", 8}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] ((((\[FormalB]_) \[CenterDot] (
                \[FormalB]_)) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
             (\[FormalA]_))) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalB], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 21} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalA])) \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
            (((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
              \[FormalA]) \[CenterDot] \[FormalA])))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 14}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             ((\[FormalB]_) \[CenterDot] (\[FormalA]_)))) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          ((\[FormalB]_) \[CenterDot] (\[FormalA]_)), "MatchingConstruct" -> 
         {"CriticalPairLemma", 20}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           ((((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] (\[FormalA]_)) -> 
          \[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalA]), 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"SubstitutionLemma", 18} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalA])) \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 21}, "Position" -> {2}, 
        "Construct" -> {"CriticalPairLemma", 16}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
              (\[FormalB]_)) \[CenterDot] (\[FormalB]_))) -> 
          \[FormalA] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
             (\[FormalA] \[CenterDot] \[FormalA])) \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalA])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 22} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalA])) \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 15}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalA], "Side" -> 1, 
        "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalB]_)), "MatchingConstruct" -> {"CriticalPairLemma", 20}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((((\[FormalA]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA] \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalA]), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 19} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         \[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 22}, "Position" -> {1}, 
        "Construct" -> {"SubstitutionLemma", 6}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
           \[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
             (\[FormalA] \[CenterDot] \[FormalA]))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 23} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
            \[FormalA])) \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 9}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (((\[FormalB]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
             (\[FormalA]_))) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)) -> \[FormalB], "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (((\[FormalB]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           (\[FormalA]_)), "MatchingConstruct" -> {"CriticalPairLemma", 20}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((((\[FormalA]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA] \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalA]), "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 20} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
            \[FormalA])) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 23}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 6}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
           (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
              \[FormalA])) \[CenterDot] \[FormalA]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 24} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalA]) == 
         (\[FormalA] \[CenterDot] (((\[FormalA] \[CenterDot] 
              (\[FormalA] \[CenterDot] \[FormalA])) \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalA])) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalB], "Side" -> 1, 
        "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          ((\[FormalC]_) \[CenterDot] (\[FormalA]_)), "MatchingConstruct" -> 
         {"SubstitutionLemma", 18}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 21} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalA]) == 
         (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalA])) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 24}, 
        "Position" -> {1, 2, 1}, "Construct" -> {"SubstitutionLemma", 20}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
           (\[FormalA]_) -> \[FormalA] \[CenterDot] \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalA]) == 
           (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalA]) \[CenterDot] \[FormalA])) \[CenterDot] 
            \[FormalA]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 22} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalA]) == 
         (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 21}, "Position" -> {1}, 
        "Construct" -> {"CriticalPairLemma", 7}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] (\[FormalA]_)) -> 
          \[FormalA] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
             \[FormalA]) == (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
            \[FormalA]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 25} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalA]) == 
         ((\[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
              \[FormalA]) \[CenterDot] \[FormalA])) \[CenterDot] 
           (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
             ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
              \[FormalA])))) \[CenterDot] ((\[FormalA] \[CenterDot] 
            ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
             \[FormalA])) \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 1}, 
        "Orientation" -> -1, "Rule" -> 
         (((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
              (\[FormalC]_))) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] (
                \[FormalC]_))))) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             (((\[FormalA]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
              (\[FormalC]_))) \[CenterDot] ((\[FormalAlpha]_) \[CenterDot] 
             ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] (
                \[FormalC]_))))) -> \[FormalC] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalC]) \[CenterDot] \[FormalC]), 
        "Side" -> 1, "Subpattern" -> (\[FormalAlpha]_) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalC]_))), "MatchingConstruct" -> {"CriticalPairLemma", 
          15}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
              (\[FormalB]_)) \[CenterDot] (\[FormalB]_))) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"SubstitutionLemma", 23} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalA])) == 
         ((\[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
              \[FormalA]) \[CenterDot] \[FormalA])) \[CenterDot] 
           (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
             ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
              \[FormalA])))) \[CenterDot] ((\[FormalA] \[CenterDot] 
            ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
             \[FormalA])) \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 25}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           (\[FormalA]_) -> \[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
            \[FormalA]), "OutputExpression" -> HoldForm[
          \[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
             (\[FormalA] \[CenterDot] \[FormalA])) == 
           ((\[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
                \[FormalA]) \[CenterDot] \[FormalA])) \[CenterDot] 
             (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] (
                (\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
                \[FormalA])))) \[CenterDot] ((\[FormalA] \[CenterDot] 
              ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
               \[FormalA])) \[CenterDot] \[FormalB])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 24} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         ((\[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
              \[FormalA]) \[CenterDot] \[FormalA])) \[CenterDot] 
           (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
             ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
              \[FormalA])))) \[CenterDot] ((\[FormalA] \[CenterDot] 
            ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
             \[FormalA])) \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 23}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 19}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalA]_))) -> 
          \[FormalA] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
           ((\[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
                \[FormalA]) \[CenterDot] \[FormalA])) \[CenterDot] 
             (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] (
                (\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
                \[FormalA])))) \[CenterDot] ((\[FormalA] \[CenterDot] 
              ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
               \[FormalA])) \[CenterDot] \[FormalB])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 25} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
            ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
             \[FormalA])) \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 24}, "Position" -> {1}, 
        "Construct" -> {"SubstitutionLemma", 14}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalA]_))) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
              (\[FormalA]_)))) -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
           \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
               \[FormalA])) \[CenterDot] \[FormalB])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 26} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
            (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
              \[FormalA]))) \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 25}, 
        "Position" -> {2, 1, 2}, "Construct" -> {"SubstitutionLemma", 22}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] (\[FormalA]_) -> 
          \[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalA]), 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
           \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
                \[FormalA]))) \[CenterDot] \[FormalB])], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 27} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalA]) \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 26}, 
        "Position" -> {2, 1}, "Construct" -> {"SubstitutionLemma", 19}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalA] \[CenterDot] \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
           \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalA]) \[CenterDot] \[FormalB])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 26} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalA]) == 
         (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 27}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
            (\[FormalB]_)) -> \[FormalA] \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalA]_), "MatchingConstruct" -> {"SubstitutionLemma", 6}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"SubstitutionLemma", 28} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 26}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 6}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalA] == 
           (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 27} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         ((((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
             (\[FormalA] \[CenterDot] \[FormalA])) \[CenterDot] 
            \[FormalB]) \[CenterDot] (((\[FormalA] \[CenterDot] 
              \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
              \[FormalA])) \[CenterDot] (((\[FormalA] \[CenterDot] 
               \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
               \[FormalA])) \[CenterDot] \[FormalB]))) \[CenterDot] 
          \[FormalA]], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 8}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((((\[FormalB]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
           (\[FormalB]_) -> \[FormalB] \[CenterDot] \[FormalB], "Side" -> 1, 
        "Subpattern" -> ((\[FormalB]_) \[CenterDot] 
           (\[FormalB]_)) \[CenterDot] (\[FormalA]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 27}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
            (\[FormalB]_)) -> \[FormalA] \[CenterDot] \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {1, 2, 1}|>|>, 
    {"SubstitutionLemma", 29} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           (((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
             (\[FormalA] \[CenterDot] \[FormalA])) \[CenterDot] 
            (((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
              (\[FormalA] \[CenterDot] \[FormalA])) \[CenterDot] 
             \[FormalB]))) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 27}, 
        "Position" -> {1, 1, 1}, "Construct" -> {"SubstitutionLemma", 6}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
           ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
             (((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] (
                \[FormalA] \[CenterDot] \[FormalA])) \[CenterDot] 
              (((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
                (\[FormalA] \[CenterDot] \[FormalA])) \[CenterDot] 
               \[FormalB]))) \[CenterDot] \[FormalA]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 30} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           (\[FormalA] \[CenterDot] (((\[FormalA] \[CenterDot] 
               \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
               \[FormalA])) \[CenterDot] \[FormalB]))) \[CenterDot] 
          \[FormalA]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 29}, 
        "Position" -> {1, 2, 1}, "Construct" -> {"SubstitutionLemma", 6}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
           ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
             (\[FormalA] \[CenterDot] (((\[FormalA] \[CenterDot] 
                 \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
                 \[FormalA])) \[CenterDot] \[FormalB]))) \[CenterDot] 
            \[FormalA]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 31} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
             \[FormalB]))) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 30}, 
        "Position" -> {1, 2, 2, 1}, "Construct" -> {"SubstitutionLemma", 6}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
           ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
             (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
               \[FormalB]))) \[CenterDot] \[FormalA]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 28} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
             \[FormalB]))) \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalA]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 4}, 
        "Orientation" -> -1, "Rule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (((\[FormalC]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] (
                \[FormalB]_))) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (\[FormalB]_)))) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalC], "Side" -> 1, 
        "Subpattern" -> (\[FormalC]_) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] (\[FormalB]_)), "MatchingConstruct" -> 
         {"SubstitutionLemma", 28}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {1, 2, 1}|>|>, {"SubstitutionLemma", 32} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
             \[FormalB]))) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 28}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           (\[FormalA]_) -> \[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
            \[FormalA]), "OutputExpression" -> HoldForm[
          \[FormalA] \[CenterDot] \[FormalA] == 
           ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
             (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
               \[FormalB]))) \[CenterDot] (\[FormalA] \[CenterDot] 
             (\[FormalA] \[CenterDot] \[FormalA]))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 29} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB])) == 
         (\[FormalA] \[CenterDot] ((((\[FormalA] \[CenterDot] 
               \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] (
                \[FormalA] \[CenterDot] \[FormalB]))) \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalA])) \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 17}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (((\[FormalB]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
             (\[FormalA]_))) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
              (\[FormalA]_)))) -> \[FormalB], "Side" -> 1, 
        "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
            (\[FormalA]_))), "MatchingConstruct" -> {"SubstitutionLemma", 
          32}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (\[FormalB]_)))) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalA]_))) -> 
          \[FormalA] \[CenterDot] \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 33} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB])) == 
         (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalA])) \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 29}, 
        "Position" -> {1, 2, 1}, "Construct" -> {"SubstitutionLemma", 31}, 
        "Orientation" -> -1, "Rule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (\[FormalB]_)))) \[CenterDot] (\[FormalA]_) -> 
          \[FormalA] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB])) == 
           (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalA]) \[CenterDot] \[FormalA])) \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalA])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 34} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB])) == 
         (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalA]))) \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 33}, 
        "Position" -> {1, 2}, "Construct" -> {"SubstitutionLemma", 22}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] (\[FormalA]_) -> 
          \[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalA]), 
        "OutputExpression" -> HoldForm[(\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
             (\[FormalA] \[CenterDot] \[FormalB])) == 
           (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
              (\[FormalA] \[CenterDot] \[FormalA]))) \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalA])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 35} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB])) == 
         (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 34}, "Position" -> {1}, 
        "Construct" -> {"SubstitutionLemma", 19}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalA]_))) -> 
          \[FormalA] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB])) == 
           (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalA])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 36} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB])) == \[FormalA]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 35}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 28}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) -> \[FormalA], 
        "OutputExpression" -> HoldForm[(\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
             (\[FormalA] \[CenterDot] \[FormalB])) == \[FormalA]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 30} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalB]) \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
             (\[FormalA] \[CenterDot] \[FormalB]))))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 36}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalB]_))) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"SubstitutionLemma", 36}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 37} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalB]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 30}, 
        "Position" -> {2, 2}, "Construct" -> {"SubstitutionLemma", 36}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalB]_))) -> \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
           \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] \[FormalA])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 31} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         (((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalA]) \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] \[FormalA]))) \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 9}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (((\[FormalB]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
             (\[FormalA]_))) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)) -> \[FormalB], "Side" -> 1, 
        "Subpattern" -> (\[FormalB]_) \[CenterDot] (\[FormalA]_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 37}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalA]_)) -> 
          \[FormalA] \[CenterDot] \[FormalB], "MatchingSide" -> 1, 
        "Position" -> {1, 2, 1}|>|>, {"SubstitutionLemma", 38} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 31}, "Position" -> {1}, 
        "Construct" -> {"SubstitutionLemma", 36}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalA])], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 32} -> 
     <|"Statement" -> HoldForm[
        ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           \[FormalA]) \[CenterDot] ((\[FormalA] \[CenterDot] 
            ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
             \[FormalA])) \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA])) == \[FormalA] \[CenterDot] 
          ((((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
             \[FormalA]) \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalA]))) \[CenterDot] 
           (\[FormalC] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalA])))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 10}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] (
                \[FormalB]_)) \[CenterDot] (\[FormalB]_))) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (\[FormalA]_)))) -> \[FormalB] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalB]), 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"SubstitutionLemma", 37}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalA]_)) -> 
          \[FormalA] \[CenterDot] \[FormalB], "MatchingSide" -> 1, 
        "Position" -> {2, 1, 2, 1}|>|>, {"SubstitutionLemma", 39} -> 
     <|"Statement" -> HoldForm[
        ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           \[FormalA]) \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalB]) \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA])) == \[FormalA] \[CenterDot] 
          ((((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
             \[FormalA]) \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalA]))) \[CenterDot] 
           (\[FormalC] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalA])))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 32}, 
        "Position" -> {2, 1}, "Construct" -> {"SubstitutionLemma", 37}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA] \[CenterDot] \[FormalB], 
        "OutputExpression" -> HoldForm[
          ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
             \[FormalA]) \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalA])) == 
           \[FormalA] \[CenterDot] ((((\[FormalA] \[CenterDot] 
                \[FormalB]) \[CenterDot] \[FormalA]) \[CenterDot] 
              ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] (
                (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
                \[FormalA]))) \[CenterDot] (\[FormalC] \[CenterDot] 
              (\[FormalA] \[CenterDot] \[FormalA])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 40} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalA] \[CenterDot] ((((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] \[FormalA]) \[CenterDot] 
            ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
             ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
              \[FormalA]))) \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalA])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 39}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 36}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
           \[FormalA] \[CenterDot] ((((\[FormalA] \[CenterDot] 
                \[FormalB]) \[CenterDot] \[FormalA]) \[CenterDot] 
              ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] (
                (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
                \[FormalA]))) \[CenterDot] (\[FormalC] \[CenterDot] 
              (\[FormalA] \[CenterDot] \[FormalA])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 41} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalB]) \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalA])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 40}, 
        "Position" -> {2, 1}, "Construct" -> {"SubstitutionLemma", 36}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalB]_))) -> \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
           \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] (\[FormalC] \[CenterDot] 
              (\[FormalA] \[CenterDot] \[FormalA])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 33} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalA]) == 
         \[FormalA] \[CenterDot] \[FormalB]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 41}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (\[FormalA]_)))) -> \[FormalA] \[CenterDot] \[FormalB], 
        "Side" -> 1, "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           (\[FormalB]_)) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalA]_))), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (((\[FormalB]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
             (\[FormalA]_))) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalA]_))) -> \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 34} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] \[FormalB] == 
         (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
          (((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
            \[FormalB]) \[CenterDot] (\[FormalC] \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 41}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (\[FormalA]_)))) -> \[FormalA] \[CenterDot] \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalA]_), "MatchingConstruct" -> {"SubstitutionLemma", 38}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2, 2, 2}|>|>, 
    {"SubstitutionLemma", 42} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalB] \[CenterDot] 
           (\[FormalC] \[CenterDot] \[FormalA])) == \[FormalB]], 
      "Proof" -> <|"Input" -> {"Axiom", 1}, "Position" -> {1}, 
        "Construct" -> {"CriticalPairLemma", 33}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] (\[FormalA]_)) -> 
          \[FormalA] \[CenterDot] \[FormalB], "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] \[FormalA])) == 
           \[FormalB]], "ConstructSide" -> 1, "InputOrientation" -> -1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 43} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalB] \[CenterDot] \[FormalA]) == 
         \[FormalA]], "Proof" -> <|"Input" -> {"CriticalPairLemma", 15}, 
        "Position" -> {2}, "Construct" -> {"CriticalPairLemma", 33}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA] \[CenterDot] \[FormalB], 
        "OutputExpression" -> HoldForm[(\[FormalA] \[CenterDot] 
             \[FormalA]) \[CenterDot] (\[FormalB] \[CenterDot] \[FormalA]) == 
           \[FormalA]], "ConstructSide" -> 1, "InputOrientation" -> -1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 44} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalB] \[CenterDot] \[FormalB]) == 
         \[FormalB]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 9}, 
        "Position" -> {1}, "Construct" -> {"CriticalPairLemma", 33}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA] \[CenterDot] \[FormalB], 
        "OutputExpression" -> HoldForm[(\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] (\[FormalB] \[CenterDot] \[FormalB]) == 
           \[FormalB]], "ConstructSide" -> 1, "InputOrientation" -> -1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 45} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
           (\[FormalC] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalA]))) == 
         \[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalB]) \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 10}, 
        "Position" -> {2, 1}, "Construct" -> {"CriticalPairLemma", 33}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA] \[CenterDot] \[FormalB], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] 
            ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
             (\[FormalC] \[CenterDot] (\[FormalA] \[CenterDot] 
               \[FormalA]))) == \[FormalB] \[CenterDot] 
            ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalB])], 
        "ConstructSide" -> 1, "InputOrientation" -> -1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 46} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
           (\[FormalC] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalA]))) == 
         \[FormalB] \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 45}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 33}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] (\[FormalA]_)) -> 
          \[FormalA] \[CenterDot] \[FormalB], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
              \[FormalA]) \[CenterDot] (\[FormalC] \[CenterDot] 
              (\[FormalA] \[CenterDot] \[FormalA]))) == 
           \[FormalB] \[CenterDot] \[FormalA]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 47} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalA]) == 
         \[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalB]) \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 11}, 
        "Position" -> {2, 1}, "Construct" -> {"CriticalPairLemma", 33}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA] \[CenterDot] \[FormalB], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] 
            ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalA]) == 
           \[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] \[FormalB])], "ConstructSide" -> 1, 
        "InputOrientation" -> -1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 48} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalB]) \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 47}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 33}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] (\[FormalA]_)) -> 
          \[FormalA] \[CenterDot] \[FormalB], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
           \[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] \[FormalB])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 49} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalB] \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 33}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] (\[FormalA]_)) -> 
          \[FormalA] \[CenterDot] \[FormalB], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
           \[FormalB] \[CenterDot] \[FormalA]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 50} -> 
     <|"Statement" -> HoldForm[
        ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           \[FormalC]) \[CenterDot] (\[FormalC] \[CenterDot] \[FormalA]) == 
         \[FormalC]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 4}, 
        "Position" -> {1}, "Construct" -> {"CriticalPairLemma", 33}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA] \[CenterDot] \[FormalB], 
        "OutputExpression" -> HoldForm[
          ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
             \[FormalC]) \[CenterDot] (\[FormalC] \[CenterDot] \[FormalA]) == 
           \[FormalC]], "ConstructSide" -> 1, "InputOrientation" -> -1, 
        "Side" -> 1|>|>, {"CriticalPairLemma", 35} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalA] \[CenterDot] ((\[FormalC] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalA])) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 41}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (\[FormalA]_)))) -> \[FormalA] \[CenterDot] \[FormalB], 
        "Side" -> 1, "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           (\[FormalB]_)) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalA]_))), 
        "MatchingConstruct" -> {"SubstitutionLemma", 49}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 51} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB])) == 
         \[FormalA] \[CenterDot] \[FormalB]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 37}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 49}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
             (\[FormalA] \[CenterDot] \[FormalB])) == \[FormalA] \[CenterDot] 
            \[FormalB]], "ConstructSide" -> 1, "InputOrientation" -> -1, 
        "Side" -> 1|>|>, {"CriticalPairLemma", 36} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalC] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 42}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalA]_))) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"SubstitutionLemma", 49}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 37} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CenterDot] 
           \[FormalA]) \[CenterDot] ((\[FormalC] \[CenterDot] 
            \[FormalB]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 42}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalA]_))) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          ((\[FormalC]_) \[CenterDot] (\[FormalA]_)), "MatchingConstruct" -> 
         {"SubstitutionLemma", 49}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 38} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalC])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 42}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalA]_))) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalC]_) \[CenterDot] 
          (\[FormalA]_), "MatchingConstruct" -> {"SubstitutionLemma", 43}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"CriticalPairLemma", 39} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 42}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalA]_))) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalC]_) \[CenterDot] 
          (\[FormalA]_), "MatchingConstruct" -> {"SubstitutionLemma", 49}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 40} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         ((\[FormalC] \[CenterDot] \[FormalA]) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB])) \[CenterDot] \[FormalB]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 42}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalA]_))) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          ((\[FormalC]_) \[CenterDot] (\[FormalA]_)), "MatchingConstruct" -> 
         {"SubstitutionLemma", 42}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalA]_))) -> \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 52} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] ((\[FormalB] \[CenterDot] 
            \[FormalC]) \[CenterDot] \[FormalA]) == \[FormalA]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 50}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 49}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] \[FormalA]) == 
           \[FormalA]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"CriticalPairLemma", 41} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalB] \[CenterDot] (((\[FormalB] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalC]) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 52}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"SubstitutionLemma", 44}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 42} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CenterDot] 
           \[FormalA]) \[CenterDot] ((\[FormalB] \[CenterDot] 
            \[FormalC]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 52}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"SubstitutionLemma", 49}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 43} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalC]) == 
         ((\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalC])) \[CenterDot] \[FormalC]) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 52}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> ((\[FormalB]_) \[CenterDot] 
           (\[FormalC]_)) \[CenterDot] (\[FormalA]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 42}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalA]_))) -> \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 44} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 52}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> ((\[FormalB]_) \[CenterDot] 
           (\[FormalC]_)) \[CenterDot] (\[FormalA]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 49}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 45} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         ((\[FormalC] \[CenterDot] \[FormalB]) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB])) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 42}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalA]_))) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          ((\[FormalC]_) \[CenterDot] (\[FormalA]_)), "MatchingConstruct" -> 
         {"CriticalPairLemma", 36}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalB]_))) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 46} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] \[FormalC] == 
         (((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalC]) \[CenterDot] \[FormalB]) \[CenterDot] \[FormalC]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 52}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> ((\[FormalB]_) \[CenterDot] 
           (\[FormalC]_)) \[CenterDot] (\[FormalA]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 37}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 53} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] ((\[FormalC] \[CenterDot] 
            \[FormalB]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 38}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 49}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] ((\[FormalC] \[CenterDot] 
              \[FormalB]) \[CenterDot] \[FormalA])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 47} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalC])) \[CenterDot] \[FormalB]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 36}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalB]_))) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          ((\[FormalC]_) \[CenterDot] (\[FormalB]_)), "MatchingConstruct" -> 
         {"CriticalPairLemma", 39}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalC]_))) -> \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 48} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalC]) == 
         ((\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalC])) \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 52}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> ((\[FormalB]_) \[CenterDot] 
           (\[FormalC]_)) \[CenterDot] (\[FormalA]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 39}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalC]_))) -> \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 49} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 44}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          ((\[FormalB]_) \[CenterDot] (\[FormalC]_)), "MatchingConstruct" -> 
         {"SubstitutionLemma", 52}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 54} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalB] \[CenterDot] ((\[FormalC] \[CenterDot] 
            \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 40}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 49}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
           \[FormalB] \[CenterDot] ((\[FormalC] \[CenterDot] 
              \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
              \[FormalB]))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 50} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalC]) \[CenterDot] \[FormalB]) == 
         ((\[FormalA] \[CenterDot] \[FormalC]) \[CenterDot] 
           \[FormalB]) \[CenterDot] ((\[FormalAlpha] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB])) \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 54}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalA]_))) -> 
          \[FormalC] \[CenterDot] \[FormalA], "Side" -> 1, 
        "Subpattern" -> (\[FormalC]_) \[CenterDot] (\[FormalA]_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 42}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
            (\[FormalB]_)) -> \[FormalB], "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"SubstitutionLemma", 55} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           \[FormalA]) \[CenterDot] ((\[FormalAlpha] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalA])) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 50}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 42}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
            (\[FormalB]_)) -> \[FormalB], "OutputExpression" -> 
         HoldForm[\[FormalA] == ((\[FormalB] \[CenterDot] 
              \[FormalC]) \[CenterDot] \[FormalA]) \[CenterDot] 
            ((\[FormalAlpha] \[CenterDot] (\[FormalB] \[CenterDot] 
               \[FormalA])) \[CenterDot] \[FormalA])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 51} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalB] \[CenterDot] 
           (\[FormalC] \[CenterDot] \[FormalA])) == 
         (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
            \[FormalA])) \[CenterDot] ((\[FormalAlpha] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB])) \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 54}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalA]_))) -> 
          \[FormalC] \[CenterDot] \[FormalA], "Side" -> 1, 
        "Subpattern" -> (\[FormalC]_) \[CenterDot] (\[FormalA]_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 42}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalB], "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"SubstitutionLemma", 56} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
          ((\[FormalAlpha] \[CenterDot] (\[FormalC] \[CenterDot] 
             \[FormalA])) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 51}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 42}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalB], "OutputExpression" -> 
         HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
            ((\[FormalAlpha] \[CenterDot] (\[FormalC] \[CenterDot] 
               \[FormalA])) \[CenterDot] \[FormalA])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 52} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) == 
         (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
            \[FormalC])) \[CenterDot] ((\[FormalAlpha] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB])) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 54}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalA]_))) -> 
          \[FormalC] \[CenterDot] \[FormalA], "Side" -> 1, 
        "Subpattern" -> (\[FormalC]_) \[CenterDot] (\[FormalA]_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 44}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"SubstitutionLemma", 57} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
          ((\[FormalAlpha] \[CenterDot] (\[FormalA] \[CenterDot] 
             \[FormalB])) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 52}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 44}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
            ((\[FormalAlpha] \[CenterDot] (\[FormalA] \[CenterDot] 
               \[FormalB])) \[CenterDot] \[FormalA])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 58} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalA] \[CenterDot] ((\[FormalC] \[CenterDot] 
            \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 45}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 49}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
           \[FormalA] \[CenterDot] ((\[FormalC] \[CenterDot] 
              \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
              \[FormalB]))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 59} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalC]))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 47}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 49}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
           \[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
              \[FormalC]))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 53} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalC] \[CenterDot] \[FormalB])) == 
         (\[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] 
            \[FormalB])) \[CenterDot] (\[FormalA] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalAlpha]))], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 59}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) -> \[FormalB] \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          (\[FormalA]_), "MatchingConstruct" -> {"CriticalPairLemma", 36}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2, 1}|>|>, {"SubstitutionLemma", 60} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
          (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalC]) \[CenterDot] \[FormalAlpha]))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 53}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 36}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
            (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalC]) \[CenterDot] \[FormalAlpha]))], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 61} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalB]) \[CenterDot] (\[FormalB] \[CenterDot] \[FormalC]))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 49}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 49}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
           \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] (\[FormalB] \[CenterDot] 
              \[FormalC]))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 62} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalC]) == \[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalC])) \[CenterDot] \[FormalC])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 43}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 49}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalC]) == \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
             \[FormalC])], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 63} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalC]) == \[FormalA] \[CenterDot] 
          (\[FormalC] \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalC])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 62}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 49}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalC]) == \[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] 
             (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
               \[FormalC])))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 54} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalA])) == 
         \[FormalA] \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 63}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] (\[FormalB]_)))) -> 
          \[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] \[FormalB]), 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
            (\[FormalB]_))), "MatchingConstruct" -> {"CriticalPairLemma", 
          39}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (\[FormalC]_))) -> \[FormalB], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 55} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB])) == 
         \[FormalA] \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 63}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] (\[FormalB]_)))) -> 
          \[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] \[FormalB]), 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
            (\[FormalB]_))), "MatchingConstruct" -> {"CriticalPairLemma", 
          44}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 56} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalB]) == \[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 63}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] (\[FormalB]_)))) -> 
          \[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] \[FormalB]), 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
            (\[FormalB]_))), "MatchingConstruct" -> {"CriticalPairLemma", 
          54}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalA]_))) -> 
          \[FormalA] \[CenterDot] \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 57} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB]) == 
         (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
          (((\[FormalC] \[CenterDot] \[FormalA]) \[CenterDot] 
            \[FormalB]) \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 55}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalA] \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"CriticalPairLemma", 37}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (((\[FormalC]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
            (\[FormalB]_)) -> \[FormalB], "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 58} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalB]) == \[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 56}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) <-> 
          (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)), "Side" -> 1, "Subpattern" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 49}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 59} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
          ((\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
             \[FormalC])) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 53}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalC]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"CriticalPairLemma", 56}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) <-> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalB]_)), "MatchingSide" -> 1, 
        "Position" -> {2, 1}|>|>, {"CriticalPairLemma", 60} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           \[FormalA]) \[CenterDot] ((\[FormalB] \[CenterDot] 
            (\[FormalC] \[CenterDot] \[FormalC])) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 37}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalC]_) \[CenterDot] 
          (\[FormalA]_), "MatchingConstruct" -> {"CriticalPairLemma", 56}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) <-> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalB]_)), "MatchingSide" -> 1, 
        "Position" -> {2, 1}|>|>, {"CriticalPairLemma", 61} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 51}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalA] \[CenterDot] \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] (\[FormalB]_)), "MatchingConstruct" -> 
         {"CriticalPairLemma", 56}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) <-> 
          (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)), "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 62} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalB]) == 
         (\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 56}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) <-> 
          (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)), "Side" -> 2, "Subpattern" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
           (\[FormalB]_)), "MatchingConstruct" -> {"SubstitutionLemma", 49}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 63} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
            \[FormalB]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 61}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalA] \[CenterDot] \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          ((\[FormalB]_) \[CenterDot] (\[FormalB]_)), "MatchingConstruct" -> 
         {"SubstitutionLemma", 49}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 64} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalA]) == 
         (\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 58}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalB]_)) <-> 
          (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalA]_)), "Side" -> 1, "Subpattern" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
           (\[FormalB]_)), "MatchingConstruct" -> {"SubstitutionLemma", 49}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 65} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
          (((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalC]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 53}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalC]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"CriticalPairLemma", 64}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalA]_)) <-> ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2, 1}|>|>, {"CriticalPairLemma", 66} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
           ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalC]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 42}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalA]_))) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalC]_) \[CenterDot] 
          (\[FormalA]_), "MatchingConstruct" -> {"CriticalPairLemma", 64}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalA]_)) <-> ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"SubstitutionLemma", 64} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] \[FormalC] == \[FormalC] \[CenterDot] 
          (((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalC]) \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 46}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 49}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalC] == \[FormalC] \[CenterDot] 
            (((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
              \[FormalC]) \[CenterDot] \[FormalB])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 65} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] \[FormalC] == \[FormalC] \[CenterDot] 
          (\[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalC]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 64}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 49}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalC] == \[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] 
             ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
              \[FormalC]))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 66} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalC]) == \[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalC])) \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 48}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 49}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalC]) == \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
             \[FormalB])], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 67} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalC]) == \[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalC])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 66}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 49}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalC]) == \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
               \[FormalC])))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 67} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalA] \[CenterDot] ((\[FormalC] \[CenterDot] 
            (\[FormalC] \[CenterDot] \[FormalA])) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 35}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (\[FormalA]_))) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (\[FormalC]_))) -> \[FormalA] \[CenterDot] \[FormalC], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] (\[FormalA]_)), "MatchingConstruct" -> 
         {"CriticalPairLemma", 56}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) <-> 
          (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)), "MatchingSide" -> 2, "Position" -> {2, 1}|>|>, 
    {"CriticalPairLemma", 68} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalB] \[CenterDot] ((\[FormalC] \[CenterDot] 
            (\[FormalC] \[CenterDot] \[FormalB])) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 41}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalC] \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           (\[FormalA]_)) \[CenterDot] (\[FormalB]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 62}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) <-> 
          ((\[FormalB]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalA]_), "MatchingSide" -> 2, "Position" -> {2, 1}|>|>, 
    {"SubstitutionLemma", 68} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalAlpha] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalA])))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 55}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 49}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == ((\[FormalB] \[CenterDot] 
              \[FormalC]) \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalA] \[CenterDot] (\[FormalAlpha] \[CenterDot] 
              (\[FormalB] \[CenterDot] \[FormalA])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 69} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
             \[FormalA]))) \[CenterDot] ((\[FormalC] \[CenterDot] 
            \[FormalAlpha]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 68}, 
        "Orientation" -> -1, "Rule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
            ((\[FormalAlpha]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (\[FormalC]_)))) -> \[FormalC], "Side" -> 1, 
        "Subpattern" -> (((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
          ((\[FormalC]_) \[CenterDot] ((\[FormalAlpha]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalC]_)))), 
        "MatchingConstruct" -> {"SubstitutionLemma", 49}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"SubstitutionLemma", 69} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalAlpha] \[CenterDot] 
            (\[FormalC] \[CenterDot] \[FormalA])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 56}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 49}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
            (\[FormalA] \[CenterDot] (\[FormalAlpha] \[CenterDot] 
              (\[FormalC] \[CenterDot] \[FormalA])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 70} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalAlpha] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 57}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 49}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
            (\[FormalA] \[CenterDot] (\[FormalAlpha] \[CenterDot] 
              (\[FormalA] \[CenterDot] \[FormalB])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 70} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] (\[FormalA] \[CenterDot] 
             \[FormalC]))) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalC] \[CenterDot] \[FormalAlpha]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 70}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalAlpha]_) \[CenterDot] 
             ((\[FormalA]_) \[CenterDot] (\[FormalB]_)))) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] ((\[FormalAlpha]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalB]_)))), 
        "MatchingConstruct" -> {"SubstitutionLemma", 49}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 71} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalA])) \[CenterDot] \[FormalB] == 
         ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
           ((\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
              \[FormalA])) \[CenterDot] \[FormalB])) \[CenterDot] 
          \[FormalB]], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 60}, 
        "Orientation" -> -1, "Rule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             ((\[FormalB]_) \[CenterDot] (\[FormalB]_))) \[CenterDot] 
            (\[FormalC]_)) -> \[FormalC], "Side" -> 1, 
        "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalB]_))) \[CenterDot] 
          (\[FormalC]_), "MatchingConstruct" -> {"CriticalPairLemma", 59}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] (\[FormalC]_))) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 71} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalA])) \[CenterDot] \[FormalB] == 
         \[FormalB] \[CenterDot] \[FormalB]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 71}, "Position" -> {1}, 
        "Construct" -> {"SubstitutionLemma", 52}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
              \[FormalA])) \[CenterDot] \[FormalB] == \[FormalB] \[CenterDot] 
            \[FormalB]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 72} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 71}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
           (\[FormalB]_) <-> (\[FormalB]_) \[CenterDot] (\[FormalB]_), 
        "Side" -> 1, "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"SubstitutionLemma", 49}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 73} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalA])) \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalC]))) == \[FormalB]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 71}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
           (\[FormalB]_) <-> (\[FormalB]_) \[CenterDot] (\[FormalB]_), 
        "Side" -> 2, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalA]_), "MatchingConstruct" -> {"SubstitutionLemma", 70}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalAlpha]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (\[FormalB]_)))) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"SubstitutionLemma", 72} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalA])) \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalB]) == \[FormalB]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 73}, "Position" -> {2}, 
        "Construct" -> {"CriticalPairLemma", 55}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalB]_))) -> 
          \[FormalA] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
              \[FormalA])) \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalB]) == \[FormalB]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 74} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalB])) \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalC] \[CenterDot] \[FormalC])))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 72}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"CriticalPairLemma", 72}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalA]_) <-> 
          (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalB]_))), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 75} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalA]) == 
         (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalC] \[CenterDot] \[FormalC]))) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 64}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) <-> 
          ((\[FormalB]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalA]_), "Side" -> 2, "Subpattern" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalA]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 72}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] (\[FormalA]_) <-> 
          (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalB]_))), "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 76} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalB]) == 
         (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalC] \[CenterDot] \[FormalC]))) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 62}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) <-> 
          ((\[FormalB]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalA]_), "Side" -> 2, "Subpattern" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalA]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 72}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] (\[FormalA]_) <-> 
          (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalB]_))), "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 77} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
          (((\[FormalC] \[CenterDot] \[FormalC]) \[CenterDot] 
            (((\[FormalC] \[CenterDot] \[FormalC]) \[CenterDot] 
              \[FormalAlpha]) \[CenterDot] (\[FormalB] \[CenterDot] 
              \[FormalC]))) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 65}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) \[CenterDot] 
           ((((\[FormalB]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          (\[FormalC]_), "MatchingConstruct" -> {"CriticalPairLemma", 41}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((((\[FormalA]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalA]_))) -> 
          \[FormalC] \[CenterDot] \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {1, 2}|>|>, {"SubstitutionLemma", 73} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
          (((\[FormalC] \[CenterDot] \[FormalC]) \[CenterDot] 
            \[FormalAlpha]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 77}, 
        "Position" -> {2, 1}, "Construct" -> {"CriticalPairLemma", 34}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] ((((\[FormalA]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalA]_))) -> 
          (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalB], 
        "OutputExpression" -> HoldForm[\[FormalA] == 
           (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
              \[FormalC])) \[CenterDot] (((\[FormalC] \[CenterDot] 
               \[FormalC]) \[CenterDot] \[FormalAlpha]) \[CenterDot] 
             \[FormalA])], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 78} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalC])) \[CenterDot] 
          ((((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalB])) \[CenterDot] 
            (((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
              \[FormalC]) \[CenterDot] (\[FormalAlpha] \[CenterDot] 
              \[FormalB]))) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 65}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) \[CenterDot] 
           ((((\[FormalB]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          (\[FormalC]_), "MatchingConstruct" -> {"CriticalPairLemma", 34}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_))) -> (\[FormalA] \[CenterDot] 
            \[FormalA]) \[CenterDot] \[FormalB], "MatchingSide" -> 1, 
        "Position" -> {1, 2}|>|>, {"SubstitutionLemma", 74} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalC])) \[CenterDot] ((\[FormalB] \[CenterDot] 
            (((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
              \[FormalC]) \[CenterDot] (\[FormalAlpha] \[CenterDot] 
              \[FormalB]))) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 78}, 
        "Position" -> {2, 1, 1}, "Construct" -> {"SubstitutionLemma", 44}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)) -> \[FormalB], "OutputExpression" -> 
         HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
             ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
              \[FormalC])) \[CenterDot] ((\[FormalB] \[CenterDot] 
              (((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
                \[FormalC]) \[CenterDot] (\[FormalAlpha] \[CenterDot] 
                \[FormalB]))) \[CenterDot] \[FormalA])], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 75} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalC])) \[CenterDot] ((\[FormalAlpha] \[CenterDot] 
            \[FormalB]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 74}, 
        "Position" -> {2, 1}, "Construct" -> {"CriticalPairLemma", 41}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalC] \[CenterDot] \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalA] == 
           (\[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalC])) \[CenterDot] 
            ((\[FormalAlpha] \[CenterDot] \[FormalB]) \[CenterDot] 
             \[FormalA])], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 79} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalB] \[CenterDot] ((\[FormalC] \[CenterDot] 
              (\[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] 
                \[FormalB]))) \[CenterDot] (\[FormalAlpha] \[CenterDot] 
              (\[FormalB] \[CenterDot] \[FormalB])))) \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalAlpha] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalB])))], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 66}, "Orientation" -> -1, 
        "Rule" -> (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalC], "Side" -> 1, 
        "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           (\[FormalA]_)) \[CenterDot] (\[FormalB]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 68}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
              (\[FormalA]_))) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalC] \[CenterDot] \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"SubstitutionLemma", 76} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalB] \[CenterDot] ((\[FormalC] \[CenterDot] 
              \[FormalB]) \[CenterDot] (\[FormalAlpha] \[CenterDot] 
              (\[FormalB] \[CenterDot] \[FormalB])))) \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalAlpha] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalB])))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 79}, "Position" -> {1, 1, 2, 1}, 
        "Construct" -> {"CriticalPairLemma", 61}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalB]_))) -> 
          \[FormalA] \[CenterDot] \[FormalB], "OutputExpression" -> 
         HoldForm[\[FormalA] == ((\[FormalB] \[CenterDot] 
              ((\[FormalC] \[CenterDot] \[FormalB]) \[CenterDot] (
                \[FormalAlpha] \[CenterDot] (\[FormalB] \[CenterDot] 
                 \[FormalB])))) \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalA] \[CenterDot] (\[FormalAlpha] \[CenterDot] 
              (\[FormalB] \[CenterDot] \[FormalB])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 77} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalAlpha] \[CenterDot] (\[FormalC] \[CenterDot] 
             \[FormalC])))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 76}, "Position" -> {1, 1}, 
        "Construct" -> {"SubstitutionLemma", 46}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             ((\[FormalA]_) \[CenterDot] (\[FormalA]_)))) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == ((\[FormalB] \[CenterDot] 
              \[FormalC]) \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalA] \[CenterDot] (\[FormalAlpha] \[CenterDot] 
              (\[FormalC] \[CenterDot] \[FormalC])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 80} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
             (\[FormalA] \[CenterDot] \[FormalB])))) \[CenterDot] 
          \[FormalB] == \[FormalB] \[CenterDot] \[FormalB]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 65}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
              (\[FormalB]_)) \[CenterDot] (\[FormalA]_))) -> 
          (\[FormalC] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          (((\[FormalC]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalA]_)), "MatchingConstruct" -> {"CriticalPairLemma", 69}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] (\[FormalA]_)))) \[CenterDot] 
           (((\[FormalC]_) \[CenterDot] (\[FormalAlpha]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 78} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] 
              \[FormalA])))) == \[FormalA] \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 80}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 49}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             (\[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] (
                \[FormalB] \[CenterDot] \[FormalA])))) == 
           \[FormalA] \[CenterDot] \[FormalA]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 81} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB]))) == \[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 67}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             ((\[FormalB]_) \[CenterDot] (\[FormalC]_)))) -> 
          \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalC]), 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalC]_))), "MatchingConstruct" -> {"SubstitutionLemma", 
          78}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
              ((\[FormalB]_) \[CenterDot] (\[FormalA]_))))) -> 
          \[FormalA] \[CenterDot] \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 82} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalB])) == \[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
           (\[FormalC] \[CenterDot] (\[FormalA] \[CenterDot] 
             (\[FormalA] \[CenterDot] \[FormalB]))))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 81}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             ((\[FormalA]_) \[CenterDot] (\[FormalB]_)))) -> 
          \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalB]), 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"CriticalPairLemma", 56}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) <-> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalB]_)), "MatchingSide" -> 2, 
        "Position" -> {2, 2, 2}|>|>, {"SubstitutionLemma", 79} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
            \[FormalB]) \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
              \[FormalB]))))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 82}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 44}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
           \[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
              \[FormalB]) \[CenterDot] (\[FormalC] \[CenterDot] 
              (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
                \[FormalB]))))], "ConstructSide" -> 1, "InputOrientation" -> 
         1, "Side" -> 1|>|>, {"CriticalPairLemma", 83} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
          ((\[FormalAlpha] \[CenterDot] (\[FormalAlpha] \[CenterDot] 
             \[FormalC])) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 73}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) \[CenterDot] 
           ((((\[FormalC]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
             (\[FormalAlpha]_)) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> ((\[FormalC]_) \[CenterDot] 
           (\[FormalC]_)) \[CenterDot] (\[FormalAlpha]_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 62}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) <-> ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 2, 
        "Position" -> {2, 1}|>|>, {"CriticalPairLemma", 84} -> 
     <|"Statement" -> HoldForm[
        ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
           (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
             \[FormalA]))) \[CenterDot] \[FormalB] == \[FormalB] \[CenterDot] 
          \[FormalB]], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 65}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
              (\[FormalB]_)) \[CenterDot] (\[FormalA]_))) -> 
          (\[FormalC] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          (((\[FormalC]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalA]_)), "MatchingConstruct" -> {"SubstitutionLemma", 73}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] ((((\[FormalC]_) \[CenterDot] 
              (\[FormalC]_)) \[CenterDot] (\[FormalAlpha]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 80} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
           (\[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] \[FormalB]))) == 
         \[FormalA] \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 84}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 49}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
              \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
              (\[FormalC] \[CenterDot] \[FormalB]))) == 
           \[FormalA] \[CenterDot] \[FormalA]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 85} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
           (\[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] 
             (\[FormalB] \[CenterDot] (\[FormalAlpha] \[CenterDot] (
                \[FormalB] \[CenterDot] \[FormalAlpha]))))))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 80}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
              (\[FormalB]_)))) -> \[FormalA] \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"CriticalPairLemma", 70}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             ((\[FormalA]_) \[CenterDot] (\[FormalC]_)))) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalAlpha]_))) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2, 1}|>|>, {"SubstitutionLemma", 81} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
           (\[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalB]))))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 85}, 
        "Position" -> {2, 2, 2, 2}, "Construct" -> {"CriticalPairLemma", 55}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalA] \[CenterDot] \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
           \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             (\[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] (
                \[FormalB] \[CenterDot] \[FormalB]))))], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 86} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         \[FormalA] \[CenterDot] (((\[FormalB] \[CenterDot] 
             \[FormalB]) \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalB])) \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] 
              \[FormalC]))))], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 80}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] (\[FormalB]_)))) -> 
          \[FormalA] \[CenterDot] \[FormalA], "Side" -> 1, 
        "Subpattern" -> (\[FormalC]_) \[CenterDot] (\[FormalB]_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 58}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)) <-> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)), "MatchingSide" -> 1, 
        "Position" -> {2, 2, 2}|>|>, {"SubstitutionLemma", 82} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
           (\[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalC]))))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 86}, 
        "Position" -> {2, 1}, "Construct" -> {"SubstitutionLemma", 44}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)) -> \[FormalB], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
           \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             (\[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] (
                \[FormalB] \[CenterDot] \[FormalC]))))], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 87} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalA]))) == \[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 67}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             ((\[FormalB]_) \[CenterDot] (\[FormalC]_)))) -> 
          \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalC]), 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalC]_))), "MatchingConstruct" -> {"SubstitutionLemma", 
          81}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
              ((\[FormalB]_) \[CenterDot] (\[FormalB]_))))) -> 
          \[FormalA] \[CenterDot] \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 88} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
           ((\[FormalC] \[CenterDot] (\[FormalC] \[CenterDot] 
              \[FormalA])) \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 82}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] (
                \[FormalC]_))))) -> \[FormalA] \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
              (\[FormalC]_))))), "MatchingConstruct" -> {"CriticalPairLemma", 
          67}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             ((\[FormalB]_) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalC]_))) -> 
          \[FormalA] \[CenterDot] \[FormalC], "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 89} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         \[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
            (\[FormalC] \[CenterDot] (\[FormalA] \[CenterDot] 
              \[FormalB]))) \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 88}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
              ((\[FormalC]_) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalA] \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> ((\[FormalC]_) \[CenterDot] 
           ((\[FormalC]_) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"SubstitutionLemma", 69}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalAlpha]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
              (\[FormalA]_)))) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 90} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalA]) == 
         (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
          (\[FormalB] \[CenterDot] ((\[FormalC] \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 88}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
              ((\[FormalC]_) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalA] \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalC]_) \[CenterDot] 
          ((\[FormalC]_) \[CenterDot] (\[FormalA]_)), "MatchingConstruct" -> 
         {"CriticalPairLemma", 61}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalA] \[CenterDot] \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {2, 2, 1}|>|>, 
    {"SubstitutionLemma", 83} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalB] \[CenterDot] 
           ((\[FormalC] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 90}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 44}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "OutputExpression" -> HoldForm[\[FormalA] == 
           (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalB] \[CenterDot] ((\[FormalC] \[CenterDot] 
               \[FormalA]) \[CenterDot] \[FormalB]))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 91} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] ((\[FormalB] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalC])) \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 83}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (((\[FormalC]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalA], "Side" -> 1, 
        "Subpattern" -> ((\[FormalC]_) \[CenterDot] 
           (\[FormalA]_)) \[CenterDot] (\[FormalB]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 44}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"CriticalPairLemma", 92} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] \[FormalA]) == 
         (\[FormalC] \[CenterDot] (\[FormalA] \[CenterDot] 
            ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
             \[FormalA]))) \[CenterDot] \[FormalC]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 42}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           (\[FormalC]_)) \[CenterDot] (\[FormalB]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 83}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (((\[FormalC]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 84} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalB] \[CenterDot] 
           (\[FormalB] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalC])))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 91}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 49}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
             \[FormalA]) \[CenterDot] (\[FormalB] \[CenterDot] 
             (\[FormalB] \[CenterDot] (\[FormalA] \[CenterDot] 
               \[FormalC])))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 85} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
           (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
             (\[FormalA] \[CenterDot] \[FormalB]))))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 89}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 49}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
           \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] (
                \[FormalA] \[CenterDot] \[FormalB]))))], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 93} -> 
     <|"Statement" -> HoldForm[
        ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB])) \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalC]) == 
         (\[FormalAlpha] \[CenterDot] (\[FormalAlpha] \[CenterDot] 
            \[FormalAlpha])) \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 74}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] (\[FormalC]_)))) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          ((\[FormalC]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
            (\[FormalC]_))), "MatchingConstruct" -> {"SubstitutionLemma", 
          75}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
              (\[FormalB]_)) \[CenterDot] (\[FormalC]_))) \[CenterDot] 
           (((\[FormalAlpha]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 86} -> 
     <|"Statement" -> HoldForm[
        ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB])) \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalC]) == 
         \[FormalA] \[CenterDot] \[FormalB]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 93}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 72}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (\[FormalA]_))) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)) -> \[FormalB], "OutputExpression" -> 
         HoldForm[((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
             (\[FormalA] \[CenterDot] \[FormalB])) \[CenterDot] 
            ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalC]) == 
           \[FormalA] \[CenterDot] \[FormalB]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 94} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
             \[FormalAlpha]))) \[CenterDot] ((\[FormalAlpha] \[CenterDot] 
            ((\[FormalC] \[CenterDot] \[FormalC]) \[CenterDot] 
             \[FormalAlpha])) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 83}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) \[CenterDot] 
           (((\[FormalAlpha]_) \[CenterDot] ((\[FormalAlpha]_) \[CenterDot] 
              (\[FormalC]_))) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalAlpha]_) \[CenterDot] 
          (\[FormalC]_), "MatchingConstruct" -> {"CriticalPairLemma", 64}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalA]_)) <-> ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2, 1, 2}|>|>, {"SubstitutionLemma", 87} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
             \[FormalAlpha]))) \[CenterDot] ((\[FormalAlpha] \[CenterDot] 
            \[FormalC]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 94}, 
        "Position" -> {2, 1}, "Construct" -> {"CriticalPairLemma", 63}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA] \[CenterDot] \[FormalB], 
        "OutputExpression" -> HoldForm[\[FormalA] == 
           (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
              (\[FormalC] \[CenterDot] \[FormalAlpha]))) \[CenterDot] 
            ((\[FormalAlpha] \[CenterDot] \[FormalC]) \[CenterDot] 
             \[FormalA])], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 88} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalAlpha] \[CenterDot] (\[FormalC] \[CenterDot] 
             \[FormalB])))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 87}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 49}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == ((\[FormalB] \[CenterDot] 
              \[FormalC]) \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalA] \[CenterDot] (\[FormalAlpha] \[CenterDot] 
              (\[FormalC] \[CenterDot] \[FormalB])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 95} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
            (\[FormalC] \[CenterDot] \[FormalB])) \[CenterDot] \[FormalA]) == 
         (\[FormalC] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 75}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) <-> 
          ((\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] (\[FormalC]_)))) \[CenterDot] 
           (\[FormalA]_), "Side" -> 2, "Subpattern" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalB]_))), 
        "MatchingConstruct" -> {"SubstitutionLemma", 88}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
            ((\[FormalAlpha]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
              (\[FormalA]_)))) -> \[FormalC], "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 96} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] \[FormalC] == \[FormalC] \[CenterDot] 
          (((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalA])) \[CenterDot] \[FormalC])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 95}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] (\[FormalB]_))) \[CenterDot] 
            (\[FormalA]_)) -> (\[FormalC] \[CenterDot] 
            \[FormalB]) \[CenterDot] \[FormalA], "Side" -> 1, 
        "Subpattern" -> (\[FormalC]_) \[CenterDot] (\[FormalB]_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 49}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2, 1, 2}|>|>, {"SubstitutionLemma", 89} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] \[FormalC] == \[FormalC] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 96}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 63}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalA]_)) -> 
          \[FormalA] \[CenterDot] \[FormalB], "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalC] == \[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalA])], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 97} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalC]) == 
         (\[FormalC] \[CenterDot] (\[FormalC] \[CenterDot] 
            \[FormalB])) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 89}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (\[FormalC]_) <-> 
          (\[FormalC]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalA]_)), "Side" -> 1, "Subpattern" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 56}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) <-> 
          (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)), "MatchingSide" -> 2, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 98} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] \[FormalC] == 
         (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
          ((\[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalA])) \[CenterDot] (\[FormalC] \[CenterDot] 
            \[FormalAlpha]))], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 61}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) -> \[FormalA] \[CenterDot] \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"SubstitutionLemma", 89}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalC]_) <-> (\[FormalC]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)), "MatchingSide" -> 1, 
        "Position" -> {2, 1}|>|>, {"CriticalPairLemma", 99} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
          \[FormalAlpha] == \[FormalAlpha] \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 89}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (\[FormalC]_) <-> 
          (\[FormalC]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalA]_)), "Side" -> 2, "Subpattern" -> 
         (\[FormalB]_) \[CenterDot] (\[FormalC]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 89}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (\[FormalC]_) <-> 
          (\[FormalC]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalA]_)), "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 100} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalC] \[CenterDot] 
           \[FormalAlpha]) == (\[FormalB] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalAlpha] \[CenterDot] 
           \[FormalC])], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 
          89}, "Orientation" -> 1, "Rule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalC]_) <-> (\[FormalC]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)), "Side" -> 2, 
        "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          ((\[FormalB]_) \[CenterDot] (\[FormalC]_)), "MatchingConstruct" -> 
         {"SubstitutionLemma", 89}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (\[FormalC]_) <-> 
          (\[FormalC]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalA]_)), "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"CriticalPairLemma", 101} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalA]))) == \[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 87}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             ((\[FormalA]_) \[CenterDot] (\[FormalA]_)))) <-> 
          (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)), "Side" -> 2, "Subpattern" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
           (\[FormalB]_)), "MatchingConstruct" -> {"CriticalPairLemma", 58}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)) <-> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)), "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 102} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] (((\[FormalB] \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalC]) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalA])) == 
         (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalC]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 101}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             ((\[FormalA]_) \[CenterDot] (\[FormalA]_)))) -> 
          \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalA]), 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          ((\[FormalC]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_))), "MatchingConstruct" -> {"SubstitutionLemma", 
          77}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
            ((\[FormalAlpha]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
              (\[FormalB]_)))) -> \[FormalC], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 103} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalA]) == \[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalC]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 101}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             ((\[FormalA]_) \[CenterDot] (\[FormalA]_)))) -> 
          \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalA]), 
        "Side" -> 1, "Subpattern" -> (\[FormalC]_) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] (\[FormalA]_)), "MatchingConstruct" -> 
         {"SubstitutionLemma", 89}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (\[FormalC]_) <-> 
          (\[FormalC]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalA]_)), "MatchingSide" -> 2, "Position" -> {2, 2}|>|>, 
    {"CriticalPairLemma", 104} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
            \[FormalAlpha])) == ((\[FormalAlpha] \[CenterDot] 
            \[FormalC]) \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 99}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) \[CenterDot] 
           (\[FormalAlpha]_) <-> (\[FormalAlpha]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalB]_))), "Side" -> 1, "Subpattern" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
           (\[FormalC]_)), "MatchingConstruct" -> {"SubstitutionLemma", 49}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 105} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
          \[FormalAlpha] == \[FormalAlpha] \[CenterDot] 
          ((\[FormalC] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 99}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) \[CenterDot] 
           (\[FormalAlpha]_) <-> (\[FormalAlpha]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalB]_))), "Side" -> 2, "Subpattern" -> 
         (\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
           (\[FormalAlpha]_)), "MatchingConstruct" -> {"SubstitutionLemma", 
          49}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 90} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB]) == 
         (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalC])))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 57}, "Position" -> {2}, 
        "Construct" -> {"CriticalPairLemma", 104}, "Orientation" -> 1, 
        "Rule" -> (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] (\[FormalAlpha]_) -> 
          \[FormalAlpha] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalA])), "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB]) == 
           (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
              (\[FormalA] \[CenterDot] \[FormalC])))], "ConstructSide" -> 2, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 91} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] \[FormalA]) == 
         \[FormalC] \[CenterDot] ((((\[FormalB] \[CenterDot] 
              \[FormalC]) \[CenterDot] \[FormalA]) \[CenterDot] 
            \[FormalA]) \[CenterDot] \[FormalC])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 92}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 105}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] (\[FormalAlpha]_) -> 
          \[FormalAlpha] \[CenterDot] ((\[FormalC] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA]), "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
              \[FormalC]) \[CenterDot] \[FormalA]) == \[FormalC] \[CenterDot] 
            ((((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
               \[FormalA]) \[CenterDot] \[FormalA]) \[CenterDot] 
             \[FormalC])], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 92} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] \[FormalA]) == 
         \[FormalC] \[CenterDot] (\[FormalC] \[CenterDot] 
           (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalC]))))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 91}, "Position" -> {2}, 
        "Construct" -> {"CriticalPairLemma", 104}, "Orientation" -> 1, 
        "Rule" -> (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] (\[FormalAlpha]_) -> 
          \[FormalAlpha] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalA])), "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
              \[FormalC]) \[CenterDot] \[FormalA]) == \[FormalC] \[CenterDot] 
            (\[FormalC] \[CenterDot] (\[FormalA] \[CenterDot] 
              (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
                \[FormalC]))))], "ConstructSide" -> 2, "InputOrientation" -> 
         1, "Side" -> 2|>|>, {"CriticalPairLemma", 106} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalA]) == \[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
            ((\[FormalAlpha] \[CenterDot] (\[FormalA] \[CenterDot] 
               \[FormalA])) \[CenterDot] \[FormalC])))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 103}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalC]_))) -> 
          \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalA]), 
        "Side" -> 1, "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           (\[FormalA]_)) \[CenterDot] (\[FormalC]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 92}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             ((\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] (
                \[FormalA]_))))) -> \[FormalB] \[CenterDot] 
           ((\[FormalC] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalB]), 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"CriticalPairLemma", 107} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] \[FormalB] == 
         (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
           (\[FormalC] \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 79}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              ((\[FormalA]_) \[CenterDot] (\[FormalB]_))))) -> 
          \[FormalA] \[CenterDot] \[FormalB], "Side" -> 1, 
        "Subpattern" -> (\[FormalC]_) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_))), "MatchingConstruct" -> {"SubstitutionLemma", 
          86}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalB]_))) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) -> \[FormalA] \[CenterDot] \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"CriticalPairLemma", 108} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] \[FormalB] == 
         (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
          ((\[FormalC] \[CenterDot] \[FormalA]) \[CenterDot] 
           ((\[FormalC] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 107}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_))) -> (\[FormalA] \[CenterDot] 
            \[FormalA]) \[CenterDot] \[FormalB], "Side" -> 1, 
        "Subpattern" -> ((\[FormalB]_) \[CenterDot] 
           (\[FormalB]_)) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
           (\[FormalA]_)), "MatchingConstruct" -> {"CriticalPairLemma", 62}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) <-> ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 2, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 109} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] ((\[FormalB] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalC])) \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalAlpha])) == 
         (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
          (((\[FormalC] \[CenterDot] \[FormalA]) \[CenterDot] 
            \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 102}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] ((((\[FormalB]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalA]_))) -> 
          (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalC], 
        "Side" -> 1, "Subpattern" -> ((\[FormalB]_) \[CenterDot] 
           (\[FormalA]_)) \[CenterDot] (\[FormalC]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 98}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             ((\[FormalB]_) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalAlpha]_))) -> 
          (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalC], 
        "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"SubstitutionLemma", 93} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] ((\[FormalB] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalC])) \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalAlpha])) == 
         (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalB]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 109}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 102}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((((\[FormalB]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (\[FormalA]_))) -> (\[FormalA] \[CenterDot] 
            \[FormalA]) \[CenterDot] \[FormalC], "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
            ((\[FormalB] \[CenterDot] (\[FormalA] \[CenterDot] 
               \[FormalC])) \[CenterDot] (\[FormalB] \[CenterDot] 
              \[FormalAlpha])) == (\[FormalA] \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalB]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 110} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalA]) == \[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] ((\[FormalC] \[CenterDot] 
             ((\[FormalC] \[CenterDot] \[FormalA]) \[CenterDot] 
              \[FormalAlpha])) \[CenterDot] \[FormalC]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 106}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (((\[FormalAlpha]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
                (\[FormalA]_))) \[CenterDot] (\[FormalC]_)))) -> 
          \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalA]), 
        "Side" -> 1, "Subpattern" -> ((\[FormalAlpha]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
          (\[FormalC]_), "MatchingConstruct" -> {"SubstitutionLemma", 60}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
             (\[FormalAlpha]_))) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2, 2, 2}|>|>, {"SubstitutionLemma", 94} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalA]) == \[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
            ((\[FormalAlpha] \[CenterDot] (\[FormalC] \[CenterDot] 
               \[FormalA])) \[CenterDot] \[FormalC])))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 110}, 
        "Position" -> {2, 2}, "Construct" -> {"CriticalPairLemma", 105}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) \[CenterDot] 
           (\[FormalAlpha]_) -> \[FormalAlpha] \[CenterDot] 
           ((\[FormalC] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA]), 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalA]) == \[FormalA] \[CenterDot] 
            (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
              ((\[FormalAlpha] \[CenterDot] (\[FormalC] \[CenterDot] 
                 \[FormalA])) \[CenterDot] \[FormalC])))], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 95} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalA]) == \[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalC] \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalC]) \[CenterDot] \[FormalAlpha]))))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 94}, 
        "Position" -> {2, 2, 2}, "Construct" -> {"CriticalPairLemma", 105}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) \[CenterDot] 
           (\[FormalAlpha]_) -> \[FormalAlpha] \[CenterDot] 
           ((\[FormalC] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA]), 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalA]) == \[FormalA] \[CenterDot] 
            (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
              (\[FormalC] \[CenterDot] ((\[FormalA] \[CenterDot] 
                 \[FormalC]) \[CenterDot] \[FormalAlpha]))))], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 111} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA]) == 
         \[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
            \[FormalB]) \[CenterDot] ((\[FormalA] \[CenterDot] 
             (\[FormalC] \[CenterDot] \[FormalB])) \[CenterDot] 
            \[FormalAlpha]))], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 95}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
              (((\[FormalA]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] (
                \[FormalAlpha]_))))) -> \[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalA]), "Side" -> 1, 
        "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          ((\[FormalC]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
             (\[FormalAlpha]_)))), "MatchingConstruct" -> 
         {"CriticalPairLemma", 108}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalC]_))) -> 
          (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalC], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 96} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
            \[FormalB]) \[CenterDot] ((\[FormalA] \[CenterDot] 
             (\[FormalC] \[CenterDot] \[FormalB])) \[CenterDot] 
            \[FormalAlpha]))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 111}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 63}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalA]_)) -> 
          \[FormalA] \[CenterDot] \[FormalB], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
           \[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
              \[FormalB]) \[CenterDot] ((\[FormalA] \[CenterDot] (
                \[FormalC] \[CenterDot] \[FormalB])) \[CenterDot] 
              \[FormalAlpha]))], "ConstructSide" -> 1, "InputOrientation" -> 
         1, "Side" -> 1|>|>, {"CriticalPairLemma", 112} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalA] \[CenterDot] ((\[FormalC] \[CenterDot] 
            (\[FormalA] \[CenterDot] (\[FormalAlpha] \[CenterDot] 
              \[FormalB]))) \[CenterDot] (\[FormalB] \[CenterDot] 
            \[FormalB]))], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 
          96}, "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] (
                \[FormalB]_))) \[CenterDot] (\[FormalAlpha]_))) -> 
          \[FormalA] \[CenterDot] \[FormalB], "Side" -> 1, 
        "Subpattern" -> ((\[FormalB]_) \[CenterDot] 
           (\[FormalB]_)) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalB]_))) \[CenterDot] 
           (\[FormalAlpha]_)), "MatchingConstruct" -> {"SubstitutionLemma", 
          65}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (((\[FormalC]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
             (\[FormalA]_))) -> (\[FormalC] \[CenterDot] 
            \[FormalB]) \[CenterDot] \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 113} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] \[FormalA])) == 
         \[FormalA] \[CenterDot] ((\[FormalC] \[CenterDot] 
            \[FormalC]) \[CenterDot] ((\[FormalB] \[CenterDot] 
             (\[FormalC] \[CenterDot] \[FormalA])) \[CenterDot] 
            (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
              \[FormalA]))))], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 112}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             ((\[FormalA]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] (
                \[FormalAlpha]_)))) \[CenterDot] 
            ((\[FormalAlpha]_) \[CenterDot] (\[FormalAlpha]_))) -> 
          \[FormalA] \[CenterDot] \[FormalAlpha], "Side" -> 1, 
        "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
            (\[FormalAlpha]_))), "MatchingConstruct" -> {"SubstitutionLemma", 
          85}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
              ((\[FormalA]_) \[CenterDot] (\[FormalB]_))))) -> 
          \[FormalA] \[CenterDot] \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2, 1}|>|>, {"SubstitutionLemma", 97} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] \[FormalA])) == 
         \[FormalA] \[CenterDot] ((\[FormalC] \[CenterDot] 
            \[FormalC]) \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 113}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 93}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (\[FormalC]_))) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalAlpha]_))) -> (\[FormalA] \[CenterDot] 
            \[FormalA]) \[CenterDot] \[FormalB], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             (\[FormalC] \[CenterDot] \[FormalA])) == \[FormalA] \[CenterDot] 
            ((\[FormalC] \[CenterDot] \[FormalC]) \[CenterDot] \[FormalB])], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 114} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalB])) \[CenterDot] \[FormalC]) == 
         \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB])) \[CenterDot] \[FormalC])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 97}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_))) <-> (\[FormalA]_) \[CenterDot] 
           (((\[FormalC]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
            (\[FormalB]_)), "Side" -> 1, "Subpattern" -> 
         (\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
           (\[FormalA]_)), "MatchingConstruct" -> {"CriticalPairLemma", 97}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalC]_)) <-> 
          ((\[FormalC]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalB]_))) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 98} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalC]) == \[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
             \[FormalB])) \[CenterDot] \[FormalC])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 114}, 
        "Position" -> {2, 1}, "Construct" -> {"SubstitutionLemma", 44}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)) -> \[FormalB], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalC]) == \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              (\[FormalA] \[CenterDot] \[FormalB])) \[CenterDot] 
             \[FormalC])], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"CriticalPairLemma", 115} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
           \[FormalAlpha]) == \[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
             \[FormalB])) \[CenterDot] \[FormalAlpha])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 98}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (\[FormalB]_))) \[CenterDot] (\[FormalC]_)) -> 
          \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalC]), 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"SubstitutionLemma", 61}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) -> \[FormalA] \[CenterDot] \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {2, 1, 2}|>|>, 
    {"SubstitutionLemma", 99} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
           \[FormalAlpha]) == \[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalAlpha])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 115}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             ((\[FormalA]_) \[CenterDot] (\[FormalB]_))) \[CenterDot] 
            (\[FormalC]_)) -> \[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC]), "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (((\[FormalA] \[CenterDot] 
               \[FormalB]) \[CenterDot] (\[FormalB] \[CenterDot] 
               \[FormalC])) \[CenterDot] \[FormalAlpha]) == 
           \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalAlpha])], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 116} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalC])) \[CenterDot] 
           \[FormalAlpha]) == \[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
             \[FormalC])) \[CenterDot] \[FormalAlpha])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 98}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (\[FormalB]_))) \[CenterDot] (\[FormalC]_)) -> 
          \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalC]), 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"SubstitutionLemma", 58}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (\[FormalC]_))) -> \[FormalA] \[CenterDot] \[FormalC], 
        "MatchingSide" -> 1, "Position" -> {2, 1, 2}|>|>, 
    {"SubstitutionLemma", 100} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalC])) \[CenterDot] 
           \[FormalAlpha]) == \[FormalA] \[CenterDot] 
          (\[FormalC] \[CenterDot] \[FormalAlpha])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 116}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             ((\[FormalA]_) \[CenterDot] (\[FormalB]_))) \[CenterDot] 
            (\[FormalC]_)) -> \[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC]), "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (((\[FormalB] \[CenterDot] 
               \[FormalC]) \[CenterDot] (\[FormalA] \[CenterDot] 
               \[FormalC])) \[CenterDot] \[FormalAlpha]) == 
           \[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] \[FormalAlpha])], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 117} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalC]) == \[FormalA] \[CenterDot] 
          (\[FormalC] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB])))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 99}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
             ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) \[CenterDot] 
            (\[FormalAlpha]_)) -> \[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalAlpha]), "Side" -> 1, 
        "Subpattern" -> (((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalC]_))) \[CenterDot] (\[FormalAlpha]_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 76}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) <-> ((\[FormalB]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
              (\[FormalC]_)))) \[CenterDot] (\[FormalA]_), 
        "MatchingSide" -> 2, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 118} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalB] \[CenterDot] 
           (\[FormalB] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalC]))) == 
         (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalC]))) \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 64}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) <-> 
          ((\[FormalB]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalA]_), "Side" -> 1, "Subpattern" -> 
         (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 117}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             ((\[FormalA]_) \[CenterDot] (\[FormalC]_)))) -> 
          \[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] \[FormalB]), 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 101} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CenterDot] 
           (\[FormalB] \[CenterDot] (\[FormalA] \[CenterDot] 
             \[FormalC]))) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalC] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 118}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 84}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             ((\[FormalA]_) \[CenterDot] (\[FormalC]_)))) -> \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalA] == 
           (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
              (\[FormalA] \[CenterDot] \[FormalC]))) \[CenterDot] 
            (\[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] \[FormalB]))], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 102} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
          (((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalC]) \[CenterDot] \[FormalC])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 101}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 105}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] (\[FormalAlpha]_) -> 
          \[FormalAlpha] \[CenterDot] ((\[FormalC] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA]), "OutputExpression" -> 
         HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
            (((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
              \[FormalC]) \[CenterDot] \[FormalC])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 103} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
          (\[FormalC] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalA])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 102}, "Position" -> {2}, 
        "Construct" -> {"CriticalPairLemma", 104}, "Orientation" -> 1, 
        "Rule" -> (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] (\[FormalAlpha]_) -> 
          \[FormalAlpha] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalA])), "OutputExpression" -> 
         HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
            (\[FormalC] \[CenterDot] (\[FormalC] \[CenterDot] 
              (\[FormalB] \[CenterDot] \[FormalA])))], "ConstructSide" -> 2, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 119} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalC])) == 
         ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
           (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalC])))) \[CenterDot] 
          \[FormalC]], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 37}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> ((\[FormalC]_) \[CenterDot] 
           (\[FormalA]_)) \[CenterDot] (\[FormalB]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 103}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) \[CenterDot] 
           ((\[FormalC]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             ((\[FormalB]_) \[CenterDot] (\[FormalA]_)))) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 104} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalC])) == 
         ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalA])) \[CenterDot] \[FormalC]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 119}, "Position" -> {1}, 
        "Construct" -> {"SubstitutionLemma", 90}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             ((\[FormalA]_) \[CenterDot] (\[FormalC]_)))) -> 
          (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB]), "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalC])) == 
           ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalA])) \[CenterDot] \[FormalC]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 105} -> 
     <|"Statement" -> HoldForm[
        (((\[FormalD] \[CenterDot] \[FormalD]) \[CenterDot] 
            (\[FormalE] \[CenterDot] \[FormalE])) \[CenterDot] 
           ((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
            (\[FormalD] \[CenterDot] \[FormalD]))) \[CenterDot] 
          (\[FormalF] \[CenterDot] \[FormalF]) == 
         (\[FormalD] \[CenterDot] \[FormalD]) \[CenterDot] 
          (((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
            (\[FormalF] \[CenterDot] \[FormalF])) \[CenterDot] 
           ((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
            (\[FormalF] \[CenterDot] \[FormalF])))], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Position" -> {1, 2}, 
        "Construct" -> {"SubstitutionLemma", 49}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(((\[FormalD] \[CenterDot] \[FormalD]) \[CenterDot] 
              (\[FormalE] \[CenterDot] \[FormalE])) \[CenterDot] 
             ((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
              (\[FormalD] \[CenterDot] \[FormalD]))) \[CenterDot] 
            (\[FormalF] \[CenterDot] \[FormalF]) == 
           (\[FormalD] \[CenterDot] \[FormalD]) \[CenterDot] 
            (((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
              (\[FormalF] \[CenterDot] \[FormalF])) \[CenterDot] 
             ((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
              (\[FormalF] \[CenterDot] \[FormalF])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 106} -> 
     <|"Statement" -> HoldForm[
        (((\[FormalD] \[CenterDot] \[FormalD]) \[CenterDot] 
            (\[FormalE] \[CenterDot] \[FormalE])) \[CenterDot] 
           ((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
            (\[FormalD] \[CenterDot] \[FormalD]))) \[CenterDot] 
          (\[FormalF] \[CenterDot] \[FormalF]) == 
         (((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
            (\[FormalF] \[CenterDot] \[FormalF])) \[CenterDot] 
           ((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
            (\[FormalF] \[CenterDot] \[FormalF]))) \[CenterDot] 
          (\[FormalD] \[CenterDot] \[FormalD])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 105}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 49}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(((\[FormalD] \[CenterDot] \[FormalD]) \[CenterDot] 
              (\[FormalE] \[CenterDot] \[FormalE])) \[CenterDot] 
             ((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
              (\[FormalD] \[CenterDot] \[FormalD]))) \[CenterDot] 
            (\[FormalF] \[CenterDot] \[FormalF]) == 
           (((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
              (\[FormalF] \[CenterDot] \[FormalF])) \[CenterDot] 
             ((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
              (\[FormalF] \[CenterDot] \[FormalF]))) \[CenterDot] 
            (\[FormalD] \[CenterDot] \[FormalD])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 107} -> 
     <|"Statement" -> HoldForm[
        (((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
            (\[FormalD] \[CenterDot] \[FormalD])) \[CenterDot] 
           ((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
            (\[FormalD] \[CenterDot] \[FormalD]))) \[CenterDot] 
          (\[FormalF] \[CenterDot] \[FormalF]) == 
         (((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
            (\[FormalF] \[CenterDot] \[FormalF])) \[CenterDot] 
           ((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
            (\[FormalF] \[CenterDot] \[FormalF]))) \[CenterDot] 
          (\[FormalD] \[CenterDot] \[FormalD])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 106}, 
        "Position" -> {1, 1}, "Construct" -> {"SubstitutionLemma", 49}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (\[FormalB]_) -> \[FormalB] \[CenterDot] \[FormalA], 
        "OutputExpression" -> HoldForm[
          (((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
              (\[FormalD] \[CenterDot] \[FormalD])) \[CenterDot] 
             ((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
              (\[FormalD] \[CenterDot] \[FormalD]))) \[CenterDot] 
            (\[FormalF] \[CenterDot] \[FormalF]) == 
           (((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
              (\[FormalF] \[CenterDot] \[FormalF])) \[CenterDot] 
             ((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
              (\[FormalF] \[CenterDot] \[FormalF]))) \[CenterDot] 
            (\[FormalD] \[CenterDot] \[FormalD])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 108} -> 
     <|"Statement" -> HoldForm[(\[FormalF] \[CenterDot] 
           \[FormalF]) \[CenterDot] (((\[FormalE] \[CenterDot] 
             \[FormalE]) \[CenterDot] (\[FormalD] \[CenterDot] 
             \[FormalD])) \[CenterDot] ((\[FormalE] \[CenterDot] 
             \[FormalE]) \[CenterDot] (\[FormalD] \[CenterDot] 
             \[FormalD]))) == (((\[FormalE] \[CenterDot] 
             \[FormalE]) \[CenterDot] (\[FormalF] \[CenterDot] 
             \[FormalF])) \[CenterDot] ((\[FormalE] \[CenterDot] 
             \[FormalE]) \[CenterDot] (\[FormalF] \[CenterDot] 
             \[FormalF]))) \[CenterDot] (\[FormalD] \[CenterDot] 
           \[FormalD])], "Proof" -> <|"Input" -> {"SubstitutionLemma", 107}, 
        "Position" -> {}, "Construct" -> {"SubstitutionLemma", 49}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (\[FormalB]_) -> \[FormalB] \[CenterDot] \[FormalA], 
        "OutputExpression" -> HoldForm[(\[FormalF] \[CenterDot] 
             \[FormalF]) \[CenterDot] (((\[FormalE] \[CenterDot] 
               \[FormalE]) \[CenterDot] (\[FormalD] \[CenterDot] 
               \[FormalD])) \[CenterDot] ((\[FormalE] \[CenterDot] 
               \[FormalE]) \[CenterDot] (\[FormalD] \[CenterDot] 
               \[FormalD]))) == (((\[FormalE] \[CenterDot] 
               \[FormalE]) \[CenterDot] (\[FormalF] \[CenterDot] 
               \[FormalF])) \[CenterDot] ((\[FormalE] \[CenterDot] 
               \[FormalE]) \[CenterDot] (\[FormalF] \[CenterDot] 
               \[FormalF]))) \[CenterDot] (\[FormalD] \[CenterDot] 
             \[FormalD])], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 109} -> 
     <|"Statement" -> HoldForm[(\[FormalF] \[CenterDot] 
           \[FormalF]) \[CenterDot] (((\[FormalE] \[CenterDot] 
             \[FormalE]) \[CenterDot] (\[FormalD] \[CenterDot] 
             \[FormalD])) \[CenterDot] ((\[FormalE] \[CenterDot] 
             \[FormalE]) \[CenterDot] (\[FormalD] \[CenterDot] 
             \[FormalD]))) == (((\[FormalE] \[CenterDot] 
             \[FormalE]) \[CenterDot] (\[FormalF] \[CenterDot] 
             \[FormalF])) \[CenterDot] ((\[FormalE] \[CenterDot] 
             \[FormalE]) \[CenterDot] (\[FormalF] \[CenterDot] 
             \[FormalF]))) \[CenterDot] 
          ((((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
             (\[FormalF] \[CenterDot] \[FormalF])) \[CenterDot] 
            ((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
             (\[FormalF] \[CenterDot] \[FormalF]))) \[CenterDot] 
           \[FormalD])], "Proof" -> <|"Input" -> {"SubstitutionLemma", 108}, 
        "Position" -> {}, "Construct" -> {"CriticalPairLemma", 56}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalB]_)) -> 
          \[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB]), 
        "OutputExpression" -> HoldForm[(\[FormalF] \[CenterDot] 
             \[FormalF]) \[CenterDot] (((\[FormalE] \[CenterDot] 
               \[FormalE]) \[CenterDot] (\[FormalD] \[CenterDot] 
               \[FormalD])) \[CenterDot] ((\[FormalE] \[CenterDot] 
               \[FormalE]) \[CenterDot] (\[FormalD] \[CenterDot] 
               \[FormalD]))) == (((\[FormalE] \[CenterDot] 
               \[FormalE]) \[CenterDot] (\[FormalF] \[CenterDot] 
               \[FormalF])) \[CenterDot] ((\[FormalE] \[CenterDot] 
               \[FormalE]) \[CenterDot] (\[FormalF] \[CenterDot] 
               \[FormalF]))) \[CenterDot] ((((\[FormalE] \[CenterDot] 
                \[FormalE]) \[CenterDot] (\[FormalF] \[CenterDot] 
                \[FormalF])) \[CenterDot] ((\[FormalE] \[CenterDot] 
                \[FormalE]) \[CenterDot] (\[FormalF] \[CenterDot] 
                \[FormalF]))) \[CenterDot] \[FormalD])], 
        "ConstructSide" -> 2, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 110} -> 
     <|"Statement" -> HoldForm[(\[FormalF] \[CenterDot] 
           \[FormalF]) \[CenterDot] ((\[FormalF] \[CenterDot] 
            \[FormalF]) \[CenterDot] ((\[FormalE] \[CenterDot] 
             \[FormalE]) \[CenterDot] (\[FormalD] \[CenterDot] 
             \[FormalD]))) == (((\[FormalE] \[CenterDot] 
             \[FormalE]) \[CenterDot] (\[FormalF] \[CenterDot] 
             \[FormalF])) \[CenterDot] ((\[FormalE] \[CenterDot] 
             \[FormalE]) \[CenterDot] (\[FormalF] \[CenterDot] 
             \[FormalF]))) \[CenterDot] 
          ((((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
             (\[FormalF] \[CenterDot] \[FormalF])) \[CenterDot] 
            ((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
             (\[FormalF] \[CenterDot] \[FormalF]))) \[CenterDot] 
           \[FormalD])], "Proof" -> <|"Input" -> {"SubstitutionLemma", 109}, 
        "Position" -> {}, "Construct" -> {"CriticalPairLemma", 56}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalB]_)) -> 
          \[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB]), 
        "OutputExpression" -> HoldForm[(\[FormalF] \[CenterDot] 
             \[FormalF]) \[CenterDot] ((\[FormalF] \[CenterDot] 
              \[FormalF]) \[CenterDot] ((\[FormalE] \[CenterDot] 
               \[FormalE]) \[CenterDot] (\[FormalD] \[CenterDot] 
               \[FormalD]))) == (((\[FormalE] \[CenterDot] 
               \[FormalE]) \[CenterDot] (\[FormalF] \[CenterDot] 
               \[FormalF])) \[CenterDot] ((\[FormalE] \[CenterDot] 
               \[FormalE]) \[CenterDot] (\[FormalF] \[CenterDot] 
               \[FormalF]))) \[CenterDot] ((((\[FormalE] \[CenterDot] 
                \[FormalE]) \[CenterDot] (\[FormalF] \[CenterDot] 
                \[FormalF])) \[CenterDot] ((\[FormalE] \[CenterDot] 
                \[FormalE]) \[CenterDot] (\[FormalF] \[CenterDot] 
                \[FormalF]))) \[CenterDot] \[FormalD])], 
        "ConstructSide" -> 2, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 111} -> 
     <|"Statement" -> HoldForm[(\[FormalF] \[CenterDot] 
           \[FormalF]) \[CenterDot] ((\[FormalF] \[CenterDot] 
            \[FormalF]) \[CenterDot] ((\[FormalE] \[CenterDot] 
             \[FormalE]) \[CenterDot] (\[FormalD] \[CenterDot] 
             \[FormalD]))) == (\[FormalF] \[CenterDot] 
           \[FormalF]) \[CenterDot] ((\[FormalF] \[CenterDot] 
            \[FormalF]) \[CenterDot] ((\[FormalE] \[CenterDot] 
             \[FormalE]) \[CenterDot] ((((\[FormalE] \[CenterDot] 
                \[FormalE]) \[CenterDot] (\[FormalF] \[CenterDot] 
                \[FormalF])) \[CenterDot] ((\[FormalE] \[CenterDot] 
                \[FormalE]) \[CenterDot] (\[FormalF] \[CenterDot] 
                \[FormalF]))) \[CenterDot] \[FormalD])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 110}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 104}, "Orientation" -> -1, 
        "Rule" -> (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalB]_))) \[CenterDot] 
           (\[FormalC]_) -> \[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalC])), "OutputExpression" -> 
         HoldForm[(\[FormalF] \[CenterDot] \[FormalF]) \[CenterDot] 
            ((\[FormalF] \[CenterDot] \[FormalF]) \[CenterDot] 
             ((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
              (\[FormalD] \[CenterDot] \[FormalD]))) == 
           (\[FormalF] \[CenterDot] \[FormalF]) \[CenterDot] 
            ((\[FormalF] \[CenterDot] \[FormalF]) \[CenterDot] 
             ((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
              ((((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
                 (\[FormalF] \[CenterDot] \[FormalF])) \[CenterDot] 
                ((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
                 (\[FormalF] \[CenterDot] \[FormalF]))) \[CenterDot] 
               \[FormalD])))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 112} -> 
     <|"Statement" -> HoldForm[(\[FormalF] \[CenterDot] 
           \[FormalF]) \[CenterDot] ((\[FormalF] \[CenterDot] 
            \[FormalF]) \[CenterDot] ((\[FormalE] \[CenterDot] 
             \[FormalE]) \[CenterDot] (\[FormalD] \[CenterDot] 
             \[FormalD]))) == (\[FormalF] \[CenterDot] 
           \[FormalF]) \[CenterDot] ((\[FormalF] \[CenterDot] 
            \[FormalF]) \[CenterDot] ((\[FormalE] \[CenterDot] 
             \[FormalE]) \[CenterDot] ((\[FormalF] \[CenterDot] 
              \[FormalF]) \[CenterDot] \[FormalD])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 111}, 
        "Position" -> {2, 2}, "Construct" -> {"SubstitutionLemma", 100}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
             ((\[FormalA]_) \[CenterDot] (\[FormalC]_))) \[CenterDot] 
            (\[FormalAlpha]_)) -> \[FormalA] \[CenterDot] 
           (\[FormalC] \[CenterDot] \[FormalAlpha]), "OutputExpression" -> 
         HoldForm[(\[FormalF] \[CenterDot] \[FormalF]) \[CenterDot] 
            ((\[FormalF] \[CenterDot] \[FormalF]) \[CenterDot] 
             ((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
              (\[FormalD] \[CenterDot] \[FormalD]))) == 
           (\[FormalF] \[CenterDot] \[FormalF]) \[CenterDot] 
            ((\[FormalF] \[CenterDot] \[FormalF]) \[CenterDot] 
             ((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
              ((\[FormalF] \[CenterDot] \[FormalF]) \[CenterDot] 
               \[FormalD])))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 113} -> 
     <|"Statement" -> HoldForm[(\[FormalF] \[CenterDot] 
           \[FormalF]) \[CenterDot] ((\[FormalF] \[CenterDot] 
            \[FormalF]) \[CenterDot] ((\[FormalE] \[CenterDot] 
             \[FormalE]) \[CenterDot] (\[FormalD] \[CenterDot] 
             \[FormalD]))) == (\[FormalF] \[CenterDot] 
           \[FormalF]) \[CenterDot] ((\[FormalF] \[CenterDot] 
            \[FormalF]) \[CenterDot] ((\[FormalE] \[CenterDot] 
             \[FormalE]) \[CenterDot] (\[FormalD] \[CenterDot] 
             (\[FormalF] \[CenterDot] \[FormalF]))))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 112}, 
        "Position" -> {2, 2}, "Construct" -> {"CriticalPairLemma", 100}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
            (\[FormalAlpha]_)) -> (\[FormalB] \[CenterDot] 
            \[FormalA]) \[CenterDot] (\[FormalAlpha] \[CenterDot] 
            \[FormalC]), "OutputExpression" -> HoldForm[
          (\[FormalF] \[CenterDot] \[FormalF]) \[CenterDot] 
            ((\[FormalF] \[CenterDot] \[FormalF]) \[CenterDot] 
             ((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
              (\[FormalD] \[CenterDot] \[FormalD]))) == 
           (\[FormalF] \[CenterDot] \[FormalF]) \[CenterDot] 
            ((\[FormalF] \[CenterDot] \[FormalF]) \[CenterDot] 
             ((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
              (\[FormalD] \[CenterDot] (\[FormalF] \[CenterDot] 
                \[FormalF]))))], "ConstructSide" -> 1, "InputOrientation" -> 
         1, "Side" -> 2|>|>, {"SubstitutionLemma", 114} -> 
     <|"Statement" -> HoldForm[(\[FormalF] \[CenterDot] 
           \[FormalF]) \[CenterDot] ((\[FormalF] \[CenterDot] 
            \[FormalF]) \[CenterDot] ((\[FormalE] \[CenterDot] 
             \[FormalE]) \[CenterDot] (\[FormalD] \[CenterDot] 
             \[FormalD]))) == (\[FormalF] \[CenterDot] 
           \[FormalF]) \[CenterDot] ((\[FormalF] \[CenterDot] 
            \[FormalF]) \[CenterDot] ((\[FormalD] \[CenterDot] 
             \[FormalD]) \[CenterDot] (\[FormalE] \[CenterDot] 
             \[FormalE])))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 113}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 97}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalA]_))) -> 
          \[FormalA] \[CenterDot] ((\[FormalC] \[CenterDot] 
             \[FormalC]) \[CenterDot] \[FormalB]), "OutputExpression" -> 
         HoldForm[(\[FormalF] \[CenterDot] \[FormalF]) \[CenterDot] 
            ((\[FormalF] \[CenterDot] \[FormalF]) \[CenterDot] 
             ((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
              (\[FormalD] \[CenterDot] \[FormalD]))) == 
           (\[FormalF] \[CenterDot] \[FormalF]) \[CenterDot] 
            ((\[FormalF] \[CenterDot] \[FormalF]) \[CenterDot] 
             ((\[FormalD] \[CenterDot] \[FormalD]) \[CenterDot] 
              (\[FormalE] \[CenterDot] \[FormalE])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"Conclusion", 1} -> <|"Statement" -> 
       HoldForm[(\[FormalF] \[CenterDot] \[FormalF]) \[CenterDot] 
          ((\[FormalF] \[CenterDot] \[FormalF]) \[CenterDot] 
           ((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
            (\[FormalD] \[CenterDot] \[FormalD]))) == 
         (\[FormalF] \[CenterDot] \[FormalF]) \[CenterDot] 
          ((\[FormalF] \[CenterDot] \[FormalF]) \[CenterDot] 
           ((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
            (\[FormalD] \[CenterDot] \[FormalD])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 114}, "Position" -> {2}, 
        "Construct" -> {"CriticalPairLemma", 100}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalC]_) \[CenterDot] (\[FormalAlpha]_)) -> 
          (\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
           (\[FormalAlpha] \[CenterDot] \[FormalC]), "OutputExpression" -> 
         HoldForm[(\[FormalF] \[CenterDot] \[FormalF]) \[CenterDot] 
            ((\[FormalF] \[CenterDot] \[FormalF]) \[CenterDot] 
             ((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
              (\[FormalD] \[CenterDot] \[FormalD]))) == 
           (\[FormalF] \[CenterDot] \[FormalF]) \[CenterDot] 
            ((\[FormalF] \[CenterDot] \[FormalF]) \[CenterDot] 
             ((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
              (\[FormalD] \[CenterDot] \[FormalD])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>}|>]
