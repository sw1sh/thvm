ProofObject["EquationalLogic", {ForAll[{\[FormalA], \[FormalB], \[FormalC]}, 
   ((\[FormalA] \[CircleTimes] \[FormalB]) \[CircleTimes] 
      \[FormalC]) \[CircleTimes] OverBar[\[FormalA] \[CircleTimes] 
       \[FormalC]] == \[FormalB]]}, 
 {ForAll[{\[FormalA], \[FormalB], \[FormalC]}, 
   \[FormalA] \[CircleTimes] (\[FormalB] \[CircleTimes] \[FormalC]) == 
    (\[FormalA] \[CircleTimes] \[FormalB]) \[CircleTimes] \[FormalC]], 
  ForAll[{\[FormalA], \[FormalB]}, \[FormalA] \[CircleTimes] \[FormalB] == 
    \[FormalB] \[CircleTimes] \[FormalA]], ForAll[\[FormalA], 
   \[FormalA] \[CircleTimes] OverTilde[1] == \[FormalA]], 
  ForAll[\[FormalA], \[FormalA] \[CircleTimes] OverBar[\[FormalA]] == 
    OverTilde[1]]}, <|"Variables" -> {\[FormalA], \[FormalB], \[FormalC], 
    \[FormalD], \[FormalE], \[FormalF], \[FormalG], \[FormalH], \[FormalI], 
    \[FormalJ]}, "Constants" -> {}, 
  "Proof" -> {{"Axiom", 1} -> <|"Statement" -> 
       HoldForm[\[FormalA] == \[FormalA] \[CircleTimes] OverTilde[1]], 
      "Proof" -> <||>|>, {"Axiom", 2} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] \[FormalB] == 
         \[FormalB] \[CircleTimes] \[FormalA]], "Proof" -> <||>|>, 
    {"Axiom", 3} -> <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          (\[FormalB] \[CircleTimes] \[FormalC]) == 
         (\[FormalA] \[CircleTimes] \[FormalB]) \[CircleTimes] \[FormalC]], 
      "Proof" -> <||>|>, {"Axiom", 4} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          OverBar[\[FormalA]] == OverTilde[1]], "Proof" -> <||>|>, 
    {"Hypothesis", 1} -> <|"Statement" -> 
       HoldForm[((\[FormalH] \[CircleTimes] \[FormalI]) \[CircleTimes] 
           \[FormalJ]) \[CircleTimes] OverBar[\[FormalH] \[CircleTimes] 
            \[FormalJ]] == \[FormalI]], "Proof" -> <||>|>, 
    {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[((\[FormalI] \[CircleTimes] 
            \[FormalH]) \[CircleTimes] \[FormalJ]) \[CircleTimes] 
          OverBar[\[FormalH] \[CircleTimes] \[FormalJ]] == \[FormalI]], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Position" -> {1, 1}, 
        "Construct" -> {"Axiom", 2}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] (\[FormalB]_) -> 
          \[FormalB] \[CircleTimes] \[FormalA], "OutputExpression" -> 
         HoldForm[((\[FormalI] \[CircleTimes] \[FormalH]) \[CircleTimes] 
             \[FormalJ]) \[CircleTimes] OverBar[\[FormalH] \[CircleTimes] 
              \[FormalJ]] == \[FormalI]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[(\[FormalI] \[CircleTimes] 
           (\[FormalH] \[CircleTimes] \[FormalJ])) \[CircleTimes] 
          OverBar[\[FormalH] \[CircleTimes] \[FormalJ]] == \[FormalI]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 1}, "Position" -> {1}, 
        "Construct" -> {"Axiom", 3}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CircleTimes] (\[FormalB]_)) \[CircleTimes] 
           (\[FormalC]_) -> \[FormalA] \[CircleTimes] 
           (\[FormalB] \[CircleTimes] \[FormalC]), "OutputExpression" -> 
         HoldForm[(\[FormalI] \[CircleTimes] (\[FormalH] \[CircleTimes] 
              \[FormalJ])) \[CircleTimes] OverBar[\[FormalH] \[CircleTimes] 
              \[FormalJ]] == \[FormalI]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, {"SubstitutionLemma", 3} -> 
     <|"Statement" -> HoldForm[\[FormalI] \[CircleTimes] 
          ((\[FormalH] \[CircleTimes] \[FormalJ]) \[CircleTimes] 
           OverBar[\[FormalH] \[CircleTimes] \[FormalJ]]) == \[FormalI]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 2}, "Position" -> {}, 
        "Construct" -> {"Axiom", 3}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CircleTimes] (\[FormalB]_)) \[CircleTimes] 
           (\[FormalC]_) -> \[FormalA] \[CircleTimes] 
           (\[FormalB] \[CircleTimes] \[FormalC]), "OutputExpression" -> 
         HoldForm[\[FormalI] \[CircleTimes] ((\[FormalH] \[CircleTimes] 
              \[FormalJ]) \[CircleTimes] OverBar[\[FormalH] \[CircleTimes] 
               \[FormalJ]]) == \[FormalI]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, {"SubstitutionLemma", 4} -> 
     <|"Statement" -> HoldForm[\[FormalI] \[CircleTimes] OverTilde[1] == 
         \[FormalI]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 3}, 
        "Position" -> {2}, "Construct" -> {"Axiom", 4}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] OverBar[\[FormalA]_] -> 
          OverTilde[1], "OutputExpression" -> HoldForm[
          \[FormalI] \[CircleTimes] OverTilde[1] == \[FormalI]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"Conclusion", 1} -> <|"Statement" -> HoldForm[\[FormalI] == \[FormalI]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 4}, "Position" -> {}, 
        "Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] OverTilde[1] -> \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalI] == \[FormalI]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>}|>]
