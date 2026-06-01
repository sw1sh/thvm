ProofObject["EquationalLogic", Inactive[Equal][
  (a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
   (b \[CenterDot] (c \[CenterDot] a)), b], 
 {Inactive[Equal][(((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
    ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] (a_))), c_]}, 
 <|"Variables" -> {a, b, c, x255, x3, x4}, "Constants" -> {}, 
  "Proof" -> {{"Axiom", 1} -> <|"Statement" -> 
       HoldForm[((a \[CenterDot] b) \[CenterDot] c) \[CenterDot] 
          (a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] a)) == c], 
      "Proof" -> <||>|>, {"Hypothesis", 1} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (b \[CenterDot] (c \[CenterDot] a)) == b], "Proof" -> <||>|>, 
    {"CriticalPairLemma", 1} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] c) \[CenterDot] a) == 
         ((((a \[CenterDot] b) \[CenterDot] c) \[CenterDot] x3) \[CenterDot] 
           (a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] a))) \[CenterDot] 
          (((a \[CenterDot] b) \[CenterDot] c) \[CenterDot] 
           (c \[CenterDot] ((a \[CenterDot] b) \[CenterDot] c)))], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] 
          (c_), "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 
         1, "MatchingRule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] 
              (c_)) \[CenterDot] (a_))) -> c, "MatchingSide" -> 1, 
        "Position" -> {2, 2, 1}|>|>, {"CriticalPairLemma", 2} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] a) == 
         ((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
           (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
          a], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 1}, 
        "Orientation" -> -1, "Rule" -> 
         (((((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
             (x3_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] (
                c_)) \[CenterDot] (a_)))) \[CenterDot] 
           ((((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
            ((c_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
              (c_)))) -> a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] a), 
        "Side" -> 1, "Subpattern" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
           (c_)) \[CenterDot] ((c_) \[CenterDot] (((a_) \[CenterDot] 
             (b_)) \[CenterDot] (c_))), "MatchingConstruct" -> {"Axiom", 1}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 3} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] a) == 
         (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             a))) \[CenterDot] a], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 2}, "Orientation" -> -1, 
        "Rule" -> (((((a_) \[CenterDot] (a_)) \[CenterDot] (a_)) \[CenterDot] 
             (b_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] (
                a_)) \[CenterDot] (a_)))) \[CenterDot] (a_) -> 
          a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), "Side" -> 1, 
        "Subpattern" -> (((a_) \[CenterDot] (a_)) \[CenterDot] 
           (a_)) \[CenterDot] (b_), "MatchingConstruct" -> {"Axiom", 1}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "MatchingSide" -> 1, "Position" -> {1, 1}|>|>, 
    {"CriticalPairLemma", 4} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a))], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "Side" -> 1, "Subpattern" -> 
         ((a_) \[CenterDot] (b_)) \[CenterDot] (c_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 3}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) \[CenterDot] 
           (a_) -> a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 5} -> 
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
    {"CriticalPairLemma", 6} -> 
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
        "Position" -> {1, 1}|>|>, {"CriticalPairLemma", 7} -> 
     <|"Statement" -> HoldForm[(c \[CenterDot] b) \[CenterDot] 
          (((c \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
           (c \[CenterDot] b)) == a \[CenterDot] (b \[CenterDot] 
           ((((c \[CenterDot] b) \[CenterDot] (((x3 \[CenterDot] 
                 x4) \[CenterDot] c) \[CenterDot] ((((x3 \[CenterDot] 
                   x4) \[CenterDot] c) \[CenterDot] b) \[CenterDot] 
                ((x3 \[CenterDot] x4) \[CenterDot] c)))) \[CenterDot] 
             ((c \[CenterDot] b) \[CenterDot] (((c \[CenterDot] 
                 b) \[CenterDot] a) \[CenterDot] (c \[CenterDot] 
                b)))) \[CenterDot] ((c \[CenterDot] b) \[CenterDot] 
             (((x3 \[CenterDot] x4) \[CenterDot] c) \[CenterDot] 
              ((((x3 \[CenterDot] x4) \[CenterDot] c) \[CenterDot] 
                b) \[CenterDot] ((x3 \[CenterDot] x4) \[CenterDot] c))))))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 5}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] 
           (((b_) \[CenterDot] (c_)) \[CenterDot] 
            ((((b_) \[CenterDot] (c_)) \[CenterDot] ((b_) \[CenterDot] (
                ((b_) \[CenterDot] (a_)) \[CenterDot] (b_)))) \[CenterDot] 
             ((b_) \[CenterDot] (c_)))) -> b \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] b), "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] (c_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 6}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           ((((c_) \[CenterDot] (x3_)) \[CenterDot] (a_)) \[CenterDot] 
            (((((c_) \[CenterDot] (x3_)) \[CenterDot] (a_)) \[CenterDot] 
              (b_)) \[CenterDot] (((c_) \[CenterDot] (x3_)) \[CenterDot] 
              (a_)))) -> b, "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"CriticalPairLemma", 8} -> 
     <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
          ((c \[CenterDot] a) \[CenterDot] 
           ((((((x3 \[CenterDot] x4) \[CenterDot] c) \[CenterDot] (
                x3 \[CenterDot] ((x3 \[CenterDot] c) \[CenterDot] 
                 x3))) \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
            ((((x3 \[CenterDot] x4) \[CenterDot] c) \[CenterDot] 
              (x3 \[CenterDot] ((x3 \[CenterDot] c) \[CenterDot] 
                x3))) \[CenterDot] a)))], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 6}, "Orientation" -> -1, 
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
    {"SubstitutionLemma", 3} -> 
     <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
          ((c \[CenterDot] a) \[CenterDot] (((c \[CenterDot] a) \[CenterDot] 
             b) \[CenterDot] ((((x3 \[CenterDot] x4) \[CenterDot] 
               c) \[CenterDot] (x3 \[CenterDot] ((x3 \[CenterDot] 
                 c) \[CenterDot] x3))) \[CenterDot] a)))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 8}, 
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
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 4} -> 
     <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
          ((c \[CenterDot] a) \[CenterDot] (((c \[CenterDot] a) \[CenterDot] 
             b) \[CenterDot] (c \[CenterDot] a)))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 3}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {2, 2, 2, 1}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
            ((c \[CenterDot] a) \[CenterDot] (((c \[CenterDot] 
                a) \[CenterDot] b) \[CenterDot] (c \[CenterDot] a)))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 5} -> 
     <|"Statement" -> HoldForm[(c \[CenterDot] b) \[CenterDot] 
          (((c \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
           (c \[CenterDot] b)) == a \[CenterDot] (b \[CenterDot] 
           ((b \[CenterDot] ((c \[CenterDot] b) \[CenterDot] 
              (((c \[CenterDot] b) \[CenterDot] a) \[CenterDot] (
                c \[CenterDot] b)))) \[CenterDot] 
            ((c \[CenterDot] b) \[CenterDot] (((x3 \[CenterDot] 
                x4) \[CenterDot] c) \[CenterDot] ((((x3 \[CenterDot] 
                  x4) \[CenterDot] c) \[CenterDot] b) \[CenterDot] (
                (x3 \[CenterDot] x4) \[CenterDot] c))))))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 7}, 
        "Construct" -> {"SubstitutionLemma", 4}, "Position" -> {2, 2, 1, 1}, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((c_) \[CenterDot] (a_)) \[CenterDot] 
            ((((c_) \[CenterDot] (a_)) \[CenterDot] (b_)) \[CenterDot] 
             ((c_) \[CenterDot] (a_)))) -> b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[(c \[CenterDot] b) \[CenterDot] 
            (((c \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
             (c \[CenterDot] b)) == a \[CenterDot] (b \[CenterDot] 
             ((b \[CenterDot] ((c \[CenterDot] b) \[CenterDot] 
                (((c \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
                 (c \[CenterDot] b)))) \[CenterDot] ((c \[CenterDot] 
                b) \[CenterDot] (((x3 \[CenterDot] x4) \[CenterDot] 
                 c) \[CenterDot] ((((x3 \[CenterDot] x4) \[CenterDot] 
                   c) \[CenterDot] b) \[CenterDot] ((x3 \[CenterDot] 
                   x4) \[CenterDot] c))))))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 6} -> 
     <|"Statement" -> HoldForm[(c \[CenterDot] b) \[CenterDot] 
          (((c \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
           (c \[CenterDot] b)) == a \[CenterDot] (b \[CenterDot] 
           ((b \[CenterDot] ((c \[CenterDot] b) \[CenterDot] 
              (((c \[CenterDot] b) \[CenterDot] a) \[CenterDot] (
                c \[CenterDot] b)))) \[CenterDot] b))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 5}, 
        "Construct" -> {"SubstitutionLemma", 4}, "Position" -> {2, 2, 2}, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((c_) \[CenterDot] (a_)) \[CenterDot] 
            ((((c_) \[CenterDot] (a_)) \[CenterDot] (b_)) \[CenterDot] 
             ((c_) \[CenterDot] (a_)))) -> b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[(c \[CenterDot] b) \[CenterDot] 
            (((c \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
             (c \[CenterDot] b)) == a \[CenterDot] (b \[CenterDot] 
             ((b \[CenterDot] ((c \[CenterDot] b) \[CenterDot] 
                (((c \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
                 (c \[CenterDot] b)))) \[CenterDot] b))], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 9} -> 
     <|"Statement" -> HoldForm[((a \[CenterDot] c) \[CenterDot] 
           b) \[CenterDot] ((((a \[CenterDot] c) \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              a))) \[CenterDot] ((a \[CenterDot] c) \[CenterDot] b)) == 
         (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
          ((((a \[CenterDot] c) \[CenterDot] b) \[CenterDot] x3) \[CenterDot] 
           (((((a \[CenterDot] c) \[CenterDot] b) \[CenterDot] 
              x3) \[CenterDot] (((a \[CenterDot] c) \[CenterDot] 
               b) \[CenterDot] (b \[CenterDot] ((a \[CenterDot] 
                 c) \[CenterDot] b)))) \[CenterDot] 
            (((a \[CenterDot] c) \[CenterDot] b) \[CenterDot] x3)))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 5}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] 
           (((b_) \[CenterDot] (c_)) \[CenterDot] 
            ((((b_) \[CenterDot] (c_)) \[CenterDot] ((b_) \[CenterDot] (
                ((b_) \[CenterDot] (a_)) \[CenterDot] (b_)))) \[CenterDot] 
             ((b_) \[CenterDot] (c_)))) -> b \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] b), "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] (a_), "MatchingConstruct" -> 
         {"Axiom", 1}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "MatchingSide" -> 1, "Position" -> {2, 2, 1, 2, 2, 
         1}|>|>, {"SubstitutionLemma", 7} -> 
     <|"Statement" -> HoldForm[((a \[CenterDot] c) \[CenterDot] 
           b) \[CenterDot] (b \[CenterDot] ((a \[CenterDot] c) \[CenterDot] 
            b)) == (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            a)) \[CenterDot] ((((a \[CenterDot] c) \[CenterDot] 
             b) \[CenterDot] x3) \[CenterDot] 
           (((((a \[CenterDot] c) \[CenterDot] b) \[CenterDot] 
              x3) \[CenterDot] (((a \[CenterDot] c) \[CenterDot] 
               b) \[CenterDot] (b \[CenterDot] ((a \[CenterDot] 
                 c) \[CenterDot] b)))) \[CenterDot] 
            (((a \[CenterDot] c) \[CenterDot] b) \[CenterDot] x3)))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 9}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {2, 1}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[((a \[CenterDot] c) \[CenterDot] b) \[CenterDot] 
            (b \[CenterDot] ((a \[CenterDot] c) \[CenterDot] b)) == 
           (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
            ((((a \[CenterDot] c) \[CenterDot] b) \[CenterDot] 
              x3) \[CenterDot] (((((a \[CenterDot] c) \[CenterDot] 
                 b) \[CenterDot] x3) \[CenterDot] (((a \[CenterDot] 
                  c) \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
                 ((a \[CenterDot] c) \[CenterDot] b)))) \[CenterDot] 
              (((a \[CenterDot] c) \[CenterDot] b) \[CenterDot] x3)))], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 10} -> 
     <|"Statement" -> HoldForm[
        c == ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             a)) \[CenterDot] c) \[CenterDot] (b \[CenterDot] 
           ((b \[CenterDot] c) \[CenterDot] b))], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] 
          (b_), "MatchingConstruct" -> {"CriticalPairLemma", 5}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] 
            ((((b_) \[CenterDot] (c_)) \[CenterDot] ((b_) \[CenterDot] (
                ((b_) \[CenterDot] (a_)) \[CenterDot] (b_)))) \[CenterDot] 
             ((b_) \[CenterDot] (c_)))) -> b \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] b), "MatchingSide" -> 1, 
        "Position" -> {1, 1}|>|>, {"CriticalPairLemma", 11} -> 
     <|"Statement" -> HoldForm[
        c == (((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              a)) \[CenterDot] b) \[CenterDot] c) \[CenterDot] 
          (b \[CenterDot] ((b \[CenterDot] c) \[CenterDot] b))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 10}, 
        "Orientation" -> -1, "Rule" -> 
         (((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
              (a_))) \[CenterDot] (c_)) \[CenterDot] ((b_) \[CenterDot] 
            (((b_) \[CenterDot] (c_)) \[CenterDot] (b_))) -> c, "Side" -> 1, 
        "Subpattern" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] 
              (c_)) \[CenterDot] (a_))) -> c, "MatchingSide" -> 1, 
        "Position" -> {1, 1, 2}|>|>, {"CriticalPairLemma", 12} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a)) \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] a))))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 11}, 
        "Orientation" -> -1, "Rule" -> 
         ((((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] (
                a_))) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((b_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] 
             (b_))) -> c, "Side" -> 1, "Subpattern" -> 
         (((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_))) \[CenterDot] (b_)) \[CenterDot] (c_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 2}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (((((a_) \[CenterDot] (a_)) \[CenterDot] (a_)) \[CenterDot] 
             (b_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] (
                a_)) \[CenterDot] (a_)))) \[CenterDot] (a_) -> 
          a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"SubstitutionLemma", 8} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           a)], "Proof" -> <|"Input" -> {"CriticalPairLemma", 12}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {2, 2}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[a == (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a)) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] a)) \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 13} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
          (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] a))) \[CenterDot] 
           (a \[CenterDot] b)) == (a \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
          (b \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              a)) \[CenterDot] b))], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 6}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            (((b_) \[CenterDot] (((c_) \[CenterDot] (b_)) \[CenterDot] (
                (((c_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
                ((c_) \[CenterDot] (b_))))) \[CenterDot] (b_))) -> 
          (c \[CenterDot] b) \[CenterDot] (((c \[CenterDot] b) \[CenterDot] 
             a) \[CenterDot] (c \[CenterDot] b)), "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] (((c_) \[CenterDot] 
            (b_)) \[CenterDot] ((((c_) \[CenterDot] (b_)) \[CenterDot] 
             (a_)) \[CenterDot] ((c_) \[CenterDot] (b_)))), 
        "MatchingConstruct" -> {"CriticalPairLemma", 5}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] 
            ((((b_) \[CenterDot] (c_)) \[CenterDot] ((b_) \[CenterDot] (
                ((b_) \[CenterDot] (a_)) \[CenterDot] (b_)))) \[CenterDot] 
             ((b_) \[CenterDot] (c_)))) -> b \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] b), "MatchingSide" -> 1, 
        "Position" -> {2, 2, 1}|>|>, {"CriticalPairLemma", 14} -> 
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
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 13}, 
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
    {"SubstitutionLemma", 9} -> 
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
              b))))], "Proof" -> <|"Input" -> {"CriticalPairLemma", 14}, 
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
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 10} -> 
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
              b))))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 9}, 
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
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 11} -> 
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
       <|"Input" -> {"SubstitutionLemma", 10}, "Construct" -> {"Axiom", 1}, 
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
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 12} -> 
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
       <|"Input" -> {"SubstitutionLemma", 11}, "Construct" -> {"Axiom", 1}, 
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
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 13} -> 
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
       <|"Input" -> {"SubstitutionLemma", 12}, "Construct" -> {"Axiom", 1}, 
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
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 15} -> 
     <|"Statement" -> HoldForm[(((a \[CenterDot] a) \[CenterDot] 
            a) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             a))) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
              a) \[CenterDot] a)) \[CenterDot] 
           ((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a))) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] a)))) == a \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] a)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 13}, 
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
        "Position" -> {2, 1, 2}|>|>, {"SubstitutionLemma", 14} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           ((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a))) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] a)))) == a \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 15}, 
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
    {"SubstitutionLemma", 15} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a)))) == a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 14}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {2, 2, 1}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
              (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)))) == 
           a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 16} -> 
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
       <|"Construct" -> {"CriticalPairLemma", 5}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] 
            ((((b_) \[CenterDot] (c_)) \[CenterDot] ((b_) \[CenterDot] (
                ((b_) \[CenterDot] (a_)) \[CenterDot] (b_)))) \[CenterDot] 
             ((b_) \[CenterDot] (c_)))) -> b \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] b), "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] (c_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 5}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CenterDot] (((b_) \[CenterDot] 
             (c_)) \[CenterDot] ((((b_) \[CenterDot] (c_)) \[CenterDot] 
              ((b_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] 
                (b_)))) \[CenterDot] ((b_) \[CenterDot] (c_)))) -> 
          b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), 
        "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"SubstitutionLemma", 16} -> 
     <|"Statement" -> HoldForm[c \[CenterDot] 
          ((c \[CenterDot] a) \[CenterDot] c) == a \[CenterDot] 
          ((b \[CenterDot] ((b \[CenterDot] c) \[CenterDot] b)) \[CenterDot] 
           (((b \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
               b)) \[CenterDot] (c \[CenterDot] ((c \[CenterDot] 
                a) \[CenterDot] c))) \[CenterDot] (c \[CenterDot] 
             ((b \[CenterDot] x3) \[CenterDot] (((b \[CenterDot] 
                 x3) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                   c) \[CenterDot] b))) \[CenterDot] (b \[CenterDot] 
                x3))))))], "Proof" -> <|"Input" -> {"CriticalPairLemma", 16}, 
        "Construct" -> {"CriticalPairLemma", 5}, "Position" -> {2, 2, 1, 1}, 
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
    {"SubstitutionLemma", 17} -> 
     <|"Statement" -> HoldForm[c \[CenterDot] 
          ((c \[CenterDot] a) \[CenterDot] c) == a \[CenterDot] 
          ((b \[CenterDot] ((b \[CenterDot] c) \[CenterDot] b)) \[CenterDot] 
           (((b \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
               b)) \[CenterDot] (c \[CenterDot] ((c \[CenterDot] 
                a) \[CenterDot] c))) \[CenterDot] (b \[CenterDot] 
             ((b \[CenterDot] c) \[CenterDot] b))))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 16}, 
        "Construct" -> {"CriticalPairLemma", 5}, "Position" -> {2, 2, 2}, 
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
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 17} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
          (((b \[CenterDot] a) \[CenterDot] ((a \[CenterDot] 
              (b \[CenterDot] a)) \[CenterDot] a)) \[CenterDot] 
           (b \[CenterDot] a)) == ((a \[CenterDot] (b \[CenterDot] 
             a)) \[CenterDot] a) \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
             a)) \[CenterDot] (((a \[CenterDot] (b \[CenterDot] 
               a)) \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] a))))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 17}, 
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
        "MatchingConstruct" -> {"SubstitutionLemma", 4}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] (b_)) \[CenterDot] (((c_) \[CenterDot] 
             (a_)) \[CenterDot] ((((c_) \[CenterDot] (a_)) \[CenterDot] 
              (b_)) \[CenterDot] ((c_) \[CenterDot] (a_)))) -> b, 
        "MatchingSide" -> 1, "Position" -> {2, 2, 1}|>|>, 
    {"CriticalPairLemma", 18} -> 
     <|"Statement" -> HoldForm[
        ((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
               b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
           a) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
              (((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
                 b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
             a)) \[CenterDot] (((a \[CenterDot] (((((a \[CenterDot] 
                   a) \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
                (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a))) \[CenterDot] a)) \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] (((((a \[CenterDot] 
                    a) \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
                 (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                   a))) \[CenterDot] a)) \[CenterDot] a)))) == 
         (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (((((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
               b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] a))) \[CenterDot] a) \[CenterDot] 
            ((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                  a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
               a)) \[CenterDot] a)) \[CenterDot] 
           (((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
              b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] a))) \[CenterDot] a))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 17}, 
        "Orientation" -> 1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           ((((a_) \[CenterDot] (b_)) \[CenterDot] (((b_) \[CenterDot] (
                (a_) \[CenterDot] (b_))) \[CenterDot] (b_))) \[CenterDot] 
            ((a_) \[CenterDot] (b_))) -> 
          ((b \[CenterDot] (a \[CenterDot] b)) \[CenterDot] b) \[CenterDot] 
           ((b \[CenterDot] ((b \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
              b)) \[CenterDot] (((b \[CenterDot] (a \[CenterDot] 
                b)) \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
              ((b \[CenterDot] (a \[CenterDot] b)) \[CenterDot] b)))), 
        "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] (b_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 2}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (((((a_) \[CenterDot] (a_)) \[CenterDot] (a_)) \[CenterDot] 
             (b_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] (
                a_)) \[CenterDot] (a_)))) \[CenterDot] (a_) -> 
          a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"SubstitutionLemma", 18} -> 
     <|"Statement" -> HoldForm[
        ((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
               b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
           a) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
              (((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
                 b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
             a)) \[CenterDot] (((a \[CenterDot] (((((a \[CenterDot] 
                   a) \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
                (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a))) \[CenterDot] a)) \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] (((((a \[CenterDot] 
                    a) \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
                 (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                   a))) \[CenterDot] a)) \[CenterDot] a)))) == 
         (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            ((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                  a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
               a)) \[CenterDot] a)) \[CenterDot] 
           (((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
              b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] a))) \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 18}, 
        "Construct" -> {"CriticalPairLemma", 2}, "Position" -> {2, 1, 1}, 
        "Rule" -> (((((a_) \[CenterDot] (a_)) \[CenterDot] (a_)) \[CenterDot] 
             (b_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] (
                a_)) \[CenterDot] (a_)))) \[CenterDot] (a_) -> 
          a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          ((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                  a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
               a)) \[CenterDot] a) \[CenterDot] ((a \[CenterDot] 
              ((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                    a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                 a)) \[CenterDot] a)) \[CenterDot] (((a \[CenterDot] 
                (((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
                   b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                     a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
               a) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                 (((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
                    b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                      a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
                a)))) == (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a)) \[CenterDot] (((a \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
                (((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
                   b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                     a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
               a)) \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                 a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] a))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 19} -> 
     <|"Statement" -> HoldForm[
        ((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
               b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
           a) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
              (((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
                 b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
             a)) \[CenterDot] (((a \[CenterDot] (((((a \[CenterDot] 
                   a) \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
                (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a))) \[CenterDot] a)) \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] (((((a \[CenterDot] 
                    a) \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
                 (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                   a))) \[CenterDot] a)) \[CenterDot] a)))) == 
         (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a))) \[CenterDot] a)) \[CenterDot] 
           (((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
              b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] a))) \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 18}, 
        "Construct" -> {"CriticalPairLemma", 2}, "Position" -> {2, 1, 2, 1, 
         2}, "Rule" -> (((((a_) \[CenterDot] (a_)) \[CenterDot] 
              (a_)) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) \[CenterDot] 
           (a_) -> a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          ((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                  a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
               a)) \[CenterDot] a) \[CenterDot] ((a \[CenterDot] 
              ((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                    a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                 a)) \[CenterDot] a)) \[CenterDot] (((a \[CenterDot] 
                (((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
                   b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                     a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
               a) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                 (((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
                    b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                      a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
                a)))) == (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a)) \[CenterDot] (((a \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
                (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a))) \[CenterDot] a)) \[CenterDot] 
             (((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
                b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                  a) \[CenterDot] a))) \[CenterDot] a))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 20} -> 
     <|"Statement" -> HoldForm[
        ((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
               b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
           a) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
              (((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
                 b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
             a)) \[CenterDot] (((a \[CenterDot] (((((a \[CenterDot] 
                   a) \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
                (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a))) \[CenterDot] a)) \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] (((((a \[CenterDot] 
                    a) \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
                 (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                   a))) \[CenterDot] a)) \[CenterDot] a)))) == 
         (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a))) \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
               a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
              ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 19}, 
        "Construct" -> {"CriticalPairLemma", 3}, "Position" -> {2, 1, 2}, 
        "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) \[CenterDot] 
           (a_) -> a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          ((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                  a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
               a)) \[CenterDot] a) \[CenterDot] ((a \[CenterDot] 
              ((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                    a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                 a)) \[CenterDot] a)) \[CenterDot] (((a \[CenterDot] 
                (((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
                   b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                     a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
               a) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                 (((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
                    b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                      a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
                a)))) == (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a)) \[CenterDot] (((a \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] (
                (a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
             (((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
                b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                  a) \[CenterDot] a))) \[CenterDot] a))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 21} -> 
     <|"Statement" -> HoldForm[
        ((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
               b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
           a) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
              (((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
                 b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
             a)) \[CenterDot] (((a \[CenterDot] (((((a \[CenterDot] 
                   a) \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
                (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a))) \[CenterDot] a)) \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] (((((a \[CenterDot] 
                    a) \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
                 (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                   a))) \[CenterDot] a)) \[CenterDot] a)))) == 
         (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
              b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] a))) \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 20}, 
        "Construct" -> {"CriticalPairLemma", 4}, "Position" -> {2, 1}, 
        "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (a_))) \[CenterDot] ((a_) \[CenterDot] 
            (((a_) \[CenterDot] (a_)) \[CenterDot] (a_))) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          ((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                  a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
               a)) \[CenterDot] a) \[CenterDot] ((a \[CenterDot] 
              ((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                    a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                 a)) \[CenterDot] a)) \[CenterDot] (((a \[CenterDot] 
                (((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
                   b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                     a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
               a) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                 (((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
                    b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                      a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
                a)))) == (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a)) \[CenterDot] (a \[CenterDot] 
             (((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
                b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                  a) \[CenterDot] a))) \[CenterDot] a))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 22} -> 
     <|"Statement" -> HoldForm[
        ((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
               b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
           a) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
              (((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
                 b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
             a)) \[CenterDot] (((a \[CenterDot] (((((a \[CenterDot] 
                   a) \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
                (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a))) \[CenterDot] a)) \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] (((((a \[CenterDot] 
                    a) \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
                 (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                   a))) \[CenterDot] a)) \[CenterDot] a)))) == 
         (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 21}, 
        "Construct" -> {"CriticalPairLemma", 2}, "Position" -> {2, 2}, 
        "Rule" -> (((((a_) \[CenterDot] (a_)) \[CenterDot] (a_)) \[CenterDot] 
             (b_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] (
                a_)) \[CenterDot] (a_)))) \[CenterDot] (a_) -> 
          a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          ((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                  a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
               a)) \[CenterDot] a) \[CenterDot] ((a \[CenterDot] 
              ((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                    a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                 a)) \[CenterDot] a)) \[CenterDot] (((a \[CenterDot] 
                (((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
                   b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                     a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
               a) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                 (((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
                    b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                      a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
                a)))) == (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a)) \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
              ((a \[CenterDot] a) \[CenterDot] a)))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 23} -> 
     <|"Statement" -> HoldForm[
        ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a))) \[CenterDot] a) \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] (((((a \[CenterDot] 
                   a) \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
                (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a))) \[CenterDot] a)) \[CenterDot] a)) \[CenterDot] 
           (((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                  a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
               a)) \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                   a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                  ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                a)) \[CenterDot] a)))) == (a \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 22}, 
        "Construct" -> {"CriticalPairLemma", 2}, "Position" -> {1, 1, 2}, 
        "Rule" -> (((((a_) \[CenterDot] (a_)) \[CenterDot] (a_)) \[CenterDot] 
             (b_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] (
                a_)) \[CenterDot] (a_)))) \[CenterDot] (a_) -> 
          a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a))) \[CenterDot] a) \[CenterDot] 
            ((a \[CenterDot] ((a \[CenterDot] (((((a \[CenterDot] 
                     a) \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
                  (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                    a))) \[CenterDot] a)) \[CenterDot] a)) \[CenterDot] 
             (((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                    a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                 a)) \[CenterDot] a) \[CenterDot] (a \[CenterDot] (
                (a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                     a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                    ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                  a)) \[CenterDot] a)))) == (a \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 24} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] (((((a \[CenterDot] 
                   a) \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
                (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a))) \[CenterDot] a)) \[CenterDot] a)) \[CenterDot] 
           (((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                  a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
               a)) \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                   a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                  ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                a)) \[CenterDot] a)))) == (a \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 23}, 
        "Construct" -> {"CriticalPairLemma", 3}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) \[CenterDot] 
           (a_) -> a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            ((a \[CenterDot] ((a \[CenterDot] (((((a \[CenterDot] 
                     a) \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
                  (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                    a))) \[CenterDot] a)) \[CenterDot] a)) \[CenterDot] 
             (((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                    a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                 a)) \[CenterDot] a) \[CenterDot] (a \[CenterDot] (
                (a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                     a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                    ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                  a)) \[CenterDot] a)))) == (a \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 25} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
           (((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                  a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
               a)) \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                   a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                  ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                a)) \[CenterDot] a)))) == (a \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 24}, 
        "Construct" -> {"CriticalPairLemma", 2}, "Position" -> {2, 1, 2, 1, 
         2}, "Rule" -> (((((a_) \[CenterDot] (a_)) \[CenterDot] 
              (a_)) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) \[CenterDot] 
           (a_) -> a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            ((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
               a)) \[CenterDot] (((a \[CenterDot] (((((a \[CenterDot] 
                     a) \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
                  (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                    a))) \[CenterDot] a)) \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] ((a \[CenterDot] (((((a \[CenterDot] 
                      a) \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
                   (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                     a))) \[CenterDot] a)) \[CenterDot] a)))) == 
           (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 26} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a))) \[CenterDot] (((a \[CenterDot] 
              (((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
                 b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
             a) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] (
                ((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
                  b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                    a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
              a)))) == (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
            a)) \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
            ((a \[CenterDot] a) \[CenterDot] a)))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 25}, 
        "Construct" -> {"CriticalPairLemma", 3}, "Position" -> {2, 1, 2}, 
        "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) \[CenterDot] 
           (a_) -> a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a))) \[CenterDot] (((a \[CenterDot] 
                (((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
                   b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                     a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
               a) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                 (((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
                    b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                      a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
                a)))) == (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a)) \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
              ((a \[CenterDot] a) \[CenterDot] a)))], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 19} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] 
          ((b \[CenterDot] a) \[CenterDot] b) == 
         (a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
             b))) \[CenterDot] (((b \[CenterDot] c) \[CenterDot] 
            a) \[CenterDot] (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
             a)))], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 6}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           ((((c_) \[CenterDot] (x3_)) \[CenterDot] (a_)) \[CenterDot] 
            (((((c_) \[CenterDot] (x3_)) \[CenterDot] (a_)) \[CenterDot] 
              (b_)) \[CenterDot] (((c_) \[CenterDot] (x3_)) \[CenterDot] 
              (a_)))) -> b, "Side" -> 1, "Subpattern" -> 
         (((c_) \[CenterDot] (x3_)) \[CenterDot] (a_)) \[CenterDot] (b_), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] 
              (c_)) \[CenterDot] (a_))) -> c, "MatchingSide" -> 1, 
        "Position" -> {2, 2, 1}|>|>, {"SubstitutionLemma", 27} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) == 
         (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 26}, 
        "Construct" -> {"CriticalPairLemma", 19}, "Position" -> {2}, 
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
               a)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 28} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 27}, 
        "Construct" -> {"CriticalPairLemma", 4}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (a_))) \[CenterDot] ((a_) \[CenterDot] 
            (((a_) \[CenterDot] (a_)) \[CenterDot] (a_))) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a == (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a)) \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
              ((a \[CenterDot] a) \[CenterDot] a)))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 29} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] a == a \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 15}, 
        "Construct" -> {"SubstitutionLemma", 28}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (a_))) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[a \[CenterDot] a == 
           a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 30} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           a)], "Proof" -> <|"Input" -> {"SubstitutionLemma", 8}, 
        "Construct" -> {"SubstitutionLemma", 29}, "Position" -> {1}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
            ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a)) \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 31} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 30}, 
        "Construct" -> {"SubstitutionLemma", 29}, "Position" -> {2, 1}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
            ((a \[CenterDot] a) \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 20} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] 
          ((b \[CenterDot] a) \[CenterDot] b) == a \[CenterDot] 
          (a \[CenterDot] ((a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                a) \[CenterDot] b))) \[CenterDot] a))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 11}, 
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
        "Position" -> {1}|>|>, {"CriticalPairLemma", 21} -> 
     <|"Statement" -> HoldForm[
        (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
          (((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
            a) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
             b))) == a \[CenterDot] (a \[CenterDot] 
           ((a \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                b)) \[CenterDot] a)) \[CenterDot] a))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 20}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
            (((a_) \[CenterDot] ((b_) \[CenterDot] (((b_) \[CenterDot] 
                 (a_)) \[CenterDot] (b_)))) \[CenterDot] (a_))) -> 
          b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), "Side" -> 1, 
        "Subpattern" -> ((b_) \[CenterDot] (a_)) \[CenterDot] (b_), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] 
              (c_)) \[CenterDot] (a_))) -> c, "MatchingSide" -> 1, 
        "Position" -> {2, 2, 1, 2, 2}|>|>, {"SubstitutionLemma", 32} -> 
     <|"Statement" -> HoldForm[
        (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
          a == a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
             ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                b)) \[CenterDot] a)) \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 21}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {2}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[(b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
              b)) \[CenterDot] a == a \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] 
                   a) \[CenterDot] b)) \[CenterDot] a)) \[CenterDot] a))], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 22} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          a == a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] a))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 32}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
            (((a_) \[CenterDot] (((b_) \[CenterDot] (((b_) \[CenterDot] 
                  (a_)) \[CenterDot] (b_))) \[CenterDot] (a_))) \[CenterDot] 
             (a_))) -> (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
             b)) \[CenterDot] a, "Side" -> 1, "Subpattern" -> 
         (b_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] (b_)), 
        "MatchingConstruct" -> {"SubstitutionLemma", 29}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)) -> 
          a \[CenterDot] a, "MatchingSide" -> 1, "Position" -> {2, 2, 1, 2, 
         1}|>|>, {"SubstitutionLemma", 33} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          a == a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
             a) \[CenterDot] a))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 22}, "Construct" -> 
         {"SubstitutionLemma", 29}, "Position" -> {2, 2, 1}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            a == a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] a))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 34} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          a == a \[CenterDot] (a \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 33}, 
        "Construct" -> {"SubstitutionLemma", 29}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            a == a \[CenterDot] (a \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 35} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] a == 
         a \[CenterDot] (a \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 34}, 
        "Construct" -> {"SubstitutionLemma", 29}, "Position" -> {1}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(a \[CenterDot] a) \[CenterDot] a == 
           a \[CenterDot] (a \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 36} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 31}, 
        "Construct" -> {"SubstitutionLemma", 35}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (a \[CenterDot] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] a))], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 23} -> 
     <|"Statement" -> HoldForm[((a \[CenterDot] b) \[CenterDot] 
           (a \[CenterDot] b)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))) == 
         (a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
            a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
           (((((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                b)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (
                (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                 b)))) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] (
                a \[CenterDot] b)) \[CenterDot] ((a \[CenterDot] 
                b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
                (a \[CenterDot] b))))) \[CenterDot] 
            (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
               b)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))))))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 7}, 
        "Orientation" -> -1, "Rule" -> 
         ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_))) \[CenterDot] (((((a_) \[CenterDot] (c_)) \[CenterDot] 
              (b_)) \[CenterDot] (x3_)) \[CenterDot] 
            ((((((a_) \[CenterDot] (c_)) \[CenterDot] (b_)) \[CenterDot] (
                x3_)) \[CenterDot] ((((a_) \[CenterDot] (c_)) \[CenterDot] 
                (b_)) \[CenterDot] ((b_) \[CenterDot] (((a_) \[CenterDot] 
                  (c_)) \[CenterDot] (b_))))) \[CenterDot] 
             ((((a_) \[CenterDot] (c_)) \[CenterDot] (b_)) \[CenterDot] 
              (x3_)))) -> ((a \[CenterDot] c) \[CenterDot] b) \[CenterDot] 
           (b \[CenterDot] ((a \[CenterDot] c) \[CenterDot] b)), "Side" -> 1, 
        "Subpattern" -> (((a_) \[CenterDot] (c_)) \[CenterDot] 
           (b_)) \[CenterDot] (x3_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 36}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] ((a_) \[CenterDot] (a_))) -> a, 
        "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"SubstitutionLemma", 37} -> 
     <|"Statement" -> HoldForm[((a \[CenterDot] b) \[CenterDot] 
           (a \[CenterDot] b)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))) == 
         (a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
            a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
           ((((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
               b)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                b)))) \[CenterDot] ((((a \[CenterDot] b) \[CenterDot] (
                a \[CenterDot] b)) \[CenterDot] ((a \[CenterDot] 
                b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
                (a \[CenterDot] b)))) \[CenterDot] (((a \[CenterDot] 
                b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
              ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] 
                 b) \[CenterDot] (a \[CenterDot] b)))))))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 23}, 
        "Construct" -> {"SubstitutionLemma", 35}, "Position" -> {2, 2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (a \[CenterDot] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] b)) \[CenterDot] ((a \[CenterDot] 
              b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] b))) == (a \[CenterDot] 
             ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
              a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             ((((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                 b)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
                ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                  b)))) \[CenterDot] ((((a \[CenterDot] b) \[CenterDot] 
                 (a \[CenterDot] b)) \[CenterDot] ((a \[CenterDot] 
                  b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
                  (a \[CenterDot] b)))) \[CenterDot] (((a \[CenterDot] 
                  b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
                ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] 
                   b) \[CenterDot] (a \[CenterDot] b)))))))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 38} -> 
     <|"Statement" -> HoldForm[((a \[CenterDot] b) \[CenterDot] 
           (a \[CenterDot] b)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))) == 
         (a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
            a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] 
            ((((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                b)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (
                (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                 b)))) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] (
                a \[CenterDot] b)) \[CenterDot] ((a \[CenterDot] 
                b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
                (a \[CenterDot] b)))))))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 37}, "Construct" -> 
         {"SubstitutionLemma", 36}, "Position" -> {2, 2, 1}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            ((a_) \[CenterDot] (a_))) -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] b)) \[CenterDot] ((a \[CenterDot] 
              b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] b))) == (a \[CenterDot] 
             ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
              a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] ((((a \[CenterDot] 
                  b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
                ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] 
                   b) \[CenterDot] (a \[CenterDot] b)))) \[CenterDot] (
                ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                  b)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
                 ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)))))))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 39} -> 
     <|"Statement" -> HoldForm[((a \[CenterDot] b) \[CenterDot] 
           (a \[CenterDot] b)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))) == 
         (a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
            a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                b)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (
                (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)))))))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 38}, 
        "Construct" -> {"SubstitutionLemma", 36}, "Position" -> {2, 2, 2, 1}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            ((a_) \[CenterDot] (a_))) -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] b)) \[CenterDot] ((a \[CenterDot] 
              b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] b))) == (a \[CenterDot] 
             ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
              a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] 
                b) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
                 (a \[CenterDot] b)) \[CenterDot] ((a \[CenterDot] 
                  b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
                  (a \[CenterDot] b)))))))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 40} -> 
     <|"Statement" -> HoldForm[((a \[CenterDot] b) \[CenterDot] 
           (a \[CenterDot] b)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))) == 
         (a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
            a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] b))))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 39}, "Construct" -> 
         {"SubstitutionLemma", 36}, "Position" -> {2, 2, 2, 2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            ((a_) \[CenterDot] (a_))) -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] b)) \[CenterDot] ((a \[CenterDot] 
              b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] b))) == (a \[CenterDot] 
             ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
              a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] 
                b) \[CenterDot] (a \[CenterDot] b))))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 41} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] a == a \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 29}, 
        "Construct" -> {"SubstitutionLemma", 35}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (a \[CenterDot] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CenterDot] a == 
           a \[CenterDot] (a \[CenterDot] (a \[CenterDot] a))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 42} -> 
     <|"Statement" -> HoldForm[((a \[CenterDot] b) \[CenterDot] 
           (a \[CenterDot] b)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))) == 
         (a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
            a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
           (a \[CenterDot] b))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 40}, "Construct" -> 
         {"SubstitutionLemma", 41}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (a_))) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] b)) \[CenterDot] ((a \[CenterDot] 
              b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] b))) == (a \[CenterDot] 
             ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
              a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] b))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 43} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] b == 
         (a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
            a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
           (a \[CenterDot] b))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 42}, "Construct" -> 
         {"SubstitutionLemma", 36}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            ((a_) \[CenterDot] (a_))) -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CenterDot] b == 
           (a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
              a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] b))], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 24} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
          (a \[CenterDot] b) == (a \[CenterDot] b) \[CenterDot] 
          (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] b))) \[CenterDot] a))], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "Side" -> 1, "Subpattern" -> 
         ((a_) \[CenterDot] (b_)) \[CenterDot] (c_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 43}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] 
              ((a_) \[CenterDot] (b_))) \[CenterDot] (a_))) \[CenterDot] 
           (((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
             (b_))) -> a \[CenterDot] b, "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 25} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] b))) \[CenterDot] a) == 
         ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
          ((a \[CenterDot] b) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] b)) \[CenterDot] (a \[CenterDot] b)))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 5}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] 
           (((b_) \[CenterDot] (c_)) \[CenterDot] 
            ((((b_) \[CenterDot] (c_)) \[CenterDot] ((b_) \[CenterDot] (
                ((b_) \[CenterDot] (a_)) \[CenterDot] (b_)))) \[CenterDot] 
             ((b_) \[CenterDot] (c_)))) -> b \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] b), "Side" -> 1, 
        "Subpattern" -> ((b_) \[CenterDot] (c_)) \[CenterDot] 
          ((b_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] (b_))), 
        "MatchingConstruct" -> {"CriticalPairLemma", 24}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
            (((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] (
                (a_) \[CenterDot] (b_)))) \[CenterDot] (a_))) -> 
          (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b), 
        "MatchingSide" -> 1, "Position" -> {2, 2, 1}|>|>, 
    {"SubstitutionLemma", 44} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] b))) \[CenterDot] a) == 
         ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
          ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 25}, 
        "Construct" -> {"SubstitutionLemma", 35}, "Position" -> {2, 2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (a \[CenterDot] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CenterDot] 
            ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                b))) \[CenterDot] a) == ((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] b)) \[CenterDot] ((a \[CenterDot] 
              b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 45} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] b))) \[CenterDot] a) == 
         ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
          ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 44}, 
        "Construct" -> {"SubstitutionLemma", 41}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (a_))) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CenterDot] 
            ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                b))) \[CenterDot] a) == ((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] b)) \[CenterDot] ((a \[CenterDot] 
              b) \[CenterDot] (a \[CenterDot] b))], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 26} -> 
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
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 5}, 
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
         (a_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] 
            ((((b_) \[CenterDot] (c_)) \[CenterDot] ((b_) \[CenterDot] (
                ((b_) \[CenterDot] (a_)) \[CenterDot] (b_)))) \[CenterDot] 
             ((b_) \[CenterDot] (c_)))) -> b \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] b), "MatchingSide" -> 1, 
        "Position" -> {2, 2, 1}|>|>, {"CriticalPairLemma", 27} -> 
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
       <|"Construct" -> {"CriticalPairLemma", 26}, "Orientation" -> 1, 
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
        "MatchingConstruct" -> {"CriticalPairLemma", 4}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (a_))) \[CenterDot] ((a_) \[CenterDot] 
            (((a_) \[CenterDot] (a_)) \[CenterDot] (a_))) -> a, 
        "MatchingSide" -> 1, "Position" -> {2, 1, 2, 2, 1, 2}|>|>, 
    {"SubstitutionLemma", 46} -> 
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
             a)))], "Proof" -> <|"Input" -> {"CriticalPairLemma", 27}, 
        "Construct" -> {"CriticalPairLemma", 4}, "Position" -> {2, 1}, 
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
               a)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 47} -> 
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
             a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 46}, 
        "Construct" -> {"CriticalPairLemma", 4}, "Position" -> {1, 2, 1, 2}, 
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
               a)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 48} -> 
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
             a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 47}, 
        "Construct" -> {"CriticalPairLemma", 4}, "Position" -> {2, 1}, 
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
               a)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 49} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a)) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
              ((a \[CenterDot] a) \[CenterDot] a))))) == 
         (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 48}, 
        "Construct" -> {"CriticalPairLemma", 4}, "Position" -> {2, 2, 1, 2, 
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
               a)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 50} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a)) \[CenterDot] a)) == (a \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 49}, 
        "Construct" -> {"CriticalPairLemma", 4}, "Position" -> {2, 2, 2}, 
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
               a)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 51} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a)) \[CenterDot] a)) == a], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 50}, "Construct" -> 
         {"SubstitutionLemma", 28}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (a_))) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a)) \[CenterDot] a)) == a], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 52} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a)) \[CenterDot] a)) == a], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 51}, "Construct" -> 
         {"SubstitutionLemma", 29}, "Position" -> {1}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a)) \[CenterDot] a)) == a], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 53} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) == a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 52}, 
        "Construct" -> {"SubstitutionLemma", 29}, "Position" -> {2, 2, 1}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) == a], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 54} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] a) == a], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 53}, "Construct" -> 
         {"SubstitutionLemma", 29}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] a) == a], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 55} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] b))) \[CenterDot] a) == a \[CenterDot] b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 45}, 
        "Construct" -> {"SubstitutionLemma", 54}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                b) \[CenterDot] (a \[CenterDot] b))) \[CenterDot] a) == 
           a \[CenterDot] b], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 28} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
          ((c \[CenterDot] a) \[CenterDot] (((c \[CenterDot] a) \[CenterDot] 
             b) \[CenterDot] (c \[CenterDot] a))) == 
         (a \[CenterDot] b) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
            (b \[CenterDot] ((a \[CenterDot] b) \[CenterDot] ((c \[CenterDot] 
                a) \[CenterDot] (((c \[CenterDot] a) \[CenterDot] 
                 b) \[CenterDot] (c \[CenterDot] a)))))) \[CenterDot] 
           (a \[CenterDot] b))], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 55}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] 
             (((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] (
                b_)))) \[CenterDot] (a_)) -> a \[CenterDot] b, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 4}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((c_) \[CenterDot] (a_)) \[CenterDot] 
            ((((c_) \[CenterDot] (a_)) \[CenterDot] (b_)) \[CenterDot] 
             ((c_) \[CenterDot] (a_)))) -> b, "MatchingSide" -> 1, 
        "Position" -> {2, 1, 2, 1}|>|>, {"SubstitutionLemma", 56} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
          ((c \[CenterDot] a) \[CenterDot] (((c \[CenterDot] a) \[CenterDot] 
             b) \[CenterDot] (c \[CenterDot] a))) == 
         (a \[CenterDot] b) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
            (b \[CenterDot] b)) \[CenterDot] (a \[CenterDot] b))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 28}, 
        "Construct" -> {"SubstitutionLemma", 4}, "Position" -> {2, 1, 2, 2}, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((c_) \[CenterDot] (a_)) \[CenterDot] 
            ((((c_) \[CenterDot] (a_)) \[CenterDot] (b_)) \[CenterDot] 
             ((c_) \[CenterDot] (a_)))) -> b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
            ((c \[CenterDot] a) \[CenterDot] (((c \[CenterDot] 
                a) \[CenterDot] b) \[CenterDot] (c \[CenterDot] a))) == 
           (a \[CenterDot] b) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
              (b \[CenterDot] b)) \[CenterDot] (a \[CenterDot] b))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 57} -> 
     <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
          (((a \[CenterDot] b) \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
           (a \[CenterDot] b))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 56}, "Construct" -> 
         {"SubstitutionLemma", 4}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((c_) \[CenterDot] (a_)) \[CenterDot] 
            ((((c_) \[CenterDot] (a_)) \[CenterDot] (b_)) \[CenterDot] 
             ((c_) \[CenterDot] (a_)))) -> b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
            (((a \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
               b)) \[CenterDot] (a \[CenterDot] b))], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 29} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] c) \[CenterDot] a) == 
         (((a \[CenterDot] b) \[CenterDot] c) \[CenterDot] 
           (a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] a))) \[CenterDot] 
          ((c \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] 
               a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                c) \[CenterDot] a)))) \[CenterDot] 
           (((a \[CenterDot] b) \[CenterDot] c) \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] a))))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 57}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           ((((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
              (b_))) \[CenterDot] ((a_) \[CenterDot] (b_))) -> b, 
        "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] (b_), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] 
              (c_)) \[CenterDot] (a_))) -> c, "MatchingSide" -> 1, 
        "Position" -> {2, 1, 1}|>|>, {"SubstitutionLemma", 58} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] c) \[CenterDot] a) == c \[CenterDot] 
          ((c \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] 
               a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                c) \[CenterDot] a)))) \[CenterDot] 
           (((a \[CenterDot] b) \[CenterDot] c) \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] a))))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 29}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {1}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] a) == 
           c \[CenterDot] ((c \[CenterDot] ((a \[CenterDot] 
                ((a \[CenterDot] c) \[CenterDot] a)) \[CenterDot] (
                a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] 
                 a)))) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
               c) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                 c) \[CenterDot] a))))], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 30} -> 
     <|"Statement" -> HoldForm[a == ((a \[CenterDot] b) \[CenterDot] 
           a) \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
            (a \[CenterDot] a)))], "Proof" -> <|"Construct" -> {"Axiom", 1}, 
        "Orientation" -> 1, "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] 
              (c_)) \[CenterDot] (a_))) -> c, "Side" -> 1, 
        "Subpattern" -> ((a_) \[CenterDot] (c_)) \[CenterDot] (a_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 35}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((a_) \[CenterDot] (a_)) \[CenterDot] (a_) -> a \[CenterDot] 
           (a \[CenterDot] a), "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"SubstitutionLemma", 59} -> 
     <|"Statement" -> HoldForm[a == ((a \[CenterDot] b) \[CenterDot] 
           a) \[CenterDot] (a \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 30}, 
        "Construct" -> {"SubstitutionLemma", 41}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (a_))) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a == ((a \[CenterDot] b) \[CenterDot] 
             a) \[CenterDot] (a \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 31} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] 
          ((b \[CenterDot] a) \[CenterDot] b) == a \[CenterDot] 
          ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 59}, 
        "Orientation" -> -1, "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (a_)) \[CenterDot] ((a_) \[CenterDot] (a_)) -> a, "Side" -> 1, 
        "Subpattern" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] 
              (c_)) \[CenterDot] (a_))) -> c, "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 60} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] c) \[CenterDot] a) == c \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] a)) \[CenterDot] 
           (((a \[CenterDot] b) \[CenterDot] c) \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] a))))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 58}, 
        "Construct" -> {"CriticalPairLemma", 31}, "Position" -> {2, 1}, 
        "Rule" -> (a_) \[CenterDot] (((b_) \[CenterDot] 
             (((b_) \[CenterDot] (a_)) \[CenterDot] (b_))) \[CenterDot] 
            ((b_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] 
              (b_)))) -> b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] a) == 
           c \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] 
               a)) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
               c) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                 c) \[CenterDot] a))))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 61} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] c) \[CenterDot] a) == c \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] a)) \[CenterDot] 
           c)], "Proof" -> <|"Input" -> {"SubstitutionLemma", 60}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {2, 2}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] a) == 
           c \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] 
               a)) \[CenterDot] c)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 62} -> 
     <|"Statement" -> HoldForm[
        (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
          a == a \[CenterDot] (a \[CenterDot] ((b \[CenterDot] 
             ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 32}, 
        "Construct" -> {"SubstitutionLemma", 61}, "Position" -> {2, 2, 1}, 
        "Rule" -> (a_) \[CenterDot] (((b_) \[CenterDot] 
             (((b_) \[CenterDot] (a_)) \[CenterDot] (b_))) \[CenterDot] 
            (a_)) -> b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
            a == a \[CenterDot] (a \[CenterDot] ((b \[CenterDot] (
                (b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] a))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 63} -> 
     <|"Statement" -> HoldForm[
        (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
          a == a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
             a) \[CenterDot] b))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 62}, "Construct" -> 
         {"SubstitutionLemma", 61}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (((b_) \[CenterDot] 
             (((b_) \[CenterDot] (a_)) \[CenterDot] (b_))) \[CenterDot] 
            (a_)) -> b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
            a == a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
               a) \[CenterDot] b))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 64} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] 
          ((b \[CenterDot] a) \[CenterDot] b) == a \[CenterDot] 
          (a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
             b)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 61}, 
        "Construct" -> {"SubstitutionLemma", 63}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_))) \[CenterDot] (b_) -> b \[CenterDot] (a \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] a)), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[b \[CenterDot] 
            ((b \[CenterDot] a) \[CenterDot] b) == a \[CenterDot] 
            (a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b)))], "Source" -> "norm"|>|>, {"CriticalPairLemma", 32} -> 
     <|"Statement" -> HoldForm[a == ((a \[CenterDot] b) \[CenterDot] 
           a) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            a))], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 57}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           ((((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
              (b_))) \[CenterDot] ((a_) \[CenterDot] (b_))) -> b, 
        "Side" -> 1, "Subpattern" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
          ((b_) \[CenterDot] (b_)), "MatchingConstruct" -> 
         {"SubstitutionLemma", 59}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (a_)) \[CenterDot] ((a_) \[CenterDot] (a_)) -> a, 
        "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"CriticalPairLemma", 33} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] a == 
         ((((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            a)) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
            a) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             a)))], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 32}, 
        "Orientation" -> -1, "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (a_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] 
              (b_)) \[CenterDot] (a_))) -> a, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 59}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (a_)) \[CenterDot] ((a_) \[CenterDot] (a_)) -> a, 
        "MatchingSide" -> 1, "Position" -> {2, 2, 1}|>|>, 
    {"SubstitutionLemma", 65} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] a == 
         (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
          (((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
           (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 33}, 
        "Construct" -> {"SubstitutionLemma", 59}, "Position" -> {1, 1}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (a_)) -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[(a \[CenterDot] b) \[CenterDot] a == 
           (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
            (((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 66} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] a == 
         (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
          a], "Proof" -> <|"Input" -> {"SubstitutionLemma", 65}, 
        "Construct" -> {"CriticalPairLemma", 32}, "Position" -> {2}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_))) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[(a \[CenterDot] b) \[CenterDot] a == 
           (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
            a], "Source" -> "norm"|>|>, {"CriticalPairLemma", 34} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
           a) == ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
          (((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
           (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 64}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
            ((b_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] 
              (b_)))) -> b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), 
        "Side" -> 1, "Subpattern" -> ((b_) \[CenterDot] (a_)) \[CenterDot] 
          (b_), "MatchingConstruct" -> {"SubstitutionLemma", 66}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_))) \[CenterDot] (a_) -> (a \[CenterDot] b) \[CenterDot] a, 
        "MatchingSide" -> 1, "Position" -> {2, 2, 2}|>|>, 
    {"SubstitutionLemma", 67} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
           a) == ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 34}, 
        "Construct" -> {"CriticalPairLemma", 32}, "Position" -> {2}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_))) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                b) \[CenterDot] a)) \[CenterDot] a) == 
           ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] a], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 68} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] b) \[CenterDot] a) == 
         ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 67}, 
        "Construct" -> {"SubstitutionLemma", 66}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_))) \[CenterDot] (a_) -> (a \[CenterDot] b) \[CenterDot] a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a) == 
           ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] a], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 35} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
          (((b \[CenterDot] a) \[CenterDot] ((c \[CenterDot] b) \[CenterDot] 
             (((c \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
              (c \[CenterDot] b)))) \[CenterDot] (b \[CenterDot] a)) == 
         (a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
          (b \[CenterDot] a)], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 68}, "Orientation" -> -1, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
           (a_) -> a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a), 
        "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] (b_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 4}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] (b_)) \[CenterDot] (((c_) \[CenterDot] 
             (a_)) \[CenterDot] ((((c_) \[CenterDot] (a_)) \[CenterDot] 
              (b_)) \[CenterDot] ((c_) \[CenterDot] (a_)))) -> b, 
        "MatchingSide" -> 1, "Position" -> {1, 1}|>|>, 
    {"SubstitutionLemma", 69} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] (b \[CenterDot] a)) == 
         (a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
          (b \[CenterDot] a)], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 35}, "Construct" -> 
         {"SubstitutionLemma", 4}, "Position" -> {2, 1}, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((c_) \[CenterDot] (a_)) \[CenterDot] 
            ((((c_) \[CenterDot] (a_)) \[CenterDot] (b_)) \[CenterDot] 
             ((c_) \[CenterDot] (a_)))) -> b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] (b \[CenterDot] a)) == 
           (a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
            (b \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 36} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] b))) \[CenterDot] a)) \[CenterDot] 
          (((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
           (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (
                a \[CenterDot] b))) \[CenterDot] a))) == 
         (((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
               b))) \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
            ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                b))) \[CenterDot] a))) \[CenterDot] (a \[CenterDot] b)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 69}, 
        "Orientation" -> -1, "Rule" -> 
         ((a_) \[CenterDot] ((b_) \[CenterDot] (a_))) \[CenterDot] 
           ((b_) \[CenterDot] (a_)) -> (b \[CenterDot] a) \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] a)), "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] (a_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 55}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CenterDot] (((a_) \[CenterDot] 
             (((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] (
                b_)))) \[CenterDot] (a_)) -> a \[CenterDot] b, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 70} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] b))) \[CenterDot] a)) \[CenterDot] 
          (((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
           (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (
                a \[CenterDot] b))) \[CenterDot] a))) == 
         a \[CenterDot] (a \[CenterDot] b)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 36}, 
        "Construct" -> {"CriticalPairLemma", 32}, "Position" -> {1}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_))) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[(a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                 b) \[CenterDot] (a \[CenterDot] b))) \[CenterDot] 
              a)) \[CenterDot] (((a \[CenterDot] ((a \[CenterDot] 
                 b) \[CenterDot] (a \[CenterDot] b))) \[CenterDot] 
              a) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                  b))) \[CenterDot] a))) == a \[CenterDot] 
            (a \[CenterDot] b)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 71} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
          (((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
           (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (
                a \[CenterDot] b))) \[CenterDot] a))) == 
         a \[CenterDot] (a \[CenterDot] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 70}, 
        "Construct" -> {"SubstitutionLemma", 55}, "Position" -> {1}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] 
             (((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] (
                b_)))) \[CenterDot] (a_)) -> a \[CenterDot] b, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (a \[CenterDot] b) \[CenterDot] (((a \[CenterDot] ((a \[CenterDot] 
                 b) \[CenterDot] (a \[CenterDot] b))) \[CenterDot] 
              a) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                  b))) \[CenterDot] a))) == a \[CenterDot] 
            (a \[CenterDot] b)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 72} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] a == 
         a \[CenterDot] (a \[CenterDot] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 71}, 
        "Construct" -> {"CriticalPairLemma", 32}, "Position" -> {2}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_))) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[(a \[CenterDot] b) \[CenterDot] a == a \[CenterDot] 
            (a \[CenterDot] b)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 73} -> 
     <|"Statement" -> HoldForm[(c \[CenterDot] b) \[CenterDot] 
          (((c \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
           (c \[CenterDot] b)) == a \[CenterDot] (b \[CenterDot] 
           (b \[CenterDot] (b \[CenterDot] ((c \[CenterDot] b) \[CenterDot] 
              (((c \[CenterDot] b) \[CenterDot] a) \[CenterDot] (
                c \[CenterDot] b))))))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 6}, "Construct" -> 
         {"SubstitutionLemma", 72}, "Position" -> {2, 2}, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[(c \[CenterDot] b) \[CenterDot] 
            (((c \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
             (c \[CenterDot] b)) == a \[CenterDot] (b \[CenterDot] 
             (b \[CenterDot] (b \[CenterDot] ((c \[CenterDot] b) \[CenterDot] 
                (((c \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
                 (c \[CenterDot] b))))))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 74} -> 
     <|"Statement" -> HoldForm[(c \[CenterDot] b) \[CenterDot] 
          (((c \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
           (c \[CenterDot] b)) == a \[CenterDot] (b \[CenterDot] 
           (b \[CenterDot] (b \[CenterDot] ((c \[CenterDot] b) \[CenterDot] 
              ((c \[CenterDot] b) \[CenterDot] ((c \[CenterDot] 
                 b) \[CenterDot] a))))))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 73}, "Construct" -> 
         {"SubstitutionLemma", 72}, "Position" -> {2, 2, 2, 2, 2}, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[(c \[CenterDot] b) \[CenterDot] 
            (((c \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
             (c \[CenterDot] b)) == a \[CenterDot] (b \[CenterDot] 
             (b \[CenterDot] (b \[CenterDot] ((c \[CenterDot] b) \[CenterDot] 
                ((c \[CenterDot] b) \[CenterDot] ((c \[CenterDot] 
                   b) \[CenterDot] a))))))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 75} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] a == 
         a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            a))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 66}, 
        "Construct" -> {"SubstitutionLemma", 72}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[(a \[CenterDot] b) \[CenterDot] a == 
           a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              a))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 76} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] a == 
         a \[CenterDot] (a \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
             b)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 75}, 
        "Construct" -> {"SubstitutionLemma", 72}, "Position" -> {2, 2}, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[(a \[CenterDot] b) \[CenterDot] a == 
           a \[CenterDot] (a \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
               b)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 77} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] b) == 
         a \[CenterDot] (a \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
             b)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 76}, 
        "Construct" -> {"SubstitutionLemma", 72}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CenterDot] (a \[CenterDot] b) == 
           a \[CenterDot] (a \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
               b)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 78} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
           (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] b)))) == a \[CenterDot] b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 55}, 
        "Construct" -> {"SubstitutionLemma", 72}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
             (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                b)))) == a \[CenterDot] b], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 37} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
           (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] b)))) == a \[CenterDot] (a \[CenterDot] 
           (a \[CenterDot] b))], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 77}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             ((a_) \[CenterDot] (b_)))) -> a \[CenterDot] (a \[CenterDot] b), 
        "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
           (b_)), "MatchingConstruct" -> {"SubstitutionLemma", 78}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] (
                b_))))) -> a \[CenterDot] b, "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"SubstitutionLemma", 79} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] b == a \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] b))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 37}, 
        "Construct" -> {"SubstitutionLemma", 78}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] (
                b_))))) -> a \[CenterDot] b, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CenterDot] b == 
           a \[CenterDot] (a \[CenterDot] (a \[CenterDot] b))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 80} -> 
     <|"Statement" -> HoldForm[(c \[CenterDot] b) \[CenterDot] 
          (((c \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
           (c \[CenterDot] b)) == a \[CenterDot] (b \[CenterDot] 
           ((c \[CenterDot] b) \[CenterDot] ((c \[CenterDot] b) \[CenterDot] 
             ((c \[CenterDot] b) \[CenterDot] a))))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 74}, 
        "Construct" -> {"SubstitutionLemma", 79}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (b_))) -> a \[CenterDot] b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[(c \[CenterDot] b) \[CenterDot] 
            (((c \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
             (c \[CenterDot] b)) == a \[CenterDot] (b \[CenterDot] 
             ((c \[CenterDot] b) \[CenterDot] ((c \[CenterDot] 
                b) \[CenterDot] ((c \[CenterDot] b) \[CenterDot] a))))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 81} -> 
     <|"Statement" -> HoldForm[(c \[CenterDot] b) \[CenterDot] 
          (((c \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
           (c \[CenterDot] b)) == a \[CenterDot] (b \[CenterDot] 
           ((c \[CenterDot] b) \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 80}, 
        "Construct" -> {"SubstitutionLemma", 79}, "Position" -> {2, 2}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (b_))) -> a \[CenterDot] b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[(c \[CenterDot] b) \[CenterDot] 
            (((c \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
             (c \[CenterDot] b)) == a \[CenterDot] (b \[CenterDot] 
             ((c \[CenterDot] b) \[CenterDot] a))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 82} -> 
     <|"Statement" -> HoldForm[(c \[CenterDot] b) \[CenterDot] 
          ((c \[CenterDot] b) \[CenterDot] ((c \[CenterDot] b) \[CenterDot] 
            a)) == a \[CenterDot] (b \[CenterDot] 
           ((c \[CenterDot] b) \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 81}, 
        "Construct" -> {"SubstitutionLemma", 72}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(c \[CenterDot] b) \[CenterDot] 
            ((c \[CenterDot] b) \[CenterDot] ((c \[CenterDot] b) \[CenterDot] 
              a)) == a \[CenterDot] (b \[CenterDot] 
             ((c \[CenterDot] b) \[CenterDot] a))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 83} -> 
     <|"Statement" -> HoldForm[(c \[CenterDot] b) \[CenterDot] a == 
         a \[CenterDot] (b \[CenterDot] ((c \[CenterDot] b) \[CenterDot] 
            a))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 82}, 
        "Construct" -> {"SubstitutionLemma", 79}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (b_))) -> a \[CenterDot] b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(c \[CenterDot] b) \[CenterDot] a == 
           a \[CenterDot] (b \[CenterDot] ((c \[CenterDot] b) \[CenterDot] 
              a))], "Source" -> "norm"|>|>, {"CriticalPairLemma", 38} -> 
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
    {"CriticalPairLemma", 39} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          (((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
            a) \[CenterDot] a)], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 6}, "Orientation" -> -1, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           ((((c_) \[CenterDot] (x3_)) \[CenterDot] (a_)) \[CenterDot] 
            (((((c_) \[CenterDot] (x3_)) \[CenterDot] (a_)) \[CenterDot] 
              (b_)) \[CenterDot] (((c_) \[CenterDot] (x3_)) \[CenterDot] 
              (a_)))) -> b, "Side" -> 1, "Subpattern" -> 
         ((((c_) \[CenterDot] (x3_)) \[CenterDot] (a_)) \[CenterDot] 
           (b_)) \[CenterDot] (((c_) \[CenterDot] (x3_)) \[CenterDot] (a_)), 
        "MatchingConstruct" -> {"CriticalPairLemma", 38}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] (
                a_))) \[CenterDot] (c_)) \[CenterDot] (b_)) \[CenterDot] 
           (((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
              (a_))) \[CenterDot] (b_)) -> b, "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"SubstitutionLemma", 84} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          ((a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
              b))) \[CenterDot] a)], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 39}, "Construct" -> 
         {"SubstitutionLemma", 63}, "Position" -> {2, 1}, 
        "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_))) \[CenterDot] (b_) -> b \[CenterDot] (a \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] a)), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
            ((a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                b))) \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 85} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] 
             ((b \[CenterDot] a) \[CenterDot] b))))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 84}, 
        "Construct" -> {"SubstitutionLemma", 72}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                 a) \[CenterDot] b))))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 86} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] 
              (b \[CenterDot] a)))))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 85}, "Construct" -> 
         {"SubstitutionLemma", 72}, "Position" -> {2, 2, 2, 2}, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] 
                (b \[CenterDot] a)))))], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 40} -> 
     <|"Statement" -> HoldForm[
        ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           (((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b)) \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
             ((b \[CenterDot] a) \[CenterDot] b)))) \[CenterDot] a == 
         a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
             (((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                 b)) \[CenterDot] a) \[CenterDot] a)) \[CenterDot] a))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 32}, 
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
         2}|>|>, {"SubstitutionLemma", 87} -> 
     <|"Statement" -> HoldForm[
        ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           a) \[CenterDot] a == a \[CenterDot] (a \[CenterDot] 
           ((a \[CenterDot] (((b \[CenterDot] ((b \[CenterDot] 
                  a) \[CenterDot] b)) \[CenterDot] a) \[CenterDot] 
              a)) \[CenterDot] a))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 40}, "Construct" -> {"Axiom", 1}, 
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
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 41} -> 
     <|"Statement" -> HoldForm[
        ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           a) \[CenterDot] a == (a \[CenterDot] 
           (((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b)) \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
            (a \[CenterDot] a)))], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 4}, "Orientation" -> -1, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((c_) \[CenterDot] (a_)) \[CenterDot] 
            ((((c_) \[CenterDot] (a_)) \[CenterDot] (b_)) \[CenterDot] 
             ((c_) \[CenterDot] (a_)))) -> b, "Side" -> 1, 
        "Subpattern" -> ((c_) \[CenterDot] (a_)) \[CenterDot] (b_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 39}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] (a_)) \[CenterDot] 
           ((((b_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] (
                b_))) \[CenterDot] (a_)) \[CenterDot] (a_)) -> a, 
        "MatchingSide" -> 1, "Position" -> {2, 2, 1}|>|>, 
    {"SubstitutionLemma", 88} -> 
     <|"Statement" -> HoldForm[
        ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           a) \[CenterDot] a == (a \[CenterDot] 
           (((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b)) \[CenterDot] a) \[CenterDot] a)) \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 41}, 
        "Construct" -> {"SubstitutionLemma", 36}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            ((a_) \[CenterDot] (a_))) -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[
          ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
             a) \[CenterDot] a == (a \[CenterDot] (((b \[CenterDot] 
                ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
               a) \[CenterDot] a)) \[CenterDot] a], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 89} -> 
     <|"Statement" -> HoldForm[
        ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           a) \[CenterDot] a == a \[CenterDot] (a \[CenterDot] 
           (((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b)) \[CenterDot] a) \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 87}, 
        "Construct" -> {"SubstitutionLemma", 88}, "Position" -> {2, 2}, 
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
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 90} -> 
     <|"Statement" -> HoldForm[
        ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           a) \[CenterDot] a == a \[CenterDot] (a \[CenterDot] 
           ((a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b))) \[CenterDot] a))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 89}, "Construct" -> 
         {"SubstitutionLemma", 63}, "Position" -> {2, 2, 1}, 
        "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_))) \[CenterDot] (b_) -> b \[CenterDot] (a \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] a)), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[
          ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
             a) \[CenterDot] a == a \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                  a) \[CenterDot] b))) \[CenterDot] a))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 91} -> 
     <|"Statement" -> HoldForm[
        ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           a) \[CenterDot] a == b \[CenterDot] 
          ((b \[CenterDot] a) \[CenterDot] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 90}, 
        "Construct" -> {"CriticalPairLemma", 20}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
            (((a_) \[CenterDot] ((b_) \[CenterDot] (((b_) \[CenterDot] 
                 (a_)) \[CenterDot] (b_)))) \[CenterDot] (a_))) -> 
          b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
             a) \[CenterDot] a == b \[CenterDot] ((b \[CenterDot] 
              a) \[CenterDot] b)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 92} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] (b \[CenterDot] 
            ((b \[CenterDot] a) \[CenterDot] b))) \[CenterDot] a == 
         b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 91}, 
        "Construct" -> {"SubstitutionLemma", 63}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_))) \[CenterDot] (b_) -> b \[CenterDot] (a \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] a)), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          (a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b))) \[CenterDot] a == b \[CenterDot] 
            ((b \[CenterDot] a) \[CenterDot] b)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 93} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
           (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b))) == 
         b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 92}, 
        "Construct" -> {"SubstitutionLemma", 72}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
             (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b))) == 
           b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 94} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
           (b \[CenterDot] (b \[CenterDot] (b \[CenterDot] a)))) == 
         b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 93}, 
        "Construct" -> {"SubstitutionLemma", 72}, "Position" -> {2, 2, 2}, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
             (b \[CenterDot] (b \[CenterDot] (b \[CenterDot] a)))) == 
           b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 95} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
           (b \[CenterDot] (b \[CenterDot] (b \[CenterDot] a)))) == 
         b \[CenterDot] (b \[CenterDot] (b \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 94}, 
        "Construct" -> {"SubstitutionLemma", 72}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
             (b \[CenterDot] (b \[CenterDot] (b \[CenterDot] a)))) == 
           b \[CenterDot] (b \[CenterDot] (b \[CenterDot] a))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 96} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          (b \[CenterDot] (b \[CenterDot] (b \[CenterDot] a)))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 86}, 
        "Construct" -> {"SubstitutionLemma", 95}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] 
             ((b_) \[CenterDot] ((b_) \[CenterDot] (a_))))) -> 
          b \[CenterDot] (b \[CenterDot] (b \[CenterDot] a)), 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a == (a \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
             (b \[CenterDot] (b \[CenterDot] a)))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 97} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          (b \[CenterDot] a)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 96}, "Construct" -> 
         {"SubstitutionLemma", 79}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (b_))) -> a \[CenterDot] b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
            (b \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 42} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] b))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 97}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
           ((b_) \[CenterDot] (a_)) -> a, "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] (a_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 72}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (a \[CenterDot] b), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 43} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] b)], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 42}, "Orientation" -> -1, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            ((a_) \[CenterDot] (b_))) -> a, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] ((a_) \[CenterDot] (b_)), 
        "MatchingConstruct" -> {"SubstitutionLemma", 79}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] (b_))) -> 
          a \[CenterDot] b, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 44} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] a == a \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] b)], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 43}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (b_)) -> a, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (a_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 97}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
           ((b_) \[CenterDot] (a_)) -> a, "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 45} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] b) \[CenterDot] a == 
         a \[CenterDot] (b \[CenterDot] b)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 83}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            (((c_) \[CenterDot] (b_)) \[CenterDot] (a_))) -> 
          (c \[CenterDot] b) \[CenterDot] a, "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] (((c_) \[CenterDot] 
            (b_)) \[CenterDot] (a_)), "MatchingConstruct" -> 
         {"CriticalPairLemma", 44}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CenterDot] (((a_) \[CenterDot] 
             (a_)) \[CenterDot] (b_)) -> a \[CenterDot] a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 46} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a)) == 
         a \[CenterDot] b], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 45}, "Orientation" -> 1, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (b_) -> 
          b \[CenterDot] (a \[CenterDot] a), "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (a_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 97}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
           ((b_) \[CenterDot] (a_)) -> a, "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 98} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] a == a \[CenterDot] b], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 46}, 
        "Construct" -> {"SubstitutionLemma", 97}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[b \[CenterDot] a == a \[CenterDot] b], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 47} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] 
          ((b \[CenterDot] b) \[CenterDot] b) == 
         (((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
            b) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
             b))) \[CenterDot] (b \[CenterDot] (b \[CenterDot] 
            ((b \[CenterDot] b) \[CenterDot] b)))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 11}, 
        "Orientation" -> -1, "Rule" -> 
         ((((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] (
                a_))) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((b_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] 
             (b_))) -> c, "Side" -> 1, "Subpattern" -> 
         ((b_) \[CenterDot] (c_)) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 3}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) \[CenterDot] 
           (a_) -> a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"SubstitutionLemma", 100} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] 
          ((b \[CenterDot] b) \[CenterDot] b) == b \[CenterDot] 
          (b \[CenterDot] (b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
             b)))], "Proof" -> <|"Input" -> {"CriticalPairLemma", 47}, 
        "Construct" -> {"CriticalPairLemma", 10}, "Position" -> {1}, 
        "Rule" -> (((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
              (a_))) \[CenterDot] (c_)) \[CenterDot] ((b_) \[CenterDot] 
            (((b_) \[CenterDot] (c_)) \[CenterDot] (b_))) -> c, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] b) == 
           b \[CenterDot] (b \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                b) \[CenterDot] b)))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 101} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] a) == a \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 100}, 
        "Construct" -> {"SubstitutionLemma", 29}, "Position" -> {2, 2}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CenterDot] 
            ((a \[CenterDot] a) \[CenterDot] a) == a \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] a))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 201} -> 
     <|"Statement" -> HoldForm[((a \[CenterDot] b) \[CenterDot] 
           c) \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
            (a \[CenterDot] c))) == c], "Proof" -> <|"Input" -> {"Axiom", 1}, 
        "Construct" -> {"SubstitutionLemma", 72}, "Position" -> {2, 2}, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[((a \[CenterDot] b) \[CenterDot] 
             c) \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
              (a \[CenterDot] c))) == c], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 202} -> 
     <|"Statement" -> HoldForm[((a \[CenterDot] b) \[CenterDot] 
           c) \[CenterDot] (a \[CenterDot] c) == c], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 201}, 
        "Construct" -> {"SubstitutionLemma", 79}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (b_))) -> a \[CenterDot] b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[((a \[CenterDot] b) \[CenterDot] 
             c) \[CenterDot] (a \[CenterDot] c) == c], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 203} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] (a \[CenterDot] 
            b)) \[CenterDot] (a \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 59}, 
        "Construct" -> {"SubstitutionLemma", 72}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a == (a \[CenterDot] 
             (a \[CenterDot] b)) \[CenterDot] (a \[CenterDot] a)], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 48} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] b) \[CenterDot] 
          (a \[CenterDot] a)], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 203}, "Orientation" -> -1, 
        "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] (b_))) \[CenterDot] 
           ((a_) \[CenterDot] (a_)) -> a, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] ((a_) \[CenterDot] (b_)), 
        "MatchingConstruct" -> {"SubstitutionLemma", 79}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] (b_))) -> 
          a \[CenterDot] b, "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 49} -> 
     <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
          ((a \[CenterDot] c) \[CenterDot] b)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 202}, 
        "Orientation" -> 1, "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (c_)) \[CenterDot] ((a_) \[CenterDot] (c_)) -> c, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 48}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           ((a_) \[CenterDot] (a_)) -> a, "MatchingSide" -> 1, 
        "Position" -> {1, 1}|>|>, {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[
        (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a))) \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
            a)) \[CenterDot] (b \[CenterDot] (c \[CenterDot] a)) == b], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Construct" -> 
         {"CriticalPairLemma", 4}, "Position" -> {1, 1}, 
        "Rule" -> a_ -> (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
              a) \[CenterDot] a)), "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[(((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] a))) \[CenterDot] ((b \[CenterDot] 
               a) \[CenterDot] a)) \[CenterDot] (b \[CenterDot] 
             (c \[CenterDot] a)) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[
        (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a)) \[CenterDot] (((a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] (
                a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                 a)))) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] a)) \[CenterDot] (((a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a))) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                  a) \[CenterDot] a)))))) \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (b \[CenterDot] (c \[CenterDot] a)) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 1}, 
        "Construct" -> {"CriticalPairLemma", 4}, "Position" -> {1, 1, 2}, 
        "Rule" -> a_ -> (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
              a) \[CenterDot] a)), "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[(((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a)) \[CenterDot] (((a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a)) \[CenterDot] (((a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                  (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                    a))) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                    a) \[CenterDot] a)))) \[CenterDot] ((a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                    a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                     a) \[CenterDot] a))) \[CenterDot] (a \[CenterDot] 
                  ((a \[CenterDot] a) \[CenterDot] a)))))) \[CenterDot] 
             ((b \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (b \[CenterDot] (c \[CenterDot] a)) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 99} -> 
     <|"Statement" -> HoldForm[
        (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a)) \[CenterDot] (((a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] (
                a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                 a)))) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] a)) \[CenterDot] (((a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a))) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                  a) \[CenterDot] a)))))) \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] a))) \[CenterDot] (b \[CenterDot] 
           (c \[CenterDot] a)) == b], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 2}, "Construct" -> 
         {"SubstitutionLemma", 98}, "Position" -> {1, 2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
              (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a)) \[CenterDot] (((a \[CenterDot] ((a \[CenterDot] 
                     a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                 (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                   a)))) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a)) \[CenterDot] (((a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                  (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                    a))) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                    a) \[CenterDot] a)))))) \[CenterDot] (a \[CenterDot] 
              (b \[CenterDot] a))) \[CenterDot] (b \[CenterDot] 
             (c \[CenterDot] a)) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 102} -> 
     <|"Statement" -> HoldForm[
        (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a)) \[CenterDot] (((a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] (
                a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                 a)))) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
                ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] (
                (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a))))))) \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] a))) \[CenterDot] (b \[CenterDot] 
           (c \[CenterDot] a)) == b], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 99}, "Construct" -> 
         {"SubstitutionLemma", 101}, "Position" -> {1, 1, 2, 2}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] (a \[CenterDot] (a \[CenterDot] a)), 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
              (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a)) \[CenterDot] (((a \[CenterDot] ((a \[CenterDot] 
                     a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                 (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                   a)))) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
                  ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                 ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                    a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                     a) \[CenterDot] a))))))) \[CenterDot] (a \[CenterDot] 
              (b \[CenterDot] a))) \[CenterDot] (b \[CenterDot] 
             (c \[CenterDot] a)) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 103} -> 
     <|"Statement" -> HoldForm[
        (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a)) \[CenterDot] (((a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] (
                a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                 a)))) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] (
                (a \[CenterDot] a) \[CenterDot] a))))) \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] a))) \[CenterDot] 
          (b \[CenterDot] (c \[CenterDot] a)) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 102}, 
        "Construct" -> {"SubstitutionLemma", 79}, "Position" -> {1, 1, 2, 2}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (b_))) -> a \[CenterDot] b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
              (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a)) \[CenterDot] (((a \[CenterDot] ((a \[CenterDot] 
                     a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                 (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                   a)))) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))))) \[CenterDot] 
             (a \[CenterDot] (b \[CenterDot] a))) \[CenterDot] 
            (b \[CenterDot] (c \[CenterDot] a)) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 104} -> 
     <|"Statement" -> HoldForm[
        (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a)) \[CenterDot] (((a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] (
                a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                 a)))) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] (
                a \[CenterDot] (a \[CenterDot] a)))))) \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] a))) \[CenterDot] 
          (b \[CenterDot] (c \[CenterDot] a)) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 103}, 
        "Construct" -> {"SubstitutionLemma", 101}, "Position" -> {1, 1, 2, 2, 
         2}, "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] 
             (a_)) \[CenterDot] (a_)) -> a \[CenterDot] (a \[CenterDot] 
            (a \[CenterDot] a)), "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[(((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a)) \[CenterDot] (((a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a)) \[CenterDot] (((a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                  (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                    a))) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                    a) \[CenterDot] a)))) \[CenterDot] ((a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                (a \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
                   a)))))) \[CenterDot] (a \[CenterDot] (b \[CenterDot] 
               a))) \[CenterDot] (b \[CenterDot] (c \[CenterDot] a)) == b], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 105} -> 
     <|"Statement" -> HoldForm[
        (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a)) \[CenterDot] (((a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] (
                a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                 a)))) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
               a)))) \[CenterDot] (a \[CenterDot] (b \[CenterDot] 
             a))) \[CenterDot] (b \[CenterDot] (c \[CenterDot] a)) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 104}, 
        "Construct" -> {"SubstitutionLemma", 79}, "Position" -> {1, 1, 2, 2, 
         2}, "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
            ((a_) \[CenterDot] (b_))) -> a \[CenterDot] b, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
              (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a)) \[CenterDot] (((a \[CenterDot] ((a \[CenterDot] 
                     a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                 (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                   a)))) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                 a)))) \[CenterDot] (a \[CenterDot] (b \[CenterDot] 
               a))) \[CenterDot] (b \[CenterDot] (c \[CenterDot] a)) == b], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 106} -> 
     <|"Statement" -> HoldForm[
        (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a)) \[CenterDot] (((a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] (
                a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                 a)))) \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
                (a \[CenterDot] a))) \[CenterDot] (a \[CenterDot] 
               a)))) \[CenterDot] (a \[CenterDot] (b \[CenterDot] 
             a))) \[CenterDot] (b \[CenterDot] (c \[CenterDot] a)) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 105}, 
        "Construct" -> {"SubstitutionLemma", 101}, "Position" -> {1, 1, 2, 2, 
         1}, "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] 
             (a_)) \[CenterDot] (a_)) -> a \[CenterDot] (a \[CenterDot] 
            (a \[CenterDot] a)), "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[(((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a)) \[CenterDot] (((a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a)) \[CenterDot] (((a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                  (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                    a))) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                    a) \[CenterDot] a)))) \[CenterDot] ((a \[CenterDot] 
                 (a \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
                (a \[CenterDot] a)))) \[CenterDot] (a \[CenterDot] 
              (b \[CenterDot] a))) \[CenterDot] (b \[CenterDot] 
             (c \[CenterDot] a)) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 107} -> 
     <|"Statement" -> HoldForm[
        (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a)) \[CenterDot] (((a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] (
                a \[CenterDot] a))) \[CenterDot] ((a \[CenterDot] (
                a \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
              (a \[CenterDot] a)))) \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] a))) \[CenterDot] (b \[CenterDot] 
           (c \[CenterDot] a)) == b], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 106}, "Construct" -> 
         {"CriticalPairLemma", 44}, "Position" -> {1, 1, 2, 1, 2, 2}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (b_)) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
              (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a)) \[CenterDot] (((a \[CenterDot] ((a \[CenterDot] 
                     a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                 (a \[CenterDot] a))) \[CenterDot] ((a \[CenterDot] 
                 (a \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
                (a \[CenterDot] a)))) \[CenterDot] (a \[CenterDot] 
              (b \[CenterDot] a))) \[CenterDot] (b \[CenterDot] 
             (c \[CenterDot] a)) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 108} -> 
     <|"Statement" -> HoldForm[
        (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a)) \[CenterDot] (((a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
             ((a \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
                 a))) \[CenterDot] (a \[CenterDot] a)))) \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] a))) \[CenterDot] 
          (b \[CenterDot] (c \[CenterDot] a)) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 107}, 
        "Construct" -> {"CriticalPairLemma", 44}, "Position" -> {1, 1, 2, 1, 
         2, 1, 2}, "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] 
             (a_)) \[CenterDot] (b_)) -> a \[CenterDot] a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
              (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a)) \[CenterDot] (((a \[CenterDot] ((a \[CenterDot] 
                     a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                   a)) \[CenterDot] (a \[CenterDot] a))) \[CenterDot] (
                (a \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
                   a))) \[CenterDot] (a \[CenterDot] a)))) \[CenterDot] 
             (a \[CenterDot] (b \[CenterDot] a))) \[CenterDot] 
            (b \[CenterDot] (c \[CenterDot] a)) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 109} -> 
     <|"Statement" -> HoldForm[
        (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a)) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                a))) \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
                (a \[CenterDot] a))) \[CenterDot] (a \[CenterDot] 
               a)))) \[CenterDot] (a \[CenterDot] (b \[CenterDot] 
             a))) \[CenterDot] (b \[CenterDot] (c \[CenterDot] a)) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 108}, 
        "Construct" -> {"CriticalPairLemma", 44}, "Position" -> {1, 1, 2, 1, 
         2, 1, 1}, "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] 
             (a_)) \[CenterDot] (b_)) -> a \[CenterDot] a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
              (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a)) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
                  (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                  a))) \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
                  (a \[CenterDot] a))) \[CenterDot] (a \[CenterDot] 
                 a)))) \[CenterDot] (a \[CenterDot] (b \[CenterDot] 
               a))) \[CenterDot] (b \[CenterDot] (c \[CenterDot] a)) == b], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 110} -> 
     <|"Statement" -> HoldForm[
        (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (((a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] (
                a \[CenterDot] a))) \[CenterDot] ((a \[CenterDot] (
                a \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
              (a \[CenterDot] a)))) \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] a))) \[CenterDot] (b \[CenterDot] 
           (c \[CenterDot] a)) == b], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 109}, "Construct" -> 
         {"CriticalPairLemma", 44}, "Position" -> {1, 1, 2, 1, 1}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (b_)) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
              (((a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] 
                   a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
                 (a \[CenterDot] a))) \[CenterDot] ((a \[CenterDot] 
                 (a \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
                (a \[CenterDot] a)))) \[CenterDot] (a \[CenterDot] 
              (b \[CenterDot] a))) \[CenterDot] (b \[CenterDot] 
             (c \[CenterDot] a)) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 111} -> 
     <|"Statement" -> HoldForm[
        (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] (
                a \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
              (a \[CenterDot] a)))) \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] a))) \[CenterDot] (b \[CenterDot] 
           (c \[CenterDot] a)) == b], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 110}, "Construct" -> 
         {"SubstitutionLemma", 98}, "Position" -> {1, 1, 2, 1}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
              (((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                   a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
                (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
                 (a \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
                (a \[CenterDot] a)))) \[CenterDot] (a \[CenterDot] 
              (b \[CenterDot] a))) \[CenterDot] (b \[CenterDot] 
             (c \[CenterDot] a)) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 112} -> 
     <|"Statement" -> HoldForm[
        (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] (a \[CenterDot] a)))) \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] a))) \[CenterDot] 
          (b \[CenterDot] (c \[CenterDot] a)) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 111}, 
        "Construct" -> {"SubstitutionLemma", 79}, "Position" -> {1, 1, 2, 2, 
         1}, "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
            ((a_) \[CenterDot] (b_))) -> a \[CenterDot] b, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
              (((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                   a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
                (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] a)))) \[CenterDot] 
             (a \[CenterDot] (b \[CenterDot] a))) \[CenterDot] 
            (b \[CenterDot] (c \[CenterDot] a)) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 113} -> 
     <|"Statement" -> HoldForm[
        (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            ((((a \[CenterDot] a) \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] (a \[CenterDot] a)))) \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] a))) \[CenterDot] 
          (b \[CenterDot] (c \[CenterDot] a)) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 112}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {1, 1, 2, 1, 
         1}, "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
              ((((a \[CenterDot] a) \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
                (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] a)))) \[CenterDot] 
             (a \[CenterDot] (b \[CenterDot] a))) \[CenterDot] 
            (b \[CenterDot] (c \[CenterDot] a)) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 114} -> 
     <|"Statement" -> HoldForm[(((a \[CenterDot] a) \[CenterDot] 
            ((((a \[CenterDot] a) \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] (a \[CenterDot] a)))) \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] a))) \[CenterDot] 
          (b \[CenterDot] (c \[CenterDot] a)) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 113}, 
        "Construct" -> {"CriticalPairLemma", 44}, "Position" -> {1, 1, 1}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (b_)) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(((a \[CenterDot] a) \[CenterDot] 
              ((((a \[CenterDot] a) \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
                (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] a)))) \[CenterDot] 
             (a \[CenterDot] (b \[CenterDot] a))) \[CenterDot] 
            (b \[CenterDot] (c \[CenterDot] a)) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 115} -> 
     <|"Statement" -> HoldForm[
        ((((((a \[CenterDot] a) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a))) \[CenterDot] (a \[CenterDot] 
               a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] a))) \[CenterDot] (a \[CenterDot] 
             a)) \[CenterDot] (a \[CenterDot] (b \[CenterDot] 
             a))) \[CenterDot] (b \[CenterDot] (c \[CenterDot] a)) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 114}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {1, 1}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          ((((((a \[CenterDot] a) \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
                (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
              (b \[CenterDot] a))) \[CenterDot] (b \[CenterDot] 
             (c \[CenterDot] a)) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 116} -> 
     <|"Statement" -> HoldForm[
        (((((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
            (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] a))) \[CenterDot] (b \[CenterDot] 
           (c \[CenterDot] a)) == b], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 115}, "Construct" -> 
         {"SubstitutionLemma", 98}, "Position" -> {1, 1, 1, 1, 1}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (((((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                   a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
                (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
              (b \[CenterDot] a))) \[CenterDot] (b \[CenterDot] 
             (c \[CenterDot] a)) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 117} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] (c \[CenterDot] 
            a)) \[CenterDot] (((((((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
               a))) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] a))) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 116}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (b \[CenterDot] (c \[CenterDot] a)) \[CenterDot] 
            (((((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                   a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
                (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
              (b \[CenterDot] a))) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 118} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] (c \[CenterDot] 
            a)) \[CenterDot] ((a \[CenterDot] (b \[CenterDot] 
             a)) \[CenterDot] ((((((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
               a))) \[CenterDot] (a \[CenterDot] a))) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 117}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (b \[CenterDot] (c \[CenterDot] a)) \[CenterDot] 
            ((a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
             ((((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                   a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
                (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
              (a \[CenterDot] a))) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 119} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] (c \[CenterDot] 
            a)) \[CenterDot] ((a \[CenterDot] (b \[CenterDot] 
             a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
            (((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] (a \[CenterDot] a))))) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 118}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {2, 2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (b \[CenterDot] (c \[CenterDot] a)) \[CenterDot] 
            ((a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] (((((a \[CenterDot] 
                   a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
                 (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a))))) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 120} -> 
     <|"Statement" -> HoldForm[
        ((a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] 
            (((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] (a \[CenterDot] a))))) \[CenterDot] 
          (b \[CenterDot] (c \[CenterDot] a)) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 119}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          ((a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] (((((a \[CenterDot] 
                   a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
                 (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a))))) \[CenterDot] (b \[CenterDot] 
             (c \[CenterDot] a)) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 121} -> 
     <|"Statement" -> HoldForm[
        ((a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] 
            (((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] (a \[CenterDot] a))))) \[CenterDot] 
          ((c \[CenterDot] a) \[CenterDot] b) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 120}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          ((a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] (((((a \[CenterDot] 
                   a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
                 (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a))))) \[CenterDot] 
            ((c \[CenterDot] a) \[CenterDot] b) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 122} -> 
     <|"Statement" -> HoldForm[
        ((a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] 
            (((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] (a \[CenterDot] a))))) \[CenterDot] 
          ((a \[CenterDot] c) \[CenterDot] b) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 121}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {2, 1}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          ((a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] (((((a \[CenterDot] 
                   a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
                 (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a))))) \[CenterDot] 
            ((a \[CenterDot] c) \[CenterDot] b) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 123} -> 
     <|"Statement" -> HoldForm[((a \[CenterDot] c) \[CenterDot] 
           b) \[CenterDot] ((a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] 
            (((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] (a \[CenterDot] a))))) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 122}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          ((a \[CenterDot] c) \[CenterDot] b) \[CenterDot] 
            ((a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] (((((a \[CenterDot] 
                   a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
                 (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a))))) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 124} -> 
     <|"Statement" -> HoldForm[((a \[CenterDot] c) \[CenterDot] 
           b) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
            (((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] (a \[CenterDot] a)))) \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] a))) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 123}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          ((a \[CenterDot] c) \[CenterDot] b) \[CenterDot] 
            (((a \[CenterDot] a) \[CenterDot] (((((a \[CenterDot] 
                   a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
                 (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a)))) \[CenterDot] (a \[CenterDot] 
              (b \[CenterDot] a))) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 125} -> 
     <|"Statement" -> HoldForm[((a \[CenterDot] c) \[CenterDot] 
           b) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
            (((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] (a \[CenterDot] a)))) \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] a)) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 124}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {2, 2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          ((a \[CenterDot] c) \[CenterDot] b) \[CenterDot] 
            (((a \[CenterDot] a) \[CenterDot] (((((a \[CenterDot] 
                   a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
                 (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a)))) \[CenterDot] ((b \[CenterDot] 
               a) \[CenterDot] a)) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 126} -> 
     <|"Statement" -> HoldForm[(((a \[CenterDot] a) \[CenterDot] 
            (((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] (a \[CenterDot] a)))) \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          ((a \[CenterDot] c) \[CenterDot] b) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 125}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (((a \[CenterDot] a) \[CenterDot] (((((a \[CenterDot] 
                   a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
                 (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a)))) \[CenterDot] ((b \[CenterDot] 
               a) \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
              c) \[CenterDot] b) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 127} -> 
     <|"Statement" -> HoldForm[(((a \[CenterDot] a) \[CenterDot] 
            (((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] (a \[CenterDot] a)))) \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (b \[CenterDot] (a \[CenterDot] c)) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 126}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (((a \[CenterDot] a) \[CenterDot] (((((a \[CenterDot] 
                   a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
                 (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a)))) \[CenterDot] ((b \[CenterDot] 
               a) \[CenterDot] a)) \[CenterDot] (b \[CenterDot] 
             (a \[CenterDot] c)) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 128} -> 
     <|"Statement" -> HoldForm[(((a \[CenterDot] a) \[CenterDot] 
            (((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] (a \[CenterDot] a)))) \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (b \[CenterDot] (c \[CenterDot] a)) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 127}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {2, 2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (((a \[CenterDot] a) \[CenterDot] (((((a \[CenterDot] 
                   a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
                 (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a)))) \[CenterDot] ((b \[CenterDot] 
               a) \[CenterDot] a)) \[CenterDot] (b \[CenterDot] 
             (c \[CenterDot] a)) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 129} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] (c \[CenterDot] 
            a)) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
            (((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] (a \[CenterDot] a)))) \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] a)) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 128}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (b \[CenterDot] (c \[CenterDot] a)) \[CenterDot] 
            (((a \[CenterDot] a) \[CenterDot] (((((a \[CenterDot] 
                   a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
                 (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a)))) \[CenterDot] ((b \[CenterDot] 
               a) \[CenterDot] a)) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 130} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] (c \[CenterDot] 
            a)) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
            (((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] (a \[CenterDot] a)))) \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] a)) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 129}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {2, 2, 1}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (b \[CenterDot] (c \[CenterDot] a)) \[CenterDot] 
            (((a \[CenterDot] a) \[CenterDot] (((((a \[CenterDot] 
                   a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
                 (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a)))) \[CenterDot] ((a \[CenterDot] 
               b) \[CenterDot] a)) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 131} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] (c \[CenterDot] 
            a)) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
            a) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
            (((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] (a \[CenterDot] a))))) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 130}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (b \[CenterDot] (c \[CenterDot] a)) \[CenterDot] 
            (((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] (((((a \[CenterDot] 
                   a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
                 (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a))))) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 132} -> 
     <|"Statement" -> HoldForm[(((a \[CenterDot] b) \[CenterDot] 
            a) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
            (((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] (a \[CenterDot] a))))) \[CenterDot] 
          (b \[CenterDot] (c \[CenterDot] a)) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 131}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] (((((a \[CenterDot] 
                   a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
                 (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a))))) \[CenterDot] (b \[CenterDot] 
             (c \[CenterDot] a)) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 133} -> 
     <|"Statement" -> HoldForm[(((a \[CenterDot] b) \[CenterDot] 
            a) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
            (((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] (a \[CenterDot] a))))) \[CenterDot] 
          ((c \[CenterDot] a) \[CenterDot] b) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 132}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] (((((a \[CenterDot] 
                   a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
                 (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a))))) \[CenterDot] 
            ((c \[CenterDot] a) \[CenterDot] b) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 134} -> 
     <|"Statement" -> HoldForm[(((a \[CenterDot] b) \[CenterDot] 
            a) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
            (((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] (a \[CenterDot] a))))) \[CenterDot] 
          ((a \[CenterDot] c) \[CenterDot] b) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 133}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {2, 1}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] (((((a \[CenterDot] 
                   a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
                 (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a))))) \[CenterDot] 
            ((a \[CenterDot] c) \[CenterDot] b) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 135} -> 
     <|"Statement" -> HoldForm[((a \[CenterDot] c) \[CenterDot] 
           b) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] 
            (((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] (a \[CenterDot] a))))) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 134}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          ((a \[CenterDot] c) \[CenterDot] b) \[CenterDot] 
            (((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] (((((a \[CenterDot] 
                   a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
                 (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a))))) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 136} -> 
     <|"Statement" -> HoldForm[((a \[CenterDot] c) \[CenterDot] 
           b) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
           ((((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
            (a \[CenterDot] a))) == b], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 135}, "Construct" -> 
         {"SubstitutionLemma", 98}, "Position" -> {2, 2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          ((a \[CenterDot] c) \[CenterDot] b) \[CenterDot] 
            (((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
             ((((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                   a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
                (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
              (a \[CenterDot] a))) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 137} -> 
     <|"Statement" -> HoldForm[((a \[CenterDot] c) \[CenterDot] 
           b) \[CenterDot] (((((((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
               a))) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] a)) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 136}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          ((a \[CenterDot] c) \[CenterDot] b) \[CenterDot] 
            (((((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                   a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
                (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               b) \[CenterDot] a)) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 138} -> 
     <|"Statement" -> HoldForm[
        (((((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
            (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            a)) \[CenterDot] ((a \[CenterDot] c) \[CenterDot] b) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 137}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (((((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                   a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
                (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               b) \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
              c) \[CenterDot] b) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 139} -> 
     <|"Statement" -> HoldForm[
        (((((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
            (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            a)) \[CenterDot] (b \[CenterDot] (a \[CenterDot] c)) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 138}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (((((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                   a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
                (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               b) \[CenterDot] a)) \[CenterDot] (b \[CenterDot] 
             (a \[CenterDot] c)) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 140} -> 
     <|"Statement" -> HoldForm[
        (((((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
            (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            a)) \[CenterDot] (b \[CenterDot] (c \[CenterDot] a)) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 139}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {2, 2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (((((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                   a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
                (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               b) \[CenterDot] a)) \[CenterDot] (b \[CenterDot] 
             (c \[CenterDot] a)) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 141} -> 
     <|"Statement" -> HoldForm[
        (((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
             ((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
              (a \[CenterDot] a))) \[CenterDot] (a \[CenterDot] 
             a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            a)) \[CenterDot] (b \[CenterDot] (c \[CenterDot] a)) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 140}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {1, 1, 1}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] ((((a \[CenterDot] a) \[CenterDot] 
                  (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                  a)) \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               b) \[CenterDot] a)) \[CenterDot] (b \[CenterDot] 
             (c \[CenterDot] a)) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 142} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] (c \[CenterDot] 
            a)) \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((((a \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] (
                a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
               a))) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] a)) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 141}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (b \[CenterDot] (c \[CenterDot] a)) \[CenterDot] 
            (((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] ((((a \[CenterDot] a) \[CenterDot] 
                  (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                  a)) \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               b) \[CenterDot] a)) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 143} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] (c \[CenterDot] 
            a)) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
            a) \[CenterDot] ((((a \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((((a \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] (
                a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
               a))) \[CenterDot] (a \[CenterDot] a))) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 142}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (b \[CenterDot] (c \[CenterDot] a)) \[CenterDot] 
            (((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
             ((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] ((((a \[CenterDot] a) \[CenterDot] 
                  (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                  a)) \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
              (a \[CenterDot] a))) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 144} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] (c \[CenterDot] 
            a)) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
            a) \[CenterDot] ((((a \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                a)))) \[CenterDot] (a \[CenterDot] a))) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 143}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {2, 2, 1, 2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (b \[CenterDot] (c \[CenterDot] a)) \[CenterDot] 
            (((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
             ((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                   a)) \[CenterDot] (a \[CenterDot] a)))) \[CenterDot] 
              (a \[CenterDot] a))) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 145} -> 
     <|"Statement" -> HoldForm[(((a \[CenterDot] b) \[CenterDot] 
            a) \[CenterDot] ((((a \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                a)))) \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
          (b \[CenterDot] (c \[CenterDot] a)) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 144}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
             ((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                   a)) \[CenterDot] (a \[CenterDot] a)))) \[CenterDot] 
              (a \[CenterDot] a))) \[CenterDot] (b \[CenterDot] 
             (c \[CenterDot] a)) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 146} -> 
     <|"Statement" -> HoldForm[(((a \[CenterDot] b) \[CenterDot] 
            a) \[CenterDot] ((((a \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                a)))) \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
          ((c \[CenterDot] a) \[CenterDot] b) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 145}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
             ((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                   a)) \[CenterDot] (a \[CenterDot] a)))) \[CenterDot] 
              (a \[CenterDot] a))) \[CenterDot] ((c \[CenterDot] 
              a) \[CenterDot] b) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 147} -> 
     <|"Statement" -> HoldForm[(((a \[CenterDot] b) \[CenterDot] 
            a) \[CenterDot] ((((a \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                a)))) \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
          ((a \[CenterDot] c) \[CenterDot] b) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 146}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {2, 1}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
             ((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                   a)) \[CenterDot] (a \[CenterDot] a)))) \[CenterDot] 
              (a \[CenterDot] a))) \[CenterDot] ((a \[CenterDot] 
              c) \[CenterDot] b) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 148} -> 
     <|"Statement" -> HoldForm[((a \[CenterDot] c) \[CenterDot] 
           b) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
           ((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
               a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] (a \[CenterDot] a)))) \[CenterDot] 
            (a \[CenterDot] a))) == b], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 147}, "Construct" -> 
         {"SubstitutionLemma", 98}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          ((a \[CenterDot] c) \[CenterDot] b) \[CenterDot] 
            (((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
             ((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                   a)) \[CenterDot] (a \[CenterDot] a)))) \[CenterDot] 
              (a \[CenterDot] a))) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 149} -> 
     <|"Statement" -> HoldForm[((a \[CenterDot] c) \[CenterDot] 
           b) \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                a)))) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] a)) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 148}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          ((a \[CenterDot] c) \[CenterDot] b) \[CenterDot] 
            (((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                   a)) \[CenterDot] (a \[CenterDot] a)))) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               b) \[CenterDot] a)) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 150} -> 
     <|"Statement" -> HoldForm[((a \[CenterDot] c) \[CenterDot] 
           b) \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                a)))) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] b))) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 149}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {2, 2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          ((a \[CenterDot] c) \[CenterDot] b) \[CenterDot] 
            (((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                   a)) \[CenterDot] (a \[CenterDot] a)))) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
              (a \[CenterDot] b))) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 151} -> 
     <|"Statement" -> HoldForm[
        (((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] (
                a \[CenterDot] a)))) \[CenterDot] (a \[CenterDot] 
             a)) \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
             b))) \[CenterDot] ((a \[CenterDot] c) \[CenterDot] b) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 150}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                   a)) \[CenterDot] (a \[CenterDot] a)))) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
              (a \[CenterDot] b))) \[CenterDot] ((a \[CenterDot] 
              c) \[CenterDot] b) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 152} -> 
     <|"Statement" -> HoldForm[
        (((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] (
                a \[CenterDot] a)))) \[CenterDot] (a \[CenterDot] 
             a)) \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
             b))) \[CenterDot] (b \[CenterDot] (a \[CenterDot] c)) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 151}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                   a)) \[CenterDot] (a \[CenterDot] a)))) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
              (a \[CenterDot] b))) \[CenterDot] (b \[CenterDot] 
             (a \[CenterDot] c)) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 153} -> 
     <|"Statement" -> HoldForm[
        (((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] (
                a \[CenterDot] a)))) \[CenterDot] (a \[CenterDot] 
             a)) \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
             b))) \[CenterDot] (b \[CenterDot] (c \[CenterDot] a)) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 152}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {2, 2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                   a)) \[CenterDot] (a \[CenterDot] a)))) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
              (a \[CenterDot] b))) \[CenterDot] (b \[CenterDot] 
             (c \[CenterDot] a)) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 154} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] (c \[CenterDot] 
            a)) \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                a)))) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] b))) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 153}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (b \[CenterDot] (c \[CenterDot] a)) \[CenterDot] 
            (((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                   a)) \[CenterDot] (a \[CenterDot] a)))) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
              (a \[CenterDot] b))) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 155} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] (c \[CenterDot] 
            a)) \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                a)))) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] a))) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 154}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {2, 2, 2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (b \[CenterDot] (c \[CenterDot] a)) \[CenterDot] 
            (((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                   a)) \[CenterDot] (a \[CenterDot] a)))) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
              (b \[CenterDot] a))) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 156} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] (c \[CenterDot] 
            a)) \[CenterDot] ((a \[CenterDot] (b \[CenterDot] 
             a)) \[CenterDot] ((((a \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                a)))) \[CenterDot] (a \[CenterDot] a))) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 155}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (b \[CenterDot] (c \[CenterDot] a)) \[CenterDot] 
            ((a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
             ((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                   a)) \[CenterDot] (a \[CenterDot] a)))) \[CenterDot] 
              (a \[CenterDot] a))) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 157} -> 
     <|"Statement" -> HoldForm[
        ((a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
           ((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
               a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] (a \[CenterDot] a)))) \[CenterDot] 
            (a \[CenterDot] a))) \[CenterDot] (b \[CenterDot] 
           (c \[CenterDot] a)) == b], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 156}, "Construct" -> 
         {"SubstitutionLemma", 98}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          ((a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
             ((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                   a)) \[CenterDot] (a \[CenterDot] a)))) \[CenterDot] 
              (a \[CenterDot] a))) \[CenterDot] (b \[CenterDot] 
             (c \[CenterDot] a)) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 158} -> 
     <|"Statement" -> HoldForm[
        ((a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
           ((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
               a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] (a \[CenterDot] a)))) \[CenterDot] 
            (a \[CenterDot] a))) \[CenterDot] 
          ((c \[CenterDot] a) \[CenterDot] b) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 157}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          ((a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
             ((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                   a)) \[CenterDot] (a \[CenterDot] a)))) \[CenterDot] 
              (a \[CenterDot] a))) \[CenterDot] ((c \[CenterDot] 
              a) \[CenterDot] b) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 159} -> 
     <|"Statement" -> HoldForm[
        ((a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
           ((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
               a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] (a \[CenterDot] a)))) \[CenterDot] 
            (a \[CenterDot] a))) \[CenterDot] 
          ((a \[CenterDot] c) \[CenterDot] b) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 158}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {2, 1}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          ((a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
             ((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                   a)) \[CenterDot] (a \[CenterDot] a)))) \[CenterDot] 
              (a \[CenterDot] a))) \[CenterDot] ((a \[CenterDot] 
              c) \[CenterDot] b) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 160} -> 
     <|"Statement" -> HoldForm[((a \[CenterDot] c) \[CenterDot] 
           b) \[CenterDot] ((a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
           ((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
               a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] (a \[CenterDot] a)))) \[CenterDot] 
            (a \[CenterDot] a))) == b], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 159}, "Construct" -> 
         {"SubstitutionLemma", 98}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          ((a \[CenterDot] c) \[CenterDot] b) \[CenterDot] 
            ((a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
             ((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                   a)) \[CenterDot] (a \[CenterDot] a)))) \[CenterDot] 
              (a \[CenterDot] a))) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 161} -> 
     <|"Statement" -> HoldForm[((a \[CenterDot] c) \[CenterDot] 
           b) \[CenterDot] ((a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] a)))))) == 
         b], "Proof" -> <|"Input" -> {"SubstitutionLemma", 160}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {2, 2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          ((a \[CenterDot] c) \[CenterDot] b) \[CenterDot] 
            ((a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] (
                (a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] 
                   a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
                 (a \[CenterDot] a)))))) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 162} -> 
     <|"Statement" -> HoldForm[((a \[CenterDot] c) \[CenterDot] 
           b) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
            (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
               a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] (a \[CenterDot] a))))) \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] a))) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 161}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          ((a \[CenterDot] c) \[CenterDot] b) \[CenterDot] 
            (((a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] (
                (a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] 
                   a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
                 (a \[CenterDot] a))))) \[CenterDot] (a \[CenterDot] 
              (b \[CenterDot] a))) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 163} -> 
     <|"Statement" -> HoldForm[(((a \[CenterDot] a) \[CenterDot] 
            (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
               a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] (a \[CenterDot] a))))) \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] a))) \[CenterDot] 
          ((a \[CenterDot] c) \[CenterDot] b) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 162}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (((a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
                  (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                  a))))) \[CenterDot] (a \[CenterDot] (b \[CenterDot] 
               a))) \[CenterDot] ((a \[CenterDot] c) \[CenterDot] b) == b], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 164} -> 
     <|"Statement" -> HoldForm[(((a \[CenterDot] a) \[CenterDot] 
            (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
               a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] (a \[CenterDot] a))))) \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] a))) \[CenterDot] 
          (b \[CenterDot] (a \[CenterDot] c)) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 163}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (((a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
                  (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                  a))))) \[CenterDot] (a \[CenterDot] (b \[CenterDot] 
               a))) \[CenterDot] (b \[CenterDot] (a \[CenterDot] c)) == b], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 165} -> 
     <|"Statement" -> HoldForm[(((a \[CenterDot] a) \[CenterDot] 
            (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
               a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] (a \[CenterDot] a))))) \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] a))) \[CenterDot] 
          (b \[CenterDot] (c \[CenterDot] a)) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 164}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {2, 2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (((a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
                  (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                  a))))) \[CenterDot] (a \[CenterDot] (b \[CenterDot] 
               a))) \[CenterDot] (b \[CenterDot] (c \[CenterDot] a)) == b], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 166} -> 
     <|"Statement" -> HoldForm[(((a \[CenterDot] a) \[CenterDot] 
            (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
               a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] (a \[CenterDot] a))))) \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (b \[CenterDot] (c \[CenterDot] a)) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 165}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {1, 2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (((a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
                  (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                  a))))) \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
              a)) \[CenterDot] (b \[CenterDot] (c \[CenterDot] a)) == b], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 167} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] (c \[CenterDot] 
            a)) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
            (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
               a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] (a \[CenterDot] a))))) \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] a)) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 166}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (b \[CenterDot] (c \[CenterDot] a)) \[CenterDot] 
            (((a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] (
                (a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] 
                   a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
                 (a \[CenterDot] a))))) \[CenterDot] 
             ((b \[CenterDot] a) \[CenterDot] a)) == b], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 168} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] (c \[CenterDot] 
            a)) \[CenterDot] (((b \[CenterDot] a) \[CenterDot] 
            a) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
            (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
               a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] (a \[CenterDot] a)))))) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 167}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (b \[CenterDot] (c \[CenterDot] a)) \[CenterDot] 
            (((b \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] (
                (a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] 
                   a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
                 (a \[CenterDot] a)))))) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 169} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] (c \[CenterDot] 
            a)) \[CenterDot] (((b \[CenterDot] a) \[CenterDot] 
            a) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
            (((a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] (
                a \[CenterDot] a))) \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] (a \[CenterDot] a))))) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 168}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {2, 2, 2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (b \[CenterDot] (c \[CenterDot] a)) \[CenterDot] 
            (((b \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] 
                 a) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
                  (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                  a))) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a))))) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 170} -> 
     <|"Statement" -> HoldForm[(((b \[CenterDot] a) \[CenterDot] 
            a) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
            (((a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] (
                a \[CenterDot] a))) \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] (a \[CenterDot] a))))) \[CenterDot] 
          (b \[CenterDot] (c \[CenterDot] a)) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 169}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (((b \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] 
                 a) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
                  (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                  a))) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a))))) \[CenterDot] (b \[CenterDot] 
             (c \[CenterDot] a)) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 171} -> 
     <|"Statement" -> HoldForm[(((b \[CenterDot] a) \[CenterDot] 
            a) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
            (((a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] (
                a \[CenterDot] a))) \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] (a \[CenterDot] a))))) \[CenterDot] 
          ((c \[CenterDot] a) \[CenterDot] b) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 170}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (((b \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] 
                 a) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
                  (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                  a))) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a))))) \[CenterDot] 
            ((c \[CenterDot] a) \[CenterDot] b) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 172} -> 
     <|"Statement" -> HoldForm[(((b \[CenterDot] a) \[CenterDot] 
            a) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
            (((a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] (
                a \[CenterDot] a))) \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] (a \[CenterDot] a))))) \[CenterDot] 
          ((a \[CenterDot] c) \[CenterDot] b) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 171}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {2, 1}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (((b \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] 
                 a) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
                  (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                  a))) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a))))) \[CenterDot] 
            ((a \[CenterDot] c) \[CenterDot] b) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 173} -> 
     <|"Statement" -> HoldForm[((a \[CenterDot] c) \[CenterDot] 
           b) \[CenterDot] (((b \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
              (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a))))) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 172}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          ((a \[CenterDot] c) \[CenterDot] b) \[CenterDot] 
            (((b \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] 
                 a) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
                  (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                  a))) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a))))) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 174} -> 
     <|"Statement" -> HoldForm[((a \[CenterDot] c) \[CenterDot] 
           b) \[CenterDot] (((b \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
           ((((a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] (
                a \[CenterDot] a))) \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
            (a \[CenterDot] a))) == b], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 173}, "Construct" -> 
         {"SubstitutionLemma", 98}, "Position" -> {2, 2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          ((a \[CenterDot] c) \[CenterDot] b) \[CenterDot] 
            (((b \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
             ((((a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] 
                   a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
                 (a \[CenterDot] a))) \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
              (a \[CenterDot] a))) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 175} -> 
     <|"Statement" -> HoldForm[((a \[CenterDot] c) \[CenterDot] 
           b) \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
              (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
               a))) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] a)) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 174}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          ((a \[CenterDot] c) \[CenterDot] b) \[CenterDot] 
            (((((a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] 
                   a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
                 (a \[CenterDot] a))) \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((b \[CenterDot] 
               a) \[CenterDot] a)) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 176} -> 
     <|"Statement" -> HoldForm[
        (((((a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                a))) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] a))) \[CenterDot] (a \[CenterDot] 
             a)) \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
            a)) \[CenterDot] ((a \[CenterDot] c) \[CenterDot] b) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 175}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (((((a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] 
                   a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
                 (a \[CenterDot] a))) \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((b \[CenterDot] 
               a) \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
              c) \[CenterDot] b) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 177} -> 
     <|"Statement" -> HoldForm[
        (((((a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                a))) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] a))) \[CenterDot] (a \[CenterDot] 
             a)) \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
            a)) \[CenterDot] (b \[CenterDot] (a \[CenterDot] c)) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 176}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (((((a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] 
                   a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
                 (a \[CenterDot] a))) \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((b \[CenterDot] 
               a) \[CenterDot] a)) \[CenterDot] (b \[CenterDot] 
             (a \[CenterDot] c)) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 178} -> 
     <|"Statement" -> HoldForm[
        (((((a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                a))) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] a))) \[CenterDot] (a \[CenterDot] 
             a)) \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
            a)) \[CenterDot] (b \[CenterDot] (c \[CenterDot] a)) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 177}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {2, 2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (((((a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] 
                   a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
                 (a \[CenterDot] a))) \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((b \[CenterDot] 
               a) \[CenterDot] a)) \[CenterDot] (b \[CenterDot] 
             (c \[CenterDot] a)) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 179} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] (c \[CenterDot] 
            a)) \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
              (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
               a))) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] a)) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 178}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (b \[CenterDot] (c \[CenterDot] a)) \[CenterDot] 
            (((((a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] 
                   a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
                 (a \[CenterDot] a))) \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((b \[CenterDot] 
               a) \[CenterDot] a)) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 180} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] (c \[CenterDot] 
            a)) \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
              (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
               a))) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] a)) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 179}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {2, 2, 1}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (b \[CenterDot] (c \[CenterDot] a)) \[CenterDot] 
            (((((a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] 
                   a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
                 (a \[CenterDot] a))) \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               b) \[CenterDot] a)) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 181} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] (c \[CenterDot] 
            a)) \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
              (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
             a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] a)) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 180}, 
        "Construct" -> {"SubstitutionLemma", 97}, "Position" -> {2, 1, 1, 2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[(b \[CenterDot] (c \[CenterDot] a)) \[CenterDot] 
            (((((a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] 
                   a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
                 (a \[CenterDot] a))) \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               b) \[CenterDot] a)) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 182} -> 
     <|"Statement" -> HoldForm[
        (((((a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                a))) \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
             a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            a)) \[CenterDot] (b \[CenterDot] (c \[CenterDot] a)) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 181}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (((((a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] 
                   a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
                 (a \[CenterDot] a))) \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               b) \[CenterDot] a)) \[CenterDot] (b \[CenterDot] 
             (c \[CenterDot] a)) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 183} -> 
     <|"Statement" -> HoldForm[
        (((((a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                a))) \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
             a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            a)) \[CenterDot] ((c \[CenterDot] a) \[CenterDot] b) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 182}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (((((a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] 
                   a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
                 (a \[CenterDot] a))) \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               b) \[CenterDot] a)) \[CenterDot] ((c \[CenterDot] 
              a) \[CenterDot] b) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 184} -> 
     <|"Statement" -> HoldForm[
        (((((a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                a))) \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
             a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            a)) \[CenterDot] ((a \[CenterDot] c) \[CenterDot] b) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 183}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {2, 1}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (((((a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] 
                   a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
                 (a \[CenterDot] a))) \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               b) \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
              c) \[CenterDot] b) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 185} -> 
     <|"Statement" -> HoldForm[((a \[CenterDot] c) \[CenterDot] 
           b) \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
              (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
             a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] a)) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 184}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          ((a \[CenterDot] c) \[CenterDot] b) \[CenterDot] 
            (((((a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] 
                   a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
                 (a \[CenterDot] a))) \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               b) \[CenterDot] a)) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 186} -> 
     <|"Statement" -> HoldForm[((a \[CenterDot] c) \[CenterDot] 
           b) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
           ((((a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] (
                a \[CenterDot] a))) \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] a))) == b], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 185}, "Construct" -> 
         {"SubstitutionLemma", 98}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          ((a \[CenterDot] c) \[CenterDot] b) \[CenterDot] 
            (((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
             ((((a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] 
                   a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
                 (a \[CenterDot] a))) \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] a))) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 187} -> 
     <|"Statement" -> HoldForm[((a \[CenterDot] c) \[CenterDot] 
           b) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
              (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] (a \[CenterDot] a))) \[CenterDot] a))) == 
         b], "Proof" -> <|"Input" -> {"SubstitutionLemma", 186}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {2, 2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          ((a \[CenterDot] c) \[CenterDot] b) \[CenterDot] 
            (((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] 
                 a) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
                  (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                  a))) \[CenterDot] a))) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 188} -> 
     <|"Statement" -> HoldForm[(((a \[CenterDot] b) \[CenterDot] 
            a) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
            (((a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] (
                a \[CenterDot] a))) \[CenterDot] a))) \[CenterDot] 
          ((a \[CenterDot] c) \[CenterDot] b) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 187}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] 
                 a) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
                  (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                  a))) \[CenterDot] a))) \[CenterDot] 
            ((a \[CenterDot] c) \[CenterDot] b) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 189} -> 
     <|"Statement" -> HoldForm[(((a \[CenterDot] b) \[CenterDot] 
            a) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
            (((a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] (
                a \[CenterDot] a))) \[CenterDot] a))) \[CenterDot] 
          (b \[CenterDot] (a \[CenterDot] c)) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 188}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] 
                 a) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
                  (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                  a))) \[CenterDot] a))) \[CenterDot] (b \[CenterDot] 
             (a \[CenterDot] c)) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 190} -> 
     <|"Statement" -> HoldForm[(((a \[CenterDot] b) \[CenterDot] 
            a) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
            (((a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] (
                a \[CenterDot] a))) \[CenterDot] a))) \[CenterDot] 
          (b \[CenterDot] (c \[CenterDot] a)) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 189}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {2, 2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] 
                 a) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
                  (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                  a))) \[CenterDot] a))) \[CenterDot] (b \[CenterDot] 
             (c \[CenterDot] a)) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 191} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] (c \[CenterDot] 
            a)) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
            a) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
            (((a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] (
                a \[CenterDot] a))) \[CenterDot] a))) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 190}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (b \[CenterDot] (c \[CenterDot] a)) \[CenterDot] 
            (((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] 
                 a) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
                  (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                  a))) \[CenterDot] a))) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 192} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] (c \[CenterDot] 
            a)) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
            a) \[CenterDot] a) == b], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 191}, "Construct" -> 
         {"SubstitutionLemma", 97}, "Position" -> {2, 2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[(b \[CenterDot] (c \[CenterDot] a)) \[CenterDot] 
            (((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] a) == b], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 193} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] (c \[CenterDot] 
            a)) \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
             b)) \[CenterDot] a) == b], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 192}, "Construct" -> 
         {"SubstitutionLemma", 98}, "Position" -> {2, 1}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (b \[CenterDot] (c \[CenterDot] a)) \[CenterDot] 
            ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] a) == b], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 194} -> 
     <|"Statement" -> HoldForm[
        ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] a) \[CenterDot] 
          (b \[CenterDot] (c \[CenterDot] a)) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 193}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] a) \[CenterDot] 
            (b \[CenterDot] (c \[CenterDot] a)) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 195} -> 
     <|"Statement" -> HoldForm[
        ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] a) \[CenterDot] 
          ((c \[CenterDot] a) \[CenterDot] b) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 194}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] a) \[CenterDot] 
            ((c \[CenterDot] a) \[CenterDot] b) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 196} -> 
     <|"Statement" -> HoldForm[
        ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] a) \[CenterDot] 
          ((a \[CenterDot] c) \[CenterDot] b) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 195}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {2, 1}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] a) \[CenterDot] 
            ((a \[CenterDot] c) \[CenterDot] b) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 197} -> 
     <|"Statement" -> HoldForm[((a \[CenterDot] c) \[CenterDot] 
           b) \[CenterDot] ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
           a) == b], "Proof" -> <|"Input" -> {"SubstitutionLemma", 196}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          ((a \[CenterDot] c) \[CenterDot] b) \[CenterDot] 
            ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] a) == b], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 198} -> 
     <|"Statement" -> HoldForm[((a \[CenterDot] c) \[CenterDot] 
           b) \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
            (a \[CenterDot] b))) == b], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 197}, "Construct" -> 
         {"SubstitutionLemma", 98}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          ((a \[CenterDot] c) \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] (a \[CenterDot] b))) == b], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 199} -> 
     <|"Statement" -> HoldForm[((a \[CenterDot] c) \[CenterDot] 
           b) \[CenterDot] (a \[CenterDot] b) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 198}, 
        "Construct" -> {"SubstitutionLemma", 79}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (b_))) -> a \[CenterDot] b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[((a \[CenterDot] c) \[CenterDot] 
             b) \[CenterDot] (a \[CenterDot] b) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 200} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
          ((a \[CenterDot] c) \[CenterDot] b) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 199}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] c) \[CenterDot] 
             b) == b], "Source" -> "cpl"|>|>, {"Conclusion", 1} -> 
     <|"Statement" -> HoldForm[b == b], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 200}, "Construct" -> 
         {"CriticalPairLemma", 49}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((a_) \[CenterDot] (c_)) \[CenterDot] (b_)) -> b, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[b == b], 
        "Source" -> "cpl"|>|>}|>]
