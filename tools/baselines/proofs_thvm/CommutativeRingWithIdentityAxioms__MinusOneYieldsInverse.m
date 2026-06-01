ProofObject["EquationalLogic", Inactive[Equal][
  OverBar[OverTilde[1]] \[CircleTimes] a, OverBar[a]], 
 {Inactive[Equal][(a_) \[CirclePlus] ((b_) \[CirclePlus] (c_)), 
   ((a_) \[CirclePlus] (b_)) \[CirclePlus] (c_)], 
  Inactive[Equal][(a_) \[CirclePlus] (b_), (b_) \[CirclePlus] (a_)], 
  Inactive[Equal][(a_) \[CirclePlus] OverTilde[0], a_], 
  Inactive[Equal][(a_) \[CirclePlus] OverBar[a_], OverTilde[0]], 
  Inactive[Equal][(a_) \[CircleTimes] ((b_) \[CirclePlus] (c_)), 
   (a_) \[CircleTimes] (b_) \[CirclePlus] (a_) \[CircleTimes] (c_)], 
  Inactive[Equal][(a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)), 
   ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_)], 
  Inactive[Equal][(a_) \[CircleTimes] (b_), (b_) \[CircleTimes] (a_)], 
  Inactive[Equal][(a_) \[CircleTimes] OverTilde[1], a_]}, 
 <|"Variables" -> {a, b, c, x3}, "Constants" -> {}, 
  "Proof" -> {{"Axiom", 1} -> <|"Statement" -> 
       HoldForm[a \[CirclePlus] (b \[CirclePlus] c) == 
         (a \[CirclePlus] b) \[CirclePlus] c], "Proof" -> <||>|>, 
    {"Axiom", 2} -> <|"Statement" -> HoldForm[a \[CirclePlus] b == 
         b \[CirclePlus] a], "Proof" -> <||>|>, 
    {"Axiom", 3} -> <|"Statement" -> HoldForm[a \[CirclePlus] OverTilde[0] == 
         a], "Proof" -> <||>|>, {"Axiom", 4} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] OverBar[a] == OverTilde[0]], 
      "Proof" -> <||>|>, {"Axiom", 5} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CirclePlus] c) == 
         a \[CircleTimes] b \[CirclePlus] a \[CircleTimes] c], 
      "Proof" -> <||>|>, {"Axiom", 6} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CircleTimes] c) == 
         (a \[CircleTimes] b) \[CircleTimes] c], "Proof" -> <||>|>, 
    {"Axiom", 7} -> <|"Statement" -> HoldForm[a \[CircleTimes] b == 
         b \[CircleTimes] a], "Proof" -> <||>|>, 
    {"Axiom", 8} -> <|"Statement" -> HoldForm[
        a \[CircleTimes] OverTilde[1] == a], "Proof" -> <||>|>, 
    {"Hypothesis", 1} -> <|"Statement" -> 
       HoldForm[OverBar[OverTilde[1]] \[CircleTimes] a == OverBar[a]], 
      "Proof" -> <||>|>, {"CriticalPairLemma", 1} -> 
     <|"Statement" -> HoldForm[b \[CirclePlus] (OverBar[b] \[CirclePlus] 
           a) == OverTilde[0] \[CirclePlus] a], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> ((a_) \[CirclePlus] (b_)) \[CirclePlus] (c_) -> 
          a \[CirclePlus] (b \[CirclePlus] c), "Side" -> 1, 
        "Subpattern" -> (a_) \[CirclePlus] (b_), "MatchingConstruct" -> 
         {"Axiom", 4}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CirclePlus] OverBar[a_] -> OverTilde[0], "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 2} -> 
     <|"Statement" -> HoldForm[OverTilde[0] \[CirclePlus] a == a], 
      "Proof" -> <|"Construct" -> {"Axiom", 2}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, "Side" -> 1, 
        "Subpattern" -> {}, "MatchingConstruct" -> {"Axiom", 3}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CirclePlus] OverTilde[0] -> a, "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[b \[CirclePlus] (OverBar[b] \[CirclePlus] 
           a) == a], "Proof" -> <|"Input" -> {"CriticalPairLemma", 1}, 
        "Construct" -> {"CriticalPairLemma", 2}, "Position" -> {}, 
        "Rule" -> OverTilde[0] \[CirclePlus] (a_) -> a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[b \[CirclePlus] 
            (OverBar[b] \[CirclePlus] a) == a], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 3} -> 
     <|"Statement" -> HoldForm[b == (OverBar[a] \[CirclePlus] 
           b) \[CirclePlus] a], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 2}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CirclePlus] (OverBar[a_] \[CirclePlus] (b_)) -> b, 
        "Side" -> 1, "Subpattern" -> {}, "MatchingConstruct" -> {"Axiom", 2}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"SubstitutionLemma", 3} -> 
     <|"Statement" -> HoldForm[b == OverBar[a] \[CirclePlus] 
          (b \[CirclePlus] a)], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 3}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {}, "Rule" -> ((a_) \[CirclePlus] (b_)) \[CirclePlus] 
           (c_) -> a \[CirclePlus] (b \[CirclePlus] c), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[b == OverBar[a] \[CirclePlus] 
            (b \[CirclePlus] a)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 4} -> 
     <|"Statement" -> HoldForm[OverBar[b] == 
         OverBar[a \[CirclePlus] b] \[CirclePlus] a], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 3}, 
        "Orientation" -> -1, "Rule" -> OverBar[a_] \[CirclePlus] 
           ((b_) \[CirclePlus] (a_)) -> b, "Side" -> 1, 
        "Subpattern" -> (b_) \[CirclePlus] (a_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 3}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> OverBar[a_] \[CirclePlus] ((b_) \[CirclePlus] 
            (a_)) -> b, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 4} -> 
     <|"Statement" -> HoldForm[OverBar[b] == a \[CirclePlus] 
          OverBar[a \[CirclePlus] b]], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 4}, "Construct" -> {"Axiom", 2}, 
        "Position" -> {}, "Rule" -> (a_) \[CirclePlus] (b_) -> 
          b \[CirclePlus] a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[OverBar[b] == a \[CirclePlus] OverBar[a \[CirclePlus] b]], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 5} -> 
     <|"Statement" -> HoldForm[OverBar[a \[CircleTimes] c] == 
         a \[CircleTimes] b \[CirclePlus] OverBar[a \[CircleTimes] 
            (b \[CirclePlus] c)]], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 4}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CirclePlus] OverBar[(a_) \[CirclePlus] (b_)] -> 
          OverBar[b], "Side" -> 1, "Subpattern" -> (a_) \[CirclePlus] (b_), 
        "MatchingConstruct" -> {"Axiom", 5}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CircleTimes] (b_) \[CirclePlus] 
           (a_) \[CircleTimes] (c_) -> a \[CircleTimes] (b \[CirclePlus] c), 
        "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"CriticalPairLemma", 6} -> 
     <|"Statement" -> HoldForm[OverBar[a \[CircleTimes] 
           (OverBar[b] \[CirclePlus] c)] == a \[CircleTimes] b \[CirclePlus] 
          OverBar[a \[CircleTimes] c]], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 5}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CircleTimes] (b_) \[CirclePlus] 
           OverBar[(a_) \[CircleTimes] ((b_) \[CirclePlus] (c_))] -> 
          OverBar[a \[CircleTimes] c], "Side" -> 1, "Subpattern" -> 
         (b_) \[CirclePlus] (c_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 2}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CirclePlus] (OverBar[a_] \[CirclePlus] 
            (b_)) -> b, "MatchingSide" -> 1, "Position" -> {2, 1, 2}|>|>, 
    {"CriticalPairLemma", 7} -> 
     <|"Statement" -> HoldForm[OverBar[a \[CircleTimes] OverTilde[0]] == 
         a \[CircleTimes] b \[CirclePlus] OverBar[a \[CircleTimes] b]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 5}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CircleTimes] (b_) \[CirclePlus] 
           OverBar[(a_) \[CircleTimes] ((b_) \[CirclePlus] (c_))] -> 
          OverBar[a \[CircleTimes] c], "Side" -> 1, "Subpattern" -> 
         (b_) \[CirclePlus] (c_), "MatchingConstruct" -> {"Axiom", 3}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CirclePlus] OverTilde[0] -> a, "MatchingSide" -> 1, 
        "Position" -> {2, 1, 2}|>|>, {"SubstitutionLemma", 5} -> 
     <|"Statement" -> HoldForm[OverBar[a \[CircleTimes] OverTilde[0]] == 
         OverTilde[0]], "Proof" -> <|"Input" -> {"CriticalPairLemma", 7}, 
        "Construct" -> {"Axiom", 4}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] OverBar[a_] -> OverTilde[0], 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a \[CircleTimes] OverTilde[0]] == OverTilde[0]], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 8} -> 
     <|"Statement" -> HoldForm[OverTilde[0] == 
         a \[CircleTimes] OverTilde[0] \[CirclePlus] OverTilde[0]], 
      "Proof" -> <|"Construct" -> {"Axiom", 4}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CirclePlus] OverBar[a_] -> OverTilde[0], 
        "Side" -> 1, "Subpattern" -> OverBar[a_], "MatchingConstruct" -> 
         {"SubstitutionLemma", 5}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> OverBar[(a_) \[CircleTimes] OverTilde[0]] -> 
          OverTilde[0], "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 6} -> 
     <|"Statement" -> HoldForm[OverTilde[0] == a \[CircleTimes] 
          OverTilde[0]], "Proof" -> <|"Input" -> {"CriticalPairLemma", 8}, 
        "Construct" -> {"Axiom", 3}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] OverTilde[0] -> a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverTilde[0] == a \[CircleTimes] 
            OverTilde[0]], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 9} -> 
     <|"Statement" -> HoldForm[OverBar[a \[CircleTimes] 
           (OverBar[b] \[CirclePlus] OverTilde[0])] == 
         a \[CircleTimes] b \[CirclePlus] OverBar[OverTilde[0]]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 6}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CircleTimes] (b_) \[CirclePlus] 
           OverBar[(a_) \[CircleTimes] (c_)] -> OverBar[a \[CircleTimes] 
            (OverBar[b] \[CirclePlus] c)], "Side" -> 1, 
        "Subpattern" -> (a_) \[CircleTimes] (c_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 6}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CircleTimes] OverTilde[0] -> OverTilde[0], 
        "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"SubstitutionLemma", 7} -> 
     <|"Statement" -> HoldForm[OverBar[a \[CircleTimes] 
           (OverBar[b] \[CirclePlus] OverTilde[0])] == 
         OverBar[OverTilde[0]] \[CirclePlus] a \[CircleTimes] b], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 9}, 
        "Construct" -> {"Axiom", 2}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a \[CircleTimes] (OverBar[b] \[CirclePlus] 
              OverTilde[0])] == OverBar[OverTilde[0]] \[CirclePlus] 
            a \[CircleTimes] b], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 10} -> 
     <|"Statement" -> HoldForm[OverBar[OverTilde[0]] == OverTilde[0]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 2}, 
        "Orientation" -> 1, "Rule" -> OverTilde[0] \[CirclePlus] (a_) -> a, 
        "Side" -> 1, "Subpattern" -> {}, "MatchingConstruct" -> {"Axiom", 4}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CirclePlus] OverBar[a_] -> OverTilde[0], "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"SubstitutionLemma", 8} -> 
     <|"Statement" -> HoldForm[OverBar[a \[CircleTimes] 
           (OverBar[b] \[CirclePlus] OverTilde[0])] == 
         OverTilde[0] \[CirclePlus] a \[CircleTimes] b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 7}, 
        "Construct" -> {"CriticalPairLemma", 10}, "Position" -> {1}, 
        "Rule" -> OverBar[OverTilde[0]] -> OverTilde[0], "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a \[CircleTimes] 
             (OverBar[b] \[CirclePlus] OverTilde[0])] == 
           OverTilde[0] \[CirclePlus] a \[CircleTimes] b], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 9} -> 
     <|"Statement" -> HoldForm[OverBar[a \[CircleTimes] 
           (OverBar[b] \[CirclePlus] OverTilde[0])] == a \[CircleTimes] b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 8}, 
        "Construct" -> {"CriticalPairLemma", 2}, "Position" -> {}, 
        "Rule" -> OverTilde[0] \[CirclePlus] (a_) -> a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a \[CircleTimes] 
             (OverBar[b] \[CirclePlus] OverTilde[0])] == a \[CircleTimes] b], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 11} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[a]] == a \[CirclePlus] 
          OverTilde[0]], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 
          2}, "Orientation" -> 1, "Rule" -> (a_) \[CirclePlus] 
           (OverBar[a_] \[CirclePlus] (b_)) -> b, "Side" -> 1, 
        "Subpattern" -> OverBar[a_] \[CirclePlus] (b_), 
        "MatchingConstruct" -> {"Axiom", 4}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CirclePlus] OverBar[a_] -> OverTilde[0], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 10} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[a]] == a], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 11}, 
        "Construct" -> {"Axiom", 3}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] OverTilde[0] -> a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[OverBar[a]] == a], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 12} -> 
     <|"Statement" -> HoldForm[b == OverBar[a] \[CirclePlus] 
          (a \[CirclePlus] b)], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 2}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CirclePlus] (OverBar[a_] \[CirclePlus] (b_)) -> b, 
        "Side" -> 1, "Subpattern" -> OverBar[a_], "MatchingConstruct" -> 
         {"SubstitutionLemma", 10}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> OverBar[OverBar[a_]] -> a, "MatchingSide" -> 1, 
        "Position" -> {2, 1}|>|>, {"CriticalPairLemma", 13} -> 
     <|"Statement" -> HoldForm[OverBar[a \[CirclePlus] b] == 
         OverBar[a] \[CirclePlus] OverBar[b]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 4}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CirclePlus] 
           OverBar[(a_) \[CirclePlus] (b_)] -> OverBar[b], "Side" -> 1, 
        "Subpattern" -> (a_) \[CirclePlus] (b_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 12}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> OverBar[a_] \[CirclePlus] ((a_) \[CirclePlus] 
            (b_)) -> b, "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"CriticalPairLemma", 14} -> 
     <|"Statement" -> HoldForm[OverBar[b \[CirclePlus] a] == 
         OverBar[a] \[CirclePlus] OverBar[b]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 4}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CirclePlus] 
           OverBar[(a_) \[CirclePlus] (b_)] -> OverBar[b], "Side" -> 1, 
        "Subpattern" -> (a_) \[CirclePlus] (b_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 3}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> OverBar[a_] \[CirclePlus] ((b_) \[CirclePlus] 
            (a_)) -> b, "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"SubstitutionLemma", 11} -> 
     <|"Statement" -> HoldForm[OverBar[a \[CirclePlus] b] == 
         OverBar[b \[CirclePlus] a]], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 13}, "Construct" -> 
         {"CriticalPairLemma", 14}, "Position" -> {}, 
        "Rule" -> OverBar[a_] \[CirclePlus] OverBar[b_] -> 
          OverBar[b \[CirclePlus] a], "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a \[CirclePlus] b] == 
           OverBar[b \[CirclePlus] a]], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 15} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CirclePlus] c) == 
         a \[CircleTimes] b \[CirclePlus] c \[CircleTimes] a], 
      "Proof" -> <|"Construct" -> {"Axiom", 5}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CircleTimes] (b_) \[CirclePlus] (a_) \[CircleTimes] 
            (c_) -> a \[CircleTimes] (b \[CirclePlus] c), "Side" -> 1, 
        "Subpattern" -> (a_) \[CircleTimes] (c_), "MatchingConstruct" -> 
         {"Axiom", 7}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 16} -> 
     <|"Statement" -> HoldForm[OverBar[c \[CircleTimes] a \[CirclePlus] 
           a \[CircleTimes] b] == OverBar[a \[CircleTimes] 
           (b \[CirclePlus] c)]], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 11}, "Orientation" -> 1, 
        "Rule" -> OverBar[(a_) \[CirclePlus] (b_)] -> 
          OverBar[b \[CirclePlus] a], "Side" -> 1, "Subpattern" -> 
         (a_) \[CirclePlus] (b_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 15}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CircleTimes] (b_) \[CirclePlus] 
           (c_) \[CircleTimes] (a_) -> a \[CircleTimes] (b \[CirclePlus] c), 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 17} -> 
     <|"Statement" -> HoldForm[b \[CircleTimes] (a \[CirclePlus] c) == 
         a \[CircleTimes] b \[CirclePlus] b \[CircleTimes] c], 
      "Proof" -> <|"Construct" -> {"Axiom", 5}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CircleTimes] (b_) \[CirclePlus] (a_) \[CircleTimes] 
            (c_) -> a \[CircleTimes] (b \[CirclePlus] c), "Side" -> 1, 
        "Subpattern" -> (a_) \[CircleTimes] (b_), "MatchingConstruct" -> 
         {"Axiom", 7}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 12} -> 
     <|"Statement" -> HoldForm[OverBar[a \[CircleTimes] (c \[CirclePlus] 
            b)] == OverBar[a \[CircleTimes] (b \[CirclePlus] c)]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 16}, 
        "Construct" -> {"CriticalPairLemma", 17}, "Position" -> {1}, 
        "Rule" -> (a_) \[CircleTimes] (b_) \[CirclePlus] (b_) \[CircleTimes] 
            (c_) -> b \[CircleTimes] (a \[CirclePlus] c), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          OverBar[a \[CircleTimes] (c \[CirclePlus] b)] == 
           OverBar[a \[CircleTimes] (b \[CirclePlus] c)]], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 13} -> 
     <|"Statement" -> HoldForm[OverBar[a \[CircleTimes] 
           (OverTilde[0] \[CirclePlus] OverBar[b])] == a \[CircleTimes] b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 9}, 
        "Construct" -> {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Rule" -> OverBar[(a_) \[CircleTimes] ((b_) \[CirclePlus] (c_))] -> 
          OverBar[a \[CircleTimes] (c \[CirclePlus] b)], "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[OverBar[a \[CircleTimes] 
             (OverTilde[0] \[CirclePlus] OverBar[b])] == a \[CircleTimes] b], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 14} -> 
     <|"Statement" -> HoldForm[OverBar[a \[CircleTimes] OverBar[b]] == 
         a \[CircleTimes] b], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 13}, "Construct" -> 
         {"CriticalPairLemma", 2}, "Position" -> {1, 2}, 
        "Rule" -> OverTilde[0] \[CirclePlus] (a_) -> a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[OverBar[a \[CircleTimes] 
             OverBar[b]] == a \[CircleTimes] b], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 18} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] OverBar[b] == 
         OverBar[a \[CircleTimes] b]], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 14}, "Orientation" -> 1, 
        "Rule" -> OverBar[(a_) \[CircleTimes] OverBar[b_]] -> 
          a \[CircleTimes] b, "Side" -> 1, "Subpattern" -> OverBar[b_], 
        "MatchingConstruct" -> {"SubstitutionLemma", 10}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> OverBar[OverBar[a_]] -> 
          a, "MatchingSide" -> 1, "Position" -> {1, 2}|>|>, 
    {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] OverBar[OverTilde[1]] == 
         OverBar[a]], "Proof" -> <|"Input" -> {"Hypothesis", 1}, 
        "Construct" -> {"Axiom", 7}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a \[CircleTimes] OverBar[OverTilde[1]] == OverBar[a]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 15} -> 
     <|"Statement" -> HoldForm[OverBar[a \[CircleTimes] OverTilde[1]] == 
         OverBar[a]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 1}, 
        "Construct" -> {"CriticalPairLemma", 18}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] OverBar[b_] -> 
          OverBar[a \[CircleTimes] b], "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[OverBar[a \[CircleTimes] 
             OverTilde[1]] == OverBar[a]], "Source" -> "cpl"|>|>, 
    {"Conclusion", 1} -> <|"Statement" -> HoldForm[OverBar[a] == OverBar[a]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 15}, 
        "Construct" -> {"Axiom", 8}, "Position" -> {1}, 
        "Rule" -> (a_) \[CircleTimes] OverTilde[1] -> a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[OverBar[a] == OverBar[a]], 
        "Source" -> "cpl"|>|>}|>]
