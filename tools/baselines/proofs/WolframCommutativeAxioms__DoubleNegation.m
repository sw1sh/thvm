ProofObject["EquationalLogic", 
 {ForAll[\[FormalA], (\[FormalA] \[CenterDot] \[FormalA]) \[CenterDot] 
     (\[FormalA] \[CenterDot] \[FormalA]) == \[FormalA]]}, 
 {ForAll[{\[FormalA], \[FormalB]}, \[FormalA] \[CenterDot] \[FormalB] == 
    \[FormalB] \[CenterDot] \[FormalA]], 
  ForAll[{\[FormalA], \[FormalB], \[FormalC]}, 
   (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
     (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalC])) == 
    \[FormalA]]}, <|"Variables" -> {\[FormalA], \[FormalB], \[FormalC], 
    \[FormalD], \[FormalE], \[FormalF]}, "Constants" -> {}, 
  "Proof" -> {{"Axiom", 1} -> <|"Statement" -> 
       HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           \[FormalB]) \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalB] \[CenterDot] \[FormalC]))], "Proof" -> <||>|>, 
    {"Axiom", 2} -> <|"Statement" -> HoldForm[
        \[FormalA] \[CenterDot] \[FormalB] == \[FormalB] \[CenterDot] 
          \[FormalA]], "Proof" -> <||>|>, {"Hypothesis", 1} -> 
     <|"Statement" -> HoldForm[(\[FormalF] \[CenterDot] 
           \[FormalF]) \[CenterDot] (\[FormalF] \[CenterDot] \[FormalF]) == 
         \[FormalF]], "Proof" -> <||>|>, {"CriticalPairLemma", 1} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         ((\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
           \[FormalA]) \[CenterDot] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) -> \[FormalA], "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          ((\[FormalB]_) \[CenterDot] (\[FormalC]_)), "MatchingConstruct" -> 
         {"Axiom", 1}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
            \[FormalB]) \[CenterDot] \[FormalA])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 1}, "Position" -> {}, 
        "Construct" -> {"Axiom", 2}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
           \[FormalA] \[CenterDot] ((\[FormalA] \[CenterDot] 
              \[FormalB]) \[CenterDot] \[FormalA])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
         \[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
           (\[FormalA] \[CenterDot] \[FormalB]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 1}, "Position" -> {2}, 
        "Construct" -> {"Axiom", 2}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CenterDot] \[FormalB] == 
           \[FormalA] \[CenterDot] (\[FormalA] \[CenterDot] 
             (\[FormalA] \[CenterDot] \[FormalB]))], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, {"CriticalPairLemma", 2} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CenterDot] 
           \[FormalA]) \[CenterDot] (\[FormalA] \[CenterDot] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalB]_) \[CenterDot] 
             (\[FormalC]_))) -> \[FormalA], "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CenterDot] 
          ((\[FormalB]_) \[CenterDot] (\[FormalC]_)), "MatchingConstruct" -> 
         {"SubstitutionLemma", 2}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] ((\[FormalA]_) \[CenterDot] 
             (\[FormalB]_))) -> \[FormalA] \[CenterDot] \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"Conclusion", 1} -> <|"Statement" -> HoldForm[\[FormalF] == \[FormalF]], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 2}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CenterDot] (\[FormalA]_)) \[CenterDot] 
           ((\[FormalA]_) \[CenterDot] (\[FormalB]_)) -> \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalF] == \[FormalF]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>}|>]
