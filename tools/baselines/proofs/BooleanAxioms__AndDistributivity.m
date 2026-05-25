ProofObject["EquationalLogic", {ForAll[{\[FormalA], \[FormalB], \[FormalC]}, 
   \[FormalA] \[CircleTimes] (\[FormalB] \[CirclePlus] \[FormalC]) == 
    \[FormalA] \[CircleTimes] \[FormalB] \[CirclePlus] 
     \[FormalA] \[CircleTimes] \[FormalC]]}, 
 {ForAll[{\[FormalA], \[FormalB]}, \[FormalA] \[CircleTimes] \[FormalB] == 
    \[FormalB] \[CircleTimes] \[FormalA]], 
  ForAll[{\[FormalA], \[FormalB], \[FormalC]}, 
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
    \[FormalK], \[FormalL], \[FormalM], \[FormalN], \[FormalO], \[FormalP], 
    \[FormalQ]}, "Constants" -> {}, 
  "Proof" -> {{"Axiom", 1} -> <|"Statement" -> 
       HoldForm[\[FormalA] \[CircleTimes] \[FormalB] \[CirclePlus] 
          \[FormalA] \[CircleTimes] \[FormalC] == \[FormalA] \[CircleTimes] 
          (\[FormalB] \[CirclePlus] \[FormalC])], "Proof" -> <||>|>, 
    {"Hypothesis", 1} -> <|"Statement" -> 
       HoldForm[\[FormalO] \[CircleTimes] \[FormalP] \[CirclePlus] 
          \[FormalO] \[CircleTimes] \[FormalQ] == \[FormalO] \[CircleTimes] 
          (\[FormalP] \[CirclePlus] \[FormalQ])], "Proof" -> <||>|>, 
    {"Conclusion", 1} -> <|"Statement" -> 
       HoldForm[\[FormalO] \[CircleTimes] (\[FormalP] \[CirclePlus] 
           \[FormalQ]) == \[FormalO] \[CircleTimes] (\[FormalP] \[CirclePlus] 
           \[FormalQ])], "Proof" -> <|"Input" -> {"Hypothesis", 1}, 
        "Position" -> {}, "Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] (\[FormalB]_) \[CirclePlus] 
           (\[FormalA]_) \[CircleTimes] (\[FormalC]_) -> 
          \[FormalA] \[CircleTimes] (\[FormalB] \[CirclePlus] \[FormalC]), 
        "OutputExpression" -> HoldForm[\[FormalO] \[CircleTimes] 
            (\[FormalP] \[CirclePlus] \[FormalQ]) == 
           \[FormalO] \[CircleTimes] (\[FormalP] \[CirclePlus] \[FormalQ])], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>}|>]
