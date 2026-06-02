ProofObject["EquationalLogic", Inactive[Equal][a \[CirclePlus] b, 
  b \[CirclePlus] a], 
 {Inactive[Equal][(a_) \[CirclePlus] ((b_) \[CirclePlus] (c_)), 
   ((a_) \[CirclePlus] (b_)) \[CirclePlus] (c_)], 
  Inactive[Equal][(a_) \[CirclePlus] (b_), (b_) \[CirclePlus] (a_)], 
  Inactive[Equal][OverBar[OverBar[a_] \[CirclePlus] (b_)] \[CirclePlus] 
    OverBar[OverBar[a_] \[CirclePlus] OverBar[b_]], a_]}, 
 <|"Variables" -> {a, b, c}, "Constants" -> {}, 
  "Proof" -> {{"Axiom", 1} -> <|"Statement" -> 
       HoldForm[(a_) \[CirclePlus] ((b_) \[CirclePlus] (c_)) == 
         ((a_) \[CirclePlus] (b_)) \[CirclePlus] (c_)], "Proof" -> <||>|>, 
    {"Axiom", 2} -> <|"Statement" -> HoldForm[(a_) \[CirclePlus] (b_) == 
         (b_) \[CirclePlus] (a_)], "Proof" -> <||>|>, 
    {"Axiom", 3} -> <|"Statement" -> HoldForm[
        OverBar[OverBar[a_] \[CirclePlus] (b_)] \[CirclePlus] 
          OverBar[OverBar[a_] \[CirclePlus] OverBar[b_]] == (a_)], 
      "Proof" -> <||>|>, {"Hypothesis", 1} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] b == b \[CirclePlus] a], 
      "Proof" -> <||>|>, {"Conclusion", 1} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] b == a \[CirclePlus] b], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Construct" -> {"Axiom", 2}, 
        "Position" -> {}, "Rule" -> (a_) \[CirclePlus] (b_) -> 
          b \[CirclePlus] a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[a \[CirclePlus] b == a \[CirclePlus] b], 
        "Source" -> "synth"|>|>}|>]
