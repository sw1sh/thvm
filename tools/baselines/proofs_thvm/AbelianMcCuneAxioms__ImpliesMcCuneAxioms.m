ProofObject["EquationalLogic", Inactive[Equal][
  a \[CircleTimes] OverBar[b \[CircleTimes] 
     (((c \[CircleTimes] OverBar[c]) \[CircleTimes] 
       OverBar[d \[CircleTimes] b]) \[CircleTimes] a)], d], 
 {Inactive[Equal][(((a_) \[CircleTimes] (b_)) \[CircleTimes] 
     (c_)) \[CircleTimes] OverBar[(a_) \[CircleTimes] (c_)], b_]}, 
 <|"Variables" -> {a, b, c, x3}, "Constants" -> {}, 
  "Proof" -> {{"Axiom", 1} -> <|"Statement" -> 
       HoldForm[((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] 
          OverBar[a \[CircleTimes] c] == b], "Proof" -> <||>|>, 
    {"Hypothesis", 1} -> <|"Statement" -> 
       HoldForm[a \[CircleTimes] OverBar[b \[CircleTimes] 
            (((c \[CircleTimes] OverBar[c]) \[CircleTimes] 
              OverBar[d \[CircleTimes] b]) \[CircleTimes] a)] == d], 
      "Proof" -> <||>|>, {"CriticalPairLemma", 1} -> 
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
        "Position" -> {1}|>|>, {"CriticalPairLemma", 6} -> 
     <|"Statement" -> HoldForm[OverBar[b \[CircleTimes] 
           OverBar[b \[CircleTimes] a]] == a], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 4}, 
        "Orientation" -> -1, "Rule" -> OverBar[a_] \[CircleTimes] 
           OverBar[(b_) \[CircleTimes] OverBar[a_]] -> OverBar[b], 
        "Side" -> 1, "Subpattern" -> {}, "MatchingConstruct" -> 
         {"CriticalPairLemma", 5}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CircleTimes] OverBar[
            ((b_) \[CircleTimes] (a_)) \[CircleTimes] 
             OverBar[(b_) \[CircleTimes] (c_)]] -> c, "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 7} -> 
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
    {"CriticalPairLemma", 8} -> 
     <|"Statement" -> HoldForm[OverBar[a] == OverBar[OverBar[OverBar[a]]]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 6}, 
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
        "MatchingConstruct" -> {"CriticalPairLemma", 6}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         OverBar[(a_) \[CircleTimes] OverBar[(a_) \[CircleTimes] (b_)]] -> b, 
        "MatchingSide" -> 1, "Position" -> {1, 1}|>|>, 
    {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[a == OverBar[OverBar[a]]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 9}, 
        "Construct" -> {"CriticalPairLemma", 6}, "Position" -> {}, 
        "Rule" -> OverBar[(a_) \[CircleTimes] OverBar[(a_) \[CircleTimes] 
              (b_)]] -> b, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[a == OverBar[OverBar[a]]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 3} -> 
     <|"Statement" -> HoldForm[OverBar[a \[CircleTimes] OverBar[b]] == 
         OverBar[a] \[CircleTimes] b], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 7}, "Construct" -> 
         {"SubstitutionLemma", 2}, "Position" -> {2}, 
        "Rule" -> OverBar[OverBar[a_]] -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a \[CircleTimes] 
             OverBar[b]] == OverBar[a] \[CircleTimes] b], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 4} -> 
     <|"Statement" -> HoldForm[OverBar[b] \[CircleTimes] (b \[CircleTimes] 
           a) == a], "Proof" -> <|"Input" -> {"CriticalPairLemma", 6}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Position" -> {}, 
        "Rule" -> OverBar[(a_) \[CircleTimes] OverBar[b_]] -> 
          OverBar[a] \[CircleTimes] b, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[OverBar[b] \[CircleTimes] 
            (b \[CircleTimes] a) == a], "Source" -> "cpl"|>|>, 
    {"CriticalPairLemma", 10} -> 
     <|"Statement" -> HoldForm[b == a \[CircleTimes] 
          (OverBar[a] \[CircleTimes] b)], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 4}, "Orientation" -> 1, 
        "Rule" -> OverBar[a_] \[CircleTimes] ((a_) \[CircleTimes] (b_)) -> b, 
        "Side" -> 1, "Subpattern" -> OverBar[a_], "MatchingConstruct" -> 
         {"SubstitutionLemma", 2}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> OverBar[OverBar[a_]] -> a, "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 5} -> 
     <|"Statement" -> HoldForm[c == a \[CircleTimes] 
          (OverBar[b \[CircleTimes] a] \[CircleTimes] (b \[CircleTimes] c))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 5}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Position" -> {2}, 
        "Rule" -> OverBar[(a_) \[CircleTimes] OverBar[b_]] -> 
          OverBar[a] \[CircleTimes] b, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[c == a \[CircleTimes] 
            (OverBar[b \[CircleTimes] a] \[CircleTimes] (b \[CircleTimes] 
              c))], "Source" -> "cpl"|>|>, {"CriticalPairLemma", 11} -> 
     <|"Statement" -> HoldForm[b \[CircleTimes] c == a \[CircleTimes] 
          (OverBar[OverBar[b] \[CircleTimes] a] \[CircleTimes] c)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 5}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CircleTimes] 
           (OverBar[(b_) \[CircleTimes] (a_)] \[CircleTimes] 
            ((b_) \[CircleTimes] (c_))) -> c, "Side" -> 1, 
        "Subpattern" -> (b_) \[CircleTimes] (c_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 4}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> OverBar[a_] \[CircleTimes] ((a_) \[CircleTimes] 
            (b_)) -> b, "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"CriticalPairLemma", 12} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] OverBar[b] == 
         OverBar[OverBar[a] \[CircleTimes] b]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 2}, 
        "Orientation" -> -1, "Rule" -> OverBar[OverBar[a_]] -> a, 
        "Side" -> 1, "Subpattern" -> OverBar[a_], "MatchingConstruct" -> 
         {"SubstitutionLemma", 3}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> OverBar[(a_) \[CircleTimes] OverBar[b_]] -> 
          OverBar[a] \[CircleTimes] b, "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 6} -> 
     <|"Statement" -> HoldForm[b \[CircleTimes] c == a \[CircleTimes] 
          ((b \[CircleTimes] OverBar[a]) \[CircleTimes] c)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 11}, 
        "Construct" -> {"CriticalPairLemma", 12}, "Position" -> {2, 1}, 
        "Rule" -> OverBar[OverBar[a_] \[CircleTimes] (b_)] -> 
          a \[CircleTimes] OverBar[b], "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[b \[CircleTimes] c == 
           a \[CircleTimes] ((b \[CircleTimes] OverBar[a]) \[CircleTimes] 
             c)], "Source" -> "cpl"|>|>, {"CriticalPairLemma", 13} -> 
     <|"Statement" -> HoldForm[
        (b \[CircleTimes] OverBar[OverBar[a]]) \[CircleTimes] c == 
         a \[CircleTimes] (b \[CircleTimes] c)], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 10}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CircleTimes] 
           (OverBar[a_] \[CircleTimes] (b_)) -> b, "Side" -> 1, 
        "Subpattern" -> OverBar[a_] \[CircleTimes] (b_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 6}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CircleTimes] (((b_) \[CircleTimes] OverBar[
              a_]) \[CircleTimes] (c_)) -> b \[CircleTimes] c, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 7} -> 
     <|"Statement" -> HoldForm[(b \[CircleTimes] a) \[CircleTimes] c == 
         a \[CircleTimes] (b \[CircleTimes] c)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 13}, 
        "Construct" -> {"SubstitutionLemma", 2}, "Position" -> {1, 2}, 
        "Rule" -> OverBar[OverBar[a_]] -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(b \[CircleTimes] a) \[CircleTimes] 
            c == a \[CircleTimes] (b \[CircleTimes] c)], 
        "Source" -> "cpl"|>|>, {"CriticalPairLemma", 14} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] (b \[CircleTimes] 
           OverBar[c]) == OverBar[a \[CircleTimes] (OverBar[b] \[CircleTimes] 
            c)]], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 3}, 
        "Orientation" -> 1, "Rule" -> OverBar[(a_) \[CircleTimes] 
            OverBar[b_]] -> OverBar[a] \[CircleTimes] b, "Side" -> 1, 
        "Subpattern" -> OverBar[b_], "MatchingConstruct" -> 
         {"SubstitutionLemma", 3}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> OverBar[(a_) \[CircleTimes] OverBar[b_]] -> 
          OverBar[a] \[CircleTimes] b, "MatchingSide" -> 1, 
        "Position" -> {1, 2}|>|>, {"CriticalPairLemma", 15} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CircleTimes] OverBar[b] == 
         OverBar[a \[CircleTimes] b]], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 3}, "Orientation" -> 1, 
        "Rule" -> OverBar[(a_) \[CircleTimes] OverBar[b_]] -> 
          OverBar[a] \[CircleTimes] b, "Side" -> 1, "Subpattern" -> 
         OverBar[b_], "MatchingConstruct" -> {"SubstitutionLemma", 2}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         OverBar[OverBar[a_]] -> a, "MatchingSide" -> 1, 
        "Position" -> {1, 2}|>|>, {"CriticalPairLemma", 16} -> 
     <|"Statement" -> HoldForm[OverBar[b] == 
         OverBar[OverBar[a]] \[CircleTimes] OverBar[a \[CircleTimes] b]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 4}, 
        "Orientation" -> 1, "Rule" -> OverBar[a_] \[CircleTimes] 
           ((a_) \[CircleTimes] (b_)) -> b, "Side" -> 1, 
        "Subpattern" -> (a_) \[CircleTimes] (b_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 15}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> OverBar[a_] \[CircleTimes] OverBar[b_] -> 
          OverBar[a \[CircleTimes] b], "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 14} -> 
     <|"Statement" -> HoldForm[OverBar[b] == a \[CircleTimes] 
          OverBar[a \[CircleTimes] b]], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 16}, "Construct" -> 
         {"SubstitutionLemma", 2}, "Position" -> {1}, 
        "Rule" -> OverBar[OverBar[a_]] -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[b] == a \[CircleTimes] 
            OverBar[a \[CircleTimes] b]], "Source" -> "cpl"|>|>, 
    {"CriticalPairLemma", 17} -> 
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
        "Position" -> {1}|>|>, {"SubstitutionLemma", 16} -> 
     <|"Statement" -> HoldForm[
        OverBar[b \[CircleTimes] OverBar[a]] \[CircleTimes] b == a], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 17}, 
        "Construct" -> {"SubstitutionLemma", 2}, "Position" -> {}, 
        "Rule" -> OverBar[OverBar[a_]] -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[
          OverBar[b \[CircleTimes] OverBar[a]] \[CircleTimes] b == a], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 17} -> 
     <|"Statement" -> HoldForm[(OverBar[b] \[CircleTimes] a) \[CircleTimes] 
          b == a], "Proof" -> <|"Input" -> {"SubstitutionLemma", 16}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Position" -> {1}, 
        "Rule" -> OverBar[(a_) \[CircleTimes] OverBar[b_]] -> 
          OverBar[a] \[CircleTimes] b, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(OverBar[b] \[CircleTimes] 
             a) \[CircleTimes] b == a], "Source" -> "cpl"|>|>, 
    {"CriticalPairLemma", 18} -> 
     <|"Statement" -> HoldForm[b \[CircleTimes] a == a \[CircleTimes] b], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 17}, 
        "Orientation" -> 1, "Rule" -> (OverBar[a_] \[CircleTimes] 
            (b_)) \[CircleTimes] (a_) -> b, "Side" -> 1, 
        "Subpattern" -> OverBar[a_] \[CircleTimes] (b_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 4}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         OverBar[a_] \[CircleTimes] ((a_) \[CircleTimes] (b_)) -> b, 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 19} -> 
     <|"Statement" -> HoldForm[b == a \[CircleTimes] (b \[CircleTimes] 
           OverBar[a])], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 
          10}, "Orientation" -> -1, "Rule" -> 
         (a_) \[CircleTimes] (OverBar[a_] \[CircleTimes] (b_)) -> b, 
        "Side" -> 1, "Subpattern" -> OverBar[a_] \[CircleTimes] (b_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 18}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] (b_) -> b \[CircleTimes] a, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 8} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] OverBar[b \[CircleTimes] 
            (OverBar[d \[CircleTimes] b] \[CircleTimes] 
             ((c \[CircleTimes] OverBar[c]) \[CircleTimes] a))] == d], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Construct" -> 
         {"SubstitutionLemma", 7}, "Position" -> {2, 1, 2}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CircleTimes] 
            OverBar[b \[CircleTimes] (OverBar[d \[CircleTimes] 
                 b] \[CircleTimes] ((c \[CircleTimes] OverBar[
                  c]) \[CircleTimes] a))] == d], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 9} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (OverBar[b] \[CircleTimes] 
           ((d \[CircleTimes] b) \[CircleTimes] OverBar[
             (c \[CircleTimes] OverBar[c]) \[CircleTimes] a])) == d], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 8}, 
        "Construct" -> {"CriticalPairLemma", 14}, "Position" -> {2}, 
        "Rule" -> OverBar[(a_) \[CircleTimes] (OverBar[b_] \[CircleTimes] 
             (c_))] -> OverBar[a] \[CircleTimes] (b \[CircleTimes] 
            OverBar[c]), "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[a \[CircleTimes] (OverBar[b] \[CircleTimes] 
             ((d \[CircleTimes] b) \[CircleTimes] OverBar[(c \[CircleTimes] 
                 OverBar[c]) \[CircleTimes] a])) == d], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 10} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (OverBar[b] \[CircleTimes] 
           (b \[CircleTimes] (d \[CircleTimes] OverBar[(c \[CircleTimes] 
                OverBar[c]) \[CircleTimes] a]))) == d], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 9}, 
        "Construct" -> {"SubstitutionLemma", 7}, "Position" -> {2, 2}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CircleTimes] 
            (OverBar[b] \[CircleTimes] (b \[CircleTimes] (d \[CircleTimes] 
               OverBar[(c \[CircleTimes] OverBar[c]) \[CircleTimes] a]))) == 
           d], "Source" -> "cpl"|>|>, {"SubstitutionLemma", 11} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (d \[CircleTimes] 
           OverBar[(c \[CircleTimes] OverBar[c]) \[CircleTimes] a]) == d], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 10}, 
        "Construct" -> {"SubstitutionLemma", 4}, "Position" -> {2}, 
        "Rule" -> OverBar[a_] \[CircleTimes] ((a_) \[CircleTimes] (b_)) -> b, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a \[CircleTimes] (d \[CircleTimes] OverBar[(c \[CircleTimes] 
                OverBar[c]) \[CircleTimes] a]) == d], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 12} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (d \[CircleTimes] 
           OverBar[OverBar[c] \[CircleTimes] (c \[CircleTimes] a)]) == d], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 11}, 
        "Construct" -> {"SubstitutionLemma", 7}, "Position" -> {2, 2, 1}, 
        "Rule" -> ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_) -> 
          b \[CircleTimes] (a \[CircleTimes] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CircleTimes] (d \[CircleTimes] 
             OverBar[OverBar[c] \[CircleTimes] (c \[CircleTimes] a)]) == d], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 13} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (d \[CircleTimes] 
           (c \[CircleTimes] OverBar[c \[CircleTimes] a])) == d], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 12}, 
        "Construct" -> {"CriticalPairLemma", 12}, "Position" -> {2, 2}, 
        "Rule" -> OverBar[OverBar[a_] \[CircleTimes] (b_)] -> 
          a \[CircleTimes] OverBar[b], "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CircleTimes] (d \[CircleTimes] 
             (c \[CircleTimes] OverBar[c \[CircleTimes] a])) == d], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 15} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (d \[CircleTimes] 
           OverBar[a]) == d], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 13}, "Construct" -> 
         {"SubstitutionLemma", 14}, "Position" -> {2, 2}, 
        "Rule" -> (a_) \[CircleTimes] OverBar[(a_) \[CircleTimes] (b_)] -> 
          OverBar[b], "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[a \[CircleTimes] (d \[CircleTimes] OverBar[a]) == d], 
        "Source" -> "cpl"|>|>, {"Conclusion", 1} -> 
     <|"Statement" -> HoldForm[d == d], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 15}, "Construct" -> 
         {"CriticalPairLemma", 19}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] OverBar[a_]) -> b, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[d == d], 
        "Source" -> "cpl"|>|>}|>]
