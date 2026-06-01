{ProofObject["EquationalLogic", Inactive[Equal][
   (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] (b \[CenterDot] c)), a], 
  {Inactive[Equal][(a_) \[CenterDot] ((b_) \[CenterDot] 
      ((a_) \[CenterDot] (c_))), (((c_) \[CenterDot] (b_)) \[CenterDot] 
      (b_)) \[CenterDot] (a_)], Inactive[Equal][
    ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] (a_)), a_]}, 
  <|"Variables" -> {a, b, c}, "Constants" -> {}, 
   "Proof" -> {{"Axiom", 1} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (b \[CenterDot] (a \[CenterDot] c)) == 
          ((c \[CenterDot] b) \[CenterDot] b) \[CenterDot] a], 
       "Proof" -> <||>|>, {"Axiom", 2} -> 
      <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
           (b \[CenterDot] a) == a], "Proof" -> <||>|>, 
     {"Hypothesis", 1} -> <|"Statement" -> HoldForm[
         (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] (b \[CenterDot] 
             c)) == a], "Proof" -> <||>|>, {"CriticalPairLemma", 1} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] a == a \[CenterDot] 
           (b \[CenterDot] (a \[CenterDot] a))], 
       "Proof" -> <|"Construct" -> {"Axiom", 2}, "Orientation" -> 1, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
             (a_)) -> a, "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] (a_), 
         "MatchingConstruct" -> {"Axiom", 2}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((b_) \[CenterDot] (a_)) -> a, "MatchingSide" -> 1, 
         "Position" -> {1}|>|>, {"CriticalPairLemma", 2} -> 
      <|"Statement" -> HoldForm[
         (((c \[CenterDot] (a \[CenterDot] a)) \[CenterDot] b) \[CenterDot] 
            b) \[CenterDot] a == a \[CenterDot] (b \[CenterDot] 
            (a \[CenterDot] a))], "Proof" -> <|"Construct" -> {"Axiom", 1}, 
         "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             ((a_) \[CenterDot] (c_))) -> ((c \[CenterDot] b) \[CenterDot] 
             b) \[CenterDot] a, "Side" -> 1, "Subpattern" -> 
          (a_) \[CenterDot] (c_), "MatchingConstruct" -> 
          {"CriticalPairLemma", 1}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             ((a_) \[CenterDot] (a_))) -> a \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
     {"SubstitutionLemma", 1} -> <|"Statement" -> 
        HoldForm[(((c \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
             b) \[CenterDot] b) \[CenterDot] a == a \[CenterDot] a], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 2}, 
         "Construct" -> {"CriticalPairLemma", 1}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] ((a_) \[CenterDot] 
              (a_))) -> a \[CenterDot] a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[
           (((c \[CenterDot] (a \[CenterDot] a)) \[CenterDot] b) \[CenterDot] 
              b) \[CenterDot] a == a \[CenterDot] a], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 3} -> <|"Statement" -> 
        HoldForm[(((b \[CenterDot] b) \[CenterDot] b) \[CenterDot] 
            b) \[CenterDot] a == a \[CenterDot] (b \[CenterDot] b)], 
       "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] ((a_) \[CenterDot] 
              (c_))) -> ((c \[CenterDot] b) \[CenterDot] b) \[CenterDot] a, 
         "Side" -> 1, "Subpattern" -> (b_) \[CenterDot] ((a_) \[CenterDot] 
            (c_)), "MatchingConstruct" -> {"CriticalPairLemma", 1}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          (a_) \[CenterDot] ((b_) \[CenterDot] ((a_) \[CenterDot] (a_))) -> 
           a \[CenterDot] a, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"SubstitutionLemma", 2} -> <|"Statement" -> 
        HoldForm[(b \[CenterDot] (b \[CenterDot] (b \[CenterDot] 
              b))) \[CenterDot] a == a \[CenterDot] (b \[CenterDot] b)], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 3}, 
         "Construct" -> {"Axiom", 1}, "Position" -> {1}, 
         "Rule" -> (((c_) \[CenterDot] (b_)) \[CenterDot] (b_)) \[CenterDot] 
            (a_) -> a \[CenterDot] (b \[CenterDot] (a \[CenterDot] c)), 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           (b \[CenterDot] (b \[CenterDot] (b \[CenterDot] b))) \[CenterDot] 
             a == a \[CenterDot] (b \[CenterDot] b)], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 3} -> <|"Statement" -> 
        HoldForm[(b \[CenterDot] b) \[CenterDot] a == a \[CenterDot] 
           (b \[CenterDot] b)], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 2}, "Construct" -> 
          {"CriticalPairLemma", 1}, "Position" -> {1}, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] ((a_) \[CenterDot] 
              (a_))) -> a \[CenterDot] a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[(b \[CenterDot] b) \[CenterDot] a == 
            a \[CenterDot] (b \[CenterDot] b)], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 4} -> <|"Statement" -> 
        HoldForm[((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
             b)) \[CenterDot] a == a \[CenterDot] b], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 3}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             (b_)) -> (b \[CenterDot] b) \[CenterDot] a, "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"Axiom", 2}, "MatchingOrientation" -> 1, "MatchingRule" -> 
          ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] (a_)) -> 
           a, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"SubstitutionLemma", 4} -> <|"Statement" -> 
        HoldForm[b \[CenterDot] a == a \[CenterDot] b], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 4}, 
         "Construct" -> {"Axiom", 2}, "Position" -> {1}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
             (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[b \[CenterDot] a == a \[CenterDot] b], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 5} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] 
           (((c \[CenterDot] (a \[CenterDot] a)) \[CenterDot] b) \[CenterDot] 
            b) == a \[CenterDot] a], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 1}, "Construct" -> 
          {"SubstitutionLemma", 4}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           a \[CenterDot] (((c \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
               b) \[CenterDot] b) == a \[CenterDot] a], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 6} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
            ((c \[CenterDot] (a \[CenterDot] a)) \[CenterDot] b)) == 
          a \[CenterDot] a], "Proof" -> <|"Input" -> {"SubstitutionLemma", 
           5}, "Construct" -> {"SubstitutionLemma", 4}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           a \[CenterDot] (b \[CenterDot] ((c \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] b)) == a \[CenterDot] a], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 5} -> 
      <|"Statement" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
           (a \[CenterDot] a) == a], "Proof" -> 
        <|"Construct" -> {"SubstitutionLemma", 4}, "Orientation" -> 1, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, "Side" -> 1, 
         "Subpattern" -> {}, "MatchingConstruct" -> {"Axiom", 2}, 
         "MatchingOrientation" -> 1, "MatchingRule" -> 
          ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] (a_)) -> 
           a, "MatchingSide" -> 1, "Position" -> {}|>|>, 
     {"CriticalPairLemma", 6} -> <|"Statement" -> 
        HoldForm[a == (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] a)], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 5}, 
         "Orientation" -> 1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((b_) \[CenterDot] (b_)) -> b, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 4}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {1}|>|>, 
     {"CriticalPairLemma", 7} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] a == a \[CenterDot] 
           ((b \[CenterDot] b) \[CenterDot] b)], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 6}, 
         "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             (((c_) \[CenterDot] ((a_) \[CenterDot] (a_))) \[CenterDot] 
              (b_))) -> a \[CenterDot] a, "Side" -> 1, "Subpattern" -> 
          ((c_) \[CenterDot] ((a_) \[CenterDot] (a_))) \[CenterDot] (b_), 
         "MatchingConstruct" -> {"CriticalPairLemma", 6}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          ((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] (a_)) -> 
           a, "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
     {"SubstitutionLemma", 7} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] a == a \[CenterDot] (b \[CenterDot] 
            (b \[CenterDot] b))], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 7}, "Construct" -> 
          {"SubstitutionLemma", 3}, "Position" -> {2}, 
         "Rule" -> ((b_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
           a \[CenterDot] (b \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] a == 
            a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b))], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 8} -> 
      <|"Statement" -> HoldForm[(((a \[CenterDot] a) \[CenterDot] 
             b) \[CenterDot] b) \[CenterDot] a == a \[CenterDot] 
           (b \[CenterDot] b)], "Proof" -> <|"Construct" -> {"Axiom", 1}, 
         "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             ((a_) \[CenterDot] (c_))) -> ((c \[CenterDot] b) \[CenterDot] 
             b) \[CenterDot] a, "Side" -> 1, "Subpattern" -> 
          (b_) \[CenterDot] ((a_) \[CenterDot] (c_)), "MatchingConstruct" -> 
          {"SubstitutionLemma", 7}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             ((b_) \[CenterDot] (b_))) -> a \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"SubstitutionLemma", 8} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
             b) \[CenterDot] b) == a \[CenterDot] (b \[CenterDot] b)], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 8}, 
         "Construct" -> {"SubstitutionLemma", 4}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           a \[CenterDot] (((a \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
              b) == a \[CenterDot] (b \[CenterDot] b)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 9} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
            ((a \[CenterDot] a) \[CenterDot] b)) == a \[CenterDot] 
           (b \[CenterDot] b)], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 8}, "Construct" -> 
          {"SubstitutionLemma", 4}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           a \[CenterDot] (b \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               b)) == a \[CenterDot] (b \[CenterDot] b)], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 9} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] ((b \[CenterDot] 
             a) \[CenterDot] (b \[CenterDot] a)) == a \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] a)], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 9}, 
         "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (b_))) -> 
           a \[CenterDot] (b \[CenterDot] b), "Side" -> 1, 
         "Subpattern" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (b_), 
         "MatchingConstruct" -> {"Axiom", 2}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((b_) \[CenterDot] (a_)) -> a, "MatchingSide" -> 1, 
         "Position" -> {2, 2}|>|>, {"SubstitutionLemma", 10} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] ((b \[CenterDot] 
             a) \[CenterDot] (b \[CenterDot] a)) == a \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] a))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 9}, 
         "Construct" -> {"SubstitutionLemma", 4}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
               a)) == a \[CenterDot] (a \[CenterDot] (b \[CenterDot] a))], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 10} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] b == 
          ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
           b], "Proof" -> <|"Construct" -> {"Axiom", 2}, "Orientation" -> 1, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
             (a_)) -> a, "Side" -> 1, "Subpattern" -> (b_) \[CenterDot] (a_), 
         "MatchingConstruct" -> {"Axiom", 2}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((b_) \[CenterDot] (a_)) -> a, "MatchingSide" -> 1, 
         "Position" -> {2}|>|>, {"SubstitutionLemma", 11} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] b == b \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 10}, 
         "Construct" -> {"SubstitutionLemma", 3}, "Position" -> {}, 
         "Rule" -> ((b_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
           a \[CenterDot] (b \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] b == 
            b \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
               b))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 12} -> 
      <|"Statement" -> HoldForm[b \[CenterDot] a == a \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] a))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 10}, 
         "Construct" -> {"SubstitutionLemma", 11}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] 
             ((b_) \[CenterDot] (a_))) -> b \[CenterDot] a, 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[b \[CenterDot] a == 
            a \[CenterDot] (a \[CenterDot] (b \[CenterDot] a))], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 11} -> 
      <|"Statement" -> HoldForm[(b \[CenterDot] b) \[CenterDot] 
           (a \[CenterDot] b) == (a \[CenterDot] b) \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] b)], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 12}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
             ((b_) \[CenterDot] (a_))) -> b \[CenterDot] a, "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] (a_), "MatchingConstruct" -> 
          {"Axiom", 2}, "MatchingOrientation" -> 1, "MatchingRule" -> 
          ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] (a_)) -> 
           a, "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
     {"SubstitutionLemma", 13} -> <|"Statement" -> 
        HoldForm[(b \[CenterDot] b) \[CenterDot] (a \[CenterDot] b) == 
          (a \[CenterDot] b) \[CenterDot] (b \[CenterDot] (a \[CenterDot] 
             b))], "Proof" -> <|"Input" -> {"CriticalPairLemma", 11}, 
         "Construct" -> {"SubstitutionLemma", 4}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           (b \[CenterDot] b) \[CenterDot] (a \[CenterDot] b) == 
            (a \[CenterDot] b) \[CenterDot] (b \[CenterDot] (a \[CenterDot] 
               b))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 14} -> 
      <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
           (b \[CenterDot] (a \[CenterDot] b))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 13}, 
         "Construct" -> {"Axiom", 2}, "Position" -> {}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
             (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[b == (a \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
              (a \[CenterDot] b))], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 12} -> <|"Statement" -> 
        HoldForm[b == (a \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
            (b \[CenterDot] a))], "Proof" -> 
        <|"Construct" -> {"SubstitutionLemma", 14}, "Orientation" -> -1, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
             ((a_) \[CenterDot] (b_))) -> b, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 4}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
     {"CriticalPairLemma", 13} -> <|"Statement" -> 
        HoldForm[((a \[CenterDot] b) \[CenterDot] b) \[CenterDot] a == 
          a \[CenterDot] a], "Proof" -> <|"Construct" -> {"Axiom", 1}, 
         "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             ((a_) \[CenterDot] (c_))) -> ((c \[CenterDot] b) \[CenterDot] 
             b) \[CenterDot] a, "Side" -> 1, "Subpattern" -> {}, 
         "MatchingConstruct" -> {"CriticalPairLemma", 1}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          (a_) \[CenterDot] ((b_) \[CenterDot] ((a_) \[CenterDot] (a_))) -> 
           a \[CenterDot] a, "MatchingSide" -> 1, "Position" -> {}|>|>, 
     {"SubstitutionLemma", 15} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] b) == 
          a \[CenterDot] a], "Proof" -> <|"Input" -> {"CriticalPairLemma", 
           13}, "Construct" -> {"SubstitutionLemma", 4}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] b) == 
            a \[CenterDot] a], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 16} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (b \[CenterDot] (a \[CenterDot] b)) == 
          a \[CenterDot] a], "Proof" -> <|"Input" -> {"SubstitutionLemma", 
           15}, "Construct" -> {"SubstitutionLemma", 4}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           a \[CenterDot] (b \[CenterDot] (a \[CenterDot] b)) == 
            a \[CenterDot] a], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 14} -> <|"Statement" -> 
        HoldForm[(((b \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
            b) \[CenterDot] a == a \[CenterDot] (b \[CenterDot] b)], 
       "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] ((a_) \[CenterDot] 
              (c_))) -> ((c \[CenterDot] b) \[CenterDot] b) \[CenterDot] a, 
         "Side" -> 1, "Subpattern" -> (b_) \[CenterDot] ((a_) \[CenterDot] 
            (c_)), "MatchingConstruct" -> {"SubstitutionLemma", 16}, 
         "MatchingOrientation" -> 1, "MatchingRule" -> 
          (a_) \[CenterDot] ((b_) \[CenterDot] ((a_) \[CenterDot] (b_))) -> 
           a \[CenterDot] a, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"SubstitutionLemma", 17} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (((b \[CenterDot] a) \[CenterDot] 
             b) \[CenterDot] b) == a \[CenterDot] (b \[CenterDot] b)], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 14}, 
         "Construct" -> {"SubstitutionLemma", 4}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           a \[CenterDot] (((b \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
              b) == a \[CenterDot] (b \[CenterDot] b)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 18} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
            ((b \[CenterDot] a) \[CenterDot] b)) == a \[CenterDot] 
           (b \[CenterDot] b)], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 17}, "Construct" -> 
          {"SubstitutionLemma", 4}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b)) == a \[CenterDot] (b \[CenterDot] b)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 19} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
            (b \[CenterDot] (b \[CenterDot] a))) == a \[CenterDot] 
           (b \[CenterDot] b)], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 18}, "Construct" -> 
          {"SubstitutionLemma", 4}, "Position" -> {2, 2}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           a \[CenterDot] (b \[CenterDot] (b \[CenterDot] (b \[CenterDot] 
                a))) == a \[CenterDot] (b \[CenterDot] b)], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 15} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] b) == 
          a \[CenterDot] (b \[CenterDot] (b \[CenterDot] (a \[CenterDot] 
              b)))], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 19}, 
         "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             ((b_) \[CenterDot] ((b_) \[CenterDot] (a_)))) -> 
           a \[CenterDot] (b \[CenterDot] b), "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] (a_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 4}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {2, 2, 2}|>|>, 
     {"SubstitutionLemma", 20} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (b \[CenterDot] b) == a \[CenterDot] 
           (a \[CenterDot] b)], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 15}, "Construct" -> 
          {"SubstitutionLemma", 12}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] 
              (a_))) -> b \[CenterDot] a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] (b \[CenterDot] b) == 
            a \[CenterDot] (a \[CenterDot] b)], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 16} -> <|"Statement" -> 
        HoldForm[b == (a \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
            (a \[CenterDot] a))], "Proof" -> 
        <|"Construct" -> {"CriticalPairLemma", 12}, "Orientation" -> -1, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
             ((b_) \[CenterDot] (a_))) -> b, "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] ((b_) \[CenterDot] (a_)), 
         "MatchingConstruct" -> {"SubstitutionLemma", 20}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          (a_) \[CenterDot] ((a_) \[CenterDot] (b_)) -> a \[CenterDot] 
            (b \[CenterDot] b), "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"CriticalPairLemma", 17} -> <|"Statement" -> 
        HoldForm[b \[CenterDot] (a \[CenterDot] b) == 
          (a \[CenterDot] a) \[CenterDot] ((b \[CenterDot] (a \[CenterDot] 
              b)) \[CenterDot] (a \[CenterDot] a))], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 16}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((b_) \[CenterDot] ((a_) \[CenterDot] (a_))) -> b, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 16}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             ((a_) \[CenterDot] (b_))) -> a \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {1}|>|>, 
     {"CriticalPairLemma", 18} -> <|"Statement" -> 
        HoldForm[(a \[CenterDot] a) \[CenterDot] (b \[CenterDot] b) == 
          (a \[CenterDot] a) \[CenterDot] (b \[CenterDot] (a \[CenterDot] 
             b))], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 9}, 
         "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (b_))) -> 
           a \[CenterDot] (b \[CenterDot] b), "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (a_), "MatchingConstruct" -> 
          {"Axiom", 2}, "MatchingOrientation" -> 1, "MatchingRule" -> 
          ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] (a_)) -> 
           a, "MatchingSide" -> 1, "Position" -> {2, 2, 1}|>|>, 
     {"CriticalPairLemma", 19} -> <|"Statement" -> 
        HoldForm[(b \[CenterDot] b) \[CenterDot] (a \[CenterDot] a) == 
          (a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
           (b \[CenterDot] b)], "Proof" -> 
        <|"Construct" -> {"CriticalPairLemma", 18}, "Orientation" -> -1, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
             ((a_) \[CenterDot] (b_))) -> (a \[CenterDot] a) \[CenterDot] 
            (b \[CenterDot] b), "Side" -> 1, "Subpattern" -> {}, 
         "MatchingConstruct" -> {"SubstitutionLemma", 4}, 
         "MatchingOrientation" -> 1, "MatchingRule" -> 
          (a_) \[CenterDot] (b_) -> b \[CenterDot] a, "MatchingSide" -> 1, 
         "Position" -> {}|>|>, {"SubstitutionLemma", 21} -> 
      <|"Statement" -> HoldForm[b \[CenterDot] (a \[CenterDot] b) == 
          (a \[CenterDot] a) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
            (b \[CenterDot] b))], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 17}, "Construct" -> 
          {"CriticalPairLemma", 19}, "Position" -> {2}, 
         "Rule" -> ((a_) \[CenterDot] ((b_) \[CenterDot] (a_))) \[CenterDot] 
            ((b_) \[CenterDot] (b_)) -> (b \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] a), "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[b \[CenterDot] (a \[CenterDot] b) == 
            (a \[CenterDot] a) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              (b \[CenterDot] b))], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 20} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b)) == a \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] b))], "Proof" -> 
        <|"Construct" -> {"SubstitutionLemma", 20}, "Orientation" -> -1, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] (b_)) -> 
           a \[CenterDot] (b \[CenterDot] b), "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 20}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> (a_) \[CenterDot] ((a_) \[CenterDot] (b_)) -> 
           a \[CenterDot] (b \[CenterDot] b), "MatchingSide" -> 1, 
         "Position" -> {2}|>|>, {"SubstitutionLemma", 22} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
            (a \[CenterDot] b)) == a \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] b))], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 20}, "Construct" -> 
          {"SubstitutionLemma", 20}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (b_)) -> 
           a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
              (a \[CenterDot] b)) == a \[CenterDot] (a \[CenterDot] 
              (b \[CenterDot] b))], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 21} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] a == ((a \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
            b)], "Proof" -> <|"Construct" -> {"Axiom", 2}, 
         "Orientation" -> 1, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((b_) \[CenterDot] (a_)) -> a, "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] (a_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 3}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (b_)) -> 
           (b \[CenterDot] b) \[CenterDot] a, "MatchingSide" -> 1, 
         "Position" -> {2}|>|>, {"SubstitutionLemma", 23} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] a == a \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] b)], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 21}, 
         "Construct" -> {"Axiom", 2}, "Position" -> {1}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
             (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[a \[CenterDot] a == a \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] b)], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 22} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] ((((a \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] b) \[CenterDot] 
            (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
               a)) \[CenterDot] b)) == a \[CenterDot] 
           ((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
               a)) \[CenterDot] b) \[CenterDot] ((a \[CenterDot] 
              a) \[CenterDot] (a \[CenterDot] a)))], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 9}, 
         "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (b_))) -> 
           a \[CenterDot] (b \[CenterDot] b), "Side" -> 1, 
         "Subpattern" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (b_), 
         "MatchingConstruct" -> {"SubstitutionLemma", 23}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] (b_)) -> 
           a \[CenterDot] a, "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
     {"SubstitutionLemma", 24} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] ((((a \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] b) \[CenterDot] 
            (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
               a)) \[CenterDot] b)) == a \[CenterDot] 
           (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
            (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
               a)) \[CenterDot] b))], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 22}, "Construct" -> 
          {"SubstitutionLemma", 3}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (b_)) -> 
           (b \[CenterDot] b) \[CenterDot] a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] 
             ((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] b) \[CenterDot] (((a \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] b)) == 
            a \[CenterDot] (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                a)) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a)) \[CenterDot] b))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 25} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] 
           ((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
               a)) \[CenterDot] b) \[CenterDot] 
            (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
               a)) \[CenterDot] b)) == a \[CenterDot] (a \[CenterDot] 
            (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
               a)) \[CenterDot] b))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 24}, "Construct" -> {"Axiom", 2}, 
         "Position" -> {2, 1}, "Rule" -> 
          ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] (a_)) -> 
           a, "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 
          1, "Side" -> 2, "OutputExpression" -> HoldForm[
           a \[CenterDot] ((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] b) \[CenterDot] (((a \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] b)) == 
            a \[CenterDot] (a \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a)) \[CenterDot] b))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 26} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] 
           ((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
               a)) \[CenterDot] b) \[CenterDot] 
            (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
               a)) \[CenterDot] b)) == a \[CenterDot] (a \[CenterDot] 
            (a \[CenterDot] b))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 25}, "Construct" -> {"Axiom", 2}, 
         "Position" -> {2, 2, 1}, "Rule" -> 
          ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] (a_)) -> 
           a, "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 
          1, "Side" -> 2, "OutputExpression" -> HoldForm[
           a \[CenterDot] ((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] b) \[CenterDot] (((a \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] b)) == 
            a \[CenterDot] (a \[CenterDot] (a \[CenterDot] b))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 27} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] ((a \[CenterDot] 
             b) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] b)) == a \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] b))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 26}, 
         "Construct" -> {"Axiom", 2}, "Position" -> {2, 1, 1}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
             (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] b)) == a \[CenterDot] (a \[CenterDot] 
              (a \[CenterDot] b))], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 28} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b)) == a \[CenterDot] (a \[CenterDot] 
            (a \[CenterDot] b))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 27}, "Construct" -> {"Axiom", 2}, 
         "Position" -> {2, 2, 1}, "Rule" -> 
          ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] (a_)) -> 
           a, "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 
          1, "Side" -> 1, "OutputExpression" -> HoldForm[
           a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
               b)) == a \[CenterDot] (a \[CenterDot] (a \[CenterDot] b))], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 23} -> 
      <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
           (a \[CenterDot] b)], "Proof" -> <|"Construct" -> {"Axiom", 2}, 
         "Orientation" -> 1, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((b_) \[CenterDot] (a_)) -> a, "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] (a_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 4}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"CriticalPairLemma", 24} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] b == ((a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b)) \[CenterDot] a], 
       "Proof" -> <|"Construct" -> {"Axiom", 2}, "Orientation" -> 1, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
             (a_)) -> a, "Side" -> 1, "Subpattern" -> (b_) \[CenterDot] (a_), 
         "MatchingConstruct" -> {"CriticalPairLemma", 23}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] (b_)) -> 
           a, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"SubstitutionLemma", 29} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] b == a \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 24}, 
         "Construct" -> {"SubstitutionLemma", 3}, "Position" -> {}, 
         "Rule" -> ((b_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
           a \[CenterDot] (b \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] b == 
            a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
               b))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 30} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] b == a \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] b))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 28}, 
         "Construct" -> {"SubstitutionLemma", 29}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             ((a_) \[CenterDot] (b_))) -> a \[CenterDot] b, 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[a \[CenterDot] b == 
            a \[CenterDot] (a \[CenterDot] (a \[CenterDot] b))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 31} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] b == a \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] b))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 22}, 
         "Construct" -> {"SubstitutionLemma", 30}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
              (b_))) -> a \[CenterDot] b, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[a \[CenterDot] b == 
            a \[CenterDot] (a \[CenterDot] (b \[CenterDot] b))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 32} -> 
      <|"Statement" -> HoldForm[b \[CenterDot] (a \[CenterDot] b) == 
          (a \[CenterDot] a) \[CenterDot] b], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 21}, "Construct" -> 
          {"SubstitutionLemma", 31}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] 
              (b_))) -> a \[CenterDot] b, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[b \[CenterDot] (a \[CenterDot] b) == 
            (a \[CenterDot] a) \[CenterDot] b], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 25} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (b \[CenterDot] a) == a \[CenterDot] 
           (b \[CenterDot] b)], "Proof" -> 
        <|"Construct" -> {"SubstitutionLemma", 32}, "Orientation" -> -1, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (b_) -> 
           b \[CenterDot] (a \[CenterDot] b), "Side" -> 1, 
         "Subpattern" -> {}, "MatchingConstruct" -> {"SubstitutionLemma", 4}, 
         "MatchingOrientation" -> 1, "MatchingRule" -> 
          (a_) \[CenterDot] (b_) -> b \[CenterDot] a, "MatchingSide" -> 1, 
         "Position" -> {}|>|>, {"CriticalPairLemma", 26} -> 
      <|"Statement" -> HoldForm[(((c \[CenterDot] c) \[CenterDot] 
             b) \[CenterDot] b) \[CenterDot] a == a \[CenterDot] 
           (b \[CenterDot] (a \[CenterDot] (c \[CenterDot] a)))], 
       "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] ((a_) \[CenterDot] 
              (c_))) -> ((c \[CenterDot] b) \[CenterDot] b) \[CenterDot] a, 
         "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] (c_), 
         "MatchingConstruct" -> {"CriticalPairLemma", 25}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          (a_) \[CenterDot] ((b_) \[CenterDot] (b_)) -> a \[CenterDot] 
            (b \[CenterDot] a), "MatchingSide" -> 1, "Position" -> {2, 
          2}|>|>, {"SubstitutionLemma", 33} -> 
      <|"Statement" -> HoldForm[
         (b \[CenterDot] ((c \[CenterDot] c) \[CenterDot] b)) \[CenterDot] 
           a == a \[CenterDot] (b \[CenterDot] (a \[CenterDot] 
             (c \[CenterDot] a)))], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 26}, "Construct" -> 
          {"SubstitutionLemma", 4}, "Position" -> {1}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           (b \[CenterDot] ((c \[CenterDot] c) \[CenterDot] b)) \[CenterDot] 
             a == a \[CenterDot] (b \[CenterDot] (a \[CenterDot] (
                c \[CenterDot] a)))], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 27} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
            (b \[CenterDot] b)) == a \[CenterDot] 
           ((b \[CenterDot] b) \[CenterDot] a)], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 20}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
             (b_)) -> a \[CenterDot] (b \[CenterDot] b), "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 3}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (b_)) -> 
           (b \[CenterDot] b) \[CenterDot] a, "MatchingSide" -> 1, 
         "Position" -> {2}|>|>, {"SubstitutionLemma", 34} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] b == a \[CenterDot] 
           ((b \[CenterDot] b) \[CenterDot] a)], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 27}, 
         "Construct" -> {"Axiom", 2}, "Position" -> {2}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
             (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[a \[CenterDot] b == a \[CenterDot] 
             ((b \[CenterDot] b) \[CenterDot] a)], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 35} -> <|"Statement" -> 
        HoldForm[(b \[CenterDot] c) \[CenterDot] a == a \[CenterDot] 
           (b \[CenterDot] (a \[CenterDot] (c \[CenterDot] a)))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 33}, 
         "Construct" -> {"SubstitutionLemma", 34}, "Position" -> {1}, 
         "Rule" -> (a_) \[CenterDot] (((b_) \[CenterDot] (b_)) \[CenterDot] 
             (a_)) -> a \[CenterDot] b, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[(b \[CenterDot] c) \[CenterDot] a == 
            a \[CenterDot] (b \[CenterDot] (a \[CenterDot] (c \[CenterDot] 
                a)))], "Source" -> "norm"|>|>, {"CriticalPairLemma", 28} -> 
      <|"Statement" -> HoldForm[(c \[CenterDot] a) \[CenterDot] b == 
          (((a \[CenterDot] b) \[CenterDot] c) \[CenterDot] c) \[CenterDot] 
           b], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 35}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             ((a_) \[CenterDot] ((c_) \[CenterDot] (a_)))) -> 
           (b \[CenterDot] c) \[CenterDot] a, "Side" -> 1, 
         "Subpattern" -> {}, "MatchingConstruct" -> {"Axiom", 1}, 
         "MatchingOrientation" -> 1, "MatchingRule" -> 
          (a_) \[CenterDot] ((b_) \[CenterDot] ((a_) \[CenterDot] (c_))) -> 
           ((c \[CenterDot] b) \[CenterDot] b) \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {}|>|>, 
     {"SubstitutionLemma", 36} -> <|"Statement" -> 
        HoldForm[(c \[CenterDot] a) \[CenterDot] b == b \[CenterDot] 
           (((a \[CenterDot] b) \[CenterDot] c) \[CenterDot] c)], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 28}, 
         "Construct" -> {"SubstitutionLemma", 4}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           (c \[CenterDot] a) \[CenterDot] b == b \[CenterDot] 
             (((a \[CenterDot] b) \[CenterDot] c) \[CenterDot] c)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 37} -> 
      <|"Statement" -> HoldForm[(c \[CenterDot] a) \[CenterDot] b == 
          b \[CenterDot] (c \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             c))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 36}, 
         "Construct" -> {"SubstitutionLemma", 4}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           (c \[CenterDot] a) \[CenterDot] b == b \[CenterDot] 
             (c \[CenterDot] ((a \[CenterDot] b) \[CenterDot] c))], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 29} -> 
      <|"Statement" -> HoldForm[(((b \[CenterDot] a) \[CenterDot] 
             c) \[CenterDot] b) \[CenterDot] a == a \[CenterDot] 
           (((b \[CenterDot] a) \[CenterDot] c) \[CenterDot] 
            ((b \[CenterDot] a) \[CenterDot] (c \[CenterDot] c)))], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 37}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             (((c_) \[CenterDot] (a_)) \[CenterDot] (b_))) -> 
           (b \[CenterDot] c) \[CenterDot] a, "Side" -> 1, 
         "Subpattern" -> ((c_) \[CenterDot] (a_)) \[CenterDot] (b_), 
         "MatchingConstruct" -> {"SubstitutionLemma", 20}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          (a_) \[CenterDot] ((a_) \[CenterDot] (b_)) -> a \[CenterDot] 
            (b \[CenterDot] b), "MatchingSide" -> 1, "Position" -> {2, 
          2}|>|>, {"CriticalPairLemma", 30} -> 
      <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
           (a \[CenterDot] b) == (a \[CenterDot] b) \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] a)], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 12}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
             ((b_) \[CenterDot] (a_))) -> b \[CenterDot] a, "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] (a_), "MatchingConstruct" -> 
          {"CriticalPairLemma", 23}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] (b_)) -> a, "MatchingSide" -> 1, 
         "Position" -> {2, 2}|>|>, {"SubstitutionLemma", 38} -> 
      <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
           (a \[CenterDot] b) == (a \[CenterDot] b) \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] b))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 30}, 
         "Construct" -> {"SubstitutionLemma", 4}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           (a \[CenterDot] a) \[CenterDot] (a \[CenterDot] b) == 
            (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
               b))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 39} -> 
      <|"Statement" -> HoldForm[a == (a \[CenterDot] b) \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] b))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 38}, 
         "Construct" -> {"CriticalPairLemma", 23}, "Position" -> {}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
             (b_)) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[a == (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
              (a \[CenterDot] b))], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 31} -> <|"Statement" -> 
        HoldForm[a == (a \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] (a \[CenterDot] b)))], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 39}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((a_) \[CenterDot] ((a_) \[CenterDot] (b_))) -> a, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 20}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> (a_) \[CenterDot] ((a_) \[CenterDot] (b_)) -> 
           a \[CenterDot] (b \[CenterDot] b), "MatchingSide" -> 1, 
         "Position" -> {1}|>|>, {"SubstitutionLemma", 40} -> 
      <|"Statement" -> HoldForm[a == (a \[CenterDot] (b \[CenterDot] 
             b)) \[CenterDot] (a \[CenterDot] b)], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 31}, 
         "Construct" -> {"SubstitutionLemma", 30}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
              (b_))) -> a \[CenterDot] b, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a == (a \[CenterDot] 
              (b \[CenterDot] b)) \[CenterDot] (a \[CenterDot] b)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 41} -> 
      <|"Statement" -> HoldForm[a == (a \[CenterDot] b) \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] b))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 40}, 
         "Construct" -> {"SubstitutionLemma", 4}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           a == (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
              (b \[CenterDot] b))], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 42} -> <|"Statement" -> 
        HoldForm[(((b \[CenterDot] a) \[CenterDot] c) \[CenterDot] 
            b) \[CenterDot] a == a \[CenterDot] (b \[CenterDot] a)], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 29}, 
         "Construct" -> {"SubstitutionLemma", 41}, "Position" -> {2}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
             ((b_) \[CenterDot] (b_))) -> a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[(((b \[CenterDot] a) \[CenterDot] 
               c) \[CenterDot] b) \[CenterDot] a == a \[CenterDot] 
             (b \[CenterDot] a)], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 43} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (((b \[CenterDot] a) \[CenterDot] 
             c) \[CenterDot] b) == a \[CenterDot] (b \[CenterDot] a)], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 42}, 
         "Construct" -> {"SubstitutionLemma", 4}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           a \[CenterDot] (((b \[CenterDot] a) \[CenterDot] c) \[CenterDot] 
              b) == a \[CenterDot] (b \[CenterDot] a)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 44} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
            ((b \[CenterDot] a) \[CenterDot] c)) == a \[CenterDot] 
           (b \[CenterDot] a)], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 43}, "Construct" -> 
          {"SubstitutionLemma", 4}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               c)) == a \[CenterDot] (b \[CenterDot] a)], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 32} -> 
      <|"Statement" -> HoldForm[(c \[CenterDot] b) \[CenterDot] a == 
          a \[CenterDot] ((a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
            c)], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 35}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             ((a_) \[CenterDot] ((c_) \[CenterDot] (a_)))) -> 
           (b \[CenterDot] c) \[CenterDot] a, "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] ((a_) \[CenterDot] 
            ((c_) \[CenterDot] (a_))), "MatchingConstruct" -> 
          {"SubstitutionLemma", 4}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"CriticalPairLemma", 33} -> <|"Statement" -> 
        HoldForm[(a \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
            (a \[CenterDot] b)) == (a \[CenterDot] b) \[CenterDot] 
           ((c \[CenterDot] a) \[CenterDot] b)], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 44}, 
         "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             (((b_) \[CenterDot] (a_)) \[CenterDot] (c_))) -> 
           a \[CenterDot] (b \[CenterDot] a), "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] (((b_) \[CenterDot] 
             (a_)) \[CenterDot] (c_)), "MatchingConstruct" -> 
          {"CriticalPairLemma", 32}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> (a_) \[CenterDot] (((a_) \[CenterDot] 
              ((b_) \[CenterDot] (a_))) \[CenterDot] (c_)) -> 
           (c \[CenterDot] b) \[CenterDot] a, "MatchingSide" -> 1, 
         "Position" -> {2}|>|>, {"SubstitutionLemma", 45} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] a) == 
          a \[CenterDot] (b \[CenterDot] b)], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 9}, "Construct" -> 
          {"SubstitutionLemma", 34}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] (((b_) \[CenterDot] (b_)) \[CenterDot] 
             (a_)) -> a \[CenterDot] b, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[a \[CenterDot] (b \[CenterDot] a) == 
            a \[CenterDot] (b \[CenterDot] b)], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 46} -> <|"Statement" -> 
        HoldForm[(a \[CenterDot] b) \[CenterDot] (b \[CenterDot] b) == 
          (a \[CenterDot] b) \[CenterDot] ((c \[CenterDot] a) \[CenterDot] 
            b)], "Proof" -> <|"Input" -> {"CriticalPairLemma", 33}, 
         "Construct" -> {"SubstitutionLemma", 45}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (a_)) -> 
           a \[CenterDot] (b \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
             (b \[CenterDot] b) == (a \[CenterDot] b) \[CenterDot] 
             ((c \[CenterDot] a) \[CenterDot] b)], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 47} -> <|"Statement" -> 
        HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
           ((c \[CenterDot] a) \[CenterDot] b)], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 46}, 
         "Construct" -> {"CriticalPairLemma", 5}, "Position" -> {}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
             (b_)) -> b, "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
             ((c \[CenterDot] a) \[CenterDot] b)], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 34} -> <|"Statement" -> 
        HoldForm[a == (a \[CenterDot] b) \[CenterDot] 
           ((c \[CenterDot] b) \[CenterDot] a)], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 47}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            (((c_) \[CenterDot] (a_)) \[CenterDot] (b_)) -> b, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 4}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {1}|>|>, 
     {"CriticalPairLemma", 35} -> <|"Statement" -> 
        HoldForm[a == (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
            (c \[CenterDot] b))], "Proof" -> 
        <|"Construct" -> {"CriticalPairLemma", 34}, "Orientation" -> -1, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            (((c_) \[CenterDot] (b_)) \[CenterDot] (a_)) -> a, "Side" -> 1, 
         "Subpattern" -> ((c_) \[CenterDot] (b_)) \[CenterDot] (a_), 
         "MatchingConstruct" -> {"SubstitutionLemma", 4}, 
         "MatchingOrientation" -> 1, "MatchingRule" -> 
          (a_) \[CenterDot] (b_) -> b \[CenterDot] a, "MatchingSide" -> 1, 
         "Position" -> {2}|>|>, {"CriticalPairLemma", 36} -> 
      <|"Statement" -> HoldForm[a == (a \[CenterDot] b) \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] c))], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 35}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((a_) \[CenterDot] ((c_) \[CenterDot] (b_))) -> a, "Side" -> 1, 
         "Subpattern" -> (c_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 4}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
     {"Conclusion", 1} -> <|"Statement" -> HoldForm[a == a], 
       "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Construct" -> 
          {"CriticalPairLemma", 36}, "Position" -> {}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
             ((b_) \[CenterDot] (c_))) -> a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[a == a], "Source" -> "cpl"|>|>}|>], 
 ProofObject["EquationalLogic", Inactive[Equal][a \[CenterDot] b, 
   b \[CenterDot] a], {Inactive[Equal][(a_) \[CenterDot] 
     ((b_) \[CenterDot] ((a_) \[CenterDot] (c_))), 
    (((c_) \[CenterDot] (b_)) \[CenterDot] (b_)) \[CenterDot] (a_)], 
   Inactive[Equal][((a_) \[CenterDot] (a_)) \[CenterDot] 
     ((b_) \[CenterDot] (a_)), a_]}, <|"Variables" -> {a, b, c}, 
   "Constants" -> {}, "Proof" -> 
    {{"Axiom", 1} -> <|"Statement" -> HoldForm[
         a \[CenterDot] (b \[CenterDot] (a \[CenterDot] c)) == 
          ((c \[CenterDot] b) \[CenterDot] b) \[CenterDot] a], 
       "Proof" -> <||>|>, {"Axiom", 2} -> 
      <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
           (b \[CenterDot] a) == a], "Proof" -> <||>|>, 
     {"Hypothesis", 1} -> <|"Statement" -> HoldForm[a \[CenterDot] b == 
          b \[CenterDot] a], "Proof" -> <||>|>, {"CriticalPairLemma", 1} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] a == a \[CenterDot] 
           (b \[CenterDot] (a \[CenterDot] a))], 
       "Proof" -> <|"Construct" -> {"Axiom", 2}, "Orientation" -> 1, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
             (a_)) -> a, "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] (a_), 
         "MatchingConstruct" -> {"Axiom", 2}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((b_) \[CenterDot] (a_)) -> a, "MatchingSide" -> 1, 
         "Position" -> {1}|>|>, {"CriticalPairLemma", 2} -> 
      <|"Statement" -> HoldForm[(((b \[CenterDot] b) \[CenterDot] 
             b) \[CenterDot] b) \[CenterDot] a == a \[CenterDot] 
           (b \[CenterDot] b)], "Proof" -> <|"Construct" -> {"Axiom", 1}, 
         "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             ((a_) \[CenterDot] (c_))) -> ((c \[CenterDot] b) \[CenterDot] 
             b) \[CenterDot] a, "Side" -> 1, "Subpattern" -> 
          (b_) \[CenterDot] ((a_) \[CenterDot] (c_)), "MatchingConstruct" -> 
          {"CriticalPairLemma", 1}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             ((a_) \[CenterDot] (a_))) -> a \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"SubstitutionLemma", 1} -> <|"Statement" -> 
        HoldForm[(b \[CenterDot] (b \[CenterDot] (b \[CenterDot] 
              b))) \[CenterDot] a == a \[CenterDot] (b \[CenterDot] b)], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 2}, 
         "Construct" -> {"Axiom", 1}, "Position" -> {1}, 
         "Rule" -> (((c_) \[CenterDot] (b_)) \[CenterDot] (b_)) \[CenterDot] 
            (a_) -> a \[CenterDot] (b \[CenterDot] (a \[CenterDot] c)), 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           (b \[CenterDot] (b \[CenterDot] (b \[CenterDot] b))) \[CenterDot] 
             a == a \[CenterDot] (b \[CenterDot] b)], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 2} -> <|"Statement" -> 
        HoldForm[(b \[CenterDot] b) \[CenterDot] a == a \[CenterDot] 
           (b \[CenterDot] b)], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 1}, "Construct" -> 
          {"CriticalPairLemma", 1}, "Position" -> {1}, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] ((a_) \[CenterDot] 
              (a_))) -> a \[CenterDot] a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[(b \[CenterDot] b) \[CenterDot] a == 
            a \[CenterDot] (b \[CenterDot] b)], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 3} -> <|"Statement" -> 
        HoldForm[((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
             b)) \[CenterDot] a == a \[CenterDot] b], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 2}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             (b_)) -> (b \[CenterDot] b) \[CenterDot] a, "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"Axiom", 2}, "MatchingOrientation" -> 1, "MatchingRule" -> 
          ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] (a_)) -> 
           a, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"SubstitutionLemma", 3} -> <|"Statement" -> 
        HoldForm[b \[CenterDot] a == a \[CenterDot] b], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 3}, 
         "Construct" -> {"Axiom", 2}, "Position" -> {1}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
             (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[b \[CenterDot] a == a \[CenterDot] b], 
         "Source" -> "norm"|>|>, {"Conclusion", 1} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] b == a \[CenterDot] b], 
       "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Construct" -> 
          {"SubstitutionLemma", 3}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[a \[CenterDot] b == 
            a \[CenterDot] b], "Source" -> "cpl"|>|>}|>]}
