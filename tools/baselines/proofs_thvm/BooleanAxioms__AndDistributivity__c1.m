ProofObject["EquationalLogic", Inactive[Equal][
  a \[CircleTimes] (b \[CirclePlus] c), a \[CircleTimes] b \[CirclePlus] 
   a \[CircleTimes] c], {Inactive[Equal][(a_) \[CircleTimes] (b_), 
   (b_) \[CircleTimes] (a_)], Inactive[Equal][(a_) \[CircleTimes] 
    ((b_) \[CirclePlus] (c_)), (a_) \[CircleTimes] (b_) \[CirclePlus] 
    (a_) \[CircleTimes] (c_)], Inactive[Equal][
   (a_) \[CirclePlus] (b_) \[CircleTimes] OverBar[b_], a_], 
  Inactive[Equal][(a_) \[CircleTimes] ((b_) \[CirclePlus] OverBar[b_]), a_], 
  Inactive[Equal][(a_) \[CirclePlus] (b_), (b_) \[CirclePlus] (a_)], 
  Inactive[Equal][(a_) \[CirclePlus] (b_) \[CircleTimes] (c_), 
   ((a_) \[CirclePlus] (b_)) \[CircleTimes] ((a_) \[CirclePlus] (c_))]}, 
 <|"Variables" -> {a, b, c}, "Constants" -> {}, 
  "Proof" -> {{"Axiom", 1} -> <|"Statement" -> 
       HoldForm[(a_) \[CircleTimes] (b_) == (b_) \[CircleTimes] (a_)], 
      "Proof" -> <||>|>, {"Axiom", 2} -> 
     <|"Statement" -> HoldForm[(a_) \[CircleTimes] ((b_) \[CirclePlus] 
           (c_)) == (a_) \[CircleTimes] (b_) \[CirclePlus] 
          (a_) \[CircleTimes] (c_)], "Proof" -> <||>|>, 
    {"Axiom", 3} -> <|"Statement" -> HoldForm[
        (a_) \[CirclePlus] (b_) \[CircleTimes] OverBar[b_] == (a_)], 
      "Proof" -> <||>|>, {"Axiom", 4} -> 
     <|"Statement" -> HoldForm[(a_) \[CircleTimes] ((b_) \[CirclePlus] 
           OverBar[b_]) == (a_)], "Proof" -> <||>|>, 
    {"Axiom", 5} -> <|"Statement" -> HoldForm[(a_) \[CirclePlus] (b_) == 
         (b_) \[CirclePlus] (a_)], "Proof" -> <||>|>, 
    {"Axiom", 6} -> <|"Statement" -> HoldForm[
        (a_) \[CirclePlus] (b_) \[CircleTimes] (c_) == 
         ((a_) \[CirclePlus] (b_)) \[CircleTimes] ((a_) \[CirclePlus] (c_))], 
      "Proof" -> <||>|>, {"Hypothesis", 1} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CirclePlus] c) == 
         a \[CircleTimes] b \[CirclePlus] a \[CircleTimes] c], 
      "Proof" -> <||>|>, {"Conclusion", 1} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CirclePlus] c) == 
         a \[CircleTimes] (b \[CirclePlus] c)], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Construct" -> {"Axiom", 2}, 
        "Position" -> {}, "Rule" -> (a_) \[CircleTimes] (b_) \[CirclePlus] 
           (a_) \[CircleTimes] (c_) -> a \[CircleTimes] (b \[CirclePlus] c), 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CircleTimes] (b \[CirclePlus] c) == a \[CircleTimes] 
            (b \[CirclePlus] c)], "Source" -> "synth"|>|>}|>]
