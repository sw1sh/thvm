{ProofObject["EquationalLogic", Inactive[Equal][
   a \[CenterDot] (b \[CenterDot] (a \[CenterDot] c)), 
   ((c \[CenterDot] b) \[CenterDot] b) \[CenterDot] a], 
  {Inactive[Equal][((a_) \[CenterDot] (a_)) \[CenterDot] 
     ((a_) \[CenterDot] (b_)), a_], Inactive[Equal][
    (a_) \[CenterDot] ((a_) \[CenterDot] (b_)), (a_) \[CenterDot] 
     ((b_) \[CenterDot] (b_))], Inactive[Equal][(a_) \[CenterDot] 
     ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))), 
    (b_) \[CenterDot] ((b_) \[CenterDot] ((a_) \[CenterDot] (c_)))]}, 
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
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
            (a \[CenterDot] c)) == ((c \[CenterDot] b) \[CenterDot] 
            b) \[CenterDot] a], "Proof" -> <||>|>, 
     {"CriticalPairLemma", 1} -> <|"Statement" -> 
        HoldForm[(a \[CenterDot] a) \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) == 
          (a \[CenterDot] a) \[CenterDot] a], "Proof" -> 
        <|"Construct" -> {"Axiom", 2}, "Orientation" -> 1, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] (b_)) -> 
           a \[CenterDot] (b \[CenterDot] b), "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"Axiom", 1}, "MatchingOrientation" -> 1, "MatchingRule" -> 
          ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] (b_)) -> 
           a, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"CriticalPairLemma", 2} -> <|"Statement" -> 
        HoldForm[(a \[CenterDot] b) \[CenterDot] 
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
         "Position" -> {2}|>|>, {"SubstitutionLemma", 1} -> 
      <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] b))) == (a \[CenterDot] a) \[CenterDot] 
           (a \[CenterDot] a)], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 2}, "Construct" -> {"Axiom", 2}, 
         "Position" -> {}, "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
             (b_)) -> a \[CenterDot] (b \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] (a \[CenterDot] b))) == 
            (a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 2} -> 
      <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] b))) == a], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 1}, "Construct" -> {"Axiom", 1}, 
         "Position" -> {}, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] (b_)) -> a, "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] (a \[CenterDot] b))) == a], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 3} -> 
      <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] a) == a], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 2}, 
         "Construct" -> {"Axiom", 1}, "Position" -> {2, 2}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
             (b_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[(a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] 
               b) \[CenterDot] a) == a], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 4} -> <|"Statement" -> 
        HoldForm[(a \[CenterDot] b) \[CenterDot] (a \[CenterDot] a) == a], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 3}, 
         "Construct" -> {"Axiom", 2}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] (b_)) -> 
           a \[CenterDot] (b \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] a) == a], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 3} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] b == ((a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b)) \[CenterDot] a], 
       "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
             (b_)) -> a, "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] (b_), 
         "MatchingConstruct" -> {"SubstitutionLemma", 4}, 
         "MatchingOrientation" -> 1, "MatchingRule" -> 
          ((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] (a_)) -> 
           a, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"CriticalPairLemma", 4} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] b == a \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 4}, 
         "Orientation" -> 1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((a_) \[CenterDot] (a_)) -> a, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 4}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((a_) \[CenterDot] (a_)) -> a, "MatchingSide" -> 1, 
         "Position" -> {1}|>|>, {"SubstitutionLemma", 6} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] b == a \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] b))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 4}, 
         "Construct" -> {"Axiom", 2}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (b_)) -> 
           a \[CenterDot] (a \[CenterDot] b), "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] b == 
            a \[CenterDot] (a \[CenterDot] (a \[CenterDot] b))], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 5} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] c) == 
          a \[CenterDot] (b \[CenterDot] (b \[CenterDot] (a \[CenterDot] 
              c)))], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 6}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
             ((a_) \[CenterDot] (b_))) -> a \[CenterDot] b, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] ((a_) \[CenterDot] (b_)), 
         "MatchingConstruct" -> {"Axiom", 3}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
             ((b_) \[CenterDot] (c_))) -> b \[CenterDot] (b \[CenterDot] 
             (a \[CenterDot] c)), "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"CriticalPairLemma", 6} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] a == a \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] b)], 
       "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
             (b_)) -> a, "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] (a_), 
         "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] (b_)) -> a, "MatchingSide" -> 1, 
         "Position" -> {1}|>|>, {"CriticalPairLemma", 7} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
            ((a \[CenterDot] a) \[CenterDot] c)) == a \[CenterDot] 
           (b \[CenterDot] (b \[CenterDot] (a \[CenterDot] a)))], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 5}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             ((b_) \[CenterDot] ((a_) \[CenterDot] (c_)))) -> 
           a \[CenterDot] (b \[CenterDot] c), "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (c_), "MatchingConstruct" -> 
          {"CriticalPairLemma", 6}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> (a_) \[CenterDot] (((a_) \[CenterDot] 
              (a_)) \[CenterDot] (b_)) -> a \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {2, 2, 2}|>|>, 
     {"SubstitutionLemma", 13} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (b \[CenterDot] ((a \[CenterDot] 
              a) \[CenterDot] c)) == a \[CenterDot] (b \[CenterDot] a)], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 7}, 
         "Construct" -> {"CriticalPairLemma", 5}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] ((b_) \[CenterDot] 
              ((a_) \[CenterDot] (c_)))) -> a \[CenterDot] 
            (b \[CenterDot] c), "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[a \[CenterDot] (b \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] c)) == a \[CenterDot] (b \[CenterDot] a)], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 8} -> 
      <|"Statement" -> HoldForm[b == (a \[CenterDot] (a \[CenterDot] 
             (b \[CenterDot] c))) \[CenterDot] (b \[CenterDot] b)], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 4}, 
         "Orientation" -> 1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((a_) \[CenterDot] (a_)) -> a, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"Axiom", 3}, "MatchingOrientation" -> 1, "MatchingRule" -> 
          (a_) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) -> 
           b \[CenterDot] (b \[CenterDot] (a \[CenterDot] c)), 
         "MatchingSide" -> 1, "Position" -> {1}|>|>, 
     {"CriticalPairLemma", 9} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b)) == a \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] b))], "Proof" -> <|"Construct" -> {"Axiom", 2}, 
         "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
             (b_)) -> a \[CenterDot] (b \[CenterDot] b), "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"Axiom", 2}, "MatchingOrientation" -> 1, "MatchingRule" -> 
          (a_) \[CenterDot] ((a_) \[CenterDot] (b_)) -> a \[CenterDot] 
            (b \[CenterDot] b), "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"SubstitutionLemma", 14} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (a \[CenterDot] (a \[CenterDot] b)) == 
          a \[CenterDot] (a \[CenterDot] (b \[CenterDot] b))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 9}, 
         "Construct" -> {"Axiom", 2}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (b_)) -> 
           a \[CenterDot] (a \[CenterDot] b), "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
              (a \[CenterDot] b)) == a \[CenterDot] (a \[CenterDot] 
              (b \[CenterDot] b))], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 15} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] b == a \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] b))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 14}, "Construct" -> 
          {"SubstitutionLemma", 6}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
              (b_))) -> a \[CenterDot] b, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[a \[CenterDot] b == 
            a \[CenterDot] (a \[CenterDot] (b \[CenterDot] b))], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 10} -> 
      <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
           (b \[CenterDot] b)], "Proof" -> 
        <|"Construct" -> {"CriticalPairLemma", 8}, "Orientation" -> -1, 
         "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] (
                c_)))) \[CenterDot] ((b_) \[CenterDot] (b_)) -> b, 
         "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
            ((b_) \[CenterDot] (c_))), "MatchingConstruct" -> 
          {"SubstitutionLemma", 15}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
             ((b_) \[CenterDot] (b_))) -> a \[CenterDot] b, 
         "MatchingSide" -> 1, "Position" -> {1}|>|>, 
     {"CriticalPairLemma", 11} -> <|"Statement" -> 
        HoldForm[(b \[CenterDot] a) \[CenterDot] (a \[CenterDot] a) == 
          (a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
              a))) \[CenterDot] (b \[CenterDot] a)], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 3}, 
         "Orientation" -> -1, "Rule" -> 
          (((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
              (b_))) \[CenterDot] (a_) -> a \[CenterDot] b, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"CriticalPairLemma", 10}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((b_) \[CenterDot] (b_)) -> b, "MatchingSide" -> 1, 
         "Position" -> {1, 1}|>|>, {"SubstitutionLemma", 16} -> 
      <|"Statement" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
           (a \[CenterDot] a) == (a \[CenterDot] a) \[CenterDot] 
           (b \[CenterDot] a)], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 11}, "Construct" -> 
          {"CriticalPairLemma", 10}, "Position" -> {1, 2}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
             (b_)) -> b, "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[(b \[CenterDot] a) \[CenterDot] (a \[CenterDot] a) == 
            (a \[CenterDot] a) \[CenterDot] (b \[CenterDot] a)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 17} -> 
      <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
           (b \[CenterDot] a)], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 16}, "Construct" -> 
          {"CriticalPairLemma", 10}, "Position" -> {}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
             (b_)) -> b, "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[a == (a \[CenterDot] a) \[CenterDot] (b \[CenterDot] a)], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 12} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] ((b \[CenterDot] 
             b) \[CenterDot] a) == a \[CenterDot] b], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 13}, 
         "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (c_))) -> 
           a \[CenterDot] (b \[CenterDot] a), "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] (((a_) \[CenterDot] 
             (a_)) \[CenterDot] (c_)), "MatchingConstruct" -> 
          {"SubstitutionLemma", 17}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((b_) \[CenterDot] (a_)) -> a, "MatchingSide" -> 1, 
         "Position" -> {2}|>|>, {"CriticalPairLemma", 13} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
            (a \[CenterDot] (c \[CenterDot] c))) == a \[CenterDot] 
           (b \[CenterDot] (b \[CenterDot] (a \[CenterDot] c)))], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 5}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             ((b_) \[CenterDot] ((a_) \[CenterDot] (c_)))) -> 
           a \[CenterDot] (b \[CenterDot] c), "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (c_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 15}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
             ((b_) \[CenterDot] (b_))) -> a \[CenterDot] b, 
         "MatchingSide" -> 1, "Position" -> {2, 2, 2}|>|>, 
     {"SubstitutionLemma", 19} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (b \[CenterDot] (a \[CenterDot] 
             (c \[CenterDot] c))) == a \[CenterDot] (b \[CenterDot] c)], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 13}, 
         "Construct" -> {"CriticalPairLemma", 5}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] ((b_) \[CenterDot] 
              ((a_) \[CenterDot] (c_)))) -> a \[CenterDot] 
            (b \[CenterDot] c), "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[a \[CenterDot] (b \[CenterDot] (a \[CenterDot] (
                c \[CenterDot] c))) == a \[CenterDot] (b \[CenterDot] c)], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 14} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
            (a \[CenterDot] (b \[CenterDot] c))) == a \[CenterDot] 
           (b \[CenterDot] (b \[CenterDot] (a \[CenterDot] c)))], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 5}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             ((b_) \[CenterDot] ((a_) \[CenterDot] (c_)))) -> 
           a \[CenterDot] (b \[CenterDot] c), "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] ((a_) \[CenterDot] (c_)), 
         "MatchingConstruct" -> {"CriticalPairLemma", 5}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          (a_) \[CenterDot] ((b_) \[CenterDot] ((b_) \[CenterDot] 
              ((a_) \[CenterDot] (c_)))) -> a \[CenterDot] 
            (b \[CenterDot] c), "MatchingSide" -> 1, "Position" -> {2, 
          2}|>|>, {"SubstitutionLemma", 23} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
            (a \[CenterDot] (b \[CenterDot] c))) == a \[CenterDot] 
           (b \[CenterDot] c)], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 14}, "Construct" -> 
          {"CriticalPairLemma", 5}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] ((b_) \[CenterDot] 
              ((a_) \[CenterDot] (c_)))) -> a \[CenterDot] 
            (b \[CenterDot] c), "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[a \[CenterDot] (b \[CenterDot] (a \[CenterDot] (
                b \[CenterDot] c))) == a \[CenterDot] (b \[CenterDot] c)], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 15} -> 
      <|"Statement" -> HoldForm[b \[CenterDot] b == 
          (a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
           ((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] b))], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 8}, 
         "Orientation" -> -1, "Rule" -> 
          ((a_) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] (
                c_)))) \[CenterDot] ((b_) \[CenterDot] (b_)) -> b, 
         "Side" -> 1, "Subpattern" -> (b_) \[CenterDot] (c_), 
         "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] (b_)) -> a, "MatchingSide" -> 1, 
         "Position" -> {1, 2, 2}|>|>, {"SubstitutionLemma", 24} -> 
      <|"Statement" -> HoldForm[b \[CenterDot] b == 
          (a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] b], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 15}, 
         "Construct" -> {"Axiom", 1}, "Position" -> {2}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
             (b_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[b \[CenterDot] b == (a \[CenterDot] (a \[CenterDot] 
               b)) \[CenterDot] b], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 16} -> <|"Statement" -> 
        HoldForm[(b \[CenterDot] (b \[CenterDot] a)) \[CenterDot] a == 
          ((a \[CenterDot] a) \[CenterDot] ((b \[CenterDot] (b \[CenterDot] 
               a)) \[CenterDot] a)) \[CenterDot] (b \[CenterDot] 
            (b \[CenterDot] a))], "Proof" -> 
        <|"Construct" -> {"CriticalPairLemma", 3}, "Orientation" -> -1, 
         "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
              (b_))) \[CenterDot] (a_) -> a \[CenterDot] b, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 24}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] 
              (b_))) \[CenterDot] (b_) -> b \[CenterDot] b, 
         "MatchingSide" -> 1, "Position" -> {1, 1}|>|>, 
     {"SubstitutionLemma", 25} -> <|"Statement" -> 
        HoldForm[(b \[CenterDot] (b \[CenterDot] a)) \[CenterDot] a == 
          a \[CenterDot] (b \[CenterDot] (b \[CenterDot] a))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 16}, 
         "Construct" -> {"SubstitutionLemma", 17}, "Position" -> {1}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
             (a_)) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[(b \[CenterDot] (b \[CenterDot] a)) \[CenterDot] a == 
            a \[CenterDot] (b \[CenterDot] (b \[CenterDot] a))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 26} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] a == a \[CenterDot] 
           (b \[CenterDot] (b \[CenterDot] a))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 25}, 
         "Construct" -> {"SubstitutionLemma", 24}, "Position" -> {}, 
         "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] (b_))) \[CenterDot] 
            (b_) -> b \[CenterDot] b, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[a \[CenterDot] a == 
            a \[CenterDot] (b \[CenterDot] (b \[CenterDot] a))], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 17} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] a == a \[CenterDot] 
           (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
            (a \[CenterDot] b))], "Proof" -> 
        <|"Construct" -> {"SubstitutionLemma", 26}, "Orientation" -> -1, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] ((b_) \[CenterDot] 
              (a_))) -> a \[CenterDot] a, "Side" -> 1, "Subpattern" -> 
          (b_) \[CenterDot] (a_), "MatchingConstruct" -> 
          {"CriticalPairLemma", 3}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
             ((a_) \[CenterDot] (b_))) \[CenterDot] (a_) -> a \[CenterDot] b, 
         "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
     {"CriticalPairLemma", 18} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
              a) \[CenterDot] b)) == (a \[CenterDot] a) \[CenterDot] a], 
       "Proof" -> <|"Construct" -> {"Axiom", 3}, "Orientation" -> 1, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] 
              (c_))) -> b \[CenterDot] (b \[CenterDot] (a \[CenterDot] c)), 
         "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            (c_)), "MatchingConstruct" -> {"Axiom", 1}, 
         "MatchingOrientation" -> 1, "MatchingRule" -> 
          ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] (b_)) -> 
           a, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"SubstitutionLemma", 27} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (a \[CenterDot] a) == 
          (a \[CenterDot] a) \[CenterDot] a], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 18}, "Construct" -> 
          {"CriticalPairLemma", 6}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (b_)) -> a \[CenterDot] a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[a \[CenterDot] (a \[CenterDot] a) == 
            (a \[CenterDot] a) \[CenterDot] a], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 28} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] a == a \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] b)))], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 17}, "Construct" -> 
          {"SubstitutionLemma", 27}, "Position" -> {2}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (a_) -> 
           a \[CenterDot] (a \[CenterDot] a), "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] a == 
            a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] 
                b) \[CenterDot] (a \[CenterDot] b)))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 29} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] a == a \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] b)], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 28}, 
         "Construct" -> {"CriticalPairLemma", 5}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] ((b_) \[CenterDot] 
              ((a_) \[CenterDot] (c_)))) -> a \[CenterDot] 
            (b \[CenterDot] c), "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[a \[CenterDot] a == a \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] b)], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 19} -> <|"Statement" -> 
        HoldForm[(a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
           (a \[CenterDot] b) == (a \[CenterDot] (a \[CenterDot] 
             b)) \[CenterDot] (a \[CenterDot] a)], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 23}, 
         "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             ((a_) \[CenterDot] ((b_) \[CenterDot] (c_)))) -> 
           a \[CenterDot] (b \[CenterDot] c), "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] ((a_) \[CenterDot] 
            ((b_) \[CenterDot] (c_))), "MatchingConstruct" -> 
          {"SubstitutionLemma", 29}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> (a_) \[CenterDot] (((a_) \[CenterDot] 
              (b_)) \[CenterDot] (b_)) -> a \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"SubstitutionLemma", 30} -> <|"Statement" -> 
        HoldForm[(a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
           (a \[CenterDot] b) == a], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 19}, "Construct" -> 
          {"SubstitutionLemma", 4}, "Position" -> {}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
             (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[(a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
             (a \[CenterDot] b) == a], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 20} -> <|"Statement" -> 
        HoldForm[(a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) == 
          (a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] a], 
       "Proof" -> <|"Construct" -> {"Axiom", 2}, "Orientation" -> 1, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] (b_)) -> 
           a \[CenterDot] (b \[CenterDot] b), "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 30}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] 
              (b_))) \[CenterDot] ((a_) \[CenterDot] (b_)) -> a, 
         "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"SubstitutionLemma", 31} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] b == (a \[CenterDot] (a \[CenterDot] 
             b)) \[CenterDot] a], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 20}, "Construct" -> 
          {"CriticalPairLemma", 10}, "Position" -> {}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
             (b_)) -> b, "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[a \[CenterDot] b == (a \[CenterDot] (a \[CenterDot] 
               b)) \[CenterDot] a], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 21} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (a \[CenterDot] b) == 
          (a \[CenterDot] b) \[CenterDot] a], "Proof" -> 
        <|"Construct" -> {"SubstitutionLemma", 31}, "Orientation" -> -1, 
         "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] (b_))) \[CenterDot] 
            (a_) -> a \[CenterDot] b, "Side" -> 1, "Subpattern" -> 
          (a_) \[CenterDot] ((a_) \[CenterDot] (b_)), "MatchingConstruct" -> 
          {"SubstitutionLemma", 6}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
             ((a_) \[CenterDot] (b_))) -> a \[CenterDot] b, 
         "MatchingSide" -> 1, "Position" -> {1}|>|>, 
     {"CriticalPairLemma", 22} -> <|"Statement" -> 
        HoldForm[(b \[CenterDot] a) \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] (a \[CenterDot] a)) == 
          a \[CenterDot] (b \[CenterDot] a)], "Proof" -> 
        <|"Construct" -> {"CriticalPairLemma", 21}, "Orientation" -> -1, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
           a \[CenterDot] (a \[CenterDot] b), "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"CriticalPairLemma", 10}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((b_) \[CenterDot] (b_)) -> b, "MatchingSide" -> 1, 
         "Position" -> {1}|>|>, {"SubstitutionLemma", 32} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
            ((b \[CenterDot] a) \[CenterDot] a)) == a \[CenterDot] 
           (b \[CenterDot] a)], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 22}, "Construct" -> {"Axiom", 3}, 
         "Position" -> {}, "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
             ((b_) \[CenterDot] (c_))) -> b \[CenterDot] (b \[CenterDot] 
             (a \[CenterDot] c)), "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[a \[CenterDot] (a \[CenterDot] ((b \[CenterDot] 
                a) \[CenterDot] a)) == a \[CenterDot] (b \[CenterDot] a)], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 23} -> 
      <|"Statement" -> HoldForm[b \[CenterDot] (b \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] c))) == 
          a \[CenterDot] (a \[CenterDot] (b \[CenterDot] a))], 
       "Proof" -> <|"Construct" -> {"Axiom", 3}, "Orientation" -> 1, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] 
              (c_))) -> b \[CenterDot] (b \[CenterDot] (a \[CenterDot] c)), 
         "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            (c_)), "MatchingConstruct" -> {"SubstitutionLemma", 13}, 
         "MatchingOrientation" -> 1, "MatchingRule" -> 
          (a_) \[CenterDot] ((b_) \[CenterDot] (((a_) \[CenterDot] (
                a_)) \[CenterDot] (c_))) -> a \[CenterDot] 
            (b \[CenterDot] a), "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"SubstitutionLemma", 33} -> <|"Statement" -> 
        HoldForm[b \[CenterDot] (b \[CenterDot] (a \[CenterDot] a)) == 
          a \[CenterDot] (a \[CenterDot] (b \[CenterDot] a))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 23}, 
         "Construct" -> {"CriticalPairLemma", 6}, "Position" -> {2, 2}, 
         "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (b_)) -> a \[CenterDot] a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[b \[CenterDot] (b \[CenterDot] 
              (a \[CenterDot] a)) == a \[CenterDot] (a \[CenterDot] 
              (b \[CenterDot] a))], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 34} -> <|"Statement" -> 
        HoldForm[b \[CenterDot] a == a \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] a))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 33}, "Construct" -> 
          {"SubstitutionLemma", 15}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] 
              (b_))) -> a \[CenterDot] b, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[b \[CenterDot] a == 
            a \[CenterDot] (a \[CenterDot] (b \[CenterDot] a))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 35} -> 
      <|"Statement" -> HoldForm[(b \[CenterDot] a) \[CenterDot] a == 
          a \[CenterDot] (b \[CenterDot] a)], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 32}, "Construct" -> 
          {"SubstitutionLemma", 34}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] 
              (a_))) -> b \[CenterDot] a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[(b \[CenterDot] a) \[CenterDot] a == 
            a \[CenterDot] (b \[CenterDot] a)], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 36} -> <|"Statement" -> 
        HoldForm[(a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
            (a \[CenterDot] b)) == a], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 30}, "Construct" -> 
          {"SubstitutionLemma", 35}, "Position" -> {}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (b_) -> 
           b \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] (a \[CenterDot] b)) == a], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 24} -> 
      <|"Statement" -> HoldForm[a == (a \[CenterDot] b) \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
              a)))], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 36}, 
         "Orientation" -> 1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((a_) \[CenterDot] ((a_) \[CenterDot] (b_))) -> a, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"CriticalPairLemma", 12}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (a_) \[CenterDot] (((b_) \[CenterDot] 
              (b_)) \[CenterDot] (a_)) -> a \[CenterDot] b, 
         "MatchingSide" -> 1, "Position" -> {1}|>|>, 
     {"SubstitutionLemma", 37} -> <|"Statement" -> 
        HoldForm[a == (a \[CenterDot] b) \[CenterDot] 
           ((b \[CenterDot] b) \[CenterDot] a)], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 24}, 
         "Construct" -> {"SubstitutionLemma", 34}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] 
              (a_))) -> b \[CenterDot] a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a == (a \[CenterDot] b) \[CenterDot] 
             ((b \[CenterDot] b) \[CenterDot] a)], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 25} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] a) == 
          a \[CenterDot] b], "Proof" -> <|"Construct" -> 
          {"SubstitutionLemma", 13}, "Orientation" -> 1, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (c_))) -> 
           a \[CenterDot] (b \[CenterDot] a), "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] (((a_) \[CenterDot] 
             (a_)) \[CenterDot] (c_)), "MatchingConstruct" -> 
          {"SubstitutionLemma", 37}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            (((b_) \[CenterDot] (b_)) \[CenterDot] (a_)) -> a, 
         "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"SubstitutionLemma", 38} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (a \[CenterDot] (b \[CenterDot] a)) == 
          a \[CenterDot] b], "Proof" -> <|"Input" -> {"CriticalPairLemma", 
           25}, "Construct" -> {"SubstitutionLemma", 35}, "Position" -> {2}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (b_) -> 
           b \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
              (b \[CenterDot] a)) == a \[CenterDot] b], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 39} -> 
      <|"Statement" -> HoldForm[b \[CenterDot] a == a \[CenterDot] b], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 38}, 
         "Construct" -> {"SubstitutionLemma", 34}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] 
              (a_))) -> b \[CenterDot] a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[b \[CenterDot] a == 
            a \[CenterDot] b], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 26} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] b) == 
          ((a \[CenterDot] b) \[CenterDot] b) \[CenterDot] 
           (((a \[CenterDot] b) \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] a))], "Proof" -> 
        <|"Construct" -> {"SubstitutionLemma", 34}, "Orientation" -> -1, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] 
              (a_))) -> b \[CenterDot] a, "Side" -> 1, "Subpattern" -> 
          (b_) \[CenterDot] (a_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 29}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> (a_) \[CenterDot] (((a_) \[CenterDot] 
              (b_)) \[CenterDot] (b_)) -> a \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
     {"SubstitutionLemma", 44} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] b) == 
          a \[CenterDot] (a \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
              b) \[CenterDot] a))], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 26}, "Construct" -> {"Axiom", 3}, 
         "Position" -> {}, "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
             ((b_) \[CenterDot] (c_))) -> b \[CenterDot] (b \[CenterDot] 
             (a \[CenterDot] c)), "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] b) == 
            a \[CenterDot] (a \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
                b) \[CenterDot] a))], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 45} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] b) == 
          ((a \[CenterDot] b) \[CenterDot] b) \[CenterDot] a], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 44}, 
         "Construct" -> {"SubstitutionLemma", 34}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] 
              (a_))) -> b \[CenterDot] a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] b) == 
            ((a \[CenterDot] b) \[CenterDot] b) \[CenterDot] a], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 46} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] a == 
          ((a \[CenterDot] b) \[CenterDot] b) \[CenterDot] a], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 45}, 
         "Construct" -> {"SubstitutionLemma", 29}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (b_)) -> a \[CenterDot] a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[a \[CenterDot] a == 
            ((a \[CenterDot] b) \[CenterDot] b) \[CenterDot] a], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 47} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] a == 
          (b \[CenterDot] (a \[CenterDot] b)) \[CenterDot] a], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 46}, 
         "Construct" -> {"SubstitutionLemma", 35}, "Position" -> {1}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (b_) -> 
           b \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] a == 
            (b \[CenterDot] (a \[CenterDot] b)) \[CenterDot] a], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 27} -> 
      <|"Statement" -> HoldForm[b == ((a \[CenterDot] a) \[CenterDot] 
            a) \[CenterDot] (b \[CenterDot] b)], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 8}, 
         "Orientation" -> -1, "Rule" -> 
          ((a_) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] (
                c_)))) \[CenterDot] ((b_) \[CenterDot] (b_)) -> b, 
         "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            (c_)), "MatchingConstruct" -> {"SubstitutionLemma", 17}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] (a_)) -> 
           a, "MatchingSide" -> 1, "Position" -> {1, 2}|>|>, 
     {"SubstitutionLemma", 67} -> <|"Statement" -> 
        HoldForm[b == (a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
           (b \[CenterDot] b)], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 27}, "Construct" -> 
          {"SubstitutionLemma", 27}, "Position" -> {1}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (a_) -> 
           a \[CenterDot] (a \[CenterDot] a), "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[b == (a \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] (b \[CenterDot] b)], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 28} -> 
      <|"Statement" -> HoldForm[(b \[CenterDot] (b \[CenterDot] 
             b)) \[CenterDot] (a \[CenterDot] a) == 
          (a \[CenterDot] ((b \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
             (a \[CenterDot] a))) \[CenterDot] (b \[CenterDot] 
            (b \[CenterDot] b))], "Proof" -> 
        <|"Construct" -> {"CriticalPairLemma", 3}, "Orientation" -> -1, 
         "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
              (b_))) \[CenterDot] (a_) -> a \[CenterDot] b, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 67}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] 
              (a_))) \[CenterDot] ((b_) \[CenterDot] (b_)) -> b, 
         "MatchingSide" -> 1, "Position" -> {1, 1}|>|>, 
     {"CriticalPairLemma", 29} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] a == a \[CenterDot] (b \[CenterDot] 
            (a \[CenterDot] a))], "Proof" -> 
        <|"Construct" -> {"SubstitutionLemma", 17}, "Orientation" -> -1, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
             (a_)) -> a, "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] (a_), 
         "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] (b_)) -> a, "MatchingSide" -> 1, 
         "Position" -> {1}|>|>, {"SubstitutionLemma", 68} -> 
      <|"Statement" -> HoldForm[(b \[CenterDot] (b \[CenterDot] 
             b)) \[CenterDot] (a \[CenterDot] a) == 
          (a \[CenterDot] a) \[CenterDot] (b \[CenterDot] (b \[CenterDot] 
             b))], "Proof" -> <|"Input" -> {"CriticalPairLemma", 28}, 
         "Construct" -> {"CriticalPairLemma", 29}, "Position" -> {1}, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] ((a_) \[CenterDot] 
              (a_))) -> a \[CenterDot] a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[(b \[CenterDot] (b \[CenterDot] 
               b)) \[CenterDot] (a \[CenterDot] a) == 
            (a \[CenterDot] a) \[CenterDot] (b \[CenterDot] (b \[CenterDot] 
               b))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 69} -> 
      <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
           (b \[CenterDot] (b \[CenterDot] b))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 68}, 
         "Construct" -> {"SubstitutionLemma", 67}, "Position" -> {}, 
         "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] (a_))) \[CenterDot] 
            ((b_) \[CenterDot] (b_)) -> b, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
             (b \[CenterDot] (b \[CenterDot] b))], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 5} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (b \[CenterDot] 
            (((a \[CenterDot] c) \[CenterDot] (a \[CenterDot] 
               c)) \[CenterDot] a)) == ((c \[CenterDot] b) \[CenterDot] 
            b) \[CenterDot] a], "Proof" -> <|"Input" -> {"Hypothesis", 1}, 
         "Construct" -> {"CriticalPairLemma", 3}, "Position" -> {2, 2}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> ((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] b)) \[CenterDot] a, "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
              (((a \[CenterDot] c) \[CenterDot] (a \[CenterDot] 
                 c)) \[CenterDot] a)) == ((c \[CenterDot] b) \[CenterDot] 
              b) \[CenterDot] a], "Source" -> "cpl"|>|>, 
     {"SubstitutionLemma", 7} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
             (b \[CenterDot] (((a \[CenterDot] c) \[CenterDot] 
                (a \[CenterDot] c)) \[CenterDot] a)))) == 
          ((c \[CenterDot] b) \[CenterDot] b) \[CenterDot] a], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 5}, 
         "Construct" -> {"SubstitutionLemma", 6}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> a \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] b)), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
              (a \[CenterDot] (b \[CenterDot] (((a \[CenterDot] 
                   c) \[CenterDot] (a \[CenterDot] c)) \[CenterDot] a)))) == 
            ((c \[CenterDot] b) \[CenterDot] b) \[CenterDot] a], 
         "Source" -> "cpl"|>|>, {"SubstitutionLemma", 8} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
            (a \[CenterDot] (b \[CenterDot] (a \[CenterDot] c)))) == 
          ((c \[CenterDot] b) \[CenterDot] b) \[CenterDot] a], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 7}, 
         "Construct" -> {"CriticalPairLemma", 3}, "Position" -> {2, 2, 2, 2}, 
         "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
              (b_))) \[CenterDot] (a_) -> a \[CenterDot] b, 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           a \[CenterDot] (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] 
                (a \[CenterDot] c)))) == ((c \[CenterDot] b) \[CenterDot] 
              b) \[CenterDot] a], "Source" -> "cpl"|>|>, 
     {"SubstitutionLemma", 9} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (b \[CenterDot] (b \[CenterDot] 
             (a \[CenterDot] (a \[CenterDot] c)))) == 
          ((c \[CenterDot] b) \[CenterDot] b) \[CenterDot] a], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 8}, 
         "Construct" -> {"Axiom", 3}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] 
              (c_))) -> b \[CenterDot] (b \[CenterDot] (a \[CenterDot] c)), 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           a \[CenterDot] (b \[CenterDot] (b \[CenterDot] (a \[CenterDot] 
                (a \[CenterDot] c)))) == ((c \[CenterDot] b) \[CenterDot] 
              b) \[CenterDot] a], "Source" -> "cpl"|>|>, 
     {"SubstitutionLemma", 10} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (b \[CenterDot] (b \[CenterDot] 
             (a \[CenterDot] (c \[CenterDot] c)))) == 
          ((c \[CenterDot] b) \[CenterDot] b) \[CenterDot] a], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 9}, 
         "Construct" -> {"Axiom", 2}, "Position" -> {2, 2, 2}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] (b_)) -> 
           a \[CenterDot] (b \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
              (b \[CenterDot] (a \[CenterDot] (c \[CenterDot] c)))) == 
            ((c \[CenterDot] b) \[CenterDot] b) \[CenterDot] a], 
         "Source" -> "cpl"|>|>, {"SubstitutionLemma", 11} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
            (a \[CenterDot] (b \[CenterDot] (c \[CenterDot] c)))) == 
          ((c \[CenterDot] b) \[CenterDot] b) \[CenterDot] a], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 10}, 
         "Construct" -> {"Axiom", 3}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] 
              (c_))) -> b \[CenterDot] (b \[CenterDot] (a \[CenterDot] c)), 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           a \[CenterDot] (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] 
                (c \[CenterDot] c)))) == ((c \[CenterDot] b) \[CenterDot] 
              b) \[CenterDot] a], "Source" -> "cpl"|>|>, 
     {"SubstitutionLemma", 12} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (b \[CenterDot] (c \[CenterDot] c)) == 
          ((c \[CenterDot] b) \[CenterDot] b) \[CenterDot] a], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 11}, 
         "Construct" -> {"SubstitutionLemma", 6}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
              (b_))) -> a \[CenterDot] b, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
              (c \[CenterDot] c)) == ((c \[CenterDot] b) \[CenterDot] 
              b) \[CenterDot] a], "Source" -> "cpl"|>|>, 
     {"SubstitutionLemma", 18} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (b \[CenterDot] (c \[CenterDot] c)) == 
          ((c \[CenterDot] b) \[CenterDot] b) \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] ((c \[CenterDot] b) \[CenterDot] 
             b))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 12}, 
         "Construct" -> {"CriticalPairLemma", 12}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> a \[CenterDot] 
            ((b \[CenterDot] b) \[CenterDot] a), "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
              (c \[CenterDot] c)) == ((c \[CenterDot] b) \[CenterDot] 
              b) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              ((c \[CenterDot] b) \[CenterDot] b))], "Source" -> "cpl"|>|>, 
     {"SubstitutionLemma", 20} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (b \[CenterDot] (c \[CenterDot] c)) == 
          ((c \[CenterDot] b) \[CenterDot] b) \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] (((c \[CenterDot] b) \[CenterDot] 
              b) \[CenterDot] (((c \[CenterDot] b) \[CenterDot] 
               b) \[CenterDot] ((c \[CenterDot] b) \[CenterDot] b))))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 18}, 
         "Construct" -> {"SubstitutionLemma", 19}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (c_)) -> 
           a \[CenterDot] (b \[CenterDot] (a \[CenterDot] (c \[CenterDot] 
               c))), "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[a \[CenterDot] (b \[CenterDot] (c \[CenterDot] c)) == 
            ((c \[CenterDot] b) \[CenterDot] b) \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] (((c \[CenterDot] 
                 b) \[CenterDot] b) \[CenterDot] (((c \[CenterDot] 
                  b) \[CenterDot] b) \[CenterDot] ((c \[CenterDot] 
                  b) \[CenterDot] b))))], "Source" -> "cpl"|>|>, 
     {"SubstitutionLemma", 21} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (b \[CenterDot] (c \[CenterDot] c)) == 
          ((c \[CenterDot] b) \[CenterDot] b) \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] ((c \[CenterDot] b) \[CenterDot] 
             ((c \[CenterDot] b) \[CenterDot] (((c \[CenterDot] 
                 b) \[CenterDot] b) \[CenterDot] b))))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 20}, 
         "Construct" -> {"Axiom", 3}, "Position" -> {2, 2}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] 
              (c_))) -> b \[CenterDot] (b \[CenterDot] (a \[CenterDot] c)), 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           a \[CenterDot] (b \[CenterDot] (c \[CenterDot] c)) == 
            ((c \[CenterDot] b) \[CenterDot] b) \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] ((c \[CenterDot] 
                b) \[CenterDot] ((c \[CenterDot] b) \[CenterDot] 
                (((c \[CenterDot] b) \[CenterDot] b) \[CenterDot] b))))], 
         "Source" -> "cpl"|>|>, {"SubstitutionLemma", 22} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
            (c \[CenterDot] c)) == ((c \[CenterDot] b) \[CenterDot] 
            b) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
            ((c \[CenterDot] b) \[CenterDot] ((((c \[CenterDot] 
                 b) \[CenterDot] b) \[CenterDot] b) \[CenterDot] 
              (((c \[CenterDot] b) \[CenterDot] b) \[CenterDot] b))))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 21}, 
         "Construct" -> {"Axiom", 2}, "Position" -> {2, 2}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] (b_)) -> 
           a \[CenterDot] (b \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
              (c \[CenterDot] c)) == ((c \[CenterDot] b) \[CenterDot] 
              b) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              ((c \[CenterDot] b) \[CenterDot] ((((c \[CenterDot] 
                   b) \[CenterDot] b) \[CenterDot] b) \[CenterDot] 
                (((c \[CenterDot] b) \[CenterDot] b) \[CenterDot] b))))], 
         "Source" -> "cpl"|>|>, {"SubstitutionLemma", 40} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
            (c \[CenterDot] c)) == ((c \[CenterDot] b) \[CenterDot] 
            b) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
            ((c \[CenterDot] b) \[CenterDot] ((((c \[CenterDot] 
                 b) \[CenterDot] b) \[CenterDot] b) \[CenterDot] 
              (b \[CenterDot] ((c \[CenterDot] b) \[CenterDot] b)))))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 22}, 
         "Construct" -> {"SubstitutionLemma", 39}, "Position" -> {2, 2, 2, 
          2}, "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           a \[CenterDot] (b \[CenterDot] (c \[CenterDot] c)) == 
            ((c \[CenterDot] b) \[CenterDot] b) \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] ((c \[CenterDot] 
                b) \[CenterDot] ((((c \[CenterDot] b) \[CenterDot] 
                  b) \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
                 ((c \[CenterDot] b) \[CenterDot] b)))))], 
         "Source" -> "cpl"|>|>, {"SubstitutionLemma", 41} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
            (c \[CenterDot] c)) == ((c \[CenterDot] b) \[CenterDot] 
            b) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
            ((c \[CenterDot] b) \[CenterDot] ((((c \[CenterDot] 
                 b) \[CenterDot] b) \[CenterDot] b) \[CenterDot] 
              (b \[CenterDot] (b \[CenterDot] (c \[CenterDot] b))))))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 40}, 
         "Construct" -> {"SubstitutionLemma", 39}, "Position" -> {2, 2, 2, 2, 
          2}, "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           a \[CenterDot] (b \[CenterDot] (c \[CenterDot] c)) == 
            ((c \[CenterDot] b) \[CenterDot] b) \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] ((c \[CenterDot] 
                b) \[CenterDot] ((((c \[CenterDot] b) \[CenterDot] 
                  b) \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
                 (b \[CenterDot] (c \[CenterDot] b))))))], 
         "Source" -> "cpl"|>|>, {"SubstitutionLemma", 42} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
            (c \[CenterDot] c)) == ((c \[CenterDot] b) \[CenterDot] 
            b) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
            ((c \[CenterDot] b) \[CenterDot] ((((c \[CenterDot] 
                 b) \[CenterDot] b) \[CenterDot] b) \[CenterDot] 
              (c \[CenterDot] b))))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 41}, "Construct" -> 
          {"SubstitutionLemma", 34}, "Position" -> {2, 2, 2, 2}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] 
              (a_))) -> b \[CenterDot] a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
              (c \[CenterDot] c)) == ((c \[CenterDot] b) \[CenterDot] 
              b) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              ((c \[CenterDot] b) \[CenterDot] ((((c \[CenterDot] 
                   b) \[CenterDot] b) \[CenterDot] b) \[CenterDot] 
                (c \[CenterDot] b))))], "Source" -> "cpl"|>|>, 
     {"SubstitutionLemma", 43} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (b \[CenterDot] (c \[CenterDot] c)) == 
          ((c \[CenterDot] b) \[CenterDot] b) \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] ((c \[CenterDot] b) \[CenterDot] 
             ((b \[CenterDot] ((c \[CenterDot] b) \[CenterDot] 
                b)) \[CenterDot] (c \[CenterDot] b))))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 42}, 
         "Construct" -> {"SubstitutionLemma", 39}, "Position" -> {2, 2, 2, 
          1}, "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           a \[CenterDot] (b \[CenterDot] (c \[CenterDot] c)) == 
            ((c \[CenterDot] b) \[CenterDot] b) \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] ((c \[CenterDot] 
                b) \[CenterDot] ((b \[CenterDot] ((c \[CenterDot] 
                   b) \[CenterDot] b)) \[CenterDot] (c \[CenterDot] b))))], 
         "Source" -> "cpl"|>|>, {"SubstitutionLemma", 48} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
            (c \[CenterDot] c)) == ((c \[CenterDot] b) \[CenterDot] 
            b) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
            ((c \[CenterDot] b) \[CenterDot] ((c \[CenterDot] b) \[CenterDot] 
              (c \[CenterDot] b))))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 43}, "Construct" -> 
          {"SubstitutionLemma", 47}, "Position" -> {2, 2, 2}, 
         "Rule" -> ((a_) \[CenterDot] ((b_) \[CenterDot] (a_))) \[CenterDot] 
            (b_) -> b \[CenterDot] b, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
              (c \[CenterDot] c)) == ((c \[CenterDot] b) \[CenterDot] 
              b) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              ((c \[CenterDot] b) \[CenterDot] ((c \[CenterDot] 
                 b) \[CenterDot] (c \[CenterDot] b))))], 
         "Source" -> "cpl"|>|>, {"SubstitutionLemma", 49} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
            (c \[CenterDot] c)) == ((c \[CenterDot] b) \[CenterDot] 
            b) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
            (c \[CenterDot] (c \[CenterDot] ((c \[CenterDot] b) \[CenterDot] 
               b))))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 48}, 
         "Construct" -> {"Axiom", 3}, "Position" -> {2, 2}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] 
              (c_))) -> b \[CenterDot] (b \[CenterDot] (a \[CenterDot] c)), 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           a \[CenterDot] (b \[CenterDot] (c \[CenterDot] c)) == 
            ((c \[CenterDot] b) \[CenterDot] b) \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] (c \[CenterDot] (c \[CenterDot] 
                ((c \[CenterDot] b) \[CenterDot] b))))], 
         "Source" -> "cpl"|>|>, {"SubstitutionLemma", 50} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
            (c \[CenterDot] c)) == ((c \[CenterDot] b) \[CenterDot] 
            b) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
            (c \[CenterDot] (c \[CenterDot] (b \[CenterDot] (c \[CenterDot] 
                b)))))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 49}, 
         "Construct" -> {"SubstitutionLemma", 39}, "Position" -> {2, 2, 2, 
          2}, "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           a \[CenterDot] (b \[CenterDot] (c \[CenterDot] c)) == 
            ((c \[CenterDot] b) \[CenterDot] b) \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] (c \[CenterDot] (c \[CenterDot] 
                (b \[CenterDot] (c \[CenterDot] b)))))], 
         "Source" -> "cpl"|>|>, {"SubstitutionLemma", 51} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
            (c \[CenterDot] c)) == ((c \[CenterDot] b) \[CenterDot] 
            b) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
            (b \[CenterDot] (b \[CenterDot] (c \[CenterDot] (c \[CenterDot] 
                b)))))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 50}, 
         "Construct" -> {"Axiom", 3}, "Position" -> {2, 2}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] 
              (c_))) -> b \[CenterDot] (b \[CenterDot] (a \[CenterDot] c)), 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           a \[CenterDot] (b \[CenterDot] (c \[CenterDot] c)) == 
            ((c \[CenterDot] b) \[CenterDot] b) \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] (b \[CenterDot] (b \[CenterDot] 
                (c \[CenterDot] (c \[CenterDot] b)))))], 
         "Source" -> "cpl"|>|>, {"SubstitutionLemma", 52} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
            (c \[CenterDot] c)) == ((c \[CenterDot] b) \[CenterDot] 
            b) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
            (b \[CenterDot] (b \[CenterDot] b)))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 51}, 
         "Construct" -> {"SubstitutionLemma", 26}, "Position" -> {2, 2, 2}, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] ((b_) \[CenterDot] 
              (a_))) -> a \[CenterDot] a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
              (c \[CenterDot] c)) == ((c \[CenterDot] b) \[CenterDot] 
              b) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              (b \[CenterDot] (b \[CenterDot] b)))], "Source" -> "cpl"|>|>, 
     {"SubstitutionLemma", 53} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (b \[CenterDot] (c \[CenterDot] c)) == 
          (b \[CenterDot] (c \[CenterDot] b)) \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
             (b \[CenterDot] b)))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 52}, "Construct" -> 
          {"SubstitutionLemma", 39}, "Position" -> {1}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           a \[CenterDot] (b \[CenterDot] (c \[CenterDot] c)) == 
            (b \[CenterDot] (c \[CenterDot] b)) \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] (b \[CenterDot] (b \[CenterDot] 
                b)))], "Source" -> "cpl"|>|>, {"SubstitutionLemma", 54} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
            (c \[CenterDot] c)) == ((a \[CenterDot] a) \[CenterDot] 
            (b \[CenterDot] (b \[CenterDot] b))) \[CenterDot] 
           (b \[CenterDot] (c \[CenterDot] b))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 53}, 
         "Construct" -> {"SubstitutionLemma", 39}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           a \[CenterDot] (b \[CenterDot] (c \[CenterDot] c)) == 
            ((a \[CenterDot] a) \[CenterDot] (b \[CenterDot] (b \[CenterDot] 
                b))) \[CenterDot] (b \[CenterDot] (c \[CenterDot] b))], 
         "Source" -> "cpl"|>|>, {"SubstitutionLemma", 55} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
            (c \[CenterDot] c)) == ((a \[CenterDot] a) \[CenterDot] 
            (b \[CenterDot] (b \[CenterDot] b))) \[CenterDot] 
           ((c \[CenterDot] b) \[CenterDot] b)], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 54}, 
         "Construct" -> {"SubstitutionLemma", 39}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           a \[CenterDot] (b \[CenterDot] (c \[CenterDot] c)) == 
            ((a \[CenterDot] a) \[CenterDot] (b \[CenterDot] (b \[CenterDot] 
                b))) \[CenterDot] ((c \[CenterDot] b) \[CenterDot] b)], 
         "Source" -> "cpl"|>|>, {"SubstitutionLemma", 56} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
            (c \[CenterDot] c)) == ((a \[CenterDot] a) \[CenterDot] 
            (b \[CenterDot] (b \[CenterDot] b))) \[CenterDot] 
           ((b \[CenterDot] c) \[CenterDot] b)], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 55}, 
         "Construct" -> {"SubstitutionLemma", 39}, "Position" -> {2, 1}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           a \[CenterDot] (b \[CenterDot] (c \[CenterDot] c)) == 
            ((a \[CenterDot] a) \[CenterDot] (b \[CenterDot] (b \[CenterDot] 
                b))) \[CenterDot] ((b \[CenterDot] c) \[CenterDot] b)], 
         "Source" -> "cpl"|>|>, {"SubstitutionLemma", 57} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
            (c \[CenterDot] c)) == ((b \[CenterDot] c) \[CenterDot] 
            b) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
            (b \[CenterDot] (b \[CenterDot] b)))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 56}, 
         "Construct" -> {"SubstitutionLemma", 39}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           a \[CenterDot] (b \[CenterDot] (c \[CenterDot] c)) == 
            ((b \[CenterDot] c) \[CenterDot] b) \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] (b \[CenterDot] (b \[CenterDot] 
                b)))], "Source" -> "cpl"|>|>, {"SubstitutionLemma", 58} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
            (c \[CenterDot] c)) == ((b \[CenterDot] c) \[CenterDot] 
            b) \[CenterDot] ((b \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
            (a \[CenterDot] a))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 57}, "Construct" -> 
          {"SubstitutionLemma", 39}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           a \[CenterDot] (b \[CenterDot] (c \[CenterDot] c)) == 
            ((b \[CenterDot] c) \[CenterDot] b) \[CenterDot] 
             ((b \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
              (a \[CenterDot] a))], "Source" -> "cpl"|>|>, 
     {"SubstitutionLemma", 59} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (b \[CenterDot] (c \[CenterDot] c)) == 
          ((b \[CenterDot] c) \[CenterDot] b) \[CenterDot] 
           (((b \[CenterDot] b) \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] a))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 58}, "Construct" -> 
          {"SubstitutionLemma", 39}, "Position" -> {2, 1}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           a \[CenterDot] (b \[CenterDot] (c \[CenterDot] c)) == 
            ((b \[CenterDot] c) \[CenterDot] b) \[CenterDot] 
             (((b \[CenterDot] b) \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] a))], "Source" -> "cpl"|>|>, 
     {"SubstitutionLemma", 60} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (b \[CenterDot] (c \[CenterDot] c)) == 
          (((b \[CenterDot] b) \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] a)) \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
            b)], "Proof" -> <|"Input" -> {"SubstitutionLemma", 59}, 
         "Construct" -> {"SubstitutionLemma", 39}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           a \[CenterDot] (b \[CenterDot] (c \[CenterDot] c)) == 
            (((b \[CenterDot] b) \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((b \[CenterDot] 
               c) \[CenterDot] b)], "Source" -> "cpl"|>|>, 
     {"SubstitutionLemma", 61} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (b \[CenterDot] (c \[CenterDot] c)) == 
          (((b \[CenterDot] b) \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] a)) \[CenterDot] (b \[CenterDot] 
            (b \[CenterDot] c))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 60}, "Construct" -> 
          {"SubstitutionLemma", 39}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           a \[CenterDot] (b \[CenterDot] (c \[CenterDot] c)) == 
            (((b \[CenterDot] b) \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] (b \[CenterDot] 
              (b \[CenterDot] c))], "Source" -> "cpl"|>|>, 
     {"SubstitutionLemma", 62} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (b \[CenterDot] (c \[CenterDot] c)) == 
          (((b \[CenterDot] b) \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] a)) \[CenterDot] (b \[CenterDot] 
            (c \[CenterDot] c))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 61}, "Construct" -> {"Axiom", 2}, 
         "Position" -> {2}, "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
             (b_)) -> a \[CenterDot] (b \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
              (c \[CenterDot] c)) == (((b \[CenterDot] b) \[CenterDot] 
               b) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
             (b \[CenterDot] (c \[CenterDot] c))], "Source" -> "cpl"|>|>, 
     {"SubstitutionLemma", 63} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (b \[CenterDot] (c \[CenterDot] c)) == 
          (b \[CenterDot] (c \[CenterDot] c)) \[CenterDot] 
           (((b \[CenterDot] b) \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] a))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 62}, "Construct" -> 
          {"SubstitutionLemma", 39}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           a \[CenterDot] (b \[CenterDot] (c \[CenterDot] c)) == 
            (b \[CenterDot] (c \[CenterDot] c)) \[CenterDot] 
             (((b \[CenterDot] b) \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] a))], "Source" -> "cpl"|>|>, 
     {"SubstitutionLemma", 64} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (b \[CenterDot] (c \[CenterDot] c)) == 
          (b \[CenterDot] (c \[CenterDot] c)) \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
             b))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 63}, 
         "Construct" -> {"SubstitutionLemma", 39}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           a \[CenterDot] (b \[CenterDot] (c \[CenterDot] c)) == 
            (b \[CenterDot] (c \[CenterDot] c)) \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] ((b \[CenterDot] 
                b) \[CenterDot] b))], "Source" -> "cpl"|>|>, 
     {"SubstitutionLemma", 65} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (b \[CenterDot] (c \[CenterDot] c)) == 
          (b \[CenterDot] (c \[CenterDot] c)) \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
             (b \[CenterDot] b)))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 64}, "Construct" -> 
          {"SubstitutionLemma", 39}, "Position" -> {2, 2}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           a \[CenterDot] (b \[CenterDot] (c \[CenterDot] c)) == 
            (b \[CenterDot] (c \[CenterDot] c)) \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] (b \[CenterDot] (b \[CenterDot] 
                b)))], "Source" -> "cpl"|>|>, {"SubstitutionLemma", 66} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
            (c \[CenterDot] c)) == ((a \[CenterDot] a) \[CenterDot] 
            (b \[CenterDot] (b \[CenterDot] b))) \[CenterDot] 
           (b \[CenterDot] (c \[CenterDot] c))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 65}, 
         "Construct" -> {"SubstitutionLemma", 39}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           a \[CenterDot] (b \[CenterDot] (c \[CenterDot] c)) == 
            ((a \[CenterDot] a) \[CenterDot] (b \[CenterDot] (b \[CenterDot] 
                b))) \[CenterDot] (b \[CenterDot] (c \[CenterDot] c))], 
         "Source" -> "cpl"|>|>, {"Conclusion", 1} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
            (c \[CenterDot] c)) == a \[CenterDot] (b \[CenterDot] 
            (c \[CenterDot] c))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 66}, "Construct" -> 
          {"SubstitutionLemma", 69}, "Position" -> {1}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
             ((b_) \[CenterDot] (b_))) -> a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
              (c \[CenterDot] c)) == a \[CenterDot] (b \[CenterDot] 
              (c \[CenterDot] c))], "Source" -> "cpl"|>|>}|>], 
 ProofObject["EquationalLogic", Inactive[Equal][
   (a \[CenterDot] a) \[CenterDot] (b \[CenterDot] a), a], 
  {Inactive[Equal][((a_) \[CenterDot] (a_)) \[CenterDot] 
     ((a_) \[CenterDot] (b_)), a_], Inactive[Equal][
    (a_) \[CenterDot] ((a_) \[CenterDot] (b_)), (a_) \[CenterDot] 
     ((b_) \[CenterDot] (b_))], Inactive[Equal][(a_) \[CenterDot] 
     ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))), 
    (b_) \[CenterDot] ((b_) \[CenterDot] ((a_) \[CenterDot] (c_)))]}, 
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
      <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
           (b \[CenterDot] a) == a], "Proof" -> <||>|>, 
     {"CriticalPairLemma", 1} -> <|"Statement" -> 
        HoldForm[(a \[CenterDot] a) \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) == 
          (a \[CenterDot] a) \[CenterDot] a], "Proof" -> 
        <|"Construct" -> {"Axiom", 2}, "Orientation" -> 1, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] (b_)) -> 
           a \[CenterDot] (b \[CenterDot] b), "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"Axiom", 1}, "MatchingOrientation" -> 1, "MatchingRule" -> 
          ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] (b_)) -> 
           a, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"CriticalPairLemma", 2} -> <|"Statement" -> 
        HoldForm[(a \[CenterDot] b) \[CenterDot] 
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
         "Position" -> {2}|>|>, {"SubstitutionLemma", 1} -> 
      <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] b))) == (a \[CenterDot] a) \[CenterDot] 
           (a \[CenterDot] a)], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 2}, "Construct" -> {"Axiom", 2}, 
         "Position" -> {}, "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
             (b_)) -> a \[CenterDot] (b \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] (a \[CenterDot] b))) == 
            (a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 2} -> 
      <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] b))) == a], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 1}, "Construct" -> {"Axiom", 1}, 
         "Position" -> {}, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] (b_)) -> a, "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] 
                a) \[CenterDot] (a \[CenterDot] b))) == a], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 3} -> 
      <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] a) == a], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 2}, 
         "Construct" -> {"Axiom", 1}, "Position" -> {2, 2}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
             (b_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[(a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] 
               b) \[CenterDot] a) == a], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 4} -> <|"Statement" -> 
        HoldForm[(a \[CenterDot] b) \[CenterDot] (a \[CenterDot] a) == a], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 3}, 
         "Construct" -> {"Axiom", 2}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] (b_)) -> 
           a \[CenterDot] (b \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] a) == a], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 3} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] b == ((a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b)) \[CenterDot] a], 
       "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
             (b_)) -> a, "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] (b_), 
         "MatchingConstruct" -> {"SubstitutionLemma", 4}, 
         "MatchingOrientation" -> 1, "MatchingRule" -> 
          ((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] (a_)) -> 
           a, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"CriticalPairLemma", 4} -> <|"Statement" -> 
        HoldForm[b == (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] 
              c))) \[CenterDot] (b \[CenterDot] b)], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 4}, 
         "Orientation" -> 1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((a_) \[CenterDot] (a_)) -> a, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"Axiom", 3}, "MatchingOrientation" -> 1, "MatchingRule" -> 
          (a_) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) -> 
           b \[CenterDot] (b \[CenterDot] (a \[CenterDot] c)), 
         "MatchingSide" -> 1, "Position" -> {1}|>|>, 
     {"CriticalPairLemma", 5} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b)) == a \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] b))], "Proof" -> <|"Construct" -> {"Axiom", 2}, 
         "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
             (b_)) -> a \[CenterDot] (b \[CenterDot] b), "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"Axiom", 2}, "MatchingOrientation" -> 1, "MatchingRule" -> 
          (a_) \[CenterDot] ((a_) \[CenterDot] (b_)) -> a \[CenterDot] 
            (b \[CenterDot] b), "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"SubstitutionLemma", 5} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (a \[CenterDot] (a \[CenterDot] b)) == 
          a \[CenterDot] (a \[CenterDot] (b \[CenterDot] b))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 5}, 
         "Construct" -> {"Axiom", 2}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (b_)) -> 
           a \[CenterDot] (a \[CenterDot] b), "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
              (a \[CenterDot] b)) == a \[CenterDot] (a \[CenterDot] 
              (b \[CenterDot] b))], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 6} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] b == a \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 4}, 
         "Orientation" -> 1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((a_) \[CenterDot] (a_)) -> a, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 4}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((a_) \[CenterDot] (a_)) -> a, "MatchingSide" -> 1, 
         "Position" -> {1}|>|>, {"SubstitutionLemma", 6} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] b == a \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] b))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 6}, 
         "Construct" -> {"Axiom", 2}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (b_)) -> 
           a \[CenterDot] (a \[CenterDot] b), "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] b == 
            a \[CenterDot] (a \[CenterDot] (a \[CenterDot] b))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 7} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] b == a \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] b))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 5}, 
         "Construct" -> {"SubstitutionLemma", 6}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
              (b_))) -> a \[CenterDot] b, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[a \[CenterDot] b == 
            a \[CenterDot] (a \[CenterDot] (b \[CenterDot] b))], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 7} -> 
      <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
           (b \[CenterDot] b)], "Proof" -> 
        <|"Construct" -> {"CriticalPairLemma", 4}, "Orientation" -> -1, 
         "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] (
                c_)))) \[CenterDot] ((b_) \[CenterDot] (b_)) -> b, 
         "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
            ((b_) \[CenterDot] (c_))), "MatchingConstruct" -> 
          {"SubstitutionLemma", 7}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
             ((b_) \[CenterDot] (b_))) -> a \[CenterDot] b, 
         "MatchingSide" -> 1, "Position" -> {1}|>|>, 
     {"CriticalPairLemma", 8} -> <|"Statement" -> 
        HoldForm[(b \[CenterDot] a) \[CenterDot] (a \[CenterDot] a) == 
          (a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
              a))) \[CenterDot] (b \[CenterDot] a)], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 3}, 
         "Orientation" -> -1, "Rule" -> 
          (((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
              (b_))) \[CenterDot] (a_) -> a \[CenterDot] b, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"CriticalPairLemma", 7}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((b_) \[CenterDot] (b_)) -> b, "MatchingSide" -> 1, 
         "Position" -> {1, 1}|>|>, {"SubstitutionLemma", 8} -> 
      <|"Statement" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
           (a \[CenterDot] a) == (a \[CenterDot] a) \[CenterDot] 
           (b \[CenterDot] a)], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 8}, "Construct" -> 
          {"CriticalPairLemma", 7}, "Position" -> {1, 2}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
             (b_)) -> b, "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[(b \[CenterDot] a) \[CenterDot] (a \[CenterDot] a) == 
            (a \[CenterDot] a) \[CenterDot] (b \[CenterDot] a)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 9} -> 
      <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
           (b \[CenterDot] a)], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 8}, "Construct" -> 
          {"CriticalPairLemma", 7}, "Position" -> {}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
             (b_)) -> b, "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[a == (a \[CenterDot] a) \[CenterDot] (b \[CenterDot] a)], 
         "Source" -> "norm"|>|>, {"Conclusion", 1} -> 
      <|"Statement" -> HoldForm[a == a], "Proof" -> 
        <|"Input" -> {"Hypothesis", 1}, "Construct" -> {"SubstitutionLemma", 
           9}, "Position" -> {}, "Rule" -> 
          ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] (a_)) -> 
           a, "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[a == a], "Source" -> "cpl"|>|>}|>]}
