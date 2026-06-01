ProofObject["EquationalLogic", Inactive[Equal][OverBar[OverBar[a]], a], 
 {Inactive[Equal][(((a_) \[CircleTimes] (b_)) \[CircleTimes] 
     (c_)) \[CircleTimes] OverBar[(a_) \[CircleTimes] (c_)], b_]}, 
 <|"Variables" -> {a, b, c, x3}, "Constants" -> {}, 
  "Proof" -> {{"Axiom", 1} -> <|"Statement" -> 
       HoldForm[((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] 
          OverBar[a \[CircleTimes] c] == b], "Proof" -> <||>|>, 
    {"Hypothesis", 1} -> <|"Statement" -> HoldForm[OverBar[OverBar[a]] == a], 
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
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 4} -> 
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
     <|"Statement" -> HoldForm[OverBar[a] == OverBar[OverBar[OverBar[a]]]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 6}, 
        "Orientation" -> 1, "Rule" -> OverBar[(a_) \[CircleTimes] 
            OverBar[(a_) \[CircleTimes] (b_)]] -> b, "Side" -> 1, 
        "Subpattern" -> (a_) \[CircleTimes] OverBar[(a_) \[CircleTimes] 
            (b_)], "MatchingConstruct" -> {"CriticalPairLemma", 4}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         OverBar[a_] \[CircleTimes] OverBar[(b_) \[CircleTimes] 
             OverBar[a_]] -> OverBar[b], "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 8} -> 
     <|"Statement" -> HoldForm[OverBar[b \[CircleTimes] 
           OverBar[b \[CircleTimes] a]] == OverBar[OverBar[a]]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 7}, 
        "Orientation" -> -1, "Rule" -> OverBar[OverBar[OverBar[a_]]] -> 
          OverBar[a], "Side" -> 1, "Subpattern" -> OverBar[a_], 
        "MatchingConstruct" -> {"CriticalPairLemma", 6}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         OverBar[(a_) \[CircleTimes] OverBar[(a_) \[CircleTimes] (b_)]] -> b, 
        "MatchingSide" -> 1, "Position" -> {1, 1}|>|>, 
    {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[a == OverBar[OverBar[a]]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 8}, 
        "Construct" -> {"CriticalPairLemma", 6}, "Position" -> {}, 
        "Rule" -> OverBar[(a_) \[CircleTimes] OverBar[(a_) \[CircleTimes] 
              (b_)]] -> b, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[a == OverBar[OverBar[a]]], "Source" -> "norm"|>|>, 
    {"Conclusion", 1} -> <|"Statement" -> HoldForm[a == a], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Construct" -> 
         {"SubstitutionLemma", 2}, "Position" -> {}, 
        "Rule" -> OverBar[OverBar[a_]] -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a == a], "Source" -> "cpl"|>|>}|>]
