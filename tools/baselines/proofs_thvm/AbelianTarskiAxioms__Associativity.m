ProofObject["EquationalLogic", Inactive[Equal][
  a \[CircleDot] ((a \[CircleDot] a) \[CircleDot] 
    (b \[CircleDot] ((a \[CircleDot] a) \[CircleDot] c))), 
  (a \[CircleDot] ((a \[CircleDot] a) \[CircleDot] b)) \[CircleDot] 
   ((a \[CircleDot] a) \[CircleDot] c)], 
 {Inactive[Equal][(a_) \[CircleDot] ((b_) \[CircleDot] 
     ((c_) \[CircleDot] ((a_) \[CircleDot] (b_)))), c_]}, 
 <|"Variables" -> {a, b, c, x3}, "Constants" -> {}, 
  "Proof" -> {{"Axiom", 1} -> <|"Statement" -> 
       HoldForm[a \[CircleDot] (b \[CircleDot] (c \[CircleDot] 
            (a \[CircleDot] b))) == c], "Proof" -> <||>|>, 
    {"Hypothesis", 1} -> <|"Statement" -> 
       HoldForm[a \[CircleDot] ((a \[CircleDot] a) \[CircleDot] 
           (b \[CircleDot] ((a \[CircleDot] a) \[CircleDot] c))) == 
         (a \[CircleDot] ((a \[CircleDot] a) \[CircleDot] b)) \[CircleDot] 
          ((a \[CircleDot] a) \[CircleDot] c)], "Proof" -> <||>|>, 
    {"CriticalPairLemma", 1} -> 
     <|"Statement" -> HoldForm[c == a \[CircleDot] 
          ((b \[CircleDot] (c \[CircleDot] a)) \[CircleDot] b)], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CircleDot] ((b_) \[CircleDot] ((c_) \[CircleDot] 
             ((a_) \[CircleDot] (b_)))) -> c, "Side" -> 1, 
        "Subpattern" -> (c_) \[CircleDot] ((a_) \[CircleDot] (b_)), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CircleDot] ((b_) \[CircleDot] 
            ((c_) \[CircleDot] ((a_) \[CircleDot] (b_)))) -> c, 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"CriticalPairLemma", 2} -> 
     <|"Statement" -> HoldForm[(a \[CircleDot] c) \[CircleDot] 
          (b \[CircleDot] c) == a \[CircleDot] b], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CircleDot] ((b_) \[CircleDot] ((c_) \[CircleDot] 
             ((a_) \[CircleDot] (b_)))) -> c, "Side" -> 1, 
        "Subpattern" -> (b_) \[CircleDot] ((c_) \[CircleDot] 
           ((a_) \[CircleDot] (b_))), "MatchingConstruct" -> 
         {"CriticalPairLemma", 1}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CircleDot] (((b_) \[CircleDot] 
             ((c_) \[CircleDot] (a_))) \[CircleDot] (b_)) -> c, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 3} -> 
     <|"Statement" -> HoldForm[c \[CircleDot] b == a \[CircleDot] 
          (b \[CircleDot] (c \[CircleDot] a))], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CircleDot] ((b_) \[CircleDot] ((c_) \[CircleDot] 
             ((a_) \[CircleDot] (b_)))) -> c, "Side" -> 1, 
        "Subpattern" -> (c_) \[CircleDot] ((a_) \[CircleDot] (b_)), 
        "MatchingConstruct" -> {"CriticalPairLemma", 2}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((a_) \[CircleDot] (b_)) \[CircleDot] ((c_) \[CircleDot] (b_)) -> 
          a \[CircleDot] c, "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[a \[CircleDot] (a \[CircleDot] c) == c], 
      "Proof" -> <|"Input" -> {"Axiom", 1}, "Construct" -> 
         {"CriticalPairLemma", 3}, "Position" -> {2}, 
        "Rule" -> (a_) \[CircleDot] ((b_) \[CircleDot] ((c_) \[CircleDot] 
             (a_))) -> c \[CircleDot] b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CircleDot] (a \[CircleDot] c) == 
           c], "Source" -> "norm"|>|>, {"CriticalPairLemma", 4} -> 
     <|"Statement" -> HoldForm[b \[CircleDot] b == a \[CircleDot] a], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 3}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CircleDot] ((b_) \[CircleDot] 
            ((c_) \[CircleDot] (a_))) -> c \[CircleDot] b, "Side" -> 1, 
        "Subpattern" -> (b_) \[CircleDot] ((c_) \[CircleDot] (a_)), 
        "MatchingConstruct" -> {"SubstitutionLemma", 1}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleDot] ((a_) \[CircleDot] (b_)) -> b, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 5} -> 
     <|"Statement" -> HoldForm[c \[CircleDot] b == 
         (a \[CircleDot] a) \[CircleDot] (b \[CircleDot] c)], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 2}, 
        "Orientation" -> 1, "Rule" -> ((a_) \[CircleDot] (b_)) \[CircleDot] 
           ((c_) \[CircleDot] (b_)) -> a \[CircleDot] c, "Side" -> 1, 
        "Subpattern" -> (a_) \[CircleDot] (b_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 4}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CircleDot] (a_) -> b \[CircleDot] b, 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 6} -> 
     <|"Statement" -> HoldForm[(a \[CircleDot] (b \[CircleDot] 
            c)) \[CircleDot] c == a \[CircleDot] b], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 3}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CircleDot] ((b_) \[CircleDot] 
            ((c_) \[CircleDot] (a_))) -> c \[CircleDot] b, "Side" -> 1, 
        "Subpattern" -> (b_) \[CircleDot] ((c_) \[CircleDot] (a_)), 
        "MatchingConstruct" -> {"CriticalPairLemma", 1}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CircleDot] (((b_) \[CircleDot] ((c_) \[CircleDot] 
              (a_))) \[CircleDot] (b_)) -> c, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 7} -> 
     <|"Statement" -> HoldForm[a \[CircleDot] (b \[CircleDot] 
           (c \[CircleDot] x3)) == (a \[CircleDot] (b \[CircleDot] 
            c)) \[CircleDot] x3], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 6}, "Orientation" -> 1, 
        "Rule" -> ((a_) \[CircleDot] ((b_) \[CircleDot] (c_))) \[CircleDot] 
           (c_) -> a \[CircleDot] b, "Side" -> 1, "Subpattern" -> 
         (b_) \[CircleDot] (c_), "MatchingConstruct" -> {"CriticalPairLemma", 
          6}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((a_) \[CircleDot] ((b_) \[CircleDot] (c_))) \[CircleDot] (c_) -> 
          a \[CircleDot] b, "MatchingSide" -> 1, "Position" -> {1, 2}|>|>, 
    {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[a \[CircleDot] 
          (((a \[CircleDot] a) \[CircleDot] c) \[CircleDot] b) == 
         (a \[CircleDot] ((a \[CircleDot] a) \[CircleDot] b)) \[CircleDot] 
          ((a \[CircleDot] a) \[CircleDot] c)], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Construct" -> 
         {"CriticalPairLemma", 5}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CircleDot] (a_)) \[CircleDot] ((b_) \[CircleDot] 
            (c_)) -> c \[CircleDot] b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CircleDot] 
            (((a \[CircleDot] a) \[CircleDot] c) \[CircleDot] b) == 
           (a \[CircleDot] ((a \[CircleDot] a) \[CircleDot] b)) \[CircleDot] 
            ((a \[CircleDot] a) \[CircleDot] c)], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 3} -> 
     <|"Statement" -> HoldForm[a \[CircleDot] 
          (((a \[CircleDot] a) \[CircleDot] c) \[CircleDot] b) == 
         a \[CircleDot] ((a \[CircleDot] a) \[CircleDot] (b \[CircleDot] 
            ((a \[CircleDot] a) \[CircleDot] c)))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 2}, 
        "Construct" -> {"CriticalPairLemma", 7}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleDot] ((b_) \[CircleDot] (c_))) \[CircleDot] 
           (x3_) -> a \[CircleDot] (b \[CircleDot] (c \[CircleDot] x3)), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CircleDot] (((a \[CircleDot] a) \[CircleDot] c) \[CircleDot] 
             b) == a \[CircleDot] ((a \[CircleDot] a) \[CircleDot] 
             (b \[CircleDot] ((a \[CircleDot] a) \[CircleDot] c)))], 
        "Source" -> "cpl"|>|>, {"Conclusion", 1} -> 
     <|"Statement" -> HoldForm[a \[CircleDot] 
          (((a \[CircleDot] a) \[CircleDot] c) \[CircleDot] b) == 
         a \[CircleDot] (((a \[CircleDot] a) \[CircleDot] c) \[CircleDot] 
           b)], "Proof" -> <|"Input" -> {"SubstitutionLemma", 3}, 
        "Construct" -> {"CriticalPairLemma", 5}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CircleDot] (a_)) \[CircleDot] ((b_) \[CircleDot] 
            (c_)) -> c \[CircleDot] b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CircleDot] 
            (((a \[CircleDot] a) \[CircleDot] c) \[CircleDot] b) == 
           a \[CircleDot] (((a \[CircleDot] a) \[CircleDot] c) \[CircleDot] 
             b)], "Source" -> "cpl"|>|>}|>]
