ProofObject["EquationalLogic", Inactive[Equal][
  (a \[CircleTimes] b) \[CircleTimes] c, a \[CircleTimes] 
   (b \[CircleTimes] c)], {Inactive[Equal][(a_) \[CircleTimes] (b_), 
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
     <|"Statement" -> HoldForm[(a \[CircleTimes] b) \[CircleTimes] c == 
         a \[CircleTimes] (b \[CircleTimes] c)], "Proof" -> <||>|>, 
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
    {"CriticalPairLemma", 3} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CirclePlus] a) == 
         a \[CircleTimes] b \[CirclePlus] a], 
      "Proof" -> <|"Construct" -> {"Axiom", 2}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CircleTimes] (b_) \[CirclePlus] (a_) \[CircleTimes] 
            (c_) -> a \[CircleTimes] (b \[CirclePlus] c), "Side" -> 1, 
        "Subpattern" -> (a_) \[CircleTimes] (c_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 2}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CircleTimes] (a_) -> a, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CirclePlus] a) == 
         a \[CirclePlus] a \[CircleTimes] b], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 3}, 
        "Construct" -> {"Axiom", 5}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CircleTimes] (b \[CirclePlus] a) == a \[CirclePlus] 
            a \[CircleTimes] b], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 4} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (a \[CirclePlus] b) == 
         a \[CirclePlus] a \[CircleTimes] b], 
      "Proof" -> <|"Construct" -> {"Axiom", 2}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CircleTimes] (b_) \[CirclePlus] (a_) \[CircleTimes] 
            (c_) -> a \[CircleTimes] (b \[CirclePlus] c), "Side" -> 1, 
        "Subpattern" -> (a_) \[CircleTimes] (b_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 2}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CircleTimes] (a_) -> a, "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CirclePlus] a) == 
         a \[CircleTimes] (a \[CirclePlus] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 1}, 
        "Construct" -> {"CriticalPairLemma", 4}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] (a_) \[CircleTimes] (b_) -> 
          a \[CircleTimes] (a \[CirclePlus] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CircleTimes] (b \[CirclePlus] 
             a) == a \[CircleTimes] (a \[CirclePlus] b)], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 5} -> 
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
     <|"Statement" -> HoldForm[a \[CirclePlus] (b \[CirclePlus] 
           OverBar[OverBar[a]]) == a \[CirclePlus] OverBar[a] \[CircleTimes] 
           b], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 6}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CirclePlus] 
           OverBar[a_] \[CircleTimes] (b_) -> a \[CirclePlus] b, "Side" -> 1, 
        "Subpattern" -> OverBar[a_] \[CircleTimes] (b_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 1}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] ((b_) \[CirclePlus] OverBar[a_]) -> 
          a \[CircleTimes] b, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 3} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] (b \[CirclePlus] 
           OverBar[OverBar[a]]) == a \[CirclePlus] b], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 7}, 
        "Construct" -> {"CriticalPairLemma", 6}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] OverBar[a_] \[CircleTimes] (b_) -> 
          a \[CirclePlus] b, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[a \[CirclePlus] (b \[CirclePlus] OverBar[OverBar[a]]) == 
           a \[CirclePlus] b], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 8} -> 
     <|"Statement" -> HoldForm[b == a \[CircleTimes] OverBar[a] \[CirclePlus] 
          b], "Proof" -> <|"Construct" -> {"Axiom", 3}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CirclePlus] (b_) \[CircleTimes] OverBar[b_] -> a, 
        "Side" -> 1, "Subpattern" -> {}, "MatchingConstruct" -> {"Axiom", 5}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 9} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (OverBar[a] \[CirclePlus] 
           b) == a \[CircleTimes] b], "Proof" -> 
       <|"Construct" -> {"Axiom", 2}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CircleTimes] (b_) \[CirclePlus] (a_) \[CircleTimes] 
            (c_) -> a \[CircleTimes] (b \[CirclePlus] c), "Side" -> 1, 
        "Subpattern" -> {}, "MatchingConstruct" -> {"CriticalPairLemma", 8}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CircleTimes] OverBar[a_] \[CirclePlus] (b_) -> b, 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"CriticalPairLemma", 10} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] OverBar[OverBar[a]] == a], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 9}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CircleTimes] 
           (OverBar[a_] \[CirclePlus] (b_)) -> a \[CircleTimes] b, 
        "Side" -> 1, "Subpattern" -> {}, "MatchingConstruct" -> {"Axiom", 4}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] ((b_) \[CirclePlus] OverBar[b_]) -> a, 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"CriticalPairLemma", 11} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] OverBar[
           OverBar[OverBar[a]]] == a \[CirclePlus] OverBar[a]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 6}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CirclePlus] 
           OverBar[a_] \[CircleTimes] (b_) -> a \[CirclePlus] b, "Side" -> 1, 
        "Subpattern" -> OverBar[a_] \[CircleTimes] (b_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 10}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] OverBar[OverBar[a_]] -> a, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 12} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[a]] \[CircleTimes] a == 
         OverBar[OverBar[a]] \[CircleTimes] (a \[CirclePlus] OverBar[a])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 1}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] 
            OverBar[a_]) -> a \[CircleTimes] b, "Side" -> 1, 
        "Subpattern" -> (b_) \[CirclePlus] OverBar[a_], 
        "MatchingConstruct" -> {"CriticalPairLemma", 11}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CirclePlus] OverBar[OverBar[OverBar[a_]]] -> 
          a \[CirclePlus] OverBar[a], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 4} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[a]] \[CircleTimes] a == 
         OverBar[OverBar[a]]], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 12}, "Construct" -> {"Axiom", 4}, 
        "Position" -> {}, "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] 
            OverBar[b_]) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[OverBar[OverBar[a]] \[CircleTimes] a == 
           OverBar[OverBar[a]]], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 5} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] OverBar[OverBar[a]] == 
         OverBar[OverBar[a]]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 4}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {}, "Rule" -> (a_) \[CircleTimes] (b_) -> 
          b \[CircleTimes] a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[a \[CircleTimes] OverBar[OverBar[a]] == 
           OverBar[OverBar[a]]], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 6} -> 
     <|"Statement" -> HoldForm[a == OverBar[OverBar[a]]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 5}, 
        "Construct" -> {"CriticalPairLemma", 10}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] OverBar[OverBar[a_]] -> a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a == OverBar[OverBar[a]]], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 7} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] (b \[CirclePlus] a) == 
         a \[CirclePlus] b], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 3}, "Construct" -> 
         {"SubstitutionLemma", 6}, "Position" -> {2, 2}, 
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
         {"SubstitutionLemma", 7}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CirclePlus] ((b_) \[CirclePlus] (a_)) -> 
          a \[CirclePlus] b, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 8} -> 
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
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 8}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CirclePlus] (b_) \[CircleTimes] 
            ((c_) \[CirclePlus] (a_)) -> a \[CirclePlus] b \[CircleTimes] c, 
        "Side" -> 1, "Subpattern" -> (b_) \[CircleTimes] ((c_) \[CirclePlus] 
           (a_)), "MatchingConstruct" -> {"CriticalPairLemma", 9}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] (OverBar[a_] \[CirclePlus] (b_)) -> 
          a \[CircleTimes] b, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 15} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (a \[CirclePlus] b) == 
         a \[CirclePlus] b \[CircleTimes] a], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 4}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CirclePlus] (a_) \[CircleTimes] 
            (b_) -> a \[CircleTimes] (a \[CirclePlus] b), "Side" -> 1, 
        "Subpattern" -> (a_) \[CircleTimes] (b_), "MatchingConstruct" -> 
         {"Axiom", 1}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 9} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] b \[CircleTimes] OverBar[b] == 
         a \[CircleTimes] (a \[CirclePlus] b)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 14}, 
        "Construct" -> {"CriticalPairLemma", 15}, "Position" -> {}, 
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
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CirclePlus] a) == a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 2}, 
        "Construct" -> {"SubstitutionLemma", 10}, "Position" -> {}, 
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
         {"SubstitutionLemma", 11}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] (a_)) -> a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 12} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CirclePlus] 
           (c \[CirclePlus] a)) == a \[CirclePlus] a \[CircleTimes] b], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 16}, 
        "Construct" -> {"Axiom", 5}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CircleTimes] (b \[CirclePlus] (c \[CirclePlus] a)) == 
           a \[CirclePlus] a \[CircleTimes] b], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 13} -> 
     <|"Statement" -> HoldForm[a == a \[CirclePlus] a \[CircleTimes] b], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 4}, 
        "Construct" -> {"SubstitutionLemma", 10}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((a_) \[CirclePlus] (b_)) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a == a \[CirclePlus] a \[CircleTimes] b], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 14} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CirclePlus] 
           (c \[CirclePlus] a)) == a], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 12}, "Construct" -> 
         {"SubstitutionLemma", 13}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] (a_) \[CircleTimes] (b_) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CircleTimes] (b \[CirclePlus] (c \[CirclePlus] a)) == a], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 17} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] b == 
         (a \[CircleTimes] b) \[CircleTimes] (c \[CirclePlus] a)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 14}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] 
            ((c_) \[CirclePlus] (a_))) -> a, "Side" -> 1, 
        "Subpattern" -> (c_) \[CirclePlus] (a_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 13}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CirclePlus] (a_) \[CircleTimes] (b_) -> a, 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"CriticalPairLemma", 18} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] 
          (a \[CircleTimes] b) \[CircleTimes] c == a \[CirclePlus] 
          a \[CircleTimes] b], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 8}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CirclePlus] (b_) \[CircleTimes] ((c_) \[CirclePlus] 
             (a_)) -> a \[CirclePlus] b \[CircleTimes] c, "Side" -> 1, 
        "Subpattern" -> (b_) \[CircleTimes] ((c_) \[CirclePlus] (a_)), 
        "MatchingConstruct" -> {"CriticalPairLemma", 17}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CircleTimes] (b_)) \[CircleTimes] ((c_) \[CirclePlus] 
            (a_)) -> a \[CircleTimes] b, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 15} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] 
          (a \[CircleTimes] b) \[CircleTimes] c == a], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 18}, 
        "Construct" -> {"SubstitutionLemma", 13}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] (a_) \[CircleTimes] (b_) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CirclePlus] (a \[CircleTimes] b) \[CircleTimes] c == a], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 19} -> 
     <|"Statement" -> HoldForm[a == a \[CirclePlus] 
          (b \[CircleTimes] a) \[CircleTimes] c], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 15}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CirclePlus] 
           ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> a, "Side" -> 1, 
        "Subpattern" -> (a_) \[CircleTimes] (b_), "MatchingConstruct" -> 
         {"Axiom", 1}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, "MatchingSide" -> 1, 
        "Position" -> {2, 1}|>|>, {"CriticalPairLemma", 20} -> 
     <|"Statement" -> HoldForm[(a \[CircleTimes] b) \[CircleTimes] c == 
         ((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] b], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 11}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] 
            (a_)) -> a, "Side" -> 1, "Subpattern" -> (b_) \[CirclePlus] (a_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 19}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CirclePlus] ((b_) \[CircleTimes] (a_)) \[CircleTimes] (c_) -> 
          a, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 16} -> 
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
    {"SubstitutionLemma", 17} -> 
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
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 9}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CircleTimes] 
           (OverBar[a_] \[CirclePlus] (b_)) -> a \[CircleTimes] b, 
        "Side" -> 1, "Subpattern" -> OverBar[a_] \[CirclePlus] (b_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 17}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CirclePlus] ((b_) \[CircleTimes] OverBar[a_]) \[CircleTimes] 
            (c_) -> a \[CirclePlus] b \[CircleTimes] c, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 18} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] 
          ((b \[CircleTimes] OverBar[OverBar[a]]) \[CircleTimes] c) == 
         a \[CircleTimes] (b \[CircleTimes] c)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 23}, 
        "Construct" -> {"CriticalPairLemma", 9}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (OverBar[a_] \[CirclePlus] (b_)) -> 
          a \[CircleTimes] b, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[a \[CircleTimes] ((b \[CircleTimes] OverBar[OverBar[
                a]]) \[CircleTimes] c) == a \[CircleTimes] (b \[CircleTimes] 
             c)], "Source" -> "norm"|>|>, {"SubstitutionLemma", 19} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] 
          ((b \[CircleTimes] a) \[CircleTimes] c) == a \[CircleTimes] 
          (b \[CircleTimes] c)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 18}, "Construct" -> 
         {"SubstitutionLemma", 6}, "Position" -> {2, 1, 2}, 
        "Rule" -> OverBar[OverBar[a_]] -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CircleTimes] 
            ((b \[CircleTimes] a) \[CircleTimes] c) == a \[CircleTimes] 
            (b \[CircleTimes] c)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 20} -> 
     <|"Statement" -> HoldForm[(a \[CircleTimes] b) \[CircleTimes] c == 
         b \[CircleTimes] (a \[CircleTimes] c)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 16}, 
        "Construct" -> {"SubstitutionLemma", 19}, "Position" -> {}, 
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
         {"SubstitutionLemma", 6}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> OverBar[OverBar[a_]] -> a, "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 25} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CircleTimes] a) == 
         a \[CircleTimes] (OverBar[a] \[CirclePlus] b)], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 9}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CircleTimes] 
           (OverBar[a_] \[CirclePlus] (b_)) -> a \[CircleTimes] b, 
        "Side" -> 1, "Subpattern" -> OverBar[a_] \[CirclePlus] (b_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 24}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         OverBar[a_] \[CirclePlus] (b_) \[CircleTimes] (a_) -> 
          OverBar[a] \[CirclePlus] b, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 22} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CircleTimes] a) == 
         a \[CircleTimes] b], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 25}, "Construct" -> 
         {"CriticalPairLemma", 9}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (OverBar[a_] \[CirclePlus] (b_)) -> 
          a \[CircleTimes] b, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[a \[CircleTimes] (b \[CircleTimes] a) == 
           a \[CircleTimes] b], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 26} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] 
          (b \[CircleTimes] a \[CirclePlus] c) == 
         a \[CircleTimes] b \[CirclePlus] a \[CircleTimes] c], 
      "Proof" -> <|"Construct" -> {"Axiom", 2}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CircleTimes] (b_) \[CirclePlus] (a_) \[CircleTimes] 
            (c_) -> a \[CircleTimes] (b \[CirclePlus] c), "Side" -> 1, 
        "Subpattern" -> (a_) \[CircleTimes] (b_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 22}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (a_)) -> 
          a \[CircleTimes] b, "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"SubstitutionLemma", 23} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] 
          (b \[CircleTimes] a \[CirclePlus] c) == a \[CircleTimes] 
          (b \[CirclePlus] c)], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 26}, "Construct" -> {"Axiom", 2}, 
        "Position" -> {}, "Rule" -> (a_) \[CircleTimes] (b_) \[CirclePlus] 
           (a_) \[CircleTimes] (c_) -> a \[CircleTimes] (b \[CirclePlus] c), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CircleTimes] (b \[CircleTimes] a \[CirclePlus] c) == 
           a \[CircleTimes] (b \[CirclePlus] c)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 27} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CirclePlus] 
           b \[CircleTimes] c) == a \[CircleTimes] (b \[CircleTimes] 
           (a \[CirclePlus] c))], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 23}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (a_) \[CirclePlus] 
            (c_)) -> a \[CircleTimes] (b \[CirclePlus] c), "Side" -> 1, 
        "Subpattern" -> (b_) \[CircleTimes] (a_) \[CirclePlus] (c_), 
        "MatchingConstruct" -> {"Axiom", 2}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CircleTimes] (b_) \[CirclePlus] 
           (a_) \[CircleTimes] (c_) -> a \[CircleTimes] (b \[CirclePlus] c), 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 24} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] b == a \[CircleTimes] 
          (b \[CircleTimes] (a \[CirclePlus] c))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 27}, 
        "Construct" -> {"SubstitutionLemma", 13}, "Position" -> {2}, 
        "Rule" -> (a_) \[CirclePlus] (a_) \[CircleTimes] (b_) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[a \[CircleTimes] b == 
           a \[CircleTimes] (b \[CircleTimes] (a \[CirclePlus] c))], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 28} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] c == a \[CircleTimes] 
          ((a \[CirclePlus] b) \[CircleTimes] c)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 24}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CircleTimes] 
           ((b_) \[CircleTimes] ((a_) \[CirclePlus] (c_))) -> 
          a \[CircleTimes] b, "Side" -> 1, "Subpattern" -> 
         (b_) \[CircleTimes] ((a_) \[CirclePlus] (c_)), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 29} -> 
     <|"Statement" -> HoldForm[b \[CircleTimes] (a \[CirclePlus] c) == 
         a \[CircleTimes] b \[CirclePlus] b \[CircleTimes] c], 
      "Proof" -> <|"Construct" -> {"Axiom", 2}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CircleTimes] (b_) \[CirclePlus] (a_) \[CircleTimes] 
            (c_) -> a \[CircleTimes] (b \[CirclePlus] c), "Side" -> 1, 
        "Subpattern" -> (a_) \[CircleTimes] (b_), "MatchingConstruct" -> 
         {"Axiom", 1}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 30} -> 
     <|"Statement" -> HoldForm[(a \[CircleTimes] b) \[CircleTimes] x3 == 
         (a \[CircleTimes] b) \[CircleTimes] ((b \[CircleTimes] 
            (a \[CirclePlus] c)) \[CircleTimes] x3)], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 28}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CircleTimes] 
           (((a_) \[CirclePlus] (b_)) \[CircleTimes] (c_)) -> 
          a \[CircleTimes] c, "Side" -> 1, "Subpattern" -> 
         (a_) \[CirclePlus] (b_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 29}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CircleTimes] (b_) \[CirclePlus] 
           (b_) \[CircleTimes] (c_) -> b \[CircleTimes] (a \[CirclePlus] c), 
        "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"SubstitutionLemma", 25} -> 
     <|"Statement" -> HoldForm[(a \[CircleTimes] b) \[CircleTimes] x3 == 
         b \[CircleTimes] (a \[CircleTimes] ((b \[CircleTimes] 
             (a \[CirclePlus] c)) \[CircleTimes] x3))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 30}, 
        "Construct" -> {"SubstitutionLemma", 20}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[(a \[CircleTimes] b) \[CircleTimes] 
            x3 == b \[CircleTimes] (a \[CircleTimes] ((b \[CircleTimes] (
                a \[CirclePlus] c)) \[CircleTimes] x3))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 26} -> 
     <|"Statement" -> HoldForm[(a \[CircleTimes] b) \[CircleTimes] x3 == 
         b \[CircleTimes] (a \[CircleTimes] ((a \[CirclePlus] 
             c) \[CircleTimes] (b \[CircleTimes] x3)))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 25}, 
        "Construct" -> {"SubstitutionLemma", 20}, "Position" -> {2, 2}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[(a \[CircleTimes] b) \[CircleTimes] 
            x3 == b \[CircleTimes] (a \[CircleTimes] 
             ((a \[CirclePlus] c) \[CircleTimes] (b \[CircleTimes] x3)))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 27} -> 
     <|"Statement" -> HoldForm[(a \[CircleTimes] b) \[CircleTimes] x3 == 
         b \[CircleTimes] (a \[CircleTimes] (b \[CircleTimes] x3))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 26}, 
        "Construct" -> {"CriticalPairLemma", 28}, "Position" -> {2}, 
        "Rule" -> (a_) \[CircleTimes] (((a_) \[CirclePlus] 
             (b_)) \[CircleTimes] (c_)) -> a \[CircleTimes] c, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (a \[CircleTimes] b) \[CircleTimes] x3 == b \[CircleTimes] 
            (a \[CircleTimes] (b \[CircleTimes] x3))], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 31} -> 
     <|"Statement" -> HoldForm[a == a \[CirclePlus] b \[CircleTimes] 
           (a \[CircleTimes] c)], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 15}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CirclePlus] ((a_) \[CircleTimes] 
             (b_)) \[CircleTimes] (c_) -> a, "Side" -> 1, 
        "Subpattern" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 32} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CircleTimes] c) == 
         (a \[CircleTimes] (b \[CircleTimes] c)) \[CircleTimes] b], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 11}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] 
            (a_)) -> a, "Side" -> 1, "Subpattern" -> (b_) \[CirclePlus] (a_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 31}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CirclePlus] (b_) \[CircleTimes] ((a_) \[CircleTimes] (c_)) -> 
          a, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 28} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CircleTimes] c) == 
         b \[CircleTimes] (a \[CircleTimes] (b \[CircleTimes] c))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 32}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CircleTimes] (b \[CircleTimes] c) == b \[CircleTimes] 
            (a \[CircleTimes] (b \[CircleTimes] c))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 29} -> 
     <|"Statement" -> HoldForm[(a \[CircleTimes] b) \[CircleTimes] x3 == 
         a \[CircleTimes] (b \[CircleTimes] x3)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 27}, 
        "Construct" -> {"SubstitutionLemma", 28}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] 
            ((a_) \[CircleTimes] (c_))) -> b \[CircleTimes] 
           (a \[CircleTimes] c), "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[(a \[CircleTimes] b) \[CircleTimes] x3 == 
           a \[CircleTimes] (b \[CircleTimes] x3)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 30} -> 
     <|"Statement" -> HoldForm[b \[CircleTimes] (a \[CircleTimes] x3) == 
         a \[CircleTimes] (b \[CircleTimes] x3)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 29}, 
        "Construct" -> {"SubstitutionLemma", 20}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[b \[CircleTimes] (a \[CircleTimes] 
             x3) == a \[CircleTimes] (b \[CircleTimes] x3)], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 21} -> 
     <|"Statement" -> HoldForm[b \[CircleTimes] (a \[CircleTimes] c) == 
         a \[CircleTimes] (b \[CircleTimes] c)], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Construct" -> 
         {"SubstitutionLemma", 20}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[b \[CircleTimes] (a \[CircleTimes] 
             c) == a \[CircleTimes] (b \[CircleTimes] c)], 
        "Source" -> "cpl"|>|>, {"Conclusion", 1} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CircleTimes] c) == 
         a \[CircleTimes] (b \[CircleTimes] c)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 21}, 
        "Construct" -> {"SubstitutionLemma", 30}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CircleTimes] (b \[CircleTimes] 
             c) == a \[CircleTimes] (b \[CircleTimes] c)], 
        "Source" -> "cpl"|>|>}|>]
