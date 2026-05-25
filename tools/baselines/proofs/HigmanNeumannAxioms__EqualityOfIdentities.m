ProofObject["EquationalLogic", 
 {ForAll[\[FormalA], \[FormalA] \[CircleDot] (\[FormalA] \[CircleDot] 
      \[FormalA]) == (\[FormalA] \[CircleDot] \[FormalA]) \[CircleDot] 
     ((\[FormalA] \[CircleDot] \[FormalA]) \[CircleDot] \[FormalA])]}, 
 {ForAll[{\[FormalA], \[FormalB], \[FormalC]}, 
   \[FormalA] \[CircleDot] 
     ((((\[FormalA] \[CircleDot] \[FormalA]) \[CircleDot] 
        \[FormalB]) \[CircleDot] \[FormalC]) \[CircleDot] 
      (((\[FormalA] \[CircleDot] \[FormalA]) \[CircleDot] 
        \[FormalA]) \[CircleDot] \[FormalC])) == \[FormalB]]}, 
 <|"Variables" -> {\[FormalA], \[FormalB], \[FormalC], \[FormalD], 
    \[FormalAlpha]}, "Constants" -> {}, 
  "Proof" -> {{"Axiom", 1} -> <|"Statement" -> 
       HoldForm[\[FormalA] == \[FormalB] \[CircleDot] 
          ((((\[FormalB] \[CircleDot] \[FormalB]) \[CircleDot] 
             \[FormalA]) \[CircleDot] \[FormalC]) \[CircleDot] 
           (((\[FormalB] \[CircleDot] \[FormalB]) \[CircleDot] 
             \[FormalB]) \[CircleDot] \[FormalC]))], "Proof" -> <||>|>, 
    {"Hypothesis", 1} -> <|"Statement" -> 
       HoldForm[(\[FormalD] \[CircleDot] \[FormalD]) \[CircleDot] 
          ((\[FormalD] \[CircleDot] \[FormalD]) \[CircleDot] \[FormalD]) == 
         \[FormalD] \[CircleDot] (\[FormalD] \[CircleDot] \[FormalD])], 
      "Proof" -> <||>|>, {"CriticalPairLemma", 1} -> 
     <|"Statement" -> HoldForm[
        ((((\[FormalA] \[CircleDot] \[FormalA]) \[CircleDot] 
             (\[FormalA] \[CircleDot] \[FormalA])) \[CircleDot] 
            \[FormalB]) \[CircleDot] \[FormalC]) \[CircleDot] 
          ((((\[FormalA] \[CircleDot] \[FormalA]) \[CircleDot] 
             (\[FormalA] \[CircleDot] \[FormalA])) \[CircleDot] 
            (\[FormalA] \[CircleDot] \[FormalA])) \[CircleDot] \[FormalC]) == 
         \[FormalA] \[CircleDot] ((\[FormalB] \[CircleDot] 
            \[FormalAlpha]) \[CircleDot] (((\[FormalA] \[CircleDot] 
              \[FormalA]) \[CircleDot] \[FormalA]) \[CircleDot] 
            \[FormalAlpha]))], "Proof" -> <|"Construct" -> {"Axiom", 1}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CircleDot] 
           (((((\[FormalA]_) \[CircleDot] (\[FormalA]_)) \[CircleDot] 
              (\[FormalB]_)) \[CircleDot] (\[FormalC]_)) \[CircleDot] 
            ((((\[FormalA]_) \[CircleDot] (\[FormalA]_)) \[CircleDot] 
              (\[FormalA]_)) \[CircleDot] (\[FormalC]_))) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> ((\[FormalA]_) \[CircleDot] 
           (\[FormalA]_)) \[CircleDot] (\[FormalB]_), "MatchingConstruct" -> 
         {"Axiom", 1}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleDot] (((((\[FormalA]_) \[CircleDot] (
                \[FormalA]_)) \[CircleDot] (\[FormalB]_)) \[CircleDot] 
             (\[FormalC]_)) \[CircleDot] ((((\[FormalA]_) \[CircleDot] (
                \[FormalA]_)) \[CircleDot] (\[FormalA]_)) \[CircleDot] 
             (\[FormalC]_))) -> \[FormalB], "MatchingSide" -> 1, 
        "Position" -> {2, 1, 1}|>|>, {"CriticalPairLemma", 2} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CircleDot] 
           \[FormalB]) \[CircleDot] (\[FormalB] \[CircleDot] 
           ((\[FormalA] \[CircleDot] \[FormalC]) \[CircleDot] 
            (((\[FormalB] \[CircleDot] \[FormalB]) \[CircleDot] 
              \[FormalB]) \[CircleDot] \[FormalC])))], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CircleDot] 
           (((((\[FormalA]_) \[CircleDot] (\[FormalA]_)) \[CircleDot] 
              (\[FormalB]_)) \[CircleDot] (\[FormalC]_)) \[CircleDot] 
            ((((\[FormalA]_) \[CircleDot] (\[FormalA]_)) \[CircleDot] 
              (\[FormalA]_)) \[CircleDot] (\[FormalC]_))) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> 
         ((((\[FormalA]_) \[CircleDot] (\[FormalA]_)) \[CircleDot] 
            (\[FormalB]_)) \[CircleDot] (\[FormalC]_)) \[CircleDot] 
          ((((\[FormalA]_) \[CircleDot] (\[FormalA]_)) \[CircleDot] 
            (\[FormalA]_)) \[CircleDot] (\[FormalC]_)), 
        "MatchingConstruct" -> {"CriticalPairLemma", 1}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (((((\[FormalA]_) \[CircleDot] (\[FormalA]_)) \[CircleDot] 
              ((\[FormalA]_) \[CircleDot] (\[FormalA]_))) \[CircleDot] 
             (\[FormalB]_)) \[CircleDot] (\[FormalC]_)) \[CircleDot] 
           (((((\[FormalA]_) \[CircleDot] (\[FormalA]_)) \[CircleDot] 
              ((\[FormalA]_) \[CircleDot] (\[FormalA]_))) \[CircleDot] 
             ((\[FormalA]_) \[CircleDot] (\[FormalA]_))) \[CircleDot] 
            (\[FormalC]_)) <-> (\[FormalA]_) \[CircleDot] 
           (((\[FormalB]_) \[CircleDot] (\[FormalAlpha]_)) \[CircleDot] 
            ((((\[FormalA]_) \[CircleDot] (\[FormalA]_)) \[CircleDot] 
              (\[FormalA]_)) \[CircleDot] (\[FormalAlpha]_))), 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 3} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CircleDot] 
           \[FormalB]) \[CircleDot] (\[FormalB] \[CircleDot] 
           (\[FormalC] \[CircleDot] (((\[FormalB] \[CircleDot] 
               \[FormalB]) \[CircleDot] \[FormalB]) \[CircleDot] 
             ((((\[FormalA] \[CircleDot] \[FormalA]) \[CircleDot] 
                \[FormalC]) \[CircleDot] \[FormalAlpha]) \[CircleDot] 
              (((\[FormalA] \[CircleDot] \[FormalA]) \[CircleDot] 
                \[FormalA]) \[CircleDot] \[FormalAlpha])))))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 2}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CircleDot] 
            (\[FormalA]_)) \[CircleDot] ((\[FormalA]_) \[CircleDot] 
            (((\[FormalB]_) \[CircleDot] (\[FormalC]_)) \[CircleDot] 
             ((((\[FormalA]_) \[CircleDot] (\[FormalA]_)) \[CircleDot] (
                \[FormalA]_)) \[CircleDot] (\[FormalC]_)))) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CircleDot] 
          (\[FormalC]_), "MatchingConstruct" -> {"Axiom", 1}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleDot] (((((\[FormalA]_) \[CircleDot] (
                \[FormalA]_)) \[CircleDot] (\[FormalB]_)) \[CircleDot] 
             (\[FormalC]_)) \[CircleDot] ((((\[FormalA]_) \[CircleDot] (
                \[FormalA]_)) \[CircleDot] (\[FormalA]_)) \[CircleDot] 
             (\[FormalC]_))) -> \[FormalB], "MatchingSide" -> 1, 
        "Position" -> {2, 2, 1}|>|>, {"CriticalPairLemma", 4} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CircleDot] 
           \[FormalA]) \[CircleDot] \[FormalA] == 
         (\[FormalA] \[CircleDot] \[FormalA]) \[CircleDot] 
          (\[FormalA] \[CircleDot] (\[FormalB] \[CircleDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 3}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CircleDot] 
            (\[FormalA]_)) \[CircleDot] ((\[FormalA]_) \[CircleDot] 
            ((\[FormalB]_) \[CircleDot] ((((\[FormalA]_) \[CircleDot] 
                (\[FormalA]_)) \[CircleDot] (\[FormalA]_)) \[CircleDot] 
              (((((\[FormalC]_) \[CircleDot] (\[FormalC]_)) \[CircleDot] 
                 (\[FormalB]_)) \[CircleDot] (\[FormalAlpha]_)) \[CircleDot] (
                (((\[FormalC]_) \[CircleDot] (\[FormalC]_)) \[CircleDot] 
                 (\[FormalC]_)) \[CircleDot] (\[FormalAlpha]_)))))) -> 
          \[FormalC], "Side" -> 1, "Subpattern" -> 
         (((\[FormalA]_) \[CircleDot] (\[FormalA]_)) \[CircleDot] 
           (\[FormalA]_)) \[CircleDot] 
          (((((\[FormalC]_) \[CircleDot] (\[FormalC]_)) \[CircleDot] 
             (\[FormalB]_)) \[CircleDot] (\[FormalAlpha]_)) \[CircleDot] 
           ((((\[FormalC]_) \[CircleDot] (\[FormalC]_)) \[CircleDot] 
             (\[FormalC]_)) \[CircleDot] (\[FormalAlpha]_))), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CircleDot] 
           (((((\[FormalA]_) \[CircleDot] (\[FormalA]_)) \[CircleDot] 
              (\[FormalB]_)) \[CircleDot] (\[FormalC]_)) \[CircleDot] 
            ((((\[FormalA]_) \[CircleDot] (\[FormalA]_)) \[CircleDot] 
              (\[FormalA]_)) \[CircleDot] (\[FormalC]_))) -> \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {2, 2, 2}|>|>, 
    {"CriticalPairLemma", 5} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleDot] 
          (\[FormalB] \[CircleDot] \[FormalB]) == \[FormalA] \[CircleDot] 
          ((((\[FormalA] \[CircleDot] \[FormalA]) \[CircleDot] 
             \[FormalA]) \[CircleDot] \[FormalC]) \[CircleDot] 
           (((\[FormalA] \[CircleDot] \[FormalA]) \[CircleDot] 
             \[FormalA]) \[CircleDot] \[FormalC]))], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CircleDot] 
           (((((\[FormalA]_) \[CircleDot] (\[FormalA]_)) \[CircleDot] 
              (\[FormalB]_)) \[CircleDot] (\[FormalC]_)) \[CircleDot] 
            ((((\[FormalA]_) \[CircleDot] (\[FormalA]_)) \[CircleDot] 
              (\[FormalA]_)) \[CircleDot] (\[FormalC]_))) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> ((\[FormalA]_) \[CircleDot] 
           (\[FormalA]_)) \[CircleDot] (\[FormalB]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 4}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CircleDot] 
            (\[FormalA]_)) \[CircleDot] ((\[FormalA]_) \[CircleDot] 
            ((\[FormalB]_) \[CircleDot] (\[FormalB]_))) -> 
          (\[FormalA] \[CircleDot] \[FormalA]) \[CircleDot] \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2, 1, 1}|>|>, 
    {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleDot] 
          (\[FormalB] \[CircleDot] \[FormalB]) == \[FormalA]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 5}, "Position" -> {}, 
        "Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CircleDot] 
           (((((\[FormalA]_) \[CircleDot] (\[FormalA]_)) \[CircleDot] 
              (\[FormalB]_)) \[CircleDot] (\[FormalC]_)) \[CircleDot] 
            ((((\[FormalA]_) \[CircleDot] (\[FormalA]_)) \[CircleDot] 
              (\[FormalA]_)) \[CircleDot] (\[FormalC]_))) -> \[FormalB], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CircleDot] 
            (\[FormalB] \[CircleDot] \[FormalB]) == \[FormalA]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 6} -> 
     <|"Statement" -> HoldForm[\[FormalA] == \[FormalB] \[CircleDot] 
          (((\[FormalB] \[CircleDot] \[FormalB]) \[CircleDot] 
            \[FormalA]) \[CircleDot] ((\[FormalB] \[CircleDot] 
             \[FormalB]) \[CircleDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CircleDot] 
           (((((\[FormalA]_) \[CircleDot] (\[FormalA]_)) \[CircleDot] 
              (\[FormalB]_)) \[CircleDot] (\[FormalC]_)) \[CircleDot] 
            ((((\[FormalA]_) \[CircleDot] (\[FormalA]_)) \[CircleDot] 
              (\[FormalA]_)) \[CircleDot] (\[FormalC]_))) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> 
         ((((\[FormalA]_) \[CircleDot] (\[FormalA]_)) \[CircleDot] 
            (\[FormalB]_)) \[CircleDot] (\[FormalC]_)) \[CircleDot] 
          ((((\[FormalA]_) \[CircleDot] (\[FormalA]_)) \[CircleDot] 
            (\[FormalA]_)) \[CircleDot] (\[FormalC]_)), 
        "MatchingConstruct" -> {"SubstitutionLemma", 1}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleDot] ((\[FormalB]_) \[CircleDot] 
            (\[FormalB]_)) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 7} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CircleDot] 
           \[FormalB]) \[CircleDot] (((\[FormalB] \[CircleDot] 
             \[FormalB]) \[CircleDot] \[FormalA]) \[CircleDot] 
           (((\[FormalB] \[CircleDot] \[FormalB]) \[CircleDot] 
             (\[FormalB] \[CircleDot] \[FormalB])) \[CircleDot] 
            (\[FormalB] \[CircleDot] \[FormalB])))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 6}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CircleDot] 
           ((((\[FormalA]_) \[CircleDot] (\[FormalA]_)) \[CircleDot] 
             (\[FormalB]_)) \[CircleDot] (((\[FormalA]_) \[CircleDot] 
              (\[FormalA]_)) \[CircleDot] (\[FormalA]_))) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CircleDot] 
          (\[FormalA]_), "MatchingConstruct" -> {"SubstitutionLemma", 1}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleDot] ((\[FormalB]_) \[CircleDot] 
            (\[FormalB]_)) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2, 1, 1}|>|>, {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CircleDot] 
           \[FormalB]) \[CircleDot] (((\[FormalB] \[CircleDot] 
             \[FormalB]) \[CircleDot] \[FormalA]) \[CircleDot] 
           ((\[FormalB] \[CircleDot] \[FormalB]) \[CircleDot] 
            (\[FormalB] \[CircleDot] \[FormalB])))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 7}, "Position" -> {2, 2}, 
        "Construct" -> {"SubstitutionLemma", 1}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CircleDot] ((\[FormalB]_) \[CircleDot] 
            (\[FormalB]_)) -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == (\[FormalB] \[CircleDot] 
             \[FormalB]) \[CircleDot] (((\[FormalB] \[CircleDot] 
               \[FormalB]) \[CircleDot] \[FormalA]) \[CircleDot] 
             ((\[FormalB] \[CircleDot] \[FormalB]) \[CircleDot] 
              (\[FormalB] \[CircleDot] \[FormalB])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, {"SubstitutionLemma", 3} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CircleDot] 
           \[FormalB]) \[CircleDot] (((\[FormalB] \[CircleDot] 
             \[FormalB]) \[CircleDot] \[FormalA]) \[CircleDot] 
           (\[FormalB] \[CircleDot] \[FormalB]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 2}, "Position" -> {2, 2}, 
        "Construct" -> {"SubstitutionLemma", 1}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CircleDot] ((\[FormalB]_) \[CircleDot] 
            (\[FormalB]_)) -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == (\[FormalB] \[CircleDot] 
             \[FormalB]) \[CircleDot] (((\[FormalB] \[CircleDot] 
               \[FormalB]) \[CircleDot] \[FormalA]) \[CircleDot] 
             (\[FormalB] \[CircleDot] \[FormalB]))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, {"SubstitutionLemma", 4} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CircleDot] 
           \[FormalB]) \[CircleDot] ((\[FormalB] \[CircleDot] 
            \[FormalB]) \[CircleDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 3}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 1}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CircleDot] ((\[FormalB]_) \[CircleDot] 
            (\[FormalB]_)) -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == (\[FormalB] \[CircleDot] 
             \[FormalB]) \[CircleDot] ((\[FormalB] \[CircleDot] 
              \[FormalB]) \[CircleDot] \[FormalA])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, {"SubstitutionLemma", 5} -> 
     <|"Statement" -> HoldForm[(\[FormalD] \[CircleDot] 
           \[FormalD]) \[CircleDot] ((\[FormalD] \[CircleDot] 
            \[FormalD]) \[CircleDot] \[FormalD]) == \[FormalD]], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 1}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CircleDot] ((\[FormalB]_) \[CircleDot] 
            (\[FormalB]_)) -> \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalD] \[CircleDot] \[FormalD]) \[CircleDot] 
            ((\[FormalD] \[CircleDot] \[FormalD]) \[CircleDot] \[FormalD]) == 
           \[FormalD]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"Conclusion", 1} -> 
     <|"Statement" -> HoldForm[\[FormalD] == \[FormalD]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 5}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 4}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CircleDot] (\[FormalA]_)) \[CircleDot] 
           (((\[FormalA]_) \[CircleDot] (\[FormalA]_)) \[CircleDot] 
            (\[FormalB]_)) -> \[FormalB], "OutputExpression" -> 
         HoldForm[\[FormalD] == \[FormalD]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>}|>]
