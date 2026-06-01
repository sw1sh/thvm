ProofObject["EquationalLogic", Inactive[Equal][
  (a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
   (b \[CenterDot] (c \[CenterDot] a)), b], 
 {Inactive[Equal][((a_) \[CenterDot] (a_)) \[CenterDot] 
    ((a_) \[CenterDot] (a_)), a_], Inactive[Equal][
   (a_) \[CenterDot] ((b_) \[CenterDot] ((b_) \[CenterDot] (b_))), 
   (a_) \[CenterDot] (a_)], Inactive[Equal][
   ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) \[CenterDot] 
    ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))), 
   (((b_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
    (((c_) \[CenterDot] (c_)) \[CenterDot] (a_))]}, 
 <|"Variables" -> {a, b, c, x3}, "Constants" -> {}, 
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
       HoldForm[(a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
            a)) \[CenterDot] (b \[CenterDot] (c \[CenterDot] a)) == b], 
      "Proof" -> <||>|>, {"CriticalPairLemma", 1} -> 
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
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 9} -> 
     <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
          (b \[CenterDot] (a \[CenterDot] a))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 6}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((a_) \[CenterDot] (a_)) \[CenterDot] (b_)) -> b, "Side" -> 1, 
        "Subpattern" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (b_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 12}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CenterDot] (b_) -> b \[CenterDot] a, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 10} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] b) == 
         a \[CenterDot] ((a \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] (b \[CenterDot] a)))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 9}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           ((b_) \[CenterDot] ((a_) \[CenterDot] (a_))) -> b, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 9}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           ((b_) \[CenterDot] ((a_) \[CenterDot] (a_))) -> b, 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 11} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] b) == 
         a \[CenterDot] (((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] (b \[CenterDot] a)))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 10}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] 
           (((a_) \[CenterDot] ((b_) \[CenterDot] (b_))) \[CenterDot] 
            (((b_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
              (a_)))) -> a \[CenterDot] (b \[CenterDot] b), "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] ((b_) \[CenterDot] (b_)), 
        "MatchingConstruct" -> {"SubstitutionLemma", 12}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CenterDot] (b_) -> b \[CenterDot] a, "MatchingSide" -> 1, 
        "Position" -> {2, 1}|>|>, {"CriticalPairLemma", 12} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] b) \[CenterDot] a == 
         a \[CenterDot] (((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] (b \[CenterDot] a)))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 9}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           ((b_) \[CenterDot] ((a_) \[CenterDot] (a_))) -> b, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 6}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((a_) \[CenterDot] (a_)) \[CenterDot] (b_)) -> b, 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"SubstitutionLemma", 14} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] b) == 
         (b \[CenterDot] b) \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 11}, 
        "Construct" -> {"CriticalPairLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] ((((b_) \[CenterDot] (b_)) \[CenterDot] 
             (a_)) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] 
             ((b_) \[CenterDot] (a_)))) -> (b \[CenterDot] b) \[CenterDot] a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CenterDot] (b \[CenterDot] b) == 
           (b \[CenterDot] b) \[CenterDot] a], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 15} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b)))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 1}, 
        "Construct" -> {"SubstitutionLemma", 2}, "Position" -> {}, 
        "Rule" -> (((a_) \[CenterDot] (a_)) \[CenterDot] (b_)) \[CenterDot] 
           ((a_) \[CenterDot] (b_)) -> b, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b)))], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 13} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
           (b \[CenterDot] b)) == (((a \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] (b \[CenterDot] b)))) \[CenterDot] a], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 2}, 
        "Orientation" -> 1, "Rule" -> (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (b_)) \[CenterDot] ((a_) \[CenterDot] (b_)) -> b, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 15}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] ((b_) \[CenterDot] ((b_) \[CenterDot] 
              (b_)))) -> a, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 16} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
           (b \[CenterDot] b)) == (a \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] (b \[CenterDot] b)))) \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 13}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {1, 1}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b)) == 
           (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] 
                b)))) \[CenterDot] a], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 14} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] (b \[CenterDot] b) == 
         ((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b)))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 2}, 
        "Orientation" -> 1, "Rule" -> (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (b_)) \[CenterDot] ((a_) \[CenterDot] (b_)) -> b, "Side" -> 1, 
        "Subpattern" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (b_), 
        "MatchingConstruct" -> {"Axiom", 2}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            ((b_) \[CenterDot] (b_))) -> a \[CenterDot] a, 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"SubstitutionLemma", 17} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] (b \[CenterDot] b) == 
         a \[CenterDot] (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] 
             b)))], "Proof" -> <|"Input" -> {"CriticalPairLemma", 14}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[b \[CenterDot] (b \[CenterDot] b) == a \[CenterDot] 
            (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b)))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 18} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
           (b \[CenterDot] b)) == (b \[CenterDot] (b \[CenterDot] 
            b)) \[CenterDot] a], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 16}, "Construct" -> 
         {"SubstitutionLemma", 17}, "Position" -> {1}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] 
             ((b_) \[CenterDot] (b_)))) -> b \[CenterDot] (b \[CenterDot] b), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b)) == 
           (b \[CenterDot] (b \[CenterDot] b)) \[CenterDot] a], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 15} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] b == 
         (a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] b], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 11}, 
        "Orientation" -> -1, "Rule" -> 
         ((a_) \[CenterDot] ((a_) \[CenterDot] (a_))) \[CenterDot] 
           ((b_) \[CenterDot] (b_)) -> b, "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"Axiom", 1}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] (a_)) -> a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 16} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
           (b \[CenterDot] b)) == a \[CenterDot] a], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 18}, 
        "Orientation" -> -1, "Rule" -> 
         ((a_) \[CenterDot] ((a_) \[CenterDot] (a_))) \[CenterDot] (b_) -> 
          b \[CenterDot] (a \[CenterDot] (a \[CenterDot] a)), "Side" -> 1, 
        "Subpattern" -> {}, "MatchingConstruct" -> {"CriticalPairLemma", 15}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] ((a_) \[CenterDot] (a_))) \[CenterDot] (b_) -> 
          b \[CenterDot] b, "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"CriticalPairLemma", 17} -> 
     <|"Statement" -> HoldForm[c \[CenterDot] (a \[CenterDot] a) == 
         (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b))) \[CenterDot] 
          c], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 14}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
           (b_) -> b \[CenterDot] (a \[CenterDot] a), "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (a_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 16}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CenterDot] (a_) -> a \[CenterDot] 
           (b \[CenterDot] (b \[CenterDot] b)), "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 18} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] b) \[CenterDot] 
          ((b \[CenterDot] b) \[CenterDot] a)], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 6}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((a_) \[CenterDot] (a_)) \[CenterDot] (b_)) -> b, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 12}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 19} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] (b \[CenterDot] 
            c)) \[CenterDot] (((c \[CenterDot] b) \[CenterDot] 
            (c \[CenterDot] b)) \[CenterDot] a)], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 18}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((b_) \[CenterDot] (b_)) \[CenterDot] (a_)) -> a, "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 5}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           ((a_) \[CenterDot] (b_)) -> (b \[CenterDot] a) \[CenterDot] 
           (b \[CenterDot] a), "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"CriticalPairLemma", 20} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] (b \[CenterDot] 
            c)) \[CenterDot] (((b \[CenterDot] c) \[CenterDot] 
            (c \[CenterDot] b)) \[CenterDot] a)], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 19}, 
        "Orientation" -> -1, "Rule" -> 
         ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) \[CenterDot] 
           ((((c_) \[CenterDot] (b_)) \[CenterDot] ((c_) \[CenterDot] 
              (b_))) \[CenterDot] (a_)) -> a, "Side" -> 1, 
        "Subpattern" -> (c_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 12}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "MatchingSide" -> 1, "Position" -> {2, 1, 1}|>|>, 
    {"CriticalPairLemma", 21} -> 
     <|"Statement" -> HoldForm[c \[CenterDot] 
          (((((a \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
               a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              (b \[CenterDot] a))) \[CenterDot] (a \[CenterDot] 
             b)) \[CenterDot] ((((a \[CenterDot] b) \[CenterDot] 
              (b \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               b) \[CenterDot] (b \[CenterDot] a))) \[CenterDot] 
            (a \[CenterDot] b))) == (((a \[CenterDot] b) \[CenterDot] 
            (b \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            (b \[CenterDot] a))) \[CenterDot] c], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 17}, 
        "Orientation" -> -1, "Rule" -> 
         ((a_) \[CenterDot] ((b_) \[CenterDot] ((b_) \[CenterDot] 
              (b_)))) \[CenterDot] (c_) -> c \[CenterDot] (a \[CenterDot] a), 
        "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
           ((b_) \[CenterDot] (b_))), "MatchingConstruct" -> 
         {"CriticalPairLemma", 20}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] ((b_) \[CenterDot] 
             (c_))) \[CenterDot] ((((b_) \[CenterDot] (c_)) \[CenterDot] 
             ((c_) \[CenterDot] (b_))) \[CenterDot] (a_)) -> a, 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 22} -> 
     <|"Statement" -> HoldForm[c == ((a \[CenterDot] b) \[CenterDot] 
           c) \[CenterDot] (((b \[CenterDot] a) \[CenterDot] 
            (b \[CenterDot] a)) \[CenterDot] c)], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 6}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((a_) \[CenterDot] (a_)) \[CenterDot] (b_)) -> b, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (a_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 5}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           ((a_) \[CenterDot] (b_)) -> (b \[CenterDot] a) \[CenterDot] 
           (b \[CenterDot] a), "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"CriticalPairLemma", 23} -> 
     <|"Statement" -> HoldForm[((b \[CenterDot] a) \[CenterDot] 
           (b \[CenterDot] a)) \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
           (b \[CenterDot] a)) == ((a \[CenterDot] b) \[CenterDot] 
           (((b \[CenterDot] a) \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
            ((b \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
              a)))) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
           (((b \[CenterDot] a) \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
            ((b \[CenterDot] a) \[CenterDot] (b \[CenterDot] a))))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 22}, 
        "Orientation" -> -1, "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            (c_)) \[CenterDot] ((((b_) \[CenterDot] (a_)) \[CenterDot] 
             ((b_) \[CenterDot] (a_))) \[CenterDot] (c_)) -> c, "Side" -> 1, 
        "Subpattern" -> {}, "MatchingConstruct" -> {"Axiom", 2}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CenterDot] ((b_) \[CenterDot] ((b_) \[CenterDot] (b_))) -> 
          a \[CenterDot] a, "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"SubstitutionLemma", 19} -> 
     <|"Statement" -> HoldForm[((b \[CenterDot] a) \[CenterDot] 
           (b \[CenterDot] a)) \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
           (b \[CenterDot] a)) == ((a \[CenterDot] b) \[CenterDot] 
           (b \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
           (((b \[CenterDot] a) \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
            ((b \[CenterDot] a) \[CenterDot] (b \[CenterDot] a))))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 23}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {1, 2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[((b \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
              a)) \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
             (b \[CenterDot] a)) == ((a \[CenterDot] b) \[CenterDot] 
             (b \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
              b) \[CenterDot] (((b \[CenterDot] a) \[CenterDot] (
                b \[CenterDot] a)) \[CenterDot] ((b \[CenterDot] 
                a) \[CenterDot] (b \[CenterDot] a))))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 20} -> 
     <|"Statement" -> HoldForm[((b \[CenterDot] a) \[CenterDot] 
           (b \[CenterDot] a)) \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
           (b \[CenterDot] a)) == ((a \[CenterDot] b) \[CenterDot] 
           (b \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
           (b \[CenterDot] a))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 19}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {2, 2}, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (a_)) -> a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[((b \[CenterDot] a) \[CenterDot] 
             (b \[CenterDot] a)) \[CenterDot] ((b \[CenterDot] 
              a) \[CenterDot] (b \[CenterDot] a)) == 
           ((a \[CenterDot] b) \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] (b \[CenterDot] a))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 21} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] a == 
         ((a \[CenterDot] b) \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
          ((a \[CenterDot] b) \[CenterDot] (b \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 20}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[b \[CenterDot] a == ((a \[CenterDot] b) \[CenterDot] 
             (b \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
              b) \[CenterDot] (b \[CenterDot] a))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 22} -> 
     <|"Statement" -> HoldForm[c \[CenterDot] 
          (((((a \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
               a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              (b \[CenterDot] a))) \[CenterDot] (a \[CenterDot] 
             b)) \[CenterDot] ((((a \[CenterDot] b) \[CenterDot] 
              (b \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               b) \[CenterDot] (b \[CenterDot] a))) \[CenterDot] 
            (a \[CenterDot] b))) == (b \[CenterDot] a) \[CenterDot] c], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 21}, 
        "Construct" -> {"SubstitutionLemma", 21}, "Position" -> {1}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
             (a_))) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((b_) \[CenterDot] (a_))) -> b \[CenterDot] a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          c \[CenterDot] (((((a \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
                 a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
                (b \[CenterDot] a))) \[CenterDot] (a \[CenterDot] 
               b)) \[CenterDot] ((((a \[CenterDot] b) \[CenterDot] 
                (b \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
                 b) \[CenterDot] (b \[CenterDot] a))) \[CenterDot] 
              (a \[CenterDot] b))) == (b \[CenterDot] a) \[CenterDot] c], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 23} -> 
     <|"Statement" -> HoldForm[c \[CenterDot] 
          (((a \[CenterDot] b) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
              (b \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               b) \[CenterDot] (b \[CenterDot] a)))) \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
              (b \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               b) \[CenterDot] (b \[CenterDot] a))))) == 
         (b \[CenterDot] a) \[CenterDot] c], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 22}, 
        "Construct" -> {"Axiom", 3}, "Position" -> {2}, 
        "Rule" -> (((b_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
           (((c_) \[CenterDot] (c_)) \[CenterDot] (a_)) -> 
          (a \[CenterDot] (b \[CenterDot] c)) \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] c)), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[c \[CenterDot] 
            (((a \[CenterDot] b) \[CenterDot] (((a \[CenterDot] 
                 b) \[CenterDot] (b \[CenterDot] a)) \[CenterDot] (
                (a \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
                 a)))) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              (((a \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
                 a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
                (b \[CenterDot] a))))) == (b \[CenterDot] a) \[CenterDot] c], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 24} -> 
     <|"Statement" -> HoldForm[c \[CenterDot] 
          (((a \[CenterDot] b) \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
              (b \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               b) \[CenterDot] (b \[CenterDot] a))))) == 
         (b \[CenterDot] a) \[CenterDot] c], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 23}, 
        "Construct" -> {"SubstitutionLemma", 21}, "Position" -> {2, 1, 2}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
             (a_))) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((b_) \[CenterDot] (a_))) -> b \[CenterDot] a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          c \[CenterDot] (((a \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
               a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              (((a \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
                 a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
                (b \[CenterDot] a))))) == (b \[CenterDot] a) \[CenterDot] c], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 25} -> 
     <|"Statement" -> HoldForm[c \[CenterDot] 
          (((a \[CenterDot] b) \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] (b \[CenterDot] a))) == 
         (b \[CenterDot] a) \[CenterDot] c], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 24}, 
        "Construct" -> {"SubstitutionLemma", 21}, "Position" -> {2, 2, 2}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
             (a_))) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((b_) \[CenterDot] (a_))) -> b \[CenterDot] a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          c \[CenterDot] (((a \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
               a)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              (b \[CenterDot] a))) == (b \[CenterDot] a) \[CenterDot] c], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 26} -> 
     <|"Statement" -> HoldForm[c \[CenterDot] (b \[CenterDot] a) == 
         (b \[CenterDot] a) \[CenterDot] c], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 25}, 
        "Construct" -> {"SubstitutionLemma", 21}, "Position" -> {2}, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
             (a_))) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((b_) \[CenterDot] (a_))) -> b \[CenterDot] a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          c \[CenterDot] (b \[CenterDot] a) == 
           (b \[CenterDot] a) \[CenterDot] c], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 24} -> 
     <|"Statement" -> HoldForm[c \[CenterDot] (b \[CenterDot] a) == 
         (a \[CenterDot] b) \[CenterDot] c], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 26}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (c_) -> c \[CenterDot] (a \[CenterDot] b), "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 12}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 25} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (c \[CenterDot] b) == 
         a \[CenterDot] (b \[CenterDot] c)], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 24}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (c_) -> c \[CenterDot] (b \[CenterDot] a), "Side" -> 1, 
        "Subpattern" -> {}, "MatchingConstruct" -> {"SubstitutionLemma", 12}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CenterDot] (b_) -> b \[CenterDot] a, "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 26} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
           (x3 \[CenterDot] c)) == a \[CenterDot] (b \[CenterDot] 
           (c \[CenterDot] x3))], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 25}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (c_)) -> 
          a \[CenterDot] (c \[CenterDot] b), "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] (c_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 24}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (c_) -> 
          c \[CenterDot] (b \[CenterDot] a), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 27} -> 
     <|"Statement" -> HoldForm[c \[CenterDot] b == 
         (a \[CenterDot] (b \[CenterDot] c)) \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] (c \[CenterDot] b))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 18}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((b_) \[CenterDot] (b_)) \[CenterDot] (a_)) -> a, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 24}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (c_) -> 
          c \[CenterDot] (b \[CenterDot] a), "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 28} -> 
     <|"Statement" -> HoldForm[
        a == (((a \[CenterDot] (b \[CenterDot] (b \[CenterDot] 
               b))) \[CenterDot] (a \[CenterDot] (b \[CenterDot] 
              (b \[CenterDot] b)))) \[CenterDot] a) \[CenterDot] 
          (b \[CenterDot] (b \[CenterDot] b))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 2}, 
        "Orientation" -> 1, "Rule" -> (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (b_)) \[CenterDot] ((a_) \[CenterDot] (b_)) -> b, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 9}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] ((b_) \[CenterDot] 
             ((b_) \[CenterDot] (b_)))) \[CenterDot] (a_) -> 
          b \[CenterDot] (b \[CenterDot] b), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 28} -> 
     <|"Statement" -> HoldForm[a == ((((b \[CenterDot] b) \[CenterDot] 
             a) \[CenterDot] (((b \[CenterDot] b) \[CenterDot] 
              (b \[CenterDot] b)) \[CenterDot] a)) \[CenterDot] 
           a) \[CenterDot] (b \[CenterDot] (b \[CenterDot] b))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 28}, 
        "Construct" -> {"Axiom", 3}, "Position" -> {1, 1}, 
        "Rule" -> ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) \[CenterDot] 
           ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) -> 
          ((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
           ((c \[CenterDot] c) \[CenterDot] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[
          a == ((((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
              (((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
                 b)) \[CenterDot] a)) \[CenterDot] a) \[CenterDot] 
            (b \[CenterDot] (b \[CenterDot] b))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 29} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          (b \[CenterDot] (b \[CenterDot] b))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 28}, 
        "Construct" -> {"CriticalPairLemma", 6}, "Position" -> {1, 1}, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((a_) \[CenterDot] (a_)) \[CenterDot] (b_)) -> b, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a == (a \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
             (b \[CenterDot] b))], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 29} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] (a \[CenterDot] c) == 
         (((a \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
           ((c \[CenterDot] c) \[CenterDot] b)) \[CenterDot] 
          (x3 \[CenterDot] (x3 \[CenterDot] x3))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 29}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
           ((b_) \[CenterDot] ((b_) \[CenterDot] (b_))) -> a, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (a_), "MatchingConstruct" -> 
         {"Axiom", 3}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) \[CenterDot] 
           ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) -> 
          ((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
           ((c \[CenterDot] c) \[CenterDot] a), "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 30} -> 
     <|"Statement" -> HoldForm[((b \[CenterDot] b) \[CenterDot] 
           (b \[CenterDot] b)) \[CenterDot] (a \[CenterDot] b) == 
         (((a \[CenterDot] a) \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
             (b \[CenterDot] b))) \[CenterDot] ((a \[CenterDot] 
             a) \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
             (b \[CenterDot] b)))) \[CenterDot] (c \[CenterDot] 
           (c \[CenterDot] c))], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 29}, "Orientation" -> -1, 
        "Rule" -> ((((a_) \[CenterDot] (a_)) \[CenterDot] (b_)) \[CenterDot] 
            (((c_) \[CenterDot] (c_)) \[CenterDot] (b_))) \[CenterDot] 
           ((x3_) \[CenterDot] ((x3_) \[CenterDot] (x3_))) -> 
          b \[CenterDot] (a \[CenterDot] c), "Side" -> 1, 
        "Subpattern" -> (((a_) \[CenterDot] (a_)) \[CenterDot] 
           (b_)) \[CenterDot] (((c_) \[CenterDot] (c_)) \[CenterDot] (b_)), 
        "MatchingConstruct" -> {"Axiom", 2}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            ((b_) \[CenterDot] (b_))) -> a \[CenterDot] a, 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"SubstitutionLemma", 30} -> 
     <|"Statement" -> HoldForm[((b \[CenterDot] b) \[CenterDot] 
           (b \[CenterDot] b)) \[CenterDot] (a \[CenterDot] b) == 
         (a \[CenterDot] a) \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
           (b \[CenterDot] b))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 30}, "Construct" -> 
         {"SubstitutionLemma", 29}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
            ((b_) \[CenterDot] (b_))) -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[((b \[CenterDot] b) \[CenterDot] 
             (b \[CenterDot] b)) \[CenterDot] (a \[CenterDot] b) == 
           (a \[CenterDot] a) \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
             (b \[CenterDot] b))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 31} -> 
     <|"Statement" -> HoldForm[((b \[CenterDot] b) \[CenterDot] 
           (b \[CenterDot] b)) \[CenterDot] (a \[CenterDot] b) == 
         (a \[CenterDot] a) \[CenterDot] b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 30}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
              b)) \[CenterDot] (a \[CenterDot] b) == 
           (a \[CenterDot] a) \[CenterDot] b], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 32} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] (a \[CenterDot] b) == 
         (a \[CenterDot] a) \[CenterDot] b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 31}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[b \[CenterDot] (a \[CenterDot] b) == 
           (a \[CenterDot] a) \[CenterDot] b], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 31} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] (b \[CenterDot] 
            a)) \[CenterDot] (((b \[CenterDot] b) \[CenterDot] 
            (b \[CenterDot] b)) \[CenterDot] a)], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 6}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((a_) \[CenterDot] (a_)) \[CenterDot] (b_)) -> b, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 32}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (b_) -> 
          b \[CenterDot] (a \[CenterDot] b), "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 33} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] (b \[CenterDot] 
            a)) \[CenterDot] (b \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 31}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {2, 1}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[a == (a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
            (b \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 34} -> 
     <|"Statement" -> HoldForm[a == (b \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] (b \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 33}, 
        "Construct" -> {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a == (b \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
             (b \[CenterDot] a))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 35} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          (b \[CenterDot] a)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 34}, "Construct" -> 
         {"SubstitutionLemma", 32}, "Position" -> {}, 
        "Rule" -> (b_) \[CenterDot] ((a_) \[CenterDot] (b_)) -> 
          (a \[CenterDot] a) \[CenterDot] b, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
            (b \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 32} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] b)], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 35}, "Orientation" -> -1, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
            (a_)) -> a, "Side" -> 1, "Subpattern" -> (b_) \[CenterDot] (a_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 12}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CenterDot] (b_) -> b \[CenterDot] a, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 33} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] b == 
         (a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] a], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 27}, 
        "Orientation" -> -1, "Rule" -> 
         ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) \[CenterDot] 
           (((a_) \[CenterDot] (a_)) \[CenterDot] ((c_) \[CenterDot] 
             (b_))) -> c \[CenterDot] b, "Side" -> 1, 
        "Subpattern" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
          ((c_) \[CenterDot] (b_)), "MatchingConstruct" -> 
         {"CriticalPairLemma", 32}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (b_)) -> a, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 36} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] b == a \[CenterDot] 
          (a \[CenterDot] (b \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 33}, 
        "Construct" -> {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[a \[CenterDot] b == 
           a \[CenterDot] (a \[CenterDot] (b \[CenterDot] a))], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 34} -> 
     <|"Statement" -> HoldForm[(((b \[CenterDot] b) \[CenterDot] 
            (b \[CenterDot] b)) \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] (b \[CenterDot] a)) == 
         a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
           ((b \[CenterDot] b) \[CenterDot] a))], 
      "Proof" -> <|"Construct" -> {"Axiom", 3}, "Orientation" -> 1, 
        "Rule" -> ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) \[CenterDot] 
           ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) -> 
          ((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
           ((c \[CenterDot] c) \[CenterDot] a), "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] ((b_) \[CenterDot] (c_)), 
        "MatchingConstruct" -> {"CriticalPairLemma", 6}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] (b_)) \[CenterDot] (((a_) \[CenterDot] 
             (a_)) \[CenterDot] (b_)) -> b, "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 39} -> 
     <|"Statement" -> HoldForm[(((b \[CenterDot] b) \[CenterDot] 
            (b \[CenterDot] b)) \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] (b \[CenterDot] a)) == 
         a \[CenterDot] a], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 34}, "Construct" -> 
         {"CriticalPairLemma", 6}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((a_) \[CenterDot] (a_)) \[CenterDot] (b_)) -> b, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
             (b \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
              a) \[CenterDot] (b \[CenterDot] a)) == a \[CenterDot] a], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 40} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] (b \[CenterDot] 
            a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
           (b \[CenterDot] a)) == a \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 39}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {1, 1}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[(b \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
            ((a \[CenterDot] a) \[CenterDot] (b \[CenterDot] a)) == 
           a \[CenterDot] a], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 35} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] b) \[CenterDot] 
          (b \[CenterDot] b) == (a \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] b))) \[CenterDot] (b \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] b)))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 40}, 
        "Orientation" -> 1, "Rule" -> 
         ((a_) \[CenterDot] ((a_) \[CenterDot] (b_))) \[CenterDot] 
           (((b_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
             (b_))) -> b \[CenterDot] b, "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"Axiom", 1}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] (a_)) -> a, 
        "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"SubstitutionLemma", 41} -> 
     <|"Statement" -> HoldForm[b == (a \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] b))) \[CenterDot] (b \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] b)))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 35}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[b == (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] 
               b))) \[CenterDot] (b \[CenterDot] (a \[CenterDot] 
              (b \[CenterDot] b)))], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 36} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] 
          ((a \[CenterDot] (a \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
           b) == a \[CenterDot] b], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 32}, "Orientation" -> -1, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (b_) -> 
          b \[CenterDot] (a \[CenterDot] b), "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (a_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 41}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] 
             ((b_) \[CenterDot] (b_)))) \[CenterDot] ((b_) \[CenterDot] 
            ((a_) \[CenterDot] ((b_) \[CenterDot] (b_)))) -> b, 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"SubstitutionLemma", 42} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] (b \[CenterDot] 
           (a \[CenterDot] a)) == a \[CenterDot] b], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 36}, 
        "Construct" -> {"CriticalPairLemma", 17}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] ((b_) \[CenterDot] ((b_) \[CenterDot] 
              (b_)))) \[CenterDot] (c_) -> c \[CenterDot] (a \[CenterDot] a), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          b \[CenterDot] (b \[CenterDot] (a \[CenterDot] a)) == 
           a \[CenterDot] b], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 37} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] b == 
         (a \[CenterDot] b) \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] b))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 27}, 
        "Orientation" -> -1, "Rule" -> 
         ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) \[CenterDot] 
           (((a_) \[CenterDot] (a_)) \[CenterDot] ((c_) \[CenterDot] 
             (b_))) -> c \[CenterDot] b, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] ((b_) \[CenterDot] (c_)), 
        "MatchingConstruct" -> {"SubstitutionLemma", 42}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] (b_))) -> 
          b \[CenterDot] a, "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"SubstitutionLemma", 43} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] b == 
         (a \[CenterDot] b) \[CenterDot] b], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 37}, 
        "Construct" -> {"SubstitutionLemma", 35}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[(a \[CenterDot] a) \[CenterDot] b == 
           (a \[CenterDot] b) \[CenterDot] b], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 44} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] b == 
         b \[CenterDot] (a \[CenterDot] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 43}, 
        "Construct" -> {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (a \[CenterDot] a) \[CenterDot] b == b \[CenterDot] 
            (a \[CenterDot] b)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 38} -> 
     <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
          (b \[CenterDot] b)], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 35}, "Orientation" -> -1, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
            (a_)) -> a, "Side" -> 1, "Subpattern" -> {}, 
        "MatchingConstruct" -> {"SubstitutionLemma", 12}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CenterDot] (b_) -> b \[CenterDot] a, "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 39} -> 
     <|"Statement" -> HoldForm[((b \[CenterDot] a) \[CenterDot] 
           (b \[CenterDot] a)) \[CenterDot] (a \[CenterDot] a) == 
         (a \[CenterDot] a) \[CenterDot] a], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 44}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            (a_)) -> (b \[CenterDot] b) \[CenterDot] a, "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] (a_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 38}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           ((b_) \[CenterDot] (b_)) -> b, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 45} -> 
     <|"Statement" -> HoldForm[((b \[CenterDot] a) \[CenterDot] 
           (b \[CenterDot] a)) \[CenterDot] (a \[CenterDot] a) == 
         a \[CenterDot] (a \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 39}, 
        "Construct" -> {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          ((b \[CenterDot] a) \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] a) == a \[CenterDot] (a \[CenterDot] a)], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 46} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
          ((b \[CenterDot] a) \[CenterDot] (b \[CenterDot] a)) == 
         a \[CenterDot] (a \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 45}, 
        "Construct" -> {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (a \[CenterDot] a) \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
             (b \[CenterDot] a)) == a \[CenterDot] (a \[CenterDot] a)], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 40} -> 
     <|"Statement" -> HoldForm[(((b \[CenterDot] b) \[CenterDot] 
            (b \[CenterDot] b)) \[CenterDot] a) \[CenterDot] 
          ((((c \[CenterDot] b) \[CenterDot] (c \[CenterDot] b)) \[CenterDot] 
            ((c \[CenterDot] b) \[CenterDot] (c \[CenterDot] 
              b))) \[CenterDot] a) == (a \[CenterDot] (b \[CenterDot] 
            (b \[CenterDot] b))) \[CenterDot] (a \[CenterDot] 
           ((b \[CenterDot] b) \[CenterDot] ((c \[CenterDot] b) \[CenterDot] 
             (c \[CenterDot] b))))], "Proof" -> 
       <|"Construct" -> {"Axiom", 3}, "Orientation" -> 1, 
        "Rule" -> ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) \[CenterDot] 
           ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) -> 
          ((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
           ((c \[CenterDot] c) \[CenterDot] a), "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] (c_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 46}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
           (((b_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
             (a_))) -> a \[CenterDot] (a \[CenterDot] a), 
        "MatchingSide" -> 1, "Position" -> {1, 2}|>|>, 
    {"CriticalPairLemma", 41} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] (b \[CenterDot] 
            (b \[CenterDot] b))) \[CenterDot] (a \[CenterDot] c)], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 32}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (b_)) -> a, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (a_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 16}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CenterDot] (a_) -> a \[CenterDot] 
           (b \[CenterDot] (b \[CenterDot] b)), "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 47} -> 
     <|"Statement" -> HoldForm[(((b \[CenterDot] b) \[CenterDot] 
            (b \[CenterDot] b)) \[CenterDot] a) \[CenterDot] 
          ((((c \[CenterDot] b) \[CenterDot] (c \[CenterDot] b)) \[CenterDot] 
            ((c \[CenterDot] b) \[CenterDot] (c \[CenterDot] 
              b))) \[CenterDot] a) == a], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 40}, "Construct" -> 
         {"CriticalPairLemma", 41}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] ((b_) \[CenterDot] ((b_) \[CenterDot] 
              (b_)))) \[CenterDot] ((a_) \[CenterDot] (c_)) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
             a) \[CenterDot] ((((c \[CenterDot] b) \[CenterDot] (
                c \[CenterDot] b)) \[CenterDot] ((c \[CenterDot] 
                b) \[CenterDot] (c \[CenterDot] b))) \[CenterDot] a) == a], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 48} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
          ((((c \[CenterDot] b) \[CenterDot] (c \[CenterDot] b)) \[CenterDot] 
            ((c \[CenterDot] b) \[CenterDot] (c \[CenterDot] 
              b))) \[CenterDot] a) == a], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 47}, "Construct" -> 
         {"SubstitutionLemma", 35}, "Position" -> {1, 1}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[(b \[CenterDot] a) \[CenterDot] 
            ((((c \[CenterDot] b) \[CenterDot] (c \[CenterDot] 
                b)) \[CenterDot] ((c \[CenterDot] b) \[CenterDot] (
                c \[CenterDot] b))) \[CenterDot] a) == a], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 49} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
          ((c \[CenterDot] b) \[CenterDot] a) == a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 48}, 
        "Construct" -> {"SubstitutionLemma", 35}, "Position" -> {2, 1}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[(b \[CenterDot] a) \[CenterDot] 
            ((c \[CenterDot] b) \[CenterDot] a) == a], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 42} -> 
     <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
          (b \[CenterDot] (c \[CenterDot] a))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 49}, 
        "Orientation" -> 1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((c_) \[CenterDot] (a_)) \[CenterDot] (b_)) -> b, "Side" -> 1, 
        "Subpattern" -> ((c_) \[CenterDot] (a_)) \[CenterDot] (b_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 12}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CenterDot] (b_) -> b \[CenterDot] a, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 13} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] (c \[CenterDot] 
            a)) \[CenterDot] (a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
            a)) == b], "Proof" -> <|"Input" -> {"Hypothesis", 1}, 
        "Construct" -> {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (b \[CenterDot] (c \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] a)) == b], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 27} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] (c \[CenterDot] 
            a)) \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] a))) == b], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 13}, "Construct" -> 
         {"CriticalPairLemma", 26}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] ((c_) \[CenterDot] 
             (x3_))) -> a \[CenterDot] (b \[CenterDot] (x3 \[CenterDot] c)), 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (b \[CenterDot] (c \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] a))) == b], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 37} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] (c \[CenterDot] 
            a)) \[CenterDot] (a \[CenterDot] b) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 27}, 
        "Construct" -> {"SubstitutionLemma", 36}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] 
             (a_))) -> a \[CenterDot] b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          (b \[CenterDot] (c \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] b) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 38} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
          (b \[CenterDot] (c \[CenterDot] a)) == b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 37}, 
        "Construct" -> {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (a \[CenterDot] b) \[CenterDot] (b \[CenterDot] (c \[CenterDot] 
              a)) == b], "Source" -> "cpl"|>|>, {"Conclusion", 1} -> 
     <|"Statement" -> HoldForm[b == b], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 38}, "Construct" -> 
         {"CriticalPairLemma", 42}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
            ((c_) \[CenterDot] (a_))) -> b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[b == b], "Source" -> "cpl"|>|>}|>]
