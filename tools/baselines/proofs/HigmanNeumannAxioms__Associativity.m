ProofObject["EquationalLogic", {ForAll[{\[FormalA], \[FormalB], \[FormalC]}, 
   \[FormalA] \[CircleDot] ((\[FormalA] \[CircleDot] \[FormalA]) \[CircleDot] 
      (\[FormalB] \[CircleDot] ((\[FormalA] \[CircleDot] 
         \[FormalA]) \[CircleDot] \[FormalC]))) == 
    (\[FormalA] \[CircleDot] ((\[FormalA] \[CircleDot] 
        \[FormalA]) \[CircleDot] \[FormalB])) \[CircleDot] 
     ((\[FormalA] \[CircleDot] \[FormalA]) \[CircleDot] \[FormalC])]}, 
 {ForAll[{\[FormalA], \[FormalB], \[FormalC]}, 
   \[FormalA] \[CircleDot] 
     ((((\[FormalA] \[CircleDot] \[FormalA]) \[CircleDot] 
        \[FormalB]) \[CircleDot] \[FormalC]) \[CircleDot] 
      (((\[FormalA] \[CircleDot] \[FormalA]) \[CircleDot] 
        \[FormalA]) \[CircleDot] \[FormalC])) == \[FormalB]]}, 
 <|"Variables" -> {\[FormalA], \[FormalB], \[FormalC], \[FormalD], 
    \[FormalE], \[FormalF], \[FormalAlpha]}, "Constants" -> {}, 
  "Proof" -> {{"Axiom", 1} -> <|"Statement" -> 
       HoldForm[\[FormalA] == \[FormalB] \[CircleDot] 
          ((((\[FormalB] \[CircleDot] \[FormalB]) \[CircleDot] 
             \[FormalA]) \[CircleDot] \[FormalC]) \[CircleDot] 
           (((\[FormalB] \[CircleDot] \[FormalB]) \[CircleDot] 
             \[FormalB]) \[CircleDot] \[FormalC]))], "Proof" -> <||>|>, 
    {"Hypothesis", 1} -> <|"Statement" -> 
       HoldForm[(\[FormalD] \[CircleDot] ((\[FormalD] \[CircleDot] 
             \[FormalD]) \[CircleDot] \[FormalE])) \[CircleDot] 
          ((\[FormalD] \[CircleDot] \[FormalD]) \[CircleDot] \[FormalF]) == 
         \[FormalD] \[CircleDot] ((\[FormalD] \[CircleDot] 
            \[FormalD]) \[CircleDot] (\[FormalE] \[CircleDot] 
            ((\[FormalD] \[CircleDot] \[FormalD]) \[CircleDot] 
             \[FormalF])))], "Proof" -> <||>|>, {"CriticalPairLemma", 1} -> 
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
        "InputOrientation" -> 1, "Side" -> 2|>|>, {"CriticalPairLemma", 8} -> 
     <|"Statement" -> HoldForm[
        (((\[FormalA] \[CircleDot] \[FormalA]) \[CircleDot] 
            (\[FormalA] \[CircleDot] \[FormalA])) \[CircleDot] 
           \[FormalB]) \[CircleDot] (((\[FormalA] \[CircleDot] 
             \[FormalA]) \[CircleDot] (\[FormalA] \[CircleDot] 
             \[FormalA])) \[CircleDot] (\[FormalA] \[CircleDot] 
            \[FormalA])) == \[FormalA] \[CircleDot] (\[FormalB] \[CircleDot] 
           ((\[FormalA] \[CircleDot] \[FormalA]) \[CircleDot] \[FormalA]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 6}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CircleDot] 
           ((((\[FormalA]_) \[CircleDot] (\[FormalA]_)) \[CircleDot] 
             (\[FormalB]_)) \[CircleDot] (((\[FormalA]_) \[CircleDot] 
              (\[FormalA]_)) \[CircleDot] (\[FormalA]_))) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> ((\[FormalA]_) \[CircleDot] 
           (\[FormalA]_)) \[CircleDot] (\[FormalB]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 6}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CircleDot] 
           ((((\[FormalA]_) \[CircleDot] (\[FormalA]_)) \[CircleDot] 
             (\[FormalB]_)) \[CircleDot] (((\[FormalA]_) \[CircleDot] 
              (\[FormalA]_)) \[CircleDot] (\[FormalA]_))) -> \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"SubstitutionLemma", 5} -> 
     <|"Statement" -> HoldForm[
        ((\[FormalA] \[CircleDot] \[FormalA]) \[CircleDot] 
           \[FormalB]) \[CircleDot] (((\[FormalA] \[CircleDot] 
             \[FormalA]) \[CircleDot] (\[FormalA] \[CircleDot] 
             \[FormalA])) \[CircleDot] (\[FormalA] \[CircleDot] 
            \[FormalA])) == \[FormalA] \[CircleDot] (\[FormalB] \[CircleDot] 
           ((\[FormalA] \[CircleDot] \[FormalA]) \[CircleDot] \[FormalA]))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 8}, "Position" -> {1, 1}, 
        "Construct" -> {"SubstitutionLemma", 1}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CircleDot] ((\[FormalB]_) \[CircleDot] 
            (\[FormalB]_)) -> \[FormalA], "OutputExpression" -> 
         HoldForm[((\[FormalA] \[CircleDot] \[FormalA]) \[CircleDot] 
             \[FormalB]) \[CircleDot] (((\[FormalA] \[CircleDot] 
               \[FormalA]) \[CircleDot] (\[FormalA] \[CircleDot] 
               \[FormalA])) \[CircleDot] (\[FormalA] \[CircleDot] 
              \[FormalA])) == \[FormalA] \[CircleDot] 
            (\[FormalB] \[CircleDot] ((\[FormalA] \[CircleDot] 
               \[FormalA]) \[CircleDot] \[FormalA]))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, {"SubstitutionLemma", 6} -> 
     <|"Statement" -> HoldForm[
        ((\[FormalA] \[CircleDot] \[FormalA]) \[CircleDot] 
           \[FormalB]) \[CircleDot] ((\[FormalA] \[CircleDot] 
            \[FormalA]) \[CircleDot] (\[FormalA] \[CircleDot] \[FormalA])) == 
         \[FormalA] \[CircleDot] (\[FormalB] \[CircleDot] 
           ((\[FormalA] \[CircleDot] \[FormalA]) \[CircleDot] \[FormalA]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 5}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 1}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CircleDot] ((\[FormalB]_) \[CircleDot] 
            (\[FormalB]_)) -> \[FormalA], "OutputExpression" -> 
         HoldForm[((\[FormalA] \[CircleDot] \[FormalA]) \[CircleDot] 
             \[FormalB]) \[CircleDot] ((\[FormalA] \[CircleDot] 
              \[FormalA]) \[CircleDot] (\[FormalA] \[CircleDot] 
              \[FormalA])) == \[FormalA] \[CircleDot] 
            (\[FormalB] \[CircleDot] ((\[FormalA] \[CircleDot] 
               \[FormalA]) \[CircleDot] \[FormalA]))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, {"SubstitutionLemma", 7} -> 
     <|"Statement" -> HoldForm[
        ((\[FormalA] \[CircleDot] \[FormalA]) \[CircleDot] 
           \[FormalB]) \[CircleDot] (\[FormalA] \[CircleDot] \[FormalA]) == 
         \[FormalA] \[CircleDot] (\[FormalB] \[CircleDot] 
           ((\[FormalA] \[CircleDot] \[FormalA]) \[CircleDot] \[FormalA]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 6}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 1}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CircleDot] ((\[FormalB]_) \[CircleDot] 
            (\[FormalB]_)) -> \[FormalA], "OutputExpression" -> 
         HoldForm[((\[FormalA] \[CircleDot] \[FormalA]) \[CircleDot] 
             \[FormalB]) \[CircleDot] (\[FormalA] \[CircleDot] \[FormalA]) == 
           \[FormalA] \[CircleDot] (\[FormalB] \[CircleDot] 
             ((\[FormalA] \[CircleDot] \[FormalA]) \[CircleDot] 
              \[FormalA]))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 8} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CircleDot] 
           \[FormalA]) \[CircleDot] \[FormalB] == \[FormalA] \[CircleDot] 
          (\[FormalB] \[CircleDot] ((\[FormalA] \[CircleDot] 
             \[FormalA]) \[CircleDot] \[FormalA]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 7}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 1}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CircleDot] ((\[FormalB]_) \[CircleDot] 
            (\[FormalB]_)) -> \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CircleDot] \[FormalA]) \[CircleDot] 
            \[FormalB] == \[FormalA] \[CircleDot] (\[FormalB] \[CircleDot] 
             ((\[FormalA] \[CircleDot] \[FormalA]) \[CircleDot] 
              \[FormalA]))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"CriticalPairLemma", 9} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleDot] \[FormalA] == 
         (\[FormalB] \[CircleDot] \[FormalB]) \[CircleDot] 
          (\[FormalB] \[CircleDot] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 4}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CircleDot] 
            (\[FormalA]_)) \[CircleDot] (((\[FormalA]_) \[CircleDot] 
             (\[FormalA]_)) \[CircleDot] (\[FormalB]_)) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> ((\[FormalA]_) \[CircleDot] 
           (\[FormalA]_)) \[CircleDot] (\[FormalB]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 1}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CircleDot] 
           ((\[FormalB]_) \[CircleDot] (\[FormalB]_)) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 9} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleDot] \[FormalA] == 
         \[FormalB] \[CircleDot] \[FormalB]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 9}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 1}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CircleDot] ((\[FormalB]_) \[CircleDot] 
            (\[FormalB]_)) -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CircleDot] \[FormalA] == 
           \[FormalB] \[CircleDot] \[FormalB]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 10} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CircleDot] 
           \[FormalB]) \[CircleDot] ((\[FormalC] \[CircleDot] 
            \[FormalC]) \[CircleDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 4}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CircleDot] 
            (\[FormalA]_)) \[CircleDot] (((\[FormalA]_) \[CircleDot] 
             (\[FormalA]_)) \[CircleDot] (\[FormalB]_)) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CircleDot] 
          (\[FormalA]_), "MatchingConstruct" -> {"SubstitutionLemma", 9}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleDot] (\[FormalA]_) <-> 
          (\[FormalB]_) \[CircleDot] (\[FormalB]_), "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 11} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CircleDot] 
           \[FormalB]) \[CircleDot] ((((\[FormalC] \[CircleDot] 
              \[FormalC]) \[CircleDot] (\[FormalC] \[CircleDot] 
              \[FormalC])) \[CircleDot] (\[FormalC] \[CircleDot] 
             \[FormalC])) \[CircleDot] \[FormalB]) == \[FormalA]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 10}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CircleDot] 
            (\[FormalA]_)) \[CircleDot] (((\[FormalB]_) \[CircleDot] 
             (\[FormalB]_)) \[CircleDot] (\[FormalC]_)) -> \[FormalC], 
        "Side" -> 1, "Subpattern" -> ((\[FormalA]_) \[CircleDot] 
           (\[FormalA]_)) \[CircleDot] (((\[FormalB]_) \[CircleDot] 
            (\[FormalB]_)) \[CircleDot] (\[FormalC]_)), 
        "MatchingConstruct" -> {"CriticalPairLemma", 2}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CircleDot] (\[FormalA]_)) \[CircleDot] 
           ((\[FormalA]_) \[CircleDot] (((\[FormalB]_) \[CircleDot] 
              (\[FormalC]_)) \[CircleDot] ((((\[FormalA]_) \[CircleDot] 
                (\[FormalA]_)) \[CircleDot] (\[FormalA]_)) \[CircleDot] 
              (\[FormalC]_)))) -> \[FormalB], "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"SubstitutionLemma", 10} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CircleDot] 
           \[FormalB]) \[CircleDot] (((\[FormalC] \[CircleDot] 
             \[FormalC]) \[CircleDot] (\[FormalC] \[CircleDot] 
             \[FormalC])) \[CircleDot] \[FormalB]) == \[FormalA]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 11}, 
        "Position" -> {2, 1}, "Construct" -> {"SubstitutionLemma", 1}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CircleDot] 
           ((\[FormalB]_) \[CircleDot] (\[FormalB]_)) -> \[FormalA], 
        "OutputExpression" -> HoldForm[(\[FormalA] \[CircleDot] 
             \[FormalB]) \[CircleDot] (((\[FormalC] \[CircleDot] 
               \[FormalC]) \[CircleDot] (\[FormalC] \[CircleDot] 
               \[FormalC])) \[CircleDot] \[FormalB]) == \[FormalA]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 11} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CircleDot] 
           \[FormalB]) \[CircleDot] ((\[FormalC] \[CircleDot] 
            \[FormalC]) \[CircleDot] \[FormalB]) == \[FormalA]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 10}, 
        "Position" -> {2, 1}, "Construct" -> {"SubstitutionLemma", 1}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CircleDot] 
           ((\[FormalB]_) \[CircleDot] (\[FormalB]_)) -> \[FormalA], 
        "OutputExpression" -> HoldForm[(\[FormalA] \[CircleDot] 
             \[FormalB]) \[CircleDot] ((\[FormalC] \[CircleDot] 
              \[FormalC]) \[CircleDot] \[FormalB]) == \[FormalA]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 12} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CircleDot] 
           ((\[FormalB] \[CircleDot] \[FormalB]) \[CircleDot] 
            \[FormalC])) \[CircleDot] \[FormalC]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 11}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CircleDot] 
            (\[FormalB]_)) \[CircleDot] (((\[FormalC]_) \[CircleDot] 
             (\[FormalC]_)) \[CircleDot] (\[FormalB]_)) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> ((\[FormalC]_) \[CircleDot] 
           (\[FormalC]_)) \[CircleDot] (\[FormalB]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 10}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CircleDot] 
            (\[FormalA]_)) \[CircleDot] (((\[FormalB]_) \[CircleDot] 
             (\[FormalB]_)) \[CircleDot] (\[FormalC]_)) -> \[FormalC], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 13} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CircleDot] 
           \[FormalA]) \[CircleDot] (\[FormalB] \[CircleDot] 
           ((\[FormalC] \[CircleDot] \[FormalC]) \[CircleDot] 
            ((\[FormalA] \[CircleDot] \[FormalA]) \[CircleDot] 
             \[FormalA]))) == \[FormalA] \[CircleDot] \[FormalB]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 8}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CircleDot] 
           ((\[FormalB]_) \[CircleDot] (((\[FormalA]_) \[CircleDot] 
              (\[FormalA]_)) \[CircleDot] (\[FormalA]_))) -> 
          (\[FormalA] \[CircleDot] \[FormalA]) \[CircleDot] \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CircleDot] 
          (((\[FormalA]_) \[CircleDot] (\[FormalA]_)) \[CircleDot] 
           (\[FormalA]_)), "MatchingConstruct" -> {"CriticalPairLemma", 12}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CircleDot] (((\[FormalB]_) \[CircleDot] 
              (\[FormalB]_)) \[CircleDot] (\[FormalC]_))) \[CircleDot] 
           (\[FormalC]_) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 12} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CircleDot] 
           \[FormalA]) \[CircleDot] (\[FormalB] \[CircleDot] \[FormalA]) == 
         \[FormalA] \[CircleDot] \[FormalB]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 13}, 
        "Position" -> {2, 2}, "Construct" -> {"CriticalPairLemma", 10}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CircleDot] 
            (\[FormalA]_)) \[CircleDot] (((\[FormalB]_) \[CircleDot] 
             (\[FormalB]_)) \[CircleDot] (\[FormalC]_)) -> \[FormalC], 
        "OutputExpression" -> HoldForm[(\[FormalA] \[CircleDot] 
             \[FormalA]) \[CircleDot] (\[FormalB] \[CircleDot] \[FormalA]) == 
           \[FormalA] \[CircleDot] \[FormalB]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 14} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleDot] \[FormalB] == 
         (\[FormalC] \[CircleDot] \[FormalC]) \[CircleDot] 
          (\[FormalB] \[CircleDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 12}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CircleDot] 
            (\[FormalA]_)) \[CircleDot] ((\[FormalB]_) \[CircleDot] 
            (\[FormalA]_)) -> \[FormalA] \[CircleDot] \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CircleDot] 
          (\[FormalA]_), "MatchingConstruct" -> {"SubstitutionLemma", 9}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleDot] (\[FormalA]_) <-> 
          (\[FormalB]_) \[CircleDot] (\[FormalB]_), "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 15} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CircleDot] 
           (\[FormalB] \[CircleDot] \[FormalC])) \[CircleDot] 
          (\[FormalC] \[CircleDot] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 11}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CircleDot] 
            (\[FormalB]_)) \[CircleDot] (((\[FormalC]_) \[CircleDot] 
             (\[FormalC]_)) \[CircleDot] (\[FormalB]_)) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> ((\[FormalC]_) \[CircleDot] 
           (\[FormalC]_)) \[CircleDot] (\[FormalB]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 14}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CircleDot] 
            (\[FormalA]_)) \[CircleDot] ((\[FormalB]_) \[CircleDot] 
            (\[FormalC]_)) -> \[FormalC] \[CircleDot] \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 13} -> 
     <|"Statement" -> HoldForm[
        ((\[FormalA] \[CircleDot] \[FormalB]) \[CircleDot] 
           (((\[FormalC] \[CircleDot] \[FormalC]) \[CircleDot] 
             \[FormalC]) \[CircleDot] \[FormalB])) \[CircleDot] \[FormalC] == 
         \[FormalA]], "Proof" -> <|"Input" -> {"CriticalPairLemma", 2}, 
        "Position" -> {}, "Construct" -> {"CriticalPairLemma", 14}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CircleDot] 
            (\[FormalA]_)) \[CircleDot] ((\[FormalB]_) \[CircleDot] 
            (\[FormalC]_)) -> \[FormalC] \[CircleDot] \[FormalB], 
        "OutputExpression" -> HoldForm[
          ((\[FormalA] \[CircleDot] \[FormalB]) \[CircleDot] 
             (((\[FormalC] \[CircleDot] \[FormalC]) \[CircleDot] 
               \[FormalC]) \[CircleDot] \[FormalB])) \[CircleDot] 
            \[FormalC] == \[FormalA]], "ConstructSide" -> 1, 
        "InputOrientation" -> -1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 16} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CircleDot] 
           \[FormalB]) \[CircleDot] 
          (((((\[FormalC] \[CircleDot] \[FormalC]) \[CircleDot] 
              \[FormalAlpha]) \[CircleDot] ((\[FormalC] \[CircleDot] 
               \[FormalC]) \[CircleDot] \[FormalAlpha])) \[CircleDot] 
            ((\[FormalC] \[CircleDot] \[FormalC]) \[CircleDot] 
             \[FormalAlpha])) \[CircleDot] \[FormalB]) == 
         \[FormalA] \[CircleDot] \[FormalAlpha]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 12}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CircleDot] 
            (((\[FormalB]_) \[CircleDot] (\[FormalB]_)) \[CircleDot] 
             (\[FormalC]_))) \[CircleDot] (\[FormalC]_) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CircleDot] 
          (((\[FormalB]_) \[CircleDot] (\[FormalB]_)) \[CircleDot] 
           (\[FormalC]_)), "MatchingConstruct" -> {"SubstitutionLemma", 13}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (((\[FormalA]_) \[CircleDot] (\[FormalB]_)) \[CircleDot] 
            ((((\[FormalC]_) \[CircleDot] (\[FormalC]_)) \[CircleDot] 
              (\[FormalC]_)) \[CircleDot] (\[FormalB]_))) \[CircleDot] 
           (\[FormalC]_) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 14} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CircleDot] 
           \[FormalB]) \[CircleDot] ((\[FormalC] \[CircleDot] 
            (\[FormalAlpha] \[CircleDot] \[FormalAlpha])) \[CircleDot] 
           \[FormalB]) == \[FormalA] \[CircleDot] \[FormalC]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 16}, 
        "Position" -> {2, 1}, "Construct" -> {"CriticalPairLemma", 14}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CircleDot] 
            (\[FormalA]_)) \[CircleDot] ((\[FormalB]_) \[CircleDot] 
            (\[FormalC]_)) -> \[FormalC] \[CircleDot] \[FormalB], 
        "OutputExpression" -> HoldForm[(\[FormalA] \[CircleDot] 
             \[FormalB]) \[CircleDot] ((\[FormalC] \[CircleDot] 
              (\[FormalAlpha] \[CircleDot] \[FormalAlpha])) \[CircleDot] 
             \[FormalB]) == \[FormalA] \[CircleDot] \[FormalC]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 15} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CircleDot] 
           \[FormalB]) \[CircleDot] (\[FormalC] \[CircleDot] \[FormalB]) == 
         \[FormalA] \[CircleDot] \[FormalC]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 14}, 
        "Position" -> {2, 1}, "Construct" -> {"SubstitutionLemma", 1}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CircleDot] 
           ((\[FormalB]_) \[CircleDot] (\[FormalB]_)) -> \[FormalA], 
        "OutputExpression" -> HoldForm[(\[FormalA] \[CircleDot] 
             \[FormalB]) \[CircleDot] (\[FormalC] \[CircleDot] \[FormalB]) == 
           \[FormalA] \[CircleDot] \[FormalC]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 17} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CircleDot] 
           (\[FormalB] \[CircleDot] \[FormalC])) \[CircleDot] 
          \[FormalAlpha] == \[FormalA] \[CircleDot] 
          (\[FormalAlpha] \[CircleDot] (\[FormalC] \[CircleDot] 
            \[FormalB]))], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 
          15}, "Orientation" -> 1, "Rule" -> 
         ((\[FormalA]_) \[CircleDot] (\[FormalB]_)) \[CircleDot] 
           ((\[FormalC]_) \[CircleDot] (\[FormalB]_)) -> 
          \[FormalA] \[CircleDot] \[FormalC], "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CircleDot] (\[FormalB]_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 15}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CircleDot] ((\[FormalB]_) \[CircleDot] 
             (\[FormalC]_))) \[CircleDot] ((\[FormalC]_) \[CircleDot] 
            (\[FormalB]_)) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 16} -> 
     <|"Statement" -> HoldForm[(\[FormalD] \[CircleDot] 
           ((\[FormalD] \[CircleDot] \[FormalD]) \[CircleDot] 
            \[FormalE])) \[CircleDot] ((\[FormalD] \[CircleDot] 
            \[FormalD]) \[CircleDot] \[FormalF]) == \[FormalD] \[CircleDot] 
          (((\[FormalD] \[CircleDot] \[FormalD]) \[CircleDot] 
            \[FormalF]) \[CircleDot] \[FormalE])], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Position" -> {2}, 
        "Construct" -> {"CriticalPairLemma", 14}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CircleDot] (\[FormalA]_)) \[CircleDot] 
           ((\[FormalB]_) \[CircleDot] (\[FormalC]_)) -> 
          \[FormalC] \[CircleDot] \[FormalB], "OutputExpression" -> 
         HoldForm[(\[FormalD] \[CircleDot] ((\[FormalD] \[CircleDot] 
               \[FormalD]) \[CircleDot] \[FormalE])) \[CircleDot] 
            ((\[FormalD] \[CircleDot] \[FormalD]) \[CircleDot] \[FormalF]) == 
           \[FormalD] \[CircleDot] (((\[FormalD] \[CircleDot] 
               \[FormalD]) \[CircleDot] \[FormalF]) \[CircleDot] 
             \[FormalE])], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 17} -> 
     <|"Statement" -> HoldForm[\[FormalD] \[CircleDot] 
          (((\[FormalD] \[CircleDot] \[FormalD]) \[CircleDot] 
            \[FormalF]) \[CircleDot] (\[FormalE] \[CircleDot] 
            (\[FormalD] \[CircleDot] \[FormalD]))) == \[FormalD] \[CircleDot] 
          (((\[FormalD] \[CircleDot] \[FormalD]) \[CircleDot] 
            \[FormalF]) \[CircleDot] \[FormalE])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 16}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 17}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CircleDot] ((\[FormalB]_) \[CircleDot] 
             (\[FormalC]_))) \[CircleDot] (\[FormalAlpha]_) -> 
          \[FormalA] \[CircleDot] (\[FormalAlpha] \[CircleDot] 
            (\[FormalC] \[CircleDot] \[FormalB])), "OutputExpression" -> 
         HoldForm[\[FormalD] \[CircleDot] (((\[FormalD] \[CircleDot] 
               \[FormalD]) \[CircleDot] \[FormalF]) \[CircleDot] 
             (\[FormalE] \[CircleDot] (\[FormalD] \[CircleDot] 
               \[FormalD]))) == \[FormalD] \[CircleDot] 
            (((\[FormalD] \[CircleDot] \[FormalD]) \[CircleDot] 
              \[FormalF]) \[CircleDot] \[FormalE])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"Conclusion", 1} -> <|"Statement" -> 
       HoldForm[\[FormalD] \[CircleDot] (((\[FormalD] \[CircleDot] 
             \[FormalD]) \[CircleDot] \[FormalF]) \[CircleDot] \[FormalE]) == 
         \[FormalD] \[CircleDot] (((\[FormalD] \[CircleDot] 
             \[FormalD]) \[CircleDot] \[FormalF]) \[CircleDot] \[FormalE])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 17}, 
        "Position" -> {2, 2}, "Construct" -> {"SubstitutionLemma", 1}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CircleDot] 
           ((\[FormalB]_) \[CircleDot] (\[FormalB]_)) -> \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalD] \[CircleDot] 
            (((\[FormalD] \[CircleDot] \[FormalD]) \[CircleDot] 
              \[FormalF]) \[CircleDot] \[FormalE]) == \[FormalD] \[CircleDot] 
            (((\[FormalD] \[CircleDot] \[FormalD]) \[CircleDot] 
              \[FormalF]) \[CircleDot] \[FormalE])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>}|>]
