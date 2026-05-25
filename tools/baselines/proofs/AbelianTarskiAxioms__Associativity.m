ProofObject["EquationalLogic", {ForAll[{\[FormalA], \[FormalB], \[FormalC]}, 
   \[FormalA] \[CircleDot] ((\[FormalA] \[CircleDot] \[FormalA]) \[CircleDot] 
      (\[FormalB] \[CircleDot] ((\[FormalA] \[CircleDot] 
         \[FormalA]) \[CircleDot] \[FormalC]))) == 
    (\[FormalA] \[CircleDot] ((\[FormalA] \[CircleDot] 
        \[FormalA]) \[CircleDot] \[FormalB])) \[CircleDot] 
     ((\[FormalA] \[CircleDot] \[FormalA]) \[CircleDot] \[FormalC])]}, 
 {ForAll[{\[FormalA], \[FormalB], \[FormalC]}, 
   \[FormalA] \[CircleDot] (\[FormalB] \[CircleDot] (\[FormalC] \[CircleDot] 
       (\[FormalA] \[CircleDot] \[FormalB]))) == \[FormalC]]}, 
 <|"Variables" -> {\[FormalA], \[FormalB], \[FormalC], \[FormalD], 
    \[FormalE], \[FormalF], \[FormalAlpha]}, "Constants" -> {}, 
  "Proof" -> {{"Axiom", 1} -> <|"Statement" -> 
       HoldForm[\[FormalA] == \[FormalB] \[CircleDot] 
          (\[FormalC] \[CircleDot] (\[FormalA] \[CircleDot] 
            (\[FormalB] \[CircleDot] \[FormalC])))], "Proof" -> <||>|>, 
    {"Hypothesis", 1} -> <|"Statement" -> 
       HoldForm[(\[FormalD] \[CircleDot] ((\[FormalD] \[CircleDot] 
             \[FormalD]) \[CircleDot] \[FormalE])) \[CircleDot] 
          ((\[FormalD] \[CircleDot] \[FormalD]) \[CircleDot] \[FormalF]) == 
         \[FormalD] \[CircleDot] ((\[FormalD] \[CircleDot] 
            \[FormalD]) \[CircleDot] (\[FormalE] \[CircleDot] 
            ((\[FormalD] \[CircleDot] \[FormalD]) \[CircleDot] 
             \[FormalF])))], "Proof" -> <||>|>, {"CriticalPairLemma", 1} -> 
     <|"Statement" -> HoldForm[\[FormalA] == \[FormalB] \[CircleDot] 
          ((\[FormalC] \[CircleDot] (\[FormalA] \[CircleDot] 
             \[FormalB])) \[CircleDot] \[FormalC])], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CircleDot] ((\[FormalB]_) \[CircleDot] 
            ((\[FormalC]_) \[CircleDot] ((\[FormalA]_) \[CircleDot] 
              (\[FormalB]_)))) -> \[FormalC], "Side" -> 1, 
        "Subpattern" -> (\[FormalC]_) \[CircleDot] 
          ((\[FormalA]_) \[CircleDot] (\[FormalB]_)), "MatchingConstruct" -> 
         {"Axiom", 1}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleDot] ((\[FormalB]_) \[CircleDot] 
            ((\[FormalC]_) \[CircleDot] ((\[FormalA]_) \[CircleDot] 
              (\[FormalB]_)))) -> \[FormalC], "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 2} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CircleDot] 
           \[FormalB]) \[CircleDot] (\[FormalC] \[CircleDot] \[FormalB]) == 
         \[FormalA] \[CircleDot] \[FormalC]], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CircleDot] ((\[FormalB]_) \[CircleDot] 
            ((\[FormalC]_) \[CircleDot] ((\[FormalA]_) \[CircleDot] 
              (\[FormalB]_)))) -> \[FormalC], "Side" -> 1, 
        "Subpattern" -> (\[FormalB]_) \[CircleDot] 
          ((\[FormalC]_) \[CircleDot] ((\[FormalA]_) \[CircleDot] 
            (\[FormalB]_))), "MatchingConstruct" -> {"CriticalPairLemma", 1}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleDot] (((\[FormalB]_) \[CircleDot] 
             ((\[FormalC]_) \[CircleDot] (\[FormalA]_))) \[CircleDot] 
            (\[FormalB]_)) -> \[FormalC], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 3} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleDot] \[FormalB] == 
         \[FormalC] \[CircleDot] (\[FormalB] \[CircleDot] 
           (\[FormalA] \[CircleDot] \[FormalC]))], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CircleDot] ((\[FormalB]_) \[CircleDot] 
            ((\[FormalC]_) \[CircleDot] ((\[FormalA]_) \[CircleDot] 
              (\[FormalB]_)))) -> \[FormalC], "Side" -> 1, 
        "Subpattern" -> (\[FormalC]_) \[CircleDot] 
          ((\[FormalA]_) \[CircleDot] (\[FormalB]_)), "MatchingConstruct" -> 
         {"CriticalPairLemma", 2}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> ((\[FormalA]_) \[CircleDot] 
            (\[FormalB]_)) \[CircleDot] ((\[FormalC]_) \[CircleDot] 
            (\[FormalB]_)) -> \[FormalA] \[CircleDot] \[FormalC], 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"CriticalPairLemma", 4} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleDot] 
          ((\[FormalA] \[CircleDot] \[FormalB]) \[CircleDot] 
           (\[FormalC] \[CircleDot] \[FormalB])) == \[FormalC]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 3}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CircleDot] 
           ((\[FormalB]_) \[CircleDot] ((\[FormalC]_) \[CircleDot] 
             (\[FormalA]_))) -> \[FormalC] \[CircleDot] \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CircleDot] 
          ((\[FormalB]_) \[CircleDot] ((\[FormalC]_) \[CircleDot] 
            (\[FormalA]_))), "MatchingConstruct" -> {"CriticalPairLemma", 1}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleDot] (((\[FormalB]_) \[CircleDot] 
             ((\[FormalC]_) \[CircleDot] (\[FormalA]_))) \[CircleDot] 
            (\[FormalB]_)) -> \[FormalC], "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleDot] 
          (\[FormalA] \[CircleDot] \[FormalB]) == \[FormalB]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 4}, "Position" -> {2}, 
        "Construct" -> {"CriticalPairLemma", 2}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CircleDot] (\[FormalB]_)) \[CircleDot] 
           ((\[FormalC]_) \[CircleDot] (\[FormalB]_)) -> 
          \[FormalA] \[CircleDot] \[FormalC], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CircleDot] (\[FormalA] \[CircleDot] 
             \[FormalB]) == \[FormalB]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, {"CriticalPairLemma", 5} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CircleDot] 
           (\[FormalB] \[CircleDot] \[FormalC])) \[CircleDot] \[FormalC] == 
         \[FormalA] \[CircleDot] \[FormalB]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 3}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CircleDot] 
           ((\[FormalB]_) \[CircleDot] ((\[FormalC]_) \[CircleDot] 
             (\[FormalA]_))) -> \[FormalC] \[CircleDot] \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CircleDot] 
          ((\[FormalC]_) \[CircleDot] (\[FormalA]_)), "MatchingConstruct" -> 
         {"CriticalPairLemma", 1}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CircleDot] 
           (((\[FormalB]_) \[CircleDot] ((\[FormalC]_) \[CircleDot] 
              (\[FormalA]_))) \[CircleDot] (\[FormalB]_)) -> \[FormalC], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 6} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleDot] \[FormalA] == 
         \[FormalB] \[CircleDot] \[FormalB]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 3}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CircleDot] 
           ((\[FormalB]_) \[CircleDot] ((\[FormalC]_) \[CircleDot] 
             (\[FormalA]_))) -> \[FormalC] \[CircleDot] \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CircleDot] 
          ((\[FormalC]_) \[CircleDot] (\[FormalA]_)), "MatchingConstruct" -> 
         {"SubstitutionLemma", 1}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CircleDot] 
           ((\[FormalA]_) \[CircleDot] (\[FormalB]_)) -> \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 7} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleDot] \[FormalB] == 
         (\[FormalC] \[CircleDot] \[FormalC]) \[CircleDot] 
          (\[FormalB] \[CircleDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 2}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CircleDot] 
            (\[FormalB]_)) \[CircleDot] ((\[FormalC]_) \[CircleDot] 
            (\[FormalB]_)) -> \[FormalA] \[CircleDot] \[FormalC], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CircleDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"CriticalPairLemma", 6}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleDot] (\[FormalA]_) <-> 
          (\[FormalB]_) \[CircleDot] (\[FormalB]_), "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 8} -> 
     <|"Statement" -> HoldForm[\[FormalA] == \[FormalA] \[CircleDot] 
          (\[FormalB] \[CircleDot] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 1}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CircleDot] 
           ((\[FormalA]_) \[CircleDot] (\[FormalB]_)) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CircleDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"CriticalPairLemma", 6}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleDot] (\[FormalA]_) <-> 
          (\[FormalB]_) \[CircleDot] (\[FormalB]_), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 9} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CircleDot] 
           (\[FormalB] \[CircleDot] \[FormalC])) \[CircleDot] 
          \[FormalAlpha] == (\[FormalA] \[CircleDot] \[FormalB]) \[CircleDot] 
          (\[FormalAlpha] \[CircleDot] \[FormalC])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 2}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CircleDot] 
            (\[FormalB]_)) \[CircleDot] ((\[FormalC]_) \[CircleDot] 
            (\[FormalB]_)) -> \[FormalA] \[CircleDot] \[FormalC], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CircleDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"CriticalPairLemma", 5}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((\[FormalA]_) \[CircleDot] ((\[FormalB]_) \[CircleDot] 
             (\[FormalC]_))) \[CircleDot] (\[FormalC]_) -> 
          \[FormalA] \[CircleDot] \[FormalB], "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[(\[FormalD] \[CircleDot] 
           ((\[FormalD] \[CircleDot] \[FormalD]) \[CircleDot] 
            \[FormalE])) \[CircleDot] ((\[FormalD] \[CircleDot] 
            \[FormalD]) \[CircleDot] \[FormalF]) == \[FormalD] \[CircleDot] 
          (((\[FormalD] \[CircleDot] \[FormalD]) \[CircleDot] 
            \[FormalF]) \[CircleDot] \[FormalE])], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Position" -> {2}, 
        "Construct" -> {"CriticalPairLemma", 7}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CircleDot] (\[FormalA]_)) \[CircleDot] 
           ((\[FormalB]_) \[CircleDot] (\[FormalC]_)) -> 
          \[FormalC] \[CircleDot] \[FormalB], "OutputExpression" -> 
         HoldForm[(\[FormalD] \[CircleDot] ((\[FormalD] \[CircleDot] 
               \[FormalD]) \[CircleDot] \[FormalE])) \[CircleDot] 
            ((\[FormalD] \[CircleDot] \[FormalD]) \[CircleDot] \[FormalF]) == 
           \[FormalD] \[CircleDot] (((\[FormalD] \[CircleDot] 
               \[FormalD]) \[CircleDot] \[FormalF]) \[CircleDot] 
             \[FormalE])], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 3} -> 
     <|"Statement" -> HoldForm[(\[FormalD] \[CircleDot] 
           (\[FormalD] \[CircleDot] \[FormalD])) \[CircleDot] 
          (((\[FormalD] \[CircleDot] \[FormalD]) \[CircleDot] 
            \[FormalF]) \[CircleDot] \[FormalE]) == \[FormalD] \[CircleDot] 
          (((\[FormalD] \[CircleDot] \[FormalD]) \[CircleDot] 
            \[FormalF]) \[CircleDot] \[FormalE])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 2}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 9}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CircleDot] ((\[FormalB]_) \[CircleDot] 
             (\[FormalC]_))) \[CircleDot] (\[FormalAlpha]_) -> 
          (\[FormalA] \[CircleDot] \[FormalB]) \[CircleDot] 
           (\[FormalAlpha] \[CircleDot] \[FormalC]), "OutputExpression" -> 
         HoldForm[(\[FormalD] \[CircleDot] (\[FormalD] \[CircleDot] 
              \[FormalD])) \[CircleDot] (((\[FormalD] \[CircleDot] 
               \[FormalD]) \[CircleDot] \[FormalF]) \[CircleDot] 
             \[FormalE]) == \[FormalD] \[CircleDot] 
            (((\[FormalD] \[CircleDot] \[FormalD]) \[CircleDot] 
              \[FormalF]) \[CircleDot] \[FormalE])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"Conclusion", 1} -> <|"Statement" -> 
       HoldForm[\[FormalD] \[CircleDot] (((\[FormalD] \[CircleDot] 
             \[FormalD]) \[CircleDot] \[FormalF]) \[CircleDot] \[FormalE]) == 
         \[FormalD] \[CircleDot] (((\[FormalD] \[CircleDot] 
             \[FormalD]) \[CircleDot] \[FormalF]) \[CircleDot] \[FormalE])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 3}, "Position" -> {1}, 
        "Construct" -> {"CriticalPairLemma", 8}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CircleDot] ((\[FormalB]_) \[CircleDot] 
            (\[FormalB]_)) -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalD] \[CircleDot] (((\[FormalD] \[CircleDot] 
               \[FormalD]) \[CircleDot] \[FormalF]) \[CircleDot] 
             \[FormalE]) == \[FormalD] \[CircleDot] 
            (((\[FormalD] \[CircleDot] \[FormalD]) \[CircleDot] 
              \[FormalF]) \[CircleDot] \[FormalE])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>}|>]
