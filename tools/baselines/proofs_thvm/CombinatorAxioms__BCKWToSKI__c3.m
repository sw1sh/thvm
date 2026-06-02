ProofObject["EquationalLogic", Inactive[Equal][
  CombinatorW \[Application] a \[Application] b, 
  CombinatorS \[Application] CombinatorS \[Application] 
     (CombinatorS \[Application] CombinatorK) \[Application] a \[Application] 
   b], {Inactive[Equal][CombinatorI \[Application] (a_), a_], 
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
       HoldForm[CombinatorI \[Application] (a_) == (a_)], "Proof" -> <||>|>, 
    {"Axiom", 2} -> <|"Statement" -> HoldForm[
        CombinatorK \[Application] (a_) \[Application] (b_) == (a_)], 
      "Proof" -> <||>|>, {"Axiom", 3} -> 
     <|"Statement" -> HoldForm[CombinatorS \[Application] (a_) \[Application] 
           (b_) \[Application] (c_) == (a_) \[Application] 
           (c_) \[Application] ((b_) \[Application] (c_))], 
      "Proof" -> <||>|>, {"Axiom", 4} -> 
     <|"Statement" -> HoldForm[CombinatorC \[Application] (a_) \[Application] 
           (b_) \[Application] (c_) == (a_) \[Application] 
           (c_) \[Application] (b_)], "Proof" -> <||>|>, 
    {"Axiom", 5} -> <|"Statement" -> HoldForm[
        CombinatorB \[Application] (a_) \[Application] (b_) \[Application] 
          (c_) == (a_) \[Application] ((b_) \[Application] (c_))], 
      "Proof" -> <||>|>, {"Axiom", 6} -> 
     <|"Statement" -> HoldForm[CombinatorW \[Application] (a_) \[Application] 
          (b_) == (a_) \[Application] (b_) \[Application] (b_)], 
      "Proof" -> <||>|>, {"Hypothesis", 1} -> 
     <|"Statement" -> HoldForm[CombinatorW \[Application] a \[Application] 
          b == CombinatorS \[Application] CombinatorS \[Application] 
            (CombinatorS \[Application] CombinatorK) \[Application] 
           a \[Application] b], "Proof" -> <||>|>, 
    {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[CombinatorW \[Application] a \[Application] 
          b == CombinatorS \[Application] a \[Application] 
           (CombinatorS \[Application] CombinatorK \[Application] 
            a) \[Application] b], "Proof" -> <|"Input" -> {"Hypothesis", 1}, 
        "Construct" -> {"Axiom", 3}, "Position" -> {1}, 
        "Rule" -> CombinatorS \[Application] (a_) \[Application] 
            (b_) \[Application] (c_) -> a \[Application] c \[Application] 
           (b \[Application] c), "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[CombinatorW \[Application] a \[Application] b == 
           CombinatorS \[Application] a \[Application] 
             (CombinatorS \[Application] CombinatorK \[Application] 
              a) \[Application] b], "Source" -> "synth"|>|>, 
    {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[CombinatorW \[Application] a \[Application] 
          b == a \[Application] b \[Application] 
          (CombinatorS \[Application] CombinatorK \[Application] 
            a \[Application] b)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 1}, "Construct" -> {"Axiom", 3}, 
        "Position" -> {}, "Rule" -> 
         CombinatorS \[Application] (a_) \[Application] (b_) \[Application] 
           (c_) -> a \[Application] c \[Application] (b \[Application] c), 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          CombinatorW \[Application] a \[Application] b == 
           a \[Application] b \[Application] (CombinatorS \[Application] 
               CombinatorK \[Application] a \[Application] b)], 
        "Source" -> "synth"|>|>, {"SubstitutionLemma", 3} -> 
     <|"Statement" -> HoldForm[CombinatorW \[Application] a \[Application] 
          b == a \[Application] b \[Application] (CombinatorK \[Application] 
            b \[Application] (a \[Application] b))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 2}, 
        "Construct" -> {"Axiom", 3}, "Position" -> {2}, 
        "Rule" -> CombinatorS \[Application] (a_) \[Application] 
            (b_) \[Application] (c_) -> a \[Application] c \[Application] 
           (b \[Application] c), "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[CombinatorW \[Application] a \[Application] b == 
           a \[Application] b \[Application] (CombinatorK \[Application] 
              b \[Application] (a \[Application] b))], 
        "Source" -> "synth"|>|>, {"SubstitutionLemma", 4} -> 
     <|"Statement" -> HoldForm[CombinatorW \[Application] a \[Application] 
          b == a \[Application] b \[Application] b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 3}, 
        "Construct" -> {"Axiom", 2}, "Position" -> {2}, 
        "Rule" -> CombinatorK \[Application] (a_) \[Application] (b_) -> a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          CombinatorW \[Application] a \[Application] b == 
           a \[Application] b \[Application] b], "Source" -> "synth"|>|>, 
    {"Conclusion", 1} -> <|"Statement" -> 
       HoldForm[CombinatorW \[Application] a \[Application] b == 
         CombinatorW \[Application] a \[Application] b], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 4}, 
        "Construct" -> {"Axiom", 6}, "Position" -> {}, 
        "Rule" -> (a_) \[Application] (b_) \[Application] (b_) -> 
          CombinatorW \[Application] a \[Application] b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[CombinatorW \[Application] 
             a \[Application] b == CombinatorW \[Application] 
             a \[Application] b], "Source" -> "synth"|>|>}|>]
