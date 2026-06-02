ProofObject["EquationalLogic", Inactive[Equal][
  a \[CircleDot] ((((a \[CircleDot] a) \[CircleDot] b) \[CircleDot] 
     c) \[CircleDot] (((a \[CircleDot] a) \[CircleDot] a) \[CircleDot] c)), 
  b], {Inactive[Equal][(a_) \[CircleDot] ((b_) \[CircleDot] 
     ((c_) \[CircleDot] ((a_) \[CircleDot] (b_)))), c_]}, 
 <|"Variables" -> {a, b, c}, "Constants" -> {}, 
  "Proof" -> {{"Axiom", 1} -> <|"Statement" -> 
       HoldForm[a \[CircleDot] (b \[CircleDot] (c \[CircleDot] 
            (a \[CircleDot] b))) == c], "Proof" -> <||>|>, 
    {"Hypothesis", 1} -> <|"Statement" -> 
       HoldForm[a \[CircleDot] ((((a \[CircleDot] a) \[CircleDot] 
             b) \[CircleDot] c) \[CircleDot] 
           (((a \[CircleDot] a) \[CircleDot] a) \[CircleDot] c)) == b], 
      "Proof" -> <||>|>, {"CriticalPairLemma", 1} -> 
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
    {"SubstitutionLemma", 3} -> 
     <|"Statement" -> HoldForm[a \[CircleDot] (a \[CircleDot] c) == c], 
      "Proof" -> <|"Input" -> {"Axiom", 1}, "Construct" -> 
         {"CriticalPairLemma", 3}, "Position" -> {2}, 
        "Rule" -> (a_) \[CircleDot] ((b_) \[CircleDot] ((c_) \[CircleDot] 
             (a_))) -> c \[CircleDot] b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CircleDot] (a \[CircleDot] c) == 
           c], "Source" -> "norm"|>|>, {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[a \[CircleDot] 
          (((a \[CircleDot] a) \[CircleDot] b) \[CircleDot] 
           ((a \[CircleDot] a) \[CircleDot] a)) == b], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Construct" -> 
         {"CriticalPairLemma", 2}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CircleDot] (b_)) \[CircleDot] ((c_) \[CircleDot] 
            (b_)) -> a \[CircleDot] c, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CircleDot] 
            (((a \[CircleDot] a) \[CircleDot] b) \[CircleDot] 
             ((a \[CircleDot] a) \[CircleDot] a)) == b], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[(a \[CircleDot] a) \[CircleDot] 
          ((a \[CircleDot] a) \[CircleDot] b) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 1}, 
        "Construct" -> {"CriticalPairLemma", 3}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleDot] ((b_) \[CircleDot] ((c_) \[CircleDot] 
             (a_))) -> c \[CircleDot] b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(a \[CircleDot] a) \[CircleDot] 
            ((a \[CircleDot] a) \[CircleDot] b) == b], "Source" -> "cpl"|>|>, 
    {"Conclusion", 1} -> <|"Statement" -> HoldForm[b == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 2}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleDot] ((a_) \[CircleDot] (b_)) -> b, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[b == b], 
        "Source" -> "cpl"|>|>}|>]
