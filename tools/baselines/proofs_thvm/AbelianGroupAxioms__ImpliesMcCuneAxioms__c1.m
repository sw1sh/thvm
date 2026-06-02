ProofObject["EquationalLogic", Inactive[Equal][
  a \[CircleTimes] OverBar[b \[CircleTimes] 
     (((c \[CircleTimes] OverBar[c]) \[CircleTimes] 
       OverBar[d \[CircleTimes] b]) \[CircleTimes] a)], d], 
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
     <|"Statement" -> HoldForm[a \[CircleTimes] OverBar[b \[CircleTimes] 
            (((c \[CircleTimes] OverBar[c]) \[CircleTimes] 
              OverBar[d \[CircleTimes] b]) \[CircleTimes] a)] == d], 
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
     <|"Statement" -> HoldForm[c \[CircleTimes] (a \[CircleTimes] b) == 
         a \[CircleTimes] (b \[CircleTimes] c)], 
      "Proof" -> <|"Construct" -> {"Axiom", 2}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Side" -> 1, "Subpattern" -> {}, "MatchingConstruct" -> {"Axiom", 1}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          a \[CircleTimes] (b \[CircleTimes] c), "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 4} -> 
     <|"Statement" -> HoldForm[b == (OverBar[a] \[CircleTimes] 
           b) \[CircleTimes] a], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 3}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CircleTimes] (OverBar[a_] \[CircleTimes] (b_)) -> b, 
        "Side" -> 1, "Subpattern" -> {}, "MatchingConstruct" -> {"Axiom", 2}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"SubstitutionLemma", 6} -> 
     <|"Statement" -> HoldForm[b == OverBar[a] \[CircleTimes] 
          (b \[CircleTimes] a)], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 4}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {}, "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] 
           (c_) -> a \[CircleTimes] (b \[CircleTimes] c), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          b == OverBar[a] \[CircleTimes] (b \[CircleTimes] a)], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 5} -> 
     <|"Statement" -> HoldForm[OverBar[b] == 
         OverBar[a \[CircleTimes] b] \[CircleTimes] a], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 6}, 
        "Orientation" -> -1, "Rule" -> OverBar[a_] \[CircleTimes] 
           ((b_) \[CircleTimes] (a_)) -> b, "Side" -> 1, 
        "Subpattern" -> (b_) \[CircleTimes] (a_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 6}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> OverBar[a_] \[CircleTimes] ((b_) \[CircleTimes] 
            (a_)) -> b, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 7} -> 
     <|"Statement" -> HoldForm[OverBar[b] == a \[CircleTimes] 
          OverBar[a \[CircleTimes] b]], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 5}, "Construct" -> {"Axiom", 2}, 
        "Position" -> {}, "Rule" -> (a_) \[CircleTimes] (b_) -> 
          b \[CircleTimes] a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[OverBar[b] == a \[CircleTimes] OverBar[a \[CircleTimes] 
              b]], "Source" -> "norm"|>|>, {"CriticalPairLemma", 6} -> 
     <|"Statement" -> HoldForm[b == a \[CircleTimes] (b \[CircleTimes] 
           OverBar[a])], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 
          3}, "Orientation" -> 1, "Rule" -> (a_) \[CircleTimes] 
           (OverBar[a_] \[CircleTimes] (b_)) -> b, "Side" -> 1, 
        "Subpattern" -> OverBar[a_] \[CircleTimes] (b_), 
        "MatchingConstruct" -> {"Axiom", 2}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 7} -> 
     <|"Statement" -> HoldForm[OverBar[b \[CircleTimes] OverBar[a]] == 
         a \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 7}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CircleTimes] OverBar[(a_) \[CircleTimes] (b_)] -> 
          OverBar[b], "Side" -> 1, "Subpattern" -> (a_) \[CircleTimes] (b_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 6}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CircleTimes] ((b_) \[CircleTimes] OverBar[a_]) -> b, 
        "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] OverBar[b \[CircleTimes] 
            ((c \[CircleTimes] OverBar[c]) \[CircleTimes] 
             (OverBar[d \[CircleTimes] b] \[CircleTimes] a))] == d], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {2, 1, 2}, "Rule" -> 
         ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          a \[CircleTimes] (b \[CircleTimes] c), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CircleTimes] 
            OverBar[b \[CircleTimes] ((c \[CircleTimes] OverBar[
                 c]) \[CircleTimes] (OverBar[d \[CircleTimes] 
                  b] \[CircleTimes] a))] == d], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] OverBar[b \[CircleTimes] 
            (c \[CircleTimes] (OverBar[c] \[CircleTimes] (OverBar[
                d \[CircleTimes] b] \[CircleTimes] a)))] == d], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 1}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {2, 1, 2}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          a \[CircleTimes] (b \[CircleTimes] c), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CircleTimes] 
            OverBar[b \[CircleTimes] (c \[CircleTimes] (OverBar[
                 c] \[CircleTimes] (OverBar[d \[CircleTimes] 
                   b] \[CircleTimes] a)))] == d], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 4} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] OverBar[b \[CircleTimes] 
            (OverBar[d \[CircleTimes] b] \[CircleTimes] a)] == d], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 2}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Position" -> {2, 1, 2}, 
        "Rule" -> (a_) \[CircleTimes] (OverBar[a_] \[CircleTimes] (b_)) -> b, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a \[CircleTimes] OverBar[b \[CircleTimes] (OverBar[d \[CircleTimes] 
                 b] \[CircleTimes] a)] == d], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 5} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] OverBar[a \[CircleTimes] 
            (b \[CircleTimes] OverBar[d \[CircleTimes] b])] == d], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 4}, 
        "Construct" -> {"CriticalPairLemma", 3}, "Position" -> {2, 1}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)) -> 
          c \[CircleTimes] (a \[CircleTimes] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CircleTimes] 
            OverBar[a \[CircleTimes] (b \[CircleTimes] OverBar[
                d \[CircleTimes] b])] == d], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 8} -> 
     <|"Statement" -> HoldForm[OverBar[b \[CircleTimes] 
           OverBar[d \[CircleTimes] b]] == d], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 5}, 
        "Construct" -> {"SubstitutionLemma", 7}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] OverBar[(a_) \[CircleTimes] (b_)] -> 
          OverBar[b], "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[OverBar[b \[CircleTimes] OverBar[d \[CircleTimes] b]] == 
           d], "Source" -> "cpl"|>|>, {"SubstitutionLemma", 9} -> 
     <|"Statement" -> HoldForm[(d \[CircleTimes] b) \[CircleTimes] 
          OverBar[b] == d], "Proof" -> <|"Input" -> {"SubstitutionLemma", 8}, 
        "Construct" -> {"CriticalPairLemma", 7}, "Position" -> {}, 
        "Rule" -> OverBar[(a_) \[CircleTimes] OverBar[b_]] -> 
          b \[CircleTimes] OverBar[a], "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(d \[CircleTimes] b) \[CircleTimes] 
            OverBar[b] == d], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 10} -> 
     <|"Statement" -> HoldForm[d \[CircleTimes] (b \[CircleTimes] 
           OverBar[b]) == d], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 9}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {}, "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] 
           (c_) -> a \[CircleTimes] (b \[CircleTimes] c), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          d \[CircleTimes] (b \[CircleTimes] OverBar[b]) == d], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 11} -> 
     <|"Statement" -> HoldForm[d \[CircleTimes] OverTilde[1] == d], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 10}, 
        "Construct" -> {"Axiom", 4}, "Position" -> {2}, 
        "Rule" -> (a_) \[CircleTimes] OverBar[a_] -> OverTilde[1], 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          d \[CircleTimes] OverTilde[1] == d], "Source" -> "cpl"|>|>, 
    {"Conclusion", 1} -> <|"Statement" -> HoldForm[d == d], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 11}, 
        "Construct" -> {"Axiom", 3}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] OverTilde[1] -> a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[d == d], "Source" -> "cpl"|>|>}|>]
