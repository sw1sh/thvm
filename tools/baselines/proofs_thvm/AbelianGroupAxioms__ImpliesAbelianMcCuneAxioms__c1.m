ProofObject["EquationalLogic", Inactive[Equal][
  ((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] 
   OverBar[a \[CircleTimes] c], b], 
 {Inactive[Equal][(a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)), 
   ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_)], 
  Inactive[Equal][(a_) \[CircleTimes] (b_), (b_) \[CircleTimes] (a_)], 
  Inactive[Equal][(a_) \[CircleTimes] OverTilde[1], a_], 
  Inactive[Equal][(a_) \[CircleTimes] OverBar[a_], OverTilde[1]]}, 
 <|"Variables" -> {a, b, c}, "Constants" -> {}, 
  "Proof" -> {{"Axiom", 1} -> <|"Statement" -> 
       HoldForm[a \[CircleTimes] (b \[CircleTimes] c) == 
         (a \[CircleTimes] b) \[CircleTimes] c], "Proof" -> <||>|>, 
    {"Axiom", 2} -> <|"Statement" -> HoldForm[a \[CircleTimes] b == 
         b \[CircleTimes] a], "Proof" -> <||>|>, 
    {"Axiom", 3} -> <|"Statement" -> HoldForm[
        a \[CircleTimes] OverTilde[1] == a], "Proof" -> <||>|>, 
    {"Axiom", 4} -> <|"Statement" -> HoldForm[a \[CircleTimes] OverBar[a] == 
         OverTilde[1]], "Proof" -> <||>|>, {"Hypothesis", 1} -> 
     <|"Statement" -> HoldForm[((a \[CircleTimes] b) \[CircleTimes] 
           c) \[CircleTimes] OverBar[a \[CircleTimes] c] == b], 
      "Proof" -> <||>|>, {"CriticalPairLemma", 1} -> 
     <|"Statement" -> HoldForm[b \[CircleTimes] (OverBar[b] \[CircleTimes] 
           a) == OverTilde[1] \[CircleTimes] a], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          a \[CircleTimes] (b \[CircleTimes] c), "Side" -> 1, 
        "Subpattern" -> (a_) \[CircleTimes] (b_), "MatchingConstruct" -> 
         {"Axiom", 4}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] OverBar[a_] -> OverTilde[1], 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 2} -> 
     <|"Statement" -> HoldForm[OverTilde[1] \[CircleTimes] a == a], 
      "Proof" -> <|"Construct" -> {"Axiom", 2}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Side" -> 1, "Subpattern" -> {}, "MatchingConstruct" -> {"Axiom", 3}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] OverTilde[1] -> a, "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"SubstitutionLemma", 3} -> 
     <|"Statement" -> HoldForm[b \[CircleTimes] (OverBar[b] \[CircleTimes] 
           a) == a], "Proof" -> <|"Input" -> {"CriticalPairLemma", 1}, 
        "Construct" -> {"CriticalPairLemma", 2}, "Position" -> {}, 
        "Rule" -> OverTilde[1] \[CircleTimes] (a_) -> a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[b \[CircleTimes] 
            (OverBar[b] \[CircleTimes] a) == a], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 3} -> 
     <|"Statement" -> HoldForm[b == (OverBar[a] \[CircleTimes] 
           b) \[CircleTimes] a], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 3}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CircleTimes] (OverBar[a_] \[CircleTimes] (b_)) -> b, 
        "Side" -> 1, "Subpattern" -> {}, "MatchingConstruct" -> {"Axiom", 2}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"SubstitutionLemma", 4} -> 
     <|"Statement" -> HoldForm[b == OverBar[a] \[CircleTimes] 
          (b \[CircleTimes] a)], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 3}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {}, "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] 
           (c_) -> a \[CircleTimes] (b \[CircleTimes] c), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          b == OverBar[a] \[CircleTimes] (b \[CircleTimes] a)], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 4} -> 
     <|"Statement" -> HoldForm[OverBar[b] == 
         OverBar[a \[CircleTimes] b] \[CircleTimes] a], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 4}, 
        "Orientation" -> -1, "Rule" -> OverBar[a_] \[CircleTimes] 
           ((b_) \[CircleTimes] (a_)) -> b, "Side" -> 1, 
        "Subpattern" -> (b_) \[CircleTimes] (a_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 4}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> OverBar[a_] \[CircleTimes] ((b_) \[CircleTimes] 
            (a_)) -> b, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 5} -> 
     <|"Statement" -> HoldForm[OverBar[b] == a \[CircleTimes] 
          OverBar[a \[CircleTimes] b]], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 4}, "Construct" -> {"Axiom", 2}, 
        "Position" -> {}, "Rule" -> (a_) \[CircleTimes] (b_) -> 
          b \[CircleTimes] a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[OverBar[b] == a \[CircleTimes] OverBar[a \[CircleTimes] 
              b]], "Source" -> "norm"|>|>, {"CriticalPairLemma", 5} -> 
     <|"Statement" -> HoldForm[OverBar[b \[CircleTimes] a] == 
         OverBar[a] \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 5}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CircleTimes] 
           OverBar[(a_) \[CircleTimes] (b_)] -> OverBar[b], "Side" -> 1, 
        "Subpattern" -> (a_) \[CircleTimes] (b_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 4}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> OverBar[a_] \[CircleTimes] ((b_) \[CircleTimes] 
            (a_)) -> b, "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"CriticalPairLemma", 6} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[a] \[CircleTimes] b] == 
         a \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 5}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CircleTimes] OverBar[(a_) \[CircleTimes] (b_)] -> 
          OverBar[b], "Side" -> 1, "Subpattern" -> (a_) \[CircleTimes] (b_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 3}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] (OverBar[a_] \[CircleTimes] (b_)) -> b, 
        "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"CriticalPairLemma", 7} -> 
     <|"Statement" -> HoldForm[OverBar[c \[CircleTimes] 
           (OverBar[a] \[CircleTimes] b)] == 
         (a \[CircleTimes] OverBar[b]) \[CircleTimes] OverBar[c]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 5}, 
        "Orientation" -> -1, "Rule" -> OverBar[a_] \[CircleTimes] 
           OverBar[b_] -> OverBar[b \[CircleTimes] a], "Side" -> 1, 
        "Subpattern" -> OverBar[a_], "MatchingConstruct" -> 
         {"CriticalPairLemma", 6}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> OverBar[OverBar[a_] \[CircleTimes] (b_)] -> 
          a \[CircleTimes] OverBar[b], "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 6} -> 
     <|"Statement" -> HoldForm[OverBar[c \[CircleTimes] 
           (OverBar[a] \[CircleTimes] b)] == a \[CircleTimes] 
          (OverBar[b] \[CircleTimes] OverBar[c])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 7}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          a \[CircleTimes] (b \[CircleTimes] c), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[c \[CircleTimes] 
             (OverBar[a] \[CircleTimes] b)] == a \[CircleTimes] 
            (OverBar[b] \[CircleTimes] OverBar[c])], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 7} -> 
     <|"Statement" -> HoldForm[OverBar[c \[CircleTimes] 
           (OverBar[a] \[CircleTimes] b)] == a \[CircleTimes] 
          OverBar[c \[CircleTimes] b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 6}, "Construct" -> 
         {"CriticalPairLemma", 5}, "Position" -> {2}, 
        "Rule" -> OverBar[a_] \[CircleTimes] OverBar[b_] -> 
          OverBar[b \[CircleTimes] a], "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[c \[CircleTimes] 
             (OverBar[a] \[CircleTimes] b)] == a \[CircleTimes] 
            OverBar[c \[CircleTimes] b]], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 8} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CircleTimes] 
           OverBar[a \[CircleTimes] b]) == OverTilde[1]], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          a \[CircleTimes] (b \[CircleTimes] c), "Side" -> 1, 
        "Subpattern" -> {}, "MatchingConstruct" -> {"Axiom", 4}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] OverBar[a_] -> OverTilde[1], 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"CriticalPairLemma", 9} -> 
     <|"Statement" -> HoldForm[b \[CircleTimes] OverBar[a \[CircleTimes] 
            (c \[CircleTimes] OverBar[OverBar[b] \[CircleTimes] c])] == 
         OverBar[a \[CircleTimes] OverTilde[1]]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 7}, 
        "Orientation" -> 1, "Rule" -> OverBar[(a_) \[CircleTimes] 
            (OverBar[b_] \[CircleTimes] (c_))] -> b \[CircleTimes] 
           OverBar[a \[CircleTimes] c], "Side" -> 1, "Subpattern" -> 
         OverBar[b_] \[CircleTimes] (c_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 8}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] 
            OverBar[(a_) \[CircleTimes] (b_)]) -> OverTilde[1], 
        "MatchingSide" -> 1, "Position" -> {1, 2}|>|>, 
    {"SubstitutionLemma", 8} -> 
     <|"Statement" -> HoldForm[b \[CircleTimes] OverBar[a \[CircleTimes] 
            (c \[CircleTimes] OverBar[OverBar[b] \[CircleTimes] c])] == 
         OverBar[a]], "Proof" -> <|"Input" -> {"CriticalPairLemma", 9}, 
        "Construct" -> {"Axiom", 3}, "Position" -> {1}, 
        "Rule" -> (a_) \[CircleTimes] OverTilde[1] -> a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[b \[CircleTimes] 
            OverBar[a \[CircleTimes] (c \[CircleTimes] OverBar[
                OverBar[b] \[CircleTimes] c])] == OverBar[a]], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 9} -> 
     <|"Statement" -> HoldForm[b \[CircleTimes] OverBar[a \[CircleTimes] 
            (c \[CircleTimes] (b \[CircleTimes] OverBar[c]))] == OverBar[a]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 8}, 
        "Construct" -> {"CriticalPairLemma", 6}, "Position" -> {2, 1, 2, 2}, 
        "Rule" -> OverBar[OverBar[a_] \[CircleTimes] (b_)] -> 
          a \[CircleTimes] OverBar[b], "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[b \[CircleTimes] 
            OverBar[a \[CircleTimes] (c \[CircleTimes] (b \[CircleTimes] 
                OverBar[c]))] == OverBar[a]], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 10} -> 
     <|"Statement" -> HoldForm[b == a \[CircleTimes] (b \[CircleTimes] 
           OverBar[a])], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 
          3}, "Orientation" -> 1, "Rule" -> (a_) \[CircleTimes] 
           (OverBar[a_] \[CircleTimes] (b_)) -> b, "Side" -> 1, 
        "Subpattern" -> OverBar[a_] \[CircleTimes] (b_), 
        "MatchingConstruct" -> {"Axiom", 2}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 10} -> 
     <|"Statement" -> HoldForm[b \[CircleTimes] OverBar[a \[CircleTimes] 
            b] == OverBar[a]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 9}, "Construct" -> 
         {"CriticalPairLemma", 10}, "Position" -> {2, 1, 2}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] OverBar[a_]) -> b, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          b \[CircleTimes] OverBar[a \[CircleTimes] b] == OverBar[a]], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[(a \[CircleTimes] b) \[CircleTimes] 
          (c \[CircleTimes] OverBar[a \[CircleTimes] c]) == b], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {}, "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] 
           (c_) -> a \[CircleTimes] (b \[CircleTimes] c), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (a \[CircleTimes] b) \[CircleTimes] (c \[CircleTimes] 
             OverBar[a \[CircleTimes] c]) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CircleTimes] 
           (c \[CircleTimes] OverBar[a \[CircleTimes] c])) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 1}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          a \[CircleTimes] (b \[CircleTimes] c), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CircleTimes] (b \[CircleTimes] 
             (c \[CircleTimes] OverBar[a \[CircleTimes] c])) == b], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 11} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CircleTimes] 
           OverBar[a]) == b], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 2}, "Construct" -> 
         {"SubstitutionLemma", 10}, "Position" -> {2, 2}, 
        "Rule" -> (a_) \[CircleTimes] OverBar[(b_) \[CircleTimes] (a_)] -> 
          OverBar[b], "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[a \[CircleTimes] (b \[CircleTimes] OverBar[a]) == b], 
        "Source" -> "cpl"|>|>, {"Conclusion", 1} -> 
     <|"Statement" -> HoldForm[b == b], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 11}, "Construct" -> 
         {"CriticalPairLemma", 10}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] OverBar[a_]) -> b, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[b == b], 
        "Source" -> "cpl"|>|>}|>]
