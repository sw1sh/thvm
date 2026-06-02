ProofObject["EquationalLogic", Inactive[Equal][
  (a \[CenterDot] a) \[CenterDot] (b \[CenterDot] a), a], 
 {Inactive[Equal][(((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
    ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] (a_))), c_]}, 
 <|"Variables" -> {a, b, c, x3, x4}, "Constants" -> {}, 
  "Proof" -> {{"Axiom", 1} -> <|"Statement" -> 
       HoldForm[((a \[CenterDot] b) \[CenterDot] c) \[CenterDot] 
          (a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] a)) == c], 
      "Proof" -> <||>|>, {"Hypothesis", 1} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
          (b \[CenterDot] a) == a], "Proof" -> <||>|>, 
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
     <|"Statement" -> HoldForm[(x3 \[CenterDot] a) \[CenterDot] 
          (((x3 \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
           (x3 \[CenterDot] a)) == (((a \[CenterDot] b) \[CenterDot] 
            c) \[CenterDot] ((x3 \[CenterDot] a) \[CenterDot] 
            (((x3 \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
             (x3 \[CenterDot] a)))) \[CenterDot] 
          ((a \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
            (a \[CenterDot] b)))], "Proof" -> <|"Construct" -> {"Axiom", 1}, 
        "Orientation" -> 1, "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] 
              (c_)) \[CenterDot] (a_))) -> c, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (c_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 2}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((c_) \[CenterDot] (a_)) \[CenterDot] 
            ((((c_) \[CenterDot] (a_)) \[CenterDot] (b_)) \[CenterDot] 
             ((c_) \[CenterDot] (a_)))) -> b, "MatchingSide" -> 1, 
        "Position" -> {2, 2, 1}|>|>, {"CriticalPairLemma", 4} -> 
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
        "Position" -> {2, 2, 1}|>|>, {"CriticalPairLemma", 5} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] a) == 
         (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             a))) \[CenterDot] a], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 4}, "Orientation" -> -1, 
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
    {"CriticalPairLemma", 6} -> 
     <|"Statement" -> HoldForm[(c \[CenterDot] b) \[CenterDot] 
          (((c \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
           (c \[CenterDot] b)) == a \[CenterDot] (b \[CenterDot] 
           ((b \[CenterDot] ((c \[CenterDot] b) \[CenterDot] 
              (((c \[CenterDot] b) \[CenterDot] a) \[CenterDot] (
                c \[CenterDot] b)))) \[CenterDot] b))], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "Side" -> 1, "Subpattern" -> 
         ((a_) \[CenterDot] (b_)) \[CenterDot] (c_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 2}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((c_) \[CenterDot] (a_)) \[CenterDot] 
            ((((c_) \[CenterDot] (a_)) \[CenterDot] (b_)) \[CenterDot] 
             ((c_) \[CenterDot] (a_)))) -> b, "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 7} -> 
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
    {"CriticalPairLemma", 8} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
          (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] a))) \[CenterDot] 
           (a \[CenterDot] b)) == (a \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
          (b \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              a)) \[CenterDot] b))], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 6}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            (((b_) \[CenterDot] (((c_) \[CenterDot] (b_)) \[CenterDot] (
                (((c_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
                ((c_) \[CenterDot] (b_))))) \[CenterDot] (b_))) -> 
          (c \[CenterDot] b) \[CenterDot] (((c \[CenterDot] b) \[CenterDot] 
             a) \[CenterDot] (c \[CenterDot] b)), "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] (((c_) \[CenterDot] 
            (b_)) \[CenterDot] ((((c_) \[CenterDot] (b_)) \[CenterDot] 
             (a_)) \[CenterDot] ((c_) \[CenterDot] (b_)))), 
        "MatchingConstruct" -> {"CriticalPairLemma", 7}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] 
            ((((b_) \[CenterDot] (c_)) \[CenterDot] ((b_) \[CenterDot] (
                ((b_) \[CenterDot] (a_)) \[CenterDot] (b_)))) \[CenterDot] 
             ((b_) \[CenterDot] (c_)))) -> b \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] b), "MatchingSide" -> 1, 
        "Position" -> {2, 2, 1}|>|>, {"CriticalPairLemma", 9} -> 
     <|"Statement" -> HoldForm[(((b \[CenterDot] c) \[CenterDot] 
            a) \[CenterDot] ((((b \[CenterDot] c) \[CenterDot] 
              a) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                a) \[CenterDot] b))) \[CenterDot] 
            ((b \[CenterDot] c) \[CenterDot] a))) \[CenterDot] 
          ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           ((((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
             ((((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] (
                b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                 b))) \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
               a))) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
               a) \[CenterDot] b)))) == a \[CenterDot] 
          (((((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
             (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b))) \[CenterDot] (((b \[CenterDot] c) \[CenterDot] 
              a) \[CenterDot] ((((b \[CenterDot] c) \[CenterDot] 
                a) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                  a) \[CenterDot] b))) \[CenterDot] ((b \[CenterDot] 
                c) \[CenterDot] a)))) \[CenterDot] 
           (((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
            (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b))))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 8}, 
        "Orientation" -> 1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           ((((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
              (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)))) \[CenterDot] 
            ((a_) \[CenterDot] (b_))) -> (a \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
           (b \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
               a)) \[CenterDot] b)), "Side" -> 1, "Subpattern" -> 
         (a_) \[CenterDot] (b_), "MatchingConstruct" -> {"Axiom", 1}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"SubstitutionLemma", 3} -> 
     <|"Statement" -> HoldForm[(((b \[CenterDot] c) \[CenterDot] 
            a) \[CenterDot] ((((b \[CenterDot] c) \[CenterDot] 
              a) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                a) \[CenterDot] b))) \[CenterDot] 
            ((b \[CenterDot] c) \[CenterDot] a))) \[CenterDot] 
          ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           ((((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
             ((((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] (
                b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                 b))) \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
               a))) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
               a) \[CenterDot] b)))) == a \[CenterDot] 
          ((a \[CenterDot] (((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
             ((((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] (
                b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                 b))) \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
               a)))) \[CenterDot] (((b \[CenterDot] c) \[CenterDot] 
             a) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
              b))))], "Proof" -> <|"Input" -> {"CriticalPairLemma", 9}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {2, 1, 1}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[(((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
             ((((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] (
                b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                 b))) \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
               a))) \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] 
                a) \[CenterDot] b)) \[CenterDot] 
             ((((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] (
                (((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
                 (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                   b))) \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
                 a))) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                 a) \[CenterDot] b)))) == a \[CenterDot] 
            ((a \[CenterDot] (((b \[CenterDot] c) \[CenterDot] 
                a) \[CenterDot] ((((b \[CenterDot] c) \[CenterDot] 
                  a) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                    a) \[CenterDot] b))) \[CenterDot] ((b \[CenterDot] 
                  c) \[CenterDot] a)))) \[CenterDot] 
             (((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
              (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b))))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 4} -> 
     <|"Statement" -> HoldForm[(((b \[CenterDot] c) \[CenterDot] 
            a) \[CenterDot] ((((b \[CenterDot] c) \[CenterDot] 
              a) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                a) \[CenterDot] b))) \[CenterDot] 
            ((b \[CenterDot] c) \[CenterDot] a))) \[CenterDot] 
          ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           ((((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
             ((((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] (
                b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                 b))) \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
               a))) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
               a) \[CenterDot] b)))) == a \[CenterDot] 
          ((a \[CenterDot] (((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
               a)))) \[CenterDot] (((b \[CenterDot] c) \[CenterDot] 
             a) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
              b))))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 3}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {2, 1, 2, 2, 1}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[(((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
             ((((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] (
                b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                 b))) \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
               a))) \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] 
                a) \[CenterDot] b)) \[CenterDot] 
             ((((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] (
                (((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
                 (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                   b))) \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
                 a))) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                 a) \[CenterDot] b)))) == a \[CenterDot] 
            ((a \[CenterDot] (((b \[CenterDot] c) \[CenterDot] 
                a) \[CenterDot] (a \[CenterDot] ((b \[CenterDot] 
                  c) \[CenterDot] a)))) \[CenterDot] 
             (((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
              (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b))))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 5} -> 
     <|"Statement" -> HoldForm[(((b \[CenterDot] c) \[CenterDot] 
            a) \[CenterDot] ((((b \[CenterDot] c) \[CenterDot] 
              a) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                a) \[CenterDot] b))) \[CenterDot] 
            ((b \[CenterDot] c) \[CenterDot] a))) \[CenterDot] 
          ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           ((((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
             ((((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] (
                b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                 b))) \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
               a))) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
               a) \[CenterDot] b)))) == a \[CenterDot] 
          ((a \[CenterDot] (((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
               a)))) \[CenterDot] a)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 4}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {2, 2}, "Rule" -> 
         (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[(((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
             ((((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] (
                b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                 b))) \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
               a))) \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] 
                a) \[CenterDot] b)) \[CenterDot] 
             ((((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] (
                (((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
                 (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                   b))) \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
                 a))) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                 a) \[CenterDot] b)))) == a \[CenterDot] 
            ((a \[CenterDot] (((b \[CenterDot] c) \[CenterDot] 
                a) \[CenterDot] (a \[CenterDot] ((b \[CenterDot] 
                  c) \[CenterDot] a)))) \[CenterDot] a)], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 6} -> 
     <|"Statement" -> HoldForm[(((b \[CenterDot] c) \[CenterDot] 
            a) \[CenterDot] (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
             a))) \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] 
              a) \[CenterDot] b)) \[CenterDot] 
           ((((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
             ((((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] (
                b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                 b))) \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
               a))) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
               a) \[CenterDot] b)))) == a \[CenterDot] 
          ((a \[CenterDot] (((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
               a)))) \[CenterDot] a)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 5}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {1, 2, 1}, "Rule" -> 
         (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[(((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
               a))) \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] 
                a) \[CenterDot] b)) \[CenterDot] 
             ((((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] (
                (((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
                 (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                   b))) \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
                 a))) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                 a) \[CenterDot] b)))) == a \[CenterDot] 
            ((a \[CenterDot] (((b \[CenterDot] c) \[CenterDot] 
                a) \[CenterDot] (a \[CenterDot] ((b \[CenterDot] 
                  c) \[CenterDot] a)))) \[CenterDot] a)], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 7} -> 
     <|"Statement" -> HoldForm[(((b \[CenterDot] c) \[CenterDot] 
            a) \[CenterDot] (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
             a))) \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] 
              a) \[CenterDot] b)) \[CenterDot] 
           ((((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
               a))) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
               a) \[CenterDot] b)))) == a \[CenterDot] 
          ((a \[CenterDot] (((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
               a)))) \[CenterDot] a)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 6}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {2, 2, 1, 2, 1}, "Rule" -> 
         (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[(((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
               a))) \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] 
                a) \[CenterDot] b)) \[CenterDot] 
             ((((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] (
                a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
                 a))) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                 a) \[CenterDot] b)))) == a \[CenterDot] 
            ((a \[CenterDot] (((b \[CenterDot] c) \[CenterDot] 
                a) \[CenterDot] (a \[CenterDot] ((b \[CenterDot] 
                  c) \[CenterDot] a)))) \[CenterDot] a)], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 10} -> 
     <|"Statement" -> HoldForm[(((a \[CenterDot] a) \[CenterDot] 
            a) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             a))) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
              a) \[CenterDot] a)) \[CenterDot] 
           ((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a))) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] a)))) == a \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] a)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 7}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] 
           (((a_) \[CenterDot] ((((b_) \[CenterDot] (c_)) \[CenterDot] (
                a_)) \[CenterDot] ((a_) \[CenterDot] (((b_) \[CenterDot] 
                 (c_)) \[CenterDot] (a_))))) \[CenterDot] (a_)) -> 
          (((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
              a))) \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] 
               a) \[CenterDot] b)) \[CenterDot] 
            ((((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
                a))) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                a) \[CenterDot] b)))), "Side" -> 1, "Subpattern" -> 
         (((b_) \[CenterDot] (c_)) \[CenterDot] (a_)) \[CenterDot] 
          ((a_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] (a_))), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] 
              (c_)) \[CenterDot] (a_))) -> c, "MatchingSide" -> 1, 
        "Position" -> {2, 1, 2}|>|>, {"SubstitutionLemma", 8} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           ((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a))) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] a)))) == a \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 10}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {1}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] a)) \[CenterDot] 
             ((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] (
                a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                 a))) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] a)))) == a \[CenterDot] 
            ((a \[CenterDot] a) \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 9} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a)))) == a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 8}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {2, 2, 1}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
              (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)))) == 
           a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 11} -> 
     <|"Statement" -> HoldForm[
        c == ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             a)) \[CenterDot] c) \[CenterDot] (b \[CenterDot] 
           ((b \[CenterDot] c) \[CenterDot] b))], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] 
          (b_), "MatchingConstruct" -> {"CriticalPairLemma", 7}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] 
            ((((b_) \[CenterDot] (c_)) \[CenterDot] ((b_) \[CenterDot] (
                ((b_) \[CenterDot] (a_)) \[CenterDot] (b_)))) \[CenterDot] 
             ((b_) \[CenterDot] (c_)))) -> b \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] b), "MatchingSide" -> 1, 
        "Position" -> {1, 1}|>|>, {"CriticalPairLemma", 12} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
          (((b \[CenterDot] a) \[CenterDot] ((a \[CenterDot] 
              (b \[CenterDot] a)) \[CenterDot] a)) \[CenterDot] 
           (b \[CenterDot] a)) == ((a \[CenterDot] (b \[CenterDot] 
             a)) \[CenterDot] a) \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
             a)) \[CenterDot] (((a \[CenterDot] (b \[CenterDot] 
               a)) \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] a))))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 3}, 
        "Orientation" -> -1, "Rule" -> 
         ((((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
            (((x3_) \[CenterDot] (a_)) \[CenterDot] ((((x3_) \[CenterDot] 
                (a_)) \[CenterDot] (b_)) \[CenterDot] ((x3_) \[CenterDot] (
                a_))))) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((b_) \[CenterDot] ((a_) \[CenterDot] (b_)))) -> 
          (x3 \[CenterDot] a) \[CenterDot] (((x3 \[CenterDot] a) \[CenterDot] 
             b) \[CenterDot] (x3 \[CenterDot] a)), "Side" -> 1, 
        "Subpattern" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
           (c_)) \[CenterDot] (((x3_) \[CenterDot] (a_)) \[CenterDot] 
           ((((x3_) \[CenterDot] (a_)) \[CenterDot] (b_)) \[CenterDot] 
            ((x3_) \[CenterDot] (a_)))), "MatchingConstruct" -> 
         {"CriticalPairLemma", 11}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (((a_) \[CenterDot] (((a_) \[CenterDot] (
                b_)) \[CenterDot] (a_))) \[CenterDot] (c_)) \[CenterDot] 
           ((b_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] 
             (b_))) -> c, "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 13} -> 
     <|"Statement" -> HoldForm[
        ((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
           a) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a))) \[CenterDot] a)) \[CenterDot] a)) \[CenterDot] 
           (((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
               a)) \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
                  ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                a)) \[CenterDot] a)))) == (a \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          ((((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a))) \[CenterDot] a) \[CenterDot] 
            ((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
               a)) \[CenterDot] a)) \[CenterDot] 
           ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a))) \[CenterDot] a))], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 12}, "Orientation" -> 1, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           ((((a_) \[CenterDot] (b_)) \[CenterDot] (((b_) \[CenterDot] (
                (a_) \[CenterDot] (b_))) \[CenterDot] (b_))) \[CenterDot] 
            ((a_) \[CenterDot] (b_))) -> 
          ((b \[CenterDot] (a \[CenterDot] b)) \[CenterDot] b) \[CenterDot] 
           ((b \[CenterDot] ((b \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
              b)) \[CenterDot] (((b \[CenterDot] (a \[CenterDot] 
                b)) \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
              ((b \[CenterDot] (a \[CenterDot] b)) \[CenterDot] b)))), 
        "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] (b_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 5}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] (
                a_)) \[CenterDot] (a_)))) \[CenterDot] (a_) -> 
          a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"SubstitutionLemma", 10} -> 
     <|"Statement" -> HoldForm[
        ((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
           a) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a))) \[CenterDot] a)) \[CenterDot] a)) \[CenterDot] 
           (((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
               a)) \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
                  ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                a)) \[CenterDot] a)))) == (a \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            ((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
               a)) \[CenterDot] a)) \[CenterDot] 
           ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a))) \[CenterDot] a))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 13}, "Construct" -> 
         {"CriticalPairLemma", 5}, "Position" -> {2, 1, 1}, 
        "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) \[CenterDot] 
           (a_) -> a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          ((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
             a) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                     a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
               a)) \[CenterDot] (((a \[CenterDot] ((a \[CenterDot] 
                  (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                    a))) \[CenterDot] a)) \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                   (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                     a))) \[CenterDot] a)) \[CenterDot] a)))) == 
           (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a)) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                  (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                    a))) \[CenterDot] a)) \[CenterDot] a)) \[CenterDot] 
             ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                  a) \[CenterDot] a))) \[CenterDot] a))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 11} -> 
     <|"Statement" -> HoldForm[
        ((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
           a) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a))) \[CenterDot] a)) \[CenterDot] a)) \[CenterDot] 
           (((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
               a)) \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
                  ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                a)) \[CenterDot] a)))) == (a \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a))) \[CenterDot] a)) \[CenterDot] 
           ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a))) \[CenterDot] a))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 10}, "Construct" -> 
         {"CriticalPairLemma", 5}, "Position" -> {2, 1, 2, 1, 2}, 
        "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) \[CenterDot] 
           (a_) -> a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          ((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
             a) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                     a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
               a)) \[CenterDot] (((a \[CenterDot] ((a \[CenterDot] 
                  (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                    a))) \[CenterDot] a)) \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                   (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                     a))) \[CenterDot] a)) \[CenterDot] a)))) == 
           (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a)) \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
               a)) \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
                ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] a))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 12} -> 
     <|"Statement" -> HoldForm[
        ((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
           a) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a))) \[CenterDot] a)) \[CenterDot] a)) \[CenterDot] 
           (((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
               a)) \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
                  ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                a)) \[CenterDot] a)))) == (a \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a))) \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
              ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 11}, 
        "Construct" -> {"CriticalPairLemma", 5}, "Position" -> {2, 1, 2}, 
        "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) \[CenterDot] 
           (a_) -> a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          ((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
             a) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                     a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
               a)) \[CenterDot] (((a \[CenterDot] ((a \[CenterDot] 
                  (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                    a))) \[CenterDot] a)) \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                   (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                     a))) \[CenterDot] a)) \[CenterDot] a)))) == 
           (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] a))) \[CenterDot] ((a \[CenterDot] (
                a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                 a))) \[CenterDot] a))], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 14} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a))], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "Side" -> 1, "Subpattern" -> 
         ((a_) \[CenterDot] (b_)) \[CenterDot] (c_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 5}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) \[CenterDot] 
           (a_) -> a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"SubstitutionLemma", 13} -> 
     <|"Statement" -> HoldForm[
        ((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
           a) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a))) \[CenterDot] a)) \[CenterDot] a)) \[CenterDot] 
           (((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
               a)) \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
                  ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                a)) \[CenterDot] a)))) == (a \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] a))) \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 12}, 
        "Construct" -> {"CriticalPairLemma", 14}, "Position" -> {2, 1}, 
        "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (a_))) \[CenterDot] ((a_) \[CenterDot] 
            (((a_) \[CenterDot] (a_)) \[CenterDot] (a_))) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          ((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
             a) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                     a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
               a)) \[CenterDot] (((a \[CenterDot] ((a \[CenterDot] 
                  (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                    a))) \[CenterDot] a)) \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                   (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                     a))) \[CenterDot] a)) \[CenterDot] a)))) == 
           (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
                ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] a))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 14} -> 
     <|"Statement" -> HoldForm[
        ((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
           a) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a))) \[CenterDot] a)) \[CenterDot] a)) \[CenterDot] 
           (((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
               a)) \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
                  ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                a)) \[CenterDot] a)))) == (a \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 13}, 
        "Construct" -> {"CriticalPairLemma", 5}, "Position" -> {2, 2}, 
        "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) \[CenterDot] 
           (a_) -> a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          ((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
             a) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                     a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
               a)) \[CenterDot] (((a \[CenterDot] ((a \[CenterDot] 
                  (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                    a))) \[CenterDot] a)) \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                   (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                     a))) \[CenterDot] a)) \[CenterDot] a)))) == 
           (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 15} -> 
     <|"Statement" -> HoldForm[
        ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a))) \[CenterDot] a) \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
               a)) \[CenterDot] a)) \[CenterDot] 
           (((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
               a)) \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
                  ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                a)) \[CenterDot] a)))) == (a \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 14}, 
        "Construct" -> {"CriticalPairLemma", 5}, "Position" -> {1, 1, 2}, 
        "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) \[CenterDot] 
           (a_) -> a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a))) \[CenterDot] a) \[CenterDot] 
            ((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                  (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                    a))) \[CenterDot] a)) \[CenterDot] a)) \[CenterDot] 
             (((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                 a)) \[CenterDot] a) \[CenterDot] (a \[CenterDot] (
                (a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
                    ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                  a)) \[CenterDot] a)))) == (a \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 16} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
               a)) \[CenterDot] a)) \[CenterDot] 
           (((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
               a)) \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
                  ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                a)) \[CenterDot] a)))) == (a \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 15}, 
        "Construct" -> {"CriticalPairLemma", 5}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) \[CenterDot] 
           (a_) -> a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            ((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                  (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                    a))) \[CenterDot] a)) \[CenterDot] a)) \[CenterDot] 
             (((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                 a)) \[CenterDot] a) \[CenterDot] (a \[CenterDot] (
                (a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
                    ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                  a)) \[CenterDot] a)))) == (a \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 17} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
           (((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
               a)) \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
                  ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                a)) \[CenterDot] a)))) == (a \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 16}, 
        "Construct" -> {"CriticalPairLemma", 5}, "Position" -> {2, 1, 2, 1, 
         2}, "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) \[CenterDot] 
           (a_) -> a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            ((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
               a)) \[CenterDot] (((a \[CenterDot] ((a \[CenterDot] 
                  (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                    a))) \[CenterDot] a)) \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                   (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                     a))) \[CenterDot] a)) \[CenterDot] a)))) == 
           (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 18} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a))) \[CenterDot] (((a \[CenterDot] ((a \[CenterDot] 
                (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a))) \[CenterDot] a)) \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
                  ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                a)) \[CenterDot] a)))) == (a \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 17}, 
        "Construct" -> {"CriticalPairLemma", 5}, "Position" -> {2, 1, 2}, 
        "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) \[CenterDot] 
           (a_) -> a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a))) \[CenterDot] (((a \[CenterDot] ((a \[CenterDot] 
                  (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                    a))) \[CenterDot] a)) \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                   (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                     a))) \[CenterDot] a)) \[CenterDot] a)))) == 
           (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 19} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) == 
         (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 18}, 
        "Construct" -> {"CriticalPairLemma", 4}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] ((b_) \[CenterDot] 
             (((b_) \[CenterDot] (a_)) \[CenterDot] (b_)))) \[CenterDot] 
           ((((b_) \[CenterDot] (c_)) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] 
              (a_)))) -> b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) == 
           (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 20} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 19}, 
        "Construct" -> {"CriticalPairLemma", 14}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (a_))) \[CenterDot] ((a_) \[CenterDot] 
            (((a_) \[CenterDot] (a_)) \[CenterDot] (a_))) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a == (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a)) \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
              ((a \[CenterDot] a) \[CenterDot] a)))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 21} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] a == a \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 9}, 
        "Construct" -> {"SubstitutionLemma", 20}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (a_))) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[a \[CenterDot] a == 
           a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 22} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] a) == 
         (a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 5}, 
        "Construct" -> {"SubstitutionLemma", 21}, "Position" -> {1, 2}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CenterDot] 
            ((a \[CenterDot] a) \[CenterDot] a) == 
           (a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] a], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 23} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] a == 
         (a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 22}, 
        "Construct" -> {"SubstitutionLemma", 21}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CenterDot] a == 
           (a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] a], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 15} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
          (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
                c)) \[CenterDot] a))) \[CenterDot] (a \[CenterDot] b)) == 
         (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              c)) \[CenterDot] a)) \[CenterDot] 
          (((a \[CenterDot] b) \[CenterDot] c) \[CenterDot] 
           ((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
                c)) \[CenterDot] a)) \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] c)))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 7}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] 
           (((b_) \[CenterDot] (c_)) \[CenterDot] 
            ((((b_) \[CenterDot] (c_)) \[CenterDot] ((b_) \[CenterDot] (
                ((b_) \[CenterDot] (a_)) \[CenterDot] (b_)))) \[CenterDot] 
             ((b_) \[CenterDot] (c_)))) -> b \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] b), "Side" -> 1, 
        "Subpattern" -> ((b_) \[CenterDot] (c_)) \[CenterDot] 
          ((b_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] (b_))), 
        "MatchingConstruct" -> {"CriticalPairLemma", 7}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] 
            ((((b_) \[CenterDot] (c_)) \[CenterDot] ((b_) \[CenterDot] (
                ((b_) \[CenterDot] (a_)) \[CenterDot] (b_)))) \[CenterDot] 
             ((b_) \[CenterDot] (c_)))) -> b \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] b), "MatchingSide" -> 1, 
        "Position" -> {2, 2, 1}|>|>, {"CriticalPairLemma", 16} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] (
                (a \[CenterDot] a) \[CenterDot] a)))) \[CenterDot] 
            a)) \[CenterDot] (((a \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
           ((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a)))) \[CenterDot] a)) \[CenterDot] 
            ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] a))))) == (a \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a))) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
              a) \[CenterDot] a)))], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 15}, "Orientation" -> 1, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           ((((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
              (((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
                 (c_))) \[CenterDot] (a_)))) \[CenterDot] ((a_) \[CenterDot] 
             (b_))) -> (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                b) \[CenterDot] c)) \[CenterDot] a)) \[CenterDot] 
           (((a \[CenterDot] b) \[CenterDot] c) \[CenterDot] 
            ((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                  b) \[CenterDot] c)) \[CenterDot] a)) \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] c))), "Side" -> 1, 
        "Subpattern" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (c_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 14}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (a_))) \[CenterDot] ((a_) \[CenterDot] 
            (((a_) \[CenterDot] (a_)) \[CenterDot] (a_))) -> a, 
        "MatchingSide" -> 1, "Position" -> {2, 1, 2, 2, 1, 2}|>|>, 
    {"SubstitutionLemma", 24} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] (
                (a \[CenterDot] a) \[CenterDot] a)))) \[CenterDot] 
            a)) \[CenterDot] (((a \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
           ((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a)))) \[CenterDot] a)) \[CenterDot] 
            ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] a))))) == (a \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             a)))], "Proof" -> <|"Input" -> {"CriticalPairLemma", 16}, 
        "Construct" -> {"CriticalPairLemma", 14}, "Position" -> {2, 1}, 
        "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (a_))) \[CenterDot] ((a_) \[CenterDot] 
            (((a_) \[CenterDot] (a_)) \[CenterDot] (a_))) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a)))) \[CenterDot] 
              a)) \[CenterDot] (((a \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] (
                (a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
             ((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                  (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                    a)))) \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
                ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] (
                a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a))))) == 
           (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 25} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a))) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] (
                (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a)))) \[CenterDot] a)) \[CenterDot] 
            ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] a))))) == (a \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 24}, 
        "Construct" -> {"CriticalPairLemma", 14}, "Position" -> {1, 2, 1, 2}, 
        "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (a_))) \[CenterDot] ((a_) \[CenterDot] 
            (((a_) \[CenterDot] (a_)) \[CenterDot] (a_))) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] a))) \[CenterDot] ((a \[CenterDot] (
                (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                     a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a)))) \[CenterDot] 
                a)) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                  a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                ((a \[CenterDot] a) \[CenterDot] a))))) == 
           (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 26} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a)))) \[CenterDot] a)) \[CenterDot] 
            ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] a))))) == (a \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 25}, 
        "Construct" -> {"CriticalPairLemma", 14}, "Position" -> {2, 1}, 
        "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (a_))) \[CenterDot] ((a_) \[CenterDot] 
            (((a_) \[CenterDot] (a_)) \[CenterDot] (a_))) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                 ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                    a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                     a) \[CenterDot] a)))) \[CenterDot] a)) \[CenterDot] 
              ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                 a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                  a) \[CenterDot] a))))) == (a \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 27} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a)) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
              ((a \[CenterDot] a) \[CenterDot] a))))) == 
         (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 26}, 
        "Construct" -> {"CriticalPairLemma", 14}, "Position" -> {2, 2, 1, 2, 
         1, 2}, "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] 
              (a_)) \[CenterDot] (a_))) \[CenterDot] ((a_) \[CenterDot] 
            (((a_) \[CenterDot] (a_)) \[CenterDot] (a_))) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a)) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                  a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                ((a \[CenterDot] a) \[CenterDot] a))))) == 
           (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 28} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a)) \[CenterDot] a)) == (a \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 27}, 
        "Construct" -> {"CriticalPairLemma", 14}, "Position" -> {2, 2, 2}, 
        "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (a_))) \[CenterDot] ((a_) \[CenterDot] 
            (((a_) \[CenterDot] (a_)) \[CenterDot] (a_))) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a)) \[CenterDot] a)) == (a \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 29} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a)) \[CenterDot] a)) == a], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 28}, "Construct" -> 
         {"SubstitutionLemma", 20}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (a_))) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a)) \[CenterDot] a)) == a], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 30} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a)) \[CenterDot] a)) == a], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 29}, "Construct" -> 
         {"SubstitutionLemma", 21}, "Position" -> {1}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a)) \[CenterDot] a)) == a], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 31} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) == a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 30}, 
        "Construct" -> {"SubstitutionLemma", 21}, "Position" -> {2, 2, 1}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) == a], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 32} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] a) == a], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 31}, "Construct" -> 
         {"SubstitutionLemma", 21}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] a) == a], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 17} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] a) == ((a \[CenterDot] a) \[CenterDot] 
           a) \[CenterDot] (a \[CenterDot] a)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 23}, 
        "Orientation" -> -1, "Rule" -> 
         ((a_) \[CenterDot] ((a_) \[CenterDot] (a_))) \[CenterDot] (a_) -> 
          a \[CenterDot] a, "Side" -> 1, "Subpattern" -> 
         (a_) \[CenterDot] (a_), "MatchingConstruct" -> {"SubstitutionLemma", 
          32}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] (a_)) -> a, 
        "MatchingSide" -> 1, "Position" -> {1, 2}|>|>, 
    {"CriticalPairLemma", 18} -> 
     <|"Statement" -> HoldForm[
        c == (((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              a)) \[CenterDot] b) \[CenterDot] c) \[CenterDot] 
          (b \[CenterDot] ((b \[CenterDot] c) \[CenterDot] b))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 11}, 
        "Orientation" -> -1, "Rule" -> 
         (((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
              (a_))) \[CenterDot] (c_)) \[CenterDot] ((b_) \[CenterDot] 
            (((b_) \[CenterDot] (c_)) \[CenterDot] (b_))) -> c, "Side" -> 1, 
        "Subpattern" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] 
              (c_)) \[CenterDot] (a_))) -> c, "MatchingSide" -> 1, 
        "Position" -> {1, 1, 2}|>|>, {"CriticalPairLemma", 19} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] 
          ((b \[CenterDot] a) \[CenterDot] b) == a \[CenterDot] 
          (a \[CenterDot] ((a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                a) \[CenterDot] b))) \[CenterDot] a))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 18}, 
        "Orientation" -> -1, "Rule" -> 
         ((((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] (
                a_))) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((b_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] 
             (b_))) -> c, "Side" -> 1, "Subpattern" -> 
         (((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_))) \[CenterDot] (b_)) \[CenterDot] (c_), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] 
              (c_)) \[CenterDot] (a_))) -> c, "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 20} -> 
     <|"Statement" -> HoldForm[
        (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
          (((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
            a) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
             b))) == a \[CenterDot] (a \[CenterDot] 
           ((a \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                b)) \[CenterDot] a)) \[CenterDot] a))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 19}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
            (((a_) \[CenterDot] ((b_) \[CenterDot] (((b_) \[CenterDot] 
                 (a_)) \[CenterDot] (b_)))) \[CenterDot] (a_))) -> 
          b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), "Side" -> 1, 
        "Subpattern" -> ((b_) \[CenterDot] (a_)) \[CenterDot] (b_), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] 
              (c_)) \[CenterDot] (a_))) -> c, "MatchingSide" -> 1, 
        "Position" -> {2, 2, 1, 2, 2}|>|>, {"SubstitutionLemma", 33} -> 
     <|"Statement" -> HoldForm[
        (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
          a == a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
             ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                b)) \[CenterDot] a)) \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 20}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {2}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[(b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
              b)) \[CenterDot] a == a \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] 
                   a) \[CenterDot] b)) \[CenterDot] a)) \[CenterDot] a))], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 21} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          a == a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] a))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 33}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
            (((a_) \[CenterDot] (((b_) \[CenterDot] (((b_) \[CenterDot] 
                  (a_)) \[CenterDot] (b_))) \[CenterDot] (a_))) \[CenterDot] 
             (a_))) -> (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
             b)) \[CenterDot] a, "Side" -> 1, "Subpattern" -> 
         (b_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] (b_)), 
        "MatchingConstruct" -> {"SubstitutionLemma", 21}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)) -> 
          a \[CenterDot] a, "MatchingSide" -> 1, "Position" -> {2, 2, 1, 2, 
         1}|>|>, {"SubstitutionLemma", 34} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          a == a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
             a) \[CenterDot] a))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 21}, "Construct" -> 
         {"SubstitutionLemma", 21}, "Position" -> {2, 2, 1}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            a == a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] a))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 35} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          a == a \[CenterDot] (a \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 34}, 
        "Construct" -> {"SubstitutionLemma", 21}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            a == a \[CenterDot] (a \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 36} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] a == 
         a \[CenterDot] (a \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 35}, 
        "Construct" -> {"SubstitutionLemma", 21}, "Position" -> {1}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(a \[CenterDot] a) \[CenterDot] a == 
           a \[CenterDot] (a \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 37} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] a) == (a \[CenterDot] (a \[CenterDot] 
            a)) \[CenterDot] (a \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 17}, 
        "Construct" -> {"SubstitutionLemma", 36}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (a \[CenterDot] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] a) == (a \[CenterDot] (a \[CenterDot] 
              a)) \[CenterDot] (a \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 38} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] (a \[CenterDot] 
            a)) \[CenterDot] (a \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 37}, 
        "Construct" -> {"SubstitutionLemma", 32}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[a == (a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 22} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] (a \[CenterDot] 
            a)) \[CenterDot] (((a \[CenterDot] (a \[CenterDot] 
              a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] a))) == 
         ((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
            b) \[CenterDot] ((a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
               a))))) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
            ((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a))))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 3}, 
        "Orientation" -> -1, "Rule" -> 
         ((((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
            (((x3_) \[CenterDot] (a_)) \[CenterDot] ((((x3_) \[CenterDot] 
                (a_)) \[CenterDot] (b_)) \[CenterDot] ((x3_) \[CenterDot] (
                a_))))) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((b_) \[CenterDot] ((a_) \[CenterDot] (b_)))) -> 
          (x3 \[CenterDot] a) \[CenterDot] (((x3 \[CenterDot] a) \[CenterDot] 
             b) \[CenterDot] (x3 \[CenterDot] a)), "Side" -> 1, 
        "Subpattern" -> ((x3_) \[CenterDot] (a_)) \[CenterDot] (b_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 38}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] ((a_) \[CenterDot] (a_))) \[CenterDot] 
           ((a_) \[CenterDot] (a_)) -> a, "MatchingSide" -> 1, 
        "Position" -> {1, 2, 2, 1}|>|>, {"SubstitutionLemma", 39} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] (a \[CenterDot] 
            a)) \[CenterDot] (((a \[CenterDot] (a \[CenterDot] 
              a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] a))) == 
         ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
              a)) \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
              (a \[CenterDot] a))))) \[CenterDot] 
          (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] a))))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 22}, "Construct" -> 
         {"SubstitutionLemma", 32}, "Position" -> {1, 1, 1}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[(a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
            (((a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
              (a \[CenterDot] a))) == ((a \[CenterDot] b) \[CenterDot] 
             ((a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
              (a \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
                 a))))) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] (
                a \[CenterDot] a))))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 40} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] a == a \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 21}, 
        "Construct" -> {"SubstitutionLemma", 36}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (a \[CenterDot] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CenterDot] a == 
           a \[CenterDot] (a \[CenterDot] (a \[CenterDot] a))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 41} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] (a \[CenterDot] 
            a)) \[CenterDot] (((a \[CenterDot] (a \[CenterDot] 
              a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] a))) == 
         ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
              a)) \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
          (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] a))))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 39}, "Construct" -> 
         {"SubstitutionLemma", 40}, "Position" -> {1, 2, 2}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (a_))) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[
          (a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
            (((a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
              (a \[CenterDot] a))) == ((a \[CenterDot] b) \[CenterDot] 
             ((a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
              (a \[CenterDot] a))) \[CenterDot] 
            (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
               a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              ((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a))))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 42} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] (a \[CenterDot] 
            a)) \[CenterDot] (((a \[CenterDot] (a \[CenterDot] 
              a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] a))) == 
         ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
          (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] a))))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 41}, "Construct" -> 
         {"SubstitutionLemma", 38}, "Position" -> {1, 2}, 
        "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] (a_))) \[CenterDot] 
           ((a_) \[CenterDot] (a_)) -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[
          (a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
            (((a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
              (a \[CenterDot] a))) == ((a \[CenterDot] b) \[CenterDot] 
             a) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] (
                a \[CenterDot] a))))], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 23} -> 
     <|"Statement" -> HoldForm[
        b == (((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              a)) \[CenterDot] c) \[CenterDot] b) \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
           b)], "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "Side" -> 1, "Subpattern" -> 
         ((a_) \[CenterDot] (c_)) \[CenterDot] (a_), "MatchingConstruct" -> 
         {"Axiom", 1}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"CriticalPairLemma", 24} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           a)], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 23}, 
        "Orientation" -> -1, "Rule" -> 
         ((((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] (
                a_))) \[CenterDot] (c_)) \[CenterDot] (b_)) \[CenterDot] 
           (((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
              (a_))) \[CenterDot] (b_)) -> b, "Side" -> 1, 
        "Subpattern" -> ((a_) \[CenterDot] (((a_) \[CenterDot] 
             (b_)) \[CenterDot] (a_))) \[CenterDot] (c_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 14}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (a_))) \[CenterDot] ((a_) \[CenterDot] 
            (((a_) \[CenterDot] (a_)) \[CenterDot] (a_))) -> a, 
        "MatchingSide" -> 1, "Position" -> {1, 1}|>|>, 
    {"SubstitutionLemma", 43} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 24}, 
        "Construct" -> {"SubstitutionLemma", 21}, "Position" -> {2, 1}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
            ((a \[CenterDot] a) \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 44} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 43}, 
        "Construct" -> {"SubstitutionLemma", 36}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (a \[CenterDot] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] a))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 45} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] (a \[CenterDot] 
            a)) \[CenterDot] (((a \[CenterDot] (a \[CenterDot] 
              a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] a))) == 
         ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] a)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 42}, "Construct" -> 
         {"SubstitutionLemma", 44}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            ((a_) \[CenterDot] (a_))) -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[
          (a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
            (((a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
              (a \[CenterDot] a))) == ((a \[CenterDot] b) \[CenterDot] 
             a) \[CenterDot] (a \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 46} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] (a \[CenterDot] 
            a)) \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
            (a \[CenterDot] a))) == ((a \[CenterDot] b) \[CenterDot] 
           a) \[CenterDot] (a \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 45}, 
        "Construct" -> {"SubstitutionLemma", 38}, "Position" -> {2, 1}, 
        "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] (a_))) \[CenterDot] 
           ((a_) \[CenterDot] (a_)) -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          (a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] (a \[CenterDot] a))) == 
           ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 47} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] (a \[CenterDot] 
            a)) \[CenterDot] (a \[CenterDot] a) == 
         ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] a)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 46}, "Construct" -> 
         {"SubstitutionLemma", 40}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (a_))) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          (a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] a) == ((a \[CenterDot] b) \[CenterDot] 
             a) \[CenterDot] (a \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 48} -> 
     <|"Statement" -> HoldForm[a == ((a \[CenterDot] b) \[CenterDot] 
           a) \[CenterDot] (a \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 47}, 
        "Construct" -> {"SubstitutionLemma", 38}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] (a_))) \[CenterDot] 
           ((a_) \[CenterDot] (a_)) -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a == ((a \[CenterDot] b) \[CenterDot] 
             a) \[CenterDot] (a \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 25} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] a == 
         (((a \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
           (a \[CenterDot] a)) \[CenterDot] a], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 48}, 
        "Orientation" -> -1, "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (a_)) \[CenterDot] ((a_) \[CenterDot] (a_)) -> a, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (a_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 32}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (a_)) -> a, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 26} -> 
     <|"Statement" -> HoldForm[
        ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           (((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b)) \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
             ((b \[CenterDot] a) \[CenterDot] b)))) \[CenterDot] a == 
         a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
             (((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                 b)) \[CenterDot] a) \[CenterDot] a)) \[CenterDot] a))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 33}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
            (((a_) \[CenterDot] (((b_) \[CenterDot] (((b_) \[CenterDot] 
                  (a_)) \[CenterDot] (b_))) \[CenterDot] (a_))) \[CenterDot] 
             (a_))) -> (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
             b)) \[CenterDot] a, "Side" -> 1, "Subpattern" -> 
         ((b_) \[CenterDot] (a_)) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"Axiom", 1}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "MatchingSide" -> 1, "Position" -> {2, 2, 1, 2, 1, 
         2}|>|>, {"SubstitutionLemma", 49} -> 
     <|"Statement" -> HoldForm[
        ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           a) \[CenterDot] a == a \[CenterDot] (a \[CenterDot] 
           ((a \[CenterDot] (((b \[CenterDot] ((b \[CenterDot] 
                  a) \[CenterDot] b)) \[CenterDot] a) \[CenterDot] 
              a)) \[CenterDot] a))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 26}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {1, 2}, "Rule" -> 
         (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b)) \[CenterDot] a) \[CenterDot] a == a \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] (((b \[CenterDot] 
                  ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
                 a) \[CenterDot] a)) \[CenterDot] a))], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 27} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          (((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
            a) \[CenterDot] a)], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 1}, "Orientation" -> -1, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           ((((c_) \[CenterDot] (x3_)) \[CenterDot] (a_)) \[CenterDot] 
            (((((c_) \[CenterDot] (x3_)) \[CenterDot] (a_)) \[CenterDot] 
              (b_)) \[CenterDot] (((c_) \[CenterDot] (x3_)) \[CenterDot] 
              (a_)))) -> b, "Side" -> 1, "Subpattern" -> 
         ((((c_) \[CenterDot] (x3_)) \[CenterDot] (a_)) \[CenterDot] 
           (b_)) \[CenterDot] (((c_) \[CenterDot] (x3_)) \[CenterDot] (a_)), 
        "MatchingConstruct" -> {"CriticalPairLemma", 23}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] (
                a_))) \[CenterDot] (c_)) \[CenterDot] (b_)) \[CenterDot] 
           (((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
              (a_))) \[CenterDot] (b_)) -> b, "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 28} -> 
     <|"Statement" -> HoldForm[
        ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           a) \[CenterDot] a == (a \[CenterDot] 
           (((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b)) \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
            (a \[CenterDot] a)))], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 2}, "Orientation" -> -1, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((c_) \[CenterDot] (a_)) \[CenterDot] 
            ((((c_) \[CenterDot] (a_)) \[CenterDot] (b_)) \[CenterDot] 
             ((c_) \[CenterDot] (a_)))) -> b, "Side" -> 1, 
        "Subpattern" -> ((c_) \[CenterDot] (a_)) \[CenterDot] (b_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 27}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] (a_)) \[CenterDot] 
           ((((b_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] (
                b_))) \[CenterDot] (a_)) \[CenterDot] (a_)) -> a, 
        "MatchingSide" -> 1, "Position" -> {2, 2, 1}|>|>, 
    {"SubstitutionLemma", 50} -> 
     <|"Statement" -> HoldForm[
        ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           a) \[CenterDot] a == (a \[CenterDot] 
           (((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b)) \[CenterDot] a) \[CenterDot] a)) \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 28}, 
        "Construct" -> {"SubstitutionLemma", 44}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            ((a_) \[CenterDot] (a_))) -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[
          ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
             a) \[CenterDot] a == (a \[CenterDot] (((b \[CenterDot] 
                ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
               a) \[CenterDot] a)) \[CenterDot] a], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 51} -> 
     <|"Statement" -> HoldForm[
        ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           a) \[CenterDot] a == a \[CenterDot] (a \[CenterDot] 
           (((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b)) \[CenterDot] a) \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 49}, 
        "Construct" -> {"SubstitutionLemma", 50}, "Position" -> {2, 2}, 
        "Rule" -> ((a_) \[CenterDot] ((((b_) \[CenterDot] (((b_) \[CenterDot] 
                 (a_)) \[CenterDot] (b_))) \[CenterDot] (a_)) \[CenterDot] 
             (a_))) \[CenterDot] (a_) -> 
          ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
            a) \[CenterDot] a, "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b)) \[CenterDot] a) \[CenterDot] a == a \[CenterDot] 
            (a \[CenterDot] (((b \[CenterDot] ((b \[CenterDot] 
                  a) \[CenterDot] b)) \[CenterDot] a) \[CenterDot] a))], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 29} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] 
          ((b \[CenterDot] a) \[CenterDot] b) == a \[CenterDot] 
          ((b \[CenterDot] ((b \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                 a) \[CenterDot] b))) \[CenterDot] b)) \[CenterDot] 
           (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 7}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] 
           (((b_) \[CenterDot] (c_)) \[CenterDot] 
            ((((b_) \[CenterDot] (c_)) \[CenterDot] ((b_) \[CenterDot] (
                ((b_) \[CenterDot] (a_)) \[CenterDot] (b_)))) \[CenterDot] 
             ((b_) \[CenterDot] (c_)))) -> b \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] b), "Side" -> 1, 
        "Subpattern" -> (((b_) \[CenterDot] (c_)) \[CenterDot] 
           ((b_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] 
             (b_)))) \[CenterDot] ((b_) \[CenterDot] (c_)), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] 
              (c_)) \[CenterDot] (a_))) -> c, "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 30} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
          (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
           (a \[CenterDot] b)) == (a \[CenterDot] b) \[CenterDot] 
          (b \[CenterDot] ((b \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] 
                 b) \[CenterDot] (a \[CenterDot] b))))) \[CenterDot] b))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 6}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            (((b_) \[CenterDot] (((c_) \[CenterDot] (b_)) \[CenterDot] (
                (((c_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
                ((c_) \[CenterDot] (b_))))) \[CenterDot] (b_))) -> 
          (c \[CenterDot] b) \[CenterDot] (((c \[CenterDot] b) \[CenterDot] 
             a) \[CenterDot] (c \[CenterDot] b)), "Side" -> 1, 
        "Subpattern" -> (((c_) \[CenterDot] (b_)) \[CenterDot] 
           (a_)) \[CenterDot] ((c_) \[CenterDot] (b_)), 
        "MatchingConstruct" -> {"SubstitutionLemma", 36}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((a_) \[CenterDot] (a_)) \[CenterDot] (a_) -> a \[CenterDot] 
           (a \[CenterDot] a), "MatchingSide" -> 1, "Position" -> {2, 2, 1, 
         2, 2}|>|>, {"SubstitutionLemma", 52} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
          (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
           (a \[CenterDot] b)) == (a \[CenterDot] b) \[CenterDot] 
          (b \[CenterDot] ((b \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] b))) \[CenterDot] b))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 30}, 
        "Construct" -> {"SubstitutionLemma", 40}, "Position" -> {2, 2, 1, 2}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (a_))) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
            (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
               b)) \[CenterDot] (a \[CenterDot] b)) == 
           (a \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
             ((b \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
                (a \[CenterDot] b))) \[CenterDot] b))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 53} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
          ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b))) == (a \[CenterDot] b) \[CenterDot] 
          (b \[CenterDot] ((b \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] b))) \[CenterDot] b))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 52}, 
        "Construct" -> {"SubstitutionLemma", 36}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (a \[CenterDot] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] b))) == (a \[CenterDot] b) \[CenterDot] 
            (b \[CenterDot] ((b \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
                (a \[CenterDot] b))) \[CenterDot] b))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 54} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
          (a \[CenterDot] b) == (a \[CenterDot] b) \[CenterDot] 
          (b \[CenterDot] ((b \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] b))) \[CenterDot] b))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 53}, 
        "Construct" -> {"SubstitutionLemma", 40}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (a_))) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b) == (a \[CenterDot] b) \[CenterDot] 
            (b \[CenterDot] ((b \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
                (a \[CenterDot] b))) \[CenterDot] b))], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 31} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
          (((b \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                (b \[CenterDot] a))) \[CenterDot] a))) \[CenterDot] 
           (b \[CenterDot] a)) == (a \[CenterDot] 
           ((a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
              (b \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
          (((b \[CenterDot] a) \[CenterDot] (((b \[CenterDot] a) \[CenterDot] 
              ((b \[CenterDot] a) \[CenterDot] (((b \[CenterDot] 
                  a) \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
                (b \[CenterDot] a)))) \[CenterDot] (b \[CenterDot] 
              a))) \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
            (((b \[CenterDot] a) \[CenterDot] (a \[CenterDot] (
                (a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                  (b \[CenterDot] a))) \[CenterDot] a))) \[CenterDot] 
             (b \[CenterDot] a))))], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 29}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CenterDot] (((b_) \[CenterDot] 
             (((b_) \[CenterDot] ((b_) \[CenterDot] (((b_) \[CenterDot] 
                  (a_)) \[CenterDot] (b_)))) \[CenterDot] (b_))) \[CenterDot] 
            ((b_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] 
              (b_)))) -> b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), 
        "Side" -> 1, "Subpattern" -> (b_) \[CenterDot] (a_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 54}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
            (((b_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] (
                (a_) \[CenterDot] (b_)))) \[CenterDot] (b_))) -> 
          (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b), 
        "MatchingSide" -> 1, "Position" -> {2, 1, 2, 1, 2, 2, 1}|>|>, 
    {"SubstitutionLemma", 55} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
          (((b \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                (b \[CenterDot] a))) \[CenterDot] a))) \[CenterDot] 
           (b \[CenterDot] a)) == (a \[CenterDot] 
           ((a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
              (b \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
          (((b \[CenterDot] a) \[CenterDot] (((b \[CenterDot] a) \[CenterDot] 
              ((b \[CenterDot] a) \[CenterDot] ((b \[CenterDot] 
                 a) \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                 (b \[CenterDot] a))))) \[CenterDot] (b \[CenterDot] 
              a))) \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
            (((b \[CenterDot] a) \[CenterDot] (a \[CenterDot] (
                (a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                  (b \[CenterDot] a))) \[CenterDot] a))) \[CenterDot] 
             (b \[CenterDot] a))))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 31}, "Construct" -> 
         {"SubstitutionLemma", 36}, "Position" -> {2, 1, 2, 1, 2, 2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (a \[CenterDot] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
            (((b \[CenterDot] a) \[CenterDot] (a \[CenterDot] (
                (a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                  (b \[CenterDot] a))) \[CenterDot] a))) \[CenterDot] 
             (b \[CenterDot] a)) == (a \[CenterDot] ((a \[CenterDot] (
                (b \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
                 a))) \[CenterDot] a)) \[CenterDot] 
            (((b \[CenterDot] a) \[CenterDot] (((b \[CenterDot] 
                 a) \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                 ((b \[CenterDot] a) \[CenterDot] ((b \[CenterDot] 
                    a) \[CenterDot] (b \[CenterDot] a))))) \[CenterDot] (
                b \[CenterDot] a))) \[CenterDot] ((b \[CenterDot] 
               a) \[CenterDot] (((b \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] ((a \[CenterDot] ((b \[CenterDot] 
                     a) \[CenterDot] (b \[CenterDot] a))) \[CenterDot] 
                  a))) \[CenterDot] (b \[CenterDot] a))))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 56} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
          (((b \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                (b \[CenterDot] a))) \[CenterDot] a))) \[CenterDot] 
           (b \[CenterDot] a)) == (a \[CenterDot] 
           ((a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
              (b \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
          (((b \[CenterDot] a) \[CenterDot] (((b \[CenterDot] a) \[CenterDot] 
              ((b \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
                a))) \[CenterDot] (b \[CenterDot] a))) \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] (((b \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] ((a \[CenterDot] ((b \[CenterDot] 
                   a) \[CenterDot] (b \[CenterDot] a))) \[CenterDot] 
                a))) \[CenterDot] (b \[CenterDot] a))))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 55}, 
        "Construct" -> {"SubstitutionLemma", 40}, "Position" -> {2, 1, 2, 1, 
         2}, "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
            ((a_) \[CenterDot] (a_))) -> a \[CenterDot] a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (b \[CenterDot] a) \[CenterDot] (((b \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] ((a \[CenterDot] ((b \[CenterDot] 
                   a) \[CenterDot] (b \[CenterDot] a))) \[CenterDot] 
                a))) \[CenterDot] (b \[CenterDot] a)) == 
           (a \[CenterDot] ((a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                (b \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
            (((b \[CenterDot] a) \[CenterDot] (((b \[CenterDot] 
                 a) \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                 (b \[CenterDot] a))) \[CenterDot] (b \[CenterDot] 
                a))) \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
              (((b \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                    (b \[CenterDot] a))) \[CenterDot] a))) \[CenterDot] (
                b \[CenterDot] a))))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 57} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
          (((b \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                (b \[CenterDot] a))) \[CenterDot] a))) \[CenterDot] 
           (b \[CenterDot] a)) == (a \[CenterDot] 
           ((a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
              (b \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
          (((b \[CenterDot] a) \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
             (b \[CenterDot] a))) \[CenterDot] ((b \[CenterDot] 
             a) \[CenterDot] (((b \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] ((a \[CenterDot] ((b \[CenterDot] 
                   a) \[CenterDot] (b \[CenterDot] a))) \[CenterDot] 
                a))) \[CenterDot] (b \[CenterDot] a))))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 56}, 
        "Construct" -> {"SubstitutionLemma", 23}, "Position" -> {2, 1, 2}, 
        "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] (a_))) \[CenterDot] 
           (a_) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
            (((b \[CenterDot] a) \[CenterDot] (a \[CenterDot] (
                (a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                  (b \[CenterDot] a))) \[CenterDot] a))) \[CenterDot] 
             (b \[CenterDot] a)) == (a \[CenterDot] ((a \[CenterDot] (
                (b \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
                 a))) \[CenterDot] a)) \[CenterDot] 
            (((b \[CenterDot] a) \[CenterDot] ((b \[CenterDot] 
                a) \[CenterDot] (b \[CenterDot] a))) \[CenterDot] 
             ((b \[CenterDot] a) \[CenterDot] (((b \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                   ((b \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
                     a))) \[CenterDot] a))) \[CenterDot] (b \[CenterDot] 
                a))))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 58} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
          (((b \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                (b \[CenterDot] a))) \[CenterDot] a))) \[CenterDot] 
           (b \[CenterDot] a)) == (a \[CenterDot] 
           ((a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
              (b \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
          (((b \[CenterDot] a) \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
             (b \[CenterDot] a))) \[CenterDot] ((b \[CenterDot] 
             a) \[CenterDot] (((b \[CenterDot] a) \[CenterDot] 
              (b \[CenterDot] a)) \[CenterDot] (b \[CenterDot] a))))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 57}, 
        "Construct" -> {"SubstitutionLemma", 54}, "Position" -> {2, 2, 2, 1}, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
            (((b_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] (
                (a_) \[CenterDot] (b_)))) \[CenterDot] (b_))) -> 
          (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (b \[CenterDot] a) \[CenterDot] (((b \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] ((a \[CenterDot] ((b \[CenterDot] 
                   a) \[CenterDot] (b \[CenterDot] a))) \[CenterDot] 
                a))) \[CenterDot] (b \[CenterDot] a)) == 
           (a \[CenterDot] ((a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                (b \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
            (((b \[CenterDot] a) \[CenterDot] ((b \[CenterDot] 
                a) \[CenterDot] (b \[CenterDot] a))) \[CenterDot] 
             ((b \[CenterDot] a) \[CenterDot] (((b \[CenterDot] 
                 a) \[CenterDot] (b \[CenterDot] a)) \[CenterDot] (
                b \[CenterDot] a))))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 59} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
          (((b \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                (b \[CenterDot] a))) \[CenterDot] a))) \[CenterDot] 
           (b \[CenterDot] a)) == (a \[CenterDot] 
           ((a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
              (b \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
          (((b \[CenterDot] a) \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
             (b \[CenterDot] a))) \[CenterDot] ((b \[CenterDot] 
             a) \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
             ((b \[CenterDot] a) \[CenterDot] (b \[CenterDot] a)))))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 58}, 
        "Construct" -> {"SubstitutionLemma", 36}, "Position" -> {2, 2, 2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (a \[CenterDot] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
            (((b \[CenterDot] a) \[CenterDot] (a \[CenterDot] (
                (a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                  (b \[CenterDot] a))) \[CenterDot] a))) \[CenterDot] 
             (b \[CenterDot] a)) == (a \[CenterDot] ((a \[CenterDot] (
                (b \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
                 a))) \[CenterDot] a)) \[CenterDot] 
            (((b \[CenterDot] a) \[CenterDot] ((b \[CenterDot] 
                a) \[CenterDot] (b \[CenterDot] a))) \[CenterDot] 
             ((b \[CenterDot] a) \[CenterDot] ((b \[CenterDot] 
                a) \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                (b \[CenterDot] a)))))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 60} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
          (((b \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                (b \[CenterDot] a))) \[CenterDot] a))) \[CenterDot] 
           (b \[CenterDot] a)) == (a \[CenterDot] 
           ((a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
              (b \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
          (((b \[CenterDot] a) \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
             (b \[CenterDot] a))) \[CenterDot] ((b \[CenterDot] 
             a) \[CenterDot] (b \[CenterDot] a)))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 59}, 
        "Construct" -> {"SubstitutionLemma", 40}, "Position" -> {2, 2}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (a_))) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
            (((b \[CenterDot] a) \[CenterDot] (a \[CenterDot] (
                (a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                  (b \[CenterDot] a))) \[CenterDot] a))) \[CenterDot] 
             (b \[CenterDot] a)) == (a \[CenterDot] ((a \[CenterDot] (
                (b \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
                 a))) \[CenterDot] a)) \[CenterDot] 
            (((b \[CenterDot] a) \[CenterDot] ((b \[CenterDot] 
                a) \[CenterDot] (b \[CenterDot] a))) \[CenterDot] 
             ((b \[CenterDot] a) \[CenterDot] (b \[CenterDot] a)))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 61} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
          (((b \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                (b \[CenterDot] a))) \[CenterDot] a))) \[CenterDot] 
           (b \[CenterDot] a)) == (a \[CenterDot] 
           ((a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
              (b \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
          (b \[CenterDot] a)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 60}, "Construct" -> 
         {"SubstitutionLemma", 38}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] (a_))) \[CenterDot] 
           ((a_) \[CenterDot] (a_)) -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
            (((b \[CenterDot] a) \[CenterDot] (a \[CenterDot] (
                (a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                  (b \[CenterDot] a))) \[CenterDot] a))) \[CenterDot] 
             (b \[CenterDot] a)) == (a \[CenterDot] ((a \[CenterDot] (
                (b \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
                 a))) \[CenterDot] a)) \[CenterDot] (b \[CenterDot] a)], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 62} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
          (((b \[CenterDot] a) \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
           (b \[CenterDot] a)) == (a \[CenterDot] 
           ((a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
              (b \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
          (b \[CenterDot] a)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 61}, "Construct" -> 
         {"SubstitutionLemma", 54}, "Position" -> {2, 1}, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
            (((b_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] (
                (a_) \[CenterDot] (b_)))) \[CenterDot] (b_))) -> 
          (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (b \[CenterDot] a) \[CenterDot] (((b \[CenterDot] a) \[CenterDot] 
              (b \[CenterDot] a)) \[CenterDot] (b \[CenterDot] a)) == 
           (a \[CenterDot] ((a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                (b \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
            (b \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 63} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
          ((b \[CenterDot] a) \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
            (b \[CenterDot] a))) == (a \[CenterDot] 
           ((a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
              (b \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
          (b \[CenterDot] a)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 62}, "Construct" -> 
         {"SubstitutionLemma", 36}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (a \[CenterDot] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
            ((b \[CenterDot] a) \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
              (b \[CenterDot] a))) == (a \[CenterDot] 
             ((a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                (b \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
            (b \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 64} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
          (b \[CenterDot] a) == (a \[CenterDot] 
           ((a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
              (b \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
          (b \[CenterDot] a)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 63}, "Construct" -> 
         {"SubstitutionLemma", 40}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (a_))) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
            (b \[CenterDot] a) == (a \[CenterDot] ((a \[CenterDot] (
                (b \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
                 a))) \[CenterDot] a)) \[CenterDot] (b \[CenterDot] a)], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 32} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] b == 
         ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
          (b \[CenterDot] ((b \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
            b))], "Proof" -> <|"Construct" -> {"Axiom", 1}, 
        "Orientation" -> 1, "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] 
              (c_)) \[CenterDot] (a_))) -> c, "Side" -> 1, 
        "Subpattern" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (c_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 64}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] (((a_) \[CenterDot] (((b_) \[CenterDot] 
                (a_)) \[CenterDot] ((b_) \[CenterDot] (a_)))) \[CenterDot] 
             (a_))) \[CenterDot] ((b_) \[CenterDot] (a_)) -> 
          (b \[CenterDot] a) \[CenterDot] (b \[CenterDot] a), 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 33} -> 
     <|"Statement" -> HoldForm[((b \[CenterDot] c) \[CenterDot] 
           a) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
            b)) == (a \[CenterDot] (((b \[CenterDot] c) \[CenterDot] 
             a) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
              b)))) \[CenterDot] ((b \[CenterDot] 
            ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           (((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b)) \[CenterDot] (((b \[CenterDot] c) \[CenterDot] 
               a) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                 a) \[CenterDot] b)))) \[CenterDot] (b \[CenterDot] 
             ((b \[CenterDot] a) \[CenterDot] b))))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 32}, 
        "Orientation" -> -1, "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((a_) \[CenterDot] (b_))) \[CenterDot] ((b_) \[CenterDot] 
            (((b_) \[CenterDot] ((a_) \[CenterDot] (b_))) \[CenterDot] 
             (b_))) -> a \[CenterDot] b, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"Axiom", 1}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "MatchingSide" -> 1, "Position" -> {1, 1}|>|>, 
    {"SubstitutionLemma", 65} -> 
     <|"Statement" -> HoldForm[((b \[CenterDot] c) \[CenterDot] 
           a) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
            b)) == (a \[CenterDot] a) \[CenterDot] 
          ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           (((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b)) \[CenterDot] (((b \[CenterDot] c) \[CenterDot] 
               a) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                 a) \[CenterDot] b)))) \[CenterDot] (b \[CenterDot] 
             ((b \[CenterDot] a) \[CenterDot] b))))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 33}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {1, 2}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
            (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) == 
           (a \[CenterDot] a) \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] 
                a) \[CenterDot] b)) \[CenterDot] (((b \[CenterDot] 
                ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] (
                ((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
                (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                  b)))) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                 a) \[CenterDot] b))))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 66} -> 
     <|"Statement" -> HoldForm[((b \[CenterDot] c) \[CenterDot] 
           a) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
            b)) == (a \[CenterDot] a) \[CenterDot] 
          ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           (((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b)) \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
             ((b \[CenterDot] a) \[CenterDot] b))))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 65}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {2, 2, 1, 2}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
            (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) == 
           (a \[CenterDot] a) \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] 
                a) \[CenterDot] b)) \[CenterDot] (((b \[CenterDot] 
                ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
               a) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                 a) \[CenterDot] b))))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 67} -> 
     <|"Statement" -> HoldForm[((b \[CenterDot] c) \[CenterDot] 
           a) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
            b)) == (a \[CenterDot] a) \[CenterDot] 
          ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           a)], "Proof" -> <|"Input" -> {"SubstitutionLemma", 66}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {2, 2}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
            (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) == 
           (a \[CenterDot] a) \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] 
                a) \[CenterDot] b)) \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 68} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           a)], "Proof" -> <|"Input" -> {"SubstitutionLemma", 67}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
            ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b)) \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 34} -> 
     <|"Statement" -> HoldForm[
        (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
          a == (a \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] 
               a) \[CenterDot] b)) \[CenterDot] a)) \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
            (a \[CenterDot] a)))], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 2}, "Orientation" -> -1, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((c_) \[CenterDot] (a_)) \[CenterDot] 
            ((((c_) \[CenterDot] (a_)) \[CenterDot] (b_)) \[CenterDot] 
             ((c_) \[CenterDot] (a_)))) -> b, "Side" -> 1, 
        "Subpattern" -> ((c_) \[CenterDot] (a_)) \[CenterDot] (b_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 68}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] (a_)) \[CenterDot] (((b_) \[CenterDot] 
             (((b_) \[CenterDot] (a_)) \[CenterDot] (b_))) \[CenterDot] 
            (a_)) -> a, "MatchingSide" -> 1, "Position" -> {2, 2, 1}|>|>, 
    {"SubstitutionLemma", 69} -> 
     <|"Statement" -> HoldForm[
        (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
          a == (a \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] 
               a) \[CenterDot] b)) \[CenterDot] a)) \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 34}, 
        "Construct" -> {"SubstitutionLemma", 44}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            ((a_) \[CenterDot] (a_))) -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[
          (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
            a == (a \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] 
                 a) \[CenterDot] b)) \[CenterDot] a)) \[CenterDot] a], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 70} -> 
     <|"Statement" -> HoldForm[
        (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
          a == a \[CenterDot] (a \[CenterDot] ((b \[CenterDot] 
             ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 33}, 
        "Construct" -> {"SubstitutionLemma", 69}, "Position" -> {2, 2}, 
        "Rule" -> ((a_) \[CenterDot] (((b_) \[CenterDot] (((b_) \[CenterDot] 
                (a_)) \[CenterDot] (b_))) \[CenterDot] (a_))) \[CenterDot] 
           (a_) -> (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
             b)) \[CenterDot] a, "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[(b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
              b)) \[CenterDot] a == a \[CenterDot] (a \[CenterDot] 
             ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                b)) \[CenterDot] a))], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 35} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] a) == 
         ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
            ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
             a))) \[CenterDot] (a \[CenterDot] b)], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 4}, 
        "Orientation" -> -1, "Rule" -> 
         ((a_) \[CenterDot] ((b_) \[CenterDot] (((b_) \[CenterDot] (
                a_)) \[CenterDot] (b_)))) \[CenterDot] 
           ((((b_) \[CenterDot] (c_)) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] 
              (a_)))) -> b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), 
        "Side" -> 1, "Subpattern" -> (((b_) \[CenterDot] (c_)) \[CenterDot] 
           (a_)) \[CenterDot] ((a_) \[CenterDot] (((b_) \[CenterDot] 
             (c_)) \[CenterDot] (a_))), "MatchingConstruct" -> 
         {"SubstitutionLemma", 44}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] ((a_) \[CenterDot] (a_))) -> a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 36} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] b == 
         (a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
            a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
           (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
            (a \[CenterDot] b)))], "Proof" -> <|"Construct" -> {"Axiom", 1}, 
        "Orientation" -> 1, "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] 
              (c_)) \[CenterDot] (a_))) -> c, "Side" -> 1, 
        "Subpattern" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (c_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 35}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] ((a_) \[CenterDot] (b_))) \[CenterDot] 
              (a_)))) \[CenterDot] ((a_) \[CenterDot] (b_)) -> 
          a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
            a), "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"SubstitutionLemma", 71} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] b == 
         (a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
            a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] b))))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 36}, "Construct" -> 
         {"SubstitutionLemma", 36}, "Position" -> {2, 2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (a \[CenterDot] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CenterDot] b == 
           (a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
              a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] 
                b) \[CenterDot] (a \[CenterDot] b))))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 72} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] b == 
         (a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
            a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
           (a \[CenterDot] b))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 71}, "Construct" -> 
         {"SubstitutionLemma", 40}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (a_))) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CenterDot] b == 
           (a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
              a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] b))], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 37} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
          (a \[CenterDot] b) == (a \[CenterDot] b) \[CenterDot] 
          (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] b))) \[CenterDot] a))], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "Side" -> 1, "Subpattern" -> 
         ((a_) \[CenterDot] (b_)) \[CenterDot] (c_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 72}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] 
              ((a_) \[CenterDot] (b_))) \[CenterDot] (a_))) \[CenterDot] 
           (((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
             (b_))) -> a \[CenterDot] b, "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 38} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] b))) \[CenterDot] a) == 
         ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
          ((a \[CenterDot] b) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] b)) \[CenterDot] (a \[CenterDot] b)))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 7}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] 
           (((b_) \[CenterDot] (c_)) \[CenterDot] 
            ((((b_) \[CenterDot] (c_)) \[CenterDot] ((b_) \[CenterDot] (
                ((b_) \[CenterDot] (a_)) \[CenterDot] (b_)))) \[CenterDot] 
             ((b_) \[CenterDot] (c_)))) -> b \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] b), "Side" -> 1, 
        "Subpattern" -> ((b_) \[CenterDot] (c_)) \[CenterDot] 
          ((b_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] (b_))), 
        "MatchingConstruct" -> {"CriticalPairLemma", 37}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
            (((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] (
                (a_) \[CenterDot] (b_)))) \[CenterDot] (a_))) -> 
          (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b), 
        "MatchingSide" -> 1, "Position" -> {2, 2, 1}|>|>, 
    {"SubstitutionLemma", 73} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] b))) \[CenterDot] a) == 
         ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
          ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 38}, 
        "Construct" -> {"SubstitutionLemma", 36}, "Position" -> {2, 2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (a \[CenterDot] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CenterDot] 
            ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                b))) \[CenterDot] a) == ((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] b)) \[CenterDot] ((a \[CenterDot] 
              b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 74} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] b))) \[CenterDot] a) == 
         ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
          ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 73}, 
        "Construct" -> {"SubstitutionLemma", 40}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (a_))) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CenterDot] 
            ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                b))) \[CenterDot] a) == ((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] b)) \[CenterDot] ((a \[CenterDot] 
              b) \[CenterDot] (a \[CenterDot] b))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 75} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] b))) \[CenterDot] a) == a \[CenterDot] b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 74}, 
        "Construct" -> {"SubstitutionLemma", 32}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                b) \[CenterDot] (a \[CenterDot] b))) \[CenterDot] a) == 
           a \[CenterDot] b], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 39} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((b \[CenterDot] c) \[CenterDot] (((b \[CenterDot] c) \[CenterDot] 
             (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b))) \[CenterDot] (b \[CenterDot] c))) == 
         a \[CenterDot] ((a \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] 
                a) \[CenterDot] b)) \[CenterDot] (a \[CenterDot] 
              ((b \[CenterDot] c) \[CenterDot] (((b \[CenterDot] 
                  c) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                    a) \[CenterDot] b))) \[CenterDot] (b \[CenterDot] 
                 c)))))) \[CenterDot] a)], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 75}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] 
             (((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] (
                b_)))) \[CenterDot] (a_)) -> a \[CenterDot] b, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 7}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CenterDot] (((b_) \[CenterDot] 
             (c_)) \[CenterDot] ((((b_) \[CenterDot] (c_)) \[CenterDot] 
              ((b_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] 
                (b_)))) \[CenterDot] ((b_) \[CenterDot] (c_)))) -> 
          b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), 
        "MatchingSide" -> 1, "Position" -> {2, 1, 2, 1}|>|>, 
    {"SubstitutionLemma", 76} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((b \[CenterDot] c) \[CenterDot] (((b \[CenterDot] c) \[CenterDot] 
             (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b))) \[CenterDot] (b \[CenterDot] c))) == 
         a \[CenterDot] ((a \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] 
                a) \[CenterDot] b)) \[CenterDot] (b \[CenterDot] 
              ((b \[CenterDot] a) \[CenterDot] b)))) \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 39}, 
        "Construct" -> {"CriticalPairLemma", 7}, "Position" -> {2, 1, 2, 2}, 
        "Rule" -> (a_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] 
            ((((b_) \[CenterDot] (c_)) \[CenterDot] ((b_) \[CenterDot] (
                ((b_) \[CenterDot] (a_)) \[CenterDot] (b_)))) \[CenterDot] 
             ((b_) \[CenterDot] (c_)))) -> b \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CenterDot] 
            ((b \[CenterDot] c) \[CenterDot] (((b \[CenterDot] 
                c) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                  a) \[CenterDot] b))) \[CenterDot] (b \[CenterDot] c))) == 
           a \[CenterDot] ((a \[CenterDot] ((b \[CenterDot] 
                ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] (
                b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                 b)))) \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 40} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] 
          ((b \[CenterDot] a) \[CenterDot] b) == a \[CenterDot] 
          ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 48}, 
        "Orientation" -> -1, "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (a_)) \[CenterDot] ((a_) \[CenterDot] (a_)) -> a, "Side" -> 1, 
        "Subpattern" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] 
              (c_)) \[CenterDot] (a_))) -> c, "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 77} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((b \[CenterDot] c) \[CenterDot] (((b \[CenterDot] c) \[CenterDot] 
             (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b))) \[CenterDot] (b \[CenterDot] c))) == 
         a \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
             b)) \[CenterDot] a)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 76}, "Construct" -> 
         {"CriticalPairLemma", 40}, "Position" -> {2, 1}, 
        "Rule" -> (a_) \[CenterDot] (((b_) \[CenterDot] 
             (((b_) \[CenterDot] (a_)) \[CenterDot] (b_))) \[CenterDot] 
            ((b_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] 
              (b_)))) -> b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
             (((b \[CenterDot] c) \[CenterDot] (b \[CenterDot] 
                ((b \[CenterDot] a) \[CenterDot] b))) \[CenterDot] 
              (b \[CenterDot] c))) == a \[CenterDot] 
            ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b)) \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 78} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] 
          ((b \[CenterDot] a) \[CenterDot] b) == a \[CenterDot] 
          ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           a)], "Proof" -> <|"Input" -> {"SubstitutionLemma", 77}, 
        "Construct" -> {"CriticalPairLemma", 7}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] 
            ((((b_) \[CenterDot] (c_)) \[CenterDot] ((b_) \[CenterDot] (
                ((b_) \[CenterDot] (a_)) \[CenterDot] (b_)))) \[CenterDot] 
             ((b_) \[CenterDot] (c_)))) -> b \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[b \[CenterDot] 
            ((b \[CenterDot] a) \[CenterDot] b) == a \[CenterDot] 
            ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b)) \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 79} -> 
     <|"Statement" -> HoldForm[
        (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
          a == a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
             a) \[CenterDot] b))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 70}, "Construct" -> 
         {"SubstitutionLemma", 78}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (((b_) \[CenterDot] 
             (((b_) \[CenterDot] (a_)) \[CenterDot] (b_))) \[CenterDot] 
            (a_)) -> b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
            a == a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
               a) \[CenterDot] b))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 80} -> 
     <|"Statement" -> HoldForm[
        ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           a) \[CenterDot] a == a \[CenterDot] (a \[CenterDot] 
           ((a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b))) \[CenterDot] a))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 51}, "Construct" -> 
         {"SubstitutionLemma", 79}, "Position" -> {2, 2, 1}, 
        "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_))) \[CenterDot] (b_) -> b \[CenterDot] (a \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] a)), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[
          ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
             a) \[CenterDot] a == a \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                  a) \[CenterDot] b))) \[CenterDot] a))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 81} -> 
     <|"Statement" -> HoldForm[
        ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           a) \[CenterDot] a == b \[CenterDot] 
          ((b \[CenterDot] a) \[CenterDot] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 80}, 
        "Construct" -> {"CriticalPairLemma", 19}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
            (((a_) \[CenterDot] ((b_) \[CenterDot] (((b_) \[CenterDot] 
                 (a_)) \[CenterDot] (b_)))) \[CenterDot] (a_))) -> 
          b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
             a) \[CenterDot] a == b \[CenterDot] ((b \[CenterDot] 
              a) \[CenterDot] b)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 82} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] (b \[CenterDot] 
            ((b \[CenterDot] a) \[CenterDot] b))) \[CenterDot] a == 
         b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 81}, 
        "Construct" -> {"SubstitutionLemma", 79}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_))) \[CenterDot] (b_) -> b \[CenterDot] (a \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] a)), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          (a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b))) \[CenterDot] a == b \[CenterDot] 
            ((b \[CenterDot] a) \[CenterDot] b)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 41} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] b == 
         (a \[CenterDot] ((a \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
            a)) \[CenterDot] b], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 25}, "Orientation" -> -1, 
        "Rule" -> ((((a_) \[CenterDot] (a_)) \[CenterDot] (b_)) \[CenterDot] 
            ((a_) \[CenterDot] (a_))) \[CenterDot] (a_) -> a \[CenterDot] a, 
        "Side" -> 1, "Subpattern" -> (((a_) \[CenterDot] (a_)) \[CenterDot] 
           (b_)) \[CenterDot] ((a_) \[CenterDot] (a_)), 
        "MatchingConstruct" -> {"SubstitutionLemma", 82}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((a_) \[CenterDot] ((b_) \[CenterDot] (((b_) \[CenterDot] (
                a_)) \[CenterDot] (b_)))) \[CenterDot] (a_) -> 
          b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 42} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b))], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "Side" -> 1, "Subpattern" -> 
         ((a_) \[CenterDot] (b_)) \[CenterDot] (c_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 41}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] 
              ((b_) \[CenterDot] (b_))) \[CenterDot] (a_))) \[CenterDot] 
           (b_) -> b \[CenterDot] b, "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 43} -> 
     <|"Statement" -> HoldForm[((a \[CenterDot] b) \[CenterDot] 
           a) \[CenterDot] (a \[CenterDot] a) == 
         ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
          ((((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] (((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] a)))) \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] a))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 75}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] 
           (((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
              ((a_) \[CenterDot] (b_)))) \[CenterDot] (a_)) -> 
          a \[CenterDot] b, "Side" -> 1, "Subpattern" -> 
         (a_) \[CenterDot] (b_), "MatchingConstruct" -> {"SubstitutionLemma", 
          48}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (a_)) -> a, "MatchingSide" -> 1, 
        "Position" -> {2, 1, 2, 1}|>|>, {"SubstitutionLemma", 83} -> 
     <|"Statement" -> HoldForm[((a \[CenterDot] b) \[CenterDot] 
           a) \[CenterDot] (a \[CenterDot] a) == 
         ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
          ((((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            a))], "Proof" -> <|"Input" -> {"CriticalPairLemma", 43}, 
        "Construct" -> {"SubstitutionLemma", 48}, "Position" -> {2, 1, 2, 2}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (a_)) -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[((a \[CenterDot] b) \[CenterDot] 
             a) \[CenterDot] (a \[CenterDot] a) == 
           ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            ((((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               b) \[CenterDot] a))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 84} -> 
     <|"Statement" -> HoldForm[((a \[CenterDot] b) \[CenterDot] 
           a) \[CenterDot] (a \[CenterDot] a) == 
         ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 83}, 
        "Construct" -> {"SubstitutionLemma", 48}, "Position" -> {2, 1}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (a_)) -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[((a \[CenterDot] b) \[CenterDot] 
             a) \[CenterDot] (a \[CenterDot] a) == 
           ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 85} -> 
     <|"Statement" -> HoldForm[a == ((a \[CenterDot] b) \[CenterDot] 
           a) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            a))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 84}, 
        "Construct" -> {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (a_)) -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a == ((a \[CenterDot] b) \[CenterDot] 
             a) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              a))], "Source" -> "norm"|>|>, {"CriticalPairLemma", 44} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] a == 
         (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
          (((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
           ((((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
              b) \[CenterDot] a)))], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 85}, "Orientation" -> -1, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_))) -> a, "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] 
          (b_), "MatchingConstruct" -> {"SubstitutionLemma", 48}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (a_)) -> a, "MatchingSide" -> 1, 
        "Position" -> {1, 1}|>|>, {"SubstitutionLemma", 86} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] a == 
         (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
          (((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
           (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 44}, 
        "Construct" -> {"SubstitutionLemma", 48}, "Position" -> {2, 2, 1}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (a_)) -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[(a \[CenterDot] b) \[CenterDot] a == 
           (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
            (((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 87} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] a == 
         (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
          a], "Proof" -> <|"Input" -> {"SubstitutionLemma", 86}, 
        "Construct" -> {"SubstitutionLemma", 85}, "Position" -> {2}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_))) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[(a \[CenterDot] b) \[CenterDot] a == 
           (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
            a], "Source" -> "norm"|>|>, {"CriticalPairLemma", 45} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
           (((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
               a)) \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] a)))) == 
         ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
           (((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              a)))) \[CenterDot] a], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 79}, "Orientation" -> 1, 
        "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_))) \[CenterDot] (b_) -> b \[CenterDot] (a \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] a)), "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 87}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] 
              (b_)) \[CenterDot] (a_))) \[CenterDot] (a_) -> 
          (a \[CenterDot] b) \[CenterDot] a, "MatchingSide" -> 1, 
        "Position" -> {1, 2, 1}|>|>, {"SubstitutionLemma", 88} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
           (((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
               a)) \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] a)))) == 
         ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
           a) \[CenterDot] a], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 45}, "Construct" -> 
         {"SubstitutionLemma", 85}, "Position" -> {1, 2}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_))) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                b) \[CenterDot] a)) \[CenterDot] (((a \[CenterDot] 
                ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
               a) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                 b) \[CenterDot] a)))) == 
           ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
             a) \[CenterDot] a], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 89} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
           (((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
               a)) \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] a)))) == 
         ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 88}, 
        "Construct" -> {"SubstitutionLemma", 87}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_))) \[CenterDot] (a_) -> (a \[CenterDot] b) \[CenterDot] a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
               a)) \[CenterDot] (((a \[CenterDot] ((a \[CenterDot] 
                  b) \[CenterDot] a)) \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)))) == 
           ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] a], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 90} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
           (((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)))) == 
         ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 89}, 
        "Construct" -> {"SubstitutionLemma", 87}, "Position" -> {2, 2, 1}, 
        "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_))) \[CenterDot] (a_) -> (a \[CenterDot] b) \[CenterDot] a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
               a)) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
               a) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                 b) \[CenterDot] a)))) == ((a \[CenterDot] b) \[CenterDot] 
             a) \[CenterDot] a], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 91} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
           a) == ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 90}, 
        "Construct" -> {"SubstitutionLemma", 85}, "Position" -> {2, 2}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_))) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                b) \[CenterDot] a)) \[CenterDot] a) == 
           ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] a], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 92} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] b) \[CenterDot] a) == 
         ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 91}, 
        "Construct" -> {"SubstitutionLemma", 87}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_))) \[CenterDot] (a_) -> (a \[CenterDot] b) \[CenterDot] a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a) == 
           ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] a], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 46} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (
                a \[CenterDot] b))) \[CenterDot] a)) \[CenterDot] a) == 
         ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] a], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 92}, 
        "Orientation" -> -1, "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (a_)) \[CenterDot] (a_) -> a \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] a), "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 75}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CenterDot] (((a_) \[CenterDot] 
             (((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] (
                b_)))) \[CenterDot] (a_)) -> a \[CenterDot] b, 
        "MatchingSide" -> 1, "Position" -> {1, 1}|>|>, 
    {"SubstitutionLemma", 93} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (
                a \[CenterDot] b))) \[CenterDot] a)) \[CenterDot] a) == 
         a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 46}, 
        "Construct" -> {"SubstitutionLemma", 92}, "Position" -> {}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
           (a_) -> a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                  b) \[CenterDot] (a \[CenterDot] b))) \[CenterDot] 
               a)) \[CenterDot] a) == a \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 94} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] b))) \[CenterDot] a) == a \[CenterDot] 
          ((a \[CenterDot] b) \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 93}, 
        "Construct" -> {"SubstitutionLemma", 87}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_))) \[CenterDot] (a_) -> (a \[CenterDot] b) \[CenterDot] a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (
                a \[CenterDot] b))) \[CenterDot] a) == a \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 95} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] b == a \[CenterDot] 
          ((a \[CenterDot] b) \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 94}, 
        "Construct" -> {"SubstitutionLemma", 75}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] 
             (((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] (
                b_)))) \[CenterDot] (a_)) -> a \[CenterDot] b, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[a \[CenterDot] b == 
           a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 96} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          (b \[CenterDot] a)], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 42}, "Construct" -> 
         {"SubstitutionLemma", 95}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
            (b \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"Conclusion", 1} -> <|"Statement" -> HoldForm[a == a], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Construct" -> 
         {"SubstitutionLemma", 96}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[a == a], "Source" -> "cpl"|>|>}|>]
