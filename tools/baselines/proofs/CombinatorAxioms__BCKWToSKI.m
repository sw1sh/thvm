ProofObject["EquationalLogic", {ForAll[{\[FormalX], \[FormalY], \[FormalZ]}, 
   CombinatorB \[Application] \[FormalX] \[Application] 
      \[FormalY] \[Application] \[FormalZ] == 
    CombinatorS \[Application] (CombinatorK \[Application] 
          CombinatorS) \[Application] CombinatorK \[Application] 
       \[FormalX] \[Application] \[FormalY] \[Application] \[FormalZ]], 
  ForAll[{\[FormalX], \[FormalY], \[FormalZ]}, 
   CombinatorC \[Application] \[FormalX] \[Application] 
      \[FormalY] \[Application] \[FormalZ] == 
    CombinatorS \[Application] (CombinatorS \[Application] 
           (CombinatorK \[Application] (CombinatorS \[Application] 
              (CombinatorK \[Application] CombinatorS) \[Application] 
             CombinatorK)) \[Application] CombinatorS) \[Application] 
        (CombinatorK \[Application] CombinatorK) \[Application] 
       \[FormalX] \[Application] \[FormalY] \[Application] \[FormalZ]], 
  ForAll[{\[FormalX], \[FormalY]}, 
   CombinatorW \[Application] \[FormalX] \[Application] \[FormalY] == 
    CombinatorS \[Application] CombinatorS \[Application] 
       (CombinatorS \[Application] CombinatorK) \[Application] 
      \[FormalX] \[Application] \[FormalY]]}, 
 {ForAll[{\[FormalX]}, CombinatorI \[Application] \[FormalX] == \[FormalX]], 
  ForAll[{\[FormalX], \[FormalY]}, 
   CombinatorK \[Application] \[FormalX] \[Application] \[FormalY] == 
    \[FormalX]], ForAll[{\[FormalX], \[FormalY], \[FormalZ]}, 
   CombinatorS \[Application] \[FormalX] \[Application] 
      \[FormalY] \[Application] \[FormalZ] == 
    \[FormalX] \[Application] \[FormalZ] \[Application] 
     (\[FormalY] \[Application] \[FormalZ])], 
  ForAll[{\[FormalX], \[FormalY], \[FormalZ]}, 
   CombinatorC \[Application] \[FormalX] \[Application] 
      \[FormalY] \[Application] \[FormalZ] == 
    \[FormalX] \[Application] \[FormalZ] \[Application] \[FormalY]], 
  ForAll[{\[FormalX], \[FormalY], \[FormalZ]}, 
   CombinatorB \[Application] \[FormalX] \[Application] 
      \[FormalY] \[Application] \[FormalZ] == \[FormalX] \[Application] 
     (\[FormalY] \[Application] \[FormalZ])], 
  ForAll[{\[FormalX], \[FormalY]}, 
   CombinatorW \[Application] \[FormalX] \[Application] \[FormalY] == 
    \[FormalX] \[Application] \[FormalY] \[Application] \[FormalY]], 
  ForAll[{\[FormalX]}, CombinatorY \[Application] \[FormalX] == 
    \[FormalX] \[Application] (CombinatorY \[Application] \[FormalX])]}, 
 <|"Variables" -> {\[FormalA], \[FormalB], \[FormalC], \[FormalD], 
    \[FormalE], \[FormalF], \[FormalG], \[FormalH], \[FormalI], \[FormalJ], 
    \[FormalK], \[FormalL], \[FormalM], \[FormalN], \[FormalO], \[FormalP], 
    \[FormalQ], \[FormalR], \[FormalS], \[FormalT], \[FormalX], \[FormalY], 
    \[FormalZ]}, "Constants" -> {}, 
  "Proof" -> {{"Axiom", 1} -> <|"Statement" -> 
       HoldForm[\[FormalA] == CombinatorK \[Application] 
           \[FormalA] \[Application] \[FormalB]], "Proof" -> <||>|>, 
    {"Axiom", 2} -> <|"Statement" -> HoldForm[\[FormalA] \[Application] 
          (\[FormalB] \[Application] \[FormalC]) == 
         CombinatorB \[Application] \[FormalA] \[Application] 
           \[FormalB] \[Application] \[FormalC]], "Proof" -> <||>|>, 
    {"Axiom", 3} -> <|"Statement" -> HoldForm[
        \[FormalA] \[Application] \[FormalB] \[Application] \[FormalC] == 
         CombinatorC \[Application] \[FormalA] \[Application] 
           \[FormalC] \[Application] \[FormalB]], "Proof" -> <||>|>, 
    {"Axiom", 4} -> <|"Statement" -> HoldForm[
        \[FormalA] \[Application] \[FormalB] \[Application] \[FormalB] == 
         CombinatorW \[Application] \[FormalA] \[Application] \[FormalB]], 
      "Proof" -> <||>|>, {"Axiom", 5} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[Application] 
           \[FormalB] \[Application] (\[FormalC] \[Application] 
           \[FormalB]) == CombinatorS \[Application] 
            \[FormalA] \[Application] \[FormalC] \[Application] \[FormalB]], 
      "Proof" -> <||>|>, {"Hypothesis", 1} -> 
     <|"Statement" -> HoldForm[
        CombinatorS \[Application] (CombinatorS \[Application] 
                (CombinatorK \[Application] (CombinatorS \[Application] 
                   (CombinatorK \[Application] CombinatorS) \[Application] 
                  CombinatorK)) \[Application] CombinatorS) \[Application] 
             (CombinatorK \[Application] CombinatorK) \[Application] 
            \[FormalP] \[Application] \[FormalQ] \[Application] \[FormalR] == 
         CombinatorC \[Application] \[FormalP] \[Application] 
           \[FormalQ] \[Application] \[FormalR]], "Proof" -> <||>|>, 
    {"Hypothesis", 2} -> <|"Statement" -> 
       HoldForm[CombinatorS \[Application] (CombinatorK \[Application] 
               CombinatorS) \[Application] CombinatorK \[Application] 
            \[FormalM] \[Application] \[FormalN] \[Application] \[FormalO] == 
         CombinatorB \[Application] \[FormalM] \[Application] 
           \[FormalN] \[Application] \[FormalO]], "Proof" -> <||>|>, 
    {"Hypothesis", 3} -> <|"Statement" -> 
       HoldForm[CombinatorS \[Application] CombinatorS \[Application] 
            (CombinatorS \[Application] CombinatorK) \[Application] 
           \[FormalS] \[Application] \[FormalT] == 
         CombinatorW \[Application] \[FormalS] \[Application] \[FormalT]], 
      "Proof" -> <||>|>, {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[
        CombinatorS \[Application] (CombinatorS \[Application] 
                (CombinatorK \[Application] (CombinatorS \[Application] 
                   (CombinatorK \[Application] CombinatorS) \[Application] 
                  CombinatorK)) \[Application] CombinatorS) \[Application] 
             (CombinatorK \[Application] CombinatorK) \[Application] 
            \[FormalP] \[Application] \[FormalQ] \[Application] \[FormalR] == 
         \[FormalP] \[Application] \[FormalR] \[Application] \[FormalQ]], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Position" -> {}, 
        "Construct" -> {"Axiom", 3}, "Orientation" -> -1, 
        "Rule" -> CombinatorC \[Application] (\[FormalA]_) \[Application] 
            (\[FormalB]_) \[Application] (\[FormalC]_) -> 
          \[FormalA] \[Application] \[FormalC] \[Application] \[FormalB], 
        "OutputExpression" -> HoldForm[
          CombinatorS \[Application] (CombinatorS \[Application] 
                  (CombinatorK \[Application] (CombinatorS \[Application] 
                     (CombinatorK \[Application] CombinatorS) \[Application] 
                    CombinatorK)) \[Application] CombinatorS) \[Application] (
                CombinatorK \[Application] CombinatorK) \[Application] 
              \[FormalP] \[Application] \[FormalQ] \[Application] 
            \[FormalR] == \[FormalP] \[Application] \[FormalR] \[Application] 
            \[FormalQ]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 2|>|>, {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[
        CombinatorS \[Application] (CombinatorK \[Application] 
                (CombinatorS \[Application] (CombinatorK \[Application] 
                   CombinatorS) \[Application] CombinatorK)) \[Application] 
              CombinatorS \[Application] \[FormalP] \[Application] 
            (CombinatorK \[Application] CombinatorK \[Application] 
             \[FormalP]) \[Application] \[FormalQ] \[Application] 
          \[FormalR] == \[FormalP] \[Application] \[FormalR] \[Application] 
          \[FormalQ]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 1}, 
        "Position" -> {1, 1}, "Construct" -> {"Axiom", 5}, 
        "Orientation" -> -1, "Rule" -> 
         CombinatorS \[Application] (\[FormalA]_) \[Application] 
            (\[FormalB]_) \[Application] (\[FormalC]_) -> 
          \[FormalA] \[Application] \[FormalC] \[Application] 
           (\[FormalB] \[Application] \[FormalC]), "OutputExpression" -> 
         HoldForm[CombinatorS \[Application] (CombinatorK \[Application] 
                  (CombinatorS \[Application] (CombinatorK \[Application] 
                     CombinatorS) \[Application] CombinatorK)) \[Application] 
                CombinatorS \[Application] \[FormalP] \[Application] 
              (CombinatorK \[Application] CombinatorK \[Application] 
               \[FormalP]) \[Application] \[FormalQ] \[Application] 
            \[FormalR] == \[FormalP] \[Application] \[FormalR] \[Application] 
            \[FormalQ]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 3} -> 
     <|"Statement" -> HoldForm[
        CombinatorS \[Application] (CombinatorK \[Application] 
                (CombinatorS \[Application] (CombinatorK \[Application] 
                   CombinatorS) \[Application] CombinatorK)) \[Application] 
              CombinatorS \[Application] \[FormalP] \[Application] 
            CombinatorK \[Application] \[FormalQ] \[Application] 
          \[FormalR] == \[FormalP] \[Application] \[FormalR] \[Application] 
          \[FormalQ]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 2}, 
        "Position" -> {1, 1, 2}, "Construct" -> {"Axiom", 1}, 
        "Orientation" -> -1, "Rule" -> CombinatorK \[Application] 
            (\[FormalA]_) \[Application] (\[FormalB]_) -> \[FormalA], 
        "OutputExpression" -> HoldForm[
          CombinatorS \[Application] (CombinatorK \[Application] 
                  (CombinatorS \[Application] (CombinatorK \[Application] 
                     CombinatorS) \[Application] CombinatorK)) \[Application] 
                CombinatorS \[Application] \[FormalP] \[Application] 
              CombinatorK \[Application] \[FormalQ] \[Application] 
            \[FormalR] == \[FormalP] \[Application] \[FormalR] \[Application] 
            \[FormalQ]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 4} -> 
     <|"Statement" -> HoldForm[
        CombinatorK \[Application] (CombinatorS \[Application] 
                 (CombinatorK \[Application] CombinatorS) \[Application] 
                CombinatorK) \[Application] \[FormalP] \[Application] 
             (CombinatorS \[Application] \[FormalP]) \[Application] 
            CombinatorK \[Application] \[FormalQ] \[Application] 
          \[FormalR] == \[FormalP] \[Application] \[FormalR] \[Application] 
          \[FormalQ]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 3}, 
        "Position" -> {1, 1, 1}, "Construct" -> {"Axiom", 5}, 
        "Orientation" -> -1, "Rule" -> 
         CombinatorS \[Application] (\[FormalA]_) \[Application] 
            (\[FormalB]_) \[Application] (\[FormalC]_) -> 
          \[FormalA] \[Application] \[FormalC] \[Application] 
           (\[FormalB] \[Application] \[FormalC]), "OutputExpression" -> 
         HoldForm[CombinatorK \[Application] (CombinatorS \[Application] 
                   (CombinatorK \[Application] CombinatorS) \[Application] 
                  CombinatorK) \[Application] \[FormalP] \[Application] (
                CombinatorS \[Application] \[FormalP]) \[Application] 
              CombinatorK \[Application] \[FormalQ] \[Application] 
            \[FormalR] == \[FormalP] \[Application] \[FormalR] \[Application] 
            \[FormalQ]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 5} -> 
     <|"Statement" -> HoldForm[
        CombinatorS \[Application] (CombinatorK \[Application] 
                CombinatorS) \[Application] CombinatorK \[Application] 
             (CombinatorS \[Application] \[FormalP]) \[Application] 
            CombinatorK \[Application] \[FormalQ] \[Application] 
          \[FormalR] == \[FormalP] \[Application] \[FormalR] \[Application] 
          \[FormalQ]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 4}, 
        "Position" -> {1, 1, 1, 1}, "Construct" -> {"Axiom", 1}, 
        "Orientation" -> -1, "Rule" -> CombinatorK \[Application] 
            (\[FormalA]_) \[Application] (\[FormalB]_) -> \[FormalA], 
        "OutputExpression" -> HoldForm[
          CombinatorS \[Application] (CombinatorK \[Application] 
                  CombinatorS) \[Application] CombinatorK \[Application] (
                CombinatorS \[Application] \[FormalP]) \[Application] 
              CombinatorK \[Application] \[FormalQ] \[Application] 
            \[FormalR] == \[FormalP] \[Application] \[FormalR] \[Application] 
            \[FormalQ]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 6} -> 
     <|"Statement" -> HoldForm[
        CombinatorK \[Application] CombinatorS \[Application] 
              (CombinatorS \[Application] \[FormalP]) \[Application] 
             (CombinatorK \[Application] (CombinatorS \[Application] 
               \[FormalP])) \[Application] CombinatorK \[Application] 
           \[FormalQ] \[Application] \[FormalR] == 
         \[FormalP] \[Application] \[FormalR] \[Application] \[FormalQ]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 5}, 
        "Position" -> {1, 1, 1}, "Construct" -> {"Axiom", 5}, 
        "Orientation" -> -1, "Rule" -> 
         CombinatorS \[Application] (\[FormalA]_) \[Application] 
            (\[FormalB]_) \[Application] (\[FormalC]_) -> 
          \[FormalA] \[Application] \[FormalC] \[Application] 
           (\[FormalB] \[Application] \[FormalC]), "OutputExpression" -> 
         HoldForm[CombinatorK \[Application] CombinatorS \[Application] 
                (CombinatorS \[Application] \[FormalP]) \[Application] (
                CombinatorK \[Application] (CombinatorS \[Application] 
                 \[FormalP])) \[Application] CombinatorK \[Application] 
             \[FormalQ] \[Application] \[FormalR] == 
           \[FormalP] \[Application] \[FormalR] \[Application] \[FormalQ]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 7} -> 
     <|"Statement" -> HoldForm[
        CombinatorS \[Application] (CombinatorK \[Application] 
              (CombinatorS \[Application] \[FormalP])) \[Application] 
            CombinatorK \[Application] \[FormalQ] \[Application] 
          \[FormalR] == \[FormalP] \[Application] \[FormalR] \[Application] 
          \[FormalQ]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 6}, 
        "Position" -> {1, 1, 1, 1}, "Construct" -> {"Axiom", 1}, 
        "Orientation" -> -1, "Rule" -> CombinatorK \[Application] 
            (\[FormalA]_) \[Application] (\[FormalB]_) -> \[FormalA], 
        "OutputExpression" -> HoldForm[
          CombinatorS \[Application] (CombinatorK \[Application] 
                (CombinatorS \[Application] \[FormalP])) \[Application] 
              CombinatorK \[Application] \[FormalQ] \[Application] 
            \[FormalR] == \[FormalP] \[Application] \[FormalR] \[Application] 
            \[FormalQ]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 8} -> 
     <|"Statement" -> HoldForm[
        CombinatorK \[Application] (CombinatorS \[Application] 
              \[FormalP]) \[Application] \[FormalQ] \[Application] 
           (CombinatorK \[Application] \[FormalQ]) \[Application] 
          \[FormalR] == \[FormalP] \[Application] \[FormalR] \[Application] 
          \[FormalQ]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 7}, 
        "Position" -> {1}, "Construct" -> {"Axiom", 5}, "Orientation" -> -1, 
        "Rule" -> CombinatorS \[Application] (\[FormalA]_) \[Application] 
            (\[FormalB]_) \[Application] (\[FormalC]_) -> 
          \[FormalA] \[Application] \[FormalC] \[Application] 
           (\[FormalB] \[Application] \[FormalC]), "OutputExpression" -> 
         HoldForm[CombinatorK \[Application] (CombinatorS \[Application] 
                \[FormalP]) \[Application] \[FormalQ] \[Application] 
             (CombinatorK \[Application] \[FormalQ]) \[Application] 
            \[FormalR] == \[FormalP] \[Application] \[FormalR] \[Application] 
            \[FormalQ]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 9} -> 
     <|"Statement" -> HoldForm[CombinatorS \[Application] 
            \[FormalP] \[Application] (CombinatorK \[Application] 
            \[FormalQ]) \[Application] \[FormalR] == 
         \[FormalP] \[Application] \[FormalR] \[Application] \[FormalQ]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 8}, "Position" -> {1, 1}, 
        "Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> CombinatorK \[Application] (\[FormalA]_) \[Application] 
           (\[FormalB]_) -> \[FormalA], "OutputExpression" -> 
         HoldForm[CombinatorS \[Application] \[FormalP] \[Application] 
             (CombinatorK \[Application] \[FormalQ]) \[Application] 
            \[FormalR] == \[FormalP] \[Application] \[FormalR] \[Application] 
            \[FormalQ]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 10} -> 
     <|"Statement" -> HoldForm[\[FormalP] \[Application] 
           \[FormalR] \[Application] (CombinatorK \[Application] 
            \[FormalQ] \[Application] \[FormalR]) == 
         \[FormalP] \[Application] \[FormalR] \[Application] \[FormalQ]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 9}, "Position" -> {}, 
        "Construct" -> {"Axiom", 5}, "Orientation" -> -1, 
        "Rule" -> CombinatorS \[Application] (\[FormalA]_) \[Application] 
            (\[FormalB]_) \[Application] (\[FormalC]_) -> 
          \[FormalA] \[Application] \[FormalC] \[Application] 
           (\[FormalB] \[Application] \[FormalC]), "OutputExpression" -> 
         HoldForm[\[FormalP] \[Application] \[FormalR] \[Application] 
            (CombinatorK \[Application] \[FormalQ] \[Application] 
             \[FormalR]) == \[FormalP] \[Application] 
             \[FormalR] \[Application] \[FormalQ]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"Conclusion", 1} -> <|"Statement" -> 
       HoldForm[\[FormalP] \[Application] \[FormalR] \[Application] 
          \[FormalQ] == \[FormalP] \[Application] \[FormalR] \[Application] 
          \[FormalQ]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 10}, 
        "Position" -> {2}, "Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> CombinatorK \[Application] (\[FormalA]_) \[Application] 
           (\[FormalB]_) -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalP] \[Application] \[FormalR] \[Application] 
            \[FormalQ] == \[FormalP] \[Application] \[FormalR] \[Application] 
            \[FormalQ]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 11} -> 
     <|"Statement" -> HoldForm[
        CombinatorS \[Application] (CombinatorK \[Application] 
               CombinatorS) \[Application] CombinatorK \[Application] 
            \[FormalM] \[Application] \[FormalN] \[Application] \[FormalO] == 
         \[FormalM] \[Application] (\[FormalN] \[Application] \[FormalO])], 
      "Proof" -> <|"Input" -> {"Hypothesis", 2}, "Position" -> {}, 
        "Construct" -> {"Axiom", 2}, "Orientation" -> -1, 
        "Rule" -> CombinatorB \[Application] (\[FormalA]_) \[Application] 
            (\[FormalB]_) \[Application] (\[FormalC]_) -> 
          \[FormalA] \[Application] (\[FormalB] \[Application] \[FormalC]), 
        "OutputExpression" -> HoldForm[
          CombinatorS \[Application] (CombinatorK \[Application] 
                 CombinatorS) \[Application] CombinatorK \[Application] 
              \[FormalM] \[Application] \[FormalN] \[Application] 
            \[FormalO] == \[FormalM] \[Application] 
            (\[FormalN] \[Application] \[FormalO])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 12} -> 
     <|"Statement" -> HoldForm[
        CombinatorK \[Application] CombinatorS \[Application] 
             \[FormalM] \[Application] (CombinatorK \[Application] 
             \[FormalM]) \[Application] \[FormalN] \[Application] 
          \[FormalO] == \[FormalM] \[Application] (\[FormalN] \[Application] 
           \[FormalO])], "Proof" -> <|"Input" -> {"SubstitutionLemma", 11}, 
        "Position" -> {1, 1}, "Construct" -> {"Axiom", 5}, 
        "Orientation" -> -1, "Rule" -> 
         CombinatorS \[Application] (\[FormalA]_) \[Application] 
            (\[FormalB]_) \[Application] (\[FormalC]_) -> 
          \[FormalA] \[Application] \[FormalC] \[Application] 
           (\[FormalB] \[Application] \[FormalC]), "OutputExpression" -> 
         HoldForm[CombinatorK \[Application] CombinatorS \[Application] 
               \[FormalM] \[Application] (CombinatorK \[Application] 
               \[FormalM]) \[Application] \[FormalN] \[Application] 
            \[FormalO] == \[FormalM] \[Application] 
            (\[FormalN] \[Application] \[FormalO])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 13} -> 
     <|"Statement" -> HoldForm[CombinatorS \[Application] 
            (CombinatorK \[Application] \[FormalM]) \[Application] 
           \[FormalN] \[Application] \[FormalO] == \[FormalM] \[Application] 
          (\[FormalN] \[Application] \[FormalO])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 12}, 
        "Position" -> {1, 1, 1}, "Construct" -> {"Axiom", 1}, 
        "Orientation" -> -1, "Rule" -> CombinatorK \[Application] 
            (\[FormalA]_) \[Application] (\[FormalB]_) -> \[FormalA], 
        "OutputExpression" -> HoldForm[
          CombinatorS \[Application] (CombinatorK \[Application] 
               \[FormalM]) \[Application] \[FormalN] \[Application] 
            \[FormalO] == \[FormalM] \[Application] 
            (\[FormalN] \[Application] \[FormalO])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 14} -> 
     <|"Statement" -> HoldForm[CombinatorK \[Application] 
            \[FormalM] \[Application] \[FormalO] \[Application] 
          (\[FormalN] \[Application] \[FormalO]) == \[FormalM] \[Application] 
          (\[FormalN] \[Application] \[FormalO])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 13}, "Position" -> {}, 
        "Construct" -> {"Axiom", 5}, "Orientation" -> -1, 
        "Rule" -> CombinatorS \[Application] (\[FormalA]_) \[Application] 
            (\[FormalB]_) \[Application] (\[FormalC]_) -> 
          \[FormalA] \[Application] \[FormalC] \[Application] 
           (\[FormalB] \[Application] \[FormalC]), "OutputExpression" -> 
         HoldForm[CombinatorK \[Application] \[FormalM] \[Application] 
             \[FormalO] \[Application] (\[FormalN] \[Application] 
             \[FormalO]) == \[FormalM] \[Application] 
            (\[FormalN] \[Application] \[FormalO])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"Conclusion", 2} -> <|"Statement" -> 
       HoldForm[\[FormalM] \[Application] (\[FormalN] \[Application] 
           \[FormalO]) == \[FormalM] \[Application] 
          (\[FormalN] \[Application] \[FormalO])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 14}, "Position" -> {1}, 
        "Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> CombinatorK \[Application] (\[FormalA]_) \[Application] 
           (\[FormalB]_) -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalM] \[Application] (\[FormalN] \[Application] 
             \[FormalO]) == \[FormalM] \[Application] 
            (\[FormalN] \[Application] \[FormalO])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 15} -> 
     <|"Statement" -> HoldForm[
        CombinatorS \[Application] CombinatorS \[Application] 
            (CombinatorS \[Application] CombinatorK) \[Application] 
           \[FormalS] \[Application] \[FormalT] == 
         \[FormalS] \[Application] \[FormalT] \[Application] \[FormalT]], 
      "Proof" -> <|"Input" -> {"Hypothesis", 3}, "Position" -> {}, 
        "Construct" -> {"Axiom", 4}, "Orientation" -> -1, 
        "Rule" -> CombinatorW \[Application] (\[FormalA]_) \[Application] 
           (\[FormalB]_) -> \[FormalA] \[Application] 
            \[FormalB] \[Application] \[FormalB], "OutputExpression" -> 
         HoldForm[CombinatorS \[Application] CombinatorS \[Application] 
              (CombinatorS \[Application] CombinatorK) \[Application] 
             \[FormalS] \[Application] \[FormalT] == 
           \[FormalS] \[Application] \[FormalT] \[Application] \[FormalT]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 16} -> 
     <|"Statement" -> HoldForm[CombinatorS \[Application] 
            \[FormalS] \[Application] (CombinatorS \[Application] 
             CombinatorK \[Application] \[FormalS]) \[Application] 
          \[FormalT] == \[FormalS] \[Application] \[FormalT] \[Application] 
          \[FormalT]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 15}, 
        "Position" -> {1}, "Construct" -> {"Axiom", 5}, "Orientation" -> -1, 
        "Rule" -> CombinatorS \[Application] (\[FormalA]_) \[Application] 
            (\[FormalB]_) \[Application] (\[FormalC]_) -> 
          \[FormalA] \[Application] \[FormalC] \[Application] 
           (\[FormalB] \[Application] \[FormalC]), "OutputExpression" -> 
         HoldForm[CombinatorS \[Application] \[FormalS] \[Application] 
             (CombinatorS \[Application] CombinatorK \[Application] 
              \[FormalS]) \[Application] \[FormalT] == 
           \[FormalS] \[Application] \[FormalT] \[Application] \[FormalT]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 17} -> 
     <|"Statement" -> HoldForm[\[FormalS] \[Application] 
           \[FormalT] \[Application] (CombinatorS \[Application] 
             CombinatorK \[Application] \[FormalS] \[Application] 
           \[FormalT]) == \[FormalS] \[Application] \[FormalT] \[Application] 
          \[FormalT]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 16}, 
        "Position" -> {}, "Construct" -> {"Axiom", 5}, "Orientation" -> -1, 
        "Rule" -> CombinatorS \[Application] (\[FormalA]_) \[Application] 
            (\[FormalB]_) \[Application] (\[FormalC]_) -> 
          \[FormalA] \[Application] \[FormalC] \[Application] 
           (\[FormalB] \[Application] \[FormalC]), "OutputExpression" -> 
         HoldForm[\[FormalS] \[Application] \[FormalT] \[Application] 
            (CombinatorS \[Application] CombinatorK \[Application] 
              \[FormalS] \[Application] \[FormalT]) == 
           \[FormalS] \[Application] \[FormalT] \[Application] \[FormalT]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 18} -> 
     <|"Statement" -> HoldForm[\[FormalS] \[Application] 
           \[FormalT] \[Application] (CombinatorK \[Application] 
            \[FormalT] \[Application] (\[FormalS] \[Application] 
            \[FormalT])) == \[FormalS] \[Application] 
           \[FormalT] \[Application] \[FormalT]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 17}, "Position" -> {2}, 
        "Construct" -> {"Axiom", 5}, "Orientation" -> -1, 
        "Rule" -> CombinatorS \[Application] (\[FormalA]_) \[Application] 
            (\[FormalB]_) \[Application] (\[FormalC]_) -> 
          \[FormalA] \[Application] \[FormalC] \[Application] 
           (\[FormalB] \[Application] \[FormalC]), "OutputExpression" -> 
         HoldForm[\[FormalS] \[Application] \[FormalT] \[Application] 
            (CombinatorK \[Application] \[FormalT] \[Application] 
             (\[FormalS] \[Application] \[FormalT])) == 
           \[FormalS] \[Application] \[FormalT] \[Application] \[FormalT]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"Conclusion", 3} -> <|"Statement" -> 
       HoldForm[\[FormalS] \[Application] \[FormalT] \[Application] 
          \[FormalT] == \[FormalS] \[Application] \[FormalT] \[Application] 
          \[FormalT]], "Proof" -> <|"Input" -> {"SubstitutionLemma", 18}, 
        "Position" -> {2}, "Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> CombinatorK \[Application] (\[FormalA]_) \[Application] 
           (\[FormalB]_) -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalS] \[Application] \[FormalT] \[Application] 
            \[FormalT] == \[FormalS] \[Application] \[FormalT] \[Application] 
            \[FormalT]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>}|>]
