ProofObject["EquationalLogic", Inactive[Equal][
  (a \[CircleDot] a) \[CircleDot] ((a \[CircleDot] a) \[CircleDot] a), a], 
 {Inactive[Equal][(a_) \[CircleDot] 
    (((((a_) \[CircleDot] (a_)) \[CircleDot] (b_)) \[CircleDot] 
      (c_)) \[CircleDot] ((((a_) \[CircleDot] (a_)) \[CircleDot] 
       (a_)) \[CircleDot] (c_))), b_]}, <|"Variables" -> {a, b, c, x3, x4}, 
  "Constants" -> {}, "Proof" -> 
   {{"Axiom", 1} -> <|"Statement" -> HoldForm[
        a \[CircleDot] ((((a \[CircleDot] a) \[CircleDot] b) \[CircleDot] 
            c) \[CircleDot] (((a \[CircleDot] a) \[CircleDot] a) \[CircleDot] 
            c)) == b], "Proof" -> <||>|>, {"Hypothesis", 1} -> 
     <|"Statement" -> HoldForm[(a \[CircleDot] a) \[CircleDot] 
          ((a \[CircleDot] a) \[CircleDot] a) == a], "Proof" -> <||>|>, 
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
    {"SubstitutionLemma", 22} -> 
     <|"Statement" -> HoldForm[b == (a \[CircleDot] a) \[CircleDot] 
          (((a \[CircleDot] a) \[CircleDot] b) \[CircleDot] 
           ((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] a)))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 15}, 
        "Construct" -> {"SubstitutionLemma", 21}, "Position" -> {2, 2}, 
        "Rule" -> (a_) \[CircleDot] ((b_) \[CircleDot] (b_)) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          b == (a \[CircleDot] a) \[CircleDot] 
            (((a \[CircleDot] a) \[CircleDot] b) \[CircleDot] 
             ((a \[CircleDot] a) \[CircleDot] (a \[CircleDot] a)))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 23} -> 
     <|"Statement" -> HoldForm[b == (a \[CircleDot] a) \[CircleDot] 
          ((a \[CircleDot] a) \[CircleDot] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 22}, 
        "Construct" -> {"SubstitutionLemma", 21}, "Position" -> {2}, 
        "Rule" -> (a_) \[CircleDot] ((b_) \[CircleDot] (b_)) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          b == (a \[CircleDot] a) \[CircleDot] ((a \[CircleDot] 
              a) \[CircleDot] b)], "Source" -> "norm"|>|>, 
    {"Conclusion", 1} -> <|"Statement" -> HoldForm[a == a], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Construct" -> 
         {"SubstitutionLemma", 23}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleDot] (a_)) \[CircleDot] 
           (((a_) \[CircleDot] (a_)) \[CircleDot] (b_)) -> b, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[a == a], 
        "Source" -> "cpl"|>|>}|>]
