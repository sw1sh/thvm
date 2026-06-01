ProofObject["EquationalLogic", Inactive[Equal][
  a \[CircleDot] ((a \[CircleDot] a) \[CircleDot] 
    (b \[CircleDot] ((a \[CircleDot] a) \[CircleDot] c))), 
  (a \[CircleDot] ((a \[CircleDot] a) \[CircleDot] b)) \[CircleDot] 
   ((a \[CircleDot] a) \[CircleDot] c)], 
 {Inactive[Equal][(a_) \[CircleDot] 
    (((((a_) \[CircleDot] (a_)) \[CircleDot] (b_)) \[CircleDot] 
      (c_)) \[CircleDot] ((((a_) \[CircleDot] (a_)) \[CircleDot] 
       (a_)) \[CircleDot] (c_))), b_]}, <|"Variables" -> {a, b, c, x3, x4}, 
  "Constants" -> {}, "Proof" -> 
   {{"Axiom", 1} -> <|"Statement" -> HoldForm[
        a \[CircleDot] ((((a \[CircleDot] a) \[CircleDot] b) \[CircleDot] 
            c) \[CircleDot] (((a \[CircleDot] a) \[CircleDot] a) \[CircleDot] 
            c)) == b], "Proof" -> <||>|>, {"Hypothesis", 1} -> 
     <|"Statement" -> HoldForm[a \[CircleDot] 
          ((a \[CircleDot] a) \[CircleDot] (b \[CircleDot] 
            ((a \[CircleDot] a) \[CircleDot] c))) == 
         (a \[CircleDot] ((a \[CircleDot] a) \[CircleDot] b)) \[CircleDot] 
          ((a \[CircleDot] a) \[CircleDot] c)], "Proof" -> <||>|>, 
    {"CriticalPairLemma", 1} -> 
     <|"Statement" -> HoldForm[
        ((((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] a)) \[CircleDot] 
            b) \[CircleDot] x3) \[CircleDot] 
          ((((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] a)) \[CircleDot] 
            (a \[CircleDot] a)) \[CircleDot] x3) == a \[CircleDot] 
          ((b \[CircleDot] c) \[CircleDot] (((a \[CircleDot] a) \[CircleDot] 
             a) \[CircleDot] c))], "Proof" -> <|"Construct" -> {"Axiom", 1}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CircleDot] 
           (((((a_) \[CircleDot] (a_)) \[CircleDot] (b_)) \[CircleDot] 
             (c_)) \[CircleDot] ((((a_) \[CircleDot] (a_)) \[CircleDot] 
              (a_)) \[CircleDot] (c_))) -> b, "Side" -> 1, 
        "Subpattern" -> ((a_) \[CircleDot] (a_)) \[CircleDot] (b_), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CircleDot] 
           (((((a_) \[CircleDot] (a_)) \[CircleDot] (b_)) \[CircleDot] 
             (c_)) \[CircleDot] ((((a_) \[CircleDot] (a_)) \[CircleDot] 
              (a_)) \[CircleDot] (c_))) -> b, "MatchingSide" -> 1, 
        "Position" -> {2, 1, 1}|>|>, {"CriticalPairLemma", 2} -> 
     <|"Statement" -> HoldForm[
        ((((b \[CircleDot] b) \[CircleDot] (b \[CircleDot] b)) \[CircleDot] 
            ((b \[CircleDot] b) \[CircleDot] a)) \[CircleDot] c) \[CircleDot] 
          ((((b \[CircleDot] b) \[CircleDot] (b \[CircleDot] b)) \[CircleDot] 
            (b \[CircleDot] b)) \[CircleDot] c) == a], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 1}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CircleDot] 
           (((b_) \[CircleDot] (c_)) \[CircleDot] 
            ((((a_) \[CircleDot] (a_)) \[CircleDot] (a_)) \[CircleDot] 
             (c_))) -> ((((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] 
               a)) \[CircleDot] b) \[CircleDot] x3) \[CircleDot] 
           ((((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] 
               a)) \[CircleDot] (a \[CircleDot] a)) \[CircleDot] x3), 
        "Side" -> 1, "Subpattern" -> {}, "MatchingConstruct" -> {"Axiom", 1}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleDot] (((((a_) \[CircleDot] (a_)) \[CircleDot] 
              (b_)) \[CircleDot] (c_)) \[CircleDot] 
            ((((a_) \[CircleDot] (a_)) \[CircleDot] (a_)) \[CircleDot] 
             (c_))) -> b, "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"CriticalPairLemma", 3} -> 
     <|"Statement" -> HoldForm[(b \[CircleDot] x4) \[CircleDot] 
          ((((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] a)) \[CircleDot] 
            (a \[CircleDot] a)) \[CircleDot] x4) == 
         ((((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] a)) \[CircleDot] 
            ((((((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] 
                  a)) \[CircleDot] ((a \[CircleDot] a) \[CircleDot] 
                 (a \[CircleDot] a))) \[CircleDot] b) \[CircleDot] 
              c) \[CircleDot] (((((a \[CircleDot] a) \[CircleDot] 
                 (a \[CircleDot] a)) \[CircleDot] ((a \[CircleDot] 
                  a) \[CircleDot] (a \[CircleDot] a))) \[CircleDot] (
                (a \[CircleDot] a) \[CircleDot] (a \[CircleDot] 
                 a))) \[CircleDot] c))) \[CircleDot] x3) \[CircleDot] 
          ((((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] a)) \[CircleDot] 
            (a \[CircleDot] a)) \[CircleDot] x3)], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 2}, 
        "Orientation" -> 1, "Rule" -> 
         (((((a_) \[CircleDot] (a_)) \[CircleDot] ((a_) \[CircleDot] (
                a_))) \[CircleDot] (((a_) \[CircleDot] (a_)) \[CircleDot] 
              (b_))) \[CircleDot] (c_)) \[CircleDot] 
           (((((a_) \[CircleDot] (a_)) \[CircleDot] ((a_) \[CircleDot] (
                a_))) \[CircleDot] ((a_) \[CircleDot] (a_))) \[CircleDot] 
            (c_)) -> b, "Side" -> 1, "Subpattern" -> 
         ((a_) \[CircleDot] (a_)) \[CircleDot] (b_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 1}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CircleDot] (((b_) \[CircleDot] 
             (c_)) \[CircleDot] ((((a_) \[CircleDot] (a_)) \[CircleDot] 
              (a_)) \[CircleDot] (c_))) -> 
          ((((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] a)) \[CircleDot] 
             b) \[CircleDot] x3) \[CircleDot] 
           ((((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] 
               a)) \[CircleDot] (a \[CircleDot] a)) \[CircleDot] x3), 
        "MatchingSide" -> 1, "Position" -> {1, 1, 2}|>|>, 
    {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[(b \[CircleDot] x4) \[CircleDot] 
          ((((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] a)) \[CircleDot] 
            (a \[CircleDot] a)) \[CircleDot] x4) == 
         (b \[CircleDot] x3) \[CircleDot] ((((a \[CircleDot] a) \[CircleDot] 
             (a \[CircleDot] a)) \[CircleDot] (a \[CircleDot] 
             a)) \[CircleDot] x3)], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 3}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {1, 1}, "Rule" -> (a_) \[CircleDot] 
           (((((a_) \[CircleDot] (a_)) \[CircleDot] (b_)) \[CircleDot] 
             (c_)) \[CircleDot] ((((a_) \[CircleDot] (a_)) \[CircleDot] 
              (a_)) \[CircleDot] (c_))) -> b, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[(b \[CircleDot] x4) \[CircleDot] 
            ((((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] 
                a)) \[CircleDot] (a \[CircleDot] a)) \[CircleDot] x4) == 
           (b \[CircleDot] x3) \[CircleDot] 
            ((((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] 
                a)) \[CircleDot] (a \[CircleDot] a)) \[CircleDot] x3)], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 4} -> 
     <|"Statement" -> HoldForm[(((c \[CircleDot] c) \[CircleDot] 
            (c \[CircleDot] c)) \[CircleDot] (c \[CircleDot] c)) \[CircleDot] 
          a == a \[CircleDot] ((((a \[CircleDot] b) \[CircleDot] 
             ((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                 c)) \[CircleDot] (c \[CircleDot] c)) \[CircleDot] 
              b)) \[CircleDot] x3) \[CircleDot] 
           (((a \[CircleDot] a) \[CircleDot] a) \[CircleDot] x3))], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CircleDot] (((((a_) \[CircleDot] (a_)) \[CircleDot] 
              (b_)) \[CircleDot] (c_)) \[CircleDot] 
            ((((a_) \[CircleDot] (a_)) \[CircleDot] (a_)) \[CircleDot] 
             (c_))) -> b, "Side" -> 1, "Subpattern" -> 
         ((a_) \[CircleDot] (a_)) \[CircleDot] (b_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 1}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> ((a_) \[CircleDot] (b_)) \[CircleDot] 
           (((((c_) \[CircleDot] (c_)) \[CircleDot] ((c_) \[CircleDot] (
                c_))) \[CircleDot] ((c_) \[CircleDot] (c_))) \[CircleDot] 
            (b_)) -> (a \[CircleDot] x3) \[CircleDot] 
           ((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
               c)) \[CircleDot] (c \[CircleDot] c)) \[CircleDot] x3), 
        "MatchingSide" -> 1, "Position" -> {2, 1, 1}|>|>, 
    {"CriticalPairLemma", 5} -> 
     <|"Statement" -> HoldForm[(a \[CircleDot] c) \[CircleDot] 
          (((((b \[CircleDot] b) \[CircleDot] (b \[CircleDot] 
               b)) \[CircleDot] ((b \[CircleDot] b) \[CircleDot] 
              (b \[CircleDot] b))) \[CircleDot] ((b \[CircleDot] 
              b) \[CircleDot] (b \[CircleDot] b))) \[CircleDot] c) == 
         (a \[CircleDot] ((((b \[CircleDot] b) \[CircleDot] (b \[CircleDot] 
               b)) \[CircleDot] (b \[CircleDot] b)) \[CircleDot] 
            ((b \[CircleDot] b) \[CircleDot] (b \[CircleDot] 
              b)))) \[CircleDot] (b \[CircleDot] b)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 1}, 
        "Orientation" -> 1, "Rule" -> ((a_) \[CircleDot] (b_)) \[CircleDot] 
           (((((c_) \[CircleDot] (c_)) \[CircleDot] ((c_) \[CircleDot] (
                c_))) \[CircleDot] ((c_) \[CircleDot] (c_))) \[CircleDot] 
            (b_)) -> (a \[CircleDot] x3) \[CircleDot] 
           ((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
               c)) \[CircleDot] (c \[CircleDot] c)) \[CircleDot] x3), 
        "Side" -> 1, "Subpattern" -> ((((c_) \[CircleDot] (c_)) \[CircleDot] 
            ((c_) \[CircleDot] (c_))) \[CircleDot] ((c_) \[CircleDot] 
            (c_))) \[CircleDot] (b_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 2}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (((((a_) \[CircleDot] (a_)) \[CircleDot] 
              ((a_) \[CircleDot] (a_))) \[CircleDot] (((a_) \[CircleDot] (
                a_)) \[CircleDot] (b_))) \[CircleDot] (c_)) \[CircleDot] 
           (((((a_) \[CircleDot] (a_)) \[CircleDot] ((a_) \[CircleDot] (
                a_))) \[CircleDot] ((a_) \[CircleDot] (a_))) \[CircleDot] 
            (c_)) -> b, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 6} -> 
     <|"Statement" -> HoldForm[
        ((((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                  c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                 (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                 c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
              x3) \[CircleDot] (((((c \[CircleDot] c) \[CircleDot] 
                 (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                  c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] (
                (c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                 c))) \[CircleDot] x3)) \[CircleDot] 
            ((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                  c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                 (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                 c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
              x3) \[CircleDot] (((((c \[CircleDot] c) \[CircleDot] 
                 (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                  c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] (
                (c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                 c))) \[CircleDot] x3))) \[CircleDot] 
           ((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                 c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
             x3) \[CircleDot] (((((c \[CircleDot] c) \[CircleDot] 
                (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                 c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
              ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                c))) \[CircleDot] x3))) \[CircleDot] a == 
         a \[CircleDot] ((((a \[CircleDot] b) \[CircleDot] 
             (((((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                      c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                     (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                     c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                  ((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                      c)) \[CircleDot] (c \[CircleDot] c)) \[CircleDot] 
                   ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                     c)))) \[CircleDot] (c \[CircleDot] c)) \[CircleDot] 
                ((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                      c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                     (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                     c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                  x3) \[CircleDot] (((((c \[CircleDot] c) \[CircleDot] 
                     (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                      c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                   ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                     c))) \[CircleDot] x3))) \[CircleDot] (
                (((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                     c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                    (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                    c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                 x3) \[CircleDot] (((((c \[CircleDot] c) \[CircleDot] 
                    (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                     c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                  ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                    c))) \[CircleDot] x3))) \[CircleDot] b)) \[CircleDot] 
            x4) \[CircleDot] (((a \[CircleDot] a) \[CircleDot] 
             a) \[CircleDot] x4))], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 4}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CircleDot] (((((a_) \[CircleDot] (b_)) \[CircleDot] 
              (((((c_) \[CircleDot] (c_)) \[CircleDot] ((c_) \[CircleDot] 
                  (c_))) \[CircleDot] ((c_) \[CircleDot] (c_))) \[CircleDot] (
                b_))) \[CircleDot] (x3_)) \[CircleDot] 
            ((((a_) \[CircleDot] (a_)) \[CircleDot] (a_)) \[CircleDot] 
             (x3_))) -> (((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
              c)) \[CircleDot] (c \[CircleDot] c)) \[CircleDot] a, 
        "Side" -> 1, "Subpattern" -> (c_) \[CircleDot] (c_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 5}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((a_) \[CircleDot] (b_)) \[CircleDot] 
           ((((((c_) \[CircleDot] (c_)) \[CircleDot] ((c_) \[CircleDot] 
                (c_))) \[CircleDot] (((c_) \[CircleDot] (c_)) \[CircleDot] (
                (c_) \[CircleDot] (c_)))) \[CircleDot] 
             (((c_) \[CircleDot] (c_)) \[CircleDot] ((c_) \[CircleDot] (
                c_)))) \[CircleDot] (b_)) -> 
          (a \[CircleDot] ((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                c)) \[CircleDot] (c \[CircleDot] c)) \[CircleDot] 
             ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
               c)))) \[CircleDot] (c \[CircleDot] c), "MatchingSide" -> 1, 
        "Position" -> {2, 1, 1, 2, 1, 1, 1}|>|>, {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[
        ((((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                  c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                 (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                 c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
              x3) \[CircleDot] (((((c \[CircleDot] c) \[CircleDot] 
                 (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                  c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] (
                (c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                 c))) \[CircleDot] x3)) \[CircleDot] 
            ((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                  c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                 (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                 c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
              x3) \[CircleDot] (((((c \[CircleDot] c) \[CircleDot] 
                 (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                  c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] (
                (c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                 c))) \[CircleDot] x3))) \[CircleDot] 
           ((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                 c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
             x3) \[CircleDot] (((((c \[CircleDot] c) \[CircleDot] 
                (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                 c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
              ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                c))) \[CircleDot] x3))) \[CircleDot] a == 
         a \[CircleDot] ((((a \[CircleDot] b) \[CircleDot] 
             (((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                  c)) \[CircleDot] ((((((c \[CircleDot] c) \[CircleDot] 
                     (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                      c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                   ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                     c))) \[CircleDot] x3) \[CircleDot] 
                 (((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                      c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                     (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                     c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                  x3))) \[CircleDot] ((((((c \[CircleDot] c) \[CircleDot] 
                    (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                     c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                  ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                    c))) \[CircleDot] x3) \[CircleDot] 
                (((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                     c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                    (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                    c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                 x3))) \[CircleDot] b)) \[CircleDot] x4) \[CircleDot] 
           (((a \[CircleDot] a) \[CircleDot] a) \[CircleDot] x4))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 6}, 
        "Construct" -> {"CriticalPairLemma", 2}, "Position" -> {2, 1, 1, 2, 
         1, 1, 1, 1}, "Rule" -> (((((a_) \[CircleDot] (a_)) \[CircleDot] 
              ((a_) \[CircleDot] (a_))) \[CircleDot] (((a_) \[CircleDot] (
                a_)) \[CircleDot] (b_))) \[CircleDot] (c_)) \[CircleDot] 
           (((((a_) \[CircleDot] (a_)) \[CircleDot] ((a_) \[CircleDot] (
                a_))) \[CircleDot] ((a_) \[CircleDot] (a_))) \[CircleDot] 
            (c_)) -> b, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[((((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                    c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                   (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                   c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                x3) \[CircleDot] (((((c \[CircleDot] c) \[CircleDot] 
                   (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                    c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                 ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                   c))) \[CircleDot] x3)) \[CircleDot] 
              ((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                    c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                   (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                   c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                x3) \[CircleDot] (((((c \[CircleDot] c) \[CircleDot] 
                   (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                    c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                 ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                   c))) \[CircleDot] x3))) \[CircleDot] 
             ((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                   c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                  (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                  c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
               x3) \[CircleDot] (((((c \[CircleDot] c) \[CircleDot] 
                  (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                   c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                  c))) \[CircleDot] x3))) \[CircleDot] a == 
           a \[CircleDot] ((((a \[CircleDot] b) \[CircleDot] (
                ((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                    c)) \[CircleDot] ((((((c \[CircleDot] c) \[CircleDot] 
                       (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                        c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                     ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                       c))) \[CircleDot] x3) \[CircleDot] 
                   (((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                        c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                       (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                       c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                    x3))) \[CircleDot] ((((((c \[CircleDot] c) \[CircleDot] 
                      (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                       c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                    ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                      c))) \[CircleDot] x3) \[CircleDot] 
                  (((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                       c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                      (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                      c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                   x3))) \[CircleDot] b)) \[CircleDot] x4) \[CircleDot] 
             (((a \[CircleDot] a) \[CircleDot] a) \[CircleDot] x4))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 3} -> 
     <|"Statement" -> HoldForm[
        ((((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                  c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                 (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                 c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
              x3) \[CircleDot] (((((c \[CircleDot] c) \[CircleDot] 
                 (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                  c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] (
                (c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                 c))) \[CircleDot] x3)) \[CircleDot] 
            ((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                  c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                 (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                 c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
              x3) \[CircleDot] (((((c \[CircleDot] c) \[CircleDot] 
                 (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                  c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] (
                (c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                 c))) \[CircleDot] x3))) \[CircleDot] 
           ((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                 c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
             x3) \[CircleDot] (((((c \[CircleDot] c) \[CircleDot] 
                (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                 c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
              ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                c))) \[CircleDot] x3))) \[CircleDot] a == 
         a \[CircleDot] ((((a \[CircleDot] b) \[CircleDot] 
             ((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                 c)) \[CircleDot] ((((((c \[CircleDot] c) \[CircleDot] 
                    (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                     c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                  ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                    c))) \[CircleDot] x3) \[CircleDot] 
                (((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                     c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                    (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                    c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                 x3))) \[CircleDot] b)) \[CircleDot] x4) \[CircleDot] 
           (((a \[CircleDot] a) \[CircleDot] a) \[CircleDot] x4))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 2}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {2, 1, 1, 2, 1, 1}, 
        "Rule" -> (a_) \[CircleDot] (((((a_) \[CircleDot] (a_)) \[CircleDot] 
              (b_)) \[CircleDot] (c_)) \[CircleDot] 
            ((((a_) \[CircleDot] (a_)) \[CircleDot] (a_)) \[CircleDot] 
             (c_))) -> b, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[((((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                    c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                   (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                   c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                x3) \[CircleDot] (((((c \[CircleDot] c) \[CircleDot] 
                   (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                    c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                 ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                   c))) \[CircleDot] x3)) \[CircleDot] 
              ((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                    c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                   (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                   c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                x3) \[CircleDot] (((((c \[CircleDot] c) \[CircleDot] 
                   (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                    c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                 ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                   c))) \[CircleDot] x3))) \[CircleDot] 
             ((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                   c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                  (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                  c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
               x3) \[CircleDot] (((((c \[CircleDot] c) \[CircleDot] 
                  (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                   c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                  c))) \[CircleDot] x3))) \[CircleDot] a == 
           a \[CircleDot] ((((a \[CircleDot] b) \[CircleDot] (
                (((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                   c)) \[CircleDot] ((((((c \[CircleDot] c) \[CircleDot] 
                      (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                       c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                    ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                      c))) \[CircleDot] x3) \[CircleDot] 
                  (((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                       c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                      (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                      c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                   x3))) \[CircleDot] b)) \[CircleDot] x4) \[CircleDot] 
             (((a \[CircleDot] a) \[CircleDot] a) \[CircleDot] x4))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 4} -> 
     <|"Statement" -> HoldForm[
        ((((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                  c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                 (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                 c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
              x3) \[CircleDot] (((((c \[CircleDot] c) \[CircleDot] 
                 (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                  c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] (
                (c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                 c))) \[CircleDot] x3)) \[CircleDot] 
            ((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                  c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                 (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                 c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
              x3) \[CircleDot] (((((c \[CircleDot] c) \[CircleDot] 
                 (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                  c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] (
                (c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                 c))) \[CircleDot] x3))) \[CircleDot] 
           ((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                 c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
             x3) \[CircleDot] (((((c \[CircleDot] c) \[CircleDot] 
                (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                 c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
              ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                c))) \[CircleDot] x3))) \[CircleDot] a == 
         a \[CircleDot] ((((a \[CircleDot] b) \[CircleDot] 
             (((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                c)) \[CircleDot] b)) \[CircleDot] x4) \[CircleDot] 
           (((a \[CircleDot] a) \[CircleDot] a) \[CircleDot] x4))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 3}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {2, 1, 1, 2, 1}, 
        "Rule" -> (a_) \[CircleDot] (((((a_) \[CircleDot] (a_)) \[CircleDot] 
              (b_)) \[CircleDot] (c_)) \[CircleDot] 
            ((((a_) \[CircleDot] (a_)) \[CircleDot] (a_)) \[CircleDot] 
             (c_))) -> b, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[((((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                    c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                   (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                   c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                x3) \[CircleDot] (((((c \[CircleDot] c) \[CircleDot] 
                   (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                    c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                 ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                   c))) \[CircleDot] x3)) \[CircleDot] 
              ((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                    c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                   (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                   c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                x3) \[CircleDot] (((((c \[CircleDot] c) \[CircleDot] 
                   (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                    c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                 ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                   c))) \[CircleDot] x3))) \[CircleDot] 
             ((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                   c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                  (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                  c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
               x3) \[CircleDot] (((((c \[CircleDot] c) \[CircleDot] 
                  (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                   c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                  c))) \[CircleDot] x3))) \[CircleDot] a == 
           a \[CircleDot] ((((a \[CircleDot] b) \[CircleDot] (
                ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                  c)) \[CircleDot] b)) \[CircleDot] x4) \[CircleDot] 
             (((a \[CircleDot] a) \[CircleDot] a) \[CircleDot] x4))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 5} -> 
     <|"Statement" -> HoldForm[
        ((((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                  c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                 (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                 c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
              ((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                  c)) \[CircleDot] (c \[CircleDot] c)) \[CircleDot] (
                (c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                 c)))) \[CircleDot] (c \[CircleDot] c)) \[CircleDot] 
            ((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                  c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                 (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                 c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
              x3) \[CircleDot] (((((c \[CircleDot] c) \[CircleDot] 
                 (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                  c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] (
                (c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                 c))) \[CircleDot] x3))) \[CircleDot] 
           ((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                 c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
             x3) \[CircleDot] (((((c \[CircleDot] c) \[CircleDot] 
                (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                 c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
              ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                c))) \[CircleDot] x3))) \[CircleDot] a == 
         a \[CircleDot] ((((a \[CircleDot] b) \[CircleDot] 
             (((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                c)) \[CircleDot] b)) \[CircleDot] x4) \[CircleDot] 
           (((a \[CircleDot] a) \[CircleDot] a) \[CircleDot] x4))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 4}, 
        "Construct" -> {"CriticalPairLemma", 5}, "Position" -> {1, 1, 1}, 
        "Rule" -> ((a_) \[CircleDot] (b_)) \[CircleDot] 
           ((((((c_) \[CircleDot] (c_)) \[CircleDot] ((c_) \[CircleDot] 
                (c_))) \[CircleDot] (((c_) \[CircleDot] (c_)) \[CircleDot] (
                (c_) \[CircleDot] (c_)))) \[CircleDot] 
             (((c_) \[CircleDot] (c_)) \[CircleDot] ((c_) \[CircleDot] (
                c_)))) \[CircleDot] (b_)) -> 
          (a \[CircleDot] ((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                c)) \[CircleDot] (c \[CircleDot] c)) \[CircleDot] 
             ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
               c)))) \[CircleDot] (c \[CircleDot] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          ((((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                    c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                   (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                   c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                ((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                    c)) \[CircleDot] (c \[CircleDot] c)) \[CircleDot] 
                 ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                   c)))) \[CircleDot] (c \[CircleDot] c)) \[CircleDot] 
              ((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                    c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                   (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                   c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                x3) \[CircleDot] (((((c \[CircleDot] c) \[CircleDot] 
                   (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                    c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                 ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                   c))) \[CircleDot] x3))) \[CircleDot] 
             ((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                   c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                  (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                  c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
               x3) \[CircleDot] (((((c \[CircleDot] c) \[CircleDot] 
                  (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                   c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                  c))) \[CircleDot] x3))) \[CircleDot] a == 
           a \[CircleDot] ((((a \[CircleDot] b) \[CircleDot] (
                ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                  c)) \[CircleDot] b)) \[CircleDot] x4) \[CircleDot] 
             (((a \[CircleDot] a) \[CircleDot] a) \[CircleDot] x4))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 6} -> 
     <|"Statement" -> HoldForm[
        ((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] c)) \[CircleDot] 
            ((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                  c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                 (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                 c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
              x3) \[CircleDot] (((((c \[CircleDot] c) \[CircleDot] 
                 (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                  c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] (
                (c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                 c))) \[CircleDot] x3))) \[CircleDot] 
           ((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                 c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
             x3) \[CircleDot] (((((c \[CircleDot] c) \[CircleDot] 
                (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                 c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
              ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                c))) \[CircleDot] x3))) \[CircleDot] a == 
         a \[CircleDot] ((((a \[CircleDot] b) \[CircleDot] 
             (((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                c)) \[CircleDot] b)) \[CircleDot] x4) \[CircleDot] 
           (((a \[CircleDot] a) \[CircleDot] a) \[CircleDot] x4))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 5}, 
        "Construct" -> {"CriticalPairLemma", 2}, "Position" -> {1, 1, 1, 1}, 
        "Rule" -> (((((a_) \[CircleDot] (a_)) \[CircleDot] ((a_) \[CircleDot] 
               (a_))) \[CircleDot] (((a_) \[CircleDot] (a_)) \[CircleDot] 
              (b_))) \[CircleDot] (c_)) \[CircleDot] 
           (((((a_) \[CircleDot] (a_)) \[CircleDot] ((a_) \[CircleDot] (
                a_))) \[CircleDot] ((a_) \[CircleDot] (a_))) \[CircleDot] 
            (c_)) -> b, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                c)) \[CircleDot] ((((((c \[CircleDot] c) \[CircleDot] 
                   (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                    c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                 ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                   c))) \[CircleDot] x3) \[CircleDot] (
                ((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                    c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                   (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                   c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                x3))) \[CircleDot] ((((((c \[CircleDot] c) \[CircleDot] 
                  (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                   c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                  c))) \[CircleDot] x3) \[CircleDot] 
              (((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                   c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                  (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                  c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
               x3))) \[CircleDot] a == a \[CircleDot] 
            ((((a \[CircleDot] b) \[CircleDot] (((c \[CircleDot] 
                  c) \[CircleDot] (c \[CircleDot] c)) \[CircleDot] 
                b)) \[CircleDot] x4) \[CircleDot] (((a \[CircleDot] 
                a) \[CircleDot] a) \[CircleDot] x4))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 7} -> 
     <|"Statement" -> HoldForm[(((c \[CircleDot] c) \[CircleDot] 
            (c \[CircleDot] c)) \[CircleDot] 
           ((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                 c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
             x3) \[CircleDot] (((((c \[CircleDot] c) \[CircleDot] 
                (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                 c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
              ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                c))) \[CircleDot] x3))) \[CircleDot] a == 
         a \[CircleDot] ((((a \[CircleDot] b) \[CircleDot] 
             (((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                c)) \[CircleDot] b)) \[CircleDot] x4) \[CircleDot] 
           (((a \[CircleDot] a) \[CircleDot] a) \[CircleDot] x4))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 6}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {1, 1}, 
        "Rule" -> (a_) \[CircleDot] (((((a_) \[CircleDot] (a_)) \[CircleDot] 
              (b_)) \[CircleDot] (c_)) \[CircleDot] 
            ((((a_) \[CircleDot] (a_)) \[CircleDot] (a_)) \[CircleDot] 
             (c_))) -> b, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[(((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
               c)) \[CircleDot] ((((((c \[CircleDot] c) \[CircleDot] 
                  (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                   c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                  c))) \[CircleDot] x3) \[CircleDot] 
              (((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                   c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                  (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                  c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
               x3))) \[CircleDot] a == a \[CircleDot] 
            ((((a \[CircleDot] b) \[CircleDot] (((c \[CircleDot] 
                  c) \[CircleDot] (c \[CircleDot] c)) \[CircleDot] 
                b)) \[CircleDot] x4) \[CircleDot] (((a \[CircleDot] 
                a) \[CircleDot] a) \[CircleDot] x4))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 8} -> 
     <|"Statement" -> HoldForm[((c \[CircleDot] c) \[CircleDot] 
           (c \[CircleDot] c)) \[CircleDot] a == a \[CircleDot] 
          ((((a \[CircleDot] b) \[CircleDot] (((c \[CircleDot] 
                c) \[CircleDot] (c \[CircleDot] c)) \[CircleDot] 
              b)) \[CircleDot] x4) \[CircleDot] 
           (((a \[CircleDot] a) \[CircleDot] a) \[CircleDot] x4))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 7}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {1}, 
        "Rule" -> (a_) \[CircleDot] (((((a_) \[CircleDot] (a_)) \[CircleDot] 
              (b_)) \[CircleDot] (c_)) \[CircleDot] 
            ((((a_) \[CircleDot] (a_)) \[CircleDot] (a_)) \[CircleDot] 
             (c_))) -> b, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
              c)) \[CircleDot] a == a \[CircleDot] 
            ((((a \[CircleDot] b) \[CircleDot] (((c \[CircleDot] 
                  c) \[CircleDot] (c \[CircleDot] c)) \[CircleDot] 
                b)) \[CircleDot] x4) \[CircleDot] (((a \[CircleDot] 
                a) \[CircleDot] a) \[CircleDot] x4))], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 7} -> 
     <|"Statement" -> HoldForm[
        (((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                 c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
             x3) \[CircleDot] (((((c \[CircleDot] c) \[CircleDot] 
                (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                 c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
              ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                c))) \[CircleDot] x3)) \[CircleDot] 
           ((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                 c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
             x3) \[CircleDot] (((((c \[CircleDot] c) \[CircleDot] 
                (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                 c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
              ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                c))) \[CircleDot] x3))) \[CircleDot] a == 
         a \[CircleDot] ((((a \[CircleDot] b) \[CircleDot] 
             ((((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                     c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                    (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                    c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                 x3) \[CircleDot] (((((c \[CircleDot] c) \[CircleDot] 
                    (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                     c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                  ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                    c))) \[CircleDot] x3)) \[CircleDot] (
                (((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                     c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                    (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                    c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                 ((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                     c)) \[CircleDot] (c \[CircleDot] c)) \[CircleDot] 
                  ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                    c)))) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
              b)) \[CircleDot] x4) \[CircleDot] 
           (((a \[CircleDot] a) \[CircleDot] a) \[CircleDot] x4))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 8}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CircleDot] 
           (((((a_) \[CircleDot] (b_)) \[CircleDot] ((((c_) \[CircleDot] 
                 (c_)) \[CircleDot] ((c_) \[CircleDot] (c_))) \[CircleDot] (
                b_))) \[CircleDot] (x3_)) \[CircleDot] 
            ((((a_) \[CircleDot] (a_)) \[CircleDot] (a_)) \[CircleDot] 
             (x3_))) -> ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
             c)) \[CircleDot] a, "Side" -> 1, "Subpattern" -> 
         (c_) \[CircleDot] (c_), "MatchingConstruct" -> {"CriticalPairLemma", 
          5}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((a_) \[CircleDot] (b_)) \[CircleDot] 
           ((((((c_) \[CircleDot] (c_)) \[CircleDot] ((c_) \[CircleDot] 
                (c_))) \[CircleDot] (((c_) \[CircleDot] (c_)) \[CircleDot] (
                (c_) \[CircleDot] (c_)))) \[CircleDot] 
             (((c_) \[CircleDot] (c_)) \[CircleDot] ((c_) \[CircleDot] (
                c_)))) \[CircleDot] (b_)) -> 
          (a \[CircleDot] ((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                c)) \[CircleDot] (c \[CircleDot] c)) \[CircleDot] 
             ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
               c)))) \[CircleDot] (c \[CircleDot] c), "MatchingSide" -> 1, 
        "Position" -> {2, 1, 1, 2, 1, 2}|>|>, {"SubstitutionLemma", 9} -> 
     <|"Statement" -> HoldForm[
        (((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                 c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
             x3) \[CircleDot] (((((c \[CircleDot] c) \[CircleDot] 
                (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                 c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
              ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                c))) \[CircleDot] x3)) \[CircleDot] 
           ((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                 c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
             x3) \[CircleDot] (((((c \[CircleDot] c) \[CircleDot] 
                (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                 c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
              ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                c))) \[CircleDot] x3))) \[CircleDot] a == 
         a \[CircleDot] ((((a \[CircleDot] b) \[CircleDot] 
             ((((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                     c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                    (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                    c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                 ((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                     c)) \[CircleDot] (c \[CircleDot] c)) \[CircleDot] 
                  ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                    c)))) \[CircleDot] (c \[CircleDot] c)) \[CircleDot] (
                (((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                     c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                    (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                    c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                 ((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                     c)) \[CircleDot] (c \[CircleDot] c)) \[CircleDot] 
                  ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                    c)))) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
              b)) \[CircleDot] x4) \[CircleDot] 
           (((a \[CircleDot] a) \[CircleDot] a) \[CircleDot] x4))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 7}, 
        "Construct" -> {"CriticalPairLemma", 5}, "Position" -> {2, 1, 1, 2, 
         1, 1}, "Rule" -> ((a_) \[CircleDot] (b_)) \[CircleDot] 
           ((((((c_) \[CircleDot] (c_)) \[CircleDot] ((c_) \[CircleDot] 
                (c_))) \[CircleDot] (((c_) \[CircleDot] (c_)) \[CircleDot] (
                (c_) \[CircleDot] (c_)))) \[CircleDot] 
             (((c_) \[CircleDot] (c_)) \[CircleDot] ((c_) \[CircleDot] (
                c_)))) \[CircleDot] (b_)) -> 
          (a \[CircleDot] ((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                c)) \[CircleDot] (c \[CircleDot] c)) \[CircleDot] 
             ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
               c)))) \[CircleDot] (c \[CircleDot] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[
          (((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                   c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                  (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                  c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
               x3) \[CircleDot] (((((c \[CircleDot] c) \[CircleDot] 
                  (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                   c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                  c))) \[CircleDot] x3)) \[CircleDot] 
             ((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                   c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                  (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                  c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
               x3) \[CircleDot] (((((c \[CircleDot] c) \[CircleDot] 
                  (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                   c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                  c))) \[CircleDot] x3))) \[CircleDot] a == 
           a \[CircleDot] ((((a \[CircleDot] b) \[CircleDot] (
                (((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                       c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                      (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                      c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                   ((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                       c)) \[CircleDot] (c \[CircleDot] c)) \[CircleDot] 
                    ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                      c)))) \[CircleDot] (c \[CircleDot] c)) \[CircleDot] 
                 ((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                       c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                      (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                      c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                   ((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                       c)) \[CircleDot] (c \[CircleDot] c)) \[CircleDot] 
                    ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                      c)))) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                b)) \[CircleDot] x4) \[CircleDot] (((a \[CircleDot] 
                a) \[CircleDot] a) \[CircleDot] x4))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 10} -> 
     <|"Statement" -> HoldForm[
        (((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                 c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
             x3) \[CircleDot] (((((c \[CircleDot] c) \[CircleDot] 
                (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                 c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
              ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                c))) \[CircleDot] x3)) \[CircleDot] 
           ((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                 c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
             x3) \[CircleDot] (((((c \[CircleDot] c) \[CircleDot] 
                (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                 c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
              ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                c))) \[CircleDot] x3))) \[CircleDot] a == 
         a \[CircleDot] ((((a \[CircleDot] b) \[CircleDot] 
             ((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                 c)) \[CircleDot] ((((((c \[CircleDot] c) \[CircleDot] 
                    (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                     c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                  ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                    c))) \[CircleDot] ((((c \[CircleDot] c) \[CircleDot] 
                    (c \[CircleDot] c)) \[CircleDot] (c \[CircleDot] 
                    c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                   (c \[CircleDot] c)))) \[CircleDot] (c \[CircleDot] 
                 c))) \[CircleDot] b)) \[CircleDot] x4) \[CircleDot] 
           (((a \[CircleDot] a) \[CircleDot] a) \[CircleDot] x4))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 9}, 
        "Construct" -> {"CriticalPairLemma", 2}, "Position" -> {2, 1, 1, 2, 
         1, 1, 1}, "Rule" -> (((((a_) \[CircleDot] (a_)) \[CircleDot] 
              ((a_) \[CircleDot] (a_))) \[CircleDot] (((a_) \[CircleDot] (
                a_)) \[CircleDot] (b_))) \[CircleDot] (c_)) \[CircleDot] 
           (((((a_) \[CircleDot] (a_)) \[CircleDot] ((a_) \[CircleDot] (
                a_))) \[CircleDot] ((a_) \[CircleDot] (a_))) \[CircleDot] 
            (c_)) -> b, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[(((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                   c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                  (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                  c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
               x3) \[CircleDot] (((((c \[CircleDot] c) \[CircleDot] 
                  (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                   c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                  c))) \[CircleDot] x3)) \[CircleDot] 
             ((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                   c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                  (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                  c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
               x3) \[CircleDot] (((((c \[CircleDot] c) \[CircleDot] 
                  (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                   c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                  c))) \[CircleDot] x3))) \[CircleDot] a == 
           a \[CircleDot] ((((a \[CircleDot] b) \[CircleDot] (
                (((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                   c)) \[CircleDot] ((((((c \[CircleDot] c) \[CircleDot] 
                      (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                       c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                    ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                      c))) \[CircleDot] ((((c \[CircleDot] c) \[CircleDot] 
                      (c \[CircleDot] c)) \[CircleDot] (c \[CircleDot] 
                      c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                     (c \[CircleDot] c)))) \[CircleDot] (c \[CircleDot] 
                   c))) \[CircleDot] b)) \[CircleDot] x4) \[CircleDot] 
             (((a \[CircleDot] a) \[CircleDot] a) \[CircleDot] x4))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 11} -> 
     <|"Statement" -> HoldForm[
        (((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                 c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
             x3) \[CircleDot] (((((c \[CircleDot] c) \[CircleDot] 
                (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                 c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
              ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                c))) \[CircleDot] x3)) \[CircleDot] 
           ((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                 c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
             x3) \[CircleDot] (((((c \[CircleDot] c) \[CircleDot] 
                (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                 c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
              ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                c))) \[CircleDot] x3))) \[CircleDot] a == 
         a \[CircleDot] ((((a \[CircleDot] b) \[CircleDot] 
             ((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                 c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                (c \[CircleDot] c))) \[CircleDot] b)) \[CircleDot] 
            x4) \[CircleDot] (((a \[CircleDot] a) \[CircleDot] 
             a) \[CircleDot] x4))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 10}, "Construct" -> 
         {"CriticalPairLemma", 2}, "Position" -> {2, 1, 1, 2, 1, 2, 1}, 
        "Rule" -> (((((a_) \[CircleDot] (a_)) \[CircleDot] ((a_) \[CircleDot] 
               (a_))) \[CircleDot] (((a_) \[CircleDot] (a_)) \[CircleDot] 
              (b_))) \[CircleDot] (c_)) \[CircleDot] 
           (((((a_) \[CircleDot] (a_)) \[CircleDot] ((a_) \[CircleDot] (
                a_))) \[CircleDot] ((a_) \[CircleDot] (a_))) \[CircleDot] 
            (c_)) -> b, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[(((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                   c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                  (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                  c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
               x3) \[CircleDot] (((((c \[CircleDot] c) \[CircleDot] 
                  (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                   c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                  c))) \[CircleDot] x3)) \[CircleDot] 
             ((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                   c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                  (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                  c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
               x3) \[CircleDot] (((((c \[CircleDot] c) \[CircleDot] 
                  (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                   c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                  c))) \[CircleDot] x3))) \[CircleDot] a == 
           a \[CircleDot] ((((a \[CircleDot] b) \[CircleDot] (
                (((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                   c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                  (c \[CircleDot] c))) \[CircleDot] b)) \[CircleDot] 
              x4) \[CircleDot] (((a \[CircleDot] a) \[CircleDot] 
               a) \[CircleDot] x4))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 12} -> 
     <|"Statement" -> HoldForm[
        (((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                 c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
             x3) \[CircleDot] (((((c \[CircleDot] c) \[CircleDot] 
                (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                 c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
              ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                c))) \[CircleDot] x3)) \[CircleDot] 
           ((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                 c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
             x3) \[CircleDot] (((((c \[CircleDot] c) \[CircleDot] 
                (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                 c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
              ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                c))) \[CircleDot] x3))) \[CircleDot] a == 
         (((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] c)) \[CircleDot] 
           ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
          a], "Proof" -> <|"Input" -> {"SubstitutionLemma", 11}, 
        "Construct" -> {"SubstitutionLemma", 8}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleDot] (((((a_) \[CircleDot] (b_)) \[CircleDot] 
              ((((c_) \[CircleDot] (c_)) \[CircleDot] ((c_) \[CircleDot] 
                 (c_))) \[CircleDot] (b_))) \[CircleDot] (x3_)) \[CircleDot] 
            ((((a_) \[CircleDot] (a_)) \[CircleDot] (a_)) \[CircleDot] 
             (x3_))) -> ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
             c)) \[CircleDot] a, "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[(((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                   c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                  (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                  c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
               x3) \[CircleDot] (((((c \[CircleDot] c) \[CircleDot] 
                  (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                   c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                  c))) \[CircleDot] x3)) \[CircleDot] 
             ((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                   c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                  (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                  c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
               x3) \[CircleDot] (((((c \[CircleDot] c) \[CircleDot] 
                  (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                   c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                  c))) \[CircleDot] x3))) \[CircleDot] a == 
           (((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] c)) \[CircleDot] 
             ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
               c))) \[CircleDot] a], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 13} -> 
     <|"Statement" -> HoldForm[
        (((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                 c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
             ((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                 c)) \[CircleDot] (c \[CircleDot] c)) \[CircleDot] 
              ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                c)))) \[CircleDot] (c \[CircleDot] c)) \[CircleDot] 
           ((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                 c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
             x3) \[CircleDot] (((((c \[CircleDot] c) \[CircleDot] 
                (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                 c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
              ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                c))) \[CircleDot] x3))) \[CircleDot] a == 
         (((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] c)) \[CircleDot] 
           ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
          a], "Proof" -> <|"Input" -> {"SubstitutionLemma", 12}, 
        "Construct" -> {"CriticalPairLemma", 5}, "Position" -> {1, 1}, 
        "Rule" -> ((a_) \[CircleDot] (b_)) \[CircleDot] 
           ((((((c_) \[CircleDot] (c_)) \[CircleDot] ((c_) \[CircleDot] 
                (c_))) \[CircleDot] (((c_) \[CircleDot] (c_)) \[CircleDot] (
                (c_) \[CircleDot] (c_)))) \[CircleDot] 
             (((c_) \[CircleDot] (c_)) \[CircleDot] ((c_) \[CircleDot] (
                c_)))) \[CircleDot] (b_)) -> 
          (a \[CircleDot] ((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                c)) \[CircleDot] (c \[CircleDot] c)) \[CircleDot] 
             ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
               c)))) \[CircleDot] (c \[CircleDot] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          (((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                   c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                  (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                  c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] (
                (((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                   c)) \[CircleDot] (c \[CircleDot] c)) \[CircleDot] 
                ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                  c)))) \[CircleDot] (c \[CircleDot] c)) \[CircleDot] 
             ((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                   c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                  (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                  c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
               x3) \[CircleDot] (((((c \[CircleDot] c) \[CircleDot] 
                  (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                   c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                  c))) \[CircleDot] x3))) \[CircleDot] a == 
           (((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] c)) \[CircleDot] 
             ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
               c))) \[CircleDot] a], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 14} -> 
     <|"Statement" -> HoldForm[(((c \[CircleDot] c) \[CircleDot] 
            (c \[CircleDot] c)) \[CircleDot] 
           ((((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                 c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
             x3) \[CircleDot] (((((c \[CircleDot] c) \[CircleDot] 
                (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                 c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
              ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                c))) \[CircleDot] x3))) \[CircleDot] a == 
         (((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] c)) \[CircleDot] 
           ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
          a], "Proof" -> <|"Input" -> {"SubstitutionLemma", 13}, 
        "Construct" -> {"CriticalPairLemma", 2}, "Position" -> {1, 1, 1}, 
        "Rule" -> (((((a_) \[CircleDot] (a_)) \[CircleDot] ((a_) \[CircleDot] 
               (a_))) \[CircleDot] (((a_) \[CircleDot] (a_)) \[CircleDot] 
              (b_))) \[CircleDot] (c_)) \[CircleDot] 
           (((((a_) \[CircleDot] (a_)) \[CircleDot] ((a_) \[CircleDot] (
                a_))) \[CircleDot] ((a_) \[CircleDot] (a_))) \[CircleDot] 
            (c_)) -> b, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[(((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
               c)) \[CircleDot] ((((((c \[CircleDot] c) \[CircleDot] 
                  (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
                   c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
                ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                  c))) \[CircleDot] x3) \[CircleDot] 
              (((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
                   c)) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
                  (c \[CircleDot] c))) \[CircleDot] ((c \[CircleDot] 
                  c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
               x3))) \[CircleDot] a == (((c \[CircleDot] c) \[CircleDot] 
              (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
               c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] a], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 15} -> 
     <|"Statement" -> HoldForm[((c \[CircleDot] c) \[CircleDot] 
           (c \[CircleDot] c)) \[CircleDot] a == 
         (((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] c)) \[CircleDot] 
           ((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] 
          a], "Proof" -> <|"Input" -> {"SubstitutionLemma", 14}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {1}, 
        "Rule" -> (a_) \[CircleDot] (((((a_) \[CircleDot] (a_)) \[CircleDot] 
              (b_)) \[CircleDot] (c_)) \[CircleDot] 
            ((((a_) \[CircleDot] (a_)) \[CircleDot] (a_)) \[CircleDot] 
             (c_))) -> b, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
              c)) \[CircleDot] a == (((c \[CircleDot] c) \[CircleDot] 
              (c \[CircleDot] c)) \[CircleDot] ((c \[CircleDot] 
               c) \[CircleDot] (c \[CircleDot] c))) \[CircleDot] a], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 8} -> 
     <|"Statement" -> HoldForm[a \[CircleDot] a == 
         (((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] a)) \[CircleDot] 
           b) \[CircleDot] ((((a \[CircleDot] a) \[CircleDot] 
             (a \[CircleDot] a)) \[CircleDot] (a \[CircleDot] 
             a)) \[CircleDot] b)], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 2}, "Orientation" -> 1, 
        "Rule" -> (((((a_) \[CircleDot] (a_)) \[CircleDot] ((a_) \[CircleDot] 
               (a_))) \[CircleDot] (((a_) \[CircleDot] (a_)) \[CircleDot] 
              (b_))) \[CircleDot] (c_)) \[CircleDot] 
           (((((a_) \[CircleDot] (a_)) \[CircleDot] ((a_) \[CircleDot] (
                a_))) \[CircleDot] ((a_) \[CircleDot] (a_))) \[CircleDot] 
            (c_)) -> b, "Side" -> 1, "Subpattern" -> 
         ((((a_) \[CircleDot] (a_)) \[CircleDot] ((a_) \[CircleDot] 
             (a_))) \[CircleDot] (((a_) \[CircleDot] (a_)) \[CircleDot] 
            (b_))) \[CircleDot] (c_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 15}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((((a_) \[CircleDot] (a_)) \[CircleDot] 
             ((a_) \[CircleDot] (a_))) \[CircleDot] 
            (((a_) \[CircleDot] (a_)) \[CircleDot] ((a_) \[CircleDot] 
              (a_)))) \[CircleDot] (b_) -> ((a \[CircleDot] a) \[CircleDot] 
            (a \[CircleDot] a)) \[CircleDot] b, "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 9} -> 
     <|"Statement" -> HoldForm[((a \[CircleDot] a) \[CircleDot] 
           (a \[CircleDot] a)) \[CircleDot] ((a \[CircleDot] a) \[CircleDot] 
           (a \[CircleDot] a)) == (a \[CircleDot] a) \[CircleDot] 
          ((((((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] 
                a)) \[CircleDot] ((a \[CircleDot] a) \[CircleDot] (
                a \[CircleDot] a))) \[CircleDot] (((a \[CircleDot] 
                a) \[CircleDot] (a \[CircleDot] a)) \[CircleDot] 
              ((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] 
                a)))) \[CircleDot] (((a \[CircleDot] a) \[CircleDot] 
              (a \[CircleDot] a)) \[CircleDot] ((a \[CircleDot] 
               a) \[CircleDot] (a \[CircleDot] a)))) \[CircleDot] 
           ((((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] 
               a)) \[CircleDot] (a \[CircleDot] a)) \[CircleDot] 
            (((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] 
               a)) \[CircleDot] ((a \[CircleDot] a) \[CircleDot] 
              (a \[CircleDot] a)))))], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 8}, "Orientation" -> -1, 
        "Rule" -> ((((a_) \[CircleDot] (a_)) \[CircleDot] ((a_) \[CircleDot] 
              (a_))) \[CircleDot] (b_)) \[CircleDot] 
           (((((a_) \[CircleDot] (a_)) \[CircleDot] ((a_) \[CircleDot] (
                a_))) \[CircleDot] ((a_) \[CircleDot] (a_))) \[CircleDot] 
            (b_)) -> a \[CircleDot] a, "Side" -> 1, "Subpattern" -> 
         (((a_) \[CircleDot] (a_)) \[CircleDot] ((a_) \[CircleDot] 
            (a_))) \[CircleDot] (b_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 2}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (((((a_) \[CircleDot] (a_)) \[CircleDot] 
              ((a_) \[CircleDot] (a_))) \[CircleDot] (((a_) \[CircleDot] (
                a_)) \[CircleDot] (b_))) \[CircleDot] (c_)) \[CircleDot] 
           (((((a_) \[CircleDot] (a_)) \[CircleDot] ((a_) \[CircleDot] (
                a_))) \[CircleDot] ((a_) \[CircleDot] (a_))) \[CircleDot] 
            (c_)) -> b, "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"SubstitutionLemma", 16} -> 
     <|"Statement" -> HoldForm[((a \[CircleDot] a) \[CircleDot] 
           (a \[CircleDot] a)) \[CircleDot] ((a \[CircleDot] a) \[CircleDot] 
           (a \[CircleDot] a)) == (a \[CircleDot] a) \[CircleDot] 
          (((((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] 
               a)) \[CircleDot] ((a \[CircleDot] a) \[CircleDot] 
              (a \[CircleDot] a))) \[CircleDot] 
            (((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] 
               a)) \[CircleDot] ((a \[CircleDot] a) \[CircleDot] 
              (a \[CircleDot] a)))) \[CircleDot] 
           ((((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] 
               a)) \[CircleDot] (a \[CircleDot] a)) \[CircleDot] 
            (((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] 
               a)) \[CircleDot] ((a \[CircleDot] a) \[CircleDot] 
              (a \[CircleDot] a)))))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 9}, "Construct" -> 
         {"SubstitutionLemma", 15}, "Position" -> {2, 1}, 
        "Rule" -> ((((a_) \[CircleDot] (a_)) \[CircleDot] ((a_) \[CircleDot] 
              (a_))) \[CircleDot] (((a_) \[CircleDot] (a_)) \[CircleDot] 
             ((a_) \[CircleDot] (a_)))) \[CircleDot] (b_) -> 
          ((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] a)) \[CircleDot] 
           b, "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] 
              a)) \[CircleDot] ((a \[CircleDot] a) \[CircleDot] 
             (a \[CircleDot] a)) == (a \[CircleDot] a) \[CircleDot] 
            (((((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] 
                 a)) \[CircleDot] ((a \[CircleDot] a) \[CircleDot] 
                (a \[CircleDot] a))) \[CircleDot] (((a \[CircleDot] 
                 a) \[CircleDot] (a \[CircleDot] a)) \[CircleDot] (
                (a \[CircleDot] a) \[CircleDot] (a \[CircleDot] 
                 a)))) \[CircleDot] ((((a \[CircleDot] a) \[CircleDot] 
                (a \[CircleDot] a)) \[CircleDot] (a \[CircleDot] 
                a)) \[CircleDot] (((a \[CircleDot] a) \[CircleDot] 
                (a \[CircleDot] a)) \[CircleDot] ((a \[CircleDot] 
                 a) \[CircleDot] (a \[CircleDot] a)))))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 17} -> 
     <|"Statement" -> HoldForm[((a \[CircleDot] a) \[CircleDot] 
           (a \[CircleDot] a)) \[CircleDot] ((a \[CircleDot] a) \[CircleDot] 
           (a \[CircleDot] a)) == (a \[CircleDot] a) \[CircleDot] 
          (a \[CircleDot] a)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 16}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {}, "Rule" -> (a_) \[CircleDot] 
           (((((a_) \[CircleDot] (a_)) \[CircleDot] (b_)) \[CircleDot] 
             (c_)) \[CircleDot] ((((a_) \[CircleDot] (a_)) \[CircleDot] 
              (a_)) \[CircleDot] (c_))) -> b, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[((a \[CircleDot] a) \[CircleDot] 
             (a \[CircleDot] a)) \[CircleDot] ((a \[CircleDot] 
              a) \[CircleDot] (a \[CircleDot] a)) == 
           (a \[CircleDot] a) \[CircleDot] (a \[CircleDot] a)], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 10} -> 
     <|"Statement" -> HoldForm[(a \[CircleDot] a) \[CircleDot] 
          (a \[CircleDot] a) == (((a \[CircleDot] a) \[CircleDot] 
            (a \[CircleDot] a)) \[CircleDot] b) \[CircleDot] 
          (((((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] 
               a)) \[CircleDot] ((a \[CircleDot] a) \[CircleDot] 
              (a \[CircleDot] a))) \[CircleDot] ((a \[CircleDot] 
              a) \[CircleDot] (a \[CircleDot] a))) \[CircleDot] b)], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 8}, 
        "Orientation" -> -1, "Rule" -> 
         ((((a_) \[CircleDot] (a_)) \[CircleDot] ((a_) \[CircleDot] 
              (a_))) \[CircleDot] (b_)) \[CircleDot] 
           (((((a_) \[CircleDot] (a_)) \[CircleDot] ((a_) \[CircleDot] (
                a_))) \[CircleDot] ((a_) \[CircleDot] (a_))) \[CircleDot] 
            (b_)) -> a \[CircleDot] a, "Side" -> 1, "Subpattern" -> 
         ((a_) \[CircleDot] (a_)) \[CircleDot] ((a_) \[CircleDot] (a_)), 
        "MatchingConstruct" -> {"SubstitutionLemma", 17}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (((a_) \[CircleDot] (a_)) \[CircleDot] ((a_) \[CircleDot] 
             (a_))) \[CircleDot] (((a_) \[CircleDot] (a_)) \[CircleDot] 
            ((a_) \[CircleDot] (a_))) -> (a \[CircleDot] a) \[CircleDot] 
           (a \[CircleDot] a), "MatchingSide" -> 1, "Position" -> {1, 1}|>|>, 
    {"SubstitutionLemma", 18} -> 
     <|"Statement" -> HoldForm[(a \[CircleDot] a) \[CircleDot] 
          (a \[CircleDot] a) == (((a \[CircleDot] a) \[CircleDot] 
            (a \[CircleDot] a)) \[CircleDot] b) \[CircleDot] 
          ((((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] a)) \[CircleDot] 
            ((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] 
              a))) \[CircleDot] b)], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 10}, "Construct" -> 
         {"SubstitutionLemma", 17}, "Position" -> {2, 1, 1}, 
        "Rule" -> (((a_) \[CircleDot] (a_)) \[CircleDot] ((a_) \[CircleDot] 
             (a_))) \[CircleDot] (((a_) \[CircleDot] (a_)) \[CircleDot] 
            ((a_) \[CircleDot] (a_))) -> (a \[CircleDot] a) \[CircleDot] 
           (a \[CircleDot] a), "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[(a \[CircleDot] a) \[CircleDot] (a \[CircleDot] a) == 
           (((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] a)) \[CircleDot] 
             b) \[CircleDot] ((((a \[CircleDot] a) \[CircleDot] (
                a \[CircleDot] a)) \[CircleDot] ((a \[CircleDot] 
                a) \[CircleDot] (a \[CircleDot] a))) \[CircleDot] b)], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 19} -> 
     <|"Statement" -> HoldForm[(a \[CircleDot] a) \[CircleDot] 
          (a \[CircleDot] a) == (((a \[CircleDot] a) \[CircleDot] 
            (a \[CircleDot] a)) \[CircleDot] b) \[CircleDot] 
          (((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] a)) \[CircleDot] 
           b)], "Proof" -> <|"Input" -> {"SubstitutionLemma", 18}, 
        "Construct" -> {"SubstitutionLemma", 17}, "Position" -> {2, 1}, 
        "Rule" -> (((a_) \[CircleDot] (a_)) \[CircleDot] ((a_) \[CircleDot] 
             (a_))) \[CircleDot] (((a_) \[CircleDot] (a_)) \[CircleDot] 
            ((a_) \[CircleDot] (a_))) -> (a \[CircleDot] a) \[CircleDot] 
           (a \[CircleDot] a), "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[(a \[CircleDot] a) \[CircleDot] (a \[CircleDot] a) == 
           (((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] a)) \[CircleDot] 
             b) \[CircleDot] (((a \[CircleDot] a) \[CircleDot] 
              (a \[CircleDot] a)) \[CircleDot] b)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 11} -> 
     <|"Statement" -> HoldForm[(b \[CircleDot] b) \[CircleDot] 
          (b \[CircleDot] b) == a \[CircleDot] 
          (((b \[CircleDot] b) \[CircleDot] (b \[CircleDot] b)) \[CircleDot] 
           ((((((b \[CircleDot] b) \[CircleDot] (b \[CircleDot] 
                 b)) \[CircleDot] ((b \[CircleDot] b) \[CircleDot] 
                (b \[CircleDot] b))) \[CircleDot] a) \[CircleDot] 
             c) \[CircleDot] (((((b \[CircleDot] b) \[CircleDot] 
                (b \[CircleDot] b)) \[CircleDot] ((b \[CircleDot] 
                 b) \[CircleDot] (b \[CircleDot] b))) \[CircleDot] 
              ((b \[CircleDot] b) \[CircleDot] (b \[CircleDot] 
                b))) \[CircleDot] c)))], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 19}, "Orientation" -> -1, 
        "Rule" -> ((((a_) \[CircleDot] (a_)) \[CircleDot] ((a_) \[CircleDot] 
              (a_))) \[CircleDot] (b_)) \[CircleDot] 
           ((((a_) \[CircleDot] (a_)) \[CircleDot] ((a_) \[CircleDot] 
              (a_))) \[CircleDot] (b_)) -> (a \[CircleDot] a) \[CircleDot] 
           (a \[CircleDot] a), "Side" -> 1, "Subpattern" -> 
         (((a_) \[CircleDot] (a_)) \[CircleDot] ((a_) \[CircleDot] 
            (a_))) \[CircleDot] (b_), "MatchingConstruct" -> {"Axiom", 1}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleDot] (((((a_) \[CircleDot] (a_)) \[CircleDot] 
              (b_)) \[CircleDot] (c_)) \[CircleDot] 
            ((((a_) \[CircleDot] (a_)) \[CircleDot] (a_)) \[CircleDot] 
             (c_))) -> b, "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"SubstitutionLemma", 20} -> 
     <|"Statement" -> HoldForm[(b \[CircleDot] b) \[CircleDot] 
          (b \[CircleDot] b) == a \[CircleDot] a], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 11}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {2}, 
        "Rule" -> (a_) \[CircleDot] (((((a_) \[CircleDot] (a_)) \[CircleDot] 
              (b_)) \[CircleDot] (c_)) \[CircleDot] 
            ((((a_) \[CircleDot] (a_)) \[CircleDot] (a_)) \[CircleDot] 
             (c_))) -> b, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[(b \[CircleDot] b) \[CircleDot] (b \[CircleDot] b) == 
           a \[CircleDot] a], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 12} -> 
     <|"Statement" -> HoldForm[a == a \[CircleDot] 
          ((b \[CircleDot] b) \[CircleDot] (b \[CircleDot] b))], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CircleDot] (((((a_) \[CircleDot] (a_)) \[CircleDot] 
              (b_)) \[CircleDot] (c_)) \[CircleDot] 
            ((((a_) \[CircleDot] (a_)) \[CircleDot] (a_)) \[CircleDot] 
             (c_))) -> b, "Side" -> 1, "Subpattern" -> 
         ((((a_) \[CircleDot] (a_)) \[CircleDot] (b_)) \[CircleDot] 
           (c_)) \[CircleDot] ((((a_) \[CircleDot] (a_)) \[CircleDot] 
            (a_)) \[CircleDot] (c_)), "MatchingConstruct" -> 
         {"SubstitutionLemma", 20}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CircleDot] (a_) -> 
          (b \[CircleDot] b) \[CircleDot] (b \[CircleDot] b), 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 13} -> 
     <|"Statement" -> HoldForm[a == a \[CircleDot] 
          ((b \[CircleDot] b) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
            (c \[CircleDot] c)))], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 12}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CircleDot] (((b_) \[CircleDot] (b_)) \[CircleDot] 
            ((b_) \[CircleDot] (b_))) -> a, "Side" -> 1, 
        "Subpattern" -> (b_) \[CircleDot] (b_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 20}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CircleDot] (a_) -> 
          (b \[CircleDot] b) \[CircleDot] (b \[CircleDot] b), 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"SubstitutionLemma", 21} -> 
     <|"Statement" -> HoldForm[a == a \[CircleDot] (b \[CircleDot] b)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 13}, 
        "Construct" -> {"CriticalPairLemma", 12}, "Position" -> {2}, 
        "Rule" -> (a_) \[CircleDot] (((b_) \[CircleDot] (b_)) \[CircleDot] 
            ((b_) \[CircleDot] (b_))) -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a == a \[CircleDot] 
            (b \[CircleDot] b)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 14} -> 
     <|"Statement" -> HoldForm[b == a \[CircleDot] 
          (((a \[CircleDot] a) \[CircleDot] b) \[CircleDot] 
           ((a \[CircleDot] a) \[CircleDot] a))], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CircleDot] (((((a_) \[CircleDot] (a_)) \[CircleDot] 
              (b_)) \[CircleDot] (c_)) \[CircleDot] 
            ((((a_) \[CircleDot] (a_)) \[CircleDot] (a_)) \[CircleDot] 
             (c_))) -> b, "Side" -> 1, "Subpattern" -> 
         ((((a_) \[CircleDot] (a_)) \[CircleDot] (b_)) \[CircleDot] 
           (c_)) \[CircleDot] ((((a_) \[CircleDot] (a_)) \[CircleDot] 
            (a_)) \[CircleDot] (c_)), "MatchingConstruct" -> 
         {"SubstitutionLemma", 21}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CircleDot] ((b_) \[CircleDot] (b_)) -> a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 15} -> 
     <|"Statement" -> HoldForm[(((a \[CircleDot] a) \[CircleDot] 
            (a \[CircleDot] a)) \[CircleDot] b) \[CircleDot] 
          (((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] a)) \[CircleDot] 
           (a \[CircleDot] a)) == a \[CircleDot] (b \[CircleDot] 
           ((a \[CircleDot] a) \[CircleDot] a))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 14}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CircleDot] 
           ((((a_) \[CircleDot] (a_)) \[CircleDot] (b_)) \[CircleDot] 
            (((a_) \[CircleDot] (a_)) \[CircleDot] (a_))) -> b, "Side" -> 1, 
        "Subpattern" -> ((a_) \[CircleDot] (a_)) \[CircleDot] (b_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 14}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CircleDot] ((((a_) \[CircleDot] (a_)) \[CircleDot] 
             (b_)) \[CircleDot] (((a_) \[CircleDot] (a_)) \[CircleDot] 
             (a_))) -> b, "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"SubstitutionLemma", 22} -> 
     <|"Statement" -> HoldForm[((a \[CircleDot] a) \[CircleDot] 
           b) \[CircleDot] (((a \[CircleDot] a) \[CircleDot] 
            (a \[CircleDot] a)) \[CircleDot] (a \[CircleDot] a)) == 
         a \[CircleDot] (b \[CircleDot] ((a \[CircleDot] a) \[CircleDot] 
            a))], "Proof" -> <|"Input" -> {"CriticalPairLemma", 15}, 
        "Construct" -> {"SubstitutionLemma", 21}, "Position" -> {1, 1}, 
        "Rule" -> (a_) \[CircleDot] ((b_) \[CircleDot] (b_)) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          ((a \[CircleDot] a) \[CircleDot] b) \[CircleDot] 
            (((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] 
               a)) \[CircleDot] (a \[CircleDot] a)) == a \[CircleDot] 
            (b \[CircleDot] ((a \[CircleDot] a) \[CircleDot] a))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 23} -> 
     <|"Statement" -> HoldForm[((a \[CircleDot] a) \[CircleDot] 
           b) \[CircleDot] ((a \[CircleDot] a) \[CircleDot] 
           (a \[CircleDot] a)) == a \[CircleDot] (b \[CircleDot] 
           ((a \[CircleDot] a) \[CircleDot] a))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 22}, 
        "Construct" -> {"SubstitutionLemma", 21}, "Position" -> {2}, 
        "Rule" -> (a_) \[CircleDot] ((b_) \[CircleDot] (b_)) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          ((a \[CircleDot] a) \[CircleDot] b) \[CircleDot] 
            ((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] a)) == 
           a \[CircleDot] (b \[CircleDot] ((a \[CircleDot] a) \[CircleDot] 
              a))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 24} -> 
     <|"Statement" -> HoldForm[(a \[CircleDot] a) \[CircleDot] b == 
         a \[CircleDot] (b \[CircleDot] ((a \[CircleDot] a) \[CircleDot] 
            a))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 23}, 
        "Construct" -> {"SubstitutionLemma", 21}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleDot] ((b_) \[CircleDot] (b_)) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (a \[CircleDot] a) \[CircleDot] b == a \[CircleDot] 
            (b \[CircleDot] ((a \[CircleDot] a) \[CircleDot] a))], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 16} -> 
     <|"Statement" -> HoldForm[(a \[CircleDot] a) \[CircleDot] b == 
         a \[CircleDot] (b \[CircleDot] (((c \[CircleDot] c) \[CircleDot] 
             (c \[CircleDot] c)) \[CircleDot] a))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 24}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CircleDot] ((b_) \[CircleDot] 
            (((a_) \[CircleDot] (a_)) \[CircleDot] (a_))) -> 
          (a \[CircleDot] a) \[CircleDot] b, "Side" -> 1, 
        "Subpattern" -> (a_) \[CircleDot] (a_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 20}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CircleDot] (a_) -> 
          (b \[CircleDot] b) \[CircleDot] (b \[CircleDot] b), 
        "MatchingSide" -> 1, "Position" -> {2, 2, 1}|>|>, 
    {"SubstitutionLemma", 25} -> 
     <|"Statement" -> HoldForm[(a \[CircleDot] a) \[CircleDot] b == 
         a \[CircleDot] (b \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
            a))], "Proof" -> <|"Input" -> {"CriticalPairLemma", 16}, 
        "Construct" -> {"SubstitutionLemma", 21}, "Position" -> {2, 2, 1}, 
        "Rule" -> (a_) \[CircleDot] ((b_) \[CircleDot] (b_)) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (a \[CircleDot] a) \[CircleDot] b == a \[CircleDot] 
            (b \[CircleDot] ((c \[CircleDot] c) \[CircleDot] a))], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 17} -> 
     <|"Statement" -> HoldForm[b == (a \[CircleDot] a) \[CircleDot] 
          (((a \[CircleDot] a) \[CircleDot] b) \[CircleDot] 
           (((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] a)) \[CircleDot] 
            (a \[CircleDot] a)))], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 14}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CircleDot] ((((a_) \[CircleDot] (a_)) \[CircleDot] 
             (b_)) \[CircleDot] (((a_) \[CircleDot] (a_)) \[CircleDot] 
             (a_))) -> b, "Side" -> 1, "Subpattern" -> (a_) \[CircleDot] 
          (a_), "MatchingConstruct" -> {"SubstitutionLemma", 21}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CircleDot] ((b_) \[CircleDot] (b_)) -> a, 
        "MatchingSide" -> 1, "Position" -> {2, 1, 1}|>|>, 
    {"SubstitutionLemma", 26} -> 
     <|"Statement" -> HoldForm[b == (a \[CircleDot] a) \[CircleDot] 
          (((a \[CircleDot] a) \[CircleDot] b) \[CircleDot] 
           ((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] a)))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 17}, 
        "Construct" -> {"SubstitutionLemma", 21}, "Position" -> {2, 2}, 
        "Rule" -> (a_) \[CircleDot] ((b_) \[CircleDot] (b_)) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          b == (a \[CircleDot] a) \[CircleDot] 
            (((a \[CircleDot] a) \[CircleDot] b) \[CircleDot] 
             ((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] a)))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 27} -> 
     <|"Statement" -> HoldForm[b == (a \[CircleDot] a) \[CircleDot] 
          ((a \[CircleDot] a) \[CircleDot] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 26}, 
        "Construct" -> {"SubstitutionLemma", 21}, "Position" -> {2}, 
        "Rule" -> (a_) \[CircleDot] ((b_) \[CircleDot] (b_)) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          b == (a \[CircleDot] a) \[CircleDot] ((a \[CircleDot] 
              a) \[CircleDot] b)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 18} -> 
     <|"Statement" -> HoldForm[c == ((a \[CircleDot] a) \[CircleDot] 
           (a \[CircleDot] a)) \[CircleDot] ((b \[CircleDot] b) \[CircleDot] 
           c)], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 27}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CircleDot] (a_)) \[CircleDot] 
           (((a_) \[CircleDot] (a_)) \[CircleDot] (b_)) -> b, "Side" -> 1, 
        "Subpattern" -> (a_) \[CircleDot] (a_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 20}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CircleDot] (a_) -> 
          (b \[CircleDot] b) \[CircleDot] (b \[CircleDot] b), 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"SubstitutionLemma", 28} -> 
     <|"Statement" -> HoldForm[c == (a \[CircleDot] a) \[CircleDot] 
          ((b \[CircleDot] b) \[CircleDot] c)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 18}, 
        "Construct" -> {"SubstitutionLemma", 21}, "Position" -> {1}, 
        "Rule" -> (a_) \[CircleDot] ((b_) \[CircleDot] (b_)) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          c == (a \[CircleDot] a) \[CircleDot] ((b \[CircleDot] 
              b) \[CircleDot] c)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 19} -> 
     <|"Statement" -> HoldForm[(((a \[CircleDot] a) \[CircleDot] 
            b) \[CircleDot] ((a \[CircleDot] a) \[CircleDot] b)) \[CircleDot] 
          c == ((a \[CircleDot] a) \[CircleDot] b) \[CircleDot] 
          (c \[CircleDot] b)], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 25}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CircleDot] ((b_) \[CircleDot] 
            (((c_) \[CircleDot] (c_)) \[CircleDot] (a_))) -> 
          (a \[CircleDot] a) \[CircleDot] b, "Side" -> 1, 
        "Subpattern" -> ((c_) \[CircleDot] (c_)) \[CircleDot] (a_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 28}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CircleDot] (a_)) \[CircleDot] (((b_) \[CircleDot] 
             (b_)) \[CircleDot] (c_)) -> c, "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"SubstitutionLemma", 29} -> 
     <|"Statement" -> HoldForm[a \[CircleDot] a == 
         ((a \[CircleDot] a) \[CircleDot] b) \[CircleDot] 
          ((((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] a)) \[CircleDot] 
            (a \[CircleDot] a)) \[CircleDot] b)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 8}, 
        "Construct" -> {"SubstitutionLemma", 21}, "Position" -> {1, 1}, 
        "Rule" -> (a_) \[CircleDot] ((b_) \[CircleDot] (b_)) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[a \[CircleDot] a == 
           ((a \[CircleDot] a) \[CircleDot] b) \[CircleDot] 
            ((((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] 
                a)) \[CircleDot] (a \[CircleDot] a)) \[CircleDot] b)], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 30} -> 
     <|"Statement" -> HoldForm[a \[CircleDot] a == 
         ((a \[CircleDot] a) \[CircleDot] b) \[CircleDot] 
          (((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] a)) \[CircleDot] 
           b)], "Proof" -> <|"Input" -> {"SubstitutionLemma", 29}, 
        "Construct" -> {"SubstitutionLemma", 21}, "Position" -> {2, 1}, 
        "Rule" -> (a_) \[CircleDot] ((b_) \[CircleDot] (b_)) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[a \[CircleDot] a == 
           ((a \[CircleDot] a) \[CircleDot] b) \[CircleDot] 
            (((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] 
               a)) \[CircleDot] b)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 31} -> 
     <|"Statement" -> HoldForm[a \[CircleDot] a == 
         ((a \[CircleDot] a) \[CircleDot] b) \[CircleDot] 
          ((a \[CircleDot] a) \[CircleDot] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 30}, 
        "Construct" -> {"SubstitutionLemma", 21}, "Position" -> {2, 1}, 
        "Rule" -> (a_) \[CircleDot] ((b_) \[CircleDot] (b_)) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[a \[CircleDot] a == 
           ((a \[CircleDot] a) \[CircleDot] b) \[CircleDot] 
            ((a \[CircleDot] a) \[CircleDot] b)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 20} -> 
     <|"Statement" -> HoldForm[c \[CircleDot] c == 
         (((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] a)) \[CircleDot] 
           b) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] b)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 31}, 
        "Orientation" -> -1, "Rule" -> (((a_) \[CircleDot] (a_)) \[CircleDot] 
            (b_)) \[CircleDot] (((a_) \[CircleDot] (a_)) \[CircleDot] 
            (b_)) -> a \[CircleDot] a, "Side" -> 1, "Subpattern" -> 
         (a_) \[CircleDot] (a_), "MatchingConstruct" -> {"SubstitutionLemma", 
          20}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CircleDot] (a_) -> (b \[CircleDot] b) \[CircleDot] 
           (b \[CircleDot] b), "MatchingSide" -> 1, "Position" -> {1, 1}|>|>, 
    {"SubstitutionLemma", 32} -> 
     <|"Statement" -> HoldForm[c \[CircleDot] c == 
         ((a \[CircleDot] a) \[CircleDot] b) \[CircleDot] 
          ((c \[CircleDot] c) \[CircleDot] b)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 20}, 
        "Construct" -> {"SubstitutionLemma", 21}, "Position" -> {1, 1}, 
        "Rule" -> (a_) \[CircleDot] ((b_) \[CircleDot] (b_)) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[c \[CircleDot] c == 
           ((a \[CircleDot] a) \[CircleDot] b) \[CircleDot] 
            ((c \[CircleDot] c) \[CircleDot] b)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 33} -> 
     <|"Statement" -> HoldForm[(a \[CircleDot] a) \[CircleDot] c == 
         ((a \[CircleDot] a) \[CircleDot] b) \[CircleDot] 
          (c \[CircleDot] b)], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 19}, "Construct" -> 
         {"SubstitutionLemma", 32}, "Position" -> {1}, 
        "Rule" -> (((a_) \[CircleDot] (a_)) \[CircleDot] (b_)) \[CircleDot] 
           (((c_) \[CircleDot] (c_)) \[CircleDot] (b_)) -> c \[CircleDot] c, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (a \[CircleDot] a) \[CircleDot] c == 
           ((a \[CircleDot] a) \[CircleDot] b) \[CircleDot] 
            (c \[CircleDot] b)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 21} -> 
     <|"Statement" -> HoldForm[
        ((((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] a)) \[CircleDot] 
            b) \[CircleDot] c) \[CircleDot] 
          ((((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] a)) \[CircleDot] 
            (a \[CircleDot] a)) \[CircleDot] c) == a \[CircleDot] 
          (b \[CircleDot] ((a \[CircleDot] a) \[CircleDot] a))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 1}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CircleDot] 
           (((b_) \[CircleDot] (c_)) \[CircleDot] 
            ((((a_) \[CircleDot] (a_)) \[CircleDot] (a_)) \[CircleDot] 
             (c_))) -> ((((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] 
               a)) \[CircleDot] b) \[CircleDot] x3) \[CircleDot] 
           ((((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] 
               a)) \[CircleDot] (a \[CircleDot] a)) \[CircleDot] x3), 
        "Side" -> 1, "Subpattern" -> ((b_) \[CircleDot] (c_)) \[CircleDot] 
          ((((a_) \[CircleDot] (a_)) \[CircleDot] (a_)) \[CircleDot] (c_)), 
        "MatchingConstruct" -> {"SubstitutionLemma", 21}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CircleDot] ((b_) \[CircleDot] (b_)) -> a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 34} -> 
     <|"Statement" -> HoldForm[
        ((((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] a)) \[CircleDot] 
            b) \[CircleDot] c) \[CircleDot] 
          ((((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] a)) \[CircleDot] 
            (a \[CircleDot] a)) \[CircleDot] c) == 
         (a \[CircleDot] a) \[CircleDot] b], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 21}, 
        "Construct" -> {"SubstitutionLemma", 25}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleDot] ((b_) \[CircleDot] 
            (((c_) \[CircleDot] (c_)) \[CircleDot] (a_))) -> 
          (a \[CircleDot] a) \[CircleDot] b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[
          ((((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] a)) \[CircleDot] 
              b) \[CircleDot] c) \[CircleDot] 
            ((((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] 
                a)) \[CircleDot] (a \[CircleDot] a)) \[CircleDot] c) == 
           (a \[CircleDot] a) \[CircleDot] b], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 35} -> 
     <|"Statement" -> HoldForm[(((a \[CircleDot] a) \[CircleDot] 
            b) \[CircleDot] c) \[CircleDot] 
          ((((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] a)) \[CircleDot] 
            (a \[CircleDot] a)) \[CircleDot] c) == 
         (a \[CircleDot] a) \[CircleDot] b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 34}, 
        "Construct" -> {"SubstitutionLemma", 21}, "Position" -> {1, 1, 1}, 
        "Rule" -> (a_) \[CircleDot] ((b_) \[CircleDot] (b_)) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (((a \[CircleDot] a) \[CircleDot] b) \[CircleDot] c) \[CircleDot] 
            ((((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] 
                a)) \[CircleDot] (a \[CircleDot] a)) \[CircleDot] c) == 
           (a \[CircleDot] a) \[CircleDot] b], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 36} -> 
     <|"Statement" -> HoldForm[(((a \[CircleDot] a) \[CircleDot] 
            b) \[CircleDot] c) \[CircleDot] (((a \[CircleDot] a) \[CircleDot] 
            (a \[CircleDot] a)) \[CircleDot] c) == 
         (a \[CircleDot] a) \[CircleDot] b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 35}, 
        "Construct" -> {"SubstitutionLemma", 21}, "Position" -> {2, 1}, 
        "Rule" -> (a_) \[CircleDot] ((b_) \[CircleDot] (b_)) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (((a \[CircleDot] a) \[CircleDot] b) \[CircleDot] c) \[CircleDot] 
            (((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] 
               a)) \[CircleDot] c) == (a \[CircleDot] a) \[CircleDot] b], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 37} -> 
     <|"Statement" -> HoldForm[(((a \[CircleDot] a) \[CircleDot] 
            b) \[CircleDot] c) \[CircleDot] ((a \[CircleDot] a) \[CircleDot] 
           c) == (a \[CircleDot] a) \[CircleDot] b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 36}, 
        "Construct" -> {"SubstitutionLemma", 21}, "Position" -> {2, 1}, 
        "Rule" -> (a_) \[CircleDot] ((b_) \[CircleDot] (b_)) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (((a \[CircleDot] a) \[CircleDot] b) \[CircleDot] c) \[CircleDot] 
            ((a \[CircleDot] a) \[CircleDot] c) == 
           (a \[CircleDot] a) \[CircleDot] b], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 22} -> 
     <|"Statement" -> HoldForm[(c \[CircleDot] c) \[CircleDot] 
          (((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
               c)) \[CircleDot] a) \[CircleDot] x3) \[CircleDot] 
           ((((c \[CircleDot] c) \[CircleDot] (c \[CircleDot] 
               c)) \[CircleDot] (c \[CircleDot] c)) \[CircleDot] x3)) == 
         (a \[CircleDot] b) \[CircleDot] ((c \[CircleDot] c) \[CircleDot] 
           b)], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 37}, 
        "Orientation" -> 1, "Rule" -> 
         ((((a_) \[CircleDot] (a_)) \[CircleDot] (b_)) \[CircleDot] 
            (c_)) \[CircleDot] (((a_) \[CircleDot] (a_)) \[CircleDot] 
            (c_)) -> (a \[CircleDot] a) \[CircleDot] b, "Side" -> 1, 
        "Subpattern" -> ((a_) \[CircleDot] (a_)) \[CircleDot] (b_), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CircleDot] 
           (((((a_) \[CircleDot] (a_)) \[CircleDot] (b_)) \[CircleDot] 
             (c_)) \[CircleDot] ((((a_) \[CircleDot] (a_)) \[CircleDot] 
              (a_)) \[CircleDot] (c_))) -> b, "MatchingSide" -> 1, 
        "Position" -> {1, 1}|>|>, {"SubstitutionLemma", 38} -> 
     <|"Statement" -> HoldForm[a == (a \[CircleDot] b) \[CircleDot] 
          ((c \[CircleDot] c) \[CircleDot] b)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 22}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleDot] (((((a_) \[CircleDot] (a_)) \[CircleDot] 
              (b_)) \[CircleDot] (c_)) \[CircleDot] 
            ((((a_) \[CircleDot] (a_)) \[CircleDot] (a_)) \[CircleDot] 
             (c_))) -> b, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[a == (a \[CircleDot] b) \[CircleDot] 
            ((c \[CircleDot] c) \[CircleDot] b)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 23} -> 
     <|"Statement" -> HoldForm[(a \[CircleDot] a) \[CircleDot] 
          (x3 \[CircleDot] c) == ((a \[CircleDot] a) \[CircleDot] 
           ((b \[CircleDot] b) \[CircleDot] c)) \[CircleDot] x3], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 33}, 
        "Orientation" -> -1, "Rule" -> (((a_) \[CircleDot] (a_)) \[CircleDot] 
            (b_)) \[CircleDot] ((c_) \[CircleDot] (b_)) -> 
          (a \[CircleDot] a) \[CircleDot] c, "Side" -> 1, 
        "Subpattern" -> (c_) \[CircleDot] (b_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 38}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CircleDot] (b_)) \[CircleDot] 
           (((c_) \[CircleDot] (c_)) \[CircleDot] (b_)) -> a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 39} -> 
     <|"Statement" -> HoldForm[(a \[CircleDot] a) \[CircleDot] 
          (x3 \[CircleDot] c) == c \[CircleDot] x3], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 23}, 
        "Construct" -> {"SubstitutionLemma", 28}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CircleDot] (a_)) \[CircleDot] 
           (((b_) \[CircleDot] (b_)) \[CircleDot] (c_)) -> c, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (a \[CircleDot] a) \[CircleDot] (x3 \[CircleDot] c) == 
           c \[CircleDot] x3], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 24} -> 
     <|"Statement" -> HoldForm[((c \[CircleDot] x4) \[CircleDot] 
           (((b \[CircleDot] b) \[CircleDot] b) \[CircleDot] 
            x4)) \[CircleDot] b == (a \[CircleDot] a) \[CircleDot] 
          (((((b \[CircleDot] b) \[CircleDot] (b \[CircleDot] 
               b)) \[CircleDot] c) \[CircleDot] x3) \[CircleDot] 
           ((((b \[CircleDot] b) \[CircleDot] (b \[CircleDot] 
               b)) \[CircleDot] (b \[CircleDot] b)) \[CircleDot] x3))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 39}, 
        "Orientation" -> 1, "Rule" -> ((a_) \[CircleDot] (a_)) \[CircleDot] 
           ((b_) \[CircleDot] (c_)) -> c \[CircleDot] b, "Side" -> 1, 
        "Subpattern" -> (b_) \[CircleDot] (c_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 1}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CircleDot] (((b_) \[CircleDot] 
             (c_)) \[CircleDot] ((((a_) \[CircleDot] (a_)) \[CircleDot] 
              (a_)) \[CircleDot] (c_))) -> 
          ((((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] a)) \[CircleDot] 
             b) \[CircleDot] x3) \[CircleDot] 
           ((((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] 
               a)) \[CircleDot] (a \[CircleDot] a)) \[CircleDot] x3), 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 41} -> 
     <|"Statement" -> HoldForm[((c \[CircleDot] x4) \[CircleDot] 
           (((b \[CircleDot] b) \[CircleDot] b) \[CircleDot] 
            x4)) \[CircleDot] b == ((((b \[CircleDot] b) \[CircleDot] 
             (b \[CircleDot] b)) \[CircleDot] (b \[CircleDot] 
             b)) \[CircleDot] x3) \[CircleDot] 
          ((((b \[CircleDot] b) \[CircleDot] (b \[CircleDot] b)) \[CircleDot] 
            c) \[CircleDot] x3)], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 24}, "Construct" -> 
         {"SubstitutionLemma", 39}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleDot] (a_)) \[CircleDot] ((b_) \[CircleDot] 
            (c_)) -> c \[CircleDot] b, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[((c \[CircleDot] x4) \[CircleDot] 
             (((b \[CircleDot] b) \[CircleDot] b) \[CircleDot] 
              x4)) \[CircleDot] b == ((((b \[CircleDot] b) \[CircleDot] (
                b \[CircleDot] b)) \[CircleDot] (b \[CircleDot] 
               b)) \[CircleDot] x3) \[CircleDot] 
            ((((b \[CircleDot] b) \[CircleDot] (b \[CircleDot] 
                b)) \[CircleDot] c) \[CircleDot] x3)], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 42} -> 
     <|"Statement" -> HoldForm[((c \[CircleDot] x4) \[CircleDot] 
           (((b \[CircleDot] b) \[CircleDot] b) \[CircleDot] 
            x4)) \[CircleDot] b == (((b \[CircleDot] b) \[CircleDot] 
            (b \[CircleDot] b)) \[CircleDot] x3) \[CircleDot] 
          ((((b \[CircleDot] b) \[CircleDot] (b \[CircleDot] b)) \[CircleDot] 
            c) \[CircleDot] x3)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 41}, "Construct" -> 
         {"SubstitutionLemma", 21}, "Position" -> {1, 1}, 
        "Rule" -> (a_) \[CircleDot] ((b_) \[CircleDot] (b_)) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          ((c \[CircleDot] x4) \[CircleDot] (((b \[CircleDot] b) \[CircleDot] 
               b) \[CircleDot] x4)) \[CircleDot] b == 
           (((b \[CircleDot] b) \[CircleDot] (b \[CircleDot] b)) \[CircleDot] 
             x3) \[CircleDot] ((((b \[CircleDot] b) \[CircleDot] (
                b \[CircleDot] b)) \[CircleDot] c) \[CircleDot] x3)], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 43} -> 
     <|"Statement" -> HoldForm[((c \[CircleDot] x4) \[CircleDot] 
           (((b \[CircleDot] b) \[CircleDot] b) \[CircleDot] 
            x4)) \[CircleDot] b == ((b \[CircleDot] b) \[CircleDot] 
           (b \[CircleDot] b)) \[CircleDot] (((b \[CircleDot] b) \[CircleDot] 
            (b \[CircleDot] b)) \[CircleDot] c)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 42}, 
        "Construct" -> {"SubstitutionLemma", 33}, "Position" -> {}, 
        "Rule" -> (((a_) \[CircleDot] (a_)) \[CircleDot] (b_)) \[CircleDot] 
           ((c_) \[CircleDot] (b_)) -> (a \[CircleDot] a) \[CircleDot] c, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          ((c \[CircleDot] x4) \[CircleDot] (((b \[CircleDot] b) \[CircleDot] 
               b) \[CircleDot] x4)) \[CircleDot] b == 
           ((b \[CircleDot] b) \[CircleDot] (b \[CircleDot] b)) \[CircleDot] 
            (((b \[CircleDot] b) \[CircleDot] (b \[CircleDot] 
               b)) \[CircleDot] c)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 44} -> 
     <|"Statement" -> HoldForm[((c \[CircleDot] x4) \[CircleDot] 
           (((b \[CircleDot] b) \[CircleDot] b) \[CircleDot] 
            x4)) \[CircleDot] b == c \[CircleDot] 
          ((b \[CircleDot] b) \[CircleDot] (b \[CircleDot] b))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 43}, 
        "Construct" -> {"SubstitutionLemma", 39}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleDot] (a_)) \[CircleDot] ((b_) \[CircleDot] 
            (c_)) -> c \[CircleDot] b, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[((c \[CircleDot] x4) \[CircleDot] 
             (((b \[CircleDot] b) \[CircleDot] b) \[CircleDot] 
              x4)) \[CircleDot] b == c \[CircleDot] 
            ((b \[CircleDot] b) \[CircleDot] (b \[CircleDot] b))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 45} -> 
     <|"Statement" -> HoldForm[((c \[CircleDot] x4) \[CircleDot] 
           (((b \[CircleDot] b) \[CircleDot] b) \[CircleDot] 
            x4)) \[CircleDot] b == c], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 44}, "Construct" -> 
         {"SubstitutionLemma", 21}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleDot] ((b_) \[CircleDot] (b_)) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          ((c \[CircleDot] x4) \[CircleDot] (((b \[CircleDot] b) \[CircleDot] 
               b) \[CircleDot] x4)) \[CircleDot] b == c], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 25} -> 
     <|"Statement" -> HoldForm[(a \[CircleDot] a) \[CircleDot] 
          ((b \[CircleDot] c) \[CircleDot] 
           (((((x3 \[CircleDot] x3) \[CircleDot] a) \[CircleDot] 
              ((x3 \[CircleDot] x3) \[CircleDot] a)) \[CircleDot] 
             ((x3 \[CircleDot] x3) \[CircleDot] a)) \[CircleDot] c)) == 
         a \[CircleDot] b], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 25}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CircleDot] ((b_) \[CircleDot] 
            (((c_) \[CircleDot] (c_)) \[CircleDot] (a_))) -> 
          (a \[CircleDot] a) \[CircleDot] b, "Side" -> 1, 
        "Subpattern" -> (b_) \[CircleDot] (((c_) \[CircleDot] 
            (c_)) \[CircleDot] (a_)), "MatchingConstruct" -> 
         {"SubstitutionLemma", 45}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (((a_) \[CircleDot] (b_)) \[CircleDot] 
            ((((c_) \[CircleDot] (c_)) \[CircleDot] (c_)) \[CircleDot] 
             (b_))) \[CircleDot] (c_) -> a, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 46} -> 
     <|"Statement" -> HoldForm[
        (((((x3 \[CircleDot] x3) \[CircleDot] a) \[CircleDot] 
             ((x3 \[CircleDot] x3) \[CircleDot] a)) \[CircleDot] 
            ((x3 \[CircleDot] x3) \[CircleDot] a)) \[CircleDot] 
           c) \[CircleDot] (b \[CircleDot] c) == a \[CircleDot] b], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 25}, 
        "Construct" -> {"SubstitutionLemma", 39}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleDot] (a_)) \[CircleDot] ((b_) \[CircleDot] 
            (c_)) -> c \[CircleDot] b, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          (((((x3 \[CircleDot] x3) \[CircleDot] a) \[CircleDot] (
                (x3 \[CircleDot] x3) \[CircleDot] a)) \[CircleDot] 
              ((x3 \[CircleDot] x3) \[CircleDot] a)) \[CircleDot] 
             c) \[CircleDot] (b \[CircleDot] c) == a \[CircleDot] b], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 47} -> 
     <|"Statement" -> HoldForm[
        ((a \[CircleDot] (x3 \[CircleDot] x3)) \[CircleDot] c) \[CircleDot] 
          (b \[CircleDot] c) == a \[CircleDot] b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 46}, 
        "Construct" -> {"SubstitutionLemma", 39}, "Position" -> {1, 1}, 
        "Rule" -> ((a_) \[CircleDot] (a_)) \[CircleDot] ((b_) \[CircleDot] 
            (c_)) -> c \[CircleDot] b, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          ((a \[CircleDot] (x3 \[CircleDot] x3)) \[CircleDot] c) \[CircleDot] 
            (b \[CircleDot] c) == a \[CircleDot] b], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 48} -> 
     <|"Statement" -> HoldForm[(a \[CircleDot] c) \[CircleDot] 
          (b \[CircleDot] c) == a \[CircleDot] b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 47}, 
        "Construct" -> {"SubstitutionLemma", 21}, "Position" -> {1, 1}, 
        "Rule" -> (a_) \[CircleDot] ((b_) \[CircleDot] (b_)) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (a \[CircleDot] c) \[CircleDot] (b \[CircleDot] c) == 
           a \[CircleDot] b], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 26} -> 
     <|"Statement" -> HoldForm[a == (a \[CircleDot] (b \[CircleDot] 
            c)) \[CircleDot] (c \[CircleDot] b)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 38}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CircleDot] (b_)) \[CircleDot] 
           (((c_) \[CircleDot] (c_)) \[CircleDot] (b_)) -> a, "Side" -> 1, 
        "Subpattern" -> ((c_) \[CircleDot] (c_)) \[CircleDot] (b_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 39}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((a_) \[CircleDot] (a_)) \[CircleDot] ((b_) \[CircleDot] (c_)) -> 
          c \[CircleDot] b, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 27} -> 
     <|"Statement" -> HoldForm[(a \[CircleDot] (x3 \[CircleDot] 
            c)) \[CircleDot] b == a \[CircleDot] (b \[CircleDot] 
           (c \[CircleDot] x3))], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 48}, "Orientation" -> 1, 
        "Rule" -> ((a_) \[CircleDot] (b_)) \[CircleDot] ((c_) \[CircleDot] 
            (b_)) -> a \[CircleDot] c, "Side" -> 1, "Subpattern" -> 
         (a_) \[CircleDot] (b_), "MatchingConstruct" -> {"CriticalPairLemma", 
          26}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CircleDot] ((b_) \[CircleDot] (c_))) \[CircleDot] 
           ((c_) \[CircleDot] (b_)) -> a, "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 40} -> 
     <|"Statement" -> HoldForm[a \[CircleDot] 
          (((a \[CircleDot] a) \[CircleDot] c) \[CircleDot] b) == 
         (a \[CircleDot] ((a \[CircleDot] a) \[CircleDot] b)) \[CircleDot] 
          ((a \[CircleDot] a) \[CircleDot] c)], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Construct" -> 
         {"SubstitutionLemma", 39}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CircleDot] (a_)) \[CircleDot] ((b_) \[CircleDot] 
            (c_)) -> c \[CircleDot] b, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CircleDot] 
            (((a \[CircleDot] a) \[CircleDot] c) \[CircleDot] b) == 
           (a \[CircleDot] ((a \[CircleDot] a) \[CircleDot] b)) \[CircleDot] 
            ((a \[CircleDot] a) \[CircleDot] c)], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 49} -> 
     <|"Statement" -> HoldForm[a \[CircleDot] 
          (((a \[CircleDot] a) \[CircleDot] c) \[CircleDot] b) == 
         a \[CircleDot] (((a \[CircleDot] a) \[CircleDot] c) \[CircleDot] 
           (b \[CircleDot] (a \[CircleDot] a)))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 40}, 
        "Construct" -> {"CriticalPairLemma", 27}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleDot] ((b_) \[CircleDot] (c_))) \[CircleDot] 
           (x3_) -> a \[CircleDot] (x3 \[CircleDot] (c \[CircleDot] b)), 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CircleDot] (((a \[CircleDot] a) \[CircleDot] c) \[CircleDot] 
             b) == a \[CircleDot] (((a \[CircleDot] a) \[CircleDot] 
              c) \[CircleDot] (b \[CircleDot] (a \[CircleDot] a)))], 
        "Source" -> "cpl"|>|>, {"Conclusion", 1} -> 
     <|"Statement" -> HoldForm[a \[CircleDot] 
          (((a \[CircleDot] a) \[CircleDot] c) \[CircleDot] b) == 
         a \[CircleDot] (((a \[CircleDot] a) \[CircleDot] c) \[CircleDot] 
           b)], "Proof" -> <|"Input" -> {"SubstitutionLemma", 49}, 
        "Construct" -> {"SubstitutionLemma", 21}, "Position" -> {2, 2}, 
        "Rule" -> (a_) \[CircleDot] ((b_) \[CircleDot] (b_)) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CircleDot] (((a \[CircleDot] a) \[CircleDot] c) \[CircleDot] 
             b) == a \[CircleDot] (((a \[CircleDot] a) \[CircleDot] 
              c) \[CircleDot] b)], "Source" -> "cpl"|>|>}|>]
