ProofObject["EquationalLogic", Inactive[Equal][
  (a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
   (b \[CenterDot] (c \[CenterDot] a)), b], 
 {Inactive[Equal][(((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
    ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] (a_))), c_]}, 
 <|"Variables" -> {a, b, c, x3, x4}, "Constants" -> {}, 
  "Proof" -> {{"Axiom", 1} -> <|"Statement" -> 
       HoldForm[((a \[CenterDot] b) \[CenterDot] c) \[CenterDot] 
          (a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] a)) == c], 
      "Proof" -> <||>|>, {"Hypothesis", 1} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (b \[CenterDot] (c \[CenterDot] a)) == b], "Proof" -> <||>|>, 
    {"CriticalPairLemma", 1} -> 
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
    {"CriticalPairLemma", 2} -> 
     <|"Statement" -> HoldForm[
        c == ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             a)) \[CenterDot] c) \[CenterDot] (b \[CenterDot] 
           ((b \[CenterDot] c) \[CenterDot] b))], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] 
          (b_), "MatchingConstruct" -> {"CriticalPairLemma", 1}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] 
            ((((b_) \[CenterDot] (c_)) \[CenterDot] ((b_) \[CenterDot] (
                ((b_) \[CenterDot] (a_)) \[CenterDot] (b_)))) \[CenterDot] 
             ((b_) \[CenterDot] (c_)))) -> b \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] b), "MatchingSide" -> 1, 
        "Position" -> {1, 1}|>|>, {"CriticalPairLemma", 3} -> 
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
       <|"Construct" -> {"CriticalPairLemma", 1}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] 
            ((((b_) \[CenterDot] (c_)) \[CenterDot] ((b_) \[CenterDot] (
                ((b_) \[CenterDot] (a_)) \[CenterDot] (b_)))) \[CenterDot] 
             ((b_) \[CenterDot] (c_)))) -> b \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] b), "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] (c_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 1}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CenterDot] (((b_) \[CenterDot] 
             (c_)) \[CenterDot] ((((b_) \[CenterDot] (c_)) \[CenterDot] 
              ((b_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] 
                (b_)))) \[CenterDot] ((b_) \[CenterDot] (c_)))) -> 
          b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), 
        "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[c \[CenterDot] 
          ((c \[CenterDot] a) \[CenterDot] c) == a \[CenterDot] 
          ((b \[CenterDot] ((b \[CenterDot] c) \[CenterDot] b)) \[CenterDot] 
           (((b \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
               b)) \[CenterDot] (c \[CenterDot] ((c \[CenterDot] 
                a) \[CenterDot] c))) \[CenterDot] (c \[CenterDot] 
             ((b \[CenterDot] x3) \[CenterDot] (((b \[CenterDot] 
                 x3) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                   c) \[CenterDot] b))) \[CenterDot] (b \[CenterDot] 
                x3))))))], "Proof" -> <|"Input" -> {"CriticalPairLemma", 3}, 
        "Construct" -> {"CriticalPairLemma", 1}, "Position" -> {2, 2, 1, 1}, 
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
    {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[c \[CenterDot] 
          ((c \[CenterDot] a) \[CenterDot] c) == a \[CenterDot] 
          ((b \[CenterDot] ((b \[CenterDot] c) \[CenterDot] b)) \[CenterDot] 
           (((b \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
               b)) \[CenterDot] (c \[CenterDot] ((c \[CenterDot] 
                a) \[CenterDot] c))) \[CenterDot] (b \[CenterDot] 
             ((b \[CenterDot] c) \[CenterDot] b))))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 1}, 
        "Construct" -> {"CriticalPairLemma", 1}, "Position" -> {2, 2, 2}, 
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
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 4} -> 
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
        "Position" -> {1, 1}|>|>, {"CriticalPairLemma", 5} -> 
     <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
          ((c \[CenterDot] a) \[CenterDot] 
           ((((((x3 \[CenterDot] x4) \[CenterDot] c) \[CenterDot] (
                x3 \[CenterDot] ((x3 \[CenterDot] c) \[CenterDot] 
                 x3))) \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
            ((((x3 \[CenterDot] x4) \[CenterDot] c) \[CenterDot] 
              (x3 \[CenterDot] ((x3 \[CenterDot] c) \[CenterDot] 
                x3))) \[CenterDot] a)))], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 4}, "Orientation" -> -1, 
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
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 5}, 
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
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 6} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] 
          ((b \[CenterDot] a) \[CenterDot] b) == 
         (a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
             b))) \[CenterDot] (((b \[CenterDot] c) \[CenterDot] 
            a) \[CenterDot] (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
             a)))], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 4}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((c_) \[CenterDot] (a_)) \[CenterDot] 
            ((((c_) \[CenterDot] (a_)) \[CenterDot] (b_)) \[CenterDot] 
             ((c_) \[CenterDot] (a_)))) -> b, "Side" -> 1, 
        "Subpattern" -> ((c_) \[CenterDot] (a_)) \[CenterDot] (b_), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] 
              (c_)) \[CenterDot] (a_))) -> c, "MatchingSide" -> 1, 
        "Position" -> {2, 2, 1}|>|>, {"CriticalPairLemma", 7} -> 
     <|"Statement" -> HoldForm[
        c == ((a \[CenterDot] ((a \[CenterDot] (b \[CenterDot] (
                (b \[CenterDot] c) \[CenterDot] b))) \[CenterDot] 
             a)) \[CenterDot] c) \[CenterDot] 
          ((b \[CenterDot] ((b \[CenterDot] c) \[CenterDot] b)) \[CenterDot] 
           c)], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 2}, 
        "Orientation" -> -1, "Rule" -> 
         (((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
              (a_))) \[CenterDot] (c_)) \[CenterDot] ((b_) \[CenterDot] 
            (((b_) \[CenterDot] (c_)) \[CenterDot] (b_))) -> c, "Side" -> 1, 
        "Subpattern" -> ((b_) \[CenterDot] (c_)) \[CenterDot] (b_), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] 
              (c_)) \[CenterDot] (a_))) -> c, "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 8} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] a) == 
         (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             a))) \[CenterDot] a], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 6}, "Orientation" -> -1, 
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
    {"CriticalPairLemma", 9} -> 
     <|"Statement" -> HoldForm[
        a == ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] a))) \[CenterDot] a) \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           a)], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 7}, 
        "Orientation" -> -1, "Rule" -> 
         (((a_) \[CenterDot] (((a_) \[CenterDot] ((b_) \[CenterDot] 
                (((b_) \[CenterDot] (c_)) \[CenterDot] (b_)))) \[CenterDot] 
              (a_))) \[CenterDot] (c_)) \[CenterDot] 
           (((b_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] 
              (b_))) \[CenterDot] (c_)) -> c, "Side" -> 1, 
        "Subpattern" -> ((a_) \[CenterDot] ((b_) \[CenterDot] 
            (((b_) \[CenterDot] (c_)) \[CenterDot] (b_)))) \[CenterDot] (a_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 8}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] (
                a_)) \[CenterDot] (a_)))) \[CenterDot] (a_) -> 
          a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
        "MatchingSide" -> 1, "Position" -> {1, 1, 2}|>|>, 
    {"SubstitutionLemma", 5} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           a)], "Proof" -> <|"Input" -> {"CriticalPairLemma", 9}, 
        "Construct" -> {"CriticalPairLemma", 8}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) \[CenterDot] 
           (a_) -> a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a == (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a)) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] a)) \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 10} -> 
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
         {"SubstitutionLemma", 4}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((c_) \[CenterDot] (a_)) \[CenterDot] 
            ((((c_) \[CenterDot] (a_)) \[CenterDot] (b_)) \[CenterDot] 
             ((c_) \[CenterDot] (a_)))) -> b, "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 11} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
          (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] a))) \[CenterDot] 
           (a \[CenterDot] b)) == (a \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
          (b \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              a)) \[CenterDot] b))], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 10}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            (((b_) \[CenterDot] (((c_) \[CenterDot] (b_)) \[CenterDot] (
                (((c_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
                ((c_) \[CenterDot] (b_))))) \[CenterDot] (b_))) -> 
          (c \[CenterDot] b) \[CenterDot] (((c \[CenterDot] b) \[CenterDot] 
             a) \[CenterDot] (c \[CenterDot] b)), "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] (((c_) \[CenterDot] 
            (b_)) \[CenterDot] ((((c_) \[CenterDot] (b_)) \[CenterDot] 
             (a_)) \[CenterDot] ((c_) \[CenterDot] (b_)))), 
        "MatchingConstruct" -> {"CriticalPairLemma", 1}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] 
            ((((b_) \[CenterDot] (c_)) \[CenterDot] ((b_) \[CenterDot] (
                ((b_) \[CenterDot] (a_)) \[CenterDot] (b_)))) \[CenterDot] 
             ((b_) \[CenterDot] (c_)))) -> b \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] b), "MatchingSide" -> 1, 
        "Position" -> {2, 2, 1}|>|>, {"CriticalPairLemma", 12} -> 
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
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 11}, 
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
    {"SubstitutionLemma", 6} -> 
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
              b))))], "Proof" -> <|"Input" -> {"CriticalPairLemma", 12}, 
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
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 7} -> 
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
              b))))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 6}, 
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
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 8} -> 
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
       <|"Input" -> {"SubstitutionLemma", 7}, "Construct" -> {"Axiom", 1}, 
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
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 9} -> 
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
       <|"Input" -> {"SubstitutionLemma", 8}, "Construct" -> {"Axiom", 1}, 
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
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 10} -> 
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
       <|"Input" -> {"SubstitutionLemma", 9}, "Construct" -> {"Axiom", 1}, 
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
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 13} -> 
     <|"Statement" -> HoldForm[(((a \[CenterDot] a) \[CenterDot] 
            a) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             a))) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
              a) \[CenterDot] a)) \[CenterDot] 
           ((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a))) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] a)))) == a \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] a)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 10}, 
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
        "Position" -> {2, 1, 2}|>|>, {"SubstitutionLemma", 11} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           ((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a))) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] a)))) == a \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 13}, 
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
    {"SubstitutionLemma", 12} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a)))) == a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 11}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {2, 2, 1}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
              (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)))) == 
           a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 14} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
          (((b \[CenterDot] a) \[CenterDot] ((a \[CenterDot] 
              (b \[CenterDot] a)) \[CenterDot] a)) \[CenterDot] 
           (b \[CenterDot] a)) == ((a \[CenterDot] (b \[CenterDot] 
             a)) \[CenterDot] a) \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
             a)) \[CenterDot] (((a \[CenterDot] (b \[CenterDot] 
               a)) \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] a))))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 2}, 
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
    {"CriticalPairLemma", 15} -> 
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
       <|"Construct" -> {"CriticalPairLemma", 14}, "Orientation" -> 1, 
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
        "MatchingConstruct" -> {"CriticalPairLemma", 8}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] (
                a_)) \[CenterDot] (a_)))) \[CenterDot] (a_) -> 
          a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
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
          (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            ((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
               a)) \[CenterDot] a)) \[CenterDot] 
           ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a))) \[CenterDot] a))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 15}, "Construct" -> 
         {"CriticalPairLemma", 8}, "Position" -> {2, 1, 1}, 
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
          (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a))) \[CenterDot] a)) \[CenterDot] 
           ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a))) \[CenterDot] a))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 13}, "Construct" -> 
         {"CriticalPairLemma", 8}, "Position" -> {2, 1, 2, 1, 2}, 
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
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 15} -> 
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
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 14}, 
        "Construct" -> {"CriticalPairLemma", 8}, "Position" -> {2, 1, 2}, 
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
    {"CriticalPairLemma", 16} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a))], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "Side" -> 1, "Subpattern" -> 
         ((a_) \[CenterDot] (b_)) \[CenterDot] (c_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 8}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) \[CenterDot] 
           (a_) -> a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"SubstitutionLemma", 16} -> 
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
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 15}, 
        "Construct" -> {"CriticalPairLemma", 16}, "Position" -> {2, 1}, 
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
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 17} -> 
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
             a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 16}, 
        "Construct" -> {"CriticalPairLemma", 8}, "Position" -> {2, 2}, 
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
               a)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 18} -> 
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
             a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 17}, 
        "Construct" -> {"CriticalPairLemma", 8}, "Position" -> {1, 1, 2}, 
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
               a)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 19} -> 
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
             a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 18}, 
        "Construct" -> {"CriticalPairLemma", 8}, "Position" -> {1}, 
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
               a)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 20} -> 
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
             a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 19}, 
        "Construct" -> {"CriticalPairLemma", 8}, "Position" -> {2, 1, 2, 1, 
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
               a)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 21} -> 
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
             a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 20}, 
        "Construct" -> {"CriticalPairLemma", 8}, "Position" -> {2, 1, 2}, 
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
               a)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 22} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) == 
         (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 21}, 
        "Construct" -> {"CriticalPairLemma", 6}, "Position" -> {2}, 
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
               a)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 23} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 22}, 
        "Construct" -> {"CriticalPairLemma", 16}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (a_))) \[CenterDot] ((a_) \[CenterDot] 
            (((a_) \[CenterDot] (a_)) \[CenterDot] (a_))) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a == (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a)) \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
              ((a \[CenterDot] a) \[CenterDot] a)))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 24} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] a == a \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 12}, 
        "Construct" -> {"SubstitutionLemma", 23}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (a_))) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[a \[CenterDot] a == 
           a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 25} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           a)], "Proof" -> <|"Input" -> {"SubstitutionLemma", 5}, 
        "Construct" -> {"SubstitutionLemma", 24}, "Position" -> {1}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
            ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a)) \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 26} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 25}, 
        "Construct" -> {"SubstitutionLemma", 24}, "Position" -> {2, 1}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
            ((a \[CenterDot] a) \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 17} -> 
     <|"Statement" -> HoldForm[
        c == (((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              a)) \[CenterDot] b) \[CenterDot] c) \[CenterDot] 
          (b \[CenterDot] ((b \[CenterDot] c) \[CenterDot] b))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 2}, 
        "Orientation" -> -1, "Rule" -> 
         (((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
              (a_))) \[CenterDot] (c_)) \[CenterDot] ((b_) \[CenterDot] 
            (((b_) \[CenterDot] (c_)) \[CenterDot] (b_))) -> c, "Side" -> 1, 
        "Subpattern" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] 
              (c_)) \[CenterDot] (a_))) -> c, "MatchingSide" -> 1, 
        "Position" -> {1, 1, 2}|>|>, {"CriticalPairLemma", 18} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] 
          ((b \[CenterDot] a) \[CenterDot] b) == a \[CenterDot] 
          (a \[CenterDot] ((a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                a) \[CenterDot] b))) \[CenterDot] a))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 17}, 
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
        "Position" -> {1}|>|>, {"CriticalPairLemma", 19} -> 
     <|"Statement" -> HoldForm[
        (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
          (((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
            a) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
             b))) == a \[CenterDot] (a \[CenterDot] 
           ((a \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                b)) \[CenterDot] a)) \[CenterDot] a))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 18}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
            (((a_) \[CenterDot] ((b_) \[CenterDot] (((b_) \[CenterDot] 
                 (a_)) \[CenterDot] (b_)))) \[CenterDot] (a_))) -> 
          b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), "Side" -> 1, 
        "Subpattern" -> ((b_) \[CenterDot] (a_)) \[CenterDot] (b_), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] 
              (c_)) \[CenterDot] (a_))) -> c, "MatchingSide" -> 1, 
        "Position" -> {2, 2, 1, 2, 2}|>|>, {"SubstitutionLemma", 27} -> 
     <|"Statement" -> HoldForm[
        (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
          a == a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
             ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                b)) \[CenterDot] a)) \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 19}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {2}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[(b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
              b)) \[CenterDot] a == a \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] 
                   a) \[CenterDot] b)) \[CenterDot] a)) \[CenterDot] a))], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 20} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          a == a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] a))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 27}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
            (((a_) \[CenterDot] (((b_) \[CenterDot] (((b_) \[CenterDot] 
                  (a_)) \[CenterDot] (b_))) \[CenterDot] (a_))) \[CenterDot] 
             (a_))) -> (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
             b)) \[CenterDot] a, "Side" -> 1, "Subpattern" -> 
         (b_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] (b_)), 
        "MatchingConstruct" -> {"SubstitutionLemma", 24}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)) -> 
          a \[CenterDot] a, "MatchingSide" -> 1, "Position" -> {2, 2, 1, 2, 
         1}|>|>, {"SubstitutionLemma", 28} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          a == a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
             a) \[CenterDot] a))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 20}, "Construct" -> 
         {"SubstitutionLemma", 24}, "Position" -> {2, 2, 1}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            a == a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] a))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 29} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          a == a \[CenterDot] (a \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 28}, 
        "Construct" -> {"SubstitutionLemma", 24}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            a == a \[CenterDot] (a \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 30} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] a == 
         a \[CenterDot] (a \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 29}, 
        "Construct" -> {"SubstitutionLemma", 24}, "Position" -> {1}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(a \[CenterDot] a) \[CenterDot] a == 
           a \[CenterDot] (a \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 31} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 26}, 
        "Construct" -> {"SubstitutionLemma", 30}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (a \[CenterDot] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] a))], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 21} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] a) == 
         ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
            ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
             a))) \[CenterDot] (a \[CenterDot] b)], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 6}, 
        "Orientation" -> -1, "Rule" -> 
         ((a_) \[CenterDot] ((b_) \[CenterDot] (((b_) \[CenterDot] (
                a_)) \[CenterDot] (b_)))) \[CenterDot] 
           ((((b_) \[CenterDot] (c_)) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] 
              (a_)))) -> b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), 
        "Side" -> 1, "Subpattern" -> (((b_) \[CenterDot] (c_)) \[CenterDot] 
           (a_)) \[CenterDot] ((a_) \[CenterDot] (((b_) \[CenterDot] 
             (c_)) \[CenterDot] (a_))), "MatchingConstruct" -> 
         {"SubstitutionLemma", 31}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] ((a_) \[CenterDot] (a_))) -> a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 22} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] b == 
         (a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
            a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
           (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
            (a \[CenterDot] b)))], "Proof" -> <|"Construct" -> {"Axiom", 1}, 
        "Orientation" -> 1, "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] 
              (c_)) \[CenterDot] (a_))) -> c, "Side" -> 1, 
        "Subpattern" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (c_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 21}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] ((a_) \[CenterDot] (b_))) \[CenterDot] 
              (a_)))) \[CenterDot] ((a_) \[CenterDot] (b_)) -> 
          a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
            a), "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"SubstitutionLemma", 32} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] b == 
         (a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
            a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] b))))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 22}, "Construct" -> 
         {"SubstitutionLemma", 30}, "Position" -> {2, 2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (a \[CenterDot] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CenterDot] b == 
           (a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
              a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] 
                b) \[CenterDot] (a \[CenterDot] b))))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 33} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] a == a \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 24}, 
        "Construct" -> {"SubstitutionLemma", 30}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (a \[CenterDot] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CenterDot] a == 
           a \[CenterDot] (a \[CenterDot] (a \[CenterDot] a))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 34} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] b == 
         (a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
            a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
           (a \[CenterDot] b))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 32}, "Construct" -> 
         {"SubstitutionLemma", 33}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (a_))) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CenterDot] b == 
           (a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
              a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] b))], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 23} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
          (a \[CenterDot] b) == (a \[CenterDot] b) \[CenterDot] 
          (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] b))) \[CenterDot] a))], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "Side" -> 1, "Subpattern" -> 
         ((a_) \[CenterDot] (b_)) \[CenterDot] (c_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 34}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] 
              ((a_) \[CenterDot] (b_))) \[CenterDot] (a_))) \[CenterDot] 
           (((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
             (b_))) -> a \[CenterDot] b, "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 24} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] b))) \[CenterDot] a) == 
         ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
          ((a \[CenterDot] b) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] b)) \[CenterDot] (a \[CenterDot] b)))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 1}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] 
           (((b_) \[CenterDot] (c_)) \[CenterDot] 
            ((((b_) \[CenterDot] (c_)) \[CenterDot] ((b_) \[CenterDot] (
                ((b_) \[CenterDot] (a_)) \[CenterDot] (b_)))) \[CenterDot] 
             ((b_) \[CenterDot] (c_)))) -> b \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] b), "Side" -> 1, 
        "Subpattern" -> ((b_) \[CenterDot] (c_)) \[CenterDot] 
          ((b_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] (b_))), 
        "MatchingConstruct" -> {"CriticalPairLemma", 23}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
            (((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] (
                (a_) \[CenterDot] (b_)))) \[CenterDot] (a_))) -> 
          (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b), 
        "MatchingSide" -> 1, "Position" -> {2, 2, 1}|>|>, 
    {"SubstitutionLemma", 35} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] b))) \[CenterDot] a) == 
         ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
          ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 24}, 
        "Construct" -> {"SubstitutionLemma", 30}, "Position" -> {2, 2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (a \[CenterDot] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CenterDot] 
            ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                b))) \[CenterDot] a) == ((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] b)) \[CenterDot] ((a \[CenterDot] 
              b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 36} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] b))) \[CenterDot] a) == 
         ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
          ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 35}, 
        "Construct" -> {"SubstitutionLemma", 33}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (a_))) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CenterDot] 
            ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                b))) \[CenterDot] a) == ((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] b)) \[CenterDot] ((a \[CenterDot] 
              b) \[CenterDot] (a \[CenterDot] b))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 37} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 16}, 
        "Construct" -> {"SubstitutionLemma", 24}, "Position" -> {1}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 38} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] a)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 37}, "Construct" -> 
         {"SubstitutionLemma", 24}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 39} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] b))) \[CenterDot] a) == a \[CenterDot] b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 36}, 
        "Construct" -> {"SubstitutionLemma", 38}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                b) \[CenterDot] (a \[CenterDot] b))) \[CenterDot] a) == 
           a \[CenterDot] b], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 25} -> 
     <|"Statement" -> HoldForm[a == ((a \[CenterDot] b) \[CenterDot] 
           a) \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
            (a \[CenterDot] a)))], "Proof" -> <|"Construct" -> {"Axiom", 1}, 
        "Orientation" -> 1, "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] 
              (c_)) \[CenterDot] (a_))) -> c, "Side" -> 1, 
        "Subpattern" -> ((a_) \[CenterDot] (c_)) \[CenterDot] (a_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 30}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((a_) \[CenterDot] (a_)) \[CenterDot] (a_) -> a \[CenterDot] 
           (a \[CenterDot] a), "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"SubstitutionLemma", 40} -> 
     <|"Statement" -> HoldForm[a == ((a \[CenterDot] b) \[CenterDot] 
           a) \[CenterDot] (a \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 25}, 
        "Construct" -> {"SubstitutionLemma", 33}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (a_))) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a == ((a \[CenterDot] b) \[CenterDot] 
             a) \[CenterDot] (a \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 26} -> 
     <|"Statement" -> HoldForm[((a \[CenterDot] b) \[CenterDot] 
           a) \[CenterDot] (a \[CenterDot] a) == 
         ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
          ((((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] (((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] a)))) \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] a))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 39}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] 
           (((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
              ((a_) \[CenterDot] (b_)))) \[CenterDot] (a_)) -> 
          a \[CenterDot] b, "Side" -> 1, "Subpattern" -> 
         (a_) \[CenterDot] (b_), "MatchingConstruct" -> {"SubstitutionLemma", 
          40}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (a_)) -> a, "MatchingSide" -> 1, 
        "Position" -> {2, 1, 2, 1}|>|>, {"SubstitutionLemma", 41} -> 
     <|"Statement" -> HoldForm[((a \[CenterDot] b) \[CenterDot] 
           a) \[CenterDot] (a \[CenterDot] a) == 
         ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
          ((((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            a))], "Proof" -> <|"Input" -> {"CriticalPairLemma", 26}, 
        "Construct" -> {"SubstitutionLemma", 40}, "Position" -> {2, 1, 2, 2}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (a_)) -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[((a \[CenterDot] b) \[CenterDot] 
             a) \[CenterDot] (a \[CenterDot] a) == 
           ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            ((((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               b) \[CenterDot] a))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 42} -> 
     <|"Statement" -> HoldForm[((a \[CenterDot] b) \[CenterDot] 
           a) \[CenterDot] (a \[CenterDot] a) == 
         ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 41}, 
        "Construct" -> {"SubstitutionLemma", 40}, "Position" -> {2, 1}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (a_)) -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[((a \[CenterDot] b) \[CenterDot] 
             a) \[CenterDot] (a \[CenterDot] a) == 
           ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 43} -> 
     <|"Statement" -> HoldForm[a == ((a \[CenterDot] b) \[CenterDot] 
           a) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            a))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 42}, 
        "Construct" -> {"SubstitutionLemma", 40}, "Position" -> {}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (a_)) -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a == ((a \[CenterDot] b) \[CenterDot] 
             a) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              a))], "Source" -> "norm"|>|>, {"CriticalPairLemma", 27} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] a == 
         (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
          (((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
           ((((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
              b) \[CenterDot] a)))], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 43}, "Orientation" -> -1, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_))) -> a, "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] 
          (b_), "MatchingConstruct" -> {"SubstitutionLemma", 40}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (a_)) -> a, "MatchingSide" -> 1, 
        "Position" -> {1, 1}|>|>, {"SubstitutionLemma", 44} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] a == 
         (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
          (((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
           (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 27}, 
        "Construct" -> {"SubstitutionLemma", 40}, "Position" -> {2, 2, 1}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (a_)) -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[(a \[CenterDot] b) \[CenterDot] a == 
           (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
            (((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 45} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] a == 
         (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
          a], "Proof" -> <|"Input" -> {"SubstitutionLemma", 44}, 
        "Construct" -> {"SubstitutionLemma", 43}, "Position" -> {2}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_))) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[(a \[CenterDot] b) \[CenterDot] a == 
           (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
            a], "Source" -> "norm"|>|>, {"CriticalPairLemma", 28} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] 
          ((b \[CenterDot] a) \[CenterDot] b) == a \[CenterDot] 
          (((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
            (((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                b)) \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
              ((b \[CenterDot] a) \[CenterDot] b)))) \[CenterDot] 
           ((((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                b)) \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
              ((b \[CenterDot] a) \[CenterDot] b))) \[CenterDot] 
            ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b)) \[CenterDot] (((b \[CenterDot] ((b \[CenterDot] 
                  a) \[CenterDot] b)) \[CenterDot] b) \[CenterDot] 
              (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b))))))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 2}, 
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
        "MatchingConstruct" -> {"SubstitutionLemma", 45}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_))) \[CenterDot] (a_) -> (a \[CenterDot] b) \[CenterDot] a, 
        "MatchingSide" -> 1, "Position" -> {2, 2, 1}|>|>, 
    {"SubstitutionLemma", 46} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] 
          ((b \[CenterDot] a) \[CenterDot] b) == a \[CenterDot] 
          (((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
            (((b \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
             (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b)))) \[CenterDot] ((((b \[CenterDot] ((b \[CenterDot] 
                 a) \[CenterDot] b)) \[CenterDot] b) \[CenterDot] 
             (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b))) \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] 
                a) \[CenterDot] b)) \[CenterDot] (((b \[CenterDot] 
                ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
               b) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                 a) \[CenterDot] b))))))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 28}, "Construct" -> 
         {"SubstitutionLemma", 45}, "Position" -> {2, 1, 2, 1}, 
        "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_))) \[CenterDot] (a_) -> (a \[CenterDot] b) \[CenterDot] a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b) == 
           a \[CenterDot] (((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                b)) \[CenterDot] (((b \[CenterDot] a) \[CenterDot] 
                b) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                  a) \[CenterDot] b)))) \[CenterDot] 
             ((((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                  b)) \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
                ((b \[CenterDot] a) \[CenterDot] b))) \[CenterDot] 
              ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                 b)) \[CenterDot] (((b \[CenterDot] ((b \[CenterDot] 
                    a) \[CenterDot] b)) \[CenterDot] b) \[CenterDot] 
                (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b))))))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 47} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] 
          ((b \[CenterDot] a) \[CenterDot] b) == a \[CenterDot] 
          (((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
            b) \[CenterDot] ((((b \[CenterDot] ((b \[CenterDot] 
                 a) \[CenterDot] b)) \[CenterDot] b) \[CenterDot] 
             (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b))) \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] 
                a) \[CenterDot] b)) \[CenterDot] (((b \[CenterDot] 
                ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
               b) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                 a) \[CenterDot] b))))))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 46}, "Construct" -> 
         {"SubstitutionLemma", 43}, "Position" -> {2, 1, 2}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_))) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b) == 
           a \[CenterDot] (((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                b)) \[CenterDot] b) \[CenterDot] 
             ((((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                  b)) \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
                ((b \[CenterDot] a) \[CenterDot] b))) \[CenterDot] 
              ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                 b)) \[CenterDot] (((b \[CenterDot] ((b \[CenterDot] 
                    a) \[CenterDot] b)) \[CenterDot] b) \[CenterDot] 
                (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b))))))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 48} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] 
          ((b \[CenterDot] a) \[CenterDot] b) == a \[CenterDot] 
          (((b \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
           ((((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                b)) \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
              ((b \[CenterDot] a) \[CenterDot] b))) \[CenterDot] 
            ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b)) \[CenterDot] (((b \[CenterDot] ((b \[CenterDot] 
                  a) \[CenterDot] b)) \[CenterDot] b) \[CenterDot] 
              (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b))))))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 47}, 
        "Construct" -> {"SubstitutionLemma", 45}, "Position" -> {2, 1}, 
        "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_))) \[CenterDot] (a_) -> (a \[CenterDot] b) \[CenterDot] a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b) == 
           a \[CenterDot] (((b \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
             ((((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                  b)) \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
                ((b \[CenterDot] a) \[CenterDot] b))) \[CenterDot] 
              ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                 b)) \[CenterDot] (((b \[CenterDot] ((b \[CenterDot] 
                    a) \[CenterDot] b)) \[CenterDot] b) \[CenterDot] 
                (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b))))))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 49} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] 
          ((b \[CenterDot] a) \[CenterDot] b) == a \[CenterDot] 
          (((b \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
           (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 48}, 
        "Construct" -> {"SubstitutionLemma", 43}, "Position" -> {2, 2}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_))) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b) == 
           a \[CenterDot] (((b \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
             (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 50} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] 
          ((b \[CenterDot] a) \[CenterDot] b) == a \[CenterDot] b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 49}, 
        "Construct" -> {"SubstitutionLemma", 43}, "Position" -> {2}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_))) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b) == 
           a \[CenterDot] b], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 51} -> 
     <|"Statement" -> HoldForm[c == ((b \[CenterDot] a) \[CenterDot] 
           c) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
            b))], "Proof" -> <|"Input" -> {"CriticalPairLemma", 2}, 
        "Construct" -> {"SubstitutionLemma", 50}, "Position" -> {1, 1}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (a_)) -> b \[CenterDot] a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[c == ((b \[CenterDot] a) \[CenterDot] 
             c) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
              b))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 52} -> 
     <|"Statement" -> HoldForm[c == ((b \[CenterDot] a) \[CenterDot] 
           c) \[CenterDot] (c \[CenterDot] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 51}, 
        "Construct" -> {"SubstitutionLemma", 50}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (a_)) -> b \[CenterDot] a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[c == ((b \[CenterDot] a) \[CenterDot] 
             c) \[CenterDot] (c \[CenterDot] b)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 29} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] a == 
         (((a \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
           (a \[CenterDot] a)) \[CenterDot] a], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 40}, 
        "Orientation" -> -1, "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (a_)) \[CenterDot] ((a_) \[CenterDot] (a_)) -> a, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (a_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 38}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (a_)) -> a, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 30} -> 
     <|"Statement" -> HoldForm[
        ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           (((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b)) \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
             ((b \[CenterDot] a) \[CenterDot] b)))) \[CenterDot] a == 
         a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
             (((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                 b)) \[CenterDot] a) \[CenterDot] a)) \[CenterDot] a))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 27}, 
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
         2}|>|>, {"SubstitutionLemma", 53} -> 
     <|"Statement" -> HoldForm[
        ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           a) \[CenterDot] a == a \[CenterDot] (a \[CenterDot] 
           ((a \[CenterDot] (((b \[CenterDot] ((b \[CenterDot] 
                  a) \[CenterDot] b)) \[CenterDot] a) \[CenterDot] 
              a)) \[CenterDot] a))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 30}, "Construct" -> {"Axiom", 1}, 
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
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 31} -> 
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
    {"CriticalPairLemma", 32} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          (((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
            a) \[CenterDot] a)], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 4}, "Orientation" -> -1, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           ((((c_) \[CenterDot] (x3_)) \[CenterDot] (a_)) \[CenterDot] 
            (((((c_) \[CenterDot] (x3_)) \[CenterDot] (a_)) \[CenterDot] 
              (b_)) \[CenterDot] (((c_) \[CenterDot] (x3_)) \[CenterDot] 
              (a_)))) -> b, "Side" -> 1, "Subpattern" -> 
         ((((c_) \[CenterDot] (x3_)) \[CenterDot] (a_)) \[CenterDot] 
           (b_)) \[CenterDot] (((c_) \[CenterDot] (x3_)) \[CenterDot] (a_)), 
        "MatchingConstruct" -> {"CriticalPairLemma", 31}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] (
                a_))) \[CenterDot] (c_)) \[CenterDot] (b_)) \[CenterDot] 
           (((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
              (a_))) \[CenterDot] (b_)) -> b, "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 33} -> 
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
        "MatchingConstruct" -> {"CriticalPairLemma", 32}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] (a_)) \[CenterDot] 
           ((((b_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] (
                b_))) \[CenterDot] (a_)) \[CenterDot] (a_)) -> a, 
        "MatchingSide" -> 1, "Position" -> {2, 2, 1}|>|>, 
    {"SubstitutionLemma", 54} -> 
     <|"Statement" -> HoldForm[
        ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           a) \[CenterDot] a == (a \[CenterDot] 
           (((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b)) \[CenterDot] a) \[CenterDot] a)) \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 33}, 
        "Construct" -> {"SubstitutionLemma", 31}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            ((a_) \[CenterDot] (a_))) -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[
          ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
             a) \[CenterDot] a == (a \[CenterDot] (((b \[CenterDot] 
                ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
               a) \[CenterDot] a)) \[CenterDot] a], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 55} -> 
     <|"Statement" -> HoldForm[
        ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           a) \[CenterDot] a == a \[CenterDot] (a \[CenterDot] 
           (((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b)) \[CenterDot] a) \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 53}, 
        "Construct" -> {"SubstitutionLemma", 54}, "Position" -> {2, 2}, 
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
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 34} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] 
          ((b \[CenterDot] a) \[CenterDot] b) == a \[CenterDot] 
          ((b \[CenterDot] ((b \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                 a) \[CenterDot] b))) \[CenterDot] b)) \[CenterDot] 
           (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 1}, 
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
        "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 35} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] a == a \[CenterDot] 
          ((a \[CenterDot] b) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] b)))], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "Side" -> 1, "Subpattern" -> 
         ((a_) \[CenterDot] (b_)) \[CenterDot] (c_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 40}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (a_)) \[CenterDot] ((a_) \[CenterDot] (a_)) -> a, 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 36} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                a)) \[CenterDot] (a \[CenterDot] b)))) \[CenterDot] a) == 
         ((a \[CenterDot] b) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
             b))) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
              (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a))) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
            ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (
                ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                  a)) \[CenterDot] (a \[CenterDot] b)))) \[CenterDot] a)))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 34}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] 
           (((b_) \[CenterDot] (((b_) \[CenterDot] ((b_) \[CenterDot] 
                (((b_) \[CenterDot] (a_)) \[CenterDot] (b_)))) \[CenterDot] 
              (b_))) \[CenterDot] ((b_) \[CenterDot] (((b_) \[CenterDot] (
                a_)) \[CenterDot] (b_)))) -> b \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] b), "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] (a_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 35}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CenterDot] (((a_) \[CenterDot] 
             (b_)) \[CenterDot] ((((a_) \[CenterDot] (b_)) \[CenterDot] 
              ((a_) \[CenterDot] (a_))) \[CenterDot] ((a_) \[CenterDot] 
              (b_)))) -> a \[CenterDot] a, "MatchingSide" -> 1, 
        "Position" -> {2, 1, 2, 1, 2, 2, 1}|>|>, {"SubstitutionLemma", 56} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                a)) \[CenterDot] (a \[CenterDot] b)))) \[CenterDot] a) == 
         ((a \[CenterDot] b) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
             b))) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
              (a \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
                 a)))) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
            ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (
                ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                  a)) \[CenterDot] (a \[CenterDot] b)))) \[CenterDot] a)))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 36}, 
        "Construct" -> {"SubstitutionLemma", 30}, "Position" -> {2, 1, 2, 1, 
         2, 2}, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (a \[CenterDot] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CenterDot] 
            ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (
                ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                  a)) \[CenterDot] (a \[CenterDot] b)))) \[CenterDot] a) == 
           ((a \[CenterDot] b) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
               (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
               b))) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                (a \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
                   a)))) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
              ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
                 (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                    a)) \[CenterDot] (a \[CenterDot] b)))) \[CenterDot] 
               a)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 57} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                a)) \[CenterDot] (a \[CenterDot] b)))) \[CenterDot] a) == 
         ((a \[CenterDot] b) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
             b))) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (
                ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                  a)) \[CenterDot] (a \[CenterDot] b)))) \[CenterDot] a)))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 56}, 
        "Construct" -> {"SubstitutionLemma", 33}, "Position" -> {2, 1, 2, 1, 
         2}, "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
            ((a_) \[CenterDot] (a_))) -> a \[CenterDot] a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (
                ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                  a)) \[CenterDot] (a \[CenterDot] b)))) \[CenterDot] a) == 
           ((a \[CenterDot] b) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
               (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
               b))) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                (a \[CenterDot] a)) \[CenterDot] a)) \[CenterDot] 
             (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                  b) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
                   (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                   b)))) \[CenterDot] a)))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 58} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] a) == 
         (a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 8}, 
        "Construct" -> {"SubstitutionLemma", 24}, "Position" -> {1, 2}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CenterDot] 
            ((a \[CenterDot] a) \[CenterDot] a) == 
           (a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] a], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 59} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
           (a \[CenterDot] a)) == (a \[CenterDot] (a \[CenterDot] 
            a)) \[CenterDot] a], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 58}, "Construct" -> 
         {"SubstitutionLemma", 30}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (a \[CenterDot] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
             (a \[CenterDot] a)) == (a \[CenterDot] (a \[CenterDot] 
              a)) \[CenterDot] a], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 60} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] a == 
         (a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 59}, 
        "Construct" -> {"SubstitutionLemma", 33}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (a_))) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CenterDot] a == 
           (a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] a], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 61} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                a)) \[CenterDot] (a \[CenterDot] b)))) \[CenterDot] a) == 
         ((a \[CenterDot] b) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
             b))) \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
             a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
              ((a \[CenterDot] b) \[CenterDot] (((a \[CenterDot] 
                  b) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
                (a \[CenterDot] b)))) \[CenterDot] a)))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 57}, 
        "Construct" -> {"SubstitutionLemma", 60}, "Position" -> {2, 1, 2}, 
        "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] (a_))) \[CenterDot] 
           (a_) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CenterDot] 
            ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (
                ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                  a)) \[CenterDot] (a \[CenterDot] b)))) \[CenterDot] a) == 
           ((a \[CenterDot] b) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
               (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
               b))) \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
               a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                ((a \[CenterDot] b) \[CenterDot] (((a \[CenterDot] 
                    b) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
                  (a \[CenterDot] b)))) \[CenterDot] a)))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 62} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                a)) \[CenterDot] (a \[CenterDot] b)))) \[CenterDot] a) == 
         ((a \[CenterDot] b) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
             b))) \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
             a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
              a) \[CenterDot] a)))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 61}, "Construct" -> 
         {"CriticalPairLemma", 35}, "Position" -> {2, 2, 2, 1}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] (
                a_))) \[CenterDot] ((a_) \[CenterDot] (b_)))) -> 
          a \[CenterDot] a, "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                b) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
                 (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                 b)))) \[CenterDot] a) == ((a \[CenterDot] b) \[CenterDot] 
             (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                a)) \[CenterDot] (a \[CenterDot] b))) \[CenterDot] 
            ((a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
             (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 63} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                a)) \[CenterDot] (a \[CenterDot] b)))) \[CenterDot] a) == 
         ((a \[CenterDot] b) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
             b))) \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
             a)) \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
             (a \[CenterDot] a))))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 62}, "Construct" -> 
         {"SubstitutionLemma", 30}, "Position" -> {2, 2, 2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (a \[CenterDot] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CenterDot] 
            ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (
                ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                  a)) \[CenterDot] (a \[CenterDot] b)))) \[CenterDot] a) == 
           ((a \[CenterDot] b) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
               (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
               b))) \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
               a)) \[CenterDot] (a \[CenterDot] (a \[CenterDot] (
                a \[CenterDot] a))))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 64} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                a)) \[CenterDot] (a \[CenterDot] b)))) \[CenterDot] a) == 
         ((a \[CenterDot] b) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
             b))) \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
             a)) \[CenterDot] (a \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 63}, 
        "Construct" -> {"SubstitutionLemma", 33}, "Position" -> {2, 2}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (a_))) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CenterDot] 
            ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (
                ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                  a)) \[CenterDot] (a \[CenterDot] b)))) \[CenterDot] a) == 
           ((a \[CenterDot] b) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
               (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
               b))) \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
               a)) \[CenterDot] (a \[CenterDot] a))], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 37} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] a))) == ((a \[CenterDot] a) \[CenterDot] 
           a) \[CenterDot] (a \[CenterDot] a)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 59}, 
        "Orientation" -> -1, "Rule" -> 
         ((a_) \[CenterDot] ((a_) \[CenterDot] (a_))) \[CenterDot] (a_) -> 
          a \[CenterDot] (a \[CenterDot] (a \[CenterDot] a)), "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (a_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 38}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (a_)) -> a, "MatchingSide" -> 1, 
        "Position" -> {1, 2}|>|>, {"SubstitutionLemma", 65} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] a))) == (a \[CenterDot] (a \[CenterDot] 
            a)) \[CenterDot] (a \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 37}, 
        "Construct" -> {"SubstitutionLemma", 30}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (a \[CenterDot] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
            ((a \[CenterDot] a) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] a))) == (a \[CenterDot] (a \[CenterDot] 
              a)) \[CenterDot] (a \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 66} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] a) == (a \[CenterDot] (a \[CenterDot] 
            a)) \[CenterDot] (a \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 65}, 
        "Construct" -> {"SubstitutionLemma", 33}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (a_))) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] a) == (a \[CenterDot] (a \[CenterDot] 
              a)) \[CenterDot] (a \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 67} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] (a \[CenterDot] 
            a)) \[CenterDot] (a \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 66}, 
        "Construct" -> {"SubstitutionLemma", 38}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[a == (a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 68} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                a)) \[CenterDot] (a \[CenterDot] b)))) \[CenterDot] a) == 
         ((a \[CenterDot] b) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
             b))) \[CenterDot] a], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 64}, "Construct" -> 
         {"SubstitutionLemma", 67}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] (a_))) \[CenterDot] 
           ((a_) \[CenterDot] (a_)) -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CenterDot] 
            ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (
                ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                  a)) \[CenterDot] (a \[CenterDot] b)))) \[CenterDot] a) == 
           ((a \[CenterDot] b) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
               (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
               b))) \[CenterDot] a], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 69} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] a) == 
         ((a \[CenterDot] b) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
             b))) \[CenterDot] a], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 68}, "Construct" -> 
         {"CriticalPairLemma", 35}, "Position" -> {2, 1}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] (
                a_))) \[CenterDot] ((a_) \[CenterDot] (b_)))) -> 
          a \[CenterDot] a, "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a) == 
           ((a \[CenterDot] b) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
               (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
               b))) \[CenterDot] a], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 70} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
           (a \[CenterDot] a)) == ((a \[CenterDot] b) \[CenterDot] 
           (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] b))) \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 69}, 
        "Construct" -> {"SubstitutionLemma", 30}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (a \[CenterDot] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
             (a \[CenterDot] a)) == ((a \[CenterDot] b) \[CenterDot] 
             (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                a)) \[CenterDot] (a \[CenterDot] b))) \[CenterDot] a], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 71} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] a == 
         ((a \[CenterDot] b) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
             b))) \[CenterDot] a], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 70}, "Construct" -> 
         {"SubstitutionLemma", 33}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (a_))) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CenterDot] a == 
           ((a \[CenterDot] b) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
               (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
               b))) \[CenterDot] a], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 38} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          ((a \[CenterDot] b) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
             a) \[CenterDot] (a \[CenterDot] b)))], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "Side" -> 1, "Subpattern" -> 
         ((a_) \[CenterDot] (b_)) \[CenterDot] (c_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 71}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] (
                a_))) \[CenterDot] ((a_) \[CenterDot] (b_)))) \[CenterDot] 
           (a_) -> a \[CenterDot] a, "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 39} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           (((a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] (
                ((b \[CenterDot] c) \[CenterDot] (b \[CenterDot] 
                  ((b \[CenterDot] a) \[CenterDot] b))) \[CenterDot] 
                (b \[CenterDot] c)))) \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
              (((b \[CenterDot] c) \[CenterDot] (b \[CenterDot] 
                 ((b \[CenterDot] a) \[CenterDot] b))) \[CenterDot] (
                b \[CenterDot] c))))))], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 38}, "Orientation" -> -1, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
           (((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
             ((a_) \[CenterDot] (b_)))) -> a, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 1}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CenterDot] (((b_) \[CenterDot] 
             (c_)) \[CenterDot] ((((b_) \[CenterDot] (c_)) \[CenterDot] 
              ((b_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] 
                (b_)))) \[CenterDot] ((b_) \[CenterDot] (c_)))) -> 
          b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), 
        "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"SubstitutionLemma", 72} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           (((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b)) \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
             ((b \[CenterDot] c) \[CenterDot] (((b \[CenterDot] 
                 c) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                   a) \[CenterDot] b))) \[CenterDot] (b \[CenterDot] 
                c))))))], "Proof" -> <|"Input" -> {"CriticalPairLemma", 39}, 
        "Construct" -> {"CriticalPairLemma", 1}, "Position" -> {2, 2, 1, 1}, 
        "Rule" -> (a_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] 
            ((((b_) \[CenterDot] (c_)) \[CenterDot] ((b_) \[CenterDot] (
                ((b_) \[CenterDot] (a_)) \[CenterDot] (b_)))) \[CenterDot] 
             ((b_) \[CenterDot] (c_)))) -> b \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
            ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b)) \[CenterDot] (((b \[CenterDot] ((b \[CenterDot] 
                  a) \[CenterDot] b)) \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
                (((b \[CenterDot] c) \[CenterDot] (b \[CenterDot] 
                   ((b \[CenterDot] a) \[CenterDot] b))) \[CenterDot] 
                 (b \[CenterDot] c))))))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 73} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           (((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b)) \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
             ((b \[CenterDot] a) \[CenterDot] b))))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 72}, 
        "Construct" -> {"CriticalPairLemma", 1}, "Position" -> {2, 2, 2}, 
        "Rule" -> (a_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] 
            ((((b_) \[CenterDot] (c_)) \[CenterDot] ((b_) \[CenterDot] (
                ((b_) \[CenterDot] (a_)) \[CenterDot] (b_)))) \[CenterDot] 
             ((b_) \[CenterDot] (c_)))) -> b \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
            ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b)) \[CenterDot] (((b \[CenterDot] ((b \[CenterDot] 
                  a) \[CenterDot] b)) \[CenterDot] a) \[CenterDot] 
              (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b))))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 74} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           a)], "Proof" -> <|"Input" -> {"SubstitutionLemma", 73}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {2, 2}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
            ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b)) \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 40} -> 
     <|"Statement" -> HoldForm[
        (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
          a == (a \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] 
               a) \[CenterDot] b)) \[CenterDot] a)) \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
            (a \[CenterDot] a)))], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 4}, "Orientation" -> -1, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((c_) \[CenterDot] (a_)) \[CenterDot] 
            ((((c_) \[CenterDot] (a_)) \[CenterDot] (b_)) \[CenterDot] 
             ((c_) \[CenterDot] (a_)))) -> b, "Side" -> 1, 
        "Subpattern" -> ((c_) \[CenterDot] (a_)) \[CenterDot] (b_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 74}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] (a_)) \[CenterDot] (((b_) \[CenterDot] 
             (((b_) \[CenterDot] (a_)) \[CenterDot] (b_))) \[CenterDot] 
            (a_)) -> a, "MatchingSide" -> 1, "Position" -> {2, 2, 1}|>|>, 
    {"SubstitutionLemma", 75} -> 
     <|"Statement" -> HoldForm[
        (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
          a == (a \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] 
               a) \[CenterDot] b)) \[CenterDot] a)) \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 40}, 
        "Construct" -> {"SubstitutionLemma", 31}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            ((a_) \[CenterDot] (a_))) -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[
          (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
            a == (a \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] 
                 a) \[CenterDot] b)) \[CenterDot] a)) \[CenterDot] a], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 76} -> 
     <|"Statement" -> HoldForm[
        (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
          a == a \[CenterDot] (a \[CenterDot] ((b \[CenterDot] 
             ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 27}, 
        "Construct" -> {"SubstitutionLemma", 75}, "Position" -> {2, 2}, 
        "Rule" -> ((a_) \[CenterDot] (((b_) \[CenterDot] (((b_) \[CenterDot] 
                (a_)) \[CenterDot] (b_))) \[CenterDot] (a_))) \[CenterDot] 
           (a_) -> (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
             b)) \[CenterDot] a, "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[(b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
              b)) \[CenterDot] a == a \[CenterDot] (a \[CenterDot] 
             ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                b)) \[CenterDot] a))], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 41} -> 
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
       <|"Construct" -> {"SubstitutionLemma", 39}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] 
             (((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] (
                b_)))) \[CenterDot] (a_)) -> a \[CenterDot] b, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 1}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CenterDot] (((b_) \[CenterDot] 
             (c_)) \[CenterDot] ((((b_) \[CenterDot] (c_)) \[CenterDot] 
              ((b_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] 
                (b_)))) \[CenterDot] ((b_) \[CenterDot] (c_)))) -> 
          b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), 
        "MatchingSide" -> 1, "Position" -> {2, 1, 2, 1}|>|>, 
    {"SubstitutionLemma", 77} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((b \[CenterDot] c) \[CenterDot] (((b \[CenterDot] c) \[CenterDot] 
             (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b))) \[CenterDot] (b \[CenterDot] c))) == 
         a \[CenterDot] ((a \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] 
                a) \[CenterDot] b)) \[CenterDot] (b \[CenterDot] 
              ((b \[CenterDot] a) \[CenterDot] b)))) \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 41}, 
        "Construct" -> {"CriticalPairLemma", 1}, "Position" -> {2, 1, 2, 2}, 
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
    {"CriticalPairLemma", 42} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] 
          ((b \[CenterDot] a) \[CenterDot] b) == a \[CenterDot] 
          ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 40}, 
        "Orientation" -> -1, "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (a_)) \[CenterDot] ((a_) \[CenterDot] (a_)) -> a, "Side" -> 1, 
        "Subpattern" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] 
              (c_)) \[CenterDot] (a_))) -> c, "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 78} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((b \[CenterDot] c) \[CenterDot] (((b \[CenterDot] c) \[CenterDot] 
             (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b))) \[CenterDot] (b \[CenterDot] c))) == 
         a \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
             b)) \[CenterDot] a)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 77}, "Construct" -> 
         {"CriticalPairLemma", 42}, "Position" -> {2, 1}, 
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
    {"SubstitutionLemma", 79} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] 
          ((b \[CenterDot] a) \[CenterDot] b) == a \[CenterDot] 
          ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           a)], "Proof" -> <|"Input" -> {"SubstitutionLemma", 78}, 
        "Construct" -> {"CriticalPairLemma", 1}, "Position" -> {}, 
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
    {"SubstitutionLemma", 80} -> 
     <|"Statement" -> HoldForm[
        (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
          a == a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
             a) \[CenterDot] b))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 76}, "Construct" -> 
         {"SubstitutionLemma", 79}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (((b_) \[CenterDot] 
             (((b_) \[CenterDot] (a_)) \[CenterDot] (b_))) \[CenterDot] 
            (a_)) -> b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
            a == a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
               a) \[CenterDot] b))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 81} -> 
     <|"Statement" -> HoldForm[
        ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           a) \[CenterDot] a == a \[CenterDot] (a \[CenterDot] 
           ((a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b))) \[CenterDot] a))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 55}, "Construct" -> 
         {"SubstitutionLemma", 80}, "Position" -> {2, 2, 1}, 
        "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_))) \[CenterDot] (b_) -> b \[CenterDot] (a \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] a)), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[
          ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
             a) \[CenterDot] a == a \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                  a) \[CenterDot] b))) \[CenterDot] a))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 82} -> 
     <|"Statement" -> HoldForm[
        ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           a) \[CenterDot] a == b \[CenterDot] 
          ((b \[CenterDot] a) \[CenterDot] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 81}, 
        "Construct" -> {"CriticalPairLemma", 18}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
            (((a_) \[CenterDot] ((b_) \[CenterDot] (((b_) \[CenterDot] 
                 (a_)) \[CenterDot] (b_)))) \[CenterDot] (a_))) -> 
          b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
             a) \[CenterDot] a == b \[CenterDot] ((b \[CenterDot] 
              a) \[CenterDot] b)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 83} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] (b \[CenterDot] 
            ((b \[CenterDot] a) \[CenterDot] b))) \[CenterDot] a == 
         b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 82}, 
        "Construct" -> {"SubstitutionLemma", 80}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_))) \[CenterDot] (b_) -> b \[CenterDot] (a \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] a)), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          (a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b))) \[CenterDot] a == b \[CenterDot] 
            ((b \[CenterDot] a) \[CenterDot] b)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 43} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] b == 
         (a \[CenterDot] ((a \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
            a)) \[CenterDot] b], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 29}, "Orientation" -> -1, 
        "Rule" -> ((((a_) \[CenterDot] (a_)) \[CenterDot] (b_)) \[CenterDot] 
            ((a_) \[CenterDot] (a_))) \[CenterDot] (a_) -> a \[CenterDot] a, 
        "Side" -> 1, "Subpattern" -> (((a_) \[CenterDot] (a_)) \[CenterDot] 
           (b_)) \[CenterDot] ((a_) \[CenterDot] (a_)), 
        "MatchingConstruct" -> {"SubstitutionLemma", 83}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((a_) \[CenterDot] ((b_) \[CenterDot] (((b_) \[CenterDot] (
                a_)) \[CenterDot] (b_)))) \[CenterDot] (a_) -> 
          b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 44} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b))], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "Side" -> 1, "Subpattern" -> 
         ((a_) \[CenterDot] (b_)) \[CenterDot] (c_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 43}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] 
              ((b_) \[CenterDot] (b_))) \[CenterDot] (a_))) \[CenterDot] 
           (b_) -> b \[CenterDot] b, "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 84} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] b)], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 44}, "Construct" -> 
         {"SubstitutionLemma", 50}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (a_)) -> b \[CenterDot] a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] b)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 45} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] b == a \[CenterDot] 
          ((a \[CenterDot] b) \[CenterDot] a)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 52}, 
        "Orientation" -> -1, "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (c_)) \[CenterDot] ((c_) \[CenterDot] (a_)) -> c, "Side" -> 1, 
        "Subpattern" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (c_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 84}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] (b_)) -> a, 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"SubstitutionLemma", 85} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] b == b \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 45}, 
        "Construct" -> {"SubstitutionLemma", 50}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (a_)) -> b \[CenterDot] a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CenterDot] b == b \[CenterDot] a], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 87} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] a == 
         (b \[CenterDot] a) \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 45}, 
        "Construct" -> {"SubstitutionLemma", 50}, "Position" -> {1}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (a_)) -> b \[CenterDot] a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[(a \[CenterDot] b) \[CenterDot] a == 
           (b \[CenterDot] a) \[CenterDot] a], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 88} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] b) == 
         (b \[CenterDot] a) \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 87}, 
        "Construct" -> {"SubstitutionLemma", 85}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a \[CenterDot] (a \[CenterDot] b) == 
           (b \[CenterDot] a) \[CenterDot] a], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 89} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] b) == 
         a \[CenterDot] (b \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 88}, 
        "Construct" -> {"SubstitutionLemma", 85}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CenterDot] (a \[CenterDot] b) == a \[CenterDot] 
            (b \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 46} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] a == a \[CenterDot] 
          ((b \[CenterDot] a) \[CenterDot] a)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 50}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] 
           (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) -> b \[CenterDot] a, 
        "Side" -> 1, "Subpattern" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
          (a_), "MatchingConstruct" -> {"SubstitutionLemma", 87}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((a_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
          (b \[CenterDot] a) \[CenterDot] a, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 91} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] a == a \[CenterDot] 
          (a \[CenterDot] (b \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 46}, 
        "Construct" -> {"SubstitutionLemma", 85}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[b \[CenterDot] a == 
           a \[CenterDot] (a \[CenterDot] (b \[CenterDot] a))], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 47} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] (b \[CenterDot] 
            c)) \[CenterDot] (a \[CenterDot] b)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 52}, 
        "Orientation" -> -1, "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (c_)) \[CenterDot] ((c_) \[CenterDot] (a_)) -> c, "Side" -> 1, 
        "Subpattern" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (c_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 85}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CenterDot] (b_) -> b \[CenterDot] a, "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 94} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] b) \[CenterDot] 
          (a \[CenterDot] (b \[CenterDot] c))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 47}, 
        "Construct" -> {"SubstitutionLemma", 85}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a == (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
             (b \[CenterDot] c))], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 48} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] b) \[CenterDot] 
          (a \[CenterDot] (c \[CenterDot] b))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 94}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) -> a, "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] (c_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 85}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"SubstitutionLemma", 86} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] (c \[CenterDot] 
            a)) \[CenterDot] (a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
            a)) == b], "Proof" -> <|"Input" -> {"Hypothesis", 1}, 
        "Construct" -> {"SubstitutionLemma", 85}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (b \[CenterDot] (c \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] a)) == b], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 90} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] (c \[CenterDot] 
            a)) \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] a))) == b], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 86}, "Construct" -> 
         {"SubstitutionLemma", 89}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (a_)) -> 
          a \[CenterDot] (a \[CenterDot] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          (b \[CenterDot] (c \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] a))) == b], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 92} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] (c \[CenterDot] 
            a)) \[CenterDot] (b \[CenterDot] a) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 90}, 
        "Construct" -> {"SubstitutionLemma", 91}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] 
             (a_))) -> b \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          (b \[CenterDot] (c \[CenterDot] a)) \[CenterDot] 
            (b \[CenterDot] a) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 93} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
          (b \[CenterDot] (c \[CenterDot] a)) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 92}, 
        "Construct" -> {"SubstitutionLemma", 85}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (b \[CenterDot] a) \[CenterDot] (b \[CenterDot] (c \[CenterDot] 
              a)) == b], "Source" -> "cpl"|>|>, {"Conclusion", 1} -> 
     <|"Statement" -> HoldForm[b == b], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 93}, "Construct" -> 
         {"CriticalPairLemma", 48}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
            ((c_) \[CenterDot] (b_))) -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[b == b], "Source" -> "cpl"|>|>}|>]
