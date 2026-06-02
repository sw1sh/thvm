ProofObject["EquationalLogic", Inactive[Equal][
  (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
     (b \[CenterDot] c))) \[CenterDot] (a \[CenterDot] 
    ((b \[CenterDot] c) \[CenterDot] (b \[CenterDot] c))), 
  (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
    c) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
     (a \[CenterDot] b)) \[CenterDot] c)], 
 {Inactive[Equal][((a_) \[CenterDot] (a_)) \[CenterDot] 
    ((a_) \[CenterDot] (b_)), a_], Inactive[Equal][
   (a_) \[CenterDot] ((a_) \[CenterDot] (b_)), 
   (a_) \[CenterDot] ((b_) \[CenterDot] (b_))], 
  Inactive[Equal][(a_) \[CenterDot] ((a_) \[CenterDot] 
     ((b_) \[CenterDot] (c_))), (b_) \[CenterDot] 
    ((b_) \[CenterDot] ((a_) \[CenterDot] (c_)))]}, 
 <|"Variables" -> {a, b, c}, "Constants" -> {}, 
  "Proof" -> {{"Axiom", 1} -> <|"Statement" -> 
       HoldForm[(a \[CenterDot] a) \[CenterDot] (a \[CenterDot] b) == a], 
      "Proof" -> <||>|>, {"Axiom", 2} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] b) == 
         a \[CenterDot] (b \[CenterDot] b)], "Proof" -> <||>|>, 
    {"Axiom", 3} -> <|"Statement" -> HoldForm[
        a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c)) == 
         b \[CenterDot] (b \[CenterDot] (a \[CenterDot] c))], 
      "Proof" -> <||>|>, {"Hypothesis", 1} -> 
     <|"Statement" -> HoldForm[
        (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] (b \[CenterDot] 
             c))) \[CenterDot] (a \[CenterDot] ((b \[CenterDot] 
             c) \[CenterDot] (b \[CenterDot] c))) == 
         (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
           c) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b)) \[CenterDot] c)], "Proof" -> <||>|>, 
    {"CriticalPairLemma", 1} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
          ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) == 
         (a \[CenterDot] a) \[CenterDot] a], 
      "Proof" -> <|"Construct" -> {"Axiom", 2}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] (b_)) -> 
          a \[CenterDot] (b \[CenterDot] b), "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"Axiom", 1}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] (b_)) -> a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 2} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
          ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] b))) == (a \[CenterDot] a) \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] a)], 
      "Proof" -> <|"Construct" -> {"Axiom", 3}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] 
             (c_))) -> b \[CenterDot] (b \[CenterDot] (a \[CenterDot] c)), 
        "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
           (c_)), "MatchingConstruct" -> {"CriticalPairLemma", 1}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((a_) \[CenterDot] (a_)) \[CenterDot] (((a_) \[CenterDot] 
             (b_)) \[CenterDot] ((a_) \[CenterDot] (b_))) -> 
          (a \[CenterDot] a) \[CenterDot] a, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 3} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
          ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] b))) == (a \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] a)], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 2}, "Construct" -> {"Axiom", 2}, 
        "Position" -> {}, "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
            (b_)) -> a \[CenterDot] (b \[CenterDot] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] b))) == (a \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 4} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
          ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] b))) == a], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 3}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {}, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (b_)) -> a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] b))) == a], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 5} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
          ((a \[CenterDot] b) \[CenterDot] a) == a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 4}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {2, 2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            (b_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[(a \[CenterDot] b) \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] a) == a], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 6} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
          (a \[CenterDot] a) == a], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 5}, "Construct" -> {"Axiom", 2}, 
        "Position" -> {}, "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
            (b_)) -> a \[CenterDot] (b \[CenterDot] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] a) == a], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 3} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] b == a \[CenterDot] 
          ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 6}, 
        "Orientation" -> 1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           ((a_) \[CenterDot] (a_)) -> a, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 6}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           ((a_) \[CenterDot] (a_)) -> a, "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 7} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] b == a \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] b))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 3}, 
        "Construct" -> {"Axiom", 2}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (b_)) -> 
          a \[CenterDot] (a \[CenterDot] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CenterDot] b == 
           a \[CenterDot] (a \[CenterDot] (a \[CenterDot] b))], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 4} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] c) == 
         a \[CenterDot] (b \[CenterDot] (b \[CenterDot] (a \[CenterDot] 
             c)))], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 7}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
            ((a_) \[CenterDot] (b_))) -> a \[CenterDot] b, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] ((a_) \[CenterDot] (b_)), 
        "MatchingConstruct" -> {"Axiom", 3}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
            ((b_) \[CenterDot] (c_))) -> b \[CenterDot] (b \[CenterDot] 
            (a \[CenterDot] c)), "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 5} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] a == a \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] b)], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            (b_)) -> a, "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] (a_), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (b_)) -> a, "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 6} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] c)) == a \[CenterDot] 
          (b \[CenterDot] (b \[CenterDot] (a \[CenterDot] a)))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 4}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            ((b_) \[CenterDot] ((a_) \[CenterDot] (c_)))) -> 
          a \[CenterDot] (b \[CenterDot] c), "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (c_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 5}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CenterDot] (((a_) \[CenterDot] 
             (a_)) \[CenterDot] (b_)) -> a \[CenterDot] a, 
        "MatchingSide" -> 1, "Position" -> {2, 2, 2}|>|>, 
    {"SubstitutionLemma", 8} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] c)) == a \[CenterDot] 
          (b \[CenterDot] a)], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 6}, "Construct" -> 
         {"CriticalPairLemma", 4}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] ((b_) \[CenterDot] 
             ((a_) \[CenterDot] (c_)))) -> a \[CenterDot] (b \[CenterDot] c), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CenterDot] (b \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              c)) == a \[CenterDot] (b \[CenterDot] a)], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 7} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] c))) == a \[CenterDot] 
          (b \[CenterDot] (b \[CenterDot] (a \[CenterDot] c)))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 4}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            ((b_) \[CenterDot] ((a_) \[CenterDot] (c_)))) -> 
          a \[CenterDot] (b \[CenterDot] c), "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] ((a_) \[CenterDot] (c_)), 
        "MatchingConstruct" -> {"CriticalPairLemma", 4}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CenterDot] ((b_) \[CenterDot] ((b_) \[CenterDot] 
             ((a_) \[CenterDot] (c_)))) -> a \[CenterDot] (b \[CenterDot] c), 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"SubstitutionLemma", 9} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] c))) == a \[CenterDot] 
          (b \[CenterDot] c)], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 7}, "Construct" -> 
         {"CriticalPairLemma", 4}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] ((b_) \[CenterDot] 
             ((a_) \[CenterDot] (c_)))) -> a \[CenterDot] (b \[CenterDot] c), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CenterDot] (b \[CenterDot] (a \[CenterDot] (b \[CenterDot] 
               c))) == a \[CenterDot] (b \[CenterDot] c)], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 8} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] b == 
         ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
          a], "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            (b_)) -> a, "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] (b_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 6}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] (a_)) -> a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 9} -> 
     <|"Statement" -> HoldForm[b == (a \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] c))) \[CenterDot] (b \[CenterDot] b)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 6}, 
        "Orientation" -> 1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           ((a_) \[CenterDot] (a_)) -> a, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"Axiom", 3}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) -> 
          b \[CenterDot] (b \[CenterDot] (a \[CenterDot] c)), 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 10} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] b == 
         (a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
          ((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] b))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 9}, 
        "Orientation" -> -1, "Rule" -> 
         ((a_) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] 
              (c_)))) \[CenterDot] ((b_) \[CenterDot] (b_)) -> b, 
        "Side" -> 1, "Subpattern" -> (b_) \[CenterDot] (c_), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (b_)) -> a, "MatchingSide" -> 1, 
        "Position" -> {1, 2, 2}|>|>, {"SubstitutionLemma", 10} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] b == 
         (a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] b], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 10}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            (b_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[b \[CenterDot] b == (a \[CenterDot] (a \[CenterDot] 
              b)) \[CenterDot] b], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 11} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] (b \[CenterDot] 
            a)) \[CenterDot] a == ((a \[CenterDot] a) \[CenterDot] 
           ((b \[CenterDot] (b \[CenterDot] a)) \[CenterDot] a)) \[CenterDot] 
          (b \[CenterDot] (b \[CenterDot] a))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 8}, 
        "Orientation" -> -1, "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((a_) \[CenterDot] (b_))) \[CenterDot] (a_) -> a \[CenterDot] b, 
        "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] (b_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 10}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] ((a_) \[CenterDot] (b_))) \[CenterDot] (b_) -> 
          b \[CenterDot] b, "MatchingSide" -> 1, "Position" -> {1, 1}|>|>, 
    {"CriticalPairLemma", 12} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) == 
         a \[CenterDot] (a \[CenterDot] (b \[CenterDot] b))], 
      "Proof" -> <|"Construct" -> {"Axiom", 2}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] (b_)) -> 
          a \[CenterDot] (b \[CenterDot] b), "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"Axiom", 2}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CenterDot] ((a_) \[CenterDot] (b_)) -> a \[CenterDot] 
           (b \[CenterDot] b), "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 11} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
           (a \[CenterDot] b)) == a \[CenterDot] (a \[CenterDot] 
           (b \[CenterDot] b))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 12}, "Construct" -> {"Axiom", 2}, 
        "Position" -> {}, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            (b_)) -> a \[CenterDot] (a \[CenterDot] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
             (a \[CenterDot] b)) == a \[CenterDot] (a \[CenterDot] 
             (b \[CenterDot] b))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 12} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] b == a \[CenterDot] 
          (a \[CenterDot] (b \[CenterDot] b))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 11}, 
        "Construct" -> {"SubstitutionLemma", 7}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (b_))) -> a \[CenterDot] b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CenterDot] b == 
           a \[CenterDot] (a \[CenterDot] (b \[CenterDot] b))], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 13} -> 
     <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
          (b \[CenterDot] b)], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 9}, "Orientation" -> -1, 
        "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] 
              (c_)))) \[CenterDot] ((b_) \[CenterDot] (b_)) -> b, 
        "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
           ((b_) \[CenterDot] (c_))), "MatchingConstruct" -> 
         {"SubstitutionLemma", 12}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
            ((b_) \[CenterDot] (b_))) -> a \[CenterDot] b, 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 14} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] a) == (a \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
          (b \[CenterDot] a)], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 8}, "Orientation" -> -1, 
        "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
             (b_))) \[CenterDot] (a_) -> a \[CenterDot] b, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 13}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           ((b_) \[CenterDot] (b_)) -> b, "MatchingSide" -> 1, 
        "Position" -> {1, 1}|>|>, {"SubstitutionLemma", 13} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] a) == (a \[CenterDot] a) \[CenterDot] 
          (b \[CenterDot] a)], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 14}, "Construct" -> 
         {"CriticalPairLemma", 13}, "Position" -> {1, 2}, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
            (b_)) -> b, "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[(b \[CenterDot] a) \[CenterDot] (a \[CenterDot] a) == 
           (a \[CenterDot] a) \[CenterDot] (b \[CenterDot] a)], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 14} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          (b \[CenterDot] a)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 13}, "Construct" -> 
         {"CriticalPairLemma", 13}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
            (b_)) -> b, "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[a == (a \[CenterDot] a) \[CenterDot] (b \[CenterDot] a)], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 15} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] (b \[CenterDot] 
            a)) \[CenterDot] a == a \[CenterDot] (b \[CenterDot] 
           (b \[CenterDot] a))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 11}, "Construct" -> 
         {"SubstitutionLemma", 14}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[(b \[CenterDot] (b \[CenterDot] a)) \[CenterDot] a == 
           a \[CenterDot] (b \[CenterDot] (b \[CenterDot] a))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 16} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] a == a \[CenterDot] 
          (b \[CenterDot] (b \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 15}, 
        "Construct" -> {"SubstitutionLemma", 10}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] (b_))) \[CenterDot] 
           (b_) -> b \[CenterDot] b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CenterDot] a == 
           a \[CenterDot] (b \[CenterDot] (b \[CenterDot] a))], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 15} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] a == a \[CenterDot] 
          (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
           (a \[CenterDot] b))], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 16}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] ((b_) \[CenterDot] 
             (a_))) -> a \[CenterDot] a, "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] (a_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 8}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((a_) \[CenterDot] (b_))) \[CenterDot] (a_) -> a \[CenterDot] b, 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"CriticalPairLemma", 16} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] b)) == 
         (a \[CenterDot] a) \[CenterDot] a], 
      "Proof" -> <|"Construct" -> {"Axiom", 3}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] 
             (c_))) -> b \[CenterDot] (b \[CenterDot] (a \[CenterDot] c)), 
        "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
           (c_)), "MatchingConstruct" -> {"Axiom", 1}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] (b_)) -> a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 17} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] a) == 
         (a \[CenterDot] a) \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 16}, 
        "Construct" -> {"CriticalPairLemma", 5}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (b_)) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CenterDot] (a \[CenterDot] a) == 
           (a \[CenterDot] a) \[CenterDot] a], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 18} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] a == a \[CenterDot] 
          ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b)))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 15}, "Construct" -> 
         {"SubstitutionLemma", 17}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (a \[CenterDot] a), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CenterDot] a == 
           a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 19} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] a == a \[CenterDot] 
          ((a \[CenterDot] b) \[CenterDot] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 18}, 
        "Construct" -> {"CriticalPairLemma", 4}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] ((b_) \[CenterDot] 
             ((a_) \[CenterDot] (c_)))) -> a \[CenterDot] (b \[CenterDot] c), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[a \[CenterDot] a == 
           a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] b)], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 17} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] (a \[CenterDot] 
            b)) \[CenterDot] (a \[CenterDot] b) == 
         (a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
          (a \[CenterDot] a)], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 9}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] ((a_) \[CenterDot] 
             ((b_) \[CenterDot] (c_)))) -> a \[CenterDot] (b \[CenterDot] c), 
        "Side" -> 1, "Subpattern" -> (b_) \[CenterDot] ((a_) \[CenterDot] 
           ((b_) \[CenterDot] (c_))), "MatchingConstruct" -> 
         {"SubstitutionLemma", 19}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CenterDot] (((a_) \[CenterDot] 
             (b_)) \[CenterDot] (b_)) -> a \[CenterDot] a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 20} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] (a \[CenterDot] 
            b)) \[CenterDot] (a \[CenterDot] b) == a], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 17}, 
        "Construct" -> {"SubstitutionLemma", 6}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[(a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
            (a \[CenterDot] b) == a], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 18} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] (a \[CenterDot] 
            b)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
           (a \[CenterDot] b)) == (a \[CenterDot] (a \[CenterDot] 
            b)) \[CenterDot] a], "Proof" -> <|"Construct" -> {"Axiom", 2}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
            (b_)) -> a \[CenterDot] (b \[CenterDot] b), "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 20}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (b_))) \[CenterDot] ((a_) \[CenterDot] (b_)) -> a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 21} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] b == 
         (a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 18}, 
        "Construct" -> {"CriticalPairLemma", 13}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
            (b_)) -> b, "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[a \[CenterDot] b == (a \[CenterDot] (a \[CenterDot] 
              b)) \[CenterDot] a], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 19} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] b) == 
         (a \[CenterDot] b) \[CenterDot] a], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 21}, 
        "Orientation" -> -1, "Rule" -> 
         ((a_) \[CenterDot] ((a_) \[CenterDot] (b_))) \[CenterDot] (a_) -> 
          a \[CenterDot] b, "Side" -> 1, "Subpattern" -> 
         (a_) \[CenterDot] ((a_) \[CenterDot] (b_)), "MatchingConstruct" -> 
         {"SubstitutionLemma", 7}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
            ((a_) \[CenterDot] (b_))) -> a \[CenterDot] b, 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 20} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
          ((b \[CenterDot] a) \[CenterDot] (a \[CenterDot] a)) == 
         a \[CenterDot] (b \[CenterDot] a)], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 19}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (a_) -> a \[CenterDot] (a \[CenterDot] b), "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 13}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           ((b_) \[CenterDot] (b_)) -> b, "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 22} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] a)) == a \[CenterDot] 
          (b \[CenterDot] a)], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 20}, "Construct" -> {"Axiom", 3}, 
        "Position" -> {}, "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
            ((b_) \[CenterDot] (c_))) -> b \[CenterDot] (b \[CenterDot] 
            (a \[CenterDot] c)), "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[a \[CenterDot] (a \[CenterDot] ((b \[CenterDot] 
               a) \[CenterDot] a)) == a \[CenterDot] (b \[CenterDot] a)], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 21} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] (b \[CenterDot] 
           (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] c))) == 
         a \[CenterDot] (a \[CenterDot] (b \[CenterDot] a))], 
      "Proof" -> <|"Construct" -> {"Axiom", 3}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] 
             (c_))) -> b \[CenterDot] (b \[CenterDot] (a \[CenterDot] c)), 
        "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
           (c_)), "MatchingConstruct" -> {"SubstitutionLemma", 8}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CenterDot] ((b_) \[CenterDot] (((a_) \[CenterDot] 
              (a_)) \[CenterDot] (c_))) -> a \[CenterDot] (b \[CenterDot] a), 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 23} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] (b \[CenterDot] 
           (a \[CenterDot] a)) == a \[CenterDot] (a \[CenterDot] 
           (b \[CenterDot] a))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 21}, "Construct" -> 
         {"CriticalPairLemma", 5}, "Position" -> {2, 2}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (b_)) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[b \[CenterDot] (b \[CenterDot] 
             (a \[CenterDot] a)) == a \[CenterDot] (a \[CenterDot] 
             (b \[CenterDot] a))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 24} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] a == a \[CenterDot] 
          (a \[CenterDot] (b \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 23}, 
        "Construct" -> {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] 
             (b_))) -> a \[CenterDot] b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[b \[CenterDot] a == 
           a \[CenterDot] (a \[CenterDot] (b \[CenterDot] a))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 25} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] a) \[CenterDot] a == 
         a \[CenterDot] (b \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 22}, 
        "Construct" -> {"SubstitutionLemma", 24}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] 
             (a_))) -> b \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(b \[CenterDot] a) \[CenterDot] a == 
           a \[CenterDot] (b \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 26} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] b)) == a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 20}, 
        "Construct" -> {"SubstitutionLemma", 25}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (b_) -> 
          b \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] b)) == a], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 22} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((b \[CenterDot] b) \[CenterDot] a) == a \[CenterDot] b], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 8}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            (((a_) \[CenterDot] (a_)) \[CenterDot] (c_))) -> 
          a \[CenterDot] (b \[CenterDot] a), "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] (((a_) \[CenterDot] 
            (a_)) \[CenterDot] (c_)), "MatchingConstruct" -> 
         {"SubstitutionLemma", 14}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
           ((b_) \[CenterDot] (a_)) -> a, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 23} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] b) \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
             a)))], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 26}, 
        "Orientation" -> 1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           ((a_) \[CenterDot] ((a_) \[CenterDot] (b_))) -> a, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 22}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CenterDot] (((b_) \[CenterDot] 
             (b_)) \[CenterDot] (a_)) -> a \[CenterDot] b, 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"SubstitutionLemma", 27} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] b) \[CenterDot] 
          ((b \[CenterDot] b) \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 23}, 
        "Construct" -> {"SubstitutionLemma", 24}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] 
             (a_))) -> b \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a == (a \[CenterDot] b) \[CenterDot] 
            ((b \[CenterDot] b) \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 24} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((b \[CenterDot] a) \[CenterDot] a) == a \[CenterDot] b], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 8}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            (((a_) \[CenterDot] (a_)) \[CenterDot] (c_))) -> 
          a \[CenterDot] (b \[CenterDot] a), "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] (((a_) \[CenterDot] 
            (a_)) \[CenterDot] (c_)), "MatchingConstruct" -> 
         {"SubstitutionLemma", 27}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((b_) \[CenterDot] (b_)) \[CenterDot] (a_)) -> a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 28} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
           (b \[CenterDot] a)) == a \[CenterDot] b], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 24}, 
        "Construct" -> {"SubstitutionLemma", 25}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (b_) -> 
          b \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
             (b \[CenterDot] a)) == a \[CenterDot] b], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 29} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] a == a \[CenterDot] b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 28}, 
        "Construct" -> {"SubstitutionLemma", 24}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] 
             (a_))) -> b \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[b \[CenterDot] a == a \[CenterDot] b], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] c))) \[CenterDot] (a \[CenterDot] 
           ((b \[CenterDot] c) \[CenterDot] (b \[CenterDot] c))) == 
         (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
           c) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b)) \[CenterDot] c)], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Construct" -> {"Axiom", 2}, 
        "Position" -> {1}, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            (b_)) -> a \[CenterDot] (a \[CenterDot] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) \[CenterDot] 
            (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
              (b \[CenterDot] c))) == (((a \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] b)) \[CenterDot] c) \[CenterDot] 
            (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
               b)) \[CenterDot] c)], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] c))) \[CenterDot] (a \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] c))) == 
         (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
           c) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b)) \[CenterDot] c)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 1}, 
        "Construct" -> {"Axiom", 2}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (b_)) -> 
          a \[CenterDot] (a \[CenterDot] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) == 
           (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
             c) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] b)) \[CenterDot] c)], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 30} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] c))) \[CenterDot] (a \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] c))) == 
         (c \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
             b))) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b)) \[CenterDot] c)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 2}, 
        "Construct" -> {"SubstitutionLemma", 29}, "Position" -> {1}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) == 
           (c \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
               b))) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] b)) \[CenterDot] c)], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 31} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] c))) \[CenterDot] (a \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] c))) == 
         (c \[CenterDot] (c \[CenterDot] (a \[CenterDot] b))) \[CenterDot] 
          (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
           c)], "Proof" -> <|"Input" -> {"SubstitutionLemma", 30}, 
        "Construct" -> {"Axiom", 2}, "Position" -> {1}, 
        "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (b_)) -> 
          a \[CenterDot] (a \[CenterDot] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[
          (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) == 
           (c \[CenterDot] (c \[CenterDot] (a \[CenterDot] b))) \[CenterDot] 
            (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
               b)) \[CenterDot] c)], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 32} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] c))) \[CenterDot] (a \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] c))) == 
         (a \[CenterDot] (a \[CenterDot] (c \[CenterDot] b))) \[CenterDot] 
          (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
           c)], "Proof" -> <|"Input" -> {"SubstitutionLemma", 31}, 
        "Construct" -> {"Axiom", 3}, "Position" -> {1}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] 
             (c_))) -> b \[CenterDot] (b \[CenterDot] (a \[CenterDot] c)), 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) == 
           (a \[CenterDot] (a \[CenterDot] (c \[CenterDot] b))) \[CenterDot] 
            (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
               b)) \[CenterDot] c)], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 33} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] c))) \[CenterDot] (a \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] c))) == 
         (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) \[CenterDot] 
          (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
           c)], "Proof" -> <|"Input" -> {"SubstitutionLemma", 32}, 
        "Construct" -> {"SubstitutionLemma", 29}, "Position" -> {1, 2, 2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) == 
           (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) \[CenterDot] 
            (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
               b)) \[CenterDot] c)], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 34} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] c))) \[CenterDot] (a \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] c))) == 
         (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) \[CenterDot] 
          (c \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b)))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 33}, "Construct" -> 
         {"SubstitutionLemma", 29}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) == 
           (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) \[CenterDot] 
            (c \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] b)))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 35} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] c))) \[CenterDot] (a \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] c))) == 
         (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) \[CenterDot] 
          (c \[CenterDot] (c \[CenterDot] (a \[CenterDot] b)))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 34}, 
        "Construct" -> {"Axiom", 2}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (b_)) -> 
          a \[CenterDot] (a \[CenterDot] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[
          (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) == 
           (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) \[CenterDot] 
            (c \[CenterDot] (c \[CenterDot] (a \[CenterDot] b)))], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 36} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] c))) \[CenterDot] (a \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] c))) == 
         (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] (c \[CenterDot] b)))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 35}, 
        "Construct" -> {"Axiom", 3}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] 
             (c_))) -> b \[CenterDot] (b \[CenterDot] (a \[CenterDot] c)), 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) == 
           (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] (c \[CenterDot] b)))], 
        "Source" -> "cpl"|>|>, {"Conclusion", 1} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] c))) \[CenterDot] (a \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] c))) == 
         (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c)))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 36}, 
        "Construct" -> {"SubstitutionLemma", 29}, "Position" -> {2, 2, 2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) == 
           (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c)))], 
        "Source" -> "cpl"|>|>}|>]
