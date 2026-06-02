ProofObject["EquationalLogic", Inactive[Equal][
  a \[CircleDot] ((a \[CircleDot] a) \[CircleDot] b), 
  b \[CircleDot] ((a \[CircleDot] a) \[CircleDot] a)], 
 {Inactive[Equal][(a_) \[CircleDot] ((b_) \[CircleDot] 
     ((c_) \[CircleDot] ((a_) \[CircleDot] (b_)))), c_]}, 
 <|"Variables" -> {a, b, c, x3, x4, x5}, "Constants" -> {}, 
  "Proof" -> {{"Axiom", 1} -> <|"Statement" -> 
       HoldForm[a \[CircleDot] (b \[CircleDot] (c \[CircleDot] 
            (a \[CircleDot] b))) == c], "Proof" -> <||>|>, 
    {"Hypothesis", 1} -> <|"Statement" -> 
       HoldForm[a \[CircleDot] ((a \[CircleDot] a) \[CircleDot] b) == 
         b \[CircleDot] ((a \[CircleDot] a) \[CircleDot] a)], 
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
    {"CriticalPairLemma", 4} -> 
     <|"Statement" -> HoldForm[(a \[CircleDot] x3) \[CircleDot] c == 
         (a \[CircleDot] b) \[CircleDot] (c \[CircleDot] (b \[CircleDot] 
            x3))], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 2}, 
        "Orientation" -> 1, "Rule" -> ((a_) \[CircleDot] (b_)) \[CircleDot] 
           ((c_) \[CircleDot] (b_)) -> a \[CircleDot] c, "Side" -> 1, 
        "Subpattern" -> (a_) \[CircleDot] (b_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 2}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> ((a_) \[CircleDot] (b_)) \[CircleDot] 
           ((c_) \[CircleDot] (b_)) -> a \[CircleDot] c, "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 5} -> 
     <|"Statement" -> HoldForm[x3 \[CircleDot] (c \[CircleDot] a) == 
         (a \[CircleDot] b) \[CircleDot] ((c \[CircleDot] b) \[CircleDot] 
           x3)], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 3}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CircleDot] ((b_) \[CircleDot] 
            ((c_) \[CircleDot] (a_))) -> c \[CircleDot] b, "Side" -> 1, 
        "Subpattern" -> (b_) \[CircleDot] ((c_) \[CircleDot] (a_)), 
        "MatchingConstruct" -> {"CriticalPairLemma", 4}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CircleDot] (b_)) \[CircleDot] ((c_) \[CircleDot] 
            ((b_) \[CircleDot] (x3_))) -> (a \[CircleDot] x3) \[CircleDot] c, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 6} -> 
     <|"Statement" -> HoldForm[x5 \[CircleDot] (x3 \[CircleDot] 
           (a \[CircleDot] x4)) == ((a \[CircleDot] b) \[CircleDot] 
           c) \[CircleDot] ((x3 \[CircleDot] (c \[CircleDot] 
             (x4 \[CircleDot] b))) \[CircleDot] x5)], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 5}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CircleDot] (b_)) \[CircleDot] 
           (((c_) \[CircleDot] (b_)) \[CircleDot] (x3_)) -> 
          x3 \[CircleDot] (c \[CircleDot] a), "Side" -> 1, 
        "Subpattern" -> (a_) \[CircleDot] (b_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 4}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CircleDot] (b_)) \[CircleDot] 
           ((c_) \[CircleDot] ((b_) \[CircleDot] (x3_))) -> 
          (a \[CircleDot] x3) \[CircleDot] c, "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 7} -> 
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
        "Position" -> {2}|>|>, {"CriticalPairLemma", 8} -> 
     <|"Statement" -> HoldForm[a \[CircleDot] (b \[CircleDot] 
           (c \[CircleDot] x3)) == (a \[CircleDot] (b \[CircleDot] 
            c)) \[CircleDot] x3], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 7}, "Orientation" -> 1, 
        "Rule" -> ((a_) \[CircleDot] ((b_) \[CircleDot] (c_))) \[CircleDot] 
           (c_) -> a \[CircleDot] b, "Side" -> 1, "Subpattern" -> 
         (b_) \[CircleDot] (c_), "MatchingConstruct" -> {"CriticalPairLemma", 
          7}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((a_) \[CircleDot] ((b_) \[CircleDot] (c_))) \[CircleDot] (c_) -> 
          a \[CircleDot] b, "MatchingSide" -> 1, "Position" -> {1, 2}|>|>, 
    {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[x5 \[CircleDot] (x3 \[CircleDot] 
           (a \[CircleDot] x4)) == ((a \[CircleDot] b) \[CircleDot] 
           c) \[CircleDot] (x3 \[CircleDot] (c \[CircleDot] 
            ((x4 \[CircleDot] b) \[CircleDot] x5)))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 6}, 
        "Construct" -> {"CriticalPairLemma", 8}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CircleDot] ((b_) \[CircleDot] (c_))) \[CircleDot] 
           (x3_) -> a \[CircleDot] (b \[CircleDot] (c \[CircleDot] x3)), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          x5 \[CircleDot] (x3 \[CircleDot] (a \[CircleDot] x4)) == 
           ((a \[CircleDot] b) \[CircleDot] c) \[CircleDot] 
            (x3 \[CircleDot] (c \[CircleDot] ((x4 \[CircleDot] 
                b) \[CircleDot] x5)))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[x5 \[CircleDot] (x3 \[CircleDot] 
           (a \[CircleDot] x4)) == ((a \[CircleDot] b) \[CircleDot] 
           ((x4 \[CircleDot] b) \[CircleDot] x5)) \[CircleDot] x3], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 1}, 
        "Construct" -> {"CriticalPairLemma", 4}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleDot] (b_)) \[CircleDot] ((c_) \[CircleDot] 
            ((b_) \[CircleDot] (x3_))) -> (a \[CircleDot] x3) \[CircleDot] c, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          x5 \[CircleDot] (x3 \[CircleDot] (a \[CircleDot] x4)) == 
           ((a \[CircleDot] b) \[CircleDot] ((x4 \[CircleDot] b) \[CircleDot] 
              x5)) \[CircleDot] x3], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 3} -> 
     <|"Statement" -> HoldForm[x5 \[CircleDot] (x3 \[CircleDot] 
           (a \[CircleDot] x4)) == (a \[CircleDot] b) \[CircleDot] 
          ((x4 \[CircleDot] b) \[CircleDot] (x5 \[CircleDot] x3))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 2}, 
        "Construct" -> {"CriticalPairLemma", 8}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleDot] ((b_) \[CircleDot] (c_))) \[CircleDot] 
           (x3_) -> a \[CircleDot] (b \[CircleDot] (c \[CircleDot] x3)), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          x5 \[CircleDot] (x3 \[CircleDot] (a \[CircleDot] x4)) == 
           (a \[CircleDot] b) \[CircleDot] ((x4 \[CircleDot] b) \[CircleDot] 
             (x5 \[CircleDot] x3))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 4} -> 
     <|"Statement" -> HoldForm[x5 \[CircleDot] (x3 \[CircleDot] 
           (a \[CircleDot] x4)) == (x5 \[CircleDot] x3) \[CircleDot] 
          (x4 \[CircleDot] a)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 3}, "Construct" -> 
         {"CriticalPairLemma", 5}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleDot] (b_)) \[CircleDot] 
           (((c_) \[CircleDot] (b_)) \[CircleDot] (x3_)) -> 
          x3 \[CircleDot] (c \[CircleDot] a), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[x5 \[CircleDot] (x3 \[CircleDot] 
             (a \[CircleDot] x4)) == (x5 \[CircleDot] x3) \[CircleDot] 
            (x4 \[CircleDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 5} -> 
     <|"Statement" -> HoldForm[x3 \[CircleDot] (c \[CircleDot] a) == 
         a \[CircleDot] (b \[CircleDot] (x3 \[CircleDot] (c \[CircleDot] 
             b)))], "Proof" -> <|"Input" -> {"CriticalPairLemma", 5}, 
        "Construct" -> {"SubstitutionLemma", 4}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleDot] (b_)) \[CircleDot] ((c_) \[CircleDot] 
            (x3_)) -> a \[CircleDot] (b \[CircleDot] (x3 \[CircleDot] c)), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          x3 \[CircleDot] (c \[CircleDot] a) == a \[CircleDot] 
            (b \[CircleDot] (x3 \[CircleDot] (c \[CircleDot] b)))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 6} -> 
     <|"Statement" -> HoldForm[c \[CircleDot] (x3 \[CircleDot] a) == 
         a \[CircleDot] (x3 \[CircleDot] c)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 5}, 
        "Construct" -> {"CriticalPairLemma", 3}, "Position" -> {2}, 
        "Rule" -> (a_) \[CircleDot] ((b_) \[CircleDot] ((c_) \[CircleDot] 
             (a_))) -> c \[CircleDot] b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[c \[CircleDot] (x3 \[CircleDot] a) == 
           a \[CircleDot] (x3 \[CircleDot] c)], "Source" -> "norm"|>|>, 
    {"Conclusion", 1} -> <|"Statement" -> 
       HoldForm[a \[CircleDot] ((a \[CircleDot] a) \[CircleDot] b) == 
         a \[CircleDot] ((a \[CircleDot] a) \[CircleDot] b)], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Construct" -> 
         {"SubstitutionLemma", 6}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleDot] ((b_) \[CircleDot] (c_)) -> 
          c \[CircleDot] (b \[CircleDot] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CircleDot] 
            ((a \[CircleDot] a) \[CircleDot] b) == a \[CircleDot] 
            ((a \[CircleDot] a) \[CircleDot] b)], "Source" -> "cpl"|>|>}|>]
