ProofObject["EquationalLogic", Inactive[Equal][
  CombinatorS \[Application] a \[Application] b \[Application] c, 
  CombinatorB \[Application] (CombinatorB \[Application] 
         (CombinatorB \[Application] CombinatorW) \[Application] 
        CombinatorC) \[Application] (CombinatorB \[Application] 
       CombinatorB) \[Application] a \[Application] b \[Application] c], 
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
                (CombinatorB \[Application] CombinatorW) \[Application] 
               CombinatorC) \[Application] (CombinatorB \[Application] 
              CombinatorB) \[Application] a \[Application] b \[Application] 
          c], "Proof" -> <||>|>, {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[CombinatorS \[Application] a \[Application] 
           b \[Application] c == 
         CombinatorB \[Application] (CombinatorI \[Application] 
                 CombinatorB \[Application] (CombinatorB \[Application] 
                 CombinatorW) \[Application] CombinatorC) \[Application] 
             (CombinatorB \[Application] CombinatorB) \[Application] 
            a \[Application] b \[Application] c], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {1, 1, 1, 1, 2, 1, 1}, "Rule" -> 
         a_ -> CombinatorI \[Application] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[
          CombinatorS \[Application] a \[Application] b \[Application] c == 
           CombinatorB \[Application] (CombinatorI \[Application] 
                   CombinatorB \[Application] (CombinatorB \[Application] 
                   CombinatorW) \[Application] CombinatorC) \[Application] (
                CombinatorB \[Application] CombinatorB) \[Application] 
              a \[Application] b \[Application] c], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[CombinatorS \[Application] a \[Application] 
           b \[Application] c == 
         CombinatorI \[Application] CombinatorB \[Application] 
              (CombinatorB \[Application] CombinatorW) \[Application] 
             CombinatorC \[Application] (CombinatorB \[Application] 
              CombinatorB \[Application] a) \[Application] b \[Application] 
          c], "Proof" -> <|"Input" -> {"SubstitutionLemma", 1}, 
        "Construct" -> {"Axiom", 5}, "Position" -> {1, 1}, 
        "Rule" -> CombinatorB \[Application] (a_) \[Application] 
            (b_) \[Application] (c_) -> a \[Application] (b \[Application] 
            c), "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[CombinatorS \[Application] a \[Application] 
             b \[Application] c == CombinatorI \[Application] 
                 CombinatorB \[Application] (CombinatorB \[Application] 
                 CombinatorW) \[Application] CombinatorC \[Application] 
              (CombinatorB \[Application] CombinatorB \[Application] 
               a) \[Application] b \[Application] c], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 3} -> 
     <|"Statement" -> HoldForm[CombinatorS \[Application] a \[Application] 
           b \[Application] c == 
         CombinatorI \[Application] CombinatorB \[Application] 
              (CombinatorB \[Application] CombinatorW) \[Application] 
             CombinatorC \[Application] (CombinatorI \[Application] 
             (CombinatorB \[Application] CombinatorB \[Application] 
              a)) \[Application] b \[Application] c], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 2}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {1, 1, 2}, 
        "Rule" -> a_ -> CombinatorI \[Application] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[
          CombinatorS \[Application] a \[Application] b \[Application] c == 
           CombinatorI \[Application] CombinatorB \[Application] 
                (CombinatorB \[Application] CombinatorW) \[Application] 
               CombinatorC \[Application] (CombinatorI \[Application] (
                CombinatorB \[Application] CombinatorB \[Application] 
                a)) \[Application] b \[Application] c], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 4} -> 
     <|"Statement" -> HoldForm[CombinatorS \[Application] a \[Application] 
           b \[Application] c == 
         CombinatorB \[Application] (CombinatorB \[Application] 
               CombinatorW) \[Application] CombinatorC \[Application] 
            (CombinatorI \[Application] (CombinatorB \[Application] 
               CombinatorB \[Application] a)) \[Application] b \[Application] 
          c], "Proof" -> <|"Input" -> {"SubstitutionLemma", 3}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {1, 1, 1, 1, 1}, 
        "Rule" -> CombinatorI \[Application] (a_) -> a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[
          CombinatorS \[Application] a \[Application] b \[Application] c == 
           CombinatorB \[Application] (CombinatorB \[Application] 
                 CombinatorW) \[Application] CombinatorC \[Application] 
              (CombinatorI \[Application] (CombinatorB \[Application] 
                 CombinatorB \[Application] a)) \[Application] 
             b \[Application] c], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 5} -> 
     <|"Statement" -> HoldForm[CombinatorS \[Application] a \[Application] 
           b \[Application] c == CombinatorB \[Application] 
             CombinatorW \[Application] (CombinatorC \[Application] 
             (CombinatorI \[Application] (CombinatorB \[Application] 
                CombinatorB \[Application] a))) \[Application] 
           b \[Application] c], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 4}, "Construct" -> {"Axiom", 5}, 
        "Position" -> {1, 1}, "Rule" -> 
         CombinatorB \[Application] (a_) \[Application] (b_) \[Application] 
           (c_) -> a \[Application] (b \[Application] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[
          CombinatorS \[Application] a \[Application] b \[Application] c == 
           CombinatorB \[Application] CombinatorW \[Application] 
              (CombinatorC \[Application] (CombinatorI \[Application] 
                (CombinatorB \[Application] CombinatorB \[Application] 
                 a))) \[Application] b \[Application] c], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 6} -> 
     <|"Statement" -> HoldForm[CombinatorS \[Application] a \[Application] 
           b \[Application] c == CombinatorW \[Application] 
           (CombinatorC \[Application] (CombinatorI \[Application] 
              (CombinatorB \[Application] CombinatorB \[Application] 
               a)) \[Application] b) \[Application] c], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 5}, 
        "Construct" -> {"Axiom", 5}, "Position" -> {1}, 
        "Rule" -> CombinatorB \[Application] (a_) \[Application] 
            (b_) \[Application] (c_) -> a \[Application] (b \[Application] 
            c), "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[CombinatorS \[Application] a \[Application] 
             b \[Application] c == CombinatorW \[Application] 
             (CombinatorC \[Application] (CombinatorI \[Application] 
                (CombinatorB \[Application] CombinatorB \[Application] 
                 a)) \[Application] b) \[Application] c], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 7} -> 
     <|"Statement" -> HoldForm[CombinatorS \[Application] a \[Application] 
           b \[Application] c == CombinatorC \[Application] 
             (CombinatorI \[Application] (CombinatorB \[Application] 
                CombinatorB \[Application] a)) \[Application] 
            b \[Application] c \[Application] c], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 6}, 
        "Construct" -> {"Axiom", 6}, "Position" -> {}, 
        "Rule" -> CombinatorW \[Application] (a_) \[Application] (b_) -> 
          a \[Application] b \[Application] b, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[
          CombinatorS \[Application] a \[Application] b \[Application] c == 
           CombinatorC \[Application] (CombinatorI \[Application] 
                (CombinatorB \[Application] CombinatorB \[Application] 
                 a)) \[Application] b \[Application] c \[Application] c], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 8} -> 
     <|"Statement" -> HoldForm[CombinatorS \[Application] a \[Application] 
           b \[Application] c == CombinatorI \[Application] 
             (CombinatorB \[Application] CombinatorB \[Application] 
              a) \[Application] c \[Application] b \[Application] c], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 7}, 
        "Construct" -> {"Axiom", 4}, "Position" -> {1}, 
        "Rule" -> CombinatorC \[Application] (a_) \[Application] 
            (b_) \[Application] (c_) -> a \[Application] c \[Application] b, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          CombinatorS \[Application] a \[Application] b \[Application] c == 
           CombinatorI \[Application] (CombinatorB \[Application] 
                 CombinatorB \[Application] a) \[Application] 
              c \[Application] b \[Application] c], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 9} -> 
     <|"Statement" -> HoldForm[CombinatorS \[Application] a \[Application] 
           b \[Application] c == 
         CombinatorB \[Application] CombinatorB \[Application] 
             a \[Application] c \[Application] b \[Application] c], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 8}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {1, 1, 1}, 
        "Rule" -> CombinatorI \[Application] (a_) -> a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[
          CombinatorS \[Application] a \[Application] b \[Application] c == 
           CombinatorB \[Application] CombinatorB \[Application] 
               a \[Application] c \[Application] b \[Application] c], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 10} -> 
     <|"Statement" -> HoldForm[CombinatorS \[Application] a \[Application] 
           b \[Application] c == CombinatorB \[Application] 
            (a \[Application] c) \[Application] b \[Application] c], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 9}, 
        "Construct" -> {"Axiom", 5}, "Position" -> {1, 1}, 
        "Rule" -> CombinatorB \[Application] (a_) \[Application] 
            (b_) \[Application] (c_) -> a \[Application] (b \[Application] 
            c), "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[CombinatorS \[Application] a \[Application] 
             b \[Application] c == CombinatorB \[Application] 
              (a \[Application] c) \[Application] b \[Application] c], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 11} -> 
     <|"Statement" -> HoldForm[CombinatorS \[Application] a \[Application] 
           b \[Application] c == a \[Application] c \[Application] 
          (b \[Application] c)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 10}, "Construct" -> {"Axiom", 5}, 
        "Position" -> {}, "Rule" -> 
         CombinatorB \[Application] (a_) \[Application] (b_) \[Application] 
           (c_) -> a \[Application] (b \[Application] c), "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[
          CombinatorS \[Application] a \[Application] b \[Application] c == 
           a \[Application] c \[Application] (b \[Application] c)], 
        "Source" -> "cpl"|>|>, {"Conclusion", 1} -> 
     <|"Statement" -> HoldForm[a \[Application] c \[Application] 
          (b \[Application] c) == a \[Application] c \[Application] 
          (b \[Application] c)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 11}, "Construct" -> {"Axiom", 3}, 
        "Position" -> {}, "Rule" -> 
         CombinatorS \[Application] (a_) \[Application] (b_) \[Application] 
           (c_) -> a \[Application] c \[Application] (b \[Application] c), 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a \[Application] c \[Application] (b \[Application] c) == 
           a \[Application] c \[Application] (b \[Application] c)], 
        "Source" -> "cpl"|>|>}|>]
