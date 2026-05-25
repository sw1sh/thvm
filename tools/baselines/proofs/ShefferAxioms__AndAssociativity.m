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
 {ForAll[\[FormalA], (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
     (\[FormalA] \[CenterDot] \[FormalA]) == \[FormalA]], 
  ForAll[{\[FormalA], \[FormalB]}, 
   \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
       \[FormalB])) == \[FormalA] \[CenterDot] \[FormalA]], 
  ForAll[{\[FormalA], \[FormalB], \[FormalC]}, 
   (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
       \[FormalC])) \[CenterDot] (\[FormalA] \[CenterDot] 
      (\[FormalB] \[CenterDot] \[FormalC])) == 
    ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
      \[FormalA]) \[CenterDot] ((\[FormalC] \[CenterDot] 
       \[FormalC]) \[CenterDot] \[FormalA])]}, 
 <|"Variables" -> {\[FormalA], \[FormalB], \[FormalC], \[FormalD], 
    \[FormalE], \[FormalF], \[FormalG], \[FormalH], \[FormalI]}, 
  "Constants" -> {}, "Proof" -> 
   {{"Axiom", 1} -> <|"Statement" -> HoldForm[\[FormalA] == 
         (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalA])], "Proof" -> <||>|>, 
    {"Axiom", 2} -> <|"Statement" -> HoldForm[
        \[FormalA] \[CenterDot] \[FormalA] == \[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalB]))], 
      "Proof" -> <||>|>, {"Axiom", 3} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalC])) == 
         ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
           \[FormalA]) \[CenterDot] ((\[FormalC] \[CenterDot] 
            \[FormalC]) \[CenterDot] \[FormalA])], "Proof" -> <||>|>, 
    {"Hypothesis", 1} -> <|"Statement" -> 
       HoldForm[(((\[FormalG] \[CenterDot] \[FormalH]) \[CenterDot] 
            (\[FormalG] \[CenterDot] \[FormalH])) \[CenterDot] 
           \[FormalI]) \[CenterDot] (((\[FormalG] \[CenterDot] 
             \[FormalH]) \[CenterDot] (\[FormalG] \[CenterDot] 
             \[FormalH])) \[CenterDot] \[FormalI]) == 
         (\[FormalG] \[CenterDot] ((\[FormalH] \[CenterDot] 
             \[FormalI]) \[CenterDot] (\[FormalH] \[CenterDot] 
             \[FormalI]))) \[CenterDot] (\[FormalG] \[CenterDot] 
           ((\[FormalH] \[CenterDot] \[FormalI]) \[CenterDot] 
            (\[FormalH] \[CenterDot] \[FormalI])))], "Proof" -> <||>|>, 
    {"CriticalPairLemma", 1} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalB] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalB])) == \[FormalA]], 
      "Proof" -> <|"Construct" -> {"Axiom", 2}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalA]_) <-> 
          (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalB]_))), "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CenterDot] (\[FormalA]_), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 2} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         \[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
            \[FormalB]) \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"Axiom", 2}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalA]_) <-> 
          (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalB]_))), "Side" -> 2, 
        "Subpattern" -> (\[FormalB]_) \[CenterDot] (\[FormalB]_), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 3} -> 
     <|"Statement" -> HoldForm[
        ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
           (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalC]))) \[CenterDot] (((\[FormalB] \[CenterDot] 
             \[FormalC]) \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalC])) \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalC]))) == 
         ((\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalC])) \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalC]))) \[CenterDot] 
          (((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalA]) \[CenterDot] ((\[FormalC] \[CenterDot] 
             \[FormalC]) \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Construct" -> {"Axiom", 3}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) <-> 
          (((\[FormalB]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] (\[FormalA]_)), "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          ((\[FormalB]_) \[CenterDot] (\[FormalC]_)), "MatchingConstruct" -> 
         {"Axiom", 3}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) <-> 
          (((\[FormalB]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] (\[FormalA]_)), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 4} -> 
     <|"Statement" -> HoldForm[
        (((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalA])) \[CenterDot] 
           ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalC])) \[CenterDot] ((\[FormalC] \[CenterDot] 
            \[FormalC]) \[CenterDot] ((\[FormalB] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalC])) == 
         ((\[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalA])) \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalA]))) \[CenterDot] 
          (((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalC]) \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalC]))], 
      "Proof" -> <|"Construct" -> {"Axiom", 3}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) <-> 
          (((\[FormalB]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] (\[FormalA]_)), "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          ((\[FormalB]_) \[CenterDot] (\[FormalC]_)), "MatchingConstruct" -> 
         {"Axiom", 3}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) <-> 
          (((\[FormalB]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] (\[FormalA]_)), "MatchingSide" -> 2, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalC])) \[CenterDot] ((\[FormalC] \[CenterDot] 
            \[FormalC]) \[CenterDot] ((\[FormalB] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalC])) == 
         ((\[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalA])) \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalA]))) \[CenterDot] 
          (((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalC]) \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalC]))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 4}, "Position" -> {1, 1}, 
        "Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "OutputExpression" -> HoldForm[(\[FormalA] \[CenterDot] 
             ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
              \[FormalC])) \[CenterDot] ((\[FormalC] \[CenterDot] 
              \[FormalC]) \[CenterDot] ((\[FormalB] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalC])) == 
           ((\[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] 
               \[FormalA])) \[CenterDot] (\[FormalC] \[CenterDot] 
              (\[FormalB] \[CenterDot] \[FormalA]))) \[CenterDot] 
            (((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
              \[FormalC]) \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalA]) \[CenterDot] \[FormalC]))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, {"CriticalPairLemma", 5} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] \[FormalB] == 
         ((\[FormalB] \[CenterDot] (\[FormalA] \[CenterDot] 
             \[FormalA])) \[CenterDot] (\[FormalB] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalA]))) \[CenterDot] 
          (\[FormalC] \[CenterDot] (\[FormalC] \[CenterDot] \[FormalC]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 1}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalB]_))) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalA]_), "MatchingConstruct" -> {"Axiom", 3}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) <-> 
          (((\[FormalB]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] (\[FormalA]_)), "MatchingSide" -> 2, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] \[FormalB] == \[FormalB] \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 5}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 1}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
            \[FormalB] == \[FormalB] \[CenterDot] (\[FormalA] \[CenterDot] 
             \[FormalA])], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 6} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalB])) == \[FormalB] \[CenterDot] 
          \[FormalA]], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 2}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)), "Side" -> 1, "Subpattern" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalA]_), "MatchingConstruct" -> 
         {"Axiom", 1}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"SubstitutionLemma", 3} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalB] \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 6}, "Position" -> {2}, 
        "Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
           \[FormalB] \[CenterDot] \[FormalA]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, {"CriticalPairLemma", 7} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalA])) \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalB]) == \[FormalB]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 3}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (\[FormalB]_) <-> (\[FormalB]_) \[CenterDot] (\[FormalA]_), 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"CriticalPairLemma", 1}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 8} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
            \[FormalB])) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 7}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"Axiom", 1}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 4} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 2}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
           \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalB]))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, {"CriticalPairLemma", 9} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalB]))) \[CenterDot] (\[FormalA] \[CenterDot] 
           \[FormalA])], "Proof" -> <|"Construct" -> {"Axiom", 1}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA], "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CenterDot] (\[FormalA]_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 4}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalA]_) <-> 
          (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalB]_))), "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 10} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalB]))) \[CenterDot] (\[FormalC] \[CenterDot] 
           (\[FormalC] \[CenterDot] \[FormalC]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 1}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalB]_))) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalA]_), "MatchingConstruct" -> {"SubstitutionLemma", 4}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalA]_) <-> 
          (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalB]_))), "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 11} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalA]) == 
         ((\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalB])) \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalA]))) \[CenterDot] 
          (\[FormalC] \[CenterDot] (\[FormalC] \[CenterDot] \[FormalC]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 10}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
              (\[FormalB]_)))) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalC]_))) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          ((\[FormalB]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_))), "MatchingConstruct" -> {"CriticalPairLemma", 8}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalA]_) <-> 
          ((\[FormalB]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalB]_))) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 5} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalA]) == \[FormalB] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 11}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 10}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             ((\[FormalB]_) \[CenterDot] (\[FormalB]_)))) \[CenterDot] 
           ((\[FormalC]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalC]_))) -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
             \[FormalA]) == \[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalB])], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 12} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] ((\[FormalB] \[CenterDot] 
            \[FormalB]) \[CenterDot] (\[FormalB] \[CenterDot] \[FormalB])) == 
         ((((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalB])) \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB])) \[CenterDot] 
           (((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalB])) \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB]))) \[CenterDot] 
          (\[FormalC] \[CenterDot] (\[FormalC] \[CenterDot] \[FormalC]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 10}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
              (\[FormalB]_)))) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalC]_))) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          ((\[FormalB]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_))), "MatchingConstruct" -> {"Axiom", 3}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) <-> 
          (((\[FormalB]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] (\[FormalA]_)), "MatchingSide" -> 2, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 6} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] \[FormalB] == 
         ((((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalB])) \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB])) \[CenterDot] 
           (((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalB])) \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB]))) \[CenterDot] 
          (\[FormalC] \[CenterDot] (\[FormalC] \[CenterDot] \[FormalC]))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 12}, "Position" -> {2}, 
        "Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "OutputExpression" -> HoldForm[(\[FormalA] \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalB] == 
           ((((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] (
                \[FormalB] \[CenterDot] \[FormalB])) \[CenterDot] 
              (\[FormalA] \[CenterDot] \[FormalB])) \[CenterDot] 
             (((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] (
                \[FormalB] \[CenterDot] \[FormalB])) \[CenterDot] 
              (\[FormalA] \[CenterDot] \[FormalB]))) \[CenterDot] 
            (\[FormalC] \[CenterDot] (\[FormalC] \[CenterDot] \[FormalC]))], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 7} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] \[FormalB] == 
         ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalB])) \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 6}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 1}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
            \[FormalB] == ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalB])) \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, {"SubstitutionLemma", 8} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] \[FormalB] == \[FormalB] \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 7}, "Position" -> {1}, 
        "Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "OutputExpression" -> HoldForm[(\[FormalA] \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalB] == \[FormalB] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 13} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalA]) == \[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 8}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)), "Side" -> 1, "Subpattern" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"SubstitutionLemma", 3}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 14} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA]) == 
         \[FormalB] \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 8}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)), "Side" -> 1, "Subpattern" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalA]_), "MatchingConstruct" -> 
         {"Axiom", 1}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 15} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalA]) == 
         ((\[FormalC] \[CenterDot] (\[FormalC] \[CenterDot] 
             \[FormalC])) \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 8}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)), "Side" -> 1, "Subpattern" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalA]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 8}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] (\[FormalA]_) <-> 
          ((\[FormalB]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalB]_))) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 16} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalA]) == 
         (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalC] \[CenterDot] \[FormalC]))) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 8}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)), "Side" -> 1, "Subpattern" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalA]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 4}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] (\[FormalA]_) <-> 
          (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalB]_))), "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 17} -> 
     <|"Statement" -> HoldForm[
        ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalA])) \[CenterDot] \[FormalB] == 
         \[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 8}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)), "Side" -> 2, "Subpattern" -> 
         (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 8}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)), "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 9} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 17}, "Position" -> {1}, 
        "Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
           \[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
             (\[FormalA] \[CenterDot] \[FormalB]))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 18} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] \[FormalB] == \[FormalB] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 8}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)), "Side" -> 2, "Subpattern" -> 
         (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 3}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 19} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 14}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalB] \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> ((\[FormalB]_) \[CenterDot] 
           (\[FormalB]_)) \[CenterDot] (\[FormalA]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 3}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 20} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
            (\[FormalC] \[CenterDot] (\[FormalC] \[CenterDot] 
              \[FormalC]))) \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 14}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalB] \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"SubstitutionLemma", 4}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalA]_) <-> 
          (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalB]_))), "MatchingSide" -> 1, 
        "Position" -> {2, 1}|>|>, {"CriticalPairLemma", 21} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 9}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalB] \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          (\[FormalA]_), "MatchingConstruct" -> {"SubstitutionLemma", 3}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 22} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalB]) == \[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 13}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) <-> 
          (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)), "Side" -> 1, "Subpattern" -> 
         (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 3}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 23} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalB]) == 
         (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalC] \[CenterDot] \[FormalC]))) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 18}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalA]_)), "Side" -> 1, "Subpattern" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalA]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 4}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] (\[FormalA]_) <-> 
          (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalB]_))), "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 24} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalB]) == \[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalC] \[CenterDot] \[FormalC])))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 22}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalB]_)) <-> 
          (\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)), "Side" -> 1, "Subpattern" -> 
         (\[FormalB]_) \[CenterDot] (\[FormalB]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 4}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] (\[FormalA]_) <-> 
          (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalB]_))), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 25} -> 
     <|"Statement" -> HoldForm[
        ((\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
             \[FormalA])) \[CenterDot] \[FormalB]) \[CenterDot] \[FormalC] == 
         \[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 15}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) <-> 
          (((\[FormalC]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
              (\[FormalC]_))) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalA]_), "Side" -> 1, "Subpattern" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
           (\[FormalA]_)), "MatchingConstruct" -> {"CriticalPairLemma", 13}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalA]_)) <-> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalB]_)), "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 26} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalB]))) \[CenterDot] \[FormalC] == \[FormalC] \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 16}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) <-> 
          ((\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] (\[FormalC]_)))) \[CenterDot] 
           (\[FormalA]_), "Side" -> 1, "Subpattern" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
           (\[FormalA]_)), "MatchingConstruct" -> {"CriticalPairLemma", 13}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalA]_)) <-> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalB]_)), "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 27} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalB]))) \[CenterDot] (\[FormalA] \[CenterDot] 
           \[FormalA]) == ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
           \[FormalA]) \[CenterDot] (((\[FormalB] \[CenterDot] 
             \[FormalB]) \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalB])) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 26}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
              (\[FormalB]_)))) \[CenterDot] (\[FormalC]_) <-> 
          (\[FormalC]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)), "Side" -> 1, "Subpattern" -> 
         ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalB]_)))) \[CenterDot] 
          (\[FormalC]_), "MatchingConstruct" -> {"Axiom", 3}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) <-> 
          (((\[FormalB]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] (\[FormalA]_)), "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"SubstitutionLemma", 10} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
           \[FormalA]) \[CenterDot] (((\[FormalB] \[CenterDot] 
             \[FormalB]) \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalB])) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 27}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 9}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             ((\[FormalB]_) \[CenterDot] (\[FormalB]_)))) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalA] == 
           ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
             \[FormalA]) \[CenterDot] (((\[FormalB] \[CenterDot] 
               \[FormalB]) \[CenterDot] (\[FormalB] \[CenterDot] 
               \[FormalB])) \[CenterDot] \[FormalA])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 11} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalB] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 10}, 
        "Position" -> {2, 1}, "Construct" -> {"Axiom", 1}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == ((\[FormalB] \[CenterDot] 
              \[FormalB]) \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalA])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 12} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CenterDot] 
           \[FormalA]) \[CenterDot] ((\[FormalB] \[CenterDot] 
            \[FormalB]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 11}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == (\[FormalB] \[CenterDot] 
             \[FormalA]) \[CenterDot] ((\[FormalB] \[CenterDot] 
              \[FormalB]) \[CenterDot] \[FormalA])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 28} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalA])) \[CenterDot] 
          (((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalB])) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 12}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"SubstitutionLemma", 8}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           (\[FormalB]_) <-> (\[FormalB]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalB]_)), "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 13} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalA])) \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 28}, 
        "Position" -> {2, 1}, "Construct" -> {"Axiom", 1}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalA])) \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalA])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 14} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 13}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == (\[FormalB] \[CenterDot] 
             \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalA]))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 15} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 14}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 13}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalB]), "OutputExpression" -> 
         HoldForm[\[FormalA] == (\[FormalB] \[CenterDot] 
             \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalA])], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 29} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalB] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 15}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)) -> \[FormalB], "Side" -> 1, 
        "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
           (\[FormalB]_)), "MatchingConstruct" -> {"SubstitutionLemma", 3}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 30} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 15}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)) -> \[FormalB], "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 3}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 31} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
            \[FormalA])) \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 15}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)) -> \[FormalB], "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 22}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)) <-> (\[FormalA]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalB]_)), "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 16} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
            \[FormalA])) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 31}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 15}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
           (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
              \[FormalA])) \[CenterDot] \[FormalA]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 32} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         (\[FormalB] \[CenterDot] (\[FormalA] \[CenterDot] 
            \[FormalB])) \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 15}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)) -> \[FormalB], "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 13}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalA]_)) <-> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalB]_)), "MatchingSide" -> 2, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 17} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         (\[FormalB] \[CenterDot] (\[FormalA] \[CenterDot] 
            \[FormalB])) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 32}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 15}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
           (\[FormalB] \[CenterDot] (\[FormalA] \[CenterDot] 
              \[FormalB])) \[CenterDot] \[FormalA]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 33} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] ((\[FormalB] \[CenterDot] 
            \[FormalA]) \[CenterDot] (\[FormalB] \[CenterDot] \[FormalA])) == 
         (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 13}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) <-> 
          (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)), "Side" -> 1, "Subpattern" -> 
         (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 15}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)) -> \[FormalB], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 34} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 29}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA], "Side" -> 1, 
        "Subpattern" -> (\[FormalB]_) \[CenterDot] (\[FormalA]_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 3}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 35} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalB]))) \[CenterDot] (\[FormalA] \[CenterDot] 
           \[FormalC])], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 
          30}, "Orientation" -> -1, "Rule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           (\[FormalB]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
           (\[FormalA]_)), "MatchingConstruct" -> {"CriticalPairLemma", 26}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             ((\[FormalB]_) \[CenterDot] (\[FormalB]_)))) \[CenterDot] 
           (\[FormalC]_) <-> (\[FormalC]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalA]_)), "MatchingSide" -> 2, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 36} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalB])) \[CenterDot] \[FormalA]) \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalC])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 30}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA], "Side" -> 1, 
        "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           (\[FormalB]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
           (\[FormalA]_)), "MatchingConstruct" -> {"CriticalPairLemma", 25}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (((\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (\[FormalA]_))) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalC]_) <-> (\[FormalC]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalB]_)), "MatchingSide" -> 2, 
        "Position" -> {}|>|>, {"SubstitutionLemma", 18} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 16}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
           \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalA]))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 19} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 17}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
           \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             (\[FormalA] \[CenterDot] \[FormalB]))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 20} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] ((\[FormalB] \[CenterDot] 
            \[FormalA]) \[CenterDot] (\[FormalB] \[CenterDot] \[FormalA])) == 
         \[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 33}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 18}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           (\[FormalB]_) -> \[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
            \[FormalA]), "OutputExpression" -> HoldForm[
          (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
            ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalA])) == \[FormalA] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalA])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 37} -> 
     <|"Statement" -> HoldForm[
        (((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalA])) \[CenterDot] 
           \[FormalB]) \[CenterDot] ((((\[FormalC] \[CenterDot] 
              \[FormalA]) \[CenterDot] (\[FormalC] \[CenterDot] 
              \[FormalA])) \[CenterDot] ((\[FormalC] \[CenterDot] 
              \[FormalA]) \[CenterDot] (\[FormalC] \[CenterDot] 
              \[FormalA]))) \[CenterDot] \[FormalB]) == 
         (\[FormalB] \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalA]))) \[CenterDot] 
          (\[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalA]) \[CenterDot] ((\[FormalC] \[CenterDot] 
              \[FormalA]) \[CenterDot] (\[FormalC] \[CenterDot] 
              \[FormalA]))))], "Proof" -> <|"Construct" -> {"Axiom", 3}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) <-> (((\[FormalB]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           (((\[FormalC]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
            (\[FormalA]_)), "Side" -> 1, "Subpattern" -> 
         (\[FormalB]_) \[CenterDot] (\[FormalC]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 20}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalA] \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalA]), "MatchingSide" -> 1, 
        "Position" -> {1, 2}|>|>, {"SubstitutionLemma", 21} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] ((((\[FormalC] \[CenterDot] 
              \[FormalA]) \[CenterDot] (\[FormalC] \[CenterDot] 
              \[FormalA])) \[CenterDot] ((\[FormalC] \[CenterDot] 
              \[FormalA]) \[CenterDot] (\[FormalC] \[CenterDot] 
              \[FormalA]))) \[CenterDot] \[FormalB]) == 
         (\[FormalB] \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalA]))) \[CenterDot] 
          (\[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalA]) \[CenterDot] ((\[FormalC] \[CenterDot] 
              \[FormalA]) \[CenterDot] (\[FormalC] \[CenterDot] 
              \[FormalA]))))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 37}, "Position" -> {1, 1}, 
        "Construct" -> {"SubstitutionLemma", 15}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "OutputExpression" -> HoldForm[(\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] ((((\[FormalC] \[CenterDot] 
                \[FormalA]) \[CenterDot] (\[FormalC] \[CenterDot] 
                \[FormalA])) \[CenterDot] ((\[FormalC] \[CenterDot] 
                \[FormalA]) \[CenterDot] (\[FormalC] \[CenterDot] 
                \[FormalA]))) \[CenterDot] \[FormalB]) == 
           (\[FormalB] \[CenterDot] (\[FormalA] \[CenterDot] 
              (\[FormalA] \[CenterDot] \[FormalA]))) \[CenterDot] 
            (\[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalA]) \[CenterDot] ((\[FormalC] \[CenterDot] 
                \[FormalA]) \[CenterDot] (\[FormalC] \[CenterDot] 
                \[FormalA]))))], "ConstructSide" -> 1, "InputOrientation" -> 
         1, "Side" -> 1|>|>, {"SubstitutionLemma", 22} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] ((\[FormalC] \[CenterDot] 
            \[FormalA]) \[CenterDot] \[FormalB]) == 
         (\[FormalB] \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalA]))) \[CenterDot] 
          (\[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalA]) \[CenterDot] ((\[FormalC] \[CenterDot] 
              \[FormalA]) \[CenterDot] (\[FormalC] \[CenterDot] 
              \[FormalA]))))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 21}, "Position" -> {2, 1}, 
        "Construct" -> {"SubstitutionLemma", 15}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "OutputExpression" -> HoldForm[(\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] ((\[FormalC] \[CenterDot] 
              \[FormalA]) \[CenterDot] \[FormalB]) == 
           (\[FormalB] \[CenterDot] (\[FormalA] \[CenterDot] 
              (\[FormalA] \[CenterDot] \[FormalA]))) \[CenterDot] 
            (\[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalA]) \[CenterDot] ((\[FormalC] \[CenterDot] 
                \[FormalA]) \[CenterDot] (\[FormalC] \[CenterDot] 
                \[FormalA]))))], "ConstructSide" -> 1, "InputOrientation" -> 
         1, "Side" -> 1|>|>, {"SubstitutionLemma", 23} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] ((\[FormalC] \[CenterDot] 
            \[FormalA]) \[CenterDot] \[FormalB]) == \[FormalB]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 35}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             ((\[FormalB]_) \[CenterDot] (\[FormalB]_)))) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalC]_)) -> \[FormalA], 
        "OutputExpression" -> HoldForm[(\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] ((\[FormalC] \[CenterDot] 
              \[FormalA]) \[CenterDot] \[FormalB]) == \[FormalB]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 38} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalB] \[CenterDot] ((\[FormalC] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalB])) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 23}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"CriticalPairLemma", 29}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 39} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] ((\[FormalC] \[CenterDot] 
            \[FormalB]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 23}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"SubstitutionLemma", 3}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 40} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB])) \[CenterDot] 
          ((\[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalB])) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 23}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"CriticalPairLemma", 18}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           (\[FormalB]_) <-> (\[FormalB]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)), "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 41} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalC] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 23}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> ((\[FormalC]_) \[CenterDot] 
           (\[FormalA]_)) \[CenterDot] (\[FormalB]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 3}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 42} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalC])) \[CenterDot] \[FormalA]) \[CenterDot] 
          ((\[FormalC] \[CenterDot] \[FormalC]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 23}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalC]_) \[CenterDot] 
          (\[FormalA]_), "MatchingConstruct" -> {"SubstitutionLemma", 18}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalA]_))) -> 
          \[FormalA] \[CenterDot] \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2, 1}|>|>, {"CriticalPairLemma", 43} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalB] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 23}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalC]_) \[CenterDot] 
          (\[FormalA]_), "MatchingConstruct" -> {"CriticalPairLemma", 36}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (((\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (\[FormalA]_))) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalC]_)) -> \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"CriticalPairLemma", 44} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB]) == 
         (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
          (((\[FormalC] \[CenterDot] \[FormalA]) \[CenterDot] 
            \[FormalB]) \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 19}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalA] \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"SubstitutionLemma", 23}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (((\[FormalC]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
            (\[FormalB]_)) -> \[FormalB], "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 45} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalC] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 39}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> ((\[FormalC]_) \[CenterDot] 
           (\[FormalB]_)) \[CenterDot] (\[FormalA]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 3}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 46} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 39}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalC]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"CriticalPairLemma", 36}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (((\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (\[FormalA]_))) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalC]_)) -> \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"CriticalPairLemma", 47} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] ((\[FormalB] \[CenterDot] 
            \[FormalC]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 39}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalC]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"SubstitutionLemma", 3}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2, 1}|>|>, {"CriticalPairLemma", 48} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] \[FormalC] == 
         (((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalC]) \[CenterDot] \[FormalB]) \[CenterDot] \[FormalC]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 39}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> ((\[FormalC]_) \[CenterDot] 
           (\[FormalB]_)) \[CenterDot] (\[FormalA]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 39}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 49} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB]) == 
         (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
          (((\[FormalC] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalA]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 19}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalA] \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"CriticalPairLemma", 39}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (((\[FormalC]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"SubstitutionLemma", 24} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CenterDot] 
           \[FormalA]) \[CenterDot] ((\[FormalB] \[CenterDot] 
            \[FormalC]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 43}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == (\[FormalB] \[CenterDot] 
             \[FormalA]) \[CenterDot] ((\[FormalB] \[CenterDot] 
              \[FormalC]) \[CenterDot] \[FormalA])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 50} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalC]) == 
         (\[FormalC] \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalC]))) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 24}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           (\[FormalC]_)) \[CenterDot] (\[FormalB]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 41}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalA]_))) -> \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 51} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 45}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalB]_))) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalC]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"CriticalPairLemma", 36}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (((\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (\[FormalA]_))) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalC]_)) -> \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"CriticalPairLemma", 52} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         ((\[FormalC] \[CenterDot] \[FormalB]) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB])) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 41}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalA]_))) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          ((\[FormalC]_) \[CenterDot] (\[FormalA]_)), "MatchingConstruct" -> 
         {"CriticalPairLemma", 45}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalB]_))) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 25} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC]))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 46}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == (\[FormalB] \[CenterDot] 
             \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalC]))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 53} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalC]) == 
         (\[FormalB] \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalC]))) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 24}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           (\[FormalC]_)) \[CenterDot] (\[FormalB]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 25}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalC]_))) -> \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 26} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC]))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 51}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalC]))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 54} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 26}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          ((\[FormalB]_) \[CenterDot] (\[FormalC]_)), "MatchingConstruct" -> 
         {"CriticalPairLemma", 47}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 27} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalA] \[CenterDot] ((\[FormalC] \[CenterDot] 
            \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 52}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
           \[FormalA] \[CenterDot] ((\[FormalC] \[CenterDot] 
              \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
              \[FormalB]))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 28} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalB]) \[CenterDot] (\[FormalB] \[CenterDot] \[FormalC]))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 54}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
           \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] (\[FormalB] \[CenterDot] 
              \[FormalC]))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 29} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] \[FormalC] == \[FormalC] \[CenterDot] 
          (((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalC]) \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 48}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalC] == \[FormalC] \[CenterDot] 
            (((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
              \[FormalC]) \[CenterDot] \[FormalB])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 30} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] \[FormalC] == \[FormalC] \[CenterDot] 
          (\[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalC]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 29}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalC] == \[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] 
             ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
              \[FormalC]))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 31} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalC]) == \[FormalA] \[CenterDot] 
          (\[FormalC] \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalC])))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 50}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalC]) == \[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] 
             (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
               \[FormalC])))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 32} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalC]) == \[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalC])))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 53}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalC]) == \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
               \[FormalC])))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 55} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalB] \[CenterDot] ((\[FormalC] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 38}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (\[FormalA]_))) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalC] \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] (\[FormalA]_)), "MatchingConstruct" -> 
         {"CriticalPairLemma", 13}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) <-> 
          (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)), "MatchingSide" -> 2, "Position" -> {2, 1}|>|>, 
    {"CriticalPairLemma", 56} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB])) \[CenterDot] 
          ((\[FormalC] \[CenterDot] (\[FormalC] \[CenterDot] 
             \[FormalB])) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 40}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalB]_))) \[CenterDot] 
           (((\[FormalC]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
              (\[FormalB]_))) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalC]_) \[CenterDot] 
          ((\[FormalB]_) \[CenterDot] (\[FormalB]_)), "MatchingConstruct" -> 
         {"CriticalPairLemma", 22}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalB]_)) <-> 
          (\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)), "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"CriticalPairLemma", 57} -> 
     <|"Statement" -> HoldForm[
        (((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalA])) \[CenterDot] 
           ((\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
              \[FormalA])) \[CenterDot] \[FormalC])) \[CenterDot] 
          ((\[FormalC] \[CenterDot] \[FormalC]) \[CenterDot] 
           ((\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
              \[FormalA])) \[CenterDot] \[FormalC])) == 
         \[FormalC] \[CenterDot] (((\[FormalB] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalA])) \[CenterDot] 
            \[FormalC]) \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalC]))], 
      "Proof" -> <|"Construct" -> {"Axiom", 3}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) <-> 
          (((\[FormalB]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] (\[FormalA]_)), "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          ((\[FormalB]_) \[CenterDot] (\[FormalC]_)), "MatchingConstruct" -> 
         {"CriticalPairLemma", 42}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (((\[FormalA]_) \[CenterDot] 
             ((\[FormalA]_) \[CenterDot] (\[FormalB]_))) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalC]_)) -> \[FormalC], 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"SubstitutionLemma", 33} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           ((\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
              \[FormalA])) \[CenterDot] \[FormalC])) \[CenterDot] 
          ((\[FormalC] \[CenterDot] \[FormalC]) \[CenterDot] 
           ((\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
              \[FormalA])) \[CenterDot] \[FormalC])) == 
         \[FormalC] \[CenterDot] (((\[FormalB] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalA])) \[CenterDot] 
            \[FormalC]) \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalC]))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 57}, 
        "Position" -> {1, 1}, "Construct" -> {"SubstitutionLemma", 15}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)) -> \[FormalB], "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] (
                \[FormalB] \[CenterDot] \[FormalA])) \[CenterDot] 
              \[FormalC])) \[CenterDot] ((\[FormalC] \[CenterDot] 
              \[FormalC]) \[CenterDot] ((\[FormalB] \[CenterDot] (
                \[FormalB] \[CenterDot] \[FormalA])) \[CenterDot] 
              \[FormalC])) == \[FormalC] \[CenterDot] 
            (((\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
                \[FormalA])) \[CenterDot] \[FormalC]) \[CenterDot] 
             ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
              \[FormalC]))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 34} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           ((\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
              \[FormalA])) \[CenterDot] \[FormalC])) \[CenterDot] 
          \[FormalC] == \[FormalC] \[CenterDot] 
          (((\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
              \[FormalA])) \[CenterDot] \[FormalC]) \[CenterDot] 
           ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] \[FormalC]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 33}, "Position" -> {2}, 
        "Construct" -> {"CriticalPairLemma", 29}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "OutputExpression" -> HoldForm[(\[FormalA] \[CenterDot] 
             ((\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
                \[FormalA])) \[CenterDot] \[FormalC])) \[CenterDot] 
            \[FormalC] == \[FormalC] \[CenterDot] 
            (((\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
                \[FormalA])) \[CenterDot] \[FormalC]) \[CenterDot] 
             ((\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
              \[FormalC]))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 35} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           ((\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
              \[FormalA])) \[CenterDot] \[FormalC])) \[CenterDot] 
          \[FormalC] == \[FormalC] \[CenterDot] \[FormalC]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 34}, "Position" -> {2}, 
        "Construct" -> {"CriticalPairLemma", 42}, "Orientation" -> -1, 
        "Rule" -> (((\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (\[FormalB]_))) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) -> \[FormalC], "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] (
                \[FormalB] \[CenterDot] \[FormalA])) \[CenterDot] 
              \[FormalC])) \[CenterDot] \[FormalC] == \[FormalC] \[CenterDot] 
            \[FormalC]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 36} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] ((\[FormalC] \[CenterDot] 
             (\[FormalC] \[CenterDot] \[FormalB])) \[CenterDot] 
            \[FormalA])) == \[FormalA] \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 35}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             ((\[FormalC] \[CenterDot] (\[FormalC] \[CenterDot] 
                \[FormalB])) \[CenterDot] \[FormalA])) == 
           \[FormalA] \[CenterDot] \[FormalA]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 58} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
            (\[FormalC] \[CenterDot] (\[FormalA] \[CenterDot] 
              \[FormalC])))) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 36}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
              ((\[FormalC]_) \[CenterDot] (\[FormalB]_))) \[CenterDot] 
             (\[FormalA]_))) -> \[FormalA] \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          ((\[FormalB]_) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] (\[FormalB]_))) \[CenterDot] 
            (\[FormalA]_))), "MatchingConstruct" -> {"CriticalPairLemma", 
          55}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             ((\[FormalA]_) \[CenterDot] (\[FormalB]_))) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalA]_))) -> 
          \[FormalC] \[CenterDot] \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"SubstitutionLemma", 37} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
           (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
             (\[FormalA] \[CenterDot] \[FormalC]))))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 58}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
           \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] (
                \[FormalA] \[CenterDot] \[FormalC]))))], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 59} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalB]))) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalB]))) == 
         (\[FormalC] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalD] \[CenterDot] ((\[FormalA] \[CenterDot] (
                \[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
                 \[FormalB]))) \[CenterDot] \[FormalD])))) \[CenterDot] 
          (\[FormalA] \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 37}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] (
                \[FormalC]_))))) -> \[FormalA] \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          ((\[FormalB]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (\[FormalC]_))))), "MatchingConstruct" -> {"CriticalPairLemma", 
          26}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             ((\[FormalB]_) \[CenterDot] (\[FormalB]_)))) \[CenterDot] 
           (\[FormalC]_) <-> (\[FormalC]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalA]_)), "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"SubstitutionLemma", 38} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CenterDot] 
           (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
             ((\[FormalA] \[CenterDot] (\[FormalD] \[CenterDot] 
                (\[FormalD] \[CenterDot] \[FormalD]))) \[CenterDot] 
              \[FormalC])))) \[CenterDot] (\[FormalA] \[CenterDot] 
           \[FormalA])], "Proof" -> <|"Input" -> {"CriticalPairLemma", 59}, 
        "Position" -> {}, "Construct" -> {"CriticalPairLemma", 35}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
              (\[FormalB]_)))) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalC]_)) -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == (\[FormalB] \[CenterDot] 
             (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] (
                (\[FormalA] \[CenterDot] (\[FormalD] \[CenterDot] 
                  (\[FormalD] \[CenterDot] \[FormalD]))) \[CenterDot] 
                \[FormalC])))) \[CenterDot] (\[FormalA] \[CenterDot] 
             \[FormalA])], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 39} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CenterDot] 
           (\[FormalB] \[CenterDot] (\[FormalA] \[CenterDot] 
             \[FormalC]))) \[CenterDot] (\[FormalA] \[CenterDot] 
           \[FormalA])], "Proof" -> <|"Input" -> {"SubstitutionLemma", 38}, 
        "Position" -> {1, 2, 2}, "Construct" -> {"CriticalPairLemma", 20}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
              ((\[FormalC]_) \[CenterDot] (\[FormalC]_)))) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalB] \[CenterDot] \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalA] == 
           (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
              (\[FormalA] \[CenterDot] \[FormalC]))) \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalA])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 60} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
           ((\[FormalC] \[CenterDot] (\[FormalA] \[CenterDot] 
              \[FormalC])) \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 37}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] (
                \[FormalC]_))))) -> \[FormalA] \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          ((\[FormalC]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalC]_))), "MatchingConstruct" -> {"SubstitutionLemma", 3}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 61} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CenterDot] 
           (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
             \[FormalA]))) \[CenterDot] (\[FormalA] \[CenterDot] 
           \[FormalA])], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 
          39}, "Orientation" -> -1, "Rule" -> 
         ((\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             ((\[FormalB]_) \[CenterDot] (\[FormalC]_)))) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          ((\[FormalB]_) \[CenterDot] (\[FormalC]_)), "MatchingConstruct" -> 
         {"SubstitutionLemma", 31}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] (\[FormalB]_)))) -> 
          \[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] \[FormalB]), 
        "MatchingSide" -> 1, "Position" -> {1, 2}|>|>, 
    {"CriticalPairLemma", 62} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
             (\[FormalC] \[CenterDot] \[FormalD]))) \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalD])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 25}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalC]_))) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalC]_), "MatchingConstruct" -> {"CriticalPairLemma", 61}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             ((\[FormalB]_) \[CenterDot] (\[FormalC]_)))) \[CenterDot] 
           ((\[FormalC]_) \[CenterDot] (\[FormalC]_)) -> \[FormalC], 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"CriticalPairLemma", 63} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         \[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
            ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
             \[FormalC])) \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 60}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
              ((\[FormalA]_) \[CenterDot] (\[FormalC]_))) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalA] \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> ((\[FormalC]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalC]_))) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"SubstitutionLemma", 26}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 64} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         \[FormalA] \[CenterDot] (((\[FormalB] \[CenterDot] 
             (\[FormalA] \[CenterDot] \[FormalC])) \[CenterDot] 
            \[FormalC]) \[CenterDot] \[FormalC])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 60}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
              ((\[FormalA]_) \[CenterDot] (\[FormalC]_))) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalA] \[CenterDot] \[FormalA], 
        "Side" -> 1, "Subpattern" -> ((\[FormalC]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalC]_))) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"CriticalPairLemma", 39}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (((\[FormalC]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"SubstitutionLemma", 40} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
           (\[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] \[FormalC])))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 63}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
           \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             (\[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
                \[FormalB]) \[CenterDot] \[FormalC])))], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 41} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
           ((\[FormalC] \[CenterDot] (\[FormalA] \[CenterDot] 
              \[FormalB])) \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 64}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
           \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             ((\[FormalC] \[CenterDot] (\[FormalA] \[CenterDot] 
                \[FormalB])) \[CenterDot] \[FormalB]))], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 42} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
           (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
             (\[FormalA] \[CenterDot] \[FormalB]))))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 41}, 
        "Position" -> {2, 2}, "Construct" -> {"SubstitutionLemma", 3}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (\[FormalB]_) -> \[FormalB] \[CenterDot] \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
           \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] (
                \[FormalA] \[CenterDot] \[FormalB]))))], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 65} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
            ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
             \[FormalC])) \[CenterDot] ((\[FormalA] \[CenterDot] 
             ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
              \[FormalC])) \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalB])))], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 42}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
              ((\[FormalA]_) \[CenterDot] (\[FormalB]_))))) -> 
          \[FormalA] \[CenterDot] \[FormalA], "Side" -> 1, 
        "Subpattern" -> (\[FormalC]_) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] (\[FormalB]_)), "MatchingConstruct" -> 
         {"SubstitutionLemma", 40}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
              (\[FormalC]_)))) -> \[FormalA] \[CenterDot] \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2, 2, 2}|>|>, 
    {"SubstitutionLemma", 43} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
         \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
           (\[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
              \[FormalA]) \[CenterDot] \[FormalC])))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 65}, "Position" -> {2}, 
        "Construct" -> {"CriticalPairLemma", 19}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalB]_))) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalA] == 
           \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             (\[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
                \[FormalA]) \[CenterDot] \[FormalC])))], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 66} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalC])) == \[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 32}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             ((\[FormalB]_) \[CenterDot] (\[FormalC]_)))) -> 
          \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalC]), 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalC]_))), "MatchingConstruct" -> {"SubstitutionLemma", 
          43}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] (
                \[FormalA]_)) \[CenterDot] (\[FormalC]_)))) -> 
          \[FormalA] \[CenterDot] \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 67} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalB]) == \[FormalA] \[CenterDot] 
          ((\[FormalC] \[CenterDot] (\[FormalA] \[CenterDot] 
             \[FormalB])) \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 66}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
              (\[FormalB]_)) \[CenterDot] (\[FormalC]_))) -> 
          \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalB]), 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CenterDot] 
          (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalC]_)), "MatchingConstruct" -> {"SubstitutionLemma", 30}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (((\[FormalC]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
             (\[FormalA]_))) -> (\[FormalC] \[CenterDot] 
            \[FormalB]) \[CenterDot] \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 44} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalB]) == \[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB])))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 67}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalB]) == \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             (\[FormalC] \[CenterDot] (\[FormalA] \[CenterDot] 
               \[FormalB])))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 68} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalC]))) \[CenterDot] ((\[FormalC] \[CenterDot] 
            \[FormalB]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 56}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalB]_))) \[CenterDot] 
           (((\[FormalC]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
              (\[FormalB]_))) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalC]_) \[CenterDot] 
          ((\[FormalC]_) \[CenterDot] (\[FormalB]_)), "MatchingConstruct" -> 
         {"CriticalPairLemma", 21}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalB] \[CenterDot] \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"SubstitutionLemma", 45} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] ((\[FormalC] \[CenterDot] 
            (\[FormalC] \[CenterDot] (\[FormalD] \[CenterDot] 
              \[FormalB]))) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 62}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
             \[FormalB]) \[CenterDot] ((\[FormalC] \[CenterDot] 
              (\[FormalC] \[CenterDot] (\[FormalD] \[CenterDot] 
                \[FormalB]))) \[CenterDot] \[FormalA])], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 69} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
             \[FormalD]))) \[CenterDot] ((\[FormalC] \[CenterDot] 
            (\[FormalC] \[CenterDot] (\[FormalD] \[CenterDot] 
              \[FormalD]))) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 45}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] ((\[FormalD]_) \[CenterDot] (
                \[FormalB]_)))) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalC]_) \[CenterDot] 
          ((\[FormalD]_) \[CenterDot] (\[FormalB]_)), "MatchingConstruct" -> 
         {"SubstitutionLemma", 44}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             ((\[FormalA]_) \[CenterDot] (\[FormalB]_)))) -> 
          \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalB]), 
        "MatchingSide" -> 1, "Position" -> {2, 1, 2}|>|>, 
    {"SubstitutionLemma", 46} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
             \[FormalD]))) \[CenterDot] ((\[FormalD] \[CenterDot] 
            \[FormalC]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 69}, 
        "Position" -> {2, 1}, "Construct" -> {"CriticalPairLemma", 19}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalB] \[CenterDot] \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalA] == 
           (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
              (\[FormalC] \[CenterDot] \[FormalD]))) \[CenterDot] 
            ((\[FormalD] \[CenterDot] \[FormalC]) \[CenterDot] \[FormalA])], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 47} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
           (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalC]))) == 
         ((\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalC])) \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalC]))) \[CenterDot] 
          (((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalA]) \[CenterDot] ((\[FormalC] \[CenterDot] 
             \[FormalC]) \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 3}, "Position" -> {1}, 
        "Construct" -> {"CriticalPairLemma", 34}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) -> \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] 
            (((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
              (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
             (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
               \[FormalC]))) == ((\[FormalA] \[CenterDot] 
              (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
             (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
               \[FormalC]))) \[CenterDot] (((\[FormalB] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalA]) \[CenterDot] 
             ((\[FormalC] \[CenterDot] \[FormalC]) \[CenterDot] 
              \[FormalA]))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 48} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalC]) == 
         ((\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalC])) \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalC]))) \[CenterDot] 
          (((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalA]) \[CenterDot] ((\[FormalC] \[CenterDot] 
             \[FormalC]) \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 47}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 27}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_)) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (\[FormalC]_))) -> \[FormalA] \[CenterDot] \[FormalC], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalC]) == 
           ((\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
               \[FormalC])) \[CenterDot] (\[FormalA] \[CenterDot] 
              (\[FormalB] \[CenterDot] \[FormalC]))) \[CenterDot] 
            (((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
              \[FormalA]) \[CenterDot] ((\[FormalC] \[CenterDot] 
               \[FormalC]) \[CenterDot] \[FormalA]))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 49} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] \[FormalB])))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 68}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] == ((\[FormalB] \[CenterDot] 
              \[FormalC]) \[CenterDot] \[FormalA]) \[CenterDot] 
            (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
              (\[FormalC] \[CenterDot] \[FormalB])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 70} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] ((\[FormalB] \[CenterDot] 
             \[FormalC]) \[CenterDot] (\[FormalC] \[CenterDot] 
             \[FormalB]))) == \[FormalA] \[CenterDot] 
          (\[FormalC] \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 24}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) <-> 
          (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
              (\[FormalC]_)))), "Side" -> 2, "Subpattern" -> 
         (\[FormalB]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
           ((\[FormalC]_) \[CenterDot] (\[FormalC]_))), 
        "MatchingConstruct" -> {"SubstitutionLemma", 49}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
              (\[FormalA]_)))) -> \[FormalC], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 71} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalC]) == \[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] ((\[FormalC] \[CenterDot] 
             \[FormalB]) \[CenterDot] (\[FormalC] \[CenterDot] 
             \[FormalB])))], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 70}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
             ((\[FormalC]_) \[CenterDot] (\[FormalB]_)))) -> 
          \[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] \[FormalB]), 
        "Side" -> 1, "Subpattern" -> (\[FormalC]_) \[CenterDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"SubstitutionLemma", 3}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2, 2, 2}|>|>, {"SubstitutionLemma", 50} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalC]) == 
         (\[FormalC] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 71}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 19}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalB]_))) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalC]) == (\[FormalC] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalA]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 72} -> 
     <|"Statement" -> HoldForm[
        ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           \[FormalA]) \[CenterDot] \[FormalC] == \[FormalC] \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 50}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalC]_)) <-> 
          ((\[FormalC]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalA]_), "Side" -> 1, "Subpattern" -> 
         (\[FormalB]_) \[CenterDot] (\[FormalC]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 22}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalB]_)) <-> 
          (\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)), "MatchingSide" -> 2, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 73} -> 
     <|"Statement" -> HoldForm[
        ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           \[FormalA]) \[CenterDot] \[FormalC] == \[FormalC] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 50}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalC]_)) <-> 
          ((\[FormalC]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalA]_), "Side" -> 1, "Subpattern" -> 
         (\[FormalB]_) \[CenterDot] (\[FormalC]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 18}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalA]_)), "MatchingSide" -> 2, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 74} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalB])) \[CenterDot] \[FormalC] == 
         \[FormalC] \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 50}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalC]_)) <-> 
          ((\[FormalC]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalA]_), "Side" -> 1, "Subpattern" -> 
         (\[FormalB]_) \[CenterDot] (\[FormalC]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 18}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalA]_)), "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 75} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalB])) \[CenterDot] \[FormalC] == 
         \[FormalC] \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 50}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalC]_)) <-> 
          ((\[FormalC]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalA]_), "Side" -> 1, "Subpattern" -> 
         (\[FormalB]_) \[CenterDot] (\[FormalC]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 8}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)), "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 76} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] \[FormalD]) == 
         ((\[FormalC] \[CenterDot] \[FormalB]) \[CenterDot] 
           \[FormalD]) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 50}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalC]_)) <-> 
          ((\[FormalC]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalA]_), "Side" -> 2, "Subpattern" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 50}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalC]_)) <-> 
          ((\[FormalC]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalA]_), "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 77} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalC] \[CenterDot] \[FormalD]) == 
         (\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
          (\[FormalD] \[CenterDot] \[FormalC])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 50}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalC]_)) <-> 
          ((\[FormalC]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalA]_), "Side" -> 2, "Subpattern" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
          (\[FormalC]_), "MatchingConstruct" -> {"SubstitutionLemma", 50}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalC]_)) <-> ((\[FormalC]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 78} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] \[FormalD])) == 
         (\[FormalB] \[CenterDot] (\[FormalD] \[CenterDot] 
            \[FormalC])) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 50}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalC]_)) <-> 
          ((\[FormalC]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalA]_), "Side" -> 2, "Subpattern" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 50}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalC]_)) <-> 
          ((\[FormalC]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalA]_), "MatchingSide" -> 2, "Position" -> {1}|>|>, 
    {"SubstitutionLemma", 51} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
           \[FormalA]) \[CenterDot] ((\[FormalD] \[CenterDot] 
            (\[FormalC] \[CenterDot] \[FormalB])) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 46}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 50}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalC]_) -> \[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] 
            \[FormalA]), "OutputExpression" -> HoldForm[
          \[FormalA] == ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] 
             \[FormalA]) \[CenterDot] ((\[FormalD] \[CenterDot] 
              (\[FormalC] \[CenterDot] \[FormalB])) \[CenterDot] 
             \[FormalA])], "ConstructSide" -> 2, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 52} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalA])) \[CenterDot] \[FormalC] == 
         \[FormalC] \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 72}, "Position" -> {1}, 
        "Construct" -> {"SubstitutionLemma", 50}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalC]_) -> \[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] 
            \[FormalA]), "OutputExpression" -> HoldForm[
          (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
              \[FormalA])) \[CenterDot] \[FormalC] == \[FormalC] \[CenterDot] 
            (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalB]))], 
        "ConstructSide" -> 2, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 53} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalA])) \[CenterDot] \[FormalC] == 
         \[FormalC] \[CenterDot] ((\[FormalB] \[CenterDot] 
            \[FormalB]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 73}, "Position" -> {1}, 
        "Construct" -> {"SubstitutionLemma", 50}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           (\[FormalC]_) -> \[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] 
            \[FormalA]), "OutputExpression" -> HoldForm[
          (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
              \[FormalA])) \[CenterDot] \[FormalC] == \[FormalC] \[CenterDot] 
            ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] \[FormalA])], 
        "ConstructSide" -> 2, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 79} -> 
     <|"Statement" -> HoldForm[
        ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           \[FormalC]) \[CenterDot] \[FormalD] == \[FormalD] \[CenterDot] 
          (\[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 76}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
            (\[FormalD]_)) <-> (((\[FormalC]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalD]_)) \[CenterDot] 
           (\[FormalA]_), "Side" -> 1, "Subpattern" -> 
         ((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
          (\[FormalD]_), "MatchingConstruct" -> {"SubstitutionLemma", 3}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 80} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalB] \[CenterDot] \[FormalC]) \[CenterDot] \[FormalD]) == 
         (\[FormalD] \[CenterDot] (\[FormalC] \[CenterDot] 
            \[FormalB])) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 76}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalB]_) \[CenterDot] (\[FormalC]_)) \[CenterDot] 
            (\[FormalD]_)) <-> (((\[FormalC]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalD]_)) \[CenterDot] 
           (\[FormalA]_), "Side" -> 2, "Subpattern" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
          (\[FormalC]_), "MatchingConstruct" -> {"SubstitutionLemma", 3}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 54} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB]) == 
         (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalC])))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 44}, "Position" -> {2}, 
        "Construct" -> {"CriticalPairLemma", 79}, "Orientation" -> 1, 
        "Rule" -> (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] (\[FormalD]_) -> 
          \[FormalD] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalA])), "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB]) == 
           (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
              (\[FormalA] \[CenterDot] \[FormalC])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 55} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB]) == 
         (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalC])))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 49}, "Position" -> {2}, 
        "Construct" -> {"CriticalPairLemma", 79}, "Orientation" -> 1, 
        "Rule" -> (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] (\[FormalD]_) -> 
          \[FormalD] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalA])), "OutputExpression" -> 
         HoldForm[(\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB]) == 
           (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
              (\[FormalB] \[CenterDot] \[FormalC])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 56} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalC])) \[CenterDot] \[FormalC] == 
         ((\[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalA])) \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalA]))) \[CenterDot] 
          (((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalC]) \[CenterDot] ((\[FormalA] \[CenterDot] 
             \[FormalA]) \[CenterDot] \[FormalC]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 1}, "Position" -> {2}, 
        "Construct" -> {"CriticalPairLemma", 29}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) -> \[FormalA], 
        "OutputExpression" -> HoldForm[(\[FormalA] \[CenterDot] 
             ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
              \[FormalC])) \[CenterDot] \[FormalC] == 
           ((\[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] 
               \[FormalA])) \[CenterDot] (\[FormalC] \[CenterDot] 
              (\[FormalB] \[CenterDot] \[FormalA]))) \[CenterDot] 
            (((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
              \[FormalC]) \[CenterDot] ((\[FormalA] \[CenterDot] 
               \[FormalA]) \[CenterDot] \[FormalC]))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 57} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalB])) \[CenterDot] \[FormalC]) == 
         ((\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalC])) \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalC]))) \[CenterDot] 
          (((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalA]) \[CenterDot] ((\[FormalC] \[CenterDot] 
             \[FormalC]) \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 56}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 80}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] (\[FormalD]_) -> 
          \[FormalD] \[CenterDot] ((\[FormalC] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA]), "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              (\[FormalB] \[CenterDot] \[FormalB])) \[CenterDot] 
             \[FormalC]) == ((\[FormalA] \[CenterDot] 
              (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
             (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
               \[FormalC]))) \[CenterDot] (((\[FormalB] \[CenterDot] 
               \[FormalB]) \[CenterDot] \[FormalA]) \[CenterDot] 
             ((\[FormalC] \[CenterDot] \[FormalC]) \[CenterDot] 
              \[FormalA]))], "ConstructSide" -> 2, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 58} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          ((\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalB])) \[CenterDot] \[FormalC]) == \[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalC])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 57}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 48}, "Orientation" -> -1, 
        "Rule" -> (((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
              (\[FormalC]_))) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             ((\[FormalB]_) \[CenterDot] (\[FormalC]_)))) \[CenterDot] 
           ((((\[FormalB]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
              (\[FormalC]_)) \[CenterDot] (\[FormalA]_))) -> 
          \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalC]), 
        "OutputExpression" -> HoldForm[\[FormalA] \[CenterDot] 
            ((\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
               \[FormalB])) \[CenterDot] \[FormalC]) == 
           \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalC])], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 81} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalC]) == \[FormalA] \[CenterDot] 
          (\[FormalC] \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB])))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 58}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
              (\[FormalB]_))) \[CenterDot] (\[FormalC]_)) -> 
          \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalC]), 
        "Side" -> 1, "Subpattern" -> ((\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalB]_))) \[CenterDot] 
          (\[FormalC]_), "MatchingConstruct" -> {"CriticalPairLemma", 74}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalB]_))) \[CenterDot] (\[FormalC]_) <-> 
          (\[FormalC]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalB]_))), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 82} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] \[FormalD]) == 
         \[FormalA] \[CenterDot] (\[FormalD] \[CenterDot] 
           (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB])))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 81}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             ((\[FormalA]_) \[CenterDot] (\[FormalC]_)))) -> 
          \[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] \[FormalB]), 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          (\[FormalC]_), "MatchingConstruct" -> {"SubstitutionLemma", 28}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] (((\[FormalA]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) -> \[FormalA] \[CenterDot] \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {2, 2, 2}|>|>, 
    {"SubstitutionLemma", 59} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] \[FormalD]) == 
         \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalD])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 82}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 81}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
              (\[FormalC]_)))) -> \[FormalA] \[CenterDot] 
           (\[FormalC] \[CenterDot] \[FormalB]), "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (((\[FormalA] \[CenterDot] 
               \[FormalB]) \[CenterDot] (\[FormalB] \[CenterDot] 
               \[FormalC])) \[CenterDot] \[FormalD]) == 
           \[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalD])], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 83} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalB] \[CenterDot] \[FormalC]) == \[FormalA] \[CenterDot] 
          (\[FormalC] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalB])))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 59}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
             ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) \[CenterDot] 
            (\[FormalD]_)) -> \[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalD]), "Side" -> 1, 
        "Subpattern" -> (((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalC]_))) \[CenterDot] (\[FormalD]_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 23}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) <-> ((\[FormalB]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
              (\[FormalC]_)))) \[CenterDot] (\[FormalA]_), 
        "MatchingSide" -> 2, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 84} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CenterDot] 
           (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalC]))) \[CenterDot] (\[FormalB] \[CenterDot] 
           \[FormalB]) == (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalC]))) \[CenterDot] 
          (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] \[FormalA]))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 13}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_)) <-> 
          (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalB]_)), "Side" -> 1, "Subpattern" -> 
         (\[FormalB]_) \[CenterDot] (\[FormalA]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 83}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             ((\[FormalA]_) \[CenterDot] (\[FormalC]_)))) -> 
          \[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] \[FormalB]), 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 60} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CenterDot] 
           (\[FormalB] \[CenterDot] (\[FormalA] \[CenterDot] 
             \[FormalC]))) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalC] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 84}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 39}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             ((\[FormalB]_) \[CenterDot] (\[FormalC]_)))) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "OutputExpression" -> HoldForm[\[FormalA] == 
           (\[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
              (\[FormalA] \[CenterDot] \[FormalC]))) \[CenterDot] 
            (\[FormalA] \[CenterDot] (\[FormalC] \[CenterDot] \[FormalB]))], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 61} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
          (((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalC]) \[CenterDot] \[FormalC])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 60}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 80}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] (\[FormalD]_) -> 
          \[FormalD] \[CenterDot] ((\[FormalC] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA]), "OutputExpression" -> 
         HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
            (((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
              \[FormalC]) \[CenterDot] \[FormalC])], "ConstructSide" -> 2, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 62} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
          (\[FormalC] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalA])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 61}, "Position" -> {2}, 
        "Construct" -> {"CriticalPairLemma", 79}, "Orientation" -> 1, 
        "Rule" -> (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] (\[FormalD]_) -> 
          \[FormalD] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalA])), "OutputExpression" -> 
         HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalC])) \[CenterDot] 
            (\[FormalC] \[CenterDot] (\[FormalC] \[CenterDot] 
              (\[FormalB] \[CenterDot] \[FormalA])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 85} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalC])) == 
         ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
           (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalC])))) \[CenterDot] 
          \[FormalC]], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 23}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (((\[FormalC]_) \[CenterDot] 
             (\[FormalA]_)) \[CenterDot] (\[FormalB]_)) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> ((\[FormalC]_) \[CenterDot] 
           (\[FormalA]_)) \[CenterDot] (\[FormalB]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 62}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) \[CenterDot] 
           ((\[FormalC]_) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
             ((\[FormalB]_) \[CenterDot] (\[FormalA]_)))) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 63} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalC])) == 
         ((\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalA])) \[CenterDot] \[FormalC]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 85}, "Position" -> {1}, 
        "Construct" -> {"SubstitutionLemma", 54}, "Orientation" -> -1, 
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
    {"CriticalPairLemma", 86} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalC])) == 
         ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           (\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalC])))) \[CenterDot] 
          \[FormalC]], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 51}, 
        "Orientation" -> -1, "Rule" -> 
         (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] (((\[FormalD]_) \[CenterDot] 
             ((\[FormalB]_) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
            (\[FormalC]_)) -> \[FormalC], "Side" -> 1, 
        "Subpattern" -> ((\[FormalD]_) \[CenterDot] 
           ((\[FormalB]_) \[CenterDot] (\[FormalA]_))) \[CenterDot] 
          (\[FormalC]_), "MatchingConstruct" -> {"SubstitutionLemma", 62}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] ((\[FormalC]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
              (\[FormalA]_)))) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 64} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalC])) == 
         ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB])) \[CenterDot] \[FormalC]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 86}, "Position" -> {1}, 
        "Construct" -> {"SubstitutionLemma", 55}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             ((\[FormalB]_) \[CenterDot] (\[FormalC]_)))) -> 
          (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB]), "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalC])) == 
           ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
             (\[FormalA] \[CenterDot] \[FormalB])) \[CenterDot] \[FormalC]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 65} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] 
          (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalC])) == 
         \[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalC]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 64}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 63}, "Orientation" -> -1, 
        "Rule" -> (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            ((\[FormalA]_) \[CenterDot] (\[FormalB]_))) \[CenterDot] 
           (\[FormalC]_) -> \[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalC])), "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
             (\[FormalB] \[CenterDot] \[FormalC])) == \[FormalB] \[CenterDot] 
            (\[FormalB] \[CenterDot] (\[FormalA] \[CenterDot] \[FormalC]))], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 66} -> 
     <|"Statement" -> HoldForm[
        (((\[FormalG] \[CenterDot] \[FormalH]) \[CenterDot] 
            (\[FormalG] \[CenterDot] \[FormalH])) \[CenterDot] 
           \[FormalI]) \[CenterDot] (\[FormalI] \[CenterDot] 
           ((\[FormalG] \[CenterDot] \[FormalH]) \[CenterDot] 
            (\[FormalG] \[CenterDot] \[FormalH]))) == 
         (\[FormalG] \[CenterDot] ((\[FormalH] \[CenterDot] 
             \[FormalI]) \[CenterDot] (\[FormalH] \[CenterDot] 
             \[FormalI]))) \[CenterDot] (\[FormalG] \[CenterDot] 
           ((\[FormalH] \[CenterDot] \[FormalI]) \[CenterDot] 
            (\[FormalH] \[CenterDot] \[FormalI])))], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 79}, "Orientation" -> 1, 
        "Rule" -> (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] (\[FormalD]_) -> 
          \[FormalD] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalA])), "OutputExpression" -> 
         HoldForm[(((\[FormalG] \[CenterDot] \[FormalH]) \[CenterDot] 
              (\[FormalG] \[CenterDot] \[FormalH])) \[CenterDot] 
             \[FormalI]) \[CenterDot] (\[FormalI] \[CenterDot] 
             ((\[FormalG] \[CenterDot] \[FormalH]) \[CenterDot] 
              (\[FormalG] \[CenterDot] \[FormalH]))) == 
           (\[FormalG] \[CenterDot] ((\[FormalH] \[CenterDot] 
               \[FormalI]) \[CenterDot] (\[FormalH] \[CenterDot] 
               \[FormalI]))) \[CenterDot] (\[FormalG] \[CenterDot] 
             ((\[FormalH] \[CenterDot] \[FormalI]) \[CenterDot] 
              (\[FormalH] \[CenterDot] \[FormalI])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 67} -> 
     <|"Statement" -> HoldForm[(\[FormalI] \[CenterDot] 
           ((\[FormalG] \[CenterDot] \[FormalH]) \[CenterDot] 
            (\[FormalG] \[CenterDot] \[FormalH]))) \[CenterDot] 
          (\[FormalI] \[CenterDot] ((\[FormalG] \[CenterDot] 
             \[FormalH]) \[CenterDot] (\[FormalG] \[CenterDot] 
             \[FormalH]))) == (\[FormalG] \[CenterDot] 
           ((\[FormalH] \[CenterDot] \[FormalI]) \[CenterDot] 
            (\[FormalH] \[CenterDot] \[FormalI]))) \[CenterDot] 
          (\[FormalG] \[CenterDot] ((\[FormalH] \[CenterDot] 
             \[FormalI]) \[CenterDot] (\[FormalH] \[CenterDot] 
             \[FormalI])))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 66}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 79}, "Orientation" -> 1, 
        "Rule" -> (((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
            (\[FormalC]_)) \[CenterDot] (\[FormalD]_) -> 
          \[FormalD] \[CenterDot] (\[FormalC] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalA])), "OutputExpression" -> 
         HoldForm[(\[FormalI] \[CenterDot] ((\[FormalG] \[CenterDot] 
               \[FormalH]) \[CenterDot] (\[FormalG] \[CenterDot] 
               \[FormalH]))) \[CenterDot] (\[FormalI] \[CenterDot] 
             ((\[FormalG] \[CenterDot] \[FormalH]) \[CenterDot] 
              (\[FormalG] \[CenterDot] \[FormalH]))) == 
           (\[FormalG] \[CenterDot] ((\[FormalH] \[CenterDot] 
               \[FormalI]) \[CenterDot] (\[FormalH] \[CenterDot] 
               \[FormalI]))) \[CenterDot] (\[FormalG] \[CenterDot] 
             ((\[FormalH] \[CenterDot] \[FormalI]) \[CenterDot] 
              (\[FormalH] \[CenterDot] \[FormalI])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 68} -> 
     <|"Statement" -> HoldForm[(\[FormalI] \[CenterDot] 
           ((\[FormalG] \[CenterDot] \[FormalH]) \[CenterDot] 
            (\[FormalG] \[CenterDot] \[FormalH]))) \[CenterDot] 
          (\[FormalI] \[CenterDot] ((\[FormalG] \[CenterDot] 
             \[FormalH]) \[CenterDot] \[FormalI])) == 
         (\[FormalG] \[CenterDot] ((\[FormalH] \[CenterDot] 
             \[FormalI]) \[CenterDot] (\[FormalH] \[CenterDot] 
             \[FormalI]))) \[CenterDot] (\[FormalG] \[CenterDot] 
           ((\[FormalH] \[CenterDot] \[FormalI]) \[CenterDot] 
            (\[FormalH] \[CenterDot] \[FormalI])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 67}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 75}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalB]_))) \[CenterDot] (\[FormalC]_) -> 
          \[FormalC] \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalA])), "OutputExpression" -> 
         HoldForm[(\[FormalI] \[CenterDot] ((\[FormalG] \[CenterDot] 
               \[FormalH]) \[CenterDot] (\[FormalG] \[CenterDot] 
               \[FormalH]))) \[CenterDot] (\[FormalI] \[CenterDot] 
             ((\[FormalG] \[CenterDot] \[FormalH]) \[CenterDot] 
              \[FormalI])) == (\[FormalG] \[CenterDot] 
             ((\[FormalH] \[CenterDot] \[FormalI]) \[CenterDot] 
              (\[FormalH] \[CenterDot] \[FormalI]))) \[CenterDot] 
            (\[FormalG] \[CenterDot] ((\[FormalH] \[CenterDot] 
               \[FormalI]) \[CenterDot] (\[FormalH] \[CenterDot] 
               \[FormalI])))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 69} -> 
     <|"Statement" -> HoldForm[(\[FormalI] \[CenterDot] 
           ((\[FormalG] \[CenterDot] \[FormalH]) \[CenterDot] 
            \[FormalI])) \[CenterDot] (((\[FormalG] \[CenterDot] 
             \[FormalH]) \[CenterDot] (\[FormalG] \[CenterDot] 
             \[FormalH])) \[CenterDot] \[FormalI]) == 
         (\[FormalG] \[CenterDot] ((\[FormalH] \[CenterDot] 
             \[FormalI]) \[CenterDot] (\[FormalH] \[CenterDot] 
             \[FormalI]))) \[CenterDot] (\[FormalG] \[CenterDot] 
           ((\[FormalH] \[CenterDot] \[FormalI]) \[CenterDot] 
            (\[FormalH] \[CenterDot] \[FormalI])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 68}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 80}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] (\[FormalD]_) -> 
          \[FormalD] \[CenterDot] ((\[FormalC] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA]), "OutputExpression" -> 
         HoldForm[(\[FormalI] \[CenterDot] ((\[FormalG] \[CenterDot] 
               \[FormalH]) \[CenterDot] \[FormalI])) \[CenterDot] 
            (((\[FormalG] \[CenterDot] \[FormalH]) \[CenterDot] 
              (\[FormalG] \[CenterDot] \[FormalH])) \[CenterDot] 
             \[FormalI]) == (\[FormalG] \[CenterDot] 
             ((\[FormalH] \[CenterDot] \[FormalI]) \[CenterDot] 
              (\[FormalH] \[CenterDot] \[FormalI]))) \[CenterDot] 
            (\[FormalG] \[CenterDot] ((\[FormalH] \[CenterDot] 
               \[FormalI]) \[CenterDot] (\[FormalH] \[CenterDot] 
               \[FormalI])))], "ConstructSide" -> 2, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 70} -> 
     <|"Statement" -> HoldForm[(\[FormalI] \[CenterDot] 
           ((\[FormalG] \[CenterDot] \[FormalH]) \[CenterDot] 
            \[FormalI])) \[CenterDot] (\[FormalI] \[CenterDot] 
           ((\[FormalG] \[CenterDot] \[FormalH]) \[CenterDot] \[FormalI])) == 
         (\[FormalG] \[CenterDot] ((\[FormalH] \[CenterDot] 
             \[FormalI]) \[CenterDot] (\[FormalH] \[CenterDot] 
             \[FormalI]))) \[CenterDot] (\[FormalG] \[CenterDot] 
           ((\[FormalH] \[CenterDot] \[FormalI]) \[CenterDot] 
            (\[FormalH] \[CenterDot] \[FormalI])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 69}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 53}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalC]_)) -> 
          (\[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalC])) \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalI] \[CenterDot] ((\[FormalG] \[CenterDot] 
               \[FormalH]) \[CenterDot] \[FormalI])) \[CenterDot] 
            (\[FormalI] \[CenterDot] ((\[FormalG] \[CenterDot] 
               \[FormalH]) \[CenterDot] \[FormalI])) == 
           (\[FormalG] \[CenterDot] ((\[FormalH] \[CenterDot] 
               \[FormalI]) \[CenterDot] (\[FormalH] \[CenterDot] 
               \[FormalI]))) \[CenterDot] (\[FormalG] \[CenterDot] 
             ((\[FormalH] \[CenterDot] \[FormalI]) \[CenterDot] 
              (\[FormalH] \[CenterDot] \[FormalI])))], "ConstructSide" -> 2, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 71} -> 
     <|"Statement" -> HoldForm[(\[FormalI] \[CenterDot] 
           ((\[FormalG] \[CenterDot] \[FormalH]) \[CenterDot] 
            \[FormalI])) \[CenterDot] (\[FormalI] \[CenterDot] 
           (\[FormalI] \[CenterDot] (\[FormalG] \[CenterDot] \[FormalH]))) == 
         (\[FormalG] \[CenterDot] ((\[FormalH] \[CenterDot] 
             \[FormalI]) \[CenterDot] (\[FormalH] \[CenterDot] 
             \[FormalI]))) \[CenterDot] (\[FormalG] \[CenterDot] 
           ((\[FormalH] \[CenterDot] \[FormalI]) \[CenterDot] 
            (\[FormalH] \[CenterDot] \[FormalI])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 70}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 78}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] (\[FormalD]_) -> 
          \[FormalD] \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalC] \[CenterDot] \[FormalB])), "OutputExpression" -> 
         HoldForm[(\[FormalI] \[CenterDot] ((\[FormalG] \[CenterDot] 
               \[FormalH]) \[CenterDot] \[FormalI])) \[CenterDot] 
            (\[FormalI] \[CenterDot] (\[FormalI] \[CenterDot] 
              (\[FormalG] \[CenterDot] \[FormalH]))) == 
           (\[FormalG] \[CenterDot] ((\[FormalH] \[CenterDot] 
               \[FormalI]) \[CenterDot] (\[FormalH] \[CenterDot] 
               \[FormalI]))) \[CenterDot] (\[FormalG] \[CenterDot] 
             ((\[FormalH] \[CenterDot] \[FormalI]) \[CenterDot] 
              (\[FormalH] \[CenterDot] \[FormalI])))], "ConstructSide" -> 2, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 72} -> 
     <|"Statement" -> HoldForm[(\[FormalI] \[CenterDot] 
           (\[FormalI] \[CenterDot] (\[FormalG] \[CenterDot] 
             \[FormalH]))) \[CenterDot] ((\[FormalI] \[CenterDot] 
            (\[FormalG] \[CenterDot] \[FormalH])) \[CenterDot] \[FormalI]) == 
         (\[FormalG] \[CenterDot] ((\[FormalH] \[CenterDot] 
             \[FormalI]) \[CenterDot] (\[FormalH] \[CenterDot] 
             \[FormalI]))) \[CenterDot] (\[FormalG] \[CenterDot] 
           ((\[FormalH] \[CenterDot] \[FormalI]) \[CenterDot] 
            (\[FormalH] \[CenterDot] \[FormalI])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 71}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 80}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] (\[FormalD]_) -> 
          \[FormalD] \[CenterDot] ((\[FormalC] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA]), "OutputExpression" -> 
         HoldForm[(\[FormalI] \[CenterDot] (\[FormalI] \[CenterDot] 
              (\[FormalG] \[CenterDot] \[FormalH]))) \[CenterDot] 
            ((\[FormalI] \[CenterDot] (\[FormalG] \[CenterDot] 
               \[FormalH])) \[CenterDot] \[FormalI]) == 
           (\[FormalG] \[CenterDot] ((\[FormalH] \[CenterDot] 
               \[FormalI]) \[CenterDot] (\[FormalH] \[CenterDot] 
               \[FormalI]))) \[CenterDot] (\[FormalG] \[CenterDot] 
             ((\[FormalH] \[CenterDot] \[FormalI]) \[CenterDot] 
              (\[FormalH] \[CenterDot] \[FormalI])))], "ConstructSide" -> 2, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 73} -> 
     <|"Statement" -> HoldForm[(\[FormalI] \[CenterDot] 
           (\[FormalI] \[CenterDot] (\[FormalG] \[CenterDot] 
             \[FormalH]))) \[CenterDot] (\[FormalI] \[CenterDot] 
           (\[FormalI] \[CenterDot] (\[FormalG] \[CenterDot] \[FormalH]))) == 
         (\[FormalG] \[CenterDot] ((\[FormalH] \[CenterDot] 
             \[FormalI]) \[CenterDot] (\[FormalH] \[CenterDot] 
             \[FormalI]))) \[CenterDot] (\[FormalG] \[CenterDot] 
           ((\[FormalH] \[CenterDot] \[FormalI]) \[CenterDot] 
            (\[FormalH] \[CenterDot] \[FormalI])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 72}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 50}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalC]_)) -> (\[FormalC] \[CenterDot] 
            \[FormalB]) \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalI] \[CenterDot] (\[FormalI] \[CenterDot] 
              (\[FormalG] \[CenterDot] \[FormalH]))) \[CenterDot] 
            (\[FormalI] \[CenterDot] (\[FormalI] \[CenterDot] 
              (\[FormalG] \[CenterDot] \[FormalH]))) == 
           (\[FormalG] \[CenterDot] ((\[FormalH] \[CenterDot] 
               \[FormalI]) \[CenterDot] (\[FormalH] \[CenterDot] 
               \[FormalI]))) \[CenterDot] (\[FormalG] \[CenterDot] 
             ((\[FormalH] \[CenterDot] \[FormalI]) \[CenterDot] 
              (\[FormalH] \[CenterDot] \[FormalI])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 74} -> 
     <|"Statement" -> HoldForm[
        ((\[FormalI] \[CenterDot] \[FormalI]) \[CenterDot] 
           \[FormalI]) \[CenterDot] (((\[FormalG] \[CenterDot] 
             \[FormalH]) \[CenterDot] (\[FormalG] \[CenterDot] 
             \[FormalH])) \[CenterDot] \[FormalI]) == 
         (\[FormalG] \[CenterDot] ((\[FormalH] \[CenterDot] 
             \[FormalI]) \[CenterDot] (\[FormalH] \[CenterDot] 
             \[FormalI]))) \[CenterDot] (\[FormalG] \[CenterDot] 
           ((\[FormalH] \[CenterDot] \[FormalI]) \[CenterDot] 
            (\[FormalH] \[CenterDot] \[FormalI])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 73}, "Position" -> {}, 
        "Construct" -> {"Axiom", 3}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) -> 
          ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalA]) \[CenterDot] ((\[FormalC] \[CenterDot] 
             \[FormalC]) \[CenterDot] \[FormalA]), "OutputExpression" -> 
         HoldForm[((\[FormalI] \[CenterDot] \[FormalI]) \[CenterDot] 
             \[FormalI]) \[CenterDot] (((\[FormalG] \[CenterDot] 
               \[FormalH]) \[CenterDot] (\[FormalG] \[CenterDot] 
               \[FormalH])) \[CenterDot] \[FormalI]) == 
           (\[FormalG] \[CenterDot] ((\[FormalH] \[CenterDot] 
               \[FormalI]) \[CenterDot] (\[FormalH] \[CenterDot] 
               \[FormalI]))) \[CenterDot] (\[FormalG] \[CenterDot] 
             ((\[FormalH] \[CenterDot] \[FormalI]) \[CenterDot] 
              (\[FormalH] \[CenterDot] \[FormalI])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 75} -> 
     <|"Statement" -> HoldForm[(\[FormalI] \[CenterDot] 
           (\[FormalI] \[CenterDot] \[FormalI])) \[CenterDot] 
          (\[FormalI] \[CenterDot] ((\[FormalG] \[CenterDot] 
             \[FormalH]) \[CenterDot] (\[FormalG] \[CenterDot] 
             \[FormalH]))) == (\[FormalG] \[CenterDot] 
           ((\[FormalH] \[CenterDot] \[FormalI]) \[CenterDot] 
            (\[FormalH] \[CenterDot] \[FormalI]))) \[CenterDot] 
          (\[FormalG] \[CenterDot] ((\[FormalH] \[CenterDot] 
             \[FormalI]) \[CenterDot] (\[FormalH] \[CenterDot] 
             \[FormalI])))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 74}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 77}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalC]_) \[CenterDot] (\[FormalD]_)) -> 
          (\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
           (\[FormalD] \[CenterDot] \[FormalC]), "OutputExpression" -> 
         HoldForm[(\[FormalI] \[CenterDot] (\[FormalI] \[CenterDot] 
              \[FormalI])) \[CenterDot] (\[FormalI] \[CenterDot] 
             ((\[FormalG] \[CenterDot] \[FormalH]) \[CenterDot] 
              (\[FormalG] \[CenterDot] \[FormalH]))) == 
           (\[FormalG] \[CenterDot] ((\[FormalH] \[CenterDot] 
               \[FormalI]) \[CenterDot] (\[FormalH] \[CenterDot] 
               \[FormalI]))) \[CenterDot] (\[FormalG] \[CenterDot] 
             ((\[FormalH] \[CenterDot] \[FormalI]) \[CenterDot] 
              (\[FormalH] \[CenterDot] \[FormalI])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 76} -> 
     <|"Statement" -> HoldForm[(\[FormalI] \[CenterDot] 
           ((\[FormalG] \[CenterDot] \[FormalH]) \[CenterDot] 
            \[FormalI])) \[CenterDot] (\[FormalI] \[CenterDot] 
           (\[FormalI] \[CenterDot] \[FormalI])) == 
         (\[FormalG] \[CenterDot] ((\[FormalH] \[CenterDot] 
             \[FormalI]) \[CenterDot] (\[FormalH] \[CenterDot] 
             \[FormalI]))) \[CenterDot] (\[FormalG] \[CenterDot] 
           ((\[FormalH] \[CenterDot] \[FormalI]) \[CenterDot] 
            (\[FormalH] \[CenterDot] \[FormalI])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 75}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 52}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalC]_))) -> 
          (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
             \[FormalB])) \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalI] \[CenterDot] ((\[FormalG] \[CenterDot] 
               \[FormalH]) \[CenterDot] \[FormalI])) \[CenterDot] 
            (\[FormalI] \[CenterDot] (\[FormalI] \[CenterDot] \[FormalI])) == 
           (\[FormalG] \[CenterDot] ((\[FormalH] \[CenterDot] 
               \[FormalI]) \[CenterDot] (\[FormalH] \[CenterDot] 
               \[FormalI]))) \[CenterDot] (\[FormalG] \[CenterDot] 
             ((\[FormalH] \[CenterDot] \[FormalI]) \[CenterDot] 
              (\[FormalH] \[CenterDot] \[FormalI])))], "ConstructSide" -> 2, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 77} -> 
     <|"Statement" -> HoldForm[(\[FormalI] \[CenterDot] 
           (\[FormalI] \[CenterDot] \[FormalI])) \[CenterDot] 
          ((\[FormalI] \[CenterDot] (\[FormalG] \[CenterDot] 
             \[FormalH])) \[CenterDot] \[FormalI]) == 
         (\[FormalG] \[CenterDot] ((\[FormalH] \[CenterDot] 
             \[FormalI]) \[CenterDot] (\[FormalH] \[CenterDot] 
             \[FormalI]))) \[CenterDot] (\[FormalG] \[CenterDot] 
           ((\[FormalH] \[CenterDot] \[FormalI]) \[CenterDot] 
            (\[FormalH] \[CenterDot] \[FormalI])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 76}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 80}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] (\[FormalD]_) -> 
          \[FormalD] \[CenterDot] ((\[FormalC] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA]), "OutputExpression" -> 
         HoldForm[(\[FormalI] \[CenterDot] (\[FormalI] \[CenterDot] 
              \[FormalI])) \[CenterDot] ((\[FormalI] \[CenterDot] 
              (\[FormalG] \[CenterDot] \[FormalH])) \[CenterDot] 
             \[FormalI]) == (\[FormalG] \[CenterDot] 
             ((\[FormalH] \[CenterDot] \[FormalI]) \[CenterDot] 
              (\[FormalH] \[CenterDot] \[FormalI]))) \[CenterDot] 
            (\[FormalG] \[CenterDot] ((\[FormalH] \[CenterDot] 
               \[FormalI]) \[CenterDot] (\[FormalH] \[CenterDot] 
               \[FormalI])))], "ConstructSide" -> 2, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 78} -> 
     <|"Statement" -> HoldForm[(\[FormalI] \[CenterDot] 
           (\[FormalI] \[CenterDot] \[FormalI])) \[CenterDot] 
          (\[FormalI] \[CenterDot] ((\[FormalH] \[CenterDot] 
             \[FormalG]) \[CenterDot] \[FormalI])) == 
         (\[FormalG] \[CenterDot] ((\[FormalH] \[CenterDot] 
             \[FormalI]) \[CenterDot] (\[FormalH] \[CenterDot] 
             \[FormalI]))) \[CenterDot] (\[FormalG] \[CenterDot] 
           ((\[FormalH] \[CenterDot] \[FormalI]) \[CenterDot] 
            (\[FormalH] \[CenterDot] \[FormalI])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 77}, "Position" -> {2}, 
        "Construct" -> {"CriticalPairLemma", 80}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] (\[FormalD]_) -> 
          \[FormalD] \[CenterDot] ((\[FormalC] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA]), "OutputExpression" -> 
         HoldForm[(\[FormalI] \[CenterDot] (\[FormalI] \[CenterDot] 
              \[FormalI])) \[CenterDot] (\[FormalI] \[CenterDot] 
             ((\[FormalH] \[CenterDot] \[FormalG]) \[CenterDot] 
              \[FormalI])) == (\[FormalG] \[CenterDot] 
             ((\[FormalH] \[CenterDot] \[FormalI]) \[CenterDot] 
              (\[FormalH] \[CenterDot] \[FormalI]))) \[CenterDot] 
            (\[FormalG] \[CenterDot] ((\[FormalH] \[CenterDot] 
               \[FormalI]) \[CenterDot] (\[FormalH] \[CenterDot] 
               \[FormalI])))], "ConstructSide" -> 2, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 79} -> 
     <|"Statement" -> HoldForm[(\[FormalI] \[CenterDot] 
           (\[FormalI] \[CenterDot] \[FormalI])) \[CenterDot] 
          (\[FormalI] \[CenterDot] (\[FormalI] \[CenterDot] 
            (\[FormalG] \[CenterDot] \[FormalH]))) == 
         (\[FormalG] \[CenterDot] ((\[FormalH] \[CenterDot] 
             \[FormalI]) \[CenterDot] (\[FormalH] \[CenterDot] 
             \[FormalI]))) \[CenterDot] (\[FormalG] \[CenterDot] 
           ((\[FormalH] \[CenterDot] \[FormalI]) \[CenterDot] 
            (\[FormalH] \[CenterDot] \[FormalI])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 78}, 
        "Position" -> {2, 2}, "Construct" -> {"SubstitutionLemma", 50}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (\[FormalC]_) -> 
          \[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalA]), 
        "OutputExpression" -> HoldForm[(\[FormalI] \[CenterDot] 
             (\[FormalI] \[CenterDot] \[FormalI])) \[CenterDot] 
            (\[FormalI] \[CenterDot] (\[FormalI] \[CenterDot] 
              (\[FormalG] \[CenterDot] \[FormalH]))) == 
           (\[FormalG] \[CenterDot] ((\[FormalH] \[CenterDot] 
               \[FormalI]) \[CenterDot] (\[FormalH] \[CenterDot] 
               \[FormalI]))) \[CenterDot] (\[FormalG] \[CenterDot] 
             ((\[FormalH] \[CenterDot] \[FormalI]) \[CenterDot] 
              (\[FormalH] \[CenterDot] \[FormalI])))], "ConstructSide" -> 2, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 80} -> 
     <|"Statement" -> HoldForm[(\[FormalI] \[CenterDot] 
           (\[FormalI] \[CenterDot] \[FormalI])) \[CenterDot] 
          (\[FormalI] \[CenterDot] (\[FormalI] \[CenterDot] 
            (\[FormalH] \[CenterDot] \[FormalG]))) == 
         (\[FormalG] \[CenterDot] ((\[FormalH] \[CenterDot] 
             \[FormalI]) \[CenterDot] (\[FormalH] \[CenterDot] 
             \[FormalI]))) \[CenterDot] (\[FormalG] \[CenterDot] 
           ((\[FormalH] \[CenterDot] \[FormalI]) \[CenterDot] 
            (\[FormalH] \[CenterDot] \[FormalI])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 79}, 
        "Position" -> {2, 2, 2}, "Construct" -> {"SubstitutionLemma", 3}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (\[FormalB]_) -> \[FormalB] \[CenterDot] \[FormalA], 
        "OutputExpression" -> HoldForm[(\[FormalI] \[CenterDot] 
             (\[FormalI] \[CenterDot] \[FormalI])) \[CenterDot] 
            (\[FormalI] \[CenterDot] (\[FormalI] \[CenterDot] 
              (\[FormalH] \[CenterDot] \[FormalG]))) == 
           (\[FormalG] \[CenterDot] ((\[FormalH] \[CenterDot] 
               \[FormalI]) \[CenterDot] (\[FormalH] \[CenterDot] 
               \[FormalI]))) \[CenterDot] (\[FormalG] \[CenterDot] 
             ((\[FormalH] \[CenterDot] \[FormalI]) \[CenterDot] 
              (\[FormalH] \[CenterDot] \[FormalI])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 81} -> 
     <|"Statement" -> HoldForm[(\[FormalI] \[CenterDot] 
           (\[FormalI] \[CenterDot] \[FormalI])) \[CenterDot] 
          (\[FormalI] \[CenterDot] (\[FormalI] \[CenterDot] 
            (\[FormalH] \[CenterDot] \[FormalG]))) == 
         (\[FormalG] \[CenterDot] ((\[FormalH] \[CenterDot] 
             \[FormalI]) \[CenterDot] (\[FormalH] \[CenterDot] 
             \[FormalI]))) \[CenterDot] (\[FormalG] \[CenterDot] 
           ((\[FormalH] \[CenterDot] \[FormalI]) \[CenterDot] \[FormalG]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 80}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 75}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalB]_))) \[CenterDot] (\[FormalC]_) -> 
          \[FormalC] \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalB] \[CenterDot] \[FormalA])), "OutputExpression" -> 
         HoldForm[(\[FormalI] \[CenterDot] (\[FormalI] \[CenterDot] 
              \[FormalI])) \[CenterDot] (\[FormalI] \[CenterDot] 
             (\[FormalI] \[CenterDot] (\[FormalH] \[CenterDot] 
               \[FormalG]))) == (\[FormalG] \[CenterDot] 
             ((\[FormalH] \[CenterDot] \[FormalI]) \[CenterDot] 
              (\[FormalH] \[CenterDot] \[FormalI]))) \[CenterDot] 
            (\[FormalG] \[CenterDot] ((\[FormalH] \[CenterDot] 
               \[FormalI]) \[CenterDot] \[FormalG]))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 82} -> 
     <|"Statement" -> HoldForm[(\[FormalI] \[CenterDot] 
           (\[FormalI] \[CenterDot] \[FormalI])) \[CenterDot] 
          (\[FormalI] \[CenterDot] (\[FormalI] \[CenterDot] 
            (\[FormalH] \[CenterDot] \[FormalG]))) == 
         (\[FormalG] \[CenterDot] ((\[FormalH] \[CenterDot] 
             \[FormalI]) \[CenterDot] \[FormalG])) \[CenterDot] 
          (((\[FormalH] \[CenterDot] \[FormalI]) \[CenterDot] 
            (\[FormalH] \[CenterDot] \[FormalI])) \[CenterDot] \[FormalG])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 81}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 80}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] (\[FormalD]_) -> 
          \[FormalD] \[CenterDot] ((\[FormalC] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA]), "OutputExpression" -> 
         HoldForm[(\[FormalI] \[CenterDot] (\[FormalI] \[CenterDot] 
              \[FormalI])) \[CenterDot] (\[FormalI] \[CenterDot] 
             (\[FormalI] \[CenterDot] (\[FormalH] \[CenterDot] 
               \[FormalG]))) == (\[FormalG] \[CenterDot] 
             ((\[FormalH] \[CenterDot] \[FormalI]) \[CenterDot] 
              \[FormalG])) \[CenterDot] (((\[FormalH] \[CenterDot] 
               \[FormalI]) \[CenterDot] (\[FormalH] \[CenterDot] 
               \[FormalI])) \[CenterDot] \[FormalG])], "ConstructSide" -> 2, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 83} -> 
     <|"Statement" -> HoldForm[(\[FormalI] \[CenterDot] 
           (\[FormalI] \[CenterDot] \[FormalI])) \[CenterDot] 
          (\[FormalI] \[CenterDot] (\[FormalI] \[CenterDot] 
            (\[FormalH] \[CenterDot] \[FormalG]))) == 
         (\[FormalG] \[CenterDot] ((\[FormalH] \[CenterDot] 
             \[FormalI]) \[CenterDot] \[FormalG])) \[CenterDot] 
          (\[FormalG] \[CenterDot] ((\[FormalH] \[CenterDot] 
             \[FormalI]) \[CenterDot] \[FormalG]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 82}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 53}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (((\[FormalB]_) \[CenterDot] 
             (\[FormalB]_)) \[CenterDot] (\[FormalC]_)) -> 
          (\[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] 
             \[FormalC])) \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalI] \[CenterDot] (\[FormalI] \[CenterDot] 
              \[FormalI])) \[CenterDot] (\[FormalI] \[CenterDot] 
             (\[FormalI] \[CenterDot] (\[FormalH] \[CenterDot] 
               \[FormalG]))) == (\[FormalG] \[CenterDot] 
             ((\[FormalH] \[CenterDot] \[FormalI]) \[CenterDot] 
              \[FormalG])) \[CenterDot] (\[FormalG] \[CenterDot] 
             ((\[FormalH] \[CenterDot] \[FormalI]) \[CenterDot] 
              \[FormalG]))], "ConstructSide" -> 2, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 84} -> 
     <|"Statement" -> HoldForm[(\[FormalI] \[CenterDot] 
           (\[FormalI] \[CenterDot] \[FormalI])) \[CenterDot] 
          (\[FormalI] \[CenterDot] (\[FormalI] \[CenterDot] 
            (\[FormalH] \[CenterDot] \[FormalG]))) == 
         (\[FormalG] \[CenterDot] ((\[FormalH] \[CenterDot] 
             \[FormalI]) \[CenterDot] \[FormalG])) \[CenterDot] 
          (\[FormalG] \[CenterDot] (\[FormalG] \[CenterDot] 
            (\[FormalH] \[CenterDot] \[FormalI])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 83}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 78}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] (\[FormalD]_) -> 
          \[FormalD] \[CenterDot] (\[FormalA] \[CenterDot] 
            (\[FormalC] \[CenterDot] \[FormalB])), "OutputExpression" -> 
         HoldForm[(\[FormalI] \[CenterDot] (\[FormalI] \[CenterDot] 
              \[FormalI])) \[CenterDot] (\[FormalI] \[CenterDot] 
             (\[FormalI] \[CenterDot] (\[FormalH] \[CenterDot] 
               \[FormalG]))) == (\[FormalG] \[CenterDot] 
             ((\[FormalH] \[CenterDot] \[FormalI]) \[CenterDot] 
              \[FormalG])) \[CenterDot] (\[FormalG] \[CenterDot] 
             (\[FormalG] \[CenterDot] (\[FormalH] \[CenterDot] 
               \[FormalI])))], "ConstructSide" -> 2, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 85} -> 
     <|"Statement" -> HoldForm[(\[FormalI] \[CenterDot] 
           (\[FormalI] \[CenterDot] \[FormalI])) \[CenterDot] 
          (\[FormalI] \[CenterDot] (\[FormalI] \[CenterDot] 
            (\[FormalH] \[CenterDot] \[FormalG]))) == 
         (\[FormalG] \[CenterDot] (\[FormalG] \[CenterDot] 
            (\[FormalH] \[CenterDot] \[FormalI]))) \[CenterDot] 
          ((\[FormalG] \[CenterDot] (\[FormalH] \[CenterDot] 
             \[FormalI])) \[CenterDot] \[FormalG])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 84}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 80}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] (\[FormalD]_) -> 
          \[FormalD] \[CenterDot] ((\[FormalC] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA]), "OutputExpression" -> 
         HoldForm[(\[FormalI] \[CenterDot] (\[FormalI] \[CenterDot] 
              \[FormalI])) \[CenterDot] (\[FormalI] \[CenterDot] 
             (\[FormalI] \[CenterDot] (\[FormalH] \[CenterDot] 
               \[FormalG]))) == (\[FormalG] \[CenterDot] 
             (\[FormalG] \[CenterDot] (\[FormalH] \[CenterDot] 
               \[FormalI]))) \[CenterDot] ((\[FormalG] \[CenterDot] 
              (\[FormalH] \[CenterDot] \[FormalI])) \[CenterDot] 
             \[FormalG])], "ConstructSide" -> 2, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 86} -> 
     <|"Statement" -> HoldForm[(\[FormalI] \[CenterDot] 
           (\[FormalI] \[CenterDot] \[FormalI])) \[CenterDot] 
          (\[FormalI] \[CenterDot] (\[FormalI] \[CenterDot] 
            (\[FormalH] \[CenterDot] \[FormalG]))) == 
         (\[FormalG] \[CenterDot] (\[FormalG] \[CenterDot] 
            (\[FormalH] \[CenterDot] \[FormalI]))) \[CenterDot] 
          (\[FormalG] \[CenterDot] (\[FormalG] \[CenterDot] 
            (\[FormalH] \[CenterDot] \[FormalI])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 85}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 50}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            (\[FormalC]_)) -> (\[FormalC] \[CenterDot] 
            \[FormalB]) \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalI] \[CenterDot] (\[FormalI] \[CenterDot] 
              \[FormalI])) \[CenterDot] (\[FormalI] \[CenterDot] 
             (\[FormalI] \[CenterDot] (\[FormalH] \[CenterDot] 
               \[FormalG]))) == (\[FormalG] \[CenterDot] 
             (\[FormalG] \[CenterDot] (\[FormalH] \[CenterDot] 
               \[FormalI]))) \[CenterDot] (\[FormalG] \[CenterDot] 
             (\[FormalG] \[CenterDot] (\[FormalH] \[CenterDot] 
               \[FormalI])))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 87} -> 
     <|"Statement" -> HoldForm[(\[FormalI] \[CenterDot] 
           (\[FormalI] \[CenterDot] \[FormalI])) \[CenterDot] 
          (\[FormalI] \[CenterDot] (\[FormalI] \[CenterDot] 
            (\[FormalH] \[CenterDot] \[FormalG]))) == 
         ((\[FormalG] \[CenterDot] \[FormalG]) \[CenterDot] 
           \[FormalG]) \[CenterDot] (((\[FormalH] \[CenterDot] 
             \[FormalI]) \[CenterDot] (\[FormalH] \[CenterDot] 
             \[FormalI])) \[CenterDot] \[FormalG])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 86}, "Position" -> {}, 
        "Construct" -> {"Axiom", 3}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) -> 
          ((\[FormalB] \[CenterDot] \[FormalB]) \[CenterDot] 
            \[FormalA]) \[CenterDot] ((\[FormalC] \[CenterDot] 
             \[FormalC]) \[CenterDot] \[FormalA]), "OutputExpression" -> 
         HoldForm[(\[FormalI] \[CenterDot] (\[FormalI] \[CenterDot] 
              \[FormalI])) \[CenterDot] (\[FormalI] \[CenterDot] 
             (\[FormalI] \[CenterDot] (\[FormalH] \[CenterDot] 
               \[FormalG]))) == ((\[FormalG] \[CenterDot] 
              \[FormalG]) \[CenterDot] \[FormalG]) \[CenterDot] 
            (((\[FormalH] \[CenterDot] \[FormalI]) \[CenterDot] 
              (\[FormalH] \[CenterDot] \[FormalI])) \[CenterDot] 
             \[FormalG])], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 88} -> 
     <|"Statement" -> HoldForm[(\[FormalI] \[CenterDot] 
           (\[FormalI] \[CenterDot] \[FormalI])) \[CenterDot] 
          (\[FormalI] \[CenterDot] (\[FormalI] \[CenterDot] 
            (\[FormalH] \[CenterDot] \[FormalG]))) == 
         (\[FormalG] \[CenterDot] (\[FormalG] \[CenterDot] 
            \[FormalG])) \[CenterDot] (\[FormalG] \[CenterDot] 
           ((\[FormalH] \[CenterDot] \[FormalI]) \[CenterDot] 
            (\[FormalH] \[CenterDot] \[FormalI])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 87}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 77}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalC]_) \[CenterDot] (\[FormalD]_)) -> 
          (\[FormalB] \[CenterDot] \[FormalA]) \[CenterDot] 
           (\[FormalD] \[CenterDot] \[FormalC]), "OutputExpression" -> 
         HoldForm[(\[FormalI] \[CenterDot] (\[FormalI] \[CenterDot] 
              \[FormalI])) \[CenterDot] (\[FormalI] \[CenterDot] 
             (\[FormalI] \[CenterDot] (\[FormalH] \[CenterDot] 
               \[FormalG]))) == (\[FormalG] \[CenterDot] 
             (\[FormalG] \[CenterDot] \[FormalG])) \[CenterDot] 
            (\[FormalG] \[CenterDot] ((\[FormalH] \[CenterDot] 
               \[FormalI]) \[CenterDot] (\[FormalH] \[CenterDot] 
               \[FormalI])))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 89} -> 
     <|"Statement" -> HoldForm[(\[FormalI] \[CenterDot] 
           (\[FormalI] \[CenterDot] \[FormalI])) \[CenterDot] 
          (\[FormalI] \[CenterDot] (\[FormalI] \[CenterDot] 
            (\[FormalH] \[CenterDot] \[FormalG]))) == 
         (\[FormalG] \[CenterDot] ((\[FormalH] \[CenterDot] 
             \[FormalI]) \[CenterDot] \[FormalG])) \[CenterDot] 
          (\[FormalG] \[CenterDot] (\[FormalG] \[CenterDot] \[FormalG]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 88}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 52}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
            ((\[FormalC]_) \[CenterDot] (\[FormalC]_))) -> 
          (\[FormalB] \[CenterDot] (\[FormalC] \[CenterDot] 
             \[FormalB])) \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[(\[FormalI] \[CenterDot] (\[FormalI] \[CenterDot] 
              \[FormalI])) \[CenterDot] (\[FormalI] \[CenterDot] 
             (\[FormalI] \[CenterDot] (\[FormalH] \[CenterDot] 
               \[FormalG]))) == (\[FormalG] \[CenterDot] 
             ((\[FormalH] \[CenterDot] \[FormalI]) \[CenterDot] 
              \[FormalG])) \[CenterDot] (\[FormalG] \[CenterDot] 
             (\[FormalG] \[CenterDot] \[FormalG]))], "ConstructSide" -> 2, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 90} -> 
     <|"Statement" -> HoldForm[(\[FormalI] \[CenterDot] 
           (\[FormalI] \[CenterDot] \[FormalI])) \[CenterDot] 
          (\[FormalI] \[CenterDot] (\[FormalI] \[CenterDot] 
            (\[FormalH] \[CenterDot] \[FormalG]))) == 
         (\[FormalG] \[CenterDot] (\[FormalG] \[CenterDot] 
            \[FormalG])) \[CenterDot] ((\[FormalG] \[CenterDot] 
            (\[FormalH] \[CenterDot] \[FormalI])) \[CenterDot] \[FormalG])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 89}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 80}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] (\[FormalD]_) -> 
          \[FormalD] \[CenterDot] ((\[FormalC] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA]), "OutputExpression" -> 
         HoldForm[(\[FormalI] \[CenterDot] (\[FormalI] \[CenterDot] 
              \[FormalI])) \[CenterDot] (\[FormalI] \[CenterDot] 
             (\[FormalI] \[CenterDot] (\[FormalH] \[CenterDot] 
               \[FormalG]))) == (\[FormalG] \[CenterDot] 
             (\[FormalG] \[CenterDot] \[FormalG])) \[CenterDot] 
            ((\[FormalG] \[CenterDot] (\[FormalH] \[CenterDot] 
               \[FormalI])) \[CenterDot] \[FormalG])], "ConstructSide" -> 2, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 91} -> 
     <|"Statement" -> HoldForm[(\[FormalI] \[CenterDot] 
           (\[FormalI] \[CenterDot] \[FormalI])) \[CenterDot] 
          (\[FormalI] \[CenterDot] (\[FormalI] \[CenterDot] 
            (\[FormalH] \[CenterDot] \[FormalG]))) == 
         (\[FormalI] \[CenterDot] (\[FormalI] \[CenterDot] 
            \[FormalI])) \[CenterDot] ((\[FormalG] \[CenterDot] 
            (\[FormalH] \[CenterDot] \[FormalI])) \[CenterDot] \[FormalG])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 90}, "Position" -> {1}, 
        "Construct" -> {"SubstitutionLemma", 5}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            (\[FormalA]_)) -> \[FormalI] \[CenterDot] 
           (\[FormalI] \[CenterDot] \[FormalI]), "OutputExpression" -> 
         HoldForm[(\[FormalI] \[CenterDot] (\[FormalI] \[CenterDot] 
              \[FormalI])) \[CenterDot] (\[FormalI] \[CenterDot] 
             (\[FormalI] \[CenterDot] (\[FormalH] \[CenterDot] 
               \[FormalG]))) == (\[FormalI] \[CenterDot] 
             (\[FormalI] \[CenterDot] \[FormalI])) \[CenterDot] 
            ((\[FormalG] \[CenterDot] (\[FormalH] \[CenterDot] 
               \[FormalI])) \[CenterDot] \[FormalG])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 92} -> 
     <|"Statement" -> HoldForm[(\[FormalI] \[CenterDot] 
           (\[FormalI] \[CenterDot] \[FormalI])) \[CenterDot] 
          (\[FormalI] \[CenterDot] (\[FormalI] \[CenterDot] 
            (\[FormalH] \[CenterDot] \[FormalG]))) == 
         (\[FormalI] \[CenterDot] (\[FormalI] \[CenterDot] 
            \[FormalI])) \[CenterDot] (\[FormalG] \[CenterDot] 
           ((\[FormalI] \[CenterDot] \[FormalH]) \[CenterDot] \[FormalG]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 91}, "Position" -> {2}, 
        "Construct" -> {"CriticalPairLemma", 80}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) \[CenterDot] (\[FormalD]_) -> 
          \[FormalD] \[CenterDot] ((\[FormalC] \[CenterDot] 
             \[FormalB]) \[CenterDot] \[FormalA]), "OutputExpression" -> 
         HoldForm[(\[FormalI] \[CenterDot] (\[FormalI] \[CenterDot] 
              \[FormalI])) \[CenterDot] (\[FormalI] \[CenterDot] 
             (\[FormalI] \[CenterDot] (\[FormalH] \[CenterDot] 
               \[FormalG]))) == (\[FormalI] \[CenterDot] 
             (\[FormalI] \[CenterDot] \[FormalI])) \[CenterDot] 
            (\[FormalG] \[CenterDot] ((\[FormalI] \[CenterDot] 
               \[FormalH]) \[CenterDot] \[FormalG]))], "ConstructSide" -> 2, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 93} -> 
     <|"Statement" -> HoldForm[(\[FormalI] \[CenterDot] 
           (\[FormalI] \[CenterDot] \[FormalI])) \[CenterDot] 
          (\[FormalI] \[CenterDot] (\[FormalI] \[CenterDot] 
            (\[FormalH] \[CenterDot] \[FormalG]))) == 
         (\[FormalI] \[CenterDot] (\[FormalI] \[CenterDot] 
            \[FormalI])) \[CenterDot] (\[FormalG] \[CenterDot] 
           (\[FormalG] \[CenterDot] (\[FormalH] \[CenterDot] \[FormalI])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 92}, 
        "Position" -> {2, 2}, "Construct" -> {"SubstitutionLemma", 50}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CenterDot] 
            (\[FormalB]_)) \[CenterDot] (\[FormalC]_) -> 
          \[FormalC] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalA]), 
        "OutputExpression" -> HoldForm[(\[FormalI] \[CenterDot] 
             (\[FormalI] \[CenterDot] \[FormalI])) \[CenterDot] 
            (\[FormalI] \[CenterDot] (\[FormalI] \[CenterDot] 
              (\[FormalH] \[CenterDot] \[FormalG]))) == 
           (\[FormalI] \[CenterDot] (\[FormalI] \[CenterDot] 
              \[FormalI])) \[CenterDot] (\[FormalG] \[CenterDot] 
             (\[FormalG] \[CenterDot] (\[FormalH] \[CenterDot] 
               \[FormalI])))], "ConstructSide" -> 2, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 94} -> 
     <|"Statement" -> HoldForm[(\[FormalI] \[CenterDot] 
           (\[FormalI] \[CenterDot] \[FormalI])) \[CenterDot] 
          (\[FormalI] \[CenterDot] (\[FormalI] \[CenterDot] 
            (\[FormalH] \[CenterDot] \[FormalG]))) == 
         (\[FormalI] \[CenterDot] (\[FormalI] \[CenterDot] 
            \[FormalI])) \[CenterDot] (\[FormalH] \[CenterDot] 
           (\[FormalH] \[CenterDot] (\[FormalG] \[CenterDot] \[FormalI])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 93}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 65}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) -> 
          \[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalC])), "OutputExpression" -> 
         HoldForm[(\[FormalI] \[CenterDot] (\[FormalI] \[CenterDot] 
              \[FormalI])) \[CenterDot] (\[FormalI] \[CenterDot] 
             (\[FormalI] \[CenterDot] (\[FormalH] \[CenterDot] 
               \[FormalG]))) == (\[FormalI] \[CenterDot] 
             (\[FormalI] \[CenterDot] \[FormalI])) \[CenterDot] 
            (\[FormalH] \[CenterDot] (\[FormalH] \[CenterDot] 
              (\[FormalG] \[CenterDot] \[FormalI])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 95} -> 
     <|"Statement" -> HoldForm[(\[FormalI] \[CenterDot] 
           (\[FormalI] \[CenterDot] \[FormalI])) \[CenterDot] 
          (\[FormalI] \[CenterDot] (\[FormalI] \[CenterDot] 
            (\[FormalH] \[CenterDot] \[FormalG]))) == 
         (\[FormalI] \[CenterDot] (\[FormalI] \[CenterDot] 
            \[FormalI])) \[CenterDot] (\[FormalH] \[CenterDot] 
           (\[FormalH] \[CenterDot] (\[FormalI] \[CenterDot] \[FormalG])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 94}, 
        "Position" -> {2, 2, 2}, "Construct" -> {"SubstitutionLemma", 3}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CenterDot] 
           (\[FormalB]_) -> \[FormalB] \[CenterDot] \[FormalA], 
        "OutputExpression" -> HoldForm[(\[FormalI] \[CenterDot] 
             (\[FormalI] \[CenterDot] \[FormalI])) \[CenterDot] 
            (\[FormalI] \[CenterDot] (\[FormalI] \[CenterDot] 
              (\[FormalH] \[CenterDot] \[FormalG]))) == 
           (\[FormalI] \[CenterDot] (\[FormalI] \[CenterDot] 
              \[FormalI])) \[CenterDot] (\[FormalH] \[CenterDot] 
             (\[FormalH] \[CenterDot] (\[FormalI] \[CenterDot] 
               \[FormalG])))], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"Conclusion", 1} -> 
     <|"Statement" -> HoldForm[(\[FormalI] \[CenterDot] 
           (\[FormalI] \[CenterDot] \[FormalI])) \[CenterDot] 
          (\[FormalI] \[CenterDot] (\[FormalI] \[CenterDot] 
            (\[FormalH] \[CenterDot] \[FormalG]))) == 
         (\[FormalI] \[CenterDot] (\[FormalI] \[CenterDot] 
            \[FormalI])) \[CenterDot] (\[FormalI] \[CenterDot] 
           (\[FormalI] \[CenterDot] (\[FormalH] \[CenterDot] \[FormalG])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 95}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 65}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
            ((\[FormalB]_) \[CenterDot] (\[FormalC]_))) -> 
          \[FormalB] \[CenterDot] (\[FormalB] \[CenterDot] 
            (\[FormalA] \[CenterDot] \[FormalC])), "OutputExpression" -> 
         HoldForm[(\[FormalI] \[CenterDot] (\[FormalI] \[CenterDot] 
              \[FormalI])) \[CenterDot] (\[FormalI] \[CenterDot] 
             (\[FormalI] \[CenterDot] (\[FormalH] \[CenterDot] 
               \[FormalG]))) == (\[FormalI] \[CenterDot] 
             (\[FormalI] \[CenterDot] \[FormalI])) \[CenterDot] 
            (\[FormalI] \[CenterDot] (\[FormalI] \[CenterDot] 
              (\[FormalH] \[CenterDot] \[FormalG])))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>}|>]
