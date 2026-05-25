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
   ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
      \[FormalC]) \[CenterDot] (\[FormalA] \[CenterDot] 
      ((\[FormalA] \[CenterDot] \[FormalC]) \[CenterDot] \[FormalA])) == 
    \[FormalC]]}, <|"Variables" -> {\[FormalA], \[FormalB], \[FormalC], 
    \[FormalD], \[FormalE], \[FormalF], \[FormalAlpha], \[FormalBeta]}, 
  "Constants" -> {}, "Proof" -> 
   {{"Axiom", 1} -> <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalB] \[CenterDot] 
           ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalB]))], 
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
          ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA]) == 
         \[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalC]) \[CenterDot] (((\[FormalA] \[CenterDot] 
              \[FormalC]) \[CenterDot] (\[FormalA] \[CenterDot] 
              ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
               \[FormalA]))) \[CenterDot] (\[FormalA] \[CenterDot] 
             \[FormalC])))], "Proof" -> <|"Construct" -> {"Axiom", 1}, 
        "Orientation" -> -1, "Rule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalC], "Side" -> 1, 
        "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           (\[FormalB]_)) \[CenterDot] (\[FormalC]_), "MatchingConstruct" -> 
         {"Axiom", 1}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalC], "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 2} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CenterDot] 
           \[FormalA]) \[CenterDot] (((\[FormalC] \[CenterDot] 
             \[FormalAlpha]) \[CenterDot] \[FormalB]) \[CenterDot] 
           ((((\[FormalC] \[CenterDot] \[FormalAlpha]) \[CenterDot] 
              \[FormalB]) \[CenterDot] \[FormalA]) \[CenterDot] 
            ((\[FormalC] \[CenterDot] \[FormalAlpha]) \[CenterDot] 
             \[FormalB])))], "Proof" -> <|"Construct" -> {"Axiom", 1}, 
        "Orientation" -> -1, "Rule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalC], "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (((\[FormalA]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
              (\[FormalC]_)) \[CenterDot] (\[FormalA]_))) -> \[FormalC], 
        "MatchingSide" -> 1, "Position" -> {1, 1}|>|>, 
    {"CriticalPairLemma", 3} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         (((\[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
               \[FormalA]) \[CenterDot] \[FormalB])) \[CenterDot] 
            \[FormalC]) \[CenterDot] \[FormalA]) \[CenterDot] 
          ((\[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
              \[FormalA]) \[CenterDot] \[FormalB])) \[CenterDot] 
           \[FormalA])], "Proof" -> <|"Construct" -> {"Axiom", 1}, 
        "Orientation" -> -1, "Rule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalC], "Side" -> 1, 
        "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           (\[FormalC]_)) \[CenterDot] (\[FormalA]_), "MatchingConstruct" -> 
         {"Axiom", 1}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalC], "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 4} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
              \[FormalC]) \[CenterDot] \[FormalB])) \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalC] \[CenterDot] 
           (((((\[FormalB] \[CenterDot] \[FormalAlpha]) \[CenterDot] 
               \[FormalC]) \[CenterDot] (\[FormalB] \[CenterDot] (
                (\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
                \[FormalB]))) \[CenterDot] \[FormalA]) \[CenterDot] 
            (((\[FormalB] \[CenterDot] \[FormalAlpha]) \[CenterDot] 
              \[FormalC]) \[CenterDot] (\[FormalB] \[CenterDot] 
              ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
               \[FormalB])))))], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 2}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((((\[FormalC]_) \[CenterDot] (\[FormalAlpha]_)) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] (((((\[FormalC]_) \[CenterDot] 
                (\[FormalAlpha]_)) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
              (\[FormalB]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] (
                \[FormalAlpha]_)) \[CenterDot] (\[FormalA]_)))) -> 
          \[FormalB], "Side" -> 1, "Subpattern" -> 
         ((\[FormalC]_) \[CenterDot] (\[FormalAlpha]_)) \[CenterDot] 
          (\[FormalA]_), "MatchingConstruct" -> {"Axiom", 1}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalC], "MatchingSide" -> 1, 
        "Position" -> {2, 1}|>|>, {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
              \[FormalC]) \[CenterDot] \[FormalB])) \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalC] \[CenterDot] 
           ((\[FormalC] \[CenterDot] \[FormalA]) \[CenterDot] 
            (((\[FormalB] \[CenterDot] \[FormalAlpha]) \[CenterDot] 
              \[FormalC]) \[CenterDot] (\[FormalB] \[CenterDot] 
              ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
               \[FormalB])))))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 4}, "Position" -> {2, 2, 1, 1}, 
        "Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalC], "OutputExpression" -> 
         HoldForm[\[FormalA] == ((\[FormalB] \[CenterDot] 
              ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
               \[FormalB])) \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalC] \[CenterDot] ((\[FormalC] \[CenterDot] 
               \[FormalA]) \[CenterDot] (((\[FormalB] \[CenterDot] 
                 \[FormalAlpha]) \[CenterDot] \[FormalC]) \[CenterDot] (
                \[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
                  \[FormalC]) \[CenterDot] \[FormalB])))))], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
              \[FormalC]) \[CenterDot] \[FormalB])) \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalC] \[CenterDot] 
           ((\[FormalC] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalC]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 1}, 
        "Position" -> {2, 2, 2}, "Construct" -> {"Axiom", 1}, 
        "Orientation" -> -1, "Rule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalC], "OutputExpression" -> 
         HoldForm[\[FormalA] == ((\[FormalB] \[CenterDot] 
              ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
               \[FormalB])) \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalC] \[CenterDot] ((\[FormalC] \[CenterDot] 
               \[FormalA]) \[CenterDot] \[FormalC]))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, {"CriticalPairLemma", 5} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CenterDot] 
           \[FormalA]) \[CenterDot] ((\[FormalC] \[CenterDot] 
            \[FormalB]) \[CenterDot] ((((((\[FormalAlpha] \[CenterDot] 
                 \[FormalBeta]) \[CenterDot] \[FormalC]) \[CenterDot] (
                \[FormalAlpha] \[CenterDot] ((\[FormalAlpha] \[CenterDot] 
                  \[FormalC]) \[CenterDot] \[FormalAlpha]))) \[CenterDot] 
              \[FormalB]) \[CenterDot] \[FormalA]) \[CenterDot] 
            ((((\[FormalAlpha] \[CenterDot] \[FormalBeta]) \[CenterDot] 
               \[FormalC]) \[CenterDot] (\[FormalAlpha] \[CenterDot] (
                (\[FormalAlpha] \[CenterDot] \[FormalC]) \[CenterDot] 
                \[FormalAlpha]))) \[CenterDot] \[FormalB])))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 2}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((((\[FormalC]_) \[CenterDot] 
              (\[FormalAlpha]_)) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
            (((((\[FormalC]_) \[CenterDot] (\[FormalAlpha]_)) \[CenterDot] (
                \[FormalA]_)) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
             (((\[FormalC]_) \[CenterDot] (\[FormalAlpha]_)) \[CenterDot] 
              (\[FormalA]_)))) -> \[FormalB], "Side" -> 1, 
        "Subpattern" -> (\[FormalC]_) \[CenterDot] (\[FormalAlpha]_), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (((\[FormalA]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
              (\[FormalC]_)) \[CenterDot] (\[FormalA]_))) -> \[FormalC], 
        "MatchingSide" -> 1, "Position" -> {2, 1, 1}|>|>, 
    {"SubstitutionLemma", 3} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CenterDot] 
           \[FormalA]) \[CenterDot] ((\[FormalC] \[CenterDot] 
            \[FormalB]) \[CenterDot] (((\[FormalC] \[CenterDot] 
              \[FormalB]) \[CenterDot] \[FormalA]) \[CenterDot] 
            ((((\[FormalAlpha] \[CenterDot] \[FormalBeta]) \[CenterDot] 
               \[FormalC]) \[CenterDot] (\[FormalAlpha] \[CenterDot] (
                (\[FormalAlpha] \[CenterDot] \[FormalC]) \[CenterDot] 
                \[FormalAlpha]))) \[CenterDot] \[FormalB])))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 5}, 
        "Position" -> {2, 2, 1, 1, 1}, "Construct" -> {"Axiom", 1}, 
        "Orientation" -> -1, "Rule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalC], "OutputExpression" -> 
         HoldForm[\[FormalA] == (\[FormalB] \[CenterDot] 
             \[FormalA]) \[CenterDot] ((\[FormalC] \[CenterDot] 
              \[FormalB]) \[CenterDot] (((\[FormalC] \[CenterDot] 
                \[FormalB]) \[CenterDot] \[FormalA]) \[CenterDot] 
              ((((\[FormalAlpha] \[CenterDot] \[FormalBeta]) \[CenterDot] 
                 \[FormalC]) \[CenterDot] (\[FormalAlpha] \[CenterDot] 
                 ((\[FormalAlpha] \[CenterDot] \[FormalC]) \[CenterDot] 
                  \[FormalAlpha]))) \[CenterDot] \[FormalB])))], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 4} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CenterDot] 
           \[FormalA]) \[CenterDot] ((\[FormalC] \[CenterDot] 
            \[FormalB]) \[CenterDot] (((\[FormalC] \[CenterDot] 
              \[FormalB]) \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalC] \[CenterDot] \[FormalB])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 3}, 
        "Position" -> {2, 2, 2, 1}, "Construct" -> {"Axiom", 1}, 
        "Orientation" -> -1, "Rule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalC], "OutputExpression" -> 
         HoldForm[\[FormalA] == (\[FormalB] \[CenterDot] 
             \[FormalA]) \[CenterDot] ((\[FormalC] \[CenterDot] 
              \[FormalB]) \[CenterDot] (((\[FormalC] \[CenterDot] 
                \[FormalB]) \[CenterDot] \[FormalA]) \[CenterDot] 
              (\[FormalC] \[CenterDot] \[FormalB])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, {"CriticalPairLemma", 6} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         (((\[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
               \[FormalC]) \[CenterDot] \[FormalB])) \[CenterDot] 
            \[FormalC]) \[CenterDot] \[FormalA]) \[CenterDot] 
          (\[FormalC] \[CenterDot] ((\[FormalC] \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalC]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 2}, 
        "Orientation" -> -1, "Rule" -> 
         (((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] (
                \[FormalB]_)) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalC], "Side" -> 1, 
        "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           (\[FormalB]_)) \[CenterDot] (\[FormalA]_), "MatchingConstruct" -> 
         {"Axiom", 1}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalC], "MatchingSide" -> 1, 
        "Position" -> {1, 1, 2}|>|>, {"CriticalPairLemma", 7} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA]) == 
         \[FormalB] \[CenterDot] ((\[FormalC] \[CenterDot] 
            ((\[FormalC] \[CenterDot] \[FormalA]) \[CenterDot] 
             \[FormalC])) \[CenterDot] (((\[FormalC] \[CenterDot] 
              ((\[FormalC] \[CenterDot] \[FormalA]) \[CenterDot] 
               \[FormalC])) \[CenterDot] (\[FormalA] \[CenterDot] 
              ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
               \[FormalA]))) \[CenterDot] (\[FormalC] \[CenterDot] 
             ((\[FormalC] \[CenterDot] \[FormalA]) \[CenterDot] 
              \[FormalC]))))], "Proof" -> <|"Construct" -> {"Axiom", 1}, 
        "Orientation" -> -1, "Rule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalC], "Side" -> 1, 
        "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           (\[FormalB]_)) \[CenterDot] (\[FormalC]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 2}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (((\[FormalA]_) \[CenterDot] 
             (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
              (\[FormalA]_))) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
              (\[FormalC]_)) \[CenterDot] (\[FormalB]_))) -> \[FormalC], 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 8} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA]) == 
         (\[FormalB] \[CenterDot] (\[FormalA] \[CenterDot] 
            ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
             \[FormalA]))) \[CenterDot] (((\[FormalA] \[CenterDot] 
             \[FormalC]) \[CenterDot] \[FormalB]) \[CenterDot] 
           (\[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalC]) \[CenterDot] \[FormalB])))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 4}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] ((((\[FormalC]_) \[CenterDot] (
                \[FormalA]_)) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] (\[FormalA]_)))) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> ((\[FormalC]_) \[CenterDot] 
           (\[FormalA]_)) \[CenterDot] (\[FormalB]_), "MatchingConstruct" -> 
         {"Axiom", 1}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalC], "MatchingSide" -> 1, 
        "Position" -> {2, 2, 1}|>|>, {"CriticalPairLemma", 9} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA]) == 
         (\[FormalB] \[CenterDot] (\[FormalA] \[CenterDot] 
            ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
             \[FormalA]))) \[CenterDot] (((\[FormalC] \[CenterDot] 
             ((\[FormalC] \[CenterDot] \[FormalA]) \[CenterDot] 
              \[FormalC])) \[CenterDot] \[FormalB]) \[CenterDot] 
           (\[FormalB] \[CenterDot] ((\[FormalC] \[CenterDot] 
              ((\[FormalC] \[CenterDot] \[FormalA]) \[CenterDot] 
               \[FormalC])) \[CenterDot] \[FormalB])))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 4}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] ((((\[FormalC]_) \[CenterDot] (
                \[FormalA]_)) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] (\[FormalA]_)))) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> ((\[FormalC]_) \[CenterDot] 
           (\[FormalA]_)) \[CenterDot] (\[FormalB]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 2}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (((\[FormalA]_) \[CenterDot] 
             (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
              (\[FormalA]_))) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
              (\[FormalC]_)) \[CenterDot] (\[FormalB]_))) -> \[FormalC], 
        "MatchingSide" -> 1, "Position" -> {2, 2, 1}|>|>, 
    {"CriticalPairLemma", 10} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalC]) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB])) == \[FormalC] \[CenterDot] 
          (\[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
             ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
              (((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
                \[FormalC]) \[CenterDot] (\[FormalA] \[CenterDot] 
                \[FormalB])))) \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalC], "Side" -> 1, 
        "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           (\[FormalB]_)) \[CenterDot] (\[FormalC]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 4}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] ((((\[FormalC]_) \[CenterDot] (
                \[FormalA]_)) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] (\[FormalA]_)))) -> \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 11} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalC]) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB])) == 
         (((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
            \[FormalAlpha]) \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] (((\[FormalA] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalC]) \[CenterDot] 
             (\[FormalA] \[CenterDot] \[FormalB])))) \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           (\[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalC])))], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalC], "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CenterDot] (\[FormalC]_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 4}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (((\[FormalC]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
            ((((\[FormalC]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
              (\[FormalB]_)) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
              (\[FormalA]_)))) -> \[FormalB], "MatchingSide" -> 1, 
        "Position" -> {2, 2, 1}|>|>, {"CriticalPairLemma", 12} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalC]) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB])) == 
         (\[FormalC] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] (((\[FormalA] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalC]) \[CenterDot] 
             (\[FormalA] \[CenterDot] \[FormalB])))) \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           (\[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalC])))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 4}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] ((((\[FormalC]_) \[CenterDot] (
                \[FormalA]_)) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] (\[FormalA]_)))) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> ((\[FormalC]_) \[CenterDot] 
           (\[FormalA]_)) \[CenterDot] (\[FormalB]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 4}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] ((((\[FormalC]_) \[CenterDot] (
                \[FormalA]_)) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] (\[FormalA]_)))) -> \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {2, 2, 1}|>|>, 
    {"CriticalPairLemma", 13} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA]) == 
         \[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
           ((\[FormalB] \[CenterDot] (\[FormalA] \[CenterDot] 
              ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
               \[FormalA]))) \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 6}, 
        "Orientation" -> -1, "Rule" -> 
         ((((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
                (\[FormalB]_)) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
              (\[FormalC]_)) \[CenterDot] (\[FormalB]_))) -> \[FormalC], 
        "Side" -> 1, "Subpattern" -> (((\[FormalA]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
             (\[FormalA]_))) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
          (\[FormalC]_), "MatchingConstruct" -> {"Axiom", 1}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalC], "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 14} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalA])) \[CenterDot] \[FormalB] == \[FormalB] \[CenterDot] 
          (\[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
             ((\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
                 \[FormalB]) \[CenterDot] \[FormalA])) \[CenterDot] 
              \[FormalB])) \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 6}, 
        "Orientation" -> -1, "Rule" -> 
         ((((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
                (\[FormalB]_)) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
              (\[FormalC]_)) \[CenterDot] (\[FormalB]_))) -> \[FormalC], 
        "Side" -> 1, "Subpattern" -> (((\[FormalA]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
             (\[FormalA]_))) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
          (\[FormalC]_), "MatchingConstruct" -> {"CriticalPairLemma", 3}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
                (\[FormalB]_)) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] (
                \[FormalB]_)) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
            (\[FormalB]_)) -> \[FormalB], "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 15} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA]) == 
         \[FormalB] \[CenterDot] (((\[FormalC] \[CenterDot] 
             ((\[FormalC] \[CenterDot] \[FormalA]) \[CenterDot] 
              \[FormalC])) \[CenterDot] \[FormalA]) \[CenterDot] 
           ((((\[FormalC] \[CenterDot] ((\[FormalC] \[CenterDot] 
                 \[FormalA]) \[CenterDot] \[FormalC])) \[CenterDot] 
              \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
              ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
               \[FormalA]))) \[CenterDot] ((\[FormalC] \[CenterDot] 
              ((\[FormalC] \[CenterDot] \[FormalA]) \[CenterDot] 
               \[FormalC])) \[CenterDot] \[FormalA])))], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalC], "Side" -> 1, 
        "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           (\[FormalB]_)) \[CenterDot] (\[FormalC]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 6}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((((\[FormalA]_) \[CenterDot] 
              (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] (
                \[FormalA]_))) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalC], "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 16} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalA]) == 
         (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
            ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
             \[FormalA]))) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 8}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] (
                \[FormalA]_)) \[CenterDot] (\[FormalB]_)))) \[CenterDot] 
           ((((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
              (\[FormalA]_)))) -> \[FormalB] \[CenterDot] 
           ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalB]), 
        "Side" -> 1, "Subpattern" -> (((\[FormalB]_) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] (\[FormalA]_))), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (((\[FormalA]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
              (\[FormalC]_)) \[CenterDot] (\[FormalA]_))) -> \[FormalC], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 17} -> 
     <|"Statement" -> HoldForm[
        ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           \[FormalC]) \[CenterDot] (\[FormalC] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalC])) == 
         (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalC]) \[CenterDot] \[FormalA])) \[CenterDot] 
          (\[FormalC] \[CenterDot] ((\[FormalC] \[CenterDot] 
             (((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
               \[FormalC]) \[CenterDot] (\[FormalC] \[CenterDot] (
                (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
                \[FormalC])))) \[CenterDot] \[FormalC]))], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalC], "Side" -> 1, 
        "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           (\[FormalB]_)) \[CenterDot] (\[FormalC]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 8}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] (
                \[FormalA]_)) \[CenterDot] (\[FormalB]_)))) \[CenterDot] 
           ((((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
              (\[FormalA]_)))) -> \[FormalB] \[CenterDot] 
           ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalB]), 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 18} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
            \[FormalA])) \[CenterDot] (\[FormalA] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalC], "Side" -> 1, 
        "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           (\[FormalB]_)) \[CenterDot] (\[FormalC]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 16}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] (
                \[FormalA]_)) \[CenterDot] (\[FormalA]_)))) \[CenterDot] 
           (\[FormalA]_) -> \[FormalA] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalA]), 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 19} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
             ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
              \[FormalB]))) \[CenterDot] \[FormalA]) \[CenterDot] 
          ((\[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
              \[FormalB]) \[CenterDot] \[FormalB])) \[CenterDot] 
           (((\[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
                \[FormalB]) \[CenterDot] \[FormalB])) \[CenterDot] 
             \[FormalA]) \[CenterDot] (\[FormalB] \[CenterDot] 
             ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
              \[FormalB]))))], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 2}, "Orientation" -> -1, 
        "Rule" -> (((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] (
                \[FormalB]_)) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalC], "Side" -> 1, 
        "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           (\[FormalB]_)) \[CenterDot] (\[FormalA]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 16}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] (
                \[FormalA]_)) \[CenterDot] (\[FormalA]_)))) \[CenterDot] 
           (\[FormalA]_) -> \[FormalA] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalA]), 
        "MatchingSide" -> 1, "Position" -> {1, 1, 2}|>|>, 
    {"CriticalPairLemma", 20} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalA]) == 
         (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
            ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
             \[FormalA]))) \[CenterDot] ((\[FormalA] \[CenterDot] 
            ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
             \[FormalA])) \[CenterDot] (\[FormalA] \[CenterDot] 
            ((\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] (
                (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
                \[FormalA]))) \[CenterDot] \[FormalA])))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 8}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] (
                \[FormalA]_)) \[CenterDot] (\[FormalB]_)))) \[CenterDot] 
           ((((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
              (\[FormalA]_)))) -> \[FormalB] \[CenterDot] 
           ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalB]), 
        "Side" -> 1, "Subpattern" -> ((\[FormalB]_) \[CenterDot] 
           (\[FormalC]_)) \[CenterDot] (\[FormalA]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 16}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] (
                \[FormalA]_)) \[CenterDot] (\[FormalA]_)))) \[CenterDot] 
           (\[FormalA]_) -> \[FormalA] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalA]), 
        "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"SubstitutionLemma", 5} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalA]) == 
         (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
            ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
             \[FormalA]))) \[CenterDot] ((\[FormalA] \[CenterDot] 
            ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
             \[FormalA])) \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalA]) \[CenterDot] \[FormalA]))))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 20}, 
        "Position" -> {2, 2, 2}, "Construct" -> {"CriticalPairLemma", 16}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] (
                \[FormalA]_)) \[CenterDot] (\[FormalA]_)))) \[CenterDot] 
           (\[FormalA]_) -> \[FormalA] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalA]), 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] 
            ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalA]) == 
           (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
              ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
               \[FormalA]))) \[CenterDot] ((\[FormalA] \[CenterDot] 
              ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
               \[FormalA])) \[CenterDot] (\[FormalA] \[CenterDot] 
              (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
                 \[FormalA]) \[CenterDot] \[FormalA]))))], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 21} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalA]) == 
         (((\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalA]) \[CenterDot] \[FormalA])) \[CenterDot] 
            \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
            ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
             \[FormalA]))) \[CenterDot] ((\[FormalA] \[CenterDot] 
            ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
             \[FormalA])) \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalA]) \[CenterDot] \[FormalA]))))], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalC], "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CenterDot] (\[FormalC]_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 18}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalA]_))) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2, 2, 1}|>|>, 
    {"CriticalPairLemma", 22} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalA]) == 
         (((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
            \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
            ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
             \[FormalA]))) \[CenterDot] ((\[FormalA] \[CenterDot] 
            ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
             \[FormalA])) \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalA]) \[CenterDot] \[FormalA]))))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 4}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] ((((\[FormalC]_) \[CenterDot] (
                \[FormalA]_)) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] (\[FormalA]_)))) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> ((\[FormalC]_) \[CenterDot] 
           (\[FormalA]_)) \[CenterDot] (\[FormalB]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 18}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
             (\[FormalA]_))) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2, 2, 1}|>|>, {"SubstitutionLemma", 6} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalA]) == 
         \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
            ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
             \[FormalA])) \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalA]) \[CenterDot] \[FormalA]))))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 22}, "Position" -> {1}, 
        "Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalC], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalA]) \[CenterDot] \[FormalA]) == \[FormalA] \[CenterDot] 
            ((\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
                \[FormalA]) \[CenterDot] \[FormalA])) \[CenterDot] 
             (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] (
                (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
                \[FormalA]))))], "ConstructSide" -> 1, "InputOrientation" -> 
         1, "Side" -> 2|>|>, {"CriticalPairLemma", 23} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
             ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
              \[FormalA]))) \[CenterDot] (\[FormalA] \[CenterDot] 
            \[FormalB])) == (\[FormalA] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalA])) \[CenterDot] (\[FormalB] \[CenterDot] 
           ((\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalA])) \[CenterDot] 
            \[FormalB]))], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 
          10}, "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
              (((\[FormalC]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] (
                (((\[FormalC]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
                 (\[FormalA]_)) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
                 (\[FormalB]_))))) \[CenterDot] (\[FormalB]_))) -> 
          (\[FormalC] \[CenterDot] \[FormalB]) \[CenterDot] 
           (((\[FormalC] \[CenterDot] \[FormalB]) \[CenterDot] 
             \[FormalA]) \[CenterDot] (\[FormalC] \[CenterDot] \[FormalB])), 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          (((\[FormalC]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((((\[FormalC]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalB]_)))), "MatchingConstruct" -> {"CriticalPairLemma", 
          1}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] ((((\[FormalB]_) \[CenterDot] (
                \[FormalC]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] (
                ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
                (\[FormalB]_)))) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
              (\[FormalC]_)))) -> \[FormalB] \[CenterDot] 
           ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalB]), 
        "MatchingSide" -> 1, "Position" -> {2, 2, 1}|>|>, 
    {"CriticalPairLemma", 24} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
            \[FormalA])) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalA]) \[CenterDot] \[FormalA]))) == 
         (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalA])) \[CenterDot] 
          (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
             ((\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
                 \[FormalA]) \[CenterDot] \[FormalA])) \[CenterDot] 
              (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
                ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
                 \[FormalA]))))) \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalC], "Side" -> 1, 
        "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           (\[FormalB]_)) \[CenterDot] (\[FormalC]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 5}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] (
                \[FormalA]_)) \[CenterDot] (\[FormalA]_)))) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] (
                \[FormalA]_)) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] (
                \[FormalA]_))))) -> \[FormalA] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalA]), 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"SubstitutionLemma", 7} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
            \[FormalA])) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalA]) \[CenterDot] \[FormalA]))) == 
         (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalA])) \[CenterDot] 
          (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
             ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
              \[FormalA])) \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 24}, 
        "Position" -> {2, 2, 1}, "Construct" -> {"SubstitutionLemma", 6}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] (
                \[FormalA]_)) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] (
                \[FormalA]_))))) -> \[FormalA] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalA]), 
        "OutputExpression" -> HoldForm[(\[FormalA] \[CenterDot] 
             ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
              \[FormalA])) \[CenterDot] (\[FormalA] \[CenterDot] 
             (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
                \[FormalA]) \[CenterDot] \[FormalA]))) == 
           (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalA]) \[CenterDot] \[FormalA])) \[CenterDot] 
            (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] (
                (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
                \[FormalA])) \[CenterDot] \[FormalA]))], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 8} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
            \[FormalA])) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalA]) \[CenterDot] \[FormalA]))) == 
         (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
          (((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalA]) \[CenterDot] \[FormalA]))) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 7}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 23}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
              (\[FormalB]_)) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
              (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] (
                \[FormalA]_))) \[CenterDot] (\[FormalB]_))) -> 
          (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           (((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
             (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
                \[FormalB]) \[CenterDot] \[FormalA]))) \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB])), "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalA]) \[CenterDot] \[FormalA])) \[CenterDot] 
            (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
              ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
               \[FormalA]))) == (\[FormalA] \[CenterDot] 
             \[FormalA]) \[CenterDot] (((\[FormalA] \[CenterDot] 
               \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] (
                (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
                \[FormalA]))) \[CenterDot] (\[FormalA] \[CenterDot] 
              \[FormalA]))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 25} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalA]) == 
         ((\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
             ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
              \[FormalA]))) \[CenterDot] (\[FormalA] \[CenterDot] 
            ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
             \[FormalA]))) \[CenterDot] ((\[FormalA] \[CenterDot] 
            ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
             \[FormalA])) \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalA]) \[CenterDot] \[FormalA]))))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 19}, 
        "Orientation" -> -1, "Rule" -> 
         (((\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] (
                \[FormalA]_)))) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] (
                \[FormalA]_)) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
            ((((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
                 (\[FormalA]_)) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
              (\[FormalB]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] (
                \[FormalA]_))))) -> \[FormalB], "Side" -> 1, 
        "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
            (\[FormalA]_))) \[CenterDot] (\[FormalB]_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 18}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalA]_))) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2, 2, 1}|>|>, 
    {"CriticalPairLemma", 26} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalA]) == 
         \[FormalA] \[CenterDot] (((\[FormalB] \[CenterDot] 
             ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
              \[FormalB])) \[CenterDot] \[FormalA]) \[CenterDot] 
           (\[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
              ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
               \[FormalB])) \[CenterDot] \[FormalA])))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 15}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((((\[FormalB]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
                (\[FormalC]_)) \[CenterDot] (\[FormalB]_))) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] (((((\[FormalB]_) \[CenterDot] 
                (((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
                 (\[FormalB]_))) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
              ((\[FormalC]_) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
                 (\[FormalA]_)) \[CenterDot] (\[FormalC]_)))) \[CenterDot] 
             (((\[FormalB]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
                 (\[FormalC]_)) \[CenterDot] (\[FormalB]_))) \[CenterDot] 
              (\[FormalC]_)))) -> \[FormalC] \[CenterDot] 
           ((\[FormalC] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalC]), 
        "Side" -> 1, "Subpattern" -> (((\[FormalB]_) \[CenterDot] 
            (((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
             (\[FormalB]_))) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
          ((\[FormalC]_) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] (\[FormalC]_))), 
        "MatchingConstruct" -> {"SubstitutionLemma", 2}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] (
                \[FormalB]_)) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalC], "MatchingSide" -> 1, 
        "Position" -> {2, 2, 1}|>|>, {"CriticalPairLemma", 27} -> 
     <|"Statement" -> HoldForm[
        ((\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] \[FormalA])) \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalB] \[CenterDot] 
           ((\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalA])) \[CenterDot] 
            \[FormalB])) == (\[FormalA] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalA])) \[CenterDot] (\[FormalB] \[CenterDot] 
           ((\[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalB])) \[CenterDot] 
            \[FormalB]))], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 
          17}, "Orientation" -> -1, "Rule" -> 
         ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
              (\[FormalB]_)) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
              ((((\[FormalA]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
                (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
                (((\[FormalA]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
                 (\[FormalB]_))))) \[CenterDot] (\[FormalB]_))) -> 
          ((\[FormalA] \[CenterDot] \[FormalC]) \[CenterDot] 
            \[FormalB]) \[CenterDot] (\[FormalB] \[CenterDot] 
            ((\[FormalA] \[CenterDot] \[FormalC]) \[CenterDot] \[FormalB])), 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          ((((\[FormalA]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
             (\[FormalB]_)))), "MatchingConstruct" -> {"CriticalPairLemma", 
          26}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((((\[FormalB]_) \[CenterDot] 
              (((\[FormalB]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] (
                \[FormalB]_))) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] (
                ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
                (\[FormalB]_))) \[CenterDot] (\[FormalA]_)))) -> 
          \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalA]), "MatchingSide" -> 1, 
        "Position" -> {2, 2, 1}|>|>, {"CriticalPairLemma", 28} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA]) == 
         (\[FormalB] \[CenterDot] (\[FormalA] \[CenterDot] 
            ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
             \[FormalA]))) \[CenterDot] ((\[FormalA] \[CenterDot] 
            ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
             \[FormalA])) \[CenterDot] (\[FormalB] \[CenterDot] 
            ((\[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
                \[FormalB]) \[CenterDot] \[FormalB])) \[CenterDot] 
             \[FormalB])))], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 8}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (((\[FormalB]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
              (\[FormalB]_)))) \[CenterDot] ((((\[FormalB]_) \[CenterDot] 
              (\[FormalC]_)) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] (
                \[FormalC]_)) \[CenterDot] (\[FormalA]_)))) -> 
          \[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalB]), "Side" -> 1, 
        "Subpattern" -> (((\[FormalB]_) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] (\[FormalA]_))), 
        "MatchingConstruct" -> {"CriticalPairLemma", 27}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] (
                \[FormalB]_)) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
                (\[FormalB]_)) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
             (\[FormalB]_))) <-> ((\[FormalA]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
             (\[FormalA]_))) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (((\[FormalB]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
                (\[FormalB]_)) \[CenterDot] (\[FormalB]_))) \[CenterDot] 
             (\[FormalB]_))), "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 29} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalA]) == 
         \[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
            ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
             \[FormalB])) \[CenterDot] (\[FormalA] \[CenterDot] 
            ((\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
                \[FormalA]) \[CenterDot] \[FormalA])) \[CenterDot] 
             \[FormalA])))], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 26}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] ((((\[FormalB]_) \[CenterDot] 
              (((\[FormalB]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] (
                \[FormalB]_))) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] (
                ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
                (\[FormalB]_))) \[CenterDot] (\[FormalA]_)))) -> 
          \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalA]), "Side" -> 1, 
        "Subpattern" -> (((\[FormalB]_) \[CenterDot] 
            (((\[FormalB]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
             (\[FormalB]_))) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (((\[FormalB]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
              (\[FormalB]_))) \[CenterDot] (\[FormalA]_))), 
        "MatchingConstruct" -> {"CriticalPairLemma", 27}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] (
                \[FormalB]_)) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
                (\[FormalB]_)) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
             (\[FormalB]_))) <-> ((\[FormalA]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
             (\[FormalA]_))) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (((\[FormalB]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
                (\[FormalB]_)) \[CenterDot] (\[FormalB]_))) \[CenterDot] 
             (\[FormalB]_))), "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 30} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalA]) == 
         (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
            ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
             \[FormalA]))) \[CenterDot] ((\[FormalB] \[CenterDot] 
            ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
             \[FormalB])) \[CenterDot] (\[FormalA] \[CenterDot] 
            ((\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
                \[FormalA]) \[CenterDot] \[FormalA])) \[CenterDot] 
             \[FormalA])))], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 9}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (((\[FormalB]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
              (\[FormalB]_)))) \[CenterDot] ((((\[FormalC]_) \[CenterDot] 
              (((\[FormalC]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] (
                \[FormalC]_))) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (((\[FormalC]_) \[CenterDot] (
                ((\[FormalC]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
                (\[FormalC]_))) \[CenterDot] (\[FormalA]_)))) -> 
          \[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalB]), "Side" -> 1, 
        "Subpattern" -> (((\[FormalC]_) \[CenterDot] 
            (((\[FormalC]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             (((\[FormalC]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
              (\[FormalC]_))) \[CenterDot] (\[FormalA]_))), 
        "MatchingConstruct" -> {"CriticalPairLemma", 27}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] (
                \[FormalB]_)) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
                (\[FormalB]_)) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
             (\[FormalB]_))) <-> ((\[FormalA]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
             (\[FormalA]_))) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (((\[FormalB]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
                (\[FormalB]_)) \[CenterDot] (\[FormalB]_))) \[CenterDot] 
             (\[FormalB]_))), "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 31} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalA])) \[CenterDot] (\[FormalB] \[CenterDot] 
           ((\[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalB])) \[CenterDot] 
            \[FormalB])) == (\[FormalB] \[CenterDot] 
           ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalB])) \[CenterDot] (\[FormalB] \[CenterDot] 
           ((\[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] (
                (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
                \[FormalA])) \[CenterDot] (\[FormalB] \[CenterDot] (
                (\[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
                   \[FormalB]) \[CenterDot] \[FormalB])) \[CenterDot] 
                \[FormalB])))) \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalC], "Side" -> 1, 
        "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           (\[FormalB]_)) \[CenterDot] (\[FormalC]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 30}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] (
                \[FormalA]_)) \[CenterDot] (\[FormalA]_)))) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] (
                \[FormalA]_)) \[CenterDot] (\[FormalB]_))) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] (
                ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
                (\[FormalA]_))) \[CenterDot] (\[FormalA]_)))) -> 
          \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalA]), "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 9} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalA])) \[CenterDot] (\[FormalB] \[CenterDot] 
           ((\[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalB])) \[CenterDot] 
            \[FormalB])) == (\[FormalB] \[CenterDot] 
           ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalB])) \[CenterDot] (\[FormalB] \[CenterDot] 
           ((\[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalB])) \[CenterDot] 
            \[FormalB]))], "Proof" -> <|"Input" -> {"CriticalPairLemma", 31}, 
        "Position" -> {2, 2, 1}, "Construct" -> {"CriticalPairLemma", 29}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] (
                \[FormalA]_)) \[CenterDot] (\[FormalB]_))) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] (
                ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
                (\[FormalA]_))) \[CenterDot] (\[FormalA]_)))) -> 
          \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalA]), "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalA])) \[CenterDot] 
            (\[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] (
                (\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
                \[FormalB])) \[CenterDot] \[FormalB])) == 
           (\[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalB])) \[CenterDot] 
            (\[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] (
                (\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
                \[FormalB])) \[CenterDot] \[FormalB]))], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 10} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalA])) \[CenterDot] (\[FormalB] \[CenterDot] 
           ((\[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalB])) \[CenterDot] 
            \[FormalB])) == (\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
          (((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
            (\[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalB]))) \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 9}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 23}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
              (\[FormalB]_)) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
              (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] (
                \[FormalA]_))) \[CenterDot] (\[FormalB]_))) -> 
          (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           (((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
             (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
                \[FormalB]) \[CenterDot] \[FormalA]))) \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB])), "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalA])) \[CenterDot] 
            (\[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] (
                (\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
                \[FormalB])) \[CenterDot] \[FormalB])) == 
           (\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
            (((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
              (\[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
                 \[FormalB]) \[CenterDot] \[FormalB]))) \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalB]))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 11} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalA])) \[CenterDot] (\[FormalB] \[CenterDot] 
           ((\[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalB])) \[CenterDot] 
            \[FormalB])) == (\[FormalB] \[CenterDot] 
           ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalB])) \[CenterDot] (\[FormalB] \[CenterDot] 
           (\[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
              \[FormalB]) \[CenterDot] \[FormalB])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 10}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 8}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
             ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
                (\[FormalA]_)) \[CenterDot] (\[FormalA]_)))) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalA]_))) -> 
          (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalA]) \[CenterDot] \[FormalA])) \[CenterDot] 
           (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
             ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
              \[FormalA]))), "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalA])) \[CenterDot] 
            (\[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] (
                (\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
                \[FormalB])) \[CenterDot] \[FormalB])) == 
           (\[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalB])) \[CenterDot] 
            (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
              ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
               \[FormalB])))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 32} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA]) == 
         (\[FormalB] \[CenterDot] (\[FormalA] \[CenterDot] 
            ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
             \[FormalA]))) \[CenterDot] ((\[FormalB] \[CenterDot] 
            ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
             \[FormalB])) \[CenterDot] (\[FormalB] \[CenterDot] 
            (\[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalB]))))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 28}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] (
                \[FormalA]_)) \[CenterDot] (\[FormalB]_)))) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] (
                \[FormalA]_)) \[CenterDot] (\[FormalB]_))) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] (
                ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
                (\[FormalA]_))) \[CenterDot] (\[FormalA]_)))) -> 
          \[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalB]), "Side" -> 1, 
        "Subpattern" -> ((\[FormalB]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
            (\[FormalB]_))) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] (
                \[FormalA]_)) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
            (\[FormalA]_))), "MatchingConstruct" -> {"SubstitutionLemma", 
          11}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
              (\[FormalB]_)) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
              (((\[FormalB]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] (
                \[FormalB]_))) \[CenterDot] (\[FormalB]_))) <-> 
          ((\[FormalB]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
              (\[FormalB]_)) \[CenterDot] (\[FormalB]_))) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (((\[FormalB]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
              (\[FormalB]_)))), "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 33} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] (\[FormalB] \[CenterDot] 
             ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
              \[FormalB]))) \[CenterDot] (\[FormalA] \[CenterDot] 
            \[FormalB])) == (((\[FormalB] \[CenterDot] 
             (\[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
                \[FormalB]) \[CenterDot] \[FormalB]))) \[CenterDot] 
            \[FormalC]) \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] (((\[FormalA] \[CenterDot] 
               \[FormalB]) \[CenterDot] (\[FormalB] \[CenterDot] (
                (\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
                \[FormalB]))) \[CenterDot] (\[FormalA] \[CenterDot] 
              \[FormalB])))) \[CenterDot] (\[FormalB] \[CenterDot] 
           ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 11}, 
        "Orientation" -> -1, "Rule" -> 
         ((((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] (((\[FormalAlpha]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] ((((\[FormalAlpha]_) \[CenterDot] 
                (\[FormalA]_)) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
              ((\[FormalAlpha]_) \[CenterDot] (\[FormalA]_))))) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (\[FormalB]_)))) -> (\[FormalAlpha] \[CenterDot] 
            \[FormalA]) \[CenterDot] (((\[FormalAlpha] \[CenterDot] 
              \[FormalA]) \[CenterDot] \[FormalB]) \[CenterDot] 
            (\[FormalAlpha] \[CenterDot] \[FormalA])), "Side" -> 1, 
        "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalB]_))), 
        "MatchingConstruct" -> {"CriticalPairLemma", 32}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (((\[FormalB]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
              (\[FormalB]_)))) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
              (\[FormalA]_))) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
                (\[FormalA]_)) \[CenterDot] (\[FormalA]_))))) -> 
          \[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalB]), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 34} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] (\[FormalB] \[CenterDot] 
             ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
              \[FormalB]))) \[CenterDot] (\[FormalA] \[CenterDot] 
            \[FormalB])) == ((\[FormalB] \[CenterDot] 
            ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
             \[FormalB])) \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] (((\[FormalA] \[CenterDot] 
               \[FormalB]) \[CenterDot] (\[FormalB] \[CenterDot] (
                (\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
                \[FormalB]))) \[CenterDot] (\[FormalA] \[CenterDot] 
              \[FormalB])))) \[CenterDot] (\[FormalB] \[CenterDot] 
           ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 12}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
             ((((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] (
                \[FormalA]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] (
                \[FormalC]_))))) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] (\[FormalA]_)))) -> 
          (\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           (((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
             \[FormalA]) \[CenterDot] (\[FormalB] \[CenterDot] \[FormalC])), 
        "Side" -> 1, "Subpattern" -> ((\[FormalC]_) \[CenterDot] 
           (\[FormalA]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
           ((\[FormalC]_) \[CenterDot] (\[FormalA]_))), 
        "MatchingConstruct" -> {"CriticalPairLemma", 32}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (((\[FormalB]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
              (\[FormalB]_)))) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
              (\[FormalA]_))) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
                (\[FormalA]_)) \[CenterDot] (\[FormalA]_))))) -> 
          \[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalB]), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 35} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalA]) == 
         ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
           (((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
             (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
                \[FormalA]) \[CenterDot] \[FormalA]))) \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalA]))) \[CenterDot] 
          ((\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalA]) \[CenterDot] \[FormalA])) \[CenterDot] 
           (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
             ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
              \[FormalA]))))], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 21}, "Orientation" -> -1, 
        "Rule" -> ((((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
                (\[FormalA]_)) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
              (\[FormalA]_)))) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
              (\[FormalA]_))) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
                (\[FormalA]_)) \[CenterDot] (\[FormalA]_))))) -> 
          \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalA]), "Side" -> 1, 
        "Subpattern" -> (((\[FormalA]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
             (\[FormalA]_))) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] (\[FormalA]_))), 
        "MatchingConstruct" -> {"CriticalPairLemma", 34}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] (
                \[FormalA]_)) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
            (((\[FormalB]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
             ((((\[FormalB]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] (
                (\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
                  (\[FormalA]_)) \[CenterDot] (\[FormalA]_)))) \[CenterDot] 
              ((\[FormalB]_) \[CenterDot] (\[FormalA]_))))) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalA]_))) -> 
          (\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
           (((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
             (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
                \[FormalA]) \[CenterDot] \[FormalA]))) \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalA])), "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 36} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalA]) == 
         ((\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalA]) \[CenterDot] \[FormalA])) \[CenterDot] 
           (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
             ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
              \[FormalA])))) \[CenterDot] ((\[FormalA] \[CenterDot] 
            ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
             \[FormalA])) \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalA]) \[CenterDot] \[FormalA]))))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 35}, 
        "Orientation" -> -1, "Rule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            ((((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
              ((\[FormalB]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
                 (\[FormalB]_)) \[CenterDot] (\[FormalB]_)))) \[CenterDot] 
             ((\[FormalA]_) \[CenterDot] (\[FormalB]_)))) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] (
                \[FormalB]_)) \[CenterDot] (\[FormalB]_))) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
              (((\[FormalB]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] (
                \[FormalB]_))))) -> \[FormalB] \[CenterDot] 
           ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalB]), 
        "Side" -> 1, "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           (\[FormalB]_)) \[CenterDot] ((((\[FormalA]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (((\[FormalB]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
              (\[FormalB]_)))) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_))), "MatchingConstruct" -> {"SubstitutionLemma", 8}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
             ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
                (\[FormalA]_)) \[CenterDot] (\[FormalA]_)))) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalA]_))) -> 
          (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalA]) \[CenterDot] \[FormalA])) \[CenterDot] 
           (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
             ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
              \[FormalA]))), "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 37} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] (((\[FormalA] \[CenterDot] 
             \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
             ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
              \[FormalA]))) \[CenterDot] (\[FormalA] \[CenterDot] 
            \[FormalA])) == (((\[FormalA] \[CenterDot] 
             (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
                \[FormalA]) \[CenterDot] \[FormalA]))) \[CenterDot] 
            \[FormalB]) \[CenterDot] ((\[FormalA] \[CenterDot] 
             ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
              \[FormalA])) \[CenterDot] (\[FormalA] \[CenterDot] 
             (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
                \[FormalA]) \[CenterDot] \[FormalA]))))) \[CenterDot] 
          (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 33}, 
        "Orientation" -> -1, "Rule" -> 
         ((((\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] (
                ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
                (\[FormalA]_)))) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (((\[FormalC]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
             ((((\[FormalC]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] (
                (\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
                  (\[FormalA]_)) \[CenterDot] (\[FormalA]_)))) \[CenterDot] 
              ((\[FormalC]_) \[CenterDot] (\[FormalA]_))))) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalA]_))) -> 
          (\[FormalC] \[CenterDot] \[FormalA]) \[CenterDot] 
           (((\[FormalC] \[CenterDot] \[FormalA]) \[CenterDot] 
             (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
                \[FormalA]) \[CenterDot] \[FormalA]))) \[CenterDot] 
            (\[FormalC] \[CenterDot] \[FormalA])), "Side" -> 1, 
        "Subpattern" -> ((\[FormalC]_) \[CenterDot] 
           (\[FormalA]_)) \[CenterDot] ((((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
              (\[FormalA]_)))) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
            (\[FormalA]_))), "MatchingConstruct" -> {"SubstitutionLemma", 8}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
             ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
                (\[FormalA]_)) \[CenterDot] (\[FormalA]_)))) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalA]_))) -> 
          (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalA]) \[CenterDot] \[FormalA])) \[CenterDot] 
           (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
             ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
              \[FormalA]))), "MatchingSide" -> 1, "Position" -> {1, 2}|>|>, 
    {"SubstitutionLemma", 12} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
            \[FormalA])) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalA]) \[CenterDot] \[FormalA]))) == 
         (((\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
              ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
               \[FormalA]))) \[CenterDot] \[FormalB]) \[CenterDot] 
           ((\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalA]) \[CenterDot] \[FormalA])) \[CenterDot] 
            (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
              ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
               \[FormalA]))))) \[CenterDot] (\[FormalA] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 37}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 8}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
             ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
                (\[FormalA]_)) \[CenterDot] (\[FormalA]_)))) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalA]_))) -> 
          (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalA]) \[CenterDot] \[FormalA])) \[CenterDot] 
           (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
             ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
              \[FormalA]))), "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalA]) \[CenterDot] \[FormalA])) \[CenterDot] 
            (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
              ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
               \[FormalA]))) == (((\[FormalA] \[CenterDot] (
                \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
                  \[FormalA]) \[CenterDot] \[FormalA]))) \[CenterDot] 
              \[FormalB]) \[CenterDot] ((\[FormalA] \[CenterDot] (
                (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
                \[FormalA])) \[CenterDot] (\[FormalA] \[CenterDot] (
                \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
                  \[FormalA]) \[CenterDot] \[FormalA]))))) \[CenterDot] 
            (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalA]) \[CenterDot] \[FormalA]))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 38} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
            \[FormalA])) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalA]) \[CenterDot] \[FormalA]))) == 
         (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalA])) \[CenterDot] 
          (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 12}, 
        "Orientation" -> -1, "Rule" -> 
         ((((\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] (
                ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
                (\[FormalA]_)))) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
                (\[FormalA]_)) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
             ((\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] (
                ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
                (\[FormalA]_)))))) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
             (\[FormalA]_))) -> (\[FormalA] \[CenterDot] 
            ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
             \[FormalA])) \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalA]) \[CenterDot] \[FormalA]))), "Side" -> 1, 
        "Subpattern" -> (((\[FormalA]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] (
                \[FormalA]_)) \[CenterDot] (\[FormalA]_)))) \[CenterDot] 
           (\[FormalB]_)) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
             (\[FormalA]_))) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] (
                \[FormalA]_)) \[CenterDot] (\[FormalA]_))))), 
        "MatchingConstruct" -> {"CriticalPairLemma", 25}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (((\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] (
                \[FormalA]_)))) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
              (\[FormalA]_)))) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
              (\[FormalA]_))) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
                (\[FormalA]_)) \[CenterDot] (\[FormalA]_))))) -> 
          \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalA]), "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 13} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
            \[FormalA])) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalA]) \[CenterDot] \[FormalA]))) == \[FormalA]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 38}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 18}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalA]_))) -> \[FormalA], 
        "OutputExpression" -> HoldForm[(\[FormalA] \[CenterDot] 
             ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
              \[FormalA])) \[CenterDot] (\[FormalA] \[CenterDot] 
             (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
                \[FormalA]) \[CenterDot] \[FormalA]))) == \[FormalA]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 14} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalA]) \[CenterDot] \[FormalA])) \[CenterDot] 
           (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
             ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
              \[FormalA])))) == \[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 36}, "Position" -> {1}, 
        "Construct" -> {"SubstitutionLemma", 13}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
              (\[FormalA]_)))) -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
               \[FormalA])) \[CenterDot] (\[FormalA] \[CenterDot] 
              (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
                 \[FormalA]) \[CenterDot] \[FormalA])))) == 
           \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalA]) \[CenterDot] \[FormalA])], "ConstructSide" -> 1, 
        "InputOrientation" -> -1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 15} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalA]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 14}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 13}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
              (\[FormalA]_)))) -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
           \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalA]) \[CenterDot] \[FormalA])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 16} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
              \[FormalA]) \[CenterDot] \[FormalB]))) \[CenterDot] 
          \[FormalA] == \[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
            \[FormalA]) \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 32}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 13}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
              (\[FormalA]_)))) -> \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
              ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
               \[FormalB]))) \[CenterDot] \[FormalA] == 
           \[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
              \[FormalA]) \[CenterDot] \[FormalB])], "ConstructSide" -> 1, 
        "InputOrientation" -> -1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 17} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalA]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 6}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 13}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
              (\[FormalA]_)))) -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
           \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalA]) \[CenterDot] \[FormalA])], "ConstructSide" -> 1, 
        "InputOrientation" -> -1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 39} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
            \[FormalA])) \[CenterDot] \[FormalA] == \[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
             ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
              \[FormalA])) \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 14}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
              (((\[FormalB]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
                  (\[FormalA]_)) \[CenterDot] (\[FormalB]_))) \[CenterDot] (
                \[FormalA]_))) \[CenterDot] (\[FormalA]_))) -> 
          (\[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
              \[FormalA]) \[CenterDot] \[FormalB])) \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          (((\[FormalB]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           (\[FormalB]_)), "MatchingConstruct" -> {"SubstitutionLemma", 15}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] (\[FormalA]_)) -> 
          \[FormalA] \[CenterDot] \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2, 2, 1, 2, 1}|>|>, {"SubstitutionLemma", 18} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] \[FormalA] == \[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
             ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
              \[FormalA])) \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 39}, "Position" -> {1}, 
        "Construct" -> {"SubstitutionLemma", 15}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] (\[FormalA]_)) -> 
          \[FormalA] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
            \[FormalA] == \[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
             ((\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
                 \[FormalA]) \[CenterDot] \[FormalA])) \[CenterDot] 
              \[FormalA]))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 19} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] \[FormalA] == \[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 18}, 
        "Position" -> {2, 2, 1}, "Construct" -> {"SubstitutionLemma", 15}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA] \[CenterDot] \[FormalA], 
        "OutputExpression" -> HoldForm[(\[FormalA] \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalA] == \[FormalA] \[CenterDot] 
            (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalA]) \[CenterDot] \[FormalA]))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 20} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] \[FormalA] == \[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 19}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 15}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] (\[FormalA]_)) -> 
          \[FormalA] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
            \[FormalA] == \[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
             \[FormalA])], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 21} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalA])) == 
         \[FormalA]], "Proof" -> <|"Input" -> {"CriticalPairLemma", 18}, 
        "Position" -> {1}, "Construct" -> {"SubstitutionLemma", 15}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA] \[CenterDot] \[FormalA], 
        "OutputExpression" -> HoldForm[(\[FormalA] \[CenterDot] 
             \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
             ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
              \[FormalA])) == \[FormalA]], "ConstructSide" -> 1, 
        "InputOrientation" -> -1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 22} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalA]) == 
         \[FormalA]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 21}, 
        "Position" -> {2}, "Construct" -> {"SubstitutionLemma", 15}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA] \[CenterDot] \[FormalA], 
        "OutputExpression" -> HoldForm[(\[FormalA] \[CenterDot] 
             \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalA]) == 
           \[FormalA]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"CriticalPairLemma", 40} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalA])))], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalC], "Side" -> 1, 
        "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           (\[FormalC]_)) \[CenterDot] (\[FormalA]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 20}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] (\[FormalA]_) -> 
          \[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalA]), 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"SubstitutionLemma", 23} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         \[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 17}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 20}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           (\[FormalA]_) -> \[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
            \[FormalA]), "OutputExpression" -> HoldForm[
          \[FormalA] \[CenterDot] \[FormalA] == \[FormalA] \[CenterDot] 
            (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalA]))], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 24} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 40}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 23}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalA]_))) -> 
          \[FormalA] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == ((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalA])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 41} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         (((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
            \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
            \[FormalA])) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 24}, 
        "Orientation" -> -1, "Rule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA], "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CenterDot] (\[FormalA]_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 22}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 42} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CenterDot] 
           ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
            \[FormalB])) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 24}, 
        "Orientation" -> -1, "Rule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA], "Side" -> 1, 
        "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           (\[FormalB]_)) \[CenterDot] (\[FormalA]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 16}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] (
                \[FormalA]_)) \[CenterDot] (\[FormalB]_)))) \[CenterDot] 
           (\[FormalA]_) -> \[FormalB] \[CenterDot] 
           ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalB]), 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 43} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         (\[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
             (\[FormalA] \[CenterDot] \[FormalA])) \[CenterDot] 
            \[FormalB])) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 41}, 
        "Orientation" -> -1, "Rule" -> 
         ((((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (\[FormalA]_))) \[CenterDot] (\[FormalA]_) -> 
          \[FormalA] \[CenterDot] \[FormalA], "Side" -> 1, 
        "Subpattern" -> (((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] (\[FormalA]_)), "MatchingConstruct" -> 
         {"SubstitutionLemma", 16}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] (
                \[FormalA]_)) \[CenterDot] (\[FormalB]_)))) \[CenterDot] 
           (\[FormalA]_) -> \[FormalB] \[CenterDot] 
           ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalB]), 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"SubstitutionLemma", 25} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
            ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
             \[FormalB]))) == \[FormalB] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 13}, 
        "Position" -> {2, 2}, "Construct" -> {"SubstitutionLemma", 16}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] (
                \[FormalA]_)) \[CenterDot] (\[FormalB]_)))) \[CenterDot] 
           (\[FormalA]_) -> \[FormalB] \[CenterDot] 
           ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalB]), 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] 
            (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
              ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
               \[FormalB]))) == \[FormalB] \[CenterDot] 
            ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalB])], 
        "ConstructSide" -> 1, "InputOrientation" -> -1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 44} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
           ((\[FormalB] \[CenterDot] (\[FormalA] \[CenterDot] 
              \[FormalA])) \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalC], "Side" -> 1, 
        "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           (\[FormalB]_)) \[CenterDot] (\[FormalC]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 42}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
             (\[FormalA]_))) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)) -> \[FormalB], "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 45} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalB] \[CenterDot] 
           ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalC], "Side" -> 1, 
        "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           (\[FormalB]_)) \[CenterDot] (\[FormalC]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 43}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] (
                \[FormalB]_))) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
           (\[FormalB]_) -> \[FormalB] \[CenterDot] \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 46} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
              \[FormalB]))) \[CenterDot] \[FormalA]) == 
         ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB])) \[CenterDot] 
          ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           (((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
             (\[FormalA] \[CenterDot] \[FormalB])) \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB])))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 1}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
            ((((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
              ((\[FormalB]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
                 (\[FormalA]_)) \[CenterDot] (\[FormalB]_)))) \[CenterDot] 
             ((\[FormalB]_) \[CenterDot] (\[FormalC]_)))) -> 
          \[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalB]), "Side" -> 1, 
        "Subpattern" -> ((\[FormalB]_) \[CenterDot] 
           (\[FormalC]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
            (\[FormalB]_))), "MatchingConstruct" -> {"CriticalPairLemma", 
          44}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] (
                \[FormalA]_))) \[CenterDot] (\[FormalB]_))) -> 
          \[FormalA] \[CenterDot] \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2, 2, 1}|>|>, {"SubstitutionLemma", 26} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
              \[FormalB]))) \[CenterDot] \[FormalA]) == 
         \[FormalA] \[CenterDot] \[FormalB]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 46}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 45}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalB]_))) -> \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] 
            ((\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
                \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
                \[FormalB]))) \[CenterDot] \[FormalA]) == 
           \[FormalA] \[CenterDot] \[FormalB]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 47} -> 
     <|"Statement" -> HoldForm[
        ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalA]) == 
         ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           \[FormalA]) \[CenterDot] ((((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalA] \[CenterDot] (((\[FormalA] \[CenterDot] 
                \[FormalB]) \[CenterDot] \[FormalA]) \[CenterDot] 
              (\[FormalA] \[CenterDot] \[FormalA])))) \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 26}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] (
                \[FormalB]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] (
                \[FormalB]_)))) \[CenterDot] (\[FormalA]_)) -> 
          \[FormalA] \[CenterDot] \[FormalB], "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 24}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2, 1, 2, 1}|>|>, {"SubstitutionLemma", 27} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           \[FormalA]) \[CenterDot] ((((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalA] \[CenterDot] (((\[FormalA] \[CenterDot] 
                \[FormalB]) \[CenterDot] \[FormalA]) \[CenterDot] 
              (\[FormalA] \[CenterDot] \[FormalA])))) \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 47}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 24}, "Orientation" -> -1, 
        "Rule" -> (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == ((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] \[FormalA]) \[CenterDot] 
            ((((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
               \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] (
                ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
                 \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
                 \[FormalA])))) \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalA]))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 28} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           \[FormalA]) \[CenterDot] ((((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalA])) \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 27}, 
        "Position" -> {2, 1, 2, 2}, "Construct" -> {"SubstitutionLemma", 24}, 
        "Orientation" -> -1, "Rule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == ((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] \[FormalA]) \[CenterDot] 
            ((((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
               \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
               \[FormalA])) \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalA]))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 29} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 28}, 
        "Position" -> {2, 1}, "Construct" -> {"SubstitutionLemma", 24}, 
        "Orientation" -> -1, "Rule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == ((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalA]))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 48} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] \[FormalA] == 
         (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA])) \[CenterDot] 
          (((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalA]) \[CenterDot] ((((\[FormalA] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalA]) \[CenterDot] 
             (\[FormalA] \[CenterDot] \[FormalA])) \[CenterDot] 
            ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
             \[FormalA])))], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 29}, "Orientation" -> -1, 
        "Rule" -> (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalA], "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 24}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {1, 1}|>|>, {"SubstitutionLemma", 30} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] \[FormalA] == 
         (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA])) \[CenterDot] 
          (((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
            ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
             \[FormalA])))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 48}, "Position" -> {2, 2, 1}, 
        "Construct" -> {"SubstitutionLemma", 24}, "Orientation" -> -1, 
        "Rule" -> (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalA] == (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalA])) \[CenterDot] 
            (((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
              \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
              ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
               \[FormalA])))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 31} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] \[FormalA] == 
         (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA])) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 30}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 29}, "Orientation" -> -1, 
        "Rule" -> (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalA] == (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalA])) \[CenterDot] 
            \[FormalA]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 49} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] \[FormalA])) \[CenterDot] 
           \[FormalA]) == ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           \[FormalA]) \[CenterDot] (((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA]) \[CenterDot] 
           (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] \[FormalA])))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 25}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (((\[FormalB]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
              (\[FormalB]_)))) -> \[FormalB] \[CenterDot] 
           ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalB]), 
        "Side" -> 1, "Subpattern" -> ((\[FormalB]_) \[CenterDot] 
           (\[FormalA]_)) \[CenterDot] (\[FormalB]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 31}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
             (\[FormalA]_))) \[CenterDot] (\[FormalA]_) -> 
          (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2, 2, 2}|>|>, 
    {"SubstitutionLemma", 32} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA]) == 
         ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           \[FormalA]) \[CenterDot] (((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA]) \[CenterDot] 
           (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] \[FormalA])))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 49}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 31}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
              (\[FormalB]_)) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
           (\[FormalA]_) -> (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           \[FormalA], "OutputExpression" -> HoldForm[
          \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] \[FormalA]) == 
           ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
             \[FormalA]) \[CenterDot] (((\[FormalA] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalA]) \[CenterDot] 
             (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
                \[FormalB]) \[CenterDot] \[FormalA])))], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 33} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA]) == 
         ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           \[FormalA]) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 32}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 29}, "Orientation" -> -1, 
        "Rule" -> (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] \[FormalA]) == 
           ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalA]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 50} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] (
                \[FormalA] \[CenterDot] \[FormalB]))) \[CenterDot] 
             \[FormalA])) \[CenterDot] \[FormalA]) == 
         ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           \[FormalA]) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 33}, 
        "Orientation" -> -1, "Rule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] (\[FormalA]_) -> 
          \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA]), "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 26}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
              ((\[FormalA]_) \[CenterDot] (\[FormalB]_)))) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA] \[CenterDot] \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {1, 1}|>|>, 
    {"SubstitutionLemma", 34} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
              \[FormalB]))) \[CenterDot] \[FormalA]) == 
         ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           \[FormalA]) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 50}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 31}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
              (\[FormalB]_)) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
           (\[FormalA]_) -> (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           \[FormalA], "OutputExpression" -> HoldForm[
          \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] (
                \[FormalA] \[CenterDot] \[FormalB]))) \[CenterDot] 
             \[FormalA]) == ((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] \[FormalA]) \[CenterDot] \[FormalA]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 35} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           \[FormalA]) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 34}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 26}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
              ((\[FormalA]_) \[CenterDot] (\[FormalB]_)))) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA] \[CenterDot] \[FormalB], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
           ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalA]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 36} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalB]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 35}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 33}, "Orientation" -> -1, 
        "Rule" -> (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] (\[FormalA]_) -> 
          \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA]), "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
           \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] \[FormalA])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 37} -> 
     <|"Statement" -> HoldForm[(((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
          (\[FormalA]_) -> \[FormalA] \[CenterDot] \[FormalB]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 33}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 36}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalA]_)) -> 
          \[FormalA] \[CenterDot] \[FormalB], "OutputExpression" -> 
         HoldForm[(((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] (\[FormalA]_) -> 
           \[FormalA] \[CenterDot] \[FormalB]], "ConstructSide" -> 1, 
        "InputOrientation" -> -1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 38} -> 
     <|"Statement" -> HoldForm[
        ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           \[FormalC]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalC]) == 
         \[FormalC]], "Proof" -> <|"Input" -> {"Axiom", 1}, 
        "Position" -> {2}, "Construct" -> {"SubstitutionLemma", 36}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA] \[CenterDot] \[FormalB], 
        "OutputExpression" -> HoldForm[
          ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
             \[FormalC]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalC]) == 
           \[FormalC]], "ConstructSide" -> 1, "InputOrientation" -> -1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 39} -> 
     <|"Statement" -> HoldForm[
        ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB]) == 
         \[FormalA]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 29}, 
        "Position" -> {2}, "Construct" -> {"SubstitutionLemma", 36}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA] \[CenterDot] \[FormalB], 
        "OutputExpression" -> HoldForm[
          ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
             \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB]) == 
           \[FormalA]], "ConstructSide" -> 1, "InputOrientation" -> -1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 40} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalB] \[CenterDot] \[FormalA]) == 
         \[FormalA]], "Proof" -> <|"Input" -> {"CriticalPairLemma", 45}, 
        "Position" -> {2}, "Construct" -> {"SubstitutionLemma", 36}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA] \[CenterDot] \[FormalB], 
        "OutputExpression" -> HoldForm[(\[FormalA] \[CenterDot] 
             \[FormalA]) \[CenterDot] (\[FormalB] \[CenterDot] \[FormalA]) == 
           \[FormalA]], "ConstructSide" -> 1, "InputOrientation" -> -1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 41} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalB] \[CenterDot] \[FormalB]) == 
         \[FormalB]], "Proof" -> <|"Input" -> {"CriticalPairLemma", 42}, 
        "Position" -> {1}, "Construct" -> {"SubstitutionLemma", 36}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA] \[CenterDot] \[FormalB], 
        "OutputExpression" -> HoldForm[(\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] (\[FormalB] \[CenterDot] \[FormalB]) == 
           \[FormalB]], "ConstructSide" -> 1, "InputOrientation" -> -1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 42} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalA])) == 
         \[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
            \[FormalA]) \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 25}, 
        "Position" -> {2, 2}, "Construct" -> {"SubstitutionLemma", 36}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA] \[CenterDot] \[FormalB], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] 
            (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalA])) == 
           \[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
              \[FormalA]) \[CenterDot] \[FormalB])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 43} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalA])) == 
         \[FormalB] \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 42}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 36}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalA]_)) -> 
          \[FormalA] \[CenterDot] \[FormalB], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalA])) == \[FormalB] \[CenterDot] 
            \[FormalA]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 44} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] ((\[FormalC] \[CenterDot] 
            \[FormalA]) \[CenterDot] \[FormalB]) == \[FormalB]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 4}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 36}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalA]_)) -> 
          \[FormalA] \[CenterDot] \[FormalB], "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            ((\[FormalC] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalB]) == 
           \[FormalB]], "ConstructSide" -> 1, "InputOrientation" -> -1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 45} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
              \[FormalC]) \[CenterDot] \[FormalB])) \[CenterDot] 
           (\[FormalC] \[CenterDot] ((\[FormalC] \[CenterDot] 
              \[FormalA]) \[CenterDot] \[FormalC]))) == 
         \[FormalC] \[CenterDot] ((\[FormalC] \[CenterDot] 
            \[FormalA]) \[CenterDot] \[FormalC])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 7}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 36}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalA]_)) -> 
          \[FormalA] \[CenterDot] \[FormalB], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
              ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
               \[FormalB])) \[CenterDot] (\[FormalC] \[CenterDot] 
              ((\[FormalC] \[CenterDot] \[FormalA]) \[CenterDot] 
               \[FormalC]))) == \[FormalC] \[CenterDot] 
            ((\[FormalC] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalC])], 
        "ConstructSide" -> 1, "InputOrientation" -> -1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 46} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           (\[FormalC] \[CenterDot] ((\[FormalC] \[CenterDot] 
              \[FormalA]) \[CenterDot] \[FormalC]))) == 
         \[FormalC] \[CenterDot] ((\[FormalC] \[CenterDot] 
            \[FormalA]) \[CenterDot] \[FormalC])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 45}, 
        "Position" -> {2, 1}, "Construct" -> {"SubstitutionLemma", 36}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA] \[CenterDot] \[FormalB], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] 
            ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
             (\[FormalC] \[CenterDot] ((\[FormalC] \[CenterDot] 
                \[FormalA]) \[CenterDot] \[FormalC]))) == 
           \[FormalC] \[CenterDot] ((\[FormalC] \[CenterDot] 
              \[FormalA]) \[CenterDot] \[FormalC])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 47} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           (\[FormalC] \[CenterDot] \[FormalA])) == \[FormalC] \[CenterDot] 
          ((\[FormalC] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalC])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 46}, 
        "Position" -> {2, 2}, "Construct" -> {"SubstitutionLemma", 36}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA] \[CenterDot] \[FormalB], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] 
            ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
             (\[FormalC] \[CenterDot] \[FormalA])) == \[FormalC] \[CenterDot] 
            ((\[FormalC] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalC])], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 48} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           (\[FormalC] \[CenterDot] \[FormalA])) == \[FormalC] \[CenterDot] 
          \[FormalA]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 47}, 
        "Position" -> {}, "Construct" -> {"SubstitutionLemma", 36}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA] \[CenterDot] \[FormalB], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] 
            ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
             (\[FormalC] \[CenterDot] \[FormalA])) == \[FormalC] \[CenterDot] 
            \[FormalA]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 49} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           (\[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
              \[FormalA]) \[CenterDot] \[FormalB]))) == 
         \[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
            \[FormalA]) \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 1}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 36}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalA]_)) -> 
          \[FormalA] \[CenterDot] \[FormalB], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
              \[FormalC]) \[CenterDot] (\[FormalB] \[CenterDot] 
              ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
               \[FormalB]))) == \[FormalB] \[CenterDot] 
            ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalB])], 
        "ConstructSide" -> 1, "InputOrientation" -> -1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 50} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalA])) == \[FormalB] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 49}, 
        "Position" -> {2, 2}, "Construct" -> {"SubstitutionLemma", 36}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA] \[CenterDot] \[FormalB], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] 
            ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalA])) == \[FormalB] \[CenterDot] 
            ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalB])], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 51} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalA])) == \[FormalB] \[CenterDot] 
          \[FormalA]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 50}, 
        "Position" -> {}, "Construct" -> {"SubstitutionLemma", 36}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA] \[CenterDot] \[FormalB], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] 
            ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalA])) == \[FormalB] \[CenterDot] 
            \[FormalA]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 52} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] ((\[FormalC] \[CenterDot] 
             \[FormalB]) \[CenterDot] (((\[FormalC] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalA]) \[CenterDot] 
             (\[FormalC] \[CenterDot] \[FormalB])))) == 
         (\[FormalC] \[CenterDot] \[FormalB]) \[CenterDot] 
          (((\[FormalC] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalA]) \[CenterDot] (\[FormalC] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 10}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 36}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalA]_)) -> 
          \[FormalA] \[CenterDot] \[FormalB], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             ((\[FormalC] \[CenterDot] \[FormalB]) \[CenterDot] 
              (((\[FormalC] \[CenterDot] \[FormalB]) \[CenterDot] 
                \[FormalA]) \[CenterDot] (\[FormalC] \[CenterDot] 
                \[FormalB])))) == (\[FormalC] \[CenterDot] 
             \[FormalB]) \[CenterDot] (((\[FormalC] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalA]) \[CenterDot] 
             (\[FormalC] \[CenterDot] \[FormalB]))], "ConstructSide" -> 1, 
        "InputOrientation" -> -1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 53} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] ((\[FormalC] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA])) == 
         (\[FormalC] \[CenterDot] \[FormalB]) \[CenterDot] 
          (((\[FormalC] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalA]) \[CenterDot] (\[FormalC] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 52}, 
        "Position" -> {2, 2}, "Construct" -> {"SubstitutionLemma", 36}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA] \[CenterDot] \[FormalB], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] 
            (\[FormalB] \[CenterDot] ((\[FormalC] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalA])) == 
           (\[FormalC] \[CenterDot] \[FormalB]) \[CenterDot] 
            (((\[FormalC] \[CenterDot] \[FormalB]) \[CenterDot] 
              \[FormalA]) \[CenterDot] (\[FormalC] \[CenterDot] 
              \[FormalB]))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 54} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] ((\[FormalC] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA])) == 
         (\[FormalC] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 53}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 36}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalA]_)) -> 
          \[FormalA] \[CenterDot] \[FormalB], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             ((\[FormalC] \[CenterDot] \[FormalB]) \[CenterDot] 
              \[FormalA])) == (\[FormalC] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 51} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 40}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA], "Side" -> 1, 
        "Subpattern" -> (\[FormalB]_) \[CenterDot] (\[FormalA]_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 37}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalA]_)) -> 
          \[FormalA] \[CenterDot] \[FormalB], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 52} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 41}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)) -> \[FormalB], "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 37}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalA]_)) -> 
          \[FormalA] \[CenterDot] \[FormalB], "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 53} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] \[FormalA] == \[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 43}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalB] \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          ((\[FormalB]_) \[CenterDot] (\[FormalA]_)), "MatchingConstruct" -> 
         {"SubstitutionLemma", 36}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA] \[CenterDot] \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 54} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CenterDot] 
           \[FormalA]) \[CenterDot] ((\[FormalB] \[CenterDot] 
            \[FormalC]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 38}, 
        "Orientation" -> 1, "Rule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalC]_)) -> \[FormalC], "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 52}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {1, 1}|>|>, 
    {"SubstitutionLemma", 55} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalB]) \[CenterDot] \[FormalA]) == \[FormalA]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 39}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 53}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalA]_) -> \[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
            \[FormalB]), "OutputExpression" -> HoldForm[
          (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA]) == 
           \[FormalA]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 56} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB])) == \[FormalA]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 55}, "Position" -> {2}, 
        "Construct" -> {"CriticalPairLemma", 53}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalA]_) -> \[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
            \[FormalB]), "OutputExpression" -> HoldForm[
          (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB])) == 
           \[FormalA]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"CriticalPairLemma", 55} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalB] \[CenterDot] ((\[FormalC] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalB])) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 44}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"SubstitutionLemma", 40}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 56} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalA] \[CenterDot] ((\[FormalC] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalA])) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 44}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"CriticalPairLemma", 51}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 57} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] \[FormalC] == 
         (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalC])) \[CenterDot] \[FormalC]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 54}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           (\[FormalC]_)) \[CenterDot] (\[FormalB]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 54}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 58} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalB]) \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 48}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalA]_))) -> 
          \[FormalC] \[CenterDot] \[FormalA], "Side" -> 1, 
        "Subpattern" -> ((\[FormalB]_) \[CenterDot] 
           (\[FormalC]_)) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
           (\[FormalA]_)), "MatchingConstruct" -> {"CriticalPairLemma", 53}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalA]_) -> \[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
            \[FormalB]), "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 57} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB])))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 58}, 
        "Position" -> {2, 2}, "Construct" -> {"CriticalPairLemma", 53}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (\[FormalA]_) -> 
          \[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB]), 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
           \[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
              (\[FormalA] \[CenterDot] \[FormalB])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 58} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalB] \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 57}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 56}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
           \[FormalB] \[CenterDot] \[FormalA]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 59} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] ((\[FormalC] \[CenterDot] 
            \[FormalA]) \[CenterDot] \[FormalB]) == 
         ((\[FormalC] \[CenterDot] \[FormalA]) \[CenterDot] 
           \[FormalB]) \[CenterDot] ((\[FormalAlpha] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB])) \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 48}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalA]_))) -> 
          \[FormalC] \[CenterDot] \[FormalA], "Side" -> 1, 
        "Subpattern" -> (\[FormalC]_) \[CenterDot] (\[FormalA]_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 44}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (((\[FormalC]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
            (\[FormalB]_)) -> \[FormalB], "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"SubstitutionLemma", 59} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           \[FormalA]) \[CenterDot] ((\[FormalAlpha] \[CenterDot] 
            (\[FormalC] \[CenterDot] \[FormalA])) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 59}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 44}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (((\[FormalC]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
            (\[FormalB]_)) -> \[FormalB], "OutputExpression" -> 
         HoldForm[\[FormalA] == ((\[FormalB] \[CenterDot] 
              \[FormalC]) \[CenterDot] \[FormalA]) \[CenterDot] 
            ((\[FormalAlpha] \[CenterDot] (\[FormalC] \[CenterDot] 
               \[FormalA])) \[CenterDot] \[FormalA])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 60} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalB] \[CenterDot] ((\[FormalC] \[CenterDot] 
            \[FormalA]) \[CenterDot] (\[FormalB] \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 48}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalA]_))) -> 
          \[FormalC] \[CenterDot] \[FormalA], "Side" -> 1, 
        "Subpattern" -> (\[FormalC]_) \[CenterDot] (\[FormalA]_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 58}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 61} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] ((\[FormalB] \[CenterDot] 
            \[FormalC]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 54}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"SubstitutionLemma", 58}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 62} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 54}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           (\[FormalC]_)) \[CenterDot] (\[FormalB]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 58}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 63} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] ((\[FormalC] \[CenterDot] 
            \[FormalB]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 44}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"SubstitutionLemma", 58}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 64} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalC] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 44}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> ((\[FormalC]_) \[CenterDot] 
           (\[FormalA]_)) \[CenterDot] (\[FormalB]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 58}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 65} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 61}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> ((\[FormalB]_) \[CenterDot] 
           (\[FormalC]_)) \[CenterDot] (\[FormalA]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 58}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 66} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalB]) \[CenterDot] ((\[FormalB] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalC]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 62}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalC]_))) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"SubstitutionLemma", 40}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 67} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB])) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 62}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalC]_))) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] (\[FormalC]_)), "MatchingConstruct" -> 
         {"CriticalPairLemma", 61}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 68} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalC]) == 
         ((\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalC])) \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 61}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> ((\[FormalB]_) \[CenterDot] 
           (\[FormalC]_)) \[CenterDot] (\[FormalA]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 62}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalC]_))) -> \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 69} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalB] \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalC])) == 
         (\[FormalB] \[CenterDot] (\[FormalA] \[CenterDot] 
            \[FormalC])) \[CenterDot] ((\[FormalAlpha] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB])) \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 48}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalA]_))) -> 
          \[FormalC] \[CenterDot] \[FormalA], "Side" -> 1, 
        "Subpattern" -> (\[FormalC]_) \[CenterDot] (\[FormalA]_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 62}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (\[FormalC]_))) -> \[FormalB], "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"SubstitutionLemma", 60} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
          ((\[FormalAlpha] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalA])) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 69}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 62}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (\[FormalC]_))) -> \[FormalB], "OutputExpression" -> 
         HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
            ((\[FormalAlpha] \[CenterDot] (\[FormalB] \[CenterDot] 
               \[FormalA])) \[CenterDot] \[FormalA])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 70} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalC] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 63}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> ((\[FormalC]_) \[CenterDot] 
           (\[FormalB]_)) \[CenterDot] (\[FormalA]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 58}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 71} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalB]) \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalB])))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 64}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalA]_))) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"SubstitutionLemma", 40}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 72} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 65}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          ((\[FormalB]_) \[CenterDot] (\[FormalC]_)), "MatchingConstruct" -> 
         {"CriticalPairLemma", 61}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 73} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) == 
         (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
            \[FormalC])) \[CenterDot] ((\[FormalAlpha] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB])) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 48}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalA]_))) -> 
          \[FormalC] \[CenterDot] \[FormalA], "Side" -> 1, 
        "Subpattern" -> (\[FormalC]_) \[CenterDot] (\[FormalA]_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 65}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"SubstitutionLemma", 61} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
          ((\[FormalAlpha] \[CenterDot] (\[FormalA] \[CenterDot] 
             \[FormalB])) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 73}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 65}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
            ((\[FormalAlpha] \[CenterDot] (\[FormalA] \[CenterDot] 
               \[FormalB])) \[CenterDot] \[FormalA])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 74} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalC] \[CenterDot] \[FormalB])) == 
         (\[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] 
            \[FormalB])) \[CenterDot] (((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalAlpha]) \[CenterDot] 
           \[FormalA])], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 
          51}, "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalA]_))) -> 
          \[FormalB] \[CenterDot] \[FormalA], "Side" -> 1, 
        "Subpattern" -> (\[FormalB]_) \[CenterDot] (\[FormalA]_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 70}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"SubstitutionLemma", 62} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
          (((\[FormalA] \[CenterDot] \[FormalC]) \[CenterDot] 
            \[FormalAlpha]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 74}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 70}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
            (((\[FormalA] \[CenterDot] \[FormalC]) \[CenterDot] 
              \[FormalAlpha]) \[CenterDot] \[FormalA])], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 63} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
            \[FormalC]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 67}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 58}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
           \[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
              \[FormalC]) \[CenterDot] (\[FormalA] \[CenterDot] 
              \[FormalB]))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 64} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalB]) \[CenterDot] (\[FormalB] \[CenterDot] \[FormalC]))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 72}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 58}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
           \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] (\[FormalB] \[CenterDot] 
              \[FormalC]))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 75} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalA])) \[CenterDot] \[FormalB] == 
         \[FormalB] \[CenterDot] \[FormalB]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 54}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
              (\[FormalB]_)) \[CenterDot] (\[FormalA]_))) -> 
          (\[FormalC] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          (((\[FormalC]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalA]_)), "MatchingConstruct" -> {"CriticalPairLemma", 61}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 65} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB])) == 
         \[FormalA] \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 75}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 58}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             (\[FormalA] \[CenterDot] \[FormalB])) == \[FormalA] \[CenterDot] 
            \[FormalA]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"CriticalPairLemma", 76} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB]) == 
         (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
          ((\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
             \[FormalA])) \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 65}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalA] \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"CriticalPairLemma", 64}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalB], "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 77} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
             \[FormalB]))) \[CenterDot] ((\[FormalC] \[CenterDot] 
            \[FormalC]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 63}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalC]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"SubstitutionLemma", 65}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalB]_))) -> 
          \[FormalA] \[CenterDot] \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2, 1}|>|>, {"SubstitutionLemma", 66} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] \[FormalC] == \[FormalC] \[CenterDot] 
          (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalC]))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 57}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 58}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalC] == \[FormalC] \[CenterDot] (\[FormalA] \[CenterDot] 
             ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
              \[FormalC]))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 67} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalC]) == \[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalC])) \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 68}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 58}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalC]) == \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
             \[FormalB])], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 68} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalC]) == \[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalC])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 67}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 58}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalC]) == \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
               \[FormalC])))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 78} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalA]) == \[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 68}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             ((\[FormalB]_) \[CenterDot] (\[FormalC]_)))) -> 
          \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalC]), 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalC]_))), "MatchingConstruct" -> {"SubstitutionLemma", 
          65}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalB]_))) -> 
          \[FormalA] \[CenterDot] \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 79} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalB]) == \[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 78}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) <-> 
          (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)), "Side" -> 1, "Subpattern" -> 
         (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 58}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 80} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 43}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalB] \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          ((\[FormalB]_) \[CenterDot] (\[FormalA]_)), "MatchingConstruct" -> 
         {"CriticalPairLemma", 78}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) <-> 
          (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)), "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 81} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalB])))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 70}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalB]_))) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalC]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"CriticalPairLemma", 78}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalA]_)) <-> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalB]_)), "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 82} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalA]) == 
         (\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 78}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) <-> 
          (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)), "Side" -> 2, "Subpattern" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
           (\[FormalB]_)), "MatchingConstruct" -> {"SubstitutionLemma", 58}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 83} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA]) == 
         \[FormalA] \[CenterDot] \[FormalB]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 78}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) <-> 
          (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)), "Side" -> 2, "Subpattern" -> 
         (\[FormalB]_) \[CenterDot] (\[FormalB]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 41}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)) -> \[FormalB], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 84} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] \[FormalB] == \[FormalB] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 80}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalB] \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"SubstitutionLemma", 41}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"CriticalPairLemma", 85} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
          (\[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalC]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 70}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalB]_))) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalC]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"CriticalPairLemma", 82}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalA]_)) <-> ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 86} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
           ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalC]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 64}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalA]_))) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalC]_) \[CenterDot] 
          (\[FormalA]_), "MatchingConstruct" -> {"CriticalPairLemma", 82}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalA]_)) <-> ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 87} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
          (((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalC]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 63}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalC]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"CriticalPairLemma", 82}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalA]_)) <-> ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2, 1}|>|>, {"CriticalPairLemma", 88} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalB] \[CenterDot] ((\[FormalC] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 55}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (\[FormalA]_))) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalC] \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] (\[FormalA]_)), "MatchingConstruct" -> 
         {"CriticalPairLemma", 78}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) <-> 
          (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)), "MatchingSide" -> 2, "Position" -> {2, 1}|>|>, 
    {"CriticalPairLemma", 89} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalA] \[CenterDot] ((\[FormalC] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalC])) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 56}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (\[FormalA]_))) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (\[FormalC]_))) -> \[FormalA] \[CenterDot] \[FormalC], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] (\[FormalA]_)), "MatchingConstruct" -> 
         {"CriticalPairLemma", 78}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) <-> 
          (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)), "MatchingSide" -> 2, "Position" -> {2, 1}|>|>, 
    {"SubstitutionLemma", 69} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalAlpha] \[CenterDot] (\[FormalC] \[CenterDot] 
             \[FormalA])))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 59}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 58}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == ((\[FormalB] \[CenterDot] 
              \[FormalC]) \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalA] \[CenterDot] (\[FormalAlpha] \[CenterDot] 
              (\[FormalC] \[CenterDot] \[FormalA])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 90} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
             \[FormalA]))) \[CenterDot] ((\[FormalAlpha] \[CenterDot] 
            \[FormalC]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 69}, 
        "Orientation" -> -1, "Rule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
            ((\[FormalAlpha]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
              (\[FormalC]_)))) -> \[FormalC], "Side" -> 1, 
        "Subpattern" -> (((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
          ((\[FormalC]_) \[CenterDot] ((\[FormalAlpha]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_)))), 
        "MatchingConstruct" -> {"SubstitutionLemma", 58}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"SubstitutionLemma", 70} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalAlpha] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalA])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 60}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 58}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
            (\[FormalA] \[CenterDot] (\[FormalAlpha] \[CenterDot] 
              (\[FormalB] \[CenterDot] \[FormalA])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 91} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
             \[FormalA]))) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalC] \[CenterDot] \[FormalAlpha]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 70}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalAlpha]_) \[CenterDot] 
             ((\[FormalB]_) \[CenterDot] (\[FormalA]_)))) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] ((\[FormalAlpha]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalA]_)))), 
        "MatchingConstruct" -> {"SubstitutionLemma", 58}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 92} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           (\[FormalC] \[CenterDot] \[FormalAlpha])) \[CenterDot] 
          ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalC])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 70}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalAlpha]_) \[CenterDot] 
             ((\[FormalB]_) \[CenterDot] (\[FormalA]_)))) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalAlpha]_) \[CenterDot] 
          ((\[FormalB]_) \[CenterDot] (\[FormalA]_)), "MatchingConstruct" -> 
         {"CriticalPairLemma", 62}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalC]_))) -> \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"SubstitutionLemma", 71} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalAlpha] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 61}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 58}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
            (\[FormalA] \[CenterDot] (\[FormalAlpha] \[CenterDot] 
              (\[FormalA] \[CenterDot] \[FormalB])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 72} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
          (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalC]) \[CenterDot] \[FormalAlpha]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 62}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 58}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
            (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalC]) \[CenterDot] \[FormalAlpha]))], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 93} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalC])) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalAlpha] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 72}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
              (\[FormalC]_)) \[CenterDot] (\[FormalAlpha]_))) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] (\[FormalAlpha]_))), 
        "MatchingConstruct" -> {"SubstitutionLemma", 58}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 94} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalB])) == 
         ((\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalB]))) \[CenterDot] 
           ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalA])) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 81}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             ((\[FormalB]_) \[CenterDot] (\[FormalB]_)))) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          ((\[FormalC]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_))), "MatchingConstruct" -> {"CriticalPairLemma", 
          81}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
              (\[FormalB]_)))) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 73} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalB])) == 
         \[FormalA] \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 94}, "Position" -> {1}, 
        "Construct" -> {"CriticalPairLemma", 77}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] (\[FormalB]_)))) \[CenterDot] 
           (((\[FormalC]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalB])) == \[FormalA] \[CenterDot] 
            \[FormalA]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 95} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
            \[FormalB])) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 73}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalB]_))) <-> (\[FormalA]_) \[CenterDot] (\[FormalA]_), 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          ((\[FormalB]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_))), "MatchingConstruct" -> {"SubstitutionLemma", 
          58}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 96} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] (\[FormalA] \[CenterDot] 
             \[FormalB]))) \[CenterDot] (\[FormalC] \[CenterDot] 
           (\[FormalC] \[CenterDot] \[FormalC])) == \[FormalA]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 73}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalB]_))) <-> (\[FormalA]_) \[CenterDot] (\[FormalA]_), 
        "Side" -> 2, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalA]_), "MatchingConstruct" -> {"SubstitutionLemma", 71}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalAlpha]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (\[FormalB]_)))) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"SubstitutionLemma", 74} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalB] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalB])) == \[FormalA]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 96}, "Position" -> {1}, 
        "Construct" -> {"SubstitutionLemma", 65}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalB]_))) -> 
          \[FormalA] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalB])) == 
           \[FormalA]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"CriticalPairLemma", 97} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalB]) == 
         (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalC] \[CenterDot] \[FormalC]))) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 84}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalA]_)), "Side" -> 1, "Subpattern" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalA]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 73}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalB]_))) <-> (\[FormalA]_) \[CenterDot] (\[FormalA]_), 
        "MatchingSide" -> 2, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 98} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalB])) \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 74}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalB]_))) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           (\[FormalA]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalB]_))), 
        "MatchingConstruct" -> {"SubstitutionLemma", 58}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 99} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalB])) \[CenterDot] 
          ((\[FormalC] \[CenterDot] (\[FormalC] \[CenterDot] 
             \[FormalC])) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 98}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"CriticalPairLemma", 95}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalA]_) <-> 
          ((\[FormalB]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalB]_))) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 100} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
           ((\[FormalC] \[CenterDot] (\[FormalC] \[CenterDot] 
              \[FormalC])) \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 80}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalB] \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"CriticalPairLemma", 95}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalA]_) <-> 
          ((\[FormalB]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalB]_))) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 101} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalA]) == 
         ((\[FormalC] \[CenterDot] (\[FormalC] \[CenterDot] 
             \[FormalC])) \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 82}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) <-> 
          ((\[FormalB]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalA]_), "Side" -> 2, "Subpattern" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalA]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 95}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] (\[FormalA]_) <-> 
          ((\[FormalB]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalB]_))) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 102} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] ((\[FormalC] \[CenterDot] 
              ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
               \[FormalC])) \[CenterDot] (\[FormalAlpha] \[CenterDot] 
              (\[FormalB] \[CenterDot] \[FormalB]))))) \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalAlpha] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalB])))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 85}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
              (\[FormalB]_)) \[CenterDot] (\[FormalC]_))) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> ((\[FormalB]_) \[CenterDot] 
           (\[FormalB]_)) \[CenterDot] (\[FormalC]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 88}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (\[FormalB]_))) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalC] \[CenterDot] \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"SubstitutionLemma", 75} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] ((\[FormalC] \[CenterDot] 
              \[FormalB]) \[CenterDot] (\[FormalAlpha] \[CenterDot] 
              (\[FormalB] \[CenterDot] \[FormalB]))))) \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalAlpha] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalB])))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 102}, 
        "Position" -> {1, 2, 2, 1}, "Construct" -> {"CriticalPairLemma", 83}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA] \[CenterDot] \[FormalB], 
        "OutputExpression" -> HoldForm[\[FormalA] == 
           (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
              ((\[FormalC] \[CenterDot] \[FormalB]) \[CenterDot] (
                \[FormalAlpha] \[CenterDot] (\[FormalB] \[CenterDot] 
                 \[FormalB]))))) \[CenterDot] (\[FormalA] \[CenterDot] 
             (\[FormalAlpha] \[CenterDot] (\[FormalB] \[CenterDot] 
               \[FormalB])))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 76} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalAlpha] \[CenterDot] 
            (\[FormalC] \[CenterDot] \[FormalC])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 75}, 
        "Position" -> {1, 2}, "Construct" -> {"CriticalPairLemma", 71}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (\[FormalA]_)))) -> \[FormalB] \[CenterDot] \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalA] == 
           (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
              \[FormalC])) \[CenterDot] (\[FormalA] \[CenterDot] 
             (\[FormalAlpha] \[CenterDot] (\[FormalC] \[CenterDot] 
               \[FormalC])))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 103} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalB] \[CenterDot] ((\[FormalC] \[CenterDot] 
              ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
               \[FormalC])) \[CenterDot] (\[FormalAlpha] \[CenterDot] 
              (\[FormalB] \[CenterDot] \[FormalB])))) \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalAlpha] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalB])))], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 86}, "Orientation" -> -1, 
        "Rule" -> (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalC], "Side" -> 1, 
        "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           (\[FormalA]_)) \[CenterDot] (\[FormalB]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 88}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (\[FormalB]_))) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalC] \[CenterDot] \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"SubstitutionLemma", 77} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalB] \[CenterDot] ((\[FormalC] \[CenterDot] 
              \[FormalB]) \[CenterDot] (\[FormalAlpha] \[CenterDot] 
              (\[FormalB] \[CenterDot] \[FormalB])))) \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalAlpha] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalB])))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 103}, "Position" -> {1, 1, 2, 1}, 
        "Construct" -> {"CriticalPairLemma", 83}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalA]_)) -> 
          \[FormalA] \[CenterDot] \[FormalB], "OutputExpression" -> 
         HoldForm[\[FormalA] == ((\[FormalB] \[CenterDot] 
              ((\[FormalC] \[CenterDot] \[FormalB]) \[CenterDot] (
                \[FormalAlpha] \[CenterDot] (\[FormalB] \[CenterDot] 
                 \[FormalB])))) \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalA] \[CenterDot] (\[FormalAlpha] \[CenterDot] 
              (\[FormalB] \[CenterDot] \[FormalB])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 78} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalAlpha] \[CenterDot] (\[FormalC] \[CenterDot] 
             \[FormalC])))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 77}, "Position" -> {1, 1}, 
        "Construct" -> {"CriticalPairLemma", 71}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             ((\[FormalA]_) \[CenterDot] (\[FormalA]_)))) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == ((\[FormalB] \[CenterDot] 
              \[FormalC]) \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalA] \[CenterDot] (\[FormalAlpha] \[CenterDot] 
              (\[FormalC] \[CenterDot] \[FormalC])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 104} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalB] \[CenterDot] ((\[FormalC] \[CenterDot] 
              ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
               \[FormalC])) \[CenterDot] ((\[FormalB] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalAlpha]))) \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
           ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalAlpha]))], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 86}, "Orientation" -> -1, 
        "Rule" -> (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalC], "Side" -> 1, 
        "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           (\[FormalA]_)) \[CenterDot] (\[FormalB]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 89}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (\[FormalB]_))) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (\[FormalC]_))) -> \[FormalA] \[CenterDot] \[FormalC], 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"SubstitutionLemma", 79} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalB] \[CenterDot] ((\[FormalC] \[CenterDot] 
              \[FormalB]) \[CenterDot] ((\[FormalB] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalAlpha]))) \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
           ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalAlpha]))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 104}, "Position" -> {1, 1, 2, 1}, 
        "Construct" -> {"CriticalPairLemma", 83}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalA]_)) -> 
          \[FormalA] \[CenterDot] \[FormalB], "OutputExpression" -> 
         HoldForm[\[FormalA] == ((\[FormalB] \[CenterDot] 
              ((\[FormalC] \[CenterDot] \[FormalB]) \[CenterDot] (
                (\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
                \[FormalAlpha]))) \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalAlpha]))], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 80} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
           ((\[FormalC] \[CenterDot] \[FormalC]) \[CenterDot] 
            \[FormalAlpha]))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 79}, "Position" -> {1, 1}, 
        "Construct" -> {"CriticalPairLemma", 66}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalC]_))) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == ((\[FormalB] \[CenterDot] 
              \[FormalC]) \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalA] \[CenterDot] ((\[FormalC] \[CenterDot] 
               \[FormalC]) \[CenterDot] \[FormalAlpha]))], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 105} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] ((\[FormalC] \[CenterDot] 
              ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
               \[FormalC])) \[CenterDot] (\[FormalAlpha] \[CenterDot] 
              (\[FormalB] \[CenterDot] \[FormalB]))))) \[CenterDot] 
          ((\[FormalAlpha] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalB])) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 87}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) \[CenterDot] 
           ((((\[FormalB]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> ((\[FormalB]_) \[CenterDot] 
           (\[FormalB]_)) \[CenterDot] (\[FormalC]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 88}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (\[FormalB]_))) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalC] \[CenterDot] \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"SubstitutionLemma", 81} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] ((\[FormalC] \[CenterDot] 
              \[FormalB]) \[CenterDot] (\[FormalAlpha] \[CenterDot] 
              (\[FormalB] \[CenterDot] \[FormalB]))))) \[CenterDot] 
          ((\[FormalAlpha] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalB])) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 105}, 
        "Position" -> {1, 2, 2, 1}, "Construct" -> {"CriticalPairLemma", 83}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA] \[CenterDot] \[FormalB], 
        "OutputExpression" -> HoldForm[\[FormalA] == 
           (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
              ((\[FormalC] \[CenterDot] \[FormalB]) \[CenterDot] (
                \[FormalAlpha] \[CenterDot] (\[FormalB] \[CenterDot] 
                 \[FormalB]))))) \[CenterDot] ((\[FormalAlpha] \[CenterDot] 
              (\[FormalB] \[CenterDot] \[FormalB])) \[CenterDot] 
             \[FormalA])], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 82} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
          ((\[FormalAlpha] \[CenterDot] (\[FormalC] \[CenterDot] 
             \[FormalC])) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 81}, 
        "Position" -> {1, 2}, "Construct" -> {"CriticalPairLemma", 71}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (\[FormalA]_)))) -> \[FormalB] \[CenterDot] \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalA] == 
           (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
              \[FormalC])) \[CenterDot] ((\[FormalAlpha] \[CenterDot] 
              (\[FormalC] \[CenterDot] \[FormalC])) \[CenterDot] 
             \[FormalA])], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 106} -> 
     <|"Statement" -> HoldForm[
        ((\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             (\[FormalC] \[CenterDot] \[FormalA]))) \[CenterDot] 
           \[FormalC]) \[CenterDot] \[FormalA] == \[FormalA] \[CenterDot] 
          \[FormalA]], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 66}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
              (\[FormalC]_)) \[CenterDot] (\[FormalA]_))) -> 
          (\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          (((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
           (\[FormalA]_)), "MatchingConstruct" -> {"CriticalPairLemma", 90}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] (\[FormalA]_)))) \[CenterDot] 
           (((\[FormalAlpha]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 83} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             (\[FormalC] \[CenterDot] \[FormalA]))) \[CenterDot] 
           \[FormalC]) == \[FormalA] \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 106}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 58}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
                \[FormalA]))) \[CenterDot] \[FormalC]) == 
           \[FormalA] \[CenterDot] \[FormalA]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 84} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] 
              \[FormalA])))) == \[FormalA] \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 83}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 58}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             (\[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] (
                \[FormalB] \[CenterDot] \[FormalA])))) == 
           \[FormalA] \[CenterDot] \[FormalA]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 107} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB]))) == \[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 68}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             ((\[FormalB]_) \[CenterDot] (\[FormalC]_)))) -> 
          \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalC]), 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalC]_))), "MatchingConstruct" -> {"SubstitutionLemma", 
          84}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
              ((\[FormalB]_) \[CenterDot] (\[FormalA]_))))) -> 
          \[FormalA] \[CenterDot] \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 108} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalB])) == \[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
           (\[FormalC] \[CenterDot] (\[FormalA] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalA]))))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 107}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             ((\[FormalA]_) \[CenterDot] (\[FormalB]_)))) -> 
          \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalB]), 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"CriticalPairLemma", 78}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalA]_)) <-> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalB]_)), "MatchingSide" -> 2, 
        "Position" -> {2, 2, 2}|>|>, {"SubstitutionLemma", 85} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
            \[FormalB]) \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
              \[FormalA]))))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 108}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 41}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
           \[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
              \[FormalB]) \[CenterDot] (\[FormalC] \[CenterDot] 
              (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
                \[FormalA]))))], "ConstructSide" -> 1, "InputOrientation" -> 
         1, "Side" -> 1|>|>, {"CriticalPairLemma", 109} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalC])) \[CenterDot] (\[FormalC] \[CenterDot] 
            \[FormalC])) == \[FormalA] \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 68}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             ((\[FormalB]_) \[CenterDot] (\[FormalC]_)))) -> 
          \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalC]), 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalC]_))), "MatchingConstruct" -> {"SubstitutionLemma", 
          76}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalAlpha]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
              (\[FormalC]_)))) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 110} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
            (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
              \[FormalC]))) \[CenterDot] ((\[FormalC] \[CenterDot] 
             \[FormalC]) \[CenterDot] (\[FormalC] \[CenterDot] 
             \[FormalC])))], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 109}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalC]_))) -> 
          \[FormalA] \[CenterDot] \[FormalA], "Side" -> 1, 
        "Subpattern" -> (\[FormalB]_) \[CenterDot] (\[FormalC]_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 79}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)) <-> (\[FormalA]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalB]_)), "MatchingSide" -> 1, 
        "Position" -> {2, 1, 2}|>|>, {"SubstitutionLemma", 86} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
            (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
              \[FormalC]))) \[CenterDot] \[FormalC])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 110}, 
        "Position" -> {2, 2}, "Construct" -> {"SubstitutionLemma", 41}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)) -> \[FormalB], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
           \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
                \[FormalC]))) \[CenterDot] \[FormalC])], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 87} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
           (\[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] 
             (\[FormalC] \[CenterDot] \[FormalB]))))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 86}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 58}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
           \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             (\[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] (
                \[FormalC] \[CenterDot] \[FormalB]))))], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 111} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
           (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
             (\[FormalA] \[CenterDot] \[FormalC]))))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 87}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] (
                \[FormalB]_))))) -> \[FormalA] \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
              (\[FormalB]_))))), "MatchingConstruct" -> {"CriticalPairLemma", 
          89}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             ((\[FormalA]_) \[CenterDot] (\[FormalB]_))) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalC]_))) -> 
          \[FormalA] \[CenterDot] \[FormalC], "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 112} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalC] \[CenterDot] \[FormalA]))) == \[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 68}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             ((\[FormalB]_) \[CenterDot] (\[FormalC]_)))) -> 
          \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalC]), 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalC]_))), "MatchingConstruct" -> {"SubstitutionLemma", 
          87}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
              ((\[FormalC]_) \[CenterDot] (\[FormalB]_))))) -> 
          \[FormalA] \[CenterDot] \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 113} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         \[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
            (\[FormalC] \[CenterDot] (\[FormalA] \[CenterDot] 
              \[FormalB]))) \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 111}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] (
                \[FormalC]_))))) -> \[FormalA] \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          ((\[FormalC]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalC]_))), "MatchingConstruct" -> {"CriticalPairLemma", 
          91}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] (\[FormalA]_)))) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalAlpha]_))) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 114} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalA]) == 
         (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
            (\[FormalC] \[CenterDot] \[FormalA])))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 111}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] (
                \[FormalC]_))))) -> \[FormalA] \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalC]_) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] (\[FormalC]_)), "MatchingConstruct" -> 
         {"CriticalPairLemma", 83}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA] \[CenterDot] \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {2, 2, 2}|>|>, 
    {"SubstitutionLemma", 88} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalB] \[CenterDot] 
           (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] \[FormalA])))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 114}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 41}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "OutputExpression" -> HoldForm[\[FormalA] == 
           (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
              (\[FormalC] \[CenterDot] \[FormalA])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 115} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalA]) == 
         (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
            (\[FormalC] \[CenterDot] (\[FormalC] \[CenterDot] 
              (\[FormalA] \[CenterDot] \[FormalC])))))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 111}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] (
                \[FormalC]_))))) -> \[FormalA] \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalC]_), "MatchingConstruct" -> {"CriticalPairLemma", 82}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalA]_)) <-> ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 2, 
        "Position" -> {2, 2, 2, 2}|>|>, {"SubstitutionLemma", 89} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalB] \[CenterDot] 
           (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
             (\[FormalC] \[CenterDot] (\[FormalA] \[CenterDot] 
               \[FormalC])))))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 115}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 41}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "OutputExpression" -> HoldForm[\[FormalA] == 
           (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
              (\[FormalC] \[CenterDot] (\[FormalC] \[CenterDot] 
                (\[FormalA] \[CenterDot] \[FormalC])))))], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 90} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalB] \[CenterDot] 
           (\[FormalB] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalC])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 89}, 
        "Position" -> {2, 2, 2}, "Construct" -> {"SubstitutionLemma", 43}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalB] \[CenterDot] \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalA] == 
           (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
              (\[FormalA] \[CenterDot] \[FormalC])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 116} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalC])) == 
         ((\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalC]))) \[CenterDot] 
           \[FormalC]) \[CenterDot] \[FormalC]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 63}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> ((\[FormalC]_) \[CenterDot] 
           (\[FormalB]_)) \[CenterDot] (\[FormalA]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 88}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
              (\[FormalA]_)))) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 117} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalB] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalC]) \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 90}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (\[FormalC]_)))) -> \[FormalA], "Side" -> 1, 
        "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] (\[FormalC]_)), "MatchingConstruct" -> 
         {"SubstitutionLemma", 66}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
              (\[FormalC]_)) \[CenterDot] (\[FormalA]_))) -> 
          (\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"SubstitutionLemma", 91} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
           (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
             (\[FormalA] \[CenterDot] \[FormalB]))))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 113}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 58}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
           \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] (
                \[FormalA] \[CenterDot] \[FormalB]))))], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 118} -> 
     <|"Statement" -> HoldForm[
        ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB])) \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalC]) == 
         (\[FormalAlpha] \[CenterDot] (\[FormalAlpha] \[CenterDot] 
            \[FormalAlpha])) \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 99}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
              (\[FormalB]_))) \[CenterDot] (\[FormalC]_)) -> \[FormalC], 
        "Side" -> 1, "Subpattern" -> ((\[FormalB]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalB]_))) \[CenterDot] 
          (\[FormalC]_), "MatchingConstruct" -> {"SubstitutionLemma", 80}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
            (((\[FormalB]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
             (\[FormalAlpha]_))) -> \[FormalC], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 92} -> 
     <|"Statement" -> HoldForm[
        ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB])) \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalC]) == 
         \[FormalA] \[CenterDot] \[FormalB]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 118}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 98}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (\[FormalA]_))) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)) -> \[FormalB], "OutputExpression" -> 
         HoldForm[((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
             (\[FormalA] \[CenterDot] \[FormalB])) \[CenterDot] 
            ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalC]) == 
           \[FormalA] \[CenterDot] \[FormalB]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 119} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
             \[FormalAlpha]))) \[CenterDot] ((\[FormalAlpha] \[CenterDot] 
            \[FormalC]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 82}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) \[CenterDot] 
           (((\[FormalAlpha]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
              (\[FormalC]_))) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalAlpha]_) \[CenterDot] 
          ((\[FormalC]_) \[CenterDot] (\[FormalC]_)), "MatchingConstruct" -> 
         {"CriticalPairLemma", 60}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalC]_))) -> 
          \[FormalC] \[CenterDot] \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2, 1}|>|>, {"SubstitutionLemma", 93} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalAlpha] \[CenterDot] (\[FormalC] \[CenterDot] 
             \[FormalB])))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 119}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 58}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == ((\[FormalB] \[CenterDot] 
              \[FormalC]) \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalA] \[CenterDot] (\[FormalAlpha] \[CenterDot] 
              (\[FormalC] \[CenterDot] \[FormalB])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 120} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
             \[FormalC]) \[CenterDot] (\[FormalC] \[CenterDot] 
             \[FormalB]))) == (\[FormalC] \[CenterDot] 
           \[FormalB]) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 97}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) <-> 
          ((\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] (\[FormalC]_)))) \[CenterDot] 
           (\[FormalA]_), "Side" -> 2, "Subpattern" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalB]_))), 
        "MatchingConstruct" -> {"SubstitutionLemma", 93}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
            ((\[FormalAlpha]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
              (\[FormalA]_)))) -> \[FormalC], "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 121} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] \[FormalC] == \[FormalC] \[CenterDot] 
          (((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB])) \[CenterDot] \[FormalC])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 120}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
              (\[FormalC]_)) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
              (\[FormalB]_)))) -> (\[FormalC] \[CenterDot] 
            \[FormalB]) \[CenterDot] \[FormalA], "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
           ((\[FormalC]_) \[CenterDot] (\[FormalB]_))), 
        "MatchingConstruct" -> {"SubstitutionLemma", 58}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 122} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] \[FormalC] == \[FormalC] \[CenterDot] 
          (((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalA])) \[CenterDot] \[FormalC])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 121}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] (\[FormalB]_))) \[CenterDot] 
            (\[FormalA]_)) -> (\[FormalC] \[CenterDot] 
            \[FormalB]) \[CenterDot] \[FormalA], "Side" -> 1, 
        "Subpattern" -> (\[FormalC]_) \[CenterDot] (\[FormalB]_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 58}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2, 1, 2}|>|>, {"SubstitutionLemma", 94} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] \[FormalC] == \[FormalC] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 122}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 83}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalA]_)) -> 
          \[FormalA] \[CenterDot] \[FormalB], "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalC] == \[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalA])], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 123} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] \[FormalC]) == 
         (\[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] 
            \[FormalB])) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 94}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (\[FormalC]_) <-> 
          (\[FormalC]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalA]_)), "Side" -> 1, "Subpattern" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 78}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) <-> 
          (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)), "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 124} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] \[FormalC] == 
         (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
          ((\[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalA])) \[CenterDot] (\[FormalC] \[CenterDot] 
            \[FormalAlpha]))], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 64}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) -> \[FormalA] \[CenterDot] \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"SubstitutionLemma", 94}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalC]_) <-> (\[FormalC]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)), "MatchingSide" -> 1, 
        "Position" -> {2, 1}|>|>, {"CriticalPairLemma", 125} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
          \[FormalAlpha] == \[FormalAlpha] \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 94}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (\[FormalC]_) <-> 
          (\[FormalC]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalA]_)), "Side" -> 2, "Subpattern" -> 
         (\[FormalB]_) \[CenterDot] (\[FormalC]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 94}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (\[FormalC]_) <-> 
          (\[FormalC]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalA]_)), "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 126} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalC] \[CenterDot] \[FormalA]))) == \[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 112}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] (\[FormalA]_)))) <-> 
          (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)), "Side" -> 2, "Subpattern" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
           (\[FormalB]_)), "MatchingConstruct" -> {"CriticalPairLemma", 79}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)) <-> (\[FormalA]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalB]_)), "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 127} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalA]) \[CenterDot] ((\[FormalB] \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalC])) == 
         (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalC]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 126}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] (\[FormalA]_)))) -> 
          \[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB]), 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          ((\[FormalC]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
            (\[FormalA]_))), "MatchingConstruct" -> {"SubstitutionLemma", 
          78}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
            ((\[FormalAlpha]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
              (\[FormalB]_)))) -> \[FormalC], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 128} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalB]) == \[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalC]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 126}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] (\[FormalA]_)))) -> 
          \[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB]), 
        "Side" -> 1, "Subpattern" -> (\[FormalC]_) \[CenterDot] 
          ((\[FormalC]_) \[CenterDot] (\[FormalA]_)), "MatchingConstruct" -> 
         {"CriticalPairLemma", 84}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalA]_)), "MatchingSide" -> 2, "Position" -> {2, 2}|>|>, 
    {"SubstitutionLemma", 95} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalC])) == 
         (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
            \[FormalC])) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 123}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 94}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalC]_) -> \[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] 
            \[FormalA]), "OutputExpression" -> HoldForm[
          \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalC])) == 
           (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
              \[FormalC])) \[CenterDot] \[FormalA]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 129} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
            \[FormalAlpha])) == ((\[FormalAlpha] \[CenterDot] 
            \[FormalC]) \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 125}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) \[CenterDot] 
           (\[FormalAlpha]_) <-> (\[FormalAlpha]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalB]_))), "Side" -> 1, "Subpattern" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
           (\[FormalC]_)), "MatchingConstruct" -> {"SubstitutionLemma", 58}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 130} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
          \[FormalAlpha] == \[FormalAlpha] \[CenterDot] 
          ((\[FormalC] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 125}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) \[CenterDot] 
           (\[FormalAlpha]_) <-> (\[FormalAlpha]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalB]_))), "Side" -> 2, "Subpattern" -> 
         (\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
           (\[FormalAlpha]_)), "MatchingConstruct" -> {"SubstitutionLemma", 
          58}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 96} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB]) == 
         (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
          (\[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalC]) \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 76}, "Position" -> {2}, 
        "Construct" -> {"CriticalPairLemma", 130}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] (\[FormalAlpha]_) -> 
          \[FormalAlpha] \[CenterDot] ((\[FormalC] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA]), "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB]) == 
           (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            (\[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalC]) \[CenterDot] \[FormalB]))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 97} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalC])) == 
         \[FormalC] \[CenterDot] (\[FormalC] \[CenterDot] 
           ((\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
              \[FormalC])) \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 116}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 129}, "Orientation" -> 1, 
        "Rule" -> (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] (\[FormalAlpha]_) -> 
          \[FormalAlpha] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalA])), "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalC])) == \[FormalC] \[CenterDot] 
            (\[FormalC] \[CenterDot] ((\[FormalA] \[CenterDot] (
                \[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
              \[FormalA]))], "ConstructSide" -> 2, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 98} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalC])) == 
         \[FormalC] \[CenterDot] (\[FormalC] \[CenterDot] 
           (\[FormalA] \[CenterDot] ((\[FormalC] \[CenterDot] 
              \[FormalB]) \[CenterDot] \[FormalA])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 97}, 
        "Position" -> {2, 2}, "Construct" -> {"CriticalPairLemma", 130}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) \[CenterDot] 
           (\[FormalAlpha]_) -> \[FormalAlpha] \[CenterDot] 
           ((\[FormalC] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA]), 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] 
            (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalC])) == 
           \[FormalC] \[CenterDot] (\[FormalC] \[CenterDot] 
             (\[FormalA] \[CenterDot] ((\[FormalC] \[CenterDot] 
                \[FormalB]) \[CenterDot] \[FormalA])))], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 131} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalB]) == \[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalC] \[CenterDot] (\[FormalAlpha] \[CenterDot] 
              (\[FormalA] \[CenterDot] \[FormalA])))))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 128}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalC]_))) -> 
          \[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB]), 
        "Side" -> 1, "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           (\[FormalA]_)) \[CenterDot] (\[FormalC]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 98}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (((\[FormalA]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
              (\[FormalB]_)))) -> \[FormalB] \[CenterDot] 
           (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] \[FormalA])), 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"SubstitutionLemma", 99} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           \[FormalC]) \[CenterDot] ((\[FormalC] \[CenterDot] 
            \[FormalAlpha]) \[CenterDot] (\[FormalB] \[CenterDot] 
            \[FormalA]))], "Proof" -> <|"Input" -> {"CriticalPairLemma", 92}, 
        "Position" -> {}, "Construct" -> {"CriticalPairLemma", 129}, 
        "Orientation" -> 1, "Rule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] (\[FormalAlpha]_) -> 
          \[FormalAlpha] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalA])), "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
           ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
             \[FormalC]) \[CenterDot] ((\[FormalC] \[CenterDot] 
              \[FormalAlpha]) \[CenterDot] (\[FormalB] \[CenterDot] 
              \[FormalA]))], "ConstructSide" -> 2, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 132} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] ((\[FormalB] \[CenterDot] 
            \[FormalA]) \[CenterDot] \[FormalC]) == 
         ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
           \[FormalC]) \[CenterDot] (\[FormalB] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 100}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
              ((\[FormalB]_) \[CenterDot] (\[FormalB]_))) \[CenterDot] 
             (\[FormalC]_))) -> \[FormalC] \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (((\[FormalB]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalB]_))) \[CenterDot] (\[FormalC]_)), 
        "MatchingConstruct" -> {"SubstitutionLemma", 99}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             (\[FormalAlpha]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalA] \[CenterDot] \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 100} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] ((\[FormalB] \[CenterDot] 
            \[FormalA]) \[CenterDot] \[FormalC]) == 
         (\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
          (\[FormalC] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 132}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 129}, "Orientation" -> 1, 
        "Rule" -> (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] (\[FormalAlpha]_) -> 
          \[FormalAlpha] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalA])), "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalC]) == 
           (\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalC] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB]))], 
        "ConstructSide" -> 2, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 133} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] \[FormalB] == 
         (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
           (\[FormalC] \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 85}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              ((\[FormalB]_) \[CenterDot] (\[FormalA]_))))) -> 
          \[FormalA] \[CenterDot] \[FormalB], "Side" -> 1, 
        "Subpattern" -> (\[FormalC]_) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalA]_))), "MatchingConstruct" -> {"SubstitutionLemma", 
          92}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalB]_))) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) -> \[FormalA] \[CenterDot] \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"CriticalPairLemma", 134} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] \[FormalB] == 
         (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
          ((\[FormalC] \[CenterDot] \[FormalA]) \[CenterDot] 
           ((\[FormalC] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 133}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_))) -> (\[FormalA] \[CenterDot] 
            \[FormalA]) \[CenterDot] \[FormalB], "Side" -> 1, 
        "Subpattern" -> ((\[FormalB]_) \[CenterDot] 
           (\[FormalB]_)) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
           (\[FormalA]_)), "MatchingConstruct" -> {"CriticalPairLemma", 84}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           (\[FormalB]_) <-> (\[FormalB]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 135} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] ((\[FormalB] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalC])) \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalAlpha])) == 
         (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
          ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
           ((\[FormalC] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 127}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalC]_))) -> 
          (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalC], 
        "Side" -> 1, "Subpattern" -> ((\[FormalB]_) \[CenterDot] 
           (\[FormalA]_)) \[CenterDot] (\[FormalC]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 124}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             ((\[FormalB]_) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalAlpha]_))) -> 
          (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalC], 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"SubstitutionLemma", 101} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] ((\[FormalB] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalC])) \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalAlpha])) == 
         (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalB]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 135}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 127}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
            (((\[FormalB]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
             (\[FormalC]_))) -> (\[FormalA] \[CenterDot] 
            \[FormalA]) \[CenterDot] \[FormalC], "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
            ((\[FormalB] \[CenterDot] (\[FormalA] \[CenterDot] 
               \[FormalC])) \[CenterDot] (\[FormalB] \[CenterDot] 
              \[FormalAlpha])) == (\[FormalA] \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalB]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 136} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalB]) == \[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] ((\[FormalC] \[CenterDot] 
             ((\[FormalC] \[CenterDot] \[FormalA]) \[CenterDot] 
              \[FormalAlpha])) \[CenterDot] \[FormalC]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 131}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] ((\[FormalAlpha]_) \[CenterDot] (
                (\[FormalA]_) \[CenterDot] (\[FormalA]_)))))) -> 
          \[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB]), 
        "Side" -> 1, "Subpattern" -> (\[FormalC]_) \[CenterDot] 
          ((\[FormalAlpha]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_))), "MatchingConstruct" -> {"CriticalPairLemma", 
          93}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
              (\[FormalB]_)) \[CenterDot] (\[FormalC]_))) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalAlpha]_) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2, 2, 2}|>|>, {"SubstitutionLemma", 102} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalB]) == \[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
            ((\[FormalAlpha] \[CenterDot] (\[FormalC] \[CenterDot] 
               \[FormalA])) \[CenterDot] \[FormalC])))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 136}, 
        "Position" -> {2, 2}, "Construct" -> {"CriticalPairLemma", 130}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) \[CenterDot] 
           (\[FormalAlpha]_) -> \[FormalAlpha] \[CenterDot] 
           ((\[FormalC] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA]), 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB]) == \[FormalA] \[CenterDot] 
            (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
              ((\[FormalAlpha] \[CenterDot] (\[FormalC] \[CenterDot] 
                 \[FormalA])) \[CenterDot] \[FormalC])))], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 103} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalB]) == \[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalC] \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalC]) \[CenterDot] \[FormalAlpha]))))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 102}, 
        "Position" -> {2, 2, 2}, "Construct" -> {"CriticalPairLemma", 130}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) \[CenterDot] 
           (\[FormalAlpha]_) -> \[FormalAlpha] \[CenterDot] 
           ((\[FormalC] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA]), 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB]) == \[FormalA] \[CenterDot] 
            (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
              (\[FormalC] \[CenterDot] ((\[FormalA] \[CenterDot] 
                 \[FormalC]) \[CenterDot] \[FormalAlpha]))))], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 137} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalB])) == 
         \[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
            \[FormalB]) \[CenterDot] ((\[FormalA] \[CenterDot] 
             (\[FormalC] \[CenterDot] \[FormalB])) \[CenterDot] 
            \[FormalAlpha]))], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 103}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
              (((\[FormalA]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] (
                \[FormalAlpha]_))))) -> \[FormalA] \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB]), "Side" -> 1, 
        "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          ((\[FormalC]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
             (\[FormalAlpha]_)))), "MatchingConstruct" -> 
         {"CriticalPairLemma", 134}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalC]_))) -> 
          (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalC], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 104} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalA]) \[CenterDot] ((\[FormalB] \[CenterDot] 
             (\[FormalC] \[CenterDot] \[FormalA])) \[CenterDot] 
            \[FormalAlpha]))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 137}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 80}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalB]_))) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
           \[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalA]) \[CenterDot] ((\[FormalB] \[CenterDot] (
                \[FormalC] \[CenterDot] \[FormalA])) \[CenterDot] 
              \[FormalAlpha]))], "ConstructSide" -> 1, "InputOrientation" -> 
         1, "Side" -> 1|>|>, {"CriticalPairLemma", 138} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalB] \[CenterDot] ((\[FormalC] \[CenterDot] 
            (\[FormalB] \[CenterDot] (\[FormalAlpha] \[CenterDot] 
              \[FormalA]))) \[CenterDot] (\[FormalA] \[CenterDot] 
            \[FormalA]))], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 
          104}, "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] (
                \[FormalB]_))) \[CenterDot] (\[FormalAlpha]_))) -> 
          \[FormalB] \[CenterDot] \[FormalA], "Side" -> 1, 
        "Subpattern" -> ((\[FormalB]_) \[CenterDot] 
           (\[FormalB]_)) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalB]_))) \[CenterDot] 
           (\[FormalAlpha]_)), "MatchingConstruct" -> {"SubstitutionLemma", 
          54}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (((\[FormalC]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
             (\[FormalA]_))) -> (\[FormalC] \[CenterDot] 
            \[FormalB]) \[CenterDot] \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 139} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] \[FormalC] == 
         \[FormalC] \[CenterDot] ((\[FormalB] \[CenterDot] 
            \[FormalB]) \[CenterDot] ((\[FormalA] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
            (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
              \[FormalC]))))], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 138}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             ((\[FormalA]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] (
                \[FormalAlpha]_)))) \[CenterDot] 
            ((\[FormalAlpha]_) \[CenterDot] (\[FormalAlpha]_))) -> 
          \[FormalAlpha] \[CenterDot] \[FormalA], "Side" -> 1, 
        "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
            (\[FormalAlpha]_))), "MatchingConstruct" -> {"SubstitutionLemma", 
          91}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
              ((\[FormalA]_) \[CenterDot] (\[FormalB]_))))) -> 
          \[FormalA] \[CenterDot] \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2, 1}|>|>, {"SubstitutionLemma", 105} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] \[FormalC] == 
         \[FormalC] \[CenterDot] ((\[FormalB] \[CenterDot] 
            \[FormalB]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 139}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 101}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (\[FormalC]_))) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalAlpha]_))) -> (\[FormalA] \[CenterDot] 
            \[FormalA]) \[CenterDot] \[FormalB], "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
              \[FormalC])) \[CenterDot] \[FormalC] == \[FormalC] \[CenterDot] 
            ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA])], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 106} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalC]) == 
         \[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
            \[FormalB]) \[CenterDot] \[FormalC])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 105}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 130}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] (\[FormalAlpha]_) -> 
          \[FormalAlpha] \[CenterDot] ((\[FormalC] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA]), "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] \[FormalC]) == \[FormalA] \[CenterDot] 
            ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalC])], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 140} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalB])) \[CenterDot] \[FormalC]) == 
         \[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] 
           (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB])))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 106}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) <-> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)), "Side" -> 1, "Subpattern" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
          (\[FormalC]_), "MatchingConstruct" -> {"SubstitutionLemma", 95}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) <-> 
          ((\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 2, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 107} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalC]) == \[FormalA] \[CenterDot] 
          (\[FormalC] \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB])))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 140}, 
        "Position" -> {2, 1}, "Construct" -> {"SubstitutionLemma", 41}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)) -> \[FormalB], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalC]) == \[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] 
             (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
               \[FormalB])))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"CriticalPairLemma", 141} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB])) \[CenterDot] 
           \[FormalAlpha]) == \[FormalA] \[CenterDot] 
          (\[FormalAlpha] \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB])))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 107}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             ((\[FormalA]_) \[CenterDot] (\[FormalC]_)))) -> 
          \[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] \[FormalB]), 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalC]_), "MatchingConstruct" -> {"SubstitutionLemma", 63}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalA] \[CenterDot] \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {2, 2, 2}|>|>, 
    {"SubstitutionLemma", 108} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB])) \[CenterDot] 
           \[FormalAlpha]) == \[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalAlpha])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 141}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 107}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (\[FormalC]_)))) -> \[FormalA] \[CenterDot] 
           (\[FormalC] \[CenterDot] \[FormalB]), "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (((\[FormalB] \[CenterDot] 
               \[FormalC]) \[CenterDot] (\[FormalA] \[CenterDot] 
               \[FormalB])) \[CenterDot] \[FormalAlpha]) == 
           \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalAlpha])], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 142} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalC]) == \[FormalA] \[CenterDot] 
          (\[FormalC] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalC]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 108}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
             ((\[FormalA]_) \[CenterDot] (\[FormalB]_))) \[CenterDot] 
            (\[FormalAlpha]_)) -> \[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalAlpha]), "Side" -> 1, 
        "Subpattern" -> (((\[FormalB]_) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_))) \[CenterDot] (\[FormalAlpha]_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 101}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalA]_)) <-> (((\[FormalC]_) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] (\[FormalC]_))) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 2, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 143} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalB] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalC]) \[CenterDot] \[FormalB])) == 
         (\[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalC]) \[CenterDot] \[FormalB])) \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 82}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) <-> 
          ((\[FormalB]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalA]_), "Side" -> 1, "Subpattern" -> 
         (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 142}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
              (\[FormalC]_)) \[CenterDot] (\[FormalB]_))) -> 
          \[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] \[FormalB]), 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 109} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalC]) \[CenterDot] 
            \[FormalB])) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalC] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 143}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 117}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
              (\[FormalC]_)) \[CenterDot] (\[FormalB]_))) -> \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalA] == 
           (\[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalC]) \[CenterDot] \[FormalB])) \[CenterDot] 
            (\[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] \[FormalB]))], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 110} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
          ((\[FormalC] \[CenterDot] (\[FormalA] \[CenterDot] 
             \[FormalB])) \[CenterDot] \[FormalC])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 109}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 130}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] (\[FormalAlpha]_) -> 
          \[FormalAlpha] \[CenterDot] ((\[FormalC] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA]), "OutputExpression" -> 
         HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
            ((\[FormalC] \[CenterDot] (\[FormalA] \[CenterDot] 
               \[FormalB])) \[CenterDot] \[FormalC])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 111} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
          (\[FormalC] \[CenterDot] ((\[FormalB] \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalC]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 110}, "Position" -> {2}, 
        "Construct" -> {"CriticalPairLemma", 130}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] (\[FormalAlpha]_) -> 
          \[FormalAlpha] \[CenterDot] ((\[FormalC] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA]), "OutputExpression" -> 
         HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
            (\[FormalC] \[CenterDot] ((\[FormalB] \[CenterDot] 
               \[FormalA]) \[CenterDot] \[FormalC]))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 144} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] \[FormalA]) == 
         ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
           (\[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
              \[FormalC]) \[CenterDot] \[FormalA]))) \[CenterDot] 
          \[FormalC]], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 44}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> ((\[FormalC]_) \[CenterDot] 
           (\[FormalA]_)) \[CenterDot] (\[FormalB]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 111}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) \[CenterDot] 
           ((\[FormalC]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalC]_))) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 112} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] \[FormalA]) == 
         ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalA])) \[CenterDot] \[FormalC]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 144}, "Position" -> {1}, 
        "Construct" -> {"SubstitutionLemma", 96}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
              (\[FormalC]_)) \[CenterDot] (\[FormalB]_))) -> 
          (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB]), "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
              \[FormalC]) \[CenterDot] \[FormalA]) == 
           ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalA])) \[CenterDot] \[FormalC]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 145} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] \[FormalA]) == 
         \[FormalC] \[CenterDot] ((\[FormalB] \[CenterDot] 
            \[FormalA]) \[CenterDot] (\[FormalB] \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 112}, 
        "Orientation" -> -1, "Rule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalB]_))) \[CenterDot] 
           (\[FormalC]_) -> \[FormalB] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalC]) \[CenterDot] \[FormalB]), 
        "Side" -> 1, "Subpattern" -> (((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_))) \[CenterDot] (\[FormalC]_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 94}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalC]_) <-> (\[FormalC]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)), "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"SubstitutionLemma", 113} -> 
     <|"Statement" -> HoldForm[
        (((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
            (\[FormalD] \[CenterDot] \[FormalD])) \[CenterDot] 
           ((\[FormalD] \[CenterDot] \[FormalD]) \[CenterDot] 
            (\[FormalE] \[CenterDot] \[FormalE]))) \[CenterDot] 
          (\[FormalF] \[CenterDot] \[FormalF]) == 
         (\[FormalD] \[CenterDot] \[FormalD]) \[CenterDot] 
          (((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
            (\[FormalF] \[CenterDot] \[FormalF])) \[CenterDot] 
           ((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
            (\[FormalF] \[CenterDot] \[FormalF])))], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Position" -> {1, 1}, 
        "Construct" -> {"SubstitutionLemma", 58}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
              (\[FormalD] \[CenterDot] \[FormalD])) \[CenterDot] 
             ((\[FormalD] \[CenterDot] \[FormalD]) \[CenterDot] 
              (\[FormalE] \[CenterDot] \[FormalE]))) \[CenterDot] 
            (\[FormalF] \[CenterDot] \[FormalF]) == 
           (\[FormalD] \[CenterDot] \[FormalD]) \[CenterDot] 
            (((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
              (\[FormalF] \[CenterDot] \[FormalF])) \[CenterDot] 
             ((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
              (\[FormalF] \[CenterDot] \[FormalF])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 114} -> 
     <|"Statement" -> HoldForm[(\[FormalF] \[CenterDot] 
           \[FormalF]) \[CenterDot] (((\[FormalE] \[CenterDot] 
             \[FormalE]) \[CenterDot] (\[FormalD] \[CenterDot] 
             \[FormalD])) \[CenterDot] ((\[FormalD] \[CenterDot] 
             \[FormalD]) \[CenterDot] (\[FormalE] \[CenterDot] 
             \[FormalE]))) == (\[FormalD] \[CenterDot] 
           \[FormalD]) \[CenterDot] (((\[FormalE] \[CenterDot] 
             \[FormalE]) \[CenterDot] (\[FormalF] \[CenterDot] 
             \[FormalF])) \[CenterDot] ((\[FormalE] \[CenterDot] 
             \[FormalE]) \[CenterDot] (\[FormalF] \[CenterDot] 
             \[FormalF])))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 113}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 58}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalF] \[CenterDot] \[FormalF]) \[CenterDot] 
            (((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
              (\[FormalD] \[CenterDot] \[FormalD])) \[CenterDot] 
             ((\[FormalD] \[CenterDot] \[FormalD]) \[CenterDot] 
              (\[FormalE] \[CenterDot] \[FormalE]))) == 
           (\[FormalD] \[CenterDot] \[FormalD]) \[CenterDot] 
            (((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
              (\[FormalF] \[CenterDot] \[FormalF])) \[CenterDot] 
             ((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
              (\[FormalF] \[CenterDot] \[FormalF])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 115} -> 
     <|"Statement" -> HoldForm[(\[FormalF] \[CenterDot] 
           \[FormalF]) \[CenterDot] (((\[FormalE] \[CenterDot] 
             \[FormalE]) \[CenterDot] (\[FormalD] \[CenterDot] 
             \[FormalD])) \[CenterDot] ((\[FormalD] \[CenterDot] 
             \[FormalD]) \[CenterDot] (\[FormalE] \[CenterDot] 
             (\[FormalD] \[CenterDot] \[FormalD])))) == 
         (\[FormalD] \[CenterDot] \[FormalD]) \[CenterDot] 
          (((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
            (\[FormalF] \[CenterDot] \[FormalF])) \[CenterDot] 
           ((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
            (\[FormalF] \[CenterDot] \[FormalF])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 114}, 
        "Position" -> {2, 2}, "Construct" -> {"CriticalPairLemma", 78}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalB]_)) -> 
          \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalA]), 
        "OutputExpression" -> HoldForm[(\[FormalF] \[CenterDot] 
             \[FormalF]) \[CenterDot] (((\[FormalE] \[CenterDot] 
               \[FormalE]) \[CenterDot] (\[FormalD] \[CenterDot] 
               \[FormalD])) \[CenterDot] ((\[FormalD] \[CenterDot] 
               \[FormalD]) \[CenterDot] (\[FormalE] \[CenterDot] (
                \[FormalD] \[CenterDot] \[FormalD])))) == 
           (\[FormalD] \[CenterDot] \[FormalD]) \[CenterDot] 
            (((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
              (\[FormalF] \[CenterDot] \[FormalF])) \[CenterDot] 
             ((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
              (\[FormalF] \[CenterDot] \[FormalF])))], "ConstructSide" -> 2, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 116} -> 
     <|"Statement" -> HoldForm[(\[FormalF] \[CenterDot] 
           \[FormalF]) \[CenterDot] (((\[FormalE] \[CenterDot] 
             \[FormalE]) \[CenterDot] (\[FormalD] \[CenterDot] 
             \[FormalD])) \[CenterDot] ((\[FormalD] \[CenterDot] 
             \[FormalD]) \[CenterDot] ((\[FormalD] \[CenterDot] 
              \[FormalD]) \[CenterDot] \[FormalE]))) == 
         (\[FormalD] \[CenterDot] \[FormalD]) \[CenterDot] 
          (((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
            (\[FormalF] \[CenterDot] \[FormalF])) \[CenterDot] 
           ((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
            (\[FormalF] \[CenterDot] \[FormalF])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 115}, 
        "Position" -> {2, 2, 2}, "Construct" -> {"SubstitutionLemma", 58}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (\[FormalB]_) -> \[FormalB] \[CenterDot] \[FormalA], 
        "OutputExpression" -> HoldForm[(\[FormalF] \[CenterDot] 
             \[FormalF]) \[CenterDot] (((\[FormalE] \[CenterDot] 
               \[FormalE]) \[CenterDot] (\[FormalD] \[CenterDot] 
               \[FormalD])) \[CenterDot] ((\[FormalD] \[CenterDot] 
               \[FormalD]) \[CenterDot] ((\[FormalD] \[CenterDot] 
                \[FormalD]) \[CenterDot] \[FormalE]))) == 
           (\[FormalD] \[CenterDot] \[FormalD]) \[CenterDot] 
            (((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
              (\[FormalF] \[CenterDot] \[FormalF])) \[CenterDot] 
             ((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
              (\[FormalF] \[CenterDot] \[FormalF])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 117} -> 
     <|"Statement" -> HoldForm[(\[FormalF] \[CenterDot] 
           \[FormalF]) \[CenterDot] (((\[FormalE] \[CenterDot] 
             \[FormalE]) \[CenterDot] (\[FormalD] \[CenterDot] 
             \[FormalD])) \[CenterDot] (((\[FormalD] \[CenterDot] 
              \[FormalD]) \[CenterDot] \[FormalE]) \[CenterDot] 
            (\[FormalD] \[CenterDot] \[FormalD]))) == 
         (\[FormalD] \[CenterDot] \[FormalD]) \[CenterDot] 
          (((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
            (\[FormalF] \[CenterDot] \[FormalF])) \[CenterDot] 
           ((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
            (\[FormalF] \[CenterDot] \[FormalF])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 116}, 
        "Position" -> {2, 2}, "Construct" -> {"SubstitutionLemma", 58}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (\[FormalB]_) -> \[FormalB] \[CenterDot] \[FormalA], 
        "OutputExpression" -> HoldForm[(\[FormalF] \[CenterDot] 
             \[FormalF]) \[CenterDot] (((\[FormalE] \[CenterDot] 
               \[FormalE]) \[CenterDot] (\[FormalD] \[CenterDot] 
               \[FormalD])) \[CenterDot] (((\[FormalD] \[CenterDot] 
                \[FormalD]) \[CenterDot] \[FormalE]) \[CenterDot] 
              (\[FormalD] \[CenterDot] \[FormalD]))) == 
           (\[FormalD] \[CenterDot] \[FormalD]) \[CenterDot] 
            (((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
              (\[FormalF] \[CenterDot] \[FormalF])) \[CenterDot] 
             ((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
              (\[FormalF] \[CenterDot] \[FormalF])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 118} -> 
     <|"Statement" -> HoldForm[(\[FormalF] \[CenterDot] 
           \[FormalF]) \[CenterDot] (((\[FormalD] \[CenterDot] 
             \[FormalD]) \[CenterDot] (\[FormalE] \[CenterDot] 
             \[FormalE])) \[CenterDot] (((\[FormalD] \[CenterDot] 
              \[FormalD]) \[CenterDot] \[FormalE]) \[CenterDot] 
            (\[FormalD] \[CenterDot] \[FormalD]))) == 
         (\[FormalD] \[CenterDot] \[FormalD]) \[CenterDot] 
          (((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
            (\[FormalF] \[CenterDot] \[FormalF])) \[CenterDot] 
           ((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
            (\[FormalF] \[CenterDot] \[FormalF])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 117}, 
        "Position" -> {2, 1}, "Construct" -> {"SubstitutionLemma", 58}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (\[FormalB]_) -> \[FormalB] \[CenterDot] \[FormalA], 
        "OutputExpression" -> HoldForm[(\[FormalF] \[CenterDot] 
             \[FormalF]) \[CenterDot] (((\[FormalD] \[CenterDot] 
               \[FormalD]) \[CenterDot] (\[FormalE] \[CenterDot] 
               \[FormalE])) \[CenterDot] (((\[FormalD] \[CenterDot] 
                \[FormalD]) \[CenterDot] \[FormalE]) \[CenterDot] 
              (\[FormalD] \[CenterDot] \[FormalD]))) == 
           (\[FormalD] \[CenterDot] \[FormalD]) \[CenterDot] 
            (((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
              (\[FormalF] \[CenterDot] \[FormalF])) \[CenterDot] 
             ((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
              (\[FormalF] \[CenterDot] \[FormalF])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 119} -> 
     <|"Statement" -> HoldForm[(\[FormalF] \[CenterDot] 
           \[FormalF]) \[CenterDot] (((\[FormalD] \[CenterDot] 
             \[FormalD]) \[CenterDot] (\[FormalE] \[CenterDot] 
             (\[FormalD] \[CenterDot] \[FormalD]))) \[CenterDot] 
           (((\[FormalD] \[CenterDot] \[FormalD]) \[CenterDot] 
             \[FormalE]) \[CenterDot] (\[FormalD] \[CenterDot] 
             \[FormalD]))) == (\[FormalD] \[CenterDot] 
           \[FormalD]) \[CenterDot] (((\[FormalE] \[CenterDot] 
             \[FormalE]) \[CenterDot] (\[FormalF] \[CenterDot] 
             \[FormalF])) \[CenterDot] ((\[FormalE] \[CenterDot] 
             \[FormalE]) \[CenterDot] (\[FormalF] \[CenterDot] 
             \[FormalF])))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 118}, "Position" -> {2, 1}, 
        "Construct" -> {"CriticalPairLemma", 78}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)) -> \[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalA]), "OutputExpression" -> 
         HoldForm[(\[FormalF] \[CenterDot] \[FormalF]) \[CenterDot] 
            (((\[FormalD] \[CenterDot] \[FormalD]) \[CenterDot] 
              (\[FormalE] \[CenterDot] (\[FormalD] \[CenterDot] 
                \[FormalD]))) \[CenterDot] (((\[FormalD] \[CenterDot] 
                \[FormalD]) \[CenterDot] \[FormalE]) \[CenterDot] 
              (\[FormalD] \[CenterDot] \[FormalD]))) == 
           (\[FormalD] \[CenterDot] \[FormalD]) \[CenterDot] 
            (((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
              (\[FormalF] \[CenterDot] \[FormalF])) \[CenterDot] 
             ((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
              (\[FormalF] \[CenterDot] \[FormalF])))], "ConstructSide" -> 2, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 120} -> 
     <|"Statement" -> HoldForm[(\[FormalF] \[CenterDot] 
           \[FormalF]) \[CenterDot] (((\[FormalD] \[CenterDot] 
             \[FormalD]) \[CenterDot] (\[FormalE] \[CenterDot] 
             (\[FormalD] \[CenterDot] \[FormalD]))) \[CenterDot] 
           ((\[FormalE] \[CenterDot] (\[FormalD] \[CenterDot] 
              \[FormalD])) \[CenterDot] (\[FormalD] \[CenterDot] 
             \[FormalD]))) == (\[FormalD] \[CenterDot] 
           \[FormalD]) \[CenterDot] (((\[FormalE] \[CenterDot] 
             \[FormalE]) \[CenterDot] (\[FormalF] \[CenterDot] 
             \[FormalF])) \[CenterDot] ((\[FormalE] \[CenterDot] 
             \[FormalE]) \[CenterDot] (\[FormalF] \[CenterDot] 
             \[FormalF])))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 119}, "Position" -> {2, 2, 1}, 
        "Construct" -> {"SubstitutionLemma", 58}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalF] \[CenterDot] \[FormalF]) \[CenterDot] 
            (((\[FormalD] \[CenterDot] \[FormalD]) \[CenterDot] 
              (\[FormalE] \[CenterDot] (\[FormalD] \[CenterDot] 
                \[FormalD]))) \[CenterDot] ((\[FormalE] \[CenterDot] (
                \[FormalD] \[CenterDot] \[FormalD])) \[CenterDot] 
              (\[FormalD] \[CenterDot] \[FormalD]))) == 
           (\[FormalD] \[CenterDot] \[FormalD]) \[CenterDot] 
            (((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
              (\[FormalF] \[CenterDot] \[FormalF])) \[CenterDot] 
             ((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
              (\[FormalF] \[CenterDot] \[FormalF])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 121} -> 
     <|"Statement" -> HoldForm[(\[FormalF] \[CenterDot] 
           \[FormalF]) \[CenterDot] (((\[FormalD] \[CenterDot] 
             \[FormalD]) \[CenterDot] (\[FormalE] \[CenterDot] 
             (\[FormalD] \[CenterDot] \[FormalD]))) \[CenterDot] 
           ((\[FormalD] \[CenterDot] \[FormalD]) \[CenterDot] 
            (\[FormalE] \[CenterDot] (\[FormalD] \[CenterDot] 
              \[FormalD])))) == (\[FormalD] \[CenterDot] 
           \[FormalD]) \[CenterDot] (((\[FormalE] \[CenterDot] 
             \[FormalE]) \[CenterDot] (\[FormalF] \[CenterDot] 
             \[FormalF])) \[CenterDot] ((\[FormalE] \[CenterDot] 
             \[FormalE]) \[CenterDot] (\[FormalF] \[CenterDot] 
             \[FormalF])))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 120}, "Position" -> {2, 2}, 
        "Construct" -> {"SubstitutionLemma", 58}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalF] \[CenterDot] \[FormalF]) \[CenterDot] 
            (((\[FormalD] \[CenterDot] \[FormalD]) \[CenterDot] 
              (\[FormalE] \[CenterDot] (\[FormalD] \[CenterDot] 
                \[FormalD]))) \[CenterDot] ((\[FormalD] \[CenterDot] 
               \[FormalD]) \[CenterDot] (\[FormalE] \[CenterDot] (
                \[FormalD] \[CenterDot] \[FormalD])))) == 
           (\[FormalD] \[CenterDot] \[FormalD]) \[CenterDot] 
            (((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
              (\[FormalF] \[CenterDot] \[FormalF])) \[CenterDot] 
             ((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
              (\[FormalF] \[CenterDot] \[FormalF])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 122} -> 
     <|"Statement" -> HoldForm[(\[FormalF] \[CenterDot] 
           \[FormalF]) \[CenterDot] (((\[FormalD] \[CenterDot] 
             \[FormalD]) \[CenterDot] (\[FormalE] \[CenterDot] 
             (\[FormalD] \[CenterDot] \[FormalD]))) \[CenterDot] 
           (\[FormalF] \[CenterDot] \[FormalF])) == 
         (\[FormalD] \[CenterDot] \[FormalD]) \[CenterDot] 
          (((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
            (\[FormalF] \[CenterDot] \[FormalF])) \[CenterDot] 
           ((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
            (\[FormalF] \[CenterDot] \[FormalF])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 121}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 78}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)) -> \[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalA]), "OutputExpression" -> 
         HoldForm[(\[FormalF] \[CenterDot] \[FormalF]) \[CenterDot] 
            (((\[FormalD] \[CenterDot] \[FormalD]) \[CenterDot] 
              (\[FormalE] \[CenterDot] (\[FormalD] \[CenterDot] 
                \[FormalD]))) \[CenterDot] (\[FormalF] \[CenterDot] 
              \[FormalF])) == (\[FormalD] \[CenterDot] 
             \[FormalD]) \[CenterDot] (((\[FormalE] \[CenterDot] 
               \[FormalE]) \[CenterDot] (\[FormalF] \[CenterDot] 
               \[FormalF])) \[CenterDot] ((\[FormalE] \[CenterDot] 
               \[FormalE]) \[CenterDot] (\[FormalF] \[CenterDot] 
               \[FormalF])))], "ConstructSide" -> 2, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 123} -> 
     <|"Statement" -> HoldForm[(\[FormalF] \[CenterDot] 
           \[FormalF]) \[CenterDot] ((\[FormalF] \[CenterDot] 
            \[FormalF]) \[CenterDot] ((\[FormalD] \[CenterDot] 
             \[FormalD]) \[CenterDot] (\[FormalE] \[CenterDot] 
             (\[FormalD] \[CenterDot] \[FormalD])))) == 
         (\[FormalD] \[CenterDot] \[FormalD]) \[CenterDot] 
          (((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
            (\[FormalF] \[CenterDot] \[FormalF])) \[CenterDot] 
           ((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
            (\[FormalF] \[CenterDot] \[FormalF])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 122}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 58}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalF] \[CenterDot] \[FormalF]) \[CenterDot] 
            ((\[FormalF] \[CenterDot] \[FormalF]) \[CenterDot] 
             ((\[FormalD] \[CenterDot] \[FormalD]) \[CenterDot] 
              (\[FormalE] \[CenterDot] (\[FormalD] \[CenterDot] 
                \[FormalD])))) == (\[FormalD] \[CenterDot] 
             \[FormalD]) \[CenterDot] (((\[FormalE] \[CenterDot] 
               \[FormalE]) \[CenterDot] (\[FormalF] \[CenterDot] 
               \[FormalF])) \[CenterDot] ((\[FormalE] \[CenterDot] 
               \[FormalE]) \[CenterDot] (\[FormalF] \[CenterDot] 
               \[FormalF])))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 124} -> 
     <|"Statement" -> HoldForm[(\[FormalF] \[CenterDot] 
           \[FormalF]) \[CenterDot] ((\[FormalF] \[CenterDot] 
            \[FormalF]) \[CenterDot] ((\[FormalD] \[CenterDot] 
             \[FormalD]) \[CenterDot] (\[FormalE] \[CenterDot] 
             \[FormalE]))) == (\[FormalD] \[CenterDot] 
           \[FormalD]) \[CenterDot] (((\[FormalE] \[CenterDot] 
             \[FormalE]) \[CenterDot] (\[FormalF] \[CenterDot] 
             \[FormalF])) \[CenterDot] ((\[FormalE] \[CenterDot] 
             \[FormalE]) \[CenterDot] (\[FormalF] \[CenterDot] 
             \[FormalF])))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 123}, "Position" -> {2, 2}, 
        "Construct" -> {"CriticalPairLemma", 78}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalB]), "OutputExpression" -> 
         HoldForm[(\[FormalF] \[CenterDot] \[FormalF]) \[CenterDot] 
            ((\[FormalF] \[CenterDot] \[FormalF]) \[CenterDot] 
             ((\[FormalD] \[CenterDot] \[FormalD]) \[CenterDot] 
              (\[FormalE] \[CenterDot] \[FormalE]))) == 
           (\[FormalD] \[CenterDot] \[FormalD]) \[CenterDot] 
            (((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
              (\[FormalF] \[CenterDot] \[FormalF])) \[CenterDot] 
             ((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
              (\[FormalF] \[CenterDot] \[FormalF])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 125} -> 
     <|"Statement" -> HoldForm[(\[FormalF] \[CenterDot] 
           \[FormalF]) \[CenterDot] ((\[FormalF] \[CenterDot] 
            \[FormalF]) \[CenterDot] ((\[FormalE] \[CenterDot] 
             \[FormalE]) \[CenterDot] (\[FormalD] \[CenterDot] 
             \[FormalD]))) == (\[FormalD] \[CenterDot] 
           \[FormalD]) \[CenterDot] (((\[FormalE] \[CenterDot] 
             \[FormalE]) \[CenterDot] (\[FormalF] \[CenterDot] 
             \[FormalF])) \[CenterDot] ((\[FormalE] \[CenterDot] 
             \[FormalE]) \[CenterDot] (\[FormalF] \[CenterDot] 
             \[FormalF])))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 124}, "Position" -> {2, 2}, 
        "Construct" -> {"SubstitutionLemma", 58}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalF] \[CenterDot] \[FormalF]) \[CenterDot] 
            ((\[FormalF] \[CenterDot] \[FormalF]) \[CenterDot] 
             ((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
              (\[FormalD] \[CenterDot] \[FormalD]))) == 
           (\[FormalD] \[CenterDot] \[FormalD]) \[CenterDot] 
            (((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
              (\[FormalF] \[CenterDot] \[FormalF])) \[CenterDot] 
             ((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
              (\[FormalF] \[CenterDot] \[FormalF])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 126} -> 
     <|"Statement" -> HoldForm[(\[FormalF] \[CenterDot] 
           \[FormalF]) \[CenterDot] ((\[FormalF] \[CenterDot] 
            \[FormalF]) \[CenterDot] ((\[FormalE] \[CenterDot] 
             \[FormalE]) \[CenterDot] (\[FormalD] \[CenterDot] 
             \[FormalD]))) == (\[FormalF] \[CenterDot] 
           \[FormalF]) \[CenterDot] (((\[FormalE] \[CenterDot] 
             \[FormalE]) \[CenterDot] (\[FormalD] \[CenterDot] 
             \[FormalD])) \[CenterDot] (\[FormalF] \[CenterDot] 
            \[FormalF]))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 
          125}, "Position" -> {}, "Construct" -> {"CriticalPairLemma", 145}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) -> 
          \[FormalC] \[CenterDot] ((\[FormalB] \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalC]), "OutputExpression" -> 
         HoldForm[(\[FormalF] \[CenterDot] \[FormalF]) \[CenterDot] 
            ((\[FormalF] \[CenterDot] \[FormalF]) \[CenterDot] 
             ((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
              (\[FormalD] \[CenterDot] \[FormalD]))) == 
           (\[FormalF] \[CenterDot] \[FormalF]) \[CenterDot] 
            (((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
              (\[FormalD] \[CenterDot] \[FormalD])) \[CenterDot] 
             (\[FormalF] \[CenterDot] \[FormalF]))], "ConstructSide" -> 1, 
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
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 126}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 100}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalC]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalA]_))) -> (\[FormalB] \[CenterDot] 
            \[FormalA]) \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalC]), "OutputExpression" -> 
         HoldForm[(\[FormalF] \[CenterDot] \[FormalF]) \[CenterDot] 
            ((\[FormalF] \[CenterDot] \[FormalF]) \[CenterDot] 
             ((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
              (\[FormalD] \[CenterDot] \[FormalD]))) == 
           (\[FormalF] \[CenterDot] \[FormalF]) \[CenterDot] 
            ((\[FormalF] \[CenterDot] \[FormalF]) \[CenterDot] 
             ((\[FormalE] \[CenterDot] \[FormalE]) \[CenterDot] 
              (\[FormalD] \[CenterDot] \[FormalD])))], "ConstructSide" -> 2, 
        "InputOrientation" -> 1, "Side" -> 2|>|>}|>]
