ProofObject["EquationalLogic", Inactive[Equal][
  CombinatorS \[Application] a \[Application] b \[Application] c, 
  CombinatorB \[Application] (CombinatorB \[Application] 
        CombinatorW) \[Application] (CombinatorB \[Application] 
        CombinatorB \[Application] CombinatorC) \[Application] 
     a \[Application] b \[Application] c], 
 {Inactive[Equal][CombinatorI \[Application] (a_), a_], 
  Inactive[Equal][CombinatorK \[Application] (a_) \[Application] (b_), a_], 
  Inactive[Equal][CombinatorS \[Application] (a_) \[Application] 
     (b_) \[Application] (c_), (a_) \[Application] (c_) \[Application] 
    ((b_) \[Application] (c_))], Inactive[Equal][
   CombinatorC \[Application] (a_) \[Application] (b_) \[Application] (c_), 
   (a_) \[Application] (c_) \[Application] (b_)], 
  Inactive[Equal][CombinatorB \[Application] (a_) \[Application] 
     (b_) \[Application] (c_), (a_) \[Application] 
    ((b_) \[Application] (c_))], Inactive[Equal][
   CombinatorW \[Application] (a_) \[Application] (b_), 
   (a_) \[Application] (b_) \[Application] (b_)]}, 
 <|"Variables" -> {a, b, c}, "Constants" -> {}, 
  "Proof" -> {{"Axiom", 1} -> <|"Statement" -> 
       HoldForm[CombinatorI \[Application] a == a], "Proof" -> <||>|>, 
    {"Axiom", 2} -> <|"Statement" -> HoldForm[
        CombinatorK \[Application] a \[Application] b == a], 
      "Proof" -> <||>|>, {"Axiom", 3} -> 
     <|"Statement" -> HoldForm[CombinatorS \[Application] a \[Application] 
           b \[Application] c == a \[Application] c \[Application] 
          (b \[Application] c)], "Proof" -> <||>|>, 
    {"Axiom", 4} -> <|"Statement" -> HoldForm[
        CombinatorC \[Application] a \[Application] b \[Application] c == 
         a \[Application] c \[Application] b], "Proof" -> <||>|>, 
    {"Axiom", 5} -> <|"Statement" -> HoldForm[
        CombinatorB \[Application] a \[Application] b \[Application] c == 
         a \[Application] (b \[Application] c)], "Proof" -> <||>|>, 
    {"Axiom", 6} -> <|"Statement" -> HoldForm[
        CombinatorW \[Application] a \[Application] b == 
         a \[Application] b \[Application] b], "Proof" -> <||>|>, 
    {"Hypothesis", 1} -> <|"Statement" -> 
       HoldForm[CombinatorS \[Application] a \[Application] b \[Application] 
          c == CombinatorB \[Application] (CombinatorB \[Application] 
               CombinatorW) \[Application] (CombinatorB \[Application] 
               CombinatorB \[Application] CombinatorC) \[Application] 
            a \[Application] b \[Application] c], "Proof" -> <||>|>, 
    {"CriticalPairLemma", 1} -> 
     <|"Statement" -> HoldForm[CombinatorW \[Application] 
           (CombinatorB \[Application] a \[Application] b) \[Application] 
          c == a \[Application] (b \[Application] c) \[Application] c], 
      "Proof" -> <|"Construct" -> {"Axiom", 6}, "Orientation" -> -1, 
        "Rule" -> (a_) \[Application] (b_) \[Application] (b_) -> 
          CombinatorW \[Application] a \[Application] b, "Side" -> 1, 
        "Subpattern" -> (a_) \[Application] (b_), "MatchingConstruct" -> 
         {"Axiom", 5}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         CombinatorB \[Application] (a_) \[Application] (b_) \[Application] 
           (c_) -> a \[Application] (b \[Application] c), 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[a \[Application] c \[Application] 
          (b \[Application] c) == 
         CombinatorB \[Application] (CombinatorB \[Application] 
               CombinatorW) \[Application] (CombinatorB \[Application] 
               CombinatorB \[Application] CombinatorC) \[Application] 
            a \[Application] b \[Application] c], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Construct" -> {"Axiom", 3}, 
        "Position" -> {}, "Rule" -> 
         CombinatorS \[Application] (a_) \[Application] (b_) \[Application] 
           (c_) -> a \[Application] c \[Application] (b \[Application] c), 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a \[Application] c \[Application] (b \[Application] c) == 
           CombinatorB \[Application] (CombinatorB \[Application] 
                 CombinatorW) \[Application] (CombinatorB \[Application] 
                 CombinatorB \[Application] CombinatorC) \[Application] 
              a \[Application] b \[Application] c], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[a \[Application] c \[Application] 
          (b \[Application] c) == CombinatorB \[Application] 
             CombinatorW \[Application] (CombinatorB \[Application] 
               CombinatorB \[Application] CombinatorC \[Application] 
             a) \[Application] b \[Application] c], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 1}, 
        "Construct" -> {"Axiom", 5}, "Position" -> {1, 1}, 
        "Rule" -> CombinatorB \[Application] (a_) \[Application] 
            (b_) \[Application] (c_) -> a \[Application] (b \[Application] 
            c), "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[a \[Application] c \[Application] (b \[Application] c) == 
           CombinatorB \[Application] CombinatorW \[Application] 
              (CombinatorB \[Application] CombinatorB \[Application] 
                CombinatorC \[Application] a) \[Application] b \[Application] 
            c], "Source" -> "cpl"|>|>, {"SubstitutionLemma", 3} -> 
     <|"Statement" -> HoldForm[a \[Application] c \[Application] 
          (b \[Application] c) == CombinatorW \[Application] 
           (CombinatorB \[Application] CombinatorB \[Application] 
              CombinatorC \[Application] a \[Application] b) \[Application] 
          c], "Proof" -> <|"Input" -> {"SubstitutionLemma", 2}, 
        "Construct" -> {"Axiom", 5}, "Position" -> {1}, 
        "Rule" -> CombinatorB \[Application] (a_) \[Application] 
            (b_) \[Application] (c_) -> a \[Application] (b \[Application] 
            c), "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[a \[Application] c \[Application] (b \[Application] c) == 
           CombinatorW \[Application] (CombinatorB \[Application] 
                 CombinatorB \[Application] CombinatorC \[Application] 
               a \[Application] b) \[Application] c], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 4} -> 
     <|"Statement" -> HoldForm[a \[Application] c \[Application] 
          (b \[Application] c) == CombinatorW \[Application] 
           (CombinatorB \[Application] (CombinatorC \[Application] 
              a) \[Application] b) \[Application] c], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 3}, 
        "Construct" -> {"Axiom", 5}, "Position" -> {1, 2, 1}, 
        "Rule" -> CombinatorB \[Application] (a_) \[Application] 
            (b_) \[Application] (c_) -> a \[Application] (b \[Application] 
            c), "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[a \[Application] c \[Application] (b \[Application] c) == 
           CombinatorW \[Application] (CombinatorB \[Application] (
                CombinatorC \[Application] a) \[Application] 
              b) \[Application] c], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 5} -> 
     <|"Statement" -> HoldForm[a \[Application] c \[Application] 
          (b \[Application] c) == CombinatorC \[Application] a \[Application] 
           (b \[Application] c) \[Application] c], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 4}, 
        "Construct" -> {"CriticalPairLemma", 1}, "Position" -> {}, 
        "Rule" -> CombinatorW \[Application] (CombinatorB \[Application] 
              (a_) \[Application] (b_)) \[Application] (c_) -> 
          a \[Application] (b \[Application] c) \[Application] c, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[Application] c \[Application] (b \[Application] c) == 
           CombinatorC \[Application] a \[Application] (b \[Application] 
              c) \[Application] c], "Source" -> "cpl"|>|>, 
    {"Conclusion", 1} -> <|"Statement" -> 
       HoldForm[a \[Application] c \[Application] (b \[Application] c) == 
         a \[Application] c \[Application] (b \[Application] c)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 5}, 
        "Construct" -> {"Axiom", 4}, "Position" -> {}, 
        "Rule" -> CombinatorC \[Application] (a_) \[Application] 
            (b_) \[Application] (c_) -> a \[Application] c \[Application] b, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[Application] c \[Application] (b \[Application] c) == 
           a \[Application] c \[Application] (b \[Application] c)], 
        "Source" -> "cpl"|>|>}|>]
