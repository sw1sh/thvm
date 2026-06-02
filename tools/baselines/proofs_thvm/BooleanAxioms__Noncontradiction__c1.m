ProofObject["EquationalLogic", Inactive[Equal][OverBar[a] \[CircleTimes] a, 
  OverBar[b] \[CircleTimes] b], {Inactive[Equal][(a_) \[CircleTimes] (b_), 
   (b_) \[CircleTimes] (a_)], Inactive[Equal][(a_) \[CircleTimes] 
    ((b_) \[CirclePlus] (c_)), (a_) \[CircleTimes] (b_) \[CirclePlus] 
    (a_) \[CircleTimes] (c_)], Inactive[Equal][
   (a_) \[CirclePlus] (b_) \[CircleTimes] OverBar[b_], a_], 
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
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         OverBar[b] \[CircleTimes] b], "Proof" -> <||>|>, 
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
     <|"Statement" -> HoldForm[a \[CircleTimes] OverBar[OverBar[a]] == a], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 2}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CircleTimes] 
           (OverBar[a_] \[CirclePlus] (b_)) -> a \[CircleTimes] b, 
        "Side" -> 1, "Subpattern" -> {}, "MatchingConstruct" -> {"Axiom", 4}, 
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
     <|"Statement" -> HoldForm[a \[CircleTimes] a == a], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 4}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] 
            OverBar[a_]) -> a \[CircleTimes] b, "Side" -> 1, 
        "Subpattern" -> {}, "MatchingConstruct" -> {"Axiom", 4}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] ((b_) \[CirclePlus] OverBar[b_]) -> a, 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"CriticalPairLemma", 6} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CirclePlus] a) == 
         a \[CircleTimes] b \[CirclePlus] a], 
      "Proof" -> <|"Construct" -> {"Axiom", 2}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CircleTimes] (b_) \[CirclePlus] (a_) \[CircleTimes] 
            (c_) -> a \[CircleTimes] (b \[CirclePlus] c), "Side" -> 1, 
        "Subpattern" -> (a_) \[CircleTimes] (c_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 5}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CircleTimes] (a_) -> a, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 3} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CirclePlus] a) == 
         a \[CirclePlus] a \[CircleTimes] b], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 6}, 
        "Construct" -> {"Axiom", 5}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CircleTimes] (b \[CirclePlus] a) == a \[CirclePlus] 
            a \[CircleTimes] b], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 7} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (a \[CirclePlus] b) == 
         a \[CirclePlus] a \[CircleTimes] b], 
      "Proof" -> <|"Construct" -> {"Axiom", 2}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CircleTimes] (b_) \[CirclePlus] (a_) \[CircleTimes] 
            (c_) -> a \[CircleTimes] (b \[CirclePlus] c), "Side" -> 1, 
        "Subpattern" -> (a_) \[CircleTimes] (b_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 5}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CircleTimes] (a_) -> a, "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 4} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CirclePlus] a) == 
         a \[CircleTimes] (a \[CirclePlus] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 3}, 
        "Construct" -> {"CriticalPairLemma", 7}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] (a_) \[CircleTimes] (b_) -> 
          a \[CircleTimes] (a \[CirclePlus] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CircleTimes] (b \[CirclePlus] 
             a) == a \[CircleTimes] (a \[CirclePlus] b)], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 8} -> 
     <|"Statement" -> HoldForm[b == (a \[CirclePlus] 
           OverBar[a]) \[CircleTimes] b], "Proof" -> 
       <|"Construct" -> {"Axiom", 4}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] OverBar[b_]) -> a, 
        "Side" -> 1, "Subpattern" -> {}, "MatchingConstruct" -> {"Axiom", 1}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 9} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] OverBar[a] \[CircleTimes] b == 
         a \[CirclePlus] b], "Proof" -> <|"Construct" -> {"Axiom", 6}, 
        "Orientation" -> -1, "Rule" -> 
         ((a_) \[CirclePlus] (b_)) \[CircleTimes] ((a_) \[CirclePlus] 
            (c_)) -> a \[CirclePlus] b \[CircleTimes] c, "Side" -> 1, 
        "Subpattern" -> {}, "MatchingConstruct" -> {"CriticalPairLemma", 8}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CirclePlus] OverBar[a_]) \[CircleTimes] (b_) -> b, 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"CriticalPairLemma", 10} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] (b \[CirclePlus] 
           OverBar[OverBar[a]]) == a \[CirclePlus] OverBar[a] \[CircleTimes] 
           b], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 9}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CirclePlus] 
           OverBar[a_] \[CircleTimes] (b_) -> a \[CirclePlus] b, "Side" -> 1, 
        "Subpattern" -> OverBar[a_] \[CircleTimes] (b_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 4}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] ((b_) \[CirclePlus] OverBar[a_]) -> 
          a \[CircleTimes] b, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 5} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] (b \[CirclePlus] 
           OverBar[OverBar[a]]) == a \[CirclePlus] b], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 10}, 
        "Construct" -> {"CriticalPairLemma", 9}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] OverBar[a_] \[CircleTimes] (b_) -> 
          a \[CirclePlus] b, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[a \[CirclePlus] (b \[CirclePlus] OverBar[OverBar[a]]) == 
           a \[CirclePlus] b], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 11} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] OverBar[
           OverBar[OverBar[a]]] == a \[CirclePlus] OverBar[a]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 9}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CirclePlus] 
           OverBar[a_] \[CircleTimes] (b_) -> a \[CirclePlus] b, "Side" -> 1, 
        "Subpattern" -> OverBar[a_] \[CircleTimes] (b_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 3}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] OverBar[OverBar[a_]] -> a, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 12} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[a]] \[CircleTimes] a == 
         OverBar[OverBar[a]] \[CircleTimes] (a \[CirclePlus] OverBar[a])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 4}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] 
            OverBar[a_]) -> a \[CircleTimes] b, "Side" -> 1, 
        "Subpattern" -> (b_) \[CirclePlus] OverBar[a_], 
        "MatchingConstruct" -> {"CriticalPairLemma", 11}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CirclePlus] OverBar[OverBar[OverBar[a_]]] -> 
          a \[CirclePlus] OverBar[a], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 6} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[a]] \[CircleTimes] a == 
         OverBar[OverBar[a]]], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 12}, "Construct" -> {"Axiom", 4}, 
        "Position" -> {}, "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] 
            OverBar[b_]) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[OverBar[OverBar[a]] \[CircleTimes] a == 
           OverBar[OverBar[a]]], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 7} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] OverBar[OverBar[a]] == 
         OverBar[OverBar[a]]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 6}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {}, "Rule" -> (a_) \[CircleTimes] (b_) -> 
          b \[CircleTimes] a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[a \[CircleTimes] OverBar[OverBar[a]] == 
           OverBar[OverBar[a]]], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 8} -> 
     <|"Statement" -> HoldForm[a == OverBar[OverBar[a]]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 7}, 
        "Construct" -> {"CriticalPairLemma", 3}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] OverBar[OverBar[a_]] -> a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a == OverBar[OverBar[a]]], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 9} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] (b \[CirclePlus] a) == 
         a \[CirclePlus] b], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 5}, "Construct" -> 
         {"SubstitutionLemma", 8}, "Position" -> {2, 2}, 
        "Rule" -> OverBar[OverBar[a_]] -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CirclePlus] (b \[CirclePlus] a) == 
           a \[CirclePlus] b], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 13} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] b \[CircleTimes] 
           (c \[CirclePlus] a) == (a \[CirclePlus] b) \[CircleTimes] 
          (a \[CirclePlus] c)], "Proof" -> <|"Construct" -> {"Axiom", 6}, 
        "Orientation" -> -1, "Rule" -> 
         ((a_) \[CirclePlus] (b_)) \[CircleTimes] ((a_) \[CirclePlus] 
            (c_)) -> a \[CirclePlus] b \[CircleTimes] c, "Side" -> 1, 
        "Subpattern" -> (a_) \[CirclePlus] (c_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 9}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CirclePlus] ((b_) \[CirclePlus] (a_)) -> 
          a \[CirclePlus] b, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 10} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] b \[CircleTimes] 
           (c \[CirclePlus] a) == a \[CirclePlus] b \[CircleTimes] c], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 13}, 
        "Construct" -> {"Axiom", 6}, "Position" -> {}, 
        "Rule" -> ((a_) \[CirclePlus] (b_)) \[CircleTimes] 
           ((a_) \[CirclePlus] (c_)) -> a \[CirclePlus] b \[CircleTimes] c, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CirclePlus] b \[CircleTimes] (c \[CirclePlus] a) == 
           a \[CirclePlus] b \[CircleTimes] c], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 14} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] b \[CircleTimes] OverBar[b] == 
         a \[CirclePlus] b \[CircleTimes] a], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 10}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CirclePlus] (b_) \[CircleTimes] 
            ((c_) \[CirclePlus] (a_)) -> a \[CirclePlus] b \[CircleTimes] c, 
        "Side" -> 1, "Subpattern" -> (b_) \[CircleTimes] ((c_) \[CirclePlus] 
           (a_)), "MatchingConstruct" -> {"CriticalPairLemma", 2}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] (OverBar[a_] \[CirclePlus] (b_)) -> 
          a \[CircleTimes] b, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 15} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (a \[CirclePlus] b) == 
         a \[CirclePlus] b \[CircleTimes] a], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 7}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CirclePlus] (a_) \[CircleTimes] 
            (b_) -> a \[CircleTimes] (a \[CirclePlus] b), "Side" -> 1, 
        "Subpattern" -> (a_) \[CircleTimes] (b_), "MatchingConstruct" -> 
         {"Axiom", 1}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 11} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] b \[CircleTimes] OverBar[b] == 
         a \[CircleTimes] (a \[CirclePlus] b)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 14}, 
        "Construct" -> {"CriticalPairLemma", 15}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] (b_) \[CircleTimes] (a_) -> 
          a \[CircleTimes] (a \[CirclePlus] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CirclePlus] b \[CircleTimes] 
             OverBar[b] == a \[CircleTimes] (a \[CirclePlus] b)], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 12} -> 
     <|"Statement" -> HoldForm[a == a \[CircleTimes] (a \[CirclePlus] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 11}, 
        "Construct" -> {"Axiom", 3}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] (b_) \[CircleTimes] OverBar[b_] -> a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a == a \[CircleTimes] (a \[CirclePlus] b)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 13} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CirclePlus] a) == a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 4}, 
        "Construct" -> {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((a_) \[CirclePlus] (b_)) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CircleTimes] (b \[CirclePlus] a) == a], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 16} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CirclePlus] 
           (c \[CirclePlus] a)) == a \[CircleTimes] b \[CirclePlus] a], 
      "Proof" -> <|"Construct" -> {"Axiom", 2}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CircleTimes] (b_) \[CirclePlus] (a_) \[CircleTimes] 
            (c_) -> a \[CircleTimes] (b \[CirclePlus] c), "Side" -> 1, 
        "Subpattern" -> (a_) \[CircleTimes] (c_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 13}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] (a_)) -> a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 14} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CirclePlus] 
           (c \[CirclePlus] a)) == a \[CirclePlus] a \[CircleTimes] b], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 16}, 
        "Construct" -> {"Axiom", 5}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CircleTimes] (b \[CirclePlus] (c \[CirclePlus] a)) == 
           a \[CirclePlus] a \[CircleTimes] b], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 15} -> 
     <|"Statement" -> HoldForm[a == a \[CirclePlus] a \[CircleTimes] b], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 7}, 
        "Construct" -> {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((a_) \[CirclePlus] (b_)) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a == a \[CirclePlus] a \[CircleTimes] b], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 16} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CirclePlus] 
           (c \[CirclePlus] a)) == a], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 14}, "Construct" -> 
         {"SubstitutionLemma", 15}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] (a_) \[CircleTimes] (b_) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CircleTimes] (b \[CirclePlus] (c \[CirclePlus] a)) == a], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 17} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] b == 
         (a \[CircleTimes] b) \[CircleTimes] (c \[CirclePlus] a)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 16}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] 
            ((c_) \[CirclePlus] (a_))) -> a, "Side" -> 1, 
        "Subpattern" -> (c_) \[CirclePlus] (a_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 15}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CirclePlus] (a_) \[CircleTimes] (b_) -> a, 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"CriticalPairLemma", 18} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] 
          (a \[CircleTimes] b) \[CircleTimes] c == a \[CirclePlus] 
          a \[CircleTimes] b], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 10}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CirclePlus] (b_) \[CircleTimes] ((c_) \[CirclePlus] 
             (a_)) -> a \[CirclePlus] b \[CircleTimes] c, "Side" -> 1, 
        "Subpattern" -> (b_) \[CircleTimes] ((c_) \[CirclePlus] (a_)), 
        "MatchingConstruct" -> {"CriticalPairLemma", 17}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CircleTimes] (b_)) \[CircleTimes] ((c_) \[CirclePlus] 
            (a_)) -> a \[CircleTimes] b, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 17} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] 
          (a \[CircleTimes] b) \[CircleTimes] c == a], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 18}, 
        "Construct" -> {"SubstitutionLemma", 15}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] (a_) \[CircleTimes] (b_) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CirclePlus] (a \[CircleTimes] b) \[CircleTimes] c == a], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 19} -> 
     <|"Statement" -> HoldForm[a == a \[CirclePlus] 
          (b \[CircleTimes] a) \[CircleTimes] c], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 17}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CirclePlus] 
           ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> a, "Side" -> 1, 
        "Subpattern" -> (a_) \[CircleTimes] (b_), "MatchingConstruct" -> 
         {"Axiom", 1}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, "MatchingSide" -> 1, 
        "Position" -> {2, 1}|>|>, {"CriticalPairLemma", 20} -> 
     <|"Statement" -> HoldForm[(a \[CircleTimes] b) \[CircleTimes] c == 
         ((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] b], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 13}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] 
            (a_)) -> a, "Side" -> 1, "Subpattern" -> (b_) \[CirclePlus] (a_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 19}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CirclePlus] ((b_) \[CircleTimes] (a_)) \[CircleTimes] (c_) -> 
          a, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 18} -> 
     <|"Statement" -> HoldForm[(a \[CircleTimes] b) \[CircleTimes] c == 
         b \[CircleTimes] ((a \[CircleTimes] b) \[CircleTimes] c)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 20}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (a \[CircleTimes] b) \[CircleTimes] c == b \[CircleTimes] 
            ((a \[CircleTimes] b) \[CircleTimes] c)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 21} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] b \[CircleTimes] OverBar[a] == 
         a \[CirclePlus] b], "Proof" -> <|"Construct" -> {"Axiom", 6}, 
        "Orientation" -> -1, "Rule" -> 
         ((a_) \[CirclePlus] (b_)) \[CircleTimes] ((a_) \[CirclePlus] 
            (c_)) -> a \[CirclePlus] b \[CircleTimes] c, "Side" -> 1, 
        "Subpattern" -> {}, "MatchingConstruct" -> {"Axiom", 4}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] ((b_) \[CirclePlus] OverBar[b_]) -> a, 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"CriticalPairLemma", 22} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] 
          (b \[CircleTimes] OverBar[a]) \[CircleTimes] c == 
         (a \[CirclePlus] b) \[CircleTimes] (a \[CirclePlus] c)], 
      "Proof" -> <|"Construct" -> {"Axiom", 6}, "Orientation" -> -1, 
        "Rule" -> ((a_) \[CirclePlus] (b_)) \[CircleTimes] 
           ((a_) \[CirclePlus] (c_)) -> a \[CirclePlus] b \[CircleTimes] c, 
        "Side" -> 1, "Subpattern" -> (a_) \[CirclePlus] (b_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 21}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CirclePlus] (b_) \[CircleTimes] OverBar[a_] -> 
          a \[CirclePlus] b, "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"SubstitutionLemma", 19} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] 
          (b \[CircleTimes] OverBar[a]) \[CircleTimes] c == 
         a \[CirclePlus] b \[CircleTimes] c], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 22}, 
        "Construct" -> {"Axiom", 6}, "Position" -> {}, 
        "Rule" -> ((a_) \[CirclePlus] (b_)) \[CircleTimes] 
           ((a_) \[CirclePlus] (c_)) -> a \[CirclePlus] b \[CircleTimes] c, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CirclePlus] (b \[CircleTimes] OverBar[a]) \[CircleTimes] c == 
           a \[CirclePlus] b \[CircleTimes] c], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 23} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] 
          ((b \[CircleTimes] OverBar[OverBar[a]]) \[CircleTimes] c) == 
         a \[CircleTimes] (OverBar[a] \[CirclePlus] b \[CircleTimes] c)], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 2}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CircleTimes] 
           (OverBar[a_] \[CirclePlus] (b_)) -> a \[CircleTimes] b, 
        "Side" -> 1, "Subpattern" -> OverBar[a_] \[CirclePlus] (b_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 19}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CirclePlus] ((b_) \[CircleTimes] OverBar[a_]) \[CircleTimes] 
            (c_) -> a \[CirclePlus] b \[CircleTimes] c, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 20} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] 
          ((b \[CircleTimes] OverBar[OverBar[a]]) \[CircleTimes] c) == 
         a \[CircleTimes] (b \[CircleTimes] c)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 23}, 
        "Construct" -> {"CriticalPairLemma", 2}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (OverBar[a_] \[CirclePlus] (b_)) -> 
          a \[CircleTimes] b, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[a \[CircleTimes] ((b \[CircleTimes] OverBar[OverBar[
                a]]) \[CircleTimes] c) == a \[CircleTimes] (b \[CircleTimes] 
             c)], "Source" -> "norm"|>|>, {"SubstitutionLemma", 21} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] 
          ((b \[CircleTimes] a) \[CircleTimes] c) == a \[CircleTimes] 
          (b \[CircleTimes] c)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 20}, "Construct" -> 
         {"SubstitutionLemma", 8}, "Position" -> {2, 1, 2}, 
        "Rule" -> OverBar[OverBar[a_]] -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CircleTimes] 
            ((b \[CircleTimes] a) \[CircleTimes] c) == a \[CircleTimes] 
            (b \[CircleTimes] c)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 22} -> 
     <|"Statement" -> HoldForm[(a \[CircleTimes] b) \[CircleTimes] c == 
         b \[CircleTimes] (a \[CircleTimes] c)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 18}, 
        "Construct" -> {"SubstitutionLemma", 21}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (((b_) \[CircleTimes] 
             (a_)) \[CircleTimes] (c_)) -> a \[CircleTimes] 
           (b \[CircleTimes] c), "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[(a \[CircleTimes] b) \[CircleTimes] c == 
           b \[CircleTimes] (a \[CircleTimes] c)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 24} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] b == 
         OverBar[a] \[CirclePlus] b \[CircleTimes] a], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 21}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CirclePlus] (b_) \[CircleTimes] 
            OverBar[a_] -> a \[CirclePlus] b, "Side" -> 1, 
        "Subpattern" -> OverBar[a_], "MatchingConstruct" -> 
         {"SubstitutionLemma", 8}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> OverBar[OverBar[a_]] -> a, "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 25} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CircleTimes] a) == 
         a \[CircleTimes] (OverBar[a] \[CirclePlus] b)], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 2}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CircleTimes] 
           (OverBar[a_] \[CirclePlus] (b_)) -> a \[CircleTimes] b, 
        "Side" -> 1, "Subpattern" -> OverBar[a_] \[CirclePlus] (b_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 24}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         OverBar[a_] \[CirclePlus] (b_) \[CircleTimes] (a_) -> 
          OverBar[a] \[CirclePlus] b, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 38} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CircleTimes] a) == 
         a \[CircleTimes] b], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 25}, "Construct" -> 
         {"CriticalPairLemma", 2}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (OverBar[a_] \[CirclePlus] (b_)) -> 
          a \[CircleTimes] b, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[a \[CircleTimes] (b \[CircleTimes] a) == 
           a \[CircleTimes] b], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 26} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (c \[CircleTimes] b) == 
         a \[CircleTimes] (b \[CircleTimes] (c \[CircleTimes] a))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 38}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CircleTimes] 
           ((b_) \[CircleTimes] (a_)) -> a \[CircleTimes] b, "Side" -> 1, 
        "Subpattern" -> (b_) \[CircleTimes] (a_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 22}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 27} -> 
     <|"Statement" -> HoldForm[a == a \[CirclePlus] b \[CircleTimes] 
           (a \[CircleTimes] c)], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 17}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CirclePlus] ((a_) \[CircleTimes] 
             (b_)) \[CircleTimes] (c_) -> a, "Side" -> 1, 
        "Subpattern" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 28} -> 
     <|"Statement" -> HoldForm[a == a \[CirclePlus] b \[CircleTimes] 
           (c \[CircleTimes] a)], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 27}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CirclePlus] (b_) \[CircleTimes] ((a_) \[CircleTimes] 
             (c_)) -> a, "Side" -> 1, "Subpattern" -> (a_) \[CircleTimes] 
          (c_), "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 
         1, "MatchingRule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"CriticalPairLemma", 29} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CircleTimes] c) == 
         (a \[CircleTimes] (b \[CircleTimes] c)) \[CircleTimes] c], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 13}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] 
            (a_)) -> a, "Side" -> 1, "Subpattern" -> (b_) \[CirclePlus] (a_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 28}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CirclePlus] (b_) \[CircleTimes] ((c_) \[CircleTimes] (a_)) -> 
          a, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 39} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CircleTimes] c) == 
         c \[CircleTimes] (a \[CircleTimes] (b \[CircleTimes] c))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 29}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CircleTimes] (b \[CircleTimes] c) == c \[CircleTimes] 
            (a \[CircleTimes] (b \[CircleTimes] c))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 40} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (c \[CircleTimes] b) == 
         b \[CircleTimes] (c \[CircleTimes] a)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 26}, 
        "Construct" -> {"SubstitutionLemma", 39}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] 
            ((c_) \[CircleTimes] (a_))) -> b \[CircleTimes] 
           (c \[CircleTimes] a), "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[a \[CircleTimes] (c \[CircleTimes] b) == 
           b \[CircleTimes] (c \[CircleTimes] a)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 30} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] 
          (b \[CircleTimes] a \[CirclePlus] c) == 
         a \[CircleTimes] b \[CirclePlus] a \[CircleTimes] c], 
      "Proof" -> <|"Construct" -> {"Axiom", 2}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CircleTimes] (b_) \[CirclePlus] (a_) \[CircleTimes] 
            (c_) -> a \[CircleTimes] (b \[CirclePlus] c), "Side" -> 1, 
        "Subpattern" -> (a_) \[CircleTimes] (b_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 38}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (a_)) -> 
          a \[CircleTimes] b, "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"SubstitutionLemma", 43} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] 
          (b \[CircleTimes] a \[CirclePlus] c) == a \[CircleTimes] 
          (b \[CirclePlus] c)], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 30}, "Construct" -> {"Axiom", 2}, 
        "Position" -> {}, "Rule" -> (a_) \[CircleTimes] (b_) \[CirclePlus] 
           (a_) \[CircleTimes] (c_) -> a \[CircleTimes] (b \[CirclePlus] c), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CircleTimes] (b \[CircleTimes] a \[CirclePlus] c) == 
           a \[CircleTimes] (b \[CirclePlus] c)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 31} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CirclePlus] 
           b \[CircleTimes] c) == a \[CircleTimes] (b \[CircleTimes] 
           (a \[CirclePlus] c))], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 43}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (a_) \[CirclePlus] 
            (c_)) -> a \[CircleTimes] (b \[CirclePlus] c), "Side" -> 1, 
        "Subpattern" -> (b_) \[CircleTimes] (a_) \[CirclePlus] (c_), 
        "MatchingConstruct" -> {"Axiom", 2}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CircleTimes] (b_) \[CirclePlus] 
           (a_) \[CircleTimes] (c_) -> a \[CircleTimes] (b \[CirclePlus] c), 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 44} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] b == a \[CircleTimes] 
          (b \[CircleTimes] (a \[CirclePlus] c))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 31}, 
        "Construct" -> {"SubstitutionLemma", 15}, "Position" -> {2}, 
        "Rule" -> (a_) \[CirclePlus] (a_) \[CircleTimes] (b_) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[a \[CircleTimes] b == 
           a \[CircleTimes] (b \[CircleTimes] (a \[CirclePlus] c))], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 32} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] b == a \[CircleTimes] 
          (b \[CircleTimes] (c \[CirclePlus] a))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 44}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CircleTimes] 
           ((b_) \[CircleTimes] ((a_) \[CirclePlus] (c_))) -> 
          a \[CircleTimes] b, "Side" -> 1, "Subpattern" -> 
         (a_) \[CirclePlus] (c_), "MatchingConstruct" -> {"Axiom", 5}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"SubstitutionLemma", 45} -> 
     <|"Statement" -> HoldForm[a == a \[CirclePlus] b \[CircleTimes] a], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 15}, 
        "Construct" -> {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((a_) \[CirclePlus] (b_)) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a == a \[CirclePlus] b \[CircleTimes] a], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 33} -> 
     <|"Statement" -> HoldForm[(a \[CircleTimes] b) \[CircleTimes] c == 
         (a \[CircleTimes] b) \[CircleTimes] (c \[CircleTimes] b)], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 32}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CircleTimes] 
           ((b_) \[CircleTimes] ((c_) \[CirclePlus] (a_))) -> 
          a \[CircleTimes] b, "Side" -> 1, "Subpattern" -> 
         (c_) \[CirclePlus] (a_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 45}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CirclePlus] (b_) \[CircleTimes] (a_) -> a, 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"SubstitutionLemma", 46} -> 
     <|"Statement" -> HoldForm[(a \[CircleTimes] b) \[CircleTimes] c == 
         b \[CircleTimes] (a \[CircleTimes] (c \[CircleTimes] b))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 33}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[(a \[CircleTimes] b) \[CircleTimes] 
            c == b \[CircleTimes] (a \[CircleTimes] (c \[CircleTimes] b))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 47} -> 
     <|"Statement" -> HoldForm[(a \[CircleTimes] b) \[CircleTimes] c == 
         a \[CircleTimes] (c \[CircleTimes] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 46}, 
        "Construct" -> {"SubstitutionLemma", 39}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] 
            ((c_) \[CircleTimes] (a_))) -> b \[CircleTimes] 
           (c \[CircleTimes] a), "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[(a \[CircleTimes] b) \[CircleTimes] c == 
           a \[CircleTimes] (c \[CircleTimes] b)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 48} -> 
     <|"Statement" -> HoldForm[b \[CircleTimes] (a \[CircleTimes] c) == 
         a \[CircleTimes] (c \[CircleTimes] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 47}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[b \[CircleTimes] (a \[CircleTimes] 
             c) == a \[CircleTimes] (c \[CircleTimes] b)], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 34} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] c == a \[CircleTimes] 
          ((a \[CirclePlus] b) \[CircleTimes] c)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 44}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CircleTimes] 
           ((b_) \[CircleTimes] ((a_) \[CirclePlus] (c_))) -> 
          a \[CircleTimes] b, "Side" -> 1, "Subpattern" -> 
         (b_) \[CircleTimes] ((a_) \[CirclePlus] (c_)), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 35} -> 
     <|"Statement" -> HoldForm[b \[CircleTimes] (a \[CirclePlus] c) == 
         a \[CircleTimes] b \[CirclePlus] b \[CircleTimes] c], 
      "Proof" -> <|"Construct" -> {"Axiom", 2}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CircleTimes] (b_) \[CirclePlus] (a_) \[CircleTimes] 
            (c_) -> a \[CircleTimes] (b \[CirclePlus] c), "Side" -> 1, 
        "Subpattern" -> (a_) \[CircleTimes] (b_), "MatchingConstruct" -> 
         {"Axiom", 1}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 36} -> 
     <|"Statement" -> HoldForm[(a \[CircleTimes] b) \[CircleTimes] x3 == 
         (a \[CircleTimes] b) \[CircleTimes] ((b \[CircleTimes] 
            (a \[CirclePlus] c)) \[CircleTimes] x3)], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 34}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CircleTimes] 
           (((a_) \[CirclePlus] (b_)) \[CircleTimes] (c_)) -> 
          a \[CircleTimes] c, "Side" -> 1, "Subpattern" -> 
         (a_) \[CirclePlus] (b_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 35}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CircleTimes] (b_) \[CirclePlus] 
           (b_) \[CircleTimes] (c_) -> b \[CircleTimes] (a \[CirclePlus] c), 
        "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"SubstitutionLemma", 68} -> 
     <|"Statement" -> HoldForm[(a \[CircleTimes] b) \[CircleTimes] x3 == 
         b \[CircleTimes] (a \[CircleTimes] ((b \[CircleTimes] 
             (a \[CirclePlus] c)) \[CircleTimes] x3))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 36}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[(a \[CircleTimes] b) \[CircleTimes] 
            x3 == b \[CircleTimes] (a \[CircleTimes] ((b \[CircleTimes] (
                a \[CirclePlus] c)) \[CircleTimes] x3))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 69} -> 
     <|"Statement" -> HoldForm[(a \[CircleTimes] b) \[CircleTimes] x3 == 
         b \[CircleTimes] (a \[CircleTimes] ((a \[CirclePlus] 
             c) \[CircleTimes] (b \[CircleTimes] x3)))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 68}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {2, 2}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[(a \[CircleTimes] b) \[CircleTimes] 
            x3 == b \[CircleTimes] (a \[CircleTimes] 
             ((a \[CirclePlus] c) \[CircleTimes] (b \[CircleTimes] x3)))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 70} -> 
     <|"Statement" -> HoldForm[(a \[CircleTimes] b) \[CircleTimes] x3 == 
         b \[CircleTimes] (a \[CircleTimes] (b \[CircleTimes] x3))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 69}, 
        "Construct" -> {"CriticalPairLemma", 34}, "Position" -> {2}, 
        "Rule" -> (a_) \[CircleTimes] (((a_) \[CirclePlus] 
             (b_)) \[CircleTimes] (c_)) -> a \[CircleTimes] c, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (a \[CircleTimes] b) \[CircleTimes] x3 == b \[CircleTimes] 
            (a \[CircleTimes] (b \[CircleTimes] x3))], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 37} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CircleTimes] c) == 
         (a \[CircleTimes] (b \[CircleTimes] c)) \[CircleTimes] b], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 13}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] 
            (a_)) -> a, "Side" -> 1, "Subpattern" -> (b_) \[CirclePlus] (a_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 27}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CirclePlus] (b_) \[CircleTimes] ((a_) \[CircleTimes] (c_)) -> 
          a, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 71} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CircleTimes] c) == 
         b \[CircleTimes] (a \[CircleTimes] (b \[CircleTimes] c))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 37}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CircleTimes] (b \[CircleTimes] c) == b \[CircleTimes] 
            (a \[CircleTimes] (b \[CircleTimes] c))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 72} -> 
     <|"Statement" -> HoldForm[(a \[CircleTimes] b) \[CircleTimes] x3 == 
         a \[CircleTimes] (b \[CircleTimes] x3)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 70}, 
        "Construct" -> {"SubstitutionLemma", 71}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] 
            ((a_) \[CircleTimes] (c_))) -> b \[CircleTimes] 
           (a \[CircleTimes] c), "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[(a \[CircleTimes] b) \[CircleTimes] x3 == 
           a \[CircleTimes] (b \[CircleTimes] x3)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 73} -> 
     <|"Statement" -> HoldForm[b \[CircleTimes] (a \[CircleTimes] x3) == 
         a \[CircleTimes] (b \[CircleTimes] x3)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 72}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[b \[CircleTimes] (a \[CircleTimes] 
             x3) == a \[CircleTimes] (b \[CircleTimes] x3)], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 38} -> 
     <|"Statement" -> HoldForm[b \[CircleTimes] OverBar[b] == 
         a \[CircleTimes] OverBar[a]], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 1}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CircleTimes] OverBar[a_] \[CirclePlus] (b_) -> b, 
        "Side" -> 1, "Subpattern" -> {}, "MatchingConstruct" -> {"Axiom", 3}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CirclePlus] (b_) \[CircleTimes] OverBar[b_] -> a, 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"CriticalPairLemma", 39} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] b == a \[CircleTimes] 
          (a \[CircleTimes] b)], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 38}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (a_)) -> 
          a \[CircleTimes] b, "Side" -> 1, "Subpattern" -> 
         (b_) \[CircleTimes] (a_), "MatchingConstruct" -> {"Axiom", 1}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 40} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] OverBar[a] == 
         (a \[CircleTimes] OverBar[a]) \[CircleTimes] b], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 12}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CircleTimes] 
           ((a_) \[CirclePlus] (b_)) -> a, "Side" -> 1, 
        "Subpattern" -> (a_) \[CirclePlus] (b_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 1}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CircleTimes] OverBar[a_] \[CirclePlus] 
           (b_) -> b, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 41} -> 
     <|"Statement" -> HoldForm[b \[CircleTimes] OverBar[b] == 
         a \[CircleTimes] (b \[CircleTimes] OverBar[b])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 40}, 
        "Orientation" -> -1, "Rule" -> 
         ((a_) \[CircleTimes] OverBar[a_]) \[CircleTimes] (b_) -> 
          a \[CircleTimes] OverBar[a], "Side" -> 1, "Subpattern" -> {}, 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"Hypothesis", 1}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {}, "Rule" -> (a_) \[CircleTimes] (b_) -> 
          b \[CircleTimes] a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[OverBar[a] \[CircleTimes] a == b \[CircleTimes] 
            OverBar[b]], "Source" -> "cpl"|>|>, {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         (b \[CircleTimes] OverBar[OverBar[b]]) \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 1}, 
        "Construct" -> {"CriticalPairLemma", 3}, "Position" -> {1}, 
        "Rule" -> a_ -> a \[CircleTimes] OverBar[OverBar[a]], 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a] \[CircleTimes] a == (b \[CircleTimes] 
             OverBar[OverBar[b]]) \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 23} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] OverBar[b])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 2}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] OverBar[b])], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 24} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         (OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] 
            OverBar[b])) \[CircleTimes] (OverBar[OverBar[b]] \[CircleTimes] 
           (b \[CircleTimes] OverBar[b]))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 23}, "Construct" -> 
         {"CriticalPairLemma", 5}, "Position" -> {}, 
        "Rule" -> a_ -> a \[CircleTimes] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           (OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] 
              OverBar[b])) \[CircleTimes] (OverBar[OverBar[b]] \[CircleTimes] 
             (b \[CircleTimes] OverBar[b]))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 25} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         (b \[CircleTimes] OverBar[b]) \[CircleTimes] 
          (OverBar[OverBar[b]] \[CircleTimes] 
           (OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] 
             OverBar[b])))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 24}, "Construct" -> 
         {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           (b \[CircleTimes] OverBar[b]) \[CircleTimes] 
            (OverBar[OverBar[b]] \[CircleTimes] (OverBar[OverBar[
                b]] \[CircleTimes] (b \[CircleTimes] OverBar[b])))], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 26} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         OverBar[b] \[CircleTimes] (b \[CircleTimes] 
           (OverBar[OverBar[b]] \[CircleTimes] 
            (OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] 
              OverBar[b]))))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 25}, "Construct" -> 
         {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           OverBar[b] \[CircleTimes] (b \[CircleTimes] 
             (OverBar[OverBar[b]] \[CircleTimes] (OverBar[OverBar[
                 b]] \[CircleTimes] (b \[CircleTimes] OverBar[b]))))], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 27} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         (b \[CircleTimes] (OverBar[OverBar[b]] \[CircleTimes] 
            (OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] 
              OverBar[b])))) \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 26}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a] \[CircleTimes] a == (b \[CircleTimes] 
             (OverBar[OverBar[b]] \[CircleTimes] (OverBar[OverBar[
                 b]] \[CircleTimes] (b \[CircleTimes] OverBar[
                 b])))) \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 28} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         ((OverBar[OverBar[b]] \[CircleTimes] 
            (OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] 
              OverBar[b]))) \[CircleTimes] b) \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 27}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {1}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a] \[CircleTimes] a == ((OverBar[OverBar[b]] \[CircleTimes] 
              (OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] 
                OverBar[b]))) \[CircleTimes] b) \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 29} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         b \[CircleTimes] ((OverBar[OverBar[b]] \[CircleTimes] 
            (OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] 
              OverBar[b]))) \[CircleTimes] OverBar[b])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 28}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           b \[CircleTimes] ((OverBar[OverBar[b]] \[CircleTimes] 
              (OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] 
                OverBar[b]))) \[CircleTimes] OverBar[b])], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 30} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         ((OverBar[OverBar[b]] \[CircleTimes] 
            (OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] 
              OverBar[b]))) \[CircleTimes] OverBar[b]) \[CircleTimes] b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 29}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a] \[CircleTimes] a == ((OverBar[OverBar[b]] \[CircleTimes] 
              (OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] 
                OverBar[b]))) \[CircleTimes] OverBar[b]) \[CircleTimes] b], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 31} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         ((OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] 
             OverBar[b])) \[CircleTimes] (OverBar[OverBar[b]] \[CircleTimes] 
            OverBar[b])) \[CircleTimes] b], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 30}, "Construct" -> 
         {"SubstitutionLemma", 22}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           ((OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] OverBar[
                b])) \[CircleTimes] (OverBar[OverBar[b]] \[CircleTimes] 
              OverBar[b])) \[CircleTimes] b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 32} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         (OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
          ((OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] 
             OverBar[b])) \[CircleTimes] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 31}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           (OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
            ((OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] OverBar[
                b])) \[CircleTimes] b)], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 33} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         OverBar[b] \[CircleTimes] (OverBar[OverBar[b]] \[CircleTimes] 
           ((OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] 
              OverBar[b])) \[CircleTimes] b))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 32}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           OverBar[b] \[CircleTimes] (OverBar[OverBar[b]] \[CircleTimes] 
             ((OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] 
                OverBar[b])) \[CircleTimes] b))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 34} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         (OverBar[OverBar[b]] \[CircleTimes] 
           ((OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] 
              OverBar[b])) \[CircleTimes] b)) \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 33}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a] \[CircleTimes] a == (OverBar[OverBar[b]] \[CircleTimes] 
             ((OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] 
                OverBar[b])) \[CircleTimes] b)) \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 35} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         (((OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] 
              OverBar[b])) \[CircleTimes] b) \[CircleTimes] 
           OverBar[OverBar[b]]) \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 34}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {1}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a] \[CircleTimes] a == 
           (((OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] 
                OverBar[b])) \[CircleTimes] b) \[CircleTimes] 
             OverBar[OverBar[b]]) \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 36} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         OverBar[OverBar[b]] \[CircleTimes] 
          (((OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] 
              OverBar[b])) \[CircleTimes] b) \[CircleTimes] OverBar[b])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 35}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           OverBar[OverBar[b]] \[CircleTimes] 
            (((OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] 
                OverBar[b])) \[CircleTimes] b) \[CircleTimes] OverBar[b])], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 37} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         (((OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] 
              OverBar[b])) \[CircleTimes] b) \[CircleTimes] 
           OverBar[b]) \[CircleTimes] OverBar[OverBar[b]]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 36}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a] \[CircleTimes] a == 
           (((OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] 
                OverBar[b])) \[CircleTimes] b) \[CircleTimes] 
             OverBar[b]) \[CircleTimes] OverBar[OverBar[b]]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 41} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         (((OverBar[b] \[CircleTimes] (b \[CircleTimes] OverBar[OverBar[
                b]])) \[CircleTimes] b) \[CircleTimes] 
           OverBar[b]) \[CircleTimes] OverBar[OverBar[b]]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 37}, 
        "Construct" -> {"SubstitutionLemma", 40}, "Position" -> {1, 1, 1}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (b \[CircleTimes] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           (((OverBar[b] \[CircleTimes] (b \[CircleTimes] OverBar[
                 OverBar[b]])) \[CircleTimes] b) \[CircleTimes] 
             OverBar[b]) \[CircleTimes] OverBar[OverBar[b]]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 42} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         OverBar[b] \[CircleTimes] (((OverBar[b] \[CircleTimes] 
             (b \[CircleTimes] OverBar[OverBar[b]])) \[CircleTimes] 
            b) \[CircleTimes] OverBar[OverBar[b]])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 41}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           OverBar[b] \[CircleTimes] (((OverBar[b] \[CircleTimes] (
                b \[CircleTimes] OverBar[OverBar[b]])) \[CircleTimes] 
              b) \[CircleTimes] OverBar[OverBar[b]])], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 49} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         OverBar[OverBar[b]] \[CircleTimes] (OverBar[b] \[CircleTimes] 
           ((OverBar[b] \[CircleTimes] (b \[CircleTimes] OverBar[OverBar[
                b]])) \[CircleTimes] b))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 42}, "Construct" -> 
         {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           OverBar[OverBar[b]] \[CircleTimes] (OverBar[b] \[CircleTimes] 
             ((OverBar[b] \[CircleTimes] (b \[CircleTimes] OverBar[
                 OverBar[b]])) \[CircleTimes] b))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 50} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         ((OverBar[b] \[CircleTimes] (b \[CircleTimes] 
             OverBar[OverBar[b]])) \[CircleTimes] b) \[CircleTimes] 
          (OverBar[OverBar[b]] \[CircleTimes] OverBar[b])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 49}, 
        "Construct" -> {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           ((OverBar[b] \[CircleTimes] (b \[CircleTimes] OverBar[
                OverBar[b]])) \[CircleTimes] b) \[CircleTimes] 
            (OverBar[OverBar[b]] \[CircleTimes] OverBar[b])], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 51} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         b \[CircleTimes] ((OverBar[b] \[CircleTimes] (b \[CircleTimes] 
             OverBar[OverBar[b]])) \[CircleTimes] 
           (OverBar[OverBar[b]] \[CircleTimes] OverBar[b]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 50}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           b \[CircleTimes] ((OverBar[b] \[CircleTimes] (b \[CircleTimes] 
               OverBar[OverBar[b]])) \[CircleTimes] (OverBar[OverBar[
                b]] \[CircleTimes] OverBar[b]))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 52} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         (OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
          (b \[CircleTimes] (OverBar[b] \[CircleTimes] (b \[CircleTimes] 
             OverBar[OverBar[b]])))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 51}, "Construct" -> 
         {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           (OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
            (b \[CircleTimes] (OverBar[b] \[CircleTimes] (b \[CircleTimes] 
               OverBar[OverBar[b]])))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 53} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         (OverBar[b] \[CircleTimes] (b \[CircleTimes] 
            OverBar[OverBar[b]])) \[CircleTimes] 
          ((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
           b)], "Proof" -> <|"Input" -> {"SubstitutionLemma", 52}, 
        "Construct" -> {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           (OverBar[b] \[CircleTimes] (b \[CircleTimes] OverBar[OverBar[
                b]])) \[CircleTimes] ((OverBar[OverBar[b]] \[CircleTimes] 
              OverBar[b]) \[CircleTimes] b)], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 54} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         (b \[CircleTimes] OverBar[OverBar[b]]) \[CircleTimes] 
          (OverBar[b] \[CircleTimes] ((OverBar[OverBar[b]] \[CircleTimes] 
             OverBar[b]) \[CircleTimes] b))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 53}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           (b \[CircleTimes] OverBar[OverBar[b]]) \[CircleTimes] 
            (OverBar[b] \[CircleTimes] ((OverBar[OverBar[b]] \[CircleTimes] 
               OverBar[b]) \[CircleTimes] b))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 55} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         ((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
           b) \[CircleTimes] ((b \[CircleTimes] OverBar[
             OverBar[b]]) \[CircleTimes] OverBar[b])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 54}, 
        "Construct" -> {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           ((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
             b) \[CircleTimes] ((b \[CircleTimes] OverBar[OverBar[
                b]]) \[CircleTimes] OverBar[b])], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 56} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         OverBar[b] \[CircleTimes] (((OverBar[OverBar[b]] \[CircleTimes] 
             OverBar[b]) \[CircleTimes] b) \[CircleTimes] (b \[CircleTimes] 
            OverBar[OverBar[b]]))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 55}, "Construct" -> 
         {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           OverBar[b] \[CircleTimes] (((OverBar[OverBar[b]] \[CircleTimes] 
               OverBar[b]) \[CircleTimes] b) \[CircleTimes] (b \[CircleTimes] 
              OverBar[OverBar[b]]))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 57} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         (((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
            b) \[CircleTimes] (b \[CircleTimes] OverBar[
             OverBar[b]])) \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 56}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a] \[CircleTimes] a == 
           (((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
              b) \[CircleTimes] (b \[CircleTimes] OverBar[OverBar[
                b]])) \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 58} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         (OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] 
            ((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
             b))) \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 57}, 
        "Construct" -> {"SubstitutionLemma", 40}, "Position" -> {1}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (b \[CircleTimes] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           (OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] 
              ((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
               b))) \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 59} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         (b \[CircleTimes] ((OverBar[OverBar[b]] \[CircleTimes] 
             OverBar[b]) \[CircleTimes] b)) \[CircleTimes] 
          (OverBar[OverBar[b]] \[CircleTimes] OverBar[b])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 58}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           (b \[CircleTimes] ((OverBar[OverBar[b]] \[CircleTimes] OverBar[
                b]) \[CircleTimes] b)) \[CircleTimes] 
            (OverBar[OverBar[b]] \[CircleTimes] OverBar[b])], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 60} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         OverBar[b] \[CircleTimes] ((b \[CircleTimes] 
            ((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
             b)) \[CircleTimes] OverBar[OverBar[b]])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 59}, 
        "Construct" -> {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           OverBar[b] \[CircleTimes] ((b \[CircleTimes] 
              ((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
               b)) \[CircleTimes] OverBar[OverBar[b]])], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 61} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         OverBar[OverBar[b]] \[CircleTimes] (OverBar[b] \[CircleTimes] 
           (b \[CircleTimes] ((OverBar[OverBar[b]] \[CircleTimes] 
              OverBar[b]) \[CircleTimes] b)))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 60}, 
        "Construct" -> {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           OverBar[OverBar[b]] \[CircleTimes] (OverBar[b] \[CircleTimes] 
             (b \[CircleTimes] ((OverBar[OverBar[b]] \[CircleTimes] 
                OverBar[b]) \[CircleTimes] b)))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 62} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         (OverBar[b] \[CircleTimes] (b \[CircleTimes] 
            ((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
             b))) \[CircleTimes] OverBar[OverBar[b]]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 61}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a] \[CircleTimes] a == (OverBar[b] \[CircleTimes] 
             (b \[CircleTimes] ((OverBar[OverBar[b]] \[CircleTimes] 
                OverBar[b]) \[CircleTimes] b))) \[CircleTimes] 
            OverBar[OverBar[b]]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 63} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         (((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
            b) \[CircleTimes] (b \[CircleTimes] OverBar[b])) \[CircleTimes] 
          OverBar[OverBar[b]]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 62}, "Construct" -> 
         {"SubstitutionLemma", 40}, "Position" -> {1}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (b \[CircleTimes] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           (((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
              b) \[CircleTimes] (b \[CircleTimes] OverBar[b])) \[CircleTimes] 
            OverBar[OverBar[b]]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 64} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         (b \[CircleTimes] OverBar[b]) \[CircleTimes] 
          (((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
            b) \[CircleTimes] OverBar[OverBar[b]])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 63}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           (b \[CircleTimes] OverBar[b]) \[CircleTimes] 
            (((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
              b) \[CircleTimes] OverBar[OverBar[b]])], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 65} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         OverBar[OverBar[b]] \[CircleTimes] ((b \[CircleTimes] 
            OverBar[b]) \[CircleTimes] ((OverBar[OverBar[b]] \[CircleTimes] 
             OverBar[b]) \[CircleTimes] b))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 64}, 
        "Construct" -> {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           OverBar[OverBar[b]] \[CircleTimes] ((b \[CircleTimes] 
              OverBar[b]) \[CircleTimes] ((OverBar[OverBar[b]] \[CircleTimes] 
               OverBar[b]) \[CircleTimes] b))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 66} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] 
           ((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
            (b \[CircleTimes] OverBar[b])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 65}, 
        "Construct" -> {"SubstitutionLemma", 40}, "Position" -> {2}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (b \[CircleTimes] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] 
             ((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
              (b \[CircleTimes] OverBar[b])))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 67} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         ((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
           (b \[CircleTimes] OverBar[b])) \[CircleTimes] (b \[CircleTimes] 
           OverBar[OverBar[b]])], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 66}, "Construct" -> 
         {"SubstitutionLemma", 40}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (b \[CircleTimes] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           ((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
             (b \[CircleTimes] OverBar[b])) \[CircleTimes] (b \[CircleTimes] 
             OverBar[OverBar[b]])], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 74} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         b \[CircleTimes] (((OverBar[OverBar[b]] \[CircleTimes] 
             OverBar[b]) \[CircleTimes] (b \[CircleTimes] 
             OverBar[b])) \[CircleTimes] OverBar[OverBar[b]])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 67}, 
        "Construct" -> {"SubstitutionLemma", 73}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           b \[CircleTimes] (((OverBar[OverBar[b]] \[CircleTimes] OverBar[
                b]) \[CircleTimes] (b \[CircleTimes] OverBar[
                b])) \[CircleTimes] OverBar[OverBar[b]])], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 75} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         (((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
            (b \[CircleTimes] OverBar[b])) \[CircleTimes] 
           OverBar[OverBar[b]]) \[CircleTimes] b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 74}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a] \[CircleTimes] a == 
           (((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
              (b \[CircleTimes] OverBar[b])) \[CircleTimes] 
             OverBar[OverBar[b]]) \[CircleTimes] b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 76} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         ((b \[CircleTimes] OverBar[b]) \[CircleTimes] 
           ((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
            OverBar[OverBar[b]])) \[CircleTimes] b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 75}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           ((b \[CircleTimes] OverBar[b]) \[CircleTimes] 
             ((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
              OverBar[OverBar[b]])) \[CircleTimes] b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 77} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         ((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
           OverBar[OverBar[b]]) \[CircleTimes] ((b \[CircleTimes] 
            OverBar[b]) \[CircleTimes] b)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 76}, "Construct" -> 
         {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           ((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
             OverBar[OverBar[b]]) \[CircleTimes] ((b \[CircleTimes] 
              OverBar[b]) \[CircleTimes] b)], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 78} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         b \[CircleTimes] (((OverBar[OverBar[b]] \[CircleTimes] 
             OverBar[b]) \[CircleTimes] OverBar[OverBar[b]]) \[CircleTimes] 
           (b \[CircleTimes] OverBar[b]))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 77}, "Construct" -> 
         {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           b \[CircleTimes] (((OverBar[OverBar[b]] \[CircleTimes] OverBar[
                b]) \[CircleTimes] OverBar[OverBar[b]]) \[CircleTimes] 
             (b \[CircleTimes] OverBar[b]))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 79} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         b \[CircleTimes] (OverBar[b] \[CircleTimes] (b \[CircleTimes] 
            ((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
             OverBar[OverBar[b]])))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 78}, "Construct" -> 
         {"SubstitutionLemma", 40}, "Position" -> {2}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (b \[CircleTimes] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           b \[CircleTimes] (OverBar[b] \[CircleTimes] (b \[CircleTimes] 
              ((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
               OverBar[OverBar[b]])))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 80} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         (b \[CircleTimes] ((OverBar[OverBar[b]] \[CircleTimes] 
             OverBar[b]) \[CircleTimes] OverBar[OverBar[b]])) \[CircleTimes] 
          (OverBar[b] \[CircleTimes] b)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 79}, "Construct" -> 
         {"SubstitutionLemma", 40}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (b \[CircleTimes] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           (b \[CircleTimes] ((OverBar[OverBar[b]] \[CircleTimes] OverBar[
                b]) \[CircleTimes] OverBar[OverBar[b]])) \[CircleTimes] 
            (OverBar[b] \[CircleTimes] b)], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 81} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         OverBar[b] \[CircleTimes] ((b \[CircleTimes] 
            ((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
             OverBar[OverBar[b]])) \[CircleTimes] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 80}, 
        "Construct" -> {"SubstitutionLemma", 73}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           OverBar[b] \[CircleTimes] ((b \[CircleTimes] 
              ((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
               OverBar[OverBar[b]])) \[CircleTimes] b)], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 82} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         ((b \[CircleTimes] ((OverBar[OverBar[b]] \[CircleTimes] 
              OverBar[b]) \[CircleTimes] OverBar[OverBar[b]])) \[CircleTimes] 
           b) \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 81}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {}, "Rule" -> (a_) \[CircleTimes] (b_) -> 
          b \[CircleTimes] a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[OverBar[a] \[CircleTimes] a == 
           ((b \[CircleTimes] ((OverBar[OverBar[b]] \[CircleTimes] 
                OverBar[b]) \[CircleTimes] OverBar[OverBar[
                 b]])) \[CircleTimes] b) \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 83} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         (((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
            OverBar[OverBar[b]]) \[CircleTimes] (b \[CircleTimes] 
            b)) \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 82}, "Construct" -> 
         {"SubstitutionLemma", 22}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           (((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
              OverBar[OverBar[b]]) \[CircleTimes] (b \[CircleTimes] 
              b)) \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 84} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         (b \[CircleTimes] b) \[CircleTimes] 
          (((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
            OverBar[OverBar[b]]) \[CircleTimes] OverBar[b])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 83}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           (b \[CircleTimes] b) \[CircleTimes] 
            (((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
              OverBar[OverBar[b]]) \[CircleTimes] OverBar[b])], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 85} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         OverBar[b] \[CircleTimes] ((b \[CircleTimes] b) \[CircleTimes] 
           ((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
            OverBar[OverBar[b]]))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 84}, "Construct" -> 
         {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           OverBar[b] \[CircleTimes] ((b \[CircleTimes] b) \[CircleTimes] 
             ((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
              OverBar[OverBar[b]]))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 86} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         OverBar[b] \[CircleTimes] (OverBar[OverBar[b]] \[CircleTimes] 
           ((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
            (b \[CircleTimes] b)))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 85}, "Construct" -> 
         {"SubstitutionLemma", 40}, "Position" -> {2}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (b \[CircleTimes] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           OverBar[b] \[CircleTimes] (OverBar[OverBar[b]] \[CircleTimes] 
             ((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
              (b \[CircleTimes] b)))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 87} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         ((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
           (b \[CircleTimes] b)) \[CircleTimes] (OverBar[b] \[CircleTimes] 
           OverBar[OverBar[b]])], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 86}, "Construct" -> 
         {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           ((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
             (b \[CircleTimes] b)) \[CircleTimes] (OverBar[b] \[CircleTimes] 
             OverBar[OverBar[b]])], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 88} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         OverBar[OverBar[b]] \[CircleTimes] 
          (((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
            (b \[CircleTimes] b)) \[CircleTimes] OverBar[b])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 87}, 
        "Construct" -> {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           OverBar[OverBar[b]] \[CircleTimes] 
            (((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
              (b \[CircleTimes] b)) \[CircleTimes] OverBar[b])], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 89} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         (((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
            (b \[CircleTimes] b)) \[CircleTimes] OverBar[b]) \[CircleTimes] 
          OverBar[OverBar[b]]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 88}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {}, "Rule" -> (a_) \[CircleTimes] (b_) -> 
          b \[CircleTimes] a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[OverBar[a] \[CircleTimes] a == 
           (((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
              (b \[CircleTimes] b)) \[CircleTimes] OverBar[b]) \[CircleTimes] 
            OverBar[OverBar[b]]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 90} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         ((b \[CircleTimes] b) \[CircleTimes] 
           ((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
            OverBar[b])) \[CircleTimes] OverBar[OverBar[b]]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 89}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           ((b \[CircleTimes] b) \[CircleTimes] 
             ((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
              OverBar[b])) \[CircleTimes] OverBar[OverBar[b]]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 91} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         ((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
           OverBar[b]) \[CircleTimes] ((b \[CircleTimes] b) \[CircleTimes] 
           OverBar[OverBar[b]])], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 90}, "Construct" -> 
         {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           ((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
             OverBar[b]) \[CircleTimes] ((b \[CircleTimes] b) \[CircleTimes] 
             OverBar[OverBar[b]])], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 92} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         OverBar[OverBar[b]] \[CircleTimes] 
          (((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
            OverBar[b]) \[CircleTimes] (b \[CircleTimes] b))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 91}, 
        "Construct" -> {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           OverBar[OverBar[b]] \[CircleTimes] 
            (((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
              OverBar[b]) \[CircleTimes] (b \[CircleTimes] b))], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 93} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] 
           (((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
             OverBar[b]) \[CircleTimes] b))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 92}, 
        "Construct" -> {"SubstitutionLemma", 73}, "Position" -> {2}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] 
             (((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
               OverBar[b]) \[CircleTimes] b))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 94} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         (((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
            OverBar[b]) \[CircleTimes] b) \[CircleTimes] (b \[CircleTimes] 
           OverBar[OverBar[b]])], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 93}, "Construct" -> 
         {"SubstitutionLemma", 40}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (b \[CircleTimes] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           (((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
              OverBar[b]) \[CircleTimes] b) \[CircleTimes] (b \[CircleTimes] 
             OverBar[OverBar[b]])], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 95} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         b \[CircleTimes] ((((OverBar[OverBar[b]] \[CircleTimes] 
              OverBar[b]) \[CircleTimes] OverBar[b]) \[CircleTimes] 
            b) \[CircleTimes] OverBar[OverBar[b]])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 94}, 
        "Construct" -> {"SubstitutionLemma", 73}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           b \[CircleTimes] ((((OverBar[OverBar[b]] \[CircleTimes] 
                OverBar[b]) \[CircleTimes] OverBar[b]) \[CircleTimes] 
              b) \[CircleTimes] OverBar[OverBar[b]])], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 96} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         ((((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
             OverBar[b]) \[CircleTimes] b) \[CircleTimes] 
           OverBar[OverBar[b]]) \[CircleTimes] b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 95}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a] \[CircleTimes] a == 
           ((((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
               OverBar[b]) \[CircleTimes] b) \[CircleTimes] 
             OverBar[OverBar[b]]) \[CircleTimes] b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 97} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         (b \[CircleTimes] (((OverBar[OverBar[b]] \[CircleTimes] 
              OverBar[b]) \[CircleTimes] OverBar[b]) \[CircleTimes] 
            OverBar[OverBar[b]])) \[CircleTimes] b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 96}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           (b \[CircleTimes] (((OverBar[OverBar[b]] \[CircleTimes] 
                OverBar[b]) \[CircleTimes] OverBar[b]) \[CircleTimes] 
              OverBar[OverBar[b]])) \[CircleTimes] b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 98} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         (((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
            OverBar[b]) \[CircleTimes] OverBar[OverBar[b]]) \[CircleTimes] 
          (b \[CircleTimes] b)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 97}, "Construct" -> 
         {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           (((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
              OverBar[b]) \[CircleTimes] OverBar[OverBar[b]]) \[CircleTimes] 
            (b \[CircleTimes] b)], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 99} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         b \[CircleTimes] ((((OverBar[OverBar[b]] \[CircleTimes] 
              OverBar[b]) \[CircleTimes] OverBar[b]) \[CircleTimes] 
            OverBar[OverBar[b]]) \[CircleTimes] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 98}, 
        "Construct" -> {"SubstitutionLemma", 73}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           b \[CircleTimes] ((((OverBar[OverBar[b]] \[CircleTimes] 
                OverBar[b]) \[CircleTimes] OverBar[b]) \[CircleTimes] 
              OverBar[OverBar[b]]) \[CircleTimes] b)], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 100} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         b \[CircleTimes] ((OverBar[b] \[CircleTimes] 
            ((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
             OverBar[OverBar[b]])) \[CircleTimes] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 99}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {2, 1}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           b \[CircleTimes] ((OverBar[b] \[CircleTimes] 
              ((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
               OverBar[OverBar[b]])) \[CircleTimes] b)], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 101} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         ((OverBar[b] \[CircleTimes] ((OverBar[OverBar[b]] \[CircleTimes] 
              OverBar[b]) \[CircleTimes] OverBar[OverBar[b]])) \[CircleTimes] 
           b) \[CircleTimes] b], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 100}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {}, "Rule" -> (a_) \[CircleTimes] (b_) -> 
          b \[CircleTimes] a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[OverBar[a] \[CircleTimes] a == 
           ((OverBar[b] \[CircleTimes] ((OverBar[OverBar[b]] \[CircleTimes] 
                OverBar[b]) \[CircleTimes] OverBar[OverBar[
                 b]])) \[CircleTimes] b) \[CircleTimes] b], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 102} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         (((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
            OverBar[OverBar[b]]) \[CircleTimes] (OverBar[b] \[CircleTimes] 
            b)) \[CircleTimes] b], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 101}, "Construct" -> 
         {"SubstitutionLemma", 22}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           (((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
              OverBar[OverBar[b]]) \[CircleTimes] (OverBar[b] \[CircleTimes] 
              b)) \[CircleTimes] b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 103} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         (OverBar[b] \[CircleTimes] b) \[CircleTimes] 
          (((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
            OverBar[OverBar[b]]) \[CircleTimes] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 102}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           (OverBar[b] \[CircleTimes] b) \[CircleTimes] 
            (((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
              OverBar[OverBar[b]]) \[CircleTimes] b)], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 104} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         b \[CircleTimes] ((OverBar[b] \[CircleTimes] b) \[CircleTimes] 
           ((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
            OverBar[OverBar[b]]))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 103}, "Construct" -> 
         {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           b \[CircleTimes] ((OverBar[b] \[CircleTimes] b) \[CircleTimes] 
             ((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
              OverBar[OverBar[b]]))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 105} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         b \[CircleTimes] (OverBar[OverBar[b]] \[CircleTimes] 
           ((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
            (OverBar[b] \[CircleTimes] b)))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 104}, 
        "Construct" -> {"SubstitutionLemma", 40}, "Position" -> {2}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (b \[CircleTimes] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           b \[CircleTimes] (OverBar[OverBar[b]] \[CircleTimes] 
             ((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
              (OverBar[b] \[CircleTimes] b)))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 106} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         ((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
           (OverBar[b] \[CircleTimes] b)) \[CircleTimes] 
          (OverBar[OverBar[b]] \[CircleTimes] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 105}, 
        "Construct" -> {"SubstitutionLemma", 40}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (b \[CircleTimes] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           ((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
             (OverBar[b] \[CircleTimes] b)) \[CircleTimes] 
            (OverBar[OverBar[b]] \[CircleTimes] b)], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 107} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         OverBar[OverBar[b]] \[CircleTimes] 
          (((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
            (OverBar[b] \[CircleTimes] b)) \[CircleTimes] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 106}, 
        "Construct" -> {"SubstitutionLemma", 73}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           OverBar[OverBar[b]] \[CircleTimes] 
            (((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
              (OverBar[b] \[CircleTimes] b)) \[CircleTimes] b)], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 108} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         (((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
            (OverBar[b] \[CircleTimes] b)) \[CircleTimes] b) \[CircleTimes] 
          OverBar[OverBar[b]]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 107}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {}, "Rule" -> (a_) \[CircleTimes] (b_) -> 
          b \[CircleTimes] a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[OverBar[a] \[CircleTimes] a == 
           (((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
              (OverBar[b] \[CircleTimes] b)) \[CircleTimes] b) \[CircleTimes] 
            OverBar[OverBar[b]]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 109} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         ((OverBar[b] \[CircleTimes] b) \[CircleTimes] 
           ((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
            b)) \[CircleTimes] OverBar[OverBar[b]]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 108}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           ((OverBar[b] \[CircleTimes] b) \[CircleTimes] 
             ((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
              b)) \[CircleTimes] OverBar[OverBar[b]]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 110} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         ((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
           b) \[CircleTimes] ((OverBar[b] \[CircleTimes] b) \[CircleTimes] 
           OverBar[OverBar[b]])], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 109}, "Construct" -> 
         {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           ((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
             b) \[CircleTimes] ((OverBar[b] \[CircleTimes] b) \[CircleTimes] 
             OverBar[OverBar[b]])], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 111} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         ((OverBar[b] \[CircleTimes] b) \[CircleTimes] 
           OverBar[OverBar[b]]) \[CircleTimes] 
          ((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
           b)], "Proof" -> <|"Input" -> {"SubstitutionLemma", 110}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a] \[CircleTimes] a == ((OverBar[b] \[CircleTimes] 
              b) \[CircleTimes] OverBar[OverBar[b]]) \[CircleTimes] 
            ((OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
             b)], "Source" -> "cpl"|>|>, {"SubstitutionLemma", 112} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         b \[CircleTimes] (((OverBar[b] \[CircleTimes] b) \[CircleTimes] 
            OverBar[OverBar[b]]) \[CircleTimes] 
           (OverBar[OverBar[b]] \[CircleTimes] OverBar[b]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 111}, 
        "Construct" -> {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           b \[CircleTimes] (((OverBar[b] \[CircleTimes] b) \[CircleTimes] 
              OverBar[OverBar[b]]) \[CircleTimes] (OverBar[OverBar[
                b]] \[CircleTimes] OverBar[b]))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 113} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         (OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
          (b \[CircleTimes] ((OverBar[b] \[CircleTimes] b) \[CircleTimes] 
            OverBar[OverBar[b]]))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 112}, "Construct" -> 
         {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           (OverBar[OverBar[b]] \[CircleTimes] OverBar[b]) \[CircleTimes] 
            (b \[CircleTimes] ((OverBar[b] \[CircleTimes] b) \[CircleTimes] 
              OverBar[OverBar[b]]))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 114} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         OverBar[b] \[CircleTimes] (OverBar[OverBar[b]] \[CircleTimes] 
           (b \[CircleTimes] ((OverBar[b] \[CircleTimes] b) \[CircleTimes] 
             OverBar[OverBar[b]])))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 113}, "Construct" -> 
         {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           OverBar[b] \[CircleTimes] (OverBar[OverBar[b]] \[CircleTimes] 
             (b \[CircleTimes] ((OverBar[b] \[CircleTimes] b) \[CircleTimes] 
               OverBar[OverBar[b]])))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 115} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         (b \[CircleTimes] ((OverBar[b] \[CircleTimes] b) \[CircleTimes] 
            OverBar[OverBar[b]])) \[CircleTimes] (OverBar[b] \[CircleTimes] 
           OverBar[OverBar[b]])], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 114}, "Construct" -> 
         {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           (b \[CircleTimes] ((OverBar[b] \[CircleTimes] b) \[CircleTimes] 
              OverBar[OverBar[b]])) \[CircleTimes] (OverBar[b] \[CircleTimes] 
             OverBar[OverBar[b]])], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 116} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         OverBar[OverBar[b]] \[CircleTimes] ((b \[CircleTimes] 
            ((OverBar[b] \[CircleTimes] b) \[CircleTimes] 
             OverBar[OverBar[b]])) \[CircleTimes] OverBar[b])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 115}, 
        "Construct" -> {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           OverBar[OverBar[b]] \[CircleTimes] ((b \[CircleTimes] 
              ((OverBar[b] \[CircleTimes] b) \[CircleTimes] OverBar[
                OverBar[b]])) \[CircleTimes] OverBar[b])], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 117} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         ((b \[CircleTimes] ((OverBar[b] \[CircleTimes] b) \[CircleTimes] 
             OverBar[OverBar[b]])) \[CircleTimes] OverBar[b]) \[CircleTimes] 
          OverBar[OverBar[b]]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 116}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {}, "Rule" -> (a_) \[CircleTimes] (b_) -> 
          b \[CircleTimes] a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[OverBar[a] \[CircleTimes] a == 
           ((b \[CircleTimes] ((OverBar[b] \[CircleTimes] b) \[CircleTimes] 
               OverBar[OverBar[b]])) \[CircleTimes] OverBar[
              b]) \[CircleTimes] OverBar[OverBar[b]]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 118} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         (((OverBar[b] \[CircleTimes] b) \[CircleTimes] 
            OverBar[OverBar[b]]) \[CircleTimes] (b \[CircleTimes] 
            OverBar[b])) \[CircleTimes] OverBar[OverBar[b]]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 117}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           (((OverBar[b] \[CircleTimes] b) \[CircleTimes] OverBar[OverBar[
                b]]) \[CircleTimes] (b \[CircleTimes] OverBar[
               b])) \[CircleTimes] OverBar[OverBar[b]]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 119} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         (b \[CircleTimes] OverBar[b]) \[CircleTimes] 
          (((OverBar[b] \[CircleTimes] b) \[CircleTimes] 
            OverBar[OverBar[b]]) \[CircleTimes] OverBar[OverBar[b]])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 118}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           (b \[CircleTimes] OverBar[b]) \[CircleTimes] 
            (((OverBar[b] \[CircleTimes] b) \[CircleTimes] OverBar[OverBar[
                b]]) \[CircleTimes] OverBar[OverBar[b]])], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 120} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         OverBar[OverBar[b]] \[CircleTimes] ((b \[CircleTimes] 
            OverBar[b]) \[CircleTimes] ((OverBar[b] \[CircleTimes] 
             b) \[CircleTimes] OverBar[OverBar[b]]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 119}, 
        "Construct" -> {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           OverBar[OverBar[b]] \[CircleTimes] ((b \[CircleTimes] 
              OverBar[b]) \[CircleTimes] ((OverBar[b] \[CircleTimes] 
               b) \[CircleTimes] OverBar[OverBar[b]]))], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 121} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         ((OverBar[b] \[CircleTimes] b) \[CircleTimes] 
           OverBar[OverBar[b]]) \[CircleTimes] 
          (OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] 
            OverBar[b]))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 
          120}, "Construct" -> {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           ((OverBar[b] \[CircleTimes] b) \[CircleTimes] 
             OverBar[OverBar[b]]) \[CircleTimes] 
            (OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] 
              OverBar[b]))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 122} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         OverBar[OverBar[b]] \[CircleTimes] ((OverBar[b] \[CircleTimes] 
            b) \[CircleTimes] (OverBar[OverBar[b]] \[CircleTimes] 
            (b \[CircleTimes] OverBar[b])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 121}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           OverBar[OverBar[b]] \[CircleTimes] ((OverBar[b] \[CircleTimes] 
              b) \[CircleTimes] (OverBar[OverBar[b]] \[CircleTimes] 
              (b \[CircleTimes] OverBar[b])))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 123} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         ((OverBar[b] \[CircleTimes] b) \[CircleTimes] 
           (OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] 
             OverBar[b]))) \[CircleTimes] OverBar[OverBar[b]]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 122}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a] \[CircleTimes] a == ((OverBar[b] \[CircleTimes] 
              b) \[CircleTimes] (OverBar[OverBar[b]] \[CircleTimes] 
              (b \[CircleTimes] OverBar[b]))) \[CircleTimes] 
            OverBar[OverBar[b]]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 124} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         ((b \[CircleTimes] OverBar[b]) \[CircleTimes] 
           (OverBar[OverBar[b]] \[CircleTimes] (OverBar[b] \[CircleTimes] 
             b))) \[CircleTimes] OverBar[OverBar[b]]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 123}, 
        "Construct" -> {"SubstitutionLemma", 40}, "Position" -> {1}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (b \[CircleTimes] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           ((b \[CircleTimes] OverBar[b]) \[CircleTimes] 
             (OverBar[OverBar[b]] \[CircleTimes] (OverBar[b] \[CircleTimes] 
               b))) \[CircleTimes] OverBar[OverBar[b]]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 125} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         (OverBar[OverBar[b]] \[CircleTimes] (OverBar[b] \[CircleTimes] 
            b)) \[CircleTimes] ((b \[CircleTimes] OverBar[b]) \[CircleTimes] 
           OverBar[OverBar[b]])], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 124}, "Construct" -> 
         {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           (OverBar[OverBar[b]] \[CircleTimes] (OverBar[b] \[CircleTimes] 
              b)) \[CircleTimes] ((b \[CircleTimes] OverBar[
               b]) \[CircleTimes] OverBar[OverBar[b]])], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 126} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         OverBar[OverBar[b]] \[CircleTimes] 
          ((OverBar[OverBar[b]] \[CircleTimes] (OverBar[b] \[CircleTimes] 
             b)) \[CircleTimes] (b \[CircleTimes] OverBar[b]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 125}, 
        "Construct" -> {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           OverBar[OverBar[b]] \[CircleTimes] 
            ((OverBar[OverBar[b]] \[CircleTimes] (OverBar[b] \[CircleTimes] 
               b)) \[CircleTimes] (b \[CircleTimes] OverBar[b]))], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 127} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         OverBar[OverBar[b]] \[CircleTimes] (OverBar[b] \[CircleTimes] 
           (b \[CircleTimes] (OverBar[OverBar[b]] \[CircleTimes] 
             (OverBar[b] \[CircleTimes] b))))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 126}, 
        "Construct" -> {"SubstitutionLemma", 40}, "Position" -> {2}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (b \[CircleTimes] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           OverBar[OverBar[b]] \[CircleTimes] (OverBar[b] \[CircleTimes] 
             (b \[CircleTimes] (OverBar[OverBar[b]] \[CircleTimes] (
                OverBar[b] \[CircleTimes] b))))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 128} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         (b \[CircleTimes] (OverBar[OverBar[b]] \[CircleTimes] 
            (OverBar[b] \[CircleTimes] b))) \[CircleTimes] 
          (OverBar[OverBar[b]] \[CircleTimes] OverBar[b])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 127}, 
        "Construct" -> {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           (b \[CircleTimes] (OverBar[OverBar[b]] \[CircleTimes] 
              (OverBar[b] \[CircleTimes] b))) \[CircleTimes] 
            (OverBar[OverBar[b]] \[CircleTimes] OverBar[b])], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 129} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         OverBar[b] \[CircleTimes] ((b \[CircleTimes] 
            (OverBar[OverBar[b]] \[CircleTimes] (OverBar[b] \[CircleTimes] 
              b))) \[CircleTimes] OverBar[OverBar[b]])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 128}, 
        "Construct" -> {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           OverBar[b] \[CircleTimes] ((b \[CircleTimes] (OverBar[
                OverBar[b]] \[CircleTimes] (OverBar[b] \[CircleTimes] 
                b))) \[CircleTimes] OverBar[OverBar[b]])], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 130} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         ((b \[CircleTimes] (OverBar[OverBar[b]] \[CircleTimes] 
             (OverBar[b] \[CircleTimes] b))) \[CircleTimes] 
           OverBar[OverBar[b]]) \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 129}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a] \[CircleTimes] a == 
           ((b \[CircleTimes] (OverBar[OverBar[b]] \[CircleTimes] (
                OverBar[b] \[CircleTimes] b))) \[CircleTimes] 
             OverBar[OverBar[b]]) \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 131} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         ((OverBar[OverBar[b]] \[CircleTimes] (OverBar[b] \[CircleTimes] 
             b)) \[CircleTimes] (b \[CircleTimes] OverBar[
             OverBar[b]])) \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 130}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           ((OverBar[OverBar[b]] \[CircleTimes] (OverBar[b] \[CircleTimes] 
               b)) \[CircleTimes] (b \[CircleTimes] OverBar[OverBar[
                b]])) \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 132} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         (b \[CircleTimes] OverBar[OverBar[b]]) \[CircleTimes] 
          ((OverBar[OverBar[b]] \[CircleTimes] (OverBar[b] \[CircleTimes] 
             b)) \[CircleTimes] OverBar[b])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 131}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           (b \[CircleTimes] OverBar[OverBar[b]]) \[CircleTimes] 
            ((OverBar[OverBar[b]] \[CircleTimes] (OverBar[b] \[CircleTimes] 
               b)) \[CircleTimes] OverBar[b])], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 133} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         OverBar[b] \[CircleTimes] ((b \[CircleTimes] 
            OverBar[OverBar[b]]) \[CircleTimes] 
           (OverBar[OverBar[b]] \[CircleTimes] (OverBar[b] \[CircleTimes] 
             b)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 132}, 
        "Construct" -> {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           OverBar[b] \[CircleTimes] ((b \[CircleTimes] OverBar[OverBar[
                b]]) \[CircleTimes] (OverBar[OverBar[b]] \[CircleTimes] 
              (OverBar[b] \[CircleTimes] b)))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 134} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         OverBar[b] \[CircleTimes] ((OverBar[b] \[CircleTimes] 
            b) \[CircleTimes] (OverBar[OverBar[b]] \[CircleTimes] 
            (b \[CircleTimes] OverBar[OverBar[b]])))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 133}, 
        "Construct" -> {"SubstitutionLemma", 40}, "Position" -> {2}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (b \[CircleTimes] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           OverBar[b] \[CircleTimes] ((OverBar[b] \[CircleTimes] 
              b) \[CircleTimes] (OverBar[OverBar[b]] \[CircleTimes] 
              (b \[CircleTimes] OverBar[OverBar[b]])))], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 135} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         (OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] 
            OverBar[OverBar[b]])) \[CircleTimes] ((OverBar[b] \[CircleTimes] 
            b) \[CircleTimes] OverBar[b])], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 134}, "Construct" -> 
         {"SubstitutionLemma", 40}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (b \[CircleTimes] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           (OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] 
              OverBar[OverBar[b]])) \[CircleTimes] 
            ((OverBar[b] \[CircleTimes] b) \[CircleTimes] OverBar[b])], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 136} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         (OverBar[b] \[CircleTimes] b) \[CircleTimes] 
          ((OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] 
             OverBar[OverBar[b]])) \[CircleTimes] OverBar[b])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 135}, 
        "Construct" -> {"SubstitutionLemma", 73}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           (OverBar[b] \[CircleTimes] b) \[CircleTimes] 
            ((OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] OverBar[
                OverBar[b]])) \[CircleTimes] OverBar[b])], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 137} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         b \[CircleTimes] (OverBar[b] \[CircleTimes] 
           ((OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] 
              OverBar[OverBar[b]])) \[CircleTimes] OverBar[b]))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 136}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           b \[CircleTimes] (OverBar[b] \[CircleTimes] 
             ((OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] 
                OverBar[OverBar[b]])) \[CircleTimes] OverBar[b]))], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 138} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         ((OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] 
             OverBar[OverBar[b]])) \[CircleTimes] OverBar[b]) \[CircleTimes] 
          (b \[CircleTimes] OverBar[b])], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 137}, "Construct" -> 
         {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           ((OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] OverBar[
                OverBar[b]])) \[CircleTimes] OverBar[b]) \[CircleTimes] 
            (b \[CircleTimes] OverBar[b])], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 139} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         OverBar[b] \[CircleTimes] (((OverBar[OverBar[b]] \[CircleTimes] 
             (b \[CircleTimes] OverBar[OverBar[b]])) \[CircleTimes] 
            OverBar[b]) \[CircleTimes] b)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 138}, "Construct" -> 
         {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           OverBar[b] \[CircleTimes] (((OverBar[OverBar[b]] \[CircleTimes] (
                b \[CircleTimes] OverBar[OverBar[b]])) \[CircleTimes] 
              OverBar[b]) \[CircleTimes] b)], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 140} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         (((OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] 
              OverBar[OverBar[b]])) \[CircleTimes] OverBar[b]) \[CircleTimes] 
           b) \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 139}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {}, "Rule" -> (a_) \[CircleTimes] (b_) -> 
          b \[CircleTimes] a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[OverBar[a] \[CircleTimes] a == 
           (((OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] 
                OverBar[OverBar[b]])) \[CircleTimes] OverBar[
               b]) \[CircleTimes] b) \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 141} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         (OverBar[b] \[CircleTimes] ((OverBar[OverBar[b]] \[CircleTimes] 
             (b \[CircleTimes] OverBar[OverBar[b]])) \[CircleTimes] 
            b)) \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 140}, "Construct" -> 
         {"SubstitutionLemma", 22}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           (OverBar[b] \[CircleTimes] ((OverBar[OverBar[b]] \[CircleTimes] (
                b \[CircleTimes] OverBar[OverBar[b]])) \[CircleTimes] 
              b)) \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 142} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         ((OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] 
             OverBar[OverBar[b]])) \[CircleTimes] b) \[CircleTimes] 
          (OverBar[b] \[CircleTimes] OverBar[b])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 141}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           ((OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] OverBar[
                OverBar[b]])) \[CircleTimes] b) \[CircleTimes] 
            (OverBar[b] \[CircleTimes] OverBar[b])], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 143} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         OverBar[b] \[CircleTimes] (((OverBar[OverBar[b]] \[CircleTimes] 
             (b \[CircleTimes] OverBar[OverBar[b]])) \[CircleTimes] 
            b) \[CircleTimes] OverBar[b])], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 142}, "Construct" -> 
         {"SubstitutionLemma", 73}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           OverBar[b] \[CircleTimes] (((OverBar[OverBar[b]] \[CircleTimes] (
                b \[CircleTimes] OverBar[OverBar[b]])) \[CircleTimes] 
              b) \[CircleTimes] OverBar[b])], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 144} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         OverBar[b] \[CircleTimes] ((OverBar[OverBar[b]] \[CircleTimes] 
            (b \[CircleTimes] OverBar[OverBar[b]])) \[CircleTimes] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 143}, 
        "Construct" -> {"SubstitutionLemma", 38}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (a_)) -> 
          a \[CircleTimes] b, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[OverBar[a] \[CircleTimes] a == OverBar[b] \[CircleTimes] 
            ((OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] OverBar[
                OverBar[b]])) \[CircleTimes] b)], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 145} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         b \[CircleTimes] ((OverBar[OverBar[b]] \[CircleTimes] 
            (b \[CircleTimes] OverBar[OverBar[b]])) \[CircleTimes] 
           OverBar[b])], "Proof" -> <|"Input" -> {"SubstitutionLemma", 144}, 
        "Construct" -> {"SubstitutionLemma", 40}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (b \[CircleTimes] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           b \[CircleTimes] ((OverBar[OverBar[b]] \[CircleTimes] 
              (b \[CircleTimes] OverBar[OverBar[b]])) \[CircleTimes] 
             OverBar[b])], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 146} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         (OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] 
            OverBar[OverBar[b]])) \[CircleTimes] (b \[CircleTimes] 
           OverBar[b])], "Proof" -> <|"Input" -> {"SubstitutionLemma", 145}, 
        "Construct" -> {"SubstitutionLemma", 73}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           (OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] 
              OverBar[OverBar[b]])) \[CircleTimes] (b \[CircleTimes] 
             OverBar[b])], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 147} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         (b \[CircleTimes] OverBar[OverBar[b]]) \[CircleTimes] 
          (OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] 
            OverBar[b]))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 
          146}, "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           (b \[CircleTimes] OverBar[OverBar[b]]) \[CircleTimes] 
            (OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] 
              OverBar[b]))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 148} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         (b \[CircleTimes] OverBar[b]) \[CircleTimes] 
          ((b \[CircleTimes] OverBar[OverBar[b]]) \[CircleTimes] 
           OverBar[OverBar[b]])], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 147}, "Construct" -> 
         {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           (b \[CircleTimes] OverBar[b]) \[CircleTimes] 
            ((b \[CircleTimes] OverBar[OverBar[b]]) \[CircleTimes] 
             OverBar[OverBar[b]])], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 149} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         OverBar[OverBar[b]] \[CircleTimes] ((b \[CircleTimes] 
            OverBar[b]) \[CircleTimes] (b \[CircleTimes] 
            OverBar[OverBar[b]]))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 148}, "Construct" -> 
         {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           OverBar[OverBar[b]] \[CircleTimes] ((b \[CircleTimes] 
              OverBar[b]) \[CircleTimes] (b \[CircleTimes] OverBar[OverBar[
                b]]))], "Source" -> "cpl"|>|>, {"SubstitutionLemma", 150} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         ((b \[CircleTimes] OverBar[b]) \[CircleTimes] (b \[CircleTimes] 
            OverBar[OverBar[b]])) \[CircleTimes] OverBar[OverBar[b]]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 149}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a] \[CircleTimes] a == 
           ((b \[CircleTimes] OverBar[b]) \[CircleTimes] (b \[CircleTimes] 
              OverBar[OverBar[b]])) \[CircleTimes] OverBar[OverBar[b]]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 151} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         (OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] 
            (b \[CircleTimes] OverBar[b]))) \[CircleTimes] 
          OverBar[OverBar[b]]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 150}, "Construct" -> 
         {"SubstitutionLemma", 40}, "Position" -> {1}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (b \[CircleTimes] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           (OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] 
              (b \[CircleTimes] OverBar[b]))) \[CircleTimes] 
            OverBar[OverBar[b]]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 152} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         (b \[CircleTimes] (b \[CircleTimes] OverBar[b])) \[CircleTimes] 
          (OverBar[OverBar[b]] \[CircleTimes] OverBar[OverBar[b]])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 151}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           (b \[CircleTimes] (b \[CircleTimes] OverBar[b])) \[CircleTimes] 
            (OverBar[OverBar[b]] \[CircleTimes] OverBar[OverBar[b]])], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 153} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         OverBar[OverBar[b]] \[CircleTimes] ((b \[CircleTimes] 
            (b \[CircleTimes] OverBar[b])) \[CircleTimes] 
           OverBar[OverBar[b]])], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 152}, "Construct" -> 
         {"SubstitutionLemma", 73}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           OverBar[OverBar[b]] \[CircleTimes] ((b \[CircleTimes] 
              (b \[CircleTimes] OverBar[b])) \[CircleTimes] 
             OverBar[OverBar[b]])], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 154} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         OverBar[OverBar[b]] \[CircleTimes] ((OverBar[b] \[CircleTimes] 
            (b \[CircleTimes] b)) \[CircleTimes] OverBar[OverBar[b]])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 153}, 
        "Construct" -> {"SubstitutionLemma", 48}, "Position" -> {2, 1}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           OverBar[OverBar[b]] \[CircleTimes] ((OverBar[b] \[CircleTimes] 
              (b \[CircleTimes] b)) \[CircleTimes] OverBar[OverBar[b]])], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 155} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         ((OverBar[b] \[CircleTimes] (b \[CircleTimes] b)) \[CircleTimes] 
           OverBar[OverBar[b]]) \[CircleTimes] OverBar[OverBar[b]]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 154}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a] \[CircleTimes] a == ((OverBar[b] \[CircleTimes] 
              (b \[CircleTimes] b)) \[CircleTimes] OverBar[
              OverBar[b]]) \[CircleTimes] OverBar[OverBar[b]]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 156} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         ((b \[CircleTimes] b) \[CircleTimes] (OverBar[b] \[CircleTimes] 
            OverBar[OverBar[b]])) \[CircleTimes] OverBar[OverBar[b]]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 155}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           ((b \[CircleTimes] b) \[CircleTimes] (OverBar[b] \[CircleTimes] 
              OverBar[OverBar[b]])) \[CircleTimes] OverBar[OverBar[b]]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 157} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         (OverBar[b] \[CircleTimes] OverBar[OverBar[b]]) \[CircleTimes] 
          ((b \[CircleTimes] b) \[CircleTimes] OverBar[OverBar[b]])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 156}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           (OverBar[b] \[CircleTimes] OverBar[OverBar[b]]) \[CircleTimes] 
            ((b \[CircleTimes] b) \[CircleTimes] OverBar[OverBar[b]])], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 158} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         OverBar[OverBar[b]] \[CircleTimes] ((OverBar[b] \[CircleTimes] 
            OverBar[OverBar[b]]) \[CircleTimes] (b \[CircleTimes] b))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 157}, 
        "Construct" -> {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           OverBar[OverBar[b]] \[CircleTimes] ((OverBar[b] \[CircleTimes] 
              OverBar[OverBar[b]]) \[CircleTimes] (b \[CircleTimes] b))], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 159} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         (b \[CircleTimes] b) \[CircleTimes] 
          (OverBar[OverBar[b]] \[CircleTimes] (OverBar[b] \[CircleTimes] 
            OverBar[OverBar[b]]))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 158}, "Construct" -> 
         {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           (b \[CircleTimes] b) \[CircleTimes] 
            (OverBar[OverBar[b]] \[CircleTimes] (OverBar[b] \[CircleTimes] 
              OverBar[OverBar[b]]))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 160} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         (OverBar[OverBar[b]] \[CircleTimes] (OverBar[b] \[CircleTimes] 
            OverBar[OverBar[b]])) \[CircleTimes] (b \[CircleTimes] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 159}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a] \[CircleTimes] a == (OverBar[OverBar[b]] \[CircleTimes] 
             (OverBar[b] \[CircleTimes] OverBar[OverBar[b]])) \[CircleTimes] 
            (b \[CircleTimes] b)], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 161} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         (OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] 
            OverBar[b])) \[CircleTimes] (b \[CircleTimes] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 160}, 
        "Construct" -> {"CriticalPairLemma", 38}, "Position" -> {1, 2}, 
        "Rule" -> (a_) \[CircleTimes] OverBar[a_] -> b \[CircleTimes] 
           OverBar[b], "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[OverBar[a] \[CircleTimes] a == 
           (OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] 
              OverBar[b])) \[CircleTimes] (b \[CircleTimes] b)], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 162} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         b \[CircleTimes] (b \[CircleTimes] 
           (OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] 
             OverBar[b])))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 161}, "Construct" -> 
         {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (c_) \[CircleTimes] ((a_) \[CircleTimes] (b_)) -> 
          a \[CircleTimes] (b \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           b \[CircleTimes] (b \[CircleTimes] (OverBar[OverBar[
                b]] \[CircleTimes] (b \[CircleTimes] OverBar[b])))], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 163} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         b \[CircleTimes] (OverBar[OverBar[b]] \[CircleTimes] 
           (b \[CircleTimes] OverBar[b]))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 162}, "Construct" -> 
         {"CriticalPairLemma", 39}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((a_) \[CircleTimes] (b_)) -> 
          a \[CircleTimes] b, "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[OverBar[a] \[CircleTimes] a == b \[CircleTimes] 
            (OverBar[OverBar[b]] \[CircleTimes] (b \[CircleTimes] 
              OverBar[b]))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 164} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         (b \[CircleTimes] OverBar[b]) \[CircleTimes] 
          (OverBar[OverBar[b]] \[CircleTimes] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 163}, 
        "Construct" -> {"SubstitutionLemma", 40}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (b \[CircleTimes] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           (b \[CircleTimes] OverBar[b]) \[CircleTimes] 
            (OverBar[OverBar[b]] \[CircleTimes] b)], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 165} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         OverBar[OverBar[b]] \[CircleTimes] ((b \[CircleTimes] 
            OverBar[b]) \[CircleTimes] b)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 164}, "Construct" -> 
         {"SubstitutionLemma", 73}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           OverBar[OverBar[b]] \[CircleTimes] ((b \[CircleTimes] 
              OverBar[b]) \[CircleTimes] b)], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 166} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         ((b \[CircleTimes] OverBar[b]) \[CircleTimes] b) \[CircleTimes] 
          OverBar[OverBar[b]]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 165}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {}, "Rule" -> (a_) \[CircleTimes] (b_) -> 
          b \[CircleTimes] a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[OverBar[a] \[CircleTimes] a == 
           ((b \[CircleTimes] OverBar[b]) \[CircleTimes] b) \[CircleTimes] 
            OverBar[OverBar[b]]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 167} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         ((b \[CircleTimes] OverBar[b]) \[CircleTimes] b) \[CircleTimes] b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 166}, 
        "Construct" -> {"SubstitutionLemma", 8}, "Position" -> {2}, 
        "Rule" -> OverBar[OverBar[a_]] -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           ((b \[CircleTimes] OverBar[b]) \[CircleTimes] b) \[CircleTimes] 
            b], "Source" -> "cpl"|>|>, {"SubstitutionLemma", 168} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         (OverBar[b] \[CircleTimes] (b \[CircleTimes] b)) \[CircleTimes] b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 167}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           (OverBar[b] \[CircleTimes] (b \[CircleTimes] b)) \[CircleTimes] 
            b], "Source" -> "cpl"|>|>, {"SubstitutionLemma", 169} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         (b \[CircleTimes] b) \[CircleTimes] (OverBar[b] \[CircleTimes] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 168}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           (b \[CircleTimes] b) \[CircleTimes] (OverBar[b] \[CircleTimes] 
             b)], "Source" -> "cpl"|>|>, {"SubstitutionLemma", 170} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         b \[CircleTimes] ((b \[CircleTimes] b) \[CircleTimes] OverBar[b])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 169}, 
        "Construct" -> {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           b \[CircleTimes] ((b \[CircleTimes] b) \[CircleTimes] 
             OverBar[b])], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 171} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         OverBar[b] \[CircleTimes] (b \[CircleTimes] (b \[CircleTimes] b))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 170}, 
        "Construct" -> {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           OverBar[b] \[CircleTimes] (b \[CircleTimes] (b \[CircleTimes] 
              b))], "Source" -> "cpl"|>|>, {"SubstitutionLemma", 172} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         (b \[CircleTimes] (b \[CircleTimes] b)) \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 171}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a] \[CircleTimes] a == (b \[CircleTimes] (b \[CircleTimes] 
              b)) \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 173} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         (b \[CircleTimes] (b \[CircleTimes] b)) \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 172}, 
        "Construct" -> {"SubstitutionLemma", 73}, "Position" -> {1}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           (b \[CircleTimes] (b \[CircleTimes] b)) \[CircleTimes] 
            OverBar[b]], "Source" -> "cpl"|>|>, {"SubstitutionLemma", 174} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         (b \[CircleTimes] b) \[CircleTimes] (b \[CircleTimes] OverBar[b])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 173}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           (b \[CircleTimes] b) \[CircleTimes] (b \[CircleTimes] 
             OverBar[b])], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 175} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] a == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 174}, "Construct" -> 
         {"CriticalPairLemma", 41}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] OverBar[b_]) -> 
          b \[CircleTimes] OverBar[b], "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] a == 
           b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 176} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] OverBar[a] == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 175}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {}, "Rule" -> (a_) \[CircleTimes] (b_) -> 
          b \[CircleTimes] a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[a \[CircleTimes] OverBar[a] == b \[CircleTimes] 
            OverBar[b]], "Source" -> "cpl"|>|>, {"SubstitutionLemma", 177} -> 
     <|"Statement" -> HoldForm[
        (a \[CircleTimes] OverBar[OverBar[a]]) \[CircleTimes] OverBar[a] == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 176}, "Construct" -> 
         {"CriticalPairLemma", 3}, "Position" -> {1}, 
        "Rule" -> a_ -> a \[CircleTimes] OverBar[OverBar[a]], 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (a \[CircleTimes] OverBar[OverBar[a]]) \[CircleTimes] OverBar[a] == 
           b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 178} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[a]] \[CircleTimes] 
          (a \[CircleTimes] OverBar[a]) == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 177}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[OverBar[OverBar[a]] \[CircleTimes] 
            (a \[CircleTimes] OverBar[a]) == b \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 179} -> 
     <|"Statement" -> HoldForm[(OverBar[OverBar[a]] \[CircleTimes] 
           (a \[CircleTimes] OverBar[a])) \[CircleTimes] 
          (OverBar[OverBar[a]] \[CircleTimes] (a \[CircleTimes] 
            OverBar[a])) == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 178}, 
        "Construct" -> {"CriticalPairLemma", 5}, "Position" -> {}, 
        "Rule" -> a_ -> a \[CircleTimes] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(OverBar[OverBar[a]] \[CircleTimes] 
             (a \[CircleTimes] OverBar[a])) \[CircleTimes] 
            (OverBar[OverBar[a]] \[CircleTimes] (a \[CircleTimes] 
              OverBar[a])) == b \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 180} -> 
     <|"Statement" -> HoldForm[(a \[CircleTimes] OverBar[a]) \[CircleTimes] 
          (OverBar[OverBar[a]] \[CircleTimes] 
           (OverBar[OverBar[a]] \[CircleTimes] (a \[CircleTimes] 
             OverBar[a]))) == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 179}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          (a \[CircleTimes] OverBar[a]) \[CircleTimes] 
            (OverBar[OverBar[a]] \[CircleTimes] (OverBar[OverBar[
                a]] \[CircleTimes] (a \[CircleTimes] OverBar[a]))) == 
           b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 181} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] (a \[CircleTimes] 
           (OverBar[OverBar[a]] \[CircleTimes] 
            (OverBar[OverBar[a]] \[CircleTimes] (a \[CircleTimes] 
              OverBar[a])))) == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 180}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] 
            (a \[CircleTimes] (OverBar[OverBar[a]] \[CircleTimes] 
              (OverBar[OverBar[a]] \[CircleTimes] (a \[CircleTimes] 
                OverBar[a])))) == b \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 182} -> 
     <|"Statement" -> HoldForm[
        (a \[CircleTimes] (OverBar[OverBar[a]] \[CircleTimes] 
            (OverBar[OverBar[a]] \[CircleTimes] (a \[CircleTimes] 
              OverBar[a])))) \[CircleTimes] OverBar[a] == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 181}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {}, "Rule" -> (a_) \[CircleTimes] (b_) -> 
          b \[CircleTimes] a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[(a \[CircleTimes] (OverBar[OverBar[a]] \[CircleTimes] 
              (OverBar[OverBar[a]] \[CircleTimes] (a \[CircleTimes] 
                OverBar[a])))) \[CircleTimes] OverBar[a] == 
           b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 183} -> 
     <|"Statement" -> HoldForm[((OverBar[OverBar[a]] \[CircleTimes] 
            (OverBar[OverBar[a]] \[CircleTimes] (a \[CircleTimes] 
              OverBar[a]))) \[CircleTimes] a) \[CircleTimes] OverBar[a] == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 182}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {1}, "Rule" -> (a_) \[CircleTimes] (b_) -> 
          b \[CircleTimes] a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[((OverBar[OverBar[a]] \[CircleTimes] (OverBar[
                OverBar[a]] \[CircleTimes] (a \[CircleTimes] OverBar[
                 a]))) \[CircleTimes] a) \[CircleTimes] OverBar[a] == 
           b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 184} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] 
          ((OverBar[OverBar[a]] \[CircleTimes] 
            (OverBar[OverBar[a]] \[CircleTimes] (a \[CircleTimes] 
              OverBar[a]))) \[CircleTimes] OverBar[a]) == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 183}, "Construct" -> 
         {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CircleTimes] 
            ((OverBar[OverBar[a]] \[CircleTimes] (OverBar[OverBar[
                 a]] \[CircleTimes] (a \[CircleTimes] OverBar[
                 a]))) \[CircleTimes] OverBar[a]) == b \[CircleTimes] 
            OverBar[b]], "Source" -> "cpl"|>|>, {"SubstitutionLemma", 185} -> 
     <|"Statement" -> HoldForm[((OverBar[OverBar[a]] \[CircleTimes] 
            (OverBar[OverBar[a]] \[CircleTimes] (a \[CircleTimes] 
              OverBar[a]))) \[CircleTimes] OverBar[a]) \[CircleTimes] a == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 184}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {}, "Rule" -> (a_) \[CircleTimes] (b_) -> 
          b \[CircleTimes] a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[((OverBar[OverBar[a]] \[CircleTimes] (OverBar[
                OverBar[a]] \[CircleTimes] (a \[CircleTimes] OverBar[
                 a]))) \[CircleTimes] OverBar[a]) \[CircleTimes] a == 
           b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 186} -> 
     <|"Statement" -> HoldForm[((OverBar[OverBar[a]] \[CircleTimes] 
            (a \[CircleTimes] OverBar[a])) \[CircleTimes] 
           (OverBar[OverBar[a]] \[CircleTimes] OverBar[a])) \[CircleTimes] 
          a == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 185}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[((OverBar[OverBar[a]] \[CircleTimes] 
              (a \[CircleTimes] OverBar[a])) \[CircleTimes] 
             (OverBar[OverBar[a]] \[CircleTimes] OverBar[a])) \[CircleTimes] 
            a == b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 187} -> 
     <|"Statement" -> HoldForm[(OverBar[OverBar[a]] \[CircleTimes] 
           OverBar[a]) \[CircleTimes] ((OverBar[OverBar[a]] \[CircleTimes] 
            (a \[CircleTimes] OverBar[a])) \[CircleTimes] a) == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 186}, "Construct" -> 
         {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(OverBar[OverBar[a]] \[CircleTimes] 
             OverBar[a]) \[CircleTimes] ((OverBar[OverBar[a]] \[CircleTimes] 
              (a \[CircleTimes] OverBar[a])) \[CircleTimes] a) == 
           b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 188} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] 
          (OverBar[OverBar[a]] \[CircleTimes] 
           ((OverBar[OverBar[a]] \[CircleTimes] (a \[CircleTimes] 
              OverBar[a])) \[CircleTimes] a)) == b \[CircleTimes] 
          OverBar[b]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 187}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] 
            (OverBar[OverBar[a]] \[CircleTimes] 
             ((OverBar[OverBar[a]] \[CircleTimes] (a \[CircleTimes] 
                OverBar[a])) \[CircleTimes] a)) == b \[CircleTimes] 
            OverBar[b]], "Source" -> "cpl"|>|>, {"SubstitutionLemma", 189} -> 
     <|"Statement" -> HoldForm[(OverBar[OverBar[a]] \[CircleTimes] 
           ((OverBar[OverBar[a]] \[CircleTimes] (a \[CircleTimes] 
              OverBar[a])) \[CircleTimes] a)) \[CircleTimes] OverBar[a] == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 188}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {}, "Rule" -> (a_) \[CircleTimes] (b_) -> 
          b \[CircleTimes] a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[(OverBar[OverBar[a]] \[CircleTimes] 
             ((OverBar[OverBar[a]] \[CircleTimes] (a \[CircleTimes] 
                OverBar[a])) \[CircleTimes] a)) \[CircleTimes] OverBar[a] == 
           b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 190} -> 
     <|"Statement" -> HoldForm[
        (((OverBar[OverBar[a]] \[CircleTimes] (a \[CircleTimes] 
              OverBar[a])) \[CircleTimes] a) \[CircleTimes] 
           OverBar[OverBar[a]]) \[CircleTimes] OverBar[a] == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 189}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {1}, "Rule" -> (a_) \[CircleTimes] (b_) -> 
          b \[CircleTimes] a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[(((OverBar[OverBar[a]] \[CircleTimes] (a \[CircleTimes] 
                OverBar[a])) \[CircleTimes] a) \[CircleTimes] 
             OverBar[OverBar[a]]) \[CircleTimes] OverBar[a] == 
           b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 191} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[a]] \[CircleTimes] 
          (((OverBar[OverBar[a]] \[CircleTimes] (a \[CircleTimes] 
              OverBar[a])) \[CircleTimes] a) \[CircleTimes] OverBar[a]) == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 190}, "Construct" -> 
         {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[OverBar[OverBar[a]] \[CircleTimes] 
            (((OverBar[OverBar[a]] \[CircleTimes] (a \[CircleTimes] 
                OverBar[a])) \[CircleTimes] a) \[CircleTimes] OverBar[a]) == 
           b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 192} -> 
     <|"Statement" -> HoldForm[
        (((OverBar[OverBar[a]] \[CircleTimes] (a \[CircleTimes] 
              OverBar[a])) \[CircleTimes] a) \[CircleTimes] 
           OverBar[a]) \[CircleTimes] OverBar[OverBar[a]] == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 191}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {}, "Rule" -> (a_) \[CircleTimes] (b_) -> 
          b \[CircleTimes] a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[(((OverBar[OverBar[a]] \[CircleTimes] (a \[CircleTimes] 
                OverBar[a])) \[CircleTimes] a) \[CircleTimes] 
             OverBar[a]) \[CircleTimes] OverBar[OverBar[a]] == 
           b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 193} -> 
     <|"Statement" -> HoldForm[
        (((OverBar[a] \[CircleTimes] (a \[CircleTimes] OverBar[OverBar[
                a]])) \[CircleTimes] a) \[CircleTimes] 
           OverBar[a]) \[CircleTimes] OverBar[OverBar[a]] == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 192}, "Construct" -> 
         {"SubstitutionLemma", 40}, "Position" -> {1, 1, 1}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (b \[CircleTimes] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          (((OverBar[a] \[CircleTimes] (a \[CircleTimes] OverBar[
                 OverBar[a]])) \[CircleTimes] a) \[CircleTimes] 
             OverBar[a]) \[CircleTimes] OverBar[OverBar[a]] == 
           b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 194} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] 
          (((OverBar[a] \[CircleTimes] (a \[CircleTimes] OverBar[OverBar[
                a]])) \[CircleTimes] a) \[CircleTimes] 
           OverBar[OverBar[a]]) == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 193}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] 
            (((OverBar[a] \[CircleTimes] (a \[CircleTimes] OverBar[
                 OverBar[a]])) \[CircleTimes] a) \[CircleTimes] 
             OverBar[OverBar[a]]) == b \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 195} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[a]] \[CircleTimes] 
          (OverBar[a] \[CircleTimes] ((OverBar[a] \[CircleTimes] 
             (a \[CircleTimes] OverBar[OverBar[a]])) \[CircleTimes] a)) == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 194}, "Construct" -> 
         {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[OverBar[OverBar[a]] \[CircleTimes] 
            (OverBar[a] \[CircleTimes] ((OverBar[a] \[CircleTimes] (
                a \[CircleTimes] OverBar[OverBar[a]])) \[CircleTimes] a)) == 
           b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 196} -> 
     <|"Statement" -> HoldForm[((OverBar[a] \[CircleTimes] (a \[CircleTimes] 
             OverBar[OverBar[a]])) \[CircleTimes] a) \[CircleTimes] 
          (OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 195}, "Construct" -> 
         {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          ((OverBar[a] \[CircleTimes] (a \[CircleTimes] OverBar[
                OverBar[a]])) \[CircleTimes] a) \[CircleTimes] 
            (OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) == 
           b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 197} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] ((OverBar[a] \[CircleTimes] 
            (a \[CircleTimes] OverBar[OverBar[a]])) \[CircleTimes] 
           (OverBar[OverBar[a]] \[CircleTimes] OverBar[a])) == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 196}, "Construct" -> 
         {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CircleTimes] 
            ((OverBar[a] \[CircleTimes] (a \[CircleTimes] OverBar[
                OverBar[a]])) \[CircleTimes] (OverBar[OverBar[
                a]] \[CircleTimes] OverBar[a])) == b \[CircleTimes] 
            OverBar[b]], "Source" -> "cpl"|>|>, {"SubstitutionLemma", 198} -> 
     <|"Statement" -> HoldForm[(OverBar[OverBar[a]] \[CircleTimes] 
           OverBar[a]) \[CircleTimes] (a \[CircleTimes] 
           (OverBar[a] \[CircleTimes] (a \[CircleTimes] 
             OverBar[OverBar[a]]))) == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 197}, 
        "Construct" -> {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(OverBar[OverBar[a]] \[CircleTimes] 
             OverBar[a]) \[CircleTimes] (a \[CircleTimes] 
             (OverBar[a] \[CircleTimes] (a \[CircleTimes] OverBar[
                OverBar[a]]))) == b \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 199} -> 
     <|"Statement" -> HoldForm[(OverBar[a] \[CircleTimes] (a \[CircleTimes] 
            OverBar[OverBar[a]])) \[CircleTimes] 
          ((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
           a) == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 198}, 
        "Construct" -> {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(OverBar[a] \[CircleTimes] 
             (a \[CircleTimes] OverBar[OverBar[a]])) \[CircleTimes] 
            ((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
             a) == b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 200} -> 
     <|"Statement" -> HoldForm[
        (a \[CircleTimes] OverBar[OverBar[a]]) \[CircleTimes] 
          (OverBar[a] \[CircleTimes] ((OverBar[OverBar[a]] \[CircleTimes] 
             OverBar[a]) \[CircleTimes] a)) == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 199}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          (a \[CircleTimes] OverBar[OverBar[a]]) \[CircleTimes] 
            (OverBar[a] \[CircleTimes] ((OverBar[OverBar[a]] \[CircleTimes] 
               OverBar[a]) \[CircleTimes] a)) == b \[CircleTimes] 
            OverBar[b]], "Source" -> "cpl"|>|>, {"SubstitutionLemma", 201} -> 
     <|"Statement" -> HoldForm[((OverBar[OverBar[a]] \[CircleTimes] 
            OverBar[a]) \[CircleTimes] a) \[CircleTimes] 
          ((a \[CircleTimes] OverBar[OverBar[a]]) \[CircleTimes] 
           OverBar[a]) == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 200}, 
        "Construct" -> {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[((OverBar[OverBar[a]] \[CircleTimes] 
              OverBar[a]) \[CircleTimes] a) \[CircleTimes] 
            ((a \[CircleTimes] OverBar[OverBar[a]]) \[CircleTimes] 
             OverBar[a]) == b \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 202} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] 
          (((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
            a) \[CircleTimes] (a \[CircleTimes] OverBar[OverBar[a]])) == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 201}, "Construct" -> 
         {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] 
            (((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
              a) \[CircleTimes] (a \[CircleTimes] OverBar[OverBar[a]])) == 
           b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 203} -> 
     <|"Statement" -> HoldForm[
        (((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
            a) \[CircleTimes] (a \[CircleTimes] OverBar[
             OverBar[a]])) \[CircleTimes] OverBar[a] == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 202}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {}, "Rule" -> (a_) \[CircleTimes] (b_) -> 
          b \[CircleTimes] a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[(((OverBar[OverBar[a]] \[CircleTimes] OverBar[
                a]) \[CircleTimes] a) \[CircleTimes] (a \[CircleTimes] 
              OverBar[OverBar[a]])) \[CircleTimes] OverBar[a] == 
           b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 204} -> 
     <|"Statement" -> HoldForm[(OverBar[OverBar[a]] \[CircleTimes] 
           (a \[CircleTimes] ((OverBar[OverBar[a]] \[CircleTimes] 
              OverBar[a]) \[CircleTimes] a))) \[CircleTimes] OverBar[a] == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 203}, "Construct" -> 
         {"SubstitutionLemma", 40}, "Position" -> {1}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (b \[CircleTimes] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(OverBar[OverBar[a]] \[CircleTimes] 
             (a \[CircleTimes] ((OverBar[OverBar[a]] \[CircleTimes] 
                OverBar[a]) \[CircleTimes] a))) \[CircleTimes] OverBar[a] == 
           b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 205} -> 
     <|"Statement" -> HoldForm[
        (a \[CircleTimes] ((OverBar[OverBar[a]] \[CircleTimes] 
             OverBar[a]) \[CircleTimes] a)) \[CircleTimes] 
          (OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 204}, "Construct" -> 
         {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          (a \[CircleTimes] ((OverBar[OverBar[a]] \[CircleTimes] OverBar[
                a]) \[CircleTimes] a)) \[CircleTimes] 
            (OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) == 
           b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 206} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] 
          ((a \[CircleTimes] ((OverBar[OverBar[a]] \[CircleTimes] 
              OverBar[a]) \[CircleTimes] a)) \[CircleTimes] 
           OverBar[OverBar[a]]) == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 205}, 
        "Construct" -> {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] 
            ((a \[CircleTimes] ((OverBar[OverBar[a]] \[CircleTimes] 
                OverBar[a]) \[CircleTimes] a)) \[CircleTimes] 
             OverBar[OverBar[a]]) == b \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 207} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[a]] \[CircleTimes] 
          (OverBar[a] \[CircleTimes] (a \[CircleTimes] 
            ((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
             a))) == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 206}, 
        "Construct" -> {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[OverBar[OverBar[a]] \[CircleTimes] 
            (OverBar[a] \[CircleTimes] (a \[CircleTimes] 
              ((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
               a))) == b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 208} -> 
     <|"Statement" -> HoldForm[(OverBar[a] \[CircleTimes] (a \[CircleTimes] 
            ((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
             a))) \[CircleTimes] OverBar[OverBar[a]] == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 207}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {}, "Rule" -> (a_) \[CircleTimes] (b_) -> 
          b \[CircleTimes] a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[(OverBar[a] \[CircleTimes] (a \[CircleTimes] 
              ((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
               a))) \[CircleTimes] OverBar[OverBar[a]] == b \[CircleTimes] 
            OverBar[b]], "Source" -> "cpl"|>|>, {"SubstitutionLemma", 209} -> 
     <|"Statement" -> HoldForm[
        (((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
            a) \[CircleTimes] (a \[CircleTimes] OverBar[a])) \[CircleTimes] 
          OverBar[OverBar[a]] == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 208}, 
        "Construct" -> {"SubstitutionLemma", 40}, "Position" -> {1}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (b \[CircleTimes] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          (((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
              a) \[CircleTimes] (a \[CircleTimes] OverBar[a])) \[CircleTimes] 
            OverBar[OverBar[a]] == b \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 210} -> 
     <|"Statement" -> HoldForm[(a \[CircleTimes] OverBar[a]) \[CircleTimes] 
          (((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
            a) \[CircleTimes] OverBar[OverBar[a]]) == b \[CircleTimes] 
          OverBar[b]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 209}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          (a \[CircleTimes] OverBar[a]) \[CircleTimes] 
            (((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
              a) \[CircleTimes] OverBar[OverBar[a]]) == b \[CircleTimes] 
            OverBar[b]], "Source" -> "cpl"|>|>, {"SubstitutionLemma", 211} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[a]] \[CircleTimes] 
          ((a \[CircleTimes] OverBar[a]) \[CircleTimes] 
           ((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
            a)) == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 210}, 
        "Construct" -> {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[OverBar[OverBar[a]] \[CircleTimes] 
            ((a \[CircleTimes] OverBar[a]) \[CircleTimes] 
             ((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
              a)) == b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 212} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[a]] \[CircleTimes] 
          (a \[CircleTimes] ((OverBar[OverBar[a]] \[CircleTimes] 
             OverBar[a]) \[CircleTimes] (a \[CircleTimes] OverBar[a]))) == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 211}, "Construct" -> 
         {"SubstitutionLemma", 40}, "Position" -> {2}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (b \[CircleTimes] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[OverBar[OverBar[a]] \[CircleTimes] 
            (a \[CircleTimes] ((OverBar[OverBar[a]] \[CircleTimes] OverBar[
                a]) \[CircleTimes] (a \[CircleTimes] OverBar[a]))) == 
           b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 213} -> 
     <|"Statement" -> HoldForm[((OverBar[OverBar[a]] \[CircleTimes] 
            OverBar[a]) \[CircleTimes] (a \[CircleTimes] 
            OverBar[a])) \[CircleTimes] (a \[CircleTimes] 
           OverBar[OverBar[a]]) == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 212}, 
        "Construct" -> {"SubstitutionLemma", 40}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (b \[CircleTimes] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[((OverBar[OverBar[a]] \[CircleTimes] 
              OverBar[a]) \[CircleTimes] (a \[CircleTimes] 
              OverBar[a])) \[CircleTimes] (a \[CircleTimes] 
             OverBar[OverBar[a]]) == b \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 214} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] 
          (((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
            (a \[CircleTimes] OverBar[a])) \[CircleTimes] 
           OverBar[OverBar[a]]) == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 213}, 
        "Construct" -> {"SubstitutionLemma", 73}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CircleTimes] 
            (((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
              (a \[CircleTimes] OverBar[a])) \[CircleTimes] 
             OverBar[OverBar[a]]) == b \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 215} -> 
     <|"Statement" -> HoldForm[
        (((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
            (a \[CircleTimes] OverBar[a])) \[CircleTimes] 
           OverBar[OverBar[a]]) \[CircleTimes] a == b \[CircleTimes] 
          OverBar[b]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 214}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
              (a \[CircleTimes] OverBar[a])) \[CircleTimes] 
             OverBar[OverBar[a]]) \[CircleTimes] a == b \[CircleTimes] 
            OverBar[b]], "Source" -> "cpl"|>|>, {"SubstitutionLemma", 216} -> 
     <|"Statement" -> HoldForm[((a \[CircleTimes] OverBar[a]) \[CircleTimes] 
           ((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
            OverBar[OverBar[a]])) \[CircleTimes] a == b \[CircleTimes] 
          OverBar[b]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 215}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          ((a \[CircleTimes] OverBar[a]) \[CircleTimes] 
             ((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
              OverBar[OverBar[a]])) \[CircleTimes] a == b \[CircleTimes] 
            OverBar[b]], "Source" -> "cpl"|>|>, {"SubstitutionLemma", 217} -> 
     <|"Statement" -> HoldForm[((OverBar[OverBar[a]] \[CircleTimes] 
            OverBar[a]) \[CircleTimes] OverBar[OverBar[a]]) \[CircleTimes] 
          ((a \[CircleTimes] OverBar[a]) \[CircleTimes] a) == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 216}, "Construct" -> 
         {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[((OverBar[OverBar[a]] \[CircleTimes] 
              OverBar[a]) \[CircleTimes] OverBar[OverBar[a]]) \[CircleTimes] 
            ((a \[CircleTimes] OverBar[a]) \[CircleTimes] a) == 
           b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 218} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] 
          (((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
            OverBar[OverBar[a]]) \[CircleTimes] (a \[CircleTimes] 
            OverBar[a])) == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 217}, 
        "Construct" -> {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CircleTimes] 
            (((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
              OverBar[OverBar[a]]) \[CircleTimes] (a \[CircleTimes] 
              OverBar[a])) == b \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 219} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (OverBar[a] \[CircleTimes] 
           (a \[CircleTimes] ((OverBar[OverBar[a]] \[CircleTimes] 
              OverBar[a]) \[CircleTimes] OverBar[OverBar[a]]))) == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 218}, "Construct" -> 
         {"SubstitutionLemma", 40}, "Position" -> {2}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (b \[CircleTimes] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CircleTimes] 
            (OverBar[a] \[CircleTimes] (a \[CircleTimes] 
              ((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
               OverBar[OverBar[a]]))) == b \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 220} -> 
     <|"Statement" -> HoldForm[
        (a \[CircleTimes] ((OverBar[OverBar[a]] \[CircleTimes] 
             OverBar[a]) \[CircleTimes] OverBar[OverBar[a]])) \[CircleTimes] 
          (OverBar[a] \[CircleTimes] a) == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 219}, 
        "Construct" -> {"SubstitutionLemma", 40}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (b \[CircleTimes] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          (a \[CircleTimes] ((OverBar[OverBar[a]] \[CircleTimes] OverBar[
                a]) \[CircleTimes] OverBar[OverBar[a]])) \[CircleTimes] 
            (OverBar[a] \[CircleTimes] a) == b \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 221} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] 
          ((a \[CircleTimes] ((OverBar[OverBar[a]] \[CircleTimes] 
              OverBar[a]) \[CircleTimes] OverBar[OverBar[a]])) \[CircleTimes] 
           a) == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 220}, 
        "Construct" -> {"SubstitutionLemma", 73}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] 
            ((a \[CircleTimes] ((OverBar[OverBar[a]] \[CircleTimes] 
                OverBar[a]) \[CircleTimes] OverBar[OverBar[
                 a]])) \[CircleTimes] a) == b \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 222} -> 
     <|"Statement" -> HoldForm[
        ((a \[CircleTimes] ((OverBar[OverBar[a]] \[CircleTimes] 
              OverBar[a]) \[CircleTimes] OverBar[OverBar[a]])) \[CircleTimes] 
           a) \[CircleTimes] OverBar[a] == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 221}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          ((a \[CircleTimes] ((OverBar[OverBar[a]] \[CircleTimes] 
                OverBar[a]) \[CircleTimes] OverBar[OverBar[
                 a]])) \[CircleTimes] a) \[CircleTimes] OverBar[a] == 
           b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 223} -> 
     <|"Statement" -> HoldForm[
        (((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
            OverBar[OverBar[a]]) \[CircleTimes] (a \[CircleTimes] 
            a)) \[CircleTimes] OverBar[a] == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 222}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          (((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
              OverBar[OverBar[a]]) \[CircleTimes] (a \[CircleTimes] 
              a)) \[CircleTimes] OverBar[a] == b \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 224} -> 
     <|"Statement" -> HoldForm[(a \[CircleTimes] a) \[CircleTimes] 
          (((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
            OverBar[OverBar[a]]) \[CircleTimes] OverBar[a]) == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 223}, "Construct" -> 
         {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(a \[CircleTimes] a) \[CircleTimes] 
            (((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
              OverBar[OverBar[a]]) \[CircleTimes] OverBar[a]) == 
           b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 225} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] 
          ((a \[CircleTimes] a) \[CircleTimes] 
           ((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
            OverBar[OverBar[a]])) == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 224}, 
        "Construct" -> {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] 
            ((a \[CircleTimes] a) \[CircleTimes] 
             ((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
              OverBar[OverBar[a]])) == b \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 226} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] 
          (OverBar[OverBar[a]] \[CircleTimes] 
           ((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
            (a \[CircleTimes] a))) == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 225}, 
        "Construct" -> {"SubstitutionLemma", 40}, "Position" -> {2}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (b \[CircleTimes] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] 
            (OverBar[OverBar[a]] \[CircleTimes] 
             ((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
              (a \[CircleTimes] a))) == b \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 227} -> 
     <|"Statement" -> HoldForm[((OverBar[OverBar[a]] \[CircleTimes] 
            OverBar[a]) \[CircleTimes] (a \[CircleTimes] a)) \[CircleTimes] 
          (OverBar[a] \[CircleTimes] OverBar[OverBar[a]]) == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 226}, "Construct" -> 
         {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[((OverBar[OverBar[a]] \[CircleTimes] 
              OverBar[a]) \[CircleTimes] (a \[CircleTimes] a)) \[CircleTimes] 
            (OverBar[a] \[CircleTimes] OverBar[OverBar[a]]) == 
           b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 228} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[a]] \[CircleTimes] 
          (((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
            (a \[CircleTimes] a)) \[CircleTimes] OverBar[a]) == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 227}, "Construct" -> 
         {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[OverBar[OverBar[a]] \[CircleTimes] 
            (((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
              (a \[CircleTimes] a)) \[CircleTimes] OverBar[a]) == 
           b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 229} -> 
     <|"Statement" -> HoldForm[
        (((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
            (a \[CircleTimes] a)) \[CircleTimes] OverBar[a]) \[CircleTimes] 
          OverBar[OverBar[a]] == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 228}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
              (a \[CircleTimes] a)) \[CircleTimes] OverBar[a]) \[CircleTimes] 
            OverBar[OverBar[a]] == b \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 230} -> 
     <|"Statement" -> HoldForm[((a \[CircleTimes] a) \[CircleTimes] 
           ((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
            OverBar[a])) \[CircleTimes] OverBar[OverBar[a]] == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 229}, "Construct" -> 
         {"SubstitutionLemma", 22}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[((a \[CircleTimes] a) \[CircleTimes] 
             ((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
              OverBar[a])) \[CircleTimes] OverBar[OverBar[a]] == 
           b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 231} -> 
     <|"Statement" -> HoldForm[((OverBar[OverBar[a]] \[CircleTimes] 
            OverBar[a]) \[CircleTimes] OverBar[a]) \[CircleTimes] 
          ((a \[CircleTimes] a) \[CircleTimes] OverBar[OverBar[a]]) == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 230}, "Construct" -> 
         {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[((OverBar[OverBar[a]] \[CircleTimes] 
              OverBar[a]) \[CircleTimes] OverBar[a]) \[CircleTimes] 
            ((a \[CircleTimes] a) \[CircleTimes] OverBar[OverBar[a]]) == 
           b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 232} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[a]] \[CircleTimes] 
          (((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
            OverBar[a]) \[CircleTimes] (a \[CircleTimes] a)) == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 231}, "Construct" -> 
         {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[OverBar[OverBar[a]] \[CircleTimes] 
            (((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
              OverBar[a]) \[CircleTimes] (a \[CircleTimes] a)) == 
           b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 233} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[a]] \[CircleTimes] 
          (a \[CircleTimes] (((OverBar[OverBar[a]] \[CircleTimes] 
              OverBar[a]) \[CircleTimes] OverBar[a]) \[CircleTimes] a)) == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 232}, "Construct" -> 
         {"SubstitutionLemma", 73}, "Position" -> {2}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[OverBar[OverBar[a]] \[CircleTimes] 
            (a \[CircleTimes] (((OverBar[OverBar[a]] \[CircleTimes] 
                OverBar[a]) \[CircleTimes] OverBar[a]) \[CircleTimes] a)) == 
           b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 234} -> 
     <|"Statement" -> HoldForm[
        (((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
            OverBar[a]) \[CircleTimes] a) \[CircleTimes] (a \[CircleTimes] 
           OverBar[OverBar[a]]) == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 233}, 
        "Construct" -> {"SubstitutionLemma", 40}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (b \[CircleTimes] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          (((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
              OverBar[a]) \[CircleTimes] a) \[CircleTimes] (a \[CircleTimes] 
             OverBar[OverBar[a]]) == b \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 235} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] 
          ((((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
             OverBar[a]) \[CircleTimes] a) \[CircleTimes] 
           OverBar[OverBar[a]]) == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 234}, 
        "Construct" -> {"SubstitutionLemma", 73}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CircleTimes] 
            ((((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
               OverBar[a]) \[CircleTimes] a) \[CircleTimes] 
             OverBar[OverBar[a]]) == b \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 236} -> 
     <|"Statement" -> HoldForm[
        ((((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
             OverBar[a]) \[CircleTimes] a) \[CircleTimes] 
           OverBar[OverBar[a]]) \[CircleTimes] a == b \[CircleTimes] 
          OverBar[b]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 235}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          ((((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
               OverBar[a]) \[CircleTimes] a) \[CircleTimes] 
             OverBar[OverBar[a]]) \[CircleTimes] a == b \[CircleTimes] 
            OverBar[b]], "Source" -> "cpl"|>|>, {"SubstitutionLemma", 237} -> 
     <|"Statement" -> HoldForm[
        (a \[CircleTimes] (((OverBar[OverBar[a]] \[CircleTimes] 
              OverBar[a]) \[CircleTimes] OverBar[a]) \[CircleTimes] 
            OverBar[OverBar[a]])) \[CircleTimes] a == b \[CircleTimes] 
          OverBar[b]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 236}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          (a \[CircleTimes] (((OverBar[OverBar[a]] \[CircleTimes] 
                OverBar[a]) \[CircleTimes] OverBar[a]) \[CircleTimes] 
              OverBar[OverBar[a]])) \[CircleTimes] a == b \[CircleTimes] 
            OverBar[b]], "Source" -> "cpl"|>|>, {"SubstitutionLemma", 238} -> 
     <|"Statement" -> HoldForm[
        (((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
            OverBar[a]) \[CircleTimes] OverBar[OverBar[a]]) \[CircleTimes] 
          (a \[CircleTimes] a) == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 237}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          (((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
              OverBar[a]) \[CircleTimes] OverBar[OverBar[a]]) \[CircleTimes] 
            (a \[CircleTimes] a) == b \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 239} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] 
          ((((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
             OverBar[a]) \[CircleTimes] OverBar[OverBar[a]]) \[CircleTimes] 
           a) == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 238}, 
        "Construct" -> {"SubstitutionLemma", 73}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CircleTimes] 
            ((((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
               OverBar[a]) \[CircleTimes] OverBar[OverBar[a]]) \[CircleTimes] 
             a) == b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 240} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] ((OverBar[a] \[CircleTimes] 
            ((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
             OverBar[OverBar[a]])) \[CircleTimes] a) == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 239}, "Construct" -> 
         {"SubstitutionLemma", 22}, "Position" -> {2, 1}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CircleTimes] 
            ((OverBar[a] \[CircleTimes] ((OverBar[OverBar[a]] \[CircleTimes] 
                OverBar[a]) \[CircleTimes] OverBar[OverBar[
                 a]])) \[CircleTimes] a) == b \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 241} -> 
     <|"Statement" -> HoldForm[((OverBar[a] \[CircleTimes] 
            ((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
             OverBar[OverBar[a]])) \[CircleTimes] a) \[CircleTimes] a == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 240}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {}, "Rule" -> (a_) \[CircleTimes] (b_) -> 
          b \[CircleTimes] a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[((OverBar[a] \[CircleTimes] ((OverBar[OverBar[
                  a]] \[CircleTimes] OverBar[a]) \[CircleTimes] OverBar[
                OverBar[a]])) \[CircleTimes] a) \[CircleTimes] a == 
           b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 242} -> 
     <|"Statement" -> HoldForm[
        (((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
            OverBar[OverBar[a]]) \[CircleTimes] (OverBar[a] \[CircleTimes] 
            a)) \[CircleTimes] a == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 241}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          (((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
              OverBar[OverBar[a]]) \[CircleTimes] (OverBar[a] \[CircleTimes] 
              a)) \[CircleTimes] a == b \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 243} -> 
     <|"Statement" -> HoldForm[(OverBar[a] \[CircleTimes] a) \[CircleTimes] 
          (((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
            OverBar[OverBar[a]]) \[CircleTimes] a) == b \[CircleTimes] 
          OverBar[b]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 242}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(OverBar[a] \[CircleTimes] 
             a) \[CircleTimes] (((OverBar[OverBar[a]] \[CircleTimes] OverBar[
                a]) \[CircleTimes] OverBar[OverBar[a]]) \[CircleTimes] a) == 
           b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 244} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] ((OverBar[a] \[CircleTimes] 
            a) \[CircleTimes] ((OverBar[OverBar[a]] \[CircleTimes] 
             OverBar[a]) \[CircleTimes] OverBar[OverBar[a]])) == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 243}, "Construct" -> 
         {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CircleTimes] 
            ((OverBar[a] \[CircleTimes] a) \[CircleTimes] 
             ((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
              OverBar[OverBar[a]])) == b \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 245} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] 
          (OverBar[OverBar[a]] \[CircleTimes] 
           ((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
            (OverBar[a] \[CircleTimes] a))) == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 244}, 
        "Construct" -> {"SubstitutionLemma", 40}, "Position" -> {2}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (b \[CircleTimes] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CircleTimes] 
            (OverBar[OverBar[a]] \[CircleTimes] 
             ((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
              (OverBar[a] \[CircleTimes] a))) == b \[CircleTimes] 
            OverBar[b]], "Source" -> "cpl"|>|>, {"SubstitutionLemma", 246} -> 
     <|"Statement" -> HoldForm[((OverBar[OverBar[a]] \[CircleTimes] 
            OverBar[a]) \[CircleTimes] (OverBar[a] \[CircleTimes] 
            a)) \[CircleTimes] (OverBar[OverBar[a]] \[CircleTimes] a) == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 245}, "Construct" -> 
         {"SubstitutionLemma", 40}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (b \[CircleTimes] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[((OverBar[OverBar[a]] \[CircleTimes] 
              OverBar[a]) \[CircleTimes] (OverBar[a] \[CircleTimes] 
              a)) \[CircleTimes] (OverBar[OverBar[a]] \[CircleTimes] a) == 
           b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 247} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[a]] \[CircleTimes] 
          (((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
            (OverBar[a] \[CircleTimes] a)) \[CircleTimes] a) == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 246}, "Construct" -> 
         {"SubstitutionLemma", 73}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[OverBar[OverBar[a]] \[CircleTimes] 
            (((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
              (OverBar[a] \[CircleTimes] a)) \[CircleTimes] a) == 
           b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 248} -> 
     <|"Statement" -> HoldForm[
        (((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
            (OverBar[a] \[CircleTimes] a)) \[CircleTimes] a) \[CircleTimes] 
          OverBar[OverBar[a]] == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 247}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
              (OverBar[a] \[CircleTimes] a)) \[CircleTimes] a) \[CircleTimes] 
            OverBar[OverBar[a]] == b \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 249} -> 
     <|"Statement" -> HoldForm[((OverBar[a] \[CircleTimes] a) \[CircleTimes] 
           ((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
            a)) \[CircleTimes] OverBar[OverBar[a]] == b \[CircleTimes] 
          OverBar[b]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 248}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          ((OverBar[a] \[CircleTimes] a) \[CircleTimes] 
             ((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
              a)) \[CircleTimes] OverBar[OverBar[a]] == b \[CircleTimes] 
            OverBar[b]], "Source" -> "cpl"|>|>, {"SubstitutionLemma", 250} -> 
     <|"Statement" -> HoldForm[((OverBar[OverBar[a]] \[CircleTimes] 
            OverBar[a]) \[CircleTimes] a) \[CircleTimes] 
          ((OverBar[a] \[CircleTimes] a) \[CircleTimes] 
           OverBar[OverBar[a]]) == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 249}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[((OverBar[OverBar[a]] \[CircleTimes] 
              OverBar[a]) \[CircleTimes] a) \[CircleTimes] 
            ((OverBar[a] \[CircleTimes] a) \[CircleTimes] 
             OverBar[OverBar[a]]) == b \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 251} -> 
     <|"Statement" -> HoldForm[((OverBar[a] \[CircleTimes] a) \[CircleTimes] 
           OverBar[OverBar[a]]) \[CircleTimes] 
          ((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
           a) == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 250}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          ((OverBar[a] \[CircleTimes] a) \[CircleTimes] 
             OverBar[OverBar[a]]) \[CircleTimes] 
            ((OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) \[CircleTimes] 
             a) == b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 252} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] 
          (((OverBar[a] \[CircleTimes] a) \[CircleTimes] 
            OverBar[OverBar[a]]) \[CircleTimes] 
           (OverBar[OverBar[a]] \[CircleTimes] OverBar[a])) == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 251}, "Construct" -> 
         {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CircleTimes] 
            (((OverBar[a] \[CircleTimes] a) \[CircleTimes] OverBar[OverBar[
                a]]) \[CircleTimes] (OverBar[OverBar[a]] \[CircleTimes] 
              OverBar[a])) == b \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 253} -> 
     <|"Statement" -> HoldForm[(OverBar[OverBar[a]] \[CircleTimes] 
           OverBar[a]) \[CircleTimes] (a \[CircleTimes] 
           ((OverBar[a] \[CircleTimes] a) \[CircleTimes] 
            OverBar[OverBar[a]])) == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 252}, 
        "Construct" -> {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(OverBar[OverBar[a]] \[CircleTimes] 
             OverBar[a]) \[CircleTimes] (a \[CircleTimes] 
             ((OverBar[a] \[CircleTimes] a) \[CircleTimes] OverBar[OverBar[
                a]])) == b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 254} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] 
          (OverBar[OverBar[a]] \[CircleTimes] (a \[CircleTimes] 
            ((OverBar[a] \[CircleTimes] a) \[CircleTimes] 
             OverBar[OverBar[a]]))) == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 253}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] 
            (OverBar[OverBar[a]] \[CircleTimes] (a \[CircleTimes] 
              ((OverBar[a] \[CircleTimes] a) \[CircleTimes] OverBar[
                OverBar[a]]))) == b \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 255} -> 
     <|"Statement" -> HoldForm[(a \[CircleTimes] ((OverBar[a] \[CircleTimes] 
             a) \[CircleTimes] OverBar[OverBar[a]])) \[CircleTimes] 
          (OverBar[a] \[CircleTimes] OverBar[OverBar[a]]) == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 254}, "Construct" -> 
         {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          (a \[CircleTimes] ((OverBar[a] \[CircleTimes] a) \[CircleTimes] 
              OverBar[OverBar[a]])) \[CircleTimes] (OverBar[a] \[CircleTimes] 
             OverBar[OverBar[a]]) == b \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 256} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[a]] \[CircleTimes] 
          ((a \[CircleTimes] ((OverBar[a] \[CircleTimes] a) \[CircleTimes] 
             OverBar[OverBar[a]])) \[CircleTimes] OverBar[a]) == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 255}, "Construct" -> 
         {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[OverBar[OverBar[a]] \[CircleTimes] 
            ((a \[CircleTimes] ((OverBar[a] \[CircleTimes] a) \[CircleTimes] 
               OverBar[OverBar[a]])) \[CircleTimes] OverBar[a]) == 
           b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 257} -> 
     <|"Statement" -> HoldForm[
        ((a \[CircleTimes] ((OverBar[a] \[CircleTimes] a) \[CircleTimes] 
             OverBar[OverBar[a]])) \[CircleTimes] OverBar[a]) \[CircleTimes] 
          OverBar[OverBar[a]] == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 256}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          ((a \[CircleTimes] ((OverBar[a] \[CircleTimes] a) \[CircleTimes] 
               OverBar[OverBar[a]])) \[CircleTimes] OverBar[
              a]) \[CircleTimes] OverBar[OverBar[a]] == b \[CircleTimes] 
            OverBar[b]], "Source" -> "cpl"|>|>, {"SubstitutionLemma", 258} -> 
     <|"Statement" -> HoldForm[(((OverBar[a] \[CircleTimes] a) \[CircleTimes] 
            OverBar[OverBar[a]]) \[CircleTimes] (a \[CircleTimes] 
            OverBar[a])) \[CircleTimes] OverBar[OverBar[a]] == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 257}, "Construct" -> 
         {"SubstitutionLemma", 22}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          (((OverBar[a] \[CircleTimes] a) \[CircleTimes] OverBar[OverBar[
                a]]) \[CircleTimes] (a \[CircleTimes] OverBar[
               a])) \[CircleTimes] OverBar[OverBar[a]] == b \[CircleTimes] 
            OverBar[b]], "Source" -> "cpl"|>|>, {"SubstitutionLemma", 259} -> 
     <|"Statement" -> HoldForm[(a \[CircleTimes] OverBar[a]) \[CircleTimes] 
          (((OverBar[a] \[CircleTimes] a) \[CircleTimes] 
            OverBar[OverBar[a]]) \[CircleTimes] OverBar[OverBar[a]]) == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 258}, "Construct" -> 
         {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          (a \[CircleTimes] OverBar[a]) \[CircleTimes] 
            (((OverBar[a] \[CircleTimes] a) \[CircleTimes] OverBar[OverBar[
                a]]) \[CircleTimes] OverBar[OverBar[a]]) == 
           b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 260} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[a]] \[CircleTimes] 
          ((a \[CircleTimes] OverBar[a]) \[CircleTimes] 
           ((OverBar[a] \[CircleTimes] a) \[CircleTimes] 
            OverBar[OverBar[a]])) == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 259}, 
        "Construct" -> {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[OverBar[OverBar[a]] \[CircleTimes] 
            ((a \[CircleTimes] OverBar[a]) \[CircleTimes] 
             ((OverBar[a] \[CircleTimes] a) \[CircleTimes] OverBar[OverBar[
                a]])) == b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 261} -> 
     <|"Statement" -> HoldForm[((OverBar[a] \[CircleTimes] a) \[CircleTimes] 
           OverBar[OverBar[a]]) \[CircleTimes] 
          (OverBar[OverBar[a]] \[CircleTimes] (a \[CircleTimes] 
            OverBar[a])) == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 260}, 
        "Construct" -> {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          ((OverBar[a] \[CircleTimes] a) \[CircleTimes] 
             OverBar[OverBar[a]]) \[CircleTimes] 
            (OverBar[OverBar[a]] \[CircleTimes] (a \[CircleTimes] 
              OverBar[a])) == b \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 262} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[a]] \[CircleTimes] 
          ((OverBar[a] \[CircleTimes] a) \[CircleTimes] 
           (OverBar[OverBar[a]] \[CircleTimes] (a \[CircleTimes] 
             OverBar[a]))) == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 261}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[OverBar[OverBar[a]] \[CircleTimes] 
            ((OverBar[a] \[CircleTimes] a) \[CircleTimes] 
             (OverBar[OverBar[a]] \[CircleTimes] (a \[CircleTimes] OverBar[
                a]))) == b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 263} -> 
     <|"Statement" -> HoldForm[((OverBar[a] \[CircleTimes] a) \[CircleTimes] 
           (OverBar[OverBar[a]] \[CircleTimes] (a \[CircleTimes] 
             OverBar[a]))) \[CircleTimes] OverBar[OverBar[a]] == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 262}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {}, "Rule" -> (a_) \[CircleTimes] (b_) -> 
          b \[CircleTimes] a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[((OverBar[a] \[CircleTimes] a) \[CircleTimes] 
             (OverBar[OverBar[a]] \[CircleTimes] (a \[CircleTimes] OverBar[
                a]))) \[CircleTimes] OverBar[OverBar[a]] == 
           b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 264} -> 
     <|"Statement" -> HoldForm[((a \[CircleTimes] OverBar[a]) \[CircleTimes] 
           (OverBar[OverBar[a]] \[CircleTimes] (OverBar[a] \[CircleTimes] 
             a))) \[CircleTimes] OverBar[OverBar[a]] == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 263}, "Construct" -> 
         {"SubstitutionLemma", 40}, "Position" -> {1}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (b \[CircleTimes] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          ((a \[CircleTimes] OverBar[a]) \[CircleTimes] 
             (OverBar[OverBar[a]] \[CircleTimes] (OverBar[a] \[CircleTimes] 
               a))) \[CircleTimes] OverBar[OverBar[a]] == b \[CircleTimes] 
            OverBar[b]], "Source" -> "cpl"|>|>, {"SubstitutionLemma", 265} -> 
     <|"Statement" -> HoldForm[(OverBar[OverBar[a]] \[CircleTimes] 
           (OverBar[a] \[CircleTimes] a)) \[CircleTimes] 
          ((a \[CircleTimes] OverBar[a]) \[CircleTimes] 
           OverBar[OverBar[a]]) == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 264}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(OverBar[OverBar[a]] \[CircleTimes] 
             (OverBar[a] \[CircleTimes] a)) \[CircleTimes] 
            ((a \[CircleTimes] OverBar[a]) \[CircleTimes] 
             OverBar[OverBar[a]]) == b \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 266} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[a]] \[CircleTimes] 
          ((OverBar[OverBar[a]] \[CircleTimes] (OverBar[a] \[CircleTimes] 
             a)) \[CircleTimes] (a \[CircleTimes] OverBar[a])) == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 265}, "Construct" -> 
         {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[OverBar[OverBar[a]] \[CircleTimes] 
            ((OverBar[OverBar[a]] \[CircleTimes] (OverBar[a] \[CircleTimes] 
               a)) \[CircleTimes] (a \[CircleTimes] OverBar[a])) == 
           b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 267} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[a]] \[CircleTimes] 
          (OverBar[a] \[CircleTimes] (a \[CircleTimes] 
            (OverBar[OverBar[a]] \[CircleTimes] (OverBar[a] \[CircleTimes] 
              a)))) == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 266}, 
        "Construct" -> {"SubstitutionLemma", 40}, "Position" -> {2}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (b \[CircleTimes] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[OverBar[OverBar[a]] \[CircleTimes] 
            (OverBar[a] \[CircleTimes] (a \[CircleTimes] (OverBar[
                OverBar[a]] \[CircleTimes] (OverBar[a] \[CircleTimes] 
                a)))) == b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 268} -> 
     <|"Statement" -> HoldForm[
        (a \[CircleTimes] (OverBar[OverBar[a]] \[CircleTimes] 
            (OverBar[a] \[CircleTimes] a))) \[CircleTimes] 
          (OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 267}, "Construct" -> 
         {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          (a \[CircleTimes] (OverBar[OverBar[a]] \[CircleTimes] 
              (OverBar[a] \[CircleTimes] a))) \[CircleTimes] 
            (OverBar[OverBar[a]] \[CircleTimes] OverBar[a]) == 
           b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 269} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] 
          ((a \[CircleTimes] (OverBar[OverBar[a]] \[CircleTimes] 
             (OverBar[a] \[CircleTimes] a))) \[CircleTimes] 
           OverBar[OverBar[a]]) == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 268}, 
        "Construct" -> {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] 
            ((a \[CircleTimes] (OverBar[OverBar[a]] \[CircleTimes] (
                OverBar[a] \[CircleTimes] a))) \[CircleTimes] 
             OverBar[OverBar[a]]) == b \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 270} -> 
     <|"Statement" -> HoldForm[
        ((a \[CircleTimes] (OverBar[OverBar[a]] \[CircleTimes] 
             (OverBar[a] \[CircleTimes] a))) \[CircleTimes] 
           OverBar[OverBar[a]]) \[CircleTimes] OverBar[a] == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 269}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {}, "Rule" -> (a_) \[CircleTimes] (b_) -> 
          b \[CircleTimes] a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[((a \[CircleTimes] (OverBar[OverBar[a]] \[CircleTimes] (
                OverBar[a] \[CircleTimes] a))) \[CircleTimes] 
             OverBar[OverBar[a]]) \[CircleTimes] OverBar[a] == 
           b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 271} -> 
     <|"Statement" -> HoldForm[((OverBar[OverBar[a]] \[CircleTimes] 
            (OverBar[a] \[CircleTimes] a)) \[CircleTimes] (a \[CircleTimes] 
            OverBar[OverBar[a]])) \[CircleTimes] OverBar[a] == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 270}, "Construct" -> 
         {"SubstitutionLemma", 22}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[((OverBar[OverBar[a]] \[CircleTimes] 
              (OverBar[a] \[CircleTimes] a)) \[CircleTimes] (a \[CircleTimes] 
              OverBar[OverBar[a]])) \[CircleTimes] OverBar[a] == 
           b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 272} -> 
     <|"Statement" -> HoldForm[
        (a \[CircleTimes] OverBar[OverBar[a]]) \[CircleTimes] 
          ((OverBar[OverBar[a]] \[CircleTimes] (OverBar[a] \[CircleTimes] 
             a)) \[CircleTimes] OverBar[a]) == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 271}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          (a \[CircleTimes] OverBar[OverBar[a]]) \[CircleTimes] 
            ((OverBar[OverBar[a]] \[CircleTimes] (OverBar[a] \[CircleTimes] 
               a)) \[CircleTimes] OverBar[a]) == b \[CircleTimes] 
            OverBar[b]], "Source" -> "cpl"|>|>, {"SubstitutionLemma", 273} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] 
          ((a \[CircleTimes] OverBar[OverBar[a]]) \[CircleTimes] 
           (OverBar[OverBar[a]] \[CircleTimes] (OverBar[a] \[CircleTimes] 
             a))) == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 272}, 
        "Construct" -> {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] 
            ((a \[CircleTimes] OverBar[OverBar[a]]) \[CircleTimes] 
             (OverBar[OverBar[a]] \[CircleTimes] (OverBar[a] \[CircleTimes] 
               a))) == b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 274} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] 
          ((OverBar[a] \[CircleTimes] a) \[CircleTimes] 
           (OverBar[OverBar[a]] \[CircleTimes] (a \[CircleTimes] 
             OverBar[OverBar[a]]))) == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 273}, 
        "Construct" -> {"SubstitutionLemma", 40}, "Position" -> {2}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (b \[CircleTimes] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] 
            ((OverBar[a] \[CircleTimes] a) \[CircleTimes] 
             (OverBar[OverBar[a]] \[CircleTimes] (a \[CircleTimes] OverBar[
                OverBar[a]]))) == b \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 275} -> 
     <|"Statement" -> HoldForm[(OverBar[OverBar[a]] \[CircleTimes] 
           (a \[CircleTimes] OverBar[OverBar[a]])) \[CircleTimes] 
          ((OverBar[a] \[CircleTimes] a) \[CircleTimes] OverBar[a]) == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 274}, "Construct" -> 
         {"SubstitutionLemma", 40}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (b \[CircleTimes] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(OverBar[OverBar[a]] \[CircleTimes] 
             (a \[CircleTimes] OverBar[OverBar[a]])) \[CircleTimes] 
            ((OverBar[a] \[CircleTimes] a) \[CircleTimes] OverBar[a]) == 
           b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 276} -> 
     <|"Statement" -> HoldForm[(OverBar[a] \[CircleTimes] a) \[CircleTimes] 
          ((OverBar[OverBar[a]] \[CircleTimes] (a \[CircleTimes] 
             OverBar[OverBar[a]])) \[CircleTimes] OverBar[a]) == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 275}, "Construct" -> 
         {"SubstitutionLemma", 73}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(OverBar[a] \[CircleTimes] 
             a) \[CircleTimes] ((OverBar[OverBar[a]] \[CircleTimes] 
              (a \[CircleTimes] OverBar[OverBar[a]])) \[CircleTimes] 
             OverBar[a]) == b \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 277} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (OverBar[a] \[CircleTimes] 
           ((OverBar[OverBar[a]] \[CircleTimes] (a \[CircleTimes] 
              OverBar[OverBar[a]])) \[CircleTimes] OverBar[a])) == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 276}, "Construct" -> 
         {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CircleTimes] 
            (OverBar[a] \[CircleTimes] ((OverBar[OverBar[a]] \[CircleTimes] (
                a \[CircleTimes] OverBar[OverBar[a]])) \[CircleTimes] 
              OverBar[a])) == b \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 278} -> 
     <|"Statement" -> HoldForm[((OverBar[OverBar[a]] \[CircleTimes] 
            (a \[CircleTimes] OverBar[OverBar[a]])) \[CircleTimes] 
           OverBar[a]) \[CircleTimes] (a \[CircleTimes] OverBar[a]) == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 277}, "Construct" -> 
         {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[((OverBar[OverBar[a]] \[CircleTimes] 
              (a \[CircleTimes] OverBar[OverBar[a]])) \[CircleTimes] 
             OverBar[a]) \[CircleTimes] (a \[CircleTimes] OverBar[a]) == 
           b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 279} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] 
          (((OverBar[OverBar[a]] \[CircleTimes] (a \[CircleTimes] 
              OverBar[OverBar[a]])) \[CircleTimes] OverBar[a]) \[CircleTimes] 
           a) == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 278}, 
        "Construct" -> {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] 
            (((OverBar[OverBar[a]] \[CircleTimes] (a \[CircleTimes] 
                OverBar[OverBar[a]])) \[CircleTimes] OverBar[
               a]) \[CircleTimes] a) == b \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 280} -> 
     <|"Statement" -> HoldForm[
        (((OverBar[OverBar[a]] \[CircleTimes] (a \[CircleTimes] 
              OverBar[OverBar[a]])) \[CircleTimes] OverBar[a]) \[CircleTimes] 
           a) \[CircleTimes] OverBar[a] == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 279}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (((OverBar[OverBar[a]] \[CircleTimes] (a \[CircleTimes] 
                OverBar[OverBar[a]])) \[CircleTimes] OverBar[
               a]) \[CircleTimes] a) \[CircleTimes] OverBar[a] == 
           b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 281} -> 
     <|"Statement" -> HoldForm[(OverBar[a] \[CircleTimes] 
           ((OverBar[OverBar[a]] \[CircleTimes] (a \[CircleTimes] 
              OverBar[OverBar[a]])) \[CircleTimes] a)) \[CircleTimes] 
          OverBar[a] == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 280}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(OverBar[a] \[CircleTimes] 
             ((OverBar[OverBar[a]] \[CircleTimes] (a \[CircleTimes] 
                OverBar[OverBar[a]])) \[CircleTimes] a)) \[CircleTimes] 
            OverBar[a] == b \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 282} -> 
     <|"Statement" -> HoldForm[((OverBar[OverBar[a]] \[CircleTimes] 
            (a \[CircleTimes] OverBar[OverBar[a]])) \[CircleTimes] 
           a) \[CircleTimes] (OverBar[a] \[CircleTimes] OverBar[a]) == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 281}, "Construct" -> 
         {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[((OverBar[OverBar[a]] \[CircleTimes] 
              (a \[CircleTimes] OverBar[OverBar[a]])) \[CircleTimes] 
             a) \[CircleTimes] (OverBar[a] \[CircleTimes] OverBar[a]) == 
           b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 283} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] 
          (((OverBar[OverBar[a]] \[CircleTimes] (a \[CircleTimes] 
              OverBar[OverBar[a]])) \[CircleTimes] a) \[CircleTimes] 
           OverBar[a]) == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 282}, 
        "Construct" -> {"SubstitutionLemma", 73}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CircleTimes] 
            (((OverBar[OverBar[a]] \[CircleTimes] (a \[CircleTimes] 
                OverBar[OverBar[a]])) \[CircleTimes] a) \[CircleTimes] 
             OverBar[a]) == b \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 284} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] 
          ((OverBar[OverBar[a]] \[CircleTimes] (a \[CircleTimes] 
             OverBar[OverBar[a]])) \[CircleTimes] a) == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 283}, "Construct" -> 
         {"SubstitutionLemma", 38}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (a_)) -> 
          a \[CircleTimes] b, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[OverBar[a] \[CircleTimes] 
            ((OverBar[OverBar[a]] \[CircleTimes] (a \[CircleTimes] OverBar[
                OverBar[a]])) \[CircleTimes] a) == b \[CircleTimes] 
            OverBar[b]], "Source" -> "cpl"|>|>, {"SubstitutionLemma", 285} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] 
          ((OverBar[OverBar[a]] \[CircleTimes] (a \[CircleTimes] 
             OverBar[OverBar[a]])) \[CircleTimes] OverBar[a]) == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 284}, "Construct" -> 
         {"SubstitutionLemma", 40}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (b \[CircleTimes] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CircleTimes] 
            ((OverBar[OverBar[a]] \[CircleTimes] (a \[CircleTimes] OverBar[
                OverBar[a]])) \[CircleTimes] OverBar[a]) == 
           b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 286} -> 
     <|"Statement" -> HoldForm[(OverBar[OverBar[a]] \[CircleTimes] 
           (a \[CircleTimes] OverBar[OverBar[a]])) \[CircleTimes] 
          (a \[CircleTimes] OverBar[a]) == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 285}, 
        "Construct" -> {"SubstitutionLemma", 73}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(OverBar[OverBar[a]] \[CircleTimes] 
             (a \[CircleTimes] OverBar[OverBar[a]])) \[CircleTimes] 
            (a \[CircleTimes] OverBar[a]) == b \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 287} -> 
     <|"Statement" -> HoldForm[
        (a \[CircleTimes] OverBar[OverBar[a]]) \[CircleTimes] 
          (OverBar[OverBar[a]] \[CircleTimes] (a \[CircleTimes] 
            OverBar[a])) == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 286}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          (a \[CircleTimes] OverBar[OverBar[a]]) \[CircleTimes] 
            (OverBar[OverBar[a]] \[CircleTimes] (a \[CircleTimes] 
              OverBar[a])) == b \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 288} -> 
     <|"Statement" -> HoldForm[(a \[CircleTimes] OverBar[a]) \[CircleTimes] 
          ((a \[CircleTimes] OverBar[OverBar[a]]) \[CircleTimes] 
           OverBar[OverBar[a]]) == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 287}, 
        "Construct" -> {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          (a \[CircleTimes] OverBar[a]) \[CircleTimes] 
            ((a \[CircleTimes] OverBar[OverBar[a]]) \[CircleTimes] 
             OverBar[OverBar[a]]) == b \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 289} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[a]] \[CircleTimes] 
          ((a \[CircleTimes] OverBar[a]) \[CircleTimes] (a \[CircleTimes] 
            OverBar[OverBar[a]])) == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 288}, 
        "Construct" -> {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[OverBar[OverBar[a]] \[CircleTimes] 
            ((a \[CircleTimes] OverBar[a]) \[CircleTimes] (a \[CircleTimes] 
              OverBar[OverBar[a]])) == b \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 290} -> 
     <|"Statement" -> HoldForm[((a \[CircleTimes] OverBar[a]) \[CircleTimes] 
           (a \[CircleTimes] OverBar[OverBar[a]])) \[CircleTimes] 
          OverBar[OverBar[a]] == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 289}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          ((a \[CircleTimes] OverBar[a]) \[CircleTimes] (a \[CircleTimes] 
              OverBar[OverBar[a]])) \[CircleTimes] OverBar[OverBar[a]] == 
           b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 291} -> 
     <|"Statement" -> HoldForm[(OverBar[OverBar[a]] \[CircleTimes] 
           (a \[CircleTimes] (a \[CircleTimes] OverBar[a]))) \[CircleTimes] 
          OverBar[OverBar[a]] == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 290}, 
        "Construct" -> {"SubstitutionLemma", 40}, "Position" -> {1}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (b \[CircleTimes] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(OverBar[OverBar[a]] \[CircleTimes] 
             (a \[CircleTimes] (a \[CircleTimes] OverBar[a]))) \[CircleTimes] 
            OverBar[OverBar[a]] == b \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 292} -> 
     <|"Statement" -> HoldForm[(a \[CircleTimes] (a \[CircleTimes] 
            OverBar[a])) \[CircleTimes] (OverBar[OverBar[a]] \[CircleTimes] 
           OverBar[OverBar[a]]) == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 291}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(a \[CircleTimes] (a \[CircleTimes] 
              OverBar[a])) \[CircleTimes] (OverBar[OverBar[a]] \[CircleTimes] 
             OverBar[OverBar[a]]) == b \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 293} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[a]] \[CircleTimes] 
          ((a \[CircleTimes] (a \[CircleTimes] OverBar[a])) \[CircleTimes] 
           OverBar[OverBar[a]]) == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 292}, 
        "Construct" -> {"SubstitutionLemma", 73}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[OverBar[OverBar[a]] \[CircleTimes] 
            ((a \[CircleTimes] (a \[CircleTimes] OverBar[a])) \[CircleTimes] 
             OverBar[OverBar[a]]) == b \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 294} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[a]] \[CircleTimes] 
          ((OverBar[a] \[CircleTimes] (a \[CircleTimes] a)) \[CircleTimes] 
           OverBar[OverBar[a]]) == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 293}, 
        "Construct" -> {"SubstitutionLemma", 48}, "Position" -> {2, 1}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[OverBar[OverBar[a]] \[CircleTimes] 
            ((OverBar[a] \[CircleTimes] (a \[CircleTimes] a)) \[CircleTimes] 
             OverBar[OverBar[a]]) == b \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 295} -> 
     <|"Statement" -> HoldForm[((OverBar[a] \[CircleTimes] (a \[CircleTimes] 
             a)) \[CircleTimes] OverBar[OverBar[a]]) \[CircleTimes] 
          OverBar[OverBar[a]] == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 294}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          ((OverBar[a] \[CircleTimes] (a \[CircleTimes] a)) \[CircleTimes] 
             OverBar[OverBar[a]]) \[CircleTimes] OverBar[OverBar[a]] == 
           b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 296} -> 
     <|"Statement" -> HoldForm[((a \[CircleTimes] a) \[CircleTimes] 
           (OverBar[a] \[CircleTimes] OverBar[OverBar[a]])) \[CircleTimes] 
          OverBar[OverBar[a]] == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 295}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[((a \[CircleTimes] a) \[CircleTimes] 
             (OverBar[a] \[CircleTimes] OverBar[OverBar[a]])) \[CircleTimes] 
            OverBar[OverBar[a]] == b \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 297} -> 
     <|"Statement" -> HoldForm[(OverBar[a] \[CircleTimes] 
           OverBar[OverBar[a]]) \[CircleTimes] ((a \[CircleTimes] 
            a) \[CircleTimes] OverBar[OverBar[a]]) == b \[CircleTimes] 
          OverBar[b]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 296}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(OverBar[a] \[CircleTimes] 
             OverBar[OverBar[a]]) \[CircleTimes] ((a \[CircleTimes] 
              a) \[CircleTimes] OverBar[OverBar[a]]) == b \[CircleTimes] 
            OverBar[b]], "Source" -> "cpl"|>|>, {"SubstitutionLemma", 298} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[a]] \[CircleTimes] 
          ((OverBar[a] \[CircleTimes] OverBar[OverBar[a]]) \[CircleTimes] 
           (a \[CircleTimes] a)) == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 297}, 
        "Construct" -> {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[OverBar[OverBar[a]] \[CircleTimes] 
            ((OverBar[a] \[CircleTimes] OverBar[OverBar[a]]) \[CircleTimes] 
             (a \[CircleTimes] a)) == b \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 299} -> 
     <|"Statement" -> HoldForm[(a \[CircleTimes] a) \[CircleTimes] 
          (OverBar[OverBar[a]] \[CircleTimes] (OverBar[a] \[CircleTimes] 
            OverBar[OverBar[a]])) == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 298}, 
        "Construct" -> {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(a \[CircleTimes] a) \[CircleTimes] 
            (OverBar[OverBar[a]] \[CircleTimes] (OverBar[a] \[CircleTimes] 
              OverBar[OverBar[a]])) == b \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 300} -> 
     <|"Statement" -> HoldForm[(OverBar[OverBar[a]] \[CircleTimes] 
           (OverBar[a] \[CircleTimes] OverBar[OverBar[a]])) \[CircleTimes] 
          (a \[CircleTimes] a) == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 299}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (OverBar[OverBar[a]] \[CircleTimes] (OverBar[a] \[CircleTimes] 
              OverBar[OverBar[a]])) \[CircleTimes] (a \[CircleTimes] a) == 
           b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 301} -> 
     <|"Statement" -> HoldForm[(OverBar[OverBar[a]] \[CircleTimes] 
           (b \[CircleTimes] OverBar[b])) \[CircleTimes] (a \[CircleTimes] 
           a) == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 300}, 
        "Construct" -> {"CriticalPairLemma", 38}, "Position" -> {1, 2}, 
        "Rule" -> (a_) \[CircleTimes] OverBar[a_] -> b \[CircleTimes] 
           OverBar[b], "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[(OverBar[OverBar[a]] \[CircleTimes] (b \[CircleTimes] 
              OverBar[b])) \[CircleTimes] (a \[CircleTimes] a) == 
           b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 302} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (a \[CircleTimes] 
           (OverBar[OverBar[a]] \[CircleTimes] (b \[CircleTimes] 
             OverBar[b]))) == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 301}, 
        "Construct" -> {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (c_) \[CircleTimes] ((a_) \[CircleTimes] (b_)) -> 
          a \[CircleTimes] (b \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CircleTimes] (a \[CircleTimes] 
             (OverBar[OverBar[a]] \[CircleTimes] (b \[CircleTimes] OverBar[
                b]))) == b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 303} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] 
          (OverBar[OverBar[a]] \[CircleTimes] (b \[CircleTimes] 
            OverBar[b])) == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 302}, 
        "Construct" -> {"CriticalPairLemma", 39}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((a_) \[CircleTimes] (b_)) -> 
          a \[CircleTimes] b, "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[a \[CircleTimes] (OverBar[OverBar[a]] \[CircleTimes] 
             (b \[CircleTimes] OverBar[b])) == b \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 304} -> 
     <|"Statement" -> HoldForm[(b \[CircleTimes] OverBar[b]) \[CircleTimes] 
          (OverBar[OverBar[a]] \[CircleTimes] a) == b \[CircleTimes] 
          OverBar[b]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 303}, 
        "Construct" -> {"SubstitutionLemma", 40}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (b \[CircleTimes] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          (b \[CircleTimes] OverBar[b]) \[CircleTimes] 
            (OverBar[OverBar[a]] \[CircleTimes] a) == b \[CircleTimes] 
            OverBar[b]], "Source" -> "cpl"|>|>, {"SubstitutionLemma", 305} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[a]] \[CircleTimes] 
          ((b \[CircleTimes] OverBar[b]) \[CircleTimes] a) == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 304}, "Construct" -> 
         {"SubstitutionLemma", 73}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[OverBar[OverBar[a]] \[CircleTimes] 
            ((b \[CircleTimes] OverBar[b]) \[CircleTimes] a) == 
           b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 306} -> 
     <|"Statement" -> HoldForm[((b \[CircleTimes] OverBar[b]) \[CircleTimes] 
           a) \[CircleTimes] OverBar[OverBar[a]] == b \[CircleTimes] 
          OverBar[b]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 305}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          ((b \[CircleTimes] OverBar[b]) \[CircleTimes] a) \[CircleTimes] 
            OverBar[OverBar[a]] == b \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 307} -> 
     <|"Statement" -> HoldForm[((b \[CircleTimes] OverBar[b]) \[CircleTimes] 
           a) \[CircleTimes] a == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 306}, 
        "Construct" -> {"SubstitutionLemma", 8}, "Position" -> {2}, 
        "Rule" -> OverBar[OverBar[a_]] -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          ((b \[CircleTimes] OverBar[b]) \[CircleTimes] a) \[CircleTimes] 
            a == b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 308} -> 
     <|"Statement" -> HoldForm[(OverBar[b] \[CircleTimes] (b \[CircleTimes] 
            a)) \[CircleTimes] a == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 307}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(OverBar[b] \[CircleTimes] 
             (b \[CircleTimes] a)) \[CircleTimes] a == b \[CircleTimes] 
            OverBar[b]], "Source" -> "cpl"|>|>, {"SubstitutionLemma", 309} -> 
     <|"Statement" -> HoldForm[(b \[CircleTimes] a) \[CircleTimes] 
          (OverBar[b] \[CircleTimes] a) == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 308}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(b \[CircleTimes] a) \[CircleTimes] 
            (OverBar[b] \[CircleTimes] a) == b \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 310} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] 
          ((b \[CircleTimes] a) \[CircleTimes] OverBar[b]) == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 309}, "Construct" -> 
         {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CircleTimes] 
            ((b \[CircleTimes] a) \[CircleTimes] OverBar[b]) == 
           b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 311} -> 
     <|"Statement" -> HoldForm[OverBar[b] \[CircleTimes] (a \[CircleTimes] 
           (b \[CircleTimes] a)) == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 310}, 
        "Construct" -> {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[OverBar[b] \[CircleTimes] 
            (a \[CircleTimes] (b \[CircleTimes] a)) == b \[CircleTimes] 
            OverBar[b]], "Source" -> "cpl"|>|>, {"SubstitutionLemma", 312} -> 
     <|"Statement" -> HoldForm[(a \[CircleTimes] (b \[CircleTimes] 
            a)) \[CircleTimes] OverBar[b] == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 311}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (a \[CircleTimes] (b \[CircleTimes] a)) \[CircleTimes] 
            OverBar[b] == b \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 313} -> 
     <|"Statement" -> HoldForm[(b \[CircleTimes] (a \[CircleTimes] 
            a)) \[CircleTimes] OverBar[b] == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 312}, 
        "Construct" -> {"SubstitutionLemma", 73}, "Position" -> {1}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(b \[CircleTimes] (a \[CircleTimes] 
              a)) \[CircleTimes] OverBar[b] == b \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 314} -> 
     <|"Statement" -> HoldForm[(a \[CircleTimes] a) \[CircleTimes] 
          (b \[CircleTimes] OverBar[b]) == b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 313}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(a \[CircleTimes] a) \[CircleTimes] 
            (b \[CircleTimes] OverBar[b]) == b \[CircleTimes] OverBar[b]], 
        "Source" -> "cpl"|>|>, {"Conclusion", 1} -> 
     <|"Statement" -> HoldForm[b \[CircleTimes] OverBar[b] == 
         b \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 314}, "Construct" -> 
         {"CriticalPairLemma", 41}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] OverBar[b_]) -> 
          b \[CircleTimes] OverBar[b], "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[b \[CircleTimes] OverBar[b] == 
           b \[CircleTimes] OverBar[b]], "Source" -> "cpl"|>|>}|>]
