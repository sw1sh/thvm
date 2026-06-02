ProofObject["EquationalLogic", Inactive[Equal][
  CombinatorC \[Application] a \[Application] b \[Application] c, 
  CombinatorS \[Application] (CombinatorS \[Application] 
         (CombinatorK \[Application] (CombinatorS \[Application] 
            (CombinatorK \[Application] CombinatorS) \[Application] 
           CombinatorK)) \[Application] CombinatorS) \[Application] 
      (CombinatorK \[Application] CombinatorK) \[Application] 
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
       HoldForm[CombinatorC \[Application] a \[Application] b \[Application] 
          c == CombinatorS \[Application] (CombinatorS \[Application] 
                (CombinatorK \[Application] (CombinatorS \[Application] 
                   (CombinatorK \[Application] CombinatorS) \[Application] 
                  CombinatorK)) \[Application] CombinatorS) \[Application] 
             (CombinatorK \[Application] CombinatorK) \[Application] 
            a \[Application] b \[Application] c], "Proof" -> <||>|>, 
    {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[a \[Application] c \[Application] b == 
         CombinatorS \[Application] (CombinatorS \[Application] 
                (CombinatorK \[Application] (CombinatorS \[Application] 
                   (CombinatorK \[Application] CombinatorS) \[Application] 
                  CombinatorK)) \[Application] CombinatorS) \[Application] 
             (CombinatorK \[Application] CombinatorK) \[Application] 
            a \[Application] b \[Application] c], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Construct" -> {"Axiom", 4}, 
        "Position" -> {}, "Rule" -> 
         CombinatorC \[Application] (a_) \[Application] (b_) \[Application] 
           (c_) -> a \[Application] c \[Application] b, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a \[Application] c \[Application] b == 
           CombinatorS \[Application] (CombinatorS \[Application] 
                  (CombinatorK \[Application] (CombinatorS \[Application] 
                     (CombinatorK \[Application] CombinatorS) \[Application] 
                    CombinatorK)) \[Application] CombinatorS) \[Application] (
                CombinatorK \[Application] CombinatorK) \[Application] 
              a \[Application] b \[Application] c], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[a \[Application] c \[Application] b == 
         CombinatorS \[Application] (CombinatorK \[Application] 
                (CombinatorS \[Application] (CombinatorK \[Application] 
                   CombinatorS) \[Application] CombinatorK)) \[Application] 
              CombinatorS \[Application] a \[Application] 
            (CombinatorK \[Application] CombinatorK \[Application] 
             a) \[Application] b \[Application] c], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 1}, 
        "Construct" -> {"Axiom", 3}, "Position" -> {1, 1}, 
        "Rule" -> CombinatorS \[Application] (a_) \[Application] 
            (b_) \[Application] (c_) -> a \[Application] c \[Application] 
           (b \[Application] c), "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[a \[Application] c \[Application] b == 
           CombinatorS \[Application] (CombinatorK \[Application] 
                  (CombinatorS \[Application] (CombinatorK \[Application] 
                     CombinatorS) \[Application] CombinatorK)) \[Application] 
                CombinatorS \[Application] a \[Application] 
              (CombinatorK \[Application] CombinatorK \[Application] 
               a) \[Application] b \[Application] c], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 3} -> 
     <|"Statement" -> HoldForm[a \[Application] c \[Application] b == 
         CombinatorS \[Application] (CombinatorK \[Application] 
                (CombinatorS \[Application] (CombinatorK \[Application] 
                   CombinatorS) \[Application] CombinatorK)) \[Application] 
              CombinatorS \[Application] a \[Application] 
            CombinatorK \[Application] b \[Application] c], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 2}, 
        "Construct" -> {"Axiom", 2}, "Position" -> {1, 1, 2}, 
        "Rule" -> CombinatorK \[Application] (a_) \[Application] (b_) -> a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[Application] c \[Application] b == 
           CombinatorS \[Application] (CombinatorK \[Application] 
                  (CombinatorS \[Application] (CombinatorK \[Application] 
                     CombinatorS) \[Application] CombinatorK)) \[Application] 
                CombinatorS \[Application] a \[Application] 
              CombinatorK \[Application] b \[Application] c], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 4} -> 
     <|"Statement" -> HoldForm[a \[Application] c \[Application] b == 
         CombinatorK \[Application] (CombinatorS \[Application] 
                 (CombinatorK \[Application] CombinatorS) \[Application] 
                CombinatorK) \[Application] a \[Application] 
             (CombinatorS \[Application] a) \[Application] 
            CombinatorK \[Application] b \[Application] c], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 3}, 
        "Construct" -> {"Axiom", 3}, "Position" -> {1, 1, 1}, 
        "Rule" -> CombinatorS \[Application] (a_) \[Application] 
            (b_) \[Application] (c_) -> a \[Application] c \[Application] 
           (b \[Application] c), "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[a \[Application] c \[Application] b == 
           CombinatorK \[Application] (CombinatorS \[Application] 
                   (CombinatorK \[Application] CombinatorS) \[Application] 
                  CombinatorK) \[Application] a \[Application] (
                CombinatorS \[Application] a) \[Application] 
              CombinatorK \[Application] b \[Application] c], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 5} -> 
     <|"Statement" -> HoldForm[a \[Application] c \[Application] b == 
         CombinatorS \[Application] (CombinatorK \[Application] 
                CombinatorS) \[Application] CombinatorK \[Application] 
             (CombinatorS \[Application] a) \[Application] 
            CombinatorK \[Application] b \[Application] c], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 4}, 
        "Construct" -> {"Axiom", 2}, "Position" -> {1, 1, 1, 1}, 
        "Rule" -> CombinatorK \[Application] (a_) \[Application] (b_) -> a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[Application] c \[Application] b == 
           CombinatorS \[Application] (CombinatorK \[Application] 
                  CombinatorS) \[Application] CombinatorK \[Application] (
                CombinatorS \[Application] a) \[Application] 
              CombinatorK \[Application] b \[Application] c], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 6} -> 
     <|"Statement" -> HoldForm[a \[Application] c \[Application] b == 
         CombinatorK \[Application] CombinatorS \[Application] 
              (CombinatorS \[Application] a) \[Application] 
             (CombinatorK \[Application] (CombinatorS \[Application] 
               a)) \[Application] CombinatorK \[Application] b \[Application] 
          c], "Proof" -> <|"Input" -> {"SubstitutionLemma", 5}, 
        "Construct" -> {"Axiom", 3}, "Position" -> {1, 1, 1}, 
        "Rule" -> CombinatorS \[Application] (a_) \[Application] 
            (b_) \[Application] (c_) -> a \[Application] c \[Application] 
           (b \[Application] c), "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[a \[Application] c \[Application] b == 
           CombinatorK \[Application] CombinatorS \[Application] 
                (CombinatorS \[Application] a) \[Application] (
                CombinatorK \[Application] (CombinatorS \[Application] 
                 a)) \[Application] CombinatorK \[Application] 
             b \[Application] c], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 7} -> 
     <|"Statement" -> HoldForm[a \[Application] c \[Application] b == 
         CombinatorS \[Application] (CombinatorK \[Application] 
              (CombinatorS \[Application] a)) \[Application] 
            CombinatorK \[Application] b \[Application] c], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 6}, 
        "Construct" -> {"Axiom", 2}, "Position" -> {1, 1, 1, 1}, 
        "Rule" -> CombinatorK \[Application] (a_) \[Application] (b_) -> a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[Application] c \[Application] b == 
           CombinatorS \[Application] (CombinatorK \[Application] 
                (CombinatorS \[Application] a)) \[Application] 
              CombinatorK \[Application] b \[Application] c], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 8} -> 
     <|"Statement" -> HoldForm[a \[Application] c \[Application] b == 
         CombinatorK \[Application] (CombinatorS \[Application] 
              a) \[Application] b \[Application] (CombinatorK \[Application] 
            b) \[Application] c], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 7}, "Construct" -> {"Axiom", 3}, 
        "Position" -> {1}, "Rule" -> 
         CombinatorS \[Application] (a_) \[Application] (b_) \[Application] 
           (c_) -> a \[Application] c \[Application] (b \[Application] c), 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[Application] c \[Application] b == 
           CombinatorK \[Application] (CombinatorS \[Application] 
                a) \[Application] b \[Application] 
             (CombinatorK \[Application] b) \[Application] c], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 9} -> 
     <|"Statement" -> HoldForm[a \[Application] c \[Application] b == 
         CombinatorS \[Application] a \[Application] 
           (CombinatorK \[Application] b) \[Application] c], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 8}, 
        "Construct" -> {"Axiom", 2}, "Position" -> {1, 1}, 
        "Rule" -> CombinatorK \[Application] (a_) \[Application] (b_) -> a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[Application] c \[Application] b == 
           CombinatorS \[Application] a \[Application] 
             (CombinatorK \[Application] b) \[Application] c], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 10} -> 
     <|"Statement" -> HoldForm[a \[Application] c \[Application] b == 
         a \[Application] c \[Application] (CombinatorK \[Application] 
            b \[Application] c)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 9}, "Construct" -> {"Axiom", 3}, 
        "Position" -> {}, "Rule" -> 
         CombinatorS \[Application] (a_) \[Application] (b_) \[Application] 
           (c_) -> a \[Application] c \[Application] (b \[Application] c), 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[Application] c \[Application] b == 
           a \[Application] c \[Application] (CombinatorK \[Application] 
              b \[Application] c)], "Source" -> "cpl"|>|>, 
    {"Conclusion", 1} -> <|"Statement" -> 
       HoldForm[a \[Application] c \[Application] b == 
         a \[Application] c \[Application] b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 10}, 
        "Construct" -> {"Axiom", 2}, "Position" -> {2}, 
        "Rule" -> CombinatorK \[Application] (a_) \[Application] (b_) -> a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[Application] c \[Application] b == 
           a \[Application] c \[Application] b], "Source" -> "cpl"|>|>}|>]
