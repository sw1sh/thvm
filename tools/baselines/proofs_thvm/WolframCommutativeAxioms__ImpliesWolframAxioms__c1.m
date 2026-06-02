ProofObject["EquationalLogic", Inactive[Equal][
  ((a \[CenterDot] b) \[CenterDot] c) \[CenterDot] 
   (a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] a)), c], 
 {Inactive[Equal][(a_) \[CenterDot] (b_), (b_) \[CenterDot] (a_)], 
  Inactive[Equal][((a_) \[CenterDot] (b_)) \[CenterDot] 
    ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))), a_]}, 
 <|"Variables" -> {a, b, c}, "Constants" -> {}, 
  "Proof" -> {{"Axiom", 1} -> <|"Statement" -> 
       HoldForm[a \[CenterDot] b == b \[CenterDot] a], "Proof" -> <||>|>, 
    {"Axiom", 2} -> <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] 
          (a \[CenterDot] (b \[CenterDot] c)) == a], "Proof" -> <||>|>, 
    {"Hypothesis", 1} -> <|"Statement" -> 
       HoldForm[((a \[CenterDot] b) \[CenterDot] c) \[CenterDot] 
          (a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] a)) == c], 
      "Proof" -> <||>|>, {"CriticalPairLemma", 1} -> 
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
    {"SubstitutionLemma", 3} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] b == a \[CenterDot] 
          ((a \[CenterDot] b) \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 1}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[a \[CenterDot] b == 
           a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] a)], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 4} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] b == a \[CenterDot] 
          (a \[CenterDot] (a \[CenterDot] b))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 3}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[a \[CenterDot] b == 
           a \[CenterDot] (a \[CenterDot] (a \[CenterDot] b))], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 2} -> 
     <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
          (b \[CenterDot] (a \[CenterDot] c))], 
      "Proof" -> <|"Construct" -> {"Axiom", 2}, "Orientation" -> 1, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
            ((b_) \[CenterDot] (c_))) -> a, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"Axiom", 1}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CenterDot] (b_) -> b \[CenterDot] a, "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[(c \[CenterDot] (a \[CenterDot] 
            b)) \[CenterDot] (a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] 
            a)) == c], "Proof" -> <|"Input" -> {"Hypothesis", 1}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {1}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (c \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] c) \[CenterDot] a)) == c], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[(c \[CenterDot] (a \[CenterDot] 
            b)) \[CenterDot] (a \[CenterDot] (a \[CenterDot] 
            (a \[CenterDot] c))) == c], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 1}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {2, 2}, "Rule" -> (a_) \[CenterDot] (b_) -> 
          b \[CenterDot] a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[(c \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
            (a \[CenterDot] (a \[CenterDot] (a \[CenterDot] c))) == c], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 5} -> 
     <|"Statement" -> HoldForm[(c \[CenterDot] (a \[CenterDot] 
            b)) \[CenterDot] (a \[CenterDot] c) == c], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 2}, 
        "Construct" -> {"SubstitutionLemma", 4}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] ((a_) \[CenterDot] ((a_) \[CenterDot] 
             (b_))) -> a \[CenterDot] b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          (c \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
            (a \[CenterDot] c) == c], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 6} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] c) \[CenterDot] 
          (c \[CenterDot] (a \[CenterDot] b)) == c], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 5}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          (a \[CenterDot] c) \[CenterDot] (c \[CenterDot] (a \[CenterDot] 
              b)) == c], "Source" -> "cpl"|>|>, {"Conclusion", 1} -> 
     <|"Statement" -> HoldForm[c == c], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 6}, "Construct" -> 
         {"CriticalPairLemma", 2}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
            ((a_) \[CenterDot] (c_))) -> b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[c == c], "Source" -> "cpl"|>|>}|>]
