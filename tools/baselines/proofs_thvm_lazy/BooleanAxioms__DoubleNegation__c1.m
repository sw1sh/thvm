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
    {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CirclePlus] c) == 
         (a \[CircleTimes] b \[CirclePlus] a) \[CircleTimes] 
          (a \[CircleTimes] b \[CirclePlus] c)], 
      "Proof" -> <|"Input" -> {"Axiom", 2}, "Construct" -> {"Axiom", 6}, 
        "Position" -> {}, "Rule" -> (a_) \[CirclePlus] (b_) \[CircleTimes] 
            (c_) -> (a \[CirclePlus] b) \[CircleTimes] (a \[CirclePlus] c), 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CircleTimes] (b \[CirclePlus] c) == 
           (a \[CircleTimes] b \[CirclePlus] a) \[CircleTimes] 
            (a \[CircleTimes] b \[CirclePlus] c)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CirclePlus] c) == 
         (a \[CirclePlus] a \[CircleTimes] b) \[CircleTimes] 
          (a \[CircleTimes] b \[CirclePlus] c)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 1}, 
        "Construct" -> {"Axiom", 5}, "Position" -> {1}, 
        "Rule" -> (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CircleTimes] (b \[CirclePlus] c) == 
           (a \[CirclePlus] a \[CircleTimes] b) \[CircleTimes] 
            (a \[CircleTimes] b \[CirclePlus] c)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 3} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CirclePlus] c) == 
         ((a \[CirclePlus] a) \[CircleTimes] (a \[CirclePlus] 
            b)) \[CircleTimes] (a \[CircleTimes] b \[CirclePlus] c)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 2}, 
        "Construct" -> {"Axiom", 6}, "Position" -> {1}, 
        "Rule" -> (a_) \[CirclePlus] (b_) \[CircleTimes] (c_) -> 
          (a \[CirclePlus] b) \[CircleTimes] (a \[CirclePlus] c), 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CircleTimes] (b \[CirclePlus] c) == 
           ((a \[CirclePlus] a) \[CircleTimes] (a \[CirclePlus] 
              b)) \[CircleTimes] (a \[CircleTimes] b \[CirclePlus] c)], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 4} -> 
     <|"Statement" -> HoldForm[(a \[CirclePlus] b) \[CircleTimes] 
          (a \[CirclePlus] OverBar[b]) == a], 
      "Proof" -> <|"Input" -> {"Axiom", 3}, "Construct" -> {"Axiom", 6}, 
        "Position" -> {}, "Rule" -> (a_) \[CirclePlus] (b_) \[CircleTimes] 
            (c_) -> (a \[CirclePlus] b) \[CircleTimes] (a \[CirclePlus] c), 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (a \[CirclePlus] b) \[CircleTimes] (a \[CirclePlus] OverBar[b]) == 
           a], "Source" -> "norm"|>|>, {"CriticalPairLemma", 1} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] a == a], 
      "Proof" -> <|"Construct" -> {"Axiom", 4}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] OverBar[b_]) -> a, 
        "Side" -> 1, "Subpattern" -> {}, "MatchingConstruct" -> 
         {"SubstitutionLemma", 4}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> ((a_) \[CirclePlus] (b_)) \[CircleTimes] 
           ((a_) \[CirclePlus] OverBar[b_]) -> a, "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"SubstitutionLemma", 5} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CirclePlus] c) == 
         (a \[CircleTimes] (a \[CirclePlus] b)) \[CircleTimes] 
          (a \[CircleTimes] b \[CirclePlus] c)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 3}, 
        "Construct" -> {"CriticalPairLemma", 1}, "Position" -> {1, 1}, 
        "Rule" -> (a_) \[CirclePlus] (a_) -> a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CircleTimes] (b \[CirclePlus] 
             c) == (a \[CircleTimes] (a \[CirclePlus] b)) \[CircleTimes] 
            (a \[CircleTimes] b \[CirclePlus] c)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 2} -> 
     <|"Statement" -> HoldForm[(c \[CirclePlus] a) \[CircleTimes] 
          (c \[CirclePlus] b) == a \[CircleTimes] b \[CirclePlus] c], 
      "Proof" -> <|"Construct" -> {"Axiom", 6}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CirclePlus] (b_) \[CircleTimes] (c_) -> 
          (a \[CirclePlus] b) \[CircleTimes] (a \[CirclePlus] c), 
        "Side" -> 1, "Subpattern" -> {}, "MatchingConstruct" -> {"Axiom", 5}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 3} -> 
     <|"Statement" -> HoldForm[b \[CircleTimes] OverBar[b] \[CirclePlus] a == 
         a], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 2}, 
        "Orientation" -> 1, "Rule" -> 
         ((c_) \[CirclePlus] (a_)) \[CircleTimes] ((c_) \[CirclePlus] 
            (b_)) -> a \[CircleTimes] b \[CirclePlus] c, "Side" -> 1, 
        "Subpattern" -> {}, "MatchingConstruct" -> {"SubstitutionLemma", 4}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((a_) \[CirclePlus] (b_)) \[CircleTimes] ((a_) \[CirclePlus] 
            OverBar[b_]) -> a, "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"CriticalPairLemma", 4} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (OverBar[a] \[CirclePlus] 
           b) == (a \[CircleTimes] (a \[CirclePlus] 
            OverBar[a])) \[CircleTimes] b], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 5}, "Orientation" -> -1, 
        "Rule" -> ((a_) \[CircleTimes] ((a_) \[CirclePlus] 
             (b_))) \[CircleTimes] ((a_) \[CircleTimes] (b_) \[CirclePlus] 
            (c_)) -> a \[CircleTimes] (b \[CirclePlus] c), "Side" -> 1, 
        "Subpattern" -> (a_) \[CircleTimes] (b_) \[CirclePlus] (c_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 3}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] OverBar[a_] \[CirclePlus] (b_) -> b, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 6} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (OverBar[a] \[CirclePlus] 
           b) == a \[CircleTimes] b], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 4}, "Construct" -> {"Axiom", 4}, 
        "Position" -> {1}, "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] 
            OverBar[b_]) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[a \[CircleTimes] (OverBar[a] \[CirclePlus] b) == 
           a \[CircleTimes] b], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 5} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] b == a \[CircleTimes] 
          (b \[CirclePlus] OverBar[a])], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 6}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CircleTimes] (OverBar[a_] \[CirclePlus] (b_)) -> 
          a \[CircleTimes] b, "Side" -> 1, "Subpattern" -> 
         OverBar[a_] \[CirclePlus] (b_), "MatchingConstruct" -> {"Axiom", 5}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 6} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CircleTimes] 
           OverBar[b]) == a \[CircleTimes] OverBar[a]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 5}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CircleTimes] 
           ((b_) \[CirclePlus] OverBar[a_]) -> a \[CircleTimes] b, 
        "Side" -> 1, "Subpattern" -> (b_) \[CirclePlus] OverBar[a_], 
        "MatchingConstruct" -> {"CriticalPairLemma", 3}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] OverBar[a_] \[CirclePlus] (b_) -> b, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 7} -> 
     <|"Statement" -> HoldForm[(a \[CirclePlus] b) \[CircleTimes] 
          (a \[CirclePlus] c \[CircleTimes] OverBar[c]) == 
         a \[CirclePlus] b \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Construct" -> {"Axiom", 6}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CirclePlus] (b_) \[CircleTimes] (c_) -> 
          (a \[CirclePlus] b) \[CircleTimes] (a \[CirclePlus] c), 
        "Side" -> 1, "Subpattern" -> (b_) \[CircleTimes] (c_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 6}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] ((b_) \[CircleTimes] OverBar[b_]) -> 
          a \[CircleTimes] OverBar[a], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 7} -> 
     <|"Statement" -> HoldForm[(a \[CirclePlus] b) \[CircleTimes] 
          (a \[CirclePlus] c \[CircleTimes] OverBar[c]) == 
         (a \[CirclePlus] b) \[CircleTimes] (a \[CirclePlus] OverBar[b])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 7}, 
        "Construct" -> {"Axiom", 6}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] (b_) \[CircleTimes] (c_) -> 
          (a \[CirclePlus] b) \[CircleTimes] (a \[CirclePlus] c), 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (a \[CirclePlus] b) \[CircleTimes] (a \[CirclePlus] 
             c \[CircleTimes] OverBar[c]) == (a \[CirclePlus] 
             b) \[CircleTimes] (a \[CirclePlus] OverBar[b])], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 8} -> 
     <|"Statement" -> HoldForm[(a \[CirclePlus] b) \[CircleTimes] 
          (a \[CirclePlus] c \[CircleTimes] OverBar[c]) == a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 7}, 
        "Construct" -> {"SubstitutionLemma", 4}, "Position" -> {}, 
        "Rule" -> ((a_) \[CirclePlus] (b_)) \[CircleTimes] 
           ((a_) \[CirclePlus] OverBar[b_]) -> a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[(a \[CirclePlus] b) \[CircleTimes] 
            (a \[CirclePlus] c \[CircleTimes] OverBar[c]) == a], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 9} -> 
     <|"Statement" -> HoldForm[(a \[CirclePlus] b) \[CircleTimes] 
          ((a \[CirclePlus] c) \[CircleTimes] (a \[CirclePlus] 
            OverBar[c])) == a], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 8}, "Construct" -> {"Axiom", 6}, 
        "Position" -> {2}, "Rule" -> (a_) \[CirclePlus] (b_) \[CircleTimes] 
            (c_) -> (a \[CirclePlus] b) \[CircleTimes] (a \[CirclePlus] c), 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (a \[CirclePlus] b) \[CircleTimes] ((a \[CirclePlus] 
              c) \[CircleTimes] (a \[CirclePlus] OverBar[c])) == a], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 10} -> 
     <|"Statement" -> HoldForm[(a \[CirclePlus] b) \[CircleTimes] a == a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 9}, 
        "Construct" -> {"SubstitutionLemma", 4}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CirclePlus] (b_)) \[CircleTimes] 
           ((a_) \[CirclePlus] OverBar[b_]) -> a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(a \[CirclePlus] b) \[CircleTimes] 
            a == a], "Source" -> "norm"|>|>, {"SubstitutionLemma", 11} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (a \[CirclePlus] b) == a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 10}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a \[CircleTimes] (a \[CirclePlus] b) == a], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 8} -> 
     <|"Statement" -> HoldForm[a == a \[CircleTimes] (b \[CirclePlus] a)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 11}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CircleTimes] ((a_) \[CirclePlus] 
            (b_)) -> a, "Side" -> 1, "Subpattern" -> (a_) \[CirclePlus] (b_), 
        "MatchingConstruct" -> {"Axiom", 5}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 9} -> 
     <|"Statement" -> HoldForm[b == (a \[CirclePlus] 
           OverBar[a]) \[CircleTimes] b], "Proof" -> 
       <|"Construct" -> {"Axiom", 4}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CirclePlus] OverBar[b_]) -> a, 
        "Side" -> 1, "Subpattern" -> {}, "MatchingConstruct" -> {"Axiom", 1}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 10} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] OverBar[OverBar[a]] == a], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 9}, 
        "Orientation" -> -1, "Rule" -> 
         ((a_) \[CirclePlus] OverBar[a_]) \[CircleTimes] (b_) -> b, 
        "Side" -> 1, "Subpattern" -> {}, "MatchingConstruct" -> 
         {"SubstitutionLemma", 4}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> ((a_) \[CirclePlus] (b_)) \[CircleTimes] 
           ((a_) \[CirclePlus] OverBar[b_]) -> a, "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 11} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[a]] == 
         OverBar[OverBar[a]] \[CircleTimes] a], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 8}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CircleTimes] 
           ((b_) \[CirclePlus] (a_)) -> a, "Side" -> 1, 
        "Subpattern" -> (b_) \[CirclePlus] (a_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 10}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CirclePlus] OverBar[OverBar[a_]] -> a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 12} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[a]] == a \[CircleTimes] 
          OverBar[OverBar[a]]], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 11}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {}, "Rule" -> (a_) \[CircleTimes] (b_) -> 
          b \[CircleTimes] a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[OverBar[OverBar[a]] == a \[CircleTimes] 
            OverBar[OverBar[a]]], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 12} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] OverBar[OverBar[a]] == a], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 6}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CircleTimes] 
           (OverBar[a_] \[CirclePlus] (b_)) -> a \[CircleTimes] b, 
        "Side" -> 1, "Subpattern" -> {}, "MatchingConstruct" -> {"Axiom", 4}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] ((b_) \[CirclePlus] OverBar[b_]) -> a, 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"SubstitutionLemma", 13} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[a]] == a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 12}, 
        "Construct" -> {"CriticalPairLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] OverBar[OverBar[a_]] -> a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[OverBar[OverBar[a]] == 
           a], "Source" -> "norm"|>|>, {"Conclusion", 1} -> 
     <|"Statement" -> HoldForm[a == a], "Proof" -> 
       <|"Input" -> {"Hypothesis", 1}, "Construct" -> {"SubstitutionLemma", 
          13}, "Position" -> {}, "Rule" -> OverBar[OverBar[a_]] -> a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[a == a], 
        "Source" -> "cpl"|>|>}|>]
