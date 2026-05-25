ProofObject["EquationalLogic", {ForAll[{\[FormalA], \[FormalB]}, 
   \[FormalA] \[CircleTimes] \[FormalB] == \[FormalB] \[CircleTimes] 
     \[FormalA]]}, {ForAll[{\[FormalA], \[FormalB]}, 
   \[FormalA] \[CircleTimes] \[FormalB] == \[FormalB] \[CircleTimes] 
     \[FormalA]], ForAll[{\[FormalA], \[FormalB], \[FormalC]}, 
   \[FormalA] \[CircleTimes] (\[FormalB] \[CirclePlus] \[FormalC]) == 
    \[FormalA] \[CircleTimes] \[FormalB] \[CirclePlus] 
     \[FormalA] \[CircleTimes] \[FormalC]], ForAll[{\[FormalA], \[FormalB]}, 
   \[FormalA] \[CirclePlus] \[FormalB] \[CircleTimes] OverBar[\[FormalB]] == 
    \[FormalA]], ForAll[{\[FormalA], \[FormalB]}, 
   \[FormalA] \[CircleTimes] (\[FormalB] \[CirclePlus] 
      OverBar[\[FormalB]]) == \[FormalA]], ForAll[{\[FormalA], \[FormalB]}, 
   \[FormalA] \[CirclePlus] \[FormalB] == \[FormalB] \[CirclePlus] 
     \[FormalA]], ForAll[{\[FormalA], \[FormalB], \[FormalC]}, 
   \[FormalA] \[CirclePlus] \[FormalB] \[CircleTimes] \[FormalC] == 
    (\[FormalA] \[CirclePlus] \[FormalB]) \[CircleTimes] 
     (\[FormalA] \[CirclePlus] \[FormalC])]}, 
 <|"Variables" -> {\[FormalA], \[FormalB], \[FormalC], \[FormalD], 
    \[FormalE], \[FormalF], \[FormalG], \[FormalH], \[FormalI], \[FormalJ], 
    \[FormalK], \[FormalL], \[FormalM], \[FormalN], \[FormalO], \[FormalP]}, 
  "Constants" -> {}, "Proof" -> 
   {{"Axiom", 1} -> <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          \[FormalB] == \[FormalB] \[CircleTimes] \[FormalA]], 
      "Proof" -> <||>|>, {"Hypothesis", 1} -> 
     <|"Statement" -> HoldForm[\[FormalO] \[CircleTimes] \[FormalP] == 
         \[FormalP] \[CircleTimes] \[FormalO]], "Proof" -> <||>|>, 
    {"Conclusion", 1} -> <|"Statement" -> 
       HoldForm[\[FormalP] \[CircleTimes] \[FormalO] == 
         \[FormalP] \[CircleTimes] \[FormalO]], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Position" -> {}, 
        "Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] (\[FormalB]_) -> 
          \[FormalB] \[CircleTimes] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalP] \[CircleTimes] \[FormalO] == 
           \[FormalP] \[CircleTimes] \[FormalO]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>}|>]
