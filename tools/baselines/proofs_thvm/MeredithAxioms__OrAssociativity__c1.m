ProofObject["EquationalLogic", Inactive[Equal][
  (a \[CenterDot] a) \[CenterDot] (((b \[CenterDot] b) \[CenterDot] 
     (c \[CenterDot] c)) \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
     (c \[CenterDot] c))), 
  (((a \[CenterDot] a) \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
    ((a \[CenterDot] a) \[CenterDot] (b \[CenterDot] b))) \[CenterDot] 
   (c \[CenterDot] c)], 
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
       HoldForm[(a \[CenterDot] a) \[CenterDot] 
          (((b \[CenterDot] b) \[CenterDot] (c \[CenterDot] c)) \[CenterDot] 
           ((b \[CenterDot] b) \[CenterDot] (c \[CenterDot] c))) == 
         (((a \[CenterDot] a) \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] (b \[CenterDot] b))) \[CenterDot] 
          (c \[CenterDot] c)], "Proof" -> <||>|>, {"CriticalPairLemma", 1} -> 
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
        "Position" -> {2}|>|>, {"SubstitutionLemma", 22} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] b == a \[CenterDot] 
          ((b \[CenterDot] b) \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 14}, 
        "Construct" -> {"Axiom", 2}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[a \[CenterDot] b == a \[CenterDot] 
            ((b \[CenterDot] b) \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 23} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] a) == 
         a \[CenterDot] (b \[CenterDot] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 14}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (((b_) \[CenterDot] (b_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CenterDot] (b \[CenterDot] a) == 
           a \[CenterDot] (b \[CenterDot] b)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 15} -> 
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
    {"SubstitutionLemma", 24} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] b) \[CenterDot] 
          (a \[CenterDot] b) == (a \[CenterDot] b) \[CenterDot] 
          (b \[CenterDot] (a \[CenterDot] b))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 15}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (b \[CenterDot] b) \[CenterDot] (a \[CenterDot] b) == 
           (a \[CenterDot] b) \[CenterDot] (b \[CenterDot] (a \[CenterDot] 
              b))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 25} -> 
     <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
          (b \[CenterDot] (a \[CenterDot] b))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 24}, 
        "Construct" -> {"Axiom", 2}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[b == (a \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
             (a \[CenterDot] b))], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 16} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] (b \[CenterDot] b) == 
         (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b))) \[CenterDot] 
          ((b \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
           (a \[CenterDot] a))], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 25}, "Orientation" -> -1, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
            ((a_) \[CenterDot] (b_))) -> b, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 12}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            ((b_) \[CenterDot] (b_))) -> a \[CenterDot] a, 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"CriticalPairLemma", 17} -> 
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
    {"SubstitutionLemma", 26} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] b == 
         (a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] b], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 17}, 
        "Construct" -> {"SubstitutionLemma", 2}, "Position" -> {1}, 
        "Rule" -> ((b_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (b \[CenterDot] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[b \[CenterDot] b == 
           (a \[CenterDot] (a \[CenterDot] a)) \[CenterDot] b], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 18} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] a) == (a \[CenterDot] a) \[CenterDot] 
          (b \[CenterDot] (b \[CenterDot] b))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 26}, 
        "Orientation" -> -1, "Rule" -> 
         ((a_) \[CenterDot] ((a_) \[CenterDot] (a_))) \[CenterDot] (b_) -> 
          b \[CenterDot] b, "Side" -> 1, "Subpattern" -> {}, 
        "MatchingConstruct" -> {"SubstitutionLemma", 2}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CenterDot] ((b_) \[CenterDot] (b_)) -> 
          (b \[CenterDot] b) \[CenterDot] a, "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"SubstitutionLemma", 27} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          (b \[CenterDot] (b \[CenterDot] b))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 18}, 
        "Construct" -> {"Axiom", 2}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[a == (a \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
             (b \[CenterDot] b))], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 19} -> 
     <|"Statement" -> HoldForm[b == (a \[CenterDot] (a \[CenterDot] 
            a)) \[CenterDot] (b \[CenterDot] b)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 27}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
           ((b_) \[CenterDot] ((b_) \[CenterDot] (b_))) -> a, "Side" -> 1, 
        "Subpattern" -> {}, "MatchingConstruct" -> {"SubstitutionLemma", 3}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CenterDot] (b_) -> b \[CenterDot] a, "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"SubstitutionLemma", 28} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] (b \[CenterDot] b) == 
         (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b))) \[CenterDot] 
          a], "Proof" -> <|"Input" -> {"CriticalPairLemma", 16}, 
        "Construct" -> {"CriticalPairLemma", 19}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] ((a_) \[CenterDot] (a_))) \[CenterDot] 
           ((b_) \[CenterDot] (b_)) -> b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[b \[CenterDot] (b \[CenterDot] b) == 
           (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b))) \[CenterDot] 
            a], "Source" -> "norm"|>|>, {"SubstitutionLemma", 29} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] (b \[CenterDot] b) == 
         a \[CenterDot] (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] 
             b)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 28}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Position" -> {}, 
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
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 23}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            (a_)) -> a \[CenterDot] (b \[CenterDot] b), "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] (a_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 29}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CenterDot] ((a_) \[CenterDot] 
            ((b_) \[CenterDot] ((b_) \[CenterDot] (b_)))) -> 
          b \[CenterDot] (b \[CenterDot] b), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 30} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] (b \[CenterDot] 
            (b \[CenterDot] b))) \[CenterDot] (a \[CenterDot] a) == 
         (b \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
          (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b)))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 20}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b))) \[CenterDot] 
            (a \[CenterDot] a) == (b \[CenterDot] (b \[CenterDot] 
              b)) \[CenterDot] (a \[CenterDot] (b \[CenterDot] 
              (b \[CenterDot] b)))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 31} -> 
     <|"Statement" -> HoldForm[a == (b \[CenterDot] (b \[CenterDot] 
            b)) \[CenterDot] (a \[CenterDot] (b \[CenterDot] 
            (b \[CenterDot] b)))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 30}, "Construct" -> 
         {"CriticalPairLemma", 9}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[a == (b \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
            (a \[CenterDot] (b \[CenterDot] (b \[CenterDot] b)))], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 21} -> 
     <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
          (b \[CenterDot] (b \[CenterDot] a))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 25}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           ((b_) \[CenterDot] ((a_) \[CenterDot] (b_))) -> b, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 3}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"CriticalPairLemma", 22} -> 
     <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
          (b \[CenterDot] (a \[CenterDot] a))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 21}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           ((b_) \[CenterDot] ((b_) \[CenterDot] (a_))) -> b, "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] ((b_) \[CenterDot] (a_)), 
        "MatchingConstruct" -> {"SubstitutionLemma", 18}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CenterDot] ((a_) \[CenterDot] (b_)) -> a \[CenterDot] 
           (b \[CenterDot] b), "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 23} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] (a \[CenterDot] b) == 
         (a \[CenterDot] a) \[CenterDot] ((b \[CenterDot] (a \[CenterDot] 
             b)) \[CenterDot] (a \[CenterDot] a))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 22}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           ((b_) \[CenterDot] ((a_) \[CenterDot] (a_))) -> b, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 5}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            ((a_) \[CenterDot] (b_))) -> a \[CenterDot] a, 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 24} -> 
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
    {"CriticalPairLemma", 25} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] b) \[CenterDot] 
          (a \[CenterDot] a) == (a \[CenterDot] (b \[CenterDot] 
            a)) \[CenterDot] (b \[CenterDot] b)], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 24}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
           ((b_) \[CenterDot] ((a_) \[CenterDot] (b_))) -> 
          (a \[CenterDot] a) \[CenterDot] (b \[CenterDot] b), "Side" -> 1, 
        "Subpattern" -> {}, "MatchingConstruct" -> {"SubstitutionLemma", 3}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CenterDot] (b_) -> b \[CenterDot] a, "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"SubstitutionLemma", 32} -> 
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
    {"CriticalPairLemma", 26} -> 
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
        "Position" -> {2}|>|>, {"SubstitutionLemma", 33} -> 
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
    {"CriticalPairLemma", 27} -> 
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
        "Position" -> {2}|>|>, {"SubstitutionLemma", 34} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] a == a \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] b)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 27}, 
        "Construct" -> {"Axiom", 2}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[a \[CenterDot] a == a \[CenterDot] 
            ((a \[CenterDot] a) \[CenterDot] b)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 28} -> 
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
        "MatchingConstruct" -> {"SubstitutionLemma", 34}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] (b_)) -> 
          a \[CenterDot] a, "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"SubstitutionLemma", 35} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
            b) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] a)) \[CenterDot] b)) == a \[CenterDot] 
          (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
           (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
            b))], "Proof" -> <|"Input" -> {"CriticalPairLemma", 28}, 
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
    {"SubstitutionLemma", 36} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
            b) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] a)) \[CenterDot] b)) == a \[CenterDot] 
          (a \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] a)) \[CenterDot] b))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 35}, 
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
    {"SubstitutionLemma", 37} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a)) \[CenterDot] 
            b) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] a)) \[CenterDot] b)) == a \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] b))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 36}, 
        "Construct" -> {"Axiom", 2}, "Position" -> {2, 2, 1}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[a \[CenterDot] ((((a \[CenterDot] a) \[CenterDot] (
                a \[CenterDot] a)) \[CenterDot] b) \[CenterDot] 
             (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                a)) \[CenterDot] b)) == a \[CenterDot] (a \[CenterDot] 
             (a \[CenterDot] b))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 38} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] b) \[CenterDot] (((a \[CenterDot] a) \[CenterDot] 
             (a \[CenterDot] a)) \[CenterDot] b)) == a \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] b))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 37}, 
        "Construct" -> {"Axiom", 2}, "Position" -> {2, 1, 1}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             (((a \[CenterDot] a) \[CenterDot] (a \[CenterDot] 
                a)) \[CenterDot] b)) == a \[CenterDot] (a \[CenterDot] 
             (a \[CenterDot] b))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 39} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) == 
         a \[CenterDot] (a \[CenterDot] (a \[CenterDot] b))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 38}, 
        "Construct" -> {"Axiom", 2}, "Position" -> {2, 2, 1}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
             (a \[CenterDot] b)) == a \[CenterDot] (a \[CenterDot] 
             (a \[CenterDot] b))], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 29} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] b)], "Proof" -> <|"Construct" -> {"Axiom", 2}, 
        "Orientation" -> 1, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
           ((b_) \[CenterDot] (a_)) -> a, "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] (a_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 3}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 30} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] b == 
         ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
          a], "Proof" -> <|"Construct" -> {"Axiom", 2}, "Orientation" -> 1, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
            (a_)) -> a, "Side" -> 1, "Subpattern" -> (b_) \[CenterDot] (a_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 29}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] (b_)) -> a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 40} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] b == a \[CenterDot] 
          ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 30}, 
        "Construct" -> {"SubstitutionLemma", 2}, "Position" -> {}, 
        "Rule" -> ((b_) \[CenterDot] (b_)) \[CenterDot] (a_) -> 
          a \[CenterDot] (b \[CenterDot] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CenterDot] b == 
           a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
              b))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 41} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] b == a \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] b))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 39}, 
        "Construct" -> {"SubstitutionLemma", 40}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((a_) \[CenterDot] (b_))) -> a \[CenterDot] b, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[a \[CenterDot] b == 
           a \[CenterDot] (a \[CenterDot] (a \[CenterDot] b))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 42} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] b == a \[CenterDot] 
          (a \[CenterDot] (b \[CenterDot] b))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 33}, 
        "Construct" -> {"SubstitutionLemma", 41}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (b_))) -> a \[CenterDot] b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CenterDot] b == 
           a \[CenterDot] (a \[CenterDot] (b \[CenterDot] b))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 43} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] (a \[CenterDot] b) == 
         (a \[CenterDot] a) \[CenterDot] b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 32}, 
        "Construct" -> {"SubstitutionLemma", 42}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] 
             (b_))) -> a \[CenterDot] b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[b \[CenterDot] (a \[CenterDot] b) == 
           (a \[CenterDot] a) \[CenterDot] b], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 31} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] a) == 
         a \[CenterDot] (b \[CenterDot] b)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 43}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
           (b_) -> b \[CenterDot] (a \[CenterDot] b), "Side" -> 1, 
        "Subpattern" -> {}, "MatchingConstruct" -> {"SubstitutionLemma", 3}, 
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
           (b \[CenterDot] a), "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"SubstitutionLemma", 44} -> 
     <|"Statement" -> HoldForm[
        (b \[CenterDot] ((c \[CenterDot] c) \[CenterDot] b)) \[CenterDot] 
          a == a \[CenterDot] (b \[CenterDot] (a \[CenterDot] 
            (c \[CenterDot] a)))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 32}, "Construct" -> 
         {"SubstitutionLemma", 3}, "Position" -> {1}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (b \[CenterDot] ((c \[CenterDot] c) \[CenterDot] b)) \[CenterDot] 
            a == a \[CenterDot] (b \[CenterDot] (a \[CenterDot] 
              (c \[CenterDot] a)))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 45} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] c) \[CenterDot] a == 
         a \[CenterDot] (b \[CenterDot] (a \[CenterDot] (c \[CenterDot] 
             a)))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 44}, 
        "Construct" -> {"SubstitutionLemma", 22}, "Position" -> {1}, 
        "Rule" -> (a_) \[CenterDot] (((b_) \[CenterDot] (b_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(b \[CenterDot] c) \[CenterDot] a == 
           a \[CenterDot] (b \[CenterDot] (a \[CenterDot] (c \[CenterDot] 
               a)))], "Source" -> "norm"|>|>, {"CriticalPairLemma", 33} -> 
     <|"Statement" -> HoldForm[(c \[CenterDot] a) \[CenterDot] b == 
         (((a \[CenterDot] b) \[CenterDot] c) \[CenterDot] c) \[CenterDot] 
          b], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 45}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            ((a_) \[CenterDot] ((c_) \[CenterDot] (a_)))) -> 
          (b \[CenterDot] c) \[CenterDot] a, "Side" -> 1, "Subpattern" -> {}, 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            ((a_) \[CenterDot] (c_))) -> ((c \[CenterDot] b) \[CenterDot] 
            b) \[CenterDot] a, "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"SubstitutionLemma", 46} -> 
     <|"Statement" -> HoldForm[(c \[CenterDot] a) \[CenterDot] b == 
         b \[CenterDot] (((a \[CenterDot] b) \[CenterDot] c) \[CenterDot] 
           c)], "Proof" -> <|"Input" -> {"CriticalPairLemma", 33}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (c \[CenterDot] a) \[CenterDot] b == b \[CenterDot] 
            (((a \[CenterDot] b) \[CenterDot] c) \[CenterDot] c)], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 47} -> 
     <|"Statement" -> HoldForm[(c \[CenterDot] a) \[CenterDot] b == 
         b \[CenterDot] (c \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            c))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 46}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Position" -> {2}, 
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
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 47}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            (((c_) \[CenterDot] (a_)) \[CenterDot] (b_))) -> 
          (b \[CenterDot] c) \[CenterDot] a, "Side" -> 1, 
        "Subpattern" -> ((c_) \[CenterDot] (a_)) \[CenterDot] (b_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 18}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CenterDot] ((a_) \[CenterDot] (b_)) -> a \[CenterDot] 
           (b \[CenterDot] b), "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"CriticalPairLemma", 35} -> 
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
        "Position" -> {2, 2}|>|>, {"SubstitutionLemma", 48} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] b) == (a \[CenterDot] b) \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] b))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 35}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (a \[CenterDot] a) \[CenterDot] (a \[CenterDot] b) == 
           (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
              b))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 49} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] b) \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] b))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 48}, 
        "Construct" -> {"CriticalPairLemma", 29}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            (b_)) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[a == (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
             (a \[CenterDot] b))], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 36} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] (b \[CenterDot] 
            b)) \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
            (a \[CenterDot] b)))], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 49}, "Orientation" -> -1, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
            ((a_) \[CenterDot] (b_))) -> a, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 18}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CenterDot] ((a_) \[CenterDot] (b_)) -> 
          a \[CenterDot] (b \[CenterDot] b), "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 50} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] (b \[CenterDot] 
            b)) \[CenterDot] (a \[CenterDot] b)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 36}, 
        "Construct" -> {"SubstitutionLemma", 41}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (b_))) -> a \[CenterDot] b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a == (a \[CenterDot] 
             (b \[CenterDot] b)) \[CenterDot] (a \[CenterDot] b)], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 51} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] b) \[CenterDot] 
          (a \[CenterDot] (b \[CenterDot] b))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 50}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a == (a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
             (b \[CenterDot] b))], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 52} -> 
     <|"Statement" -> HoldForm[(((b \[CenterDot] a) \[CenterDot] 
            c) \[CenterDot] b) \[CenterDot] a == a \[CenterDot] 
          (b \[CenterDot] a)], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 34}, "Construct" -> 
         {"SubstitutionLemma", 51}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
            ((b_) \[CenterDot] (b_))) -> a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[(((b \[CenterDot] a) \[CenterDot] 
              c) \[CenterDot] b) \[CenterDot] a == a \[CenterDot] 
            (b \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 53} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          (((b \[CenterDot] a) \[CenterDot] c) \[CenterDot] b) == 
         a \[CenterDot] (b \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 52}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a \[CenterDot] (((b \[CenterDot] a) \[CenterDot] c) \[CenterDot] 
             b) == a \[CenterDot] (b \[CenterDot] a)], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 54} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] c)) == a \[CenterDot] 
          (b \[CenterDot] a)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 53}, "Construct" -> 
         {"SubstitutionLemma", 3}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a \[CenterDot] (b \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
              c)) == a \[CenterDot] (b \[CenterDot] a)], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 37} -> 
     <|"Statement" -> HoldForm[(c \[CenterDot] b) \[CenterDot] a == 
         a \[CenterDot] ((a \[CenterDot] (b \[CenterDot] a)) \[CenterDot] 
           c)], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 45}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            ((a_) \[CenterDot] ((c_) \[CenterDot] (a_)))) -> 
          (b \[CenterDot] c) \[CenterDot] a, "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] ((a_) \[CenterDot] 
           ((c_) \[CenterDot] (a_))), "MatchingConstruct" -> 
         {"SubstitutionLemma", 3}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 38} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
          (b \[CenterDot] (a \[CenterDot] b)) == 
         (a \[CenterDot] b) \[CenterDot] ((c \[CenterDot] a) \[CenterDot] 
           b)], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 54}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            (((b_) \[CenterDot] (a_)) \[CenterDot] (c_))) -> 
          a \[CenterDot] (b \[CenterDot] a), "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] (((b_) \[CenterDot] 
            (a_)) \[CenterDot] (c_)), "MatchingConstruct" -> 
         {"CriticalPairLemma", 37}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CenterDot] (((a_) \[CenterDot] 
             ((b_) \[CenterDot] (a_))) \[CenterDot] (c_)) -> 
          (c \[CenterDot] b) \[CenterDot] a, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 55} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
          (b \[CenterDot] b) == (a \[CenterDot] b) \[CenterDot] 
          ((c \[CenterDot] a) \[CenterDot] b)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 38}, 
        "Construct" -> {"SubstitutionLemma", 23}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (a_)) -> 
          a \[CenterDot] (b \[CenterDot] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
            (b \[CenterDot] b) == (a \[CenterDot] b) \[CenterDot] 
            ((c \[CenterDot] a) \[CenterDot] b)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 56} -> 
     <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
          ((c \[CenterDot] a) \[CenterDot] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 55}, 
        "Construct" -> {"CriticalPairLemma", 8}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
            (b_)) -> b, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
            ((c \[CenterDot] a) \[CenterDot] b)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 39} -> 
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
    {"SubstitutionLemma", 57} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          (b \[CenterDot] ((c \[CenterDot] a) \[CenterDot] b))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 39}, 
        "Construct" -> {"Axiom", 2}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[a == (a \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
             ((c \[CenterDot] a) \[CenterDot] b))], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 40} -> 
     <|"Statement" -> HoldForm[x3 == 
         ((a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a)) \[CenterDot] 
           x3) \[CenterDot] (c \[CenterDot] x3)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 56}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((c_) \[CenterDot] (a_)) \[CenterDot] (b_)) -> b, "Side" -> 1, 
        "Subpattern" -> (c_) \[CenterDot] (a_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 57}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
           ((b_) \[CenterDot] (((c_) \[CenterDot] (a_)) \[CenterDot] 
             (b_))) -> a, "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"SubstitutionLemma", 58} -> 
     <|"Statement" -> HoldForm[x3 == (c \[CenterDot] x3) \[CenterDot] 
          ((a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a)) \[CenterDot] 
           x3)], "Proof" -> <|"Input" -> {"CriticalPairLemma", 40}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          x3 == (c \[CenterDot] x3) \[CenterDot] ((a \[CenterDot] 
              ((b \[CenterDot] c) \[CenterDot] a)) \[CenterDot] x3)], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 41} -> 
     <|"Statement" -> HoldForm[c \[CenterDot] 
          ((a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a))) == 
         ((a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a)) \[CenterDot] 
           ((a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] 
              a)))) \[CenterDot] ((a \[CenterDot] 
            ((b \[CenterDot] c) \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a)))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 31}, 
        "Orientation" -> -1, "Rule" -> 
         ((a_) \[CenterDot] ((a_) \[CenterDot] (a_))) \[CenterDot] 
           ((b_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
              (a_)))) -> b, "Side" -> 1, "Subpattern" -> 
         (b_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] (a_))), 
        "MatchingConstruct" -> {"SubstitutionLemma", 58}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] (b_)) \[CenterDot] (((c_) \[CenterDot] 
             (((x3_) \[CenterDot] (a_)) \[CenterDot] (c_))) \[CenterDot] 
            (b_)) -> b, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 59} -> 
     <|"Statement" -> HoldForm[c \[CenterDot] 
          ((a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a))) == 
         ((a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a))) \[CenterDot] 
          ((a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a)) \[CenterDot] 
           ((a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a)) \[CenterDot] 
            (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a))))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 41}, 
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
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 60} -> 
     <|"Statement" -> HoldForm[c \[CenterDot] 
          ((a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a))) == 
         a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 59}, 
        "Construct" -> {"CriticalPairLemma", 29}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            (b_)) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[c \[CenterDot] ((a \[CenterDot] ((b \[CenterDot] 
                c) \[CenterDot] a)) \[CenterDot] (a \[CenterDot] 
              ((b \[CenterDot] c) \[CenterDot] a))) == a \[CenterDot] 
            ((b \[CenterDot] c) \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 61} -> 
     <|"Statement" -> HoldForm[c \[CenterDot] (c \[CenterDot] 
           (a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a))) == 
         a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 60}, 
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
             c)))], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 45}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            ((a_) \[CenterDot] ((c_) \[CenterDot] (a_)))) -> 
          (b \[CenterDot] c) \[CenterDot] a, "Side" -> 1, 
        "Subpattern" -> (c_) \[CenterDot] (a_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 3}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "MatchingSide" -> 1, "Position" -> {2, 2, 2}|>|>, 
    {"CriticalPairLemma", 43} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] (c \[CenterDot] 
            ((x3 \[CenterDot] a) \[CenterDot] c))) \[CenterDot] a == 
         a \[CenterDot] (b \[CenterDot] (a \[CenterDot] 
            ((c \[CenterDot] x3) \[CenterDot] a)))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 42}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            ((a_) \[CenterDot] ((a_) \[CenterDot] (c_)))) -> 
          (b \[CenterDot] c) \[CenterDot] a, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (c_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 47}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            (((c_) \[CenterDot] (a_)) \[CenterDot] (b_))) -> 
          (b \[CenterDot] c) \[CenterDot] a, "MatchingSide" -> 1, 
        "Position" -> {2, 2, 2}|>|>, {"SubstitutionLemma", 62} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] (c \[CenterDot] 
            ((x3 \[CenterDot] a) \[CenterDot] c))) \[CenterDot] a == 
         (b \[CenterDot] (c \[CenterDot] x3)) \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 43}, 
        "Construct" -> {"SubstitutionLemma", 45}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] ((a_) \[CenterDot] 
             ((c_) \[CenterDot] (a_)))) -> (b \[CenterDot] c) \[CenterDot] a, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (b \[CenterDot] (c \[CenterDot] ((x3 \[CenterDot] a) \[CenterDot] 
               c))) \[CenterDot] a == (b \[CenterDot] (c \[CenterDot] 
              x3)) \[CenterDot] a], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 63} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
           (c \[CenterDot] ((x3 \[CenterDot] a) \[CenterDot] c))) == 
         (b \[CenterDot] (c \[CenterDot] x3)) \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 62}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a \[CenterDot] (b \[CenterDot] (c \[CenterDot] ((x3 \[CenterDot] 
                a) \[CenterDot] c))) == (b \[CenterDot] (c \[CenterDot] 
              x3)) \[CenterDot] a], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 64} -> 
     <|"Statement" -> HoldForm[(c \[CenterDot] (a \[CenterDot] 
            b)) \[CenterDot] c == a \[CenterDot] 
          ((b \[CenterDot] c) \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 61}, 
        "Construct" -> {"SubstitutionLemma", 63}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] ((c_) \[CenterDot] 
             (((x3_) \[CenterDot] (a_)) \[CenterDot] (c_)))) -> 
          (b \[CenterDot] (c \[CenterDot] x3)) \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (c \[CenterDot] (a \[CenterDot] b)) \[CenterDot] c == 
           a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a)], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 65} -> 
     <|"Statement" -> HoldForm[c \[CenterDot] (c \[CenterDot] 
           (a \[CenterDot] b)) == a \[CenterDot] 
          ((b \[CenterDot] c) \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 64}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          c \[CenterDot] (c \[CenterDot] (a \[CenterDot] b)) == 
           a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a)], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 44} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] c) \[CenterDot] a == 
         a \[CenterDot] (b \[CenterDot] ((a \[CenterDot] c) \[CenterDot] 
            b))], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 47}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            (((c_) \[CenterDot] (a_)) \[CenterDot] (b_))) -> 
          (b \[CenterDot] c) \[CenterDot] a, "Side" -> 1, 
        "Subpattern" -> (c_) \[CenterDot] (a_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 3}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "MatchingSide" -> 1, "Position" -> {2, 2, 1}|>|>, 
    {"CriticalPairLemma", 45} -> 
     <|"Statement" -> HoldForm[(((a \[CenterDot] b) \[CenterDot] 
            c) \[CenterDot] b) \[CenterDot] a == a \[CenterDot] 
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
           (b \[CenterDot] b), "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"CriticalPairLemma", 46} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] b) \[CenterDot] 
          ((c \[CenterDot] b) \[CenterDot] a)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 56}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((c_) \[CenterDot] (a_)) \[CenterDot] (b_)) -> b, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 3}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 47} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] b) \[CenterDot] 
          (a \[CenterDot] (c \[CenterDot] b))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 46}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((c_) \[CenterDot] (b_)) \[CenterDot] (a_)) -> a, "Side" -> 1, 
        "Subpattern" -> ((c_) \[CenterDot] (b_)) \[CenterDot] (a_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 3}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CenterDot] (b_) -> b \[CenterDot] a, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 67} -> 
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
    {"SubstitutionLemma", 68} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          (((a \[CenterDot] b) \[CenterDot] c) \[CenterDot] b) == 
         a \[CenterDot] (a \[CenterDot] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 67}, 
        "Construct" -> {"SubstitutionLemma", 3}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a \[CenterDot] (((a \[CenterDot] b) \[CenterDot] c) \[CenterDot] 
             b) == a \[CenterDot] (a \[CenterDot] b)], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 69} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
           ((a \[CenterDot] b) \[CenterDot] c)) == a \[CenterDot] 
          (a \[CenterDot] b)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 68}, "Construct" -> 
         {"SubstitutionLemma", 3}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a \[CenterDot] (b \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
              c)) == a \[CenterDot] (a \[CenterDot] b)], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 48} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] a) == 
         a \[CenterDot] (b \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            c))], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 54}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            (((b_) \[CenterDot] (a_)) \[CenterDot] (c_))) -> 
          a \[CenterDot] (b \[CenterDot] a), "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] (a_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 3}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "MatchingSide" -> 1, "Position" -> {2, 2, 1}|>|>, 
    {"SubstitutionLemma", 70} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] a) == 
         a \[CenterDot] (a \[CenterDot] b)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 69}, 
        "Construct" -> {"CriticalPairLemma", 48}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            (((a_) \[CenterDot] (b_)) \[CenterDot] (c_))) -> 
          a \[CenterDot] (b \[CenterDot] a), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[CenterDot] (b \[CenterDot] a) == 
           a \[CenterDot] (a \[CenterDot] b)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 19} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
            (c \[CenterDot] c))) == (((a \[CenterDot] a) \[CenterDot] 
            (b \[CenterDot] b)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
            (b \[CenterDot] b))) \[CenterDot] (c \[CenterDot] c)], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Construct" -> 
         {"SubstitutionLemma", 18}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (b_)) -> 
          a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
            ((a \[CenterDot] a) \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
              (c \[CenterDot] c))) == (((a \[CenterDot] a) \[CenterDot] 
              (b \[CenterDot] b)) \[CenterDot] ((a \[CenterDot] 
               a) \[CenterDot] (b \[CenterDot] b))) \[CenterDot] 
            (c \[CenterDot] c)], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 20} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
            (c \[CenterDot] c))) == (c \[CenterDot] c) \[CenterDot] 
          (((a \[CenterDot] a) \[CenterDot] (b \[CenterDot] b)) \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] (b \[CenterDot] b)))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 19}, 
        "Construct" -> {"SubstitutionLemma", 2}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (b_)) -> 
          (b \[CenterDot] b) \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
            ((a \[CenterDot] a) \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
              (c \[CenterDot] c))) == (c \[CenterDot] c) \[CenterDot] 
            (((a \[CenterDot] a) \[CenterDot] (b \[CenterDot] 
               b)) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              (b \[CenterDot] b)))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 21} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
            (c \[CenterDot] c))) == (c \[CenterDot] c) \[CenterDot] 
          ((c \[CenterDot] c) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
            (b \[CenterDot] b)))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 20}, "Construct" -> 
         {"SubstitutionLemma", 18}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (b_)) -> 
          a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
            ((a \[CenterDot] a) \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
              (c \[CenterDot] c))) == (c \[CenterDot] c) \[CenterDot] 
            ((c \[CenterDot] c) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              (b \[CenterDot] b)))], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 66} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
            (c \[CenterDot] c))) == (a \[CenterDot] a) \[CenterDot] 
          (((b \[CenterDot] b) \[CenterDot] (c \[CenterDot] c)) \[CenterDot] 
           (a \[CenterDot] a))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 21}, "Construct" -> 
         {"SubstitutionLemma", 65}, "Position" -> {}, 
        "Rule" -> (c_) \[CenterDot] ((c_) \[CenterDot] ((a_) \[CenterDot] 
             (b_))) -> a \[CenterDot] ((b \[CenterDot] c) \[CenterDot] a), 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (a \[CenterDot] a) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
             ((b \[CenterDot] b) \[CenterDot] (c \[CenterDot] c))) == 
           (a \[CenterDot] a) \[CenterDot] (((b \[CenterDot] b) \[CenterDot] 
              (c \[CenterDot] c)) \[CenterDot] (a \[CenterDot] a))], 
        "Source" -> "cpl"|>|>, {"Conclusion", 1} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
            (c \[CenterDot] c))) == (a \[CenterDot] a) \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
            (c \[CenterDot] c)))], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 66}, "Construct" -> 
         {"SubstitutionLemma", 70}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (a_)) -> 
          a \[CenterDot] (a \[CenterDot] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
            ((a \[CenterDot] a) \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
              (c \[CenterDot] c))) == (a \[CenterDot] a) \[CenterDot] 
            ((a \[CenterDot] a) \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
              (c \[CenterDot] c)))], "Source" -> "cpl"|>|>}|>]
