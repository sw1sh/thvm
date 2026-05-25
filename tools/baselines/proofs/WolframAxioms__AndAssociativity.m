ProofObject["EquationalLogic", {ForAll[{\[FormalA], \[FormalB], \[FormalC]}, 
   (\[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
        \[FormalC]) \[CenterDot] (\[FormalB] \[CenterDot] 
        \[FormalC]))) \[CenterDot] (\[FormalA] \[CenterDot] 
      ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
       (\[FormalB] \[CenterDot] \[FormalC]))) == 
    (((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
       (\[FormalA] \[CenterDot] \[FormalB])) \[CenterDot] 
      \[FormalC]) \[CenterDot] 
     (((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
       (\[FormalA] \[CenterDot] \[FormalB])) \[CenterDot] \[FormalC])]}, 
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
        (((\[FormalD] \[CenterDot] \[FormalE]) \[CenterDot] 
            (\[FormalD] \[CenterDot] \[FormalE])) \[CenterDot] 
           \[FormalF]) \[CenterDot] (((\[FormalD] \[CenterDot] 
             \[FormalE]) \[CenterDot] (\[FormalD] \[CenterDot] 
             \[FormalE])) \[CenterDot] \[FormalF]) == 
         (\[FormalD] \[CenterDot] ((\[FormalE] \[CenterDot] 
             \[FormalF]) \[CenterDot] (\[FormalE] \[CenterDot] 
             \[FormalF]))) \[CenterDot] (\[FormalD] \[CenterDot] 
           ((\[FormalE] \[CenterDot] \[FormalF]) \[CenterDot] 
            (\[FormalE] \[CenterDot] \[FormalF])))], "Proof" -> <||>|>, 
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
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalC]) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB])) == 
         (((\[FormalAlpha] \[CenterDot] ((\[FormalAlpha] \[CenterDot] (
                \[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
              \[FormalAlpha])) \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalC])) \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] (((\[FormalA] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalC]) \[CenterDot] 
             (\[FormalA] \[CenterDot] \[FormalB])))) \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           (\[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalC])))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 6}, 
        "Orientation" -> -1, "Rule" -> 
         ((((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
                (\[FormalB]_)) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
              (\[FormalC]_)) \[CenterDot] (\[FormalB]_))) -> \[FormalC], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          (\[FormalC]_), "MatchingConstruct" -> {"SubstitutionLemma", 4}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (((\[FormalC]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
            ((((\[FormalC]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
              (\[FormalB]_)) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
              (\[FormalA]_)))) -> \[FormalB], "MatchingSide" -> 1, 
        "Position" -> {2, 2, 1}|>|>, {"CriticalPairLemma", 16} -> 
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
        "Position" -> {1}|>|>, {"CriticalPairLemma", 17} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
             ((\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
                 \[FormalB]) \[CenterDot] \[FormalC])) \[CenterDot] 
              \[FormalA]))) \[CenterDot] (\[FormalA] \[CenterDot] 
            \[FormalB])) == (\[FormalA] \[CenterDot] 
           ((\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalC])) \[CenterDot] 
            \[FormalA])) \[CenterDot] (((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalC]) \[CenterDot] 
           ((\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] (
                (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
                \[FormalC])) \[CenterDot] \[FormalA])) \[CenterDot] 
            ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
             \[FormalC])))], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 1}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] ((((\[FormalB]_) \[CenterDot] (
                \[FormalC]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] (
                ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
                (\[FormalB]_)))) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
              (\[FormalC]_)))) -> \[FormalB] \[CenterDot] 
           ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalB]), 
        "Side" -> 1, "Subpattern" -> ((\[FormalB]_) \[CenterDot] 
           (\[FormalC]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
            (\[FormalB]_))), "MatchingConstruct" -> {"CriticalPairLemma", 1}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] ((((\[FormalB]_) \[CenterDot] (
                \[FormalC]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] (
                ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
                (\[FormalB]_)))) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
              (\[FormalC]_)))) -> \[FormalB] \[CenterDot] 
           ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalB]), 
        "MatchingSide" -> 1, "Position" -> {2, 2, 1}|>|>, 
    {"CriticalPairLemma", 18} -> 
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
    {"CriticalPairLemma", 19} -> 
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
    {"CriticalPairLemma", 20} -> 
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
         {"CriticalPairLemma", 18}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] (
                \[FormalA]_)) \[CenterDot] (\[FormalA]_)))) \[CenterDot] 
           (\[FormalA]_) -> \[FormalA] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalA]), 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 21} -> 
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
         {"CriticalPairLemma", 18}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] (
                \[FormalA]_)) \[CenterDot] (\[FormalA]_)))) \[CenterDot] 
           (\[FormalA]_) -> \[FormalA] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalA]), 
        "MatchingSide" -> 1, "Position" -> {1, 1, 2}|>|>, 
    {"CriticalPairLemma", 22} -> 
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
         {"CriticalPairLemma", 18}, "MatchingOrientation" -> -1, 
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
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 22}, 
        "Position" -> {2, 2, 2}, "Construct" -> {"CriticalPairLemma", 18}, 
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
    {"CriticalPairLemma", 23} -> 
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
        "MatchingConstruct" -> {"CriticalPairLemma", 20}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalA]_))) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2, 2, 1}|>|>, 
    {"CriticalPairLemma", 24} -> 
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
         {"CriticalPairLemma", 20}, "MatchingOrientation" -> -1, 
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
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 24}, "Position" -> {1}, 
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
         1, "Side" -> 2|>|>, {"CriticalPairLemma", 25} -> 
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
    {"CriticalPairLemma", 26} -> 
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
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 26}, 
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
        "Construct" -> {"CriticalPairLemma", 25}, "Orientation" -> -1, 
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
        "Side" -> 2|>|>, {"CriticalPairLemma", 27} -> 
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
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 21}, 
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
        "MatchingConstruct" -> {"CriticalPairLemma", 20}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalA]_))) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2, 2, 1}|>|>, 
    {"CriticalPairLemma", 28} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalA]) == 
         \[FormalA] \[CenterDot] (((\[FormalB] \[CenterDot] 
             ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
              \[FormalB])) \[CenterDot] \[FormalA]) \[CenterDot] 
           (\[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
              ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
               \[FormalB])) \[CenterDot] \[FormalA])))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 16}, 
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
        "Position" -> {2, 2, 1}|>|>, {"CriticalPairLemma", 29} -> 
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
          19}, "Orientation" -> -1, "Rule" -> 
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
          28}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((((\[FormalB]_) \[CenterDot] 
              (((\[FormalB]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] (
                \[FormalB]_))) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] (
                ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
                (\[FormalB]_))) \[CenterDot] (\[FormalA]_)))) -> 
          \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalA]), "MatchingSide" -> 1, 
        "Position" -> {2, 2, 1}|>|>, {"CriticalPairLemma", 30} -> 
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
        "MatchingConstruct" -> {"CriticalPairLemma", 29}, 
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
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalA]) == 
         \[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
            ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
             \[FormalB])) \[CenterDot] (\[FormalA] \[CenterDot] 
            ((\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
                \[FormalA]) \[CenterDot] \[FormalA])) \[CenterDot] 
             \[FormalA])))], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 28}, "Orientation" -> -1, 
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
        "MatchingConstruct" -> {"CriticalPairLemma", 29}, 
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
    {"CriticalPairLemma", 32} -> 
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
        "MatchingConstruct" -> {"CriticalPairLemma", 29}, 
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
    {"CriticalPairLemma", 33} -> 
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
         {"CriticalPairLemma", 32}, "MatchingOrientation" -> -1, 
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
            \[FormalB]))], "Proof" -> <|"Input" -> {"CriticalPairLemma", 33}, 
        "Position" -> {2, 2, 1}, "Construct" -> {"CriticalPairLemma", 31}, 
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
        "Construct" -> {"CriticalPairLemma", 25}, "Orientation" -> -1, 
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
        "Side" -> 2|>|>, {"CriticalPairLemma", 34} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA]) == 
         (\[FormalB] \[CenterDot] (\[FormalA] \[CenterDot] 
            ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
             \[FormalA]))) \[CenterDot] ((\[FormalB] \[CenterDot] 
            ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
             \[FormalB])) \[CenterDot] (\[FormalB] \[CenterDot] 
            (\[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalB]))))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 30}, 
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
    {"CriticalPairLemma", 35} -> 
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
        "MatchingConstruct" -> {"CriticalPairLemma", 34}, 
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
        "Position" -> {2}|>|>, {"CriticalPairLemma", 36} -> 
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
        "MatchingConstruct" -> {"CriticalPairLemma", 34}, 
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
        "Position" -> {2}|>|>, {"CriticalPairLemma", 37} -> 
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
       <|"Construct" -> {"CriticalPairLemma", 23}, "Orientation" -> -1, 
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
        "MatchingConstruct" -> {"CriticalPairLemma", 36}, 
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
        "Position" -> {1}|>|>, {"CriticalPairLemma", 38} -> 
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
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 37}, 
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
    {"SubstitutionLemma", 12} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
             ((\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
                 \[FormalB]) \[CenterDot] \[FormalC])) \[CenterDot] 
              \[FormalA]))) \[CenterDot] (\[FormalA] \[CenterDot] 
            \[FormalB])) == (\[FormalA] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalC])) \[CenterDot] (((\[FormalA] \[CenterDot] 
             ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
              \[FormalC])) \[CenterDot] (\[FormalA] \[CenterDot] 
             ((\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
                 \[FormalB]) \[CenterDot] \[FormalC])) \[CenterDot] 
              \[FormalA]))) \[CenterDot] (\[FormalA] \[CenterDot] 
            ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
             \[FormalC])))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 17}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 25}, "Orientation" -> -1, 
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
         HoldForm[(\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            (((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
              (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
                 ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
                  \[FormalC])) \[CenterDot] \[FormalA]))) \[CenterDot] 
             (\[FormalA] \[CenterDot] \[FormalB])) == 
           (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalC])) \[CenterDot] 
            (((\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
                 \[FormalB]) \[CenterDot] \[FormalC])) \[CenterDot] 
              (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
                 ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
                  \[FormalC])) \[CenterDot] \[FormalA]))) \[CenterDot] 
             (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
                \[FormalB]) \[CenterDot] \[FormalC])))], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 39} -> 
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
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 35}, 
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
    {"SubstitutionLemma", 13} -> 
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
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 39}, "Position" -> {}, 
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
    {"CriticalPairLemma", 40} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
            \[FormalA])) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalA]) \[CenterDot] \[FormalA]))) == 
         (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalA])) \[CenterDot] 
          (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 13}, 
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
        "MatchingConstruct" -> {"CriticalPairLemma", 27}, 
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
        "Position" -> {1}|>|>, {"SubstitutionLemma", 14} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
            \[FormalA])) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalA]) \[CenterDot] \[FormalA]))) == \[FormalA]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 40}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 20}, "Orientation" -> -1, 
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
    {"SubstitutionLemma", 15} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalA]) \[CenterDot] \[FormalA])) \[CenterDot] 
           (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
             ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
              \[FormalA])))) == \[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 38}, "Position" -> {1}, 
        "Construct" -> {"SubstitutionLemma", 14}, "Orientation" -> 1, 
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
    {"SubstitutionLemma", 16} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalA]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 15}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 14}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
              (\[FormalA]_)))) -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
           \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalA]) \[CenterDot] \[FormalA])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 17} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
              \[FormalA]) \[CenterDot] \[FormalB]))) \[CenterDot] 
          \[FormalA] == \[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
            \[FormalA]) \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 34}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 14}, "Orientation" -> 1, 
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
    {"SubstitutionLemma", 18} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalA]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 6}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 14}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
              (\[FormalA]_)))) -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
           \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalA]) \[CenterDot] \[FormalA])], "ConstructSide" -> 1, 
        "InputOrientation" -> -1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 41} -> 
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
           (\[FormalB]_)), "MatchingConstruct" -> {"SubstitutionLemma", 16}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] (\[FormalA]_)) -> 
          \[FormalA] \[CenterDot] \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2, 2, 1, 2, 1}|>|>, {"SubstitutionLemma", 19} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] \[FormalA] == \[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
             ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
              \[FormalA])) \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 41}, "Position" -> {1}, 
        "Construct" -> {"SubstitutionLemma", 16}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] (\[FormalA]_)) -> 
          \[FormalA] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
            \[FormalA] == \[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
             ((\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
                 \[FormalA]) \[CenterDot] \[FormalA])) \[CenterDot] 
              \[FormalA]))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 20} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] \[FormalA] == \[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 19}, 
        "Position" -> {2, 2, 1}, "Construct" -> {"SubstitutionLemma", 16}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA] \[CenterDot] \[FormalA], 
        "OutputExpression" -> HoldForm[(\[FormalA] \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalA] == \[FormalA] \[CenterDot] 
            (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalA]) \[CenterDot] \[FormalA]))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 21} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] \[FormalA] == \[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 20}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 16}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] (\[FormalA]_)) -> 
          \[FormalA] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
            \[FormalA] == \[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
             \[FormalA])], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 22} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalA])) == 
         \[FormalA]], "Proof" -> <|"Input" -> {"CriticalPairLemma", 20}, 
        "Position" -> {1}, "Construct" -> {"SubstitutionLemma", 16}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA] \[CenterDot] \[FormalA], 
        "OutputExpression" -> HoldForm[(\[FormalA] \[CenterDot] 
             \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
             ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
              \[FormalA])) == \[FormalA]], "ConstructSide" -> 1, 
        "InputOrientation" -> -1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 23} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalA]) == 
         \[FormalA]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 22}, 
        "Position" -> {2}, "Construct" -> {"SubstitutionLemma", 16}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA] \[CenterDot] \[FormalA], 
        "OutputExpression" -> HoldForm[(\[FormalA] \[CenterDot] 
             \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalA]) == 
           \[FormalA]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"CriticalPairLemma", 42} -> 
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
         {"SubstitutionLemma", 21}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] (\[FormalA]_) -> 
          \[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalA]), 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"SubstitutionLemma", 24} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         \[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 18}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 21}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           (\[FormalA]_) -> \[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
            \[FormalA]), "OutputExpression" -> HoldForm[
          \[FormalA] \[CenterDot] \[FormalA] == \[FormalA] \[CenterDot] 
            (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalA]))], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 25} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 42}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 24}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalA]_))) -> 
          \[FormalA] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == ((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalA])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 43} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         (((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
            \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
            \[FormalA])) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 25}, 
        "Orientation" -> -1, "Rule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA], "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CenterDot] (\[FormalA]_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 23}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 44} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CenterDot] 
           ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
            \[FormalB])) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 25}, 
        "Orientation" -> -1, "Rule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA], "Side" -> 1, 
        "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           (\[FormalB]_)) \[CenterDot] (\[FormalA]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 17}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] (
                \[FormalA]_)) \[CenterDot] (\[FormalB]_)))) \[CenterDot] 
           (\[FormalA]_) -> \[FormalB] \[CenterDot] 
           ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalB]), 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 45} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         (\[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
             (\[FormalA] \[CenterDot] \[FormalA])) \[CenterDot] 
            \[FormalB])) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 43}, 
        "Orientation" -> -1, "Rule" -> 
         ((((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (\[FormalA]_))) \[CenterDot] (\[FormalA]_) -> 
          \[FormalA] \[CenterDot] \[FormalA], "Side" -> 1, 
        "Subpattern" -> (((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] (\[FormalA]_)), "MatchingConstruct" -> 
         {"SubstitutionLemma", 17}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] (
                \[FormalA]_)) \[CenterDot] (\[FormalB]_)))) \[CenterDot] 
           (\[FormalA]_) -> \[FormalB] \[CenterDot] 
           ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalB]), 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"SubstitutionLemma", 26} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
            ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
             \[FormalB]))) == \[FormalB] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 13}, 
        "Position" -> {2, 2}, "Construct" -> {"SubstitutionLemma", 17}, 
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
    {"CriticalPairLemma", 46} -> 
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
         {"CriticalPairLemma", 44}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
             (\[FormalA]_))) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)) -> \[FormalB], "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 47} -> 
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
         {"CriticalPairLemma", 45}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] (
                \[FormalB]_))) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
           (\[FormalB]_) -> \[FormalB] \[CenterDot] \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 48} -> 
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
          46}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] (
                \[FormalA]_))) \[CenterDot] (\[FormalB]_))) -> 
          \[FormalA] \[CenterDot] \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2, 2, 1}|>|>, {"SubstitutionLemma", 27} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
              \[FormalB]))) \[CenterDot] \[FormalA]) == 
         \[FormalA] \[CenterDot] \[FormalB]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 48}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 47}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalB]_))) -> \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] 
            ((\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
                \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
                \[FormalB]))) \[CenterDot] \[FormalA]) == 
           \[FormalA] \[CenterDot] \[FormalB]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 49} -> 
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
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 27}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] (
                \[FormalB]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] (
                \[FormalB]_)))) \[CenterDot] (\[FormalA]_)) -> 
          \[FormalA] \[CenterDot] \[FormalB], "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 25}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2, 1, 2, 1}|>|>, {"SubstitutionLemma", 28} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           \[FormalA]) \[CenterDot] ((((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalA] \[CenterDot] (((\[FormalA] \[CenterDot] 
                \[FormalB]) \[CenterDot] \[FormalA]) \[CenterDot] 
              (\[FormalA] \[CenterDot] \[FormalA])))) \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 49}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 25}, "Orientation" -> -1, 
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
    {"SubstitutionLemma", 29} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           \[FormalA]) \[CenterDot] ((((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalA])) \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 28}, 
        "Position" -> {2, 1, 2, 2}, "Construct" -> {"SubstitutionLemma", 25}, 
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
    {"SubstitutionLemma", 30} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 29}, 
        "Position" -> {2, 1}, "Construct" -> {"SubstitutionLemma", 25}, 
        "Orientation" -> -1, "Rule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == ((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalA]))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 50} -> 
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
       <|"Construct" -> {"SubstitutionLemma", 30}, "Orientation" -> -1, 
        "Rule" -> (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalA], "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 25}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {1, 1}|>|>, {"SubstitutionLemma", 31} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] \[FormalA] == 
         (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA])) \[CenterDot] 
          (((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
            ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
             \[FormalA])))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 50}, "Position" -> {2, 2, 1}, 
        "Construct" -> {"SubstitutionLemma", 25}, "Orientation" -> -1, 
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
        "Side" -> 2|>|>, {"SubstitutionLemma", 32} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] \[FormalA] == 
         (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA])) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 31}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 30}, "Orientation" -> -1, 
        "Rule" -> (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalA] == (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalA])) \[CenterDot] 
            \[FormalA]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 51} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] \[FormalA])) \[CenterDot] 
           \[FormalA]) == ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           \[FormalA]) \[CenterDot] (((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA]) \[CenterDot] 
           (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] \[FormalA])))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 26}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (((\[FormalB]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
              (\[FormalB]_)))) -> \[FormalB] \[CenterDot] 
           ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalB]), 
        "Side" -> 1, "Subpattern" -> ((\[FormalB]_) \[CenterDot] 
           (\[FormalA]_)) \[CenterDot] (\[FormalB]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 32}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
             (\[FormalA]_))) \[CenterDot] (\[FormalA]_) -> 
          (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2, 2, 2}|>|>, 
    {"SubstitutionLemma", 33} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA]) == 
         ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           \[FormalA]) \[CenterDot] (((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA]) \[CenterDot] 
           (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] \[FormalA])))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 51}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 32}, "Orientation" -> -1, 
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
    {"SubstitutionLemma", 34} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA]) == 
         ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           \[FormalA]) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 33}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 30}, "Orientation" -> -1, 
        "Rule" -> (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] \[FormalA]) == 
           ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalA]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 52} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] (
                \[FormalA] \[CenterDot] \[FormalB]))) \[CenterDot] 
             \[FormalA])) \[CenterDot] \[FormalA]) == 
         ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           \[FormalA]) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 34}, 
        "Orientation" -> -1, "Rule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] (\[FormalA]_) -> 
          \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA]), "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 27}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
              ((\[FormalA]_) \[CenterDot] (\[FormalB]_)))) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA] \[CenterDot] \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {1, 1}|>|>, 
    {"SubstitutionLemma", 35} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
              \[FormalB]))) \[CenterDot] \[FormalA]) == 
         ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           \[FormalA]) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 52}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 32}, "Orientation" -> -1, 
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
    {"SubstitutionLemma", 36} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           \[FormalA]) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 35}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 27}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
              ((\[FormalA]_) \[CenterDot] (\[FormalB]_)))) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA] \[CenterDot] \[FormalB], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
           ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalA]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 37} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalB]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 36}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 34}, "Orientation" -> -1, 
        "Rule" -> (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] (\[FormalA]_) -> 
          \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA]), "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
           \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] \[FormalA])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 38} -> 
     <|"Statement" -> HoldForm[(((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
          (\[FormalA]_) -> \[FormalA] \[CenterDot] \[FormalB]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 34}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 37}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalA]_)) -> 
          \[FormalA] \[CenterDot] \[FormalB], "OutputExpression" -> 
         HoldForm[(((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] (\[FormalA]_) -> 
           \[FormalA] \[CenterDot] \[FormalB]], "ConstructSide" -> 1, 
        "InputOrientation" -> -1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 39} -> 
     <|"Statement" -> HoldForm[
        ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           \[FormalC]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalC]) == 
         \[FormalC]], "Proof" -> <|"Input" -> {"Axiom", 1}, 
        "Position" -> {2}, "Construct" -> {"SubstitutionLemma", 37}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA] \[CenterDot] \[FormalB], 
        "OutputExpression" -> HoldForm[
          ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
             \[FormalC]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalC]) == 
           \[FormalC]], "ConstructSide" -> 1, "InputOrientation" -> -1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 40} -> 
     <|"Statement" -> HoldForm[
        ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB]) == 
         \[FormalA]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 30}, 
        "Position" -> {2}, "Construct" -> {"SubstitutionLemma", 37}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA] \[CenterDot] \[FormalB], 
        "OutputExpression" -> HoldForm[
          ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
             \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB]) == 
           \[FormalA]], "ConstructSide" -> 1, "InputOrientation" -> -1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 41} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalB] \[CenterDot] \[FormalA]) == 
         \[FormalA]], "Proof" -> <|"Input" -> {"CriticalPairLemma", 47}, 
        "Position" -> {2}, "Construct" -> {"SubstitutionLemma", 37}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA] \[CenterDot] \[FormalB], 
        "OutputExpression" -> HoldForm[(\[FormalA] \[CenterDot] 
             \[FormalA]) \[CenterDot] (\[FormalB] \[CenterDot] \[FormalA]) == 
           \[FormalA]], "ConstructSide" -> 1, "InputOrientation" -> -1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 42} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalB] \[CenterDot] \[FormalB]) == 
         \[FormalB]], "Proof" -> <|"Input" -> {"CriticalPairLemma", 44}, 
        "Position" -> {1}, "Construct" -> {"SubstitutionLemma", 37}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA] \[CenterDot] \[FormalB], 
        "OutputExpression" -> HoldForm[(\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] (\[FormalB] \[CenterDot] \[FormalB]) == 
           \[FormalB]], "ConstructSide" -> 1, "InputOrientation" -> -1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 43} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalA])) == 
         \[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
            \[FormalA]) \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 26}, 
        "Position" -> {2, 2}, "Construct" -> {"SubstitutionLemma", 37}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA] \[CenterDot] \[FormalB], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] 
            (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalA])) == 
           \[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
              \[FormalA]) \[CenterDot] \[FormalB])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 44} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalA])) == 
         \[FormalB] \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 43}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 37}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalA]_)) -> 
          \[FormalA] \[CenterDot] \[FormalB], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalA])) == \[FormalB] \[CenterDot] 
            \[FormalA]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 45} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] ((\[FormalC] \[CenterDot] 
            \[FormalA]) \[CenterDot] \[FormalB]) == \[FormalB]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 4}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 37}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalA]_)) -> 
          \[FormalA] \[CenterDot] \[FormalB], "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            ((\[FormalC] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalB]) == 
           \[FormalB]], "ConstructSide" -> 1, "InputOrientation" -> -1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 46} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
              \[FormalC]) \[CenterDot] \[FormalB])) \[CenterDot] 
           (\[FormalC] \[CenterDot] ((\[FormalC] \[CenterDot] 
              \[FormalA]) \[CenterDot] \[FormalC]))) == 
         \[FormalC] \[CenterDot] ((\[FormalC] \[CenterDot] 
            \[FormalA]) \[CenterDot] \[FormalC])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 7}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 37}, "Orientation" -> -1, 
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
    {"SubstitutionLemma", 47} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           (\[FormalC] \[CenterDot] ((\[FormalC] \[CenterDot] 
              \[FormalA]) \[CenterDot] \[FormalC]))) == 
         \[FormalC] \[CenterDot] ((\[FormalC] \[CenterDot] 
            \[FormalA]) \[CenterDot] \[FormalC])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 46}, 
        "Position" -> {2, 1}, "Construct" -> {"SubstitutionLemma", 37}, 
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
    {"SubstitutionLemma", 48} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           (\[FormalC] \[CenterDot] \[FormalA])) == \[FormalC] \[CenterDot] 
          ((\[FormalC] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalC])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 47}, 
        "Position" -> {2, 2}, "Construct" -> {"SubstitutionLemma", 37}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA] \[CenterDot] \[FormalB], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] 
            ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
             (\[FormalC] \[CenterDot] \[FormalA])) == \[FormalC] \[CenterDot] 
            ((\[FormalC] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalC])], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 49} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           (\[FormalC] \[CenterDot] \[FormalA])) == \[FormalC] \[CenterDot] 
          \[FormalA]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 48}, 
        "Position" -> {}, "Construct" -> {"SubstitutionLemma", 37}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA] \[CenterDot] \[FormalB], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] 
            ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
             (\[FormalC] \[CenterDot] \[FormalA])) == \[FormalC] \[CenterDot] 
            \[FormalA]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 50} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           (\[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
              \[FormalA]) \[CenterDot] \[FormalB]))) == 
         \[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
            \[FormalA]) \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 1}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 37}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalA]_)) -> 
          \[FormalA] \[CenterDot] \[FormalB], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
              \[FormalC]) \[CenterDot] (\[FormalB] \[CenterDot] 
              ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
               \[FormalB]))) == \[FormalB] \[CenterDot] 
            ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalB])], 
        "ConstructSide" -> 1, "InputOrientation" -> -1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 51} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalA])) == \[FormalB] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 50}, 
        "Position" -> {2, 2}, "Construct" -> {"SubstitutionLemma", 37}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA] \[CenterDot] \[FormalB], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] 
            ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalA])) == \[FormalB] \[CenterDot] 
            ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalB])], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 52} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalA])) == \[FormalB] \[CenterDot] 
          \[FormalA]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 51}, 
        "Position" -> {}, "Construct" -> {"SubstitutionLemma", 37}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA] \[CenterDot] \[FormalB], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] 
            ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalA])) == \[FormalB] \[CenterDot] 
            \[FormalA]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 53} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] ((\[FormalC] \[CenterDot] 
             \[FormalB]) \[CenterDot] (((\[FormalC] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalA]) \[CenterDot] 
             (\[FormalC] \[CenterDot] \[FormalB])))) == 
         (\[FormalC] \[CenterDot] \[FormalB]) \[CenterDot] 
          (((\[FormalC] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalA]) \[CenterDot] (\[FormalC] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 10}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 37}, "Orientation" -> -1, 
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
    {"SubstitutionLemma", 54} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] ((\[FormalC] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA])) == 
         (\[FormalC] \[CenterDot] \[FormalB]) \[CenterDot] 
          (((\[FormalC] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalA]) \[CenterDot] (\[FormalC] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 53}, 
        "Position" -> {2, 2}, "Construct" -> {"SubstitutionLemma", 37}, 
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
        "Side" -> 1|>|>, {"SubstitutionLemma", 55} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] ((\[FormalC] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA])) == 
         (\[FormalC] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 54}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 37}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalA]_)) -> 
          \[FormalA] \[CenterDot] \[FormalB], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             ((\[FormalC] \[CenterDot] \[FormalB]) \[CenterDot] 
              \[FormalA])) == (\[FormalC] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 53} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 41}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA], "Side" -> 1, 
        "Subpattern" -> (\[FormalB]_) \[CenterDot] (\[FormalA]_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 38}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalA]_)) -> 
          \[FormalA] \[CenterDot] \[FormalB], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 54} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 42}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)) -> \[FormalB], "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 38}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalA]_)) -> 
          \[FormalA] \[CenterDot] \[FormalB], "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 55} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] \[FormalA] == \[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 44}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalB] \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          ((\[FormalB]_) \[CenterDot] (\[FormalA]_)), "MatchingConstruct" -> 
         {"SubstitutionLemma", 37}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA] \[CenterDot] \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 56} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB])) == 
         \[FormalA] \[CenterDot] \[FormalB]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 37}, "Position" -> {2}, 
        "Construct" -> {"CriticalPairLemma", 55}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalA]_) -> \[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
            \[FormalB]), "OutputExpression" -> HoldForm[
          \[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
             (\[FormalA] \[CenterDot] \[FormalB])) == \[FormalA] \[CenterDot] 
            \[FormalB]], "ConstructSide" -> 1, "InputOrientation" -> -1, 
        "Side" -> 1|>|>, {"CriticalPairLemma", 56} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CenterDot] 
           \[FormalA]) \[CenterDot] ((\[FormalB] \[CenterDot] 
            \[FormalC]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 39}, 
        "Orientation" -> 1, "Rule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalC]_)) -> \[FormalC], "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 54}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {1, 1}|>|>, 
    {"SubstitutionLemma", 57} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalB]) \[CenterDot] \[FormalA]) == \[FormalA]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 40}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 55}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalA]_) -> \[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
            \[FormalB]), "OutputExpression" -> HoldForm[
          (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA]) == 
           \[FormalA]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 58} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB])) == \[FormalA]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 57}, "Position" -> {2}, 
        "Construct" -> {"CriticalPairLemma", 55}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalA]_) -> \[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
            \[FormalB]), "OutputExpression" -> HoldForm[
          (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB])) == 
           \[FormalA]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"CriticalPairLemma", 57} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalB] \[CenterDot] ((\[FormalC] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalB])) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 45}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"SubstitutionLemma", 41}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 58} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalA] \[CenterDot] ((\[FormalC] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalA])) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 45}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"CriticalPairLemma", 53}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 59} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] \[FormalC] == 
         (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalC])) \[CenterDot] \[FormalC]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 56}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           (\[FormalC]_)) \[CenterDot] (\[FormalB]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 56}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 60} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalB]) \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 49}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalA]_))) -> 
          \[FormalC] \[CenterDot] \[FormalA], "Side" -> 1, 
        "Subpattern" -> ((\[FormalB]_) \[CenterDot] 
           (\[FormalC]_)) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
           (\[FormalA]_)), "MatchingConstruct" -> {"CriticalPairLemma", 55}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalA]_) -> \[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
            \[FormalB]), "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 59} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB])))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 60}, 
        "Position" -> {2, 2}, "Construct" -> {"CriticalPairLemma", 55}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (\[FormalA]_) -> 
          \[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB]), 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
           \[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
              (\[FormalA] \[CenterDot] \[FormalB])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 60} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalB] \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 59}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 58}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
           \[FormalB] \[CenterDot] \[FormalA]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 61} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] ((\[FormalC] \[CenterDot] 
            \[FormalA]) \[CenterDot] \[FormalB]) == 
         ((\[FormalC] \[CenterDot] \[FormalA]) \[CenterDot] 
           \[FormalB]) \[CenterDot] ((\[FormalAlpha] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB])) \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 49}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalA]_))) -> 
          \[FormalC] \[CenterDot] \[FormalA], "Side" -> 1, 
        "Subpattern" -> (\[FormalC]_) \[CenterDot] (\[FormalA]_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 45}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (((\[FormalC]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
            (\[FormalB]_)) -> \[FormalB], "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"SubstitutionLemma", 61} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           \[FormalA]) \[CenterDot] ((\[FormalAlpha] \[CenterDot] 
            (\[FormalC] \[CenterDot] \[FormalA])) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 61}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 45}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (((\[FormalC]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
            (\[FormalB]_)) -> \[FormalB], "OutputExpression" -> 
         HoldForm[\[FormalA] == ((\[FormalB] \[CenterDot] 
              \[FormalC]) \[CenterDot] \[FormalA]) \[CenterDot] 
            ((\[FormalAlpha] \[CenterDot] (\[FormalC] \[CenterDot] 
               \[FormalA])) \[CenterDot] \[FormalA])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 62} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalB] \[CenterDot] ((\[FormalC] \[CenterDot] 
            \[FormalA]) \[CenterDot] (\[FormalB] \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 49}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalA]_))) -> 
          \[FormalC] \[CenterDot] \[FormalA], "Side" -> 1, 
        "Subpattern" -> (\[FormalC]_) \[CenterDot] (\[FormalA]_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 60}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 63} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] ((\[FormalB] \[CenterDot] 
            \[FormalC]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 56}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"SubstitutionLemma", 60}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 64} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 56}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           (\[FormalC]_)) \[CenterDot] (\[FormalB]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 60}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 65} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] ((\[FormalC] \[CenterDot] 
            \[FormalB]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 45}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"SubstitutionLemma", 60}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 66} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalC] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 45}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> ((\[FormalC]_) \[CenterDot] 
           (\[FormalA]_)) \[CenterDot] (\[FormalB]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 60}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 67} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 63}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> ((\[FormalB]_) \[CenterDot] 
           (\[FormalC]_)) \[CenterDot] (\[FormalA]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 60}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 68} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalB]) \[CenterDot] ((\[FormalB] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalC]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 64}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalC]_))) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"SubstitutionLemma", 41}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 69} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB])) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 64}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalC]_))) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] (\[FormalC]_)), "MatchingConstruct" -> 
         {"CriticalPairLemma", 63}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 70} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalC]) == 
         ((\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalC])) \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 63}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> ((\[FormalB]_) \[CenterDot] 
           (\[FormalC]_)) \[CenterDot] (\[FormalA]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 64}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalC]_))) -> \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 71} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalB] \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalC])) == 
         (\[FormalB] \[CenterDot] (\[FormalA] \[CenterDot] 
            \[FormalC])) \[CenterDot] ((\[FormalAlpha] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB])) \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 49}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalA]_))) -> 
          \[FormalC] \[CenterDot] \[FormalA], "Side" -> 1, 
        "Subpattern" -> (\[FormalC]_) \[CenterDot] (\[FormalA]_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 64}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (\[FormalC]_))) -> \[FormalB], "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"SubstitutionLemma", 62} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
          ((\[FormalAlpha] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalA])) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 71}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 64}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (\[FormalC]_))) -> \[FormalB], "OutputExpression" -> 
         HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
            ((\[FormalAlpha] \[CenterDot] (\[FormalB] \[CenterDot] 
               \[FormalA])) \[CenterDot] \[FormalA])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 72} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalC] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 65}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> ((\[FormalC]_) \[CenterDot] 
           (\[FormalB]_)) \[CenterDot] (\[FormalA]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 60}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 73} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalB]) \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalB])))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 66}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalA]_))) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"SubstitutionLemma", 41}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 74} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalC]) == 
         ((\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalC])) \[CenterDot] \[FormalC]) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 63}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> ((\[FormalB]_) \[CenterDot] 
           (\[FormalC]_)) \[CenterDot] (\[FormalA]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 66}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalA]_))) -> \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 75} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalC]) == \[FormalA] \[CenterDot] 
          (((\[FormalC] \[CenterDot] \[FormalA]) \[CenterDot] 
            \[FormalAlpha]) \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalC])))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 56}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"CriticalPairLemma", 66}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalB], "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 76} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 67}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          ((\[FormalB]_) \[CenterDot] (\[FormalC]_)), "MatchingConstruct" -> 
         {"CriticalPairLemma", 63}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 77} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) == 
         (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
            \[FormalC])) \[CenterDot] ((\[FormalAlpha] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB])) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 49}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalA]_))) -> 
          \[FormalC] \[CenterDot] \[FormalA], "Side" -> 1, 
        "Subpattern" -> (\[FormalC]_) \[CenterDot] (\[FormalA]_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 67}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"SubstitutionLemma", 63} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
          ((\[FormalAlpha] \[CenterDot] (\[FormalA] \[CenterDot] 
             \[FormalB])) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 77}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 67}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
            ((\[FormalAlpha] \[CenterDot] (\[FormalA] \[CenterDot] 
               \[FormalB])) \[CenterDot] \[FormalA])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 78} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalC] \[CenterDot] \[FormalB])) == 
         (\[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] 
            \[FormalB])) \[CenterDot] (((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalAlpha]) \[CenterDot] 
           \[FormalA])], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 
          52}, "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalA]_))) -> 
          \[FormalB] \[CenterDot] \[FormalA], "Side" -> 1, 
        "Subpattern" -> (\[FormalB]_) \[CenterDot] (\[FormalA]_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 72}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"SubstitutionLemma", 64} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
          (((\[FormalA] \[CenterDot] \[FormalC]) \[CenterDot] 
            \[FormalAlpha]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 78}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 72}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
            (((\[FormalA] \[CenterDot] \[FormalC]) \[CenterDot] 
              \[FormalAlpha]) \[CenterDot] \[FormalA])], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 65} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
            \[FormalC]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 69}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 60}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
           \[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
              \[FormalC]) \[CenterDot] (\[FormalA] \[CenterDot] 
              \[FormalB]))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 66} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalB]) \[CenterDot] (\[FormalB] \[CenterDot] \[FormalC]))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 76}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 60}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
           \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] (\[FormalB] \[CenterDot] 
              \[FormalC]))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 79} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalA])) \[CenterDot] \[FormalB] == 
         \[FormalB] \[CenterDot] \[FormalB]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 55}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
              (\[FormalB]_)) \[CenterDot] (\[FormalA]_))) -> 
          (\[FormalC] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          (((\[FormalC]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalA]_)), "MatchingConstruct" -> {"CriticalPairLemma", 63}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 67} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB])) == 
         \[FormalA] \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 79}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 60}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             (\[FormalA] \[CenterDot] \[FormalB])) == \[FormalA] \[CenterDot] 
            \[FormalA]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"CriticalPairLemma", 80} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB]) == 
         (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
          ((\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
             \[FormalA])) \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 67}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalA] \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"CriticalPairLemma", 66}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalB], "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 81} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB]) == 
         (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
          ((\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalC])) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 67}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalA] \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"CriticalPairLemma", 67}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 82} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB]) == 
         (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
          ((\[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] 
             \[FormalB])) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 67}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalA] \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"CriticalPairLemma", 72}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 83} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
             \[FormalB]))) \[CenterDot] ((\[FormalC] \[CenterDot] 
            \[FormalC]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 65}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalC]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"SubstitutionLemma", 67}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalB]_))) -> 
          \[FormalA] \[CenterDot] \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2, 1}|>|>, {"SubstitutionLemma", 68} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] \[FormalC] == \[FormalC] \[CenterDot] 
          (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalC]))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 59}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 60}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalC] == \[FormalC] \[CenterDot] (\[FormalA] \[CenterDot] 
             ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
              \[FormalC]))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 69} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalC]) == \[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalC])) \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 70}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 60}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalC]) == \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
             \[FormalB])], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 70} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalC]) == \[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalC])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 69}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 60}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalC]) == \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
               \[FormalC])))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 84} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalA]) == \[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 70}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             ((\[FormalB]_) \[CenterDot] (\[FormalC]_)))) -> 
          \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalC]), 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalC]_))), "MatchingConstruct" -> {"SubstitutionLemma", 
          67}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalB]_))) -> 
          \[FormalA] \[CenterDot] \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 85} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalB]) == \[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 84}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) <-> 
          (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)), "Side" -> 1, "Subpattern" -> 
         (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 60}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 86} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 44}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalB] \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          ((\[FormalB]_) \[CenterDot] (\[FormalA]_)), "MatchingConstruct" -> 
         {"CriticalPairLemma", 84}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) <-> 
          (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)), "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 87} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalB])))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 72}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalB]_))) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalC]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"CriticalPairLemma", 84}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalA]_)) <-> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalB]_)), "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 88} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalA]) == 
         (\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 84}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) <-> 
          (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)), "Side" -> 2, "Subpattern" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
           (\[FormalB]_)), "MatchingConstruct" -> {"SubstitutionLemma", 60}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 89} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA]) == 
         \[FormalA] \[CenterDot] \[FormalB]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 84}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) <-> 
          (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)), "Side" -> 2, "Subpattern" -> 
         (\[FormalB]_) \[CenterDot] (\[FormalB]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 42}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)) -> \[FormalB], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 90} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] \[FormalB] == \[FormalB] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 86}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalB] \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"SubstitutionLemma", 42}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"CriticalPairLemma", 91} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
          (\[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalC]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 72}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalB]_))) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalC]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"CriticalPairLemma", 88}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalA]_)) <-> ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 92} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
           ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalC]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 66}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalA]_))) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalC]_) \[CenterDot] 
          (\[FormalA]_), "MatchingConstruct" -> {"CriticalPairLemma", 88}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalA]_)) <-> ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 93} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
          (((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalC]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 65}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalC]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"CriticalPairLemma", 88}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalA]_)) <-> ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2, 1}|>|>, {"CriticalPairLemma", 94} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           \[FormalA]) \[CenterDot] (((\[FormalB] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalC]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 45}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalC]_) \[CenterDot] 
          (\[FormalA]_), "MatchingConstruct" -> {"CriticalPairLemma", 88}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalA]_)) <-> ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2, 1}|>|>, {"SubstitutionLemma", 71} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalC]) == \[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalC])) \[CenterDot] \[FormalC])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 74}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 60}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalC]) == \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
             \[FormalC])], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 72} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalC]) == \[FormalA] \[CenterDot] 
          (\[FormalC] \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalC])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 71}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 60}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalC]) == \[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] 
             (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
               \[FormalC])))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 95} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalB] \[CenterDot] ((\[FormalC] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 57}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (\[FormalA]_))) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalC] \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] (\[FormalA]_)), "MatchingConstruct" -> 
         {"CriticalPairLemma", 84}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) <-> 
          (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)), "MatchingSide" -> 2, "Position" -> {2, 1}|>|>, 
    {"CriticalPairLemma", 96} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalA] \[CenterDot] ((\[FormalC] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalC])) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 58}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (\[FormalA]_))) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (\[FormalC]_))) -> \[FormalA] \[CenterDot] \[FormalC], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] (\[FormalA]_)), "MatchingConstruct" -> 
         {"CriticalPairLemma", 84}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) <-> 
          (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)), "MatchingSide" -> 2, "Position" -> {2, 1}|>|>, 
    {"SubstitutionLemma", 73} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalAlpha] \[CenterDot] (\[FormalC] \[CenterDot] 
             \[FormalA])))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 61}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 60}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == ((\[FormalB] \[CenterDot] 
              \[FormalC]) \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalA] \[CenterDot] (\[FormalAlpha] \[CenterDot] 
              (\[FormalC] \[CenterDot] \[FormalA])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 97} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
             \[FormalA]))) \[CenterDot] ((\[FormalAlpha] \[CenterDot] 
            \[FormalC]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 73}, 
        "Orientation" -> -1, "Rule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
            ((\[FormalAlpha]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
              (\[FormalC]_)))) -> \[FormalC], "Side" -> 1, 
        "Subpattern" -> (((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
          ((\[FormalC]_) \[CenterDot] ((\[FormalAlpha]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_)))), 
        "MatchingConstruct" -> {"SubstitutionLemma", 60}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"SubstitutionLemma", 74} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalAlpha] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalA])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 62}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 60}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
            (\[FormalA] \[CenterDot] (\[FormalAlpha] \[CenterDot] 
              (\[FormalB] \[CenterDot] \[FormalA])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 98} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
             \[FormalA]))) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalC] \[CenterDot] \[FormalAlpha]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 74}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalAlpha]_) \[CenterDot] 
             ((\[FormalB]_) \[CenterDot] (\[FormalA]_)))) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] ((\[FormalAlpha]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalA]_)))), 
        "MatchingConstruct" -> {"SubstitutionLemma", 60}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"SubstitutionLemma", 75} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalAlpha] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 63}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 60}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
            (\[FormalA] \[CenterDot] (\[FormalAlpha] \[CenterDot] 
              (\[FormalA] \[CenterDot] \[FormalB])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 76} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
          (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalC]) \[CenterDot] \[FormalAlpha]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 64}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 60}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
            (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalC]) \[CenterDot] \[FormalAlpha]))], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 99} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalC])) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalAlpha] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 76}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
              (\[FormalC]_)) \[CenterDot] (\[FormalAlpha]_))) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] (\[FormalAlpha]_))), 
        "MatchingConstruct" -> {"SubstitutionLemma", 60}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 100} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalB])) == 
         ((\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalB]))) \[CenterDot] 
           ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalA])) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 87}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             ((\[FormalB]_) \[CenterDot] (\[FormalB]_)))) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          ((\[FormalC]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_))), "MatchingConstruct" -> {"CriticalPairLemma", 
          87}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
              (\[FormalB]_)))) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 77} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalB])) == 
         \[FormalA] \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 100}, "Position" -> {1}, 
        "Construct" -> {"CriticalPairLemma", 83}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] (\[FormalB]_)))) \[CenterDot] 
           (((\[FormalC]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalB])) == \[FormalA] \[CenterDot] 
            \[FormalA]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 101} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
            \[FormalB])) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 77}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalB]_))) <-> (\[FormalA]_) \[CenterDot] (\[FormalA]_), 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          ((\[FormalB]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_))), "MatchingConstruct" -> {"SubstitutionLemma", 
          60}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 102} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] (\[FormalA] \[CenterDot] 
             \[FormalB]))) \[CenterDot] (\[FormalC] \[CenterDot] 
           (\[FormalC] \[CenterDot] \[FormalC])) == \[FormalA]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 77}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalB]_))) <-> (\[FormalA]_) \[CenterDot] (\[FormalA]_), 
        "Side" -> 2, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalA]_), "MatchingConstruct" -> {"SubstitutionLemma", 75}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalAlpha]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (\[FormalB]_)))) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"SubstitutionLemma", 78} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalB] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalB])) == \[FormalA]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 102}, "Position" -> {1}, 
        "Construct" -> {"SubstitutionLemma", 67}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalB]_))) -> 
          \[FormalA] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalB])) == 
           \[FormalA]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"CriticalPairLemma", 103} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalB]) == 
         (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalC] \[CenterDot] \[FormalC]))) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 90}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalA]_)), "Side" -> 1, "Subpattern" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalA]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 77}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalB]_))) <-> (\[FormalA]_) \[CenterDot] (\[FormalA]_), 
        "MatchingSide" -> 2, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 104} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalB])) \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 78}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalB]_))) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           (\[FormalA]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalB]_))), 
        "MatchingConstruct" -> {"SubstitutionLemma", 60}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 105} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
           (\[FormalC] \[CenterDot] (\[FormalC] \[CenterDot] \[FormalC]))) == 
         \[FormalA] \[CenterDot] ((\[FormalC] \[CenterDot] 
            (\[FormalC] \[CenterDot] \[FormalC])) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 72}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] (\[FormalB]_)))) -> 
          \[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] \[FormalB]), 
        "Side" -> 1, "Subpattern" -> (\[FormalC]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"SubstitutionLemma", 78}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2, 2, 2}|>|>, {"SubstitutionLemma", 79} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalA] \[CenterDot] ((\[FormalC] \[CenterDot] 
            (\[FormalC] \[CenterDot] \[FormalC])) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 105}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 78}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
           \[FormalA] \[CenterDot] ((\[FormalC] \[CenterDot] 
              (\[FormalC] \[CenterDot] \[FormalC])) \[CenterDot] 
             (\[FormalA] \[CenterDot] \[FormalB]))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 106} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalB])) \[CenterDot] 
          ((\[FormalC] \[CenterDot] (\[FormalC] \[CenterDot] 
             \[FormalC])) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 104}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"CriticalPairLemma", 101}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalA]_) <-> 
          ((\[FormalB]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalB]_))) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 107} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalA]) == 
         ((\[FormalC] \[CenterDot] (\[FormalC] \[CenterDot] 
             \[FormalC])) \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 88}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) <-> 
          ((\[FormalB]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalA]_), "Side" -> 2, "Subpattern" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalA]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 101}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] (\[FormalA]_) <-> 
          ((\[FormalB]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalB]_))) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 108} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalA]) == \[FormalA] \[CenterDot] 
          ((\[FormalC] \[CenterDot] (\[FormalC] \[CenterDot] 
             \[FormalC])) \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 84}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) <-> 
          (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)), "Side" -> 2, "Subpattern" -> 
         (\[FormalB]_) \[CenterDot] (\[FormalB]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 101}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] (\[FormalA]_) <-> 
          ((\[FormalB]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalB]_))) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 109} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] ((\[FormalC] \[CenterDot] 
              ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
               \[FormalC])) \[CenterDot] (\[FormalAlpha] \[CenterDot] 
              (\[FormalB] \[CenterDot] \[FormalB]))))) \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalAlpha] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalB])))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 91}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
              (\[FormalB]_)) \[CenterDot] (\[FormalC]_))) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> ((\[FormalB]_) \[CenterDot] 
           (\[FormalB]_)) \[CenterDot] (\[FormalC]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 95}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (\[FormalB]_))) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalC] \[CenterDot] \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"SubstitutionLemma", 80} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] ((\[FormalC] \[CenterDot] 
              \[FormalB]) \[CenterDot] (\[FormalAlpha] \[CenterDot] 
              (\[FormalB] \[CenterDot] \[FormalB]))))) \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalAlpha] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalB])))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 109}, 
        "Position" -> {1, 2, 2, 1}, "Construct" -> {"CriticalPairLemma", 89}, 
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
        "Side" -> 2|>|>, {"SubstitutionLemma", 81} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalAlpha] \[CenterDot] 
            (\[FormalC] \[CenterDot] \[FormalC])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 80}, 
        "Position" -> {1, 2}, "Construct" -> {"CriticalPairLemma", 73}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (\[FormalA]_)))) -> \[FormalB] \[CenterDot] \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalA] == 
           (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
              \[FormalC])) \[CenterDot] (\[FormalA] \[CenterDot] 
             (\[FormalAlpha] \[CenterDot] (\[FormalC] \[CenterDot] 
               \[FormalC])))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 110} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalB] \[CenterDot] ((\[FormalC] \[CenterDot] 
              ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
               \[FormalC])) \[CenterDot] (\[FormalAlpha] \[CenterDot] 
              (\[FormalB] \[CenterDot] \[FormalB])))) \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalAlpha] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalB])))], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 92}, "Orientation" -> -1, 
        "Rule" -> (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalC], "Side" -> 1, 
        "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           (\[FormalA]_)) \[CenterDot] (\[FormalB]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 95}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (\[FormalB]_))) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalC] \[CenterDot] \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"SubstitutionLemma", 82} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalB] \[CenterDot] ((\[FormalC] \[CenterDot] 
              \[FormalB]) \[CenterDot] (\[FormalAlpha] \[CenterDot] 
              (\[FormalB] \[CenterDot] \[FormalB])))) \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalAlpha] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalB])))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 110}, "Position" -> {1, 1, 2, 1}, 
        "Construct" -> {"CriticalPairLemma", 89}, "Orientation" -> 1, 
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
    {"SubstitutionLemma", 83} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalAlpha] \[CenterDot] (\[FormalC] \[CenterDot] 
             \[FormalC])))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 82}, "Position" -> {1, 1}, 
        "Construct" -> {"CriticalPairLemma", 73}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             ((\[FormalA]_) \[CenterDot] (\[FormalA]_)))) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == ((\[FormalB] \[CenterDot] 
              \[FormalC]) \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalA] \[CenterDot] (\[FormalAlpha] \[CenterDot] 
              (\[FormalC] \[CenterDot] \[FormalC])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 111} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalB] \[CenterDot] ((\[FormalC] \[CenterDot] 
              ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
               \[FormalC])) \[CenterDot] ((\[FormalB] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalAlpha]))) \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
           ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalAlpha]))], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 92}, "Orientation" -> -1, 
        "Rule" -> (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalC], "Side" -> 1, 
        "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           (\[FormalA]_)) \[CenterDot] (\[FormalB]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 96}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (\[FormalB]_))) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (\[FormalC]_))) -> \[FormalA] \[CenterDot] \[FormalC], 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"SubstitutionLemma", 84} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalB] \[CenterDot] ((\[FormalC] \[CenterDot] 
              \[FormalB]) \[CenterDot] ((\[FormalB] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalAlpha]))) \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
           ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalAlpha]))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 111}, "Position" -> {1, 1, 2, 1}, 
        "Construct" -> {"CriticalPairLemma", 89}, "Orientation" -> 1, 
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
    {"SubstitutionLemma", 85} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
           ((\[FormalC] \[CenterDot] \[FormalC]) \[CenterDot] 
            \[FormalAlpha]))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 84}, "Position" -> {1, 1}, 
        "Construct" -> {"CriticalPairLemma", 68}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalC]_))) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == ((\[FormalB] \[CenterDot] 
              \[FormalC]) \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalA] \[CenterDot] ((\[FormalC] \[CenterDot] 
               \[FormalC]) \[CenterDot] \[FormalAlpha]))], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 112} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] ((\[FormalC] \[CenterDot] 
              ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
               \[FormalC])) \[CenterDot] (\[FormalAlpha] \[CenterDot] 
              (\[FormalB] \[CenterDot] \[FormalB]))))) \[CenterDot] 
          ((\[FormalAlpha] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalB])) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 93}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) \[CenterDot] 
           ((((\[FormalB]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> ((\[FormalB]_) \[CenterDot] 
           (\[FormalB]_)) \[CenterDot] (\[FormalC]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 95}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (\[FormalB]_))) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalC] \[CenterDot] \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"SubstitutionLemma", 86} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] ((\[FormalC] \[CenterDot] 
              \[FormalB]) \[CenterDot] (\[FormalAlpha] \[CenterDot] 
              (\[FormalB] \[CenterDot] \[FormalB]))))) \[CenterDot] 
          ((\[FormalAlpha] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalB])) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 112}, 
        "Position" -> {1, 2, 2, 1}, "Construct" -> {"CriticalPairLemma", 89}, 
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
        "Side" -> 2|>|>, {"SubstitutionLemma", 87} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
          ((\[FormalAlpha] \[CenterDot] (\[FormalC] \[CenterDot] 
             \[FormalC])) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 86}, 
        "Position" -> {1, 2}, "Construct" -> {"CriticalPairLemma", 73}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (\[FormalA]_)))) -> \[FormalB] \[CenterDot] \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalA] == 
           (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
              \[FormalC])) \[CenterDot] ((\[FormalAlpha] \[CenterDot] 
              (\[FormalC] \[CenterDot] \[FormalC])) \[CenterDot] 
             \[FormalA])], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 113} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalB] \[CenterDot] ((\[FormalC] \[CenterDot] 
              ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
               \[FormalC])) \[CenterDot] (\[FormalAlpha] \[CenterDot] 
              (\[FormalB] \[CenterDot] \[FormalB])))) \[CenterDot] 
           \[FormalA]) \[CenterDot] ((\[FormalAlpha] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalB])) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 94}, 
        "Orientation" -> -1, "Rule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] ((((\[FormalA]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) -> \[FormalC], "Side" -> 1, 
        "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           (\[FormalA]_)) \[CenterDot] (\[FormalB]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 95}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (\[FormalB]_))) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalC] \[CenterDot] \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"SubstitutionLemma", 88} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalB] \[CenterDot] ((\[FormalC] \[CenterDot] 
              \[FormalB]) \[CenterDot] (\[FormalAlpha] \[CenterDot] 
              (\[FormalB] \[CenterDot] \[FormalB])))) \[CenterDot] 
           \[FormalA]) \[CenterDot] ((\[FormalAlpha] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalB])) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 113}, 
        "Position" -> {1, 1, 2, 1}, "Construct" -> {"CriticalPairLemma", 89}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA] \[CenterDot] \[FormalB], 
        "OutputExpression" -> HoldForm[\[FormalA] == 
           ((\[FormalB] \[CenterDot] ((\[FormalC] \[CenterDot] 
                \[FormalB]) \[CenterDot] (\[FormalAlpha] \[CenterDot] 
                (\[FormalB] \[CenterDot] \[FormalB])))) \[CenterDot] 
             \[FormalA]) \[CenterDot] ((\[FormalAlpha] \[CenterDot] 
              (\[FormalB] \[CenterDot] \[FormalB])) \[CenterDot] 
             \[FormalA])], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 89} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           \[FormalA]) \[CenterDot] ((\[FormalAlpha] \[CenterDot] 
            (\[FormalC] \[CenterDot] \[FormalC])) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 88}, 
        "Position" -> {1, 1}, "Construct" -> {"CriticalPairLemma", 73}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (\[FormalA]_)))) -> \[FormalB] \[CenterDot] \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalA] == 
           ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
             \[FormalA]) \[CenterDot] ((\[FormalAlpha] \[CenterDot] 
              (\[FormalC] \[CenterDot] \[FormalC])) \[CenterDot] 
             \[FormalA])], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 114} -> 
     <|"Statement" -> HoldForm[
        ((\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             (\[FormalC] \[CenterDot] \[FormalA]))) \[CenterDot] 
           \[FormalC]) \[CenterDot] \[FormalA] == \[FormalA] \[CenterDot] 
          \[FormalA]], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 68}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
              (\[FormalC]_)) \[CenterDot] (\[FormalA]_))) -> 
          (\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          (((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
           (\[FormalA]_)), "MatchingConstruct" -> {"CriticalPairLemma", 97}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] (\[FormalA]_)))) \[CenterDot] 
           (((\[FormalAlpha]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 90} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             (\[FormalC] \[CenterDot] \[FormalA]))) \[CenterDot] 
           \[FormalC]) == \[FormalA] \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 114}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 60}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
                \[FormalA]))) \[CenterDot] \[FormalC]) == 
           \[FormalA] \[CenterDot] \[FormalA]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 91} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] 
              \[FormalA])))) == \[FormalA] \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 90}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 60}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             (\[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] (
                \[FormalB] \[CenterDot] \[FormalA])))) == 
           \[FormalA] \[CenterDot] \[FormalA]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 115} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB]))) == \[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 70}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             ((\[FormalB]_) \[CenterDot] (\[FormalC]_)))) -> 
          \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalC]), 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalC]_))), "MatchingConstruct" -> {"SubstitutionLemma", 
          91}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
              ((\[FormalB]_) \[CenterDot] (\[FormalA]_))))) -> 
          \[FormalA] \[CenterDot] \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 116} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalB]) == \[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalC]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 115}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             ((\[FormalA]_) \[CenterDot] (\[FormalB]_)))) -> 
          \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalB]), 
        "Side" -> 1, "Subpattern" -> (\[FormalC]_) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] (\[FormalB]_)), "MatchingConstruct" -> 
         {"SubstitutionLemma", 60}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 117} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalB])) == \[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
           (\[FormalC] \[CenterDot] (\[FormalA] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalA]))))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 115}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             ((\[FormalA]_) \[CenterDot] (\[FormalB]_)))) -> 
          \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalB]), 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"CriticalPairLemma", 84}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalA]_)) <-> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalB]_)), "MatchingSide" -> 2, 
        "Position" -> {2, 2, 2}|>|>, {"SubstitutionLemma", 92} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
            \[FormalB]) \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
              \[FormalA]))))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 117}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 42}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
           \[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
              \[FormalB]) \[CenterDot] (\[FormalC] \[CenterDot] 
              (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
                \[FormalA]))))], "ConstructSide" -> 1, "InputOrientation" -> 
         1, "Side" -> 1|>|>, {"CriticalPairLemma", 118} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalB]) == \[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalC]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 116}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
              (\[FormalB]_)) \[CenterDot] (\[FormalC]_))) -> 
          \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalB]), 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"SubstitutionLemma", 60}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2, 2, 1}|>|>, {"CriticalPairLemma", 119} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalC])) \[CenterDot] (\[FormalC] \[CenterDot] 
            \[FormalC])) == \[FormalA] \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 70}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             ((\[FormalB]_) \[CenterDot] (\[FormalC]_)))) -> 
          \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalC]), 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalC]_))), "MatchingConstruct" -> {"SubstitutionLemma", 
          81}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalAlpha]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
              (\[FormalC]_)))) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 120} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
            (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
              \[FormalC]))) \[CenterDot] ((\[FormalC] \[CenterDot] 
             \[FormalC]) \[CenterDot] (\[FormalC] \[CenterDot] 
             \[FormalC])))], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 119}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalC]_))) -> 
          \[FormalA] \[CenterDot] \[FormalA], "Side" -> 1, 
        "Subpattern" -> (\[FormalB]_) \[CenterDot] (\[FormalC]_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 85}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)) <-> (\[FormalA]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalB]_)), "MatchingSide" -> 1, 
        "Position" -> {2, 1, 2}|>|>, {"SubstitutionLemma", 93} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
            (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
              \[FormalC]))) \[CenterDot] \[FormalC])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 120}, 
        "Position" -> {2, 2}, "Construct" -> {"SubstitutionLemma", 42}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)) -> \[FormalB], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
           \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
                \[FormalC]))) \[CenterDot] \[FormalC])], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 94} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
           (\[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] 
             (\[FormalC] \[CenterDot] \[FormalB]))))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 93}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 60}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
           \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             (\[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] (
                \[FormalC] \[CenterDot] \[FormalB]))))], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 121} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
           (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
             (\[FormalA] \[CenterDot] \[FormalC]))))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 94}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] (
                \[FormalB]_))))) -> \[FormalA] \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
              (\[FormalB]_))))), "MatchingConstruct" -> {"CriticalPairLemma", 
          96}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             ((\[FormalA]_) \[CenterDot] (\[FormalB]_))) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalC]_))) -> 
          \[FormalA] \[CenterDot] \[FormalC], "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 122} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalC] \[CenterDot] \[FormalA]))) == \[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 70}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             ((\[FormalB]_) \[CenterDot] (\[FormalC]_)))) -> 
          \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalC]), 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalC]_))), "MatchingConstruct" -> {"SubstitutionLemma", 
          94}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
              ((\[FormalC]_) \[CenterDot] (\[FormalB]_))))) -> 
          \[FormalA] \[CenterDot] \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 123} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         \[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
            (\[FormalC] \[CenterDot] (\[FormalA] \[CenterDot] 
              \[FormalB]))) \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 121}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] (
                \[FormalC]_))))) -> \[FormalA] \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          ((\[FormalC]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalC]_))), "MatchingConstruct" -> {"CriticalPairLemma", 
          98}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] (\[FormalA]_)))) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalAlpha]_))) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 124} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalA]) == 
         (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
            (\[FormalC] \[CenterDot] \[FormalA])))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 121}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] (
                \[FormalC]_))))) -> \[FormalA] \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalC]_) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] (\[FormalC]_)), "MatchingConstruct" -> 
         {"CriticalPairLemma", 89}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA] \[CenterDot] \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {2, 2, 2}|>|>, 
    {"SubstitutionLemma", 95} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalB] \[CenterDot] 
           (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] \[FormalA])))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 124}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 42}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "OutputExpression" -> HoldForm[\[FormalA] == 
           (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
              (\[FormalC] \[CenterDot] \[FormalA])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 125} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalA]) == 
         (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
            (\[FormalC] \[CenterDot] (\[FormalC] \[CenterDot] 
              (\[FormalA] \[CenterDot] \[FormalC])))))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 121}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] (
                \[FormalC]_))))) -> \[FormalA] \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalC]_), "MatchingConstruct" -> {"CriticalPairLemma", 88}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalA]_)) <-> ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 2, 
        "Position" -> {2, 2, 2, 2}|>|>, {"SubstitutionLemma", 96} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalB] \[CenterDot] 
           (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
             (\[FormalC] \[CenterDot] (\[FormalA] \[CenterDot] 
               \[FormalC])))))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 125}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 42}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "OutputExpression" -> HoldForm[\[FormalA] == 
           (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
              (\[FormalC] \[CenterDot] (\[FormalC] \[CenterDot] 
                (\[FormalA] \[CenterDot] \[FormalC])))))], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 97} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalB] \[CenterDot] 
           (\[FormalB] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalC])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 96}, 
        "Position" -> {2, 2, 2}, "Construct" -> {"SubstitutionLemma", 44}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalB] \[CenterDot] \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalA] == 
           (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
              (\[FormalA] \[CenterDot] \[FormalC])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 126} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalC])) == 
         ((\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalC]))) \[CenterDot] 
           \[FormalC]) \[CenterDot] \[FormalC]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 65}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> ((\[FormalC]_) \[CenterDot] 
           (\[FormalB]_)) \[CenterDot] (\[FormalA]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 95}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
              (\[FormalA]_)))) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 127} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalB] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalC]) \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 97}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (\[FormalC]_)))) -> \[FormalA], "Side" -> 1, 
        "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] (\[FormalC]_)), "MatchingConstruct" -> 
         {"SubstitutionLemma", 68}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
              (\[FormalC]_)) \[CenterDot] (\[FormalA]_))) -> 
          (\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"SubstitutionLemma", 98} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
           (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
             (\[FormalA] \[CenterDot] \[FormalB]))))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 123}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 60}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
           \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] (
                \[FormalA] \[CenterDot] \[FormalB]))))], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 128} -> 
     <|"Statement" -> HoldForm[
        ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB])) \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalC]) == 
         (\[FormalAlpha] \[CenterDot] (\[FormalAlpha] \[CenterDot] 
            \[FormalAlpha])) \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 106}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
              (\[FormalB]_))) \[CenterDot] (\[FormalC]_)) -> \[FormalC], 
        "Side" -> 1, "Subpattern" -> ((\[FormalB]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalB]_))) \[CenterDot] 
          (\[FormalC]_), "MatchingConstruct" -> {"SubstitutionLemma", 85}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
            (((\[FormalB]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
             (\[FormalAlpha]_))) -> \[FormalC], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 99} -> 
     <|"Statement" -> HoldForm[
        ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB])) \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalC]) == 
         \[FormalA] \[CenterDot] \[FormalB]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 128}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 104}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (\[FormalA]_))) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)) -> \[FormalB], "OutputExpression" -> 
         HoldForm[((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
             (\[FormalA] \[CenterDot] \[FormalB])) \[CenterDot] 
            ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalC]) == 
           \[FormalA] \[CenterDot] \[FormalB]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 129} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
             \[FormalAlpha]))) \[CenterDot] ((\[FormalAlpha] \[CenterDot] 
            \[FormalC]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 87}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) \[CenterDot] 
           (((\[FormalAlpha]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
              (\[FormalC]_))) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalAlpha]_) \[CenterDot] 
          ((\[FormalC]_) \[CenterDot] (\[FormalC]_)), "MatchingConstruct" -> 
         {"CriticalPairLemma", 62}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalC]_))) -> 
          \[FormalC] \[CenterDot] \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2, 1}|>|>, {"CriticalPairLemma", 130} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
             \[FormalAlpha])) \[CenterDot] \[FormalA]) \[CenterDot] 
          ((\[FormalAlpha] \[CenterDot] \[FormalC]) \[CenterDot] 
           \[FormalA])], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 
          89}, "Orientation" -> -1, "Rule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] (((\[FormalAlpha]_) \[CenterDot] 
             ((\[FormalB]_) \[CenterDot] (\[FormalB]_))) \[CenterDot] 
            (\[FormalC]_)) -> \[FormalC], "Side" -> 1, 
        "Subpattern" -> (\[FormalAlpha]_) \[CenterDot] 
          ((\[FormalB]_) \[CenterDot] (\[FormalB]_)), "MatchingConstruct" -> 
         {"CriticalPairLemma", 62}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalC]_))) -> 
          \[FormalC] \[CenterDot] \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2, 1}|>|>, {"SubstitutionLemma", 100} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalAlpha] \[CenterDot] (\[FormalC] \[CenterDot] 
             \[FormalB])))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 129}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 60}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == ((\[FormalB] \[CenterDot] 
              \[FormalC]) \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalA] \[CenterDot] (\[FormalAlpha] \[CenterDot] 
              (\[FormalC] \[CenterDot] \[FormalB])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 101} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           \[FormalA]) \[CenterDot] ((\[FormalAlpha] \[CenterDot] 
            (\[FormalC] \[CenterDot] \[FormalB])) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 130}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 60}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == ((\[FormalB] \[CenterDot] 
              \[FormalC]) \[CenterDot] \[FormalA]) \[CenterDot] 
            ((\[FormalAlpha] \[CenterDot] (\[FormalC] \[CenterDot] 
               \[FormalB])) \[CenterDot] \[FormalA])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 131} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
             \[FormalC]) \[CenterDot] (\[FormalC] \[CenterDot] 
             \[FormalB]))) == (\[FormalC] \[CenterDot] 
           \[FormalB]) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 103}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) <-> 
          ((\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] (\[FormalC]_)))) \[CenterDot] 
           (\[FormalA]_), "Side" -> 2, "Subpattern" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalB]_))), 
        "MatchingConstruct" -> {"SubstitutionLemma", 100}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
            ((\[FormalAlpha]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
              (\[FormalA]_)))) -> \[FormalC], "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 132} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] \[FormalC] == \[FormalC] \[CenterDot] 
          (((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB])) \[CenterDot] \[FormalC])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 131}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
              (\[FormalC]_)) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
              (\[FormalB]_)))) -> (\[FormalC] \[CenterDot] 
            \[FormalB]) \[CenterDot] \[FormalA], "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
           ((\[FormalC]_) \[CenterDot] (\[FormalB]_))), 
        "MatchingConstruct" -> {"SubstitutionLemma", 60}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 133} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] \[FormalC] == \[FormalC] \[CenterDot] 
          (((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalA])) \[CenterDot] \[FormalC])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 132}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] (\[FormalB]_))) \[CenterDot] 
            (\[FormalA]_)) -> (\[FormalC] \[CenterDot] 
            \[FormalB]) \[CenterDot] \[FormalA], "Side" -> 1, 
        "Subpattern" -> (\[FormalC]_) \[CenterDot] (\[FormalB]_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 60}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2, 1, 2}|>|>, {"SubstitutionLemma", 102} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] \[FormalC] == \[FormalC] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 133}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 89}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalA]_)) -> 
          \[FormalA] \[CenterDot] \[FormalB], "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalC] == \[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalA])], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 134} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] \[FormalC]) == 
         (\[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] 
            \[FormalB])) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 102}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (\[FormalC]_) <-> 
          (\[FormalC]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalA]_)), "Side" -> 1, "Subpattern" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 84}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) <-> 
          (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)), "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 135} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalC]) == 
         (\[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] 
            \[FormalC])) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 102}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (\[FormalC]_) <-> 
          (\[FormalC]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalA]_)), "Side" -> 1, "Subpattern" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 84}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) <-> 
          (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)), "MatchingSide" -> 2, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 136} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] \[FormalC])) == 
         (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
            \[FormalB])) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 102}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (\[FormalC]_) <-> 
          (\[FormalC]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalA]_)), "Side" -> 1, "Subpattern" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 88}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) <-> 
          ((\[FormalB]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalA]_), "MatchingSide" -> 2, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 137} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] \[FormalC] == 
         (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
          ((\[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalA])) \[CenterDot] (\[FormalC] \[CenterDot] 
            \[FormalAlpha]))], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 66}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) -> \[FormalA] \[CenterDot] \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"SubstitutionLemma", 102}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalC]_) <-> (\[FormalC]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)), "MatchingSide" -> 1, 
        "Position" -> {2, 1}|>|>, {"CriticalPairLemma", 138} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
          \[FormalAlpha] == \[FormalAlpha] \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 102}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (\[FormalC]_) <-> 
          (\[FormalC]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalA]_)), "Side" -> 2, "Subpattern" -> 
         (\[FormalB]_) \[CenterDot] (\[FormalC]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 102}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (\[FormalC]_) <-> 
          (\[FormalC]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalA]_)), "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 139} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalC] \[CenterDot] \[FormalA]))) == \[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 122}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] (\[FormalA]_)))) <-> 
          (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)), "Side" -> 2, "Subpattern" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
           (\[FormalB]_)), "MatchingConstruct" -> {"CriticalPairLemma", 85}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)) <-> (\[FormalA]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalB]_)), "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 140} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalA]) \[CenterDot] ((\[FormalB] \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalC])) == 
         (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalC]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 139}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] (\[FormalA]_)))) -> 
          \[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB]), 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          ((\[FormalC]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
            (\[FormalA]_))), "MatchingConstruct" -> {"SubstitutionLemma", 
          83}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
            ((\[FormalAlpha]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
              (\[FormalB]_)))) -> \[FormalC], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 141} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalB]) == \[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalC]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 139}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] (\[FormalA]_)))) -> 
          \[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB]), 
        "Side" -> 1, "Subpattern" -> (\[FormalC]_) \[CenterDot] 
          ((\[FormalC]_) \[CenterDot] (\[FormalA]_)), "MatchingConstruct" -> 
         {"CriticalPairLemma", 90}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalA]_)), "MatchingSide" -> 2, "Position" -> {2, 2}|>|>, 
    {"SubstitutionLemma", 103} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalC])) == 
         (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
            \[FormalC])) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 134}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 102}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalC]_) -> \[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] 
            \[FormalA]), "OutputExpression" -> HoldForm[
          \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalC])) == 
           (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
              \[FormalC])) \[CenterDot] \[FormalA]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 142} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
            \[FormalAlpha])) == ((\[FormalAlpha] \[CenterDot] 
            \[FormalC]) \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 138}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) \[CenterDot] 
           (\[FormalAlpha]_) <-> (\[FormalAlpha]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalB]_))), "Side" -> 1, "Subpattern" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
           (\[FormalC]_)), "MatchingConstruct" -> {"SubstitutionLemma", 60}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 143} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
          \[FormalAlpha] == \[FormalAlpha] \[CenterDot] 
          ((\[FormalC] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 138}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) \[CenterDot] 
           (\[FormalAlpha]_) <-> (\[FormalAlpha]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalB]_))), "Side" -> 2, "Subpattern" -> 
         (\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
           (\[FormalAlpha]_)), "MatchingConstruct" -> {"SubstitutionLemma", 
          60}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 104} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB]) == 
         (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
          (\[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalC]) \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 80}, "Position" -> {2}, 
        "Construct" -> {"CriticalPairLemma", 143}, "Orientation" -> 1, 
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
    {"SubstitutionLemma", 105} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB]) == 
         (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
          (\[FormalA] \[CenterDot] ((\[FormalC] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 81}, "Position" -> {2}, 
        "Construct" -> {"CriticalPairLemma", 143}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] (\[FormalAlpha]_) -> 
          \[FormalAlpha] \[CenterDot] ((\[FormalC] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA]), "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB]) == 
           (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            (\[FormalA] \[CenterDot] ((\[FormalC] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalA]))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 106} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB]) == 
         (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
          (\[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
             \[FormalC]) \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 82}, "Position" -> {2}, 
        "Construct" -> {"CriticalPairLemma", 143}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] (\[FormalAlpha]_) -> 
          \[FormalAlpha] \[CenterDot] ((\[FormalC] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA]), "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB]) == 
           (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            (\[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
               \[FormalC]) \[CenterDot] \[FormalA]))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 107} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalC])) == 
         \[FormalC] \[CenterDot] (\[FormalC] \[CenterDot] 
           ((\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
              \[FormalC])) \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 126}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 142}, "Orientation" -> 1, 
        "Rule" -> (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] (\[FormalAlpha]_) -> 
          \[FormalAlpha] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalA])), "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalC])) == \[FormalC] \[CenterDot] 
            (\[FormalC] \[CenterDot] ((\[FormalA] \[CenterDot] (
                \[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
              \[FormalA]))], "ConstructSide" -> 2, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 108} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalC])) == 
         \[FormalC] \[CenterDot] (\[FormalC] \[CenterDot] 
           (\[FormalA] \[CenterDot] ((\[FormalC] \[CenterDot] 
              \[FormalB]) \[CenterDot] \[FormalA])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 107}, 
        "Position" -> {2, 2}, "Construct" -> {"CriticalPairLemma", 143}, 
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
    {"CriticalPairLemma", 144} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalB]) == \[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalC] \[CenterDot] (\[FormalAlpha] \[CenterDot] 
              (\[FormalA] \[CenterDot] \[FormalA])))))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 141}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalC]_))) -> 
          \[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB]), 
        "Side" -> 1, "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           (\[FormalA]_)) \[CenterDot] (\[FormalC]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 108}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (((\[FormalA]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
              (\[FormalB]_)))) -> \[FormalB] \[CenterDot] 
           (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] \[FormalA])), 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"CriticalPairLemma", 145} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] \[FormalB] == 
         (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
           (\[FormalC] \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 92}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              ((\[FormalB]_) \[CenterDot] (\[FormalA]_))))) -> 
          \[FormalA] \[CenterDot] \[FormalB], "Side" -> 1, 
        "Subpattern" -> (\[FormalC]_) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalA]_))), "MatchingConstruct" -> {"SubstitutionLemma", 
          99}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalB]_))) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) -> \[FormalA] \[CenterDot] \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"CriticalPairLemma", 146} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] \[FormalB] == 
         (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
          ((\[FormalC] \[CenterDot] \[FormalA]) \[CenterDot] 
           ((\[FormalC] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 145}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_))) -> (\[FormalA] \[CenterDot] 
            \[FormalA]) \[CenterDot] \[FormalB], "Side" -> 1, 
        "Subpattern" -> ((\[FormalB]_) \[CenterDot] 
           (\[FormalB]_)) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
           (\[FormalA]_)), "MatchingConstruct" -> {"CriticalPairLemma", 90}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           (\[FormalB]_) <-> (\[FormalB]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 109} -> 
     <|"Statement" -> HoldForm[
        ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           (\[FormalB] \[CenterDot] (\[FormalA] \[CenterDot] 
             \[FormalB]))) \[CenterDot] (((\[FormalC] \[CenterDot] 
             \[FormalA]) \[CenterDot] (((\[FormalC] \[CenterDot] 
               \[FormalA]) \[CenterDot] \[FormalB]) \[CenterDot] 
             (\[FormalC] \[CenterDot] \[FormalA]))) \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            (\[FormalAlpha] \[CenterDot] ((\[FormalAlpha] \[CenterDot] (
                \[FormalA] \[CenterDot] \[FormalB])) \[CenterDot] 
              \[FormalAlpha])))) == (\[FormalC] \[CenterDot] 
           \[FormalA]) \[CenterDot] (((\[FormalC] \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalB]) \[CenterDot] 
           (\[FormalC] \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 15}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 142}, "Orientation" -> 1, 
        "Rule" -> (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] (\[FormalAlpha]_) -> 
          \[FormalAlpha] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalA])), "OutputExpression" -> 
         HoldForm[((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
             (\[FormalB] \[CenterDot] (\[FormalA] \[CenterDot] 
               \[FormalB]))) \[CenterDot] (((\[FormalC] \[CenterDot] 
               \[FormalA]) \[CenterDot] (((\[FormalC] \[CenterDot] 
                 \[FormalA]) \[CenterDot] \[FormalB]) \[CenterDot] (
                \[FormalC] \[CenterDot] \[FormalA]))) \[CenterDot] 
             ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
              (\[FormalAlpha] \[CenterDot] ((\[FormalAlpha] \[CenterDot] 
                 (\[FormalA] \[CenterDot] \[FormalB])) \[CenterDot] 
                \[FormalAlpha])))) == (\[FormalC] \[CenterDot] 
             \[FormalA]) \[CenterDot] (((\[FormalC] \[CenterDot] 
               \[FormalA]) \[CenterDot] \[FormalB]) \[CenterDot] 
             (\[FormalC] \[CenterDot] \[FormalA]))], "ConstructSide" -> 2, 
        "InputOrientation" -> -1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 110} -> 
     <|"Statement" -> HoldForm[
        (((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            (((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
              \[FormalC]) \[CenterDot] (\[FormalA] \[CenterDot] 
              \[FormalB]))) \[CenterDot] ((\[FormalB] \[CenterDot] 
             \[FormalC]) \[CenterDot] (\[FormalAlpha] \[CenterDot] 
             ((\[FormalAlpha] \[CenterDot] (\[FormalB] \[CenterDot] 
                \[FormalC])) \[CenterDot] \[FormalAlpha])))) \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           (\[FormalC] \[CenterDot] \[FormalC])) == 
         (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
          (((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalC]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 109}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 136}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalA]_))) \[CenterDot] (\[FormalC]_) -> 
          \[FormalC] \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalB])), "OutputExpression" -> 
         HoldForm[(((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
              (((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
                \[FormalC]) \[CenterDot] (\[FormalA] \[CenterDot] 
                \[FormalB]))) \[CenterDot] ((\[FormalB] \[CenterDot] 
               \[FormalC]) \[CenterDot] (\[FormalAlpha] \[CenterDot] (
                (\[FormalAlpha] \[CenterDot] (\[FormalB] \[CenterDot] 
                  \[FormalC])) \[CenterDot] \[FormalAlpha])))) \[CenterDot] 
            ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
             (\[FormalC] \[CenterDot] \[FormalC])) == 
           (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            (((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
              \[FormalC]) \[CenterDot] (\[FormalA] \[CenterDot] 
              \[FormalB]))], "ConstructSide" -> 2, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 111} -> 
     <|"Statement" -> HoldForm[
        ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalB])) \[CenterDot] 
          (((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            (\[FormalC] \[CenterDot] ((\[FormalC] \[CenterDot] (
                \[FormalA] \[CenterDot] \[FormalB])) \[CenterDot] 
              \[FormalC]))) \[CenterDot] 
           ((((\[FormalAlpha] \[CenterDot] \[FormalA]) \[CenterDot] 
              \[FormalB]) \[CenterDot] (\[FormalAlpha] \[CenterDot] 
              \[FormalA])) \[CenterDot] (\[FormalAlpha] \[CenterDot] 
             \[FormalA]))) == (\[FormalAlpha] \[CenterDot] 
           \[FormalA]) \[CenterDot] (((\[FormalAlpha] \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalB]) \[CenterDot] 
           (\[FormalAlpha] \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 110}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 142}, "Orientation" -> 1, 
        "Rule" -> (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] (\[FormalAlpha]_) -> 
          \[FormalAlpha] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalA])), "OutputExpression" -> 
         HoldForm[((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalB])) \[CenterDot] 
            (((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
              (\[FormalC] \[CenterDot] ((\[FormalC] \[CenterDot] 
                 (\[FormalA] \[CenterDot] \[FormalB])) \[CenterDot] 
                \[FormalC]))) \[CenterDot] ((((\[FormalAlpha] \[CenterDot] 
                 \[FormalA]) \[CenterDot] \[FormalB]) \[CenterDot] (
                \[FormalAlpha] \[CenterDot] \[FormalA])) \[CenterDot] 
              (\[FormalAlpha] \[CenterDot] \[FormalA]))) == 
           (\[FormalAlpha] \[CenterDot] \[FormalA]) \[CenterDot] 
            (((\[FormalAlpha] \[CenterDot] \[FormalA]) \[CenterDot] 
              \[FormalB]) \[CenterDot] (\[FormalAlpha] \[CenterDot] 
              \[FormalA]))], "ConstructSide" -> 2, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 112} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalC] \[CenterDot] ((\[FormalC] \[CenterDot] (
                \[FormalB] \[CenterDot] \[FormalA])) \[CenterDot] 
              \[FormalC]))) \[CenterDot] 
           ((((\[FormalAlpha] \[CenterDot] \[FormalB]) \[CenterDot] 
              \[FormalA]) \[CenterDot] (\[FormalAlpha] \[CenterDot] 
              \[FormalB])) \[CenterDot] (\[FormalAlpha] \[CenterDot] 
             \[FormalB]))) == (\[FormalAlpha] \[CenterDot] 
           \[FormalB]) \[CenterDot] (((\[FormalAlpha] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA]) \[CenterDot] 
           (\[FormalAlpha] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 111}, "Position" -> {1}, 
        "Construct" -> {"SubstitutionLemma", 42}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] 
            (((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
              (\[FormalC] \[CenterDot] ((\[FormalC] \[CenterDot] 
                 (\[FormalB] \[CenterDot] \[FormalA])) \[CenterDot] 
                \[FormalC]))) \[CenterDot] ((((\[FormalAlpha] \[CenterDot] 
                 \[FormalB]) \[CenterDot] \[FormalA]) \[CenterDot] (
                \[FormalAlpha] \[CenterDot] \[FormalB])) \[CenterDot] 
              (\[FormalAlpha] \[CenterDot] \[FormalB]))) == 
           (\[FormalAlpha] \[CenterDot] \[FormalB]) \[CenterDot] 
            (((\[FormalAlpha] \[CenterDot] \[FormalB]) \[CenterDot] 
              \[FormalA]) \[CenterDot] (\[FormalAlpha] \[CenterDot] 
              \[FormalB]))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 113} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalC] \[CenterDot] \[FormalC])) \[CenterDot] 
           ((((\[FormalAlpha] \[CenterDot] \[FormalB]) \[CenterDot] 
              \[FormalA]) \[CenterDot] (\[FormalAlpha] \[CenterDot] 
              \[FormalB])) \[CenterDot] (\[FormalAlpha] \[CenterDot] 
             \[FormalB]))) == (\[FormalAlpha] \[CenterDot] 
           \[FormalB]) \[CenterDot] (((\[FormalAlpha] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA]) \[CenterDot] 
           (\[FormalAlpha] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 112}, 
        "Position" -> {2, 1}, "Construct" -> {"CriticalPairLemma", 118}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalC]_))) -> 
          \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalB]), 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] 
            (((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
              (\[FormalC] \[CenterDot] \[FormalC])) \[CenterDot] 
             ((((\[FormalAlpha] \[CenterDot] \[FormalB]) \[CenterDot] 
                \[FormalA]) \[CenterDot] (\[FormalAlpha] \[CenterDot] 
                \[FormalB])) \[CenterDot] (\[FormalAlpha] \[CenterDot] 
               \[FormalB]))) == (\[FormalAlpha] \[CenterDot] 
             \[FormalB]) \[CenterDot] (((\[FormalAlpha] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalA]) \[CenterDot] 
             (\[FormalAlpha] \[CenterDot] \[FormalB]))], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 114} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalC] \[CenterDot] \[FormalC])) \[CenterDot] 
           ((\[FormalAlpha] \[CenterDot] \[FormalB]) \[CenterDot] 
            ((\[FormalAlpha] \[CenterDot] \[FormalB]) \[CenterDot] 
             (\[FormalA] \[CenterDot] (\[FormalAlpha] \[CenterDot] 
               \[FormalB]))))) == (\[FormalAlpha] \[CenterDot] 
           \[FormalB]) \[CenterDot] (((\[FormalAlpha] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA]) \[CenterDot] 
           (\[FormalAlpha] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 113}, 
        "Position" -> {2, 2}, "Construct" -> {"CriticalPairLemma", 142}, 
        "Orientation" -> 1, "Rule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] (\[FormalAlpha]_) -> 
          \[FormalAlpha] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalA])), "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (((\[FormalB] \[CenterDot] 
               \[FormalA]) \[CenterDot] (\[FormalC] \[CenterDot] 
               \[FormalC])) \[CenterDot] ((\[FormalAlpha] \[CenterDot] 
               \[FormalB]) \[CenterDot] ((\[FormalAlpha] \[CenterDot] 
                \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
                (\[FormalAlpha] \[CenterDot] \[FormalB]))))) == 
           (\[FormalAlpha] \[CenterDot] \[FormalB]) \[CenterDot] 
            (((\[FormalAlpha] \[CenterDot] \[FormalB]) \[CenterDot] 
              \[FormalA]) \[CenterDot] (\[FormalAlpha] \[CenterDot] 
              \[FormalB]))], "ConstructSide" -> 2, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 115} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalC] \[CenterDot] \[FormalC])) \[CenterDot] 
           (\[FormalA] \[CenterDot] (\[FormalAlpha] \[CenterDot] 
             \[FormalB]))) == (\[FormalAlpha] \[CenterDot] 
           \[FormalB]) \[CenterDot] (((\[FormalAlpha] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA]) \[CenterDot] 
           (\[FormalAlpha] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 114}, 
        "Position" -> {2, 2}, "Construct" -> {"SubstitutionLemma", 44}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalB] \[CenterDot] \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] 
            (((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
              (\[FormalC] \[CenterDot] \[FormalC])) \[CenterDot] 
             (\[FormalA] \[CenterDot] (\[FormalAlpha] \[CenterDot] 
               \[FormalB]))) == (\[FormalAlpha] \[CenterDot] 
             \[FormalB]) \[CenterDot] (((\[FormalAlpha] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalA]) \[CenterDot] 
             (\[FormalAlpha] \[CenterDot] \[FormalB]))], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 116} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalC]) == 
         (\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
          (((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
            \[FormalA]) \[CenterDot] (\[FormalB] \[CenterDot] \[FormalC]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 115}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 75}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] ((((\[FormalB]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] ((\[FormalAlpha]_) \[CenterDot] 
              (\[FormalB]_)))) -> \[FormalA] \[CenterDot] 
           (\[FormalAlpha] \[CenterDot] \[FormalB]), "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalC]) == (\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
            (((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
              \[FormalA]) \[CenterDot] (\[FormalB] \[CenterDot] 
              \[FormalC]))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 117} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalC]) == 
         (\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           (\[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] \[FormalB])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 116}, "Position" -> {2}, 
        "Construct" -> {"CriticalPairLemma", 142}, "Orientation" -> 1, 
        "Rule" -> (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] (\[FormalAlpha]_) -> 
          \[FormalAlpha] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalA])), "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalC]) == (\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
            ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
             (\[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] 
               \[FormalB])))], "ConstructSide" -> 2, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 147} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] ((\[FormalB] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalC])) \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalAlpha])) == 
         (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
          ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
           ((\[FormalC] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 140}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalC]_))) -> 
          (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalC], 
        "Side" -> 1, "Subpattern" -> ((\[FormalB]_) \[CenterDot] 
           (\[FormalA]_)) \[CenterDot] (\[FormalC]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 137}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             ((\[FormalB]_) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalAlpha]_))) -> 
          (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalC], 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"SubstitutionLemma", 118} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] ((\[FormalB] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalC])) \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalAlpha])) == 
         (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalB]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 147}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 140}, "Orientation" -> 1, 
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
    {"CriticalPairLemma", 148} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalB]) == \[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] ((\[FormalC] \[CenterDot] 
             ((\[FormalC] \[CenterDot] \[FormalA]) \[CenterDot] 
              \[FormalAlpha])) \[CenterDot] \[FormalC]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 144}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] ((\[FormalAlpha]_) \[CenterDot] (
                (\[FormalA]_) \[CenterDot] (\[FormalA]_)))))) -> 
          \[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB]), 
        "Side" -> 1, "Subpattern" -> (\[FormalC]_) \[CenterDot] 
          ((\[FormalAlpha]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_))), "MatchingConstruct" -> {"CriticalPairLemma", 
          99}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
              (\[FormalB]_)) \[CenterDot] (\[FormalC]_))) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalAlpha]_) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2, 2, 2}|>|>, {"SubstitutionLemma", 119} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalC])) \[CenterDot] ((\[FormalF] \[CenterDot] 
            (\[FormalF] \[CenterDot] \[FormalF])) \[CenterDot] 
           ((\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalC])) \[CenterDot] 
            (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] (
                (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
                \[FormalC])) \[CenterDot] \[FormalA])))) == 
         (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
          (((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] (
                (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
                \[FormalC])) \[CenterDot] \[FormalA]))) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 108}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA] \[CenterDot] 
           ((\[FormalF] \[CenterDot] (\[FormalF] \[CenterDot] 
              \[FormalF])) \[CenterDot] \[FormalB]), "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalC])) \[CenterDot] 
            ((\[FormalF] \[CenterDot] (\[FormalF] \[CenterDot] 
               \[FormalF])) \[CenterDot] ((\[FormalA] \[CenterDot] (
                (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
                \[FormalC])) \[CenterDot] (\[FormalA] \[CenterDot] (
                (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
                   \[FormalB]) \[CenterDot] \[FormalC])) \[CenterDot] 
                \[FormalA])))) == (\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] (((\[FormalA] \[CenterDot] 
               \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] (
                (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
                   \[FormalB]) \[CenterDot] \[FormalC])) \[CenterDot] 
                \[FormalA]))) \[CenterDot] (\[FormalA] \[CenterDot] 
              \[FormalB]))], "ConstructSide" -> 1, "InputOrientation" -> -1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 120} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalC])) \[CenterDot] (\[FormalA] \[CenterDot] 
           ((\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalC])) \[CenterDot] 
            \[FormalA])) == (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
          (((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] (
                (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
                \[FormalC])) \[CenterDot] \[FormalA]))) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 119}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 79}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             ((\[FormalB]_) \[CenterDot] (\[FormalB]_))) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalC]_))) -> 
          \[FormalA] \[CenterDot] \[FormalC], "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalC])) \[CenterDot] 
            (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] (
                (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
                \[FormalC])) \[CenterDot] \[FormalA])) == 
           (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            (((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
              (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
                 ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
                  \[FormalC])) \[CenterDot] \[FormalA]))) \[CenterDot] 
             (\[FormalA] \[CenterDot] \[FormalB]))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 121} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalC])) \[CenterDot] (\[FormalA] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalC])) == 
         (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
          (((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] (
                (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
                \[FormalC])) \[CenterDot] \[FormalA]))) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 120}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 105}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
              (\[FormalB]_)) \[CenterDot] (\[FormalA]_))) -> 
          (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB]), "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalC])) \[CenterDot] 
            (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalC])) == 
           (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            (((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
              (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
                 ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
                  \[FormalC])) \[CenterDot] \[FormalA]))) \[CenterDot] 
             (\[FormalA] \[CenterDot] \[FormalB]))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 122} -> 
     <|"Statement" -> HoldForm[(\[FormalF] \[CenterDot] 
           (\[FormalF] \[CenterDot] \[FormalF])) \[CenterDot] 
          (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalC])) == 
         (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
          (((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] (
                (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
                \[FormalC])) \[CenterDot] \[FormalA]))) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 121}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 101}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalA]_) -> 
          (\[FormalF] \[CenterDot] (\[FormalF] \[CenterDot] 
             \[FormalF])) \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalF] \[CenterDot] (\[FormalF] \[CenterDot] 
              \[FormalF])) \[CenterDot] (\[FormalA] \[CenterDot] 
             ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
              \[FormalC])) == (\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] (((\[FormalA] \[CenterDot] 
               \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] (
                (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
                   \[FormalB]) \[CenterDot] \[FormalC])) \[CenterDot] 
                \[FormalA]))) \[CenterDot] (\[FormalA] \[CenterDot] 
              \[FormalB]))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 123} -> 
     <|"Statement" -> HoldForm[(\[FormalF] \[CenterDot] 
           (\[FormalF] \[CenterDot] \[FormalF])) \[CenterDot] 
          (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalC])) == 
         (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
          ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           ((\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] (
                (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
                \[FormalC])) \[CenterDot] \[FormalA])) \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalA])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 122}, "Position" -> {2}, 
        "Construct" -> {"CriticalPairLemma", 142}, "Orientation" -> 1, 
        "Rule" -> (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] (\[FormalAlpha]_) -> 
          \[FormalAlpha] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalA])), "OutputExpression" -> 
         HoldForm[(\[FormalF] \[CenterDot] (\[FormalF] \[CenterDot] 
              \[FormalF])) \[CenterDot] (\[FormalA] \[CenterDot] 
             ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
              \[FormalC])) == (\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] ((\[FormalA] \[CenterDot] (
                (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
                   \[FormalB]) \[CenterDot] \[FormalC])) \[CenterDot] 
                \[FormalA])) \[CenterDot] (\[FormalB] \[CenterDot] 
               \[FormalA])))], "ConstructSide" -> 2, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 124} -> 
     <|"Statement" -> HoldForm[(\[FormalF] \[CenterDot] 
           (\[FormalF] \[CenterDot] \[FormalF])) \[CenterDot] 
          (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalC])) == 
         (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
             ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
              \[FormalC])) \[CenterDot] \[FormalA])) \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 123}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 117}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
              (\[FormalA]_)))) -> \[FormalC] \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB]), "OutputExpression" -> 
         HoldForm[(\[FormalF] \[CenterDot] (\[FormalF] \[CenterDot] 
              \[FormalF])) \[CenterDot] (\[FormalA] \[CenterDot] 
             ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
              \[FormalC])) == (\[FormalA] \[CenterDot] 
             ((\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
                 \[FormalB]) \[CenterDot] \[FormalC])) \[CenterDot] 
              \[FormalA])) \[CenterDot] (\[FormalA] \[CenterDot] 
             \[FormalB])], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 125} -> 
     <|"Statement" -> HoldForm[(\[FormalF] \[CenterDot] 
           (\[FormalF] \[CenterDot] \[FormalF])) \[CenterDot] 
          (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalC])) == 
         (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
          ((\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
             ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
              \[FormalC]))) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 124}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 143}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] (\[FormalAlpha]_) -> 
          \[FormalAlpha] \[CenterDot] ((\[FormalC] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA]), "OutputExpression" -> 
         HoldForm[(\[FormalF] \[CenterDot] (\[FormalF] \[CenterDot] 
              \[FormalF])) \[CenterDot] (\[FormalA] \[CenterDot] 
             ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
              \[FormalC])) == (\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] ((\[FormalA] \[CenterDot] 
              (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
                 \[FormalB]) \[CenterDot] \[FormalC]))) \[CenterDot] 
             \[FormalA])], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 126} -> 
     <|"Statement" -> HoldForm[(\[FormalF] \[CenterDot] 
           (\[FormalF] \[CenterDot] \[FormalF])) \[CenterDot] 
          (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalC])) == 
         (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
          (\[FormalA] \[CenterDot] ((((\[FormalA] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalC]) \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 125}, "Position" -> {2}, 
        "Construct" -> {"CriticalPairLemma", 143}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] (\[FormalAlpha]_) -> 
          \[FormalAlpha] \[CenterDot] ((\[FormalC] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA]), "OutputExpression" -> 
         HoldForm[(\[FormalF] \[CenterDot] (\[FormalF] \[CenterDot] 
              \[FormalF])) \[CenterDot] (\[FormalA] \[CenterDot] 
             ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
              \[FormalC])) == (\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
             ((((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
                \[FormalC]) \[CenterDot] \[FormalA]) \[CenterDot] 
              \[FormalA]))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 127} -> 
     <|"Statement" -> HoldForm[(\[FormalF] \[CenterDot] 
           (\[FormalF] \[CenterDot] \[FormalF])) \[CenterDot] 
          (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalC])) == 
         (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] 
              (\[FormalA] \[CenterDot] \[FormalB])))))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 126}, 
        "Position" -> {2, 2}, "Construct" -> {"CriticalPairLemma", 142}, 
        "Orientation" -> 1, "Rule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] (\[FormalAlpha]_) -> 
          \[FormalAlpha] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalA])), "OutputExpression" -> 
         HoldForm[(\[FormalF] \[CenterDot] (\[FormalF] \[CenterDot] 
              \[FormalF])) \[CenterDot] (\[FormalA] \[CenterDot] 
             ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
              \[FormalC])) == (\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
             (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] (
                \[FormalC] \[CenterDot] (\[FormalA] \[CenterDot] 
                 \[FormalB])))))], "ConstructSide" -> 2, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 128} -> 
     <|"Statement" -> HoldForm[(\[FormalF] \[CenterDot] 
           (\[FormalF] \[CenterDot] \[FormalF])) \[CenterDot] 
          (\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalC])) == 
         (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 127}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 56}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalB]_))) -> 
          \[FormalA] \[CenterDot] \[FormalB], "OutputExpression" -> 
         HoldForm[(\[FormalF] \[CenterDot] (\[FormalF] \[CenterDot] 
              \[FormalF])) \[CenterDot] (\[FormalA] \[CenterDot] 
             ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
              \[FormalC])) == (\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
             (\[FormalC] \[CenterDot] (\[FormalA] \[CenterDot] 
               \[FormalB])))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 149} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
           ((\[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
               \[FormalC]) \[CenterDot] \[FormalA])) \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB]))) == 
         (\[FormalF] \[CenterDot] (\[FormalF] \[CenterDot] 
            \[FormalF])) \[CenterDot] (\[FormalA] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB])))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 128}, 
        "Orientation" -> 1, "Rule" -> (\[FormalF] \[CenterDot] 
            (\[FormalF] \[CenterDot] \[FormalF])) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
              (\[FormalB]_)) \[CenterDot] (\[FormalC]_))) <-> 
          ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             ((\[FormalA]_) \[CenterDot] (\[FormalB]_)))), "Side" -> 1, 
        "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           (\[FormalB]_)) \[CenterDot] (\[FormalC]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 106}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
             (\[FormalA]_))) -> (\[FormalA] \[CenterDot] 
            \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB]), 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"SubstitutionLemma", 129} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
            \[FormalA])) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB]) == 
         (\[FormalF] \[CenterDot] (\[FormalF] \[CenterDot] 
            \[FormalF])) \[CenterDot] (\[FormalA] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB])))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 149}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 68}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
             (\[FormalA]_))) -> (\[FormalB] \[CenterDot] 
            \[FormalC]) \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
               \[FormalC]) \[CenterDot] \[FormalA])) \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB]) == 
           (\[FormalF] \[CenterDot] (\[FormalF] \[CenterDot] 
              \[FormalF])) \[CenterDot] (\[FormalA] \[CenterDot] 
             ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
              (\[FormalA] \[CenterDot] \[FormalB])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 130} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
            \[FormalA])) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB]) == 
         (\[FormalF] \[CenterDot] (\[FormalF] \[CenterDot] 
            \[FormalF])) \[CenterDot] (\[FormalB] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 129}, "Position" -> {2}, 
        "Construct" -> {"CriticalPairLemma", 62}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (\[FormalC]_))) -> \[FormalC] \[CenterDot] \[FormalA], 
        "OutputExpression" -> HoldForm[(\[FormalA] \[CenterDot] 
             ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
              \[FormalA])) \[CenterDot] (\[FormalA] \[CenterDot] 
             \[FormalB]) == (\[FormalF] \[CenterDot] (\[FormalF] \[CenterDot] 
              \[FormalF])) \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalA])], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 131} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] ((\[FormalA] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] \[FormalA]) == 
         (\[FormalF] \[CenterDot] (\[FormalF] \[CenterDot] 
            \[FormalF])) \[CenterDot] (\[FormalB] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 130}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 143}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] (\[FormalAlpha]_) -> 
          \[FormalAlpha] \[CenterDot] ((\[FormalC] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA]), "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            ((\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
               \[FormalC])) \[CenterDot] \[FormalA]) == 
           (\[FormalF] \[CenterDot] (\[FormalF] \[CenterDot] 
              \[FormalF])) \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalA])], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 132} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
           ((\[FormalC] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA])) == 
         (\[FormalF] \[CenterDot] (\[FormalF] \[CenterDot] 
            \[FormalF])) \[CenterDot] (\[FormalB] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 131}, "Position" -> {2}, 
        "Construct" -> {"CriticalPairLemma", 143}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] (\[FormalAlpha]_) -> 
          \[FormalAlpha] \[CenterDot] ((\[FormalC] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA]), "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            (\[FormalA] \[CenterDot] ((\[FormalC] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalA])) == 
           (\[FormalF] \[CenterDot] (\[FormalF] \[CenterDot] 
              \[FormalF])) \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalA])], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 133} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB]) == 
         (\[FormalF] \[CenterDot] (\[FormalF] \[CenterDot] 
            \[FormalF])) \[CenterDot] (\[FormalB] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 132}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 105}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
              (\[FormalB]_)) \[CenterDot] (\[FormalA]_))) -> 
          (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB]), "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB]) == 
           (\[FormalF] \[CenterDot] (\[FormalF] \[CenterDot] 
              \[FormalF])) \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalA])], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 134} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalB]) == \[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
            ((\[FormalAlpha] \[CenterDot] (\[FormalC] \[CenterDot] 
               \[FormalA])) \[CenterDot] \[FormalC])))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 148}, 
        "Position" -> {2, 2}, "Construct" -> {"CriticalPairLemma", 143}, 
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
    {"SubstitutionLemma", 135} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalB]) == \[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalC] \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalC]) \[CenterDot] \[FormalAlpha]))))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 134}, 
        "Position" -> {2, 2, 2}, "Construct" -> {"CriticalPairLemma", 143}, 
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
    {"CriticalPairLemma", 150} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalB])) == 
         \[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
            \[FormalB]) \[CenterDot] ((\[FormalA] \[CenterDot] 
             (\[FormalC] \[CenterDot] \[FormalB])) \[CenterDot] 
            \[FormalAlpha]))], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 135}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
              (((\[FormalA]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] (
                \[FormalAlpha]_))))) -> \[FormalA] \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB]), "Side" -> 1, 
        "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          ((\[FormalC]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
             (\[FormalAlpha]_)))), "MatchingConstruct" -> 
         {"CriticalPairLemma", 146}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalC]_))) -> 
          (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalC], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 136} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalA]) \[CenterDot] ((\[FormalB] \[CenterDot] 
             (\[FormalC] \[CenterDot] \[FormalA])) \[CenterDot] 
            \[FormalAlpha]))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 150}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 86}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalB]_))) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
           \[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalA]) \[CenterDot] ((\[FormalB] \[CenterDot] (
                \[FormalC] \[CenterDot] \[FormalA])) \[CenterDot] 
              \[FormalAlpha]))], "ConstructSide" -> 1, "InputOrientation" -> 
         1, "Side" -> 1|>|>, {"CriticalPairLemma", 151} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalB] \[CenterDot] ((\[FormalC] \[CenterDot] 
            (\[FormalB] \[CenterDot] (\[FormalAlpha] \[CenterDot] 
              \[FormalA]))) \[CenterDot] (\[FormalA] \[CenterDot] 
            \[FormalA]))], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 
          136}, "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (((\[FormalA]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] (
                \[FormalB]_))) \[CenterDot] (\[FormalAlpha]_))) -> 
          \[FormalB] \[CenterDot] \[FormalA], "Side" -> 1, 
        "Subpattern" -> ((\[FormalB]_) \[CenterDot] 
           (\[FormalB]_)) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalB]_))) \[CenterDot] 
           (\[FormalAlpha]_)), "MatchingConstruct" -> {"SubstitutionLemma", 
          55}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (((\[FormalC]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
             (\[FormalA]_))) -> (\[FormalC] \[CenterDot] 
            \[FormalB]) \[CenterDot] \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 152} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] \[FormalC] == 
         \[FormalC] \[CenterDot] ((\[FormalB] \[CenterDot] 
            \[FormalB]) \[CenterDot] ((\[FormalA] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
            (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
              \[FormalC]))))], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 151}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             ((\[FormalA]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] (
                \[FormalAlpha]_)))) \[CenterDot] 
            ((\[FormalAlpha]_) \[CenterDot] (\[FormalAlpha]_))) -> 
          \[FormalAlpha] \[CenterDot] \[FormalA], "Side" -> 1, 
        "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
            (\[FormalAlpha]_))), "MatchingConstruct" -> {"SubstitutionLemma", 
          98}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
              ((\[FormalA]_) \[CenterDot] (\[FormalB]_))))) -> 
          \[FormalA] \[CenterDot] \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2, 1}|>|>, {"SubstitutionLemma", 137} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] \[FormalC] == 
         \[FormalC] \[CenterDot] ((\[FormalB] \[CenterDot] 
            \[FormalB]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 152}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 118}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (\[FormalC]_))) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalAlpha]_))) -> (\[FormalA] \[CenterDot] 
            \[FormalA]) \[CenterDot] \[FormalB], "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
              \[FormalC])) \[CenterDot] \[FormalC] == \[FormalC] \[CenterDot] 
            ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA])], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 138} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalC]) == 
         \[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
            \[FormalB]) \[CenterDot] \[FormalC])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 137}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 143}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] (\[FormalAlpha]_) -> 
          \[FormalAlpha] \[CenterDot] ((\[FormalC] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA]), "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] \[FormalC]) == \[FormalA] \[CenterDot] 
            ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalC])], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 153} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalB])) \[CenterDot] \[FormalC]) == 
         \[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] 
           (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB])))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 138}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) <-> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)), "Side" -> 1, "Subpattern" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
          (\[FormalC]_), "MatchingConstruct" -> {"SubstitutionLemma", 103}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) <-> 
          ((\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 2, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 139} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalC]) == \[FormalA] \[CenterDot] 
          (\[FormalC] \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB])))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 153}, 
        "Position" -> {2, 1}, "Construct" -> {"SubstitutionLemma", 42}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)) -> \[FormalB], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalC]) == \[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] 
             (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
               \[FormalB])))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"CriticalPairLemma", 154} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB])) \[CenterDot] 
           \[FormalAlpha]) == \[FormalA] \[CenterDot] 
          (\[FormalAlpha] \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB])))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 139}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             ((\[FormalA]_) \[CenterDot] (\[FormalC]_)))) -> 
          \[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] \[FormalB]), 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalC]_), "MatchingConstruct" -> {"SubstitutionLemma", 65}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalA] \[CenterDot] \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {2, 2, 2}|>|>, 
    {"SubstitutionLemma", 140} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB])) \[CenterDot] 
           \[FormalAlpha]) == \[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalAlpha])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 154}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 139}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (\[FormalC]_)))) -> \[FormalA] \[CenterDot] 
           (\[FormalC] \[CenterDot] \[FormalB]), "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (((\[FormalB] \[CenterDot] 
               \[FormalC]) \[CenterDot] (\[FormalA] \[CenterDot] 
               \[FormalB])) \[CenterDot] \[FormalAlpha]) == 
           \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalAlpha])], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 155} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalC]) == \[FormalA] \[CenterDot] 
          (\[FormalC] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalC]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 140}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
             ((\[FormalA]_) \[CenterDot] (\[FormalB]_))) \[CenterDot] 
            (\[FormalAlpha]_)) -> \[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalAlpha]), "Side" -> 1, 
        "Subpattern" -> (((\[FormalB]_) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_))) \[CenterDot] (\[FormalAlpha]_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 107}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalA]_)) <-> (((\[FormalC]_) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] (\[FormalC]_))) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 2, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 156} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalB] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalC]) \[CenterDot] \[FormalB])) == 
         (\[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalC]) \[CenterDot] \[FormalB])) \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 88}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) <-> 
          ((\[FormalB]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalA]_), "Side" -> 1, "Subpattern" -> 
         (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 155}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
              (\[FormalC]_)) \[CenterDot] (\[FormalB]_))) -> 
          \[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] \[FormalB]), 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 141} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalC]) \[CenterDot] 
            \[FormalB])) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalC] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 156}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 127}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
              (\[FormalC]_)) \[CenterDot] (\[FormalB]_))) -> \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalA] == 
           (\[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalC]) \[CenterDot] \[FormalB])) \[CenterDot] 
            (\[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] \[FormalB]))], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 142} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
          ((\[FormalC] \[CenterDot] (\[FormalA] \[CenterDot] 
             \[FormalB])) \[CenterDot] \[FormalC])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 141}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 143}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] (\[FormalAlpha]_) -> 
          \[FormalAlpha] \[CenterDot] ((\[FormalC] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA]), "OutputExpression" -> 
         HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
            ((\[FormalC] \[CenterDot] (\[FormalA] \[CenterDot] 
               \[FormalB])) \[CenterDot] \[FormalC])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 143} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
          (\[FormalC] \[CenterDot] ((\[FormalB] \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalC]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 142}, "Position" -> {2}, 
        "Construct" -> {"CriticalPairLemma", 143}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] (\[FormalAlpha]_) -> 
          \[FormalAlpha] \[CenterDot] ((\[FormalC] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA]), "OutputExpression" -> 
         HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
            (\[FormalC] \[CenterDot] ((\[FormalB] \[CenterDot] 
               \[FormalA]) \[CenterDot] \[FormalC]))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 157} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] \[FormalA]) == 
         ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
           (\[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
              \[FormalC]) \[CenterDot] \[FormalA]))) \[CenterDot] 
          \[FormalC]], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 45}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> ((\[FormalC]_) \[CenterDot] 
           (\[FormalA]_)) \[CenterDot] (\[FormalB]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 143}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) \[CenterDot] 
           ((\[FormalC]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalC]_))) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 144} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] \[FormalA]) == 
         ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalA])) \[CenterDot] \[FormalC]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 157}, "Position" -> {1}, 
        "Construct" -> {"SubstitutionLemma", 104}, "Orientation" -> -1, 
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
    {"CriticalPairLemma", 158} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] \[FormalA]) == 
         ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           (\[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
              \[FormalC]) \[CenterDot] \[FormalA]))) \[CenterDot] 
          \[FormalC]], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 
          101}, "Orientation" -> -1, "Rule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] (((\[FormalAlpha]_) \[CenterDot] 
             ((\[FormalB]_) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
            (\[FormalC]_)) -> \[FormalC], "Side" -> 1, 
        "Subpattern" -> ((\[FormalAlpha]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
          (\[FormalC]_), "MatchingConstruct" -> {"SubstitutionLemma", 143}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
            (((\[FormalB]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
             (\[FormalC]_))) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 145} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] \[FormalA]) == 
         ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB])) \[CenterDot] \[FormalC]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 158}, "Position" -> {1}, 
        "Construct" -> {"SubstitutionLemma", 106}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
              (\[FormalC]_)) \[CenterDot] (\[FormalA]_))) -> 
          (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB]), "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
              \[FormalC]) \[CenterDot] \[FormalA]) == 
           ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
             (\[FormalA] \[CenterDot] \[FormalB])) \[CenterDot] \[FormalC]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 146} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] \[FormalA]) == 
         \[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalC]) \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 145}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 144}, "Orientation" -> -1, 
        "Rule" -> (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalB]_))) \[CenterDot] 
           (\[FormalC]_) -> \[FormalB] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalC]) \[CenterDot] \[FormalB]), 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] 
            ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] \[FormalA]) == 
           \[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalC]) \[CenterDot] \[FormalB])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 147} -> 
     <|"Statement" -> HoldForm[
        (((\[FormalD] \[CenterDot] \[FormalE]) \[CenterDot] 
            (\[FormalD] \[CenterDot] \[FormalE])) \[CenterDot] 
           \[FormalF]) \[CenterDot] (\[FormalF] \[CenterDot] 
           ((\[FormalD] \[CenterDot] \[FormalE]) \[CenterDot] \[FormalF])) == 
         (\[FormalD] \[CenterDot] ((\[FormalE] \[CenterDot] 
             \[FormalF]) \[CenterDot] (\[FormalE] \[CenterDot] 
             \[FormalF]))) \[CenterDot] (\[FormalD] \[CenterDot] 
           ((\[FormalE] \[CenterDot] \[FormalF]) \[CenterDot] 
            (\[FormalE] \[CenterDot] \[FormalF])))], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Position" -> {2}, 
        "Construct" -> {"CriticalPairLemma", 88}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           (\[FormalB]_) -> \[FormalB] \[CenterDot] (\[FormalA] \[CenterDot] 
            \[FormalB]), "OutputExpression" -> HoldForm[
          (((\[FormalD] \[CenterDot] \[FormalE]) \[CenterDot] 
              (\[FormalD] \[CenterDot] \[FormalE])) \[CenterDot] 
             \[FormalF]) \[CenterDot] (\[FormalF] \[CenterDot] 
             ((\[FormalD] \[CenterDot] \[FormalE]) \[CenterDot] 
              \[FormalF])) == (\[FormalD] \[CenterDot] 
             ((\[FormalE] \[CenterDot] \[FormalF]) \[CenterDot] 
              (\[FormalE] \[CenterDot] \[FormalF]))) \[CenterDot] 
            (\[FormalD] \[CenterDot] ((\[FormalE] \[CenterDot] 
               \[FormalF]) \[CenterDot] (\[FormalE] \[CenterDot] 
               \[FormalF])))], "ConstructSide" -> 2, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 148} -> 
     <|"Statement" -> HoldForm[
        (((\[FormalD] \[CenterDot] \[FormalE]) \[CenterDot] 
            (\[FormalD] \[CenterDot] \[FormalE])) \[CenterDot] 
           \[FormalF]) \[CenterDot] (\[FormalF] \[CenterDot] 
           ((\[FormalE] \[CenterDot] \[FormalD]) \[CenterDot] \[FormalF])) == 
         (\[FormalD] \[CenterDot] ((\[FormalE] \[CenterDot] 
             \[FormalF]) \[CenterDot] (\[FormalE] \[CenterDot] 
             \[FormalF]))) \[CenterDot] (\[FormalD] \[CenterDot] 
           ((\[FormalE] \[CenterDot] \[FormalF]) \[CenterDot] 
            (\[FormalE] \[CenterDot] \[FormalF])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 147}, 
        "Position" -> {2, 2, 1}, "Construct" -> {"SubstitutionLemma", 60}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (\[FormalB]_) -> \[FormalB] \[CenterDot] \[FormalA], 
        "OutputExpression" -> HoldForm[
          (((\[FormalD] \[CenterDot] \[FormalE]) \[CenterDot] 
              (\[FormalD] \[CenterDot] \[FormalE])) \[CenterDot] 
             \[FormalF]) \[CenterDot] (\[FormalF] \[CenterDot] 
             ((\[FormalE] \[CenterDot] \[FormalD]) \[CenterDot] 
              \[FormalF])) == (\[FormalD] \[CenterDot] 
             ((\[FormalE] \[CenterDot] \[FormalF]) \[CenterDot] 
              (\[FormalE] \[CenterDot] \[FormalF]))) \[CenterDot] 
            (\[FormalD] \[CenterDot] ((\[FormalE] \[CenterDot] 
               \[FormalF]) \[CenterDot] (\[FormalE] \[CenterDot] 
               \[FormalF])))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 149} -> 
     <|"Statement" -> HoldForm[
        (((\[FormalD] \[CenterDot] \[FormalE]) \[CenterDot] 
            (\[FormalD] \[CenterDot] \[FormalE])) \[CenterDot] 
           \[FormalF]) \[CenterDot] (((\[FormalE] \[CenterDot] 
             \[FormalD]) \[CenterDot] \[FormalF]) \[CenterDot] \[FormalF]) == 
         (\[FormalD] \[CenterDot] ((\[FormalE] \[CenterDot] 
             \[FormalF]) \[CenterDot] (\[FormalE] \[CenterDot] 
             \[FormalF]))) \[CenterDot] (\[FormalD] \[CenterDot] 
           ((\[FormalE] \[CenterDot] \[FormalF]) \[CenterDot] 
            (\[FormalE] \[CenterDot] \[FormalF])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 148}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 60}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(((\[FormalD] \[CenterDot] \[FormalE]) \[CenterDot] 
              (\[FormalD] \[CenterDot] \[FormalE])) \[CenterDot] 
             \[FormalF]) \[CenterDot] (((\[FormalE] \[CenterDot] 
               \[FormalD]) \[CenterDot] \[FormalF]) \[CenterDot] 
             \[FormalF]) == (\[FormalD] \[CenterDot] 
             ((\[FormalE] \[CenterDot] \[FormalF]) \[CenterDot] 
              (\[FormalE] \[CenterDot] \[FormalF]))) \[CenterDot] 
            (\[FormalD] \[CenterDot] ((\[FormalE] \[CenterDot] 
               \[FormalF]) \[CenterDot] (\[FormalE] \[CenterDot] 
               \[FormalF])))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 150} -> 
     <|"Statement" -> HoldForm[(\[FormalF] \[CenterDot] 
           ((\[FormalD] \[CenterDot] \[FormalE]) \[CenterDot] 
            \[FormalF])) \[CenterDot] (((\[FormalE] \[CenterDot] 
             \[FormalD]) \[CenterDot] \[FormalF]) \[CenterDot] \[FormalF]) == 
         (\[FormalD] \[CenterDot] ((\[FormalE] \[CenterDot] 
             \[FormalF]) \[CenterDot] (\[FormalE] \[CenterDot] 
             \[FormalF]))) \[CenterDot] (\[FormalD] \[CenterDot] 
           ((\[FormalE] \[CenterDot] \[FormalF]) \[CenterDot] 
            (\[FormalE] \[CenterDot] \[FormalF])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 149}, "Position" -> {1}, 
        "Construct" -> {"CriticalPairLemma", 88}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           (\[FormalB]_) -> \[FormalB] \[CenterDot] (\[FormalA] \[CenterDot] 
            \[FormalB]), "OutputExpression" -> HoldForm[
          (\[FormalF] \[CenterDot] ((\[FormalD] \[CenterDot] 
               \[FormalE]) \[CenterDot] \[FormalF])) \[CenterDot] 
            (((\[FormalE] \[CenterDot] \[FormalD]) \[CenterDot] 
              \[FormalF]) \[CenterDot] \[FormalF]) == 
           (\[FormalD] \[CenterDot] ((\[FormalE] \[CenterDot] 
               \[FormalF]) \[CenterDot] (\[FormalE] \[CenterDot] 
               \[FormalF]))) \[CenterDot] (\[FormalD] \[CenterDot] 
             ((\[FormalE] \[CenterDot] \[FormalF]) \[CenterDot] 
              (\[FormalE] \[CenterDot] \[FormalF])))], "ConstructSide" -> 2, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 151} -> 
     <|"Statement" -> HoldForm[
        (((\[FormalD] \[CenterDot] \[FormalE]) \[CenterDot] 
            \[FormalF]) \[CenterDot] \[FormalF]) \[CenterDot] 
          (((\[FormalE] \[CenterDot] \[FormalD]) \[CenterDot] 
            \[FormalF]) \[CenterDot] \[FormalF]) == 
         (\[FormalD] \[CenterDot] ((\[FormalE] \[CenterDot] 
             \[FormalF]) \[CenterDot] (\[FormalE] \[CenterDot] 
             \[FormalF]))) \[CenterDot] (\[FormalD] \[CenterDot] 
           ((\[FormalE] \[CenterDot] \[FormalF]) \[CenterDot] 
            (\[FormalE] \[CenterDot] \[FormalF])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 150}, "Position" -> {1}, 
        "Construct" -> {"SubstitutionLemma", 60}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(((\[FormalD] \[CenterDot] \[FormalE]) \[CenterDot] 
              \[FormalF]) \[CenterDot] \[FormalF]) \[CenterDot] 
            (((\[FormalE] \[CenterDot] \[FormalD]) \[CenterDot] 
              \[FormalF]) \[CenterDot] \[FormalF]) == 
           (\[FormalD] \[CenterDot] ((\[FormalE] \[CenterDot] 
               \[FormalF]) \[CenterDot] (\[FormalE] \[CenterDot] 
               \[FormalF]))) \[CenterDot] (\[FormalD] \[CenterDot] 
             ((\[FormalE] \[CenterDot] \[FormalF]) \[CenterDot] 
              (\[FormalE] \[CenterDot] \[FormalF])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 152} -> 
     <|"Statement" -> HoldForm[
        (((\[FormalD] \[CenterDot] \[FormalE]) \[CenterDot] 
            \[FormalF]) \[CenterDot] \[FormalF]) \[CenterDot] 
          (((\[FormalD] \[CenterDot] \[FormalE]) \[CenterDot] 
            \[FormalF]) \[CenterDot] \[FormalF]) == 
         (\[FormalD] \[CenterDot] ((\[FormalE] \[CenterDot] 
             \[FormalF]) \[CenterDot] (\[FormalE] \[CenterDot] 
             \[FormalF]))) \[CenterDot] (\[FormalD] \[CenterDot] 
           ((\[FormalE] \[CenterDot] \[FormalF]) \[CenterDot] 
            (\[FormalE] \[CenterDot] \[FormalF])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 151}, 
        "Position" -> {2, 1, 1}, "Construct" -> {"SubstitutionLemma", 60}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (\[FormalB]_) -> \[FormalB] \[CenterDot] \[FormalA], 
        "OutputExpression" -> HoldForm[
          (((\[FormalD] \[CenterDot] \[FormalE]) \[CenterDot] 
              \[FormalF]) \[CenterDot] \[FormalF]) \[CenterDot] 
            (((\[FormalD] \[CenterDot] \[FormalE]) \[CenterDot] 
              \[FormalF]) \[CenterDot] \[FormalF]) == 
           (\[FormalD] \[CenterDot] ((\[FormalE] \[CenterDot] 
               \[FormalF]) \[CenterDot] (\[FormalE] \[CenterDot] 
               \[FormalF]))) \[CenterDot] (\[FormalD] \[CenterDot] 
             ((\[FormalE] \[CenterDot] \[FormalF]) \[CenterDot] 
              (\[FormalE] \[CenterDot] \[FormalF])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 153} -> 
     <|"Statement" -> HoldForm[
        (((\[FormalD] \[CenterDot] \[FormalE]) \[CenterDot] 
            \[FormalF]) \[CenterDot] \[FormalF]) \[CenterDot] 
          (\[FormalF] \[CenterDot] (\[FormalF] \[CenterDot] \[FormalF])) == 
         (\[FormalD] \[CenterDot] ((\[FormalE] \[CenterDot] 
             \[FormalF]) \[CenterDot] (\[FormalE] \[CenterDot] 
             \[FormalF]))) \[CenterDot] (\[FormalD] \[CenterDot] 
           ((\[FormalE] \[CenterDot] \[FormalF]) \[CenterDot] 
            (\[FormalE] \[CenterDot] \[FormalF])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 152}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 77}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalA]_) -> 
          \[FormalA] \[CenterDot] (\[FormalF] \[CenterDot] 
            (\[FormalF] \[CenterDot] \[FormalF])), "OutputExpression" -> 
         HoldForm[(((\[FormalD] \[CenterDot] \[FormalE]) \[CenterDot] 
              \[FormalF]) \[CenterDot] \[FormalF]) \[CenterDot] 
            (\[FormalF] \[CenterDot] (\[FormalF] \[CenterDot] \[FormalF])) == 
           (\[FormalD] \[CenterDot] ((\[FormalE] \[CenterDot] 
               \[FormalF]) \[CenterDot] (\[FormalE] \[CenterDot] 
               \[FormalF]))) \[CenterDot] (\[FormalD] \[CenterDot] 
             ((\[FormalE] \[CenterDot] \[FormalF]) \[CenterDot] 
              (\[FormalE] \[CenterDot] \[FormalF])))], "ConstructSide" -> 2, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 154} -> 
     <|"Statement" -> HoldForm[
        (((\[FormalD] \[CenterDot] \[FormalE]) \[CenterDot] 
            \[FormalF]) \[CenterDot] \[FormalF]) \[CenterDot] 
          ((\[FormalF] \[CenterDot] \[FormalF]) \[CenterDot] \[FormalF]) == 
         (\[FormalD] \[CenterDot] ((\[FormalE] \[CenterDot] 
             \[FormalF]) \[CenterDot] (\[FormalE] \[CenterDot] 
             \[FormalF]))) \[CenterDot] (\[FormalD] \[CenterDot] 
           ((\[FormalE] \[CenterDot] \[FormalF]) \[CenterDot] 
            (\[FormalE] \[CenterDot] \[FormalF])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 153}, "Position" -> {2}, 
        "Construct" -> {"CriticalPairLemma", 88}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalA]_)) -> (\[FormalB] \[CenterDot] 
            \[FormalB]) \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(((\[FormalD] \[CenterDot] \[FormalE]) \[CenterDot] 
              \[FormalF]) \[CenterDot] \[FormalF]) \[CenterDot] 
            ((\[FormalF] \[CenterDot] \[FormalF]) \[CenterDot] \[FormalF]) == 
           (\[FormalD] \[CenterDot] ((\[FormalE] \[CenterDot] 
               \[FormalF]) \[CenterDot] (\[FormalE] \[CenterDot] 
               \[FormalF]))) \[CenterDot] (\[FormalD] \[CenterDot] 
             ((\[FormalE] \[CenterDot] \[FormalF]) \[CenterDot] 
              (\[FormalE] \[CenterDot] \[FormalF])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 155} -> 
     <|"Statement" -> HoldForm[
        (((\[FormalE] \[CenterDot] \[FormalD]) \[CenterDot] 
            \[FormalF]) \[CenterDot] \[FormalF]) \[CenterDot] 
          ((\[FormalF] \[CenterDot] \[FormalF]) \[CenterDot] \[FormalF]) == 
         (\[FormalD] \[CenterDot] ((\[FormalE] \[CenterDot] 
             \[FormalF]) \[CenterDot] (\[FormalE] \[CenterDot] 
             \[FormalF]))) \[CenterDot] (\[FormalD] \[CenterDot] 
           ((\[FormalE] \[CenterDot] \[FormalF]) \[CenterDot] 
            (\[FormalE] \[CenterDot] \[FormalF])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 154}, 
        "Position" -> {1, 1, 1}, "Construct" -> {"SubstitutionLemma", 60}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (\[FormalB]_) -> \[FormalB] \[CenterDot] \[FormalA], 
        "OutputExpression" -> HoldForm[
          (((\[FormalE] \[CenterDot] \[FormalD]) \[CenterDot] 
              \[FormalF]) \[CenterDot] \[FormalF]) \[CenterDot] 
            ((\[FormalF] \[CenterDot] \[FormalF]) \[CenterDot] \[FormalF]) == 
           (\[FormalD] \[CenterDot] ((\[FormalE] \[CenterDot] 
               \[FormalF]) \[CenterDot] (\[FormalE] \[CenterDot] 
               \[FormalF]))) \[CenterDot] (\[FormalD] \[CenterDot] 
             ((\[FormalE] \[CenterDot] \[FormalF]) \[CenterDot] 
              (\[FormalE] \[CenterDot] \[FormalF])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 156} -> 
     <|"Statement" -> HoldForm[
        ((\[FormalF] \[CenterDot] (\[FormalE] \[CenterDot] 
             \[FormalD])) \[CenterDot] \[FormalF]) \[CenterDot] 
          ((\[FormalF] \[CenterDot] \[FormalF]) \[CenterDot] \[FormalF]) == 
         (\[FormalD] \[CenterDot] ((\[FormalE] \[CenterDot] 
             \[FormalF]) \[CenterDot] (\[FormalE] \[CenterDot] 
             \[FormalF]))) \[CenterDot] (\[FormalD] \[CenterDot] 
           ((\[FormalE] \[CenterDot] \[FormalF]) \[CenterDot] 
            (\[FormalE] \[CenterDot] \[FormalF])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 155}, 
        "Position" -> {1, 1}, "Construct" -> {"SubstitutionLemma", 60}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (\[FormalB]_) -> \[FormalB] \[CenterDot] \[FormalA], 
        "OutputExpression" -> HoldForm[
          ((\[FormalF] \[CenterDot] (\[FormalE] \[CenterDot] 
               \[FormalD])) \[CenterDot] \[FormalF]) \[CenterDot] 
            ((\[FormalF] \[CenterDot] \[FormalF]) \[CenterDot] \[FormalF]) == 
           (\[FormalD] \[CenterDot] ((\[FormalE] \[CenterDot] 
               \[FormalF]) \[CenterDot] (\[FormalE] \[CenterDot] 
               \[FormalF]))) \[CenterDot] (\[FormalD] \[CenterDot] 
             ((\[FormalE] \[CenterDot] \[FormalF]) \[CenterDot] 
              (\[FormalE] \[CenterDot] \[FormalF])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 157} -> 
     <|"Statement" -> HoldForm[(\[FormalF] \[CenterDot] 
           (\[FormalF] \[CenterDot] (\[FormalE] \[CenterDot] 
             \[FormalD]))) \[CenterDot] ((\[FormalF] \[CenterDot] 
            \[FormalF]) \[CenterDot] \[FormalF]) == 
         (\[FormalD] \[CenterDot] ((\[FormalE] \[CenterDot] 
             \[FormalF]) \[CenterDot] (\[FormalE] \[CenterDot] 
             \[FormalF]))) \[CenterDot] (\[FormalD] \[CenterDot] 
           ((\[FormalE] \[CenterDot] \[FormalF]) \[CenterDot] 
            (\[FormalE] \[CenterDot] \[FormalF])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 156}, "Position" -> {1}, 
        "Construct" -> {"SubstitutionLemma", 60}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalF] \[CenterDot] (\[FormalF] \[CenterDot] 
              (\[FormalE] \[CenterDot] \[FormalD]))) \[CenterDot] 
            ((\[FormalF] \[CenterDot] \[FormalF]) \[CenterDot] \[FormalF]) == 
           (\[FormalD] \[CenterDot] ((\[FormalE] \[CenterDot] 
               \[FormalF]) \[CenterDot] (\[FormalE] \[CenterDot] 
               \[FormalF]))) \[CenterDot] (\[FormalD] \[CenterDot] 
             ((\[FormalE] \[CenterDot] \[FormalF]) \[CenterDot] 
              (\[FormalE] \[CenterDot] \[FormalF])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 158} -> 
     <|"Statement" -> HoldForm[
        ((\[FormalF] \[CenterDot] \[FormalF]) \[CenterDot] 
           \[FormalF]) \[CenterDot] (\[FormalF] \[CenterDot] 
           (\[FormalF] \[CenterDot] (\[FormalE] \[CenterDot] \[FormalD]))) == 
         (\[FormalD] \[CenterDot] ((\[FormalE] \[CenterDot] 
             \[FormalF]) \[CenterDot] (\[FormalE] \[CenterDot] 
             \[FormalF]))) \[CenterDot] (\[FormalD] \[CenterDot] 
           ((\[FormalE] \[CenterDot] \[FormalF]) \[CenterDot] 
            (\[FormalE] \[CenterDot] \[FormalF])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 157}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 60}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[((\[FormalF] \[CenterDot] \[FormalF]) \[CenterDot] 
             \[FormalF]) \[CenterDot] (\[FormalF] \[CenterDot] 
             (\[FormalF] \[CenterDot] (\[FormalE] \[CenterDot] 
               \[FormalD]))) == (\[FormalD] \[CenterDot] 
             ((\[FormalE] \[CenterDot] \[FormalF]) \[CenterDot] 
              (\[FormalE] \[CenterDot] \[FormalF]))) \[CenterDot] 
            (\[FormalD] \[CenterDot] ((\[FormalE] \[CenterDot] 
               \[FormalF]) \[CenterDot] (\[FormalE] \[CenterDot] 
               \[FormalF])))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 159} -> 
     <|"Statement" -> HoldForm[(\[FormalF] \[CenterDot] 
           (\[FormalF] \[CenterDot] \[FormalF])) \[CenterDot] 
          (\[FormalF] \[CenterDot] (\[FormalF] \[CenterDot] 
            (\[FormalE] \[CenterDot] \[FormalD]))) == 
         (\[FormalD] \[CenterDot] ((\[FormalE] \[CenterDot] 
             \[FormalF]) \[CenterDot] (\[FormalE] \[CenterDot] 
             \[FormalF]))) \[CenterDot] (\[FormalD] \[CenterDot] 
           ((\[FormalE] \[CenterDot] \[FormalF]) \[CenterDot] 
            (\[FormalE] \[CenterDot] \[FormalF])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 158}, "Position" -> {1}, 
        "Construct" -> {"CriticalPairLemma", 90}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           (\[FormalB]_) -> \[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
            \[FormalA]), "OutputExpression" -> HoldForm[
          (\[FormalF] \[CenterDot] (\[FormalF] \[CenterDot] 
              \[FormalF])) \[CenterDot] (\[FormalF] \[CenterDot] 
             (\[FormalF] \[CenterDot] (\[FormalE] \[CenterDot] 
               \[FormalD]))) == (\[FormalD] \[CenterDot] 
             ((\[FormalE] \[CenterDot] \[FormalF]) \[CenterDot] 
              (\[FormalE] \[CenterDot] \[FormalF]))) \[CenterDot] 
            (\[FormalD] \[CenterDot] ((\[FormalE] \[CenterDot] 
               \[FormalF]) \[CenterDot] (\[FormalE] \[CenterDot] 
               \[FormalF])))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 160} -> 
     <|"Statement" -> HoldForm[(\[FormalF] \[CenterDot] 
           (\[FormalF] \[CenterDot] \[FormalF])) \[CenterDot] 
          (\[FormalF] \[CenterDot] (\[FormalF] \[CenterDot] 
            (\[FormalE] \[CenterDot] \[FormalD]))) == 
         (\[FormalD] \[CenterDot] ((\[FormalE] \[CenterDot] 
             \[FormalF]) \[CenterDot] (\[FormalE] \[CenterDot] 
             \[FormalF]))) \[CenterDot] (\[FormalD] \[CenterDot] 
           (\[FormalD] \[CenterDot] (\[FormalE] \[CenterDot] \[FormalF])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 159}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 103}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalB]_))) \[CenterDot] (\[FormalC]_) -> 
          \[FormalC] \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB])), "OutputExpression" -> 
         HoldForm[(\[FormalF] \[CenterDot] (\[FormalF] \[CenterDot] 
              \[FormalF])) \[CenterDot] (\[FormalF] \[CenterDot] 
             (\[FormalF] \[CenterDot] (\[FormalE] \[CenterDot] 
               \[FormalD]))) == (\[FormalD] \[CenterDot] 
             ((\[FormalE] \[CenterDot] \[FormalF]) \[CenterDot] 
              (\[FormalE] \[CenterDot] \[FormalF]))) \[CenterDot] 
            (\[FormalD] \[CenterDot] (\[FormalD] \[CenterDot] 
              (\[FormalE] \[CenterDot] \[FormalF])))], "ConstructSide" -> 2, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 161} -> 
     <|"Statement" -> HoldForm[(\[FormalF] \[CenterDot] 
           (\[FormalF] \[CenterDot] \[FormalF])) \[CenterDot] 
          (\[FormalF] \[CenterDot] (\[FormalF] \[CenterDot] 
            (\[FormalE] \[CenterDot] \[FormalD]))) == 
         (\[FormalD] \[CenterDot] (\[FormalD] \[CenterDot] 
            (\[FormalE] \[CenterDot] \[FormalF]))) \[CenterDot] 
          (((\[FormalE] \[CenterDot] \[FormalF]) \[CenterDot] 
            (\[FormalE] \[CenterDot] \[FormalF])) \[CenterDot] \[FormalD])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 160}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 143}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] (\[FormalAlpha]_) -> 
          \[FormalAlpha] \[CenterDot] ((\[FormalC] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA]), "OutputExpression" -> 
         HoldForm[(\[FormalF] \[CenterDot] (\[FormalF] \[CenterDot] 
              \[FormalF])) \[CenterDot] (\[FormalF] \[CenterDot] 
             (\[FormalF] \[CenterDot] (\[FormalE] \[CenterDot] 
               \[FormalD]))) == (\[FormalD] \[CenterDot] 
             (\[FormalD] \[CenterDot] (\[FormalE] \[CenterDot] 
               \[FormalF]))) \[CenterDot] (((\[FormalE] \[CenterDot] 
               \[FormalF]) \[CenterDot] (\[FormalE] \[CenterDot] 
               \[FormalF])) \[CenterDot] \[FormalD])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 162} -> 
     <|"Statement" -> HoldForm[(\[FormalF] \[CenterDot] 
           (\[FormalF] \[CenterDot] \[FormalF])) \[CenterDot] 
          (\[FormalF] \[CenterDot] (\[FormalF] \[CenterDot] 
            (\[FormalE] \[CenterDot] \[FormalD]))) == 
         (\[FormalD] \[CenterDot] ((\[FormalE] \[CenterDot] 
             \[FormalF]) \[CenterDot] \[FormalD])) \[CenterDot] 
          (\[FormalD] \[CenterDot] (\[FormalD] \[CenterDot] 
            (\[FormalE] \[CenterDot] \[FormalF])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 161}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 135}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalC]_)) -> 
          (\[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalC])) \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalF] \[CenterDot] (\[FormalF] \[CenterDot] 
              \[FormalF])) \[CenterDot] (\[FormalF] \[CenterDot] 
             (\[FormalF] \[CenterDot] (\[FormalE] \[CenterDot] 
               \[FormalD]))) == (\[FormalD] \[CenterDot] 
             ((\[FormalE] \[CenterDot] \[FormalF]) \[CenterDot] 
              \[FormalD])) \[CenterDot] (\[FormalD] \[CenterDot] 
             (\[FormalD] \[CenterDot] (\[FormalE] \[CenterDot] 
               \[FormalF])))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 163} -> 
     <|"Statement" -> HoldForm[(\[FormalF] \[CenterDot] 
           (\[FormalF] \[CenterDot] \[FormalF])) \[CenterDot] 
          (\[FormalF] \[CenterDot] (\[FormalF] \[CenterDot] 
            (\[FormalE] \[CenterDot] \[FormalD]))) == 
         (\[FormalD] \[CenterDot] (\[FormalD] \[CenterDot] 
            (\[FormalE] \[CenterDot] \[FormalF]))) \[CenterDot] 
          ((\[FormalD] \[CenterDot] (\[FormalE] \[CenterDot] 
             \[FormalF])) \[CenterDot] \[FormalD])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 162}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 143}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] (\[FormalAlpha]_) -> 
          \[FormalAlpha] \[CenterDot] ((\[FormalC] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA]), "OutputExpression" -> 
         HoldForm[(\[FormalF] \[CenterDot] (\[FormalF] \[CenterDot] 
              \[FormalF])) \[CenterDot] (\[FormalF] \[CenterDot] 
             (\[FormalF] \[CenterDot] (\[FormalE] \[CenterDot] 
               \[FormalD]))) == (\[FormalD] \[CenterDot] 
             (\[FormalD] \[CenterDot] (\[FormalE] \[CenterDot] 
               \[FormalF]))) \[CenterDot] ((\[FormalD] \[CenterDot] 
              (\[FormalE] \[CenterDot] \[FormalF])) \[CenterDot] 
             \[FormalD])], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 164} -> 
     <|"Statement" -> HoldForm[(\[FormalF] \[CenterDot] 
           (\[FormalF] \[CenterDot] \[FormalF])) \[CenterDot] 
          (\[FormalF] \[CenterDot] (\[FormalF] \[CenterDot] 
            (\[FormalE] \[CenterDot] \[FormalD]))) == 
         (\[FormalD] \[CenterDot] (\[FormalD] \[CenterDot] 
            (\[FormalE] \[CenterDot] \[FormalF]))) \[CenterDot] 
          (\[FormalD] \[CenterDot] (\[FormalD] \[CenterDot] 
            (\[FormalE] \[CenterDot] \[FormalF])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 163}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 102}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalC]_)) -> (\[FormalC] \[CenterDot] 
            \[FormalB]) \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalF] \[CenterDot] (\[FormalF] \[CenterDot] 
              \[FormalF])) \[CenterDot] (\[FormalF] \[CenterDot] 
             (\[FormalF] \[CenterDot] (\[FormalE] \[CenterDot] 
               \[FormalD]))) == (\[FormalD] \[CenterDot] 
             (\[FormalD] \[CenterDot] (\[FormalE] \[CenterDot] 
               \[FormalF]))) \[CenterDot] (\[FormalD] \[CenterDot] 
             (\[FormalD] \[CenterDot] (\[FormalE] \[CenterDot] 
               \[FormalF])))], "ConstructSide" -> 2, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 165} -> 
     <|"Statement" -> HoldForm[(\[FormalF] \[CenterDot] 
           (\[FormalF] \[CenterDot] \[FormalF])) \[CenterDot] 
          (\[FormalF] \[CenterDot] (\[FormalF] \[CenterDot] 
            (\[FormalE] \[CenterDot] \[FormalD]))) == 
         (\[FormalF] \[CenterDot] (\[FormalF] \[CenterDot] 
            \[FormalF])) \[CenterDot] ((\[FormalD] \[CenterDot] 
            (\[FormalE] \[CenterDot] \[FormalF])) \[CenterDot] \[FormalD])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 164}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 133}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) -> 
          (\[FormalF] \[CenterDot] (\[FormalF] \[CenterDot] 
             \[FormalF])) \[CenterDot] (\[FormalB] \[CenterDot] \[FormalA]), 
        "OutputExpression" -> HoldForm[(\[FormalF] \[CenterDot] 
             (\[FormalF] \[CenterDot] \[FormalF])) \[CenterDot] 
            (\[FormalF] \[CenterDot] (\[FormalF] \[CenterDot] 
              (\[FormalE] \[CenterDot] \[FormalD]))) == 
           (\[FormalF] \[CenterDot] (\[FormalF] \[CenterDot] 
              \[FormalF])) \[CenterDot] ((\[FormalD] \[CenterDot] 
              (\[FormalE] \[CenterDot] \[FormalF])) \[CenterDot] 
             \[FormalD])], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 166} -> 
     <|"Statement" -> HoldForm[(\[FormalF] \[CenterDot] 
           (\[FormalF] \[CenterDot] \[FormalF])) \[CenterDot] 
          (\[FormalF] \[CenterDot] (\[FormalF] \[CenterDot] 
            (\[FormalE] \[CenterDot] \[FormalD]))) == 
         (\[FormalF] \[CenterDot] (\[FormalF] \[CenterDot] 
            \[FormalF])) \[CenterDot] (\[FormalD] \[CenterDot] 
           ((\[FormalF] \[CenterDot] \[FormalE]) \[CenterDot] \[FormalD]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 165}, "Position" -> {2}, 
        "Construct" -> {"CriticalPairLemma", 143}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] (\[FormalAlpha]_) -> 
          \[FormalAlpha] \[CenterDot] ((\[FormalC] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA]), "OutputExpression" -> 
         HoldForm[(\[FormalF] \[CenterDot] (\[FormalF] \[CenterDot] 
              \[FormalF])) \[CenterDot] (\[FormalF] \[CenterDot] 
             (\[FormalF] \[CenterDot] (\[FormalE] \[CenterDot] 
               \[FormalD]))) == (\[FormalF] \[CenterDot] 
             (\[FormalF] \[CenterDot] \[FormalF])) \[CenterDot] 
            (\[FormalD] \[CenterDot] ((\[FormalF] \[CenterDot] 
               \[FormalE]) \[CenterDot] \[FormalD]))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 167} -> 
     <|"Statement" -> HoldForm[(\[FormalF] \[CenterDot] 
           (\[FormalF] \[CenterDot] \[FormalF])) \[CenterDot] 
          (\[FormalF] \[CenterDot] (\[FormalF] \[CenterDot] 
            (\[FormalE] \[CenterDot] \[FormalD]))) == 
         (\[FormalF] \[CenterDot] (\[FormalF] \[CenterDot] 
            \[FormalF])) \[CenterDot] (\[FormalF] \[CenterDot] 
           ((\[FormalD] \[CenterDot] \[FormalE]) \[CenterDot] \[FormalF]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 166}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 146}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] (\[FormalA]_)) -> 
          \[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalC]) \[CenterDot] \[FormalB]), "OutputExpression" -> 
         HoldForm[(\[FormalF] \[CenterDot] (\[FormalF] \[CenterDot] 
              \[FormalF])) \[CenterDot] (\[FormalF] \[CenterDot] 
             (\[FormalF] \[CenterDot] (\[FormalE] \[CenterDot] 
               \[FormalD]))) == (\[FormalF] \[CenterDot] 
             (\[FormalF] \[CenterDot] \[FormalF])) \[CenterDot] 
            (\[FormalF] \[CenterDot] ((\[FormalD] \[CenterDot] 
               \[FormalE]) \[CenterDot] \[FormalF]))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"Conclusion", 1} -> <|"Statement" -> 
       HoldForm[(\[FormalF] \[CenterDot] (\[FormalF] \[CenterDot] 
            \[FormalF])) \[CenterDot] (\[FormalF] \[CenterDot] 
           (\[FormalF] \[CenterDot] (\[FormalE] \[CenterDot] \[FormalD]))) == 
         (\[FormalF] \[CenterDot] (\[FormalF] \[CenterDot] 
            \[FormalF])) \[CenterDot] (\[FormalF] \[CenterDot] 
           (\[FormalF] \[CenterDot] (\[FormalE] \[CenterDot] \[FormalD])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 167}, 
        "Position" -> {2, 2}, "Construct" -> {"SubstitutionLemma", 102}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (\[FormalC]_) -> 
          \[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalA]), 
        "OutputExpression" -> HoldForm[(\[FormalF] \[CenterDot] 
             (\[FormalF] \[CenterDot] \[FormalF])) \[CenterDot] 
            (\[FormalF] \[CenterDot] (\[FormalF] \[CenterDot] 
              (\[FormalE] \[CenterDot] \[FormalD]))) == 
           (\[FormalF] \[CenterDot] (\[FormalF] \[CenterDot] 
              \[FormalF])) \[CenterDot] (\[FormalF] \[CenterDot] 
             (\[FormalF] \[CenterDot] (\[FormalE] \[CenterDot] 
               \[FormalD])))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>}|>]
