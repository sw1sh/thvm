{ProofObject["EquationalLogic", Inactive[Equal][
   (a \[CenterDot] a) \[CenterDot] (a \[CenterDot] b), a], 
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
         (a \[CenterDot] a) \[CenterDot] (a \[CenterDot] b) == a], 
       "Proof" -> <||>|>, {"CriticalPairLemma", 1} -> 
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
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 4} -> 
      <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
           (a \[CenterDot] b)], "Proof" -> <|"Construct" -> {"Axiom", 2}, 
         "Orientation" -> 1, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((b_) \[CenterDot] (a_)) -> a, "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] (a_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 3}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"Conclusion", 1} -> <|"Statement" -> HoldForm[a == a], 
       "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Construct" -> 
          {"CriticalPairLemma", 4}, "Position" -> {}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
             (b_)) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[a == a], "Source" -> "cpl"|>|>}|>], 
 ProofObject["EquationalLogic", Inactive[Equal][
   a \[CenterDot] (a \[CenterDot] b), a \[CenterDot] (b \[CenterDot] b)], 
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
         a \[CenterDot] (a \[CenterDot] b) == a \[CenterDot] 
           (b \[CenterDot] b)], "Proof" -> <||>|>, 
     {"CriticalPairLemma", 1} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] a == a \[CenterDot] (b \[CenterDot] 
            (a \[CenterDot] a))], "Proof" -> <|"Construct" -> {"Axiom", 2}, 
         "Orientation" -> 1, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((b_) \[CenterDot] (a_)) -> a, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (a_), "MatchingConstruct" -> 
          {"Axiom", 2}, "MatchingOrientation" -> 1, "MatchingRule" -> 
          ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] (a_)) -> 
           a, "MatchingSide" -> 1, "Position" -> {1}|>|>, 
     {"CriticalPairLemma", 2} -> <|"Statement" -> 
        HoldForm[((a \[CenterDot] b) \[CenterDot] b) \[CenterDot] a == 
          a \[CenterDot] a], "Proof" -> <|"Construct" -> {"Axiom", 1}, 
         "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             ((a_) \[CenterDot] (c_))) -> ((c \[CenterDot] b) \[CenterDot] 
             b) \[CenterDot] a, "Side" -> 1, "Subpattern" -> {}, 
         "MatchingConstruct" -> {"CriticalPairLemma", 1}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          (a_) \[CenterDot] ((b_) \[CenterDot] ((a_) \[CenterDot] (a_))) -> 
           a \[CenterDot] a, "MatchingSide" -> 1, "Position" -> {}|>|>, 
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
     {"SubstitutionLemma", 1} -> <|"Statement" -> 
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
     {"CriticalPairLemma", 4} -> <|"Statement" -> 
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
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 4}, 
         "Construct" -> {"Axiom", 2}, "Position" -> {1}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
             (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[b \[CenterDot] a == a \[CenterDot] b], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 4} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] ((a \[CenterDot] 
             b) \[CenterDot] b) == a \[CenterDot] a], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 2}, 
         "Construct" -> {"SubstitutionLemma", 3}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] b) == 
            a \[CenterDot] a], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 5} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (b \[CenterDot] (a \[CenterDot] b)) == 
          a \[CenterDot] a], "Proof" -> <|"Input" -> {"SubstitutionLemma", 
           4}, "Construct" -> {"SubstitutionLemma", 3}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           a \[CenterDot] (b \[CenterDot] (a \[CenterDot] b)) == 
            a \[CenterDot] a], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 5} -> <|"Statement" -> 
        HoldForm[(((b \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
            b) \[CenterDot] a == a \[CenterDot] (b \[CenterDot] b)], 
       "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] ((a_) \[CenterDot] 
              (c_))) -> ((c \[CenterDot] b) \[CenterDot] b) \[CenterDot] a, 
         "Side" -> 1, "Subpattern" -> (b_) \[CenterDot] ((a_) \[CenterDot] 
            (c_)), "MatchingConstruct" -> {"SubstitutionLemma", 5}, 
         "MatchingOrientation" -> 1, "MatchingRule" -> 
          (a_) \[CenterDot] ((b_) \[CenterDot] ((a_) \[CenterDot] (b_))) -> 
           a \[CenterDot] a, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"SubstitutionLemma", 6} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (((b \[CenterDot] a) \[CenterDot] 
             b) \[CenterDot] b) == a \[CenterDot] (b \[CenterDot] b)], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 5}, 
         "Construct" -> {"SubstitutionLemma", 3}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           a \[CenterDot] (((b \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
              b) == a \[CenterDot] (b \[CenterDot] b)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 7} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
            ((b \[CenterDot] a) \[CenterDot] b)) == a \[CenterDot] 
           (b \[CenterDot] b)], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 6}, "Construct" -> 
          {"SubstitutionLemma", 3}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b)) == a \[CenterDot] (b \[CenterDot] b)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 8} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
            (b \[CenterDot] (b \[CenterDot] a))) == a \[CenterDot] 
           (b \[CenterDot] b)], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 7}, "Construct" -> 
          {"SubstitutionLemma", 3}, "Position" -> {2, 2}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           a \[CenterDot] (b \[CenterDot] (b \[CenterDot] (b \[CenterDot] 
                a))) == a \[CenterDot] (b \[CenterDot] b)], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 6} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] b) == 
          a \[CenterDot] (b \[CenterDot] (b \[CenterDot] (a \[CenterDot] 
              b)))], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 8}, 
         "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             ((b_) \[CenterDot] ((b_) \[CenterDot] (a_)))) -> 
           a \[CenterDot] (b \[CenterDot] b), "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] (a_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 3}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {2, 2, 2}|>|>, 
     {"CriticalPairLemma", 7} -> <|"Statement" -> 
        HoldForm[(((c \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
             b) \[CenterDot] b) \[CenterDot] a == a \[CenterDot] 
           (b \[CenterDot] (a \[CenterDot] a))], 
       "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] ((a_) \[CenterDot] 
              (c_))) -> ((c \[CenterDot] b) \[CenterDot] b) \[CenterDot] a, 
         "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] (c_), 
         "MatchingConstruct" -> {"CriticalPairLemma", 1}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          (a_) \[CenterDot] ((b_) \[CenterDot] ((a_) \[CenterDot] (a_))) -> 
           a \[CenterDot] a, "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
     {"SubstitutionLemma", 9} -> <|"Statement" -> 
        HoldForm[(((c \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
             b) \[CenterDot] b) \[CenterDot] a == a \[CenterDot] a], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 7}, 
         "Construct" -> {"CriticalPairLemma", 1}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] ((a_) \[CenterDot] 
              (a_))) -> a \[CenterDot] a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[
           (((c \[CenterDot] (a \[CenterDot] a)) \[CenterDot] b) \[CenterDot] 
              b) \[CenterDot] a == a \[CenterDot] a], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 10} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (((c \[CenterDot] (a \[CenterDot] 
               a)) \[CenterDot] b) \[CenterDot] b) == a \[CenterDot] a], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 9}, 
         "Construct" -> {"SubstitutionLemma", 3}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           a \[CenterDot] (((c \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
               b) \[CenterDot] b) == a \[CenterDot] a], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 11} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
            ((c \[CenterDot] (a \[CenterDot] a)) \[CenterDot] b)) == 
          a \[CenterDot] a], "Proof" -> <|"Input" -> {"SubstitutionLemma", 
           10}, "Construct" -> {"SubstitutionLemma", 3}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           a \[CenterDot] (b \[CenterDot] ((c \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] b)) == a \[CenterDot] a], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 8} -> 
      <|"Statement" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
           (a \[CenterDot] a) == a], "Proof" -> 
        <|"Construct" -> {"SubstitutionLemma", 3}, "Orientation" -> 1, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, "Side" -> 1, 
         "Subpattern" -> {}, "MatchingConstruct" -> {"Axiom", 2}, 
         "MatchingOrientation" -> 1, "MatchingRule" -> 
          ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] (a_)) -> 
           a, "MatchingSide" -> 1, "Position" -> {}|>|>, 
     {"CriticalPairLemma", 9} -> <|"Statement" -> 
        HoldForm[a == (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] a)], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 8}, 
         "Orientation" -> 1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((b_) \[CenterDot] (b_)) -> b, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 3}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {1}|>|>, 
     {"CriticalPairLemma", 10} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] a == a \[CenterDot] 
           ((b \[CenterDot] b) \[CenterDot] b)], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 11}, 
         "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             (((c_) \[CenterDot] ((a_) \[CenterDot] (a_))) \[CenterDot] 
              (b_))) -> a \[CenterDot] a, "Side" -> 1, "Subpattern" -> 
          ((c_) \[CenterDot] ((a_) \[CenterDot] (a_))) \[CenterDot] (b_), 
         "MatchingConstruct" -> {"CriticalPairLemma", 9}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          ((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] (a_)) -> 
           a, "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
     {"SubstitutionLemma", 12} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] a == a \[CenterDot] (b \[CenterDot] 
            (b \[CenterDot] b))], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 10}, "Construct" -> 
          {"SubstitutionLemma", 2}, "Position" -> {2}, 
         "Rule" -> ((b_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
           a \[CenterDot] (b \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] a == 
            a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b))], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 11} -> 
      <|"Statement" -> HoldForm[(((a \[CenterDot] a) \[CenterDot] 
             b) \[CenterDot] b) \[CenterDot] a == a \[CenterDot] 
           (b \[CenterDot] b)], "Proof" -> <|"Construct" -> {"Axiom", 1}, 
         "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             ((a_) \[CenterDot] (c_))) -> ((c \[CenterDot] b) \[CenterDot] 
             b) \[CenterDot] a, "Side" -> 1, "Subpattern" -> 
          (b_) \[CenterDot] ((a_) \[CenterDot] (c_)), "MatchingConstruct" -> 
          {"SubstitutionLemma", 12}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             ((b_) \[CenterDot] (b_))) -> a \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"SubstitutionLemma", 13} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
             b) \[CenterDot] b) == a \[CenterDot] (b \[CenterDot] b)], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 11}, 
         "Construct" -> {"SubstitutionLemma", 3}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           a \[CenterDot] (((a \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
              b) == a \[CenterDot] (b \[CenterDot] b)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 14} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
            ((a \[CenterDot] a) \[CenterDot] b)) == a \[CenterDot] 
           (b \[CenterDot] b)], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 13}, "Construct" -> 
          {"SubstitutionLemma", 3}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           a \[CenterDot] (b \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
               b)) == a \[CenterDot] (b \[CenterDot] b)], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 12} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] ((b \[CenterDot] 
             a) \[CenterDot] (b \[CenterDot] a)) == a \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] a)], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 14}, 
         "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (b_))) -> 
           a \[CenterDot] (b \[CenterDot] b), "Side" -> 1, 
         "Subpattern" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (b_), 
         "MatchingConstruct" -> {"Axiom", 2}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((b_) \[CenterDot] (a_)) -> a, "MatchingSide" -> 1, 
         "Position" -> {2, 2}|>|>, {"SubstitutionLemma", 15} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] ((b \[CenterDot] 
             a) \[CenterDot] (b \[CenterDot] a)) == a \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] a))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 12}, 
         "Construct" -> {"SubstitutionLemma", 3}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
               a)) == a \[CenterDot] (a \[CenterDot] (b \[CenterDot] a))], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 13} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] b == 
          ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
           b], "Proof" -> <|"Construct" -> {"Axiom", 2}, "Orientation" -> 1, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
             (a_)) -> a, "Side" -> 1, "Subpattern" -> (b_) \[CenterDot] (a_), 
         "MatchingConstruct" -> {"Axiom", 2}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((b_) \[CenterDot] (a_)) -> a, "MatchingSide" -> 1, 
         "Position" -> {2}|>|>, {"SubstitutionLemma", 16} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] b == b \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 13}, 
         "Construct" -> {"SubstitutionLemma", 2}, "Position" -> {}, 
         "Rule" -> ((b_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
           a \[CenterDot] (b \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] b == 
            b \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
               b))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 17} -> 
      <|"Statement" -> HoldForm[b \[CenterDot] a == a \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] a))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 15}, 
         "Construct" -> {"SubstitutionLemma", 16}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] 
             ((b_) \[CenterDot] (a_))) -> b \[CenterDot] a, 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[b \[CenterDot] a == 
            a \[CenterDot] (a \[CenterDot] (b \[CenterDot] a))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 18} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] b) == 
          a \[CenterDot] (a \[CenterDot] b)], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 6}, "Construct" -> 
          {"SubstitutionLemma", 17}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] 
              (a_))) -> b \[CenterDot] a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] (b \[CenterDot] b) == 
            a \[CenterDot] (a \[CenterDot] b)], "Source" -> "norm"|>|>, 
     {"Conclusion", 1} -> <|"Statement" -> HoldForm[
         a \[CenterDot] (a \[CenterDot] b) == a \[CenterDot] 
           (a \[CenterDot] b)], "Proof" -> <|"Input" -> {"Hypothesis", 1}, 
         "Construct" -> {"SubstitutionLemma", 18}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (b_)) -> 
           a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] (a \[CenterDot] b) == 
            a \[CenterDot] (a \[CenterDot] b)], "Source" -> "cpl"|>|>}|>], 
 ProofObject["EquationalLogic", Inactive[Equal][
   a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c)), 
   b \[CenterDot] (b \[CenterDot] (a \[CenterDot] c))], 
  {Inactive[Equal][(a_) \[CenterDot] ((b_) \[CenterDot] 
      ((a_) \[CenterDot] (c_))), (((c_) \[CenterDot] (b_)) \[CenterDot] 
      (b_)) \[CenterDot] (a_)], Inactive[Equal][
    ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] (a_)), a_]}, 
  <|"Variables" -> {a, b, c, x3}, "Constants" -> {}, 
   "Proof" -> {{"Axiom", 1} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (b \[CenterDot] (a \[CenterDot] c)) == 
          ((c \[CenterDot] b) \[CenterDot] b) \[CenterDot] a], 
       "Proof" -> <||>|>, {"Axiom", 2} -> 
      <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
           (b \[CenterDot] a) == a], "Proof" -> <||>|>, 
     {"Hypothesis", 1} -> <|"Statement" -> HoldForm[
         a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c)) == 
          b \[CenterDot] (b \[CenterDot] (a \[CenterDot] c))], 
       "Proof" -> <||>|>, {"CriticalPairLemma", 1} -> 
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
      <|"Statement" -> HoldForm[((a \[CenterDot] b) \[CenterDot] 
            b) \[CenterDot] a == a \[CenterDot] a], 
       "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] ((a_) \[CenterDot] 
              (c_))) -> ((c \[CenterDot] b) \[CenterDot] b) \[CenterDot] a, 
         "Side" -> 1, "Subpattern" -> {}, "MatchingConstruct" -> 
          {"CriticalPairLemma", 1}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             ((a_) \[CenterDot] (a_))) -> a \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {}|>|>, 
     {"SubstitutionLemma", 10} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] b) == 
          a \[CenterDot] a], "Proof" -> <|"Input" -> {"CriticalPairLemma", 
           9}, "Construct" -> {"SubstitutionLemma", 4}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] b) == 
            a \[CenterDot] a], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 11} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (b \[CenterDot] (a \[CenterDot] b)) == 
          a \[CenterDot] a], "Proof" -> <|"Input" -> {"SubstitutionLemma", 
           10}, "Construct" -> {"SubstitutionLemma", 4}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           a \[CenterDot] (b \[CenterDot] (a \[CenterDot] b)) == 
            a \[CenterDot] a], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 10} -> <|"Statement" -> 
        HoldForm[(((b \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
            b) \[CenterDot] a == a \[CenterDot] (b \[CenterDot] b)], 
       "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] ((a_) \[CenterDot] 
              (c_))) -> ((c \[CenterDot] b) \[CenterDot] b) \[CenterDot] a, 
         "Side" -> 1, "Subpattern" -> (b_) \[CenterDot] ((a_) \[CenterDot] 
            (c_)), "MatchingConstruct" -> {"SubstitutionLemma", 11}, 
         "MatchingOrientation" -> 1, "MatchingRule" -> 
          (a_) \[CenterDot] ((b_) \[CenterDot] ((a_) \[CenterDot] (b_))) -> 
           a \[CenterDot] a, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"SubstitutionLemma", 12} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (((b \[CenterDot] a) \[CenterDot] 
             b) \[CenterDot] b) == a \[CenterDot] (b \[CenterDot] b)], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 10}, 
         "Construct" -> {"SubstitutionLemma", 4}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           a \[CenterDot] (((b \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
              b) == a \[CenterDot] (b \[CenterDot] b)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 13} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
            ((b \[CenterDot] a) \[CenterDot] b)) == a \[CenterDot] 
           (b \[CenterDot] b)], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 12}, "Construct" -> 
          {"SubstitutionLemma", 4}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               b)) == a \[CenterDot] (b \[CenterDot] b)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 14} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
            (b \[CenterDot] (b \[CenterDot] a))) == a \[CenterDot] 
           (b \[CenterDot] b)], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 13}, "Construct" -> 
          {"SubstitutionLemma", 4}, "Position" -> {2, 2}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           a \[CenterDot] (b \[CenterDot] (b \[CenterDot] (b \[CenterDot] 
                a))) == a \[CenterDot] (b \[CenterDot] b)], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 11} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] b) == 
          a \[CenterDot] (b \[CenterDot] (b \[CenterDot] (a \[CenterDot] 
              b)))], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 14}, 
         "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             ((b_) \[CenterDot] ((b_) \[CenterDot] (a_)))) -> 
           a \[CenterDot] (b \[CenterDot] b), "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] (a_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 4}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {2, 2, 2}|>|>, 
     {"CriticalPairLemma", 12} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
            (b \[CenterDot] a)) == a \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] a)], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 9}, 
         "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (b_))) -> 
           a \[CenterDot] (b \[CenterDot] b), "Side" -> 1, 
         "Subpattern" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (b_), 
         "MatchingConstruct" -> {"Axiom", 2}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((b_) \[CenterDot] (a_)) -> a, "MatchingSide" -> 1, 
         "Position" -> {2, 2}|>|>, {"SubstitutionLemma", 15} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] ((b \[CenterDot] 
             a) \[CenterDot] (b \[CenterDot] a)) == a \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] a))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 12}, 
         "Construct" -> {"SubstitutionLemma", 4}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
               a)) == a \[CenterDot] (a \[CenterDot] (b \[CenterDot] a))], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 13} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] b == 
          ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
           b], "Proof" -> <|"Construct" -> {"Axiom", 2}, "Orientation" -> 1, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
             (a_)) -> a, "Side" -> 1, "Subpattern" -> (b_) \[CenterDot] (a_), 
         "MatchingConstruct" -> {"Axiom", 2}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((b_) \[CenterDot] (a_)) -> a, "MatchingSide" -> 1, 
         "Position" -> {2}|>|>, {"SubstitutionLemma", 16} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] b == b \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 13}, 
         "Construct" -> {"SubstitutionLemma", 3}, "Position" -> {}, 
         "Rule" -> ((b_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
           a \[CenterDot] (b \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] b == 
            b \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
               b))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 17} -> 
      <|"Statement" -> HoldForm[b \[CenterDot] a == a \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] a))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 15}, 
         "Construct" -> {"SubstitutionLemma", 16}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] 
             ((b_) \[CenterDot] (a_))) -> b \[CenterDot] a, 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[b \[CenterDot] a == 
            a \[CenterDot] (a \[CenterDot] (b \[CenterDot] a))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 18} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] b) == 
          a \[CenterDot] (a \[CenterDot] b)], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 11}, "Construct" -> 
          {"SubstitutionLemma", 17}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] 
              (a_))) -> b \[CenterDot] a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] (b \[CenterDot] b) == 
            a \[CenterDot] (a \[CenterDot] b)], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 14} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
            (b \[CenterDot] b)) == a \[CenterDot] 
           ((b \[CenterDot] b) \[CenterDot] a)], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 18}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
             (b_)) -> a \[CenterDot] (b \[CenterDot] b), "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 3}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (b_)) -> 
           (b \[CenterDot] b) \[CenterDot] a, "MatchingSide" -> 1, 
         "Position" -> {2}|>|>, {"SubstitutionLemma", 19} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] b == a \[CenterDot] 
           ((b \[CenterDot] b) \[CenterDot] a)], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 14}, 
         "Construct" -> {"Axiom", 2}, "Position" -> {2}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
             (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[a \[CenterDot] b == a \[CenterDot] 
             ((b \[CenterDot] b) \[CenterDot] a)], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 20} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (b \[CenterDot] a) == a \[CenterDot] 
           (b \[CenterDot] b)], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 9}, "Construct" -> 
          {"SubstitutionLemma", 19}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] (((b_) \[CenterDot] (b_)) \[CenterDot] 
             (a_)) -> a \[CenterDot] b, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[a \[CenterDot] (b \[CenterDot] a) == 
            a \[CenterDot] (b \[CenterDot] b)], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 15} -> <|"Statement" -> 
        HoldForm[(b \[CenterDot] b) \[CenterDot] (a \[CenterDot] b) == 
          (a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            b)], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 17}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
             ((b_) \[CenterDot] (a_))) -> b \[CenterDot] a, "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] (a_), "MatchingConstruct" -> 
          {"Axiom", 2}, "MatchingOrientation" -> 1, "MatchingRule" -> 
          ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] (a_)) -> 
           a, "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
     {"SubstitutionLemma", 21} -> <|"Statement" -> 
        HoldForm[(b \[CenterDot] b) \[CenterDot] (a \[CenterDot] b) == 
          (a \[CenterDot] b) \[CenterDot] (b \[CenterDot] (a \[CenterDot] 
             b))], "Proof" -> <|"Input" -> {"CriticalPairLemma", 15}, 
         "Construct" -> {"SubstitutionLemma", 4}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           (b \[CenterDot] b) \[CenterDot] (a \[CenterDot] b) == 
            (a \[CenterDot] b) \[CenterDot] (b \[CenterDot] (a \[CenterDot] 
               b))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 22} -> 
      <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
           (b \[CenterDot] (a \[CenterDot] b))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 21}, 
         "Construct" -> {"Axiom", 2}, "Position" -> {}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
             (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[b == (a \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
              (a \[CenterDot] b))], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 16} -> <|"Statement" -> 
        HoldForm[b \[CenterDot] (b \[CenterDot] b) == 
          (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b))) \[CenterDot] 
           ((b \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
            (a \[CenterDot] a))], "Proof" -> 
        <|"Construct" -> {"SubstitutionLemma", 22}, "Orientation" -> -1, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
             ((a_) \[CenterDot] (b_))) -> b, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 7}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             ((b_) \[CenterDot] (b_))) -> a \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
     {"CriticalPairLemma", 17} -> <|"Statement" -> 
        HoldForm[b \[CenterDot] b == ((a \[CenterDot] a) \[CenterDot] 
            a) \[CenterDot] b], "Proof" -> 
        <|"Construct" -> {"SubstitutionLemma", 11}, "Orientation" -> 1, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] ((a_) \[CenterDot] 
              (b_))) -> a \[CenterDot] a, "Side" -> 1, "Subpattern" -> {}, 
         "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             ((a_) \[CenterDot] (c_))) -> ((c \[CenterDot] b) \[CenterDot] 
             b) \[CenterDot] a, "MatchingSide" -> 1, "Position" -> {}|>|>, 
     {"SubstitutionLemma", 23} -> <|"Statement" -> 
        HoldForm[b \[CenterDot] b == (a \[CenterDot] (a \[CenterDot] 
             a)) \[CenterDot] b], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 17}, "Construct" -> 
          {"SubstitutionLemma", 3}, "Position" -> {1}, 
         "Rule" -> ((b_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
           a \[CenterDot] (b \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[b \[CenterDot] b == 
            (a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] b], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 18} -> 
      <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
           (a \[CenterDot] a) == (a \[CenterDot] a) \[CenterDot] 
           (b \[CenterDot] (b \[CenterDot] b))], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 23}, 
         "Orientation" -> -1, "Rule" -> 
          ((a_) \[CenterDot] ((a_) \[CenterDot] (a_))) \[CenterDot] (b_) -> 
           b \[CenterDot] b, "Side" -> 1, "Subpattern" -> {}, 
         "MatchingConstruct" -> {"SubstitutionLemma", 3}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          (a_) \[CenterDot] ((b_) \[CenterDot] (b_)) -> 
           (b \[CenterDot] b) \[CenterDot] a, "MatchingSide" -> 1, 
         "Position" -> {}|>|>, {"SubstitutionLemma", 24} -> 
      <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
           (b \[CenterDot] (b \[CenterDot] b))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 18}, 
         "Construct" -> {"Axiom", 2}, "Position" -> {}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
             (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[a == (a \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
              (b \[CenterDot] b))], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 19} -> <|"Statement" -> 
        HoldForm[b == (a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
           (b \[CenterDot] b)], "Proof" -> 
        <|"Construct" -> {"SubstitutionLemma", 24}, "Orientation" -> -1, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
             ((b_) \[CenterDot] (b_))) -> a, "Side" -> 1, "Subpattern" -> {}, 
         "MatchingConstruct" -> {"SubstitutionLemma", 4}, 
         "MatchingOrientation" -> 1, "MatchingRule" -> 
          (a_) \[CenterDot] (b_) -> b \[CenterDot] a, "MatchingSide" -> 1, 
         "Position" -> {}|>|>, {"SubstitutionLemma", 25} -> 
      <|"Statement" -> HoldForm[b \[CenterDot] (b \[CenterDot] b) == 
          (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b))) \[CenterDot] 
           a], "Proof" -> <|"Input" -> {"CriticalPairLemma", 16}, 
         "Construct" -> {"CriticalPairLemma", 19}, "Position" -> {2}, 
         "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] (a_))) \[CenterDot] 
            ((b_) \[CenterDot] (b_)) -> b, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[b \[CenterDot] (b \[CenterDot] b) == 
            (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b))) \[CenterDot] 
             a], "Source" -> "norm"|>|>, {"SubstitutionLemma", 26} -> 
      <|"Statement" -> HoldForm[b \[CenterDot] (b \[CenterDot] b) == 
          a \[CenterDot] (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] 
              b)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 25}, 
         "Construct" -> {"SubstitutionLemma", 4}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           b \[CenterDot] (b \[CenterDot] b) == a \[CenterDot] 
             (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b)))], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 20} -> 
      <|"Statement" -> HoldForm[(a \[CenterDot] (b \[CenterDot] 
             (b \[CenterDot] b))) \[CenterDot] (a \[CenterDot] a) == 
          (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b))) \[CenterDot] 
           (b \[CenterDot] (b \[CenterDot] b))], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 20}, 
         "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             (a_)) -> a \[CenterDot] (b \[CenterDot] b), "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] (a_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 26}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
             ((b_) \[CenterDot] ((b_) \[CenterDot] (b_)))) -> 
           b \[CenterDot] (b \[CenterDot] b), "MatchingSide" -> 1, 
         "Position" -> {2}|>|>, {"SubstitutionLemma", 27} -> 
      <|"Statement" -> HoldForm[(a \[CenterDot] (b \[CenterDot] 
             (b \[CenterDot] b))) \[CenterDot] (a \[CenterDot] a) == 
          (b \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b)))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 20}, 
         "Construct" -> {"SubstitutionLemma", 4}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b))) \[CenterDot] 
             (a \[CenterDot] a) == (b \[CenterDot] (b \[CenterDot] 
               b)) \[CenterDot] (a \[CenterDot] (b \[CenterDot] (
                b \[CenterDot] b)))], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 28} -> <|"Statement" -> 
        HoldForm[a == (b \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b)))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 27}, 
         "Construct" -> {"CriticalPairLemma", 6}, "Position" -> {}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
             (a_)) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[a == (b \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
             (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b)))], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 21} -> 
      <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
           (b \[CenterDot] (b \[CenterDot] a))], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 22}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((b_) \[CenterDot] ((a_) \[CenterDot] (b_))) -> b, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 4}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
     {"CriticalPairLemma", 22} -> <|"Statement" -> 
        HoldForm[b == (a \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
            (a \[CenterDot] a))], "Proof" -> 
        <|"Construct" -> {"CriticalPairLemma", 21}, "Orientation" -> -1, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
             ((b_) \[CenterDot] (a_))) -> b, "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] ((b_) \[CenterDot] (a_)), 
         "MatchingConstruct" -> {"SubstitutionLemma", 18}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          (a_) \[CenterDot] ((a_) \[CenterDot] (b_)) -> a \[CenterDot] 
            (b \[CenterDot] b), "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"CriticalPairLemma", 23} -> <|"Statement" -> 
        HoldForm[b \[CenterDot] (a \[CenterDot] b) == 
          (a \[CenterDot] a) \[CenterDot] ((b \[CenterDot] (a \[CenterDot] 
              b)) \[CenterDot] (a \[CenterDot] a))], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 22}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((b_) \[CenterDot] ((a_) \[CenterDot] (a_))) -> b, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 11}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             ((a_) \[CenterDot] (b_))) -> a \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {1}|>|>, 
     {"CriticalPairLemma", 24} -> <|"Statement" -> 
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
     {"CriticalPairLemma", 25} -> <|"Statement" -> 
        HoldForm[(b \[CenterDot] b) \[CenterDot] (a \[CenterDot] a) == 
          (a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
           (b \[CenterDot] b)], "Proof" -> 
        <|"Construct" -> {"CriticalPairLemma", 24}, "Orientation" -> -1, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
             ((a_) \[CenterDot] (b_))) -> (a \[CenterDot] a) \[CenterDot] 
            (b \[CenterDot] b), "Side" -> 1, "Subpattern" -> {}, 
         "MatchingConstruct" -> {"SubstitutionLemma", 4}, 
         "MatchingOrientation" -> 1, "MatchingRule" -> 
          (a_) \[CenterDot] (b_) -> b \[CenterDot] a, "MatchingSide" -> 1, 
         "Position" -> {}|>|>, {"SubstitutionLemma", 29} -> 
      <|"Statement" -> HoldForm[b \[CenterDot] (a \[CenterDot] b) == 
          (a \[CenterDot] a) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
            (b \[CenterDot] b))], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 23}, "Construct" -> 
          {"CriticalPairLemma", 25}, "Position" -> {2}, 
         "Rule" -> ((a_) \[CenterDot] ((b_) \[CenterDot] (a_))) \[CenterDot] 
            ((b_) \[CenterDot] (b_)) -> (b \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] a), "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[b \[CenterDot] (a \[CenterDot] b) == 
            (a \[CenterDot] a) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              (b \[CenterDot] b))], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 26} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b)) == a \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] b))], "Proof" -> 
        <|"Construct" -> {"SubstitutionLemma", 18}, "Orientation" -> -1, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] (b_)) -> 
           a \[CenterDot] (b \[CenterDot] b), "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 18}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> (a_) \[CenterDot] ((a_) \[CenterDot] (b_)) -> 
           a \[CenterDot] (b \[CenterDot] b), "MatchingSide" -> 1, 
         "Position" -> {2}|>|>, {"SubstitutionLemma", 30} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
            (a \[CenterDot] b)) == a \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] b))], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 26}, "Construct" -> 
          {"SubstitutionLemma", 18}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (b_)) -> 
           a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
              (a \[CenterDot] b)) == a \[CenterDot] (a \[CenterDot] 
              (b \[CenterDot] b))], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 27} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] a == ((a \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
            b)], "Proof" -> <|"Construct" -> {"Axiom", 2}, 
         "Orientation" -> 1, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((b_) \[CenterDot] (a_)) -> a, "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] (a_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 3}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (b_)) -> 
           (b \[CenterDot] b) \[CenterDot] a, "MatchingSide" -> 1, 
         "Position" -> {2}|>|>, {"SubstitutionLemma", 31} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] a == a \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] b)], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 27}, 
         "Construct" -> {"Axiom", 2}, "Position" -> {1}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
             (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[a \[CenterDot] a == a \[CenterDot] 
             ((a \[CenterDot] a) \[CenterDot] b)], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 28} -> <|"Statement" -> 
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
         "MatchingConstruct" -> {"SubstitutionLemma", 31}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] (b_)) -> 
           a \[CenterDot] a, "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
     {"SubstitutionLemma", 32} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] ((((a \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] b) \[CenterDot] 
            (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
               a)) \[CenterDot] b)) == a \[CenterDot] 
           (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
            (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
               a)) \[CenterDot] b))], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 28}, "Construct" -> 
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
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 33} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] 
           ((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
               a)) \[CenterDot] b) \[CenterDot] 
            (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
               a)) \[CenterDot] b)) == a \[CenterDot] (a \[CenterDot] 
            (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
               a)) \[CenterDot] b))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 32}, "Construct" -> {"Axiom", 2}, 
         "Position" -> {2, 1}, "Rule" -> 
          ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] (a_)) -> 
           a, "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 
          1, "Side" -> 2, "OutputExpression" -> HoldForm[
           a \[CenterDot] ((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] b) \[CenterDot] (((a \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] b)) == 
            a \[CenterDot] (a \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
                (a \[CenterDot] a)) \[CenterDot] b))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 34} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] 
           ((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
               a)) \[CenterDot] b) \[CenterDot] 
            (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
               a)) \[CenterDot] b)) == a \[CenterDot] (a \[CenterDot] 
            (a \[CenterDot] b))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 33}, "Construct" -> {"Axiom", 2}, 
         "Position" -> {2, 2, 1}, "Rule" -> 
          ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] (a_)) -> 
           a, "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 
          1, "Side" -> 2, "OutputExpression" -> HoldForm[
           a \[CenterDot] ((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] b) \[CenterDot] (((a \[CenterDot] 
                 a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] b)) == 
            a \[CenterDot] (a \[CenterDot] (a \[CenterDot] b))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 35} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] ((a \[CenterDot] 
             b) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
              (a \[CenterDot] a)) \[CenterDot] b)) == a \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] b))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 34}, 
         "Construct" -> {"Axiom", 2}, "Position" -> {2, 1, 1}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
             (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                 a)) \[CenterDot] b)) == a \[CenterDot] (a \[CenterDot] 
              (a \[CenterDot] b))], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 36} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b)) == a \[CenterDot] (a \[CenterDot] 
            (a \[CenterDot] b))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 35}, "Construct" -> {"Axiom", 2}, 
         "Position" -> {2, 2, 1}, "Rule" -> 
          ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] (a_)) -> 
           a, "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 
          1, "Side" -> 1, "OutputExpression" -> HoldForm[
           a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
               b)) == a \[CenterDot] (a \[CenterDot] (a \[CenterDot] b))], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 29} -> 
      <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
           (a \[CenterDot] b)], "Proof" -> <|"Construct" -> {"Axiom", 2}, 
         "Orientation" -> 1, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((b_) \[CenterDot] (a_)) -> a, "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] (a_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 4}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"CriticalPairLemma", 30} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] b == ((a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b)) \[CenterDot] a], 
       "Proof" -> <|"Construct" -> {"Axiom", 2}, "Orientation" -> 1, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
             (a_)) -> a, "Side" -> 1, "Subpattern" -> (b_) \[CenterDot] (a_), 
         "MatchingConstruct" -> {"CriticalPairLemma", 29}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] (b_)) -> 
           a, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"SubstitutionLemma", 37} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] b == a \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 30}, 
         "Construct" -> {"SubstitutionLemma", 3}, "Position" -> {}, 
         "Rule" -> ((b_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
           a \[CenterDot] (b \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] b == 
            a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
               b))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 38} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] b == a \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] b))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 36}, 
         "Construct" -> {"SubstitutionLemma", 37}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             ((a_) \[CenterDot] (b_))) -> a \[CenterDot] b, 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[a \[CenterDot] b == 
            a \[CenterDot] (a \[CenterDot] (a \[CenterDot] b))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 39} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] b == a \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] b))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 30}, 
         "Construct" -> {"SubstitutionLemma", 38}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
              (b_))) -> a \[CenterDot] b, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[a \[CenterDot] b == 
            a \[CenterDot] (a \[CenterDot] (b \[CenterDot] b))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 40} -> 
      <|"Statement" -> HoldForm[b \[CenterDot] (a \[CenterDot] b) == 
          (a \[CenterDot] a) \[CenterDot] b], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 29}, "Construct" -> 
          {"SubstitutionLemma", 39}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] 
              (b_))) -> a \[CenterDot] b, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[b \[CenterDot] (a \[CenterDot] b) == 
            (a \[CenterDot] a) \[CenterDot] b], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 31} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (b \[CenterDot] a) == a \[CenterDot] 
           (b \[CenterDot] b)], "Proof" -> 
        <|"Construct" -> {"SubstitutionLemma", 40}, "Orientation" -> -1, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (b_) -> 
           b \[CenterDot] (a \[CenterDot] b), "Side" -> 1, 
         "Subpattern" -> {}, "MatchingConstruct" -> {"SubstitutionLemma", 4}, 
         "MatchingOrientation" -> 1, "MatchingRule" -> 
          (a_) \[CenterDot] (b_) -> b \[CenterDot] a, "MatchingSide" -> 1, 
         "Position" -> {}|>|>, {"CriticalPairLemma", 32} -> 
      <|"Statement" -> HoldForm[(((c \[CenterDot] c) \[CenterDot] 
             b) \[CenterDot] b) \[CenterDot] a == a \[CenterDot] 
           (b \[CenterDot] (a \[CenterDot] (c \[CenterDot] a)))], 
       "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] ((a_) \[CenterDot] 
              (c_))) -> ((c \[CenterDot] b) \[CenterDot] b) \[CenterDot] a, 
         "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] (c_), 
         "MatchingConstruct" -> {"CriticalPairLemma", 31}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          (a_) \[CenterDot] ((b_) \[CenterDot] (b_)) -> a \[CenterDot] 
            (b \[CenterDot] a), "MatchingSide" -> 1, "Position" -> {2, 
          2}|>|>, {"SubstitutionLemma", 41} -> 
      <|"Statement" -> HoldForm[
         (b \[CenterDot] ((c \[CenterDot] c) \[CenterDot] b)) \[CenterDot] 
           a == a \[CenterDot] (b \[CenterDot] (a \[CenterDot] 
             (c \[CenterDot] a)))], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 32}, "Construct" -> 
          {"SubstitutionLemma", 4}, "Position" -> {1}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           (b \[CenterDot] ((c \[CenterDot] c) \[CenterDot] b)) \[CenterDot] 
             a == a \[CenterDot] (b \[CenterDot] (a \[CenterDot] (
                c \[CenterDot] a)))], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 42} -> <|"Statement" -> 
        HoldForm[(b \[CenterDot] c) \[CenterDot] a == a \[CenterDot] 
           (b \[CenterDot] (a \[CenterDot] (c \[CenterDot] a)))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 41}, 
         "Construct" -> {"SubstitutionLemma", 19}, "Position" -> {1}, 
         "Rule" -> (a_) \[CenterDot] (((b_) \[CenterDot] (b_)) \[CenterDot] 
             (a_)) -> a \[CenterDot] b, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[(b \[CenterDot] c) \[CenterDot] a == 
            a \[CenterDot] (b \[CenterDot] (a \[CenterDot] (c \[CenterDot] 
                a)))], "Source" -> "norm"|>|>, {"CriticalPairLemma", 33} -> 
      <|"Statement" -> HoldForm[(c \[CenterDot] a) \[CenterDot] b == 
          (((a \[CenterDot] b) \[CenterDot] c) \[CenterDot] c) \[CenterDot] 
           b], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 42}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             ((a_) \[CenterDot] ((c_) \[CenterDot] (a_)))) -> 
           (b \[CenterDot] c) \[CenterDot] a, "Side" -> 1, 
         "Subpattern" -> {}, "MatchingConstruct" -> {"Axiom", 1}, 
         "MatchingOrientation" -> 1, "MatchingRule" -> 
          (a_) \[CenterDot] ((b_) \[CenterDot] ((a_) \[CenterDot] (c_))) -> 
           ((c \[CenterDot] b) \[CenterDot] b) \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {}|>|>, 
     {"SubstitutionLemma", 43} -> <|"Statement" -> 
        HoldForm[(c \[CenterDot] a) \[CenterDot] b == b \[CenterDot] 
           (((a \[CenterDot] b) \[CenterDot] c) \[CenterDot] c)], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 33}, 
         "Construct" -> {"SubstitutionLemma", 4}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           (c \[CenterDot] a) \[CenterDot] b == b \[CenterDot] 
             (((a \[CenterDot] b) \[CenterDot] c) \[CenterDot] c)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 44} -> 
      <|"Statement" -> HoldForm[(c \[CenterDot] a) \[CenterDot] b == 
          b \[CenterDot] (c \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             c))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 43}, 
         "Construct" -> {"SubstitutionLemma", 4}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           (c \[CenterDot] a) \[CenterDot] b == b \[CenterDot] 
             (c \[CenterDot] ((a \[CenterDot] b) \[CenterDot] c))], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 34} -> 
      <|"Statement" -> HoldForm[(((b \[CenterDot] a) \[CenterDot] 
             c) \[CenterDot] b) \[CenterDot] a == a \[CenterDot] 
           (((b \[CenterDot] a) \[CenterDot] c) \[CenterDot] 
            ((b \[CenterDot] a) \[CenterDot] (c \[CenterDot] c)))], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 44}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             (((c_) \[CenterDot] (a_)) \[CenterDot] (b_))) -> 
           (b \[CenterDot] c) \[CenterDot] a, "Side" -> 1, 
         "Subpattern" -> ((c_) \[CenterDot] (a_)) \[CenterDot] (b_), 
         "MatchingConstruct" -> {"SubstitutionLemma", 18}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          (a_) \[CenterDot] ((a_) \[CenterDot] (b_)) -> a \[CenterDot] 
            (b \[CenterDot] b), "MatchingSide" -> 1, "Position" -> {2, 
          2}|>|>, {"CriticalPairLemma", 35} -> 
      <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
           (a \[CenterDot] b) == (a \[CenterDot] b) \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] a)], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 17}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
             ((b_) \[CenterDot] (a_))) -> b \[CenterDot] a, "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] (a_), "MatchingConstruct" -> 
          {"CriticalPairLemma", 29}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] (b_)) -> a, "MatchingSide" -> 1, 
         "Position" -> {2, 2}|>|>, {"SubstitutionLemma", 45} -> 
      <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
           (a \[CenterDot] b) == (a \[CenterDot] b) \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] b))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 35}, 
         "Construct" -> {"SubstitutionLemma", 4}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           (a \[CenterDot] a) \[CenterDot] (a \[CenterDot] b) == 
            (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
               b))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 46} -> 
      <|"Statement" -> HoldForm[a == (a \[CenterDot] b) \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] b))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 45}, 
         "Construct" -> {"CriticalPairLemma", 29}, "Position" -> {}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
             (b_)) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[a == (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
              (a \[CenterDot] b))], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 36} -> <|"Statement" -> 
        HoldForm[a == (a \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] (a \[CenterDot] b)))], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 46}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((a_) \[CenterDot] ((a_) \[CenterDot] (b_))) -> a, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 18}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> (a_) \[CenterDot] ((a_) \[CenterDot] (b_)) -> 
           a \[CenterDot] (b \[CenterDot] b), "MatchingSide" -> 1, 
         "Position" -> {1}|>|>, {"SubstitutionLemma", 47} -> 
      <|"Statement" -> HoldForm[a == (a \[CenterDot] (b \[CenterDot] 
             b)) \[CenterDot] (a \[CenterDot] b)], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 36}, 
         "Construct" -> {"SubstitutionLemma", 38}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
              (b_))) -> a \[CenterDot] b, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a == (a \[CenterDot] 
              (b \[CenterDot] b)) \[CenterDot] (a \[CenterDot] b)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 48} -> 
      <|"Statement" -> HoldForm[a == (a \[CenterDot] b) \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] b))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 47}, 
         "Construct" -> {"SubstitutionLemma", 4}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           a == (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
              (b \[CenterDot] b))], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 49} -> <|"Statement" -> 
        HoldForm[(((b \[CenterDot] a) \[CenterDot] c) \[CenterDot] 
            b) \[CenterDot] a == a \[CenterDot] (b \[CenterDot] a)], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 34}, 
         "Construct" -> {"SubstitutionLemma", 48}, "Position" -> {2}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
             ((b_) \[CenterDot] (b_))) -> a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[(((b \[CenterDot] a) \[CenterDot] 
               c) \[CenterDot] b) \[CenterDot] a == a \[CenterDot] 
             (b \[CenterDot] a)], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 50} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (((b \[CenterDot] a) \[CenterDot] 
             c) \[CenterDot] b) == a \[CenterDot] (b \[CenterDot] a)], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 49}, 
         "Construct" -> {"SubstitutionLemma", 4}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           a \[CenterDot] (((b \[CenterDot] a) \[CenterDot] c) \[CenterDot] 
              b) == a \[CenterDot] (b \[CenterDot] a)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 51} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
            ((b \[CenterDot] a) \[CenterDot] c)) == a \[CenterDot] 
           (b \[CenterDot] a)], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 50}, "Construct" -> 
          {"SubstitutionLemma", 4}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
               c)) == a \[CenterDot] (b \[CenterDot] a)], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 37} -> 
      <|"Statement" -> HoldForm[(c \[CenterDot] b) \[CenterDot] a == 
          a \[CenterDot] ((a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
            c)], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 42}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             ((a_) \[CenterDot] ((c_) \[CenterDot] (a_)))) -> 
           (b \[CenterDot] c) \[CenterDot] a, "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] ((a_) \[CenterDot] 
            ((c_) \[CenterDot] (a_))), "MatchingConstruct" -> 
          {"SubstitutionLemma", 4}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"CriticalPairLemma", 38} -> <|"Statement" -> 
        HoldForm[(a \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
            (a \[CenterDot] b)) == (a \[CenterDot] b) \[CenterDot] 
           ((c \[CenterDot] a) \[CenterDot] b)], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 51}, 
         "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             (((b_) \[CenterDot] (a_)) \[CenterDot] (c_))) -> 
           a \[CenterDot] (b \[CenterDot] a), "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] (((b_) \[CenterDot] 
             (a_)) \[CenterDot] (c_)), "MatchingConstruct" -> 
          {"CriticalPairLemma", 37}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> (a_) \[CenterDot] (((a_) \[CenterDot] 
              ((b_) \[CenterDot] (a_))) \[CenterDot] (c_)) -> 
           (c \[CenterDot] b) \[CenterDot] a, "MatchingSide" -> 1, 
         "Position" -> {2}|>|>, {"SubstitutionLemma", 52} -> 
      <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
           (b \[CenterDot] b) == (a \[CenterDot] b) \[CenterDot] 
           ((c \[CenterDot] a) \[CenterDot] b)], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 38}, 
         "Construct" -> {"SubstitutionLemma", 20}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (a_)) -> 
           a \[CenterDot] (b \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
             (b \[CenterDot] b) == (a \[CenterDot] b) \[CenterDot] 
             ((c \[CenterDot] a) \[CenterDot] b)], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 53} -> <|"Statement" -> 
        HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
           ((c \[CenterDot] a) \[CenterDot] b)], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 52}, 
         "Construct" -> {"CriticalPairLemma", 5}, "Position" -> {}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
             (b_)) -> b, "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
             ((c \[CenterDot] a) \[CenterDot] b)], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 39} -> <|"Statement" -> 
        HoldForm[(a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a) == 
          (a \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
            ((c \[CenterDot] a) \[CenterDot] b))], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 6}, 
         "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             (((c_) \[CenterDot] ((a_) \[CenterDot] (a_))) \[CenterDot] 
              (b_))) -> a \[CenterDot] a, "Side" -> 1, "Subpattern" -> 
          (a_) \[CenterDot] (a_), "MatchingConstruct" -> {"Axiom", 2}, 
         "MatchingOrientation" -> 1, "MatchingRule" -> 
          ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] (a_)) -> 
           a, "MatchingSide" -> 1, "Position" -> {2, 2, 1, 2}|>|>, 
     {"SubstitutionLemma", 54} -> <|"Statement" -> 
        HoldForm[a == (a \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
            ((c \[CenterDot] a) \[CenterDot] b))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 39}, 
         "Construct" -> {"Axiom", 2}, "Position" -> {}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
             (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[a == (a \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
              ((c \[CenterDot] a) \[CenterDot] b))], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 40} -> <|"Statement" -> 
        HoldForm[x3 == ((a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
              a)) \[CenterDot] x3) \[CenterDot] (c \[CenterDot] x3)], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 53}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            (((c_) \[CenterDot] (a_)) \[CenterDot] (b_)) -> b, "Side" -> 1, 
         "Subpattern" -> (c_) \[CenterDot] (a_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 54}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((b_) \[CenterDot] (((c_) \[CenterDot] (a_)) \[CenterDot] 
              (b_))) -> a, "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
     {"SubstitutionLemma", 55} -> <|"Statement" -> 
        HoldForm[x3 == (c \[CenterDot] x3) \[CenterDot] 
           ((a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a)) \[CenterDot] 
            x3)], "Proof" -> <|"Input" -> {"CriticalPairLemma", 40}, 
         "Construct" -> {"SubstitutionLemma", 4}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           x3 == (c \[CenterDot] x3) \[CenterDot] ((a \[CenterDot] (
                (b \[CenterDot] c) \[CenterDot] a)) \[CenterDot] x3)], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 41} -> 
      <|"Statement" -> HoldForm[c \[CenterDot] ((a \[CenterDot] 
             ((b \[CenterDot] c) \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a))) == 
          ((a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a)) \[CenterDot] 
            ((a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
               a)) \[CenterDot] (a \[CenterDot] ((b \[CenterDot] 
                c) \[CenterDot] a)))) \[CenterDot] 
           ((a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a)))], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 28}, 
         "Orientation" -> -1, "Rule" -> 
          ((a_) \[CenterDot] ((a_) \[CenterDot] (a_))) \[CenterDot] 
            ((b_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] (
                a_)))) -> b, "Side" -> 1, "Subpattern" -> 
          (b_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] (a_))), 
         "MatchingConstruct" -> {"SubstitutionLemma", 55}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          ((a_) \[CenterDot] (b_)) \[CenterDot] (((c_) \[CenterDot] 
              (((x3_) \[CenterDot] (a_)) \[CenterDot] (c_))) \[CenterDot] 
             (b_)) -> b, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"SubstitutionLemma", 56} -> <|"Statement" -> 
        HoldForm[c \[CenterDot] ((a \[CenterDot] ((b \[CenterDot] 
               c) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
             ((b \[CenterDot] c) \[CenterDot] a))) == 
          ((a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
              a))) \[CenterDot] ((a \[CenterDot] ((b \[CenterDot] 
               c) \[CenterDot] a)) \[CenterDot] ((a \[CenterDot] 
              ((b \[CenterDot] c) \[CenterDot] a)) \[CenterDot] 
             (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a))))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 41}, 
         "Construct" -> {"SubstitutionLemma", 3}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (b_)) -> 
           (b \[CenterDot] b) \[CenterDot] a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[c \[CenterDot] 
             ((a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
                a)) \[CenterDot] (a \[CenterDot] ((b \[CenterDot] 
                 c) \[CenterDot] a))) == ((a \[CenterDot] ((b \[CenterDot] 
                 c) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] (
                (b \[CenterDot] c) \[CenterDot] a))) \[CenterDot] 
             ((a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
                a)) \[CenterDot] ((a \[CenterDot] ((b \[CenterDot] 
                  c) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
                ((b \[CenterDot] c) \[CenterDot] a))))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 57} -> 
      <|"Statement" -> HoldForm[c \[CenterDot] ((a \[CenterDot] 
             ((b \[CenterDot] c) \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a))) == 
          a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a)], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 56}, 
         "Construct" -> {"CriticalPairLemma", 29}, "Position" -> {}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
             (b_)) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[c \[CenterDot] ((a \[CenterDot] ((b \[CenterDot] 
                 c) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] (
                (b \[CenterDot] c) \[CenterDot] a))) == a \[CenterDot] 
             ((b \[CenterDot] c) \[CenterDot] a)], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 58} -> <|"Statement" -> 
        HoldForm[c \[CenterDot] (c \[CenterDot] (a \[CenterDot] 
             ((b \[CenterDot] c) \[CenterDot] a))) == a \[CenterDot] 
           ((b \[CenterDot] c) \[CenterDot] a)], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 57}, 
         "Construct" -> {"SubstitutionLemma", 18}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (b_)) -> 
           a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[c \[CenterDot] (c \[CenterDot] 
              (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a))) == 
            a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a)], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 42} -> 
      <|"Statement" -> HoldForm[(b \[CenterDot] c) \[CenterDot] a == 
          a \[CenterDot] (b \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
              c)))], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 42}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             ((a_) \[CenterDot] ((c_) \[CenterDot] (a_)))) -> 
           (b \[CenterDot] c) \[CenterDot] a, "Side" -> 1, 
         "Subpattern" -> (c_) \[CenterDot] (a_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 4}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {2, 2, 2}|>|>, 
     {"CriticalPairLemma", 43} -> <|"Statement" -> 
        HoldForm[(b \[CenterDot] (c \[CenterDot] ((x3 \[CenterDot] 
               a) \[CenterDot] c))) \[CenterDot] a == a \[CenterDot] 
           (b \[CenterDot] (a \[CenterDot] ((c \[CenterDot] x3) \[CenterDot] 
              a)))], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 42}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             ((a_) \[CenterDot] ((a_) \[CenterDot] (c_)))) -> 
           (b \[CenterDot] c) \[CenterDot] a, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (c_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 44}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             (((c_) \[CenterDot] (a_)) \[CenterDot] (b_))) -> 
           (b \[CenterDot] c) \[CenterDot] a, "MatchingSide" -> 1, 
         "Position" -> {2, 2, 2}|>|>, {"SubstitutionLemma", 59} -> 
      <|"Statement" -> HoldForm[(b \[CenterDot] (c \[CenterDot] 
             ((x3 \[CenterDot] a) \[CenterDot] c))) \[CenterDot] a == 
          (b \[CenterDot] (c \[CenterDot] x3)) \[CenterDot] a], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 43}, 
         "Construct" -> {"SubstitutionLemma", 42}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] ((a_) \[CenterDot] 
              ((c_) \[CenterDot] (a_)))) -> (b \[CenterDot] c) \[CenterDot] 
            a, "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[(b \[CenterDot] (c \[CenterDot] ((x3 \[CenterDot] 
                 a) \[CenterDot] c))) \[CenterDot] a == 
            (b \[CenterDot] (c \[CenterDot] x3)) \[CenterDot] a], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 60} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
            (c \[CenterDot] ((x3 \[CenterDot] a) \[CenterDot] c))) == 
          (b \[CenterDot] (c \[CenterDot] x3)) \[CenterDot] a], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 59}, 
         "Construct" -> {"SubstitutionLemma", 4}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           a \[CenterDot] (b \[CenterDot] (c \[CenterDot] ((x3 \[CenterDot] 
                 a) \[CenterDot] c))) == (b \[CenterDot] (c \[CenterDot] 
               x3)) \[CenterDot] a], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 61} -> <|"Statement" -> 
        HoldForm[(c \[CenterDot] (a \[CenterDot] b)) \[CenterDot] c == 
          a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a)], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 58}, 
         "Construct" -> {"SubstitutionLemma", 60}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] ((c_) \[CenterDot] 
              (((x3_) \[CenterDot] (a_)) \[CenterDot] (c_)))) -> 
           (b \[CenterDot] (c \[CenterDot] x3)) \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           (c \[CenterDot] (a \[CenterDot] b)) \[CenterDot] c == 
            a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 62} -> 
      <|"Statement" -> HoldForm[c \[CenterDot] (c \[CenterDot] 
            (a \[CenterDot] b)) == a \[CenterDot] 
           ((b \[CenterDot] c) \[CenterDot] a)], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 61}, 
         "Construct" -> {"SubstitutionLemma", 4}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           c \[CenterDot] (c \[CenterDot] (a \[CenterDot] b)) == 
            a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a)], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 44} -> 
      <|"Statement" -> HoldForm[(b \[CenterDot] c) \[CenterDot] a == 
          a \[CenterDot] (b \[CenterDot] ((a \[CenterDot] c) \[CenterDot] 
             b))], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 44}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             (((c_) \[CenterDot] (a_)) \[CenterDot] (b_))) -> 
           (b \[CenterDot] c) \[CenterDot] a, "Side" -> 1, 
         "Subpattern" -> (c_) \[CenterDot] (a_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 4}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {2, 2, 1}|>|>, 
     {"CriticalPairLemma", 45} -> <|"Statement" -> 
        HoldForm[(((a \[CenterDot] b) \[CenterDot] c) \[CenterDot] 
            b) \[CenterDot] a == a \[CenterDot] 
           (((a \[CenterDot] b) \[CenterDot] c) \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] (c \[CenterDot] c)))], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 44}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             (((a_) \[CenterDot] (c_)) \[CenterDot] (b_))) -> 
           (b \[CenterDot] c) \[CenterDot] a, "Side" -> 1, 
         "Subpattern" -> ((a_) \[CenterDot] (c_)) \[CenterDot] (b_), 
         "MatchingConstruct" -> {"SubstitutionLemma", 18}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          (a_) \[CenterDot] ((a_) \[CenterDot] (b_)) -> a \[CenterDot] 
            (b \[CenterDot] b), "MatchingSide" -> 1, "Position" -> {2, 
          2}|>|>, {"CriticalPairLemma", 46} -> 
      <|"Statement" -> HoldForm[a == (a \[CenterDot] b) \[CenterDot] 
           ((c \[CenterDot] b) \[CenterDot] a)], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 53}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            (((c_) \[CenterDot] (a_)) \[CenterDot] (b_)) -> b, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 4}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {1}|>|>, 
     {"CriticalPairLemma", 47} -> <|"Statement" -> 
        HoldForm[a == (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
            (c \[CenterDot] b))], "Proof" -> 
        <|"Construct" -> {"CriticalPairLemma", 46}, "Orientation" -> -1, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            (((c_) \[CenterDot] (b_)) \[CenterDot] (a_)) -> a, "Side" -> 1, 
         "Subpattern" -> ((c_) \[CenterDot] (b_)) \[CenterDot] (a_), 
         "MatchingConstruct" -> {"SubstitutionLemma", 4}, 
         "MatchingOrientation" -> 1, "MatchingRule" -> 
          (a_) \[CenterDot] (b_) -> b \[CenterDot] a, "MatchingSide" -> 1, 
         "Position" -> {2}|>|>, {"SubstitutionLemma", 64} -> 
      <|"Statement" -> HoldForm[(((a \[CenterDot] b) \[CenterDot] 
             c) \[CenterDot] b) \[CenterDot] a == a \[CenterDot] 
           (a \[CenterDot] b)], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 45}, "Construct" -> 
          {"CriticalPairLemma", 47}, "Position" -> {2}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
             ((c_) \[CenterDot] (b_))) -> a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[(((a \[CenterDot] b) \[CenterDot] 
               c) \[CenterDot] b) \[CenterDot] a == a \[CenterDot] 
             (a \[CenterDot] b)], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 65} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
             c) \[CenterDot] b) == a \[CenterDot] (a \[CenterDot] b)], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 64}, 
         "Construct" -> {"SubstitutionLemma", 4}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           a \[CenterDot] (((a \[CenterDot] b) \[CenterDot] c) \[CenterDot] 
              b) == a \[CenterDot] (a \[CenterDot] b)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 66} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
            ((a \[CenterDot] b) \[CenterDot] c)) == a \[CenterDot] 
           (a \[CenterDot] b)], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 65}, "Construct" -> 
          {"SubstitutionLemma", 4}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           a \[CenterDot] (b \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
               c)) == a \[CenterDot] (a \[CenterDot] b)], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 48} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] a) == 
          a \[CenterDot] (b \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             c))], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 51}, 
         "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             (((b_) \[CenterDot] (a_)) \[CenterDot] (c_))) -> 
           a \[CenterDot] (b \[CenterDot] a), "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] (a_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 4}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {2, 2, 1}|>|>, 
     {"SubstitutionLemma", 67} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (b \[CenterDot] a) == a \[CenterDot] 
           (a \[CenterDot] b)], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 66}, "Construct" -> 
          {"CriticalPairLemma", 48}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             (((a_) \[CenterDot] (b_)) \[CenterDot] (c_))) -> 
           a \[CenterDot] (b \[CenterDot] a), "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[a \[CenterDot] (b \[CenterDot] a) == 
            a \[CenterDot] (a \[CenterDot] b)], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 49} -> <|"Statement" -> 
        HoldForm[b == (a \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
            (c \[CenterDot] a))], "Proof" -> 
        <|"Construct" -> {"SubstitutionLemma", 53}, "Orientation" -> -1, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            (((c_) \[CenterDot] (a_)) \[CenterDot] (b_)) -> b, "Side" -> 1, 
         "Subpattern" -> ((c_) \[CenterDot] (a_)) \[CenterDot] (b_), 
         "MatchingConstruct" -> {"SubstitutionLemma", 4}, 
         "MatchingOrientation" -> 1, "MatchingRule" -> 
          (a_) \[CenterDot] (b_) -> b \[CenterDot] a, "MatchingSide" -> 1, 
         "Position" -> {2}|>|>, {"CriticalPairLemma", 50} -> 
      <|"Statement" -> HoldForm[
         ((a \[CenterDot] (b \[CenterDot] c)) \[CenterDot] c) \[CenterDot] 
           a == a \[CenterDot] ((a \[CenterDot] (b \[CenterDot] 
              c)) \[CenterDot] a)], "Proof" -> 
        <|"Construct" -> {"SubstitutionLemma", 44}, "Orientation" -> -1, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             (((c_) \[CenterDot] (a_)) \[CenterDot] (b_))) -> 
           (b \[CenterDot] c) \[CenterDot] a, "Side" -> 1, 
         "Subpattern" -> ((c_) \[CenterDot] (a_)) \[CenterDot] (b_), 
         "MatchingConstruct" -> {"CriticalPairLemma", 49}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          ((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
             ((c_) \[CenterDot] (a_))) -> b, "MatchingSide" -> 1, 
         "Position" -> {2, 2}|>|>, {"SubstitutionLemma", 69} -> 
      <|"Statement" -> HoldForm[
         ((a \[CenterDot] (b \[CenterDot] c)) \[CenterDot] c) \[CenterDot] 
           a == a \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
             (b \[CenterDot] c)))], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 50}, "Construct" -> 
          {"SubstitutionLemma", 4}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           ((a \[CenterDot] (b \[CenterDot] c)) \[CenterDot] c) \[CenterDot] 
             a == a \[CenterDot] (a \[CenterDot] (a \[CenterDot] (
                b \[CenterDot] c)))], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 70} -> <|"Statement" -> 
        HoldForm[((a \[CenterDot] (b \[CenterDot] c)) \[CenterDot] 
            c) \[CenterDot] a == a \[CenterDot] (b \[CenterDot] c)], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 69}, 
         "Construct" -> {"SubstitutionLemma", 38}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
              (b_))) -> a \[CenterDot] b, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[((a \[CenterDot] (b \[CenterDot] 
                c)) \[CenterDot] c) \[CenterDot] a == a \[CenterDot] 
             (b \[CenterDot] c)], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 71} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] ((a \[CenterDot] (b \[CenterDot] 
              c)) \[CenterDot] c) == a \[CenterDot] (b \[CenterDot] c)], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 70}, 
         "Construct" -> {"SubstitutionLemma", 4}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           a \[CenterDot] ((a \[CenterDot] (b \[CenterDot] c)) \[CenterDot] 
              c) == a \[CenterDot] (b \[CenterDot] c)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 72} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (c \[CenterDot] 
            (a \[CenterDot] (b \[CenterDot] c))) == a \[CenterDot] 
           (b \[CenterDot] c)], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 71}, "Construct" -> 
          {"SubstitutionLemma", 4}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           a \[CenterDot] (c \[CenterDot] (a \[CenterDot] (b \[CenterDot] 
                c))) == a \[CenterDot] (b \[CenterDot] c)], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 51} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (c \[CenterDot] b) == 
          a \[CenterDot] (b \[CenterDot] (a \[CenterDot] (b \[CenterDot] 
              c)))], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 72}, 
         "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             ((a_) \[CenterDot] ((c_) \[CenterDot] (b_)))) -> 
           a \[CenterDot] (c \[CenterDot] b), "Side" -> 1, 
         "Subpattern" -> (c_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 4}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {2, 2, 2}|>|>, 
     {"CriticalPairLemma", 52} -> <|"Statement" -> 
        HoldForm[a == (a \[CenterDot] (b \[CenterDot] c)) \[CenterDot] 
           (b \[CenterDot] a)], "Proof" -> 
        <|"Construct" -> {"CriticalPairLemma", 46}, "Orientation" -> -1, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            (((c_) \[CenterDot] (b_)) \[CenterDot] (a_)) -> a, "Side" -> 1, 
         "Subpattern" -> (c_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"CriticalPairLemma", 29}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] (b_)) -> a, "MatchingSide" -> 1, 
         "Position" -> {2, 1}|>|>, {"SubstitutionLemma", 73} -> 
      <|"Statement" -> HoldForm[a == (b \[CenterDot] a) \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] c))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 52}, 
         "Construct" -> {"SubstitutionLemma", 4}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           a == (b \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
              (b \[CenterDot] c))], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 53} -> <|"Statement" -> 
        HoldForm[((a \[CenterDot] (b \[CenterDot] c)) \[CenterDot] 
            b) \[CenterDot] a == a \[CenterDot] 
           ((a \[CenterDot] (b \[CenterDot] c)) \[CenterDot] a)], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 44}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             (((c_) \[CenterDot] (a_)) \[CenterDot] (b_))) -> 
           (b \[CenterDot] c) \[CenterDot] a, "Side" -> 1, 
         "Subpattern" -> ((c_) \[CenterDot] (a_)) \[CenterDot] (b_), 
         "MatchingConstruct" -> {"SubstitutionLemma", 73}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          ((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
             ((a_) \[CenterDot] (c_))) -> b, "MatchingSide" -> 1, 
         "Position" -> {2, 2}|>|>, {"SubstitutionLemma", 74} -> 
      <|"Statement" -> HoldForm[
         ((a \[CenterDot] (b \[CenterDot] c)) \[CenterDot] b) \[CenterDot] 
           a == a \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
             (b \[CenterDot] c)))], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 53}, "Construct" -> 
          {"SubstitutionLemma", 4}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           ((a \[CenterDot] (b \[CenterDot] c)) \[CenterDot] b) \[CenterDot] 
             a == a \[CenterDot] (a \[CenterDot] (a \[CenterDot] (
                b \[CenterDot] c)))], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 75} -> <|"Statement" -> 
        HoldForm[((a \[CenterDot] (b \[CenterDot] c)) \[CenterDot] 
            b) \[CenterDot] a == a \[CenterDot] (b \[CenterDot] c)], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 74}, 
         "Construct" -> {"SubstitutionLemma", 38}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
              (b_))) -> a \[CenterDot] b, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[((a \[CenterDot] (b \[CenterDot] 
                c)) \[CenterDot] b) \[CenterDot] a == a \[CenterDot] 
             (b \[CenterDot] c)], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 76} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] ((a \[CenterDot] (b \[CenterDot] 
              c)) \[CenterDot] b) == a \[CenterDot] (b \[CenterDot] c)], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 75}, 
         "Construct" -> {"SubstitutionLemma", 4}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           a \[CenterDot] ((a \[CenterDot] (b \[CenterDot] c)) \[CenterDot] 
              b) == a \[CenterDot] (b \[CenterDot] c)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 77} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
            (a \[CenterDot] (b \[CenterDot] c))) == a \[CenterDot] 
           (b \[CenterDot] c)], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 76}, "Construct" -> 
          {"SubstitutionLemma", 4}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           a \[CenterDot] (b \[CenterDot] (a \[CenterDot] (b \[CenterDot] 
                c))) == a \[CenterDot] (b \[CenterDot] c)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 78} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (c \[CenterDot] b) == 
          a \[CenterDot] (b \[CenterDot] c)], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 51}, "Construct" -> 
          {"SubstitutionLemma", 77}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] ((a_) \[CenterDot] 
              ((b_) \[CenterDot] (c_)))) -> a \[CenterDot] 
            (b \[CenterDot] c), "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[a \[CenterDot] (c \[CenterDot] b) == a \[CenterDot] 
             (b \[CenterDot] c)], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 63} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c)) == 
          a \[CenterDot] ((c \[CenterDot] b) \[CenterDot] a)], 
       "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Construct" -> 
          {"SubstitutionLemma", 62}, "Position" -> {}, 
         "Rule" -> (c_) \[CenterDot] ((c_) \[CenterDot] ((a_) \[CenterDot] 
              (b_))) -> a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a), 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c)) == 
            a \[CenterDot] ((c \[CenterDot] b) \[CenterDot] a)], 
         "Source" -> "cpl"|>|>, {"SubstitutionLemma", 68} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] c)) == a \[CenterDot] (a \[CenterDot] 
            (c \[CenterDot] b))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 63}, "Construct" -> 
          {"SubstitutionLemma", 67}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (a_)) -> 
           a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
              (b \[CenterDot] c)) == a \[CenterDot] (a \[CenterDot] 
              (c \[CenterDot] b))], "Source" -> "cpl"|>|>, 
     {"Conclusion", 1} -> <|"Statement" -> HoldForm[
         a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c)) == 
          a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 68}, 
         "Construct" -> {"SubstitutionLemma", 78}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (c_)) -> 
           a \[CenterDot] (c \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
              (b \[CenterDot] c)) == a \[CenterDot] (a \[CenterDot] 
              (b \[CenterDot] c))], "Source" -> "cpl"|>|>}|>]}
