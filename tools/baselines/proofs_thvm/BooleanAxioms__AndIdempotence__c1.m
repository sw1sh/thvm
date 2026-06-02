ProofObject["EquationalLogic", Inactive[Equal][a, a \[CircleTimes] a], 
 {Inactive[Equal][(a_) \[CircleTimes] (b_), (b_) \[CircleTimes] (a_)], 
  Inactive[Equal][(a_) \[CircleTimes] ((b_) \[CirclePlus] (c_)), 
   (a_) \[CircleTimes] (b_) \[CirclePlus] (a_) \[CircleTimes] (c_)], 
  Inactive[Equal][(a_) \[CirclePlus] (b_) \[CircleTimes] OverBar[b_], a_], 
  Inactive[Equal][(a_) \[CircleTimes] ((b_) \[CirclePlus] OverBar[b_]), a_], 
  Inactive[Equal][(a_) \[CirclePlus] (b_), (b_) \[CirclePlus] (a_)], 
  Inactive[Equal][(a_) \[CirclePlus] (b_) \[CircleTimes] (c_), 
   ((a_) \[CirclePlus] (b_)) \[CircleTimes] ((a_) \[CirclePlus] (c_))]}, 
 <|"Variables" -> {a, b, c}, "Constants" -> {}, 
  "Proof" -> {{"Axiom", 1} -> <|"Statement" -> 
       HoldForm[a \[CircleTimes] b == b \[CircleTimes] a], "Proof" -> <||>|>, 
    {"Axiom", 2} -> <|"Statement" -> HoldForm[
        a \[CircleTimes] (b \[CirclePlus] c) == 
         a \[CircleTimes] b \[CirclePlus] a \[CircleTimes] c], 
      "Proof" -> <||>|>, {"Axiom", 3} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] b \[CircleTimes] OverBar[b] == 
         a], "Proof" -> <||>|>, {"Axiom", 4} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CirclePlus] 
           OverBar[b]) == a], "Proof" -> <||>|>, 
    {"Axiom", 5} -> <|"Statement" -> HoldForm[a \[CirclePlus] b == 
         b \[CirclePlus] a], "Proof" -> <||>|>, 
    {"Axiom", 6} -> <|"Statement" -> HoldForm[
        a \[CirclePlus] b \[CircleTimes] c == 
         (a \[CirclePlus] b) \[CircleTimes] (a \[CirclePlus] c)], 
      "Proof" -> <||>|>, {"Hypothesis", 1} -> 
     <|"Statement" -> HoldForm[a == a \[CircleTimes] a], "Proof" -> <||>|>, 
    {"CriticalPairLemma", 1} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CirclePlus] 
           OverBar[a]) == a \[CircleTimes] b], 
      "Proof" -> <|"Construct" -> {"Axiom", 2}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CircleTimes] (b_) \[CirclePlus] (a_) \[CircleTimes] 
            (c_) -> a \[CircleTimes] (b \[CirclePlus] c), "Side" -> 1, 
        "Subpattern" -> {}, "MatchingConstruct" -> {"Axiom", 3}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CirclePlus] (b_) \[CircleTimes] OverBar[b_] -> a, 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"CriticalPairLemma", 2} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] a == a], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 1}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] 
            OverBar[a_]) -> a \[CircleTimes] b, "Side" -> 1, 
        "Subpattern" -> {}, "MatchingConstruct" -> {"Axiom", 4}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] ((b_) \[CirclePlus] OverBar[b_]) -> a, 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"Conclusion", 1} -> <|"Statement" -> HoldForm[a == a], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Construct" -> 
         {"CriticalPairLemma", 2}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (a_) -> a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a == a], "Source" -> "cpl"|>|>}|>]
