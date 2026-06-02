ProofObject["EquationalLogic", Inactive[Equal][
  (a \[CenterDot] a) \[CenterDot] (a \[CenterDot] b), a], 
 {Inactive[Equal][(((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
    ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] (a_))), c_]}, 
 <|"Variables" -> {a, b, c, x3, x4}, "Constants" -> {}, 
  "Proof" -> {{"Axiom", 1} -> <|"Statement" -> 
       HoldForm[((a \[CenterDot] b) \[CenterDot] c) \[CenterDot] 
          (a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] a)) == c], 
      "Proof" -> <||>|>, {"Hypothesis", 1} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] b) == a], "Proof" -> <||>|>, 
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
    {"CriticalPairLemma", 3} -> 
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
        "MatchingConstruct" -> {"CriticalPairLemma", 2}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] (
                a_))) \[CenterDot] (c_)) \[CenterDot] (b_)) \[CenterDot] 
           (((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
              (a_))) \[CenterDot] (b_)) -> b, "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 4} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          ((((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b)) \[CenterDot] a) \[CenterDot] a) \[CenterDot] a)], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 3}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
           ((((b_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] (
                b_))) \[CenterDot] (a_)) \[CenterDot] (a_)) -> a, 
        "Side" -> 1, "Subpattern" -> ((b_) \[CenterDot] (a_)) \[CenterDot] 
          (b_), "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 
         1, "MatchingRule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] 
              (c_)) \[CenterDot] (a_))) -> c, "MatchingSide" -> 1, 
        "Position" -> {2, 1, 1, 2}|>|>, {"CriticalPairLemma", 5} -> 
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
        "Position" -> {1, 1}|>|>, {"CriticalPairLemma", 7} -> 
     <|"Statement" -> HoldForm[
        c == (((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              a)) \[CenterDot] b) \[CenterDot] c) \[CenterDot] 
          (b \[CenterDot] ((b \[CenterDot] c) \[CenterDot] b))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 6}, 
        "Orientation" -> -1, "Rule" -> 
         (((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
              (a_))) \[CenterDot] (c_)) \[CenterDot] ((b_) \[CenterDot] 
            (((b_) \[CenterDot] (c_)) \[CenterDot] (b_))) -> c, "Side" -> 1, 
        "Subpattern" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] 
              (c_)) \[CenterDot] (a_))) -> c, "MatchingSide" -> 1, 
        "Position" -> {1, 1, 2}|>|>, {"CriticalPairLemma", 8} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] 
          ((b \[CenterDot] a) \[CenterDot] b) == a \[CenterDot] 
          (a \[CenterDot] ((a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                a) \[CenterDot] b))) \[CenterDot] a))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 7}, 
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
        "Position" -> {1}|>|>, {"CriticalPairLemma", 9} -> 
     <|"Statement" -> HoldForm[
        (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
          (((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
            a) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
             b))) == a \[CenterDot] (a \[CenterDot] 
           ((a \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                b)) \[CenterDot] a)) \[CenterDot] a))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 8}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
            (((a_) \[CenterDot] ((b_) \[CenterDot] (((b_) \[CenterDot] 
                 (a_)) \[CenterDot] (b_)))) \[CenterDot] (a_))) -> 
          b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), "Side" -> 1, 
        "Subpattern" -> ((b_) \[CenterDot] (a_)) \[CenterDot] (b_), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] 
              (c_)) \[CenterDot] (a_))) -> c, "MatchingSide" -> 1, 
        "Position" -> {2, 2, 1, 2, 2}|>|>, {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[
        (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
          a == a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
             ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                b)) \[CenterDot] a)) \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 9}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {2}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[(b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
              b)) \[CenterDot] a == a \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] 
                   a) \[CenterDot] b)) \[CenterDot] a)) \[CenterDot] a))], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 10} -> 
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
    {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
          ((c \[CenterDot] a) \[CenterDot] (((c \[CenterDot] a) \[CenterDot] 
             b) \[CenterDot] ((((x3 \[CenterDot] x4) \[CenterDot] 
               c) \[CenterDot] (x3 \[CenterDot] ((x3 \[CenterDot] 
                 c) \[CenterDot] x3))) \[CenterDot] a)))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 10}, 
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
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 3} -> 
     <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
          ((c \[CenterDot] a) \[CenterDot] (((c \[CenterDot] a) \[CenterDot] 
             b) \[CenterDot] (c \[CenterDot] a)))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 2}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {2, 2, 2, 1}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
            ((c \[CenterDot] a) \[CenterDot] (((c \[CenterDot] 
                a) \[CenterDot] b) \[CenterDot] (c \[CenterDot] a)))], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 11} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] 
          ((b \[CenterDot] a) \[CenterDot] b) == a \[CenterDot] 
          ((b \[CenterDot] ((b \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                 a) \[CenterDot] b))) \[CenterDot] b)) \[CenterDot] 
           (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 5}, 
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
        "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 12} -> 
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
         {"SubstitutionLemma", 3}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((c_) \[CenterDot] (a_)) \[CenterDot] 
            ((((c_) \[CenterDot] (a_)) \[CenterDot] (b_)) \[CenterDot] 
             ((c_) \[CenterDot] (a_)))) -> b, "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 13} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
          (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] a))) \[CenterDot] 
           (a \[CenterDot] b)) == (a \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
          (b \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              a)) \[CenterDot] b))], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 12}, "Orientation" -> -1, 
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
    {"SubstitutionLemma", 4} -> 
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
               a)))) \[CenterDot] (((b \[CenterDot] c) \[CenterDot] 
             a) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
              b))))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 4}, 
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
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 6} -> 
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
       <|"Input" -> {"SubstitutionLemma", 5}, "Construct" -> {"Axiom", 1}, 
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
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 7} -> 
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
       <|"Input" -> {"SubstitutionLemma", 6}, "Construct" -> {"Axiom", 1}, 
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
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 8} -> 
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
       <|"Input" -> {"SubstitutionLemma", 7}, "Construct" -> {"Axiom", 1}, 
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
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 8}, 
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
        "Position" -> {2, 1, 2}|>|>, {"SubstitutionLemma", 9} -> 
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
    {"SubstitutionLemma", 10} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a)))) == a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 9}, 
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
         {"SubstitutionLemma", 3}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((c_) \[CenterDot] (a_)) \[CenterDot] 
            ((((c_) \[CenterDot] (a_)) \[CenterDot] (b_)) \[CenterDot] 
             ((c_) \[CenterDot] (a_)))) -> b, "MatchingSide" -> 1, 
        "Position" -> {2, 2, 1}|>|>, {"CriticalPairLemma", 17} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
          (((b \[CenterDot] a) \[CenterDot] ((a \[CenterDot] 
              (b \[CenterDot] a)) \[CenterDot] a)) \[CenterDot] 
           (b \[CenterDot] a)) == ((a \[CenterDot] (b \[CenterDot] 
             a)) \[CenterDot] a) \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
             a)) \[CenterDot] (((a \[CenterDot] (b \[CenterDot] 
               a)) \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] a))))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 16}, 
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
         {"CriticalPairLemma", 6}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (((a_) \[CenterDot] (((a_) \[CenterDot] (
                b_)) \[CenterDot] (a_))) \[CenterDot] (c_)) \[CenterDot] 
           ((b_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] 
             (b_))) -> c, "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 18} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] 
          ((b \[CenterDot] a) \[CenterDot] b) == 
         (a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
             b))) \[CenterDot] (((b \[CenterDot] c) \[CenterDot] 
            a) \[CenterDot] (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
             a)))], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 3}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((c_) \[CenterDot] (a_)) \[CenterDot] 
            ((((c_) \[CenterDot] (a_)) \[CenterDot] (b_)) \[CenterDot] 
             ((c_) \[CenterDot] (a_)))) -> b, "Side" -> 1, 
        "Subpattern" -> ((c_) \[CenterDot] (a_)) \[CenterDot] (b_), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] 
              (c_)) \[CenterDot] (a_))) -> c, "MatchingSide" -> 1, 
        "Position" -> {2, 2, 1}|>|>, {"CriticalPairLemma", 19} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] a) == 
         (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             a))) \[CenterDot] a], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 18}, "Orientation" -> -1, 
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
    {"CriticalPairLemma", 20} -> 
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
       <|"Construct" -> {"CriticalPairLemma", 17}, "Orientation" -> 1, 
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
        "MatchingConstruct" -> {"CriticalPairLemma", 19}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] (
                a_)) \[CenterDot] (a_)))) \[CenterDot] (a_) -> 
          a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"SubstitutionLemma", 11} -> 
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
       <|"Input" -> {"CriticalPairLemma", 20}, "Construct" -> 
         {"CriticalPairLemma", 19}, "Position" -> {2, 1, 1}, 
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
            ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a))) \[CenterDot] a)) \[CenterDot] 
           ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a))) \[CenterDot] a))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 11}, "Construct" -> 
         {"CriticalPairLemma", 19}, "Position" -> {2, 1, 2, 1, 2}, 
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
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 13} -> 
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
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 12}, 
        "Construct" -> {"CriticalPairLemma", 19}, "Position" -> {2, 1, 2}, 
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
    {"CriticalPairLemma", 21} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a))], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "Side" -> 1, "Subpattern" -> 
         ((a_) \[CenterDot] (b_)) \[CenterDot] (c_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 19}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) \[CenterDot] 
           (a_) -> a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"SubstitutionLemma", 14} -> 
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
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 13}, 
        "Construct" -> {"CriticalPairLemma", 21}, "Position" -> {2, 1}, 
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
          (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 14}, 
        "Construct" -> {"CriticalPairLemma", 19}, "Position" -> {2, 2}, 
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
               a)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 16} -> 
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
             a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 15}, 
        "Construct" -> {"CriticalPairLemma", 19}, "Position" -> {1, 1, 2}, 
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
               a)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 17} -> 
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
             a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 16}, 
        "Construct" -> {"CriticalPairLemma", 19}, "Position" -> {1}, 
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
               a)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 18} -> 
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
             a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 17}, 
        "Construct" -> {"CriticalPairLemma", 19}, "Position" -> {2, 1, 2, 1, 
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
               a)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 19} -> 
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
             a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 18}, 
        "Construct" -> {"CriticalPairLemma", 19}, "Position" -> {2, 1, 2}, 
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
               a)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 20} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) == 
         (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 19}, 
        "Construct" -> {"CriticalPairLemma", 18}, "Position" -> {2}, 
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
               a)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 21} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 20}, 
        "Construct" -> {"CriticalPairLemma", 21}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (a_))) \[CenterDot] ((a_) \[CenterDot] 
            (((a_) \[CenterDot] (a_)) \[CenterDot] (a_))) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a == (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a)) \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
              ((a \[CenterDot] a) \[CenterDot] a)))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 22} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] a == a \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 10}, 
        "Construct" -> {"SubstitutionLemma", 21}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (a_))) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[a \[CenterDot] a == 
           a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 22} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          a == a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] a))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 1}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
            (((a_) \[CenterDot] (((b_) \[CenterDot] (((b_) \[CenterDot] 
                  (a_)) \[CenterDot] (b_))) \[CenterDot] (a_))) \[CenterDot] 
             (a_))) -> (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
             b)) \[CenterDot] a, "Side" -> 1, "Subpattern" -> 
         (b_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] (b_)), 
        "MatchingConstruct" -> {"SubstitutionLemma", 22}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)) -> 
          a \[CenterDot] a, "MatchingSide" -> 1, "Position" -> {2, 2, 1, 2, 
         1}|>|>, {"SubstitutionLemma", 23} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          a == a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
             a) \[CenterDot] a))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 22}, "Construct" -> 
         {"SubstitutionLemma", 22}, "Position" -> {2, 2, 1}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            a == a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] a))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 24} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          a == a \[CenterDot] (a \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 23}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            a == a \[CenterDot] (a \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 25} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] a == 
         a \[CenterDot] (a \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 24}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {1}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(a \[CenterDot] a) \[CenterDot] a == 
           a \[CenterDot] (a \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 23} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
          (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
           (a \[CenterDot] b)) == (a \[CenterDot] b) \[CenterDot] 
          (b \[CenterDot] ((b \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] 
                 b) \[CenterDot] (a \[CenterDot] b))))) \[CenterDot] b))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 12}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            (((b_) \[CenterDot] (((c_) \[CenterDot] (b_)) \[CenterDot] (
                (((c_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
                ((c_) \[CenterDot] (b_))))) \[CenterDot] (b_))) -> 
          (c \[CenterDot] b) \[CenterDot] (((c \[CenterDot] b) \[CenterDot] 
             a) \[CenterDot] (c \[CenterDot] b)), "Side" -> 1, 
        "Subpattern" -> (((c_) \[CenterDot] (b_)) \[CenterDot] 
           (a_)) \[CenterDot] ((c_) \[CenterDot] (b_)), 
        "MatchingConstruct" -> {"SubstitutionLemma", 25}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((a_) \[CenterDot] (a_)) \[CenterDot] (a_) -> a \[CenterDot] 
           (a \[CenterDot] a), "MatchingSide" -> 1, "Position" -> {2, 2, 1, 
         2, 2}|>|>, {"SubstitutionLemma", 26} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] a == a \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 22}, 
        "Construct" -> {"SubstitutionLemma", 25}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (a \[CenterDot] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CenterDot] a == 
           a \[CenterDot] (a \[CenterDot] (a \[CenterDot] a))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 27} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
          (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
           (a \[CenterDot] b)) == (a \[CenterDot] b) \[CenterDot] 
          (b \[CenterDot] ((b \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] b))) \[CenterDot] b))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 23}, 
        "Construct" -> {"SubstitutionLemma", 26}, "Position" -> {2, 2, 1, 2}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (a_))) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
            (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
               b)) \[CenterDot] (a \[CenterDot] b)) == 
           (a \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
             ((b \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
                (a \[CenterDot] b))) \[CenterDot] b))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 28} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
          ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b))) == (a \[CenterDot] b) \[CenterDot] 
          (b \[CenterDot] ((b \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] b))) \[CenterDot] b))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 27}, 
        "Construct" -> {"SubstitutionLemma", 25}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (a \[CenterDot] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] b))) == (a \[CenterDot] b) \[CenterDot] 
            (b \[CenterDot] ((b \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
                (a \[CenterDot] b))) \[CenterDot] b))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 29} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
          (a \[CenterDot] b) == (a \[CenterDot] b) \[CenterDot] 
          (b \[CenterDot] ((b \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] b))) \[CenterDot] b))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 28}, 
        "Construct" -> {"SubstitutionLemma", 26}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (a_))) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b) == (a \[CenterDot] b) \[CenterDot] 
            (b \[CenterDot] ((b \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
                (a \[CenterDot] b))) \[CenterDot] b))], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 24} -> 
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
       <|"Construct" -> {"CriticalPairLemma", 11}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CenterDot] (((b_) \[CenterDot] 
             (((b_) \[CenterDot] ((b_) \[CenterDot] (((b_) \[CenterDot] 
                  (a_)) \[CenterDot] (b_)))) \[CenterDot] (b_))) \[CenterDot] 
            ((b_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] 
              (b_)))) -> b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), 
        "Side" -> 1, "Subpattern" -> (b_) \[CenterDot] (a_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 29}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
            (((b_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] (
                (a_) \[CenterDot] (b_)))) \[CenterDot] (b_))) -> 
          (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b), 
        "MatchingSide" -> 1, "Position" -> {2, 1, 2, 1, 2, 2, 1}|>|>, 
    {"SubstitutionLemma", 30} -> 
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
       <|"Input" -> {"CriticalPairLemma", 24}, "Construct" -> 
         {"SubstitutionLemma", 25}, "Position" -> {2, 1, 2, 1, 2, 2}, 
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
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 31} -> 
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
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 30}, 
        "Construct" -> {"SubstitutionLemma", 26}, "Position" -> {2, 1, 2, 1, 
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
    {"SubstitutionLemma", 32} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] a) == 
         (a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 19}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {1, 2}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CenterDot] 
            ((a \[CenterDot] a) \[CenterDot] a) == 
           (a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] a], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 33} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] a == 
         (a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 32}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CenterDot] a == 
           (a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] a], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 34} -> 
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
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 31}, 
        "Construct" -> {"SubstitutionLemma", 33}, "Position" -> {2, 1, 2}, 
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
                a))))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 35} -> 
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
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 34}, 
        "Construct" -> {"SubstitutionLemma", 29}, "Position" -> {2, 2, 2, 1}, 
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
    {"SubstitutionLemma", 36} -> 
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
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 35}, 
        "Construct" -> {"SubstitutionLemma", 25}, "Position" -> {2, 2, 2}, 
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
    {"SubstitutionLemma", 37} -> 
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
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 36}, 
        "Construct" -> {"SubstitutionLemma", 26}, "Position" -> {2, 2}, 
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
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 25} -> 
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
        "Position" -> {2, 2, 1}|>|>, {"CriticalPairLemma", 26} -> 
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
       <|"Construct" -> {"CriticalPairLemma", 25}, "Orientation" -> 1, 
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
        "MatchingConstruct" -> {"CriticalPairLemma", 21}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (a_))) \[CenterDot] ((a_) \[CenterDot] 
            (((a_) \[CenterDot] (a_)) \[CenterDot] (a_))) -> a, 
        "MatchingSide" -> 1, "Position" -> {2, 1, 2, 2, 1, 2}|>|>, 
    {"SubstitutionLemma", 38} -> 
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
             a)))], "Proof" -> <|"Input" -> {"CriticalPairLemma", 26}, 
        "Construct" -> {"CriticalPairLemma", 21}, "Position" -> {2, 1}, 
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
               a)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 39} -> 
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
             a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 38}, 
        "Construct" -> {"CriticalPairLemma", 21}, "Position" -> {1, 2, 1, 2}, 
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
               a)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 40} -> 
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
             a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 39}, 
        "Construct" -> {"CriticalPairLemma", 21}, "Position" -> {2, 1}, 
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
               a)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 41} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a)) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
              ((a \[CenterDot] a) \[CenterDot] a))))) == 
         (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 40}, 
        "Construct" -> {"CriticalPairLemma", 21}, "Position" -> {2, 2, 1, 2, 
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
               a)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 42} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a)) \[CenterDot] a)) == (a \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 41}, 
        "Construct" -> {"CriticalPairLemma", 21}, "Position" -> {2, 2, 2}, 
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
               a)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 43} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a)) \[CenterDot] a)) == a], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 42}, "Construct" -> 
         {"SubstitutionLemma", 21}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (a_))) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a)) \[CenterDot] a)) == a], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 44} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a)) \[CenterDot] a)) == a], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 43}, "Construct" -> 
         {"SubstitutionLemma", 22}, "Position" -> {1}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a)) \[CenterDot] a)) == a], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 45} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) == a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 44}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {2, 2, 1}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) == a], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 46} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] a) == a], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 45}, "Construct" -> 
         {"SubstitutionLemma", 22}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] a) == a], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 27} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] a) == ((a \[CenterDot] a) \[CenterDot] 
           a) \[CenterDot] (a \[CenterDot] a)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 33}, 
        "Orientation" -> -1, "Rule" -> 
         ((a_) \[CenterDot] ((a_) \[CenterDot] (a_))) \[CenterDot] (a_) -> 
          a \[CenterDot] a, "Side" -> 1, "Subpattern" -> 
         (a_) \[CenterDot] (a_), "MatchingConstruct" -> {"SubstitutionLemma", 
          46}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] (a_)) -> a, 
        "MatchingSide" -> 1, "Position" -> {1, 2}|>|>, 
    {"SubstitutionLemma", 47} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] a) == (a \[CenterDot] (a \[CenterDot] 
            a)) \[CenterDot] (a \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 27}, 
        "Construct" -> {"SubstitutionLemma", 25}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (a \[CenterDot] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] a) == (a \[CenterDot] (a \[CenterDot] 
              a)) \[CenterDot] (a \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 48} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] (a \[CenterDot] 
            a)) \[CenterDot] (a \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 47}, 
        "Construct" -> {"SubstitutionLemma", 46}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[a == (a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 49} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
          (((b \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                (b \[CenterDot] a))) \[CenterDot] a))) \[CenterDot] 
           (b \[CenterDot] a)) == (a \[CenterDot] 
           ((a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
              (b \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
          (b \[CenterDot] a)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 37}, "Construct" -> 
         {"SubstitutionLemma", 48}, "Position" -> {2}, 
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
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 50} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
          (((b \[CenterDot] a) \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
           (b \[CenterDot] a)) == (a \[CenterDot] 
           ((a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
              (b \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
          (b \[CenterDot] a)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 49}, "Construct" -> 
         {"SubstitutionLemma", 29}, "Position" -> {2, 1}, 
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
    {"SubstitutionLemma", 51} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
          ((b \[CenterDot] a) \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
            (b \[CenterDot] a))) == (a \[CenterDot] 
           ((a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
              (b \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
          (b \[CenterDot] a)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 50}, "Construct" -> 
         {"SubstitutionLemma", 25}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (a \[CenterDot] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
            ((b \[CenterDot] a) \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
              (b \[CenterDot] a))) == (a \[CenterDot] 
             ((a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                (b \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
            (b \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 52} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
          (b \[CenterDot] a) == (a \[CenterDot] 
           ((a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
              (b \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
          (b \[CenterDot] a)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 51}, "Construct" -> 
         {"SubstitutionLemma", 26}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (a_))) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
            (b \[CenterDot] a) == (a \[CenterDot] ((a \[CenterDot] (
                (b \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
                 a))) \[CenterDot] a)) \[CenterDot] (b \[CenterDot] a)], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 28} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] b == 
         ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
          (b \[CenterDot] ((b \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
            b))], "Proof" -> <|"Construct" -> {"Axiom", 1}, 
        "Orientation" -> 1, "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] 
              (c_)) \[CenterDot] (a_))) -> c, "Side" -> 1, 
        "Subpattern" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (c_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 52}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] (((a_) \[CenterDot] (((b_) \[CenterDot] 
                (a_)) \[CenterDot] ((b_) \[CenterDot] (a_)))) \[CenterDot] 
             (a_))) \[CenterDot] ((b_) \[CenterDot] (a_)) -> 
          (b \[CenterDot] a) \[CenterDot] (b \[CenterDot] a), 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 29} -> 
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
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 28}, 
        "Orientation" -> -1, "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((a_) \[CenterDot] (b_))) \[CenterDot] ((b_) \[CenterDot] 
            (((b_) \[CenterDot] ((a_) \[CenterDot] (b_))) \[CenterDot] 
             (b_))) -> a \[CenterDot] b, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"Axiom", 1}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "MatchingSide" -> 1, "Position" -> {1, 1}|>|>, 
    {"SubstitutionLemma", 53} -> 
     <|"Statement" -> HoldForm[((b \[CenterDot] c) \[CenterDot] 
           a) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
            b)) == (a \[CenterDot] a) \[CenterDot] 
          ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           (((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b)) \[CenterDot] (((b \[CenterDot] c) \[CenterDot] 
               a) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                 a) \[CenterDot] b)))) \[CenterDot] (b \[CenterDot] 
             ((b \[CenterDot] a) \[CenterDot] b))))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 29}, 
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
    {"SubstitutionLemma", 54} -> 
     <|"Statement" -> HoldForm[((b \[CenterDot] c) \[CenterDot] 
           a) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
            b)) == (a \[CenterDot] a) \[CenterDot] 
          ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           (((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b)) \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
             ((b \[CenterDot] a) \[CenterDot] b))))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 53}, 
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
    {"SubstitutionLemma", 55} -> 
     <|"Statement" -> HoldForm[((b \[CenterDot] c) \[CenterDot] 
           a) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
            b)) == (a \[CenterDot] a) \[CenterDot] 
          ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           a)], "Proof" -> <|"Input" -> {"SubstitutionLemma", 54}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {2, 2}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
            (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) == 
           (a \[CenterDot] a) \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] 
                a) \[CenterDot] b)) \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 56} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           a)], "Proof" -> <|"Input" -> {"SubstitutionLemma", 55}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
            ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b)) \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 30} -> 
     <|"Statement" -> HoldForm[
        (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
          a == (a \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] 
               a) \[CenterDot] b)) \[CenterDot] a)) \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
            (a \[CenterDot] a)))], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 3}, "Orientation" -> -1, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((c_) \[CenterDot] (a_)) \[CenterDot] 
            ((((c_) \[CenterDot] (a_)) \[CenterDot] (b_)) \[CenterDot] 
             ((c_) \[CenterDot] (a_)))) -> b, "Side" -> 1, 
        "Subpattern" -> ((c_) \[CenterDot] (a_)) \[CenterDot] (b_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 56}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] (a_)) \[CenterDot] (((b_) \[CenterDot] 
             (((b_) \[CenterDot] (a_)) \[CenterDot] (b_))) \[CenterDot] 
            (a_)) -> a, "MatchingSide" -> 1, "Position" -> {2, 2, 1}|>|>, 
    {"CriticalPairLemma", 31} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           a)], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 2}, 
        "Orientation" -> -1, "Rule" -> 
         ((((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] (
                a_))) \[CenterDot] (c_)) \[CenterDot] (b_)) \[CenterDot] 
           (((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
              (a_))) \[CenterDot] (b_)) -> b, "Side" -> 1, 
        "Subpattern" -> ((a_) \[CenterDot] (((a_) \[CenterDot] 
             (b_)) \[CenterDot] (a_))) \[CenterDot] (c_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 21}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (a_))) \[CenterDot] ((a_) \[CenterDot] 
            (((a_) \[CenterDot] (a_)) \[CenterDot] (a_))) -> a, 
        "MatchingSide" -> 1, "Position" -> {1, 1}|>|>, 
    {"SubstitutionLemma", 57} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 31}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {2, 1}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
            ((a \[CenterDot] a) \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 58} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 57}, 
        "Construct" -> {"SubstitutionLemma", 25}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (a \[CenterDot] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] a))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 59} -> 
     <|"Statement" -> HoldForm[
        (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
          a == (a \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] 
               a) \[CenterDot] b)) \[CenterDot] a)) \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 30}, 
        "Construct" -> {"SubstitutionLemma", 58}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            ((a_) \[CenterDot] (a_))) -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[
          (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
            a == (a \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] 
                 a) \[CenterDot] b)) \[CenterDot] a)) \[CenterDot] a], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 60} -> 
     <|"Statement" -> HoldForm[
        (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
          a == a \[CenterDot] (a \[CenterDot] ((b \[CenterDot] 
             ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 1}, 
        "Construct" -> {"SubstitutionLemma", 59}, "Position" -> {2, 2}, 
        "Rule" -> ((a_) \[CenterDot] (((b_) \[CenterDot] (((b_) \[CenterDot] 
                (a_)) \[CenterDot] (b_))) \[CenterDot] (a_))) \[CenterDot] 
           (a_) -> (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
             b)) \[CenterDot] a, "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[(b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
              b)) \[CenterDot] a == a \[CenterDot] (a \[CenterDot] 
             ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                b)) \[CenterDot] a))], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 32} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] a) == 
         ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
            ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
             a))) \[CenterDot] (a \[CenterDot] b)], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 18}, 
        "Orientation" -> -1, "Rule" -> 
         ((a_) \[CenterDot] ((b_) \[CenterDot] (((b_) \[CenterDot] (
                a_)) \[CenterDot] (b_)))) \[CenterDot] 
           ((((b_) \[CenterDot] (c_)) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] 
              (a_)))) -> b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), 
        "Side" -> 1, "Subpattern" -> (((b_) \[CenterDot] (c_)) \[CenterDot] 
           (a_)) \[CenterDot] ((a_) \[CenterDot] (((b_) \[CenterDot] 
             (c_)) \[CenterDot] (a_))), "MatchingConstruct" -> 
         {"SubstitutionLemma", 58}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] ((a_) \[CenterDot] (a_))) -> a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 33} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] b == 
         (a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
            a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
           (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
            (a \[CenterDot] b)))], "Proof" -> <|"Construct" -> {"Axiom", 1}, 
        "Orientation" -> 1, "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] 
              (c_)) \[CenterDot] (a_))) -> c, "Side" -> 1, 
        "Subpattern" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (c_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 32}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] ((a_) \[CenterDot] (b_))) \[CenterDot] 
              (a_)))) \[CenterDot] ((a_) \[CenterDot] (b_)) -> 
          a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
            a), "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"SubstitutionLemma", 61} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] b == 
         (a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
            a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] b))))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 33}, "Construct" -> 
         {"SubstitutionLemma", 25}, "Position" -> {2, 2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (a \[CenterDot] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CenterDot] b == 
           (a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
              a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] 
                b) \[CenterDot] (a \[CenterDot] b))))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 62} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] b == 
         (a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
            a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
           (a \[CenterDot] b))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 61}, "Construct" -> 
         {"SubstitutionLemma", 26}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (a_))) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CenterDot] b == 
           (a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
              a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] b))], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 34} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
          (a \[CenterDot] b) == (a \[CenterDot] b) \[CenterDot] 
          (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] b))) \[CenterDot] a))], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "Side" -> 1, "Subpattern" -> 
         ((a_) \[CenterDot] (b_)) \[CenterDot] (c_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 62}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] 
              ((a_) \[CenterDot] (b_))) \[CenterDot] (a_))) \[CenterDot] 
           (((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
             (b_))) -> a \[CenterDot] b, "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 35} -> 
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
        "MatchingConstruct" -> {"CriticalPairLemma", 34}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
            (((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] (
                (a_) \[CenterDot] (b_)))) \[CenterDot] (a_))) -> 
          (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b), 
        "MatchingSide" -> 1, "Position" -> {2, 2, 1}|>|>, 
    {"SubstitutionLemma", 63} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] b))) \[CenterDot] a) == 
         ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
          ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 35}, 
        "Construct" -> {"SubstitutionLemma", 25}, "Position" -> {2, 2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (a \[CenterDot] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CenterDot] 
            ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                b))) \[CenterDot] a) == ((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] b)) \[CenterDot] ((a \[CenterDot] 
              b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 64} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] b))) \[CenterDot] a) == 
         ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
          ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 63}, 
        "Construct" -> {"SubstitutionLemma", 26}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (a_))) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CenterDot] 
            ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                b))) \[CenterDot] a) == ((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] b)) \[CenterDot] ((a \[CenterDot] 
              b) \[CenterDot] (a \[CenterDot] b))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 65} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] b))) \[CenterDot] a) == a \[CenterDot] b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 64}, 
        "Construct" -> {"SubstitutionLemma", 46}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                b) \[CenterDot] (a \[CenterDot] b))) \[CenterDot] a) == 
           a \[CenterDot] b], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 36} -> 
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
       <|"Construct" -> {"SubstitutionLemma", 65}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] 
             (((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] (
                b_)))) \[CenterDot] (a_)) -> a \[CenterDot] b, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 5}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CenterDot] (((b_) \[CenterDot] 
             (c_)) \[CenterDot] ((((b_) \[CenterDot] (c_)) \[CenterDot] 
              ((b_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] 
                (b_)))) \[CenterDot] ((b_) \[CenterDot] (c_)))) -> 
          b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), 
        "MatchingSide" -> 1, "Position" -> {2, 1, 2, 1}|>|>, 
    {"SubstitutionLemma", 66} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((b \[CenterDot] c) \[CenterDot] (((b \[CenterDot] c) \[CenterDot] 
             (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b))) \[CenterDot] (b \[CenterDot] c))) == 
         a \[CenterDot] ((a \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] 
                a) \[CenterDot] b)) \[CenterDot] (b \[CenterDot] 
              ((b \[CenterDot] a) \[CenterDot] b)))) \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 36}, 
        "Construct" -> {"CriticalPairLemma", 5}, "Position" -> {2, 1, 2, 2}, 
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
    {"CriticalPairLemma", 37} -> 
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
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 16}, 
        "Orientation" -> -1, "Rule" -> 
         ((((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
            (((x3_) \[CenterDot] (a_)) \[CenterDot] ((((x3_) \[CenterDot] 
                (a_)) \[CenterDot] (b_)) \[CenterDot] ((x3_) \[CenterDot] (
                a_))))) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((b_) \[CenterDot] ((a_) \[CenterDot] (b_)))) -> 
          (x3 \[CenterDot] a) \[CenterDot] (((x3 \[CenterDot] a) \[CenterDot] 
             b) \[CenterDot] (x3 \[CenterDot] a)), "Side" -> 1, 
        "Subpattern" -> ((x3_) \[CenterDot] (a_)) \[CenterDot] (b_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 48}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] ((a_) \[CenterDot] (a_))) \[CenterDot] 
           ((a_) \[CenterDot] (a_)) -> a, "MatchingSide" -> 1, 
        "Position" -> {1, 2, 2, 1}|>|>, {"SubstitutionLemma", 67} -> 
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
       <|"Input" -> {"CriticalPairLemma", 37}, "Construct" -> 
         {"SubstitutionLemma", 46}, "Position" -> {1, 1, 1}, 
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
    {"SubstitutionLemma", 68} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] (a \[CenterDot] 
            a)) \[CenterDot] (((a \[CenterDot] (a \[CenterDot] 
              a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] a))) == 
         ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
              a)) \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
          (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] a))))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 67}, "Construct" -> 
         {"SubstitutionLemma", 26}, "Position" -> {1, 2, 2}, 
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
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 69} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] (a \[CenterDot] 
            a)) \[CenterDot] (((a \[CenterDot] (a \[CenterDot] 
              a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] a))) == 
         ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
          (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] a))))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 68}, "Construct" -> 
         {"SubstitutionLemma", 48}, "Position" -> {1, 2}, 
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
    {"SubstitutionLemma", 70} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] (a \[CenterDot] 
            a)) \[CenterDot] (((a \[CenterDot] (a \[CenterDot] 
              a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] a))) == 
         ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] a)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 69}, "Construct" -> 
         {"SubstitutionLemma", 58}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            ((a_) \[CenterDot] (a_))) -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[
          (a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
            (((a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
              (a \[CenterDot] a))) == ((a \[CenterDot] b) \[CenterDot] 
             a) \[CenterDot] (a \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 71} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] (a \[CenterDot] 
            a)) \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
            (a \[CenterDot] a))) == ((a \[CenterDot] b) \[CenterDot] 
           a) \[CenterDot] (a \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 70}, 
        "Construct" -> {"SubstitutionLemma", 48}, "Position" -> {2, 1}, 
        "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] (a_))) \[CenterDot] 
           ((a_) \[CenterDot] (a_)) -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          (a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] (a \[CenterDot] a))) == 
           ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 72} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] (a \[CenterDot] 
            a)) \[CenterDot] (a \[CenterDot] a) == 
         ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] a)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 71}, "Construct" -> 
         {"SubstitutionLemma", 26}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (a_))) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          (a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] a) == ((a \[CenterDot] b) \[CenterDot] 
             a) \[CenterDot] (a \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 73} -> 
     <|"Statement" -> HoldForm[a == ((a \[CenterDot] b) \[CenterDot] 
           a) \[CenterDot] (a \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 72}, 
        "Construct" -> {"SubstitutionLemma", 48}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] (a_))) \[CenterDot] 
           ((a_) \[CenterDot] (a_)) -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a == ((a \[CenterDot] b) \[CenterDot] 
             a) \[CenterDot] (a \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 38} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] 
          ((b \[CenterDot] a) \[CenterDot] b) == a \[CenterDot] 
          ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 73}, 
        "Orientation" -> -1, "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (a_)) \[CenterDot] ((a_) \[CenterDot] (a_)) -> a, "Side" -> 1, 
        "Subpattern" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] 
              (c_)) \[CenterDot] (a_))) -> c, "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 74} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((b \[CenterDot] c) \[CenterDot] (((b \[CenterDot] c) \[CenterDot] 
             (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b))) \[CenterDot] (b \[CenterDot] c))) == 
         a \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
             b)) \[CenterDot] a)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 66}, "Construct" -> 
         {"CriticalPairLemma", 38}, "Position" -> {2, 1}, 
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
    {"SubstitutionLemma", 75} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] 
          ((b \[CenterDot] a) \[CenterDot] b) == a \[CenterDot] 
          ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           a)], "Proof" -> <|"Input" -> {"SubstitutionLemma", 74}, 
        "Construct" -> {"CriticalPairLemma", 5}, "Position" -> {}, 
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
    {"SubstitutionLemma", 76} -> 
     <|"Statement" -> HoldForm[
        (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
          a == a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
             a) \[CenterDot] b))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 60}, "Construct" -> 
         {"SubstitutionLemma", 75}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (((b_) \[CenterDot] 
             (((b_) \[CenterDot] (a_)) \[CenterDot] (b_))) \[CenterDot] 
            (a_)) -> b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
            a == a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
               a) \[CenterDot] b))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 77} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          (((a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b))) \[CenterDot] a) \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 4}, 
        "Construct" -> {"SubstitutionLemma", 76}, "Position" -> {2, 1, 1}, 
        "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_))) \[CenterDot] (b_) -> b \[CenterDot] (a \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] a)), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
            (((a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                  a) \[CenterDot] b))) \[CenterDot] a) \[CenterDot] a)], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 39} -> 
     <|"Statement" -> HoldForm[((a \[CenterDot] b) \[CenterDot] 
           a) \[CenterDot] (a \[CenterDot] a) == 
         ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
          ((((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] (((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] a)))) \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] a))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 65}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] 
           (((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
              ((a_) \[CenterDot] (b_)))) \[CenterDot] (a_)) -> 
          a \[CenterDot] b, "Side" -> 1, "Subpattern" -> 
         (a_) \[CenterDot] (b_), "MatchingConstruct" -> {"SubstitutionLemma", 
          73}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (a_)) -> a, "MatchingSide" -> 1, 
        "Position" -> {2, 1, 2, 1}|>|>, {"SubstitutionLemma", 78} -> 
     <|"Statement" -> HoldForm[((a \[CenterDot] b) \[CenterDot] 
           a) \[CenterDot] (a \[CenterDot] a) == 
         ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
          ((((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            a))], "Proof" -> <|"Input" -> {"CriticalPairLemma", 39}, 
        "Construct" -> {"SubstitutionLemma", 73}, "Position" -> {2, 1, 2, 2}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (a_)) -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[((a \[CenterDot] b) \[CenterDot] 
             a) \[CenterDot] (a \[CenterDot] a) == 
           ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            ((((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               b) \[CenterDot] a))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 79} -> 
     <|"Statement" -> HoldForm[((a \[CenterDot] b) \[CenterDot] 
           a) \[CenterDot] (a \[CenterDot] a) == 
         ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 78}, 
        "Construct" -> {"SubstitutionLemma", 73}, "Position" -> {2, 1}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (a_)) -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[((a \[CenterDot] b) \[CenterDot] 
             a) \[CenterDot] (a \[CenterDot] a) == 
           ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 80} -> 
     <|"Statement" -> HoldForm[a == ((a \[CenterDot] b) \[CenterDot] 
           a) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            a))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 79}, 
        "Construct" -> {"SubstitutionLemma", 73}, "Position" -> {}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (a_)) -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a == ((a \[CenterDot] b) \[CenterDot] 
             a) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              a))], "Source" -> "norm"|>|>, {"CriticalPairLemma", 40} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] a == 
         (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
          (((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
           ((((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
              b) \[CenterDot] a)))], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 80}, "Orientation" -> -1, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_))) -> a, "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] 
          (b_), "MatchingConstruct" -> {"SubstitutionLemma", 73}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (a_)) -> a, "MatchingSide" -> 1, 
        "Position" -> {1, 1}|>|>, {"SubstitutionLemma", 81} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] a == 
         (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
          (((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
           (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 40}, 
        "Construct" -> {"SubstitutionLemma", 73}, "Position" -> {2, 2, 1}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (a_)) -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[(a \[CenterDot] b) \[CenterDot] a == 
           (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
            (((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 82} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] a == 
         (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
          a], "Proof" -> <|"Input" -> {"SubstitutionLemma", 81}, 
        "Construct" -> {"SubstitutionLemma", 80}, "Position" -> {2}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_))) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[(a \[CenterDot] b) \[CenterDot] a == 
           (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
            a], "Source" -> "norm"|>|>, {"CriticalPairLemma", 41} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
           (((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
               a)) \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] a)))) == 
         ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
           (((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              a)))) \[CenterDot] a], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 76}, "Orientation" -> 1, 
        "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_))) \[CenterDot] (b_) -> b \[CenterDot] (a \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] a)), "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 82}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] 
              (b_)) \[CenterDot] (a_))) \[CenterDot] (a_) -> 
          (a \[CenterDot] b) \[CenterDot] a, "MatchingSide" -> 1, 
        "Position" -> {1, 2, 1}|>|>, {"SubstitutionLemma", 83} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
           (((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
               a)) \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] a)))) == 
         ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
           a) \[CenterDot] a], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 41}, "Construct" -> 
         {"SubstitutionLemma", 80}, "Position" -> {1, 2}, 
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
    {"SubstitutionLemma", 84} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
           (((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
               a)) \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] a)))) == 
         ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 83}, 
        "Construct" -> {"SubstitutionLemma", 82}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_))) \[CenterDot] (a_) -> (a \[CenterDot] b) \[CenterDot] a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
               a)) \[CenterDot] (((a \[CenterDot] ((a \[CenterDot] 
                  b) \[CenterDot] a)) \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)))) == 
           ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] a], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 85} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
           (((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)))) == 
         ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 84}, 
        "Construct" -> {"SubstitutionLemma", 82}, "Position" -> {2, 2, 1}, 
        "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_))) \[CenterDot] (a_) -> (a \[CenterDot] b) \[CenterDot] a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
               a)) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
               a) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                 b) \[CenterDot] a)))) == ((a \[CenterDot] b) \[CenterDot] 
             a) \[CenterDot] a], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 86} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
           a) == ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 85}, 
        "Construct" -> {"SubstitutionLemma", 80}, "Position" -> {2, 2}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_))) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                b) \[CenterDot] a)) \[CenterDot] a) == 
           ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] a], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 87} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] b) \[CenterDot] a) == 
         ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 86}, 
        "Construct" -> {"SubstitutionLemma", 82}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_))) \[CenterDot] (a_) -> (a \[CenterDot] b) \[CenterDot] a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a) == 
           ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] a], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 42} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (
                a \[CenterDot] b))) \[CenterDot] a)) \[CenterDot] a) == 
         ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] a], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 87}, 
        "Orientation" -> -1, "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (a_)) \[CenterDot] (a_) -> a \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] a), "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 65}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CenterDot] (((a_) \[CenterDot] 
             (((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] (
                b_)))) \[CenterDot] (a_)) -> a \[CenterDot] b, 
        "MatchingSide" -> 1, "Position" -> {1, 1}|>|>, 
    {"SubstitutionLemma", 88} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (
                a \[CenterDot] b))) \[CenterDot] a)) \[CenterDot] a) == 
         a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 42}, 
        "Construct" -> {"SubstitutionLemma", 87}, "Position" -> {}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
           (a_) -> a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                  b) \[CenterDot] (a \[CenterDot] b))) \[CenterDot] 
               a)) \[CenterDot] a) == a \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 89} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] b))) \[CenterDot] a) == a \[CenterDot] 
          ((a \[CenterDot] b) \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 88}, 
        "Construct" -> {"SubstitutionLemma", 82}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_))) \[CenterDot] (a_) -> (a \[CenterDot] b) \[CenterDot] a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (
                a \[CenterDot] b))) \[CenterDot] a) == a \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 90} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] b == a \[CenterDot] 
          ((a \[CenterDot] b) \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 89}, 
        "Construct" -> {"SubstitutionLemma", 65}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] 
             (((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] (
                b_)))) \[CenterDot] (a_)) -> a \[CenterDot] b, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[a \[CenterDot] b == 
           a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 91} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          (((a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] a) \[CenterDot] 
           a)], "Proof" -> <|"Input" -> {"SubstitutionLemma", 77}, 
        "Construct" -> {"SubstitutionLemma", 90}, "Position" -> {2, 1, 1, 2}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
            (((a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
              a) \[CenterDot] a)], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 92} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] b == 
         ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 87}, 
        "Construct" -> {"SubstitutionLemma", 90}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CenterDot] b == 
           ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] a], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 93} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] (b \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 91}, 
        "Construct" -> {"SubstitutionLemma", 92}, "Position" -> {2}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
           (a_) -> a \[CenterDot] b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] (b \[CenterDot] a))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 94} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] 
          ((b \[CenterDot] a) \[CenterDot] b) == a \[CenterDot] 
          (a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
             b)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 75}, 
        "Construct" -> {"SubstitutionLemma", 76}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_))) \[CenterDot] (b_) -> b \[CenterDot] (a \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] a)), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[b \[CenterDot] 
            ((b \[CenterDot] a) \[CenterDot] b) == a \[CenterDot] 
            (a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 95} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] 
          ((b \[CenterDot] a) \[CenterDot] b) == a \[CenterDot] 
          (a \[CenterDot] (b \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 94}, 
        "Construct" -> {"SubstitutionLemma", 90}, "Position" -> {2, 2}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[b \[CenterDot] 
            ((b \[CenterDot] a) \[CenterDot] b) == a \[CenterDot] 
            (a \[CenterDot] (b \[CenterDot] a))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 96} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] a == a \[CenterDot] 
          (a \[CenterDot] (b \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 95}, 
        "Construct" -> {"SubstitutionLemma", 90}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[b \[CenterDot] a == 
           a \[CenterDot] (a \[CenterDot] (b \[CenterDot] a))], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 43} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] a == 
         a \[CenterDot] (a \[CenterDot] b)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 96}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
            ((b_) \[CenterDot] (a_))) -> b \[CenterDot] a, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] ((b_) \[CenterDot] (a_)), 
        "MatchingConstruct" -> {"SubstitutionLemma", 90}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) -> 
          a \[CenterDot] b, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 44} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] (a \[CenterDot] b)))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 93}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] ((b_) \[CenterDot] (a_))) -> a, "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] (a_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 43}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (a \[CenterDot] b), "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 45} -> 
     <|"Statement" -> HoldForm[
        ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           (((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b)) \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
             ((b \[CenterDot] a) \[CenterDot] b)))) \[CenterDot] a == 
         a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
             (((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                 b)) \[CenterDot] a) \[CenterDot] a)) \[CenterDot] a))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 1}, 
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
         2}|>|>, {"SubstitutionLemma", 97} -> 
     <|"Statement" -> HoldForm[
        ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           a) \[CenterDot] a == a \[CenterDot] (a \[CenterDot] 
           ((a \[CenterDot] (((b \[CenterDot] ((b \[CenterDot] 
                  a) \[CenterDot] b)) \[CenterDot] a) \[CenterDot] 
              a)) \[CenterDot] a))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 45}, "Construct" -> {"Axiom", 1}, 
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
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 46} -> 
     <|"Statement" -> HoldForm[
        ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           a) \[CenterDot] a == (a \[CenterDot] 
           (((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b)) \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
            (a \[CenterDot] a)))], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 3}, "Orientation" -> -1, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((c_) \[CenterDot] (a_)) \[CenterDot] 
            ((((c_) \[CenterDot] (a_)) \[CenterDot] (b_)) \[CenterDot] 
             ((c_) \[CenterDot] (a_)))) -> b, "Side" -> 1, 
        "Subpattern" -> ((c_) \[CenterDot] (a_)) \[CenterDot] (b_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 3}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] (a_)) \[CenterDot] 
           ((((b_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] (
                b_))) \[CenterDot] (a_)) \[CenterDot] (a_)) -> a, 
        "MatchingSide" -> 1, "Position" -> {2, 2, 1}|>|>, 
    {"SubstitutionLemma", 98} -> 
     <|"Statement" -> HoldForm[
        ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           a) \[CenterDot] a == (a \[CenterDot] 
           (((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b)) \[CenterDot] a) \[CenterDot] a)) \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 46}, 
        "Construct" -> {"SubstitutionLemma", 58}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            ((a_) \[CenterDot] (a_))) -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[
          ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
             a) \[CenterDot] a == (a \[CenterDot] (((b \[CenterDot] 
                ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
               a) \[CenterDot] a)) \[CenterDot] a], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 99} -> 
     <|"Statement" -> HoldForm[
        ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           a) \[CenterDot] a == a \[CenterDot] (a \[CenterDot] 
           (((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b)) \[CenterDot] a) \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 97}, 
        "Construct" -> {"SubstitutionLemma", 98}, "Position" -> {2, 2}, 
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
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 100} -> 
     <|"Statement" -> HoldForm[
        ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           a) \[CenterDot] a == a \[CenterDot] (a \[CenterDot] 
           ((a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b))) \[CenterDot] a))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 99}, "Construct" -> 
         {"SubstitutionLemma", 76}, "Position" -> {2, 2, 1}, 
        "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_))) \[CenterDot] (b_) -> b \[CenterDot] (a \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] a)), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[
          ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
             a) \[CenterDot] a == a \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                  a) \[CenterDot] b))) \[CenterDot] a))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 101} -> 
     <|"Statement" -> HoldForm[
        ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           a) \[CenterDot] a == b \[CenterDot] 
          ((b \[CenterDot] a) \[CenterDot] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 100}, 
        "Construct" -> {"CriticalPairLemma", 8}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
            (((a_) \[CenterDot] ((b_) \[CenterDot] (((b_) \[CenterDot] 
                 (a_)) \[CenterDot] (b_)))) \[CenterDot] (a_))) -> 
          b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
             a) \[CenterDot] a == b \[CenterDot] ((b \[CenterDot] 
              a) \[CenterDot] b)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 102} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] (b \[CenterDot] 
            ((b \[CenterDot] a) \[CenterDot] b))) \[CenterDot] a == 
         b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 101}, 
        "Construct" -> {"SubstitutionLemma", 76}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_))) \[CenterDot] (b_) -> b \[CenterDot] (a \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] a)), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          (a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b))) \[CenterDot] a == b \[CenterDot] 
            ((b \[CenterDot] a) \[CenterDot] b)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 103} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] (b \[CenterDot] 
            a)) \[CenterDot] a == b \[CenterDot] 
          ((b \[CenterDot] a) \[CenterDot] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 102}, 
        "Construct" -> {"SubstitutionLemma", 90}, "Position" -> {1, 2}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          (a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] a == 
           b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 104} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] (b \[CenterDot] 
            a)) \[CenterDot] a == b \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 103}, 
        "Construct" -> {"SubstitutionLemma", 90}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[
          (a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] a == 
           b \[CenterDot] a], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 47} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] (a \[CenterDot] b) == 
         (a \[CenterDot] b) \[CenterDot] b], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 92}, 
        "Orientation" -> -1, "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (a_)) \[CenterDot] (a_) -> a \[CenterDot] b, "Side" -> 1, 
        "Subpattern" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 104}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((a_) \[CenterDot] ((b_) \[CenterDot] (a_))) \[CenterDot] (a_) -> 
          b \[CenterDot] a, "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"SubstitutionLemma", 105} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] b == a \[CenterDot] 
          ((a \[CenterDot] b) \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 92}, 
        "Construct" -> {"CriticalPairLemma", 47}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (b_) -> 
          b \[CenterDot] (a \[CenterDot] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CenterDot] b == 
           a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 106} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] b == a \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] b))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 105}, 
        "Construct" -> {"CriticalPairLemma", 43}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CenterDot] b == 
           a \[CenterDot] (a \[CenterDot] (a \[CenterDot] b))], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 107} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] b)], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 44}, "Construct" -> 
         {"SubstitutionLemma", 106}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (b_))) -> a \[CenterDot] b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] b)], "Source" -> "norm"|>|>, 
    {"Conclusion", 1} -> <|"Statement" -> HoldForm[a == a], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Construct" -> 
         {"SubstitutionLemma", 107}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            (b_)) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[a == a], "Source" -> "cpl"|>|>}|>]
