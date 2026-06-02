ProofObject["EquationalLogic", Inactive[Equal][
  a \[CenterDot] (a \[CenterDot] b), a \[CenterDot] (b \[CenterDot] b)], 
 {Inactive[Equal][((a_) \[CenterDot] (a_)) \[CenterDot] 
    ((a_) \[CenterDot] (a_)), a_], Inactive[Equal][
   (a_) \[CenterDot] ((b_) \[CenterDot] ((b_) \[CenterDot] (b_))), 
   (a_) \[CenterDot] (a_)], Inactive[Equal][
   ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) \[CenterDot] 
    ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))), 
   (((b_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
    (((c_) \[CenterDot] (c_)) \[CenterDot] (a_))]}, 
 <|"Variables" -> {a, b, c}, "Constants" -> {}, 
  "Proof" -> {{"Axiom", 1} -> <|"Statement" -> 
       HoldForm[(a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a) == a], 
      "Proof" -> <||>|>, {"Axiom", 2} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
           (b \[CenterDot] b)) == a \[CenterDot] a], "Proof" -> <||>|>, 
    {"Axiom", 3} -> <|"Statement" -> HoldForm[
        (a \[CenterDot] (b \[CenterDot] c)) \[CenterDot] 
          (a \[CenterDot] (b \[CenterDot] c)) == 
         ((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
          ((c \[CenterDot] c) \[CenterDot] a)], "Proof" -> <||>|>, 
    {"Hypothesis", 1} -> <|"Statement" -> 
       HoldForm[a \[CenterDot] (a \[CenterDot] b) == a \[CenterDot] 
          (b \[CenterDot] b)], "Proof" -> <||>|>, {"CriticalPairLemma", 1} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] a == a \[CenterDot] 
          ((b \[CenterDot] b) \[CenterDot] b)], 
      "Proof" -> <|"Construct" -> {"Axiom", 2}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] ((b_) \[CenterDot] 
             (b_))) -> a \[CenterDot] a, "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"Axiom", 1}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] (a_)) -> a, 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] b) == 
         a \[CenterDot] (((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
             b)) \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
            (b \[CenterDot] b)))], "Proof" -> <|"Input" -> {"Hypothesis", 1}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {2}, 
        "Rule" -> a_ -> (a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CenterDot] (a \[CenterDot] b) == a \[CenterDot] 
            (((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
               b)) \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
              (b \[CenterDot] b)))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] b) == 
         ((a \[CenterDot] (((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
               b)) \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
              (b \[CenterDot] b)))) \[CenterDot] (a \[CenterDot] 
            (((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
               b)) \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
              (b \[CenterDot] b))))) \[CenterDot] 
          ((a \[CenterDot] (((b \[CenterDot] b) \[CenterDot] 
              (b \[CenterDot] b)) \[CenterDot] ((b \[CenterDot] 
               b) \[CenterDot] (b \[CenterDot] b)))) \[CenterDot] 
           (a \[CenterDot] (((b \[CenterDot] b) \[CenterDot] 
              (b \[CenterDot] b)) \[CenterDot] ((b \[CenterDot] 
               b) \[CenterDot] (b \[CenterDot] b)))))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 1}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> a_ -> (a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CenterDot] (a \[CenterDot] b) == 
           ((a \[CenterDot] (((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
                 b)) \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                (b \[CenterDot] b)))) \[CenterDot] (a \[CenterDot] 
              (((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
                 b)) \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                (b \[CenterDot] b))))) \[CenterDot] 
            ((a \[CenterDot] (((b \[CenterDot] b) \[CenterDot] 
                (b \[CenterDot] b)) \[CenterDot] ((b \[CenterDot] 
                 b) \[CenterDot] (b \[CenterDot] b)))) \[CenterDot] 
             (a \[CenterDot] (((b \[CenterDot] b) \[CenterDot] 
                (b \[CenterDot] b)) \[CenterDot] ((b \[CenterDot] 
                 b) \[CenterDot] (b \[CenterDot] b)))))], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 3} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] b) == 
         ((a \[CenterDot] (((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
               b)) \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
              (b \[CenterDot] b)))) \[CenterDot] (a \[CenterDot] 
            (((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
               b)) \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
              (b \[CenterDot] b))))) \[CenterDot] 
          ((a \[CenterDot] (((b \[CenterDot] b) \[CenterDot] 
              (b \[CenterDot] b)) \[CenterDot] ((b \[CenterDot] 
               b) \[CenterDot] (b \[CenterDot] b)))) \[CenterDot] 
           (a \[CenterDot] (((b \[CenterDot] b) \[CenterDot] 
              (b \[CenterDot] b)) \[CenterDot] b)))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 2}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {2, 2, 2, 2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[a \[CenterDot] (a \[CenterDot] b) == 
           ((a \[CenterDot] (((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
                 b)) \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                (b \[CenterDot] b)))) \[CenterDot] (a \[CenterDot] 
              (((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
                 b)) \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                (b \[CenterDot] b))))) \[CenterDot] 
            ((a \[CenterDot] (((b \[CenterDot] b) \[CenterDot] 
                (b \[CenterDot] b)) \[CenterDot] ((b \[CenterDot] 
                 b) \[CenterDot] (b \[CenterDot] b)))) \[CenterDot] 
             (a \[CenterDot] (((b \[CenterDot] b) \[CenterDot] 
                (b \[CenterDot] b)) \[CenterDot] b)))], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 4} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] b) == 
         ((a \[CenterDot] (((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
               b)) \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
              (b \[CenterDot] b)))) \[CenterDot] (a \[CenterDot] 
            (((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
               b)) \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
              (b \[CenterDot] b))))) \[CenterDot] 
          ((a \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
           (a \[CenterDot] (((b \[CenterDot] b) \[CenterDot] 
              (b \[CenterDot] b)) \[CenterDot] b)))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 3}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {2, 1, 2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[a \[CenterDot] (a \[CenterDot] b) == 
           ((a \[CenterDot] (((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
                 b)) \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                (b \[CenterDot] b)))) \[CenterDot] (a \[CenterDot] 
              (((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
                 b)) \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                (b \[CenterDot] b))))) \[CenterDot] 
            ((a \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
             (a \[CenterDot] (((b \[CenterDot] b) \[CenterDot] 
                (b \[CenterDot] b)) \[CenterDot] b)))], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 5} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] b) == 
         ((a \[CenterDot] (((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
               b)) \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
              (b \[CenterDot] b)))) \[CenterDot] (a \[CenterDot] 
            (((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
               b)) \[CenterDot] b))) \[CenterDot] 
          ((a \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
           (a \[CenterDot] (((b \[CenterDot] b) \[CenterDot] 
              (b \[CenterDot] b)) \[CenterDot] b)))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 4}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {1, 2, 2, 2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[a \[CenterDot] (a \[CenterDot] b) == 
           ((a \[CenterDot] (((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
                 b)) \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                (b \[CenterDot] b)))) \[CenterDot] (a \[CenterDot] 
              (((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
                 b)) \[CenterDot] b))) \[CenterDot] 
            ((a \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
             (a \[CenterDot] (((b \[CenterDot] b) \[CenterDot] 
                (b \[CenterDot] b)) \[CenterDot] b)))], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 6} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] b) == 
         ((a \[CenterDot] (((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
               b)) \[CenterDot] b)) \[CenterDot] (a \[CenterDot] 
            (((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
               b)) \[CenterDot] b))) \[CenterDot] 
          ((a \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
           (a \[CenterDot] (((b \[CenterDot] b) \[CenterDot] 
              (b \[CenterDot] b)) \[CenterDot] b)))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 5}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {1, 1, 2, 2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[a \[CenterDot] (a \[CenterDot] b) == 
           ((a \[CenterDot] (((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
                 b)) \[CenterDot] b)) \[CenterDot] (a \[CenterDot] 
              (((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
                 b)) \[CenterDot] b))) \[CenterDot] 
            ((a \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
             (a \[CenterDot] (((b \[CenterDot] b) \[CenterDot] 
                (b \[CenterDot] b)) \[CenterDot] b)))], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 7} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] b) == 
         (((((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
             ((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
               b))) \[CenterDot] a) \[CenterDot] 
           ((b \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
          ((a \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
           (a \[CenterDot] (((b \[CenterDot] b) \[CenterDot] 
              (b \[CenterDot] b)) \[CenterDot] b)))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 6}, 
        "Construct" -> {"Axiom", 3}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) \[CenterDot] 
           ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) -> 
          ((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
           ((c \[CenterDot] c) \[CenterDot] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CenterDot] (a \[CenterDot] b) == 
           (((((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
                 b)) \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                (b \[CenterDot] b))) \[CenterDot] a) \[CenterDot] 
             ((b \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
            ((a \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
             (a \[CenterDot] (((b \[CenterDot] b) \[CenterDot] 
                (b \[CenterDot] b)) \[CenterDot] b)))], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 8} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] b) == 
         (((((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
             b) \[CenterDot] a) \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
            a)) \[CenterDot] ((a \[CenterDot] (b \[CenterDot] 
             b)) \[CenterDot] (a \[CenterDot] 
            (((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
               b)) \[CenterDot] b)))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 7}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {1, 1, 1, 2}, "Rule" -> 
         ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] (a_)) -> a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CenterDot] (a \[CenterDot] b) == 
           (((((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
                 b)) \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
             ((b \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
            ((a \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
             (a \[CenterDot] (((b \[CenterDot] b) \[CenterDot] 
                (b \[CenterDot] b)) \[CenterDot] b)))], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 9} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] b) == 
         (((((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
             b) \[CenterDot] a) \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
            a)) \[CenterDot] ((a \[CenterDot] (b \[CenterDot] 
             b)) \[CenterDot] (a \[CenterDot] (b \[CenterDot] b)))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 8}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {2, 2, 2, 1}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[a \[CenterDot] (a \[CenterDot] b) == 
           (((((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
                 b)) \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
             ((b \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
            ((a \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
             (a \[CenterDot] (b \[CenterDot] b)))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 10} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] b) == 
         (((((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
             b) \[CenterDot] a) \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
            a)) \[CenterDot] (((b \[CenterDot] b) \[CenterDot] 
            a) \[CenterDot] ((b \[CenterDot] b) \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 9}, 
        "Construct" -> {"Axiom", 3}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) \[CenterDot] 
           ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) -> 
          ((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
           ((c \[CenterDot] c) \[CenterDot] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CenterDot] (a \[CenterDot] b) == 
           (((((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
                 b)) \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
             ((b \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
            (((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
             ((b \[CenterDot] b) \[CenterDot] a))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 11} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] b) == 
         (((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
           ((b \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
          (((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
           ((b \[CenterDot] b) \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 10}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {1, 1, 1, 1}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[a \[CenterDot] (a \[CenterDot] b) == 
           (((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
             ((b \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
            (((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
             ((b \[CenterDot] b) \[CenterDot] a))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 12} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] b) == 
         (b \[CenterDot] b) \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 11}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[a \[CenterDot] (a \[CenterDot] b) == 
           (b \[CenterDot] b) \[CenterDot] a], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 13} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
           b) == (b \[CenterDot] b) \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 12}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {2, 1}, 
        "Rule" -> a_ -> (a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a \[CenterDot] (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
               a)) \[CenterDot] b) == (b \[CenterDot] b) \[CenterDot] a], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 14} -> 
     <|"Statement" -> HoldForm[
        ((a \[CenterDot] (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
               a)) \[CenterDot] b)) \[CenterDot] (a \[CenterDot] 
            (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
               a)) \[CenterDot] b))) \[CenterDot] 
          ((a \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] b)) \[CenterDot] 
           (a \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] b))) == 
         (b \[CenterDot] b) \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 13}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> a_ -> (a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          ((a \[CenterDot] (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] b)) \[CenterDot] (a \[CenterDot] 
              (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] b))) \[CenterDot] 
            ((a \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a)) \[CenterDot] b)) \[CenterDot] 
             (a \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a)) \[CenterDot] b))) == 
           (b \[CenterDot] b) \[CenterDot] a], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 15} -> 
     <|"Statement" -> HoldForm[
        ((a \[CenterDot] (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
               a)) \[CenterDot] b)) \[CenterDot] (a \[CenterDot] 
            (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
               a)) \[CenterDot] b))) \[CenterDot] 
          (((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
               a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] a))) \[CenterDot] a) \[CenterDot] 
           ((b \[CenterDot] b) \[CenterDot] a)) == 
         (b \[CenterDot] b) \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 14}, 
        "Construct" -> {"Axiom", 3}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) \[CenterDot] 
           ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) -> 
          ((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
           ((c \[CenterDot] c) \[CenterDot] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          ((a \[CenterDot] (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] b)) \[CenterDot] (a \[CenterDot] 
              (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] b))) \[CenterDot] 
            (((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a))) \[CenterDot] a) \[CenterDot] 
             ((b \[CenterDot] b) \[CenterDot] a)) == 
           (b \[CenterDot] b) \[CenterDot] a], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 16} -> 
     <|"Statement" -> HoldForm[
        ((a \[CenterDot] (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
               a)) \[CenterDot] b)) \[CenterDot] (a \[CenterDot] 
            (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
               a)) \[CenterDot] b))) \[CenterDot] 
          (((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
           ((b \[CenterDot] b) \[CenterDot] a)) == 
         (b \[CenterDot] b) \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 15}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {2, 1, 1}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[((a \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a)) \[CenterDot] b)) \[CenterDot] 
             (a \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a)) \[CenterDot] b))) \[CenterDot] 
            (((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
             ((b \[CenterDot] b) \[CenterDot] a)) == 
           (b \[CenterDot] b) \[CenterDot] a], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 17} -> 
     <|"Statement" -> HoldForm[
        (((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
               a))) \[CenterDot] a) \[CenterDot] 
           ((b \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
          (((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
           ((b \[CenterDot] b) \[CenterDot] a)) == 
         (b \[CenterDot] b) \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 16}, 
        "Construct" -> {"Axiom", 3}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) \[CenterDot] 
           ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) -> 
          ((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
           ((c \[CenterDot] c) \[CenterDot] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          (((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a))) \[CenterDot] a) \[CenterDot] 
             ((b \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
            (((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
             ((b \[CenterDot] b) \[CenterDot] a)) == 
           (b \[CenterDot] b) \[CenterDot] a], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 18} -> 
     <|"Statement" -> HoldForm[(((a \[CenterDot] a) \[CenterDot] 
            a) \[CenterDot] ((b \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
          (((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
           ((b \[CenterDot] b) \[CenterDot] a)) == 
         (b \[CenterDot] b) \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 17}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {1, 1, 1}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[(((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
             ((b \[CenterDot] b) \[CenterDot] a)) \[CenterDot] 
            (((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] 
             ((b \[CenterDot] b) \[CenterDot] a)) == 
           (b \[CenterDot] b) \[CenterDot] a], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 19} -> 
     <|"Statement" -> HoldForm[(((b \[CenterDot] b) \[CenterDot] 
            (b \[CenterDot] b)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
            a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] a)) == 
         (b \[CenterDot] b) \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 18}, 
        "Construct" -> {"Axiom", 3}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) \[CenterDot] 
           ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) -> 
          ((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
           ((c \[CenterDot] c) \[CenterDot] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(((b \[CenterDot] b) \[CenterDot] 
              (b \[CenterDot] b)) \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
              a) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) == 
           (b \[CenterDot] b) \[CenterDot] a], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 20} -> 
     <|"Statement" -> HoldForm[(((b \[CenterDot] b) \[CenterDot] 
            (b \[CenterDot] b)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
            a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
           (a \[CenterDot] a)) == (b \[CenterDot] b) \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 19}, 
        "Construct" -> {"CriticalPairLemma", 1}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (((b_) \[CenterDot] (b_)) \[CenterDot] 
            (b_)) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(((b \[CenterDot] b) \[CenterDot] 
              (b \[CenterDot] b)) \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
              a) \[CenterDot] (a \[CenterDot] a)) == 
           (b \[CenterDot] b) \[CenterDot] a], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 21} -> 
     <|"Statement" -> HoldForm[(((b \[CenterDot] b) \[CenterDot] 
            (b \[CenterDot] b)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
            a)) \[CenterDot] a == (b \[CenterDot] b) \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 20}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[(((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
               b)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a)) \[CenterDot] a == (b \[CenterDot] b) \[CenterDot] a], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 22} -> 
     <|"Statement" -> HoldForm[(((b \[CenterDot] b) \[CenterDot] 
            (b \[CenterDot] b)) \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
            (b \[CenterDot] b))) \[CenterDot] a == 
         (b \[CenterDot] b) \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 21}, 
        "Construct" -> {"CriticalPairLemma", 1}, "Position" -> {1}, 
        "Rule" -> (a_) \[CenterDot] (((b_) \[CenterDot] (b_)) \[CenterDot] 
            (b_)) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(((b \[CenterDot] b) \[CenterDot] 
              (b \[CenterDot] b)) \[CenterDot] ((b \[CenterDot] 
               b) \[CenterDot] (b \[CenterDot] b))) \[CenterDot] a == 
           (b \[CenterDot] b) \[CenterDot] a], "Source" -> "cpl"|>|>, 
    {"Conclusion", 1} -> <|"Statement" -> 
       HoldForm[(b \[CenterDot] b) \[CenterDot] a == 
         (b \[CenterDot] b) \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 22}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[(b \[CenterDot] b) \[CenterDot] a == 
           (b \[CenterDot] b) \[CenterDot] a], "Source" -> "cpl"|>|>}|>]
