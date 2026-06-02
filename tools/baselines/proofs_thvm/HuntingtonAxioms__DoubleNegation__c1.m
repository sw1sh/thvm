ProofObject["EquationalLogic", Inactive[Equal][OverBar[OverBar[a]], a], 
 {Inactive[Equal][(a_) \[CirclePlus] ((b_) \[CirclePlus] (c_)), 
   ((a_) \[CirclePlus] (b_)) \[CirclePlus] (c_)], 
  Inactive[Equal][(a_) \[CirclePlus] (b_), (b_) \[CirclePlus] (a_)], 
  Inactive[Equal][OverBar[OverBar[a_] \[CirclePlus] (b_)] \[CirclePlus] 
    OverBar[OverBar[a_] \[CirclePlus] OverBar[b_]], a_]}, 
 <|"Variables" -> {a, b, c, x3}, "Constants" -> {}, 
  "Proof" -> {{"Axiom", 1} -> <|"Statement" -> 
       HoldForm[a \[CirclePlus] (b \[CirclePlus] c) == 
         (a \[CirclePlus] b) \[CirclePlus] c], "Proof" -> <||>|>, 
    {"Axiom", 2} -> <|"Statement" -> HoldForm[a \[CirclePlus] b == 
         b \[CirclePlus] a], "Proof" -> <||>|>, 
    {"Axiom", 3} -> <|"Statement" -> HoldForm[
        OverBar[OverBar[a] \[CirclePlus] b] \[CirclePlus] 
          OverBar[OverBar[a] \[CirclePlus] OverBar[b]] == a], 
      "Proof" -> <||>|>, {"Hypothesis", 1} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[a]] == a], "Proof" -> <||>|>, 
    {"CriticalPairLemma", 1} -> 
     <|"Statement" -> HoldForm[b == OverBar[a \[CirclePlus] 
            OverBar[b]] \[CirclePlus] OverBar[OverBar[b] \[CirclePlus] 
            OverBar[a]]], "Proof" -> <|"Construct" -> {"Axiom", 3}, 
        "Orientation" -> 1, "Rule" -> 
         OverBar[OverBar[a_] \[CirclePlus] (b_)] \[CirclePlus] 
           OverBar[OverBar[a_] \[CirclePlus] OverBar[b_]] -> a, "Side" -> 1, 
        "Subpattern" -> OverBar[a_] \[CirclePlus] (b_), 
        "MatchingConstruct" -> {"Axiom", 2}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, 
        "MatchingSide" -> 1, "Position" -> {1, 1}|>|>, 
    {"CriticalPairLemma", 2} -> 
     <|"Statement" -> HoldForm[a == OverBar[OverBar[a] \[CirclePlus] 
            b] \[CirclePlus] OverBar[OverBar[b] \[CirclePlus] OverBar[a]]], 
      "Proof" -> <|"Construct" -> {"Axiom", 3}, "Orientation" -> 1, 
        "Rule" -> OverBar[OverBar[a_] \[CirclePlus] (b_)] \[CirclePlus] 
           OverBar[OverBar[a_] \[CirclePlus] OverBar[b_]] -> a, "Side" -> 1, 
        "Subpattern" -> OverBar[a_] \[CirclePlus] OverBar[b_], 
        "MatchingConstruct" -> {"Axiom", 2}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, 
        "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"CriticalPairLemma", 3} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] OverBar[b] == 
         OverBar[OverBar[OverBar[a] \[CirclePlus] OverBar[b]] \[CirclePlus] 
            (OverBar[a] \[CirclePlus] b)] \[CirclePlus] OverBar[a]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 2}, 
        "Orientation" -> -1, "Rule" -> 
         OverBar[OverBar[a_] \[CirclePlus] (b_)] \[CirclePlus] 
           OverBar[OverBar[b_] \[CirclePlus] OverBar[a_]] -> a, "Side" -> 1, 
        "Subpattern" -> OverBar[b_] \[CirclePlus] OverBar[a_], 
        "MatchingConstruct" -> {"Axiom", 3}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> OverBar[OverBar[a_] \[CirclePlus] 
             (b_)] \[CirclePlus] OverBar[OverBar[a_] \[CirclePlus] 
             OverBar[b_]] -> a, "MatchingSide" -> 1, "Position" -> {2, 
         1}|>|>, {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] OverBar[b] == 
         OverBar[a] \[CirclePlus] OverBar[OverBar[OverBar[a] \[CirclePlus] 
              OverBar[b]] \[CirclePlus] (OverBar[a] \[CirclePlus] b)]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 3}, 
        "Construct" -> {"Axiom", 2}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a] \[CirclePlus] OverBar[b] == OverBar[a] \[CirclePlus] 
            OverBar[OverBar[OverBar[a] \[CirclePlus] OverBar[
                 b]] \[CirclePlus] (OverBar[a] \[CirclePlus] b)]], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] OverBar[b] == 
         OverBar[a] \[CirclePlus] OverBar[(OverBar[a] \[CirclePlus] 
             b) \[CirclePlus] OverBar[OverBar[a] \[CirclePlus] OverBar[b]]]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 1}, 
        "Construct" -> {"Axiom", 2}, "Position" -> {2, 1}, 
        "Rule" -> (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a] \[CirclePlus] OverBar[b] == OverBar[a] \[CirclePlus] 
            OverBar[(OverBar[a] \[CirclePlus] b) \[CirclePlus] 
              OverBar[OverBar[a] \[CirclePlus] OverBar[b]]]], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 3} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] OverBar[b] == 
         OverBar[a] \[CirclePlus] OverBar[OverBar[a] \[CirclePlus] 
            (b \[CirclePlus] OverBar[OverBar[a] \[CirclePlus] OverBar[b]])]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 2}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {2, 1}, 
        "Rule" -> ((a_) \[CirclePlus] (b_)) \[CirclePlus] (c_) -> 
          a \[CirclePlus] (b \[CirclePlus] c), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CirclePlus] OverBar[b] == 
           OverBar[a] \[CirclePlus] OverBar[OverBar[a] \[CirclePlus] 
              (b \[CirclePlus] OverBar[OverBar[a] \[CirclePlus] OverBar[
                  b]])]], "Source" -> "norm"|>|>, {"CriticalPairLemma", 4} -> 
     <|"Statement" -> HoldForm[b \[CirclePlus] (a \[CirclePlus] c) == 
         (a \[CirclePlus] b) \[CirclePlus] c], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> ((a_) \[CirclePlus] (b_)) \[CirclePlus] (c_) -> 
          a \[CirclePlus] (b \[CirclePlus] c), "Side" -> 1, 
        "Subpattern" -> (a_) \[CirclePlus] (b_), "MatchingConstruct" -> 
         {"Axiom", 2}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 4} -> 
     <|"Statement" -> HoldForm[b \[CirclePlus] (a \[CirclePlus] c) == 
         a \[CirclePlus] (b \[CirclePlus] c)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 4}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> ((a_) \[CirclePlus] (b_)) \[CirclePlus] (c_) -> 
          a \[CirclePlus] (b \[CirclePlus] c), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[b \[CirclePlus] (a \[CirclePlus] c) == 
           a \[CirclePlus] (b \[CirclePlus] c)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 5} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[a] \[CirclePlus] 
            c] \[CirclePlus] (OverBar[OverBar[a] \[CirclePlus] 
             OverBar[c]] \[CirclePlus] b) == a \[CirclePlus] b], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> ((a_) \[CirclePlus] (b_)) \[CirclePlus] (c_) -> 
          a \[CirclePlus] (b \[CirclePlus] c), "Side" -> 1, 
        "Subpattern" -> (a_) \[CirclePlus] (b_), "MatchingConstruct" -> 
         {"Axiom", 3}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         OverBar[OverBar[a_] \[CirclePlus] (b_)] \[CirclePlus] 
           OverBar[OverBar[a_] \[CirclePlus] OverBar[b_]] -> a, 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 6} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] OverBar[
           OverBar[a] \[CirclePlus] OverBar[OverBar[b]]] == 
         OverBar[OverBar[a] \[CirclePlus] b] \[CirclePlus] a], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 5}, 
        "Orientation" -> 1, "Rule" -> 
         OverBar[OverBar[a_] \[CirclePlus] (b_)] \[CirclePlus] 
           (OverBar[OverBar[a_] \[CirclePlus] OverBar[b_]] \[CirclePlus] 
            (c_)) -> a \[CirclePlus] c, "Side" -> 1, "Subpattern" -> 
         OverBar[OverBar[a_] \[CirclePlus] OverBar[b_]] \[CirclePlus] (c_), 
        "MatchingConstruct" -> {"Axiom", 3}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> OverBar[OverBar[a_] \[CirclePlus] 
             (b_)] \[CirclePlus] OverBar[OverBar[a_] \[CirclePlus] 
             OverBar[b_]] -> a, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 5} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] OverBar[
           OverBar[a] \[CirclePlus] OverBar[OverBar[b]]] == 
         a \[CirclePlus] OverBar[OverBar[a] \[CirclePlus] b]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 6}, 
        "Construct" -> {"Axiom", 2}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CirclePlus] OverBar[OverBar[a] \[CirclePlus] OverBar[OverBar[
                b]]] == a \[CirclePlus] OverBar[OverBar[a] \[CirclePlus] b]], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 7} -> 
     <|"Statement" -> HoldForm[b \[CirclePlus] (a \[CirclePlus] 
           OverBar[OverBar[b] \[CirclePlus] OverBar[OverBar[c]]]) == 
         a \[CirclePlus] (b \[CirclePlus] OverBar[OverBar[b] \[CirclePlus] 
             c])], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 4}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CirclePlus] ((b_) \[CirclePlus] 
            (c_)) -> b \[CirclePlus] (a \[CirclePlus] c), "Side" -> 1, 
        "Subpattern" -> (b_) \[CirclePlus] (c_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 5}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CirclePlus] OverBar[
            OverBar[a_] \[CirclePlus] OverBar[OverBar[b_]]] -> 
          a \[CirclePlus] OverBar[OverBar[a] \[CirclePlus] b], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 8} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[OverBar[b]] \[CirclePlus] 
            a] \[CirclePlus] (a \[CirclePlus] OverBar[
            OverBar[a] \[CirclePlus] b]) == a \[CirclePlus] OverBar[b]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 7}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CirclePlus] ((b_) \[CirclePlus] 
            OverBar[OverBar[a_] \[CirclePlus] OverBar[OverBar[c_]]]) -> 
          b \[CirclePlus] (a \[CirclePlus] OverBar[OverBar[a] \[CirclePlus] 
              c]), "Side" -> 1, "Subpattern" -> (b_) \[CirclePlus] 
          OverBar[OverBar[a_] \[CirclePlus] OverBar[OverBar[c_]]], 
        "MatchingConstruct" -> {"CriticalPairLemma", 2}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         OverBar[OverBar[a_] \[CirclePlus] (b_)] \[CirclePlus] 
           OverBar[OverBar[b_] \[CirclePlus] OverBar[a_]] -> a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 9} -> 
     <|"Statement" -> HoldForm[c \[CirclePlus] (a \[CirclePlus] b) == 
         a \[CirclePlus] (b \[CirclePlus] c)], 
      "Proof" -> <|"Construct" -> {"Axiom", 2}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, "Side" -> 1, 
        "Subpattern" -> {}, "MatchingConstruct" -> {"Axiom", 1}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CirclePlus] (b_)) \[CirclePlus] (c_) -> 
          a \[CirclePlus] (b \[CirclePlus] c), "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"SubstitutionLemma", 6} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[a] \[CirclePlus] 
            b] \[CirclePlus] (OverBar[OverBar[OverBar[b]] \[CirclePlus] 
             a] \[CirclePlus] a) == a \[CirclePlus] OverBar[b]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 8}, 
        "Construct" -> {"CriticalPairLemma", 9}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] ((b_) \[CirclePlus] (c_)) -> 
          c \[CirclePlus] (a \[CirclePlus] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          OverBar[OverBar[a] \[CirclePlus] b] \[CirclePlus] 
            (OverBar[OverBar[OverBar[b]] \[CirclePlus] a] \[CirclePlus] a) == 
           a \[CirclePlus] OverBar[b]], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 7} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] 
          (OverBar[OverBar[a] \[CirclePlus] b] \[CirclePlus] 
           OverBar[OverBar[OverBar[b]] \[CirclePlus] a]) == 
         a \[CirclePlus] OverBar[b]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 6}, "Construct" -> 
         {"CriticalPairLemma", 9}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] ((b_) \[CirclePlus] (c_)) -> 
          c \[CirclePlus] (a \[CirclePlus] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CirclePlus] 
            (OverBar[OverBar[a] \[CirclePlus] b] \[CirclePlus] 
             OverBar[OverBar[OverBar[b]] \[CirclePlus] a]) == 
           a \[CirclePlus] OverBar[b]], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 10} -> 
     <|"Statement" -> HoldForm[b == OverBar[a \[CirclePlus] 
            OverBar[b]] \[CirclePlus] OverBar[OverBar[a] \[CirclePlus] 
            OverBar[b]]], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 
          1}, "Orientation" -> -1, "Rule" -> 
         OverBar[(a_) \[CirclePlus] OverBar[b_]] \[CirclePlus] 
           OverBar[OverBar[b_] \[CirclePlus] OverBar[a_]] -> b, "Side" -> 1, 
        "Subpattern" -> OverBar[b_] \[CirclePlus] OverBar[a_], 
        "MatchingConstruct" -> {"Axiom", 2}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, 
        "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"CriticalPairLemma", 11} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] 
          OverBar[OverBar[a]] == OverBar[a] \[CirclePlus] a], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 7}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CirclePlus] 
           (OverBar[OverBar[a_] \[CirclePlus] (b_)] \[CirclePlus] 
            OverBar[OverBar[OverBar[b_]] \[CirclePlus] (a_)]) -> 
          a \[CirclePlus] OverBar[b], "Side" -> 1, "Subpattern" -> 
         OverBar[OverBar[a_] \[CirclePlus] (b_)] \[CirclePlus] 
          OverBar[OverBar[OverBar[b_]] \[CirclePlus] (a_)], 
        "MatchingConstruct" -> {"CriticalPairLemma", 10}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         OverBar[(a_) \[CirclePlus] OverBar[b_]] \[CirclePlus] 
           OverBar[OverBar[a_] \[CirclePlus] OverBar[b_]] -> b, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 8} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] 
          OverBar[OverBar[a]] == a \[CirclePlus] OverBar[a]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 11}, 
        "Construct" -> {"Axiom", 2}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a] \[CirclePlus] OverBar[OverBar[a]] == 
           a \[CirclePlus] OverBar[a]], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 12} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] 
          (OverBar[OverBar[a]] \[CirclePlus] b) == 
         (a \[CirclePlus] OverBar[a]) \[CirclePlus] b], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> ((a_) \[CirclePlus] (b_)) \[CirclePlus] (c_) -> 
          a \[CirclePlus] (b \[CirclePlus] c), "Side" -> 1, 
        "Subpattern" -> (a_) \[CirclePlus] (b_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 8}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> OverBar[a_] \[CirclePlus] OverBar[OverBar[a_]] -> 
          a \[CirclePlus] OverBar[a], "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 9} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] 
          (OverBar[OverBar[a]] \[CirclePlus] b) == a \[CirclePlus] 
          (OverBar[a] \[CirclePlus] b)], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 12}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {}, "Rule" -> ((a_) \[CirclePlus] (b_)) \[CirclePlus] 
           (c_) -> a \[CirclePlus] (b \[CirclePlus] c), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a] \[CirclePlus] 
            (OverBar[OverBar[a]] \[CirclePlus] b) == a \[CirclePlus] 
            (OverBar[a] \[CirclePlus] b)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 13} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] 
          OverBar[OverBar[OverBar[a]]] == OverBar[a] \[CirclePlus] 
          OverBar[a \[CirclePlus] (OverBar[a] \[CirclePlus] 
             OverBar[OverBar[a] \[CirclePlus] OverBar[OverBar[OverBar[
                  a]]]])]], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 3}, "Orientation" -> -1, 
        "Rule" -> OverBar[a_] \[CirclePlus] OverBar[OverBar[a_] \[CirclePlus] 
             ((b_) \[CirclePlus] OverBar[OverBar[a_] \[CirclePlus] 
                OverBar[b_]])] -> OverBar[a] \[CirclePlus] OverBar[b], 
        "Side" -> 1, "Subpattern" -> OverBar[a_] \[CirclePlus] 
          ((b_) \[CirclePlus] OverBar[OverBar[a_] \[CirclePlus] 
             OverBar[b_]]), "MatchingConstruct" -> {"SubstitutionLemma", 9}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         OverBar[a_] \[CirclePlus] (OverBar[OverBar[a_]] \[CirclePlus] 
            (b_)) -> a \[CirclePlus] (OverBar[a] \[CirclePlus] b), 
        "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"SubstitutionLemma", 10} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] 
          OverBar[OverBar[OverBar[a]]] == OverBar[a] \[CirclePlus] 
          OverBar[OverBar[a] \[CirclePlus] (a \[CirclePlus] 
             OverBar[OverBar[a] \[CirclePlus] OverBar[a]])]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 13}, 
        "Construct" -> {"CriticalPairLemma", 7}, "Position" -> {2, 1}, 
        "Rule" -> (a_) \[CirclePlus] ((b_) \[CirclePlus] 
            OverBar[OverBar[a_] \[CirclePlus] OverBar[OverBar[c_]]]) -> 
          b \[CirclePlus] (a \[CirclePlus] OverBar[OverBar[a] \[CirclePlus] 
              c]), "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[OverBar[a] \[CirclePlus] OverBar[OverBar[OverBar[a]]] == 
           OverBar[a] \[CirclePlus] OverBar[OverBar[a] \[CirclePlus] 
              (a \[CirclePlus] OverBar[OverBar[a] \[CirclePlus] OverBar[
                  a]])]], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 11} -> 
     <|"Statement" -> HoldForm[OverBar[a] \[CirclePlus] 
          OverBar[OverBar[OverBar[a]]] == OverBar[a] \[CirclePlus] 
          OverBar[a]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 10}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Position" -> {}, 
        "Rule" -> OverBar[a_] \[CirclePlus] OverBar[OverBar[a_] \[CirclePlus] 
             ((b_) \[CirclePlus] OverBar[OverBar[a_] \[CirclePlus] 
                OverBar[b_]])] -> OverBar[a] \[CirclePlus] OverBar[b], 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a] \[CirclePlus] OverBar[OverBar[OverBar[a]]] == 
           OverBar[a] \[CirclePlus] OverBar[a]], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 14} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[a]] == 
         OverBar[OverBar[a] \[CirclePlus] OverBar[a]] \[CirclePlus] 
          OverBar[OverBar[OverBar[OverBar[a]]] \[CirclePlus] 
            OverBar[OverBar[a]]]], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 1}, "Orientation" -> -1, 
        "Rule" -> OverBar[(a_) \[CirclePlus] OverBar[b_]] \[CirclePlus] 
           OverBar[OverBar[b_] \[CirclePlus] OverBar[a_]] -> b, "Side" -> 1, 
        "Subpattern" -> (a_) \[CirclePlus] OverBar[b_], 
        "MatchingConstruct" -> {"SubstitutionLemma", 11}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         OverBar[a_] \[CirclePlus] OverBar[OverBar[OverBar[a_]]] -> 
          OverBar[a] \[CirclePlus] OverBar[a], "MatchingSide" -> 1, 
        "Position" -> {1, 1}|>|>, {"SubstitutionLemma", 12} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[a]] == 
         OverBar[OverBar[a] \[CirclePlus] OverBar[a]] \[CirclePlus] 
          OverBar[OverBar[OverBar[a]] \[CirclePlus] 
            OverBar[OverBar[OverBar[a]]]]], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 14}, "Construct" -> {"Axiom", 2}, 
        "Position" -> {2, 1}, "Rule" -> (a_) \[CirclePlus] (b_) -> 
          b \[CirclePlus] a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[OverBar[OverBar[a]] == OverBar[OverBar[a] \[CirclePlus] 
              OverBar[a]] \[CirclePlus] OverBar[OverBar[OverBar[
                a]] \[CirclePlus] OverBar[OverBar[OverBar[a]]]]], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 13} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[a]] == 
         OverBar[OverBar[a] \[CirclePlus] OverBar[a]] \[CirclePlus] 
          OverBar[OverBar[a] \[CirclePlus] OverBar[OverBar[a]]]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 12}, 
        "Construct" -> {"SubstitutionLemma", 8}, "Position" -> {2, 1}, 
        "Rule" -> OverBar[a_] \[CirclePlus] OverBar[OverBar[a_]] -> 
          a \[CirclePlus] OverBar[a], "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[OverBar[a]] == 
           OverBar[OverBar[a] \[CirclePlus] OverBar[a]] \[CirclePlus] 
            OverBar[OverBar[a] \[CirclePlus] OverBar[OverBar[a]]]], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 14} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[a]] == a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 13}, 
        "Construct" -> {"Axiom", 3}, "Position" -> {}, 
        "Rule" -> OverBar[OverBar[a_] \[CirclePlus] (b_)] \[CirclePlus] 
           OverBar[OverBar[a_] \[CirclePlus] OverBar[b_]] -> a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[OverBar[OverBar[a]] == 
           a], "Source" -> "norm"|>|>, {"Conclusion", 1} -> 
     <|"Statement" -> HoldForm[a == a], "Proof" -> 
       <|"Input" -> {"Hypothesis", 1}, "Construct" -> {"SubstitutionLemma", 
          14}, "Position" -> {}, "Rule" -> OverBar[OverBar[a_]] -> a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[a == a], 
        "Source" -> "cpl"|>|>}|>]
