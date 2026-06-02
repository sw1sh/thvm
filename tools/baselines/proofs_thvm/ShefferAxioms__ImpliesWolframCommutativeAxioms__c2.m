ProofObject["EquationalLogic", Inactive[Equal][a \[CenterDot] b, 
  b \[CenterDot] a], {Inactive[Equal][((a_) \[CenterDot] (a_)) \[CenterDot] 
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
    {"Hypothesis", 1} -> <|"Statement" -> HoldForm[a \[CenterDot] b == 
         b \[CenterDot] a], "Proof" -> <||>|>, {"CriticalPairLemma", 1} -> 
     <|"Statement" -> HoldForm[((b \[CenterDot] b) \[CenterDot] 
           a) \[CenterDot] (((b \[CenterDot] b) \[CenterDot] 
            (b \[CenterDot] b)) \[CenterDot] a) == 
         (a \[CenterDot] a) \[CenterDot] (a \[CenterDot] (b \[CenterDot] 
            (b \[CenterDot] b)))], "Proof" -> <|"Construct" -> {"Axiom", 3}, 
        "Orientation" -> 1, "Rule" -> 
         ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) \[CenterDot] 
           ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) -> 
          ((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
           ((c \[CenterDot] c) \[CenterDot] a), "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] ((b_) \[CenterDot] (c_)), 
        "MatchingConstruct" -> {"Axiom", 2}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            ((b_) \[CenterDot] (b_))) -> a \[CenterDot] a, 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[((b \[CenterDot] b) \[CenterDot] 
           a) \[CenterDot] (b \[CenterDot] a) == 
         (a \[CenterDot] a) \[CenterDot] (a \[CenterDot] (b \[CenterDot] 
            (b \[CenterDot] b)))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 1}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {2, 1}, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (a_)) -> a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[((b \[CenterDot] b) \[CenterDot] 
             a) \[CenterDot] (b \[CenterDot] a) == 
           (a \[CenterDot] a) \[CenterDot] (a \[CenterDot] (b \[CenterDot] 
              (b \[CenterDot] b)))], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 2} -> 
     <|"Statement" -> HoldForm[((b \[CenterDot] b) \[CenterDot] 
           a) \[CenterDot] (b \[CenterDot] a) == 
         (a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 1}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] ((b_) \[CenterDot] ((b_) \[CenterDot] 
              (b_)))) -> ((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
           (b \[CenterDot] a), "Side" -> 1, "Subpattern" -> 
         (a_) \[CenterDot] ((b_) \[CenterDot] ((b_) \[CenterDot] (b_))), 
        "MatchingConstruct" -> {"Axiom", 2}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            ((b_) \[CenterDot] (b_))) -> a \[CenterDot] a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[((b \[CenterDot] b) \[CenterDot] 
           a) \[CenterDot] (b \[CenterDot] a) == a], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 2}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            (b \[CenterDot] a) == a], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 3} -> 
     <|"Statement" -> HoldForm[(((b \[CenterDot] b) \[CenterDot] 
            (b \[CenterDot] b)) \[CenterDot] a) \[CenterDot] 
          (((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
           a) == (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
           ((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] b)))], 
      "Proof" -> <|"Construct" -> {"Axiom", 3}, "Orientation" -> 1, 
        "Rule" -> ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) \[CenterDot] 
           ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) -> 
          ((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
           ((c \[CenterDot] c) \[CenterDot] a), "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] (c_), "MatchingConstruct" -> 
         {"Axiom", 1}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] (a_)) -> a, 
        "MatchingSide" -> 1, "Position" -> {1, 2}|>|>, 
    {"SubstitutionLemma", 3} -> 
     <|"Statement" -> HoldForm[(((b \[CenterDot] b) \[CenterDot] 
            (b \[CenterDot] b)) \[CenterDot] a) \[CenterDot] 
          (((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
           a) == (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 3}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {2, 2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[(((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
               b)) \[CenterDot] a) \[CenterDot] 
            (((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
               b)) \[CenterDot] a) == (a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 4} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
          (((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
           a) == (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 3}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {1, 1}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[(b \[CenterDot] a) \[CenterDot] 
            (((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
               b)) \[CenterDot] a) == (a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 5} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
          (b \[CenterDot] a) == (a \[CenterDot] b) \[CenterDot] 
          (a \[CenterDot] b)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 4}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {2, 1}, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (a_)) -> a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
            (b \[CenterDot] a) == (a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 4} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] b == 
         (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
           (a \[CenterDot] b)) \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
           (b \[CenterDot] a))], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 2}, "Orientation" -> 1, 
        "Rule" -> (((a_) \[CenterDot] (a_)) \[CenterDot] (b_)) \[CenterDot] 
           ((a_) \[CenterDot] (b_)) -> b, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 5}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           ((a_) \[CenterDot] (b_)) -> (b \[CenterDot] a) \[CenterDot] 
           (b \[CenterDot] a), "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 5} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] a) == 
         (a \[CenterDot] a) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] a)))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 2}, 
        "Orientation" -> 1, "Rule" -> (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (b_)) \[CenterDot] ((a_) \[CenterDot] (b_)) -> b, "Side" -> 1, 
        "Subpattern" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (b_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 2}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (((a_) \[CenterDot] (a_)) \[CenterDot] (b_)) \[CenterDot] 
           ((a_) \[CenterDot] (b_)) -> b, "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 6} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] a) == 
         (a \[CenterDot] a) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
           (a \[CenterDot] a))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 5}, "Construct" -> {"Axiom", 2}, 
        "Position" -> {2}, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            ((b_) \[CenterDot] (b_))) -> a \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CenterDot] (a \[CenterDot] a) == 
           (a \[CenterDot] a) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] a))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 7} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] a) == 
         (a \[CenterDot] a) \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 6}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[a \[CenterDot] (a \[CenterDot] a) == 
           (a \[CenterDot] a) \[CenterDot] a], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 8} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] b == 
         ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b))) \[CenterDot] 
          ((b \[CenterDot] a) \[CenterDot] (b \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 4}, 
        "Construct" -> {"SubstitutionLemma", 7}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (a \[CenterDot] a), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CenterDot] b == 
           ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] b))) \[CenterDot] ((b \[CenterDot] 
              a) \[CenterDot] (b \[CenterDot] a))], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 6} -> 
     <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] b)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 2}, 
        "Orientation" -> 1, "Rule" -> (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (b_)) \[CenterDot] ((a_) \[CenterDot] (b_)) -> b, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (a_), "MatchingConstruct" -> 
         {"Axiom", 1}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] (a_)) -> a, 
        "MatchingSide" -> 1, "Position" -> {1, 1}|>|>, 
    {"CriticalPairLemma", 7} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] (b \[CenterDot] b) == 
         (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b))) \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 6}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((a_) \[CenterDot] (a_)) \[CenterDot] (b_)) -> b, "Side" -> 1, 
        "Subpattern" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (b_), 
        "MatchingConstruct" -> {"Axiom", 2}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            ((b_) \[CenterDot] (b_))) -> a \[CenterDot] a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 9} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] (b \[CenterDot] b) == 
         (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b))) \[CenterDot] 
          a], "Proof" -> <|"Input" -> {"CriticalPairLemma", 7}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[b \[CenterDot] (b \[CenterDot] b) == 
           (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b))) \[CenterDot] 
            a], "Source" -> "norm"|>|>, {"CriticalPairLemma", 8} -> 
     <|"Statement" -> HoldForm[b == (a \[CenterDot] (a \[CenterDot] 
            a)) \[CenterDot] (((b \[CenterDot] (a \[CenterDot] 
              (a \[CenterDot] a))) \[CenterDot] (b \[CenterDot] 
             (a \[CenterDot] (a \[CenterDot] a)))) \[CenterDot] b)], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 6}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((a_) \[CenterDot] (a_)) \[CenterDot] (b_)) -> b, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 9}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] ((b_) \[CenterDot] 
             ((b_) \[CenterDot] (b_)))) \[CenterDot] (a_) -> 
          b \[CenterDot] (b \[CenterDot] b), "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 10} -> 
     <|"Statement" -> HoldForm[b == (a \[CenterDot] (a \[CenterDot] 
            a)) \[CenterDot] ((((a \[CenterDot] a) \[CenterDot] 
             b) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] b)) \[CenterDot] b)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 8}, 
        "Construct" -> {"Axiom", 3}, "Position" -> {2, 1}, 
        "Rule" -> ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) \[CenterDot] 
           ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) -> 
          ((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
           ((c \[CenterDot] c) \[CenterDot] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[b == (a \[CenterDot] 
             (a \[CenterDot] a)) \[CenterDot] 
            ((((a \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
              (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] b)) \[CenterDot] b)], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 11} -> 
     <|"Statement" -> HoldForm[b == (a \[CenterDot] (a \[CenterDot] 
            a)) \[CenterDot] (b \[CenterDot] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 10}, 
        "Construct" -> {"CriticalPairLemma", 6}, "Position" -> {2, 1}, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((a_) \[CenterDot] (a_)) \[CenterDot] (b_)) -> b, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          b == (a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
            (b \[CenterDot] b)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 12} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] b == b \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 8}, 
        "Construct" -> {"SubstitutionLemma", 11}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] (a_))) \[CenterDot] 
           ((b_) \[CenterDot] (b_)) -> b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CenterDot] b == b \[CenterDot] a], 
        "Source" -> "norm"|>|>, {"Conclusion", 1} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] b == a \[CenterDot] b], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Construct" -> 
         {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[a \[CenterDot] b == 
           a \[CenterDot] b], "Source" -> "cpl"|>|>}|>]
