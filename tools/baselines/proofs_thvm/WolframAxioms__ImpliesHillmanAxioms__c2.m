ProofObject["EquationalLogic", Inactive[Equal][
  a \[CenterDot] (a \[CenterDot] b), a \[CenterDot] (b \[CenterDot] b)], 
 {Inactive[Equal][(((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
    ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] (a_))), c_]}, 
 <|"Variables" -> {a, b, c, x3, x4}, "Constants" -> {}, 
  "Proof" -> {{"Axiom", 1} -> <|"Statement" -> 
       HoldForm[((a \[CenterDot] b) \[CenterDot] c) \[CenterDot] 
          (a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] a)) == c], 
      "Proof" -> <||>|>, {"Hypothesis", 1} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] b) == 
         a \[CenterDot] (b \[CenterDot] b)], "Proof" -> <||>|>, 
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
        "Position" -> {1}|>|>, {"CriticalPairLemma", 7} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] c) \[CenterDot] 
          (((b \[CenterDot] c) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             (((a \[CenterDot] b) \[CenterDot] c) \[CenterDot] 
              (a \[CenterDot] b)))) \[CenterDot] (b \[CenterDot] c)) == 
         ((a \[CenterDot] b) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
             c) \[CenterDot] (a \[CenterDot] b))) \[CenterDot] 
          (c \[CenterDot] ((c \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
              (c \[CenterDot] (b \[CenterDot] c)))) \[CenterDot] c))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 6}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            (((b_) \[CenterDot] (((c_) \[CenterDot] (b_)) \[CenterDot] (
                (((c_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
                ((c_) \[CenterDot] (b_))))) \[CenterDot] (b_))) -> 
          (c \[CenterDot] b) \[CenterDot] (((c \[CenterDot] b) \[CenterDot] 
             a) \[CenterDot] (c \[CenterDot] b)), "Side" -> 1, 
        "Subpattern" -> ((c_) \[CenterDot] (b_)) \[CenterDot] (a_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 4}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] (b_)) \[CenterDot] (((c_) \[CenterDot] 
             (a_)) \[CenterDot] ((((c_) \[CenterDot] (a_)) \[CenterDot] 
              (b_)) \[CenterDot] ((c_) \[CenterDot] (a_)))) -> b, 
        "MatchingSide" -> 1, "Position" -> {2, 2, 1, 2, 2, 1}|>|>, 
    {"SubstitutionLemma", 5} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] c) \[CenterDot] 
          (c \[CenterDot] (b \[CenterDot] c)) == 
         ((a \[CenterDot] b) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
             c) \[CenterDot] (a \[CenterDot] b))) \[CenterDot] 
          (c \[CenterDot] ((c \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
              (c \[CenterDot] (b \[CenterDot] c)))) \[CenterDot] c))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 7}, 
        "Construct" -> {"SubstitutionLemma", 4}, "Position" -> {2, 1}, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((c_) \[CenterDot] (a_)) \[CenterDot] 
            ((((c_) \[CenterDot] (a_)) \[CenterDot] (b_)) \[CenterDot] 
             ((c_) \[CenterDot] (a_)))) -> b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(b \[CenterDot] c) \[CenterDot] 
            (c \[CenterDot] (b \[CenterDot] c)) == 
           ((a \[CenterDot] b) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
               c) \[CenterDot] (a \[CenterDot] b))) \[CenterDot] 
            (c \[CenterDot] ((c \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
                (c \[CenterDot] (b \[CenterDot] c)))) \[CenterDot] c))], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 8} -> 
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
    {"CriticalPairLemma", 9} -> 
     <|"Statement" -> HoldForm[
        c == ((a \[CenterDot] ((a \[CenterDot] (b \[CenterDot] (
                (b \[CenterDot] c) \[CenterDot] b))) \[CenterDot] 
             a)) \[CenterDot] c) \[CenterDot] 
          ((b \[CenterDot] ((b \[CenterDot] c) \[CenterDot] b)) \[CenterDot] 
           c)], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 8}, 
        "Orientation" -> -1, "Rule" -> 
         ((((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] (
                a_))) \[CenterDot] (c_)) \[CenterDot] (b_)) \[CenterDot] 
           (((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
              (a_))) \[CenterDot] (b_)) -> b, "Side" -> 1, 
        "Subpattern" -> ((a_) \[CenterDot] (((a_) \[CenterDot] 
             (b_)) \[CenterDot] (a_))) \[CenterDot] (c_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 1}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] 
            ((((b_) \[CenterDot] (c_)) \[CenterDot] ((b_) \[CenterDot] (
                ((b_) \[CenterDot] (a_)) \[CenterDot] (b_)))) \[CenterDot] 
             ((b_) \[CenterDot] (c_)))) -> b \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] b), "MatchingSide" -> 1, 
        "Position" -> {1, 1}|>|>, {"CriticalPairLemma", 10} -> 
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
        "Position" -> {2, 2, 1}|>|>, {"CriticalPairLemma", 11} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] a) == 
         (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             a))) \[CenterDot] a], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 10}, "Orientation" -> -1, 
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
    {"CriticalPairLemma", 12} -> 
     <|"Statement" -> HoldForm[
        a == ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] a))) \[CenterDot] a) \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           a)], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 9}, 
        "Orientation" -> -1, "Rule" -> 
         (((a_) \[CenterDot] (((a_) \[CenterDot] ((b_) \[CenterDot] 
                (((b_) \[CenterDot] (c_)) \[CenterDot] (b_)))) \[CenterDot] 
              (a_))) \[CenterDot] (c_)) \[CenterDot] 
           (((b_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] 
              (b_))) \[CenterDot] (c_)) -> c, "Side" -> 1, 
        "Subpattern" -> ((a_) \[CenterDot] ((b_) \[CenterDot] 
            (((b_) \[CenterDot] (c_)) \[CenterDot] (b_)))) \[CenterDot] (a_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 11}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] (
                a_)) \[CenterDot] (a_)))) \[CenterDot] (a_) -> 
          a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
        "MatchingSide" -> 1, "Position" -> {1, 1, 2}|>|>, 
    {"SubstitutionLemma", 6} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           a)], "Proof" -> <|"Input" -> {"CriticalPairLemma", 12}, 
        "Construct" -> {"CriticalPairLemma", 11}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) \[CenterDot] 
           (a_) -> a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a == (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a)) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] a)) \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 13} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a))], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "Side" -> 1, "Subpattern" -> 
         ((a_) \[CenterDot] (b_)) \[CenterDot] (c_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 11}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) \[CenterDot] 
           (a_) -> a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 14} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] a) == a \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a))))], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 1}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] 
           (((b_) \[CenterDot] (c_)) \[CenterDot] 
            ((((b_) \[CenterDot] (c_)) \[CenterDot] ((b_) \[CenterDot] (
                ((b_) \[CenterDot] (a_)) \[CenterDot] (b_)))) \[CenterDot] 
             ((b_) \[CenterDot] (c_)))) -> b \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] b), "Side" -> 1, 
        "Subpattern" -> ((b_) \[CenterDot] (c_)) \[CenterDot] 
          ((b_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] (b_))), 
        "MatchingConstruct" -> {"CriticalPairLemma", 13}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (a_))) \[CenterDot] ((a_) \[CenterDot] 
            (((a_) \[CenterDot] (a_)) \[CenterDot] (a_))) -> a, 
        "MatchingSide" -> 1, "Position" -> {2, 2, 1}|>|>, 
    {"CriticalPairLemma", 15} -> 
     <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           ((((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                  a) \[CenterDot] a))) \[CenterDot] a) \[CenterDot] 
             b) \[CenterDot] ((a \[CenterDot] (a \[CenterDot] (
                (a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] a)))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 4}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((c_) \[CenterDot] (a_)) \[CenterDot] 
            ((((c_) \[CenterDot] (a_)) \[CenterDot] (b_)) \[CenterDot] 
             ((c_) \[CenterDot] (a_)))) -> b, "Side" -> 1, 
        "Subpattern" -> (c_) \[CenterDot] (a_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 11}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) \[CenterDot] 
           (a_) -> a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
        "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"SubstitutionLemma", 7} -> 
     <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a)) \[CenterDot] b) \[CenterDot] ((a \[CenterDot] 
              (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a))) \[CenterDot] a)))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 15}, "Construct" -> 
         {"CriticalPairLemma", 11}, "Position" -> {2, 2, 1, 1}, 
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
    {"SubstitutionLemma", 8} -> 
     <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a)) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] a))))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 7}, 
        "Construct" -> {"CriticalPairLemma", 11}, "Position" -> {2, 2, 2}, 
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
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 16} -> 
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
        "MatchingConstruct" -> {"SubstitutionLemma", 8}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] (b_)) \[CenterDot] (((a_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (a_))) \[CenterDot] 
            ((((a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
                (a_))) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
              (((a_) \[CenterDot] (a_)) \[CenterDot] (a_))))) -> b, 
        "MatchingSide" -> 1, "Position" -> {2, 2, 1}|>|>, 
    {"SubstitutionLemma", 9} -> 
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
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 16}, 
        "Construct" -> {"CriticalPairLemma", 11}, "Position" -> {1}, 
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
    {"SubstitutionLemma", 10} -> 
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
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 9}, 
        "Construct" -> {"CriticalPairLemma", 11}, "Position" -> {2, 1, 2}, 
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
    {"SubstitutionLemma", 11} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a))) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
            ((a \[CenterDot] a) \[CenterDot] a))) == 
         (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 10}, 
        "Construct" -> {"CriticalPairLemma", 10}, "Position" -> {2}, 
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
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 12} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a))) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
            ((a \[CenterDot] a) \[CenterDot] a))) == a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 11}, 
        "Construct" -> {"CriticalPairLemma", 13}, "Position" -> {}, 
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
    {"SubstitutionLemma", 13} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a))) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
              a) \[CenterDot] a))) == a], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 12}, "Construct" -> 
         {"CriticalPairLemma", 11}, "Position" -> {2, 1, 2}, 
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
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 14} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             a))) == a], "Proof" -> <|"Input" -> {"SubstitutionLemma", 13}, 
        "Construct" -> {"CriticalPairLemma", 13}, "Position" -> {2, 1}, 
        "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (a_))) \[CenterDot] ((a_) \[CenterDot] 
            (((a_) \[CenterDot] (a_)) \[CenterDot] (a_))) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a))) == a], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 15} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] a) == a \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 14}, 
        "Construct" -> {"SubstitutionLemma", 14}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (a_))) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) -> a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a) == 
           a \[CenterDot] a], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 16} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           a)], "Proof" -> <|"Input" -> {"SubstitutionLemma", 6}, 
        "Construct" -> {"SubstitutionLemma", 15}, "Position" -> {1}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
            ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a)) \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 17} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 16}, 
        "Construct" -> {"SubstitutionLemma", 15}, "Position" -> {2, 1}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] a, "Orientation" -> 1, 
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
     <|"Statement" -> HoldForm[
        (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
          a == a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
             ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                b)) \[CenterDot] a)) \[CenterDot] a))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 17}, 
        "Orientation" -> -1, "Rule" -> 
         ((((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] (
                a_))) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((b_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] 
             (b_))) -> c, "Side" -> 1, "Subpattern" -> 
         (((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_))) \[CenterDot] (b_)) \[CenterDot] (c_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 8}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] (
                a_))) \[CenterDot] (c_)) \[CenterDot] (b_)) \[CenterDot] 
           (((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
              (a_))) \[CenterDot] (b_)) -> b, "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 19} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          a == a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] a))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 18}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
            (((a_) \[CenterDot] (((b_) \[CenterDot] (((b_) \[CenterDot] 
                  (a_)) \[CenterDot] (b_))) \[CenterDot] (a_))) \[CenterDot] 
             (a_))) -> (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
             b)) \[CenterDot] a, "Side" -> 1, "Subpattern" -> 
         (b_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] (b_)), 
        "MatchingConstruct" -> {"SubstitutionLemma", 15}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)) -> 
          a \[CenterDot] a, "MatchingSide" -> 1, "Position" -> {2, 2, 1, 2, 
         1}|>|>, {"SubstitutionLemma", 18} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          a == a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
             a) \[CenterDot] a))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 19}, "Construct" -> 
         {"SubstitutionLemma", 15}, "Position" -> {2, 2, 1}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            a == a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] a))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 19} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          a == a \[CenterDot] (a \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 18}, 
        "Construct" -> {"SubstitutionLemma", 15}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            a == a \[CenterDot] (a \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 20} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] a == 
         a \[CenterDot] (a \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 19}, 
        "Construct" -> {"SubstitutionLemma", 15}, "Position" -> {1}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(a \[CenterDot] a) \[CenterDot] a == 
           a \[CenterDot] (a \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 21} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 17}, 
        "Construct" -> {"SubstitutionLemma", 20}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (a \[CenterDot] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] a))], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 20} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] b) \[CenterDot] 
          (b \[CenterDot] (b \[CenterDot] b)) == 
         ((a \[CenterDot] b) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
             b) \[CenterDot] (a \[CenterDot] b))) \[CenterDot] 
          (b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] b))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 5}, 
        "Orientation" -> -1, "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
             ((a_) \[CenterDot] (b_)))) \[CenterDot] ((c_) \[CenterDot] 
            (((c_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] (
                (c_) \[CenterDot] ((b_) \[CenterDot] (c_))))) \[CenterDot] 
             (c_))) -> (b \[CenterDot] c) \[CenterDot] (c \[CenterDot] 
            (b \[CenterDot] c)), "Side" -> 1, "Subpattern" -> 
         ((b_) \[CenterDot] (c_)) \[CenterDot] ((c_) \[CenterDot] 
           ((b_) \[CenterDot] (c_))), "MatchingConstruct" -> 
         {"SubstitutionLemma", 21}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] ((a_) \[CenterDot] (a_))) -> a, 
        "MatchingSide" -> 1, "Position" -> {2, 2, 1, 2}|>|>, 
    {"SubstitutionLemma", 22} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] b) \[CenterDot] 
          (b \[CenterDot] (b \[CenterDot] b)) == 
         ((a \[CenterDot] b) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
             b) \[CenterDot] (a \[CenterDot] b))) \[CenterDot] 
          (b \[CenterDot] (b \[CenterDot] (b \[CenterDot] b)))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 20}, 
        "Construct" -> {"SubstitutionLemma", 20}, "Position" -> {2, 2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (a \[CenterDot] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[(b \[CenterDot] b) \[CenterDot] 
            (b \[CenterDot] (b \[CenterDot] b)) == 
           ((a \[CenterDot] b) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
               b) \[CenterDot] (a \[CenterDot] b))) \[CenterDot] 
            (b \[CenterDot] (b \[CenterDot] (b \[CenterDot] b)))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 23} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
           (a \[CenterDot] a)) == a \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 15}, 
        "Construct" -> {"SubstitutionLemma", 20}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (a \[CenterDot] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
             (a \[CenterDot] a)) == a \[CenterDot] a], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 24} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] b) \[CenterDot] 
          (b \[CenterDot] (b \[CenterDot] b)) == 
         ((a \[CenterDot] b) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
             b) \[CenterDot] (a \[CenterDot] b))) \[CenterDot] 
          (b \[CenterDot] b)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 22}, "Construct" -> 
         {"SubstitutionLemma", 23}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (a_))) -> a \[CenterDot] a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[(b \[CenterDot] b) \[CenterDot] 
            (b \[CenterDot] (b \[CenterDot] b)) == 
           ((a \[CenterDot] b) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
               b) \[CenterDot] (a \[CenterDot] b))) \[CenterDot] 
            (b \[CenterDot] b)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 25} -> 
     <|"Statement" -> HoldForm[b == ((a \[CenterDot] b) \[CenterDot] 
           (((a \[CenterDot] b) \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b))) \[CenterDot] (b \[CenterDot] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 24}, 
        "Construct" -> {"SubstitutionLemma", 21}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            ((a_) \[CenterDot] (a_))) -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[b == ((a \[CenterDot] b) \[CenterDot] 
             (((a \[CenterDot] b) \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] b))) \[CenterDot] (b \[CenterDot] b)], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 21} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] a == a \[CenterDot] 
          ((b \[CenterDot] a) \[CenterDot] (((b \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] a)) \[CenterDot] (b \[CenterDot] a)))], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "Side" -> 1, "Subpattern" -> 
         ((a_) \[CenterDot] (b_)) \[CenterDot] (c_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 25}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((((a_) \[CenterDot] (b_)) \[CenterDot] (b_)) \[CenterDot] 
             ((a_) \[CenterDot] (b_)))) \[CenterDot] ((b_) \[CenterDot] 
            (b_)) -> b, "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 22} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
          (((b \[CenterDot] a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
           (b \[CenterDot] a)) == (a \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 6}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            (((b_) \[CenterDot] (((c_) \[CenterDot] (b_)) \[CenterDot] (
                (((c_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
                ((c_) \[CenterDot] (b_))))) \[CenterDot] (b_))) -> 
          (c \[CenterDot] b) \[CenterDot] (((c \[CenterDot] b) \[CenterDot] 
             a) \[CenterDot] (c \[CenterDot] b)), "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] (((c_) \[CenterDot] 
            (b_)) \[CenterDot] ((((c_) \[CenterDot] (b_)) \[CenterDot] 
             (a_)) \[CenterDot] ((c_) \[CenterDot] (b_)))), 
        "MatchingConstruct" -> {"CriticalPairLemma", 21}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] 
            ((((b_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] (
                a_))) \[CenterDot] ((b_) \[CenterDot] (a_)))) -> 
          a \[CenterDot] a, "MatchingSide" -> 1, "Position" -> {2, 2, 1}|>|>, 
    {"SubstitutionLemma", 26} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
          (((b \[CenterDot] a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
           (b \[CenterDot] a)) == (a \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] (a \[CenterDot] a)))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 22}, 
        "Construct" -> {"SubstitutionLemma", 20}, "Position" -> {2, 2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (a \[CenterDot] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
            (((b \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
               a)) \[CenterDot] (b \[CenterDot] a)) == 
           (a \[CenterDot] a) \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
              (a \[CenterDot] a)))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 27} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
          (((b \[CenterDot] a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
           (b \[CenterDot] a)) == (a \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] a)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 26}, "Construct" -> 
         {"SubstitutionLemma", 23}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (a_))) -> a \[CenterDot] a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
            (((b \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
               a)) \[CenterDot] (b \[CenterDot] a)) == 
           (a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a)], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 28} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 13}, 
        "Construct" -> {"SubstitutionLemma", 15}, "Position" -> {1}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 29} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] a)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 28}, "Construct" -> 
         {"SubstitutionLemma", 15}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 30} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
          (((b \[CenterDot] a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
           (b \[CenterDot] a)) == a], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 27}, "Construct" -> 
         {"SubstitutionLemma", 29}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[(b \[CenterDot] a) \[CenterDot] 
            (((b \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
               a)) \[CenterDot] (b \[CenterDot] a)) == a], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 23} -> 
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
         {"SubstitutionLemma", 4}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((c_) \[CenterDot] (a_)) \[CenterDot] 
            ((((c_) \[CenterDot] (a_)) \[CenterDot] (b_)) \[CenterDot] 
             ((c_) \[CenterDot] (a_)))) -> b, "MatchingSide" -> 1, 
        "Position" -> {2, 2, 1}|>|>, {"CriticalPairLemma", 24} -> 
     <|"Statement" -> HoldForm[((a \[CenterDot] c) \[CenterDot] 
           b) \[CenterDot] ((((a \[CenterDot] c) \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              a))) \[CenterDot] ((a \[CenterDot] c) \[CenterDot] b)) == 
         (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
          (b \[CenterDot] ((b \[CenterDot] (((a \[CenterDot] c) \[CenterDot] 
               b) \[CenterDot] (b \[CenterDot] ((a \[CenterDot] 
                 c) \[CenterDot] b)))) \[CenterDot] b))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 6}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            (((b_) \[CenterDot] (((c_) \[CenterDot] (b_)) \[CenterDot] (
                (((c_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
                ((c_) \[CenterDot] (b_))))) \[CenterDot] (b_))) -> 
          (c \[CenterDot] b) \[CenterDot] (((c \[CenterDot] b) \[CenterDot] 
             a) \[CenterDot] (c \[CenterDot] b)), "Side" -> 1, 
        "Subpattern" -> ((c_) \[CenterDot] (b_)) \[CenterDot] (a_), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] 
              (c_)) \[CenterDot] (a_))) -> c, "MatchingSide" -> 1, 
        "Position" -> {2, 2, 1, 2, 2, 1}|>|>, {"SubstitutionLemma", 31} -> 
     <|"Statement" -> HoldForm[((a \[CenterDot] c) \[CenterDot] 
           b) \[CenterDot] (b \[CenterDot] ((a \[CenterDot] c) \[CenterDot] 
            b)) == (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            a)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
             (((a \[CenterDot] c) \[CenterDot] b) \[CenterDot] 
              (b \[CenterDot] ((a \[CenterDot] c) \[CenterDot] 
                b)))) \[CenterDot] b))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 24}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {2, 1}, "Rule" -> 
         (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[((a \[CenterDot] c) \[CenterDot] b) \[CenterDot] 
            (b \[CenterDot] ((a \[CenterDot] c) \[CenterDot] b)) == 
           (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
            (b \[CenterDot] ((b \[CenterDot] (((a \[CenterDot] 
                  c) \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
                 ((a \[CenterDot] c) \[CenterDot] b)))) \[CenterDot] b))], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 25} -> 
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
        "Position" -> {2, 2, 1}|>|>, {"CriticalPairLemma", 26} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] a) == a \[CenterDot] 
          (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            a) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] a)) \[CenterDot] a)))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 25}, 
        "Orientation" -> -1, "Rule" -> 
         (((((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
             (x3_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] (
                c_)) \[CenterDot] (a_)))) \[CenterDot] 
           ((((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
            ((c_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
              (c_)))) -> a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] a), 
        "Side" -> 1, "Subpattern" -> ((((a_) \[CenterDot] (b_)) \[CenterDot] 
            (c_)) \[CenterDot] (x3_)) \[CenterDot] ((a_) \[CenterDot] 
           (((a_) \[CenterDot] (c_)) \[CenterDot] (a_))), 
        "MatchingConstruct" -> {"CriticalPairLemma", 17}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] (
                a_))) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((b_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] 
             (b_))) -> c, "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 27} -> 
     <|"Statement" -> HoldForm[
        ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           a) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] a)) == 
         (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a)) \[CenterDot] a))], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 31}, "Orientation" -> -1, 
        "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_))) \[CenterDot] ((b_) \[CenterDot] 
            (((b_) \[CenterDot] ((((a_) \[CenterDot] (c_)) \[CenterDot] 
                (b_)) \[CenterDot] ((b_) \[CenterDot] (((a_) \[CenterDot] 
                  (c_)) \[CenterDot] (b_))))) \[CenterDot] (b_))) -> 
          ((a \[CenterDot] c) \[CenterDot] b) \[CenterDot] 
           (b \[CenterDot] ((a \[CenterDot] c) \[CenterDot] b)), "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] 
          ((((a_) \[CenterDot] (c_)) \[CenterDot] (b_)) \[CenterDot] 
           ((b_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] (b_)))), 
        "MatchingConstruct" -> {"CriticalPairLemma", 26}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CenterDot] ((((a_) \[CenterDot] (((a_) \[CenterDot] 
                (a_)) \[CenterDot] (a_))) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] (((a_) \[CenterDot] (((a_) \[CenterDot] 
                 (a_)) \[CenterDot] (a_))) \[CenterDot] (a_)))) -> 
          a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
        "MatchingSide" -> 1, "Position" -> {2, 2, 1}|>|>, 
    {"CriticalPairLemma", 28} -> 
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
        "MatchingConstruct" -> {"CriticalPairLemma", 1}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] 
            ((((b_) \[CenterDot] (c_)) \[CenterDot] ((b_) \[CenterDot] (
                ((b_) \[CenterDot] (a_)) \[CenterDot] (b_)))) \[CenterDot] 
             ((b_) \[CenterDot] (c_)))) -> b \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] b), "MatchingSide" -> 1, 
        "Position" -> {2, 2, 1}|>|>, {"SubstitutionLemma", 32} -> 
     <|"Statement" -> HoldForm[
        ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           a) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] a)) == 
         (a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a))) \[CenterDot] (a \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 27}, 
        "Construct" -> {"CriticalPairLemma", 28}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_))) \[CenterDot] ((b_) \[CenterDot] 
            (((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] (
                a_))) \[CenterDot] (b_))) -> (a \[CenterDot] b) \[CenterDot] 
           (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                b) \[CenterDot] a))) \[CenterDot] (a \[CenterDot] b)), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
             a) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] (
                (a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] a)) == 
           (a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a))) \[CenterDot] (a \[CenterDot] a))], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 29} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] a) == 
         (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             a))) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
              a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
            ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a))) \[CenterDot] a)))], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 10}, "Orientation" -> -1, 
        "Rule" -> ((a_) \[CenterDot] ((b_) \[CenterDot] 
             (((b_) \[CenterDot] (a_)) \[CenterDot] (b_)))) \[CenterDot] 
           ((((b_) \[CenterDot] (c_)) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] 
              (a_)))) -> b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), 
        "Side" -> 1, "Subpattern" -> ((b_) \[CenterDot] (c_)) \[CenterDot] 
          (a_), "MatchingConstruct" -> {"CriticalPairLemma", 11}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] (
                a_)) \[CenterDot] (a_)))) \[CenterDot] (a_) -> 
          a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
        "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"SubstitutionLemma", 33} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] a) == 
         (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             a))) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
              a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a))))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 29}, 
        "Construct" -> {"CriticalPairLemma", 11}, "Position" -> {2, 2, 2}, 
        "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) \[CenterDot] 
           (a_) -> a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a) == 
           (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a))) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
              (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a))))], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 30} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             a))) == (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
            a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
             ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a)) \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
                ((a \[CenterDot] a) \[CenterDot] a))))) \[CenterDot] a))], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
             (a_))) -> c, "Side" -> 1, "Subpattern" -> 
         ((a_) \[CenterDot] (b_)) \[CenterDot] (c_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 33}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) \[CenterDot] 
           (((a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
              (a_))) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
              (((a_) \[CenterDot] (a_)) \[CenterDot] (a_))))) -> 
          a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"SubstitutionLemma", 34} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             a))) == (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
            a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 30}, 
        "Construct" -> {"CriticalPairLemma", 14}, "Position" -> {2, 2, 1}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (a_))) \[CenterDot] 
            ((a_) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] 
                (a_)) \[CenterDot] (a_))))) -> a \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] a), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a))) == (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] (
                (a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] a))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 35} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             a))) == (a \[CenterDot] a) \[CenterDot] 
          (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
           (a \[CenterDot] a))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 34}, "Construct" -> 
         {"CriticalPairLemma", 28}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_))) \[CenterDot] ((b_) \[CenterDot] 
            (((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] (
                a_))) \[CenterDot] (b_))) -> (a \[CenterDot] b) \[CenterDot] 
           (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                b) \[CenterDot] a))) \[CenterDot] (a \[CenterDot] b)), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a))) == (a \[CenterDot] a) \[CenterDot] 
            (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] (
                (a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
             (a \[CenterDot] a))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 36} -> 
     <|"Statement" -> HoldForm[
        ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           a) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] a)) == 
         (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 32}, 
        "Construct" -> {"SubstitutionLemma", 35}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
           ((((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
              (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) \[CenterDot] 
            ((a_) \[CenterDot] (a_))) -> (a \[CenterDot] 
            ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a))), "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a)) \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a)) \[CenterDot] a)) == (a \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 37} -> 
     <|"Statement" -> HoldForm[
        ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           a) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] a)) == a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 36}, 
        "Construct" -> {"SubstitutionLemma", 14}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (a_))) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) -> a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
             a) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] (
                (a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] a)) == a], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 38} -> 
     <|"Statement" -> HoldForm[((a \[CenterDot] a) \[CenterDot] 
           a) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] a)) == a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 37}, 
        "Construct" -> {"SubstitutionLemma", 15}, "Position" -> {1, 1}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[((a \[CenterDot] a) \[CenterDot] 
             a) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] (
                (a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] a)) == a], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 39} -> 
     <|"Statement" -> HoldForm[((a \[CenterDot] a) \[CenterDot] 
           a) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
            a)) == a], "Proof" -> <|"Input" -> {"SubstitutionLemma", 38}, 
        "Construct" -> {"SubstitutionLemma", 15}, "Position" -> {2, 2, 1}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[((a \[CenterDot] a) \[CenterDot] 
             a) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a)) == a], "Source" -> "norm"|>|>, {"SubstitutionLemma", 40} -> 
     <|"Statement" -> HoldForm[((a \[CenterDot] a) \[CenterDot] 
           a) \[CenterDot] (a \[CenterDot] a) == a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 39}, 
        "Construct" -> {"SubstitutionLemma", 15}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[((a \[CenterDot] a) \[CenterDot] 
             a) \[CenterDot] (a \[CenterDot] a) == a], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 41} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] (a \[CenterDot] 
            a)) \[CenterDot] (a \[CenterDot] a) == a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 40}, 
        "Construct" -> {"SubstitutionLemma", 20}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (a \[CenterDot] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          (a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] a) == a], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 31} -> 
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
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 23}, 
        "Orientation" -> -1, "Rule" -> 
         ((((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
            (((x3_) \[CenterDot] (a_)) \[CenterDot] ((((x3_) \[CenterDot] 
                (a_)) \[CenterDot] (b_)) \[CenterDot] ((x3_) \[CenterDot] (
                a_))))) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((b_) \[CenterDot] ((a_) \[CenterDot] (b_)))) -> 
          (x3 \[CenterDot] a) \[CenterDot] (((x3 \[CenterDot] a) \[CenterDot] 
             b) \[CenterDot] (x3 \[CenterDot] a)), "Side" -> 1, 
        "Subpattern" -> ((x3_) \[CenterDot] (a_)) \[CenterDot] (b_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 41}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((a_) \[CenterDot] ((a_) \[CenterDot] (a_))) \[CenterDot] 
           ((a_) \[CenterDot] (a_)) -> a, "MatchingSide" -> 1, 
        "Position" -> {1, 2, 2, 1}|>|>, {"SubstitutionLemma", 42} -> 
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
       <|"Input" -> {"CriticalPairLemma", 31}, "Construct" -> 
         {"SubstitutionLemma", 29}, "Position" -> {1, 1, 1}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
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
    {"SubstitutionLemma", 43} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] (a \[CenterDot] 
            a)) \[CenterDot] (((a \[CenterDot] (a \[CenterDot] 
              a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] a))) == 
         ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
              a)) \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
          (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] a))))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 42}, "Construct" -> 
         {"SubstitutionLemma", 23}, "Position" -> {1, 2, 2}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (a_))) -> a \[CenterDot] a, "Orientation" -> 1, 
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
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 44} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] (a \[CenterDot] 
            a)) \[CenterDot] (((a \[CenterDot] (a \[CenterDot] 
              a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] a))) == 
         ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
          (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] a))))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 43}, "Construct" -> 
         {"SubstitutionLemma", 41}, "Position" -> {1, 2}, 
        "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] (a_))) \[CenterDot] 
           ((a_) \[CenterDot] (a_)) -> a, "Orientation" -> 1, 
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
    {"SubstitutionLemma", 45} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] (a \[CenterDot] 
            a)) \[CenterDot] (((a \[CenterDot] (a \[CenterDot] 
              a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] a))) == 
         ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] a)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 44}, "Construct" -> 
         {"SubstitutionLemma", 21}, "Position" -> {2}, 
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
        "Construct" -> {"SubstitutionLemma", 41}, "Position" -> {2, 1}, 
        "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] (a_))) \[CenterDot] 
           ((a_) \[CenterDot] (a_)) -> a, "Orientation" -> 1, 
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
         {"SubstitutionLemma", 23}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (a_))) -> a \[CenterDot] a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          (a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] a) == ((a \[CenterDot] b) \[CenterDot] 
             a) \[CenterDot] (a \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 48} -> 
     <|"Statement" -> HoldForm[a == ((a \[CenterDot] b) \[CenterDot] 
           a) \[CenterDot] (a \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 47}, 
        "Construct" -> {"SubstitutionLemma", 41}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] (a_))) \[CenterDot] 
           ((a_) \[CenterDot] (a_)) -> a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a == ((a \[CenterDot] b) \[CenterDot] 
             a) \[CenterDot] (a \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 32} -> 
     <|"Statement" -> HoldForm[a == ((a \[CenterDot] b) \[CenterDot] 
           a) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            a))], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 30}, 
        "Orientation" -> 1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           ((((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
              (b_))) \[CenterDot] ((a_) \[CenterDot] (b_))) -> b, 
        "Side" -> 1, "Subpattern" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
          ((b_) \[CenterDot] (b_)), "MatchingConstruct" -> 
         {"SubstitutionLemma", 48}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (a_)) \[CenterDot] ((a_) \[CenterDot] (a_)) -> a, 
        "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"CriticalPairLemma", 33} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] a == 
         (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
          (((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
           ((((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
              b) \[CenterDot] a)))], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 32}, "Orientation" -> -1, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_))) -> a, "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] 
          (b_), "MatchingConstruct" -> {"SubstitutionLemma", 48}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (a_)) -> a, "MatchingSide" -> 1, 
        "Position" -> {1, 1}|>|>, {"SubstitutionLemma", 49} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] a == 
         (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
          a], "Proof" -> <|"Input" -> {"CriticalPairLemma", 33}, 
        "Construct" -> {"SubstitutionLemma", 30}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           ((((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
              (b_))) \[CenterDot] ((a_) \[CenterDot] (b_))) -> b, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (a \[CenterDot] b) \[CenterDot] a == 
           (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
            a], "Source" -> "norm"|>|>, {"CriticalPairLemma", 34} -> 
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
        "MatchingConstruct" -> {"SubstitutionLemma", 49}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_))) \[CenterDot] (a_) -> (a \[CenterDot] b) \[CenterDot] a, 
        "MatchingSide" -> 1, "Position" -> {2, 2, 1}|>|>, 
    {"SubstitutionLemma", 50} -> 
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
       <|"Input" -> {"CriticalPairLemma", 34}, "Construct" -> 
         {"SubstitutionLemma", 49}, "Position" -> {2, 1, 2, 1}, 
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
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 51} -> 
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
       <|"Input" -> {"SubstitutionLemma", 50}, "Construct" -> 
         {"CriticalPairLemma", 32}, "Position" -> {2, 1, 2}, 
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
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 52} -> 
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
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 51}, 
        "Construct" -> {"SubstitutionLemma", 49}, "Position" -> {2, 1}, 
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
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 53} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] 
          ((b \[CenterDot] a) \[CenterDot] b) == a \[CenterDot] 
          (((b \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
           (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 52}, 
        "Construct" -> {"CriticalPairLemma", 32}, "Position" -> {2, 2}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_))) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b) == 
           a \[CenterDot] (((b \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
             (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 54} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] 
          ((b \[CenterDot] a) \[CenterDot] b) == a \[CenterDot] b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 53}, 
        "Construct" -> {"CriticalPairLemma", 32}, "Position" -> {2}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_))) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b) == 
           a \[CenterDot] b], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 55} -> 
     <|"Statement" -> HoldForm[c == ((b \[CenterDot] a) \[CenterDot] 
           c) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
            b))], "Proof" -> <|"Input" -> {"CriticalPairLemma", 2}, 
        "Construct" -> {"SubstitutionLemma", 54}, "Position" -> {1, 1}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (a_)) -> b \[CenterDot] a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[c == ((b \[CenterDot] a) \[CenterDot] 
             c) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
              b))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 56} -> 
     <|"Statement" -> HoldForm[c == ((b \[CenterDot] a) \[CenterDot] 
           c) \[CenterDot] (c \[CenterDot] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 55}, 
        "Construct" -> {"SubstitutionLemma", 54}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (a_)) -> b \[CenterDot] a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[c == ((b \[CenterDot] a) \[CenterDot] 
             c) \[CenterDot] (c \[CenterDot] b)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 35} -> 
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
        "Position" -> {1}|>|>, {"SubstitutionLemma", 57} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] 
          ((b \[CenterDot] a) \[CenterDot] b) == a \[CenterDot] 
          ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           a)], "Proof" -> <|"Input" -> {"CriticalPairLemma", 35}, 
        "Construct" -> {"SubstitutionLemma", 54}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (a_)) -> b \[CenterDot] a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[b \[CenterDot] 
            ((b \[CenterDot] a) \[CenterDot] b) == a \[CenterDot] 
            ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b)) \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 58} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] 
          ((b \[CenterDot] a) \[CenterDot] b) == a \[CenterDot] 
          ((a \[CenterDot] b) \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 57}, 
        "Construct" -> {"SubstitutionLemma", 54}, "Position" -> {2, 1}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (a_)) -> b \[CenterDot] a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[b \[CenterDot] 
            ((b \[CenterDot] a) \[CenterDot] b) == a \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 59} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] 
          ((b \[CenterDot] a) \[CenterDot] b) == b \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 58}, 
        "Construct" -> {"SubstitutionLemma", 54}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (a_)) -> b \[CenterDot] a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[b \[CenterDot] 
            ((b \[CenterDot] a) \[CenterDot] b) == b \[CenterDot] a], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 60} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] a == a \[CenterDot] b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 59}, 
        "Construct" -> {"SubstitutionLemma", 54}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (a_)) -> b \[CenterDot] a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[b \[CenterDot] a == a \[CenterDot] b], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 61} -> 
     <|"Statement" -> HoldForm[c == (c \[CenterDot] a) \[CenterDot] 
          ((a \[CenterDot] b) \[CenterDot] c)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 56}, 
        "Construct" -> {"SubstitutionLemma", 60}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          c == (c \[CenterDot] a) \[CenterDot] ((a \[CenterDot] 
              b) \[CenterDot] c)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 62} -> 
     <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
          (b \[CenterDot] (c \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 4}, 
        "Construct" -> {"SubstitutionLemma", 54}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (a_)) -> b \[CenterDot] a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
            (b \[CenterDot] (c \[CenterDot] a))], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 36} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] c) == 
         ((a \[CenterDot] (b \[CenterDot] c)) \[CenterDot] c) \[CenterDot] 
          a], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 61}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((b_) \[CenterDot] (c_)) \[CenterDot] (a_)) -> a, "Side" -> 1, 
        "Subpattern" -> ((b_) \[CenterDot] (c_)) \[CenterDot] (a_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 62}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
            ((c_) \[CenterDot] (a_))) -> b, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 63} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] c) == 
         a \[CenterDot] ((a \[CenterDot] (b \[CenterDot] c)) \[CenterDot] 
           c)], "Proof" -> <|"Input" -> {"CriticalPairLemma", 36}, 
        "Construct" -> {"SubstitutionLemma", 60}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CenterDot] (b \[CenterDot] c) == a \[CenterDot] 
            ((a \[CenterDot] (b \[CenterDot] c)) \[CenterDot] c)], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 64} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] c) == 
         a \[CenterDot] (c \[CenterDot] (a \[CenterDot] (b \[CenterDot] 
             c)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 63}, 
        "Construct" -> {"SubstitutionLemma", 60}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CenterDot] (b \[CenterDot] c) == a \[CenterDot] 
            (c \[CenterDot] (a \[CenterDot] (b \[CenterDot] c)))], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 37} -> 
     <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
          ((a \[CenterDot] c) \[CenterDot] b)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 61}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((b_) \[CenterDot] (c_)) \[CenterDot] (a_)) -> a, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 60}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 38} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] c == 
         (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] c)) \[CenterDot] 
          c], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 37}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((a_) \[CenterDot] (c_)) \[CenterDot] (b_)) -> b, "Side" -> 1, 
        "Subpattern" -> ((a_) \[CenterDot] (c_)) \[CenterDot] (b_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 37}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] (b_)) \[CenterDot] (((a_) \[CenterDot] 
             (c_)) \[CenterDot] (b_)) -> b, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 65} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] c == 
         c \[CenterDot] (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            c))], "Proof" -> <|"Input" -> {"CriticalPairLemma", 38}, 
        "Construct" -> {"SubstitutionLemma", 60}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (a \[CenterDot] b) \[CenterDot] c == c \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] c))], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 39} -> 
     <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
          ((c \[CenterDot] a) \[CenterDot] b)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 62}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           ((b_) \[CenterDot] ((c_) \[CenterDot] (a_))) -> b, "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] ((c_) \[CenterDot] (a_)), 
        "MatchingConstruct" -> {"SubstitutionLemma", 60}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CenterDot] (b_) -> b \[CenterDot] a, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 40} -> 
     <|"Statement" -> HoldForm[((b \[CenterDot] a) \[CenterDot] 
           b) \[CenterDot] a == a \[CenterDot] a], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 65}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            (((b_) \[CenterDot] (c_)) \[CenterDot] (a_))) -> 
          (b \[CenterDot] c) \[CenterDot] a, "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] (((b_) \[CenterDot] 
            (c_)) \[CenterDot] (a_)), "MatchingConstruct" -> 
         {"CriticalPairLemma", 39}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((c_) \[CenterDot] (a_)) \[CenterDot] (b_)) -> b, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 66} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((b \[CenterDot] a) \[CenterDot] b) == a \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 40}, 
        "Construct" -> {"SubstitutionLemma", 60}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b) == 
           a \[CenterDot] a], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 67} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
           (b \[CenterDot] a)) == a \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 66}, 
        "Construct" -> {"SubstitutionLemma", 60}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a \[CenterDot] (b \[CenterDot] (b \[CenterDot] a)) == 
           a \[CenterDot] a], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 41} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] b) == 
         a \[CenterDot] (b \[CenterDot] b)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 64}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            ((a_) \[CenterDot] ((c_) \[CenterDot] (b_)))) -> 
          a \[CenterDot] (c \[CenterDot] b), "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] ((a_) \[CenterDot] 
           ((c_) \[CenterDot] (b_))), "MatchingConstruct" -> 
         {"SubstitutionLemma", 67}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            ((b_) \[CenterDot] (a_))) -> a \[CenterDot] a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"Conclusion", 1} -> <|"Statement" -> 
       HoldForm[a \[CenterDot] (a \[CenterDot] b) == a \[CenterDot] 
          (a \[CenterDot] b)], "Proof" -> <|"Input" -> {"Hypothesis", 1}, 
        "Construct" -> {"CriticalPairLemma", 41}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (b_)) -> 
          a \[CenterDot] (a \[CenterDot] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CenterDot] (a \[CenterDot] b) == 
           a \[CenterDot] (a \[CenterDot] b)], "Source" -> "cpl"|>|>}|>]
