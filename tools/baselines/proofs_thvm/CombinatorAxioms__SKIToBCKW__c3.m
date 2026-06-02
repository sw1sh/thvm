ProofObject["EquationalLogic", Inactive[Equal][CombinatorI \[Application] a, 
  CombinatorW \[Application] CombinatorK \[Application] a], 
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
     <|"Statement" -> HoldForm[CombinatorI \[Application] a == 
         CombinatorW \[Application] CombinatorK \[Application] a], 
      "Proof" -> <||>|>, {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[a == CombinatorW \[Application] 
           CombinatorK \[Application] a], "Proof" -> 
       <|"Input" -> {"Hypothesis", 1}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {}, "Rule" -> CombinatorI \[Application] (a_) -> a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a == CombinatorW \[Application] CombinatorK \[Application] a], 
        "Source" -> "synth"|>|>, {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[a == CombinatorK \[Application] 
           a \[Application] a], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 1}, "Construct" -> {"Axiom", 6}, 
        "Position" -> {}, "Rule" -> CombinatorW \[Application] 
            (a_) \[Application] (b_) -> a \[Application] b \[Application] b, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a == CombinatorK \[Application] a \[Application] a], 
        "Source" -> "synth"|>|>, {"Conclusion", 1} -> 
     <|"Statement" -> HoldForm[a == a], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 2}, "Construct" -> {"Axiom", 2}, 
        "Position" -> {}, "Rule" -> CombinatorK \[Application] 
            (a_) \[Application] (b_) -> a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a == a], "Source" -> "synth"|>|>}|>]
