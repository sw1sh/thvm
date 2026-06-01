{ProofObject["EquationalLogic", Inactive[Equal][
   (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] (b \[CenterDot] c)), a], 
  {Inactive[Equal][(((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
     ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] (a_))), c_]}, 
  <|"Variables" -> {a, b, c, x255, x3, x4}, "Constants" -> {}, 
   "Proof" -> {{"Axiom", 1} -> <|"Statement" -> 
        HoldForm[((a \[CenterDot] b) \[CenterDot] c) \[CenterDot] 
           (a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] a)) == c], 
       "Proof" -> <||>|>, {"Hypothesis", 1} -> 
      <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] c)) == a], "Proof" -> <||>|>, 
     {"CriticalPairLemma", 1} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] a) == 
          ((((a \[CenterDot] b) \[CenterDot] c) \[CenterDot] x3) \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] 
              a))) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
             c) \[CenterDot] (c \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              c)))], "Proof" -> <|"Construct" -> {"Axiom", 1}, 
         "Orientation" -> 1, "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] (
                c_)) \[CenterDot] (a_))) -> c, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (c_), "MatchingConstruct" -> 
          {"Axiom", 1}, "MatchingOrientation" -> 1, "MatchingRule" -> 
          (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
            ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
              (a_))) -> c, "MatchingSide" -> 1, "Position" -> {2, 2, 1}|>|>, 
     {"CriticalPairLemma", 2} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a) == 
          ((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a))) \[CenterDot] a], "Proof" -> 
        <|"Construct" -> {"CriticalPairLemma", 1}, "Orientation" -> -1, 
         "Rule" -> (((((a_) \[CenterDot] (b_)) \[CenterDot] (
                c_)) \[CenterDot] (x3_)) \[CenterDot] ((a_) \[CenterDot] 
              (((a_) \[CenterDot] (c_)) \[CenterDot] (a_)))) \[CenterDot] 
            ((((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
             ((c_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] (
                c_)))) -> a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] a), 
         "Side" -> 1, "Subpattern" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (c_)) \[CenterDot] ((c_) \[CenterDot] (((a_) \[CenterDot] 
              (b_)) \[CenterDot] (c_))), "MatchingConstruct" -> {"Axiom", 1}, 
         "MatchingOrientation" -> 1, "MatchingRule" -> 
          (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
            ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
              (a_))) -> c, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"CriticalPairLemma", 3} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a) == 
          (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a))) \[CenterDot] a], "Proof" -> 
        <|"Construct" -> {"CriticalPairLemma", 2}, "Orientation" -> -1, 
         "Rule" -> (((((a_) \[CenterDot] (a_)) \[CenterDot] (
                a_)) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
              (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) \[CenterDot] 
            (a_) -> a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
         "Side" -> 1, "Subpattern" -> (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (a_)) \[CenterDot] (b_), "MatchingConstruct" -> {"Axiom", 1}, 
         "MatchingOrientation" -> 1, "MatchingRule" -> 
          (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
            ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
              (a_))) -> c, "MatchingSide" -> 1, "Position" -> {1, 1}|>|>, 
     {"CriticalPairLemma", 4} -> <|"Statement" -> 
        HoldForm[a == (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
              a) \[CenterDot] a))], "Proof" -> <|"Construct" -> {"Axiom", 1}, 
         "Orientation" -> 1, "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] (
                c_)) \[CenterDot] (a_))) -> c, "Side" -> 1, 
         "Subpattern" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (c_), 
         "MatchingConstruct" -> {"CriticalPairLemma", 3}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          ((a_) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] 
                (a_)) \[CenterDot] (a_)))) \[CenterDot] (a_) -> 
           a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
         "MatchingSide" -> 1, "Position" -> {1}|>|>, 
     {"CriticalPairLemma", 5} -> <|"Statement" -> 
        HoldForm[b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b) == 
          a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
            (((b \[CenterDot] c) \[CenterDot] (b \[CenterDot] (
                (b \[CenterDot] a) \[CenterDot] b))) \[CenterDot] 
             (b \[CenterDot] c)))], "Proof" -> <|"Construct" -> {"Axiom", 1}, 
         "Orientation" -> 1, "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] (
                c_)) \[CenterDot] (a_))) -> c, "Side" -> 1, 
         "Subpattern" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (c_), 
         "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] (
                c_)) \[CenterDot] (a_))) -> c, "MatchingSide" -> 1, 
         "Position" -> {1}|>|>, {"CriticalPairLemma", 6} -> 
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
             ((((b_) \[CenterDot] (c_)) \[CenterDot] ((b_) \[CenterDot] 
                (((b_) \[CenterDot] (a_)) \[CenterDot] (b_)))) \[CenterDot] 
              ((b_) \[CenterDot] (c_)))) -> b \[CenterDot] 
            ((b \[CenterDot] a) \[CenterDot] b), "MatchingSide" -> 1, 
         "Position" -> {1, 1}|>|>, {"CriticalPairLemma", 7} -> 
      <|"Statement" -> HoldForm[
         c == (((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
               a)) \[CenterDot] b) \[CenterDot] c) \[CenterDot] 
           (b \[CenterDot] ((b \[CenterDot] c) \[CenterDot] b))], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 6}, 
         "Orientation" -> -1, "Rule" -> 
          (((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] (
                a_))) \[CenterDot] (c_)) \[CenterDot] ((b_) \[CenterDot] 
             (((b_) \[CenterDot] (c_)) \[CenterDot] (b_))) -> c, "Side" -> 1, 
         "Subpattern" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_), 
         "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] (
                c_)) \[CenterDot] (a_))) -> c, "MatchingSide" -> 1, 
         "Position" -> {1, 1, 2}|>|>, {"CriticalPairLemma", 8} -> 
      <|"Statement" -> HoldForm[a == (a \[CenterDot] 
            ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a)) \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
              ((a \[CenterDot] a) \[CenterDot] a))))], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 7}, 
         "Orientation" -> -1, "Rule" -> 
          ((((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
                (a_))) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
            ((b_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] 
              (b_))) -> c, "Side" -> 1, "Subpattern" -> 
          (((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
              (a_))) \[CenterDot] (b_)) \[CenterDot] (c_), 
         "MatchingConstruct" -> {"CriticalPairLemma", 2}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          (((((a_) \[CenterDot] (a_)) \[CenterDot] (a_)) \[CenterDot] 
              (b_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] 
                (a_)) \[CenterDot] (a_)))) \[CenterDot] (a_) -> 
           a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
         "MatchingSide" -> 1, "Position" -> {1}|>|>, 
     {"SubstitutionLemma", 2} -> <|"Statement" -> 
        HoldForm[a == (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             a)) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] a)) \[CenterDot] a)], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 8}, 
         "Construct" -> {"Axiom", 1}, "Position" -> {2, 2}, 
         "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
            ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
              (a_))) -> c, "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[a == (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a)) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] a)) \[CenterDot] a)], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 9} -> 
      <|"Statement" -> HoldForm[((a \[CenterDot] c) \[CenterDot] 
            b) \[CenterDot] ((((a \[CenterDot] c) \[CenterDot] 
              b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                b) \[CenterDot] a))) \[CenterDot] 
            ((a \[CenterDot] c) \[CenterDot] b)) == 
          (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
           ((((a \[CenterDot] c) \[CenterDot] b) \[CenterDot] 
             x3) \[CenterDot] (((((a \[CenterDot] c) \[CenterDot] 
                b) \[CenterDot] x3) \[CenterDot] (((a \[CenterDot] 
                 c) \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
                ((a \[CenterDot] c) \[CenterDot] b)))) \[CenterDot] 
             (((a \[CenterDot] c) \[CenterDot] b) \[CenterDot] x3)))], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 5}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] 
            (((b_) \[CenterDot] (c_)) \[CenterDot] ((((b_) \[CenterDot] 
                (c_)) \[CenterDot] ((b_) \[CenterDot] (((b_) \[CenterDot] 
                  (a_)) \[CenterDot] (b_)))) \[CenterDot] ((b_) \[CenterDot] (
                c_)))) -> b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), 
         "Side" -> 1, "Subpattern" -> (b_) \[CenterDot] (a_), 
         "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] (
                c_)) \[CenterDot] (a_))) -> c, "MatchingSide" -> 1, 
         "Position" -> {2, 2, 1, 2, 2, 1}|>|>, {"SubstitutionLemma", 8} -> 
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
                  ((a \[CenterDot] c) \[CenterDot] b)))) \[CenterDot] (
                ((a \[CenterDot] c) \[CenterDot] b) \[CenterDot] x3)))], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 10} -> 
      <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
           (((c \[CenterDot] x3) \[CenterDot] a) \[CenterDot] 
            ((((c \[CenterDot] x3) \[CenterDot] a) \[CenterDot] 
              b) \[CenterDot] ((c \[CenterDot] x3) \[CenterDot] a)))], 
       "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
         "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
            ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
              (a_))) -> c, "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] 
           (b_), "MatchingConstruct" -> {"Axiom", 1}, 
         "MatchingOrientation" -> 1, "MatchingRule" -> 
          (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
            ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
              (a_))) -> c, "MatchingSide" -> 1, "Position" -> {1, 1}|>|>, 
     {"CriticalPairLemma", 11} -> <|"Statement" -> 
        HoldForm[(c \[CenterDot] b) \[CenterDot] 
           (((c \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            (c \[CenterDot] b)) == a \[CenterDot] (b \[CenterDot] 
            ((((c \[CenterDot] b) \[CenterDot] (((x3 \[CenterDot] 
                  x4) \[CenterDot] c) \[CenterDot] ((((x3 \[CenterDot] 
                    x4) \[CenterDot] c) \[CenterDot] b) \[CenterDot] 
                 ((x3 \[CenterDot] x4) \[CenterDot] c)))) \[CenterDot] 
              ((c \[CenterDot] b) \[CenterDot] (((c \[CenterDot] 
                  b) \[CenterDot] a) \[CenterDot] (c \[CenterDot] 
                 b)))) \[CenterDot] ((c \[CenterDot] b) \[CenterDot] 
              (((x3 \[CenterDot] x4) \[CenterDot] c) \[CenterDot] (
                (((x3 \[CenterDot] x4) \[CenterDot] c) \[CenterDot] 
                 b) \[CenterDot] ((x3 \[CenterDot] x4) \[CenterDot] c))))))], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 5}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] 
            (((b_) \[CenterDot] (c_)) \[CenterDot] ((((b_) \[CenterDot] 
                (c_)) \[CenterDot] ((b_) \[CenterDot] (((b_) \[CenterDot] 
                  (a_)) \[CenterDot] (b_)))) \[CenterDot] ((b_) \[CenterDot] (
                c_)))) -> b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), 
         "Side" -> 1, "Subpattern" -> (b_) \[CenterDot] (c_), 
         "MatchingConstruct" -> {"CriticalPairLemma", 10}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((((c_) \[CenterDot] (x3_)) \[CenterDot] (a_)) \[CenterDot] 
             (((((c_) \[CenterDot] (x3_)) \[CenterDot] (a_)) \[CenterDot] (
                b_)) \[CenterDot] (((c_) \[CenterDot] (x3_)) \[CenterDot] (
                a_)))) -> b, "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
     {"CriticalPairLemma", 12} -> <|"Statement" -> 
        HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
           ((c \[CenterDot] a) \[CenterDot] 
            ((((((x3 \[CenterDot] x4) \[CenterDot] c) \[CenterDot] 
                (x3 \[CenterDot] ((x3 \[CenterDot] c) \[CenterDot] 
                  x3))) \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
             ((((x3 \[CenterDot] x4) \[CenterDot] c) \[CenterDot] (
                x3 \[CenterDot] ((x3 \[CenterDot] c) \[CenterDot] 
                 x3))) \[CenterDot] a)))], "Proof" -> 
        <|"Construct" -> {"CriticalPairLemma", 10}, "Orientation" -> -1, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((((c_) \[CenterDot] (x3_)) \[CenterDot] (a_)) \[CenterDot] 
             (((((c_) \[CenterDot] (x3_)) \[CenterDot] (a_)) \[CenterDot] (
                b_)) \[CenterDot] (((c_) \[CenterDot] (x3_)) \[CenterDot] (
                a_)))) -> b, "Side" -> 1, "Subpattern" -> 
          (c_) \[CenterDot] (x3_), "MatchingConstruct" -> {"Axiom", 1}, 
         "MatchingOrientation" -> 1, "MatchingRule" -> 
          (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
            ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
              (a_))) -> c, "MatchingSide" -> 1, "Position" -> {2, 1, 1}|>|>, 
     {"SubstitutionLemma", 9} -> <|"Statement" -> 
        HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
           ((c \[CenterDot] a) \[CenterDot] (((c \[CenterDot] a) \[CenterDot] 
              b) \[CenterDot] ((((x3 \[CenterDot] x4) \[CenterDot] 
                c) \[CenterDot] (x3 \[CenterDot] ((x3 \[CenterDot] 
                  c) \[CenterDot] x3))) \[CenterDot] a)))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 12}, 
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
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 10} -> 
      <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
           ((c \[CenterDot] a) \[CenterDot] (((c \[CenterDot] a) \[CenterDot] 
              b) \[CenterDot] (c \[CenterDot] a)))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 9}, 
         "Construct" -> {"Axiom", 1}, "Position" -> {2, 2, 2, 1}, 
         "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
            ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
              (a_))) -> c, "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
             ((c \[CenterDot] a) \[CenterDot] (((c \[CenterDot] 
                 a) \[CenterDot] b) \[CenterDot] (c \[CenterDot] a)))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 11} -> 
      <|"Statement" -> HoldForm[(c \[CenterDot] b) \[CenterDot] 
           (((c \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            (c \[CenterDot] b)) == a \[CenterDot] (b \[CenterDot] 
            ((b \[CenterDot] ((c \[CenterDot] b) \[CenterDot] (
                ((c \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
                (c \[CenterDot] b)))) \[CenterDot] ((c \[CenterDot] 
               b) \[CenterDot] (((x3 \[CenterDot] x4) \[CenterDot] 
                c) \[CenterDot] ((((x3 \[CenterDot] x4) \[CenterDot] 
                  c) \[CenterDot] b) \[CenterDot] ((x3 \[CenterDot] 
                  x4) \[CenterDot] c))))))], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 11}, "Construct" -> 
          {"SubstitutionLemma", 10}, "Position" -> {2, 2, 1, 1}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            (((c_) \[CenterDot] (a_)) \[CenterDot] ((((c_) \[CenterDot] 
                (a_)) \[CenterDot] (b_)) \[CenterDot] ((c_) \[CenterDot] (
                a_)))) -> b, "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[(c \[CenterDot] b) \[CenterDot] (((c \[CenterDot] 
                b) \[CenterDot] a) \[CenterDot] (c \[CenterDot] b)) == 
            a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                ((c \[CenterDot] b) \[CenterDot] (((c \[CenterDot] 
                    b) \[CenterDot] a) \[CenterDot] (c \[CenterDot] 
                   b)))) \[CenterDot] ((c \[CenterDot] b) \[CenterDot] 
                (((x3 \[CenterDot] x4) \[CenterDot] c) \[CenterDot] 
                 ((((x3 \[CenterDot] x4) \[CenterDot] c) \[CenterDot] 
                   b) \[CenterDot] ((x3 \[CenterDot] x4) \[CenterDot] 
                   c))))))], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 12} -> <|"Statement" -> 
        HoldForm[(c \[CenterDot] b) \[CenterDot] 
           (((c \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            (c \[CenterDot] b)) == a \[CenterDot] (b \[CenterDot] 
            ((b \[CenterDot] ((c \[CenterDot] b) \[CenterDot] (
                ((c \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
                (c \[CenterDot] b)))) \[CenterDot] b))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 11}, 
         "Construct" -> {"SubstitutionLemma", 10}, "Position" -> {2, 2, 2}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            (((c_) \[CenterDot] (a_)) \[CenterDot] ((((c_) \[CenterDot] 
                (a_)) \[CenterDot] (b_)) \[CenterDot] ((c_) \[CenterDot] (
                a_)))) -> b, "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[(c \[CenterDot] b) \[CenterDot] (((c \[CenterDot] 
                b) \[CenterDot] a) \[CenterDot] (c \[CenterDot] b)) == 
            a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                ((c \[CenterDot] b) \[CenterDot] (((c \[CenterDot] 
                    b) \[CenterDot] a) \[CenterDot] (c \[CenterDot] 
                   b)))) \[CenterDot] b))], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 13} -> <|"Statement" -> 
        HoldForm[(a \[CenterDot] b) \[CenterDot] 
           (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                b) \[CenterDot] a))) \[CenterDot] (a \[CenterDot] b)) == 
          (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
           (b \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
               a)) \[CenterDot] b))], "Proof" -> 
        <|"Construct" -> {"SubstitutionLemma", 12}, "Orientation" -> -1, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             (((b_) \[CenterDot] (((c_) \[CenterDot] (b_)) \[CenterDot] 
                ((((c_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
                 ((c_) \[CenterDot] (b_))))) \[CenterDot] (b_))) -> 
           (c \[CenterDot] b) \[CenterDot] (((c \[CenterDot] b) \[CenterDot] 
              a) \[CenterDot] (c \[CenterDot] b)), "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] (((c_) \[CenterDot] 
             (b_)) \[CenterDot] ((((c_) \[CenterDot] (b_)) \[CenterDot] 
              (a_)) \[CenterDot] ((c_) \[CenterDot] (b_)))), 
         "MatchingConstruct" -> {"CriticalPairLemma", 5}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          (a_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] 
             ((((b_) \[CenterDot] (c_)) \[CenterDot] ((b_) \[CenterDot] 
                (((b_) \[CenterDot] (a_)) \[CenterDot] (b_)))) \[CenterDot] 
              ((b_) \[CenterDot] (c_)))) -> b \[CenterDot] 
            ((b \[CenterDot] a) \[CenterDot] b), "MatchingSide" -> 1, 
         "Position" -> {2, 2, 1}|>|>, {"CriticalPairLemma", 14} -> 
      <|"Statement" -> HoldForm[(((b \[CenterDot] c) \[CenterDot] 
             a) \[CenterDot] ((((b \[CenterDot] c) \[CenterDot] 
               a) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                 a) \[CenterDot] b))) \[CenterDot] ((b \[CenterDot] 
               c) \[CenterDot] a))) \[CenterDot] 
           ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
            ((((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
              ((((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
                (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
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
            ((((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] (
                ((a_) \[CenterDot] (b_)) \[CenterDot] (a_)))) \[CenterDot] 
             ((a_) \[CenterDot] (b_))) -> (a \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
            (b \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
                a)) \[CenterDot] b)), "Side" -> 1, "Subpattern" -> 
          (a_) \[CenterDot] (b_), "MatchingConstruct" -> {"Axiom", 1}, 
         "MatchingOrientation" -> 1, "MatchingRule" -> 
          (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
            ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
              (a_))) -> c, "MatchingSide" -> 1, "Position" -> {1}|>|>, 
     {"SubstitutionLemma", 13} -> <|"Statement" -> 
        HoldForm[(((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
            ((((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
              (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                b))) \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
              a))) \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] 
               a) \[CenterDot] b)) \[CenterDot] 
            ((((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
              ((((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
                (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                  b))) \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
                a))) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                a) \[CenterDot] b)))) == a \[CenterDot] 
           ((a \[CenterDot] (((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
              ((((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
                (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                  b))) \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
                a)))) \[CenterDot] (((b \[CenterDot] c) \[CenterDot] 
              a) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                a) \[CenterDot] b))))], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 14}, "Construct" -> {"Axiom", 1}, 
         "Position" -> {2, 1, 1}, "Rule" -> 
          (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
            ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
              (a_))) -> c, "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[(((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
              ((((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
                (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                  b))) \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
                a))) \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] 
                 a) \[CenterDot] b)) \[CenterDot] ((((b \[CenterDot] 
                  c) \[CenterDot] a) \[CenterDot] ((((b \[CenterDot] 
                    c) \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
                   ((b \[CenterDot] a) \[CenterDot] b))) \[CenterDot] 
                 ((b \[CenterDot] c) \[CenterDot] a))) \[CenterDot] (
                b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)))) == 
            a \[CenterDot] ((a \[CenterDot] (((b \[CenterDot] c) \[CenterDot] 
                 a) \[CenterDot] ((((b \[CenterDot] c) \[CenterDot] 
                   a) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                     a) \[CenterDot] b))) \[CenterDot] ((b \[CenterDot] 
                   c) \[CenterDot] a)))) \[CenterDot] (((b \[CenterDot] 
                 c) \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
                ((b \[CenterDot] a) \[CenterDot] b))))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 14} -> 
      <|"Statement" -> HoldForm[(((b \[CenterDot] c) \[CenterDot] 
             a) \[CenterDot] ((((b \[CenterDot] c) \[CenterDot] 
               a) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                 a) \[CenterDot] b))) \[CenterDot] ((b \[CenterDot] 
               c) \[CenterDot] a))) \[CenterDot] 
           ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
            ((((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
              ((((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
                (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                  b))) \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
                a))) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                a) \[CenterDot] b)))) == a \[CenterDot] 
           ((a \[CenterDot] (((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
                a)))) \[CenterDot] (((b \[CenterDot] c) \[CenterDot] 
              a) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                a) \[CenterDot] b))))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 13}, "Construct" -> {"Axiom", 1}, 
         "Position" -> {2, 1, 2, 2, 1}, "Rule" -> 
          (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
            ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
              (a_))) -> c, "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[(((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
              ((((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
                (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                  b))) \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
                a))) \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] 
                 a) \[CenterDot] b)) \[CenterDot] ((((b \[CenterDot] 
                  c) \[CenterDot] a) \[CenterDot] ((((b \[CenterDot] 
                    c) \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
                   ((b \[CenterDot] a) \[CenterDot] b))) \[CenterDot] 
                 ((b \[CenterDot] c) \[CenterDot] a))) \[CenterDot] (
                b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)))) == 
            a \[CenterDot] ((a \[CenterDot] (((b \[CenterDot] c) \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] ((b \[CenterDot] 
                   c) \[CenterDot] a)))) \[CenterDot] (((b \[CenterDot] 
                 c) \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
                ((b \[CenterDot] a) \[CenterDot] b))))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 15} -> 
      <|"Statement" -> HoldForm[(((b \[CenterDot] c) \[CenterDot] 
             a) \[CenterDot] ((((b \[CenterDot] c) \[CenterDot] 
               a) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                 a) \[CenterDot] b))) \[CenterDot] ((b \[CenterDot] 
               c) \[CenterDot] a))) \[CenterDot] 
           ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
            ((((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
              ((((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
                (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                  b))) \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
                a))) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                a) \[CenterDot] b)))) == a \[CenterDot] 
           ((a \[CenterDot] (((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
                a)))) \[CenterDot] a)], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 14}, "Construct" -> {"Axiom", 1}, 
         "Position" -> {2, 2}, "Rule" -> 
          (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
            ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
              (a_))) -> c, "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[(((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
              ((((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
                (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                  b))) \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
                a))) \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] 
                 a) \[CenterDot] b)) \[CenterDot] ((((b \[CenterDot] 
                  c) \[CenterDot] a) \[CenterDot] ((((b \[CenterDot] 
                    c) \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
                   ((b \[CenterDot] a) \[CenterDot] b))) \[CenterDot] 
                 ((b \[CenterDot] c) \[CenterDot] a))) \[CenterDot] (
                b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)))) == 
            a \[CenterDot] ((a \[CenterDot] (((b \[CenterDot] c) \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] ((b \[CenterDot] 
                   c) \[CenterDot] a)))) \[CenterDot] a)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 16} -> 
      <|"Statement" -> HoldForm[(((b \[CenterDot] c) \[CenterDot] 
             a) \[CenterDot] (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
              a))) \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] 
               a) \[CenterDot] b)) \[CenterDot] 
            ((((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
              ((((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
                (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                  b))) \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
                a))) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                a) \[CenterDot] b)))) == a \[CenterDot] 
           ((a \[CenterDot] (((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
                a)))) \[CenterDot] a)], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 15}, "Construct" -> {"Axiom", 1}, 
         "Position" -> {1, 2, 1}, "Rule" -> 
          (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
            ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
              (a_))) -> c, "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[(((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
                a))) \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] 
                 a) \[CenterDot] b)) \[CenterDot] ((((b \[CenterDot] 
                  c) \[CenterDot] a) \[CenterDot] ((((b \[CenterDot] 
                    c) \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
                   ((b \[CenterDot] a) \[CenterDot] b))) \[CenterDot] 
                 ((b \[CenterDot] c) \[CenterDot] a))) \[CenterDot] (
                b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)))) == 
            a \[CenterDot] ((a \[CenterDot] (((b \[CenterDot] c) \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] ((b \[CenterDot] 
                   c) \[CenterDot] a)))) \[CenterDot] a)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 17} -> 
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
        <|"Input" -> {"SubstitutionLemma", 16}, "Construct" -> {"Axiom", 1}, 
         "Position" -> {2, 2, 1, 2, 1}, "Rule" -> 
          (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
            ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
              (a_))) -> c, "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[(((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
                a))) \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] 
                 a) \[CenterDot] b)) \[CenterDot] ((((b \[CenterDot] 
                  c) \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 ((b \[CenterDot] c) \[CenterDot] a))) \[CenterDot] (
                b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)))) == 
            a \[CenterDot] ((a \[CenterDot] (((b \[CenterDot] c) \[CenterDot] 
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
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 17}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] 
            (((a_) \[CenterDot] ((((b_) \[CenterDot] (c_)) \[CenterDot] 
                (a_)) \[CenterDot] ((a_) \[CenterDot] (((b_) \[CenterDot] 
                  (c_)) \[CenterDot] (a_))))) \[CenterDot] (a_)) -> 
           (((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
               a))) \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] 
                a) \[CenterDot] b)) \[CenterDot] 
             ((((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] (
                a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
                 a))) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                 a) \[CenterDot] b)))), "Side" -> 1, "Subpattern" -> 
          (((b_) \[CenterDot] (c_)) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] (a_))), 
         "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] (
                c_)) \[CenterDot] (a_))) -> c, "MatchingSide" -> 1, 
         "Position" -> {2, 1, 2}|>|>, {"SubstitutionLemma", 18} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] ((a \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
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
                 a) \[CenterDot] a)) \[CenterDot] ((((a \[CenterDot] 
                  a) \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] (
                a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)))) == 
            a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 19} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] ((a \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a)))) == a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 18}, 
         "Construct" -> {"Axiom", 1}, "Position" -> {2, 2, 1}, 
         "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
            ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
              (a_))) -> c, "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] (
                a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)))) == 
            a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 16} -> 
      <|"Statement" -> HoldForm[c \[CenterDot] ((c \[CenterDot] 
             a) \[CenterDot] c) == a \[CenterDot] 
           ((b \[CenterDot] ((b \[CenterDot] c) \[CenterDot] b)) \[CenterDot] 
            (((c \[CenterDot] ((b \[CenterDot] x3) \[CenterDot] 
                (((b \[CenterDot] x3) \[CenterDot] (b \[CenterDot] 
                   ((b \[CenterDot] c) \[CenterDot] b))) \[CenterDot] 
                 (b \[CenterDot] x3)))) \[CenterDot] (c \[CenterDot] (
                (c \[CenterDot] a) \[CenterDot] c))) \[CenterDot] 
             (c \[CenterDot] ((b \[CenterDot] x3) \[CenterDot] (
                ((b \[CenterDot] x3) \[CenterDot] (b \[CenterDot] 
                  ((b \[CenterDot] c) \[CenterDot] b))) \[CenterDot] 
                (b \[CenterDot] x3))))))], "Proof" -> 
        <|"Construct" -> {"CriticalPairLemma", 5}, "Orientation" -> -1, 
         "Rule" -> (a_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] 
             ((((b_) \[CenterDot] (c_)) \[CenterDot] ((b_) \[CenterDot] 
                (((b_) \[CenterDot] (a_)) \[CenterDot] (b_)))) \[CenterDot] 
              ((b_) \[CenterDot] (c_)))) -> b \[CenterDot] 
            ((b \[CenterDot] a) \[CenterDot] b), "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] (c_), "MatchingConstruct" -> 
          {"CriticalPairLemma", 5}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> (a_) \[CenterDot] (((b_) \[CenterDot] 
              (c_)) \[CenterDot] ((((b_) \[CenterDot] (c_)) \[CenterDot] (
                (b_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] 
                 (b_)))) \[CenterDot] ((b_) \[CenterDot] (c_)))) -> 
           b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), 
         "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
     {"SubstitutionLemma", 20} -> <|"Statement" -> 
        HoldForm[c \[CenterDot] ((c \[CenterDot] a) \[CenterDot] c) == 
          a \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
              b)) \[CenterDot] (((b \[CenterDot] ((b \[CenterDot] 
                 c) \[CenterDot] b)) \[CenterDot] (c \[CenterDot] (
                (c \[CenterDot] a) \[CenterDot] c))) \[CenterDot] 
             (c \[CenterDot] ((b \[CenterDot] x3) \[CenterDot] (
                ((b \[CenterDot] x3) \[CenterDot] (b \[CenterDot] 
                  ((b \[CenterDot] c) \[CenterDot] b))) \[CenterDot] 
                (b \[CenterDot] x3))))))], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 16}, "Construct" -> 
          {"CriticalPairLemma", 5}, "Position" -> {2, 2, 1, 1}, 
         "Rule" -> (a_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] 
             ((((b_) \[CenterDot] (c_)) \[CenterDot] ((b_) \[CenterDot] 
                (((b_) \[CenterDot] (a_)) \[CenterDot] (b_)))) \[CenterDot] 
              ((b_) \[CenterDot] (c_)))) -> b \[CenterDot] 
            ((b \[CenterDot] a) \[CenterDot] b), "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[c \[CenterDot] 
             ((c \[CenterDot] a) \[CenterDot] c) == a \[CenterDot] 
             ((b \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
                b)) \[CenterDot] (((b \[CenterDot] ((b \[CenterDot] 
                   c) \[CenterDot] b)) \[CenterDot] (c \[CenterDot] 
                 ((c \[CenterDot] a) \[CenterDot] c))) \[CenterDot] (
                c \[CenterDot] ((b \[CenterDot] x3) \[CenterDot] 
                 (((b \[CenterDot] x3) \[CenterDot] (b \[CenterDot] 
                    ((b \[CenterDot] c) \[CenterDot] b))) \[CenterDot] 
                  (b \[CenterDot] x3))))))], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 21} -> <|"Statement" -> 
        HoldForm[c \[CenterDot] ((c \[CenterDot] a) \[CenterDot] c) == 
          a \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
              b)) \[CenterDot] (((b \[CenterDot] ((b \[CenterDot] 
                 c) \[CenterDot] b)) \[CenterDot] (c \[CenterDot] (
                (c \[CenterDot] a) \[CenterDot] c))) \[CenterDot] 
             (b \[CenterDot] ((b \[CenterDot] c) \[CenterDot] b))))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 20}, 
         "Construct" -> {"CriticalPairLemma", 5}, "Position" -> {2, 2, 2}, 
         "Rule" -> (a_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] 
             ((((b_) \[CenterDot] (c_)) \[CenterDot] ((b_) \[CenterDot] 
                (((b_) \[CenterDot] (a_)) \[CenterDot] (b_)))) \[CenterDot] 
              ((b_) \[CenterDot] (c_)))) -> b \[CenterDot] 
            ((b \[CenterDot] a) \[CenterDot] b), "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[c \[CenterDot] 
             ((c \[CenterDot] a) \[CenterDot] c) == a \[CenterDot] 
             ((b \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
                b)) \[CenterDot] (((b \[CenterDot] ((b \[CenterDot] 
                   c) \[CenterDot] b)) \[CenterDot] (c \[CenterDot] 
                 ((c \[CenterDot] a) \[CenterDot] c))) \[CenterDot] (
                b \[CenterDot] ((b \[CenterDot] c) \[CenterDot] b))))], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 17} -> 
      <|"Statement" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
           (((b \[CenterDot] a) \[CenterDot] ((a \[CenterDot] (b \[CenterDot] 
                a)) \[CenterDot] a)) \[CenterDot] (b \[CenterDot] a)) == 
          ((a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] a) \[CenterDot] 
           ((a \[CenterDot] ((a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
              a)) \[CenterDot] (((a \[CenterDot] (b \[CenterDot] 
                a)) \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
              ((a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] a))))], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 21}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] 
            (((b_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] (
                b_))) \[CenterDot] ((((b_) \[CenterDot] (((b_) \[CenterDot] 
                  (c_)) \[CenterDot] (b_))) \[CenterDot] ((c_) \[CenterDot] 
                (((c_) \[CenterDot] (a_)) \[CenterDot] (c_)))) \[CenterDot] 
              ((b_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] 
                (b_))))) -> c \[CenterDot] ((c \[CenterDot] a) \[CenterDot] 
             c), "Side" -> 1, "Subpattern" -> ((b_) \[CenterDot] 
            (((b_) \[CenterDot] (c_)) \[CenterDot] (b_))) \[CenterDot] 
           ((c_) \[CenterDot] (((c_) \[CenterDot] (a_)) \[CenterDot] (c_))), 
         "MatchingConstruct" -> {"SubstitutionLemma", 10}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          ((a_) \[CenterDot] (b_)) \[CenterDot] (((c_) \[CenterDot] 
              (a_)) \[CenterDot] ((((c_) \[CenterDot] (a_)) \[CenterDot] (
                b_)) \[CenterDot] ((c_) \[CenterDot] (a_)))) -> b, 
         "MatchingSide" -> 1, "Position" -> {2, 2, 1}|>|>, 
     {"CriticalPairLemma", 18} -> <|"Statement" -> 
        HoldForm[((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                 a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
              a)) \[CenterDot] a) \[CenterDot] ((a \[CenterDot] 
             ((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                   a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                  ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                a)) \[CenterDot] a)) \[CenterDot] 
            (((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                   a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                  ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                a)) \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
              ((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                    a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                 a)) \[CenterDot] a)))) == (a \[CenterDot] 
            ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
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
            ((((a_) \[CenterDot] (b_)) \[CenterDot] (((b_) \[CenterDot] 
                ((a_) \[CenterDot] (b_))) \[CenterDot] (b_))) \[CenterDot] 
             ((a_) \[CenterDot] (b_))) -> 
           ((b \[CenterDot] (a \[CenterDot] b)) \[CenterDot] b) \[CenterDot] 
            ((b \[CenterDot] ((b \[CenterDot] (a \[CenterDot] 
                 b)) \[CenterDot] b)) \[CenterDot] (((b \[CenterDot] 
                (a \[CenterDot] b)) \[CenterDot] b) \[CenterDot] 
              (b \[CenterDot] ((b \[CenterDot] (a \[CenterDot] 
                  b)) \[CenterDot] b)))), "Side" -> 1, "Subpattern" -> 
          (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"CriticalPairLemma", 2}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> (((((a_) \[CenterDot] (a_)) \[CenterDot] (
                a_)) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
              (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) \[CenterDot] 
            (a_) -> a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
         "MatchingSide" -> 1, "Position" -> {1}|>|>, 
     {"SubstitutionLemma", 22} -> <|"Statement" -> 
        HoldForm[((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                 a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
              a)) \[CenterDot] a) \[CenterDot] ((a \[CenterDot] 
             ((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                   a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                  ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                a)) \[CenterDot] a)) \[CenterDot] 
            (((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                   a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                  ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                a)) \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
              ((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                    a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                 a)) \[CenterDot] a)))) == (a \[CenterDot] 
            ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a)) \[CenterDot] ((a \[CenterDot] (((((a \[CenterDot] 
                    a) \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
                 (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                   a))) \[CenterDot] a)) \[CenterDot] a)) \[CenterDot] 
            (((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
               b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] a))) \[CenterDot] a))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 18}, 
         "Construct" -> {"CriticalPairLemma", 2}, "Position" -> {2, 1, 1}, 
         "Rule" -> (((((a_) \[CenterDot] (a_)) \[CenterDot] (
                a_)) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
              (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) \[CenterDot] 
            (a_) -> a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           ((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                   a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                  ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                a)) \[CenterDot] a) \[CenterDot] ((a \[CenterDot] (
                (a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
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
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 23} -> 
      <|"Statement" -> HoldForm[
         ((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
                b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                  a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
            a) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] (
                ((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
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
           (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a)) \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
                ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
              a)) \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] (
                (a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] a))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 22}, 
         "Construct" -> {"CriticalPairLemma", 2}, "Position" -> {2, 1, 2, 1, 
          2}, "Rule" -> (((((a_) \[CenterDot] (a_)) \[CenterDot] (
                a_)) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
              (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) \[CenterDot] 
            (a_) -> a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           ((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                   a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                  ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                a)) \[CenterDot] a) \[CenterDot] ((a \[CenterDot] (
                (a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
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
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 24} -> 
      <|"Statement" -> HoldForm[
         ((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
                b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                  a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
            a) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] (
                ((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
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
           (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] a))) \[CenterDot] 
            (((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
               b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] a))) \[CenterDot] a))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 23}, 
         "Construct" -> {"CriticalPairLemma", 3}, "Position" -> {2, 1, 2}, 
         "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] 
                (a_)) \[CenterDot] (a_)))) \[CenterDot] (a_) -> 
           a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           ((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                   a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                  ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                a)) \[CenterDot] a) \[CenterDot] ((a \[CenterDot] (
                (a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
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
                  a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
              (((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
                 b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a))) \[CenterDot] a))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 25} -> 
      <|"Statement" -> HoldForm[
         ((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
                b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                  a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
            a) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] (
                ((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
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
           (a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] (
                (a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] a))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 24}, 
         "Construct" -> {"CriticalPairLemma", 4}, "Position" -> {2, 1}, 
         "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
              (a_))) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] (
                a_)) \[CenterDot] (a_))) -> a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[
           ((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                   a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                  ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                a)) \[CenterDot] a) \[CenterDot] ((a \[CenterDot] (
                (a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
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
               a)) \[CenterDot] (a \[CenterDot] (((((a \[CenterDot] 
                   a) \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
                (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a))) \[CenterDot] a))], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 26} -> <|"Statement" -> 
        HoldForm[((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                 a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
              a)) \[CenterDot] a) \[CenterDot] ((a \[CenterDot] 
             ((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                   a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                  ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                a)) \[CenterDot] a)) \[CenterDot] 
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
              a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 25}, 
         "Construct" -> {"CriticalPairLemma", 2}, "Position" -> {2, 2}, 
         "Rule" -> (((((a_) \[CenterDot] (a_)) \[CenterDot] (
                a_)) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
              (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) \[CenterDot] 
            (a_) -> a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           ((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                   a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                  ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                a)) \[CenterDot] a) \[CenterDot] ((a \[CenterDot] (
                (a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
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
               a)) \[CenterDot] (a \[CenterDot] (a \[CenterDot] (
                (a \[CenterDot] a) \[CenterDot] a)))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 27} -> 
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
              a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 26}, 
         "Construct" -> {"CriticalPairLemma", 2}, "Position" -> {1, 1, 2}, 
         "Rule" -> (((((a_) \[CenterDot] (a_)) \[CenterDot] (
                a_)) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
              (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) \[CenterDot] 
            (a_) -> a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                 a))) \[CenterDot] a) \[CenterDot] ((a \[CenterDot] (
                (a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
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
               a)) \[CenterDot] (a \[CenterDot] (a \[CenterDot] (
                (a \[CenterDot] a) \[CenterDot] a)))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 28} -> 
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
              a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 27}, 
         "Construct" -> {"CriticalPairLemma", 3}, "Position" -> {1}, 
         "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] 
                (a_)) \[CenterDot] (a_)))) \[CenterDot] (a_) -> 
           a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
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
                  a)) \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                ((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                      a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                     ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                   a)) \[CenterDot] a)))) == (a \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
              (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 29} -> 
      <|"Statement" -> HoldForm[
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
              a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 28}, 
         "Construct" -> {"CriticalPairLemma", 2}, "Position" -> {2, 1, 2, 1, 
          2}, "Rule" -> (((((a_) \[CenterDot] (a_)) \[CenterDot] (
                a_)) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
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
                     a))) \[CenterDot] a)) \[CenterDot] a) \[CenterDot] (
                a \[CenterDot] ((a \[CenterDot] (((((a \[CenterDot] 
                       a) \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
                    (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                      a))) \[CenterDot] a)) \[CenterDot] a)))) == 
            (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
             (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 30} -> 
      <|"Statement" -> HoldForm[
         (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a))) \[CenterDot] (((a \[CenterDot] (((((a \[CenterDot] 
                    a) \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
                 (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                   a))) \[CenterDot] a)) \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] ((a \[CenterDot] (((((a \[CenterDot] 
                     a) \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
                  (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                    a))) \[CenterDot] a)) \[CenterDot] a)))) == 
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 29}, 
         "Construct" -> {"CriticalPairLemma", 3}, "Position" -> {2, 1, 2}, 
         "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] 
                (a_)) \[CenterDot] (a_)))) \[CenterDot] (a_) -> 
           a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
             ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                  a) \[CenterDot] a))) \[CenterDot] (((a \[CenterDot] 
                 (((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
                    b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                      a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
                a) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                  (((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
                     b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                       a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
                 a)))) == (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a)) \[CenterDot] (a \[CenterDot] (a \[CenterDot] (
                (a \[CenterDot] a) \[CenterDot] a)))], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 19} -> 
      <|"Statement" -> HoldForm[b \[CenterDot] ((b \[CenterDot] 
             a) \[CenterDot] b) == (a \[CenterDot] (b \[CenterDot] 
             ((b \[CenterDot] a) \[CenterDot] b))) \[CenterDot] 
           (((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a)))], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 10}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((((c_) \[CenterDot] (x3_)) \[CenterDot] (a_)) \[CenterDot] 
             (((((c_) \[CenterDot] (x3_)) \[CenterDot] (a_)) \[CenterDot] (
                b_)) \[CenterDot] (((c_) \[CenterDot] (x3_)) \[CenterDot] (
                a_)))) -> b, "Side" -> 1, "Subpattern" -> 
          (((c_) \[CenterDot] (x3_)) \[CenterDot] (a_)) \[CenterDot] (b_), 
         "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] (
                c_)) \[CenterDot] (a_))) -> c, "MatchingSide" -> 1, 
         "Position" -> {2, 2, 1}|>|>, {"SubstitutionLemma", 31} -> 
      <|"Statement" -> HoldForm[
         (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) == 
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 30}, 
         "Construct" -> {"CriticalPairLemma", 19}, "Position" -> {2}, 
         "Rule" -> ((a_) \[CenterDot] ((b_) \[CenterDot] (((b_) \[CenterDot] 
                (a_)) \[CenterDot] (b_)))) \[CenterDot] 
            ((((b_) \[CenterDot] (c_)) \[CenterDot] (a_)) \[CenterDot] 
             ((a_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] (
                a_)))) -> b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
             (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) == 
            (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
             (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 32} -> 
      <|"Statement" -> HoldForm[a == (a \[CenterDot] 
            ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 31}, 
         "Construct" -> {"CriticalPairLemma", 4}, "Position" -> {}, 
         "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
              (a_))) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] (
                a_)) \[CenterDot] (a_))) -> a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[a == (a \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
              (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 33} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] a == a \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] a)], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 19}, 
         "Construct" -> {"SubstitutionLemma", 32}, "Position" -> {2}, 
         "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
              (a_))) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
              (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) -> a, 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[a \[CenterDot] a == 
            a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 34} -> 
      <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
           ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            a)], "Proof" -> <|"Input" -> {"SubstitutionLemma", 2}, 
         "Construct" -> {"SubstitutionLemma", 33}, "Position" -> {1}, 
         "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (a_)) -> a \[CenterDot] a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
             ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a)) \[CenterDot] a)], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 35} -> <|"Statement" -> 
        HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] a)], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 34}, 
         "Construct" -> {"SubstitutionLemma", 33}, "Position" -> {2, 1}, 
         "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (a_)) -> a \[CenterDot] a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] a)], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 20} -> <|"Statement" -> 
        HoldForm[b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b) == 
          a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] (b \[CenterDot] (
                (b \[CenterDot] a) \[CenterDot] b))) \[CenterDot] a))], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 7}, 
         "Orientation" -> -1, "Rule" -> 
          ((((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
                (a_))) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
            ((b_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] 
              (b_))) -> c, "Side" -> 1, "Subpattern" -> 
          (((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
              (a_))) \[CenterDot] (b_)) \[CenterDot] (c_), 
         "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] (
                c_)) \[CenterDot] (a_))) -> c, "MatchingSide" -> 1, 
         "Position" -> {1}|>|>, {"CriticalPairLemma", 21} -> 
      <|"Statement" -> HoldForm[
         (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           (((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b)) \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
             ((b \[CenterDot] a) \[CenterDot] b))) == a \[CenterDot] 
           (a \[CenterDot] ((a \[CenterDot] ((b \[CenterDot] 
                ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
               a)) \[CenterDot] a))], "Proof" -> 
        <|"Construct" -> {"CriticalPairLemma", 20}, "Orientation" -> -1, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] ((b_) \[CenterDot] (((b_) \[CenterDot] 
                  (a_)) \[CenterDot] (b_)))) \[CenterDot] (a_))) -> 
           b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), "Side" -> 1, 
         "Subpattern" -> ((b_) \[CenterDot] (a_)) \[CenterDot] (b_), 
         "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] (
                c_)) \[CenterDot] (a_))) -> c, "MatchingSide" -> 1, 
         "Position" -> {2, 2, 1, 2, 2}|>|>, {"SubstitutionLemma", 36} -> 
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
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 36}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] (((b_) \[CenterDot] (((b_) \[CenterDot] 
                   (a_)) \[CenterDot] (b_))) \[CenterDot] (a_))) \[CenterDot] 
              (a_))) -> (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
              b)) \[CenterDot] a, "Side" -> 1, "Subpattern" -> 
          (b_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] (b_)), 
         "MatchingConstruct" -> {"SubstitutionLemma", 33}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)) -> 
           a \[CenterDot] a, "MatchingSide" -> 1, "Position" -> {2, 2, 1, 2, 
          1}|>|>, {"SubstitutionLemma", 37} -> 
      <|"Statement" -> HoldForm[
         (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           a == a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
              a) \[CenterDot] a))], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 22}, "Construct" -> 
          {"SubstitutionLemma", 33}, "Position" -> {2, 2, 1}, 
         "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (a_)) -> a \[CenterDot] a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[(a \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] a)) \[CenterDot] a == a \[CenterDot] 
             (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 38} -> 
      <|"Statement" -> HoldForm[
         (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           a == a \[CenterDot] (a \[CenterDot] a)], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 37}, 
         "Construct" -> {"SubstitutionLemma", 33}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (a_)) -> a \[CenterDot] a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[(a \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] a)) \[CenterDot] a == a \[CenterDot] 
             (a \[CenterDot] a)], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 39} -> <|"Statement" -> 
        HoldForm[(a \[CenterDot] a) \[CenterDot] a == a \[CenterDot] 
           (a \[CenterDot] a)], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 38}, "Construct" -> 
          {"SubstitutionLemma", 33}, "Position" -> {1}, 
         "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (a_)) -> a \[CenterDot] a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[(a \[CenterDot] a) \[CenterDot] a == 
            a \[CenterDot] (a \[CenterDot] a)], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 40} -> <|"Statement" -> 
        HoldForm[a == (a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
            (a \[CenterDot] a))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 35}, "Construct" -> 
          {"SubstitutionLemma", 39}, "Position" -> {2}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (a_) -> 
           a \[CenterDot] (a \[CenterDot] a), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] (a \[CenterDot] a))], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 23} -> <|"Statement" -> 
        HoldForm[((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
             b)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))) == 
          (a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
             a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            (((((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                 b)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
                ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                  b)))) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
                (a \[CenterDot] b)) \[CenterDot] ((a \[CenterDot] 
                 b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
                 (a \[CenterDot] b))))) \[CenterDot] 
             (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                b)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (
                (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))))))], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 8}, 
         "Orientation" -> -1, "Rule" -> 
          ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
              (a_))) \[CenterDot] (((((a_) \[CenterDot] (c_)) \[CenterDot] (
                b_)) \[CenterDot] (x3_)) \[CenterDot] 
             ((((((a_) \[CenterDot] (c_)) \[CenterDot] (b_)) \[CenterDot] 
                (x3_)) \[CenterDot] ((((a_) \[CenterDot] (c_)) \[CenterDot] 
                 (b_)) \[CenterDot] ((b_) \[CenterDot] (((a_) \[CenterDot] 
                   (c_)) \[CenterDot] (b_))))) \[CenterDot] 
              ((((a_) \[CenterDot] (c_)) \[CenterDot] (b_)) \[CenterDot] (
                x3_)))) -> ((a \[CenterDot] c) \[CenterDot] b) \[CenterDot] 
            (b \[CenterDot] ((a \[CenterDot] c) \[CenterDot] b)), 
         "Side" -> 1, "Subpattern" -> (((a_) \[CenterDot] (c_)) \[CenterDot] 
            (b_)) \[CenterDot] (x3_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 40}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] ((a_) \[CenterDot] (a_))) -> a, 
         "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
     {"SubstitutionLemma", 41} -> <|"Statement" -> 
        HoldForm[((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
             b)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))) == 
          (a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
             a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            ((((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                b)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (
                (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                 b)))) \[CenterDot] ((((a \[CenterDot] b) \[CenterDot] 
                (a \[CenterDot] b)) \[CenterDot] ((a \[CenterDot] 
                 b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
                 (a \[CenterDot] b)))) \[CenterDot] (((a \[CenterDot] 
                 b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] (
                (a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] 
                  b) \[CenterDot] (a \[CenterDot] b)))))))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 23}, 
         "Construct" -> {"SubstitutionLemma", 39}, "Position" -> {2, 2}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (a_) -> 
           a \[CenterDot] (a \[CenterDot] a), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[((a \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] b)) \[CenterDot] ((a \[CenterDot] 
               b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (
                a \[CenterDot] b))) == (a \[CenterDot] ((a \[CenterDot] 
                (a \[CenterDot] b)) \[CenterDot] a)) \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] ((((a \[CenterDot] 
                  b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
                ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] 
                   b) \[CenterDot] (a \[CenterDot] b)))) \[CenterDot] (
                (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                   b)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
                  ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                    b)))) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
                  (a \[CenterDot] b)) \[CenterDot] ((a \[CenterDot] 
                   b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
                   (a \[CenterDot] b)))))))], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 42} -> <|"Statement" -> 
        HoldForm[((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
             b)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))) == 
          (a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
             a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] ((((a \[CenterDot] 
                 b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] (
                (a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] 
                  b) \[CenterDot] (a \[CenterDot] b)))) \[CenterDot] 
              (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                 b)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
                ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)))))))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 41}, 
         "Construct" -> {"SubstitutionLemma", 40}, "Position" -> {2, 2, 1}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
             ((a_) \[CenterDot] (a_))) -> a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[((a \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] b)) \[CenterDot] ((a \[CenterDot] 
               b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (
                a \[CenterDot] b))) == (a \[CenterDot] ((a \[CenterDot] 
                (a \[CenterDot] b)) \[CenterDot] a)) \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] 
                b) \[CenterDot] ((((a \[CenterDot] b) \[CenterDot] 
                  (a \[CenterDot] b)) \[CenterDot] ((a \[CenterDot] 
                   b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
                   (a \[CenterDot] b)))) \[CenterDot] (((a \[CenterDot] 
                   b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
                 ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] 
                    b) \[CenterDot] (a \[CenterDot] b)))))))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 43} -> 
      <|"Statement" -> HoldForm[((a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))) == 
          (a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
             a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                 b)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
                ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)))))))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 42}, 
         "Construct" -> {"SubstitutionLemma", 40}, "Position" -> {2, 2, 2, 
          1}, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] ((a_) \[CenterDot] (a_))) -> a, 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] 
                b) \[CenterDot] (a \[CenterDot] b))) == 
            (a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
               a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] 
                 b) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
                  (a \[CenterDot] b)) \[CenterDot] ((a \[CenterDot] 
                   b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
                   (a \[CenterDot] b)))))))], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 44} -> <|"Statement" -> 
        HoldForm[((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
             b)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))) == 
          (a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
             a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] b))))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 43}, "Construct" -> 
          {"SubstitutionLemma", 40}, "Position" -> {2, 2, 2, 2}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
             ((a_) \[CenterDot] (a_))) -> a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[((a \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] b)) \[CenterDot] ((a \[CenterDot] 
               b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (
                a \[CenterDot] b))) == (a \[CenterDot] ((a \[CenterDot] 
                (a \[CenterDot] b)) \[CenterDot] a)) \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] 
                b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
                (a \[CenterDot] b))))], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 45} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] a == a \[CenterDot] (a \[CenterDot] 
            (a \[CenterDot] a))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 33}, "Construct" -> 
          {"SubstitutionLemma", 39}, "Position" -> {2}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (a_) -> 
           a \[CenterDot] (a \[CenterDot] a), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] a == 
            a \[CenterDot] (a \[CenterDot] (a \[CenterDot] a))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 46} -> 
      <|"Statement" -> HoldForm[((a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))) == 
          (a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
             a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 44}, "Construct" -> 
          {"SubstitutionLemma", 45}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
              (a_))) -> a \[CenterDot] a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[((a \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] b)) \[CenterDot] ((a \[CenterDot] 
               b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (
                a \[CenterDot] b))) == (a \[CenterDot] ((a \[CenterDot] 
                (a \[CenterDot] b)) \[CenterDot] a)) \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 47} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] b == 
          (a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
             a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 46}, "Construct" -> 
          {"SubstitutionLemma", 40}, "Position" -> {}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
             ((a_) \[CenterDot] (a_))) -> a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[a \[CenterDot] b == 
            (a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
               a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] b))], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 24} -> <|"Statement" -> 
        HoldForm[(a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b) == 
          (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
            ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                b))) \[CenterDot] a))], "Proof" -> 
        <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
         "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
            ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
              (a_))) -> c, "Side" -> 1, "Subpattern" -> 
          ((a_) \[CenterDot] (b_)) \[CenterDot] (c_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 47}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (
                (a_) \[CenterDot] (b_))) \[CenterDot] (a_))) \[CenterDot] 
            (((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
              (b_))) -> a \[CenterDot] b, "MatchingSide" -> 1, 
         "Position" -> {1}|>|>, {"CriticalPairLemma", 25} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] ((a \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
               b))) \[CenterDot] a) == ((a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
               b)) \[CenterDot] (a \[CenterDot] b)))], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 5}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] 
            (((b_) \[CenterDot] (c_)) \[CenterDot] ((((b_) \[CenterDot] 
                (c_)) \[CenterDot] ((b_) \[CenterDot] (((b_) \[CenterDot] 
                  (a_)) \[CenterDot] (b_)))) \[CenterDot] ((b_) \[CenterDot] (
                c_)))) -> b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), 
         "Side" -> 1, "Subpattern" -> ((b_) \[CenterDot] (c_)) \[CenterDot] 
           ((b_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] (b_))), 
         "MatchingConstruct" -> {"CriticalPairLemma", 24}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          ((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
                ((a_) \[CenterDot] (b_)))) \[CenterDot] (a_))) -> 
           (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b), 
         "MatchingSide" -> 1, "Position" -> {2, 2, 1}|>|>, 
     {"SubstitutionLemma", 48} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
               b) \[CenterDot] (a \[CenterDot] b))) \[CenterDot] a) == 
          ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 25}, 
         "Construct" -> {"SubstitutionLemma", 39}, "Position" -> {2, 2}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (a_) -> 
           a \[CenterDot] (a \[CenterDot] a), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] 
             ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
                (a \[CenterDot] b))) \[CenterDot] a) == 
            ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] 
                b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
                (a \[CenterDot] b))))], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 49} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
               b) \[CenterDot] (a \[CenterDot] b))) \[CenterDot] a) == 
          ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 48}, 
         "Construct" -> {"SubstitutionLemma", 45}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
              (a_))) -> a \[CenterDot] a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] 
             ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
                (a \[CenterDot] b))) \[CenterDot] a) == 
            ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 26} -> 
      <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
           (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                ((a \[CenterDot] b) \[CenterDot] c)) \[CenterDot] 
               a))) \[CenterDot] (a \[CenterDot] b)) == 
          (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
               c)) \[CenterDot] a)) \[CenterDot] 
           (((a \[CenterDot] b) \[CenterDot] c) \[CenterDot] 
            ((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                  b) \[CenterDot] c)) \[CenterDot] a)) \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] c)))], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 5}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] 
            (((b_) \[CenterDot] (c_)) \[CenterDot] ((((b_) \[CenterDot] 
                (c_)) \[CenterDot] ((b_) \[CenterDot] (((b_) \[CenterDot] 
                  (a_)) \[CenterDot] (b_)))) \[CenterDot] ((b_) \[CenterDot] (
                c_)))) -> b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), 
         "Side" -> 1, "Subpattern" -> ((b_) \[CenterDot] (c_)) \[CenterDot] 
           ((b_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] (b_))), 
         "MatchingConstruct" -> {"CriticalPairLemma", 5}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          (a_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] 
             ((((b_) \[CenterDot] (c_)) \[CenterDot] ((b_) \[CenterDot] 
                (((b_) \[CenterDot] (a_)) \[CenterDot] (b_)))) \[CenterDot] 
              ((b_) \[CenterDot] (c_)))) -> b \[CenterDot] 
            ((b \[CenterDot] a) \[CenterDot] b), "MatchingSide" -> 1, 
         "Position" -> {2, 2, 1}|>|>, {"CriticalPairLemma", 27} -> 
      <|"Statement" -> HoldForm[
         (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                  a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                ((a \[CenterDot] a) \[CenterDot] a)))) \[CenterDot] 
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
           (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] a))) \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] a)))], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 26}, 
         "Orientation" -> 1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] (
                ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
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
              (a_))) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] (
                a_)) \[CenterDot] (a_))) -> a, "MatchingSide" -> 1, 
         "Position" -> {2, 1, 2, 2, 1, 2}|>|>, {"SubstitutionLemma", 50} -> 
      <|"Statement" -> HoldForm[
         (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                  a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                ((a \[CenterDot] a) \[CenterDot] a)))) \[CenterDot] 
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
              (a_))) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] (
                a_)) \[CenterDot] (a_))) -> a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[(a \[CenterDot] ((a \[CenterDot] 
                ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                   a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                    a) \[CenterDot] a)))) \[CenterDot] a)) \[CenterDot] 
             (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                 a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                  a) \[CenterDot] a))) \[CenterDot] ((a \[CenterDot] 
                ((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                      a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                    ((a \[CenterDot] a) \[CenterDot] a)))) \[CenterDot] 
                 a)) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))))) == 
            (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
             (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 51} -> 
      <|"Statement" -> HoldForm[
         (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] a))) \[CenterDot] 
            ((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                  ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                 (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                   a)))) \[CenterDot] a)) \[CenterDot] 
             ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] a))))) == (a \[CenterDot] 
            ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 50}, 
         "Construct" -> {"CriticalPairLemma", 4}, "Position" -> {1, 2, 1, 2}, 
         "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
              (a_))) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] (
                a_)) \[CenterDot] (a_))) -> a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[(a \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] a)) \[CenterDot] (((a \[CenterDot] 
                ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] (
                a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                 a))) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                  ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                     a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                      a) \[CenterDot] a)))) \[CenterDot] a)) \[CenterDot] (
                (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a))))) == (a \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
              (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 52} -> 
      <|"Statement" -> HoldForm[
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
              a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 51}, 
         "Construct" -> {"CriticalPairLemma", 4}, "Position" -> {2, 1}, 
         "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
              (a_))) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] (
                a_)) \[CenterDot] (a_))) -> a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[(a \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
              ((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                    ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                   (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                     a)))) \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a))))) == 
            (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
             (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 53} -> 
      <|"Statement" -> HoldForm[
         (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a)) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] (
                (a \[CenterDot] a) \[CenterDot] a))))) == 
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 52}, 
         "Construct" -> {"CriticalPairLemma", 4}, "Position" -> {2, 2, 1, 2, 
          1, 2}, "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (
                a_)) \[CenterDot] (a_))) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (a_))) -> a, 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
             (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                  a) \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a))))) == 
            (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
             (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 54} -> 
      <|"Statement" -> HoldForm[
         (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a)) \[CenterDot] a)) == (a \[CenterDot] 
            ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 53}, 
         "Construct" -> {"CriticalPairLemma", 4}, "Position" -> {2, 2, 2}, 
         "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
              (a_))) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] (
                a_)) \[CenterDot] (a_))) -> a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[(a \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
              ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                 a)) \[CenterDot] a)) == (a \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
              (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 55} -> 
      <|"Statement" -> HoldForm[
         (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a)) \[CenterDot] a)) == a], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 54}, "Construct" -> 
          {"SubstitutionLemma", 32}, "Position" -> {}, 
         "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
              (a_))) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
              (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) -> a, 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
             (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                  a) \[CenterDot] a)) \[CenterDot] a)) == a], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 56} -> 
      <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
           (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a)) \[CenterDot] a)) == a], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 55}, "Construct" -> 
          {"SubstitutionLemma", 33}, "Position" -> {1}, 
         "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (a_)) -> a \[CenterDot] a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                  a) \[CenterDot] a)) \[CenterDot] a)) == a], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 57} -> 
      <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
           (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) == a], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 56}, 
         "Construct" -> {"SubstitutionLemma", 33}, "Position" -> {2, 2, 1}, 
         "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (a_)) -> a \[CenterDot] a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) == a], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 58} -> 
      <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
           (a \[CenterDot] a) == a], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 57}, "Construct" -> 
          {"SubstitutionLemma", 33}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (a_)) -> a \[CenterDot] a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] a) == a], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 59} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
               b) \[CenterDot] (a \[CenterDot] b))) \[CenterDot] a) == 
          a \[CenterDot] b], "Proof" -> <|"Input" -> {"SubstitutionLemma", 
           49}, "Construct" -> {"SubstitutionLemma", 58}, "Position" -> {}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
             (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                 b) \[CenterDot] (a \[CenterDot] b))) \[CenterDot] a) == 
            a \[CenterDot] b], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 28} -> <|"Statement" -> 
        HoldForm[(a \[CenterDot] b) \[CenterDot] 
           ((c \[CenterDot] a) \[CenterDot] (((c \[CenterDot] a) \[CenterDot] 
              b) \[CenterDot] (c \[CenterDot] a))) == 
          (a \[CenterDot] b) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
             (b \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (
                (c \[CenterDot] a) \[CenterDot] (((c \[CenterDot] 
                   a) \[CenterDot] b) \[CenterDot] (c \[CenterDot] 
                  a)))))) \[CenterDot] (a \[CenterDot] b))], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 59}, 
         "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] 
            (((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] (
                (a_) \[CenterDot] (b_)))) \[CenterDot] (a_)) -> 
           a \[CenterDot] b, "Side" -> 1, "Subpattern" -> 
          (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 10}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            (((c_) \[CenterDot] (a_)) \[CenterDot] ((((c_) \[CenterDot] 
                (a_)) \[CenterDot] (b_)) \[CenterDot] ((c_) \[CenterDot] (
                a_)))) -> b, "MatchingSide" -> 1, "Position" -> {2, 1, 2, 
          1}|>|>, {"SubstitutionLemma", 60} -> 
      <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
           ((c \[CenterDot] a) \[CenterDot] (((c \[CenterDot] a) \[CenterDot] 
              b) \[CenterDot] (c \[CenterDot] a))) == 
          (a \[CenterDot] b) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
             (b \[CenterDot] b)) \[CenterDot] (a \[CenterDot] b))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 28}, 
         "Construct" -> {"SubstitutionLemma", 10}, "Position" -> {2, 1, 2, 
          2}, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            (((c_) \[CenterDot] (a_)) \[CenterDot] ((((c_) \[CenterDot] 
                (a_)) \[CenterDot] (b_)) \[CenterDot] ((c_) \[CenterDot] (
                a_)))) -> b, "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[(a \[CenterDot] b) \[CenterDot] ((c \[CenterDot] 
               a) \[CenterDot] (((c \[CenterDot] a) \[CenterDot] 
                b) \[CenterDot] (c \[CenterDot] a))) == 
            (a \[CenterDot] b) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
               (b \[CenterDot] b)) \[CenterDot] (a \[CenterDot] b))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 61} -> 
      <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
           (((a \[CenterDot] b) \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
            (a \[CenterDot] b))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 60}, "Construct" -> 
          {"SubstitutionLemma", 10}, "Position" -> {}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            (((c_) \[CenterDot] (a_)) \[CenterDot] ((((c_) \[CenterDot] 
                (a_)) \[CenterDot] (b_)) \[CenterDot] ((c_) \[CenterDot] (
                a_)))) -> b, "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
             (((a \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
                b)) \[CenterDot] (a \[CenterDot] b))], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 29} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] ((a \[CenterDot] 
             c) \[CenterDot] a) == (((a \[CenterDot] b) \[CenterDot] 
             c) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] 
              a))) \[CenterDot] ((c \[CenterDot] ((a \[CenterDot] (
                (a \[CenterDot] c) \[CenterDot] a)) \[CenterDot] 
              (a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] 
                a)))) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
              c) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                c) \[CenterDot] a))))], "Proof" -> 
        <|"Construct" -> {"SubstitutionLemma", 61}, "Orientation" -> -1, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] (
                b_))) \[CenterDot] ((a_) \[CenterDot] (b_))) -> b, 
         "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] (b_), 
         "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] (
                c_)) \[CenterDot] (a_))) -> c, "MatchingSide" -> 1, 
         "Position" -> {2, 1, 1}|>|>, {"SubstitutionLemma", 62} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] ((a \[CenterDot] 
             c) \[CenterDot] a) == c \[CenterDot] 
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
                 ((a \[CenterDot] c) \[CenterDot] a)) \[CenterDot] 
                (a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] 
                  a)))) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
                c) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                  c) \[CenterDot] a))))], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 30} -> <|"Statement" -> 
        HoldForm[a == ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] (a \[CenterDot] a)))], 
       "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
         "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
            ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
              (a_))) -> c, "Side" -> 1, "Subpattern" -> 
          ((a_) \[CenterDot] (c_)) \[CenterDot] (a_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 39}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (a_) -> 
           a \[CenterDot] (a \[CenterDot] a), "MatchingSide" -> 1, 
         "Position" -> {2, 2}|>|>, {"SubstitutionLemma", 63} -> 
      <|"Statement" -> HoldForm[a == ((a \[CenterDot] b) \[CenterDot] 
            a) \[CenterDot] (a \[CenterDot] a)], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 30}, 
         "Construct" -> {"SubstitutionLemma", 45}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
              (a_))) -> a \[CenterDot] a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a == ((a \[CenterDot] b) \[CenterDot] 
              a) \[CenterDot] (a \[CenterDot] a)], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 31} -> <|"Statement" -> 
        HoldForm[b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b) == 
          a \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
              b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
               a) \[CenterDot] b)))], "Proof" -> 
        <|"Construct" -> {"SubstitutionLemma", 63}, "Orientation" -> -1, 
         "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] (a_)) -> a, "Side" -> 1, 
         "Subpattern" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_), 
         "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] (
                c_)) \[CenterDot] (a_))) -> c, "MatchingSide" -> 1, 
         "Position" -> {1}|>|>, {"SubstitutionLemma", 64} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] ((a \[CenterDot] 
             c) \[CenterDot] a) == c \[CenterDot] 
           ((a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] a)) \[CenterDot] 
            (((a \[CenterDot] b) \[CenterDot] c) \[CenterDot] 
             (a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] a))))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 62}, 
         "Construct" -> {"CriticalPairLemma", 31}, "Position" -> {2, 1}, 
         "Rule" -> (a_) \[CenterDot] (((b_) \[CenterDot] (((b_) \[CenterDot] 
                (a_)) \[CenterDot] (b_))) \[CenterDot] ((b_) \[CenterDot] 
              (((b_) \[CenterDot] (a_)) \[CenterDot] (b_)))) -> 
           b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] a) == 
            c \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] 
                a)) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
                c) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                  c) \[CenterDot] a))))], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 65} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] a) == 
          c \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] 
              a)) \[CenterDot] c)], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 64}, "Construct" -> {"Axiom", 1}, 
         "Position" -> {2, 2}, "Rule" -> 
          (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
            ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
              (a_))) -> c, "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] a) == 
            c \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] 
                a)) \[CenterDot] c)], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 66} -> <|"Statement" -> 
        HoldForm[(b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
             b)) \[CenterDot] a == a \[CenterDot] (a \[CenterDot] 
            ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b)) \[CenterDot] a))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 36}, "Construct" -> 
          {"SubstitutionLemma", 65}, "Position" -> {2, 2, 1}, 
         "Rule" -> (a_) \[CenterDot] (((b_) \[CenterDot] (((b_) \[CenterDot] 
                (a_)) \[CenterDot] (b_))) \[CenterDot] (a_)) -> 
           b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
             a == a \[CenterDot] (a \[CenterDot] ((b \[CenterDot] 
                ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] a))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 67} -> 
      <|"Statement" -> HoldForm[
         (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           a == a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
              a) \[CenterDot] b))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 66}, "Construct" -> 
          {"SubstitutionLemma", 65}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] (((b_) \[CenterDot] (((b_) \[CenterDot] 
                (a_)) \[CenterDot] (b_))) \[CenterDot] (a_)) -> 
           b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
             a == a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                a) \[CenterDot] b))], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 68} -> <|"Statement" -> 
        HoldForm[b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b) == 
          a \[CenterDot] (a \[CenterDot] (b \[CenterDot] 
             ((b \[CenterDot] a) \[CenterDot] b)))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 65}, 
         "Construct" -> {"SubstitutionLemma", 67}, "Position" -> {2}, 
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
             a))], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 61}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] (
                b_))) \[CenterDot] ((a_) \[CenterDot] (b_))) -> b, 
         "Side" -> 1, "Subpattern" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           ((b_) \[CenterDot] (b_)), "MatchingConstruct" -> 
          {"SubstitutionLemma", 63}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_)) \[CenterDot] ((a_) \[CenterDot] (a_)) -> a, 
         "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
     {"CriticalPairLemma", 33} -> <|"Statement" -> 
        HoldForm[(a \[CenterDot] b) \[CenterDot] a == 
          ((((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
              b) \[CenterDot] a)) \[CenterDot] 
           (((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)))], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 32}, 
         "Orientation" -> -1, "Rule" -> 
          (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
              (a_))) -> a, "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] 
           (b_), "MatchingConstruct" -> {"SubstitutionLemma", 63}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] (a_)) -> a, "MatchingSide" -> 1, 
         "Position" -> {2, 2, 1}|>|>, {"SubstitutionLemma", 69} -> 
      <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] a == 
          (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
           (((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 33}, 
         "Construct" -> {"SubstitutionLemma", 63}, "Position" -> {1, 1}, 
         "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] (a_)) -> a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[(a \[CenterDot] b) \[CenterDot] a == 
            (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
             (((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 70} -> 
      <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] a == 
          (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
           a], "Proof" -> <|"Input" -> {"SubstitutionLemma", 69}, 
         "Construct" -> {"CriticalPairLemma", 32}, "Position" -> {2}, 
         "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
              (a_))) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[(a \[CenterDot] b) \[CenterDot] a == 
            (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
             a], "Source" -> "norm"|>|>, {"CriticalPairLemma", 34} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] ((a \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] a) == 
          ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
           (((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)))], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 68}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
             ((b_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] (
                b_)))) -> b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), 
         "Side" -> 1, "Subpattern" -> ((b_) \[CenterDot] (a_)) \[CenterDot] 
           (b_), "MatchingConstruct" -> {"SubstitutionLemma", 70}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
              (a_))) \[CenterDot] (a_) -> (a \[CenterDot] b) \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {2, 2, 2}|>|>, 
     {"SubstitutionLemma", 71} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
               b) \[CenterDot] a)) \[CenterDot] a) == 
          ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] a], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 34}, 
         "Construct" -> {"CriticalPairLemma", 32}, "Position" -> {2}, 
         "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
              (a_))) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                 b) \[CenterDot] a)) \[CenterDot] a) == 
            ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] a], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 72} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] ((a \[CenterDot] 
             b) \[CenterDot] a) == ((a \[CenterDot] b) \[CenterDot] 
            a) \[CenterDot] a], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 71}, "Construct" -> 
          {"SubstitutionLemma", 70}, "Position" -> {2}, 
         "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
              (a_))) \[CenterDot] (a_) -> (a \[CenterDot] b) \[CenterDot] a, 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a) == 
            ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] a], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 35} -> 
      <|"Statement" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
           (((b \[CenterDot] a) \[CenterDot] ((c \[CenterDot] b) \[CenterDot] 
              (((c \[CenterDot] b) \[CenterDot] a) \[CenterDot] (
                c \[CenterDot] b)))) \[CenterDot] (b \[CenterDot] a)) == 
          (a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
           (b \[CenterDot] a)], "Proof" -> 
        <|"Construct" -> {"SubstitutionLemma", 72}, "Orientation" -> -1, 
         "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
            (a_) -> a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a), 
         "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] (b_), 
         "MatchingConstruct" -> {"SubstitutionLemma", 10}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          ((a_) \[CenterDot] (b_)) \[CenterDot] (((c_) \[CenterDot] 
              (a_)) \[CenterDot] ((((c_) \[CenterDot] (a_)) \[CenterDot] (
                b_)) \[CenterDot] ((c_) \[CenterDot] (a_)))) -> b, 
         "MatchingSide" -> 1, "Position" -> {1, 1}|>|>, 
     {"SubstitutionLemma", 73} -> <|"Statement" -> 
        HoldForm[(b \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] a)) == (a \[CenterDot] (b \[CenterDot] 
             a)) \[CenterDot] (b \[CenterDot] a)], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 35}, 
         "Construct" -> {"SubstitutionLemma", 10}, "Position" -> {2, 1}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            (((c_) \[CenterDot] (a_)) \[CenterDot] ((((c_) \[CenterDot] 
                (a_)) \[CenterDot] (b_)) \[CenterDot] ((c_) \[CenterDot] (
                a_)))) -> b, "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[(b \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
              (b \[CenterDot] a)) == (a \[CenterDot] (b \[CenterDot] 
               a)) \[CenterDot] (b \[CenterDot] a)], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 36} -> <|"Statement" -> 
        HoldForm[(a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                b) \[CenterDot] (a \[CenterDot] b))) \[CenterDot] 
             a)) \[CenterDot] (((a \[CenterDot] ((a \[CenterDot] 
                b) \[CenterDot] (a \[CenterDot] b))) \[CenterDot] 
             a) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] (
                (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                 b))) \[CenterDot] a))) == 
          (((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                b))) \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
                (a \[CenterDot] b))) \[CenterDot] a))) \[CenterDot] 
           (a \[CenterDot] b)], "Proof" -> 
        <|"Construct" -> {"SubstitutionLemma", 73}, "Orientation" -> -1, 
         "Rule" -> ((a_) \[CenterDot] ((b_) \[CenterDot] (a_))) \[CenterDot] 
            ((b_) \[CenterDot] (a_)) -> (b \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] (b \[CenterDot] a)), "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] (a_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 59}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (a_) \[CenterDot] (((a_) \[CenterDot] 
              (((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
                (b_)))) \[CenterDot] (a_)) -> a \[CenterDot] b, 
         "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"SubstitutionLemma", 74} -> <|"Statement" -> 
        HoldForm[(a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                b) \[CenterDot] (a \[CenterDot] b))) \[CenterDot] 
             a)) \[CenterDot] (((a \[CenterDot] ((a \[CenterDot] 
                b) \[CenterDot] (a \[CenterDot] b))) \[CenterDot] 
             a) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] (
                (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                 b))) \[CenterDot] a))) == a \[CenterDot] 
           (a \[CenterDot] b)], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 36}, "Construct" -> 
          {"CriticalPairLemma", 32}, "Position" -> {1}, 
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
     {"SubstitutionLemma", 75} -> <|"Statement" -> 
        HoldForm[(a \[CenterDot] b) \[CenterDot] 
           (((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                b))) \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
                (a \[CenterDot] b))) \[CenterDot] a))) == 
          a \[CenterDot] (a \[CenterDot] b)], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 74}, "Construct" -> 
          {"SubstitutionLemma", 59}, "Position" -> {1}, 
         "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (((a_) \[CenterDot] 
                (b_)) \[CenterDot] ((a_) \[CenterDot] (b_)))) \[CenterDot] 
             (a_)) -> a \[CenterDot] b, "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
             (((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
                 (a \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                   b) \[CenterDot] (a \[CenterDot] b))) \[CenterDot] a))) == 
            a \[CenterDot] (a \[CenterDot] b)], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 76} -> <|"Statement" -> 
        HoldForm[(a \[CenterDot] b) \[CenterDot] a == a \[CenterDot] 
           (a \[CenterDot] b)], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 75}, "Construct" -> 
          {"CriticalPairLemma", 32}, "Position" -> {2}, 
         "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
              (a_))) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[(a \[CenterDot] b) \[CenterDot] a == a \[CenterDot] 
             (a \[CenterDot] b)], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 77} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)))) == 
          a \[CenterDot] b], "Proof" -> <|"Input" -> {"SubstitutionLemma", 
           59}, "Construct" -> {"SubstitutionLemma", 76}, "Position" -> {2}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
           a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
              (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
                (a \[CenterDot] b)))) == a \[CenterDot] b], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 78} -> 
      <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] a == 
          a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             a))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 70}, 
         "Construct" -> {"SubstitutionLemma", 76}, "Position" -> {}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
           a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[(a \[CenterDot] b) \[CenterDot] a == 
            a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
               a))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 79} -> 
      <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] a == 
          a \[CenterDot] (a \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
              b)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 78}, 
         "Construct" -> {"SubstitutionLemma", 76}, "Position" -> {2, 2}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
           a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[(a \[CenterDot] b) \[CenterDot] a == 
            a \[CenterDot] (a \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
                b)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 80} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] b) == 
          a \[CenterDot] (a \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
              b)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 79}, 
         "Construct" -> {"SubstitutionLemma", 76}, "Position" -> {}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
           a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[a \[CenterDot] (a \[CenterDot] b) == 
            a \[CenterDot] (a \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
                b)))], "Source" -> "norm"|>|>, {"CriticalPairLemma", 37} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] b)))) == a \[CenterDot] (a \[CenterDot] 
            (a \[CenterDot] b))], "Proof" -> 
        <|"Construct" -> {"SubstitutionLemma", 80}, "Orientation" -> -1, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
              ((a_) \[CenterDot] (b_)))) -> a \[CenterDot] 
            (a \[CenterDot] b), "Side" -> 1, "Subpattern" -> 
          (a_) \[CenterDot] ((a_) \[CenterDot] (b_)), "MatchingConstruct" -> 
          {"SubstitutionLemma", 77}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
             ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] (
                (a_) \[CenterDot] (b_))))) -> a \[CenterDot] b, 
         "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
     {"SubstitutionLemma", 81} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] b == a \[CenterDot] (a \[CenterDot] 
            (a \[CenterDot] b))], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 37}, "Construct" -> 
          {"SubstitutionLemma", 77}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
              (((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
                (b_))))) -> a \[CenterDot] b, "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[a \[CenterDot] b == 
            a \[CenterDot] (a \[CenterDot] (a \[CenterDot] b))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 82} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] ((a \[CenterDot] 
             b) \[CenterDot] (a \[CenterDot] b)) == a \[CenterDot] b], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 77}, 
         "Construct" -> {"SubstitutionLemma", 81}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
              (b_))) -> a \[CenterDot] b, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[a \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) == 
            a \[CenterDot] b], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 38} -> <|"Statement" -> 
        HoldForm[b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] b) == 
          (((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
             b) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
              b))) \[CenterDot] (b \[CenterDot] (b \[CenterDot] 
             ((b \[CenterDot] b) \[CenterDot] b)))], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 7}, 
         "Orientation" -> -1, "Rule" -> 
          ((((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
                (a_))) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
            ((b_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] 
              (b_))) -> c, "Side" -> 1, "Subpattern" -> 
          ((b_) \[CenterDot] (c_)) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"CriticalPairLemma", 3}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] 
              (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) \[CenterDot] 
            (a_) -> a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
         "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
     {"SubstitutionLemma", 84} -> <|"Statement" -> 
        HoldForm[b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] b) == 
          b \[CenterDot] (b \[CenterDot] (b \[CenterDot] 
             ((b \[CenterDot] b) \[CenterDot] b)))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 38}, 
         "Construct" -> {"CriticalPairLemma", 6}, "Position" -> {1}, 
         "Rule" -> (((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
               (a_))) \[CenterDot] (c_)) \[CenterDot] ((b_) \[CenterDot] 
             (((b_) \[CenterDot] (c_)) \[CenterDot] (b_))) -> c, 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] b) == 
            b \[CenterDot] (b \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                 b) \[CenterDot] b)))], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 85} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a) == 
          a \[CenterDot] (a \[CenterDot] (a \[CenterDot] a))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 84}, 
         "Construct" -> {"SubstitutionLemma", 33}, "Position" -> {2, 2}, 
         "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (a_)) -> a \[CenterDot] a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] a) == a \[CenterDot] 
             (a \[CenterDot] (a \[CenterDot] a))], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 39} -> <|"Statement" -> 
        HoldForm[b == (((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
               a)) \[CenterDot] c) \[CenterDot] b) \[CenterDot] 
           ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
            b)], "Proof" -> <|"Construct" -> {"Axiom", 1}, 
         "Orientation" -> 1, "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] (
                c_)) \[CenterDot] (a_))) -> c, "Side" -> 1, 
         "Subpattern" -> ((a_) \[CenterDot] (c_)) \[CenterDot] (a_), 
         "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] (
                c_)) \[CenterDot] (a_))) -> c, "MatchingSide" -> 1, 
         "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 40} -> 
      <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
           (((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b)) \[CenterDot] a) \[CenterDot] a)], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 10}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((((c_) \[CenterDot] (x3_)) \[CenterDot] (a_)) \[CenterDot] 
             (((((c_) \[CenterDot] (x3_)) \[CenterDot] (a_)) \[CenterDot] (
                b_)) \[CenterDot] (((c_) \[CenterDot] (x3_)) \[CenterDot] (
                a_)))) -> b, "Side" -> 1, "Subpattern" -> 
          ((((c_) \[CenterDot] (x3_)) \[CenterDot] (a_)) \[CenterDot] 
            (b_)) \[CenterDot] (((c_) \[CenterDot] (x3_)) \[CenterDot] (a_)), 
         "MatchingConstruct" -> {"CriticalPairLemma", 39}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          ((((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
                (a_))) \[CenterDot] (c_)) \[CenterDot] (b_)) \[CenterDot] 
            (((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] (
                a_))) \[CenterDot] (b_)) -> b, "MatchingSide" -> 1, 
         "Position" -> {2, 2}|>|>, {"SubstitutionLemma", 91} -> 
      <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
           ((a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b))) \[CenterDot] a)], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 40}, "Construct" -> 
          {"SubstitutionLemma", 67}, "Position" -> {2, 1}, 
         "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
              (a_))) \[CenterDot] (b_) -> b \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] a)), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
             ((a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                  a) \[CenterDot] b))) \[CenterDot] a)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 92} -> 
      <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                a) \[CenterDot] b))))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 91}, "Construct" -> 
          {"SubstitutionLemma", 76}, "Position" -> {2}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
           a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] 
                ((b \[CenterDot] a) \[CenterDot] b))))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 93} -> 
      <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] (
                b \[CenterDot] a)))))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 92}, "Construct" -> 
          {"SubstitutionLemma", 76}, "Position" -> {2, 2, 2, 2}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
           a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] 
                 (b \[CenterDot] a)))))], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 41} -> <|"Statement" -> 
        HoldForm[((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
              b)) \[CenterDot] (((b \[CenterDot] ((b \[CenterDot] 
                 a) \[CenterDot] b)) \[CenterDot] a) \[CenterDot] 
             (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b)))) \[CenterDot] a == a \[CenterDot] (a \[CenterDot] 
            ((a \[CenterDot] (((b \[CenterDot] ((b \[CenterDot] 
                   a) \[CenterDot] b)) \[CenterDot] a) \[CenterDot] 
               a)) \[CenterDot] a))], "Proof" -> 
        <|"Construct" -> {"SubstitutionLemma", 36}, "Orientation" -> -1, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] (((b_) \[CenterDot] (((b_) \[CenterDot] 
                   (a_)) \[CenterDot] (b_))) \[CenterDot] (a_))) \[CenterDot] 
              (a_))) -> (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
              b)) \[CenterDot] a, "Side" -> 1, "Subpattern" -> 
          ((b_) \[CenterDot] (a_)) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"Axiom", 1}, "MatchingOrientation" -> 1, "MatchingRule" -> 
          (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
            ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
              (a_))) -> c, "MatchingSide" -> 1, "Position" -> {2, 2, 1, 2, 1, 
          2}|>|>, {"SubstitutionLemma", 94} -> 
      <|"Statement" -> HoldForm[
         ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
            a) \[CenterDot] a == a \[CenterDot] (a \[CenterDot] 
            ((a \[CenterDot] (((b \[CenterDot] ((b \[CenterDot] 
                   a) \[CenterDot] b)) \[CenterDot] a) \[CenterDot] 
               a)) \[CenterDot] a))], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 41}, "Construct" -> {"Axiom", 1}, 
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
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 42} -> 
      <|"Statement" -> HoldForm[
         ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
            a) \[CenterDot] a == (a \[CenterDot] 
            (((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                b)) \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
             (a \[CenterDot] a)))], "Proof" -> 
        <|"Construct" -> {"SubstitutionLemma", 10}, "Orientation" -> -1, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            (((c_) \[CenterDot] (a_)) \[CenterDot] ((((c_) \[CenterDot] 
                (a_)) \[CenterDot] (b_)) \[CenterDot] ((c_) \[CenterDot] (
                a_)))) -> b, "Side" -> 1, "Subpattern" -> 
          ((c_) \[CenterDot] (a_)) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"CriticalPairLemma", 40}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((((b_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] 
                (b_))) \[CenterDot] (a_)) \[CenterDot] (a_)) -> a, 
         "MatchingSide" -> 1, "Position" -> {2, 2, 1}|>|>, 
     {"SubstitutionLemma", 95} -> <|"Statement" -> 
        HoldForm[((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
              b)) \[CenterDot] a) \[CenterDot] a == 
          (a \[CenterDot] (((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                b)) \[CenterDot] a) \[CenterDot] a)) \[CenterDot] a], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 42}, 
         "Construct" -> {"SubstitutionLemma", 40}, "Position" -> {2}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
             ((a_) \[CenterDot] (a_))) -> a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[
           ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
              a) \[CenterDot] a == (a \[CenterDot] (((b \[CenterDot] 
                 ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
                a) \[CenterDot] a)) \[CenterDot] a], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 96} -> <|"Statement" -> 
        HoldForm[((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
              b)) \[CenterDot] a) \[CenterDot] a == a \[CenterDot] 
           (a \[CenterDot] (((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                b)) \[CenterDot] a) \[CenterDot] a))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 94}, 
         "Construct" -> {"SubstitutionLemma", 95}, "Position" -> {2, 2}, 
         "Rule" -> ((a_) \[CenterDot] ((((b_) \[CenterDot] 
                (((b_) \[CenterDot] (a_)) \[CenterDot] (b_))) \[CenterDot] (
                a_)) \[CenterDot] (a_))) \[CenterDot] (a_) -> 
           ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
             a) \[CenterDot] a, "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                b)) \[CenterDot] a) \[CenterDot] a == a \[CenterDot] 
             (a \[CenterDot] (((b \[CenterDot] ((b \[CenterDot] 
                   a) \[CenterDot] b)) \[CenterDot] a) \[CenterDot] a))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 97} -> 
      <|"Statement" -> HoldForm[
         ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
            a) \[CenterDot] a == a \[CenterDot] (a \[CenterDot] 
            ((a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                b))) \[CenterDot] a))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 96}, "Construct" -> 
          {"SubstitutionLemma", 67}, "Position" -> {2, 2, 1}, 
         "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
              (a_))) \[CenterDot] (b_) -> b \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] a)), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[
           ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
              a) \[CenterDot] a == a \[CenterDot] (a \[CenterDot] 
              ((a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                   a) \[CenterDot] b))) \[CenterDot] a))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 98} -> 
      <|"Statement" -> HoldForm[
         ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
            a) \[CenterDot] a == b \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] b)], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 97}, 
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
     {"SubstitutionLemma", 99} -> <|"Statement" -> 
        HoldForm[(a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
               a) \[CenterDot] b))) \[CenterDot] a == b \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] b)], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 98}, 
         "Construct" -> {"SubstitutionLemma", 67}, "Position" -> {1}, 
         "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
              (a_))) \[CenterDot] (b_) -> b \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] a)), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[(a \[CenterDot] (b \[CenterDot] (
                (b \[CenterDot] a) \[CenterDot] b))) \[CenterDot] a == 
            b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 100} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b))) == 
          b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 99}, 
         "Construct" -> {"SubstitutionLemma", 76}, "Position" -> {}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
           a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
              (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b))) == 
            b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 101} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] (b \[CenterDot] (b \[CenterDot] a)))) == 
          b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 100}, 
         "Construct" -> {"SubstitutionLemma", 76}, "Position" -> {2, 2, 2}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
           a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
              (b \[CenterDot] (b \[CenterDot] (b \[CenterDot] a)))) == 
            b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 102} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] (b \[CenterDot] (b \[CenterDot] a)))) == 
          b \[CenterDot] (b \[CenterDot] (b \[CenterDot] a))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 101}, 
         "Construct" -> {"SubstitutionLemma", 76}, "Position" -> {2}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
           a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
              (b \[CenterDot] (b \[CenterDot] (b \[CenterDot] a)))) == 
            b \[CenterDot] (b \[CenterDot] (b \[CenterDot] a))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 103} -> 
      <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
           (b \[CenterDot] (b \[CenterDot] (b \[CenterDot] a)))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 93}, 
         "Construct" -> {"SubstitutionLemma", 102}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] 
              ((b_) \[CenterDot] ((b_) \[CenterDot] (a_))))) -> 
           b \[CenterDot] (b \[CenterDot] (b \[CenterDot] a)), 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           a == (a \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
              (b \[CenterDot] (b \[CenterDot] a)))], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 104} -> 
      <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
           (b \[CenterDot] a)], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 103}, "Construct" -> 
          {"SubstitutionLemma", 81}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
              (b_))) -> a \[CenterDot] b, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
             (b \[CenterDot] a)], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 107} -> 
      <|"Statement" -> HoldForm[(c \[CenterDot] b) \[CenterDot] 
           (((c \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            (c \[CenterDot] b)) == a \[CenterDot] (b \[CenterDot] 
            (b \[CenterDot] (b \[CenterDot] ((c \[CenterDot] b) \[CenterDot] (
                ((c \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
                (c \[CenterDot] b))))))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 12}, "Construct" -> 
          {"SubstitutionLemma", 76}, "Position" -> {2, 2}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
           a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[(c \[CenterDot] b) \[CenterDot] 
             (((c \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
              (c \[CenterDot] b)) == a \[CenterDot] (b \[CenterDot] 
              (b \[CenterDot] (b \[CenterDot] ((c \[CenterDot] 
                  b) \[CenterDot] (((c \[CenterDot] b) \[CenterDot] 
                   a) \[CenterDot] (c \[CenterDot] b))))))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 108} -> 
      <|"Statement" -> HoldForm[(c \[CenterDot] b) \[CenterDot] 
           (((c \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            (c \[CenterDot] b)) == a \[CenterDot] (b \[CenterDot] 
            (b \[CenterDot] (b \[CenterDot] ((c \[CenterDot] b) \[CenterDot] (
                (c \[CenterDot] b) \[CenterDot] ((c \[CenterDot] 
                  b) \[CenterDot] a))))))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 107}, "Construct" -> 
          {"SubstitutionLemma", 76}, "Position" -> {2, 2, 2, 2, 2}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
           a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[(c \[CenterDot] b) \[CenterDot] 
             (((c \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
              (c \[CenterDot] b)) == a \[CenterDot] (b \[CenterDot] 
              (b \[CenterDot] (b \[CenterDot] ((c \[CenterDot] 
                  b) \[CenterDot] ((c \[CenterDot] b) \[CenterDot] 
                  ((c \[CenterDot] b) \[CenterDot] a))))))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 109} -> 
      <|"Statement" -> HoldForm[(c \[CenterDot] b) \[CenterDot] 
           (((c \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            (c \[CenterDot] b)) == a \[CenterDot] (b \[CenterDot] 
            ((c \[CenterDot] b) \[CenterDot] ((c \[CenterDot] b) \[CenterDot] 
              ((c \[CenterDot] b) \[CenterDot] a))))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 108}, 
         "Construct" -> {"SubstitutionLemma", 81}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
              (b_))) -> a \[CenterDot] b, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[(c \[CenterDot] b) \[CenterDot] 
             (((c \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
              (c \[CenterDot] b)) == a \[CenterDot] (b \[CenterDot] 
              ((c \[CenterDot] b) \[CenterDot] ((c \[CenterDot] 
                 b) \[CenterDot] ((c \[CenterDot] b) \[CenterDot] a))))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 110} -> 
      <|"Statement" -> HoldForm[(c \[CenterDot] b) \[CenterDot] 
           (((c \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            (c \[CenterDot] b)) == a \[CenterDot] (b \[CenterDot] 
            ((c \[CenterDot] b) \[CenterDot] a))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 109}, 
         "Construct" -> {"SubstitutionLemma", 81}, "Position" -> {2, 2}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
              (b_))) -> a \[CenterDot] b, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[(c \[CenterDot] b) \[CenterDot] 
             (((c \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
              (c \[CenterDot] b)) == a \[CenterDot] (b \[CenterDot] 
              ((c \[CenterDot] b) \[CenterDot] a))], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 111} -> 
      <|"Statement" -> HoldForm[(c \[CenterDot] b) \[CenterDot] 
           ((c \[CenterDot] b) \[CenterDot] ((c \[CenterDot] b) \[CenterDot] 
             a)) == a \[CenterDot] (b \[CenterDot] 
            ((c \[CenterDot] b) \[CenterDot] a))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 110}, 
         "Construct" -> {"SubstitutionLemma", 76}, "Position" -> {2}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
           a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[(c \[CenterDot] b) \[CenterDot] 
             ((c \[CenterDot] b) \[CenterDot] ((c \[CenterDot] 
                b) \[CenterDot] a)) == a \[CenterDot] (b \[CenterDot] 
              ((c \[CenterDot] b) \[CenterDot] a))], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 112} -> 
      <|"Statement" -> HoldForm[(c \[CenterDot] b) \[CenterDot] a == 
          a \[CenterDot] (b \[CenterDot] ((c \[CenterDot] b) \[CenterDot] 
             a))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 111}, 
         "Construct" -> {"SubstitutionLemma", 81}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
              (b_))) -> a \[CenterDot] b, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[(c \[CenterDot] b) \[CenterDot] a == 
            a \[CenterDot] (b \[CenterDot] ((c \[CenterDot] b) \[CenterDot] 
               a))], "Source" -> "norm"|>|>, {"CriticalPairLemma", 43} -> 
      <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] b))], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 104}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((b_) \[CenterDot] (a_)) -> a, "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] (a_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 76}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
           a \[CenterDot] (a \[CenterDot] b), "MatchingSide" -> 1, 
         "Position" -> {2}|>|>, {"CriticalPairLemma", 44} -> 
      <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
           (a \[CenterDot] b)], "Proof" -> 
        <|"Construct" -> {"CriticalPairLemma", 43}, "Orientation" -> -1, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
             ((a_) \[CenterDot] (b_))) -> a, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] ((a_) \[CenterDot] (b_)), 
         "MatchingConstruct" -> {"SubstitutionLemma", 81}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] (b_))) -> 
           a \[CenterDot] b, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"CriticalPairLemma", 45} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] a == a \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] b)], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 44}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] (b_)) -> a, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (a_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 104}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((b_) \[CenterDot] (a_)) -> a, "MatchingSide" -> 1, 
         "Position" -> {1}|>|>, {"CriticalPairLemma", 46} -> 
      <|"Statement" -> HoldForm[(b \[CenterDot] b) \[CenterDot] a == 
          a \[CenterDot] (b \[CenterDot] b)], "Proof" -> 
        <|"Construct" -> {"SubstitutionLemma", 112}, "Orientation" -> -1, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             (((c_) \[CenterDot] (b_)) \[CenterDot] (a_))) -> 
           (c \[CenterDot] b) \[CenterDot] a, "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] (((c_) \[CenterDot] 
             (b_)) \[CenterDot] (a_)), "MatchingConstruct" -> 
          {"CriticalPairLemma", 45}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> (a_) \[CenterDot] (((a_) \[CenterDot] 
              (a_)) \[CenterDot] (b_)) -> a \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"CriticalPairLemma", 47} -> <|"Statement" -> 
        HoldForm[b \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] a)) == a \[CenterDot] b], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 46}, 
         "Orientation" -> 1, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            (b_) -> b \[CenterDot] (a \[CenterDot] a), "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (a_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 104}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((b_) \[CenterDot] (a_)) -> a, "MatchingSide" -> 1, 
         "Position" -> {1}|>|>, {"SubstitutionLemma", 113} -> 
      <|"Statement" -> HoldForm[b \[CenterDot] a == a \[CenterDot] b], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 47}, 
         "Construct" -> {"SubstitutionLemma", 104}, "Position" -> {2}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
             (a_)) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[b \[CenterDot] a == a \[CenterDot] b], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 122} -> 
      <|"Statement" -> HoldForm[a == (a \[CenterDot] (a \[CenterDot] 
             b)) \[CenterDot] (a \[CenterDot] a)], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 63}, 
         "Construct" -> {"SubstitutionLemma", 76}, "Position" -> {1}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
           a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a == (a \[CenterDot] 
              (a \[CenterDot] b)) \[CenterDot] (a \[CenterDot] a)], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 48} -> 
      <|"Statement" -> HoldForm[a == (a \[CenterDot] b) \[CenterDot] 
           (a \[CenterDot] a)], "Proof" -> 
        <|"Construct" -> {"SubstitutionLemma", 122}, "Orientation" -> -1, 
         "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] (b_))) \[CenterDot] 
            ((a_) \[CenterDot] (a_)) -> a, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] ((a_) \[CenterDot] (b_)), 
         "MatchingConstruct" -> {"SubstitutionLemma", 81}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] (b_))) -> 
           a \[CenterDot] b, "MatchingSide" -> 1, "Position" -> {1}|>|>, 
     {"SubstitutionLemma", 126} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] ((a \[CenterDot] 
             b) \[CenterDot] a) == (a \[CenterDot] (a \[CenterDot] 
             b)) \[CenterDot] a], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 72}, "Construct" -> 
          {"SubstitutionLemma", 76}, "Position" -> {1}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
           a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] a) == 
            (a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] a], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 127} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] ((a \[CenterDot] 
             b) \[CenterDot] a) == a \[CenterDot] (a \[CenterDot] 
            (a \[CenterDot] b))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 126}, "Construct" -> 
          {"SubstitutionLemma", 76}, "Position" -> {}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
           a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] a) == a \[CenterDot] 
             (a \[CenterDot] (a \[CenterDot] b))], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 139} -> 
      <|"Statement" -> HoldForm[((a \[CenterDot] b) \[CenterDot] 
            c) \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
             (a \[CenterDot] c))) == c], "Proof" -> 
        <|"Input" -> {"Axiom", 1}, "Construct" -> {"SubstitutionLemma", 76}, 
         "Position" -> {2, 2}, "Rule" -> 
          ((a_) \[CenterDot] (b_)) \[CenterDot] (a_) -> a \[CenterDot] 
            (a \[CenterDot] b), "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[((a \[CenterDot] b) \[CenterDot] c) \[CenterDot] 
             (a \[CenterDot] (a \[CenterDot] (a \[CenterDot] c))) == c], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 140} -> 
      <|"Statement" -> HoldForm[((a \[CenterDot] b) \[CenterDot] 
            c) \[CenterDot] (a \[CenterDot] c) == c], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 139}, 
         "Construct" -> {"SubstitutionLemma", 81}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
              (b_))) -> a \[CenterDot] b, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[((a \[CenterDot] b) \[CenterDot] 
              c) \[CenterDot] (a \[CenterDot] c) == c], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 49} -> 
      <|"Statement" -> HoldForm[b == (a \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
           (b \[CenterDot] b)], "Proof" -> 
        <|"Construct" -> {"SubstitutionLemma", 63}, "Orientation" -> -1, 
         "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] (a_)) -> a, "Side" -> 1, 
         "Subpattern" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_), 
         "MatchingConstruct" -> {"SubstitutionLemma", 99}, 
         "MatchingOrientation" -> 1, "MatchingRule" -> 
          ((a_) \[CenterDot] ((b_) \[CenterDot] (((b_) \[CenterDot] 
                (a_)) \[CenterDot] (b_)))) \[CenterDot] (a_) -> 
           b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), 
         "MatchingSide" -> 1, "Position" -> {1}|>|>, 
     {"SubstitutionLemma", 141} -> 
      <|"Statement" -> HoldForm[b == (a \[CenterDot] (a \[CenterDot] 
             (a \[CenterDot] b))) \[CenterDot] (b \[CenterDot] b)], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 49}, 
         "Construct" -> {"SubstitutionLemma", 76}, "Position" -> {1, 2}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
           a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[b == (a \[CenterDot] (a \[CenterDot] (
                a \[CenterDot] b))) \[CenterDot] (b \[CenterDot] b)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 142} -> 
      <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
           (b \[CenterDot] b)], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 141}, "Construct" -> 
          {"SubstitutionLemma", 81}, "Position" -> {1}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
              (b_))) -> a \[CenterDot] b, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
             (b \[CenterDot] b)], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 50} -> <|"Statement" -> 
        HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
           ((c \[CenterDot] a) \[CenterDot] b)], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 140}, 
         "Orientation" -> 1, "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (c_)) \[CenterDot] ((a_) \[CenterDot] (c_)) -> c, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 142}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((b_) \[CenterDot] (b_)) -> b, "MatchingSide" -> 1, 
         "Position" -> {1, 1}|>|>, {"SubstitutionLemma", 1} -> 
      <|"Statement" -> HoldForm[
         (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
             (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a))) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] c)) == a], "Proof" -> 
        <|"Input" -> {"Hypothesis", 1}, "Construct" -> {"CriticalPairLemma", 
           4}, "Position" -> {1, 1}, "Rule" -> 
          a_ -> (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] a)), "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[(((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                 a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                  a) \[CenterDot] a))) \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] (b \[CenterDot] c)) == a], 
         "Source" -> "cpl"|>|>, {"SubstitutionLemma", 3} -> 
      <|"Statement" -> HoldForm[
         (((((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                 a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                  a) \[CenterDot] a))) \[CenterDot] ((((a \[CenterDot] 
                  ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                 (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                   a))) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                    a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                  ((a \[CenterDot] a) \[CenterDot] a)))) \[CenterDot] (
                (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a))))) \[CenterDot] 
             ((((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a))) \[CenterDot] ((((a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                  (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                    a))) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                     a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a)))) \[CenterDot] 
                ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                   a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                    a) \[CenterDot] a))))) \[CenterDot] ((a \[CenterDot] 
                ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] (
                a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                 a))))) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] c)) == a], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 1}, "Construct" -> 
          {"SubstitutionLemma", 2}, "Position" -> {1, 1}, 
         "Rule" -> a_ -> (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a)) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] a)) \[CenterDot] a), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[
           (((((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                   a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                    a) \[CenterDot] a))) \[CenterDot] 
                ((((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                     a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                      a) \[CenterDot] a))) \[CenterDot] ((a \[CenterDot] 
                    ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                   (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                     a)))) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                     a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a))))) \[CenterDot] (
                (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                    a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                     a) \[CenterDot] a))) \[CenterDot] 
                 ((((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                      a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                       a) \[CenterDot] a))) \[CenterDot] ((a \[CenterDot] 
                     ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                    (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                      a)))) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                      a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                    ((a \[CenterDot] a) \[CenterDot] a))))) \[CenterDot] 
                ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                   a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                    a) \[CenterDot] a))))) \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] (b \[CenterDot] c)) == a], 
         "Source" -> "cpl"|>|>, {"SubstitutionLemma", 4} -> 
      <|"Statement" -> HoldForm[
         (((((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                 a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                  a) \[CenterDot] a))) \[CenterDot] ((((a \[CenterDot] 
                  ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                 (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                   a))) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                    a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                  ((a \[CenterDot] a) \[CenterDot] a)))) \[CenterDot] (
                (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a))))) \[CenterDot] 
             ((((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a))) \[CenterDot] ((((a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                  (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                    a))) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                     a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a)))) \[CenterDot] 
                ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                   a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                    a) \[CenterDot] a))))) \[CenterDot] a)) \[CenterDot] 
            b) \[CenterDot] (a \[CenterDot] (b \[CenterDot] c)) == a], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 3}, 
         "Construct" -> {"CriticalPairLemma", 4}, "Position" -> {1, 1, 2, 2}, 
         "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
              (a_))) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] (
                a_)) \[CenterDot] (a_))) -> a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[
           (((((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                   a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                    a) \[CenterDot] a))) \[CenterDot] 
                ((((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                     a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                      a) \[CenterDot] a))) \[CenterDot] ((a \[CenterDot] 
                    ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                   (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                     a)))) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                     a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a))))) \[CenterDot] (
                (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                    a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                     a) \[CenterDot] a))) \[CenterDot] 
                 ((((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                      a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                       a) \[CenterDot] a))) \[CenterDot] ((a \[CenterDot] 
                     ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                    (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                      a)))) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                      a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                    ((a \[CenterDot] a) \[CenterDot] a))))) \[CenterDot] 
                a)) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
              (b \[CenterDot] c)) == a], "Source" -> "cpl"|>|>, 
     {"SubstitutionLemma", 5} -> <|"Statement" -> 
        HoldForm[(((((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                 a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                  a) \[CenterDot] a))) \[CenterDot] ((((a \[CenterDot] 
                  ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                 (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                   a))) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                    a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                  ((a \[CenterDot] a) \[CenterDot] a)))) \[CenterDot] (
                (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a))))) \[CenterDot] 
             ((((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a))) \[CenterDot] ((((a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                  (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                    a))) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                     a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a)))) \[CenterDot] 
                a)) \[CenterDot] a)) \[CenterDot] b) \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] c)) == a], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 4}, 
         "Construct" -> {"CriticalPairLemma", 4}, "Position" -> {1, 1, 2, 1, 
          2, 2}, "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (
                a_)) \[CenterDot] (a_))) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (a_))) -> a, 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           (((((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                   a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                    a) \[CenterDot] a))) \[CenterDot] 
                ((((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                     a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                      a) \[CenterDot] a))) \[CenterDot] ((a \[CenterDot] 
                    ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                   (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                     a)))) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                     a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a))))) \[CenterDot] (
                (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                    a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                     a) \[CenterDot] a))) \[CenterDot] 
                 ((((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                      a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                       a) \[CenterDot] a))) \[CenterDot] ((a \[CenterDot] 
                     ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                    (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                      a)))) \[CenterDot] a)) \[CenterDot] a)) \[CenterDot] 
              b) \[CenterDot] (a \[CenterDot] (b \[CenterDot] c)) == a], 
         "Source" -> "cpl"|>|>, {"SubstitutionLemma", 6} -> 
      <|"Statement" -> HoldForm[
         (((((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                 a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                  a) \[CenterDot] a))) \[CenterDot] ((a \[CenterDot] 
                ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                   a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                    a) \[CenterDot] a)))) \[CenterDot] ((a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a))))) \[CenterDot] ((((a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] (
                (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                    a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                     a) \[CenterDot] a))) \[CenterDot] ((a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                  (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                    a)))) \[CenterDot] a)) \[CenterDot] a)) \[CenterDot] 
            b) \[CenterDot] (a \[CenterDot] (b \[CenterDot] c)) == a], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 5}, 
         "Construct" -> {"CriticalPairLemma", 4}, "Position" -> {1, 1, 1, 2, 
          1, 1}, "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (
                a_)) \[CenterDot] (a_))) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (a_))) -> a, 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           (((((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                   a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                    a) \[CenterDot] a))) \[CenterDot] ((a \[CenterDot] 
                  ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                     a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                      a) \[CenterDot] a)))) \[CenterDot] ((a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                  (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                    a))))) \[CenterDot] ((((a \[CenterDot] ((a \[CenterDot] 
                     a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                 ((((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                      a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                       a) \[CenterDot] a))) \[CenterDot] ((a \[CenterDot] 
                     ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                    (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                      a)))) \[CenterDot] a)) \[CenterDot] a)) \[CenterDot] 
              b) \[CenterDot] (a \[CenterDot] (b \[CenterDot] c)) == a], 
         "Source" -> "cpl"|>|>, {"SubstitutionLemma", 7} -> 
      <|"Statement" -> HoldForm[
         (((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                    a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                  ((a \[CenterDot] a) \[CenterDot] a)))) \[CenterDot] (
                (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a))))) \[CenterDot] 
             ((((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a))) \[CenterDot] ((((a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                  (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                    a))) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                     a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a)))) \[CenterDot] 
                a)) \[CenterDot] a)) \[CenterDot] b) \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] c)) == a], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 6}, 
         "Construct" -> {"CriticalPairLemma", 4}, "Position" -> {1, 1, 1, 1}, 
         "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
              (a_))) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] (
                a_)) \[CenterDot] (a_))) -> a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[
           (((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                    ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                   (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                     a)))) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                     a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a))))) \[CenterDot] (
                (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                    a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                     a) \[CenterDot] a))) \[CenterDot] 
                 ((((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                      a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                       a) \[CenterDot] a))) \[CenterDot] ((a \[CenterDot] 
                     ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                    (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                      a)))) \[CenterDot] a)) \[CenterDot] a)) \[CenterDot] 
              b) \[CenterDot] (a \[CenterDot] (b \[CenterDot] c)) == a], 
         "Source" -> "cpl"|>|>, {"SubstitutionLemma", 83} -> 
      <|"Statement" -> HoldForm[
         (((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                 a)) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))))) \[CenterDot] 
             ((((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a))) \[CenterDot] ((((a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                  (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                    a))) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                     a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a)))) \[CenterDot] 
                a)) \[CenterDot] a)) \[CenterDot] b) \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] c)) == a], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 7}, 
         "Construct" -> {"SubstitutionLemma", 82}, "Position" -> {1, 1, 1, 2, 
          1}, "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] 
              (b_)) \[CenterDot] ((a_) \[CenterDot] (b_))) -> 
           a \[CenterDot] b, "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[(((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                    a) \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                  (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                    a))))) \[CenterDot] ((((a \[CenterDot] ((a \[CenterDot] 
                     a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                 ((((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                      a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                       a) \[CenterDot] a))) \[CenterDot] ((a \[CenterDot] 
                     ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                    (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                      a)))) \[CenterDot] a)) \[CenterDot] a)) \[CenterDot] 
              b) \[CenterDot] (a \[CenterDot] (b \[CenterDot] c)) == a], 
         "Source" -> "cpl"|>|>, {"SubstitutionLemma", 86} -> 
      <|"Statement" -> HoldForm[
         (((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                 a)) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))))) \[CenterDot] 
             ((((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a))) \[CenterDot] ((((a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                  (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                    a))) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                     a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                   (a \[CenterDot] (a \[CenterDot] a))))) \[CenterDot] 
                a)) \[CenterDot] a)) \[CenterDot] b) \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] c)) == a], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 83}, 
         "Construct" -> {"SubstitutionLemma", 85}, "Position" -> {1, 1, 2, 1, 
          2, 1, 2, 2}, "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] 
              (a_)) \[CenterDot] (a_)) -> a \[CenterDot] (a \[CenterDot] 
             (a \[CenterDot] a)), "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[(((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                    a) \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                  (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                    a))))) \[CenterDot] ((((a \[CenterDot] ((a \[CenterDot] 
                     a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                 ((((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                      a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                       a) \[CenterDot] a))) \[CenterDot] ((a \[CenterDot] 
                     ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                    (a \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
                       a))))) \[CenterDot] a)) \[CenterDot] a)) \[CenterDot] 
              b) \[CenterDot] (a \[CenterDot] (b \[CenterDot] c)) == a], 
         "Source" -> "cpl"|>|>, {"SubstitutionLemma", 87} -> 
      <|"Statement" -> HoldForm[
         (((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                 a)) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))))) \[CenterDot] 
             ((((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a))) \[CenterDot] ((((a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                  (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                    a))) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                     a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                   a))) \[CenterDot] a)) \[CenterDot] a)) \[CenterDot] 
            b) \[CenterDot] (a \[CenterDot] (b \[CenterDot] c)) == a], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 86}, 
         "Construct" -> {"SubstitutionLemma", 81}, "Position" -> {1, 1, 2, 1, 
          2, 1, 2, 2}, "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
             ((a_) \[CenterDot] (b_))) -> a \[CenterDot] b, 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           (((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                    a) \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                  (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                    a))))) \[CenterDot] ((((a \[CenterDot] ((a \[CenterDot] 
                     a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                 ((((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                      a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                       a) \[CenterDot] a))) \[CenterDot] ((a \[CenterDot] 
                     ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                    (a \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
                a)) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
              (b \[CenterDot] c)) == a], "Source" -> "cpl"|>|>, 
     {"SubstitutionLemma", 88} -> <|"Statement" -> 
        HoldForm[(((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                  a) \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a))))) \[CenterDot] ((((a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] (
                (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                    a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                     a) \[CenterDot] a))) \[CenterDot] ((a \[CenterDot] 
                   (a \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
                  (a \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
              a)) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] c)) == a], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 87}, "Construct" -> 
          {"SubstitutionLemma", 85}, "Position" -> {1, 1, 2, 1, 2, 1, 2, 1}, 
         "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (a_)) -> a \[CenterDot] (a \[CenterDot] (a \[CenterDot] a)), 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           (((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                    a) \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                  (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                    a))))) \[CenterDot] ((((a \[CenterDot] ((a \[CenterDot] 
                     a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                 ((((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                      a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                       a) \[CenterDot] a))) \[CenterDot] ((a \[CenterDot] 
                     (a \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
                    (a \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
                a)) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
              (b \[CenterDot] c)) == a], "Source" -> "cpl"|>|>, 
     {"SubstitutionLemma", 89} -> <|"Statement" -> 
        HoldForm[(((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                  a) \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a))))) \[CenterDot] ((((a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] (
                (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                    a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                     a) \[CenterDot] a))) \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
                a)) \[CenterDot] a)) \[CenterDot] b) \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] c)) == a], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 88}, 
         "Construct" -> {"SubstitutionLemma", 81}, "Position" -> {1, 1, 2, 1, 
          2, 1, 2, 1}, "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
             ((a_) \[CenterDot] (b_))) -> a \[CenterDot] b, 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           (((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                    a) \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                  (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                    a))))) \[CenterDot] ((((a \[CenterDot] ((a \[CenterDot] 
                     a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                 ((((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                      a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                       a) \[CenterDot] a))) \[CenterDot] ((a \[CenterDot] 
                     a) \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
                  a)) \[CenterDot] a)) \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] (b \[CenterDot] c)) == a], 
         "Source" -> "cpl"|>|>, {"SubstitutionLemma", 90} -> 
      <|"Statement" -> HoldForm[
         (((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                 a)) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))))) \[CenterDot] 
             ((((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a))) \[CenterDot] ((((a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                  (a \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
                     a)))) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  (a \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
              a)) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] c)) == a], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 89}, "Construct" -> 
          {"SubstitutionLemma", 85}, "Position" -> {1, 1, 2, 1, 2, 1, 1, 2}, 
         "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (a_)) -> a \[CenterDot] (a \[CenterDot] (a \[CenterDot] a)), 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           (((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                    a) \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                  (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                    a))))) \[CenterDot] ((((a \[CenterDot] ((a \[CenterDot] 
                     a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                 ((((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                      a)) \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
                      (a \[CenterDot] a)))) \[CenterDot] ((a \[CenterDot] 
                     a) \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
                  a)) \[CenterDot] a)) \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] (b \[CenterDot] c)) == a], 
         "Source" -> "cpl"|>|>, {"SubstitutionLemma", 105} -> 
      <|"Statement" -> HoldForm[
         (((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                 a)) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))))) \[CenterDot] 
             ((((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a))) \[CenterDot] ((((a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                  (a \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
                     a)))) \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
              a)) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] c)) == a], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 90}, "Construct" -> 
          {"SubstitutionLemma", 104}, "Position" -> {1, 1, 2, 1, 2, 1, 2}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
             (a_)) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[(((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                    a) \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                  (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                    a))))) \[CenterDot] ((((a \[CenterDot] ((a \[CenterDot] 
                     a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                 ((((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                      a)) \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
                      (a \[CenterDot] a)))) \[CenterDot] a) \[CenterDot] 
                  a)) \[CenterDot] a)) \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] (b \[CenterDot] c)) == a], 
         "Source" -> "cpl"|>|>, {"SubstitutionLemma", 106} -> 
      <|"Statement" -> HoldForm[
         (((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                 a)) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))))) \[CenterDot] 
             ((((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a))) \[CenterDot] ((((a \[CenterDot] 
                   (a \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
                  (a \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
                     a)))) \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
              a)) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] c)) == a], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 105}, "Construct" -> 
          {"SubstitutionLemma", 85}, "Position" -> {1, 1, 2, 1, 2, 1, 1, 1}, 
         "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (a_)) -> a \[CenterDot] (a \[CenterDot] (a \[CenterDot] a)), 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           (((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                    a) \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                  (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                    a))))) \[CenterDot] ((((a \[CenterDot] ((a \[CenterDot] 
                     a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                 ((((a \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
                       a))) \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
                      (a \[CenterDot] a)))) \[CenterDot] a) \[CenterDot] 
                  a)) \[CenterDot] a)) \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] (b \[CenterDot] c)) == a], 
         "Source" -> "cpl"|>|>, {"SubstitutionLemma", 114} -> 
      <|"Statement" -> HoldForm[
         (((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                 a)) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))))) \[CenterDot] 
             ((((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a))) \[CenterDot] (a \[CenterDot] 
                (((a \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
                     a))) \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
                    (a \[CenterDot] a)))) \[CenterDot] a))) \[CenterDot] 
              a)) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] c)) == a], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 106}, "Construct" -> 
          {"SubstitutionLemma", 113}, "Position" -> {1, 1, 2, 1, 2}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           (((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                    a) \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                  (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                    a))))) \[CenterDot] ((((a \[CenterDot] ((a \[CenterDot] 
                     a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                 (a \[CenterDot] (((a \[CenterDot] (a \[CenterDot] 
                      (a \[CenterDot] a))) \[CenterDot] (a \[CenterDot] 
                     (a \[CenterDot] (a \[CenterDot] a)))) \[CenterDot] 
                   a))) \[CenterDot] a)) \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] (b \[CenterDot] c)) == a], 
         "Source" -> "cpl"|>|>, {"SubstitutionLemma", 115} -> 
      <|"Statement" -> HoldForm[
         (((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                 a)) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))))) \[CenterDot] 
             ((((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] (
                a \[CenterDot] (((a \[CenterDot] (a \[CenterDot] 
                    (a \[CenterDot] a))) \[CenterDot] (a \[CenterDot] 
                   (a \[CenterDot] (a \[CenterDot] a)))) \[CenterDot] 
                 a))) \[CenterDot] a)) \[CenterDot] b) \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] c)) == a], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 114}, 
         "Construct" -> {"CriticalPairLemma", 45}, "Position" -> {1, 1, 2, 1, 
          1, 2}, "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] 
              (a_)) \[CenterDot] (b_)) -> a \[CenterDot] a, 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           (((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                    a) \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                  (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                    a))))) \[CenterDot] ((((a \[CenterDot] ((a \[CenterDot] 
                     a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                   a)) \[CenterDot] (a \[CenterDot] (((a \[CenterDot] 
                     (a \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
                    (a \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
                       a)))) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
              b) \[CenterDot] (a \[CenterDot] (b \[CenterDot] c)) == a], 
         "Source" -> "cpl"|>|>, {"SubstitutionLemma", 116} -> 
      <|"Statement" -> HoldForm[
         (((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                 a)) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                 a)))) \[CenterDot] ((((a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] (a \[CenterDot] (((a \[CenterDot] 
                   (a \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
                  (a \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
                     a)))) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
            b) \[CenterDot] (a \[CenterDot] (b \[CenterDot] c)) == a], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 115}, 
         "Construct" -> {"CriticalPairLemma", 45}, "Position" -> {1, 1, 1, 2, 
          2, 2}, "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] 
              (a_)) \[CenterDot] (b_)) -> a \[CenterDot] a, 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           (((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                    a) \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                  (a \[CenterDot] a)))) \[CenterDot] ((((a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                  (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                  (((a \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
                       a))) \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
                      (a \[CenterDot] a)))) \[CenterDot] a))) \[CenterDot] 
                a)) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
              (b \[CenterDot] c)) == a], "Source" -> "cpl"|>|>, 
     {"SubstitutionLemma", 117} -> 
      <|"Statement" -> HoldForm[
         (((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                 a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a)))) \[CenterDot] 
             ((((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] (
                a \[CenterDot] (((a \[CenterDot] (a \[CenterDot] 
                    (a \[CenterDot] a))) \[CenterDot] (a \[CenterDot] 
                   (a \[CenterDot] (a \[CenterDot] a)))) \[CenterDot] 
                 a))) \[CenterDot] a)) \[CenterDot] b) \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] c)) == a], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 116}, 
         "Construct" -> {"CriticalPairLemma", 45}, "Position" -> {1, 1, 1, 2, 
          2, 1}, "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] 
              (a_)) \[CenterDot] (b_)) -> a \[CenterDot] a, 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           (((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                    a) \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] (a \[CenterDot] a)))) \[CenterDot] (
                (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                    a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
                 (a \[CenterDot] (((a \[CenterDot] (a \[CenterDot] 
                      (a \[CenterDot] a))) \[CenterDot] (a \[CenterDot] 
                     (a \[CenterDot] (a \[CenterDot] a)))) \[CenterDot] 
                   a))) \[CenterDot] a)) \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] (b \[CenterDot] c)) == a], 
         "Source" -> "cpl"|>|>, {"SubstitutionLemma", 118} -> 
      <|"Statement" -> HoldForm[
         (((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
                  a))) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a)))) \[CenterDot] 
             ((((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] (
                a \[CenterDot] (((a \[CenterDot] (a \[CenterDot] 
                    (a \[CenterDot] a))) \[CenterDot] (a \[CenterDot] 
                   (a \[CenterDot] (a \[CenterDot] a)))) \[CenterDot] 
                 a))) \[CenterDot] a)) \[CenterDot] b) \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] c)) == a], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 117}, 
         "Construct" -> {"SubstitutionLemma", 85}, "Position" -> {1, 1, 1, 2, 
          1}, "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] 
              (a_)) \[CenterDot] (a_)) -> a \[CenterDot] (a \[CenterDot] 
             (a \[CenterDot] a)), "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[(((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
                   (a \[CenterDot] a))) \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] (a \[CenterDot] a)))) \[CenterDot] (
                (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                    a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
                 (a \[CenterDot] (((a \[CenterDot] (a \[CenterDot] 
                      (a \[CenterDot] a))) \[CenterDot] (a \[CenterDot] 
                     (a \[CenterDot] (a \[CenterDot] a)))) \[CenterDot] 
                   a))) \[CenterDot] a)) \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] (b \[CenterDot] c)) == a], 
         "Source" -> "cpl"|>|>, {"SubstitutionLemma", 119} -> 
      <|"Statement" -> HoldForm[
         (((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
                  a))) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a)))) \[CenterDot] 
             ((((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] (
                a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                   (a \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
                  (a \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
                     a))))))) \[CenterDot] a)) \[CenterDot] b) \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] c)) == a], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 118}, 
         "Construct" -> {"SubstitutionLemma", 113}, "Position" -> {1, 1, 2, 
          1, 2, 2}, "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           (((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
                    a))) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  (a \[CenterDot] a)))) \[CenterDot] ((((a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                  (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                  (a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
                      (a \[CenterDot] a))) \[CenterDot] (a \[CenterDot] 
                     (a \[CenterDot] (a \[CenterDot] a))))))) \[CenterDot] 
                a)) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
              (b \[CenterDot] c)) == a], "Source" -> "cpl"|>|>, 
     {"SubstitutionLemma", 120} -> 
      <|"Statement" -> HoldForm[
         (((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
                  a))) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a)))) \[CenterDot] 
             ((((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] (
                a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                   (a \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
                  (a \[CenterDot] a))))) \[CenterDot] a)) \[CenterDot] 
            b) \[CenterDot] (a \[CenterDot] (b \[CenterDot] c)) == a], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 119}, 
         "Construct" -> {"SubstitutionLemma", 81}, "Position" -> {1, 1, 2, 1, 
          2, 2, 2, 2}, "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
             ((a_) \[CenterDot] (b_))) -> a \[CenterDot] b, 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           (((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
                    a))) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  (a \[CenterDot] a)))) \[CenterDot] ((((a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                  (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                  (a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
                      (a \[CenterDot] a))) \[CenterDot] (a \[CenterDot] 
                     a))))) \[CenterDot] a)) \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] (b \[CenterDot] c)) == a], 
         "Source" -> "cpl"|>|>, {"SubstitutionLemma", 121} -> 
      <|"Statement" -> HoldForm[
         (((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
                  a))) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a)))) \[CenterDot] 
             ((((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a)) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] (
                a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] (a \[CenterDot] a))))) \[CenterDot] 
              a)) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] c)) == a], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 120}, "Construct" -> 
          {"SubstitutionLemma", 81}, "Position" -> {1, 1, 2, 1, 2, 2, 2, 1}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
              (b_))) -> a \[CenterDot] b, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[
           (((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
                    a))) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  (a \[CenterDot] a)))) \[CenterDot] ((((a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                  (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                  (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                    (a \[CenterDot] a))))) \[CenterDot] a)) \[CenterDot] 
              b) \[CenterDot] (a \[CenterDot] (b \[CenterDot] c)) == a], 
         "Source" -> "cpl"|>|>, {"SubstitutionLemma", 123} -> 
      <|"Statement" -> HoldForm[
         (((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
                  a))) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a)))) \[CenterDot] ((a \[CenterDot] (
                a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] (a \[CenterDot] a))))) \[CenterDot] 
              a)) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] c)) == a], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 121}, "Construct" -> 
          {"CriticalPairLemma", 48}, "Position" -> {1, 1, 2, 1, 1}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
             (a_)) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[(((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
                   (a \[CenterDot] a))) \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] (a \[CenterDot] a)))) \[CenterDot] (
                (a \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                     a))))) \[CenterDot] a)) \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] (b \[CenterDot] c)) == a], 
         "Source" -> "cpl"|>|>, {"SubstitutionLemma", 124} -> 
      <|"Statement" -> HoldForm[
         (((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
                  a))) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a)))) \[CenterDot] ((a \[CenterDot] (
                (a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a))) \[CenterDot] a)) \[CenterDot] b) \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] c)) == a], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 123}, 
         "Construct" -> {"SubstitutionLemma", 81}, "Position" -> {1, 1, 2, 
          1}, "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
             ((a_) \[CenterDot] (b_))) -> a \[CenterDot] b, 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           (((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
                    a))) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  (a \[CenterDot] a)))) \[CenterDot] ((a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                   a))) \[CenterDot] a)) \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] (b \[CenterDot] c)) == a], 
         "Source" -> "cpl"|>|>, {"SubstitutionLemma", 125} -> 
      <|"Statement" -> HoldForm[
         (((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
                  a))) \[CenterDot] a)) \[CenterDot] 
             ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
            b) \[CenterDot] (a \[CenterDot] (b \[CenterDot] c)) == a], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 124}, 
         "Construct" -> {"SubstitutionLemma", 104}, "Position" -> {1, 1, 1, 
          2, 2}, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((b_) \[CenterDot] (a_)) -> a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[
           (((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
                    a))) \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                   a))) \[CenterDot] a)) \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] (b \[CenterDot] c)) == a], 
         "Source" -> "cpl"|>|>, {"SubstitutionLemma", 128} -> 
      <|"Statement" -> HoldForm[
         (((a \[CenterDot] (a \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
                 (a \[CenterDot] a))))) \[CenterDot] 
             ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
            b) \[CenterDot] (a \[CenterDot] (b \[CenterDot] c)) == a], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 125}, 
         "Construct" -> {"SubstitutionLemma", 127}, "Position" -> {1, 1, 1}, 
         "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_)) -> a \[CenterDot] (a \[CenterDot] (a \[CenterDot] b)), 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           (((a \[CenterDot] (a \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
                   (a \[CenterDot] a))))) \[CenterDot] ((a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                   a))) \[CenterDot] a)) \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] (b \[CenterDot] c)) == a], 
         "Source" -> "cpl"|>|>, {"SubstitutionLemma", 129} -> 
      <|"Statement" -> HoldForm[
         (((a \[CenterDot] (a \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
             ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
            b) \[CenterDot] (a \[CenterDot] (b \[CenterDot] c)) == a], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 128}, 
         "Construct" -> {"SubstitutionLemma", 81}, "Position" -> {1, 1, 1}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
              (b_))) -> a \[CenterDot] b, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[
           (((a \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
                  a))) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
                a)) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
              (b \[CenterDot] c)) == a], "Source" -> "cpl"|>|>, 
     {"SubstitutionLemma", 130} -> 
      <|"Statement" -> HoldForm[(((a \[CenterDot] a) \[CenterDot] 
             ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
            b) \[CenterDot] (a \[CenterDot] (b \[CenterDot] c)) == a], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 129}, 
         "Construct" -> {"SubstitutionLemma", 81}, "Position" -> {1, 1, 1}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
              (b_))) -> a \[CenterDot] b, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[(((a \[CenterDot] a) \[CenterDot] (
                (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  (a \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
              b) \[CenterDot] (a \[CenterDot] (b \[CenterDot] c)) == a], 
         "Source" -> "cpl"|>|>, {"SubstitutionLemma", 131} -> 
      <|"Statement" -> HoldForm[
         ((((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a))) \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
              a)) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] c)) == a], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 130}, "Construct" -> 
          {"SubstitutionLemma", 113}, "Position" -> {1, 1}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           ((((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  (a \[CenterDot] a))) \[CenterDot] a) \[CenterDot] (
                a \[CenterDot] a)) \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] (b \[CenterDot] c)) == a], 
         "Source" -> "cpl"|>|>, {"SubstitutionLemma", 132} -> 
      <|"Statement" -> HoldForm[(a \[CenterDot] (b \[CenterDot] 
             c)) \[CenterDot] ((((a \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
              a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] b) == a], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 131}, 
         "Construct" -> {"SubstitutionLemma", 113}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           (a \[CenterDot] (b \[CenterDot] c)) \[CenterDot] 
             ((((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  (a \[CenterDot] a))) \[CenterDot] a) \[CenterDot] (
                a \[CenterDot] a)) \[CenterDot] b) == a], 
         "Source" -> "cpl"|>|>, {"SubstitutionLemma", 133} -> 
      <|"Statement" -> HoldForm[(a \[CenterDot] (b \[CenterDot] 
             c)) \[CenterDot] (b \[CenterDot] 
            (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a))) \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] a))) == a], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 132}, "Construct" -> 
          {"SubstitutionLemma", 113}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           (a \[CenterDot] (b \[CenterDot] c)) \[CenterDot] 
             (b \[CenterDot] (((a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
                a) \[CenterDot] (a \[CenterDot] a))) == a], 
         "Source" -> "cpl"|>|>, {"SubstitutionLemma", 134} -> 
      <|"Statement" -> HoldForm[(a \[CenterDot] (b \[CenterDot] 
             c)) \[CenterDot] (b \[CenterDot] ((a \[CenterDot] 
              a) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] a))) \[CenterDot] a))) == 
          a], "Proof" -> <|"Input" -> {"SubstitutionLemma", 133}, 
         "Construct" -> {"SubstitutionLemma", 113}, "Position" -> {2, 2}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           (a \[CenterDot] (b \[CenterDot] c)) \[CenterDot] 
             (b \[CenterDot] ((a \[CenterDot] a) \[CenterDot] (
                (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  (a \[CenterDot] a))) \[CenterDot] a))) == a], 
         "Source" -> "cpl"|>|>, {"SubstitutionLemma", 135} -> 
      <|"Statement" -> HoldForm[
         (b \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a))) \[CenterDot] a))) \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] c)) == a], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 134}, 
         "Construct" -> {"SubstitutionLemma", 113}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           (b \[CenterDot] ((a \[CenterDot] a) \[CenterDot] ((a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                   a))) \[CenterDot] a))) \[CenterDot] (a \[CenterDot] 
              (b \[CenterDot] c)) == a], "Source" -> "cpl"|>|>, 
     {"SubstitutionLemma", 136} -> 
      <|"Statement" -> HoldForm[
         (b \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a))) \[CenterDot] a))) \[CenterDot] 
           ((b \[CenterDot] c) \[CenterDot] a) == a], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 135}, 
         "Construct" -> {"SubstitutionLemma", 113}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           (b \[CenterDot] ((a \[CenterDot] a) \[CenterDot] ((a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                   a))) \[CenterDot] a))) \[CenterDot] 
             ((b \[CenterDot] c) \[CenterDot] a) == a], 
         "Source" -> "cpl"|>|>, {"SubstitutionLemma", 137} -> 
      <|"Statement" -> HoldForm[
         (b \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a))) \[CenterDot] a))) \[CenterDot] 
           ((c \[CenterDot] b) \[CenterDot] a) == a], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 136}, 
         "Construct" -> {"SubstitutionLemma", 113}, "Position" -> {2, 1}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           (b \[CenterDot] ((a \[CenterDot] a) \[CenterDot] ((a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                   a))) \[CenterDot] a))) \[CenterDot] 
             ((c \[CenterDot] b) \[CenterDot] a) == a], 
         "Source" -> "cpl"|>|>, {"SubstitutionLemma", 138} -> 
      <|"Statement" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
           ((c \[CenterDot] b) \[CenterDot] a) == a], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 137}, 
         "Construct" -> {"SubstitutionLemma", 104}, "Position" -> {1, 2}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
             (a_)) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[(b \[CenterDot] a) \[CenterDot] ((c \[CenterDot] 
               b) \[CenterDot] a) == a], "Source" -> "cpl"|>|>, 
     {"Conclusion", 1} -> <|"Statement" -> HoldForm[a == a], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 138}, 
         "Construct" -> {"CriticalPairLemma", 50}, "Position" -> {}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            (((c_) \[CenterDot] (a_)) \[CenterDot] (b_)) -> b, 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[a == a], 
         "Source" -> "cpl"|>|>}|>], ProofObject["EquationalLogic", 
  Inactive[Equal][a \[CenterDot] b, b \[CenterDot] a], 
  {Inactive[Equal][(((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
     ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] (a_))), c_]}, 
  <|"Variables" -> {a, b, c, x255, x3, x4}, "Constants" -> {}, 
   "Proof" -> {{"Axiom", 1} -> <|"Statement" -> 
        HoldForm[((a \[CenterDot] b) \[CenterDot] c) \[CenterDot] 
           (a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] a)) == c], 
       "Proof" -> <||>|>, {"Hypothesis", 1} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] b == b \[CenterDot] a], 
       "Proof" -> <||>|>, {"CriticalPairLemma", 1} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] ((a \[CenterDot] 
             c) \[CenterDot] a) == ((((a \[CenterDot] b) \[CenterDot] 
              c) \[CenterDot] x3) \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] c) \[CenterDot] a))) \[CenterDot] 
           (((a \[CenterDot] b) \[CenterDot] c) \[CenterDot] 
            (c \[CenterDot] ((a \[CenterDot] b) \[CenterDot] c)))], 
       "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
         "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
            ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
              (a_))) -> c, "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] 
           (c_), "MatchingConstruct" -> {"Axiom", 1}, 
         "MatchingOrientation" -> 1, "MatchingRule" -> 
          (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
            ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
              (a_))) -> c, "MatchingSide" -> 1, "Position" -> {2, 2, 1}|>|>, 
     {"CriticalPairLemma", 2} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a) == 
          ((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a))) \[CenterDot] a], "Proof" -> 
        <|"Construct" -> {"CriticalPairLemma", 1}, "Orientation" -> -1, 
         "Rule" -> (((((a_) \[CenterDot] (b_)) \[CenterDot] (
                c_)) \[CenterDot] (x3_)) \[CenterDot] ((a_) \[CenterDot] 
              (((a_) \[CenterDot] (c_)) \[CenterDot] (a_)))) \[CenterDot] 
            ((((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
             ((c_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] (
                c_)))) -> a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] a), 
         "Side" -> 1, "Subpattern" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (c_)) \[CenterDot] ((c_) \[CenterDot] (((a_) \[CenterDot] 
              (b_)) \[CenterDot] (c_))), "MatchingConstruct" -> {"Axiom", 1}, 
         "MatchingOrientation" -> 1, "MatchingRule" -> 
          (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
            ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
              (a_))) -> c, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"CriticalPairLemma", 3} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a) == 
          (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a))) \[CenterDot] a], "Proof" -> 
        <|"Construct" -> {"CriticalPairLemma", 2}, "Orientation" -> -1, 
         "Rule" -> (((((a_) \[CenterDot] (a_)) \[CenterDot] (
                a_)) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
              (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) \[CenterDot] 
            (a_) -> a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
         "Side" -> 1, "Subpattern" -> (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (a_)) \[CenterDot] (b_), "MatchingConstruct" -> {"Axiom", 1}, 
         "MatchingOrientation" -> 1, "MatchingRule" -> 
          (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
            ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
              (a_))) -> c, "MatchingSide" -> 1, "Position" -> {1, 1}|>|>, 
     {"CriticalPairLemma", 4} -> <|"Statement" -> 
        HoldForm[a == (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
              a) \[CenterDot] a))], "Proof" -> <|"Construct" -> {"Axiom", 1}, 
         "Orientation" -> 1, "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] (
                c_)) \[CenterDot] (a_))) -> c, "Side" -> 1, 
         "Subpattern" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (c_), 
         "MatchingConstruct" -> {"CriticalPairLemma", 3}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          ((a_) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] 
                (a_)) \[CenterDot] (a_)))) \[CenterDot] (a_) -> 
           a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
         "MatchingSide" -> 1, "Position" -> {1}|>|>, 
     {"CriticalPairLemma", 5} -> <|"Statement" -> 
        HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
           (((c \[CenterDot] x3) \[CenterDot] a) \[CenterDot] 
            ((((c \[CenterDot] x3) \[CenterDot] a) \[CenterDot] 
              b) \[CenterDot] ((c \[CenterDot] x3) \[CenterDot] a)))], 
       "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
         "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
            ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
              (a_))) -> c, "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] 
           (b_), "MatchingConstruct" -> {"Axiom", 1}, 
         "MatchingOrientation" -> 1, "MatchingRule" -> 
          (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
            ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
              (a_))) -> c, "MatchingSide" -> 1, "Position" -> {1, 1}|>|>, 
     {"CriticalPairLemma", 6} -> <|"Statement" -> 
        HoldForm[b == (((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
               a)) \[CenterDot] c) \[CenterDot] b) \[CenterDot] 
           ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
            b)], "Proof" -> <|"Construct" -> {"Axiom", 1}, 
         "Orientation" -> 1, "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] (
                c_)) \[CenterDot] (a_))) -> c, "Side" -> 1, 
         "Subpattern" -> ((a_) \[CenterDot] (c_)) \[CenterDot] (a_), 
         "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] (
                c_)) \[CenterDot] (a_))) -> c, "MatchingSide" -> 1, 
         "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 7} -> 
      <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
           (((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b)) \[CenterDot] a) \[CenterDot] a)], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 5}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((((c_) \[CenterDot] (x3_)) \[CenterDot] (a_)) \[CenterDot] 
             (((((c_) \[CenterDot] (x3_)) \[CenterDot] (a_)) \[CenterDot] (
                b_)) \[CenterDot] (((c_) \[CenterDot] (x3_)) \[CenterDot] (
                a_)))) -> b, "Side" -> 1, "Subpattern" -> 
          ((((c_) \[CenterDot] (x3_)) \[CenterDot] (a_)) \[CenterDot] 
            (b_)) \[CenterDot] (((c_) \[CenterDot] (x3_)) \[CenterDot] (a_)), 
         "MatchingConstruct" -> {"CriticalPairLemma", 6}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          ((((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
                (a_))) \[CenterDot] (c_)) \[CenterDot] (b_)) \[CenterDot] 
            (((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] (
                a_))) \[CenterDot] (b_)) -> b, "MatchingSide" -> 1, 
         "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 8} -> 
      <|"Statement" -> HoldForm[b \[CenterDot] ((b \[CenterDot] 
             a) \[CenterDot] b) == a \[CenterDot] 
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
     {"CriticalPairLemma", 9} -> <|"Statement" -> 
        HoldForm[c == ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              a)) \[CenterDot] c) \[CenterDot] (b \[CenterDot] 
            ((b \[CenterDot] c) \[CenterDot] b))], 
       "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
         "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
            ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
              (a_))) -> c, "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] 
           (b_), "MatchingConstruct" -> {"CriticalPairLemma", 8}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          (a_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] 
             ((((b_) \[CenterDot] (c_)) \[CenterDot] ((b_) \[CenterDot] 
                (((b_) \[CenterDot] (a_)) \[CenterDot] (b_)))) \[CenterDot] 
              ((b_) \[CenterDot] (c_)))) -> b \[CenterDot] 
            ((b \[CenterDot] a) \[CenterDot] b), "MatchingSide" -> 1, 
         "Position" -> {1, 1}|>|>, {"CriticalPairLemma", 10} -> 
      <|"Statement" -> HoldForm[
         c == (((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
               a)) \[CenterDot] b) \[CenterDot] c) \[CenterDot] 
           (b \[CenterDot] ((b \[CenterDot] c) \[CenterDot] b))], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 9}, 
         "Orientation" -> -1, "Rule" -> 
          (((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] (
                a_))) \[CenterDot] (c_)) \[CenterDot] ((b_) \[CenterDot] 
             (((b_) \[CenterDot] (c_)) \[CenterDot] (b_))) -> c, "Side" -> 1, 
         "Subpattern" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_), 
         "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] (
                c_)) \[CenterDot] (a_))) -> c, "MatchingSide" -> 1, 
         "Position" -> {1, 1, 2}|>|>, {"CriticalPairLemma", 11} -> 
      <|"Statement" -> HoldForm[b \[CenterDot] ((b \[CenterDot] 
             a) \[CenterDot] b) == a \[CenterDot] (a \[CenterDot] 
            ((a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                b))) \[CenterDot] a))], "Proof" -> 
        <|"Construct" -> {"CriticalPairLemma", 10}, "Orientation" -> -1, 
         "Rule" -> ((((a_) \[CenterDot] (((a_) \[CenterDot] 
                 (b_)) \[CenterDot] (a_))) \[CenterDot] (b_)) \[CenterDot] 
             (c_)) \[CenterDot] ((b_) \[CenterDot] (((b_) \[CenterDot] (
                c_)) \[CenterDot] (b_))) -> c, "Side" -> 1, 
         "Subpattern" -> (((a_) \[CenterDot] (((a_) \[CenterDot] (
                b_)) \[CenterDot] (a_))) \[CenterDot] (b_)) \[CenterDot] 
           (c_), "MatchingConstruct" -> {"Axiom", 1}, 
         "MatchingOrientation" -> 1, "MatchingRule" -> 
          (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
            ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
              (a_))) -> c, "MatchingSide" -> 1, "Position" -> {1}|>|>, 
     {"CriticalPairLemma", 12} -> <|"Statement" -> 
        HoldForm[(b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
             b)) \[CenterDot] (((b \[CenterDot] ((b \[CenterDot] 
                a) \[CenterDot] b)) \[CenterDot] a) \[CenterDot] 
            (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b))) == 
          a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] ((b \[CenterDot] 
                ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
               a)) \[CenterDot] a))], "Proof" -> 
        <|"Construct" -> {"CriticalPairLemma", 11}, "Orientation" -> -1, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] ((b_) \[CenterDot] (((b_) \[CenterDot] 
                  (a_)) \[CenterDot] (b_)))) \[CenterDot] (a_))) -> 
           b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), "Side" -> 1, 
         "Subpattern" -> ((b_) \[CenterDot] (a_)) \[CenterDot] (b_), 
         "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] (
                c_)) \[CenterDot] (a_))) -> c, "MatchingSide" -> 1, 
         "Position" -> {2, 2, 1, 2, 2}|>|>, {"SubstitutionLemma", 4} -> 
      <|"Statement" -> HoldForm[
         (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           a == a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
              ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                 b)) \[CenterDot] a)) \[CenterDot] a))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 12}, 
         "Construct" -> {"Axiom", 1}, "Position" -> {2}, 
         "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
            ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
              (a_))) -> c, "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[(b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b)) \[CenterDot] a == a \[CenterDot] (a \[CenterDot] 
              ((a \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] 
                    a) \[CenterDot] b)) \[CenterDot] a)) \[CenterDot] a))], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 13} -> 
      <|"Statement" -> HoldForm[((a \[CenterDot] c) \[CenterDot] 
            b) \[CenterDot] ((((a \[CenterDot] c) \[CenterDot] 
              b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                b) \[CenterDot] a))) \[CenterDot] 
            ((a \[CenterDot] c) \[CenterDot] b)) == 
          (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
           ((((a \[CenterDot] c) \[CenterDot] b) \[CenterDot] 
             x3) \[CenterDot] (((((a \[CenterDot] c) \[CenterDot] 
                b) \[CenterDot] x3) \[CenterDot] (((a \[CenterDot] 
                 c) \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
                ((a \[CenterDot] c) \[CenterDot] b)))) \[CenterDot] 
             (((a \[CenterDot] c) \[CenterDot] b) \[CenterDot] x3)))], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 8}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] 
            (((b_) \[CenterDot] (c_)) \[CenterDot] ((((b_) \[CenterDot] 
                (c_)) \[CenterDot] ((b_) \[CenterDot] (((b_) \[CenterDot] 
                  (a_)) \[CenterDot] (b_)))) \[CenterDot] ((b_) \[CenterDot] (
                c_)))) -> b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), 
         "Side" -> 1, "Subpattern" -> (b_) \[CenterDot] (a_), 
         "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] (
                c_)) \[CenterDot] (a_))) -> c, "MatchingSide" -> 1, 
         "Position" -> {2, 2, 1, 2, 2, 1}|>|>, {"SubstitutionLemma", 5} -> 
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
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 13}, 
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
                  ((a \[CenterDot] c) \[CenterDot] b)))) \[CenterDot] (
                ((a \[CenterDot] c) \[CenterDot] b) \[CenterDot] x3)))], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 14} -> 
      <|"Statement" -> HoldForm[a == (a \[CenterDot] 
            ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a)) \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
              ((a \[CenterDot] a) \[CenterDot] a))))], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 10}, 
         "Orientation" -> -1, "Rule" -> 
          ((((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
                (a_))) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
            ((b_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] 
              (b_))) -> c, "Side" -> 1, "Subpattern" -> 
          (((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
              (a_))) \[CenterDot] (b_)) \[CenterDot] (c_), 
         "MatchingConstruct" -> {"CriticalPairLemma", 2}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          (((((a_) \[CenterDot] (a_)) \[CenterDot] (a_)) \[CenterDot] 
              (b_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] 
                (a_)) \[CenterDot] (a_)))) \[CenterDot] (a_) -> 
           a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
         "MatchingSide" -> 1, "Position" -> {1}|>|>, 
     {"SubstitutionLemma", 6} -> <|"Statement" -> 
        HoldForm[a == (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             a)) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] a)) \[CenterDot] a)], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 14}, 
         "Construct" -> {"Axiom", 1}, "Position" -> {2, 2}, 
         "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
            ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
              (a_))) -> c, "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[a == (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a)) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] a)) \[CenterDot] a)], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 15} -> 
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
              (((x3 \[CenterDot] x4) \[CenterDot] c) \[CenterDot] (
                (((x3 \[CenterDot] x4) \[CenterDot] c) \[CenterDot] 
                 b) \[CenterDot] ((x3 \[CenterDot] x4) \[CenterDot] c))))))], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 8}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] 
            (((b_) \[CenterDot] (c_)) \[CenterDot] ((((b_) \[CenterDot] 
                (c_)) \[CenterDot] ((b_) \[CenterDot] (((b_) \[CenterDot] 
                  (a_)) \[CenterDot] (b_)))) \[CenterDot] ((b_) \[CenterDot] (
                c_)))) -> b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), 
         "Side" -> 1, "Subpattern" -> (b_) \[CenterDot] (c_), 
         "MatchingConstruct" -> {"CriticalPairLemma", 5}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((((c_) \[CenterDot] (x3_)) \[CenterDot] (a_)) \[CenterDot] 
             (((((c_) \[CenterDot] (x3_)) \[CenterDot] (a_)) \[CenterDot] (
                b_)) \[CenterDot] (((c_) \[CenterDot] (x3_)) \[CenterDot] (
                a_)))) -> b, "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
     {"CriticalPairLemma", 16} -> <|"Statement" -> 
        HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
           ((c \[CenterDot] a) \[CenterDot] 
            ((((((x3 \[CenterDot] x4) \[CenterDot] c) \[CenterDot] 
                (x3 \[CenterDot] ((x3 \[CenterDot] c) \[CenterDot] 
                  x3))) \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
             ((((x3 \[CenterDot] x4) \[CenterDot] c) \[CenterDot] (
                x3 \[CenterDot] ((x3 \[CenterDot] c) \[CenterDot] 
                 x3))) \[CenterDot] a)))], "Proof" -> 
        <|"Construct" -> {"CriticalPairLemma", 5}, "Orientation" -> -1, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((((c_) \[CenterDot] (x3_)) \[CenterDot] (a_)) \[CenterDot] 
             (((((c_) \[CenterDot] (x3_)) \[CenterDot] (a_)) \[CenterDot] (
                b_)) \[CenterDot] (((c_) \[CenterDot] (x3_)) \[CenterDot] (
                a_)))) -> b, "Side" -> 1, "Subpattern" -> 
          (c_) \[CenterDot] (x3_), "MatchingConstruct" -> {"Axiom", 1}, 
         "MatchingOrientation" -> 1, "MatchingRule" -> 
          (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
            ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
              (a_))) -> c, "MatchingSide" -> 1, "Position" -> {2, 1, 1}|>|>, 
     {"SubstitutionLemma", 7} -> <|"Statement" -> 
        HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
           ((c \[CenterDot] a) \[CenterDot] (((c \[CenterDot] a) \[CenterDot] 
              b) \[CenterDot] ((((x3 \[CenterDot] x4) \[CenterDot] 
                c) \[CenterDot] (x3 \[CenterDot] ((x3 \[CenterDot] 
                  c) \[CenterDot] x3))) \[CenterDot] a)))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 16}, 
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
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 8} -> 
      <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
           ((c \[CenterDot] a) \[CenterDot] (((c \[CenterDot] a) \[CenterDot] 
              b) \[CenterDot] (c \[CenterDot] a)))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 7}, 
         "Construct" -> {"Axiom", 1}, "Position" -> {2, 2, 2, 1}, 
         "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
            ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
              (a_))) -> c, "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
             ((c \[CenterDot] a) \[CenterDot] (((c \[CenterDot] 
                 a) \[CenterDot] b) \[CenterDot] (c \[CenterDot] a)))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 9} -> 
      <|"Statement" -> HoldForm[(c \[CenterDot] b) \[CenterDot] 
           (((c \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            (c \[CenterDot] b)) == a \[CenterDot] (b \[CenterDot] 
            ((b \[CenterDot] ((c \[CenterDot] b) \[CenterDot] (
                ((c \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
                (c \[CenterDot] b)))) \[CenterDot] ((c \[CenterDot] 
               b) \[CenterDot] (((x3 \[CenterDot] x4) \[CenterDot] 
                c) \[CenterDot] ((((x3 \[CenterDot] x4) \[CenterDot] 
                  c) \[CenterDot] b) \[CenterDot] ((x3 \[CenterDot] 
                  x4) \[CenterDot] c))))))], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 15}, "Construct" -> 
          {"SubstitutionLemma", 8}, "Position" -> {2, 2, 1, 1}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            (((c_) \[CenterDot] (a_)) \[CenterDot] ((((c_) \[CenterDot] 
                (a_)) \[CenterDot] (b_)) \[CenterDot] ((c_) \[CenterDot] (
                a_)))) -> b, "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[(c \[CenterDot] b) \[CenterDot] (((c \[CenterDot] 
                b) \[CenterDot] a) \[CenterDot] (c \[CenterDot] b)) == 
            a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                ((c \[CenterDot] b) \[CenterDot] (((c \[CenterDot] 
                    b) \[CenterDot] a) \[CenterDot] (c \[CenterDot] 
                   b)))) \[CenterDot] ((c \[CenterDot] b) \[CenterDot] 
                (((x3 \[CenterDot] x4) \[CenterDot] c) \[CenterDot] 
                 ((((x3 \[CenterDot] x4) \[CenterDot] c) \[CenterDot] 
                   b) \[CenterDot] ((x3 \[CenterDot] x4) \[CenterDot] 
                   c))))))], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 10} -> <|"Statement" -> 
        HoldForm[(c \[CenterDot] b) \[CenterDot] 
           (((c \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            (c \[CenterDot] b)) == a \[CenterDot] (b \[CenterDot] 
            ((b \[CenterDot] ((c \[CenterDot] b) \[CenterDot] (
                ((c \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
                (c \[CenterDot] b)))) \[CenterDot] b))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 9}, 
         "Construct" -> {"SubstitutionLemma", 8}, "Position" -> {2, 2, 2}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            (((c_) \[CenterDot] (a_)) \[CenterDot] ((((c_) \[CenterDot] 
                (a_)) \[CenterDot] (b_)) \[CenterDot] ((c_) \[CenterDot] (
                a_)))) -> b, "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[(c \[CenterDot] b) \[CenterDot] (((c \[CenterDot] 
                b) \[CenterDot] a) \[CenterDot] (c \[CenterDot] b)) == 
            a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                ((c \[CenterDot] b) \[CenterDot] (((c \[CenterDot] 
                    b) \[CenterDot] a) \[CenterDot] (c \[CenterDot] 
                   b)))) \[CenterDot] b))], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 17} -> <|"Statement" -> 
        HoldForm[(a \[CenterDot] b) \[CenterDot] 
           (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                b) \[CenterDot] a))) \[CenterDot] (a \[CenterDot] b)) == 
          (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
           (b \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
               a)) \[CenterDot] b))], "Proof" -> 
        <|"Construct" -> {"SubstitutionLemma", 10}, "Orientation" -> -1, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             (((b_) \[CenterDot] (((c_) \[CenterDot] (b_)) \[CenterDot] 
                ((((c_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
                 ((c_) \[CenterDot] (b_))))) \[CenterDot] (b_))) -> 
           (c \[CenterDot] b) \[CenterDot] (((c \[CenterDot] b) \[CenterDot] 
              a) \[CenterDot] (c \[CenterDot] b)), "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] (((c_) \[CenterDot] 
             (b_)) \[CenterDot] ((((c_) \[CenterDot] (b_)) \[CenterDot] 
              (a_)) \[CenterDot] ((c_) \[CenterDot] (b_)))), 
         "MatchingConstruct" -> {"CriticalPairLemma", 8}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          (a_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] 
             ((((b_) \[CenterDot] (c_)) \[CenterDot] ((b_) \[CenterDot] 
                (((b_) \[CenterDot] (a_)) \[CenterDot] (b_)))) \[CenterDot] 
              ((b_) \[CenterDot] (c_)))) -> b \[CenterDot] 
            ((b \[CenterDot] a) \[CenterDot] b), "MatchingSide" -> 1, 
         "Position" -> {2, 2, 1}|>|>, {"CriticalPairLemma", 18} -> 
      <|"Statement" -> HoldForm[(((b \[CenterDot] c) \[CenterDot] 
             a) \[CenterDot] ((((b \[CenterDot] c) \[CenterDot] 
               a) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                 a) \[CenterDot] b))) \[CenterDot] ((b \[CenterDot] 
               c) \[CenterDot] a))) \[CenterDot] 
           ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
            ((((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
              ((((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
                (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
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
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 17}, 
         "Orientation" -> 1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] (
                ((a_) \[CenterDot] (b_)) \[CenterDot] (a_)))) \[CenterDot] 
             ((a_) \[CenterDot] (b_))) -> (a \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
            (b \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
                a)) \[CenterDot] b)), "Side" -> 1, "Subpattern" -> 
          (a_) \[CenterDot] (b_), "MatchingConstruct" -> {"Axiom", 1}, 
         "MatchingOrientation" -> 1, "MatchingRule" -> 
          (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
            ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
              (a_))) -> c, "MatchingSide" -> 1, "Position" -> {1}|>|>, 
     {"SubstitutionLemma", 11} -> <|"Statement" -> 
        HoldForm[(((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
            ((((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
              (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                b))) \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
              a))) \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] 
               a) \[CenterDot] b)) \[CenterDot] 
            ((((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
              ((((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
                (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                  b))) \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
                a))) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                a) \[CenterDot] b)))) == a \[CenterDot] 
           ((a \[CenterDot] (((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
              ((((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
                (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                  b))) \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
                a)))) \[CenterDot] (((b \[CenterDot] c) \[CenterDot] 
              a) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                a) \[CenterDot] b))))], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 18}, "Construct" -> {"Axiom", 1}, 
         "Position" -> {2, 1, 1}, "Rule" -> 
          (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
            ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
              (a_))) -> c, "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[(((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
              ((((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
                (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                  b))) \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
                a))) \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] 
                 a) \[CenterDot] b)) \[CenterDot] ((((b \[CenterDot] 
                  c) \[CenterDot] a) \[CenterDot] ((((b \[CenterDot] 
                    c) \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
                   ((b \[CenterDot] a) \[CenterDot] b))) \[CenterDot] 
                 ((b \[CenterDot] c) \[CenterDot] a))) \[CenterDot] (
                b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)))) == 
            a \[CenterDot] ((a \[CenterDot] (((b \[CenterDot] c) \[CenterDot] 
                 a) \[CenterDot] ((((b \[CenterDot] c) \[CenterDot] 
                   a) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                     a) \[CenterDot] b))) \[CenterDot] ((b \[CenterDot] 
                   c) \[CenterDot] a)))) \[CenterDot] (((b \[CenterDot] 
                 c) \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
                ((b \[CenterDot] a) \[CenterDot] b))))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 12} -> 
      <|"Statement" -> HoldForm[(((b \[CenterDot] c) \[CenterDot] 
             a) \[CenterDot] ((((b \[CenterDot] c) \[CenterDot] 
               a) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                 a) \[CenterDot] b))) \[CenterDot] ((b \[CenterDot] 
               c) \[CenterDot] a))) \[CenterDot] 
           ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
            ((((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
              ((((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
                (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                  b))) \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
                a))) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                a) \[CenterDot] b)))) == a \[CenterDot] 
           ((a \[CenterDot] (((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
                a)))) \[CenterDot] (((b \[CenterDot] c) \[CenterDot] 
              a) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                a) \[CenterDot] b))))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 11}, "Construct" -> {"Axiom", 1}, 
         "Position" -> {2, 1, 2, 2, 1}, "Rule" -> 
          (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
            ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
              (a_))) -> c, "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[(((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
              ((((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
                (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                  b))) \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
                a))) \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] 
                 a) \[CenterDot] b)) \[CenterDot] ((((b \[CenterDot] 
                  c) \[CenterDot] a) \[CenterDot] ((((b \[CenterDot] 
                    c) \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
                   ((b \[CenterDot] a) \[CenterDot] b))) \[CenterDot] 
                 ((b \[CenterDot] c) \[CenterDot] a))) \[CenterDot] (
                b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)))) == 
            a \[CenterDot] ((a \[CenterDot] (((b \[CenterDot] c) \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] ((b \[CenterDot] 
                   c) \[CenterDot] a)))) \[CenterDot] (((b \[CenterDot] 
                 c) \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
                ((b \[CenterDot] a) \[CenterDot] b))))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 13} -> 
      <|"Statement" -> HoldForm[(((b \[CenterDot] c) \[CenterDot] 
             a) \[CenterDot] ((((b \[CenterDot] c) \[CenterDot] 
               a) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                 a) \[CenterDot] b))) \[CenterDot] ((b \[CenterDot] 
               c) \[CenterDot] a))) \[CenterDot] 
           ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
            ((((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
              ((((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
                (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                  b))) \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
                a))) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                a) \[CenterDot] b)))) == a \[CenterDot] 
           ((a \[CenterDot] (((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
                a)))) \[CenterDot] a)], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 12}, "Construct" -> {"Axiom", 1}, 
         "Position" -> {2, 2}, "Rule" -> 
          (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
            ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
              (a_))) -> c, "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[(((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
              ((((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
                (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                  b))) \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
                a))) \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] 
                 a) \[CenterDot] b)) \[CenterDot] ((((b \[CenterDot] 
                  c) \[CenterDot] a) \[CenterDot] ((((b \[CenterDot] 
                    c) \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
                   ((b \[CenterDot] a) \[CenterDot] b))) \[CenterDot] 
                 ((b \[CenterDot] c) \[CenterDot] a))) \[CenterDot] (
                b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)))) == 
            a \[CenterDot] ((a \[CenterDot] (((b \[CenterDot] c) \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] ((b \[CenterDot] 
                   c) \[CenterDot] a)))) \[CenterDot] a)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 14} -> 
      <|"Statement" -> HoldForm[(((b \[CenterDot] c) \[CenterDot] 
             a) \[CenterDot] (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
              a))) \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] 
               a) \[CenterDot] b)) \[CenterDot] 
            ((((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
              ((((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
                (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                  b))) \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
                a))) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                a) \[CenterDot] b)))) == a \[CenterDot] 
           ((a \[CenterDot] (((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
                a)))) \[CenterDot] a)], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 13}, "Construct" -> {"Axiom", 1}, 
         "Position" -> {1, 2, 1}, "Rule" -> 
          (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
            ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
              (a_))) -> c, "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[(((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
                a))) \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] 
                 a) \[CenterDot] b)) \[CenterDot] ((((b \[CenterDot] 
                  c) \[CenterDot] a) \[CenterDot] ((((b \[CenterDot] 
                    c) \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
                   ((b \[CenterDot] a) \[CenterDot] b))) \[CenterDot] 
                 ((b \[CenterDot] c) \[CenterDot] a))) \[CenterDot] (
                b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)))) == 
            a \[CenterDot] ((a \[CenterDot] (((b \[CenterDot] c) \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] ((b \[CenterDot] 
                   c) \[CenterDot] a)))) \[CenterDot] a)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 15} -> 
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
        <|"Input" -> {"SubstitutionLemma", 14}, "Construct" -> {"Axiom", 1}, 
         "Position" -> {2, 2, 1, 2, 1}, "Rule" -> 
          (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
            ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
              (a_))) -> c, "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[(((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
                a))) \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] 
                 a) \[CenterDot] b)) \[CenterDot] ((((b \[CenterDot] 
                  c) \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 ((b \[CenterDot] c) \[CenterDot] a))) \[CenterDot] (
                b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)))) == 
            a \[CenterDot] ((a \[CenterDot] (((b \[CenterDot] c) \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] ((b \[CenterDot] 
                   c) \[CenterDot] a)))) \[CenterDot] a)], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 19} -> 
      <|"Statement" -> HoldForm[(((a \[CenterDot] a) \[CenterDot] 
             a) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a))) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] a)) \[CenterDot] 
            ((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a))) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] a)))) == a \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] a)], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 15}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] 
            (((a_) \[CenterDot] ((((b_) \[CenterDot] (c_)) \[CenterDot] 
                (a_)) \[CenterDot] ((a_) \[CenterDot] (((b_) \[CenterDot] 
                  (c_)) \[CenterDot] (a_))))) \[CenterDot] (a_)) -> 
           (((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
               a))) \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] 
                a) \[CenterDot] b)) \[CenterDot] 
             ((((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] (
                a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
                 a))) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                 a) \[CenterDot] b)))), "Side" -> 1, "Subpattern" -> 
          (((b_) \[CenterDot] (c_)) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] (a_))), 
         "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] (
                c_)) \[CenterDot] (a_))) -> c, "MatchingSide" -> 1, 
         "Position" -> {2, 1, 2}|>|>, {"SubstitutionLemma", 16} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] ((a \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            ((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a))) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] a)))) == a \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] a)], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 19}, 
         "Construct" -> {"Axiom", 1}, "Position" -> {1}, 
         "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
            ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
              (a_))) -> c, "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] a)) \[CenterDot] ((((a \[CenterDot] 
                  a) \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] (
                a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)))) == 
            a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 17} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] ((a \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a)))) == a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 16}, 
         "Construct" -> {"Axiom", 1}, "Position" -> {2, 2, 1}, 
         "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
            ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
              (a_))) -> c, "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] (
                a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)))) == 
            a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 20} -> 
      <|"Statement" -> HoldForm[c \[CenterDot] ((c \[CenterDot] 
             a) \[CenterDot] c) == a \[CenterDot] 
           ((b \[CenterDot] ((b \[CenterDot] c) \[CenterDot] b)) \[CenterDot] 
            (((c \[CenterDot] ((b \[CenterDot] x3) \[CenterDot] 
                (((b \[CenterDot] x3) \[CenterDot] (b \[CenterDot] 
                   ((b \[CenterDot] c) \[CenterDot] b))) \[CenterDot] 
                 (b \[CenterDot] x3)))) \[CenterDot] (c \[CenterDot] (
                (c \[CenterDot] a) \[CenterDot] c))) \[CenterDot] 
             (c \[CenterDot] ((b \[CenterDot] x3) \[CenterDot] (
                ((b \[CenterDot] x3) \[CenterDot] (b \[CenterDot] 
                  ((b \[CenterDot] c) \[CenterDot] b))) \[CenterDot] 
                (b \[CenterDot] x3))))))], "Proof" -> 
        <|"Construct" -> {"CriticalPairLemma", 8}, "Orientation" -> -1, 
         "Rule" -> (a_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] 
             ((((b_) \[CenterDot] (c_)) \[CenterDot] ((b_) \[CenterDot] 
                (((b_) \[CenterDot] (a_)) \[CenterDot] (b_)))) \[CenterDot] 
              ((b_) \[CenterDot] (c_)))) -> b \[CenterDot] 
            ((b \[CenterDot] a) \[CenterDot] b), "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] (c_), "MatchingConstruct" -> 
          {"CriticalPairLemma", 8}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> (a_) \[CenterDot] (((b_) \[CenterDot] 
              (c_)) \[CenterDot] ((((b_) \[CenterDot] (c_)) \[CenterDot] (
                (b_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] 
                 (b_)))) \[CenterDot] ((b_) \[CenterDot] (c_)))) -> 
           b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), 
         "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
     {"SubstitutionLemma", 18} -> <|"Statement" -> 
        HoldForm[c \[CenterDot] ((c \[CenterDot] a) \[CenterDot] c) == 
          a \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
              b)) \[CenterDot] (((b \[CenterDot] ((b \[CenterDot] 
                 c) \[CenterDot] b)) \[CenterDot] (c \[CenterDot] (
                (c \[CenterDot] a) \[CenterDot] c))) \[CenterDot] 
             (c \[CenterDot] ((b \[CenterDot] x3) \[CenterDot] (
                ((b \[CenterDot] x3) \[CenterDot] (b \[CenterDot] 
                  ((b \[CenterDot] c) \[CenterDot] b))) \[CenterDot] 
                (b \[CenterDot] x3))))))], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 20}, "Construct" -> 
          {"CriticalPairLemma", 8}, "Position" -> {2, 2, 1, 1}, 
         "Rule" -> (a_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] 
             ((((b_) \[CenterDot] (c_)) \[CenterDot] ((b_) \[CenterDot] 
                (((b_) \[CenterDot] (a_)) \[CenterDot] (b_)))) \[CenterDot] 
              ((b_) \[CenterDot] (c_)))) -> b \[CenterDot] 
            ((b \[CenterDot] a) \[CenterDot] b), "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[c \[CenterDot] 
             ((c \[CenterDot] a) \[CenterDot] c) == a \[CenterDot] 
             ((b \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
                b)) \[CenterDot] (((b \[CenterDot] ((b \[CenterDot] 
                   c) \[CenterDot] b)) \[CenterDot] (c \[CenterDot] 
                 ((c \[CenterDot] a) \[CenterDot] c))) \[CenterDot] (
                c \[CenterDot] ((b \[CenterDot] x3) \[CenterDot] 
                 (((b \[CenterDot] x3) \[CenterDot] (b \[CenterDot] 
                    ((b \[CenterDot] c) \[CenterDot] b))) \[CenterDot] 
                  (b \[CenterDot] x3))))))], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 19} -> <|"Statement" -> 
        HoldForm[c \[CenterDot] ((c \[CenterDot] a) \[CenterDot] c) == 
          a \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
              b)) \[CenterDot] (((b \[CenterDot] ((b \[CenterDot] 
                 c) \[CenterDot] b)) \[CenterDot] (c \[CenterDot] (
                (c \[CenterDot] a) \[CenterDot] c))) \[CenterDot] 
             (b \[CenterDot] ((b \[CenterDot] c) \[CenterDot] b))))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 18}, 
         "Construct" -> {"CriticalPairLemma", 8}, "Position" -> {2, 2, 2}, 
         "Rule" -> (a_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] 
             ((((b_) \[CenterDot] (c_)) \[CenterDot] ((b_) \[CenterDot] 
                (((b_) \[CenterDot] (a_)) \[CenterDot] (b_)))) \[CenterDot] 
              ((b_) \[CenterDot] (c_)))) -> b \[CenterDot] 
            ((b \[CenterDot] a) \[CenterDot] b), "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[c \[CenterDot] 
             ((c \[CenterDot] a) \[CenterDot] c) == a \[CenterDot] 
             ((b \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
                b)) \[CenterDot] (((b \[CenterDot] ((b \[CenterDot] 
                   c) \[CenterDot] b)) \[CenterDot] (c \[CenterDot] 
                 ((c \[CenterDot] a) \[CenterDot] c))) \[CenterDot] (
                b \[CenterDot] ((b \[CenterDot] c) \[CenterDot] b))))], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 21} -> 
      <|"Statement" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
           (((b \[CenterDot] a) \[CenterDot] ((a \[CenterDot] (b \[CenterDot] 
                a)) \[CenterDot] a)) \[CenterDot] (b \[CenterDot] a)) == 
          ((a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] a) \[CenterDot] 
           ((a \[CenterDot] ((a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
              a)) \[CenterDot] (((a \[CenterDot] (b \[CenterDot] 
                a)) \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
              ((a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] a))))], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 19}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] 
            (((b_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] (
                b_))) \[CenterDot] ((((b_) \[CenterDot] (((b_) \[CenterDot] 
                  (c_)) \[CenterDot] (b_))) \[CenterDot] ((c_) \[CenterDot] 
                (((c_) \[CenterDot] (a_)) \[CenterDot] (c_)))) \[CenterDot] 
              ((b_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] 
                (b_))))) -> c \[CenterDot] ((c \[CenterDot] a) \[CenterDot] 
             c), "Side" -> 1, "Subpattern" -> ((b_) \[CenterDot] 
            (((b_) \[CenterDot] (c_)) \[CenterDot] (b_))) \[CenterDot] 
           ((c_) \[CenterDot] (((c_) \[CenterDot] (a_)) \[CenterDot] (c_))), 
         "MatchingConstruct" -> {"SubstitutionLemma", 8}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          ((a_) \[CenterDot] (b_)) \[CenterDot] (((c_) \[CenterDot] 
              (a_)) \[CenterDot] ((((c_) \[CenterDot] (a_)) \[CenterDot] (
                b_)) \[CenterDot] ((c_) \[CenterDot] (a_)))) -> b, 
         "MatchingSide" -> 1, "Position" -> {2, 2, 1}|>|>, 
     {"CriticalPairLemma", 22} -> <|"Statement" -> 
        HoldForm[((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                 a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
              a)) \[CenterDot] a) \[CenterDot] ((a \[CenterDot] 
             ((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                   a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                  ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                a)) \[CenterDot] a)) \[CenterDot] 
            (((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                   a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                  ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                a)) \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
              ((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                    a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                 a)) \[CenterDot] a)))) == (a \[CenterDot] 
            ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
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
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 21}, 
         "Orientation" -> 1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((((a_) \[CenterDot] (b_)) \[CenterDot] (((b_) \[CenterDot] 
                ((a_) \[CenterDot] (b_))) \[CenterDot] (b_))) \[CenterDot] 
             ((a_) \[CenterDot] (b_))) -> 
           ((b \[CenterDot] (a \[CenterDot] b)) \[CenterDot] b) \[CenterDot] 
            ((b \[CenterDot] ((b \[CenterDot] (a \[CenterDot] 
                 b)) \[CenterDot] b)) \[CenterDot] (((b \[CenterDot] 
                (a \[CenterDot] b)) \[CenterDot] b) \[CenterDot] 
              (b \[CenterDot] ((b \[CenterDot] (a \[CenterDot] 
                  b)) \[CenterDot] b)))), "Side" -> 1, "Subpattern" -> 
          (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"CriticalPairLemma", 2}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> (((((a_) \[CenterDot] (a_)) \[CenterDot] (
                a_)) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
              (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) \[CenterDot] 
            (a_) -> a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
         "MatchingSide" -> 1, "Position" -> {1}|>|>, 
     {"SubstitutionLemma", 20} -> <|"Statement" -> 
        HoldForm[((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                 a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
              a)) \[CenterDot] a) \[CenterDot] ((a \[CenterDot] 
             ((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                   a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                  ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                a)) \[CenterDot] a)) \[CenterDot] 
            (((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                   a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                  ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                a)) \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
              ((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                    a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                   ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                 a)) \[CenterDot] a)))) == (a \[CenterDot] 
            ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a)) \[CenterDot] ((a \[CenterDot] (((((a \[CenterDot] 
                    a) \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
                 (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                   a))) \[CenterDot] a)) \[CenterDot] a)) \[CenterDot] 
            (((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
               b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] a))) \[CenterDot] a))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 22}, 
         "Construct" -> {"CriticalPairLemma", 2}, "Position" -> {2, 1, 1}, 
         "Rule" -> (((((a_) \[CenterDot] (a_)) \[CenterDot] (
                a_)) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
              (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) \[CenterDot] 
            (a_) -> a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           ((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                   a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                  ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                a)) \[CenterDot] a) \[CenterDot] ((a \[CenterDot] (
                (a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
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
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 21} -> 
      <|"Statement" -> HoldForm[
         ((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
                b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                  a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
            a) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] (
                ((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
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
           (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a)) \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
                ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
              a)) \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] (
                (a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] a))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 20}, 
         "Construct" -> {"CriticalPairLemma", 2}, "Position" -> {2, 1, 2, 1, 
          2}, "Rule" -> (((((a_) \[CenterDot] (a_)) \[CenterDot] (
                a_)) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
              (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) \[CenterDot] 
            (a_) -> a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           ((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                   a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                  ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                a)) \[CenterDot] a) \[CenterDot] ((a \[CenterDot] (
                (a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
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
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 22} -> 
      <|"Statement" -> HoldForm[
         ((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
                b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                  a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
            a) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] (
                ((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
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
           (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] a))) \[CenterDot] 
            (((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
               b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] a))) \[CenterDot] a))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 21}, 
         "Construct" -> {"CriticalPairLemma", 3}, "Position" -> {2, 1, 2}, 
         "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] 
                (a_)) \[CenterDot] (a_)))) \[CenterDot] (a_) -> 
           a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           ((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                   a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                  ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                a)) \[CenterDot] a) \[CenterDot] ((a \[CenterDot] (
                (a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
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
                  a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
              (((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
                 b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a))) \[CenterDot] a))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 23} -> 
      <|"Statement" -> HoldForm[
         ((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
                b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                  a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
            a) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] (
                ((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
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
           (a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] (
                (a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] a))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 22}, 
         "Construct" -> {"CriticalPairLemma", 4}, "Position" -> {2, 1}, 
         "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
              (a_))) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] (
                a_)) \[CenterDot] (a_))) -> a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[
           ((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                   a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                  ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                a)) \[CenterDot] a) \[CenterDot] ((a \[CenterDot] (
                (a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
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
               a)) \[CenterDot] (a \[CenterDot] (((((a \[CenterDot] 
                   a) \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
                (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a))) \[CenterDot] a))], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 24} -> <|"Statement" -> 
        HoldForm[((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                 a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
              a)) \[CenterDot] a) \[CenterDot] ((a \[CenterDot] 
             ((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                   a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                  ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                a)) \[CenterDot] a)) \[CenterDot] 
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
         "Construct" -> {"CriticalPairLemma", 2}, "Position" -> {2, 2}, 
         "Rule" -> (((((a_) \[CenterDot] (a_)) \[CenterDot] (
                a_)) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
              (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) \[CenterDot] 
            (a_) -> a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           ((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                   a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                  ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                a)) \[CenterDot] a) \[CenterDot] ((a \[CenterDot] (
                (a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
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
               a)) \[CenterDot] (a \[CenterDot] (a \[CenterDot] (
                (a \[CenterDot] a) \[CenterDot] a)))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 25} -> 
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
              a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 24}, 
         "Construct" -> {"CriticalPairLemma", 2}, "Position" -> {1, 1, 2}, 
         "Rule" -> (((((a_) \[CenterDot] (a_)) \[CenterDot] (
                a_)) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
              (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) \[CenterDot] 
            (a_) -> a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                 a))) \[CenterDot] a) \[CenterDot] ((a \[CenterDot] (
                (a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
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
               a)) \[CenterDot] (a \[CenterDot] (a \[CenterDot] (
                (a \[CenterDot] a) \[CenterDot] a)))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 26} -> 
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
              a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 25}, 
         "Construct" -> {"CriticalPairLemma", 3}, "Position" -> {1}, 
         "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] 
                (a_)) \[CenterDot] (a_)))) \[CenterDot] (a_) -> 
           a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
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
                  a)) \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                ((a \[CenterDot] (((((a \[CenterDot] a) \[CenterDot] 
                      a) \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                     ((a \[CenterDot] a) \[CenterDot] a))) \[CenterDot] 
                   a)) \[CenterDot] a)))) == (a \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
              (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 27} -> 
      <|"Statement" -> HoldForm[
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
              a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 26}, 
         "Construct" -> {"CriticalPairLemma", 2}, "Position" -> {2, 1, 2, 1, 
          2}, "Rule" -> (((((a_) \[CenterDot] (a_)) \[CenterDot] (
                a_)) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
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
                     a))) \[CenterDot] a)) \[CenterDot] a) \[CenterDot] (
                a \[CenterDot] ((a \[CenterDot] (((((a \[CenterDot] 
                       a) \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
                    (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                      a))) \[CenterDot] a)) \[CenterDot] a)))) == 
            (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
             (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 28} -> 
      <|"Statement" -> HoldForm[
         (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a))) \[CenterDot] (((a \[CenterDot] (((((a \[CenterDot] 
                    a) \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
                 (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                   a))) \[CenterDot] a)) \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] ((a \[CenterDot] (((((a \[CenterDot] 
                     a) \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
                  (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                    a))) \[CenterDot] a)) \[CenterDot] a)))) == 
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 27}, 
         "Construct" -> {"CriticalPairLemma", 3}, "Position" -> {2, 1, 2}, 
         "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] 
                (a_)) \[CenterDot] (a_)))) \[CenterDot] (a_) -> 
           a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a), 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
             ((a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                  a) \[CenterDot] a))) \[CenterDot] (((a \[CenterDot] 
                 (((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
                    b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                      a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
                a) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                  (((((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
                     b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                       a) \[CenterDot] a))) \[CenterDot] a)) \[CenterDot] 
                 a)))) == (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a)) \[CenterDot] (a \[CenterDot] (a \[CenterDot] (
                (a \[CenterDot] a) \[CenterDot] a)))], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 23} -> 
      <|"Statement" -> HoldForm[b \[CenterDot] ((b \[CenterDot] 
             a) \[CenterDot] b) == (a \[CenterDot] (b \[CenterDot] 
             ((b \[CenterDot] a) \[CenterDot] b))) \[CenterDot] 
           (((b \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a)))], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 5}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((((c_) \[CenterDot] (x3_)) \[CenterDot] (a_)) \[CenterDot] 
             (((((c_) \[CenterDot] (x3_)) \[CenterDot] (a_)) \[CenterDot] (
                b_)) \[CenterDot] (((c_) \[CenterDot] (x3_)) \[CenterDot] (
                a_)))) -> b, "Side" -> 1, "Subpattern" -> 
          (((c_) \[CenterDot] (x3_)) \[CenterDot] (a_)) \[CenterDot] (b_), 
         "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] (
                c_)) \[CenterDot] (a_))) -> c, "MatchingSide" -> 1, 
         "Position" -> {2, 2, 1}|>|>, {"SubstitutionLemma", 29} -> 
      <|"Statement" -> HoldForm[
         (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) == 
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 28}, 
         "Construct" -> {"CriticalPairLemma", 23}, "Position" -> {2}, 
         "Rule" -> ((a_) \[CenterDot] ((b_) \[CenterDot] (((b_) \[CenterDot] 
                (a_)) \[CenterDot] (b_)))) \[CenterDot] 
            ((((b_) \[CenterDot] (c_)) \[CenterDot] (a_)) \[CenterDot] 
             ((a_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] (
                a_)))) -> b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
             (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) == 
            (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
             (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 30} -> 
      <|"Statement" -> HoldForm[a == (a \[CenterDot] 
            ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 29}, 
         "Construct" -> {"CriticalPairLemma", 4}, "Position" -> {}, 
         "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
              (a_))) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] (
                a_)) \[CenterDot] (a_))) -> a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[a == (a \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
              (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 31} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] a == a \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] a)], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 17}, 
         "Construct" -> {"SubstitutionLemma", 30}, "Position" -> {2}, 
         "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
              (a_))) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
              (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) -> a, 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[a \[CenterDot] a == 
            a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 32} -> 
      <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
           ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            a)], "Proof" -> <|"Input" -> {"SubstitutionLemma", 6}, 
         "Construct" -> {"SubstitutionLemma", 31}, "Position" -> {1}, 
         "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (a_)) -> a \[CenterDot] a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
             ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a)) \[CenterDot] a)], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 33} -> <|"Statement" -> 
        HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] a)], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 32}, 
         "Construct" -> {"SubstitutionLemma", 31}, "Position" -> {2, 1}, 
         "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (a_)) -> a \[CenterDot] a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] a)], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 24} -> <|"Statement" -> 
        HoldForm[(a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             a)) \[CenterDot] a == a \[CenterDot] (a \[CenterDot] 
            ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a)) \[CenterDot] a))], "Proof" -> 
        <|"Construct" -> {"SubstitutionLemma", 4}, "Orientation" -> -1, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] (((b_) \[CenterDot] (((b_) \[CenterDot] 
                   (a_)) \[CenterDot] (b_))) \[CenterDot] (a_))) \[CenterDot] 
              (a_))) -> (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
              b)) \[CenterDot] a, "Side" -> 1, "Subpattern" -> 
          (b_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] (b_)), 
         "MatchingConstruct" -> {"SubstitutionLemma", 31}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)) -> 
           a \[CenterDot] a, "MatchingSide" -> 1, "Position" -> {2, 2, 1, 2, 
          1}|>|>, {"SubstitutionLemma", 34} -> 
      <|"Statement" -> HoldForm[
         (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           a == a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
              a) \[CenterDot] a))], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 24}, "Construct" -> 
          {"SubstitutionLemma", 31}, "Position" -> {2, 2, 1}, 
         "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (a_)) -> a \[CenterDot] a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[(a \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] a)) \[CenterDot] a == a \[CenterDot] 
             (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 35} -> 
      <|"Statement" -> HoldForm[
         (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           a == a \[CenterDot] (a \[CenterDot] a)], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 34}, 
         "Construct" -> {"SubstitutionLemma", 31}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (a_)) -> a \[CenterDot] a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[(a \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] a)) \[CenterDot] a == a \[CenterDot] 
             (a \[CenterDot] a)], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 36} -> <|"Statement" -> 
        HoldForm[(a \[CenterDot] a) \[CenterDot] a == a \[CenterDot] 
           (a \[CenterDot] a)], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 35}, "Construct" -> 
          {"SubstitutionLemma", 31}, "Position" -> {1}, 
         "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (a_)) -> a \[CenterDot] a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[(a \[CenterDot] a) \[CenterDot] a == 
            a \[CenterDot] (a \[CenterDot] a)], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 37} -> <|"Statement" -> 
        HoldForm[a == (a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
            (a \[CenterDot] a))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 33}, "Construct" -> 
          {"SubstitutionLemma", 36}, "Position" -> {2}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (a_) -> 
           a \[CenterDot] (a \[CenterDot] a), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] (a \[CenterDot] a))], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 25} -> <|"Statement" -> 
        HoldForm[((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
             b)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))) == 
          (a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
             a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            (((((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                 b)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
                ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                  b)))) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
                (a \[CenterDot] b)) \[CenterDot] ((a \[CenterDot] 
                 b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
                 (a \[CenterDot] b))))) \[CenterDot] 
             (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                b)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (
                (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))))))], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 5}, 
         "Orientation" -> -1, "Rule" -> 
          ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
              (a_))) \[CenterDot] (((((a_) \[CenterDot] (c_)) \[CenterDot] (
                b_)) \[CenterDot] (x3_)) \[CenterDot] 
             ((((((a_) \[CenterDot] (c_)) \[CenterDot] (b_)) \[CenterDot] 
                (x3_)) \[CenterDot] ((((a_) \[CenterDot] (c_)) \[CenterDot] 
                 (b_)) \[CenterDot] ((b_) \[CenterDot] (((a_) \[CenterDot] 
                   (c_)) \[CenterDot] (b_))))) \[CenterDot] 
              ((((a_) \[CenterDot] (c_)) \[CenterDot] (b_)) \[CenterDot] (
                x3_)))) -> ((a \[CenterDot] c) \[CenterDot] b) \[CenterDot] 
            (b \[CenterDot] ((a \[CenterDot] c) \[CenterDot] b)), 
         "Side" -> 1, "Subpattern" -> (((a_) \[CenterDot] (c_)) \[CenterDot] 
            (b_)) \[CenterDot] (x3_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 37}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] ((a_) \[CenterDot] (a_))) -> a, 
         "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
     {"SubstitutionLemma", 38} -> <|"Statement" -> 
        HoldForm[((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
             b)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))) == 
          (a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
             a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            ((((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                b)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (
                (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                 b)))) \[CenterDot] ((((a \[CenterDot] b) \[CenterDot] 
                (a \[CenterDot] b)) \[CenterDot] ((a \[CenterDot] 
                 b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
                 (a \[CenterDot] b)))) \[CenterDot] (((a \[CenterDot] 
                 b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] (
                (a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] 
                  b) \[CenterDot] (a \[CenterDot] b)))))))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 25}, 
         "Construct" -> {"SubstitutionLemma", 36}, "Position" -> {2, 2}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (a_) -> 
           a \[CenterDot] (a \[CenterDot] a), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[((a \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] b)) \[CenterDot] ((a \[CenterDot] 
               b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (
                a \[CenterDot] b))) == (a \[CenterDot] ((a \[CenterDot] 
                (a \[CenterDot] b)) \[CenterDot] a)) \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] ((((a \[CenterDot] 
                  b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
                ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] 
                   b) \[CenterDot] (a \[CenterDot] b)))) \[CenterDot] (
                (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                   b)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
                  ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                    b)))) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
                  (a \[CenterDot] b)) \[CenterDot] ((a \[CenterDot] 
                   b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
                   (a \[CenterDot] b)))))))], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 39} -> <|"Statement" -> 
        HoldForm[((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
             b)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))) == 
          (a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
             a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] ((((a \[CenterDot] 
                 b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] (
                (a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] 
                  b) \[CenterDot] (a \[CenterDot] b)))) \[CenterDot] 
              (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                 b)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
                ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)))))))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 38}, 
         "Construct" -> {"SubstitutionLemma", 37}, "Position" -> {2, 2, 1}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
             ((a_) \[CenterDot] (a_))) -> a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[((a \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] b)) \[CenterDot] ((a \[CenterDot] 
               b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (
                a \[CenterDot] b))) == (a \[CenterDot] ((a \[CenterDot] 
                (a \[CenterDot] b)) \[CenterDot] a)) \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] 
                b) \[CenterDot] ((((a \[CenterDot] b) \[CenterDot] 
                  (a \[CenterDot] b)) \[CenterDot] ((a \[CenterDot] 
                   b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
                   (a \[CenterDot] b)))) \[CenterDot] (((a \[CenterDot] 
                   b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
                 ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] 
                    b) \[CenterDot] (a \[CenterDot] b)))))))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 40} -> 
      <|"Statement" -> HoldForm[((a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))) == 
          (a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
             a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                 b)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
                ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)))))))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 39}, 
         "Construct" -> {"SubstitutionLemma", 37}, "Position" -> {2, 2, 2, 
          1}, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] ((a_) \[CenterDot] (a_))) -> a, 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] 
                b) \[CenterDot] (a \[CenterDot] b))) == 
            (a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
               a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] 
                 b) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
                  (a \[CenterDot] b)) \[CenterDot] ((a \[CenterDot] 
                   b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
                   (a \[CenterDot] b)))))))], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 41} -> <|"Statement" -> 
        HoldForm[((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
             b)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))) == 
          (a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
             a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] b))))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 40}, "Construct" -> 
          {"SubstitutionLemma", 37}, "Position" -> {2, 2, 2, 2}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
             ((a_) \[CenterDot] (a_))) -> a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[((a \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] b)) \[CenterDot] ((a \[CenterDot] 
               b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (
                a \[CenterDot] b))) == (a \[CenterDot] ((a \[CenterDot] 
                (a \[CenterDot] b)) \[CenterDot] a)) \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] 
                b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
                (a \[CenterDot] b))))], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 42} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] a == a \[CenterDot] (a \[CenterDot] 
            (a \[CenterDot] a))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 31}, "Construct" -> 
          {"SubstitutionLemma", 36}, "Position" -> {2}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (a_) -> 
           a \[CenterDot] (a \[CenterDot] a), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] a == 
            a \[CenterDot] (a \[CenterDot] (a \[CenterDot] a))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 43} -> 
      <|"Statement" -> HoldForm[((a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))) == 
          (a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
             a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 41}, "Construct" -> 
          {"SubstitutionLemma", 42}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
              (a_))) -> a \[CenterDot] a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[((a \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] b)) \[CenterDot] ((a \[CenterDot] 
               b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (
                a \[CenterDot] b))) == (a \[CenterDot] ((a \[CenterDot] 
                (a \[CenterDot] b)) \[CenterDot] a)) \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 44} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] b == 
          (a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
             a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 43}, "Construct" -> 
          {"SubstitutionLemma", 37}, "Position" -> {}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
             ((a_) \[CenterDot] (a_))) -> a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[a \[CenterDot] b == 
            (a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
               a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] b))], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 26} -> <|"Statement" -> 
        HoldForm[(a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b) == 
          (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
            ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                b))) \[CenterDot] a))], "Proof" -> 
        <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
         "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
            ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
              (a_))) -> c, "Side" -> 1, "Subpattern" -> 
          ((a_) \[CenterDot] (b_)) \[CenterDot] (c_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 44}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (
                (a_) \[CenterDot] (b_))) \[CenterDot] (a_))) \[CenterDot] 
            (((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
              (b_))) -> a \[CenterDot] b, "MatchingSide" -> 1, 
         "Position" -> {1}|>|>, {"CriticalPairLemma", 27} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] ((a \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
               b))) \[CenterDot] a) == ((a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
               b)) \[CenterDot] (a \[CenterDot] b)))], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 8}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] 
            (((b_) \[CenterDot] (c_)) \[CenterDot] ((((b_) \[CenterDot] 
                (c_)) \[CenterDot] ((b_) \[CenterDot] (((b_) \[CenterDot] 
                  (a_)) \[CenterDot] (b_)))) \[CenterDot] ((b_) \[CenterDot] (
                c_)))) -> b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), 
         "Side" -> 1, "Subpattern" -> ((b_) \[CenterDot] (c_)) \[CenterDot] 
           ((b_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] (b_))), 
         "MatchingConstruct" -> {"CriticalPairLemma", 26}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          ((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
                ((a_) \[CenterDot] (b_)))) \[CenterDot] (a_))) -> 
           (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b), 
         "MatchingSide" -> 1, "Position" -> {2, 2, 1}|>|>, 
     {"SubstitutionLemma", 45} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
               b) \[CenterDot] (a \[CenterDot] b))) \[CenterDot] a) == 
          ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 27}, 
         "Construct" -> {"SubstitutionLemma", 36}, "Position" -> {2, 2}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (a_) -> 
           a \[CenterDot] (a \[CenterDot] a), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] 
             ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
                (a \[CenterDot] b))) \[CenterDot] a) == 
            ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] 
                b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
                (a \[CenterDot] b))))], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 46} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
               b) \[CenterDot] (a \[CenterDot] b))) \[CenterDot] a) == 
          ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 45}, 
         "Construct" -> {"SubstitutionLemma", 42}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
              (a_))) -> a \[CenterDot] a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] 
             ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
                (a \[CenterDot] b))) \[CenterDot] a) == 
            ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 28} -> 
      <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
           (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                ((a \[CenterDot] b) \[CenterDot] c)) \[CenterDot] 
               a))) \[CenterDot] (a \[CenterDot] b)) == 
          (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
               c)) \[CenterDot] a)) \[CenterDot] 
           (((a \[CenterDot] b) \[CenterDot] c) \[CenterDot] 
            ((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                  b) \[CenterDot] c)) \[CenterDot] a)) \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] c)))], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 8}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] 
            (((b_) \[CenterDot] (c_)) \[CenterDot] ((((b_) \[CenterDot] 
                (c_)) \[CenterDot] ((b_) \[CenterDot] (((b_) \[CenterDot] 
                  (a_)) \[CenterDot] (b_)))) \[CenterDot] ((b_) \[CenterDot] (
                c_)))) -> b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), 
         "Side" -> 1, "Subpattern" -> ((b_) \[CenterDot] (c_)) \[CenterDot] 
           ((b_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] (b_))), 
         "MatchingConstruct" -> {"CriticalPairLemma", 8}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          (a_) \[CenterDot] (((b_) \[CenterDot] (c_)) \[CenterDot] 
             ((((b_) \[CenterDot] (c_)) \[CenterDot] ((b_) \[CenterDot] 
                (((b_) \[CenterDot] (a_)) \[CenterDot] (b_)))) \[CenterDot] 
              ((b_) \[CenterDot] (c_)))) -> b \[CenterDot] 
            ((b \[CenterDot] a) \[CenterDot] b), "MatchingSide" -> 1, 
         "Position" -> {2, 2, 1}|>|>, {"CriticalPairLemma", 29} -> 
      <|"Statement" -> HoldForm[
         (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                  a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                ((a \[CenterDot] a) \[CenterDot] a)))) \[CenterDot] 
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
           (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] a))) \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] a)))], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 28}, 
         "Orientation" -> 1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] (
                ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
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
              (a_))) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] (
                a_)) \[CenterDot] (a_))) -> a, "MatchingSide" -> 1, 
         "Position" -> {2, 1, 2, 2, 1, 2}|>|>, {"SubstitutionLemma", 47} -> 
      <|"Statement" -> HoldForm[
         (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                  a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                ((a \[CenterDot] a) \[CenterDot] a)))) \[CenterDot] 
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
              a)))], "Proof" -> <|"Input" -> {"CriticalPairLemma", 29}, 
         "Construct" -> {"CriticalPairLemma", 4}, "Position" -> {2, 1}, 
         "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
              (a_))) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] (
                a_)) \[CenterDot] (a_))) -> a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[(a \[CenterDot] ((a \[CenterDot] 
                ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                   a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                    a) \[CenterDot] a)))) \[CenterDot] a)) \[CenterDot] 
             (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                 a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                  a) \[CenterDot] a))) \[CenterDot] ((a \[CenterDot] 
                ((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
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
           (((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] a))) \[CenterDot] 
            ((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                  ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                 (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                   a)))) \[CenterDot] a)) \[CenterDot] 
             ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] a))))) == (a \[CenterDot] 
            ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 47}, 
         "Construct" -> {"CriticalPairLemma", 4}, "Position" -> {1, 2, 1, 2}, 
         "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
              (a_))) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] (
                a_)) \[CenterDot] (a_))) -> a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[(a \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] a)) \[CenterDot] (((a \[CenterDot] 
                ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] (
                a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                 a))) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                  ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                     a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                      a) \[CenterDot] a)))) \[CenterDot] a)) \[CenterDot] (
                (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                  a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                   a) \[CenterDot] a))))) == (a \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
              (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 49} -> 
      <|"Statement" -> HoldForm[
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
              a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 48}, 
         "Construct" -> {"CriticalPairLemma", 4}, "Position" -> {2, 1}, 
         "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
              (a_))) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] (
                a_)) \[CenterDot] (a_))) -> a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[(a \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
              ((a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                    ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                   (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                     a)))) \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a))))) == 
            (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
             (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 50} -> 
      <|"Statement" -> HoldForm[
         (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a)) \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                 a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] (
                (a \[CenterDot] a) \[CenterDot] a))))) == 
          (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 49}, 
         "Construct" -> {"CriticalPairLemma", 4}, "Position" -> {2, 2, 1, 2, 
          1, 2}, "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (
                a_)) \[CenterDot] (a_))) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (a_))) -> a, 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
             (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                  a) \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
                 ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
                (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a))))) == 
            (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
             (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                a)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 51} -> 
      <|"Statement" -> HoldForm[
         (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a)) \[CenterDot] a)) == (a \[CenterDot] 
            ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 50}, 
         "Construct" -> {"CriticalPairLemma", 4}, "Position" -> {2, 2, 2}, 
         "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
              (a_))) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] (
                a_)) \[CenterDot] (a_))) -> a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[(a \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
              ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                 a)) \[CenterDot] a)) == (a \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
              (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 52} -> 
      <|"Statement" -> HoldForm[
         (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a)) \[CenterDot] a)) == a], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 51}, "Construct" -> 
          {"SubstitutionLemma", 30}, "Position" -> {}, 
         "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
              (a_))) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
              (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)))) -> a, 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
             (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                  a) \[CenterDot] a)) \[CenterDot] a)) == a], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 53} -> 
      <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
           (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               a)) \[CenterDot] a)) == a], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 52}, "Construct" -> 
          {"SubstitutionLemma", 31}, "Position" -> {1}, 
         "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (a_)) -> a \[CenterDot] a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                  a) \[CenterDot] a)) \[CenterDot] a)) == a], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 54} -> 
      <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
           (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) == a], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 53}, 
         "Construct" -> {"SubstitutionLemma", 31}, "Position" -> {2, 2, 1}, 
         "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (a_)) -> a \[CenterDot] a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) == a], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 55} -> 
      <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
           (a \[CenterDot] a) == a], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 54}, "Construct" -> 
          {"SubstitutionLemma", 31}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (a_)) -> a \[CenterDot] a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] a) == a], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 56} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
               b) \[CenterDot] (a \[CenterDot] b))) \[CenterDot] a) == 
          a \[CenterDot] b], "Proof" -> <|"Input" -> {"SubstitutionLemma", 
           46}, "Construct" -> {"SubstitutionLemma", 55}, "Position" -> {}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
             (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                 b) \[CenterDot] (a \[CenterDot] b))) \[CenterDot] a) == 
            a \[CenterDot] b], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 30} -> <|"Statement" -> 
        HoldForm[(a \[CenterDot] b) \[CenterDot] 
           ((c \[CenterDot] a) \[CenterDot] (((c \[CenterDot] a) \[CenterDot] 
              b) \[CenterDot] (c \[CenterDot] a))) == 
          (a \[CenterDot] b) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
             (b \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (
                (c \[CenterDot] a) \[CenterDot] (((c \[CenterDot] 
                   a) \[CenterDot] b) \[CenterDot] (c \[CenterDot] 
                  a)))))) \[CenterDot] (a \[CenterDot] b))], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 56}, 
         "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] 
            (((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] (
                (a_) \[CenterDot] (b_)))) \[CenterDot] (a_)) -> 
           a \[CenterDot] b, "Side" -> 1, "Subpattern" -> 
          (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 8}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            (((c_) \[CenterDot] (a_)) \[CenterDot] ((((c_) \[CenterDot] 
                (a_)) \[CenterDot] (b_)) \[CenterDot] ((c_) \[CenterDot] (
                a_)))) -> b, "MatchingSide" -> 1, "Position" -> {2, 1, 2, 
          1}|>|>, {"SubstitutionLemma", 57} -> 
      <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
           ((c \[CenterDot] a) \[CenterDot] (((c \[CenterDot] a) \[CenterDot] 
              b) \[CenterDot] (c \[CenterDot] a))) == 
          (a \[CenterDot] b) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
             (b \[CenterDot] b)) \[CenterDot] (a \[CenterDot] b))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 30}, 
         "Construct" -> {"SubstitutionLemma", 8}, "Position" -> {2, 1, 2, 2}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            (((c_) \[CenterDot] (a_)) \[CenterDot] ((((c_) \[CenterDot] 
                (a_)) \[CenterDot] (b_)) \[CenterDot] ((c_) \[CenterDot] (
                a_)))) -> b, "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[(a \[CenterDot] b) \[CenterDot] ((c \[CenterDot] 
               a) \[CenterDot] (((c \[CenterDot] a) \[CenterDot] 
                b) \[CenterDot] (c \[CenterDot] a))) == 
            (a \[CenterDot] b) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
               (b \[CenterDot] b)) \[CenterDot] (a \[CenterDot] b))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 58} -> 
      <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
           (((a \[CenterDot] b) \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
            (a \[CenterDot] b))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 57}, "Construct" -> 
          {"SubstitutionLemma", 8}, "Position" -> {}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            (((c_) \[CenterDot] (a_)) \[CenterDot] ((((c_) \[CenterDot] 
                (a_)) \[CenterDot] (b_)) \[CenterDot] ((c_) \[CenterDot] (
                a_)))) -> b, "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
             (((a \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
                b)) \[CenterDot] (a \[CenterDot] b))], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 31} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] ((a \[CenterDot] 
             c) \[CenterDot] a) == (((a \[CenterDot] b) \[CenterDot] 
             c) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] 
              a))) \[CenterDot] ((c \[CenterDot] ((a \[CenterDot] (
                (a \[CenterDot] c) \[CenterDot] a)) \[CenterDot] 
              (a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] 
                a)))) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
              c) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                c) \[CenterDot] a))))], "Proof" -> 
        <|"Construct" -> {"SubstitutionLemma", 58}, "Orientation" -> -1, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] (
                b_))) \[CenterDot] ((a_) \[CenterDot] (b_))) -> b, 
         "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] (b_), 
         "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] (
                c_)) \[CenterDot] (a_))) -> c, "MatchingSide" -> 1, 
         "Position" -> {2, 1, 1}|>|>, {"SubstitutionLemma", 59} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] ((a \[CenterDot] 
             c) \[CenterDot] a) == c \[CenterDot] 
           ((c \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] 
                a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                 c) \[CenterDot] a)))) \[CenterDot] 
            (((a \[CenterDot] b) \[CenterDot] c) \[CenterDot] 
             (a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] a))))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 31}, 
         "Construct" -> {"Axiom", 1}, "Position" -> {1}, 
         "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
            ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
              (a_))) -> c, "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] a) == 
            c \[CenterDot] ((c \[CenterDot] ((a \[CenterDot] 
                 ((a \[CenterDot] c) \[CenterDot] a)) \[CenterDot] 
                (a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] 
                  a)))) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
                c) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                  c) \[CenterDot] a))))], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 32} -> <|"Statement" -> 
        HoldForm[a == ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] (a \[CenterDot] a)))], 
       "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
         "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
            ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
              (a_))) -> c, "Side" -> 1, "Subpattern" -> 
          ((a_) \[CenterDot] (c_)) \[CenterDot] (a_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 36}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (a_) -> 
           a \[CenterDot] (a \[CenterDot] a), "MatchingSide" -> 1, 
         "Position" -> {2, 2}|>|>, {"SubstitutionLemma", 60} -> 
      <|"Statement" -> HoldForm[a == ((a \[CenterDot] b) \[CenterDot] 
            a) \[CenterDot] (a \[CenterDot] a)], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 32}, 
         "Construct" -> {"SubstitutionLemma", 42}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
              (a_))) -> a \[CenterDot] a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a == ((a \[CenterDot] b) \[CenterDot] 
              a) \[CenterDot] (a \[CenterDot] a)], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 33} -> <|"Statement" -> 
        HoldForm[b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b) == 
          a \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
              b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
               a) \[CenterDot] b)))], "Proof" -> 
        <|"Construct" -> {"SubstitutionLemma", 60}, "Orientation" -> -1, 
         "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] (a_)) -> a, "Side" -> 1, 
         "Subpattern" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_), 
         "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (c_)) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] (
                c_)) \[CenterDot] (a_))) -> c, "MatchingSide" -> 1, 
         "Position" -> {1}|>|>, {"SubstitutionLemma", 61} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] ((a \[CenterDot] 
             c) \[CenterDot] a) == c \[CenterDot] 
           ((a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] a)) \[CenterDot] 
            (((a \[CenterDot] b) \[CenterDot] c) \[CenterDot] 
             (a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] a))))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 59}, 
         "Construct" -> {"CriticalPairLemma", 33}, "Position" -> {2, 1}, 
         "Rule" -> (a_) \[CenterDot] (((b_) \[CenterDot] (((b_) \[CenterDot] 
                (a_)) \[CenterDot] (b_))) \[CenterDot] ((b_) \[CenterDot] 
              (((b_) \[CenterDot] (a_)) \[CenterDot] (b_)))) -> 
           b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] a) == 
            c \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] 
                a)) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
                c) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                  c) \[CenterDot] a))))], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 62} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] a) == 
          c \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] 
              a)) \[CenterDot] c)], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 61}, "Construct" -> {"Axiom", 1}, 
         "Position" -> {2, 2}, "Rule" -> 
          (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
            ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
              (a_))) -> c, "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] a) == 
            c \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] 
                a)) \[CenterDot] c)], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 63} -> <|"Statement" -> 
        HoldForm[(b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
             b)) \[CenterDot] a == a \[CenterDot] (a \[CenterDot] 
            ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b)) \[CenterDot] a))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 4}, "Construct" -> 
          {"SubstitutionLemma", 62}, "Position" -> {2, 2, 1}, 
         "Rule" -> (a_) \[CenterDot] (((b_) \[CenterDot] (((b_) \[CenterDot] 
                (a_)) \[CenterDot] (b_))) \[CenterDot] (a_)) -> 
           b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
             a == a \[CenterDot] (a \[CenterDot] ((b \[CenterDot] 
                ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] a))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 64} -> 
      <|"Statement" -> HoldForm[
         (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           a == a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
              a) \[CenterDot] b))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 63}, "Construct" -> 
          {"SubstitutionLemma", 62}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] (((b_) \[CenterDot] (((b_) \[CenterDot] 
                (a_)) \[CenterDot] (b_))) \[CenterDot] (a_)) -> 
           b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
             a == a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                a) \[CenterDot] b))], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 65} -> <|"Statement" -> 
        HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
           ((a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b))) \[CenterDot] a)], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 7}, "Construct" -> 
          {"SubstitutionLemma", 64}, "Position" -> {2, 1}, 
         "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
              (a_))) \[CenterDot] (b_) -> b \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] a)), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
             ((a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                  a) \[CenterDot] b))) \[CenterDot] a)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 66} -> 
      <|"Statement" -> HoldForm[b \[CenterDot] ((b \[CenterDot] 
             a) \[CenterDot] b) == a \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 62}, 
         "Construct" -> {"SubstitutionLemma", 64}, "Position" -> {2}, 
         "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
              (a_))) \[CenterDot] (b_) -> b \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] a)), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[b \[CenterDot] 
             ((b \[CenterDot] a) \[CenterDot] b) == a \[CenterDot] 
             (a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                b)))], "Source" -> "norm"|>|>, {"CriticalPairLemma", 34} -> 
      <|"Statement" -> HoldForm[a == ((a \[CenterDot] b) \[CenterDot] 
            a) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             a))], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 58}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] (
                b_))) \[CenterDot] ((a_) \[CenterDot] (b_))) -> b, 
         "Side" -> 1, "Subpattern" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           ((b_) \[CenterDot] (b_)), "MatchingConstruct" -> 
          {"SubstitutionLemma", 60}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_)) \[CenterDot] ((a_) \[CenterDot] (a_)) -> a, 
         "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
     {"CriticalPairLemma", 35} -> <|"Statement" -> 
        HoldForm[(a \[CenterDot] b) \[CenterDot] a == 
          ((((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
              b) \[CenterDot] a)) \[CenterDot] 
           (((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)))], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 34}, 
         "Orientation" -> -1, "Rule" -> 
          (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
              (a_))) -> a, "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] 
           (b_), "MatchingConstruct" -> {"SubstitutionLemma", 60}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] (a_)) -> a, "MatchingSide" -> 1, 
         "Position" -> {2, 2, 1}|>|>, {"SubstitutionLemma", 67} -> 
      <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] a == 
          (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
           (((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 35}, 
         "Construct" -> {"SubstitutionLemma", 60}, "Position" -> {1, 1}, 
         "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] (a_)) -> a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[(a \[CenterDot] b) \[CenterDot] a == 
            (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
             (((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 68} -> 
      <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] a == 
          (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
           a], "Proof" -> <|"Input" -> {"SubstitutionLemma", 67}, 
         "Construct" -> {"CriticalPairLemma", 34}, "Position" -> {2}, 
         "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
              (a_))) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[(a \[CenterDot] b) \[CenterDot] a == 
            (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
             a], "Source" -> "norm"|>|>, {"CriticalPairLemma", 36} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] ((a \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] a)) \[CenterDot] a) == 
          ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
           (((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)))], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 66}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
             ((b_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] (
                b_)))) -> b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), 
         "Side" -> 1, "Subpattern" -> ((b_) \[CenterDot] (a_)) \[CenterDot] 
           (b_), "MatchingConstruct" -> {"SubstitutionLemma", 68}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
              (a_))) \[CenterDot] (a_) -> (a \[CenterDot] b) \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {2, 2, 2}|>|>, 
     {"SubstitutionLemma", 69} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
               b) \[CenterDot] a)) \[CenterDot] a) == 
          ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] a], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 36}, 
         "Construct" -> {"CriticalPairLemma", 34}, "Position" -> {2}, 
         "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
              (a_))) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                 b) \[CenterDot] a)) \[CenterDot] a) == 
            ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] a], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 70} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] ((a \[CenterDot] 
             b) \[CenterDot] a) == ((a \[CenterDot] b) \[CenterDot] 
            a) \[CenterDot] a], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 69}, "Construct" -> 
          {"SubstitutionLemma", 68}, "Position" -> {2}, 
         "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
              (a_))) \[CenterDot] (a_) -> (a \[CenterDot] b) \[CenterDot] a, 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a) == 
            ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] a], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 37} -> 
      <|"Statement" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
           (((b \[CenterDot] a) \[CenterDot] ((c \[CenterDot] b) \[CenterDot] 
              (((c \[CenterDot] b) \[CenterDot] a) \[CenterDot] (
                c \[CenterDot] b)))) \[CenterDot] (b \[CenterDot] a)) == 
          (a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
           (b \[CenterDot] a)], "Proof" -> 
        <|"Construct" -> {"SubstitutionLemma", 70}, "Orientation" -> -1, 
         "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
            (a_) -> a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a), 
         "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] (b_), 
         "MatchingConstruct" -> {"SubstitutionLemma", 8}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          ((a_) \[CenterDot] (b_)) \[CenterDot] (((c_) \[CenterDot] 
              (a_)) \[CenterDot] ((((c_) \[CenterDot] (a_)) \[CenterDot] (
                b_)) \[CenterDot] ((c_) \[CenterDot] (a_)))) -> b, 
         "MatchingSide" -> 1, "Position" -> {1, 1}|>|>, 
     {"SubstitutionLemma", 71} -> <|"Statement" -> 
        HoldForm[(b \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] a)) == (a \[CenterDot] (b \[CenterDot] 
             a)) \[CenterDot] (b \[CenterDot] a)], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 37}, 
         "Construct" -> {"SubstitutionLemma", 8}, "Position" -> {2, 1}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            (((c_) \[CenterDot] (a_)) \[CenterDot] ((((c_) \[CenterDot] 
                (a_)) \[CenterDot] (b_)) \[CenterDot] ((c_) \[CenterDot] (
                a_)))) -> b, "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[(b \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
              (b \[CenterDot] a)) == (a \[CenterDot] (b \[CenterDot] 
               a)) \[CenterDot] (b \[CenterDot] a)], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 38} -> <|"Statement" -> 
        HoldForm[(a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                b) \[CenterDot] (a \[CenterDot] b))) \[CenterDot] 
             a)) \[CenterDot] (((a \[CenterDot] ((a \[CenterDot] 
                b) \[CenterDot] (a \[CenterDot] b))) \[CenterDot] 
             a) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] (
                (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                 b))) \[CenterDot] a))) == 
          (((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                b))) \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
                (a \[CenterDot] b))) \[CenterDot] a))) \[CenterDot] 
           (a \[CenterDot] b)], "Proof" -> 
        <|"Construct" -> {"SubstitutionLemma", 71}, "Orientation" -> -1, 
         "Rule" -> ((a_) \[CenterDot] ((b_) \[CenterDot] (a_))) \[CenterDot] 
            ((b_) \[CenterDot] (a_)) -> (b \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] (b \[CenterDot] a)), "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] (a_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 56}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (a_) \[CenterDot] (((a_) \[CenterDot] 
              (((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
                (b_)))) \[CenterDot] (a_)) -> a \[CenterDot] b, 
         "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"SubstitutionLemma", 72} -> <|"Statement" -> 
        HoldForm[(a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                b) \[CenterDot] (a \[CenterDot] b))) \[CenterDot] 
             a)) \[CenterDot] (((a \[CenterDot] ((a \[CenterDot] 
                b) \[CenterDot] (a \[CenterDot] b))) \[CenterDot] 
             a) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] (
                (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                 b))) \[CenterDot] a))) == a \[CenterDot] 
           (a \[CenterDot] b)], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 38}, "Construct" -> 
          {"CriticalPairLemma", 34}, "Position" -> {1}, 
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
     {"SubstitutionLemma", 73} -> <|"Statement" -> 
        HoldForm[(a \[CenterDot] b) \[CenterDot] 
           (((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                b))) \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
                (a \[CenterDot] b))) \[CenterDot] a))) == 
          a \[CenterDot] (a \[CenterDot] b)], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 72}, "Construct" -> 
          {"SubstitutionLemma", 56}, "Position" -> {1}, 
         "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (((a_) \[CenterDot] 
                (b_)) \[CenterDot] ((a_) \[CenterDot] (b_)))) \[CenterDot] 
             (a_)) -> a \[CenterDot] b, "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
             (((a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
                 (a \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] ((a \[CenterDot] ((a \[CenterDot] 
                   b) \[CenterDot] (a \[CenterDot] b))) \[CenterDot] a))) == 
            a \[CenterDot] (a \[CenterDot] b)], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 74} -> <|"Statement" -> 
        HoldForm[(a \[CenterDot] b) \[CenterDot] a == a \[CenterDot] 
           (a \[CenterDot] b)], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 73}, "Construct" -> 
          {"CriticalPairLemma", 34}, "Position" -> {2}, 
         "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
              (a_))) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[(a \[CenterDot] b) \[CenterDot] a == a \[CenterDot] 
             (a \[CenterDot] b)], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 75} -> <|"Statement" -> 
        HoldForm[a == (a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
            (a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b))))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 65}, 
         "Construct" -> {"SubstitutionLemma", 74}, "Position" -> {2}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
           a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] 
                ((b \[CenterDot] a) \[CenterDot] b))))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 76} -> 
      <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] (
                b \[CenterDot] a)))))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 75}, "Construct" -> 
          {"SubstitutionLemma", 74}, "Position" -> {2, 2, 2, 2}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
           a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] 
                 (b \[CenterDot] a)))))], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 39} -> <|"Statement" -> 
        HoldForm[((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
              b)) \[CenterDot] (((b \[CenterDot] ((b \[CenterDot] 
                 a) \[CenterDot] b)) \[CenterDot] a) \[CenterDot] 
             (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b)))) \[CenterDot] a == a \[CenterDot] (a \[CenterDot] 
            ((a \[CenterDot] (((b \[CenterDot] ((b \[CenterDot] 
                   a) \[CenterDot] b)) \[CenterDot] a) \[CenterDot] 
               a)) \[CenterDot] a))], "Proof" -> 
        <|"Construct" -> {"SubstitutionLemma", 4}, "Orientation" -> -1, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] (((b_) \[CenterDot] (((b_) \[CenterDot] 
                   (a_)) \[CenterDot] (b_))) \[CenterDot] (a_))) \[CenterDot] 
              (a_))) -> (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
              b)) \[CenterDot] a, "Side" -> 1, "Subpattern" -> 
          ((b_) \[CenterDot] (a_)) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"Axiom", 1}, "MatchingOrientation" -> 1, "MatchingRule" -> 
          (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
            ((a_) \[CenterDot] (((a_) \[CenterDot] (c_)) \[CenterDot] 
              (a_))) -> c, "MatchingSide" -> 1, "Position" -> {2, 2, 1, 2, 1, 
          2}|>|>, {"SubstitutionLemma", 77} -> 
      <|"Statement" -> HoldForm[
         ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
            a) \[CenterDot] a == a \[CenterDot] (a \[CenterDot] 
            ((a \[CenterDot] (((b \[CenterDot] ((b \[CenterDot] 
                   a) \[CenterDot] b)) \[CenterDot] a) \[CenterDot] 
               a)) \[CenterDot] a))], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 39}, "Construct" -> {"Axiom", 1}, 
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
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 40} -> 
      <|"Statement" -> HoldForm[
         ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
            a) \[CenterDot] a == (a \[CenterDot] 
            (((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                b)) \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
             (a \[CenterDot] a)))], "Proof" -> 
        <|"Construct" -> {"SubstitutionLemma", 8}, "Orientation" -> -1, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            (((c_) \[CenterDot] (a_)) \[CenterDot] ((((c_) \[CenterDot] 
                (a_)) \[CenterDot] (b_)) \[CenterDot] ((c_) \[CenterDot] (
                a_)))) -> b, "Side" -> 1, "Subpattern" -> 
          ((c_) \[CenterDot] (a_)) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"CriticalPairLemma", 7}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((((b_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] 
                (b_))) \[CenterDot] (a_)) \[CenterDot] (a_)) -> a, 
         "MatchingSide" -> 1, "Position" -> {2, 2, 1}|>|>, 
     {"SubstitutionLemma", 78} -> <|"Statement" -> 
        HoldForm[((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
              b)) \[CenterDot] a) \[CenterDot] a == 
          (a \[CenterDot] (((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                b)) \[CenterDot] a) \[CenterDot] a)) \[CenterDot] a], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 40}, 
         "Construct" -> {"SubstitutionLemma", 37}, "Position" -> {2}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
             ((a_) \[CenterDot] (a_))) -> a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[
           ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
              a) \[CenterDot] a == (a \[CenterDot] (((b \[CenterDot] 
                 ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
                a) \[CenterDot] a)) \[CenterDot] a], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 79} -> <|"Statement" -> 
        HoldForm[((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
              b)) \[CenterDot] a) \[CenterDot] a == a \[CenterDot] 
           (a \[CenterDot] (((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                b)) \[CenterDot] a) \[CenterDot] a))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 77}, 
         "Construct" -> {"SubstitutionLemma", 78}, "Position" -> {2, 2}, 
         "Rule" -> ((a_) \[CenterDot] ((((b_) \[CenterDot] 
                (((b_) \[CenterDot] (a_)) \[CenterDot] (b_))) \[CenterDot] (
                a_)) \[CenterDot] (a_))) \[CenterDot] (a_) -> 
           ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
             a) \[CenterDot] a, "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                b)) \[CenterDot] a) \[CenterDot] a == a \[CenterDot] 
             (a \[CenterDot] (((b \[CenterDot] ((b \[CenterDot] 
                   a) \[CenterDot] b)) \[CenterDot] a) \[CenterDot] a))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 80} -> 
      <|"Statement" -> HoldForm[
         ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
            a) \[CenterDot] a == a \[CenterDot] (a \[CenterDot] 
            ((a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
                b))) \[CenterDot] a))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 79}, "Construct" -> 
          {"SubstitutionLemma", 64}, "Position" -> {2, 2, 1}, 
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
         "Construct" -> {"CriticalPairLemma", 11}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] ((b_) \[CenterDot] (((b_) \[CenterDot] 
                  (a_)) \[CenterDot] (b_)))) \[CenterDot] (a_))) -> 
           b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b), 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           ((b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
              a) \[CenterDot] a == b \[CenterDot] ((b \[CenterDot] 
               a) \[CenterDot] b)], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 82} -> <|"Statement" -> 
        HoldForm[(a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
               a) \[CenterDot] b))) \[CenterDot] a == b \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] b)], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 81}, 
         "Construct" -> {"SubstitutionLemma", 64}, "Position" -> {1}, 
         "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
              (a_))) \[CenterDot] (b_) -> b \[CenterDot] (a \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] a)), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[(a \[CenterDot] (b \[CenterDot] (
                (b \[CenterDot] a) \[CenterDot] b))) \[CenterDot] a == 
            b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 83} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b))) == 
          b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 82}, 
         "Construct" -> {"SubstitutionLemma", 74}, "Position" -> {}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
           a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
              (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b))) == 
            b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 84} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] (b \[CenterDot] (b \[CenterDot] a)))) == 
          b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 83}, 
         "Construct" -> {"SubstitutionLemma", 74}, "Position" -> {2, 2, 2}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
           a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
              (b \[CenterDot] (b \[CenterDot] (b \[CenterDot] a)))) == 
            b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 85} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] (b \[CenterDot] (b \[CenterDot] a)))) == 
          b \[CenterDot] (b \[CenterDot] (b \[CenterDot] a))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 84}, 
         "Construct" -> {"SubstitutionLemma", 74}, "Position" -> {2}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
           a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
              (b \[CenterDot] (b \[CenterDot] (b \[CenterDot] a)))) == 
            b \[CenterDot] (b \[CenterDot] (b \[CenterDot] a))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 86} -> 
      <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
           (b \[CenterDot] (b \[CenterDot] (b \[CenterDot] a)))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 76}, 
         "Construct" -> {"SubstitutionLemma", 85}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] 
              ((b_) \[CenterDot] ((b_) \[CenterDot] (a_))))) -> 
           b \[CenterDot] (b \[CenterDot] (b \[CenterDot] a)), 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           a == (a \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
              (b \[CenterDot] (b \[CenterDot] a)))], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 87} -> <|"Statement" -> 
        HoldForm[(a \[CenterDot] b) \[CenterDot] a == a \[CenterDot] 
           (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 68}, 
         "Construct" -> {"SubstitutionLemma", 74}, "Position" -> {}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
           a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[(a \[CenterDot] b) \[CenterDot] a == 
            a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
               a))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 88} -> 
      <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] a == 
          a \[CenterDot] (a \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
              b)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 87}, 
         "Construct" -> {"SubstitutionLemma", 74}, "Position" -> {2, 2}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
           a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[(a \[CenterDot] b) \[CenterDot] a == 
            a \[CenterDot] (a \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
                b)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 89} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] b) == 
          a \[CenterDot] (a \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
              b)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 88}, 
         "Construct" -> {"SubstitutionLemma", 74}, "Position" -> {}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
           a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[a \[CenterDot] (a \[CenterDot] b) == 
            a \[CenterDot] (a \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
                b)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 90} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] b)))) == a \[CenterDot] b], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 56}, 
         "Construct" -> {"SubstitutionLemma", 74}, "Position" -> {2}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
           a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
              (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
                (a \[CenterDot] b)))) == a \[CenterDot] b], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 41} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] b)))) == a \[CenterDot] (a \[CenterDot] 
            (a \[CenterDot] b))], "Proof" -> 
        <|"Construct" -> {"SubstitutionLemma", 89}, "Orientation" -> -1, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
              ((a_) \[CenterDot] (b_)))) -> a \[CenterDot] 
            (a \[CenterDot] b), "Side" -> 1, "Subpattern" -> 
          (a_) \[CenterDot] ((a_) \[CenterDot] (b_)), "MatchingConstruct" -> 
          {"SubstitutionLemma", 90}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
             ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] (
                (a_) \[CenterDot] (b_))))) -> a \[CenterDot] b, 
         "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
     {"SubstitutionLemma", 91} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] b == a \[CenterDot] (a \[CenterDot] 
            (a \[CenterDot] b))], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 41}, "Construct" -> 
          {"SubstitutionLemma", 90}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
              (((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
                (b_))))) -> a \[CenterDot] b, "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[a \[CenterDot] b == 
            a \[CenterDot] (a \[CenterDot] (a \[CenterDot] b))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 92} -> 
      <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
           (b \[CenterDot] a)], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 86}, "Construct" -> 
          {"SubstitutionLemma", 91}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
              (b_))) -> a \[CenterDot] b, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
             (b \[CenterDot] a)], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 42} -> <|"Statement" -> 
        HoldForm[a == (a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
            (a \[CenterDot] b))], "Proof" -> 
        <|"Construct" -> {"SubstitutionLemma", 92}, "Orientation" -> -1, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
             (a_)) -> a, "Side" -> 1, "Subpattern" -> (b_) \[CenterDot] (a_), 
         "MatchingConstruct" -> {"SubstitutionLemma", 74}, 
         "MatchingOrientation" -> 1, "MatchingRule" -> 
          ((a_) \[CenterDot] (b_)) \[CenterDot] (a_) -> a \[CenterDot] 
            (a \[CenterDot] b), "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"CriticalPairLemma", 43} -> <|"Statement" -> 
        HoldForm[a == (a \[CenterDot] a) \[CenterDot] (a \[CenterDot] b)], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 42}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] ((a_) \[CenterDot] (b_))) -> a, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] ((a_) \[CenterDot] (b_)), 
         "MatchingConstruct" -> {"SubstitutionLemma", 91}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] (b_))) -> 
           a \[CenterDot] b, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"CriticalPairLemma", 44} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] a == a \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] b)], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 43}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] (b_)) -> a, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (a_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 92}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((b_) \[CenterDot] (a_)) -> a, "MatchingSide" -> 1, 
         "Position" -> {1}|>|>, {"SubstitutionLemma", 94} -> 
      <|"Statement" -> HoldForm[(c \[CenterDot] b) \[CenterDot] 
           (((c \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            (c \[CenterDot] b)) == a \[CenterDot] (b \[CenterDot] 
            (b \[CenterDot] (b \[CenterDot] ((c \[CenterDot] b) \[CenterDot] (
                ((c \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
                (c \[CenterDot] b))))))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 10}, "Construct" -> 
          {"SubstitutionLemma", 74}, "Position" -> {2, 2}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
           a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[(c \[CenterDot] b) \[CenterDot] 
             (((c \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
              (c \[CenterDot] b)) == a \[CenterDot] (b \[CenterDot] 
              (b \[CenterDot] (b \[CenterDot] ((c \[CenterDot] 
                  b) \[CenterDot] (((c \[CenterDot] b) \[CenterDot] 
                   a) \[CenterDot] (c \[CenterDot] b))))))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 95} -> 
      <|"Statement" -> HoldForm[(c \[CenterDot] b) \[CenterDot] 
           (((c \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            (c \[CenterDot] b)) == a \[CenterDot] (b \[CenterDot] 
            (b \[CenterDot] (b \[CenterDot] ((c \[CenterDot] b) \[CenterDot] (
                (c \[CenterDot] b) \[CenterDot] ((c \[CenterDot] 
                  b) \[CenterDot] a))))))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 94}, "Construct" -> 
          {"SubstitutionLemma", 74}, "Position" -> {2, 2, 2, 2, 2}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
           a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[(c \[CenterDot] b) \[CenterDot] 
             (((c \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
              (c \[CenterDot] b)) == a \[CenterDot] (b \[CenterDot] 
              (b \[CenterDot] (b \[CenterDot] ((c \[CenterDot] 
                  b) \[CenterDot] ((c \[CenterDot] b) \[CenterDot] 
                  ((c \[CenterDot] b) \[CenterDot] a))))))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 96} -> 
      <|"Statement" -> HoldForm[(c \[CenterDot] b) \[CenterDot] 
           (((c \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            (c \[CenterDot] b)) == a \[CenterDot] (b \[CenterDot] 
            ((c \[CenterDot] b) \[CenterDot] ((c \[CenterDot] b) \[CenterDot] 
              ((c \[CenterDot] b) \[CenterDot] a))))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 95}, 
         "Construct" -> {"SubstitutionLemma", 91}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
              (b_))) -> a \[CenterDot] b, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[(c \[CenterDot] b) \[CenterDot] 
             (((c \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
              (c \[CenterDot] b)) == a \[CenterDot] (b \[CenterDot] 
              ((c \[CenterDot] b) \[CenterDot] ((c \[CenterDot] 
                 b) \[CenterDot] ((c \[CenterDot] b) \[CenterDot] a))))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 97} -> 
      <|"Statement" -> HoldForm[(c \[CenterDot] b) \[CenterDot] 
           (((c \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            (c \[CenterDot] b)) == a \[CenterDot] (b \[CenterDot] 
            ((c \[CenterDot] b) \[CenterDot] a))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 96}, 
         "Construct" -> {"SubstitutionLemma", 91}, "Position" -> {2, 2}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
              (b_))) -> a \[CenterDot] b, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[(c \[CenterDot] b) \[CenterDot] 
             (((c \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
              (c \[CenterDot] b)) == a \[CenterDot] (b \[CenterDot] 
              ((c \[CenterDot] b) \[CenterDot] a))], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 98} -> <|"Statement" -> 
        HoldForm[(c \[CenterDot] b) \[CenterDot] 
           ((c \[CenterDot] b) \[CenterDot] ((c \[CenterDot] b) \[CenterDot] 
             a)) == a \[CenterDot] (b \[CenterDot] 
            ((c \[CenterDot] b) \[CenterDot] a))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 97}, 
         "Construct" -> {"SubstitutionLemma", 74}, "Position" -> {2}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
           a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[(c \[CenterDot] b) \[CenterDot] 
             ((c \[CenterDot] b) \[CenterDot] ((c \[CenterDot] 
                b) \[CenterDot] a)) == a \[CenterDot] (b \[CenterDot] 
              ((c \[CenterDot] b) \[CenterDot] a))], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 99} -> <|"Statement" -> 
        HoldForm[(c \[CenterDot] b) \[CenterDot] a == a \[CenterDot] 
           (b \[CenterDot] ((c \[CenterDot] b) \[CenterDot] a))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 98}, 
         "Construct" -> {"SubstitutionLemma", 91}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
              (b_))) -> a \[CenterDot] b, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[(c \[CenterDot] b) \[CenterDot] a == 
            a \[CenterDot] (b \[CenterDot] ((c \[CenterDot] b) \[CenterDot] 
               a))], "Source" -> "norm"|>|>, {"CriticalPairLemma", 45} -> 
      <|"Statement" -> HoldForm[(b \[CenterDot] b) \[CenterDot] a == 
          a \[CenterDot] (b \[CenterDot] b)], "Proof" -> 
        <|"Construct" -> {"SubstitutionLemma", 99}, "Orientation" -> -1, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             (((c_) \[CenterDot] (b_)) \[CenterDot] (a_))) -> 
           (c \[CenterDot] b) \[CenterDot] a, "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] (((c_) \[CenterDot] 
             (b_)) \[CenterDot] (a_)), "MatchingConstruct" -> 
          {"CriticalPairLemma", 44}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> (a_) \[CenterDot] (((a_) \[CenterDot] 
              (a_)) \[CenterDot] (b_)) -> a \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"SubstitutionLemma", 102} -> 
      <|"Statement" -> HoldForm[a == (a \[CenterDot] (a \[CenterDot] 
             b)) \[CenterDot] (a \[CenterDot] a)], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 60}, 
         "Construct" -> {"SubstitutionLemma", 74}, "Position" -> {1}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
           a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a == (a \[CenterDot] 
              (a \[CenterDot] b)) \[CenterDot] (a \[CenterDot] a)], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 46} -> 
      <|"Statement" -> HoldForm[a == (a \[CenterDot] b) \[CenterDot] 
           (a \[CenterDot] a)], "Proof" -> 
        <|"Construct" -> {"SubstitutionLemma", 102}, "Orientation" -> -1, 
         "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] (b_))) \[CenterDot] 
            ((a_) \[CenterDot] (a_)) -> a, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] ((a_) \[CenterDot] (b_)), 
         "MatchingConstruct" -> {"SubstitutionLemma", 91}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] (b_))) -> 
           a \[CenterDot] b, "MatchingSide" -> 1, "Position" -> {1}|>|>, 
     {"SubstitutionLemma", 107} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] ((a \[CenterDot] 
             b) \[CenterDot] a) == (a \[CenterDot] (a \[CenterDot] 
             b)) \[CenterDot] a], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 70}, "Construct" -> 
          {"SubstitutionLemma", 74}, "Position" -> {1}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
           a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] a) == 
            (a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] a], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 108} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] ((a \[CenterDot] 
             b) \[CenterDot] a) == a \[CenterDot] (a \[CenterDot] 
            (a \[CenterDot] b))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 107}, "Construct" -> 
          {"SubstitutionLemma", 74}, "Position" -> {}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
           a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] a) == a \[CenterDot] 
             (a \[CenterDot] (a \[CenterDot] b))], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 1} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] b == 
          ((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] b)) \[CenterDot] 
            (b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
              b))) \[CenterDot] a], "Proof" -> 
        <|"Input" -> {"Hypothesis", 1}, "Construct" -> {"CriticalPairLemma", 
           4}, "Position" -> {1}, "Rule" -> 
          a_ -> (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] a)), "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[a \[CenterDot] b == ((b \[CenterDot] ((b \[CenterDot] 
                 b) \[CenterDot] b)) \[CenterDot] (b \[CenterDot] (
                (b \[CenterDot] b) \[CenterDot] b))) \[CenterDot] a], 
         "Source" -> "cpl"|>|>, {"SubstitutionLemma", 2} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] b == 
          ((((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                 b) \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
            (((((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                  b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                   b) \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
              (((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                  b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                   b) \[CenterDot] b))) \[CenterDot] a)) \[CenterDot] 
             (((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                 b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                  b) \[CenterDot] b))) \[CenterDot] a))) \[CenterDot] 
           ((((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                 b) \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
            (((((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                  b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                   b) \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
              (((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                  b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                   b) \[CenterDot] b))) \[CenterDot] a)) \[CenterDot] 
             (((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                 b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                  b) \[CenterDot] b))) \[CenterDot] a)))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 1}, 
         "Construct" -> {"CriticalPairLemma", 4}, "Position" -> {}, 
         "Rule" -> a_ -> (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] a)), "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[a \[CenterDot] b == ((((b \[CenterDot] ((b \[CenterDot] 
                   b) \[CenterDot] b)) \[CenterDot] (b \[CenterDot] 
                 ((b \[CenterDot] b) \[CenterDot] b))) \[CenterDot] 
               a) \[CenterDot] (((((b \[CenterDot] ((b \[CenterDot] 
                     b) \[CenterDot] b)) \[CenterDot] (b \[CenterDot] 
                   ((b \[CenterDot] b) \[CenterDot] b))) \[CenterDot] 
                 a) \[CenterDot] (((b \[CenterDot] ((b \[CenterDot] 
                     b) \[CenterDot] b)) \[CenterDot] (b \[CenterDot] 
                   ((b \[CenterDot] b) \[CenterDot] b))) \[CenterDot] 
                 a)) \[CenterDot] (((b \[CenterDot] ((b \[CenterDot] 
                    b) \[CenterDot] b)) \[CenterDot] (b \[CenterDot] 
                  ((b \[CenterDot] b) \[CenterDot] b))) \[CenterDot] 
                a))) \[CenterDot] ((((b \[CenterDot] ((b \[CenterDot] 
                   b) \[CenterDot] b)) \[CenterDot] (b \[CenterDot] 
                 ((b \[CenterDot] b) \[CenterDot] b))) \[CenterDot] 
               a) \[CenterDot] (((((b \[CenterDot] ((b \[CenterDot] 
                     b) \[CenterDot] b)) \[CenterDot] (b \[CenterDot] 
                   ((b \[CenterDot] b) \[CenterDot] b))) \[CenterDot] 
                 a) \[CenterDot] (((b \[CenterDot] ((b \[CenterDot] 
                     b) \[CenterDot] b)) \[CenterDot] (b \[CenterDot] 
                   ((b \[CenterDot] b) \[CenterDot] b))) \[CenterDot] 
                 a)) \[CenterDot] (((b \[CenterDot] ((b \[CenterDot] 
                    b) \[CenterDot] b)) \[CenterDot] (b \[CenterDot] 
                  ((b \[CenterDot] b) \[CenterDot] b))) \[CenterDot] a)))], 
         "Source" -> "cpl"|>|>, {"SubstitutionLemma", 3} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] b == 
          ((((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                 b) \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
            (((((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                  b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                   b) \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
              (b \[CenterDot] a)) \[CenterDot] (((b \[CenterDot] 
                ((b \[CenterDot] b) \[CenterDot] b)) \[CenterDot] (
                b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                 b))) \[CenterDot] a))) \[CenterDot] 
           ((((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                 b) \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
            (((((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                  b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                   b) \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
              (((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                  b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                   b) \[CenterDot] b))) \[CenterDot] a)) \[CenterDot] 
             (((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                 b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                  b) \[CenterDot] b))) \[CenterDot] a)))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 2}, 
         "Construct" -> {"CriticalPairLemma", 4}, "Position" -> {1, 2, 1, 2, 
          1}, "Rule" -> ((a_) \[CenterDot] (((a_) \[CenterDot] (
                a_)) \[CenterDot] (a_))) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (a_))) -> a, 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[a \[CenterDot] b == 
            ((((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                  b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                   b) \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
              (((((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                    b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                     b) \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
                (b \[CenterDot] a)) \[CenterDot] (((b \[CenterDot] 
                  ((b \[CenterDot] b) \[CenterDot] b)) \[CenterDot] 
                 (b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                   b))) \[CenterDot] a))) \[CenterDot] 
             ((((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                  b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                   b) \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
              (((((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                    b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                     b) \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
                (((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                    b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                     b) \[CenterDot] b))) \[CenterDot] a)) \[CenterDot] (
                ((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                   b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                    b) \[CenterDot] b))) \[CenterDot] a)))], 
         "Source" -> "cpl"|>|>, {"SubstitutionLemma", 93} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] b == 
          ((((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                 b) \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
            (((((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                  b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                   b) \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
              (b \[CenterDot] a)) \[CenterDot] (((b \[CenterDot] 
                ((b \[CenterDot] b) \[CenterDot] b)) \[CenterDot] (
                b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                 b))) \[CenterDot] a))) \[CenterDot] 
           ((((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                 b) \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
            (((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                 b) \[CenterDot] b))) \[CenterDot] a))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 3}, 
         "Construct" -> {"CriticalPairLemma", 44}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (b_)) -> a \[CenterDot] a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] b == 
            ((((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                  b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                   b) \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
              (((((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                    b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                     b) \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
                (b \[CenterDot] a)) \[CenterDot] (((b \[CenterDot] 
                  ((b \[CenterDot] b) \[CenterDot] b)) \[CenterDot] 
                 (b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                   b))) \[CenterDot] a))) \[CenterDot] 
             ((((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                  b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                   b) \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
              (((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                  b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                   b) \[CenterDot] b))) \[CenterDot] a))], 
         "Source" -> "cpl"|>|>, {"SubstitutionLemma", 100} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] b == 
          ((((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                 b) \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
            (((((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                  b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                   b) \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
              (b \[CenterDot] a)) \[CenterDot] (((b \[CenterDot] 
                ((b \[CenterDot] b) \[CenterDot] b)) \[CenterDot] (
                b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                 b))) \[CenterDot] a))) \[CenterDot] 
           ((((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                 b) \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                 b) \[CenterDot] b)))))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 93}, "Construct" -> 
          {"CriticalPairLemma", 45}, "Position" -> {2, 2}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (b_) -> 
           b \[CenterDot] (a \[CenterDot] a), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] b == 
            ((((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                  b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                   b) \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
              (((((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                    b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                     b) \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
                (b \[CenterDot] a)) \[CenterDot] (((b \[CenterDot] 
                  ((b \[CenterDot] b) \[CenterDot] b)) \[CenterDot] 
                 (b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                   b))) \[CenterDot] a))) \[CenterDot] 
             ((((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                  b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                   b) \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] 
                   b) \[CenterDot] b)) \[CenterDot] (b \[CenterDot] 
                 ((b \[CenterDot] b) \[CenterDot] b)))))], 
         "Source" -> "cpl"|>|>, {"SubstitutionLemma", 101} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] b == 
          ((((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                 b) \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
            (((((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                  b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                   b) \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
              (b \[CenterDot] a)) \[CenterDot] (((b \[CenterDot] 
                ((b \[CenterDot] b) \[CenterDot] b)) \[CenterDot] (
                b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                 b))) \[CenterDot] a))) \[CenterDot] 
           ((((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                 b) \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                b)) \[CenterDot] (b \[CenterDot] b))))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 100}, 
         "Construct" -> {"CriticalPairLemma", 44}, "Position" -> {2, 2, 2, 
          2}, "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] 
              (a_)) \[CenterDot] (b_)) -> a \[CenterDot] a, 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[a \[CenterDot] b == 
            ((((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                  b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                   b) \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
              (((((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                    b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                     b) \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
                (b \[CenterDot] a)) \[CenterDot] (((b \[CenterDot] 
                  ((b \[CenterDot] b) \[CenterDot] b)) \[CenterDot] 
                 (b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                   b))) \[CenterDot] a))) \[CenterDot] 
             ((((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                  b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                   b) \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] 
                   b) \[CenterDot] b)) \[CenterDot] (b \[CenterDot] b))))], 
         "Source" -> "cpl"|>|>, {"SubstitutionLemma", 103} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] b == 
          ((((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                 b) \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
            (((((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                  b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                   b) \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
              (b \[CenterDot] a)) \[CenterDot] (((b \[CenterDot] 
                ((b \[CenterDot] b) \[CenterDot] b)) \[CenterDot] (
                b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                 b))) \[CenterDot] a))) \[CenterDot] 
           ((((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                 b) \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] b))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 101}, "Construct" -> 
          {"CriticalPairLemma", 46}, "Position" -> {2, 2, 2}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
             (a_)) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[a \[CenterDot] b == ((((b \[CenterDot] ((b \[CenterDot] 
                   b) \[CenterDot] b)) \[CenterDot] (b \[CenterDot] 
                 ((b \[CenterDot] b) \[CenterDot] b))) \[CenterDot] 
               a) \[CenterDot] (((((b \[CenterDot] ((b \[CenterDot] 
                     b) \[CenterDot] b)) \[CenterDot] (b \[CenterDot] 
                   ((b \[CenterDot] b) \[CenterDot] b))) \[CenterDot] 
                 a) \[CenterDot] (b \[CenterDot] a)) \[CenterDot] (
                ((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                   b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                    b) \[CenterDot] b))) \[CenterDot] a))) \[CenterDot] 
             ((((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                  b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                   b) \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] b))], "Source" -> "cpl"|>|>, 
     {"SubstitutionLemma", 104} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] b == 
          ((((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                 b) \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
            (((((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                  b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                   b) \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
              (b \[CenterDot] a)) \[CenterDot] (((b \[CenterDot] 
                ((b \[CenterDot] b) \[CenterDot] b)) \[CenterDot] (
                b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                 b))) \[CenterDot] a))) \[CenterDot] 
           ((a \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                 b) \[CenterDot] b)))) \[CenterDot] (a \[CenterDot] b))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 103}, 
         "Construct" -> {"CriticalPairLemma", 45}, "Position" -> {2, 1}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (b_) -> 
           b \[CenterDot] (a \[CenterDot] a), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] b == 
            ((((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                  b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                   b) \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
              (((((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                    b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                     b) \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
                (b \[CenterDot] a)) \[CenterDot] (((b \[CenterDot] 
                  ((b \[CenterDot] b) \[CenterDot] b)) \[CenterDot] 
                 (b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                   b))) \[CenterDot] a))) \[CenterDot] 
             ((a \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] 
                   b) \[CenterDot] b)) \[CenterDot] (b \[CenterDot] 
                 ((b \[CenterDot] b) \[CenterDot] b)))) \[CenterDot] 
              (a \[CenterDot] b))], "Source" -> "cpl"|>|>, 
     {"SubstitutionLemma", 105} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] b == 
          ((((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                 b) \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
            (((((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                  b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                   b) \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
              (b \[CenterDot] a)) \[CenterDot] (((b \[CenterDot] 
                ((b \[CenterDot] b) \[CenterDot] b)) \[CenterDot] (
                b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                 b))) \[CenterDot] a))) \[CenterDot] 
           ((a \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                b)) \[CenterDot] (b \[CenterDot] b))) \[CenterDot] 
            (a \[CenterDot] b))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 104}, "Construct" -> 
          {"CriticalPairLemma", 44}, "Position" -> {2, 1, 2, 2}, 
         "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (b_)) -> a \[CenterDot] a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] b == 
            ((((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                  b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                   b) \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
              (((((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                    b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                     b) \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
                (b \[CenterDot] a)) \[CenterDot] (((b \[CenterDot] 
                  ((b \[CenterDot] b) \[CenterDot] b)) \[CenterDot] 
                 (b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                   b))) \[CenterDot] a))) \[CenterDot] 
             ((a \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] 
                   b) \[CenterDot] b)) \[CenterDot] (b \[CenterDot] 
                 b))) \[CenterDot] (a \[CenterDot] b))], 
         "Source" -> "cpl"|>|>, {"SubstitutionLemma", 106} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] b == 
          ((((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                 b) \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
            (((((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                  b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                   b) \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
              (b \[CenterDot] a)) \[CenterDot] (((b \[CenterDot] 
                ((b \[CenterDot] b) \[CenterDot] b)) \[CenterDot] (
                b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                 b))) \[CenterDot] a))) \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 105}, 
         "Construct" -> {"CriticalPairLemma", 46}, "Position" -> {2, 1, 2}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
             (a_)) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[a \[CenterDot] b == ((((b \[CenterDot] ((b \[CenterDot] 
                   b) \[CenterDot] b)) \[CenterDot] (b \[CenterDot] 
                 ((b \[CenterDot] b) \[CenterDot] b))) \[CenterDot] 
               a) \[CenterDot] (((((b \[CenterDot] ((b \[CenterDot] 
                     b) \[CenterDot] b)) \[CenterDot] (b \[CenterDot] 
                   ((b \[CenterDot] b) \[CenterDot] b))) \[CenterDot] 
                 a) \[CenterDot] (b \[CenterDot] a)) \[CenterDot] (
                ((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                   b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                    b) \[CenterDot] b))) \[CenterDot] a))) \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))], 
         "Source" -> "cpl"|>|>, {"SubstitutionLemma", 109} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] b == 
          ((((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                 b) \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
            ((((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                 b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                  b) \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
             ((((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                  b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                   b) \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
              (b \[CenterDot] a)))) \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 106}, 
         "Construct" -> {"SubstitutionLemma", 108}, "Position" -> {1}, 
         "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (a_)) -> a \[CenterDot] (a \[CenterDot] (a \[CenterDot] b)), 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[a \[CenterDot] b == 
            ((((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                  b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                   b) \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
              ((((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                   b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                    b) \[CenterDot] b))) \[CenterDot] a) \[CenterDot] (
                (((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                    b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                     b) \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
                (b \[CenterDot] a)))) \[CenterDot] ((a \[CenterDot] 
               b) \[CenterDot] (a \[CenterDot] b))], "Source" -> "cpl"|>|>, 
     {"SubstitutionLemma", 110} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] b == 
          ((((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                 b) \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
            (b \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 109}, "Construct" -> 
          {"SubstitutionLemma", 91}, "Position" -> {1}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
              (b_))) -> a \[CenterDot] b, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] b == 
            ((((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                  b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                   b) \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
              (b \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               b) \[CenterDot] (a \[CenterDot] b))], "Source" -> "cpl"|>|>, 
     {"SubstitutionLemma", 111} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] b == 
          ((a \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                 b) \[CenterDot] b)))) \[CenterDot] (b \[CenterDot] 
             a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 110}, "Construct" -> 
          {"CriticalPairLemma", 45}, "Position" -> {1, 1}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (b_) -> 
           b \[CenterDot] (a \[CenterDot] a), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] b == 
            ((a \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] 
                   b) \[CenterDot] b)) \[CenterDot] (b \[CenterDot] 
                 ((b \[CenterDot] b) \[CenterDot] b)))) \[CenterDot] 
              (b \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               b) \[CenterDot] (a \[CenterDot] b))], "Source" -> "cpl"|>|>, 
     {"SubstitutionLemma", 112} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] b == 
          ((a \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                b)) \[CenterDot] (b \[CenterDot] b))) \[CenterDot] 
            (b \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 111}, "Construct" -> 
          {"CriticalPairLemma", 44}, "Position" -> {1, 1, 2, 2}, 
         "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (b_)) -> a \[CenterDot] a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] b == 
            ((a \[CenterDot] ((b \[CenterDot] ((b \[CenterDot] 
                   b) \[CenterDot] b)) \[CenterDot] (b \[CenterDot] 
                 b))) \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))], 
         "Source" -> "cpl"|>|>, {"SubstitutionLemma", 113} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] b == 
          ((a \[CenterDot] b) \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 112}, 
         "Construct" -> {"CriticalPairLemma", 46}, "Position" -> {1, 1, 2}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
             (a_)) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[a \[CenterDot] b == ((a \[CenterDot] b) \[CenterDot] 
              (b \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               b) \[CenterDot] (a \[CenterDot] b))], "Source" -> "cpl"|>|>, 
     {"Conclusion", 1} -> <|"Statement" -> HoldForm[a \[CenterDot] b == 
          a \[CenterDot] b], "Proof" -> <|"Input" -> {"SubstitutionLemma", 
           113}, "Construct" -> {"CriticalPairLemma", 46}, "Position" -> {}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
             (a_)) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[a \[CenterDot] b == a \[CenterDot] b], 
         "Source" -> "cpl"|>|>}|>]}
