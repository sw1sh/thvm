ProofObject["EquationalLogic", Inactive[Equal][OverBar[OverBar[a]], a], 
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
     <|"Statement" -> HoldForm[OverBar[OverBar[a]] == a], "Proof" -> <||>|>, 
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
     <|"Statement" -> HoldForm[b == (a \[CirclePlus] 
           OverBar[a]) \[CircleTimes] b], "Proof" -> 
       <|"Construct" -> {"Axiom", 4}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] OverBar[b_]) -> a, 
        "Side" -> 1, "Subpattern" -> {}, "MatchingConstruct" -> {"Axiom", 1}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 3} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] OverBar[a] \[CircleTimes] b == 
         a \[CirclePlus] b], "Proof" -> <|"Construct" -> {"Axiom", 6}, 
        "Orientation" -> -1, "Rule" -> 
         ((a_) \[CirclePlus] (b_)) \[CircleTimes] ((a_) \[CirclePlus] 
            (c_)) -> a \[CirclePlus] b \[CircleTimes] c, "Side" -> 1, 
        "Subpattern" -> {}, "MatchingConstruct" -> {"CriticalPairLemma", 2}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CirclePlus] OverBar[a_]) \[CircleTimes] (b_) -> b, 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"CriticalPairLemma", 4} -> 
     <|"Statement" -> HoldForm[b == a \[CircleTimes] OverBar[a] \[CirclePlus] 
          b], "Proof" -> <|"Construct" -> {"Axiom", 3}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CirclePlus] (b_) \[CircleTimes] OverBar[b_] -> a, 
        "Side" -> 1, "Subpattern" -> {}, "MatchingConstruct" -> {"Axiom", 5}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 5} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (OverBar[a] \[CirclePlus] 
           b) == a \[CircleTimes] b], "Proof" -> 
       <|"Construct" -> {"Axiom", 2}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CircleTimes] (b_) \[CirclePlus] (a_) \[CircleTimes] 
            (c_) -> a \[CircleTimes] (b \[CirclePlus] c), "Side" -> 1, 
        "Subpattern" -> {}, "MatchingConstruct" -> {"CriticalPairLemma", 4}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CircleTimes] OverBar[a_] \[CirclePlus] (b_) -> b, 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"CriticalPairLemma", 6} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] OverBar[OverBar[a]] == a], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 5}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CircleTimes] 
           (OverBar[a_] \[CirclePlus] (b_)) -> a \[CircleTimes] b, 
        "Side" -> 1, "Subpattern" -> {}, "MatchingConstruct" -> {"Axiom", 4}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] ((b_) \[CirclePlus] OverBar[b_]) -> a, 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"CriticalPairLemma", 7} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] OverBar[
           OverBar[OverBar[a]]] == a \[CirclePlus] OverBar[a]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 3}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CirclePlus] 
           OverBar[a_] \[CircleTimes] (b_) -> a \[CirclePlus] b, "Side" -> 1, 
        "Subpattern" -> OverBar[a_] \[CircleTimes] (b_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 6}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] OverBar[OverBar[a_]] -> a, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 8} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[a]] \[CircleTimes] a == 
         OverBar[OverBar[a]] \[CircleTimes] (a \[CirclePlus] OverBar[a])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 1}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] 
            OverBar[a_]) -> a \[CircleTimes] b, "Side" -> 1, 
        "Subpattern" -> (b_) \[CirclePlus] OverBar[a_], 
        "MatchingConstruct" -> {"CriticalPairLemma", 7}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CirclePlus] OverBar[OverBar[OverBar[a_]]] -> 
          a \[CirclePlus] OverBar[a], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[a]] \[CircleTimes] a == 
         OverBar[OverBar[a]]], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 8}, "Construct" -> {"Axiom", 4}, 
        "Position" -> {}, "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] 
            OverBar[b_]) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[OverBar[OverBar[a]] \[CircleTimes] a == 
           OverBar[OverBar[a]]], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] OverBar[OverBar[a]] == 
         OverBar[OverBar[a]]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 1}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {}, "Rule" -> (a_) \[CircleTimes] (b_) -> 
          b \[CircleTimes] a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[a \[CircleTimes] OverBar[OverBar[a]] == 
           OverBar[OverBar[a]]], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 3} -> 
     <|"Statement" -> HoldForm[a == OverBar[OverBar[a]]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 2}, 
        "Construct" -> {"CriticalPairLemma", 6}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] OverBar[OverBar[a_]] -> a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a == OverBar[OverBar[a]]], "Source" -> "norm"|>|>, 
    {"Conclusion", 1} -> <|"Statement" -> HoldForm[a == a], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Construct" -> 
         {"SubstitutionLemma", 3}, "Position" -> {}, 
        "Rule" -> OverBar[OverBar[a_]] -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a == a], "Source" -> "cpl"|>|>}|>]
