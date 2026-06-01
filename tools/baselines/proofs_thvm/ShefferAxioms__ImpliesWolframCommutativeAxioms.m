{ProofObject["EquationalLogic", Inactive[Equal][
   (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] (b \[CenterDot] c)), a], 
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
     {"Hypothesis", 1} -> <|"Statement" -> HoldForm[
         (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] (b \[CenterDot] 
             c)) == a], "Proof" -> <||>|>, {"CriticalPairLemma", 1} -> 
      <|"Statement" -> HoldForm[((b \[CenterDot] b) \[CenterDot] 
            a) \[CenterDot] (((b \[CenterDot] b) \[CenterDot] 
             (b \[CenterDot] b)) \[CenterDot] a) == 
          (a \[CenterDot] a) \[CenterDot] (a \[CenterDot] (b \[CenterDot] 
             (b \[CenterDot] b)))], "Proof" -> <|"Construct" -> {"Axiom", 3}, 
         "Orientation" -> 1, "Rule" -> ((a_) \[CenterDot] ((b_) \[CenterDot] 
              (c_))) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] 
              (c_))) -> ((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            ((c \[CenterDot] c) \[CenterDot] a), "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] ((b_) \[CenterDot] (c_)), 
         "MatchingConstruct" -> {"Axiom", 2}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             ((b_) \[CenterDot] (b_))) -> a \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {1}|>|>, 
     {"SubstitutionLemma", 1} -> <|"Statement" -> 
        HoldForm[((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
           (b \[CenterDot] a) == (a \[CenterDot] a) \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b)))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 1}, 
         "Construct" -> {"Axiom", 1}, "Position" -> {2, 1}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
             (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
             (b \[CenterDot] a) == (a \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b)))], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 2} -> 
      <|"Statement" -> HoldForm[((b \[CenterDot] b) \[CenterDot] 
            a) \[CenterDot] (b \[CenterDot] a) == 
          (a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a)], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 1}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] ((b_) \[CenterDot] ((b_) \[CenterDot] (
                b_)))) -> ((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            (b \[CenterDot] a), "Side" -> 1, "Subpattern" -> 
          (a_) \[CenterDot] ((b_) \[CenterDot] ((b_) \[CenterDot] (b_))), 
         "MatchingConstruct" -> {"Axiom", 2}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             ((b_) \[CenterDot] (b_))) -> a \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"SubstitutionLemma", 2} -> <|"Statement" -> 
        HoldForm[((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
           (b \[CenterDot] a) == a], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 2}, "Construct" -> {"Axiom", 1}, 
         "Position" -> {}, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] (a_)) -> a, "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[((b \[CenterDot] b) \[CenterDot] 
              a) \[CenterDot] (b \[CenterDot] a) == a], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 3} -> 
      <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] b)], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 2}, 
         "Orientation" -> 1, "Rule" -> (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (b_)) \[CenterDot] ((a_) \[CenterDot] (b_)) -> b, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (a_), "MatchingConstruct" -> 
          {"Axiom", 1}, "MatchingOrientation" -> 1, "MatchingRule" -> 
          ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] (a_)) -> 
           a, "MatchingSide" -> 1, "Position" -> {1, 1}|>|>, 
     {"CriticalPairLemma", 4} -> <|"Statement" -> 
        HoldForm[(((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
              b)) \[CenterDot] a) \[CenterDot] 
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
          ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] (a_)) -> 
           a, "MatchingSide" -> 1, "Position" -> {1, 2}|>|>, 
     {"SubstitutionLemma", 3} -> <|"Statement" -> 
        HoldForm[(((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
              b)) \[CenterDot] a) \[CenterDot] 
           (((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
            a) == (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 4}, 
         "Construct" -> {"Axiom", 1}, "Position" -> {2, 2}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
             (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[(((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
                b)) \[CenterDot] a) \[CenterDot] (((b \[CenterDot] 
                b) \[CenterDot] (b \[CenterDot] b)) \[CenterDot] a) == 
            (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 4} -> 
      <|"Statement" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
           (((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
            a) == (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 3}, 
         "Construct" -> {"Axiom", 1}, "Position" -> {1, 1}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
             (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[(b \[CenterDot] a) \[CenterDot] (((b \[CenterDot] 
                b) \[CenterDot] (b \[CenterDot] b)) \[CenterDot] a) == 
            (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 5} -> 
      <|"Statement" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
           (b \[CenterDot] a) == (a \[CenterDot] b) \[CenterDot] 
           (a \[CenterDot] b)], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 4}, "Construct" -> {"Axiom", 1}, 
         "Position" -> {2, 1}, "Rule" -> 
          ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] (a_)) -> 
           a, "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 
          1, "Side" -> 1, "OutputExpression" -> HoldForm[
           (b \[CenterDot] a) \[CenterDot] (b \[CenterDot] a) == 
            (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 5} -> 
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
     {"CriticalPairLemma", 6} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (a \[CenterDot] a) == 
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
        <|"Input" -> {"CriticalPairLemma", 6}, "Construct" -> {"Axiom", 2}, 
         "Position" -> {2}, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             ((b_) \[CenterDot] (b_))) -> a \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           a \[CenterDot] (a \[CenterDot] a) == (a \[CenterDot] 
              a) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] a))], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 7} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (a \[CenterDot] a) == 
          (a \[CenterDot] a) \[CenterDot] a], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 6}, "Construct" -> {"Axiom", 1}, 
         "Position" -> {2}, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] (a_)) -> a, "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] (a \[CenterDot] a) == 
            (a \[CenterDot] a) \[CenterDot] a], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 8} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] b == ((a \[CenterDot] b) \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
              b))) \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
            (b \[CenterDot] a))], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 5}, "Construct" -> 
          {"SubstitutionLemma", 7}, "Position" -> {1}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (a_) -> 
           a \[CenterDot] (a \[CenterDot] a), "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] b == 
            ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
               (a \[CenterDot] b))) \[CenterDot] ((b \[CenterDot] 
               a) \[CenterDot] (b \[CenterDot] a))], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 7} -> <|"Statement" -> 
        HoldForm[b \[CenterDot] (b \[CenterDot] b) == 
          (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b))) \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a))], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 3}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            (((a_) \[CenterDot] (a_)) \[CenterDot] (b_)) -> b, "Side" -> 1, 
         "Subpattern" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (b_), 
         "MatchingConstruct" -> {"Axiom", 2}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             ((b_) \[CenterDot] (b_))) -> a \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"SubstitutionLemma", 9} -> <|"Statement" -> 
        HoldForm[b \[CenterDot] (b \[CenterDot] b) == 
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
             a)) \[CenterDot] (((b \[CenterDot] (a \[CenterDot] (
                a \[CenterDot] a))) \[CenterDot] (b \[CenterDot] 
              (a \[CenterDot] (a \[CenterDot] a)))) \[CenterDot] b)], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 3}, 
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
              b) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] (
                a \[CenterDot] a)) \[CenterDot] b)) \[CenterDot] b)], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 8}, 
         "Construct" -> {"Axiom", 3}, "Position" -> {2, 1}, 
         "Rule" -> ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) \[CenterDot] 
            ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) -> 
           ((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            ((c \[CenterDot] c) \[CenterDot] a), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[b == (a \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((((a \[CenterDot] 
                 a) \[CenterDot] b) \[CenterDot] (((a \[CenterDot] 
                  a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
                b)) \[CenterDot] b)], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 11} -> <|"Statement" -> 
        HoldForm[b == (a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
           (b \[CenterDot] b)], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 10}, "Construct" -> 
          {"CriticalPairLemma", 3}, "Position" -> {2, 1}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            (((a_) \[CenterDot] (a_)) \[CenterDot] (b_)) -> b, 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           b == (a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
             (b \[CenterDot] b)], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 12} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] b == b \[CenterDot] a], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 8}, 
         "Construct" -> {"SubstitutionLemma", 11}, "Position" -> {}, 
         "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] (a_))) \[CenterDot] 
            ((b_) \[CenterDot] (b_)) -> b, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] b == 
            b \[CenterDot] a], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 9} -> <|"Statement" -> 
        HoldForm[b == (a \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
            (a \[CenterDot] a))], "Proof" -> 
        <|"Construct" -> {"CriticalPairLemma", 3}, "Orientation" -> -1, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
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
     {"CriticalPairLemma", 11} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (b \[CenterDot] b) == a \[CenterDot] 
           (((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            ((b \[CenterDot] a) \[CenterDot] (b \[CenterDot] a)))], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 10}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] 
            (((a_) \[CenterDot] ((b_) \[CenterDot] (b_))) \[CenterDot] 
             (((b_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] (
                a_)))) -> a \[CenterDot] (b \[CenterDot] b), "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] ((b_) \[CenterDot] (b_)), 
         "MatchingConstruct" -> {"SubstitutionLemma", 12}, 
         "MatchingOrientation" -> 1, "MatchingRule" -> 
          (a_) \[CenterDot] (b_) -> b \[CenterDot] a, "MatchingSide" -> 1, 
         "Position" -> {2, 1}|>|>, {"CriticalPairLemma", 12} -> 
      <|"Statement" -> HoldForm[(b \[CenterDot] b) \[CenterDot] a == 
          a \[CenterDot] (((b \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
              a)) \[CenterDot] ((b \[CenterDot] b) \[CenterDot] a))], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 3}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            (((a_) \[CenterDot] (a_)) \[CenterDot] (b_)) -> b, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"CriticalPairLemma", 3}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            (((a_) \[CenterDot] (a_)) \[CenterDot] (b_)) -> b, 
         "MatchingSide" -> 1, "Position" -> {1}|>|>, 
     {"SubstitutionLemma", 13} -> <|"Statement" -> 
        HoldForm[(b \[CenterDot] b) \[CenterDot] a == a \[CenterDot] 
           (((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            ((b \[CenterDot] a) \[CenterDot] (b \[CenterDot] a)))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 12}, 
         "Construct" -> {"SubstitutionLemma", 12}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           (b \[CenterDot] b) \[CenterDot] a == a \[CenterDot] 
             (((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
              ((b \[CenterDot] a) \[CenterDot] (b \[CenterDot] a)))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 14} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] b) == 
          (b \[CenterDot] b) \[CenterDot] a], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 11}, "Construct" -> 
          {"SubstitutionLemma", 13}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((((b_) \[CenterDot] (b_)) \[CenterDot] 
              (a_)) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] 
              ((b_) \[CenterDot] (a_)))) -> (b \[CenterDot] b) \[CenterDot] 
            a, "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[a \[CenterDot] (b \[CenterDot] b) == 
            (b \[CenterDot] b) \[CenterDot] a], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 15} -> <|"Statement" -> 
        HoldForm[a == (a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] (b \[CenterDot] b)))], 
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
            ((a_) \[CenterDot] ((b_) \[CenterDot] ((b_) \[CenterDot] (
                b_)))) -> a, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"SubstitutionLemma", 16} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b)) == 
          (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] 
               b)))) \[CenterDot] a], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 13}, "Construct" -> {"Axiom", 1}, 
         "Position" -> {1, 1}, "Rule" -> 
          ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] (a_)) -> 
           a, "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 
          1, "Side" -> 2, "OutputExpression" -> HoldForm[
           a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b)) == 
            (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] 
                 b)))) \[CenterDot] a], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 14} -> <|"Statement" -> 
        HoldForm[b \[CenterDot] (b \[CenterDot] b) == 
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
     {"SubstitutionLemma", 17} -> <|"Statement" -> 
        HoldForm[b \[CenterDot] (b \[CenterDot] b) == a \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b)))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 14}, 
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
              ((b_) \[CenterDot] (b_)))) -> b \[CenterDot] 
            (b \[CenterDot] b), "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b)) == 
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
          ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] (a_)) -> 
           a, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"CriticalPairLemma", 16} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b)) == 
          a \[CenterDot] a], "Proof" -> <|"Construct" -> 
          {"SubstitutionLemma", 18}, "Orientation" -> -1, 
         "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] (a_))) \[CenterDot] 
            (b_) -> b \[CenterDot] (a \[CenterDot] (a \[CenterDot] a)), 
         "Side" -> 1, "Subpattern" -> {}, "MatchingConstruct" -> 
          {"CriticalPairLemma", 15}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] 
              (a_))) \[CenterDot] (b_) -> b \[CenterDot] b, 
         "MatchingSide" -> 1, "Position" -> {}|>|>, 
     {"CriticalPairLemma", 17} -> <|"Statement" -> 
        HoldForm[c \[CenterDot] (a \[CenterDot] a) == 
          (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b))) \[CenterDot] 
           c], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 14}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            (b_) -> b \[CenterDot] (a \[CenterDot] a), "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (a_), "MatchingConstruct" -> 
          {"CriticalPairLemma", 16}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> (a_) \[CenterDot] (a_) -> a \[CenterDot] 
            (b \[CenterDot] (b \[CenterDot] b)), "MatchingSide" -> 1, 
         "Position" -> {1}|>|>, {"CriticalPairLemma", 18} -> 
      <|"Statement" -> HoldForm[
         ((b \[CenterDot] (b \[CenterDot] b)) \[CenterDot] a) \[CenterDot] 
           ((b \[CenterDot] (b \[CenterDot] b)) \[CenterDot] a) == 
          (a \[CenterDot] a) \[CenterDot] (a \[CenterDot] (b \[CenterDot] 
             (b \[CenterDot] b)))], "Proof" -> 
        <|"Construct" -> {"SubstitutionLemma", 5}, "Orientation" -> 1, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
             (b_)) -> (b \[CenterDot] a) \[CenterDot] (b \[CenterDot] a), 
         "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] (b_), 
         "MatchingConstruct" -> {"Axiom", 2}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             ((b_) \[CenterDot] (b_))) -> a \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {1}|>|>, 
     {"SubstitutionLemma", 19} -> <|"Statement" -> 
        HoldForm[((b \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
            a) \[CenterDot] ((b \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
            a) == a], "Proof" -> <|"Input" -> {"CriticalPairLemma", 18}, 
         "Construct" -> {"SubstitutionLemma", 15}, "Position" -> {}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
             ((b_) \[CenterDot] ((b_) \[CenterDot] (b_)))) -> a, 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           ((b \[CenterDot] (b \[CenterDot] b)) \[CenterDot] a) \[CenterDot] 
             ((b \[CenterDot] (b \[CenterDot] b)) \[CenterDot] a) == a], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 19} -> 
      <|"Statement" -> HoldForm[(((b \[CenterDot] b) \[CenterDot] 
             (b \[CenterDot] b)) \[CenterDot] (b \[CenterDot] 
             a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
            (b \[CenterDot] a)) == a \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
             a))], "Proof" -> <|"Construct" -> {"Axiom", 3}, 
         "Orientation" -> 1, "Rule" -> ((a_) \[CenterDot] ((b_) \[CenterDot] 
              (c_))) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] 
              (c_))) -> ((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            ((c \[CenterDot] c) \[CenterDot] a), "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] ((b_) \[CenterDot] (c_)), 
         "MatchingConstruct" -> {"CriticalPairLemma", 3}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          ((a_) \[CenterDot] (b_)) \[CenterDot] (((a_) \[CenterDot] 
              (a_)) \[CenterDot] (b_)) -> b, "MatchingSide" -> 1, 
         "Position" -> {1}|>|>, {"SubstitutionLemma", 20} -> 
      <|"Statement" -> HoldForm[(((b \[CenterDot] b) \[CenterDot] 
             (b \[CenterDot] b)) \[CenterDot] (b \[CenterDot] 
             a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
            (b \[CenterDot] a)) == a \[CenterDot] a], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 19}, 
         "Construct" -> {"CriticalPairLemma", 3}, "Position" -> {2}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            (((a_) \[CenterDot] (a_)) \[CenterDot] (b_)) -> b, 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           (((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
              (b \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] (b \[CenterDot] a)) == a \[CenterDot] a], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 21} -> 
      <|"Statement" -> HoldForm[(b \[CenterDot] (b \[CenterDot] 
             a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
            (b \[CenterDot] a)) == a \[CenterDot] a], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 20}, 
         "Construct" -> {"Axiom", 1}, "Position" -> {1, 1}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
             (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[(b \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] (b \[CenterDot] a)) == 
            a \[CenterDot] a], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 20} -> <|"Statement" -> 
        HoldForm[(b \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
           (b \[CenterDot] (a \[CenterDot] a)) == 
          ((a \[CenterDot] b) \[CenterDot] b) \[CenterDot] 
           (((b \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
             (b \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
              (a \[CenterDot] a))))], "Proof" -> 
        <|"Construct" -> {"SubstitutionLemma", 21}, "Orientation" -> 1, 
         "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] (b_))) \[CenterDot] 
            (((b_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
              (b_))) -> b \[CenterDot] b, "Side" -> 1, "Subpattern" -> 
          (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"CriticalPairLemma", 9}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((b_) \[CenterDot] ((a_) \[CenterDot] (a_))) -> b, 
         "MatchingSide" -> 1, "Position" -> {1, 2}|>|>, 
     {"SubstitutionLemma", 22} -> <|"Statement" -> 
        HoldForm[(b \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
           (b \[CenterDot] (a \[CenterDot] a)) == 
          (b \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
           (((b \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
             (b \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
              (a \[CenterDot] a))))], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 20}, "Construct" -> 
          {"SubstitutionLemma", 12}, "Position" -> {1}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           (b \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
             (b \[CenterDot] (a \[CenterDot] a)) == 
            (b \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
             (((b \[CenterDot] (a \[CenterDot] a)) \[CenterDot] (
                b \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
              ((a \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
                (a \[CenterDot] a))))], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 23} -> <|"Statement" -> 
        HoldForm[(b \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
           (b \[CenterDot] (a \[CenterDot] a)) == 
          (b \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
           (((a \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
              (a \[CenterDot] a))) \[CenterDot] ((b \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] (b \[CenterDot] 
              (a \[CenterDot] a))))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 22}, "Construct" -> 
          {"SubstitutionLemma", 12}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           (b \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
             (b \[CenterDot] (a \[CenterDot] a)) == 
            (b \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
             (((a \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
                (a \[CenterDot] a))) \[CenterDot] ((b \[CenterDot] 
                (a \[CenterDot] a)) \[CenterDot] (b \[CenterDot] 
                (a \[CenterDot] a))))], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 24} -> <|"Statement" -> 
        HoldForm[(b \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
           (b \[CenterDot] (a \[CenterDot] a)) == 
          (b \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
           (b \[CenterDot] ((b \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
             (b \[CenterDot] (a \[CenterDot] a))))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 23}, 
         "Construct" -> {"CriticalPairLemma", 9}, "Position" -> {2, 1}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
             ((a_) \[CenterDot] (a_))) -> b, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[(b \[CenterDot] (a \[CenterDot] 
               a)) \[CenterDot] (b \[CenterDot] (a \[CenterDot] a)) == 
            (b \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
             (b \[CenterDot] ((b \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] (b \[CenterDot] (a \[CenterDot] a))))], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 21} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] ((a \[CenterDot] 
             (a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
             (a \[CenterDot] a))) == ((a \[CenterDot] (a \[CenterDot] 
              a)) \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
              a))) \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
              a)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] (
                a \[CenterDot] a)) \[CenterDot] (a \[CenterDot] (
                a \[CenterDot] a)))))], "Proof" -> 
        <|"Construct" -> {"SubstitutionLemma", 19}, "Orientation" -> 1, 
         "Rule" -> (((a_) \[CenterDot] ((a_) \[CenterDot] (a_))) \[CenterDot] 
             (b_)) \[CenterDot] (((a_) \[CenterDot] ((a_) \[CenterDot] (
                a_))) \[CenterDot] (b_)) -> b, "Side" -> 1, 
         "Subpattern" -> ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (a_))) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 24}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> ((a_) \[CenterDot] ((b_) \[CenterDot] 
              (a_))) \[CenterDot] ((a_) \[CenterDot] (((a_) \[CenterDot] (
                (b_) \[CenterDot] (b_))) \[CenterDot] ((a_) \[CenterDot] (
                (b_) \[CenterDot] (b_))))) -> (a \[CenterDot] 
             (b \[CenterDot] b)) \[CenterDot] (a \[CenterDot] 
             (b \[CenterDot] b)), "MatchingSide" -> 1, "Position" -> {1}|>|>, 
     {"SubstitutionLemma", 25} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
              a)) \[CenterDot] (a \[CenterDot] (a \[CenterDot] a))) == 
          ((a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
           ((a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] a)))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 21}, 
         "Construct" -> {"SubstitutionLemma", 24}, "Position" -> {2}, 
         "Rule" -> ((a_) \[CenterDot] ((b_) \[CenterDot] (a_))) \[CenterDot] 
            ((a_) \[CenterDot] (((a_) \[CenterDot] ((b_) \[CenterDot] 
                (b_))) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] 
                (b_))))) -> (a \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
            (a \[CenterDot] (b \[CenterDot] b)), "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] 
             ((a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
              (a \[CenterDot] (a \[CenterDot] a))) == 
            ((a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
              (a \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
             ((a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
              (a \[CenterDot] (a \[CenterDot] a)))], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 26} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
              a)) \[CenterDot] (a \[CenterDot] (a \[CenterDot] a))) == 
          a \[CenterDot] (a \[CenterDot] a)], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 25}, "Construct" -> {"Axiom", 1}, 
         "Position" -> {}, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] (a_)) -> a, "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] 
             ((a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
              (a \[CenterDot] (a \[CenterDot] a))) == a \[CenterDot] 
             (a \[CenterDot] a)], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 22} -> <|"Statement" -> 
        HoldForm[b \[CenterDot] (b \[CenterDot] b) == a \[CenterDot] 
           (a \[CenterDot] a)], "Proof" -> 
        <|"Construct" -> {"SubstitutionLemma", 17}, "Orientation" -> -1, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] 
              ((b_) \[CenterDot] (b_)))) -> b \[CenterDot] 
            (b \[CenterDot] b), "Side" -> 1, "Subpattern" -> 
          (a_) \[CenterDot] ((b_) \[CenterDot] ((b_) \[CenterDot] (b_))), 
         "MatchingConstruct" -> {"Axiom", 2}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             ((b_) \[CenterDot] (b_))) -> a \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"CriticalPairLemma", 23} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (a \[CenterDot] a) == a \[CenterDot] 
           ((b \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] a)))], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 26}, 
         "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] 
            (((a_) \[CenterDot] ((a_) \[CenterDot] (a_))) \[CenterDot] 
             ((a_) \[CenterDot] ((a_) \[CenterDot] (a_)))) -> 
           a \[CenterDot] (a \[CenterDot] a), "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] ((a_) \[CenterDot] (a_)), 
         "MatchingConstruct" -> {"CriticalPairLemma", 22}, 
         "MatchingOrientation" -> 1, "MatchingRule" -> 
          (a_) \[CenterDot] ((a_) \[CenterDot] (a_)) -> b \[CenterDot] 
            (b \[CenterDot] b), "MatchingSide" -> 1, "Position" -> {2, 
          1}|>|>, {"CriticalPairLemma", 24} -> 
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
          {"CriticalPairLemma", 23}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> (a_) \[CenterDot] (((b_) \[CenterDot] 
              ((b_) \[CenterDot] (b_))) \[CenterDot] ((a_) \[CenterDot] 
              ((a_) \[CenterDot] (a_)))) -> a \[CenterDot] 
            (a \[CenterDot] a), "MatchingSide" -> 1, "Position" -> {1, 
          2}|>|>, {"SubstitutionLemma", 27} -> 
      <|"Statement" -> HoldForm[((b \[CenterDot] b) \[CenterDot] 
            a) \[CenterDot] ((((c \[CenterDot] (c \[CenterDot] 
                c)) \[CenterDot] (b \[CenterDot] (b \[CenterDot] 
                b))) \[CenterDot] ((c \[CenterDot] (c \[CenterDot] 
                c)) \[CenterDot] (b \[CenterDot] (b \[CenterDot] 
                b)))) \[CenterDot] a) == (a \[CenterDot] (b \[CenterDot] 
             (b \[CenterDot] b))) \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] (b \[CenterDot] b)))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 24}, 
         "Construct" -> {"CriticalPairLemma", 23}, "Position" -> {2, 2}, 
         "Rule" -> (a_) \[CenterDot] (((b_) \[CenterDot] ((b_) \[CenterDot] (
                b_))) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] (
                a_)))) -> a \[CenterDot] (a \[CenterDot] a), 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           ((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
             ((((c \[CenterDot] (c \[CenterDot] c)) \[CenterDot] 
                (b \[CenterDot] (b \[CenterDot] b))) \[CenterDot] (
                (c \[CenterDot] (c \[CenterDot] c)) \[CenterDot] 
                (b \[CenterDot] (b \[CenterDot] b)))) \[CenterDot] a) == 
            (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b))) \[CenterDot] 
             (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b)))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 28} -> 
      <|"Statement" -> HoldForm[((b \[CenterDot] b) \[CenterDot] 
            a) \[CenterDot] ((((c \[CenterDot] (c \[CenterDot] 
                c)) \[CenterDot] (b \[CenterDot] (b \[CenterDot] 
                b))) \[CenterDot] ((c \[CenterDot] (c \[CenterDot] 
                c)) \[CenterDot] (b \[CenterDot] (b \[CenterDot] 
                b)))) \[CenterDot] a) == ((b \[CenterDot] b) \[CenterDot] 
            a) \[CenterDot] (((b \[CenterDot] b) \[CenterDot] 
             (b \[CenterDot] b)) \[CenterDot] a)], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 27}, 
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
              a) \[CenterDot] (((b \[CenterDot] b) \[CenterDot] (
                b \[CenterDot] b)) \[CenterDot] a)], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 29} -> <|"Statement" -> 
        HoldForm[((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
           ((((c \[CenterDot] (c \[CenterDot] c)) \[CenterDot] 
              (b \[CenterDot] (b \[CenterDot] b))) \[CenterDot] 
             ((c \[CenterDot] (c \[CenterDot] c)) \[CenterDot] 
              (b \[CenterDot] (b \[CenterDot] b)))) \[CenterDot] a) == a], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 28}, 
         "Construct" -> {"CriticalPairLemma", 3}, "Position" -> {}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            (((a_) \[CenterDot] (a_)) \[CenterDot] (b_)) -> b, 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           ((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
             ((((c \[CenterDot] (c \[CenterDot] c)) \[CenterDot] 
                (b \[CenterDot] (b \[CenterDot] b))) \[CenterDot] (
                (c \[CenterDot] (c \[CenterDot] c)) \[CenterDot] 
                (b \[CenterDot] (b \[CenterDot] b)))) \[CenterDot] a) == a], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 30} -> 
      <|"Statement" -> HoldForm[((b \[CenterDot] b) \[CenterDot] 
            a) \[CenterDot] ((((b \[CenterDot] b) \[CenterDot] 
              (c \[CenterDot] (c \[CenterDot] c))) \[CenterDot] 
             (((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
                b)) \[CenterDot] (c \[CenterDot] (c \[CenterDot] 
                c)))) \[CenterDot] a) == a], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 29}, "Construct" -> {"Axiom", 3}, 
         "Position" -> {2, 1}, "Rule" -> 
          ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) \[CenterDot] 
            ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) -> 
           ((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            ((c \[CenterDot] c) \[CenterDot] a), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[((b \[CenterDot] b) \[CenterDot] 
              a) \[CenterDot] ((((b \[CenterDot] b) \[CenterDot] 
                (c \[CenterDot] (c \[CenterDot] c))) \[CenterDot] (
                ((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
                  b)) \[CenterDot] (c \[CenterDot] (c \[CenterDot] 
                  c)))) \[CenterDot] a) == a], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 31} -> <|"Statement" -> 
        HoldForm[((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
           ((c \[CenterDot] (c \[CenterDot] c)) \[CenterDot] a) == a], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 30}, 
         "Construct" -> {"CriticalPairLemma", 3}, "Position" -> {2, 1}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            (((a_) \[CenterDot] (a_)) \[CenterDot] (b_)) -> b, 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           ((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
             ((c \[CenterDot] (c \[CenterDot] c)) \[CenterDot] a) == a], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 25} -> 
      <|"Statement" -> HoldForm[(b \[CenterDot] b) \[CenterDot] 
           (b \[CenterDot] b) == ((a \[CenterDot] a) \[CenterDot] 
            ((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
              b))) \[CenterDot] (b \[CenterDot] b)], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 31}, 
         "Orientation" -> 1, "Rule" -> (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (b_)) \[CenterDot] (((c_) \[CenterDot] ((c_) \[CenterDot] (
                c_))) \[CenterDot] (b_)) -> b, "Side" -> 1, 
         "Subpattern" -> ((c_) \[CenterDot] ((c_) \[CenterDot] 
             (c_))) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"CriticalPairLemma", 3}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            (((a_) \[CenterDot] (a_)) \[CenterDot] (b_)) -> b, 
         "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"SubstitutionLemma", 32} -> <|"Statement" -> 
        HoldForm[(b \[CenterDot] b) \[CenterDot] (b \[CenterDot] b) == 
          (b \[CenterDot] b) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
            ((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] b)))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 25}, 
         "Construct" -> {"SubstitutionLemma", 12}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           (b \[CenterDot] b) \[CenterDot] (b \[CenterDot] b) == 
            (b \[CenterDot] b) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              ((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] b)))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 33} -> 
      <|"Statement" -> HoldForm[(b \[CenterDot] b) \[CenterDot] 
           (b \[CenterDot] b) == (b \[CenterDot] b) \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] b)], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 32}, 
         "Construct" -> {"Axiom", 1}, "Position" -> {2, 2}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
             (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[(b \[CenterDot] b) \[CenterDot] (b \[CenterDot] b) == 
            (b \[CenterDot] b) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              b)], "Source" -> "norm"|>|>, {"SubstitutionLemma", 34} -> 
      <|"Statement" -> HoldForm[b == (b \[CenterDot] b) \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] b)], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 33}, 
         "Construct" -> {"Axiom", 1}, "Position" -> {}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
             (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[b == (b \[CenterDot] b) \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] b)], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 26} -> <|"Statement" -> 
        HoldForm[a == (a \[CenterDot] a) \[CenterDot] (b \[CenterDot] a)], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 34}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            (((b_) \[CenterDot] (b_)) \[CenterDot] (a_)) -> a, "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"Axiom", 1}, "MatchingOrientation" -> 1, "MatchingRule" -> 
          ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] (a_)) -> 
           a, "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
     {"CriticalPairLemma", 27} -> <|"Statement" -> 
        HoldForm[a == (a \[CenterDot] a) \[CenterDot] (a \[CenterDot] b)], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 26}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((b_) \[CenterDot] (a_)) -> a, "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] (a_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 12}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"CriticalPairLemma", 28} -> <|"Statement" -> 
        HoldForm[(b \[CenterDot] a) \[CenterDot] (b \[CenterDot] a) == 
          (a \[CenterDot] b) \[CenterDot] (b \[CenterDot] a)], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 5}, 
         "Orientation" -> 1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((a_) \[CenterDot] (b_)) -> (b \[CenterDot] a) \[CenterDot] 
            (b \[CenterDot] a), "Side" -> 1, "Subpattern" -> 
          (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 12}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"CriticalPairLemma", 29} -> <|"Statement" -> 
        HoldForm[c == ((a \[CenterDot] b) \[CenterDot] c) \[CenterDot] 
           (((b \[CenterDot] a) \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
            c)], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 3}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            (((a_) \[CenterDot] (a_)) \[CenterDot] (b_)) -> b, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (a_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 5}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((a_) \[CenterDot] (b_)) -> (b \[CenterDot] a) \[CenterDot] 
            (b \[CenterDot] a), "MatchingSide" -> 1, "Position" -> {2, 
          1}|>|>, {"CriticalPairLemma", 30} -> 
      <|"Statement" -> HoldForm[((b \[CenterDot] a) \[CenterDot] 
            (b \[CenterDot] a)) \[CenterDot] (c \[CenterDot] 
            (c \[CenterDot] c)) == ((a \[CenterDot] b) \[CenterDot] 
            (((b \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
               a)) \[CenterDot] (c \[CenterDot] (c \[CenterDot] 
               c)))) \[CenterDot] (c \[CenterDot] (c \[CenterDot] c))], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 29}, 
         "Orientation" -> -1, "Rule" -> 
          (((a_) \[CenterDot] (b_)) \[CenterDot] (c_)) \[CenterDot] 
            ((((b_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] (
                a_))) \[CenterDot] (c_)) -> c, "Side" -> 1, 
         "Subpattern" -> (((b_) \[CenterDot] (a_)) \[CenterDot] 
            ((b_) \[CenterDot] (a_))) \[CenterDot] (c_), 
         "MatchingConstruct" -> {"SubstitutionLemma", 17}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          (a_) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] 
              ((b_) \[CenterDot] (b_)))) -> b \[CenterDot] 
            (b \[CenterDot] b), "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"SubstitutionLemma", 35} -> <|"Statement" -> 
        HoldForm[((b \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
             a)) \[CenterDot] (c \[CenterDot] (c \[CenterDot] c)) == 
          (c \[CenterDot] (c \[CenterDot] c)) \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] (((b \[CenterDot] a) \[CenterDot] 
              (b \[CenterDot] a)) \[CenterDot] (c \[CenterDot] 
              (c \[CenterDot] c))))], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 30}, "Construct" -> 
          {"SubstitutionLemma", 18}, "Position" -> {}, 
         "Rule" -> (b_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
              (a_))) -> (a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] b, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           ((b \[CenterDot] a) \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
             (c \[CenterDot] (c \[CenterDot] c)) == 
            (c \[CenterDot] (c \[CenterDot] c)) \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] (((b \[CenterDot] 
                 a) \[CenterDot] (b \[CenterDot] a)) \[CenterDot] (
                c \[CenterDot] (c \[CenterDot] c))))], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 31} -> 
      <|"Statement" -> HoldForm[
         a == (((a \[CenterDot] (b \[CenterDot] (b \[CenterDot] 
                b))) \[CenterDot] (a \[CenterDot] (b \[CenterDot] (
                b \[CenterDot] b)))) \[CenterDot] a) \[CenterDot] 
           (b \[CenterDot] (b \[CenterDot] b))], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 2}, 
         "Orientation" -> 1, "Rule" -> (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (b_)) \[CenterDot] ((a_) \[CenterDot] (b_)) -> b, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 9}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> ((a_) \[CenterDot] ((b_) \[CenterDot] 
              ((b_) \[CenterDot] (b_)))) \[CenterDot] (a_) -> 
           b \[CenterDot] (b \[CenterDot] b), "MatchingSide" -> 1, 
         "Position" -> {2}|>|>, {"SubstitutionLemma", 36} -> 
      <|"Statement" -> HoldForm[a == ((((b \[CenterDot] b) \[CenterDot] 
              a) \[CenterDot] (((b \[CenterDot] b) \[CenterDot] (
                b \[CenterDot] b)) \[CenterDot] a)) \[CenterDot] 
            a) \[CenterDot] (b \[CenterDot] (b \[CenterDot] b))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 31}, 
         "Construct" -> {"Axiom", 3}, "Position" -> {1, 1}, 
         "Rule" -> ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) \[CenterDot] 
            ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) -> 
           ((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            ((c \[CenterDot] c) \[CenterDot] a), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[
           a == ((((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] (
                ((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
                  b)) \[CenterDot] a)) \[CenterDot] a) \[CenterDot] 
             (b \[CenterDot] (b \[CenterDot] b))], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 37} -> <|"Statement" -> 
        HoldForm[a == (a \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
            (b \[CenterDot] b))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 36}, "Construct" -> 
          {"CriticalPairLemma", 3}, "Position" -> {1, 1}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            (((a_) \[CenterDot] (a_)) \[CenterDot] (b_)) -> b, 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           a == (a \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
              (b \[CenterDot] b))], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 38} -> <|"Statement" -> 
        HoldForm[((b \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
             a)) \[CenterDot] (c \[CenterDot] (c \[CenterDot] c)) == 
          (c \[CenterDot] (c \[CenterDot] c)) \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] (b \[CenterDot] a))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 35}, 
         "Construct" -> {"SubstitutionLemma", 37}, "Position" -> {2, 2}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
             ((b_) \[CenterDot] (b_))) -> a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[((b \[CenterDot] a) \[CenterDot] 
              (b \[CenterDot] a)) \[CenterDot] (c \[CenterDot] 
              (c \[CenterDot] c)) == (c \[CenterDot] (c \[CenterDot] 
               c)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              (b \[CenterDot] a))], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 39} -> <|"Statement" -> 
        HoldForm[b \[CenterDot] a == (c \[CenterDot] (c \[CenterDot] 
             c)) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            (b \[CenterDot] a))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 38}, "Construct" -> 
          {"SubstitutionLemma", 37}, "Position" -> {}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
             ((b_) \[CenterDot] (b_))) -> a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[b \[CenterDot] a == 
            (c \[CenterDot] (c \[CenterDot] c)) \[CenterDot] 
             ((a \[CenterDot] b) \[CenterDot] (b \[CenterDot] a))], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 32} -> 
      <|"Statement" -> HoldForm[(((b \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] b)) \[CenterDot] (c \[CenterDot] 
             (c \[CenterDot] c))) \[CenterDot] 
           (((b \[CenterDot] a) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
            (c \[CenterDot] (c \[CenterDot] c))) == 
          (a \[CenterDot] b) \[CenterDot] (((b \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] b)) \[CenterDot] (c \[CenterDot] 
             (c \[CenterDot] c)))], "Proof" -> 
        <|"Construct" -> {"CriticalPairLemma", 28}, "Orientation" -> -1, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
             (a_)) -> (b \[CenterDot] a) \[CenterDot] (b \[CenterDot] a), 
         "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] (b_), 
         "MatchingConstruct" -> {"SubstitutionLemma", 39}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          ((a_) \[CenterDot] ((a_) \[CenterDot] (a_))) \[CenterDot] 
            (((b_) \[CenterDot] (c_)) \[CenterDot] ((c_) \[CenterDot] 
              (b_))) -> c \[CenterDot] b, "MatchingSide" -> 1, 
         "Position" -> {1}|>|>, {"CriticalPairLemma", 33} -> 
      <|"Statement" -> HoldForm[b \[CenterDot] a == 
          ((a \[CenterDot] b) \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
           (c \[CenterDot] (c \[CenterDot] c))], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 39}, 
         "Orientation" -> -1, "Rule" -> 
          ((a_) \[CenterDot] ((a_) \[CenterDot] (a_))) \[CenterDot] 
            (((b_) \[CenterDot] (c_)) \[CenterDot] ((c_) \[CenterDot] 
              (b_))) -> c \[CenterDot] b, "Side" -> 1, "Subpattern" -> {}, 
         "MatchingConstruct" -> {"SubstitutionLemma", 18}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          ((a_) \[CenterDot] ((a_) \[CenterDot] (a_))) \[CenterDot] (b_) -> 
           b \[CenterDot] (a \[CenterDot] (a \[CenterDot] a)), 
         "MatchingSide" -> 1, "Position" -> {}|>|>, 
     {"SubstitutionLemma", 40} -> <|"Statement" -> 
        HoldForm[(((b \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
              b)) \[CenterDot] (c \[CenterDot] (c \[CenterDot] 
              c))) \[CenterDot] (((b \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] b)) \[CenterDot] (c \[CenterDot] 
             (c \[CenterDot] c))) == (a \[CenterDot] b) \[CenterDot] 
           (a \[CenterDot] b)], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 32}, "Construct" -> 
          {"CriticalPairLemma", 33}, "Position" -> {2}, 
         "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
              (a_))) \[CenterDot] ((c_) \[CenterDot] ((c_) \[CenterDot] 
              (c_))) -> b \[CenterDot] a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[(((b \[CenterDot] a) \[CenterDot] (
                a \[CenterDot] b)) \[CenterDot] (c \[CenterDot] (
                c \[CenterDot] c))) \[CenterDot] (((b \[CenterDot] 
                a) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
              (c \[CenterDot] (c \[CenterDot] c))) == 
            (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 41} -> 
      <|"Statement" -> HoldForm[((c \[CenterDot] c) \[CenterDot] 
            ((b \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
              b))) \[CenterDot] (((c \[CenterDot] c) \[CenterDot] 
             (c \[CenterDot] c)) \[CenterDot] ((b \[CenterDot] 
              a) \[CenterDot] (a \[CenterDot] b))) == 
          (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 40}, 
         "Construct" -> {"Axiom", 3}, "Position" -> {}, 
         "Rule" -> ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) \[CenterDot] 
            ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) -> 
           ((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            ((c \[CenterDot] c) \[CenterDot] a), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[((c \[CenterDot] c) \[CenterDot] 
              ((b \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                b))) \[CenterDot] (((c \[CenterDot] c) \[CenterDot] (
                c \[CenterDot] c)) \[CenterDot] ((b \[CenterDot] 
                a) \[CenterDot] (a \[CenterDot] b))) == 
            (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 42} -> 
      <|"Statement" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
           (a \[CenterDot] b) == (a \[CenterDot] b) \[CenterDot] 
           (a \[CenterDot] b)], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 41}, "Construct" -> 
          {"CriticalPairLemma", 3}, "Position" -> {}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            (((a_) \[CenterDot] (a_)) \[CenterDot] (b_)) -> b, 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           (b \[CenterDot] a) \[CenterDot] (a \[CenterDot] b) == 
            (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 34} -> 
      <|"Statement" -> HoldForm[b \[CenterDot] a == 
          ((a \[CenterDot] b) \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] c)], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 27}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] (b_)) -> a, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (a_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 42}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((a_) \[CenterDot] (b_)) -> (b \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] b), "MatchingSide" -> 1, "Position" -> {1}|>|>, 
     {"CriticalPairLemma", 35} -> <|"Statement" -> 
        HoldForm[c \[CenterDot] (((b \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] b)) \[CenterDot] ((b \[CenterDot] 
              a) \[CenterDot] (a \[CenterDot] b))) == 
          (a \[CenterDot] b) \[CenterDot] c], "Proof" -> 
        <|"Construct" -> {"CriticalPairLemma", 17}, "Orientation" -> -1, 
         "Rule" -> ((a_) \[CenterDot] ((b_) \[CenterDot] ((b_) \[CenterDot] (
                b_)))) \[CenterDot] (c_) -> c \[CenterDot] 
            (a \[CenterDot] a), "Side" -> 1, "Subpattern" -> 
          (a_) \[CenterDot] ((b_) \[CenterDot] ((b_) \[CenterDot] (b_))), 
         "MatchingConstruct" -> {"CriticalPairLemma", 34}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          (((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
              (a_))) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] 
             (c_)) -> b \[CenterDot] a, "MatchingSide" -> 1, 
         "Position" -> {1}|>|>, {"CriticalPairLemma", 36} -> 
      <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
           (b \[CenterDot] a) == (a \[CenterDot] b) \[CenterDot] 
           (a \[CenterDot] b)], "Proof" -> 
        <|"Construct" -> {"SubstitutionLemma", 42}, "Orientation" -> -1, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
             (b_)) -> (b \[CenterDot] a) \[CenterDot] (a \[CenterDot] b), 
         "Side" -> 1, "Subpattern" -> {}, "MatchingConstruct" -> 
          {"SubstitutionLemma", 5}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((a_) \[CenterDot] (b_)) -> (b \[CenterDot] a) \[CenterDot] 
            (b \[CenterDot] a), "MatchingSide" -> 1, "Position" -> {}|>|>, 
     {"CriticalPairLemma", 37} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] b == ((a \[CenterDot] b) \[CenterDot] 
            (b \[CenterDot] a)) \[CenterDot] (c \[CenterDot] 
            (a \[CenterDot] b))], "Proof" -> 
        <|"Construct" -> {"CriticalPairLemma", 26}, "Orientation" -> -1, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
             (a_)) -> a, "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] (a_), 
         "MatchingConstruct" -> {"CriticalPairLemma", 36}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          ((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] (b_)) -> 
           (a \[CenterDot] b) \[CenterDot] (b \[CenterDot] a), 
         "MatchingSide" -> 1, "Position" -> {1}|>|>, 
     {"CriticalPairLemma", 38} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] b == ((a \[CenterDot] b) \[CenterDot] 
            (b \[CenterDot] a)) \[CenterDot] (c \[CenterDot] 
            (b \[CenterDot] a))], "Proof" -> 
        <|"Construct" -> {"CriticalPairLemma", 37}, "Orientation" -> -1, 
         "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
              (a_))) \[CenterDot] ((c_) \[CenterDot] ((a_) \[CenterDot] 
              (b_))) -> a \[CenterDot] b, "Side" -> 1, "Subpattern" -> 
          (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 12}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
     {"SubstitutionLemma", 43} -> <|"Statement" -> 
        HoldForm[c \[CenterDot] (b \[CenterDot] a) == 
          (a \[CenterDot] b) \[CenterDot] c], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 35}, "Construct" -> 
          {"CriticalPairLemma", 38}, "Position" -> {2}, 
         "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
              (a_))) \[CenterDot] ((c_) \[CenterDot] ((b_) \[CenterDot] 
              (a_))) -> a \[CenterDot] b, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[c \[CenterDot] (b \[CenterDot] a) == 
            (a \[CenterDot] b) \[CenterDot] c], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 39} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] b == a \[CenterDot] 
           (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] b))], "Proof" -> 
        <|"Construct" -> {"CriticalPairLemma", 3}, "Orientation" -> -1, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            (((a_) \[CenterDot] (a_)) \[CenterDot] (b_)) -> b, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"CriticalPairLemma", 27}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] (b_)) -> a, "MatchingSide" -> 1, 
         "Position" -> {1}|>|>, {"SubstitutionLemma", 44} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] b == a \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] b))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 39}, 
         "Construct" -> {"CriticalPairLemma", 26}, "Position" -> {2, 1}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
             (a_)) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[a \[CenterDot] b == a \[CenterDot] (a \[CenterDot] 
              (a \[CenterDot] b))], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 40} -> <|"Statement" -> 
        HoldForm[((a \[CenterDot] b) \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] c)) \[CenterDot] 
           (b \[CenterDot] a) == (a \[CenterDot] b) \[CenterDot] c], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 43}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            (c_) -> c \[CenterDot] (b \[CenterDot] a), "Side" -> 1, 
         "Subpattern" -> {}, "MatchingConstruct" -> {"SubstitutionLemma", 
           44}, "MatchingOrientation" -> -1, "MatchingRule" -> 
          (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] (b_))) -> 
           a \[CenterDot] b, "MatchingSide" -> 1, "Position" -> {}|>|>, 
     {"SubstitutionLemma", 45} -> <|"Statement" -> 
        HoldForm[(b \[CenterDot] a) \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             c)) == (a \[CenterDot] b) \[CenterDot] c], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 40}, 
         "Construct" -> {"SubstitutionLemma", 12}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           (b \[CenterDot] a) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              ((a \[CenterDot] b) \[CenterDot] c)) == 
            (a \[CenterDot] b) \[CenterDot] c], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 41} -> <|"Statement" -> 
        HoldForm[a == (a \[CenterDot] b) \[CenterDot] 
           ((b \[CenterDot] b) \[CenterDot] a)], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 3}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            (((a_) \[CenterDot] (a_)) \[CenterDot] (b_)) -> b, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 12}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {1}|>|>, 
     {"CriticalPairLemma", 42} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] b == ((a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b)) \[CenterDot] a], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 27}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] (b_)) -> a, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"CriticalPairLemma", 41}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            (((b_) \[CenterDot] (b_)) \[CenterDot] (a_)) -> a, 
         "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"SubstitutionLemma", 46} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] b == a \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 42}, 
         "Construct" -> {"SubstitutionLemma", 12}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[a \[CenterDot] b == 
            a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
               b))], "Source" -> "norm"|>|>, {"CriticalPairLemma", 43} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] c) == 
          a \[CenterDot] (((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            ((c \[CenterDot] c) \[CenterDot] a))], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 46}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] 
            (((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
              (b_))) -> a \[CenterDot] b, "Side" -> 1, "Subpattern" -> 
          ((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] (b_)), 
         "MatchingConstruct" -> {"Axiom", 3}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> ((a_) \[CenterDot] ((b_) \[CenterDot] 
              (c_))) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] 
              (c_))) -> ((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            ((c \[CenterDot] c) \[CenterDot] a), "MatchingSide" -> 1, 
         "Position" -> {2}|>|>, {"CriticalPairLemma", 44} -> 
      <|"Statement" -> HoldForm[((a \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] a)) \[CenterDot] (b \[CenterDot] a) == 
          ((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
           (((b \[CenterDot] b) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] a))) \[CenterDot] ((b \[CenterDot] 
              b) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] a))))], "Proof" -> 
        <|"Construct" -> {"CriticalPairLemma", 43}, "Orientation" -> -1, 
         "Rule" -> (a_) \[CenterDot] ((((b_) \[CenterDot] (b_)) \[CenterDot] 
              (a_)) \[CenterDot] (((c_) \[CenterDot] (c_)) \[CenterDot] 
              (a_))) -> a \[CenterDot] (b \[CenterDot] c), "Side" -> 1, 
         "Subpattern" -> (((b_) \[CenterDot] (b_)) \[CenterDot] 
            (a_)) \[CenterDot] (((c_) \[CenterDot] (c_)) \[CenterDot] (a_)), 
         "MatchingConstruct" -> {"Axiom", 2}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             ((b_) \[CenterDot] (b_))) -> a \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"CriticalPairLemma", 45} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] b == ((a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b)) \[CenterDot] b], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 26}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((b_) \[CenterDot] (a_)) -> a, "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] (a_), "MatchingConstruct" -> 
          {"CriticalPairLemma", 26}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((b_) \[CenterDot] (a_)) -> a, "MatchingSide" -> 1, 
         "Position" -> {2}|>|>, {"SubstitutionLemma", 47} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] b == b \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 45}, 
         "Construct" -> {"SubstitutionLemma", 12}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[a \[CenterDot] b == 
            b \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
               b))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 48} -> 
      <|"Statement" -> HoldForm[((a \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] a)) \[CenterDot] (b \[CenterDot] a) == 
          (b \[CenterDot] b) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] a))], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 44}, "Construct" -> 
          {"SubstitutionLemma", 47}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] 
             ((b_) \[CenterDot] (a_))) -> b \[CenterDot] a, 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           ((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
             (b \[CenterDot] a) == (b \[CenterDot] b) \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 49} -> 
      <|"Statement" -> HoldForm[((a \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] a)) \[CenterDot] (b \[CenterDot] a) == 
          (b \[CenterDot] b) \[CenterDot] a], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 48}, "Construct" -> 
          {"CriticalPairLemma", 26}, "Position" -> {2}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
             (a_)) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
               a)) \[CenterDot] (b \[CenterDot] a) == 
            (b \[CenterDot] b) \[CenterDot] a], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 50} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (b \[CenterDot] a) == 
          (b \[CenterDot] b) \[CenterDot] a], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 49}, "Construct" -> 
          {"CriticalPairLemma", 26}, "Position" -> {1}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
             (a_)) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[a \[CenterDot] (b \[CenterDot] a) == 
            (b \[CenterDot] b) \[CenterDot] a], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 46} -> <|"Statement" -> 
        HoldForm[(a \[CenterDot] a) \[CenterDot] b == 
          (a \[CenterDot] a) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
            (b \[CenterDot] (a \[CenterDot] b)))], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 45}, 
         "Orientation" -> 1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            (((b_) \[CenterDot] (a_)) \[CenterDot] (((b_) \[CenterDot] (
                a_)) \[CenterDot] (c_))) -> (b \[CenterDot] a) \[CenterDot] 
            c, "Side" -> 1, "Subpattern" -> ((b_) \[CenterDot] 
            (a_)) \[CenterDot] (c_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 50}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (b_) -> 
           b \[CenterDot] (a \[CenterDot] b), "MatchingSide" -> 1, 
         "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 47} -> 
      <|"Statement" -> HoldForm[b \[CenterDot] b == 
          (a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
           ((b \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 21}, 
         "Orientation" -> 1, "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] 
              (b_))) \[CenterDot] (((b_) \[CenterDot] (b_)) \[CenterDot] 
             ((a_) \[CenterDot] (b_))) -> b \[CenterDot] b, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 12}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {1, 2}|>|>, 
     {"SubstitutionLemma", 51} -> <|"Statement" -> 
        HoldForm[b \[CenterDot] b == (a \[CenterDot] (b \[CenterDot] 
             a)) \[CenterDot] b], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 47}, "Construct" -> 
          {"CriticalPairLemma", 26}, "Position" -> {2}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
             (a_)) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[b \[CenterDot] b == (a \[CenterDot] (b \[CenterDot] 
               a)) \[CenterDot] b], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 52} -> <|"Statement" -> 
        HoldForm[b \[CenterDot] b == b \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] a))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 51}, "Construct" -> 
          {"SubstitutionLemma", 12}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[b \[CenterDot] b == 
            b \[CenterDot] (a \[CenterDot] (b \[CenterDot] a))], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 48} -> 
      <|"Statement" -> HoldForm[b \[CenterDot] (a \[CenterDot] b) == 
          (a \[CenterDot] a) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
            (b \[CenterDot] (a \[CenterDot] b)))], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 3}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            (((a_) \[CenterDot] (a_)) \[CenterDot] (b_)) -> b, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 52}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             ((a_) \[CenterDot] (b_))) -> a \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {1}|>|>, 
     {"SubstitutionLemma", 53} -> <|"Statement" -> 
        HoldForm[(a \[CenterDot] a) \[CenterDot] b == b \[CenterDot] 
           (a \[CenterDot] b)], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 46}, "Construct" -> 
          {"CriticalPairLemma", 48}, "Position" -> {}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            (((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
              ((a_) \[CenterDot] (b_)))) -> b \[CenterDot] 
            (a \[CenterDot] b), "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[(a \[CenterDot] a) \[CenterDot] b == b \[CenterDot] 
             (a \[CenterDot] b)], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 49} -> <|"Statement" -> 
        HoldForm[b == (a \[CenterDot] b) \[CenterDot] (b \[CenterDot] b)], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 26}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((b_) \[CenterDot] (a_)) -> a, "Side" -> 1, "Subpattern" -> {}, 
         "MatchingConstruct" -> {"SubstitutionLemma", 12}, 
         "MatchingOrientation" -> 1, "MatchingRule" -> 
          (a_) \[CenterDot] (b_) -> b \[CenterDot] a, "MatchingSide" -> 1, 
         "Position" -> {}|>|>, {"CriticalPairLemma", 50} -> 
      <|"Statement" -> HoldForm[a == (a \[CenterDot] b) \[CenterDot] 
           (a \[CenterDot] a)], "Proof" -> 
        <|"Construct" -> {"CriticalPairLemma", 49}, "Orientation" -> -1, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
             (b_)) -> b, "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] (b_), 
         "MatchingConstruct" -> {"SubstitutionLemma", 12}, 
         "MatchingOrientation" -> 1, "MatchingRule" -> 
          (a_) \[CenterDot] (b_) -> b \[CenterDot] a, "MatchingSide" -> 1, 
         "Position" -> {1}|>|>, {"CriticalPairLemma", 51} -> 
      <|"Statement" -> HoldForm[((a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b)) \[CenterDot] (a \[CenterDot] a) == 
          (a \[CenterDot] a) \[CenterDot] a], "Proof" -> 
        <|"Construct" -> {"SubstitutionLemma", 53}, "Orientation" -> -1, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (a_)) -> 
           (b \[CenterDot] b) \[CenterDot] a, "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] (a_), "MatchingConstruct" -> 
          {"CriticalPairLemma", 50}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((a_) \[CenterDot] (a_)) -> a, "MatchingSide" -> 1, 
         "Position" -> {2}|>|>, {"SubstitutionLemma", 54} -> 
      <|"Statement" -> HoldForm[((a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b)) \[CenterDot] (a \[CenterDot] a) == 
          a \[CenterDot] (a \[CenterDot] a)], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 51}, "Construct" -> 
          {"SubstitutionLemma", 12}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
             (a \[CenterDot] a) == a \[CenterDot] (a \[CenterDot] a)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 55} -> 
      <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) == 
          a \[CenterDot] (a \[CenterDot] a)], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 54}, "Construct" -> 
          {"SubstitutionLemma", 12}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           (a \[CenterDot] a) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] b)) == a \[CenterDot] (a \[CenterDot] a)], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 52} -> 
      <|"Statement" -> HoldForm[(((b \[CenterDot] b) \[CenterDot] 
             (b \[CenterDot] b)) \[CenterDot] a) \[CenterDot] 
           ((((b \[CenterDot] c) \[CenterDot] (b \[CenterDot] 
               c)) \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
              (b \[CenterDot] c))) \[CenterDot] a) == 
          (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b))) \[CenterDot] 
           (a \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
             ((b \[CenterDot] c) \[CenterDot] (b \[CenterDot] c))))], 
       "Proof" -> <|"Construct" -> {"Axiom", 3}, "Orientation" -> 1, 
         "Rule" -> ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) \[CenterDot] 
            ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) -> 
           ((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            ((c \[CenterDot] c) \[CenterDot] a), "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] (c_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 55}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            (((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
              (b_))) -> a \[CenterDot] (a \[CenterDot] a), 
         "MatchingSide" -> 1, "Position" -> {1, 2}|>|>, 
     {"CriticalPairLemma", 53} -> <|"Statement" -> 
        HoldForm[a == (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] 
              b))) \[CenterDot] (a \[CenterDot] c)], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 27}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] (b_)) -> a, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (a_), "MatchingConstruct" -> 
          {"CriticalPairLemma", 16}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> (a_) \[CenterDot] (a_) -> a \[CenterDot] 
            (b \[CenterDot] (b \[CenterDot] b)), "MatchingSide" -> 1, 
         "Position" -> {1}|>|>, {"SubstitutionLemma", 56} -> 
      <|"Statement" -> HoldForm[(((b \[CenterDot] b) \[CenterDot] 
             (b \[CenterDot] b)) \[CenterDot] a) \[CenterDot] 
           ((((b \[CenterDot] c) \[CenterDot] (b \[CenterDot] 
               c)) \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
              (b \[CenterDot] c))) \[CenterDot] a) == a], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 52}, 
         "Construct" -> {"CriticalPairLemma", 53}, "Position" -> {}, 
         "Rule" -> ((a_) \[CenterDot] ((b_) \[CenterDot] ((b_) \[CenterDot] (
                b_)))) \[CenterDot] ((a_) \[CenterDot] (c_)) -> a, 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           (((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
              a) \[CenterDot] ((((b \[CenterDot] c) \[CenterDot] 
                (b \[CenterDot] c)) \[CenterDot] ((b \[CenterDot] 
                 c) \[CenterDot] (b \[CenterDot] c))) \[CenterDot] a) == a], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 57} -> 
      <|"Statement" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
           ((((b \[CenterDot] c) \[CenterDot] (b \[CenterDot] 
               c)) \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
              (b \[CenterDot] c))) \[CenterDot] a) == a], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 56}, 
         "Construct" -> {"CriticalPairLemma", 26}, "Position" -> {1, 1}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
             (a_)) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[(b \[CenterDot] a) \[CenterDot] 
             ((((b \[CenterDot] c) \[CenterDot] (b \[CenterDot] 
                 c)) \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
                (b \[CenterDot] c))) \[CenterDot] a) == a], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 58} -> 
      <|"Statement" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
           ((b \[CenterDot] c) \[CenterDot] a) == a], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 57}, 
         "Construct" -> {"CriticalPairLemma", 26}, "Position" -> {2, 1}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
             (a_)) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[(b \[CenterDot] a) \[CenterDot] ((b \[CenterDot] 
               c) \[CenterDot] a) == a], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 54} -> <|"Statement" -> 
        HoldForm[a == (a \[CenterDot] b) \[CenterDot] 
           ((b \[CenterDot] c) \[CenterDot] a)], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 58}, 
         "Orientation" -> 1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            (((a_) \[CenterDot] (c_)) \[CenterDot] (b_)) -> b, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 12}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {1}|>|>, 
     {"CriticalPairLemma", 55} -> <|"Statement" -> 
        HoldForm[a == (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] c))], "Proof" -> 
        <|"Construct" -> {"CriticalPairLemma", 54}, "Orientation" -> -1, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            (((b_) \[CenterDot] (c_)) \[CenterDot] (a_)) -> a, "Side" -> 1, 
         "Subpattern" -> ((b_) \[CenterDot] (c_)) \[CenterDot] (a_), 
         "MatchingConstruct" -> {"SubstitutionLemma", 12}, 
         "MatchingOrientation" -> 1, "MatchingRule" -> 
          (a_) \[CenterDot] (b_) -> b \[CenterDot] a, "MatchingSide" -> 1, 
         "Position" -> {2}|>|>, {"Conclusion", 1} -> 
      <|"Statement" -> HoldForm[a == a], "Proof" -> 
        <|"Input" -> {"Hypothesis", 1}, "Construct" -> {"CriticalPairLemma", 
           55}, "Position" -> {}, "Rule" -> 
          ((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
             ((b_) \[CenterDot] (c_))) -> a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[a == a], "Source" -> "cpl"|>|>}|>], 
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
         "Orientation" -> 1, "Rule" -> ((a_) \[CenterDot] ((b_) \[CenterDot] 
              (c_))) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] 
              (c_))) -> ((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            ((c \[CenterDot] c) \[CenterDot] a), "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] ((b_) \[CenterDot] (c_)), 
         "MatchingConstruct" -> {"Axiom", 2}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             ((b_) \[CenterDot] (b_))) -> a \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {1}|>|>, 
     {"SubstitutionLemma", 1} -> <|"Statement" -> 
        HoldForm[((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
           (b \[CenterDot] a) == (a \[CenterDot] a) \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b)))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 1}, 
         "Construct" -> {"Axiom", 1}, "Position" -> {2, 1}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
             (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
             (b \[CenterDot] a) == (a \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b)))], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 2} -> 
      <|"Statement" -> HoldForm[((b \[CenterDot] b) \[CenterDot] 
            a) \[CenterDot] (b \[CenterDot] a) == 
          (a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a)], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 1}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] ((b_) \[CenterDot] ((b_) \[CenterDot] (
                b_)))) -> ((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            (b \[CenterDot] a), "Side" -> 1, "Subpattern" -> 
          (a_) \[CenterDot] ((b_) \[CenterDot] ((b_) \[CenterDot] (b_))), 
         "MatchingConstruct" -> {"Axiom", 2}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             ((b_) \[CenterDot] (b_))) -> a \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"SubstitutionLemma", 2} -> <|"Statement" -> 
        HoldForm[((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
           (b \[CenterDot] a) == a], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 2}, "Construct" -> {"Axiom", 1}, 
         "Position" -> {}, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] (a_)) -> a, "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[((b \[CenterDot] b) \[CenterDot] 
              a) \[CenterDot] (b \[CenterDot] a) == a], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 3} -> 
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
          ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] (a_)) -> 
           a, "MatchingSide" -> 1, "Position" -> {1, 2}|>|>, 
     {"SubstitutionLemma", 3} -> <|"Statement" -> 
        HoldForm[(((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
              b)) \[CenterDot] a) \[CenterDot] 
           (((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
            a) == (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 3}, 
         "Construct" -> {"Axiom", 1}, "Position" -> {2, 2}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
             (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[(((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
                b)) \[CenterDot] a) \[CenterDot] (((b \[CenterDot] 
                b) \[CenterDot] (b \[CenterDot] b)) \[CenterDot] a) == 
            (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 4} -> 
      <|"Statement" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
           (((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
            a) == (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 3}, 
         "Construct" -> {"Axiom", 1}, "Position" -> {1, 1}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
             (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[(b \[CenterDot] a) \[CenterDot] (((b \[CenterDot] 
                b) \[CenterDot] (b \[CenterDot] b)) \[CenterDot] a) == 
            (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 5} -> 
      <|"Statement" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
           (b \[CenterDot] a) == (a \[CenterDot] b) \[CenterDot] 
           (a \[CenterDot] b)], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 4}, "Construct" -> {"Axiom", 1}, 
         "Position" -> {2, 1}, "Rule" -> 
          ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] (a_)) -> 
           a, "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 
          1, "Side" -> 1, "OutputExpression" -> HoldForm[
           (b \[CenterDot] a) \[CenterDot] (b \[CenterDot] a) == 
            (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 4} -> 
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
     {"CriticalPairLemma", 5} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (a \[CenterDot] a) == 
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
           a \[CenterDot] (a \[CenterDot] a) == (a \[CenterDot] 
              a) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] a))], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 7} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (a \[CenterDot] a) == 
          (a \[CenterDot] a) \[CenterDot] a], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 6}, "Construct" -> {"Axiom", 1}, 
         "Position" -> {2}, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] (a_)) -> a, "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] (a \[CenterDot] a) == 
            (a \[CenterDot] a) \[CenterDot] a], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 8} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] b == ((a \[CenterDot] b) \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
              b))) \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
            (b \[CenterDot] a))], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 4}, "Construct" -> 
          {"SubstitutionLemma", 7}, "Position" -> {1}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (a_) -> 
           a \[CenterDot] (a \[CenterDot] a), "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] b == 
            ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
               (a \[CenterDot] b))) \[CenterDot] ((b \[CenterDot] 
               a) \[CenterDot] (b \[CenterDot] a))], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 6} -> <|"Statement" -> 
        HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] b)], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 2}, 
         "Orientation" -> 1, "Rule" -> (((a_) \[CenterDot] (a_)) \[CenterDot] 
             (b_)) \[CenterDot] ((a_) \[CenterDot] (b_)) -> b, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (a_), "MatchingConstruct" -> 
          {"Axiom", 1}, "MatchingOrientation" -> 1, "MatchingRule" -> 
          ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] (a_)) -> 
           a, "MatchingSide" -> 1, "Position" -> {1, 1}|>|>, 
     {"CriticalPairLemma", 7} -> <|"Statement" -> 
        HoldForm[b \[CenterDot] (b \[CenterDot] b) == 
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
     {"SubstitutionLemma", 9} -> <|"Statement" -> 
        HoldForm[b \[CenterDot] (b \[CenterDot] b) == 
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
             a)) \[CenterDot] (((b \[CenterDot] (a \[CenterDot] (
                a \[CenterDot] a))) \[CenterDot] (b \[CenterDot] 
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
              b) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] (
                a \[CenterDot] a)) \[CenterDot] b)) \[CenterDot] b)], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 8}, 
         "Construct" -> {"Axiom", 3}, "Position" -> {2, 1}, 
         "Rule" -> ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) \[CenterDot] 
            ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) -> 
           ((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            ((c \[CenterDot] c) \[CenterDot] a), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[b == (a \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] ((((a \[CenterDot] 
                 a) \[CenterDot] b) \[CenterDot] (((a \[CenterDot] 
                  a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
                b)) \[CenterDot] b)], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 11} -> <|"Statement" -> 
        HoldForm[b == (a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
           (b \[CenterDot] b)], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 10}, "Construct" -> 
          {"CriticalPairLemma", 6}, "Position" -> {2, 1}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            (((a_) \[CenterDot] (a_)) \[CenterDot] (b_)) -> b, 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           b == (a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
             (b \[CenterDot] b)], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 12} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] b == b \[CenterDot] a], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 8}, 
         "Construct" -> {"SubstitutionLemma", 11}, "Position" -> {}, 
         "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] (a_))) \[CenterDot] 
            ((b_) \[CenterDot] (b_)) -> b, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] b == 
            b \[CenterDot] a], "Source" -> "norm"|>|>, 
     {"Conclusion", 1} -> <|"Statement" -> HoldForm[a \[CenterDot] b == 
          a \[CenterDot] b], "Proof" -> <|"Input" -> {"Hypothesis", 1}, 
         "Construct" -> {"SubstitutionLemma", 12}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[a \[CenterDot] b == 
            a \[CenterDot] b], "Source" -> "cpl"|>|>}|>]}
