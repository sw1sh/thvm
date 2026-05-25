ProofObject["EquationalLogic", 
 {ForAll[\[FormalA], OverBar[OverBar[\[FormalA]]] == \[FormalA]]}, 
 {ForAll[{\[FormalA], \[FormalB], \[FormalC]}, 
   \[FormalA] \[CirclePlus] (\[FormalB] \[CirclePlus] \[FormalC]) == 
    (\[FormalA] \[CirclePlus] \[FormalB]) \[CirclePlus] \[FormalC]], 
  ForAll[{\[FormalA], \[FormalB]}, \[FormalA] \[CirclePlus] \[FormalB] == 
    \[FormalB] \[CirclePlus] \[FormalA]], ForAll[{\[FormalA], \[FormalB]}, 
   OverBar[OverBar[\[FormalA]] \[CirclePlus] \[FormalB]] \[CirclePlus] 
     OverBar[OverBar[\[FormalA]] \[CirclePlus] OverBar[\[FormalB]]] == 
    \[FormalA]]}, <|"Variables" -> {\[FormalA], \[FormalB], \[FormalC], 
    \[FormalD], \[FormalE], \[FormalF], \[FormalG], \[FormalH]}, 
  "Constants" -> {}, "Proof" -> 
   {{"Axiom", 1} -> <|"Statement" -> HoldForm[\[FormalA] == 
         OverBar[OverBar[\[FormalA]] \[CirclePlus] \[FormalB]] \[CirclePlus] 
          OverBar[OverBar[\[FormalA]] \[CirclePlus] OverBar[\[FormalB]]]], 
      "Proof" -> <||>|>, {"Axiom", 2} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CirclePlus] \[FormalB] == 
         \[FormalB] \[CirclePlus] \[FormalA]], "Proof" -> <||>|>, 
    {"Axiom", 3} -> <|"Statement" -> HoldForm[
        \[FormalA] \[CirclePlus] (\[FormalB] \[CirclePlus] \[FormalC]) == 
         (\[FormalA] \[CirclePlus] \[FormalB]) \[CirclePlus] \[FormalC]], 
      "Proof" -> <||>|>, {"Hypothesis", 1} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[\[FormalH]]] == \[FormalH]], 
      "Proof" -> <||>|>, {"CriticalPairLemma", 1} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         OverBar[\[FormalB] \[CirclePlus] OverBar[\[FormalA]]] \[CirclePlus] 
          OverBar[OverBar[\[FormalA]] \[CirclePlus] OverBar[\[FormalB]]]], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> OverBar[OverBar[\[FormalA]_] \[CirclePlus] 
             (\[FormalB]_)] \[CirclePlus] OverBar[
            OverBar[\[FormalA]_] \[CirclePlus] OverBar[\[FormalB]_]] -> 
          \[FormalA], "Side" -> 1, "Subpattern" -> 
         OverBar[\[FormalA]_] \[CirclePlus] (\[FormalB]_), 
        "MatchingConstruct" -> {"Axiom", 2}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CirclePlus] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CirclePlus] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {1, 1}|>|>, {"CriticalPairLemma", 2} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         OverBar[OverBar[\[FormalA]] \[CirclePlus] \[FormalB]] \[CirclePlus] 
          OverBar[OverBar[\[FormalB]] \[CirclePlus] OverBar[\[FormalA]]]], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> OverBar[OverBar[\[FormalA]_] \[CirclePlus] 
             (\[FormalB]_)] \[CirclePlus] OverBar[
            OverBar[\[FormalA]_] \[CirclePlus] OverBar[\[FormalB]_]] -> 
          \[FormalA], "Side" -> 1, "Subpattern" -> 
         OverBar[\[FormalA]_] \[CirclePlus] OverBar[\[FormalB]_], 
        "MatchingConstruct" -> {"Axiom", 2}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CirclePlus] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CirclePlus] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2, 1}|>|>, {"CriticalPairLemma", 3} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CirclePlus] 
          (\[FormalB] \[CirclePlus] \[FormalC]) == \[FormalC] \[CirclePlus] 
          (\[FormalA] \[CirclePlus] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"Axiom", 3}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CirclePlus] (\[FormalB]_)) \[CirclePlus] 
           (\[FormalC]_) -> \[FormalA] \[CirclePlus] 
           (\[FormalB] \[CirclePlus] \[FormalC]), "Side" -> 1, 
        "Subpattern" -> ((\[FormalA]_) \[CirclePlus] 
           (\[FormalB]_)) \[CirclePlus] (\[FormalC]_), "MatchingConstruct" -> 
         {"Axiom", 2}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CirclePlus] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CirclePlus] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {}|>|>, {"CriticalPairLemma", 4} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[\[FormalA]] \[CirclePlus] 
            \[FormalB]] \[CirclePlus] 
          (OverBar[OverBar[\[FormalA]] \[CirclePlus] OverBar[
              \[FormalB]]] \[CirclePlus] \[FormalC]) == 
         \[FormalA] \[CirclePlus] \[FormalC]], 
      "Proof" -> <|"Construct" -> {"Axiom", 3}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CirclePlus] (\[FormalB]_)) \[CirclePlus] 
           (\[FormalC]_) -> \[FormalA] \[CirclePlus] 
           (\[FormalB] \[CirclePlus] \[FormalC]), "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CirclePlus] (\[FormalB]_), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> OverBar[OverBar[\[FormalA]_] \[CirclePlus] 
             (\[FormalB]_)] \[CirclePlus] OverBar[
            OverBar[\[FormalA]_] \[CirclePlus] OverBar[\[FormalB]_]] -> 
          \[FormalA], "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 5} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CirclePlus] 
          (\[FormalB] \[CirclePlus] \[FormalC]) == 
         (\[FormalB] \[CirclePlus] \[FormalA]) \[CirclePlus] \[FormalC]], 
      "Proof" -> <|"Construct" -> {"Axiom", 3}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CirclePlus] (\[FormalB]_)) \[CirclePlus] 
           (\[FormalC]_) -> \[FormalA] \[CirclePlus] 
           (\[FormalB] \[CirclePlus] \[FormalC]), "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CirclePlus] (\[FormalB]_), 
        "MatchingConstruct" -> {"Axiom", 2}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CirclePlus] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CirclePlus] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CirclePlus] 
          (\[FormalB] \[CirclePlus] \[FormalC]) == \[FormalB] \[CirclePlus] 
          (\[FormalA] \[CirclePlus] \[FormalC])], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 5}, "Position" -> {}, 
        "Construct" -> {"Axiom", 3}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CirclePlus] (\[FormalB]_)) \[CirclePlus] 
           (\[FormalC]_) -> \[FormalA] \[CirclePlus] 
           (\[FormalB] \[CirclePlus] \[FormalC]), "OutputExpression" -> 
         HoldForm[\[FormalA] \[CirclePlus] (\[FormalB] \[CirclePlus] 
             \[FormalC]) == \[FormalB] \[CirclePlus] 
            (\[FormalA] \[CirclePlus] \[FormalC])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, {"CriticalPairLemma", 6} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CirclePlus] 
          (\[FormalB] \[CirclePlus] \[FormalC]) == \[FormalC] \[CirclePlus] 
          (\[FormalB] \[CirclePlus] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 3}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CirclePlus] 
           ((\[FormalB]_) \[CirclePlus] (\[FormalC]_)) <-> 
          (\[FormalC]_) \[CirclePlus] ((\[FormalA]_) \[CirclePlus] 
            (\[FormalB]_)), "Side" -> 2, "Subpattern" -> 
         (\[FormalB]_) \[CirclePlus] (\[FormalC]_), "MatchingConstruct" -> 
         {"Axiom", 2}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CirclePlus] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CirclePlus] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 7} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         OverBar[\[FormalB] \[CirclePlus] OverBar[\[FormalA]]] \[CirclePlus] 
          OverBar[OverBar[\[FormalB]] \[CirclePlus] OverBar[\[FormalA]]]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 1}, 
        "Orientation" -> -1, "Rule" -> 
         OverBar[(\[FormalA]_) \[CirclePlus] OverBar[
              \[FormalB]_]] \[CirclePlus] OverBar[
            OverBar[\[FormalB]_] \[CirclePlus] OverBar[\[FormalA]_]] -> 
          \[FormalB], "Side" -> 1, "Subpattern" -> 
         OverBar[\[FormalB]_] \[CirclePlus] OverBar[\[FormalA]_], 
        "MatchingConstruct" -> {"Axiom", 2}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CirclePlus] (\[FormalB]_) <-> 
          (\[FormalB]_) \[CirclePlus] (\[FormalA]_), "MatchingSide" -> 1, 
        "Position" -> {2, 1}|>|>, {"CriticalPairLemma", 8} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CirclePlus] 
          OverBar[OverBar[\[FormalA]] \[CirclePlus] 
            OverBar[OverBar[\[FormalB]]]] == 
         OverBar[OverBar[\[FormalA]] \[CirclePlus] \[FormalB]] \[CirclePlus] 
          \[FormalA]], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 4}, 
        "Orientation" -> 1, "Rule" -> 
         OverBar[OverBar[\[FormalA]_] \[CirclePlus] 
             (\[FormalB]_)] \[CirclePlus] 
           (OverBar[OverBar[\[FormalA]_] \[CirclePlus] OverBar[
               \[FormalB]_]] \[CirclePlus] (\[FormalC]_)) -> 
          \[FormalA] \[CirclePlus] \[FormalC], "Side" -> 1, 
        "Subpattern" -> OverBar[OverBar[\[FormalA]_] \[CirclePlus] 
            OverBar[\[FormalB]_]] \[CirclePlus] (\[FormalC]_), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> OverBar[OverBar[\[FormalA]_] \[CirclePlus] 
             (\[FormalB]_)] \[CirclePlus] OverBar[
            OverBar[\[FormalA]_] \[CirclePlus] OverBar[\[FormalB]_]] -> 
          \[FormalA], "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CirclePlus] 
          OverBar[OverBar[\[FormalA]] \[CirclePlus] 
            OverBar[OverBar[\[FormalB]]]] == \[FormalA] \[CirclePlus] 
          OverBar[OverBar[\[FormalA]] \[CirclePlus] \[FormalB]]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 8}, "Position" -> {}, 
        "Construct" -> {"Axiom", 2}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CirclePlus] (\[FormalB]_) -> 
          \[FormalB] \[CirclePlus] \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CirclePlus] OverBar[
             OverBar[\[FormalA]] \[CirclePlus] OverBar[OverBar[
                \[FormalB]]]] == \[FormalA] \[CirclePlus] 
            OverBar[OverBar[\[FormalA]] \[CirclePlus] \[FormalB]]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 9} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CirclePlus] 
          (\[FormalB] \[CirclePlus] OverBar[OverBar[\[FormalA]] \[CirclePlus] 
             OverBar[OverBar[\[FormalC]]]]) == \[FormalB] \[CirclePlus] 
          (\[FormalA] \[CirclePlus] OverBar[OverBar[\[FormalA]] \[CirclePlus] 
             \[FormalC]])], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 1}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CirclePlus] ((\[FormalB]_) \[CirclePlus] 
            (\[FormalC]_)) <-> (\[FormalB]_) \[CirclePlus] 
           ((\[FormalA]_) \[CirclePlus] (\[FormalC]_)), "Side" -> 1, 
        "Subpattern" -> (\[FormalB]_) \[CirclePlus] (\[FormalC]_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 2}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CirclePlus] OverBar[
            OverBar[\[FormalA]_] \[CirclePlus] OverBar[OverBar[
               \[FormalB]_]]] -> \[FormalA] \[CirclePlus] 
           OverBar[OverBar[\[FormalA]] \[CirclePlus] \[FormalB]], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 10} -> 
     <|"Statement" -> HoldForm[
        OverBar[OverBar[OverBar[\[FormalA]]] \[CirclePlus] 
            \[FormalB]] \[CirclePlus] (\[FormalB] \[CirclePlus] 
           OverBar[OverBar[\[FormalB]] \[CirclePlus] \[FormalA]]) == 
         \[FormalB] \[CirclePlus] OverBar[\[FormalA]]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 9}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CirclePlus] 
           ((\[FormalB]_) \[CirclePlus] OverBar[
             OverBar[\[FormalA]_] \[CirclePlus] OverBar[OverBar[
                \[FormalC]_]]]) -> \[FormalB] \[CirclePlus] 
           (\[FormalA] \[CirclePlus] OverBar[
             OverBar[\[FormalA]] \[CirclePlus] \[FormalC]]), "Side" -> 1, 
        "Subpattern" -> (\[FormalB]_) \[CirclePlus] 
          OverBar[OverBar[\[FormalA]_] \[CirclePlus] 
            OverBar[OverBar[\[FormalC]_]]], "MatchingConstruct" -> 
         {"CriticalPairLemma", 2}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> OverBar[OverBar[\[FormalA]_] \[CirclePlus] 
             (\[FormalB]_)] \[CirclePlus] OverBar[
            OverBar[\[FormalB]_] \[CirclePlus] OverBar[\[FormalA]_]] -> 
          \[FormalA], "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 3} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[\[FormalA]] \[CirclePlus] 
            \[FormalB]] \[CirclePlus] (\[FormalA] \[CirclePlus] 
           OverBar[OverBar[OverBar[\[FormalB]]] \[CirclePlus] \[FormalA]]) == 
         \[FormalA] \[CirclePlus] OverBar[\[FormalB]]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 10}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 6}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CirclePlus] ((\[FormalB]_) \[CirclePlus] 
            (\[FormalC]_)) -> \[FormalC] \[CirclePlus] 
           (\[FormalB] \[CirclePlus] \[FormalA]), "OutputExpression" -> 
         HoldForm[OverBar[OverBar[\[FormalA]] \[CirclePlus] 
              \[FormalB]] \[CirclePlus] (\[FormalA] \[CirclePlus] 
             OverBar[OverBar[OverBar[\[FormalB]]] \[CirclePlus] 
               \[FormalA]]) == \[FormalA] \[CirclePlus] OverBar[\[FormalB]]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 4} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CirclePlus] 
          (OverBar[OverBar[\[FormalA]] \[CirclePlus] 
             \[FormalB]] \[CirclePlus] OverBar[
            OverBar[OverBar[\[FormalB]]] \[CirclePlus] \[FormalA]]) == 
         \[FormalA] \[CirclePlus] OverBar[\[FormalB]]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 3}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 1}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CirclePlus] ((\[FormalB]_) \[CirclePlus] 
            (\[FormalC]_)) -> \[FormalB] \[CirclePlus] 
           (\[FormalA] \[CirclePlus] \[FormalC]), "OutputExpression" -> 
         HoldForm[\[FormalA] \[CirclePlus] 
            (OverBar[OverBar[\[FormalA]] \[CirclePlus] 
               \[FormalB]] \[CirclePlus] OverBar[OverBar[OverBar[
                 \[FormalB]]] \[CirclePlus] \[FormalA]]) == 
           \[FormalA] \[CirclePlus] OverBar[\[FormalB]]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 11} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA]] \[CirclePlus] 
          OverBar[OverBar[\[FormalA]]] == OverBar[\[FormalA]] \[CirclePlus] 
          \[FormalA]], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 4}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CirclePlus] 
           (OverBar[OverBar[\[FormalA]_] \[CirclePlus] 
              (\[FormalB]_)] \[CirclePlus] OverBar[OverBar[OverBar[
                \[FormalB]_]] \[CirclePlus] (\[FormalA]_)]) -> 
          \[FormalA] \[CirclePlus] OverBar[\[FormalB]], "Side" -> 1, 
        "Subpattern" -> OverBar[OverBar[\[FormalA]_] \[CirclePlus] 
            (\[FormalB]_)] \[CirclePlus] OverBar[
           OverBar[OverBar[\[FormalB]_]] \[CirclePlus] (\[FormalA]_)], 
        "MatchingConstruct" -> {"CriticalPairLemma", 7}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         OverBar[(\[FormalA]_) \[CirclePlus] OverBar[
              \[FormalB]_]] \[CirclePlus] OverBar[
            OverBar[\[FormalA]_] \[CirclePlus] OverBar[\[FormalB]_]] -> 
          \[FormalB], "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 5} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA]] \[CirclePlus] 
          OverBar[OverBar[\[FormalA]]] == \[FormalA] \[CirclePlus] 
          OverBar[\[FormalA]]], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 11}, "Position" -> {}, 
        "Construct" -> {"Axiom", 2}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CirclePlus] (\[FormalB]_) -> 
          \[FormalB] \[CirclePlus] \[FormalA], "OutputExpression" -> 
         HoldForm[OverBar[\[FormalA]] \[CirclePlus] 
            OverBar[OverBar[\[FormalA]]] == \[FormalA] \[CirclePlus] 
            OverBar[\[FormalA]]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 12} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA]] == 
         OverBar[\[FormalA] \[CirclePlus] OverBar[OverBar[
              \[FormalA]]]] \[CirclePlus] OverBar[\[FormalA] \[CirclePlus] 
            OverBar[\[FormalA]]]], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 7}, "Orientation" -> -1, 
        "Rule" -> OverBar[(\[FormalA]_) \[CirclePlus] 
             OverBar[\[FormalB]_]] \[CirclePlus] OverBar[
            OverBar[\[FormalA]_] \[CirclePlus] OverBar[\[FormalB]_]] -> 
          \[FormalB], "Side" -> 1, "Subpattern" -> 
         OverBar[\[FormalA]_] \[CirclePlus] OverBar[\[FormalB]_], 
        "MatchingConstruct" -> {"SubstitutionLemma", 5}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         OverBar[\[FormalA]_] \[CirclePlus] OverBar[OverBar[\[FormalA]_]] -> 
          \[FormalA] \[CirclePlus] OverBar[\[FormalA]], "MatchingSide" -> 1, 
        "Position" -> {2, 1}|>|>, {"SubstitutionLemma", 6} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA]] == 
         OverBar[\[FormalA] \[CirclePlus] OverBar[\[FormalA]]] \[CirclePlus] 
          OverBar[\[FormalA] \[CirclePlus] OverBar[OverBar[\[FormalA]]]]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 12}, "Position" -> {}, 
        "Construct" -> {"Axiom", 2}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CirclePlus] (\[FormalB]_) -> 
          \[FormalB] \[CirclePlus] \[FormalA], "OutputExpression" -> 
         HoldForm[OverBar[\[FormalA]] == OverBar[\[FormalA] \[CirclePlus] 
              OverBar[\[FormalA]]] \[CirclePlus] OverBar[
             \[FormalA] \[CirclePlus] OverBar[OverBar[\[FormalA]]]]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 13} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[\[FormalA]]] == \[FormalA]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 6}, 
        "Orientation" -> -1, "Rule" -> 
         OverBar[(\[FormalA]_) \[CirclePlus] OverBar[
              \[FormalA]_]] \[CirclePlus] OverBar[(\[FormalA]_) \[CirclePlus] 
             OverBar[OverBar[\[FormalA]_]]] -> OverBar[\[FormalA]], 
        "Side" -> 1, "Subpattern" -> OverBar[(\[FormalA]_) \[CirclePlus] 
            OverBar[\[FormalA]_]] \[CirclePlus] 
          OverBar[(\[FormalA]_) \[CirclePlus] OverBar[OverBar[\[FormalA]_]]], 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> OverBar[OverBar[\[FormalA]_] \[CirclePlus] 
             (\[FormalB]_)] \[CirclePlus] OverBar[
            OverBar[\[FormalA]_] \[CirclePlus] OverBar[\[FormalB]_]] -> 
          \[FormalA], "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"Conclusion", 1} -> <|"Statement" -> HoldForm[\[FormalH] == \[FormalH]], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 13}, "Orientation" -> 1, 
        "Rule" -> OverBar[OverBar[\[FormalA]_]] -> \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalH] == \[FormalH]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>}|>]
