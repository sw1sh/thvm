ProofObject["EquationalLogic", {ForAll[{\[FormalA], \[FormalB]}, 
   OverBar[\[FormalA]] \[CirclePlus] \[FormalA] == 
    OverBar[\[FormalB]] \[CirclePlus] \[FormalB]]}, 
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
    {"Axiom", 2} -> <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          \[FormalB] == \[FormalB] \[CircleTimes] \[FormalA]], 
      "Proof" -> <||>|>, {"Axiom", 3} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CirclePlus] \[FormalB] == 
         \[FormalB] \[CirclePlus] \[FormalA]], "Proof" -> <||>|>, 
    {"Hypothesis", 1} -> <|"Statement" -> 
       HoldForm[OverBar[\[FormalO]] \[CirclePlus] \[FormalO] == 
         OverBar[\[FormalP]] \[CirclePlus] \[FormalP]], "Proof" -> <||>|>, 
    {"CriticalPairLemma", 1} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CirclePlus] 
           OverBar[\[FormalA]]) \[CircleTimes] \[FormalB] == \[FormalB]], 
      "Proof" -> <|"Construct" -> {"Axiom", 2}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CircleTimes] (\[FormalA]_), "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CircleTimes] (\[FormalB]_), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CircleTimes] 
           ((\[FormalB]_) \[CirclePlus] OverBar[\[FormalB]_]) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"CriticalPairLemma", 2} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CirclePlus] 
          OverBar[\[FormalA]] == \[FormalB] \[CirclePlus] 
          OverBar[\[FormalB]]], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 1}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CirclePlus] OverBar[
             \[FormalA]_]) \[CircleTimes] (\[FormalB]_) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> ((\[FormalA]_) \[CirclePlus] 
           OverBar[\[FormalA]_]) \[CircleTimes] (\[FormalB]_), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CircleTimes] 
           ((\[FormalB]_) \[CirclePlus] OverBar[\[FormalB]_]) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[\[FormalO] \[CirclePlus] 
          OverBar[\[FormalO]] == OverBar[\[FormalP]] \[CirclePlus] 
          \[FormalP]], "Proof" -> <|"Input" -> {"Hypothesis", 1}, 
        "Position" -> {}, "Construct" -> {"Axiom", 3}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CirclePlus] (\[FormalB]_) -> 
          \[FormalB] \[CirclePlus] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalO] \[CirclePlus] OverBar[\[FormalO]] == 
           OverBar[\[FormalP]] \[CirclePlus] \[FormalP]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[\[FormalO] \[CirclePlus] 
          OverBar[\[FormalO]] == \[FormalP] \[CirclePlus] 
          OverBar[\[FormalP]]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 1}, "Position" -> {}, 
        "Construct" -> {"Axiom", 3}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CirclePlus] (\[FormalB]_) -> 
          \[FormalB] \[CirclePlus] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalO] \[CirclePlus] OverBar[\[FormalO]] == 
           \[FormalP] \[CirclePlus] OverBar[\[FormalP]]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"Conclusion", 1} -> <|"Statement" -> 
       HoldForm[\[FormalP] \[CirclePlus] OverBar[\[FormalP]] == 
         \[FormalP] \[CirclePlus] OverBar[\[FormalP]]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 2}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 2}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CirclePlus] OverBar[\[FormalA]_] -> 
          \[FormalP] \[CirclePlus] OverBar[\[FormalP]], 
        "OutputExpression" -> HoldForm[\[FormalP] \[CirclePlus] 
            OverBar[\[FormalP]] == \[FormalP] \[CirclePlus] 
            OverBar[\[FormalP]]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>}|>]
