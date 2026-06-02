ProofObject["EquationalLogic", Inactive[Equal][a \[CircleTimes] OverTilde[0], 
  OverTilde[0]], {Inactive[Equal][(a_) \[CirclePlus] 
    ((b_) \[CirclePlus] (c_)), ((a_) \[CirclePlus] (b_)) \[CirclePlus] (c_)], 
  Inactive[Equal][(a_) \[CirclePlus] (b_), (b_) \[CirclePlus] (a_)], 
  Inactive[Equal][(a_) \[CirclePlus] OverTilde[0], a_], 
  Inactive[Equal][(a_) \[CirclePlus] OverBar[a_], OverTilde[0]], 
  Inactive[Equal][(a_) \[CircleTimes] ((b_) \[CirclePlus] (c_)), 
   (a_) \[CircleTimes] (b_) \[CirclePlus] (a_) \[CircleTimes] (c_)], 
  Inactive[Equal][(a_) \[CircleTimes] ((b_) \[CircleTimes] (c_)), 
   ((a_) \[CircleTimes] (b_)) \[CircleTimes] (c_)], 
  Inactive[Equal][(a_) \[CircleTimes] OverTilde[1], a_], 
  Inactive[Equal][((a_) \[CirclePlus] (b_)) \[CircleTimes] (c_), 
   (a_) \[CircleTimes] (c_) \[CirclePlus] (b_) \[CircleTimes] (c_)]}, 
 <|"Variables" -> {a, b, c}, "Constants" -> {}, 
  "Proof" -> {{"Axiom", 1} -> <|"Statement" -> 
       HoldForm[a \[CirclePlus] (b \[CirclePlus] c) == 
         (a \[CirclePlus] b) \[CirclePlus] c], "Proof" -> <||>|>, 
    {"Axiom", 2} -> <|"Statement" -> HoldForm[a \[CirclePlus] b == 
         b \[CirclePlus] a], "Proof" -> <||>|>, 
    {"Axiom", 3} -> <|"Statement" -> HoldForm[a \[CirclePlus] OverTilde[0] == 
         a], "Proof" -> <||>|>, {"Axiom", 4} -> 
     <|"Statement" -> HoldForm[a \[CirclePlus] OverBar[a] == OverTilde[0]], 
      "Proof" -> <||>|>, {"Axiom", 5} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CirclePlus] c) == 
         a \[CircleTimes] b \[CirclePlus] a \[CircleTimes] c], 
      "Proof" -> <||>|>, {"Axiom", 6} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (b \[CircleTimes] c) == 
         (a \[CircleTimes] b) \[CircleTimes] c], "Proof" -> <||>|>, 
    {"Axiom", 7} -> <|"Statement" -> HoldForm[
        a \[CircleTimes] OverTilde[1] == a], "Proof" -> <||>|>, 
    {"Axiom", 8} -> <|"Statement" -> HoldForm[
        (a \[CirclePlus] b) \[CircleTimes] c == 
         a \[CircleTimes] c \[CirclePlus] b \[CircleTimes] c], 
      "Proof" -> <||>|>, {"Hypothesis", 1} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] OverTilde[0] == 
         OverTilde[0]], "Proof" -> <||>|>, {"CriticalPairLemma", 1} -> 
     <|"Statement" -> HoldForm[b \[CirclePlus] (OverBar[b] \[CirclePlus] 
           a) == OverTilde[0] \[CirclePlus] a], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> ((a_) \[CirclePlus] (b_)) \[CirclePlus] (c_) -> 
          a \[CirclePlus] (b \[CirclePlus] c), "Side" -> 1, 
        "Subpattern" -> (a_) \[CirclePlus] (b_), "MatchingConstruct" -> 
         {"Axiom", 4}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CirclePlus] OverBar[a_] -> OverTilde[0], "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 2} -> 
     <|"Statement" -> HoldForm[OverTilde[0] \[CirclePlus] a == a], 
      "Proof" -> <|"Construct" -> {"Axiom", 2}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, "Side" -> 1, 
        "Subpattern" -> {}, "MatchingConstruct" -> {"Axiom", 3}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CirclePlus] OverTilde[0] -> a, "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[b \[CirclePlus] (OverBar[b] \[CirclePlus] 
           a) == a], "Proof" -> <|"Input" -> {"CriticalPairLemma", 1}, 
        "Construct" -> {"CriticalPairLemma", 2}, "Position" -> {}, 
        "Rule" -> OverTilde[0] \[CirclePlus] (a_) -> a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[b \[CirclePlus] 
            (OverBar[b] \[CirclePlus] a) == a], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 3} -> 
     <|"Statement" -> HoldForm[b == (OverBar[a] \[CirclePlus] 
           b) \[CirclePlus] a], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 1}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CirclePlus] (OverBar[a_] \[CirclePlus] (b_)) -> b, 
        "Side" -> 1, "Subpattern" -> {}, "MatchingConstruct" -> {"Axiom", 2}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CirclePlus] (b_) -> b \[CirclePlus] a, "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[b == OverBar[a] \[CirclePlus] 
          (b \[CirclePlus] a)], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 3}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {}, "Rule" -> ((a_) \[CirclePlus] (b_)) \[CirclePlus] 
           (c_) -> a \[CirclePlus] (b \[CirclePlus] c), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[b == OverBar[a] \[CirclePlus] 
            (b \[CirclePlus] a)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 4} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[a]] == a \[CirclePlus] 
          OverTilde[0]], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 
          1}, "Orientation" -> 1, "Rule" -> (a_) \[CirclePlus] 
           (OverBar[a_] \[CirclePlus] (b_)) -> b, "Side" -> 1, 
        "Subpattern" -> OverBar[a_] \[CirclePlus] (b_), 
        "MatchingConstruct" -> {"Axiom", 4}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CirclePlus] OverBar[a_] -> OverTilde[0], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 3} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[a]] == a], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 4}, 
        "Construct" -> {"Axiom", 3}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] OverTilde[0] -> a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[OverBar[a]] == a], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 5} -> 
     <|"Statement" -> HoldForm[b == OverBar[a] \[CirclePlus] 
          (a \[CirclePlus] b)], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 1}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CirclePlus] (OverBar[a_] \[CirclePlus] (b_)) -> b, 
        "Side" -> 1, "Subpattern" -> OverBar[a_], "MatchingConstruct" -> 
         {"SubstitutionLemma", 3}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> OverBar[OverBar[a_]] -> a, "MatchingSide" -> 1, 
        "Position" -> {2, 1}|>|>, {"CriticalPairLemma", 6} -> 
     <|"Statement" -> HoldForm[OverBar[a] == 
         OverBar[a \[CirclePlus] b] \[CirclePlus] b], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 2}, 
        "Orientation" -> -1, "Rule" -> OverBar[a_] \[CirclePlus] 
           ((b_) \[CirclePlus] (a_)) -> b, "Side" -> 1, 
        "Subpattern" -> (b_) \[CirclePlus] (a_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 5}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> OverBar[a_] \[CirclePlus] ((a_) \[CirclePlus] 
            (b_)) -> b, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 4} -> 
     <|"Statement" -> HoldForm[OverBar[a] == b \[CirclePlus] 
          OverBar[a \[CirclePlus] b]], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 6}, "Construct" -> {"Axiom", 2}, 
        "Position" -> {}, "Rule" -> (a_) \[CirclePlus] (b_) -> 
          b \[CirclePlus] a, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[OverBar[a] == b \[CirclePlus] OverBar[a \[CirclePlus] b]], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 7} -> 
     <|"Statement" -> HoldForm[OverBar[a \[CircleTimes] c] == 
         a \[CircleTimes] b \[CirclePlus] OverBar[a \[CircleTimes] 
            (c \[CirclePlus] b)]], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 4}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CirclePlus] OverBar[(b_) \[CirclePlus] (a_)] -> 
          OverBar[b], "Side" -> 1, "Subpattern" -> (b_) \[CirclePlus] (a_), 
        "MatchingConstruct" -> {"Axiom", 5}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CircleTimes] (b_) \[CirclePlus] 
           (a_) \[CircleTimes] (c_) -> a \[CircleTimes] (b \[CirclePlus] c), 
        "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"CriticalPairLemma", 8} -> 
     <|"Statement" -> HoldForm[OverBar[a \[CircleTimes] OverTilde[0]] == 
         a \[CircleTimes] b \[CirclePlus] OverBar[a \[CircleTimes] b]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 7}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CircleTimes] (b_) \[CirclePlus] 
           OverBar[(a_) \[CircleTimes] ((c_) \[CirclePlus] (b_))] -> 
          OverBar[a \[CircleTimes] c], "Side" -> 1, "Subpattern" -> 
         (c_) \[CirclePlus] (b_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 2}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> OverTilde[0] \[CirclePlus] (a_) -> a, 
        "MatchingSide" -> 1, "Position" -> {2, 1, 2}|>|>, 
    {"SubstitutionLemma", 5} -> 
     <|"Statement" -> HoldForm[OverBar[a \[CircleTimes] OverTilde[0]] == 
         OverTilde[0]], "Proof" -> <|"Input" -> {"CriticalPairLemma", 8}, 
        "Construct" -> {"Axiom", 4}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] OverBar[a_] -> OverTilde[0], 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          OverBar[a \[CircleTimes] OverTilde[0]] == OverTilde[0]], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 9} -> 
     <|"Statement" -> HoldForm[OverTilde[0] == 
         a \[CircleTimes] OverTilde[0] \[CirclePlus] OverTilde[0]], 
      "Proof" -> <|"Construct" -> {"Axiom", 4}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CirclePlus] OverBar[a_] -> OverTilde[0], 
        "Side" -> 1, "Subpattern" -> OverBar[a_], "MatchingConstruct" -> 
         {"SubstitutionLemma", 5}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> OverBar[(a_) \[CircleTimes] OverTilde[0]] -> 
          OverTilde[0], "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 6} -> 
     <|"Statement" -> HoldForm[OverTilde[0] == a \[CircleTimes] 
          OverTilde[0]], "Proof" -> <|"Input" -> {"CriticalPairLemma", 9}, 
        "Construct" -> {"Axiom", 3}, "Position" -> {}, 
        "Rule" -> (a_) \[CirclePlus] OverTilde[0] -> a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverTilde[0] == a \[CircleTimes] 
            OverTilde[0]], "Source" -> "norm"|>|>, 
    {"Conclusion", 1} -> <|"Statement" -> HoldForm[OverTilde[0] == 
         OverTilde[0]], "Proof" -> <|"Input" -> {"Hypothesis", 1}, 
        "Construct" -> {"SubstitutionLemma", 6}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] OverTilde[0] -> OverTilde[0], 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[OverTilde[0] == 
           OverTilde[0]], "Source" -> "cpl"|>|>}|>]
