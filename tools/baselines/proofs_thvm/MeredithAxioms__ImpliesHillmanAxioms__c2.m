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
    {"Hypothesis", 1} -> <|"Statement" -> 
       HoldForm[a \[CenterDot] (a \[CenterDot] b) == a \[CenterDot] 
          (b \[CenterDot] b)], "Proof" -> <||>|>, {"CriticalPairLemma", 1} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] a == a \[CenterDot] 
          (b \[CenterDot] (a \[CenterDot] a))], 
      "Proof" -> <|"Construct" -> {"Axiom", 2}, "Orientation" -> 1, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
            (a_)) -> a, "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] (a_), 
        "MatchingConstruct" -> {"Axiom", 2}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
           ((b_) \[CenterDot] (a_)) -> a, "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 2} -> 
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
    {"CriticalPairLemma", 3} -> 
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
    {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] (b \[CenterDot] 
            (b \[CenterDot] b))) \[CenterDot] a == a \[CenterDot] 
          (b \[CenterDot] b)], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 3}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {1}, "Rule" -> (((c_) \[CenterDot] (b_)) \[CenterDot] 
            (b_)) \[CenterDot] (a_) -> a \[CenterDot] (b \[CenterDot] 
            (a \[CenterDot] c)), "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[(b \[CenterDot] (b \[CenterDot] (b \[CenterDot] 
               b))) \[CenterDot] a == a \[CenterDot] (b \[CenterDot] b)], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] b) \[CenterDot] a == 
         a \[CenterDot] (b \[CenterDot] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 1}, 
        "Construct" -> {"CriticalPairLemma", 1}, "Position" -> {1}, 
        "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] ((a_) \[CenterDot] 
             (a_))) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(b \[CenterDot] b) \[CenterDot] a == 
           a \[CenterDot] (b \[CenterDot] b)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 4} -> 
     <|"Statement" -> HoldForm[((b \[CenterDot] b) \[CenterDot] 
           (b \[CenterDot] b)) \[CenterDot] a == a \[CenterDot] b], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 2}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            (b_)) -> (b \[CenterDot] b) \[CenterDot] a, "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"Axiom", 2}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] (a_)) -> a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 3} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] a == a \[CenterDot] b], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 4}, 
        "Construct" -> {"Axiom", 2}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[b \[CenterDot] a == a \[CenterDot] b], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 4} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] b) \[CenterDot] b) == a \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 2}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] b) == 
           a \[CenterDot] a], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 5} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
           (a \[CenterDot] b)) == a \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 4}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a \[CenterDot] (b \[CenterDot] (a \[CenterDot] b)) == 
           a \[CenterDot] a], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 5} -> 
     <|"Statement" -> HoldForm[(((b \[CenterDot] a) \[CenterDot] 
            b) \[CenterDot] b) \[CenterDot] a == a \[CenterDot] 
          (b \[CenterDot] b)], "Proof" -> <|"Construct" -> {"Axiom", 1}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            ((a_) \[CenterDot] (c_))) -> ((c \[CenterDot] b) \[CenterDot] 
            b) \[CenterDot] a, "Side" -> 1, "Subpattern" -> 
         (b_) \[CenterDot] ((a_) \[CenterDot] (c_)), "MatchingConstruct" -> 
         {"SubstitutionLemma", 5}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            ((a_) \[CenterDot] (b_))) -> a \[CenterDot] a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 6} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          (((b \[CenterDot] a) \[CenterDot] b) \[CenterDot] b) == 
         a \[CenterDot] (b \[CenterDot] b)], 
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
    {"CriticalPairLemma", 7} -> 
     <|"Statement" -> HoldForm[
        (((c \[CenterDot] (a \[CenterDot] a)) \[CenterDot] b) \[CenterDot] 
           b) \[CenterDot] a == a \[CenterDot] (b \[CenterDot] 
           (a \[CenterDot] a))], "Proof" -> <|"Construct" -> {"Axiom", 1}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            ((a_) \[CenterDot] (c_))) -> ((c \[CenterDot] b) \[CenterDot] 
            b) \[CenterDot] a, "Side" -> 1, "Subpattern" -> 
         (a_) \[CenterDot] (c_), "MatchingConstruct" -> {"CriticalPairLemma", 
          1}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CenterDot] ((b_) \[CenterDot] ((a_) \[CenterDot] (a_))) -> 
          a \[CenterDot] a, "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"SubstitutionLemma", 9} -> 
     <|"Statement" -> HoldForm[
        (((c \[CenterDot] (a \[CenterDot] a)) \[CenterDot] b) \[CenterDot] 
           b) \[CenterDot] a == a \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 7}, 
        "Construct" -> {"CriticalPairLemma", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] ((a_) \[CenterDot] 
             (a_))) -> a \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[
          (((c \[CenterDot] (a \[CenterDot] a)) \[CenterDot] b) \[CenterDot] 
             b) \[CenterDot] a == a \[CenterDot] a], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 10} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          (((c \[CenterDot] (a \[CenterDot] a)) \[CenterDot] b) \[CenterDot] 
           b) == a \[CenterDot] a], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 9}, "Construct" -> 
         {"SubstitutionLemma", 3}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a \[CenterDot] (((c \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
              b) \[CenterDot] b) == a \[CenterDot] a], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 11} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
           ((c \[CenterDot] (a \[CenterDot] a)) \[CenterDot] b)) == 
         a \[CenterDot] a], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 10}, "Construct" -> 
         {"SubstitutionLemma", 3}, "Position" -> {2}, 
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
         ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] (a_)) -> a, 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"CriticalPairLemma", 9} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] b) \[CenterDot] 
          (a \[CenterDot] a)], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 8}, "Orientation" -> 1, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
            (b_)) -> b, "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] (b_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 3}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CenterDot] (b_) -> b \[CenterDot] a, "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 10} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] a == a \[CenterDot] 
          ((b \[CenterDot] b) \[CenterDot] b)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 11}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            (((c_) \[CenterDot] ((a_) \[CenterDot] (a_))) \[CenterDot] 
             (b_))) -> a \[CenterDot] a, "Side" -> 1, 
        "Subpattern" -> ((c_) \[CenterDot] ((a_) \[CenterDot] 
            (a_))) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 9}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           ((a_) \[CenterDot] (a_)) -> a, "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"SubstitutionLemma", 12} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] a == a \[CenterDot] 
          (b \[CenterDot] (b \[CenterDot] b))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 10}, 
        "Construct" -> {"SubstitutionLemma", 2}, "Position" -> {2}, 
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
    {"SubstitutionLemma", 13} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          (((a \[CenterDot] a) \[CenterDot] b) \[CenterDot] b) == 
         a \[CenterDot] (b \[CenterDot] b)], 
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
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((b \[CenterDot] a) \[CenterDot] (b \[CenterDot] a)) == 
         a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] a)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 14}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            (((a_) \[CenterDot] (a_)) \[CenterDot] (b_))) -> 
          a \[CenterDot] (b \[CenterDot] b), "Side" -> 1, 
        "Subpattern" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (b_), 
        "MatchingConstruct" -> {"Axiom", 2}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
           ((b_) \[CenterDot] (a_)) -> a, "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"SubstitutionLemma", 15} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((b \[CenterDot] a) \[CenterDot] (b \[CenterDot] a)) == 
         a \[CenterDot] (a \[CenterDot] (b \[CenterDot] a))], 
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
         a \[CenterDot] (a \[CenterDot] b)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 6}, 
        "Construct" -> {"SubstitutionLemma", 17}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] 
             (a_))) -> b \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CenterDot] (b \[CenterDot] b) == 
           a \[CenterDot] (a \[CenterDot] b)], "Source" -> "norm"|>|>, 
    {"Conclusion", 1} -> <|"Statement" -> 
       HoldForm[a \[CenterDot] (a \[CenterDot] b) == a \[CenterDot] 
          (a \[CenterDot] b)], "Proof" -> <|"Input" -> {"Hypothesis", 1}, 
        "Construct" -> {"SubstitutionLemma", 18}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (b_)) -> 
          a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CenterDot] (a \[CenterDot] b) == 
           a \[CenterDot] (a \[CenterDot] b)], "Source" -> "cpl"|>|>}|>]
