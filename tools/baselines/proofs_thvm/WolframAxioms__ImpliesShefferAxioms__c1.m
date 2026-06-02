ProofObject["EquationalLogic", Inactive[Equal][
  (a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a), a], 
 {Inactive[Equal][(((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
    ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] (a_))), c_]}, 
 <|"Variables" -> {a, b, c, x3, x4}, "Constants" -> {}, 
  "Proof" -> {{"Axiom", 1} -> <|"Statement" -> 
       HoldForm[((a \[CenterDot] b) \[CenterDot] c) \[CenterDot] 
          (a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] a)) == c], 
      "Proof" -> <||>|>, {"Hypothesis", 1} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] a) == a], "Proof" -> <||>|>, 
    {"CriticalPairLemma", 1} -> 
     <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
          (((c \[CenterDot] x3) \[CenterDot] a) \[CenterDot] 
           ((((c \[CenterDot] x3) \[CenterDot] a) \[CenterDot] 
             b) \[CenterDot] ((c \[CenterDot] x3) \[CenterDot] a)))], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] 
          (b_), "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 
         1, "MatchingRule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] 
              (c_)) \[CenterDot] (a_))) -> c, "MatchingSide" -> 1, 
        "Position" -> {1, 1}|>|>, {"CriticalPairLemma", 2} -> 
     <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
          ((c \[CenterDot] a) \[CenterDot] 
           ((((((x3 \[CenterDot] x4) \[CenterDot] c) \[CenterDot] (
                x3 \[CenterDot] ((x3 \[CenterDot] c) \[CenterDot] 
                 x3))) \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
            ((((x3 \[CenterDot] x4) \[CenterDot] c) \[CenterDot] 
              (x3 \[CenterDot] ((x3 \[CenterDot] c) \[CenterDot] 
                x3))) \[CenterDot] a)))], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 1}, "Orientation" -> -1, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           ((((c_) \[CenterDot] (x3_)) \[CenterDot] (a_)) \[CenterDot] 
            (((((c_) \[CenterDot] (x3_)) \[CenterDot] (a_)) \[CenterDot] 
              (b_)) \[CenterDot] (((c_) \[CenterDot] (x3_)) \[CenterDot] 
              (a_)))) -> b, "Side" -> 1, "Subpattern" -> 
         (c_) \[CenterDot] (x3_), "MatchingConstruct" -> {"Axiom", 1}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "MatchingSide" -> 1, "Position" -> {2, 1, 1}|>|>, 
    {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
          ((c \[CenterDot] a) \[CenterDot] (((c \[CenterDot] a) \[CenterDot] 
             b) \[CenterDot] ((((x3 \[CenterDot] x4) \[CenterDot] 
               c) \[CenterDot] (x3 \[CenterDot] ((x3 \[CenterDot] 
                 c) \[CenterDot] x3))) \[CenterDot] a)))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 2}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {2, 2, 1, 1, 1}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
            ((c \[CenterDot] a) \[CenterDot] (((c \[CenterDot] 
                a) \[CenterDot] b) \[CenterDot] ((((x3 \[CenterDot] 
                  x4) \[CenterDot] c) \[CenterDot] (x3 \[CenterDot] 
                 ((x3 \[CenterDot] c) \[CenterDot] x3))) \[CenterDot] a)))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
          ((c \[CenterDot] a) \[CenterDot] (((c \[CenterDot] a) \[CenterDot] 
             b) \[CenterDot] (c \[CenterDot] a)))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 1}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {2, 2, 2, 1}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
            ((c \[CenterDot] a) \[CenterDot] (((c \[CenterDot] 
                a) \[CenterDot] b) \[CenterDot] (c \[CenterDot] a)))], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 3} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] 
          ((b \[CenterDot] a) \[CenterDot] b) == 
         (a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
             b))) \[CenterDot] (((b \[CenterDot] c) \[CenterDot] 
            a) \[CenterDot] (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
             a)))], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 2}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((c_) \[CenterDot] (a_)) \[CenterDot] 
            ((((c_) \[CenterDot] (a_)) \[CenterDot] (b_)) \[CenterDot] 
             ((c_) \[CenterDot] (a_)))) -> b, "Side" -> 1, 
        "Subpattern" -> ((c_) \[CenterDot] (a_)) \[CenterDot] (b_), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] 
              (c_)) \[CenterDot] (a_))) -> c, "MatchingSide" -> 1, 
        "Position" -> {2, 2, 1}|>|>, {"CriticalPairLemma", 4} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] a) == 
         (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             a))) \[CenterDot] a], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 3}, "Orientation" -> -1, 
        "Rule" -> ((a_) \[CenterDot] ((b_) \[CenterDot] 
             (((b_) \[CenterDot] (a_)) \[CenterDot] (b_)))) \[CenterDot] 
           ((((b_) \[CenterDot] (c_)) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] 
              (a_)))) -> b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), 
        "Side" -> 1, "Subpattern" -> (((b_) \[CenterDot] (c_)) \[CenterDot] 
           (a_)) \[CenterDot] ((a_) \[CenterDot] (((b_) \[CenterDot] 
             (c_)) \[CenterDot] (a_))), "MatchingConstruct" -> {"Axiom", 1}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 5} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a))], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "Side" -> 1, "Subpattern" -> 
         ((a_) \[CenterDot] (b_)) \[CenterDot] (c_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 4}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) \[CenterDot] 
           (a_) -> a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 6} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] 
          ((b \[CenterDot] a) \[CenterDot] b) == a \[CenterDot] 
          ((b \[CenterDot] c) \[CenterDot] (((b \[CenterDot] c) \[CenterDot] 
             (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b))) \[CenterDot] (b \[CenterDot] c)))], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "Side" -> 1, "Subpattern" -> 
         ((a_) \[CenterDot] (b_)) \[CenterDot] (c_), "MatchingConstruct" -> 
         {"Axiom", 1}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 7} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] a) == a \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a))))], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 6}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] 
           (((b_) \[CenterDot] (c_)) \[CenterDot] 
            ((((b_) \[CenterDot] (c_)) \[CenterDot] ((b_) \[CenterDot] (
                ((b_) \[CenterDot] (a_)) \[CenterDot] (b_)))) \[CenterDot] 
             ((b_) \[CenterDot] (c_)))) -> b \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] b), "Side" -> 1, 
        "Subpattern" -> ((b_) \[CenterDot] (c_)) \[CenterDot] 
          ((b_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] (b_))), 
        "MatchingConstruct" -> {"CriticalPairLemma", 5}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (a_))) \[CenterDot] ((a_) \[CenterDot] 
            (((a_) \[CenterDot] (a_)) \[CenterDot] (a_))) -> a, 
        "MatchingSide" -> 1, "Position" -> {2, 2, 1}|>|>, 
    {"CriticalPairLemma", 8} -> 
     <|"Statement" -> HoldForm[c \[CenterDot] 
          ((c \[CenterDot] a) \[CenterDot] c) == a \[CenterDot] 
          ((b \[CenterDot] ((b \[CenterDot] c) \[CenterDot] b)) \[CenterDot] 
           (((c \[CenterDot] ((b \[CenterDot] x3) \[CenterDot] (
                ((b \[CenterDot] x3) \[CenterDot] (b \[CenterDot] 
                  ((b \[CenterDot] c) \[CenterDot] b))) \[CenterDot] 
                (b \[CenterDot] x3)))) \[CenterDot] (c \[CenterDot] 
              ((c \[CenterDot] a) \[CenterDot] c))) \[CenterDot] 
            (c \[CenterDot] ((b \[CenterDot] x3) \[CenterDot] 
              (((b \[CenterDot] x3) \[CenterDot] (b \[CenterDot] 
                 ((b \[CenterDot] c) \[CenterDot] b))) \[CenterDot] (
                b \[CenterDot] x3))))))], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 6}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] 
            ((((b_) \[CenterDot] (c_)) \[CenterDot] ((b_) \[CenterDot] (
                ((b_) \[CenterDot] (a_)) \[CenterDot] (b_)))) \[CenterDot] 
             ((b_) \[CenterDot] (c_)))) -> b \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] b), "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] (c_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 6}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CenterDot] (((b_) \[CenterDot] 
             (c_)) \[CenterDot] ((((b_) \[CenterDot] (c_)) \[CenterDot] 
              ((b_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] 
                (b_)))) \[CenterDot] ((b_) \[CenterDot] (c_)))) -> 
          b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), 
        "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"SubstitutionLemma", 3} -> 
     <|"Statement" -> HoldForm[c \[CenterDot] 
          ((c \[CenterDot] a) \[CenterDot] c) == a \[CenterDot] 
          ((b \[CenterDot] ((b \[CenterDot] c) \[CenterDot] b)) \[CenterDot] 
           (((b \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
               b)) \[CenterDot] (c \[CenterDot] ((c \[CenterDot] 
                a) \[CenterDot] c))) \[CenterDot] (c \[CenterDot] 
             ((b \[CenterDot] x3) \[CenterDot] (((b \[CenterDot] 
                 x3) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                   c) \[CenterDot] b))) \[CenterDot] (b \[CenterDot] 
                x3))))))], "Proof" -> <|"Input" -> {"CriticalPairLemma", 8}, 
        "Construct" -> {"CriticalPairLemma", 6}, "Position" -> {2, 2, 1, 1}, 
        "Rule" -> (a_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] 
            ((((b_) \[CenterDot] (c_)) \[CenterDot] ((b_) \[CenterDot] (
                ((b_) \[CenterDot] (a_)) \[CenterDot] (b_)))) \[CenterDot] 
             ((b_) \[CenterDot] (c_)))) -> b \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[c \[CenterDot] 
            ((c \[CenterDot] a) \[CenterDot] c) == a \[CenterDot] 
            ((b \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
               b)) \[CenterDot] (((b \[CenterDot] ((b \[CenterDot] 
                  c) \[CenterDot] b)) \[CenterDot] (c \[CenterDot] 
                ((c \[CenterDot] a) \[CenterDot] c))) \[CenterDot] 
              (c \[CenterDot] ((b \[CenterDot] x3) \[CenterDot] 
                (((b \[CenterDot] x3) \[CenterDot] (b \[CenterDot] 
                   ((b \[CenterDot] c) \[CenterDot] b))) \[CenterDot] 
                 (b \[CenterDot] x3))))))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 4} -> 
     <|"Statement" -> HoldForm[c \[CenterDot] 
          ((c \[CenterDot] a) \[CenterDot] c) == a \[CenterDot] 
          ((b \[CenterDot] ((b \[CenterDot] c) \[CenterDot] b)) \[CenterDot] 
           (((b \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
               b)) \[CenterDot] (c \[CenterDot] ((c \[CenterDot] 
                a) \[CenterDot] c))) \[CenterDot] (b \[CenterDot] 
             ((b \[CenterDot] c) \[CenterDot] b))))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 3}, 
        "Construct" -> {"CriticalPairLemma", 6}, "Position" -> {2, 2, 2}, 
        "Rule" -> (a_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] 
            ((((b_) \[CenterDot] (c_)) \[CenterDot] ((b_) \[CenterDot] (
                ((b_) \[CenterDot] (a_)) \[CenterDot] (b_)))) \[CenterDot] 
             ((b_) \[CenterDot] (c_)))) -> b \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[c \[CenterDot] 
            ((c \[CenterDot] a) \[CenterDot] c) == a \[CenterDot] 
            ((b \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
               b)) \[CenterDot] (((b \[CenterDot] ((b \[CenterDot] 
                  c) \[CenterDot] b)) \[CenterDot] (c \[CenterDot] 
                ((c \[CenterDot] a) \[CenterDot] c))) \[CenterDot] 
              (b \[CenterDot] ((b \[CenterDot] c) \[CenterDot] b))))], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 9} -> 
     <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           ((((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                  a) \[CenterDot] a))) \[CenterDot] a) \[CenterDot] 
             b) \[CenterDot] ((a \[CenterDot] (a \[CenterDot] (
                (a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] a)))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 2}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((c_) \[CenterDot] (a_)) \[CenterDot] 
            ((((c_) \[CenterDot] (a_)) \[CenterDot] (b_)) \[CenterDot] 
             ((c_) \[CenterDot] (a_)))) -> b, "Side" -> 1, 
        "Subpattern" -> (c_) \[CenterDot] (a_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 4}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) \[CenterDot] 
           (a_) -> a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
        "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"SubstitutionLemma", 5} -> 
     <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a)) \[CenterDot] b) \[CenterDot] ((a \[CenterDot] 
              (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a))) \[CenterDot] a)))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 9}, "Construct" -> 
         {"CriticalPairLemma", 4}, "Position" -> {2, 2, 1, 1}, 
        "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) \[CenterDot] 
           (a_) -> a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          b == (a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] 
              ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
             (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                 a)) \[CenterDot] b) \[CenterDot] ((a \[CenterDot] 
                (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a))) \[CenterDot] a)))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 6} -> 
     <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a)) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] a))))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 5}, 
        "Construct" -> {"CriticalPairLemma", 4}, "Position" -> {2, 2, 2}, 
        "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) \[CenterDot] 
           (a_) -> a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          b == (a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] 
              ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
             (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                 a)) \[CenterDot] b) \[CenterDot] (a \[CenterDot] (
                (a \[CenterDot] a) \[CenterDot] a))))], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 10} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a))) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
            ((a \[CenterDot] a) \[CenterDot] a))) == 
         ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a))) \[CenterDot] a) \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
           (((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a))) \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                  a) \[CenterDot] a))) \[CenterDot] a))))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 4}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] 
           (((b_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] 
              (b_))) \[CenterDot] ((((b_) \[CenterDot] (((b_) \[CenterDot] 
                 (c_)) \[CenterDot] (b_))) \[CenterDot] ((c_) \[CenterDot] (
                ((c_) \[CenterDot] (a_)) \[CenterDot] (c_)))) \[CenterDot] 
             ((b_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] (
                b_))))) -> c \[CenterDot] ((c \[CenterDot] a) \[CenterDot] 
            c), "Side" -> 1, "Subpattern" -> 
         ((b_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] 
            (b_))) \[CenterDot] ((c_) \[CenterDot] 
           (((c_) \[CenterDot] (a_)) \[CenterDot] (c_))), 
        "MatchingConstruct" -> {"SubstitutionLemma", 6}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] (b_)) \[CenterDot] (((a_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (a_))) \[CenterDot] 
            ((((a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
                (a_))) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
              (((a_) \[CenterDot] (a_)) \[CenterDot] (a_))))) -> b, 
        "MatchingSide" -> 1, "Position" -> {2, 2, 1}|>|>, 
    {"SubstitutionLemma", 7} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a))) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
            ((a \[CenterDot] a) \[CenterDot] a))) == 
         (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
           (((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a))) \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                  a) \[CenterDot] a))) \[CenterDot] a))))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 10}, 
        "Construct" -> {"CriticalPairLemma", 4}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) \[CenterDot] 
           (a_) -> a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a)) \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
               a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] a))) == (a \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            ((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
               a)) \[CenterDot] (((a \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
               a) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                 (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                   a))) \[CenterDot] a))))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 8} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a))) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
            ((a \[CenterDot] a) \[CenterDot] a))) == 
         (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a))) \[CenterDot] (((a \[CenterDot] (a \[CenterDot] (
                (a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
             a) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
                ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] a))))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 7}, 
        "Construct" -> {"CriticalPairLemma", 4}, "Position" -> {2, 1, 2}, 
        "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) \[CenterDot] 
           (a_) -> a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a)) \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
               a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] a))) == (a \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a))) \[CenterDot] (((a \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
               a) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                 (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                   a))) \[CenterDot] a))))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 9} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a))) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
            ((a \[CenterDot] a) \[CenterDot] a))) == 
         (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 8}, 
        "Construct" -> {"CriticalPairLemma", 3}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] ((b_) \[CenterDot] 
             (((b_) \[CenterDot] (a_)) \[CenterDot] (b_)))) \[CenterDot] 
           ((((b_) \[CenterDot] (c_)) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] 
              (a_)))) -> b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a)) \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
               a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] a))) == (a \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 10} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a))) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
            ((a \[CenterDot] a) \[CenterDot] a))) == a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 9}, 
        "Construct" -> {"CriticalPairLemma", 5}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (a_))) \[CenterDot] ((a_) \[CenterDot] 
            (((a_) \[CenterDot] (a_)) \[CenterDot] (a_))) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a)) \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
               a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] a))) == a], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 11} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a))) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
              a) \[CenterDot] a))) == a], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 10}, "Construct" -> 
         {"CriticalPairLemma", 4}, "Position" -> {2, 1, 2}, 
        "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) \[CenterDot] 
           (a_) -> a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] a))) \[CenterDot] (a \[CenterDot] 
              ((a \[CenterDot] a) \[CenterDot] a))) == a], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 12} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             a))) == a], "Proof" -> <|"Input" -> {"SubstitutionLemma", 11}, 
        "Construct" -> {"CriticalPairLemma", 5}, "Position" -> {2, 1}, 
        "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (a_))) \[CenterDot] ((a_) \[CenterDot] 
            (((a_) \[CenterDot] (a_)) \[CenterDot] (a_))) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a))) == a], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 13} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] a) == a \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 7}, 
        "Construct" -> {"SubstitutionLemma", 12}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (a_))) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) -> a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a) == 
           a \[CenterDot] a], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 14} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 5}, 
        "Construct" -> {"SubstitutionLemma", 13}, "Position" -> {1}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 15} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] a)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 14}, "Construct" -> 
         {"SubstitutionLemma", 13}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"Conclusion", 1} -> <|"Statement" -> HoldForm[a == a], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Construct" -> 
         {"SubstitutionLemma", 15}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[a == a], "Source" -> "cpl"|>|>}|>]
