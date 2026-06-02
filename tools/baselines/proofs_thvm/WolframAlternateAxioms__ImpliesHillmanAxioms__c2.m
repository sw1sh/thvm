ProofObject["EquationalLogic", Inactive[Equal][
  a \[CenterDot] (a \[CenterDot] b), a \[CenterDot] (b \[CenterDot] b)], 
 {Inactive[Equal][((a_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] 
      (a_))) \[CenterDot] ((b_) \[CenterDot] ((c_) \[CenterDot] (a_))), b_]}, 
 <|"Variables" -> {a, b, c}, "Constants" -> {}, 
  "Proof" -> {{"Axiom", 1} -> <|"Statement" -> 
       HoldForm[(a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
            a)) \[CenterDot] (b \[CenterDot] (c \[CenterDot] a)) == b], 
      "Proof" -> <||>|>, {"Hypothesis", 1} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] b) == 
         a \[CenterDot] (b \[CenterDot] b)], "Proof" -> <||>|>, 
    {"CriticalPairLemma", 1} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] 
          ((c \[CenterDot] b) \[CenterDot] b) == 
         ((a \[CenterDot] b) \[CenterDot] 
           (((b \[CenterDot] ((c \[CenterDot] b) \[CenterDot] 
               b)) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
            (a \[CenterDot] b))) \[CenterDot] c], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
        "Rule" -> ((a_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] 
             (a_))) \[CenterDot] ((b_) \[CenterDot] ((c_) \[CenterDot] 
             (a_))) -> b, "Side" -> 1, "Subpattern" -> (b_) \[CenterDot] 
          ((c_) \[CenterDot] (a_)), "MatchingConstruct" -> {"Axiom", 1}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((a_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] 
             (a_))) \[CenterDot] ((b_) \[CenterDot] ((c_) \[CenterDot] 
             (a_))) -> b, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"CriticalPairLemma", 2} -> 
     <|"Statement" -> HoldForm[b == (a \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (b \[CenterDot] (c \[CenterDot] ((a \[CenterDot] c) \[CenterDot] 
             c)))], "Proof" -> <|"Construct" -> {"Axiom", 1}, 
        "Orientation" -> 1, "Rule" -> 
         ((a_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] 
             (a_))) \[CenterDot] ((b_) \[CenterDot] ((c_) \[CenterDot] 
             (a_))) -> b, "Side" -> 1, "Subpattern" -> (c_) \[CenterDot] 
          (a_), "MatchingConstruct" -> {"CriticalPairLemma", 1}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         (((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((((b_) \[CenterDot] (((c_) \[CenterDot] (b_)) \[CenterDot] 
                (b_))) \[CenterDot] ((a_) \[CenterDot] (b_))) \[CenterDot] 
             ((a_) \[CenterDot] (b_)))) \[CenterDot] (c_) -> 
          b \[CenterDot] ((c \[CenterDot] b) \[CenterDot] b), 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"CriticalPairLemma", 3} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] 
          ((b \[CenterDot] b) \[CenterDot] b) == 
         (a \[CenterDot] (((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
               b)) \[CenterDot] a) \[CenterDot] a)) \[CenterDot] b], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 2}, 
        "Orientation" -> -1, "Rule" -> 
         ((a_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] 
             (a_))) \[CenterDot] ((b_) \[CenterDot] ((c_) \[CenterDot] 
             (((a_) \[CenterDot] (c_)) \[CenterDot] (c_)))) -> b, 
        "Side" -> 1, "Subpattern" -> (b_) \[CenterDot] ((c_) \[CenterDot] 
           (((a_) \[CenterDot] (c_)) \[CenterDot] (c_))), 
        "MatchingConstruct" -> {"Axiom", 1}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> ((a_) \[CenterDot] (((b_) \[CenterDot] 
              (a_)) \[CenterDot] (a_))) \[CenterDot] ((b_) \[CenterDot] 
            ((c_) \[CenterDot] (a_))) -> b, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 4} -> 
     <|"Statement" -> HoldForm[
        (b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] b)) \[CenterDot] 
          (((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] b)) \[CenterDot] 
            (b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
              b))) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
              b) \[CenterDot] b))) == (a \[CenterDot] 
           ((((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                b)) \[CenterDot] (b \[CenterDot] (b \[CenterDot] 
                ((b \[CenterDot] b) \[CenterDot] b)))) \[CenterDot] 
             a) \[CenterDot] a)) \[CenterDot] (b \[CenterDot] 
           ((b \[CenterDot] b) \[CenterDot] b))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 3}, 
        "Orientation" -> -1, "Rule" -> 
         ((a_) \[CenterDot] ((((b_) \[CenterDot] (((b_) \[CenterDot] 
                 (b_)) \[CenterDot] (b_))) \[CenterDot] (a_)) \[CenterDot] 
             (a_))) \[CenterDot] (b_) -> b \[CenterDot] 
           ((b \[CenterDot] b) \[CenterDot] b), "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"Axiom", 1}, "MatchingOrientation" -> 1, "MatchingRule" -> 
         ((a_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] 
             (a_))) \[CenterDot] ((b_) \[CenterDot] ((c_) \[CenterDot] 
             (a_))) -> b, "MatchingSide" -> 1, "Position" -> {1, 2, 1, 1, 2, 
         1}|>|>, {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[
        (b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] b)) \[CenterDot] 
          (((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] b)) \[CenterDot] 
            (b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
              b))) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
              b) \[CenterDot] b))) == (a \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] b))], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 4}, 
        "Construct" -> {"CriticalPairLemma", 2}, "Position" -> {1, 2, 1, 1}, 
        "Rule" -> ((a_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] 
             (a_))) \[CenterDot] ((b_) \[CenterDot] ((c_) \[CenterDot] 
             (((a_) \[CenterDot] (c_)) \[CenterDot] (c_)))) -> b, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] b)) \[CenterDot] 
            (((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
                b)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
                 b) \[CenterDot] b))) \[CenterDot] (b \[CenterDot] 
              ((b \[CenterDot] b) \[CenterDot] b))) == 
           (a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] b))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[
        (b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] b)) \[CenterDot] 
          (b \[CenterDot] (b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
             b))) == (a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
            a)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
            b))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 1}, 
        "Construct" -> {"Axiom", 1}, "Position" -> {2, 1}, 
        "Rule" -> ((a_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] 
             (a_))) \[CenterDot] ((b_) \[CenterDot] ((c_) \[CenterDot] 
             (a_))) -> b, "Orientation" -> 1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[(b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
              b)) \[CenterDot] (b \[CenterDot] (b \[CenterDot] 
              ((b \[CenterDot] b) \[CenterDot] b))) == 
           (a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] b))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 3} -> 
     <|"Statement" -> HoldForm[b == (a \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] b))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 2}, 
        "Construct" -> {"CriticalPairLemma", 2}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] 
             (a_))) \[CenterDot] ((b_) \[CenterDot] ((c_) \[CenterDot] 
             (((a_) \[CenterDot] (c_)) \[CenterDot] (c_)))) -> b, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          b == (a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] 
              a)) \[CenterDot] (b \[CenterDot] ((b \[CenterDot] 
               b) \[CenterDot] b))], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 5} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] a) == 
         ((a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
           (a \[CenterDot] (a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
              a)))) \[CenterDot] a], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 3}, "Orientation" -> -1, 
        "Rule" -> ((a_) \[CenterDot] ((((b_) \[CenterDot] (((b_) \[CenterDot] 
                 (b_)) \[CenterDot] (b_))) \[CenterDot] (a_)) \[CenterDot] 
             (a_))) \[CenterDot] (b_) -> b \[CenterDot] 
           ((b \[CenterDot] b) \[CenterDot] b), "Side" -> 1, 
        "Subpattern" -> ((b_) \[CenterDot] (((b_) \[CenterDot] 
             (b_)) \[CenterDot] (b_))) \[CenterDot] (a_), 
        "MatchingConstruct" -> {"SubstitutionLemma", 3}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] 
             (a_))) \[CenterDot] ((b_) \[CenterDot] 
            (((b_) \[CenterDot] (b_)) \[CenterDot] (b_))) -> b, 
        "MatchingSide" -> 1, "Position" -> {1, 2, 1}|>|>, 
    {"SubstitutionLemma", 4} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((a \[CenterDot] a) \[CenterDot] a) == a \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 5}, 
        "Construct" -> {"CriticalPairLemma", 2}, "Position" -> {1}, 
        "Rule" -> ((a_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] 
             (a_))) \[CenterDot] ((b_) \[CenterDot] ((c_) \[CenterDot] 
             (((a_) \[CenterDot] (c_)) \[CenterDot] (c_)))) -> b, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CenterDot] ((a \[CenterDot] a) \[CenterDot] a) == 
           a \[CenterDot] a], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 6} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          (a \[CenterDot] (b \[CenterDot] a))], 
      "Proof" -> <|"Construct" -> {"Axiom", 1}, "Orientation" -> 1, 
        "Rule" -> ((a_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] 
             (a_))) \[CenterDot] ((b_) \[CenterDot] ((c_) \[CenterDot] 
             (a_))) -> b, "Side" -> 1, "Subpattern" -> (a_) \[CenterDot] 
          (((b_) \[CenterDot] (a_)) \[CenterDot] (a_)), 
        "MatchingConstruct" -> {"SubstitutionLemma", 4}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] (a_)) -> 
          a \[CenterDot] a, "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 7} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] b == 
         (a \[CenterDot] (((b \[CenterDot] b) \[CenterDot] a) \[CenterDot] 
            a)) \[CenterDot] b], "Proof" -> 
       <|"Construct" -> {"CriticalPairLemma", 2}, "Orientation" -> -1, 
        "Rule" -> ((a_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] 
             (a_))) \[CenterDot] ((b_) \[CenterDot] ((c_) \[CenterDot] 
             (((a_) \[CenterDot] (c_)) \[CenterDot] (c_)))) -> b, 
        "Side" -> 1, "Subpattern" -> (b_) \[CenterDot] ((c_) \[CenterDot] 
           (((a_) \[CenterDot] (c_)) \[CenterDot] (c_))), 
        "MatchingConstruct" -> {"CriticalPairLemma", 6}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] (a_)) \[CenterDot] ((a_) \[CenterDot] 
            ((b_) \[CenterDot] (a_))) -> a, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 8} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] 
          ((((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
            b) \[CenterDot] b) == ((a \[CenterDot] b) \[CenterDot] 
           (((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
            (a \[CenterDot] b))) \[CenterDot] 
          ((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 1}, 
        "Orientation" -> -1, "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((((b_) \[CenterDot] (((c_) \[CenterDot] (b_)) \[CenterDot] 
                (b_))) \[CenterDot] ((a_) \[CenterDot] (b_))) \[CenterDot] 
             ((a_) \[CenterDot] (b_)))) \[CenterDot] (c_) -> 
          b \[CenterDot] ((c \[CenterDot] b) \[CenterDot] b), "Side" -> 1, 
        "Subpattern" -> ((b_) \[CenterDot] (((c_) \[CenterDot] 
             (b_)) \[CenterDot] (b_))) \[CenterDot] ((a_) \[CenterDot] (b_)), 
        "MatchingConstruct" -> {"CriticalPairLemma", 7}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] ((((b_) \[CenterDot] (b_)) \[CenterDot] 
              (a_)) \[CenterDot] (a_))) \[CenterDot] (b_) -> 
          b \[CenterDot] b, "MatchingSide" -> 1, "Position" -> {1, 2, 1}|>|>, 
    {"SubstitutionLemma", 5} -> 
     <|"Statement" -> HoldForm[b == (a \[CenterDot] 
           ((b \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (b \[CenterDot] b)], "Proof" -> 
       <|"Input" -> {"SubstitutionLemma", 3}, "Construct" -> 
         {"SubstitutionLemma", 4}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[b == (a \[CenterDot] 
             ((b \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
            (b \[CenterDot] b)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 6} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] 
          ((((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
            b) \[CenterDot] b) == a \[CenterDot] b], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 8}, 
        "Construct" -> {"SubstitutionLemma", 5}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] 
             (a_))) \[CenterDot] ((b_) \[CenterDot] (b_)) -> b, 
        "Orientation" -> -1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          b \[CenterDot] ((((a \[CenterDot] b) \[CenterDot] (a \[CenterDot] 
                b)) \[CenterDot] b) \[CenterDot] b) == a \[CenterDot] b], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 9} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] 
           ((a \[CenterDot] a) \[CenterDot] a)) \[CenterDot] 
          (b \[CenterDot] a)], "Proof" -> <|"Construct" -> {"Axiom", 1}, 
        "Orientation" -> 1, "Rule" -> 
         ((a_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] 
             (a_))) \[CenterDot] ((b_) \[CenterDot] ((c_) \[CenterDot] 
             (a_))) -> b, "Side" -> 1, "Subpattern" -> (b_) \[CenterDot] 
          ((c_) \[CenterDot] (a_)), "MatchingConstruct" -> 
         {"SubstitutionLemma", 6}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CenterDot] 
           (((((b_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] (
                a_))) \[CenterDot] (a_)) \[CenterDot] (a_)) -> 
          b \[CenterDot] a, "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 7} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
          (b \[CenterDot] a)], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 9}, "Construct" -> 
         {"SubstitutionLemma", 4}, "Position" -> {1}, 
        "Rule" -> (a_) \[CenterDot] (((a_) \[CenterDot] (a_)) \[CenterDot] 
            (a_)) -> a \[CenterDot] a, "Orientation" -> 1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a == (a \[CenterDot] a) \[CenterDot] 
            (b \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 10} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] b) \[CenterDot] 
          (a \[CenterDot] b) == (a \[CenterDot] b) \[CenterDot] 
          (((b \[CenterDot] ((b \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] b))) \[CenterDot] (a \[CenterDot] 
             b)) \[CenterDot] (a \[CenterDot] b))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 6}, 
        "Orientation" -> 1, "Rule" -> (a_) \[CenterDot] 
           (((((b_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] (
                a_))) \[CenterDot] (a_)) \[CenterDot] (a_)) -> 
          b \[CenterDot] a, "Side" -> 1, "Subpattern" -> 
         (b_) \[CenterDot] (a_), "MatchingConstruct" -> {"SubstitutionLemma", 
          7}, "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] (a_)) -> a, 
        "MatchingSide" -> 1, "Position" -> {2, 1, 1, 1}|>|>, 
    {"SubstitutionLemma", 8} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] b) \[CenterDot] 
          (a \[CenterDot] b) == (a \[CenterDot] b) \[CenterDot] 
          (((b \[CenterDot] b) \[CenterDot] (a \[CenterDot] b)) \[CenterDot] 
           (a \[CenterDot] b))], "Proof" -> 
       <|"Input" -> {"CriticalPairLemma", 10}, "Construct" -> 
         {"SubstitutionLemma", 7}, "Position" -> {2, 1, 1, 2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[(b \[CenterDot] b) \[CenterDot] (a \[CenterDot] b) == 
           (a \[CenterDot] b) \[CenterDot] (((b \[CenterDot] b) \[CenterDot] 
              (a \[CenterDot] b)) \[CenterDot] (a \[CenterDot] b))], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 9} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] b) \[CenterDot] 
          (a \[CenterDot] b) == (a \[CenterDot] b) \[CenterDot] 
          (b \[CenterDot] (a \[CenterDot] b))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 8}, 
        "Construct" -> {"SubstitutionLemma", 7}, "Position" -> {2, 1}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[(b \[CenterDot] b) \[CenterDot] (a \[CenterDot] b) == 
           (a \[CenterDot] b) \[CenterDot] (b \[CenterDot] (a \[CenterDot] 
              b))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 10} -> 
     <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
          (b \[CenterDot] (a \[CenterDot] b))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 9}, 
        "Construct" -> {"SubstitutionLemma", 7}, "Position" -> {}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> 
         HoldForm[b == (a \[CenterDot] b) \[CenterDot] (b \[CenterDot] 
             (a \[CenterDot] b))], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 11} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] a == a \[CenterDot] 
          ((b \[CenterDot] a) \[CenterDot] ((a \[CenterDot] a) \[CenterDot] 
            (b \[CenterDot] a)))], "Proof" -> 
       <|"Construct" -> {"SubstitutionLemma", 10}, "Orientation" -> -1, 
        "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
            ((a_) \[CenterDot] (b_))) -> b, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 7}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] 
           ((b_) \[CenterDot] (a_)) -> a, "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 11} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] a == a \[CenterDot] 
          ((b \[CenterDot] a) \[CenterDot] a)], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 11}, 
        "Construct" -> {"SubstitutionLemma", 7}, "Position" -> {2, 2}, 
        "Rule" -> ((a_) \[CenterDot] (a_)) \[CenterDot] ((b_) \[CenterDot] 
            (a_)) -> a, "Orientation" -> -1, "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2, "OutputExpression" -> 
         HoldForm[b \[CenterDot] a == a \[CenterDot] 
            ((b \[CenterDot] a) \[CenterDot] a)], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 12} -> 
     <|"Statement" -> HoldForm[b == (b \[CenterDot] a) \[CenterDot] 
          (b \[CenterDot] (c \[CenterDot] ((a \[CenterDot] c) \[CenterDot] 
             c)))], "Proof" -> <|"Input" -> {"CriticalPairLemma", 2}, 
        "Construct" -> {"SubstitutionLemma", 11}, "Position" -> {1}, 
        "Rule" -> (a_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] 
            (a_)) -> b \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[b == (b \[CenterDot] a) \[CenterDot] 
            (b \[CenterDot] (c \[CenterDot] ((a \[CenterDot] c) \[CenterDot] 
               c)))], "Source" -> "norm"|>|>, {"SubstitutionLemma", 13} -> 
     <|"Statement" -> HoldForm[b == (b \[CenterDot] a) \[CenterDot] 
          (b \[CenterDot] (a \[CenterDot] c))], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 12}, 
        "Construct" -> {"SubstitutionLemma", 11}, "Position" -> {2, 2}, 
        "Rule" -> (a_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] 
            (a_)) -> b \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[b == (b \[CenterDot] a) \[CenterDot] 
            (b \[CenterDot] (a \[CenterDot] c))], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 12} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] (b \[CenterDot] 
            c)) \[CenterDot] (a \[CenterDot] c)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 13}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) -> a, "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] (c_), "MatchingConstruct" -> 
         {"SubstitutionLemma", 10}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           ((b_) \[CenterDot] ((a_) \[CenterDot] (b_))) -> b, 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"CriticalPairLemma", 13} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] b == a \[CenterDot] 
          ((a \[CenterDot] b) \[CenterDot] (b \[CenterDot] c))], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 12}, 
        "Orientation" -> -1, "Rule" -> 
         ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) \[CenterDot] 
           ((a_) \[CenterDot] (c_)) -> a, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] ((b_) \[CenterDot] (c_)), 
        "MatchingConstruct" -> {"SubstitutionLemma", 13}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
            ((b_) \[CenterDot] (c_))) -> a, "MatchingSide" -> 1, 
        "Position" -> {1}|>|>, {"SubstitutionLemma", 14} -> 
     <|"Statement" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
          (b \[CenterDot] (c \[CenterDot] a)) == b], 
      "Proof" -> <|"Input" -> {"Axiom", 1}, "Construct" -> 
         {"SubstitutionLemma", 11}, "Position" -> {1}, 
        "Rule" -> (a_) \[CenterDot] (((b_) \[CenterDot] (a_)) \[CenterDot] 
            (a_)) -> b \[CenterDot] a, "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, 
        "OutputExpression" -> HoldForm[(b \[CenterDot] a) \[CenterDot] 
            (b \[CenterDot] (c \[CenterDot] a)) == b], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 14} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] b == 
         ((a \[CenterDot] b) \[CenterDot] (b \[CenterDot] c)) \[CenterDot] 
          a], "Proof" -> <|"Construct" -> {"SubstitutionLemma", 14}, 
        "Orientation" -> 1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           ((a_) \[CenterDot] ((c_) \[CenterDot] (b_))) -> a, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] ((c_) \[CenterDot] (b_)), 
        "MatchingConstruct" -> {"SubstitutionLemma", 13}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] (b_)) \[CenterDot] ((a_) \[CenterDot] 
            ((b_) \[CenterDot] (c_))) -> a, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 15} -> 
     <|"Statement" -> HoldForm[b \[CenterDot] a == a \[CenterDot] b], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 14}, 
        "Orientation" -> -1, "Rule" -> (((a_) \[CenterDot] (b_)) \[CenterDot] 
            ((b_) \[CenterDot] (c_))) \[CenterDot] (a_) -> a \[CenterDot] b, 
        "Side" -> 1, "Subpattern" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
          ((b_) \[CenterDot] (c_)), "MatchingConstruct" -> 
         {"SubstitutionLemma", 10}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           ((b_) \[CenterDot] ((a_) \[CenterDot] (b_))) -> b, 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 16} -> 
     <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
          (b \[CenterDot] (c \[CenterDot] a))], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 14}, 
        "Orientation" -> 1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           ((a_) \[CenterDot] ((c_) \[CenterDot] (b_))) -> a, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] (b_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 15}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "MatchingSide" -> 1, "Position" -> {1}|>|>, 
    {"CriticalPairLemma", 17} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] c) == 
         a \[CenterDot] ((a \[CenterDot] (b \[CenterDot] c)) \[CenterDot] 
           c)], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 13}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] 
           (((a_) \[CenterDot] (b_)) \[CenterDot] ((b_) \[CenterDot] 
             (c_))) -> a \[CenterDot] b, "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] (c_), "MatchingConstruct" -> 
         {"CriticalPairLemma", 16}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           ((b_) \[CenterDot] ((c_) \[CenterDot] (a_))) -> b, 
        "MatchingSide" -> 1, "Position" -> {2, 2}|>|>, 
    {"SubstitutionLemma", 15} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] c) == 
         a \[CenterDot] (c \[CenterDot] (a \[CenterDot] (b \[CenterDot] 
             c)))], "Proof" -> <|"Input" -> {"CriticalPairLemma", 17}, 
        "Construct" -> {"CriticalPairLemma", 15}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          a \[CenterDot] (b \[CenterDot] c) == a \[CenterDot] 
            (c \[CenterDot] (a \[CenterDot] (b \[CenterDot] c)))], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 18} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] b) \[CenterDot] 
          ((c \[CenterDot] b) \[CenterDot] a)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 14}, 
        "Orientation" -> 1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           ((a_) \[CenterDot] ((c_) \[CenterDot] (b_))) -> a, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] ((c_) \[CenterDot] (b_)), 
        "MatchingConstruct" -> {"CriticalPairLemma", 15}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CenterDot] (b_) -> b \[CenterDot] a, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 19} -> 
     <|"Statement" -> HoldForm[a == (a \[CenterDot] b) \[CenterDot] 
          ((b \[CenterDot] c) \[CenterDot] a)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 13}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           ((a_) \[CenterDot] ((b_) \[CenterDot] (c_))) -> a, "Side" -> 1, 
        "Subpattern" -> (a_) \[CenterDot] ((b_) \[CenterDot] (c_)), 
        "MatchingConstruct" -> {"CriticalPairLemma", 15}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CenterDot] (b_) -> b \[CenterDot] a, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 20} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] c == 
         (((a \[CenterDot] b) \[CenterDot] c) \[CenterDot] a) \[CenterDot] 
          c], "Proof" -> <|"Construct" -> {"CriticalPairLemma", 18}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((c_) \[CenterDot] (b_)) \[CenterDot] (a_)) -> a, "Side" -> 1, 
        "Subpattern" -> ((c_) \[CenterDot] (b_)) \[CenterDot] (a_), 
        "MatchingConstruct" -> {"CriticalPairLemma", 19}, 
        "MatchingOrientation" -> -1, "MatchingRule" -> 
         ((a_) \[CenterDot] (b_)) \[CenterDot] (((b_) \[CenterDot] 
             (c_)) \[CenterDot] (a_)) -> a, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"SubstitutionLemma", 16} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] c == 
         c \[CenterDot] (((a \[CenterDot] b) \[CenterDot] c) \[CenterDot] 
           a)], "Proof" -> <|"Input" -> {"CriticalPairLemma", 20}, 
        "Construct" -> {"CriticalPairLemma", 15}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (a \[CenterDot] b) \[CenterDot] c == c \[CenterDot] 
            (((a \[CenterDot] b) \[CenterDot] c) \[CenterDot] a)], 
        "Source" -> "norm"|>|>, {"SubstitutionLemma", 17} -> 
     <|"Statement" -> HoldForm[(a \[CenterDot] b) \[CenterDot] c == 
         c \[CenterDot] (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] 
            c))], "Proof" -> <|"Input" -> {"SubstitutionLemma", 16}, 
        "Construct" -> {"CriticalPairLemma", 15}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2, "OutputExpression" -> HoldForm[
          (a \[CenterDot] b) \[CenterDot] c == c \[CenterDot] 
            (a \[CenterDot] ((a \[CenterDot] b) \[CenterDot] c))], 
        "Source" -> "norm"|>|>, {"CriticalPairLemma", 21} -> 
     <|"Statement" -> HoldForm[b == (a \[CenterDot] b) \[CenterDot] 
          ((c \[CenterDot] a) \[CenterDot] b)], 
      "Proof" -> <|"Construct" -> {"CriticalPairLemma", 16}, 
        "Orientation" -> -1, "Rule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           ((b_) \[CenterDot] ((c_) \[CenterDot] (a_))) -> b, "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] ((c_) \[CenterDot] (a_)), 
        "MatchingConstruct" -> {"CriticalPairLemma", 15}, 
        "MatchingOrientation" -> 1, "MatchingRule" -> 
         (a_) \[CenterDot] (b_) -> b \[CenterDot] a, "MatchingSide" -> 1, 
        "Position" -> {2}|>|>, {"CriticalPairLemma", 22} -> 
     <|"Statement" -> HoldForm[((b \[CenterDot] a) \[CenterDot] 
           b) \[CenterDot] a == a \[CenterDot] a], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 17}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            (((b_) \[CenterDot] (c_)) \[CenterDot] (a_))) -> 
          (b \[CenterDot] c) \[CenterDot] a, "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] (((b_) \[CenterDot] 
            (c_)) \[CenterDot] (a_)), "MatchingConstruct" -> 
         {"CriticalPairLemma", 21}, "MatchingOrientation" -> -1, 
        "MatchingRule" -> ((a_) \[CenterDot] (b_)) \[CenterDot] 
           (((c_) \[CenterDot] (a_)) \[CenterDot] (b_)) -> b, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"SubstitutionLemma", 18} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] 
          ((b \[CenterDot] a) \[CenterDot] b) == a \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"CriticalPairLemma", 22}, 
        "Construct" -> {"CriticalPairLemma", 15}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a \[CenterDot] ((b \[CenterDot] a) \[CenterDot] b) == 
           a \[CenterDot] a], "Source" -> "norm"|>|>, 
    {"SubstitutionLemma", 19} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (b \[CenterDot] 
           (b \[CenterDot] a)) == a \[CenterDot] a], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 18}, 
        "Construct" -> {"CriticalPairLemma", 15}, "Position" -> {2}, 
        "Rule" -> (a_) \[CenterDot] (b_) -> b \[CenterDot] a, 
        "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1, "OutputExpression" -> HoldForm[
          a \[CenterDot] (b \[CenterDot] (b \[CenterDot] a)) == 
           a \[CenterDot] a], "Source" -> "norm"|>|>, 
    {"CriticalPairLemma", 23} -> 
     <|"Statement" -> HoldForm[a \[CenterDot] (a \[CenterDot] b) == 
         a \[CenterDot] (b \[CenterDot] b)], 
      "Proof" -> <|"Construct" -> {"SubstitutionLemma", 15}, 
        "Orientation" -> -1, "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            ((a_) \[CenterDot] ((c_) \[CenterDot] (b_)))) -> 
          a \[CenterDot] (c \[CenterDot] b), "Side" -> 1, 
        "Subpattern" -> (b_) \[CenterDot] ((a_) \[CenterDot] 
           ((c_) \[CenterDot] (b_))), "MatchingConstruct" -> 
         {"SubstitutionLemma", 19}, "MatchingOrientation" -> 1, 
        "MatchingRule" -> (a_) \[CenterDot] ((b_) \[CenterDot] 
            ((b_) \[CenterDot] (a_))) -> a \[CenterDot] a, 
        "MatchingSide" -> 1, "Position" -> {2}|>|>, 
    {"Conclusion", 1} -> <|"Statement" -> 
       HoldForm[a \[CenterDot] (a \[CenterDot] b) == a \[CenterDot] 
          (a \[CenterDot] b)], "Proof" -> <|"Input" -> {"Hypothesis", 1}, 
        "Construct" -> {"CriticalPairLemma", 23}, "Position" -> {}, 
        "Rule" -> (a_) \[CenterDot] ((b_) \[CenterDot] (b_)) -> 
          a \[CenterDot] (a \[CenterDot] b), "Orientation" -> -1, 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2, 
        "OutputExpression" -> HoldForm[a \[CenterDot] (a \[CenterDot] b) == 
           a \[CenterDot] (a \[CenterDot] b)], "Source" -> "cpl"|>|>}|>]
