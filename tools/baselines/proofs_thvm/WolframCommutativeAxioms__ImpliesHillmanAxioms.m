{ProofObject["EquationalLogic", Inactive[Equal][
   (a \[CenterDot] a) \[CenterDot] (a \[CenterDot] b), a], 
  {Inactive[Equal][(a_) \[CenterDot] (b_), (b_) \[CenterDot] (a_)], 
   Inactive[Equal][((a_) \[CenterDot] (b_)) \[CenterDot] 
     ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))), a_]}, 
  <|"Variables" -> {a, b, c}, "Constants" -> {}, 
   "Proof" -> {{"Axiom", 1} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] b == b \[CenterDot] a], "Proof" -> <||>|>, 
     {"Axiom", 2} -> <|"Statement" -> HoldForm[
         (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] (b \[CenterDot] 
             c)) == a], "Proof" -> <||>|>, {"Hypothesis", 1} -> 
      <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
           (a \[CenterDot] b) == a], "Proof" -> <||>|>, 
     {"CriticalPairLemma", 1} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] b == ((a \[CenterDot] b) \[CenterDot] 
            a) \[CenterDot] a], "Proof" -> <|"Construct" -> {"Axiom", 2}, 
         "Orientation" -> 1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) -> a, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] ((b_) \[CenterDot] (c_)), 
         "MatchingConstruct" -> {"Axiom", 2}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) -> a, 
         "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"SubstitutionLemma", 1} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] b == a \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] a)], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 1}, 
         "Construct" -> {"Axiom", 1}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[a \[CenterDot] b == 
            a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 2} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] b == a \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] b))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 1}, 
         "Construct" -> {"Axiom", 1}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[a \[CenterDot] b == 
            a \[CenterDot] (a \[CenterDot] (a \[CenterDot] b))], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 2} -> 
      <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
           (a \[CenterDot] b)], "Proof" -> <|"Construct" -> {"Axiom", 2}, 
         "Orientation" -> 1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) -> a, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] ((b_) \[CenterDot] (c_)), 
         "MatchingConstruct" -> {"SubstitutionLemma", 2}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] (b_))) -> 
           a \[CenterDot] b, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"Conclusion", 1} -> <|"Statement" -> HoldForm[a == a], 
       "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Construct" -> 
          {"CriticalPairLemma", 2}, "Position" -> {}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
             (b_)) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
          HoldForm[a == a], "Source" -> "cpl"|>|>}|>], 
 ProofObject["EquationalLogic", Inactive[Equal][
   a \[CenterDot] (a \[CenterDot] b), a \[CenterDot] (b \[CenterDot] b)], 
  {Inactive[Equal][(a_) \[CenterDot] (b_), (b_) \[CenterDot] (a_)], 
   Inactive[Equal][((a_) \[CenterDot] (b_)) \[CenterDot] 
     ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))), a_]}, 
  <|"Variables" -> {a, b, c, x3}, "Constants" -> {}, 
   "Proof" -> {{"Axiom", 1} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] b == b \[CenterDot] a], "Proof" -> <||>|>, 
     {"Axiom", 2} -> <|"Statement" -> HoldForm[
         (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] (b \[CenterDot] 
             c)) == a], "Proof" -> <||>|>, {"Hypothesis", 1} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] b) == 
          a \[CenterDot] (b \[CenterDot] b)], "Proof" -> <||>|>, 
     {"CriticalPairLemma", 1} -> <|"Statement" -> 
        HoldForm[a == (a \[CenterDot] b) \[CenterDot] 
           ((b \[CenterDot] c) \[CenterDot] a)], 
       "Proof" -> <|"Construct" -> {"Axiom", 2}, "Orientation" -> 1, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
             ((b_) \[CenterDot] (c_))) -> a, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] ((b_) \[CenterDot] (c_)), 
         "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"CriticalPairLemma", 2} -> <|"Statement" -> 
        HoldForm[b == (a \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
            (a \[CenterDot] c))], "Proof" -> <|"Construct" -> {"Axiom", 2}, 
         "Orientation" -> 1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) -> a, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"Axiom", 1}, "MatchingOrientation" -> 1, "MatchingRule" -> 
          (a_) \[CenterDot] (b_) -> b \[CenterDot] a, "MatchingSide" -> 1, 
         "Position" -> {1}|>|>, {"CriticalPairLemma", 3} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] c) == 
          ((a \[CenterDot] (b \[CenterDot] c)) \[CenterDot] b) \[CenterDot] 
           a], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 1}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            (((b_) \[CenterDot] (c_)) \[CenterDot] (a_)) -> a, "Side" -> 1, 
         "Subpattern" -> ((b_) \[CenterDot] (c_)) \[CenterDot] (a_), 
         "MatchingConstruct" -> {"CriticalPairLemma", 2}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          ((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
             ((a_) \[CenterDot] (c_))) -> b, "MatchingSide" -> 1, 
         "Position" -> {2}|>|>, {"SubstitutionLemma", 1} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] c) == 
          a \[CenterDot] ((a \[CenterDot] (b \[CenterDot] c)) \[CenterDot] 
            b)], "Proof" -> <|"Input" -> {"CriticalPairLemma", 3}, 
         "Construct" -> {"Axiom", 1}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           a \[CenterDot] (b \[CenterDot] c) == a \[CenterDot] 
             ((a \[CenterDot] (b \[CenterDot] c)) \[CenterDot] b)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 2} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] c) == 
          a \[CenterDot] (b \[CenterDot] (a \[CenterDot] (b \[CenterDot] 
              c)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 1}, 
         "Construct" -> {"Axiom", 1}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           a \[CenterDot] (b \[CenterDot] c) == a \[CenterDot] 
             (b \[CenterDot] (a \[CenterDot] (b \[CenterDot] c)))], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 4} -> 
      <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
           ((a \[CenterDot] c) \[CenterDot] b)], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 2}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((b_) \[CenterDot] ((a_) \[CenterDot] (c_))) -> b, "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] ((a_) \[CenterDot] (c_)), 
         "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"CriticalPairLemma", 5} -> <|"Statement" -> 
        HoldForm[(a \[CenterDot] b) \[CenterDot] c == 
          (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] c)) \[CenterDot] 
           c], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 4}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            (((a_) \[CenterDot] (c_)) \[CenterDot] (b_)) -> b, "Side" -> 1, 
         "Subpattern" -> ((a_) \[CenterDot] (c_)) \[CenterDot] (b_), 
         "MatchingConstruct" -> {"CriticalPairLemma", 4}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          ((a_) \[CenterDot] (b_)) \[CenterDot] (((a_) \[CenterDot] 
              (c_)) \[CenterDot] (b_)) -> b, "MatchingSide" -> 1, 
         "Position" -> {2}|>|>, {"SubstitutionLemma", 3} -> 
      <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] c == 
          c \[CenterDot] (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             c))], "Proof" -> <|"Input" -> {"CriticalPairLemma", 5}, 
         "Construct" -> {"Axiom", 1}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           (a \[CenterDot] b) \[CenterDot] c == c \[CenterDot] 
             (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] c))], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 6} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] b == 
          ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] a], 
       "Proof" -> <|"Construct" -> {"Axiom", 2}, "Orientation" -> 1, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
             ((b_) \[CenterDot] (c_))) -> a, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] ((b_) \[CenterDot] (c_)), 
         "MatchingConstruct" -> {"Axiom", 2}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) -> a, 
         "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"SubstitutionLemma", 4} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] b == a \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] a)], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 6}, 
         "Construct" -> {"Axiom", 1}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[a \[CenterDot] b == 
            a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 5} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] b == a \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] b))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 4}, 
         "Construct" -> {"Axiom", 1}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[a \[CenterDot] b == 
            a \[CenterDot] (a \[CenterDot] (a \[CenterDot] b))], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 7} -> 
      <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
           (a \[CenterDot] b)], "Proof" -> <|"Construct" -> {"Axiom", 2}, 
         "Orientation" -> 1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) -> a, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] ((b_) \[CenterDot] (c_)), 
         "MatchingConstruct" -> {"SubstitutionLemma", 5}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] (b_))) -> 
           a \[CenterDot] b, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"CriticalPairLemma", 8} -> <|"Statement" -> 
        HoldForm[a == (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] a)], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 7}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] (b_)) -> a, "Side" -> 1, "Subpattern" -> {}, 
         "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {}|>|>, 
     {"CriticalPairLemma", 9} -> <|"Statement" -> 
        HoldForm[b == (a \[CenterDot] b) \[CenterDot] (b \[CenterDot] b)], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 8}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((a_) \[CenterDot] (a_)) -> a, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"Axiom", 1}, "MatchingOrientation" -> 1, "MatchingRule" -> 
          (a_) \[CenterDot] (b_) -> b \[CenterDot] a, "MatchingSide" -> 1, 
         "Position" -> {1}|>|>, {"CriticalPairLemma", 10} -> 
      <|"Statement" -> HoldForm[c == ((a \[CenterDot] b) \[CenterDot] 
            c) \[CenterDot] (c \[CenterDot] b)], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 2}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((b_) \[CenterDot] ((a_) \[CenterDot] (c_))) -> b, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (c_), "MatchingConstruct" -> 
          {"CriticalPairLemma", 9}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((b_) \[CenterDot] (b_)) -> b, "MatchingSide" -> 1, 
         "Position" -> {2, 2}|>|>, {"SubstitutionLemma", 6} -> 
      <|"Statement" -> HoldForm[c == (c \[CenterDot] b) \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] c)], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 10}, 
         "Construct" -> {"Axiom", 1}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           c == (c \[CenterDot] b) \[CenterDot] ((a \[CenterDot] 
               b) \[CenterDot] c)], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 11} -> <|"Statement" -> 
        HoldForm[((a \[CenterDot] b) \[CenterDot] b) \[CenterDot] a == 
          a \[CenterDot] a], "Proof" -> <|"Construct" -> 
          {"SubstitutionLemma", 3}, "Orientation" -> -1, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             (((b_) \[CenterDot] (c_)) \[CenterDot] (a_))) -> 
           (b \[CenterDot] c) \[CenterDot] a, "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] (((b_) \[CenterDot] 
             (c_)) \[CenterDot] (a_)), "MatchingConstruct" -> 
          {"SubstitutionLemma", 6}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            (((c_) \[CenterDot] (b_)) \[CenterDot] (a_)) -> a, 
         "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"SubstitutionLemma", 7} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] b) == 
          a \[CenterDot] a], "Proof" -> <|"Input" -> {"CriticalPairLemma", 
           11}, "Construct" -> {"Axiom", 1}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] b) == 
            a \[CenterDot] a], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 8} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (b \[CenterDot] (a \[CenterDot] b)) == 
          a \[CenterDot] a], "Proof" -> <|"Input" -> {"SubstitutionLemma", 
           7}, "Construct" -> {"Axiom", 1}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           a \[CenterDot] (b \[CenterDot] (a \[CenterDot] b)) == 
            a \[CenterDot] a], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 12} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (b \[CenterDot] a) == a \[CenterDot] 
           (b \[CenterDot] b)], "Proof" -> 
        <|"Construct" -> {"SubstitutionLemma", 2}, "Orientation" -> -1, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] ((a_) \[CenterDot] 
              ((b_) \[CenterDot] (c_)))) -> a \[CenterDot] 
            (b \[CenterDot] c), "Side" -> 1, "Subpattern" -> 
          (b_) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))), 
         "MatchingConstruct" -> {"SubstitutionLemma", 8}, 
         "MatchingOrientation" -> 1, "MatchingRule" -> 
          (a_) \[CenterDot] ((b_) \[CenterDot] ((a_) \[CenterDot] (b_))) -> 
           a \[CenterDot] a, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"CriticalPairLemma", 13} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (b \[CenterDot] c) == a \[CenterDot] 
           (b \[CenterDot] (a \[CenterDot] (c \[CenterDot] b)))], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 2}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             ((a_) \[CenterDot] ((b_) \[CenterDot] (c_)))) -> 
           a \[CenterDot] (b \[CenterDot] c), "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] (c_), "MatchingConstruct" -> 
          {"Axiom", 1}, "MatchingOrientation" -> 1, "MatchingRule" -> 
          (a_) \[CenterDot] (b_) -> b \[CenterDot] a, "MatchingSide" -> 1, 
         "Position" -> {2, 2, 2}|>|>, {"CriticalPairLemma", 14} -> 
      <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
           (b \[CenterDot] (c \[CenterDot] a))], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 2}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((b_) \[CenterDot] ((a_) \[CenterDot] (c_))) -> b, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (c_), "MatchingConstruct" -> 
          {"Axiom", 1}, "MatchingOrientation" -> 1, "MatchingRule" -> 
          (a_) \[CenterDot] (b_) -> b \[CenterDot] a, "MatchingSide" -> 1, 
         "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 15} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] c) == 
          ((a \[CenterDot] (b \[CenterDot] c)) \[CenterDot] c) \[CenterDot] 
           a], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 1}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            (((b_) \[CenterDot] (c_)) \[CenterDot] (a_)) -> a, "Side" -> 1, 
         "Subpattern" -> ((b_) \[CenterDot] (c_)) \[CenterDot] (a_), 
         "MatchingConstruct" -> {"CriticalPairLemma", 14}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          ((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
             ((c_) \[CenterDot] (a_))) -> b, "MatchingSide" -> 1, 
         "Position" -> {2}|>|>, {"SubstitutionLemma", 10} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] c) == 
          a \[CenterDot] ((a \[CenterDot] (b \[CenterDot] c)) \[CenterDot] 
            c)], "Proof" -> <|"Input" -> {"CriticalPairLemma", 15}, 
         "Construct" -> {"Axiom", 1}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           a \[CenterDot] (b \[CenterDot] c) == a \[CenterDot] 
             ((a \[CenterDot] (b \[CenterDot] c)) \[CenterDot] c)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 11} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] c) == 
          a \[CenterDot] (c \[CenterDot] (a \[CenterDot] (b \[CenterDot] 
              c)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 10}, 
         "Construct" -> {"Axiom", 1}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           a \[CenterDot] (b \[CenterDot] c) == a \[CenterDot] 
             (c \[CenterDot] (a \[CenterDot] (b \[CenterDot] c)))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 12} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] c) == 
          a \[CenterDot] (c \[CenterDot] b)], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 13}, "Construct" -> 
          {"SubstitutionLemma", 11}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] ((a_) \[CenterDot] 
              ((c_) \[CenterDot] (b_)))) -> a \[CenterDot] 
            (c \[CenterDot] b), "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[a \[CenterDot] (b \[CenterDot] c) == a \[CenterDot] 
             (c \[CenterDot] b)], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 9} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (a \[CenterDot] b) == a \[CenterDot] 
           (b \[CenterDot] a)], "Proof" -> <|"Input" -> {"Hypothesis", 1}, 
         "Construct" -> {"CriticalPairLemma", 12}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (b_)) -> 
           a \[CenterDot] (b \[CenterDot] a), "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] (a \[CenterDot] b) == 
            a \[CenterDot] (b \[CenterDot] a)], "Source" -> "cpl"|>|>, 
     {"Conclusion", 1} -> <|"Statement" -> HoldForm[
         a \[CenterDot] (a \[CenterDot] b) == a \[CenterDot] 
           (a \[CenterDot] b)], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 9}, "Construct" -> 
          {"SubstitutionLemma", 12}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (c_)) -> 
           a \[CenterDot] (c \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] (a \[CenterDot] b) == 
            a \[CenterDot] (a \[CenterDot] b)], "Source" -> "cpl"|>|>}|>], 
 ProofObject["EquationalLogic", Inactive[Equal][
   a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c)), 
   b \[CenterDot] (b \[CenterDot] (a \[CenterDot] c))], 
  {Inactive[Equal][(a_) \[CenterDot] (b_), (b_) \[CenterDot] (a_)], 
   Inactive[Equal][((a_) \[CenterDot] (b_)) \[CenterDot] 
     ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))), a_]}, 
  <|"Variables" -> {a, b, c, x3}, "Constants" -> {}, 
   "Proof" -> {{"Axiom", 1} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] b == b \[CenterDot] a], "Proof" -> <||>|>, 
     {"Axiom", 2} -> <|"Statement" -> HoldForm[
         (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] (b \[CenterDot] 
             c)) == a], "Proof" -> <||>|>, {"Hypothesis", 1} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] c)) == b \[CenterDot] (b \[CenterDot] 
            (a \[CenterDot] c))], "Proof" -> <||>|>, 
     {"CriticalPairLemma", 1} -> <|"Statement" -> 
        HoldForm[b == (a \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
            (a \[CenterDot] c))], "Proof" -> <|"Construct" -> {"Axiom", 2}, 
         "Orientation" -> 1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) -> a, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"Axiom", 1}, "MatchingOrientation" -> 1, "MatchingRule" -> 
          (a_) \[CenterDot] (b_) -> b \[CenterDot] a, "MatchingSide" -> 1, 
         "Position" -> {1}|>|>, {"CriticalPairLemma", 2} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] b == 
          ((a \[CenterDot] b) \[CenterDot] a) \[CenterDot] a], 
       "Proof" -> <|"Construct" -> {"Axiom", 2}, "Orientation" -> 1, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
             ((b_) \[CenterDot] (c_))) -> a, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] ((b_) \[CenterDot] (c_)), 
         "MatchingConstruct" -> {"Axiom", 2}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) -> a, 
         "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"SubstitutionLemma", 1} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] b == a \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] a)], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 2}, 
         "Construct" -> {"Axiom", 1}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[a \[CenterDot] b == 
            a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 2} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] b == a \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] b))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 1}, 
         "Construct" -> {"Axiom", 1}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[a \[CenterDot] b == 
            a \[CenterDot] (a \[CenterDot] (a \[CenterDot] b))], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 3} -> 
      <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
           (a \[CenterDot] b)], "Proof" -> <|"Construct" -> {"Axiom", 2}, 
         "Orientation" -> 1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) -> a, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] ((b_) \[CenterDot] (c_)), 
         "MatchingConstruct" -> {"SubstitutionLemma", 2}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] (b_))) -> 
           a \[CenterDot] b, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"CriticalPairLemma", 4} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] a == a \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] b)], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 3}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] (b_)) -> a, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (a_), "MatchingConstruct" -> 
          {"CriticalPairLemma", 3}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] (b_)) -> a, "MatchingSide" -> 1, 
         "Position" -> {1}|>|>, {"CriticalPairLemma", 5} -> 
      <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] b == 
          (a \[CenterDot] a) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
             b) \[CenterDot] (a \[CenterDot] c))], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 1}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((b_) \[CenterDot] ((a_) \[CenterDot] (c_))) -> b, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"CriticalPairLemma", 4}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> (a_) \[CenterDot] (((a_) \[CenterDot] 
              (a_)) \[CenterDot] (b_)) -> a \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {1}|>|>, 
     {"CriticalPairLemma", 6} -> <|"Statement" -> 
        HoldForm[a == (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
            (c \[CenterDot] b))], "Proof" -> <|"Construct" -> {"Axiom", 2}, 
         "Orientation" -> 1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) -> a, "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] (c_), "MatchingConstruct" -> 
          {"Axiom", 1}, "MatchingOrientation" -> 1, "MatchingRule" -> 
          (a_) \[CenterDot] (b_) -> b \[CenterDot] a, "MatchingSide" -> 1, 
         "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 7} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] b == 
          ((a \[CenterDot] b) \[CenterDot] (c \[CenterDot] b)) \[CenterDot] 
           a], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 6}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((a_) \[CenterDot] ((c_) \[CenterDot] (b_))) -> a, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] ((c_) \[CenterDot] (b_)), 
         "MatchingConstruct" -> {"CriticalPairLemma", 6}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          ((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
             ((c_) \[CenterDot] (b_))) -> a, "MatchingSide" -> 1, 
         "Position" -> {2}|>|>, {"SubstitutionLemma", 3} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] b == a \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] (c \[CenterDot] b))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 7}, 
         "Construct" -> {"Axiom", 1}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[a \[CenterDot] b == 
            a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (c \[CenterDot] 
               b))], "Source" -> "norm"|>|>, {"CriticalPairLemma", 8} -> 
      <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
           (b \[CenterDot] (c \[CenterDot] a))], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 1}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((b_) \[CenterDot] ((a_) \[CenterDot] (c_))) -> b, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (c_), "MatchingConstruct" -> 
          {"Axiom", 1}, "MatchingOrientation" -> 1, "MatchingRule" -> 
          (a_) \[CenterDot] (b_) -> b \[CenterDot] a, "MatchingSide" -> 1, 
         "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 9} -> 
      <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
           (b \[CenterDot] (x3 \[CenterDot] a)) == 
          (a \[CenterDot] b) \[CenterDot] (b \[CenterDot] (c \[CenterDot] 
             (b \[CenterDot] (x3 \[CenterDot] a))))], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 3}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] 
            (((a_) \[CenterDot] (b_)) \[CenterDot] ((c_) \[CenterDot] 
              (b_))) -> a \[CenterDot] b, "Side" -> 1, "Subpattern" -> 
          (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"CriticalPairLemma", 8}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((b_) \[CenterDot] ((c_) \[CenterDot] (a_))) -> b, 
         "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
     {"SubstitutionLemma", 4} -> <|"Statement" -> 
        HoldForm[b == (a \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
            (c \[CenterDot] (b \[CenterDot] (x3 \[CenterDot] a))))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 9}, 
         "Construct" -> {"CriticalPairLemma", 8}, "Position" -> {}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
             ((c_) \[CenterDot] (a_))) -> b, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
             (b \[CenterDot] (c \[CenterDot] (b \[CenterDot] (x3 \[CenterDot] 
                 a))))], "Source" -> "norm"|>|>, {"CriticalPairLemma", 10} -> 
      <|"Statement" -> HoldForm[((a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b)) \[CenterDot] c == 
          ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
           ((((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
               b)) \[CenterDot] c) \[CenterDot] b)], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 5}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((((a_) \[CenterDot] (a_)) \[CenterDot] (b_)) \[CenterDot] 
             ((a_) \[CenterDot] (c_))) -> (a \[CenterDot] a) \[CenterDot] b, 
         "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] (c_), 
         "MatchingConstruct" -> {"SubstitutionLemma", 4}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          ((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
             ((c_) \[CenterDot] ((b_) \[CenterDot] ((x3_) \[CenterDot] 
                (a_))))) -> b, "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
     {"CriticalPairLemma", 11} -> <|"Statement" -> 
        HoldForm[a == (a \[CenterDot] b) \[CenterDot] 
           ((b \[CenterDot] c) \[CenterDot] a)], 
       "Proof" -> <|"Construct" -> {"Axiom", 2}, "Orientation" -> 1, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
             ((b_) \[CenterDot] (c_))) -> a, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] ((b_) \[CenterDot] (c_)), 
         "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"CriticalPairLemma", 12} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (b \[CenterDot] c) == 
          ((a \[CenterDot] (b \[CenterDot] c)) \[CenterDot] b) \[CenterDot] 
           a], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 11}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            (((b_) \[CenterDot] (c_)) \[CenterDot] (a_)) -> a, "Side" -> 1, 
         "Subpattern" -> ((b_) \[CenterDot] (c_)) \[CenterDot] (a_), 
         "MatchingConstruct" -> {"CriticalPairLemma", 1}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          ((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
             ((a_) \[CenterDot] (c_))) -> b, "MatchingSide" -> 1, 
         "Position" -> {2}|>|>, {"SubstitutionLemma", 5} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] c) == 
          a \[CenterDot] ((a \[CenterDot] (b \[CenterDot] c)) \[CenterDot] 
            b)], "Proof" -> <|"Input" -> {"CriticalPairLemma", 12}, 
         "Construct" -> {"Axiom", 1}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           a \[CenterDot] (b \[CenterDot] c) == a \[CenterDot] 
             ((a \[CenterDot] (b \[CenterDot] c)) \[CenterDot] b)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 6} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] c) == 
          a \[CenterDot] (b \[CenterDot] (a \[CenterDot] (b \[CenterDot] 
              c)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 5}, 
         "Construct" -> {"Axiom", 1}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           a \[CenterDot] (b \[CenterDot] c) == a \[CenterDot] 
             (b \[CenterDot] (a \[CenterDot] (b \[CenterDot] c)))], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 13} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] c) == 
          a \[CenterDot] (b \[CenterDot] (a \[CenterDot] (c \[CenterDot] 
              b)))], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 6}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             ((a_) \[CenterDot] ((b_) \[CenterDot] (c_)))) -> 
           a \[CenterDot] (b \[CenterDot] c), "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] (c_), "MatchingConstruct" -> 
          {"Axiom", 1}, "MatchingOrientation" -> 1, "MatchingRule" -> 
          (a_) \[CenterDot] (b_) -> b \[CenterDot] a, "MatchingSide" -> 1, 
         "Position" -> {2, 2, 2}|>|>, {"CriticalPairLemma", 14} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] c) == 
          ((a \[CenterDot] (b \[CenterDot] c)) \[CenterDot] c) \[CenterDot] 
           a], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 11}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            (((b_) \[CenterDot] (c_)) \[CenterDot] (a_)) -> a, "Side" -> 1, 
         "Subpattern" -> ((b_) \[CenterDot] (c_)) \[CenterDot] (a_), 
         "MatchingConstruct" -> {"CriticalPairLemma", 8}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          ((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
             ((c_) \[CenterDot] (a_))) -> b, "MatchingSide" -> 1, 
         "Position" -> {2}|>|>, {"SubstitutionLemma", 7} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] c) == 
          a \[CenterDot] ((a \[CenterDot] (b \[CenterDot] c)) \[CenterDot] 
            c)], "Proof" -> <|"Input" -> {"CriticalPairLemma", 14}, 
         "Construct" -> {"Axiom", 1}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           a \[CenterDot] (b \[CenterDot] c) == a \[CenterDot] 
             ((a \[CenterDot] (b \[CenterDot] c)) \[CenterDot] c)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 8} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] c) == 
          a \[CenterDot] (c \[CenterDot] (a \[CenterDot] (b \[CenterDot] 
              c)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 7}, 
         "Construct" -> {"Axiom", 1}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           a \[CenterDot] (b \[CenterDot] c) == a \[CenterDot] 
             (c \[CenterDot] (a \[CenterDot] (b \[CenterDot] c)))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 9} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] c) == 
          a \[CenterDot] (c \[CenterDot] b)], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 13}, "Construct" -> 
          {"SubstitutionLemma", 8}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] ((a_) \[CenterDot] 
              ((c_) \[CenterDot] (b_)))) -> a \[CenterDot] 
            (c \[CenterDot] b), "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[a \[CenterDot] (b \[CenterDot] c) == a \[CenterDot] 
             (c \[CenterDot] b)], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 10} -> <|"Statement" -> 
        HoldForm[((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
             b)) \[CenterDot] c == ((a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b)) \[CenterDot] (b \[CenterDot] 
            (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
               b)) \[CenterDot] c))], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 10}, "Construct" -> 
          {"SubstitutionLemma", 9}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (c_)) -> 
           a \[CenterDot] (c \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[((a \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] b)) \[CenterDot] c == 
            ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
             (b \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
                (a \[CenterDot] b)) \[CenterDot] c))], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 15} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] ((a \[CenterDot] 
             b) \[CenterDot] b) == a \[CenterDot] a], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 6}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             ((a_) \[CenterDot] ((b_) \[CenterDot] (c_)))) -> 
           a \[CenterDot] (b \[CenterDot] c), "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] ((a_) \[CenterDot] 
            ((b_) \[CenterDot] (c_))), "MatchingConstruct" -> 
          {"CriticalPairLemma", 6}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((a_) \[CenterDot] ((c_) \[CenterDot] (b_))) -> a, 
         "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"SubstitutionLemma", 11} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (b \[CenterDot] (a \[CenterDot] b)) == 
          a \[CenterDot] a], "Proof" -> <|"Input" -> {"CriticalPairLemma", 
           15}, "Construct" -> {"Axiom", 1}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           a \[CenterDot] (b \[CenterDot] (a \[CenterDot] b)) == 
            a \[CenterDot] a], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 16} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] a == a \[CenterDot] 
           ((b \[CenterDot] (a \[CenterDot] (c \[CenterDot] b))) \[CenterDot] 
            (a \[CenterDot] (c \[CenterDot] b)))], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 11}, 
         "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             ((a_) \[CenterDot] (b_))) -> a \[CenterDot] a, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 8}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             ((a_) \[CenterDot] ((c_) \[CenterDot] (b_)))) -> 
           a \[CenterDot] (c \[CenterDot] b), "MatchingSide" -> 1, 
         "Position" -> {2, 2}|>|>, {"SubstitutionLemma", 12} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] a == a \[CenterDot] 
           ((a \[CenterDot] (c \[CenterDot] b)) \[CenterDot] 
            (b \[CenterDot] (a \[CenterDot] (c \[CenterDot] b))))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 16}, 
         "Construct" -> {"SubstitutionLemma", 9}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (c_)) -> 
           a \[CenterDot] (c \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] a == 
            a \[CenterDot] ((a \[CenterDot] (c \[CenterDot] b)) \[CenterDot] 
              (b \[CenterDot] (a \[CenterDot] (c \[CenterDot] b))))], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 17} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] a) == 
          a \[CenterDot] (b \[CenterDot] b)], "Proof" -> 
        <|"Construct" -> {"SubstitutionLemma", 6}, "Orientation" -> -1, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] ((a_) \[CenterDot] 
              ((b_) \[CenterDot] (c_)))) -> a \[CenterDot] 
            (b \[CenterDot] c), "Side" -> 1, "Subpattern" -> 
          (b_) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))), 
         "MatchingConstruct" -> {"SubstitutionLemma", 11}, 
         "MatchingOrientation" -> 1, "MatchingRule" -> 
          (a_) \[CenterDot] ((b_) \[CenterDot] ((a_) \[CenterDot] (b_))) -> 
           a \[CenterDot] a, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"SubstitutionLemma", 13} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] a == a \[CenterDot] 
           ((a \[CenterDot] (c \[CenterDot] b)) \[CenterDot] 
            (b \[CenterDot] b))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 12}, "Construct" -> 
          {"CriticalPairLemma", 17}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (a_)) -> 
           a \[CenterDot] (b \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] a == 
            a \[CenterDot] ((a \[CenterDot] (c \[CenterDot] b)) \[CenterDot] 
              (b \[CenterDot] b))], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 18} -> <|"Statement" -> 
        HoldForm[a == (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] a)], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 3}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] (b_)) -> a, "Side" -> 1, "Subpattern" -> {}, 
         "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {}|>|>, 
     {"CriticalPairLemma", 19} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] b == a \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             c))], "Proof" -> <|"Construct" -> {"Axiom", 2}, 
         "Orientation" -> 1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) -> a, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"CriticalPairLemma", 18}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((a_) \[CenterDot] (a_)) -> a, "MatchingSide" -> 1, 
         "Position" -> {1}|>|>, {"CriticalPairLemma", 20} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] b == a \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] a))], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 2}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
             ((a_) \[CenterDot] (b_))) -> a \[CenterDot] b, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"Axiom", 1}, "MatchingOrientation" -> 1, "MatchingRule" -> 
          (a_) \[CenterDot] (b_) -> b \[CenterDot] a, "MatchingSide" -> 1, 
         "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 21} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] ((b \[CenterDot] 
             b) \[CenterDot] a) == a \[CenterDot] b], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 17}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             (b_)) -> a \[CenterDot] (b \[CenterDot] a), "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"CriticalPairLemma", 3}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] (b_)) -> a, "MatchingSide" -> 1, 
         "Position" -> {2}|>|>, {"CriticalPairLemma", 22} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] b) == 
          a \[CenterDot] (a \[CenterDot] b)], "Proof" -> 
        <|"Construct" -> {"CriticalPairLemma", 20}, "Orientation" -> -1, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] 
              (a_))) -> a \[CenterDot] b, "Side" -> 1, "Subpattern" -> 
          (a_) \[CenterDot] ((b_) \[CenterDot] (a_)), "MatchingConstruct" -> 
          {"CriticalPairLemma", 21}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (a_) \[CenterDot] (((b_) \[CenterDot] 
              (b_)) \[CenterDot] (a_)) -> a \[CenterDot] b, 
         "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"CriticalPairLemma", 23} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (a \[CenterDot] b) == a \[CenterDot] 
           ((a \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
            ((a \[CenterDot] a) \[CenterDot] c))], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 19}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] 
            (((a_) \[CenterDot] (b_)) \[CenterDot] (((a_) \[CenterDot] (
                a_)) \[CenterDot] (c_))) -> a \[CenterDot] b, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"CriticalPairLemma", 22}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> (a_) \[CenterDot] ((a_) \[CenterDot] (b_)) -> 
           a \[CenterDot] (b \[CenterDot] b), "MatchingSide" -> 1, 
         "Position" -> {2, 1}|>|>, {"SubstitutionLemma", 14} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] b) == 
          a \[CenterDot] (b \[CenterDot] b)], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 23}, "Construct" -> 
          {"CriticalPairLemma", 19}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (c_))) -> 
           a \[CenterDot] b, "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[a \[CenterDot] (a \[CenterDot] b) == a \[CenterDot] 
             (b \[CenterDot] b)], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 24} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] a == a \[CenterDot] 
           ((a \[CenterDot] (b \[CenterDot] (b \[CenterDot] c))) \[CenterDot] 
            ((c \[CenterDot] c) \[CenterDot] (c \[CenterDot] c)))], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 13}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] 
            (((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) \[CenterDot] 
             ((c_) \[CenterDot] (c_))) -> a \[CenterDot] a, "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] (c_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 14}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (b_)) -> 
           a \[CenterDot] (a \[CenterDot] b), "MatchingSide" -> 1, 
         "Position" -> {2, 1, 2}|>|>, {"SubstitutionLemma", 15} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] a == a \[CenterDot] 
           ((a \[CenterDot] (b \[CenterDot] (b \[CenterDot] c))) \[CenterDot] 
            c)], "Proof" -> <|"Input" -> {"CriticalPairLemma", 24}, 
         "Construct" -> {"CriticalPairLemma", 3}, "Position" -> {2, 2}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
             (b_)) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[a \[CenterDot] a == a \[CenterDot] 
             ((a \[CenterDot] (b \[CenterDot] (b \[CenterDot] 
                 c))) \[CenterDot] c)], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 16} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] a == a \[CenterDot] (c \[CenterDot] 
            (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] c))))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 15}, 
         "Construct" -> {"SubstitutionLemma", 9}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (c_)) -> 
           a \[CenterDot] (c \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] a == 
            a \[CenterDot] (c \[CenterDot] (a \[CenterDot] (b \[CenterDot] 
                (b \[CenterDot] c))))], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 25} -> <|"Statement" -> 
        HoldForm[a == (a \[CenterDot] a) \[CenterDot] (b \[CenterDot] a)], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 3}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] (b_)) -> a, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"Axiom", 1}, "MatchingOrientation" -> 1, "MatchingRule" -> 
          (a_) \[CenterDot] (b_) -> b \[CenterDot] a, "MatchingSide" -> 1, 
         "Position" -> {2}|>|>, {"CriticalPairLemma", 26} -> 
      <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] b == 
          (((a \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
            ((a \[CenterDot] a) \[CenterDot] b)) \[CenterDot] 
           (b \[CenterDot] a)], "Proof" -> 
        <|"Construct" -> {"CriticalPairLemma", 25}, "Orientation" -> -1, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
             (a_)) -> a, "Side" -> 1, "Subpattern" -> (b_) \[CenterDot] (a_), 
         "MatchingConstruct" -> {"CriticalPairLemma", 21}, 
         "MatchingOrientation" -> 1, "MatchingRule" -> 
          (a_) \[CenterDot] (((b_) \[CenterDot] (b_)) \[CenterDot] (a_)) -> 
           a \[CenterDot] b, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"SubstitutionLemma", 17} -> <|"Statement" -> 
        HoldForm[(a \[CenterDot] a) \[CenterDot] b == 
          (b \[CenterDot] a) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
             b) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] b))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 26}, 
         "Construct" -> {"Axiom", 1}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           (a \[CenterDot] a) \[CenterDot] b == (b \[CenterDot] 
              a) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
               b) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] b))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 18} -> 
      <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] b == 
          (b \[CenterDot] a) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
             b) \[CenterDot] (b \[CenterDot] a))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 17}, 
         "Construct" -> {"CriticalPairLemma", 17}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (b_)) -> 
           a \[CenterDot] (b \[CenterDot] a), "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[(a \[CenterDot] a) \[CenterDot] b == 
            (b \[CenterDot] a) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
               b) \[CenterDot] (b \[CenterDot] a))], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 19} -> <|"Statement" -> 
        HoldForm[(a \[CenterDot] a) \[CenterDot] b == 
          (b \[CenterDot] a) \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
            ((a \[CenterDot] a) \[CenterDot] b))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 18}, 
         "Construct" -> {"SubstitutionLemma", 9}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (c_)) -> 
           a \[CenterDot] (c \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[(a \[CenterDot] a) \[CenterDot] b == 
            (b \[CenterDot] a) \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
              ((a \[CenterDot] a) \[CenterDot] b))], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 20} -> <|"Statement" -> 
        HoldForm[(a \[CenterDot] a) \[CenterDot] b == 
          (b \[CenterDot] a) \[CenterDot] b], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 19}, "Construct" -> 
          {"CriticalPairLemma", 11}, "Position" -> {2}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            (((b_) \[CenterDot] (c_)) \[CenterDot] (a_)) -> a, 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           (a \[CenterDot] a) \[CenterDot] b == (b \[CenterDot] 
              a) \[CenterDot] b], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 21} -> <|"Statement" -> 
        HoldForm[(a \[CenterDot] a) \[CenterDot] b == b \[CenterDot] 
           (b \[CenterDot] a)], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 20}, "Construct" -> {"Axiom", 1}, 
         "Position" -> {}, "Rule" -> (a_) \[CenterDot] (b_) -> 
           b \[CenterDot] a, "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[(a \[CenterDot] a) \[CenterDot] b == b \[CenterDot] 
             (b \[CenterDot] a)], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 27} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] a == a \[CenterDot] (b \[CenterDot] 
            (a \[CenterDot] ((b \[CenterDot] b) \[CenterDot] c)))], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 16}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             ((a_) \[CenterDot] ((c_) \[CenterDot] ((c_) \[CenterDot] 
                (b_))))) -> a \[CenterDot] a, "Side" -> 1, 
         "Subpattern" -> (c_) \[CenterDot] ((c_) \[CenterDot] (b_)), 
         "MatchingConstruct" -> {"SubstitutionLemma", 21}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          (a_) \[CenterDot] ((a_) \[CenterDot] (b_)) -> 
           (b \[CenterDot] b) \[CenterDot] a, "MatchingSide" -> 1, 
         "Position" -> {2, 2, 2}|>|>, {"CriticalPairLemma", 28} -> 
      <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
           (b \[CenterDot] b)], "Proof" -> 
        <|"Construct" -> {"CriticalPairLemma", 18}, "Orientation" -> -1, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
             (a_)) -> a, "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] (b_), 
         "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {1}|>|>, 
     {"CriticalPairLemma", 29} -> <|"Statement" -> 
        HoldForm[c == ((a \[CenterDot] b) \[CenterDot] c) \[CenterDot] 
           (c \[CenterDot] b)], "Proof" -> 
        <|"Construct" -> {"CriticalPairLemma", 1}, "Orientation" -> -1, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
             ((a_) \[CenterDot] (c_))) -> b, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (c_), "MatchingConstruct" -> 
          {"CriticalPairLemma", 28}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((b_) \[CenterDot] (b_)) -> b, "MatchingSide" -> 1, 
         "Position" -> {2, 2}|>|>, {"SubstitutionLemma", 22} -> 
      <|"Statement" -> HoldForm[c == (c \[CenterDot] b) \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] c)], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 29}, 
         "Construct" -> {"Axiom", 1}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           c == (c \[CenterDot] b) \[CenterDot] ((a \[CenterDot] 
               b) \[CenterDot] c)], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 30} -> <|"Statement" -> 
        HoldForm[a == (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
            ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] c))], 
       "Proof" -> <|"Construct" -> {"Axiom", 2}, "Orientation" -> 1, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
             ((b_) \[CenterDot] (c_))) -> a, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 2}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
             ((a_) \[CenterDot] (b_))) -> a \[CenterDot] b, 
         "MatchingSide" -> 1, "Position" -> {1}|>|>, 
     {"CriticalPairLemma", 31} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
              b)) \[CenterDot] c) == 
          ((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
              c)) \[CenterDot] b) \[CenterDot] a], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 22}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            (((c_) \[CenterDot] (b_)) \[CenterDot] (a_)) -> a, "Side" -> 1, 
         "Subpattern" -> ((c_) \[CenterDot] (b_)) \[CenterDot] (a_), 
         "MatchingConstruct" -> {"CriticalPairLemma", 30}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          ((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
             (((a_) \[CenterDot] ((a_) \[CenterDot] (b_))) \[CenterDot] 
              (c_))) -> a, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"SubstitutionLemma", 23} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
              b)) \[CenterDot] c) == a \[CenterDot] 
           ((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
              c)) \[CenterDot] b)], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 31}, "Construct" -> {"Axiom", 1}, 
         "Position" -> {}, "Rule" -> (a_) \[CenterDot] (b_) -> 
           b \[CenterDot] a, "Orientation" -> 1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
                b)) \[CenterDot] c) == a \[CenterDot] 
             ((a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
                  b)) \[CenterDot] c)) \[CenterDot] b)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 24} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] ((a \[CenterDot] 
             (a \[CenterDot] b)) \[CenterDot] c) == a \[CenterDot] 
           (b \[CenterDot] (a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
                b)) \[CenterDot] c)))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 23}, "Construct" -> 
          {"SubstitutionLemma", 9}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (c_)) -> 
           a \[CenterDot] (c \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] 
             ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] c) == 
            a \[CenterDot] (b \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                 (a \[CenterDot] b)) \[CenterDot] c)))], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 32} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] a == a \[CenterDot] 
           (b \[CenterDot] (a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] 
                (b \[CenterDot] b))) \[CenterDot] c)))], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 27}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             ((a_) \[CenterDot] (((b_) \[CenterDot] (b_)) \[CenterDot] (
                c_)))) -> a \[CenterDot] a, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (((b_) \[CenterDot] 
             (b_)) \[CenterDot] (c_)), "MatchingConstruct" -> 
          {"SubstitutionLemma", 24}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             ((a_) \[CenterDot] (((a_) \[CenterDot] ((a_) \[CenterDot] 
                 (b_))) \[CenterDot] (c_)))) -> a \[CenterDot] 
            ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] c), 
         "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
     {"CriticalPairLemma", 33} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] b == a \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] b))], "Proof" -> 
        <|"Construct" -> {"CriticalPairLemma", 21}, "Orientation" -> 1, 
         "Rule" -> (a_) \[CenterDot] (((b_) \[CenterDot] (b_)) \[CenterDot] 
             (a_)) -> a \[CenterDot] b, "Side" -> 1, "Subpattern" -> 
          ((b_) \[CenterDot] (b_)) \[CenterDot] (a_), "MatchingConstruct" -> 
          {"Axiom", 1}, "MatchingOrientation" -> 1, "MatchingRule" -> 
          (a_) \[CenterDot] (b_) -> b \[CenterDot] a, "MatchingSide" -> 1, 
         "Position" -> {2}|>|>, {"SubstitutionLemma", 25} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] a == a \[CenterDot] 
           (b \[CenterDot] (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              c)))], "Proof" -> <|"Input" -> {"CriticalPairLemma", 32}, 
         "Construct" -> {"CriticalPairLemma", 33}, "Position" -> {2, 2, 2, 
          1}, "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
             ((b_) \[CenterDot] (b_))) -> a \[CenterDot] b, 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[a \[CenterDot] a == 
            a \[CenterDot] (b \[CenterDot] (a \[CenterDot] ((a \[CenterDot] 
                 b) \[CenterDot] c)))], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 34} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
              a) \[CenterDot] c)) == a \[CenterDot] (b \[CenterDot] b)], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 6}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             ((a_) \[CenterDot] ((b_) \[CenterDot] (c_)))) -> 
           a \[CenterDot] (b \[CenterDot] c), "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] ((a_) \[CenterDot] 
            ((b_) \[CenterDot] (c_))), "MatchingConstruct" -> 
          {"SubstitutionLemma", 25}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             ((a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] (
                c_)))) -> a \[CenterDot] a, "MatchingSide" -> 1, 
         "Position" -> {2}|>|>, {"CriticalPairLemma", 35} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] c) == 
          a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
             a))], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 6}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             ((a_) \[CenterDot] ((b_) \[CenterDot] (c_)))) -> 
           a \[CenterDot] (b \[CenterDot] c), "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] ((b_) \[CenterDot] (c_)), 
         "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
     {"CriticalPairLemma", 36} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (b \[CenterDot] c) == a \[CenterDot] 
           (b \[CenterDot] ((c \[CenterDot] b) \[CenterDot] a))], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 35}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             (((b_) \[CenterDot] (c_)) \[CenterDot] (a_))) -> 
           a \[CenterDot] (b \[CenterDot] c), "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] (c_), "MatchingConstruct" -> 
          {"Axiom", 1}, "MatchingOrientation" -> 1, "MatchingRule" -> 
          (a_) \[CenterDot] (b_) -> b \[CenterDot] a, "MatchingSide" -> 1, 
         "Position" -> {2, 2, 1}|>|>, {"CriticalPairLemma", 37} -> 
      <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] c == 
          (((a \[CenterDot] b) \[CenterDot] c) \[CenterDot] b) \[CenterDot] 
           c], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 22}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            (((c_) \[CenterDot] (b_)) \[CenterDot] (a_)) -> a, "Side" -> 1, 
         "Subpattern" -> ((c_) \[CenterDot] (b_)) \[CenterDot] (a_), 
         "MatchingConstruct" -> {"SubstitutionLemma", 22}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          ((a_) \[CenterDot] (b_)) \[CenterDot] (((c_) \[CenterDot] 
              (b_)) \[CenterDot] (a_)) -> a, "MatchingSide" -> 1, 
         "Position" -> {2}|>|>, {"SubstitutionLemma", 26} -> 
      <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] c == 
          c \[CenterDot] (((a \[CenterDot] b) \[CenterDot] c) \[CenterDot] 
            b)], "Proof" -> <|"Input" -> {"CriticalPairLemma", 37}, 
         "Construct" -> {"Axiom", 1}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           (a \[CenterDot] b) \[CenterDot] c == c \[CenterDot] 
             (((a \[CenterDot] b) \[CenterDot] c) \[CenterDot] b)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 27} -> 
      <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] c == 
          c \[CenterDot] (b \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             c))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 26}, 
         "Construct" -> {"Axiom", 1}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           (a \[CenterDot] b) \[CenterDot] c == c \[CenterDot] 
             (b \[CenterDot] ((a \[CenterDot] b) \[CenterDot] c))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 28} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] c) == 
          (c \[CenterDot] b) \[CenterDot] a], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 36}, "Construct" -> 
          {"SubstitutionLemma", 27}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             (((c_) \[CenterDot] (b_)) \[CenterDot] (a_))) -> 
           (c \[CenterDot] b) \[CenterDot] a, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] (b \[CenterDot] c) == 
            (c \[CenterDot] b) \[CenterDot] a], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 38} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (b \[CenterDot] b) == a \[CenterDot] 
           (b \[CenterDot] (c \[CenterDot] (a \[CenterDot] b)))], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 34}, 
         "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             (((b_) \[CenterDot] (a_)) \[CenterDot] (c_))) -> 
           a \[CenterDot] (b \[CenterDot] b), "Side" -> 1, 
         "Subpattern" -> ((b_) \[CenterDot] (a_)) \[CenterDot] (c_), 
         "MatchingConstruct" -> {"SubstitutionLemma", 28}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          ((a_) \[CenterDot] (b_)) \[CenterDot] (c_) -> c \[CenterDot] 
            (b \[CenterDot] a), "MatchingSide" -> 1, "Position" -> {2, 
          2}|>|>, {"CriticalPairLemma", 39} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] a == a \[CenterDot] 
           ((a \[CenterDot] (b \[CenterDot] (c \[CenterDot] c))) \[CenterDot] 
            c)], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 13}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] 
            (((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) \[CenterDot] 
             ((c_) \[CenterDot] (c_))) -> a \[CenterDot] a, "Side" -> 1, 
         "Subpattern" -> (c_) \[CenterDot] (c_), "MatchingConstruct" -> 
          {"CriticalPairLemma", 3}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] (b_)) -> a, "MatchingSide" -> 1, 
         "Position" -> {2, 2}|>|>, {"SubstitutionLemma", 29} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] a == a \[CenterDot] 
           (c \[CenterDot] (a \[CenterDot] (b \[CenterDot] (c \[CenterDot] 
               c))))], "Proof" -> <|"Input" -> {"CriticalPairLemma", 39}, 
         "Construct" -> {"SubstitutionLemma", 9}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (c_)) -> 
           a \[CenterDot] (c \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] a == 
            a \[CenterDot] (c \[CenterDot] (a \[CenterDot] (b \[CenterDot] 
                (c \[CenterDot] c))))], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 40} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] ((b \[CenterDot] (c \[CenterDot] 
              (a \[CenterDot] a))) \[CenterDot] (b \[CenterDot] 
             (c \[CenterDot] (a \[CenterDot] a)))) == a \[CenterDot] 
           ((b \[CenterDot] (c \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
            (b \[CenterDot] b))], "Proof" -> 
        <|"Construct" -> {"CriticalPairLemma", 38}, "Orientation" -> -1, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] ((c_) \[CenterDot] 
              ((a_) \[CenterDot] (b_)))) -> a \[CenterDot] 
            (b \[CenterDot] b), "Side" -> 1, "Subpattern" -> 
          (c_) \[CenterDot] ((a_) \[CenterDot] (b_)), "MatchingConstruct" -> 
          {"SubstitutionLemma", 29}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             ((a_) \[CenterDot] ((c_) \[CenterDot] ((b_) \[CenterDot] 
                (b_))))) -> a \[CenterDot] a, "MatchingSide" -> 1, 
         "Position" -> {2, 2}|>|>, {"SubstitutionLemma", 30} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] ((b \[CenterDot] 
             (c \[CenterDot] (a \[CenterDot] a))) \[CenterDot] 
            (b \[CenterDot] (c \[CenterDot] (a \[CenterDot] a)))) == 
          a \[CenterDot] b], "Proof" -> <|"Input" -> {"CriticalPairLemma", 
           40}, "Construct" -> {"CriticalPairLemma", 18}, "Position" -> {2}, 
         "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
             (a_)) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[a \[CenterDot] ((b \[CenterDot] (c \[CenterDot] 
                (a \[CenterDot] a))) \[CenterDot] (b \[CenterDot] (
                c \[CenterDot] (a \[CenterDot] a)))) == a \[CenterDot] b], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 31} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] ((b \[CenterDot] 
             (c \[CenterDot] (a \[CenterDot] a))) \[CenterDot] a) == 
          a \[CenterDot] b], "Proof" -> <|"Input" -> {"SubstitutionLemma", 
           30}, "Construct" -> {"CriticalPairLemma", 17}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (b_)) -> 
           a \[CenterDot] (b \[CenterDot] a), "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[a \[CenterDot] 
             ((b \[CenterDot] (c \[CenterDot] (a \[CenterDot] 
                 a))) \[CenterDot] a) == a \[CenterDot] b], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 32} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] (c \[CenterDot] (a \[CenterDot] a)))) == 
          a \[CenterDot] b], "Proof" -> <|"Input" -> {"SubstitutionLemma", 
           31}, "Construct" -> {"SubstitutionLemma", 9}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (c_)) -> 
           a \[CenterDot] (c \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
              (b \[CenterDot] (c \[CenterDot] (a \[CenterDot] a)))) == 
            a \[CenterDot] b], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 41} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b) == 
          a \[CenterDot] a], "Proof" -> <|"Construct" -> 
          {"SubstitutionLemma", 6}, "Orientation" -> -1, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] ((a_) \[CenterDot] 
              ((b_) \[CenterDot] (c_)))) -> a \[CenterDot] 
            (b \[CenterDot] c), "Side" -> 1, "Subpattern" -> 
          (b_) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))), 
         "MatchingConstruct" -> {"CriticalPairLemma", 8}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          ((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
             ((c_) \[CenterDot] (a_))) -> b, "MatchingSide" -> 1, 
         "Position" -> {2}|>|>, {"SubstitutionLemma", 33} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
            (b \[CenterDot] a)) == a \[CenterDot] a], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 41}, 
         "Construct" -> {"Axiom", 1}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 1, "OutputExpression" -> HoldForm[
           a \[CenterDot] (b \[CenterDot] (b \[CenterDot] a)) == 
            a \[CenterDot] a], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 42} -> <|"Statement" -> 
        HoldForm[c == ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
            c) \[CenterDot] (c \[CenterDot] (b \[CenterDot] b))], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 8}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((b_) \[CenterDot] ((c_) \[CenterDot] (a_))) -> b, "Side" -> 1, 
         "Subpattern" -> (c_) \[CenterDot] (a_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 33}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             ((b_) \[CenterDot] (a_))) -> a \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
     {"CriticalPairLemma", 43} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] ((c \[CenterDot] (c \[CenterDot] 
              a)) \[CenterDot] b) == a \[CenterDot] (a \[CenterDot] b)], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 32}, 
         "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
             ((b_) \[CenterDot] ((c_) \[CenterDot] ((a_) \[CenterDot] 
                (a_))))) -> a \[CenterDot] b, "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] ((c_) \[CenterDot] 
            ((a_) \[CenterDot] (a_))), "MatchingConstruct" -> 
          {"CriticalPairLemma", 42}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> (((a_) \[CenterDot] ((a_) \[CenterDot] (
                b_))) \[CenterDot] (c_)) \[CenterDot] ((c_) \[CenterDot] 
             ((b_) \[CenterDot] (b_))) -> c, "MatchingSide" -> 1, 
         "Position" -> {2, 2}|>|>, {"CriticalPairLemma", 44} -> 
      <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
           ((a \[CenterDot] c) \[CenterDot] b)], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 1}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((b_) \[CenterDot] ((a_) \[CenterDot] (c_))) -> b, "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] ((a_) \[CenterDot] (c_)), 
         "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"CriticalPairLemma", 45} -> <|"Statement" -> 
        HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
           ((c \[CenterDot] a) \[CenterDot] b)], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 44}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            (((a_) \[CenterDot] (c_)) \[CenterDot] (b_)) -> b, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (c_), "MatchingConstruct" -> 
          {"Axiom", 1}, "MatchingOrientation" -> 1, "MatchingRule" -> 
          (a_) \[CenterDot] (b_) -> b \[CenterDot] a, "MatchingSide" -> 1, 
         "Position" -> {2, 1}|>|>, {"CriticalPairLemma", 46} -> 
      <|"Statement" -> HoldForm[b == ((a \[CenterDot] a) \[CenterDot] 
            b) \[CenterDot] ((c \[CenterDot] (a \[CenterDot] c)) \[CenterDot] 
            b)], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 45}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            (((c_) \[CenterDot] (a_)) \[CenterDot] (b_)) -> b, "Side" -> 1, 
         "Subpattern" -> (c_) \[CenterDot] (a_), "MatchingConstruct" -> 
          {"CriticalPairLemma", 17}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (b_)) -> 
           a \[CenterDot] (b \[CenterDot] a), "MatchingSide" -> 1, 
         "Position" -> {2, 1}|>|>, {"CriticalPairLemma", 47} -> 
      <|"Statement" -> HoldForm[((a \[CenterDot] a) \[CenterDot] 
            (c \[CenterDot] x3)) \[CenterDot] (x3 \[CenterDot] x3) == 
          ((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
           ((b \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
            (((a \[CenterDot] a) \[CenterDot] (c \[CenterDot] 
               x3)) \[CenterDot] (x3 \[CenterDot] x3)))], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 46}, 
         "Orientation" -> -1, "Rule" -> 
          (((a_) \[CenterDot] (a_)) \[CenterDot] (b_)) \[CenterDot] 
            (((c_) \[CenterDot] ((a_) \[CenterDot] (c_))) \[CenterDot] 
             (b_)) -> b, "Side" -> 1, "Subpattern" -> 
          ((a_) \[CenterDot] (a_)) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 13}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> (a_) \[CenterDot] (((a_) \[CenterDot] 
              ((b_) \[CenterDot] (c_))) \[CenterDot] ((c_) \[CenterDot] 
              (c_))) -> a \[CenterDot] a, "MatchingSide" -> 1, 
         "Position" -> {1}|>|>, {"SubstitutionLemma", 34} -> 
      <|"Statement" -> HoldForm[((a \[CenterDot] a) \[CenterDot] 
            (c \[CenterDot] x3)) \[CenterDot] (x3 \[CenterDot] x3) == 
          a \[CenterDot] ((b \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
            (((a \[CenterDot] a) \[CenterDot] (c \[CenterDot] 
               x3)) \[CenterDot] (x3 \[CenterDot] x3)))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 47}, 
         "Construct" -> {"CriticalPairLemma", 3}, "Position" -> {1}, 
         "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
             (b_)) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[((a \[CenterDot] a) \[CenterDot] (c \[CenterDot] 
               x3)) \[CenterDot] (x3 \[CenterDot] x3) == a \[CenterDot] 
             ((b \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
              (((a \[CenterDot] a) \[CenterDot] (c \[CenterDot] 
                 x3)) \[CenterDot] (x3 \[CenterDot] x3)))], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 48} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] c) == 
          a \[CenterDot] ((b \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
            c)], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 43}, 
         "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] 
            (((b_) \[CenterDot] ((b_) \[CenterDot] (a_))) \[CenterDot] 
             (c_)) -> a \[CenterDot] (a \[CenterDot] c), "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] (a_), "MatchingConstruct" -> 
          {"Axiom", 1}, "MatchingOrientation" -> 1, "MatchingRule" -> 
          (a_) \[CenterDot] (b_) -> b \[CenterDot] a, "MatchingSide" -> 1, 
         "Position" -> {2, 1, 2}|>|>, {"SubstitutionLemma", 35} -> 
      <|"Statement" -> HoldForm[((a \[CenterDot] a) \[CenterDot] 
            (c \[CenterDot] x3)) \[CenterDot] (x3 \[CenterDot] x3) == 
          a \[CenterDot] (a \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
              (c \[CenterDot] x3)) \[CenterDot] (x3 \[CenterDot] x3)))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 34}, 
         "Construct" -> {"CriticalPairLemma", 48}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (((b_) \[CenterDot] ((a_) \[CenterDot] (
                b_))) \[CenterDot] (c_)) -> a \[CenterDot] 
            (a \[CenterDot] c), "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[((a \[CenterDot] a) \[CenterDot] (c \[CenterDot] 
               x3)) \[CenterDot] (x3 \[CenterDot] x3) == a \[CenterDot] 
             (a \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
                (c \[CenterDot] x3)) \[CenterDot] (x3 \[CenterDot] x3)))], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 49} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] c) == 
          a \[CenterDot] (((a \[CenterDot] a) \[CenterDot] b) \[CenterDot] 
            c)], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 43}, 
         "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] 
            (((b_) \[CenterDot] ((b_) \[CenterDot] (a_))) \[CenterDot] 
             (c_)) -> a \[CenterDot] (a \[CenterDot] c), "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] ((b_) \[CenterDot] (a_)), 
         "MatchingConstruct" -> {"SubstitutionLemma", 21}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          (a_) \[CenterDot] ((a_) \[CenterDot] (b_)) -> 
           (b \[CenterDot] b) \[CenterDot] a, "MatchingSide" -> 1, 
         "Position" -> {2, 1}|>|>, {"SubstitutionLemma", 36} -> 
      <|"Statement" -> HoldForm[((a \[CenterDot] a) \[CenterDot] 
            (c \[CenterDot] x3)) \[CenterDot] (x3 \[CenterDot] x3) == 
          a \[CenterDot] (a \[CenterDot] (a \[CenterDot] (x3 \[CenterDot] 
              x3)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 35}, 
         "Construct" -> {"CriticalPairLemma", 49}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] ((((a_) \[CenterDot] (a_)) \[CenterDot] 
              (b_)) \[CenterDot] (c_)) -> a \[CenterDot] (a \[CenterDot] c), 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           ((a \[CenterDot] a) \[CenterDot] (c \[CenterDot] x3)) \[CenterDot] 
             (x3 \[CenterDot] x3) == a \[CenterDot] (a \[CenterDot] 
              (a \[CenterDot] (x3 \[CenterDot] x3)))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 37} -> 
      <|"Statement" -> HoldForm[((a \[CenterDot] a) \[CenterDot] 
            (c \[CenterDot] x3)) \[CenterDot] (x3 \[CenterDot] x3) == 
          a \[CenterDot] (x3 \[CenterDot] x3)], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 36}, 
         "Construct" -> {"SubstitutionLemma", 2}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
              (b_))) -> a \[CenterDot] b, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[((a \[CenterDot] a) \[CenterDot] 
              (c \[CenterDot] x3)) \[CenterDot] (x3 \[CenterDot] x3) == 
            a \[CenterDot] (x3 \[CenterDot] x3)], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 50} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
            (b \[CenterDot] c)) == ((a \[CenterDot] a) \[CenterDot] 
            b) \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
            (b \[CenterDot] c))], "Proof" -> 
        <|"Construct" -> {"SubstitutionLemma", 37}, "Orientation" -> 1, 
         "Rule" -> (((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
              (c_))) \[CenterDot] ((c_) \[CenterDot] (c_)) -> 
           a \[CenterDot] (c \[CenterDot] c), "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] (c_), "MatchingConstruct" -> 
          {"CriticalPairLemma", 3}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
            ((a_) \[CenterDot] (b_)) -> a, "MatchingSide" -> 1, 
         "Position" -> {1, 2}|>|>, {"CriticalPairLemma", 51} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
            ((((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
              c) \[CenterDot] (((b \[CenterDot] b) \[CenterDot] 
               a) \[CenterDot] c))) == a \[CenterDot] (b \[CenterDot] 
            ((((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
              c) \[CenterDot] (((b \[CenterDot] b) \[CenterDot] 
               a) \[CenterDot] c)))], "Proof" -> 
        <|"Construct" -> {"CriticalPairLemma", 43}, "Orientation" -> 1, 
         "Rule" -> (a_) \[CenterDot] (((b_) \[CenterDot] ((b_) \[CenterDot] (
                a_))) \[CenterDot] (c_)) -> a \[CenterDot] 
            (a \[CenterDot] c), "Side" -> 1, "Subpattern" -> 
          ((b_) \[CenterDot] ((b_) \[CenterDot] (a_))) \[CenterDot] (c_), 
         "MatchingConstruct" -> {"CriticalPairLemma", 50}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          (((a_) \[CenterDot] (a_)) \[CenterDot] (b_)) \[CenterDot] 
            (((b_) \[CenterDot] (c_)) \[CenterDot] ((b_) \[CenterDot] 
              (c_))) -> a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
             (b \[CenterDot] c)), "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"CriticalPairLemma", 52} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (c \[CenterDot] (b \[CenterDot] b)) == 
          a \[CenterDot] ((b \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
             (c \[CenterDot] (b \[CenterDot] c))))], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 8}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             ((a_) \[CenterDot] ((c_) \[CenterDot] (b_)))) -> 
           a \[CenterDot] (c \[CenterDot] b), "Side" -> 1, 
         "Subpattern" -> (c_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"CriticalPairLemma", 17}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (b_)) -> 
           a \[CenterDot] (b \[CenterDot] a), "MatchingSide" -> 1, 
         "Position" -> {2, 2, 2}|>|>, {"CriticalPairLemma", 53} -> 
      <|"Statement" -> HoldForm[c \[CenterDot] b == 
          ((a \[CenterDot] b) \[CenterDot] (c \[CenterDot] b)) \[CenterDot] 
           c], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 8}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((b_) \[CenterDot] ((c_) \[CenterDot] (a_))) -> b, "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] ((c_) \[CenterDot] (a_)), 
         "MatchingConstruct" -> {"CriticalPairLemma", 6}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          ((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
             ((c_) \[CenterDot] (b_))) -> a, "MatchingSide" -> 1, 
         "Position" -> {2}|>|>, {"SubstitutionLemma", 38} -> 
      <|"Statement" -> HoldForm[c \[CenterDot] b == c \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] (c \[CenterDot] b))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 53}, 
         "Construct" -> {"Axiom", 1}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[c \[CenterDot] b == 
            c \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (c \[CenterDot] 
               b))], "Source" -> "norm"|>|>, {"CriticalPairLemma", 54} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (c \[CenterDot] 
            (b \[CenterDot] c)) == a \[CenterDot] 
           ((b \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
             (c \[CenterDot] (b \[CenterDot] c))))], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 38}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] 
            (((b_) \[CenterDot] (c_)) \[CenterDot] ((a_) \[CenterDot] 
              (c_))) -> a \[CenterDot] c, "Side" -> 1, "Subpattern" -> 
          (b_) \[CenterDot] (c_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 11}, "MatchingOrientation" -> 1, 
         "MatchingRule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             ((a_) \[CenterDot] (b_))) -> a \[CenterDot] a, 
         "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
     {"SubstitutionLemma", 39} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (c \[CenterDot] (b \[CenterDot] b)) == 
          a \[CenterDot] (c \[CenterDot] (b \[CenterDot] c))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 52}, 
         "Construct" -> {"CriticalPairLemma", 54}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (((b_) \[CenterDot] (b_)) \[CenterDot] 
             ((a_) \[CenterDot] ((c_) \[CenterDot] ((b_) \[CenterDot] 
                (c_))))) -> a \[CenterDot] (c \[CenterDot] (b \[CenterDot] 
              c)), "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[a \[CenterDot] (c \[CenterDot] (b \[CenterDot] b)) == 
            a \[CenterDot] (c \[CenterDot] (b \[CenterDot] c))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 40} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
            ((((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
              c) \[CenterDot] (((b \[CenterDot] b) \[CenterDot] 
               a) \[CenterDot] c))) == a \[CenterDot] (b \[CenterDot] 
            ((((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
              c) \[CenterDot] b))], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 51}, "Construct" -> 
          {"SubstitutionLemma", 39}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] ((c_) \[CenterDot] 
              (c_))) -> a \[CenterDot] (b \[CenterDot] (c \[CenterDot] b)), 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           a \[CenterDot] (a \[CenterDot] ((((b \[CenterDot] b) \[CenterDot] 
                 a) \[CenterDot] c) \[CenterDot] (((b \[CenterDot] 
                  b) \[CenterDot] a) \[CenterDot] c))) == a \[CenterDot] 
             (b \[CenterDot] ((((b \[CenterDot] b) \[CenterDot] 
                 a) \[CenterDot] c) \[CenterDot] b))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 41} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
            ((((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
              c) \[CenterDot] (((b \[CenterDot] b) \[CenterDot] 
               a) \[CenterDot] c))) == a \[CenterDot] (b \[CenterDot] 
            (b \[CenterDot] (((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
              c)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 40}, 
         "Construct" -> {"SubstitutionLemma", 9}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (c_)) -> 
           a \[CenterDot] (c \[CenterDot] b), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
              ((((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
                c) \[CenterDot] (((b \[CenterDot] b) \[CenterDot] 
                 a) \[CenterDot] c))) == a \[CenterDot] (b \[CenterDot] 
              (b \[CenterDot] (((b \[CenterDot] b) \[CenterDot] 
                 a) \[CenterDot] c)))], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 42} -> <|"Statement" -> 
        HoldForm[a \[CenterDot] (a \[CenterDot] 
            ((((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
              c) \[CenterDot] (((b \[CenterDot] b) \[CenterDot] 
               a) \[CenterDot] c))) == a \[CenterDot] (b \[CenterDot] 
            (b \[CenterDot] (b \[CenterDot] c)))], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 41}, 
         "Construct" -> {"CriticalPairLemma", 49}, "Position" -> {2, 2}, 
         "Rule" -> (a_) \[CenterDot] ((((a_) \[CenterDot] (a_)) \[CenterDot] 
              (b_)) \[CenterDot] (c_)) -> a \[CenterDot] (a \[CenterDot] c), 
         "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           a \[CenterDot] (a \[CenterDot] ((((b \[CenterDot] b) \[CenterDot] 
                 a) \[CenterDot] c) \[CenterDot] (((b \[CenterDot] 
                  b) \[CenterDot] a) \[CenterDot] c))) == a \[CenterDot] 
             (b \[CenterDot] (b \[CenterDot] (b \[CenterDot] c)))], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 43} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
            ((((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
              c) \[CenterDot] (((b \[CenterDot] b) \[CenterDot] 
               a) \[CenterDot] c))) == a \[CenterDot] (b \[CenterDot] c)], 
       "Proof" -> <|"Input" -> {"SubstitutionLemma", 42}, 
         "Construct" -> {"SubstitutionLemma", 2}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
              (b_))) -> a \[CenterDot] b, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
              ((((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
                c) \[CenterDot] (((b \[CenterDot] b) \[CenterDot] 
                 a) \[CenterDot] c))) == a \[CenterDot] (b \[CenterDot] c)], 
         "Source" -> "norm"|>|>, {"SubstitutionLemma", 44} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] 
           (((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] c) == 
          a \[CenterDot] (b \[CenterDot] c)], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 43}, "Construct" -> 
          {"CriticalPairLemma", 33}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] 
              (b_))) -> a \[CenterDot] b, "Orientation" -> -1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
         "OutputExpression" -> HoldForm[a \[CenterDot] 
             (((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] c) == 
            a \[CenterDot] (b \[CenterDot] c)], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 55} -> <|"Statement" -> 
        HoldForm[b \[CenterDot] (b \[CenterDot] a) == 
          (a \[CenterDot] a) \[CenterDot] b], "Proof" -> 
        <|"Construct" -> {"SubstitutionLemma", 14}, "Orientation" -> -1, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (b_)) -> 
           a \[CenterDot] (a \[CenterDot] b), "Side" -> 1, 
         "Subpattern" -> {}, "MatchingConstruct" -> {"Axiom", 1}, 
         "MatchingOrientation" -> 1, "MatchingRule" -> 
          (a_) \[CenterDot] (b_) -> b \[CenterDot] a, "MatchingSide" -> 1, 
         "Position" -> {}|>|>, {"CriticalPairLemma", 56} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] c) == 
          a \[CenterDot] ((a \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
            c)], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 44}, 
         "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] 
            ((((b_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
             (c_)) -> a \[CenterDot] (b \[CenterDot] c), "Side" -> 1, 
         "Subpattern" -> ((b_) \[CenterDot] (b_)) \[CenterDot] (a_), 
         "MatchingConstruct" -> {"CriticalPairLemma", 55}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          ((a_) \[CenterDot] (a_)) \[CenterDot] (b_) -> b \[CenterDot] 
            (b \[CenterDot] a), "MatchingSide" -> 1, "Position" -> {2, 
          1}|>|>, {"CriticalPairLemma", 57} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] b == 
          ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] c)) \[CenterDot] 
           b], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 6}, 
         "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((a_) \[CenterDot] ((c_) \[CenterDot] (b_))) -> a, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] ((c_) \[CenterDot] (b_)), 
         "MatchingConstruct" -> {"CriticalPairLemma", 1}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          ((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
             ((a_) \[CenterDot] (c_))) -> b, "MatchingSide" -> 1, 
         "Position" -> {2}|>|>, {"SubstitutionLemma", 45} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] b == b \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] c))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 57}, 
         "Construct" -> {"Axiom", 1}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[a \[CenterDot] b == 
            b \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
               c))], "Source" -> "norm"|>|>, {"CriticalPairLemma", 58} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] 
           (((b \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
              x3)) \[CenterDot] c) == a \[CenterDot] 
           ((a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] c)], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 56}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] 
            (((a_) \[CenterDot] ((a_) \[CenterDot] (b_))) \[CenterDot] 
             (c_)) -> a \[CenterDot] (b \[CenterDot] c), "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"SubstitutionLemma", 45}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> (a_) \[CenterDot] (((b_) \[CenterDot] 
              (a_)) \[CenterDot] ((b_) \[CenterDot] (c_))) -> 
           b \[CenterDot] a, "MatchingSide" -> 1, "Position" -> {2, 1, 
          2}|>|>, {"CriticalPairLemma", 59} -> 
      <|"Statement" -> HoldForm[b \[CenterDot] (a \[CenterDot] b) == 
          (a \[CenterDot] a) \[CenterDot] b], "Proof" -> 
        <|"Construct" -> {"CriticalPairLemma", 17}, "Orientation" -> -1, 
         "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (b_)) -> 
           a \[CenterDot] (b \[CenterDot] a), "Side" -> 1, 
         "Subpattern" -> {}, "MatchingConstruct" -> {"Axiom", 1}, 
         "MatchingOrientation" -> 1, "MatchingRule" -> 
          (a_) \[CenterDot] (b_) -> b \[CenterDot] a, "MatchingSide" -> 1, 
         "Position" -> {}|>|>, {"CriticalPairLemma", 60} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] c) == 
          a \[CenterDot] ((a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
            c)], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 44}, 
         "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] 
            ((((b_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
             (c_)) -> a \[CenterDot] (b \[CenterDot] c), "Side" -> 1, 
         "Subpattern" -> ((b_) \[CenterDot] (b_)) \[CenterDot] (a_), 
         "MatchingConstruct" -> {"CriticalPairLemma", 59}, 
         "MatchingOrientation" -> -1, "MatchingRule" -> 
          ((a_) \[CenterDot] (a_)) \[CenterDot] (b_) -> b \[CenterDot] 
            (a \[CenterDot] b), "MatchingSide" -> 1, "Position" -> {2, 
          1}|>|>, {"SubstitutionLemma", 46} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] 
           (((b \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
              x3)) \[CenterDot] c) == a \[CenterDot] (b \[CenterDot] c)], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 58}, 
         "Construct" -> {"CriticalPairLemma", 60}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] ((b_) \[CenterDot] (
                a_))) \[CenterDot] (c_)) -> a \[CenterDot] 
            (b \[CenterDot] c), "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[a \[CenterDot] (((b \[CenterDot] a) \[CenterDot] (
                b \[CenterDot] x3)) \[CenterDot] c) == a \[CenterDot] 
             (b \[CenterDot] c)], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 47} -> <|"Statement" -> 
        HoldForm[((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
             b)) \[CenterDot] c == ((a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b)) \[CenterDot] (b \[CenterDot] 
            (a \[CenterDot] c))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 10}, "Construct" -> 
          {"SubstitutionLemma", 46}, "Position" -> {2}, 
         "Rule" -> (a_) \[CenterDot] ((((b_) \[CenterDot] (a_)) \[CenterDot] 
              ((b_) \[CenterDot] (c_))) \[CenterDot] (x3_)) -> 
           a \[CenterDot] (b \[CenterDot] x3), "Orientation" -> 1, 
         "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
         "OutputExpression" -> HoldForm[((a \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] b)) \[CenterDot] c == 
            ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
             (b \[CenterDot] (a \[CenterDot] c))], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 61} -> <|"Statement" -> 
        HoldForm[b \[CenterDot] a == a \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             c))], "Proof" -> <|"Construct" -> {"Axiom", 2}, 
         "Orientation" -> 1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) -> a, "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"CriticalPairLemma", 28}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((b_) \[CenterDot] (b_)) -> b, "MatchingSide" -> 1, 
         "Position" -> {1}|>|>, {"CriticalPairLemma", 62} -> 
      <|"Statement" -> HoldForm[(b \[CenterDot] b) \[CenterDot] a == 
          a \[CenterDot] ((a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
            ((a \[CenterDot] a) \[CenterDot] c))], 
       "Proof" -> <|"Construct" -> {"CriticalPairLemma", 61}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] 
            (((b_) \[CenterDot] (a_)) \[CenterDot] (((a_) \[CenterDot] (
                a_)) \[CenterDot] (c_))) -> b \[CenterDot] a, "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] (a_), "MatchingConstruct" -> 
          {"CriticalPairLemma", 59}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (b_) -> 
           b \[CenterDot] (a \[CenterDot] b), "MatchingSide" -> 1, 
         "Position" -> {2, 1}|>|>, {"SubstitutionLemma", 48} -> 
      <|"Statement" -> HoldForm[(b \[CenterDot] b) \[CenterDot] a == 
          a \[CenterDot] (b \[CenterDot] a)], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 62}, "Construct" -> 
          {"CriticalPairLemma", 19}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
             (((a_) \[CenterDot] (a_)) \[CenterDot] (c_))) -> 
           a \[CenterDot] b, "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[(b \[CenterDot] b) \[CenterDot] a == a \[CenterDot] 
             (b \[CenterDot] a)], "Source" -> "norm"|>|>, 
     {"CriticalPairLemma", 63} -> <|"Statement" -> 
        HoldForm[((b \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
             a)) \[CenterDot] (a \[CenterDot] (b \[CenterDot] c)) == 
          (a \[CenterDot] (b \[CenterDot] c)) \[CenterDot] a], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 48}, 
         "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
             (a_)) -> (b \[CenterDot] b) \[CenterDot] a, "Side" -> 1, 
         "Subpattern" -> (b_) \[CenterDot] (a_), "MatchingConstruct" -> 
          {"CriticalPairLemma", 1}, "MatchingOrientation" -> -1, 
         "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((b_) \[CenterDot] ((a_) \[CenterDot] (c_))) -> b, 
         "MatchingSide" -> 1, "Position" -> {2}|>|>, 
     {"SubstitutionLemma", 49} -> <|"Statement" -> 
        HoldForm[((b \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
             a)) \[CenterDot] (a \[CenterDot] (b \[CenterDot] c)) == 
          a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))], 
       "Proof" -> <|"Input" -> {"CriticalPairLemma", 63}, 
         "Construct" -> {"Axiom", 1}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           ((b \[CenterDot] a) \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
             (a \[CenterDot] (b \[CenterDot] c)) == a \[CenterDot] 
             (a \[CenterDot] (b \[CenterDot] c))], "Source" -> "norm"|>|>, 
     {"SubstitutionLemma", 50} -> <|"Statement" -> 
        HoldForm[((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
             b)) \[CenterDot] c == b \[CenterDot] (b \[CenterDot] 
            (a \[CenterDot] c))], "Proof" -> 
        <|"Input" -> {"SubstitutionLemma", 47}, "Construct" -> 
          {"SubstitutionLemma", 49}, "Position" -> {}, 
         "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
              (b_))) \[CenterDot] ((b_) \[CenterDot] ((a_) \[CenterDot] 
              (c_))) -> b \[CenterDot] (b \[CenterDot] (a \[CenterDot] c)), 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
             c == b \[CenterDot] (b \[CenterDot] (a \[CenterDot] c))], 
         "Source" -> "norm"|>|>, {"CriticalPairLemma", 64} -> 
      <|"Statement" -> HoldForm[b \[CenterDot] (b \[CenterDot] 
            (a \[CenterDot] c)) == ((a \[CenterDot] b) \[CenterDot] 
            (b \[CenterDot] a)) \[CenterDot] c], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 50}, 
         "Orientation" -> 1, "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
             ((a_) \[CenterDot] (b_))) \[CenterDot] (c_) -> 
           b \[CenterDot] (b \[CenterDot] (a \[CenterDot] c)), "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"Axiom", 1}, "MatchingOrientation" -> 1, "MatchingRule" -> 
          (a_) \[CenterDot] (b_) -> b \[CenterDot] a, "MatchingSide" -> 1, 
         "Position" -> {1, 2}|>|>, {"CriticalPairLemma", 65} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] c)) == ((a \[CenterDot] b) \[CenterDot] 
            (b \[CenterDot] a)) \[CenterDot] c], 
       "Proof" -> <|"Construct" -> {"SubstitutionLemma", 50}, 
         "Orientation" -> 1, "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
             ((a_) \[CenterDot] (b_))) \[CenterDot] (c_) -> 
           b \[CenterDot] (b \[CenterDot] (a \[CenterDot] c)), "Side" -> 1, 
         "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
          {"Axiom", 1}, "MatchingOrientation" -> 1, "MatchingRule" -> 
          (a_) \[CenterDot] (b_) -> b \[CenterDot] a, "MatchingSide" -> 1, 
         "Position" -> {1, 1}|>|>, {"SubstitutionLemma", 51} -> 
      <|"Statement" -> HoldForm[b \[CenterDot] (b \[CenterDot] 
            (a \[CenterDot] c)) == a \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] c))], "Proof" -> 
        <|"Input" -> {"CriticalPairLemma", 64}, "Construct" -> 
          {"CriticalPairLemma", 65}, "Position" -> {}, 
         "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
              (a_))) \[CenterDot] (c_) -> a \[CenterDot] (a \[CenterDot] 
             (b \[CenterDot] c)), "Orientation" -> -1, "ConstructSide" -> 1, 
         "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
          HoldForm[b \[CenterDot] (b \[CenterDot] (a \[CenterDot] c)) == 
            a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))], 
         "Source" -> "norm"|>|>, {"Conclusion", 1} -> 
      <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] c)) == a \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] c))], "Proof" -> <|"Input" -> {"Hypothesis", 1}, 
         "Construct" -> {"SubstitutionLemma", 51}, "Position" -> {}, 
         "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] 
              (c_))) -> b \[CenterDot] (b \[CenterDot] (a \[CenterDot] c)), 
         "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
         "Side" -> 2, "OutputExpression" -> HoldForm[
           a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c)) == 
            a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))], 
         "Source" -> "cpl"|>|>}|>]}
