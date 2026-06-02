ProofObject["EquationalLogic", Inactive[Equal][
  (a \[CenterDot] a) \[CenterDot] (b \[CenterDot] a), a], 
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
       HoldForm[(a \[CenterDot] a) \[CenterDot] (b \[CenterDot] a) == a], 
      "Proof" -> <||>|>, {"CriticalPairLemma", 1} -> 
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
    {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[(((b \[CenterDot] b) \[CenterDot] 
            (b \[CenterDot] b)) \[CenterDot] a) \[CenterDot] 
          (((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
           a) == (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 1}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {2, 2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[(((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
               b)) \[CenterDot] a) \[CenterDot] 
            (((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
               b)) \[CenterDot] a) == (a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
          (((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
           a) == (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 1}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {1, 1}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[(b \[CenterDot] a) \[CenterDot] 
            (((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
               b)) \[CenterDot] a) == (a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 3} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
          (b \[CenterDot] a) == (a \[CenterDot] b) \[CenterDot] 
          (a \[CenterDot] b)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 2}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {2, 1}, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (a_)) -> a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
            (b \[CenterDot] a) == (a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 2} -> 
     <|"Statement" -> HoldForm[
        ((b \[CenterDot] (b \[CenterDot] b)) \[CenterDot] a) \[CenterDot] 
          ((b \[CenterDot] (b \[CenterDot] b)) \[CenterDot] a) == 
         (a \[CenterDot] a) \[CenterDot] (a \[CenterDot] (b \[CenterDot] 
            (b \[CenterDot] b)))], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 3}, "Orientation" -> 1, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
            (b_)) -> (b \[CenterDot] a) \[CenterDot] (b \[CenterDot] a), 
        "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] (b_), 
        "MatchingConstruct" -> {"Axiom", 2}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            ((b_) \[CenterDot] (b_))) -> a \[CenterDot] a, 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 3} -> 
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
    {"SubstitutionLemma", 4} -> 
     <|"Statement" -> HoldForm[((b \[CenterDot] b) \[CenterDot] 
           a) \[CenterDot] (b \[CenterDot] a) == 
         (a \[CenterDot] a) \[CenterDot] (a \[CenterDot] (b \[CenterDot] 
            (b \[CenterDot] b)))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 3}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {2, 1}, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (a_)) -> a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[((b \[CenterDot] b) \[CenterDot] 
             a) \[CenterDot] (b \[CenterDot] a) == 
           (a \[CenterDot] a) \[CenterDot] (a \[CenterDot] (b \[CenterDot] 
              (b \[CenterDot] b)))], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 4} -> 
     <|"Statement" -> HoldForm[((b \[CenterDot] b) \[CenterDot] 
           a) \[CenterDot] (b \[CenterDot] a) == 
         (a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 4}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] ((b_) \[CenterDot] ((b_) \[CenterDot] 
              (b_)))) -> ((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
           (b \[CenterDot] a), "Side" -> 1, "Subpattern" -> 
         (a_) \[CenterDot] ((b_) \[CenterDot] ((b_) \[CenterDot] (b_))), 
        "MatchingConstruct" -> {"Axiom", 2}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            ((b_) \[CenterDot] (b_))) -> a \[CenterDot] a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 5} -> 
     <|"Statement" -> HoldForm[((b \[CenterDot] b) \[CenterDot] 
           a) \[CenterDot] (b \[CenterDot] a) == a], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 4}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            (b \[CenterDot] a) == a], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 6} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b)))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 4}, 
        "Construct" -> {"SubstitutionLemma", 5}, "Position" -> {}, 
        "Rule" -> (((a_) \[CenterDot] (a_)) \[CenterDot] (b_)) \[CenterDot] 
           ((a_) \[CenterDot] (b_)) -> b, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b)))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 7} -> 
     <|"Statement" -> HoldForm[
        ((b \[CenterDot] (b \[CenterDot] b)) \[CenterDot] a) \[CenterDot] 
          ((b \[CenterDot] (b \[CenterDot] b)) \[CenterDot] a) == a], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 2}, 
        "Construct" -> {"SubstitutionLemma", 6}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            ((b_) \[CenterDot] ((b_) \[CenterDot] (b_)))) -> a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          ((b \[CenterDot] (b \[CenterDot] b)) \[CenterDot] a) \[CenterDot] 
            ((b \[CenterDot] (b \[CenterDot] b)) \[CenterDot] a) == a], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 5} -> 
     <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] b)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 5}, 
        "Orientation" -> 1, "Rule" -> (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (b_)) \[CenterDot] ((a_) \[CenterDot] (b_)) -> b, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (a_), "MatchingConstruct" -> 
         {"Axiom", 1}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] (a_)) -> a, 
        "MatchingSide" -> 1, "Position" -> {1, 1}|>|>, 
    {"CriticalPairLemma", 6} -> 
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
        "MatchingConstruct" -> {"CriticalPairLemma", 5}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] (b_)) \[CenterDot] (((a_) \[CenterDot] 
             (a_)) \[CenterDot] (b_)) -> b, "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 8} -> 
     <|"Statement" -> HoldForm[(((b \[CenterDot] b) \[CenterDot] 
            (b \[CenterDot] b)) \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] (b \[CenterDot] a)) == 
         a \[CenterDot] a], "Proof" -> <|"Input" -> {"CriticalPairLemma", 6}, 
        "Construct" -> {"CriticalPairLemma", 5}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((a_) \[CenterDot] (a_)) \[CenterDot] (b_)) -> b, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
             (b \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
              a) \[CenterDot] (b \[CenterDot] a)) == a \[CenterDot] a], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 9} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] (b \[CenterDot] 
            a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
           (b \[CenterDot] a)) == a \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 8}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {1, 1}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[(b \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
            ((a \[CenterDot] a) \[CenterDot] (b \[CenterDot] a)) == 
           a \[CenterDot] a], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 7} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] b == 
         (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
           (a \[CenterDot] b)) \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
           (b \[CenterDot] a))], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 5}, "Orientation" -> 1, 
        "Rule" -> (((a_) \[CenterDot] (a_)) \[CenterDot] (b_)) \[CenterDot] 
           ((a_) \[CenterDot] (b_)) -> b, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 3}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           ((a_) \[CenterDot] (b_)) -> (b \[CenterDot] a) \[CenterDot] 
           (b \[CenterDot] a), "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 8} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] a) == 
         (a \[CenterDot] a) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] a)))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 5}, 
        "Orientation" -> 1, "Rule" -> (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (b_)) \[CenterDot] ((a_) \[CenterDot] (b_)) -> b, "Side" -> 1, 
        "Subpattern" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (b_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 5}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (((a_) \[CenterDot] (a_)) \[CenterDot] (b_)) \[CenterDot] 
           ((a_) \[CenterDot] (b_)) -> b, "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 10} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] a) == 
         (a \[CenterDot] a) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
           (a \[CenterDot] a))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 8}, "Construct" -> {"Axiom", 2}, 
        "Position" -> {2}, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            ((b_) \[CenterDot] (b_))) -> a \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CenterDot] (a \[CenterDot] a) == 
           (a \[CenterDot] a) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] a))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 11} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] a) == 
         (a \[CenterDot] a) \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 10}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[a \[CenterDot] (a \[CenterDot] a) == 
           (a \[CenterDot] a) \[CenterDot] a], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 12} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] b == 
         ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b))) \[CenterDot] 
          ((b \[CenterDot] a) \[CenterDot] (b \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 7}, 
        "Construct" -> {"SubstitutionLemma", 11}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (a \[CenterDot] a), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CenterDot] b == 
           ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] b))) \[CenterDot] ((b \[CenterDot] 
              a) \[CenterDot] (b \[CenterDot] a))], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 9} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] (b \[CenterDot] b) == 
         (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b))) \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 5}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((a_) \[CenterDot] (a_)) \[CenterDot] (b_)) -> b, "Side" -> 1, 
        "Subpattern" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (b_), 
        "MatchingConstruct" -> {"Axiom", 2}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            ((b_) \[CenterDot] (b_))) -> a \[CenterDot] a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 13} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] (b \[CenterDot] b) == 
         (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b))) \[CenterDot] 
          a], "Proof" -> <|"Input" -> {"CriticalPairLemma", 9}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[b \[CenterDot] (b \[CenterDot] b) == 
           (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b))) \[CenterDot] 
            a], "Source" -> "norm"|>|>, {"CriticalPairLemma", 10} -> 
     <|"Statement" -> HoldForm[b == (a \[CenterDot] (a \[CenterDot] 
            a)) \[CenterDot] (((b \[CenterDot] (a \[CenterDot] 
              (a \[CenterDot] a))) \[CenterDot] (b \[CenterDot] 
             (a \[CenterDot] (a \[CenterDot] a)))) \[CenterDot] b)], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 5}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((a_) \[CenterDot] (a_)) \[CenterDot] (b_)) -> b, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 13}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] ((b_) \[CenterDot] 
             ((b_) \[CenterDot] (b_)))) \[CenterDot] (a_) -> 
          b \[CenterDot] (b \[CenterDot] b), "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 14} -> 
     <|"Statement" -> HoldForm[b == (a \[CenterDot] (a \[CenterDot] 
            a)) \[CenterDot] ((((a \[CenterDot] a) \[CenterDot] 
             b) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] b)) \[CenterDot] b)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 10}, 
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
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 15} -> 
     <|"Statement" -> HoldForm[b == (a \[CenterDot] (a \[CenterDot] 
            a)) \[CenterDot] (b \[CenterDot] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 14}, 
        "Construct" -> {"CriticalPairLemma", 5}, "Position" -> {2, 1}, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((a_) \[CenterDot] (a_)) \[CenterDot] (b_)) -> b, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          b == (a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
            (b \[CenterDot] b)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 16} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] b == b \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 12}, 
        "Construct" -> {"SubstitutionLemma", 15}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] (a_))) \[CenterDot] 
           ((b_) \[CenterDot] (b_)) -> b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CenterDot] b == b \[CenterDot] a], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 11} -> 
     <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
          (b \[CenterDot] (a \[CenterDot] a))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 5}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((a_) \[CenterDot] (a_)) \[CenterDot] (b_)) -> b, "Side" -> 1, 
        "Subpattern" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (b_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 16}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CenterDot] (b_) -> b \[CenterDot] a, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 12} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] (a \[CenterDot] 
            a)) \[CenterDot] (b \[CenterDot] (a \[CenterDot] a)) == 
         ((a \[CenterDot] b) \[CenterDot] b) \[CenterDot] 
          (((b \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
            (b \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
             (a \[CenterDot] a))))], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 9}, "Orientation" -> 1, 
        "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] (b_))) \[CenterDot] 
           (((b_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
             (b_))) -> b \[CenterDot] b, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 11}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           ((b_) \[CenterDot] ((a_) \[CenterDot] (a_))) -> b, 
        "MatchingSide" -> 1, "Position" -> {1, 2}|>|>, 
    {"SubstitutionLemma", 17} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] (a \[CenterDot] 
            a)) \[CenterDot] (b \[CenterDot] (a \[CenterDot] a)) == 
         (b \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
          (((b \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
            (b \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
             (a \[CenterDot] a))))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 12}, "Construct" -> 
         {"SubstitutionLemma", 16}, "Position" -> {1}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (b \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
            (b \[CenterDot] (a \[CenterDot] a)) == 
           (b \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
            (((b \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
              (b \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] (b \[CenterDot] (a \[CenterDot] 
                a))))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 18} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] (a \[CenterDot] 
            a)) \[CenterDot] (b \[CenterDot] (a \[CenterDot] a)) == 
         (b \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
          (((a \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
             (a \[CenterDot] a))) \[CenterDot] ((b \[CenterDot] 
             (a \[CenterDot] a)) \[CenterDot] (b \[CenterDot] 
             (a \[CenterDot] a))))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 17}, "Construct" -> 
         {"SubstitutionLemma", 16}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (b \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
            (b \[CenterDot] (a \[CenterDot] a)) == 
           (b \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
            (((a \[CenterDot] b) \[CenterDot] (b \[CenterDot] (a \[CenterDot] 
                a))) \[CenterDot] ((b \[CenterDot] (a \[CenterDot] 
                a)) \[CenterDot] (b \[CenterDot] (a \[CenterDot] a))))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 19} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] (a \[CenterDot] 
            a)) \[CenterDot] (b \[CenterDot] (a \[CenterDot] a)) == 
         (b \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
          (b \[CenterDot] ((b \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
            (b \[CenterDot] (a \[CenterDot] a))))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 18}, 
        "Construct" -> {"CriticalPairLemma", 11}, "Position" -> {2, 1}, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
            ((a_) \[CenterDot] (a_))) -> b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[
          (b \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
            (b \[CenterDot] (a \[CenterDot] a)) == 
           (b \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
            (b \[CenterDot] ((b \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
              (b \[CenterDot] (a \[CenterDot] a))))], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 13} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] a))) == 
         ((a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
          ((a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
             (a \[CenterDot] (a \[CenterDot] a)))))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 7}, 
        "Orientation" -> 1, "Rule" -> 
         (((a_) \[CenterDot] ((a_) \[CenterDot] (a_))) \[CenterDot] 
            (b_)) \[CenterDot] (((a_) \[CenterDot] ((a_) \[CenterDot] 
              (a_))) \[CenterDot] (b_)) -> b, "Side" -> 1, 
        "Subpattern" -> ((a_) \[CenterDot] ((a_) \[CenterDot] 
            (a_))) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 19}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] ((b_) \[CenterDot] 
             (a_))) \[CenterDot] ((a_) \[CenterDot] 
            (((a_) \[CenterDot] ((b_) \[CenterDot] (b_))) \[CenterDot] 
             ((a_) \[CenterDot] ((b_) \[CenterDot] (b_))))) -> 
          (a \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] b)), "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 20} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] a))) == 
         ((a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
          ((a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] a)))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 13}, 
        "Construct" -> {"SubstitutionLemma", 19}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] ((b_) \[CenterDot] (a_))) \[CenterDot] 
           ((a_) \[CenterDot] (((a_) \[CenterDot] ((b_) \[CenterDot] (
                b_))) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] (
                b_))))) -> (a \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] b)), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CenterDot] 
            ((a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
             (a \[CenterDot] (a \[CenterDot] a))) == 
           ((a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
             (a \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
            ((a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
             (a \[CenterDot] (a \[CenterDot] a)))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 21} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] a))) == a \[CenterDot] 
          (a \[CenterDot] a)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 20}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {}, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (a_)) -> a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CenterDot] 
            ((a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
             (a \[CenterDot] (a \[CenterDot] a))) == a \[CenterDot] 
            (a \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 14} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] (b \[CenterDot] b) == 
         ((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
          (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b)))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 5}, 
        "Orientation" -> 1, "Rule" -> (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (b_)) \[CenterDot] ((a_) \[CenterDot] (b_)) -> b, "Side" -> 1, 
        "Subpattern" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (b_), 
        "MatchingConstruct" -> {"Axiom", 2}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            ((b_) \[CenterDot] (b_))) -> a \[CenterDot] a, 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"SubstitutionLemma", 22} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] (b \[CenterDot] b) == 
         a \[CenterDot] (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] 
             b)))], "Proof" -> <|"Input" -> {"CriticalPairLemma", 14}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[b \[CenterDot] (b \[CenterDot] b) == a \[CenterDot] 
            (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b)))], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 15} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] (b \[CenterDot] b) == 
         a \[CenterDot] (a \[CenterDot] a)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 22}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
            ((b_) \[CenterDot] ((b_) \[CenterDot] (b_)))) -> 
          b \[CenterDot] (b \[CenterDot] b), "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
           ((b_) \[CenterDot] (b_))), "MatchingConstruct" -> {"Axiom", 2}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CenterDot] ((b_) \[CenterDot] ((b_) \[CenterDot] (b_))) -> 
          a \[CenterDot] a, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 16} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] a) == 
         a \[CenterDot] ((b \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] a)))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 21}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] 
           (((a_) \[CenterDot] ((a_) \[CenterDot] (a_))) \[CenterDot] 
            ((a_) \[CenterDot] ((a_) \[CenterDot] (a_)))) -> 
          a \[CenterDot] (a \[CenterDot] a), "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] ((a_) \[CenterDot] (a_)), 
        "MatchingConstruct" -> {"CriticalPairLemma", 15}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CenterDot] ((a_) \[CenterDot] (a_)) -> b \[CenterDot] 
           (b \[CenterDot] b), "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"CriticalPairLemma", 17} -> 
     <|"Statement" -> HoldForm[((b \[CenterDot] b) \[CenterDot] 
           a) \[CenterDot] ((((c \[CenterDot] (c \[CenterDot] 
               c)) \[CenterDot] (b \[CenterDot] (b \[CenterDot] 
               b))) \[CenterDot] ((c \[CenterDot] (c \[CenterDot] 
               c)) \[CenterDot] (b \[CenterDot] (b \[CenterDot] 
               b)))) \[CenterDot] a) == (a \[CenterDot] (b \[CenterDot] 
            (b \[CenterDot] b))) \[CenterDot] (a \[CenterDot] 
           (b \[CenterDot] ((c \[CenterDot] (c \[CenterDot] c)) \[CenterDot] 
             (b \[CenterDot] (b \[CenterDot] b)))))], 
      "Proof" -> <|"Construct" -> {"Axiom", 3}, "Orientation" -> 1, 
        "Rule" -> ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) \[CenterDot] 
           ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) -> 
          ((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
           ((c \[CenterDot] c) \[CenterDot] a), "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] (c_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 16}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CenterDot] (((b_) \[CenterDot] 
             ((b_) \[CenterDot] (b_))) \[CenterDot] ((a_) \[CenterDot] 
             ((a_) \[CenterDot] (a_)))) -> a \[CenterDot] (a \[CenterDot] a), 
        "MatchingSide" -> 1, "Position" -> {1, 2}|>|>, 
    {"SubstitutionLemma", 23} -> 
     <|"Statement" -> HoldForm[((b \[CenterDot] b) \[CenterDot] 
           a) \[CenterDot] ((((c \[CenterDot] (c \[CenterDot] 
               c)) \[CenterDot] (b \[CenterDot] (b \[CenterDot] 
               b))) \[CenterDot] ((c \[CenterDot] (c \[CenterDot] 
               c)) \[CenterDot] (b \[CenterDot] (b \[CenterDot] 
               b)))) \[CenterDot] a) == (a \[CenterDot] (b \[CenterDot] 
            (b \[CenterDot] b))) \[CenterDot] (a \[CenterDot] 
           (b \[CenterDot] (b \[CenterDot] b)))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 17}, 
        "Construct" -> {"CriticalPairLemma", 16}, "Position" -> {2, 2}, 
        "Rule" -> (a_) \[CenterDot] (((b_) \[CenterDot] ((b_) \[CenterDot] 
              (b_))) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
              (a_)))) -> a \[CenterDot] (a \[CenterDot] a), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          ((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            ((((c \[CenterDot] (c \[CenterDot] c)) \[CenterDot] (
                b \[CenterDot] (b \[CenterDot] b))) \[CenterDot] 
              ((c \[CenterDot] (c \[CenterDot] c)) \[CenterDot] (
                b \[CenterDot] (b \[CenterDot] b)))) \[CenterDot] a) == 
           (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b))) \[CenterDot] 
            (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b)))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 24} -> 
     <|"Statement" -> HoldForm[((b \[CenterDot] b) \[CenterDot] 
           a) \[CenterDot] ((((c \[CenterDot] (c \[CenterDot] 
               c)) \[CenterDot] (b \[CenterDot] (b \[CenterDot] 
               b))) \[CenterDot] ((c \[CenterDot] (c \[CenterDot] 
               c)) \[CenterDot] (b \[CenterDot] (b \[CenterDot] 
               b)))) \[CenterDot] a) == ((b \[CenterDot] b) \[CenterDot] 
           a) \[CenterDot] (((b \[CenterDot] b) \[CenterDot] 
            (b \[CenterDot] b)) \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 23}, 
        "Construct" -> {"Axiom", 3}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) \[CenterDot] 
           ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) -> 
          ((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
           ((c \[CenterDot] c) \[CenterDot] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[((b \[CenterDot] b) \[CenterDot] 
             a) \[CenterDot] ((((c \[CenterDot] (c \[CenterDot] 
                 c)) \[CenterDot] (b \[CenterDot] (b \[CenterDot] 
                 b))) \[CenterDot] ((c \[CenterDot] (c \[CenterDot] 
                 c)) \[CenterDot] (b \[CenterDot] (b \[CenterDot] 
                 b)))) \[CenterDot] a) == ((b \[CenterDot] b) \[CenterDot] 
             a) \[CenterDot] (((b \[CenterDot] b) \[CenterDot] 
              (b \[CenterDot] b)) \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 25} -> 
     <|"Statement" -> HoldForm[((b \[CenterDot] b) \[CenterDot] 
           a) \[CenterDot] ((((c \[CenterDot] (c \[CenterDot] 
               c)) \[CenterDot] (b \[CenterDot] (b \[CenterDot] 
               b))) \[CenterDot] ((c \[CenterDot] (c \[CenterDot] 
               c)) \[CenterDot] (b \[CenterDot] (b \[CenterDot] 
               b)))) \[CenterDot] a) == a], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 24}, "Construct" -> 
         {"CriticalPairLemma", 5}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((a_) \[CenterDot] (a_)) \[CenterDot] (b_)) -> b, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          ((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            ((((c \[CenterDot] (c \[CenterDot] c)) \[CenterDot] (
                b \[CenterDot] (b \[CenterDot] b))) \[CenterDot] 
              ((c \[CenterDot] (c \[CenterDot] c)) \[CenterDot] (
                b \[CenterDot] (b \[CenterDot] b)))) \[CenterDot] a) == a], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 26} -> 
     <|"Statement" -> HoldForm[((b \[CenterDot] b) \[CenterDot] 
           a) \[CenterDot] ((((b \[CenterDot] b) \[CenterDot] 
             (c \[CenterDot] (c \[CenterDot] c))) \[CenterDot] 
            (((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
               b)) \[CenterDot] (c \[CenterDot] (c \[CenterDot] 
               c)))) \[CenterDot] a) == a], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 25}, "Construct" -> {"Axiom", 3}, 
        "Position" -> {2, 1}, "Rule" -> 
         ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) \[CenterDot] 
           ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) -> 
          ((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
           ((c \[CenterDot] c) \[CenterDot] a), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[((b \[CenterDot] b) \[CenterDot] 
             a) \[CenterDot] ((((b \[CenterDot] b) \[CenterDot] (
                c \[CenterDot] (c \[CenterDot] c))) \[CenterDot] 
              (((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
                 b)) \[CenterDot] (c \[CenterDot] (c \[CenterDot] 
                 c)))) \[CenterDot] a) == a], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 27} -> 
     <|"Statement" -> HoldForm[((b \[CenterDot] b) \[CenterDot] 
           a) \[CenterDot] ((c \[CenterDot] (c \[CenterDot] c)) \[CenterDot] 
           a) == a], "Proof" -> <|"Input" -> {"SubstitutionLemma", 26}, 
        "Construct" -> {"CriticalPairLemma", 5}, "Position" -> {2, 1}, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((a_) \[CenterDot] (a_)) \[CenterDot] (b_)) -> b, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          ((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            ((c \[CenterDot] (c \[CenterDot] c)) \[CenterDot] a) == a], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 18} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] b) \[CenterDot] 
          (b \[CenterDot] b) == ((a \[CenterDot] a) \[CenterDot] 
           ((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] b))) \[CenterDot] 
          (b \[CenterDot] b)], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 27}, "Orientation" -> 1, 
        "Rule" -> (((a_) \[CenterDot] (a_)) \[CenterDot] (b_)) \[CenterDot] 
           (((c_) \[CenterDot] ((c_) \[CenterDot] (c_))) \[CenterDot] 
            (b_)) -> b, "Side" -> 1, "Subpattern" -> 
         ((c_) \[CenterDot] ((c_) \[CenterDot] (c_))) \[CenterDot] (b_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 5}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] (b_)) \[CenterDot] (((a_) \[CenterDot] 
             (a_)) \[CenterDot] (b_)) -> b, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 28} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] b) \[CenterDot] 
          (b \[CenterDot] b) == (b \[CenterDot] b) \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
            (b \[CenterDot] b)))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 18}, "Construct" -> 
         {"SubstitutionLemma", 16}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (b \[CenterDot] b) \[CenterDot] (b \[CenterDot] b) == 
           (b \[CenterDot] b) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             ((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] b)))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 29} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] b) \[CenterDot] 
          (b \[CenterDot] b) == (b \[CenterDot] b) \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 28}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {2, 2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[(b \[CenterDot] b) \[CenterDot] (b \[CenterDot] b) == 
           (b \[CenterDot] b) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             b)], "Source" -> "norm"|>|>, {"SubstitutionLemma", 30} -> 
     <|"Statement" -> HoldForm[b == (b \[CenterDot] b) \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 29}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[b == (b \[CenterDot] b) \[CenterDot] 
            ((a \[CenterDot] a) \[CenterDot] b)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 19} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          (b \[CenterDot] a)], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 30}, "Orientation" -> -1, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
           (((b_) \[CenterDot] (b_)) \[CenterDot] (a_)) -> a, "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"Axiom", 1}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] (a_)) -> a, 
        "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"Conclusion", 1} -> <|"Statement" -> HoldForm[a == a], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Construct" -> 
         {"CriticalPairLemma", 19}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[a == a], "Source" -> "cpl"|>|>}|>]
