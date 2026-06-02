ProofObject["EquationalLogic", Inactive[Equal][
  a \[CenterDot] (a \[CenterDot] b), a \[CenterDot] (b \[CenterDot] b)], 
 {Inactive[Equal][(a_) \[CenterDot] (b_), (b_) \[CenterDot] (a_)], 
  Inactive[Equal][((a_) \[CenterDot] (b_)) \[CenterDot] 
    ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))), a_]}, 
 <|"Variables" -> {a, b, c, x3}, "Constants" -> {}, 
  "Proof" -> {{"Axiom", 1} -> <|"Statement" -> 
       HoldForm[a \[CenterDot] b == b \[CenterDot] a], "Proof" -> <||>|>, 
    {"Axiom", 2} -> <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
          (a \[CenterDot] (b \[CenterDot] c)) == a], "Proof" -> <||>|>, 
    {"Hypothesis", 1} -> <|"Statement" -> 
       HoldForm[a \[CenterDot] (a \[CenterDot] b) == a \[CenterDot] 
          (b \[CenterDot] b)], "Proof" -> <||>|>, {"CriticalPairLemma", 1} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] b) \[CenterDot] 
          ((b \[CenterDot] c) \[CenterDot] a)], 
      "Proof" -> <|"Construct" -> {"Axiom", 2}, "Orientation" -> 1, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
            ((b_) \[CenterDot] (c_))) -> a, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] ((b_) \[CenterDot] (c_)), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 2} -> 
     <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
          (b \[CenterDot] (a \[CenterDot] c))], 
      "Proof" -> <|"Construct" -> {"Axiom", 2}, "Orientation" -> 1, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
            ((b_) \[CenterDot] (c_))) -> a, "Side" -> 1, 
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
    {"CriticalPairLemma", 5} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] c == 
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
    {"SubstitutionLemma", 4} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] b == a \[CenterDot] 
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
    {"CriticalPairLemma", 8} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] b) \[CenterDot] 
          (a \[CenterDot] a)], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 7}, "Orientation" -> -1, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            (b_)) -> a, "Side" -> 1, "Subpattern" -> {}, 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"CriticalPairLemma", 9} -> 
     <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
          (b \[CenterDot] b)], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 8}, "Orientation" -> -1, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
            (a_)) -> a, "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] (b_), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 10} -> 
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
    {"CriticalPairLemma", 11} -> 
     <|"Statement" -> HoldForm[((a \[CenterDot] b) \[CenterDot] 
           b) \[CenterDot] a == a \[CenterDot] a], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 3}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            (((b_) \[CenterDot] (c_)) \[CenterDot] (a_))) -> 
          (b \[CenterDot] c) \[CenterDot] a, "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] (((b_) \[CenterDot] 
            (c_)) \[CenterDot] (a_)), "MatchingConstruct" -> 
         {"SubstitutionLemma", 6}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((c_) \[CenterDot] (b_)) \[CenterDot] (a_)) -> a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 7} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] b) \[CenterDot] b) == a \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 11}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] b) == 
           a \[CenterDot] a], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 8} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
           (a \[CenterDot] b)) == a \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 7}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a \[CenterDot] (b \[CenterDot] (a \[CenterDot] b)) == 
           a \[CenterDot] a], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 12} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] a) == 
         a \[CenterDot] (b \[CenterDot] b)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 2}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            ((a_) \[CenterDot] ((b_) \[CenterDot] (c_)))) -> 
          a \[CenterDot] (b \[CenterDot] c), "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] ((a_) \[CenterDot] 
           ((b_) \[CenterDot] (c_))), "MatchingConstruct" -> 
         {"SubstitutionLemma", 8}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            ((a_) \[CenterDot] (b_))) -> a \[CenterDot] a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 13} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] c) == 
         a \[CenterDot] (b \[CenterDot] (a \[CenterDot] (c \[CenterDot] 
             b)))], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 2}, 
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
         a \[CenterDot] (c \[CenterDot] b)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 13}, 
        "Construct" -> {"SubstitutionLemma", 11}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] ((a_) \[CenterDot] 
             ((c_) \[CenterDot] (b_)))) -> a \[CenterDot] (c \[CenterDot] b), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CenterDot] (b \[CenterDot] c) == a \[CenterDot] 
            (c \[CenterDot] b)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 9} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] b) == 
         a \[CenterDot] (b \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Construct" -> 
         {"CriticalPairLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (b_)) -> 
          a \[CenterDot] (b \[CenterDot] a), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CenterDot] (a \[CenterDot] b) == 
           a \[CenterDot] (b \[CenterDot] a)], "Source" -> "cpl"|>|>, 
    {"Conclusion", 1} -> <|"Statement" -> 
       HoldForm[a \[CenterDot] (a \[CenterDot] b) == a \[CenterDot] 
          (a \[CenterDot] b)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 9}, "Construct" -> 
         {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (c_)) -> 
          a \[CenterDot] (c \[CenterDot] b), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CenterDot] (a \[CenterDot] b) == 
           a \[CenterDot] (a \[CenterDot] b)], "Source" -> "cpl"|>|>}|>]
