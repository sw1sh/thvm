ProofObject["EquationalLogic", {ForAll[{\[FormalA], \[FormalB], \[FormalC]}, 
   \[FormalA] \[CircleTimes] (\[FormalB] \[CircleTimes] \[FormalC]) == 
    (\[FormalA] \[CircleTimes] \[FormalB]) \[CircleTimes] \[FormalC]]}, 
 {ForAll[{\[FormalA], \[FormalB], \[FormalC], \[FormalD]}, 
   \[FormalA] \[CircleTimes] OverBar[\[FormalB] \[CircleTimes] 
       (((\[FormalC] \[CircleTimes] OverBar[\[FormalC]]) \[CircleTimes] 
         OverBar[\[FormalD] \[CircleTimes] \[FormalB]]) \[CircleTimes] 
        \[FormalA])] == \[FormalD]]}, 
 <|"Variables" -> {\[FormalA], \[FormalB], \[FormalC], \[FormalD], 
    \[FormalE], \[FormalF], \[FormalG], \[FormalAlpha], \[FormalBeta]}, 
  "Constants" -> {}, "Proof" -> 
   {{"Axiom", 1} -> <|"Statement" -> HoldForm[\[FormalA] == 
         \[FormalB] \[CircleTimes] OverBar[\[FormalC] \[CircleTimes] 
            (((\[FormalD] \[CircleTimes] OverBar[\[FormalD]]) \[CircleTimes] 
              OverBar[\[FormalA] \[CircleTimes] \[FormalC]]) \[CircleTimes] 
             \[FormalB])]], "Proof" -> <||>|>, {"Hypothesis", 1} -> 
     <|"Statement" -> HoldForm[(\[FormalE] \[CircleTimes] 
           \[FormalF]) \[CircleTimes] \[FormalG] == \[FormalE] \[CircleTimes] 
          (\[FormalF] \[CircleTimes] \[FormalG])], "Proof" -> <||>|>, 
    {"CriticalPairLemma", 1} -> 
     <|"Statement" -> HoldForm[\[FormalA] == \[FormalB] \[CircleTimes] 
          OverBar[(((\[FormalC] \[CircleTimes] OverBar[
                \[FormalC]]) \[CircleTimes] OverBar[\[FormalD] \[CircleTimes] 
                \[FormalA]]) \[CircleTimes] (\[FormalAlpha] \[CircleTimes] 
              OverBar[\[FormalAlpha]])) \[CircleTimes] 
            (\[FormalD] \[CircleTimes] \[FormalB])]], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] OverBar[
            (\[FormalB]_) \[CircleTimes] ((((\[FormalC]_) \[CircleTimes] 
                OverBar[\[FormalC]_]) \[CircleTimes] OverBar[
                (\[FormalD]_) \[CircleTimes] (\[FormalB]_)]) \[CircleTimes] 
              (\[FormalA]_))] -> \[FormalD], "Side" -> 1, 
        "Subpattern" -> ((\[FormalC]_) \[CircleTimes] 
           OverBar[\[FormalC]_]) \[CircleTimes] 
          OverBar[(\[FormalD]_) \[CircleTimes] (\[FormalB]_)], 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CircleTimes] 
           OverBar[(\[FormalB]_) \[CircleTimes] 
             ((((\[FormalC]_) \[CircleTimes] OverBar[
                 \[FormalC]_]) \[CircleTimes] OverBar[
                (\[FormalD]_) \[CircleTimes] (\[FormalB]_)]) \[CircleTimes] 
              (\[FormalA]_))] -> \[FormalD], "MatchingSide" -> 1, 
        "Position" -> {2, 1, 2, 1}|>|>, {"CriticalPairLemma", 2} -> 
     <|"Statement" -> HoldForm[((\[FormalA] \[CircleTimes] 
            OverBar[\[FormalA]]) \[CircleTimes] OverBar[
            \[FormalB] \[CircleTimes] \[FormalC]]) \[CircleTimes] 
          (\[FormalD] \[CircleTimes] OverBar[\[FormalD]]) == 
         \[FormalAlpha] \[CircleTimes] OverBar[(\[FormalB] \[CircleTimes] 
             (\[FormalBeta] \[CircleTimes] OverBar[
               \[FormalBeta]])) \[CircleTimes] (\[FormalC] \[CircleTimes] 
             \[FormalAlpha])]], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 1}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] OverBar[
            ((((\[FormalB]_) \[CircleTimes] OverBar[
                 \[FormalB]_]) \[CircleTimes] OverBar[
                (\[FormalC]_) \[CircleTimes] (\[FormalD]_)]) \[CircleTimes] 
              ((\[FormalAlpha]_) \[CircleTimes] OverBar[
                \[FormalAlpha]_])) \[CircleTimes] 
             ((\[FormalC]_) \[CircleTimes] (\[FormalA]_))] -> \[FormalD], 
        "Side" -> 1, "Subpattern" -> ((\[FormalB]_) \[CircleTimes] 
           OverBar[\[FormalB]_]) \[CircleTimes] 
          OverBar[(\[FormalC]_) \[CircleTimes] (\[FormalD]_)], 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CircleTimes] 
           OverBar[(\[FormalB]_) \[CircleTimes] 
             ((((\[FormalC]_) \[CircleTimes] OverBar[
                 \[FormalC]_]) \[CircleTimes] OverBar[
                (\[FormalD]_) \[CircleTimes] (\[FormalB]_)]) \[CircleTimes] 
              (\[FormalA]_))] -> \[FormalD], "MatchingSide" -> 1, 
        "Position" -> {2, 1, 1, 1}|>|>, {"CriticalPairLemma", 3} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CircleTimes] 
           OverBar[\[FormalB]]) \[CircleTimes] OverBar[
           \[FormalC] \[CircleTimes] (\[FormalD] \[CircleTimes] 
             OverBar[(\[FormalA] \[CircleTimes] 
                (\[FormalAlpha] \[CircleTimes] OverBar[
                  \[FormalAlpha]])) \[CircleTimes] (\[FormalC] \[CircleTimes] 
                \[FormalD])])]], "Proof" -> <|"Construct" -> {"Axiom", 1}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CircleTimes] 
           OverBar[(\[FormalB]_) \[CircleTimes] 
             ((((\[FormalC]_) \[CircleTimes] OverBar[
                 \[FormalC]_]) \[CircleTimes] OverBar[
                (\[FormalD]_) \[CircleTimes] (\[FormalB]_)]) \[CircleTimes] 
              (\[FormalA]_))] -> \[FormalD], "Side" -> 1, 
        "Subpattern" -> (((\[FormalC]_) \[CircleTimes] 
            OverBar[\[FormalC]_]) \[CircleTimes] OverBar[
            (\[FormalD]_) \[CircleTimes] (\[FormalB]_)]) \[CircleTimes] 
          (\[FormalA]_), "MatchingConstruct" -> {"CriticalPairLemma", 2}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (((\[FormalA]_) \[CircleTimes] OverBar[\[FormalA]_]) \[CircleTimes] 
            OverBar[(\[FormalB]_) \[CircleTimes] 
              (\[FormalC]_)]) \[CircleTimes] ((\[FormalD]_) \[CircleTimes] 
            OverBar[\[FormalD]_]) <-> (\[FormalAlpha]_) \[CircleTimes] 
           OverBar[((\[FormalB]_) \[CircleTimes] 
              ((\[FormalBeta]_) \[CircleTimes] OverBar[
                \[FormalBeta]_])) \[CircleTimes] 
             ((\[FormalC]_) \[CircleTimes] (\[FormalAlpha]_))], 
        "MatchingSide" -> 1, "Position" -> {2, 1, 2}|>|>, 
    {"CriticalPairLemma", 4} -> 
     <|"Statement" -> HoldForm[\[FormalA] == \[FormalB] \[CircleTimes] 
          OverBar[(\[FormalC] \[CircleTimes] OverBar[
              (\[FormalD] \[CircleTimes] (\[FormalAlpha] \[CircleTimes] 
                 OverBar[\[FormalAlpha]])) \[CircleTimes] (
                \[FormalA] \[CircleTimes] \[FormalC])]) \[CircleTimes] 
            (\[FormalD] \[CircleTimes] \[FormalB])]], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] OverBar[
            (\[FormalB]_) \[CircleTimes] ((((\[FormalC]_) \[CircleTimes] 
                OverBar[\[FormalC]_]) \[CircleTimes] OverBar[
                (\[FormalD]_) \[CircleTimes] (\[FormalB]_)]) \[CircleTimes] 
              (\[FormalA]_))] -> \[FormalD], "Side" -> 1, 
        "Subpattern" -> ((\[FormalC]_) \[CircleTimes] 
           OverBar[\[FormalC]_]) \[CircleTimes] 
          OverBar[(\[FormalD]_) \[CircleTimes] (\[FormalB]_)], 
        "MatchingConstruct" -> {"CriticalPairLemma", 3}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CircleTimes] OverBar[\[FormalA]_]) \[CircleTimes] 
           OverBar[(\[FormalB]_) \[CircleTimes] ((\[FormalC]_) \[CircleTimes] 
              OverBar[((\[FormalD]_) \[CircleTimes] 
                 ((\[FormalAlpha]_) \[CircleTimes] OverBar[
                   \[FormalAlpha]_])) \[CircleTimes] 
                ((\[FormalB]_) \[CircleTimes] (\[FormalC]_))])] -> 
          \[FormalD], "MatchingSide" -> 1, "Position" -> {2, 1, 2, 1}|>|>, 
    {"CriticalPairLemma", 5} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CircleTimes] 
           OverBar[\[FormalA]]) \[CircleTimes] OverBar[
           \[FormalB] \[CircleTimes] (\[FormalC] \[CircleTimes] 
             (\[FormalD] \[CircleTimes] OverBar[\[FormalD]]))] == 
         \[FormalAlpha] \[CircleTimes] OverBar[\[FormalB] \[CircleTimes] 
            (\[FormalC] \[CircleTimes] \[FormalAlpha])]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 4}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CircleTimes] 
           OverBar[((\[FormalB]_) \[CircleTimes] OverBar[
               ((\[FormalC]_) \[CircleTimes] ((\[FormalD]_) \[CircleTimes] 
                  OverBar[\[FormalD]_])) \[CircleTimes] 
                ((\[FormalAlpha]_) \[CircleTimes] 
                 (\[FormalB]_))]) \[CircleTimes] 
             ((\[FormalC]_) \[CircleTimes] (\[FormalA]_))] -> \[FormalAlpha], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CircleTimes] 
          OverBar[((\[FormalC]_) \[CircleTimes] ((\[FormalD]_) \[CircleTimes] 
              OverBar[\[FormalD]_])) \[CircleTimes] 
            ((\[FormalAlpha]_) \[CircleTimes] (\[FormalB]_))], 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CircleTimes] 
           OverBar[(\[FormalB]_) \[CircleTimes] 
             ((((\[FormalC]_) \[CircleTimes] OverBar[
                 \[FormalC]_]) \[CircleTimes] OverBar[
                (\[FormalD]_) \[CircleTimes] (\[FormalB]_)]) \[CircleTimes] 
              (\[FormalA]_))] -> \[FormalD], "MatchingSide" -> 1, 
        "Position" -> {2, 1, 1}|>|>, {"CriticalPairLemma", 6} -> 
     <|"Statement" -> HoldForm[\[FormalA] == \[FormalB] \[CircleTimes] 
          OverBar[(\[FormalC] \[CircleTimes] (\[FormalD] \[CircleTimes] 
              OverBar[\[FormalD]])) \[CircleTimes] 
            ((\[FormalAlpha] \[CircleTimes] OverBar[\[FormalA] \[CircleTimes] 
                (\[FormalC] \[CircleTimes] \[FormalAlpha])]) \[CircleTimes] 
             \[FormalB])]], "Proof" -> <|"Construct" -> {"Axiom", 1}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CircleTimes] 
           OverBar[(\[FormalB]_) \[CircleTimes] 
             ((((\[FormalC]_) \[CircleTimes] OverBar[
                 \[FormalC]_]) \[CircleTimes] OverBar[
                (\[FormalD]_) \[CircleTimes] (\[FormalB]_)]) \[CircleTimes] 
              (\[FormalA]_))] -> \[FormalD], "Side" -> 1, 
        "Subpattern" -> ((\[FormalC]_) \[CircleTimes] 
           OverBar[\[FormalC]_]) \[CircleTimes] 
          OverBar[(\[FormalD]_) \[CircleTimes] (\[FormalB]_)], 
        "MatchingConstruct" -> {"CriticalPairLemma", 5}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((\[FormalA]_) \[CircleTimes] OverBar[\[FormalA]_]) \[CircleTimes] 
           OverBar[(\[FormalB]_) \[CircleTimes] ((\[FormalC]_) \[CircleTimes] 
              ((\[FormalD]_) \[CircleTimes] OverBar[\[FormalD]_]))] <-> 
          (\[FormalAlpha]_) \[CircleTimes] OverBar[
            (\[FormalB]_) \[CircleTimes] ((\[FormalC]_) \[CircleTimes] 
              (\[FormalAlpha]_))], "MatchingSide" -> 1, 
        "Position" -> {2, 1, 2, 1}|>|>, {"CriticalPairLemma", 7} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          OverBar[\[FormalB] \[CircleTimes] (\[FormalC] \[CircleTimes] 
             \[FormalA])] == \[FormalD] \[CircleTimes] 
          OverBar[\[FormalB] \[CircleTimes] (\[FormalC] \[CircleTimes] 
             \[FormalD])]], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 5}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CircleTimes] OverBar[
             \[FormalA]_]) \[CircleTimes] OverBar[
            (\[FormalB]_) \[CircleTimes] ((\[FormalC]_) \[CircleTimes] 
              ((\[FormalD]_) \[CircleTimes] OverBar[\[FormalD]_]))] <-> 
          (\[FormalAlpha]_) \[CircleTimes] OverBar[
            (\[FormalB]_) \[CircleTimes] ((\[FormalC]_) \[CircleTimes] 
              (\[FormalAlpha]_))], "Side" -> 1, "Subpattern" -> 
         ((\[FormalA]_) \[CircleTimes] OverBar[\[FormalA]_]) \[CircleTimes] 
          OverBar[(\[FormalB]_) \[CircleTimes] ((\[FormalC]_) \[CircleTimes] 
             ((\[FormalD]_) \[CircleTimes] OverBar[\[FormalD]_]))], 
        "MatchingConstruct" -> {"CriticalPairLemma", 5}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((\[FormalA]_) \[CircleTimes] OverBar[\[FormalA]_]) \[CircleTimes] 
           OverBar[(\[FormalB]_) \[CircleTimes] ((\[FormalC]_) \[CircleTimes] 
              ((\[FormalD]_) \[CircleTimes] OverBar[\[FormalD]_]))] <-> 
          (\[FormalAlpha]_) \[CircleTimes] OverBar[
            (\[FormalB]_) \[CircleTimes] ((\[FormalC]_) \[CircleTimes] 
              (\[FormalAlpha]_))], "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"CriticalPairLemma", 8} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          OverBar[\[FormalB] \[CircleTimes] (\[FormalC] \[CircleTimes] 
             \[FormalA])] == OverBar[\[FormalD] \[CircleTimes] 
            (\[FormalAlpha] \[CircleTimes] \[FormalC])] \[CircleTimes] 
          OverBar[\[FormalB] \[CircleTimes] (\[FormalBeta] \[CircleTimes] 
             OverBar[\[FormalD] \[CircleTimes] (\[FormalAlpha] \[CircleTimes] 
                \[FormalBeta])])]], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 7}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] OverBar[
            (\[FormalB]_) \[CircleTimes] ((\[FormalC]_) \[CircleTimes] 
              (\[FormalA]_))] <-> (\[FormalD]_) \[CircleTimes] 
           OverBar[(\[FormalB]_) \[CircleTimes] ((\[FormalC]_) \[CircleTimes] 
              (\[FormalD]_))], "Side" -> 1, "Subpattern" -> 
         (\[FormalC]_) \[CircleTimes] (\[FormalA]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 7}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CircleTimes] 
           OverBar[(\[FormalB]_) \[CircleTimes] ((\[FormalC]_) \[CircleTimes] 
              (\[FormalA]_))] <-> (\[FormalD]_) \[CircleTimes] 
           OverBar[(\[FormalB]_) \[CircleTimes] ((\[FormalC]_) \[CircleTimes] 
              (\[FormalD]_))], "MatchingSide" -> 1, "Position" -> {2, 1, 
         2}|>|>, {"CriticalPairLemma", 9} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CircleTimes] 
           OverBar[\[FormalB]]) \[CircleTimes] OverBar[
           (\[FormalC] \[CircleTimes] OverBar[\[FormalD] \[CircleTimes] (
                \[FormalA] \[CircleTimes] \[FormalC])]) \[CircleTimes] 
            \[FormalD]]], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 
          3}, "Orientation" -> -1, "Rule" -> 
         ((\[FormalA]_) \[CircleTimes] OverBar[\[FormalA]_]) \[CircleTimes] 
           OverBar[(\[FormalB]_) \[CircleTimes] ((\[FormalC]_) \[CircleTimes] 
              OverBar[((\[FormalD]_) \[CircleTimes] 
                 ((\[FormalAlpha]_) \[CircleTimes] OverBar[
                   \[FormalAlpha]_])) \[CircleTimes] 
                ((\[FormalB]_) \[CircleTimes] (\[FormalC]_))])] -> 
          \[FormalD], "Side" -> 1, "Subpattern" -> 
         (\[FormalC]_) \[CircleTimes] OverBar[((\[FormalD]_) \[CircleTimes] 
             ((\[FormalAlpha]_) \[CircleTimes] OverBar[
               \[FormalAlpha]_])) \[CircleTimes] 
            ((\[FormalB]_) \[CircleTimes] (\[FormalC]_))], 
        "MatchingConstruct" -> {"CriticalPairLemma", 6}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleTimes] OverBar[((\[FormalB]_) \[CircleTimes] 
              ((\[FormalC]_) \[CircleTimes] OverBar[
                \[FormalC]_])) \[CircleTimes] (((\[FormalD]_) \[CircleTimes] 
               OverBar[(\[FormalAlpha]_) \[CircleTimes] 
                 ((\[FormalB]_) \[CircleTimes] 
                  (\[FormalD]_))]) \[CircleTimes] (\[FormalA]_))] -> 
          \[FormalAlpha], "MatchingSide" -> 1, "Position" -> {2, 1, 2}|>|>, 
    {"CriticalPairLemma", 10} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CircleTimes] 
           OverBar[\[FormalA]]) \[CircleTimes] OverBar[
           \[FormalB] \[CircleTimes] \[FormalC]] == 
         (\[FormalD] \[CircleTimes] OverBar[\[FormalD]]) \[CircleTimes] 
          OverBar[\[FormalB] \[CircleTimes] \[FormalC]]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 9}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CircleTimes] 
            OverBar[\[FormalA]_]) \[CircleTimes] OverBar[
            ((\[FormalB]_) \[CircleTimes] OverBar[
               (\[FormalC]_) \[CircleTimes] ((\[FormalD]_) \[CircleTimes] 
                 (\[FormalB]_))]) \[CircleTimes] (\[FormalC]_)] -> 
          \[FormalD], "Side" -> 1, "Subpattern" -> 
         (\[FormalB]_) \[CircleTimes] OverBar[(\[FormalC]_) \[CircleTimes] 
            ((\[FormalD]_) \[CircleTimes] (\[FormalB]_))], 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CircleTimes] 
           OverBar[(\[FormalB]_) \[CircleTimes] 
             ((((\[FormalC]_) \[CircleTimes] OverBar[
                 \[FormalC]_]) \[CircleTimes] OverBar[
                (\[FormalD]_) \[CircleTimes] (\[FormalB]_)]) \[CircleTimes] 
              (\[FormalA]_))] -> \[FormalD], "MatchingSide" -> 1, 
        "Position" -> {2, 1, 1}|>|>, {"CriticalPairLemma", 11} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         OverBar[(\[FormalB] \[CircleTimes] OverBar[\[FormalC] \[CircleTimes] 
               (\[FormalD] \[CircleTimes] \[FormalB])]) \[CircleTimes] 
            \[FormalC]] \[CircleTimes] OverBar[
           OverBar[\[FormalA]] \[CircleTimes] \[FormalD]]], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] OverBar[
            (\[FormalB]_) \[CircleTimes] ((((\[FormalC]_) \[CircleTimes] 
                OverBar[\[FormalC]_]) \[CircleTimes] OverBar[
                (\[FormalD]_) \[CircleTimes] (\[FormalB]_)]) \[CircleTimes] 
              (\[FormalA]_))] -> \[FormalD], "Side" -> 1, 
        "Subpattern" -> (((\[FormalC]_) \[CircleTimes] 
            OverBar[\[FormalC]_]) \[CircleTimes] OverBar[
            (\[FormalD]_) \[CircleTimes] (\[FormalB]_)]) \[CircleTimes] 
          (\[FormalA]_), "MatchingConstruct" -> {"CriticalPairLemma", 9}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CircleTimes] OverBar[\[FormalA]_]) \[CircleTimes] 
           OverBar[((\[FormalB]_) \[CircleTimes] OverBar[
               (\[FormalC]_) \[CircleTimes] ((\[FormalD]_) \[CircleTimes] 
                 (\[FormalB]_))]) \[CircleTimes] (\[FormalC]_)] -> 
          \[FormalD], "MatchingSide" -> 1, "Position" -> {2, 1, 2}|>|>, 
    {"CriticalPairLemma", 12} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          OverBar[\[FormalA]] == \[FormalB] \[CircleTimes] 
          OverBar[OverBar[\[FormalC] \[CircleTimes] 
              \[FormalD]] \[CircleTimes] (((\[FormalAlpha] \[CircleTimes] 
               OverBar[\[FormalAlpha]]) \[CircleTimes] OverBar[
               (\[FormalBeta] \[CircleTimes] OverBar[
                  \[FormalBeta]]) \[CircleTimes] OverBar[
                 \[FormalC] \[CircleTimes] \[FormalD]]]) \[CircleTimes] 
             \[FormalB])]], "Proof" -> <|"Construct" -> {"Axiom", 1}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CircleTimes] 
           OverBar[(\[FormalB]_) \[CircleTimes] 
             ((((\[FormalC]_) \[CircleTimes] OverBar[
                 \[FormalC]_]) \[CircleTimes] OverBar[
                (\[FormalD]_) \[CircleTimes] (\[FormalB]_)]) \[CircleTimes] 
              (\[FormalA]_))] -> \[FormalD], "Side" -> 1, 
        "Subpattern" -> (\[FormalD]_) \[CircleTimes] (\[FormalB]_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 10}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((\[FormalA]_) \[CircleTimes] OverBar[\[FormalA]_]) \[CircleTimes] 
           OverBar[(\[FormalB]_) \[CircleTimes] (\[FormalC]_)] <-> 
          ((\[FormalD]_) \[CircleTimes] OverBar[\[FormalD]_]) \[CircleTimes] 
           OverBar[(\[FormalB]_) \[CircleTimes] (\[FormalC]_)], 
        "MatchingSide" -> 1, "Position" -> {2, 1, 2, 1, 2, 1}|>|>, 
    {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          OverBar[\[FormalA]] == \[FormalB] \[CircleTimes] 
          OverBar[\[FormalB]]], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 12}, "Position" -> {}, 
        "Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] OverBar[
            (\[FormalB]_) \[CircleTimes] ((((\[FormalC]_) \[CircleTimes] 
                OverBar[\[FormalC]_]) \[CircleTimes] OverBar[
                (\[FormalD]_) \[CircleTimes] (\[FormalB]_)]) \[CircleTimes] 
              (\[FormalA]_))] -> \[FormalD], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CircleTimes] OverBar[\[FormalA]] == 
           \[FormalB] \[CircleTimes] OverBar[\[FormalB]]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 13} -> 
     <|"Statement" -> HoldForm[\[FormalA] == \[FormalB] \[CircleTimes] 
          OverBar[OverBar[\[FormalA]] \[CircleTimes] 
            ((\[FormalC] \[CircleTimes] OverBar[\[FormalC]]) \[CircleTimes] 
             \[FormalB])]], "Proof" -> <|"Construct" -> {"Axiom", 1}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CircleTimes] 
           OverBar[(\[FormalB]_) \[CircleTimes] 
             ((((\[FormalC]_) \[CircleTimes] OverBar[
                 \[FormalC]_]) \[CircleTimes] OverBar[
                (\[FormalD]_) \[CircleTimes] (\[FormalB]_)]) \[CircleTimes] 
              (\[FormalA]_))] -> \[FormalD], "Side" -> 1, 
        "Subpattern" -> ((\[FormalC]_) \[CircleTimes] 
           OverBar[\[FormalC]_]) \[CircleTimes] 
          OverBar[(\[FormalD]_) \[CircleTimes] (\[FormalB]_)], 
        "MatchingConstruct" -> {"SubstitutionLemma", 1}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleTimes] OverBar[\[FormalA]_] <-> 
          (\[FormalB]_) \[CircleTimes] OverBar[\[FormalB]_], 
        "MatchingSide" -> 1, "Position" -> {2, 1, 2, 1}|>|>, 
    {"CriticalPairLemma", 14} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA]] == 
         \[FormalB] \[CircleTimes] OverBar[((\[FormalC] \[CircleTimes] 
              OverBar[\[FormalC]]) \[CircleTimes] (\[FormalD] \[CircleTimes] 
              OverBar[\[FormalD]])) \[CircleTimes] (\[FormalA] \[CircleTimes] 
             \[FormalB])]], "Proof" -> <|"Construct" -> {"Axiom", 1}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CircleTimes] 
           OverBar[(\[FormalB]_) \[CircleTimes] 
             ((((\[FormalC]_) \[CircleTimes] OverBar[
                 \[FormalC]_]) \[CircleTimes] OverBar[
                (\[FormalD]_) \[CircleTimes] (\[FormalB]_)]) \[CircleTimes] 
              (\[FormalA]_))] -> \[FormalD], "Side" -> 1, 
        "Subpattern" -> ((\[FormalC]_) \[CircleTimes] 
           OverBar[\[FormalC]_]) \[CircleTimes] 
          OverBar[(\[FormalD]_) \[CircleTimes] (\[FormalB]_)], 
        "MatchingConstruct" -> {"CriticalPairLemma", 13}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleTimes] OverBar[
            OverBar[\[FormalB]_] \[CircleTimes] 
             (((\[FormalC]_) \[CircleTimes] OverBar[
                \[FormalC]_]) \[CircleTimes] (\[FormalA]_))] -> \[FormalB], 
        "MatchingSide" -> 1, "Position" -> {2, 1, 2, 1}|>|>, 
    {"CriticalPairLemma", 15} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA] \[CircleTimes] 
           OverBar[\[FormalB] \[CircleTimes] ((\[FormalC] \[CircleTimes] 
               OverBar[\[FormalC]]) \[CircleTimes] \[FormalA])]] == 
         \[FormalB]], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 14}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CircleTimes] 
           OverBar[(((\[FormalB]_) \[CircleTimes] OverBar[
                \[FormalB]_]) \[CircleTimes] ((\[FormalC]_) \[CircleTimes] 
               OverBar[\[FormalC]_])) \[CircleTimes] 
             ((\[FormalD]_) \[CircleTimes] (\[FormalA]_))] -> 
          OverBar[\[FormalD]], "Side" -> 1, "Subpattern" -> 
         (\[FormalA]_) \[CircleTimes] OverBar[
           (((\[FormalB]_) \[CircleTimes] OverBar[
               \[FormalB]_]) \[CircleTimes] ((\[FormalC]_) \[CircleTimes] 
              OverBar[\[FormalC]_])) \[CircleTimes] 
            ((\[FormalD]_) \[CircleTimes] (\[FormalA]_))], 
        "MatchingConstruct" -> {"CriticalPairLemma", 6}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleTimes] OverBar[((\[FormalB]_) \[CircleTimes] 
              ((\[FormalC]_) \[CircleTimes] OverBar[
                \[FormalC]_])) \[CircleTimes] (((\[FormalD]_) \[CircleTimes] 
               OverBar[(\[FormalAlpha]_) \[CircleTimes] 
                 ((\[FormalB]_) \[CircleTimes] 
                  (\[FormalD]_))]) \[CircleTimes] (\[FormalA]_))] -> 
          \[FormalAlpha], "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"CriticalPairLemma", 16} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CircleTimes] 
           OverBar[\[FormalA]]) \[CircleTimes] (\[FormalB] \[CircleTimes] 
           OverBar[\[FormalB]]) == OverBar[OverBar[\[FormalC] \[CircleTimes] 
            OverBar[\[FormalC]]]]], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 15}, "Orientation" -> 1, 
        "Rule" -> OverBar[(\[FormalA]_) \[CircleTimes] 
            OverBar[(\[FormalB]_) \[CircleTimes] 
              (((\[FormalC]_) \[CircleTimes] OverBar[
                 \[FormalC]_]) \[CircleTimes] (\[FormalA]_))]] -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CircleTimes] 
          OverBar[(\[FormalB]_) \[CircleTimes] (((\[FormalC]_) \[CircleTimes] 
              OverBar[\[FormalC]_]) \[CircleTimes] (\[FormalA]_))], 
        "MatchingConstruct" -> {"CriticalPairLemma", 14}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleTimes] OverBar[(((\[FormalB]_) \[CircleTimes] 
               OverBar[\[FormalB]_]) \[CircleTimes] 
              ((\[FormalC]_) \[CircleTimes] OverBar[
                \[FormalC]_])) \[CircleTimes] ((\[FormalD]_) \[CircleTimes] 
              (\[FormalA]_))] -> OverBar[\[FormalD]], "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"CriticalPairLemma", 17} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalB] \[CircleTimes] 
           OverBar[\[FormalB]]) \[CircleTimes] OverBar[
           OverBar[\[FormalA]] \[CircleTimes] OverBar[
             OverBar[\[FormalC] \[CircleTimes] OverBar[\[FormalC]]]]]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 13}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CircleTimes] 
           OverBar[OverBar[\[FormalB]_] \[CircleTimes] 
             (((\[FormalC]_) \[CircleTimes] OverBar[
                \[FormalC]_]) \[CircleTimes] (\[FormalA]_))] -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> ((\[FormalC]_) \[CircleTimes] 
           OverBar[\[FormalC]_]) \[CircleTimes] (\[FormalA]_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 16}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((\[FormalA]_) \[CircleTimes] OverBar[\[FormalA]_]) \[CircleTimes] 
           ((\[FormalB]_) \[CircleTimes] OverBar[\[FormalB]_]) <-> 
          OverBar[OverBar[(\[FormalC]_) \[CircleTimes] 
             OverBar[\[FormalC]_]]], "MatchingSide" -> 1, 
        "Position" -> {2, 1, 2}|>|>, {"CriticalPairLemma", 18} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         OverBar[(\[FormalB] \[CircleTimes] OverBar[
             \[FormalB]]) \[CircleTimes] OverBar[\[FormalA] \[CircleTimes] 
             OverBar[OverBar[\[FormalC] \[CircleTimes] OverBar[
                 \[FormalC]]]]]]], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 15}, "Orientation" -> 1, 
        "Rule" -> OverBar[(\[FormalA]_) \[CircleTimes] 
            OverBar[(\[FormalB]_) \[CircleTimes] 
              (((\[FormalC]_) \[CircleTimes] OverBar[
                 \[FormalC]_]) \[CircleTimes] (\[FormalA]_))]] -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> ((\[FormalC]_) \[CircleTimes] 
           OverBar[\[FormalC]_]) \[CircleTimes] (\[FormalA]_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 16}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((\[FormalA]_) \[CircleTimes] OverBar[\[FormalA]_]) \[CircleTimes] 
           ((\[FormalB]_) \[CircleTimes] OverBar[\[FormalB]_]) <-> 
          OverBar[OverBar[(\[FormalC]_) \[CircleTimes] 
             OverBar[\[FormalC]_]]], "MatchingSide" -> 1, 
        "Position" -> {1, 2, 1, 2}|>|>, {"CriticalPairLemma", 19} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA]] == 
         \[FormalB] \[CircleTimes] OverBar[
           OverBar[OverBar[\[FormalC] \[CircleTimes] OverBar[
                \[FormalC]]]] \[CircleTimes] (\[FormalA] \[CircleTimes] 
             \[FormalB])]], "Proof" -> <|"Construct" -> {"Axiom", 1}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CircleTimes] 
           OverBar[(\[FormalB]_) \[CircleTimes] 
             ((((\[FormalC]_) \[CircleTimes] OverBar[
                 \[FormalC]_]) \[CircleTimes] OverBar[
                (\[FormalD]_) \[CircleTimes] (\[FormalB]_)]) \[CircleTimes] 
              (\[FormalA]_))] -> \[FormalD], "Side" -> 1, 
        "Subpattern" -> ((\[FormalC]_) \[CircleTimes] 
           OverBar[\[FormalC]_]) \[CircleTimes] 
          OverBar[(\[FormalD]_) \[CircleTimes] (\[FormalB]_)], 
        "MatchingConstruct" -> {"CriticalPairLemma", 17}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CircleTimes] OverBar[\[FormalA]_]) \[CircleTimes] 
           OverBar[OverBar[\[FormalB]_] \[CircleTimes] 
             OverBar[OverBar[(\[FormalC]_) \[CircleTimes] OverBar[
                 \[FormalC]_]]]] -> \[FormalB], "MatchingSide" -> 1, 
        "Position" -> {2, 1, 2, 1}|>|>, {"CriticalPairLemma", 20} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA]] == 
         OverBar[\[FormalA]] \[CircleTimes] OverBar[
           OverBar[OverBar[\[FormalB] \[CircleTimes] OverBar[
                \[FormalB]]]] \[CircleTimes] (\[FormalC] \[CircleTimes] 
             OverBar[\[FormalC]])]], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 19}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] OverBar[
            OverBar[OverBar[(\[FormalB]_) \[CircleTimes] OverBar[
                 \[FormalB]_]]] \[CircleTimes] ((\[FormalC]_) \[CircleTimes] 
              (\[FormalA]_))] -> OverBar[\[FormalC]], "Side" -> 1, 
        "Subpattern" -> (\[FormalC]_) \[CircleTimes] (\[FormalA]_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 1}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleTimes] OverBar[\[FormalA]_] <-> 
          (\[FormalB]_) \[CircleTimes] OverBar[\[FormalB]_], 
        "MatchingSide" -> 1, "Position" -> {2, 1, 2}|>|>, 
    {"CriticalPairLemma", 21} -> 
     <|"Statement" -> HoldForm[OverBar[(\[FormalA] \[CircleTimes] 
            OverBar[\[FormalA]]) \[CircleTimes] OverBar[
            \[FormalB] \[CircleTimes] OverBar[OverBar[
               \[FormalC] \[CircleTimes] OverBar[\[FormalC]]]]]] == 
         \[FormalB] \[CircleTimes] OverBar[
           OverBar[OverBar[\[FormalD] \[CircleTimes] OverBar[
                \[FormalD]]]] \[CircleTimes] (\[FormalAlpha] \[CircleTimes] 
             OverBar[\[FormalAlpha]])]], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 20}, "Orientation" -> -1, 
        "Rule" -> OverBar[\[FormalA]_] \[CircleTimes] 
           OverBar[OverBar[OverBar[(\[FormalB]_) \[CircleTimes] 
                OverBar[\[FormalB]_]]] \[CircleTimes] 
             ((\[FormalC]_) \[CircleTimes] OverBar[\[FormalC]_])] -> 
          OverBar[\[FormalA]], "Side" -> 1, "Subpattern" -> 
         OverBar[\[FormalA]_], "MatchingConstruct" -> {"CriticalPairLemma", 
          18}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         OverBar[((\[FormalA]_) \[CircleTimes] OverBar[
              \[FormalA]_]) \[CircleTimes] OverBar[
             (\[FormalB]_) \[CircleTimes] OverBar[OverBar[
                (\[FormalC]_) \[CircleTimes] OverBar[\[FormalC]_]]]]] -> 
          \[FormalB], "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[\[FormalA] == \[FormalA] \[CircleTimes] 
          OverBar[OverBar[OverBar[\[FormalB] \[CircleTimes] OverBar[
                \[FormalB]]]] \[CircleTimes] (\[FormalC] \[CircleTimes] 
             OverBar[\[FormalC]])]], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 21}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 18}, "Orientation" -> -1, 
        "Rule" -> OverBar[((\[FormalA]_) \[CircleTimes] 
             OverBar[\[FormalA]_]) \[CircleTimes] OverBar[
             (\[FormalB]_) \[CircleTimes] OverBar[OverBar[
                (\[FormalC]_) \[CircleTimes] OverBar[\[FormalC]_]]]]] -> 
          \[FormalB], "OutputExpression" -> HoldForm[\[FormalA] == 
           \[FormalA] \[CircleTimes] OverBar[OverBar[OverBar[
                \[FormalB] \[CircleTimes] OverBar[
                  \[FormalB]]]] \[CircleTimes] (\[FormalC] \[CircleTimes] 
               OverBar[\[FormalC]])]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 22} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[\[FormalA] \[CircleTimes] 
             OverBar[\[FormalA]]]] \[CircleTimes] (\[FormalB] \[CircleTimes] 
           OverBar[\[FormalB]]) == \[FormalC] \[CircleTimes] 
          OverBar[\[FormalC]]], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 2}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] OverBar[
            OverBar[OverBar[(\[FormalB]_) \[CircleTimes] OverBar[
                 \[FormalB]_]]] \[CircleTimes] ((\[FormalC]_) \[CircleTimes] 
              OverBar[\[FormalC]_])] -> \[FormalA], "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CircleTimes] 
          OverBar[OverBar[OverBar[(\[FormalB]_) \[CircleTimes] OverBar[
                \[FormalB]_]]] \[CircleTimes] ((\[FormalC]_) \[CircleTimes] 
             OverBar[\[FormalC]_])], "MatchingConstruct" -> 
         {"SubstitutionLemma", 1}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CircleTimes] 
           OverBar[\[FormalA]_] <-> (\[FormalB]_) \[CircleTimes] 
           OverBar[\[FormalB]_], "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"CriticalPairLemma", 23} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         OverBar[(\[FormalB] \[CircleTimes] OverBar[
              \[FormalB]]) \[CircleTimes] OverBar[\[FormalA] \[CircleTimes] 
              OverBar[OverBar[\[FormalC] \[CircleTimes] OverBar[
                  \[FormalC]]]]]] \[CircleTimes] 
          OverBar[\[FormalD] \[CircleTimes] OverBar[\[FormalD]]]], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] OverBar[
            (\[FormalB]_) \[CircleTimes] ((((\[FormalC]_) \[CircleTimes] 
                OverBar[\[FormalC]_]) \[CircleTimes] OverBar[
                (\[FormalD]_) \[CircleTimes] (\[FormalB]_)]) \[CircleTimes] 
              (\[FormalA]_))] -> \[FormalD], "Side" -> 1, 
        "Subpattern" -> (\[FormalB]_) \[CircleTimes] 
          ((((\[FormalC]_) \[CircleTimes] OverBar[
              \[FormalC]_]) \[CircleTimes] OverBar[
             (\[FormalD]_) \[CircleTimes] (\[FormalB]_)]) \[CircleTimes] 
           (\[FormalA]_)), "MatchingConstruct" -> {"CriticalPairLemma", 22}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         OverBar[OverBar[(\[FormalA]_) \[CircleTimes] OverBar[
               \[FormalA]_]]] \[CircleTimes] ((\[FormalB]_) \[CircleTimes] 
            OverBar[\[FormalB]_]) <-> (\[FormalC]_) \[CircleTimes] 
           OverBar[\[FormalC]_], "MatchingSide" -> 1, "Position" -> {2, 
         1}|>|>, {"SubstitutionLemma", 3} -> 
     <|"Statement" -> HoldForm[\[FormalA] == \[FormalA] \[CircleTimes] 
          OverBar[\[FormalB] \[CircleTimes] OverBar[\[FormalB]]]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 23}, "Position" -> {1}, 
        "Construct" -> {"CriticalPairLemma", 18}, "Orientation" -> -1, 
        "Rule" -> OverBar[((\[FormalA]_) \[CircleTimes] 
             OverBar[\[FormalA]_]) \[CircleTimes] OverBar[
             (\[FormalB]_) \[CircleTimes] OverBar[OverBar[
                (\[FormalC]_) \[CircleTimes] OverBar[\[FormalC]_]]]]] -> 
          \[FormalB], "OutputExpression" -> HoldForm[\[FormalA] == 
           \[FormalA] \[CircleTimes] OverBar[\[FormalB] \[CircleTimes] 
              OverBar[\[FormalB]]]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 24} -> 
     <|"Statement" -> HoldForm[OverBar[(\[FormalA] \[CircleTimes] 
            OverBar[\[FormalB] \[CircleTimes] (OverBar[OverBar[
                 \[FormalC]]] \[CircleTimes] \[FormalA])]) \[CircleTimes] 
           \[FormalB]] == \[FormalC]], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 3}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] OverBar[
            (\[FormalB]_) \[CircleTimes] OverBar[\[FormalB]_]] -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CircleTimes] 
          OverBar[(\[FormalB]_) \[CircleTimes] OverBar[\[FormalB]_]], 
        "MatchingConstruct" -> {"CriticalPairLemma", 11}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         OverBar[((\[FormalA]_) \[CircleTimes] OverBar[
               (\[FormalB]_) \[CircleTimes] ((\[FormalC]_) \[CircleTimes] 
                 (\[FormalA]_))]) \[CircleTimes] 
             (\[FormalB]_)] \[CircleTimes] OverBar[
            OverBar[\[FormalD]_] \[CircleTimes] (\[FormalC]_)] -> \[FormalD], 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"CriticalPairLemma", 25} -> 
     <|"Statement" -> HoldForm[\[FormalA] == \[FormalB] \[CircleTimes] 
          OverBar[OverBar[\[FormalA]] \[CircleTimes] 
            OverBar[OverBar[\[FormalB]]]]], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 11}, "Orientation" -> -1, 
        "Rule" -> OverBar[((\[FormalA]_) \[CircleTimes] 
              OverBar[(\[FormalB]_) \[CircleTimes] 
                ((\[FormalC]_) \[CircleTimes] (\[FormalA]_))]) \[CircleTimes] 
             (\[FormalB]_)] \[CircleTimes] OverBar[
            OverBar[\[FormalD]_] \[CircleTimes] (\[FormalC]_)] -> \[FormalD], 
        "Side" -> 1, "Subpattern" -> OverBar[((\[FormalA]_) \[CircleTimes] 
            OverBar[(\[FormalB]_) \[CircleTimes] 
              ((\[FormalC]_) \[CircleTimes] (\[FormalA]_))]) \[CircleTimes] 
           (\[FormalB]_)], "MatchingConstruct" -> {"CriticalPairLemma", 24}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         OverBar[((\[FormalA]_) \[CircleTimes] OverBar[
              (\[FormalB]_) \[CircleTimes] (OverBar[OverBar[
                  \[FormalC]_]] \[CircleTimes] 
                (\[FormalA]_))]) \[CircleTimes] (\[FormalB]_)] -> \[FormalC], 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 26} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[\[FormalA]]] == 
         (\[FormalB] \[CircleTimes] OverBar[\[FormalB]]) \[CircleTimes] 
          \[FormalA]], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 9}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CircleTimes] 
            OverBar[\[FormalA]_]) \[CircleTimes] OverBar[
            ((\[FormalB]_) \[CircleTimes] OverBar[
               (\[FormalC]_) \[CircleTimes] ((\[FormalD]_) \[CircleTimes] 
                 (\[FormalB]_))]) \[CircleTimes] (\[FormalC]_)] -> 
          \[FormalD], "Side" -> 1, "Subpattern" -> 
         OverBar[((\[FormalB]_) \[CircleTimes] OverBar[
             (\[FormalC]_) \[CircleTimes] ((\[FormalD]_) \[CircleTimes] (
                \[FormalB]_))]) \[CircleTimes] (\[FormalC]_)], 
        "MatchingConstruct" -> {"CriticalPairLemma", 24}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         OverBar[((\[FormalA]_) \[CircleTimes] OverBar[
              (\[FormalB]_) \[CircleTimes] (OverBar[OverBar[
                  \[FormalC]_]] \[CircleTimes] 
                (\[FormalA]_))]) \[CircleTimes] (\[FormalB]_)] -> \[FormalC], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 27} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[\[FormalA]]] == 
         OverBar[OverBar[OverBar[\[FormalB] \[CircleTimes] 
              OverBar[\[FormalB]]]]] \[CircleTimes] \[FormalA]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 26}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CircleTimes] 
            OverBar[\[FormalA]_]) \[CircleTimes] (\[FormalB]_) -> 
          OverBar[OverBar[\[FormalB]]], "Side" -> 1, "Subpattern" -> 
         (\[FormalA]_) \[CircleTimes] OverBar[\[FormalA]_], 
        "MatchingConstruct" -> {"CriticalPairLemma", 26}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CircleTimes] OverBar[\[FormalA]_]) \[CircleTimes] 
           (\[FormalB]_) -> OverBar[OverBar[\[FormalB]]], 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 28} -> 
     <|"Statement" -> HoldForm[\[FormalA] == \[FormalA] \[CircleTimes] 
          OverBar[OverBar[OverBar[OverBar[\[FormalB] \[CircleTimes] OverBar[
                \[FormalB]]]]]]], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 3}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] OverBar[
            (\[FormalB]_) \[CircleTimes] OverBar[\[FormalB]_]] -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CircleTimes] 
          OverBar[\[FormalB]_], "MatchingConstruct" -> {"CriticalPairLemma", 
          26}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CircleTimes] OverBar[\[FormalA]_]) \[CircleTimes] 
           (\[FormalB]_) -> OverBar[OverBar[\[FormalB]]], 
        "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"CriticalPairLemma", 29} -> 
     <|"Statement" -> HoldForm[\[FormalA] == \[FormalB] \[CircleTimes] 
          OverBar[OverBar[OverBar[\[FormalC] \[CircleTimes] OverBar[
                \[FormalC]]]] \[CircleTimes] ((\[FormalD] \[CircleTimes] 
              OverBar[\[FormalA] \[CircleTimes] 
                ((\[FormalAlpha] \[CircleTimes] OverBar[
                   \[FormalAlpha]]) \[CircleTimes] 
                 \[FormalD])]) \[CircleTimes] \[FormalB])]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 6}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CircleTimes] 
           OverBar[((\[FormalB]_) \[CircleTimes] 
              ((\[FormalC]_) \[CircleTimes] OverBar[
                \[FormalC]_])) \[CircleTimes] (((\[FormalD]_) \[CircleTimes] 
               OverBar[(\[FormalAlpha]_) \[CircleTimes] 
                 ((\[FormalB]_) \[CircleTimes] 
                  (\[FormalD]_))]) \[CircleTimes] (\[FormalA]_))] -> 
          \[FormalAlpha], "Side" -> 1, "Subpattern" -> 
         (\[FormalB]_) \[CircleTimes] ((\[FormalC]_) \[CircleTimes] 
           OverBar[\[FormalC]_]), "MatchingConstruct" -> 
         {"CriticalPairLemma", 26}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CircleTimes] 
            OverBar[\[FormalA]_]) \[CircleTimes] (\[FormalB]_) -> 
          OverBar[OverBar[\[FormalB]]], "MatchingSide" -> 1, 
        "Position" -> {2, 1, 1}|>|>, {"SubstitutionLemma", 4} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         OverBar[\[FormalB] \[CircleTimes] OverBar[\[FormalA] \[CircleTimes] 
             ((\[FormalC] \[CircleTimes] OverBar[\[FormalC]]) \[CircleTimes] 
              \[FormalB])]]], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 29}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 19}, "Orientation" -> -1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] OverBar[
            OverBar[OverBar[(\[FormalB]_) \[CircleTimes] OverBar[
                 \[FormalB]_]]] \[CircleTimes] ((\[FormalC]_) \[CircleTimes] 
              (\[FormalA]_))] -> OverBar[\[FormalC]], "OutputExpression" -> 
         HoldForm[\[FormalA] == OverBar[\[FormalB] \[CircleTimes] 
             OverBar[\[FormalA] \[CircleTimes] ((\[FormalC] \[CircleTimes] 
                 OverBar[\[FormalC]]) \[CircleTimes] \[FormalB])]]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 5} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         OverBar[\[FormalB] \[CircleTimes] OverBar[\[FormalA] \[CircleTimes] 
             OverBar[OverBar[\[FormalB]]]]]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 4}, 
        "Position" -> {1, 2, 1, 2}, "Construct" -> {"CriticalPairLemma", 26}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CircleTimes] 
            OverBar[\[FormalA]_]) \[CircleTimes] (\[FormalB]_) -> 
          OverBar[OverBar[\[FormalB]]], "OutputExpression" -> 
         HoldForm[\[FormalA] == OverBar[\[FormalB] \[CircleTimes] 
             OverBar[\[FormalA] \[CircleTimes] OverBar[OverBar[
                 \[FormalB]]]]]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 30} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         OverBar[OverBar[\[FormalB] \[CircleTimes] OverBar[
              \[FormalB]]]] \[CircleTimes] OverBar[OverBar[\[FormalA]]]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 25}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CircleTimes] 
           OverBar[OverBar[\[FormalB]_] \[CircleTimes] 
             OverBar[OverBar[\[FormalA]_]]] -> \[FormalB], "Side" -> 1, 
        "Subpattern" -> OverBar[\[FormalB]_] \[CircleTimes] 
          OverBar[OverBar[\[FormalA]_]], "MatchingConstruct" -> 
         {"CriticalPairLemma", 28}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (\[FormalA]_) \[CircleTimes] 
           OverBar[OverBar[OverBar[OverBar[(\[FormalB]_) \[CircleTimes] 
                OverBar[\[FormalB]_]]]]] -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {2, 1}|>|>, {"CriticalPairLemma", 31} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         OverBar[OverBar[OverBar[\[FormalB] \[CircleTimes] 
              OverBar[\[FormalB]]]]] \[CircleTimes] 
          OverBar[OverBar[\[FormalA]]]], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 30}, "Orientation" -> -1, 
        "Rule" -> OverBar[OverBar[(\[FormalA]_) \[CircleTimes] 
              OverBar[\[FormalA]_]]] \[CircleTimes] 
           OverBar[OverBar[\[FormalB]_]] -> \[FormalB], "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CircleTimes] OverBar[\[FormalA]_], 
        "MatchingConstruct" -> {"CriticalPairLemma", 30}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         OverBar[OverBar[(\[FormalA]_) \[CircleTimes] OverBar[
               \[FormalA]_]]] \[CircleTimes] OverBar[OverBar[\[FormalB]_]] -> 
          \[FormalB], "MatchingSide" -> 1, "Position" -> {1, 1, 1}|>|>, 
    {"SubstitutionLemma", 6} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         OverBar[OverBar[OverBar[OverBar[\[FormalA]]]]]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 31}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 27}, "Orientation" -> -1, 
        "Rule" -> OverBar[OverBar[OverBar[(\[FormalA]_) \[CircleTimes] 
               OverBar[\[FormalA]_]]]] \[CircleTimes] (\[FormalB]_) -> 
          OverBar[OverBar[\[FormalB]]], "OutputExpression" -> 
         HoldForm[\[FormalA] == OverBar[OverBar[OverBar[
              OverBar[\[FormalA]]]]]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, {"SubstitutionLemma", 7} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          (\[FormalB] \[CircleTimes] OverBar[\[FormalB]]) == \[FormalA]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 28}, "Position" -> {2}, 
        "Construct" -> {"SubstitutionLemma", 6}, "Orientation" -> -1, 
        "Rule" -> OverBar[OverBar[OverBar[OverBar[\[FormalA]_]]]] -> 
          \[FormalA], "OutputExpression" -> HoldForm[
          \[FormalA] \[CircleTimes] (\[FormalB] \[CircleTimes] 
             OverBar[\[FormalB]]) == \[FormalA]], "ConstructSide" -> 1, 
        "InputOrientation" -> -1, "Side" -> 1|>|>, 
    {"CriticalPairLemma", 32} -> 
     <|"Statement" -> HoldForm[\[FormalA] == \[FormalA] \[CircleTimes] 
          (OverBar[OverBar[OverBar[\[FormalB]]]] \[CircleTimes] \[FormalB])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 7}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CircleTimes] 
           ((\[FormalB]_) \[CircleTimes] OverBar[\[FormalB]_]) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> OverBar[\[FormalB]_], 
        "MatchingConstruct" -> {"SubstitutionLemma", 6}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         OverBar[OverBar[OverBar[OverBar[\[FormalA]_]]]] -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"CriticalPairLemma", 33} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA]] == 
         OverBar[(\[FormalA] \[CircleTimes] OverBar[
             \[FormalB]]) \[CircleTimes] \[FormalB]]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 24}, 
        "Orientation" -> 1, "Rule" -> 
         OverBar[((\[FormalA]_) \[CircleTimes] OverBar[
              (\[FormalB]_) \[CircleTimes] (OverBar[OverBar[
                  \[FormalC]_]] \[CircleTimes] 
                (\[FormalA]_))]) \[CircleTimes] (\[FormalB]_)] -> \[FormalC], 
        "Side" -> 1, "Subpattern" -> (\[FormalB]_) \[CircleTimes] 
          (OverBar[OverBar[\[FormalC]_]] \[CircleTimes] (\[FormalA]_)), 
        "MatchingConstruct" -> {"CriticalPairLemma", 32}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleTimes] (OverBar[OverBar[OverBar[
               \[FormalB]_]]] \[CircleTimes] (\[FormalB]_)) -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {1, 1, 2, 1}|>|>, 
    {"CriticalPairLemma", 34} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA]] == 
         OverBar[OverBar[OverBar[\[FormalA]]]]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 33}, 
        "Orientation" -> -1, "Rule" -> 
         OverBar[((\[FormalA]_) \[CircleTimes] OverBar[
              \[FormalB]_]) \[CircleTimes] (\[FormalB]_)] -> 
          OverBar[\[FormalA]], "Side" -> 1, "Subpattern" -> 
         ((\[FormalA]_) \[CircleTimes] OverBar[\[FormalB]_]) \[CircleTimes] 
          (\[FormalB]_), "MatchingConstruct" -> {"CriticalPairLemma", 26}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((\[FormalA]_) \[CircleTimes] OverBar[\[FormalA]_]) \[CircleTimes] 
           (\[FormalB]_) -> OverBar[OverBar[\[FormalB]]], 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 35} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CircleTimes] 
           OverBar[\[FormalB]]) \[CircleTimes] \[FormalB] == 
         OverBar[(\[FormalC] \[CircleTimes] OverBar[\[FormalD] \[CircleTimes] 
               (\[FormalAlpha] \[CircleTimes] \[FormalC])]) \[CircleTimes] 
            \[FormalD]] \[CircleTimes] OverBar[
           OverBar[\[FormalA]] \[CircleTimes] \[FormalAlpha]]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 11}, 
        "Orientation" -> -1, "Rule" -> 
         OverBar[((\[FormalA]_) \[CircleTimes] OverBar[
               (\[FormalB]_) \[CircleTimes] ((\[FormalC]_) \[CircleTimes] 
                 (\[FormalA]_))]) \[CircleTimes] 
             (\[FormalB]_)] \[CircleTimes] OverBar[
            OverBar[\[FormalD]_] \[CircleTimes] (\[FormalC]_)] -> \[FormalD], 
        "Side" -> 1, "Subpattern" -> OverBar[\[FormalD]_], 
        "MatchingConstruct" -> {"CriticalPairLemma", 33}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         OverBar[((\[FormalA]_) \[CircleTimes] OverBar[
              \[FormalB]_]) \[CircleTimes] (\[FormalB]_)] -> 
          OverBar[\[FormalA]], "MatchingSide" -> 1, "Position" -> {2, 1, 
         1}|>|>, {"SubstitutionLemma", 8} -> 
     <|"Statement" -> HoldForm[(\[FormalA] \[CircleTimes] 
           OverBar[\[FormalB]]) \[CircleTimes] \[FormalB] == \[FormalA]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 35}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 11}, "Orientation" -> -1, 
        "Rule" -> OverBar[((\[FormalA]_) \[CircleTimes] 
              OverBar[(\[FormalB]_) \[CircleTimes] 
                ((\[FormalC]_) \[CircleTimes] (\[FormalA]_))]) \[CircleTimes] 
             (\[FormalB]_)] \[CircleTimes] OverBar[
            OverBar[\[FormalD]_] \[CircleTimes] (\[FormalC]_)] -> \[FormalD], 
        "OutputExpression" -> HoldForm[(\[FormalA] \[CircleTimes] 
             OverBar[\[FormalB]]) \[CircleTimes] \[FormalB] == \[FormalA]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"CriticalPairLemma", 36} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA] \[CircleTimes] 
           OverBar[\[FormalB] \[CircleTimes] OverBar[OverBar[
               \[FormalA]]]]] == OverBar[OverBar[\[FormalB]]]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 34}, 
        "Orientation" -> -1, "Rule" -> 
         OverBar[OverBar[OverBar[\[FormalA]_]]] -> OverBar[\[FormalA]], 
        "Side" -> 1, "Subpattern" -> OverBar[\[FormalA]_], 
        "MatchingConstruct" -> {"SubstitutionLemma", 5}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         OverBar[(\[FormalA]_) \[CircleTimes] OverBar[
             (\[FormalB]_) \[CircleTimes] OverBar[OverBar[\[FormalA]_]]]] -> 
          \[FormalB], "MatchingSide" -> 1, "Position" -> {1, 1}|>|>, 
    {"SubstitutionLemma", 9} -> 
     <|"Statement" -> HoldForm[\[FormalA] == OverBar[OverBar[\[FormalA]]]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 36}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 5}, "Orientation" -> -1, 
        "Rule" -> OverBar[(\[FormalA]_) \[CircleTimes] 
            OverBar[(\[FormalB]_) \[CircleTimes] OverBar[OverBar[
                \[FormalA]_]]]] -> \[FormalB], "OutputExpression" -> 
         HoldForm[\[FormalA] == OverBar[OverBar[\[FormalA]]]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 10} -> 
     <|"Statement" -> HoldForm[((\[FormalA]_) \[CircleTimes] 
           OverBar[\[FormalA]_]) \[CircleTimes] (\[FormalB]_) -> \[FormalB]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 26}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 9}, "Orientation" -> -1, 
        "Rule" -> OverBar[OverBar[\[FormalA]_]] -> \[FormalA], 
        "OutputExpression" -> HoldForm[((\[FormalA]_) \[CircleTimes] 
             OverBar[\[FormalA]_]) \[CircleTimes] (\[FormalB]_) -> 
           \[FormalB]], "ConstructSide" -> 1, "InputOrientation" -> -1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 11} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          OverBar[(\[FormalB] \[CircleTimes] OverBar[
              \[FormalB]]) \[CircleTimes] (\[FormalC] \[CircleTimes] 
             \[FormalA])] == OverBar[\[FormalC]]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 19}, 
        "Position" -> {2, 1, 1}, "Construct" -> {"SubstitutionLemma", 9}, 
        "Orientation" -> -1, "Rule" -> OverBar[OverBar[\[FormalA]_]] -> 
          \[FormalA], "OutputExpression" -> HoldForm[
          \[FormalA] \[CircleTimes] OverBar[(\[FormalB] \[CircleTimes] 
               OverBar[\[FormalB]]) \[CircleTimes] (\[FormalC] \[CircleTimes] 
               \[FormalA])] == OverBar[\[FormalC]]], "ConstructSide" -> 1, 
        "InputOrientation" -> -1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 12} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          OverBar[\[FormalB] \[CircleTimes] \[FormalA]] == 
         OverBar[\[FormalB]]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 11}, "Position" -> {2, 1}, 
        "Construct" -> {"SubstitutionLemma", 10}, "Orientation" -> 1, 
        "Rule" -> ((\[FormalA]_) \[CircleTimes] OverBar[
             \[FormalA]_]) \[CircleTimes] (\[FormalB]_) -> \[FormalB], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CircleTimes] 
            OverBar[\[FormalB] \[CircleTimes] \[FormalA]] == 
           OverBar[\[FormalB]]], "ConstructSide" -> 1, "InputOrientation" -> 
         1, "Side" -> 1|>|>, {"CriticalPairLemma", 37} -> 
     <|"Statement" -> HoldForm[\[FormalA] == (\[FormalA] \[CircleTimes] 
           \[FormalB]) \[CircleTimes] OverBar[\[FormalB]]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 8}, 
        "Orientation" -> 1, "Rule" -> ((\[FormalA]_) \[CircleTimes] 
            OverBar[\[FormalB]_]) \[CircleTimes] (\[FormalB]_) -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> OverBar[\[FormalB]_], 
        "MatchingConstruct" -> {"SubstitutionLemma", 9}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         OverBar[OverBar[\[FormalA]_]] -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {1, 2}|>|>, {"CriticalPairLemma", 38} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA] \[CircleTimes] 
           \[FormalB]] == OverBar[\[FormalB]] \[CircleTimes] 
          OverBar[\[FormalA]]], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 12}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] OverBar[
            (\[FormalB]_) \[CircleTimes] (\[FormalA]_)] -> 
          OverBar[\[FormalB]], "Side" -> 1, "Subpattern" -> 
         (\[FormalB]_) \[CircleTimes] (\[FormalA]_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 37}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((\[FormalA]_) \[CircleTimes] 
            (\[FormalB]_)) \[CircleTimes] OverBar[\[FormalB]_] -> \[FormalA], 
        "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"CriticalPairLemma", 39} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         OverBar[\[FormalB]] \[CircleTimes] OverBar[
           OverBar[\[FormalB] \[CircleTimes] \[FormalA]]]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 37}, 
        "Orientation" -> -1, "Rule" -> ((\[FormalA]_) \[CircleTimes] 
            (\[FormalB]_)) \[CircleTimes] OverBar[\[FormalB]_] -> \[FormalA], 
        "Side" -> 1, "Subpattern" -> (\[FormalA]_) \[CircleTimes] 
          (\[FormalB]_), "MatchingConstruct" -> {"SubstitutionLemma", 12}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleTimes] OverBar[(\[FormalB]_) \[CircleTimes] 
             (\[FormalA]_)] -> OverBar[\[FormalB]], "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 13} -> 
     <|"Statement" -> HoldForm[\[FormalA] == 
         OverBar[\[FormalB]] \[CircleTimes] (\[FormalB] \[CircleTimes] 
           \[FormalA])], "Proof" -> <|"Input" -> {"CriticalPairLemma", 39}, 
        "Position" -> {2}, "Construct" -> {"SubstitutionLemma", 9}, 
        "Orientation" -> -1, "Rule" -> OverBar[OverBar[\[FormalA]_]] -> 
          \[FormalA], "OutputExpression" -> HoldForm[\[FormalA] == 
           OverBar[\[FormalB]] \[CircleTimes] (\[FormalB] \[CircleTimes] 
             \[FormalA])], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 40} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA]] == 
         OverBar[\[FormalB] \[CircleTimes] \[FormalA]] \[CircleTimes] 
          OverBar[OverBar[\[FormalB]]]], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 12}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] OverBar[
            (\[FormalB]_) \[CircleTimes] (\[FormalA]_)] -> 
          OverBar[\[FormalB]], "Side" -> 1, "Subpattern" -> 
         (\[FormalB]_) \[CircleTimes] (\[FormalA]_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 12}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (\[FormalA]_) \[CircleTimes] 
           OverBar[(\[FormalB]_) \[CircleTimes] (\[FormalA]_)] -> 
          OverBar[\[FormalB]], "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"SubstitutionLemma", 14} -> 
     <|"Statement" -> HoldForm[OverBar[\[FormalA]] == 
         OverBar[\[FormalB] \[CircleTimes] \[FormalA]] \[CircleTimes] 
          \[FormalB]], "Proof" -> <|"Input" -> {"CriticalPairLemma", 40}, 
        "Position" -> {2}, "Construct" -> {"SubstitutionLemma", 9}, 
        "Orientation" -> -1, "Rule" -> OverBar[OverBar[\[FormalA]_]] -> 
          \[FormalA], "OutputExpression" -> HoldForm[OverBar[\[FormalA]] == 
           OverBar[\[FormalB] \[CircleTimes] \[FormalA]] \[CircleTimes] 
            \[FormalB]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 41} -> 
     <|"Statement" -> HoldForm[\[FormalA] == \[FormalB] \[CircleTimes] 
          (OverBar[\[FormalB]] \[CircleTimes] \[FormalA])], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 13}, 
        "Orientation" -> -1, "Rule" -> OverBar[\[FormalA]_] \[CircleTimes] 
           ((\[FormalA]_) \[CircleTimes] (\[FormalB]_)) -> \[FormalB], 
        "Side" -> 1, "Subpattern" -> OverBar[\[FormalA]_], 
        "MatchingConstruct" -> {"SubstitutionLemma", 9}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         OverBar[OverBar[\[FormalA]_]] -> \[FormalA], "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 15} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          OverBar[\[FormalB] \[CircleTimes] (\[FormalC] \[CircleTimes] 
             \[FormalA])] == OverBar[(\[FormalB] \[CircleTimes] 
            (\[FormalD] \[CircleTimes] OverBar[\[FormalAlpha] \[CircleTimes] (
                \[FormalBeta] \[CircleTimes] \[FormalD])])) \[CircleTimes] 
           (\[FormalAlpha] \[CircleTimes] (\[FormalBeta] \[CircleTimes] 
             \[FormalC]))]], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 8}, "Position" -> {}, 
        "Construct" -> {"CriticalPairLemma", 38}, "Orientation" -> -1, 
        "Rule" -> OverBar[\[FormalA]_] \[CircleTimes] OverBar[\[FormalB]_] -> 
          OverBar[\[FormalB] \[CircleTimes] \[FormalA]], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CircleTimes] 
            OverBar[\[FormalB] \[CircleTimes] (\[FormalC] \[CircleTimes] 
               \[FormalA])] == OverBar[(\[FormalB] \[CircleTimes] 
              (\[FormalD] \[CircleTimes] OverBar[
                \[FormalAlpha] \[CircleTimes] (\[FormalBeta] \[CircleTimes] 
                  \[FormalD])])) \[CircleTimes] 
             (\[FormalAlpha] \[CircleTimes] (\[FormalBeta] \[CircleTimes] 
               \[FormalC]))]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 42} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          OverBar[\[FormalB] \[CircleTimes] (\[FormalC] \[CircleTimes] 
             \[FormalA])] == OverBar[(\[FormalB] \[CircleTimes] 
            (\[FormalD] \[CircleTimes] OverBar[\[FormalD]])) \[CircleTimes] 
           (\[FormalAlpha] \[CircleTimes] 
            (OverBar[\[FormalAlpha]] \[CircleTimes] \[FormalC]))]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 15}, 
        "Orientation" -> 1, "Rule" -> (\[FormalA]_) \[CircleTimes] 
           OverBar[(\[FormalB]_) \[CircleTimes] ((\[FormalC]_) \[CircleTimes] 
              (\[FormalA]_))] <-> OverBar[((\[FormalB]_) \[CircleTimes] 
             ((\[FormalD]_) \[CircleTimes] OverBar[
               (\[FormalAlpha]_) \[CircleTimes] 
                ((\[FormalBeta]_) \[CircleTimes] 
                 (\[FormalD]_))])) \[CircleTimes] 
            ((\[FormalAlpha]_) \[CircleTimes] 
             ((\[FormalBeta]_) \[CircleTimes] (\[FormalC]_)))], "Side" -> 2, 
        "Subpattern" -> (\[FormalC]_) \[CircleTimes] 
          ((\[FormalD]_) \[CircleTimes] (\[FormalB]_)), 
        "MatchingConstruct" -> {"CriticalPairLemma", 41}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleTimes] (OverBar[\[FormalA]_] \[CircleTimes] 
            (\[FormalB]_)) -> \[FormalB], "MatchingSide" -> 1, 
        "Position" -> {1, 1, 2, 2, 1}|>|>, {"SubstitutionLemma", 16} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          OverBar[\[FormalB] \[CircleTimes] (\[FormalC] \[CircleTimes] 
             \[FormalA])] == OverBar[\[FormalB] \[CircleTimes] 
           (\[FormalD] \[CircleTimes] (OverBar[\[FormalD]] \[CircleTimes] 
             \[FormalC]))]], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 42}, "Position" -> {1, 1}, 
        "Construct" -> {"SubstitutionLemma", 7}, "Orientation" -> 1, 
        "Rule" -> (\[FormalA]_) \[CircleTimes] ((\[FormalB]_) \[CircleTimes] 
            OverBar[\[FormalB]_]) -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalA] \[CircleTimes] OverBar[\[FormalB] \[CircleTimes] 
              (\[FormalC] \[CircleTimes] \[FormalA])] == 
           OverBar[\[FormalB] \[CircleTimes] (\[FormalD] \[CircleTimes] 
              (OverBar[\[FormalD]] \[CircleTimes] \[FormalC]))]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 17} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          OverBar[\[FormalB] \[CircleTimes] (\[FormalC] \[CircleTimes] 
             \[FormalA])] == OverBar[\[FormalB] \[CircleTimes] \[FormalC]]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 16}, 
        "Position" -> {1, 2}, "Construct" -> {"CriticalPairLemma", 41}, 
        "Orientation" -> -1, "Rule" -> (\[FormalA]_) \[CircleTimes] 
           (OverBar[\[FormalA]_] \[CircleTimes] (\[FormalB]_)) -> \[FormalB], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CircleTimes] 
            OverBar[\[FormalB] \[CircleTimes] (\[FormalC] \[CircleTimes] 
               \[FormalA])] == OverBar[\[FormalB] \[CircleTimes] 
             \[FormalC]]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"CriticalPairLemma", 43} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[\[FormalA] \[CircleTimes] 
            (\[FormalB] \[CircleTimes] \[FormalC])]] == 
         OverBar[OverBar[\[FormalA] \[CircleTimes] 
             \[FormalB]]] \[CircleTimes] \[FormalC]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 14}, 
        "Orientation" -> -1, "Rule" -> 
         OverBar[(\[FormalA]_) \[CircleTimes] (\[FormalB]_)] \[CircleTimes] 
           (\[FormalA]_) -> OverBar[\[FormalB]], "Side" -> 1, 
        "Subpattern" -> (\[FormalA]_) \[CircleTimes] (\[FormalB]_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 17}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (\[FormalA]_) \[CircleTimes] OverBar[(\[FormalB]_) \[CircleTimes] 
             ((\[FormalC]_) \[CircleTimes] (\[FormalA]_))] -> 
          OverBar[\[FormalB] \[CircleTimes] \[FormalC]], "MatchingSide" -> 1, 
        "Position" -> {1, 1}|>|>, {"SubstitutionLemma", 18} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          (\[FormalB] \[CircleTimes] \[FormalC]) == 
         OverBar[OverBar[\[FormalA] \[CircleTimes] 
             \[FormalB]]] \[CircleTimes] \[FormalC]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 43}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 9}, "Orientation" -> -1, 
        "Rule" -> OverBar[OverBar[\[FormalA]_]] -> \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CircleTimes] 
            (\[FormalB] \[CircleTimes] \[FormalC]) == 
           OverBar[OverBar[\[FormalA] \[CircleTimes] 
               \[FormalB]]] \[CircleTimes] \[FormalC]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 19} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[CircleTimes] 
          (\[FormalB] \[CircleTimes] \[FormalC]) == 
         (\[FormalA] \[CircleTimes] \[FormalB]) \[CircleTimes] \[FormalC]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 18}, "Position" -> {1}, 
        "Construct" -> {"SubstitutionLemma", 9}, "Orientation" -> -1, 
        "Rule" -> OverBar[OverBar[\[FormalA]_]] -> \[FormalA], 
        "OutputExpression" -> HoldForm[\[FormalA] \[CircleTimes] 
            (\[FormalB] \[CircleTimes] \[FormalC]) == 
           (\[FormalA] \[CircleTimes] \[FormalB]) \[CircleTimes] \[FormalC]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"Conclusion", 1} -> <|"Statement" -> 
       HoldForm[\[FormalE] \[CircleTimes] (\[FormalF] \[CircleTimes] 
           \[FormalG]) == \[FormalE] \[CircleTimes] 
          (\[FormalF] \[CircleTimes] \[FormalG])], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Position" -> {}, 
        "Construct" -> {"SubstitutionLemma", 19}, "Orientation" -> -1, 
        "Rule" -> ((\[FormalA]_) \[CircleTimes] (\[FormalB]_)) \[CircleTimes] 
           (\[FormalC]_) -> \[FormalA] \[CircleTimes] 
           (\[FormalB] \[CircleTimes] \[FormalC]), "OutputExpression" -> 
         HoldForm[\[FormalE] \[CircleTimes] (\[FormalF] \[CircleTimes] 
             \[FormalG]) == \[FormalE] \[CircleTimes] 
            (\[FormalF] \[CircleTimes] \[FormalG])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>}|>]
