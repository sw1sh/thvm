ProofObject["EquationalLogic", {ForAll[{\[FormalA], \[FormalB]}, 
   \[FormalA] \[CirclePlus] \[FormalB] == \[FormalA] \[CirclePlus] 
     OverBar[\[FormalA]] \[CircleTimes] \[FormalB]]}, 
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
    \[FormalK], \[FormalL], \[FormalM], \[FormalN], \[FormalO], \[FormalP]}, 
  "Constants" -> {}, "Proof" -> 
   {{"Axiom", 1} -> <|"Statement" -> HoldForm[\[FormalA] == 
         \[FormalA] \[CircleTimes] (\[FormalB] \[CirclePlus] 
           OverBar[\[FormalB]])], "Proof" -> <||>|>, 
    {"Axiom", 2} -> <|"Statement" -> HoldForm[
        \[FormalA] \[CirclePlus] \[FormalB] \[CircleTimes] \[FormalC] == 
         (\[FormalA] \[CirclePlus] \[FormalB]) \[CircleTimes] 
          (\[FormalA] \[CirclePlus] \[FormalC])], "Proof" -> <||>|>, 
    {"Axiom", 3} -> <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          \[FormalB] == \[FormalB] \[CircleTimes] \[FormalA]], 
      "Proof" -> <||>|>, {"Hypothesis", 1} -> 
     <|"Statement" -> HoldForm[\[FormalO] \[CirclePlus] 
          OverBar[\[FormalO]] \[CircleTimes] \[FormalP] == 
         \[FormalO] \[CirclePlus] \[FormalP]], "Proof" -> <||>|>, 
    {"CriticalPairLemma", 1} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CirclePlus] 
          \[FormalB] \[CircleTimes] OverBar[\[FormalA]] == 
         \[FormalA] \[CirclePlus] \[FormalB]], 
      "Proof" -> <|"Construct" -> {"Axiom", 2}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CirclePlus] (\[FormalB]_)) \[CircleTimes] 
           ((\[FormalA]_) \[CirclePlus] (\[FormalC]_)) -> 
          \[FormalA] \[CirclePlus] \[FormalB] \[CircleTimes] \[FormalC], 
        "Side" -> 1, "Subpattern" -> ((\[FormalA]_) \[CirclePlus] 
           (\[FormalB]_)) \[CircleTimes] ((\[FormalA]_) \[CirclePlus] 
           (\[FormalC]_)), "MatchingConstruct" -> {"Axiom", 1}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleTimes] ((\[FormalB]_) \[CirclePlus] 
            OverBar[\[FormalB]_]) -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[\[FormalO] \[CirclePlus] 
          \[FormalP] \[CircleTimes] OverBar[\[FormalO]] == 
         \[FormalO] \[CirclePlus] \[FormalP]], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Position" -> {2}, 
        "Construct" -> {"Axiom", 3}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] (\[FormalB]_) -> 
          \[FormalB] \[CircleTimes] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalO] \[CirclePlus] \[FormalP] \[CircleTimes] 
             OverBar[\[FormalO]] == \[FormalO] \[CirclePlus] \[FormalP]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"Conclusion", 1} -> <|"Statement" -> 
       HoldForm[\[FormalO] \[CirclePlus] \[FormalP] == 
         \[FormalO] \[CirclePlus] \[FormalP]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 1}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 1}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CirclePlus] (\[FormalB]_) \[CircleTimes] 
            OverBar[\[FormalA]_] -> \[FormalA] \[CirclePlus] \[FormalB], 
        "OutputExpression" -> HoldForm[\[FormalO] \[CirclePlus] \[FormalP] == 
           \[FormalO] \[CirclePlus] \[FormalP]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>}|>]
