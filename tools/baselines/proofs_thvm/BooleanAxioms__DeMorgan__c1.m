ProofObject["EquationalLogic", Inactive[Equal][OverBar[a \[CirclePlus] b], 
  OverBar[a] \[CircleTimes] OverBar[b]], 
 {Inactive[Equal][(a_) \[CircleTimes] (b_), (b_) \[CircleTimes] (a_)], 
  Inactive[Equal][(a_) \[CircleTimes] ((b_) \[CirclePlus] (c_)), 
   (a_) \[CircleTimes] (b_) \[CirclePlus] (a_) \[CircleTimes] (c_)], 
  Inactive[Equal][(a_) \[CirclePlus] (b_) \[CircleTimes] OverBar[b_], a_], 
  Inactive[Equal][(a_) \[CircleTimes] ((b_) \[CirclePlus] OverBar[b_]), a_], 
  Inactive[Equal][(a_) \[CirclePlus] (b_), (b_) \[CirclePlus] (a_)], 
  Inactive[Equal][(a_) \[CirclePlus] (b_) \[CircleTimes] (c_), 
   ((a_) \[CirclePlus] (b_)) \[CircleTimes] ((a_) \[CirclePlus] (c_))]}, 
 <|"Variables" -> {a, b, c, x3}, "Constants" -> {}, 
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
     <|"Statement" -> HoldForm[OverBar[a \[CirclePlus] b] == 
         OverBar[a] \[CircleTimes] OverBar[b]], "Proof" -> <||>|>, 
    {"CriticalPairLemma", 1} -> 
     <|"Statement" -> HoldForm[b == a \[CircleTimes] OverBar[a] \[CirclePlus] 
          b], "Proof" -> <|"Construct" -> {"Axiom", 3}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CirclePlus] (b_) \[CircleTimes] OverBar[b_] -> a, 
        "Side" -> 1, "Subpattern" -> {}, "MatchingConstruct" -> {"Axiom", 5}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 2} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (OverBar[a] \[CirclePlus] 
           b) == a \[CircleTimes] b], "Proof" -> 
       <|"Construct" -> {"Axiom", 2}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CircleTimes] (b_) \[CirclePlus] (a_) \[CircleTimes] 
            (c_) -> a \[CircleTimes] (b \[CirclePlus] c), "Side" -> 1, 
        "Subpattern" -> {}, "MatchingConstruct" -> {"CriticalPairLemma", 1}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CircleTimes] OverBar[a_] \[CirclePlus] (b_) -> b, 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"CriticalPairLemma", 3} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] b \[CircleTimes] OverBar[a] == 
         a \[CirclePlus] b], "Proof" -> <|"Construct" -> {"Axiom", 6}, 
        "Orientation" -> -1, "Rule" -> 
         ((a_) \[CirclePlus] (b_)) \[CircleTimes] ((a_) \[CirclePlus] 
            (c_)) -> a \[CirclePlus] b \[CircleTimes] c, "Side" -> 1, 
        "Subpattern" -> {}, "MatchingConstruct" -> {"Axiom", 4}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] ((b_) \[CirclePlus] OverBar[b_]) -> a, 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"CriticalPairLemma", 4} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CirclePlus] 
           OverBar[a]) == a \[CircleTimes] b], 
      "Proof" -> <|"Construct" -> {"Axiom", 2}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CircleTimes] (b_) \[CirclePlus] (a_) \[CircleTimes] 
            (c_) -> a \[CircleTimes] (b \[CirclePlus] c), "Side" -> 1, 
        "Subpattern" -> {}, "MatchingConstruct" -> {"Axiom", 3}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CirclePlus] (b_) \[CircleTimes] OverBar[b_] -> a, 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"CriticalPairLemma", 5} -> 
     <|"Statement" -> HoldForm[b == (a \[CirclePlus] 
           OverBar[a]) \[CircleTimes] b], "Proof" -> 
       <|"Construct" -> {"Axiom", 4}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] OverBar[b_]) -> a, 
        "Side" -> 1, "Subpattern" -> {}, "MatchingConstruct" -> {"Axiom", 1}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 6} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] OverBar[a] \[CircleTimes] b == 
         a \[CirclePlus] b], "Proof" -> <|"Construct" -> {"Axiom", 6}, 
        "Orientation" -> -1, "Rule" -> 
         ((a_) \[CirclePlus] (b_)) \[CircleTimes] ((a_) \[CirclePlus] 
            (c_)) -> a \[CirclePlus] b \[CircleTimes] c, "Side" -> 1, 
        "Subpattern" -> {}, "MatchingConstruct" -> {"CriticalPairLemma", 5}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CirclePlus] OverBar[a_]) \[CircleTimes] (b_) -> b, 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"CriticalPairLemma", 7} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] OverBar[OverBar[a]] == a], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 2}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CircleTimes] 
           (OverBar[a_] \[CirclePlus] (b_)) -> a \[CircleTimes] b, 
        "Side" -> 1, "Subpattern" -> {}, "MatchingConstruct" -> {"Axiom", 4}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] ((b_) \[CirclePlus] OverBar[b_]) -> a, 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"CriticalPairLemma", 8} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] OverBar[
           OverBar[OverBar[a]]] == a \[CirclePlus] OverBar[a]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 6}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CirclePlus] 
           OverBar[a_] \[CircleTimes] (b_) -> a \[CirclePlus] b, "Side" -> 1, 
        "Subpattern" -> OverBar[a_] \[CircleTimes] (b_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 7}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] OverBar[OverBar[a_]] -> a, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 9} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[a]] \[CircleTimes] a == 
         OverBar[OverBar[a]] \[CircleTimes] (a \[CirclePlus] OverBar[a])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 4}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] 
            OverBar[a_]) -> a \[CircleTimes] b, "Side" -> 1, 
        "Subpattern" -> (b_) \[CirclePlus] OverBar[a_], 
        "MatchingConstruct" -> {"CriticalPairLemma", 8}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CirclePlus] OverBar[OverBar[OverBar[a_]]] -> 
          a \[CirclePlus] OverBar[a], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[a]] \[CircleTimes] a == 
         OverBar[OverBar[a]]], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 9}, "Construct" -> {"Axiom", 4}, 
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
        "Construct" -> {"CriticalPairLemma", 7}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] OverBar[OverBar[a_]] -> a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a == OverBar[OverBar[a]]], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 10} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] b == 
         OverBar[a] \[CirclePlus] b \[CircleTimes] a], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 3}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CirclePlus] (b_) \[CircleTimes] 
            OverBar[a_] -> a \[CirclePlus] b, "Side" -> 1, 
        "Subpattern" -> OverBar[a_], "MatchingConstruct" -> 
         {"SubstitutionLemma", 3}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> OverBar[OverBar[a_]] -> a, "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 11} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CircleTimes] a) == 
         a \[CircleTimes] (OverBar[a] \[CirclePlus] b)], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 2}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CircleTimes] 
           (OverBar[a_] \[CirclePlus] (b_)) -> a \[CircleTimes] b, 
        "Side" -> 1, "Subpattern" -> OverBar[a_] \[CirclePlus] (b_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 10}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         OverBar[a_] \[CirclePlus] (b_) \[CircleTimes] (a_) -> 
          OverBar[a] \[CirclePlus] b, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 4} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CircleTimes] a) == 
         a \[CircleTimes] b], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 11}, "Construct" -> 
         {"CriticalPairLemma", 2}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (OverBar[a_] \[CirclePlus] (b_)) -> 
          a \[CircleTimes] b, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[a \[CircleTimes] (b \[CircleTimes] a) == 
           a \[CircleTimes] b], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 12} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] 
          (b \[CircleTimes] a \[CirclePlus] c) == 
         a \[CircleTimes] b \[CirclePlus] a \[CircleTimes] c], 
      "Proof" -> <|"Construct" -> {"Axiom", 2}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CircleTimes] (b_) \[CirclePlus] (a_) \[CircleTimes] 
            (c_) -> a \[CircleTimes] (b \[CirclePlus] c), "Side" -> 1, 
        "Subpattern" -> (a_) \[CircleTimes] (b_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 4}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (a_)) -> 
          a \[CircleTimes] b, "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"SubstitutionLemma", 5} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] 
          (b \[CircleTimes] a \[CirclePlus] c) == a \[CircleTimes] 
          (b \[CirclePlus] c)], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 12}, "Construct" -> {"Axiom", 2}, 
        "Position" -> {}, "Rule" -> (a_) \[CircleTimes] (b_) \[CirclePlus] 
           (a_) \[CircleTimes] (c_) -> a \[CircleTimes] (b \[CirclePlus] c), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CircleTimes] (b \[CircleTimes] a \[CirclePlus] c) == 
           a \[CircleTimes] (b \[CirclePlus] c)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 13} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (c \[CirclePlus] b) == 
         a \[CircleTimes] (b \[CirclePlus] c \[CircleTimes] a)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 5}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CircleTimes] 
           ((b_) \[CircleTimes] (a_) \[CirclePlus] (c_)) -> 
          a \[CircleTimes] (b \[CirclePlus] c), "Side" -> 1, 
        "Subpattern" -> (b_) \[CircleTimes] (a_) \[CirclePlus] (c_), 
        "MatchingConstruct" -> {"Axiom", 5}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 14} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CirclePlus] 
           b \[CircleTimes] c) == a \[CircleTimes] (b \[CircleTimes] 
           (a \[CirclePlus] c))], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 5}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (a_) \[CirclePlus] 
            (c_)) -> a \[CircleTimes] (b \[CirclePlus] c), "Side" -> 1, 
        "Subpattern" -> (b_) \[CircleTimes] (a_) \[CirclePlus] (c_), 
        "MatchingConstruct" -> {"Axiom", 2}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CircleTimes] (b_) \[CirclePlus] 
           (a_) \[CircleTimes] (c_) -> a \[CircleTimes] (b \[CirclePlus] c), 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 15} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] a == a], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 4}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] 
            OverBar[a_]) -> a \[CircleTimes] b, "Side" -> 1, 
        "Subpattern" -> {}, "MatchingConstruct" -> {"Axiom", 4}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] ((b_) \[CirclePlus] OverBar[b_]) -> a, 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"CriticalPairLemma", 16} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (a \[CirclePlus] b) == 
         a \[CirclePlus] a \[CircleTimes] b], 
      "Proof" -> <|"Construct" -> {"Axiom", 2}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CircleTimes] (b_) \[CirclePlus] (a_) \[CircleTimes] 
            (c_) -> a \[CircleTimes] (b \[CirclePlus] c), "Side" -> 1, 
        "Subpattern" -> (a_) \[CircleTimes] (b_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 15}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CircleTimes] (a_) -> a, "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 17} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] (b \[CirclePlus] 
           OverBar[OverBar[a]]) == a \[CirclePlus] OverBar[a] \[CircleTimes] 
           b], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 6}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CirclePlus] 
           OverBar[a_] \[CircleTimes] (b_) -> a \[CirclePlus] b, "Side" -> 1, 
        "Subpattern" -> OverBar[a_] \[CircleTimes] (b_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 4}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] ((b_) \[CirclePlus] OverBar[a_]) -> 
          a \[CircleTimes] b, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 6} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] (b \[CirclePlus] 
           OverBar[OverBar[a]]) == a \[CirclePlus] b], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 17}, 
        "Construct" -> {"CriticalPairLemma", 6}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] OverBar[a_] \[CircleTimes] (b_) -> 
          a \[CirclePlus] b, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[a \[CirclePlus] (b \[CirclePlus] OverBar[OverBar[a]]) == 
           a \[CirclePlus] b], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 7} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] (b \[CirclePlus] a) == 
         a \[CirclePlus] b], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 6}, "Construct" -> 
         {"SubstitutionLemma", 3}, "Position" -> {2, 2}, 
        "Rule" -> OverBar[OverBar[a_]] -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CirclePlus] (b \[CirclePlus] a) == 
           a \[CirclePlus] b], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 18} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] b \[CircleTimes] 
           (c \[CirclePlus] a) == (a \[CirclePlus] b) \[CircleTimes] 
          (a \[CirclePlus] c)], "Proof" -> <|"Construct" -> {"Axiom", 6}, 
        "Orientation" -> -1, "Rule" -> 
         ((a_) \[CirclePlus] (b_)) \[CircleTimes] ((a_) \[CirclePlus] 
            (c_)) -> a \[CirclePlus] b \[CircleTimes] c, "Side" -> 1, 
        "Subpattern" -> (a_) \[CirclePlus] (c_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 7}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CirclePlus] ((b_) \[CirclePlus] (a_)) -> 
          a \[CirclePlus] b, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 8} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] b \[CircleTimes] 
           (c \[CirclePlus] a) == a \[CirclePlus] b \[CircleTimes] c], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 18}, 
        "Construct" -> {"Axiom", 6}, "Position" -> {}, 
        "Rule" -> ((a_) \[CirclePlus] (b_)) \[CircleTimes] 
           ((a_) \[CirclePlus] (c_)) -> a \[CirclePlus] b \[CircleTimes] c, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CirclePlus] b \[CircleTimes] (c \[CirclePlus] a) == 
           a \[CirclePlus] b \[CircleTimes] c], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 19} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] b \[CircleTimes] OverBar[b] == 
         a \[CirclePlus] b \[CircleTimes] a], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 8}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CirclePlus] (b_) \[CircleTimes] 
            ((c_) \[CirclePlus] (a_)) -> a \[CirclePlus] b \[CircleTimes] c, 
        "Side" -> 1, "Subpattern" -> (b_) \[CircleTimes] ((c_) \[CirclePlus] 
           (a_)), "MatchingConstruct" -> {"CriticalPairLemma", 2}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] (OverBar[a_] \[CirclePlus] (b_)) -> 
          a \[CircleTimes] b, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 20} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (a \[CirclePlus] b) == 
         a \[CirclePlus] b \[CircleTimes] a], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 16}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CirclePlus] (a_) \[CircleTimes] 
            (b_) -> a \[CircleTimes] (a \[CirclePlus] b), "Side" -> 1, 
        "Subpattern" -> (a_) \[CircleTimes] (b_), "MatchingConstruct" -> 
         {"Axiom", 1}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 9} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] b \[CircleTimes] OverBar[b] == 
         a \[CircleTimes] (a \[CirclePlus] b)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 19}, 
        "Construct" -> {"CriticalPairLemma", 20}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] (b_) \[CircleTimes] (a_) -> 
          a \[CircleTimes] (a \[CirclePlus] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CirclePlus] b \[CircleTimes] 
             OverBar[b] == a \[CircleTimes] (a \[CirclePlus] b)], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 10} -> 
     <|"Statement" -> HoldForm[a == a \[CircleTimes] (a \[CirclePlus] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 9}, 
        "Construct" -> {"Axiom", 3}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] (b_) \[CircleTimes] OverBar[b_] -> a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a == a \[CircleTimes] (a \[CirclePlus] b)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 11} -> 
     <|"Statement" -> HoldForm[a == a \[CirclePlus] a \[CircleTimes] b], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 16}, 
        "Construct" -> {"SubstitutionLemma", 10}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((a_) \[CirclePlus] (b_)) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a == a \[CirclePlus] a \[CircleTimes] b], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 12} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] b == a \[CircleTimes] 
          (b \[CircleTimes] (a \[CirclePlus] c))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 14}, 
        "Construct" -> {"SubstitutionLemma", 11}, "Position" -> {2}, 
        "Rule" -> (a_) \[CirclePlus] (a_) \[CircleTimes] (b_) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[a \[CircleTimes] b == 
           a \[CircleTimes] (b \[CircleTimes] (a \[CirclePlus] c))], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 21} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] b == 
         OverBar[a] \[CircleTimes] (a \[CirclePlus] b)], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 2}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CircleTimes] 
           (OverBar[a_] \[CirclePlus] (b_)) -> a \[CircleTimes] b, 
        "Side" -> 1, "Subpattern" -> OverBar[a_], "MatchingConstruct" -> 
         {"SubstitutionLemma", 3}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> OverBar[OverBar[a_]] -> a, "MatchingSide" -> 1, 
        "Position" -> {2, 1}|>|>, {"CriticalPairLemma", 22} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] OverBar[a] == 
         a \[CircleTimes] (OverBar[a] \[CircleTimes] b)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 12}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CircleTimes] 
           ((b_) \[CircleTimes] ((a_) \[CirclePlus] (c_))) -> 
          a \[CircleTimes] b, "Side" -> 1, "Subpattern" -> 
         (b_) \[CircleTimes] ((a_) \[CirclePlus] (c_)), 
        "MatchingConstruct" -> {"CriticalPairLemma", 21}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         OverBar[a_] \[CircleTimes] ((a_) \[CirclePlus] (b_)) -> 
          OverBar[a] \[CircleTimes] b, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 23} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] OverBar[a] == 
         a \[CircleTimes] (b \[CircleTimes] OverBar[a])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 22}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CircleTimes] 
           (OverBar[a_] \[CircleTimes] (b_)) -> a \[CircleTimes] OverBar[a], 
        "Side" -> 1, "Subpattern" -> OverBar[a_] \[CircleTimes] (b_), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 24} -> 
     <|"Statement" -> HoldForm[(a \[CircleTimes] OverBar[b]) \[CircleTimes] 
          (b \[CirclePlus] c) == (a \[CircleTimes] OverBar[b]) \[CircleTimes] 
          (c \[CirclePlus] b \[CircleTimes] OverBar[b])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 13}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CircleTimes] 
           ((b_) \[CirclePlus] (c_) \[CircleTimes] (a_)) -> 
          a \[CircleTimes] (c \[CirclePlus] b), "Side" -> 1, 
        "Subpattern" -> (c_) \[CircleTimes] (a_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 23}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] 
            OverBar[a_]) -> a \[CircleTimes] OverBar[a], "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 25} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CirclePlus] a) == 
         a \[CircleTimes] b \[CirclePlus] a], 
      "Proof" -> <|"Construct" -> {"Axiom", 2}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CircleTimes] (b_) \[CirclePlus] (a_) \[CircleTimes] 
            (c_) -> a \[CircleTimes] (b \[CirclePlus] c), "Side" -> 1, 
        "Subpattern" -> (a_) \[CircleTimes] (c_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 15}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CircleTimes] (a_) -> a, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 13} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CirclePlus] a) == 
         a \[CirclePlus] a \[CircleTimes] b], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 25}, 
        "Construct" -> {"Axiom", 5}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CircleTimes] (b \[CirclePlus] a) == a \[CirclePlus] 
            a \[CircleTimes] b], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 14} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CirclePlus] a) == 
         a \[CircleTimes] (a \[CirclePlus] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 13}, 
        "Construct" -> {"CriticalPairLemma", 16}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] (a_) \[CircleTimes] (b_) -> 
          a \[CircleTimes] (a \[CirclePlus] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CircleTimes] (b \[CirclePlus] 
             a) == a \[CircleTimes] (a \[CirclePlus] b)], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 15} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CirclePlus] a) == a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 14}, 
        "Construct" -> {"SubstitutionLemma", 10}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((a_) \[CirclePlus] (b_)) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CircleTimes] (b \[CirclePlus] a) == a], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 26} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CirclePlus] 
           (c \[CirclePlus] a)) == a \[CircleTimes] b \[CirclePlus] a], 
      "Proof" -> <|"Construct" -> {"Axiom", 2}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CircleTimes] (b_) \[CirclePlus] (a_) \[CircleTimes] 
            (c_) -> a \[CircleTimes] (b \[CirclePlus] c), "Side" -> 1, 
        "Subpattern" -> (a_) \[CircleTimes] (c_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 15}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] (a_)) -> a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 16} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CirclePlus] 
           (c \[CirclePlus] a)) == a \[CirclePlus] a \[CircleTimes] b], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 26}, 
        "Construct" -> {"Axiom", 5}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CircleTimes] (b \[CirclePlus] (c \[CirclePlus] a)) == 
           a \[CirclePlus] a \[CircleTimes] b], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 17} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CirclePlus] 
           (c \[CirclePlus] a)) == a], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 16}, "Construct" -> 
         {"SubstitutionLemma", 11}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] (a_) \[CircleTimes] (b_) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CircleTimes] (b \[CirclePlus] (c \[CirclePlus] a)) == a], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 27} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] b == 
         (a \[CircleTimes] b) \[CircleTimes] (c \[CirclePlus] a)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 17}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] 
            ((c_) \[CirclePlus] (a_))) -> a, "Side" -> 1, 
        "Subpattern" -> (c_) \[CirclePlus] (a_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 11}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CirclePlus] (a_) \[CircleTimes] (b_) -> a, 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"CriticalPairLemma", 28} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] 
          (a \[CircleTimes] b) \[CircleTimes] c == a \[CirclePlus] 
          a \[CircleTimes] b], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 8}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CirclePlus] (b_) \[CircleTimes] ((c_) \[CirclePlus] 
             (a_)) -> a \[CirclePlus] b \[CircleTimes] c, "Side" -> 1, 
        "Subpattern" -> (b_) \[CircleTimes] ((c_) \[CirclePlus] (a_)), 
        "MatchingConstruct" -> {"CriticalPairLemma", 27}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CircleTimes] (b_)) \[CircleTimes] ((c_) \[CirclePlus] 
            (a_)) -> a \[CircleTimes] b, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 18} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] 
          (a \[CircleTimes] b) \[CircleTimes] c == a], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 28}, 
        "Construct" -> {"SubstitutionLemma", 11}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] (a_) \[CircleTimes] (b_) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CirclePlus] (a \[CircleTimes] b) \[CircleTimes] c == a], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 29} -> 
     <|"Statement" -> HoldForm[a == a \[CirclePlus] 
          (b \[CircleTimes] a) \[CircleTimes] c], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 18}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CirclePlus] 
           ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> a, "Side" -> 1, 
        "Subpattern" -> (a_) \[CircleTimes] (b_), "MatchingConstruct" -> 
         {"Axiom", 1}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, "MatchingSide" -> 1, 
        "Position" -> {2, 1}|>|>, {"CriticalPairLemma", 30} -> 
     <|"Statement" -> HoldForm[(a \[CircleTimes] b) \[CircleTimes] c == 
         ((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] b], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 15}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] 
            (a_)) -> a, "Side" -> 1, "Subpattern" -> (b_) \[CirclePlus] (a_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 29}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CirclePlus] ((b_) \[CircleTimes] (a_)) \[CircleTimes] (c_) -> 
          a, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 19} -> 
     <|"Statement" -> HoldForm[(a \[CircleTimes] b) \[CircleTimes] c == 
         b \[CircleTimes] ((a \[CircleTimes] b) \[CircleTimes] c)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 30}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (a \[CircleTimes] b) \[CircleTimes] c == b \[CircleTimes] 
            ((a \[CircleTimes] b) \[CircleTimes] c)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 31} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] 
          (b \[CircleTimes] OverBar[a]) \[CircleTimes] c == 
         (a \[CirclePlus] b) \[CircleTimes] (a \[CirclePlus] c)], 
      "Proof" -> <|"Construct" -> {"Axiom", 6}, "Orientation" -> -1, 
        "Rule" -> ((a_) \[CirclePlus] (b_)) \[CircleTimes] 
           ((a_) \[CirclePlus] (c_)) -> a \[CirclePlus] b \[CircleTimes] c, 
        "Side" -> 1, "Subpattern" -> (a_) \[CirclePlus] (b_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 3}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CirclePlus] (b_) \[CircleTimes] OverBar[a_] -> 
          a \[CirclePlus] b, "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"SubstitutionLemma", 20} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] 
          (b \[CircleTimes] OverBar[a]) \[CircleTimes] c == 
         a \[CirclePlus] b \[CircleTimes] c], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 31}, 
        "Construct" -> {"Axiom", 6}, "Position" -> {}, 
        "Rule" -> ((a_) \[CirclePlus] (b_)) \[CircleTimes] 
           ((a_) \[CirclePlus] (c_)) -> a \[CirclePlus] b \[CircleTimes] c, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CirclePlus] (b \[CircleTimes] OverBar[a]) \[CircleTimes] c == 
           a \[CirclePlus] b \[CircleTimes] c], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 32} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] 
          ((b \[CircleTimes] OverBar[OverBar[a]]) \[CircleTimes] c) == 
         a \[CircleTimes] (OverBar[a] \[CirclePlus] b \[CircleTimes] c)], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 2}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CircleTimes] 
           (OverBar[a_] \[CirclePlus] (b_)) -> a \[CircleTimes] b, 
        "Side" -> 1, "Subpattern" -> OverBar[a_] \[CirclePlus] (b_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 20}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CirclePlus] ((b_) \[CircleTimes] OverBar[a_]) \[CircleTimes] 
            (c_) -> a \[CirclePlus] b \[CircleTimes] c, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 21} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] 
          ((b \[CircleTimes] OverBar[OverBar[a]]) \[CircleTimes] c) == 
         a \[CircleTimes] (b \[CircleTimes] c)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 32}, 
        "Construct" -> {"CriticalPairLemma", 2}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (OverBar[a_] \[CirclePlus] (b_)) -> 
          a \[CircleTimes] b, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[a \[CircleTimes] ((b \[CircleTimes] OverBar[OverBar[
                a]]) \[CircleTimes] c) == a \[CircleTimes] (b \[CircleTimes] 
             c)], "Source" -> "norm"|>|>, {"SubstitutionLemma", 22} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] 
          ((b \[CircleTimes] a) \[CircleTimes] c) == a \[CircleTimes] 
          (b \[CircleTimes] c)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 21}, "Construct" -> 
         {"SubstitutionLemma", 3}, "Position" -> {2, 1, 2}, 
        "Rule" -> OverBar[OverBar[a_]] -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CircleTimes] 
            ((b \[CircleTimes] a) \[CircleTimes] c) == a \[CircleTimes] 
            (b \[CircleTimes] c)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 23} -> 
     <|"Statement" -> HoldForm[(a \[CircleTimes] b) \[CircleTimes] c == 
         b \[CircleTimes] (a \[CircleTimes] c)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 19}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (((b_) \[CircleTimes] 
             (a_)) \[CircleTimes] (c_)) -> a \[CircleTimes] 
           (b \[CircleTimes] c), "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[(a \[CircleTimes] b) \[CircleTimes] c == 
           b \[CircleTimes] (a \[CircleTimes] c)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 24} -> 
     <|"Statement" -> HoldForm[(a \[CircleTimes] OverBar[b]) \[CircleTimes] 
          (b \[CirclePlus] c) == OverBar[b] \[CircleTimes] 
          (a \[CircleTimes] (c \[CirclePlus] b \[CircleTimes] OverBar[b]))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 24}, 
        "Construct" -> {"SubstitutionLemma", 23}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[
          (a \[CircleTimes] OverBar[b]) \[CircleTimes] (b \[CirclePlus] c) == 
           OverBar[b] \[CircleTimes] (a \[CircleTimes] (c \[CirclePlus] 
              b \[CircleTimes] OverBar[b]))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 25} -> 
     <|"Statement" -> HoldForm[(a \[CircleTimes] OverBar[b]) \[CircleTimes] 
          (b \[CirclePlus] c) == OverBar[b] \[CircleTimes] 
          (a \[CircleTimes] c)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 24}, "Construct" -> {"Axiom", 3}, 
        "Position" -> {2, 2}, "Rule" -> (a_) \[CirclePlus] 
           (b_) \[CircleTimes] OverBar[b_] -> a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[
          (a \[CircleTimes] OverBar[b]) \[CircleTimes] (b \[CirclePlus] c) == 
           OverBar[b] \[CircleTimes] (a \[CircleTimes] c)], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 26} -> 
     <|"Statement" -> HoldForm[OverBar[b] \[CircleTimes] (a \[CircleTimes] 
           (b \[CirclePlus] c)) == OverBar[b] \[CircleTimes] 
          (a \[CircleTimes] c)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 25}, "Construct" -> 
         {"SubstitutionLemma", 23}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[OverBar[b] \[CircleTimes] 
            (a \[CircleTimes] (b \[CirclePlus] c)) == 
           OverBar[b] \[CircleTimes] (a \[CircleTimes] c)], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 33} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] b == a \[CirclePlus] 
          (a \[CirclePlus] b)], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 7}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CirclePlus] ((b_) \[CirclePlus] (a_)) -> 
          a \[CirclePlus] b, "Side" -> 1, "Subpattern" -> 
         (b_) \[CirclePlus] (a_), "MatchingConstruct" -> {"Axiom", 5}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 34} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] b \[CircleTimes] 
           (a \[CirclePlus] c) == (a \[CirclePlus] b) \[CircleTimes] 
          (a \[CirclePlus] c)], "Proof" -> <|"Construct" -> {"Axiom", 6}, 
        "Orientation" -> -1, "Rule" -> 
         ((a_) \[CirclePlus] (b_)) \[CircleTimes] ((a_) \[CirclePlus] 
            (c_)) -> a \[CirclePlus] b \[CircleTimes] c, "Side" -> 1, 
        "Subpattern" -> (a_) \[CirclePlus] (c_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 33}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CirclePlus] ((a_) \[CirclePlus] (b_)) -> 
          a \[CirclePlus] b, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 27} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] b \[CircleTimes] 
           (a \[CirclePlus] c) == a \[CirclePlus] b \[CircleTimes] c], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 34}, 
        "Construct" -> {"Axiom", 6}, "Position" -> {}, 
        "Rule" -> ((a_) \[CirclePlus] (b_)) \[CircleTimes] 
           ((a_) \[CirclePlus] (c_)) -> a \[CirclePlus] b \[CircleTimes] c, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CirclePlus] b \[CircleTimes] (a \[CirclePlus] c) == 
           a \[CirclePlus] b \[CircleTimes] c], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 35} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] b \[CircleTimes] c == 
         (a \[CirclePlus] b) \[CircleTimes] (c \[CirclePlus] a)], 
      "Proof" -> <|"Construct" -> {"Axiom", 6}, "Orientation" -> -1, 
        "Rule" -> ((a_) \[CirclePlus] (b_)) \[CircleTimes] 
           ((a_) \[CirclePlus] (c_)) -> a \[CirclePlus] b \[CircleTimes] c, 
        "Side" -> 1, "Subpattern" -> (a_) \[CirclePlus] (c_), 
        "MatchingConstruct" -> {"Axiom", 5}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 36} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] 
          (b \[CirclePlus] c) \[CircleTimes] b == a \[CirclePlus] 
          (b \[CirclePlus] c \[CircleTimes] a)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 27}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CirclePlus] (b_) \[CircleTimes] 
            ((a_) \[CirclePlus] (c_)) -> a \[CirclePlus] b \[CircleTimes] c, 
        "Side" -> 1, "Subpattern" -> (b_) \[CircleTimes] ((a_) \[CirclePlus] 
           (c_)), "MatchingConstruct" -> {"CriticalPairLemma", 35}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CirclePlus] (b_)) \[CircleTimes] ((c_) \[CirclePlus] 
            (a_)) -> a \[CirclePlus] b \[CircleTimes] c, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 37} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] 
          (b \[CirclePlus] a) \[CircleTimes] c == 
         (a \[CirclePlus] b) \[CircleTimes] (a \[CirclePlus] c)], 
      "Proof" -> <|"Construct" -> {"Axiom", 6}, "Orientation" -> -1, 
        "Rule" -> ((a_) \[CirclePlus] (b_)) \[CircleTimes] 
           ((a_) \[CirclePlus] (c_)) -> a \[CirclePlus] b \[CircleTimes] c, 
        "Side" -> 1, "Subpattern" -> (a_) \[CirclePlus] (b_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 7}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CirclePlus] ((b_) \[CirclePlus] (a_)) -> a \[CirclePlus] b, 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"SubstitutionLemma", 28} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] 
          (b \[CirclePlus] a) \[CircleTimes] c == a \[CirclePlus] 
          b \[CircleTimes] c], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 37}, "Construct" -> {"Axiom", 6}, 
        "Position" -> {}, "Rule" -> ((a_) \[CirclePlus] (b_)) \[CircleTimes] 
           ((a_) \[CirclePlus] (c_)) -> a \[CirclePlus] b \[CircleTimes] c, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CirclePlus] (b \[CirclePlus] a) \[CircleTimes] c == 
           a \[CirclePlus] b \[CircleTimes] c], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 38} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] c \[CircleTimes] b == 
         a \[CirclePlus] b \[CircleTimes] (c \[CirclePlus] a)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 28}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CirclePlus] 
           ((b_) \[CirclePlus] (a_)) \[CircleTimes] (c_) -> 
          a \[CirclePlus] b \[CircleTimes] c, "Side" -> 1, 
        "Subpattern" -> ((b_) \[CirclePlus] (a_)) \[CircleTimes] (c_), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 29} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] c \[CircleTimes] b == 
         a \[CirclePlus] b \[CircleTimes] c], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 38}, 
        "Construct" -> {"SubstitutionLemma", 8}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] (b_) \[CircleTimes] ((c_) \[CirclePlus] 
             (a_)) -> a \[CirclePlus] b \[CircleTimes] c, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CirclePlus] c \[CircleTimes] b == 
           a \[CirclePlus] b \[CircleTimes] c], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 30} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] b \[CircleTimes] 
           (b \[CirclePlus] c) == a \[CirclePlus] (b \[CirclePlus] 
           c \[CircleTimes] a)], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 36}, "Construct" -> 
         {"SubstitutionLemma", 29}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] (b_) \[CircleTimes] (c_) -> 
          a \[CirclePlus] c \[CircleTimes] b, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CirclePlus] b \[CircleTimes] 
             (b \[CirclePlus] c) == a \[CirclePlus] (b \[CirclePlus] 
             c \[CircleTimes] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 31} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] b == a \[CirclePlus] 
          (b \[CirclePlus] c \[CircleTimes] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 30}, 
        "Construct" -> {"SubstitutionLemma", 10}, "Position" -> {2}, 
        "Rule" -> (a_) \[CircleTimes] ((a_) \[CirclePlus] (b_)) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[a \[CirclePlus] b == 
           a \[CirclePlus] (b \[CirclePlus] c \[CircleTimes] a)], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 39} -> 
     <|"Statement" -> HoldForm[(a \[CirclePlus] b) \[CirclePlus] c == 
         (a \[CirclePlus] b) \[CirclePlus] (c \[CirclePlus] a)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 31}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CirclePlus] ((b_) \[CirclePlus] 
            (c_) \[CircleTimes] (a_)) -> a \[CirclePlus] b, "Side" -> 1, 
        "Subpattern" -> (c_) \[CircleTimes] (a_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 10}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CircleTimes] ((a_) \[CirclePlus] (b_)) -> a, 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"SubstitutionLemma", 32} -> 
     <|"Statement" -> HoldForm[a == a \[CirclePlus] b \[CircleTimes] a], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 20}, 
        "Construct" -> {"SubstitutionLemma", 10}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((a_) \[CirclePlus] (b_)) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a == a \[CirclePlus] b \[CircleTimes] a], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 40} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] 
          ((a \[CirclePlus] c) \[CirclePlus] b) == a \[CirclePlus] 
          a \[CircleTimes] b], "Proof" -> <|"Construct" -> {"Axiom", 2}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CircleTimes] (b_) \[CirclePlus] 
           (a_) \[CircleTimes] (c_) -> a \[CircleTimes] (b \[CirclePlus] c), 
        "Side" -> 1, "Subpattern" -> (a_) \[CircleTimes] (b_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 10}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CircleTimes] ((a_) \[CirclePlus] (b_)) -> a, 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"SubstitutionLemma", 33} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] 
          ((a \[CirclePlus] c) \[CirclePlus] b) == a], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 40}, 
        "Construct" -> {"SubstitutionLemma", 11}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] (a_) \[CircleTimes] (b_) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CircleTimes] ((a \[CirclePlus] c) \[CirclePlus] b) == a], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 41} -> 
     <|"Statement" -> HoldForm[(a \[CirclePlus] b) \[CirclePlus] c == 
         ((a \[CirclePlus] b) \[CirclePlus] c) \[CirclePlus] a], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 32}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CirclePlus] (b_) \[CircleTimes] 
            (a_) -> a, "Side" -> 1, "Subpattern" -> (b_) \[CircleTimes] (a_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 33}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] (((a_) \[CirclePlus] (b_)) \[CirclePlus] 
            (c_)) -> a, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 34} -> 
     <|"Statement" -> HoldForm[(a \[CirclePlus] b) \[CirclePlus] c == 
         a \[CirclePlus] ((a \[CirclePlus] b) \[CirclePlus] c)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 41}, 
        "Construct" -> {"Axiom", 5}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (a \[CirclePlus] b) \[CirclePlus] c == a \[CirclePlus] 
            ((a \[CirclePlus] b) \[CirclePlus] c)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 42} -> 
     <|"Statement" -> HoldForm[(a \[CirclePlus] c) \[CirclePlus] b == 
         a \[CirclePlus] (b \[CirclePlus] (a \[CirclePlus] c))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 34}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CirclePlus] 
           (((a_) \[CirclePlus] (b_)) \[CirclePlus] (c_)) -> 
          (a \[CirclePlus] b) \[CirclePlus] c, "Side" -> 1, 
        "Subpattern" -> ((a_) \[CirclePlus] (b_)) \[CirclePlus] (c_), 
        "MatchingConstruct" -> {"Axiom", 5}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 43} -> 
     <|"Statement" -> HoldForm[(a \[CirclePlus] b) \[CirclePlus] c == 
         (a \[CirclePlus] b) \[CirclePlus] (c \[CirclePlus] b)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 31}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CirclePlus] ((b_) \[CirclePlus] 
            (c_) \[CircleTimes] (a_)) -> a \[CirclePlus] b, "Side" -> 1, 
        "Subpattern" -> (c_) \[CircleTimes] (a_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 15}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] (a_)) -> a, 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"CriticalPairLemma", 44} -> 
     <|"Statement" -> HoldForm[(a \[CirclePlus] c) \[CirclePlus] 
          (b \[CirclePlus] c) == a \[CirclePlus] 
          ((b \[CirclePlus] c) \[CirclePlus] a)], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 42}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CirclePlus] ((b_) \[CirclePlus] 
            ((a_) \[CirclePlus] (c_))) -> (a \[CirclePlus] c) \[CirclePlus] 
           b, "Side" -> 1, "Subpattern" -> (b_) \[CirclePlus] 
          ((a_) \[CirclePlus] (c_)), "MatchingConstruct" -> 
         {"CriticalPairLemma", 43}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CirclePlus] (b_)) \[CirclePlus] 
           ((c_) \[CirclePlus] (b_)) -> (a \[CirclePlus] b) \[CirclePlus] c, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 35} -> 
     <|"Statement" -> HoldForm[(a \[CirclePlus] c) \[CirclePlus] 
          (b \[CirclePlus] c) == a \[CirclePlus] (b \[CirclePlus] c)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 44}, 
        "Construct" -> {"SubstitutionLemma", 7}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] ((b_) \[CirclePlus] (a_)) -> 
          a \[CirclePlus] b, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[(a \[CirclePlus] c) \[CirclePlus] (b \[CirclePlus] c) == 
           a \[CirclePlus] (b \[CirclePlus] c)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 36} -> 
     <|"Statement" -> HoldForm[(a \[CirclePlus] c) \[CirclePlus] b == 
         a \[CirclePlus] (b \[CirclePlus] c)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 35}, 
        "Construct" -> {"CriticalPairLemma", 43}, "Position" -> {}, 
        "Rule" -> ((a_) \[CirclePlus] (b_)) \[CirclePlus] ((c_) \[CirclePlus] 
            (b_)) -> (a \[CirclePlus] b) \[CirclePlus] c, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (a \[CirclePlus] c) \[CirclePlus] b == a \[CirclePlus] 
            (b \[CirclePlus] c)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 37} -> 
     <|"Statement" -> HoldForm[(a \[CirclePlus] b) \[CirclePlus] c == 
         a \[CirclePlus] ((c \[CirclePlus] a) \[CirclePlus] b)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 39}, 
        "Construct" -> {"SubstitutionLemma", 36}, "Position" -> {}, 
        "Rule" -> ((a_) \[CirclePlus] (b_)) \[CirclePlus] (c_) -> 
          a \[CirclePlus] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[(a \[CirclePlus] b) \[CirclePlus] c == 
           a \[CirclePlus] ((c \[CirclePlus] a) \[CirclePlus] b)], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 38} -> 
     <|"Statement" -> HoldForm[(a \[CirclePlus] b) \[CirclePlus] c == 
         a \[CirclePlus] (c \[CirclePlus] (b \[CirclePlus] a))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 37}, 
        "Construct" -> {"SubstitutionLemma", 36}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CirclePlus] (b_)) \[CirclePlus] (c_) -> 
          a \[CirclePlus] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[(a \[CirclePlus] b) \[CirclePlus] c == 
           a \[CirclePlus] (c \[CirclePlus] (b \[CirclePlus] a))], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 45} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] (b \[CirclePlus] c) == 
         (a \[CirclePlus] (b \[CirclePlus] c)) \[CirclePlus] c], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 32}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CirclePlus] (b_) \[CircleTimes] 
            (a_) -> a, "Side" -> 1, "Subpattern" -> (b_) \[CircleTimes] (a_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 17}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] ((b_) \[CirclePlus] ((c_) \[CirclePlus] 
             (a_))) -> a, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 39} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] (b \[CirclePlus] c) == 
         c \[CirclePlus] (a \[CirclePlus] (b \[CirclePlus] c))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 45}, 
        "Construct" -> {"Axiom", 5}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CirclePlus] (b \[CirclePlus] c) == c \[CirclePlus] 
            (a \[CirclePlus] (b \[CirclePlus] c))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 40} -> 
     <|"Statement" -> HoldForm[(a \[CirclePlus] c) \[CirclePlus] b == 
         b \[CirclePlus] (c \[CirclePlus] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 38}, 
        "Construct" -> {"SubstitutionLemma", 39}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] ((b_) \[CirclePlus] ((c_) \[CirclePlus] 
             (a_))) -> b \[CirclePlus] (c \[CirclePlus] a), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (a \[CirclePlus] c) \[CirclePlus] b == b \[CirclePlus] 
            (c \[CirclePlus] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 41} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] (b \[CirclePlus] c) == 
         b \[CirclePlus] (c \[CirclePlus] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 40}, 
        "Construct" -> {"SubstitutionLemma", 36}, "Position" -> {}, 
        "Rule" -> ((a_) \[CirclePlus] (b_)) \[CirclePlus] (c_) -> 
          a \[CirclePlus] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CirclePlus] (b \[CirclePlus] c) == 
           b \[CirclePlus] (c \[CirclePlus] a)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 46} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (c \[CirclePlus] b) == 
         a \[CircleTimes] (b \[CirclePlus] (OverBar[a] \[CirclePlus] c))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 2}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CircleTimes] 
           (OverBar[a_] \[CirclePlus] (b_)) -> a \[CircleTimes] b, 
        "Side" -> 1, "Subpattern" -> OverBar[a_] \[CirclePlus] (b_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 41}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CirclePlus] ((b_) \[CirclePlus] (c_)) -> 
          c \[CirclePlus] (a \[CirclePlus] b), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 47} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] b == 
         OverBar[a] \[CircleTimes] (b \[CirclePlus] a)], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 4}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] 
            OverBar[a_]) -> a \[CircleTimes] b, "Side" -> 1, 
        "Subpattern" -> OverBar[a_], "MatchingConstruct" -> 
         {"SubstitutionLemma", 3}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> OverBar[OverBar[a_]] -> a, "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 48} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] b == 
         OverBar[a] \[CirclePlus] a \[CircleTimes] b], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 6}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CirclePlus] 
           OverBar[a_] \[CircleTimes] (b_) -> a \[CirclePlus] b, "Side" -> 1, 
        "Subpattern" -> OverBar[a_], "MatchingConstruct" -> 
         {"SubstitutionLemma", 3}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> OverBar[OverBar[a_]] -> a, "MatchingSide" -> 1, 
        "Position" -> {2, 1}|>|>, {"CriticalPairLemma", 49} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] (b \[CirclePlus] 
           a) == OverBar[a] \[CirclePlus] a], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 48}, 
        "Orientation" -> -1, "Rule" -> OverBar[a_] \[CirclePlus] 
           (a_) \[CircleTimes] (b_) -> OverBar[a] \[CirclePlus] b, 
        "Side" -> 1, "Subpattern" -> (a_) \[CircleTimes] (b_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 15}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] ((b_) \[CirclePlus] (a_)) -> a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 42} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] (b \[CirclePlus] 
           a) == a \[CirclePlus] OverBar[a]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 49}, 
        "Construct" -> {"Axiom", 5}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a] \[CirclePlus] (b \[CirclePlus] a) == 
           a \[CirclePlus] OverBar[a]], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 50} -> 
     <|"Statement" -> HoldForm[OverBar[a \[CirclePlus] b] \[CircleTimes] 
          OverBar[b] == OverBar[a \[CirclePlus] b] \[CircleTimes] 
          (b \[CirclePlus] OverBar[b])], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 47}, "Orientation" -> -1, 
        "Rule" -> OverBar[a_] \[CircleTimes] ((b_) \[CirclePlus] (a_)) -> 
          OverBar[a] \[CircleTimes] b, "Side" -> 1, "Subpattern" -> 
         (b_) \[CirclePlus] (a_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 42}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> OverBar[a_] \[CirclePlus] ((b_) \[CirclePlus] 
            (a_)) -> a \[CirclePlus] OverBar[a], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 43} -> 
     <|"Statement" -> HoldForm[OverBar[a \[CirclePlus] b] \[CircleTimes] 
          OverBar[b] == OverBar[a \[CirclePlus] b]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 50}, 
        "Construct" -> {"Axiom", 4}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] OverBar[b_]) -> a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a \[CirclePlus] b] \[CircleTimes] OverBar[b] == 
           OverBar[a \[CirclePlus] b]], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 44} -> 
     <|"Statement" -> HoldForm[OverBar[b] \[CircleTimes] 
          OverBar[a \[CirclePlus] b] == OverBar[a \[CirclePlus] b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 43}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          OverBar[b] \[CircleTimes] OverBar[a \[CirclePlus] b] == 
           OverBar[a \[CirclePlus] b]], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 51} -> 
     <|"Statement" -> HoldForm[OverBar[a \[CirclePlus] a \[CircleTimes] b] == 
         OverBar[a \[CircleTimes] b] \[CircleTimes] OverBar[a]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 44}, 
        "Orientation" -> 1, "Rule" -> OverBar[a_] \[CircleTimes] 
           OverBar[(b_) \[CirclePlus] (a_)] -> OverBar[b \[CirclePlus] a], 
        "Side" -> 1, "Subpattern" -> (b_) \[CirclePlus] (a_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 11}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CirclePlus] (a_) \[CircleTimes] (b_) -> a, 
        "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"SubstitutionLemma", 45} -> 
     <|"Statement" -> HoldForm[OverBar[a \[CirclePlus] a \[CircleTimes] b] == 
         OverBar[a] \[CircleTimes] OverBar[a \[CircleTimes] b]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 51}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a \[CirclePlus] a \[CircleTimes] b] == 
           OverBar[a] \[CircleTimes] OverBar[a \[CircleTimes] b]], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 46} -> 
     <|"Statement" -> HoldForm[OverBar[a] == OverBar[a] \[CircleTimes] 
          OverBar[a \[CircleTimes] b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 45}, "Construct" -> 
         {"SubstitutionLemma", 11}, "Position" -> {1}, 
        "Rule" -> (a_) \[CirclePlus] (a_) \[CircleTimes] (b_) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[OverBar[a] == 
           OverBar[a] \[CircleTimes] OverBar[a \[CircleTimes] b]], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 52} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] OverBar[a \[CircleTimes] b] == 
         a \[CirclePlus] OverBar[a]], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 6}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CirclePlus] OverBar[a_] \[CircleTimes] (b_) -> 
          a \[CirclePlus] b, "Side" -> 1, "Subpattern" -> 
         OverBar[a_] \[CircleTimes] (b_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 46}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> OverBar[a_] \[CircleTimes] 
           OverBar[(a_) \[CircleTimes] (b_)] -> OverBar[a], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 53} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] OverBar[a] == 
         a \[CirclePlus] OverBar[b \[CircleTimes] a]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 52}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CirclePlus] 
           OverBar[(a_) \[CircleTimes] (b_)] -> a \[CirclePlus] OverBar[a], 
        "Side" -> 1, "Subpattern" -> (a_) \[CircleTimes] (b_), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"CriticalPairLemma", 54} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] OverBar[a] == 
         a \[CirclePlus] OverBar[b \[CircleTimes] (c \[CircleTimes] a)]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 53}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CirclePlus] 
           OverBar[(b_) \[CircleTimes] (a_)] -> a \[CirclePlus] OverBar[a], 
        "Side" -> 1, "Subpattern" -> (b_) \[CircleTimes] (a_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 23}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "MatchingSide" -> 1, 
        "Position" -> {2, 1}|>|>, {"CriticalPairLemma", 55} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] 
          OverBar[OverBar[a]] == a \[CirclePlus] (b \[CirclePlus] 
           OverBar[a])], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 
          42}, "Orientation" -> 1, "Rule" -> OverBar[a_] \[CirclePlus] 
           ((b_) \[CirclePlus] (a_)) -> a \[CirclePlus] OverBar[a], 
        "Side" -> 1, "Subpattern" -> OverBar[a_], "MatchingConstruct" -> 
         {"SubstitutionLemma", 3}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> OverBar[OverBar[a_]] -> a, "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 47} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         a \[CirclePlus] (b \[CirclePlus] OverBar[a])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 55}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Position" -> {2}, 
        "Rule" -> OverBar[OverBar[a_]] -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CirclePlus] a == 
           a \[CirclePlus] (b \[CirclePlus] OverBar[a])], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 48} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] OverBar[a] == 
         a \[CirclePlus] (b \[CirclePlus] OverBar[a])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 47}, 
        "Construct" -> {"Axiom", 5}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a \[CirclePlus] OverBar[a] == a \[CirclePlus] (b \[CirclePlus] 
             OverBar[a])], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 56} -> 
     <|"Statement" -> HoldForm[
        OverBar[a \[CirclePlus] OverBar[b]] \[CircleTimes] b == 
         OverBar[a \[CirclePlus] OverBar[b]] \[CircleTimes] 
          (b \[CirclePlus] OverBar[b])], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 47}, "Orientation" -> -1, 
        "Rule" -> OverBar[a_] \[CircleTimes] ((b_) \[CirclePlus] (a_)) -> 
          OverBar[a] \[CircleTimes] b, "Side" -> 1, "Subpattern" -> 
         (b_) \[CirclePlus] (a_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 48}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CirclePlus] ((b_) \[CirclePlus] 
            OverBar[a_]) -> a \[CirclePlus] OverBar[a], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 49} -> 
     <|"Statement" -> HoldForm[
        OverBar[a \[CirclePlus] OverBar[b]] \[CircleTimes] b == 
         OverBar[a \[CirclePlus] OverBar[b]]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 56}, 
        "Construct" -> {"Axiom", 4}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] OverBar[b_]) -> a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a \[CirclePlus] OverBar[b]] \[CircleTimes] b == 
           OverBar[a \[CirclePlus] OverBar[b]]], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 50} -> 
     <|"Statement" -> HoldForm[b \[CircleTimes] OverBar[a \[CirclePlus] 
            OverBar[b]] == OverBar[a \[CirclePlus] OverBar[b]]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 49}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          b \[CircleTimes] OverBar[a \[CirclePlus] OverBar[b]] == 
           OverBar[a \[CirclePlus] OverBar[b]]], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 57} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] OverBar[a] == 
         a \[CircleTimes] OverBar[b \[CirclePlus] OverBar[OverBar[a]]]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 22}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CircleTimes] 
           (OverBar[a_] \[CircleTimes] (b_)) -> a \[CircleTimes] OverBar[a], 
        "Side" -> 1, "Subpattern" -> OverBar[a_] \[CircleTimes] (b_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 50}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] OverBar[(b_) \[CirclePlus] OverBar[a_]] -> 
          OverBar[b \[CirclePlus] OverBar[a]], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 51} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] OverBar[a] == 
         a \[CircleTimes] OverBar[b \[CirclePlus] a]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 57}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Position" -> {2, 1, 2}, 
        "Rule" -> OverBar[OverBar[a_]] -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CircleTimes] OverBar[a] == 
           a \[CircleTimes] OverBar[b \[CirclePlus] a]], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 58} -> 
     <|"Statement" -> HoldForm[OverBar[a \[CirclePlus] b] \[CirclePlus] 
          OverBar[OverBar[a \[CirclePlus] b]] == 
         OverBar[a \[CirclePlus] b] \[CirclePlus] 
          OverBar[c \[CircleTimes] (b \[CircleTimes] OverBar[b])]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 54}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CirclePlus] 
           OverBar[(b_) \[CircleTimes] ((c_) \[CircleTimes] (a_))] -> 
          a \[CirclePlus] OverBar[a], "Side" -> 1, "Subpattern" -> 
         (c_) \[CircleTimes] (a_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 51}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CircleTimes] OverBar[(b_) \[CirclePlus] 
             (a_)] -> a \[CircleTimes] OverBar[a], "MatchingSide" -> 1, 
        "Position" -> {2, 1, 2}|>|>, {"CriticalPairLemma", 59} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] OverBar[a] == 
         (a \[CircleTimes] OverBar[a]) \[CircleTimes] b], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 10}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CircleTimes] 
           ((a_) \[CirclePlus] (b_)) -> a, "Side" -> 1, 
        "Subpattern" -> (a_) \[CirclePlus] (b_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 1}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CircleTimes] OverBar[a_] \[CirclePlus] 
           (b_) -> b, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 60} -> 
     <|"Statement" -> HoldForm[b \[CircleTimes] OverBar[b] == 
         a \[CircleTimes] (b \[CircleTimes] OverBar[b])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 59}, 
        "Orientation" -> -1, "Rule" -> 
         ((a_) \[CircleTimes] OverBar[a_]) \[CircleTimes] (b_) -> 
          a \[CircleTimes] OverBar[a], "Side" -> 1, "Subpattern" -> {}, 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"SubstitutionLemma", 52} -> 
     <|"Statement" -> HoldForm[OverBar[a \[CirclePlus] b] \[CirclePlus] 
          OverBar[OverBar[a \[CirclePlus] b]] == 
         OverBar[a \[CirclePlus] b] \[CirclePlus] 
          OverBar[b \[CircleTimes] OverBar[b]]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 58}, 
        "Construct" -> {"CriticalPairLemma", 60}, "Position" -> {2, 1}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] OverBar[b_]) -> 
          b \[CircleTimes] OverBar[b], "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[
          OverBar[a \[CirclePlus] b] \[CirclePlus] OverBar[
             OverBar[a \[CirclePlus] b]] == 
           OverBar[a \[CirclePlus] b] \[CirclePlus] OverBar[b \[CircleTimes] 
              OverBar[b]]], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 61} -> 
     <|"Statement" -> HoldForm[b == OverBar[a \[CircleTimes] 
            OverBar[a]] \[CircleTimes] b], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 5}, "Orientation" -> -1, 
        "Rule" -> ((a_) \[CirclePlus] OverBar[a_]) \[CircleTimes] (b_) -> b, 
        "Side" -> 1, "Subpattern" -> (a_) \[CirclePlus] OverBar[a_], 
        "MatchingConstruct" -> {"CriticalPairLemma", 1}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CircleTimes] OverBar[a_] \[CirclePlus] (b_) -> b, 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 62} -> 
     <|"Statement" -> HoldForm[OverBar[b \[CircleTimes] OverBar[b]] == 
         a \[CirclePlus] OverBar[b \[CircleTimes] OverBar[b]]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 15}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] 
            (a_)) -> a, "Side" -> 1, "Subpattern" -> {}, 
        "MatchingConstruct" -> {"CriticalPairLemma", 61}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         OverBar[(a_) \[CircleTimes] OverBar[a_]] \[CircleTimes] (b_) -> b, 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"SubstitutionLemma", 53} -> 
     <|"Statement" -> HoldForm[OverBar[a \[CirclePlus] b] \[CirclePlus] 
          OverBar[OverBar[a \[CirclePlus] b]] == 
         OverBar[b \[CircleTimes] OverBar[b]]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 52}, 
        "Construct" -> {"CriticalPairLemma", 62}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] OverBar[(b_) \[CircleTimes] 
             OverBar[b_]] -> OverBar[b \[CircleTimes] OverBar[b]], 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a \[CirclePlus] b] \[CirclePlus] OverBar[
             OverBar[a \[CirclePlus] b]] == OverBar[b \[CircleTimes] 
             OverBar[b]]], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 54} -> 
     <|"Statement" -> HoldForm[OverBar[a \[CirclePlus] b] \[CirclePlus] 
          (a \[CirclePlus] b) == OverBar[b \[CircleTimes] OverBar[b]]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 53}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Position" -> {2}, 
        "Rule" -> OverBar[OverBar[a_]] -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          OverBar[a \[CirclePlus] b] \[CirclePlus] (a \[CirclePlus] b) == 
           OverBar[b \[CircleTimes] OverBar[b]]], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 55} -> 
     <|"Statement" -> HoldForm[(a \[CirclePlus] b) \[CirclePlus] 
          OverBar[a \[CirclePlus] b] == OverBar[b \[CircleTimes] 
           OverBar[b]]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 54}, 
        "Construct" -> {"Axiom", 5}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (a \[CirclePlus] b) \[CirclePlus] OverBar[a \[CirclePlus] b] == 
           OverBar[b \[CircleTimes] OverBar[b]]], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 56} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] 
          (OverBar[a \[CirclePlus] b] \[CirclePlus] b) == 
         OverBar[b \[CircleTimes] OverBar[b]]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 55}, 
        "Construct" -> {"SubstitutionLemma", 36}, "Position" -> {}, 
        "Rule" -> ((a_) \[CirclePlus] (b_)) \[CirclePlus] (c_) -> 
          a \[CirclePlus] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CirclePlus] 
            (OverBar[a \[CirclePlus] b] \[CirclePlus] b) == 
           OverBar[b \[CircleTimes] OverBar[b]]], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 63} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] c == a \[CirclePlus] 
          (b \[CircleTimes] a \[CirclePlus] c)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 31}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CirclePlus] ((b_) \[CirclePlus] 
            (c_) \[CircleTimes] (a_)) -> a \[CirclePlus] b, "Side" -> 1, 
        "Subpattern" -> (b_) \[CirclePlus] (c_) \[CircleTimes] (a_), 
        "MatchingConstruct" -> {"Axiom", 5}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 64} -> 
     <|"Statement" -> HoldForm[(a \[CirclePlus] b) \[CirclePlus] c == 
         (a \[CirclePlus] b) \[CirclePlus] (b \[CirclePlus] c)], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 63}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CirclePlus] 
           ((b_) \[CircleTimes] (a_) \[CirclePlus] (c_)) -> 
          a \[CirclePlus] c, "Side" -> 1, "Subpattern" -> 
         (b_) \[CircleTimes] (a_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 15}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] (a_)) -> a, 
        "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"SubstitutionLemma", 57} -> 
     <|"Statement" -> HoldForm[(a \[CirclePlus] b) \[CirclePlus] c == 
         a \[CirclePlus] ((b \[CirclePlus] c) \[CirclePlus] b)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 64}, 
        "Construct" -> {"SubstitutionLemma", 36}, "Position" -> {}, 
        "Rule" -> ((a_) \[CirclePlus] (b_)) \[CirclePlus] (c_) -> 
          a \[CirclePlus] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[(a \[CirclePlus] b) \[CirclePlus] c == 
           a \[CirclePlus] ((b \[CirclePlus] c) \[CirclePlus] b)], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 58} -> 
     <|"Statement" -> HoldForm[(a \[CirclePlus] b) \[CirclePlus] c == 
         a \[CirclePlus] (b \[CirclePlus] (b \[CirclePlus] c))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 57}, 
        "Construct" -> {"Axiom", 5}, "Position" -> {2}, 
        "Rule" -> (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (a \[CirclePlus] b) \[CirclePlus] c == a \[CirclePlus] 
            (b \[CirclePlus] (b \[CirclePlus] c))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 59} -> 
     <|"Statement" -> HoldForm[(a \[CirclePlus] b) \[CirclePlus] c == 
         a \[CirclePlus] (b \[CirclePlus] c)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 58}, 
        "Construct" -> {"CriticalPairLemma", 33}, "Position" -> {2}, 
        "Rule" -> (a_) \[CirclePlus] ((a_) \[CirclePlus] (b_)) -> 
          a \[CirclePlus] b, "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[(a \[CirclePlus] b) \[CirclePlus] c == a \[CirclePlus] 
            (b \[CirclePlus] c)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 60} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] (c \[CirclePlus] b) == 
         a \[CirclePlus] (b \[CirclePlus] c)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 59}, 
        "Construct" -> {"SubstitutionLemma", 36}, "Position" -> {}, 
        "Rule" -> ((a_) \[CirclePlus] (b_)) \[CirclePlus] (c_) -> 
          a \[CirclePlus] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CirclePlus] (c \[CirclePlus] b) == 
           a \[CirclePlus] (b \[CirclePlus] c)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 61} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] (b \[CirclePlus] 
           OverBar[a \[CirclePlus] b]) == OverBar[b \[CircleTimes] 
           OverBar[b]]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 56}, 
        "Construct" -> {"SubstitutionLemma", 60}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] ((b_) \[CirclePlus] (c_)) -> 
          a \[CirclePlus] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CirclePlus] (b \[CirclePlus] 
             OverBar[a \[CirclePlus] b]) == OverBar[b \[CircleTimes] 
             OverBar[b]]], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 65} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] 
          (OverBar[b \[CirclePlus] OverBar[a]] \[CirclePlus] b) == 
         a \[CircleTimes] OverBar[OverBar[a] \[CircleTimes] 
            OverBar[OverBar[a]]]], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 46}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] 
            (OverBar[a_] \[CirclePlus] (c_))) -> a \[CircleTimes] 
           (c \[CirclePlus] b), "Side" -> 1, "Subpattern" -> 
         (b_) \[CirclePlus] (OverBar[a_] \[CirclePlus] (c_)), 
        "MatchingConstruct" -> {"SubstitutionLemma", 61}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CirclePlus] ((b_) \[CirclePlus] OverBar[(a_) \[CirclePlus] 
              (b_)]) -> OverBar[b \[CircleTimes] OverBar[b]], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 66} -> 
     <|"Statement" -> HoldForm[a == a \[CircleTimes] 
          OverBar[b \[CircleTimes] OverBar[b]]], 
      "Proof" -> <|"Construct" -> {"Axiom", 4}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] OverBar[b_]) -> a, 
        "Side" -> 1, "Subpattern" -> (b_) \[CirclePlus] OverBar[b_], 
        "MatchingConstruct" -> {"CriticalPairLemma", 1}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CircleTimes] OverBar[a_] \[CirclePlus] (b_) -> b, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 62} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] 
          (OverBar[b \[CirclePlus] OverBar[a]] \[CirclePlus] b) == a], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 65}, 
        "Construct" -> {"CriticalPairLemma", 66}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] OverBar[(b_) \[CircleTimes] 
             OverBar[b_]] -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[a \[CircleTimes] (OverBar[b \[CirclePlus] OverBar[
                a]] \[CirclePlus] b) == a], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 67} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CirclePlus] 
           c \[CircleTimes] a) == a \[CircleTimes] b \[CirclePlus] 
          a \[CircleTimes] c], "Proof" -> <|"Construct" -> {"Axiom", 2}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CircleTimes] (b_) \[CirclePlus] 
           (a_) \[CircleTimes] (c_) -> a \[CircleTimes] (b \[CirclePlus] c), 
        "Side" -> 1, "Subpattern" -> (a_) \[CircleTimes] (c_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 4}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] ((b_) \[CircleTimes] (a_)) -> 
          a \[CircleTimes] b, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 63} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CirclePlus] 
           c \[CircleTimes] a) == a \[CircleTimes] (b \[CirclePlus] c)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 67}, 
        "Construct" -> {"Axiom", 2}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) \[CirclePlus] (a_) \[CircleTimes] 
            (c_) -> a \[CircleTimes] (b \[CirclePlus] c), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CircleTimes] (b \[CirclePlus] c \[CircleTimes] a) == 
           a \[CircleTimes] (b \[CirclePlus] c)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 64} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (c \[CirclePlus] b) == 
         a \[CircleTimes] (b \[CirclePlus] c)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 63}, 
        "Construct" -> {"CriticalPairLemma", 13}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] (c_) \[CircleTimes] 
             (a_)) -> a \[CircleTimes] (c \[CirclePlus] b), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a \[CircleTimes] (c \[CirclePlus] b) == a \[CircleTimes] 
            (b \[CirclePlus] c)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 65} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CirclePlus] 
           OverBar[b \[CirclePlus] OverBar[a]]) == a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 62}, 
        "Construct" -> {"SubstitutionLemma", 64}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] (c_)) -> 
          a \[CircleTimes] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CircleTimes] (b \[CirclePlus] 
             OverBar[b \[CirclePlus] OverBar[a]]) == a], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 68} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] (b \[CircleTimes] 
           OverBar[a \[CirclePlus] OverBar[b]]) == OverBar[a] \[CircleTimes] 
          b], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 26}, 
        "Orientation" -> 1, "Rule" -> OverBar[a_] \[CircleTimes] 
           ((b_) \[CircleTimes] ((a_) \[CirclePlus] (c_))) -> 
          OverBar[a] \[CircleTimes] (b \[CircleTimes] c), "Side" -> 1, 
        "Subpattern" -> (b_) \[CircleTimes] ((a_) \[CirclePlus] (c_)), 
        "MatchingConstruct" -> {"SubstitutionLemma", 65}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] ((b_) \[CirclePlus] OverBar[(b_) \[CirclePlus] 
              OverBar[a_]]) -> a, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 66} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] 
          OverBar[a \[CirclePlus] OverBar[b]] == OverBar[a] \[CircleTimes] 
          b], "Proof" -> <|"Input" -> {"CriticalPairLemma", 68}, 
        "Construct" -> {"SubstitutionLemma", 50}, "Position" -> {2}, 
        "Rule" -> (a_) \[CircleTimes] OverBar[(b_) \[CirclePlus] 
             OverBar[a_]] -> OverBar[b \[CirclePlus] OverBar[a]], 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          OverBar[a] \[CircleTimes] OverBar[a \[CirclePlus] OverBar[b]] == 
           OverBar[a] \[CircleTimes] b], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 69} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] (a \[CirclePlus] 
           b) == OverBar[a] \[CirclePlus] a], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 48}, 
        "Orientation" -> -1, "Rule" -> OverBar[a_] \[CirclePlus] 
           (a_) \[CircleTimes] (b_) -> OverBar[a] \[CirclePlus] b, 
        "Side" -> 1, "Subpattern" -> (a_) \[CircleTimes] (b_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 10}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CircleTimes] ((a_) \[CirclePlus] (b_)) -> a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 67} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] (a \[CirclePlus] 
           b) == a \[CirclePlus] OverBar[a]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 69}, 
        "Construct" -> {"Axiom", 5}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a] \[CirclePlus] (a \[CirclePlus] b) == 
           a \[CirclePlus] OverBar[a]], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 70} -> 
     <|"Statement" -> HoldForm[OverBar[a \[CirclePlus] b] \[CircleTimes] 
          OverBar[a] == OverBar[a \[CirclePlus] b] \[CircleTimes] 
          (a \[CirclePlus] OverBar[a])], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 47}, "Orientation" -> -1, 
        "Rule" -> OverBar[a_] \[CircleTimes] ((b_) \[CirclePlus] (a_)) -> 
          OverBar[a] \[CircleTimes] b, "Side" -> 1, "Subpattern" -> 
         (b_) \[CirclePlus] (a_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 67}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> OverBar[a_] \[CirclePlus] ((a_) \[CirclePlus] 
            (b_)) -> a \[CirclePlus] OverBar[a], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 68} -> 
     <|"Statement" -> HoldForm[OverBar[a \[CirclePlus] b] \[CircleTimes] 
          OverBar[a] == OverBar[a \[CirclePlus] b]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 70}, 
        "Construct" -> {"Axiom", 4}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] OverBar[b_]) -> a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a \[CirclePlus] b] \[CircleTimes] OverBar[a] == 
           OverBar[a \[CirclePlus] b]], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 69} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] 
          OverBar[a \[CirclePlus] b] == OverBar[a \[CirclePlus] b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 68}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          OverBar[a] \[CircleTimes] OverBar[a \[CirclePlus] b] == 
           OverBar[a \[CirclePlus] b]], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 70} -> 
     <|"Statement" -> HoldForm[OverBar[a \[CirclePlus] OverBar[b]] == 
         OverBar[a] \[CircleTimes] b], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 66}, "Construct" -> 
         {"SubstitutionLemma", 69}, "Position" -> {}, 
        "Rule" -> OverBar[a_] \[CircleTimes] OverBar[(a_) \[CirclePlus] 
             (b_)] -> OverBar[a \[CirclePlus] b], "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[OverBar[a \[CirclePlus] OverBar[b]] == 
           OverBar[a] \[CircleTimes] b], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 71} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] OverBar[b] == 
         OverBar[a \[CirclePlus] b]], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 70}, "Orientation" -> 1, 
        "Rule" -> OverBar[(a_) \[CirclePlus] OverBar[b_]] -> 
          OverBar[a] \[CircleTimes] b, "Side" -> 1, "Subpattern" -> 
         OverBar[b_], "MatchingConstruct" -> {"SubstitutionLemma", 3}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         OverBar[OverBar[a_]] -> a, "MatchingSide" -> 1, 
        "Position" -> {1, 2}|>|>, {"Conclusion", 1} -> 
     <|"Statement" -> HoldForm[OverBar[a \[CirclePlus] b] == 
         OverBar[a \[CirclePlus] b]], "Proof" -> 
       <|"Input" -> {"Hypothesis", 1}, "Construct" -> {"CriticalPairLemma", 
          71}, "Position" -> {}, "Rule" -> OverBar[a_] \[CircleTimes] 
           OverBar[b_] -> OverBar[a \[CirclePlus] b], "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a \[CirclePlus] b] == 
           OverBar[a \[CirclePlus] b]], "Source" -> "cpl"|>|>}|>]
