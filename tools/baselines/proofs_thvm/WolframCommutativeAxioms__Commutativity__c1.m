ProofObject["EquationalLogic", Inactive[Equal][a \[CenterDot] b, 
  b \[CenterDot] a], {Inactive[Equal][(a_) \[CenterDot] (b_), 
   (b_) \[CenterDot] (a_)], Inactive[Equal][
   ((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
     ((b_) \[CenterDot] (c_))), a_]}, <|"Variables" -> {a, b, c}, 
  "Constants" -> {}, "Proof" -> 
   {{"Axiom", 1} -> <|"Statement" -> HoldForm[(a_) \[CenterDot] (b_) == 
         (b_) \[CenterDot] (a_)], "Proof" -> <||>|>, 
    {"Axiom", 2} -> <|"Statement" -> HoldForm[
        ((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
           ((b_) \[CenterDot] (c_))) == (a_)], "Proof" -> <||>|>, 
    {"Hypothesis", 1} -> <|"Statement" -> HoldForm[a \[CenterDot] b == 
         b \[CenterDot] a], "Proof" -> <||>|>, {"Conclusion", 1} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] b == a \[CenterDot] b], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {}, "Rule" -> (a_) \[CenterDot] (b_) -> 
          b \[CenterDot] a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[a \[CenterDot] b == a \[CenterDot] b], 
        "Source" -> "synth"|>|>}|>]
