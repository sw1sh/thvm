ProofObject["EquationalLogic", Inactive[Equal][
  a \[CircleTimes] OverBar[b \[CircleTimes] 
     (((c \[CircleTimes] OverBar[c]) \[CircleTimes] 
       OverBar[d \[CircleTimes] b]) \[CircleTimes] a)], d], 
 {Inactive[Equal][(a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)), 
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
    {"SubstitutionLemma", 3} -> 
     <|"Statement" -> HoldForm[OverTilde[1] \[CircleTimes] 
          OverBar[OverBar[a]] == a], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 2}, "Construct" -> {"Axiom", 2}, 
        "Position" -> {}, "Rule" -> (a_) \[CircleTimes] OverTilde[1] -> a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverTilde[1] \[CircleTimes] OverBar[OverBar[a]] == a], 
        "Source" -> "cpl"|>|>, {"CriticalPairLemma", 3} -> 
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
         {"SubstitutionLemma", 3}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> OverTilde[1] \[CircleTimes] OverBar[OverBar[a_]] -> 
          a, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 4} -> 
     <|"Statement" -> HoldForm[OverTilde[1] \[CircleTimes] a == a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 3}, 
        "Construct" -> {"CriticalPairLemma", 4}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] OverBar[OverBar[b_]] -> 
          a \[CircleTimes] b, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[OverTilde[1] \[CircleTimes] a == a], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 5} -> 
     <|"Statement" -> HoldForm[b \[CircleTimes] (OverBar[b] \[CircleTimes] 
           a) == a], "Proof" -> <|"Input" -> {"CriticalPairLemma", 1}, 
        "Construct" -> {"SubstitutionLemma", 4}, "Position" -> {}, 
        "Rule" -> OverTilde[1] \[CircleTimes] (a_) -> a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[b \[CircleTimes] 
            (OverBar[b] \[CircleTimes] a) == a], "Source" -> "cpl"|>|>, 
    {"CriticalPairLemma", 5} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[a]] == 
         OverTilde[1] \[CircleTimes] a], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 4}, "Orientation" -> 1, 
        "Rule" -> OverTilde[1] \[CircleTimes] (a_) -> a, "Side" -> 1, 
        "Subpattern" -> {}, "MatchingConstruct" -> {"CriticalPairLemma", 4}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] OverBar[OverBar[b_]] -> a \[CircleTimes] b, 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"SubstitutionLemma", 7} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[a]] == a], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 5}, 
        "Construct" -> {"SubstitutionLemma", 4}, "Position" -> {}, 
        "Rule" -> OverTilde[1] \[CircleTimes] (a_) -> a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[OverBar[a]] == a], 
        "Source" -> "cpl"|>|>, {"CriticalPairLemma", 6} -> 
     <|"Statement" -> HoldForm[OverTilde[1] == OverBar[a] \[CircleTimes] a], 
      "Proof" -> <|"Construct" -> {"Axiom", 3}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CircleTimes] OverBar[a_] -> OverTilde[1], 
        "Side" -> 1, "Subpattern" -> OverBar[a_], "MatchingConstruct" -> 
         {"SubstitutionLemma", 7}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> OverBar[OverBar[a_]] -> a, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 7} -> 
     <|"Statement" -> HoldForm[OverBar[b] \[CircleTimes] (b \[CircleTimes] 
           a) == OverTilde[1] \[CircleTimes] a], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          a \[CircleTimes] (b \[CircleTimes] c), "Side" -> 1, 
        "Subpattern" -> (a_) \[CircleTimes] (b_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 6}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> OverBar[a_] \[CircleTimes] (a_) -> OverTilde[1], 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"SubstitutionLemma", 8} -> 
     <|"Statement" -> HoldForm[OverBar[b] \[CircleTimes] (b \[CircleTimes] 
           a) == a], "Proof" -> <|"Input" -> {"CriticalPairLemma", 7}, 
        "Construct" -> {"SubstitutionLemma", 4}, "Position" -> {}, 
        "Rule" -> OverTilde[1] \[CircleTimes] (a_) -> a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[b] \[CircleTimes] 
            (b \[CircleTimes] a) == a], "Source" -> "cpl"|>|>, 
    {"CriticalPairLemma", 8} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CircleTimes] 
           OverBar[a \[CircleTimes] b]) == OverTilde[1]], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          a \[CircleTimes] (b \[CircleTimes] c), "Side" -> 1, 
        "Subpattern" -> {}, "MatchingConstruct" -> {"Axiom", 3}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] OverBar[a_] -> OverTilde[1], 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"CriticalPairLemma", 9} -> 
     <|"Statement" -> HoldForm[b \[CircleTimes] OverBar[a \[CircleTimes] 
            b] == OverBar[a] \[CircleTimes] OverTilde[1]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 8}, 
        "Orientation" -> 1, "Rule" -> OverBar[a_] \[CircleTimes] 
           ((a_) \[CircleTimes] (b_)) -> b, "Side" -> 1, 
        "Subpattern" -> (a_) \[CircleTimes] (b_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 8}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] 
            OverBar[(a_) \[CircleTimes] (b_)]) -> OverTilde[1], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 9} -> 
     <|"Statement" -> HoldForm[b \[CircleTimes] OverBar[a \[CircleTimes] 
            b] == OverBar[a]], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 9}, "Construct" -> {"Axiom", 2}, 
        "Position" -> {}, "Rule" -> (a_) \[CircleTimes] OverTilde[1] -> a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          b \[CircleTimes] OverBar[a \[CircleTimes] b] == OverBar[a]], 
        "Source" -> "cpl"|>|>, {"CriticalPairLemma", 10} -> 
     <|"Statement" -> HoldForm[OverBar[b \[CircleTimes] a] == 
         OverBar[a] \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 8}, 
        "Orientation" -> 1, "Rule" -> OverBar[a_] \[CircleTimes] 
           ((a_) \[CircleTimes] (b_)) -> b, "Side" -> 1, 
        "Subpattern" -> (a_) \[CircleTimes] (b_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 9}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CircleTimes] OverBar[(b_) \[CircleTimes] 
             (a_)] -> OverBar[b], "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 11} -> 
     <|"Statement" -> HoldForm[OverBar[b \[CircleTimes] OverBar[a]] == 
         a \[CircleTimes] OverBar[b]], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 5}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CircleTimes] (OverBar[a_] \[CircleTimes] (b_)) -> b, 
        "Side" -> 1, "Subpattern" -> OverBar[a_] \[CircleTimes] (b_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 9}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] OverBar[(b_) \[CircleTimes] (a_)] -> OverBar[b], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 12} -> 
     <|"Statement" -> HoldForm[
        OverBar[(c \[CircleTimes] OverBar[b]) \[CircleTimes] a] == 
         OverBar[a] \[CircleTimes] (b \[CircleTimes] OverBar[c])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 10}, 
        "Orientation" -> -1, "Rule" -> OverBar[a_] \[CircleTimes] 
           OverBar[b_] -> OverBar[b \[CircleTimes] a], "Side" -> 1, 
        "Subpattern" -> OverBar[b_], "MatchingConstruct" -> 
         {"CriticalPairLemma", 11}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> OverBar[(a_) \[CircleTimes] OverBar[b_]] -> 
          b \[CircleTimes] OverBar[a], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 10} -> 
     <|"Statement" -> HoldForm[OverBar[c \[CircleTimes] 
           (OverBar[b] \[CircleTimes] a)] == OverBar[a] \[CircleTimes] 
          (b \[CircleTimes] OverBar[c])], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 12}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {1}, "Rule" -> 
         ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          a \[CircleTimes] (b \[CircleTimes] c), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[OverBar[c \[CircleTimes] 
             (OverBar[b] \[CircleTimes] a)] == OverBar[a] \[CircleTimes] 
            (b \[CircleTimes] OverBar[c])], "Source" -> "cpl"|>|>, 
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
    {"SubstitutionLemma", 6} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] OverBar[b \[CircleTimes] 
            (OverBar[d \[CircleTimes] b] \[CircleTimes] a)] == d], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 2}, 
        "Construct" -> {"SubstitutionLemma", 5}, "Position" -> {2, 1, 2}, 
        "Rule" -> (a_) \[CircleTimes] (OverBar[a_] \[CircleTimes] (b_)) -> b, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a \[CircleTimes] OverBar[b \[CircleTimes] (OverBar[d \[CircleTimes] 
                 b] \[CircleTimes] a)] == d], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 11} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (OverBar[a] \[CircleTimes] 
           ((d \[CircleTimes] b) \[CircleTimes] OverBar[b])) == d], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 6}, 
        "Construct" -> {"SubstitutionLemma", 10}, "Position" -> {2}, 
        "Rule" -> OverBar[(a_) \[CircleTimes] (OverBar[b_] \[CircleTimes] 
             (c_))] -> OverBar[c] \[CircleTimes] (b \[CircleTimes] 
            OverBar[a]), "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[a \[CircleTimes] (OverBar[a] \[CircleTimes] 
             ((d \[CircleTimes] b) \[CircleTimes] OverBar[b])) == d], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 12} -> 
     <|"Statement" -> HoldForm[(d \[CircleTimes] b) \[CircleTimes] 
          OverBar[b] == d], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 11}, "Construct" -> 
         {"SubstitutionLemma", 5}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] (OverBar[a_] \[CircleTimes] (b_)) -> b, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (d \[CircleTimes] b) \[CircleTimes] OverBar[b] == d], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 13} -> 
     <|"Statement" -> HoldForm[d \[CircleTimes] (b \[CircleTimes] 
           OverBar[b]) == d], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 12}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {}, "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] 
           (c_) -> a \[CircleTimes] (b \[CircleTimes] c), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          d \[CircleTimes] (b \[CircleTimes] OverBar[b]) == d], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 14} -> 
     <|"Statement" -> HoldForm[d \[CircleTimes] OverTilde[1] == d], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 13}, 
        "Construct" -> {"Axiom", 3}, "Position" -> {2}, 
        "Rule" -> (a_) \[CircleTimes] OverBar[a_] -> OverTilde[1], 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          d \[CircleTimes] OverTilde[1] == d], "Source" -> "cpl"|>|>, 
    {"Conclusion", 1} -> <|"Statement" -> HoldForm[d == d], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 14}, 
        "Construct" -> {"Axiom", 2}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] OverTilde[1] -> a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[d == d], "Source" -> "cpl"|>|>}|>]
