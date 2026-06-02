ProofObject["EquationalLogic", Inactive[Equal][
  (a \[CenterDot] a) \[CenterDot] (a \[CenterDot] a), a], 
 {Inactive[Equal][((a_) \[CenterDot] (a_)) \[CenterDot] 
    ((a_) \[CenterDot] (a_)), a_], Inactive[Equal][
   (a_) \[CenterDot] ((b_) \[CenterDot] ((b_) \[CenterDot] (b_))), 
   (a_) \[CenterDot] (a_)], Inactive[Equal][
   ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) \[CenterDot] 
    ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))), 
   (((b_) \[CenterDot] (b_)) \[CenterDot] (a_)) \[CenterDot] 
    (((c_) \[CenterDot] (c_)) \[CenterDot] (a_))]}, 
 <|"Variables" -> {a, b, c}, "Constants" -> {}, 
  "Proof" -> {{"Axiom", 1} -> <|"Statement" -> 
       HoldForm[((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
           (a_)) == (a_)], "Proof" -> <||>|>, {"Axiom", 2} -> 
     <|"Statement" -> HoldForm[(a_) \[CenterDot] ((b_) \[CenterDot] 
           ((b_) \[CenterDot] (b_))) == (a_) \[CenterDot] (a_)], 
      "Proof" -> <||>|>, {"Axiom", 3} -> 
     <|"Statement" -> HoldForm[((a_) \[CenterDot] ((b_) \[CenterDot] 
            (c_))) \[CenterDot] ((a_) \[CenterDot] ((b_) \[CenterDot] 
            (c_))) == (((b_) \[CenterDot] (b_)) \[CenterDot] 
           (a_)) \[CenterDot] (((c_) \[CenterDot] (c_)) \[CenterDot] (a_))], 
      "Proof" -> <||>|>, {"Hypothesis", 1} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] a) == a], "Proof" -> <||>|>, 
    {"Conclusion", 1} -> <|"Statement" -> HoldForm[a == a], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {}, "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
           ((a_) \[CenterDot] (a_)) -> a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[a == a], "Source" -> "synth"|>|>}|>]
