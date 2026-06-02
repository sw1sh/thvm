ProofObject["EquationalLogic", Inactive[Equal][OverBar[a \[CircleTimes] b], 
  OverBar[b] \[CircleTimes] OverBar[a]], 
 {Inactive[Equal][(((a_) \[CircleTimes] (b_)) \[CircleTimes] 
     (c_)) \[CircleTimes] OverBar[(a_) \[CircleTimes] (c_)], b_]}, 
 <|"Variables" -> {a, b, c, x3}, "Constants" -> {}, 
  "Proof" -> {{"Axiom", 1} -> <|"Statement" -> 
       HoldForm[((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] 
          OverBar[a \[CircleTimes] c] == b], "Proof" -> <||>|>, 
    {"Hypothesis", 1} -> <|"Statement" -> 
       HoldForm[OverBar[a \[CircleTimes] b] == OverBar[b] \[CircleTimes] 
          OverBar[a]], "Proof" -> <||>|>, {"CriticalPairLemma", 1} -> 
     <|"Statement" -> HoldForm[OverBar[c \[CircleTimes] x3] == 
         (a \[CircleTimes] b) \[CircleTimes] OverBar[
           ((c \[CircleTimes] a) \[CircleTimes] x3) \[CircleTimes] b]], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
        "Rule" -> (((a_) \[CircleTimes] (b_)) \[CircleTimes] 
            (c_)) \[CircleTimes] OverBar[(a_) \[CircleTimes] (c_)] -> b, 
        "Side" -> 1, "Subpattern" -> (a_) \[CircleTimes] (b_), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (((a_) \[CircleTimes] (b_)) \[CircleTimes] 
            (c_)) \[CircleTimes] OverBar[(a_) \[CircleTimes] (c_)] -> b, 
        "MatchingSide" -> 1, "Position" -> {1, 1}|>|>, 
    {"CriticalPairLemma", 2} -> 
     <|"Statement" -> HoldForm[OverBar[b \[CircleTimes] c] == 
         (a \[CircleTimes] OverBar[b \[CircleTimes] c]) \[CircleTimes] 
          OverBar[a]], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 1}, 
        "Orientation" -> -1, "Rule" -> 
         ((a_) \[CircleTimes] (b_)) \[CircleTimes] 
           OverBar[(((c_) \[CircleTimes] (a_)) \[CircleTimes] 
              (x3_)) \[CircleTimes] (b_)] -> OverBar[c \[CircleTimes] x3], 
        "Side" -> 1, "Subpattern" -> (((c_) \[CircleTimes] 
            (a_)) \[CircleTimes] (x3_)) \[CircleTimes] (b_), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (((a_) \[CircleTimes] (b_)) \[CircleTimes] 
            (c_)) \[CircleTimes] OverBar[(a_) \[CircleTimes] (c_)] -> b, 
        "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"CriticalPairLemma", 3} -> 
     <|"Statement" -> HoldForm[OverBar[((c \[CircleTimes] b) \[CircleTimes] 
            x3) \[CircleTimes] OverBar[c \[CircleTimes] x3]] == 
         (a \[CircleTimes] OverBar[b]) \[CircleTimes] OverBar[a]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 2}, 
        "Orientation" -> -1, "Rule" -> 
         ((a_) \[CircleTimes] OverBar[(b_) \[CircleTimes] 
              (c_)]) \[CircleTimes] OverBar[a_] -> 
          OverBar[b \[CircleTimes] c], "Side" -> 1, "Subpattern" -> 
         (b_) \[CircleTimes] (c_), "MatchingConstruct" -> {"Axiom", 1}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_)) \[CircleTimes] 
           OverBar[(a_) \[CircleTimes] (c_)] -> b, "MatchingSide" -> 1, 
        "Position" -> {1, 2, 1}|>|>, {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[OverBar[b] == 
         (a \[CircleTimes] OverBar[b]) \[CircleTimes] OverBar[a]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 3}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {1}, 
        "Rule" -> (((a_) \[CircleTimes] (b_)) \[CircleTimes] 
            (c_)) \[CircleTimes] OverBar[(a_) \[CircleTimes] (c_)] -> b, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[OverBar[b] == 
           (a \[CircleTimes] OverBar[b]) \[CircleTimes] OverBar[a]], 
        "Source" -> "cpl"|>|>, {"CriticalPairLemma", 4} -> 
     <|"Statement" -> HoldForm[OverBar[b] == OverBar[a] \[CircleTimes] 
          OverBar[b \[CircleTimes] OverBar[a]]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 1}, 
        "Orientation" -> -1, "Rule" -> 
         ((a_) \[CircleTimes] OverBar[b_]) \[CircleTimes] OverBar[a_] -> 
          OverBar[b], "Side" -> 1, "Subpattern" -> (a_) \[CircleTimes] 
          OverBar[b_], "MatchingConstruct" -> {"SubstitutionLemma", 1}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CircleTimes] OverBar[b_]) \[CircleTimes] OverBar[a_] -> 
          OverBar[b], "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 5} -> 
     <|"Statement" -> HoldForm[OverBar[a \[CircleTimes] OverBar[b]] == 
         OverBar[a] \[CircleTimes] OverBar[OverBar[b]]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 4}, 
        "Orientation" -> -1, "Rule" -> OverBar[a_] \[CircleTimes] 
           OverBar[(b_) \[CircleTimes] OverBar[a_]] -> OverBar[b], 
        "Side" -> 1, "Subpattern" -> (b_) \[CircleTimes] OverBar[a_], 
        "MatchingConstruct" -> {"SubstitutionLemma", 1}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CircleTimes] OverBar[b_]) \[CircleTimes] OverBar[a_] -> 
          OverBar[b], "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"CriticalPairLemma", 6} -> 
     <|"Statement" -> HoldForm[c == a \[CircleTimes] 
          OverBar[(b \[CircleTimes] a) \[CircleTimes] 
            OverBar[b \[CircleTimes] c]]], "Proof" -> 
       <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
        "Rule" -> (((a_) \[CircleTimes] (b_)) \[CircleTimes] 
            (c_)) \[CircleTimes] OverBar[(a_) \[CircleTimes] (c_)] -> b, 
        "Side" -> 1, "Subpattern" -> ((a_) \[CircleTimes] 
           (b_)) \[CircleTimes] (c_), "MatchingConstruct" -> {"Axiom", 1}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_)) \[CircleTimes] 
           OverBar[(a_) \[CircleTimes] (c_)] -> b, "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 7} -> 
     <|"Statement" -> HoldForm[OverBar[b \[CircleTimes] 
           OverBar[b \[CircleTimes] a]] == a], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 4}, 
        "Orientation" -> -1, "Rule" -> OverBar[a_] \[CircleTimes] 
           OverBar[(b_) \[CircleTimes] OverBar[a_]] -> OverBar[b], 
        "Side" -> 1, "Subpattern" -> {}, "MatchingConstruct" -> 
         {"CriticalPairLemma", 6}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CircleTimes] OverBar[
            ((b_) \[CircleTimes] (a_)) \[CircleTimes] 
             OverBar[(b_) \[CircleTimes] (c_)]] -> c, "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 8} -> 
     <|"Statement" -> HoldForm[OverBar[a] == OverBar[OverBar[OverBar[a]]]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 7}, 
        "Orientation" -> 1, "Rule" -> OverBar[(a_) \[CircleTimes] 
            OverBar[(a_) \[CircleTimes] (b_)]] -> b, "Side" -> 1, 
        "Subpattern" -> (a_) \[CircleTimes] OverBar[(a_) \[CircleTimes] 
            (b_)], "MatchingConstruct" -> {"CriticalPairLemma", 4}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         OverBar[a_] \[CircleTimes] OverBar[(b_) \[CircleTimes] 
             OverBar[a_]] -> OverBar[b], "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 9} -> 
     <|"Statement" -> HoldForm[OverBar[b \[CircleTimes] 
           OverBar[b \[CircleTimes] a]] == OverBar[OverBar[a]]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 8}, 
        "Orientation" -> -1, "Rule" -> OverBar[OverBar[OverBar[a_]]] -> 
          OverBar[a], "Side" -> 1, "Subpattern" -> OverBar[a_], 
        "MatchingConstruct" -> {"CriticalPairLemma", 7}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         OverBar[(a_) \[CircleTimes] OverBar[(a_) \[CircleTimes] (b_)]] -> b, 
        "MatchingSide" -> 1, "Position" -> {1, 1}|>|>, 
    {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[a == OverBar[OverBar[a]]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 9}, 
        "Construct" -> {"CriticalPairLemma", 7}, "Position" -> {}, 
        "Rule" -> OverBar[(a_) \[CircleTimes] OverBar[(a_) \[CircleTimes] 
              (b_)]] -> b, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[a == OverBar[OverBar[a]]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 3} -> 
     <|"Statement" -> HoldForm[OverBar[a \[CircleTimes] OverBar[b]] == 
         OverBar[a] \[CircleTimes] b], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 5}, "Construct" -> 
         {"SubstitutionLemma", 2}, "Position" -> {2}, 
        "Rule" -> OverBar[OverBar[a_]] -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a \[CircleTimes] 
             OverBar[b]] == OverBar[a] \[CircleTimes] b], 
        "Source" -> "cpl"|>|>, {"CriticalPairLemma", 10} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] OverBar[b] == 
         OverBar[a \[CircleTimes] b]], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 3}, "Orientation" -> 1, 
        "Rule" -> OverBar[(a_) \[CircleTimes] OverBar[b_]] -> 
          OverBar[a] \[CircleTimes] b, "Side" -> 1, "Subpattern" -> 
         OverBar[b_], "MatchingConstruct" -> {"SubstitutionLemma", 2}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         OverBar[OverBar[a_]] -> a, "MatchingSide" -> 1, 
        "Position" -> {1, 2}|>|>, {"CriticalPairLemma", 11} -> 
     <|"Statement" -> HoldForm[
        OverBar[b \[CircleTimes] OverBar[a]] \[CircleTimes] b == 
         OverBar[OverBar[a]]], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 3}, "Orientation" -> 1, 
        "Rule" -> OverBar[(a_) \[CircleTimes] OverBar[b_]] -> 
          OverBar[a] \[CircleTimes] b, "Side" -> 1, "Subpattern" -> 
         (a_) \[CircleTimes] OverBar[b_], "MatchingConstruct" -> 
         {"SubstitutionLemma", 1}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CircleTimes] OverBar[b_]) \[CircleTimes] 
           OverBar[a_] -> OverBar[b], "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 5} -> 
     <|"Statement" -> HoldForm[
        OverBar[b \[CircleTimes] OverBar[a]] \[CircleTimes] b == a], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 11}, 
        "Construct" -> {"SubstitutionLemma", 2}, "Position" -> {}, 
        "Rule" -> OverBar[OverBar[a_]] -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[
          OverBar[b \[CircleTimes] OverBar[a]] \[CircleTimes] b == a], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 6} -> 
     <|"Statement" -> HoldForm[(OverBar[b] \[CircleTimes] a) \[CircleTimes] 
          b == a], "Proof" -> <|"Input" -> {"SubstitutionLemma", 5}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Position" -> {1}, 
        "Rule" -> OverBar[(a_) \[CircleTimes] OverBar[b_]] -> 
          OverBar[a] \[CircleTimes] b, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(OverBar[b] \[CircleTimes] 
             a) \[CircleTimes] b == a], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 7} -> 
     <|"Statement" -> HoldForm[OverBar[b] \[CircleTimes] (b \[CircleTimes] 
           a) == a], "Proof" -> <|"Input" -> {"CriticalPairLemma", 7}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Position" -> {}, 
        "Rule" -> OverBar[(a_) \[CircleTimes] OverBar[b_]] -> 
          OverBar[a] \[CircleTimes] b, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[OverBar[b] \[CircleTimes] 
            (b \[CircleTimes] a) == a], "Source" -> "cpl"|>|>, 
    {"CriticalPairLemma", 12} -> 
     <|"Statement" -> HoldForm[b \[CircleTimes] a == a \[CircleTimes] b], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 6}, 
        "Orientation" -> 1, "Rule" -> (OverBar[a_] \[CircleTimes] 
            (b_)) \[CircleTimes] (a_) -> b, "Side" -> 1, 
        "Subpattern" -> OverBar[a_] \[CircleTimes] (b_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 7}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         OverBar[a_] \[CircleTimes] ((a_) \[CircleTimes] (b_)) -> b, 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"SubstitutionLemma", 4} -> 
     <|"Statement" -> HoldForm[OverBar[a \[CircleTimes] b] == 
         OverBar[b \[CircleTimes] a]], "Proof" -> 
       <|"Input" -> {"Hypothesis", 1}, "Construct" -> {"CriticalPairLemma", 
          10}, "Position" -> {}, "Rule" -> OverBar[a_] \[CircleTimes] 
           OverBar[b_] -> OverBar[a \[CircleTimes] b], "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a \[CircleTimes] b] == 
           OverBar[b \[CircleTimes] a]], "Source" -> "cpl"|>|>, 
    {"Conclusion", 1} -> <|"Statement" -> 
       HoldForm[OverBar[a \[CircleTimes] b] == OverBar[a \[CircleTimes] b]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 4}, 
        "Construct" -> {"CriticalPairLemma", 12}, "Position" -> {1}, 
        "Rule" -> (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a \[CircleTimes] b] == OverBar[a \[CircleTimes] b]], 
        "Source" -> "cpl"|>|>}|>]
