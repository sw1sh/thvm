ProofObject["EquationalLogic", Inactive[Equal][OverTilde[1] \[CircleTimes] a, 
  a], {Inactive[Equal][(a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)), 
   ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_)], 
  Inactive[Equal][(a_) \[CircleTimes] OverTilde[1], a_], 
  Inactive[Equal][(a_) \[CircleTimes] OverBar[a_], OverTilde[1]]}, 
 <|"Variables" -> {a, b, c}, "Constants" -> {}, 
  "Proof" -> {{"Axiom", 1} -> <|"Statement" -> 
       HoldForm[a \[CircleTimes] (b \[CircleTimes] c) == 
         (a \[CircleTimes] b) \[CircleTimes] c], "Proof" -> <||>|>, 
    {"Axiom", 2} -> <|"Statement" -> HoldForm[
        a \[CircleTimes] OverTilde[1] == a], "Proof" -> <||>|>, 
    {"Axiom", 3} -> <|"Statement" -> HoldForm[a \[CircleTimes] OverBar[a] == 
         OverTilde[1]], "Proof" -> <||>|>, {"Hypothesis", 1} -> 
     <|"Statement" -> HoldForm[OverTilde[1] \[CircleTimes] a == a], 
      "Proof" -> <||>|>, {"CriticalPairLemma", 1} -> 
     <|"Statement" -> HoldForm[b \[CircleTimes] (OverBar[b] \[CircleTimes] 
           a) == OverTilde[1] \[CircleTimes] a], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          a \[CircleTimes] (b \[CircleTimes] c), "Side" -> 1, 
        "Subpattern" -> (a_) \[CircleTimes] (b_), "MatchingConstruct" -> 
         {"Axiom", 3}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] OverBar[a_] -> OverTilde[1], 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 2} -> 
     <|"Statement" -> HoldForm[OverTilde[1] \[CircleTimes] 
          OverBar[OverBar[a]] == a \[CircleTimes] OverTilde[1]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 1}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CircleTimes] 
           (OverBar[a_] \[CircleTimes] (b_)) -> OverTilde[1] \[CircleTimes] 
           b, "Side" -> 1, "Subpattern" -> OverBar[a_] \[CircleTimes] (b_), 
        "MatchingConstruct" -> {"Axiom", 3}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CircleTimes] OverBar[a_] -> OverTilde[1], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[OverTilde[1] \[CircleTimes] 
          OverBar[OverBar[a]] == a], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 2}, "Construct" -> {"Axiom", 2}, 
        "Position" -> {}, "Rule" -> (a_) \[CircleTimes] OverTilde[1] -> a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverTilde[1] \[CircleTimes] OverBar[OverBar[a]] == a], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 3} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (OverTilde[1] \[CircleTimes] 
           b) == a \[CircleTimes] b], "Proof" -> 
       <|"Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          a \[CircleTimes] (b \[CircleTimes] c), "Side" -> 1, 
        "Subpattern" -> (a_) \[CircleTimes] (b_), "MatchingConstruct" -> 
         {"Axiom", 2}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] OverTilde[1] -> a, "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 4} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] OverBar[OverBar[b]] == 
         a \[CircleTimes] b], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 3}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CircleTimes] (OverTilde[1] \[CircleTimes] (b_)) -> 
          a \[CircleTimes] b, "Side" -> 1, "Subpattern" -> 
         OverTilde[1] \[CircleTimes] (b_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 1}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> OverTilde[1] \[CircleTimes] OverBar[OverBar[a_]] -> 
          a, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[OverTilde[1] \[CircleTimes] a == a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 1}, 
        "Construct" -> {"CriticalPairLemma", 4}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] OverBar[OverBar[b_]] -> 
          a \[CircleTimes] b, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[OverTilde[1] \[CircleTimes] a == a], 
        "Source" -> "norm"|>|>, {"Conclusion", 1} -> 
     <|"Statement" -> HoldForm[a == a], "Proof" -> 
       <|"Input" -> {"Hypothesis", 1}, "Construct" -> {"SubstitutionLemma", 
          2}, "Position" -> {}, "Rule" -> OverTilde[1] \[CircleTimes] (a_) -> 
          a, "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 
         1, "Side" -> 1, "OutputExpression" -> HoldForm[a == a], 
        "Source" -> "cpl"|>|>}|>]
