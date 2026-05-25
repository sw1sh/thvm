ProofObject["EquationalLogic", {ForAll[{\[FormalA], \[FormalB], \[FormalC]}, 
   (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
     (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalC])) == 
    \[FormalA]], ForAll[{\[FormalA], \[FormalB]}, 
   \[FormalA] \[CenterDot] \[FormalB] == \[FormalB] \[CenterDot] 
     \[FormalA]]}, {ForAll[{\[FormalA], \[FormalB], \[FormalC]}, 
   ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
      \[FormalC]) \[CenterDot] (\[FormalA] \[CenterDot] 
      ((\[FormalA] \[CenterDot] \[FormalC]) \[CenterDot] \[FormalA])) == 
    \[FormalC]]}, <|"Variables" -> {\[FormalA], \[FormalB], \[FormalC], 
    \[FormalD], \[FormalE], \[FormalF], \[FormalG], \[FormalH], 
    \[FormalAlpha], \[FormalBeta]}, "Constants" -> {}, 
  "Proof" -> {{"Axiom", 1} -> <|"Statement" -> 
       HoldForm[\[FormalA] == ((\[FormalB] \[CenterDot] 
            \[FormalC]) \[CenterDot] \[FormalA]) \[CenterDot] 
          (\[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalB]))], "Proof" -> <||>|>, 
    {"Hypothesis", 1} -> <|"Statement" -> 
       HoldForm[(\[FormalD] \[CenterDot] \[FormalE]) \[CenterDot] 
          (\[FormalD] \[CenterDot] (\[FormalE] \[CenterDot] \[FormalF])) == 
         \[FormalD]], "Proof" -> <||>|>, {"Hypothesis", 2} -> 
     <|"Statement" -> HoldForm[\[FormalG] \[CenterDot] \[FormalH] == 
         \[FormalH] \[CenterDot] \[FormalG]], "Proof" -> <||>|>, 
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
        "Side" -> 1|>|>, {"SubstitutionLemma", 38} -> 
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
    {"SubstitutionLemma", 39} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalA])) == 
         \[FormalB] \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 38}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 36}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalA]_)) -> 
          \[FormalA] \[CenterDot] \[FormalB], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalA])) == \[FormalB] \[CenterDot] 
            \[FormalA]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 40} -> 
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
        "Side" -> 1|>|>, {"SubstitutionLemma", 41} -> 
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
    {"SubstitutionLemma", 42} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           (\[FormalC] \[CenterDot] ((\[FormalC] \[CenterDot] 
              \[FormalA]) \[CenterDot] \[FormalC]))) == 
         \[FormalC] \[CenterDot] ((\[FormalC] \[CenterDot] 
            \[FormalA]) \[CenterDot] \[FormalC])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 41}, 
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
    {"SubstitutionLemma", 43} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           (\[FormalC] \[CenterDot] \[FormalA])) == \[FormalC] \[CenterDot] 
          ((\[FormalC] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalC])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 42}, 
        "Position" -> {2, 2}, "Construct" -> {"SubstitutionLemma", 36}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA] \[CenterDot] \[FormalB], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] 
            ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
             (\[FormalC] \[CenterDot] \[FormalA])) == \[FormalC] \[CenterDot] 
            ((\[FormalC] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalC])], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 44} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           (\[FormalC] \[CenterDot] \[FormalA])) == \[FormalC] \[CenterDot] 
          \[FormalA]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 43}, 
        "Position" -> {}, "Construct" -> {"SubstitutionLemma", 36}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA] \[CenterDot] \[FormalB], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] 
            ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
             (\[FormalC] \[CenterDot] \[FormalA])) == \[FormalC] \[CenterDot] 
            \[FormalA]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 51} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] \[FormalA] == \[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 39}, 
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
    {"SubstitutionLemma", 45} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalB]) \[CenterDot] \[FormalA]) == \[FormalA]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 37}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 51}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalA]_) -> \[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
            \[FormalB]), "OutputExpression" -> HoldForm[
          (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA]) == 
           \[FormalA]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 46} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB])) == \[FormalA]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 45}, "Position" -> {2}, 
        "Construct" -> {"CriticalPairLemma", 51}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalA]_) -> \[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
            \[FormalB]), "OutputExpression" -> HoldForm[
          (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB])) == 
           \[FormalA]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"CriticalPairLemma", 52} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalB]) \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 44}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalA]_))) -> 
          \[FormalC] \[CenterDot] \[FormalA], "Side" -> 1, 
        "Subpattern" -> ((\[FormalB]_) \[CenterDot] 
           (\[FormalC]_)) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
           (\[FormalA]_)), "MatchingConstruct" -> {"CriticalPairLemma", 51}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalA]_) -> \[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
            \[FormalB]), "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 47} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB])))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 52}, 
        "Position" -> {2, 2}, "Construct" -> {"CriticalPairLemma", 51}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (\[FormalA]_) -> 
          \[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB]), 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
           \[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
              (\[FormalA] \[CenterDot] \[FormalB])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 48} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalB] \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 47}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 46}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
           \[FormalB] \[CenterDot] \[FormalA]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 49} -> 
     <|"Statement" -> HoldForm[(\[FormalD] \[CenterDot] 
           \[FormalE]) \[CenterDot] (\[FormalD] \[CenterDot] 
           (\[FormalF] \[CenterDot] \[FormalE])) == \[FormalD]], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Position" -> {2, 2}, 
        "Construct" -> {"SubstitutionLemma", 48}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalD] \[CenterDot] \[FormalE]) \[CenterDot] 
            (\[FormalD] \[CenterDot] (\[FormalF] \[CenterDot] \[FormalE])) == 
           \[FormalD]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 50} -> 
     <|"Statement" -> HoldForm[(\[FormalD] \[CenterDot] 
           \[FormalE]) \[CenterDot] ((\[FormalF] \[CenterDot] 
            \[FormalE]) \[CenterDot] \[FormalD]) == \[FormalD]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 49}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 48}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalD] \[CenterDot] \[FormalE]) \[CenterDot] 
            ((\[FormalF] \[CenterDot] \[FormalE]) \[CenterDot] \[FormalD]) == 
           \[FormalD]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 51} -> 
     <|"Statement" -> HoldForm[(\[FormalE] \[CenterDot] 
           \[FormalD]) \[CenterDot] ((\[FormalF] \[CenterDot] 
            \[FormalE]) \[CenterDot] \[FormalD]) == \[FormalD]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 50}, "Position" -> {1}, 
        "Construct" -> {"SubstitutionLemma", 48}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalE] \[CenterDot] \[FormalD]) \[CenterDot] 
            ((\[FormalF] \[CenterDot] \[FormalE]) \[CenterDot] \[FormalD]) == 
           \[FormalD]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"Conclusion", 1} -> 
     <|"Statement" -> HoldForm[\[FormalD] == \[FormalD]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 51}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 40}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (((\[FormalC]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
            (\[FormalB]_)) -> \[FormalB], "OutputExpression" -> 
         HoldForm[\[FormalD] == \[FormalD]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"Conclusion", 2} -> <|"Statement" -> 
       HoldForm[\[FormalH] \[CenterDot] \[FormalG] == \[FormalH] \[CenterDot] 
          \[FormalG]], "Proof" -> <|"Input" -> {"Hypothesis", 2}, 
        "Position" -> {}, "Construct" -> {"SubstitutionLemma", 48}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (\[FormalB]_) -> \[FormalB] \[CenterDot] \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalH] \[CenterDot] \[FormalG] == 
           \[FormalH] \[CenterDot] \[FormalG]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>}|>]
