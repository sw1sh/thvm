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
   \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] (\[FormalA] \[CenterDot] 
       \[FormalC])) == ((\[FormalC] \[CenterDot] \[FormalB]) \[CenterDot] 
      \[FormalB]) \[CenterDot] \[FormalA]], ForAll[{\[FormalA], \[FormalB]}, 
   (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
     (\[FormalB] \[CenterDot] \[FormalA]) == \[FormalA]]}, 
 <|"Variables" -> {\[FormalA], \[FormalB], \[FormalC], \[FormalD], 
    \[FormalE], \[FormalF], \[FormalG], \[FormalH]}, "Constants" -> {}, 
  "Proof" -> {{"Axiom", 1} -> <|"Statement" -> 
       HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalB] \[CenterDot] \[FormalA])], 
      "Proof" -> <||>|>, {"Axiom", 2} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalC])) == 
         ((\[FormalC] \[CenterDot] \[FormalB]) \[CenterDot] 
           \[FormalB]) \[CenterDot] \[FormalA]], "Proof" -> <||>|>, 
    {"Hypothesis", 1} -> <|"Statement" -> 
       HoldForm[(((\[FormalF] \[CenterDot] \[FormalG]) \[CenterDot] 
            (\[FormalF] \[CenterDot] \[FormalG])) \[CenterDot] 
           \[FormalH]) \[CenterDot] (((\[FormalF] \[CenterDot] 
             \[FormalG]) \[CenterDot] (\[FormalF] \[CenterDot] 
             \[FormalG])) \[CenterDot] \[FormalH]) == 
         (\[FormalF] \[CenterDot] ((\[FormalG] \[CenterDot] 
             \[FormalH]) \[CenterDot] (\[FormalG] \[CenterDot] 
             \[FormalH]))) \[CenterDot] (\[FormalF] \[CenterDot] 
           ((\[FormalG] \[CenterDot] \[FormalH]) \[CenterDot] 
            (\[FormalG] \[CenterDot] \[FormalH])))], "Proof" -> <||>|>, 
    {"CriticalPairLemma", 1} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalA]_), "MatchingConstruct" -> {"Axiom", 1}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 2} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB])) \[CenterDot] \[FormalB]], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          (\[FormalA]_), "MatchingConstruct" -> {"Axiom", 1}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 3} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           \[FormalB]) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 1}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalA] \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_))), "MatchingConstruct" -> {"Axiom", 2}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalC]_))) <-> 
          (((\[FormalC]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 4} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalA]) == 
         (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
            \[FormalA])) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 3}, 
        "Orientation" -> -1, "Rule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (\[FormalA]_) -> 
          \[FormalA] \[CenterDot] \[FormalA], "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {1, 1}|>|>, {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalA])) \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 4}, "Position" -> {}, 
        "Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalA] == 
           (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
              \[FormalA])) \[CenterDot] (\[FormalA] \[CenterDot] 
             \[FormalA])], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"CriticalPairLemma", 5} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB])) == \[FormalB] \[CenterDot] 
          \[FormalA]], "Proof" -> <|"Construct" -> {"Axiom", 2}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (\[FormalC]_))) <-> (((\[FormalC]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalA]_), "Side" -> 2, "Subpattern" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"SubstitutionLemma", 1}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalA]_))) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalB] \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 5}, "Position" -> {2}, 
        "Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
           \[FormalB] \[CenterDot] \[FormalA]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, {"CriticalPairLemma", 6} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          (\[FormalA]_), "MatchingConstruct" -> {"SubstitutionLemma", 2}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 7} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB])) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          (\[FormalA]_), "MatchingConstruct" -> {"CriticalPairLemma", 6}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 3} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalB]) == 
         \[FormalA] \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 3}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 2}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] \[FormalB]) == \[FormalA] \[CenterDot] 
            \[FormalA]], "ConstructSide" -> 1, "InputOrientation" -> -1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 4} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB])) == 
         \[FormalA] \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 3}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 2}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             (\[FormalA] \[CenterDot] \[FormalB])) == \[FormalA] \[CenterDot] 
            \[FormalA]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"CriticalPairLemma", 8} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 4}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalA] \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"SubstitutionLemma", 2}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"SubstitutionLemma", 5} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 2}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 2}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
           \[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
              \[FormalB]))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 6} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 7}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 2}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
           \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
              \[FormalB]))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 7} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalC])) == 
         (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
            \[FormalB])) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Input" -> {"Axiom", 2}, "Position" -> {1}, 
        "Construct" -> {"SubstitutionLemma", 2}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             (\[FormalA] \[CenterDot] \[FormalC])) == 
           (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
              \[FormalB])) \[CenterDot] \[FormalA]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, {"CriticalPairLemma", 9} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalA])) \[CenterDot] \[FormalB] == 
         \[FormalB] \[CenterDot] \[FormalB]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 7}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (\[FormalC]_))) <-> ((\[FormalB]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalB]_))) \[CenterDot] 
           (\[FormalA]_), "Side" -> 1, "Subpattern" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalC]_))), 
        "MatchingConstruct" -> {"SubstitutionLemma", 4}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalB]_))) -> 
          \[FormalA] \[CenterDot] \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 10} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
            \[FormalA])) \[CenterDot] \[FormalB] == \[FormalB] \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 7}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (\[FormalC]_))) <-> ((\[FormalB]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalB]_))) \[CenterDot] 
           (\[FormalA]_), "Side" -> 1, "Subpattern" -> 
         (\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
           (\[FormalC]_)), "MatchingConstruct" -> {"CriticalPairLemma", 8}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalA]_))) -> 
          \[FormalA] \[CenterDot] \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 11} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalA])) \[CenterDot] \[FormalB] == \[FormalB] \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 7}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (\[FormalC]_))) <-> ((\[FormalB]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalB]_))) \[CenterDot] 
           (\[FormalA]_), "Side" -> 1, "Subpattern" -> 
         (\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
           (\[FormalC]_)), "MatchingConstruct" -> {"SubstitutionLemma", 4}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalB]_))) -> 
          \[FormalA] \[CenterDot] \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 12} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
            \[FormalA])) \[CenterDot] (\[FormalC] \[CenterDot] \[FormalC]) == 
         (\[FormalC] \[CenterDot] \[FormalC]) \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalC])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 7}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (\[FormalC]_))) <-> ((\[FormalB]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalB]_))) \[CenterDot] 
           (\[FormalA]_), "Side" -> 1, "Subpattern" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalC]_), "MatchingConstruct" -> 
         {"Axiom", 1}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"SubstitutionLemma", 8} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
            \[FormalA])) \[CenterDot] (\[FormalC] \[CenterDot] \[FormalC]) == 
         \[FormalC]], "Proof" -> <|"Input" -> {"CriticalPairLemma", 12}, 
        "Position" -> {}, "Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "OutputExpression" -> HoldForm[(\[FormalA] \[CenterDot] 
             ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
              \[FormalA])) \[CenterDot] (\[FormalC] \[CenterDot] 
             \[FormalC]) == \[FormalC]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 13} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 9}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
           (\[FormalB]_) <-> (\[FormalB]_) \[CenterDot] (\[FormalB]_), 
        "Side" -> 1, "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"SubstitutionLemma", 2}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 14} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalA])) \[CenterDot] \[FormalB] == \[FormalB] \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 7}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (\[FormalC]_))) <-> ((\[FormalB]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalB]_))) \[CenterDot] 
           (\[FormalA]_), "Side" -> 1, "Subpattern" -> 
         (\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
           (\[FormalC]_)), "MatchingConstruct" -> {"CriticalPairLemma", 13}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalA]_) <-> 
          (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalB]_))), "MatchingSide" -> 2, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 9} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalB])) == \[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 10}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 2}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
              \[FormalB])) == \[FormalA] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalB])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 10} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB]))) == \[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 9}, "Position" -> {2, 2}, 
        "Construct" -> {"SubstitutionLemma", 2}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             (\[FormalB] \[CenterDot] (\[FormalA] \[CenterDot] 
               \[FormalB]))) == \[FormalA] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalB])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 11} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalB])) == \[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 11}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 2}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
              \[FormalB])) == \[FormalA] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalB])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 12} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalA]))) == \[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 11}, 
        "Position" -> {2, 2}, "Construct" -> {"SubstitutionLemma", 2}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (\[FormalB]_) -> \[FormalB] \[CenterDot] \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] 
            (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
              (\[FormalB] \[CenterDot] \[FormalA]))) == 
           \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalB])], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 13} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalB])) == \[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 14}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 2}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
              \[FormalB])) == \[FormalA] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalB])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 15} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalA])) == \[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 13}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalB]_))) -> 
          \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalB]), 
        "Side" -> 1, "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           (\[FormalA]_)) \[CenterDot] (\[FormalB]_), "MatchingConstruct" -> 
         {"Axiom", 1}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"SubstitutionLemma", 14} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalB]) \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 15}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 5}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalB] \[CenterDot] \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
           \[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] \[FormalB])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 16} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB])) == \[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 13}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalB]_))) -> 
          \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalB]), 
        "Side" -> 1, "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           (\[FormalA]_)) \[CenterDot] (\[FormalB]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 6}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"SubstitutionLemma", 15} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalB]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 16}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 6}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalA] \[CenterDot] \[FormalB], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
           \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] \[FormalA])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 16} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 14}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 2}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
           \[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
             (\[FormalA] \[CenterDot] \[FormalB]))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 17} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalB] \[CenterDot] \[FormalA]) == 
         (\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 16}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalB] \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          (\[FormalA]_), "MatchingConstruct" -> {"Axiom", 1}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"SubstitutionLemma", 17} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CenterDot] 
           \[FormalA]) \[CenterDot] ((\[FormalB] \[CenterDot] 
            \[FormalA]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 17}, "Position" -> {}, 
        "Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalA] == 
           (\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
            ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalA])], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 18} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB]) == 
         (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
          ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 16}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalB] \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          (\[FormalA]_), "MatchingConstruct" -> {"CriticalPairLemma", 6}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"SubstitutionLemma", 18} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalB]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 18}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 6}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) -> \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalA] == 
           (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA])], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 19} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalB]) == \[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 10}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 16}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalA]_))) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
             \[FormalB]) == \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalB])], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 20} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 15}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 2}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
           \[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
             (\[FormalA] \[CenterDot] \[FormalB]))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 21} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalA]) == \[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 12}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 20}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalB]_))) -> 
          \[FormalA] \[CenterDot] \[FormalB], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalA]) == \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalB])], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 22} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 17}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 2}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == (\[FormalB] \[CenterDot] 
             \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalA]))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 19} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 22}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalB]_))) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"SubstitutionLemma", 2}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 20} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 22}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalB]_))) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"SubstitutionLemma", 2}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"SubstitutionLemma", 23} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 18}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 2}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
             (\[FormalA] \[CenterDot] \[FormalB]))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 21} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 20}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalA]_))) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          ((\[FormalB]_) \[CenterDot] (\[FormalA]_)), "MatchingConstruct" -> 
         {"SubstitutionLemma", 19}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) <-> 
          (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)), "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 22} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
            \[FormalB])) \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB])))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 22}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalB]_))) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"SubstitutionLemma", 19}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) <-> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalB]_)), "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 24} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
            \[FormalB])) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 22}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 23}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
           (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
              \[FormalB])) \[CenterDot] \[FormalA]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 23} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalB]) == 
         (\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 19}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) <-> 
          (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)), "Side" -> 2, "Subpattern" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
           (\[FormalB]_)), "MatchingConstruct" -> {"SubstitutionLemma", 2}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 24} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB])))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 20}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalA]_))) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          (\[FormalA]_), "MatchingConstruct" -> {"SubstitutionLemma", 19}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) <-> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalB]_)), "MatchingSide" -> 2, 
        "Position" -> {2, 2}|>|>, {"SubstitutionLemma", 25} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 24}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 20}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalB]_))) -> 
          \[FormalA] \[CenterDot] \[FormalB], "OutputExpression" -> 
         HoldForm[\[FormalA] == ((\[FormalB] \[CenterDot] 
              \[FormalB]) \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 26} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 24}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 2}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
           \[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalB]))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 25} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
            \[FormalB]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 26}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalA] \[CenterDot] \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          ((\[FormalB]_) \[CenterDot] (\[FormalB]_)), "MatchingConstruct" -> 
         {"SubstitutionLemma", 2}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 26} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalC] \[CenterDot] \[FormalC]))) == 
         (\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 7}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (\[FormalC]_))) <-> ((\[FormalB]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalB]_))) \[CenterDot] 
           (\[FormalA]_), "Side" -> 2, "Subpattern" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
           (\[FormalA]_)), "MatchingConstruct" -> {"CriticalPairLemma", 25}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalA]_)) -> 
          \[FormalA] \[CenterDot] \[FormalB], "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 27} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] ((\[FormalB] \[CenterDot] 
            \[FormalB]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 25}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 2}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] ((\[FormalB] \[CenterDot] 
              \[FormalB]) \[CenterDot] \[FormalA])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 27} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalC])) == 
         ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           (\[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalB]))) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 7}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (\[FormalC]_))) <-> ((\[FormalB]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalB]_))) \[CenterDot] 
           (\[FormalA]_), "Side" -> 2, "Subpattern" -> 
         (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 21}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) <-> 
          (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)), "MatchingSide" -> 1, "Position" -> {1, 2}|>|>, 
    {"SubstitutionLemma", 28} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalC])) == \[FormalC] \[CenterDot] 
          \[FormalA]], "Proof" -> <|"Input" -> {"CriticalPairLemma", 27}, 
        "Position" -> {1}, "Construct" -> {"CriticalPairLemma", 21}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalA]_))) -> \[FormalB], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] 
            ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
             (\[FormalA] \[CenterDot] \[FormalC])) == \[FormalC] \[CenterDot] 
            \[FormalA]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 28} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalA]) == 
         (\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 21}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) <-> 
          (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)), "Side" -> 2, "Subpattern" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
           (\[FormalB]_)), "MatchingConstruct" -> {"SubstitutionLemma", 2}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 29} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalA])) \[CenterDot] \[FormalC] == \[FormalC] \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalC])))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 7}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (\[FormalC]_))) <-> ((\[FormalB]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalB]_))) \[CenterDot] 
           (\[FormalA]_), "Side" -> 1, "Subpattern" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalC]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 21}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) <-> 
          (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)), "MatchingSide" -> 2, "Position" -> {2, 2}|>|>, 
    {"SubstitutionLemma", 29} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] \[FormalC] == \[FormalC] \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalC])))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 29}, "Position" -> {1}, 
        "Construct" -> {"CriticalPairLemma", 25}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalA]_)) -> 
          \[FormalA] \[CenterDot] \[FormalB], "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalC] == \[FormalC] \[CenterDot] (\[FormalA] \[CenterDot] 
             (\[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] 
               \[FormalC])))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"CriticalPairLemma", 30} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB])) == 
         ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           ((\[FormalC] \[CenterDot] \[FormalC]) \[CenterDot] 
            \[FormalB])) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 7}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (\[FormalC]_))) <-> ((\[FormalB]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalB]_))) \[CenterDot] 
           (\[FormalA]_), "Side" -> 2, "Subpattern" -> 
         (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 23}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) <-> 
          ((\[FormalB]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalA]_), "MatchingSide" -> 1, "Position" -> {1, 2}|>|>, 
    {"SubstitutionLemma", 30} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB])) == \[FormalB] \[CenterDot] 
          \[FormalA]], "Proof" -> <|"Input" -> {"CriticalPairLemma", 30}, 
        "Position" -> {1}, "Construct" -> {"SubstitutionLemma", 27}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] 
            ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
             (\[FormalA] \[CenterDot] \[FormalB])) == \[FormalB] \[CenterDot] 
            \[FormalA]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 31} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalB] \[CenterDot] ((\[FormalB] \[CenterDot] 
            \[FormalA]) \[CenterDot] (\[FormalC] \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 28}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalC]_))) -> 
          \[FormalC] \[CenterDot] \[FormalA], "Side" -> 1, 
        "Subpattern" -> ((\[FormalB]_) \[CenterDot] 
           (\[FormalC]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
           (\[FormalC]_)), "MatchingConstruct" -> {"SubstitutionLemma", 2}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 32} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] \[FormalC] == \[FormalC] \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB])))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 28}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalC]_))) -> 
          \[FormalC] \[CenterDot] \[FormalA], "Side" -> 1, 
        "Subpattern" -> (\[FormalB]_) \[CenterDot] (\[FormalC]_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 6}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"CriticalPairLemma", 33} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalB]) \[CenterDot] (\[FormalC] \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 31}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalB]_))) -> 
          \[FormalB] \[CenterDot] \[FormalA], "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 2}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2, 1}|>|>, {"CriticalPairLemma", 34} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] \[FormalC] == \[FormalC] \[CenterDot] 
          ((\[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalB])) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 26}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] (\[FormalC]_)))) -> 
          (\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
            (\[FormalC]_))), "MatchingConstruct" -> {"SubstitutionLemma", 2}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 35} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] \[FormalC] == \[FormalC] \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalC] \[CenterDot] \[FormalB])))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 26}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] (\[FormalC]_)))) -> 
          (\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          ((\[FormalC]_) \[CenterDot] (\[FormalC]_)), "MatchingConstruct" -> 
         {"SubstitutionLemma", 19}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) <-> 
          (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)), "MatchingSide" -> 2, "Position" -> {2, 2}|>|>, 
    {"CriticalPairLemma", 36} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] \[FormalC] == 
         (\[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
             \[FormalC]) \[CenterDot] \[FormalA])) \[CenterDot] \[FormalC]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 29}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] (\[FormalA]_)))) -> 
          (\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalA]_)))), 
        "MatchingConstruct" -> {"SubstitutionLemma", 7}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalC]_))) <-> 
          ((\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalB]_))) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 37} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] \[FormalC] == \[FormalC] \[CenterDot] 
          (((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalC]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 34}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
              (\[FormalB]_))) \[CenterDot] (\[FormalC]_)) -> 
          (\[FormalC] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          ((\[FormalB]_) \[CenterDot] (\[FormalB]_)), "MatchingConstruct" -> 
         {"SubstitutionLemma", 2}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2, 1}|>|>, {"CriticalPairLemma", 38} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] \[FormalC] == 
         (\[FormalA] \[CenterDot] ((\[FormalC] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA])) \[CenterDot] \[FormalC]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 35}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             ((\[FormalA]_) \[CenterDot] (\[FormalC]_)))) -> 
          (\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalC]_)))), 
        "MatchingConstruct" -> {"SubstitutionLemma", 7}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalC]_))) <-> 
          ((\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalB]_))) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 39} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
            (\[FormalD] \[CenterDot] \[FormalB]))) \[CenterDot] \[FormalC] == 
         \[FormalC] \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalC])))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 35}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             ((\[FormalA]_) \[CenterDot] (\[FormalC]_)))) -> 
          (\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalC]_), "MatchingConstruct" -> {"CriticalPairLemma", 33}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalB] \[CenterDot] \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2, 2, 2}|>|>, 
    {"SubstitutionLemma", 31} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
            (\[FormalD] \[CenterDot] \[FormalB]))) \[CenterDot] \[FormalC] == 
         (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalC]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 39}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 29}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
              (\[FormalA]_)))) -> (\[FormalB] \[CenterDot] 
            \[FormalC]) \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
               \[FormalC]) \[CenterDot] (\[FormalD] \[CenterDot] 
               \[FormalB]))) \[CenterDot] \[FormalC] == 
           (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalC]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 40} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalC]))) \[CenterDot] \[FormalC] == \[FormalC] \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalC] \[CenterDot] \[FormalC])))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 35}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             ((\[FormalA]_) \[CenterDot] (\[FormalC]_)))) -> 
          (\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalC]_), "MatchingConstruct" -> {"CriticalPairLemma", 8}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalA]_))) -> 
          \[FormalA] \[CenterDot] \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2, 2, 2}|>|>, {"SubstitutionLemma", 32} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalC]))) \[CenterDot] \[FormalC] == 
         (\[FormalA] \[CenterDot] \[FormalC]) \[CenterDot] \[FormalC]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 40}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 26}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
              (\[FormalC]_)))) -> (\[FormalB] \[CenterDot] 
            \[FormalC]) \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
              (\[FormalB] \[CenterDot] \[FormalC]))) \[CenterDot] 
            \[FormalC] == (\[FormalA] \[CenterDot] \[FormalC]) \[CenterDot] 
            \[FormalC]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 33} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] \[FormalC] == \[FormalC] \[CenterDot] 
          (\[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
             \[FormalC]) \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 36}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 2}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalC] == \[FormalC] \[CenterDot] (\[FormalA] \[CenterDot] 
             ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
              \[FormalA]))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 41} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] \[FormalC] == \[FormalC] \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalC])))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 33}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] (\[FormalB]_))) -> 
          (\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> ((\[FormalC]_) \[CenterDot] 
           (\[FormalA]_)) \[CenterDot] (\[FormalB]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 2}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"SubstitutionLemma", 34} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] \[FormalC] == \[FormalC] \[CenterDot] 
          (\[FormalA] \[CenterDot] ((\[FormalC] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 38}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 2}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalC] == \[FormalC] \[CenterDot] (\[FormalA] \[CenterDot] 
             ((\[FormalC] \[CenterDot] \[FormalB]) \[CenterDot] 
              \[FormalA]))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 35} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalC] \[CenterDot] \[FormalA]))) == 
         (\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 32}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 2}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             (\[FormalC] \[CenterDot] (\[FormalC] \[CenterDot] 
               \[FormalA]))) == (\[FormalB] \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalA]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 36} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalC] \[CenterDot] \[FormalA]))) == \[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 35}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 2}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             (\[FormalC] \[CenterDot] (\[FormalC] \[CenterDot] 
               \[FormalA]))) == \[FormalA] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalA])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 42} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalA]) \[CenterDot] \[FormalA]) == 
         ((\[FormalC] \[CenterDot] (\[FormalC] \[CenterDot] 
             \[FormalA])) \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 36}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] (\[FormalA]_)))) -> 
          \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalA]), 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          ((\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalA]_)))), 
        "MatchingConstruct" -> {"CriticalPairLemma", 37}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((((\[FormalB]_) \[CenterDot] 
              (\[FormalB]_)) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
            (\[FormalC]_)) -> (\[FormalC] \[CenterDot] 
            \[FormalB]) \[CenterDot] \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"SubstitutionLemma", 37} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] \[FormalA] == 
         ((\[FormalC] \[CenterDot] (\[FormalC] \[CenterDot] 
             \[FormalA])) \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 42}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 37}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] ((((\[FormalB]_) \[CenterDot] 
              (\[FormalB]_)) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
            (\[FormalC]_)) -> (\[FormalC] \[CenterDot] 
            \[FormalB]) \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalA] == ((\[FormalC] \[CenterDot] (\[FormalC] \[CenterDot] 
               \[FormalA])) \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalA]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"CriticalPairLemma", 43} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) == 
         (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
            \[FormalC])) \[CenterDot] (\[FormalA] \[CenterDot] 
           ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 41}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] (\[FormalA]_)))) -> 
          (\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          ((\[FormalC]_) \[CenterDot] (\[FormalA]_)), "MatchingConstruct" -> 
         {"CriticalPairLemma", 32}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             ((\[FormalB]_) \[CenterDot] (\[FormalC]_)))) -> 
          (\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"SubstitutionLemma", 38} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) == \[FormalA]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 43}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 19}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalC])) == 
           \[FormalA]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 44} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 38}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"SubstitutionLemma", 2}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 45} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CenterDot] 
           \[FormalA]) \[CenterDot] ((\[FormalB] \[CenterDot] 
            \[FormalC]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 44}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalC]_))) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] (\[FormalC]_)), "MatchingConstruct" -> 
         {"SubstitutionLemma", 34}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
              (\[FormalC]_)) \[CenterDot] (\[FormalB]_))) -> 
          (\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 39} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalB]) == 
         ((\[FormalC] \[CenterDot] (\[FormalC] \[CenterDot] 
             \[FormalA])) \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 37}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 2}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
             \[FormalB]) == ((\[FormalC] \[CenterDot] 
              (\[FormalC] \[CenterDot] \[FormalA])) \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 40} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalB]) == \[FormalA] \[CenterDot] 
          ((\[FormalC] \[CenterDot] (\[FormalC] \[CenterDot] 
             \[FormalA])) \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 39}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 2}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
             \[FormalB]) == \[FormalA] \[CenterDot] ((\[FormalC] \[CenterDot] 
              (\[FormalC] \[CenterDot] \[FormalA])) \[CenterDot] 
             \[FormalB])], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 46} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         (((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalA])) \[CenterDot] 
           \[FormalC]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 8}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
             (\[FormalA]_))) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
            (\[FormalC]_)) -> \[FormalC], "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
           (\[FormalA]_)), "MatchingConstruct" -> {"CriticalPairLemma", 28}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalA]_)) <-> ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 41} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] ((\[FormalC] \[CenterDot] 
             \[FormalA]) \[CenterDot] (\[FormalD] \[CenterDot] 
             \[FormalC]))) == (\[FormalB] \[CenterDot] 
           \[FormalC]) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 31}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 2}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             ((\[FormalC] \[CenterDot] \[FormalA]) \[CenterDot] 
              (\[FormalD] \[CenterDot] \[FormalC]))) == 
           (\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] \[FormalA]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 47} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] \[FormalC] == \[FormalC] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 41}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
              (\[FormalA]_)) \[CenterDot] ((\[FormalD]_) \[CenterDot] 
              (\[FormalC]_)))) -> (\[FormalB] \[CenterDot] 
            \[FormalC]) \[CenterDot] \[FormalA], "Side" -> 1, 
        "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          (((\[FormalC]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalD]_) \[CenterDot] (\[FormalC]_))), 
        "MatchingConstruct" -> {"SubstitutionLemma", 30}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalB] \[CenterDot] \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 48} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] \[FormalC] == \[FormalC] \[CenterDot] 
          (\[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
             \[FormalD]) \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalC])))], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 41}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (((\[FormalC]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
             ((\[FormalD]_) \[CenterDot] (\[FormalC]_)))) -> 
          (\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> ((\[FormalC]_) \[CenterDot] 
           (\[FormalA]_)) \[CenterDot] ((\[FormalD]_) \[CenterDot] 
           (\[FormalC]_)), "MatchingConstruct" -> {"CriticalPairLemma", 37}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((((\[FormalB]_) \[CenterDot] 
              (\[FormalB]_)) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
            (\[FormalC]_)) -> (\[FormalC] \[CenterDot] 
            \[FormalB]) \[CenterDot] \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"SubstitutionLemma", 42} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalB] \[CenterDot] 
           ((\[FormalC] \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalC] \[CenterDot] \[FormalA])))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 46}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 47}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalC]_) -> \[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] 
            \[FormalA]), "OutputExpression" -> HoldForm[
          \[FormalA] == (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalB] \[CenterDot] ((\[FormalC] \[CenterDot] 
               \[FormalA]) \[CenterDot] (\[FormalC] \[CenterDot] 
               \[FormalA])))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 49} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) == 
         (\[FormalC] \[CenterDot] (\[FormalA] \[CenterDot] 
            ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalC])))) \[CenterDot] 
          \[FormalC]], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 45}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           (\[FormalC]_)) \[CenterDot] (\[FormalB]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 42}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (((\[FormalC]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] (\[FormalA]_)))) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 43} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) == 
         ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           \[FormalC]) \[CenterDot] \[FormalC]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 49}, "Position" -> {1}, 
        "Construct" -> {"CriticalPairLemma", 48}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (((\[FormalC]_) \[CenterDot] (\[FormalD]_)) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] (\[FormalA]_)))) -> 
          (\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] 
            ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalC])) == 
           ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
             \[FormalC]) \[CenterDot] \[FormalC]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 44} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) == \[FormalC] \[CenterDot] 
          (\[FormalC] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 43}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 47}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalC]_) -> \[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] 
            \[FormalA]), "OutputExpression" -> HoldForm[
          \[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
              \[FormalC]) \[CenterDot] (\[FormalB] \[CenterDot] 
              \[FormalC])) == \[FormalC] \[CenterDot] 
            (\[FormalC] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB]))], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 45} -> 
     <|"Statement" -> HoldForm[
        (((\[FormalH] \[CenterDot] (\[FormalH] \[CenterDot] 
              \[FormalH])) \[CenterDot] (\[FormalF] \[CenterDot] 
             \[FormalG])) \[CenterDot] \[FormalH]) \[CenterDot] 
          (((\[FormalF] \[CenterDot] \[FormalG]) \[CenterDot] 
            (\[FormalF] \[CenterDot] \[FormalG])) \[CenterDot] \[FormalH]) == 
         (\[FormalF] \[CenterDot] ((\[FormalG] \[CenterDot] 
             \[FormalH]) \[CenterDot] (\[FormalG] \[CenterDot] 
             \[FormalH]))) \[CenterDot] (\[FormalF] \[CenterDot] 
           ((\[FormalG] \[CenterDot] \[FormalH]) \[CenterDot] 
            (\[FormalG] \[CenterDot] \[FormalH])))], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Position" -> {1, 1}, 
        "Construct" -> {"CriticalPairLemma", 9}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalA]_) -> 
          (\[FormalH] \[CenterDot] (\[FormalH] \[CenterDot] 
             \[FormalH])) \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(((\[FormalH] \[CenterDot] (\[FormalH] \[CenterDot] 
                \[FormalH])) \[CenterDot] (\[FormalF] \[CenterDot] 
               \[FormalG])) \[CenterDot] \[FormalH]) \[CenterDot] 
            (((\[FormalF] \[CenterDot] \[FormalG]) \[CenterDot] 
              (\[FormalF] \[CenterDot] \[FormalG])) \[CenterDot] 
             \[FormalH]) == (\[FormalF] \[CenterDot] 
             ((\[FormalG] \[CenterDot] \[FormalH]) \[CenterDot] 
              (\[FormalG] \[CenterDot] \[FormalH]))) \[CenterDot] 
            (\[FormalF] \[CenterDot] ((\[FormalG] \[CenterDot] 
               \[FormalH]) \[CenterDot] (\[FormalG] \[CenterDot] 
               \[FormalH])))], "ConstructSide" -> 2, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 46} -> 
     <|"Statement" -> HoldForm[
        (((\[FormalH] \[CenterDot] (\[FormalH] \[CenterDot] 
              \[FormalH])) \[CenterDot] (\[FormalF] \[CenterDot] 
             \[FormalG])) \[CenterDot] \[FormalH]) \[CenterDot] 
          (((\[FormalF] \[CenterDot] \[FormalG]) \[CenterDot] 
            (\[FormalF] \[CenterDot] \[FormalG])) \[CenterDot] \[FormalH]) == 
         (\[FormalF] \[CenterDot] ((\[FormalH] \[CenterDot] 
             (\[FormalH] \[CenterDot] \[FormalH])) \[CenterDot] 
            (\[FormalG] \[CenterDot] \[FormalH]))) \[CenterDot] 
          (\[FormalF] \[CenterDot] ((\[FormalG] \[CenterDot] 
             \[FormalH]) \[CenterDot] (\[FormalG] \[CenterDot] 
             \[FormalH])))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 45}, "Position" -> {1, 2}, 
        "Construct" -> {"CriticalPairLemma", 9}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalA]_) -> 
          (\[FormalH] \[CenterDot] (\[FormalH] \[CenterDot] 
             \[FormalH])) \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(((\[FormalH] \[CenterDot] (\[FormalH] \[CenterDot] 
                \[FormalH])) \[CenterDot] (\[FormalF] \[CenterDot] 
               \[FormalG])) \[CenterDot] \[FormalH]) \[CenterDot] 
            (((\[FormalF] \[CenterDot] \[FormalG]) \[CenterDot] 
              (\[FormalF] \[CenterDot] \[FormalG])) \[CenterDot] 
             \[FormalH]) == (\[FormalF] \[CenterDot] 
             ((\[FormalH] \[CenterDot] (\[FormalH] \[CenterDot] 
                \[FormalH])) \[CenterDot] (\[FormalG] \[CenterDot] 
               \[FormalH]))) \[CenterDot] (\[FormalF] \[CenterDot] 
             ((\[FormalG] \[CenterDot] \[FormalH]) \[CenterDot] 
              (\[FormalG] \[CenterDot] \[FormalH])))], "ConstructSide" -> 2, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 47} -> 
     <|"Statement" -> HoldForm[
        (((\[FormalF] \[CenterDot] \[FormalG]) \[CenterDot] 
            (\[FormalF] \[CenterDot] \[FormalG])) \[CenterDot] 
           \[FormalH]) \[CenterDot] (\[FormalH] \[CenterDot] 
           ((\[FormalH] \[CenterDot] (\[FormalH] \[CenterDot] 
              \[FormalH])) \[CenterDot] (\[FormalF] \[CenterDot] 
             \[FormalG]))) == (\[FormalF] \[CenterDot] 
           ((\[FormalH] \[CenterDot] (\[FormalH] \[CenterDot] 
              \[FormalH])) \[CenterDot] (\[FormalG] \[CenterDot] 
             \[FormalH]))) \[CenterDot] (\[FormalF] \[CenterDot] 
           ((\[FormalG] \[CenterDot] \[FormalH]) \[CenterDot] 
            (\[FormalG] \[CenterDot] \[FormalH])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 46}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 47}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalC]_) -> \[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] 
            \[FormalA]), "OutputExpression" -> HoldForm[
          (((\[FormalF] \[CenterDot] \[FormalG]) \[CenterDot] 
              (\[FormalF] \[CenterDot] \[FormalG])) \[CenterDot] 
             \[FormalH]) \[CenterDot] (\[FormalH] \[CenterDot] 
             ((\[FormalH] \[CenterDot] (\[FormalH] \[CenterDot] 
                \[FormalH])) \[CenterDot] (\[FormalF] \[CenterDot] 
               \[FormalG]))) == (\[FormalF] \[CenterDot] 
             ((\[FormalH] \[CenterDot] (\[FormalH] \[CenterDot] 
                \[FormalH])) \[CenterDot] (\[FormalG] \[CenterDot] 
               \[FormalH]))) \[CenterDot] (\[FormalF] \[CenterDot] 
             ((\[FormalG] \[CenterDot] \[FormalH]) \[CenterDot] 
              (\[FormalG] \[CenterDot] \[FormalH])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 48} -> 
     <|"Statement" -> HoldForm[(\[FormalH] \[CenterDot] 
           ((\[FormalF] \[CenterDot] \[FormalG]) \[CenterDot] 
            (\[FormalF] \[CenterDot] \[FormalG]))) \[CenterDot] 
          (\[FormalH] \[CenterDot] ((\[FormalH] \[CenterDot] 
             (\[FormalH] \[CenterDot] \[FormalH])) \[CenterDot] 
            (\[FormalF] \[CenterDot] \[FormalG]))) == 
         (\[FormalF] \[CenterDot] ((\[FormalH] \[CenterDot] 
             (\[FormalH] \[CenterDot] \[FormalH])) \[CenterDot] 
            (\[FormalG] \[CenterDot] \[FormalH]))) \[CenterDot] 
          (\[FormalF] \[CenterDot] ((\[FormalG] \[CenterDot] 
             \[FormalH]) \[CenterDot] (\[FormalG] \[CenterDot] 
             \[FormalH])))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 47}, "Position" -> {1}, 
        "Construct" -> {"CriticalPairLemma", 47}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalC]_) -> \[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] 
            \[FormalA]), "OutputExpression" -> HoldForm[
          (\[FormalH] \[CenterDot] ((\[FormalF] \[CenterDot] 
               \[FormalG]) \[CenterDot] (\[FormalF] \[CenterDot] 
               \[FormalG]))) \[CenterDot] (\[FormalH] \[CenterDot] 
             ((\[FormalH] \[CenterDot] (\[FormalH] \[CenterDot] 
                \[FormalH])) \[CenterDot] (\[FormalF] \[CenterDot] 
               \[FormalG]))) == (\[FormalF] \[CenterDot] 
             ((\[FormalH] \[CenterDot] (\[FormalH] \[CenterDot] 
                \[FormalH])) \[CenterDot] (\[FormalG] \[CenterDot] 
               \[FormalH]))) \[CenterDot] (\[FormalF] \[CenterDot] 
             ((\[FormalG] \[CenterDot] \[FormalH]) \[CenterDot] 
              (\[FormalG] \[CenterDot] \[FormalH])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 49} -> 
     <|"Statement" -> HoldForm[(\[FormalG] \[CenterDot] 
           (\[FormalG] \[CenterDot] (\[FormalH] \[CenterDot] 
             \[FormalF]))) \[CenterDot] (\[FormalH] \[CenterDot] 
           ((\[FormalH] \[CenterDot] (\[FormalH] \[CenterDot] 
              \[FormalH])) \[CenterDot] (\[FormalF] \[CenterDot] 
             \[FormalG]))) == (\[FormalF] \[CenterDot] 
           ((\[FormalH] \[CenterDot] (\[FormalH] \[CenterDot] 
              \[FormalH])) \[CenterDot] (\[FormalG] \[CenterDot] 
             \[FormalH]))) \[CenterDot] (\[FormalF] \[CenterDot] 
           ((\[FormalG] \[CenterDot] \[FormalH]) \[CenterDot] 
            (\[FormalG] \[CenterDot] \[FormalH])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 48}, "Position" -> {1}, 
        "Construct" -> {"SubstitutionLemma", 44}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) -> \[FormalC] \[CenterDot] 
           (\[FormalC] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB])), 
        "OutputExpression" -> HoldForm[(\[FormalG] \[CenterDot] 
             (\[FormalG] \[CenterDot] (\[FormalH] \[CenterDot] 
               \[FormalF]))) \[CenterDot] (\[FormalH] \[CenterDot] 
             ((\[FormalH] \[CenterDot] (\[FormalH] \[CenterDot] 
                \[FormalH])) \[CenterDot] (\[FormalF] \[CenterDot] 
               \[FormalG]))) == (\[FormalF] \[CenterDot] 
             ((\[FormalH] \[CenterDot] (\[FormalH] \[CenterDot] 
                \[FormalH])) \[CenterDot] (\[FormalG] \[CenterDot] 
               \[FormalH]))) \[CenterDot] (\[FormalF] \[CenterDot] 
             ((\[FormalG] \[CenterDot] \[FormalH]) \[CenterDot] 
              (\[FormalG] \[CenterDot] \[FormalH])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 50} -> 
     <|"Statement" -> HoldForm[(\[FormalG] \[CenterDot] 
           (\[FormalG] \[CenterDot] (\[FormalH] \[CenterDot] 
             \[FormalF]))) \[CenterDot] (\[FormalH] \[CenterDot] 
           (\[FormalH] \[CenterDot] (\[FormalF] \[CenterDot] \[FormalG]))) == 
         (\[FormalF] \[CenterDot] ((\[FormalH] \[CenterDot] 
             (\[FormalH] \[CenterDot] \[FormalH])) \[CenterDot] 
            (\[FormalG] \[CenterDot] \[FormalH]))) \[CenterDot] 
          (\[FormalF] \[CenterDot] ((\[FormalG] \[CenterDot] 
             \[FormalH]) \[CenterDot] (\[FormalG] \[CenterDot] 
             \[FormalH])))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 49}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 40}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             ((\[FormalB]_) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
            (\[FormalC]_)) -> \[FormalA] \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalC]), "OutputExpression" -> 
         HoldForm[(\[FormalG] \[CenterDot] (\[FormalG] \[CenterDot] 
              (\[FormalH] \[CenterDot] \[FormalF]))) \[CenterDot] 
            (\[FormalH] \[CenterDot] (\[FormalH] \[CenterDot] 
              (\[FormalF] \[CenterDot] \[FormalG]))) == 
           (\[FormalF] \[CenterDot] ((\[FormalH] \[CenterDot] (
                \[FormalH] \[CenterDot] \[FormalH])) \[CenterDot] 
              (\[FormalG] \[CenterDot] \[FormalH]))) \[CenterDot] 
            (\[FormalF] \[CenterDot] ((\[FormalG] \[CenterDot] 
               \[FormalH]) \[CenterDot] (\[FormalG] \[CenterDot] 
               \[FormalH])))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 51} -> 
     <|"Statement" -> HoldForm[(\[FormalH] \[CenterDot] 
           (\[FormalH] \[CenterDot] (\[FormalF] \[CenterDot] 
             \[FormalG]))) \[CenterDot] ((\[FormalG] \[CenterDot] 
            (\[FormalH] \[CenterDot] \[FormalF])) \[CenterDot] \[FormalG]) == 
         (\[FormalF] \[CenterDot] ((\[FormalH] \[CenterDot] 
             (\[FormalH] \[CenterDot] \[FormalH])) \[CenterDot] 
            (\[FormalG] \[CenterDot] \[FormalH]))) \[CenterDot] 
          (\[FormalF] \[CenterDot] ((\[FormalG] \[CenterDot] 
             \[FormalH]) \[CenterDot] (\[FormalG] \[CenterDot] 
             \[FormalH])))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 50}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 47}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalC]_) -> \[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] 
            \[FormalA]), "OutputExpression" -> HoldForm[
          (\[FormalH] \[CenterDot] (\[FormalH] \[CenterDot] 
              (\[FormalF] \[CenterDot] \[FormalG]))) \[CenterDot] 
            ((\[FormalG] \[CenterDot] (\[FormalH] \[CenterDot] 
               \[FormalF])) \[CenterDot] \[FormalG]) == 
           (\[FormalF] \[CenterDot] ((\[FormalH] \[CenterDot] (
                \[FormalH] \[CenterDot] \[FormalH])) \[CenterDot] 
              (\[FormalG] \[CenterDot] \[FormalH]))) \[CenterDot] 
            (\[FormalF] \[CenterDot] ((\[FormalG] \[CenterDot] 
               \[FormalH]) \[CenterDot] (\[FormalG] \[CenterDot] 
               \[FormalH])))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 52} -> 
     <|"Statement" -> HoldForm[(\[FormalH] \[CenterDot] 
           (\[FormalH] \[CenterDot] (\[FormalG] \[CenterDot] 
             \[FormalF]))) \[CenterDot] ((\[FormalG] \[CenterDot] 
            (\[FormalH] \[CenterDot] \[FormalF])) \[CenterDot] \[FormalG]) == 
         (\[FormalF] \[CenterDot] ((\[FormalH] \[CenterDot] 
             (\[FormalH] \[CenterDot] \[FormalH])) \[CenterDot] 
            (\[FormalG] \[CenterDot] \[FormalH]))) \[CenterDot] 
          (\[FormalF] \[CenterDot] ((\[FormalG] \[CenterDot] 
             \[FormalH]) \[CenterDot] (\[FormalG] \[CenterDot] 
             \[FormalH])))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 51}, "Position" -> {1, 2, 2}, 
        "Construct" -> {"SubstitutionLemma", 2}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalH] \[CenterDot] (\[FormalH] \[CenterDot] 
              (\[FormalG] \[CenterDot] \[FormalF]))) \[CenterDot] 
            ((\[FormalG] \[CenterDot] (\[FormalH] \[CenterDot] 
               \[FormalF])) \[CenterDot] \[FormalG]) == 
           (\[FormalF] \[CenterDot] ((\[FormalH] \[CenterDot] (
                \[FormalH] \[CenterDot] \[FormalH])) \[CenterDot] 
              (\[FormalG] \[CenterDot] \[FormalH]))) \[CenterDot] 
            (\[FormalF] \[CenterDot] ((\[FormalG] \[CenterDot] 
               \[FormalH]) \[CenterDot] (\[FormalG] \[CenterDot] 
               \[FormalH])))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 53} -> 
     <|"Statement" -> HoldForm[(\[FormalH] \[CenterDot] 
           (\[FormalH] \[CenterDot] (\[FormalG] \[CenterDot] 
             \[FormalF]))) \[CenterDot] (\[FormalG] \[CenterDot] 
           ((\[FormalH] \[CenterDot] \[FormalF]) \[CenterDot] \[FormalG])) == 
         (\[FormalF] \[CenterDot] ((\[FormalH] \[CenterDot] 
             (\[FormalH] \[CenterDot] \[FormalH])) \[CenterDot] 
            (\[FormalG] \[CenterDot] \[FormalH]))) \[CenterDot] 
          (\[FormalF] \[CenterDot] ((\[FormalG] \[CenterDot] 
             \[FormalH]) \[CenterDot] (\[FormalG] \[CenterDot] 
             \[FormalH])))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 52}, "Position" -> {2}, 
        "Construct" -> {"CriticalPairLemma", 47}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalC]_) -> \[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] 
            \[FormalA]), "OutputExpression" -> HoldForm[
          (\[FormalH] \[CenterDot] (\[FormalH] \[CenterDot] 
              (\[FormalG] \[CenterDot] \[FormalF]))) \[CenterDot] 
            (\[FormalG] \[CenterDot] ((\[FormalH] \[CenterDot] 
               \[FormalF]) \[CenterDot] \[FormalG])) == 
           (\[FormalF] \[CenterDot] ((\[FormalH] \[CenterDot] (
                \[FormalH] \[CenterDot] \[FormalH])) \[CenterDot] 
              (\[FormalG] \[CenterDot] \[FormalH]))) \[CenterDot] 
            (\[FormalF] \[CenterDot] ((\[FormalG] \[CenterDot] 
               \[FormalH]) \[CenterDot] (\[FormalG] \[CenterDot] 
               \[FormalH])))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 54} -> 
     <|"Statement" -> HoldForm[(\[FormalH] \[CenterDot] 
           (\[FormalH] \[CenterDot] (\[FormalG] \[CenterDot] 
             \[FormalF]))) \[CenterDot] (\[FormalG] \[CenterDot] 
           (\[FormalG] \[CenterDot] (\[FormalF] \[CenterDot] \[FormalH]))) == 
         (\[FormalF] \[CenterDot] ((\[FormalH] \[CenterDot] 
             (\[FormalH] \[CenterDot] \[FormalH])) \[CenterDot] 
            (\[FormalG] \[CenterDot] \[FormalH]))) \[CenterDot] 
          (\[FormalF] \[CenterDot] ((\[FormalG] \[CenterDot] 
             \[FormalH]) \[CenterDot] (\[FormalG] \[CenterDot] 
             \[FormalH])))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 53}, "Position" -> {2, 2}, 
        "Construct" -> {"CriticalPairLemma", 47}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalC]_) -> \[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] 
            \[FormalA]), "OutputExpression" -> HoldForm[
          (\[FormalH] \[CenterDot] (\[FormalH] \[CenterDot] 
              (\[FormalG] \[CenterDot] \[FormalF]))) \[CenterDot] 
            (\[FormalG] \[CenterDot] (\[FormalG] \[CenterDot] 
              (\[FormalF] \[CenterDot] \[FormalH]))) == 
           (\[FormalF] \[CenterDot] ((\[FormalH] \[CenterDot] (
                \[FormalH] \[CenterDot] \[FormalH])) \[CenterDot] 
              (\[FormalG] \[CenterDot] \[FormalH]))) \[CenterDot] 
            (\[FormalF] \[CenterDot] ((\[FormalG] \[CenterDot] 
               \[FormalH]) \[CenterDot] (\[FormalG] \[CenterDot] 
               \[FormalH])))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 55} -> 
     <|"Statement" -> HoldForm[(\[FormalH] \[CenterDot] 
           (\[FormalH] \[CenterDot] (\[FormalG] \[CenterDot] 
             \[FormalF]))) \[CenterDot] (\[FormalG] \[CenterDot] 
           (\[FormalG] \[CenterDot] (\[FormalH] \[CenterDot] \[FormalF]))) == 
         (\[FormalF] \[CenterDot] ((\[FormalH] \[CenterDot] 
             (\[FormalH] \[CenterDot] \[FormalH])) \[CenterDot] 
            (\[FormalG] \[CenterDot] \[FormalH]))) \[CenterDot] 
          (\[FormalF] \[CenterDot] ((\[FormalG] \[CenterDot] 
             \[FormalH]) \[CenterDot] (\[FormalG] \[CenterDot] 
             \[FormalH])))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 54}, "Position" -> {2, 2, 2}, 
        "Construct" -> {"SubstitutionLemma", 2}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalH] \[CenterDot] (\[FormalH] \[CenterDot] 
              (\[FormalG] \[CenterDot] \[FormalF]))) \[CenterDot] 
            (\[FormalG] \[CenterDot] (\[FormalG] \[CenterDot] 
              (\[FormalH] \[CenterDot] \[FormalF]))) == 
           (\[FormalF] \[CenterDot] ((\[FormalH] \[CenterDot] (
                \[FormalH] \[CenterDot] \[FormalH])) \[CenterDot] 
              (\[FormalG] \[CenterDot] \[FormalH]))) \[CenterDot] 
            (\[FormalF] \[CenterDot] ((\[FormalG] \[CenterDot] 
               \[FormalH]) \[CenterDot] (\[FormalG] \[CenterDot] 
               \[FormalH])))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 56} -> 
     <|"Statement" -> HoldForm[(\[FormalH] \[CenterDot] 
           (\[FormalH] \[CenterDot] (\[FormalG] \[CenterDot] 
             \[FormalF]))) \[CenterDot] (\[FormalG] \[CenterDot] 
           (\[FormalG] \[CenterDot] (\[FormalH] \[CenterDot] \[FormalF]))) == 
         (\[FormalF] \[CenterDot] ((\[FormalG] \[CenterDot] 
             \[FormalH]) \[CenterDot] (\[FormalG] \[CenterDot] 
             \[FormalH]))) \[CenterDot] (((\[FormalH] \[CenterDot] 
             (\[FormalH] \[CenterDot] \[FormalH])) \[CenterDot] 
            (\[FormalG] \[CenterDot] \[FormalH])) \[CenterDot] \[FormalF])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 55}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 47}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalC]_) -> \[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] 
            \[FormalA]), "OutputExpression" -> HoldForm[
          (\[FormalH] \[CenterDot] (\[FormalH] \[CenterDot] 
              (\[FormalG] \[CenterDot] \[FormalF]))) \[CenterDot] 
            (\[FormalG] \[CenterDot] (\[FormalG] \[CenterDot] 
              (\[FormalH] \[CenterDot] \[FormalF]))) == 
           (\[FormalF] \[CenterDot] ((\[FormalG] \[CenterDot] 
               \[FormalH]) \[CenterDot] (\[FormalG] \[CenterDot] 
               \[FormalH]))) \[CenterDot] (((\[FormalH] \[CenterDot] (
                \[FormalH] \[CenterDot] \[FormalH])) \[CenterDot] 
              (\[FormalG] \[CenterDot] \[FormalH])) \[CenterDot] 
             \[FormalF])], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 57} -> 
     <|"Statement" -> HoldForm[(\[FormalH] \[CenterDot] 
           (\[FormalH] \[CenterDot] (\[FormalG] \[CenterDot] 
             \[FormalF]))) \[CenterDot] (\[FormalG] \[CenterDot] 
           (\[FormalG] \[CenterDot] (\[FormalH] \[CenterDot] \[FormalF]))) == 
         (\[FormalH] \[CenterDot] (\[FormalH] \[CenterDot] 
            (\[FormalF] \[CenterDot] \[FormalG]))) \[CenterDot] 
          (((\[FormalH] \[CenterDot] (\[FormalH] \[CenterDot] 
              \[FormalH])) \[CenterDot] (\[FormalG] \[CenterDot] 
             \[FormalH])) \[CenterDot] \[FormalF])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 56}, "Position" -> {1}, 
        "Construct" -> {"SubstitutionLemma", 44}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) -> \[FormalC] \[CenterDot] 
           (\[FormalC] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB])), 
        "OutputExpression" -> HoldForm[(\[FormalH] \[CenterDot] 
             (\[FormalH] \[CenterDot] (\[FormalG] \[CenterDot] 
               \[FormalF]))) \[CenterDot] (\[FormalG] \[CenterDot] 
             (\[FormalG] \[CenterDot] (\[FormalH] \[CenterDot] 
               \[FormalF]))) == (\[FormalH] \[CenterDot] 
             (\[FormalH] \[CenterDot] (\[FormalF] \[CenterDot] 
               \[FormalG]))) \[CenterDot] (((\[FormalH] \[CenterDot] (
                \[FormalH] \[CenterDot] \[FormalH])) \[CenterDot] 
              (\[FormalG] \[CenterDot] \[FormalH])) \[CenterDot] 
             \[FormalF])], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 58} -> 
     <|"Statement" -> HoldForm[(\[FormalH] \[CenterDot] 
           (\[FormalH] \[CenterDot] (\[FormalG] \[CenterDot] 
             \[FormalF]))) \[CenterDot] (\[FormalG] \[CenterDot] 
           (\[FormalG] \[CenterDot] (\[FormalH] \[CenterDot] \[FormalF]))) == 
         (\[FormalH] \[CenterDot] (\[FormalH] \[CenterDot] 
            (\[FormalG] \[CenterDot] \[FormalF]))) \[CenterDot] 
          (((\[FormalH] \[CenterDot] (\[FormalH] \[CenterDot] 
              \[FormalH])) \[CenterDot] (\[FormalG] \[CenterDot] 
             \[FormalH])) \[CenterDot] \[FormalF])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 57}, 
        "Position" -> {1, 2, 2}, "Construct" -> {"SubstitutionLemma", 2}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (\[FormalB]_) -> \[FormalB] \[CenterDot] \[FormalA], 
        "OutputExpression" -> HoldForm[(\[FormalH] \[CenterDot] 
             (\[FormalH] \[CenterDot] (\[FormalG] \[CenterDot] 
               \[FormalF]))) \[CenterDot] (\[FormalG] \[CenterDot] 
             (\[FormalG] \[CenterDot] (\[FormalH] \[CenterDot] 
               \[FormalF]))) == (\[FormalH] \[CenterDot] 
             (\[FormalH] \[CenterDot] (\[FormalG] \[CenterDot] 
               \[FormalF]))) \[CenterDot] (((\[FormalH] \[CenterDot] (
                \[FormalH] \[CenterDot] \[FormalH])) \[CenterDot] 
              (\[FormalG] \[CenterDot] \[FormalH])) \[CenterDot] 
             \[FormalF])], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 59} -> 
     <|"Statement" -> HoldForm[(\[FormalH] \[CenterDot] 
           (\[FormalH] \[CenterDot] (\[FormalG] \[CenterDot] 
             \[FormalF]))) \[CenterDot] (\[FormalG] \[CenterDot] 
           (\[FormalG] \[CenterDot] (\[FormalH] \[CenterDot] \[FormalF]))) == 
         (\[FormalH] \[CenterDot] (\[FormalH] \[CenterDot] 
            (\[FormalG] \[CenterDot] \[FormalF]))) \[CenterDot] 
          (\[FormalF] \[CenterDot] ((\[FormalG] \[CenterDot] 
             \[FormalH]) \[CenterDot] (\[FormalH] \[CenterDot] 
             (\[FormalH] \[CenterDot] \[FormalH]))))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 58}, "Position" -> {2}, 
        "Construct" -> {"CriticalPairLemma", 47}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalC]_) -> \[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] 
            \[FormalA]), "OutputExpression" -> HoldForm[
          (\[FormalH] \[CenterDot] (\[FormalH] \[CenterDot] 
              (\[FormalG] \[CenterDot] \[FormalF]))) \[CenterDot] 
            (\[FormalG] \[CenterDot] (\[FormalG] \[CenterDot] 
              (\[FormalH] \[CenterDot] \[FormalF]))) == 
           (\[FormalH] \[CenterDot] (\[FormalH] \[CenterDot] 
              (\[FormalG] \[CenterDot] \[FormalF]))) \[CenterDot] 
            (\[FormalF] \[CenterDot] ((\[FormalG] \[CenterDot] 
               \[FormalH]) \[CenterDot] (\[FormalH] \[CenterDot] (
                \[FormalH] \[CenterDot] \[FormalH]))))], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 60} -> 
     <|"Statement" -> HoldForm[(\[FormalH] \[CenterDot] 
           (\[FormalH] \[CenterDot] (\[FormalG] \[CenterDot] 
             \[FormalF]))) \[CenterDot] (\[FormalG] \[CenterDot] 
           (\[FormalG] \[CenterDot] (\[FormalH] \[CenterDot] \[FormalF]))) == 
         (\[FormalH] \[CenterDot] (\[FormalH] \[CenterDot] 
            (\[FormalG] \[CenterDot] \[FormalF]))) \[CenterDot] 
          (\[FormalF] \[CenterDot] ((\[FormalG] \[CenterDot] 
             \[FormalH]) \[CenterDot] (\[FormalG] \[CenterDot] 
             \[FormalH])))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 59}, "Position" -> {2, 2}, 
        "Construct" -> {"CriticalPairLemma", 13}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalB]_))) -> 
          \[FormalA] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalH] \[CenterDot] (\[FormalH] \[CenterDot] 
              (\[FormalG] \[CenterDot] \[FormalF]))) \[CenterDot] 
            (\[FormalG] \[CenterDot] (\[FormalG] \[CenterDot] 
              (\[FormalH] \[CenterDot] \[FormalF]))) == 
           (\[FormalH] \[CenterDot] (\[FormalH] \[CenterDot] 
              (\[FormalG] \[CenterDot] \[FormalF]))) \[CenterDot] 
            (\[FormalF] \[CenterDot] ((\[FormalG] \[CenterDot] 
               \[FormalH]) \[CenterDot] (\[FormalG] \[CenterDot] 
               \[FormalH])))], "ConstructSide" -> 2, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 61} -> 
     <|"Statement" -> HoldForm[(\[FormalH] \[CenterDot] 
           (\[FormalH] \[CenterDot] (\[FormalG] \[CenterDot] 
             \[FormalF]))) \[CenterDot] (\[FormalG] \[CenterDot] 
           (\[FormalG] \[CenterDot] (\[FormalH] \[CenterDot] \[FormalF]))) == 
         (\[FormalH] \[CenterDot] (\[FormalH] \[CenterDot] 
            (\[FormalG] \[CenterDot] \[FormalF]))) \[CenterDot] 
          (\[FormalF] \[CenterDot] ((\[FormalG] \[CenterDot] 
             \[FormalH]) \[CenterDot] (\[FormalH] \[CenterDot] 
             \[FormalG])))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 60}, "Position" -> {2, 2}, 
        "Construct" -> {"CriticalPairLemma", 47}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalC]_) -> \[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] 
            \[FormalA]), "OutputExpression" -> HoldForm[
          (\[FormalH] \[CenterDot] (\[FormalH] \[CenterDot] 
              (\[FormalG] \[CenterDot] \[FormalF]))) \[CenterDot] 
            (\[FormalG] \[CenterDot] (\[FormalG] \[CenterDot] 
              (\[FormalH] \[CenterDot] \[FormalF]))) == 
           (\[FormalH] \[CenterDot] (\[FormalH] \[CenterDot] 
              (\[FormalG] \[CenterDot] \[FormalF]))) \[CenterDot] 
            (\[FormalF] \[CenterDot] ((\[FormalG] \[CenterDot] 
               \[FormalH]) \[CenterDot] (\[FormalH] \[CenterDot] 
               \[FormalG])))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 62} -> 
     <|"Statement" -> HoldForm[(\[FormalH] \[CenterDot] 
           (\[FormalH] \[CenterDot] (\[FormalG] \[CenterDot] 
             \[FormalF]))) \[CenterDot] (\[FormalG] \[CenterDot] 
           (\[FormalG] \[CenterDot] (\[FormalH] \[CenterDot] \[FormalF]))) == 
         (\[FormalH] \[CenterDot] (\[FormalH] \[CenterDot] 
            (\[FormalG] \[CenterDot] \[FormalF]))) \[CenterDot] 
          (\[FormalF] \[CenterDot] ((\[FormalH] \[CenterDot] 
             \[FormalG]) \[CenterDot] (\[FormalH] \[CenterDot] 
             \[FormalG])))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 61}, "Position" -> {2, 2}, 
        "Construct" -> {"CriticalPairLemma", 47}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalC]_) -> \[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] 
            \[FormalA]), "OutputExpression" -> HoldForm[
          (\[FormalH] \[CenterDot] (\[FormalH] \[CenterDot] 
              (\[FormalG] \[CenterDot] \[FormalF]))) \[CenterDot] 
            (\[FormalG] \[CenterDot] (\[FormalG] \[CenterDot] 
              (\[FormalH] \[CenterDot] \[FormalF]))) == 
           (\[FormalH] \[CenterDot] (\[FormalH] \[CenterDot] 
              (\[FormalG] \[CenterDot] \[FormalF]))) \[CenterDot] 
            (\[FormalF] \[CenterDot] ((\[FormalH] \[CenterDot] 
               \[FormalG]) \[CenterDot] (\[FormalH] \[CenterDot] 
               \[FormalG])))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 63} -> 
     <|"Statement" -> HoldForm[(\[FormalH] \[CenterDot] 
           (\[FormalH] \[CenterDot] (\[FormalG] \[CenterDot] 
             \[FormalF]))) \[CenterDot] (\[FormalG] \[CenterDot] 
           (\[FormalG] \[CenterDot] (\[FormalH] \[CenterDot] \[FormalF]))) == 
         (\[FormalH] \[CenterDot] (\[FormalH] \[CenterDot] 
            (\[FormalG] \[CenterDot] \[FormalF]))) \[CenterDot] 
          (\[FormalG] \[CenterDot] (\[FormalG] \[CenterDot] 
            (\[FormalF] \[CenterDot] \[FormalH])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 62}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 44}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) -> \[FormalC] \[CenterDot] 
           (\[FormalC] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB])), 
        "OutputExpression" -> HoldForm[(\[FormalH] \[CenterDot] 
             (\[FormalH] \[CenterDot] (\[FormalG] \[CenterDot] 
               \[FormalF]))) \[CenterDot] (\[FormalG] \[CenterDot] 
             (\[FormalG] \[CenterDot] (\[FormalH] \[CenterDot] 
               \[FormalF]))) == (\[FormalH] \[CenterDot] 
             (\[FormalH] \[CenterDot] (\[FormalG] \[CenterDot] 
               \[FormalF]))) \[CenterDot] (\[FormalG] \[CenterDot] 
             (\[FormalG] \[CenterDot] (\[FormalF] \[CenterDot] 
               \[FormalH])))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"Conclusion", 1} -> 
     <|"Statement" -> HoldForm[(\[FormalH] \[CenterDot] 
           (\[FormalH] \[CenterDot] (\[FormalG] \[CenterDot] 
             \[FormalF]))) \[CenterDot] (\[FormalG] \[CenterDot] 
           (\[FormalG] \[CenterDot] (\[FormalH] \[CenterDot] \[FormalF]))) == 
         (\[FormalH] \[CenterDot] (\[FormalH] \[CenterDot] 
            (\[FormalG] \[CenterDot] \[FormalF]))) \[CenterDot] 
          (\[FormalG] \[CenterDot] (\[FormalG] \[CenterDot] 
            (\[FormalH] \[CenterDot] \[FormalF])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 63}, 
        "Position" -> {2, 2, 2}, "Construct" -> {"SubstitutionLemma", 2}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (\[FormalB]_) -> \[FormalB] \[CenterDot] \[FormalA], 
        "OutputExpression" -> HoldForm[(\[FormalH] \[CenterDot] 
             (\[FormalH] \[CenterDot] (\[FormalG] \[CenterDot] 
               \[FormalF]))) \[CenterDot] (\[FormalG] \[CenterDot] 
             (\[FormalG] \[CenterDot] (\[FormalH] \[CenterDot] 
               \[FormalF]))) == (\[FormalH] \[CenterDot] 
             (\[FormalH] \[CenterDot] (\[FormalG] \[CenterDot] 
               \[FormalF]))) \[CenterDot] (\[FormalG] \[CenterDot] 
             (\[FormalG] \[CenterDot] (\[FormalH] \[CenterDot] 
               \[FormalF])))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>}|>]
