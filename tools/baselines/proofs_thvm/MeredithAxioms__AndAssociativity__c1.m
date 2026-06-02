ProofObject["EquationalLogic", Inactive[Equal][
  (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
     (b \[CenterDot] c))) \[CenterDot] (a \[CenterDot] 
    ((b \[CenterDot] c) \[CenterDot] (b \[CenterDot] c))), 
  (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
    c) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
     (a \[CenterDot] b)) \[CenterDot] c)], 
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
    {"Hypothesis", 1} -> <|"Statement" -> 
       HoldForm[(a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
            (b \[CenterDot] c))) \[CenterDot] (a \[CenterDot] 
           ((b \[CenterDot] c) \[CenterDot] (b \[CenterDot] c))) == 
         (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
           c) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b)) \[CenterDot] c)], "Proof" -> <||>|>, 
    {"CriticalPairLemma", 1} -> 
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
    {"CriticalPairLemma", 14} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] b) \[CenterDot] 
          (a \[CenterDot] b) == (a \[CenterDot] b) \[CenterDot] 
          ((a \[CenterDot] b) \[CenterDot] b)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 17}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
            ((b_) \[CenterDot] (a_))) -> b \[CenterDot] a, "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] (a_), "MatchingConstruct" -> 
         {"Axiom", 2}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] (a_)) -> a, 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"SubstitutionLemma", 21} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] b) \[CenterDot] 
          (a \[CenterDot] b) == (a \[CenterDot] b) \[CenterDot] 
          (b \[CenterDot] (a \[CenterDot] b))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 14}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Position" -> {2}, 
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
    {"CriticalPairLemma", 15} -> 
     <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
          (b \[CenterDot] (b \[CenterDot] a))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 22}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           ((b_) \[CenterDot] ((a_) \[CenterDot] (b_))) -> b, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 3}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"CriticalPairLemma", 16} -> 
     <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
          (b \[CenterDot] (a \[CenterDot] a))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 15}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           ((b_) \[CenterDot] ((b_) \[CenterDot] (a_))) -> b, "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] ((b_) \[CenterDot] (a_)), 
        "MatchingConstruct" -> {"SubstitutionLemma", 18}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CenterDot] ((a_) \[CenterDot] (b_)) -> a \[CenterDot] 
           (b \[CenterDot] b), "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 17} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] (a \[CenterDot] b) == 
         (a \[CenterDot] a) \[CenterDot] ((b \[CenterDot] (a \[CenterDot] 
             b)) \[CenterDot] (a \[CenterDot] a))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 16}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           ((b_) \[CenterDot] ((a_) \[CenterDot] (a_))) -> b, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 5}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            ((a_) \[CenterDot] (b_))) -> a \[CenterDot] a, 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 18} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
          (b \[CenterDot] b) == (a \[CenterDot] a) \[CenterDot] 
          (b \[CenterDot] (a \[CenterDot] b))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 14}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            (((a_) \[CenterDot] (a_)) \[CenterDot] (b_))) -> 
          a \[CenterDot] (b \[CenterDot] b), "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (a_), "MatchingConstruct" -> 
         {"Axiom", 2}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] (a_)) -> a, 
        "MatchingSide" -> 1, "Position" -> {2, 2, 1}|>|>, 
    {"CriticalPairLemma", 19} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] b) \[CenterDot] 
          (a \[CenterDot] a) == (a \[CenterDot] (b \[CenterDot] 
            a)) \[CenterDot] (b \[CenterDot] b)], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 18}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
           ((b_) \[CenterDot] ((a_) \[CenterDot] (b_))) -> 
          (a \[CenterDot] a) \[CenterDot] (b \[CenterDot] b), "Side" -> 1, 
        "Subpattern" -> {}, "MatchingConstruct" -> {"SubstitutionLemma", 3}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CenterDot] (b_) -> b \[CenterDot] a, "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"SubstitutionLemma", 23} -> 
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
    {"CriticalPairLemma", 20} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) == 
         a \[CenterDot] (a \[CenterDot] (b \[CenterDot] b))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 18}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
            (b_)) -> a \[CenterDot] (b \[CenterDot] b), "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 18}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CenterDot] ((a_) \[CenterDot] (b_)) -> 
          a \[CenterDot] (b \[CenterDot] b), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 24} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
           (a \[CenterDot] b)) == a \[CenterDot] (a \[CenterDot] 
           (b \[CenterDot] b))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 20}, "Construct" -> 
         {"SubstitutionLemma", 18}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (b_)) -> 
          a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CenterDot] (a \[CenterDot] 
             (a \[CenterDot] b)) == a \[CenterDot] (a \[CenterDot] 
             (b \[CenterDot] b))], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 21} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] a == 
         ((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] b)], 
      "Proof" -> <|"Construct" -> {"Axiom", 2}, "Orientation" -> 1, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
            (a_)) -> a, "Side" -> 1, "Subpattern" -> (b_) \[CenterDot] (a_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 2}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CenterDot] ((b_) \[CenterDot] (b_)) -> 
          (b \[CenterDot] b) \[CenterDot] a, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 25} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] a == a \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] b)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 21}, 
        "Construct" -> {"Axiom", 2}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[a \[CenterDot] a == a \[CenterDot] 
            ((a \[CenterDot] a) \[CenterDot] b)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 22} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
            b) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] a)) \[CenterDot] b)) == a \[CenterDot] 
          ((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
            b) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
            (a \[CenterDot] a)))], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 14}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            (((a_) \[CenterDot] (a_)) \[CenterDot] (b_))) -> 
          a \[CenterDot] (b \[CenterDot] b), "Side" -> 1, 
        "Subpattern" -> ((a_) \[CenterDot] (a_)) \[CenterDot] (b_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 25}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] (b_)) -> 
          a \[CenterDot] a, "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"SubstitutionLemma", 26} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
            b) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] a)) \[CenterDot] b)) == a \[CenterDot] 
          (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
           (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
            b))], "Proof" -> <|"Input" -> {"CriticalPairLemma", 22}, 
        "Construct" -> {"SubstitutionLemma", 2}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (b_)) -> 
          (b \[CenterDot] b) \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CenterDot] 
            ((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                a)) \[CenterDot] b) \[CenterDot] (((a \[CenterDot] 
                a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] b)) == 
           a \[CenterDot] (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
               a)) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] (
                a \[CenterDot] a)) \[CenterDot] b))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 27} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
            b) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] a)) \[CenterDot] b)) == a \[CenterDot] 
          (a \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] a)) \[CenterDot] b))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 26}, 
        "Construct" -> {"Axiom", 2}, "Position" -> {2, 1}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[a \[CenterDot] ((((a \[CenterDot] a) \[CenterDot] (
                a \[CenterDot] a)) \[CenterDot] b) \[CenterDot] 
             (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                a)) \[CenterDot] b)) == a \[CenterDot] (a \[CenterDot] 
             (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                a)) \[CenterDot] b))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 28} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
            b) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] a)) \[CenterDot] b)) == a \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] b))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 27}, 
        "Construct" -> {"Axiom", 2}, "Position" -> {2, 2, 1}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[a \[CenterDot] ((((a \[CenterDot] a) \[CenterDot] (
                a \[CenterDot] a)) \[CenterDot] b) \[CenterDot] 
             (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                a)) \[CenterDot] b)) == a \[CenterDot] (a \[CenterDot] 
             (a \[CenterDot] b))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 29} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] b) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] a)) \[CenterDot] b)) == a \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] b))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 28}, 
        "Construct" -> {"Axiom", 2}, "Position" -> {2, 1, 1}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                a)) \[CenterDot] b)) == a \[CenterDot] (a \[CenterDot] 
             (a \[CenterDot] b))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 30} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) == 
         a \[CenterDot] (a \[CenterDot] (a \[CenterDot] b))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 29}, 
        "Construct" -> {"Axiom", 2}, "Position" -> {2, 2, 1}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] b)) == a \[CenterDot] (a \[CenterDot] 
             (a \[CenterDot] b))], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 23} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] b)], "Proof" -> <|"Construct" -> {"Axiom", 2}, 
        "Orientation" -> 1, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
           ((b_) \[CenterDot] (a_)) -> a, "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] (a_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 3}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 24} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] b == 
         ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
          a], "Proof" -> <|"Construct" -> {"Axiom", 2}, "Orientation" -> 1, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
            (a_)) -> a, "Side" -> 1, "Subpattern" -> (b_) \[CenterDot] (a_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 23}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] (b_)) -> a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 31} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] b == a \[CenterDot] 
          ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 24}, 
        "Construct" -> {"SubstitutionLemma", 2}, "Position" -> {}, 
        "Rule" -> ((b_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (b \[CenterDot] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CenterDot] b == 
           a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
              b))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 32} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] b == a \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] b))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 30}, 
        "Construct" -> {"SubstitutionLemma", 31}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((a_) \[CenterDot] (b_))) -> a \[CenterDot] b, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[a \[CenterDot] b == 
           a \[CenterDot] (a \[CenterDot] (a \[CenterDot] b))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 33} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] b == a \[CenterDot] 
          (a \[CenterDot] (b \[CenterDot] b))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 24}, 
        "Construct" -> {"SubstitutionLemma", 32}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (b_))) -> a \[CenterDot] b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CenterDot] b == 
           a \[CenterDot] (a \[CenterDot] (b \[CenterDot] b))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 34} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] (a \[CenterDot] b) == 
         (a \[CenterDot] a) \[CenterDot] b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 23}, 
        "Construct" -> {"SubstitutionLemma", 33}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] 
             (b_))) -> a \[CenterDot] b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[b \[CenterDot] (a \[CenterDot] b) == 
           (a \[CenterDot] a) \[CenterDot] b], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 25} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] a) == 
         a \[CenterDot] (b \[CenterDot] b)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 34}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
           (b_) -> b \[CenterDot] (a \[CenterDot] b), "Side" -> 1, 
        "Subpattern" -> {}, "MatchingConstruct" -> {"SubstitutionLemma", 3}, 
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
           (b \[CenterDot] a), "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"SubstitutionLemma", 35} -> 
     <|"Statement" -> HoldForm[
        (b \[CenterDot] ((c \[CenterDot] c) \[CenterDot] b)) \[CenterDot] 
          a == a \[CenterDot] (b \[CenterDot] (a \[CenterDot] 
            (c \[CenterDot] a)))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 26}, "Construct" -> 
         {"SubstitutionLemma", 3}, "Position" -> {1}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (b \[CenterDot] ((c \[CenterDot] c) \[CenterDot] b)) \[CenterDot] 
            a == a \[CenterDot] (b \[CenterDot] (a \[CenterDot] 
              (c \[CenterDot] a)))], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 27} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((b \[CenterDot] b) \[CenterDot] (b \[CenterDot] b)) == 
         a \[CenterDot] ((b \[CenterDot] b) \[CenterDot] a)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 18}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
            (b_)) -> a \[CenterDot] (b \[CenterDot] b), "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 2}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (b_)) -> 
          (b \[CenterDot] b) \[CenterDot] a, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 36} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] b == a \[CenterDot] 
          ((b \[CenterDot] b) \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 27}, 
        "Construct" -> {"Axiom", 2}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[a \[CenterDot] b == a \[CenterDot] 
            ((b \[CenterDot] b) \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 37} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] c) \[CenterDot] a == 
         a \[CenterDot] (b \[CenterDot] (a \[CenterDot] (c \[CenterDot] 
             a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 35}, 
        "Construct" -> {"SubstitutionLemma", 36}, "Position" -> {1}, 
        "Rule" -> (a_) \[CenterDot] (((b_) \[CenterDot] (b_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(b \[CenterDot] c) \[CenterDot] a == 
           a \[CenterDot] (b \[CenterDot] (a \[CenterDot] (c \[CenterDot] 
               a)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 38} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] a) == 
         a \[CenterDot] (b \[CenterDot] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 14}, 
        "Construct" -> {"SubstitutionLemma", 36}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (((b_) \[CenterDot] (b_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CenterDot] (b \[CenterDot] a) == 
           a \[CenterDot] (b \[CenterDot] b)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 28} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] c) \[CenterDot] a == 
         a \[CenterDot] (b \[CenterDot] (a \[CenterDot] (c \[CenterDot] 
             c)))], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 37}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            ((a_) \[CenterDot] ((c_) \[CenterDot] (a_)))) -> 
          (b \[CenterDot] c) \[CenterDot] a, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] ((c_) \[CenterDot] (a_)), 
        "MatchingConstruct" -> {"SubstitutionLemma", 38}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CenterDot] ((b_) \[CenterDot] (a_)) -> a \[CenterDot] 
           (b \[CenterDot] b), "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"CriticalPairLemma", 29} -> 
     <|"Statement" -> HoldForm[(c \[CenterDot] b) \[CenterDot] a == 
         a \[CenterDot] ((a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
           c)], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 37}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            ((a_) \[CenterDot] ((c_) \[CenterDot] (a_)))) -> 
          (b \[CenterDot] c) \[CenterDot] a, "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] ((a_) \[CenterDot] 
           ((c_) \[CenterDot] (a_))), "MatchingConstruct" -> 
         {"SubstitutionLemma", 3}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 30} -> 
     <|"Statement" -> HoldForm[
        ((a \[CenterDot] (c \[CenterDot] a)) \[CenterDot] b) \[CenterDot] 
          a == ((a \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
           c) \[CenterDot] a], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 28}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] ((a_) \[CenterDot] 
             ((c_) \[CenterDot] (c_)))) -> (b \[CenterDot] c) \[CenterDot] a, 
        "Side" -> 1, "Subpattern" -> {}, "MatchingConstruct" -> 
         {"CriticalPairLemma", 29}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CenterDot] (((a_) \[CenterDot] 
             ((b_) \[CenterDot] (a_))) \[CenterDot] (c_)) -> 
          (c \[CenterDot] b) \[CenterDot] a, "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"SubstitutionLemma", 39} -> 
     <|"Statement" -> HoldForm[
        ((a \[CenterDot] (c \[CenterDot] a)) \[CenterDot] b) \[CenterDot] 
          a == a \[CenterDot] ((a \[CenterDot] (b \[CenterDot] 
             b)) \[CenterDot] c)], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 30}, "Construct" -> 
         {"SubstitutionLemma", 3}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          ((a \[CenterDot] (c \[CenterDot] a)) \[CenterDot] b) \[CenterDot] 
            a == a \[CenterDot] ((a \[CenterDot] (b \[CenterDot] 
               b)) \[CenterDot] c)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 31} -> 
     <|"Statement" -> HoldForm[(c \[CenterDot] b) \[CenterDot] a == 
         a \[CenterDot] ((a \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
           c)], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 29}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] 
           (((a_) \[CenterDot] ((b_) \[CenterDot] (a_))) \[CenterDot] 
            (c_)) -> (c \[CenterDot] b) \[CenterDot] a, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] ((b_) \[CenterDot] (a_)), 
        "MatchingConstruct" -> {"SubstitutionLemma", 38}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CenterDot] ((b_) \[CenterDot] (a_)) -> a \[CenterDot] 
           (b \[CenterDot] b), "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"SubstitutionLemma", 40} -> 
     <|"Statement" -> HoldForm[
        ((a \[CenterDot] (c \[CenterDot] a)) \[CenterDot] b) \[CenterDot] 
          a == (c \[CenterDot] b) \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 39}, 
        "Construct" -> {"CriticalPairLemma", 31}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] ((b_) \[CenterDot] 
              (b_))) \[CenterDot] (c_)) -> (c \[CenterDot] b) \[CenterDot] a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          ((a \[CenterDot] (c \[CenterDot] a)) \[CenterDot] b) \[CenterDot] 
            a == (c \[CenterDot] b) \[CenterDot] a], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 41} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] (c \[CenterDot] a)) \[CenterDot] b) == 
         (c \[CenterDot] b) \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 40}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a \[CenterDot] ((a \[CenterDot] (c \[CenterDot] a)) \[CenterDot] 
             b) == (c \[CenterDot] b) \[CenterDot] a], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 42} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] c) \[CenterDot] a == 
         (c \[CenterDot] b) \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 41}, 
        "Construct" -> {"CriticalPairLemma", 29}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] ((b_) \[CenterDot] 
              (a_))) \[CenterDot] (c_)) -> (c \[CenterDot] b) \[CenterDot] a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (b \[CenterDot] c) \[CenterDot] a == 
           (c \[CenterDot] b) \[CenterDot] a], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 32} -> 
     <|"Statement" -> HoldForm[(c \[CenterDot] a) \[CenterDot] b == 
         (((a \[CenterDot] b) \[CenterDot] c) \[CenterDot] c) \[CenterDot] 
          b], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 37}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            ((a_) \[CenterDot] ((c_) \[CenterDot] (a_)))) -> 
          (b \[CenterDot] c) \[CenterDot] a, "Side" -> 1, "Subpattern" -> {}, 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            ((a_) \[CenterDot] (c_))) -> ((c \[CenterDot] b) \[CenterDot] 
            b) \[CenterDot] a, "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"SubstitutionLemma", 44} -> 
     <|"Statement" -> HoldForm[(c \[CenterDot] a) \[CenterDot] b == 
         b \[CenterDot] (((a \[CenterDot] b) \[CenterDot] c) \[CenterDot] 
           c)], "Proof" -> <|"Input" -> {"CriticalPairLemma", 32}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (c \[CenterDot] a) \[CenterDot] b == b \[CenterDot] 
            (((a \[CenterDot] b) \[CenterDot] c) \[CenterDot] c)], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 45} -> 
     <|"Statement" -> HoldForm[(c \[CenterDot] a) \[CenterDot] b == 
         b \[CenterDot] (c \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            c))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 44}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (c \[CenterDot] a) \[CenterDot] b == b \[CenterDot] 
            (c \[CenterDot] ((a \[CenterDot] b) \[CenterDot] c))], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 33} -> 
     <|"Statement" -> HoldForm[(((b \[CenterDot] a) \[CenterDot] 
            c) \[CenterDot] b) \[CenterDot] a == a \[CenterDot] 
          (((b \[CenterDot] a) \[CenterDot] c) \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] (c \[CenterDot] c)))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 45}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            (((c_) \[CenterDot] (a_)) \[CenterDot] (b_))) -> 
          (b \[CenterDot] c) \[CenterDot] a, "Side" -> 1, 
        "Subpattern" -> ((c_) \[CenterDot] (a_)) \[CenterDot] (b_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 18}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CenterDot] ((a_) \[CenterDot] (b_)) -> a \[CenterDot] 
           (b \[CenterDot] b), "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"CriticalPairLemma", 34} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] b) == (a \[CenterDot] b) \[CenterDot] 
          ((a \[CenterDot] b) \[CenterDot] a)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 17}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
            ((b_) \[CenterDot] (a_))) -> b \[CenterDot] a, "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] (a_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 23}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (b_)) -> a, "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"SubstitutionLemma", 46} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] b) == (a \[CenterDot] b) \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] b))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 34}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (a \[CenterDot] a) \[CenterDot] (a \[CenterDot] b) == 
           (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
              b))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 47} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] b) \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] b))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 46}, 
        "Construct" -> {"CriticalPairLemma", 23}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            (b_)) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[a == (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
             (a \[CenterDot] b))], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 35} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] (b \[CenterDot] 
            b)) \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
            (a \[CenterDot] b)))], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 47}, "Orientation" -> -1, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
            ((a_) \[CenterDot] (b_))) -> a, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 18}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CenterDot] ((a_) \[CenterDot] (b_)) -> 
          a \[CenterDot] (b \[CenterDot] b), "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 48} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] (b \[CenterDot] 
            b)) \[CenterDot] (a \[CenterDot] b)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 35}, 
        "Construct" -> {"SubstitutionLemma", 32}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (b_))) -> a \[CenterDot] b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a == (a \[CenterDot] 
             (b \[CenterDot] b)) \[CenterDot] (a \[CenterDot] b)], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 49} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] b) \[CenterDot] 
          (a \[CenterDot] (b \[CenterDot] b))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 48}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a == (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
             (b \[CenterDot] b))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 50} -> 
     <|"Statement" -> HoldForm[(((b \[CenterDot] a) \[CenterDot] 
            c) \[CenterDot] b) \[CenterDot] a == a \[CenterDot] 
          (b \[CenterDot] a)], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 33}, "Construct" -> 
         {"SubstitutionLemma", 49}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
            ((b_) \[CenterDot] (b_))) -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[(((b \[CenterDot] a) \[CenterDot] 
              c) \[CenterDot] b) \[CenterDot] a == a \[CenterDot] 
            (b \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 51} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          (((b \[CenterDot] a) \[CenterDot] c) \[CenterDot] b) == 
         a \[CenterDot] (b \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 50}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a \[CenterDot] (((b \[CenterDot] a) \[CenterDot] c) \[CenterDot] 
             b) == a \[CenterDot] (b \[CenterDot] a)], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 52} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] c)) == a \[CenterDot] 
          (b \[CenterDot] a)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 51}, "Construct" -> 
         {"SubstitutionLemma", 3}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
              c)) == a \[CenterDot] (b \[CenterDot] a)], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 36} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
          (b \[CenterDot] (a \[CenterDot] b)) == 
         (a \[CenterDot] b) \[CenterDot] ((c \[CenterDot] a) \[CenterDot] 
           b)], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 52}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            (((b_) \[CenterDot] (a_)) \[CenterDot] (c_))) -> 
          a \[CenterDot] (b \[CenterDot] a), "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] (((b_) \[CenterDot] 
            (a_)) \[CenterDot] (c_)), "MatchingConstruct" -> 
         {"CriticalPairLemma", 29}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CenterDot] (((a_) \[CenterDot] 
             ((b_) \[CenterDot] (a_))) \[CenterDot] (c_)) -> 
          (c \[CenterDot] b) \[CenterDot] a, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 53} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
          (b \[CenterDot] b) == (a \[CenterDot] b) \[CenterDot] 
          ((c \[CenterDot] a) \[CenterDot] b)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 36}, 
        "Construct" -> {"SubstitutionLemma", 38}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (a_)) -> 
          a \[CenterDot] (b \[CenterDot] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
            (b \[CenterDot] b) == (a \[CenterDot] b) \[CenterDot] 
            ((c \[CenterDot] a) \[CenterDot] b)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 54} -> 
     <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
          ((c \[CenterDot] a) \[CenterDot] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 53}, 
        "Construct" -> {"CriticalPairLemma", 8}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
            (b_)) -> b, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
            ((c \[CenterDot] a) \[CenterDot] b)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 37} -> 
     <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
          (b \[CenterDot] (c \[CenterDot] a))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 54}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((c_) \[CenterDot] (a_)) \[CenterDot] (b_)) -> b, "Side" -> 1, 
        "Subpattern" -> ((c_) \[CenterDot] (a_)) \[CenterDot] (b_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 3}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CenterDot] (b_) -> b \[CenterDot] a, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 38} -> 
     <|"Statement" -> HoldForm[
        ((a \[CenterDot] (b \[CenterDot] c)) \[CenterDot] c) \[CenterDot] 
          a == a \[CenterDot] ((a \[CenterDot] (b \[CenterDot] 
             c)) \[CenterDot] a)], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 45}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            (((c_) \[CenterDot] (a_)) \[CenterDot] (b_))) -> 
          (b \[CenterDot] c) \[CenterDot] a, "Side" -> 1, 
        "Subpattern" -> ((c_) \[CenterDot] (a_)) \[CenterDot] (b_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 37}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
            ((c_) \[CenterDot] (a_))) -> b, "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"SubstitutionLemma", 55} -> 
     <|"Statement" -> HoldForm[
        ((a \[CenterDot] (b \[CenterDot] c)) \[CenterDot] c) \[CenterDot] 
          a == a \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] c)))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 38}, "Construct" -> 
         {"SubstitutionLemma", 3}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          ((a \[CenterDot] (b \[CenterDot] c)) \[CenterDot] c) \[CenterDot] 
            a == a \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
              (b \[CenterDot] c)))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 56} -> 
     <|"Statement" -> HoldForm[
        ((a \[CenterDot] (b \[CenterDot] c)) \[CenterDot] c) \[CenterDot] 
          a == a \[CenterDot] (b \[CenterDot] c)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 55}, 
        "Construct" -> {"SubstitutionLemma", 32}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (b_))) -> a \[CenterDot] b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[
          ((a \[CenterDot] (b \[CenterDot] c)) \[CenterDot] c) \[CenterDot] 
            a == a \[CenterDot] (b \[CenterDot] c)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 57} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] (b \[CenterDot] c)) \[CenterDot] c) == 
         a \[CenterDot] (b \[CenterDot] c)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 56}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a \[CenterDot] ((a \[CenterDot] (b \[CenterDot] c)) \[CenterDot] 
             c) == a \[CenterDot] (b \[CenterDot] c)], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 58} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (c \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] c))) == a \[CenterDot] 
          (b \[CenterDot] c)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 57}, "Construct" -> 
         {"SubstitutionLemma", 3}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a \[CenterDot] (c \[CenterDot] (a \[CenterDot] (b \[CenterDot] 
               c))) == a \[CenterDot] (b \[CenterDot] c)], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 39} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (c \[CenterDot] b) == 
         a \[CenterDot] (b \[CenterDot] (a \[CenterDot] (b \[CenterDot] 
             c)))], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 58}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            ((a_) \[CenterDot] ((c_) \[CenterDot] (b_)))) -> 
          a \[CenterDot] (c \[CenterDot] b), "Side" -> 1, 
        "Subpattern" -> (c_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 3}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "MatchingSide" -> 1, "Position" -> {2, 2, 2}|>|>, 
    {"CriticalPairLemma", 40} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] b) \[CenterDot] 
          ((c \[CenterDot] b) \[CenterDot] a)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 54}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((c_) \[CenterDot] (a_)) \[CenterDot] (b_)) -> b, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 3}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 41} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] (b \[CenterDot] 
            c)) \[CenterDot] (b \[CenterDot] a)], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 40}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((c_) \[CenterDot] (b_)) \[CenterDot] (a_)) -> a, "Side" -> 1, 
        "Subpattern" -> (c_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 23}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (b_)) -> a, "MatchingSide" -> 1, 
        "Position" -> {2, 1}|>|>, {"SubstitutionLemma", 59} -> 
     <|"Statement" -> HoldForm[a == (b \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] (b \[CenterDot] c))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 41}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a == (b \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
             (b \[CenterDot] c))], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 42} -> 
     <|"Statement" -> HoldForm[
        ((a \[CenterDot] (b \[CenterDot] c)) \[CenterDot] b) \[CenterDot] 
          a == a \[CenterDot] ((a \[CenterDot] (b \[CenterDot] 
             c)) \[CenterDot] a)], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 45}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            (((c_) \[CenterDot] (a_)) \[CenterDot] (b_))) -> 
          (b \[CenterDot] c) \[CenterDot] a, "Side" -> 1, 
        "Subpattern" -> ((c_) \[CenterDot] (a_)) \[CenterDot] (b_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 59}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
            ((a_) \[CenterDot] (c_))) -> b, "MatchingSide" -> 1, 
        "Position" -> {2, 2}|>|>, {"SubstitutionLemma", 60} -> 
     <|"Statement" -> HoldForm[
        ((a \[CenterDot] (b \[CenterDot] c)) \[CenterDot] b) \[CenterDot] 
          a == a \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] c)))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 42}, "Construct" -> 
         {"SubstitutionLemma", 3}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          ((a \[CenterDot] (b \[CenterDot] c)) \[CenterDot] b) \[CenterDot] 
            a == a \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
              (b \[CenterDot] c)))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 61} -> 
     <|"Statement" -> HoldForm[
        ((a \[CenterDot] (b \[CenterDot] c)) \[CenterDot] b) \[CenterDot] 
          a == a \[CenterDot] (b \[CenterDot] c)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 60}, 
        "Construct" -> {"SubstitutionLemma", 32}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (b_))) -> a \[CenterDot] b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[
          ((a \[CenterDot] (b \[CenterDot] c)) \[CenterDot] b) \[CenterDot] 
            a == a \[CenterDot] (b \[CenterDot] c)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 62} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] (b \[CenterDot] c)) \[CenterDot] b) == 
         a \[CenterDot] (b \[CenterDot] c)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 61}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a \[CenterDot] ((a \[CenterDot] (b \[CenterDot] c)) \[CenterDot] 
             b) == a \[CenterDot] (b \[CenterDot] c)], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 63} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] c))) == a \[CenterDot] 
          (b \[CenterDot] c)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 62}, "Construct" -> 
         {"SubstitutionLemma", 3}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a \[CenterDot] (b \[CenterDot] (a \[CenterDot] (b \[CenterDot] 
               c))) == a \[CenterDot] (b \[CenterDot] c)], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 64} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (c \[CenterDot] b) == 
         a \[CenterDot] (b \[CenterDot] c)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 39}, 
        "Construct" -> {"SubstitutionLemma", 63}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] ((a_) \[CenterDot] 
             ((b_) \[CenterDot] (c_)))) -> a \[CenterDot] (b \[CenterDot] c), 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CenterDot] (c \[CenterDot] b) == a \[CenterDot] 
            (b \[CenterDot] c)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 43} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] (b \[CenterDot] b) == 
         (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b))) \[CenterDot] 
          ((b \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
           (a \[CenterDot] a))], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 22}, "Orientation" -> -1, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
            ((a_) \[CenterDot] (b_))) -> b, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 12}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            ((b_) \[CenterDot] (b_))) -> a \[CenterDot] a, 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"CriticalPairLemma", 44} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] b == 
         ((a \[CenterDot] a) \[CenterDot] a) \[CenterDot] b], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 5}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            ((a_) \[CenterDot] (b_))) -> a \[CenterDot] a, "Side" -> 1, 
        "Subpattern" -> {}, "MatchingConstruct" -> {"Axiom", 1}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CenterDot] ((b_) \[CenterDot] ((a_) \[CenterDot] (c_))) -> 
          ((c \[CenterDot] b) \[CenterDot] b) \[CenterDot] a, 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"SubstitutionLemma", 67} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] b == 
         (a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] b], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 44}, 
        "Construct" -> {"SubstitutionLemma", 2}, "Position" -> {1}, 
        "Rule" -> ((b_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (b \[CenterDot] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[b \[CenterDot] b == 
           (a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] b], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 45} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] a) == (a \[CenterDot] a) \[CenterDot] 
          (b \[CenterDot] (b \[CenterDot] b))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 67}, 
        "Orientation" -> -1, "Rule" -> 
         ((a_) \[CenterDot] ((a_) \[CenterDot] (a_))) \[CenterDot] (b_) -> 
          b \[CenterDot] b, "Side" -> 1, "Subpattern" -> {}, 
        "MatchingConstruct" -> {"SubstitutionLemma", 2}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CenterDot] ((b_) \[CenterDot] (b_)) -> 
          (b \[CenterDot] b) \[CenterDot] a, "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"SubstitutionLemma", 68} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          (b \[CenterDot] (b \[CenterDot] b))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 45}, 
        "Construct" -> {"Axiom", 2}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[a == (a \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
             (b \[CenterDot] b))], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 46} -> 
     <|"Statement" -> HoldForm[b == (a \[CenterDot] (a \[CenterDot] 
            a)) \[CenterDot] (b \[CenterDot] b)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 68}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
           ((b_) \[CenterDot] ((b_) \[CenterDot] (b_))) -> a, "Side" -> 1, 
        "Subpattern" -> {}, "MatchingConstruct" -> {"SubstitutionLemma", 3}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CenterDot] (b_) -> b \[CenterDot] a, "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"SubstitutionLemma", 69} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] (b \[CenterDot] b) == 
         (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b))) \[CenterDot] 
          a], "Proof" -> <|"Input" -> {"CriticalPairLemma", 43}, 
        "Construct" -> {"CriticalPairLemma", 46}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] (a_))) \[CenterDot] 
           ((b_) \[CenterDot] (b_)) -> b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[b \[CenterDot] (b \[CenterDot] b) == 
           (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b))) \[CenterDot] 
            a], "Source" -> "norm"|>|>, {"SubstitutionLemma", 70} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] (b \[CenterDot] b) == 
         a \[CenterDot] (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] 
             b)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 69}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          b \[CenterDot] (b \[CenterDot] b) == a \[CenterDot] 
            (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b)))], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 47} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] (b \[CenterDot] 
            (b \[CenterDot] b))) \[CenterDot] (a \[CenterDot] a) == 
         (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b))) \[CenterDot] 
          (b \[CenterDot] (b \[CenterDot] b))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 38}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            (a_)) -> a \[CenterDot] (b \[CenterDot] b), "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] (a_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 70}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
            ((b_) \[CenterDot] ((b_) \[CenterDot] (b_)))) -> 
          b \[CenterDot] (b \[CenterDot] b), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 71} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] (b \[CenterDot] 
            (b \[CenterDot] b))) \[CenterDot] (a \[CenterDot] a) == 
         (b \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
          (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b)))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 47}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b))) \[CenterDot] 
            (a \[CenterDot] a) == (b \[CenterDot] (b \[CenterDot] 
              b)) \[CenterDot] (a \[CenterDot] (b \[CenterDot] 
              (b \[CenterDot] b)))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 72} -> 
     <|"Statement" -> HoldForm[a == (b \[CenterDot] (b \[CenterDot] 
            b)) \[CenterDot] (a \[CenterDot] (b \[CenterDot] 
            (b \[CenterDot] b)))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 71}, "Construct" -> 
         {"CriticalPairLemma", 9}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[a == (b \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
            (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b)))], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 48} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] a) == (a \[CenterDot] a) \[CenterDot] 
          (b \[CenterDot] ((c \[CenterDot] a) \[CenterDot] b))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 11}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            (((c_) \[CenterDot] ((a_) \[CenterDot] (a_))) \[CenterDot] 
             (b_))) -> a \[CenterDot] a, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (a_), "MatchingConstruct" -> 
         {"Axiom", 2}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] (a_)) -> a, 
        "MatchingSide" -> 1, "Position" -> {2, 2, 1, 2}|>|>, 
    {"SubstitutionLemma", 73} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          (b \[CenterDot] ((c \[CenterDot] a) \[CenterDot] b))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 48}, 
        "Construct" -> {"Axiom", 2}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[a == (a \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
             ((c \[CenterDot] a) \[CenterDot] b))], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 49} -> 
     <|"Statement" -> HoldForm[x3 == 
         ((a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a)) \[CenterDot] 
           x3) \[CenterDot] (c \[CenterDot] x3)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 54}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((c_) \[CenterDot] (a_)) \[CenterDot] (b_)) -> b, "Side" -> 1, 
        "Subpattern" -> (c_) \[CenterDot] (a_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 73}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
           ((b_) \[CenterDot] (((c_) \[CenterDot] (a_)) \[CenterDot] 
             (b_))) -> a, "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"SubstitutionLemma", 74} -> 
     <|"Statement" -> HoldForm[x3 == (c \[CenterDot] x3) \[CenterDot] 
          ((a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a)) \[CenterDot] 
           x3)], "Proof" -> <|"Input" -> {"CriticalPairLemma", 49}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          x3 == (c \[CenterDot] x3) \[CenterDot] ((a \[CenterDot] 
              ((b \[CenterDot] c) \[CenterDot] a)) \[CenterDot] x3)], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 50} -> 
     <|"Statement" -> HoldForm[c \[CenterDot] 
          ((a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a))) == 
         ((a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a)) \[CenterDot] 
           ((a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
              a)))) \[CenterDot] ((a \[CenterDot] 
            ((b \[CenterDot] c) \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a)))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 72}, 
        "Orientation" -> -1, "Rule" -> 
         ((a_) \[CenterDot] ((a_) \[CenterDot] (a_))) \[CenterDot] 
           ((b_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
              (a_)))) -> b, "Side" -> 1, "Subpattern" -> 
         (b_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] (a_))), 
        "MatchingConstruct" -> {"SubstitutionLemma", 74}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] (b_)) \[CenterDot] (((c_) \[CenterDot] 
             (((x3_) \[CenterDot] (a_)) \[CenterDot] (c_))) \[CenterDot] 
            (b_)) -> b, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 75} -> 
     <|"Statement" -> HoldForm[c \[CenterDot] 
          ((a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a))) == 
         ((a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a))) \[CenterDot] 
          ((a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a)) \[CenterDot] 
           ((a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a))))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 50}, 
        "Construct" -> {"SubstitutionLemma", 2}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (b_)) -> 
          (b \[CenterDot] b) \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[c \[CenterDot] 
            ((a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
               a)) \[CenterDot] (a \[CenterDot] ((b \[CenterDot] 
                c) \[CenterDot] a))) == ((a \[CenterDot] ((b \[CenterDot] 
                c) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
              ((b \[CenterDot] c) \[CenterDot] a))) \[CenterDot] 
            ((a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
               a)) \[CenterDot] ((a \[CenterDot] ((b \[CenterDot] 
                 c) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] (
                (b \[CenterDot] c) \[CenterDot] a))))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 76} -> 
     <|"Statement" -> HoldForm[c \[CenterDot] 
          ((a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a))) == 
         a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 75}, 
        "Construct" -> {"CriticalPairLemma", 23}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            (b_)) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[c \[CenterDot] ((a \[CenterDot] ((b \[CenterDot] 
                c) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
              ((b \[CenterDot] c) \[CenterDot] a))) == a \[CenterDot] 
            ((b \[CenterDot] c) \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 77} -> 
     <|"Statement" -> HoldForm[c \[CenterDot] (c \[CenterDot] 
           (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a))) == 
         a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 76}, 
        "Construct" -> {"SubstitutionLemma", 18}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (b_)) -> 
          a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[c \[CenterDot] (c \[CenterDot] 
             (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a))) == 
           a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a)], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 51} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] c) \[CenterDot] a == 
         a \[CenterDot] (b \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
             c)))], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 37}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            ((a_) \[CenterDot] ((c_) \[CenterDot] (a_)))) -> 
          (b \[CenterDot] c) \[CenterDot] a, "Side" -> 1, 
        "Subpattern" -> (c_) \[CenterDot] (a_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 3}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "MatchingSide" -> 1, "Position" -> {2, 2, 2}|>|>, 
    {"CriticalPairLemma", 52} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] (c \[CenterDot] 
            ((x3 \[CenterDot] a) \[CenterDot] c))) \[CenterDot] a == 
         a \[CenterDot] (b \[CenterDot] (a \[CenterDot] 
            ((c \[CenterDot] x3) \[CenterDot] a)))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 51}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            ((a_) \[CenterDot] ((a_) \[CenterDot] (c_)))) -> 
          (b \[CenterDot] c) \[CenterDot] a, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (c_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 45}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            (((c_) \[CenterDot] (a_)) \[CenterDot] (b_))) -> 
          (b \[CenterDot] c) \[CenterDot] a, "MatchingSide" -> 1, 
        "Position" -> {2, 2, 2}|>|>, {"SubstitutionLemma", 78} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] (c \[CenterDot] 
            ((x3 \[CenterDot] a) \[CenterDot] c))) \[CenterDot] a == 
         (b \[CenterDot] (c \[CenterDot] x3)) \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 52}, 
        "Construct" -> {"SubstitutionLemma", 37}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] ((a_) \[CenterDot] 
             ((c_) \[CenterDot] (a_)))) -> (b \[CenterDot] c) \[CenterDot] a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (b \[CenterDot] (c \[CenterDot] ((x3 \[CenterDot] a) \[CenterDot] 
               c))) \[CenterDot] a == (b \[CenterDot] (c \[CenterDot] 
              x3)) \[CenterDot] a], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 79} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
           (c \[CenterDot] ((x3 \[CenterDot] a) \[CenterDot] c))) == 
         (b \[CenterDot] (c \[CenterDot] x3)) \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 78}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a \[CenterDot] (b \[CenterDot] (c \[CenterDot] ((x3 \[CenterDot] 
                a) \[CenterDot] c))) == (b \[CenterDot] (c \[CenterDot] 
              x3)) \[CenterDot] a], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 80} -> 
     <|"Statement" -> HoldForm[(c \[CenterDot] (a \[CenterDot] 
            b)) \[CenterDot] c == a \[CenterDot] 
          ((b \[CenterDot] c) \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 77}, 
        "Construct" -> {"SubstitutionLemma", 79}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] ((c_) \[CenterDot] 
             (((x3_) \[CenterDot] (a_)) \[CenterDot] (c_)))) -> 
          (b \[CenterDot] (c \[CenterDot] x3)) \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (c \[CenterDot] (a \[CenterDot] b)) \[CenterDot] c == 
           a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a)], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 81} -> 
     <|"Statement" -> HoldForm[c \[CenterDot] (c \[CenterDot] 
           (a \[CenterDot] b)) == a \[CenterDot] 
          ((b \[CenterDot] c) \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 80}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          c \[CenterDot] (c \[CenterDot] (a \[CenterDot] b)) == 
           a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a)], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 53} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] c) \[CenterDot] a == 
         a \[CenterDot] (b \[CenterDot] ((a \[CenterDot] c) \[CenterDot] 
            b))], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 45}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            (((c_) \[CenterDot] (a_)) \[CenterDot] (b_))) -> 
          (b \[CenterDot] c) \[CenterDot] a, "Side" -> 1, 
        "Subpattern" -> (c_) \[CenterDot] (a_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 3}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "MatchingSide" -> 1, "Position" -> {2, 2, 1}|>|>, 
    {"CriticalPairLemma", 54} -> 
     <|"Statement" -> HoldForm[(((a \[CenterDot] b) \[CenterDot] 
            c) \[CenterDot] b) \[CenterDot] a == a \[CenterDot] 
          (((a \[CenterDot] b) \[CenterDot] c) \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] (c \[CenterDot] c)))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 53}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            (((a_) \[CenterDot] (c_)) \[CenterDot] (b_))) -> 
          (b \[CenterDot] c) \[CenterDot] a, "Side" -> 1, 
        "Subpattern" -> ((a_) \[CenterDot] (c_)) \[CenterDot] (b_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 18}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CenterDot] ((a_) \[CenterDot] (b_)) -> a \[CenterDot] 
           (b \[CenterDot] b), "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"CriticalPairLemma", 55} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] b) \[CenterDot] 
          (a \[CenterDot] (c \[CenterDot] b))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 40}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((c_) \[CenterDot] (b_)) \[CenterDot] (a_)) -> a, "Side" -> 1, 
        "Subpattern" -> ((c_) \[CenterDot] (b_)) \[CenterDot] (a_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 3}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CenterDot] (b_) -> b \[CenterDot] a, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 83} -> 
     <|"Statement" -> HoldForm[(((a \[CenterDot] b) \[CenterDot] 
            c) \[CenterDot] b) \[CenterDot] a == a \[CenterDot] 
          (a \[CenterDot] b)], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 54}, "Construct" -> 
         {"CriticalPairLemma", 55}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
            ((c_) \[CenterDot] (b_))) -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[(((a \[CenterDot] b) \[CenterDot] 
              c) \[CenterDot] b) \[CenterDot] a == a \[CenterDot] 
            (a \[CenterDot] b)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 84} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          (((a \[CenterDot] b) \[CenterDot] c) \[CenterDot] b) == 
         a \[CenterDot] (a \[CenterDot] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 83}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a \[CenterDot] (((a \[CenterDot] b) \[CenterDot] c) \[CenterDot] 
             b) == a \[CenterDot] (a \[CenterDot] b)], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 85} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] c)) == a \[CenterDot] 
          (a \[CenterDot] b)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 84}, "Construct" -> 
         {"SubstitutionLemma", 3}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a \[CenterDot] (b \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              c)) == a \[CenterDot] (a \[CenterDot] b)], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 56} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] a) == 
         a \[CenterDot] (b \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            c))], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 52}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            (((b_) \[CenterDot] (a_)) \[CenterDot] (c_))) -> 
          a \[CenterDot] (b \[CenterDot] a), "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] (a_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 3}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "MatchingSide" -> 1, "Position" -> {2, 2, 1}|>|>, 
    {"SubstitutionLemma", 86} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] a) == 
         a \[CenterDot] (a \[CenterDot] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 85}, 
        "Construct" -> {"CriticalPairLemma", 56}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            (((a_) \[CenterDot] (b_)) \[CenterDot] (c_))) -> 
          a \[CenterDot] (b \[CenterDot] a), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CenterDot] (b \[CenterDot] a) == 
           a \[CenterDot] (a \[CenterDot] b)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 19} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] c))) \[CenterDot] (a \[CenterDot] 
           ((b \[CenterDot] c) \[CenterDot] (b \[CenterDot] c))) == 
         (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
           c) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b)) \[CenterDot] c)], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Construct" -> 
         {"SubstitutionLemma", 18}, "Position" -> {1}, 
        "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (b_)) -> 
          a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) \[CenterDot] 
            (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
              (b \[CenterDot] c))) == (((a \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] b)) \[CenterDot] c) \[CenterDot] 
            (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
               b)) \[CenterDot] c)], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 20} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] c))) \[CenterDot] (a \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] c))) == 
         (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
           c) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b)) \[CenterDot] c)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 19}, 
        "Construct" -> {"SubstitutionLemma", 18}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (b_)) -> 
          a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) == 
           (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
             c) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] b)) \[CenterDot] c)], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 43} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] c))) \[CenterDot] (a \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] c))) == 
         (c \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
             b))) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b)) \[CenterDot] c)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 20}, 
        "Construct" -> {"SubstitutionLemma", 42}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] (c_) -> 
          (b \[CenterDot] a) \[CenterDot] c, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[
          (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) == 
           (c \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
               b))) \[CenterDot] (((a \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] b)) \[CenterDot] c)], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 65} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] c))) \[CenterDot] (a \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] c))) == 
         (c \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
             b))) \[CenterDot] (c \[CenterDot] ((a \[CenterDot] 
             b) \[CenterDot] (a \[CenterDot] b)))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 43}, 
        "Construct" -> {"SubstitutionLemma", 64}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (c_)) -> 
          a \[CenterDot] (c \[CenterDot] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[
          (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) == 
           (c \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
               b))) \[CenterDot] (c \[CenterDot] ((a \[CenterDot] 
               b) \[CenterDot] (a \[CenterDot] b)))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 66} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] c))) \[CenterDot] (a \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] c))) == 
         (c \[CenterDot] (c \[CenterDot] (a \[CenterDot] b))) \[CenterDot] 
          (c \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b)))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 65}, "Construct" -> 
         {"SubstitutionLemma", 18}, "Position" -> {1}, 
        "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (b_)) -> 
          a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[
          (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) == 
           (c \[CenterDot] (c \[CenterDot] (a \[CenterDot] b))) \[CenterDot] 
            (c \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] b)))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 82} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] c))) \[CenterDot] (a \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] c))) == 
         (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a)) \[CenterDot] 
          (c \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b)))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 66}, "Construct" -> 
         {"SubstitutionLemma", 81}, "Position" -> {1}, 
        "Rule" -> (c_) \[CenterDot] ((c_) \[CenterDot] ((a_) \[CenterDot] 
             (b_))) -> a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a), 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) == 
           (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a)) \[CenterDot] 
            (c \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] b)))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 87} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] c))) \[CenterDot] (a \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] c))) == 
         (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) \[CenterDot] 
          (c \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            (a \[CenterDot] b)))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 82}, "Construct" -> 
         {"SubstitutionLemma", 86}, "Position" -> {1}, 
        "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (a_)) -> 
          a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[
          (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) == 
           (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) \[CenterDot] 
            (c \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] b)))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 88} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] c))) \[CenterDot] (a \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] c))) == 
         (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) \[CenterDot] 
          (c \[CenterDot] (c \[CenterDot] (a \[CenterDot] b)))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 87}, 
        "Construct" -> {"SubstitutionLemma", 18}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (b_)) -> 
          a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[
          (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) == 
           (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) \[CenterDot] 
            (c \[CenterDot] (c \[CenterDot] (a \[CenterDot] b)))], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 89} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] c))) \[CenterDot] (a \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] c))) == 
         (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) \[CenterDot] 
          (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 88}, 
        "Construct" -> {"SubstitutionLemma", 81}, "Position" -> {2}, 
        "Rule" -> (c_) \[CenterDot] ((c_) \[CenterDot] ((a_) \[CenterDot] 
             (b_))) -> a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a), 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) == 
           (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) \[CenterDot] 
            (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a))], 
        "Source" -> "cpl"|>|>, {"Conclusion", 1} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] (a \[CenterDot] 
            (b \[CenterDot] c))) \[CenterDot] (a \[CenterDot] 
           (a \[CenterDot] (b \[CenterDot] c))) == 
         (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c)))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 89}, 
        "Construct" -> {"SubstitutionLemma", 86}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (a_)) -> 
          a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[
          (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) == 
           (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c))) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] (b \[CenterDot] c)))], 
        "Source" -> "cpl"|>|>}|>]
