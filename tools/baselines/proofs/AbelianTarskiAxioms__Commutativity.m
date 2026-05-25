ProofObject["EquationalLogic", {ForAll[{\[FormalA], \[FormalB]}, 
   \[FormalA] \[CircleDot] ((\[FormalA] \[CircleDot] \[FormalA]) \[CircleDot] 
      \[FormalB]) == \[FormalB] \[CircleDot] 
     ((\[FormalA] \[CircleDot] \[FormalA]) \[CircleDot] \[FormalA])]}, 
 {ForAll[{\[FormalA], \[FormalB], \[FormalC]}, 
   \[FormalA] \[CircleDot] (\[FormalB] \[CircleDot] (\[FormalC] \[CircleDot] 
       (\[FormalA] \[CircleDot] \[FormalB]))) == \[FormalC]]}, 
 <|"Variables" -> {\[FormalA], \[FormalB], \[FormalC], \[FormalD], 
    \[FormalE]}, "Constants" -> {}, 
  "Proof" -> {{"Axiom", 1} -> <|"Statement" -> 
       HoldForm[\[FormalA] == \[FormalB] \[CircleDot] 
          (\[FormalC] \[CircleDot] (\[FormalA] \[CircleDot] 
            (\[FormalB] \[CircleDot] \[FormalC])))], "Proof" -> <||>|>, 
    {"Hypothesis", 1} -> <|"Statement" -> 
       HoldForm[\[FormalE] \[CircleDot] ((\[FormalD] \[CircleDot] 
            \[FormalD]) \[CircleDot] \[FormalD]) == \[FormalD] \[CircleDot] 
          ((\[FormalD] \[CircleDot] \[FormalD]) \[CircleDot] \[FormalE])], 
      "Proof" -> <||>|>, {"CriticalPairLemma", 1} -> 
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
     <|"Statement" -> HoldForm[\[FormalA] \[CircleDot] 
          (\[FormalB] \[CircleDot] \[FormalC]) == \[FormalC] \[CircleDot] 
          (\[FormalB] \[CircleDot] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 1}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CircleDot] 
           ((\[FormalA]_) \[CircleDot] (\[FormalB]_)) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CircleDot] 
          (\[FormalB]_), "MatchingConstruct" -> {"CriticalPairLemma", 3}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleDot] ((\[FormalB]_) \[CircleDot] 
            ((\[FormalC]_) \[CircleDot] (\[FormalA]_))) -> 
          \[FormalC] \[CircleDot] \[FormalB], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"Conclusion", 1} -> 
     <|"Statement" -> HoldForm[\[FormalD] \[CircleDot] 
          ((\[FormalD] \[CircleDot] \[FormalD]) \[CircleDot] \[FormalE]) == 
         \[FormalD] \[CircleDot] ((\[FormalD] \[CircleDot] 
            \[FormalD]) \[CircleDot] \[FormalE])], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 5}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CircleDot] ((\[FormalB]_) \[CircleDot] 
            (\[FormalC]_)) -> \[FormalC] \[CircleDot] 
           (\[FormalB] \[CircleDot] \[FormalA]), "OutputExpression" -> 
         HoldForm[\[FormalD] \[CircleDot] ((\[FormalD] \[CircleDot] 
              \[FormalD]) \[CircleDot] \[FormalE]) == \[FormalD] \[CircleDot] 
            ((\[FormalD] \[CircleDot] \[FormalD]) \[CircleDot] \[FormalE])], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>}|>]
