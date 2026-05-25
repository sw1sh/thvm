ProofObject["EquationalLogic", {ForAll[{\[FormalA], \[FormalB]}, 
   \[FormalA] \[CenterDot] \[FormalB] == \[FormalB] \[CenterDot] 
     \[FormalA]]}, {ForAll[{\[FormalA], \[FormalB]}, 
   \[FormalA] \[CenterDot] \[FormalB] == \[FormalB] \[CenterDot] \[FormalA]], 
  ForAll[{\[FormalA], \[FormalB], \[FormalC]}, 
   (\[FormalA] \[CenterDot] \[FormalB]) \[CenterDot] 
     (\[FormalA] \[CenterDot] (\[FormalB] \[CenterDot] \[FormalC])) == 
    \[FormalA]]}, <|"Variables" -> {\[FormalA], \[FormalB], \[FormalC], 
    \[FormalD], \[FormalE], \[FormalF], \[FormalG]}, "Constants" -> {}, 
  "Proof" -> {{"Axiom", 1} -> <|"Statement" -> 
       HoldForm[\[FormalA] \[CenterDot] \[FormalB] == \[FormalB] \[CenterDot] 
          \[FormalA]], "Proof" -> <||>|>, {"Hypothesis", 1} -> 
     <|"Statement" -> HoldForm[\[FormalF] \[CenterDot] \[FormalG] == 
         \[FormalG] \[CenterDot] \[FormalF]], "Proof" -> <||>|>, 
    {"Conclusion", 1} -> <|"Statement" -> 
       HoldForm[\[FormalG] \[CenterDot] \[FormalF] == \[FormalG] \[CenterDot] 
          \[FormalF]], "Proof" -> <|"Input" -> {"Hypothesis", 1}, 
        "Position" -> {}, "Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CenterDot] (\[FormalB]_) -> 
          \[FormalB] \[CenterDot] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalG] \[CenterDot] \[FormalF] == 
           \[FormalG] \[CenterDot] \[FormalF]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>}|>]
