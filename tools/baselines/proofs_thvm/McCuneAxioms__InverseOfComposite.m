ProofObject["EquationalLogic", Inactive[Equal][OverBar[a \[CircleTimes] b], 
  OverBar[b] \[CircleTimes] OverBar[a]], 
 {Inactive[Equal][(a_) \[CircleTimes] OverBar[(b_) \[CircleTimes] 
      ((((c_) \[CircleTimes] OverBar[c_]) \[CircleTimes] 
        OverBar[(d_) \[CircleTimes] (b_)]) \[CircleTimes] (a_))], d_]}, 
 <|"Variables" -> {a, b, c, d, x4, x5, x6, x7, x8, x9}, "Constants" -> {}, 
  "Proof" -> {{"Axiom", 1} -> <|"Statement" -> 
       HoldForm[a \[CircleTimes] OverBar[b \[CircleTimes] 
            (((c \[CircleTimes] OverBar[c]) \[CircleTimes] 
              OverBar[d \[CircleTimes] b]) \[CircleTimes] a)] == d], 
      "Proof" -> <||>|>, {"Hypothesis", 1} -> 
     <|"Statement" -> HoldForm[OverBar[a \[CircleTimes] b] == 
         OverBar[b] \[CircleTimes] OverBar[a]], "Proof" -> <||>|>, 
    {"CriticalPairLemma", 1} -> 
     <|"Statement" -> HoldForm[d == a \[CircleTimes] 
          OverBar[(((b \[CircleTimes] OverBar[b]) \[CircleTimes] 
              OverBar[c \[CircleTimes] d]) \[CircleTimes] (x4 \[CircleTimes] 
              OverBar[x4])) \[CircleTimes] (c \[CircleTimes] a)]], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CircleTimes] OverBar[(b_) \[CircleTimes] 
             ((((c_) \[CircleTimes] OverBar[c_]) \[CircleTimes] OverBar[
                (d_) \[CircleTimes] (b_)]) \[CircleTimes] (a_))] -> d, 
        "Side" -> 1, "Subpattern" -> ((c_) \[CircleTimes] 
           OverBar[c_]) \[CircleTimes] OverBar[(d_) \[CircleTimes] (b_)], 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CircleTimes] OverBar[(b_) \[CircleTimes] 
             ((((c_) \[CircleTimes] OverBar[c_]) \[CircleTimes] OverBar[
                (d_) \[CircleTimes] (b_)]) \[CircleTimes] (a_))] -> d, 
        "MatchingSide" -> 1, "Position" -> {2, 1, 2, 1}|>|>, 
    {"CriticalPairLemma", 2} -> 
     <|"Statement" -> HoldForm[x4 \[CircleTimes] (x6 \[CircleTimes] 
           OverBar[x6]) == a \[CircleTimes] OverBar[
           (b \[CircleTimes] (c \[CircleTimes] OverBar[c])) \[CircleTimes] 
            ((((d \[CircleTimes] OverBar[d]) \[CircleTimes] OverBar[
                x4 \[CircleTimes] b]) \[CircleTimes] (x5 \[CircleTimes] 
               OverBar[x5])) \[CircleTimes] a)]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 1}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CircleTimes] 
           OverBar[((((b_) \[CircleTimes] OverBar[b_]) \[CircleTimes] OverBar[
                (c_) \[CircleTimes] (d_)]) \[CircleTimes] 
              ((x4_) \[CircleTimes] OverBar[x4_])) \[CircleTimes] 
             ((c_) \[CircleTimes] (a_))] -> d, "Side" -> 1, 
        "Subpattern" -> ((b_) \[CircleTimes] OverBar[b_]) \[CircleTimes] 
          OverBar[(c_) \[CircleTimes] (d_)], "MatchingConstruct" -> 
         {"CriticalPairLemma", 1}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> (a_) \[CircleTimes] OverBar[
            ((((b_) \[CircleTimes] OverBar[b_]) \[CircleTimes] OverBar[
                (c_) \[CircleTimes] (d_)]) \[CircleTimes] 
              ((x4_) \[CircleTimes] OverBar[x4_])) \[CircleTimes] 
             ((c_) \[CircleTimes] (a_))] -> d, "MatchingSide" -> 1, 
        "Position" -> {2, 1, 1, 1}|>|>, {"CriticalPairLemma", 3} -> 
     <|"Statement" -> HoldForm[
        ((x4 \[CircleTimes] OverBar[x4]) \[CircleTimes] 
           OverBar[b \[CircleTimes] d]) \[CircleTimes] (x5 \[CircleTimes] 
           OverBar[x5]) == a \[CircleTimes] OverBar[
           (b \[CircleTimes] (c \[CircleTimes] OverBar[c])) \[CircleTimes] 
            (d \[CircleTimes] a)]], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 1}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CircleTimes] OverBar[((((b_) \[CircleTimes] 
                OverBar[b_]) \[CircleTimes] OverBar[(c_) \[CircleTimes] 
                 (d_)]) \[CircleTimes] ((x4_) \[CircleTimes] OverBar[
                x4_])) \[CircleTimes] ((c_) \[CircleTimes] (a_))] -> d, 
        "Side" -> 1, "Subpattern" -> ((b_) \[CircleTimes] 
           OverBar[b_]) \[CircleTimes] OverBar[(c_) \[CircleTimes] (d_)], 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CircleTimes] OverBar[(b_) \[CircleTimes] 
             ((((c_) \[CircleTimes] OverBar[c_]) \[CircleTimes] OverBar[
                (d_) \[CircleTimes] (b_)]) \[CircleTimes] (a_))] -> d, 
        "MatchingSide" -> 1, "Position" -> {2, 1, 1, 1}|>|>, 
    {"CriticalPairLemma", 4} -> 
     <|"Statement" -> HoldForm[((b \[CircleTimes] OverBar[b]) \[CircleTimes] 
           OverBar[((c \[CircleTimes] OverBar[c]) \[CircleTimes] 
              OverBar[d \[CircleTimes] a]) \[CircleTimes] d]) \[CircleTimes] 
          (x4 \[CircleTimes] OverBar[x4]) == a], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 3}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CircleTimes] 
           OverBar[((b_) \[CircleTimes] ((c_) \[CircleTimes] OverBar[
                c_])) \[CircleTimes] ((d_) \[CircleTimes] (a_))] -> 
          ((x4 \[CircleTimes] OverBar[x4]) \[CircleTimes] 
            OverBar[b \[CircleTimes] d]) \[CircleTimes] (x5 \[CircleTimes] 
            OverBar[x5]), "Side" -> 1, "Subpattern" -> {}, 
        "MatchingConstruct" -> {"CriticalPairLemma", 1}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CircleTimes] OverBar[((((b_) \[CircleTimes] OverBar[
                 b_]) \[CircleTimes] OverBar[(c_) \[CircleTimes] 
                 (d_)]) \[CircleTimes] ((x4_) \[CircleTimes] OverBar[
                x4_])) \[CircleTimes] ((c_) \[CircleTimes] (a_))] -> d, 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"CriticalPairLemma", 5} -> 
     <|"Statement" -> HoldForm[
        ((x6 \[CircleTimes] OverBar[x6]) \[CircleTimes] 
           OverBar[((x7 \[CircleTimes] OverBar[x7]) \[CircleTimes] 
              OverBar[x8 \[CircleTimes] x4]) \[CircleTimes] 
             x8]) \[CircleTimes] (x9 \[CircleTimes] OverBar[x9]) == 
         a \[CircleTimes] OverBar[((b \[CircleTimes] OverBar[
               b]) \[CircleTimes] (c \[CircleTimes] OverBar[
               c])) \[CircleTimes] ((((d \[CircleTimes] OverBar[
                 d]) \[CircleTimes] OverBar[x4]) \[CircleTimes] 
              (x5 \[CircleTimes] OverBar[x5])) \[CircleTimes] a)]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 2}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CircleTimes] 
           OverBar[((b_) \[CircleTimes] ((c_) \[CircleTimes] OverBar[
                c_])) \[CircleTimes] (((((d_) \[CircleTimes] OverBar[
                  d_]) \[CircleTimes] OverBar[(x4_) \[CircleTimes] 
                  (b_)]) \[CircleTimes] ((x5_) \[CircleTimes] OverBar[
                 x5_])) \[CircleTimes] (a_))] -> x4 \[CircleTimes] 
           (x6 \[CircleTimes] OverBar[x6]), "Side" -> 1, 
        "Subpattern" -> (x4_) \[CircleTimes] (b_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 4}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (((a_) \[CircleTimes] OverBar[a_]) \[CircleTimes] 
            OverBar[(((b_) \[CircleTimes] OverBar[b_]) \[CircleTimes] OverBar[
                (c_) \[CircleTimes] (d_)]) \[CircleTimes] 
              (c_)]) \[CircleTimes] ((x4_) \[CircleTimes] OverBar[x4_]) -> d, 
        "MatchingSide" -> 1, "Position" -> {2, 1, 2, 1, 1, 2, 1}|>|>, 
    {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[x4 == a \[CircleTimes] 
          OverBar[((b \[CircleTimes] OverBar[b]) \[CircleTimes] 
             (c \[CircleTimes] OverBar[c])) \[CircleTimes] 
            ((((d \[CircleTimes] OverBar[d]) \[CircleTimes] OverBar[
                x4]) \[CircleTimes] (x5 \[CircleTimes] OverBar[
                x5])) \[CircleTimes] a)]], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 5}, "Construct" -> 
         {"CriticalPairLemma", 4}, "Position" -> {}, 
        "Rule" -> (((a_) \[CircleTimes] OverBar[a_]) \[CircleTimes] 
            OverBar[(((b_) \[CircleTimes] OverBar[b_]) \[CircleTimes] OverBar[
                (c_) \[CircleTimes] (d_)]) \[CircleTimes] 
              (c_)]) \[CircleTimes] ((x4_) \[CircleTimes] OverBar[x4_]) -> d, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          x4 == a \[CircleTimes] OverBar[((b \[CircleTimes] OverBar[
                 b]) \[CircleTimes] (c \[CircleTimes] OverBar[
                 c])) \[CircleTimes] ((((d \[CircleTimes] OverBar[
                   d]) \[CircleTimes] OverBar[x4]) \[CircleTimes] 
                (x5 \[CircleTimes] OverBar[x5])) \[CircleTimes] a)]], 
        "Source" -> "cpl"|>|>, {"CriticalPairLemma", 6} -> 
     <|"Statement" -> HoldForm[(d \[CircleTimes] OverBar[d]) \[CircleTimes] 
          OverBar[b \[CircleTimes] c] == (a \[CircleTimes] 
           OverBar[a]) \[CircleTimes] OverBar[b \[CircleTimes] c]], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CircleTimes] OverBar[(b_) \[CircleTimes] 
             ((((c_) \[CircleTimes] OverBar[c_]) \[CircleTimes] OverBar[
                (d_) \[CircleTimes] (b_)]) \[CircleTimes] (a_))] -> d, 
        "Side" -> 1, "Subpattern" -> (((c_) \[CircleTimes] 
            OverBar[c_]) \[CircleTimes] OverBar[(d_) \[CircleTimes] 
             (b_)]) \[CircleTimes] (a_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 4}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (((a_) \[CircleTimes] OverBar[a_]) \[CircleTimes] 
            OverBar[(((b_) \[CircleTimes] OverBar[b_]) \[CircleTimes] OverBar[
                (c_) \[CircleTimes] (d_)]) \[CircleTimes] 
              (c_)]) \[CircleTimes] ((x4_) \[CircleTimes] OverBar[x4_]) -> d, 
        "MatchingSide" -> 1, "Position" -> {2, 1, 2}|>|>, 
    {"CriticalPairLemma", 7} -> 
     <|"Statement" -> HoldForm[x5 \[CircleTimes] OverBar[x5] == 
         a \[CircleTimes] OverBar[OverBar[b \[CircleTimes] c] \[CircleTimes] 
            (((d \[CircleTimes] OverBar[d]) \[CircleTimes] 
              OverBar[(x4 \[CircleTimes] OverBar[x4]) \[CircleTimes] 
                OverBar[b \[CircleTimes] c]]) \[CircleTimes] a)]], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CircleTimes] OverBar[(b_) \[CircleTimes] 
             ((((c_) \[CircleTimes] OverBar[c_]) \[CircleTimes] OverBar[
                (d_) \[CircleTimes] (b_)]) \[CircleTimes] (a_))] -> d, 
        "Side" -> 1, "Subpattern" -> (d_) \[CircleTimes] (b_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 6}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((a_) \[CircleTimes] OverBar[a_]) \[CircleTimes] 
           OverBar[(b_) \[CircleTimes] (c_)] -> 
          (d \[CircleTimes] OverBar[d]) \[CircleTimes] 
           OverBar[b \[CircleTimes] c], "MatchingSide" -> 1, 
        "Position" -> {2, 1, 2, 1, 2, 1}|>|>, {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[x5 \[CircleTimes] OverBar[x5] == 
         x4 \[CircleTimes] OverBar[x4]], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 7}, "Construct" -> {"Axiom", 1}, 
        "Position" -> {}, "Rule" -> (a_) \[CircleTimes] 
           OverBar[(b_) \[CircleTimes] ((((c_) \[CircleTimes] OverBar[
                 c_]) \[CircleTimes] OverBar[(d_) \[CircleTimes] 
                 (b_)]) \[CircleTimes] (a_))] -> d, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[x5 \[CircleTimes] OverBar[x5] == 
           x4 \[CircleTimes] OverBar[x4]], "Source" -> "cpl"|>|>, 
    {"CriticalPairLemma", 8} -> 
     <|"Statement" -> HoldForm[OverBar[d] == a \[CircleTimes] 
          OverBar[((b \[CircleTimes] OverBar[b]) \[CircleTimes] 
             (c \[CircleTimes] OverBar[c])) \[CircleTimes] (d \[CircleTimes] 
             a)]], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 1}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CircleTimes] 
           OverBar[((((b_) \[CircleTimes] OverBar[b_]) \[CircleTimes] OverBar[
                (c_) \[CircleTimes] (d_)]) \[CircleTimes] 
              ((x4_) \[CircleTimes] OverBar[x4_])) \[CircleTimes] 
             ((c_) \[CircleTimes] (a_))] -> d, "Side" -> 1, 
        "Subpattern" -> ((b_) \[CircleTimes] OverBar[b_]) \[CircleTimes] 
          OverBar[(c_) \[CircleTimes] (d_)], "MatchingConstruct" -> 
         {"SubstitutionLemma", 2}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CircleTimes] OverBar[a_] -> 
          b \[CircleTimes] OverBar[b], "MatchingSide" -> 1, 
        "Position" -> {2, 1, 1, 1}|>|>, {"SubstitutionLemma", 3} -> 
     <|"Statement" -> HoldForm[x4 == OverBar[
          ((d \[CircleTimes] OverBar[d]) \[CircleTimes] 
            OverBar[x4]) \[CircleTimes] (x5 \[CircleTimes] OverBar[x5])]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 1}, 
        "Construct" -> {"CriticalPairLemma", 8}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] OverBar[(((b_) \[CircleTimes] OverBar[
                b_]) \[CircleTimes] ((c_) \[CircleTimes] OverBar[
                c_])) \[CircleTimes] ((d_) \[CircleTimes] (a_))] -> 
          OverBar[d], "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[x4 == OverBar[((d \[CircleTimes] OverBar[d]) \[CircleTimes] 
              OverBar[x4]) \[CircleTimes] (x5 \[CircleTimes] OverBar[x5])]], 
        "Source" -> "cpl"|>|>, {"CriticalPairLemma", 9} -> 
     <|"Statement" -> HoldForm[((b \[CircleTimes] OverBar[b]) \[CircleTimes] 
           OverBar[c \[CircleTimes] a]) \[CircleTimes] c == OverBar[a]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 3}, 
        "Orientation" -> -1, "Rule" -> 
         OverBar[(((a_) \[CircleTimes] OverBar[a_]) \[CircleTimes] 
             OverBar[b_]) \[CircleTimes] ((c_) \[CircleTimes] 
             OverBar[c_])] -> b, "Side" -> 1, "Subpattern" -> 
         (((a_) \[CircleTimes] OverBar[a_]) \[CircleTimes] 
           OverBar[b_]) \[CircleTimes] ((c_) \[CircleTimes] OverBar[c_]), 
        "MatchingConstruct" -> {"CriticalPairLemma", 4}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (((a_) \[CircleTimes] OverBar[a_]) \[CircleTimes] 
            OverBar[(((b_) \[CircleTimes] OverBar[b_]) \[CircleTimes] OverBar[
                (c_) \[CircleTimes] (d_)]) \[CircleTimes] 
              (c_)]) \[CircleTimes] ((x4_) \[CircleTimes] OverBar[x4_]) -> d, 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 10} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[b]] == 
         (a \[CircleTimes] OverBar[a]) \[CircleTimes] b], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 9}, 
        "Orientation" -> 1, "Rule" -> 
         (((a_) \[CircleTimes] OverBar[a_]) \[CircleTimes] 
            OverBar[(b_) \[CircleTimes] (c_)]) \[CircleTimes] (b_) -> 
          OverBar[c], "Side" -> 1, "Subpattern" -> 
         ((a_) \[CircleTimes] OverBar[a_]) \[CircleTimes] 
          OverBar[(b_) \[CircleTimes] (c_)], "MatchingConstruct" -> 
         {"SubstitutionLemma", 2}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CircleTimes] OverBar[a_] -> 
          b \[CircleTimes] OverBar[b], "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 4} -> 
     <|"Statement" -> HoldForm[
        OverBar[OverBar[OverBar[c \[CircleTimes] a]]] \[CircleTimes] c == 
         OverBar[a]], "Proof" -> <|"Input" -> {"CriticalPairLemma", 9}, 
        "Construct" -> {"CriticalPairLemma", 10}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CircleTimes] OverBar[a_]) \[CircleTimes] (b_) -> 
          OverBar[OverBar[b]], "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[OverBar[OverBar[OverBar[c \[CircleTimes] 
                a]]] \[CircleTimes] c == OverBar[a]], "Source" -> "cpl"|>|>, 
    {"CriticalPairLemma", 11} -> 
     <|"Statement" -> HoldForm[x4 == a \[CircleTimes] 
          OverBar[OverBar[b \[CircleTimes] (((c \[CircleTimes] OverBar[
                  c]) \[CircleTimes] OverBar[d \[CircleTimes] 
                  b]) \[CircleTimes] x4)] \[CircleTimes] 
            (((x5 \[CircleTimes] OverBar[x5]) \[CircleTimes] 
              OverBar[d]) \[CircleTimes] a)]], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CircleTimes] OverBar[(b_) \[CircleTimes] 
             ((((c_) \[CircleTimes] OverBar[c_]) \[CircleTimes] OverBar[
                (d_) \[CircleTimes] (b_)]) \[CircleTimes] (a_))] -> d, 
        "Side" -> 1, "Subpattern" -> (d_) \[CircleTimes] (b_), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CircleTimes] OverBar[(b_) \[CircleTimes] 
             ((((c_) \[CircleTimes] OverBar[c_]) \[CircleTimes] OverBar[
                (d_) \[CircleTimes] (b_)]) \[CircleTimes] (a_))] -> d, 
        "MatchingSide" -> 1, "Position" -> {2, 1, 2, 1, 2, 1}|>|>, 
    {"CriticalPairLemma", 12} -> 
     <|"Statement" -> HoldForm[x5 == OverBar[a \[CircleTimes] 
            b] \[CircleTimes] OverBar[OverBar[c \[CircleTimes] 
              (((d \[CircleTimes] OverBar[d]) \[CircleTimes] OverBar[
                 (x4 \[CircleTimes] OverBar[x4]) \[CircleTimes] 
                  c]) \[CircleTimes] x5)] \[CircleTimes] 
            ((x6 \[CircleTimes] OverBar[x6]) \[CircleTimes] 
             OverBar[a \[CircleTimes] b])]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 11}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CircleTimes] 
           OverBar[OverBar[(b_) \[CircleTimes] ((((c_) \[CircleTimes] 
                  OverBar[c_]) \[CircleTimes] OverBar[(d_) \[CircleTimes] 
                   (b_)]) \[CircleTimes] (x4_))] \[CircleTimes] 
             ((((x5_) \[CircleTimes] OverBar[x5_]) \[CircleTimes] OverBar[
                d_]) \[CircleTimes] (a_))] -> x4, "Side" -> 1, 
        "Subpattern" -> (((x5_) \[CircleTimes] OverBar[x5_]) \[CircleTimes] 
           OverBar[d_]) \[CircleTimes] (a_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 6}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> ((a_) \[CircleTimes] OverBar[a_]) \[CircleTimes] 
           OverBar[(b_) \[CircleTimes] (c_)] -> 
          (d \[CircleTimes] OverBar[d]) \[CircleTimes] 
           OverBar[b \[CircleTimes] c], "MatchingSide" -> 1, 
        "Position" -> {2, 1, 2}|>|>, {"CriticalPairLemma", 13} -> 
     <|"Statement" -> HoldForm[b == a \[CircleTimes] 
          OverBar[OverBar[b] \[CircleTimes] ((c \[CircleTimes] 
              OverBar[c]) \[CircleTimes] a)]], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
        "Rule" -> (a_) \[CircleTimes] OverBar[(b_) \[CircleTimes] 
             ((((c_) \[CircleTimes] OverBar[c_]) \[CircleTimes] OverBar[
                (d_) \[CircleTimes] (b_)]) \[CircleTimes] (a_))] -> d, 
        "Side" -> 1, "Subpattern" -> ((c_) \[CircleTimes] 
           OverBar[c_]) \[CircleTimes] OverBar[(d_) \[CircleTimes] (b_)], 
        "MatchingConstruct" -> {"SubstitutionLemma", 2}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] OverBar[a_] -> b \[CircleTimes] OverBar[b], 
        "MatchingSide" -> 1, "Position" -> {2, 1, 2, 1}|>|>, 
    {"SubstitutionLemma", 5} -> 
     <|"Statement" -> HoldForm[x5 == c \[CircleTimes] 
          (((d \[CircleTimes] OverBar[d]) \[CircleTimes] 
            OverBar[(x4 \[CircleTimes] OverBar[x4]) \[CircleTimes] 
              c]) \[CircleTimes] x5)], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 12}, "Construct" -> 
         {"CriticalPairLemma", 13}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] OverBar[OverBar[b_] \[CircleTimes] 
             (((c_) \[CircleTimes] OverBar[c_]) \[CircleTimes] (a_))] -> b, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          x5 == c \[CircleTimes] (((d \[CircleTimes] OverBar[
                d]) \[CircleTimes] OverBar[(x4 \[CircleTimes] OverBar[
                  x4]) \[CircleTimes] c]) \[CircleTimes] x5)], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 6} -> 
     <|"Statement" -> HoldForm[x5 == c \[CircleTimes] 
          (((d \[CircleTimes] OverBar[d]) \[CircleTimes] 
            OverBar[OverBar[OverBar[c]]]) \[CircleTimes] x5)], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 5}, 
        "Construct" -> {"CriticalPairLemma", 10}, "Position" -> {2, 1, 2, 1}, 
        "Rule" -> ((a_) \[CircleTimes] OverBar[a_]) \[CircleTimes] (b_) -> 
          OverBar[OverBar[b]], "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[x5 == c \[CircleTimes] (((d \[CircleTimes] OverBar[
                d]) \[CircleTimes] OverBar[OverBar[OverBar[
                 c]]]) \[CircleTimes] x5)], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 7} -> 
     <|"Statement" -> HoldForm[x5 == c \[CircleTimes] 
          (OverBar[OverBar[OverBar[OverBar[OverBar[c]]]]] \[CircleTimes] 
           x5)], "Proof" -> <|"Input" -> {"SubstitutionLemma", 6}, 
        "Construct" -> {"CriticalPairLemma", 10}, "Position" -> {2, 1}, 
        "Rule" -> ((a_) \[CircleTimes] OverBar[a_]) \[CircleTimes] (b_) -> 
          OverBar[OverBar[b]], "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[x5 == c \[CircleTimes] (OverBar[OverBar[OverBar[
                OverBar[OverBar[c]]]]] \[CircleTimes] x5)], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 8} -> 
     <|"Statement" -> HoldForm[b == a \[CircleTimes] 
          OverBar[OverBar[b] \[CircleTimes] OverBar[OverBar[a]]]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 13}, 
        "Construct" -> {"CriticalPairLemma", 10}, "Position" -> {2, 1, 2}, 
        "Rule" -> ((a_) \[CircleTimes] OverBar[a_]) \[CircleTimes] (b_) -> 
          OverBar[OverBar[b]], "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[b == a \[CircleTimes] OverBar[OverBar[b] \[CircleTimes] 
              OverBar[OverBar[a]]]], "Source" -> "cpl"|>|>, 
    {"CriticalPairLemma", 14} -> 
     <|"Statement" -> HoldForm[
        OverBar[OverBar[OverBar[OverBar[a]] \[CircleTimes] b]] == 
         a \[CircleTimes] OverBar[OverBar[b]]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 8}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CircleTimes] 
           OverBar[OverBar[b_] \[CircleTimes] OverBar[OverBar[a_]]] -> b, 
        "Side" -> 1, "Subpattern" -> OverBar[b_] \[CircleTimes] 
          OverBar[OverBar[a_]], "MatchingConstruct" -> {"SubstitutionLemma", 
          4}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         OverBar[OverBar[OverBar[(a_) \[CircleTimes] (b_)]]] \[CircleTimes] 
           (a_) -> OverBar[b], "MatchingSide" -> 1, "Position" -> {2, 1}|>|>, 
    {"CriticalPairLemma", 15} -> 
     <|"Statement" -> HoldForm[OverBar[b \[CircleTimes] a] \[CircleTimes] 
          OverBar[OverBar[b]] == OverBar[OverBar[OverBar[a]]]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 14}, 
        "Orientation" -> 1, "Rule" -> 
         OverBar[OverBar[OverBar[OverBar[a_]] \[CircleTimes] (b_)]] -> 
          a \[CircleTimes] OverBar[OverBar[b]], "Side" -> 1, 
        "Subpattern" -> OverBar[OverBar[a_]] \[CircleTimes] (b_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 4}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         OverBar[OverBar[OverBar[(a_) \[CircleTimes] (b_)]]] \[CircleTimes] 
           (a_) -> OverBar[b], "MatchingSide" -> 1, "Position" -> {1, 1}|>|>, 
    {"CriticalPairLemma", 16} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[OverBar[OverBar[b]]]] == 
         OverBar[a \[CircleTimes] OverBar[a]] \[CircleTimes] 
          OverBar[OverBar[b]]], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 15}, "Orientation" -> 1, 
        "Rule" -> OverBar[(a_) \[CircleTimes] (b_)] \[CircleTimes] 
           OverBar[OverBar[a_]] -> OverBar[OverBar[OverBar[b]]], "Side" -> 1, 
        "Subpattern" -> (a_) \[CircleTimes] (b_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 2}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CircleTimes] OverBar[a_] -> 
          b \[CircleTimes] OverBar[b], "MatchingSide" -> 1, 
        "Position" -> {1, 1}|>|>, {"CriticalPairLemma", 17} -> 
     <|"Statement" -> HoldForm[c == OverBar[a \[CircleTimes] 
            OverBar[a]] \[CircleTimes] ((b \[CircleTimes] 
            OverBar[b]) \[CircleTimes] c)], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 5}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CircleTimes] ((((b_) \[CircleTimes] 
              OverBar[b_]) \[CircleTimes] OverBar[((c_) \[CircleTimes] 
                OverBar[c_]) \[CircleTimes] (a_)]) \[CircleTimes] (d_)) -> d, 
        "Side" -> 1, "Subpattern" -> ((b_) \[CircleTimes] 
           OverBar[b_]) \[CircleTimes] OverBar[((c_) \[CircleTimes] 
             OverBar[c_]) \[CircleTimes] (a_)], "MatchingConstruct" -> 
         {"SubstitutionLemma", 2}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CircleTimes] OverBar[a_] -> 
          b \[CircleTimes] OverBar[b], "MatchingSide" -> 1, 
        "Position" -> {2, 1}|>|>, {"SubstitutionLemma", 9} -> 
     <|"Statement" -> HoldForm[c == OverBar[a \[CircleTimes] 
            OverBar[a]] \[CircleTimes] OverBar[OverBar[c]]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 17}, 
        "Construct" -> {"CriticalPairLemma", 10}, "Position" -> {2}, 
        "Rule" -> ((a_) \[CircleTimes] OverBar[a_]) \[CircleTimes] (b_) -> 
          OverBar[OverBar[b]], "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[c == OverBar[a \[CircleTimes] OverBar[a]] \[CircleTimes] 
            OverBar[OverBar[c]]], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 10} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[OverBar[OverBar[b]]]] == b], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 16}, 
        "Construct" -> {"SubstitutionLemma", 9}, "Position" -> {}, 
        "Rule" -> OverBar[(a_) \[CircleTimes] OverBar[a_]] \[CircleTimes] 
           OverBar[OverBar[b_]] -> b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[
          OverBar[OverBar[OverBar[OverBar[b]]]] == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 11} -> 
     <|"Statement" -> HoldForm[x5 == c \[CircleTimes] 
          (OverBar[c] \[CircleTimes] x5)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 7}, "Construct" -> 
         {"SubstitutionLemma", 10}, "Position" -> {2, 1, 1}, 
        "Rule" -> OverBar[OverBar[OverBar[OverBar[a_]]]] -> a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          x5 == c \[CircleTimes] (OverBar[c] \[CircleTimes] x5)], 
        "Source" -> "cpl"|>|>, {"CriticalPairLemma", 18} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[a]] == a \[CircleTimes] 
          (b \[CircleTimes] OverBar[b])], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 11}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CircleTimes] (OverBar[a_] \[CircleTimes] (b_)) -> b, 
        "Side" -> 1, "Subpattern" -> OverBar[a_] \[CircleTimes] (b_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 2}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CircleTimes] OverBar[a_] -> b \[CircleTimes] OverBar[b], 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 19} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[a]] \[CircleTimes] 
          OverBar[OverBar[b]] == OverBar[OverBar[a \[CircleTimes] b]]], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 14}, 
        "Orientation" -> 1, "Rule" -> 
         OverBar[OverBar[OverBar[OverBar[a_]] \[CircleTimes] (b_)]] -> 
          a \[CircleTimes] OverBar[OverBar[b]], "Side" -> 1, 
        "Subpattern" -> OverBar[OverBar[a_]], "MatchingConstruct" -> 
         {"SubstitutionLemma", 10}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> OverBar[OverBar[OverBar[OverBar[a_]]]] -> a, 
        "MatchingSide" -> 1, "Position" -> {1, 1, 1}|>|>, 
    {"CriticalPairLemma", 20} -> 
     <|"Statement" -> HoldForm[OverBar[b] == a \[CircleTimes] 
          OverBar[OverBar[OverBar[b \[CircleTimes] a]]]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 8}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CircleTimes] 
           OverBar[OverBar[b_] \[CircleTimes] OverBar[OverBar[a_]]] -> b, 
        "Side" -> 1, "Subpattern" -> OverBar[b_] \[CircleTimes] 
          OverBar[OverBar[a_]], "MatchingConstruct" -> {"CriticalPairLemma", 
          19}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         OverBar[OverBar[a_]] \[CircleTimes] OverBar[OverBar[b_]] -> 
          OverBar[OverBar[a \[CircleTimes] b]], "MatchingSide" -> 1, 
        "Position" -> {2, 1}|>|>, {"CriticalPairLemma", 21} -> 
     <|"Statement" -> HoldForm[a \[CircleTimes] (c \[CircleTimes] 
           OverBar[c]) == a \[CircleTimes] (b \[CircleTimes] OverBar[b])], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 2}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CircleTimes] 
           OverBar[((b_) \[CircleTimes] ((c_) \[CircleTimes] OverBar[
                c_])) \[CircleTimes] (((((d_) \[CircleTimes] OverBar[
                  d_]) \[CircleTimes] OverBar[(x4_) \[CircleTimes] 
                  (b_)]) \[CircleTimes] ((x5_) \[CircleTimes] OverBar[
                 x5_])) \[CircleTimes] (a_))] -> x4 \[CircleTimes] 
           (x6 \[CircleTimes] OverBar[x6]), "Side" -> 1, "Subpattern" -> {}, 
        "MatchingConstruct" -> {"CriticalPairLemma", 2}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (a_) \[CircleTimes] OverBar[((b_) \[CircleTimes] 
              ((c_) \[CircleTimes] OverBar[c_])) \[CircleTimes] 
             (((((d_) \[CircleTimes] OverBar[d_]) \[CircleTimes] 
                OverBar[(x4_) \[CircleTimes] (b_)]) \[CircleTimes] (
                (x5_) \[CircleTimes] OverBar[x5_])) \[CircleTimes] (a_))] -> 
          x4 \[CircleTimes] (x6 \[CircleTimes] OverBar[x6]), 
        "MatchingSide" -> 1, "Position" -> {}|>|>, 
    {"CriticalPairLemma", 22} -> 
     <|"Statement" -> HoldForm[OverBar[b] == 
         (a \[CircleTimes] OverBar[a]) \[CircleTimes] 
          OverBar[OverBar[OverBar[b \[CircleTimes] (c \[CircleTimes] OverBar[
                c])]]]], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 
          20}, "Orientation" -> -1, "Rule" -> 
         (a_) \[CircleTimes] OverBar[OverBar[OverBar[(b_) \[CircleTimes] (
                a_)]]] -> OverBar[b], "Side" -> 1, "Subpattern" -> 
         (b_) \[CircleTimes] (a_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 21}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] 
            OverBar[b_]) -> a \[CircleTimes] (c \[CircleTimes] OverBar[c]), 
        "MatchingSide" -> 1, "Position" -> {2, 1, 1, 1}|>|>, 
    {"SubstitutionLemma", 12} -> 
     <|"Statement" -> HoldForm[OverBar[b] == 
         OverBar[OverBar[OverBar[OverBar[OverBar[b \[CircleTimes] (
                c \[CircleTimes] OverBar[c])]]]]]], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 22}, 
        "Construct" -> {"CriticalPairLemma", 10}, "Position" -> {}, 
        "Rule" -> ((a_) \[CircleTimes] OverBar[a_]) \[CircleTimes] (b_) -> 
          OverBar[OverBar[b]], "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[OverBar[b] == OverBar[OverBar[OverBar[OverBar[OverBar[
                b \[CircleTimes] (c \[CircleTimes] OverBar[c])]]]]]], 
        "Source" -> "cpl"|>|>, {"SubstitutionLemma", 13} -> 
     <|"Statement" -> HoldForm[OverBar[b] == OverBar[b \[CircleTimes] 
           (c \[CircleTimes] OverBar[c])]], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 12}, "Construct" -> 
         {"SubstitutionLemma", 10}, "Position" -> {1}, 
        "Rule" -> OverBar[OverBar[OverBar[OverBar[a_]]]] -> a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[OverBar[b] == 
           OverBar[b \[CircleTimes] (c \[CircleTimes] OverBar[c])]], 
        "Source" -> "cpl"|>|>, {"CriticalPairLemma", 23} -> 
     <|"Statement" -> HoldForm[b \[CircleTimes] (c \[CircleTimes] 
           OverBar[c]) == a \[CircleTimes] OverBar[OverBar[b] \[CircleTimes] 
            OverBar[OverBar[a]]]], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 8}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CircleTimes] OverBar[OverBar[b_] \[CircleTimes] 
             OverBar[OverBar[a_]]] -> b, "Side" -> 1, 
        "Subpattern" -> OverBar[b_], "MatchingConstruct" -> 
         {"SubstitutionLemma", 13}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> OverBar[(a_) \[CircleTimes] ((b_) \[CircleTimes] 
             OverBar[b_])] -> OverBar[a], "MatchingSide" -> 1, 
        "Position" -> {2, 1, 1}|>|>, {"SubstitutionLemma", 14} -> 
     <|"Statement" -> HoldForm[b \[CircleTimes] (c \[CircleTimes] 
           OverBar[c]) == b], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 23}, "Construct" -> 
         {"SubstitutionLemma", 8}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] OverBar[OverBar[b_] \[CircleTimes] 
             OverBar[OverBar[a_]]] -> b, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[b \[CircleTimes] (c \[CircleTimes] 
             OverBar[c]) == b], "Source" -> "cpl"|>|>, 
    {"SubstitutionLemma", 15} -> 
     <|"Statement" -> HoldForm[OverBar[OverBar[a]] == a], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 18}, 
        "Construct" -> {"SubstitutionLemma", 14}, "Position" -> {}, 
        "Rule" -> (a_) \[CircleTimes] ((b_) \[CircleTimes] OverBar[b_]) -> a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[OverBar[OverBar[a]] == 
           a], "Source" -> "cpl"|>|>, {"SubstitutionLemma", 16} -> 
     <|"Statement" -> HoldForm[OverBar[c \[CircleTimes] a] \[CircleTimes] 
          c == OverBar[a]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 4}, 
        "Construct" -> {"SubstitutionLemma", 15}, "Position" -> {1, 1}, 
        "Rule" -> OverBar[OverBar[a_]] -> a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[
          OverBar[c \[CircleTimes] a] \[CircleTimes] c == OverBar[a]], 
        "Source" -> "cpl"|>|>, {"CriticalPairLemma", 24} -> 
     <|"Statement" -> HoldForm[b == OverBar[a] \[CircleTimes] 
          (a \[CircleTimes] b)], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 11}, "Orientation" -> -1, 
        "Rule" -> (a_) \[CircleTimes] (OverBar[a_] \[CircleTimes] (b_)) -> b, 
        "Side" -> 1, "Subpattern" -> OverBar[a_], "MatchingConstruct" -> 
         {"SubstitutionLemma", 15}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> OverBar[OverBar[a_]] -> a, "MatchingSide" -> 1, 
        "Position" -> {2, 1}|>|>, {"CriticalPairLemma", 25} -> 
     <|"Statement" -> HoldForm[OverBar[b \[CircleTimes] a] == 
         OverBar[a] \[CircleTimes] OverBar[b]], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 16}, 
        "Orientation" -> 1, "Rule" -> 
         OverBar[(a_) \[CircleTimes] (b_)] \[CircleTimes] (a_) -> OverBar[b], 
        "Side" -> 1, "Subpattern" -> (a_) \[CircleTimes] (b_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 24}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         OverBar[a_] \[CircleTimes] ((a_) \[CircleTimes] (b_)) -> b, 
        "MatchingSide" -> 1, "Position" -> {1, 1}|>|>, 
    {"Conclusion", 1} -> <|"Statement" -> 
       HoldForm[OverBar[a \[CircleTimes] b] == OverBar[a \[CircleTimes] b]], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Construct" -> 
         {"CriticalPairLemma", 25}, "Position" -> {}, 
        "Rule" -> OverBar[a_] \[CircleTimes] OverBar[b_] -> 
          OverBar[b \[CircleTimes] a], "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[OverBar[a \[CircleTimes] b] == 
           OverBar[a \[CircleTimes] b]], "Source" -> "cpl"|>|>}|>]
