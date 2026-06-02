ProofObject["EquationalLogic", Inactive[Equal][OverBar[a] \[CirclePlus] a, 
  OverBar[b] \[CirclePlus] b], {Inactive[Equal][(a_) \[CircleTimes] (b_), 
   (b_) \[CircleTimes] (a_)], Inactive[Equal][(a_) \[CircleTimes] 
    ((b_) \[CirclePlus] (c_)), (a_) \[CircleTimes] (b_) \[CirclePlus] 
    (a_) \[CircleTimes] (c_)], Inactive[Equal][
   (a_) \[CirclePlus] (b_) \[CircleTimes] OverBar[b_], a_], 
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
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         OverBar[b] \[CirclePlus] b], "Proof" -> <||>|>, 
    {"CriticalPairLemma", 1} -> 
     <|"Statement" -> HoldForm[b == (a \[CirclePlus] 
           OverBar[a]) \[CircleTimes] b], "Proof" -> 
       <|"Construct" -> {"Axiom", 4}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] OverBar[b_]) -> a, 
        "Side" -> 1, "Subpattern" -> {}, "MatchingConstruct" -> {"Axiom", 1}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 2} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] OverBar[a] \[CircleTimes] b == 
         a \[CirclePlus] b], "Proof" -> <|"Construct" -> {"Axiom", 6}, 
        "Orientation" -> -1, "Rule" -> 
         ((a_) \[CirclePlus] (b_)) \[CircleTimes] ((a_) \[CirclePlus] 
            (c_)) -> a \[CirclePlus] b \[CircleTimes] c, "Side" -> 1, 
        "Subpattern" -> {}, "MatchingConstruct" -> {"CriticalPairLemma", 1}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CirclePlus] OverBar[a_]) \[CircleTimes] (b_) -> b, 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"CriticalPairLemma", 3} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CirclePlus] 
           OverBar[a]) == a \[CircleTimes] b], 
      "Proof" -> <|"Construct" -> {"Axiom", 2}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CircleTimes] (b_) \[CirclePlus] (a_) \[CircleTimes] 
            (c_) -> a \[CircleTimes] (b \[CirclePlus] c), "Side" -> 1, 
        "Subpattern" -> {}, "MatchingConstruct" -> {"Axiom", 3}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CirclePlus] (b_) \[CircleTimes] OverBar[b_] -> a, 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"CriticalPairLemma", 4} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] a == a], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 3}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] 
            OverBar[a_]) -> a \[CircleTimes] b, "Side" -> 1, 
        "Subpattern" -> {}, "MatchingConstruct" -> {"Axiom", 4}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] ((b_) \[CirclePlus] OverBar[b_]) -> a, 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"CriticalPairLemma", 5} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (a \[CirclePlus] b) == 
         a \[CirclePlus] a \[CircleTimes] b], 
      "Proof" -> <|"Construct" -> {"Axiom", 2}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CircleTimes] (b_) \[CirclePlus] (a_) \[CircleTimes] 
            (c_) -> a \[CircleTimes] (b \[CirclePlus] c), "Side" -> 1, 
        "Subpattern" -> (a_) \[CircleTimes] (b_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 4}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CircleTimes] (a_) -> a, "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 6} -> 
     <|"Statement" -> HoldForm[b == a \[CircleTimes] OverBar[a] \[CirclePlus] 
          b], "Proof" -> <|"Construct" -> {"Axiom", 3}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CirclePlus] (b_) \[CircleTimes] OverBar[b_] -> a, 
        "Side" -> 1, "Subpattern" -> {}, "MatchingConstruct" -> {"Axiom", 5}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 7} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (OverBar[a] \[CirclePlus] 
           b) == a \[CircleTimes] b], "Proof" -> 
       <|"Construct" -> {"Axiom", 2}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CircleTimes] (b_) \[CirclePlus] (a_) \[CircleTimes] 
            (c_) -> a \[CircleTimes] (b \[CirclePlus] c), "Side" -> 1, 
        "Subpattern" -> {}, "MatchingConstruct" -> {"CriticalPairLemma", 6}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CircleTimes] OverBar[a_] \[CirclePlus] (b_) -> b, 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"CriticalPairLemma", 8} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] OverBar[OverBar[a]] == a], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 7}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CircleTimes] 
           (OverBar[a_] \[CirclePlus] (b_)) -> a \[CircleTimes] b, 
        "Side" -> 1, "Subpattern" -> {}, "MatchingConstruct" -> {"Axiom", 4}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] ((b_) \[CirclePlus] OverBar[b_]) -> a, 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"CriticalPairLemma", 9} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (a \[CirclePlus] 
           OverBar[OverBar[a]]) == a \[CirclePlus] a], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 5}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CirclePlus] (a_) \[CircleTimes] 
            (b_) -> a \[CircleTimes] (a \[CirclePlus] b), "Side" -> 1, 
        "Subpattern" -> (a_) \[CircleTimes] (b_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 8}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CircleTimes] OverBar[OverBar[a_]] -> a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 10} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] b \[CircleTimes] OverBar[a] == 
         a \[CirclePlus] b], "Proof" -> <|"Construct" -> {"Axiom", 6}, 
        "Orientation" -> -1, "Rule" -> 
         ((a_) \[CirclePlus] (b_)) \[CircleTimes] ((a_) \[CirclePlus] 
            (c_)) -> a \[CirclePlus] b \[CircleTimes] c, "Side" -> 1, 
        "Subpattern" -> {}, "MatchingConstruct" -> {"Axiom", 4}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] ((b_) \[CirclePlus] OverBar[b_]) -> a, 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"CriticalPairLemma", 11} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] a == a], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 10}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CirclePlus] (b_) \[CircleTimes] 
            OverBar[a_] -> a \[CirclePlus] b, "Side" -> 1, 
        "Subpattern" -> {}, "MatchingConstruct" -> {"Axiom", 3}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CirclePlus] (b_) \[CircleTimes] OverBar[b_] -> a, 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (a \[CirclePlus] 
           OverBar[OverBar[a]]) == a], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 9}, "Construct" -> 
         {"CriticalPairLemma", 11}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] (a_) -> a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CircleTimes] (a \[CirclePlus] 
             OverBar[OverBar[a]]) == a], "Source" -> "cpl"|>|>, 
    {"CriticalPairLemma", 12} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] OverBar[
           OverBar[OverBar[a]]] == a \[CirclePlus] OverBar[a]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 2}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CirclePlus] 
           OverBar[a_] \[CircleTimes] (b_) -> a \[CirclePlus] b, "Side" -> 1, 
        "Subpattern" -> OverBar[a_] \[CircleTimes] (b_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 8}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] OverBar[OverBar[a_]] -> a, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 13} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[a]] \[CircleTimes] a == 
         OverBar[OverBar[a]] \[CircleTimes] (a \[CirclePlus] OverBar[a])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 3}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] 
            OverBar[a_]) -> a \[CircleTimes] b, "Side" -> 1, 
        "Subpattern" -> (b_) \[CirclePlus] OverBar[a_], 
        "MatchingConstruct" -> {"CriticalPairLemma", 12}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CirclePlus] OverBar[OverBar[OverBar[a_]]] -> 
          a \[CirclePlus] OverBar[a], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 5} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] OverBar[OverBar[a]] == 
         OverBar[OverBar[a]] \[CircleTimes] (a \[CirclePlus] OverBar[a])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 13}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a \[CircleTimes] OverBar[OverBar[a]] == 
           OverBar[OverBar[a]] \[CircleTimes] (a \[CirclePlus] OverBar[a])], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 6} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] OverBar[OverBar[a]] == 
         OverBar[OverBar[a]]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 5}, "Construct" -> {"Axiom", 4}, 
        "Position" -> {}, "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] 
            OverBar[b_]) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[a \[CircleTimes] OverBar[OverBar[a]] == 
           OverBar[OverBar[a]]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 7} -> 
     <|"Statement" -> HoldForm[a == OverBar[OverBar[a]]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 6}, 
        "Construct" -> {"CriticalPairLemma", 8}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] OverBar[OverBar[a_]] -> a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a == OverBar[OverBar[a]]], "Source" -> "cpl"|>|>, 
    {"CriticalPairLemma", 14} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] b == 
         OverBar[a] \[CirclePlus] b \[CircleTimes] a], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 10}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CirclePlus] (b_) \[CircleTimes] 
            OverBar[a_] -> a \[CirclePlus] b, "Side" -> 1, 
        "Subpattern" -> OverBar[a_], "MatchingConstruct" -> 
         {"SubstitutionLemma", 7}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> OverBar[OverBar[a_]] -> a, "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 15} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CircleTimes] a) == 
         a \[CircleTimes] (OverBar[a] \[CirclePlus] b)], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 7}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CircleTimes] 
           (OverBar[a_] \[CirclePlus] (b_)) -> a \[CircleTimes] b, 
        "Side" -> 1, "Subpattern" -> OverBar[a_] \[CirclePlus] (b_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 14}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         OverBar[a_] \[CirclePlus] (b_) \[CircleTimes] (a_) -> 
          OverBar[a] \[CirclePlus] b, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 9} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CircleTimes] a) == 
         a \[CircleTimes] b], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 15}, "Construct" -> 
         {"CriticalPairLemma", 7}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (OverBar[a_] \[CirclePlus] (b_)) -> 
          a \[CircleTimes] b, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[a \[CircleTimes] (b \[CircleTimes] a) == 
           a \[CircleTimes] b], "Source" -> "cpl"|>|>, 
    {"CriticalPairLemma", 16} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CirclePlus] 
           c \[CircleTimes] a) == a \[CircleTimes] b \[CirclePlus] 
          a \[CircleTimes] c], "Proof" -> <|"Construct" -> {"Axiom", 2}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CircleTimes] (b_) \[CirclePlus] 
           (a_) \[CircleTimes] (c_) -> a \[CircleTimes] (b \[CirclePlus] c), 
        "Side" -> 1, "Subpattern" -> (a_) \[CircleTimes] (c_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 9}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] ((b_) \[CircleTimes] (a_)) -> 
          a \[CircleTimes] b, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 10} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CirclePlus] 
           c \[CircleTimes] a) == a \[CircleTimes] (b \[CirclePlus] c)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 16}, 
        "Construct" -> {"Axiom", 2}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) \[CirclePlus] (a_) \[CircleTimes] 
            (c_) -> a \[CircleTimes] (b \[CirclePlus] c), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CircleTimes] (b \[CirclePlus] c \[CircleTimes] a) == 
           a \[CircleTimes] (b \[CirclePlus] c)], "Source" -> "cpl"|>|>, 
    {"CriticalPairLemma", 17} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] 
          (b \[CircleTimes] a \[CirclePlus] c) == 
         a \[CircleTimes] b \[CirclePlus] a \[CircleTimes] c], 
      "Proof" -> <|"Construct" -> {"Axiom", 2}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CircleTimes] (b_) \[CirclePlus] (a_) \[CircleTimes] 
            (c_) -> a \[CircleTimes] (b \[CirclePlus] c), "Side" -> 1, 
        "Subpattern" -> (a_) \[CircleTimes] (b_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 9}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (a_)) -> 
          a \[CircleTimes] b, "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"SubstitutionLemma", 11} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] 
          (b \[CircleTimes] a \[CirclePlus] c) == a \[CircleTimes] 
          (b \[CirclePlus] c)], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 17}, "Construct" -> {"Axiom", 2}, 
        "Position" -> {}, "Rule" -> (a_) \[CircleTimes] (b_) \[CirclePlus] 
           (a_) \[CircleTimes] (c_) -> a \[CircleTimes] (b \[CirclePlus] c), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CircleTimes] (b \[CircleTimes] a \[CirclePlus] c) == 
           a \[CircleTimes] (b \[CirclePlus] c)], "Source" -> "cpl"|>|>, 
    {"CriticalPairLemma", 18} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (c \[CirclePlus] b) == 
         a \[CircleTimes] (b \[CirclePlus] c \[CircleTimes] a)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 11}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CircleTimes] 
           ((b_) \[CircleTimes] (a_) \[CirclePlus] (c_)) -> 
          a \[CircleTimes] (b \[CirclePlus] c), "Side" -> 1, 
        "Subpattern" -> (b_) \[CircleTimes] (a_) \[CirclePlus] (c_), 
        "MatchingConstruct" -> {"Axiom", 5}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 12} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (c \[CirclePlus] b) == 
         a \[CircleTimes] (b \[CirclePlus] c)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 10}, 
        "Construct" -> {"CriticalPairLemma", 18}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] (c_) \[CircleTimes] 
             (a_)) -> a \[CircleTimes] (c \[CirclePlus] b), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a \[CircleTimes] (c \[CirclePlus] b) == a \[CircleTimes] 
            (b \[CirclePlus] c)], "Source" -> "cpl"|>|>, 
    {"CriticalPairLemma", 19} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] (b \[CirclePlus] 
           OverBar[OverBar[a]]) == a \[CirclePlus] OverBar[a] \[CircleTimes] 
           b], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 2}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CirclePlus] 
           OverBar[a_] \[CircleTimes] (b_) -> a \[CirclePlus] b, "Side" -> 1, 
        "Subpattern" -> OverBar[a_] \[CircleTimes] (b_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 3}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] ((b_) \[CirclePlus] OverBar[a_]) -> 
          a \[CircleTimes] b, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 16} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] (b \[CirclePlus] 
           OverBar[OverBar[a]]) == a \[CirclePlus] b], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 19}, 
        "Construct" -> {"CriticalPairLemma", 2}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] OverBar[a_] \[CircleTimes] (b_) -> 
          a \[CirclePlus] b, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[a \[CirclePlus] (b \[CirclePlus] OverBar[OverBar[a]]) == 
           a \[CirclePlus] b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 17} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] (b \[CirclePlus] a) == 
         a \[CirclePlus] b], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 16}, "Construct" -> 
         {"SubstitutionLemma", 7}, "Position" -> {2, 2}, 
        "Rule" -> OverBar[OverBar[a_]] -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CirclePlus] (b \[CirclePlus] a) == 
           a \[CirclePlus] b], "Source" -> "cpl"|>|>, 
    {"CriticalPairLemma", 20} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] b == a \[CirclePlus] 
          (a \[CirclePlus] b)], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 17}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CirclePlus] ((b_) \[CirclePlus] (a_)) -> 
          a \[CirclePlus] b, "Side" -> 1, "Subpattern" -> 
         (b_) \[CirclePlus] (a_), "MatchingConstruct" -> {"Axiom", 5}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 21} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] b \[CircleTimes] 
           (a \[CirclePlus] c) == (a \[CirclePlus] b) \[CircleTimes] 
          (a \[CirclePlus] c)], "Proof" -> <|"Construct" -> {"Axiom", 6}, 
        "Orientation" -> -1, "Rule" -> 
         ((a_) \[CirclePlus] (b_)) \[CircleTimes] ((a_) \[CirclePlus] 
            (c_)) -> a \[CirclePlus] b \[CircleTimes] c, "Side" -> 1, 
        "Subpattern" -> (a_) \[CirclePlus] (c_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 20}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CirclePlus] ((a_) \[CirclePlus] (b_)) -> 
          a \[CirclePlus] b, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 18} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] b \[CircleTimes] 
           (a \[CirclePlus] c) == a \[CirclePlus] b \[CircleTimes] c], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 21}, 
        "Construct" -> {"Axiom", 6}, "Position" -> {}, 
        "Rule" -> ((a_) \[CirclePlus] (b_)) \[CircleTimes] 
           ((a_) \[CirclePlus] (c_)) -> a \[CirclePlus] b \[CircleTimes] c, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CirclePlus] b \[CircleTimes] (a \[CirclePlus] c) == 
           a \[CirclePlus] b \[CircleTimes] c], "Source" -> "cpl"|>|>, 
    {"CriticalPairLemma", 22} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] b \[CircleTimes] c == 
         (a \[CirclePlus] b) \[CircleTimes] (c \[CirclePlus] a)], 
      "Proof" -> <|"Construct" -> {"Axiom", 6}, "Orientation" -> -1, 
        "Rule" -> ((a_) \[CirclePlus] (b_)) \[CircleTimes] 
           ((a_) \[CirclePlus] (c_)) -> a \[CirclePlus] b \[CircleTimes] c, 
        "Side" -> 1, "Subpattern" -> (a_) \[CirclePlus] (c_), 
        "MatchingConstruct" -> {"Axiom", 5}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 23} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] 
          (b \[CirclePlus] c) \[CircleTimes] b == a \[CirclePlus] 
          (b \[CirclePlus] c \[CircleTimes] a)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 18}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CirclePlus] (b_) \[CircleTimes] 
            ((a_) \[CirclePlus] (c_)) -> a \[CirclePlus] b \[CircleTimes] c, 
        "Side" -> 1, "Subpattern" -> (b_) \[CircleTimes] ((a_) \[CirclePlus] 
           (c_)), "MatchingConstruct" -> {"CriticalPairLemma", 22}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CirclePlus] (b_)) \[CircleTimes] ((c_) \[CirclePlus] 
            (a_)) -> a \[CirclePlus] b \[CircleTimes] c, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 19} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] b \[CircleTimes] 
           (b \[CirclePlus] c) == a \[CirclePlus] (b \[CirclePlus] 
           c \[CircleTimes] a)], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 23}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {2}, "Rule" -> (a_) \[CircleTimes] (b_) -> 
          b \[CircleTimes] a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[a \[CirclePlus] b \[CircleTimes] (b \[CirclePlus] c) == 
           a \[CirclePlus] (b \[CirclePlus] c \[CircleTimes] a)], 
        "Source" -> "cpl"|>|>, {"CriticalPairLemma", 24} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] b \[CircleTimes] 
           (c \[CirclePlus] a) == (a \[CirclePlus] b) \[CircleTimes] 
          (a \[CirclePlus] c)], "Proof" -> <|"Construct" -> {"Axiom", 6}, 
        "Orientation" -> -1, "Rule" -> 
         ((a_) \[CirclePlus] (b_)) \[CircleTimes] ((a_) \[CirclePlus] 
            (c_)) -> a \[CirclePlus] b \[CircleTimes] c, "Side" -> 1, 
        "Subpattern" -> (a_) \[CirclePlus] (c_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 17}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CirclePlus] ((b_) \[CirclePlus] (a_)) -> 
          a \[CirclePlus] b, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 20} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] b \[CircleTimes] 
           (c \[CirclePlus] a) == a \[CirclePlus] b \[CircleTimes] c], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 24}, 
        "Construct" -> {"Axiom", 6}, "Position" -> {}, 
        "Rule" -> ((a_) \[CirclePlus] (b_)) \[CircleTimes] 
           ((a_) \[CirclePlus] (c_)) -> a \[CirclePlus] b \[CircleTimes] c, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CirclePlus] b \[CircleTimes] (c \[CirclePlus] a) == 
           a \[CirclePlus] b \[CircleTimes] c], "Source" -> "cpl"|>|>, 
    {"CriticalPairLemma", 25} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] b \[CircleTimes] OverBar[b] == 
         a \[CirclePlus] b \[CircleTimes] a], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 20}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CirclePlus] (b_) \[CircleTimes] 
            ((c_) \[CirclePlus] (a_)) -> a \[CirclePlus] b \[CircleTimes] c, 
        "Side" -> 1, "Subpattern" -> (b_) \[CircleTimes] ((c_) \[CirclePlus] 
           (a_)), "MatchingConstruct" -> {"CriticalPairLemma", 7}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] (OverBar[a_] \[CirclePlus] (b_)) -> 
          a \[CircleTimes] b, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 21} -> 
     <|"Statement" -> HoldForm[a == a \[CirclePlus] b \[CircleTimes] a], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 25}, 
        "Construct" -> {"Axiom", 3}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] (b_) \[CircleTimes] OverBar[b_] -> a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a == a \[CirclePlus] b \[CircleTimes] a], "Source" -> "cpl"|>|>, 
    {"CriticalPairLemma", 26} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (a \[CirclePlus] b) == 
         a \[CirclePlus] b \[CircleTimes] a], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 5}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CirclePlus] (a_) \[CircleTimes] 
            (b_) -> a \[CircleTimes] (a \[CirclePlus] b), "Side" -> 1, 
        "Subpattern" -> (a_) \[CircleTimes] (b_), "MatchingConstruct" -> 
         {"Axiom", 1}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 22} -> 
     <|"Statement" -> HoldForm[a == a \[CircleTimes] (a \[CirclePlus] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 21}, 
        "Construct" -> {"CriticalPairLemma", 26}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] (b_) \[CircleTimes] (a_) -> 
          a \[CircleTimes] (a \[CirclePlus] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a == a \[CircleTimes] 
            (a \[CirclePlus] b)], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 23} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] b == a \[CirclePlus] 
          (b \[CirclePlus] c \[CircleTimes] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 19}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {2}, 
        "Rule" -> (a_) \[CircleTimes] ((a_) \[CirclePlus] (b_)) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[a \[CirclePlus] b == 
           a \[CirclePlus] (b \[CirclePlus] c \[CircleTimes] a)], 
        "Source" -> "cpl"|>|>, {"CriticalPairLemma", 27} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] c == a \[CirclePlus] 
          (b \[CircleTimes] a \[CirclePlus] c)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 23}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CirclePlus] ((b_) \[CirclePlus] 
            (c_) \[CircleTimes] (a_)) -> a \[CirclePlus] b, "Side" -> 1, 
        "Subpattern" -> (b_) \[CirclePlus] (c_) \[CircleTimes] (a_), 
        "MatchingConstruct" -> {"Axiom", 5}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 28} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CirclePlus] a) == 
         a \[CircleTimes] b \[CirclePlus] a], 
      "Proof" -> <|"Construct" -> {"Axiom", 2}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CircleTimes] (b_) \[CirclePlus] (a_) \[CircleTimes] 
            (c_) -> a \[CircleTimes] (b \[CirclePlus] c), "Side" -> 1, 
        "Subpattern" -> (a_) \[CircleTimes] (c_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 4}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CircleTimes] (a_) -> a, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 24} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CirclePlus] a) == 
         a \[CirclePlus] a \[CircleTimes] b], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 28}, 
        "Construct" -> {"Axiom", 5}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CircleTimes] (b \[CirclePlus] a) == a \[CirclePlus] 
            a \[CircleTimes] b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 25} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CirclePlus] a) == 
         a \[CircleTimes] (a \[CirclePlus] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 24}, 
        "Construct" -> {"CriticalPairLemma", 5}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] (a_) \[CircleTimes] (b_) -> 
          a \[CircleTimes] (a \[CirclePlus] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CircleTimes] (b \[CirclePlus] 
             a) == a \[CircleTimes] (a \[CirclePlus] b)], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 26} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CirclePlus] a) == a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 25}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((a_) \[CirclePlus] (b_)) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CircleTimes] (b \[CirclePlus] a) == a], "Source" -> "cpl"|>|>, 
    {"CriticalPairLemma", 29} -> 
     <|"Statement" -> HoldForm[(a \[CirclePlus] b) \[CirclePlus] c == 
         (a \[CirclePlus] b) \[CirclePlus] (b \[CirclePlus] c)], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 27}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CirclePlus] 
           ((b_) \[CircleTimes] (a_) \[CirclePlus] (c_)) -> 
          a \[CirclePlus] c, "Side" -> 1, "Subpattern" -> 
         (b_) \[CircleTimes] (a_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 26}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] (a_)) -> a, 
        "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"SubstitutionLemma", 27} -> 
     <|"Statement" -> HoldForm[c \[CirclePlus] (a \[CirclePlus] b) == 
         (a \[CirclePlus] b) \[CirclePlus] (b \[CirclePlus] c)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 29}, 
        "Construct" -> {"Axiom", 5}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          c \[CirclePlus] (a \[CirclePlus] b) == 
           (a \[CirclePlus] b) \[CirclePlus] (b \[CirclePlus] c)], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 28} -> 
     <|"Statement" -> HoldForm[a == a \[CirclePlus] b \[CircleTimes] a], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 26}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((a_) \[CirclePlus] (b_)) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a == a \[CirclePlus] b \[CircleTimes] a], "Source" -> "cpl"|>|>, 
    {"CriticalPairLemma", 30} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] 
          ((a \[CirclePlus] c) \[CirclePlus] b) == a \[CirclePlus] 
          a \[CircleTimes] b], "Proof" -> <|"Construct" -> {"Axiom", 2}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CircleTimes] (b_) \[CirclePlus] 
           (a_) \[CircleTimes] (c_) -> a \[CircleTimes] (b \[CirclePlus] c), 
        "Side" -> 1, "Subpattern" -> (a_) \[CircleTimes] (b_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 22}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CircleTimes] ((a_) \[CirclePlus] (b_)) -> a, 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"SubstitutionLemma", 29} -> 
     <|"Statement" -> HoldForm[a == a \[CirclePlus] a \[CircleTimes] b], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 5}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((a_) \[CirclePlus] (b_)) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a == a \[CirclePlus] a \[CircleTimes] b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 30} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] 
          ((a \[CirclePlus] c) \[CirclePlus] b) == a], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 30}, 
        "Construct" -> {"SubstitutionLemma", 29}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] (a_) \[CircleTimes] (b_) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CircleTimes] ((a \[CirclePlus] c) \[CirclePlus] b) == a], 
        "Source" -> "cpl"|>|>, {"CriticalPairLemma", 31} -> 
     <|"Statement" -> HoldForm[(a \[CirclePlus] b) \[CirclePlus] c == 
         ((a \[CirclePlus] b) \[CirclePlus] c) \[CirclePlus] a], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 28}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CirclePlus] (b_) \[CircleTimes] 
            (a_) -> a, "Side" -> 1, "Subpattern" -> (b_) \[CircleTimes] (a_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 30}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] (((a_) \[CirclePlus] (b_)) \[CirclePlus] 
            (c_)) -> a, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 31} -> 
     <|"Statement" -> HoldForm[(a \[CirclePlus] b) \[CirclePlus] c == 
         a \[CirclePlus] ((a \[CirclePlus] b) \[CirclePlus] c)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 31}, 
        "Construct" -> {"Axiom", 5}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (a \[CirclePlus] b) \[CirclePlus] c == a \[CirclePlus] 
            ((a \[CirclePlus] b) \[CirclePlus] c)], "Source" -> "cpl"|>|>, 
    {"CriticalPairLemma", 32} -> 
     <|"Statement" -> HoldForm[(a \[CirclePlus] b) \[CirclePlus] c == 
         a \[CirclePlus] ((b \[CirclePlus] a) \[CirclePlus] c)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 31}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CirclePlus] 
           (((a_) \[CirclePlus] (b_)) \[CirclePlus] (c_)) -> 
          (a \[CirclePlus] b) \[CirclePlus] c, "Side" -> 1, 
        "Subpattern" -> (a_) \[CirclePlus] (b_), "MatchingConstruct" -> 
         {"Axiom", 5}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, "MatchingSide" -> 1, 
        "Position" -> {2, 1}|>|>, {"CriticalPairLemma", 33} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] 
          ((c \[CirclePlus] a) \[CirclePlus] b) == a \[CirclePlus] 
          a \[CircleTimes] b], "Proof" -> <|"Construct" -> {"Axiom", 2}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CircleTimes] (b_) \[CirclePlus] 
           (a_) \[CircleTimes] (c_) -> a \[CircleTimes] (b \[CirclePlus] c), 
        "Side" -> 1, "Subpattern" -> (a_) \[CircleTimes] (b_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 26}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] ((b_) \[CirclePlus] (a_)) -> a, 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"SubstitutionLemma", 32} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] 
          ((c \[CirclePlus] a) \[CirclePlus] b) == a], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 33}, 
        "Construct" -> {"SubstitutionLemma", 29}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] (a_) \[CircleTimes] (b_) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CircleTimes] ((c \[CirclePlus] a) \[CirclePlus] b) == a], 
        "Source" -> "cpl"|>|>, {"CriticalPairLemma", 34} -> 
     <|"Statement" -> HoldForm[(a \[CirclePlus] b) \[CirclePlus] c == 
         ((a \[CirclePlus] b) \[CirclePlus] c) \[CirclePlus] b], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 28}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CirclePlus] (b_) \[CircleTimes] 
            (a_) -> a, "Side" -> 1, "Subpattern" -> (b_) \[CircleTimes] (a_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 32}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] (((b_) \[CirclePlus] (a_)) \[CirclePlus] 
            (c_)) -> a, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 33} -> 
     <|"Statement" -> HoldForm[(a \[CirclePlus] b) \[CirclePlus] c == 
         b \[CirclePlus] ((a \[CirclePlus] b) \[CirclePlus] c)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 34}, 
        "Construct" -> {"Axiom", 5}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (a \[CirclePlus] b) \[CirclePlus] c == b \[CirclePlus] 
            ((a \[CirclePlus] b) \[CirclePlus] c)], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 34} -> 
     <|"Statement" -> HoldForm[(a \[CirclePlus] b) \[CirclePlus] c == 
         (b \[CirclePlus] a) \[CirclePlus] c], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 32}, 
        "Construct" -> {"SubstitutionLemma", 33}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] (((b_) \[CirclePlus] (a_)) \[CirclePlus] 
            (c_)) -> (b \[CirclePlus] a) \[CirclePlus] c, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (a \[CirclePlus] b) \[CirclePlus] c == 
           (b \[CirclePlus] a) \[CirclePlus] c], "Source" -> "cpl"|>|>, 
    {"CriticalPairLemma", 35} -> 
     <|"Statement" -> HoldForm[(a \[CirclePlus] c) \[CirclePlus] b == 
         a \[CirclePlus] (b \[CirclePlus] (a \[CirclePlus] c))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 31}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CirclePlus] 
           (((a_) \[CirclePlus] (b_)) \[CirclePlus] (c_)) -> 
          (a \[CirclePlus] b) \[CirclePlus] c, "Side" -> 1, 
        "Subpattern" -> ((a_) \[CirclePlus] (b_)) \[CirclePlus] (c_), 
        "MatchingConstruct" -> {"Axiom", 5}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 36} -> 
     <|"Statement" -> HoldForm[(a \[CirclePlus] b) \[CirclePlus] c == 
         (a \[CirclePlus] b) \[CirclePlus] (c \[CirclePlus] b)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 23}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CirclePlus] ((b_) \[CirclePlus] 
            (c_) \[CircleTimes] (a_)) -> a \[CirclePlus] b, "Side" -> 1, 
        "Subpattern" -> (c_) \[CircleTimes] (a_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 26}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] (a_)) -> a, 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"CriticalPairLemma", 37} -> 
     <|"Statement" -> HoldForm[(a \[CirclePlus] c) \[CirclePlus] 
          (b \[CirclePlus] c) == a \[CirclePlus] 
          ((b \[CirclePlus] c) \[CirclePlus] a)], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 35}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CirclePlus] ((b_) \[CirclePlus] 
            ((a_) \[CirclePlus] (c_))) -> (a \[CirclePlus] c) \[CirclePlus] 
           b, "Side" -> 1, "Subpattern" -> (b_) \[CirclePlus] 
          ((a_) \[CirclePlus] (c_)), "MatchingConstruct" -> 
         {"CriticalPairLemma", 36}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CirclePlus] (b_)) \[CirclePlus] 
           ((c_) \[CirclePlus] (b_)) -> (a \[CirclePlus] b) \[CirclePlus] c, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 35} -> 
     <|"Statement" -> HoldForm[(a \[CirclePlus] c) \[CirclePlus] 
          (b \[CirclePlus] c) == a \[CirclePlus] (b \[CirclePlus] c)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 37}, 
        "Construct" -> {"SubstitutionLemma", 17}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] ((b_) \[CirclePlus] (a_)) -> 
          a \[CirclePlus] b, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[(a \[CirclePlus] c) \[CirclePlus] (b \[CirclePlus] c) == 
           a \[CirclePlus] (b \[CirclePlus] c)], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 36} -> 
     <|"Statement" -> HoldForm[(a \[CirclePlus] c) \[CirclePlus] b == 
         a \[CirclePlus] (b \[CirclePlus] c)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 35}, 
        "Construct" -> {"CriticalPairLemma", 36}, "Position" -> {}, 
        "Rule" -> ((a_) \[CirclePlus] (b_)) \[CirclePlus] ((c_) \[CirclePlus] 
            (b_)) -> (a \[CirclePlus] b) \[CirclePlus] c, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (a \[CirclePlus] c) \[CirclePlus] b == a \[CirclePlus] 
            (b \[CirclePlus] c)], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 37} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] (c \[CirclePlus] b) == 
         (b \[CirclePlus] a) \[CirclePlus] c], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 34}, 
        "Construct" -> {"SubstitutionLemma", 36}, "Position" -> {}, 
        "Rule" -> ((a_) \[CirclePlus] (b_)) \[CirclePlus] (c_) -> 
          a \[CirclePlus] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CirclePlus] (c \[CirclePlus] b) == 
           (b \[CirclePlus] a) \[CirclePlus] c], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 38} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] (c \[CirclePlus] b) == 
         b \[CirclePlus] (c \[CirclePlus] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 37}, 
        "Construct" -> {"SubstitutionLemma", 36}, "Position" -> {}, 
        "Rule" -> ((a_) \[CirclePlus] (b_)) \[CirclePlus] (c_) -> 
          a \[CirclePlus] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CirclePlus] (c \[CirclePlus] b) == 
           b \[CirclePlus] (c \[CirclePlus] a)], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 39} -> 
     <|"Statement" -> HoldForm[c \[CirclePlus] (a \[CirclePlus] b) == 
         c \[CirclePlus] (b \[CirclePlus] (a \[CirclePlus] b))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 27}, 
        "Construct" -> {"SubstitutionLemma", 38}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] ((b_) \[CirclePlus] (c_)) -> 
          c \[CirclePlus] (b \[CirclePlus] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[c \[CirclePlus] (a \[CirclePlus] b) == 
           c \[CirclePlus] (b \[CirclePlus] (a \[CirclePlus] b))], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 40} -> 
     <|"Statement" -> HoldForm[c \[CirclePlus] (a \[CirclePlus] b) == 
         c \[CirclePlus] (b \[CirclePlus] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 39}, 
        "Construct" -> {"SubstitutionLemma", 17}, "Position" -> {2}, 
        "Rule" -> (a_) \[CirclePlus] ((b_) \[CirclePlus] (a_)) -> 
          a \[CirclePlus] b, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[c \[CirclePlus] (a \[CirclePlus] b) == c \[CirclePlus] 
            (b \[CirclePlus] a)], "Source" -> "cpl"|>|>, 
    {"CriticalPairLemma", 38} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] 
          (b \[CirclePlus] a) \[CircleTimes] c == 
         (a \[CirclePlus] b) \[CircleTimes] (a \[CirclePlus] c)], 
      "Proof" -> <|"Construct" -> {"Axiom", 6}, "Orientation" -> -1, 
        "Rule" -> ((a_) \[CirclePlus] (b_)) \[CircleTimes] 
           ((a_) \[CirclePlus] (c_)) -> a \[CirclePlus] b \[CircleTimes] c, 
        "Side" -> 1, "Subpattern" -> (a_) \[CirclePlus] (b_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 17}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CirclePlus] ((b_) \[CirclePlus] (a_)) -> a \[CirclePlus] b, 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"SubstitutionLemma", 44} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] 
          (b \[CirclePlus] a) \[CircleTimes] c == a \[CirclePlus] 
          b \[CircleTimes] c], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 38}, "Construct" -> {"Axiom", 6}, 
        "Position" -> {}, "Rule" -> ((a_) \[CirclePlus] (b_)) \[CircleTimes] 
           ((a_) \[CirclePlus] (c_)) -> a \[CirclePlus] b \[CircleTimes] c, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CirclePlus] (b \[CirclePlus] a) \[CircleTimes] c == 
           a \[CirclePlus] b \[CircleTimes] c], "Source" -> "cpl"|>|>, 
    {"CriticalPairLemma", 39} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] c \[CircleTimes] b == 
         a \[CirclePlus] b \[CircleTimes] (c \[CirclePlus] a)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 44}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CirclePlus] 
           ((b_) \[CirclePlus] (a_)) \[CircleTimes] (c_) -> 
          a \[CirclePlus] b \[CircleTimes] c, "Side" -> 1, 
        "Subpattern" -> ((b_) \[CirclePlus] (a_)) \[CircleTimes] (c_), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 45} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] c \[CircleTimes] b == 
         a \[CirclePlus] b \[CircleTimes] c], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 39}, 
        "Construct" -> {"SubstitutionLemma", 20}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] (b_) \[CircleTimes] ((c_) \[CirclePlus] 
             (a_)) -> a \[CirclePlus] b \[CircleTimes] c, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CirclePlus] c \[CircleTimes] b == 
           a \[CirclePlus] b \[CircleTimes] c], "Source" -> "cpl"|>|>, 
    {"CriticalPairLemma", 40} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CirclePlus] 
           (c \[CirclePlus] a)) == a \[CircleTimes] b \[CirclePlus] a], 
      "Proof" -> <|"Construct" -> {"Axiom", 2}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CircleTimes] (b_) \[CirclePlus] (a_) \[CircleTimes] 
            (c_) -> a \[CircleTimes] (b \[CirclePlus] c), "Side" -> 1, 
        "Subpattern" -> (a_) \[CircleTimes] (c_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 26}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] (a_)) -> a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 85} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CirclePlus] 
           (c \[CirclePlus] a)) == a \[CirclePlus] a \[CircleTimes] b], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 40}, 
        "Construct" -> {"Axiom", 5}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CircleTimes] (b \[CirclePlus] (c \[CirclePlus] a)) == 
           a \[CirclePlus] a \[CircleTimes] b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 86} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CirclePlus] 
           (c \[CirclePlus] a)) == a], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 85}, "Construct" -> 
         {"SubstitutionLemma", 29}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] (a_) \[CircleTimes] (b_) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CircleTimes] (b \[CirclePlus] (c \[CirclePlus] a)) == a], 
        "Source" -> "cpl"|>|>, {"CriticalPairLemma", 41} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] (b \[CirclePlus] c) == 
         (a \[CirclePlus] (b \[CirclePlus] c)) \[CirclePlus] c], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 28}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CirclePlus] (b_) \[CircleTimes] 
            (a_) -> a, "Side" -> 1, "Subpattern" -> (b_) \[CircleTimes] (a_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 86}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] ((b_) \[CirclePlus] ((c_) \[CirclePlus] 
             (a_))) -> a, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 87} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] (b \[CirclePlus] c) == 
         c \[CirclePlus] (a \[CirclePlus] (b \[CirclePlus] c))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 41}, 
        "Construct" -> {"Axiom", 5}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CirclePlus] (b \[CirclePlus] c) == c \[CirclePlus] 
            (a \[CirclePlus] (b \[CirclePlus] c))], "Source" -> "cpl"|>|>, 
    {"CriticalPairLemma", 42} -> 
     <|"Statement" -> HoldForm[b \[CirclePlus] (c \[CirclePlus] a) == 
         a \[CirclePlus] (b \[CirclePlus] (a \[CirclePlus] c))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 87}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CirclePlus] ((b_) \[CirclePlus] 
            ((c_) \[CirclePlus] (a_))) -> b \[CirclePlus] (c \[CirclePlus] 
            a), "Side" -> 1, "Subpattern" -> (c_) \[CirclePlus] (a_), 
        "MatchingConstruct" -> {"Axiom", 5}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"SubstitutionLemma", 88} -> 
     <|"Statement" -> HoldForm[b \[CirclePlus] (c \[CirclePlus] a) == 
         (a \[CirclePlus] c) \[CirclePlus] b], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 42}, 
        "Construct" -> {"CriticalPairLemma", 35}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] ((b_) \[CirclePlus] ((a_) \[CirclePlus] 
             (c_))) -> (a \[CirclePlus] c) \[CirclePlus] b, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          b \[CirclePlus] (c \[CirclePlus] a) == 
           (a \[CirclePlus] c) \[CirclePlus] b], "Source" -> "cpl"|>|>, 
    {"CriticalPairLemma", 43} -> 
     <|"Statement" -> HoldForm[(a \[CirclePlus] b) \[CirclePlus] c == 
         (a \[CirclePlus] b) \[CirclePlus] (c \[CirclePlus] a)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 23}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CirclePlus] ((b_) \[CirclePlus] 
            (c_) \[CircleTimes] (a_)) -> a \[CirclePlus] b, "Side" -> 1, 
        "Subpattern" -> (c_) \[CircleTimes] (a_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 22}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CircleTimes] ((a_) \[CirclePlus] (b_)) -> a, 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"SubstitutionLemma", 94} -> 
     <|"Statement" -> HoldForm[(a \[CirclePlus] b) \[CirclePlus] c == 
         a \[CirclePlus] ((c \[CirclePlus] a) \[CirclePlus] b)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 43}, 
        "Construct" -> {"SubstitutionLemma", 36}, "Position" -> {}, 
        "Rule" -> ((a_) \[CirclePlus] (b_)) \[CirclePlus] (c_) -> 
          a \[CirclePlus] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[(a \[CirclePlus] b) \[CirclePlus] c == 
           a \[CirclePlus] ((c \[CirclePlus] a) \[CirclePlus] b)], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 95} -> 
     <|"Statement" -> HoldForm[(a \[CirclePlus] b) \[CirclePlus] c == 
         a \[CirclePlus] (c \[CirclePlus] (b \[CirclePlus] a))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 94}, 
        "Construct" -> {"SubstitutionLemma", 36}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CirclePlus] (b_)) \[CirclePlus] (c_) -> 
          a \[CirclePlus] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[(a \[CirclePlus] b) \[CirclePlus] c == 
           a \[CirclePlus] (c \[CirclePlus] (b \[CirclePlus] a))], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 96} -> 
     <|"Statement" -> HoldForm[(a \[CirclePlus] b) \[CirclePlus] c == 
         c \[CirclePlus] (b \[CirclePlus] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 95}, 
        "Construct" -> {"SubstitutionLemma", 87}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] ((b_) \[CirclePlus] ((c_) \[CirclePlus] 
             (a_))) -> b \[CirclePlus] (c \[CirclePlus] a), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (a \[CirclePlus] b) \[CirclePlus] c == c \[CirclePlus] 
            (b \[CirclePlus] a)], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 97} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] (c \[CirclePlus] b) == 
         c \[CirclePlus] (b \[CirclePlus] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 96}, 
        "Construct" -> {"SubstitutionLemma", 36}, "Position" -> {}, 
        "Rule" -> ((a_) \[CirclePlus] (b_)) \[CirclePlus] (c_) -> 
          a \[CirclePlus] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CirclePlus] (c \[CirclePlus] b) == 
           c \[CirclePlus] (b \[CirclePlus] a)], "Source" -> "cpl"|>|>, 
    {"CriticalPairLemma", 44} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] b == 
         OverBar[a] \[CirclePlus] a \[CircleTimes] b], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 2}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CirclePlus] 
           OverBar[a_] \[CircleTimes] (b_) -> a \[CirclePlus] b, "Side" -> 1, 
        "Subpattern" -> OverBar[a_], "MatchingConstruct" -> 
         {"SubstitutionLemma", 7}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> OverBar[OverBar[a_]] -> a, "MatchingSide" -> 1, 
        "Position" -> {2, 1}|>|>, {"CriticalPairLemma", 45} -> 
     <|"Statement" -> HoldForm[b \[CirclePlus] OverBar[b] == 
         a \[CirclePlus] OverBar[a]], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 1}, "Orientation" -> -1, 
        "Rule" -> ((a_) \[CirclePlus] OverBar[a_]) \[CircleTimes] (b_) -> b, 
        "Side" -> 1, "Subpattern" -> {}, "MatchingConstruct" -> {"Axiom", 4}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] ((b_) \[CirclePlus] OverBar[b_]) -> a, 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"CriticalPairLemma", 46} -> 
     <|"Statement" -> HoldForm[b \[CirclePlus] OverBar[b] == 
         a \[CirclePlus] (b \[CirclePlus] OverBar[b])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 26}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] 
            (a_)) -> a, "Side" -> 1, "Subpattern" -> {}, 
        "MatchingConstruct" -> {"CriticalPairLemma", 1}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CirclePlus] OverBar[a_]) \[CircleTimes] (b_) -> b, 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         OverBar[b] \[CirclePlus] OverBar[OverBar[b]] \[CircleTimes] b], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Construct" -> 
         {"CriticalPairLemma", 2}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] (b_) -> a \[CirclePlus] 
           OverBar[a] \[CircleTimes] b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CirclePlus] a == 
           OverBar[b] \[CirclePlus] OverBar[OverBar[b]] \[CircleTimes] b], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 3} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         (OverBar[b] \[CirclePlus] OverBar[OverBar[b]] \[CircleTimes] 
            b) \[CircleTimes] ((OverBar[b] \[CirclePlus] 
            OverBar[OverBar[b]] \[CircleTimes] b) \[CirclePlus] 
           OverBar[OverBar[OverBar[b] \[CirclePlus] OverBar[OverBar[
                 b]] \[CircleTimes] b]])], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 1}, "Construct" -> 
         {"SubstitutionLemma", 2}, "Position" -> {}, 
        "Rule" -> a_ -> a \[CircleTimes] (a \[CirclePlus] 
            OverBar[OverBar[a]]), "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[OverBar[a] \[CirclePlus] a == (OverBar[b] \[CirclePlus] 
             OverBar[OverBar[b]] \[CircleTimes] b) \[CircleTimes] 
            ((OverBar[b] \[CirclePlus] OverBar[OverBar[b]] \[CircleTimes] 
               b) \[CirclePlus] OverBar[OverBar[OverBar[b] \[CirclePlus] 
                OverBar[OverBar[b]] \[CircleTimes] b]])], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 4} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         ((OverBar[b] \[CirclePlus] OverBar[OverBar[b]] \[CircleTimes] 
             b) \[CirclePlus] OverBar[OverBar[OverBar[b] \[CirclePlus] 
              OverBar[OverBar[b]] \[CircleTimes] b]]) \[CircleTimes] 
          (OverBar[b] \[CirclePlus] OverBar[OverBar[b]] \[CircleTimes] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 3}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a] \[CirclePlus] a == ((OverBar[b] \[CirclePlus] 
              OverBar[OverBar[b]] \[CircleTimes] b) \[CirclePlus] 
             OverBar[OverBar[OverBar[b] \[CirclePlus] OverBar[OverBar[
                   b]] \[CircleTimes] b]]) \[CircleTimes] 
            (OverBar[b] \[CirclePlus] OverBar[OverBar[b]] \[CircleTimes] b)], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 8} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         ((OverBar[b] \[CirclePlus] OverBar[OverBar[b]] \[CircleTimes] 
             b) \[CirclePlus] OverBar[OverBar[OverBar[b] \[CirclePlus] 
              OverBar[OverBar[b]]]]) \[CircleTimes] (OverBar[b] \[CirclePlus] 
           OverBar[OverBar[b]] \[CircleTimes] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 4}, 
        "Construct" -> {"CriticalPairLemma", 14}, "Position" -> {1, 2, 1, 1}, 
        "Rule" -> OverBar[a_] \[CirclePlus] (b_) \[CircleTimes] (a_) -> 
          OverBar[a] \[CirclePlus] b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CirclePlus] a == 
           ((OverBar[b] \[CirclePlus] OverBar[OverBar[b]] \[CircleTimes] 
               b) \[CirclePlus] OverBar[OverBar[OverBar[b] \[CirclePlus] 
                OverBar[OverBar[b]]]]) \[CircleTimes] 
            (OverBar[b] \[CirclePlus] OverBar[OverBar[b]] \[CircleTimes] b)], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 13} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         ((OverBar[b] \[CirclePlus] OverBar[OverBar[b]] \[CircleTimes] 
             b) \[CirclePlus] OverBar[OverBar[OverBar[b] \[CirclePlus] 
              OverBar[OverBar[b]]]]) \[CircleTimes] 
          (OverBar[OverBar[b]] \[CircleTimes] b \[CirclePlus] OverBar[b])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 8}, 
        "Construct" -> {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] (c_)) -> 
          a \[CircleTimes] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CirclePlus] a == 
           ((OverBar[b] \[CirclePlus] OverBar[OverBar[b]] \[CircleTimes] 
               b) \[CirclePlus] OverBar[OverBar[OverBar[b] \[CirclePlus] 
                OverBar[OverBar[b]]]]) \[CircleTimes] 
            (OverBar[OverBar[b]] \[CircleTimes] b \[CirclePlus] OverBar[b])], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 14} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         (OverBar[OverBar[b]] \[CircleTimes] b \[CirclePlus] 
           OverBar[b]) \[CircleTimes] ((OverBar[b] \[CirclePlus] 
            OverBar[OverBar[b]] \[CircleTimes] b) \[CirclePlus] 
           OverBar[OverBar[OverBar[b] \[CirclePlus] OverBar[OverBar[b]]]])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 13}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a] \[CirclePlus] a == (OverBar[OverBar[b]] \[CircleTimes] 
              b \[CirclePlus] OverBar[b]) \[CircleTimes] 
            ((OverBar[b] \[CirclePlus] OverBar[OverBar[b]] \[CircleTimes] 
               b) \[CirclePlus] OverBar[OverBar[OverBar[b] \[CirclePlus] 
                OverBar[OverBar[b]]]])], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 15} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         (OverBar[OverBar[b]] \[CircleTimes] b \[CirclePlus] 
           OverBar[b]) \[CircleTimes] 
          (OverBar[OverBar[OverBar[b] \[CirclePlus] OverBar[OverBar[
                b]]]] \[CirclePlus] (OverBar[b] \[CirclePlus] 
            OverBar[OverBar[b]] \[CircleTimes] b))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 14}, 
        "Construct" -> {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] (c_)) -> 
          a \[CircleTimes] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CirclePlus] a == 
           (OverBar[OverBar[b]] \[CircleTimes] b \[CirclePlus] 
             OverBar[b]) \[CircleTimes] (OverBar[OverBar[OverBar[
                 b] \[CirclePlus] OverBar[OverBar[b]]]] \[CirclePlus] 
             (OverBar[b] \[CirclePlus] OverBar[OverBar[b]] \[CircleTimes] 
               b))], "Source" -> "cpl"|>|>, {"SubstitutionLemma", 41} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         (OverBar[OverBar[b]] \[CircleTimes] b \[CirclePlus] 
           OverBar[b]) \[CircleTimes] 
          (OverBar[OverBar[OverBar[b] \[CirclePlus] OverBar[OverBar[
                b]]]] \[CirclePlus] (OverBar[OverBar[b]] \[CircleTimes] 
             b \[CirclePlus] OverBar[b]))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 15}, "Construct" -> 
         {"SubstitutionLemma", 40}, "Position" -> {2}, 
        "Rule" -> (a_) \[CirclePlus] ((b_) \[CirclePlus] (c_)) -> 
          a \[CirclePlus] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CirclePlus] a == 
           (OverBar[OverBar[b]] \[CircleTimes] b \[CirclePlus] 
             OverBar[b]) \[CircleTimes] (OverBar[OverBar[OverBar[
                 b] \[CirclePlus] OverBar[OverBar[b]]]] \[CirclePlus] 
             (OverBar[OverBar[b]] \[CircleTimes] b \[CirclePlus] 
              OverBar[b]))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 42} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         (OverBar[OverBar[OverBar[b] \[CirclePlus] OverBar[OverBar[
                b]]]] \[CirclePlus] (OverBar[OverBar[b]] \[CircleTimes] 
             b \[CirclePlus] OverBar[b])) \[CircleTimes] 
          (OverBar[OverBar[b]] \[CircleTimes] b \[CirclePlus] OverBar[b])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 41}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a] \[CirclePlus] a == 
           (OverBar[OverBar[OverBar[b] \[CirclePlus] OverBar[OverBar[
                  b]]]] \[CirclePlus] (OverBar[OverBar[b]] \[CircleTimes] 
               b \[CirclePlus] OverBar[b])) \[CircleTimes] 
            (OverBar[OverBar[b]] \[CircleTimes] b \[CirclePlus] OverBar[b])], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 43} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         (OverBar[OverBar[OverBar[b] \[CirclePlus] OverBar[OverBar[
                b]]]] \[CirclePlus] (OverBar[OverBar[b]] \[CircleTimes] 
             b \[CirclePlus] OverBar[b])) \[CircleTimes] 
          (OverBar[b] \[CirclePlus] OverBar[OverBar[b]] \[CircleTimes] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 42}, 
        "Construct" -> {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] (c_)) -> 
          a \[CircleTimes] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CirclePlus] a == 
           (OverBar[OverBar[OverBar[b] \[CirclePlus] OverBar[OverBar[
                  b]]]] \[CirclePlus] (OverBar[OverBar[b]] \[CircleTimes] 
               b \[CirclePlus] OverBar[b])) \[CircleTimes] 
            (OverBar[b] \[CirclePlus] OverBar[OverBar[b]] \[CircleTimes] b)], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 46} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         (OverBar[OverBar[OverBar[b] \[CirclePlus] OverBar[OverBar[
                b]]]] \[CirclePlus] (OverBar[OverBar[b]] \[CircleTimes] 
             b \[CirclePlus] OverBar[b])) \[CircleTimes] 
          (OverBar[b] \[CirclePlus] b \[CircleTimes] OverBar[OverBar[b]])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 43}, 
        "Construct" -> {"SubstitutionLemma", 45}, "Position" -> {2}, 
        "Rule" -> (a_) \[CirclePlus] (b_) \[CircleTimes] (c_) -> 
          a \[CirclePlus] c \[CircleTimes] b, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CirclePlus] a == 
           (OverBar[OverBar[OverBar[b] \[CirclePlus] OverBar[OverBar[
                  b]]]] \[CirclePlus] (OverBar[OverBar[b]] \[CircleTimes] 
               b \[CirclePlus] OverBar[b])) \[CircleTimes] 
            (OverBar[b] \[CirclePlus] b \[CircleTimes] OverBar[OverBar[b]])], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 47} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         (OverBar[b] \[CirclePlus] b \[CircleTimes] 
            OverBar[OverBar[b]]) \[CircleTimes] 
          (OverBar[OverBar[OverBar[b] \[CirclePlus] OverBar[OverBar[
                b]]]] \[CirclePlus] (OverBar[OverBar[b]] \[CircleTimes] 
             b \[CirclePlus] OverBar[b]))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 46}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {}, "Rule" -> (a_) \[CircleTimes] (b_) -> 
          b \[CircleTimes] a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[OverBar[a] \[CirclePlus] a == (OverBar[b] \[CirclePlus] 
             b \[CircleTimes] OverBar[OverBar[b]]) \[CircleTimes] 
            (OverBar[OverBar[OverBar[b] \[CirclePlus] OverBar[OverBar[
                  b]]]] \[CirclePlus] (OverBar[OverBar[b]] \[CircleTimes] 
               b \[CirclePlus] OverBar[b]))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 48} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         (OverBar[b] \[CirclePlus] b \[CircleTimes] 
            OverBar[OverBar[b]]) \[CircleTimes] 
          ((OverBar[OverBar[b]] \[CircleTimes] b \[CirclePlus] 
            OverBar[b]) \[CirclePlus] OverBar[OverBar[
             OverBar[b] \[CirclePlus] OverBar[OverBar[b]]]])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 47}, 
        "Construct" -> {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] (c_)) -> 
          a \[CircleTimes] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CirclePlus] a == 
           (OverBar[b] \[CirclePlus] b \[CircleTimes] OverBar[OverBar[
                b]]) \[CircleTimes] ((OverBar[OverBar[b]] \[CircleTimes] 
               b \[CirclePlus] OverBar[b]) \[CirclePlus] 
             OverBar[OverBar[OverBar[b] \[CirclePlus] OverBar[OverBar[
                  b]]]])], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 49} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         (OverBar[b] \[CirclePlus] b \[CircleTimes] 
            OverBar[OverBar[b]]) \[CircleTimes] 
          ((OverBar[OverBar[b]] \[CircleTimes] b \[CirclePlus] 
            OverBar[b]) \[CirclePlus] (OverBar[b] \[CirclePlus] 
            OverBar[OverBar[b]]))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 48}, "Construct" -> 
         {"SubstitutionLemma", 7}, "Position" -> {2, 2}, 
        "Rule" -> OverBar[OverBar[a_]] -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CirclePlus] a == 
           (OverBar[b] \[CirclePlus] b \[CircleTimes] OverBar[OverBar[
                b]]) \[CircleTimes] ((OverBar[OverBar[b]] \[CircleTimes] 
               b \[CirclePlus] OverBar[b]) \[CirclePlus] 
             (OverBar[b] \[CirclePlus] OverBar[OverBar[b]]))], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 50} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         ((OverBar[OverBar[b]] \[CircleTimes] b \[CirclePlus] 
            OverBar[b]) \[CirclePlus] (OverBar[b] \[CirclePlus] 
            OverBar[OverBar[b]])) \[CircleTimes] (OverBar[b] \[CirclePlus] 
           b \[CircleTimes] OverBar[OverBar[b]])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 49}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a] \[CirclePlus] a == ((OverBar[OverBar[b]] \[CircleTimes] 
               b \[CirclePlus] OverBar[b]) \[CirclePlus] 
             (OverBar[b] \[CirclePlus] OverBar[OverBar[b]])) \[CircleTimes] 
            (OverBar[b] \[CirclePlus] b \[CircleTimes] OverBar[OverBar[b]])], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 51} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         ((OverBar[OverBar[b]] \[CircleTimes] b \[CirclePlus] 
            OverBar[b]) \[CirclePlus] (OverBar[b] \[CirclePlus] 
            OverBar[OverBar[b]])) \[CircleTimes] 
          (b \[CircleTimes] OverBar[OverBar[b]] \[CirclePlus] OverBar[b])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 50}, 
        "Construct" -> {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] (c_)) -> 
          a \[CircleTimes] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CirclePlus] a == 
           ((OverBar[OverBar[b]] \[CircleTimes] b \[CirclePlus] 
              OverBar[b]) \[CirclePlus] (OverBar[b] \[CirclePlus] 
              OverBar[OverBar[b]])) \[CircleTimes] 
            (b \[CircleTimes] OverBar[OverBar[b]] \[CirclePlus] OverBar[b])], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 52} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         ((OverBar[OverBar[b]] \[CircleTimes] b \[CirclePlus] 
            OverBar[b]) \[CirclePlus] (OverBar[b] \[CirclePlus] 
            OverBar[OverBar[b]])) \[CircleTimes] 
          (OverBar[OverBar[b]] \[CircleTimes] b \[CirclePlus] OverBar[b])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 51}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {2, 1}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a] \[CirclePlus] a == ((OverBar[OverBar[b]] \[CircleTimes] 
               b \[CirclePlus] OverBar[b]) \[CirclePlus] 
             (OverBar[b] \[CirclePlus] OverBar[OverBar[b]])) \[CircleTimes] 
            (OverBar[OverBar[b]] \[CircleTimes] b \[CirclePlus] OverBar[b])], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 53} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         (OverBar[OverBar[b]] \[CircleTimes] b \[CirclePlus] 
           OverBar[b]) \[CircleTimes] ((OverBar[OverBar[b]] \[CircleTimes] 
             b \[CirclePlus] OverBar[b]) \[CirclePlus] 
           (OverBar[b] \[CirclePlus] OverBar[OverBar[b]]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 52}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a] \[CirclePlus] a == (OverBar[OverBar[b]] \[CircleTimes] 
              b \[CirclePlus] OverBar[b]) \[CircleTimes] 
            ((OverBar[OverBar[b]] \[CircleTimes] b \[CirclePlus] 
              OverBar[b]) \[CirclePlus] (OverBar[b] \[CirclePlus] 
              OverBar[OverBar[b]]))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 54} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         (OverBar[OverBar[b]] \[CircleTimes] b \[CirclePlus] 
           OverBar[b]) \[CircleTimes] ((OverBar[OverBar[b]] \[CircleTimes] 
             b \[CirclePlus] OverBar[b]) \[CirclePlus] 
           (OverBar[OverBar[b]] \[CirclePlus] OverBar[b]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 53}, 
        "Construct" -> {"SubstitutionLemma", 40}, "Position" -> {2}, 
        "Rule" -> (a_) \[CirclePlus] ((b_) \[CirclePlus] (c_)) -> 
          a \[CirclePlus] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CirclePlus] a == 
           (OverBar[OverBar[b]] \[CircleTimes] b \[CirclePlus] 
             OverBar[b]) \[CircleTimes] ((OverBar[OverBar[b]] \[CircleTimes] 
               b \[CirclePlus] OverBar[b]) \[CirclePlus] 
             (OverBar[OverBar[b]] \[CirclePlus] OverBar[b]))], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 55} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         (OverBar[OverBar[b]] \[CircleTimes] b \[CirclePlus] 
           OverBar[b]) \[CircleTimes] ((OverBar[OverBar[b]] \[CirclePlus] 
            OverBar[b]) \[CirclePlus] (OverBar[OverBar[b]] \[CircleTimes] 
             b \[CirclePlus] OverBar[b]))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 54}, "Construct" -> 
         {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] (c_)) -> 
          a \[CircleTimes] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CirclePlus] a == 
           (OverBar[OverBar[b]] \[CircleTimes] b \[CirclePlus] 
             OverBar[b]) \[CircleTimes] ((OverBar[OverBar[b]] \[CirclePlus] 
              OverBar[b]) \[CirclePlus] (OverBar[OverBar[b]] \[CircleTimes] 
               b \[CirclePlus] OverBar[b]))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 56} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         ((OverBar[OverBar[b]] \[CirclePlus] OverBar[b]) \[CirclePlus] 
           (OverBar[OverBar[b]] \[CircleTimes] b \[CirclePlus] 
            OverBar[b])) \[CircleTimes] (OverBar[OverBar[b]] \[CircleTimes] 
            b \[CirclePlus] OverBar[b])], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 55}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {}, "Rule" -> (a_) \[CircleTimes] (b_) -> 
          b \[CircleTimes] a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[OverBar[a] \[CirclePlus] a == 
           ((OverBar[OverBar[b]] \[CirclePlus] OverBar[b]) \[CirclePlus] 
             (OverBar[OverBar[b]] \[CircleTimes] b \[CirclePlus] 
              OverBar[b])) \[CircleTimes] (OverBar[OverBar[b]] \[CircleTimes] 
              b \[CirclePlus] OverBar[b])], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 57} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         ((OverBar[OverBar[b]] \[CirclePlus] OverBar[b]) \[CirclePlus] 
           (OverBar[OverBar[b]] \[CircleTimes] b \[CirclePlus] 
            OverBar[b])) \[CircleTimes] (OverBar[b] \[CirclePlus] 
           OverBar[OverBar[b]] \[CircleTimes] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 56}, 
        "Construct" -> {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] (c_)) -> 
          a \[CircleTimes] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CirclePlus] a == 
           ((OverBar[OverBar[b]] \[CirclePlus] OverBar[b]) \[CirclePlus] 
             (OverBar[OverBar[b]] \[CircleTimes] b \[CirclePlus] 
              OverBar[b])) \[CircleTimes] (OverBar[b] \[CirclePlus] 
             OverBar[OverBar[b]] \[CircleTimes] b)], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 58} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         ((OverBar[OverBar[b]] \[CirclePlus] OverBar[b]) \[CirclePlus] 
           (OverBar[OverBar[b]] \[CircleTimes] b \[CirclePlus] 
            OverBar[b])) \[CircleTimes] (OverBar[b] \[CirclePlus] 
           b \[CircleTimes] OverBar[OverBar[b]])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 57}, 
        "Construct" -> {"SubstitutionLemma", 45}, "Position" -> {2}, 
        "Rule" -> (a_) \[CirclePlus] (b_) \[CircleTimes] (c_) -> 
          a \[CirclePlus] c \[CircleTimes] b, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CirclePlus] a == 
           ((OverBar[OverBar[b]] \[CirclePlus] OverBar[b]) \[CirclePlus] 
             (OverBar[OverBar[b]] \[CircleTimes] b \[CirclePlus] 
              OverBar[b])) \[CircleTimes] (OverBar[b] \[CirclePlus] 
             b \[CircleTimes] OverBar[OverBar[b]])], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 59} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         (OverBar[b] \[CirclePlus] b \[CircleTimes] 
            OverBar[OverBar[b]]) \[CircleTimes] 
          ((OverBar[OverBar[b]] \[CirclePlus] OverBar[b]) \[CirclePlus] 
           (OverBar[OverBar[b]] \[CircleTimes] b \[CirclePlus] OverBar[b]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 58}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a] \[CirclePlus] a == (OverBar[b] \[CirclePlus] 
             b \[CircleTimes] OverBar[OverBar[b]]) \[CircleTimes] 
            ((OverBar[OverBar[b]] \[CirclePlus] OverBar[b]) \[CirclePlus] 
             (OverBar[OverBar[b]] \[CircleTimes] b \[CirclePlus] 
              OverBar[b]))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 60} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         (OverBar[b] \[CirclePlus] b \[CircleTimes] 
            OverBar[OverBar[b]]) \[CircleTimes] 
          ((OverBar[OverBar[b]] \[CirclePlus] OverBar[b]) \[CirclePlus] 
           (OverBar[b] \[CirclePlus] OverBar[OverBar[b]] \[CircleTimes] b))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 59}, 
        "Construct" -> {"SubstitutionLemma", 40}, "Position" -> {2}, 
        "Rule" -> (a_) \[CirclePlus] ((b_) \[CirclePlus] (c_)) -> 
          a \[CirclePlus] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CirclePlus] a == 
           (OverBar[b] \[CirclePlus] b \[CircleTimes] OverBar[OverBar[
                b]]) \[CircleTimes] ((OverBar[OverBar[b]] \[CirclePlus] 
              OverBar[b]) \[CirclePlus] (OverBar[b] \[CirclePlus] 
              OverBar[OverBar[b]] \[CircleTimes] b))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 61} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         (OverBar[b] \[CirclePlus] b \[CircleTimes] 
            OverBar[OverBar[b]]) \[CircleTimes] ((OverBar[b] \[CirclePlus] 
            OverBar[OverBar[b]] \[CircleTimes] b) \[CirclePlus] 
           (OverBar[OverBar[b]] \[CirclePlus] OverBar[b]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 60}, 
        "Construct" -> {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] (c_)) -> 
          a \[CircleTimes] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CirclePlus] a == 
           (OverBar[b] \[CirclePlus] b \[CircleTimes] OverBar[OverBar[
                b]]) \[CircleTimes] ((OverBar[b] \[CirclePlus] 
              OverBar[OverBar[b]] \[CircleTimes] b) \[CirclePlus] 
             (OverBar[OverBar[b]] \[CirclePlus] OverBar[b]))], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 62} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         ((OverBar[b] \[CirclePlus] OverBar[OverBar[b]] \[CircleTimes] 
             b) \[CirclePlus] (OverBar[OverBar[b]] \[CirclePlus] 
            OverBar[b])) \[CircleTimes] (OverBar[b] \[CirclePlus] 
           b \[CircleTimes] OverBar[OverBar[b]])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 61}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a] \[CirclePlus] a == ((OverBar[b] \[CirclePlus] 
              OverBar[OverBar[b]] \[CircleTimes] b) \[CirclePlus] 
             (OverBar[OverBar[b]] \[CirclePlus] OverBar[b])) \[CircleTimes] 
            (OverBar[b] \[CirclePlus] b \[CircleTimes] OverBar[OverBar[b]])], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 63} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         ((OverBar[b] \[CirclePlus] OverBar[OverBar[b]] \[CircleTimes] 
             b) \[CirclePlus] (OverBar[OverBar[b]] \[CirclePlus] 
            OverBar[b])) \[CircleTimes] (b \[CircleTimes] 
            OverBar[OverBar[b]] \[CirclePlus] OverBar[b])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 62}, 
        "Construct" -> {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] (c_)) -> 
          a \[CircleTimes] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CirclePlus] a == 
           ((OverBar[b] \[CirclePlus] OverBar[OverBar[b]] \[CircleTimes] 
               b) \[CirclePlus] (OverBar[OverBar[b]] \[CirclePlus] 
              OverBar[b])) \[CircleTimes] (b \[CircleTimes] OverBar[OverBar[
                b]] \[CirclePlus] OverBar[b])], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 64} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         ((OverBar[b] \[CirclePlus] OverBar[OverBar[b]] \[CircleTimes] 
             b) \[CirclePlus] (OverBar[OverBar[b]] \[CirclePlus] 
            OverBar[b])) \[CircleTimes] (OverBar[OverBar[b]] \[CircleTimes] 
            b \[CirclePlus] OverBar[b])], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 63}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {2, 1}, "Rule" -> (a_) \[CircleTimes] (b_) -> 
          b \[CircleTimes] a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[OverBar[a] \[CirclePlus] a == 
           ((OverBar[b] \[CirclePlus] OverBar[OverBar[b]] \[CircleTimes] 
               b) \[CirclePlus] (OverBar[OverBar[b]] \[CirclePlus] 
              OverBar[b])) \[CircleTimes] (OverBar[OverBar[b]] \[CircleTimes] 
              b \[CirclePlus] OverBar[b])], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 65} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         ((OverBar[b] \[CirclePlus] b \[CircleTimes] OverBar[
              OverBar[b]]) \[CirclePlus] (OverBar[OverBar[b]] \[CirclePlus] 
            OverBar[b])) \[CircleTimes] (OverBar[OverBar[b]] \[CircleTimes] 
            b \[CirclePlus] OverBar[b])], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 64}, "Construct" -> 
         {"SubstitutionLemma", 45}, "Position" -> {1, 1}, 
        "Rule" -> (a_) \[CirclePlus] (b_) \[CircleTimes] (c_) -> 
          a \[CirclePlus] c \[CircleTimes] b, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CirclePlus] a == 
           ((OverBar[b] \[CirclePlus] b \[CircleTimes] OverBar[OverBar[
                 b]]) \[CirclePlus] (OverBar[OverBar[b]] \[CirclePlus] 
              OverBar[b])) \[CircleTimes] (OverBar[OverBar[b]] \[CircleTimes] 
              b \[CirclePlus] OverBar[b])], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 66} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         (OverBar[OverBar[b]] \[CircleTimes] b \[CirclePlus] 
           OverBar[b]) \[CircleTimes] ((OverBar[b] \[CirclePlus] 
            b \[CircleTimes] OverBar[OverBar[b]]) \[CirclePlus] 
           (OverBar[OverBar[b]] \[CirclePlus] OverBar[b]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 65}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a] \[CirclePlus] a == (OverBar[OverBar[b]] \[CircleTimes] 
              b \[CirclePlus] OverBar[b]) \[CircleTimes] 
            ((OverBar[b] \[CirclePlus] b \[CircleTimes] OverBar[
                OverBar[b]]) \[CirclePlus] (OverBar[OverBar[b]] \[CirclePlus] 
              OverBar[b]))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 67} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         (OverBar[OverBar[b]] \[CircleTimes] b \[CirclePlus] 
           OverBar[b]) \[CircleTimes] ((OverBar[OverBar[b]] \[CirclePlus] 
            OverBar[b]) \[CirclePlus] (OverBar[b] \[CirclePlus] 
            b \[CircleTimes] OverBar[OverBar[b]]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 66}, 
        "Construct" -> {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] (c_)) -> 
          a \[CircleTimes] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CirclePlus] a == 
           (OverBar[OverBar[b]] \[CircleTimes] b \[CirclePlus] 
             OverBar[b]) \[CircleTimes] ((OverBar[OverBar[b]] \[CirclePlus] 
              OverBar[b]) \[CirclePlus] (OverBar[b] \[CirclePlus] 
              b \[CircleTimes] OverBar[OverBar[b]]))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 68} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         (OverBar[OverBar[b]] \[CircleTimes] b \[CirclePlus] 
           OverBar[b]) \[CircleTimes] ((OverBar[b] \[CirclePlus] 
            OverBar[OverBar[b]]) \[CirclePlus] (OverBar[b] \[CirclePlus] 
            b \[CircleTimes] OverBar[OverBar[b]]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 67}, 
        "Construct" -> {"SubstitutionLemma", 34}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CirclePlus] (b_)) \[CirclePlus] (c_) -> 
          (b \[CirclePlus] a) \[CirclePlus] c, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CirclePlus] a == 
           (OverBar[OverBar[b]] \[CircleTimes] b \[CirclePlus] 
             OverBar[b]) \[CircleTimes] ((OverBar[b] \[CirclePlus] 
              OverBar[OverBar[b]]) \[CirclePlus] (OverBar[b] \[CirclePlus] 
              b \[CircleTimes] OverBar[OverBar[b]]))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 69} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         ((OverBar[b] \[CirclePlus] OverBar[OverBar[b]]) \[CirclePlus] 
           (OverBar[b] \[CirclePlus] b \[CircleTimes] 
             OverBar[OverBar[b]])) \[CircleTimes] 
          (OverBar[OverBar[b]] \[CircleTimes] b \[CirclePlus] OverBar[b])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 68}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a] \[CirclePlus] a == ((OverBar[b] \[CirclePlus] 
              OverBar[OverBar[b]]) \[CirclePlus] (OverBar[b] \[CirclePlus] 
              b \[CircleTimes] OverBar[OverBar[b]])) \[CircleTimes] 
            (OverBar[OverBar[b]] \[CircleTimes] b \[CirclePlus] OverBar[b])], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 70} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         ((OverBar[b] \[CirclePlus] OverBar[OverBar[b]]) \[CirclePlus] 
           (OverBar[b] \[CirclePlus] b \[CircleTimes] 
             OverBar[OverBar[b]])) \[CircleTimes] (OverBar[b] \[CirclePlus] 
           OverBar[OverBar[b]] \[CircleTimes] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 69}, 
        "Construct" -> {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] (c_)) -> 
          a \[CircleTimes] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CirclePlus] a == 
           ((OverBar[b] \[CirclePlus] OverBar[OverBar[b]]) \[CirclePlus] 
             (OverBar[b] \[CirclePlus] b \[CircleTimes] OverBar[
                OverBar[b]])) \[CircleTimes] (OverBar[b] \[CirclePlus] 
             OverBar[OverBar[b]] \[CircleTimes] b)], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 71} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         ((OverBar[b] \[CirclePlus] OverBar[OverBar[b]]) \[CirclePlus] 
           (OverBar[b] \[CirclePlus] b \[CircleTimes] 
             OverBar[OverBar[b]])) \[CircleTimes] (OverBar[b] \[CirclePlus] 
           b \[CircleTimes] OverBar[OverBar[b]])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 70}, 
        "Construct" -> {"SubstitutionLemma", 45}, "Position" -> {2}, 
        "Rule" -> (a_) \[CirclePlus] (b_) \[CircleTimes] (c_) -> 
          a \[CirclePlus] c \[CircleTimes] b, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CirclePlus] a == 
           ((OverBar[b] \[CirclePlus] OverBar[OverBar[b]]) \[CirclePlus] 
             (OverBar[b] \[CirclePlus] b \[CircleTimes] OverBar[
                OverBar[b]])) \[CircleTimes] (OverBar[b] \[CirclePlus] 
             b \[CircleTimes] OverBar[OverBar[b]])], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 72} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         (OverBar[b] \[CirclePlus] b \[CircleTimes] 
            OverBar[OverBar[b]]) \[CircleTimes] ((OverBar[b] \[CirclePlus] 
            OverBar[OverBar[b]]) \[CirclePlus] (OverBar[b] \[CirclePlus] 
            b \[CircleTimes] OverBar[OverBar[b]]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 71}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a] \[CirclePlus] a == (OverBar[b] \[CirclePlus] 
             b \[CircleTimes] OverBar[OverBar[b]]) \[CircleTimes] 
            ((OverBar[b] \[CirclePlus] OverBar[OverBar[b]]) \[CirclePlus] 
             (OverBar[b] \[CirclePlus] b \[CircleTimes] OverBar[
                OverBar[b]]))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 73} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         (OverBar[b] \[CirclePlus] b \[CircleTimes] 
            OverBar[OverBar[b]]) \[CircleTimes] ((OverBar[b] \[CirclePlus] 
            b \[CircleTimes] OverBar[OverBar[b]]) \[CirclePlus] 
           (OverBar[b] \[CirclePlus] OverBar[OverBar[b]]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 72}, 
        "Construct" -> {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] (c_)) -> 
          a \[CircleTimes] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CirclePlus] a == 
           (OverBar[b] \[CirclePlus] b \[CircleTimes] OverBar[OverBar[
                b]]) \[CircleTimes] ((OverBar[b] \[CirclePlus] 
              b \[CircleTimes] OverBar[OverBar[b]]) \[CirclePlus] 
             (OverBar[b] \[CirclePlus] OverBar[OverBar[b]]))], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 74} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         (b \[CircleTimes] OverBar[OverBar[b]] \[CirclePlus] 
           OverBar[b]) \[CircleTimes] ((OverBar[b] \[CirclePlus] 
            b \[CircleTimes] OverBar[OverBar[b]]) \[CirclePlus] 
           (OverBar[b] \[CirclePlus] OverBar[OverBar[b]]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 73}, 
        "Construct" -> {"Axiom", 5}, "Position" -> {1}, 
        "Rule" -> (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a] \[CirclePlus] a == (b \[CircleTimes] OverBar[OverBar[
                b]] \[CirclePlus] OverBar[b]) \[CircleTimes] 
            ((OverBar[b] \[CirclePlus] b \[CircleTimes] OverBar[
                OverBar[b]]) \[CirclePlus] (OverBar[b] \[CirclePlus] 
              OverBar[OverBar[b]]))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 75} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         (b \[CircleTimes] OverBar[OverBar[b]] \[CirclePlus] 
           OverBar[b]) \[CircleTimes] 
          ((b \[CircleTimes] OverBar[OverBar[b]] \[CirclePlus] 
            OverBar[b]) \[CirclePlus] (OverBar[b] \[CirclePlus] 
            OverBar[OverBar[b]]))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 74}, "Construct" -> 
         {"SubstitutionLemma", 34}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CirclePlus] (b_)) \[CirclePlus] (c_) -> 
          (b \[CirclePlus] a) \[CirclePlus] c, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CirclePlus] a == 
           (b \[CircleTimes] OverBar[OverBar[b]] \[CirclePlus] 
             OverBar[b]) \[CircleTimes] ((b \[CircleTimes] OverBar[
                OverBar[b]] \[CirclePlus] OverBar[b]) \[CirclePlus] 
             (OverBar[b] \[CirclePlus] OverBar[OverBar[b]]))], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 76} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         (b \[CircleTimes] OverBar[OverBar[b]] \[CirclePlus] 
           OverBar[b]) \[CircleTimes] ((OverBar[b] \[CirclePlus] 
            OverBar[OverBar[b]]) \[CirclePlus] (b \[CircleTimes] 
             OverBar[OverBar[b]] \[CirclePlus] OverBar[b]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 75}, 
        "Construct" -> {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] (c_)) -> 
          a \[CircleTimes] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CirclePlus] a == 
           (b \[CircleTimes] OverBar[OverBar[b]] \[CirclePlus] 
             OverBar[b]) \[CircleTimes] ((OverBar[b] \[CirclePlus] 
              OverBar[OverBar[b]]) \[CirclePlus] (b \[CircleTimes] OverBar[
                OverBar[b]] \[CirclePlus] OverBar[b]))], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 77} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         ((OverBar[b] \[CirclePlus] OverBar[OverBar[b]]) \[CirclePlus] 
           (b \[CircleTimes] OverBar[OverBar[b]] \[CirclePlus] 
            OverBar[b])) \[CircleTimes] (b \[CircleTimes] 
            OverBar[OverBar[b]] \[CirclePlus] OverBar[b])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 76}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a] \[CirclePlus] a == ((OverBar[b] \[CirclePlus] 
              OverBar[OverBar[b]]) \[CirclePlus] (b \[CircleTimes] OverBar[
                OverBar[b]] \[CirclePlus] OverBar[b])) \[CircleTimes] 
            (b \[CircleTimes] OverBar[OverBar[b]] \[CirclePlus] OverBar[b])], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 78} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         ((OverBar[OverBar[b]] \[CirclePlus] OverBar[b]) \[CirclePlus] 
           (b \[CircleTimes] OverBar[OverBar[b]] \[CirclePlus] 
            OverBar[b])) \[CircleTimes] (b \[CircleTimes] 
            OverBar[OverBar[b]] \[CirclePlus] OverBar[b])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 77}, 
        "Construct" -> {"SubstitutionLemma", 34}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CirclePlus] (b_)) \[CirclePlus] (c_) -> 
          (b \[CirclePlus] a) \[CirclePlus] c, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CirclePlus] a == 
           ((OverBar[OverBar[b]] \[CirclePlus] OverBar[b]) \[CirclePlus] 
             (b \[CircleTimes] OverBar[OverBar[b]] \[CirclePlus] 
              OverBar[b])) \[CircleTimes] (b \[CircleTimes] OverBar[OverBar[
                b]] \[CirclePlus] OverBar[b])], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 79} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         ((b \[CircleTimes] OverBar[OverBar[b]] \[CirclePlus] 
            OverBar[b]) \[CirclePlus] (OverBar[OverBar[b]] \[CirclePlus] 
            OverBar[b])) \[CircleTimes] (b \[CircleTimes] 
            OverBar[OverBar[b]] \[CirclePlus] OverBar[b])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 78}, 
        "Construct" -> {"Axiom", 5}, "Position" -> {1}, 
        "Rule" -> (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a] \[CirclePlus] a == 
           ((b \[CircleTimes] OverBar[OverBar[b]] \[CirclePlus] 
              OverBar[b]) \[CirclePlus] (OverBar[OverBar[b]] \[CirclePlus] 
              OverBar[b])) \[CircleTimes] (b \[CircleTimes] OverBar[OverBar[
                b]] \[CirclePlus] OverBar[b])], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 80} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         ((b \[CircleTimes] OverBar[OverBar[b]] \[CirclePlus] 
            OverBar[b]) \[CirclePlus] (OverBar[OverBar[b]] \[CirclePlus] 
            OverBar[b])) \[CircleTimes] (OverBar[b] \[CirclePlus] 
           b \[CircleTimes] OverBar[OverBar[b]])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 79}, 
        "Construct" -> {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] (c_)) -> 
          a \[CircleTimes] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CirclePlus] a == 
           ((b \[CircleTimes] OverBar[OverBar[b]] \[CirclePlus] 
              OverBar[b]) \[CirclePlus] (OverBar[OverBar[b]] \[CirclePlus] 
              OverBar[b])) \[CircleTimes] (OverBar[b] \[CirclePlus] 
             b \[CircleTimes] OverBar[OverBar[b]])], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 81} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         (OverBar[b] \[CirclePlus] b \[CircleTimes] 
            OverBar[OverBar[b]]) \[CircleTimes] 
          ((b \[CircleTimes] OverBar[OverBar[b]] \[CirclePlus] 
            OverBar[b]) \[CirclePlus] (OverBar[OverBar[b]] \[CirclePlus] 
            OverBar[b]))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 80}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a] \[CirclePlus] a == (OverBar[b] \[CirclePlus] 
             b \[CircleTimes] OverBar[OverBar[b]]) \[CircleTimes] 
            ((b \[CircleTimes] OverBar[OverBar[b]] \[CirclePlus] 
              OverBar[b]) \[CirclePlus] (OverBar[OverBar[b]] \[CirclePlus] 
              OverBar[b]))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 82} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         (OverBar[b] \[CirclePlus] OverBar[OverBar[b]] \[CircleTimes] 
            b) \[CircleTimes] ((b \[CircleTimes] OverBar[
              OverBar[b]] \[CirclePlus] OverBar[b]) \[CirclePlus] 
           (OverBar[OverBar[b]] \[CirclePlus] OverBar[b]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 81}, 
        "Construct" -> {"SubstitutionLemma", 45}, "Position" -> {1}, 
        "Rule" -> (a_) \[CirclePlus] (b_) \[CircleTimes] (c_) -> 
          a \[CirclePlus] c \[CircleTimes] b, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CirclePlus] a == 
           (OverBar[b] \[CirclePlus] OverBar[OverBar[b]] \[CircleTimes] 
              b) \[CircleTimes] ((b \[CircleTimes] OverBar[OverBar[
                 b]] \[CirclePlus] OverBar[b]) \[CirclePlus] 
             (OverBar[OverBar[b]] \[CirclePlus] OverBar[b]))], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 83} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         (OverBar[b] \[CirclePlus] OverBar[OverBar[b]] \[CircleTimes] 
            b) \[CircleTimes] ((OverBar[OverBar[b]] \[CirclePlus] 
            OverBar[b]) \[CirclePlus] (b \[CircleTimes] 
             OverBar[OverBar[b]] \[CirclePlus] OverBar[b]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 82}, 
        "Construct" -> {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] (c_)) -> 
          a \[CircleTimes] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CirclePlus] a == 
           (OverBar[b] \[CirclePlus] OverBar[OverBar[b]] \[CircleTimes] 
              b) \[CircleTimes] ((OverBar[OverBar[b]] \[CirclePlus] 
              OverBar[b]) \[CirclePlus] (b \[CircleTimes] OverBar[
                OverBar[b]] \[CirclePlus] OverBar[b]))], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 84} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         (OverBar[b] \[CirclePlus] OverBar[OverBar[b]] \[CircleTimes] 
            b) \[CircleTimes] ((OverBar[OverBar[b]] \[CirclePlus] 
            OverBar[b]) \[CirclePlus] b \[CircleTimes] OverBar[OverBar[b]])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 83}, 
        "Construct" -> {"CriticalPairLemma", 36}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CirclePlus] (b_)) \[CirclePlus] ((c_) \[CirclePlus] 
            (b_)) -> (a \[CirclePlus] b) \[CirclePlus] c, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a] \[CirclePlus] a == (OverBar[b] \[CirclePlus] 
             OverBar[OverBar[b]] \[CircleTimes] b) \[CircleTimes] 
            ((OverBar[OverBar[b]] \[CirclePlus] OverBar[b]) \[CirclePlus] 
             b \[CircleTimes] OverBar[OverBar[b]])], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 89} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         (OverBar[b] \[CirclePlus] OverBar[OverBar[b]] \[CircleTimes] 
            b) \[CircleTimes] (b \[CircleTimes] OverBar[
             OverBar[b]] \[CirclePlus] (OverBar[b] \[CirclePlus] 
            OverBar[OverBar[b]]))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 84}, "Construct" -> 
         {"SubstitutionLemma", 88}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CirclePlus] (b_)) \[CirclePlus] (c_) -> 
          c \[CirclePlus] (b \[CirclePlus] a), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CirclePlus] a == 
           (OverBar[b] \[CirclePlus] OverBar[OverBar[b]] \[CircleTimes] 
              b) \[CircleTimes] (b \[CircleTimes] OverBar[OverBar[
                b]] \[CirclePlus] (OverBar[b] \[CirclePlus] OverBar[OverBar[
                b]]))], "Source" -> "cpl"|>|>, {"SubstitutionLemma", 90} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         (b \[CircleTimes] OverBar[OverBar[b]] \[CirclePlus] 
           (OverBar[b] \[CirclePlus] OverBar[OverBar[b]])) \[CircleTimes] 
          (OverBar[b] \[CirclePlus] OverBar[OverBar[b]] \[CircleTimes] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 89}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a] \[CirclePlus] a == (b \[CircleTimes] OverBar[OverBar[
                b]] \[CirclePlus] (OverBar[b] \[CirclePlus] OverBar[OverBar[
                b]])) \[CircleTimes] (OverBar[b] \[CirclePlus] 
             OverBar[OverBar[b]] \[CircleTimes] b)], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 91} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         (b \[CircleTimes] OverBar[OverBar[b]] \[CirclePlus] 
           (OverBar[b] \[CirclePlus] OverBar[OverBar[b]])) \[CircleTimes] 
          (OverBar[OverBar[b]] \[CircleTimes] b \[CirclePlus] OverBar[b])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 90}, 
        "Construct" -> {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] (c_)) -> 
          a \[CircleTimes] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CirclePlus] a == 
           (b \[CircleTimes] OverBar[OverBar[b]] \[CirclePlus] 
             (OverBar[b] \[CirclePlus] OverBar[OverBar[b]])) \[CircleTimes] 
            (OverBar[OverBar[b]] \[CircleTimes] b \[CirclePlus] OverBar[b])], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 92} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         (b \[CircleTimes] OverBar[OverBar[b]] \[CirclePlus] 
           (OverBar[b] \[CirclePlus] OverBar[OverBar[b]])) \[CircleTimes] 
          (b \[CircleTimes] b \[CirclePlus] OverBar[b])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 91}, 
        "Construct" -> {"SubstitutionLemma", 7}, "Position" -> {2, 1, 1}, 
        "Rule" -> OverBar[OverBar[a_]] -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CirclePlus] a == 
           (b \[CircleTimes] OverBar[OverBar[b]] \[CirclePlus] 
             (OverBar[b] \[CirclePlus] OverBar[OverBar[b]])) \[CircleTimes] 
            (b \[CircleTimes] b \[CirclePlus] OverBar[b])], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 93} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         (b \[CircleTimes] b \[CirclePlus] OverBar[b]) \[CircleTimes] 
          (b \[CircleTimes] OverBar[OverBar[b]] \[CirclePlus] 
           (OverBar[b] \[CirclePlus] OverBar[OverBar[b]]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 92}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a] \[CirclePlus] a == (b \[CircleTimes] b \[CirclePlus] 
             OverBar[b]) \[CircleTimes] (b \[CircleTimes] OverBar[OverBar[
                b]] \[CirclePlus] (OverBar[b] \[CirclePlus] OverBar[OverBar[
                b]]))], "Source" -> "cpl"|>|>, {"SubstitutionLemma", 98} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         (b \[CircleTimes] b \[CirclePlus] OverBar[b]) \[CircleTimes] 
          (OverBar[OverBar[b]] \[CirclePlus] (b \[CircleTimes] 
             OverBar[OverBar[b]] \[CirclePlus] OverBar[b]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 93}, 
        "Construct" -> {"SubstitutionLemma", 97}, "Position" -> {2}, 
        "Rule" -> (a_) \[CirclePlus] ((b_) \[CirclePlus] (c_)) -> 
          c \[CirclePlus] (a \[CirclePlus] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CirclePlus] a == 
           (b \[CircleTimes] b \[CirclePlus] OverBar[b]) \[CircleTimes] 
            (OverBar[OverBar[b]] \[CirclePlus] (b \[CircleTimes] OverBar[
                OverBar[b]] \[CirclePlus] OverBar[b]))], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 99} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         (b \[CircleTimes] b \[CirclePlus] OverBar[b]) \[CircleTimes] 
          (OverBar[b] \[CirclePlus] (OverBar[OverBar[b]] \[CirclePlus] 
            b \[CircleTimes] OverBar[OverBar[b]]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 98}, 
        "Construct" -> {"SubstitutionLemma", 97}, "Position" -> {2}, 
        "Rule" -> (a_) \[CirclePlus] ((b_) \[CirclePlus] (c_)) -> 
          c \[CirclePlus] (a \[CirclePlus] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CirclePlus] a == 
           (b \[CircleTimes] b \[CirclePlus] OverBar[b]) \[CircleTimes] 
            (OverBar[b] \[CirclePlus] (OverBar[OverBar[b]] \[CirclePlus] 
              b \[CircleTimes] OverBar[OverBar[b]]))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 100} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         (OverBar[b] \[CirclePlus] (OverBar[OverBar[b]] \[CirclePlus] 
            b \[CircleTimes] OverBar[OverBar[b]])) \[CircleTimes] 
          (b \[CircleTimes] b \[CirclePlus] OverBar[b])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 99}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a] \[CirclePlus] a == (OverBar[b] \[CirclePlus] 
             (OverBar[OverBar[b]] \[CirclePlus] b \[CircleTimes] OverBar[
                OverBar[b]])) \[CircleTimes] (b \[CircleTimes] 
              b \[CirclePlus] OverBar[b])], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 101} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         (OverBar[b] \[CirclePlus] (OverBar[OverBar[b]] \[CirclePlus] 
            b \[CircleTimes] OverBar[OverBar[b]])) \[CircleTimes] 
          (OverBar[b] \[CirclePlus] b \[CircleTimes] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 100}, 
        "Construct" -> {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] (c_)) -> 
          a \[CircleTimes] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CirclePlus] a == 
           (OverBar[b] \[CirclePlus] (OverBar[OverBar[b]] \[CirclePlus] 
              b \[CircleTimes] OverBar[OverBar[b]])) \[CircleTimes] 
            (OverBar[b] \[CirclePlus] b \[CircleTimes] b)], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 102} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         (b \[CircleTimes] OverBar[OverBar[b]] \[CirclePlus] 
           (OverBar[OverBar[b]] \[CirclePlus] OverBar[b])) \[CircleTimes] 
          (OverBar[b] \[CirclePlus] b \[CircleTimes] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 101}, 
        "Construct" -> {"SubstitutionLemma", 38}, "Position" -> {1}, 
        "Rule" -> (a_) \[CirclePlus] ((b_) \[CirclePlus] (c_)) -> 
          c \[CirclePlus] (b \[CirclePlus] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CirclePlus] a == 
           (b \[CircleTimes] OverBar[OverBar[b]] \[CirclePlus] 
             (OverBar[OverBar[b]] \[CirclePlus] OverBar[b])) \[CircleTimes] 
            (OverBar[b] \[CirclePlus] b \[CircleTimes] b)], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 103} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         (OverBar[b] \[CirclePlus] b \[CircleTimes] b) \[CircleTimes] 
          (b \[CircleTimes] OverBar[OverBar[b]] \[CirclePlus] 
           (OverBar[OverBar[b]] \[CirclePlus] OverBar[b]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 102}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a] \[CirclePlus] a == (OverBar[b] \[CirclePlus] 
             b \[CircleTimes] b) \[CircleTimes] (b \[CircleTimes] 
              OverBar[OverBar[b]] \[CirclePlus] (OverBar[OverBar[
                b]] \[CirclePlus] OverBar[b]))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 104} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         (OverBar[b] \[CirclePlus] b \[CircleTimes] b) \[CircleTimes] 
          (OverBar[b] \[CirclePlus] (b \[CircleTimes] 
             OverBar[OverBar[b]] \[CirclePlus] OverBar[OverBar[b]]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 103}, 
        "Construct" -> {"SubstitutionLemma", 97}, "Position" -> {2}, 
        "Rule" -> (a_) \[CirclePlus] ((b_) \[CirclePlus] (c_)) -> 
          c \[CirclePlus] (a \[CirclePlus] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CirclePlus] a == 
           (OverBar[b] \[CirclePlus] b \[CircleTimes] b) \[CircleTimes] 
            (OverBar[b] \[CirclePlus] (b \[CircleTimes] OverBar[
                OverBar[b]] \[CirclePlus] OverBar[OverBar[b]]))], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 105} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         (OverBar[b] \[CirclePlus] b \[CircleTimes] b) \[CircleTimes] 
          (OverBar[OverBar[b]] \[CirclePlus] (OverBar[b] \[CirclePlus] 
            b \[CircleTimes] OverBar[OverBar[b]]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 104}, 
        "Construct" -> {"SubstitutionLemma", 97}, "Position" -> {2}, 
        "Rule" -> (a_) \[CirclePlus] ((b_) \[CirclePlus] (c_)) -> 
          c \[CirclePlus] (a \[CirclePlus] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CirclePlus] a == 
           (OverBar[b] \[CirclePlus] b \[CircleTimes] b) \[CircleTimes] 
            (OverBar[OverBar[b]] \[CirclePlus] (OverBar[b] \[CirclePlus] 
              b \[CircleTimes] OverBar[OverBar[b]]))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 106} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         (OverBar[OverBar[b]] \[CirclePlus] (OverBar[b] \[CirclePlus] 
            b \[CircleTimes] OverBar[OverBar[b]])) \[CircleTimes] 
          (OverBar[b] \[CirclePlus] b \[CircleTimes] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 105}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a] \[CirclePlus] a == (OverBar[OverBar[b]] \[CirclePlus] 
             (OverBar[b] \[CirclePlus] b \[CircleTimes] OverBar[
                OverBar[b]])) \[CircleTimes] (OverBar[b] \[CirclePlus] 
             b \[CircleTimes] b)], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 107} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         (OverBar[OverBar[b]] \[CirclePlus] (OverBar[b] \[CirclePlus] 
            OverBar[OverBar[b]])) \[CircleTimes] (OverBar[b] \[CirclePlus] 
           b \[CircleTimes] b)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 106}, "Construct" -> 
         {"CriticalPairLemma", 44}, "Position" -> {1, 2}, 
        "Rule" -> OverBar[a_] \[CirclePlus] (a_) \[CircleTimes] (b_) -> 
          OverBar[a] \[CirclePlus] b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CirclePlus] a == 
           (OverBar[OverBar[b]] \[CirclePlus] (OverBar[b] \[CirclePlus] 
              OverBar[OverBar[b]])) \[CircleTimes] (OverBar[b] \[CirclePlus] 
             b \[CircleTimes] b)], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 108} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         (OverBar[OverBar[b]] \[CirclePlus] (OverBar[b] \[CirclePlus] 
            OverBar[OverBar[b]])) \[CircleTimes] 
          (b \[CircleTimes] b \[CirclePlus] OverBar[b])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 107}, 
        "Construct" -> {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] (c_)) -> 
          a \[CircleTimes] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CirclePlus] a == 
           (OverBar[OverBar[b]] \[CirclePlus] (OverBar[b] \[CirclePlus] 
              OverBar[OverBar[b]])) \[CircleTimes] 
            (b \[CircleTimes] b \[CirclePlus] OverBar[b])], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 109} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         (b \[CircleTimes] b \[CirclePlus] OverBar[b]) \[CircleTimes] 
          (OverBar[OverBar[b]] \[CirclePlus] (OverBar[b] \[CirclePlus] 
            OverBar[OverBar[b]]))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 108}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {}, "Rule" -> (a_) \[CircleTimes] (b_) -> 
          b \[CircleTimes] a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[OverBar[a] \[CirclePlus] a == 
           (b \[CircleTimes] b \[CirclePlus] OverBar[b]) \[CircleTimes] 
            (OverBar[OverBar[b]] \[CirclePlus] (OverBar[b] \[CirclePlus] 
              OverBar[OverBar[b]]))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 110} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         (b \[CircleTimes] b \[CirclePlus] OverBar[b]) \[CircleTimes] 
          (OverBar[OverBar[b]] \[CirclePlus] (b \[CirclePlus] OverBar[b]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 109}, 
        "Construct" -> {"CriticalPairLemma", 45}, "Position" -> {2, 2}, 
        "Rule" -> (a_) \[CirclePlus] OverBar[a_] -> b \[CirclePlus] 
           OverBar[b], "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[OverBar[a] \[CirclePlus] a == 
           (b \[CircleTimes] b \[CirclePlus] OverBar[b]) \[CircleTimes] 
            (OverBar[OverBar[b]] \[CirclePlus] (b \[CirclePlus] 
              OverBar[b]))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 111} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         (b \[CircleTimes] b \[CirclePlus] OverBar[b]) \[CircleTimes] 
          (OverBar[b] \[CirclePlus] (OverBar[OverBar[b]] \[CirclePlus] b))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 110}, 
        "Construct" -> {"SubstitutionLemma", 97}, "Position" -> {2}, 
        "Rule" -> (a_) \[CirclePlus] ((b_) \[CirclePlus] (c_)) -> 
          c \[CirclePlus] (a \[CirclePlus] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CirclePlus] a == 
           (b \[CircleTimes] b \[CirclePlus] OverBar[b]) \[CircleTimes] 
            (OverBar[b] \[CirclePlus] (OverBar[OverBar[b]] \[CirclePlus] 
              b))], "Source" -> "cpl"|>|>, {"SubstitutionLemma", 112} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         (OverBar[b] \[CirclePlus] (OverBar[OverBar[b]] \[CirclePlus] 
            b)) \[CircleTimes] (b \[CircleTimes] b \[CirclePlus] 
           OverBar[b])], "Proof" -> <|"Input" -> {"SubstitutionLemma", 111}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a] \[CirclePlus] a == (OverBar[b] \[CirclePlus] 
             (OverBar[OverBar[b]] \[CirclePlus] b)) \[CircleTimes] 
            (b \[CircleTimes] b \[CirclePlus] OverBar[b])], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 113} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         (OverBar[b] \[CirclePlus] (OverBar[OverBar[b]] \[CirclePlus] 
            b)) \[CircleTimes] (OverBar[b] \[CirclePlus] b \[CircleTimes] 
            b)], "Proof" -> <|"Input" -> {"SubstitutionLemma", 112}, 
        "Construct" -> {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] (c_)) -> 
          a \[CircleTimes] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CirclePlus] a == 
           (OverBar[b] \[CirclePlus] (OverBar[OverBar[b]] \[CirclePlus] 
              b)) \[CircleTimes] (OverBar[b] \[CirclePlus] b \[CircleTimes] 
              b)], "Source" -> "cpl"|>|>, {"SubstitutionLemma", 114} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         (OverBar[b] \[CirclePlus] (OverBar[OverBar[b]] \[CirclePlus] 
            b)) \[CircleTimes] (OverBar[b] \[CirclePlus] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 113}, 
        "Construct" -> {"CriticalPairLemma", 14}, "Position" -> {2}, 
        "Rule" -> OverBar[a_] \[CirclePlus] (b_) \[CircleTimes] (a_) -> 
          OverBar[a] \[CirclePlus] b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CirclePlus] a == 
           (OverBar[b] \[CirclePlus] (OverBar[OverBar[b]] \[CirclePlus] 
              b)) \[CircleTimes] (OverBar[b] \[CirclePlus] b)], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 115} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         (OverBar[b] \[CirclePlus] b) \[CircleTimes] 
          (OverBar[b] \[CirclePlus] (OverBar[OverBar[b]] \[CirclePlus] b))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 114}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a] \[CirclePlus] a == (OverBar[b] \[CirclePlus] 
             b) \[CircleTimes] (OverBar[b] \[CirclePlus] 
             (OverBar[OverBar[b]] \[CirclePlus] b))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 116} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         (OverBar[b] \[CirclePlus] b) \[CircleTimes] (b \[CirclePlus] 
           (OverBar[b] \[CirclePlus] OverBar[OverBar[b]]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 115}, 
        "Construct" -> {"SubstitutionLemma", 97}, "Position" -> {2}, 
        "Rule" -> (a_) \[CirclePlus] ((b_) \[CirclePlus] (c_)) -> 
          c \[CirclePlus] (a \[CirclePlus] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CirclePlus] a == 
           (OverBar[b] \[CirclePlus] b) \[CircleTimes] (b \[CirclePlus] 
             (OverBar[b] \[CirclePlus] OverBar[OverBar[b]]))], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 117} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         (OverBar[b] \[CirclePlus] b) \[CircleTimes] 
          (OverBar[OverBar[b]] \[CirclePlus] (b \[CirclePlus] OverBar[b]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 116}, 
        "Construct" -> {"SubstitutionLemma", 97}, "Position" -> {2}, 
        "Rule" -> (a_) \[CirclePlus] ((b_) \[CirclePlus] (c_)) -> 
          c \[CirclePlus] (a \[CirclePlus] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CirclePlus] a == 
           (OverBar[b] \[CirclePlus] b) \[CircleTimes] 
            (OverBar[OverBar[b]] \[CirclePlus] (b \[CirclePlus] 
              OverBar[b]))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 118} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         (OverBar[OverBar[b]] \[CirclePlus] (b \[CirclePlus] 
            OverBar[b])) \[CircleTimes] (OverBar[b] \[CirclePlus] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 117}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a] \[CirclePlus] a == (OverBar[OverBar[b]] \[CirclePlus] 
             (b \[CirclePlus] OverBar[b])) \[CircleTimes] 
            (OverBar[b] \[CirclePlus] b)], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 119} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         (OverBar[OverBar[b]] \[CirclePlus] (b \[CirclePlus] 
            OverBar[b])) \[CircleTimes] (b \[CirclePlus] OverBar[b])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 118}, 
        "Construct" -> {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] (c_)) -> 
          a \[CircleTimes] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CirclePlus] a == 
           (OverBar[OverBar[b]] \[CirclePlus] (b \[CirclePlus] 
              OverBar[b])) \[CircleTimes] (b \[CirclePlus] OverBar[b])], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 120} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         OverBar[OverBar[b]] \[CirclePlus] (b \[CirclePlus] OverBar[b])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 119}, 
        "Construct" -> {"Axiom", 4}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] OverBar[b_]) -> a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a] \[CirclePlus] a == OverBar[OverBar[b]] \[CirclePlus] 
            (b \[CirclePlus] OverBar[b])], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 121} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] a == 
         b \[CirclePlus] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 120}, "Construct" -> 
         {"CriticalPairLemma", 46}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] ((b_) \[CirclePlus] OverBar[b_]) -> 
          b \[CirclePlus] OverBar[b], "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CirclePlus] a == 
           b \[CirclePlus] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 122} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] 
          OverBar[OverBar[a]] \[CircleTimes] a == b \[CirclePlus] 
          OverBar[b]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 121}, 
        "Construct" -> {"CriticalPairLemma", 2}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] (b_) -> a \[CirclePlus] 
           OverBar[a] \[CircleTimes] b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CirclePlus] 
            OverBar[OverBar[a]] \[CircleTimes] a == b \[CirclePlus] 
            OverBar[b]], "Source" -> "cpl"|>|>, {"SubstitutionLemma", 123} -> 
     <|"Statement" -> HoldForm[(OverBar[a] \[CirclePlus] 
           OverBar[OverBar[a]] \[CircleTimes] a) \[CircleTimes] 
          ((OverBar[a] \[CirclePlus] OverBar[OverBar[a]] \[CircleTimes] 
             a) \[CirclePlus] OverBar[OverBar[OverBar[a] \[CirclePlus] 
              OverBar[OverBar[a]] \[CircleTimes] a]]) == 
         b \[CirclePlus] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 122}, "Construct" -> 
         {"SubstitutionLemma", 2}, "Position" -> {}, 
        "Rule" -> a_ -> a \[CircleTimes] (a \[CirclePlus] 
            OverBar[OverBar[a]]), "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[(OverBar[a] \[CirclePlus] OverBar[OverBar[
                a]] \[CircleTimes] a) \[CircleTimes] 
            ((OverBar[a] \[CirclePlus] OverBar[OverBar[a]] \[CircleTimes] 
               a) \[CirclePlus] OverBar[OverBar[OverBar[a] \[CirclePlus] 
                OverBar[OverBar[a]] \[CircleTimes] a]]) == 
           b \[CirclePlus] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 124} -> 
     <|"Statement" -> HoldForm[((OverBar[a] \[CirclePlus] 
            OverBar[OverBar[a]] \[CircleTimes] a) \[CirclePlus] 
           OverBar[OverBar[OverBar[a] \[CirclePlus] OverBar[OverBar[
                 a]] \[CircleTimes] a]]) \[CircleTimes] 
          (OverBar[a] \[CirclePlus] OverBar[OverBar[a]] \[CircleTimes] a) == 
         b \[CirclePlus] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 123}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {}, "Rule" -> (a_) \[CircleTimes] (b_) -> 
          b \[CircleTimes] a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[((OverBar[a] \[CirclePlus] OverBar[OverBar[
                 a]] \[CircleTimes] a) \[CirclePlus] OverBar[
              OverBar[OverBar[a] \[CirclePlus] OverBar[OverBar[
                   a]] \[CircleTimes] a]]) \[CircleTimes] 
            (OverBar[a] \[CirclePlus] OverBar[OverBar[a]] \[CircleTimes] 
              a) == b \[CirclePlus] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 125} -> 
     <|"Statement" -> HoldForm[((OverBar[a] \[CirclePlus] 
            OverBar[OverBar[a]] \[CircleTimes] a) \[CirclePlus] 
           OverBar[OverBar[OverBar[a] \[CirclePlus] OverBar[OverBar[
                a]]]]) \[CircleTimes] (OverBar[a] \[CirclePlus] 
           OverBar[OverBar[a]] \[CircleTimes] a) == b \[CirclePlus] 
          OverBar[b]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 124}, 
        "Construct" -> {"CriticalPairLemma", 14}, "Position" -> {1, 2, 1, 1}, 
        "Rule" -> OverBar[a_] \[CirclePlus] (b_) \[CircleTimes] (a_) -> 
          OverBar[a] \[CirclePlus] b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          ((OverBar[a] \[CirclePlus] OverBar[OverBar[a]] \[CircleTimes] 
               a) \[CirclePlus] OverBar[OverBar[OverBar[a] \[CirclePlus] 
                OverBar[OverBar[a]]]]) \[CircleTimes] 
            (OverBar[a] \[CirclePlus] OverBar[OverBar[a]] \[CircleTimes] 
              a) == b \[CirclePlus] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 126} -> 
     <|"Statement" -> HoldForm[((OverBar[a] \[CirclePlus] 
            OverBar[OverBar[a]] \[CircleTimes] a) \[CirclePlus] 
           OverBar[OverBar[OverBar[a] \[CirclePlus] OverBar[OverBar[
                a]]]]) \[CircleTimes] (OverBar[OverBar[a]] \[CircleTimes] 
            a \[CirclePlus] OverBar[a]) == b \[CirclePlus] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 125}, 
        "Construct" -> {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] (c_)) -> 
          a \[CircleTimes] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          ((OverBar[a] \[CirclePlus] OverBar[OverBar[a]] \[CircleTimes] 
               a) \[CirclePlus] OverBar[OverBar[OverBar[a] \[CirclePlus] 
                OverBar[OverBar[a]]]]) \[CircleTimes] 
            (OverBar[OverBar[a]] \[CircleTimes] a \[CirclePlus] 
             OverBar[a]) == b \[CirclePlus] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 127} -> 
     <|"Statement" -> HoldForm[(OverBar[OverBar[a]] \[CircleTimes] 
            a \[CirclePlus] OverBar[a]) \[CircleTimes] 
          ((OverBar[a] \[CirclePlus] OverBar[OverBar[a]] \[CircleTimes] 
             a) \[CirclePlus] OverBar[OverBar[OverBar[a] \[CirclePlus] 
              OverBar[OverBar[a]]]]) == b \[CirclePlus] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 126}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (OverBar[OverBar[a]] \[CircleTimes] a \[CirclePlus] 
             OverBar[a]) \[CircleTimes] ((OverBar[a] \[CirclePlus] 
              OverBar[OverBar[a]] \[CircleTimes] a) \[CirclePlus] 
             OverBar[OverBar[OverBar[a] \[CirclePlus] OverBar[OverBar[
                  a]]]]) == b \[CirclePlus] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 128} -> 
     <|"Statement" -> HoldForm[(OverBar[OverBar[a]] \[CircleTimes] 
            a \[CirclePlus] OverBar[a]) \[CircleTimes] 
          (OverBar[OverBar[OverBar[a] \[CirclePlus] OverBar[OverBar[
                a]]]] \[CirclePlus] (OverBar[a] \[CirclePlus] 
            OverBar[OverBar[a]] \[CircleTimes] a)) == b \[CirclePlus] 
          OverBar[b]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 127}, 
        "Construct" -> {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] (c_)) -> 
          a \[CircleTimes] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(OverBar[OverBar[a]] \[CircleTimes] 
              a \[CirclePlus] OverBar[a]) \[CircleTimes] 
            (OverBar[OverBar[OverBar[a] \[CirclePlus] OverBar[OverBar[
                  a]]]] \[CirclePlus] (OverBar[a] \[CirclePlus] 
              OverBar[OverBar[a]] \[CircleTimes] a)) == b \[CirclePlus] 
            OverBar[b]], "Source" -> "cpl"|>|>, {"SubstitutionLemma", 129} -> 
     <|"Statement" -> HoldForm[(OverBar[OverBar[a]] \[CircleTimes] 
            a \[CirclePlus] OverBar[a]) \[CircleTimes] 
          (OverBar[OverBar[OverBar[a] \[CirclePlus] OverBar[OverBar[
                a]]]] \[CirclePlus] (OverBar[OverBar[a]] \[CircleTimes] 
             a \[CirclePlus] OverBar[a])) == b \[CirclePlus] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 128}, 
        "Construct" -> {"SubstitutionLemma", 40}, "Position" -> {2}, 
        "Rule" -> (a_) \[CirclePlus] ((b_) \[CirclePlus] (c_)) -> 
          a \[CirclePlus] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(OverBar[OverBar[a]] \[CircleTimes] 
              a \[CirclePlus] OverBar[a]) \[CircleTimes] 
            (OverBar[OverBar[OverBar[a] \[CirclePlus] OverBar[OverBar[
                  a]]]] \[CirclePlus] (OverBar[OverBar[a]] \[CircleTimes] 
               a \[CirclePlus] OverBar[a])) == b \[CirclePlus] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 130} -> 
     <|"Statement" -> HoldForm[
        (OverBar[OverBar[OverBar[a] \[CirclePlus] OverBar[OverBar[
                a]]]] \[CirclePlus] (OverBar[OverBar[a]] \[CircleTimes] 
             a \[CirclePlus] OverBar[a])) \[CircleTimes] 
          (OverBar[OverBar[a]] \[CircleTimes] a \[CirclePlus] OverBar[a]) == 
         b \[CirclePlus] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 129}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {}, "Rule" -> (a_) \[CircleTimes] (b_) -> 
          b \[CircleTimes] a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[(OverBar[OverBar[OverBar[a] \[CirclePlus] OverBar[
                 OverBar[a]]]] \[CirclePlus] (OverBar[OverBar[
                 a]] \[CircleTimes] a \[CirclePlus] OverBar[
               a])) \[CircleTimes] (OverBar[OverBar[a]] \[CircleTimes] 
              a \[CirclePlus] OverBar[a]) == b \[CirclePlus] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 131} -> 
     <|"Statement" -> HoldForm[
        (OverBar[OverBar[OverBar[a] \[CirclePlus] OverBar[OverBar[
                a]]]] \[CirclePlus] (OverBar[OverBar[a]] \[CircleTimes] 
             a \[CirclePlus] OverBar[a])) \[CircleTimes] 
          (OverBar[a] \[CirclePlus] OverBar[OverBar[a]] \[CircleTimes] a) == 
         b \[CirclePlus] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 130}, "Construct" -> 
         {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] (c_)) -> 
          a \[CircleTimes] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          (OverBar[OverBar[OverBar[a] \[CirclePlus] OverBar[OverBar[
                  a]]]] \[CirclePlus] (OverBar[OverBar[a]] \[CircleTimes] 
               a \[CirclePlus] OverBar[a])) \[CircleTimes] 
            (OverBar[a] \[CirclePlus] OverBar[OverBar[a]] \[CircleTimes] 
              a) == b \[CirclePlus] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 132} -> 
     <|"Statement" -> HoldForm[
        (OverBar[OverBar[OverBar[a] \[CirclePlus] OverBar[OverBar[
                a]]]] \[CirclePlus] (OverBar[OverBar[a]] \[CircleTimes] 
             a \[CirclePlus] OverBar[a])) \[CircleTimes] 
          (OverBar[a] \[CirclePlus] a \[CircleTimes] OverBar[OverBar[a]]) == 
         b \[CirclePlus] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 131}, "Construct" -> 
         {"SubstitutionLemma", 45}, "Position" -> {2}, 
        "Rule" -> (a_) \[CirclePlus] (b_) \[CircleTimes] (c_) -> 
          a \[CirclePlus] c \[CircleTimes] b, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          (OverBar[OverBar[OverBar[a] \[CirclePlus] OverBar[OverBar[
                  a]]]] \[CirclePlus] (OverBar[OverBar[a]] \[CircleTimes] 
               a \[CirclePlus] OverBar[a])) \[CircleTimes] 
            (OverBar[a] \[CirclePlus] a \[CircleTimes] OverBar[OverBar[
                a]]) == b \[CirclePlus] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 133} -> 
     <|"Statement" -> HoldForm[(OverBar[a] \[CirclePlus] a \[CircleTimes] 
            OverBar[OverBar[a]]) \[CircleTimes] 
          (OverBar[OverBar[OverBar[a] \[CirclePlus] OverBar[OverBar[
                a]]]] \[CirclePlus] (OverBar[OverBar[a]] \[CircleTimes] 
             a \[CirclePlus] OverBar[a])) == b \[CirclePlus] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 132}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (OverBar[a] \[CirclePlus] a \[CircleTimes] OverBar[OverBar[
                a]]) \[CircleTimes] (OverBar[OverBar[OverBar[a] \[CirclePlus] 
                OverBar[OverBar[a]]]] \[CirclePlus] 
             (OverBar[OverBar[a]] \[CircleTimes] a \[CirclePlus] 
              OverBar[a])) == b \[CirclePlus] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 134} -> 
     <|"Statement" -> HoldForm[(OverBar[a] \[CirclePlus] a \[CircleTimes] 
            OverBar[OverBar[a]]) \[CircleTimes] 
          ((OverBar[OverBar[a]] \[CircleTimes] a \[CirclePlus] 
            OverBar[a]) \[CirclePlus] OverBar[OverBar[
             OverBar[a] \[CirclePlus] OverBar[OverBar[a]]]]) == 
         b \[CirclePlus] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 133}, "Construct" -> 
         {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] (c_)) -> 
          a \[CircleTimes] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(OverBar[a] \[CirclePlus] 
             a \[CircleTimes] OverBar[OverBar[a]]) \[CircleTimes] 
            ((OverBar[OverBar[a]] \[CircleTimes] a \[CirclePlus] 
              OverBar[a]) \[CirclePlus] OverBar[OverBar[OverBar[
                 a] \[CirclePlus] OverBar[OverBar[a]]]]) == 
           b \[CirclePlus] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 135} -> 
     <|"Statement" -> HoldForm[(OverBar[a] \[CirclePlus] a \[CircleTimes] 
            OverBar[OverBar[a]]) \[CircleTimes] 
          ((OverBar[OverBar[a]] \[CircleTimes] a \[CirclePlus] 
            OverBar[a]) \[CirclePlus] (OverBar[a] \[CirclePlus] 
            OverBar[OverBar[a]])) == b \[CirclePlus] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 134}, 
        "Construct" -> {"SubstitutionLemma", 7}, "Position" -> {2, 2}, 
        "Rule" -> OverBar[OverBar[a_]] -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(OverBar[a] \[CirclePlus] 
             a \[CircleTimes] OverBar[OverBar[a]]) \[CircleTimes] 
            ((OverBar[OverBar[a]] \[CircleTimes] a \[CirclePlus] 
              OverBar[a]) \[CirclePlus] (OverBar[a] \[CirclePlus] 
              OverBar[OverBar[a]])) == b \[CirclePlus] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 136} -> 
     <|"Statement" -> HoldForm[
        ((OverBar[OverBar[a]] \[CircleTimes] a \[CirclePlus] 
            OverBar[a]) \[CirclePlus] (OverBar[a] \[CirclePlus] 
            OverBar[OverBar[a]])) \[CircleTimes] (OverBar[a] \[CirclePlus] 
           a \[CircleTimes] OverBar[OverBar[a]]) == b \[CirclePlus] 
          OverBar[b]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 135}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          ((OverBar[OverBar[a]] \[CircleTimes] a \[CirclePlus] 
              OverBar[a]) \[CirclePlus] (OverBar[a] \[CirclePlus] 
              OverBar[OverBar[a]])) \[CircleTimes] (OverBar[a] \[CirclePlus] 
             a \[CircleTimes] OverBar[OverBar[a]]) == b \[CirclePlus] 
            OverBar[b]], "Source" -> "cpl"|>|>, {"SubstitutionLemma", 137} -> 
     <|"Statement" -> HoldForm[
        ((OverBar[OverBar[a]] \[CircleTimes] a \[CirclePlus] 
            OverBar[a]) \[CirclePlus] (OverBar[a] \[CirclePlus] 
            OverBar[OverBar[a]])) \[CircleTimes] 
          (a \[CircleTimes] OverBar[OverBar[a]] \[CirclePlus] OverBar[a]) == 
         b \[CirclePlus] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 136}, "Construct" -> 
         {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] (c_)) -> 
          a \[CircleTimes] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          ((OverBar[OverBar[a]] \[CircleTimes] a \[CirclePlus] 
              OverBar[a]) \[CirclePlus] (OverBar[a] \[CirclePlus] 
              OverBar[OverBar[a]])) \[CircleTimes] 
            (a \[CircleTimes] OverBar[OverBar[a]] \[CirclePlus] 
             OverBar[a]) == b \[CirclePlus] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 138} -> 
     <|"Statement" -> HoldForm[
        ((OverBar[OverBar[a]] \[CircleTimes] a \[CirclePlus] 
            OverBar[a]) \[CirclePlus] (OverBar[a] \[CirclePlus] 
            OverBar[OverBar[a]])) \[CircleTimes] 
          (OverBar[OverBar[a]] \[CircleTimes] a \[CirclePlus] OverBar[a]) == 
         b \[CirclePlus] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 137}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {2, 1}, "Rule" -> (a_) \[CircleTimes] (b_) -> 
          b \[CircleTimes] a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[((OverBar[OverBar[a]] \[CircleTimes] a \[CirclePlus] 
              OverBar[a]) \[CirclePlus] (OverBar[a] \[CirclePlus] 
              OverBar[OverBar[a]])) \[CircleTimes] 
            (OverBar[OverBar[a]] \[CircleTimes] a \[CirclePlus] 
             OverBar[a]) == b \[CirclePlus] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 139} -> 
     <|"Statement" -> HoldForm[(OverBar[OverBar[a]] \[CircleTimes] 
            a \[CirclePlus] OverBar[a]) \[CircleTimes] 
          ((OverBar[OverBar[a]] \[CircleTimes] a \[CirclePlus] 
            OverBar[a]) \[CirclePlus] (OverBar[a] \[CirclePlus] 
            OverBar[OverBar[a]])) == b \[CirclePlus] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 138}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (OverBar[OverBar[a]] \[CircleTimes] a \[CirclePlus] 
             OverBar[a]) \[CircleTimes] ((OverBar[OverBar[a]] \[CircleTimes] 
               a \[CirclePlus] OverBar[a]) \[CirclePlus] 
             (OverBar[a] \[CirclePlus] OverBar[OverBar[a]])) == 
           b \[CirclePlus] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 140} -> 
     <|"Statement" -> HoldForm[(OverBar[OverBar[a]] \[CircleTimes] 
            a \[CirclePlus] OverBar[a]) \[CircleTimes] 
          ((OverBar[OverBar[a]] \[CircleTimes] a \[CirclePlus] 
            OverBar[a]) \[CirclePlus] (OverBar[OverBar[a]] \[CirclePlus] 
            OverBar[a])) == b \[CirclePlus] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 139}, 
        "Construct" -> {"SubstitutionLemma", 40}, "Position" -> {2}, 
        "Rule" -> (a_) \[CirclePlus] ((b_) \[CirclePlus] (c_)) -> 
          a \[CirclePlus] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(OverBar[OverBar[a]] \[CircleTimes] 
              a \[CirclePlus] OverBar[a]) \[CircleTimes] 
            ((OverBar[OverBar[a]] \[CircleTimes] a \[CirclePlus] 
              OverBar[a]) \[CirclePlus] (OverBar[OverBar[a]] \[CirclePlus] 
              OverBar[a])) == b \[CirclePlus] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 141} -> 
     <|"Statement" -> HoldForm[(OverBar[OverBar[a]] \[CircleTimes] 
            a \[CirclePlus] OverBar[a]) \[CircleTimes] 
          ((OverBar[OverBar[a]] \[CirclePlus] OverBar[a]) \[CirclePlus] 
           (OverBar[OverBar[a]] \[CircleTimes] a \[CirclePlus] 
            OverBar[a])) == b \[CirclePlus] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 140}, 
        "Construct" -> {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] (c_)) -> 
          a \[CircleTimes] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(OverBar[OverBar[a]] \[CircleTimes] 
              a \[CirclePlus] OverBar[a]) \[CircleTimes] 
            ((OverBar[OverBar[a]] \[CirclePlus] OverBar[a]) \[CirclePlus] 
             (OverBar[OverBar[a]] \[CircleTimes] a \[CirclePlus] 
              OverBar[a])) == b \[CirclePlus] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 142} -> 
     <|"Statement" -> HoldForm[((OverBar[OverBar[a]] \[CirclePlus] 
            OverBar[a]) \[CirclePlus] (OverBar[OverBar[a]] \[CircleTimes] 
             a \[CirclePlus] OverBar[a])) \[CircleTimes] 
          (OverBar[OverBar[a]] \[CircleTimes] a \[CirclePlus] OverBar[a]) == 
         b \[CirclePlus] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 141}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {}, "Rule" -> (a_) \[CircleTimes] (b_) -> 
          b \[CircleTimes] a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[((OverBar[OverBar[a]] \[CirclePlus] OverBar[
               a]) \[CirclePlus] (OverBar[OverBar[a]] \[CircleTimes] 
               a \[CirclePlus] OverBar[a])) \[CircleTimes] 
            (OverBar[OverBar[a]] \[CircleTimes] a \[CirclePlus] 
             OverBar[a]) == b \[CirclePlus] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 143} -> 
     <|"Statement" -> HoldForm[((OverBar[OverBar[a]] \[CirclePlus] 
            OverBar[a]) \[CirclePlus] (OverBar[OverBar[a]] \[CircleTimes] 
             a \[CirclePlus] OverBar[a])) \[CircleTimes] 
          (OverBar[a] \[CirclePlus] OverBar[OverBar[a]] \[CircleTimes] a) == 
         b \[CirclePlus] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 142}, "Construct" -> 
         {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] (c_)) -> 
          a \[CircleTimes] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[((OverBar[OverBar[a]] \[CirclePlus] 
              OverBar[a]) \[CirclePlus] (OverBar[OverBar[a]] \[CircleTimes] 
               a \[CirclePlus] OverBar[a])) \[CircleTimes] 
            (OverBar[a] \[CirclePlus] OverBar[OverBar[a]] \[CircleTimes] 
              a) == b \[CirclePlus] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 144} -> 
     <|"Statement" -> HoldForm[((OverBar[OverBar[a]] \[CirclePlus] 
            OverBar[a]) \[CirclePlus] (OverBar[OverBar[a]] \[CircleTimes] 
             a \[CirclePlus] OverBar[a])) \[CircleTimes] 
          (OverBar[a] \[CirclePlus] a \[CircleTimes] OverBar[OverBar[a]]) == 
         b \[CirclePlus] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 143}, "Construct" -> 
         {"SubstitutionLemma", 45}, "Position" -> {2}, 
        "Rule" -> (a_) \[CirclePlus] (b_) \[CircleTimes] (c_) -> 
          a \[CirclePlus] c \[CircleTimes] b, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[((OverBar[OverBar[a]] \[CirclePlus] 
              OverBar[a]) \[CirclePlus] (OverBar[OverBar[a]] \[CircleTimes] 
               a \[CirclePlus] OverBar[a])) \[CircleTimes] 
            (OverBar[a] \[CirclePlus] a \[CircleTimes] OverBar[OverBar[
                a]]) == b \[CirclePlus] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 145} -> 
     <|"Statement" -> HoldForm[(OverBar[a] \[CirclePlus] a \[CircleTimes] 
            OverBar[OverBar[a]]) \[CircleTimes] 
          ((OverBar[OverBar[a]] \[CirclePlus] OverBar[a]) \[CirclePlus] 
           (OverBar[OverBar[a]] \[CircleTimes] a \[CirclePlus] 
            OverBar[a])) == b \[CirclePlus] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 144}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (OverBar[a] \[CirclePlus] a \[CircleTimes] OverBar[OverBar[
                a]]) \[CircleTimes] ((OverBar[OverBar[a]] \[CirclePlus] 
              OverBar[a]) \[CirclePlus] (OverBar[OverBar[a]] \[CircleTimes] 
               a \[CirclePlus] OverBar[a])) == b \[CirclePlus] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 146} -> 
     <|"Statement" -> HoldForm[(OverBar[a] \[CirclePlus] a \[CircleTimes] 
            OverBar[OverBar[a]]) \[CircleTimes] 
          ((OverBar[OverBar[a]] \[CirclePlus] OverBar[a]) \[CirclePlus] 
           (OverBar[a] \[CirclePlus] OverBar[OverBar[a]] \[CircleTimes] 
             a)) == b \[CirclePlus] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 145}, 
        "Construct" -> {"SubstitutionLemma", 40}, "Position" -> {2}, 
        "Rule" -> (a_) \[CirclePlus] ((b_) \[CirclePlus] (c_)) -> 
          a \[CirclePlus] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(OverBar[a] \[CirclePlus] 
             a \[CircleTimes] OverBar[OverBar[a]]) \[CircleTimes] 
            ((OverBar[OverBar[a]] \[CirclePlus] OverBar[a]) \[CirclePlus] 
             (OverBar[a] \[CirclePlus] OverBar[OverBar[a]] \[CircleTimes] 
               a)) == b \[CirclePlus] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 147} -> 
     <|"Statement" -> HoldForm[(OverBar[a] \[CirclePlus] a \[CircleTimes] 
            OverBar[OverBar[a]]) \[CircleTimes] ((OverBar[a] \[CirclePlus] 
            OverBar[OverBar[a]] \[CircleTimes] a) \[CirclePlus] 
           (OverBar[OverBar[a]] \[CirclePlus] OverBar[a])) == 
         b \[CirclePlus] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 146}, "Construct" -> 
         {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] (c_)) -> 
          a \[CircleTimes] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(OverBar[a] \[CirclePlus] 
             a \[CircleTimes] OverBar[OverBar[a]]) \[CircleTimes] 
            ((OverBar[a] \[CirclePlus] OverBar[OverBar[a]] \[CircleTimes] 
               a) \[CirclePlus] (OverBar[OverBar[a]] \[CirclePlus] 
              OverBar[a])) == b \[CirclePlus] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 148} -> 
     <|"Statement" -> HoldForm[((OverBar[a] \[CirclePlus] 
            OverBar[OverBar[a]] \[CircleTimes] a) \[CirclePlus] 
           (OverBar[OverBar[a]] \[CirclePlus] OverBar[a])) \[CircleTimes] 
          (OverBar[a] \[CirclePlus] a \[CircleTimes] OverBar[OverBar[a]]) == 
         b \[CirclePlus] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 147}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {}, "Rule" -> (a_) \[CircleTimes] (b_) -> 
          b \[CircleTimes] a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[((OverBar[a] \[CirclePlus] OverBar[OverBar[
                 a]] \[CircleTimes] a) \[CirclePlus] 
             (OverBar[OverBar[a]] \[CirclePlus] OverBar[a])) \[CircleTimes] 
            (OverBar[a] \[CirclePlus] a \[CircleTimes] OverBar[OverBar[
                a]]) == b \[CirclePlus] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 149} -> 
     <|"Statement" -> HoldForm[((OverBar[a] \[CirclePlus] 
            OverBar[OverBar[a]] \[CircleTimes] a) \[CirclePlus] 
           (OverBar[OverBar[a]] \[CirclePlus] OverBar[a])) \[CircleTimes] 
          (a \[CircleTimes] OverBar[OverBar[a]] \[CirclePlus] OverBar[a]) == 
         b \[CirclePlus] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 148}, "Construct" -> 
         {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] (c_)) -> 
          a \[CircleTimes] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          ((OverBar[a] \[CirclePlus] OverBar[OverBar[a]] \[CircleTimes] 
               a) \[CirclePlus] (OverBar[OverBar[a]] \[CirclePlus] 
              OverBar[a])) \[CircleTimes] (a \[CircleTimes] OverBar[OverBar[
                a]] \[CirclePlus] OverBar[a]) == b \[CirclePlus] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 150} -> 
     <|"Statement" -> HoldForm[((OverBar[a] \[CirclePlus] 
            OverBar[OverBar[a]] \[CircleTimes] a) \[CirclePlus] 
           (OverBar[OverBar[a]] \[CirclePlus] OverBar[a])) \[CircleTimes] 
          (OverBar[OverBar[a]] \[CircleTimes] a \[CirclePlus] OverBar[a]) == 
         b \[CirclePlus] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 149}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {2, 1}, "Rule" -> (a_) \[CircleTimes] (b_) -> 
          b \[CircleTimes] a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[((OverBar[a] \[CirclePlus] OverBar[OverBar[
                 a]] \[CircleTimes] a) \[CirclePlus] 
             (OverBar[OverBar[a]] \[CirclePlus] OverBar[a])) \[CircleTimes] 
            (OverBar[OverBar[a]] \[CircleTimes] a \[CirclePlus] 
             OverBar[a]) == b \[CirclePlus] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 151} -> 
     <|"Statement" -> HoldForm[((OverBar[a] \[CirclePlus] a \[CircleTimes] 
             OverBar[OverBar[a]]) \[CirclePlus] 
           (OverBar[OverBar[a]] \[CirclePlus] OverBar[a])) \[CircleTimes] 
          (OverBar[OverBar[a]] \[CircleTimes] a \[CirclePlus] OverBar[a]) == 
         b \[CirclePlus] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 150}, "Construct" -> 
         {"SubstitutionLemma", 45}, "Position" -> {1, 1}, 
        "Rule" -> (a_) \[CirclePlus] (b_) \[CircleTimes] (c_) -> 
          a \[CirclePlus] c \[CircleTimes] b, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          ((OverBar[a] \[CirclePlus] a \[CircleTimes] OverBar[OverBar[
                 a]]) \[CirclePlus] (OverBar[OverBar[a]] \[CirclePlus] 
              OverBar[a])) \[CircleTimes] (OverBar[OverBar[a]] \[CircleTimes] 
              a \[CirclePlus] OverBar[a]) == b \[CirclePlus] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 152} -> 
     <|"Statement" -> HoldForm[(OverBar[OverBar[a]] \[CircleTimes] 
            a \[CirclePlus] OverBar[a]) \[CircleTimes] 
          ((OverBar[a] \[CirclePlus] a \[CircleTimes] 
             OverBar[OverBar[a]]) \[CirclePlus] 
           (OverBar[OverBar[a]] \[CirclePlus] OverBar[a])) == 
         b \[CirclePlus] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 151}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {}, "Rule" -> (a_) \[CircleTimes] (b_) -> 
          b \[CircleTimes] a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[(OverBar[OverBar[a]] \[CircleTimes] a \[CirclePlus] 
             OverBar[a]) \[CircleTimes] ((OverBar[a] \[CirclePlus] 
              a \[CircleTimes] OverBar[OverBar[a]]) \[CirclePlus] 
             (OverBar[OverBar[a]] \[CirclePlus] OverBar[a])) == 
           b \[CirclePlus] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 153} -> 
     <|"Statement" -> HoldForm[(OverBar[OverBar[a]] \[CircleTimes] 
            a \[CirclePlus] OverBar[a]) \[CircleTimes] 
          ((OverBar[OverBar[a]] \[CirclePlus] OverBar[a]) \[CirclePlus] 
           (OverBar[a] \[CirclePlus] a \[CircleTimes] 
             OverBar[OverBar[a]])) == b \[CirclePlus] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 152}, 
        "Construct" -> {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] (c_)) -> 
          a \[CircleTimes] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(OverBar[OverBar[a]] \[CircleTimes] 
              a \[CirclePlus] OverBar[a]) \[CircleTimes] 
            ((OverBar[OverBar[a]] \[CirclePlus] OverBar[a]) \[CirclePlus] 
             (OverBar[a] \[CirclePlus] a \[CircleTimes] OverBar[
                OverBar[a]])) == b \[CirclePlus] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 154} -> 
     <|"Statement" -> HoldForm[(OverBar[OverBar[a]] \[CircleTimes] 
            a \[CirclePlus] OverBar[a]) \[CircleTimes] 
          ((OverBar[a] \[CirclePlus] OverBar[OverBar[a]]) \[CirclePlus] 
           (OverBar[a] \[CirclePlus] a \[CircleTimes] 
             OverBar[OverBar[a]])) == b \[CirclePlus] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 153}, 
        "Construct" -> {"SubstitutionLemma", 34}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CirclePlus] (b_)) \[CirclePlus] (c_) -> 
          (b \[CirclePlus] a) \[CirclePlus] c, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(OverBar[OverBar[a]] \[CircleTimes] 
              a \[CirclePlus] OverBar[a]) \[CircleTimes] 
            ((OverBar[a] \[CirclePlus] OverBar[OverBar[a]]) \[CirclePlus] 
             (OverBar[a] \[CirclePlus] a \[CircleTimes] OverBar[
                OverBar[a]])) == b \[CirclePlus] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 155} -> 
     <|"Statement" -> HoldForm[((OverBar[a] \[CirclePlus] 
            OverBar[OverBar[a]]) \[CirclePlus] (OverBar[a] \[CirclePlus] 
            a \[CircleTimes] OverBar[OverBar[a]])) \[CircleTimes] 
          (OverBar[OverBar[a]] \[CircleTimes] a \[CirclePlus] OverBar[a]) == 
         b \[CirclePlus] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 154}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {}, "Rule" -> (a_) \[CircleTimes] (b_) -> 
          b \[CircleTimes] a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[((OverBar[a] \[CirclePlus] OverBar[OverBar[
                a]]) \[CirclePlus] (OverBar[a] \[CirclePlus] 
              a \[CircleTimes] OverBar[OverBar[a]])) \[CircleTimes] 
            (OverBar[OverBar[a]] \[CircleTimes] a \[CirclePlus] 
             OverBar[a]) == b \[CirclePlus] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 156} -> 
     <|"Statement" -> HoldForm[((OverBar[a] \[CirclePlus] 
            OverBar[OverBar[a]]) \[CirclePlus] (OverBar[a] \[CirclePlus] 
            a \[CircleTimes] OverBar[OverBar[a]])) \[CircleTimes] 
          (OverBar[a] \[CirclePlus] OverBar[OverBar[a]] \[CircleTimes] a) == 
         b \[CirclePlus] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 155}, "Construct" -> 
         {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] (c_)) -> 
          a \[CircleTimes] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          ((OverBar[a] \[CirclePlus] OverBar[OverBar[a]]) \[CirclePlus] 
             (OverBar[a] \[CirclePlus] a \[CircleTimes] OverBar[
                OverBar[a]])) \[CircleTimes] (OverBar[a] \[CirclePlus] 
             OverBar[OverBar[a]] \[CircleTimes] a) == b \[CirclePlus] 
            OverBar[b]], "Source" -> "cpl"|>|>, {"SubstitutionLemma", 157} -> 
     <|"Statement" -> HoldForm[((OverBar[a] \[CirclePlus] 
            OverBar[OverBar[a]]) \[CirclePlus] (OverBar[a] \[CirclePlus] 
            a \[CircleTimes] OverBar[OverBar[a]])) \[CircleTimes] 
          (OverBar[a] \[CirclePlus] a \[CircleTimes] OverBar[OverBar[a]]) == 
         b \[CirclePlus] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 156}, "Construct" -> 
         {"SubstitutionLemma", 45}, "Position" -> {2}, 
        "Rule" -> (a_) \[CirclePlus] (b_) \[CircleTimes] (c_) -> 
          a \[CirclePlus] c \[CircleTimes] b, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          ((OverBar[a] \[CirclePlus] OverBar[OverBar[a]]) \[CirclePlus] 
             (OverBar[a] \[CirclePlus] a \[CircleTimes] OverBar[
                OverBar[a]])) \[CircleTimes] (OverBar[a] \[CirclePlus] 
             a \[CircleTimes] OverBar[OverBar[a]]) == b \[CirclePlus] 
            OverBar[b]], "Source" -> "cpl"|>|>, {"SubstitutionLemma", 158} -> 
     <|"Statement" -> HoldForm[(OverBar[a] \[CirclePlus] a \[CircleTimes] 
            OverBar[OverBar[a]]) \[CircleTimes] ((OverBar[a] \[CirclePlus] 
            OverBar[OverBar[a]]) \[CirclePlus] (OverBar[a] \[CirclePlus] 
            a \[CircleTimes] OverBar[OverBar[a]])) == b \[CirclePlus] 
          OverBar[b]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 157}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (OverBar[a] \[CirclePlus] a \[CircleTimes] OverBar[OverBar[
                a]]) \[CircleTimes] ((OverBar[a] \[CirclePlus] 
              OverBar[OverBar[a]]) \[CirclePlus] (OverBar[a] \[CirclePlus] 
              a \[CircleTimes] OverBar[OverBar[a]])) == b \[CirclePlus] 
            OverBar[b]], "Source" -> "cpl"|>|>, {"SubstitutionLemma", 159} -> 
     <|"Statement" -> HoldForm[(OverBar[a] \[CirclePlus] a \[CircleTimes] 
            OverBar[OverBar[a]]) \[CircleTimes] ((OverBar[a] \[CirclePlus] 
            a \[CircleTimes] OverBar[OverBar[a]]) \[CirclePlus] 
           (OverBar[a] \[CirclePlus] OverBar[OverBar[a]])) == 
         b \[CirclePlus] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 158}, "Construct" -> 
         {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] (c_)) -> 
          a \[CircleTimes] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(OverBar[a] \[CirclePlus] 
             a \[CircleTimes] OverBar[OverBar[a]]) \[CircleTimes] 
            ((OverBar[a] \[CirclePlus] a \[CircleTimes] OverBar[
                OverBar[a]]) \[CirclePlus] (OverBar[a] \[CirclePlus] 
              OverBar[OverBar[a]])) == b \[CirclePlus] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 160} -> 
     <|"Statement" -> HoldForm[
        (a \[CircleTimes] OverBar[OverBar[a]] \[CirclePlus] 
           OverBar[a]) \[CircleTimes] ((OverBar[a] \[CirclePlus] 
            a \[CircleTimes] OverBar[OverBar[a]]) \[CirclePlus] 
           (OverBar[a] \[CirclePlus] OverBar[OverBar[a]])) == 
         b \[CirclePlus] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 159}, "Construct" -> {"Axiom", 5}, 
        "Position" -> {1}, "Rule" -> (a_) \[CirclePlus] (b_) -> 
          b \[CirclePlus] a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[(a \[CircleTimes] OverBar[OverBar[a]] \[CirclePlus] 
             OverBar[a]) \[CircleTimes] ((OverBar[a] \[CirclePlus] 
              a \[CircleTimes] OverBar[OverBar[a]]) \[CirclePlus] 
             (OverBar[a] \[CirclePlus] OverBar[OverBar[a]])) == 
           b \[CirclePlus] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 161} -> 
     <|"Statement" -> HoldForm[
        (a \[CircleTimes] OverBar[OverBar[a]] \[CirclePlus] 
           OverBar[a]) \[CircleTimes] 
          ((a \[CircleTimes] OverBar[OverBar[a]] \[CirclePlus] 
            OverBar[a]) \[CirclePlus] (OverBar[a] \[CirclePlus] 
            OverBar[OverBar[a]])) == b \[CirclePlus] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 160}, 
        "Construct" -> {"SubstitutionLemma", 34}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CirclePlus] (b_)) \[CirclePlus] (c_) -> 
          (b \[CirclePlus] a) \[CirclePlus] c, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          (a \[CircleTimes] OverBar[OverBar[a]] \[CirclePlus] 
             OverBar[a]) \[CircleTimes] ((a \[CircleTimes] OverBar[
                OverBar[a]] \[CirclePlus] OverBar[a]) \[CirclePlus] 
             (OverBar[a] \[CirclePlus] OverBar[OverBar[a]])) == 
           b \[CirclePlus] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 162} -> 
     <|"Statement" -> HoldForm[
        (a \[CircleTimes] OverBar[OverBar[a]] \[CirclePlus] 
           OverBar[a]) \[CircleTimes] ((OverBar[a] \[CirclePlus] 
            OverBar[OverBar[a]]) \[CirclePlus] (a \[CircleTimes] 
             OverBar[OverBar[a]] \[CirclePlus] OverBar[a])) == 
         b \[CirclePlus] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 161}, "Construct" -> 
         {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] (c_)) -> 
          a \[CircleTimes] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          (a \[CircleTimes] OverBar[OverBar[a]] \[CirclePlus] 
             OverBar[a]) \[CircleTimes] ((OverBar[a] \[CirclePlus] 
              OverBar[OverBar[a]]) \[CirclePlus] (a \[CircleTimes] OverBar[
                OverBar[a]] \[CirclePlus] OverBar[a])) == 
           b \[CirclePlus] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 163} -> 
     <|"Statement" -> HoldForm[((OverBar[a] \[CirclePlus] 
            OverBar[OverBar[a]]) \[CirclePlus] (a \[CircleTimes] 
             OverBar[OverBar[a]] \[CirclePlus] OverBar[a])) \[CircleTimes] 
          (a \[CircleTimes] OverBar[OverBar[a]] \[CirclePlus] OverBar[a]) == 
         b \[CirclePlus] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 162}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {}, "Rule" -> (a_) \[CircleTimes] (b_) -> 
          b \[CircleTimes] a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[((OverBar[a] \[CirclePlus] OverBar[OverBar[
                a]]) \[CirclePlus] (a \[CircleTimes] OverBar[OverBar[
                 a]] \[CirclePlus] OverBar[a])) \[CircleTimes] 
            (a \[CircleTimes] OverBar[OverBar[a]] \[CirclePlus] 
             OverBar[a]) == b \[CirclePlus] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 164} -> 
     <|"Statement" -> HoldForm[((OverBar[OverBar[a]] \[CirclePlus] 
            OverBar[a]) \[CirclePlus] (a \[CircleTimes] 
             OverBar[OverBar[a]] \[CirclePlus] OverBar[a])) \[CircleTimes] 
          (a \[CircleTimes] OverBar[OverBar[a]] \[CirclePlus] OverBar[a]) == 
         b \[CirclePlus] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 163}, "Construct" -> 
         {"SubstitutionLemma", 34}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CirclePlus] (b_)) \[CirclePlus] (c_) -> 
          (b \[CirclePlus] a) \[CirclePlus] c, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[((OverBar[OverBar[a]] \[CirclePlus] 
              OverBar[a]) \[CirclePlus] (a \[CircleTimes] OverBar[
                OverBar[a]] \[CirclePlus] OverBar[a])) \[CircleTimes] 
            (a \[CircleTimes] OverBar[OverBar[a]] \[CirclePlus] 
             OverBar[a]) == b \[CirclePlus] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 165} -> 
     <|"Statement" -> HoldForm[
        ((a \[CircleTimes] OverBar[OverBar[a]] \[CirclePlus] 
            OverBar[a]) \[CirclePlus] (OverBar[OverBar[a]] \[CirclePlus] 
            OverBar[a])) \[CircleTimes] (a \[CircleTimes] 
            OverBar[OverBar[a]] \[CirclePlus] OverBar[a]) == 
         b \[CirclePlus] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 164}, "Construct" -> {"Axiom", 5}, 
        "Position" -> {1}, "Rule" -> (a_) \[CirclePlus] (b_) -> 
          b \[CirclePlus] a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[((a \[CircleTimes] OverBar[OverBar[a]] \[CirclePlus] 
              OverBar[a]) \[CirclePlus] (OverBar[OverBar[a]] \[CirclePlus] 
              OverBar[a])) \[CircleTimes] (a \[CircleTimes] OverBar[OverBar[
                a]] \[CirclePlus] OverBar[a]) == b \[CirclePlus] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 166} -> 
     <|"Statement" -> HoldForm[
        ((a \[CircleTimes] OverBar[OverBar[a]] \[CirclePlus] 
            OverBar[a]) \[CirclePlus] (OverBar[OverBar[a]] \[CirclePlus] 
            OverBar[a])) \[CircleTimes] (OverBar[a] \[CirclePlus] 
           a \[CircleTimes] OverBar[OverBar[a]]) == b \[CirclePlus] 
          OverBar[b]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 165}, 
        "Construct" -> {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] (c_)) -> 
          a \[CircleTimes] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          ((a \[CircleTimes] OverBar[OverBar[a]] \[CirclePlus] 
              OverBar[a]) \[CirclePlus] (OverBar[OverBar[a]] \[CirclePlus] 
              OverBar[a])) \[CircleTimes] (OverBar[a] \[CirclePlus] 
             a \[CircleTimes] OverBar[OverBar[a]]) == b \[CirclePlus] 
            OverBar[b]], "Source" -> "cpl"|>|>, {"SubstitutionLemma", 167} -> 
     <|"Statement" -> HoldForm[(OverBar[a] \[CirclePlus] a \[CircleTimes] 
            OverBar[OverBar[a]]) \[CircleTimes] 
          ((a \[CircleTimes] OverBar[OverBar[a]] \[CirclePlus] 
            OverBar[a]) \[CirclePlus] (OverBar[OverBar[a]] \[CirclePlus] 
            OverBar[a])) == b \[CirclePlus] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 166}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (OverBar[a] \[CirclePlus] a \[CircleTimes] OverBar[OverBar[
                a]]) \[CircleTimes] ((a \[CircleTimes] OverBar[OverBar[
                 a]] \[CirclePlus] OverBar[a]) \[CirclePlus] 
             (OverBar[OverBar[a]] \[CirclePlus] OverBar[a])) == 
           b \[CirclePlus] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 168} -> 
     <|"Statement" -> HoldForm[(OverBar[a] \[CirclePlus] 
           OverBar[OverBar[a]] \[CircleTimes] a) \[CircleTimes] 
          ((a \[CircleTimes] OverBar[OverBar[a]] \[CirclePlus] 
            OverBar[a]) \[CirclePlus] (OverBar[OverBar[a]] \[CirclePlus] 
            OverBar[a])) == b \[CirclePlus] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 167}, 
        "Construct" -> {"SubstitutionLemma", 45}, "Position" -> {1}, 
        "Rule" -> (a_) \[CirclePlus] (b_) \[CircleTimes] (c_) -> 
          a \[CirclePlus] c \[CircleTimes] b, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(OverBar[a] \[CirclePlus] 
             OverBar[OverBar[a]] \[CircleTimes] a) \[CircleTimes] 
            ((a \[CircleTimes] OverBar[OverBar[a]] \[CirclePlus] 
              OverBar[a]) \[CirclePlus] (OverBar[OverBar[a]] \[CirclePlus] 
              OverBar[a])) == b \[CirclePlus] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 169} -> 
     <|"Statement" -> HoldForm[(OverBar[a] \[CirclePlus] 
           OverBar[OverBar[a]] \[CircleTimes] a) \[CircleTimes] 
          ((OverBar[OverBar[a]] \[CirclePlus] OverBar[a]) \[CirclePlus] 
           (a \[CircleTimes] OverBar[OverBar[a]] \[CirclePlus] 
            OverBar[a])) == b \[CirclePlus] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 168}, 
        "Construct" -> {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] (c_)) -> 
          a \[CircleTimes] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(OverBar[a] \[CirclePlus] 
             OverBar[OverBar[a]] \[CircleTimes] a) \[CircleTimes] 
            ((OverBar[OverBar[a]] \[CirclePlus] OverBar[a]) \[CirclePlus] 
             (a \[CircleTimes] OverBar[OverBar[a]] \[CirclePlus] 
              OverBar[a])) == b \[CirclePlus] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 170} -> 
     <|"Statement" -> HoldForm[(OverBar[a] \[CirclePlus] 
           OverBar[OverBar[a]] \[CircleTimes] a) \[CircleTimes] 
          ((OverBar[OverBar[a]] \[CirclePlus] OverBar[a]) \[CirclePlus] 
           a \[CircleTimes] OverBar[OverBar[a]]) == b \[CirclePlus] 
          OverBar[b]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 169}, 
        "Construct" -> {"CriticalPairLemma", 36}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CirclePlus] (b_)) \[CirclePlus] ((c_) \[CirclePlus] 
            (b_)) -> (a \[CirclePlus] b) \[CirclePlus] c, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (OverBar[a] \[CirclePlus] OverBar[OverBar[a]] \[CircleTimes] 
              a) \[CircleTimes] ((OverBar[OverBar[a]] \[CirclePlus] 
              OverBar[a]) \[CirclePlus] a \[CircleTimes] OverBar[OverBar[
                a]]) == b \[CirclePlus] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 171} -> 
     <|"Statement" -> HoldForm[(OverBar[a] \[CirclePlus] 
           OverBar[OverBar[a]] \[CircleTimes] a) \[CircleTimes] 
          (a \[CircleTimes] OverBar[OverBar[a]] \[CirclePlus] 
           (OverBar[a] \[CirclePlus] OverBar[OverBar[a]])) == 
         b \[CirclePlus] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 170}, "Construct" -> 
         {"SubstitutionLemma", 88}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CirclePlus] (b_)) \[CirclePlus] (c_) -> 
          c \[CirclePlus] (b \[CirclePlus] a), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(OverBar[a] \[CirclePlus] 
             OverBar[OverBar[a]] \[CircleTimes] a) \[CircleTimes] 
            (a \[CircleTimes] OverBar[OverBar[a]] \[CirclePlus] 
             (OverBar[a] \[CirclePlus] OverBar[OverBar[a]])) == 
           b \[CirclePlus] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 172} -> 
     <|"Statement" -> HoldForm[
        (a \[CircleTimes] OverBar[OverBar[a]] \[CirclePlus] 
           (OverBar[a] \[CirclePlus] OverBar[OverBar[a]])) \[CircleTimes] 
          (OverBar[a] \[CirclePlus] OverBar[OverBar[a]] \[CircleTimes] a) == 
         b \[CirclePlus] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 171}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {}, "Rule" -> (a_) \[CircleTimes] (b_) -> 
          b \[CircleTimes] a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[(a \[CircleTimes] OverBar[OverBar[a]] \[CirclePlus] 
             (OverBar[a] \[CirclePlus] OverBar[OverBar[a]])) \[CircleTimes] 
            (OverBar[a] \[CirclePlus] OverBar[OverBar[a]] \[CircleTimes] 
              a) == b \[CirclePlus] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 173} -> 
     <|"Statement" -> HoldForm[
        (a \[CircleTimes] OverBar[OverBar[a]] \[CirclePlus] 
           (OverBar[a] \[CirclePlus] OverBar[OverBar[a]])) \[CircleTimes] 
          (OverBar[OverBar[a]] \[CircleTimes] a \[CirclePlus] OverBar[a]) == 
         b \[CirclePlus] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 172}, "Construct" -> 
         {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] (c_)) -> 
          a \[CircleTimes] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          (a \[CircleTimes] OverBar[OverBar[a]] \[CirclePlus] 
             (OverBar[a] \[CirclePlus] OverBar[OverBar[a]])) \[CircleTimes] 
            (OverBar[OverBar[a]] \[CircleTimes] a \[CirclePlus] 
             OverBar[a]) == b \[CirclePlus] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 174} -> 
     <|"Statement" -> HoldForm[
        (a \[CircleTimes] OverBar[OverBar[a]] \[CirclePlus] 
           (OverBar[a] \[CirclePlus] OverBar[OverBar[a]])) \[CircleTimes] 
          (a \[CircleTimes] a \[CirclePlus] OverBar[a]) == 
         b \[CirclePlus] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 173}, "Construct" -> 
         {"SubstitutionLemma", 7}, "Position" -> {2, 1, 1}, 
        "Rule" -> OverBar[OverBar[a_]] -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          (a \[CircleTimes] OverBar[OverBar[a]] \[CirclePlus] 
             (OverBar[a] \[CirclePlus] OverBar[OverBar[a]])) \[CircleTimes] 
            (a \[CircleTimes] a \[CirclePlus] OverBar[a]) == 
           b \[CirclePlus] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 175} -> 
     <|"Statement" -> HoldForm[(a \[CircleTimes] a \[CirclePlus] 
           OverBar[a]) \[CircleTimes] (a \[CircleTimes] 
            OverBar[OverBar[a]] \[CirclePlus] (OverBar[a] \[CirclePlus] 
            OverBar[OverBar[a]])) == b \[CirclePlus] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 174}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (a \[CircleTimes] a \[CirclePlus] OverBar[a]) \[CircleTimes] 
            (a \[CircleTimes] OverBar[OverBar[a]] \[CirclePlus] 
             (OverBar[a] \[CirclePlus] OverBar[OverBar[a]])) == 
           b \[CirclePlus] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 176} -> 
     <|"Statement" -> HoldForm[(a \[CircleTimes] a \[CirclePlus] 
           OverBar[a]) \[CircleTimes] (OverBar[OverBar[a]] \[CirclePlus] 
           (a \[CircleTimes] OverBar[OverBar[a]] \[CirclePlus] 
            OverBar[a])) == b \[CirclePlus] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 175}, 
        "Construct" -> {"SubstitutionLemma", 97}, "Position" -> {2}, 
        "Rule" -> (a_) \[CirclePlus] ((b_) \[CirclePlus] (c_)) -> 
          c \[CirclePlus] (a \[CirclePlus] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(a \[CircleTimes] a \[CirclePlus] 
             OverBar[a]) \[CircleTimes] (OverBar[OverBar[a]] \[CirclePlus] 
             (a \[CircleTimes] OverBar[OverBar[a]] \[CirclePlus] 
              OverBar[a])) == b \[CirclePlus] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 177} -> 
     <|"Statement" -> HoldForm[(a \[CircleTimes] a \[CirclePlus] 
           OverBar[a]) \[CircleTimes] (OverBar[a] \[CirclePlus] 
           (OverBar[OverBar[a]] \[CirclePlus] a \[CircleTimes] 
             OverBar[OverBar[a]])) == b \[CirclePlus] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 176}, 
        "Construct" -> {"SubstitutionLemma", 97}, "Position" -> {2}, 
        "Rule" -> (a_) \[CirclePlus] ((b_) \[CirclePlus] (c_)) -> 
          c \[CirclePlus] (a \[CirclePlus] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(a \[CircleTimes] a \[CirclePlus] 
             OverBar[a]) \[CircleTimes] (OverBar[a] \[CirclePlus] 
             (OverBar[OverBar[a]] \[CirclePlus] a \[CircleTimes] OverBar[
                OverBar[a]])) == b \[CirclePlus] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 178} -> 
     <|"Statement" -> HoldForm[(OverBar[a] \[CirclePlus] 
           (OverBar[OverBar[a]] \[CirclePlus] a \[CircleTimes] 
             OverBar[OverBar[a]])) \[CircleTimes] 
          (a \[CircleTimes] a \[CirclePlus] OverBar[a]) == 
         b \[CirclePlus] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 177}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {}, "Rule" -> (a_) \[CircleTimes] (b_) -> 
          b \[CircleTimes] a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[(OverBar[a] \[CirclePlus] (OverBar[OverBar[
                a]] \[CirclePlus] a \[CircleTimes] OverBar[OverBar[
                 a]])) \[CircleTimes] (a \[CircleTimes] a \[CirclePlus] 
             OverBar[a]) == b \[CirclePlus] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 179} -> 
     <|"Statement" -> HoldForm[(OverBar[a] \[CirclePlus] 
           (OverBar[OverBar[a]] \[CirclePlus] a \[CircleTimes] 
             OverBar[OverBar[a]])) \[CircleTimes] (OverBar[a] \[CirclePlus] 
           a \[CircleTimes] a) == b \[CirclePlus] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 178}, 
        "Construct" -> {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] (c_)) -> 
          a \[CircleTimes] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(OverBar[a] \[CirclePlus] 
             (OverBar[OverBar[a]] \[CirclePlus] a \[CircleTimes] OverBar[
                OverBar[a]])) \[CircleTimes] (OverBar[a] \[CirclePlus] 
             a \[CircleTimes] a) == b \[CirclePlus] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 180} -> 
     <|"Statement" -> HoldForm[
        (a \[CircleTimes] OverBar[OverBar[a]] \[CirclePlus] 
           (OverBar[OverBar[a]] \[CirclePlus] OverBar[a])) \[CircleTimes] 
          (OverBar[a] \[CirclePlus] a \[CircleTimes] a) == 
         b \[CirclePlus] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 179}, "Construct" -> 
         {"SubstitutionLemma", 38}, "Position" -> {1}, 
        "Rule" -> (a_) \[CirclePlus] ((b_) \[CirclePlus] (c_)) -> 
          c \[CirclePlus] (b \[CirclePlus] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          (a \[CircleTimes] OverBar[OverBar[a]] \[CirclePlus] 
             (OverBar[OverBar[a]] \[CirclePlus] OverBar[a])) \[CircleTimes] 
            (OverBar[a] \[CirclePlus] a \[CircleTimes] a) == 
           b \[CirclePlus] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 181} -> 
     <|"Statement" -> HoldForm[(OverBar[a] \[CirclePlus] a \[CircleTimes] 
            a) \[CircleTimes] (a \[CircleTimes] OverBar[
             OverBar[a]] \[CirclePlus] (OverBar[OverBar[a]] \[CirclePlus] 
            OverBar[a])) == b \[CirclePlus] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 180}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (OverBar[a] \[CirclePlus] a \[CircleTimes] a) \[CircleTimes] 
            (a \[CircleTimes] OverBar[OverBar[a]] \[CirclePlus] 
             (OverBar[OverBar[a]] \[CirclePlus] OverBar[a])) == 
           b \[CirclePlus] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 182} -> 
     <|"Statement" -> HoldForm[(OverBar[a] \[CirclePlus] a \[CircleTimes] 
            a) \[CircleTimes] (OverBar[a] \[CirclePlus] 
           (a \[CircleTimes] OverBar[OverBar[a]] \[CirclePlus] 
            OverBar[OverBar[a]])) == b \[CirclePlus] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 181}, 
        "Construct" -> {"SubstitutionLemma", 97}, "Position" -> {2}, 
        "Rule" -> (a_) \[CirclePlus] ((b_) \[CirclePlus] (c_)) -> 
          c \[CirclePlus] (a \[CirclePlus] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(OverBar[a] \[CirclePlus] 
             a \[CircleTimes] a) \[CircleTimes] (OverBar[a] \[CirclePlus] 
             (a \[CircleTimes] OverBar[OverBar[a]] \[CirclePlus] 
              OverBar[OverBar[a]])) == b \[CirclePlus] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 183} -> 
     <|"Statement" -> HoldForm[(OverBar[a] \[CirclePlus] a \[CircleTimes] 
            a) \[CircleTimes] (OverBar[OverBar[a]] \[CirclePlus] 
           (OverBar[a] \[CirclePlus] a \[CircleTimes] 
             OverBar[OverBar[a]])) == b \[CirclePlus] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 182}, 
        "Construct" -> {"SubstitutionLemma", 97}, "Position" -> {2}, 
        "Rule" -> (a_) \[CirclePlus] ((b_) \[CirclePlus] (c_)) -> 
          c \[CirclePlus] (a \[CirclePlus] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(OverBar[a] \[CirclePlus] 
             a \[CircleTimes] a) \[CircleTimes] 
            (OverBar[OverBar[a]] \[CirclePlus] (OverBar[a] \[CirclePlus] 
              a \[CircleTimes] OverBar[OverBar[a]])) == b \[CirclePlus] 
            OverBar[b]], "Source" -> "cpl"|>|>, {"SubstitutionLemma", 184} -> 
     <|"Statement" -> HoldForm[(OverBar[OverBar[a]] \[CirclePlus] 
           (OverBar[a] \[CirclePlus] a \[CircleTimes] 
             OverBar[OverBar[a]])) \[CircleTimes] (OverBar[a] \[CirclePlus] 
           a \[CircleTimes] a) == b \[CirclePlus] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 183}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (OverBar[OverBar[a]] \[CirclePlus] (OverBar[a] \[CirclePlus] 
              a \[CircleTimes] OverBar[OverBar[a]])) \[CircleTimes] 
            (OverBar[a] \[CirclePlus] a \[CircleTimes] a) == 
           b \[CirclePlus] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 185} -> 
     <|"Statement" -> HoldForm[(OverBar[OverBar[a]] \[CirclePlus] 
           (OverBar[a] \[CirclePlus] OverBar[OverBar[a]])) \[CircleTimes] 
          (OverBar[a] \[CirclePlus] a \[CircleTimes] a) == 
         b \[CirclePlus] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 184}, "Construct" -> 
         {"CriticalPairLemma", 44}, "Position" -> {1, 2}, 
        "Rule" -> OverBar[a_] \[CirclePlus] (a_) \[CircleTimes] (b_) -> 
          OverBar[a] \[CirclePlus] b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(OverBar[OverBar[a]] \[CirclePlus] 
             (OverBar[a] \[CirclePlus] OverBar[OverBar[a]])) \[CircleTimes] 
            (OverBar[a] \[CirclePlus] a \[CircleTimes] a) == 
           b \[CirclePlus] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 186} -> 
     <|"Statement" -> HoldForm[(OverBar[OverBar[a]] \[CirclePlus] 
           (OverBar[a] \[CirclePlus] OverBar[OverBar[a]])) \[CircleTimes] 
          (a \[CircleTimes] a \[CirclePlus] OverBar[a]) == 
         b \[CirclePlus] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 185}, "Construct" -> 
         {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] (c_)) -> 
          a \[CircleTimes] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(OverBar[OverBar[a]] \[CirclePlus] 
             (OverBar[a] \[CirclePlus] OverBar[OverBar[a]])) \[CircleTimes] 
            (a \[CircleTimes] a \[CirclePlus] OverBar[a]) == 
           b \[CirclePlus] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 187} -> 
     <|"Statement" -> HoldForm[(a \[CircleTimes] a \[CirclePlus] 
           OverBar[a]) \[CircleTimes] (OverBar[OverBar[a]] \[CirclePlus] 
           (OverBar[a] \[CirclePlus] OverBar[OverBar[a]])) == 
         b \[CirclePlus] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 186}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {}, "Rule" -> (a_) \[CircleTimes] (b_) -> 
          b \[CircleTimes] a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[(a \[CircleTimes] a \[CirclePlus] OverBar[
              a]) \[CircleTimes] (OverBar[OverBar[a]] \[CirclePlus] 
             (OverBar[a] \[CirclePlus] OverBar[OverBar[a]])) == 
           b \[CirclePlus] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 188} -> 
     <|"Statement" -> HoldForm[(a \[CircleTimes] a \[CirclePlus] 
           OverBar[a]) \[CircleTimes] (OverBar[OverBar[a]] \[CirclePlus] 
           (b \[CirclePlus] OverBar[b])) == b \[CirclePlus] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 187}, 
        "Construct" -> {"CriticalPairLemma", 45}, "Position" -> {2, 2}, 
        "Rule" -> (a_) \[CirclePlus] OverBar[a_] -> b \[CirclePlus] 
           OverBar[b], "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[(a \[CircleTimes] a \[CirclePlus] OverBar[
              a]) \[CircleTimes] (OverBar[OverBar[a]] \[CirclePlus] 
             (b \[CirclePlus] OverBar[b])) == b \[CirclePlus] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 189} -> 
     <|"Statement" -> HoldForm[(a \[CircleTimes] a \[CirclePlus] 
           OverBar[a]) \[CircleTimes] (OverBar[b] \[CirclePlus] 
           (OverBar[OverBar[a]] \[CirclePlus] b)) == b \[CirclePlus] 
          OverBar[b]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 188}, 
        "Construct" -> {"SubstitutionLemma", 97}, "Position" -> {2}, 
        "Rule" -> (a_) \[CirclePlus] ((b_) \[CirclePlus] (c_)) -> 
          c \[CirclePlus] (a \[CirclePlus] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(a \[CircleTimes] a \[CirclePlus] 
             OverBar[a]) \[CircleTimes] (OverBar[b] \[CirclePlus] 
             (OverBar[OverBar[a]] \[CirclePlus] b)) == b \[CirclePlus] 
            OverBar[b]], "Source" -> "cpl"|>|>, {"SubstitutionLemma", 190} -> 
     <|"Statement" -> HoldForm[(OverBar[b] \[CirclePlus] 
           (OverBar[OverBar[a]] \[CirclePlus] b)) \[CircleTimes] 
          (a \[CircleTimes] a \[CirclePlus] OverBar[a]) == 
         b \[CirclePlus] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 189}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {}, "Rule" -> (a_) \[CircleTimes] (b_) -> 
          b \[CircleTimes] a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[(OverBar[b] \[CirclePlus] (OverBar[OverBar[
                a]] \[CirclePlus] b)) \[CircleTimes] 
            (a \[CircleTimes] a \[CirclePlus] OverBar[a]) == 
           b \[CirclePlus] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 191} -> 
     <|"Statement" -> HoldForm[(OverBar[b] \[CirclePlus] 
           (OverBar[OverBar[a]] \[CirclePlus] b)) \[CircleTimes] 
          (OverBar[a] \[CirclePlus] a \[CircleTimes] a) == 
         b \[CirclePlus] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 190}, "Construct" -> 
         {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] (c_)) -> 
          a \[CircleTimes] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(OverBar[b] \[CirclePlus] 
             (OverBar[OverBar[a]] \[CirclePlus] b)) \[CircleTimes] 
            (OverBar[a] \[CirclePlus] a \[CircleTimes] a) == 
           b \[CirclePlus] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 192} -> 
     <|"Statement" -> HoldForm[(OverBar[b] \[CirclePlus] 
           (OverBar[OverBar[a]] \[CirclePlus] b)) \[CircleTimes] 
          (OverBar[a] \[CirclePlus] a) == b \[CirclePlus] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 191}, 
        "Construct" -> {"CriticalPairLemma", 14}, "Position" -> {2}, 
        "Rule" -> OverBar[a_] \[CirclePlus] (b_) \[CircleTimes] (a_) -> 
          OverBar[a] \[CirclePlus] b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(OverBar[b] \[CirclePlus] 
             (OverBar[OverBar[a]] \[CirclePlus] b)) \[CircleTimes] 
            (OverBar[a] \[CirclePlus] a) == b \[CirclePlus] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 193} -> 
     <|"Statement" -> HoldForm[(OverBar[a] \[CirclePlus] a) \[CircleTimes] 
          (OverBar[b] \[CirclePlus] (OverBar[OverBar[a]] \[CirclePlus] b)) == 
         b \[CirclePlus] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 192}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {}, "Rule" -> (a_) \[CircleTimes] (b_) -> 
          b \[CircleTimes] a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[(OverBar[a] \[CirclePlus] a) \[CircleTimes] 
            (OverBar[b] \[CirclePlus] (OverBar[OverBar[a]] \[CirclePlus] 
              b)) == b \[CirclePlus] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 194} -> 
     <|"Statement" -> HoldForm[(OverBar[a] \[CirclePlus] a) \[CircleTimes] 
          (b \[CirclePlus] (OverBar[b] \[CirclePlus] OverBar[OverBar[a]])) == 
         b \[CirclePlus] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 193}, "Construct" -> 
         {"SubstitutionLemma", 97}, "Position" -> {2}, 
        "Rule" -> (a_) \[CirclePlus] ((b_) \[CirclePlus] (c_)) -> 
          c \[CirclePlus] (a \[CirclePlus] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(OverBar[a] \[CirclePlus] 
             a) \[CircleTimes] (b \[CirclePlus] (OverBar[b] \[CirclePlus] 
              OverBar[OverBar[a]])) == b \[CirclePlus] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 195} -> 
     <|"Statement" -> HoldForm[(OverBar[a] \[CirclePlus] a) \[CircleTimes] 
          (OverBar[OverBar[a]] \[CirclePlus] (b \[CirclePlus] OverBar[b])) == 
         b \[CirclePlus] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 194}, "Construct" -> 
         {"SubstitutionLemma", 97}, "Position" -> {2}, 
        "Rule" -> (a_) \[CirclePlus] ((b_) \[CirclePlus] (c_)) -> 
          c \[CirclePlus] (a \[CirclePlus] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(OverBar[a] \[CirclePlus] 
             a) \[CircleTimes] (OverBar[OverBar[a]] \[CirclePlus] 
             (b \[CirclePlus] OverBar[b])) == b \[CirclePlus] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 196} -> 
     <|"Statement" -> HoldForm[(OverBar[OverBar[a]] \[CirclePlus] 
           (b \[CirclePlus] OverBar[b])) \[CircleTimes] 
          (OverBar[a] \[CirclePlus] a) == b \[CirclePlus] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 195}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (OverBar[OverBar[a]] \[CirclePlus] (b \[CirclePlus] 
              OverBar[b])) \[CircleTimes] (OverBar[a] \[CirclePlus] a) == 
           b \[CirclePlus] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 197} -> 
     <|"Statement" -> HoldForm[(OverBar[OverBar[a]] \[CirclePlus] 
           (b \[CirclePlus] OverBar[b])) \[CircleTimes] (a \[CirclePlus] 
           OverBar[a]) == b \[CirclePlus] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 196}, 
        "Construct" -> {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] (c_)) -> 
          a \[CircleTimes] (c \[CirclePlus] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(OverBar[OverBar[a]] \[CirclePlus] 
             (b \[CirclePlus] OverBar[b])) \[CircleTimes] (a \[CirclePlus] 
             OverBar[a]) == b \[CirclePlus] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 198} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[a]] \[CirclePlus] 
          (b \[CirclePlus] OverBar[b]) == b \[CirclePlus] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 197}, 
        "Construct" -> {"Axiom", 4}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] OverBar[b_]) -> a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          OverBar[OverBar[a]] \[CirclePlus] (b \[CirclePlus] OverBar[b]) == 
           b \[CirclePlus] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"Conclusion", 1} -> <|"Statement" -> 
       HoldForm[b \[CirclePlus] OverBar[b] == b \[CirclePlus] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 198}, 
        "Construct" -> {"CriticalPairLemma", 46}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] ((b_) \[CirclePlus] OverBar[b_]) -> 
          b \[CirclePlus] OverBar[b], "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[b \[CirclePlus] OverBar[b] == 
           b \[CirclePlus] OverBar[b]], "Source" -> "cpl"|>|>}|>]
